// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "uvncUiAccess.h"

#include "vncOSVersion.h"
#include "winvnc.h"

#include <array>
#include <cstdio>
#include <cstring>

comm_serv* keyEventFn = nullptr;
comm_serv* StopeventFn = nullptr;
comm_serv* StarteventFn = nullptr;

int g_lockcode{ 0 };
int g_initialized = false;
int errorcounter{ 0 };
int preventDeadlock{ 0 };

mini_lock::mini_lock(int lockcode)
{
    preventDeadlock = 0;
    while (g_lockcode != 0)
    {

        ::Sleep(100);
        ++preventDeadlock;
        if (preventDeadlock > 20)
        {
            g_lockcode = 0;
        }
    }
    g_lockcode = lockcode;
}

mini_lock::~mini_lock() { g_lockcode = 0; }


bool Shellexecuteforuiaccess()
{
    std::array<char, MAX_PATH> WORKDIR;
    ::strcpy_s(WORKDIR.data(), WORKDIR.size(), winvncFolder.data());
    ::strcat_s(WORKDIR.data(), WORKDIR.size(), "\\uvnckeyboardhelper.exe");

    FILE* fp = std::fopen(WORKDIR.data(), "rb");
    if (fp)
    {
        std::fclose(fp);
    }
    else
    {
        return false;
    }

    SHELLEXECUTEINFO shExecInfo;
    std::memset(&shExecInfo, 0, sizeof(shExecInfo));
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.fMask = 0;
    shExecInfo.hwnd = nullptr;
    shExecInfo.lpVerb = "runas";
    shExecInfo.lpFile = WORKDIR.data();
    shExecInfo.lpParameters = "";
    shExecInfo.lpDirectory = nullptr;
    shExecInfo.nShow = SW_HIDE;
    shExecInfo.hInstApp = nullptr;
    if (!::ShellExecuteEx(&shExecInfo))
    {
        return false;
    }
    return true;
    ::Sleep(2000);
}

void keepalive()
{
    if (!VNC_OSVersion::getInstance()->OS_WIN8 || (errorcounter > 3))
    {
        return;
    }
    mini_lock ml(1);
    unsigned char Invalue{ 12U };
    unsigned char Outvalue{ 0U };
    if (StarteventFn)
    {
        StarteventFn->Call_Fnction_Long_Timeout((char*)&Invalue, (char*)&Outvalue, 1);
        if (Invalue == Outvalue)
        {
            errorcounter = 0;
            return;
        }
    }
    //Try to reinit the keyboard
    keybd_initialize_no_crit();
    ++errorcounter;
    if (errorcounter > 3)
    {
        goto error;
    }
    return;
// This disable the keyboard helper, better to have something
error:
    if (keyEventFn)
    {
        delete keyEventFn;
    }
    keyEventFn = nullptr;
    if (StopeventFn)
    {
        delete StopeventFn;
    }
    StopeventFn = nullptr;
    if (StarteventFn)
    {
        delete StarteventFn;
    }
    StarteventFn = nullptr;
}

void keybd_uni_event(_In_ BYTE bVk, _In_ BYTE bScan, const _In_ DWORD dwFlags, const _In_ ULONG_PTR dwExtraInfo)
{
    if (!VNC_OSVersion::getInstance()->OS_WIN8 || (g_lockcode != 0) || !g_initialized)
    {
        keybd_event(bVk, bScan, dwFlags, dwExtraInfo);
        return;
    }
    mini_lock ml(2);
    bool ldown = HIBYTE(::GetKeyState(VK_LMENU)) != 0;
    bool lcdown = HIBYTE(::GetKeyState(VK_LCONTROL)) != 0;
    bool lwindown = HIBYTE(::GetKeyState(VK_LWIN)) != 0;
    // The shared memory trick to inject keys seems to generate an overhead when fast key presses are done.
    // Actual we only need it for special keys, so we better ctivate the check
    if ((keyEventFn == nullptr) || (!ldown && !lcdown && !lwindown))
    {
        keybd_event(bVk, bScan, dwFlags, dwExtraInfo);
    }
    else
    {
        keyEventdata ked;
        ked.bVk = bVk;
        ked.bScan = bScan;
        ked.dwflags = dwFlags;
        keyEventFn->Call_Fnction((char*)&ked, nullptr);
    }
}

