#pragma once

#include "Types.h"

#include <algorithm>

namespace eclips
{
    // Reloj puro de presentacion para la intro del motor.
    // El preroll despierta audio/render sin consumir el arco visible.
    class IntroTimeline
    {
    public:
        static constexpr f64 kPreRollDur = 0.20;
        static constexpr f64 kEclipseDur = 5.00;
        static constexpr f64 kHandoffDur = 0.55;
        static constexpr f64 kTotalDur = kEclipseDur + kHandoffDur;

        void reset()
        {
            m_preRollRemaining = kPreRollDur;
            m_presentationTime = 0.0;
        }

        void advance(f64 dt)
        {
            if (dt <= 0.0) return;

            if (m_preRollRemaining > 0.0)
            {
                const f64 consumed = std::min(dt, m_preRollRemaining);
                m_preRollRemaining -= consumed;
                dt -= consumed;
            }

            m_presentationTime += dt;
        }

        void skipTo(f64 presentationTime)
        {
            m_preRollRemaining = 0.0;
            if (m_presentationTime < presentationTime)
                m_presentationTime = presentationTime;
        }

        bool inPreRoll() const { return m_preRollRemaining > 0.0; }
        bool inHandoff() const { return m_presentationTime > kEclipseDur; }
        bool finished() const { return m_presentationTime >= kTotalDur; }

        f64 presentationTime() const { return m_presentationTime; }

        f32 handoffWhiteAlpha() const
        {
            if (!inHandoff()) return 1.0f;
            const f64 p = (m_presentationTime - kEclipseDur) / kHandoffDur;
            return static_cast<f32>(1.0 - std::clamp(p, 0.0, 1.0));
        }

    private:
        f64 m_preRollRemaining = kPreRollDur;
        f64 m_presentationTime = 0.0;
    };
}
