// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2000 Const Kaplinsky. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// vncHTTPConnect.cpp

// Implementation of the HTTP server class

#include "stdhdrs.h"

#include "vnchttpconnect.h"

#include "resource.h"
#include "SchannelTLS.h"
#include "SettingsManager.h"
#include "vncserver.h"
#include "vsocket.h"

#include <omnithread.h>

#include <process.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctype.h>
#include <stdint.h>
#include <vector>
//	[v1.0.2-jp1 fix]

extern HINSTANCE hInstResDLL;
extern bool fShutdownOrdered;

// HTTP messages / message formats
const char HTTP_MSG_OK[] = "HTTP/1.0 200 OK\r\n\r\n";

const char HTTP_MSG_NOSOCKCONN[] = "<HTML>\n"
                                   "  <HEAD><TITLE>UltraVNC desktop</TITLE></HEAD>\n"
                                   "  <BODY>\n"
                                   "    <H1>Connections Disabled</H1>\n"
                                   "    The requested desktop is not configured to accept incoming connections.\n"
                                   "  </BODY>\n"
                                   "</HTML>\n";

const char HTTP_MSG_NOSUCHFILE[] = "HTTP/1.0 404 Not Found\r\n\r\n"
                                   "<HTML>\n"
                                   "  <HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"
                                   "  <BODY>\n"
                                   "    <H1>Not Found</H1>\n"
                                   "    The requested file could not be found.\n"
                                   "  </BODY>\n"
                                   "</HTML>\n";



// Filename to resource ID mappings for the WebViewer files:
typedef struct _FileToResourceMap
{
    char* filename;
    char* type;
    int resourceID;
} FileMap;


