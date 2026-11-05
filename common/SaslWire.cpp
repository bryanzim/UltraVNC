// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//
#include "SaslWire.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SaslWire
{
    bool WriteU8(ISaslTransport* const ch, std::uint8_t v) { return ch->SendAll(&v, 1); }

    bool WriteU32(ISaslTransport* const ch, const std::uint32_t v)
    {
        std::array<std::uint8_t, 4U> b{ static_cast<std::uint8_t>((v >> 24)), static_cast<std::uint8_t>((v >> 16)),
                                        static_cast<std::uint8_t>((v >> 8)), static_cast<std::uint8_t>(v) };
        return ch->SendAll(b.data(), 4);
    }

    bool ReadU8(ISaslTransport* const ch, std::uint8_t& v) { return ch->RecvAll(&v, 1); }

    bool ReadU32(ISaslTransport* const ch, std::uint32_t& v)
    {
        std::array<std::uint8_t, 4U> b;
        if (!ch->RecvAll(b.data(), 4))
        {
            return false;
        }
        v = (static_cast<std::uint32_t>(b[0U]) << 24) | (static_cast<std::uint32_t>(b[1U]) << 16) |
            (static_cast<std::uint32_t>(b[2U]) << 8) | static_cast<std::uint32_t>(b[3U]);
        return true;
    }

    bool WriteString(ISaslTransport* const ch, const char* const s, const std::size_t len)
    { return WriteU32(ch, static_cast<std::uint32_t>(len)) && ((len == 0) || ch->SendAll(s, len)); }

    bool WriteString(ISaslTransport* const ch, const std::string& s) { return WriteString(ch, s.data(), s.size()); }

    bool ReadString(ISaslTransport* const ch, const std::uint32_t maxLen, std::string& out)
    {
        std::uint32_t len;
        if (!ReadU32(ch, len) || (len > maxLen))
        {
            return false;
        }
        out.assign(len, '\0');
        return (len == 0) || ch->RecvAll(&out[0U], len);
    }

    bool WriteData(ISaslTransport* ch, const void* const data, const std::size_t len)
    {
        if (len == 0)
        {
            return WriteU32(ch, 0);
        } // NULL data
        std::uint8_t nul{ 0U };
        return WriteU32(ch, static_cast<std::uint32_t>(len) + 1) && ch->SendAll(data, len) && ch->SendAll(&nul, 1);
    }

    bool WriteData(ISaslTransport* const ch, const std::string& s) { return WriteData(ch, s.data(), s.size()); }

    bool ReadData(ISaslTransport* const ch, std::vector<std::uint8_t>& out, bool& isNull)
    {
        std::uint32_t len;
        if (!ReadU32(ch, len) || (len > kMaxSaslData))
        {
            return false;
        }
        isNull = (len == 0);
        out.clear();
        if (isNull)
        {
            return true;
        }
        out.resize(len);
        if (!ch->RecvAll(out.data(), len))
        {
            out.clear();
            return false;
        }
        if (out.back() != 0)
        {
            out.clear();
            return false;
        } // QEMU requires NUL pad
        out.pop_back();
        return true;
    }

    bool ReadDataString(ISaslTransport* const ch, std::string& out, bool& isNull)
    {
        std::vector<std::uint8_t> buf;
        if (!ReadData(ch, buf, isNull))
        {
            return false;
        }
        out.assign((const char*)buf.data(), buf.size());
        return true;
    }

    bool WriteStep(ISaslTransport* const ch, const void* const data, const std::size_t len, const bool complete)
    { return WriteData(ch, data, len) && WriteU8(ch, complete ? 1 : 0); }

    bool ReadStep(ISaslTransport* const ch, std::vector<std::uint8_t>& out, bool& isNull, bool& complete)
    {
        std::uint8_t c;
        return ReadData(ch, out, isNull) && ReadU8(ch, c) && (complete = (c != 0), true);
    }
}
