// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// RSA-AES authentication and encryption

#include "stdhdrs.h"

#include "../common/BcryptHelp.h"
#include "AuthDialog.h"
#include "ClientConnection.h"
#include "Exception.h"

#include <bcrypt.h>
#include <CommCtrl.h>

#include <array>
#include <cstring>
#include <vector>

#ifndef BCRYPT_KDF_RAW_SECRET
#define BCRYPT_KDF_RAW_SECRET L"TRUNCATE"
#endif
extern "C" {
#include "../common/mnemonic.h"
#include <cwchar>
}

const int secTypeRA2None{ 0 };
const int secTypeRA2UserPass{ 1 };
const int secTypeRA2Pass{ 2 };

static wchar_t lastError[1024U];
char hex[24U];
std::array<char, 1024U> catchphrase;

static void ArraySwap(BYTE* const p, const DWORD size)
{
    for (DWORD i{ 0U }; i < size / 2; ++i)
    {
        const BYTE t = p[i];
        p[i] = p[size - 1 - i];
        p[size - 1 - i] = t;
    }
}

static bool ArrayEqual(BYTE* const a, BYTE* const b, const DWORD size)
{
    BYTE r{ 0U };
    for (DWORD i{ 0U }; i < size; ++i)
    {
        r |= a[i] ^ b[i];
    }
    return r == 0;
}

// AES cipher implementation as wrapper for Crypto API forward permutaion

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

    explicit AESCipher() : hProv{ 0 }, hKey{ 0 } {}

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
            ::swprintf_s(lastError, L"CryptAcquireContext failed (%d)", ::GetLastError());
            return SetLastError(lastError);
        }
        SymKeyBlob keyBlob{ 0 };
        keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
        keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
        keyBlob.hdr.aiKeyAlg = (keySize == 128 ? CALG_AES_128 : CALG_AES_256);
        keyBlob.cbKeySize = minimum(keySize / 8, sizeof(keyBlob.key));
        std::memcpy(keyBlob.key, key, keyBlob.cbKeySize);
        if (!::CryptImportKey(hProv, reinterpret_cast<BYTE*>(&keyBlob), offsetof(SymKeyBlob, key) + keyBlob.cbKeySize, 0, 0, &hKey))
        {
            ::swprintf_s(lastError, L"CryptImportKey failed (%d)", ::GetLastError());
            return SetLastError(lastError);
        }
        DWORD mode = CRYPT_MODE_ECB;
        if (!::CryptSetKeyParam(hKey, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
        {
            ::swprintf_s(lastError, L"CryptSetKeyParam(KP_MODE) failed (%d)", ::GetLastError());
            return SetLastError(lastError);
        }
        return true;
    }

    bool Encrypt(void* const buf, DWORD size = BlockSize)
    {
        if (!::CryptEncrypt(hKey, 0, FALSE, 0, reinterpret_cast<BYTE*>(buf), &size, size))
        {
            ::swprintf_s(lastError, L"CryptEncrypt failed (%d)", ::GetLastError());
            return SetLastError(lastError);
        }
        return true;
    }

private:
    static bool SetLastError(const wchar_t* const error)
    {
        vnclog.Print(0, _T("AESCipher: %S\n"), error);
        throw WarningException(error);
        return false;
    }
};

// CMAC authenticator implementation

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
        cipher->Encrypt(pad);
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

    bool Process(void* const buf, const DWORD size)
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
            pad[pos++] ^= (reinterpret_cast<BYTE*>(buf))[i];
        }
        return true;
    }

    bool Finalize(void* const tag, const DWORD size = BlockSize)
    {
        if ((size < 4) || (size > BlockSize))
        {
            ::swprintf_s(lastError, L"Invalid tag size for CMAC (%d)", size);
            return SetLastError(lastError);
        }
        if (pos < BlockSize)
        {
            pad[pos] ^= 0x80;
        }
        BYTE* key = (pos < BlockSize ? k2 : k1);
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

    static bool SetLastError(const wchar_t* const error)
    {
        vnclog.Print(0, _T("CMACAuth: %S\n"), error);
        throw WarningException(error);
        return false;
    }
};