const FileMap filemapping[] = {
    { "/core/base64.js", "JS", IDR_WEBVIEWER_CORE_BASE64_JS },
    { "/core/clipboard.js", "JS", IDR_WEBVIEWER_CORE_CLIPBOARD_JS },
    { "/core/crypto/aes.js", "JS", IDR_WEBVIEWER_CORE_CRYPTO_AES_JS },
    { "/core/crypto/bigint.js", "JS", IDR_WEBVIEWER_CORE_CRYPTO_BIGINT_JS },
    { "/core/crypto/crypto.js", "JS", IDR_WEBVIEWER_CORE_CRYPTO_CRYPTO_JS },
    { "/core/crypto/des.js", "JS", IDR_WEBVIEWER_CORE_CRYPTO_DES_JS },
    { "/core/crypto/dh.js", "JS", IDR_WEBVIEWER_CORE_CRYPTO_DH_JS },
    { "/core/crypto/md5.js", "JS", IDR_WEBVIEWER_CORE_CRYPTO_MD5_JS },
    { "/core/crypto/rsa.js", "JS", IDR_WEBVIEWER_CORE_CRYPTO_RSA_JS },
    { "/core/decoders/copyrect.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_COPYRECT_JS },
    { "/core/decoders/h264.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_H264_JS },
    { "/core/decoders/hextile.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_HEXTILE_JS },
    { "/core/decoders/jpeg.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_JPEG_JS },
    { "/core/decoders/raw.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_RAW_JS },
    { "/core/decoders/rre.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_RRE_JS },
    { "/core/decoders/tight.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_TIGHT_JS },
    { "/core/decoders/tightpng.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_TIGHTPNG_JS },
    { "/core/decoders/zlib.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_ZLIB_JS },
    { "/core/decoders/zrle.js", "JS", IDR_WEBVIEWER_CORE_DECODERS_ZRLE_JS },
    { "/core/deflator.js", "JS", IDR_WEBVIEWER_CORE_DEFLATOR_JS },
    { "/core/display.js", "JS", IDR_WEBVIEWER_CORE_DISPLAY_JS },
    { "/core/encodings.js", "JS", IDR_WEBVIEWER_CORE_ENCODINGS_JS },
    { "/core/inflator.js", "JS", IDR_WEBVIEWER_CORE_INFLATOR_JS },
    { "/core/input/domkeytable.js", "JS", IDR_WEBVIEWER_CORE_INPUT_DOMKEYTABLE_JS },
    { "/core/input/fixedkeys.js", "JS", IDR_WEBVIEWER_CORE_INPUT_FIXEDKEYS_JS },
    { "/core/input/gesturehandler.js", "JS", IDR_WEBVIEWER_CORE_INPUT_GESTUREHANDLER_JS },
    { "/core/input/keyboard.js", "JS", IDR_WEBVIEWER_CORE_INPUT_KEYBOARD_JS },
    { "/core/input/keysym.js", "JS", IDR_WEBVIEWER_CORE_INPUT_KEYSYM_JS },
    { "/core/input/keysymdef.js", "JS", IDR_WEBVIEWER_CORE_INPUT_KEYSYMDEF_JS },
    { "/core/input/util.js", "JS", IDR_WEBVIEWER_CORE_INPUT_UTIL_JS },
    { "/core/input/vkeys.js", "JS", IDR_WEBVIEWER_CORE_INPUT_VKEYS_JS },
    { "/core/input/xtscancodes.js", "JS", IDR_WEBVIEWER_CORE_INPUT_XTSCANCODES_JS },
    { "/core/ra2.js", "JS", IDR_WEBVIEWER_CORE_RA2_JS },
    { "/core/rfb.js", "JS", IDR_WEBVIEWER_CORE_RFB_JS },
    { "/core/util/browser.js", "JS", IDR_WEBVIEWER_CORE_UTIL_BROWSER_JS },
    { "/core/util/cursor.js", "JS", IDR_WEBVIEWER_CORE_UTIL_CURSOR_JS },
    { "/core/util/element.js", "JS", IDR_WEBVIEWER_CORE_UTIL_ELEMENT_JS },
    { "/core/util/events.js", "JS", IDR_WEBVIEWER_CORE_UTIL_EVENTS_JS },
    { "/core/util/eventtarget.js", "JS", IDR_WEBVIEWER_CORE_UTIL_EVENTTARGET_JS },
    { "/core/util/int.js", "JS", IDR_WEBVIEWER_CORE_UTIL_INT_JS },
    { "/core/util/logging.js", "JS", IDR_WEBVIEWER_CORE_UTIL_LOGGING_JS },
    { "/core/util/strings.js", "JS", IDR_WEBVIEWER_CORE_UTIL_STRINGS_JS },
    { "/core/websock.js", "JS", IDR_WEBVIEWER_CORE_WEBSOCK_JS },
    { "/index.html", "HTML", IDR_WEBVIEWER_INDEX_HTML },
    { "/", "HTML", IDR_WEBVIEWER_INDEX_HTML },
    { "/vendor/pako/lib/utils/common.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_UTILS_COMMON_JS },
    { "/vendor/pako/lib/zlib/adler32.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_ADLER32_JS },
    { "/vendor/pako/lib/zlib/constants.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_CONSTANTS_JS },
    { "/vendor/pako/lib/zlib/crc32.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_CRC32_JS },
    { "/vendor/pako/lib/zlib/deflate.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_DEFLATE_JS },
    { "/vendor/pako/lib/zlib/gzheader.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_GZHEADER_JS },
    { "/vendor/pako/lib/zlib/inffast.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_INFFAST_JS },
    { "/vendor/pako/lib/zlib/inflate.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_INFLATE_JS },
    { "/vendor/pako/lib/zlib/inftrees.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_INFTREES_JS },
    { "/vendor/pako/lib/zlib/messages.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_MESSAGES_JS },
    { "/vendor/pako/lib/zlib/trees.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_TREES_JS },
    { "/vendor/pako/lib/zlib/zstream.js", "JS", IDR_WEBVIEWER_VENDOR_PAKO_LIB_ZLIB_ZSTREAM_JS },
};





const int filemappingsize = sizeof(filemapping) / sizeof(filemapping[0U]);

// -------------------------------------------------------------------------
// WebSocket helpers: handshake (SHA-1 + Base64) and framing.
// The HTTP server uses these to tunnel the browser's binary VNC stream to
// the local RFB port, replacing the obsolete Java applet.
// -------------------------------------------------------------------------

static const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void WebBase64Encode(const unsigned char* const in, const std::size_t len, char* const out)
{
    std::size_t i{ 0U }, j = 0;
    while (i < len)
    {
        std::size_t start = i;
        unsigned const char b0 = in[i++];
        unsigned const char b1 = (i < len) ? in[i++] : 0;
        unsigned const char b2 = (i < len) ? in[i++] : 0;
        out[j++] = base64Chars[(b0 >> 2) & 0x3F];
        out[j++] = base64Chars[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)];
        out[j++] = (i - start >= 2) ? base64Chars[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)] : '=';
        out[j++] = (i - start == 3) ? base64Chars[b2 & 0x3F] : '=';
    }
    out[j] = 0;
}

// Simple big-endian 32-bit store (SHA-1 length words are big-endian).
static inline void PUT32(unsigned char* const p, const unsigned v)
{
    p[0U] = static_cast<unsigned char>((v >> 24));
    p[1U] = static_cast<unsigned char>((v >> 16));
    p[2U] = static_cast<unsigned char>((v >> 8));
    p[3U] = static_cast<unsigned char>((v));
}
static inline unsigned ROL(const unsigned v, const unsigned n) { return (v << n) | (v >> (32 - n)); }

static void WebSHA1Hash(const unsigned char* const data, std::size_t len, unsigned char hash[20U])
{
    std::array<unsigned, 5U> h{ 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    std::array<unsigned char, 64U> buf;
    std::size_t idx{ 0U };
    std::uint64_t bitlen = (static_cast<std::uint64_t>(len)) * 8;

    // Process full 64-byte blocks.
    while (len >= 64)
    {
        std::memcpy(buf.data(), data + idx, 64);
        idx += 64;
        len -= 64;
        std::array<unsigned, 80U> w;
        for (int i{ 0 }; i < 16; ++i)
        {
            w[static_cast<std::size_t>(i)] = (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4)]) << 24) | (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 1)]) << 16) | (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 2)]) << 8) | static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 3)]);
        }
        for (int i{ 16 }; i < 80; ++i)
        {
            w[static_cast<unsigned long long>(i)] = ROL(w[static_cast<unsigned long long>(i - 3)] ^ w[static_cast<unsigned long long>(i - 8)] ^ w[static_cast<unsigned long long>(i - 14)] ^ w[static_cast<unsigned long long>(i - 16)], 1);
        }

        unsigned a = h[0U], b = h[1U], c = h[2U], d = h[3U], e = h[4U];
        for (int i{ 0 }; i < 80; ++i)
        {
            unsigned f, k;
            if (i < 20)
            {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            unsigned t = ROL(a, 5) + f + e + k + w[static_cast<unsigned long long>(i)];
            e = d;
            d = c;
            c = ROL(b, 30);
            b = a;
            a = t;
        }
        h[0U] += a;
        h[1U] += b;
        h[2U] += c;
        h[3U] += d;
        h[4U] += e;
    }

    // Final block with padding.
    std::memset(buf.data(), 0, 64);
    std::memcpy(buf.data(), data + idx, len);
    buf[len] = 0x80;
    if (len >= 56)
    {
        // Need an extra block for length.
        std::array<unsigned, 80U> w;
        for (int i{ 0 }; i < 16; ++i)
        {
            w[static_cast<std::size_t>(i)] = (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4)]) << 24) | (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 1)]) << 16) | (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 2)]) << 8) | static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 3)]);
        }
        for (int i{ 16 }; i < 80; ++i)
        {
            w[static_cast<unsigned long long>(i)] = ROL(w[static_cast<unsigned long long>(i - 3)] ^ w[static_cast<unsigned long long>(i - 8)] ^ w[static_cast<unsigned long long>(i - 14)] ^ w[static_cast<unsigned long long>(i - 16)], 1);
        }
        unsigned a = h[0U], b = h[1U], c = h[2U], d = h[3U], e = h[4U];
        for (int i{ 0 }; i < 80; ++i)
        {
            unsigned f, k;
            if (i < 20)
            {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            unsigned t = ROL(a, 5) + f + e + k + w[static_cast<unsigned long long>(i)];
            e = d;
            d = c;
            c = ROL(b, 30);
            b = a;
            a = t;
        }
        h[0U] += a;
        h[1U] += b;
        h[2U] += c;
        h[3U] += d;
        h[4U] += e;
        std::memset(buf.data(), 0, 56);
    }
    PUT32(buf.data() + 56, static_cast<unsigned>((bitlen >> 32)));
    PUT32(buf.data() + 60, static_cast<unsigned>(bitlen));

    {
        std::array<unsigned, 80U> w;
        for (int i{ 0 }; i < 16; ++i)
        {
            w[static_cast<std::size_t>(i)] = (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4)]) << 24) | (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 1)]) << 16) | (static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 2)]) << 8) | static_cast<unsigned>(buf[static_cast<std::size_t>(i * 4 + 3)]);
        }
        for (int i{ 16 }; i < 80; ++i)
        {
            w[static_cast<unsigned long long>(i)] = ROL(w[static_cast<unsigned long long>(i - 3)] ^ w[static_cast<unsigned long long>(i - 8)] ^ w[static_cast<unsigned long long>(i - 14)] ^ w[static_cast<unsigned long long>(i - 16)], 1);
        }
        unsigned a = h[0U], b = h[1U], c = h[2U], d = h[3U], e = h[4U];
        for (int i{ 0 }; i < 80; ++i)
        {
            unsigned f, k;
            if (i < 20)
            {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            unsigned t = ROL(a, 5) + f + e + k + w[static_cast<unsigned long long>(i)];
            e = d;
            d = c;
            c = ROL(b, 30);
            b = a;
            a = t;
        }
        h[0U] += a;
        h[1U] += b;
        h[2U] += c;
        h[3U] += d;
        h[4U] += e;
    }

    for (int i{ 0 }; i < 5; ++i)
    {
        PUT32(hash + i * 4, h[static_cast<unsigned long long>(i)]);
    }
}

