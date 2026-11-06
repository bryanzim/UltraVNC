// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//
#pragma warning(disable : 4996)


#ifndef SC_20
#include "stdhdrs.h"

#include "UltraVNCService.h"

#include "common/inifile.h"
#include "common/win32_helpers.h"
#include "Localization.h"
#include "SettingsManager.h"

#include <windows.h>

#include <direct.h>
#include <shlobj.h>
#include <userenv.h>
#include <wtsapi32.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <errno.h>
#include <fstream>

SERVICE_STATUS UltraVNCService::serviceStatus{};
SERVICE_STATUS_HANDLE UltraVNCService::serviceStatusHandle = nullptr;
std::array<char, MAX_PATH> UltraVNCService::service_path{};
std::array<char, 256U> UltraVNCService::service_name{"uvnc_service"};
std::array<char, MAX_PATH> UltraVNCService::app_path{};
int UltraVNCService::kickrdp = 0;
PROCESS_INFORMATION UltraVNCService::ProcessInfo{};
bool UltraVNCService::IsShutdown = false;
HANDLE UltraVNCService::hEndSessionEvent = nullptr;
HANDLE UltraVNCService::hEvent = nullptr;
int UltraVNCService::clear_console = 0;
char* UltraVNCService::app_name = "UltraVNC";
std::array<char, 256U> UltraVNCService::cmdtext{};
std::array<char, MAX_PATH> UltraVNCService::configfilename{};
std::array<char, MAX_PATH> UltraVNCService::inifile{};
IniFile UltraVNCService::iniFileService;

// Constants for portable mode and configuration
const char* SERVICE_PORTABLE_MARKER_FILE = "ultravnc.portable";
const char* SERVICE_CONFIG_SUBFOLDER = "\\UltraVNC";

void GetServiceExecutablePath(char* const path, const std::size_t size)
{
    if (::GetModuleFileName(nullptr, path, static_cast<DWORD>(size)))
    {
        char* p = std::strrchr(path, '\\');
        if (p)
        {
            *p = '\0';
        }
    }
    else
    {
        path[0U] = '\0';
    }
}

// Check if running in portable mode (service version)
// Portable mode is detected by the presence of "ultravnc.portable" file in the application folder
bool IsServicePortableMode(const char* const appFolder)
{
    std::array<char, MAX_PATH> portableMarker{};
    ::strcpy_s(portableMarker.data(), portableMarker.size(), appFolder);
    ::strcat_s(portableMarker.data(), portableMarker.size(), "\\");
    ::strcat_s(portableMarker.data(), portableMarker.size(), SERVICE_PORTABLE_MARKER_FILE);

    // Check if marker file exists
    std::ifstream markerFile(portableMarker.data());
    const bool isPortable = markerFile.good();
    markerFile.close();

    return isPortable;
}

// Migrate INI file from install folder to ProgramData (service version)
// Returns true if migration was performed or file already exists in target
bool MigrateServiceIniToProgramData(const char* const installFolderPath, const char* const programDataPath,
                                    const char* const configFileName)
{
    std::array<char, MAX_PATH> sourceIni{};
    std::array<char, MAX_PATH> targetIni{};
    std::array<char, MAX_PATH> targetFolder{};

    // Build paths
    ::strcpy_s(sourceIni.data(), sourceIni.size(), installFolderPath);
    ::strcat_s(sourceIni.data(), sourceIni.size(), "\\");
    ::strcat_s(sourceIni.data(), sourceIni.size(), configFileName);

    ::strcpy_s(targetIni.data(), targetIni.size(), programDataPath);
    ::strcat_s(targetIni.data(), targetIni.size(), SERVICE_CONFIG_SUBFOLDER);
    ::strcpy_s(targetFolder.data(), targetFolder.size(), targetIni.data());
    ::strcat_s(targetIni.data(), targetIni.size(), "\\");
    ::strcat_s(targetIni.data(), targetIni.size(), configFileName);

    // Check if target already exists
    std::ifstream targetFile(targetIni.data());
    if (targetFile.good())
    {
        targetFile.close();
        return true;
    }
    targetFile.close();

    // Check if source exists
    std::ifstream sourceFile(sourceIni.data());
    if (!sourceFile.good())
    {
        sourceFile.close();
        return false;
    }
    sourceFile.close();

    // Create target directory if it doesn't exist
    if ((::_mkdir(targetFolder.data()) != 0) && (errno != EEXIST))
    {
        // Directory creation failed, but continue to try copying anyway
        // in case the directory exists but _mkdir still failed
    }

    // Copy the file
    if (::CopyFileA(sourceIni.data(), targetIni.data(), FALSE))
    {
        return true;
    }
    return false;
}

UltraVNCService::UltraVNCService() {}

////////////////////////////////////////////////////////////////////////////////
void WINAPI UltraVNCService::service_main([[maybe_unused]] const DWORD argc, LPTSTR* const argv)
{
    ::SetDllDirectory(TEXT(""));
    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);

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
    /* initialise service status */
    serviceStatus.dwServiceType = SERVICE_WIN32;
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwWin32ExitCode = NO_ERROR;
    serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;
    if (std::strcmp(argv[0U], service_name.data()) == 0)
    {
        ::strcpy_s(configfilename.data(), configfilename.size(), "ultravnc.ini");
    }
    else
    {
        ::strcpy_s(configfilename.data(), configfilename.size(), argv[0U]);
        ::strcat_s(configfilename.data(), configfilename.size(), ".ini");
    }

    std::array<char, MAX_PATH> programdataPath{};
    std::array<char, MAX_PATH> programdataFolder{};
    std::array<char, MAX_PATH> installFolder{};

    // Get install folder
    GetServiceExecutablePath(installFolder.data(), installFolder.size());

    // Check if running in portable mode
    if (IsServicePortableMode(installFolder.data()))
    {
        // Portable mode: Use install folder for config
        ::strcpy_s(inifile.data(), inifile.size(), installFolder.data());
        ::strcat_s(inifile.data(), inifile.size(), "\\");
        ::strcat_s(inifile.data(), inifile.size(), configfilename.data());
    }
    else
    {
        // Standard mode: Use ProgramData for shared configuration
        // Get ProgramData path
        ::SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programdataPath.data());

        // Attempt to migrate from install folder to ProgramData
        MigrateServiceIniToProgramData(installFolder.data(), programdataPath.data(), configfilename.data());

        // Build ProgramData INI path
        ::strcpy_s(programdataFolder.data(), programdataFolder.size(), programdataPath.data());
        ::strcat_s(programdataFolder.data(), programdataFolder.size(), SERVICE_CONFIG_SUBFOLDER);
        ::strcpy_s(inifile.data(), inifile.size(), programdataFolder.data());
        ::strcat_s(inifile.data(), inifile.size(), "\\");
        ::strcat_s(inifile.data(), inifile.size(), configfilename.data());

        // Check if config exists, create folder if needed
        std::ifstream file(inifile.data());
        if (!file.good())
        {
            // Config doesn't exist, ensure directory exists
            if ((::_mkdir(programdataFolder.data()) != 0) && (errno != EEXIST))
            {
                // Failed to create directory, continue anyway
            }
        }
    }

    iniFileService.setIniFile(inifile.data());
    settings->Initialize(inifile.data());

    typedef SERVICE_STATUS_HANDLE(WINAPI * pfnRegisterServiceCtrlHandlerEx)(LPCTSTR, LPHANDLER_FUNCTION_EX, LPVOID);
    helper::DynamicFn<pfnRegisterServiceCtrlHandlerEx> pRegisterServiceCtrlHandlerEx("advapi32.dll",
                                                                                     "RegisterServiceCtrlHandlerExA");

    if (pRegisterServiceCtrlHandlerEx.isValid())
    {
        serviceStatusHandle = (*pRegisterServiceCtrlHandlerEx)(service_name.data(), control_handler_ex, 0);
    }
    else
    {
        serviceStatusHandle = ::RegisterServiceCtrlHandler(service_name.data(), control_handler);
    }

    if (serviceStatusHandle)
    {
        /* service is starting */
        serviceStatus.dwCurrentState = SERVICE_START_PENDING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);

        /* running */
        serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwControlsAccepted |= SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_CONTROL_PRESHUTDOWN;

        serviceStatus.dwCurrentState = SERVICE_RUNNING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);

        Restore_after_reboot();

        while (serviceStatus.dwCurrentState == SERVICE_RUNNING)
        {
            monitorSessions();
            ::Sleep(3000);
        }


        /* service was stopped */
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);

        /* service is now stopped */
        serviceStatus.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);
    }
}
////////////////////////////////////////////////////////////////////////////////
void WINAPI UltraVNCService::control_handler(const DWORD controlCode) { control_handler_ex(controlCode, 0, 0, 0); }
DWORD WINAPI UltraVNCService::control_handler_ex(DWORD controlCode, const DWORD dwEventType, [[maybe_unused]] const LPVOID lpEventData,
                                                 [[maybe_unused]] const LPVOID lpContext)
{
    switch (controlCode)
    {
    case SERVICE_CONTROL_INTERROGATE:
        break;

    case SERVICE_CONTROL_PRESHUTDOWN:
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        ::SetServiceStatus(serviceStatusHandle, &serviceStatus);
        ::SetEvent(hEndSessionEvent);
        ::SetEvent(hEvent);
        return NO_ERROR;

    case SERVICE_CONTROL_PAUSE:
        break;

    case SERVICE_CONTROL_CONTINUE:
        break;

    case SERVICE_CONTROL_SESSIONCHANGE: {
        if (dwEventType == WTS_REMOTE_DISCONNECT)
        {
            // disconnect rdp, and reconnect to the console
            if (clear_console)
            {
                disconnect_remote_sessions();
            }
        }
    }
    break;

    default:
        if ((controlCode >= 128) && (controlCode <= 255))
        {
            break;
        }
        else
        {
            break;
        }
    }
    ::SetServiceStatus(serviceStatusHandle, &serviceStatus);
    return NO_ERROR;
}
////////////////////////////////////////////////////////////////////////////////
int UltraVNCService::start_service(char* const cmd)
{
    ::strcpy_s(cmdtext.data(), 256, cmd);
    SERVICE_TABLE_ENTRY serviceTable[] = { { service_name.data(), service_main }, { 0, 0 } };

    if (!::StartServiceCtrlDispatcher(serviceTable))
    {
        return 1;
    }
    return 0; /* NT service started */
}
////////////////////////////////////////////////////////////////////////////////
void UltraVNCService::set_service_description()
{
    // Add service description
    DWORD dw;
    HKEY hKey;
    std::array<char, 256U> tempName;
    char desc[] = "UltraVNC Server provides secure remote desktop sharing";
    ::_snprintf_s(tempName.data(), tempName.size(), sizeof tempName, "SYSTEM\\CurrentControlSet\\Services\\%s", service_name.data());
    ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, tempName.data(), 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, nullptr, &hKey,
                   &dw);
    ::RegSetValueEx(hKey, "Description", 0, REG_SZ, (const BYTE*)desc, static_cast<DWORD>(std::strlen(desc) + 1));


    ::RegCloseKey(hKey);
}


