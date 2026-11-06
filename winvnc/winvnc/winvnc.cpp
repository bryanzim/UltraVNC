// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// WinVNC.cpp

// 24/11/97		WEZ

// WinMain and main WndProc for the new version of UltraVNC Server
////////////////////////////
// System headers
#include "stdhdrs.h"

#include "winvnc.h"

#include "cadthread.h"
#include "videodriver.h"
#include "VirtualDisplay.h"
#include "vncauth.h"
#include "vncinsthandler.h"
#include "vncmenu.h"
#include "vncOSVersion.h"
#include "vncserver.h"
////////////////////////////
// Custom headers
#include "vsocket.h"

#include <intrin.h>
#include <mmsystem.h>

#include <array>
#include <cstddef>
#include <cstring>

#define LOCALIZATION_MESSAGES
#include "credentials.h"
#include "Localization.h" // Act : add localization on messages
#include "ScSelect.h"
#include "SettingsManager.h"
#include "UltraVNCService.h"

#include <commctrl.h>
#include <direct.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <cctype>
#include <cstdio>
#include <errno.h>
#include <fstream>

#pragma comment(lib, "comctl32")

// Application instance and name
HINSTANCE hAppInstance;
const char* szAppName = "WinVNC";
DWORD mainthreadId;
char configFile[MAX_PATH] = { 0 };
int configfileskip{ 0 };
bool showSettings{ false };
std::array<char, MAX_PATH> winvncFolder;

// Constants for portable mode and configuration
const char* PORTABLE_MARKER_FILE = "ultravnc.portable";
const char* CONFIG_SUBFOLDER = "\\UltraVNC";

//adzm 2009-06-20
char* g_szRepeaterHost{ nullptr };

// sf@2007 - New shutdown order handling stuff (with uvnc_service)
bool fShutdownOrdered{ false };
static HANDLE hShutdownEvent = nullptr;
HANDLE hShutdownEventcad = nullptr;
MMRESULT mmRes;

void WRITETOLOG(char* szText, int size, DWORD* byteswritten, void*);

//// Handle Old PostAdd message
bool PostAddAutoConnectClient_bool{ false };
bool PostAddNewClient_bool{ false };
bool PostAddAutoConnectClient_bool_null{ false };
bool PostAddConnectClient_bool{ false };
bool PostAddConnectClient_bool_null{ false };
bool PostAddNewRepeaterClient_bool{ false };

std::array<char, 20U> pszId_char;
VCard32 address_vcard4;
in6_addr address_in6;
VCard32 address_vcard;
int port_int;


// [v1.0.2-jp1 fix] Load resouce from dll
HINSTANCE hInstResDLL;
//BOOL G_HTTP;

void Shellexecuteforuiaccess();

//HACK to use name in autoreconnect from service with dyn dns
std::array<char, 255U> dnsname;
extern bool PreConnect;
// UltraVNC Server winvnc.exe will also be used for helper exe
// This allow us to minimize the number of seperate exe
#define u16 unsigned short
#define u32 unsigned int
u16 getVolumeHash()
{
    DWORD serialNum{ 0U };

    // Determine if this volume uses an NTFS file system.
    ::GetVolumeInformation("c:\\", nullptr, 0, &serialNum, nullptr, nullptr, nullptr, 0);
    u16 hash = static_cast<unsigned short>(((serialNum + (serialNum >> 16)) & 0xFFFF));

    return hash;
}

u16 getCpuHash()
{
    std::array<int, 4U> cpuinfo{ 0, 0, 0, 0 };
    __cpuid(cpuinfo.data(), 0);
    u16 hash = 0;
    u16* ptr = (u16*)(&cpuinfo[0U]);
    for (u32 i = 0; i < 8; ++i)
    {
        hash += ptr[i];
    }

    return hash;
}

bool Myinit(const HINSTANCE hInstance)
{
    setbuf(stderr, 0);

    // Note: Language DLL and localization already loaded early in WinMain

#ifdef _DEBUG
    {
        // Get current flag
        int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

        // Turn on leak-checking bit
        tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

        // Set flag to the new value
        _CrtSetDbgFlag(tmpFlag);
    }
#endif

    // Save the application instance and main thread id
    hAppInstance = hInstance;
    mainthreadId = ::GetCurrentThreadId();

    // Initialise the VSocket system
    VSocketSystem socksys;
    if (!socksys.Initialised())
    {
        MessageBoxSecure(nullptr, sz_ID_FAILED_INIT.data(), szAppName, MB_OK);
        return 0;
    }
    std::array<unsigned char, 8U> key{ 23, 82, 107, 6, 35, 78, 88, 7 };
    SYSTEM_INFO siSysInfo;
    ::GetSystemInfo(&siSysInfo);
    key[0U] = LOBYTE((siSysInfo.dwOemId + siSysInfo.dwNumberOfProcessors + siSysInfo.dwProcessorType));
    key[1U] = LOBYTE(getVolumeHash());
    key[2U] = LOBYTE(getCpuHash());
    key[3U] = HIBYTE((siSysInfo.dwOemId + siSysInfo.dwNumberOfProcessors + siSysInfo.dwProcessorType));
    key[4U] = HIBYTE(getVolumeHash());
    key[5U] = HIBYTE(getCpuHash());
    vncSetDynKey(key.data());
    return 1;
}

bool return2(const bool value)
{
    if (SettingsManager::getInstance())
    {
        delete SettingsManager::getInstance();
    }
#ifdef SC_20
    if (ScSelect::g_dis_uac)
    {
        ScSelect::Restore_UAC_for_admin_elevated();
    }
#endif // SC_20
    return value;
}

void replaceFilename(char* const path, const char* const newFilename)
{
    char* lastSlash = std::strrchr(path, '\\'); // Find the last '/'
    if (lastSlash)
    {
        *(lastSlash + 1) = '\0';               // Truncate after the last '/'
        ::strcat_s(path, MAX_PATH, newFilename); // Append the new filename
    }
    else
    {
        // No '/' found, replace the whole string
        ::strcpy_s(path, MAX_PATH, newFilename);
    }
}

// Check if running in portable mode
// Portable mode is detected by the presence of "ultravnc.portable" file in the application folder
bool IsPortableMode(const char* const appFolder)
{
    std::array<char, MAX_PATH> portableMarker{};
    ::strcpy_s(portableMarker.data(), portableMarker.size(), appFolder);
    ::strcat_s(portableMarker.data(), portableMarker.size(), "\\");
    ::strcat_s(portableMarker.data(), portableMarker.size(), PORTABLE_MARKER_FILE);

    // Check if marker file exists
    std::ifstream markerFile(portableMarker.data());
    bool isPortable = markerFile.good();
    markerFile.close();

    if (isPortable)
    {
        vnclog.Print(LL_LOGSCREEN, "Portable mode detected (marker file: %s)", portableMarker.data());
    }

    return isPortable;
}

