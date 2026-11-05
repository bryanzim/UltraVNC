// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#ifndef VNCOPTIONS_H__
#define VNCOPTIONS_H__

#define MAX_HOST_NAME_LEN 250
#pragma once

#include <array>
#include <vector>

#ifdef _XZ
#define LASTENCODING rfbEncodingZSTDYWRLE
#else
#define LASTENCODING rfbEncodingZSTDYWRLE
#endif
#define NOCURSOR 0
#define DOTCURSOR 1
#define NORMALCURSOR 2

// Connection type enumeration
enum ConnectionType
{
    DIRECT_TCP = 0,      // Direct TCP connection
    REPEATER_SERVER = 1, // Repeater server (proxy mode)
};

inline bool SwitchMatch(LPCTSTR arg, const LPCTSTR swtch)
{ return ((arg[0U] == '-') || (arg[0U] == '/')) && (::_tcsicmp(&arg[1U], swtch) == 0); }

class VNCOptions
{
public:
    explicit VNCOptions();
    VNCOptions& operator=(VNCOptions& s);
    virtual ~VNCOptions();

    // Save and load a set of options from a config file
    void SaveOptions(const wchar_t* fname);
    void LoadOptions(const wchar_t* fname);

    // process options
    bool m_listening;
    int m_listenPort;
    std::array<TCHAR, 1280U> m_listenAuthHosts;
    bool m_connectionSpecified;
    bool m_configSpecified;
    std::array<char, 256U> m_cmdlnUser;     // act: add user option on command line
    std::array<char, 256U> m_clearPassword; // Modif sf@2002
    std::array<char, 65U> m_token;          // VeNCrypt token (64 ASCII chars + null)
    std::array<char, 65U> m_sessionToken;   // Server-issued session token for reconnect (not persisted)
    bool m_fUseTokenTLS;        // Prefer VeNCrypt token-TLS auth
    int m_quickoption;          // Modif sf@2002 - v1.1.2
    std::array<TCHAR, _MAX_PATH> m_configFilename;
    bool m_restricted;
    bool m_ipv6;
    bool m_AllowUntrustedServers;

    // default connection options - can be set through Dialog
    bool m_ViewOnly;
    bool m_NoStatus;
    bool m_NoHotKeys;
    bool m_FullScreen;
    std::array<TCHAR, 32U> m_language; // Language preference (en, fr, de, es)
    bool m_SavePos;
    bool m_SaveSize;
    bool m_Directx;
    bool m_cmdDirectx;
    bool m_fKeepAspect;
    bool m_ShowToolbar;
    bool m_GNOME;

    bool autoDetect;
    int m_Use8Bit;
    std::vector<int> m_PreferredEncodings;

    bool m_SwapMouse;
    bool m_BlockSameMouse;
    bool m_Emul3Buttons;
    bool m_JapKeyboard;
    int m_Emul3Timeout;
    int m_Emul3Fuzz;
    bool m_Shared;
    bool m_NoBorder;
    bool m_DeiconifyOnBell;
    bool m_DisableClipboard;
    int m_localCursor;
    int m_throttleMouse; // adzm 2010-10
    bool m_scaling;
    bool m_fAutoScaling;
    bool m_fAutoScalingEven;
    bool m_fAutoScalingLimit;
    int m_scale_num, m_scale_den; // Numerator & denominator
    // Tight specific
    bool m_useCompressLevel;
    int m_compressLevel;
    bool m_enableJpegCompression;
    int m_jpegQualityLevel;
    bool m_requestShapeUpdates;
    bool m_ignoreShapeUpdates;

    // Modif sf@2002 - Server Scaling
    int m_nServerScale; // Divider of the Target Server's screensize
    int m_reconnectcounter;
    int m_x, m_y, m_w, m_h;
    bool m_fEnableCache;
    bool m_fEnableZstd;
    bool m_fUseDSMPlugin;
    std::array<wchar_t, _MAX_PATH> m_szDSMPluginFilename;
    bool m_oldplugin;
    int m_saved_scale_num;
    int m_saved_scale_den;
    bool m_saved_scaling;
    std::array<TCHAR, 9U> m_kbdname;
    bool m_kbdSpecified;
    std::array<bool, LASTENCODING + 1> m_UseEnc;
    std::array<TCHAR, MAX_HOST_NAME_LEN> m_host_options;
    int m_port;
    std::array<TCHAR, MAX_HOST_NAME_LEN> m_proxyhost;
    int m_proxyport;
    ConnectionType m_connectionType;
    bool m_allowMonitorSpanning;
    bool m_ChangeServerRes;
    bool m_extendDisplay;
    bool m_showExtend;
    bool m_use_virt;
    bool m_useAllMonitors;
    int m_requestedWidth;
    int m_requestedHeight;

    int m_logLevel;
    bool m_logToFile, m_logToConsole;
    std::array<TCHAR, _MAX_PATH> m_logFilename;
    int m_delay;
    int m_autoReconnect;
    bool m_NoMoreCommandLineUserPassword;
    bool m_fExitCheck; //PGM @ Advantig
    int m_FTTimeout;
    int m_keepAliveInterval;
    int m_IdleInterval;
    bool m_fAutoAcceptIncoming;
    bool m_fAutoAcceptNoDSM;
    bool m_fRequireEncryption;
    bool m_UseOnlyDefaultConfigFile;
    bool m_preemptiveUpdates;
    std::array<WCHAR, MAX_PATH * 2> m_szUploadLocal;
    std::array<WCHAR, MAX_PATH * 2> m_szUploadRemote;
    void CheckProxyAndHost();
#ifdef _Gii
    bool m_giiEnable;
#endif

    int DoDialog(bool running = false, HWND hwnd = nullptr);
    void SetFromCommandLine(LPTSTR szCmdLine);
    std::array<wchar_t, 8191U> szCmdLine{};

    void CancelDialog();
    void setDefaultDocumentPath();
    static BOOL CALLBACK OptDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    std::array<wchar_t, MAX_PATH> m_document_folder;
    wchar_t m_prefix[56U];
    std::array<wchar_t, 56U> m_imageFormat;
    bool m_running;

    wchar_t m_optionfile[MAX_PATH];
    static void setDefaultOptionsFileName(wchar_t* optionfile);
    wchar_t* getDefaultOptionsFileName();
    std::array<wchar_t, 255U> m_InfoMsg;
    std::array<wchar_t, 255U> m_ClassName;

    bool m_HideEndOfStreamError;

private:
    void ShowUsage(LPTSTR info = nullptr);
    void FixScaling();

    HWND hwnd;
};

#endif //VNCOPTIONS_H__
