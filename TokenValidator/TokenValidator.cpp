// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//

// Native token validation DLL for UltraVNC VeNCrypt token auth.
//
// Supports three credential types:
//   1. TOTP (RFC 6238) codes from authenticator apps such as Microsoft
//      Authenticator, Google Authenticator, etc.
//   2. Server-issued 64-character session tokens for reconnects.
//   3. SCRAM-SHA-256 verifiers for SASL auth (VeNCrypt subtype 263).
//
// Exports:
//   validate_token(user, token)
//   tv_has_management_ui()
//   tv_show_management_ui(HWND parent)
//
// TOTP secrets are read from Windows Credential Manager first (target name
// "UltraVNC_TOTP_<user>"), and fall back to a DPAPI-encrypted INI file next to
// this DLL (<this DLL>.credentials.ini) for service accounts such as SYSTEM whose
// Credential Manager vault is different from the interactive admin's vault.

#include "../common/ScramSha256.h"
#include "TokenCore.h"
#include "TokenRes.h"
#include "TokenValidatorPlugin.h"

#include <Windows.h>

#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <time.h>
#include <time.h>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{

    constexpr std::size_t SHA1_DIGEST_LEN{ 20U };

    // ---------------------------------------------------------------------------
    // HMAC-SHA1 using Windows CNG (BCrypt)
    // ---------------------------------------------------------------------------
    bool hmacSha1(const BYTE* const key, const std::size_t keyLen, const BYTE* const msg, const std::size_t msgLen,
                  BYTE out[SHA1_DIGEST_LEN])
    {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        NTSTATUS status;

        status = ::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (!BCRYPT_SUCCESS(status))
        {
            return false;
        }

        status = ::BCryptCreateHash(hAlg, &hHash, nullptr, 0, const_cast<PUCHAR>(key), static_cast<ULONG>(keyLen), 0);
        if (!BCRYPT_SUCCESS(status))
        {
            ::BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        status = ::BCryptHashData(hHash, const_cast<PUCHAR>(msg), static_cast<ULONG>(msgLen), 0);
        if (BCRYPT_SUCCESS(status))
        {
            status = ::BCryptFinishHash(hHash, out, SHA1_DIGEST_LEN, 0);
        }

        ::BCryptDestroyHash(hHash);
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return BCRYPT_SUCCESS(status);
    }

    // ---------------------------------------------------------------------------
    // RFC 6238 TOTP helpers
    // ---------------------------------------------------------------------------
    std::uint64_t currentUnixTime()
    {
        FILETIME ft;
        ::GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER u;
        u.LowPart = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return (u.QuadPart - 116444736000000000ULL) / 10000000ULL;
    }

    std::uint32_t hotp(const BYTE* const secret, const std::size_t secretLen, std::uint64_t counter)
    {
        std::array<BYTE, 8U> counterBE;
        for (int i{ 7 }; i >= 0; --i)
        {
            counterBE[i] = static_cast<BYTE>(counter & 0xFF);
            counter >>= 8;
        }

        std::array<BYTE, SHA1_DIGEST_LEN> digest;
        if (!hmacSha1(secret, secretLen, counterBE.data(), sizeof(counterBE), digest.data()))
        {
            return 0xFFFFFFFF;
        }

        const int offset = digest[SHA1_DIGEST_LEN - 1] & 0x0F;
        std::uint32_t binary =
            ((static_cast<std::uint32_t>(digest[offset] & 0x7F) << 24) | (static_cast<std::uint32_t>(digest[offset + 1]) << 16) |
             (static_cast<std::uint32_t>(digest[offset + 2]) << 8) | (static_cast<std::uint32_t>(digest[offset + 3])));
        return binary % 1000000;
    }

    constexpr const char* kTotpLastStepSection = "TotpLastStep";
    std::mutex g_totpMutex;

    bool validateTotp(const char* const user, const char* const code)
    {
        std::size_t codeLen = std::strlen(code);
        if ((codeLen < 6) || (codeLen > 8))
        {
            return false;
        }
        for (std::size_t i{ 0U }; i < codeLen; ++i)
        {
            if ((code[i] < '0') || (code[i] >)'9')
            {
                return false;
            }
        }

        std::string secretBase32;
        if (!TokenCore::ReadTotpSecret(user, secretBase32) || secretBase32.empty())
        {
            return false;
        }

        std::vector<BYTE> secret;
        if (!TokenCore::Base32Decode(secretBase32, secret) || secret.empty())
        {
            return false;
        }

        std::uint64_t t = currentUnixTime() / 30;
        std::uint32_t expected = static_cast<std::uint32_t>(atoi(code));

        // Allow current, previous and next time step to tolerate clock skew.
        std::uint64_t matchedStep{ 0U };
        bool matched{ false };
        for (std::int64_t delta = -1; delta <= 1; ++delta)
        {
            std::uint64_t step = t + delta;
            if (hotp(secret, secret.size(), step) == expected)
            {
                matchedStep = step;
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(g_totpMutex);
        std::string lastText;
        if (TokenCore::ReadProtectedValue(kTotpLastStepSection, user, lastText))
        {
            std::uint64_t lastStep = _strtoui64(lastText.c_str(), nullptr, 10);
            if (matchedStep <= lastStep)
            {
                return false;
            }
        }
        return TokenCore::WriteProtectedValue(kTotpLastStepSection, user, std::to_string(matchedStep));
    }

    bool isAllDigits(const char* const s)
    {
        if (!s || !*s)
        {
            return false;
        }
        for (const char* p = s; *p; ++p)
        {
            if ((*p < '0') || (*p >)'9')
            {
                return false;
            }
        }
        return true;
    }

    // ---------------------------------------------------------------------------
    // Management dialog
    // ---------------------------------------------------------------------------
    HINSTANCE g_hInst = nullptr;
    HBITMAP g_hQrBitmap = nullptr;

    void SetQrImage(const HWND hDlg, const std::string& uri)
    {
        HWND hStatic = ::GetDlgItem(hDlg, IDC_TV_QR_IMAGE);
        if (!hStatic)
        {
            return;
        }

        HBITMAP hBmp = TokenCore::RenderQrBitmap(hDlg, uri, 5, 4);
        if (!hBmp)
        {
            return;
        }

        HBITMAP hOld = (HBITMAP)::SendMessageA(hStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
        if (hOld && (hOld != hBmp))
        {
            ::DeleteObject(hOld);
        }

        if (g_hQrBitmap && (g_hQrBitmap != hOld) && (g_hQrBitmap != hBmp))
        {
            ::DeleteObject(g_hQrBitmap);
        }

        g_hQrBitmap = hBmp;
    }

    void UpdateUriAndQr(const HWND hDlg, const std::string& user, const std::string& secret)
    {
        std::string uri = TokenCore::BuildOtpAuthUri(user, secret);
        ::SetDlgItemTextA(hDlg, IDC_TV_URI, uri.c_str());
        SetQrImage(hDlg, uri);
    }

    void RefreshUserList(const HWND hDlg)
    {
        HWND hList = ::GetDlgItem(hDlg, IDC_TV_USER_LIST);
        if (!hList)
        {
            return;
        }

        int prevSel = (int)::SendMessageA(hList, LB_GETCURSEL, 0, 0);
        std::array<char, 256U> prevUser{};
        if (prevSel != LB_ERR)
        {
            ::SendMessageA(hList, LB_GETTEXT, prevSel, (LPARAM)prevUser.data());
        }

        ::SendMessageA(hList, LB_RESETCONTENT, 0, 0);

        // Union of TOTP (CredMan + INI) and SCRAM (INI) users, with a factor badge.
        std::vector<std::string> totpUsers = TokenCore::ListTotpUsers();
        std::vector<std::string> scramUsers = TokenCore::ListProtectedKeys("SCRAM");

        std::vector<std::string> all = totpUsers;
        for (const auto& s : scramUsers)
        {
            if (std::find(all.begin(), all.end(), s) == all.end())
            {
                all.push_back(s);
            }
        }

        for (const auto& u : all)
        {
            bool hasTotp = std::find(totpUsers.begin(), totpUsers.end(), u) != totpUsers.end();
            bool hasScram = std::find(scramUsers.begin(), scramUsers.end(), u) != scramUsers.end();
            std::string display = u;
            if (hasTotp && hasScram)
            {
                display += " [TOTP+SCRAM]";
            }
            else if (hasTotp)
            {
                display += " [TOTP]";
            }
            else if (hasScram)
            {
                display += " [SCRAM]";
            }

            int idx = (int)::SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
            if ((idx != LB_ERR) && (std::strcmp(u.c_str(), prevUser.data()) == 0))
            {
                ::SendMessageA(hList, LB_SETCURSEL, idx, 0);
            }
        }
    }

    // Strip display decorations (" [TOTP+SCRAM]", " (static fallback)") from a
    // listbox string, leaving the bare username.
    void StripListName(char* const name)
    {
        char* p = strstr(name, " [");
        char* q = strstr(name, " (");
        char* cut = (p && q) ? (p < q ? p : q) : (p ? p : q);
        if (cut)
        {
            *cut = '\0';
        }
    }

    void UpdateFactorsLabel(const HWND hDlg, const char* const user)
    {
        if (!user || !*user)
        {
            ::SetDlgItemTextA(hDlg, IDC_TV_FACTORS, "");
            return;
        }
        bool hasTotp{ false }, hasScram = false;
        std::string tmp;
        std::vector<std::string> t = TokenCore::ListTotpUsers();
        hasTotp = std::find(t.begin(), t.end(), user) != t.end();
        hasScram = TokenCore::ReadProtectedValue("SCRAM", user, tmp) && !tmp.empty();

        std::string label = "Factors: ";
        if (hasTotp)
        {
            label += "TOTP ";
        }
        if (hasScram)
        {
            label += "SCRAM";
        }
        if (!hasTotp && !hasScram)
        {
            label += "none";
        }
        ::SetDlgItemTextA(hDlg, IDC_TV_FACTORS, label.c_str());
    }

    void OnUserSelectionChanged(const HWND hDlg)
    {
        HWND hList = ::GetDlgItem(hDlg, IDC_TV_USER_LIST);
        if (!hList)
        {
            return;
        }

        int idx = (int)::SendMessageA(hList, LB_GETCURSEL, 0, 0);
        if (idx == LB_ERR)
        {
            ::SetDlgItemTextA(hDlg, IDC_TV_URI, "");
            UpdateFactorsLabel(hDlg, "");
            return;
        }

        std::array<char, 256U> user{};
        ::SendMessageA(hList, LB_GETTEXT, idx, (LPARAM)user.data());
        StripListName(user.data());

        UpdateFactorsLabel(hDlg, user.data());

        std::string secret;
        if (TokenCore::ReadTotpSecret(user.data(), secret) && !secret.empty())
        {
            UpdateUriAndQr(hDlg, user.data(), secret);
        }
        else
        {
            ::SetDlgItemTextA(hDlg, IDC_TV_URI, "");
        }
    }

    bool GetSelectedUser(const HWND hDlg, char* const user, const std::size_t userSize)
    {
        HWND hList = ::GetDlgItem(hDlg, IDC_TV_USER_LIST);
        if (!hList)
        {
            return false;
        }

        int idx = (int)::SendMessageA(hList, LB_GETCURSEL, 0, 0);
        if (idx == LB_ERR)
        {
            return false;
        }

        ::SendMessageA(hList, LB_GETTEXT, idx, (LPARAM)user);
        if (strstr(user, " (static fallback)"))
        {
            return false;
        } // read-only static fallback
        StripListName(user);

        return user[0U] != '\0';
    }

    void OnAddUser(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        ::GetDlgItemTextA(hDlg, IDC_TV_USERNAME, user.data(), sizeof(user));

        // Trim leading/trailing spaces.
        char* start = user.data();
        while ((*start == ' ') || (*start == '\t'))
        {
            ++start;
        }
        char* end = user.data() + std::strlen(user.data());
        while ((end > start) && ((*(end - 1) == ' ') || (*(end - 1) == '\t')))
        {
            --end;
        }
        *end = '\0';
        if (start != user.data())
        {
            std::memmove(user.data(), start, end - start + 1);
        }

        if (std::strlen(user.data()) == 0)
        {
            ::MessageBoxA(hDlg, "Enter a username.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::string secret;
        if (!TokenCore::GenerateRandomBase32Secret(secret))
        {
            ::MessageBoxA(hDlg, "Failed to generate a new secret.", "Token Manager", MB_OK | MB_ICONERROR);
            return;
        }

        if (!TokenCore::WriteTotpSecret(user.data(), secret))
        {
            ::MessageBoxA(hDlg,
                        "Failed to save the secret.\nMake sure the account running the dialog can write to Credential Manager and "
                        "the INI fallback.",
                        "Token Manager", MB_OK | MB_ICONERROR);
            return;
        }

        RefreshUserList(hDlg);

        // Select the new user.
        HWND hList = ::GetDlgItem(hDlg, IDC_TV_USER_LIST);
        const int count = (int)::SendMessageA(hList, LB_GETCOUNT, 0, 0);
        for (int i{ 0 }; i < count; ++i)
        {
            std::array<char, 256U> item{};
            ::SendMessageA(hList, LB_GETTEXT, i, (LPARAM)item.data());
            if (std::strcmp(item.data(), user.data()) == 0)
            {
                ::SendMessageA(hList, LB_SETCURSEL, i, 0);
                break;
            }
        }

        UpdateUriAndQr(hDlg, user.data(), secret);
        ::SetDlgItemTextA(hDlg, IDC_TV_USERNAME, "");

        // Also write a BMP file next to the INI for the administrator's convenience.
        std::array<char, MAX_PATH> tempPath{};
        if (::GetTempPathA(MAX_PATH, tempPath.data()))
        {
            std::array<char, MAX_PATH> bmpPath{};
            ::sprintf_s(bmpPath.data(), bmpPath.size(), "%s%s_uvnc_totp.bmp", tempPath.data(), user.data());
            TokenCore::WriteQrBmpToFile(TokenCore::BuildOtpAuthUri(user.data(), secret), bmpPath.data(), 6, 4);
        }
    }

    void OnRemoveUser(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        if (!GetSelectedUser(hDlg, user.data(), sizeof(user)))
        {
            ::MessageBoxA(hDlg, "Select a user to remove.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::array<char, 300U> msg{};
        ::sprintf_s(msg.data(), msg.size(), "Remove user '%s'? This deletes their TOTP secret, SCRAM verifier and pending session token.", user.data());
        if (::MessageBoxA(hDlg, msg.data(), "Token Manager", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }

        TokenCore::DeleteTotpSecret(user.data());
        TokenCore::DeleteProtectedValue("SCRAM", user.data());
        TokenCore::DeleteProtectedValue("SessionTokens", user.data());
        TokenCore::DeleteProtectedValue("TotpLastStep", user.data());
        ::SetDlgItemTextA(hDlg, IDC_TV_URI, "");
        UpdateFactorsLabel(hDlg, "");
        RefreshUserList(hDlg);
    }

    void OnSetPassword(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        if (!GetSelectedUser(hDlg, user.data(), sizeof(user)))
        {
            ::MessageBoxA(hDlg, "Select a user first.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::array<char, 256U> password{};
        ::GetDlgItemTextA(hDlg, IDC_TV_PASSWORD, password.data(), sizeof(password));
        if (!password[0U])
        {
            ::MessageBoxA(hDlg, "Enter a password in the Password field.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        ScramSha256::Verifier v;
        if (!ScramSha256::MakeVerifier(password.data(), v) ||
            !TokenCore::WriteProtectedValue("SCRAM", user.data(), ScramSha256::VerifierToString(v)))
        {
            ::MessageBoxA(hDlg, "Failed to store the SCRAM verifier.", "Token Manager", MB_OK | MB_ICONERROR);
            return;
        }

        // Never keep the plaintext around longer than needed.
        ::SecureZeroMemory(password.data(), sizeof(password));
        ::SetDlgItemTextA(hDlg, IDC_TV_PASSWORD, "");

        ::MessageBoxA(hDlg, "SCRAM password stored. The plaintext is not saved — only the verifier.", "Token Manager",
                    MB_OK | MB_ICONINFORMATION);
        RefreshUserList(hDlg);
        UpdateFactorsLabel(hDlg, user.data());
    }

    void OnTestPassword(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        if (!GetSelectedUser(hDlg, user.data(), sizeof(user)))
        {
            ::MessageBoxA(hDlg, "Select a user first.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::string text;
        ScramSha256::Verifier v;
        if (!TokenCore::ReadProtectedValue("SCRAM", user.data(), text) || !ScramSha256::ParseVerifier(text, v))
        {
            ::MessageBoxA(hDlg, "No SCRAM credential stored for this user.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::array<char, 256U> password{};
        ::GetDlgItemTextA(hDlg, IDC_TV_PASSWORD, password.data(), sizeof(password));
        if (!password[0U])
        {
            ::MessageBoxA(hDlg, "Enter the password to test in the Password field.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        bool ok = ScramSha256::CheckPassword(password.data(), v);
        ::SecureZeroMemory(password.data(), sizeof(password));
        ::SetDlgItemTextA(hDlg, IDC_TV_PASSWORD, "");

        ::MessageBoxA(hDlg, ok ? "Password matches the stored verifier." : "Password does NOT match.", "Token Manager",
                    ok ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONWARNING);
    }

    void OnClearPassword(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        if (!GetSelectedUser(hDlg, user.data(), sizeof(user)))
        {
            ::MessageBoxA(hDlg, "Select a user first.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::array<char, 300U> msg{};
        ::sprintf_s(msg.data(), msg.size(), "Remove the SCRAM password for '%s'? SASL authentication will stop working for this user.", user.data());
        if (::MessageBoxA(hDlg, msg.data(), "Token Manager", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }

        TokenCore::DeleteProtectedValue("SCRAM", user.data());
        RefreshUserList(hDlg);
        UpdateFactorsLabel(hDlg, user.data());
    }

    void OnClearTotp(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        if (!GetSelectedUser(hDlg, user.data(), sizeof(user)))
        {
            ::MessageBoxA(hDlg, "Select a user first.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::vector<std::string> users = TokenCore::ListTotpUsers();
        if (std::find(users.begin(), users.end(), user.data()) == users.end())
        {
            ::MessageBoxA(hDlg, "No TOTP credential is stored for this user.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::array<char, 300U> msg{};
        ::sprintf_s(msg.data(), msg.size(), "Remove only the TOTP credential for '%s'? SCRAM and session credentials are preserved.", user.data());
        if (::MessageBoxA(hDlg, msg.data(), "Token Manager", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }

        if (!TokenCore::DeleteTotpSecret(user.data()))
        {
            ::MessageBoxA(hDlg, "Failed to remove the TOTP credential.", "Token Manager", MB_OK | MB_ICONERROR);
            return;
        }

        ::SetDlgItemTextA(hDlg, IDC_TV_URI, "");
        RefreshUserList(hDlg);

        HWND hList = ::GetDlgItem(hDlg, IDC_TV_USER_LIST);
        const int count = (int)::SendMessageA(hList, LB_GETCOUNT, 0, 0);
        for (int i{ 0 }; i < count; ++i)
        {
            std::array<char, 256U> item{};
            ::SendMessageA(hList, LB_GETTEXT, i, (LPARAM)item.data());
            StripListName(item.data());
            if (std::strcmp(item.data(), user.data()) == 0)
            {
                ::SendMessageA(hList, LB_SETCURSEL, i, 0);
                break;
            }
        }
        UpdateFactorsLabel(hDlg, user.data());
    }

    void OnResetUser(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        if (!GetSelectedUser(hDlg, user.data(), sizeof(user)))
        {
            ::MessageBoxA(hDlg, "Select a user to reset.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::array<char, 300U> msg{};
        ::sprintf_s(msg.data(), msg.size(), "Reset the secret for '%s'? The old QR code will stop working.", user.data());
        if (::MessageBoxA(hDlg, msg.data(), "Token Manager", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }

        std::string secret;
        if (!TokenCore::GenerateRandomBase32Secret(secret))
        {
            ::MessageBoxA(hDlg, "Failed to generate a new secret.", "Token Manager", MB_OK | MB_ICONERROR);
            return;
        }

        if (!TokenCore::WriteTotpSecret(user.data(), secret))
        {
            ::MessageBoxA(hDlg, "Failed to save the new secret.", "Token Manager", MB_OK | MB_ICONERROR);
            return;
        }

        UpdateUriAndQr(hDlg, user.data(), secret);

        std::array<char, MAX_PATH> tempPath{};
        if (::GetTempPathA(MAX_PATH, tempPath.data()))
        {
            std::array<char, MAX_PATH> bmpPath{};
            ::sprintf_s(bmpPath.data(), bmpPath.size(), "%s%s_uvnc_totp.bmp", tempPath.data(), user.data());
            TokenCore::WriteQrBmpToFile(TokenCore::BuildOtpAuthUri(user.data(), secret), bmpPath.data(), 6, 4);
        }
    }

    void OnShowQr(const HWND hDlg)
    {
        std::array<char, 256U> user{};
        if (!GetSelectedUser(hDlg, user.data(), sizeof(user)))
        {
            ::MessageBoxA(hDlg, "Select a user to show the QR code.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        std::string secret;
        if (!TokenCore::ReadTotpSecret(user.data(), secret) || secret.empty())
        {
            ::MessageBoxA(hDlg, "No stored secret found for this user.", "Token Manager", MB_OK | MB_ICONWARNING);
            return;
        }

        UpdateUriAndQr(hDlg, user.data(), secret);
    }

    INT_PTR CALLBACK TokenManagerDlgProc(const HWND hDlg, const UINT message, const WPARAM wParam, const LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG: {
            // Title-bar icon: the UltraVNC icon embedded in this DLL.
            HICON hIcon = (HICON)::LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_TOKENMANAGER), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                            GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
            if (hIcon)
            {
                ::SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }
            HICON hIconBig = (HICON)::LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_TOKENMANAGER), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                               GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
            if (hIconBig)
            {
                ::SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
            }
            RefreshUserList(hDlg);
            ::SetDlgItemTextA(hDlg, IDC_TV_URI, "");
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_TV_USER_LIST:
                if (HIWORD(wParam) == LBN_SELCHANGE)
                {
                    OnUserSelectionChanged(hDlg);
                }
                return (INT_PTR)TRUE;

            case IDC_TV_ADD:
                OnAddUser(hDlg);
                return (INT_PTR)TRUE;

            case IDC_TV_REMOVE:
                OnRemoveUser(hDlg);
                return (INT_PTR)TRUE;

            case IDC_TV_RESET:
                OnResetUser(hDlg);
                return (INT_PTR)TRUE;

            case IDC_TV_SHOWQR:
                OnShowQr(hDlg);
                return (INT_PTR)TRUE;

            case IDC_TV_DELTOTP:
                OnClearTotp(hDlg);
                return (INT_PTR)TRUE;

            case IDC_TV_SETPASS:
                OnSetPassword(hDlg);
                return (INT_PTR)TRUE;

            case IDC_TV_TESTPASS:
                OnTestPassword(hDlg);
                return (INT_PTR)TRUE;

            case IDC_TV_DELPASS:
                OnClearPassword(hDlg);
                return (INT_PTR)TRUE;

            case IDOK:
            case IDCANCEL: {
                if (g_hQrBitmap)
                {
                    ::DeleteObject(g_hQrBitmap);
                    g_hQrBitmap = nullptr;
                }
                ::EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            }
            break;
        }

        return (INT_PTR)FALSE;
    }

    // ---------------------------------------------------------------------------
    // Session-token helpers
    // A session token remains pending until it is used or replaced. DPAPI machine
    // protection lets a restarted service desktop process validate it.
    // ---------------------------------------------------------------------------
    constexpr const char* kSessionSection = "SessionTokens";
    constexpr const char* kScramSection = "SCRAM"; // per-user "iter:saltB64:skB64:svkB64"
    constexpr std::uint64_t kSessionTokenLifetimeSeconds = 12U * 60 * 60;
    std::mutex g_sessionMutex;

    std::string GenerateSessionTokenString()
    {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        const std::size_t tokenLen{ 64U };
        const std::size_t alphabetLen = sizeof(alphabet) - 1;

        std::vector<BYTE> randBytes;
        randBytes.resize(tokenLen);
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, nullptr, 0)))
        {
            return std::string();
        }
        NTSTATUS status = ::BCryptGenRandom(hAlg, randBytes, static_cast<ULONG>(randBytes.size()), 0);
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        if (!BCRYPT_SUCCESS(status))
        {
            return std::string();
        }

        std::string token;
        token.reserve(tokenLen);
        for (BYTE b : randBytes)
        {
            token.push_back(alphabet[b % alphabetLen]);
        }
        return token;
    }

    bool ValidateSessionToken(const char* const user, const char* const token)
    {
        if (!user || !token)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(g_sessionMutex);
        std::string record;
        if (!TokenCore::ReadProtectedValue(kSessionSection, user, record))
        {
            return false;
        }

        std::size_t separator = record.rfind('|');
        if (separator == std::string::npos)
        {
            return false;
        }
        std::string storedToken = record.substr(0, separator);
        std::uint64_t issued = _strtoui64(record.c_str() + separator + 1, nullptr, 10);
        std::uint64_t now = currentUnixTime();
        if ((issued == 0) || (now < issued) || (now - issued > kSessionTokenLifetimeSeconds))
        {
            TokenCore::DeleteProtectedValue(kSessionSection, user);
            return false;
        }

        std::size_t tokenLen = std::strlen(token);
        if (storedToken.size() != tokenLen)
        {
            return false;
        }
        unsigned char diff{ 0U };
        for (std::size_t i{ 0U }; i < tokenLen; ++i)
        {
            diff |= static_cast<unsigned char>(storedToken[i] ^ token[i]);
        }
        if (diff != 0)
        {
            return false;
        }

        TokenCore::DeleteProtectedValue(kSessionSection, user);
        return true;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// DLL exports
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport) int __cdecl validate_token(const char* const user, const char* const token)
{
    if (!user || !token)
    {
        return 0;
    }

    std::size_t len = std::strlen(token);

    // TOTP codes (6-8 digits) are checked against stored per-user secrets.
    if ((len >= 6) && (len <= 8) && isAllDigits(token))
    {
        return validateTotp(user, token) ? 1 : 0;
    }

    // 64-character server-issued session tokens allow reconnect without a new TOTP.
    if (len == 64)
    {
        return ValidateSessionToken(user, token) ? 1 : 0;
    }

    return 0;
}

extern "C" __declspec(dllexport) int __cdecl issue_session_token(const char* const user, char* const outBuf,
                                                                 const std::size_t outBufSize)
{
    if (!user || !outBuf || (outBufSize < 65))
    {
        return 0;
    }

    std::string token = GenerateSessionTokenString();
    if (token.empty())
    {
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        std::string record = token + "|" + std::to_string(currentUnixTime());
        if (!TokenCore::WriteProtectedValue(kSessionSection, user, record))
        {
            return 0;
        }
    }

    ::strncpy_s(outBuf, outBufSize, token.c_str(), _TRUNCATE);
    return 1;
}

// Return the stored SCRAM-SHA-256 verifier for SASL subtype 263.
// The verifier string is key material: a stolen verifier lets an attacker
// complete the server side but not recover the password, matching the
// security model of PostgreSQL/pg_authid.
extern "C" __declspec(dllexport) int __cdecl sasl_lookup(const char* const user, char* const outBuf, const std::size_t outBufSize)
{
    if (!user || !*user || !outBuf || (outBufSize == 0))
    {
        return 0;
    }

    std::string verifier;
    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        if (!TokenCore::ReadProtectedValue(kScramSection, user, verifier) || verifier.empty())
        {
            return 0;
        }
    }
    if (verifier.size() >= outBufSize)
    {
        return 0;
    }
    std::memcpy(outBuf, verifier, verifier.size());
    outBuf[verifier.size()] = '\0';
    return 1;
}

extern "C" __declspec(dllexport) int __cdecl tv_get_capabilities(void)
{ return TV_CAP_SESSION_TOKEN | TV_CAP_MANAGEMENT_UI | TV_CAP_SASL; }

extern "C" __declspec(dllexport) int __cdecl tv_has_management_ui(void) { return 1; }

extern "C" __declspec(dllexport) int __cdecl tv_show_management_ui(const HWND hwndParent)
{
    g_hQrBitmap = nullptr;
    INT_PTR ret = ::DialogBoxParamA(g_hInst, MAKEINTRESOURCEA(IDD_TVMANAGER), hwndParent, TokenManagerDlgProc, 0);
    if (g_hQrBitmap)
    {
        ::DeleteObject(g_hQrBitmap);
        g_hQrBitmap = nullptr;
    }
    return ((ret == IDOK) || (ret == IDCANCEL)) ? 1 : 0;
}

extern "C" __declspec(dllexport) int __cdecl tv_get_last_error(char* const buf, std::size_t bufSize)
{
    if (buf && (bufSize > 0))
    {
        buf[0U] = '\0';
    }
    return 0;
}

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(const HMODULE hModule, const DWORD ul_reason_for_call, const LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = (HINSTANCE)hModule;
        TokenCore::SetSecretStorePathsFromModuleHandle(hModule);
        ::DisableThreadLibraryCalls(hModule);
        break;

    case DLL_PROCESS_DETACH:
        if (g_hQrBitmap)
        {
            ::DeleteObject(g_hQrBitmap);
            g_hQrBitmap = nullptr;
        }
        break;
    }
    return TRUE;
}
