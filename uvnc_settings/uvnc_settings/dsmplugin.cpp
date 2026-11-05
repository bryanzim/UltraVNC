// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


/*	virtual BOOL IsDSMPluginEnabled();
    virtual void EnableDSMPlugin(BOOL fEnable);
    virtual char* GetDSMPluginName();
    virtual void SetDSMPluginName(char* szDSMPlugin);
    virtual BOOL SetDSMPlugin(BOOL fForceReload);
    virtual CDSMPlugin* GetDSMPluginPointer() { return m_pDSMPlugin;};*/

#include "stdafx.h"

#include "DSMPlugin.h"

#include <memory.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
// Utils
//

BOOL MyStrToken(LPSTR szToken, LPSTR lpString, const int nTokenNum, const char cSep)
{
    int i{ 1 };
    while (i < nTokenNum)
    {
        while (*lpString && (*lpString != cSep) && (*lpString != '\0'))
        {
            ++lpString;
        }
        ++i;
        ++lpString;
    }
    while ((*lpString != cSep) && (*lpString != '\0'))
    {
        *szToken = *lpString;
        ++szToken;
        ++lpString;
    }
    *szToken = '\0';
    if ((!*lpString) || (!*szToken))
    {
        return 0;
    }
    return FALSE;
}



//
//
//
CDSMPlugin::CDSMPlugin()
{
    m_fLoaded = false;
    m_fEnabled = false;
    m_lPassLen = 0;

    m_pTransBuffer = nullptr;
    m_pRestBuffer = nullptr;

    std::snprintf(m_szPluginName.data(), m_szPluginName.size(), "Unknown");
    std::snprintf(m_szPluginVersion.data(), m_szPluginVersion.size(), "0.0.0");
    std::snprintf(m_szPluginDate.data(), m_szPluginDate.size(), "12-12-2002");
    std::snprintf(m_szPluginAuthor.data(), m_szPluginAuthor.size(), "Someone");
    std::snprintf(m_szPluginFileName.data(), m_szPluginFileName.size(), "Plugin.dsm"); // No path, just the filename

    m_hPDll = nullptr;

    // Plugin's functions pointers init
    /*
    STARTUP			m_PStartup = NULL;
    SHUTDOWN		m_PShutdown = NULL;
    SETPARAMS		m_PSetParams = NULL;
    GETPARAMS		m_PGetParams = NULL;
    TRANSFORMBUFFER m_PTransformBuffer = NULL;
    RESTOREBUFFER	m_PRestoreBuffer = NULL;
    TRANSFORMBUFFER	m_PFreeBuffer = NULL;
    RESET			m_PReset = NULL;
    //adzm - 2009-06-21
    CREATEPLUGININTERFACE	m_PCreatePluginInterface = NULL;
    */

    //adzm - 2009-07-05
    // Plugin's functions pointers init
    m_PDescription = nullptr;
    m_PStartup = nullptr;
    m_PShutdown = nullptr;
    m_PSetParams = nullptr;
    m_PGetParams = nullptr;
    m_PTransformBuffer = nullptr;
    m_PRestoreBuffer = nullptr;
    m_PFreeBuffer = nullptr;
    m_PReset = nullptr;
    //adzm - 2009-06-21
    m_PCreatePluginInterface = nullptr;
    //adzm 2010-05-10
    m_PCreateIntegratedPluginInterface = nullptr;
    //adzm 2010-05-12 - dsmplugin config
    m_PConfig = nullptr;
}

//
//
//
CDSMPlugin::~CDSMPlugin()
{
    // TODO: Log events
    if (IsLoaded())
    {
        UnloadPlugin();
    }
}


//
//
//
void CDSMPlugin::SetEnabled(const bool fEnable) { m_fEnabled = fEnable; }


//
//
//
void CDSMPlugin::SetLoaded(const bool fEnable) { m_fLoaded = fEnable; }


//
//
//
char* CDSMPlugin::DescribePlugin(void)
{
    // TODO: Log events
    char* szDescription{ nullptr };
    if (m_PDescription)
    {
        szDescription = (*m_PDescription)();
        if (szDescription != nullptr)
        {
            //adzm 2010-05-10 - this was inconsistent with the way the plugins are written
            MyStrToken(m_szPluginName.data(), szDescription, 1, ',');
            MyStrToken(m_szPluginVersion.data(), szDescription, 2, ',');
            MyStrToken(m_szPluginDate.data(), szDescription, 3, ',');
            MyStrToken(m_szPluginAuthor.data(), szDescription, 4, ',');
            MyStrToken(m_szPluginFileName.data(), szDescription, 5, ',');
        }
        return szDescription;
    }

    else
    {
        return "No Plugin loaded";
    }
}


