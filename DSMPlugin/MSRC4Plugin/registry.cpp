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


//
// REGISTRY class allows editing of strings in the registry
//
#include "registry.h"

#include <cstring>

static const int REGISTRY_MAX_ITEM_LENGTH{ 256 };

REGISTRY::REGISTRY(const HKEY hivename, const LPTSTR keyname, const bool blnWritable)
{
    int rc{ 0 };
    DWORD dispos;

    //PrintLog((DEST,"REGISTRY Opening hive"));

    if (blnWritable == true)
    {
        // Create the registry key. If unsuccessful all other methods will be no-ops.
        rc = ::RegCreateKeyEx(hivename, keyname, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &m_hRegKey, &dispos);
        if (rc != ERROR_SUCCESS)
        {
#ifdef _WITH_LOG
            DebugLog((DEST, "ReadItem failed"));
#endif
            m_hRegKey = nullptr;
        }
    }
    else
    {
        // Create the registry key. If unsuccessful all other methods will be no-ops.
        rc = ::RegCreateKeyEx(hivename, keyname, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &m_hRegKey, &dispos);
        if (rc != ERROR_SUCCESS)
        {
#ifdef _WITH_LOG
            DebugLog((DEST, "ReadItem failed"));
#endif
            m_hRegKey = nullptr;
            blnOpen = false;
        }
        blnOpen = true;
    }
    writable = blnWritable;
}


// Load the index string from the registry
BOOL REGISTRY::ReadItem(const LPSTR keyValue, DWORD dwindexlen, const TCHAR* const indexVal, const TCHAR* const defaultVal)
{
    int rc{ 0 };
    BOOL bRC{ false };

    if (m_hRegKey == nullptr)
    {
        return false;
    }

    // Read the index
    DWORD valtype;
    valtype = REG_SZ;
    //DWORD dwindexlen = sizeof(keyValue);
    rc = RegQueryValueEx(m_hRegKey, indexVal, nullptr, &valtype, (LPBYTE)keyValue, &dwindexlen);
    if (rc == ERROR_SUCCESS)
    {
        return true;
    }
    else
    {
        if (rc == ERROR_MORE_DATA)
        {
#ifdef _WITH_LOG
            DebugLog((DEST, "ReadItem failed - indexVal too small"));
#endif
        }
        else
        {
#ifdef _WITH_LOG
            DebugLog((DEST, "ReadItem - missing key %s", indexVal));
#endif

            // If index entry doesn't exist, create it
            if (writable)
            {
                bRC = WriteItem((char*)defaultVal, indexVal);
                std::strcpy(keyValue, defaultVal);
            }
        }
    }

    return bRC;
}

// Save the index string to the registry
BOOL REGISTRY::WriteItem(const LPSTR keyValue, const TCHAR* const indexVal)
{
    int rc{ 0 };

    if (m_hRegKey == nullptr)
    {
        return false;
    }

    rc = ::RegSetValueEx(m_hRegKey, indexVal, nullptr, REG_SZ, (CONST BYTE*)keyValue, std::strlen(keyValue));

    if (rc == ERROR_SUCCESS)
    {
        return true;
    }
    else
    {
#ifdef _WITH_LOG
        DebugLog((DEST, "WriteItem %s failed", indexVal));
#endif
        return false;
    }
}


REGISTRY::~REGISTRY()
{
    if (m_hRegKey != nullptr)
    {

        //PrintLog((DEST,"~REGISTRY Closing hive"));
        ::RegCloseKey(m_hRegKey);
        m_hRegKey = nullptr;
    }
}
