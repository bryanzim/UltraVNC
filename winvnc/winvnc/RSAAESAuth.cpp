// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//

// Server-side RSA-AES (RA2 / RA2ne) authentication.
// Wire protocol mirrors TigerVNC common/rfb/SSecurityRSAAES.cxx and
// noVNC core/ra2.js; the AES-EAX framing matches the viewer's
// ClientConnectionRSAAES.cpp.

#include "stdhdrs.h"

#include "RSAAESAuth.h"

#include "SchannelTLS.h"
#include "SettingsManager.h"
#include "vnclog.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
// Older SDKs may not define the CNG PKCS1 padding flag.

#ifndef NCRYPT_PAD_PKCS1
#define NCRYPT_PAD_PKCS1 0x00000002
#endif

extern VNCLog vnclog;

static const int secTypeRA2None{ 0 };
static const int secTypeRA2UserPass{ 1 };
static const int secTypeRA2Pass{ 2 };

static const DWORD MinRsaKeyLength{ 1024U };
static const DWORD MaxRsaKeyLength{ 8192U };
static const DWORD MaxRsaKeyBytes = MaxRsaKeyLength / 8;

static void ArraySwap(BYTE* const p, const DWORD size)
{
    for (DWORD i{ 0U }; i < size / 2; ++i)
    {
        const BYTE t = p[i];
        p[i] = p[size - 1 - i];
        p[size - 1 - i] = t;
    }
}

static bool ArrayEqual(const BYTE* const a, const BYTE* const b, const DWORD size)
{
    BYTE r{ 0U };
    for (DWORD i{ 0U }; i < size; ++i)
    {
        r |= a[i] ^ b[i];
    }
    return r == 0;
}

// AES cipher as wrapper for Crypto API forward permutation (same as viewer).

struct AESCipher
{
    static const DWORD BlockSize{ 16U };

    struct SymKeyBlob
    {
        BLOBHEADER hdr;
        DWORD cbKeySize;
        BYTE key[32U];
    };

    HCRYPTPROV hProv;
    HCRYPTKEY hKey;
    char err[256U];

    explicit AESCipher() : hProv{ 0 }, hKey{ 0 } { err[0U] = 0; }

    ~AESCipher()
    {
        if (hKey)
        {
            ::CryptDestroyKey(hKey);
        }
        if (hProv)
        {
            ::CryptReleaseContext(hProv, 0);
        }
    }