void keybd_initialize_no_crit()
{
    {
        if (keyEventFn)
        {
            delete keyEventFn;
        }
        keyEventFn = nullptr;
        if (StopeventFn)
        {
            delete StopeventFn;
        }
        StopeventFn = nullptr;
        if (StarteventFn)
        {
            delete StarteventFn;
        }
        StarteventFn = nullptr;
        keyEventFn = new comm_serv;
        StopeventFn = new comm_serv;
        StarteventFn = new comm_serv;
        if (!keyEventFn->Init("keyEvent", sizeof(keyEventdata), 0, false, true))
        {
            goto error;
        }
        if (!StopeventFn->Init("stop_event", 0, 0, false, true))
        {
            goto error;
        }
        if (!StarteventFn->Init("start_event", 1, 1, false, true))
        {
            goto error;
        }
        if (!Shellexecuteforuiaccess())
        {
            goto error;
        }
        unsigned char Invalue{ 12U };
        unsigned char Outvalue{ 0U };
        if (StarteventFn == nullptr)
        {
            goto error;
        }
        int StartErrorCounter{ 0 };
        while (Invalue != Outvalue)
        {
            StarteventFn->Call_Fnction_Long_Timeout((char*)&Invalue, (char*)&Outvalue, 1);
            ++StartErrorCounter;
            if (StartErrorCounter > 5)
            {
                goto error;
            }
        }
        return;
    }

error:
    if (keyEventFn)
    {
        delete keyEventFn;
    }
    keyEventFn = nullptr;
    if (StopeventFn)
    {
        delete StopeventFn;
    }
    StopeventFn = nullptr;
    if (StarteventFn)
    {
        delete StarteventFn;
    }
    StarteventFn = nullptr;
    return;
}

void keybd_initialize()
{
    {
        if (!VNC_OSVersion::getInstance()->OS_WIN8)
        {
            return;
        }
        g_initialized = true;
        errorcounter = 0;
        g_lockcode = 0;
        mini_lock ml(4);
        keyEventFn = new comm_serv;
        StopeventFn = new comm_serv;
        StarteventFn = new comm_serv;
        if (!keyEventFn->Init("keyEvent", sizeof(keyEventdata), 0, false, true))
        {
            goto error;
        }
        if (!StopeventFn->Init("stop_event", 0, 0, false, true))
        {
            goto error;
        }
        if (!StarteventFn->Init("start_event", 1, 1, false, true))
        {
            goto error;
        }
        if (!Shellexecuteforuiaccess())
        {
            goto error;
        }
        unsigned char Invalue{ 12U };
        unsigned char Outvalue{ 0U };
        if (StarteventFn == nullptr)
        {
            goto error;
        }
        int StartErrorCounter{ 0 };
        while (Invalue != Outvalue)
        {
            StarteventFn->Call_Fnction_Long_Timeout((char*)&Invalue, (char*)&Outvalue, 1);
            ++StartErrorCounter;
            if (StartErrorCounter > 5)
            {
                goto error;
            }
        }
        return;
    }
error:
    if (keyEventFn)
    {
        delete keyEventFn;
    }
    keyEventFn = nullptr;
    if (StopeventFn)
    {
        delete StopeventFn;
    }
    StopeventFn = nullptr;
    if (StarteventFn)
    {
        delete StarteventFn;
    }
    StarteventFn = nullptr;
    return;
}

void keybd_delete()
{
    if (!VNC_OSVersion::getInstance()->OS_WIN8)
    {
        return;
    }
    if (StopeventFn)
    {
        StopeventFn->Call_Fnction_no_feedback();
    }
    if (keyEventFn)
    {
        delete keyEventFn;
    }
    keyEventFn = nullptr;
    if (StopeventFn)
    {
        delete StopeventFn;
    }
    StopeventFn = nullptr;
    if (StarteventFn)
    {
        delete StarteventFn;
    }
    StarteventFn = nullptr;
    g_initialized = false;
}

