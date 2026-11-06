// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "UltraVNCHelperFunctions.h"

#include <array>
#include <cstdio>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Version.lib")

std::array<wchar_t, 128U> str50275;
std::array<wchar_t, 128U> str50276;
std::array<wchar_t, 128U> str50277;
std::array<wchar_t, 128U> str50278;
std::array<wchar_t, 128U> str50279;
std::array<wchar_t, 128U> str50280;
std::array<wchar_t, 128U> str50281;
std::array<wchar_t, 128U> str50282;
std::array<wchar_t, 128U> str50283;
std::array<wchar_t, 128U> str50284;
std::array<wchar_t, 128U> str50285;
std::array<wchar_t, 128U> str50286;
std::array<wchar_t, 128U> str50287;
std::array<wchar_t, 128U> str50288;
std::array<wchar_t, 128U> str50289;
std::array<wchar_t, 128U> str50290;
std::array<wchar_t, 128U> str50293;
std::array<wchar_t, 128U> str50294;
std::array<wchar_t, 128U> str50295;
std::array<wchar_t, 128U> str50296;
std::array<wchar_t, 128U> str50297;
extern HINSTANCE m_hInstResDLL;


void loadStrings(const HINSTANCE m_hInstResDLLLocal)
{
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50275, str50275.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50276, str50276.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50277, str50277.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50278, str50278.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50279, str50279.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50280, str50280.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50281, str50281.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50282, str50282.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50283, str50283.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50284, str50284.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50285, str50285.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50286, str50286.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50287, str50287.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50288, str50288.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50289, str50289.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50290, str50290.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50293, str50293.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50294, str50294.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50295, str50295.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50296, str50296.data(), 128 - 1);
    ::LoadStringW(m_hInstResDLLLocal, IDS_STRING50297, str50297.data(), 128 - 1);
}

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

                    if (::VerQueryValue(pResCopy, _T("\\"), reinterpret_cast<LPVOID*>(&lpFfi), &uLen))
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
    //strcat(version, (char* )"-dev");
    return version;
}