    bool Init(const DWORD keySize, void* const key)
    {
        if (!::CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        {
            return SetErr("CryptAcquireContext failed (%u)", ::GetLastError());
        }
        SymKeyBlob keyBlob{ 0 };
        keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
        keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
        keyBlob.hdr.aiKeyAlg = (keySize == 128 ? CALG_AES_128 : CALG_AES_256);
        keyBlob.cbKeySize = min(keySize / 8, static_cast<DWORD>(sizeof(keyBlob.key)));
        std::memcpy(keyBlob.key, key, keyBlob.cbKeySize);
        if (!::CryptImportKey(hProv, reinterpret_cast<BYTE*>(&keyBlob), offsetof(SymKeyBlob, key) + keyBlob.cbKeySize, 0, 0, &hKey))
        {
            return SetErr("CryptImportKey failed (%u)", ::GetLastError());
        }
        DWORD mode = CRYPT_MODE_ECB;
        if (!::CryptSetKeyParam(hKey, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
        {
            return SetErr("CryptSetKeyParam failed (%u)", ::GetLastError());
        }
        return true;
    }

    bool Encrypt(void* const buf, DWORD size = BlockSize)
    {
        if (!::CryptEncrypt(hKey, 0, FALSE, 0, reinterpret_cast<BYTE*>(buf), &size, size))
        {
            return SetErr("CryptEncrypt failed (%u)", ::GetLastError());
        }
        return true;
    }

private:
    bool SetErr(const char* const fmt, const DWORD e)
    {
        std::snprintf(err, sizeof(err), fmt, e);
        return false;
    }
};

// CMAC authenticator (same algorithm as the viewer's CMACAuth).

struct CMACAuth
{
    static const DWORD BlockSize = AESCipher::BlockSize;

    AESCipher* cipher;
    BYTE k1[BlockSize], k2[BlockSize];
    BYTE pad[BlockSize];
    DWORD pos;

    bool Init(AESCipher& c)
    {
        cipher = &c;
        Reset();
        if (!cipher->Encrypt(pad))
        {
            return false;
        }
        DoubleBlock(pad, k1);
        DoubleBlock(k1, k2);
        Reset();
        return true;
    }

    void Reset()
    {
        std::memset(pad, 0, sizeof(pad));
        pos = 0;
    }

    bool Process(const void* const buf, const DWORD size)
    {
        for (DWORD i{ 0U }; i < size; ++i)
        {
            if (pos == BlockSize)
            {
                if (!cipher->Encrypt(pad))
                {
                    return false;
                }
                pos = 0;
            }
            pad[pos++] ^= (reinterpret_cast<const BYTE*>(buf))[i];
        }
        return true;
    }

    bool Finalize(void* const tag, const DWORD size = BlockSize)
    {
        if (pos < BlockSize)
        {
            pad[pos] ^= 0x80;
        }
        const BYTE* key = (pos < BlockSize ? k2 : k1);
        for (DWORD i{ 0U }; i < BlockSize; ++i)
        {
            pad[i] ^= key[i];
        }
        if (!cipher->Encrypt(pad))
        {
            return false;
        }
        pos = 0;
        std::memcpy(tag, pad, size);
        return true;
    }

private:
    static void DoubleBlock(BYTE* const in, BYTE* const out)
    {
        DWORD c{ 0U };
        for (int i = BlockSize - 1; i >= 0; --i)
        {
            const DWORD t = in[i];
            out[i] = static_cast<BYTE>(((t << 1) | c));
            c = ((t & 0x80) != 0);
        }
        out[BlockSize - 1] ^= c * 0x87;
    }
};

// EAX mode AEAD (same as the viewer's EAXMode).

struct EAXMode
{
    static const DWORD BlockSize = AESCipher::BlockSize;
    static const DWORD MacSize = CMACAuth::BlockSize;

    AESCipher cipher;
    CMACAuth auth;
    BYTE ctr[BlockSize], pad[BlockSize];
    int pos;
    BYTE tagNonce[MacSize];
    BYTE tagAad[MacSize];

    bool Init(const DWORD keySize, void* const key, char* const errBuf, const std::size_t errSize)
    {
        errBuf[0U] = 0;
        if (!cipher.Init(keySize, key))
        {
            std::snprintf(errBuf, errSize, "%s", cipher.err);
            return false;
        }
        if (!auth.Init(cipher))
        {
            std::snprintf(errBuf, errSize, "%s", cipher.err);
            return false;
        }
        pos = -1;
        return true;
    }

    bool SetNonce(DWORD msg)
    {
        BYTE buf[BlockSize] = { 0 };
        std::memcpy(buf, &msg, 4);
        return SetNonce(buf, BlockSize);
    }

    bool SetNonce(const void* const buf, const DWORD size)
    {
        if (!Mac(0, buf, size, tagNonce))
        {
            return false;
        }
        std::memcpy(ctr, tagNonce, sizeof(ctr));
        return true;
    }

    bool SetAad(const void* const buf, const DWORD size) { return Mac(1, buf, size, tagAad); }

    bool Process(void* const buf, const DWORD size, const bool encrypt = true)
    {
        if (pos < 0)
        {
            if (!Mac(2, nullptr, 0, nullptr))
            {
                return false;
            }
            pos = 0;
        }
        if (!encrypt && !auth.Process(buf, size))
        {
            return false;
        }
        for (DWORD i{ 0U }; i < size; ++i)
        {
            if ((pos == 0) || (pos == BlockSize))
            {
                std::memcpy(pad, ctr, BlockSize);
                if (!cipher.Encrypt(pad))
                {
                    return false;
                }
                WrapIncr(ctr);
                pos = 0;
            }
            (reinterpret_cast<BYTE*>(buf))[i] ^= pad[pos++];
        }
        if (encrypt && !auth.Process(buf, size))
        {
            return false;
        }
        return true;
    }

    bool Finalize(void* const tag, const DWORD size = MacSize)
    {
        if (!auth.Finalize(tag, size))
        {
            return false;
        }
        for (DWORD i{ 0U }; i < size; ++i)
        {
            (reinterpret_cast<BYTE*>(tag))[i] ^= tagNonce[i] ^ tagAad[i];
        }
        pos = -1;
        return true;
    }

private:
    bool Mac(const BYTE step, const void* const buf, const DWORD size, void* const tag)
    {
        BYTE block[BlockSize] = { 0 };
        block[BlockSize - 1] = step;
        auth.Reset();
        if (!auth.Process(block, sizeof(block)))
        {
            return false;
        }
        if (size && !auth.Process(buf, size))
        {
            return false;
        }
        if (tag)
        {
            return auth.Finalize(tag, MacSize);
        }
        return true;
    }

    static void WrapIncr(BYTE ctr[BlockSize])
    {
        for (int i = BlockSize - 1; i >= 0; --i)
        {
            if (++ctr[i])
            {
                break;
            }
        }
    }
};

// EAX message framing on the raw socket: u16be length (AAD) + ciphertext +
// 16-byte tag, nonce = per-direction little-endian message counter.

struct EAXStream
{
    static const DWORD AadSize{ 2U };
    static const DWORD MacSize = EAXMode::MacSize;

    EAXMode encAead, decAead;
    DWORD encMsg, decMsg;
    std::vector<BYTE> plain;
    char err[256U];

    explicit EAXStream() : encMsg{ 0 }, decMsg{ 0 } { err[0U] = 0; }

    bool Init(const DWORD keySize, void* const encKey, void* const decKey)
    {
        if (!encAead.Init(keySize, encKey, err, sizeof(err)))
        {
            return false;
        }
        if (!decAead.Init(keySize, decKey, err, sizeof(err)))
        {
            return false;
        }
        return true;
    }

    // Returns a complete EAX frame for `size` plaintext bytes.
    bool MakeMessage(const void* const data, const DWORD size, std::vector<BYTE>& out)
    {
        out.resize(AadSize + size + MacSize);
        out[0U] = static_cast<BYTE>((size >> 8));
        out[1U] = static_cast<BYTE>((size & 0xff));
        std::memcpy(out.data() + AadSize, data, size);
        if (!encAead.SetNonce(encMsg++))
        {
            return SetErr("SetNonce failed");
        }
        if (!encAead.SetAad(out.data(), AadSize))
        {
            return SetErr("SetAad failed");
        }
        if (!encAead.Process(out.data() + AadSize, size))
        {
            return SetErr("Encrypt failed");
        }
        if (!encAead.Finalize(out.data() + AadSize + size, MacSize))
        {
            return SetErr("Tag failed on encryption");
        }
        return true;
    }

    // Decrypts one frame (len bytes ciphertext + tag, AAD = len).
    bool OpenMessage(BYTE* const frame, const DWORD size, std::vector<BYTE>& outPlain)
    {
        outPlain.assign(frame, frame + size);
        if (!decAead.SetNonce(decMsg++))
        {
            return SetErr("SetNonce failed");
        }
        if (!decAead.SetAad(frame - AadSize, AadSize))
        {
            return SetErr("SetAad failed");
        }
        if (!decAead.Process(outPlain.data(), size, false))
        {
            return SetErr("Decrypt failed");
        }
        BYTE tag[MacSize];
        if (!decAead.Finalize(tag, MacSize))
        {
            return SetErr("Tag failed on decryption");
        }
        if (!ArrayEqual(tag, frame + size, MacSize))
        {
            return SetErr("Decryption tag does not match");
        }
        return true;
    }

private:
    bool SetErr(const char* const msg)
    {
        std::snprintf(err, sizeof(err), "%s", msg);
        return false;
    }
};

// Convenience: send one EAX-framed message over the callback-installed
// socket. Used before TLSCallbacks take over, so messages are built and
// sent manually on the raw socket.
bool RSAAESAuth::RawSendAll(const BYTE* const data, const DWORD len)
{
    DWORD sent{ 0U };
    while (sent < len)
    {
        int n = m_socket->Send(reinterpret_cast<const char*>(data) + sent, static_cast<VCard>((len - sent)));
        if (n <= 0)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("RA2: raw send failed n=%d wsa=%d\n"), n, ::WSAGetLastError());
            return false;
        }
        sent += static_cast<DWORD>(n);
    }
    return true;
}

bool RSAAESAuth::RawReadExact(BYTE* const data, const DWORD len)
{
    DWORD got{ 0U };
    while (got < len)
    {
        int n = m_socket->Read(reinterpret_cast<char*>(data) + got, static_cast<int>((len - got)));
        if (n <= 0)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("RA2: raw recv failed n=%d wsa=%d\n"), n, ::WSAGetLastError());
            return false;
        }
        got += static_cast<DWORD>(n);
    }
    return true;
}