// Migrate INI file from install folder to ProgramData
// Returns true if migration was performed or file already exists in target
bool MigrateIniToProgramData(const char* const installFolderPath, const char* const programDataPath)
{
    std::array<char, MAX_PATH> sourceIni{};
    std::array<char, MAX_PATH> targetIni{};
    std::array<char, MAX_PATH> targetFolder{};

    // Build paths
    ::strcpy_s(sourceIni.data(), sourceIni.size(), installFolderPath);
    ::strcat_s(sourceIni.data(), sourceIni.size(), "\\");
    ::strcat_s(sourceIni.data(), sourceIni.size(), INIFILE_NAME);

    ::strcpy_s(targetIni.data(), targetIni.size(), programDataPath);
    ::strcat_s(targetIni.data(), targetIni.size(), CONFIG_SUBFOLDER);
    ::strcpy_s(targetFolder.data(), targetFolder.size(), targetIni.data());
    ::strcat_s(targetIni.data(), targetIni.size(), "\\");
    ::strcat_s(targetIni.data(), targetIni.size(), INIFILE_NAME);

    // Check if target already exists
    std::ifstream targetFile(targetIni.data());
    if (targetFile.good())
    {
        targetFile.close();
        vnclog.Print(LL_LOGSCREEN, "Config already exists in ProgramData: %s", targetIni.data());
        return true;
    }
    targetFile.close();

    // Check if source exists
    std::ifstream sourceFile(sourceIni.data());
    if (!sourceFile.good())
    {
        sourceFile.close();
        vnclog.Print(LL_LOGSCREEN, "No config to migrate from install folder: %s", sourceIni.data());
        return false;
    }
    sourceFile.close();

    // Create target directory if it doesn't exist
    if ((::_mkdir(targetFolder.data()) != 0) && (errno != EEXIST))
    {
        vnclog.Print(LL_LOGSCREEN, "Failed to create target directory: %s (errno: %d)", targetFolder.data(), errno);
        return false;
    }

    // Copy the file
    if (::CopyFileA(sourceIni.data(), targetIni.data(), FALSE))
    {
        vnclog.Print(LL_LOGSCREEN, "Migrated config from %s to %s", sourceIni.data(), targetIni.data());
        return true;
    }
    else
    {
        DWORD error = ::GetLastError();
        vnclog.Print(LL_LOGSCREEN, "Failed to migrate config (error %d): %s -> %s", error, sourceIni.data(), targetIni.data());
        return false;
    }
}

void extractConfig(char* szCmdLine)
{
    // Check if running as service - check for -service in command line
    bool isServiceMode = strstr(szCmdLine, winvncRunService) != nullptr;

    std::size_t i{ 0U };
    while (szCmdLine[i] != '\0')
    {
        if (std::strncmp(&szCmdLine[i], winvncConfig, std::strlen(winvncConfig)) == 0)
        {
            i += std::strlen(winvncConfig); // Skip the -config part
            // Skip any leading spaces
            while (szCmdLine[i] == ' ')
            {
                ++i;
                ++configfileskip;
            }

            std::size_t pathLength{ 0U }; // Variable to store the path length

            // Check if the value is quoted
            if (szCmdLine[i] == '"')
            {
                ++i; // Skip the opening quote
                ++configfileskip;
                const char* start = &szCmdLine[i];
                const char* end = std::strchr(start, '"'); // Find the closing quote
                if (end)
                {
                    pathLength = end - start;                           // Calculate the length of the path
                    ::strncpy_s(configFile, MAX_PATH, start, pathLength); // Copy the path into the char array
                    configFile[pathLength] = '\0';                      // Null-terminate the path
                    i += pathLength + 1;                                // Move i past the closing quote
                    configfileskip += pathLength + 1;
                }
            }
            else
            {
                // Unquoted value
                const char* start = &szCmdLine[i];
                const char* end = std::strchr(start, ' '); // Find the next space
                if (end)
                {
                    pathLength = end - start;                           // Calculate the length of the path
                    ::strncpy_s(configFile, MAX_PATH, start, pathLength); // Copy the path into the char array
                    configFile[pathLength] = '\0';                      // Null-terminate the path
                    i += pathLength;                                    // Move i past the path
                    configfileskip += pathLength;
                }
                else
                {
                    // Path is the rest of the string
                    ::strcpy_s(configFile, MAX_PATH, &szCmdLine[i]); // Copy the rest of the string into the path
                    pathLength = std::strlen(&szCmdLine[i]);
                    i += pathLength; // Move i past the path
                    configfileskip += pathLength;
                }
            }
            break; // Exit the loop after processing -config
        }
        ++i;
    }
    if (std::strlen(configFile) == 0)
    {
        std::array<char, MAX_PATH> programdataPath{};
        std::array<char, MAX_PATH> programdataFolder{};
        std::array<char, MAX_PATH> programdataIni{};
        std::array<char, MAX_PATH> appdataPath{};
        std::array<char, MAX_PATH> appdataFolder{};
        std::array<char, MAX_PATH> appdataIni{};
#ifndef SC_20
        // Check if running in portable mode
        if (IsPortableMode(winvncFolder.data()))
        {
            // Portable mode: Use install folder for config
            ::strcpy_s(configFile, winvncFolder.data());
            ::strcat_s(configFile, "\\");
            ::strcat_s(configFile, INIFILE_NAME);
            showSettings = true;
            vnclog.Print(LL_LOGSCREEN, "Portable mode: Using config file: %s", configFile);
        }
        else
        {
            // Admin mode: Always use ProgramData (system-wide shared config)
            // Non-admin: Hybrid mode - LocalAppData (user-specific) → ProgramData (shared fallback)

            // Get ProgramData path (system-wide) - always needed
            ::SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programdataPath.data());
            ::strcpy_s(programdataFolder.data(), programdataFolder.size(), programdataPath.data());
            ::strcat_s(programdataFolder.data(), programdataFolder.size(), CONFIG_SUBFOLDER);
            ::strcpy_s(programdataIni.data(), programdataIni.size(), programdataFolder.data());
            ::strcat_s(programdataIni.data(), programdataIni.size(), "\\");
            ::strcat_s(programdataIni.data(), programdataIni.size(), INIFILE_NAME);

            // Check if running as administrator or service
            bool isAdmin = Credentials::RunningAsAdministrator(false);

            if (isAdmin || isServiceMode)
            {
                // Admin or service always uses ProgramData, skip LocalAppData entirely
                if (isServiceMode)
                {
                    vnclog.Print(LL_LOGSCREEN, "Running as service: Using ProgramData config");
                }
                else
                {
                    vnclog.Print(LL_LOGSCREEN, "Running as admin: Using ProgramData config");
                }

                // Attempt to migrate from install folder to ProgramData (one-time)
                MigrateIniToProgramData(winvncFolder.data(), programdataPath.data());

                // Check if ProgramData config exists
                std::ifstream file(programdataIni.data());
                if (file.good())
                {
                    ::strcpy_s(configFile, programdataIni.data());
                    vnclog.Print(LL_LOGSCREEN, "Using shared config: %s", configFile);
                }
                else
                {
                    // No config found, create in ProgramData
                    vnclog.Print(LL_LOGSCREEN, "No config found, creating shared config: %s", programdataIni.data());
                    if ((::_mkdir(programdataFolder.data()) != 0) && (errno != EEXIST))
                    {
                        vnclog.Print(LL_LOGSCREEN, "Warning: Failed to create ProgramData folder: %s (errno: %d)",
                                     programdataFolder.data(), errno);
                    }
                    ::strcpy_s(configFile, programdataIni.data());
                }
                showSettings = true; // Admin can always edit
            }
            else
            {
                // Non-admin: Hybrid mode - LocalAppData (user-specific) → ProgramData (shared fallback)

                // Get LocalAppData path (user-specific)
                ::SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdataPath.data());
                ::strcpy_s(appdataFolder.data(), appdataFolder.size(), appdataPath.data());
                ::strcat_s(appdataFolder.data(), appdataFolder.size(), CONFIG_SUBFOLDER);
                ::strcpy_s(appdataIni.data(), appdataIni.size(), appdataFolder.data());
                ::strcat_s(appdataIni.data(), appdataIni.size(), "\\");
                ::strcat_s(appdataIni.data(), appdataIni.size(), INIFILE_NAME);

                // Attempt to migrate from install folder to ProgramData (one-time)
                MigrateIniToProgramData(winvncFolder.data(), programdataPath.data());

                // Priority: LocalAppData → ProgramData
                // Check for user-specific config first
                {
                    std::ifstream file(appdataIni.data());
                    if (file.good())
                    {
                        // User has personal config in LocalAppData
                        ::strcpy_s(configFile, appdataIni.data());
                        showSettings = true; // Users can always edit their own AppData
                        vnclog.Print(LL_LOGSCREEN, "Using user config: %s", configFile);
                    }
                }

                // If no user config, check for shared ProgramData config
                if (std::strlen(configFile) == 0)
                {
                    std::ifstream file(programdataIni.data());
                    if (file.good())
                    {
                        // Fallback to ProgramData (shared config)
                        ::strcpy_s(configFile, programdataIni.data());
                        // Admins can edit ProgramData, non-admins cannot
                        showSettings = isAdmin;
                        vnclog.Print(LL_LOGSCREEN, "Using shared config: %s (%s)", configFile, isAdmin ? "editable" : "read-only");
                    }
                    else
                    {
                        // No config found - non-admin cannot create in ProgramData
                        // This shouldn't happen as admin should have created it
                        vnclog.Print(LL_LOGSCREEN, "No config found in ProgramData - admin must run winvnc first");
                        ::strcpy_s(configFile, programdataIni.data());
                        showSettings = isAdmin;
                    }
                }
            }
        }
