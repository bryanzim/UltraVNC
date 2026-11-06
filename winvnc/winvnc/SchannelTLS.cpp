// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//

#include "stdhdrs.h"

#include "SchannelTLS.h"

#include "common/ChannelBinding.h"
#include "vnclog.h"
#include "vsocket.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

extern VNCLog vnclog;

// IO_BUFFER_SIZE for raw token exchange during handshake
static const DWORD IO_BUFFER_SIZE{ 32768U };

// OpenSSL 3 (security level 2) and GnuTLS reject certificates with RSA keys
// below 2048 bits during verification. Older builds created 1024-bit keys.
static const DWORD MinCertKeyBits{ 2048U };
static const DWORD NewCertKeyBits{ 3072U };

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static std::string HrStr(const SECURITY_STATUS hr)
{
    std::array<char, 32U> buf;
    ::sprintf_s(buf.data(), buf.size(), "0x%08X", static_cast<unsigned>(hr));
    return buf.data();
}

// Convert hex thumbprint string (40 chars) to 20-byte binary
static bool HexToBin(const char* const hex, BYTE* const out, const DWORD outLen)
{
    DWORD len = static_cast<DWORD>(std::strlen(hex));
    if (len != outLen * 2)
    {
        return false;
    }
    for (DWORD i{ 0U }; i < outLen; ++i)
    {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];
        auto nib = [](char c) -> int {
            if ((c >= '0') && (c <= '9'))
            {
                return c - '0';
            }
            if ((c >= 'a') && (c <= 'f'))
            {
                return c - 'a' + 10;
            }
            if ((c >= 'A') && (c <= 'F'))
            {
                return c - 'A' + 10;
            }
            return -1;
        };
        int h = nib(hi), l = nib(lo);
        if ((h < 0) || (l < 0))
        {
            return false;
        }
        out[i] = static_cast<BYTE>(((h << 4) | l));
    }
    return true;
}

// -----------------------------------------------------------------------
// SchannelTLS
// -----------------------------------------------------------------------

SchannelTLS::SchannelTLS() : m_certCtx{ nullptr }, m_credValid{ false }, m_ctxValid{ false }, m_ready{ false }
{
    SecInvalidateHandle(&m_credHandle)
    SecInvalidateHandle(&m_ctxHandle)
    std::memset(&m_streamSizes, 0, sizeof(m_streamSizes));
    ::InitializeCriticalSection(&m_csCrypto);
}

SchannelTLS::~SchannelTLS()
{
    ::DeleteCriticalSection(&m_csCrypto);
    if (m_ctxValid)
    {
        ::DeleteSecurityContext(&m_ctxHandle);
        m_ctxValid = false;
    }
    if (m_credValid)
    {
        ::FreeCredentialsHandle(&m_credHandle);
        m_credValid = false;
    }
    if (m_certCtx)
    {
        ::CertFreeCertificateContext(m_certCtx);
        m_certCtx = nullptr;
    }
}

// -----------------------------------------------------------------------
// Certificate loading
// -----------------------------------------------------------------------

static PCCERT_CONTEXT FindCertInStore(const HCERTSTORE hStore, const BYTE* const sha1, const DWORD sha1Len)
{
    CRYPT_HASH_BLOB blob;
    blob.cbData = sha1Len;
    blob.pbData = const_cast<BYTE*>(sha1);
    return ::CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SHA1_HASH, &blob, nullptr);
}

static DWORD CertKeyBits(PCCERT_CONTEXT ctx)
{ return ::CertGetPublicKeyLength(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, &ctx->pCertInfo->SubjectPublicKeyInfo); }

