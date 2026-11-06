// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include <Windows.h>

#include <array>

#if (!defined(WINVNC_INIFILE))
#define WINVNC_INIFILE

#define INIFILE_NAME "ultravnc.ini"

class IniFile
{

    // Fields
public:
    std::array<char, MAX_PATH> myInifile;

    // Methods
public:
    // Make the desktop thread & window proc friends

    explicit IniFile();
    ~IniFile();
    bool WriteString(const char* key1, const char* key2, const char* value);
    bool WritePassword(char* value);
    bool WritePassword2(char* value); //PGM
    bool WriteInt(const char* key1, const char* key2, int value);
    int ReadInt(const char* key1, const char* key2, int Defaultvalue);
    void ReadString(const char* key1, const char* key2, char* value, int valuesize);
    void ReadPassword(char* value, int valuesize);
    void ReadPassword2(char* value, int valuesize); //PGM
    void IniFileSetSecure();
    //void IniFileSetTemp();
    void IniFileSetTemp(const char* lpCmdLine);
    void copy_to_secure();

    bool IsWritable();

protected:
};
#endif