// List of other required services ("dependency 1\0dependency 2\0\0")
// *** These need filling in properly
#define VNCDEPENDENCIES "Tcpip\0\0"



int UltraVNCService::install_service(void)
{
    SC_HANDLE scm, service;
    pad();

    scm = ::OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
    {
        MessageBoxSecure(nullptr, sz_ID_FAILED_OPEN_SVC_MGR.data(), app_name, MB_ICONERROR);
        return 1;
    }
    //"UltraVNC Server provides secure remote desktop sharing"
    service = ::CreateService(scm, service_name.data(), service_name.data(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                            SERVICE_ERROR_NORMAL, service_path.data(), nullptr, nullptr, VNCDEPENDENCIES, nullptr, nullptr);
    if (!service)
    {
        DWORD myerror = ::GetLastError();
        if (myerror == ERROR_ACCESS_DENIED)
        {
            MessageBoxSecure(nullptr, sz_ID_FAILED_PERMISSION_DENIED.data(), app_name, MB_ICONERROR);
            ::CloseServiceHandle(scm);
            return 1;
        }
        if (myerror == ERROR_SERVICE_EXISTS)
        {
            ::CloseServiceHandle(scm);
            return 1;
        }

        MessageBoxSecure(nullptr, sz_ID_FAILED_CREATE_SVC.data(), app_name, MB_ICONERROR);
        ::CloseServiceHandle(scm);
        return 1;
    }
    else
    {
        set_service_description();
    }
    ::CloseServiceHandle(service);
    ::CloseServiceHandle(scm);
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
int UltraVNCService::uninstall_service(void)
{
    SC_HANDLE scm, service;
    SERVICE_STATUS serviceStatusIn;

    scm = ::OpenSCManager(0, 0, SC_MANAGER_CONNECT);
    if (!scm)
    {
        MessageBoxSecure(nullptr, sz_ID_FAILED_OPEN_SVC_MGR.data(), app_name, MB_ICONERROR);
        return 1;
    }

    service = ::OpenService(scm, service_name.data(), SERVICE_QUERY_STATUS | DELETE);
    if (!service)
    {
        DWORD myerror = ::GetLastError();
        if (myerror == ERROR_ACCESS_DENIED)
        {
            MessageBoxSecure(nullptr, sz_ID_FAILED_PERMISSION_DENIED.data(), app_name, MB_ICONERROR);
            ::CloseServiceHandle(scm);
            return 1;
        }
        if (myerror == ERROR_SERVICE_DOES_NOT_EXIST)
        {
#if 0
			MessageBoxSecure(nullptr, "Failed: Service is not installed",
            app_name, MB_ICONERROR);
#endif
            ::CloseServiceHandle(scm);
            return 1;
        }

        MessageBoxSecure(nullptr, sz_ID_FAILED_OPEN_SVC.data(), app_name, MB_ICONERROR);
        ::CloseServiceHandle(scm);
        return 1;
    }
    if (!::QueryServiceStatus(service, &serviceStatusIn))
    {
        MessageBoxSecure(nullptr, sz_ID_FAILED_QUERY_SVC_STATUS.data(), app_name, MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return 1;
    }
    if (serviceStatusIn.dwCurrentState != SERVICE_STOPPED)
    {
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        ::Sleep(2500);
        uninstall_service();
        return 1;
    }
    if (!::DeleteService(service))
    {
        MessageBoxSecure(nullptr, sz_ID_FAILED_DELETE_SVC.data(), app_name, MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return 1;
    }
    ::CloseServiceHandle(service);
    ::CloseServiceHandle(scm);
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
int UltraVNCService::pad()
{
    std::array<char, MAX_PATH> dir;
    char* ptr;
    std::array<char, MAX_PATH> exe_file_name;
    ::GetModuleFileName(0, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));
    /* set current directory */
    ::strcpy_s(dir.data(), dir.size(), exe_file_name.data());
    ptr = std::strrchr(dir.data(), '\\'); /* last backslash */
    if (ptr)
    {
        ptr[1U] = '\0';
    } /* truncate program name */
    if (!::SetCurrentDirectory(dir.data()))
    {
        return 1;
    }

    ::strcpy_s(service_path.data(), service_path.size(), "\"");
    ::strcat_s(service_path.data(), service_path.size(), exe_file_name.data());
    ::strcat_s(service_path.data(), service_path.size(), "\"");
    ::strcat_s(service_path.data(), service_path.size(), " -service");
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
BOOL UltraVNCService::CreateServiceSafeBootKey()
{
    HKEY hKey;
    DWORD dwDisp{ 0U };
    LONG lSuccess;
    std::array<char, 1024U> szKey;
    ::_snprintf_s(szKey.data(), szKey.size(), 1024, "SYSTEM\\CurrentControlSet\\Control\\SafeBoot\\%s\\%s", "Network", service_name.data());
    lSuccess = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, szKey.data(), 0L, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                              &hKey, &dwDisp);
    if (lSuccess == ERROR_SUCCESS)
    {
        ::RegSetValueEx(hKey, nullptr, 0, REG_SZ, (unsigned char*)"Service", 8);
        ::RegCloseKey(hKey);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}
////////////////////////////////////////////////////////////////////////////////
void UltraVNCService::Set_Safemode()
{
    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    if (OSversion.dwMajorVersion < 6)
    {
        std::array<char, 150U> drivepath;
        std::array<char, 150U> systemdrive;
        std::array<char, 512U> stringvalue;
        ::GetEnvironmentVariable("SYSTEMDRIVE", systemdrive.data(), 150);
        ::strcat_s(systemdrive.data(), systemdrive.size(), "/boot.ini");
        ::GetPrivateProfileString("boot loader", "default", "", drivepath.data(), 150, systemdrive.data());
        if (std::strlen(drivepath.data()) == 0)
        {
            return;
        }
        ::GetPrivateProfileString("operating systems", drivepath.data(), "", stringvalue.data(), 512, systemdrive.data());
        if (std::strlen(stringvalue.data()) == 0)
        {
            return;
        }
        ::strcat_s(stringvalue.data(), stringvalue.size(), " /safeboot:network");
        ::SetFileAttributes(systemdrive.data(), FILE_ATTRIBUTE_NORMAL);
        ::WritePrivateProfileString("operating systems", drivepath.data(), stringvalue.data(), systemdrive.data());
        [[maybe_unused]] const DWORD err = ::GetLastError();
    }
    else
    {

#ifdef _X64
        std::array<char, 150U> systemroot;
        ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
        std::array<char, MAX_PATH> exe_file_name;
        std::array<char, MAX_PATH> parameters;
        ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
        ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
        ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
        ::strcpy_s(parameters.data(), parameters.size(), "/set safeboot network");
        SHELLEXECUTEINFO shExecInfo;
        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = parameters.data();
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_HIDE;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
#else

        typedef BOOL(WINAPI * LPFN_Wow64DisableWow64FsRedirection)(PVOID * OldValue);
        typedef BOOL(WINAPI * LPFN_Wow64RevertWow64FsRedirection)(PVOID OldValue);
        PVOID OldValue;
        LPFN_Wow64DisableWow64FsRedirection pfnWow64DisableWowFsRedirection =
            (LPFN_Wow64DisableWow64FsRedirection)::GetProcAddress(::GetModuleHandle("kernel32"), "Wow64DisableWow64FsRedirection");
        LPFN_Wow64RevertWow64FsRedirection pfnWow64RevertWow64FsRedirection =
            (LPFN_Wow64RevertWow64FsRedirection)::GetProcAddress(::GetModuleHandle("kernel32"), "Wow64RevertWow64FsRedirection");
        if (pfnWow64DisableWowFsRedirection && pfnWow64RevertWow64FsRedirection)
        {
            if (TRUE == pfnWow64DisableWowFsRedirection(&OldValue))
            {
                std::array<char, 150U> systemroot;
                ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
                std::array<char, MAX_PATH> exe_file_name;
                std::array<char, MAX_PATH> parameters;
                ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
                ::strcpy_s(parameters.data(), parameters.size(), "/set safeboot network");
                SHELLEXECUTEINFO shExecInfo;
                shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
                shExecInfo.fMask = nullptr;
                shExecInfo.hwnd = ::GetForegroundWindow();
                shExecInfo.lpVerb = "runas";
                shExecInfo.lpFile = exe_file_name.data();
                shExecInfo.lpParameters = parameters.data();
                shExecInfo.lpDirectory = nullptr;
                shExecInfo.nShow = SW_HIDE;
                shExecInfo.hInstApp = nullptr;
                ::ShellExecuteEx(&shExecInfo);
                pfnWow64RevertWow64FsRedirection(OldValue);
            }
            else
            {
                std::array<char, 150U> systemroot;
                ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
                std::array<char, MAX_PATH> exe_file_name;
                std::array<char, MAX_PATH> parameters;
                ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
                ::strcpy_s(parameters.data(), parameters.size(), "/set safeboot network");
                SHELLEXECUTEINFO shExecInfo;
                shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
                shExecInfo.fMask = nullptr;
                shExecInfo.hwnd = ::GetForegroundWindow();
                shExecInfo.lpVerb = "runas";
                shExecInfo.lpFile = exe_file_name.data();
                shExecInfo.lpParameters = parameters.data();
                shExecInfo.lpDirectory = nullptr;
                shExecInfo.nShow = SW_HIDE;
                shExecInfo.hInstApp = nullptr;
                ::ShellExecuteEx(&shExecInfo);
            }
        }
        else
        {
            std::array<char, 150U> systemroot;
            ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
            std::array<char, MAX_PATH> exe_file_name;
            std::array<char, MAX_PATH> parameters;
            ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
            ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
            ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
            ::strcpy_s(parameters.data(), parameters.size(), "/set safeboot network");
            SHELLEXECUTEINFO shExecInfo;
            shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
            shExecInfo.fMask = nullptr;
            shExecInfo.hwnd = ::GetForegroundWindow();
            shExecInfo.lpVerb = "runas";
            shExecInfo.lpFile = exe_file_name.data();
            shExecInfo.lpParameters = parameters.data();
            shExecInfo.lpDirectory = nullptr;
            shExecInfo.nShow = SW_HIDE;
            shExecInfo.hInstApp = nullptr;
            ::ShellExecuteEx(&shExecInfo);
        }
#endif
    }
}

BOOL UltraVNCService::reboot()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        ::LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0U].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0U].Attributes = SE_PRIVILEGE_ENABLED;
        if (::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES) nullptr, 0))
        {
            ::ExitWindowsEx(EWX_REBOOT, 0);
        }
    }
    return TRUE;
}

