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

#include "common/win32_helpers.h"
#include "SessionDialog.h"
#include "UltraVNCHelperFunctions.h"
#include "vncviewer.h"

#include <direct.h>
#include <ShlObj.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <fstream>

using namespace helper;
extern HINSTANCE m_hInstResDLL;

extern std::array<wchar_t, 64U> sz_K1;
extern std::array<wchar_t, 64U> sz_K2;
extern bool g_disable_sponsor;
// OPENFILENAMEW declared locally in SaveConnection

extern int EncodingFromString(const wchar_t* const szEncoding);
void SessionDialog::SaveConnection(const HWND hwnd, const bool saveAs)
{
    SettingsFromUI();
    std::array<wchar_t, _MAX_PATH> fname;
    int disp = PORT_TO_DISPLAY(m_port);
    ::swprintf_s(fname.data(), fname.size(), L"%.15s-%d.vnc", m_host_dialog.data(), ((disp > 0) && (disp < 100)) ? disp : m_port);
    std::array<wchar_t, _MAX_PATH> buffer;
    getAppData(buffer.data());
    ::wcscat_s(buffer.data(), buffer.size(), L"\\UltraVNC");
    ::_wmkdir(buffer.data());

    if (saveAs)
    {
        std::array<wchar_t, _MAX_FNAME + _MAX_EXT> tname;

        static wchar_t filter[] = L"VNC files (*.vnc)\0*.vnc\0"
                                  L"All files (*.*)\0*.*\0";
        OPENFILENAMEW ofnW;
        std::memset(reinterpret_cast<void*>(&ofnW), 0, sizeof(OPENFILENAMEW));
        ofnW.lStructSize = OPENFILENAME_SIZE_VERSION_400;
        ofnW.lpstrFilter = filter;
        ofnW.nMaxFile = _MAX_PATH;
        ofnW.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
        ofnW.lpstrDefExt = L"vnc";
        ofnW.hwndOwner = hwnd;
        ofnW.lpstrFile = fname.data();
        ofnW.lpstrFileTitle = tname.data();
        ofnW.lpstrInitialDir = buffer.data();
        ofnW.Flags = OFN_HIDEREADONLY;
        if (!::GetSaveFileNameW(&ofnW))
        {
            DWORD err = ::CommDlgExtendedError();
            switch (err)
            {
            case 0: // user cancelled
                break;
            case FNERR_INVALIDFILENAME:
                yesUVNCMessageBox(m_hInstResDLL, hwnd, sz_K1.data(), sz_K2.data(), MB_ICONERROR);
                break;
            default:
                vnclog.Print(0, _T("Error %d from GetSaveFileName\n"), err);
                break;
            }
            return;
        }
        SaveToFile(fname.data());
    }
    else
    {
        ::wcscat_s(buffer.data(), buffer.size(), L"\\");
        ::wcscat_s(buffer.data(), buffer.size(), fname.data());
        SaveToFile(buffer.data());
    }

    std::array<TCHAR, 256U> hostname;
    ::GetDlgItemText(hwnd, IDC_HOSTNAME_EDIT, hostname.data(), 256);
    m_pMRU->AddItem(hostname.data());
    //InitMRU(hwnd);
}

void SessionDialog::SettingsFromUI()
{
    ReadDlgProcEncoders();
    ReadDlgProcKeyboardMouse();
    ReadDlgProcDisplay();
    ReadDlgProcMisc();
    ReadDlgProcSecurity();
    ReadDlgProcListen();
    ReadDlgProc();
    ReadDlgProcConfig();
}

void SessionDialog::SettingsToUI(const bool initMruNeeded)
{
    InitDlgProcEncoders();
    InitDlgProcKeyboardMouse();
    InitDlgProcDisplay();
    InitDlgProcMisc();
    InitDlgProcSecurity();
    InitDlgProcListen();
    InitDlgProcConfig();
    InitDlgProc(true, initMruNeeded);
}

void SessionDialog::saveInt(wchar_t* const name, const int value, wchar_t* const fname)
{
    std::array<wchar_t, 10U> buf;
    ::swprintf_s(buf.data(), 10, L"%d", value);
    ::WritePrivateProfileStringW(L"options", name, buf.data(), fname);
}

int SessionDialog::readInt(wchar_t* const name, const int defval, wchar_t* const fname)
{ return static_cast<int>(::GetPrivateProfileIntW(L"options", name, defval, fname)); }

