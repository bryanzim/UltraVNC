// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "stdafx.h"

#include "inifile.h"

#include <direct.h>
#include <shlobj.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <errno.h>

IniFile::IniFile()
{
    std::array<char, MAX_PATH> programdataPath{};
    std::array<char, MAX_PATH> programdataFolder{};
    if (::SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programdataPath.data()) != S_OK)
    {
        return;
    }
    ::strcpy_s(programdataFolder.data(), programdataFolder.size(), programdataPath.data());
    ::strcat_s(programdataFolder.data(), programdataFolder.size(), "\\UltraVNC");
    // Create directory if it doesn't exist
    if ((::_mkdir(programdataFolder.data()) != 0) && (errno != EEXIST))
    {
        // Failed to create directory, fall back to exe folder
        std::array<char, MAX_PATH> WORKDIR;
        if (::GetModuleFileName(nullptr, WORKDIR.data(), WORKDIR.size()))
        {
            char* p = std::strrchr(WORKDIR.data(), '\\');
            if (p != nullptr)
            {
                *p = '\0';
            }
        }
        ::strcpy_s(myInifile.data(), myInifile.size(), WORKDIR.data());
        ::strcat_s(myInifile.data(), myInifile.size(), "\\");
        ::strcat_s(myInifile.data(), myInifile.size(), INIFILE_NAME);
        return;
    }
    ::strcpy_s(myInifile.data(), myInifile.size(), programdataFolder.data());
    ::strcat_s(myInifile.data(), myInifile.size(), "\\");
    ::strcat_s(myInifile.data(), myInifile.size(), INIFILE_NAME);
}

void IniFile::IniFileSetSecure()
{
    std::array<char, MAX_PATH> programdataPath{};
    std::array<char, MAX_PATH> programdataFolder{};
    if (::SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programdataPath.data()) != S_OK)
    {
        return;
    }
    ::strcpy_s(programdataFolder.data(), programdataFolder.size(), programdataPath.data());
    ::strcat_s(programdataFolder.data(), programdataFolder.size(), "\\UltraVNC");
    if ((::_mkdir(programdataFolder.data()) != 0) && (errno != EEXIST))
    {
        return;
    }
    ::strcpy_s(myInifile.data(), myInifile.size(), programdataFolder.data());
    ::strcat_s(myInifile.data(), myInifile.size(), "\\");
    ::strcat_s(myInifile.data(), myInifile.size(), INIFILE_NAME);
}

void IniFile::IniFileSetTemp(const char* const lpCmdLine) { ::strcpy_s(myInifile.data(), myInifile.size(), lpCmdLine); }

IniFile::~IniFile() {}

bool IniFile::WriteString(const char* const key1, const char* const key2, const char* const value)
{
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifile);
    return (FALSE != ::WritePrivateProfileString(key1, key2, value, myInifile.data()));
}

bool IniFile::WriteInt(const char* const key1, const char* const key2, const int value)
{
    std::array<char, 32U> buf;
    std::snprintf(buf.data(), buf.size(), "%d", value);
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifile);
    int result = ::WritePrivateProfileString(key1, key2, buf.data(), myInifile.data());
    if (result == 0)
    {
        return false;
    }
    return true;
}

int IniFile::ReadInt(const char* const key1, const char* const key2, const int Defaultvalue)
{
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifile);
    return ::GetPrivateProfileInt(key1, key2, Defaultvalue, myInifile.data());
}

void IniFile::ReadString(const char* const key1, const char* const key2, char* const value, const int valuesize)
{
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifile);
    ::GetPrivateProfileString(key1, key2, "", value, valuesize, myInifile.data());
}

bool IniFile::ReadPassword(char* const value, const int valuesize)
{
    //int size=ReadInt("UltraVNC", "passwdsize",0);
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifilePasswd);
    return ::GetPrivateProfileStruct("UltraVNC", "passwd", value, 8, myInifile.data());
}

bool IniFile::WritePassword(char* const value)
{
    //WriteInt("UltraVNC", "passwdsize",sizeof(value));
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifile);
    return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "passwd", value, 8, myInifile.data()));
}

bool                                                                            //PGM
IniFile::ReadPassword2(char* const value, const int valuesize)                  //PGM
{                                                                               //PGM
    return ::GetPrivateProfileStruct("UltraVNC", "passwd2", value, 8, myInifile.data()); //PGM
} //PGM

bool                                                                                         //PGM
IniFile::WritePassword2(char* const value)                                                   //PGM
{                                                                                            //PGM
    return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "passwd2", value, 8, myInifile.data())); //PGM
} //PGM

bool IniFile::IsWritable()
{
    bool writable = WriteInt("Permissions", "isWritable", 1);
    if (writable)
    {
        ::WritePrivateProfileSection("Permissions", "", myInifile.data());
    }

    return writable;
}