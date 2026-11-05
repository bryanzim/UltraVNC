// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#pragma once
#include "stdhdrs.h"

#include "common/inifile.h"
#include "rfb.h"

#include <array>
#include <cstring>

#define MAXPWLEN 8
#define MAXMSPWLEN 32
#define MAX_HOST_NAME_LEN 250

class SettingsManager
{
public:
    static SettingsManager* getInstance();

    void Initialize(char* configFile);
    void load();
    void save();
    void savePassword();
    void saveViewOnlyPassword();
    BOOL getAllowProperties() { return m_pref_allowproperties; };
    BOOL getUseBridge() { return m_pref_fUseBridge; };
    BOOL getAllowInjection() { return m_pref_allowInjection; };
    BOOL getAllowShutdown() { return m_pref_allowshutdown; };
    BOOL getAllowEditClients() { return m_pref_alloweditclients; };

    BOOL getSecure() { return m_pref_Secure; };
    char* getService_commandline() { return m_pref_service_commandline.data(); };
    char* getAccept_reject_mesg() { return m_pref_accept_reject_mesg.data(); };
    BOOL getEnableFileTransfer() { return m_pref_EnableFileTransfer; };
    BOOL getFTUserImpersonation() { return m_pref_FTUserImpersonation; };
    char* getFileTransferRoot() { return m_pref_FileTransferRoot.data(); };
    BOOL getPrimary() { return m_pref_Primary; };
    BOOL getSecondary() { return m_pref_Secondary; };
    void setPrimary(const BOOL value) { m_pref_Primary = value; };
    void setSecondary(const BOOL value) { m_pref_Secondary = value; };
    BOOL getEnableBlankMonitor() { return m_pref_EnableBlankMonitor; };
    BOOL getBlankInputsOnly() { return m_pref_BlankInputsOnly; };
    int getDefaultScale() { return m_pref_DefaultScale; };
    UINT getQuerySetting() { return m_pref_QuerySetting; };
    UINT getQueryTimeout() { return m_pref_QueryTimeout; };
    UINT getQueryDisableTime() { return m_pref_QueryDisableTime; };
    UINT getQueryAccept() { return m_pref_QueryAccept; };
    UINT getMaxViewerSetting() { return m_pref_MaxViewerSetting; };
    UINT getMaxViewers() { return m_pref_MaxViewers; };
    BOOL getCollabo() { return m_pref_Collabo; };
    BOOL getFrame() { return m_pref_Frame; };
    BOOL getNotification() { return m_pref_Notification; };
    BOOL getOSD() { return m_pref_OSD; };
    int getNotificationSelection() { return m_pref_NotificationSelection; };
    UINT getIdleTimeout() { return m_pref_IdleTimeout; };
    BOOL getRemoveWallpaper() { return m_pref_RemoveWallpaper; };
    BOOL getRemoveFontSmoothing() { return m_pref_RemoveFontSmoothing; };
    BOOL getRemoveEffects() { return m_pref_RemoveEffects; };
    BOOL getEnableConnections() { return m_pref_EnableConnection; };
    BOOL getHTTPConnect() { return m_pref_EnableHTTPConnect; };
    void setEnableConnections(const bool value) { m_pref_EnableConnection = value; };
    void setHTTPConnect(const bool value) { m_pref_EnableHTTPConnect = value; };

    BOOL getEnableTLS() { return m_pref_EnableTLS; };
    void setEnableTLS(const BOOL v) { m_pref_EnableTLS = v; };
    LONG getTLSPort() { return m_pref_TLSPort; };
    void setTLSPort(const LONG v) { m_pref_TLSPort = v; };
    const char* getTLSCertThumbprint() { return m_pref_TLSCertThumbprint.data(); };
    void setTLSCertThumbprint(const char* const v)
    {
        ::strncpy_s(m_pref_TLSCertThumbprint.data(), m_pref_TLSCertThumbprint.size(), v, 40);
        m_pref_TLSCertThumbprint[40U] = 0;
    };

