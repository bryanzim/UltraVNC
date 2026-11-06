// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// VeNCrypt authentication with TLS encrypted transport

#include "stdhdrs.h"

#include "AuthDialog.h"
#include "ClientConnection.h"
#include "common/ChannelBinding.h"
#include "common/SaslWire.h"
#include "common/ScramSha256.h"
#include "Exception.h"
#include "rfb.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#define SECURITY_WIN32
#include <security.h>
#include <winternl.h>

#define SCHANNEL_USE_BLACKLISTS
#include <CommCtrl.h>
#include <cryptuiapi.h>
#include <schannel.h>
#include <Shlwapi.h>
#include <wincrypt.h>

#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "cryptui.lib")

// Dynamicly sized I/O buffers

struct DynBuffer
{
    static const int CapacityStep{ 1024 };

    BYTE* buffer;
    DWORD pos, size, capacity;

    explicit DynBuffer() : buffer(nullptr), pos{ 0 }, size{ 0 }, capacity{ 0 } {}
    ~DynBuffer() { delete[] buffer; }

    BYTE* EnsureFree(const DWORD avail)
    {
        if ((pos > 0) && (pos == size))
        {
            pos = size = 0;
        }
        if ((pos > 0) && (size + avail > capacity))
        {
            size -= pos;
            std::memcpy(buffer, buffer + pos, size);
            pos = 0;
        }
        if (size + avail > capacity)
        {
            capacity = (size + avail + CapacityStep - 1) & static_cast<DWORD>(-CapacityStep);
            BYTE* prev = buffer;
            buffer = new BYTE[capacity];
            std::memcpy(buffer, prev, size);
            delete[] prev;
        }
        return buffer + size;
    }

    inline BYTE* GetHead() { return buffer + pos; }

    inline BYTE* GetTail() { return buffer + size; }

    inline int GetAvailable() { return maximum(static_cast<int>(size - pos), 0); }

    inline int GetFree() { return maximum(static_cast<int>(capacity - size), 0); }
};

// Schannel based TLS wrapper implemented as a "Sans I/O" protocol

struct TLSSession
{
    enum State
    {
        StateNew = 0,
        StateClosed = 1,
        StateHandshakeStart = 2,
        StatePostHandshake = 8,
        StateShutdown = 9,
    };

    State state;
    wchar_t* hostName;
    bool isServer;
    PCCERT_CONTEXT pLocalCert, pRemoteCert;
    DWORD contextReq;
    CredHandle hCredentials;
    CtxtHandle hContext;
    SecBufferDesc inDesc, outDesc;
    std::array<SecBuffer, 5U> inBuffers;
    std::array<SecBuffer, 5U> outBuffers;
    SecPkgContext_StreamSizes tlsSizes;
    char lastError[1024U];
    std::array<wchar_t, 128U> cipherSuite{};
    bool modernCredentials;

    explicit TLSSession()
        : state(StateNew), hostName{ nullptr }, isServer{ false }, pLocalCert{ nullptr }, pRemoteCert{ nullptr }, contextReq{ 0 },
          modernCredentials{ false }
    {
        SecInvalidateHandle(&hCredentials)
        SecInvalidateHandle(&hContext)
        inDesc.ulVersion = SECBUFFER_VERSION;
        inDesc.cBuffers = static_cast<unsigned long>(inBuffers.size());
        inDesc.pBuffers = inBuffers.data();
        std::memset(inBuffers.data(), 0, sizeof(inBuffers));
        outDesc.ulVersion = SECBUFFER_VERSION;
        outDesc.cBuffers = static_cast<unsigned long>(outBuffers.size());
        outDesc.pBuffers = outBuffers.data();
        std::memset(outBuffers.data(), 0, sizeof(outBuffers));
    }

    TLSSession(TLSSession& from)
    {
        std::memcpy(this, &from, sizeof(TLSSession));
        inDesc.pBuffers = inBuffers.data();
        outDesc.pBuffers = outBuffers.data();
        SecInvalidateHandle(&from.hContext)
        SecInvalidateHandle(&from.hCredentials)
        from.pLocalCert = from.pRemoteCert = nullptr;
    }

    ~TLSSession()
    {
        if (SecIsValidHandle(&hContext))
        {
            ::DeleteSecurityContext(&hContext);
        }
        if (SecIsValidHandle(&hCredentials))
        {
            ::FreeCredentialsHandle(&hCredentials);
        }
        if (pLocalCert)
        {
            ::CertFreeCertificateContext(pLocalCert);
        }
        if (pRemoteCert)
        {
            ::CertFreeCertificateContext(pRemoteCert);
        }
    }

    void Init(TCHAR* const _hostName, const bool _isServer = false, const PCERT_CONTEXT _pLocalCert = nullptr)
    {
        static std::array<wchar_t, MAX_HOST_NAME_LEN> wHostName;
        if (_hostName)
        {
            ::wcsncpy_s(wHostName.data(), wHostName.size(), _hostName, MAX_HOST_NAME_LEN);
        }
        else
        {
            wHostName[0U] = 0;
        }
        hostName = wHostName.data();
        isServer = _isServer;
        pLocalCert = _pLocalCert;
        state = StateHandshakeStart;
    }

    bool IsReady() { return state >= StatePostHandshake; }

