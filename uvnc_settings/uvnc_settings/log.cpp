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

#include "log.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

#define LOG_LINES 7
extern HWND EditControl;

static struct LIST
{
    struct LIST* next;
    int len;
    std::array<char, 1U> txt; /* single character for trailing '\0' */
} *head = nullptr, *tail = nullptr;

static char* log_txt(void)
{
    char* buff;
    int ptr{ 0 }, len = 0;
    struct LIST* curr;

    //   enter_critical_section(CRIT_WIN_LOG);
    for (curr = head; curr; curr = curr->next)
    {
        len += curr->len + 2;
    } /* +2 for trailing '\r\n' */
    buff = reinterpret_cast<char*>(std::malloc(static_cast<size_t>(len + 1))); /* +1 for trailing '\0' */
    for (curr = head; curr; curr = curr->next)
    {
        std::memcpy(buff + ptr, curr->txt.data(), static_cast<size_t>(curr->len));
        ptr += curr->len;
        if (curr->next)
        {
            buff[ptr++] = '\r';
            buff[ptr++] = '\n';
        }
    }
    buff[ptr] = '\0';
    //   leave_critical_section(CRIT_WIN_LOG);

    return buff;
}

void win_log_cleanup()
{
    struct LIST* curr;
    struct LIST* temp;

    curr = head;
    if (curr == nullptr)
    {
        return;
    }
    temp = curr->next;
    //check
    if (curr != nullptr)
    {
        std::free(curr);
    }
    while (temp != nullptr)
    {
        curr = temp;
        temp = curr->next;
        std::free(curr);
    }
}


void win_log(char* const line)
{ /* Also used in log.c */
    struct LIST* curr;
    int len;
    static int log_len{ 0 };
    char* txt;
    len = static_cast<int>(std::strlen(line));
    curr = reinterpret_cast<LIST*>(std::malloc(sizeof(struct LIST) + static_cast<unsigned long long>(len)));
    curr->len = len;
    std::strcpy(curr->txt.data(), line);
    curr->next = nullptr;

    //   enter_critical_section(CRIT_WIN_LOG);
    if (tail)
    {
        tail->next = curr;
    }
    tail = curr;
    if (!head)
    {
        head = tail;
    }
    ++log_len;
    while (log_len > LOG_LINES)
    {
        curr = head;
        head = head->next;
        std::free(curr);
        --log_len;
    }
    txt = log_txt();
    ::SetWindowText(EditControl, txt);
    std::free(txt);
}

void debug(const char* const fmt, ...)
{
    std::array<char, 256U> myoutput;
    std::array<char, 256U> myoutput2;
    va_list args;
    std::memset(myoutput2.data(), 0, 256);
    va_start(args, fmt);
    std::snprintf(myoutput2.data(), myoutput2.size(), "> ");
    vsprintf(myoutput.data(), fmt, args);
    va_end(args);
    std::strncat(myoutput2.data(), myoutput.data(), std::strlen(myoutput.data()));
    win_log(myoutput2.data());
}
