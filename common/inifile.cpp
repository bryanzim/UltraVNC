// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


//#include "stdhdrs.h"
#include "inifile.h"

#include <array>
#include <cstdio>

#pragma warning(disable : 4100)

// UltraVNC stores the VNC password as an 8-byte DES-encrypted blob.
// The encrypted value may contain NUL bytes, so it must not be checked
// with strlen() -- some valid passwords would be mistaken for empty.
static bool IsEncryptedPasswordEmpty(const char encrypted[8U])
{
    for (int i{ 0 }; i < 8; ++i)
    {
        if (encrypted[i] != 0)
        {
            return false;
        }
    }
    return true;
}

IniFile::IniFile() {}

void IniFile::setIniFile(char* const iniFile) { ::strcpy_s(strIniFile.data(), strIniFile.size(), iniFile); }

IniFile::~IniFile() {}

bool IniFile::WriteString(const char* const key1, const char* const key2, char* const value)
{
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),strIniFile);
    return (FALSE != ::WritePrivateProfileString(key1, key2, value, strIniFile.data()));
}

bool IniFile::WriteInt(const char* const key1, const char* const key2, const int value)
{
    std::array<char, 32U> buf;
    std::snprintf(buf.data(), buf.size(), "%d", value);
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),strIniFile);
    int result = ::WritePrivateProfileString(key1, key2, buf.data(), strIniFile.data());
    if (result == 0)
    {
        return false;
    }
    return true;
}

int IniFile::ReadInt(const char* const key1, const char* const key2, const int Defaultvalue)
{
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),strIniFile);
    return ::GetPrivateProfileInt(key1, key2, Defaultvalue, strIniFile.data());
}

void IniFile::ReadString(const char* const key1, const char* const key2, char* const value, const int valuesize)
{
    //vnclog.Print(LL_INTERR, VNCLOG("%s \n"),strIniFile);
    ::GetPrivateProfileString(key1, key2, "", value, valuesize, strIniFile.data());
}

void IniFile::ReadPassword(char* const value, const int valuesize)
{ ::GetPrivateProfileStruct("UltraVNC", "passwd", value, 8, strIniFile.data()); }

void IniFile::ReadPasswordViewOnly(char* const value, const int valuesize) //PGM
{
    ::GetPrivateProfileStruct("UltraVNC", "passwd2", value, 8, strIniFile.data()); //PGM
}

bool IniFile::WritePassword(char* const value)
{
    if (IsEncryptedPasswordEmpty(value))
    {
        return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "passwd", nullptr, 8, strIniFile.data()));
    }
    return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "passwd", value, 8, strIniFile.data()));
}

bool IniFile::WritePasswordViewOnly(char* const value)
{
    if (IsEncryptedPasswordEmpty(value))
    {
        return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "passwd2", nullptr, 8, strIniFile.data()));
    }
    return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "passwd2", value, 8, strIniFile.data()));
}

bool IniFile::IsWritable()
{
    bool writable = WriteInt("Permissions", "isWritable", 1);
    if (writable)
    {
        ::WritePrivateProfileSection("Permissions", "", strIniFile.data());
    }

    return writable;
}

void IniFile::ReadHash(char* const value, const int valuesize)
{ ::GetPrivateProfileStruct("UltraVNC", "hash", value, valuesize, strIniFile.data()); }

bool IniFile::WriteHash(char* const value, const int valuesize)
{ return (FALSE != ::WritePrivateProfileStruct("UltraVNC", "hash", value, valuesize, strIniFile.data())); }