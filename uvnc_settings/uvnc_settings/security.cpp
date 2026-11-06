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

#include "dsmplugin.h"
#include "resource.h"
#include "vncsetauth.h"

#include <comutil.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

bool initdone2{ false };

extern LONG MSLogonRequired;
extern LONG NewMSLogon;
extern LONG UseDSMPlugin;
extern LONG AuthRequired;
extern LONG AllowShutdown;
extern LONG AllowProperties;
extern LONG AllowEditClients;
extern HINSTANCE hInst;
extern std::array<char, 128U> DSMPlugin; //PGM
#define MAXPWLEN 8
extern char passwd[MAXPWLEN];
extern char passwd2[MAXPWLEN]; //PGM
char* plaintext;
char* plaintext2; //PGM
vncSetAuth m_vncauth;

char* GetDSMPluginName();
CDSMPlugin* m_pDSMPlugin = nullptr;
CDSMPlugin* GetDSMPluginPointer() { return m_pDSMPlugin; };


std::array<unsigned char, 8U> fixedkey{ 23, 82, 107, 6, 35, 78, 88, 7 };
static std::array<unsigned long, 32U> KnL{ 0L };
static std::array<unsigned short, 8U> bytebit{ 01, 02, 04, 010, 020, 040, 0100, 0200 };
static std::array<unsigned long, 24U> bigbyte{ 0x800000L, 0x400000L, 0x200000L, 0x100000L, 0x80000L, 0x40000L, 0x20000L, 0x10000L,
                                      0x8000L,   0x4000L,   0x2000L,   0x1000L,   0x800L,   0x400L,   0x200L,   0x100L,
                                      0x80L,     0x40L,     0x20L,     0x10L,     0x8L,     0x4L,     0x2L,     0x1L };
static std::array<unsigned char, 56U> pc1{ 56, 48, 40, 32, 24, 16, 8,  0,  57, 49, 41, 33, 25, 17, 9,  1,  58, 50, 42,
                                  34, 26, 18, 10, 2,  59, 51, 43, 35, 62, 54, 46, 38, 30, 22, 14, 6,  61, 53,
                                  45, 37, 29, 21, 13, 5,  60, 52, 44, 36, 28, 20, 12, 4,  27, 19, 11, 3 };
static std::array<unsigned char, 16U> totrot{ 1, 2, 4, 6, 8, 10, 12, 14, 15, 17, 19, 21, 23, 25, 27, 28 };
static std::array<unsigned char, 48U> pc2{ 13, 16, 10, 23, 0,  4,  2,  27, 14, 5,  20, 9,  22, 18, 11, 3,  25, 7,  15, 6,  26, 19, 12, 1,
                                  40, 51, 30, 36, 46, 54, 29, 39, 50, 44, 32, 47, 43, 48, 38, 55, 33, 52, 45, 41, 49, 35, 28, 31 };

#define DE1 1
#define EN0 0

static std::array<unsigned long, 64U> SP1{ 0x01010400L, 0x00000000L, 0x00010000L, 0x01010404L, 0x01010004L, 0x00010404L, 0x00000004L,
                                  0x00010000L, 0x00000400L, 0x01010400L, 0x01010404L, 0x00000400L, 0x01000404L, 0x01010004L,
                                  0x01000000L, 0x00000004L, 0x00000404L, 0x01000400L, 0x01000400L, 0x00010400L, 0x00010400L,
                                  0x01010000L, 0x01010000L, 0x01000404L, 0x00010004L, 0x01000004L, 0x01000004L, 0x00010004L,
                                  0x00000000L, 0x00000404L, 0x00010404L, 0x01000000L, 0x00010000L, 0x01010404L, 0x00000004L,
                                  0x01010000L, 0x01010400L, 0x01000000L, 0x01000000L, 0x00000400L, 0x01010004L, 0x00010000L,
                                  0x00010400L, 0x01000004L, 0x00000400L, 0x00000004L, 0x01000404L, 0x00010404L, 0x01010404L,
                                  0x00010004L, 0x01010000L, 0x01000404L, 0x01000004L, 0x00000404L, 0x00010404L, 0x01010400L,
                                  0x00000404L, 0x01000400L, 0x01000400L, 0x00000000L, 0x00010004L, 0x00010400L, 0x00000000L,
                                  0x01010004L };