    bool Handshake(DynBuffer& inbuf, DynBuffer& outbuf)
    {
        HRESULT hr;
        DWORD contextAttr;

        if (state == StateClosed)
        {
            return SetLastError("Connection closed");
        }
        if (contextReq == 0)
        {
            contextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
                         ISC_REQ_EXTENDED_ERROR | ISC_REQ_STREAM;
        }
        if (!SecIsValidHandle(&hCredentials))
        {
            SCHANNEL_CRED cred{ 0 };
            cred.dwVersion = SCHANNEL_CRED_VERSION;
            cred.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION;
            cred.grbitEnabledProtocols = isServer ? SP_PROT_TLS1_2_SERVER : SP_PROT_TLS1_2_CLIENT;
            if (pLocalCert)
            {
                cred.cCreds = 1;
                cred.paCred = &pLocalCert;
                contextReq |= ISC_REQ_USE_SUPPLIED_CREDS;
            }
            if (TRUE) // SCH_CREDENTIALS is available on Windows 10 1809+ / Server 2019+
            {
                SCH_CREDENTIALS newCred{ 0 };
                TLS_PARAMETERS params{ 0 };
                newCred.dwVersion = SCH_CREDENTIALS_VERSION;
                newCred.cCreds = cred.cCreds;
                newCred.paCred = cred.paCred;
                newCred.dwFlags = cred.dwFlags | SCH_USE_STRONG_CRYPTO;
                newCred.cTlsParameters = 1;
                newCred.pTlsParameters = &params;
                // Disable only legacy protocols; let Schannel negotiate TLS 1.2 or 1.3.
                if (isServer)
                {
                    params.grbitDisabledProtocols =
                        SP_PROT_SSL2_SERVER | SP_PROT_SSL3_SERVER | SP_PROT_TLS1_0_SERVER | SP_PROT_TLS1_1_SERVER;
                }
                else
                {
                    params.grbitDisabledProtocols =
                        SP_PROT_SSL2_CLIENT | SP_PROT_SSL3_CLIENT | SP_PROT_TLS1_0_CLIENT | SP_PROT_TLS1_1_CLIENT;
                }
                hr = ::AcquireCredentialsHandleW(nullptr, UNISP_NAME, (isServer ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND),
                                               nullptr, &newCred, nullptr, nullptr, &hCredentials, nullptr);
                modernCredentials = SUCCEEDED(hr);
            }
            else
            {
                hr = -1;
            }
            if (FAILED(hr))
            {
                hr = ::AcquireCredentialsHandleW(nullptr, UNISP_NAME, (isServer ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND),
                                               nullptr, &cred, nullptr, nullptr, &hCredentials, nullptr);
                modernCredentials = false;
            }
            if (FAILED(hr))
            {
                return SetLastError("AcquireCredentialsHandleW failed", hr);
            }
        }
        bool done{ false };
        while (!done)
        {
            PSecBufferDesc pIn = nullptr;
            if (inbuf.size > 0)
            {
                inBuffers[0U].BufferType = SECBUFFER_TOKEN;
                inBuffers[0U].pvBuffer = inbuf.GetHead();
                inBuffers[0U].cbBuffer = static_cast<unsigned long>(inbuf.GetAvailable());
                pIn = &inDesc;
            }
            bool initialClientCall = !isServer && !SecIsValidHandle(&hContext) && (pIn == nullptr);
            if (isServer)
            {
                hr = ::AcceptSecurityContext(&hCredentials, SecIsValidHandle(&hContext) ? &hContext : nullptr, pIn, contextReq,
                                           SECURITY_NATIVE_DREP, &hContext, &outDesc, &contextAttr, nullptr);
            }
            else
            {
                hr = ::InitializeSecurityContextW(&hCredentials, SecIsValidHandle(&hContext) ? &hContext : nullptr, hostName,
                                                contextReq, 0, SECURITY_NATIVE_DREP, pIn, 0, &hContext, &outDesc, &contextAttr,
                                                nullptr);
            }
            if (hr == SEC_E_INCOMPLETE_MESSAGE)
            {
                std::memset(inBuffers.data(), 0, sizeof(inBuffers));
                break;
            }
            if ((hr == SEC_E_INVALID_TOKEN) && modernCredentials && initialClientCall)
            {
                if (SecIsValidHandle(&hContext))
                {
                    ::DeleteSecurityContext(&hContext);
                    SecInvalidateHandle(&hContext)
                }
                ::FreeCredentialsHandle(&hCredentials);
                SecInvalidateHandle(&hCredentials)
                SCHANNEL_CRED legacyCred{ 0 };
                legacyCred.dwVersion = SCHANNEL_CRED_VERSION;
                legacyCred.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION;
                legacyCred.grbitEnabledProtocols = isServer ? SP_PROT_TLS1_2_SERVER : SP_PROT_TLS1_2_CLIENT;
                if (pLocalCert)
                {
                    legacyCred.cCreds = 1;
                    legacyCred.paCred = &pLocalCert;
                }
                hr = ::AcquireCredentialsHandleW(nullptr, UNISP_NAME, (isServer ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND),
                                               nullptr, &legacyCred, nullptr, nullptr, &hCredentials, nullptr);
                if (FAILED(hr))
                {
                    return SetLastError("AcquireCredentialsHandleW failed", hr);
                }
                modernCredentials = false;
                for (DWORD i{ 0U }; i < outDesc.cBuffers; ++i)
                {
                    if (outBuffers[i].pvBuffer)
                    {
                        ::FreeContextBuffer(outBuffers[i].pvBuffer);
                    }
                }
                std::memset(outBuffers.data(), 0, sizeof(outBuffers));
                continue;
            }
            inbuf.size = 0;
            if (FAILED(hr))
            {
                return SetLastError(isServer ? "AcceptSecurityContext failed" : "InitializeSecurityContextW failed", hr);
            }
            for (DWORD i{ 0U }; i < inDesc.cBuffers; ++i)
            {
                if ((inBuffers[i].cbBuffer > 0) && (inBuffers[i].BufferType == SECBUFFER_EXTRA))
                {
                    BYTE* dst = inbuf.EnsureFree(inBuffers[i].cbBuffer);
                    std::memcpy(dst, inBuffers[i].pvBuffer, inBuffers[i].cbBuffer);
                    inbuf.size += inBuffers[i].cbBuffer;
                }
            }
            std::memset(inBuffers.data(), 0, sizeof(inBuffers));
            for (DWORD i{ 0U }; i < outDesc.cBuffers; ++i)
            {
                if (outBuffers[i].cbBuffer > 0)
                {
                    if (outBuffers[i].BufferType == SECBUFFER_TOKEN)
                    {
                        BYTE* dst = outbuf.EnsureFree(outBuffers[i].cbBuffer);
                        std::memcpy(dst, outBuffers[i].pvBuffer, outBuffers[i].cbBuffer);
                        outbuf.size += outBuffers[i].cbBuffer;
                    }
                    ::FreeContextBuffer(outBuffers[i].pvBuffer);
                }
            }
            std::memset(outBuffers.data(), 0, sizeof(outBuffers));
            switch (hr)
            {
            case SEC_I_CONTINUE_NEEDED:
                // do nothing
                break;
            case SEC_E_OK:
                hr = ::QueryContextAttributes(&hContext, SECPKG_ATTR_STREAM_SIZES, &tlsSizes);
                if (FAILED(hr))
                {
                    return SetLastError("QueryContextAttributes failed", hr);
                }
                if ((tlsSizes.cBuffers > inBuffers.size()) || (tlsSizes.cBuffers > outBuffers.size()))
                {
                    return SetLastError("Not enough I/O buffers");
                }
                inDesc.cBuffers = tlsSizes.cBuffers;
                outDesc.cBuffers = tlsSizes.cBuffers;
                ::QueryContextAttributes(&hContext, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &pRemoteCert);
                state = StatePostHandshake;
                done = true;
                {
                    SecPkgContext_CipherInfo cipherInfo;
                    SecPkgContext_ConnectionInfo connInfo;
                    if (::QueryContextAttributes(&hContext, SECPKG_ATTR_CIPHER_INFO, &cipherInfo) == S_OK)
                    {
                        vnclog.Print(0, _T("TLSSession: Using %ls (0x%X) ciphersuite\n"), cipherInfo.szCipherSuite,
                                     cipherInfo.dwCipherSuite);
                        ::wcsncpy_s(cipherSuite.data(), cipherSuite.size(), cipherInfo.szCipherSuite, _TRUNCATE);
                    }
                    if (::QueryContextAttributes(&hContext, SECPKG_ATTR_CONNECTION_INFO, &connInfo) == S_OK)
                    {
                        vnclog.Print(0, _T("TLSSession: %s using %s cipher with %s hash and %s key-exchange\n"),
                                     GetAlgName(connInfo.dwProtocol), GetAlgName(connInfo.aiCipher), GetAlgName(connInfo.aiHash),
                                     GetAlgName(connInfo.aiExch));
                    }
                }
                break;
            case SEC_I_CONTEXT_EXPIRED:
                state = StateShutdown;
                return SetLastError("Handshake aborted", hr);
            default:
                return SetLastError("Handshake cannot be completed", hr);
            }
            if (inbuf.size == 0)
            {
                break;
            }
        }
        return true;
    }