bool SchannelTLS::LoadCertByThumbprint(const char* const thumbprint)
{
    if (!thumbprint || (std::strlen(thumbprint) != 40))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: invalid thumbprint (must be 40 hex chars)\n"));
        return false;
    }

    std::array<BYTE, 20U> sha1;
    if (!HexToBin(thumbprint, sha1.data(), 20))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: thumbprint is not valid hex\n"));
        return false;
    }

    // Try Local Machine first, then Current User
    const DWORD locations[] = { CERT_SYSTEM_STORE_LOCAL_MACHINE, CERT_SYSTEM_STORE_CURRENT_USER };
    for (DWORD loc : locations)
    {
        HCERTSTORE hStore = ::CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, loc | CERT_STORE_READONLY_FLAG, L"MY");
        if (!hStore)
        {
            continue;
        }
        PCCERT_CONTEXT ctx = FindCertInStore(hStore, sha1.data(), 20);
        ::CertCloseStore(hStore, 0);
        if (ctx)
        {
            if (m_certCtx)
            {
                ::CertFreeCertificateContext(m_certCtx);
            }
            m_certCtx = ctx;

            // Log subject
            std::array<char, 256U> subject{};
            ::CertGetNameStringA(m_certCtx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, subject.data(), sizeof(subject));
            vnclog.Print(LL_CLIENTS, VNCLOG("TLS: loaded cert subject='%s'\n"), subject.data());

            // Verify not expired
            FILETIME ftNow;
            ::GetSystemTimeAsFileTime(&ftNow);
            if (::CompareFileTime(&m_certCtx->pCertInfo->NotAfter, &ftNow) < 0)
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: certificate expired\n"));
                ::CertFreeCertificateContext(m_certCtx);
                m_certCtx = nullptr;
                return false;
            }

            const DWORD keyBits = CertKeyBits(m_certCtx);
            if (keyBits < MinCertKeyBits)
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: certificate key too weak (%lu bits, need %lu)\n"), keyBits, MinCertKeyBits);
                ::CertFreeCertificateContext(m_certCtx);
                m_certCtx = nullptr;
                return false;
            }

            // Verify private key accessible
            HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hKey = 0;
            DWORD keySpec{ 0U };
            BOOL mustFree = FALSE;
            if (!::CryptAcquireCertificatePrivateKey(m_certCtx, CRYPT_ACQUIRE_SILENT_FLAG | CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG,
                                                   nullptr, &hKey, &keySpec, &mustFree))
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: private key not accessible: %lu\n"), ::GetLastError());
                ::CertFreeCertificateContext(m_certCtx);
                m_certCtx = nullptr;
                return false;
            }
            if (mustFree)
            {
                if (keySpec == CERT_NCRYPT_KEY_SPEC)
                {
                    ::NCryptFreeObject(hKey);
                }
                else
                {
                    ::CryptReleaseContext(hKey, 0);
                }
            }

            vnclog.Print(LL_CLIENTS, VNCLOG("TLS: certificate ready\n"));
            return true;
        }
    }

    vnclog.Print(LL_CONNERR, VNCLOG("TLS: certificate not found in store\n"));
    return false;
}