static std::array<unsigned long, 64U> SP2{ 0x80108020L, 0x80008000L, 0x00008000L, 0x00108020L, 0x00100000L, 0x00000020L, 0x80100020L,
                                  0x80008020L, 0x80000020L, 0x80108020L, 0x80108000L, 0x80000000L, 0x80008000L, 0x00100000L,
                                  0x00000020L, 0x80100020L, 0x00108000L, 0x00100020L, 0x80008020L, 0x00000000L, 0x80000000L,
                                  0x00008000L, 0x00108020L, 0x80100000L, 0x00100020L, 0x80000020L, 0x00000000L, 0x00108000L,
                                  0x00008020L, 0x80108000L, 0x80100000L, 0x00008020L, 0x00000000L, 0x00108020L, 0x80100020L,
                                  0x00100000L, 0x80008020L, 0x80100000L, 0x80108000L, 0x00008000L, 0x80100000L, 0x80008000L,
                                  0x00000020L, 0x80108020L, 0x00108020L, 0x00000020L, 0x00008000L, 0x80000000L, 0x00008020L,
                                  0x80108000L, 0x00100000L, 0x80000020L, 0x00100020L, 0x80008020L, 0x80000020L, 0x00100020L,
                                  0x00108000L, 0x00000000L, 0x80008000L, 0x00008020L, 0x80000000L, 0x80100020L, 0x80108020L,
                                  0x00108000L };

static std::array<unsigned long, 64U> SP3{ 0x00000208L, 0x08020200L, 0x00000000L, 0x08020008L, 0x08000200L, 0x00000000L, 0x00020208L,
                                  0x08000200L, 0x00020008L, 0x08000008L, 0x08000008L, 0x00020000L, 0x08020208L, 0x00020008L,
                                  0x08020000L, 0x00000208L, 0x08000000L, 0x00000008L, 0x08020200L, 0x00000200L, 0x00020200L,
                                  0x08020000L, 0x08020008L, 0x00020208L, 0x08000208L, 0x00020200L, 0x00020000L, 0x08000208L,
                                  0x00000008L, 0x08020208L, 0x00000200L, 0x08000000L, 0x08020200L, 0x08000000L, 0x00020008L,
                                  0x00000208L, 0x00020000L, 0x08020200L, 0x08000200L, 0x00000000L, 0x00000200L, 0x00020008L,
                                  0x08020208L, 0x08000200L, 0x08000008L, 0x00000200L, 0x00000000L, 0x08020008L, 0x08000208L,
                                  0x00020000L, 0x08000000L, 0x08020208L, 0x00000008L, 0x00020208L, 0x00020200L, 0x08000008L,
                                  0x08020000L, 0x08000208L, 0x00000208L, 0x08020000L, 0x00020208L, 0x00000008L, 0x08020008L,
                                  0x00020200L };

static std::array<unsigned long, 64U> SP4{ 0x00802001L, 0x00002081L, 0x00002081L, 0x00000080L, 0x00802080L, 0x00800081L, 0x00800001L,
                                  0x00002001L, 0x00000000L, 0x00802000L, 0x00802000L, 0x00802081L, 0x00000081L, 0x00000000L,
                                  0x00800080L, 0x00800001L, 0x00000001L, 0x00002000L, 0x00800000L, 0x00802001L, 0x00000080L,
                                  0x00800000L, 0x00002001L, 0x00002080L, 0x00800081L, 0x00000001L, 0x00002080L, 0x00800080L,
                                  0x00002000L, 0x00802080L, 0x00802081L, 0x00000081L, 0x00800080L, 0x00800001L, 0x00802000L,
                                  0x00802081L, 0x00000081L, 0x00000000L, 0x00000000L, 0x00802000L, 0x00002080L, 0x00800080L,
                                  0x00800081L, 0x00000001L, 0x00802001L, 0x00002081L, 0x00002081L, 0x00000080L, 0x00802081L,
                                  0x00000081L, 0x00000001L, 0x00002000L, 0x00800001L, 0x00002001L, 0x00802080L, 0x00800081L,
                                  0x00002001L, 0x00002080L, 0x00800000L, 0x00802001L, 0x00000080L, 0x00800000L, 0x00002000L,
                                  0x00802080L };

