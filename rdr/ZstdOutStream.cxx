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


#include "ZstdOutStream.h"
#include "Exception.h"
#include <stdlib.h> 

using namespace rdr;

enum { DEFAULT_BUF_SIZE = 16384 };


// adzm - 2010-07 - Custom compression level
ZstdOutStream::ZstdOutStream(OutStream* os, int bufSize_, [[maybe_unused]] int compressionLevel)
  : underlying(os), bufSize(bufSize_ ? bufSize_ : DEFAULT_BUF_SIZE), offset(0)
{
  zstds = ZSTD_createCStream();
  unsigned int inSize = static_cast<unsigned int>(ZSTD_CStreamInSize());
  unsigned int outSize = static_cast<unsigned int>(ZSTD_CStreamOutSize());
  bufSize = inSize > outSize ? static_cast<int>(inSize) : static_cast<int>(outSize);
  bufSize = bufSize_ > bufSize ? bufSize_ : bufSize;
  ZSTD_initCStream(zstds, ZSTD_CLEVEL_DEFAULT);
  outBuffer = new ZSTD_outBuffer;
  inBuffer = new ZSTD_inBuffer;
  ptr = start = new U8[static_cast<unsigned long long>(bufSize)];
  end = start + bufSize;
}

ZstdOutStream::~ZstdOutStream()
{
  ZSTD_freeCStream(zstds);
  delete outBuffer;
  delete inBuffer;
  delete[] start;
}

void ZstdOutStream::setUnderlying(OutStream* os)
{
  underlying = os;
}

int ZstdOutStream::length()
{
  return static_cast<int>((offset + ptr - start));
}


void ZstdOutStream::flush()
{	
	inBuffer->src = start;
	inBuffer->size = static_cast<size_t>(ptr - start);
	inBuffer->pos = 0;
	unsigned int rc = 0;

	while (inBuffer->size != 0) {
		do {
			underlying->check(1);
			outBuffer->dst = underlying->getptr();
			outBuffer->size = static_cast<size_t>(underlying->getend() - underlying->getptr());
			outBuffer->pos = 0;

			rc = static_cast<unsigned int>(ZSTD_compressStream2(zstds, outBuffer, inBuffer, ZSTD_e_flush));
			if (ZSTD_isError(rc)) {
				auto error = ZSTD_getErrorName(rc);
				throw Exception(error);
			}
			inBuffer->src = const_cast<U8*>(reinterpret_cast<const U8 *>(inBuffer->src)) + inBuffer->pos;
			inBuffer->size -= inBuffer->pos;
			inBuffer->pos = 0;
			outBuffer->dst = reinterpret_cast<U8*>(outBuffer->dst) + outBuffer->pos;
			outBuffer->size -= outBuffer->pos;
			outBuffer->pos = 0;

			underlying->setptr(reinterpret_cast<U8*>(outBuffer->dst));
		} while (outBuffer->size == 0);
	}

	if (rc != 0)
		__debugbreak();	
	offset += static_cast<int>((ptr - start));
	ptr = start;
}

int ZstdOutStream::overrun(int itemSize, int nItems)
{
	//    fprintf(stderr,"ZstdOutStream overrun\n");
	if (itemSize > bufSize)
		throw Exception("ZstdOutStream overrun: max itemSize exceeded");

	while (end - ptr < itemSize) {
		inBuffer->src = start;
		inBuffer->size = static_cast<size_t>(ptr - start);
		inBuffer->pos = 0;

		do {
			underlying->check(1);
			outBuffer->dst = underlying->getptr();
			outBuffer->size = static_cast<size_t>(underlying->getend() - underlying->getptr());
			outBuffer->pos = 0;

			auto rc = ZSTD_compressStream(zstds, outBuffer, inBuffer);
			if (ZSTD_isError(rc)) {
				auto error = ZSTD_getErrorName(rc);
				throw Exception(error);
			}
			inBuffer->src = const_cast<U8*>(reinterpret_cast<const U8 *>(inBuffer->src)) + inBuffer->pos;
			inBuffer->size -= inBuffer->pos;
			inBuffer->pos = 0;
			outBuffer->dst = reinterpret_cast<U8*>(outBuffer->dst) + outBuffer->pos;
			outBuffer->size -= outBuffer->pos;
			outBuffer->pos = 0;

			underlying->setptr(reinterpret_cast<U8*>(outBuffer->dst));
		} while (outBuffer->size == 0);
		// output buffer not full
		if (inBuffer->size == 0) {
			offset += static_cast<int>((ptr - start));
			ptr = start;
		}
		else {
			// but didn't consume all the data?  try shifting what's left to the
			// start of the buffer.
			fprintf(stderr, "z out buf not full, but in data not consumed\n");
			memmove(start, inBuffer->src, ptr - const_cast<U8*>(reinterpret_cast<const U8 *>(inBuffer->src)));
			offset += static_cast<int>(((U8*)inBuffer->src - start));
			ptr -= const_cast<U8*>(reinterpret_cast<const U8 *>(inBuffer->src)) - start;
		}
	}
	if (itemSize * nItems > end - ptr)
		nItems = static_cast<int>(((end - ptr) / itemSize));
	return nItems;
}
