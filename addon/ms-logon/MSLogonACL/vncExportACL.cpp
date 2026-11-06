// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2004 Martin Scharpf. All Rights Reserved.
// Based on Felix Kasza's DumpACL (http://win32.mvps.org/security/dumpacl.html).
//


#include "vncExportACL.h"

#include <tchar.h>

#include <array>
#include <wchar.h>
// Get ACL from regkey HKLM\Software\ORL\WinVNC3\ACL

bool vncExportACL::GetACL(PACL* pACL)
{
    HKEY hk = nullptr;
    DWORD dwValueLength = SECURITY_DESCRIPTOR_MIN_LENGTH;

    _ftprintf(stderr, _T("== Entering GetACL\n"));
    __try
    {

        if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\ORL\\WinVNC3"), 0, KEY_QUERY_VALUE, &hk))
        {
            _ftprintf(stderr, _T("== Error %d: RegOpenKeyEx\n"), ::GetLastError());
            __leave;
        }
        // Read the ACL value from the VNC registry key
        // First call to RegQueryValueEx just gets the buffer length.
        if (ERROR_SUCCESS != RegQueryValueEx(hk,        // subkey handle
                                             _T("ACL"), // value name
                                             nullptr,         // must be zero
                                             nullptr,         // value type , not needed here
                                             nullptr,   //
                                             &dwValueLength))
        { // length of value data
            _ftprintf(stderr, _T("== Error %d: RegQueryValueEx 1\tValueLength = %d\n"), ::GetLastError(), dwValueLength);
            __leave;
        }
        *pACL = reinterpret_cast<PACL>(::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, dwValueLength));

        if (ERROR_SUCCESS != RegQueryValueEx(hk,        // subkey handle
                                             _T("ACL"), // value name
                                             nullptr,         // must be zero
                                             nullptr,         // value type
                                             reinterpret_cast<LPBYTE>(*pACL), &dwValueLength))
        { // length of value data
            _ftprintf(stderr, _T("== Error %d: RegQueryValueEx 2\n"), ::GetLastError());
            __leave;
        }
        _ftprintf(stderr, _T("== RegQueryValueEx passed\tdwValueLength = %d\n"), dwValueLength);
    }
    __finally
    {
        if (hk)
        {
            ::RegCloseKey(hk);
        }
    }
    return true;
}



const TCHAR* vncExportACL::SidToText(const PSID psid)
{
    // S-rev- + SIA + subauthlen*maxsubauth + terminator
    static std::array<TCHAR, 15 + 12 + 12 * SID_MAX_SUB_AUTHORITIES + 1> buf;
    TCHAR* p = &buf[0U];
    PSID_IDENTIFIER_AUTHORITY psia;
    DWORD numSubAuths, i;

    // Validate the binary SID.

    if (!::IsValidSid(psid))
    {
        return FALSE;
    }

    psia = ::GetSidIdentifierAuthority(psid);

    p = buf.data();
    p += _sntprintf(p, &buf[sizeof buf] - p, _T("S-%lu-"), 0x0f & *(reinterpret_cast<byte*>(psid)));

    if ((psia->Value[0U] != 0) || (psia->Value[1U] != 0))
    {
        p += _sntprintf(p, &buf[sizeof buf] - p, _T("0x%02hx%02hx%02hx%02hx%02hx%02hx"), static_cast<USHORT>(psia->Value[0U]),
                        static_cast<USHORT>(psia->Value[1U]), static_cast<USHORT>(psia->Value[2U]), static_cast<USHORT>(psia->Value[3U]), static_cast<USHORT>(psia->Value[4U]),
                        static_cast<USHORT>(psia->Value[5U]));
    }
    else
    {
        p += _sntprintf(p, &buf[sizeof buf] - p, _T("%lu"),
                        static_cast<ULONG>((psia->Value[5U])) + static_cast<ULONG>((psia->Value[4U] << 8)) +
                            static_cast<ULONG>((psia->Value[3U] << 16)) + static_cast<ULONG>((psia->Value[2U] << 24)));
    }

    // Add SID subauthorities to the string.

    numSubAuths = *GetSidSubAuthorityCount(psid);
    for (i = 0; i < numSubAuths; ++i)
    {
        p += _sntprintf(p, &buf[sizeof buf] - p, _T("-%lu"), *GetSidSubAuthority(psid, i));
    }

    return buf.data();
}