static std::array<unsigned long, 64U> SP5{ 0x00000100L, 0x02080100L, 0x02080000L, 0x42000100L, 0x00080000L, 0x00000100L, 0x40000000L,
                                  0x02080000L, 0x40080100L, 0x00080000L, 0x02000100L, 0x40080100L, 0x42000100L, 0x42080000L,
                                  0x00080100L, 0x40000000L, 0x02000000L, 0x40080000L, 0x40080000L, 0x00000000L, 0x40000100L,
                                  0x42080100L, 0x42080100L, 0x02000100L, 0x42080000L, 0x40000100L, 0x00000000L, 0x42000000L,
                                  0x02080100L, 0x02000000L, 0x42000000L, 0x00080100L, 0x00080000L, 0x42000100L, 0x00000100L,
                                  0x02000000L, 0x40000000L, 0x02080000L, 0x42000100L, 0x40080100L, 0x02000100L, 0x40000000L,
                                  0x42080000L, 0x02080100L, 0x40080100L, 0x00000100L, 0x02000000L, 0x42080000L, 0x42080100L,
                                  0x00080100L, 0x42000000L, 0x42080100L, 0x02080000L, 0x00000000L, 0x40080000L, 0x42000000L,
                                  0x00080100L, 0x02000100L, 0x40000100L, 0x00080000L, 0x00000000L, 0x40080000L, 0x02080100L,
                                  0x40000100L };

static std::array<unsigned long, 64U> SP6{ 0x20000010L, 0x20400000L, 0x00004000L, 0x20404010L, 0x20400000L, 0x00000010L, 0x20404010L,
                                  0x00400000L, 0x20004000L, 0x00404010L, 0x00400000L, 0x20000010L, 0x00400010L, 0x20004000L,
                                  0x20000000L, 0x00004010L, 0x00000000L, 0x00400010L, 0x20004010L, 0x00004000L, 0x00404000L,
                                  0x20004010L, 0x00000010L, 0x20400010L, 0x20400010L, 0x00000000L, 0x00404010L, 0x20404000L,
                                  0x00004010L, 0x00404000L, 0x20404000L, 0x20000000L, 0x20004000L, 0x00000010L, 0x20400010L,
                                  0x00404000L, 0x20404010L, 0x00400000L, 0x00004010L, 0x20000010L, 0x00400000L, 0x20004000L,
                                  0x20000000L, 0x00004010L, 0x20000010L, 0x20404010L, 0x00404000L, 0x20400000L, 0x00404010L,
                                  0x20404000L, 0x00000000L, 0x20400010L, 0x00000010L, 0x00004000L, 0x20400000L, 0x00404010L,
                                  0x00004000L, 0x00400010L, 0x20004010L, 0x00000000L, 0x20404000L, 0x20000000L, 0x00400010L,
                                  0x20004010L };

static std::array<unsigned long, 64U> SP7{ 0x00200000L, 0x04200002L, 0x04000802L, 0x00000000L, 0x00000800L, 0x04000802L, 0x00200802L,
                                  0x04200800L, 0x04200802L, 0x00200000L, 0x00000000L, 0x04000002L, 0x00000002L, 0x04000000L,
                                  0x04200002L, 0x00000802L, 0x04000800L, 0x00200802L, 0x00200002L, 0x04000800L, 0x04000002L,
                                  0x04200000L, 0x04200800L, 0x00200002L, 0x04200000L, 0x00000800L, 0x00000802L, 0x04200802L,
                                  0x00200800L, 0x00000002L, 0x04000000L, 0x00200800L, 0x04000000L, 0x00200800L, 0x00200000L,
                                  0x04000802L, 0x04000802L, 0x04200002L, 0x04200002L, 0x00000002L, 0x00200002L, 0x04000000L,
                                  0x04000800L, 0x00200000L, 0x04200800L, 0x00000802L, 0x00200802L, 0x04200800L, 0x00000802L,
                                  0x04000002L, 0x04200802L, 0x04200000L, 0x00200800L, 0x00000000L, 0x00000002L, 0x04200802L,
                                  0x00000000L, 0x00200802L, 0x04200000L, 0x00000800L, 0x04000002L, 0x04000800L, 0x00000800L,
                                  0x00200002L };

