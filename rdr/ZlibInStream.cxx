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


#include "ZlibInStream.h"
#include "Exception.h"

#ifdef _VCPKG
#include <zlib.h>
#else
#include "../zlib/zlib.h"
#endif


using namespace rdr;

enum { DEFAULT_BUF_SIZE = 16384 };

ZlibInStream::ZlibInStream(int bufSize_)
  : underlying(nullptr), bufSize(bufSize_ ? bufSize_ : DEFAULT_BUF_SIZE), offset(0),
    bytesIn(0)
{
  zs = new z_stream;
  zs->zalloc    = Z_NULL;
  zs->zfree     = Z_NULL;
  zs->opaque    = Z_NULL;
  zs->next_in   = Z_NULL;
  zs->avail_in  = 0;
  if (inflateInit(zs) != Z_OK) {
    delete zs;
    throw Exception("ZlibInStream: inflateInit failed");
  }
  ptr = end = start = new U8[static_cast<unsigned long long>(bufSize)];
}

ZlibInStream::~ZlibInStream()
{
  delete [] start;
  inflateEnd(zs);
  delete zs;
}

void ZlibInStream::setUnderlying(InStream* is, int bytesIn_)
{
  underlying = is;
  bytesIn = bytesIn_;
  ptr = end = start;
}

int ZlibInStream::pos()
{
  return static_cast<int>((offset + ptr - start));
}

void ZlibInStream::reset()
{
  ptr = end = start;
  if (!underlying) return;

  while (bytesIn > 0) {
    decompress();
    end = start; // throw away any data
  }
  underlying = nullptr;
}

int ZlibInStream::overrun(int itemSize, int nItems)
{
  if (itemSize > bufSize)
    throw Exception("ZlibInStream overrun: max itemSize exceeded");
  if (!underlying)
    throw Exception("ZlibInStream overrun: no underlying stream");

  if (end - ptr != 0)
    memmove(start, ptr, static_cast<size_t>(end - ptr));

  offset += static_cast<int>((ptr - start));
  end -= ptr - start;
  ptr = start;

  while (end - ptr < itemSize) {
    decompress();
  }

  if (itemSize * nItems > end - ptr)
    nItems = static_cast<int>(((end - ptr) / itemSize));

  return nItems;
}

// decompress() calls the decompressor once. Note that this won't necessarily
// generate any output data - it may just consume some input data.

void ZlibInStream::decompress()
{
  zs->next_out = const_cast<U8*>(reinterpret_cast<const U8 *>(end));
  zs->avail_out = static_cast<uInt>((start + bufSize - end));

  underlying->check(1);
  zs->next_in = const_cast<U8*>(reinterpret_cast<const U8 *>(underlying->getptr()));
  zs->avail_in = static_cast<uInt>((underlying->getend() - underlying->getptr()));
  if (static_cast<int>(zs->avail_in) > bytesIn)
    zs->avail_in = static_cast<uInt>(bytesIn);

  int rc = inflate(zs, Z_SYNC_FLUSH);
  if (rc != Z_OK && rc != Z_STREAM_END) {
    throw Exception("ZlibInStream: inflate failed");
  }

  bytesIn -= static_cast<int>((zs->next_in - underlying->getptr()));
  end = zs->next_out;
  underlying->setptr(zs->next_in);
}