    bool Send(BYTE* const pDataBuffer, const int nDataLen, DynBuffer& outbuf)
    {
        if ((state == StateClosed) || !SecIsValidHandle(&hContext))
        {
            return SetLastError("Connection closed");
        }
        int cnt = static_cast<int>((nDataLen + tlsSizes.cbMaximumMessage - 1) / tlsSizes.cbMaximumMessage);
        outbuf.EnsureFree(tlsSizes.cbHeader * static_cast<unsigned long>(cnt) + static_cast<unsigned long>(nDataLen) + tlsSizes.cbTrailer * static_cast<unsigned long>(cnt));
        for (int i{ 0 }; i < nDataLen; i += tlsSizes.cbMaximumMessage)
        {
            int size = minimum(nDataLen - i, static_cast<int>(tlsSizes.cbMaximumMessage));
            BYTE* dst = outbuf.EnsureFree(tlsSizes.cbHeader + static_cast<unsigned long>(size) + tlsSizes.cbTrailer);
            outBuffers[0U].BufferType = SECBUFFER_STREAM_HEADER;
            outBuffers[0U].pvBuffer = dst;
            outBuffers[0U].cbBuffer = tlsSizes.cbHeader;
            dst += tlsSizes.cbHeader;
            outBuffers[1U].BufferType = SECBUFFER_DATA;
            outBuffers[1U].pvBuffer = dst;
            outBuffers[1U].cbBuffer = static_cast<unsigned long>(size);
            std::memcpy(dst, pDataBuffer + i, static_cast<size_t>(size));
            dst += size;
            outBuffers[2U].BufferType = SECBUFFER_STREAM_TRAILER;
            outBuffers[2U].pvBuffer = dst;
            outBuffers[2U].cbBuffer = tlsSizes.cbTrailer;
            for (DWORD iInner{ 3U }; iInner < inDesc.cBuffers; ++iInner)
            {
                outBuffers[iInner].BufferType = SECBUFFER_EMPTY;
            }
            HRESULT hr = ::EncryptMessage(&hContext, 0, &outDesc, 0);
            if (FAILED(hr))
            {
                return SetLastError("EncryptMessage failed", hr);
            }
            // use cbBuffer sizes as returned by EncryptMessage in buffers, not original sizes from tlsSizes
            outbuf.size += outBuffers[0U].cbBuffer + outBuffers[1U].cbBuffer + outBuffers[2U].cbBuffer;
            std::memset(outBuffers.data(), 0, sizeof(outBuffers));
        }
        return true;
    }

    bool Receive(DynBuffer& inbuf, DynBuffer& plainbuf, DynBuffer& outbuf)
    {
        if ((state == StateClosed) || !SecIsValidHandle(&hContext))
        {
            return SetLastError("Connection closed");
        }
        if ((state == StateHandshakeStart) && (inbuf.size > 0))
        {
            if (!Handshake(inbuf, outbuf))
            {
                return false;
            }
            if (!IsReady() || (outbuf.GetAvailable() > 0))
            {
                return true;
            }
        }
        bool done{ false };
        while (!done && (inbuf.size > 0))
        {
            inBuffers[0U].BufferType = SECBUFFER_DATA;
            inBuffers[0U].pvBuffer = inbuf.GetHead();
            inBuffers[0U].cbBuffer = static_cast<unsigned long>(inbuf.GetAvailable());
            HRESULT hr = ::DecryptMessage(&hContext, &inDesc, 0, nullptr);
            if (hr == SEC_E_INCOMPLETE_MESSAGE)
            {
                std::memset(inBuffers.data(), 0, sizeof(inBuffers));
                return true;
            }
            if (hr == SEC_E_INVALID_HANDLE)
            { // session on hContext already closed
                break;
            }
            if (FAILED(hr))
            {
                return SetLastError("DecryptMessage failed", hr);
            }
            inbuf.size = 0;
            for (DWORD i{ 0U }; i < inDesc.cBuffers; ++i)
            {
                if (inBuffers[i].cbBuffer > 0)
                {
                    if (inBuffers[i].BufferType == SECBUFFER_DATA)
                    {
                        BYTE* dst = plainbuf.EnsureFree(inBuffers[i].cbBuffer);
                        std::memcpy(dst, inBuffers[i].pvBuffer, inBuffers[i].cbBuffer);
                        plainbuf.size += inBuffers[i].cbBuffer;
                    }
                    else if (inBuffers[i].BufferType == SECBUFFER_EXTRA)
                    {
                        BYTE* dst = inbuf.EnsureFree(inBuffers[i].cbBuffer);
                        void* src = inBuffers[i].pvBuffer;
                        if (!src)
                        {
                            src = inbuf.buffer + inBuffers[0U].cbBuffer - inBuffers[i].cbBuffer;
                        }
                        std::memcpy(dst, src, inBuffers[i].cbBuffer);
                        inbuf.size += inBuffers[i].cbBuffer;
                    }
                }
            }
            std::memset(inBuffers.data(), 0, sizeof(inBuffers));
            switch (hr)
            {
            case SEC_E_OK:
                break;
            case SEC_I_RENEGOTIATE:
                state = StateHandshakeStart;
                if (!Handshake(inbuf, outbuf))
                {
                    return false;
                }
                done = true;
                break;
            case SEC_I_CONTEXT_EXPIRED:
                state = StateShutdown;
                done = true;
                break;
            default:
                return SetLastError("Unexpected DecryptMessage result", hr);
            }
        }
        return true;
    }