static BOOL PerformWebSocketHandshake(VSocket* const socket, const char* const headers)
{
    const char* key = strstr(headers, "Sec-WebSocket-Key:");
    if (!key)
    {
        key = strstr(headers, "sec-websocket-key:");
    }
    if (!key)
    {
        return FALSE;
    }
    key += 18;
    while ((*key == ' ') || (*key == '\t') || (*key == ':'))
    {
        ++key;
    }
    const char* end = key;
    while (*end && (*end != '\r') && (*end != '\n'))
    {
        ++end;
    }
    if (end == key)
    {
        return FALSE;
    }

    std::array<char, 128U> wsKey;
    std::size_t keyLen = static_cast<std::size_t>((end - key));
    if (keyLen > sizeof(wsKey) - 1)
    {
        keyLen = sizeof(wsKey) - 1;
    }
    std::memcpy(wsKey.data(), key, keyLen);
    wsKey[keyLen] = 0;

    const char* magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::array<unsigned char, 128 + 36> shaInput;
    std::memcpy(shaInput.data(), wsKey.data(), keyLen);
    std::memcpy(shaInput.data() + keyLen, magic, 36);
    std::array<unsigned char, 20U> hash;
    WebSHA1Hash(shaInput.data(), keyLen + 36, hash.data());

    std::array<char, 64U> accept;
    WebBase64Encode(hash.data(), 20, accept.data());

    std::array<char, 512U> response;
    ::sprintf_s(response.data(), response.size(), "HTTP/1.1 101 Switching Protocols\r\n"
              "Upgrade: websocket\r\n"
              "Connection: Upgrade\r\n"
              "Sec-WebSocket-Accept: %s\r\n"
              "\r\n", accept.data());
    return socket->SendExactHTTP(response.data(), static_cast<VCard>(std::strlen(response.data())));
}