bool SchannelTLS::EnsureSelfSignedCert(const char* const subjectName, const char* const containerName, char* const outThumbprint,
                                       DWORD keyBits)
{
    if ((keyBits != 2048) && (keyBits != 3072) && (keyBits != 4096))
    {
        keyBits = NewCertKeyBits;
    }

    // Use legacy defaults when callers pass nullptr.
    const char* subjectA = subjectName ? subjectName : "CN=UltraVNC-WebViewer";
    const char* containerA = containerName ? containerName : "UltraVNC-WebViewer";

    // Convert subject/container to wide strings for the CryptoAPI calls.
    auto ToW = [](const char* s) -> std::wstring {
        if (!s)
        {
            return std::wstring();
        }
        int len = ::MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
        if (len <= 0)
        {
            return std::wstring();
        }
        std::wstring w(static_cast<unsigned long long>(len - 1), 0);
        ::MultiByteToWideChar(CP_ACP, 0, s, -1, &w[0U], len);
        return w;
    };
    std::wstring subjectW = ToW(subjectA);
    std::wstring containerW = ToW(containerA);
    const wchar_t* CN = subjectW.c_str();

    const char* simpleName = subjectA;
    if (::_strnicmp(subjectA, "CN=", 3) == 0)
    {
        simpleName = subjectA + 3;
    }

    // Try Local Machine store first (requires admin/SYSTEM), fall back to Current User
    struct StoreCandidate
    {
        DWORD location;
        bool machineKeyset;
        const wchar_t* label;
    };
    StoreCandidate candidates[] = {
        { CERT_SYSTEM_STORE_LOCAL_MACHINE, true, L"LocalMachine" },
        { CERT_SYSTEM_STORE_CURRENT_USER, false, L"CurrentUser" },
    };

    HCERTSTORE hStore = nullptr;
    bool useMachineKeyset{ false };
    for (auto& c : candidates)
    {
        hStore = ::CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, c.location, L"MY");
        if (hStore)
        {
            useMachineKeyset = c.machineKeyset;
            vnclog.Print(LL_CLIENTS, VNCLOG("TLS: using cert store %S\\MY\n"), c.label);
            break;
        }
        vnclog.Print(LL_INTINFO, VNCLOG("TLS: cannot open %S store: %lu, trying next\n"), c.label, ::GetLastError());
    }

    if (!hStore)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: cannot open any cert store\n"));
        return false;
    }

    // Search for existing valid self-signed cert
    PCCERT_CONTEXT existing = nullptr;
    PCCERT_CONTEXT cur = nullptr;
    while ((cur = ::CertEnumCertificatesInStore(hStore, cur)) != nullptr)
    {
        std::array<char, 256U> name{};
        ::CertGetNameStringA(cur, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name.data(), sizeof(name));
        if (::_stricmp(name.data(), simpleName) == 0)
        {
            FILETIME ftNow;
            ::GetSystemTimeAsFileTime(&ftNow);
            if ((::CompareFileTime(&cur->pCertInfo->NotAfter, &ftNow) > 0) && (CertKeyBits(cur) >= MinCertKeyBits))
            {
                existing = ::CertDuplicateCertificateContext(cur);
                ::CertFreeCertificateContext(cur);
                break;
            }
        }
    }

    if (existing)
    {
        if (m_certCtx)
        {
            ::CertFreeCertificateContext(m_certCtx);
        }
        m_certCtx = existing;
        ::CertCloseStore(hStore, 0);
        vnclog.Print(LL_CLIENTS, VNCLOG("TLS: reusing existing self-signed certificate\n"));
    }
    else
    {
        // Encode subject name
        CERT_NAME_BLOB nameBlob{};
        DWORD nameLen{ 0U };
        if (!::CertStrToNameW(X509_ASN_ENCODING, CN, CERT_X500_NAME_STR, nullptr, nullptr, &nameLen, nullptr))
        {
            ::CertCloseStore(hStore, 0);
            return false;
        }
        std::vector<BYTE> nameBuf(nameLen);
        ::CertStrToNameW(X509_ASN_ENCODING, CN, CERT_X500_NAME_STR, nullptr, nameBuf.data(), &nameLen, nullptr);
        nameBlob.cbData = nameLen;
        nameBlob.pbData = nameBuf.data();

        // 2-year validity
        SYSTEMTIME start, end;
        ::GetSystemTime(&start);
        end = start;
        end.wYear += 2;

        // Create the RSA key explicitly: letting CertCreateSelfSignCertificate
        // generate it uses the provider default, which is 1024 bits for the
        // legacy Schannel CSP. A separate key name keeps old 1024-bit
        // containers (still referenced by old certificates) untouched.
        const std::wstring keyName = containerW + L"-RSA" + std::to_wstring(keyBits);
        const DWORD machineFlag = useMachineKeyset ? NCRYPT_MACHINE_KEY_FLAG : 0;
        NCRYPT_PROV_HANDLE hProv = 0;
        NCRYPT_KEY_HANDLE hKey = 0;
        SECURITY_STATUS st = ::NCryptOpenStorageProvider(&hProv, MS_KEY_STORAGE_PROVIDER, 0);
        if (st == ERROR_SUCCESS)
        {
            st = ::NCryptCreatePersistedKey(hProv, &hKey, NCRYPT_RSA_ALGORITHM, keyName.c_str(), AT_KEYEXCHANGE,
                                          NCRYPT_OVERWRITE_KEY_FLAG | machineFlag);
        }
        if (st == ERROR_SUCCESS)
        {
            st = ::NCryptSetProperty(hKey, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&keyBits), sizeof(keyBits), 0);
        }
        if (st == ERROR_SUCCESS)
        {
            st = ::NCryptFinalizeKey(hKey, 0);
        }
        if (st != ERROR_SUCCESS)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: RSA key generation failed: %s\n"), HrStr(st).c_str());
            if (hKey)
            {
                ::NCryptFreeObject(hKey);
            }
            if (hProv)
            {
                ::NCryptFreeObject(hProv);
            }
            ::CertCloseStore(hStore, 0);
            return false;
        }

        CRYPT_KEY_PROV_INFO keyProvInfo{};
        keyProvInfo.pwszContainerName = const_cast<LPWSTR>(keyName.c_str());
        keyProvInfo.pwszProvName = const_cast<LPWSTR>(MS_KEY_STORAGE_PROVIDER);
        keyProvInfo.dwProvType = 0;
        keyProvInfo.dwFlags = machineFlag;
        keyProvInfo.dwKeySpec = AT_KEYEXCHANGE;

        CRYPT_ALGORITHM_IDENTIFIER sigAlg{};
        sigAlg.pszObjId = const_cast<LPSTR>(szOID_RSA_SHA256RSA);

        PCCERT_CONTEXT newCtx = ::CertCreateSelfSignCertificate(hKey, &nameBlob, 0, &keyProvInfo, &sigAlg, &start, &end, nullptr);
        const DWORD createErr = ::GetLastError();
        ::NCryptFreeObject(hKey);
        ::NCryptFreeObject(hProv);

        if (!newCtx)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: self-signed cert creation failed: %lu\n"), createErr);
            ::CertCloseStore(hStore, 0);
            return false;
        }

        // Add to store
        if (!::CertAddCertificateContextToStore(hStore, newCtx, CERT_STORE_ADD_REPLACE_EXISTING, &m_certCtx))
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: cannot add cert to store: %lu\n"), ::GetLastError());
            ::CertFreeCertificateContext(newCtx);
            ::CertCloseStore(hStore, 0);
            return false;
        }
        ::CertFreeCertificateContext(newCtx);
        ::CertCloseStore(hStore, 0);
        vnclog.Print(LL_CLIENTS, VNCLOG("TLS: created new self-signed certificate\n"));
    }

    // Extract thumbprint and convert to hex string
    std::array<BYTE, 20U> sha1;
    DWORD sha1Len = sizeof(sha1);
    if (!::CertGetCertificateContextProperty(m_certCtx, CERT_SHA1_HASH_PROP_ID, sha1.data(), &sha1Len))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: cannot get cert thumbprint\n"));
        return false;
    }
    if (outThumbprint)
    {
        for (int i{ 0 }; i < 20; ++i)
        {
            ::sprintf_s(outThumbprint + i * 2, 3, "%02x", sha1[static_cast<unsigned long long>(i)]);
        }
        outThumbprint[40U] = '\0';
        vnclog.Print(LL_CLIENTS, VNCLOG("TLS: self-signed cert thumbprint: %s\n"), outThumbprint);
    }

    return true;
}

