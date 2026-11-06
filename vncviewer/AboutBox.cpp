// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "stdhdrs.h"

#include "../common/Hyperlinks.h"
#include "UltraVNCHelperFunctions.h"
#include "vncviewer.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

HBITMAP
DoGetBkGndBitmap(IN CONST UINT uBmpResId)
{
    static HBITMAP hbmBkGnd = nullptr;
    if (nullptr == hbmBkGnd)
    {
        /*char szFileName[MAX_PATH];
        if (GetModuleFileName(NULL, szFileName, MAX_PATH))
            {
            char* p = strrchr(szFileName, '\\');
                if (p == NULL) return false;
                *p = '\0';
            strcat (szFileName,"\\background2.bmp");
        }
        hbmBkGnd = (HBITMAP)LoadImage( NULL, szFileName, IMAGE_BITMAP, 0, 0,
           LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE );*/
        hbmBkGnd =
            reinterpret_cast<HBITMAP>(::LoadImage(::GetModuleHandle(nullptr), MAKEINTRESOURCE(uBmpResId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));


        if (nullptr == hbmBkGnd)
        {
            hbmBkGnd = reinterpret_cast<HBITMAP>(-1);
        }
    }
    return (hbmBkGnd == reinterpret_cast<HBITMAP>(-1)) ? nullptr : hbmBkGnd;
}
BOOL DoSDKEraseBkGnd(IN CONST HDC hDC, IN CONST COLORREF crBkGndFill)
{
    HBITMAP hbmBkGnd = DoGetBkGndBitmap(IDB_BITMAP10);
    if (hDC && hbmBkGnd)
    {
        RECT rc;
        if ((ERROR != ::GetClipBox(hDC, &rc)) && !::IsRectEmpty(&rc))
        {
            HDC hdcMem = ::CreateCompatibleDC(hDC);
            if (hdcMem)
            {
                HBRUSH hbrBkGnd = ::CreateSolidBrush(crBkGndFill);
                if (hbrBkGnd)
                {
                    HGDIOBJ hbrOld = ::SelectObject(hDC, hbrBkGnd);
                    if (hbrOld)
                    {
                        SIZE size{ (rc.right - rc.left), (rc.bottom - rc.top) };

                        if (::PatBlt(hDC, rc.left, rc.top, size.cx, size.cy, PATCOPY))
                        {
                            HGDIOBJ hbmOld = ::SelectObject(hdcMem, hbmBkGnd);
                            if (hbmOld)
                            {
                                ::BitBlt(hDC, rc.left, rc.top, size.cx, size.cy, hdcMem, rc.left, rc.top, SRCCOPY);
                                ::SelectObject(hdcMem, hbmOld);
                            }
                        }
                        ::SelectObject(hDC, hbrOld);
                    }
                    ::DeleteObject(hbrBkGnd);
                }
                ::DeleteDC(hdcMem);
            }
        }
    }
    return TRUE;
}

void convertToISO8601(const char* const input, char* const output, const std::size_t size)
{
    // Expected format: "Mar 14 2025 12:34:56"

    // Convert month abbreviation to a number
    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";

    std::array<char, 4U> monthStr; // Buffer for the month abbreviation (e.g., "Mar")
    int day, year, month;
    int hour, minute, second;

    // Extract components from the input string
    std::sscanf(input, "%3s %d %d %d:%d:%d", monthStr.data(), &day, &year, &hour, &minute, &second);

    // Convert month abbreviation to number (1-12)
    month = static_cast<int>((std::strstr(months, monthStr.data()) - months) / 3 + 1);

    // Format into ISO 8601 format "YYYY-MM-DDTHH:MM:SS"
    std::snprintf(output, size, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
}

// Process the About dialog.
static LRESULT CALLBACK AboutDlgProc(const HWND hwnd, const UINT iMsg, const WPARAM wParam, [[maybe_unused]] const LPARAM lParam)
{
    switch (iMsg)
    {
    case WM_INITDIALOG: {
        //CentreWindow(hwnd);
        ::SetForegroundWindow(hwnd);
        extern char buildtime[];
        std::array<char, 20U> isoTime; // Buffer for ISO output
        convertToISO8601(buildtime, isoTime.data(), sizeof(isoTime));
        {
            std::array<wchar_t, 20U> _wt;
            ::MultiByteToWideChar(CP_UTF8, 0, isoTime.data(), -1, _wt.data(), 20);
            ::SetDlgItemTextW(hwnd, IDC_BUILDTIME, _wt.data());
        }

        ConvertStaticToHyperlink(hwnd, IDC_UVNCCOM);
        std::array<char, 50U> version{};
        std::array<char, 256U> title{};
        ::strcpy_s(title.data(), title.size(), "UltraVNC Viewer -");
        ::strcat_s(title.data(), title.size(), GetVersionFromResource(version.data()));
        {
            std::array<wchar_t, 256U> _wtitle;
            ::MultiByteToWideChar(CP_UTF8, 0, title.data(), -1, _wtitle.data(), 256);
            ::SetDlgItemTextW(hwnd, IDC_UVVERSION, _wtitle.data());
        }

        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));

        return TRUE;
    }
    case WM_CLOSE:
        ::EndDialog(hwnd, TRUE);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            ::EndDialog(hwnd, TRUE);
        }
        if (LOWORD(wParam) == IDC_UVNCCOM)
        {
            ::ShellExecute(::GetDesktopWindow(), _T("open"), _T("https://uvnc.com/"), _T(""), nullptr, SW_SHOWNORMAL);
        }
    }
    return FALSE;
}

void ShowAboutBox()
{
    [[maybe_unused]] const int res = static_cast<int>(::DialogBox(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_APP_ABOUT), nullptr, reinterpret_cast<DLGPROC>(AboutDlgProc)));
}