void SessionDialog::SaveToFile(wchar_t* const fname, const bool asDefault)
{
    int ret;
    std::array<wchar_t, 32U> buf;
    if (!asDefault)
    {
        ret = ::WritePrivateProfileStringW(L"connection", L"host", m_host_dialog.data(), fname);
        ::swprintf_s(buf.data(), 32, L"%d", m_port);
        ::WritePrivateProfileStringW(L"connection", L"port", buf.data(), fname);
    }
    else
    {
        SettingsFromUI();
    }
    ret = ::WritePrivateProfileStringW(L"connection", L"proxyhost", m_proxyhost.data(), fname);
    ::swprintf_s(buf.data(), 32, L"%d", m_proxyport);
    ::WritePrivateProfileStringW(L"connection", L"proxyport", buf.data(), fname);
    for (int i = rfbEncodingRaw; i <= LASTENCODING; ++i)
    {
        std::array<wchar_t, 128U> bufInner;
        ::swprintf_s(bufInner.data(), 128, L"use_encoding_%d", i);
        saveInt(bufInner.data(), UseEnc[static_cast<unsigned long long>(i)], fname);
    }
    if (!PreferredEncodings.empty())
    {
        saveInt(L"preferred_encoding", PreferredEncodings[0U], fname);
    }
    saveInt(L"viewonly", ViewOnly, fname);
    saveInt(L"showtoolbar", ShowToolbar, fname);
    saveInt(L"fullscreen", FullScreen, fname);
    saveInt(L"SavePos", SavePos, fname);
    saveInt(L"SaveSize", SaveSize, fname);
    saveInt(L"GNOME", GNOME, fname);
    saveInt(L"directx", Directx, fname);
    saveInt(L"autoDetect", autoDetect, fname);
    saveInt(L"8bit", Use8Bit, fname);
    saveInt(L"shared", Shared, fname);
    saveInt(L"swapmouse", SwapMouse, fname);
    saveInt(L"emulate3", Emul3Buttons, fname);
    saveInt(L"JapKeyboard", JapKeyboard, fname);
    saveInt(L"disableclipboard", DisableClipboard, fname);
    saveInt(L"Scaling", scaling, fname);
    saveInt(L"AutoScaling", fAutoScaling, fname);
    saveInt(L"AutoScalingEven", fAutoScalingEven, fname);
    saveInt(L"AutoScalingLimit", fAutoScalingLimit, fname);
    saveInt(L"scale_num", scale_num, fname);
    saveInt(L"scale_den", scale_den, fname);
    // Tight Specific
    saveInt(L"cursorshape", requestShapeUpdates, fname);
    saveInt(L"noremotecursor", ignoreShapeUpdates, fname);
    if (useCompressLevel)
    {
        saveInt(L"compresslevel", compressLevel, fname);
    }
    if (enableJpegCompression)
    {
        saveInt(L"quality", jpegQualityLevel, fname);
    }
    // Modif sf@2002
    saveInt(L"ServerScale", nServerScale, fname);
    saveInt(L"Reconnect", reconnectcounter, fname);
    saveInt(L"EnableCache", fEnableCache, fname);
    saveInt(L"EnableZstd", fEnableZstd, fname);
    saveInt(L"QuickOption", quickoption, fname);
    saveInt(L"UseDSMPlugin", fUseDSMPlugin, fname);
    saveInt(L"UseProxy", static_cast<int>(m_connectionType), fname);
    saveInt(L"allowMonitorSpanning", allowMonitorSpanning, fname);
    saveInt(L"ChangeServerRes", changeServerRes, fname);
    saveInt(L"extendDisplay", extendDisplay, fname);
    saveInt(L"showExtend", showExtend, fname);
    saveInt(L"use_virt", use_virt, fname);
    saveInt(L"useAllMonitors", useAllMonitors, fname);
    saveInt(L"requestedWidth", requestedWidth, fname);
    saveInt(L"requestedHeight", requestedHeight, fname);


    ::WritePrivateProfileStringW(L"options", L"DSMPlugin", szDSMPluginFilename.data(), fname);
    ::WritePrivateProfileStringW(L"options", L"folder", folder.data(), fname);
    ::WritePrivateProfileStringW(L"options", L"prefix", prefix, fname);
    ::WritePrivateProfileStringW(L"options", L"imageFormat", imageFormat.data(), fname);
    ::WritePrivateProfileStringW(L"options", L"InfoMsg", reinterpret_cast<LPCWSTR>(InfoMsg.data()), fname);
    saveInt(L"AutoReconnect", autoReconnect, fname);
    saveInt(L"FileTransferTimeout", FTTimeout, fname);
    saveInt(L"ListenPort", listenport, fname);
    saveInt(L"ThrottleMouse", throttleMouse, fname);
    saveInt(L"KeepAliveInterval", keepAliveInterval, fname);
    saveInt(L"AutoAcceptIncoming", fAutoAcceptIncoming, fname);
    saveInt(L"AutoAcceptNoDSM", fAutoAcceptNoDSM, fname);
#ifdef _Gii
    saveInt(L"GiiEnable", giiEnable, fname);
#endif
    saveInt(L"RequireEncryption", fRequireEncryption, fname);
    saveInt(L"UseVeNCryptTLS", fUseTokenTLS, fname);
    saveInt(L"UseOnlyDefaultConfigFile", fUseOnlyDefaultConfigFile, fname);
    saveInt(L"restricted", restricted, fname); //hide menu
    saveInt(L"ipv6", ipv6, fname);             //hide menu
    saveInt(L"AllowUntrustedServers", AllowUntrustedServers, fname);
    saveInt(L"nostatus", NoStatus, fname);                      //hide status window
    saveInt(L"HideEOStreamError", HideEndOfStreamError, fname); // hide End of Stream error
    saveInt(L"nohotkeys", NoHotKeys, fname);                    //disable hotkeys
    saveInt(L"sponsor", g_disable_sponsor, fname);
    saveInt(L"PreemptiveUpdates", preemptiveUpdates, fname);
}
void SessionDialog::LoadFromFile(wchar_t* const fname)
{
    for (int i = rfbEncodingRaw; i <= LASTENCODING; ++i)
    {
        std::array<wchar_t, 128U> buf;
        ::swprintf_s(buf.data(), 128, L"use_encoding_%d", i);
        UseEnc[static_cast<unsigned long long>(i)] = readInt(buf.data(), UseEnc[static_cast<unsigned long long>(i)], fname) != 0;
    }
    int nExistingPreferred = PreferredEncodings.empty() ? rfbEncodingZRLE : PreferredEncodings[0U];
    int nPreferredEncoding = readInt(L"preferred_encoding", nExistingPreferred, fname);
    PreferredEncodings.clear();
    PreferredEncodings.push_back(nPreferredEncoding);

    restricted = readInt(L"restricted", restricted, fname) != 0;
    ipv6 = readInt(L"ipv6", ipv6, fname) != 0;
    AllowUntrustedServers = readInt(L"AllowUntrustedServers", AllowUntrustedServers, fname) != 0;
    ViewOnly = readInt(L"viewonly", ViewOnly, fname) != 0;
    NoStatus = readInt(L"nostatus", NoStatus, fname) != 0;
    NoHotKeys = readInt(L"nohotkeys", NoHotKeys, fname) != 0;
    ShowToolbar = readInt(L"showtoolbar", ShowToolbar, fname) != 0;
    FullScreen = readInt(L"fullscreen", FullScreen, fname) != 0;
    SavePos = readInt(L"SavePos", SavePos, fname) != 0;
    SaveSize = readInt(L"SaveSize", SaveSize, fname) != 0;
    GNOME = readInt(L"GNOME", GNOME, fname) != 0;
    Directx = readInt(L"directx", Directx, fname) != 0;
    autoDetect = readInt(L"autoDetect", autoDetect, fname) != 0;
    Use8Bit = readInt(L"8bit", Use8Bit, fname);
    Shared = readInt(L"shared", Shared, fname) != 0;
    SwapMouse = readInt(L"swapmouse", SwapMouse, fname) != 0;
    Emul3Buttons = readInt(L"emulate3", Emul3Buttons, fname) != 0;
    JapKeyboard = readInt(L"JapKeyboard", JapKeyboard, fname) != 0;
    DisableClipboard = readInt(L"disableclipboard", DisableClipboard, fname) != 0;
    scaling = readInt(L"Scaling", scaling, fname) != 0;
    fAutoScaling = readInt(L"AutoScaling", fAutoScaling, fname) != 0;
    fAutoScalingEven = readInt(L"AutoScalingEven", fAutoScalingEven, fname) != 0;
    fAutoScalingLimit = readInt(L"AutoScalingLimit", fAutoScalingLimit, fname) != 0;
    scale_num = readInt(L"scale_num", scale_num, fname);
    scale_den = readInt(L"scale_den", scale_den, fname);
    // Tight specific
    requestShapeUpdates = readInt(L"cursorshape", requestShapeUpdates, fname) != 0;
    ignoreShapeUpdates = readInt(L"noremotecursor", ignoreShapeUpdates, fname) != 0;
    int level = readInt(L"compresslevel", -1, fname);
    if (level != -1)
    {
        useCompressLevel = true;
        compressLevel = level;
    }
    level = readInt(L"quality", -1, fname);
    if (level != -1)
    {
        enableJpegCompression = true;
        jpegQualityLevel = level;
    }
    // Modif sf@2002
    nServerScale = readInt(L"ServerScale", nServerScale, fname);
    reconnectcounter = readInt(L"Reconnect", reconnectcounter, fname);
    fEnableCache = readInt(L"EnableCache", fEnableCache, fname) != 0;
    fEnableZstd = readInt(L"EnableZstd", fEnableZstd, fname);
    quickoption = readInt(L"QuickOption", quickoption, fname);
    fUseDSMPlugin = readInt(L"UseDSMPlugin", fUseDSMPlugin, fname) != 0;
    m_connectionType = static_cast<ConnectionType>(readInt(L"UseProxy", static_cast<int>(m_connectionType), fname));
    allowMonitorSpanning = readInt(L"allowMonitorSpanning", allowMonitorSpanning, fname);
    changeServerRes = readInt(L"ChangeServerRes", changeServerRes, fname);
    extendDisplay = readInt(L"extendDisplay", extendDisplay, fname);
    showExtend = readInt(L"showExtend", showExtend, fname);
    use_virt = readInt(L"use_virt", use_virt, fname);
    useAllMonitors = readInt(L"useAllMonitors", useAllMonitors, fname);

    requestedWidth = readInt(L"requestedWidth", requestedWidth, fname);
    requestedHeight = readInt(L"requestedHeight", requestedHeight, fname);

    ::GetPrivateProfileStringW(L"options", L"DSMPlugin", L"NoPlugin", szDSMPluginFilename.data(), MAX_PATH, fname);
    ::GetPrivateProfileStringW(L"options", L"folder", folder.data(), folder.data(), static_cast<DWORD>(folder.size()), fname);
    ::GetPrivateProfileStringW(L"options", L"prefix", prefix, prefix, 56, fname);
    ::GetPrivateProfileStringW(L"options", L"imageFormat", imageFormat.data(), imageFormat.data(), 56, fname);
    ::GetPrivateProfileStringW(L"options", L"InfoMsg", reinterpret_cast<LPCWSTR>(InfoMsg.data()), reinterpret_cast<LPWSTR>(InfoMsg.data()), 254, fname);
    if (!g_disable_sponsor)
    {
        g_disable_sponsor = readInt(L"sponsor", g_disable_sponsor, fname) != 0;
    }
    autoReconnect = readInt(L"AutoReconnect", autoReconnect, fname);
    FTTimeout = readInt(L"FileTransferTimeout", FTTimeout, fname);
    if (FTTimeout > 600)
    {
        FTTimeout = 600;
    } // cap at 1 minute
    listenport = readInt(L"ListenPort", listenport, fname);
    keepAliveInterval = readInt(L"KeepAliveInterval", keepAliveInterval, fname);
    if (keepAliveInterval >= (FTTimeout - KEEPALIVE_HEADROOM))
    {
        keepAliveInterval = (FTTimeout - KEEPALIVE_HEADROOM);
    }
    throttleMouse = readInt(L"ThrottleMouse", throttleMouse, fname); // adzm 2010-10
#ifdef _Gii
    giiEnable = readInt(L"GiiEnable", static_cast<int>(giiEnable), fname) ? true : false;
#endif
    fAutoAcceptIncoming = readInt(L"AutoAcceptIncoming", static_cast<int>(fAutoAcceptIncoming), fname) ? true : false;
    fAutoAcceptNoDSM = readInt(L"AutoAcceptNoDSM", static_cast<int>(fAutoAcceptNoDSM), fname) ? true : false;
    fRequireEncryption = readInt(L"RequireEncryption", static_cast<int>(fRequireEncryption), fname) ? true : false;
    fUseTokenTLS = readInt(L"UseVeNCryptTLS", static_cast<int>(fUseTokenTLS), fname) ? true : false;
    preemptiveUpdates = readInt(L"PreemptiveUpdates", static_cast<int>(preemptiveUpdates), fname) ? true : false;

    ::GetPrivateProfileStringW(L"connection", L"proxyhost", L"", m_proxyhost.data(), static_cast<DWORD>(m_proxyhost.size()), fname);
    m_proxyport = static_cast<int>(::GetPrivateProfileIntW(L"connection", L"proxyport", 0, fname));
    overwriteCommandLine();
}

