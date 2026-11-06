// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//
// BcryptHelp.h - minimal BCrypt wrappers (SHA-256, HMAC-SHA256, PBKDF2, RNG)
// used by the SASL/SCRAM implementation.
#pragma once
#include <Windows.h>

#include <bcrypt.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace BcryptHelp
{
    bool Sha256(const void* data, std::size_t len, std::uint8_t out[32U]);
    bool Sha256(const std::vector<std::uint8_t>& data, std::uint8_t out[32U]);
    bool HmacSha256(const void* key, std::size_t keyLen, const void* data, std::size_t dataLen, std::uint8_t out[32U]);
    bool Pbkdf2Sha256(const void* password, std::size_t passLen, const void* salt, std::size_t saltLen, std::uint64_t iterations,
                      std::uint8_t* out, std::size_t outLen);
    bool Random(std::uint8_t* buf, std::size_t len);
}