static std::array<unsigned long, 64U> SP8{ 0x10001040L, 0x00001000L, 0x00040000L, 0x10041040L, 0x10000000L, 0x10001040L, 0x00000040L,
                                  0x10000000L, 0x00040040L, 0x10040000L, 0x10041040L, 0x00041000L, 0x10041000L, 0x00041040L,
                                  0x00001000L, 0x00000040L, 0x10040000L, 0x10000040L, 0x10001000L, 0x00001040L, 0x00041000L,
                                  0x00040040L, 0x10040040L, 0x10041000L, 0x00001040L, 0x00000000L, 0x00000000L, 0x10040040L,
                                  0x10000040L, 0x10001000L, 0x00041040L, 0x00040000L, 0x00041040L, 0x00040000L, 0x10041000L,
                                  0x00001000L, 0x00000040L, 0x10040040L, 0x00001000L, 0x00041040L, 0x10001000L, 0x00000040L,
                                  0x10000040L, 0x10040000L, 0x10040040L, 0x10000000L, 0x00040000L, 0x10001040L, 0x00000000L,
                                  0x10041040L, 0x00040040L, 0x10000040L, 0x10040000L, 0x10001000L, 0x10001040L, 0x00000000L,
                                  0x10041040L, 0x00041000L, 0x00041000L, 0x00001040L, 0x00001040L, 0x00040040L, 0x10000000L,
                                  0x10041000L };

void usekey(register unsigned long* from)
{
    register unsigned long *to, *endp;

    to = KnL.data(), endp = &KnL[32U];
    while (to < endp)
    {
        *to++ = *from++;
    }
    return;
}

static void cookey(register unsigned long* raw1)
{
    register unsigned long *cook, *raw0;
    std::array<unsigned long, 32U> dough;
    register int i;

    cook = dough.data();
    for (i = 0; i < 16; ++i, ++raw1)
    {
        raw0 = raw1++;
        *cook = (*raw0 & 0x00fc0000L) << 6;
        *cook |= (*raw0 & 0x00000fc0L) << 10;
        *cook |= (*raw1 & 0x00fc0000L) >> 10;
        *cook++ |= (*raw1 & 0x00000fc0L) >> 6;
        *cook = (*raw0 & 0x0003f000L) << 12;
        *cook |= (*raw0 & 0x0000003fL) << 16;
        *cook |= (*raw1 & 0x0003f000L) >> 4;
        *cook++ |= (*raw1 & 0x0000003fL);
    }
    usekey(dough.data());
    return;
}

