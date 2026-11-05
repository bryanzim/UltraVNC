// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// /macine-vnc Greg Wood (wood@agressiv.com)


#include "logging.h"

#include <array>
#include <cstdio>
#include <cstring>
/////////////////////////

BOOL APIENTRY DllMain(const HANDLE hModule, const DWORD ul_reason_for_call, const LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////

EventLogging::EventLogging()
{
    // returns a handle that links the source to the registry
    m_hEventLinker = ::RegisterEventSource(nullptr, "UltraVNC");
}

EventLogging::~EventLogging()
{
    // Releases the handle to the registry
    ::DeregisterEventSource(m_hEventLinker);
}



void EventLogging::LogIt(const WORD CategoryID, const DWORD EventID, LPCTSTR* const ArrayOfStrings, const UINT NumOfArrayStr,
                         const LPVOID RawData, const DWORD RawDataSize)
{

    // Writes data to the event log
    ::ReportEvent(m_hEventLinker, EVENTLOG_INFORMATION_TYPE, CategoryID, EventID, nullptr, 1, RawDataSize, ArrayOfStrings, RawData);
}


void EventLogging::AddEventSourceToRegistry(const LPCTSTR lpszSourceName)
{
    HKEY hk;
    DWORD dwData;
    std::array<TCHAR, MAX_PATH> szBuf;
    std::array<TCHAR, 255U> szKey{_T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\")};
    std::array<TCHAR, MAX_PATH> szServicePath;

    ::lstrcat(szKey.data(), _T("UltraVNC"));

    if (::RegCreateKey(HKEY_LOCAL_MACHINE, szKey.data(), &hk) != ERROR_SUCCESS)
    {
        return;
    }

    if (::GetModuleFileName(nullptr, szServicePath.data(), szServicePath.size()))
    {
        char* p = std::strrchr(szServicePath.data(), '\\');
        if (p == nullptr)
        {
            return;
        }
        *p = '\0';
        ::strcat_s(szServicePath.data(), szServicePath.size(), "\\logmessages.dll");
    }
    //printf(szServicePath);
    ::lstrcpy(szBuf.data(), szServicePath.data());

    // Add the name to the EventMessageFile subkey.
    if (::RegSetValueEx(hk, _T("EventMessageFile"), 0, REG_EXPAND_SZ, (LPBYTE)szBuf.data(),
                      (::lstrlen(szBuf.data()) + 1) * sizeof(TCHAR)) != ERROR_SUCCESS)
    {
        ::RegCloseKey(hk);
        return;
    }

    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    if (::RegSetValueEx(hk, _T("TypesSupported"), 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD)) != ERROR_SUCCESS) {}
    ::RegCloseKey(hk);
}



/////////////////////////
///////////////////////

LOGGING_API
void LOGEXIT(char* const machine, char* const user, const int clientId, const bool isinteractive)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::array<char, 64U> mode_id;
    std::snprintf(mode_id.data(), mode_id.size(), " (%s) (ID:%d)", isinteractive ? "Interactive" : "ViewOnly", clientId);
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Client ");
    std::strcat(texttowrite.data(), machine);
    if (user != nullptr)
    {
        std::strcat(texttowrite.data(), " using ");
        std::strcat(texttowrite.data(), user);
        std::strcat(texttowrite.data(), " account");
    }
    std::strcat(texttowrite.data(), " disconnected");
    std::strcat(texttowrite.data(), mode_id.data());
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640003L, ps.data(), 1, nullptr, 0);

    std::array<char, MAX_PATH> szMslogonLog;
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), szMslogonLog.size()))
    {
        char* p = std::strrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::strcat_s(szMslogonLog.data(), szMslogonLog.size(), "\\mslogon.log");
        }
    }
    file = std::fopen(szMslogonLog.data(), "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGEXITV2(char* const machine, char* const user, const int clientId, const bool isinteractive, char* const szMslogonLog)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::array<char, 64U> mode_id;
    std::snprintf(mode_id.data(), mode_id.size(), " (%s) (ID:%d)", isinteractive ? "Interactive" : "ViewOnly", clientId);
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Client ");
    std::strcat(texttowrite.data(), machine);
    if (user != nullptr)
    {
        std::strcat(texttowrite.data(), " using ");
        std::strcat(texttowrite.data(), user);
        std::strcat(texttowrite.data(), " account");
    }
    std::strcat(texttowrite.data(), " disconnected");
    std::strcat(texttowrite.data(), mode_id.data());
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640003L, ps.data(), 1, nullptr, 0);

    file = std::fopen(szMslogonLog, "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGLOGON(char* const machine, const int clientId, const bool isinteractive)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::array<char, 64U> mode_id;
    std::snprintf(mode_id.data(), mode_id.size(), " (%s) (ID:%d)", isinteractive ? "Interactive" : "ViewOnly", clientId);
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Password authentication accepted from ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), mode_id.data());
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640001L, ps.data(), 1, nullptr, 0);

    std::array<char, MAX_PATH> szMslogonLog;
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), szMslogonLog.size()))
    {
        char* p = std::strrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::strcat_s(szMslogonLog.data(), szMslogonLog.size(), "\\mslogon.log");
        }
    }
    file = std::fopen(szMslogonLog.data(), "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGLOGONV2(char* const machine, const int clientId, const bool isinteractive, char* const szMslogonLog)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::array<char, 64U> mode_id;
    std::snprintf(mode_id.data(), mode_id.size(), " (%s) (ID:%d)", isinteractive ? "Interactive" : "ViewOnly", clientId);
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Password authentication accepted from ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), mode_id.data());
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640001L, ps.data(), 1, nullptr, 0);

    file = std::fopen(szMslogonLog, "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

void LOGCONN(char* const machine, const int clientId, const bool isinteractive)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Connection received from ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640001L, ps.data(), 1, nullptr, 0);

    std::array<char, MAX_PATH> szMslogonLog;
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), szMslogonLog.size()))
    {
        char* p = std::strrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::strcat_s(szMslogonLog.data(), szMslogonLog.size(), "\\mslogon.log");
        }
    }
    file = std::fopen(szMslogonLog.data(), "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

void LOGCONNV2(char* const machine, const int clientId, const bool isinteractive, char* const szMslogonLog)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Connection received from ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640001L, ps.data(), 1, nullptr, 0);

    file = std::fopen(szMslogonLog, "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGFAILED(char* const machine)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Invalid attempt from client ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640002L, ps.data(), 1, nullptr, 0);

    std::array<char, MAX_PATH> szMslogonLog;
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), szMslogonLog.size()))
    {
        char* p = std::strrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::strcat_s(szMslogonLog.data(), szMslogonLog.size(), "\\mslogon.log");
        }
    }
    file = std::fopen(szMslogonLog.data(), "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}


LOGGING_API
void LOGFAILEDV2(char* const machine, char* const szMslogonLog)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Invalid attempt from client ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640002L, ps.data(), 1, nullptr, 0);

    file = std::fopen(szMslogonLog, "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGLOGONUSER(char* const machine, char* const user)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Authentication accepted from ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), " using ");
    std::strcat(texttowrite.data(), user);
    std::strcat(texttowrite.data(), " account ");
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640001L, ps.data(), 1, nullptr, 0);

    std::array<char, MAX_PATH> szMslogonLog;
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), szMslogonLog.size()))
    {
        char* p = std::strrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::strcat_s(szMslogonLog.data(), szMslogonLog.size(), "\\mslogon.log");
        }
    }
    file = std::fopen(szMslogonLog.data(), "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGLOGONUSERV2(char* const machine, char* const user, char* const szMslogonLog)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Authentication accepted from ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), " using ");
    std::strcat(texttowrite.data(), user);
    std::strcat(texttowrite.data(), " account ");
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640001L, ps.data(), 1, nullptr, 0);

    file = std::fopen(szMslogonLog, "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGFAILEDUSER(char* const machine, char* const user)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Connection rejected from client ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), " using ");
    std::strcat(texttowrite.data(), user);
    std::strcat(texttowrite.data(), " account ");
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640002L, ps.data(), 1, nullptr, 0);

    std::array<char, MAX_PATH> szMslogonLog;
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), szMslogonLog.size()))
    {
        char* p = std::strrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::strcat_s(szMslogonLog.data(), szMslogonLog.size(), "\\mslogon.log");
        }
    }
    file = std::fopen(szMslogonLog.data(), "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGFAILEDUSERV2(char* const machine, char* const user, char* const szMslogonLog)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Connection rejected from client ");
    std::strcat(texttowrite.data(), machine);
    std::strcat(texttowrite.data(), " using ");
    std::strcat(texttowrite.data(), user);
    std::strcat(texttowrite.data(), " account ");
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640002L, ps.data(), 1, nullptr, 0);

    file = std::fopen(szMslogonLog, "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGEXTRAINFO(char* const info)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Viewer message: ");
    std::strcat(texttowrite.data(), info);
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640002L, ps.data(), 1, nullptr, 0);

    std::array<char, MAX_PATH> szMslogonLog;
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), szMslogonLog.size()))
    {
        char* p = std::strrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::strcat_s(szMslogonLog.data(), szMslogonLog.size(), "\\mslogon.log");
        }
    }
    file = std::fopen(szMslogonLog.data(), "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

LOGGING_API
void LOGEXTRAINFOV2(char* const info, char* const szMslogonLog)
{
    FILE* file;
    std::array<const char*, 3U> ps;
    std::array<char, 512U> texttowrite;
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    std::array<char, 256U> szText;
    std::snprintf(szText.data(), szText.size(), "%02d/%02d/%d %02d:%02d:%02d\t", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute,
            time.wSecond);
    std::strcpy(texttowrite.data(), szText.data());
    std::strcat(texttowrite.data(), "Viewer message: ");
    std::strcat(texttowrite.data(), info);
    std::strcat(texttowrite.data(), "\n");
    ps[0U] = texttowrite.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, 0x00640002L, ps.data(), 1, nullptr, 0);

    file = std::fopen(szMslogonLog, "a");
    if (file != nullptr)
    {
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}