    // Native VeNCrypt TLS + token authentication
    BOOL getEnableVeNCryptTLS() { return m_pref_EnableVeNCryptTLS; };
    void setEnableVeNCryptTLS(const BOOL v) { m_pref_EnableVeNCryptTLS = v; };
    BOOL getVeNCryptAllowPlainAuth() { return m_pref_VeNCryptAllowPlainAuth; };
    void setVeNCryptAllowPlainAuth(const BOOL v) { m_pref_VeNCryptAllowPlainAuth = v; };
    BOOL getVeNCryptAllowLegacyAuth() { return m_pref_VeNCryptAllowLegacyAuth; };
    void setVeNCryptAllowLegacyAuth(const BOOL v) { m_pref_VeNCryptAllowLegacyAuth = v; };
    BOOL getVeNCryptRequireChannelBinding() { return m_pref_VeNCryptRequireChannelBinding; };
    void setVeNCryptRequireChannelBinding(const BOOL v) { m_pref_VeNCryptRequireChannelBinding = v; };
    DWORD getTLSCertKeyBits()
    { return ((m_pref_TLSCertKeyBits == 2048) || (m_pref_TLSCertKeyBits == 4096)) ? m_pref_TLSCertKeyBits : 3072; };
    void setTLSCertKeyBits(const DWORD v) { m_pref_TLSCertKeyBits = v; };
    const char* getVeNCryptTLSCertThumbprint() { return m_pref_VeNCryptTLSCertThumbprint.data(); };
    void setVeNCryptTLSCertThumbprint(const char* const v)
    {
        ::strncpy_s(m_pref_VeNCryptTLSCertThumbprint.data(), m_pref_VeNCryptTLSCertThumbprint.size(), v, 40);
        m_pref_VeNCryptTLSCertThumbprint[40U] = 0;
    };
    const char* getVeNCryptTokenDLLPath() { return m_pref_VeNCryptTokenDLLPath.data(); };
    void setVeNCryptTokenDLLPath(const char* const v)
    {
        ::strncpy_s(m_pref_VeNCryptTokenDLLPath.data(), m_pref_VeNCryptTokenDLLPath.size(), v, MAX_PATH - 1);
        m_pref_VeNCryptTokenDLLPath[MAX_PATH - 1] = 0;
    };
    const char* getVeNCryptSubtypes() { return m_pref_VeNCryptSubtypes.data(); };
    void setVeNCryptSubtypes(const char* const v)
    {
        ::strncpy_s(m_pref_VeNCryptSubtypes.data(), m_pref_VeNCryptSubtypes.size(), v, sizeof(m_pref_VeNCryptSubtypes) - 1);
        m_pref_VeNCryptSubtypes[sizeof(m_pref_VeNCryptSubtypes) - 1] = 0;
    };
    // RA2 / RA2ne (RealVNC RSA-AES security types 5/6/129/130), reusing the
    // VeNCrypt certificate RSA key as the server identity.
    BOOL getEnableRA2() { return m_pref_EnableRA2; };
    void setEnableRA2(const BOOL v) { m_pref_EnableRA2 = v; };
    BOOL getEnableRA2ne() { return m_pref_EnableRA2ne; };
    void setEnableRA2ne(const BOOL v) { m_pref_EnableRA2ne = v; };
    BOOL getEnableRemoteInputs() { return m_pref_EnableRemoteInputs; };
    BOOL getDisableLocalInputs() { return m_pref_DisableLocalInputs; };
    BOOL getEnableJapInput() { return m_pref_EnableJapInput; };
    BOOL getForceCursorShape() { return m_pref_ForceCursorShape; };
    BOOL getEnableUnicodeInput() { return m_pref_EnableUnicodeInput; };
    BOOL getEnableWin8Helper() { return m_pref_EnableWin8Helper; };
    BOOL getClearconsole() { return m_pref_clearconsole; };
    char* getPasswd() { return m_pref_passwd.data(); };
    char* getPasswdViewOnly() { return m_pref_passwdViewOnly.data(); };
    bool isPasswdSet() const;
    bool isPasswdViewOnlySet() const;
    BOOL getAutoPortSelect() { return m_pref_AutoPortSelect; };
    LONG getPortNumber() { return m_pref_PortNumber; };
    LONG getHttpPortNumber() { return m_pref_HttpPortNumber; };
    int getLockSettings() { return m_pref_LockSettings; };
    char* getDSMPluginConfig() { return m_pref_DSMPluginConfig.data(); };
    char* getSzDSMPlugin() { return m_pref_szDSMPlugin.data(); };
    BOOL getUseDSMPlugin() { return m_pref_UseDSMPlugin; };
    void setService_commandline(char* const value) { ::strcpy_s(m_pref_service_commandline.data(), m_pref_service_commandline.size(), value); };
    void setAccept_reject_mesg(char* const value) { ::strcpy_s(m_pref_accept_reject_mesg.data(), m_pref_accept_reject_mesg.size(), value); };
    BOOL getDebugMode() { return m_pref_DebugMode; };
    void setDebugMode(const BOOL value) { m_pref_DebugMode = value; };