// -----------------------------------------------------------------------
// Handshake
// -----------------------------------------------------------------------

bool SchannelTLS::ServerHandshake(VSocket* const sock)
{
    if (!m_certCtx)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: no certificate loaded\n"));
        return false;
    }

    TimeStamp tsExpiry;
    SECURITY_STATUS ss;
    bool credsAcquired{ false };

#ifdef SCH_CREDENTIALS_VERSION
    // Modern path: disable only legacy protocols, let Schannel negotiate
    // TLS 1.2 or TLS 1.3 depending on OS support.
    SCH_CREDENTIALS newCred{};
    TLS_PARAMETERS tlsParams{};
    newCred.dwVersion = SCH_CREDENTIALS_VERSION;
    newCred.cCreds = 1;
    newCred.paCred = &m_certCtx;
    newCred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER | SCH_USE_STRONG_CRYPTO;
    newCred.cTlsParameters = 1;
    newCred.pTlsParameters = &tlsParams;
    tlsParams.grbitDisabledProtocols = SP_PROT_SSL2_SERVER | SP_PROT_SSL3_SERVER | SP_PROT_TLS1_0_SERVER | SP_PROT_TLS1_1_SERVER;

    ss = ::AcquireCredentialsHandleW(nullptr, UNISP_NAME_W, SECPKG_CRED_INBOUND, nullptr, &newCred, nullptr, nullptr, &m_credHandle,
                                   &tsExpiry);

    if (ss == SEC_E_OK)
    {
        credsAcquired = true;
        vnclog.Print(LL_CLIENTS, VNCLOG("TLS: acquired credentials via SCH_CREDENTIALS (TLS 1.2/1.3 enabled)\n"));
    }
    else
    {
        vnclog.Print(LL_INTINFO, VNCLOG("TLS: SCH_CREDENTIALS unavailable (%s), falling back to SCHANNEL_CRED\n"),
                     HrStr(ss).c_str());
    }
#endif

    if (!credsAcquired)
    {
        // Legacy path: TLS 1.2 only. Safe on older Windows and older SDKs.
        SCHANNEL_CRED sc{};
        sc.dwVersion = SCHANNEL_CRED_VERSION;
        sc.cCreds = 1;
        sc.paCred = &m_certCtx;
        sc.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
        sc.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER | SCH_USE_STRONG_CRYPTO;
        sc.dwMinimumCipherStrength = 128;

        ss = ::AcquireCredentialsHandleW(nullptr, UNISP_NAME_W, SECPKG_CRED_INBOUND, nullptr, &sc, nullptr, nullptr, &m_credHandle,
                                       &tsExpiry);

        if (ss != SEC_E_OK)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: AcquireCredentialsHandle failed: %s\n"), HrStr(ss).c_str());
            return false;
        }
    }
    m_credValid = true;

    // Handshake loop
    SecBuffer extraData{};
    if (!HandshakeLoop(sock, true, &extraData))
    {
        return false;
    }

    // If server sent extra data after handshake, buffer it
    if ((extraData.cbBuffer > 0) && extraData.pvBuffer)
    {
        m_rawBuf.insert(m_rawBuf.end(), reinterpret_cast<BYTE*>(extraData.pvBuffer), reinterpret_cast<BYTE*>(extraData.pvBuffer) + extraData.cbBuffer);
        ::FreeContextBuffer(extraData.pvBuffer);
    }

    // Get stream sizes for encrypt/decrypt
    ss = ::QueryContextAttributes(&m_ctxHandle, SECPKG_ATTR_STREAM_SIZES, &m_streamSizes);
    if (ss != SEC_E_OK)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: QueryContextAttributes failed: %s\n"), HrStr(ss).c_str());
        return false;
    }

    // Log the negotiated protocol so admins can verify whether TLS 1.3 was used.
    SecPkgContext_ConnectionInfo connInfo{};
    if (::QueryContextAttributes(&m_ctxHandle, SECPKG_ATTR_CONNECTION_INFO, &connInfo) == SEC_E_OK)
    {
        vnclog.Print(LL_CLIENTS, VNCLOG("TLS: negotiated protocol 0x%08X, cipher 0x%08X\n"), connInfo.dwProtocol,
                     connInfo.aiCipher);
    }

    m_ready = true;
    vnclog.Print(LL_CLIENTS, VNCLOG("TLS: handshake complete\n"));
    return true;
}

