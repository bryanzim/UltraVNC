// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.

#include "stdhdrs.h"

#include "CredentialManager.h"

#include <wincred.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <wchar.h>

#pragma comment(lib, "advapi32.lib")

void CredentialManager::BuildTargetName(const wchar_t* const prefix, const wchar_t* const host, const int port,
                                        wchar_t* const target, const std::size_t targetSize)
{ ::_snwprintf_s(target, targetSize, _TRUNCATE, L"ultravnc:%s:%s:%d", prefix, host, port); }

static void BuildVncTargetName(const wchar_t* const host, const int port, wchar_t* const target, const std::size_t targetSize)
{ CredentialManager::BuildTargetName(L"vnc", host, port, target, targetSize); }

static void BuildMsLogonTargetName(const wchar_t* const host, const int port, wchar_t* const target, const std::size_t targetSize)
{ CredentialManager::BuildTargetName(L"mslogon", host, port, target, targetSize); }

bool CredentialManager::ReadVncPassword(const wchar_t* const host, const int port, char* const outPassword,
                                        const std::size_t outPasswordSize)
{
    if (!host || !outPassword || (outPasswordSize == 0))
    {
        return false;
    }

    std::array<wchar_t, CRED_MAX_GENERIC_TARGET_NAME_LENGTH> target;
    BuildVncTargetName(host, port, target.data(), target.size());

    PCREDENTIALW pcred = nullptr;
    if (!::CredReadW(target.data(), CRED_TYPE_GENERIC, 0, &pcred))
    {
        return false;
    }

    bool ok{ false };
    if (pcred && (pcred->CredentialBlobSize > 0) && pcred->CredentialBlob)
    {
        // CredentialBlob is a byte array; treat as ANSI string for VNC passwords.
        std::size_t len = pcred->CredentialBlobSize;
        if (len < outPasswordSize)
        {
            std::memcpy(outPassword, pcred->CredentialBlob, len);
            outPassword[len] = '\0';
            ok = true;
        }
    }

    if (pcred)
    {
        ::CredFree(pcred);
    }

    return ok;
}

bool CredentialManager::WriteVncPassword(const wchar_t* const host, const int port, const char* const password)
{
    if (!host || !password)
    {
        return false;
    }

    std::array<wchar_t, CRED_MAX_GENERIC_TARGET_NAME_LENGTH> target;
    BuildVncTargetName(host, port, target.data(), target.size());

    std::size_t pwLen = std::strlen(password);
    if ((pwLen == 0) || (pwLen > 256))
    {
        return false;
    }

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = target.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.CredentialBlobSize = static_cast<DWORD>(pwLen);
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(password));
    cred.UserName = L"UltraVNC";

    return (::CredWriteW(&cred, 0) != FALSE);
}

bool CredentialManager::DeleteVncPassword(const wchar_t* const host, const int port)
{
    if (!host)
    {
        return false;
    }

    std::array<wchar_t, CRED_MAX_GENERIC_TARGET_NAME_LENGTH> target;
    BuildVncTargetName(host, port, target.data(), target.size());

    return (::CredDeleteW(target.data(), CRED_TYPE_GENERIC, 0) != FALSE);
}

bool CredentialManager::ReadMsLogonPassword(const wchar_t* const host, const int port, char* const outDomain,
                                            const std::size_t domainSize, char* const outUser, const std::size_t userSize,
                                            char* const outPassword, const std::size_t passwordSize)
{
    if (!host || !outDomain || (domainSize == 0) || !outUser || (userSize == 0) || !outPassword || (passwordSize == 0))
    {
        return false;
    }

    outDomain[0U] = '\0';
    outUser[0U] = '\0';
    outPassword[0U] = '\0';

    std::array<wchar_t, CRED_MAX_GENERIC_TARGET_NAME_LENGTH> target;
    BuildMsLogonTargetName(host, port, target.data(), target.size());

    PCREDENTIALW pcred = nullptr;
    if (!::CredReadW(target.data(), CRED_TYPE_GENERIC, 0, &pcred))
    {
        return false;
    }

    bool ok{ false };
    if (pcred && (pcred->CredentialBlobSize > 0) && pcred->CredentialBlob)
    {
        // Copy password from the credential blob.
        std::size_t len = pcred->CredentialBlobSize;
        if (len < passwordSize)
        {
            std::memcpy(outPassword, pcred->CredentialBlob, len);
            outPassword[len] = '\0';
            ok = true;
        }

        // Parse domain\user from the UserName field.
        if (ok && pcred->UserName)
        {
            std::array<char, CRED_MAX_USERNAME_LENGTH> userNameA;
            std::size_t converted{ 0U };
            if (::wcstombs_s(&converted, userNameA.data(), sizeof(userNameA), pcred->UserName, _TRUNCATE) == 0)
            {
                const char* sep = std::strchr(userNameA.data(), '\\');
                if (sep)
                {
                    std::size_t dLen = static_cast<std::size_t>(sep - userNameA.data());
                    std::size_t uLen = std::strlen(sep + 1);
                    if ((dLen < domainSize) && (uLen < userSize))
                    {
                        std::memcpy(outDomain, userNameA.data(), dLen);
                        outDomain[dLen] = '\0';
                        std::memcpy(outUser, sep + 1, uLen);
                        outUser[uLen] = '\0';
                    }
                    else
                    {
                        ok = false;
                    }
                }
                else
                {
                    if (::strncpy_s(outUser, userSize, userNameA.data(), _TRUNCATE) != 0)
                    {
                        ok = false;
                    }
                }
            }
            else
            {
                ok = false;
            }
        }
    }

    if (pcred)
    {
        ::CredFree(pcred);
    }

    return ok;
}

bool CredentialManager::WriteMsLogonPassword(const wchar_t* const host, const int port, const char* const domain,
                                             const char* const user, const char* const password)
{
    if (!host || !domain || !user || !password)
    {
        return false;
    }

    std::array<wchar_t, CRED_MAX_GENERIC_TARGET_NAME_LENGTH> target;
    BuildMsLogonTargetName(host, port, target.data(), target.size());

    std::size_t dLen = std::strlen(domain);
    std::size_t uLen = std::strlen(user);
    std::size_t pLen = std::strlen(password);
    if ((dLen > 256) || (uLen > 256) || (pLen > 256))
    {
        return false;
    }

    // Build UserName as "domain\user" so it shows up in Credential Manager UI.
    std::array<char, CRED_MAX_USERNAME_LENGTH> userNameA;
    if (dLen > 0)
    {
        ::_snprintf_s(userNameA.data(), sizeof(userNameA), _TRUNCATE, "%s\\%s", domain, user);
    }
    else
    {
        ::_snprintf_s(userNameA.data(), sizeof(userNameA), _TRUNCATE, "%s", user);
    }

    std::array<wchar_t, CRED_MAX_USERNAME_LENGTH> userNameW;
    if (::mbstowcs_s(nullptr, userNameW.data(), userNameW.size(), userNameA.data(), _TRUNCATE) != 0)
    {
        return false;
    }

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = target.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.CredentialBlobSize = static_cast<DWORD>(pLen);
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(password));
    cred.UserName = userNameW.data();

    return (::CredWriteW(&cred, 0) != FALSE);
}

bool CredentialManager::DeleteMsLogonPassword(const wchar_t* const host, const int port)
{
    if (!host)
    {
        return false;
    }

    std::array<wchar_t, CRED_MAX_GENERIC_TARGET_NAME_LENGTH> target;
    BuildMsLogonTargetName(host, port, target.data(), target.size());

    return (::CredDeleteW(target.data(), CRED_TYPE_GENERIC, 0) != FALSE);
}