    bool Shutdown(DynBuffer& outbuf)
    {
        HRESULT hr;
        DWORD contextAttr;
        DWORD type = SCHANNEL_SHUTDOWN;

        inBuffers[0U].BufferType = SECBUFFER_TOKEN;
        inBuffers[0U].pvBuffer = &type;
        inBuffers[0U].cbBuffer = sizeof(type);
        // note: passing more than one input buffer fails w/ SEC_E_INVALID_TOKEN (&H80090308)
        inDesc.cBuffers = 1;
        hr = ::ApplyControlToken(&hContext, &inDesc);
        inDesc.cBuffers = tlsSizes.cBuffers;
        if (FAILED(hr))
        {
            return SetLastError("ApplyControlToken failed", hr);
        }
        outBuffers[0U].BufferType = SECBUFFER_TOKEN;
        if (isServer)
        {
            hr = ::AcceptSecurityContext(&hCredentials, &hContext, nullptr, contextReq, SECURITY_NATIVE_DREP, &hContext, &outDesc,
                                       &contextAttr, nullptr);
        }
        else
        {
            hr = ::InitializeSecurityContextW(&hCredentials, &hContext, hostName, contextReq, 0, SECURITY_NATIVE_DREP, nullptr, 0,
                                            &hContext, &outDesc, &contextAttr, nullptr);
        }
        if (FAILED(hr))
        {
            return SetLastError(isServer ? "AcceptSecurityContext failed" : "InitializeSecurityContextW failed", hr);
        }
        std::memset(inBuffers.data(), 0, sizeof(inBuffers));
        for (DWORD i{ 0U }; i < outDesc.cBuffers; ++i)
        {
            if (outBuffers[i].cbBuffer > 0)
            {
                if (outBuffers[i].BufferType == SECBUFFER_TOKEN)
                {
                    BYTE* dst = outbuf.EnsureFree(outBuffers[i].cbBuffer);
                    std::memcpy(dst, outBuffers[i].pvBuffer, outBuffers[i].cbBuffer);
                    outbuf.size += outBuffers[i].cbBuffer;
                }
                ::FreeContextBuffer(outBuffers[i].pvBuffer);
            }
        }
        std::memset(outBuffers.data(), 0, sizeof(outBuffers));
        return true;
    }

    bool ValidateRemoteCertificate()
    {
        bool isvalid{ false };
        std::wstring host = hostName ? hostName : L"";
        PCCERT_CHAIN_CONTEXT pChainContext = nullptr;
        CERT_CHAIN_PARA chainParams{ 0 };
        CERT_CHAIN_POLICY_PARA policyPara{ 0 };
        CERT_CHAIN_POLICY_STATUS policyStatus{ 0 };
        HTTPSPolicyCallbackData httpsPolicy{ 0 };

        chainParams.cbSize = sizeof(chainParams);
        if (!::CertGetCertificateChain(nullptr, pRemoteCert, nullptr, nullptr, &chainParams, CERT_CHAIN_REVOCATION_CHECK_CHAIN, nullptr,
                                     &pChainContext))
        {
            SetLastError("CertGetCertificateChain failed", static_cast<HRESULT>(FACILITY_WIN32 | ::GetLastError()));
            goto Exit;
        }
        if (pChainContext->cChain == 0)
        {
            SetLastError(nullptr, CERT_E_CHAINING);
            goto Exit;
        }
        httpsPolicy.cbSize = sizeof(httpsPolicy);
        httpsPolicy.dwAuthType = (isServer ? AUTHTYPE_CLIENT : AUTHTYPE_SERVER);
        httpsPolicy.pwszServerName = (isServer ? nullptr : const_cast<WCHAR*>(reinterpret_cast<const WCHAR *>(host.c_str())));
        policyPara.cbSize = sizeof(policyPara);
        policyPara.pvExtraPolicyPara = &httpsPolicy;
        policyStatus.cbSize = sizeof(policyStatus);
        if (!::CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, pChainContext, &policyPara, &policyStatus))
        {
            SetLastError("CertVerifyCertificateChainPolicy failed", static_cast<HRESULT>(FACILITY_WIN32 | ::GetLastError()));
            goto Exit;
        }
        if (FAILED(policyStatus.dwError))
        {
            SetLastError(nullptr, static_cast<HRESULT>(policyStatus.dwError));
            goto Exit;
        }
        isvalid = true;
    Exit:
        if (pChainContext)
        {
            ::CertFreeCertificateChain(pChainContext);
        }
        return isvalid;
    }

private:
    static std::wstring toWide(const char* const src)
    {
        std::wstring dst(std::strlen(src), L' ');
        dst.resize(mbstowcs(&dst[0U], src, std::strlen(src)));
        return dst;
    }

    char* GetAlgName(const ALG_ID algId)
    {
        switch (algId)
        {
        case SP_PROT_SSL2_SERVER:
            return "SSL2_SERVER";
        case SP_PROT_SSL2_CLIENT:
            return "SSL2_CLIENT";
        case SP_PROT_SSL3_SERVER:
            return "SSL3_SERVER";
        case SP_PROT_SSL3_CLIENT:
            return "SSL3_CLIENT";
        case SP_PROT_TLS1_0_SERVER:
            return "TLS1_0_SERVER";
        case SP_PROT_TLS1_0_CLIENT:
            return "TLS1_0_CLIENT";
        case SP_PROT_TLS1_1_SERVER:
            return "TLS1_1_SERVER";
        case SP_PROT_TLS1_1_CLIENT:
            return "TLS1_1_CLIENT";
        case SP_PROT_TLS1_2_SERVER:
            return "TLS1_2_SERVER";
        case SP_PROT_TLS1_2_CLIENT:
            return "TLS1_2_CLIENT";
        case SP_PROT_TLS1_3_SERVER:
            return "TLS1_3_SERVER";
        case SP_PROT_TLS1_3_CLIENT:
            return "TLS1_3_CLIENT";
        case CALG_RC2:
            return "RC2";
        case CALG_RC4:
            return "RC4";
        case CALG_DES:
            return "DES";
        case CALG_3DES:
            return "3DES";
        case CALG_AES_128:
            return "AES_128";
        case CALG_AES_192:
            return "AES_192";
        case CALG_AES_256:
            return "AES_256";
        case CALG_MD2:
            return "MD2";
        case CALG_MD5:
            return "MD5";
        case CALG_SHA1:
            return "SHA1";
        case CALG_SHA_256:
            return "SHA_256";
        case CALG_SHA_384:
            return "SHA_384";
        case CALG_SHA_512:
            return "SHA_512";
        case CALG_RSA_KEYX:
            return "RSA_KEYX";
        case CALG_RSA_SIGN:
            return "RSA_SIGN";
        case CALG_DH_EPHEM:
            return "DH_EPHEM";
        case CALG_ECDH:
            return "ECDH";
        case CALG_ECDH_EPHEM:
            return "ECDH_EPHEM";
        default:
            ::sprintf_s(lastError, "0x%X", algId);
            return lastError;
        }
    }

    bool SetLastError(const char* const error, const HRESULT hr = S_OK)
    {
        if (hr != S_OK)
        {
            std::array<wchar_t, 1024U> wmsg;
            ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(hr), 0, wmsg.data(), static_cast<DWORD>(wmsg.size()),
                           nullptr);
            for (int i = static_cast<int>(std::wcslen(wmsg.data())) - 1; (i >= 0) && !wmsg[static_cast<unsigned long long>(i + 1)]; --i)
            {
                if ((wmsg[static_cast<unsigned long long>(i)] == L'\r') || (wmsg[static_cast<unsigned long long>(i)] == L'\n') || (wmsg[static_cast<unsigned long long>(i)] == L'.'))
                {
                    wmsg[static_cast<unsigned long long>(i)] = L'\0';
                }
            }
            std::array<char, 1024U> msg;
            ::WideCharToMultiByte(CP_ACP, 0, wmsg.data(), -1, msg.data(), sizeof(msg), nullptr, nullptr);
            if (error)
            {
                ::sprintf_s(lastError, "%s. %s (0x%X)", error, msg.data(), hr);
            }
            else
            {
                ::sprintf_s(lastError, "%s (0x%X)", msg.data(), hr);
            }
        }
        else
        {
            ::strcpy_s(lastError, error);
        }
        return false;
    }
};