bool RSAAESAuth::Fail(const char* const msg)
{
    m_lastError = msg;
    vnclog.Print(LL_CONNERR, VNCLOG("RA2: %s\n"), msg);
    return false;
}

bool RSAAESAuth::Fail(const char* const msg, const DWORD err)
{
    std::array<char, 256U> buf;
    std::snprintf(buf.data(), sizeof(buf), "%s (%u)", msg, err);
    return Fail(buf.data());
}

RSAAESAuth::RSAAESAuth(VSocket* socket)
    : m_socket(socket), m_keySize{ 0 }, m_allEncrypted{ false }, m_requireUser{ false }, m_tlsLoader{ nullptr },
      m_certCtx{ nullptr }, m_hServerKey{ 0 }, m_hCertKey{ 0 }, m_certKeySpec{ 0 }, m_certKeyMustFree(FALSE), m_hProv{ 0 },
      m_hClientKey{ 0 }, m_hHash{ 0 }, m_serverKeyBits{ 0 }, m_serverKeyBytes{ 0 }, m_clientKeyBits{ 0 }, m_clientKeyBytes{ 0 },
      m_eax{ nullptr }, m_eaxInstalled{ false }
{
}

RSAAESAuth::~RSAAESAuth()
{
    if (m_socket && m_eaxInstalled)
    {
        m_socket->SetTLSCallbacks(nullptr);
    }
    if (m_certKeyMustFree && m_hCertKey)
    {
        if (m_certKeySpec == CERT_NCRYPT_KEY_SPEC)
        {
            ::NCryptFreeObject(m_hCertKey);
        }
        else
        {
            ::CryptReleaseContext(m_hCertKey, 0);
        }
    }
    if (m_hHash)
    {
        ::CryptDestroyHash(m_hHash);
    }
    if (m_hClientKey)
    {
        ::CryptDestroyKey(m_hClientKey);
    }
    if (m_hProv)
    {
        ::CryptReleaseContext(m_hProv, 0);
    }
    delete m_eax;
    delete m_tlsLoader;
}

