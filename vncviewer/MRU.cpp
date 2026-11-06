// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


//
// MRU maintains a list of 'Most Recently Used' strings in the registry
//

#include "MRU.h"

#include "VNCOptions.h"

#include <direct.h>
#include <shlobj.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <array>

static const TCHAR* INDEX_VAL_NAME = _T("index");
static const TCHAR* RESERVED_CHARS =
    _T("[;"); //String of characters that will cause a key/value line to be parsed differently if set as a key
static const TCHAR FIRST_USEABLE_ID = _T('A');
static const TCHAR LAST_USEABLE_ID = _T('~');
static const int MRU_MAX_ITEM_LENGTH{ 256 };

MRU::MRU([[maybe_unused]] const LPTSTR keyname, unsigned int maxnum)
{
    VNCOptions::setDefaultOptionsFileName(m_optionfile);
    m_maxnum = maxnum;

    // Read the index entry
    ReadIndex();
}

// Add the item specified at the front of the list
// Move it there if not already. If this makes the
// list longer than the maximum, older ones are deleted.

void ofnInit();
void MRU::AddItem(const LPTSTR txt)
{
    // We don't add empty items.
    if (::_tcslen(txt) == 0)
    {
        return;
    }
    // Read each value in index,
    // noting which is the first unused id
    TCHAR id = _T('\0');
    TCHAR firstUnusedId = FIRST_USEABLE_ID;
    std::array<TCHAR, MRU_MAX_ITEM_LENGTH + 1> itembuf;

    // Scan through the index
    for (int i{ 0 }; i < static_cast<int>(::_tcslen(m_index.data())); ++i)
    {
        id = m_index[i];
        // Does this entry already contain the item we're adding
        if (GetItem(i, itembuf.data(), MRU_MAX_ITEM_LENGTH) != 0)
        {
            // If a value matches the txt specified, move it to the front.
            if (::_tcscmp(itembuf.data(), txt) == 0)
            {
                for (int j = i; j > 0; --j)
                {
                    m_index[j] = m_index[j - 1];
                }
                m_index[0U] = id;
                WriteIndex();
                // That's all we need to do.
                return;
            }
        }
    }

    // Find first available unused id
    while (((::_tcschr(m_index.data(), _totupper(firstUnusedId)) != nullptr) || (::_tcschr(RESERVED_CHARS, firstUnusedId) != nullptr)) &&
           (::_tcscmp(&firstUnusedId, &LAST_USEABLE_ID) <= 0))
    {
        ++firstUnusedId;
    }
    // If we've run out of unused ids, use the last one in the index and then remove it from the end.
    if (::_tcscmp(&firstUnusedId, &LAST_USEABLE_ID) > 0)
    {
        firstUnusedId = id;
        if (::_tcslen(m_index.data()) < 1)
        {
            return;
        }
        m_index[::_tcslen(m_index.data()) - 1] = _T('\0');
    }

    // If we haven't found a match, then we need to create a new entry and put it
    // at the front of the index.
    std::array<TCHAR, 2U> valname;
    valname[0U] = firstUnusedId;
    valname[1U] = _T('\0');
    ::WritePrivateProfileStringW(L"connection", valname.data(), txt, m_optionfile);
    // move all the current ids up one
    for (int j = static_cast<int>(::_tcslen(m_index.data()) + 1); j >= 0; --j)
    {
        m_index[j] = m_index[j - 1];
    }
    m_index[MRU_MAX_ITEM_LENGTH] = _T('\0');
    // and insert this one at the front
    m_index[0U] = firstUnusedId;
    WriteIndex();
}

// How many items are on the list?
int MRU::NumItems()
{
    // return the length of index
    return static_cast<int>(::_tcslen(m_index.data()));
}

// Return them in order. 0 is the newest.
// NumItems()-1 is the oldest.
// Returns length, or 0 if unsuccessful.
int MRU::GetItem(const int index, const LPTSTR buf, int buflen)
{
    if (index > NumItems() - 1)
    {
        return 0;
    }
    std::array<TCHAR, 2U> valname;
    valname[0U] = m_index[index];
    valname[1U] = _T('\0');
    [[maybe_unused]] const DWORD dwbuflen = buflen;
    ::GetPrivateProfileStringW(L"connection", valname.data(), L"", buf, buflen, m_optionfile);
    return static_cast<int>(::_tcslen(buf));
}

