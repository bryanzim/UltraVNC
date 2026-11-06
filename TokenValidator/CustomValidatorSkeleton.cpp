// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//

// CustomValidatorSkeleton.cpp
//
// Minimal example of a replacement TokenValidator.dll for UltraVNC.
// Copy this file, rename it, and build with:
//
//   cl.exe -LD -FeMyTokenValidator.dll CustomValidatorSkeleton.cpp advapi32.lib bcrypt.lib crypt32.lib user32.lib gdi32.lib /EHsc
//
// The exported names must match exactly and use __cdecl.
// TokenValidatorPlugin.h contains the same function signatures.

#include <Windows.h>

#include <bcrypt.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

// In-memory store for the last issued session token. A real DLL would keep a
// per-user table with expiries; this skeleton accepts one token at a time.
static std::array<char, 65U> g_issuedToken{};

// ---------------------------------------------------------------------------
// Required export
// ---------------------------------------------------------------------------
// Return 1 to allow the connection, 0 to reject it.
// This example accepts user "demo" with a 6-digit TOTP-like code "123456",
// or the session token previously returned by issue_session_token().
extern "C" __declspec(dllexport) int __cdecl validate_token(const char* const user, const char* const token)
{
    if (!user || !token)
    {
        return 0;
    }

    if ((std::strcmp(user, "demo") == 0) && (std::strcmp(token, "123456") == 0))
    {
        return 1;
    }

    if ((std::strcmp(user, "demo") == 0) && g_issuedToken[0U] && (std::strcmp(token, g_issuedToken.data()) == 0))
    {
        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Optional export for versioned subtype-300 session-token reconnect
// ---------------------------------------------------------------------------
// Called after a successful validate_token() to issue a session token for
// reconnects. The server sends this token back to the viewer.
// If you do not need session-token reconnect, do not export this function;
// WinVNC returns an empty session-token response when this export is absent.
extern "C" __declspec(dllexport) int __cdecl issue_session_token(const char* const user, char* const outBuf,
                                                                 const std::size_t outBufSize)
{
    if (!user || !outBuf || (outBufSize < 65))
    {
        return 0;
    }

    // Cryptographically random 64-hex-char session token.
    std::array<UCHAR, 32U> rnd;
    if (::BCryptGenRandom(nullptr, rnd.data(), sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    {
        return 0;
    }

    static const char hex[] = "0123456789abcdef";
    for (const int i{ 0 }; i < 32; ++i)
    {
        outBuf[2 * i] = hex[rnd[i] >> 4];
        outBuf[2 * i + 1] = hex[rnd[i] & 0x0f];
    }
    outBuf[64U] = '\0';

    // Remember it so validate_token() can accept it on reconnect.
    ::strcpy_s(g_issuedToken.data(), sizeof(g_issuedToken), outBuf);
    return 1;
}

// ---------------------------------------------------------------------------
// Optional capabilities export
// ---------------------------------------------------------------------------
// Report which optional features this DLL supports. Flags are defined in
// TokenValidatorPlugin.h. Keep these consistent with your exports:
//   TV_CAP_SESSION_TOKEN  - you export issue_session_token AND validate_token
//                           accepts the tokens it issues.
//   TV_CAP_MANAGEMENT_UI  - you export tv_show_management_ui.
// If this export is missing entirely, WinVNC falls back to probing the
// individual exports above.
#define TV_CAP_SESSION_TOKEN 0x01
#define TV_CAP_MANAGEMENT_UI 0x02

extern "C" __declspec(dllexport) int __cdecl tv_get_capabilities(void) { return TV_CAP_SESSION_TOKEN | TV_CAP_MANAGEMENT_UI; }

// ---------------------------------------------------------------------------
// Optional management UI exports
// ---------------------------------------------------------------------------
// Return 1 if you want WinVNC to show a "Manage users..." button and let it
// open your custom dialog. Return 0 to disable the button.
extern "C" __declspec(dllexport) int __cdecl tv_has_management_ui(void) { return 1; }

// Show your management dialog. hwndParent is the WinVNC properties window.
// You can use DialogBoxParam, TaskDialog, or any custom UI. Return 1 on success.
extern "C" __declspec(dllexport) int __cdecl tv_show_management_ui(const HWND hwndParent)
{
    ::MessageBoxA(hwndParent, "Replace this with your own user/token management dialog.", "Custom TokenValidator",
                MB_OK | MB_ICONINFORMATION);
    return 1;
}

// ---------------------------------------------------------------------------
// Optional diagnostics
// ---------------------------------------------------------------------------
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
BOOL APIENTRY DllMain(const HMODULE hModule, const DWORD ul_reason_for_call, [[maybe_unused]] const LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ::DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