bool SchannelTLS::HandshakeLoop(VSocket* const sock, bool firstCall, SecBuffer* const pExtraData, const BYTE* initialData,
                                const DWORD initialLen)
{
    std::vector<BYTE> ioBuffer(IO_BUFFER_SIZE);
    if (initialLen > ioBuffer.size())
    {
        return false;
    }
    if (initialLen && initialData)
    {
        std::memcpy(ioBuffer.data(), initialData, initialLen);
    }
    DWORD ioBufferUsed = initialLen;
    SECURITY_STATUS ss = SEC_I_CONTINUE_NEEDED;

    while ((ss == SEC_I_CONTINUE_NEEDED) || (ss == SEC_E_INCOMPLETE_MESSAGE) || firstCall)
    {
        firstCall = false;

        // Read data from client if needed
        if ((ioBufferUsed == 0) || (ss == SEC_E_INCOMPLETE_MESSAGE))
        {
            if (ioBufferUsed == ioBuffer.size())
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: handshake buffer overflow\n"));
                return false;
            }
            DWORD received{ 0U };
            if (!RawRecv(sock, ioBuffer.data() + ioBufferUsed, static_cast<DWORD>((ioBuffer.size() - ioBufferUsed)), &received))
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: handshake recv failed\n"));
                return false;
            }
            ioBufferUsed += received;
        }

        // Input buffer: what we just read
        std::array<SecBuffer, 2U> inBufs;
        inBufs[0U].BufferType = SECBUFFER_TOKEN;
        inBufs[0U].cbBuffer = ioBufferUsed;
        inBufs[0U].pvBuffer = ioBuffer.data();
        inBufs[1U].BufferType = SECBUFFER_EMPTY;
        inBufs[1U].cbBuffer = 0;
        inBufs[1U].pvBuffer = nullptr;

        SecBufferDesc inDesc;
        inDesc.ulVersion = SECBUFFER_VERSION;
        inDesc.cBuffers = 2;
        inDesc.pBuffers = inBufs.data();

        // Output buffer: token to send to client
        std::array<SecBuffer, 1U> outBufs;
        outBufs[0U].BufferType = SECBUFFER_TOKEN;
        outBufs[0U].cbBuffer = 0;
        outBufs[0U].pvBuffer = nullptr;

        SecBufferDesc outDesc;
        outDesc.ulVersion = SECBUFFER_VERSION;
        outDesc.cBuffers = 1;
        outDesc.pBuffers = outBufs.data();

        DWORD ctxAttr{ 0U };
        TimeStamp tsExpiry;

        ss = ::AcceptSecurityContext(&m_credHandle, m_ctxValid ? &m_ctxHandle : nullptr, &inDesc,
                                   ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY |
                                       ASC_REQ_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM,
                                   SECURITY_NATIVE_DREP, &m_ctxHandle, &outDesc, &ctxAttr, &tsExpiry);

        m_ctxValid = true;

        // Send any token back to client
        if ((outBufs[0U].cbBuffer > 0) && outBufs[0U].pvBuffer)
        {
            if (!RawSend(sock, reinterpret_cast<BYTE*>(outBufs[0U].pvBuffer), outBufs[0U].cbBuffer))
            {
                ::FreeContextBuffer(outBufs[0U].pvBuffer);
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: handshake send failed\n"));
                return false;
            }
            ::FreeContextBuffer(outBufs[0U].pvBuffer);
        }

        if (ss == SEC_E_OK)
        {
            // Check for extra data (app data that came after handshake)
            if ((inBufs[1U].BufferType == SECBUFFER_EXTRA) && (inBufs[1U].cbBuffer > 0))
            {
                pExtraData->pvBuffer = ::LocalAlloc(LMEM_FIXED, inBufs[1U].cbBuffer);
                pExtraData->cbBuffer = inBufs[1U].cbBuffer;
                pExtraData->BufferType = SECBUFFER_TOKEN;
                if (pExtraData->pvBuffer)
                {
                    std::memcpy(pExtraData->pvBuffer, ioBuffer.data() + ioBufferUsed - inBufs[1U].cbBuffer, inBufs[1U].cbBuffer);
                }
            }
            else
            {
                pExtraData->pvBuffer = nullptr;
                pExtraData->cbBuffer = 0;
                pExtraData->BufferType = SECBUFFER_EMPTY;
            }
            return true;
        }
        else if (ss == SEC_I_CONTINUE_NEEDED)
        {
            // Consume processed bytes; keep EXTRA bytes if any
            if ((inBufs[1U].BufferType == SECBUFFER_EXTRA) && (inBufs[1U].cbBuffer > 0))
            {
                DWORD extra = inBufs[1U].cbBuffer;
                std::memmove(ioBuffer.data(), ioBuffer.data() + ioBufferUsed - extra, extra);
                ioBufferUsed = extra;
            }
            else
            {
                ioBufferUsed = 0;
            }
        }
        else if (ss == SEC_E_INCOMPLETE_MESSAGE)
        {
            // Need more data — loop again without clearing ioBuffer
        }
        else
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: AcceptSecurityContext failed: %s\n"), HrStr(ss).c_str());
            return false;
        }
    }

    return (ss == SEC_E_OK);
}

