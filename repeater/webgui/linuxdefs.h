// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


/* linuxdefs.h
 *
 * Definitions for Linux port of webio demo.
 */

#ifndef LINUXDEFS_H
#define LINUXDEFS_H 1

#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define socketclose(s) close(s)

typedef unsigned long u_long;

void* memset(void*, int chr, size_t length);
#define stricmp(s, t) strcasecmp(s, t)

#define WI_NOBLOCKSOCK(socket) fcntl(socket, F_SETFL, O_NONBLOCK)

#define closesocket(socket) close(socket)

#endif /* LINUXDEFS */