#else
        // SC_20 mode: Use install folder
        ::strcpy_s(configFile, winvncFolder.data());
        ::strcat_s(configFile, "\\");
        ::strcat_s(configFile, INIFILE_NAME);
        showSettings = true;
#endif
    }
    else
    {
        // Explicit config file specified via command line
        showSettings = true;
    }
    std::array<char, MAX_PATH> logFile;
    ::strcpy_s(logFile.data(), logFile.size(), configFile);
    replaceFilename(logFile.data(), "mslogon.log");
    settings->setLogFile(logFile.data());
    settings->setShowSettings(showSettings);
}


// WinMain parses the command line and either calls the main App
// routine or, under NT, the main service routine.
int WINAPI WinMain(const HINSTANCE hInstance, [[maybe_unused]] const HINSTANCE hPrevInstance, const PSTR szCmdLine2, [[maybe_unused]] const int iCmdShow)
{
    ::SetDllDirectory(TEXT(""));
    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    settings = SettingsManager::getInstance();

    typedef BOOL(WINAPI * pSetProcessMitigationPolicy_t)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);
    HMODULE hKernel32 = ::GetModuleHandleA("kernel32.dll");
    if (hKernel32)
    {
        pSetProcessMitigationPolicy_t pSetProcessMitigationPolicy =
            (pSetProcessMitigationPolicy_t)::GetProcAddress(hKernel32, "SetProcessMitigationPolicy");
        if (pSetProcessMitigationPolicy)
        {
            PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy{};
            policy.PreferSystem32Images = 1;
            pSetProcessMitigationPolicy(ProcessImageLoadPolicy, &policy, sizeof(policy));
        }
    }
    if (::GetModuleFileName(nullptr, winvncFolder.data(), static_cast<DWORD>(winvncFolder.size())))
    {
        char* p = std::strrchr(winvncFolder.data(), '\\');
        if (p)
        {
            *p = '\0';
        }
    }
    extractConfig(szCmdLine2);

    // Load language DLL early - before any localization strings are used
    {
        std::array<char, MAX_PATH> szCurrentDir_vnclangdll;
        std::array<char, 16U> savedLanguage{"en"}; // Default to English

        // Try to read language from the config file (already determined by extractConfig)
        ::GetPrivateProfileStringA("admin", "Language", "en", savedLanguage.data(), sizeof(savedLanguage), configFile);

        // Load appropriate language DLL based on saved preference
        hInstResDLL = hInstance; // Default to English (embedded resources)

        if (::_stricmp(savedLanguage.data(), "en") != 0)
        {
            // Try to load language DLL from languages subfolder first
            ::sprintf_s(szCurrentDir_vnclangdll.data(), szCurrentDir_vnclangdll.size(), "%s\\languages\\winvnclang_%s.dll", winvncFolder.data(), savedLanguage.data());
            HMODULE hLangDLL = ::LoadLibrary(szCurrentDir_vnclangdll.data());

            if (hLangDLL)
            {
                hInstResDLL = hLangDLL;
            }
            else
            {
                // Fallback: try root folder
                ::sprintf_s(szCurrentDir_vnclangdll.data(), szCurrentDir_vnclangdll.size(), "%s\\winvnclang_%s.dll", winvncFolder.data(), savedLanguage.data());
                hLangDLL = ::LoadLibrary(szCurrentDir_vnclangdll.data());
                if (hLangDLL)
                {
                    hInstResDLL = hLangDLL;
                }
                else
                {
                    // Fallback: try old single language DLL (vnclang_server.dll)
                    ::sprintf_s(szCurrentDir_vnclangdll.data(), szCurrentDir_vnclangdll.size(), "%s\\vnclang_server.dll", winvncFolder.data());
                    hLangDLL = ::LoadLibrary(szCurrentDir_vnclangdll.data());
                    if (hLangDLL)
                    {
                        hInstResDLL = hLangDLL;
                    }
                }
            }
        }

        // Load all messages from resource file
        Load_Localization(hInstResDLL);
    }

    ::InitCommonControls();
    INITCOMMONCONTROLSEX icex;
    std::memset(&icex, 0x0, sizeof(INITCOMMONCONTROLSEX));
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_COOL_CLASSES;
    ::InitCommonControlsEx(&icex);
    try
    {
        if (VNC_OSVersion::getInstance()->OS_XP == true)
        {
            MessageBoxSecure(nullptr, sz_ID_WINDOWS_XP_SPECIAL_BUILD.data(), sz_ID_WARNING_CAPTION.data(), MB_ICONERROR);
        }

        if (VNC_OSVersion::getInstance()->OS_NOTSUPPORTED == true)
        {
            MessageBoxSecure(nullptr, sz_ID_ERROR_OS_NOT_SUPPORTED.data(), sz_ID_UNSUPPORTED_OS_CAPTION.data(), MB_ICONERROR);
            return return2(true);
        }
        // make vnc last service to stop
        ::SetProcessShutdownParameters(0x100, false);
        // handle dpi on aero
        HMODULE hUser32 = ::LoadLibraryEx(_T("user32.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        HMODULE hSHCore = ::LoadLibraryEx(_T("SHCore.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

        HRESULT(WINAPI * _SetProcessDpiAwareness)(DWORD value);
        _SetProcessDpiAwareness = (HRESULT(WINAPI*)(DWORD))::GetProcAddress(hSHCore, "SetProcessDpiAwareness");
        if (_SetProcessDpiAwareness)
        {
            _SetProcessDpiAwareness(2);
        }
        else
        {
            BOOL(WINAPI * _SetProcessDPIAware)();
            _SetProcessDPIAware = (BOOL(WINAPI*)())::GetProcAddress(hUser32, "SetProcessDPIAware");
            if (_SetProcessDPIAware)
            {
                _SetProcessDPIAware();
            }
        }
        if (hUser32)
        {
            ::FreeLibrary(hUser32);
        }
        if (hSHCore)
        {
            ::FreeLibrary(hSHCore);
        }
        bool Injected_autoreconnect{ false };
        settings->Initialize(configFile);
#ifndef SC_20
        settings->setScExit(false);
        settings->setScPrompt(false);
#endif // SC_20
       // Update mslogon.log path to use path= setting from INI
        char* debugPath = settings->getDebugPath();
        if (debugPath && (std::strlen(debugPath) > 0))
        {
            std::array<char, MAX_PATH> logFile;
            ::strcpy_s(logFile.data(), logFile.size(), debugPath);
            std::size_t len = std::strlen(logFile.data());
            if ((len > 0) && (logFile[len - 1] != '\\'))
            {
                ::strcat_s(logFile.data(), logFile.size(), "\\");
            }
            ::strcat_s(logFile.data(), logFile.size(), "mslogon.log");
            settings->setLogFile(logFile.data());
        }
        setbuf(stderr, 0);

        // Note: Language DLL and localization already loaded early in WinMain

#ifdef _DEBUG
        {
            // Get current flag
            int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

            // Turn on leak-checking bit
            tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

            // Set flag to the new value
            _CrtSetDbgFlag(tmpFlag);
        }
#endif

        // Save the application instance and main thread id
        hAppInstance = hInstance;
        mainthreadId = ::GetCurrentThreadId();

        // Initialise the VSocket system
        VSocketSystem socksys;
        if (!socksys.Initialised())
        {
            MessageBoxSecure(nullptr, sz_ID_FAILED_INIT.data(), szAppName, MB_OK);
            return return2(0);
        }
#ifdef SC_20
        char* szCmdLine = ScSelect::InitSC(hInstance, szCmdLine2);
#else
        char* szCmdLine = szCmdLine2;
#endif // SC_20
       // look up the current service name in the registry.
       //serviceHelpers::ExistServiceName(progname, UltraVNCService::service_name);

        // Make the command-line lowercase and parse it
        std::size_t i;
        for (i = 0; i < std::strlen(szCmdLine); ++i)
        {
            szCmdLine[i] = static_cast<char>(std::tolower(szCmdLine[i]));
        }
        BOOL argfound = FALSE;
        for (i = 0; i < std::strlen(szCmdLine); ++i)
        {
            if (szCmdLine[i] <= ' ')
            {
                continue;
            }
            argfound = TRUE;

#ifndef SC_20
            if (std::strncmp(&szCmdLine[i], winvncStopserviceHelper, std::strlen(winvncStopserviceHelper)) == 0)
            {
                ::Sleep(3000);
                serviceHelpers::Set_stop_service_as_admin();
                return return2(0);
            }
#endif // SC_20

            if (std::strncmp(&szCmdLine[i], winvncKill, std::strlen(winvncKill)) == 0)
            {
                static HANDLE hShutdownEventTmp;
                hShutdownEventTmp = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, "Global\\SessionEventUltra");
                ::SetEvent(hShutdownEventTmp);
                ::CloseHandle(hShutdownEventTmp);

                //adzm 2010-02-10 - Finds the appropriate VNC window for any process. Sends this message to all of them!
                // do removed, loops forever with cpu 100
                HWND hservwnd = nullptr;
                hservwnd = postHelper::FindWinVNCWindow(false);
                if (hservwnd != nullptr)
                {
                    ::PostMessage(hservwnd, WM_COMMAND, ID_CLOSE_SILENT, 0);
                    ::PostMessage(hservwnd, WM_CLOSE, 0, 0);
                }
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenhomepage, std::strlen(winvncopenhomepage)) == 0)
            {
                Open_homepage();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenforum, std::strlen(winvncopenforum)) == 0)
            {
                Open_forum();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopengithub, std::strlen(winvncopengithub)) == 0)
            {
                Open_github();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenmastodon, std::strlen(winvncopenmastodon)) == 0)
            {
                Open_mastodon();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenbluesky, std::strlen(winvncopenbluesky)) == 0)
            {
                Open_bluesky();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenfacebook, std::strlen(winvncopenfacebook)) == 0)
            {
                Open_facebook();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenxtwitter, std::strlen(winvncopenxtwitter)) == 0)
            {
                Open_xtwitter();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenreddit, std::strlen(winvncopenreddit)) == 0)
            {
                Open_reddit();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncopenopenhub, std::strlen(winvncopenopenhub)) == 0)
            {
                Open_openhub();
                return return2(0);
            }

