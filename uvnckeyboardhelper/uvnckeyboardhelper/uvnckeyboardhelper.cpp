// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// uvnckeyboardhelper.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

#include "uvnckeyboardhelper.h"

#include <array>
#include <cstring>

bool GLOBAL_RUNNING{ true };
bool fRunningFromExternalService{ false };
comm_serv::comm_serv()
{
    Force_unblock();
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
}

comm_serv::~comm_serv()
{
    ::CloseHandle(event_E_IN);
    ::CloseHandle(event_E_IN_DONE);
    ::CloseHandle(event_E_OUT);
    ::CloseHandle(event_E_OUT_DONE);
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
    ::SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, nullptr, FALSE);
    [[maybe_unused]] TCHAR* szSD = TEXT("D:")         // Discretionary ACL
                                     //TEXT("(D;OICI;GA;;;BG)")     // Deny access to built-in guests
                                     //TEXT("(D;OICI;GA;;;AN)")     // Deny access to anonymous logon
        TEXT("(A;OICI;GRGWGX;;;AU)") // Allow read/write/execute to authenticated users
        TEXT("(A;OICI;GA;;;BA)");    // Allow full control to administrators

    PSECURITY_DESCRIPTOR pSD;
    BOOL retcode = ::ConvertStringSecurityDescriptorToSecurityDescriptor("S:(ML;;NW;;;LW)", SDDL_REVISION_1, &pSD, nullptr);
    [[maybe_unused]] const DWORD aa = ::GetLastError();

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
    ::SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, nullptr, FALSE);
    [[maybe_unused]] TCHAR* szSD = TEXT("D:")         // Discretionary ACL
                                     //TEXT("(D;OICI;GA;;;BG)")     // Deny access to built-in guests
                                     //TEXT("(D;OICI;GA;;;AN)")     // Deny access to anonymous logon
        TEXT("(A;OICI;GRGWGX;;;AU)") // Allow read/write/execute to authenticated users
        TEXT("(A;OICI;GA;;;BA)");    // Allow full control to administrators

    PSECURITY_DESCRIPTOR pSD;
    BOOL retcode = ::ConvertStringSecurityDescriptorToSecurityDescriptor("S:(ML;;NW;;;LW)", SDDL_REVISION_1, &pSD, nullptr);
    [[maybe_unused]] DWORD aa = ::GetLastError();

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
                hMapFile_IN = ::CreateFileMapping(INVALID_HANDLE_VALUE, &secAttr, PAGE_READWRITE, 0, static_cast<DWORD>(datasize_IN), filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return false;
                }
                data_IN = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN));
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
                hMapFile_OUT = ::CreateFileMapping(INVALID_HANDLE_VALUE, &secAttr, PAGE_READWRITE, 0, static_cast<DWORD>(datasize_OUT), filemapping_OUT.data());
                if (hMapFile_OUT == nullptr)
                {
                    return false;
                }
                data_OUT = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT));
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
                hMapFile_IN = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(datasize_IN), filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return false;
                }
                data_IN = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN));
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
                hMapFile_OUT = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(datasize_OUT), filemapping_OUT.data());
                if (hMapFile_OUT == nullptr)
                {
                    return false;
                }
                data_OUT = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT));
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
                [[maybe_unused]] DWORD aaInner = ::GetLastError();
                if (hMapFile_IN == nullptr)
                {
                    return false;
                }
                data_IN = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN));
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
                data_OUT = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT));
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
                data_IN = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_IN, FILE_MAP_ALL_ACCESS, 0, 0, datasize_IN));
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
                data_OUT = reinterpret_cast<char*>(::MapViewOfFile(hMapFile_OUT, FILE_MAP_ALL_ACCESS, 0, 0, datasize_OUT));
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


HANDLE comm_serv::InitFileHandle(char* const name, const int IN_datasize_IN, const int IN_datasize_OUT, const bool app,
                                 const bool master)
{
    datasize_IN = IN_datasize_IN;
    datasize_OUT = IN_datasize_OUT;
    create_sec_attribute();
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
                hMapFile_IN = ::CreateFileMapping(INVALID_HANDLE_VALUE, &secAttr, PAGE_READWRITE, 0, static_cast<DWORD>(datasize_IN), filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return nullptr;
                }
            }
        }
        else
        {
            if (datasize_IN != 0)
            {
                hMapFile_IN = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(datasize_IN), filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return nullptr;
                }
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
                    return nullptr;
                }
            }
        }
        else
        {
            if (datasize_IN != 0)
            {
                hMapFile_IN = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, filemapping_IN.data());
                if (hMapFile_IN == nullptr)
                {
                    return nullptr;
                }
            }
        }
    }
    return hMapFile_IN;
}

