// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#include <array>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#include <tchar.h>

#endif
#include "Dtwinver.h"

#include <memory.h>

//#include <math.h>
#include <stdio.h>
#include <string.h>

#if !defined(_WIN32) && !defined(_WIN64)
#define _stprintf sprintf
#define _tcscat strcat
#define LPTSTR LPSTR
#endif

#include "SettingsManager.h"

int getinfo(char mytext[1024U])

{
    COSVersion::OS_VERSION_INFO osvi;
    std::memset(&osvi, 0, sizeof(osvi));
    std::array<TCHAR, 2048U> sText;
    std::array<TCHAR, 100U> sBuf;

    COSVersion os;
    if (os.GetVersion(&osvi))
    {
        ::_stprintf_s(sText.data(), sText.size(), _T(""));
        //CE does not really have a concept of an emulated OS so
        //lets not bother displaying any info on this on CE
        if (osvi.UnderlyingPlatform == COSVersion::WindowsCE)
        {
            ::_tcscpy_s(sText.data(), sText.size(), _T("OS: "));
        }
        else
        {
            ::_tcscat_s(sText.data(), sText.size(), _T("OS: "));
        }

        switch (osvi.UnderlyingPlatform)
        {
        case COSVersion::Dos: {
            ::_tcscat_s(sText.data(), sText.size(), _T("DOS"));
            break;
        }
        case COSVersion::Windows3x: {
            ::_tcscat_s(sText.data(), sText.size(), _T("Windows"));
            break;
        }
        case COSVersion::WindowsCE: {
            if (os.IsWindowsEmbeddedCompact(&osvi, TRUE))
            {
                ::_tcscat_s(sText.data(), sText.size(), _T("Windows Embedded Compact"));
            }
            else if (os.IsWindowsCENET(&osvi, TRUE))
            {
                ::_tcscat_s(sText.data(), sText.size(), _T("Windows CE .NET"));
            }
            else
            {
                ::_tcscat_s(sText.data(), sText.size(), _T("Windows CE"));
            }
            break;
        }
        case COSVersion::Windows9x: {
            if (os.IsWindows95(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows 95"));
            }
            else if (os.IsWindows95SP1(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows 95 SP1"));
            }
            else if (os.IsWindows95B(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows 95 B [aka OSR2]"));
            }
            else if (os.IsWindows95C(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows 95 C [aka OSR2.5]"));
            }
            else if (os.IsWindows98(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows 98"));
            }
            else if (os.IsWindows98SP1(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows 98 SP1"));
            }
            else if (os.IsWindows98SE(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows 98 Second Edition"));
            }
            else if (os.IsWindowsME(&osvi, TRUE))
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows Millenium Edition"));
            }
            else
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Windows \?\?"));
            }
            ::_tcscat_s(sText.data(), sText.size(), sBuf.data());
            break;
        }
        case COSVersion::WindowsNT: {
            if (os.IsNTPreWin2k(&osvi, TRUE))
            {
                ::_tcscat_s(sText.data(), sText.size(), _T("Windows NT"));

                if (os.IsNTWorkstation(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(" (Workstation)"));
                }
                else if (os.IsNTStandAloneServer(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(" (Server)"));
                }
                else if (os.IsNTPDC(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(" (Primary Domain Controller)"));
                }
                else if (os.IsNTBDC(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(" (Backup Domain Controller)"));
                }

                if (os.IsNTDatacenterServer(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter)"));
                }
                else if (os.IsNTEnterpriseServer(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Enterprise)"));
                }
            }
            else if (os.IsWindows2000(&osvi, TRUE))
            {
                if (os.IsProfessional(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 2000 (Professional)"));
                }
                else if (os.IsWindows2000Server(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 2000 Server"));
                }
                else if (os.IsWindows2000DomainController(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 2000 (Domain Controller)"));
                }
                else
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 2000"));
                }

                if (os.IsWindows2000DatacenterServer(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter)"));
                }
                else if (os.IsWindows2000AdvancedServer(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Advanced Server)"));
                }
            }
            else if (os.IsWindowsXPOrWindowsServer2003(&osvi, TRUE))
            {
                if (os.IsStarterEdition(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows XP (Starter Edition)"));
                }
                else if (os.IsPersonal(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows XP (Personal)"));
                }
                else if (os.IsProfessional(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows XP (Professional)"));
                }
                else if (os.IsWindowsServer2003(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2003"));
                }
                else if (os.IsDomainControllerWindowsServer2003(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2003 (Domain Controller)"));
                }
                else if (os.IsWindowsServer2003R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2003 R2"));
                }
                else if (os.IsDomainControllerWindowsServer2003R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2003 R2 (Domain Controller)"));
                }
                else
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows XP"));
                }

                if (os.IsDatacenterWindowsServer2003(&osvi, TRUE) || os.IsDatacenterWindowsServer2003R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter Edition)"));
                }
                else if (os.IsEnterpriseWindowsServer2003(&osvi, TRUE) || os.IsEnterpriseWindowsServer2003(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Enterprise Edition)"));
                }
                else if (os.IsWebWindowsServer2003(&osvi, TRUE) || os.IsWebWindowsServer2003R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Web Edition)"));
                }
                else if (os.IsStandardWindowsServer2003(&osvi, TRUE) || os.IsStandardWindowsServer2003R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Standard Edition)"));
                }
            }
            else if (os.IsWindowsVistaOrWindowsServer2008(&osvi, TRUE))
            {
                if (os.IsStarterEdition(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Vista (Starter Edition)"));
                }
                else if (os.IsHomeBasic(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Vista (Home Basic)"));
                }
                else if (os.IsHomeBasicPremium(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Vista (Home Premium)"));
                }
                else if (os.IsBusiness(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Vista (Business)"));
                }
                else if (os.IsEnterprise(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Vista (Enterprise)"));
                }
                else if (os.IsUltimate(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Vista (Ultimate)"));
                }
                else if (os.IsWindowsServer2008(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2008"));
                }
                else if (os.IsDomainControllerWindowsServer2008(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2008 (Domain Controller)"));
                }
                else
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Vista"));
                }

                if (os.IsDatacenterWindowsServer2008(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter Edition)"));
                }
                else if (os.IsEnterpriseWindowsServer2008(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Enterprise Edition)"));
                }
                else if (os.IsWebWindowsServer2008(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Web Edition)"));
                }
                else if (os.IsStandardWindowsServer2008(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Standard Edition)"));
                }
            }
            else if (os.IsWindows7OrWindowsServer2008R2(&osvi, TRUE))
            {
                if (os.IsThinPC(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7 Thin PC"));
                }
                else if (os.IsStarterEdition(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7 (Starter Edition)"));
                }
                else if (os.IsHomeBasic(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7 (Home Basic)"));
                }
                else if (os.IsHomeBasicPremium(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7 (Home Premium)"));
                }
                else if (os.IsProfessional(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7 (Professional)"));
                }
                else if (os.IsEnterprise(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7 (Enterprise)"));
                }
                else if (os.IsUltimate(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7 (Ultimate)"));
                }
                else if (os.IsWindowsServer2008R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2008 R2"));
                }
                else if (os.IsDomainControllerWindowsServer2008R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2008 R2 (Domain Controller)"));
                }
                else
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 7"));
                }

                if (os.IsDatacenterWindowsServer2008R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter Edition)"));
                }
                else if (os.IsEnterpriseWindowsServer2008R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Enterprise Edition)"));
                }
                else if (os.IsWebWindowsServer2008R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Web Edition)"));
                }
                else if (os.IsStandardWindowsServer2008R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Standard Edition)"));
                }
            }
            else if (os.IsWindows8OrWindowsServer2012(&osvi, TRUE))
            {
                if (os.IsThinPC(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8 Thin PC"));
                }
                else if (os.IsWindowsRT(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8 RT"));
                }
                else if (os.IsStarterEdition(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8 (Starter Edition)"));
                }
                else if (os.IsProfessional(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8 (Pro)"));
                }
                else if (os.IsEnterprise(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8 (Enterprise)"));
                }
                else if (os.IsWindowsServer2012(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2012"));
                }
                else if (os.IsDomainControllerWindowsServer2012(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2012 (Domain Controller)"));
                }
                else
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8"));
                }

                if (os.IsDatacenterWindowsServer2012(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter Edition)"));
                }
                else if (os.IsEnterpriseWindowsServer2012(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Enterprise Edition)"));
                }
                else if (os.IsWebWindowsServer2012(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Web Edition)"));
                }
                else if (os.IsStandardWindowsServer2012(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Standard Edition)"));
                }
            }
            else if (os.IsWindows8Point1OrWindowsServer2012R2(&osvi, TRUE))
            {
                if (os.IsThinPC(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8.1 Thin PC"));
                }
                else if (os.IsWindowsRT(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8.1 RT"));
                }
                else if (os.IsStarterEdition(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8.1 (Starter Edition)"));
                }
                else if (os.IsProfessional(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8.1 (Pro)"));
                }
                else if (os.IsEnterprise(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8.1 (Enterprise)"));
                }
                else if (os.IsWindowsServer2012R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2012 R2"));
                }
                else if (os.IsDomainControllerWindowsServer2012R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2012 R2 (Domain Controller)"));
                }
                else
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 8.1"));
                }

                if (os.IsCoreConnected(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (with Bing / CoreConnected)"));
                }
                if (os.IsWindows8Point1Or2012R2Update(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Update)"));
                }

                if (os.IsDatacenterWindowsServer2012R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter Edition)"));
                }
                else if (os.IsEnterpriseWindowsServer2012R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Enterprise Edition)"));
                }
                else if (os.IsWebWindowsServer2012R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Web Edition)"));
                }
                else if (os.IsStandardWindowsServer2012R2(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Standard Edition)"));
                }
            }
            else if (os.IsWindows10OrWindowsServer2016(&osvi, TRUE))
            {
                if (os.IsThinPC(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 Thin PC"));
                }
                else if (os.IsWindowsRT(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 RT"));
                }
                else if (os.IsStarterEdition(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 (Starter Edition)"));
                }
                else if (os.IsCore(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 (Home)"));
                }
                else if (os.IsProfessional(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 (Pro)"));
                }
                else if (os.IsEnterprise(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 (Enterprise)"));
                }
                else if (os.IsNanoServer(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 Nano Server"));
                }
                else if (os.IsARM64Server(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10 ARM64 Server"));
                }
                else if (os.IsWindowsServer2016(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2016"));
                }
                else if (os.IsDomainControllerWindowsServer2016(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows Server 2016 (Domain Controller)"));
                }
                else
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T("Windows 10"));
                }

                if (os.IsCoreConnected(&osvi))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (with Bing / CoreConnected)"));
                }

                if (os.IsDatacenterWindowsServer2016(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Datacenter Edition)"));
                }
                else if (os.IsEnterpriseWindowsServer2016(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Enterprise Edition)"));
                }
                else if (os.IsWebWindowsServer2016(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Web Edition)"));
                }
                else if (os.IsStandardWindowsServer2016(&osvi, TRUE))
                {
                    ::_tcscat_s(sText.data(), sText.size(), _T(", (Standard Edition)"));
                }
            }
            break;
        }
        default: {
            ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Unknown OS"));
            ::_tcscat_s(sText.data(), sText.size(), sBuf.data());
            break;
        }
        }

#ifndef UNDER_CE
        switch (osvi.UnderlyingProcessorType)
        {
        case COSVersion::X86_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (x86-32 Processor)"));
            break;
        }
        case COSVersion::MIPS_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (MIPS Processor)"));
            break;
        }
        case COSVersion::ALPHA_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Alpha Processor)"));
            break;
        }
        case COSVersion::PPC_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (PPC Processor)"));
            break;
        }
        case COSVersion::IA64_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (IA64 Itanium[2] Processor)"));
            break;
        }
        case COSVersion::AMD64_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (x86-64 Processor)"));
            break;
        }
        case COSVersion::ALPHA64_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Alpha64 Processor)"));
            break;
        }
        case COSVersion::MSIL_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (MSIL Processor)"));
            break;
        }
        case COSVersion::ARM_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (ARM Processor)"));
            break;
        }
        case COSVersion::SHX_PROCESSOR: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (SHX Processor)"));
            break;
        }
        case COSVersion::UNKNOWN_PROCESSOR: //deliberate fallthrough
        default: {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Unknown Processor)"));
            break;
        }
        }
#endif
        ::_stprintf_s(sBuf.data(), sBuf.size(), _T(" v%d."), static_cast<int>((osvi.dwUnderlyingMajorVersion)));
        ::_tcscat_s(sText.data(), sText.size(), sBuf.data());
        if (osvi.dwUnderlyingMinorVersion % 10)
        {
            if (osvi.dwUnderlyingMinorVersion > 9)
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("%02d"), static_cast<int>((osvi.dwUnderlyingMinorVersion)));
            }
            else
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("%01d"), static_cast<int>((osvi.dwUnderlyingMinorVersion)));
            }
        }
        else
        {
            ::_stprintf_s(sBuf.data(), sBuf.size(), _T("%01d"), static_cast<int>((osvi.dwUnderlyingMinorVersion / 10)));
        }
        ::_tcscat_s(sText.data(), sText.size(), sBuf.data());
        if (osvi.dwUnderlyingBuildNumber)
        {
            ::_stprintf_s(sBuf.data(), sBuf.size(), _T(" Build:%d"), static_cast<int>((osvi.dwUnderlyingBuildNumber)));
            ::_tcscat_s(sText.data(), sText.size(), sBuf.data());
        }
        ::_tcscat_s(sText.data(), sText.size(), _T("\n"));
        if (osvi.wUnderlyingServicePackMajor)
        {
            if (osvi.wUnderlyingServicePackMinor)
            {
                // Handle the special case of Windows NT4 SP6a which Dtwinver treats as SP 6.1
                if (os.IsNTPreWin2k(&osvi, TRUE) && (osvi.wUnderlyingServicePackMajor == 6) &&
                    (osvi.wUnderlyingServicePackMinor == 1))
                {
                    ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Service Pack: 6a"));
                }
                // Handle the special case of Windows XP SP1a which Dtwinver treats as SP 1.1
                else if (os.IsWindowsXP(&osvi, TRUE) && (osvi.wUnderlyingServicePackMajor == 1) &&
                         (osvi.wUnderlyingServicePackMinor == 1))
                {
                    ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Service Pack: 1a"));
                }
                else
                {
                    ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Service Pack:%d.%d"), static_cast<int>((osvi.wUnderlyingServicePackMajor)), static_cast<int>((osvi.wUnderlyingServicePackMinor)));
                }
            }
            else
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Service Pack:%d"), static_cast<int>((osvi.wUnderlyingServicePackMajor)));
            }
            ::_tcscat_s(sText.data(), sText.size(), sBuf.data());
        }
        else
        {
            if (osvi.wUnderlyingServicePackMinor)
            {
                ::_stprintf_s(sBuf.data(), sBuf.size(), _T("Service Pack:0.%d"), static_cast<int>((osvi.wUnderlyingServicePackMinor)));
            }
        }
        if (os.IsEnterpriseStorageServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Storage Server Enterprise)"));
        }
        else if (os.IsExpressStorageServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Storage Server Express)"));
        }
        else if (os.IsStandardStorageServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Storage Server Standard)"));
        }
        else if (os.IsWorkgroupStorageServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Storage Server Workgroup)"));
        }
        else if (os.IsEssentialsStorageServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Storage Server Essentials)"));
        }
        else if (os.IsStorageServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Storage Server)"));
        }

        if (os.IsHomeServerPremiumEdition(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Home Server Premium Edition)"));
        }
        else if (os.IsHomeServerEdition(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Home Server Edition)"));
        }

        if (os.IsTerminalServices(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Terminal Services)"));
        }
        if (os.IsEmbedded(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Embedded)"));
        }
        if (os.IsTerminalServicesInRemoteAdminMode(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Terminal Services in Remote Admin Mode)"));
        }
        if (os.Is64Bit(&osvi, TRUE))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (64 Bit Edition)"));
        }
        if (os.IsMediaCenter(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Media Center Edition)"));
        }
        if (os.IsTabletPC(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Tablet PC Edition)"));
        }
        if (os.IsComputeClusterServerEdition(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Compute Cluster Edition)"));
        }
        if (os.IsServerFoundation(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Foundation Edition)"));
        }
        if (os.IsMultipointServerPremiumEdition(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (MultiPoint Premium Edition)"));
        }
        else if (os.IsMultiPointServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (MultiPoint Edition)"));
        }
        if (os.IsSecurityAppliance(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Security Appliance)"));
        }
        if (os.IsBackOffice(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (BackOffice)"));
        }
        if (os.IsNEdition(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (N Edition)"));
        }
        if (os.IsEEdition(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (E Edition)"));
        }
        if (os.IsHyperVTools(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Hyper-V Tools)"));
        }
        if (os.IsHyperVServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Hyper-V Server)"));
        }
        if (os.IsServerCore(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Server Core)"));
        }
        if (os.IsUniprocessorFree(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Uniprocessor Free)"));
        }
        if (os.IsUniprocessorChecked(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Uniprocessor Checked)"));
        }
        if (os.IsMultiprocessorFree(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Multiprocessor Free)"));
        }
        if (os.IsMultiprocessorChecked(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Multiprocessor Checked)"));
        }
        if (os.IsEssentialBusinessServerManagement(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Windows Essential Business Server Manangement Server)"));
        }
        if (os.IsEssentialBusinessServerMessaging(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Windows Essential Business Server Messaging Server)"));
        }
        if (os.IsEssentialBusinessServerSecurity(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Windows Essential Business Server Security Server)"));
        }
        if (os.IsClusterServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Cluster Server)"));
        }
        if (os.IsSmallBusiness(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Small Business Server)"));
        }
        if (os.IsSmallBusinessServerPremium(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Small Business Server Premium)"));
        }
        if (os.IsPreRelease(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Prerelease)"));
        }
        if (os.IsEvaluation(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Evaluation)"));
        }
        if (os.IsAutomotive(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Automotive)"));
        }
        if (os.IsChina(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (China)"));
        }
        if (os.IsSingleLanguage(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Single Language)"));
        }
        if (os.IsWin32sInstalled(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Win32s)"));
        }
        if (os.IsEducation(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Education)"));
        }
        if (os.IsIndustry(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Industry)"));
        }
        if (os.IsStudent(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Student)"));
        }
        if (os.IsMobile(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Mobile)"));
        }
        if (os.IsIoTOS(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (IoT Core)"));
        }
        if (os.IsCloudHostInfrastructureServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Cloud Host Infrastructure Server)"));
        }
        if (os.IsSEdition(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (S Edition)"));
        }
        if (os.IsCloudStorageServer(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Cloud Storage Server)"));
        }
        if (os.IsPPIPro(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (PPI Pro)"));
        }
        if (os.IsConnectedCar(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Connected Car)"));
        }
        if (os.IsHandheld(&osvi))
        {
            ::_tcscat_s(sText.data(), sText.size(), _T(", (Handheld)"));
        }

        ::_tcscat_s(sText.data(), sText.size(), _T("\n"));
    }
    else
    {
        ::_stprintf_s(sText.data(), sText.size(), _T("Failed in call to GetOSVersion\n"));
    }

    DWORD Size = 256U + 1;
    ::strcpy_s(mytext, 1024, "");
    std::array<char, 256 + 1> ComputerName{};
    if (::GetComputerName(ComputerName.data(), &Size) != 0)
    {
        ::strcat_s(mytext, 1024, "\nComputerName : ");
        ::strcat_s(mytext, 1024, ComputerName.data());
    }
    ///////////////////////////////

    std::array<char, 255U> name;
    char* IP{ nullptr };
    if (settings->getIPV6())
    {
        LPSOCKADDR sockaddr_ip;
        std::array<char, 46U> ipstringbuffer;
        DWORD ipbufferlength{ 46U };
        bool IsIpv4{ false };
        bool IsIpv6{ false };
        struct addrinfo hint;
        struct addrinfo* serverinfo = nullptr;
        std::memset(&hint, 0, sizeof(hint));
        hint.ai_family = AF_UNSPEC;
        hint.ai_socktype = SOCK_STREAM;
        hint.ai_protocol = IPPROTO_TCP;
        struct sockaddr_in6* pIpv6Addr;
        struct sockaddr_in* pIpv4Addr;
        struct sockaddr_in6 Ipv6Addr;
        struct sockaddr_in Ipv4Addr;
        std::memset(&Ipv6Addr, 0, sizeof(Ipv6Addr));
        std::memset(&Ipv4Addr, 0, sizeof(Ipv4Addr));

        if (getaddrinfo(name.data(), nullptr, &hint, &serverinfo) == 0)
        {
            struct addrinfo* p;
            for (p = serverinfo; p != nullptr; p = p->ai_next)
            {
                switch (p->ai_family)
                {
                case AF_INET:
                    IsIpv4 = true;
                    pIpv4Addr = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
                    std::memcpy(&Ipv4Addr, pIpv4Addr, sizeof(Ipv4Addr));
                    Ipv4Addr.sin_family = AF_INET;
                    break;
                case AF_INET6:
                    IsIpv6 = true;
                    pIpv6Addr = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
                    std::memcpy(&Ipv6Addr, pIpv6Addr, sizeof(Ipv6Addr));
                    Ipv6Addr.sin6_family = AF_INET6;
                    sockaddr_ip = reinterpret_cast<LPSOCKADDR>(p->ai_addr);
                    ipbufferlength = 46;
                    std::memset(ipstringbuffer.data(), 0, 46);
                    ::WSAAddressToString(sockaddr_ip, static_cast<DWORD>(p->ai_addrlen), nullptr, ipstringbuffer.data(),
                                       &ipbufferlength);
                    break;
                default:
                    break;
                }
            }
        }
        freeaddrinfo(serverinfo);

        if (IsIpv6 && IsIpv4)
        {
            std::array<char, 256U> szText;
            ::sprintf_s(szText.data(), szText.size(), "IPv4: %s\nIPv6: %s \n", inet_ntoa(Ipv4Addr.sin_addr), ipstringbuffer.data());
            ::strcat_s(mytext, 1024, szText.data());
        }
        else if (IsIpv6)
        {
            std::array<char, 256U> szText;
            ::sprintf_s(szText.data(), szText.size(), "IPv6: %s \n", ipstringbuffer.data());
            ::strcat_s(mytext, 1024, szText.data());
        }
        else if (IsIpv4)
        {
            std::array<char, 256U> szText;
            ::sprintf_s(szText.data(), szText.size(), "IPv4: %s \n", inet_ntoa(Ipv4Addr.sin_addr));
            ::strcat_s(mytext, 1024, szText.data());
        }
    }
    else
    {
        PHOSTENT hostinfo;
        if (gethostname(name.data(), sizeof(name)) == 0)
        {
            if ((hostinfo = gethostbyname(name.data())) != nullptr)
            {
                IP = inet_ntoa(*reinterpret_cast<struct in_addr*>(*hostinfo->h_addr_list));
            }
        }
        if (IP)
        {
            ::strcat_s(mytext, 1024, "\nIP : ");
            ::strcat_s(mytext, 1024, IP);
        }
    }
    //////////////////////////////////////
    ::strcat_s(mytext, 1024, "\n");
    ::strcat_s(mytext, 1024, sText.data());

    return 0;
}