void SessionDialog::overwriteCommandLine()
{
    TCHAR* szCmdLine = reinterpret_cast<TCHAR*>(m_pOpt->szCmdLine.data());
    int cmdlinelen = static_cast<int>(::_tcslen(szCmdLine));
    if (cmdlinelen == 0)
    {
        return;
    }

    TCHAR* cmd = new TCHAR[static_cast<unsigned long long>(cmdlinelen + 1)];
    ::_tcscpy_s(cmd, static_cast<rsize_t>(cmdlinelen + 1), szCmdLine);

    // Count the number of spaces
    // This may be more than the number of arguments, but that doesn't matter.
    int nspaces{ 0 };
    TCHAR* p = cmd;
    TCHAR* pos = cmd;
    while ((pos = ::_tcschr(p, ' ')) != nullptr)
    {
        ++nspaces;
        p = pos + 1;
    }

    // Create the array to hold pointers to each bit of string
    TCHAR** args = new LPTSTR[static_cast<unsigned long long>(nspaces + 1)];

    // replace spaces with nulls and
    // create an array of TCHAR*'s which points to start of each bit.
    pos = cmd;
    int i{ 0 };
    args[i] = cmd;
    bool inquote{ false };
    for (pos = cmd; *pos != 0; ++pos)
    {
        // Arguments are normally separated by spaces, unless there's quoting
        if ((*pos == ' ') && !inquote)
        {
            *pos = '\0';
            p = pos + 1;
            args[++i] = p;
        }
        if (*pos == '"')
        {
            if (!inquote)
            {                    // Are we starting a quoted argument?
                args[i] = ++pos; // It starts just after the quote
            }
            else
            {
                *pos = '\0'; // Finish a quoted argument?
            }
            inquote = !inquote;
        }
    }
    ++i;

    [[maybe_unused]] const bool hostGiven{ false };
    [[maybe_unused]] const bool portGiven = false;
    // take in order.
    for (int j{ 0 }; j < i; ++j)
    {
        if (SwitchMatch(args[j], _T("listen")))
        {
            listening = true;
            if ((j + 1 < i) && (args[j + 1][0U] >= '0') && (args[j + 1][0U] <= '9'))
            {
                if (_stscanf_s(args[j + 1], _T("%d"), &listenport) != 1)
                {
                    continue;
                }
                ++j;
            }
        }
        else if (SwitchMatch(args[j], _T("fttimeout")))
        { //PGM @ Advantig
            if ((j + 1 < i) && (args[j + 1][0U] >= '0') && (args[j + 1][0U] <= '9'))
            {
                if (_stscanf_s(args[j + 1], _T("%d"), &FTTimeout) != 1)
                {
                    continue;
                }
                if (FTTimeout > 600)
                {
                    FTTimeout = 600;
                }
                ++j;
            }
        }
        else if (SwitchMatch(args[j], _T("keepalive")))
        { //PGM @ Advantig
            if ((j + 1 < i) && (args[j + 1][0U] >= '0') && (args[j + 1][0U] <= '9'))
            {
                if (_stscanf_s(args[j + 1], _T("%d"), &keepAliveInterval) != 1)
                {
                    continue;
                }
                if (keepAliveInterval >= (FTTimeout - KEEPALIVE_HEADROOM))
                {
                    keepAliveInterval = (FTTimeout - KEEPALIVE_HEADROOM);
                }
                ++j;
            }
        }
        else if (SwitchMatch(args[j], _T("socketkeepalivetimeout")))
        { // adzm 2010-08
            if ((j + 1 < i) && (args[j + 1][0U] >= '0') && (args[j + 1][0U] <= '9'))
            {
                int m_socketKeepAliveTimeout;
                if (_stscanf_s(args[j + 1], _T("%d"), &m_socketKeepAliveTimeout) != 1)
                {
                    continue;
                }
                ++j;
            }
        }
        else if (SwitchMatch(args[j], _T("askexit")))
        {                      //PGM @ Advantig
            fExitCheck = true; //PGM @ Advantig
        }
        else if (SwitchMatch(args[j], _T("restricted")))
        {
            restricted = true;
        }
        else if (SwitchMatch(args[j], _T("ipv6")))
        {
            ipv6 = true;
        }
        else if (SwitchMatch(args[j], _T("AllowUntrustedServers")))
        {
            AllowUntrustedServers = true;
        }
        else if (SwitchMatch(args[j], _T("viewonly")))
        {
            ViewOnly = true;
        }
        else if (SwitchMatch(args[j], _T("nostatus")))
        {
            NoStatus = true;
        }
        else if (SwitchMatch(args[j], _T("hideendofstreamerror")))
        {
            HideEndOfStreamError = true;
        }
        else if (SwitchMatch(args[j], _T("nohotkeys")))
        {
            NoHotKeys = true;
        }
        else if (SwitchMatch(args[j], _T("notoolbar")))
        {
            ShowToolbar = false;
        }
        else if (SwitchMatch(args[j], _T("autoscaling")))
        {
            fAutoScaling = true;
        }
        else if (SwitchMatch(args[j], _T("fullscreen")))
        {
            FullScreen = true;
        }
        else if (SwitchMatch(args[j], _T("savepos")))
        {
            SavePos = true;
        }
        else if (SwitchMatch(args[j], _T("savesize")))
        {
            SaveSize = true;
        }
        else if (SwitchMatch(args[j], _T("gnome")))
        {
            GNOME = true;
        }
        else if (SwitchMatch(args[j], _T("directx")))
        {
            Directx = true;
        }
        else if (SwitchMatch(args[j], _T("noauto")))
        {
            autoDetect = false;
            quickoption = 0;
        }
        else if (SwitchMatch(args[j], _T("8bit")))
        {
            Use8Bit = rfbPF256Colors; //true;
        }
        else if (SwitchMatch(args[j], _T("256colors")))
        {
            Use8Bit = rfbPF256Colors; //true;
        }
        else if (SwitchMatch(args[j], _T("fullcolors")))
        {
            Use8Bit = rfbPFFullColors;
        }
        else if (SwitchMatch(args[j], _T("64colors")))
        {
            Use8Bit = rfbPF64Colors;
        }
        else if (SwitchMatch(args[j], _T("8colors")))
        {
            Use8Bit = rfbPF8Colors;
        }
        else if (SwitchMatch(args[j], _T("8greycolors")))
        {
            Use8Bit = rfbPF8GreyColors;
        }
        else if (SwitchMatch(args[j], _T("4greycolors")))
        {
            Use8Bit = rfbPF4GreyColors;
        }
        else if (SwitchMatch(args[j], _T("2greycolors")))
        {
            Use8Bit = rfbPF2GreyColors;
        }
        else if (SwitchMatch(args[j], _T("shared")))
        {
            Shared = true;
        }
        else if (SwitchMatch(args[j], _T("swapmouse")))
        {
            SwapMouse = true;
        }
        else if (SwitchMatch(args[j], _T("emulate3")))
        {
            Emul3Buttons = true;
        }
        else if (SwitchMatch(args[j], _T("JapKeyboard")))
        {
            JapKeyboard = true;
        }
        else if (SwitchMatch(args[j], _T("noemulate3")))
        {
            Emul3Buttons = false;
        }
        else if (SwitchMatch(args[j], _T("nocursorshape")))
        {
            requestShapeUpdates = false;
        }
        else if (SwitchMatch(args[j], _T("noremotecursor")))
        {
            requestShapeUpdates = true;
            ignoreShapeUpdates = true;
        }
        else if (SwitchMatch(args[j], _T("scale")))
        {
            if (++j == i)
            {
                continue;
            }
            int numscales = _stscanf_s(args[j], _T("%d/%d"), &scale_num, &scale_den);
            if (numscales < 1)
            {
                continue;
            }
            if (numscales == 1)
            {
                scale_den = 1;
            } // needed if you're overriding a previous setting
        }
        else if (SwitchMatch(args[j], _T("disableclipboard")))
        {
            DisableClipboard = true;
        }
        else if (SwitchMatch(args[j], _T("InfoMsg")))
        {
            if (++j == i)
            {
                continue;
            }
            ::_tcscpy_s(InfoMsg.data(), InfoMsg.size(), args[j]);
        }

        else if (SwitchMatch(args[j], _T("register")))
        {
            //      Register();
            ::PostQuitMessage(0);
        }
        else if (SwitchMatch(args[j], _T("encoding")))
        {
            if (++j == i)
            {
                continue;
            }
            int enc = EncodingFromString(args[j]);
            if (enc == -1)
            {
                continue;
            }
            else
            {
                PreferredEncodings.clear();
                PreferredEncodings.push_back(enc);
                UseEnc[static_cast<unsigned long long>(enc)] = true;
            }
        }
        else if (SwitchMatch(args[j], _T("encodings")))
        {
            if (++j == i)
            {
                continue;
            }
            int encodings_found{ 0 };
            while (encodings_found >= 0)
            {
                int enc = EncodingFromString(args[j]);
                if (enc == -1)
                {
                    if (encodings_found == 0) {}
                    else
                    {
                        --j;
                    }
                    encodings_found = -1;
                }
                else
                {
                    if (encodings_found == 0)
                    {
                        PreferredEncodings.clear();
                    }
                    UseEnc[static_cast<unsigned long long>(enc)] = true;
                    if (PreferredEncodings.end() == std::find(PreferredEncodings.begin(), PreferredEncodings.end(), enc))
                    {
                        PreferredEncodings.push_back(enc);
                    }
                    ++encodings_found;

                    ++j;

                    if (j == i)
                    {
                        encodings_found = -1;
                    }
                }
            }
        }
        // Tight options
        else if (SwitchMatch(args[j], _T("compresslevel")))
        {
            if (++j == i)
            {
                continue;
            }
            useCompressLevel = true;
            if (_stscanf_s(args[j], _T("%d"), &compressLevel) != 1)
            {
                continue;
            }
        }
        else if (SwitchMatch(args[j], _T("quality")))
        {
            if (++j == i)
            {
                continue;
            }
            enableJpegCompression = true;
            if (_stscanf_s(args[j], _T("%d"), &jpegQualityLevel) != 1)
            {
                continue;
            }
        }
        else if (SwitchMatch(args[j], _T("serverscale")))
        {
            if (++j == i)
            {
                continue;
            }
            _stscanf_s(args[j], _T("%d"), &nServerScale);
            if ((nServerScale < 1) || (nServerScale > 9))
            {
                nServerScale = 1;
            }
        }
        // Modif sf@2002
        else if (SwitchMatch(args[j], _T("quickoption")))
        {
            if (++j == i)
            {
                continue;
            }
            _stscanf_s(args[j], _T("%d"), &quickoption);
        }
        // Modif sf@2002 - DSM Plugin
        else if (SwitchMatch(args[j], _T("dsmplugin")))
        {
            if (++j == i)
            {
                continue;
            }
            fUseDSMPlugin = true;
            ::_tcscpy_s(szDSMPluginFilename.data(), szDSMPluginFilename.size(), args[j]);
        }
        else if (SwitchMatch(args[j], _T("reconnectcounter")))
        {
            if (++j == i)
            {
                ::PostQuitMessage(1);
                continue;
            }
            _stscanf_s(args[j], _T("%d"), &reconnectcounter);
        }
        else if (SwitchMatch(args[j], _T("autoreconnect")))
        {
            if (++j == i)
            {
                ::PostQuitMessage(1);
                continue;
            }
            _stscanf_s(args[j], _T("%d"), &autoReconnect);
        }
        else if (SwitchMatch(args[j], _T("disablesponsor")))
        {
            //adzm - 2009-06-21
            g_disable_sponsor = true;
        }
        else if (SwitchMatch(args[j], _T("autoacceptincoming")))
        {
            //adzm - 2009-06-21
            fAutoAcceptIncoming = true;
        }
#ifdef _Gii
        else if (SwitchMatch(args[j], _T("giienable")))
        {
            giiEnable = true;
        }
#endif
        else if (SwitchMatch(args[j], _T("autoacceptnodsm")))
        {
            //adzm 2009-07-19
            fAutoAcceptNoDSM = true;
        }
        else if (SwitchMatch(args[j], _T("requireencryption")))
        {
            //adzm 2010-05-12
            fRequireEncryption = true;
        }
        else if (SwitchMatch(args[j], _T("preemptiveupdates")))
        {
            //adzm 2010-07-04
            preemptiveUpdates = true;
        }
        else if (SwitchMatch(args[j], _T("enablecache")))
        {
            //adzm 2010-08
            fEnableCache = true;
        }
        else if (SwitchMatch(args[j], _T("throttlemouse")))
        {
            //adzm 2010-10
            if (++j == i)
            {
                continue;
            }
            if (_stscanf_s(args[j], _T("%d"), &throttleMouse) != 1)
            {
                continue;
            }
        }
    }

    if ((scale_num != 1) || (scale_den != 1))
    {
        scaling = true;
    }

    // reduce scaling factors by greatest common denominator
    if (scaling)
    {
        FixScaling();
    }
    // tidy up
    delete[] cmd;
    delete[] args;
}

