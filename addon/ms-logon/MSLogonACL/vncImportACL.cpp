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


#include "vncImportACL.h"

#include <tchar.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <wchar.h>

vncImportACL::vncImportACL()
{
    lastAllowACE = nullptr;
    lastDenyACE = nullptr;
}

vncImportACL::~vncImportACL()
{
    _ftprintf(stderr, _T("deleting ACE_DATA linked lists\n"));
    ACE_DATA* tmp = nullptr;
    for (ACE_DATA* i = lastDenyACE; i != nullptr; i = tmp)
    {
        if (i->pSID)
        {
            delete i->pSID;
        }
        tmp = i->next;
        delete i;
    }
    for (ACE_DATA* j = lastAllowACE; j != nullptr; j = tmp)
    {
        if (j->pSID)
        {
            delete j->pSID;
        }
        tmp = j->next;
        delete j;
    }
}

void vncImportACL::GetOldACL()
{
    PACL pOldACL = nullptr;
    ACL_SIZE_INFORMATION asi;
    HKEY hk = nullptr;
    DWORD dwValueLength = SECURITY_DESCRIPTOR_MIN_LENGTH;

    __try
    {

        if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\ORL\\WinVNC3"), 0, KEY_QUERY_VALUE, &hk))
        {
            __leave;
        }
        // Read the ACL value from the VNC registry key
        // First call to RegQueryValueEx just gets the buffer length.
        if (ERROR_SUCCESS != RegQueryValueEx(hk,        // subkey handle
                                             _T("ACL"), // value name
                                             0,         // must be zero
                                             0,         // value type , not needed here
                                             nullptr,   //
                                             &dwValueLength))
        { // length of value data
            __leave;
        }
        pOldACL = (PACL)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, dwValueLength);

        if (ERROR_SUCCESS != RegQueryValueEx(hk,        // subkey handle
                                             _T("ACL"), // value name
                                             0,         // must be zero
                                             0,         // value type
                                             (LPBYTE)pOldACL, &dwValueLength))
        { // length of value data
            __leave;
        }
    }
    __finally
    {
        if (hk)
        {
            ::RegCloseKey(hk);
        }
    }

    if (pOldACL == nullptr)
    {
        return;
    }

    if (!::GetAclInformation(pOldACL, &asi, sizeof asi, AclSizeInformation))
    {
        _ftprintf(stderr, _T("GAI(): gle == %lu\n"), ::GetLastError());
        return;
    }

    for (DWORD i{ 0U }; i < asi.AceCount; ++i)
    {
        ReadAce(i, pOldACL);
    }

    ::HeapFree(::GetProcessHeap(), 0, pOldACL);
}

void vncImportACL::ReadAce(const int index, const PACL pACL)
{
    ACE_HEADER* ace;
    //char *type;
    //PSID psid;
    ACE_DATA* pACEdata;

    if (!::GetAce(pACL, index, (void**)&ace))
    {
        _ftprintf(stderr, _T("DACL, entry %d: GetAce() failed, gle == %lu\n"), index, ::GetLastError());
        return;
    }

    pACEdata = new ACE_DATA;
    switch (ace->AceType)
    {
    case ACCESS_ALLOWED_ACE_TYPE:
        pACEdata->type = ACCESS_ALLOWED;
        pACEdata->pSID = CopySID(&((ACCESS_ALLOWED_ACE*)ace)->SidStart);
        pACEdata->mask = ((ACCESS_ALLOWED_ACE*)ace)->Mask;
        pACEdata->next = lastAllowACE;
        lastAllowACE = pACEdata;
        break;
    case ACCESS_DENIED_ACE_TYPE:
        pACEdata->type = ACCESS_DENIED;
        pACEdata->pSID = CopySID(&((ACCESS_DENIED_ACE*)ace)->SidStart);
        pACEdata->mask = ((ACCESS_DENIED_ACE*)ace)->Mask;
        pACEdata->next = lastDenyACE;
        lastDenyACE = pACEdata;
        break;
    default:
        _ftprintf(stderr, _T("Error: Unknown ACE type\n"));
        delete pACEdata;
        break;
    }
}

PSID vncImportACL::CopySID(const PSID pSID)
{
    PSID pNewSID = nullptr;
    DWORD sidLength{ 0U };

    if (::IsValidSid(pSID))
    {
        sidLength = ::GetLengthSid(pSID);
        pNewSID = (PSID) new char[sidLength];
        ::CopySid(sidLength, pNewSID, pSID);
    }
    return pNewSID;
}

