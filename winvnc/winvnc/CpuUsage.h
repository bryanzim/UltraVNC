// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#pragma once

#include <WinSock2.h>

#include <Windows.h>

#include <array>
////////////////////////////////////////
// class CProcessorUsage;
//
// Calculates overal processor usage at
// any given time.
//
// The usage value is updated every 200
// milliseconds;
//
// The class is fully thread-safe;
//
class CProcessorUsage
{
    typedef BOOL(WINAPI* pfnGetSystemTimes)(LPFILETIME lpIdleTime, LPFILETIME lpKernelTime, LPFILETIME lpUserTime);
    typedef LONG(WINAPI* pfnNtQuerySystemInformation)(ULONG SystemInformationClass, PVOID SystemInformation,
                                                      ULONG SystemInformationLength, PULONG ReturnLength);

    struct PROC_PERF_INFO
    {
        LARGE_INTEGER IdleTime;
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        std::array<LARGE_INTEGER, 2U> Reserved1;
        ULONG Reserved2;
    };

public:
    explicit CProcessorUsage();
    ~CProcessorUsage();

    short GetUsage();
    bool m_bLocked;

private:
    void GetSysTimes(__int64& idleTime, __int64& kernelTime, __int64& userTime);

    ////////////////////////////////////////////////
    // Set of static variables to be accessed from
    // within critical section by multiple threads:
    //
    static DWORD s_TickMark;
    static __int64 s_time;
    static __int64 s_idleTime;
    static __int64 s_kernelTime;
    static __int64 s_userTime;
    static int s_lastCpu;
    static std::array<int, 5U> s_cpu;
    static __int64 s_kernelTimeProcess;
    static __int64 s_userTimeProcess;
    static std::array<int, 5U> s_cpuProcess;
    static int s_count;
    static int s_index;
    //
    /////////////////////////////////////////////////

    pfnGetSystemTimes s_pfnGetSystemTimes;
    pfnNtQuerySystemInformation s_pfnNtQuerySystemInformation;
    CRITICAL_SECTION m_cs;
    PROC_PERF_INFO* m_pInfo;
    ULONG m_uInfoLength;
};