//service call session function
void comm_serv::Call_Fnction(char* const databuffer_IN, char* const databuffer_OUT)
{
    if (!GLOBAL_RUNNING)
    {
        return;
    }
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, static_cast<size_t>(datasize_IN));
    //ResetEvent(event_E_IN_DONE);
    ::ResetEvent(event_E_OUT);
    ::ResetEvent(event_E_OUT_DONE);
    ::SetEvent(event_E_IN);
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
    std::memcpy(databuffer_OUT, data_OUT, static_cast<size_t>(datasize_OUT));
error:
    ::SetEvent(event_E_OUT_DONE);
    ::LeaveCriticalSection(&CriticalSection_IN);
}

void comm_serv::Call_Fnction_no_feedback() { ::SetEvent(event_E_IN); }

void comm_serv::Call_Fnction_no_feedback_data(char* const databuffer_IN, [[maybe_unused]] char* const databuffer_OUT)
{
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, static_cast<size_t>(datasize_IN));
    ::SetEvent(event_E_IN);
    ::LeaveCriticalSection(&CriticalSection_IN);
}

//service call session function
void comm_serv::Call_Fnction_Long(char* const databuffer_IN, char* const databuffer_OUT)
{
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, static_cast<size_t>(datasize_IN));
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

    std::memcpy(databuffer_OUT, data_OUT, static_cast<size_t>(datasize_OUT));
    ::SetEvent(event_E_OUT_DONE);
    ::LeaveCriticalSection(&CriticalSection_OUT);
}

//service call session function
void comm_serv::Call_Fnction_Long_Timeout(char* const databuffer_IN, char* const databuffer_OUT, int timeout)
{
    timeout = (timeout + 1) * 1000;
    ::EnterCriticalSection(&CriticalSection_IN);
    std::memcpy(data_IN, databuffer_IN, static_cast<size_t>(datasize_IN));
    ::ResetEvent(event_E_IN_DONE);
    ::ResetEvent(event_E_OUT);
    ::SetEvent(event_E_IN);
    DWORD r = ::WaitForSingleObject(event_E_IN_DONE, static_cast<DWORD>(timeout));
    ::LeaveCriticalSection(&CriticalSection_IN);
    if (r == WAIT_TIMEOUT)
    {
        unsigned char value{ 99U };
        std::memcpy(databuffer_OUT, &value, static_cast<size_t>(datasize_OUT));
        return;
    }

    ::EnterCriticalSection(&CriticalSection_OUT);
    r = ::WaitForSingleObject(event_E_OUT, static_cast<DWORD>(timeout));
    std::memcpy(databuffer_OUT, data_OUT, static_cast<size_t>(datasize_OUT));
    ::SetEvent(event_E_OUT_DONE);
    if (r == WAIT_TIMEOUT)
    {
        unsigned char value{ 99U };
        std::memcpy(databuffer_OUT, &value, static_cast<size_t>(datasize_OUT));
        return;
    }
    ::LeaveCriticalSection(&CriticalSection_OUT);
}

HANDLE comm_serv::GetEvent() { return event_E_IN; }

char* comm_serv::Getsharedmem() { return data_IN; }

void comm_serv::ReadData(char* const databuffer)
{
    std::memcpy(databuffer, data_IN, static_cast<size_t>(datasize_IN));
    ::SetEvent(event_E_IN_DONE);
}

void comm_serv::SetData(char* const databuffer)
{
    if (!GLOBAL_RUNNING)
    {
        return;
    }
    std::memcpy(data_OUT, databuffer, static_cast<size_t>(datasize_OUT));
    ::SetEvent(event_E_OUT);
    [[maybe_unused]] const DWORD r = ::WaitForSingleObject(event_E_OUT_DONE, 2000);
}

void comm_serv::Force_unblock()
{
    ::SetEvent(event_E_OUT_DONE);
    ::SetEvent(event_E_IN_DONE);
    ::SetEvent(event_E_OUT);
}

void comm_serv::Release() { ::ResetEvent(event_E_IN); }