BOOL UltraVNCService::Force_reboot()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        ::LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0U].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0U].Attributes = SE_PRIVILEGE_ENABLED;
        if (::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES) nullptr, 0))
        {
            OSVERSIONINFO OSversion;
            OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            GetVersionEx(&OSversion);
            if (OSversion.dwMajorVersion < 6)
            {
                ::ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, 0);
            }
            else
            {
                ::ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
            }
        }
    }
    return TRUE;
}

void UltraVNCService::Reboot_with_force_reboot() { Force_reboot(); }

void UltraVNCService::Reboot_with_force_reboot_elevated()
{
    std::array<char, MAX_PATH> exe_file_name;
    ::GetModuleFileName(0, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));
    SHELLEXECUTEINFO shExecInfo;
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.fMask = 0;
    shExecInfo.hwnd = ::GetForegroundWindow();
    shExecInfo.lpVerb = "runas";
    shExecInfo.lpFile = exe_file_name.data();
    shExecInfo.lpParameters = "-rebootforce";
    shExecInfo.lpDirectory = nullptr;
    shExecInfo.nShow = SW_HIDE;
    shExecInfo.hInstApp = nullptr;
    ::ShellExecuteEx(&shExecInfo);
}

void UltraVNCService::Reboot_in_safemode()
{
    if (CreateServiceSafeBootKey())
    {
        Set_Safemode();
        reboot();
    }
}

void UltraVNCService::Reboot_in_safemode_elevated()
{
    std::array<char, MAX_PATH> exe_file_name;
    ::GetModuleFileName(0, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));
    SHELLEXECUTEINFO shExecInfo;
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.fMask = 0;
    shExecInfo.hwnd = ::GetForegroundWindow();
    shExecInfo.lpVerb = "runas";
    shExecInfo.lpFile = exe_file_name.data();
    shExecInfo.lpParameters = "-rebootsafemode";
    shExecInfo.lpDirectory = nullptr;
    shExecInfo.nShow = SW_HIDE;
    shExecInfo.hInstApp = nullptr;
    ::ShellExecuteEx(&shExecInfo);
}

////////////////// ALL /////////////////////////////
////////////////////////////////////////////////////

BOOL UltraVNCService::DeleteServiceSafeBootKey()
{
    LONG lSuccess;
    std::array<char, 1024U> szKey;
    ::_snprintf_s(szKey.data(), szKey.size(), 1024, "SYSTEM\\CurrentControlSet\\Control\\SafeBoot\\%s\\%s", "Network", service_name.data());
    lSuccess = ::RegDeleteKey(HKEY_LOCAL_MACHINE, szKey.data());
    return lSuccess == ERROR_SUCCESS;
}