bool RSAAESAuth::Authenticate(DWORD keySize, bool allEncrypted, bool requireUser, std::string& outUsername,
                              std::string& outPassword, std::string& outFailureMessage)
{
    m_keySize = keySize;
    m_allEncrypted = allEncrypted;
    m_requireUser = requireUser;

    if (!LoadServerKey() || !SendServerPublicKey() || !ReadClientPublicKey() || !SendServerRandom() || !ReadClientRandom() ||
        !SetCipher() || !SendKeyHash() || !ReadClientHash() || !SendSubtype() || !ReadCredentials(outUsername, outPassword))
    {
        outFailureMessage = m_lastError.empty() ? "RA2 handshake failed" : m_lastError;
        return false;
    }

    // RA2ne: the session continues unencrypted after the credential exchange.
    if (!m_allEncrypted && m_eaxInstalled)
    {
        m_socket->SetTLSCallbacks(nullptr);
        m_eaxInstalled = false;
    }
    return true;
}

// Load the VeNCrypt certificate and expose its RSA key pair for RA2.
bool RSAAESAuth::LoadServerKey()
{
    SettingsManager* settingsLocal = SettingsManager::getInstance();
    m_tlsLoader = new SchannelTLS();

    bool certOk{ false };
    const char* thumbprint = settingsLocal->getVeNCryptTLSCertThumbprint();
    if (thumbprint && thumbprint[0U])
    {
        certOk = m_tlsLoader->LoadCertByThumbprint(thumbprint);
    }
    if (!certOk)
    {
        std::array<char, 41U> generatedThumbprint{};
        certOk = m_tlsLoader->EnsureSelfSignedCert("CN=UltraVNC-VeNCrypt", "UltraVNC-VeNCrypt", generatedThumbprint.data(),
                                                   settingsLocal->getTLSCertKeyBits());
        if (certOk)
        {
            settingsLocal->setVeNCryptTLSCertThumbprint(generatedThumbprint.data());
            settingsLocal->save();
        }
    }
    if (!certOk)
    {
        return Fail("failed to load or create certificate");
    }

    PCCERT_CONTEXT ctx = m_tlsLoader->GetCertContext();
    if (!ctx)
    {
        return Fail("no certificate context");
    }

    // RA2 needs an RSA certificate.
    CERT_PUBLIC_KEY_INFO* pki = &ctx->pCertInfo->SubjectPublicKeyInfo;
    if (!pki->Algorithm.pszObjId || (std::strcmp(pki->Algorithm.pszObjId, szOID_RSA_RSA) != 0))
    {
        return Fail("certificate public key is not RSA");
    }

    // Decode the public key (gives PUBLICKEYBLOB: header + RSAPUBKEY + LE modulus).
    DWORD blobLen{ 0U };
    if (!::CryptDecodeObject(X509_ASN_ENCODING, RSA_CSP_PUBLICKEYBLOB, pki->PublicKey.pbData, pki->PublicKey.cbData, 0, nullptr,
                           &blobLen))
    {
        return Fail("public key decode size failed", ::GetLastError());
    }
    std::vector<BYTE> blob(blobLen);
    if (!::CryptDecodeObject(X509_ASN_ENCODING, RSA_CSP_PUBLICKEYBLOB, pki->PublicKey.pbData, pki->PublicKey.cbData, 0, blob.data(),
                           &blobLen))
    {
        return Fail("public key decode failed", ::GetLastError());
    }

    RSAPUBKEY* rsaPub = reinterpret_cast<RSAPUBKEY*>((blob.data() + sizeof(BLOBHEADER)));
    BYTE* modulusLE = blob.data() + sizeof(BLOBHEADER) + sizeof(RSAPUBKEY);
    if ((rsaPub->bitlen < MinRsaKeyLength) || (rsaPub->bitlen > MaxRsaKeyLength))
    {
        return Fail("certificate RSA key size out of range");
    }

    m_serverKeyBits = rsaPub->bitlen;
    m_serverKeyBytes = (m_serverKeyBits + 7) / 8;

    m_serverN.assign(modulusLE, modulusLE + m_serverKeyBytes);
    ArraySwap(m_serverN.data(), m_serverKeyBytes);
    m_serverE.assign(m_serverKeyBytes, 0);
    DWORD e = rsaPub->pubexp;
    for (int i = static_cast<int>(m_serverKeyBytes) - 1; (i >= 0) && e; --i)
    {
        m_serverE[static_cast<unsigned long long>(i)] = static_cast<BYTE>((e & 0xff));
        e >>= 8;
    }

    // Acquire the private key handle for decrypting the client random.
    if (!::CryptAcquireCertificatePrivateKey(ctx, CRYPT_ACQUIRE_SILENT_FLAG | CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG, nullptr,
                                           &m_hCertKey, &m_certKeySpec, &m_certKeyMustFree))
    {
        return Fail("private key not accessible", ::GetLastError());
    }
    m_hServerKey = (m_certKeySpec == CERT_NCRYPT_KEY_SPEC) ? static_cast<NCRYPT_KEY_HANDLE>(m_hCertKey) : 0;
    m_certCtx = ctx;
    return true;
}

