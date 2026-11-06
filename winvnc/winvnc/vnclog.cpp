// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
//


// Log.cpp: implementation of the VNCLog class.
//
//////////////////////////////////////////////////////////////////////

#include "stdhdrs.h"

#include "vnclog.h"

#include "common/inifile.h"
#include "PropertiesDialog.h"
#include "winvnc.h"

#include <io.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <ctime>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

const int VNCLog::ToDebug = 1;
const int VNCLog::ToFile = 2;
const int VNCLog::ToConsole = 4;

static const int LINE_BUFFER_SIZE{ 1024 };
#include "SettingsManager.h"

#include <cstdio>

char* removeNewlineAndCopy(const char* str)
{
    const std::size_t len = std::strlen(str); // Get the length of the string
    const bool hasNewline = ((len > 0) && (str[len - 1] == '\n'));
    std::size_t newLen = hasNewline ? len - 1 : len; // Adjust length if there's a newline

    // Allocate a new char array for the result
    char* result = new char[newLen + 1]; // +1 for null terminator
    std::strncpy(result, str, newLen);   // Copy up to newLen characters
    result[newLen] = '\0';               // Add null terminator

    return result; // Return the new char array
}


void VNCLog::Print(const int level, const char* const format, ...)
{
    if ((level == -1) || (settings && settings->getShowAllLogs()))
    {
        va_list ap;
        va_start(ap, format);
        char* cleaned = removeNewlineAndCopy(format);
        ReallyPrintScreen(cleaned, ap);
        delete[] cleaned;
        va_end(ap);
        return;
    }
    if (level > m_level)
    {
        return;
    }
    if (!m_todebug && !m_toconsole && !m_tofile)
    {
        return;
    }
    va_list ap;
    va_start(ap, format);
    ReallyPrint(format, ap);
    va_end(ap);
}

VNCLog::VNCLog()
    : m_tofile{ false }, m_todebug{ false }, m_toconsole{ false }, m_mode{ 0 }, m_level{ 0 }, hlogfile{ nullptr },
      m_append{ false }, m_video{ false }, m_lastLogTime{ 0 }
{
    ::strcpy_s(m_filename.data(), m_filename.size(), "");
    m_path[0U] = 0;
}

void VNCLog::SetMode(int mode)
{
#ifdef SC_20
    return;
#endif // SC_20
    m_mode = mode;
    if (mode & ToDebug)
    {
        m_todebug = true;
    }
    else
    {
        m_todebug = false;
    }

    if (mode & ToFile)
    {
        if (!m_tofile)
        {
            OpenFile();
        }
    }
    else
    {
        CloseFile();
        m_tofile = false;
    }

    if (mode & ToConsole)
    {
        if (!m_toconsole)
        {
            ::AllocConsole(); //lint !e534
            std::fclose(stdout);
            std::fclose(stderr);
#ifdef _MSC_VER3
            int fh = _open_osfhandle(static_cast<long>(::GetStdHandle(STD_OUTPUT_HANDLE)), 0);
            _dup2(fh, 1);
            _dup2(fh, 2);
            _fdopen(1, "wt");
            _fdopen(2, "wt");
            std::printf("fh is %i\n", fh);
            std::fflush(stdout);
#endif
        }

        m_toconsole = true;
    }
    else
    {
        m_toconsole = false;
    }
}


void VNCLog::SetLevel(int level) { m_level = level; }

void VNCLog::SetFile()
{
#ifdef SC_20
    return;
#endif // SC_20
    ::strcpy_s(m_filename.data(), m_filename.size(), m_path.data());
    ::strcat_s(m_filename.data(), m_filename.size(), "\\");
    ::strcat_s(m_filename.data(), m_filename.size(), "WinVNC.log");
    m_append = true;
    if (m_tofile || (m_mode & ToFile))
    {
        OpenFile();
    }
}

void VNCLog::OpenFile()
{
#ifdef SC_20
    return;
#endif // SC_20
    // Is there a file-name?
    if (std::strlen(m_filename.data()) == 0)
    {
        m_todebug = true;
        m_tofile = false;
        Print(0, "Error opening log file\n");
        return;
    }

    m_tofile = true;

    // If there's an existing log and we're not appending then move it
    if (!m_append)
    {
        // Build the backup filename
        char* backupfilename = new char[std::strlen(m_filename.data()) + 5];
        if (backupfilename)
        {
            ::strcpy_s(backupfilename, std::strlen(m_filename.data()) + 5, m_filename.data());
            ::strcat_s(backupfilename, std::strlen(m_filename.data()) + 5, ".bak");
            // Attempt the move and replace any existing backup
            // Note that failure is silent - where would we log a message to? ;)
            ::MoveFileEx(m_filename.data(), backupfilename, MOVEFILE_REPLACE_EXISTING);
            delete[] backupfilename;
        }
    }

    CloseFile();

    // If filename is NULL or invalid we should throw an exception here
    hlogfile = ::CreateFile(m_filename.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hlogfile == INVALID_HANDLE_VALUE)
    {
        // We should throw an exception here
        m_todebug = true;
        m_tofile = false;
        Print(0, "Error opening log file %s\n", m_filename.data());
    }
    if (m_append)
    {
        ::SetFilePointer(hlogfile, 0, nullptr, FILE_END);
    }
    else
    {
        ::SetEndOfFile(hlogfile);
    }
}