static BOOL WebSocketSendFrame(VSocket* const socket, const unsigned char opcode, const char* const payload, const int len)
{
    std::array<char, 10U> hdr;
    int hdrLen{ 2 };
    hdr[0U] = 0x80 | (opcode & 0x0f); // FIN + opcode
    if (len < 126)
    {
        hdr[1U] = static_cast<char>(len);
    }
    else if (len < 65536)
    {
        hdr[1U] = 126;
        hdr[2U] = static_cast<char>((len >> 8));
        hdr[3U] = static_cast<char>(len);
        hdrLen = 4;
    }
    else
    {
        hdr[1U] = 127;
        for (int i{ 0 }; i < 8; ++i)
        {
            hdr[static_cast<unsigned long long>(2 + i)] = static_cast<char>((len >> (56 - i * 8)));
        }
        hdrLen = 10;
    }
    if (!socket->SendExactHTTP(hdr.data(), static_cast<VCard>(hdrLen)))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("WS send frame: header send failed\n"));
        return FALSE;
    }
    if ((len > 0) && !socket->SendExactHTTP(payload, static_cast<VCard>(len)))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("WS send frame: payload send failed\n"));
        return FALSE;
    }
    return TRUE;
}

static BOOL WebSocketRecvFrame(VSocket* const socket, char* const payload, const int maxPayload, int* outLen,
                               CRITICAL_SECTION* const sendLock = nullptr)
{
    int totalLen{ 0 };

    for (;;)
    {
        std::array<char, 2U> hdr;
        if (!socket->ReadExactHTTP(hdr.data(), 2))
        {
            return FALSE;
        }
        BOOL fin = (hdr[0U] & 0x80) != 0;
        int opcode = hdr[0U] & 0x0f;
        BOOL masked = (hdr[1U] & 0x80) != 0;
        std::uint64_t len = static_cast<unsigned char>((hdr[1U] & 0x7f));

        std::array<char, 8U> lenBytes;
        if (len == 126)
        {
            if (!socket->ReadExactHTTP(lenBytes.data(), 2))
            {
                return FALSE;
            }
            len = static_cast<std::uint64_t>((static_cast<unsigned char>(lenBytes[0U]) << 8) | static_cast<unsigned char>(lenBytes[1U]));
        }
        else if (len == 127)
        {
            if (!socket->ReadExactHTTP(lenBytes.data(), 8))
            {
                return FALSE;
            }
            len = 0;
            for (int i{ 0 }; i < 8; ++i)
            {
                len = (len << 8) | static_cast<unsigned char>(lenBytes[static_cast<unsigned long long>(i)]);
            }
        }

        std::array<char, 4U> mask{};
        if (masked)
        {
            if (!socket->ReadExactHTTP(mask.data(), 4))
            {
                return FALSE;
            }
        }

        // Guard against overflow: compare as unsigned only. A 64-bit length
        // with the top bit set is illegal per RFC 6455 and must not wrap to
        // a negative value that passes a signed check.
        if ((len >> 63) != 0)
        {
            return FALSE;
        }
        std::uint64_t remaining = (totalLen < maxPayload) ? static_cast<std::uint64_t>((maxPayload - totalLen)) : 0;
        if (len > remaining)
        {
            return FALSE;
        }

        // Control frames (close/ping/pong) must be <= 125 bytes and may not
        // be fragmented (RFC 6455 5.5).
        if ((opcode >= 0x08) && (!fin || (len > 125)))
        {
            return FALSE;
        }

        if (len > 0)
        {
            if (!socket->ReadExactHTTP(payload + totalLen, static_cast<VCard>(len)))
            {
                return FALSE;
            }
            if (masked)
            {
                for (std::uint64_t i{ 0U }; i < len; ++i)
                {
                    payload[static_cast<std::uint64_t>(totalLen) + i] ^= mask[i & 3];
                }
            }
            totalLen += static_cast<int>(len);
        }

        // Control frames (may arrive mid-fragmentation): handle inline.
        if (opcode == 0x08)
        {
            if (sendLock)
            {
                ::EnterCriticalSection(sendLock);
            }
            WebSocketSendFrame(socket, 0x08, nullptr, 0);
            if (sendLock)
            {
                ::LeaveCriticalSection(sendLock);
            }
            return FALSE;
        }
        if (opcode == 0x09)
        {
            // Pong the ping, then keep reading the current message
            const char* pingPayload = payload + (totalLen - static_cast<int>(len));
            if (sendLock)
            {
                ::EnterCriticalSection(sendLock);
            }
            WebSocketSendFrame(socket, 0x0A, pingPayload, static_cast<int>(len));
            if (sendLock)
            {
                ::LeaveCriticalSection(sendLock);
            }
            totalLen -= static_cast<int>(len); // discard ping payload from buffer
            continue;
        }
        if (opcode == 0x0A)
        {
            // Unsolicited pong — discard and keep reading
            totalLen -= static_cast<int>(len);
            continue;
        }

        // If FIN is set this is the last (or only) fragment
        if (fin)
        {
            break;
        }

        // Continuation frame expected next (opcode == 0x00 for continuations)
    }

    *outLen = totalLen;
    return TRUE;
}

