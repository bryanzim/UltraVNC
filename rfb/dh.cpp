// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// CRYPTO LIBRARY FOR EXCHANGING KEYS
// USING THE DIFFIE-HELLMAN KEY EXCHANGE PROTOCOL

// The diffie-hellman can be used to securely exchange keys
// between parties, where a third party eavesdropper given
// the values being transmitted cannot determine the key.

// Implemented by Lee Griffiths, Jan 2004.
// This software is freeware, you may use it to your discretion,
// however by doing so you take full responsibility for any damage
// it may cause.

// Hope you find it useful, even if you just use some of the functions
// out of it like the prime number generator and the XtoYmodN function.

// It would be great if you could send me emails to: lee.griffiths@first4internet.co.uk
// with any suggestions, comments, or questions!

// Enjoy.

// Adopted to MS-Logon for UltraVNC by marscha, 2006.

#include "dh.h"

#include <tchar.h>

#include <cstdlib>
#include <cstring>
#include <ctime>

class Except
{
private:
    char* info;

public:
    explicit Except(const char* info_ = nullptr);
    virtual ~Except()
    {
        if (info)
        {
            delete[] info;
        }
    }
};

Except::Except(const char* const info_)
{
    if (info_ != nullptr)
    {
        info = new char[std::strlen(info_) + 1];
        ::strcpy_s(info, std::strlen(info_) + 1, info_);
    }
}

DH::DH() : maxNum((static_cast<const unsigned __int64>(1)) << DH_MAX_BITS) { std::srand(static_cast<unsigned>(std::time(nullptr))); }

DH::DH(unsigned __int64 generator, unsigned __int64 modulus)
    : gen(generator), mod(modulus), maxNum((static_cast<const unsigned __int64>(1)) << DH_MAX_BITS)
{
    if ((gen > maxNum) || (mod > maxNum))
    {
        throw Except("Input exceeds maxNum");
    }
    if (gen > mod)
    {
        throw Except("Generator is larger than modulus");
    }
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

DH::~DH() { cleanMem(); }

unsigned __int64 rng(const unsigned __int64 limit) { return (((static_cast<unsigned __int64>(std::rand())) * static_cast<unsigned long long>(std::rand()) * static_cast<unsigned long long>(std::rand())) % limit); }

//Performs the miller-rabin primality test on a guessed prime n.
//trials is the number of attempts to verify this, because the function
//is not 100% accurate it may be a composite. However setting the trial
//value to around 5 should guarantee success even with very large primes
bool DH::millerRabin(const unsigned __int64 n, const unsigned int trials)
{
    unsigned __int64 a{ 0U };

    for (unsigned int i{ 0U }; i < trials; ++i)
    {
        a = rng(n - 3) + 2; // gets random value in [2..n-1]
        if (XpowYmodN(a, n - 1, n) != 1)
        {
            return false;
        } //n composite, return false
    }
    return true; // n probably prime
}

//Generates a large prime number by
//choosing a randomly large integer, and ensuring the value is odd
//then uses the miller-rabin primality test on it to see if it is prime
//if not the value gets increased until it is prime
unsigned __int64 DH::generatePrime()
{
    unsigned __int64 prime{ 0U };

    do
    {
        unsigned __int64 start = rng(maxNum);
        prime = tryToGeneratePrime(start);
    } while (!prime);
    return prime;
}

unsigned __int64 DH::tryToGeneratePrime(unsigned __int64 prime)
{
    //ensure it is an odd number
    if ((prime & 1) == 0)
    {
        prime += 1;
    }

    unsigned __int64 cnt{ 0U };
    while (!millerRabin(prime, 25) && (cnt++ < DH_RANGE) && (prime < maxNum))
    {
        prime += 2;
        if ((prime % 3) == 0)
        {
            prime += 2;
        }
    }
    return ((cnt >= DH_RANGE) || (prime >= maxNum)) ? 0 : prime;
}

//Raises X to the power Y in modulus N
//the values of X, Y, and N can be massive, and this can be
//achieved by first calculating X to the power of 2 then
//using power chaining over modulus N
unsigned __int64 DH::XpowYmodN(const unsigned __int64 x, unsigned __int64 y, const unsigned __int64 N)
{
    unsigned __int64 result{ 1U };
    const unsigned __int64 oneShift63 = (static_cast<unsigned __int64>(1)) << 63;

    for (int i{ 0 }; i < 64; y <<= 1, ++i)
    {
        result = result * result % N;
        if (y & oneShift63)
        {
            result = result * x % N;
        }
    }
    return result;
}

void DH::createKeys()
{
    gen = generatePrime();
    mod = generatePrime();

    if (gen > mod)
    {
        unsigned __int64 swap = gen;
        gen = mod;
        mod = swap;
    }
}

unsigned __int64 DH::createInterKey()
{
    priv = rng(maxNum);
    return pub = XpowYmodN(gen, priv, mod);
}

unsigned __int64 DH::createEncryptionKey(const unsigned __int64 interKey)
{
    if (interKey >= maxNum)
    {
        throw Except("interKey larger than maxNum");
    }
    return key = XpowYmodN(interKey, priv, mod);
}

void DH::cleanMem(const DWORD flags)
{ // marscha (TODO): SecureZeroMemory?
    gen = 0;
    mod = 0;
    priv = 0;
    pub = 0;

    if (flags != DH_CLEAN_ALL_MEMORY_EXCEPT_KEY)
    {
        key = 0;
    }
}

unsigned __int64 DH::getValue(const DWORD flags)
{
    switch (flags)
    {
    case DH_MOD:
        return mod;
    case DH_GEN:
        return gen;
    case DH_PRIV:
        return priv;
    case DH_PUB:
        return pub;
    case DH_KEY:
        return key;
    default:
        return static_cast<unsigned __int64>(0);
    }
}

int bits(__int64 number)
{
    for (unsigned int i{ 0U }; i < 64; ++i)
    {
        number /= 2;
        if (number < 2)
        {
            return static_cast<int>(i);
        }
    }
    return 0;
}

bool int64ToBytes(const unsigned __int64 integer, char* const bytes)
{
    for (int i{ 0 }; i < 8; ++i)
    {
        bytes[i] = static_cast<unsigned char>((integer >> (8 * (7 - i))));
    }
    return true;
}

unsigned __int64 bytesToInt64(const char* const bytes)
{
    unsigned __int64 result{ 0U };
    for (int i{ 0 }; i < 8; ++i)
    {
        result <<= 8;
        result += static_cast<unsigned char>(bytes[i]);
    }
    return result;
}

bool vncWc2Mb(char* const multibyte, WCHAR* const widechar, const int length, const int widechar_count)
{
    multibyte[0U] = '\0';
    /* Security fix: use wcsnlen with max length to prevent OOB read
     * if widechar is not properly null-terminated (FINDING-004) */
    int origlen = static_cast<int>(wcsnlen(widechar, static_cast<size_t>(widechar_count)));
    if (origlen > length)
    {
        return false;
    }
    int newlen = ::WideCharToMultiByte(CP_ACP,    // code page
                                     0,         // performance and mapping flags
                                     widechar,  // address of wide-character string
                                     origlen,   // number of characters in string
                                     multibyte, // address of buffer for new string
                                     length,    // size of buffer
                                     nullptr, nullptr);

    if (newlen >= length)
    {
        return false;
    }
    multibyte[newlen] = '\0';
    return newlen ? true : false;
}