// Encryption/decryption plugin based on TLS session

struct TLSPlugin : public IPlugin
{
    TLSSession session;
    DynBuffer encBuffer, decBuffer, decPlain;
    int wanted;
    ClientConnection& connection;

    TLSPlugin(TLSSession& s, ClientConnection& c) : session(s), connection(c) {}

    virtual ~TLSPlugin() {}

    virtual BYTE* TransformBuffer(BYTE* pDataBuffer, int nDataLen, int* pnTransformedDataLen)
    {
        if (!session.Send(pDataBuffer, nDataLen, encBuffer))
        {
            SetLastError(session.lastError);
        }
        *pnTransformedDataLen = static_cast<int>(encBuffer.size);
        encBuffer.size = 0;
        return encBuffer.buffer;
    }

    virtual BYTE* RestoreBuffer(BYTE* pTransBuffer, int nTransDataLen, int* pnRestoredDataLen)
    {
        if (!pTransBuffer)
        {
            wanted = nTransDataLen;
            if (decPlain.GetAvailable() >= wanted)
            {
                *pnRestoredDataLen = 0;
                return decBuffer.buffer; // anything non-NULL
            }
            *pnRestoredDataLen = 1;
            BYTE* dst = decBuffer.EnsureFree(static_cast<DWORD>(*pnRestoredDataLen));
            decBuffer.size += static_cast<DWORD>(*pnRestoredDataLen);
            return dst;
        }
        if (decBuffer.GetAvailable() > 0)
        {
            DynBuffer outBuffer;
            if (!session.Receive(decBuffer, decPlain, outBuffer))
            {
                SetLastError(session.lastError);
            }
            if ((outBuffer.GetAvailable() > 0) && !connection.Write(reinterpret_cast<char*>(outBuffer.GetHead()), outBuffer.GetAvailable(), false))
            {
                SetLastError("TLS handshake response could not be sent");
            }
        }
        if (decPlain.GetAvailable() >= wanted)
        {
            *pnRestoredDataLen = wanted;
            std::memcpy(pTransBuffer, decPlain.GetHead(), static_cast<size_t>(wanted));
            decPlain.pos += static_cast<DWORD>(wanted);
        }
        else
        {
            *pnRestoredDataLen = -1;
        }
        return nullptr;
    }

private:
    static bool SetLastError(const char* const error)
    {
        vnclog.Print(0, _T("TLSPlugin: %s\n"), error);
        std::array<wchar_t, 256U> werror;
        ::MultiByteToWideChar(CP_UTF8, 0, error, -1, werror.data(), 256);
        throw WarningException(werror.data());
        return false;
    }
};

// VeNCrypt subtype constants are now in rfb/rfbproto.h (secTypePlain, secTypeTLSNone, etc.).

static const TCHAR* ThumbprintSection = _T("thumbprint");

struct ThumbHost
{
    TCHAR *key, *fname;

    ThumbHost(TCHAR* k, TCHAR* f) : key(k), fname(f) {}

    bool CompareThumbprint(char* const hex)
    {
        std::array<TCHAR, 64U> buf{ 0 };
        ::GetPrivateProfileString(ThumbprintSection, key, nullptr, buf.data(), static_cast<DWORD>(buf.size()), fname);
        return ::_tcscmp(reinterpret_cast<LPCTSTR>(hex), buf.data()) == 0;
    }

    void SaveThumbprint(char* const hex) { ::WritePrivateProfileString(ThumbprintSection, key, reinterpret_cast<LPCTSTR>(hex), fname); }
};

static bool GetCertificateThumbprint(const PCCERT_CONTEXT pCert, char* const hex, const int hexsize)
{
    std::array<BYTE, 64U> thumb{ 0 };
    DWORD size = sizeof(thumb);
    if (!pCert || !::CertGetCertificateContextProperty(pCert, CERT_HASH_PROP_ID, thumb.data(), &size))
    {
        return false;
    }
    std::snprintf(hex, static_cast<size_t>(hexsize), "%02x", thumb[0U]);
    for (DWORD i{ 1U }; i < size; ++i)
    {
        std::snprintf(hex + std::strlen(hex), static_cast<size_t>(hexsize) - std::strlen(hex), "-%02x", thumb[i]);
    }
    return true;
}