static BOOL WebSocketSendBinary(VSocket* const socket, const char* const data, const int len)
{ return WebSocketSendFrame(socket, 0x02, data, len); }

// Forward declarations
static void BridgeWebSocketToRFB(VSocket* clientSocket, vncServer* server);

static const char* GetResourceMimeType(const char* const resType)
{
    if (::_stricmp(resType, "HTML") == 0)
    {
        return "text/html";
    }
    if (::_stricmp(resType, "CSS") == 0)
    {
        return "text/css";
    }
    if (::_stricmp(resType, "JS") == 0)
    {
        return "application/javascript";
    }
    return "application/octet-stream";
}

static LPCTSTR GetResourceType(const char* const resType)
{
    if (::_stricmp(resType, "HTML") == 0)
    {
        return RT_HTML;
    }
    return resType;
}

// The function for the spawned thread to run
class vncHTTPConnectThread : public omni_thread
{
public:
    explicit vncHTTPConnectThread() : m_shutdown(FALSE), m_tlsEnabled{ false }, m_server{ nullptr }, m_socket{ nullptr }
    { std::memset(m_tlsCertThumbprint.data(), 0, sizeof(m_tlsCertThumbprint)); }

    // Init routine
    virtual BOOL Init(VSocket* socket, vncServer* server);
    void Inithttp(vncServer* const svr) { m_server = svr; }

    // Code to be executed by the thread
    virtual void* run_undetached(void* const arg);
    // Routines to handle HTTP requests
    virtual void DoHTTP(VSocket* socket);
    virtual char* ReadLine(VSocket* socket, char delimiter);


    // Fields used internally
    BOOL m_shutdown;

    // TLS mode: if true, perform SChannel handshake before HTTP
    bool m_tlsEnabled;
    std::array<char, 41U> m_tlsCertThumbprint;

protected:
    vncServer* m_server;
    VSocket* m_socket;
};


// Added for HTTP-via-RFB. This function is called when a connection is
// accepted on the RFB port. If the client sends an HTTP request we
// handle it here and return TRUE. Otherwise we return
// FALSE and the caller continues with the RFB handshake.
VBool maybeHandleHTTPRequest(VSocket* const sock, vncServer* const svr)
{
    if (!sock->ReadSelect(2000))
    {
        return false;
    }

    // Client is sending data. Create a vncHTTPConnectThread to
    // handle it.
    vncHTTPConnectThread http;
    http.Inithttp(svr);
    http.DoHTTP(sock);
    sock->Shutdown();
    sock->Close();
    delete sock;
    return true;
}


// Method implementations
BOOL vncHTTPConnectThread::Init(VSocket* socket, vncServer* server)
{
    // Save the server pointer
    m_server = server;

    // Save the socket pointer
    m_socket = socket;

    // Start the thread (TLS fields may already be set by InitTLS before this call)
    m_shutdown = FALSE;
    start_undetached();

    return TRUE;
}

// Code to be executed by the thread
void* vncHTTPConnectThread::run_undetached([[maybe_unused]] void* const arg)
{

    // Go into a loop, listening for connections on the given socket
    VSocket* new_socket = nullptr;
    while (!m_shutdown)
    {
        // Accept an incoming connection
        VSocket* new_socketInner = m_socket->Accept();
        if (new_socketInner == nullptr)
        {
            break;
        }
        if (m_shutdown)
        {
            delete new_socketInner;
            break;
        }
        vnclog.Print(LL_CLIENTS, VNCLOG("HTTP client connected\n"));
        // Successful accept - perform the transaction
        new_socketInner->SetTimeout(15000); //ms

        if (m_tlsEnabled)
        {
            // Perform TLS handshake before HTTP processing
            SchannelTLS tls;
            bool certOk{ false };
            if (m_tlsCertThumbprint[0U])
            {
                certOk = tls.LoadCertByThumbprint(m_tlsCertThumbprint.data());
            }
            // Missing, expired or too-weak certificate: fall back to a fresh self-signed one.
            if (!certOk)
            {
                std::array<char, 41U> thumbprint;
                certOk = tls.EnsureSelfSignedCert("CN=UltraVNC-WebViewer", "UltraVNC-WebViewer", thumbprint.data(),
                                                  settings->getTLSCertKeyBits());
                if (certOk)
                {
                    settings->setTLSCertThumbprint(thumbprint.data());
                    settings->save();
                    ::strncpy_s(m_tlsCertThumbprint.data(), m_tlsCertThumbprint.size(), thumbprint.data(), 40);
                }
            }

            if (certOk && tls.ServerHandshake(new_socketInner))
            {
                // Install TLS callbacks so all HTTP/WebSocket I/O is encrypted
                VSocket::TLSCallbacks cb;
                cb.send = [&tls, new_socketInner](const char* b, int l) -> bool { return tls.Send(new_socketInner, b, l); };
                cb.recv = [&tls, new_socketInner](char* b, int l, int* got) -> bool { return tls.Recv(new_socketInner, b, l, got); };
                new_socketInner->SetTLSCallbacks(&cb);

                DoHTTP(new_socketInner);

                new_socketInner->SetTLSCallbacks(nullptr);
                tls.Shutdown(new_socketInner);
            }
            else
            {
                vnclog.Print(LL_CONNERR, VNCLOG("TLS: handshake failed, dropping connection\n"));
            }
        }
        else
        {
            DoHTTP(new_socketInner);
        }

        // And close the client
        new_socketInner->Shutdown();
        new_socketInner->Close();
        delete new_socketInner;
        new_socketInner = nullptr;
    }
    if (new_socket)
    {
        delete new_socket;
    }

    return nullptr;
}