void UltraVNCService::Restore_safemode()
{
    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    if (OSversion.dwMajorVersion < 6)
    {
        std::array<char, 150U> drivepath;
        std::array<char, 150U> systemdrive;
        std::array<char, 512U> stringvalue;
        ::GetEnvironmentVariable("SYSTEMDRIVE", systemdrive.data(), 150);
        ::strcat_s(systemdrive.data(), systemdrive.size(), "/boot.ini");
        ::GetPrivateProfileString("boot loader", "default", "", drivepath.data(), 150, systemdrive.data());
        if (std::strlen(drivepath.data()) == 0)
        {
            return;
        }
        ::GetPrivateProfileString("operating systems", drivepath.data(), "", stringvalue.data(), 512, systemdrive.data());
        if (std::strlen(stringvalue.data()) == 0)
        {
            return;
        }
        char* p = std::strrchr(stringvalue.data(), '/');
        if (p == nullptr)
        {
            return;
        }
        *p = '\0';
        ::WritePrivateProfileString("operating systems", drivepath.data(), stringvalue.data(), systemdrive.data());
        ::SetFileAttributes(systemdrive.data(), FILE_ATTRIBUTE_READONLY);
    }
    else
    {
#ifdef _X64
        std::array<char, 150U> systemroot;
        ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
        std::array<char, MAX_PATH> exe_file_name;
        std::array<char, MAX_PATH> parameters;
        ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
        ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
        ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
        ::strcpy_s(parameters.data(), parameters.size(), "/deletevalue safeboot");
        SHELLEXECUTEINFO shExecInfo;
        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = parameters.data();
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_HIDE;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
#else
        typedef BOOL(WINAPI * LPFN_Wow64DisableWow64FsRedirection)(PVOID * OldValue);
        typedef BOOL(WINAPI * LPFN_Wow64RevertWow64FsRedirection)(PVOID OldValue);
        PVOID OldValue;
        LPFN_Wow64DisableWow64FsRedirection pfnWow64DisableWowFsRedirection =
            (LPFN_Wow64DisableWow64FsRedirection)::GetProcAddress(::GetModuleHandle("kernel32"), "Wow64DisableWow64FsRedirection");
        LPFN_Wow64RevertWow64FsRedirection pfnWow64RevertWow64FsRedirection =
            (LPFN_Wow64RevertWow64FsRedirection)::GetProcAddress(::GetModuleHandle("kernel32"), "Wow64RevertWow64FsRedirection");
        if (pfnWow64DisableWowFsRedirection && pfnWow64RevertWow64FsRedirection) ///win32 on x64 system
        {
            if (TRUE == pfnWow64DisableWowFsRedirection(&OldValue))
            {
                std::array<char, 150U> systemroot;
                ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
                std::array<char, MAX_PATH> exe_file_name;
                std::array<char, MAX_PATH> parameters;
                ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
                ::strcpy_s(parameters.data(), parameters.size(), "/deletevalue safeboot");
                SHELLEXECUTEINFO shExecInfo;
                shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
                shExecInfo.fMask = nullptr;
                shExecInfo.hwnd = ::GetForegroundWindow();
                shExecInfo.lpVerb = "runas";
                shExecInfo.lpFile = exe_file_name.data();
                shExecInfo.lpParameters = parameters.data();
                shExecInfo.lpDirectory = nullptr;
                shExecInfo.nShow = SW_HIDE;
                shExecInfo.hInstApp = nullptr;
                ::ShellExecuteEx(&shExecInfo);
                pfnWow64RevertWow64FsRedirection(OldValue);
            }
            else
            {
                std::array<char, 150U> systemroot;
                ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
                std::array<char, MAX_PATH> exe_file_name;
                std::array<char, MAX_PATH> parameters;
                ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
                ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
                ::strcpy_s(parameters.data(), parameters.size(), "/deletevalue safeboot");
                SHELLEXECUTEINFO shExecInfo;
                shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
                shExecInfo.fMask = nullptr;
                shExecInfo.hwnd = ::GetForegroundWindow();
                shExecInfo.lpVerb = "runas";
                shExecInfo.lpFile = exe_file_name.data();
                shExecInfo.lpParameters = parameters.data();
                shExecInfo.lpDirectory = nullptr;
                shExecInfo.nShow = SW_HIDE;
                shExecInfo.hInstApp = nullptr;
                ::ShellExecuteEx(&shExecInfo);
            }
        }
        else //win32 on W32
        {
            std::array<char, 150U> systemroot;
            ::GetEnvironmentVariable("SystemRoot", systemroot.data(), 150);
            std::array<char, MAX_PATH> exe_file_name;
            std::array<char, MAX_PATH> parameters;
            ::strcpy_s(exe_file_name.data(), exe_file_name.size(), systemroot.data());
            ::strcat_s(exe_file_name.data(), exe_file_name.size(), "\\system32\\");
            ::strcat_s(exe_file_name.data(), exe_file_name.size(), "bcdedit.exe");
            ::strcpy_s(parameters.data(), parameters.size(), "/deletevalue safeboot");
            SHELLEXECUTEINFO shExecInfo;
            shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
            shExecInfo.fMask = nullptr;
            shExecInfo.hwnd = ::GetForegroundWindow();
            shExecInfo.lpVerb = "runas";
            shExecInfo.lpFile = exe_file_name.data();
            shExecInfo.lpParameters = parameters.data();
            shExecInfo.lpDirectory = nullptr;
            shExecInfo.nShow = SW_HIDE;
            shExecInfo.hInstApp = nullptr;
            ::ShellExecuteEx(&shExecInfo);
        }
#endif
    }
}

void UltraVNCService::Restore_after_reboot()
{
    //If we are running !normal mode
    //disable boot.ini /safemode:network
    //disable safeboot service
    if (GetSystemMetrics(SM_CLEANBOOT) != 0)
    {
        Restore_safemode();
        DeleteServiceSafeBootKey();
    }
}

void UltraVNCService::disconnect_remote_sessions()
{
    typedef BOOLEAN(WINAPI * pWinStationConnect)(HANDLE, ULONG, ULONG, PCWSTR, ULONG);
    HMODULE hlibwinsta = ::LoadLibraryEx("winsta.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    pWinStationConnect WinStationConnectF = nullptr;

    // don't kick rdp off if there's still an active session
    if (IsAnyRDPSessionActive())
    {
        return;
    }
    if (hlibwinsta)
    {
        WinStationConnectF = (pWinStationConnect)::GetProcAddress(hlibwinsta, "WinStationConnectW");
    }
    if (WinStationConnectF != nullptr)
    {
        DWORD ID = ::WTSGetActiveConsoleSessionId();
        WinStationConnectF(0, 0, ID, L"", 0);
        // sleep to allow the system to finish the connect/disconnect process. If we don't
        // then the workstation won't get locked every time.
        ::Sleep(3000);
        if (!::LockWorkStation())
        {
            std::array<char, 1024U> msg;
            ::sprintf_s(msg.data(), msg.size(), "LockWorkstation failed with error 0x%0lX", ::GetLastError());
            ::OutputDebugString(msg.data());
        }
    }
    ::Sleep(3000);
    if (hlibwinsta)
    {
        ::FreeLibrary(hlibwinsta);
    }
}

bool UltraVNCService::IsAnyRDPSessionActive()
{
    WTS_SESSION_INFO* pSessions = 0;
    DWORD nSessions(0U);
    DWORD rdpSessionExists = false;

    typedef BOOL(WINAPI * pfnWTSEnumerateSessions)(HANDLE, DWORD, DWORD, PWTS_SESSION_INFO*, DWORD*);
    typedef VOID(WINAPI * pfnWTSFreeMemory)(PVOID);

    helper::DynamicFn<pfnWTSEnumerateSessions> pWTSEnumerateSessions("wtsapi32", "WTSEnumerateSessionsA");
    helper::DynamicFn<pfnWTSFreeMemory> pWTSFreeMemory("wtsapi32", "WTSFreeMemory");

    if (pWTSEnumerateSessions.isValid() && pWTSFreeMemory.isValid())
        if ((*pWTSEnumerateSessions)(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &nSessions))
        {
            for (DWORD i(0U); (i < nSessions) && !rdpSessionExists; ++i)
            {
                if ((::_stricmp(pSessions[i].pWinStationName, "Console") != 0) &&
                    ((pSessions[i].State == WTSActive) || (pSessions[i].State == WTSShadow) ||
                     (pSessions[i].State == WTSConnectQuery)))
                {
                    rdpSessionExists = true;
                }
            }
            (*pWTSFreeMemory)(pSessions);
        }
    return rdpSessionExists ? true : false;
}


int UltraVNCService::createWinvncExeCall(const bool preconnect, const bool rdpselect)
{
    std::array<char, MAX_PATH> exe_file_name;
    ::GetModuleFileName(0, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));
    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    std::array<char, MAX_PATH> cmdline;
    ::strcpy_s(app_path.data(), app_path.size(), exe_file_name.data());
    ::strcat_s(app_path.data(), app_path.size(), " -config \"");
    ::strcat_s(app_path.data(), app_path.size(), UltraVNCService::inifile.data());
    ::strcat_s(app_path.data(), app_path.size(), "\"");
    if (preconnect)
    {
        ::strcat_s(app_path.data(), app_path.size(), " -preconnect");
    }
    if (rdpselect)
    {
        ::strcat_s(app_path.data(), app_path.size(), " -service_rdp_run");
    }
    else
    {
        ::strcat_s(app_path.data(), app_path.size(), " -service_run");
    }

    kickrdp = iniFileService.ReadInt("admin", "kickrdp", kickrdp);
    clear_console = iniFileService.ReadInt("admin", "clearconsole", clear_console);
    iniFileService.ReadString("admin", "service_commandline", cmdline.data(), 256);
    if (std::strlen(cmdline.data()) != 0)
    {
        ::strcpy_s(app_path.data(), app_path.size(), exe_file_name.data());
        ::strcat_s(app_path.data(), app_path.size(), " -config \"");
        ::strcat_s(app_path.data(), app_path.size(), UltraVNCService::inifile.data());
        ::strcat_s(app_path.data(), app_path.size(), "\"");
        if (preconnect)
        {
            ::strcat_s(app_path.data(), app_path.size(), " -preconnect");
        }
        ::strcat_s(app_path.data(), app_path.size(), " ");
        ::strcat_s(app_path.data(), app_path.size(), cmdline.data());
        if (rdpselect)
        {
            ::strcat_s(app_path.data(), app_path.size(), " -service_rdp_run");
        }
        else
        {
            ::strcat_s(app_path.data(), app_path.size(), " -service_run");
        }
    }
    return 0;
}


