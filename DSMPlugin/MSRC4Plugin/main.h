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


#ifndef MAIN_H
#define MAIN_H


#pragma once
#include "EnvReg.h"

#include <Windows.h>

#include <array>

int Encrypt(char* inFile, char* outFile, char* keyFile);
int Decrypt(char* inFile, char* outFile, char* keyFile);
int CreateKey(char* keyFile, DWORD keyLen);
int ListProviders();
int WriteDebugInfo();
void Usage();
void Version();
int WinVer();


extern char CSP_NAME[70U];
extern DWORD VERIFY_CONTEXT_FLAG;
extern DWORD NULL_CONTEXT_FLAG;
extern DWORD CONTEXT_FLAG;
extern DWORD KEYLEN;
extern DWORD MAXKEYLEN;
extern std::array<CHAR, 100U> szUserName; // Buffer to hold the name of the key container.


#endif