    BOOL getDebugLevel() { return m_pref_DebugLevel; };
    BOOL getAvilog() { return m_pref_Avilog; };
    char* getDebugPath() { return m_pref_DebugPath.data(); };
    BOOL getAllowLoopback() { return m_pref_AllowLoopback; };
    BOOL getAuthRequired() { return m_pref_AuthRequired; };
    void setAuthRequired(const BOOL value) { m_pref_AuthRequired = value; };
    BOOL getAllowLoopbackWithoutDSM() { return m_pref_AllowLoopbackWithoutDSM; };
    void setAllowLoopbackWithoutDSM(const BOOL value) { m_pref_AllowLoopbackWithoutDSM = value; };
    BOOL getAllowLoopbackWithoutAuth() { return m_pref_AllowLoopbackWithoutAuth; };
    void setAllowLoopbackWithoutAuth(const BOOL value) { m_pref_AllowLoopbackWithoutAuth = value; };
    char* getAuthhosts() { return m_pref_authhosts.data(); };
    BOOL getDisableTrayIcon() { return m_pref_DisableTrayIcon; };
    BOOL getRdpmode() { return m_pref_Rdpmode; };
    BOOL getnoscreensaver() { return m_pref_Noscreensaver; };
    BOOL getLoopbackOnly() { return m_pref_LoopbackOnly; };
    int getConnectPriority() { return m_pref_ConnectPriority; };
    BOOL getRequireMSLogon() { return m_pref_RequireMSLogon; };
    BOOL getRequireMSLogonIII() { return m_pref_RequireMSLogonIII; };
    BOOL getNewMSLogon() { return m_pref_NewMSLogon; };
    BOOL getReverseAuthRequired() { return m_pref_ReverseAuthRequired; };
    int getftTimeout() { return m_pref_ftTimeout; };
    int getkeepAliveInterval() { return m_pref_keepAliveInterval; };
    int getIdleInputTimeout() { return m_pref_IdleInputTimeout; };
    BOOL getQueryIfNoLogon() { return m_pref_QueryIfNoLogon; };
    UINT getSENDBUFFER_EX() { return G_SENDBUFFER_EX; };

    void setQueryIfNoLogon(const BOOL value) { m_pref_QueryIfNoLogon = value; };
    void setAllowProperties(const BOOL value) { m_pref_allowproperties = value; };
    void setUseBridge(const BOOL value) { m_pref_fUseBridge = value; };
    void setAllowInjection(const BOOL value) { m_pref_allowInjection = value; };
    void setAllowShutdown(const BOOL value) { m_pref_allowshutdown = value; };
    void setAllowEditClients(const BOOL value) { m_pref_alloweditclients = value; };
    void setSecure(const BOOL value) { m_pref_Secure = value; };
    void setDisableTrayIcon(const BOOL value) { m_pref_DisableTrayIcon = value; };
    void setRdpmode(const BOOL value) { m_pref_Rdpmode = value; };
    void setNoScreensaver(const BOOL value) { m_pref_Noscreensaver = value; };
    void setEnableFileTransfer(const BOOL value) { m_pref_EnableFileTransfer = value; };
    void setFTUserImpersonation(const BOOL value) { m_pref_FTUserImpersonation = value; };
    void setFileTransferRoot(char* const value) { ::strcpy_s(m_pref_FileTransferRoot.data(), m_pref_FileTransferRoot.size(), value); };
    void setAutoPortSelect(const BOOL value) { m_pref_AutoPortSelect = value; };
    void setPortNumber(const LONG port) { m_pref_PortNumber = port; };
    void setHttpPortNumber(const LONG port) { m_pref_HttpPortNumber = port; };


