// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// vncAbout.cpp

// Implementation of the About dialog!

#include "stdhdrs.h"

#include "vncabout.h"

#include "../common/Hyperlinks.h"
#include "winvnc.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

extern char configFile[256U];

//	[v1.0.2-jp1 fix] Load resouce from dll
extern HINSTANCE hInstResDLL;

char* GetVersionFromResource(char* const version)
{
    HRSRC hResInfo;
    DWORD dwSize;
    HGLOBAL hResData;
    LPVOID pRes, pResCopy;
    UINT uLen{ 0U };
    VS_FIXEDFILEINFO* lpFfi = nullptr;
    HINSTANCE hInst = ::GetModuleHandle(nullptr);

    hResInfo = ::FindResource(hInst, MAKEINTRESOURCE(1), RT_VERSION);
    if (hResInfo)
    {
        dwSize = ::SizeofResource(hInst, hResInfo);
        hResData = ::LoadResource(hInst, hResInfo);
        if (hResData)
        {
            pRes = ::LockResource(hResData);
            if (pRes)
            {
                pResCopy = ::LocalAlloc(LMEM_FIXED, dwSize);
                if (pResCopy)
                {
                    ::CopyMemory(pResCopy, pRes, dwSize);

                    if (::VerQueryValue(pResCopy, ("\\"), (LPVOID*)&lpFfi, &uLen))
                    {
                        if (lpFfi != nullptr)
                        {
                            DWORD dwFileVersionMS = lpFfi->dwFileVersionMS;
                            DWORD dwFileVersionLS = lpFfi->dwFileVersionLS;

                            DWORD dwLeftMost = HIWORD(dwFileVersionMS);
                            DWORD dwSecondLeft = LOWORD(dwFileVersionMS);
                            DWORD dwSecondRight = HIWORD(dwFileVersionLS);
                            DWORD dwRightMost = LOWORD(dwFileVersionLS);

                            std::sprintf(version, " %d.%d.%d.%d", dwLeftMost, dwSecondLeft, dwSecondRight, dwRightMost);
                        }
                    }

                    ::LocalFree(pResCopy);
                }
            }
        }
    }
    //strcat(version, (char*)"-dev");
    return version;
}


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
            strcat_s(szFileName,"\\background2.bmp");
        }
        hbmBkGnd = (HBITMAP)LoadImage( NULL, szFileName, IMAGE_BITMAP, 0, 0,
           LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE );*/
        hbmBkGnd =
            (HBITMAP)::LoadImage(::GetModuleHandle(nullptr), MAKEINTRESOURCE(uBmpResId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);

        if (nullptr == hbmBkGnd)
        {
            hbmBkGnd = (HBITMAP)-1;
        }
    }
    return (hbmBkGnd == (HBITMAP)-1) ? nullptr : hbmBkGnd;
}
BOOL DoSDKEraseBkGnd(IN CONST HDC hDC, IN CONST COLORREF crBkGndFill)
{
    HBITMAP hbmBkGnd = DoGetBkGndBitmap(IDB_BITMAP1);
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
    month = (std::strstr(months, monthStr.data()) - months) / 3 + 1;

    // Format into ISO 8601 format "YYYY-MM-DDTHH:MM:SS"
    std::snprintf(output, size, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
}

// Constructor/destructor
vncAbout::vncAbout() { m_dlgvisible = FALSE; }

vncAbout::~vncAbout() {}

// Initialisation
BOOL vncAbout::Init() { return TRUE; }

// Dialog box handling functions
void vncAbout::Show(const BOOL show)
{
    if (show)
    {
        if (!m_dlgvisible)
        {
            //	[v1.0.2-jp1 fix]
            //DialogBoxParam(hAppInstance,
            ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_ABOUT), nullptr, (DLGPROC)DialogProc, (LONG_PTR)this);
        }
    }
}

BOOL CALLBACK vncAbout::DialogProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    // We use the dialog-box's USERDATA to store a _this pointer
    // This is set only once WM_INITDIALOG has been recieved, though!
#ifndef _X64
    vncAbout* _this = (vncAbout*)::GetWindowLong(hwnd, GWL_USERDATA);
#else
    vncAbout* _this = (vncAbout*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
#endif
    switch (uMsg)
    {

    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        // Retrieve the Dialog box parameter and use it as a pointer
        // to the calling vncProperties object
#ifndef _X64
        ::SetWindowLong(hwnd, GWL_USERDATA, lParam);
#else
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
#endif
        _this = (vncAbout*)lParam;

        // Insert the build time information
        extern char buildtime[];
        std::array<char, 20U> isoTime; // Buffer for ISO output

        convertToISO8601(buildtime, isoTime.data(), sizeof(isoTime));

        ::SetDlgItemText(hwnd, IDC_BUILDTIME, isoTime.data());
        ConvertStaticToHyperlink(hwnd, IDC_WWW);
        // Show the dialog
        ::SetForegroundWindow(hwnd);

        _this->m_dlgvisible = TRUE;
        std::array<char, 50U> version{};
        std::array<char, 256U> title{};
        ::strcpy_s(title.data(), title.size(), "UltraVNC Server -");
        ::strcat_s(title.data(), title.size(), GetVersionFromResource(version.data()));
#ifndef _X64
        ::strcat_s(title.data(), title.size(), " - x86");
#else
        ::strcat_s(title.data(), title.size(), " - x64");
#endif
        ::SetDlgItemText(hwnd, IDC_VERSION, title.data());

        const long lszConfigFileSize{ 256 };
        std::array<char, lszConfigFileSize> szConfigFile;

        ::_snprintf_s(szConfigFile.data(), szConfigFile.size(), lszConfigFileSize - 1, "Config file: %ls", configFile);
        ::SetDlgItemText(hwnd, IDC_CONFIG_FILE, szConfigFile.data());
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {

        case IDCANCEL:
        case IDOK:
            // Close the dialog
            ::EndDialog(hwnd, TRUE);

            _this->m_dlgvisible = FALSE;

            return TRUE;
        }
    case IDC_WWW: {
        ::ShellExecute(::GetDesktopWindow(), "open", "https://uvnc.com/", "", 0, SW_SHOWNORMAL);
        return TRUE;
    }

    break;

    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        _this->m_dlgvisible = FALSE;
        return TRUE;
        /*case WM_ERASEBKGND:
                {
                    DoSDKEraseBkGnd((HDC)wParam, RGB(255,0,0));
                    return true;
                }
        case WM_CTLCOLORSTATIC:
       {

        //GetStockObject(NULL_BRUSH);
        SetBkMode((HDC) wParam, TRANSPARENT);
        return (DWORD) GetStockObject(NULL_BRUSH);

       }*/
    }
    return 0;
}
