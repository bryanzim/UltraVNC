// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//
// Base64.h - small base64 helpers used by the SASL/SCRAM implementation.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Base64
{
    std::string Encode(const void* data, std::size_t len);
    std::string Encode(const std::vector<std::uint8_t>& data);
    bool Decode(const char* s, std::size_t len, std::vector<std::uint8_t>& out);
    bool Decode(const std::string& s, std::vector<std::uint8_t>& out);
}
