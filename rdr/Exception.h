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


#ifndef RDR_EXCEPTION_H
#define RDR_EXCEPTION_H

#include <array>
#include <cstring>
#include <stdio.h>
#include <string.h>

namespace rdr
{

    struct Exception
    {
        enum
        {
            len = 256
        };
        char str_[len];
        explicit Exception(const char* const s = nullptr, const char* const e = "rdr::Exception")
        {
            str_[0U] = 0;
            ::strncat_s(str_, e, len - 1);
            if (s)
            {
                ::strncat_s(str_, ": ", len - 1 - std::strlen(str_));
                ::strncat_s(str_, s, len - 1 - std::strlen(str_));
            }
        }
        virtual const char* str() const { return str_; }
    };

    struct SystemException : public Exception
    {
        int err;
        SystemException(const char* s, int err_) : err(err_)
        {
            str_[0U] = 0;
            ::strncat_s(str_, "rdr::SystemException: ", len - 1);
            ::strncat_s(str_, s, len - 1 - std::strlen(str_));
            ::strncat_s(str_, ": ", len - 1 - std::strlen(str_));
            std::array<char, 1024U> errorbuffer;
            ::strerror_s(errorbuffer.data(), errorbuffer.size(), err);
            ::strncat_s(str_, errorbuffer.data(), len - 1 - std::strlen(str_));
            ::strncat_s(str_, " (", len - 1 - std::strlen(str_));
            std::array<char, 20U> buf;
            ::sprintf_s(buf.data(), buf.size(), "%d", err);
            ::strncat_s(str_, buf.data(), len - 1 - std::strlen(str_));
            ::strncat_s(str_, ")", len - 1 - std::strlen(str_));
        }
    };

    struct TimedOut : public Exception
    {
        explicit TimedOut(const char* s = nullptr) : Exception(s, "rdr::TimedOut") {}
    };

    struct EndOfStream : public Exception
    {
        explicit EndOfStream(const char* s = nullptr) : Exception(s, "rdr::EndOfStream") {}
    };

    struct FrameException : public Exception
    {
        explicit FrameException(const char* s = nullptr) : Exception(s, "rdr::FrameException") {}
    };

}

#endif