void ClientConnection::AuthVeNCrypt()
{
    int version, temp, size, subType;
    auto SetLastError = [](char* const error) {
        vnclog.Print(0, _T("AuthVeNCrypt: %hs\n"), error);
        std::array<wchar_t, 256U> werror;
        ::MultiByteToWideChar(CP_UTF8, 0, error, -1, werror.data(), 256);
        throw WarningException(werror.data());
    };

    version = 0;
    ReadExact(reinterpret_cast<char*>(&version), 2);
    vnclog.Print(0, _T("AuthVeNCrypt: raw version bytes %02x %02x\n"), (reinterpret_cast<unsigned char*>(&version))[0U],
                 (reinterpret_cast<unsigned char*>(&version))[1U]);
    version = Swap16IfLE(version);
    if (version >= 2)
    {
        version = Swap16IfLE(2);
        WriteExact(reinterpret_cast<char*>(&version), 2);
    }
    else
    {
        version = 0;
        WriteExact(reinterpret_cast<char*>(&version), 2);
        return SetLastError("Unsupported version");
    }
    temp = 0;
    ReadExact(reinterpret_cast<char*>(&temp), 1);
    if (temp)
    {
        return SetLastError("Server reported unsupported version");
    }
    size = 0;
    ReadExact(reinterpret_cast<char*>(&size), 1);
    subType = -1;
    for (int i{ 0 }; i < size; ++i)
    {
        ReadExact(reinterpret_cast<char*>(&temp), 4);
        {
            temp = static_cast<int>(Swap32IfLE(temp));
            // Prefer SASL subtypes (X509SASL > others) so an updated viewer
            // uses SCRAM(-PLUS) when the server offers it; servers that don't
            // list SASL keep the previous first-supported-wins behaviour.
            // Anonymous TLS* subtypes (257-259, 264, 266) are not supported:
            // Schannel cannot do anonymous TLS.
            switch (temp)
            {
            case secTypeX509MsLogonIII:
                subType = temp; // exclusive when server policy requires MS-Logon
                break;
            case secTypeX509SASL:
                if (subType != secTypeX509MsLogonIII)
                {
                    subType = temp;
                } // strongest standard subtype: SASL over X509 TLS
                break;
            case secTypeX509None:
            case secTypeX509Vnc:
            case secTypeX509Plain:
            case secTypeTokenTLS:
            case rfbVncAuth:
            case secTypePlain:
                if (subType < 0)
                {
                    subType = temp;
                }
                break;
            }
        }
    }
    if (subType < 0)
    {
        return SetLastError("No valid sub-type");
    }
    m_lastVeNCryptSubType = subType;
    temp = static_cast<int>(Swap32IfLE(subType));
    WriteExact(reinterpret_cast<char*>(&temp), 4);

    // tls-server-end-point (RFC 5929) channel binding data, filled after the
    // TLS handshake when a certificate-bearing subtype was negotiated.
    std::vector<BYTE> endpointHash;

    // if standard RFB type negotiated -> no further msgs and no TLS session
    if (subType >= 256)
    {
        // VeNCrypt spec (rfbproto 7.2.4) requires a 0x01 Ack before TLS for
        // TLS/X509 subtypes. Read it with the normal connection timeout so a
        // delayed Ack cannot be mistaken for the first byte of a TLS record.
        temp = 0;
        ReadExact(reinterpret_cast<char*>(&temp), 1);
        if (temp != 1)
        {
            return SetLastError("Server unsupported sub-type");
        }

        // start TLS on existing connection
        TLSSession session;
        session.Init(m_host.data());
        DynBuffer inbuf, outbuf;
        while (TRUE)
        {
            if (!session.Handshake(inbuf, outbuf))
            {
                return SetLastError(session.lastError);
            }
            size = outbuf.GetAvailable();
            if (size > 0)
            {
                WriteExact(reinterpret_cast<char*>(outbuf.GetHead()), size);
                outbuf.size = 0;
            }
            if (session.IsReady())
            {
                break;
            }
            size = 1;
            inbuf.EnsureFree(static_cast<DWORD>(size));
            ReadExact(reinterpret_cast<char*>(inbuf.GetTail()), size);
            inbuf.size += static_cast<DWORD>(size);
        }
        if (!session.ValidateRemoteCertificate())
        {
            std::array<TCHAR, MAX_HOST_NAME_LEN> key;
            ::_stprintf_s(key.data(), key.size(), _T("%s:%d"), m_host.data(), m_port);
            ThumbHost host(key.data(), m_opts->getDefaultOptionsFileName());
            std::array<char, 64U> hex;
            bool done{ false };
            if (GetCertificateThumbprint(session.pRemoteCert, hex.data(), sizeof(hex)) && host.CompareThumbprint(hex.data()))
            {
                done = true;
            }
            while (!done)
            {
                int nButtonPressed{ 0 };
                std::array<WCHAR, 1024U> msg;
                ::swprintf_s(msg.data(), msg.size(), L"%hs\n\nDo you want to continue?\n", session.lastError);
                TASKDIALOGCONFIG tc{ 0 };
                const TASKDIALOG_BUTTON buttons[] = { { IDNO, L"View Certificate" } };
                BOOL bPersist = FALSE;
                tc.cbSize = sizeof(tc);
                tc.dwFlags = TDF_SIZE_TO_CONTENT;
                tc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
                tc.pszWindowTitle = L"UltraVNC Viewer - Warning";
                tc.hInstance = m_hInstResDLL;
                tc.pszMainIcon = MAKEINTRESOURCEW(IDR_TRAY);
                tc.pszMainInstruction = L"Invalid server certificate";
                tc.pszContent = msg.data();
                tc.pButtons = buttons;
                tc.cButtons = _countof(buttons);
                tc.nDefaultButton = 1;
                tc.pszVerificationText = L"Don't ask anymore";
                ::TaskDialogIndirect(&tc, &nButtonPressed, nullptr, &bPersist);
                switch (nButtonPressed)
                {
                case IDNO: {
                    BOOL changed = FALSE;
                    CRYPTUI_VIEWCERTIFICATE_STRUCT vc{ 0 };
                    vc.dwSize = sizeof(vc);
                    vc.hwndParent = m_hwndStatus;
                    vc.pCertContext = session.pRemoteCert;
                    ::CryptUIDlgViewCertificate(&vc, &changed);
                    break;
                }
                case IDCANCEL:
                    if (!session.Shutdown(outbuf))
                    {
                        return SetLastError(session.lastError);
                    }
                    size = outbuf.GetAvailable();
                    if (size > 0)
                    {
                        WriteExact(reinterpret_cast<char*>(outbuf.GetHead()), size);
                        outbuf.size = 0;
                    }
                    QuietException_helper(L"Authentication cancelled");
                    break;
                default:
                    if (bPersist)
                    {
                        host.SaveThumbprint(hex.data());
                    }
                    done = true;
                    break;
                }
            }
        }
        // Hash the certificate the server presented while session.pRemoteCert
        // is still valid — TLSPlugin's move ctor takes ownership afterwards.
        if (session.pRemoteCert)
        {
            ChannelBinding::ServerEndPointHash(session.pRemoteCert, endpointHash);
        }
        m_fUsePlugin = true;
        m_pPluginInterface = new TLSPlugin(session, *this);

        // Human-readable encryption summary for the status window, e.g.
        // "VeNCrypt X509-SASL — TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384".
        const char* subName = subType == secTypeX509None         ? "X509-None"
                              : subType == secTypeX509Vnc        ? "X509-Vnc"
                              : subType == secTypeX509Plain      ? "X509-Plain"
                              : subType == secTypeX509SASL       ? "X509-SASL"
                              : subType == secTypeTokenTLS       ? "TokenTLS"
                              : subType == secTypeX509MsLogonIII ? "X509-MSLogonIII"
                                                                 : "?";
        std::array<char, 128U> cipherA{"TLS"};
        if (session.cipherSuite[0U])
        {
            ::WideCharToMultiByte(CP_UTF8, 0, session.cipherSuite.data(), -1, cipherA.data(), sizeof(cipherA), nullptr, nullptr);
        }
        ::_snprintf_s(m_veNCryptDesc.data(), m_veNCryptDesc.size(), _TRUNCATE, "VeNCrypt %s\n%s", subName, cipherA.data());
    }
    switch (subType)
    {
    case secTypeX509MsLogonIII:
        AuthMsLogonIII();
        break;
    case secTypeX509SASL:
        AuthSasl(endpointHash.empty() ? nullptr : endpointHash.data(), endpointHash.size());
        if (m_saslMech[0U])
        {
            // Insert " (MECH)" at end of the first line of m_veNCryptDesc
            // (before the '\n' that prefixes the cipher suite).
            char* nl = std::strchr(m_veNCryptDesc.data(), '\n');
            std::array<char, 256U> tail;
            std::snprintf(tail.data(), sizeof(tail), " (%s)%s", m_saslMech.data(), nl ? nl : "");
            if (nl)
            {
                *nl = '\0';
            }
            ::strncat_s(m_veNCryptDesc.data(), m_veNCryptDesc.size(), tail.data(), _TRUNCATE);
        }
        break;
    case secTypeX509None:
        // do nothing
        break;
    case rfbVncAuth:
    case secTypeX509Vnc:
        AuthVnc();
        break;
    case secTypePlain:
    case secTypeX509Plain: {
        std::array<char, 256U> _tlsUser{ 0 };
        std::array<char, 256U> _tlsPasswd{ 0 };
        if (std::strlen(m_clearPasswd.data()) == 0)
        {
            AuthDialog ad;
            ad.SetStatusWindow(m_hwndStatus, m_opts->m_ClassName.data());
            if (!ad.DoDialog(dtUserPass, m_host.data(), m_port))
            {
                QuietException_helper(L"Authentication cancelled");
            }
            ::strcpy_s(_tlsUser.data(), 256, ad.m_user.data());
            ::strcpy_s(_tlsPasswd.data(), 256, ad.m_passwd.data());
        }
        else
        {
            ::strcpy_s(_tlsUser.data(), 256, m_cmdlnUser.data());
            ::strcpy_s(_tlsPasswd.data(), 256, m_clearPasswd.data());
        }
        temp = static_cast<int>(std::strlen(_tlsUser.data()));
        temp = static_cast<int>(Swap32IfLE(temp));
        WriteExact(reinterpret_cast<char*>(&temp), 4);
        temp = static_cast<int>(std::strlen(_tlsPasswd.data()));
        temp = static_cast<int>(Swap32IfLE(temp));
        WriteExact(reinterpret_cast<char*>(&temp), 4);
        WriteExact(_tlsUser.data(), static_cast<int>(std::strlen(_tlsUser.data())));
        WriteExact(_tlsPasswd.data(), static_cast<int>(std::strlen(_tlsPasswd.data())));
    }
    break;
    case secTypeTokenTLS: {
        std::array<char, 256U> _tlsUser{ 0 };
        std::array<char, 65U> _tlsToken{ 0 };
        bool usedSessionToken{ false };

        if ((m_opts->m_sessionToken[0U] != '\0') && (m_opts->m_cmdlnUser[0U] != '\0'))
        {
            ::strcpy_s(_tlsUser.data(), 256, m_opts->m_cmdlnUser.data());
            ::strcpy_s(_tlsToken.data(), 65, m_opts->m_sessionToken.data());
            usedSessionToken = true;
        }
        else if (m_opts->m_token[0U] == '\0')
        {
            AuthDialog ad;
            ad.SetStatusWindow(m_hwndStatus, m_opts->m_ClassName.data());
            if (!ad.DoDialog(dtUserToken, m_host.data(), m_port))
            {
                QuietException_helper(L"Authentication cancelled");
            }
            ::strcpy_s(_tlsUser.data(), 256, ad.m_user.data());
            ::strcpy_s(_tlsToken.data(), 65, ad.m_passwd.data());
            ::strcpy_s(m_opts->m_cmdlnUser.data(), m_opts->m_cmdlnUser.size(), ad.m_user.data());
        }
        else
        {
            ::strcpy_s(_tlsUser.data(), 256, m_opts->m_cmdlnUser.data());
            ::strcpy_s(_tlsToken.data(), 65, m_opts->m_token.data());
        }
        std::size_t tokenLen = std::strlen(_tlsToken.data());
        if ((tokenLen < 6) || (tokenLen > 64))
        {
            return SetLastError("Token must be 6-64 ASCII characters");
        }
        std::uint8_t uvncHeader[8U] = { 'U',
                                        'V',
                                        'N',
                                        'C',
                                        uvncTokenProtoMajor,
                                        uvncTokenProtoMinor,
                                        static_cast<std::uint8_t>((usedSessionToken ? uvncTokenAuthSession : uvncTokenAuthPrimary)),
                                        static_cast<std::uint8_t>(uvncTokenFlagRequestSession) };
        WriteExact(reinterpret_cast<char*>(uvncHeader), sizeof(uvncHeader));
        temp = static_cast<int>(std::strlen(_tlsUser.data()));
        temp = static_cast<int>(Swap32IfLE(temp));
        WriteExact(reinterpret_cast<char*>(&temp), 4);
        temp = static_cast<int>(tokenLen);
        temp = static_cast<int>(Swap32IfLE(temp));
        WriteExact(reinterpret_cast<char*>(&temp), 4);
        WriteExact(_tlsUser.data(), static_cast<int>(std::strlen(_tlsUser.data())));
        WriteExact(_tlsToken.data(), static_cast<int>(tokenLen));

        // Remember whether the session token was used so a later failure can
        // clear it and fall back to prompting for a fresh TOTP.
        if (usedSessionToken)
        {
            m_lastVeNCryptSubType |= 0x80000000;
        }
    }
    break;
    default:
        return SetLastError("Cannot complete sub-type authentication");
    }
}