comm_serv::comm_serv()
{
    event_E_IN = nullptr;
    event_E_IN_DONE = nullptr;
    event_E_OUT = nullptr;
    event_E_OUT_DONE = nullptr;
    data_IN = nullptr;
    data_OUT = nullptr;
    hMapFile_IN = nullptr;
    hMapFile_OUT = nullptr;
    ::InitializeCriticalSection(&CriticalSection_IN);
    ::InitializeCriticalSection(&CriticalSection_OUT);
    timeout_counter = 0;
    GLOBAL_RUNNING = true;
}

comm_serv::~comm_serv()
{
    GLOBAL_RUNNING = false;
    if (event_E_IN)
    {
        ::CloseHandle(event_E_IN);
    }
    if (event_E_IN_DONE)
    {
        ::CloseHandle(event_E_IN_DONE);
    }
    if (event_E_OUT)
    {
        ::CloseHandle(event_E_OUT);
    }
    if (event_E_OUT_DONE)
    {
        ::CloseHandle(event_E_OUT_DONE);
    }
    if (data_IN)
    {
        ::UnmapViewOfFile(data_IN);
    }
    if (data_OUT)
    {
        ::UnmapViewOfFile(data_OUT);
    }
    if (hMapFile_IN)
    {
        ::CloseHandle(hMapFile_IN);
    }
    if (hMapFile_OUT)
    {
        ::CloseHandle(hMapFile_OUT);
    }
    ::DeleteCriticalSection(&CriticalSection_IN);
    ::DeleteCriticalSection(&CriticalSection_OUT);
}


void comm_serv::create_sec_attribute()
{
    std::array<char, SECURITY_DESCRIPTOR_MIN_LENGTH> secDesc;
    secAttr.nLength = sizeof(secAttr);
    secAttr.bInheritHandle = FALSE;
    secAttr.lpSecurityDescriptor = &secDesc;
    ::InitializeSecurityDescriptor(secAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    ::SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, 0, FALSE);

    PSECURITY_DESCRIPTOR pSD;
    BOOL retcode = ::ConvertStringSecurityDescriptorToSecurityDescriptor("S:(ML;;NW;;;LW)", SDDL_REVISION_1, &pSD, nullptr);

    if (retcode != 0)
    {
        PACL pSacl = nullptr;
        BOOL fSaclPresent = FALSE;
        BOOL fSaclDefaulted = FALSE;
        retcode = ::GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted);
        if (pSacl)
        {
            retcode = ::SetSecurityDescriptorSacl(secAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
        }
    }
}

