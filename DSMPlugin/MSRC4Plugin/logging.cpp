// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2005 Sean E. Covel All Rights Reserved.
//


#include "logging.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

HANDLE hLogFile;
long LOGIT{ 0 }; // Flag to turn on/off logging
long DEBUGIT{ 0 };
bool nameSet{ false };
std::array<char, LOGNAME_SIZE> logFile;

void SetLogFile(const char* const logname) { std::strcpy(logFile.data(), logname); }

int SetLogStatus(const char* const sLogit)
{
    char* stop;
    int logit{ 0 };
    logit = std::strtol(sLogit, &stop, 10);

    return logit;
}

int SetLogging(const char* const logname, char* const sLogit)
{

    SetLogFile(logname);

    return SetLogStatus(sLogit);
}

int SetLogging(const char* const logname)
{
    std::array<char, BUFFER_SIZE> dsmdebug;

    SetLogFile(logname);

    GetEnvVar("dsmdebug", dsmdebug.data(), dsmdebug.size());
    PrintLog((DEST, "dsmdebug = %s", dsmdebug.data()));

    return SetLogStatus(dsmdebug.data());
}

// Same as PrintIt, but checks a different variable
void DebugIt(const char* const sMsg)
{
    long saveIt{ 0 };


    if (DEBUGIT)
    {
        saveIt = LOGIT;

        PrintIt(sMsg);

        LOGIT = saveIt;
    }
}

// Stupid error logging function. (Kinda nice actually!)
void PrintIt(const char* const sMsg)
{
    std::array<char, LOGNAME_SIZE> logname;

    if (LOGIT)
    {

        DWORD dwLogBytes{ 0U };
        long lLastError;
        std::array<char, 2048U> tmpMsg;
        std::array<char, 10U> dbuffer;
        std::array<char, 10U> tbuffer;

        // Kinda cool, get last error and write it out. Helps with the API stuff
        lLastError = ::GetLastError();

        //pgm 		_strdate( dbuffer );
        //pgm 		_strtime( tbuffer );

        // Always append...
        if (hLogFile == 0)
        {
            time_t ltime;

            if (strstr(logFile.data(), ".log") != nullptr)
            {
                std::strcpy(logname.data(), logFile.data());
                nameSet = true;
            }

            if (nameSet == false)
            {
                std::time(&ltime);
                std::snprintf(logname.data(), logname.size(), "%s%d.log", logFile.data(), ltime);
                nameSet = true;
            }

            hLogFile =
                ::CreateFile(logname.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hLogFile == INVALID_HANDLE_VALUE)
            {
                std::time(&ltime);
                ++ltime;
                std::snprintf(logname.data(), logname.size(), "%s%d.log", logFile.data(), ltime);
                hLogFile = ::CreateFile(logname.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                      nullptr);
            }

            std::strcpy(logFile.data(), logname.data());
            if (APPEND)
            {
                ::SetFilePointer(hLogFile, 0, nullptr, FILE_END);
            } // Append
            else
            {
                ::SetEndOfFile(hLogFile);
            } // Overwrite
        }

        if (_snprintf(tmpMsg.data(), sizeof(tmpMsg), "%s %s : ", dbuffer.data(), tbuffer.data()) < 0)
        {
            PrintLog((DEST, "_snprintf failed - tmpMsg too small"));
        }

        ::WriteFile(hLogFile, tmpMsg.data(), std::strlen(tmpMsg.data()), &dwLogBytes, nullptr);
        ::WriteFile(hLogFile, sMsg, std::strlen(sMsg), &dwLogBytes, nullptr);

        if (lLastError < 0)
        {
            if (_snprintf(tmpMsg.data(), sizeof(tmpMsg), " - Last Error = %d : ", lLastError) < 0)
            {
                PrintLog((DEST, "_snprintf failed - tmpMsg too small"));
            }
            ::WriteFile(hLogFile, tmpMsg.data(), std::strlen(tmpMsg.data()), &dwLogBytes, nullptr);
        }
        ::WriteFile(hLogFile, "\r\n", 2, &dwLogBytes, nullptr);

        SetLastError(0);

        // Cheap way to clean up...
        if (std::strcmp(sMsg, "Shutting Down.") == 0)
        {
            ::CloseHandle(hLogFile);
        }
    }
}