// ISaslTransport over ClientConnection::ReadExact/WriteExact. By this point
// the TLSPlugin is installed, so all bytes are transparently encrypted.
namespace
{
    class ConnTransport : public ISaslTransport
    {
    public:
        explicit ConnTransport(ClientConnection* conn) : m_conn(conn) {}
        bool SendAll(const void* const data, const std::size_t len) override
        {
            try
            {
                m_conn->WriteExact(const_cast<char*>(reinterpret_cast<const char *>(data)), static_cast<int>(len));
            }
            catch (...)
            {
                return false;
            }
            return true;
        }
        bool RecvAll(void* const data, const std::size_t len) override
        {
            try
            {
                m_conn->ReadExact(reinterpret_cast<char*>(data), static_cast<int>(len));
            }
            catch (...)
            {
                return false;
            }
            return true;
        }

    private:
        ClientConnection* m_conn;
    };
}

// QEMU-compatible SASL client exchange for VeNCrypt subtypes 263/264.
// The SASL SecurityResult u32 is read by the caller (AuthenticateServer).
// cbData/cbDataLen: tls-server-end-point hash of the server cert, or empty.
void ClientConnection::AuthSasl(std::uint8_t* const cbData, const std::size_t cbDataLen)
{
    auto SetLastError = [](char* const error) {
        vnclog.Print(0, _T("AuthSasl: %s\n"), error);
        std::array<wchar_t, 256U> werror;
        ::MultiByteToWideChar(CP_UTF8, 0, error, -1, werror.data(), 256);
        throw WarningException(werror.data());
    };

    ConnTransport transport(this);
    std::vector<std::uint8_t> cb(cbData, cbData + cbDataLen);

    // S->C mechanism list
    std::string mechlist;
    if (!SaslWire::ReadString(&transport, SaslWire::kMaxSaslData, mechlist))
    {
        return SetLastError("Failed to read SASL mechanism list");
    }
    vnclog.Print(0, _T("AuthSasl: server mechs '%hs'\n"), mechlist.c_str());

    // Pick a mechanism by token so "SCRAM-SHA-256" cannot match inside
    // "SCRAM-SHA-256-PLUS". Prefer -PLUS when we have binding data, then
    // plain SCRAM-SHA-256.
    bool hasPlus{ false }, hasScram = false;
    {
        std::size_t pos{ 0U };
        while (pos <= mechlist.size())
        {
            std::size_t end = mechlist.find(',', pos);
            if (end == std::string::npos)
            {
                end = mechlist.size();
            }
            std::string m = mechlist.substr(pos, end - pos);
            if (m == "SCRAM-SHA-256-PLUS")
            {
                hasPlus = true;
            }
            else if (m == "SCRAM-SHA-256")
            {
                hasScram = true;
            }
            pos = end + 1;
        }
    }
    std::string mech;
    if (hasPlus && !cb.empty())
    {
        mech = "SCRAM-SHA-256-PLUS";
    }
    else if (hasScram)
    {
        mech = "SCRAM-SHA-256";
    }
    else
    {
        return SetLastError("No usable SASL mechanism");
    }
    ::strncpy_s(m_saslMech.data(), m_saslMech.size(), mech.c_str(), _TRUNCATE);
    vnclog.Print(0, _T("AuthSasl: using mech '%hs'%s\n"), mech.c_str(), mech == "SCRAM-SHA-256-PLUS" ? " (channel-bound)" : "");
    if (!SaslWire::WriteString(&transport, mech))
    {
        return SetLastError("Failed to send SASL mechanism");
    }

    // Credentials
    std::array<char, 256U> user{ 0 };
    std::array<char, 256U> passwd{ 0 };
    if (std::strlen(m_clearPasswd.data()) == 0)
    {
        AuthDialog ad;
        ad.SetStatusWindow(m_hwndStatus, m_opts->m_ClassName.data());
        if (!ad.DoDialog(dtUserPass, m_host.data(), m_port))
        {
            QuietException_helper(L"Authentication cancelled");
        }
        ::strcpy_s(user.data(), user.size(), ad.m_user.data());
        ::strcpy_s(passwd.data(), passwd.size(), ad.m_passwd.data());
    }
    else
    {
        ::strcpy_s(user.data(), user.size(), m_cmdlnUser.data());
        ::strcpy_s(passwd.data(), passwd.size(), m_clearPasswd.data());
    }

    // SCRAM-SHA-256(-PLUS): cbData is only bound into the exchange for the
    // -PLUS mechanism; for plain SCRAM we still send the "y,," GS2 header to
    // signal that we would have used binding had it been offered.
    static const std::vector<std::uint8_t> kEmptyCb;
    const std::vector<std::uint8_t>& cbForMech = (mech == "SCRAM-SHA-256-PLUS") ? cb : kEmptyCb;
    bool supportsCb = !cb.empty();
    std::string firstMsg, firstBare, nonce, gs2Header;
    if (!ScramSha256::ClientFirst(user.data(), cbForMech, supportsCb, firstMsg, firstBare, nonce, gs2Header))
    {
        return SetLastError("SCRAM client-first failed");
    }
    if (!SaslWire::WriteData(&transport, firstMsg.data(), firstMsg.size()))
    {
        return SetLastError("Failed to send SCRAM client-first");
    }

    std::vector<std::uint8_t> serverFirst;
    bool isNull{ false }, complete = false;
    if (!SaslWire::ReadStep(&transport, serverFirst, isNull, complete) || isNull || complete)
    {
        return SetLastError("Malformed SCRAM server-first");
    }

    std::string finalMsg;
    std::vector<std::uint8_t> expectedSig;
    if (!ScramSha256::ClientFinal(serverFirst, passwd.data(), firstBare, nonce, gs2Header, cbForMech, finalMsg, expectedSig))
    {
        return SetLastError("SCRAM client-final failed");
    }
    if (!SaslWire::WriteData(&transport, finalMsg.data(), finalMsg.size()))
    {
        return SetLastError("Failed to send SCRAM client-final");
    }

    std::vector<std::uint8_t> serverFinal;
    if (!SaslWire::ReadStep(&transport, serverFinal, isNull, complete) || !complete)
    {
        return SetLastError("Malformed SCRAM server-final");
    }
    if (!ScramSha256::ClientVerifyServerFinal(serverFinal, expectedSig.data()))
    {
        return SetLastError("SCRAM server signature invalid");
    }
}