// Remove the specified item if it exists.
// Only one copy will be removed, but nothing should occur more than once.
void MRU::RemoveItem(const LPTSTR txt)
{
    std::array<TCHAR, MRU_MAX_ITEM_LENGTH + 1> itembuf;

    for (int i{ 0 }; i < NumItems(); ++i)
    {
        GetItem(i, itembuf.data(), MRU_MAX_ITEM_LENGTH);
        if (::_tcscmp(itembuf.data(), txt) == 0)
        {
            RemoveItem(i);
            break;
        }
    }
}

void MRU::SetPos(const LPTSTR txt, const int x, const int y, const int w, const int h)
{
    std::array<TCHAR, 32U> buf;
    ::_sntprintf_s(buf.data(), buf.size(), _TRUNCATE, _T("%d"), x);
    ::WritePrivateProfileStringW(txt, L"x", buf.data(), m_optionfile);
    ::_sntprintf_s(buf.data(), buf.size(), _TRUNCATE, _T("%d"), y);
    ::WritePrivateProfileStringW(txt, L"y", buf.data(), m_optionfile);
    ::_sntprintf_s(buf.data(), buf.size(), _TRUNCATE, _T("%d"), w);
    ::WritePrivateProfileStringW(txt, L"w", buf.data(), m_optionfile);
    ::_sntprintf_s(buf.data(), buf.size(), _TRUNCATE, _T("%d"), h);
    ::WritePrivateProfileStringW(txt, L"h", buf.data(), m_optionfile);
}

int MRU::Get_x(const LPTSTR txt)
{
    std::array<TCHAR, 32U> buf{};
    ::GetPrivateProfileStringW(txt, L"x", L"", buf.data(), 32, m_optionfile);
    return _ttoi(buf.data());
}

int MRU::Get_y(const LPTSTR txt)
{
    std::array<TCHAR, 32U> buf{};
    ::GetPrivateProfileStringW(txt, L"y", L"", buf.data(), 32, m_optionfile);
    return _ttoi(buf.data());
}
int MRU::Get_w(const LPTSTR txt)
{
    std::array<TCHAR, 32U> buf{};
    ::GetPrivateProfileStringW(txt, L"w", L"", buf.data(), 32, m_optionfile);
    return _ttoi(buf.data());
}
int MRU::Get_h(const LPTSTR txt)
{
    std::array<TCHAR, 32U> buf{};
    ::GetPrivateProfileStringW(txt, L"h", L"", buf.data(), 32, m_optionfile);
    return _ttoi(buf.data());
}
// Remove the item with the given index.
// If this is greater than NumItems()-1 it will be ignored.
void MRU::RemoveItem(const int index)
{
    if (index > NumItems() - 1)
    {
        return;
    }
    std::array<TCHAR, 2U> valname;
    valname[0U] = m_index[index];
    valname[1U] = _T('\0');
    ::WritePrivateProfileStringW(L"connection", valname.data(), nullptr, m_optionfile);

    for (unsigned int i = index; i <= ::_tcslen(m_index.data()); ++i)
    {
        m_index[i] = m_index[i + 1];
    }
    WriteIndex();
}

// Load the index string from the registry
void MRU::ReadIndex()
{
    // read the index
    DWORD dwindexlen = sizeof(m_index);
    if (::GetPrivateProfileStringW(L"connection", INDEX_VAL_NAME, L"", m_index.data(), dwindexlen, m_optionfile) == 0)
    {
        WriteIndex();
    }
    [[maybe_unused]] const int size = NumItems();
}

// Save the index string to the registry
void MRU::WriteIndex() { ::WritePrivateProfileStringW(L"connection", INDEX_VAL_NAME, m_index.data(), m_optionfile); }


MRU::~MRU() {}