#ifdef SC_20
            if (std::strncmp(&szCmdLine[i], "-noregistry", std::strlen("-noregistry")) == 0)
            {
                i += std::strlen("-noregistry");
                continue;
            }
            if (std::strncmp(&szCmdLine[i], "-secureplugin", std::strlen("-secureplugin")) == 0)
            {
                i += std::strlen("-secureplugin");
                settings->setUseDSMPlugin(true);
                continue;
            }
#endif // SC_20
#ifndef SC_20
            if (std::strncmp(&szCmdLine[i], winvncStartserviceHelper, std::strlen(winvncStartserviceHelper)) == 0)
            {
                ::Sleep(3000);
                serviceHelpers::Set_start_service_as_admin();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncInstallServiceHelper, std::strlen(winvncInstallServiceHelper)) == 0)
            {
                //Sleeps are realy needed, else runas fails...
                ::Sleep(3000);
                serviceHelpers::Set_install_service_as_admin();
                return return2(0);
            }
            if (std::strncmp(&szCmdLine[i], winvncUnInstallServiceHelper, std::strlen(winvncUnInstallServiceHelper)) == 0)
            {
                ::Sleep(3000);
                serviceHelpers::Set_uninstall_service_as_admin();
                return return2(0);
            }
            if (std::strncmp(&szCmdLine[i], winvncSoftwarecadHelper, std::strlen(winvncSoftwarecadHelper)) == 0)
            {
                ::Sleep(3000);
                vncCad::Enable_softwareCAD_elevated();
                return return2(0);
            }
            if (std::strncmp(&szCmdLine[i], winvncdelSoftwarecadHelper, std::strlen(winvncdelSoftwarecadHelper)) == 0)
            {
                ::Sleep(3000);
                vncCad::delete_softwareCAD_elevated();
                return return2(0);
            }
            if (std::strncmp(&szCmdLine[i], winvncRebootSafeHelper, std::strlen(winvncRebootSafeHelper)) == 0)
            {
                ::Sleep(3000);
                UltraVNCService::Reboot_in_safemode_elevated();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncRebootForceHelper, std::strlen(winvncRebootForceHelper)) == 0)
            {
                ::Sleep(3000);
                UltraVNCService::Reboot_with_force_reboot_elevated();
                return return2(0);
            }
            if (std::strncmp(&szCmdLine[i], winvncSecurityEditorHelper, std::strlen(winvncSecurityEditorHelper)) == 0)
            {
                ::Sleep(3000);
                serviceHelpers::winvncSecurityEditorHelper_as_admin();
                return return2(0);
            }
