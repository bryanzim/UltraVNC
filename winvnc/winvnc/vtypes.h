// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// VTypes.h

// RFB V3.0

// Datatypes used by the VGUI system

#if (!defined(ATT_VTYPES_DEFINED))
#define ATT_VTYPES_DEFINED

////////////////////////////
// Numeric data types

////////////////////////////
// Fixed size (derived from rfb.h)

using VCard32 = unsigned int;
using VCard16 = unsigned short;
using VCard8 = unsigned char;
using VInt32 = int;
using VInt16 = short;
using VInt8 = char;

////////////////////////////
// Variable size
//		These will always be at least as big as the largest
//		fixed-size data-type

using VCard = VCard32;
using VInt = VInt32;

////////////////////////////
// Useful functions on integers

static inline VInt Max(const VInt x, const VInt y)
{
    if (x > y)
    {
        return x;
    }
    else
    {
        return y;
    }
}
static inline VInt Min(const VInt x, const VInt y)
{
    if (x < y)
    {
        return x;
    }
    else
    {
        return y;
    }
}

////////////////////////////
// Boolean

using VBool = int;
const VBool VTrue = -1;
const VBool VFalse = 0;

////////////////////////////
// Others

using VChar = char;
#if (!defined(NULL))
#define NULL 0
#endif

////////////////////////////
// Compound data types

// #include "rfbgui/VPoint.h"
// #include "rfbgui/VRect.h"
using VString = VChar*;

#endif // ATT_VTYPES_DEFINED
