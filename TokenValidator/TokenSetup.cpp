// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//

// TokenSetup.exe
//
// Registers a user for UltraVNC VeNCrypt TOTP authentication.
//
// Usage:
//   TokenSetup.exe <username> [base32-secret]
//
// If the secret is omitted, a random 160-bit base32 secret is generated.
// The tool stores the secret in Windows Credential Manager and in a
// DPAPI-encrypted INI file next to TokenValidator.dll/TokenSetup.exe so the
// WinVNC token validation DLL can read it even when running as SYSTEM.
// It also emits an otpauth:// provisioning URI and a QR-code BMP image that
// can be scanned by Microsoft Authenticator, Google Authenticator, Authy, etc.

#include "../common/ScramSha256.h"
#include "TokenCore.h"

#include <windows.h>

#include <conio.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>

static bool SetStoragePathsFromExeDir()
{
    std::array<char, MAX_PATH> exePath{};
    if (!::GetModuleFileNameA(nullptr, exePath.data(), exePath.size()))
    {
        return false;
    }

    char* lastSlash = std::strrchr(exePath.data(), '\\');
    if (!lastSlash)
    {
        return false;
    }

    // Prefer the matching TokenValidator DLL next to us so the DLL and this
    // tool share the same INI fallback.
    std::array<char, MAX_PATH> baseName{};
    ::strncpy_s(baseName.data(), sizeof(baseName), exePath.data(), lastSlash - exePath.data() + 1);

    for (const char* dllName : { "TokenValidator.dll", "TokenValidator32.dll" })
    {
        std::array<char, MAX_PATH> candidate{};
        ::sprintf_s(candidate.data(), candidate.size(), "%s%s", baseName.data(), dllName);
        if (::GetFileAttributesA(candidate.data()) != INVALID_FILE_ATTRIBUTES)
        {
            TokenCore::SetSecretStorePathsFromDllPath(candidate.data());
            return true;
        }
    }

    // No DLL found: use the INI next to this executable.
    TokenCore::SetSecretStorePathsFromDllPath(exePath.data());
    return true;
}