// -----------------------------------------------------------------------
// Send / Recv
// -----------------------------------------------------------------------

bool SchannelTLS::Send(VSocket* const sock, const char* const buf, const int len)
{
    if (!m_ready)
    {
        return false;
    }

    int offset{ 0 };
    while (offset < len)
    {
        // Max plaintext per record
        int chunk = min(len - offset, static_cast<int>(m_streamSizes.cbMaximumMessage));

        std::vector<BYTE> msg(m_streamSizes.cbHeader + static_cast<unsigned long>(chunk) + m_streamSizes.cbTrailer);

        std::array<SecBuffer, 4U> bufs;
        bufs[0U].BufferType = SECBUFFER_STREAM_HEADER;
        bufs[0U].cbBuffer = m_streamSizes.cbHeader;
        bufs[0U].pvBuffer = msg.data();

        bufs[1U].BufferType = SECBUFFER_DATA;
        bufs[1U].cbBuffer = static_cast<ULONG>(chunk);
        bufs[1U].pvBuffer = msg.data() + m_streamSizes.cbHeader;
        std::memcpy(bufs[1U].pvBuffer, buf + offset, static_cast<size_t>(chunk));

        bufs[2U].BufferType = SECBUFFER_STREAM_TRAILER;
        bufs[2U].cbBuffer = m_streamSizes.cbTrailer;
        bufs[2U].pvBuffer = msg.data() + m_streamSizes.cbHeader + chunk;

        bufs[3U].BufferType = SECBUFFER_EMPTY;
        bufs[3U].cbBuffer = 0;
        bufs[3U].pvBuffer = nullptr;

        SecBufferDesc desc;
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers = 4;
        desc.pBuffers = bufs.data();

        ::EnterCriticalSection(&m_csCrypto);
        SECURITY_STATUS ss = ::EncryptMessage(&m_ctxHandle, 0, &desc, 0);
        ::LeaveCriticalSection(&m_csCrypto);
        if (ss != SEC_E_OK)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: EncryptMessage failed: %s\n"), HrStr(ss).c_str());
            return false;
        }

        DWORD totalEncrypted = bufs[0U].cbBuffer + bufs[1U].cbBuffer + bufs[2U].cbBuffer;
        if (!RawSend(sock, msg.data(), totalEncrypted))
        {
            return false;
        }

        offset += chunk;
    }
    return true;
}

