// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2026 UltraVNC Team Members. All Rights Reserved.
//

#ifndef VIEWERJPEGSOURCE_H__
#define VIEWERJPEGSOURCE_H__

extern "C" {
#ifdef _VCPKG
#include <jpeglib.h>
#else
#include "libjpeg-turbo-win/jpeglib.h"
#endif
}

// Shared jpeg_source_mgr callbacks. The whole JPEG buffer is supplied up
// front, so a request for more bytes means the stream is truncated.
namespace ViewerJpeg {

inline void initSource(j_decompress_ptr)
{
}

inline boolean fillInputBuffer(j_decompress_ptr cinfo)
{
	static const JOCTET eoi[2] = { 0xFF, JPEG_EOI };
	cinfo->src->next_input_byte = eoi;
	cinfo->src->bytes_in_buffer = 2;
	return TRUE;
}

inline void skipInputData(j_decompress_ptr cinfo, long num_bytes)
{
	if (num_bytes <= 0 || cinfo->src == NULL)
		return;
	if (num_bytes >= (long)cinfo->src->bytes_in_buffer) {
		cinfo->src->next_input_byte += cinfo->src->bytes_in_buffer;
		cinfo->src->bytes_in_buffer = 0;
	} else {
		cinfo->src->next_input_byte += (size_t)num_bytes;
		cinfo->src->bytes_in_buffer -= (size_t)num_bytes;
	}
}

}

#endif