int main(const int argc, char* argv[])
{
    if (argc < 2)
    {
        std::fprintf(stderr, "Usage: %s <username> [base32-secret]\n", argv[0U]);
        std::fprintf(stderr, "       %s -password <username> [password]   (provision SCRAM-SHA-256)\n", argv[0U]);
        std::fprintf(stderr, "       %s list                                  (list users + credential types)\n", argv[0U]);
        std::fprintf(stderr, "       %s delete <username>                   (remove all credentials)\n", argv[0U]);
        std::fprintf(stderr, "If the base32 secret is omitted, a random 160-bit secret is generated.\n");
        return 1;
    }

    SetStoragePathsFromExeDir();

    // ---------------- list all users and their credential types ----------------
    if (std::strcmp(argv[1U], "list") == 0)
    {
        std::vector<std::string> totp = TokenCore::ListTotpUsers();
        std::vector<std::string> scram = TokenCore::ListProtectedKeys("SCRAM");
        std::vector<std::string> sess = TokenCore::ListProtectedKeys("SessionTokens");

        std::vector<std::string> all = totp;
        for (const auto& u : scram)
        {
            if (std::find(all.begin(), all.end(), u) == all.end())
            {
                all.push_back(u);
            }
        }
        for (const auto& u : sess)
        {
            if (std::find(all.begin(), all.end(), u) == all.end())
            {
                all.push_back(u);
            }
        }
        std::sort(all.begin(), all.end());

        if (all.empty())
        {
            std::printf("No users provisioned.\n");
        }
        for (const auto& u : all)
        {
            std::string factors;
            if (std::find(totp.begin(), totp.end(), u) != totp.end())
            {
                factors += " TOTP";
            }
            if (std::find(scram.begin(), scram.end(), u) != scram.end())
            {
                factors += " SCRAM";
            }
            if (std::find(sess.begin(), sess.end(), u) != sess.end())
            {
                factors += " SESSION(pending)";
            }
            std::printf("%-24s %s\n", u.c_str(), factors.c_str());
        }
        std::printf("INI: %s\n", TokenCore::GetSecretStoreIniPath().c_str());
        return 0;
    }

    // ---------------- delete a user across all credential types ----------------
    if (std::strcmp(argv[1U], "delete") == 0)
    {
        if (argc < 3)
        {
            std::fprintf(stderr, "Usage: %s delete <username>\n", argv[0U]);
            return 1;
        }
        const char* user = argv[2U];
        TokenCore::DeleteTotpSecret(user);
        TokenCore::DeleteProtectedValue("SCRAM", user);
        TokenCore::DeleteProtectedValue("SessionTokens", user);
        TokenCore::DeleteProtectedValue("TotpLastStep", user);
        std::printf("Removed TOTP, SCRAM and session credentials for '%s'\n", user);
        return 0;
    }

    // ---------------- SCRAM-SHA-256 password provisioning ----------------
    if (std::strcmp(argv[1U], "-password") == 0)
    {
        if (argc < 3)
        {
            std::fprintf(stderr, "Usage: %s -password <username> [password]\n", argv[0U]);
            return 1;
        }
        const char* user = argv[2U];
        std::string password;
        if (argc >= 4)
        {
            password = argv[3U];
        }
        else
        {
            // Read without echo.
            std::fprintf(stdout, "Password for %s: ", user);
            std::fflush(stdout);
            int c;
            while (((c = _getch()) != '\r') && (c != '\n') && (c != EOF))
            {
                password.push_back(static_cast<char>(c));
            }
            std::fprintf(stdout, "\n");
        }
        if (password.empty())
        {
            std::fprintf(stderr, "Empty password\n");
            return 1;
        }
        ScramSha256::Verifier v;
        if (!ScramSha256::MakeVerifier(password, v))
        {
            std::fprintf(stderr, "Failed to derive SCRAM verifier\n");
            return 1;
        }
        if (!TokenCore::WriteProtectedValue("SCRAM", user, ScramSha256::VerifierToString(v)))
        {
            std::fprintf(stderr, "Failed to store SCRAM verifier\n");
            return 1;
        }
        std::printf("SCRAM-SHA-256 verifier stored for user '%s' (%u iterations)\n", user, static_cast<unsigned>(v.iterations));
        std::printf("INI: %s\n", TokenCore::GetSecretStoreIniPath().c_str());
        return 0;
    }


    const char* user = argv[1U];
    std::string secretBase32;

    if (argc >= 3)
    {
        secretBase32 = argv[2U];
        std::string cleaned;
        for (char c : secretBase32)
        {
            if ((c == ' ') || (c == '-'))
            {
                continue;
            }
            cleaned.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        secretBase32 = cleaned;
    }
    else
    {
        if (!TokenCore::GenerateRandomBase32Secret(secretBase32))
        {
            std::fprintf(stderr, "Failed to generate random secret\n");
            return 1;
        }
    }

    if (!TokenCore::ValidateBase32(secretBase32))
    {
        std::fprintf(stderr, "Invalid base32 secret (length must be a multiple of 8, A-Z2-7)\n");
        return 1;
    }

    if (!TokenCore::WriteTotpSecret(user, secretBase32))
    {
        std::fprintf(stderr, "Warning: failed to store secret (Credential Manager or INI fallback)\n");
    }

    std::string uri = TokenCore::BuildOtpAuthUri(user, secretBase32);

    // Write QR image next to this executable.
    std::array<char, MAX_PATH> bmpPath{};
    if (::GetModuleFileNameA(nullptr, bmpPath.data(), bmpPath.size()))
    {
        char* dot = std::strrchr(bmpPath.data(), '.');
        if (dot)
        {
            std::array<char, 256U> suffix{};
            ::sprintf_s(suffix.data(), suffix.size(), "_%s_totp.bmp", user);
            ::strcpy_s(dot, MAX_PATH - (dot - bmpPath.data()), suffix.data());
        }
    }
    if (!bmpPath[0U])
    {
        ::sprintf_s(bmpPath.data(), bmpPath.size(), "%s_totp.bmp", user);
    }

    if (!TokenCore::WriteQrBmpToFile(uri, bmpPath.data(), 6, 4))
    {
        std::fprintf(stderr, "Warning: failed to write QR code image\n");
        bmpPath[0U] = '\0';
    }

    std::printf("User: %s\n", user);
    std::printf("Base32 secret: %s\n", secretBase32.c_str());
    std::printf("Provisioning URI: %s\n", uri.c_str());
    if (bmpPath[0U])
    {
        std::printf("QR code image: %s\n", bmpPath.data());
    }
    std::printf("Credential Manager target: UltraVNC_TOTP_%s\n", user);
    std::printf("INI fallback: %s\n", TokenCore::GetSecretStoreIniPath().c_str());
    return 0;
}