bool SelectDesktop()
{
    HDESK desktop;
    HDESK old_desktop;
    DWORD dummy;
    std::array<char, 256U> new_name;
    desktop = ::OpenInputDesktop(0, FALSE,
                               DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                   DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
    if (desktop == nullptr)
    {
        return FALSE;
    }
    old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    if (!::GetUserObjectInformation(desktop, UOI_NAME, &new_name, 256, &dummy))
    {
        ::CloseDesktop(desktop);
        return FALSE;
    }
    if (!::SetThreadDesktop(desktop))
    {
        ::CloseDesktop(desktop);
        return FALSE;
    }
    ::CloseDesktop(old_desktop);
    return TRUE;
}

bool InputDesktopSelected()
{

    DWORD dummy;
    std::array<char, 256U> threadname;
    std::array<char, 256U> inputname;
    HDESK threaddesktop = ::GetThreadDesktop(::GetCurrentThreadId());
    HDESK inputdesktop = ::OpenInputDesktop(0, FALSE,
                                          DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                              DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);

    if (inputdesktop == nullptr)
    {
        return FALSE;
    }
    if (!::GetUserObjectInformation(threaddesktop, UOI_NAME, &threadname, 256, &dummy))
    {
        ::CloseDesktop(inputdesktop);
        return FALSE;
    }
    if (!::GetUserObjectInformation(inputdesktop, UOI_NAME, &inputname, 256, &dummy))
    {
        ::CloseDesktop(inputdesktop);
        return FALSE;
    }
    ::CloseDesktop(inputdesktop);

    if (std::strcmp(threadname.data(), inputname.data()) != 0)
    {
        //if (strcmp(inputname, "Screen-saver") == 0)
        {
            return SelectDesktop();
        }
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI Cadthread([[maybe_unused]] const LPVOID lpParam)
{
    HDESK inputdesktop = ::OpenInputDesktop(0, FALSE,
                                          DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                              DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
    ::SetThreadDesktop(inputdesktop);
    [[maybe_unused]] const DWORD yerror = ::GetLastError();

    comm_serv keyEventFn;
    comm_serv StopeventFn;
    comm_serv StarteventFn;
    if (!keyEventFn.Init("keyEvent", sizeof(keyEventdata), 0, false, false))
    {
        goto error;
    }
    if (!StopeventFn.Init("stop_event", 0, 0, false, false))
    {
        goto error;
    }
    if (!StarteventFn.Init("start_event", 1, 1, false, false))
    {
        goto error;
    }
    std::array<HANDLE, 1U> Events_ini;
    Events_ini[0U] = StarteventFn.GetEvent();
    DWORD dwEvent = ::WaitForMultipleObjects(1, Events_ini.data(), FALSE, 5000);
    switch (dwEvent)
    {
    case WAIT_OBJECT_0 + 0:
        unsigned char in;
        StarteventFn.ReadData(reinterpret_cast<char*>(&in));
        StarteventFn.SetData(reinterpret_cast<char*>(&in));
        break;
    case WAIT_TIMEOUT:
        goto error;
        break;
    }

    std::array<HANDLE, 3U> Events;
    Events[0U] = keyEventFn.GetEvent();
    Events[1U] = StopeventFn.GetEvent();
    Events[2U] = StarteventFn.GetEvent();
    int counter{ 0 };
    while (true)
    {
        DWORD dwEventInner = ::WaitForMultipleObjects(3, Events.data(), FALSE, 1000);
        switch (dwEventInner)
        {
        case WAIT_OBJECT_0 + 0:
            keyEventdata ked;
            keyEventFn.ReadData(reinterpret_cast<char*>(&ked));
            keybd_event(ked.bVk, ked.bScan, ked.dwflags, 0);
            keyEventFn.SetData(nullptr);
            break;
        case WAIT_OBJECT_0 + 1:
            goto error;
            break;
        case WAIT_OBJECT_0 + 2:
            unsigned char in;
            StarteventFn.ReadData(reinterpret_cast<char*>(&in));
            StarteventFn.SetData(reinterpret_cast<char*>(&in));
            InputDesktopSelected();
            counter = 0;
            break;
        case WAIT_TIMEOUT:
            ++counter;
            break;
        }
        if (counter > 5)
        {
            break;
        }
    }

error:
    return 0;
}

int APIENTRY _tWinMain([[maybe_unused]] const HINSTANCE hInstance, [[maybe_unused]] const HINSTANCE hPrevInstance, [[maybe_unused]] const LPTSTR lpCmdLine, [[maybe_unused]] const int nCmdShow)
{
    // thread is needed, else setthreaddesktop fail
    HANDLE ThreadHandle2;
    DWORD dwTId;
    ThreadHandle2 = ::CreateThread(nullptr, 0, Cadthread, nullptr, 0, &dwTId);
    ::WaitForSingleObject(ThreadHandle2, INFINITE);
    ::CloseHandle(ThreadHandle2);
}
