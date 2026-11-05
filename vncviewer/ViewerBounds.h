// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2026 UltraVNC Team Members. All Rights Reserved.
//

#ifndef VIEWERBOUNDS_H__
#define VIEWERBOUNDS_H__

#include <stddef.h>
#include <type_traits>

// Checks used before trusting sizes and rectangles from the server.
namespace ViewerBounds {

constexpr unsigned long long kMaxBytes = 512ull * 1024ull * 1024ull;
constexpr unsigned long long kMaxExtent = 20000ull;

template <typename T>
inline bool Negative(T value)
{
	if constexpr (std::is_signed<T>::value)
		return value < 0;
	else
		return false;
}

template <typename T>
inline unsigned long long AsUnsigned(T value)
{
	return static_cast<unsigned long long>(value);
}

template <typename T>
inline bool payload(T size)
{
	if (Negative(size))
		return false;
	return AsUnsigned(size) <= kMaxBytes;
}

template <typename W, typename H, typename B, typename L>
inline bool pixels(W width, H height, B bytesPerPixel, L limit)
{
	if (Negative(width) || Negative(height) || Negative(bytesPerPixel) || Negative(limit))
		return false;
	if (width == 0 || height == 0 || bytesPerPixel == 0)
		return false;
	const unsigned long long w = AsUnsigned(width);
	const unsigned long long h = AsUnsigned(height);
	const unsigned long long bpp = AsUnsigned(bytesPerPixel);
	if (w > kMaxExtent || h > kMaxExtent)
		return false;
	if (w > (~0ull) / h)
		return false;
	const unsigned long long area = w * h;
	if (bpp != 0 && area > (~0ull) / bpp)
		return false;
	const unsigned long long bytes = area * bpp;
	return bytes <= AsUnsigned(limit) && bytes <= kMaxBytes;
}

template <typename W, typename H, typename B>
inline bool framebuffer(W width, H height, B bytesPerPixel)
{
	return pixels(width, height, bytesPerPixel, kMaxBytes);
}

template <typename A, typename B, typename C>
inline bool range(A offset, B count, C size)
{
	if (Negative(offset) || Negative(count) || Negative(size))
		return false;
	const unsigned long long available = AsUnsigned(size);
	const unsigned long long needed = AsUnsigned(count);
	const unsigned long long start = AsUnsigned(offset);
	if (needed > available)
		return false;
	return start <= available - needed;
}

template <typename X, typename Y, typename W, typename H, typename FW, typename FH>
inline bool rectangle(X x, Y y, W w, H h, FW fbW, FH fbH)
{
	if (Negative(x) || Negative(y) || Negative(w) || Negative(h) || Negative(fbW) || Negative(fbH))
		return false;
	const unsigned long long left = AsUnsigned(x);
	const unsigned long long top = AsUnsigned(y);
	const unsigned long long width = AsUnsigned(w);
	const unsigned long long height = AsUnsigned(h);
	const unsigned long long frameW = AsUnsigned(fbW);
	const unsigned long long frameH = AsUnsigned(fbH);
	if (left > frameW || top > frameH)
		return false;
	if (width > frameW - left || height > frameH - top)
		return false;
	return true;
}

template <typename T, typename F, typename O>
inline void clipAxis(T& pos, T& length, F limit, O& clipped)
{
	clipped = 0;
	if (Negative(length))
		length = 0;
	if (Negative(pos)) {
		clipped = static_cast<O>(-pos);
		length = static_cast<T>(length + pos);
		pos = 0;
		if (Negative(length))
			length = 0;
	}
	if (AsUnsigned(pos) > AsUnsigned(limit)) {
		pos = static_cast<T>(limit);
		length = 0;
		return;
	}
	const unsigned long long room = AsUnsigned(limit) - AsUnsigned(pos);
	if (AsUnsigned(length) > room)
		length = static_cast<T>(room);
}

} // namespace ViewerBounds

#endif
