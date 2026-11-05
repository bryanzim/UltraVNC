// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "win32_helpers.h"

#include "rfb.h"

#include <winsock2.h>

#include <windows.h>

#include <mbstring.h>

#include <array>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#ifdef _VIEWER
#include "../vncviewer/res/resource.h"

#else
#include "../winvnc/winvnc/resource.h"

#include <cctype>
#include <cstdlib>
#include <cwchar>

#endif

namespace helper
{

    void SafeSetMsgResult(const HWND hwnd, const LPARAM result)
    {
#ifndef _X64
        ::SetWindowLong(hwnd, DWL_MSGRESULT, result);
#else
        ::SetWindowLongPtr(hwnd, DWLP_MSGRESULT, result);
#endif
    }

    void SafeSetWindowUserData(const HWND hwnd, const LPARAM lParam)
    {
#ifndef _X64
        ::SetWindowLong(hwnd, GWL_USERDATA, lParam);
#else
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
#endif
    }

    HINSTANCE SafeGetWindowInstance(const HWND hWnd)
    {
#ifndef _X64
        HINSTANCE hInstance = (HINSTANCE)::GetWindowLong(hWnd, GWL_HINSTANCE);
#else
        HINSTANCE hInstance = (HINSTANCE)::GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
#endif
        return hInstance;
    }

    LONG SafeGetWindowProc(const HWND hWnd)
    {
#ifndef _X64
        LONG pWndProc = ::GetWindowLong(hWnd, GWL_WNDPROC);
#else
        //TODO: get rid of LONG_PTR return warning for x64
        LONG pWndProc = ::GetWindowLongPtr(hWnd, GWLP_WNDPROC);
#endif
        return pWndProc;
    }

    void SafeSetWindowProc(const HWND hWnd, const LONG_PTR pWndProc)
    {
#ifndef _X64
        ::SetWindowLong(hWnd, GWL_WNDPROC, pWndProc);
#else
        ::SetWindowLongPtr(hWnd, GWLP_WNDPROC, pWndProc);
#endif
    }