void deskey(unsigned char* const key, const int edf) /* Thanks to James Gillogly & Phil Karn! */
{
    register int i, j, l, m, n;
    std::array<unsigned char, 56U> pc1m;
    std::array<unsigned char, 56U> pcr;
    std::array<unsigned long, 32U> kn;

    for (j = 0; j < 56; ++j)
    {
        l = pc1[static_cast<unsigned long long>(j)];
        m = l & 07;
        pc1m[static_cast<unsigned long long>(j)] = (key[l >> 3] & bytebit[static_cast<unsigned long long>(m)]) ? 1 : 0;
    }
    for (i = 0; i < 16; ++i)
    {
        if (edf == DE1)
        {
            m = (15 - i) << 1;
        }
        else
        {
            m = i << 1;
        }
        n = m + 1;
        kn[static_cast<unsigned long long>(m)] = kn[static_cast<unsigned long long>(n)] = 0L;
        for (j = 0; j < 28; ++j)
        {
            l = j + totrot[static_cast<unsigned long long>(i)];
            if (l < 28)
            {
                pcr[static_cast<unsigned long long>(j)] = pc1m[static_cast<unsigned long long>(l)];
            }
            else
            {
                pcr[static_cast<unsigned long long>(j)] = pc1m[static_cast<unsigned long long>(l - 28)];
            }
        }
        for (j = 28; j < 56; ++j)
        {
            l = j + totrot[static_cast<unsigned long long>(i)];
            if (l < 56)
            {
                pcr[static_cast<unsigned long long>(j)] = pc1m[static_cast<unsigned long long>(l)];
            }
            else
            {
                pcr[static_cast<unsigned long long>(j)] = pc1m[static_cast<unsigned long long>(l - 28)];
            }
        }
        for (j = 0; j < 24; ++j)
        {
            if (pcr[pc2[static_cast<unsigned long long>(j)]])
            {
                kn[static_cast<unsigned long long>(m)] |= bigbyte[static_cast<unsigned long long>(j)];
            }
            if (pcr[pc2[static_cast<unsigned long long>(j + 24)]])
            {
                kn[static_cast<unsigned long long>(n)] |= bigbyte[static_cast<unsigned long long>(j)];
            }
        }
    }
    cookey(kn.data());
    return;
}
static void scrunch(register unsigned char* outof, register unsigned long* into)
{
    *into = static_cast<unsigned long>((*outof++ & 0xffL) << 24);
    *into |= static_cast<unsigned long>((*outof++ & 0xffL) << 16);
    *into |= static_cast<unsigned long>((*outof++ & 0xffL) << 8);
    *into++ |= (*outof++ & 0xffL);
    *into = static_cast<unsigned long>((*outof++ & 0xffL) << 24);
    *into |= static_cast<unsigned long>((*outof++ & 0xffL) << 16);
    *into |= static_cast<unsigned long>((*outof++ & 0xffL) << 8);
    *into |= (*outof & 0xffL);
    return;
}

static void unscrun(register unsigned long* outof, register unsigned char* into)
{
    *into++ = static_cast<unsigned char>(((*outof >> 24) & 0xffL));
    *into++ = static_cast<unsigned char>(((*outof >> 16) & 0xffL));
    *into++ = static_cast<unsigned char>(((*outof >> 8) & 0xffL));
    *into++ = static_cast<unsigned char>((*outof++ & 0xffL));
    *into++ = static_cast<unsigned char>(((*outof >> 24) & 0xffL));
    *into++ = static_cast<unsigned char>(((*outof >> 16) & 0xffL));
    *into++ = static_cast<unsigned char>(((*outof >> 8) & 0xffL));
    *into = static_cast<unsigned char>((*outof & 0xffL));
    return;
}

static void desfunc(register unsigned long* block, register unsigned long* keys)
{
    register unsigned long fval, work, right, leftt;
    register int round;

    leftt = block[0U];
    right = block[1U];
    work = ((leftt >> 4) ^ right) & 0x0f0f0f0fL;
    right ^= work;
    leftt ^= (work << 4);
    work = ((leftt >> 16) ^ right) & 0x0000ffffL;
    right ^= work;
    leftt ^= (work << 16);
    work = ((right >> 2) ^ leftt) & 0x33333333L;
    leftt ^= work;
    right ^= (work << 2);
    work = ((right >> 8) ^ leftt) & 0x00ff00ffL;
    leftt ^= work;
    right ^= (work << 8);
    right = ((right << 1) | ((right >> 31) & 1L)) & 0xffffffffL;
    work = (leftt ^ right) & 0xaaaaaaaaL;
    leftt ^= work;
    right ^= work;
    leftt = ((leftt << 1) | ((leftt >> 31) & 1L)) & 0xffffffffL;

    for (round = 0; round < 8; ++round)
    {
        work = (right << 28) | (right >> 4);
        work ^= *keys++;
        fval = SP7[work & 0x3fL];
        fval |= SP5[(work >> 8) & 0x3fL];
        fval |= SP3[(work >> 16) & 0x3fL];
        fval |= SP1[(work >> 24) & 0x3fL];
        work = right ^ *keys++;
        fval |= SP8[work & 0x3fL];
        fval |= SP6[(work >> 8) & 0x3fL];
        fval |= SP4[(work >> 16) & 0x3fL];
        fval |= SP2[(work >> 24) & 0x3fL];
        leftt ^= fval;
        work = (leftt << 28) | (leftt >> 4);
        work ^= *keys++;
        fval = SP7[work & 0x3fL];
        fval |= SP5[(work >> 8) & 0x3fL];
        fval |= SP3[(work >> 16) & 0x3fL];
        fval |= SP1[(work >> 24) & 0x3fL];
        work = leftt ^ *keys++;
        fval |= SP8[work & 0x3fL];
        fval |= SP6[(work >> 8) & 0x3fL];
        fval |= SP4[(work >> 16) & 0x3fL];
        fval |= SP2[(work >> 24) & 0x3fL];
        right ^= fval;
    }

    right = (right << 31) | (right >> 1);
    work = (leftt ^ right) & 0xaaaaaaaaL;
    leftt ^= work;
    right ^= work;
    leftt = (leftt << 31) | (leftt >> 1);
    work = ((leftt >> 8) ^ right) & 0x00ff00ffL;
    right ^= work;
    leftt ^= (work << 8);
    work = ((leftt >> 2) ^ right) & 0x33333333L;
    right ^= work;
    leftt ^= (work << 2);
    work = ((right >> 16) ^ leftt) & 0x0000ffffL;
    leftt ^= work;
    right ^= (work << 16);
    work = ((right >> 4) ^ leftt) & 0x0f0f0f0fL;
    leftt ^= work;
    right ^= (work << 4);
    *block++ = right;
    *block = leftt;
    return;
}