int vncImportACL::ScanInput()
{
    int rc{ 0 };
    vncScanInput scanInput;

    do
    {
        std::array<TCHAR, MAXLEN> linebuf;
        TCHAR* line = linebuf.data();
        std::array<TCHAR*, 3U> token;
        std::array<TCHAR, MAXLEN> accesstype;
        std::array<TCHAR, MAXLEN> accessmaskstring;
        std::array<TCHAR, MAXLEN> domainaccount;
        DWORD accessmask{ 0U };
        token[0U] = accesstype.data();
        token[1U] = accessmaskstring.data();
        token[2U] = domainaccount.data();

        if (!scanInput.GetLine(line))
        {
            rc |= 1;
            break;
        }
        scanInput.RemoveNewline(line);
        scanInput.RemoveComment(line);
        if (scanInput.isEmptyLine(line))
        {
            continue;
        }
        if (scanInput.Tokenize(line, token.data()) && (_stscanf(accessmaskstring.data(), _T("%x"), &accessmask) == 1))
        {
            FillAceData(accesstype.data(), accessmask, domainaccount.data());
        }
        else
        {
            _tprintf(_T("Error tokenizing line\n"));
            rc |= 2;
        }
    } while (!std::feof(stdin));
    return rc;
}

PACL vncImportACL::BuildACL()
{
    PACL pACL = nullptr;
    // Need canonical order and normalization??
    // Solution canonical order: aceAllowStart and aceDenyStart?
    // Solution normalization: Check for multiple occurrance of SID
    // in allow list and deny list and merge them.
    long aclSize{ 8 }; // For ACL header

    for (ACE_DATA* i = lastDenyACE; i != nullptr; i = i->next)
    {
        aclSize += ::GetLengthSid(i->pSID) + sizeof(ACCESS_DENIED_ACE) - sizeof(DWORD);
    }
    for (ACE_DATA* j = lastAllowACE; j != nullptr; j = j->next)
    {
        aclSize += ::GetLengthSid(j->pSID) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD);
    }

    pACL = (PACL)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, aclSize);

    // Initialize the new ACL.
    if (::InitializeAcl(pACL, aclSize, ACL_REVISION))
    {
        // Add the access-denied ACEs to the ACL.
        for (ACE_DATA* i = lastDenyACE; i != nullptr; i = i->next)
        {
            //Todo: add error handling.
            ::AddAccessDeniedAce(pACL, ACL_REVISION, i->mask, i->pSID);
        }
        for (ACE_DATA* j = lastAllowACE; j != nullptr; j = j->next)
        {
            //Todo: add error handling.
            ::AddAccessAllowedAce(pACL, ACL_REVISION, j->mask, j->pSID);
        }
    }
    return pACL;
}