    void setAuthhosts(char* const value) { ::strcpy_s(m_pref_authhosts.data(), m_pref_authhosts.size(), value); };

    void setEnableRemoteInputs(const BOOL value) { m_pref_EnableRemoteInputs = value; };
    void setDisableLocalInputs(const BOOL value) { m_pref_DisableLocalInputs = value; };
    void setEnableJapInput(const BOOL value) { m_pref_EnableJapInput = value; };
    void setForceCursorShape(const BOOL value) { m_pref_ForceCursorShape = value; };
    void setEnableUnicodeInput(const BOOL value) { m_pref_EnableUnicodeInput = value; };
    void setLoopbackOnly(const BOOL value)
    {
        m_pref_LoopbackOnly = value;
        if (value)
        {
            setAllowLoopback(true);
        }
    };
    void setAllowLoopback(const BOOL value) { m_pref_AllowLoopback = value; };
    void setQuerySetting(const UINT value) { m_pref_QuerySetting = value; };
    void setConnectPriority(const int value) { m_pref_ConnectPriority = value; };
    void setQueryDisableTime(const UINT value) { m_pref_QueryDisableTime = value; };
    void setQueryTimeout(const UINT value) { m_pref_QueryTimeout = value; };
    void setDebugPath(char* const value) { ::strcpy_s(m_pref_DebugPath.data(), m_pref_DebugPath.size(), value); };

    void setMaxViewers(const UINT value) { m_pref_MaxViewers = value; };
    void setCollabo(const BOOL value) { m_pref_Collabo = value; };
    void setMaxViewerSetting(const UINT value) { m_pref_MaxViewerSetting = value; };
    void setQueryAccept(const UINT value) { m_pref_QueryAccept = value; };

    void setLockSettings(const int value) { m_pref_LockSettings = value; };
    void setRemoveWallpaper(const BOOL value) { m_pref_RemoveWallpaper = value; };
    void setEnableWin8Helper(const BOOL value) { m_pref_EnableWin8Helper = value; };
    void setFrame(const BOOL value) { m_pref_Frame = value; };
    void setNotification(const BOOL value) { m_pref_Notification = value; };
    void setNotificationSelection(const int value) { m_pref_NotificationSelection = value; };
    void setOSD(const BOOL value) { m_pref_OSD = value; };

    void setRequireMSLogon(const BOOL value) { m_pref_RequireMSLogon = value; };
    void setRequireMSLogonIII(const BOOL value) { m_pref_RequireMSLogonIII = value; };
    void setNewMSLogon(const BOOL value) { m_pref_NewMSLogon = value; };
    void setReverseAuthRequired(const BOOL value) { m_pref_ReverseAuthRequired = value; };

    void setEnableBlankMonitor(const BOOL value) { m_pref_EnableBlankMonitor = value; };
    void setBlankInputsOnly(const BOOL value) { m_pref_BlankInputsOnly = value; };

    void setPasswd(const char* passwd)
    {
        std::memset(m_pref_passwd.data(), 0, sizeof(m_pref_passwd));
        if (passwd && (passwd[0U] != '\0'))
        {
            std::memcpy(m_pref_passwd.data(), passwd, MAXPWLEN);
        }
    }
    void setPasswdViewOnly(const char* passwd)
    {
        std::memset(m_pref_passwdViewOnly.data(), 0, sizeof(m_pref_passwdViewOnly));
        if (passwd && (passwd[0U] != '\0'))
        {
            std::memcpy(m_pref_passwdViewOnly.data(), passwd, MAXPWLEN);
        }
    }

    void setDSMPluginConfig(char* const value)
    { ::strncpy_s(m_pref_DSMPluginConfig.data(), sizeof(m_pref_DSMPluginConfig) - 1, value, _TRUNCATE); };
    void setSzDSMPlugin(char* const value) { ::strncpy_s(m_pref_szDSMPlugin.data(), sizeof(m_pref_szDSMPlugin) - 1, value, _TRUNCATE); };
    void setUseDSMPlugin(const BOOL value) { m_pref_UseDSMPlugin = value; };
    void setDefaultScale(const int value) { m_pref_DefaultScale = value; };