#endif // SC_20
            if (std::strncmp(&szCmdLine[i], winvncSecurityEditor, std::strlen(winvncSecurityEditor)) == 0)
            {
                typedef void (*vncEditSecurityFn)(HWND hwnd, HINSTANCE hInstance);
                vncEditSecurityFn vncEditSecurity = 0;
                std::array<char, MAX_PATH> szCurrentDir{};
                ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
                ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\authSSP.dll");
                HMODULE hModule = ::LoadLibrary(szCurrentDir.data());
                if (hModule)
                {
                    vncEditSecurity = (vncEditSecurityFn)::GetProcAddress(hModule, "vncEditSecurity");
                    ::CoInitialize(nullptr);
                    vncEditSecurity(nullptr, hAppInstance);
                    ::CoUninitialize();
                    ::FreeLibrary(hModule);
                }
                return return2(0);
            }

#ifndef SC_20
            if (std::strncmp(&szCmdLine[i], winvncSoftwarecad, std::strlen(winvncSoftwarecad)) == 0)
            {
                vncCad::Enable_softwareCAD();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncdelSoftwarecad, std::strlen(winvncdelSoftwarecad)) == 0)
            {
                vncCad::delete_softwareCAD();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncRebootSafe, std::strlen(winvncRebootSafe)) == 0)
            {
                UltraVNCService::Reboot_in_safemode();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncRebootForce, std::strlen(winvncRebootForce)) == 0)
            {
                UltraVNCService::Reboot_with_force_reboot();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncStopservice, std::strlen(winvncStopservice)) == 0)
            {
                serviceHelpers::Real_stop_service();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncStartservice, std::strlen(winvncStartservice)) == 0)
            {
                serviceHelpers::Real_start_service();
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncInstallDriver, std::strlen(winvncInstallDriver)) == 0)
            {
                VirtualDisplay::InstallDriver(true);
                return return2(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncInstallService, std::strlen(winvncInstallService)) == 0)
            {
                // rest of command line service name, if provided.
                char* pServiceName = &szCmdLine[i];
                // skip over command switch, find next whitepace
                while (*pServiceName && !std::isspace(*(unsigned char*)pServiceName))
                {
                    ++pServiceName;
                }

                // skip past whitespace to service name
                while (*pServiceName && std::isspace(*(unsigned char*)pServiceName))
                {
                    ++pServiceName;
                }

                // strip off any quotes
                if (*pServiceName && (*pServiceName == '\"'))
                {
                    ++pServiceName;
                }

                if (*pServiceName)
                {
                    // look for trailing quote, if found, terminate the string there.
                    char* pQuote = pServiceName;
                    pQuote = std::strrchr(pServiceName, '\"');
                    if (pQuote)
                    {
                        *pQuote = 0;
                    }
                }
                // if a service name is supplied, and it differs except in case from
                // the default, use the supplied service name instead
                if (*pServiceName && (_strcmpi(pServiceName, UltraVNCService::service_name.data()) != 0))
                {
                    ::strncpy_s(UltraVNCService::service_name.data(), 256, pServiceName, 256);
                    UltraVNCService::service_name[255U] = 0;
                }
                UltraVNCService::install_service();
                ::Sleep(2000);
                serviceHelpers::Real_start_service();
                return return2(0);
            }
            if (std::strncmp(&szCmdLine[i], winvncUnInstallService, std::strlen(winvncUnInstallService)) == 0)
            {
                std::array<char, MAX_PATH + 32> command; // 29 January 2008 jdp
                // rest of command line service name, if provided.
                char* pServiceName = &szCmdLine[i];
                // skip over command switch, find next whitepace
                while (*pServiceName && !std::isspace(*(unsigned char*)pServiceName))
                {
                    ++pServiceName;
                }

                // skip past whitespace to service name
                while (*pServiceName && std::isspace(*(unsigned char*)pServiceName))
                {
                    ++pServiceName;
                }

                // strip off any quotes
                if (*pServiceName && (*pServiceName == '\"'))
                {
                    ++pServiceName;
                }

                if (*pServiceName)
                {
                    // look for trailing quote, if found, terminate the string there.
                    char* pQuote = pServiceName;
                    pQuote = std::strrchr(pServiceName, '\"');
                    if (pQuote)
                    {
                        *pQuote = 0;
                    }
                }

                if (*pServiceName && (_strcmpi(pServiceName, UltraVNCService::service_name.data()) != 0))
                {
                    ::strncpy_s(UltraVNCService::service_name.data(), 256, pServiceName, 256);
                    UltraVNCService::service_name[255U] = 0;
                }
                serviceHelpers::Real_stop_service();
                UltraVNCService::uninstall_service();
                return return2(0);
            }
#endif // SC_20
            if (std::strncmp(&szCmdLine[i], winvncPreConnect, std::strlen(winvncPreConnect)) == 0)
            {
                i += std::strlen(winvncPreConnect);
                PreConnect = true;
                continue;
            }
            if (std::strncmp(&szCmdLine[i], winvncConfig, std::strlen(winvncConfig)) == 0)
            {
                i += std::strlen(winvncConfig);
                i += configfileskip; // was already extracted, just skip chars
                continue;
            }
            if (std::strncmp(&szCmdLine[i], winvncRunService, std::strlen(winvncRunService)) == 0)
            {
                //Run as service
                if (!Myinit(hInstance))
                {
                    return return2(0);
                }
                settings->setRunningFromExternalService(true);
                int return2value = WinVNCAppMain();
                return return2(return2value);
            }

            if (std::strncmp(&szCmdLine[i], winvncRunServiceRdp, std::strlen(winvncRunServiceRdp)) == 0)
            {
                //Run as service
                if (!Myinit(hInstance))
                {
                    return return2(0);
                }
                settings->setRunningFromExternalService(true);
                settings->setRunningFromExternalServiceRdp(true);
                int return2value = WinVNCAppMain();
                return return2(return2value);
            }