void UltraVNCService::monitorSessions()
{
    BOOL RDPMODE{ false };
    RDPMODE = iniFileService.ReadInt("admin", "rdpmode", 0);
    createWinvncExeCall(false, false);
    DWORD requestedSessionID{ 0U };
    DWORD dwSessionId{ 0U };
    DWORD OlddwSessionId{ 99U };
    ProcessInfo.hProcess = 0;
    std::array<HANDLE, 3U> testevent3;
    std::array<HANDLE, 2U> testevent2;
    IsShutdown = false;
    bool preconnect_start{ false };
    HANDLE hEventIn = nullptr;
    HANDLE hEventcad = nullptr;
    HANDLE hEventPreConnect = nullptr;
    HANDLE hMapFile = nullptr;
    LPVOID data = nullptr;

    //We use this event to notify the program that the session has changed
    //The program need to end so the service can restart the program in the correct session
    wait_for_existing_process();
    hEventIn = ::CreateEvent(nullptr, FALSE, FALSE, "Global\\SessionEventUltra");
    hEventcad = ::CreateEvent(nullptr, FALSE, FALSE, "Global\\SessionEventUltraCad");
    hEventPreConnect = ::CreateEvent(nullptr, FALSE, FALSE, "Global\\SessionEventUltraPreConnect");
    hEndSessionEvent = ::CreateEvent(nullptr, FALSE, FALSE, "Global\\EndSessionEvent");
    hMapFile = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(int), "Global\\SessionUltraPreConnect");
    if (hMapFile)
    {
        data = ::MapViewOfFile(hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    }
    ::Sleep(3000);
    int* a = (int*)data;
    testevent3[0U] = hEndSessionEvent;
    testevent3[1U] = hEventcad;
    testevent3[2U] = hEventPreConnect;
    testevent2[0U] = hEndSessionEvent;
    testevent2[1U] = hEventcad;

    //IsAnyRDPSessionActive()
    while (!IsShutdown && (serviceStatus.dwCurrentState == SERVICE_RUNNING))
    {
        DWORD dwEvent;
        if (RDPMODE)
        {
            dwEvent = ::WaitForMultipleObjects(3, testevent3.data(), FALSE, 1000);
        }
        else
        {
            dwEvent = ::WaitForMultipleObjects(2, testevent2.data(), FALSE, 1000);
        }

        switch (dwEvent)
        {

            // We get some preconnect session selection input
        case WAIT_OBJECT_0 + 2: {
            //Tell UltraVNC Server to stop
            ::SetEvent(hEventIn);
            requestedSessionID = *a;
            //We always have a process handle, else we could not get the signal from it.
            DWORD dwCode = STILL_ACTIVE;
            while ((dwCode == STILL_ACTIVE) && (ProcessInfo.hProcess != nullptr) && !IsShutdown)
            {
                ::GetExitCodeProcess(ProcessInfo.hProcess, &dwCode);
                if (dwCode != STILL_ACTIVE)
                {
                    ::WaitForSingleObject(ProcessInfo.hProcess, 15000);
                    if (ProcessInfo.hProcess)
                    {
                        ::CloseHandle(ProcessInfo.hProcess);
                    }
                    if (ProcessInfo.hThread)
                    {
                        ::CloseHandle(ProcessInfo.hThread);
                    }
                }
                else
                {
                    ::Sleep(1000);
                }
            }

            dwSessionId = 0xFFFFFFFF;
            int sessidcounter{ 0 };
            while ((dwSessionId == 0xFFFFFFFF) && !IsShutdown)
            {
                dwSessionId = ::WTSGetActiveConsoleSessionId();
                ::Sleep(1000);
                ++sessidcounter;
                if (sessidcounter > 10)
                {
                    break;
                }
            }
            LaunchProcessWin(requestedSessionID, false, true);
            OlddwSessionId = requestedSessionID;
            preconnect_start = true;
        }
        break;

        //stopServiceEvent, exit while loop
        case WAIT_OBJECT_0 + 0:
            IsShutdown = true;
            break;

            //cad request
        case WAIT_OBJECT_0 + 1: {
            typedef VOID(WINAPI * SendSas)(BOOL asUser);
            HINSTANCE Inst = ::LoadLibraryEx("sas.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            SendSas sendSas = (SendSas)::GetProcAddress(Inst, "SendSAS");
            if (sendSas)
            {
                sendSas(FALSE);
            }
            else
            {
                std::array<char, MAX_PATH> WORKDIR;
                std::array<char, MAX_PATH> mycommand;
                if (::GetModuleFileName(nullptr, WORKDIR.data(), static_cast<DWORD>(WORKDIR.size())))
                {
                    char* p = std::strrchr(WORKDIR.data(), '\\');
                    if (p == nullptr)
                    {
                        return;
                    }
                    *p = '\0';
                }
                ::strcpy_s(mycommand.data(), mycommand.size(), "");
                ::strcat_s(mycommand.data(), mycommand.size(), WORKDIR.data()); //set the directory
                ::strcat_s(mycommand.data(), mycommand.size(), "\\");
                ::strcat_s(mycommand.data(), mycommand.size(), "cad.exe");
                (void)::ShellExecute(::GetDesktopWindow(), "open", mycommand.data(), "", 0, SW_SHOWNORMAL);
            }
            if (Inst)
            {
                ::FreeLibrary(Inst);
            }
        }
        break;

        case WAIT_TIMEOUT:
            if (RDPMODE && IsAnyRDPSessionActive())
            {
                //First RUN
                if (ProcessInfo.hProcess == nullptr)
                {
                    if (IsAnyRDPSessionActive())
                    {
                        LaunchProcessWin(0, true, false);
                        OlddwSessionId = 0;
                        preconnect_start = false;
                        goto whileloop;
                    }
                    else
                    {
                        dwSessionId = 0xFFFFFFFF;
                        int sessidcounter{ 0 };
                        while ((dwSessionId == 0xFFFFFFFF) && !IsShutdown)
                        {
                            dwSessionId = ::WTSGetActiveConsoleSessionId();
                            ::Sleep(1000);
                            ++sessidcounter;
                            if (sessidcounter > 10)
                            {
                                break;
                            }
                        }
                        LaunchProcessWin(dwSessionId, false, false);
                        OlddwSessionId = dwSessionId;
                        preconnect_start = false;
                        goto whileloop;
                    }
                }

                if (preconnect_start == true)
                {
                    if (!IsSessionStillActive(OlddwSessionId))
                    {
                        ::SetEvent(hEventIn);
                    }
                }

                // Monitor process
                DWORD dwCode{ 0U };
                bool returnvalue = ::GetExitCodeProcess(ProcessInfo.hProcess, &dwCode);
                if (!returnvalue)
                {
                    //bad handle, thread already terminated
                    if (ProcessInfo.hProcess)
                    {
                        ::CloseHandle(ProcessInfo.hProcess);
                    }
                    if (ProcessInfo.hThread)
                    {
                        ::CloseHandle(ProcessInfo.hThread);
                    }
                    ProcessInfo.hProcess = nullptr;
                    ProcessInfo.hThread = nullptr;
                    RDPMODE = iniFileService.ReadInt("admin", "rdpmode", 0);
                    ::Sleep(1000);
                    goto whileloop;
                }

                if (dwCode == STILL_ACTIVE)
                {
                    goto whileloop;
                }
                if (ProcessInfo.hProcess)
                {
                    ::WaitForSingleObject(ProcessInfo.hProcess, 15000);
                }
                if (ProcessInfo.hProcess)
                {
                    ::CloseHandle(ProcessInfo.hProcess);
                }
                if (ProcessInfo.hThread)
                {
                    ::CloseHandle(ProcessInfo.hThread);
                }
                ProcessInfo.hProcess = nullptr;
                ProcessInfo.hThread = nullptr;
                RDPMODE = iniFileService.ReadInt("admin", "rdpmode", 0);
                ::Sleep(1000);
                goto whileloop;
            } //timeout
            else
            {
                dwSessionId = ::WTSGetActiveConsoleSessionId();
                if (OlddwSessionId != dwSessionId)
                {
                    ::SetEvent(hEventIn);
                }
                if (dwSessionId != 0xFFFFFFFF)
                {
                    DWORD dwCode{ 0U };
                    if (ProcessInfo.hProcess == nullptr)
                    {
                        //First RUNf
                        LaunchProcessWin(dwSessionId, false, false);
                        OlddwSessionId = dwSessionId;
                    }
                    else if (::GetExitCodeProcess(ProcessInfo.hProcess, &dwCode))
                    {
                        if (dwCode != STILL_ACTIVE)
                        {
                            ::WaitForSingleObject(ProcessInfo.hProcess, 15000);
                            if (ProcessInfo.hProcess)
                            {
                                ::CloseHandle(ProcessInfo.hProcess);
                            }
                            if (ProcessInfo.hThread)
                            {
                                ::CloseHandle(ProcessInfo.hThread);
                            }
                            ProcessInfo.hProcess = nullptr;
                            ProcessInfo.hThread = nullptr;
                            int sessidcounter{ 0 };
                            while ((dwSessionId == 0xFFFFFFFF) && !IsShutdown)
                            {
                                ::Sleep(1000);
                                dwSessionId = ::WTSGetActiveConsoleSessionId();
                                ++sessidcounter;
                                if (sessidcounter > 10)
                                {
                                    break;
                                }
                            }
                            RDPMODE = iniFileService.ReadInt("admin", "rdpmode", 0);
                            goto whileloop;
                        }
                    }
                    else
                    {
                        if (ProcessInfo.hProcess)
                        {
                            ::CloseHandle(ProcessInfo.hProcess);
                        }
                        if (ProcessInfo.hThread)
                        {
                            ::CloseHandle(ProcessInfo.hThread);
                        }
                        ProcessInfo.hProcess = nullptr;
                        ProcessInfo.hThread = nullptr;
                        int sessidcounter{ 0 };

                        while ((dwSessionId == 0xFFFFFFFF) && !IsShutdown)
                        {
                            ::Sleep(1000);
                            dwSessionId = ::WTSGetActiveConsoleSessionId();
                            ++sessidcounter;
                            if (sessidcounter > 10)
                            {
                                break;
                            }
                        }

                        RDPMODE = iniFileService.ReadInt("admin", "rdpmode", 0);
                        goto whileloop;
                    }
                }
            }


        } //switch

    whileloop:;
    } //while

    if (hEventIn)
    {
        ::SetEvent(hEventIn);
    }
    if (ProcessInfo.hProcess)
    {
        ::WaitForSingleObject(ProcessInfo.hProcess, 15000);
        if (ProcessInfo.hProcess)
        {
            ::CloseHandle(ProcessInfo.hProcess);
        }
        if (ProcessInfo.hThread)
        {
            ::CloseHandle(ProcessInfo.hThread);
        }
        ProcessInfo.hProcess = nullptr;
        ProcessInfo.hThread = nullptr;
    }
    if (hEventIn)
    {
        ::CloseHandle(hEventIn);
    }
    if (hEventcad)
    {
        ::CloseHandle(hEventcad);
    }
    if (hEventPreConnect)
    {
        ::CloseHandle(hEventPreConnect);
    }
    if (hEndSessionEvent)
    {
        ::CloseHandle(hEndSessionEvent);
    }
    if (data)
    {
        ::UnmapViewOfFile(data);
    }
    if (hMapFile != nullptr)
    {
        ::CloseHandle(hMapFile);
    }
}

BOOL UltraVNCService::LaunchProcessWin(const DWORD dwSessionId, const bool preconnect, const bool rdpselect)
{
    if (IsShutdown)
    {
        return false;
    }
    BOOL bReturn = FALSE;
    HANDLE hToken = nullptr;
    STARTUPINFO StartUPInfo;
    PVOID lpEnvironment = nullptr;
    static int counter{ 0 };

    ::ZeroMemory(&StartUPInfo, sizeof(STARTUPINFO));
    ::ZeroMemory(&ProcessInfo, sizeof(PROCESS_INFORMATION));
    StartUPInfo.wShowWindow = SW_SHOW;
    //StartUPInfo.lpDesktop = "Winsta0\\Winlogon";
    StartUPInfo.lpDesktop = "Winsta0\\Default";
    StartUPInfo.cb = sizeof(STARTUPINFO);
    SetTBCPrivileges();
    createWinvncExeCall(preconnect, rdpselect);

    if (GetSessionUserTokenWin(&hToken, dwSessionId))
    {
        if (::CreateEnvironmentBlock(&lpEnvironment, hToken, FALSE))
        {
            SetLastError(0);
            if (IsShutdown)
            {
                return false;
            }
            if (::CreateProcessAsUser(hToken, nullptr, app_path.data(), nullptr, nullptr, FALSE,
                                    CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS, lpEnvironment, nullptr, &StartUPInfo,
                                    &ProcessInfo))
            {
                counter = 0;
                bReturn = TRUE;
                [[maybe_unused]] DWORD error = ::GetLastError();
#ifdef _DEBUG
                std::array<char, 256U> szText;
                ::sprintf_s(szText.data(), szText.size(), " ++++++ CreateProcessAsUser winlogon %d\n", error);
                ::OutputDebugString(szText.data());
#endif
            }
            else
            {
                DWORD error = ::GetLastError();
#ifdef _DEBUG
                std::array<char, 256U> szText;
                ::sprintf_s(szText.data(), szText.size(), " ++++++ CreateProcessAsUser failed %d %d %d\n", error, kickrdp, counter);
                ::OutputDebugString(szText.data());
#endif
                if ((error == 233) && (kickrdp == 1))
                {
                    ++counter;
                    if (counter > 3)
                    {
                        typedef BOOLEAN(WINAPI * pWinStationConnect)(HANDLE, ULONG, ULONG, PCWSTR, ULONG);
                        HMODULE hlibwinsta = ::LoadLibraryEx("winsta.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                        pWinStationConnect WinStationConnectF = nullptr;
                        if (hlibwinsta)
                        {
                            WinStationConnectF = (pWinStationConnect)::GetProcAddress(hlibwinsta, "WinStationConnectW");
                        }
                        if (WinStationConnectF != nullptr)
                        {
                            DWORD ID = ::WTSGetActiveConsoleSessionId();
                            WinStationConnectF(0, 0, ID, L"", 0);
                            ::LockWorkStation();
                        }
                        ::Sleep(3000);
                    }
                }
                else if (error == 233)
                {
                    CreateRemoteSessionProcess(dwSessionId, true, hToken, nullptr, app_path.data(), nullptr, nullptr, FALSE,
                                               CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS, lpEnvironment, nullptr, &StartUPInfo,
                                               &ProcessInfo);
                    counter = 0;
                    bReturn = TRUE;
                }
            }

            if (lpEnvironment)
            {
                ::DestroyEnvironmentBlock(lpEnvironment);
            }
        } //createenv
        else
        {
            SetLastError(0);
            if (IsShutdown)
            {
                return false;
            }
            if (::CreateProcessAsUser(hToken, nullptr, app_path.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, nullptr,
                                    &StartUPInfo, &ProcessInfo))
            {
                counter = 0;
                bReturn = TRUE;
                [[maybe_unused]] DWORD error = ::GetLastError();
#ifdef _DEBUG
                std::array<char, 256U> szText;
                ::sprintf_s(szText.data(), szText.size(), " ++++++ CreateProcessAsUser winlogon %d\n", error);
                ::OutputDebugString(szText.data());
#endif
            }
            else
            {
                DWORD error = ::GetLastError();
#ifdef _DEBUG
                std::array<char, 256U> szText;
                ::sprintf_s(szText.data(), szText.size(), " ++++++ CreateProcessAsUser no env failed %d\n", error);
                ::OutputDebugString(szText.data());
#endif
                //Little trick needed, FUS sometimes has an unreachable logon session.
                //Switch to USER B, logout user B
                //The logon session is then unreachable
                //We force the logon session on the console
                if ((error == 233) && (kickrdp == 1))
                {
                    ++counter;
                    if (counter > 3)
                    {
                        typedef BOOLEAN(WINAPI * pWinStationConnect)(HANDLE, ULONG, ULONG, PCWSTR, ULONG);
                        HMODULE hlibwinsta = ::LoadLibraryEx("winsta.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                        pWinStationConnect WinStationConnectF = nullptr;
                        if (hlibwinsta)
                        {
                            WinStationConnectF = (pWinStationConnect)::GetProcAddress(hlibwinsta, "WinStationConnectW");
                        }

                        if (WinStationConnectF != nullptr)
                        {
                            DWORD ID = ::WTSGetActiveConsoleSessionId();
                            WinStationConnectF(0, 0, ID, L"", 0);
                            ::LockWorkStation();
                        }
                        ::Sleep(3000);
                    }
                }
                else if (error == 233)
                {
                    CreateRemoteSessionProcess(dwSessionId, true, hToken, nullptr, app_path.data(), nullptr, nullptr, FALSE,
                                               DETACHED_PROCESS, nullptr, nullptr, &StartUPInfo, &ProcessInfo);
                    counter = 0;
                    bReturn = TRUE;
                }
            }
        } //getsession
        ::CloseHandle(hToken);
    }
    return bReturn;
}

BOOL UltraVNCService::CreateRemoteSessionProcess(const IN DWORD dwSessionId, IN BOOL bUseDefaultToken, IN HANDLE hToken,
                                                 IN LPCWSTR lpApplicationName, const IN LPSTR A_lpCommandLine,
                                                 [[maybe_unused]] const IN LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                                 [[maybe_unused]] const IN LPSECURITY_ATTRIBUTES lpThreadAttributes, IN BOOL bInheritHandles,
                                                 IN DWORD dwCreationFlags, IN LPVOID lpEnvironment,
                                                 [[maybe_unused]] const IN LPCWSTR lpCurrentDirectory, [[maybe_unused]] const IN LPSTARTUPINFO A_lpStartupInfo,
                                                 OUT LPPROCESS_INFORMATION lpProcessInformation)
{
    if (IsShutdown)
    {
        return false;
    }
    std::array<WCHAR, 255U> lpCommandLine;
    STARTUPINFOW StartupInfo;
    Char2Wchar(lpCommandLine.data(), A_lpCommandLine, 255);
    ::ZeroMemory(&StartupInfo, sizeof(STARTUPINFOW));
    StartupInfo.wShowWindow = SW_SHOW;
    StartupInfo.lpDesktop = L"Winsta0\\Winlogon";
    StartupInfo.cb = sizeof(STARTUPINFOW);

    std::array<WCHAR, MAX_PATH> szWinStaPath;
    BOOL bGetNPName = FALSE;
    std::array<WCHAR, MAX_PATH> szNamedPipeName{};
    DWORD dwNameLen;
    HINSTANCE hInstWinSta;
    HANDLE hNamedPipe;
    [[maybe_unused]] LPVOID pData = nullptr;
    BOOL bRet = FALSE;
    DWORD cbReadBytes, cbWriteBytes;
    DWORD dwEnvLen{ 0U };
    union
    {
        CPAU_PARAM cpauData;
        BYTE bDump[0x2000U];
    };
    CPAU_RET_PARAM cpauRetData;
    DWORD dwUsedBytes = sizeof(cpauData);
    LPBYTE pBuffer = (LPBYTE)(&cpauData + 1);
    ::GetSystemDirectoryW(szWinStaPath.data(), static_cast<UINT>(szWinStaPath.size()));
    lstrcatW(szWinStaPath.data(), L"\\winsta.dll");
    hInstWinSta = ::LoadLibraryW(szWinStaPath.data());

    if (hInstWinSta)
    {
        pWinStationQueryInformationW pfWinStationQueryInformationW =
            (pWinStationQueryInformationW)::GetProcAddress(hInstWinSta, "WinStationQueryInformationW");
        if (pfWinStationQueryInformationW)
        {
            bGetNPName = pfWinStationQueryInformationW(0, dwSessionId, 0x21, szNamedPipeName.data(), sizeof(szNamedPipeName), &dwNameLen);
        }
        ::FreeLibrary(hInstWinSta);
    }
    if (!bGetNPName || (szNamedPipeName[0U] == '\0'))
    {
        std::swprintf(szNamedPipeName.data(), 260, L"\\\\.\\Pipe\\TerminalServer\\SystemExecSrvr\\%d", dwSessionId);
    }

    do
    {
        hNamedPipe = ::CreateFileW(szNamedPipeName.data(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0);
        if (hNamedPipe == INVALID_HANDLE_VALUE)
        {
            if (::GetLastError() == ERROR_PIPE_BUSY)
            {
                if (!::WaitNamedPipeW(szNamedPipeName.data(), 30000))
                {
                    return FALSE;
                }
            }
            else
            {
                return FALSE;
            }
        }
    } while (hNamedPipe == INVALID_HANDLE_VALUE);

    std::memset(&cpauData, 0, sizeof(cpauData));
    cpauData.bInheritHandles = bInheritHandles;
    cpauData.bUseDefaultToken = bUseDefaultToken;
    cpauData.dwCreationFlags = dwCreationFlags;
    cpauData.dwProcessId = ::GetCurrentProcessId();
    cpauData.hToken = hToken;
    cpauData.lpApplicationName = (LPWSTR)MarshallString(lpApplicationName, &cpauData, sizeof(bDump), &pBuffer, &dwUsedBytes);
    cpauData.lpCommandLine = (LPWSTR)MarshallString(lpCommandLine.data(), &cpauData, sizeof(bDump), &pBuffer, &dwUsedBytes);
    cpauData.StartupInfo = StartupInfo;
    cpauData.StartupInfo.lpDesktop =
        (LPWSTR)MarshallString(cpauData.StartupInfo.lpDesktop, &cpauData, sizeof(bDump), &pBuffer, &dwUsedBytes);
    cpauData.StartupInfo.lpTitle =
        (LPWSTR)MarshallString(cpauData.StartupInfo.lpTitle, &cpauData, sizeof(bDump), &pBuffer, &dwUsedBytes);

    if (lpEnvironment)
    {
        if (dwCreationFlags & CREATE_UNICODE_ENVIRONMENT)
        {
            while ((dwEnvLen + dwUsedBytes <= sizeof(bDump)))
            {
                if ((((LPWSTR)lpEnvironment)[dwEnvLen / 2] == '\0') && (((LPWSTR)lpEnvironment)[dwEnvLen / 2 + 1] == '\0'))
                {
                    dwEnvLen += 2 * sizeof(WCHAR);
                    break;
                }
                dwEnvLen += sizeof(WCHAR);
            }
        }
        else
        {
            while (dwEnvLen + dwUsedBytes <= sizeof(bDump))
            {
                if ((((LPSTR)lpEnvironment)[dwEnvLen] == '\0') && (((LPSTR)lpEnvironment)[dwEnvLen + 1] == '\0'))
                {
                    dwEnvLen += 2;
                    break;
                }
                ++dwEnvLen;
            }
        }
        if (dwEnvLen + dwUsedBytes <= sizeof(bDump))
        {
            std::memmove(pBuffer, lpEnvironment, dwEnvLen);
            cpauData.lpEnvironment = (LPVOID)dwUsedBytes;
            pBuffer += dwEnvLen;
            dwUsedBytes += dwEnvLen;
        }
        else
        {
            cpauData.lpEnvironment = nullptr;
        }
    }
    else
    {
        cpauData.lpEnvironment = nullptr;
    }
    cpauData.cbSize = dwUsedBytes;

    HANDLE hProcess = nullptr;
    if (::WriteFile(hNamedPipe, &cpauData, cpauData.cbSize, &cbWriteBytes, nullptr))
    {
        ::Sleep(250);
        if (::ReadFile(hNamedPipe, &cpauRetData, sizeof(cpauRetData), &cbReadBytes, nullptr))
        {
            hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, cpauRetData.ProcInfo.dwProcessId);
#ifdef _DEBUG
            std::array<char, 256U> szText;
            ::sprintf_s(szText.data(), szText.size(), " ++++++cpau  %i  %p %i %i %p %p\n", cpauRetData.bRetValue, cpauRetData.ProcInfo.hProcess, cpauRetData.ProcInfo.dwProcessId, cpauRetData.ProcInfo.dwThreadId, cpauRetData.ProcInfo.hThread, hProcess);
            ::OutputDebugString(szText.data());
#endif
            bRet = cpauRetData.bRetValue;
            if (bRet)
            {
                *lpProcessInformation = cpauRetData.ProcInfo;
            }
            else
            {
                SetLastError(cpauRetData.dwLastErr);
            }
        }
    }
    else
    {
        bRet = FALSE;
    }
    // function sometimes fail, the use processid to get hprocess... bug MS
    if (lpProcessInformation->hProcess == 0)
    {
        lpProcessInformation->hProcess = hProcess;
    }
    //this should never happen, looping connections
    if (lpProcessInformation->hProcess == 0)
    {
        ::Sleep(5000);
    }
    ::CloseHandle(hNamedPipe);
    return bRet;
}

DWORD UltraVNCService::MarshallString(const LPCWSTR pszText, const LPVOID, const DWORD dwMaxSize, LPBYTE* ppNextBuf,
                                      DWORD* pdwUsedBytes)
{
    const DWORD dwOffset = *pdwUsedBytes;
    if (!pszText)
    {
        return 0;
    }
    DWORD dwLen = static_cast<DWORD>((std::wcslen(pszText) + 1)) * sizeof(WCHAR);
    if (*pdwUsedBytes + dwLen > dwMaxSize)
    {
        return 0;
    }
    std::memmove(*ppNextBuf, pszText, dwLen);
    *pdwUsedBytes += dwLen;
    *ppNextBuf += dwLen;
    return dwOffset;
}

BOOL UltraVNCService::Char2Wchar(WCHAR* const pDest, char* const pSrc, int nDestStrLen)
{
    int nSrcStrLen{ 0 };
    int nOutputBuffLen{ 0 };
    int retcode{ 0 };

    if ((pDest == nullptr) || (pSrc == nullptr))
    {
        return FALSE;
    }

    nSrcStrLen = static_cast<int>(std::strlen(pSrc));
    if (nSrcStrLen == 0)
    {
        return FALSE;
    }

    nDestStrLen = nSrcStrLen;

    if (nDestStrLen > MAXSTRLENGTH - 1)
    {
        return FALSE;
    }
    std::memset(pDest, 0, sizeof(TCHAR) * nDestStrLen);
    nOutputBuffLen = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pSrc, nSrcStrLen, pDest, nDestStrLen);

    if (nOutputBuffLen == 0)
    {
        retcode = ::GetLastError();
        return FALSE;
    }

    pDest[nOutputBuffLen] = '\0';
    return TRUE;
}

BOOL UltraVNCService::get_winlogon_handle(const OUT LPHANDLE lphUserToken, const DWORD mysessionID)
{
    BOOL bResult = FALSE;
    HANDLE hProcess;
    [[maybe_unused]] HANDLE hAccessToken = nullptr;
    HANDLE hTokenThis = nullptr;
    DWORD ID_session{ 0U };
    ID_session = mysessionID;
    DWORD Id{ 0U };
    Id = Find_winlogon(ID_session);
    if (Id == -1)
    {
        Id = GetwinlogonPid();
    }

    hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, Id);
    if (hProcess)
    {
        ::OpenProcessToken(hProcess, TOKEN_ASSIGN_PRIMARY | TOKEN_ALL_ACCESS, &hTokenThis);
        bResult = ::DuplicateTokenEx(hTokenThis, TOKEN_ASSIGN_PRIMARY | TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation,
                                   TokenPrimary, lphUserToken);
        ::SetTokenInformation(*lphUserToken, TokenSessionId, &ID_session, sizeof(DWORD));
        ::CloseHandle(hTokenThis);
        ::CloseHandle(hProcess);
    }
    return bResult;
}

BOOL UltraVNCService::GetSessionUserTokenWin(const OUT LPHANDLE lphUserToken, const DWORD mysessionID)
{
    BOOL bResult = FALSE;
    if (lphUserToken != nullptr)
    {
        bResult = get_winlogon_handle(lphUserToken, mysessionID);
    }
    return bResult;
}

DWORD UltraVNCService::GetwinlogonPid()
{
    DWORD dwExplorerLogonPid{ 0U };
    PROCESSENTRY32 procEntry;
    HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    procEntry.dwSize = sizeof(PROCESSENTRY32);
    if (!::Process32First(hSnap, &procEntry))
    {
        ::CloseHandle(hSnap);
        return 0;
    }

    do
    {
        if (::_stricmp(procEntry.szExeFile, "winlogon.exe") == 0)
        {
            dwExplorerLogonPid = procEntry.th32ProcessID;
        }

    } while (!IsShutdown && Process32Next(hSnap, &procEntry));
    ::CloseHandle(hSnap);
    return dwExplorerLogonPid;
}

DWORD UltraVNCService::Find_winlogon(const DWORD SessionId)
{
    PWTS_PROCESS_INFO pProcessInfo = nullptr;
    DWORD ProcessCount{ 0U };
    DWORD Id = static_cast<DWORD>(-1);

    typedef BOOL(WINAPI * pfnWTSEnumerateProcesses)(HANDLE, DWORD, DWORD, PWTS_PROCESS_INFO*, DWORD*);
    typedef VOID(WINAPI * pfnWTSFreeMemory)(PVOID);

    helper::DynamicFn<pfnWTSEnumerateProcesses> pWTSEnumerateProcesses("wtsapi32", "WTSEnumerateProcessesA");
    helper::DynamicFn<pfnWTSFreeMemory> pWTSFreeMemory("wtsapi32", "WTSFreeMemory");

    if (pWTSEnumerateProcesses.isValid() && pWTSFreeMemory.isValid())
    {
        if ((*pWTSEnumerateProcesses)(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pProcessInfo, &ProcessCount))
        {
            // dump each process description
            for (DWORD CurrentProcess{ 0U }; CurrentProcess < ProcessCount; ++CurrentProcess)
            {
                if (::_stricmp(pProcessInfo[CurrentProcess].pProcessName, "winlogon.exe") == 0)
                {
                    if (SessionId == pProcessInfo[CurrentProcess].SessionId)
                    {
                        Id = pProcessInfo[CurrentProcess].ProcessId;
                        break;
                    }
                }
            }
            (*pWTSFreeMemory)(pProcessInfo);
        }
    }
    return Id;
}


BOOL UltraVNCService::SetTBCPrivileges(VOID)
{
    DWORD dwPID;
    HANDLE hProcess;
    HANDLE hToken;
    LUID Luid;
    TOKEN_PRIVILEGES tpDebug;
    dwPID = ::GetCurrentProcessId();
    if ((hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPID)) == nullptr)
    {
        return FALSE;
    }
    if (::OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &hToken) == 0)
    {
        ::CloseHandle(hProcess);
        return FALSE;
    }
    if ((::LookupPrivilegeValue(nullptr, SE_TCB_NAME, &Luid)) == 0)
    {
        ::CloseHandle(hToken);
        ::CloseHandle(hProcess);
        return FALSE;
    }
    tpDebug.PrivilegeCount = 1;
    tpDebug.Privileges[0U].Luid = Luid;
    tpDebug.Privileges[0U].Attributes = SE_PRIVILEGE_ENABLED;
    if ((::AdjustTokenPrivileges(hToken, FALSE, &tpDebug, sizeof(tpDebug), nullptr, nullptr)) == 0)
    {
        ::CloseHandle(hToken);
        ::CloseHandle(hProcess);
        return FALSE;
    }
    if (::GetLastError() != ERROR_SUCCESS)
    {
        ::CloseHandle(hToken);
        ::CloseHandle(hProcess);
        return FALSE;
    }
    ::CloseHandle(hToken);
    ::CloseHandle(hProcess);
    return TRUE;
}

