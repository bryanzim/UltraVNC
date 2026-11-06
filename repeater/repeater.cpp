// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include <array>
#include <cstdio>
#include <cstring>

#include "repeater.h"

extern int notstopped;

static int visible;


void LogStats(const int code, const long recv, const long send)
{
    std::array<char, MAX_PATH> szFileName;
    std::array<char, 128U> tempchar;
    [[maybe_unused]] HANDLE hFile = nullptr;
    FILE* f;
    if (::GetModuleFileName(nullptr, szFileName.data(), static_cast<DWORD>(szFileName.size())))
    {
        char* p = std::strrchr(szFileName.data(), '\\');
        *p = '\0';
        ::strcat_s(szFileName.data(), szFileName.size(), "\\");
        ::_itoa_s(code, tempchar.data(), tempchar.size(), 10);
        ::strcat_s(szFileName.data(), szFileName.size(), tempchar.data());
        ::strcat_s(szFileName.data(), szFileName.size(), ".txt");
    }

    if ((f = std::fopen(reinterpret_cast<LPCSTR>(szFileName.data()), "a")) != nullptr)
    {
        std::array<char, 100U> msg;
        std::array<char, 5U> buf;
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        ::_itoa_s(st.wYear, buf.data(), buf.size(), 10);
        ::strcpy_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), "/");
        ::_itoa_s(st.wMonth, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), "/");
        ::_itoa_s(st.wDay, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), " ");
        ::_itoa_s(st.wHour, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), ":");
        ::_itoa_s(st.wMinute, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), ":");
        ::_itoa_s(st.wSecond, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), " ");
        ::strcat_s(msg.data(), msg.size(), "Transmitted: ");
        ::_ltoa_s((send + recv) / 512, tempchar.data(), tempchar.size(), 10);
        ::strcat_s(msg.data(), msg.size(), tempchar.data());
        ::strcat_s(msg.data(), msg.size(), "k \n");

        std::fprintf(f, msg.data());
        std::fclose(f);
    }
}
void LogStats_server(const int code, const int nummer)
{
    std::array<char, MAX_PATH> szFileName;
    //char tempchar[128];
    [[maybe_unused]] HANDLE hFile = nullptr;
    FILE* f;
    if (::GetModuleFileName(nullptr, szFileName.data(), static_cast<DWORD>(szFileName.size())))
    {
        char* p = std::strrchr(szFileName.data(), '\\');
        *p = '\0';
        ::strcat_s(szFileName.data(), szFileName.size(), "\\");
        ::strcat_s(szFileName.data(), szFileName.size(), "server_access.txt");
    }

    if ((f = std::fopen(reinterpret_cast<LPCSTR>(szFileName.data()), "a")) != nullptr)
    {
        std::array<char, 100U> msg;
        std::array<char, 5U> buf;
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        ::_itoa_s(st.wYear, buf.data(), buf.size(), 10);
        ::strcpy_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), "/");
        ::_itoa_s(st.wMonth, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), "/");
        ::_itoa_s(st.wDay, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), " ");
        ::_itoa_s(st.wHour, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), ":");
        ::_itoa_s(st.wMinute, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), ":");
        ::_itoa_s(st.wSecond, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), " ");

        std::fprintf(f, "%s;%i;%s\n", msg.data(), code, Servers[nummer].hostname);
        std::fclose(f);
    }
}

void LogStats_viewer(const int code, const int nummer)
{
    std::array<char, MAX_PATH> szFileName;
    //char tempchar[128];
    [[maybe_unused]] HANDLE hFile = nullptr;
    FILE* f;
    if (::GetModuleFileName(nullptr, szFileName.data(), static_cast<DWORD>(szFileName.size())))
    {
        char* p = std::strrchr(szFileName.data(), '\\');
        *p = '\0';
        ::strcat_s(szFileName.data(), szFileName.size(), "\\");
        ::strcat_s(szFileName.data(), szFileName.size(), "viewer_access.txt");
    }

    if ((f = std::fopen(reinterpret_cast<LPCSTR>(szFileName.data()), "a")) != nullptr)
    {
        std::array<char, 100U> msg;
        std::array<char, 5U> buf;
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        ::_itoa_s(st.wYear, buf.data(), buf.size(), 10);
        ::strcpy_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), "/");
        ::_itoa_s(st.wMonth, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), "/");
        ::_itoa_s(st.wDay, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), " ");
        ::_itoa_s(st.wHour, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), ":");
        ::_itoa_s(st.wMinute, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), ":");
        ::_itoa_s(st.wSecond, buf.data(), buf.size(), 10);
        ::strcat_s(msg.data(), msg.size(), buf.data());
        ::strcat_s(msg.data(), msg.size(), " ");

        std::fprintf(f, "%s;%i;%s\n", msg.data(), code, Viewers[nummer].hostname);
        std::fclose(f);
    }
}

