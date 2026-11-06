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


#include "ZstdInStream.h"
#include "Exception.h"

using namespace rdr;

enum { DEFAULT_BUF_SIZE = 16384 };

ZstdInStream::ZstdInStream(int bufSize_)
	: underlying(nullptr), bufSize(bufSize_ ? bufSize_ : DEFAULT_BUF_SIZE), offset(0),
	bytesIn(0)
{
	zstds = ZSTD_createDStream();
	unsigned int inSize = static_cast<unsigned int>(ZSTD_DStreamInSize());
	unsigned int outSize = static_cast<unsigned int>(ZSTD_DStreamOutSize());
	bufSize = inSize > outSize ? static_cast<int>(inSize) : static_cast<int>(outSize);
	bufSize = bufSize_ > bufSize ? bufSize_ : bufSize;
	outBuffer = new ZSTD_outBuffer;
	inBuffer = new ZSTD_inBuffer;	
	ZSTD_initDStream(zstds);	
	ptr = end = start = new U8[static_cast<unsigned long long>(bufSize)];
}

ZstdInStream::~ZstdInStream()
{
	ZSTD_freeDStream(zstds);
	delete outBuffer;
	delete inBuffer;
}

void ZstdInStream::setUnderlying(InStream* is, int bytesIn_)
{
	underlying = is;
	bytesIn = bytesIn_;
	ptr = end = start;
}

int ZstdInStream::pos()
{
	return static_cast<int>((offset + ptr - start));
}

void ZstdInStream::reset()
{
	ptr = end = start;
	if (!underlying) return;

	while (bytesIn > 0) {
		decompress();
		end = start; // throw away any data
	}
	underlying = nullptr;
}

int ZstdInStream::overrun(int itemSize, int nItems)
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

void ZstdInStream::decompress()
{

	outBuffer->dst = const_cast<U8*>(reinterpret_cast<const U8 *>(end));
	outBuffer->size = static_cast<size_t>(start + bufSize - end);
	outBuffer->pos = 0;

	underlying->check(1);
	inBuffer->src = underlying->getptr();
	inBuffer->size = static_cast<size_t>(underlying->getend() - underlying->getptr());
	inBuffer->pos = 0;

	if (inBuffer->size > static_cast<size_t>(bytesIn))
		inBuffer->size = static_cast<size_t>(bytesIn);

	auto result = ZSTD_decompressStream(zstds, outBuffer, inBuffer);
	if (ZSTD_isError(result))
		throw Exception(ZSTD_getErrorName(result));

	size_t consumed = static_cast<size_t>((((U8*)inBuffer->src + inBuffer->pos) - (U8*)underlying->getptr()));
	size_t produced = outBuffer->pos;

	// If a call neither consumes input nor produces output, the loop in
	// overrun() would spin forever once the compressed byte budget is used
	// up. Treat that as a truncated/corrupt stream, like the zlib and xz
	// in-streams do.
	if (consumed == 0 && produced == 0)
		throw Exception("ZstdInStream: no decompression progress (truncated input?)");

	bytesIn -= static_cast<int>(consumed);
	end = (reinterpret_cast<U8*>(outBuffer->dst) + outBuffer->pos);
	underlying->setptr(const_cast<U8*>(reinterpret_cast<const U8 *>(inBuffer->src)) + inBuffer->pos);
}