struct WebSocketBridgeContext
{
    VSocket* client;
    VSocket* rfb;
    volatile BOOL running;
    CRITICAL_SECTION sendLock;
};

static unsigned __stdcall WSClientToRFB(void* const arg)
{
    WebSocketBridgeContext* ctx = reinterpret_cast<WebSocketBridgeContext*>(arg);
    static const int WS_MAX_PAYLOAD = 256 * 1024;
    std::vector<char> payloadBuf(WS_MAX_PAYLOAD);
    char* payload = payloadBuf.data();
    while (ctx->running)
    {
        int len{ 0 };
        if (!WebSocketRecvFrame(ctx->client, payload, WS_MAX_PAYLOAD, &len, &ctx->sendLock))
        {
            ctx->running = FALSE;
            break;
        }
        if (len > 0)
        {
            if (!ctx->rfb->SendExact(payload, static_cast<VCard>(len)))
            {
                vnclog.Print(LL_CONNERR, VNCLOG("WS->RFB: SendExact failed\n"));
                ctx->running = FALSE;
                break;
            }
        }
    }
    return 0;
}

static unsigned __stdcall WSRFBToClient(void* const arg)
{
    WebSocketBridgeContext* ctx = reinterpret_cast<WebSocketBridgeContext*>(arg);
    static const int RFB_BUF = 256 * 1024;
    std::vector<char> buf(RFB_BUF);
    while (ctx->running)
    {
        VInt r = ctx->rfb->Read(buf.data(), RFB_BUF);
        if (r <= 0)
        {
            ctx->running = FALSE;
            break;
        }
        ::EnterCriticalSection(&ctx->sendLock);
        BOOL ok = WebSocketSendBinary(ctx->client, buf.data(), r);
        ::LeaveCriticalSection(&ctx->sendLock);
        if (!ok)
        {
            vnclog.Print(LL_CONNERR, VNCLOG("RFB->WS: WebSocketSendBinary failed\n"));
            ctx->running = FALSE;
            break;
        }
    }
    return 0;
}

static void BridgeWebSocketToRFB(VSocket* const clientSocket, [[maybe_unused]] vncServer* const server)
{
    // Connect a second socket to the local RFB listener so that the
    // browser's WebSocket stream is forwarded to the VNC server.
    VSocket* rfbSocket = new VSocket();
    if (!rfbSocket->CreateConnect("127.0.0.1", static_cast<VCard>(settings->getPortNumber())))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("failed to connect WebSocket bridge to RFB port %d\n"), settings->getPortNumber());
        delete rfbSocket;
        return;
    }

    WebSocketBridgeContext ctx;
    ctx.client = clientSocket;
    ctx.rfb = rfbSocket;
    ctx.running = TRUE;
    ::InitializeCriticalSection(&ctx.sendLock);

    unsigned int tid1{ 0U }, tid2 = 0;
    HANDLE h1 = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, WSClientToRFB, &ctx, 0, &tid1));
    HANDLE h2 = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, WSRFBToClient, &ctx, 0, &tid2));

    if (h1 && h2)
    {
        std::array<HANDLE, 2U> handles{ h1, h2 };
        // Wait for either direction to fail.
        [[maybe_unused]] const DWORD waitResult = ::WaitForMultipleObjects(2, handles.data(), FALSE, INFINITE);
        // Force the other direction out of its read by shutting both sockets down.
        clientSocket->Shutdown();
        rfbSocket->Shutdown();
        // Wait for both threads to finish.
        ::WaitForMultipleObjects(2, handles.data(), TRUE, INFINITE);
    }
    else
    {
        vnclog.Print(LL_CONNERR, VNCLOG("BridgeWebSocketToRFB: failed to create bridge threads\n"));
    }
    if (h1)
    {
        ::CloseHandle(h1);
    }
    if (h2)
    {
        ::CloseHandle(h2);
    }

    ::DeleteCriticalSection(&ctx.sendLock);
    rfbSocket->Close();
    delete rfbSocket;
}

void vncHTTPConnectThread::DoHTTP(VSocket* const socket)

