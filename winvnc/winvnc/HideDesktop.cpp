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

#include <cstring>

#define WIN32_LEAN_AND_MEAN
//#include <Shlwapi.h>
#include <Windows.h>

#include <tchar.h>
#include <wininet.h> // Shell object uses INTERNET_MAX_URL_LENGTH (go figure)

#if !__GNUC__ && _MSC_VER < 1400
#define _WIN32_IE 0x0400
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define _ATL_APARTMENT_THREADED
#define _USING_V110_SDK71_
#define _ATL_XP_TARGETING
#if !defined(__MINGW32__)
#include <atlbase.h> // ATL smart pointers

#endif
#include "stdhdrs.h"

#include <ShlGuid.h> // shell GUIDs
#include <ShlObj.h>  // IActiveDesktop
#include <Shlwapi.h> // DLLVERSIONINFO

#include <array>

struct __declspec(uuid("F490EB00-1240-11D1-9888-006097DEACF9")) IActiveDesktop;

#define PACKVERSION(major, minor) MAKELONG(minor, major)

DWORD GetDllVersion(const LPCTSTR lpszDllName)
{
    HINSTANCE hinstDll;
    DWORD dwVersion{ 0U };

    hinstDll = ::LoadLibrary(lpszDllName);

    if (hinstDll)
    {
        DLLGETVERSIONPROC pDllGetVersion;

        pDllGetVersion = reinterpret_cast<DLLGETVERSIONPROC>(::GetProcAddress(hinstDll, "DllGetVersion"));

        // Because some DLLs might not implement this function,
        // test for it explicitly. Depending on the particular
        // DLL, the lack of a DllGetVersion function can be an
        // indicator of the version.

        if (pDllGetVersion)
        {
            DLLVERSIONINFO dvi;
            HRESULT hr;

            ::ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);

            hr = (*pDllGetVersion)(&dvi);

            if (SUCCEEDED(hr))
            {
                dwVersion = static_cast<DWORD>(PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion));
            }
        }

        ::FreeLibrary(hinstDll);
    }
    return dwVersion;
}

typedef void(CALLBACK* P_SHGetSettings)(LPSHELLFLAGSTATE lpsfs, DWORD dwMask);

