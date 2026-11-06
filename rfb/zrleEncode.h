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
// zrleEncode.h - zrle encoding function.
//
// Before including this file, you must define a number of CPP macros.
//
// BPP should be 8, 16 or 32 depending on the bits per pixel.
// GET_IMAGE_INTO_BUF should be some code which gets a rectangle of pixel data
// into the given buffer. EXTRA_ARGS can be defined to pass any other
// arguments needed by GET_IMAGE_INTO_BUF.
//
// Note that the buf argument to ZRLE_ENCODE needs to be at least one pixel
// bigger than the largest tile of pixel data, since the ZRLE encoding
// algorithm writes to the position one past the end of the pixel data.
//

#include <rdr/OutStream.h>

#include <assert.h>
#include <cstring>

using namespace rdr;

/* __RFB_CONCAT2 concatenates its two arguments. __RFB_CONCAT2E does the same
   but also expands its arguments if they are macros */

#ifndef __RFB_CONCAT2E
#define __RFB_CONCAT2(a, b) a##b
#define __RFB_CONCAT2E(a, b) __RFB_CONCAT2(a, b)
#endif

#ifndef __RFB_CONCAT3E
#define __RFB_CONCAT3(a, b, c) a##b##c
#define __RFB_CONCAT3E(a, b, c) __RFB_CONCAT3(a, b, c)
#endif

#undef END_FIX
#if ZYWRLE_ENDIAN == ENDIAN_LITTLE
#define END_FIX LE
#elif ZYWRLE_ENDIAN == ENDIAN_BIG
#define END_FIX BE
#else
#define END_FIX NE
#endif

#ifdef CPIXEL
#define PIXEL_T __RFB_CONCAT2E(rdr::U, BPP)
#define WRITE_PIXEL __RFB_CONCAT2E(writeOpaque, CPIXEL)
#define ZRLE_ENCODE __RFB_CONCAT3E(zrleEncode, CPIXEL, END_FIX)
#define ZRLE_ENCODE_TILE __RFB_CONCAT3E(zrleEncodeTile, CPIXEL, END_FIX)
#define BPPOUT 24
#elif BPP == 15
#define PIXEL_T __RFB_CONCAT2E(rdr::U, 16)
#define WRITE_PIXEL __RFB_CONCAT2E(writeOpaque, 16)
#define ZRLE_ENCODE __RFB_CONCAT3E(zrleEncode, BPP, END_FIX)
#define ZRLE_ENCODE_TILE __RFB_CONCAT3E(zrleEncodeTile, BPP, END_FIX)
#define BPPOUT 16
#else
#define PIXEL_T __RFB_CONCAT2E(rdr::U, BPP)
#define WRITE_PIXEL __RFB_CONCAT2E(writeOpaque, BPP)
#define ZRLE_ENCODE __RFB_CONCAT3E(zrleEncode, BPP, END_FIX)
#define ZRLE_ENCODE_TILE __RFB_CONCAT3E(zrleEncodeTile, BPP, END_FIX)
#define BPPOUT BPP
#endif

#ifndef ZRLE_ONCE
#define ZRLE_ONCE
static const int bitsPerPackedPixel[] = { 0, 1, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };

int zywrle_level{ 1 };
int zywrleBuf[rfbZRLETileWidth * rfbZRLETileHeight];

// The PaletteHelper class helps us build up the palette from pixel data by
// storing a reverse index using a simple hash-table

class PaletteHelper
{
public:
    enum
    {
        MAX_SIZE = 127
    };

    explicit PaletteHelper()
    {
        std::memset(index, 255, sizeof(index));
        size = 0;
    }

    inline int hash(const rdr::U32 pix) { return (pix ^ (pix >> 17)) & 4095; }

    inline void insert(const rdr::U32 pix)
    {
        if (size < MAX_SIZE)
        {
            int i = hash(pix);
            while ((index[i] != 255) && (key[i] != pix))
            {
                ++i;
            }
            if (index[i] != 255)
            {
                return;
            }

            index[i] = static_cast<rdr::U8>(size);
            key[i] = pix;
            palette[static_cast<unsigned long long>(size)] = pix;
        }
        ++size;
    }

    inline int lookup(const rdr::U32 pix)
    {
        assert(size <= MAX_SIZE);
        int i = hash(pix);
        while ((index[i] != 255) && (key[i] != pix))
        {
            ++i;
        }
        if (index[i] != 255)
        {
            return index[i];
        }
        return -1;
    }

    std::array<rdr::U32, MAX_SIZE> palette;
    rdr::U8 index[4096 + MAX_SIZE];
    rdr::U32 key[4096 + MAX_SIZE];
    int size;
};
#endif

void ZRLE_ENCODE_TILE(PIXEL_T* data, int w, int h, rdr::OutStream* os);

#if BPP != 8
#define ZYWRLE_ENCODE
#include <rfb/zywrletemplate.c>

#include <array>

#endif

template <class myOutStream>
void ZRLE_ENCODE(int x, int y, int w, int h, rdr::OutStream* const os, myOutStream* const zos, void* buf EXTRA_ARGS)
{
    zos->setUnderlying(os);

    for (int ty = y; ty < y + h; ty += rfbZRLETileHeight)
    {
        int th = rfbZRLETileHeight;
        if (th > y + h - ty)
        {
            th = y + h - ty;
        }
        for (int tx = x; tx < x + w; tx += rfbZRLETileWidth)
        {
            int tw = rfbZRLETileWidth;
            if (tw > x + w - tx)
            {
                tw = x + w - tx;
            }

            GET_IMAGE_INTO_BUF(tx, ty, tw, th, buf)

            ZRLE_ENCODE_TILE(reinterpret_cast<PIXEL_T*>(buf), tw, th, zos);
        }
    }
    zos->flush();
}