void des(unsigned char* const inblock, unsigned char* const outblock)
{
    std::array<unsigned long, 2U> work;

    scrunch(inblock, work.data());
    desfunc(work.data(), KnL.data());
    unscrun(work.data(), outblock);
    return;
}
int vncEncryptPasswd(char* const passwdLocal, char* const encryptedPasswd)
{
    std::size_t i;

    /* pad password with nulls */

    for (i = 0; i < MAXPWLEN; ++i)
    {
        if (i < std::strlen(passwdLocal))
        {
            encryptedPasswd[i] = passwdLocal[i];
        }
        else
        {
            encryptedPasswd[i] = 0;
        }
    }

    /* Do encryption in-place - this way we overwrite our copy of the plaintext
       password */

    deskey(fixedkey.data(), EN0);
    des(reinterpret_cast<unsigned char*>(encryptedPasswd), reinterpret_cast<unsigned char*>(encryptedPasswd));

    return 8;
}

char* vncDecryptPasswd(char* const inouttext)
{
    unsigned char* passwdLocal = reinterpret_cast<unsigned char*>(std::malloc(9));

    deskey(fixedkey.data(), DE1);
    des(reinterpret_cast<unsigned char*>(inouttext), passwdLocal);

    passwdLocal[8U] = 0;

    return reinterpret_cast<char*>(passwdLocal);
}

