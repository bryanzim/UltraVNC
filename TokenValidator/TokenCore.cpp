// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//

// TokenCore.cpp - shared TOTP helpers for UltraVNC TokenValidator

#include "TokenCore.h"

#include "../3rdparty/qrcodegen/qrcodegen.hpp"

#include <windows.h>

#include <accctrl.h>
#include <aclapi.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <wincred.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

namespace TokenCore
{

    namespace
    {

        constexpr const char* kCredPrefix = "UltraVNC_TOTP_";
        constexpr std::size_t kCredPrefixLen{ 14U }; // strlen("UltraVNC_TOTP_")

        std::array<char, MAX_PATH> g_iniPath{};

        bool SetIniPath(const char* const modulePath)
        {
            if (!modulePath || !modulePath[0U])
            {
                return false;
            }

            const char* lastSlash = std::strrchr(modulePath, '\\');
            if (!lastSlash)
            {
                return false;
            }

            std::size_t dirLen = lastSlash - modulePath + 1;
            if (dirLen + 16 >= MAX_PATH)
            {
                return false;
            }

            std::memcpy(g_iniPath.data(), modulePath, dirLen);
            ::strcpy_s(g_iniPath.data() + dirLen, MAX_PATH - dirLen, "TokenValidator.credentials.ini");
            return true;
        }

        const char* GetIniPath()
        {
            if (g_iniPath[0U])
            {
                return g_iniPath.data();
            }

            std::array<char, MAX_PATH> path{};
            if (::GetModuleFileNameA(nullptr, path.data(), path.size()))
            {
                SetIniPath(path.data());
            }

            return g_iniPath.data();
        }

