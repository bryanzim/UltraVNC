#include <array>
// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef VNCVIEWER_WHEELINPUT_H
#define VNCVIEWER_WHEELINPUT_H

// Per-connection wheel state. The caller serializes access and supplies a sink
// taking (precise, axis, delta). Legacy deltas are always complete detents.
class WheelInput
{
public:
    explicit WheelInput() { Reset(); }
    void Reset()
    {
        m_precise = false;
        m_remainder[0U] = m_remainder[1U] = 0;
        m_firstAxis = 0;
    }

    void SetPrecise(const bool enabled) { m_precise = enabled; }
    bool HasPending() const { return (m_remainder[0U] != 0) || (m_remainder[1U] != 0); }

    template <class Send> void FlushPending(const Send send)
    {
        if (!m_precise)
        {
            return;
        }
        // Preserve the order in which the axes acquired pending remainders.
        for (unsigned i{ 0U }; i < 2; ++i)
        {
            unsigned axis = (m_firstAxis + i) % 2;
            if (m_remainder[axis])
            {
                send(true, axis, m_remainder[axis]);
                m_remainder[axis] = 0;
            }
        }
    }

    template <class Send> void Process(const unsigned axis, const int delta, const Send send)
    {
        if ((axis > 1) || (delta == 0))
        {
            return;
        }
        if (m_precise)
        {
            FlushPending(send);
            send(true, axis, delta);
            return;
        }
        if (!m_remainder[1 - axis])
        {
            m_firstAxis = axis;
        }
        // Windows deltas are signed 16-bit; adding a remainder cannot overflow int.
        int total = m_remainder[axis] + delta;
        while ((total >= 120) || (total <= -120))
        {
            int step = total > 0 ? 120 : -120;
            send(false, axis, step);
            total -= step;
        }
        m_remainder[axis] = total;
        if (!total)
        {
            m_firstAxis = 1 - axis;
        }
    }

private:
    bool m_precise;
    std::array<int, 2U> m_remainder;
    unsigned m_firstAxis;
};
#endif