bool comm_serv::Init(char* const name, const int IN_datasize_IN, const int IN_datasize_OUT, const bool app, const bool master)
{
    datasize_IN = IN_datasize_IN;
    datasize_OUT = IN_datasize_OUT;

    std::array<char, SECURITY_DESCRIPTOR_MIN_LENGTH> secDesc;
    secAttr.nLength = sizeof(secAttr);
    secAttr.bInheritHandle = FALSE;
    secAttr.lpSecurityDescriptor = &secDesc;
    ::InitializeSecurityDescriptor(secAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    ::SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, 0, FALSE);

    PSECURITY_DESCRIPTOR pSD;
    BOOL retcode = ::ConvertStringSecurityDescriptorToSecurityDescriptor("S:(ML;;NW;;;LW)", SDDL_REVISION_1, &pSD, nullptr);

    if (retcode != 0)
    {
        PACL pSacl = nullptr;
        BOOL fSaclPresent = FALSE;
        BOOL fSaclDefaulted = FALSE;
        retcode = ::GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted);
        if (pSacl)
        {
            retcode = ::SetSecurityDescriptorSacl(secAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
        }
    }

    std::array<char, 42U> savename;
    ::strcpy_s(savename.data(), 42, name);
    if (app)
    {
        ::strcpy_s(filemapping_IN.data(), 64, "");
        ::strcpy_s(filemapping_OUT.data(), 64, "");
        ::strcpy_s(event_IN.data(), 64, "");
        ::strcpy_s(event_IN_DONE.data(), 64, "");
        ::strcpy_s(event_OUT.data(), 64, "");
        ::strcpy_s(event_OUT_DONE.data(), 64, "");

        ::strcat_s(filemapping_IN.data(), 64, name);
        ::strcat_s(filemapping_IN.data(), 64, "fm_IN");
        ::strcat_s(filemapping_OUT.data(), 64, name);
        ::strcat_s(filemapping_OUT.data(), 64, "fm_OUT");
        ::strcat_s(event_IN.data(), 64, name);
        ::strcat_s(event_IN.data(), 64, "event_IN");
        ::strcat_s(event_IN_DONE.data(), 64, name);
        ::strcat_s(event_IN_DONE.data(), 64, "event_IN_DONE");
        ::strcat_s(event_OUT.data(), 64, name);
        ::strcat_s(event_OUT.data(), 64, "event_OUT");
        ::strcat_s(event_OUT_DONE.data(), 64, name);
        ::strcat_s(event_OUT_DONE.data(), 64, "event_OUT_DONE");
    }
    else
    {
        ::strcpy_s(filemapping_IN.data(), 64, "Global\\");
        ::strcpy_s(filemapping_OUT.data(), 64, "Global\\");
        ::strcpy_s(event_IN.data(), 64, "Global\\");
        ::strcpy_s(event_IN_DONE.data(), 64, "Global\\");
        ::strcpy_s(event_OUT.data(), 64, "Global\\");
        ::strcpy_s(event_OUT_DONE.data(), 64, "Global\\");

        ::strcat_s(filemapping_IN.data(), 64, name);
        ::strcat_s(filemapping_IN.data(), 64, "fm_IN");
        ::strcat_s(filemapping_OUT.data(), 64, name);
        ::strcat_s(filemapping_OUT.data(), 64, "fm_OUT");
        ::strcat_s(event_IN.data(), 64, name);
        ::strcat_s(event_IN.data(), 64, "event_IN");
        ::strcat_s(event_IN_DONE.data(), 64, name);
        ::strcat_s(event_IN_DONE.data(), 64, "event_IN_DONE");
        ::strcat_s(event_OUT.data(), 64, name);
        ::strcat_s(event_OUT.data(), 64, "event_OUT");
        ::strcat_s(event_OUT_DONE.data(), 64, name);
        ::strcat_s(event_OUT_DONE.data(), 64, "event_OUT_DONE");
    }

    if (master)
    {
        if (!app)
        {
            if (datasize_IN != 0)
            {
                hMapFile_IN = ::CreateFileMapping(INVALID_HANDLE_VALUE, &secAttr, PAGE_READWRITE, 0, datasize_IN, filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return false;
                }
                data_IN = (char*)::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN);
                if (data_IN == nullptr)
                {
                    return false;
                }
            }
            event_E_IN = ::CreateEvent(&secAttr, FALSE, FALSE, event_IN.data());
            if (event_E_IN == nullptr)
            {
                return false;
            }
            event_E_IN_DONE = ::CreateEvent(&secAttr, FALSE, FALSE, event_IN_DONE.data());
            if (event_E_IN_DONE == nullptr)
            {
                return false;
            }

            if (datasize_OUT != 0)
            {
                hMapFile_OUT = ::CreateFileMapping(INVALID_HANDLE_VALUE, &secAttr, PAGE_READWRITE, 0, datasize_OUT, filemapping_OUT.data());
                if (hMapFile_OUT == nullptr)
                {
                    return false;
                }
                data_OUT = (char*)::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT);
                if (data_OUT == nullptr)
                {
                    return false;
                }
            }
            event_E_OUT = ::CreateEvent(&secAttr, FALSE, FALSE, event_OUT.data());
            if (event_E_OUT == nullptr)
            {
                return false;
            }
            event_E_OUT_DONE = ::CreateEvent(&secAttr, FALSE, FALSE, event_OUT_DONE.data());
            if (event_E_OUT_DONE == nullptr)
            {
                return false;
            }
        }
        else
        {
            if (datasize_IN != 0)
            {
                hMapFile_IN = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, datasize_IN, filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return false;
                }
                data_IN = (char*)::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN);
                if (data_IN == nullptr)
                {
                    return false;
                }
            }
            event_E_IN = ::CreateEvent(nullptr, FALSE, FALSE, event_IN.data());
            if (event_E_IN == nullptr)
            {
                return false;
            }
            event_E_IN_DONE = ::CreateEvent(nullptr, FALSE, FALSE, event_IN_DONE.data());
            if (event_E_IN_DONE == nullptr)
            {
                return false;
            }

            if (datasize_OUT != 0)
            {
                hMapFile_OUT = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, datasize_OUT, filemapping_OUT.data());
                if (hMapFile_OUT == nullptr)
                {
                    return false;
                }
                data_OUT = (char*)::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT);
                if (data_OUT == nullptr)
                {
                    return false;
                }
            }
            event_E_OUT = ::CreateEvent(nullptr, FALSE, FALSE, event_OUT.data());
            if (event_E_OUT == nullptr)
            {
                return false;
            }
            event_E_OUT_DONE = ::CreateEvent(nullptr, FALSE, FALSE, event_OUT_DONE.data());
            if (event_E_OUT_DONE == nullptr)
            {
                return false;
            }
        }
    }
    else
    {
        if (!app)
        {
            if (datasize_IN != 0)
            {
                hMapFile_IN = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return false;
                }
                data_IN = (char*)::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN);
                if (data_IN == nullptr)
                {
                    return false;
                }
            }
            event_E_IN = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_IN.data());
            if (event_E_IN == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_IN);
            event_E_IN_DONE = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_IN_DONE.data());
            if (event_E_IN_DONE == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_IN_DONE);
            if (datasize_OUT != 0)
            {
                hMapFile_OUT = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, filemapping_OUT.data());
                if (hMapFile_OUT == nullptr)
                {
                    return false;
                }
                data_OUT = (char*)::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT);
                if (data_OUT == nullptr)
                {
                    return false;
                }
            }
            event_E_OUT = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_OUT.data());
            if (event_E_OUT == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_OUT);
            event_E_OUT_DONE = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_OUT_DONE.data());
            if (event_E_OUT_DONE == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_OUT_DONE);
        }
        else
        {
            if (datasize_IN != 0)
            {
                hMapFile_IN = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return false;
                }
                data_IN = (char*)::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN);
                if (data_IN == nullptr)
                {
                    return false;
                }
            }
            event_E_IN = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_IN.data());
            if (event_E_IN == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_IN);
            event_E_IN_DONE = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_IN_DONE.data());
            if (event_E_IN_DONE == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_IN_DONE);

            if (datasize_OUT != 0)
            {
                hMapFile_OUT = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, filemapping_OUT.data());
                if (hMapFile_OUT == nullptr)
                {
                    return false;
                }
                data_OUT = (char*)::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT);
                if (data_OUT == nullptr)
                {
                    return false;
                }
            }
            event_E_OUT = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_OUT.data());
            if (event_E_OUT == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_OUT);
            event_E_OUT_DONE = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_OUT_DONE.data());
            if (event_E_OUT_DONE == nullptr)
            {
                return false;
            }
            ::ResetEvent(event_E_OUT_DONE);
        }
    }
    return true;
}

