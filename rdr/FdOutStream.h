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


//
// FdOutStream streams to a file descriptor.
//

#ifndef RDR_FDOUTSTREAM_H
#define RDR_FDOUTSTREAM_H

#include "OutStream.h"

namespace rdr
{

    class FdOutStream : public OutStream
    {

    public:
        explicit FdOutStream(int fd, int bufSize = 0);
        virtual ~FdOutStream();

        int getFd() { return fd; }

        void flush();
        [[nodiscard]] int length();
        void writeBytes(const void* const data, const int length);

    private:
        int overrun(int itemSize, int nItems);
        int fd;
        int bufSize;
        int offset;
        U8* start;
    };

}

#endif