{
    std::array<char, 1024U> filename;
    char* line;

    vnclog.Print(LL_CLIENTS, VNCLOG("DoHTTP started\n"));

    // Read in the HTTP request line
    if ((line = ReadLine(socket, '\n')) == nullptr)
    {
        vnclog.Print(LL_CONNERR, VNCLOG("DoHTTP: ReadLine failed\n"));
        return;
    }

    // Scan the header for the filename and free the storage
    int result = sscanf_s(line, "GET %s ", filename.data(), 1024);
    delete[] line;
    if ((result == 0) || (result == EOF))
    {
        vnclog.Print(LL_CONNERR, VNCLOG("DoHTTP: sscanf failed\n"));
        return;
    }

    vnclog.Print(LL_CLIENTS, VNCLOG("file %s requested\n"), filename.data());

    // Read in the rest of the browser's request headers. We keep them for
    // WebSocket upgrade detection.
    std::array<char, 4096U> headers;
    int headerPos{ 0 };
    BOOL emptyline = TRUE;

    for (;;)
    {
        char c;

        if (!socket->ReadExactHTTP(&c, 1))
        {
            vnclog.Print(LL_CONNERR, VNCLOG("DoHTTP: header read failed\n"));
            return;
        }
        if (headerPos < static_cast<int>(sizeof(headers)) - 1)
        {
            headers[static_cast<unsigned long long>(headerPos++)] = c;
        }
        if (c == '\n')
        {
            if (emptyline)
            {
                break;
            }
            emptyline = TRUE;
        }
        else if (c >= ' ')
        {
            emptyline = FALSE;
        }
    }
    headers[static_cast<unsigned long long>(headerPos)] = 0;


    // Is this a WebSocket upgrade request for the RFB tunnel?
    if (strstr(headers.data(), "Upgrade: websocket") || strstr(headers.data(), "upgrade: websocket"))
    {
        if (std::strcmp(filename.data(), "/ws") == 0)
        {
            if (PerformWebSocketHandshake(socket, headers.data()))
            {
                vnclog.Print(LL_CLIENTS, VNCLOG("WebSocket tunnel established\n"));
                socket->SetTimeout(0); // No timeout during RFB session (auth needs user input)
                BridgeWebSocketToRFB(socket, m_server);
            }
            else
            {
                vnclog.Print(LL_CONNERR, VNCLOG("WebSocket handshake failed\n"));
            }
            return;
        }
    }

    if (filename[0U] != '/')
    {
        vnclog.Print(LL_CONNERR, VNCLOG("filename didn't begin with '/'\n"));
        socket->SendExactHTTP(HTTP_MSG_NOSUCHFILE, static_cast<const VCard>(std::strlen(HTTP_MSG_NOSUCHFILE)));
        return;
    }

    // If server is not accepting RFB connections, show a simple status page.
    if ((std::strcmp(filename.data(), "/") == 0) && !m_server->SockConnected())
    {
        vnclog.Print(LL_CLIENTS, VNCLOG("DoHTTP: sending disabled page (RFB not listening)\n"));
        if (!socket->SendExactHTTP(HTTP_MSG_OK, static_cast<VCard>(std::strlen(HTTP_MSG_OK))))
        {
            return;
        }
        socket->SendExactHTTP(HTTP_MSG_NOSOCKCONN, static_cast<const VCard>(std::strlen(HTTP_MSG_NOSOCKCONN)));
        return;
    }

    // File requested was not the index so check the mappings
    // list for a different file.

    if (settings->getRequireMSLogon())
    {

        for (int x{ 0 }; x < filemappingsize; ++x)
        {
            if (std::strcmp(filename.data(), filemapping[x].filename) == 0)
            {
                HRSRC resource;
                HGLOBAL resourcehan;
                char* resourceptr;
                int resourcesize;

                resource =
                    ::FindResource(hInstResDLL, MAKEINTRESOURCE(filemapping[x].resourceID), GetResourceType(filemapping[x].type));
                if (resource == nullptr)
                {
                    vnclog.Print(LL_CONNERR, VNCLOG("DoHTTP: resource not found for %s (id=%d type=%s)\n"),
                                 filemapping[x].filename, filemapping[x].resourceID, filemapping[x].type);
                    return;
                }

                resourcesize = static_cast<int>(::SizeofResource(hInstResDLL, resource));

                resourcehan = ::LoadResource(hInstResDLL, resource);
                if (resourcehan == nullptr)
                {
                    return;
                }

                resourceptr = reinterpret_cast<char*>(::LockResource(resourcehan));
                if (resourceptr == nullptr)
                {
                    return;
                }

                std::array<char, 512U> responseHdr;
                ::sprintf_s(responseHdr.data(), responseHdr.size(), "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
                          "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                          "Pragma: no-cache\r\n"
                          "Expires: 0\r\n\r\n", GetResourceMimeType(filemapping[x].type), resourcesize);
                if (!socket->SendExactHTTP(responseHdr.data(), static_cast<VCard>(std::strlen(responseHdr.data()))))
                {
                    return;
                }

                if (!socket->SendExactHTTP(resourceptr, static_cast<VCard>(resourcesize)))
                {
                    return;
                }

                return;
            }
        }
    }
    else
    {
        // Now search the mappings for the desired file
        for (int x{ 0 }; x < filemappingsize; ++x)
        {
            if (std::strcmp(filename.data(), filemapping[x].filename) == 0)
            {
                HRSRC resource;
                HGLOBAL resourcehan;
                char* resourceptr;
                int resourcesize;

                resource =
                    ::FindResource(hInstResDLL, MAKEINTRESOURCE(filemapping[x].resourceID), GetResourceType(filemapping[x].type));
                if (resource == nullptr)
                {
                    vnclog.Print(LL_CONNERR, VNCLOG("DoHTTP: resource not found for %s (id=%d type=%s)\n"),
                                 filemapping[x].filename, filemapping[x].resourceID, filemapping[x].type);
                    return;
                }

                resourcesize = static_cast<int>(::SizeofResource(hInstResDLL, resource));

                resourcehan = ::LoadResource(hInstResDLL, resource);
                if (resourcehan == nullptr)
                {
                    return;
                }

                resourceptr = reinterpret_cast<char*>(::LockResource(resourcehan));
                if (resourceptr == nullptr)
                {
                    return;
                }

                std::array<char, 512U> responseHdr;
                ::sprintf_s(responseHdr.data(), responseHdr.size(), "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
                          "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                          "Pragma: no-cache\r\n"
                          "Expires: 0\r\n\r\n", GetResourceMimeType(filemapping[x].type), resourcesize);
                if (!socket->SendExactHTTP(responseHdr.data(), static_cast<VCard>(std::strlen(responseHdr.data()))))
                {
                    return;
                }

                if (!socket->SendExactHTTP(resourceptr, static_cast<VCard>(resourcesize)))
                {
                    return;
                }

                return;
            }
        }
    }

    // Send the NoSuchFile notification message to the client
    vnclog.Print(LL_CONNERR, VNCLOG("DoHTTP: sending 404 for %s\n"), filename.data());
    if (!socket->SendExactHTTP(HTTP_MSG_NOSUCHFILE, static_cast<VCard>(std::strlen(HTTP_MSG_NOSUCHFILE))))
    {
        return;
    }
}