bool RSAAESAuth::SendServerPublicKey()
{
    std::vector<BYTE> msg(4 + m_serverKeyBytes * 2);
    DWORD len = Swap32IfLE(m_serverKeyBits);
    std::memcpy(msg.data(), &len, 4);
    std::memcpy(msg.data() + 4, m_serverN.data(), m_serverKeyBytes);
    std::memcpy(msg.data() + 4 + m_serverKeyBytes, m_serverE.data(), m_serverKeyBytes);
    if (!RawSendAll(msg.data(), static_cast<DWORD>(msg.size())))
    {
        return Fail("failed to send server public key");
    }
    return true;
}

bool RSAAESAuth::ReadClientPublicKey()
{
    std::array<BYTE, 4U> hdr;
    if (!RawReadExact(hdr.data(), 4))
    {
        return Fail("failed to read client key length");
    }
    m_clientKeyBits = Swap32IfLE(*reinterpret_cast<DWORD*>(hdr.data()));
    if (m_clientKeyBits < MinRsaKeyLength)
    {
        return Fail("client RSA key too small");
    }
    if (m_clientKeyBits > MaxRsaKeyLength)
    {
        return Fail("client RSA key too big");
    }
    m_clientKeyBytes = (m_clientKeyBits + 7) / 8;

    m_clientN.resize(m_clientKeyBytes);
    m_clientE.resize(m_clientKeyBytes);
    if (!RawReadExact(m_clientN.data(), m_clientKeyBytes) || !RawReadExact(m_clientE.data(), m_clientKeyBytes))
    {
        return Fail("failed to read client public key");
    }

    // Exponent must fit in 4 bytes (checked like the viewer does).
    for (DWORD i{ 0U }; i < m_clientKeyBytes - 4; ++i)
    {
        if (m_clientE[i] != 0)
        {
            return Fail("client RSA exponent too big");
        }
    }

    // Import the client key into CAPI for encrypting the server random.
    struct PubKeyBlob
    {
        BLOBHEADER hdr;
        RSAPUBKEY key;
        BYTE modulus[MaxRsaKeyBytes];
    } keyBlob = { 0 };

    if (!m_hProv && !::CryptAcquireContext(&m_hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        return Fail("CryptAcquireContext failed", ::GetLastError());
    }
    keyBlob.hdr.bType = PUBLICKEYBLOB;
    keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
    keyBlob.hdr.aiKeyAlg = CALG_RSA_KEYX;
    keyBlob.key.magic = BCRYPT_RSAPUBLIC_MAGIC; // RSA1
    keyBlob.key.bitlen = m_clientKeyBits;
    keyBlob.key.pubexp = Swap32IfLE(*reinterpret_cast<DWORD*>(&m_clientE[m_clientKeyBytes - 4]));
    std::memcpy(keyBlob.modulus, m_clientN.data(), m_clientKeyBytes);
    ArraySwap(keyBlob.modulus, m_clientKeyBytes);
    if (!::CryptImportKey(m_hProv, reinterpret_cast<BYTE*>(&keyBlob), offsetof(PubKeyBlob, modulus) + m_clientKeyBytes, 0, 0, &m_hClientKey))
    {
        return Fail("invalid client RSA key", ::GetLastError());
    }
    return true;
}

bool RSAAESAuth::SendServerRandom()
{
    BYTE buffer[MaxRsaKeyBytes];
    DWORD size = m_keySize / 8;

    if (!m_hProv && !::CryptAcquireContext(&m_hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        return Fail("CryptAcquireContext failed", ::GetLastError());
    }
    if (!::CryptGenRandom(m_hProv, size, reinterpret_cast<BYTE*>(m_serverRandom.data())))
    {
        return Fail("CryptGenRandom failed", ::GetLastError());
    }
    std::memcpy(buffer, m_serverRandom.data(), size);
    if (!::CryptEncrypt(m_hClientKey, 0, TRUE, 0, buffer, &size, sizeof(buffer)))
    {
        return Fail("server random encryption failed", ::GetLastError());
    }
    if (size != m_clientKeyBytes)
    {
        return Fail("client key size mismatch");
    }
    DWORD len16 = Swap16IfLE(size);
    ArraySwap(buffer, size); // CAPI emits little-endian ciphertext
    if (!RawSendAll(reinterpret_cast<BYTE*>(&len16), 2) || !RawSendAll(buffer, size))
    {
        return Fail("failed to send server random");
    }
    return true;
}

bool RSAAESAuth::ReadClientRandom()
{
    std::array<BYTE, 2U> hdr;
    if (!RawReadExact(hdr.data(), 2))
    {
        return Fail("failed to read client random length");
    }
    DWORD size = Swap16IfLE(*reinterpret_cast<WORD*>(hdr.data()));
    if (size != m_serverKeyBytes)
    {
        return Fail("server key size does not match random length");
    }

    std::vector<BYTE> ct(size);
    if (!RawReadExact(ct.data(), size))
    {
        return Fail("failed to read client random");
    }

    if (m_hServerKey)
    {
        // CNG: ciphertext is big-endian on the wire, no swap needed.
        DWORD plainLen = sizeof(m_clientRandom);
        SECURITY_STATUS st =
            ::NCryptDecrypt(m_hServerKey, ct.data(), size, nullptr, m_clientRandom.data(), plainLen, &plainLen, NCRYPT_PAD_PKCS1);
        if (st != ERROR_SUCCESS)
        {
            return Fail("client random decryption failed", static_cast<DWORD>(st));
        }
        if (plainLen != m_keySize / 8)
        {
            return Fail("client random size mismatch");
        }
    }
    else
    {
        // Legacy CAPI key: acquire the AT_KEYEXCHANGE key from the provider
        // handle returned by CryptAcquireCertificatePrivateKey; ciphertext
        // must be little-endian.
        ArraySwap(ct.data(), size);
        HCRYPTKEY hPrivKey = 0;
        if (!::CryptGetUserKey(reinterpret_cast<HCRYPTPROV>(m_hCertKey), AT_KEYEXCHANGE, &hPrivKey))
        {
            return Fail("cannot access certificate private key", ::GetLastError());
        }
        DWORD plainLen = size;
        BOOL ok = ::CryptDecrypt(hPrivKey, 0, TRUE, 0, ct.data(), &plainLen);
        ::CryptDestroyKey(hPrivKey);
        if (!ok)
        {
            return Fail("client random decryption failed", ::GetLastError());
        }
        if (plainLen != m_keySize / 8)
        {
            return Fail("client random size mismatch");
        }
        std::memcpy(m_clientRandom.data(), ct.data(), plainLen);
    }
    return true;
}

// Derive the EAX session keys and install the encrypted stream.
bool RSAAESAuth::SetCipher()
{
    ALG_ID algId = (m_keySize == 128 ? CALG_SHA1 : CALG_SHA_256);
    DWORD size = m_keySize / 8;
    std::array<BYTE, 32U> readKey;
    std::array<BYTE, 32U> writeKey;

    // Server reads with H(serverRand||clientRand), writes with
    // H(clientRand||serverRand) — mirrors the viewer's enc/dec derivation.
    auto Hash = [&](const BYTE* a, const BYTE* b, BYTE* out) -> bool {
        if (!m_hHash && !::CryptCreateHash(m_hProv, algId, 0, 0, &m_hHash))
        {
            return false;
        }
        if (!::CryptHashData(m_hHash, const_cast<BYTE*>(reinterpret_cast<const BYTE *>(a)), size, 0) || !::CryptHashData(m_hHash, const_cast<BYTE*>(reinterpret_cast<const BYTE *>(b)), size, 0))
        {
            return false;
        }
        std::array<BYTE, 256U> buf;
        DWORD bufSize = sizeof(buf);
        if (!::CryptGetHashParam(m_hHash, HP_HASHVAL, buf.data(), &bufSize, 0))
        {
            return false;
        }
        ::CryptDestroyHash(m_hHash);
        m_hHash = 0;
        std::memcpy(out, buf.data(), size);
        return true;
    };

    if (!Hash(m_serverRandom.data(), m_clientRandom.data(), readKey.data()) || !Hash(m_clientRandom.data(), m_serverRandom.data(), writeKey.data()))
    {
        return Fail("session key derivation failed");
    }

    m_eax = new EAXStream();
    if (!m_eax->Init(m_keySize, writeKey.data(), readKey.data()))
    {
        return Fail(m_eax->err);
    }
    std::memset(readKey.data(), 0, sizeof(readKey));
    std::memset(writeKey.data(), 0, sizeof(writeKey));

    // Install the EAX layer on the socket for all subsequent traffic.
    auto callbacks = std::make_unique<VSocket::TLSCallbacks>();
    EAXStream* eax = m_eax;
    VSocket* socket = m_socket;

    callbacks->send = [eax, socket](const char* buf, int len) -> bool {
        std::vector<BYTE> frame;
        if (!eax->MakeMessage(buf, static_cast<DWORD>(len), frame))
        {
            return false;
        }
        DWORD sent{ 0U };
        while (sent < static_cast<DWORD>(frame.size()))
        {
            int n = socket->Send(reinterpret_cast<const char*>(frame.data()) + sent, static_cast<VCard>((frame.size() - sent)));
            if (n <= 0)
            {
                return false;
            }
            sent += static_cast<DWORD>(n);
        }
        return true;
    };
    callbacks->recv = [eax, socket](char* buf, int len, int* got) -> bool {
        *got = 0;
        if (eax->plain.empty())
        {
            // Read next EAX frame: u16be length + ciphertext + tag.
            std::array<BYTE, 2U> hdr;
            DWORD got2{ 0U };
            while (got2 < 2)
            {
                int n = socket->Read(reinterpret_cast<char*>(hdr.data()) + got2, static_cast<VCard>(2 - static_cast<int>(got2)));
                if (n <= 0)
                {
                    return false;
                }
                got2 += static_cast<DWORD>(n);
            }
            DWORD size = (static_cast<DWORD>(hdr[0U]) << 8) | hdr[1U];
            std::vector<BYTE> frame(EAXStream::AadSize + size + EAXStream::MacSize);
            frame[0U] = hdr[0U];
            frame[1U] = hdr[1U];
            DWORD off = EAXStream::AadSize;
            while (off < frame.size())
            {
                int n = socket->Read(reinterpret_cast<char*>(frame.data()) + off, static_cast<int>((frame.size() - off)));
                if (n <= 0)
                {
                    return false;
                }
                off += static_cast<DWORD>(n);
            }
            if (!eax->OpenMessage(frame.data() + EAXStream::AadSize, size, eax->plain))
            {
                return false;
            }
        }
        DWORD take = min(static_cast<DWORD>(len), static_cast<DWORD>(eax->plain.size()));
        std::memcpy(buf, eax->plain.data(), take);
        eax->plain.erase(eax->plain.begin(), eax->plain.begin() + take);
        *got = static_cast<int>(take);
        return true;
    };

    m_callbacks = std::move(callbacks);
    m_socket->SetTLSCallbacks(m_callbacks.get());
    m_eaxInstalled = true;
    return true;
}

// Encrypted helpers: one EAX message out / one EAX message in.

static bool EaxSend([[maybe_unused]] RSAAESAuth* const self, VSocket* const socket, const void* const data, const DWORD len)
{
    // The callbacks are installed; SendExact routes through them.
    return socket->SendExact(reinterpret_cast<const char*>(data), len) != FALSE;
}

bool RSAAESAuth::SendKeyHash()
{
    // Server hash = H(lenS|sN|sE|lenC|cN|cE)
    ALG_ID algId = (m_keySize == 128 ? CALG_SHA1 : CALG_SHA_256);
    DWORD hashSize = (m_keySize == 128 ? 20 : 32);
    std::array<BYTE, 32U> hash;

    if (!m_hHash && !::CryptCreateHash(m_hProv, algId, 0, 0, &m_hHash))
    {
        return Fail("CryptCreateHash failed", ::GetLastError());
    }
    DWORD sl = Swap32IfLE(m_serverKeyBits);
    DWORD cl = Swap32IfLE(m_clientKeyBits);
    if (!::CryptHashData(m_hHash, reinterpret_cast<BYTE*>(&sl), 4, 0) || !::CryptHashData(m_hHash, m_serverN.data(), m_serverKeyBytes, 0) ||
        !::CryptHashData(m_hHash, m_serverE.data(), m_serverKeyBytes, 0) || !::CryptHashData(m_hHash, reinterpret_cast<BYTE*>(&cl), 4, 0) ||
        !::CryptHashData(m_hHash, m_clientN.data(), m_clientKeyBytes, 0) ||
        !::CryptHashData(m_hHash, m_clientE.data(), m_clientKeyBytes, 0))
    {
        return Fail("key hash failed", ::GetLastError());
    }
    std::array<BYTE, 256U> buf;
    DWORD bufSize = sizeof(buf);
    if (!::CryptGetHashParam(m_hHash, HP_HASHVAL, buf.data(), &bufSize, 0))
    {
        return Fail("key hash finalize failed", ::GetLastError());
    }
    ::CryptDestroyHash(m_hHash);
    m_hHash = 0;
    std::memcpy(hash.data(), buf.data(), hashSize);

    if (!EaxSend(this, m_socket, hash.data(), hashSize))
    {
        return Fail("failed to send key hash");
    }
    return true;
}

bool RSAAESAuth::ReadClientHash()
{
    DWORD hashSize = (m_keySize == 128 ? 20 : 32);
    std::vector<BYTE> recvHash(hashSize);
    if (!m_socket->ReadExact(reinterpret_cast<char*>(recvHash.data()), static_cast<VCard>(hashSize)))
    {
        return Fail("failed to read client hash");
    }

    // Expected client hash = H(lenC|cN|cE|lenS|sN|sE)
    ALG_ID algId = (m_keySize == 128 ? CALG_SHA1 : CALG_SHA_256);
    if (!m_hHash && !::CryptCreateHash(m_hProv, algId, 0, 0, &m_hHash))
    {
        return Fail("CryptCreateHash failed", ::GetLastError());
    }
    DWORD cl = Swap32IfLE(m_clientKeyBits);
    DWORD sl = Swap32IfLE(m_serverKeyBits);
    if (!::CryptHashData(m_hHash, reinterpret_cast<BYTE*>(&cl), 4, 0) || !::CryptHashData(m_hHash, m_clientN.data(), m_clientKeyBytes, 0) ||
        !::CryptHashData(m_hHash, m_clientE.data(), m_clientKeyBytes, 0) || !::CryptHashData(m_hHash, reinterpret_cast<BYTE*>(&sl), 4, 0) ||
        !::CryptHashData(m_hHash, m_serverN.data(), m_serverKeyBytes, 0) ||
        !::CryptHashData(m_hHash, m_serverE.data(), m_serverKeyBytes, 0))
    {
        return Fail("client hash compute failed", ::GetLastError());
    }
    std::array<BYTE, 256U> buf;
    DWORD bufSize = sizeof(buf);
    if (!::CryptGetHashParam(m_hHash, HP_HASHVAL, buf.data(), &bufSize, 0))
    {
        return Fail("client hash finalize failed", ::GetLastError());
    }
    ::CryptDestroyHash(m_hHash);
    m_hHash = 0;

    if (!ArrayEqual(recvHash.data(), buf.data(), hashSize))
    {
        return Fail("client hash does not match");
    }
    return true;
}

bool RSAAESAuth::SendSubtype()
{
    BYTE subtype = static_cast<BYTE>((m_requireUser ? secTypeRA2UserPass : secTypeRA2Pass));
    if (!EaxSend(this, m_socket, &subtype, 1))
    {
        return Fail("failed to send subtype");
    }
    return true;
}

bool RSAAESAuth::ReadCredentials(std::string& user, std::string& pass)
{
    BYTE ulen{ 0U }, plen = 0;
    if (!m_socket->ReadExact(reinterpret_cast<char*>(&ulen), 1))
    {
        return Fail("failed to read username length");
    }
    std::vector<char> ubuf(ulen + 1);
    if (ulen && !m_socket->ReadExact(ubuf.data(), ulen))
    {
        return Fail("failed to read username");
    }
    if (!m_socket->ReadExact(reinterpret_cast<char*>(&plen), 1))
    {
        return Fail("failed to read password length");
    }
    std::vector<char> pbuf(plen + 1);
    if (plen && !m_socket->ReadExact(pbuf.data(), plen))
    {
        return Fail("failed to read password");
    }

    user = ubuf.data();
    pass = pbuf.data();
    std::memset(ubuf.data(), 0, ubuf.size());
    std::memset(pbuf.data(), 0, pbuf.size());
    return true;
}