bool SchannelTLS::Recv(VSocket* const sock, char* const buf, const int len, int* received)
{
    *received = 0;
    if (!m_ready)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: Recv called but not ready\n"));
        return false;
    }

    // Drain any already-decrypted plaintext first
    if (!m_plainBuf.empty())
    {
        int take = min(len, static_cast<int>(m_plainBuf.size()));
        std::memcpy(buf, m_plainBuf.data(), static_cast<size_t>(take));
        m_plainBuf.erase(m_plainBuf.begin(), m_plainBuf.begin() + take);
        *received = take;
        return true;
    }

    // Read raw encrypted bytes and decrypt
    for (;;)
    {
        // Try to decrypt what we have in m_rawBuf
        if (!m_rawBuf.empty())
        {
            std::array<SecBuffer, 4U> bufs;
            bufs[0U].BufferType = SECBUFFER_DATA;
            bufs[0U].cbBuffer = static_cast<ULONG>(m_rawBuf.size());
            bufs[0U].pvBuffer = m_rawBuf.data();
            bufs[1U].BufferType = SECBUFFER_EMPTY;
            bufs[2U].BufferType = SECBUFFER_EMPTY;
            bufs[3U].BufferType = SECBUFFER_EMPTY;
            for (int i{ 1 }; i < 4; ++i)
            {
                bufs[static_cast<unsigned long long>(i)].cbBuffer = 0;
                bufs[static_cast<unsigned long long>(i)].pvBuffer = nullptr;
            }

            SecBufferDesc desc;
            desc.ulVersion = SECBUFFER_VERSION;
            desc.cBuffers = 4;
            desc.pBuffers = bufs.data();

            ::EnterCriticalSection(&m_csCrypto);
            SECURITY_STATUS ss = ::DecryptMessage(&m_ctxHandle, &desc, 0, nullptr);
            ::LeaveCriticalSection(&m_csCrypto);

            if (ss == SEC_E_OK)
            {
                // Find data buffer
                BYTE* plain{ nullptr };
                DWORD plainLen{ 0U };
                BYTE* extra{ nullptr };
                DWORD extraLen{ 0U };

                for (int i{ 0 }; i < 4; ++i)
                {
                    if (bufs[static_cast<unsigned long long>(i)].BufferType == SECBUFFER_DATA)
                    {
                        plain = reinterpret_cast<BYTE*>(bufs[i].pvBuffer);
                        plainLen = bufs[static_cast<unsigned long long>(i)].cbBuffer;
                    }
                    else if (bufs[static_cast<unsigned long long>(i)].BufferType == SECBUFFER_EXTRA)
                    {
                        extra = reinterpret_cast<BYTE*>(bufs[i].pvBuffer);
                        extraLen = bufs[static_cast<unsigned long long>(i)].cbBuffer;
                    }
                }

                // plain and extra point inside m_rawBuf — copy plaintext out
                // before the buffer is cleared or reassigned.
                std::vector<BYTE> plainCopy;
                if (plain && (plainLen > 0))
                {
                    plainCopy.assign(plain, plain + plainLen);
                }

                // Remaining undecrypted bytes. extra points inside m_rawBuf,
                // so assign() would self-alias: copy via a temp buffer.
                if ((extraLen > 0) && extra)
                {
                    std::vector<BYTE> keep(extra, extra + extraLen);
                    m_rawBuf.swap(keep);
                }
                else
                {
                    m_rawBuf.clear();
                }

                // Return plaintext to caller
                if (!plainCopy.empty())
                {
                    int take = min(len, static_cast<int>(plainCopy.size()));
                    std::memcpy(buf, plainCopy.data(), static_cast<size_t>(take));
                    *received = take;
                    // Buffer the rest
                    if (take < static_cast<int>(plainCopy.size()))
                    {
                        m_plainBuf.insert(m_plainBuf.end(), plainCopy.begin() + take, plainCopy.end());
                    }
                    return true;
                }
                // else: decrypted to nothing (heartbeat etc.) — loop
                continue;
            }
            else if (ss == SEC_I_CONTEXT_EXPIRED)
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: peer sent close_notify\n"));
                return false;
            }
            else if (ss == SEC_E_INCOMPLETE_MESSAGE)
            {
                // Need more data — fall through to read more
            }
            else if (ss == SEC_I_RENEGOTIATE)
            {
                BYTE* extra{ nullptr };
                DWORD extraLen{ 0U };
                for (int i{ 0 }; i < 4; ++i)
                {
                    if (bufs[static_cast<unsigned long long>(i)].BufferType == SECBUFFER_EXTRA)
                    {
                        extra = static_cast<BYTE*>(bufs[static_cast<unsigned long long>(i)].pvBuffer);
                        extraLen = bufs[static_cast<unsigned long long>(i)].cbBuffer;
                        break;
                    }
                }
                std::vector<BYTE> handshakeData;
                if (extra && extraLen)
                {
                    handshakeData.assign(extra, extra + extraLen);
                }
                m_rawBuf.clear();
                SecBuffer postHandshakeExtra{};
                if (!HandshakeLoop(sock, true, &postHandshakeExtra, handshakeData.empty() ? nullptr : handshakeData.data(),
                                   static_cast<DWORD>(handshakeData.size())))
                {
                    return false;
                }
                if (postHandshakeExtra.cbBuffer && postHandshakeExtra.pvBuffer)
                {
                    BYTE* p = static_cast<BYTE*>(postHandshakeExtra.pvBuffer);
                    m_rawBuf.assign(p, p + postHandshakeExtra.cbBuffer);
                    ::LocalFree(postHandshakeExtra.pvBuffer);
                }
                SECURITY_STATUS qs = ::QueryContextAttributes(&m_ctxHandle, SECPKG_ATTR_STREAM_SIZES, &m_streamSizes);
                if (qs != SEC_E_OK)
                {
                    return false;
                }
                continue;
            }
            else
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: DecryptMessage failed: %s\n"), HrStr(ss).c_str());
                return false;
            }
        }

        // Read more raw bytes from socket
        std::array<BYTE, 4096U> tmp;
        DWORD got{ 0U };
        if (!RawRecv(sock, tmp.data(), sizeof(tmp), &got) || (got == 0))
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: Recv abort, rawbuf=%u plainbuf=%u\n"), static_cast<unsigned>(m_rawBuf.size()),
                         static_cast<unsigned>(m_plainBuf.size()));
            return false;
        }
        m_rawBuf.insert(m_rawBuf.end(), tmp.data(), tmp.data() + got);
    }
}

