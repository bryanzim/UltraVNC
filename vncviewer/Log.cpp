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


// Log.cpp: implementation of the Log class.
//
//////////////////////////////////////////////////////////////////////

#include "stdhdrs.h"

#include "Log.h"

#include <array>
#include <cstring>
#include <cwchar>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

const int Log::ToDebug = 1;
const int Log::ToFile = 2;
const int Log::ToConsole = 4;

const static int LINE_BUFFER_SIZE{ 1024 };

Log::Log(const int mode, [[maybe_unused]] const int level, const LPTSTR filename, const bool append)
{
    hlogfile = nullptr;
    m_todebug = false;
    m_toconsole = false;
    m_tofile = false;
    SetMode(mode);
    if (mode & ToFile)
    {
        SetFile(filename, append);
    }
}

void Log::SetMode(const int mode)
{

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
        m_tofile = true;
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
            ::AllocConsole();
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }

        m_toconsole = true;
    }
    else
    {
        m_toconsole = false;
    }
}


void Log::SetLevel(int level) { m_level = level; }

void Log::SetFile(const LPTSTR filename, const bool append)
{
    // if a log file is open, close it now.
    CloseFile();

    m_tofile = true;

    // If filename is NULL or invalid we should throw an exception here

    hlogfile = ::CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hlogfile == INVALID_HANDLE_VALUE)
    {
        // We should throw an exception here
        m_todebug = true;
        m_tofile = false;
        Print(0, _T("Error opening log file %s\n"), filename);
    }
    if (append)
    {
        ::SetFilePointer(hlogfile, 0, nullptr, FILE_END);
    }
    else
    {
        ::SetEndOfFile(hlogfile);
    }
}

// if a log file is open, close it now.
void Log::CloseFile()
{
    if (hlogfile != nullptr)
    {
        ::CloseHandle(hlogfile);
        hlogfile = nullptr;
    }
}

void Log::ReallyPrint(const LPCTSTR format, const va_list ap)
{
    std::array<WCHAR, LINE_BUFFER_SIZE> line;
    _vsnwprintf_s(line.data(), line.size(), _TRUNCATE, format, ap);
    if (m_todebug)
    {
        ::OutputDebugStringW(line.data());
    }

    if (m_toconsole)
    {
        DWORD charsWritten;
        ::WriteConsoleW(::GetStdHandle(STD_OUTPUT_HANDLE), line.data(), static_cast<DWORD>(std::wcslen(line.data())), &charsWritten,
                      nullptr);
    };

    if (m_tofile && (hlogfile != nullptr))
    {
        std::array<char, LINE_BUFFER_SIZE> lineA;
        ::WideCharToMultiByte(CP_UTF8, 0, line.data(), -1, lineA.data(), static_cast<int>(lineA.size()), nullptr, nullptr);
        DWORD byteswritten;
        ::WriteFile(hlogfile, lineA.data(), static_cast<DWORD>(std::strlen(lineA.data())), &byteswritten, nullptr);
    }
}

Log::~Log() { CloseFile(); }

Log theLog;