#ifndef SC_20
            if (std::strncmp(&szCmdLine[i], winvncStartService, std::strlen(winvncStartService)) == 0)
            {
                UltraVNCService::start_service(szCmdLine);
                return return2(0);
            }
#endif // SC_20
            if (std::strncmp(&szCmdLine[i], winvncRunAsUserApp, std::strlen(winvncRunAsUserApp)) == 0)
            {
                // WinVNC is being run as a user-level program
                if (!Myinit(hInstance))
                {
                    return return2(0);
                }
                int return2value = WinVNCAppMain();
                return return2(return2value);
            }

            if (std::strncmp(&szCmdLine[i], winvncSCexit, std::strlen(winvncSCexit)) == 0)
            {
                settings->setScExit(true);
                i += std::strlen(winvncSCexit);
                continue;
            }

            if (std::strncmp(&szCmdLine[i], winvncSCprompt, std::strlen(winvncSCprompt)) == 0)
            {
                settings->setScPrompt(true);
                i += std::strlen(winvncSCprompt);
                continue;
            }

            if (std::strncmp(&szCmdLine[i], winvncmulti, std::strlen(winvncmulti)) == 0)
            {
                allowMultipleInstances = true;
                i += std::strlen(winvncmulti);
                continue;
            }

            if (std::strncmp(&szCmdLine[i], winvncsettings, std::strlen(winvncsettings)) == 0)
            {
                PropertiesDialog properties;
                properties.ShowDialog(true);
                ::PostQuitMessage(0);
            }

            if (std::strncmp(&szCmdLine[i], winvncStopReconnect, std::strlen(winvncStopReconnect)) == 0)
            {
                i += std::strlen(winvncStopReconnect);
                postHelper::PostAddStopConnectClientAll();
                continue;
            }

            if (std::strncmp(&szCmdLine[i], winvncAutoReconnect, std::strlen(winvncAutoReconnect)) == 0)
            {
                // Note that this "autoreconnect" param MUST be BEFORE the "connect" one
                // on the command line !
                // wa@2005 -- added support for the AutoReconnectId
                i += std::strlen(winvncAutoReconnect);
                Injected_autoreconnect = true;
                std::size_t start, end;
                char* pszId{ nullptr };
                start = i;
                // skip any spaces and grab the parameter
                while ((szCmdLine[start] <= ' ') && (szCmdLine[start] != 0))
                {
                    ++start;
                }

                if (std::strncmp(&szCmdLine[start], winvncAutoReconnectId, std::strlen(winvncAutoReconnectId)) == 0)
                {
                    end = start;
                    while (szCmdLine[end] > ' ')
                    {
                        ++end;
                    }

                    if (end - start > 0)
                    {

                        pszId = new char[end - start + 1];

                        ::strncpy_s(pszId, end - start + 1, &(szCmdLine[start]), end - start);
                        pszId[end - start] = 0;
                        pszId = _strupr(pszId);
                    }
                    //multiple spaces between autoreconnect and id
                    i = end;
                } // end of condition we found the ID: parameter

                // NOTE:  id must be NULL or the ID:???? (pointer will get deleted when message is processed)
                // We can not contact a runnning service, permissions, so we must store the settings
                // and process until the vncmenu has been started

                if (!postHelper::PostAddAutoConnectClient(pszId))
                {
                    PostAddAutoConnectClient_bool = true;
                    if (pszId == nullptr)
                    {
                        PostAddAutoConnectClient_bool_null = true;
                        PostAddAutoConnectClient_bool = false;
                    }
                    else
                    {
                        ::strcpy_s(pszId_char.data(), pszId_char.size(), pszId);
                        //memory leak fix
                        delete[] pszId;
                        pszId = nullptr;
                    }
                }
                if (pszId != nullptr)
                {
                    delete[] pszId;
                }
                pszId = nullptr;
                continue;
            }

            if (std::strncmp(&szCmdLine[i], winvncReconnectId, std::strlen(winvncReconnectId)) == 0)
            {
                i += std::strlen("-");
                std::size_t start, end;
                char* pszId{ nullptr };
                start = i;
                end = start;
                while (szCmdLine[end] > ' ')
                {
                    ++end;
                }
                if (end - start > 0)
                {
                    pszId = new char[end - start + 1];
                    if (pszId != 0)
                    {
                        ::strncpy_s(pszId, end - start + 1, &(szCmdLine[start]), end - start);
                        pszId[end - start] = 0;
                        pszId = _strupr(pszId);
                    }
                }
                i = end;
                if (!postHelper::PostAddConnectClient(pszId))
                {
                    PostAddConnectClient_bool = true;
                    if (pszId == nullptr)
                    {
                        PostAddConnectClient_bool_null = true;
                        PostAddConnectClient_bool = false;
                    }
                    else
                    {
                        ::strcpy_s(pszId_char.data(), pszId_char.size(), pszId);
                        //memory leak fix
                        delete[] pszId;
                        pszId = nullptr;
                    }
                }
                if (pszId != nullptr)
                {
                    delete[] pszId;
                }
                pszId = nullptr;
                continue;
            }

            if (std::strncmp(&szCmdLine[i], winvncConnect, std::strlen(winvncConnect)) == 0)
            {
                if (!Injected_autoreconnect)
                {
                    postHelper::PostAddStopConnectClient();
                }
                // Add a new client to an existing copy of winvnc
                i += std::strlen(winvncConnect);

                // First, we have to parse the command line to get the filename to use
                std::size_t start, end;
                start = i;
                while ((szCmdLine[start] <= ' ') && (szCmdLine[start] != 0))
                {
                    ++start;
                }
                end = start;
                while (szCmdLine[end] > ' ')
                {
                    ++end;
                }

                // Was there a hostname (and optionally a port number) given?
                if (end - start > 0)
                {
                    char* name = new char[end - start + 1];
                    char* name2 = new char[end - start + 1];
                    if (name != 0)
                    {
                        ::strncpy_s(name, end - start + 1, &(szCmdLine[start]), end - start);
                        name[end - start] = 0;
                        ::strcpy_s(name2, end - start + 1, name);
                        //detect braceletes in IPv6 address or remove port number from name
                        char* bs = std::strchr(name, '[');
                        char* be = std::strchr(name, ']');
                        if (bs && be)
                        {
                            ::strncpy_s(name2, end - start + 1, be + 1, std::strlen(be));
                            *be = '\0';
                            ::strcpy_s(name, end - start + 1, bs + 1);
                        }
                        else
                        {
                            char* portp = std::strchr(name, ':');
                            if (portp)
                            {
                                *portp++ = '\0';
                            }
                        }
                        int port = INCOMING_PORT_OFFSET;
                        char* portp = std::strchr(name2, ':');
                        if (portp)
                        {
                            *portp++ = '\0';
                            if (*portp == ':')
                            {
                                port = atoi(++portp); // Port number after "::"
                            }
                            else
                            {
                                port = atoi(portp); // Display number after ":"
                            }
                        }
                        delete[] name2;
                        vnclog.Print(LL_STATE, VNCLOG("test... %s %d\n"), name, port);
                        ::strcpy_s(dnsname.data(), dnsname.size(), name);
                        if (settings->getIPV6())
                        {
                            in6_addr address;
                            std::memset(&address, 0, sizeof(address));
                            if (VSocket::Resolve6(name, &address))
                            {
                                vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient III \n"));
                                if (!postHelper::PostAddNewClientInit6(&address, static_cast<unsigned short>(port)))
                                {
                                    PostAddNewClient_bool = true;
                                    port_int = port;
                                    address_in6 = address;
                                }
                            }
                            else
                            {
                                //ask for host,port
                                PostAddNewClient_bool = true;
                                port_int = 0;
                                std::memset(&address_in6, 0, sizeof(address_in6));
                                ::Sleep(2000);
                                delete[] name;
                                return return2(0);
                            }
                            if (port_int == 0)
                            {
                                VCard32 addressInner = VSocket::Resolve4(name);
                                if (addressInner != 0)
                                {
                                    // Post the IP address to the server
                                    // We can not contact a runnning service, permissions, so we must store the settings
                                    // and process until the vncmenu has been started
                                    vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient III \n"));
                                    if (!postHelper::PostAddNewClientInit4(static_cast<unsigned short>(addressInner), static_cast<unsigned short>(port)))
                                    {
                                        PostAddNewClient_bool = true;
                                        port_int = port;
                                        address_vcard4 = addressInner;
                                    }
                                }
                                else
                                {
                                    //ask for host,port
                                    PostAddNewClient_bool = true;
                                    port_int = 0;
                                    address_vcard4 = 0;
                                    ::Sleep(2000);
                                    delete[] name;
                                    return return2(0);
                                }
                            }

                            delete[] name;
                        }
                        else
                        {
                            VCard32 address = VSocket::Resolve(name);
#ifdef SC_20
                            if (address == 0)
                            {
                                std::array<char, 1024U> text{};
                                std::snprintf(text.data(), text.size(), " Hostnamee (%s) could not be resolved", name);
                                helper::yesUVNCMessageBox(hInstResDLL, nullptr, text.data(), (char*)szAppName, MB_ICONEXCLAMATION);
                                delete[] name;
                                return return2(0);
                            }
#endif // SC_20
                            delete[] name;
                            if (address != 0)
                            {
                                // Post the IP address to the server
                                // We can not contact a runnning service, permissions, so we must store the settings
                                // and process until the vncmenu has been started
                                vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient III \n"));
                                if (!postHelper::PostAddNewClientInit(address, static_cast<unsigned short>(port)))
                                {
                                    PostAddNewClient_bool = true;
                                    port_int = port;
                                    address_vcard = address;
                                }
                            }
                            else
                            {
                                //ask for host,port
                                PostAddNewClient_bool = true;
                                port_int = 0;
                                address_vcard = 0;
                                ::Sleep(2000);
                                //Beep(200,1000);
                                return return2(0);
                            }
                        }
                    }
                    i = end;
                    continue;
                }
                else
                {
                    // Tell the server to show the Add New Client dialog
                    // We can not contact a runnning service, permissions, so we must store the settings
                    // and process until the vncmenu has been started
                    vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient IIII\n"));
                    if (settings->getIPV6())
                    {
                        if (!postHelper::PostAddNewClient4(0, 0))
                        {
                            PostAddNewClient_bool = true;
                            port_int = 0;
                            std::memset(&address_in6, 0, sizeof(address_in6));
                        }
                    }
                    else
                    {
                        if (!postHelper::PostAddNewClient(0, 0))
                        {
                            PostAddNewClient_bool = true;
                            port_int = 0;
                            address_vcard = 0;
                        }
                    }
                }
                continue;
            }


            //adzm 2009-06-20
            if (std::strncmp(&szCmdLine[i], winvncRepeater, std::strlen(winvncRepeater)) == 0)
            {
                // set the default repeater host
                i += std::strlen(winvncRepeater);

                // First, we have to parse the command line to get the host to use
                std::size_t start, end;
                start = i;
                while ((szCmdLine[start] <= ' ') && (szCmdLine[start] != 0))
                {
                    ++start;
                }
                end = start;
                while (szCmdLine[end] > ' ')
                {
                    ++end;
                }

                // Was there a hostname (and optionally a port number) given?
                if (end - start > 0)
                {
                    if (g_szRepeaterHost)
                    {
                        delete[] g_szRepeaterHost;
                        g_szRepeaterHost = nullptr;
                    }
                    g_szRepeaterHost = new char[end - start + 1];
                    if (g_szRepeaterHost != 0)
                    {
                        ::strncpy_s(g_szRepeaterHost, end - start + 1, &(szCmdLine[start]), end - start);
                        g_szRepeaterHost[end - start] = 0;

                        // We can not contact a runnning service, permissions, so we must store the settings
                        // and process until the vncmenu has been started
                        vnclog.Print(LL_INTERR, VNCLOG("PostAddNewRepeaterClient I\n"));
                        if (!postHelper::PostAddNewRepeaterClient())
                        {
                            PostAddNewRepeaterClient_bool = true;
                            port_int = 0;
                            if (settings->getIPV6())
                            {
                                address_vcard4 = 0;
                                std::memset(&address_in6, 0, sizeof(address_in6));
                            }
                            else
                            {
                                address_vcard = 0;
                            }
                        }
                    }
                    i = end;
                    continue;
                }
                continue;
            }

            // Either the user gave the -help option or there is something odd on the cmd-line!

            // Show the usage dialog
            MessageBoxSecure(nullptr, winvncUsageText, sz_ID_WINVNC_USAGE.data(), MB_OK | MB_ICONINFORMATION);
            break;
        };

        // If no arguments were given then just run
        if (!argfound)
        {
            if (!Myinit(hInstance))
            {
                return return2(0);
            }
            int return2value = WinVNCAppMain();
            return return2(return2value);
        }
    }
    catch (...)
    {
        return return2(0);
    }

    return return2(0);
}