//service call session function
void comm_serv::Call_Fnction(char* const databuffer_IN, char* const databuffer_OUT)
{
    if (!GLOBAL_RUNNING)
    {
        return;
    }
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, datasize_IN);
    //ResetEvent(event_E_IN_DONE);
    if (event_E_OUT)
    {
        ::ResetEvent(event_E_OUT);
    }
    if (event_E_OUT_DONE)
    {
        ::ResetEvent(event_E_OUT_DONE);
    }
    if (event_E_IN)
    {
        ::SetEvent(event_E_IN);
    }
    DWORD r = ::WaitForSingleObject(event_E_IN_DONE, 1000);

    if (r == WAIT_TIMEOUT)
    {
        r = 1;
        ++timeout_counter;
    }
    else
    {
        timeout_counter = 0;
    }

    if (timeout_counter > 3)
    {
        GLOBAL_RUNNING = false;
    }
    if (!GLOBAL_RUNNING)
    {
        goto error;
    }
    r = ::WaitForSingleObject(event_E_OUT, 1000);
    std::memcpy(databuffer_OUT, data_OUT, datasize_OUT);
error:
    if (event_E_OUT_DONE)
    {
        ::SetEvent(event_E_OUT_DONE);
    }
    ::LeaveCriticalSection(&CriticalSection_IN);
}