    void setkeepAliveInterval(int secs);
    void setIdleTimeout(int secs);
    void setIdleInputTimeout(int secs);
    void setftTimeout(const int value) { m_pref_ftTimeout = value; };

    void EnableServerStateUpdates(const bool newstate) { m_pref_fEnableStateUpdates = newstate; }
    bool DoServerStateUpdates() { return m_pref_fEnableStateUpdates; }
    void EnableKeepAlives(const bool newstate) { m_pref_fEnableKeepAlive = newstate; }
    bool DoKeepAlives() { return m_pref_fEnableKeepAlive; }
    BOOL RunningFromExternalService() { return m_pref_fRunningFromExternalService; };
    void setRunningFromExternalService(BOOL fEnabled);
    BOOL RunningFromExternalServiceRdp() { return m_pref_fRunningFromExternalServiceRdp; };
    void setRunningFromExternalServiceRdp(const BOOL fEnabled) { m_pref_fRunningFromExternalServiceRdp = fEnabled; };
    void AutoRestartFlag(const BOOL fOn) { m_pref_fAutoRestart = fOn; };
    BOOL AutoRestartFlag() { return m_pref_fAutoRestart; };

    void setScExit(const BOOL value) { m_pref_ScExit = value; };
    void setScPrompt(const BOOL value) { m_pref_ScPrompt = value; };
    BOOL getScExit() { return m_pref_ScExit; };
    BOOL getScPrompt() { return m_pref_ScPrompt; };

    void setTurboMode(const BOOL value) { m_pref_TurboMode = value; };
    BOOL getTurboMode() { return m_pref_TurboMode; };
    void setPollUnderCursor(const BOOL value) { m_pref_PollUnderCursor = value; };
    BOOL getPollUnderCursor() { return m_pref_PollUnderCursor; };
    void setPollForeground(const BOOL value) { m_pref_PollForeground = value; };
    BOOL getPollForeground() { return m_pref_PollForeground; };

    void setPollFullScreen(const BOOL value) { m_pref_PollFullScreen = value; };
    BOOL getPollFullScreen() { return m_pref_PollFullScreen; };

    void setPollConsoleOnly(const BOOL value) { m_pref_PollConsoleOnly = value; };
    BOOL getPollConsoleOnly() { return m_pref_PollConsoleOnly; };

    void setPollOnEventOnly(const BOOL value) { m_pref_PollOnEventOnly = value; };
    BOOL getPollOnEventOnly() { return m_pref_PollOnEventOnly; };

    void setVirtual(const BOOL value) { m_pref_Virtual = value; };
    BOOL getVirtual() { return m_pref_Virtual; };

    void setHook(const BOOL value) { m_pref_Hook = value; };
    BOOL getHook() { return m_pref_Hook; };

    void setDriver(const BOOL value) { m_pref_Driver = value; };
    BOOL getDriver() { return m_pref_Driver; };

    void setddEngine(const BOOL value) { m_pref_ddEngine = value; };
    BOOL getddEngine() { return m_pref_ddEngine; };

    void setMaxCpu(const int value) { m_pref_MaxCpu = value; };
    int getMaxCpu() { return m_pref_MaxCpu; };
    void setMaxFPS(const int value) { m_pref_MaxFPS = value; };
    int getMaxFPS() { return m_pref_MaxFPS; };
    void setAutocapt(const BOOL value) { m_pref_autocapt = value; };
    BOOL getAutocapt() { return m_pref_autocapt; };

    void setGroup1(TCHAR* const value) { ::strcpy_s(m_pref_group1.data(), m_pref_group1.size(), value); };
    TCHAR* getGroup1() { return m_pref_group1.data(); };
    void setGroup2(TCHAR* const value) { ::strcpy_s(m_pref_group2.data(), m_pref_group2.size(), value); };
    TCHAR* getGroup2() { return m_pref_group2.data(); };
    void setGroup3(TCHAR* const value) { ::strcpy_s(m_pref_group3.data(), m_pref_group3.size(), value); };
    TCHAR* getGroup3() { return m_pref_group3.data(); };

