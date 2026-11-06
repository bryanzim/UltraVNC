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

#include <array>
#include <cstdio>
#include <cstring>

IniFile::IniFile()
{
    std::array<char, MAX_PATH> WORKDIR;
    if (::GetModuleFileName(nullptr, WORKDIR.data(), static_cast<DWORD>(WORKDIR.size())))
    {
        char* p = std::strrchr(WORKDIR.data(), '\\');
        if (p == nullptr)
        {
            return;
        }
        *p = '\0';
    }
    ::strcpy_s(myInifile.data(), myInifile.size(), WORKDIR.data());
    ::strcat_s(myInifile.data(), myInifile.size(), "\\");
    ::strcat_s(myInifile.data(), myInifile.size(), INIFILE_NAME);
}

void IniFile::IniFileSetSecure()
{
    std::array<char, MAX_PATH> WORKDIR;
    if (::GetModuleFileName(nullptr, WORKDIR.data(), static_cast<DWORD>(WORKDIR.size())))
    {
        char* p = std::strrchr(WORKDIR.data(), '\\');
        if (p == nullptr)
        {
            return;
        }
        *p = '\0';
    }
    ::strcpy_s(myInifile.data(), myInifile.size(), WORKDIR.data());
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

void IniFile::ReadPassword(char* const value, [[maybe_unused]] const int valuesize)
{
    //int size=ReadInt("UltraVNC", "passwdsize",0);
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifilePasswd);
    ::GetPrivateProfileStruct("UltraVNC", "passwd", value, 8, myInifile.data());
}

bool IniFile::WritePassword(char* const value)
{
    //WriteInt("UltraVNC", "passwdsize",sizeof(value));
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),myInifile);
    return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "passwd", value, 8, myInifile.data()));
}

void                                                                     //PGM
IniFile::ReadPassword2(char* const value, [[maybe_unused]] const int valuesize)           //PGM
{                                                                        //PGM
    ::GetPrivateProfileStruct("UltraVNC", "passwd2", value, 8, myInifile.data()); //PGM
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