void comm_serv::Call_Fnction_no_feedback() { ::SetEvent(event_E_IN); }

void comm_serv::Call_Fnction_no_feedback_data(char* const databuffer_IN, char* const databuffer_OUT)
{
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, datasize_IN);
    ::SetEvent(event_E_IN);
    ::LeaveCriticalSection(&CriticalSection_IN);
}

//service call session function
void comm_serv::Call_Fnction_Long(char* const databuffer_IN, char* const databuffer_OUT)
{
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, datasize_IN);
    ::SetEvent(event_E_IN);
    DWORD r = ::WaitForSingleObject(event_E_IN_DONE, 10000);
    if (r == WAIT_TIMEOUT)
    {
        r = 1;
    }

    ::LeaveCriticalSection(&CriticalSection_IN);

    ::EnterCriticalSection(&CriticalSection_OUT);
    r = ::WaitForSingleObject(event_E_OUT, 10000);
    if (r == WAIT_TIMEOUT)
    {
        r = 1;
    }

    std::memcpy(databuffer_OUT, data_OUT, datasize_OUT);
    if (event_E_OUT_DONE)
    {
        ::SetEvent(event_E_OUT_DONE);
    }
    ::LeaveCriticalSection(&CriticalSection_OUT);
}

//service call session function
void comm_serv::Call_Fnction_Long_Timeout(char* const databuffer_IN, char* const databuffer_OUT, int timeout)
{
    timeout = (timeout + 1) * 1000;
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, datasize_IN);
    ::ResetEvent(event_E_IN_DONE);
    ::ResetEvent(event_E_OUT);
    ::SetEvent(event_E_IN);
    DWORD r = ::WaitForSingleObject(event_E_IN_DONE, timeout);
    ::LeaveCriticalSection(&CriticalSection_IN);
    if (r == WAIT_TIMEOUT)
    {
        unsigned char value{ 99U };
        std::memcpy(databuffer_OUT, &value, datasize_OUT);
        return;
    }

    ::EnterCriticalSection(&CriticalSection_OUT);
    r = ::WaitForSingleObject(event_E_OUT, timeout);
    std::memcpy(databuffer_OUT, data_OUT, datasize_OUT);
    if (event_E_OUT_DONE)
    {
        ::SetEvent(event_E_OUT_DONE);
    }
    if (r == WAIT_TIMEOUT)
    {
        unsigned char value{ 99U };
        std::memcpy(databuffer_OUT, &value, datasize_OUT);
        ::LeaveCriticalSection(&CriticalSection_OUT);
        return;
    }
    ::LeaveCriticalSection(&CriticalSection_OUT);
}

HANDLE comm_serv::GetEvent() { return event_E_IN; }

char* comm_serv::Getsharedmem() { return data_IN; }

void comm_serv::ReadData(char* const databuffer)
{
    std::memcpy(databuffer, data_IN, datasize_IN);
    if (event_E_IN_DONE)
    {
        ::SetEvent(event_E_IN_DONE);
    }
}

void comm_serv::SetData(char* const databuffer)
{
    if (!GLOBAL_RUNNING)
    {
        return;
    }
    std::memcpy(data_OUT, databuffer, datasize_OUT);
    if (event_E_OUT)
    {
        ::SetEvent(event_E_OUT);
    }
    ::WaitForSingleObject(event_E_OUT_DONE, 2000);
}

void comm_serv::Force_unblock()
{
    if (event_E_OUT_DONE)
    {
        ::SetEvent(event_E_OUT_DONE);
    }
    if (event_E_IN_DONE)
    {
        ::SetEvent(event_E_IN_DONE);
    }
    if (event_E_OUT)
    {
        ::SetEvent(event_E_OUT);
    }
}

void comm_serv::Release()
{
    if (event_E_IN)
    {
        ::ResetEvent(event_E_IN);
    }
}