    void setLocdom1(const BOOL value) { m_pref_locdom1 = value; };
    BOOL getLocdom1() { return m_pref_locdom1; };
    void setLocdom2(const BOOL value) { m_pref_locdom2 = value; };
    BOOL getLocdom2() { return m_pref_locdom2; };
    void setLocdom3(const BOOL value) { m_pref_locdom3 = value; };
    BOOL getLocdom3() { return m_pref_locdom3; };

    void setCloudEnabled(const BOOL value) { m_pref_cloudEnabled = value; };
    BOOL getCloudEnabled() { return m_pref_cloudEnabled; };

    void setCloudServer(TCHAR* const value) { ::strcpy_s(m_pref_cloudServer.data(), m_pref_cloudServer.size(), value); };
    TCHAR* getCloudServer() { return m_pref_cloudServer.data(); };


    // Whether or not to allow connections from the local machine
    void setIPV6(const BOOL ok) { m_pref_ipv6_allowed = ok; };
    BOOL getIPV6() { return m_pref_ipv6_allowed; };

    bool IsRunninAsAdministrator();
    bool IsDesktopUserAdmin();
    bool getAllowUserSettingsWithPassword();
    void setAllowUserSettingsWithPassword(bool value);
    bool checkAdminPassword();
    void setAdminPasswordHash(char* password);
    bool isAdminPasswordSet();
    void setShowAllLogs(const bool value) { showAllLogs = value; }
    bool getShowAllLogs() { return showAllLogs; }

    char* getAlternateShell() { return m_pref_alternateShell.data(); };

    bool getShowSettings();
    void setShowSettings(const bool value) { showSettings = value; };
    char* getLogFile() { return logfile.data(); };
    void setLogFile(char* const value) { ::strcpy_s(logfile.data(), logfile.size(), value); };

private:
    explicit SettingsManager();
    static SettingsManager* s_instance;
    void setDefaults();
    void initTemp();
    IniFile iniFile;

    BOOL m_pref_fUseBridge;
    BOOL m_pref_allowproperties;
    BOOL m_pref_allowInjection;
    BOOL m_pref_allowshutdown;
    BOOL m_pref_alloweditclients;
    int m_pref_ftTimeout;
    int m_pref_keepAliveInterval;
    int m_pref_IdleInputTimeout;
    std::array<char, 1024U> m_pref_service_commandline;
    std::array<char, 512U> m_pref_accept_reject_mesg;
    BOOL m_pref_EnableConnection;
    BOOL m_pref_EnableHTTPConnect;

    BOOL m_pref_EnableTLS;
    LONG m_pref_TLSPort;
    std::array<char, 41U> m_pref_TLSCertThumbprint;
    DWORD m_pref_TLSCertKeyBits;

    BOOL m_pref_EnableVeNCryptTLS;
    BOOL m_pref_VeNCryptAllowPlainAuth;
    BOOL m_pref_VeNCryptAllowLegacyAuth;
    BOOL m_pref_VeNCryptRequireChannelBinding;
    std::array<char, 41U> m_pref_VeNCryptTLSCertThumbprint;
    std::array<char, MAX_PATH> m_pref_VeNCryptTokenDLLPath;
    std::array<char, 256U> m_pref_VeNCryptSubtypes;
    BOOL m_pref_EnableRA2;
    BOOL m_pref_EnableRA2ne;