        std::string UrlEncode(const std::string& s)
        {
            std::string out;
            for (unsigned char c : s)
            {
                if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '-') ||
                    (c == '_') || (c == '.') || (c == '~'))
                {
                    out.push_back(static_cast<char>(c));
                }
                else
                {
                    std::array<char, 4U> buf{};
                    ::sprintf_s(buf.data(), buf.size(), "%%%02X", c);
                    out += buf.data();
                }
            }
            return out;
        }

        std::string WcharToUtf8(const wchar_t* const wide)
        {
            if (!wide || !wide[0U])
            {
                return std::string();
            }
            int len = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
            if (len <= 0)
            {
                return std::string();
            }
            std::string s(len, '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, &s[0U], len, nullptr, nullptr);
            s.resize(std::strlen(s.c_str()));
            return s;
        }

        std::string Utf8ToWcharString(const char* const utf8, std::wstring& wide)
        {
            wide.clear();
            if (!utf8 || !utf8[0U])
            {
                return std::string();
            }
            int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
            if (len <= 0)
            {
                return std::string();
            }
            wide.resize(len);
            ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wide[0U], len);
            wide.resize(std::wcslen(wide.c_str()));
            return WcharToUtf8(wide.c_str());
        }

    } // anonymous namespace

    // ---------------------------------------------------------------------------
    // Base32 encode/decode
    // ---------------------------------------------------------------------------
    bool Base32Decode(const std::string& in, std::vector<BYTE>& out)
    {
        static std::array<const int, 256U> table{
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
            22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
            19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        };

        out.clear();
        int buffer{ 0 };
        int bitsInBuffer{ 0 };

        for (unsigned char c : in)
        {
            if (c == '=')
            {
                break;
            }
            if ((c == ' ') || (c == '-'))
            {
                continue;
            }
            if (c >= 256)
            {
                return false;
            }
            int val = table[c];
            if (val < 0)
            {
                return false;
            }
            buffer = (buffer << 5) | val;
            bitsInBuffer += 5;
            if (bitsInBuffer >= 8)
            {
                bitsInBuffer -= 8;
                out.push_back(static_cast<BYTE>(buffer >> bitsInBuffer));
                buffer &= (1 << bitsInBuffer) - 1;
            }
        }
        return !out.empty();
    }

    static std::string Base32Encode(const std::vector<BYTE>& data)
    {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        std::string out;
        int buffer{ 0 };
        int bitsInBuffer{ 0 };
        for (BYTE b : data)
        {
            buffer = (buffer << 8) | b;
            bitsInBuffer += 8;
            while (bitsInBuffer >= 5)
            {
                out.push_back(alphabet[(buffer >> (bitsInBuffer - 5)) & 0x1F]);
                bitsInBuffer -= 5;
                buffer &= (1 << bitsInBuffer) - 1;
            }
        }
        if (bitsInBuffer > 0)
        {
            out.push_back(alphabet[(buffer << (5 - bitsInBuffer)) & 0x1F]);
        }
        return out;
    }

    static bool GenerateRandomBytes(std::vector<BYTE>& out, const ULONG len)
    {
        out.resize(len);
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, nullptr, 0)))
        {
            return false;
        }
        NTSTATUS status = ::BCryptGenRandom(hAlg, out, len, 0);
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return BCRYPT_SUCCESS(status);
    }

    bool GenerateRandomBase32Secret(std::string& out, const std::size_t bits)
    {
        if ((bits % 8 != 0) || (bits == 0))
        {
            return false;
        }
        std::vector<BYTE> randBytes;
        if (!GenerateRandomBytes(randBytes, static_cast<ULONG>(bits / 8)))
        {
            return false;
        }
        out = Base32Encode(randBytes);
        return true;
    }

    bool ValidateBase32(const std::string& s)
    {
        if (s.empty() || ((s.size() % 8) != 0))
        {
            return false;
        }
        for (char c : s)
        {
            if ((c >= 'A') && (c <= 'Z'))
            {
                continue;
            }
            if ((c >= '2') && (c <= '7'))
            {
                continue;
            }
            return false;
        }
        return true;
    }

    // ---------------------------------------------------------------------------
    // Storage path setup
    // ---------------------------------------------------------------------------
    bool SetSecretStorePathsFromDllPath(const char* const dllPath) { return SetIniPath(dllPath); }

    bool SetSecretStorePathsFromModuleHandle(const HMODULE hModule)
    {
        std::array<char, MAX_PATH> path{};
        if (!::GetModuleFileNameA(hModule, path.data(), path.size()))
        {
            return false;
        }
        return SetIniPath(path.data());
    }

    std::string GetSecretStoreIniPath()
    {
        const char* path = GetIniPath();
        return path ? std::string(path) : std::string();
    }

    // ---------------------------------------------------------------------------
    // Credential Manager + DPAPI INI helpers
    // ---------------------------------------------------------------------------
    static bool WriteCredential(const char* const user, const std::string& secretUtf8)
    {
        std::wstring wideUser;
        Utf8ToWcharString(user, wideUser);
        if (wideUser.empty())
        {
            return false;
        }

        std::array<wchar_t, 256U> targetName{};
        ::swprintf_s(targetName.data(), targetName.size(), L"%hs%ls", kCredPrefix, wideUser.c_str());

        CREDENTIALW cred{};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = targetName.data();
        cred.CredentialBlobSize = static_cast<DWORD>(secretUtf8.size());
        cred.CredentialBlob = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(secretUtf8));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
        cred.UserName = const_cast<LPWSTR>(wideUser.c_str());

        return ::CredWriteW(&cred, 0) == TRUE;
    }

    static bool ReadCredential(const char* const user, std::string& secretOut)
    {
        std::wstring wideUser;
        Utf8ToWcharString(user, wideUser);
        if (wideUser.empty())
        {
            return false;
        }

        std::array<wchar_t, 256U> targetName{};
        ::swprintf_s(targetName.data(), targetName.size(), L"%hs%ls", kCredPrefix, wideUser.c_str());

        PCREDENTIALW cred = nullptr;
        if (!::CredReadW(targetName.data(), CRED_TYPE_GENERIC, 0, &cred) || !cred)
        {
            return false;
        }

        if (cred->CredentialBlob && (cred->CredentialBlobSize > 0))
        {
            secretOut.assign(reinterpret_cast<const char*>(cred->CredentialBlob), cred->CredentialBlobSize);
        }

        ::CredFree(cred);
        return !secretOut.empty();
    }

    static bool DeleteCredential(const char* const user)
    {
        std::wstring wideUser;
        Utf8ToWcharString(user, wideUser);
        if (wideUser.empty())
        {
            return false;
        }

        std::array<wchar_t, 256U> targetName{};
        ::swprintf_s(targetName.data(), targetName.size(), L"%hs%ls", kCredPrefix, wideUser.c_str());

        return ::CredDeleteW(targetName.data(), CRED_TYPE_GENERIC, 0) == TRUE;
    }

    static bool SetSecureFileAcl(const char* const path)
    {
        HANDLE hToken = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken))
        {
            return false;
        }

        DWORD userInfoLen{ 0U };
        ::GetTokenInformation(hToken, TokenUser, nullptr, 0, &userInfoLen);
        if (userInfoLen == 0)
        {
            ::CloseHandle(hToken);
            return false;
        }
        std::vector<BYTE> userInfo(userInfoLen);
        PTOKEN_USER pUser = reinterpret_cast<PTOKEN_USER>(userInfo);
        if (!::GetTokenInformation(hToken, TokenUser, pUser, userInfoLen, &userInfoLen) || !::IsValidSid(pUser->User.Sid))
        {
            ::CloseHandle(hToken);
            return false;
        }
        PSID userSid = pUser->User.Sid;

        std::vector<BYTE> systemSid(SECURITY_MAX_SID_SIZE);
        std::vector<BYTE> adminSid(SECURITY_MAX_SID_SIZE);
        DWORD sysLen = SECURITY_MAX_SID_SIZE, adminLen = SECURITY_MAX_SID_SIZE;
        BOOL sidsOk = ::CreateWellKnownSid(WinLocalSystemSid, nullptr, systemSid, &sysLen) &&
                      CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminSid, &adminLen);
        if (!sidsOk)
        {
            ::CloseHandle(hToken);
            return false;
        }

        std::array<EXPLICIT_ACCESS_A, 3U> ea{};

        ea[0U].grfAccessPermissions = GENERIC_ALL;
        ea[0U].grfAccessMode = SET_ACCESS;
        ea[0U].grfInheritance = NO_INHERITANCE;
        ea[0U].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0U].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[0U].Trustee.ptstrName = reinterpret_cast<LPSTR>(systemSid);

        ea[1U].grfAccessPermissions = GENERIC_ALL;
        ea[1U].grfAccessMode = SET_ACCESS;
        ea[1U].grfInheritance = NO_INHERITANCE;
        ea[1U].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[1U].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[1U].Trustee.ptstrName = reinterpret_cast<LPSTR>(adminSid);

        ea[2U].grfAccessPermissions = GENERIC_ALL;
        ea[2U].grfAccessMode = SET_ACCESS;
        ea[2U].grfInheritance = NO_INHERITANCE;
        ea[2U].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[2U].Trustee.TrusteeType = TRUSTEE_IS_USER;
        ea[2U].Trustee.ptstrName = reinterpret_cast<LPSTR>(userSid);

        PACL pAcl = nullptr;
        if (::SetEntriesInAclA(3, ea.data(), nullptr, &pAcl) != ERROR_SUCCESS)
        {
            ::CloseHandle(hToken);
            return false;
        }

        PSECURITY_DESCRIPTOR pSd = reinterpret_cast<PSECURITY_DESCRIPTOR>(::LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
        if (!pSd || !::InitializeSecurityDescriptor(pSd, SECURITY_DESCRIPTOR_REVISION) ||
            !::SetSecurityDescriptorDacl(pSd, TRUE, pAcl, FALSE))
        {
            ::LocalFree(pAcl);
            ::LocalFree(pSd);
            ::CloseHandle(hToken);
            return false;
        }
        ::SetSecurityDescriptorControl(pSd, SE_DACL_PROTECTED, SE_DACL_PROTECTED);

        const BOOL ok = ::SetFileSecurityA(path, DACL_SECURITY_INFORMATION, pSd);

        ::LocalFree(pAcl);
        ::LocalFree(pSd);
        ::CloseHandle(hToken);
        return ok == TRUE;
    }

    static bool WriteEncryptedIni(const char* const section, const char* const key, const std::string& secretUtf8)
    {
        const char* iniPath = GetIniPath();
        if (!iniPath || !iniPath[0U])
        {
            return false;
        }

        DATA_BLOB in{ static_cast<DWORD>(secretUtf8.size()), const_cast<BYTE*>(reinterpret_cast<const BYTE*>(secretUtf8)) };
        DATA_BLOB out{};

        if (!::CryptProtectData(&in, L"UltraVNC TOTP secret", nullptr, nullptr, nullptr, CRYPTPROTECT_LOCAL_MACHINE, &out))
        {
            return false;
        }

        DWORD base64Len{ 0U };
        if (!::CryptBinaryToStringA(out.pbData, out.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &base64Len))
        {
            ::LocalFree(out.pbData);
            return false;
        }

        std::vector<char> base64(base64Len);
        if (!::CryptBinaryToStringA(out.pbData, out.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64, &base64Len))
        {
            ::LocalFree(out.pbData);
            return false;
        }
        ::LocalFree(out.pbData);

        if (!::WritePrivateProfileStringA(section, key, base64, iniPath))
        {
            return false;
        }

        // Harden the fallback INI so only SYSTEM, Administrators, and the current
        // user can read it. Failures here are not fatal.
        SetSecureFileAcl(iniPath);
        return true;
    }

    static bool ReadEncryptedIni(const char* const section, const char* const key, std::string& secretOut)
    {
        const char* iniPath = GetIniPath();
        if (!iniPath || !iniPath[0U])
        {
            return false;
        }

        std::array<char, 4096U> base64{};
        if (!::GetPrivateProfileStringA(section, key, "", base64.data(), sizeof(base64), iniPath))
        {
            return false;
        }

        DWORD blobLen{ 0U };
        if (!::CryptStringToBinaryA(base64.data(), 0, CRYPT_STRING_BASE64, nullptr, &blobLen, nullptr, nullptr) || (blobLen == 0))
        {
            return false;
        }

        std::vector<BYTE> blob(blobLen);
        if (!::CryptStringToBinaryA(base64.data(), 0, CRYPT_STRING_BASE64, blob, &blobLen, nullptr, nullptr))
        {
            return false;
        }

        DATA_BLOB in{ blobLen, blob };
        DATA_BLOB out{};
        if (!::CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
        {
            return false;
        }

        if (out.pbData && (out.cbData > 0))
        {
            secretOut.assign(reinterpret_cast<const char*>(out.pbData), out.cbData);
        }

        ::LocalFree(out.pbData);
        return !secretOut.empty();
    }

    static bool DeleteEncryptedIni(const char* const section, const char* const key)
    {
        const char* iniPath = GetIniPath();
        if (!iniPath || !iniPath[0U])
        {
            return false;
        }
        return ::WritePrivateProfileStringA(section, key, nullptr, iniPath) != FALSE;
    }

    // ---------------------------------------------------------------------------
    // Public storage API
    // ---------------------------------------------------------------------------
    bool ReadTotpSecret(const std::string& user, std::string& secretOut)
    {
        secretOut.clear();
        if (user.empty())
        {
            return false;
        }
        if (ReadCredential(user.c_str(), secretOut))
        {
            return true;
        }
        return ReadEncryptedIni("TOTP", user.c_str(), secretOut);
    }

    bool WriteTotpSecret(const std::string& user, const std::string& secret)
    {
        if (user.empty() || secret.empty())
        {
            return false;
        }

        const bool ok1 = WriteCredential(user.c_str(), secret);
        const bool ok2 = WriteEncryptedIni("TOTP", user.c_str(), secret);
        return ok1 || ok2;
    }

    bool DeleteTotpSecret(const std::string& user)
    {
        if (user.empty())
        {
            return false;
        }
        const bool ok1 = DeleteCredential(user.c_str());
        const bool ok2 = DeleteEncryptedIni("TOTP", user.c_str());
        return ok1 || ok2;
    }

    bool ReadProtectedValue(const char* section, const std::string& key, std::string& valueOut)
    {
        valueOut.clear();
        return section && section[0U] && !key.empty() && ReadEncryptedIni(section, key.c_str(), valueOut);
    }

    bool WriteProtectedValue(const char* section, const std::string& key, const std::string& value)
    { return section && section[0U] && !key.empty() && !value.empty() && WriteEncryptedIni(section, key.c_str(), value); }

    bool DeleteProtectedValue(const char* section, const std::string& key)
    { return section && section[0U] && !key.empty() && DeleteEncryptedIni(section, key.c_str()); }

    std::vector<std::string> ListProtectedKeys(const char* const section)
    {
        std::vector<std::string> keys;
        const char* iniPath = GetIniPath();
        if (!section || !section[0U] || !iniPath || !iniPath[0U])
        {
            return keys;
        }

        std::array<char, 8192U> buf{};
        if (::GetPrivateProfileSectionA(section, buf.data(), sizeof(buf), iniPath) == 0)
        {
            return keys;
        }

        for (const char* p = buf.data(); *p; p += std::strlen(p) + 1)
        {
            std::string entry(p);
            std::size_t eq = entry.find('=');
            std::string key = (eq == std::string::npos) ? entry : entry.substr(0, eq);
            if (!key.empty() && (std::find(keys.begin(), keys.end(), key) == keys.end()))
            {
                keys.push_back(key);
            }
        }
        return keys;
    }

    std::vector<std::string> ListTotpUsers()
    {
        std::vector<std::string> users;

        // Credential Manager
        DWORD count{ 0U };
        PCREDENTIALW* creds = nullptr;
        if (::CredEnumerateW(nullptr, 0, &count, &creds) && creds)
        {
            for (DWORD i{ 0U }; i < count; ++i)
            {
                if (!creds[i] || !creds[i]->TargetName)
                {
                    continue;
                }
                std::string target = WcharToUtf8(creds[i]->TargetName);
                if (target.find(kCredPrefix) == 0)
                {
                    users.push_back(target.substr(kCredPrefixLen));
                }
            }
            ::CredFree(creds);
        }

        // INI fallback
        const char* iniPath = GetIniPath();
        if (iniPath && iniPath[0U])
        {
            std::array<char, 4096U> section{};
            if (::GetPrivateProfileSectionA("TOTP", section.data(), sizeof(section), iniPath) > 0)
            {
                for (const char* p = section.data(); *p; p += std::strlen(p) + 1)
                {
                    std::string entry(p);
                    std::size_t eq = entry.find('=');
                    std::string user = (eq == std::string::npos) ? entry : entry.substr(0, eq);
                    if (!user.empty() && (std::find(users.begin(), users.end(), user) == users.end()))
                    {
                        users.push_back(user);
                    }
                }
            }
        }

        return users;
    }

    // ---------------------------------------------------------------------------
    // TOTP URI
    // ---------------------------------------------------------------------------
    std::string BuildOtpAuthUri(const std::string& user, const std::string& secret, const std::string& issuer)
    {
        std::string uri = "otpauth://totp/";
        uri += UrlEncode(issuer);
        uri += ":";
        uri += UrlEncode(user);
        uri += "?secret=";
        uri += secret;
        uri += "&issuer=";
        uri += UrlEncode(issuer);
        return uri;
    }

    // ---------------------------------------------------------------------------
    // QR rendering
    // ---------------------------------------------------------------------------
    namespace
    {

#pragma pack(push, 1)
        struct BMPFILEHEADER
        {
            WORD bfType;
            DWORD bfSize;
            WORD bfReserved1;
            WORD bfReserved2;
            DWORD bfOffBits;
        };
        struct BMPINFOHEADER
        {
            DWORD biSize;
            LONG biWidth;
            LONG biHeight;
            WORD biPlanes;
            WORD biBitCount;
            DWORD biCompression;
            DWORD biSizeImage;
            LONG biXPelsPerMeter;
            LONG biYPelsPerMeter;
            DWORD biClrUsed;
            DWORD biClrImportant;
        };
#pragma pack(pop)

        static bool RenderQrPixels(const std::string& text, const int moduleScale, const int quietModules, LONG& widthOut,
                                   LONG& heightOut, int& rowSizeOut, std::vector<BYTE>& pixelsOut)
        {
            try
            {
                qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
                int modules = qr.getSize();
                const int imgSize = modules + 2 * quietModules;
                const LONG width = imgSize * moduleScale;
                const LONG height = width;
                int rowSize = ((width * 3 + 3) & ~3);
                int pixelArraySize = rowSize * height;

                pixelsOut.assign(pixelArraySize, 0xFF);

                auto setPixel = [&](const int x, const int y) {
                    int row = (height - 1 - y);
                    BYTE* p = pixelsOut + row * rowSize + x * 3;
                    p[0U] = 0;
                    p[1U] = 0;
                    p[2U] = 0;
                };

                for (int qrY = -quietModules; qrY < modules + quietModules; ++qrY)
                {
                    for (int qrX = -quietModules; qrX < modules + quietModules; ++qrX)
                    {
                        bool dark{ false };
                        if ((qrX >= 0) && (qrX < modules) && (qrY >= 0) && (qrY < modules))
                        {
                            dark = qr.getModule(qrX, qrY);
                        }

                        if (dark)
                        {
                            int startX = (qrX + quietModules) * moduleScale;
                            int startY = (qrY + quietModules) * moduleScale;
                            for (int dy{ 0 }; dy < moduleScale; ++dy)
                            {
                                for (int dx{ 0 }; dx < moduleScale; ++dx)
                                {
                                    setPixel(startX + dx, startY + dy);
                                }
                            }
                        }
                    }
                }

                widthOut = width;
                heightOut = height;
                rowSizeOut = rowSize;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

    } // anonymous namespace

    bool WriteQrBmpToFile(const std::string& text, const std::string& filename, const int moduleScale, const int quietModules)
    {
        LONG width{ 0 }, height = 0;
        int rowSize{ 0 };
        std::vector<BYTE> pixels;
        if (!RenderQrPixels(text, moduleScale, quietModules, width, height, rowSize, pixels))
        {
            return false;
        }

        BMPFILEHEADER fh{};
        BMPINFOHEADER ih{};
        fh.bfType = 0x4D42; // 'BM'
        fh.bfSize = sizeof(fh) + sizeof(ih) + static_cast<DWORD>(pixels.size());
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        ih.biSize = sizeof(ih);
        ih.biWidth = width;
        ih.biHeight = height;
        ih.biPlanes = 1;
        ih.biBitCount = 24;
        ih.biCompression = BI_RGB;
        ih.biSizeImage = static_cast<DWORD>(pixels.size());

        HANDLE hFile = ::CreateFileA(filename.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD written{ 0U };
        ::WriteFile(hFile, &fh, sizeof(fh), &written, nullptr);
        ::WriteFile(hFile, &ih, sizeof(ih), &written, nullptr);
        ::WriteFile(hFile, pixels, static_cast<DWORD>(pixels.size()), &written, nullptr);
        ::CloseHandle(hFile);
        return true;
    }

    HBITMAP RenderQrBitmap(const HWND hwndReference, const std::string& text, const int moduleScale, const int quietModules)
    {
        LONG width{ 0 }, height = 0;
        int rowSize{ 0 };
        std::vector<BYTE> pixels;
        if (!RenderQrPixels(text, moduleScale, quietModules, width, height, rowSize, pixels))
        {
            return nullptr;
        }

        HDC hdcRef = hwndReference ? ::GetDC(hwndReference) : ::GetDC(nullptr);
        if (!hdcRef)
        {
            return nullptr;
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = height; // positive = bottom-up, matches our buffer
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = static_cast<DWORD>(pixels.size());

        HBITMAP hBmp = nullptr;
        HDC hdcMem = ::CreateCompatibleDC(hdcRef);
        if (hdcMem)
        {
            void* pBits = nullptr;
            hBmp = ::CreateDIBSection(hdcRef, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
            if (hBmp && pBits)
            {
                std::memcpy(pBits, pixels, pixels.size());
            }
            ::DeleteDC(hdcMem);
        }

        ::ReleaseDC(hwndReference ? hwndReference : nullptr, hdcRef);
        return hBmp;
    }

} // namespace TokenCore