void vncExportACL::PrintSid(const PSID psid)
{
    std::array<TCHAR, 256U> name;
    std::array<TCHAR, 256U> domain;
    DWORD cbname = sizeof name, cbdomain = sizeof domain, rc;
    SID_NAME_USE sidUse;

    //!! next line has hardcoded server name !!
    // NULL server name is usually appropriate, though.
    if (::LookupAccountSid(nullptr, psid, name.data(), &cbname, domain.data(), &cbdomain, &sidUse))
    {
        //Todo: Check if WellKnownSID and reserve special names for them.

        /*		switch ( sidUse )
                {
                    case SidTypeWellKnownGroup:	type = "well-known group"; break;
                    default:					type = "bad sidUse value"; break;
                }
        */
        LPWKSTA_INFO_100 wkstainfo = nullptr;
        NET_API_STATUS nStatus;
        std::array<TCHAR, MAXLEN> langroup;
        std::array<TCHAR, MAXLEN> computername;

        nStatus = ::NetWkstaGetInfo(nullptr, 100, reinterpret_cast<LPBYTE*>(&wkstainfo));
        if (nStatus == NERR_Success)
        {
            _tcsncpy(langroup.data(), wkstainfo->wki100_langroup, MAXLEN);
            _tcsncpy(computername.data(), wkstainfo->wki100_computername, MAXLEN);
            langroup[MAXLEN - 1] = _T('\0');
            computername[MAXLEN - 1] = _T('\0');
            // replace computername with a dot
            if (::_tcsicmp(computername.data(), domain.data()) == 0)
            {
                _tcscpy(domain.data(), _T("."));
            }
            else if (::_tcsicmp(langroup.data(), domain.data()) == 0)
            {
                _tcscpy(domain.data(), _T(".."));
            }
        }
        else
        {
            _tprintf(_T("NetWkstaGetInfo() returned %lu \n"), nStatus);
        }

        NetApiBufferFree(wkstainfo);
        wkstainfo = nullptr;

        // If domain or username contains whitespace, print enclosing quotes
        if (::_tcschr(domain.data(), _T(' ')) || ::_tcschr(name.data(), _T(' ')))
        {
            _tprintf(_T("\"%s%s%s\"\n"), domain.data(), ((domain.data() == nullptr) || (*domain.data() == _T('\0'))) ? _T("") : _T("\\"),
                     name.data());
        }
        else
        {
            _tprintf(_T("%s%s%s\n"), domain.data(), ((domain.data() == nullptr) || (*domain.data() == _T('\0'))) ? _T("") : _T("\\"),
                     name.data());
        }
    }
    else
    {
        rc = ::GetLastError();
        _tprintf(_T("[%s] *** error %lu\n"), SidToText(psid), rc);
    }
}

void vncExportACL::PrintAce(const int index, const PACL acl)
{

    ACE_HEADER* ace;
    TCHAR* type;
    PSID psid;

    if (!::GetAce(acl, static_cast<DWORD>(index), reinterpret_cast<void**>(&ace)))
    {
        _tprintf(_T("DACL, entry %d: GetAce() failed, gle == %lu\n"), index, ::GetLastError());
        return;
    }

    switch (ace->AceType)
    {
    case ACCESS_ALLOWED_ACE_TYPE:
        type = _T("allow");
        psid = &(reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace))->SidStart;
        break;
    case ACCESS_DENIED_ACE_TYPE:
        type = _T("deny");
        psid = &(reinterpret_cast<ACCESS_DENIED_ACE*>(ace))->SidStart;
        break;
    default:
        type = _T("invalid");
        psid = &(reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace))->SidStart;
        break;
    }
    _tprintf(_T("%s\t"), type);

    _tprintf(_T("0x%08lX\t"), (reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace))->Mask);

    PrintSid(psid);
}



void vncExportACL::PrintAcl(const PACL acl)
{
    DWORD i;
    ACL_SIZE_INFORMATION aci;

    if (acl == nullptr)
    {
        return;
    }

    if (!::GetAclInformation(acl, &aci, sizeof aci, AclSizeInformation))
    {
        _tprintf(_T("GAI(): gle == %lu\n"), ::GetLastError());
        return;
    }

    for (i = 0; i < aci.AceCount; ++i)
    {
        PrintAce(static_cast<int>(i), acl);
    }
}