BOOL CALLBACK security(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG: {
        initdone2 = false;
        m_pDSMPlugin = new CDSMPlugin();
        plaintext = vncDecryptPasswd(passwd);
        plaintext2 = vncDecryptPasswd(passwd2); //PGM
        ::SendMessage(::GetDlgItem(hwnd, IDC_MSLOGON_CHECKD), BM_SETCHECK, static_cast<WPARAM>(MSLogonRequired), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_NEW_MSLOGON), BM_SETCHECK, static_cast<WPARAM>(NewMSLogon), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_PASSRECK), BM_SETCHECK, static_cast<WPARAM>(AuthRequired), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_ALLOWSHUTDOWN), BM_SETCHECK, static_cast<WPARAM>(AllowShutdown), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_ALLOWEDIT), BM_SETCHECK, static_cast<WPARAM>(AllowEditClients), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_ALLOWPROP), BM_SETCHECK, static_cast<WPARAM>(AllowProperties), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_PLUGIN_CHECK), BM_SETCHECK, static_cast<WPARAM>(UseDSMPlugin), 0);
        ::SetDlgItemText(hwnd, IDC_PASSWORD, plaintext);
        ::SetDlgItemText(hwnd, IDC_PASSWORD2, plaintext2); //PGM
        BOOL bMSLogonChecked = (::SendDlgItemMessage(hwnd, IDC_MSLOGON_CHECKD, BM_GETCHECK, 0, 0) == BST_CHECKED);

        ::EnableWindow(::GetDlgItem(hwnd, IDC_NEW_MSLOGON), bMSLogonChecked);
        ::EnableWindow(::GetDlgItem(hwnd, IDC_MSLOGON), bMSLogonChecked);

        HWND hPlugins = ::GetDlgItem(hwnd, IDC_PLUGINS_COMBO);
        int nPlugins = GetDSMPluginPointer()->ListPlugins(hPlugins);
        if (!nPlugins)
        {
            ::SendMessage(hPlugins, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("No Plugin Detected"));
            ::SendMessage(hPlugins, CB_SETCURSEL, 0, 0);
        }
        else
        {
            //PGM				SendMessage(hPlugins, CB_SELECTSTRING, 0, (LPARAM)GetDSMPluginName());
            ::SendMessage(hPlugins, CB_SELECTSTRING, 0, reinterpret_cast<LPARAM>(DSMPlugin.data()));
        } //PGM

        initdone2 = true;
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_PLUGIN_BUTTON: {
            HWND hPlugin = ::GetDlgItem(hwnd, IDC_PLUGIN_CHECK);
            if (::SendMessage(hPlugin, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                std::array<TCHAR, MAX_PATH> szPlugin;
                ::GetDlgItemText(hwnd, IDC_PLUGINS_COMBO, szPlugin.data(), static_cast<int>(szPlugin.size()));
                if (!GetDSMPluginPointer()->IsLoaded())
                {
                    GetDSMPluginPointer()->LoadPlugin(szPlugin.data(), false);
                }
                else
                {
                    // sf@2003 - We check if the loaded plugin is the same than
                    // the currently selected one or not
                    GetDSMPluginPointer()->DescribePlugin();
                    if (::_stricmp(GetDSMPluginPointer()->GetPluginFileName(), szPlugin.data()))
                    {
                        GetDSMPluginPointer()->UnloadPlugin();
                        GetDSMPluginPointer()->LoadPlugin(szPlugin.data(), false);
                    }
                }

                if (GetDSMPluginPointer()->IsLoaded())
                {
                    // We don't send the password yet... no matter the plugin requires
                    // it or not, we will provide it later (at plugin "real" init)
                    // Knowing the environnement ("server-svc" or "server-app") right
                    // now can be usefull or even mandatory for the plugin
                    // (specific params saving and so on...)
                    std::array<char, 32U> szParams;
                    std::strcpy(szParams.data(), "NoPassword,");
                    std::strcat(szParams.data(), "server-app");
                    char* szNewConfig{ nullptr };
                    std::array<char, 512U> DSMPluginConfig;
                    DSMPluginConfig[0U] = '\0';
                    IniFile myIniFile;
                    myIniFile.ReadString("admin", "DSMPluginConfig", DSMPluginConfig.data(), 512);

                    GetDSMPluginPointer()->SetPluginParams(hwnd, szParams.data(), DSMPluginConfig.data(), &szNewConfig);

                    if ((szNewConfig != nullptr) && (std::strlen(szNewConfig) > 0))
                    {
                        ::strcpy_s(DSMPluginConfig.data(), 511, szNewConfig);
                    }
                    myIniFile.WriteString("admin", "DSMPluginConfig", DSMPluginConfig.data());
                }
                /*else
                {
                    MessageBox(NULL,
                        sz_ID_PLUGIN_NOT_LOAD,
                        sz_ID_PLUGIN_LOADIN, MB_OK | MB_ICONEXCLAMATION );
                }*/
            }
            return TRUE;
        }
        case IDC_MSLOGON_CHECKD: {
            BOOL bMSLogonChecked = (::SendDlgItemMessage(hwnd, IDC_MSLOGON_CHECKD, BM_GETCHECK, 0, 0) == BST_CHECKED);

            ::EnableWindow(::GetDlgItem(hwnd, IDC_NEW_MSLOGON), bMSLogonChecked);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_MSLOGON), bMSLogonChecked);
        }
            return TRUE;
        case IDC_MSLOGON: {
            // Marscha@2004 - authSSP: if "New MS-Logon" is checked,
            // call vncEditSecurity from SecurityEditor.dll,
            // else call "old" dialog.
            BOOL bNewMSLogonChecked = (::SendDlgItemMessage(hwnd, IDC_NEW_MSLOGON, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (bNewMSLogonChecked)
            {
                typedef void (*vncEditSecurityFn)(HWND hwnd, HINSTANCE hInstance);
                vncEditSecurityFn vncEditSecurity = nullptr;
                std::array<char, MAX_PATH> szCurrentDir;
                if (::GetModuleFileName(nullptr, szCurrentDir.data(), static_cast<DWORD>(szCurrentDir.size())))
                {
                    char* p = std::strrchr(szCurrentDir.data(), '\\');
                    *p = '\0';
                    std::strcat(szCurrentDir.data(), "\\authSSP.dll");
                }
                HMODULE hModule = ::LoadLibrary(szCurrentDir.data());
                if (hModule)
                {
                    vncEditSecurity = reinterpret_cast<vncEditSecurityFn>(::GetProcAddress(hModule, "vncEditSecurity"));
                    [[maybe_unused]] HRESULT hr = ::CoInitialize(nullptr);
                    vncEditSecurity(nullptr, hInst);
                    ::CoUninitialize();
                    ::FreeLibrary(hModule);
                }
            }
            else
            {
                // Marscha@2004 - authSSP: end of change
                m_vncauth.Show(TRUE);
            }
        }
            return TRUE;
        case IDCANCEL:
            if (m_pDSMPlugin != nullptr)
            {
                delete (m_pDSMPlugin);
                m_pDSMPlugin = nullptr;
            }
            ::EndDialog(hwnd, IDCANCEL);
            return TRUE;
        case IDC_HELP2:
            if (lParam == 1)

            {
                std::array<char, 256U> link;
                std::strcpy(link.data(), "https://uvnc.com/webhelp/");
                std::strcat(link.data(), "security");
                std::strcat(link.data(), ".html");
                ::ShellExecute(::GetDesktopWindow(), "open", link.data(), "", nullptr, SW_SHOWNORMAL);
            }
            break;
        case IDOK:
            MSLogonRequired = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_MSLOGON_CHECKD, BM_GETCHECK, 0, 0));
            NewMSLogon = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_NEW_MSLOGON, BM_GETCHECK, 0, 0));
            AuthRequired = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_PASSRECK, BM_GETCHECK, 0, 0));
            AllowShutdown = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_ALLOWSHUTDOWN, BM_GETCHECK, 0, 0));
            AllowEditClients = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_ALLOWEDIT, BM_GETCHECK, 0, 0));
            AllowProperties = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_ALLOWPROP, BM_GETCHECK, 0, 0));
            UseDSMPlugin = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_PLUGIN_CHECK, BM_GETCHECK, 0, 0));
            ::GetDlgItemText(hwnd, IDC_PLUGINS_COMBO, DSMPlugin.data(), 128);                        //PGM
            std::array<char, MAXPWLEN + 1> plaintext1;                                                  //PGM
            int len = static_cast<int>(::GetDlgItemText(hwnd, IDC_PASSWORD, static_cast<LPSTR>(plaintext1.data()), static_cast<int>(plaintext1.size()))); //PGM
            if (len == 0)
            {
                std::strcpy(passwd, "");
            }
            else
            {
                vncEncryptPasswd(plaintext1.data(), passwd); //PGM
            }

            std::memset(plaintext1.data(), '\0', 9);                                            //PGM
            len = 0;                                                                     //PGM
            len = static_cast<int>(::GetDlgItemText(hwnd, IDC_PASSWORD2, static_cast<LPSTR>(plaintext1.data()), static_cast<int>(plaintext1.size()))); //PGM
            if (len == 0)                                                                //PGM
            {                                                                            //PGM
                std::strcpy(passwd2, "");                                                //PGM
            } //PGM
            else                                       //PGM
            {                                          //PGM
                vncEncryptPasswd(plaintext1.data(), passwd2); //PGM
            } //PGM

            return TRUE;
        }
    }
    return static_cast<INT_PTR>(FALSE);
}