void SessionDialog::getAppData(wchar_t* const buffer)
{
    [[maybe_unused]] const BOOL result = ::SHGetSpecialFolderPathW(nullptr, buffer, CSIDL_APPDATA, false);
}

void SessionDialog::IfHostExistLoadSettings(wchar_t* const hostname)
{
    //SetDefaults();
    std::array<TCHAR, MAX_HOST_NAME_LEN> tmphost;
    int port;
    ParseDisplay(hostname, tmphost.data(), static_cast<int>(tmphost.size()), &port);
    std::array<wchar_t, _MAX_PATH> fname;
    int disp = PORT_TO_DISPLAY(port);
    ::swprintf_s(fname.data(), fname.size(), L"%.15s-%d.vnc", tmphost.data(), ((disp > 0) && (disp < 100)) ? disp : port);
    std::array<wchar_t, _MAX_PATH> buffer;
    getAppData(buffer.data());
    ::wcscat_s(buffer.data(), buffer.size(), L"\\UltraVNC\\");
    ::wcscat_s(buffer.data(), buffer.size(), fname.data());
    FILE* file = _wfopen(buffer.data(), L"r");
    bool fileExists = (file != nullptr);
    if (file)
    {
        std::fclose(file);
    }
    ::wcscpy_s(customConfigFile.data(), customConfigFile.size(), buffer.data());
    if ((std::wcslen(hostname) != 0) && fileExists && !m_pOpt->m_UseOnlyDefaultConfigFile)
    {
        SetDefaults();
        LoadFromFile(m_pOpt->getDefaultOptionsFileName());
        
        fUseOnlyDefaultConfigFile =
            readInt(L"UseOnlyDefaultConfigFile", static_cast<int>(fUseOnlyDefaultConfigFile), m_pOpt->getDefaultOptionsFileName())
                ? true
                : false;
        LoadFromFile(buffer.data());
        ::SetWindowText(::GetDlgItem(hTabConfig, IDC_CUSTOMCONFIG), customConfigFile.data());
    }
    else
    {
        LoadFromFile(m_pOpt->getDefaultOptionsFileName());
        fUseOnlyDefaultConfigFile =
            readInt(L"UseOnlyDefaultConfigFile", static_cast<int>(fUseOnlyDefaultConfigFile), m_pOpt->getDefaultOptionsFileName())
                ? true
                : false;
        // Always load connection-specific settings from per-host file
        if ((std::wcslen(hostname) != 0) && fileExists)
        {
            m_connectionType =
                static_cast<ConnectionType>(::GetPrivateProfileIntW(L"options", L"UseProxy", static_cast<int>(m_connectionType), buffer.data()));
            ::GetPrivateProfileStringW(L"connection", L"proxyhost", L"", m_proxyhost.data(), static_cast<DWORD>(m_proxyhost.size()), buffer.data());
            m_proxyport = static_cast<int>(::GetPrivateProfileIntW(L"connection", L"proxyport", 0, buffer.data()));
        }
        ::SetWindowText(::GetDlgItem(hTabConfig, IDC_CUSTOMCONFIG), _T(""));
    }
    InitDlgProcListen();
}

