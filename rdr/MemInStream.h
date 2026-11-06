// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
//


#ifndef __RDR_MEMINSTREAM_H__
#define __RDR_MEMINSTREAM_H__

#include "Exception.h"
#include "InStream.h"

namespace rdr
{

    class MemInStream : public InStream
    {

    public:
        MemInStream(const void* const data, const int len)
        {
            ptr = start = (const U8*)data;
            end = start + len;
        }

        int pos() { return static_cast<int>((ptr - start)); }
        void reposition(const int pos) { ptr = start + pos; }

    private:
        int overrun([[maybe_unused]] int itemSize, [[maybe_unused]] int nItems) { throw EndOfStream("overrun"); }
        const U8* start;
    };

}

#endif
