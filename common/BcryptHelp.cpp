// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
//
#include "BcryptHelp.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace BcryptHelp
{
    static BCRYPT_ALG_HANDLE GetAlg(const LPCWSTR id, const ULONG flags)
    {
        BCRYPT_ALG_HANDLE h = nullptr;
        if (::BCryptOpenAlgorithmProvider(&h, id, nullptr, flags) != 0)
        {
            return nullptr;
        }
        return h;
    }

    bool Sha256(const void* const data, const std::size_t len, std::uint8_t out[32U])
    {
        BCRYPT_ALG_HANDLE hAlg = GetAlg(BCRYPT_SHA256_ALGORITHM, 0);
        if (!hAlg)
        {
            return false;
        }
        BCRYPT_HASH_HANDLE hHash = nullptr;
        NTSTATUS st = ::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
        if (st == 0)
        {
            st = ::BCryptHashData(hHash, (PUCHAR)data, static_cast<ULONG>(len), 0);
            if (st == 0)
            {
                st = ::BCryptFinishHash(hHash, out, 32, 0);
            }
        }
        if (hHash)
        {
            ::BCryptDestroyHash(hHash);
        }
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return st == 0;
    }

    bool Sha256(const std::vector<std::uint8_t>& data, std::uint8_t out[32U]) { return Sha256(data.data(), data.size(), out); }

    bool HmacSha256(const void* const key, const std::size_t keyLen, const void* const data, const std::size_t dataLen,
                    std::uint8_t out[32U])
    {
        BCRYPT_ALG_HANDLE hAlg = GetAlg(BCRYPT_SHA256_ALGORITHM, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (!hAlg)
        {
            return false;
        }
        BCRYPT_HASH_HANDLE hHash = nullptr;
        NTSTATUS st = ::BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key, static_cast<ULONG>(keyLen), 0);
        if (st == 0)
        {
            st = ::BCryptHashData(hHash, (PUCHAR)data, static_cast<ULONG>(dataLen), 0);
            if (st == 0)
            {
                st = ::BCryptFinishHash(hHash, out, 32, 0);
            }
        }
        if (hHash)
        {
            ::BCryptDestroyHash(hHash);
        }
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return st == 0;
    }

    bool Pbkdf2Sha256(const void* const password, const std::size_t passLen, const void* const salt, const std::size_t saltLen,
                      const std::uint64_t iterations, std::uint8_t* const out, const std::size_t outLen)
    {
        // PBKDF2-HMAC-SHA256, RFC 2898 section 5.2, implemented with our
        // HMAC helper (BCryptDeriveKeyPBKDF2 is unreliable across SKUs).
        // Only single-block output (outLen<=32) is needed for SCRAM keys.
        if (outLen > 32)
        {
            return false;
        }
        std::vector<std::uint8_t> u(saltLen + 4);
        std::memcpy(u.data(), salt, saltLen);
        u[saltLen + 0] = 0;
        u[saltLen + 1] = 0;
        u[saltLen + 2] = 0;
        u[saltLen + 3] = 1; // block index 1, big-endian

        std::array<std::uint8_t, 32U> t;
        std::array<std::uint8_t, 32U> prev;
        if (!HmacSha256(password, passLen, u.data(), u.size(), prev.data()))
        {
            return false;
        }
        std::memcpy(t.data(), prev.data(), 32);
        for (std::uint64_t i{ 1U }; i < iterations; ++i)
        {
            if (!HmacSha256(password, passLen, prev.data(), 32, prev.data()))
            {
                return false;
            }
            for (int j{ 0 }; j < 32; ++j)
            {
                t[j] ^= prev[j];
            }
        }
        std::memcpy(out, t.data(), outLen);
        return true;
    }

    bool Random(std::uint8_t* const buf, const std::size_t len)
    { return ::BCryptGenRandom(nullptr, buf, static_cast<ULONG>(len), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0; }
}