// EAX cipher mode AEAD (authenticated encryption with associated data) implementation

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

    bool Init(const DWORD keySize, void* const key)
    {
        if (!cipher.Init(keySize, key))
        {
            return false;
        }
        if (!auth.Init(cipher))
        {
            return false;
        }
        pos = -1;
        return true;
    }

    void SetNonce(DWORD msg)
    {
        BYTE buf[BlockSize] = { 0 };
        std::memcpy(buf, &msg, 4);
        SetNonce(buf, BlockSize);
    }

    void SetNonce(void* const buf, const DWORD size)
    {
        Mac(0, buf, size, tagNonce);
        std::memcpy(ctr, tagNonce, sizeof(ctr));
    }

    void SetAad(void* const buf, const DWORD size) { Mac(1, buf, size, tagAad); }

    bool Process(void* const buf, const DWORD size, const bool encrypt = true)
    {
        if (pos < 0)
        {
            Mac(2, nullptr, 0, nullptr);
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
    void Mac(const BYTE step, void* const buf, const DWORD size, void* const tag)
    {
        BYTE block[BlockSize] = { 0 };
        block[BlockSize - 1] = step;
        auth.Reset();
        auth.Process(block, sizeof(block));
        auth.Process(buf, size);
        if (tag)
        {
            auth.Finalize(tag, MacSize);
        }
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

// Encryption/decryption plugin based on AES in EAX mode

struct AESEAXPlugin : IPlugin
{
    static const int BlockSize = EAXMode::BlockSize;
    static const int MacSize = EAXMode::MacSize;
    static const int AadSize{ 2 };

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

        inline int GetAvailable() { return maximum(static_cast<int>(size - pos), 0); }
    };

    EAXMode encAead, decAead;
    DWORD encMsg, decMsg;
    DynBuffer encBuffer, decBuffer, decPlain;
    int wanted;

    AESEAXPlugin(const DWORD keySize, void* const encKey, void* const decKey)
    {
        encAead.Init(keySize, encKey);
        decAead.Init(keySize, decKey);
        encMsg = decMsg = 0;
    }

    virtual ~AESEAXPlugin() {}

    virtual BYTE* TransformBuffer(BYTE* pDataBuffer, int nDataLen, int* pnTransformedDataLen)
    {
        *pnTransformedDataLen = AadSize + nDataLen + MacSize;
        encBuffer.size = 0;
        BYTE* dst = encBuffer.EnsureFree(static_cast<DWORD>(*pnTransformedDataLen));
        *(reinterpret_cast<CARD16*>(dst)) = Swap16IfLE(nDataLen);
        std::memcpy(dst + AadSize, pDataBuffer, static_cast<size_t>(nDataLen));
        encAead.SetNonce(encMsg++);
        encAead.SetAad(dst, AadSize);
        if (!encAead.Process(dst + AadSize, static_cast<DWORD>(nDataLen)))
        {
            SetLastError(L"Encryption failed");
        }
        if (!encAead.Finalize(dst + AadSize + nDataLen, MacSize))
        {
            SetLastError(L"Tag failed on encryption");
        }
        return dst;
    }

    virtual BYTE* RestoreBuffer(BYTE* pTransBuffer, int nTransDataLen, int* pnRestoredDataLen)
    {
        if (!pTransBuffer)
        {
            wanted = nTransDataLen;
            if (decPlain.GetAvailable() >= wanted)
            {
                *pnRestoredDataLen = 0;
            }
            else if (decBuffer.GetAvailable() < AadSize)
            {
                *pnRestoredDataLen = AadSize - decBuffer.GetAvailable();
            }
            else
            {
                int needed = AadSize + Swap16IfLE(*(reinterpret_cast<CARD16*>(decBuffer.GetHead()))) + MacSize;
                *pnRestoredDataLen = maximum(needed - decBuffer.GetAvailable(), 0);
            }
            BYTE* dst = decBuffer.EnsureFree(static_cast<DWORD>(*pnRestoredDataLen));
            decBuffer.size += static_cast<DWORD>(*pnRestoredDataLen);
            return dst;
        }
        while (decBuffer.GetAvailable() >= AadSize)
        {
            DWORD size = Swap16IfLE(*(reinterpret_cast<CARD16*>(decBuffer.GetHead())));
            if (AadSize + size + MacSize > static_cast<DWORD>(decBuffer.GetAvailable()))
            {
                break;
            }
            BYTE* dst = decPlain.EnsureFree(size);
            decAead.SetNonce(decMsg++);
            decAead.SetAad(decBuffer.GetHead(), AadSize);
            std::memcpy(dst, decBuffer.GetHead() + AadSize, size);
            if (!decAead.Process(dst, size, false))
            {
                SetLastError(L"Decryption failed");
            }
            BYTE tag[MacSize];
            if (!decAead.Finalize(tag, MacSize))
            {
                SetLastError(L"Tag failed on decryption");
            }
            if (!ArrayEqual(tag, decBuffer.GetHead() + AadSize + size, MacSize))
            {
                SetLastError(L"Decryption tag does not match");
            }
            decPlain.size += size;
            decBuffer.pos += AadSize + size + MacSize;
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
    static bool SetLastError(const wchar_t* const error)
    {
        vnclog.Print(0, _T("AESEAXPlugin: %S\n"), error);
        throw WarningException(error);
        return false;
    }
};

// RSA bases key exchange

struct RSAKEX
{
    static const int MinRsaKeyLength{ 1024 };
    static const int MaxRsaKeyLength{ 8192 };
    static const int MaxRsaKeyBytes = MaxRsaKeyLength / 8;
    static const int MaxSymKeyBytes = 256 / 8;
    static const int MaxHashBytes{ 32 };

    struct PubKeyBlob
    {
        BLOBHEADER hdr;
        RSAPUBKEY key;
        BYTE modulus[MaxRsaKeyBytes];
    };

    struct RSAKeyInfo
    {
        DWORD length, bytes;
        BYTE modulus[MaxRsaKeyBytes];
        BYTE exp[MaxRsaKeyBytes];
    };

    struct IConnection
    {
        virtual void ReadExact(char* const, const int) = 0;
        virtual void WriteExact(char* const, const int) = 0;
        virtual bool CompareFingerprint(char* const hex) = 0;
        virtual void SaveFingerprint(char* const hex) = 0;
    };

    IConnection& conn;
    DWORD keySize;
    RSAKeyInfo serverKey, clientKey;
    HCRYPTPROV hProv;
    HCRYPTKEY hServerKey, hClientKey;
    HCRYPTHASH hHash;
    std::array<BYTE, MaxSymKeyBytes> serverRandom;
    std::array<BYTE, MaxSymKeyBytes> clientRandom;
    int subtype;
    wchar_t lastError[1024U];

    RSAKEX(IConnection& c, DWORD keysz) : conn(c), keySize(keysz), hProv{ 0 }, hServerKey{ 0 }, hClientKey{ 0 }, hHash{ 0 }
    { lastError[0U] = 0; }

    ~RSAKEX()
    {
        std::memset(&serverKey, 0, sizeof(serverKey));
        std::memset(&clientKey, 0, sizeof(clientKey));
        std::memset(&serverRandom, 0, sizeof(serverRandom));
        std::memset(&clientRandom, 0, sizeof(clientRandom));
        if (hHash)
        {
            ::CryptDestroyHash(hHash);
        }
        if (hClientKey)
        {
            ::CryptDestroyKey(hClientKey);
        }
        if (hServerKey)
        {
            ::CryptDestroyKey(hServerKey);
        }
        if (hProv)
        {
            ::CryptReleaseContext(hProv, 0);
        }
    }

    bool ReadPublicKey()
    {
        PubKeyBlob keyBlob;

        conn.ReadExact(reinterpret_cast<char*>(&serverKey.length), sizeof(serverKey.length));
        serverKey.length = Swap32IfLE(serverKey.length);
        if (serverKey.length < MinRsaKeyLength)
        {
            ::swprintf_s(lastError, L"Server RSA key is too small (%d)", serverKey.length);
            return false;
        }
        if (serverKey.length > MaxRsaKeyLength)
        {
            ::swprintf_s(lastError, L"Server RSA key is too big (%d)", serverKey.length);
            return false;
        }
        serverKey.bytes = (serverKey.length + 7) / 8;
        conn.ReadExact(reinterpret_cast<char*>(serverKey.modulus), static_cast<int>(serverKey.bytes));
        conn.ReadExact(reinterpret_cast<char*>(serverKey.exp), static_cast<int>(serverKey.bytes));
        for (DWORD i{ 0U }; i < serverKey.bytes - 4; ++i)
        {
            if (serverKey.exp[i] != 0)
            {
                ::swprintf_s(lastError, L"Server RSA exponent is too big (%d)", i);
                return false;
            }
        }
        if (!hProv && !::CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        {
            ::swprintf_s(lastError, L"CryptAcquireContext failed (%u)", ::GetLastError());
            return false;
        }
        keyBlob.hdr.bType = PUBLICKEYBLOB;
        keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
        keyBlob.hdr.reserved = 0;
        keyBlob.hdr.aiKeyAlg = CALG_RSA_KEYX;
        keyBlob.key.magic = BCRYPT_RSAPUBLIC_MAGIC; // RSA1
        keyBlob.key.bitlen = serverKey.length;
        keyBlob.key.pubexp = Swap32IfLE(*(reinterpret_cast<DWORD*>(&serverKey.exp[serverKey.bytes - 4])));
        std::memcpy(keyBlob.modulus, serverKey.modulus, serverKey.bytes);
        ArraySwap(keyBlob.modulus, serverKey.bytes);
        if (!::CryptImportKey(hProv, reinterpret_cast<BYTE*>(&keyBlob), offsetof(PubKeyBlob, modulus) + serverKey.bytes, 0, 0, &hServerKey))
        {
            ::swprintf_s(lastError, L"Invalid server RSA key (%u)", ::GetLastError());
            return false;
        }
        return true;
    }

    bool VerifyServer()
    {
        std::array<BYTE, 8U> f;
        std::array<WCHAR, 1024U> msg;

        DWORD size = Swap32IfLE(serverKey.length);
        if (!HashUpdate(CALG_SHA1, &size, 4) || !HashUpdate(CALG_SHA1, serverKey.modulus, serverKey.bytes) ||
            !HashUpdate(CALG_SHA1, serverKey.exp, serverKey.bytes) || !HashDigest(f.data(), sizeof(f)))
        {
            return false;
        }
        ::sprintf_s(hex, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", f[0U], f[1U], f[2U], f[3U], f[4U], f[5U], f[6U], f[7U]);
        mn_encode(f.data(), 8, catchphrase.data(), 1024, "x x x. x x x. x x x\n");
        std::strcat(catchphrase.data(), ".");
        vnclog.Print(0, _T("RSAKEX: Server fingerprint is %s\n"), hex);
        if (conn.CompareFingerprint(hex))
        {
            return true;
        }
        int nButtonPressed{ 0 };
        ::swprintf_s(msg.data(), msg.size(), L"The server has provided the following identifying information:\n\nFingerprint: %hs\n\nDo you want to continue?\n", hex);
        TASKDIALOGCONFIG tc{ 0 };
        BOOL bPersist = FALSE;
        tc.cbSize = sizeof(tc);
        tc.dwFlags = TDF_SIZE_TO_CONTENT;
        tc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
        tc.pszWindowTitle = L"UltraVNC Viewer - Warning";
        tc.hInstance = m_hInstResDLL;
        tc.pszMainIcon = MAKEINTRESOURCEW(IDR_TRAY);
        tc.pszMainInstruction = L"Identity confirmation";
        tc.pszContent = msg.data();
        tc.pszVerificationText = L"Don't ask anymore";
        ::TaskDialogIndirect(&tc, &nButtonPressed, nullptr, &bPersist);
        if (nButtonPressed != IDOK)
        {
            throw QuietException(L"Authentication cancelled");
        }
        if (bPersist)
        {
            conn.SaveFingerprint(hex);
        }
        return true;
    }

    bool WritePublicKey()
    {
#define RSA2048BIT_KEY (RSA1024BIT_KEY << 1)
        if (!::CryptGenKey(hProv, AT_KEYEXCHANGE, RSA2048BIT_KEY | CRYPT_EXPORTABLE, &hClientKey))
        {
            ::swprintf_s(lastError, L"CryptGenKey failed (%u)", ::GetLastError());
            return false;
        }
        std::array<BYTE, 2048U> keyData;
        DWORD size = sizeof(keyData);
        if (!::CryptExportKey(hClientKey, 0, PUBLICKEYBLOB, 0, keyData.data(), &size))
        {
            ::swprintf_s(lastError, L"CryptExportKey failed (%u)", ::GetLastError());
            return false;
        }
        PubKeyBlob* keyBlob = reinterpret_cast<PubKeyBlob*>(keyData.data());
        if ((keyBlob->hdr.aiKeyAlg != CALG_RSA_KEYX) || (keyBlob->key.magic != BCRYPT_RSAPUBLIC_MAGIC))
        {
            ::swprintf_s(lastError, L"Invalid client key generated");
            return false;
        }
        clientKey.length = keyBlob->key.bitlen;
        clientKey.bytes = (clientKey.length + 7) / 8;
        std::memset(clientKey.exp, 0, clientKey.bytes);
        *(reinterpret_cast<DWORD*>(&clientKey.exp[clientKey.bytes - 4])) = Swap32IfLE(keyBlob->key.pubexp);
        std::memcpy(clientKey.modulus, keyBlob->modulus, clientKey.bytes);
        ArraySwap(clientKey.modulus, clientKey.bytes);
        DWORD keyLength = Swap32IfLE(clientKey.length);
        conn.WriteExact(reinterpret_cast<char*>(&keyLength), 4);
        conn.WriteExact(reinterpret_cast<char*>(clientKey.modulus), static_cast<int>(clientKey.bytes));
        conn.WriteExact(reinterpret_cast<char*>(clientKey.exp), static_cast<int>(clientKey.bytes));
        return true;
    }

    bool WriteRandom()
    {
        BYTE buffer[MaxRsaKeyBytes];
        DWORD size = keySize / 8;

        if (!::CryptGenRandom(hProv, size, reinterpret_cast<BYTE*>(clientRandom.data())))
        {
            ::swprintf_s(lastError, L"CryptGenRandom failed (%u)", ::GetLastError());
            return false;
        }
        std::memcpy(buffer, clientRandom.data(), size);
        if (!::CryptEncrypt(hServerKey, 0, TRUE, 0, buffer, &size, sizeof(buffer)))
        {
            ::swprintf_s(lastError, L"CryptEncrypt failed (%u)", ::GetLastError());
            return false;
        }
        if (size != serverKey.bytes)
        {
            ::swprintf_s(lastError, L"Server key size doesn't match (%u vs %u)", size, serverKey.bytes);
            return false;
        }
        DWORD bufSize = Swap16IfLE(size);
        conn.WriteExact(reinterpret_cast<char*>(&bufSize), 2);
        ArraySwap(buffer, size);
        conn.WriteExact(reinterpret_cast<char*>(buffer), static_cast<int>(size));
        return true;
    }

    bool ReadRandom()
    {
        BYTE buffer[MaxRsaKeyBytes];
        DWORD size{ 0U };

        conn.ReadExact(reinterpret_cast<char*>(&size), 2);
        size = Swap16IfLE(size);
        if (size != clientKey.bytes)
        {
            ::swprintf_s(lastError, L"Client key size doesn't match (%u vs %u)", size, clientKey.bytes);
            return false;
        }
        conn.ReadExact(reinterpret_cast<char*>(buffer), static_cast<int>(size));
        ArraySwap(buffer, size);
        if (!::CryptDecrypt(hClientKey, 0, TRUE, 0, reinterpret_cast<BYTE*>(buffer), &size))
        {
            ::swprintf_s(lastError, L"Cannot decrypt server random (%u)", ::GetLastError());
            return false;
        }
        if (size != keySize / 8)
        {
            ::swprintf_s(lastError, L"Server random size doesn't match (%u vs %u)", size, keySize / 8);
            return false;
        }
        std::memcpy(serverRandom.data(), buffer, size);
        return true;
    }

    bool SetCipher(bool& fUsePlugin, IPlugin** ppPluginInterface)
    {
        ALG_ID algId = (keySize == 128 ? CALG_SHA1 : CALG_SHA_256);
        DWORD size = keySize / 8;
        BYTE encKey[MaxSymKeyBytes], decKey[MaxSymKeyBytes];

        if (!HashUpdate(algId, serverRandom.data(), size) || !HashUpdate(algId, clientRandom.data(), size) || !HashDigest(encKey, size))
        {
            return false;
        }
        if (!HashUpdate(algId, clientRandom.data(), size) || !HashUpdate(algId, serverRandom.data(), size) || !HashDigest(decKey, size))
        {
            return false;
        }
        if (ppPluginInterface)
        {
            fUsePlugin = true;
            *ppPluginInterface = new AESEAXPlugin(keySize, encKey, decKey);
        }
        return true;
    }

    bool WriteHash()
    {
        BYTE calcHash[MaxHashBytes];
        DWORD size = sizeof(calcHash);

        if (!CalcKeysHash(clientKey, serverKey, calcHash, size))
        {
            return false;
        }
        conn.WriteExact(reinterpret_cast<char*>(calcHash), static_cast<int>(size));
        return true;
    }

    bool ReadHash()
    {
        BYTE calcHash[MaxHashBytes], hash[MaxHashBytes];
        DWORD size = sizeof(calcHash);

        if (!CalcKeysHash(serverKey, clientKey, calcHash, size))
        {
            return false;
        }
        conn.ReadExact(reinterpret_cast<char*>(hash), static_cast<int>(size));
        if (!ArrayEqual(hash, calcHash, size))
        {
            ::swprintf_s(lastError, L"Hash does not match");
            return false;
        }
        return true;
    }

    bool ReadSubtype()
    {
        subtype = 0;
        conn.ReadExact(reinterpret_cast<char*>(&subtype), 1);
        if ((subtype != secTypeRA2UserPass) && (subtype != secTypeRA2Pass) && (subtype != secTypeRA2None))
        {
            ::swprintf_s(lastError, L"Invalid subtype (%d)", subtype);
            return false;
        }
        return true;
    }

    bool WriteCredentials(TCHAR* const host, const int port, char* const cmdlnUser, char* const clearPasswd)
    {
        DWORD size;

        if (subtype == secTypeRA2None)
        {
            return true;
        }

        if (std::strlen(clearPasswd) == 0)
        {
            AuthDialog ad;
            if (!ad.DoDialog((subtype == secTypeRA2UserPass) ? dtUserPassRSA : dtPassRSA, host, port, hex, catchphrase.data()))
            {
                throw QuietException(L"Authentication cancelled");
            }
            if (subtype == secTypeRA2UserPass)
            {
                ::strcpy_s(cmdlnUser, 256, ad.m_user.data());
            }
            ::strcpy_s(clearPasswd, 256, ad.m_passwd.data());
        }
        size = (subtype == secTypeRA2UserPass ? static_cast<DWORD>(std::strlen(cmdlnUser)) : 0);
        conn.WriteExact(reinterpret_cast<char*>(&size), 1);
        conn.WriteExact(cmdlnUser, static_cast<int>(size));
        size = static_cast<DWORD>(std::strlen(clearPasswd));
        conn.WriteExact(reinterpret_cast<char*>(&size), 1);
        conn.WriteExact(clearPasswd, static_cast<int>(size));
        return true;
    }

private:
    bool CalcKeysHash(RSAKeyInfo& clientKeyIn, RSAKeyInfo serverKeyIn, BYTE* const hash, DWORD& size)
    {
        ALG_ID algId = (keySize == 128 ? CALG_SHA1 : CALG_SHA_256);
        DWORD clientLength = Swap32IfLE(clientKeyIn.length);
        DWORD serverLength = Swap32IfLE(serverKeyIn.length);
        DWORD hashSize = (keySize == 128 ? 20 : 32);

        if (hashSize > size)
        {
            ::swprintf_s(lastError, L"Keys hash buffer too small (%u vs %u)", size, hashSize);
            return false;
        }
        if (!HashUpdate(algId, &clientLength, 4) || !HashUpdate(algId, clientKeyIn.modulus, clientKeyIn.bytes) ||
            !HashUpdate(algId, clientKeyIn.exp, clientKeyIn.bytes) || !HashUpdate(algId, &serverLength, 4) ||
            !HashUpdate(algId, serverKeyIn.modulus, serverKeyIn.bytes) || !HashUpdate(algId, serverKeyIn.exp, serverKeyIn.bytes) ||
            !HashDigest(hash, hashSize))
        {
            return false;
        }
        size = hashSize;
        return true;
    }

    bool HashUpdate(const ALG_ID algId, void* const data, const DWORD size)
    {
        if (!hHash && !::CryptCreateHash(hProv, algId, 0, 0, &hHash))
        {
            ::swprintf_s(lastError, L"CryptCreateHash failed (%u)", ::GetLastError());
            return false;
        }
        if (!::CryptHashData(hHash, reinterpret_cast<BYTE*>(data), size, 0))
        {
            ::swprintf_s(lastError, L"CryptHashData failed (%u)", ::GetLastError());
            return false;
        }
        return true;
    }

    bool HashDigest(void* const out, const DWORD size)
    {
        std::array<BYTE, 256U> buffer;
        DWORD bufSize = sizeof(buffer);

        if (!::CryptGetHashParam(hHash, HP_HASHVAL, reinterpret_cast<BYTE*>(buffer.data()), &bufSize, 0))
        {
            ::swprintf_s(lastError, L"CryptGetHashParam failed (%u)", ::GetLastError());
            return false;
        }
        ::CryptDestroyHash(hHash);
        hHash = 0;
        std::memset(out, 0, size);
        std::memcpy(out, buffer.data(), size < bufSize ? size : bufSize);
        return true;
    }
};

static const TCHAR* FingerprintSection = _T("fingerprint");

struct KEXHost : public RSAKEX::IConnection
{
    ClientConnection* pConn;
    TCHAR *key, *fname;

    KEXHost(ClientConnection* pcc, TCHAR* k, TCHAR* f) : pConn(pcc), key(k), fname(f) {}

    virtual void ReadExact(char* const buf, const int bytes) { pConn->ReadExact(buf, bytes); }

    virtual void WriteExact(char* const buf, const int bytes) { pConn->WriteExact(buf, bytes); }

    virtual bool CompareFingerprint(char* const hexLocal)
    {
        std::array<TCHAR, 32U> buf{ 0 };
        ::GetPrivateProfileString(FingerprintSection, key, nullptr, buf.data(), sizeof(buf), fname);
        return ::_tcscmp(reinterpret_cast<LPCTSTR>(hexLocal), buf.data()) == 0;
    }

    virtual void SaveFingerprint(char* const hexLocal) { ::WritePrivateProfileString(FingerprintSection, key, reinterpret_cast<LPCTSTR>(hexLocal), fname); }
};

void ClientConnection::AuthRSAAES(const int keySize, const bool encrypted)
{
    std::array<TCHAR, MAX_HOST_NAME_LEN> key;
    ::_stprintf_s(key.data(), key.size(), _T("%s:%d"), m_host.data(), m_port);
    KEXHost host(this, key.data(), m_opts->getDefaultOptionsFileName());
    RSAKEX st(host, static_cast<DWORD>(keySize));
    std::array<char, 256U> _rsa_user{ 0 };
    std::array<char, 256U> _rsa_passwd{ 0 };
    ::strcpy_s(_rsa_user.data(), 256, m_cmdlnUser.data());
    ::strcpy_s(_rsa_passwd.data(), 256, m_clearPasswd.data());
    if (!st.ReadPublicKey() || !st.VerifyServer() || !st.WritePublicKey() || !st.WriteRandom() || !st.ReadRandom() ||
        !st.SetCipher(m_fUsePlugin, &m_pPluginInterface) || !st.WriteHash() || !st.ReadHash() || !st.ReadSubtype() ||
        !st.WriteCredentials(m_host.data(), m_port, _rsa_user.data(), _rsa_passwd.data()))
    {
        if (std::wcslen(st.lastError))
        {
            vnclog.Print(0, _T("AuthRSAAES: %S\n"), st.lastError);
            throw WarningException(st.lastError);
        }
    }
    if (!encrypted && m_pPluginInterface)
    {
        m_fUsePlugin = false;
        delete m_pPluginInterface;
        m_pPluginInterface = nullptr;
    }
}

// Apple Remote Desktop / macOS Screen Sharing authentication (security type 30).
// Server sends DH parameters + its public key; the client replies with
// AES-128-ECB(MD5(dh_shared_secret), 64B user || 64B pass) + its DH public key.
void ClientConnection::AuthAppleDH()
{
    CARD16 gen, keySize;
    ReadExact(reinterpret_cast<char*>(&gen), 2);
    ReadExact(reinterpret_cast<char*>(&keySize), 2);
    gen = Swap16IfLE(gen);
    keySize = Swap16IfLE(keySize);
    if ((gen < 2) || (keySize < 64) || (keySize > 512))
    {
        throw WarningException(L"AppleDH: invalid DH parameters from server");
    }
    std::vector<BYTE> modulus(keySize), serverPub(keySize);
    ReadExact(reinterpret_cast<char*>(modulus.data()), keySize);
    ReadExact(reinterpret_cast<char*>(serverPub.data()), keySize);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr, hPeer = nullptr;
    BCRYPT_SECRET_HANDLE hSecret = nullptr;
    BCRYPT_ALG_HANDLE hMd5Alg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::vector<BYTE> secret(keySize);
    ULONG secretLen{ 0U };

    struct Cleanup
    {
        BCRYPT_ALG_HANDLE *pAlg, *pMd5Alg;
        BCRYPT_KEY_HANDLE *pKey, *pPeer;
        BCRYPT_SECRET_HANDLE* pSecret;
        BCRYPT_HASH_HANDLE* pHash;
        ~Cleanup()
        {
            if (*pHash)
            {
                ::BCryptDestroyHash(*pHash);
            }
            if (*pMd5Alg)
            {
                ::BCryptCloseAlgorithmProvider(*pMd5Alg, 0);
            }
            if (*pSecret)
            {
                ::BCryptDestroySecret(*pSecret);
            }
            if (*pPeer)
            {
                ::BCryptDestroyKey(*pPeer);
            }
            if (*pKey)
            {
                ::BCryptDestroyKey(*pKey);
            }
            if (*pAlg)
            {
                ::BCryptCloseAlgorithmProvider(*pAlg, 0);
            }
        }
    } cleanup{ &hAlg, &hMd5Alg, &hKey, &hPeer, &hSecret, &hHash };

    // Generate a DH key pair constrained to the server's parameters
    // (modulus + generator, keySize bytes each, big-endian)
    {
        std::vector<BYTE> blob(sizeof(BCRYPT_DH_PARAMETER_HEADER) + 2 * keySize, 0);
        BCRYPT_DH_PARAMETER_HEADER* hdr = reinterpret_cast<BCRYPT_DH_PARAMETER_HEADER*>(blob.data());
        hdr->cbLength = static_cast<ULONG>(blob.size());
        hdr->dwMagic = BCRYPT_DH_PARAMETERS_MAGIC;
        hdr->cbKeyLength = keySize;
        std::memcpy(blob.data() + sizeof(BCRYPT_DH_PARAMETER_HEADER), modulus.data(), keySize);
        BYTE* genField = blob.data() + sizeof(BCRYPT_DH_PARAMETER_HEADER) + keySize;
        genField[keySize - 2] = static_cast<BYTE>((gen >> 8));
        genField[keySize - 1] = static_cast<BYTE>((gen & 0xFF));

        NTSTATUS st = ::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_DH_ALGORITHM, nullptr, 0);
        if (st == 0)
        {
            st = ::BCryptGenerateKeyPair(hAlg, &hKey, keySize * 8, 0);
        }
        if (st == 0)
        {
            st = ::BCryptSetProperty(hKey, BCRYPT_DH_PARAMETERS, blob.data(), static_cast<ULONG>(blob.size()), 0);
        }
        if (st == 0)
        {
            st = ::BCryptFinalizeKeyPair(hKey, 0);
        }
        if (st != 0)
        {
            throw WarningException(L"AppleDH: DH key generation failed");
        }
    }

    // Export our public key - last keySize bytes of the DH public blob
    std::vector<BYTE> clientPub;
    {
        ULONG cb{ 0U };
        if (::BCryptExportKey(hKey, nullptr, BCRYPT_DH_PUBLIC_BLOB, nullptr, 0, &cb, 0) != 0)
        {
            throw WarningException(L"AppleDH: DH public key export failed");
        }
        std::vector<BYTE> blob(cb);
        if ((::BCryptExportKey(hKey, nullptr, BCRYPT_DH_PUBLIC_BLOB, blob.data(), cb, &cb, 0) != 0) || (cb < keySize))
        {
            throw WarningException(L"AppleDH: DH public key export failed");
        }
        clientPub.assign(blob.data() + cb - keySize, blob.data() + cb);
    }

    // Import the server's public key and derive the shared secret
    {
        std::vector<BYTE> blob(sizeof(BCRYPT_DH_KEY_BLOB) + 3 * keySize, 0);
        BCRYPT_DH_KEY_BLOB* hdr = reinterpret_cast<BCRYPT_DH_KEY_BLOB*>(blob.data());
        hdr->dwMagic = BCRYPT_DH_PUBLIC_MAGIC;
        hdr->cbKey = keySize;
        BYTE* p = blob.data() + sizeof(BCRYPT_DH_KEY_BLOB);
        std::memcpy(p, modulus.data(), keySize);
        p[keySize * 2 - 2] = static_cast<BYTE>((gen >> 8));
        p[keySize * 2 - 1] = static_cast<BYTE>((gen & 0xFF));
        std::memcpy(p + 2 * keySize, serverPub.data(), keySize);

        if ((::BCryptImportKeyPair(hAlg, nullptr, BCRYPT_DH_PUBLIC_BLOB, &hPeer, blob.data(), static_cast<ULONG>(blob.size()), 0) !=
             0) ||
            (::BCryptSecretAgreement(hKey, hPeer, &hSecret, 0) != 0) ||
            (::BCryptDeriveKey(hSecret, BCRYPT_KDF_RAW_SECRET, nullptr, secret.data(), keySize, &secretLen, 0) != 0) ||
            (secretLen == 0) || (secretLen > keySize))
        {
            throw WarningException(L"AppleDH: DH key agreement failed");
        }
        // CNG returns the raw DH secret little-endian with the high bytes
        // truncated; convert to a keySize-byte big-endian value.
        std::vector<BYTE> secretBE(keySize, 0);
        for (ULONG i{ 0U }; i < secretLen; ++i)
        {
            secretBE[keySize - 1 - i] = secret[i];
        }
        ::SecureZeroMemory(secret.data(), secret.size());
        secret = secretBE;
        secretLen = keySize;
    }

    // AES-128 key = MD5(shared secret)
    std::array<BYTE, 16U> aesKey;
    if ((::BCryptOpenAlgorithmProvider(&hMd5Alg, BCRYPT_MD5_ALGORITHM, nullptr, 0) != 0) ||
        (::BCryptCreateHash(hMd5Alg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) ||
        (::BCryptHashData(hHash, secret.data(), secretLen, 0) != 0) ||
        (::BCryptFinishHash(hHash, aesKey.data(), sizeof(aesKey), 0) != 0))
    {
        ::SecureZeroMemory(secret.data(), secret.size());
        throw WarningException(L"AppleDH: MD5 key derivation failed");
    }
    ::SecureZeroMemory(secret.data(), secret.size());

    // Credentials: 64B user + 64B pass, null-terminated, random padding
    std::array<char, 256U> user{};
    std::array<char, 256U> passwd{};
    ::strncpy_s(user.data(), sizeof(user), m_cmdlnUser.data(), _TRUNCATE);
    ::strncpy_s(passwd.data(), sizeof(passwd), m_clearPasswd.data(), _TRUNCATE);
    if ((user[0U] == '\0') || (passwd[0U] == '\0'))
    {
        AuthDialog ad;
        ad.SetStatusWindow(m_hwndStatus, m_opts->m_ClassName.data());
        if (!ad.DoDialog(dtUserPass, m_host.data(), m_port))
        {
            throw WarningException(L"Authentication cancelled");
        }
        ::strncpy_s(user.data(), sizeof(user), ad.m_user.data(), _TRUNCATE);
        ::strncpy_s(passwd.data(), sizeof(passwd), ad.m_passwd.data(), _TRUNCATE);
        ::strncpy_s(m_clearPasswd.data(), m_clearPasswd.size(), ad.m_passwd.data(), _TRUNCATE);
    }

    std::array<BYTE, 128U> cred;
    BcryptHelp::Random(cred.data(), sizeof(cred));
    ::strncpy_s(reinterpret_cast<char*>(cred.data()), 64, user.data(), _TRUNCATE);
    ::strncpy_s(reinterpret_cast<char*>(cred.data()) + 64, 64, passwd.data(), _TRUNCATE);
    ::SecureZeroMemory(passwd.data(), sizeof(passwd));

    AESCipher aes;
    if (!aes.Init(128, aesKey.data()) || !aes.Encrypt(cred.data(), sizeof(cred)))
    {
        ::SecureZeroMemory(cred.data(), sizeof(cred));
        ::SecureZeroMemory(aesKey.data(), sizeof(aesKey));
        throw WarningException(L"AppleDH: credential encryption failed");
    }
    ::SecureZeroMemory(aesKey.data(), sizeof(aesKey));

    WriteExact(reinterpret_cast<char*>(cred.data()), sizeof(cred));
    ::SecureZeroMemory(cred.data(), sizeof(cred));
    WriteExact(reinterpret_cast<char*>(clientPub.data()), keySize);

    vnclog.Print(0, _T("AppleDH: sent encrypted credentials for user '%hs' (%d-bit DH)\n"), user.data(), keySize * 8);
}