void ZRLE_ENCODE_TILE(PIXEL_T* const data, const int w, const int h, rdr::OutStream* const os)
{
    // First find the palette and the number of runs

    PaletteHelper ph;

    int runs{ 0 };
    int singlePixels{ 0 };

    PIXEL_T* ptr = data;
    PIXEL_T* end = ptr + h * w;
    *end = ~*(end - 1); // one past the end is different so the while loop ends

    while (ptr < end)
    {
        PIXEL_T pix = *ptr;
        if (*++ptr != pix)
        {
            ++singlePixels;
        }
        else
        {
            while (*++ptr == pix)
                ;
            ++runs;
        }
        ph.insert(pix);
    }

    //fprintf(stderr,"runs %d, single pixels %d, paletteSize %d\n",
    //        runs, singlePixels, ph.size);

    // Solid tile is a special case

    if (ph.size == 1)
    {
        os->writeU8(1);
        os->WRITE_PIXEL(static_cast<rdr::U8>(ph.palette[0U]));
        return;
    }

    // Try to work out whether to use RLE and/or a palette. We do this by
    // estimating the number of bytes which will be generated and picking the
    // method which results in the fewest bytes. Of course this may not result
    // in the fewest bytes after compression...

    bool useRle{ false };
    bool usePalette{ false };

    int estimatedBytes = w * h * (BPPOUT / 8); // start assuming raw

#if BPP != 8
    if ((zywrle_level > 0) && !(zywrle_level & 0x80))
    {
        estimatedBytes >>= zywrle_level;
    }
#endif

    int plainRleBytes = ((BPPOUT / 8) + 1) * (runs + singlePixels);

    if (plainRleBytes < estimatedBytes)
    {
        useRle = true;
        estimatedBytes = plainRleBytes;
    }

    if (ph.size < 128)
    {
        int paletteRleBytes = (BPPOUT / 8) * ph.size + 2 * runs + singlePixels;

        if (paletteRleBytes < estimatedBytes)
        {
            useRle = true;
            usePalette = true;
            estimatedBytes = paletteRleBytes;
        }

        if (ph.size < 17)
        {
            int packedBytes = ((BPPOUT / 8) * ph.size + w * h * bitsPerPackedPixel[ph.size - 1] / 8);

            if (packedBytes < estimatedBytes)
            {
                useRle = false;
                usePalette = true;
                estimatedBytes = packedBytes;
            }
        }
    }

    if (!usePalette)
    {
        ph.size = 0;
    }

    os->writeU8(static_cast<rdr::U8>((useRle ? 128 : 0) | ph.size));

    for (int i{ 0 }; i < ph.size; ++i)
    {
        os->WRITE_PIXEL(static_cast<rdr::U8>(ph.palette[static_cast<unsigned long long>(i)]));
    }

    if (useRle)
    {

        PIXEL_T* ptrInner = data;
        PIXEL_T* endInner = ptrInner + w * h;
        PIXEL_T* runStart;
        PIXEL_T pix;
        while (ptrInner < endInner)
        {
            runStart = ptrInner;
            pix = *ptrInner++;
            while ((*ptrInner == pix) && (ptrInner < endInner))
            {
                ++ptrInner;
            }
            int len = static_cast<int>((ptrInner - runStart));
            if ((len <= 2) && usePalette)
            {
                int index = ph.lookup(pix);
                if (len == 2)
                {
                    os->writeU8(static_cast<rdr::U8>(index));
                }
                os->writeU8(static_cast<rdr::U8>(index));
                continue;
            }
            if (usePalette)
            {
                int index = ph.lookup(pix);
                os->writeU8(static_cast<rdr::U8>(index | 128));
            }
            else
            {
                os->WRITE_PIXEL(pix);
            }
            len -= 1;
            while (len >= 255)
            {
                os->writeU8(255);
                len -= 255;
            }
            os->writeU8(static_cast<rdr::U8>(len));
        }
    }
    else
    {

        // no RLE

        if (usePalette)
        {

            // packed pixels

            assert(ph.size < 17);

            const int bppp = bitsPerPackedPixel[ph.size - 1];

            PIXEL_T* ptrInner = data;

            for (int i{ 0 }; i < h; ++i)
            {
                U8 nbits = 0;
                U8 byte = 0;

                PIXEL_T* eol = ptrInner + w;

                while (ptrInner < eol)
                {
                    PIXEL_T pix = *ptrInner++;
                    U8 index = static_cast<rdr::U8>(ph.lookup(pix));
                    byte = (byte << bppp) | index;
                    nbits += bppp;
                    if (nbits >= 8)
                    {
                        os->writeU8(byte);
                        nbits = 0;
                    }
                }
                if (nbits > 0)
                {
                    byte <<= 8 - nbits;
                    os->writeU8(byte);
                }
            }
        }
        else
        {

            // raw

#if BPP != 8
            if ((zywrle_level > 0) && !(zywrle_level & 0x80))
            {
                ZYWRLE_ANALYZE(data, data, w, h, w, zywrle_level, zywrleBuf);
                zywrle_level |= 0x80;
                ZRLE_ENCODE_TILE(data, w, h, os);
                zywrle_level &= 0x7F;
            }
            else
#endif
#ifdef CPIXEL
                for (PIXEL_T* ptrInner = data; ptrInner < data + w * h; ++ptrInner)
                {
                    os->WRITE_PIXEL(*ptrInner);
                }
#else
            os->writeBytes(data, w * h * (BPPOUT / 8));
#endif
        }
    }
}

#undef PIXEL_T
#undef WRITE_PIXEL
#undef ZRLE_ENCODE
#undef ZRLE_ENCODE_TILE
#undef BPPOUT