    BOOL m_pref_AutoPortSelect;
    LONG m_pref_PortNumber;
    LONG m_pref_HttpPortNumber;
    std::array<char, MAXPWLEN> m_pref_passwd;
    std::array<char, MAXPWLEN> m_pref_passwdViewOnly;
    UINT m_pref_QuerySetting;
    UINT m_pref_QueryIfNoLogon;
    UINT m_pref_QueryAccept;
    UINT m_pref_QueryTimeout;
    UINT m_pref_QueryDisableTime;
    UINT m_pref_IdleTimeout;
    UINT m_pref_MaxViewerSetting;
    UINT m_pref_MaxViewers;
    BOOL m_pref_Collabo;
    BOOL m_pref_RemoveWallpaper;
    BOOL m_pref_RemoveEffects;
    BOOL m_pref_RemoveFontSmoothing;
    BOOL m_pref_EnableRemoteInputs;
    int m_pref_LockSettings;
    BOOL m_pref_DisableLocalInputs;
    BOOL m_pref_EnableJapInput;
    BOOL m_pref_ForceCursorShape;
    BOOL m_pref_EnableUnicodeInput;
    BOOL m_pref_EnableWin8Helper;
    BOOL m_pref_clearconsole;
    BOOL m_pref_EnableFileTransfer;
    BOOL m_pref_FTUserImpersonation;
    std::array<char, MAX_PATH * 4> m_pref_FileTransferRoot;
    BOOL m_pref_EnableBlankMonitor;
    BOOL m_pref_BlankInputsOnly;
    int m_pref_DefaultScale;
    BOOL m_pref_RequireMSLogon;
    BOOL m_pref_RequireMSLogonIII; // Enforce only MS-Logon III (no legacy II)
    BOOL m_pref_Secure;
    BOOL m_pref_NewMSLogon;
    BOOL m_pref_ReverseAuthRequired;
    BOOL m_pref_UseDSMPlugin;
    std::array<char, 128U> m_pref_szDSMPlugin;
    std::array<char, 512U> m_pref_DSMPluginConfig;
    BOOL m_pref_Primary;
    BOOL m_pref_Secondary;
    BOOL m_pref_Frame;
    BOOL m_pref_Notification;
    BOOL m_pref_OSD;
    int m_pref_NotificationSelection;
    BOOL m_pref_DisableTrayIcon;
    BOOL m_pref_Rdpmode;
    BOOL m_pref_Noscreensaver;
    BOOL m_pref_LoopbackOnly;
    BOOL m_pref_AllowLoopback;
    BOOL m_pref_AuthRequired;
    BOOL m_pref_AllowLoopbackWithoutDSM;
    BOOL m_pref_AllowLoopbackWithoutAuth;
    int m_pref_ConnectPriority;
    std::array<char, 1280U> m_pref_authhosts;
    std::array<char, 1280U> m_pref_authhosts2;

    BOOL m_pref_DebugMode;
    std::array<char, 512U> m_pref_DebugPath;
    BOOL m_pref_DebugLevel;
    BOOL m_pref_Avilog;
    BOOL m_pref_UseIpv6;
    unsigned int G_SENDBUFFER_EX;
    std::array<char, MAX_PATH> logfile;
    bool m_pref_fEnableStateUpdates;
    bool m_pref_fEnableKeepAlive;
    BOOL m_pref_fRunningFromExternalService;
    BOOL m_pref_fRunningFromExternalServiceRdp;
    BOOL m_pref_fAutoRestart;

    bool m_pref_ScExit;
    bool m_pref_ScPrompt;

    BOOL m_pref_TurboMode;
    BOOL m_pref_PollUnderCursor;
    BOOL m_pref_PollForeground;
    BOOL m_pref_PollFullScreen;
    BOOL m_pref_PollConsoleOnly;
    BOOL m_pref_PollOnEventOnly;
    LONG m_pref_MaxCpu;
    LONG m_pref_MaxFPS;
    BOOL m_pref_Driver;
    BOOL m_pref_Hook;
    BOOL m_pref_Virtual;
    bool m_pref_ddEngine;
    int m_pref_autocapt;
    BOOL m_pref_ipv6_allowed;
    bool m_pref_RunninAsAdministrator;

    std::array<char, 150U> m_pref_group1;
    std::array<char, 150U> m_pref_group2;
    std::array<char, 150U> m_pref_group3;

    BOOL m_pref_locdom1;
    BOOL m_pref_locdom2;
    BOOL m_pref_locdom3;

    std::array<TCHAR, MAX_HOST_NAME_LEN> m_pref_cloudServer;
    std::array<char, 1024U> m_pref_alternateShell;
    bool m_pref_cloudEnabled;
    bool m_pref_AllowUserSettingsWithPassword;
    bool showAllLogs{ false };
    bool showSettings{ false };
    std::array<char, 16U> m_pref_language;

public:
    void setLanguage(const char* const value)
    {
        ::strncpy_s(m_pref_language.data(), m_pref_language.size(), value, 15);
        m_pref_language[15U] = '\0';
    }
    const char* getLanguage() { return m_pref_language.data(); }
};

extern SettingsManager* settings;