char* vncHTTPConnectThread::ReadLine(VSocket* const socket, const char delimiter)

{
    int max{ 1024 };
    // Allocate the maximum required buffer
    char* buffer = new char[static_cast<unsigned long long>(max + 1)];
    int buffpos{ 0 };

    // Read in data until a delimiter is read
    for (;;)
    {
        char c;

        if (!socket->ReadExactHTTP(&c, 1))
        {
            delete[] buffer;
            return nullptr;
        }

        if (c == delimiter)
        {
            buffer[buffpos] = 0;
            return buffer;
        }

        buffer[buffpos] = c;
        ++buffpos;

        if (buffpos == (max - 1))
        {
            buffer[buffpos] = 0;
            return buffer;
        }
    }
}

// The vncSockConnect class implementation

vncHTTPConnect::vncHTTPConnect() { m_thread = nullptr; }

vncHTTPConnect::~vncHTTPConnect()
{
    m_socket.Shutdown();

    // Join with our lovely thread
    if (m_thread != nullptr)
    {
        // *** This is a hack to force the listen thread out of the accept call,
        // because Winsock accept semantics are broken.
        (reinterpret_cast<vncHTTPConnectThread*>(m_thread))->m_shutdown = TRUE;

        VSocket socket;
        if (settings->getIPV6())
        {
            socket.CreateBindConnect("localhost", m_port);
        }
        else
        {
            socket.Create();
            socket.Bind(0);
            socket.Connect("localhost", m_port);
        }
        socket.Close();

        void* returnval;
        m_thread->join(&returnval);
        m_thread = nullptr;

        m_socket.Close();
    }
}

BOOL vncHTTPConnect::Init(vncServer* const server, UINT port)
{
    // Save the port id
    m_port = port;
    if (settings->getIPV6())
    {
        if (!m_socket.CreateBindListen(m_port, settings->getLoopbackOnly()))
        {
            return FALSE;
        }
    }
    else
    {
        // Create the listening socket
        if (!m_socket.Create())
        {
            return FALSE;
        }

        // Bind it
        if (!m_socket.Bind(m_port, settings->getLoopbackOnly()))
        {
            return FALSE;
        }

        // Set it to listen
        if (!m_socket.Listen())
        {
            return FALSE;
        }
    }
    // Create the new thread
    m_thread = new vncHTTPConnectThread;
    if (m_thread == nullptr)
    {
        return FALSE;
    }

    // And start it running
    return (reinterpret_cast<vncHTTPConnectThread*>(m_thread))->Init(&m_socket, server);
}

BOOL vncHTTPConnect::InitTLS(vncServer* const server, UINT port, const char* certThumbprint)
{
    m_port = port;
    if (settings->getIPV6())
    {
        if (!m_socket.CreateBindListen(m_port, settings->getLoopbackOnly()))
        {
            return FALSE;
        }
    }
    else
    {
        if (!m_socket.Create())
        {
            return FALSE;
        }
        if (!m_socket.Bind(m_port, settings->getLoopbackOnly()))
        {
            return FALSE;
        }
        if (!m_socket.Listen())
        {
            return FALSE;
        }
    }
    m_thread = new vncHTTPConnectThread;
    if (m_thread == nullptr)
    {
        return FALSE;
    }

    vncHTTPConnectThread* t = reinterpret_cast<vncHTTPConnectThread*>(m_thread);
    t->m_tlsEnabled = true;
    if (certThumbprint && certThumbprint[0U])
    {
        ::strncpy_s(t->m_tlsCertThumbprint.data(), t->m_tlsCertThumbprint.size(), certThumbprint, 40);
    }
    else
    {
        std::memset(t->m_tlsCertThumbprint.data(), 0, sizeof(t->m_tlsCertThumbprint));
    }

    return t->Init(&m_socket, server);
}