BOOL SHDesktopHTML()
{
    // Determine if Active Desktop is enabled. SHGetSettings is apparently
    // more reliable than IActiveDesktop::GetDesktopItemOptions, which can
    // report incorrectly during the middle of ApplyChanges!
    // Dynamically link to SHGetVersion so this can load on Windows with pre 4.71 shell

    HINSTANCE hinstDll;
    BOOL enabled = FALSE;

    hinstDll = ::LoadLibraryEx(_T("shell32.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (hinstDll)
    {
        P_SHGetSettings pSHGetSettings;

        if ((pSHGetSettings = reinterpret_cast<P_SHGetSettings>(::GetProcAddress(hinstDll, "SHGetSettings"))))
        {
            SHELLFLAGSTATE shfs;

            (*pSHGetSettings)(&shfs, SSF_DESKTOPHTML);
            enabled = shfs.fDesktopHTML;
        }

        ::FreeLibrary(hinstDll);
    }

    return enabled;
}

static HRESULT EnableActiveDesktop(const bool enable)
{
#if !defined(__MINGW32__)
    ::CoInitialize(nullptr);
    CComQIPtr<IActiveDesktop, &IID_IActiveDesktop> pIActiveDesktop;

    HRESULT hr;

    hr = pIActiveDesktop.CoCreateInstance(CLSID_ActiveDesktop, nullptr, CLSCTX_INPROC_SERVER);
    if (!SUCCEEDED(hr))
    {
        return hr;
    }

    COMPONENTSOPT opt;

    opt.dwSize = sizeof(opt);
    opt.fActiveDesktop = opt.fEnableComponents = enable;
    hr = pIActiveDesktop->SetDesktopItemOptions(&opt, 0);
    if (!SUCCEEDED(hr))
    {
        ::CoUninitialize();
        return hr;
    }

    hr = pIActiveDesktop->ApplyChanges(AD_APPLY_REFRESH);
    ::CoUninitialize();
    return hr;
#else
    return E_NOTIMPL;
#endif //__MINGW32__
}

bool HideActiveDesktop()
{
    // returns true if the Active Desktop was enabled and has been hidden

    if (GetDllVersion(TEXT("shell32.dll")) >= PACKVERSION(4, 71))
    {
        if (SHDesktopHTML())
        {
            return SUCCEEDED(EnableActiveDesktop(false));
        }
    }

    return false;
}

void ShowActiveDesktop()
{
    if (GetDllVersion(TEXT("shell32.dll")) >= PACKVERSION(4, 71))
    {
        EnableActiveDesktop(true);
    }
}

// OK, so this doesn't work in multiple threads or nest...
static BOOL ADWasEnabled{ false };
static BOOL ISWallPaperHided{ false };
static std::array<TCHAR, 1024U> SCREENNAME;

// Added Jef Fix - 4 March 2008 jpaquette (paquette@atnetsend.net)
static std::array<unsigned char, 10U> WallpaperStyle{}; // string "0" centered, "1" tiled, "2" stretched

static const TCHAR* const DESKTOP_KEYNAME = _T("Control Panel\\Desktop");
static const TCHAR* const DESKTOP_WALLPAPER_STYLE_KEYNAME = _T("WallpaperStyle");

void SaveWallpaperStyle()
{
    HKEY hKey;

    if (::RegOpenKeyEx(HKEY_CURRENT_USER, DESKTOP_KEYNAME, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD sz = sizeof WallpaperStyle;
        ::RegQueryValueEx(hKey, DESKTOP_WALLPAPER_STYLE_KEYNAME, nullptr, nullptr, WallpaperStyle.data(), &sz);
        ::RegCloseKey(hKey);
    }
}

void RestoreWallpaperStyle()
{
    HKEY hKey;

    if (::RegOpenKeyEx(HKEY_CURRENT_USER, DESKTOP_KEYNAME, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
    {
        DWORD sz = static_cast<DWORD>(std::strlen(reinterpret_cast<const char*>(WallpaperStyle.data())));
        ::RegSetValueEx(hKey, DESKTOP_WALLPAPER_STYLE_KEYNAME, 0, REG_SZ, WallpaperStyle.data(), sz);
        ::RegCloseKey(hKey);
    }
}
// -- end jdp


void HideDesktop()
{
    //GetProfileString("Desktop", "Pattern", "0 0 0 0 0 0 0 0", DesktopPattern, sizeof(DesktopPattern));

    // @@@efh Setting the desktop pattern via pvParam works, but is undocumented (except by www.winehq.com)
    //SystemParametersInfo(SPI_SETDESKPATTERN, 0, "0 0 0 0 0 0 0 0", SPIF_SENDCHANGE);

    // Tell all applications that there is no wallpaper
    // Note that this doesn't change the wallpaper registry setting!
    // @@@efh On Windows 95 and Windows 98 this returns an error in the debug build (but not in release)...
    vnclog.Print(LL_INTWARN, VNCLOG("Killwallpaper %i\n"), ISWallPaperHided);
    if (!ISWallPaperHided)
    {
        SaveWallpaperStyle(); // Added Jef Fix
        ::SystemParametersInfo(SPI_GETDESKWALLPAPER, 1024, SCREENNAME.data(), 0);
        ::SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, reinterpret_cast<PVOID>(const_cast<char *>("")), SPIF_SENDCHANGE);
        ADWasEnabled = HideActiveDesktop();
        ISWallPaperHided = true;
        vnclog.Print(LL_INTWARN, VNCLOG("Killwallpaper %i %i\n"), ISWallPaperHided, ADWasEnabled);
    }
}

void RestoreDesktop()
{
    vnclog.Print(LL_INTWARN, VNCLOG("Restorewallpaper %i\n"), ISWallPaperHided);
    if (ISWallPaperHided)
    {
        vnclog.Print(LL_INTWARN, VNCLOG("Restorewallpaper %i %i\n"), ISWallPaperHided, ADWasEnabled);
        if (ADWasEnabled)
        {
            ShowActiveDesktop();
        }

        ::SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, SCREENNAME.data(), SPIF_SENDCHANGE);
        RestoreWallpaperStyle(); // Added Jef Fix
    }

    ISWallPaperHided = false;

    //SystemParametersInfo(SPI_SETDESKPATTERN, 0, DesktopPattern, SPIF_SENDCHANGE);
}


// adzm - 2010-07 - Disable more effects or font smoothing
// SPI_GETUIEFFECTS can enable/disable everything in one shot, but I'm sure we'll want finer control over this eventually.

UINT spiParams[] = {
    0x1024, //SPI_GETDROPSHADOW,
    SPI_GETCOMBOBOXANIMATION,
    SPI_GETMENUANIMATION,
    SPI_GETTOOLTIPANIMATION,
    SPI_GETSELECTIONFADE,
    SPI_GETLISTBOXSMOOTHSCROLLING,
    SPI_GETHOTTRACKING,
    SPI_GETGRADIENTCAPTIONS,
    0x1042, //SPI_GETCLIENTAREAANIMATION
    0x1043, //SPI_GETDISABLEOVERLAPPEDCONTENT
};


UINT spiSuggested[] = {
    FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
};

BOOL spiValues[(sizeof(spiParams) / sizeof(spiParams[0U]))];

static bool g_bEffectsDisabled{ false };

// adzm - 2010-07 - Disable more effects or font smoothing
void DisableEffects()
{
    if (!g_bEffectsDisabled)
    {
        g_bEffectsDisabled = true;

        ::ZeroMemory(spiValues, sizeof(spiValues));

        int iParam{ 0 };
        for (iParam = 0; iParam < (sizeof(spiParams) / sizeof(spiParams[0U])); ++iParam)
        {
            if (!::SystemParametersInfo(spiParams[iParam], 0, &(spiValues[iParam]), 0))
            {
                vnclog.Print(LL_INTWARN, VNCLOG("Failed to get SPI value for 0x%04x (0x%08x)\n"), spiParams[iParam],
                             ::GetLastError());
            }
            else
            {
                vnclog.Print(LL_INTINFO, VNCLOG("Retrieved SPI value for 0x%04x: 0x%08x\n"), spiParams[iParam], spiValues[iParam]);
            }
        }
        for (iParam = 0; iParam < (sizeof(spiParams) / sizeof(spiParams[0U])); ++iParam)
        {
            if (spiValues[iParam] != static_cast<BOOL>(spiSuggested[iParam]))
            {
                if (!::SystemParametersInfo(spiParams[iParam] + 1, 0, reinterpret_cast<PVOID>(spiSuggested[iParam]), SPIF_SENDCHANGE))
                {
                    vnclog.Print(LL_INTWARN, VNCLOG("Failed to set SPI value for 0x%04x to 0x%08x (0x%08x)\n"),
                                 spiParams[iParam] + 1, spiSuggested[iParam], ::GetLastError());
                }
                else
                {
                    vnclog.Print(LL_INTINFO, VNCLOG("Set SPI value for 0x%04x to 0x%08x\n"), spiParams[iParam] + 1,
                                 spiSuggested[iParam]);
                }
            }
        }
    }
}

// adzm - 2010-07 - Disable more effects or font smoothing
void EnableEffects()
{
    if (g_bEffectsDisabled)
    {
        int iParam{ 0 };
        for (iParam = 0; iParam < (sizeof(spiParams) / sizeof(spiParams[0U])); ++iParam)
        {
            if (spiValues[iParam] != static_cast<BOOL>(spiSuggested[iParam]))
            {
                if (!::SystemParametersInfo(spiParams[iParam] + 1, 0, reinterpret_cast<PVOID>(spiValues[iParam]), SPIF_SENDCHANGE))
                {
                    vnclog.Print(LL_INTWARN, VNCLOG("Failed to restore SPI value for 0x%04x (0x%08x)\n"), spiParams[iParam] + 1,
                                 ::GetLastError());
                }
                else
                {
                    vnclog.Print(LL_INTINFO, VNCLOG("Restored SPI value for 0x%04x to 0x%08x\n"), spiParams[iParam] + 1,
                                 spiValues[iParam]);
                }
            }
        }

        ::ZeroMemory(spiValues, sizeof(spiValues));
    }

    g_bEffectsDisabled = false;
}



// adzm - 2010-07 - Disable more effects or font smoothing
//
bool g_bDisabledFontSmoothing{ false };

bool g_bGotOldFontSmoothingValue{ false };
bool g_bGotFontSmoothingType{ false };
bool g_bGotClearType{ false };
BOOL g_bOldClearTypeValue = TRUE;
BOOL g_bOldFontSmoothingValue = TRUE;
UINT g_nOldFontSmoothingType{ 0x0001U }; // FE_FONTSMOOTHINGSTANDARD

// adzm - 2010-07 - Disable more effects or font smoothing
void DisableFontSmoothing()
{
    if (!g_bDisabledFontSmoothing)
    {

        if (!g_bGotOldFontSmoothingValue)
        {
            // there appears to be a delay between setting the SPI_SETFONTSMOOTHING value and SPI_GETFONTSMOOTHING returning that
            // up-to-date value.
            if (!::SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &g_bOldFontSmoothingValue, 0))
            {
                vnclog.Print(LL_INTWARN, VNCLOG("Failed to get SPI value for SPI_GETFONTSMOOTHING (0x%08x)\n"), ::GetLastError());
                g_bGotOldFontSmoothingValue = false;
            }
            else
            {
                vnclog.Print(LL_INTINFO, VNCLOG("Retrieved SPI value for SPI_GETFONTSMOOTHING: 0x%08x\n"),
                             g_bOldFontSmoothingValue);
                g_bGotOldFontSmoothingValue = true;

                if (!::SystemParametersInfo(0x200A /*SPI_GETFONTSMOOTHINGTYPE*/, 0, &g_nOldFontSmoothingType, 0))
                {
                    vnclog.Print(LL_INTWARN, VNCLOG("Failed to get SPI value for SPI_GETFONTSMOOTHINGTYPE (0x%08x)\n"),
                                 ::GetLastError());
                    g_bGotFontSmoothingType = false;
                }
                else
                {
                    vnclog.Print(LL_INTINFO, VNCLOG("Retrieved SPI value for SPI_GETFONTSMOOTHINGTYPE: 0x%08x\n"),
                                 g_nOldFontSmoothingType);
                    g_bGotFontSmoothingType = true;
                }

                if (!::SystemParametersInfo(0x1048 /*SPI_GETCLEARTYPE*/, 0, &g_bOldClearTypeValue, 0))
                {
                    vnclog.Print(LL_INTWARN, VNCLOG("Failed to get SPI value for SPI_GETCLEARTYPE (0x%08x)\n"), ::GetLastError());
                    g_bGotClearType = false;
                }
                else
                {
                    vnclog.Print(LL_INTINFO, VNCLOG("Retrieved SPI value for SPI_GETCLEARTYPE: 0x%08x\n"), g_bOldClearTypeValue);
                    g_bGotClearType = true;
                }
            }
        }


        if (g_bGotOldFontSmoothingValue && (g_bOldFontSmoothingValue != FALSE))
        {

            if (g_bGotClearType && (g_bOldClearTypeValue != FALSE))
            {
                if (!::SystemParametersInfo(0x1049 /*SPI_SETCLEARTYPE*/, 0, reinterpret_cast<PVOID>(FALSE), SPIF_SENDCHANGE))
                {
                    vnclog.Print(LL_INTWARN, VNCLOG("Failed to set SPI value for SPI_SETCLEARTYPE (0x%08x)\n"), ::GetLastError());
                }
                else
                {
                    g_bDisabledFontSmoothing = true; // ensure we reset even if SPI_SETFONTSMOOTHING fails for some reason
                    vnclog.Print(LL_INTINFO, VNCLOG("Set SPI value for SPI_SETCLEARTYPE: 0x%08x\n"), FALSE);
                }
            }

            if (!::SystemParametersInfo(SPI_SETFONTSMOOTHING, FALSE, nullptr, SPIF_SENDCHANGE))
            {
                vnclog.Print(LL_INTWARN, VNCLOG("Failed to set SPI value for SPI_SETFONTSMOOTHING (0x%08x)\n"), ::GetLastError());
            }
            else
            {
                g_bDisabledFontSmoothing = true;
                vnclog.Print(LL_INTINFO, VNCLOG("Set SPI value for SPI_SETFONTSMOOTHING: 0x%08x\n"), FALSE);
            }
        }
    }
}

// adzm - 2010-07 - Disable more effects or font smoothing
void EnableFontSmoothing()
{
    if (g_bDisabledFontSmoothing && g_bGotOldFontSmoothingValue)
    {
        if (g_bOldFontSmoothingValue != FALSE)
        {
            if (!::SystemParametersInfo(SPI_SETFONTSMOOTHING, static_cast<UINT>(g_bOldFontSmoothingValue), nullptr, SPIF_SENDCHANGE))
            {
                vnclog.Print(LL_INTWARN, VNCLOG("Failed to restore SPI value for SPI_SETFONTSMOOTHING (0x%08x)\n"), ::GetLastError());
            }
            else
            {
                vnclog.Print(LL_INTINFO, VNCLOG("Restored SPI value for SPI_SETFONTSMOOTHING: 0x%08x\n"), g_bOldFontSmoothingValue);

                if (g_bGotClearType)
                {
                    if (!::SystemParametersInfo(0x1049 /*SPI_SETCLEARTYPE*/, 0, reinterpret_cast<PVOID>(g_bOldClearTypeValue), SPIF_SENDCHANGE))
                    {
                        vnclog.Print(LL_INTWARN, VNCLOG("Failed to restore SPI value for SPI_SETCLEARTYPE (0x%08x)\n"),
                                     ::GetLastError());
                    }
                    else
                    {
                        vnclog.Print(LL_INTINFO, VNCLOG("Restored SPI value for SPI_SETCLEARTYPE: 0x%08x\n"), g_bOldClearTypeValue);
                    }
                }

                if (g_bGotFontSmoothingType)
                {
                    if (!::SystemParametersInfo(0x200B /*SPI_SETFONTSMOOTHINGTYPE*/, 0, reinterpret_cast<PVOID>(g_nOldFontSmoothingType),
                                              SPIF_SENDCHANGE))
                    {
                        vnclog.Print(LL_INTWARN, VNCLOG("Failed to restore SPI value for SPI_SETFONTSMOOTHINGTYPE (0x%08x)\n"),
                                     ::GetLastError());
                    }
                    else
                    {
                        vnclog.Print(LL_INTINFO, VNCLOG("Restored SPI value for SPI_SETFONTSMOOTHINGTYPE: 0x%08x\n"),
                                     g_nOldFontSmoothingType);
                    }
                }
            }
        }
    }

    g_bDisabledFontSmoothing = false;
}