void LogStats_access(char* const start, char* const stop, const int code, const int viewer, const int server, const long bytes)
{
    std::array<char, MAX_PATH> szFileName;
    //	char tempchar[128];
    [[maybe_unused]] HANDLE hFile = nullptr;
    FILE* f;
    if (::GetModuleFileName(nullptr, szFileName.data(), static_cast<DWORD>(szFileName.size())))
    {
        char* p = std::strrchr(szFileName.data(), '\\');
        *p = '\0';
        ::strcat_s(szFileName.data(), szFileName.size(), "\\");
        ::strcat_s(szFileName.data(), szFileName.size(), "connections.txt");
    }

    if ((f = std::fopen(reinterpret_cast<LPCSTR>(szFileName.data()), "a")) != nullptr)
    {
        std::fprintf(f, "%s;%s;%i;%s;%s;%u\n", start, stop, code, Viewers[viewer].hostname, Servers[server].hostname, bytes);
        std::fclose(f);
    }
}

void debug(const char* const fmt, ...)
{
    std::array<char, 256U> myoutput;
    std::array<char, 256U> myoutput2;
    va_list args;
    std::memset(myoutput2.data(), 0, 256);
    va_start(args, fmt);
    ::sprintf_s(myoutput.data(), myoutput.size(), "UltraVNC> ");
    ::vsprintf_s(myoutput.data(), myoutput.size(), fmt, args);
    va_end(args);
    ::strncpy_s(myoutput2.data(), myoutput2.size(), myoutput.data(), std::strlen(myoutput.data()) - 1);
    win_log(myoutput2.data());
}



void error(const char* const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::fprintf(stderr, "ERROR: ");
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void fatal(const char* const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::fprintf(stderr, "FATAL: ");
    vfprintf(stderr, fmt, args);
    va_end(args);
    //exit (EXIT_FAILURE);
}

void report_bytes(char* const prefix, char* buf, int len)
{
    debug("%s", prefix);
    while (0 < len)
    {
        std::fprintf(stderr, " %02x", *reinterpret_cast<unsigned char*>(buf));
        ++buf;
        --len;
    }
    std::fprintf(stderr, "\n");
    return;
}



int main_test() // int argc, char **argv )
{
    while (notwebstopped)
    {
        notstopped = 1;
        WSADATA wsadata;
        ::WSAStartup(0x101, &wsadata);
        Clean_server_List();
        Clean_viewer_List();


        debug("Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.\n");
        debug("\n");
        debug("\n");
        debug("The Repeater is free software; you can redistribute it and/or modify\n");
        debug("it under the terms of the GNU General Public License as published by\n");
        debug("the Free Software Foundation; either version 2 of the License, or\n");
        debug("(at your option) any later version.\n");
        debug(" \n");
        debug("This program is distributed in the hope that it will be useful,\n");
        debug("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
        debug("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
        debug("GNU General Public License for more details.\n");
        debug(" \n");
        debug("You should have received a copy of the GNU General Public License\n");
        debug("along with this program; if not, write to the Free Software\n");
        debug("Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,\n");
        debug("USA.\n");
        debug(" \n");


        if (saved_mode2)
        {
            Start_server_listenThread();
        }
        Start_cleaupthread();
        Start_mode12listenerThread();
        while (notstopped)
        {
            ::Sleep(100);
        }
        Stop_server_listenThread();
        Stop_cleaupthread();
        Stop_mode12listenerThread();
        ::WSACleanup();
    }
    return 0;
}