void SessionDialog::SetDefaults()
{
    SettingsFromUI();
    ViewOnly = false;
    FullScreen = false;
    SavePos = false;
    SaveSize = false;
    GNOME = false;
    Directx = false;
    autoDetect = false;
    Use8Bit = rfbPFFullColors; //false;
    ShowToolbar = true;
    NoStatus = false;
    NoHotKeys = false;
    PreferredEncodings.clear();
    PreferredEncodings.push_back(rfbEncodingUltra2);
    JapKeyboard = false;
    SwapMouse = false;
    Emul3Buttons = true;
    Shared = true;
    DisableClipboard = false;
    scaling = false;
    fAutoScaling = false;
    fAutoScalingEven = false;
    fAutoScalingLimit = false;
    scale_num = 100;
    scale_den = 100;
    // Modif sf@2002 - Server Scaling
    nServerScale = 1;
    reconnectcounter = 3;
    fEnableCache = false;
    fEnableZstd = true;
    listening = false;
    listenport = INCOMING_PORT_OFFSET;
    restricted = false;
    ipv6 = false;
    AllowUntrustedServers = false;
    // Tight specific
    useCompressLevel = true;
    compressLevel = 6;
    enableJpegCompression = true;
    jpegQualityLevel = 8;
    requestShapeUpdates = true;
    ignoreShapeUpdates = false;
    quickoption = 1; // sf@2002 - Auto Mode as default
    fUseDSMPlugin = false;
    oldplugin = false;
    allowMonitorSpanning = 0;
    changeServerRes = 0;
    extendDisplay = 0;
    showExtend = 0;
    use_virt = 0;
    useAllMonitors = 0;
    requestedWidth = 0;
    requestedHeight = 0;
    ::_tcscpy_s(prefix, _countof(prefix), _T("ultravnc_"));
    ::_tcscpy_s(imageFormat.data(), imageFormat.size(), _T(".jpeg"));
    fAutoAcceptIncoming = false;
    fAutoAcceptNoDSM = false;
    fRequireEncryption = false;
    fUseOnlyDefaultConfigFile = true;
    preemptiveUpdates = false;
    scale_num = 100;
    scale_den = 100;
    scaling = false;
    autoReconnect = 3;
    fExitCheck = false; //PGM @ Advantig
    FTTimeout = FT_RECV_TIMEOUT;
    keepAliveInterval = KEEPALIVE_INTERVAL;
    throttleMouse = 0; // adzm 2010-10
    setdefaults = true;
    SettingsToUI();
    setdefaults = false;
    giiEnable = false;
}