// if a log file is open, close it now.
void VNCLog::CloseFile()
{
#ifdef SC_20
    return;
#endif // SC_20
    if (hlogfile != nullptr)
    {
        ::CloseHandle(hlogfile);
        hlogfile = nullptr;
    }
}

inline void VNCLog::ReallyPrintLine(const char* const line)
{
#ifdef SC_20
    return;
#endif // SC_20
    if (m_todebug)
    {
        ::OutputDebugString(line);
    }
    if (m_toconsole)
    {
        DWORD byteswritten;
        ::WriteConsole(::GetStdHandle(STD_OUTPUT_HANDLE), line, static_cast<DWORD>(std::strlen(line)), &byteswritten, nullptr);
    };
    if (m_tofile && (hlogfile != nullptr))
    {
        DWORD byteswritten;
        ::WriteFile(hlogfile, line, static_cast<DWORD>(std::strlen(line)), &byteswritten, nullptr);
    }
}

void VNCLog::ReallyPrint(const char* const format, const va_list ap)
{
#ifdef SC_20
    return;
#endif // SC_20
    time_t current = std::time(0);
    if (current != m_lastLogTime)
    {
        m_lastLogTime = current;
        std::array<char, 20U> isoTime; // Buffer for ISO 8601 format: "YYYY-MM-DDTHH:MM:SS"
        std::strftime(isoTime.data(), sizeof(isoTime), "%Y-%m-%d %H:%M:%S", std::localtime(&m_lastLogTime));
        ReallyPrintLine(isoTime.data());
        ReallyPrintLine("\n");
    }

    // - Write the log message, safely, limiting the output buffer size
    std::array<TCHAR, (LINE_BUFFER_SIZE * 2) + 1> line; // sf@2006 - Prevents buffer overflow
    std::array<TCHAR, LINE_BUFFER_SIZE> szErrorMsg;
    DWORD dwErrorCode = ::GetLastError();
    _vsnprintf(line.data(), LINE_BUFFER_SIZE, format, ap);
    SetLastError(0);
    if (dwErrorCode != 0)
    {
        if (::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (char*)szErrorMsg.data(), static_cast<DWORD>(szErrorMsg.size()), nullptr) == 0)
        {
            ::sprintf_s(szErrorMsg.data(), szErrorMsg.size(), "error code 0x%08X", dwErrorCode);
        }
        ::strcat_s(line.data(), line.size(), " --");
        ::strcat_s(line.data(), line.size(), szErrorMsg.data());
    }
    ReallyPrintLine(line.data());
}

void VNCLog::ReallyPrintScreen(const char* const format, const va_list ap)
{
    time_t current = std::time(0);
    std::array<char, 20U> isoTime; // Buffer for ISO 8601 format: "YYYY-MM-DDTHH:MM:SS"
    std::strftime(isoTime.data(), sizeof(isoTime), "%Y-%m-%d %H:%M:%S", std::localtime(&current));
    std::array<TCHAR, (LINE_BUFFER_SIZE * 2) + 1> line;
    std::array<TCHAR, (LINE_BUFFER_SIZE * 2) + 1> line2;
    _vsnprintf(line.data(), LINE_BUFFER_SIZE, format, ap);
    ::strcpy_s(line2.data(), line2.size(), isoTime.data());
    ::strcat_s(line2.data(), line2.size(), " ");
    ::strcat_s(line2.data(), line2.size(), line.data());
    PropertiesDialog::LogToEdit(line2.data());
}

VNCLog::~VNCLog()
{
    try
    {
        CloseFile();
    }
    catch (...)
    {
    }
}

void VNCLog::GetLastErrorMsg(LPSTR szErrorMsg) const
{

    DWORD dwErrorCode = ::GetLastError();

    // Format the error message.
    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, dwErrorCode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&szErrorMsg, 0, nullptr);
}

void VNCLog::SetPath(char path[512U])
{
    if (std::strlen(path) == 0)
    {
        ::strcpy_s(m_path.data(), m_path.size(), winvncFolder.data());
    }
    else
    {
        ::strcpy_s(m_path.data(), m_path.size(), path);
    }
}
char* VNCLog::GetPath()
{
    if (std::strlen(m_path.data()) == 0)
    {
        ::strcpy_s(m_path.data(), m_path.size(), winvncFolder.data());
    }

    return m_path.data();
}

void VNCLog::ClearAviConfig()
{
    std::array<char, MAX_PATH> WORKDIR;
    ::strcpy_s(WORKDIR.data(), WORKDIR.size(), m_path.data());
    ::strcat_s(WORKDIR.data(), WORKDIR.size(), "\\");
    ::strcat_s(WORKDIR.data(), WORKDIR.size(), "codec.cfg");
    ::DeleteFile(WORKDIR.data());
}