// rdv&sf@2007 - New TrayIcon impuDEsktop/impersonation thread stuff

DWORD WINAPI imp_desktop_thread(const LPVOID lpParam)
{
    vncServer* server = (vncServer*)lpParam;
    HDESK desktop;
    //vnclog.Print(LL_INTERR, VNCLOG("SelectDesktop \n"));
    //vnclog.Print(LL_INTERR, VNCLOG("OpenInputdesktop2 NULL\n"));
    desktop = ::OpenInputDesktop(0, FALSE,
                               DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                   DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);

    if (desktop == nullptr)
    {
        vnclog.Print(LL_INTERR, VNCLOG("OpenInputdesktop Error \n"));
    }

    HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    DWORD dummy;

    std::array<char, 256U> new_name;

    if (!::GetUserObjectInformation(desktop, UOI_NAME, &new_name, 256, &dummy))
    {
        vnclog.Print(LL_INTERR, VNCLOG("!GetUserObjectInformation \n"));
    }

    vnclog.Print(LL_INTERR, VNCLOG("SelectHDESK to %s (%x) from %x\n"), new_name.data(), desktop, old_desktop);

    if (!::SetThreadDesktop(desktop))
    {
        vnclog.Print(LL_INTERR, VNCLOG("SelectHDESK:!SetThreadDesktop \n"));
    }

    //	ImpersonateCurrentUser_();

    std::array<char, UNLEN + 1> m_username;
    HWINSTA station = ::GetProcessWindowStation();
    if (station != nullptr)
    {
        DWORD usersize;
        ::GetUserObjectInformation(station, UOI_USER_SID, nullptr, 0, &usersize);
        SetLastError(0);
        if (usersize != 0)
        {
            DWORD length = sizeof(m_username);
            if (::GetUserName(m_username.data(), &length) == 0)
            {
                UINT error = ::GetLastError();
                if (error != ERROR_NOT_LOGGED_ON)
                {
                    vnclog.Print(LL_INTERR, VNCLOG("GetUsername error %d\n"), ::GetLastError());
                    ::SetThreadDesktop(old_desktop);
                    ::CloseDesktop(desktop);
                    ::Sleep(500);
                    return FALSE;
                }
            }
        }
    }
    vnclog.Print(LL_INTERR, VNCLOG("Username %s \n"), m_username.data());

    // Create Tray icon and menu
    auto menu = std::make_unique<vncMenu>(server);
    if (menu == nullptr)
    {
        vnclog.Print(LL_INTERR, VNCLOG("failed to create tray menu\n"));
        ::PostQuitMessage(0);
    }

    // This is a good spot to handle the old PostAdd messages
    if (PostAddAutoConnectClient_bool)
    {
        postHelper::PostAddAutoConnectClient(pszId_char.data());
    }
    if (PostAddAutoConnectClient_bool_null)
    {
        postHelper::PostAddAutoConnectClient(nullptr);
    }

    if (PostAddConnectClient_bool)
    {
        postHelper::PostAddConnectClient(pszId_char.data());
    }
    if (PostAddConnectClient_bool_null)
    {
        postHelper::PostAddConnectClient(nullptr);
    }

    if (PostAddNewClient_bool)
    {
        PostAddNewClient_bool = false;
        vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient IIIII\n"));
        if (settings->getIPV6())
        {
            postHelper::PostAddNewClient6(&address_in6, static_cast<unsigned short>(port_int));
        }
        else
        {
            postHelper::PostAddNewClient(address_vcard, static_cast<unsigned short>(port_int));
        }
    }
    //adzm 2009-06-20
    if (PostAddNewRepeaterClient_bool)
    {
        PostAddNewRepeaterClient_bool = false;
        vnclog.Print(LL_INTERR, VNCLOG("PostAddNewRepeaterClient II\n"));
        postHelper::PostAddNewRepeaterClient();
    }
    bool Runonce{ false };
    MSG msg;
    while (::GetMessage(&msg, 0, 0, 0) != 0)
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (fShutdownOrdered && !Runonce)
        {
            Runonce = true;
            if (menu)
            {
                menu->Shutdown(true);
            }
        }

        if (hShutdownEvent)
        {
            // vnclog.Print(LL_INTERR, VNCLOG("****************** SDTimer tic\n"));
            DWORD result = ::WaitForSingleObject(hShutdownEvent, 1);
            if (WAIT_OBJECT_0 == result)
            {
                ::ResetEvent(hShutdownEvent);
                fShutdownOrdered = true;
                //vnclog.Print(LL_INTERR, VNCLOG("****************** WaitForSingleObject - Shutdown server\n"));
            }
        }
    }

    //vnclog.Print(LL_INTERR, VNCLOG("GetMessage stop \n"));
    ::SetThreadDesktop(old_desktop);
    ::CloseDesktop(desktop);
    //	RevertToSelf();
    return 0;
}

