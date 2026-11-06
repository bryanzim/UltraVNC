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
// rdr::InStream marshalls data from a buffer stored in RDR (RFB Data
// Representation).
//

#ifndef RDR_INSTREAM_H
#define RDR_INSTREAM_H

#pragma once

#include "types.h"

#include <cstring>
#include <string.h> // for memcpy

namespace rdr
{

    class InStream
    {

    public:
        virtual ~InStream() {}

        // check() ensures there is buffer data for at least one item of size
        // itemSize bytes. Returns the number of items in the buffer (up to a
        // maximum of nItems).

        inline int check(const int itemSize, int nItems = 1)
        {
            if (ptr + itemSize * nItems > end)
            {
                if (ptr + itemSize > end)
                {
                    return overrun(itemSize, nItems);
                }

                nItems = static_cast<int>(((end - ptr) / itemSize));
            }
            return nItems;
        }

        // readU/SN() methods read unsigned and signed N-bit integers.

        inline U8 readU8()
        {
            check(1);
            return *ptr++;
        }
        inline U16 readU16()
        {
            check(2);
            const int b0 = *ptr++;
            const int b1 = *ptr++;
            return static_cast<rdr::U16>(b0 << 8 | b1);
        }
        inline U32 readU32()
        {
            check(4);
            const int b0 = *ptr++;
            const int b1 = *ptr++;
            const int b2 = *ptr++;
            const int b3 = *ptr++;
            return b0 << 24 | b1 << 16 | static_cast<U32>(b2 << 8 | b3);
        }

        inline S8 readS8() { return static_cast<S8>(readU8()); }
        inline S16 readS16() { return static_cast<S16>(readU16()); }
        inline S32 readS32() { return static_cast<S32>(readU32()); }

        // readString() reads a string - a U32 length followed by the data.
        // Returns a null-terminated string - the caller should delete[] it
        // afterwards.

        char* readString();

        // maxStringLength protects against allocating a huge buffer. Set it
        // higher if you need longer strings.

        static U32 maxStringLength;

        inline void skip(int bytes)
        {
            while (bytes > 0)
            {
                int n = check(1, bytes);
                ptr += n;
                bytes -= n;
            }
        }

        // readBytes() reads an exact number of bytes.

        virtual void readBytes(void* const data, const int length)
        {
            U8* dataPtr = reinterpret_cast<U8*>(data);
            U8* dataEnd = dataPtr + length;
            while (dataPtr < dataEnd)
            {
                int n = check(1, static_cast<int>((dataEnd - dataPtr)));
                std::memcpy(dataPtr, ptr, static_cast<size_t>(n));
                ptr += n;
                dataPtr += n;
            }
        }

        // readOpaqueN() reads a quantity without byte-swapping.

        inline U8 readOpaque8() { return readU8(); }
        inline U16 readOpaque16()
        {
            check(2);
            U16 r;
            (reinterpret_cast<U8*>(&r))[0U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[1U] = *ptr++;
            return r;
        }
        inline U32 readOpaque32()
        {
            check(4);
            U32 r;
            (reinterpret_cast<U8*>(&r))[0U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[1U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[2U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[3U] = *ptr++;
            return r;
        }
        inline U32 readOpaque24A()
        {
            check(3);
            U32 r = 0;
            (reinterpret_cast<U8*>(&r))[0U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[1U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[2U] = *ptr++;
            return r;
        }
        inline U32 readOpaque24B()
        {
            check(3);
            U32 r = 0;
            (reinterpret_cast<U8*>(&r))[1U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[2U] = *ptr++;
            (reinterpret_cast<U8*>(&r))[3U] = *ptr++;
            return r;
        }

        // pos() returns the position in the stream.

        virtual int pos() = 0;

        // getptr(), getend() and setptr() are "dirty" methods which allow you to
        // manipulate the buffer directly. This is useful for a stream which is a
        // wrapper around an underlying stream.

        inline const U8* getptr() const { return ptr; }
        inline const U8* getend() const { return end; }
        inline void setptr(const U8* const p) { ptr = p; }

    private:
        // overrun() is implemented by a derived class to cope with buffer overrun.
        // It ensures there are at least itemSize bytes of buffer data. Returns
        // the number of items in the buffer (up to a maximum of nItems). itemSize
        // is supposed to be "small" (a few bytes).

        virtual int overrun(int itemSize, int nItems) = 0;

    protected:
        explicit InStream() {}
        const U8* ptr;
        const U8* end;
    };

}

#endif