//
// Init the DSMPlugin system
//
bool CDSMPlugin::InitPlugin(void)
{
    // TODO: Log events
    int nRes = (*m_PStartup)();
    if (nRes < 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

//
// Reset the DSMPlugin
//
bool CDSMPlugin::ResetPlugin(void)
{
    // TODO: Log events
    int nRes = (*m_PReset)();
    if (nRes < 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}


//
// szParams is the key (or password) that is transmitted to the loaded DSMplugin
//
bool CDSMPlugin::SetPluginParams(const HWND hWnd, char* const szParams, char* const szConfig, char** const pszNewConfig)
{
    // TODO: Log events

    //adzm 2010-05-12 - dsmplugin config
    if (m_PConfig)
    {
        char* szNewConfig{ nullptr };
        int nRes = (*m_PConfig)(hWnd, szParams, szConfig, pszNewConfig);
        if (nRes > 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        int nRes = (*m_PSetParams)(hWnd, szParams);
        if (nRes > 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}


//
// Return the loaded DSMplugin current param(s)
//
char* CDSMPlugin::GetPluginParams(void)
{
    //
    return (*m_PGetParams)();
}


//
// List all the plugins is the current APP directory in the given ComboBox
//
int CDSMPlugin::ListPlugins(const HWND hComboBox)
{
    // TODO: Log events
    WIN32_FIND_DATA fd;
    HANDLE ff;
    int fRet{ 1 };
    int nFiles{ 0 };
    std::array<char, MAX_PATH> szCurrentDir;

    if (::GetModuleFileName(nullptr, szCurrentDir.data(), szCurrentDir.size()))
    {
        char* p = std::strrchr(szCurrentDir.data(), '\\');
        if (p == nullptr)
        {
            return 0;
        }
        *p = '\0';
    }
    else
    {
        return 0;
    }
    // MessageBoxSecure(NULL, szCurrentDir, "Current directory", MB_OK);

    if (szCurrentDir[std::strlen(szCurrentDir.data()) - 1] != '\\')
    {
        std::strcat(szCurrentDir.data(), "\\");
    }
    std::strcat(szCurrentDir.data(), "*.dsm"); // The DSMplugin dlls must have this extension

    ff = ::FindFirstFile(szCurrentDir.data(), &fd);
    if (ff == INVALID_HANDLE_VALUE)
    {
        // Todo: Log error here
        return 0;
    }

    while (fRet != 0)
    {
        ::SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)(fd.cFileName));
        ++nFiles;
        fRet = ::FindNextFile(ff, &fd);
    }

    ::FindClose(ff);

    return nFiles;
}



//
// Load the given DSMplugin and map its functions
//
bool CDSMPlugin::LoadPlugin(char* const szPlugin, const bool fAllowMulti)
{
    // sf@2003 - Multi dll trick
    // I really don't like doing this kind of dirty workaround but I have no time to do
    // better for now. Used only by a listening viewer.
    // Create a numbered temporary copy of the original plugin dll and load it (viewer only, for now)
    if (fAllowMulti)
    {
        bool fDllCopyCreated{ false };
        int i{ 1 };
        std::array<char, MAX_PATH> szDllCopyName;
        std::array<char, MAX_PATH> szCurrentDir;
        std::array<char, MAX_PATH> szCurrentDir_szPlugin;
        std::array<char, MAX_PATH> szCurrentDir_szDllCopyName;
        while (!fDllCopyCreated)
        {
            ::strcpy_s(szDllCopyName.data(), 260, szPlugin);
            szDllCopyName[std::strlen(szPlugin) - 4] = '\0'; //remove the ".dsm" extension
            std::snprintf(szDllCopyName.data(), szDllCopyName.size(), "%s-tmp.d%d", szDllCopyName.data(), i++);
            //fDllCopyCreated = (FALSE != CopyFile(szPlugin, szDllCopyName, false));
            // Note: Let's be really dirty; Overwrite if it's possible only (dll not loaded).
            // This way if for some reason (abnormal process termination) the dll wasn't previously
            // normally deleted we overwrite/clean it with the new one at the same time.
            //strcpy (szCurrentDir_szDllCopyName,szDllCopyName);
            //DWORD error=GetLastError();
            //if (error==2)
            {
                if (::GetModuleFileName(nullptr, szCurrentDir.data(), szCurrentDir.size()))
                {
                    char* p = std::strrchr(szCurrentDir.data(), '\\');
                    *p = '\0';
                }
                std::array<char, MAX_PATH> lpPathBuffer;
                DWORD dwBufSize = MAX_PATH;
                DWORD dwRetVal;
                dwRetVal = ::GetTempPath(dwBufSize, lpPathBuffer.data());
                if ((dwRetVal > dwBufSize) || (dwRetVal == 0))
                {
                    std::strcpy(lpPathBuffer.data(), szCurrentDir.data());
                }
                else
                {
                }

                std::strcpy(szCurrentDir_szPlugin.data(), szCurrentDir.data());
                std::strcat(szCurrentDir_szPlugin.data(), "\\");
                std::strcat(szCurrentDir_szPlugin.data(), szPlugin);

                std::strcpy(szCurrentDir_szDllCopyName.data(), lpPathBuffer.data());
                //strcat (szCurrentDir_szDllCopyName,"\\");
                std::strcat(szCurrentDir_szDllCopyName.data(), szDllCopyName.data());
                fDllCopyCreated = (FALSE != ::CopyFile(szCurrentDir_szPlugin.data(), szCurrentDir_szDllCopyName.data(), false));
            }
            if (i > 99)
            {
                break;
            } // Just in case...
        }
        std::strcpy(m_szDllName.data(), szCurrentDir_szDllCopyName.data());
        m_hPDll = ::LoadLibrary(m_szDllName.data());
    }
    else // Use the original plugin dll
    {
        ::ZeroMemory(m_szDllName.data(), std::strlen(m_szDllName.data()));
        m_hPDll = ::LoadLibrary(szPlugin);
        //Try current PATH
        if (m_hPDll == nullptr)
        {
            std::array<char, MAX_PATH> szCurrentDir{};
            std::array<char, MAX_PATH> szCurrentDir_szPlugin{};
            if (::GetModuleFileName(nullptr, szCurrentDir.data(), szCurrentDir.size()))
            {
                char* p = std::strrchr(szCurrentDir.data(), '\\');
                *p = '\0';
            }
            std::strcpy(szCurrentDir_szPlugin.data(), szCurrentDir.data());
            std::strcat(szCurrentDir_szPlugin.data(), "\\");
            std::strcat(szCurrentDir_szPlugin.data(), szPlugin);
            m_hPDll = ::LoadLibrary(szCurrentDir_szPlugin.data());
        }
    }

    if (m_hPDll == nullptr)
    {
        return false;
    }

    m_PDescription = (DESCRIPTION)::GetProcAddress(m_hPDll, "Description");
    m_PStartup = (STARTUP)::GetProcAddress(m_hPDll, "Startup");
    m_PShutdown = (SHUTDOWN)::GetProcAddress(m_hPDll, "Shutdown");
    m_PSetParams = (SETPARAMS)::GetProcAddress(m_hPDll, "SetParams");
    m_PGetParams = (GETPARAMS)::GetProcAddress(m_hPDll, "GetParams");
    m_PTransformBuffer = (TRANSFORMBUFFER)::GetProcAddress(m_hPDll, "TransformBuffer");
    m_PRestoreBuffer = (RESTOREBUFFER)::GetProcAddress(m_hPDll, "RestoreBuffer");
    m_PFreeBuffer = (FREEBUFFER)::GetProcAddress(m_hPDll, "FreeBuffer");
    m_PReset = (RESET)::GetProcAddress(m_hPDll, "Reset");
    //adzm - 2009-06-21
    m_PCreatePluginInterface = (CREATEPLUGININTERFACE)::GetProcAddress(m_hPDll, "CreatePluginInterface");
    //adzm 2010-05-10
    m_PCreateIntegratedPluginInterface =
        (CREATEINTEGRATEDPLUGININTERFACE)::GetProcAddress(m_hPDll, "CreateIntegratedPluginInterface");
    //adzm 2010-05-12 - dsmplugin config
    m_PConfig = (CONFIG)::GetProcAddress(m_hPDll, "Config");

    if ((m_PStartup == nullptr) || (m_PShutdown == nullptr) || (m_PSetParams == nullptr) || (m_PGetParams == nullptr) ||
        (m_PTransformBuffer == nullptr) || (m_PRestoreBuffer == nullptr) || (m_PFreeBuffer == nullptr))
    {
        ::FreeLibrary(m_hPDll);
        if (*m_szDllName.data())
        {
            ::DeleteFile(m_szDllName.data());
        }
        return false;
    }

    // return ((*m_PStartup)());
    SetLoaded(true);
    return true;
}


//
// Unload the current DSMPlugin from memory
//
bool CDSMPlugin::UnloadPlugin(void)
{
    // TODO: Log events
    // Force the DSMplugin to free the buffers it allocated
    if (m_pTransBuffer != nullptr)
    {
        (*m_PFreeBuffer)(m_pTransBuffer);
    }
    if (m_pRestBuffer != nullptr)
    {
        (*m_PFreeBuffer)(m_pRestBuffer);
    }

    m_pTransBuffer = nullptr;
    m_pRestBuffer = nullptr;

    SetLoaded(false);

    if ((*m_PShutdown)())
    {
        bool fFreed{ false };
        fFreed = (FALSE != ::FreeLibrary(m_hPDll));
        if (*m_szDllName.data())
        {
            ::DeleteFile(m_szDllName.data());
        }
        return fFreed;
    }
    else
    {
        return false;
    }
}

//adzm - 2009-06-21
IPlugin* CDSMPlugin::CreatePluginInterface()
{
    if (m_PCreatePluginInterface)
    {
        return m_PCreatePluginInterface();
    }
    else
    {
        return nullptr;
    }
}

bool CDSMPlugin::SupportsMultithreaded()
{
    //adzm 2010-05-10
    return (m_PCreatePluginInterface != nullptr) || (m_PCreateIntegratedPluginInterface != nullptr);
}

//adzm 2010-05-10
IIntegratedPlugin* CDSMPlugin::CreateIntegratedPluginInterface()
{
    if (m_PCreateIntegratedPluginInterface)
    {
        return m_PCreateIntegratedPluginInterface();
    }
    else
    {
        return nullptr;
    }
}

bool CDSMPlugin::SupportsIntegrated() { return m_PCreateIntegratedPluginInterface != nullptr; }


//
// Tell the plugin to do its transformation on the source data buffer
// Return: pointer on the new transformed buffer (allocated by the plugin)
// nTransformedDataLen is the number of bytes contained in the transformed buffer
//
BYTE* CDSMPlugin::TransformBuffer(BYTE* const pDataBuffer, const int nDataLen, int* const pnTransformedDataLen)
{


    m_pTransBuffer = (*m_PTransformBuffer)(pDataBuffer, nDataLen, pnTransformedDataLen);

    return m_pTransBuffer;
}


// - If pRestoredDataBuffer = NULL, the plugin check its local buffer and return the pointer
// - Otherwise, restore data contained in its rest. buffer and put the result in pRestoredDataBuffer
//   pnRestoredDataLen is the number bytes put in pRestoredDataBuffers
BYTE* CDSMPlugin::RestoreBufferStep1(BYTE* const pRestoredDataBuffer, const int nDataLen, int* const pnRestoredDataLen)
{
    //m_RestMutex.lock();
    m_pRestBuffer = (*m_PRestoreBuffer)(pRestoredDataBuffer, nDataLen, pnRestoredDataLen);
    return m_pRestBuffer;
}

BYTE* CDSMPlugin::RestoreBufferStep2(BYTE* const pRestoredDataBuffer, const int nDataLen, int* const pnRestoredDataLen)
{
    m_pRestBuffer = (*m_PRestoreBuffer)(pRestoredDataBuffer, nDataLen, pnRestoredDataLen);
    //m_RestMutex.unlock();
    return nullptr;
}

void CDSMPlugin::RestoreBufferUnlock()
{
    //m_RestMutex.unlock();
}


const char Base64::cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
** Translation Table to decode (created by author)
*/
const char Base64::cd64[] = "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";


void Base64::encodeblock(BYTE in[3U], BYTE out[4U], const int len)
{
    out[0U] = cb64[in[0U] >> 2];
    out[1U] = cb64[((in[0U] & 0x03) << 4) | ((in[1U] & 0xf0) >> 4)];
    out[2U] = static_cast<BYTE>((len > 1 ? cb64[((in[1U] & 0x0f) << 2) | ((in[2U] & 0xc0) >> 6)] : '='));
    out[3U] = static_cast<BYTE>((len > 2 ? cb64[in[2U] & 0x3f] : '='));
}

void Base64::decodeblock(BYTE in[4U], BYTE out[3U])
{
    out[0U] = static_cast<BYTE>((in[0U] << 2 | in[1U] >> 4));
    out[1U] = static_cast<BYTE>((in[1U] << 4 | in[2U] >> 2));
    out[2U] = static_cast<BYTE>((((in[2U] << 6) & 0xc0) | in[3U]));
}

void Base64::encode(const char* const szIn, char* const szOut)
{
    std::array<BYTE, 3U> in;
    std::array<BYTE, 4U> out;
    int i, len, blocksout = 0;

    const char* pIn = szIn;
    char* pOut = szOut;

    while (*pIn != '\0')
    {
        len = 0;
        for (i = 0; i < 3; ++i)
        {
            in[i] = (BYTE)*pIn;
            if (*pIn != '\0')
            {
                ++len;
                ++pIn;
            }
            else
            {
                in[i] = 0;
            }
        }
        if (len)
        {
            encodeblock(in.data(), out.data(), len);
            for (i = 0; i < 4; ++i)
            {
                *pOut = static_cast<char>(out[i]);
                ++pOut;
            }
        }
    }
    *pOut = '\0';
}

void Base64::decode(const char* const szIn, char* const szOut)
{
    BYTE in[4U], out[3U], v, o;
    int i, len;

    const char* pIn = szIn;
    char* pOut = szOut;

    while (*pIn != '\0')
    {
        for (len = 0, i = 0; (i < 4) && (*pIn != '\0'); ++i)
        {
            v = 0;
            o = 0;
            while ((*pIn != '\0') && (v == 0))
            {
                v = (BYTE)*pIn;
                o = v;
                ++pIn;

                v = static_cast<BYTE>((((v < 43) || (v > 122)) ? 0 : cd64[v - 43]));
                if (v)
                {
                    v = static_cast<BYTE>(((v == '$') ? 0 : v - 61));
                }
            }
            if ((o != '\0') && (v != 0))
            {
                ++len;
                //if( v ) {
                in[i] = static_cast<BYTE>((v - 1));
                //}
            }
            else
            {
                in[i] = 0;
            }
        }
        if (len)
        {
            decodeblock(in, out);
            for (i = 0; i < len - 1; ++i)
            {
                *pOut = static_cast<char>(out[i]);
                ++pOut;
            }
        }
    }
    *pOut = '\0';
}


void ConfigHelper::SetConfigHelper(const DWORD dwFlags, char* const szPassphrase)
{
    m_szConfig = new char[512U];
    m_szConfig[0U] = '\0';

    std::array<char, 256U> szEncoded;
    szEncoded[0U] = '\0';
    if (szPassphrase[0U] != '\0')
    {
        Base64::encode(szPassphrase, szEncoded.data());
    }

    ::_snprintf_s(m_szConfig, 512 - 1 - 1, _TRUNCATE, "SecureVNC;0;0x%08x;%s", dwFlags, szEncoded.data());
}

ConfigHelper::ConfigHelper(const char* szConfig)
    : m_szConfig{ nullptr }, m_szPassphrase{ nullptr }, m_dwFlags(0x01 | 0x4000 | 0x00100000)
{
    m_szPassphrase = new char[256U];
    m_szPassphrase[0U] = '\0';

    if (szConfig == nullptr)
    {
        return;
    }

    const char* szHeader = "SecureVNC;0;";

    if (std::strncmp(szConfig, "SecureVNC;0;", std::strlen(szHeader)) != 0)
    {
        return;
    }

    szConfig += std::strlen(szHeader);

    char* szEnd{ nullptr };
    DWORD dwFlags = std::strtoul(szConfig, &szEnd, 16);

    if (dwFlags != ULONG_MAX)
    {
        m_dwFlags = dwFlags;

        if (szEnd && (szEnd != szConfig))
        {
            //m_szPassphrase = new char[256];
            //m_szPassphrase[0] = '\0';

            std::array<char, 256U> szEncoded;
            szEncoded[0U] = '\0';

            ::strcpy_s(szEncoded.data(), 256 - 1, szEnd + 1);

            if (szEncoded[0U] != '\0')
            {
                Base64::decode(szEncoded.data(), m_szPassphrase);
            }
        }
    }
}

ConfigHelper::~ConfigHelper()
{
    if (m_szConfig)
    {
        delete[] m_szConfig;
        m_szConfig = nullptr;
    }

    if (m_szPassphrase)
    {
        delete[] m_szPassphrase;
        m_szPassphrase = nullptr;
    }
}