    void close_handle(HANDLE& h)
    {
        if (h != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
    }

    // Add the \\?\ (or \\?\UNC\) prefix so paths longer than MAX_PATH work.
    static void PrefixLongPathW(LPCWSTR in, WCHAR* const out, const int outLen)
    {
        if ((in[0U] == L'\\') && (in[1U] == L'\\') && (in[2U] == L'?') && (in[3U] == L'\\'))
        {
            ::wcscpy_s(out, outLen, in);
        } // already prefixed
        else if ((in[0U] == L'\\') && (in[1U] == L'\\'))
        {
            ::_snwprintf_s(out, outLen, _TRUNCATE, L"\\\\?\\UNC\\%s", in + 2);
        }
        else if (in[0U] && (in[1U] == L':'))
        {
            ::_snwprintf_s(out, outLen, _TRUNCATE, L"\\\\?\\%s", in);
        }
        else
        {
            ::wcscpy_s(out, outLen, in);
        }
    }

    // Helper to log a merge failure with the failing path and error.
    static void MoveDirLog(const LPCWSTR msg, const LPCWSTR path)
    {
        DWORD dwErr = ::GetLastError();
        std::array<WCHAR, MAX_PATH * 4 + 128> buf;
        ::_snwprintf_s(buf.data(), buf.size(), _TRUNCATE, L"[MoveDirContentsInto] %s: %s (err=%lu)\n", msg, path, dwErr);
        ::OutputDebugStringW(buf.data());
    }

    // Recursive worker on already-prefixed paths.
    static bool MoveDirContentsIntoImpl(const LPCWSTR srcDirL, const LPCWSTR dstDirL, const unsigned depth)
    {
        if (depth > 64)
        {
            return false;
        }

        DWORD dstAttr = ::GetFileAttributesW(dstDirL);
        if (dstAttr == INVALID_FILE_ATTRIBUTES)
        {
            if (!::CreateDirectoryW(dstDirL, nullptr))
            {
                MoveDirLog(L"CreateDirectoryW failed", dstDirL);
                return false;
            }
        }
        else if (!(dstAttr & FILE_ATTRIBUTE_DIRECTORY))
        {
            MoveDirLog(L"destination is not a directory", dstDirL);
            return false; // can't merge a directory into a file
        }

        std::vector<WCHAR> pattern(MAX_PATH * 4);
        ::_snwprintf_s(pattern.data(), pattern.size(), _TRUNCATE, L"%s\\*", srcDirL);
        WIN32_FIND_DATAW fd;
        HANDLE hFind = ::FindFirstFileW(pattern.data(), &fd);
        if (hFind == INVALID_HANDLE_VALUE)
        {
            MoveDirLog(L"FindFirstFileW failed", srcDirL);
            return false;
        }

        bool ok{ true };
        do
        {
            if (!std::wcscmp(fd.cFileName, L".") || !std::wcscmp(fd.cFileName, L".."))
            {
                continue;
            }

            std::vector<WCHAR> srcChild(MAX_PATH * 4), dstChild(MAX_PATH * 4);
            ::_snwprintf_s(srcChild.data(), srcChild.size(), _TRUNCATE, L"%s\\%s", srcDirL, fd.cFileName);
            ::_snwprintf_s(dstChild.data(), dstChild.size(), _TRUNCATE, L"%s\\%s", dstDirL, fd.cFileName);

            DWORD a = ::GetFileAttributesW(dstChild.data());
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if ((a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY))
                {
                    MoveDirLog(L"source dir conflicts with destination file", dstChild.data());
                    ok = false; // file/dir name conflict
                    continue;
                }
                // Always create/ensure the target dir then merge recursively.
                // This is slower than a whole-subtree rename, but it works across
                // volumes and with long paths, and the source subdir is removed
                // by the recursive call once it is empty.
                if ((a == INVALID_FILE_ATTRIBUTES) && !::CreateDirectoryW(dstChild.data(), nullptr))
                {
                    MoveDirLog(L"CreateDirectoryW failed for subdir", dstChild.data());
                    ok = false;
                    continue;
                }
                if (!MoveDirContentsIntoImpl(srcChild.data(), dstChild.data(), depth + 1))
                {
                    ok = false;
                }
            }
            else
            {
                if (a != INVALID_FILE_ATTRIBUTES)
                {
                    if (a & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        MoveDirLog(L"source file conflicts with destination dir", dstChild.data());
                        ok = false; // dir/file name conflict
                        continue;
                    }
                    if (a & FILE_ATTRIBUTE_READONLY)
                    {
                        ::SetFileAttributesW(dstChild.data(), a & ~FILE_ATTRIBUTE_READONLY);
                    }
                }
                if (!::MoveFileExW(srcChild.data(), dstChild.data(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
                {
                    MoveDirLog(L"MoveFileExW failed", srcChild.data());
                    ok = false;
                }
            }
        } while (::FindNextFileW(hFind, &fd));
        ::FindClose(hFind);

        if (ok)
        {
            if (!::RemoveDirectoryW(srcDirL))
            {
                MoveDirLog(L"RemoveDirectoryW failed on source", srcDirL);
                ok = false;
            }
        }
        return ok;
    }

    bool MoveDirContentsInto(const LPCWSTR srcDir, const LPCWSTR dstDir)
    {
        std::array<WCHAR, MAX_PATH * 4> srcL;
        std::array<WCHAR, MAX_PATH * 4> dstL;
        PrefixLongPathW(srcDir, srcL.data(), srcL.size());
        PrefixLongPathW(dstDir, dstL.data(), dstL.size());

        std::array<WCHAR, MAX_PATH * 4 + 64> info;
        ::_snwprintf_s(info.data(), info.size(), _TRUNCATE, L"[MoveDirContentsInto] %s -> %s\n", srcL.data(), dstL.data());
        ::OutputDebugStringW(info.data());

        return MoveDirContentsIntoImpl(srcL.data(), dstL.data(), 0);
    }

    bool IsSafeFileNameW(const LPCWSTR name)
    {
        if ((name == nullptr) || (name[0U] == L'\0'))
        {
            return false;
        }
        if ((std::wcscmp(name, L".") == 0) || (std::wcscmp(name, L"..") == 0))
        {
            return false;
        }
        std::size_t len = std::wcslen(name);
        if ((name[len - 1] == L'.') || (name[len - 1] == L' '))
        {
            return false;
        }
        for (const WCHAR* p = name; *p; ++p)
        {
            switch (*p)
            {
            case L'\\':
            case L'/':
            case L':':
            case L'*':
            case L'?':
            case L'"':
            case L'<':
            case L'>':
            case L'|':
                return false;
            }
        }
        return true;
    }

    bool IsSafeRelativePathW(const LPCWSTR name)
    {
        if ((name == nullptr) || (name[0U] == L'\0'))
        {
            return false;
        }
        if ((name[0U] == L'\\') || (name[0U] == L'/'))
        { // rooted or UNC
            return false;
        }
        if (name[1U] == L':')
        { // drive absolute (C:...)
            return false;
        }

        const WCHAR* p = name;
        while (*p)
        {
            const WCHAR* e = p;
            while (*e && (*e != L'\\') && (*e != L'/'))
            {
                ++e;
            }
            std::size_t len = e - p;
            if (len > 0)
            {
                if (((len == 1) && (p[0U] == L'.')) || ((len == 2) && (p[0U] == L'.') && (p[1U] == L'.')))
                {
                    return false;
                }
                for (std::size_t i{ 0U }; i < len; ++i)
                {
                    if (p[i] == L':')
                    {
                        return false;
                    }
                }
            }
            p = (*e) ? e + 1 : e;
        }
        return true;
    }

    bool IsDirectChildPathW(const LPCWSTR parentDir, const LPCWSTR path)
    {
        if ((parentDir == nullptr) || (path == nullptr))
        {
            return false;
        }

        std::array<WCHAR, MAX_PATH * 4> fullParent;
        std::array<WCHAR, MAX_PATH * 4> fullChild;
        if (!::GetFullPathNameW(parentDir, MAX_PATH * 4, fullParent.data(), nullptr))
        {
            return false;
        }
        if (!::GetFullPathNameW(path, MAX_PATH * 4, fullChild.data(), nullptr))
        {
            return false;
        }

        std::size_t parentLen = std::wcslen(fullParent.data());
        while ((parentLen > 0) && (fullParent[parentLen - 1] == L'\\'))
        {
            fullParent[--parentLen] = L'\0';
        }
        std::size_t childLen = std::wcslen(fullChild.data());
        if ((parentLen == 0) || (childLen <= parentLen + 1))
        {
            return false;
        }
        if (::_wcsnicmp(fullChild.data(), fullParent.data(), parentLen) != 0)
        {
            return false;
        }
        if (fullChild[parentLen] != L'\\')
        {
            return false;
        }
        // The remainder must be a single path component.
        if (std::wcschr(fullChild.data() + parentLen + 1, L'\\') != nullptr)
        {
            return false;
        }
        return true;
    }

    bool IsExpectedDirZipReply(const char* const szReceived, const char* const szRequested, const bool fUnicodeWireNames,
                               const bool fOldProtocol)
    {
        if ((szReceived == nullptr) || (szRequested == nullptr))
        {
            return false;
        }

        // The requested path must end in a directory marker: "[ name ]" for the
        // current protocol, "(name)" for the <= RC18 protocol (the request is
        // rewritten for old servers). _mbsrchr handles DBCS names like strrchr
        // is meant to in the file-transfer code.
        const char* reqBase = (const char*)_mbsrchr((const unsigned char*)szRequested, '\\');
        reqBase = reqBase ? reqBase + 1 : szRequested;
        std::size_t reqLen = std::strlen(reqBase);
        bool bBracketMarker = ((reqLen >= 5) && (reqBase[0U] == rfbDirPrefix[0U]) && (reqBase[1U] == rfbDirPrefix[1U]) &&
                               (reqBase[reqLen - 2] == rfbDirSuffix[0U]) && (reqBase[reqLen - 1] == rfbDirSuffix[1U]));
        bool bParenMarker = (fOldProtocol && (reqLen >= 3) && (reqBase[0U] == '(') && (reqBase[reqLen - 1] == ')'));
        if (!bBracketMarker && !bParenMarker)
        {
            return false;
        }

        // The reply basename must be !UVNCDIR-<name>.zip
        const char* rcvBase = (const char*)_mbsrchr((const unsigned char*)szReceived, '\\');
        rcvBase = rcvBase ? rcvBase + 1 : szReceived;
        if (std::strncmp(rcvBase, rfbZipDirectoryPrefix, std::strlen(rfbZipDirectoryPrefix)) != 0)
        {
            return false;
        }
        const char* mid = rcvBase + std::strlen(rfbZipDirectoryPrefix);
        std::size_t midLen = std::strlen(mid);
        if ((midLen <= 4) || (::_stricmp(mid + midLen - 4, ".zip") != 0))
        {
            return false;
        }
        midLen -= 4;
        if ((midLen == 0) || (midLen >= MAX_PATH * 3))
        {
            return false;
        }

        if (fUnicodeWireNames)
        {
            // Hex-encoded UTF-8 dirname; must decode to a prefix of the requested
            // directory name (senders truncate the hex at 200 characters).
            // Unicode-capable servers always use the "[ name ]" request marker.
            if (!bBracketMarker)
            {
                return false;
            }
            if (((midLen % 2) != 0) || (midLen > 200))
            {
                return false;
            }
            const char* reqName = reqBase + 2;
            std::size_t reqNameLen = reqLen - 4;
            if (midLen / 2 > reqNameLen)
            {
                return false;
            }
            for (std::size_t i{ 0U }; i < midLen / 2; ++i)
            {
                if (!std::isxdigit(static_cast<unsigned char>(mid[i * 2])) || !std::isxdigit(static_cast<unsigned char>(mid[i * 2 + 1])))
                {
                    return false;
                }
                char hexByte[3U] = { mid[i * 2], mid[i * 2 + 1], '\0' };
                if ((unsigned char)std::strtol(hexByte, nullptr, 16) != static_cast<unsigned char>(reqName[i]))
                {
                    return false;
                }
            }
            return true;
        }

        // Old servers reply with a plain (8.3) directory name. It becomes the
        // local folder name, so only accept a safe, traversal-free name.
        std::array<char, MAX_PATH * 3> szMid;
        std::memcpy(szMid.data(), mid, midLen);
        szMid[midLen] = '\0';
        std::array<WCHAR, MAX_PATH * 4> szMidW;
        ::MultiByteToWideChar(CP_ACP, 0, szMid.data(), -1, szMidW.data(), szMidW.size());
        return IsSafeFileNameW(szMidW.data());
    }

    DynamicFnBase::DynamicFnBase(const TCHAR* dllName, const char* fnName) : fnPtr{ 0 }, dllHandle{ 0 }
    {
        dllHandle = ::LoadLibrary(dllName);
        if (!dllHandle)
        {
            return;
        }
        fnPtr = (void*)::GetProcAddress(dllHandle, fnName);
    }

    DynamicFnBase::~DynamicFnBase()
    {
        if (dllHandle)
        {
            ::FreeLibrary(dllHandle);
        }
    }

    bool yesnoUVNCMessageBox(const HINSTANCE hInst, const HWND m_hWnd, char* const szHeader, char* const body, char* const okStr,
                             char* const cancelStr, char* const checkbox, BOOL& bCheckboxChecked)
    {
        std::array<wchar_t, 128U> w_header;
        std::array<wchar_t, 1024U> w_body;
        std::array<wchar_t, 1024U> w_checkbox;
        std::array<wchar_t, 512U> w_okStr;
        std::array<wchar_t, 512U> w_cancelStr;
        ::MultiByteToWideChar(CP_ACP, 0, szHeader, -1, w_header.data(), 128);
        ::MultiByteToWideChar(CP_ACP, 0, body, -1, w_body.data(), 1024);
        if (std::strlen(checkbox) > 0)
        {
            ::MultiByteToWideChar(CP_ACP, 0, checkbox, -1, w_checkbox.data(), 1024);
        }
        ::MultiByteToWideChar(CP_ACP, 0, okStr, -1, w_okStr.data(), 512);
        ::MultiByteToWideChar(CP_ACP, 0, cancelStr, -1, w_cancelStr.data(), 512);

        HRESULT hr;
        TASKDIALOGCONFIG tdc{ sizeof(TASKDIALOGCONFIG) };
        int nClickedBtn;
#ifdef _VIEWER
        LPCWSTR szTitle = L"UltraVNC Viewer";
#else
        LPCWSTR szTitle = L"UltraVNC Server";
#endif
        TASKDIALOG_BUTTON aCustomButtons[] = { { 1000, w_okStr.data() }, { 1001, w_cancelStr.data() } };
        tdc.cbSize = sizeof(tdc);
        tdc.hInstance = hInst;
        tdc.hwndParent = m_hWnd;
        tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS;
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = _countof(aCustomButtons);
        tdc.pszWindowTitle = szTitle;
        tdc.nDefaultButton = 1001;
#ifdef _VIEWER
        tdc.pszMainIcon = MAKEINTRESOURCEW(IDR_TRAY); // TD_INFORMATION_ICON;
#else
        tdc.pszMainIcon = MAKEINTRESOURCEW(IDI_WINVNC); // TD_INFORMATION_ICON;
#endif
        tdc.pszMainInstruction = w_header.data();
        tdc.pszContent = w_body.data();
        if (std::strlen(checkbox) > 0)
        {
            tdc.pszVerificationText = w_checkbox.data();
        }

        hr = ::TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &bCheckboxChecked);

        if (SUCCEEDED(hr) && (1000 == nClickedBtn))
        {
            return true;
        }
        return false;
    }

    bool yesUVNCMessageBox(const HINSTANCE hInst, const HWND m_hWnd, char* const body, char* const szHeader, const int icon)
    {
        std::array<wchar_t, 128U> w_header;
        std::array<wchar_t, 2048U> w_body;
        ::MultiByteToWideChar(CP_ACP, 0, szHeader, -1, w_header.data(), 128);
        ::MultiByteToWideChar(CP_ACP, 0, body, -1, w_body.data(), 2048);

        HRESULT hr;
        TASKDIALOGCONFIG tdc{ sizeof(TASKDIALOGCONFIG) };
        int nClickedBtn;
#ifdef _VIEWER
        LPCWSTR szTitle = L"UltraVNC Viewer";
#else
        LPCWSTR szTitle = L"UltraVNC Server";
#endif

        tdc.cbSize = sizeof(tdc);
        tdc.hInstance = hInst;
        tdc.hwndParent = m_hWnd;
        tdc.dwCommonButtons = TDCBF_OK_BUTTON;
        tdc.pszWindowTitle = szTitle;
        tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;

        switch (icon)
        {
        case MB_ICONEXCLAMATION:
            tdc.pszMainIcon = TD_WARNING_ICON;
            break;
        case MB_ICONINFORMATION:
            tdc.pszMainIcon = TD_INFORMATION_ICON;
            break;
        case MB_ICONERROR:
            tdc.pszMainIcon = TD_ERROR_ICON;
            break;
        default:
#ifdef _VIEWER
            tdc.pszMainIcon = MAKEINTRESOURCEW(IDR_TRAY); // TD_INFORMATION_ICON;
#else
            tdc.pszMainIcon = MAKEINTRESOURCEW(IDI_WINVNC); // TD_INFORMATION_ICON;
#endif
            break;
        }
        tdc.pszMainInstruction = w_header.data();
        tdc.pszContent = w_body.data();

        hr = ::TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
        if (SUCCEEDED(hr) && (TDCBF_OK_BUTTON == nClickedBtn))
        {
            return true;
        }
        return false;
    }

    bool yesUVNCMessageBox(const HINSTANCE hInst, const HWND m_hWnd, wchar_t* const body, wchar_t* const szHeader, const int icon)
    {
        HRESULT hr;
        TASKDIALOGCONFIG tdc{ sizeof(TASKDIALOGCONFIG) };
        int nClickedBtn;
#ifdef _VIEWER
        LPCWSTR szTitle = L"UltraVNC Viewer";
#else
        LPCWSTR szTitle = L"UltraVNC Server";
#endif

        tdc.cbSize = sizeof(tdc);
        tdc.hInstance = hInst;
        tdc.hwndParent = m_hWnd;
        tdc.dwCommonButtons = TDCBF_OK_BUTTON;
        tdc.pszWindowTitle = szTitle;
        tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;

        switch (icon)
        {
        case MB_ICONEXCLAMATION:
            tdc.pszMainIcon = TD_WARNING_ICON;
            break;
        case MB_ICONINFORMATION:
            tdc.pszMainIcon = TD_INFORMATION_ICON;
            break;
        case MB_ICONERROR:
            tdc.pszMainIcon = TD_ERROR_ICON;
            break;
        default:
#ifdef _VIEWER
            tdc.pszMainIcon = MAKEINTRESOURCEW(IDR_TRAY);
#else
            tdc.pszMainIcon = MAKEINTRESOURCEW(IDI_WINVNC);
#endif
            break;
        }
        tdc.pszMainInstruction = szHeader;
        tdc.pszContent = body;

        hr = ::TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
        if (SUCCEEDED(hr) && (TDCBF_OK_BUTTON == nClickedBtn))
        {
            return true;
        }
        return false;
    }

    bool yesnoUVNCMessageBox(const HINSTANCE hInst, const HWND m_hWnd, wchar_t* const szHeader, wchar_t* const body,
                             wchar_t* const okStr, wchar_t* const cancelStr, wchar_t* const checkbox, BOOL& bCheckboxChecked)
    {
        HRESULT hr;
        TASKDIALOGCONFIG tdc{ sizeof(TASKDIALOGCONFIG) };
        int nClickedBtn;
#ifdef _VIEWER
        LPCWSTR szTitle = L"UltraVNC Viewer";
#else
        LPCWSTR szTitle = L"UltraVNC Server";
#endif
        TASKDIALOG_BUTTON aCustomButtons[] = { { 1000, okStr }, { 1001, cancelStr } };
        tdc.cbSize = sizeof(tdc);
        tdc.hInstance = hInst;
        tdc.hwndParent = m_hWnd;
        tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS;
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = _countof(aCustomButtons);
        tdc.pszWindowTitle = szTitle;
        tdc.nDefaultButton = 1001;
#ifdef _VIEWER
        tdc.pszMainIcon = MAKEINTRESOURCEW(IDR_TRAY);
#else
        tdc.pszMainIcon = MAKEINTRESOURCEW(IDI_WINVNC);
#endif
        tdc.pszMainInstruction = szHeader;
        tdc.pszContent = body;
        if (checkbox && (std::wcslen(checkbox) > 0))
        {
            tdc.pszVerificationText = checkbox;
        }

        hr = ::TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &bCheckboxChecked);

        if (SUCCEEDED(hr) && (1000 == nClickedBtn))
        {
            return true;
        }
        return false;
    }

} // namespace helper