bool vncImportACL::SetACL(const PACL pACL)
{
    HKEY hk = nullptr;
    bool isSaveOK{ false };
    DWORD dwDisposition;

    ACL_SIZE_INFORMATION AclInformation{ 0, 0, 0 };
    DWORD nAclInformationLength = sizeof(AclInformation);

    // Todo: Better error handling
    if (pACL)
    {
        ::GetAclInformation(pACL, &AclInformation, nAclInformationLength, AclSizeInformation);
    }

    __try
    {
        //if (ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE,
        //	_T("Software\\ORL\\WinVNC3"),
        //	0, KEY_SET_VALUE, &hk )){
        //	_ftprintf(stderr, _T("Error %d: RegOpenKeyEx\n"), GetLastError());
        //	__leave;
        //}

        if (ERROR_SUCCESS != ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\ORL\\WinVNC3"), 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                                            &hk, &dwDisposition))
        {
            _ftprintf(stderr, _T("Error %d: RegOpenKeyEx\n"), ::GetLastError());
            __leave;
        }

        // Add the ACL value to the VNC registry key
        if (ERROR_SUCCESS != ::RegSetValueEx(hk, _T("ACL"), 0, REG_BINARY, (LPBYTE)pACL, AclInformation.AclBytesInUse))
        {
            _ftprintf(stderr, _T("Error %d: RegSetValueEx\n"), ::GetLastError());
            __leave;
        }
        isSaveOK = true;
        _ftprintf(stderr, _T("RegSetValueEx passed\n"));
    }
    __finally
    {
        if (hk)
        {
            ::RegCloseKey(hk);
        }
    }
    return isSaveOK;
}

bool vncImportACL::FillAceData(const TCHAR* type, DWORD mask, const TCHAR* const user)
{
    ACE_TYPE acetype;
    TCHAR* domainaccount = nullptr;
    PSID pSID = nullptr;

    //type
    if (::_tcsicmp(type, _T("allow")) == 0)
    {
        acetype = ACCESS_ALLOWED;
    }
    else if (::_tcsicmp(type, _T("deny")) == 0)
    {
        acetype = ACCESS_DENIED;
    }
    else
    {
        _ftprintf(stderr, _T("error with ACE type, neither 'allow' nor 'deny'\n"));
        return false;
    }

    //mask
    mask &= ALL_RIGHTS;

    // if name starts with ".\", replace the dot with the computername.
    // if name starts with "..\", replace the dots with the domainname.
    if (_tcsncmp(user, _T(".\\"), 2) == 0)
    {
        domainaccount = AddComputername(user);
    }
    else if (_tcsncmp(user, _T("..\\"), 2) == 0)
    {
        domainaccount = AddDomainname(user);
    }
    else
    {
        domainaccount = new TCHAR[::_tcslen(user) + 1];
        _tcscpy(domainaccount, user);
    }
    pSID = GetSID(domainaccount);
    if (!pSID)
    {
        return false;
    }
    _tprintf(_T("account: %s, mask: %x, type: %s\n"), domainaccount, mask, acetype == ACCESS_ALLOWED ? _T("allow") : _T("deny"));
    ACE_DATA* ace = new ACE_DATA;
    ace->type = acetype;
    ace->mask = mask;
    ace->pSID = pSID;
    switch (acetype)
    {
    case ACCESS_ALLOWED:
        ace->next = lastAllowACE;
        lastAllowACE = ace;
        break;
    case ACCESS_DENIED:
        ace->next = lastDenyACE;
        lastDenyACE = ace;
        break;
    default:
        break;
    }

    delete[] domainaccount;
    domainaccount = nullptr;

    return true; // Needs better error signalling.
}

PSID vncImportACL::GetSID(const TCHAR* const domainaccount)
{
    PSID pSid = nullptr;
    unsigned long ulDomLen{ 0U };
    unsigned long ulSidLen{ 0U };
    SID_NAME_USE peUse;
    TCHAR* domain = nullptr;

    // Get accounts's SID
    // First call to LookupAccountName is expected to fail.
    // Sets pSid and domain size
    ::LookupAccountName(nullptr, domainaccount, pSid, &ulSidLen, domain, &ulDomLen, &peUse);
    pSid = (PSID) new TCHAR[ulSidLen];
    domain = new TCHAR[ulDomLen];
    ::LookupAccountName(nullptr, domainaccount, pSid, &ulSidLen, domain, &ulDomLen, &peUse);
    if (!::IsValidSid(pSid))
    {
        _ftprintf(stderr, _T("%s: SID not valid.\n"), domainaccount);
        delete[] pSid;
        pSid = nullptr;
    }
    delete[] domain;

    return pSid;
}

// SplitString splits a string 'input' on the first occurence of char 'separator'
// into string 'head' and 'tail' (removing the separator).
// If separator is not found, head = "" and tail = input.
const TCHAR* vncImportACL::SplitString(const TCHAR* const input, const TCHAR separator, TCHAR* const head)
{
    const TCHAR* tail;
    std::size_t l;

    tail = ::_tcschr(input, separator);
    if (tail)
    {
        l = tail - input;
        // get rid of separator
        tail = tail + 1;
        _tcsncpy(head, input, l);
        head[l] = _T('\0');
    }
    else
    {
        tail = input;
        head[0U] = _T('\0');
    }
    return tail;
}

TCHAR* vncImportACL::AddComputername(const TCHAR* const user)
{
    unsigned long buflen = MAXLEN;
    std::array<TCHAR, MAXLEN> computername;
    if (!::GetComputerName(computername.data(), &buflen))
    {
        _ftprintf(stderr, _T("GetComputerName error %i\n"), ::GetLastError());
        return nullptr;
    }
    _ftprintf(stderr, _T("Detected computername = %s\n"), computername.data());
    // Length of computername and user minus beginning dot plus terminating '\0'.
    TCHAR* domainaccount = new TCHAR[::_tcslen(computername.data()) + ::_tcslen(user)];
    _tcscpy(domainaccount, computername.data());
    _tcscat(domainaccount, user + 1);

    return domainaccount;
}

TCHAR* vncImportACL::AddDomainname(const TCHAR* const user)
{
    unsigned const long buflen = MAXLEN;
    std::array<TCHAR, MAXLEN * sizeof(wchar_t)> domain;
    LPWKSTA_INFO_100 wkstainfo = nullptr;
    NET_API_STATUS nStatus;

    nStatus = ::NetWkstaGetInfo(0, 100, (LPBYTE*)&wkstainfo);
    if (nStatus == NERR_Success)
    {
        _tcsncpy(domain.data(), wkstainfo->wki100_langroup, MAXLEN);
    }
    else
    {
        _ftprintf(stderr, _T("NetWkstaGetInfo() returned %lu \n"), nStatus);
    }
    domain[MAXLEN - 1] = _T('\0');
    _ftprintf(stderr, _T("Detected domain = %s\n"), domain.data());
    NetApiBufferFree(wkstainfo);
    wkstainfo = nullptr;

    // Length of domainname and user minus beginning dots plus terminating '\0'.
    TCHAR* domainaccount = new TCHAR[::_tcslen(domain.data()) + ::_tcslen(user) - 1];
    _tcscpy(domainaccount, domain.data());
    _tcscat(domainaccount, user + 2);

    return domainaccount;
}

// vncScanInput struct

bool vncScanInput::GetLine(TCHAR* const line)
{
    line[0U] = _T('\0');
    _fgetts(line, MAXLEN, stdin);
    return ferror(stdin) ? false : true;
}

bool vncScanInput::isEmptyLine(TCHAR* line)
{
    for (unsigned int i{ 0U }; i < ::_tcslen(line); ++i)
    {
        if ((line[i] != _T(' ')) && (line[i] != _T('\t')))
        {
            return false;
        }
    }
    return true;
}

void vncScanInput::RemoveComment(TCHAR* const line)
{
    if (TCHAR* comment = ::_tcschr(line, _T(';')))
    {
        *comment = _T('\0');
    }
}

void vncScanInput::RemoveNewline(TCHAR* const line)
{
    if (line[::_tcslen(line) - 1] == _T('\n'))
    {
        line[::_tcslen(line) - 1] = _T('\0');
    }
}

bool vncScanInput::Tokenize(const TCHAR* line, TCHAR** const token)
{
    int tokenCount{ 0 };

    // Loop until the end of line or until 4th token found
    for (; (::_tcslen(line) > 0) && (tokenCount < 4);)
    {
        int len{ 0 };
        switch (*line)
        {
        case _T(' '):
        case _T('\t'):
            ++line; // Eat whitespace
            break;
        case _T('"'):
            ++line; // Eat opening quote
            len = GetQuoteLength(line);
            tokenCount = AddToken(token, tokenCount, &line, len);
            ++line; // Eat closing quote
            break;
        default:
            len = (int)GetWordLength(line);
            tokenCount = AddToken(token, tokenCount, &line, len);
            break;
        }
    }
    return ((tokenCount == 3) && (::_tcslen(line) == 0)) ? true : false;
}

std::size_t vncScanInput::GetWordLength(const TCHAR* const line)
{
    std::size_t length{ 0U };
    std::size_t blength{ 0U };
    std::size_t tlength{ 0U };

    const TCHAR* nextblank = ::_tcschr(line, _T(' '));
    const TCHAR* nexttab = ::_tcschr(line, _T('\t'));
    if (nextblank)
    {
        blength = nextblank - line;
    }
    if (nexttab)
    {
        tlength = nexttab - line;
    }
    if (nextblank && nexttab)
    { // Found blank and tab
        length = __min(blength, tlength);
    }
    else if (nextblank)
    { // Found only a blank
        length = blength;
    }
    else if (nexttab)
    { // Found only a tab
        length = tlength;
    }
    if (!length)
    { // Found neither blank nor tab
        length = ::_tcslen(line);
    }
    return length;
}

int vncScanInput::GetQuoteLength(const TCHAR* const line)
{
    const TCHAR* eoq = ::_tcschr(line, _T('"'));
    std::size_t len{ 0U };
    if (eoq)
    {
        len = eoq - line;
    }
    return static_cast<int>(len);
}

int vncScanInput::AddToken(TCHAR** const token, int tokenCount, const TCHAR** line, const int len)
{
    if (len)
    {
        if (tokenCount < 3)
        {
            _tcsncpy(token[tokenCount], *line, len);
            token[tokenCount][len] = _T('\0');
        }
        *line += len; // Eat token
        ++tokenCount;
    }
    return tokenCount;
}