void UltraVNCService::wait_for_existing_process()
{
    while (!IsShutdown && ((hEvent = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, "Global\\SessionEventUltra")) != nullptr))
    {
        ::SetEvent(hEvent); // signal tray icon to shut down
        ::CloseHandle(hEvent);
        ::Sleep(1000);
    }
}

bool UltraVNCService::IsSessionStillActive(const int ID)
{
    typedef BOOL(WINAPI * pfnWTSEnumerateSessions)(HANDLE, DWORD, DWORD, PWTS_SESSION_INFO*, DWORD*);
    ;
    typedef VOID(WINAPI * pfnWTSFreeMemory)(PVOID);

    helper::DynamicFn<pfnWTSEnumerateSessions> pWTSEnumerateSessions("wtsapi32", "WTSEnumerateSessionsA");
    helper::DynamicFn<pfnWTSFreeMemory> pWTSFreeMemory("wtsapi32", "WTSFreeMemory");
    if (pWTSEnumerateSessions.isValid() && pWTSFreeMemory.isValid())
    {
        WTS_SESSION_INFO* pSessions = 0;
        DWORD nSessions(0U);
        DWORD rdpSessionExists = false;
        if ((*pWTSEnumerateSessions)(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &nSessions))
        {
            for (DWORD i(0U); (i < nSessions) && !rdpSessionExists; ++i)
            {
                //exclude console session
                if ((::_stricmp(pSessions[i].pWinStationName, "Console") == 0) && (pSessions[i].SessionId == ID))
                {
                    rdpSessionExists = true;
                }
                else if ((pSessions[i].SessionId == ID) &&
                         ((pSessions[i].State == WTSActive) || (pSessions[i].State == WTSShadow) ||
                          (pSessions[i].State == WTSConnectQuery)))
                {
                    rdpSessionExists = true;
                }
            }
            (*pWTSFreeMemory)(pSessions);
        }
        return rdpSessionExists ? true : false;
    }
    return false;
}
#endif