// This is the main routine for UltraVNC Server when running as an application
// (under Windows 95 or Windows NT)
// Under NT, UltraVNC Server can also run as a service. The WinVNCServerMain routine,
// defined in the vncService header, is used instead when running as a service.

int WinVNCAppMain()
{
    vnclog.SetPath(settings->getDebugPath());
    vnclog.SetLevel(settings->getDebugLevel());
    vnclog.SetMode(settings->getDebugMode());
    vnclog.SetFile();
    vnclog.SetVideo(settings->getAvilog());

    vnclog.Print(-1, VNCLOG("WinVNCAPPMain-----Application started\n"));
#ifdef CRASH_ENABLED
    LPVOID lpvState = ::Install(nullptr, "rudi.de.vos@skynet.be", "UltraVNC");
#endif

    // Set this process to be the last application to be shut down.
    // Check for previous instances of UltraVNC Server!
    auto instancehan = std::make_unique<vncInstHandler>();
    if (!allowMultipleInstances) // this allow to overwrite the multiple instance check
    {
        if (!instancehan->Init())
        {
            vnclog.Print(LL_INTINFO, VNCLOG("%s -- exiting\n"), sz_ID_ANOTHER_INST.data());
            // We don't allow multiple instances!
            if (!settings->RunningFromExternalService())
            {
                MessageBoxSecure(nullptr, sz_ID_ANOTHER_INST.data(), szAppName, MB_OK);
            }
            return 0;
        }
    }

    //vnclog.Print(LL_INTINFO, VNCLOG("***** DBG - Previous instance checked - Trying to create server\n"));
    // CREATE SERVER
    vncServer server;

    // Set the name and port number
    server.SetName(szAppName);
    vnclog.Print(LL_STATE, VNCLOG("server created ok\n"));
    ///uninstall driver before cont

    // sf@2007 - New impersonation thread stuff for Tray icon & menu
    // Subscribe to shutdown event
    hShutdownEvent = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, "Global\\SessionEventUltra");
    if (hShutdownEvent)
    {
        ::ResetEvent(hShutdownEvent);
    }
    //vnclog.Print(LL_STATE, VNCLOG("***************** SDEvent created \n"));
    // Create the timer that looks periodicaly for shutdown event
    mmRes = static_cast<MMRESULT>(-1);
    //InitSDTimer();

    while (!fShutdownOrdered)
    {
        //vnclog.Print(LL_STATE, VNCLOG("################## Creating Imp Thread : %d \n"), nn);

        HANDLE threadHandle;
        DWORD dwTId;
        threadHandle = ::CreateThread(nullptr, 0, imp_desktop_thread, &server, 0, &dwTId);

        if (threadHandle)
        {
            ::WaitForSingleObject(threadHandle, INFINITE);
            ::CloseHandle(threadHandle);
        }
        vnclog.Print(LL_STATE, VNCLOG("################## Closing Imp Thread\n"));
    }
    fShutdownOrdered = true;

    if (hShutdownEvent)
    {
        ::CloseHandle(hShutdownEvent);
    }
    vnclog.Print(LL_STATE, VNCLOG("################## SHUTING DOWN SERVER ####################\n"));

    //adzm 2009-06-20
    if (g_szRepeaterHost)
    {
        delete[] g_szRepeaterHost;
        g_szRepeaterHost = nullptr;
    }

    VNC_OSVersion::releaseInstance();
    return 1;
};