bool SchannelTLS::GetEndpointBinding(std::vector<BYTE>& outHash)
{
    // tls-server-end-point (RFC 5929): hash of the server certificate as sent
    // in the TLS Certificate message. Computed locally from m_certCtx so it
    // works identically regardless of whether the client sent a certificate.
    if (!m_certCtx)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: GetEndpointBinding with no certificate\n"));
        return false;
    }
    return ChannelBinding::ServerEndPointHash(m_certCtx, outHash);
}

void SchannelTLS::Shutdown(VSocket* const sock)
{
    if (!m_ctxValid)
    {
        return;
    }

    DWORD shutType = SCHANNEL_SHUTDOWN;
    SecBuffer shutBuf;
    shutBuf.cbBuffer = sizeof(shutType);
    shutBuf.BufferType = SECBUFFER_TOKEN;
    shutBuf.pvBuffer = &shutType;

    SecBufferDesc shutDesc;
    shutDesc.ulVersion = SECBUFFER_VERSION;
    shutDesc.cBuffers = 1;
    shutDesc.pBuffers = &shutBuf;

    ::ApplyControlToken(&m_ctxHandle, &shutDesc);

    // Build close_notify record
    SecBuffer outBuf;
    outBuf.cbBuffer = 0;
    outBuf.BufferType = SECBUFFER_TOKEN;
    outBuf.pvBuffer = nullptr;

    SecBufferDesc outDesc;
    outDesc.ulVersion = SECBUFFER_VERSION;
    outDesc.cBuffers = 1;
    outDesc.pBuffers = &outBuf;

    DWORD ctxAttr{ 0U };
    TimeStamp tsExpiry;
    ::AcceptSecurityContext(&m_credHandle, &m_ctxHandle, nullptr,
                          ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR |
                              ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM,
                          SECURITY_NATIVE_DREP, &m_ctxHandle, &outDesc, &ctxAttr, &tsExpiry);

    if (outBuf.pvBuffer && outBuf.cbBuffer)
    {
        RawSend(sock, reinterpret_cast<BYTE*>(outBuf.pvBuffer), outBuf.cbBuffer);
        ::FreeContextBuffer(outBuf.pvBuffer);
    }

    m_ready = false;
}

// -----------------------------------------------------------------------
// Raw socket I/O (bypasses VSocket's HTTP wrappers to use the SOCKET directly)
// -----------------------------------------------------------------------

bool SchannelTLS::RawSend(VSocket* const sock, const BYTE* const data, const DWORD len)
{
    DWORD sent{ 0U };
    while (sent < len)
    {
        int n = sock->Send(reinterpret_cast<const char*>(data) + sent, static_cast<VCard>((len - sent)));
        if (n <= 0)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("TLS: RawSend failed n=%d wsa=%d\n"), n, ::WSAGetLastError());
            return false;
        }
        sent += static_cast<DWORD>(n);
    }
    return true;
}

bool SchannelTLS::RawRecv(VSocket* const sock, BYTE* const data, const DWORD len, DWORD* received)
{
    // We need at least 1 byte; use non-exact read to avoid blocking on full len
    int n = sock->Read(reinterpret_cast<char*>(data), static_cast<int>(len));
    if (n <= 0)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("TLS: RawRecv failed n=%d wsa=%d\n"), n, ::WSAGetLastError());
        *received = 0;
        return false;
    }
    *received = static_cast<DWORD>(n);
    return true;
}
