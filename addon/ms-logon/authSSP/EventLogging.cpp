// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2004 Martin Scharpf. All Rights Reserved.
//


#include "EventLogging.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

EventLogging::EventLogging()
{
    // returns a handle that links the source to the registry
    m_hEventLinker = ::RegisterEventSource(nullptr, _T("UltraVNC"));
}

EventLogging::~EventLogging()
{
    // Releases the handle to the registry
    ::DeregisterEventSource(m_hEventLinker);
}



void EventLogging::LogIt(const WORD CategoryID, const DWORD EventID, LPCTSTR* const ArrayOfStrings, [[maybe_unused]] const UINT NumOfArrayStr,
                         const LPVOID RawData, const DWORD RawDataSize)
{

    // Writes data to the event log
    ::ReportEvent(m_hEventLinker, EVENTLOG_INFORMATION_TYPE, CategoryID, EventID, nullptr, 1, RawDataSize, ArrayOfStrings, RawData);
}


void EventLogging::AddEventSourceToRegistry([[maybe_unused]] const LPCTSTR lpszSourceName)
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

    if (::GetModuleFileName(nullptr, szServicePath.data(), static_cast<DWORD>(szServicePath.size())))
    {
        TCHAR* p = ::_tcsrchr(szServicePath.data(), '\\');
        if (p == nullptr)
        {
            return;
        }
        *p = '\0';
        ::_tcscat_s(szServicePath.data(), szServicePath.size(), _T("\\logmessages.dll"));
    }
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

void LOGV2(TCHAR* const szMslogonLog, const long EventID, const TCHAR* const format, ...)
{
    FILE* file;
    std::array<LPCTSTR, 3U> ps;
    std::array<TCHAR, 2 * MAXLEN> textbuf{};
    std::array<char, 2 * MAXLEN> texttowrite{};
    std::array<TCHAR, MAXLEN> szTimestamp{};
    std::array<TCHAR, MAXLEN> szText{};
    SYSTEMTIME time;

    va_list ap;
    va_start(ap, format);
    _vstprintf_s(szText.data(), szText.size(), format, ap);
    va_end(ap);
    // Prepend timestamp to message
    ::GetLocalTime(&time);
    ::_stprintf_s(szTimestamp.data(), szTimestamp.size(), _T("%.2d/%.2d/%d %.2d:%.2d:%.2d\t"), time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
    ::_tcscpy_s(textbuf.data(), textbuf.size(), szTimestamp.data());
    ::_tcscat_s(textbuf.data(), textbuf.size(), szText.data());
    ps[0U] = textbuf.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, EventID, ps.data(), 1, nullptr, 0);
    file = _tfopen(szMslogonLog, _T("a"));
    if (file != nullptr)
    {

        // Write ANSI
#if defined UNICODE || defined _UNICODE
        std::size_t pnconv;
        ::wcstombs_s(&pnconv, texttowrite.data(), 512, textbuf.data(), textbuf.size());
#else
        std::strcpy(texttowrite.data(), texttowrite.data());
#endif
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}

void LOG(const long EventID, const TCHAR* const format, ...)
{
    FILE* file;
    std::array<TCHAR, MAX_PATH> szMslogonLog;
    std::array<LPCTSTR, 3U> ps;
    std::array<TCHAR, 2 * MAXLEN> textbuf{};
    std::array<char, 2 * MAXLEN> texttowrite{};
    std::array<TCHAR, MAXLEN> szTimestamp{};
    std::array<TCHAR, MAXLEN> szText{};
    SYSTEMTIME time;

    va_list ap;
    va_start(ap, format);
    _vstprintf_s(szText.data(), szText.size(), format, ap);
    va_end(ap);
    // Prepend timestamp to message
    ::GetLocalTime(&time);
    ::_stprintf_s(szTimestamp.data(), szTimestamp.size(), _T("%.2d/%.2d/%d %.2d:%.2d:%.2d\t"), time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
    ::_tcscpy_s(textbuf.data(), textbuf.size(), szTimestamp.data());
    ::_tcscat_s(textbuf.data(), textbuf.size(), szText.data());
    ps[0U] = textbuf.data();
    EventLogging log;
    log.AddEventSourceToRegistry(nullptr);
    log.LogIt(1, EventID, ps.data(), 1, nullptr, 0);
    if (::GetModuleFileName(nullptr, szMslogonLog.data(), static_cast<DWORD>(szMslogonLog.size())))
    {
        TCHAR* p = ::_tcsrchr(szMslogonLog.data(), '\\');
        if (p != nullptr)
        {
            *p = '\0';
            ::_tcscat_s(szMslogonLog.data(), szMslogonLog.size(), _T("\\mslogon.log"));
        }
    }
    file = _tfopen(szMslogonLog.data(), _T("a"));
    if (file != nullptr)
    {

        // Write ANSI
#if defined UNICODE || defined _UNICODE
        std::size_t pnconv;
        ::wcstombs_s(&pnconv, texttowrite.data(), 512, textbuf.data(), textbuf.size());
#else
        std::strcpy(texttowrite.data(), texttowrite.data());
#endif
        std::fwrite(texttowrite.data(), sizeof(char), std::strlen(texttowrite.data()), file);
        std::fclose(file);
    }
}