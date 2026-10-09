#pragma once

namespace eclips::display_change
{
    // Estado logico de display. windowW/H describen la preferencia windowed
    // incluso mientras fullscreen esta activo; windowSizeUserSet conserva si
    // ese tamano era una eleccion del jugador o el fallback de arranque.
    struct State
    {
        bool fullscreen = false;
        int windowW = 0;
        int windowH = 0;
        bool windowSizeUserSet = false;

        friend bool operator==(const State& a, const State& b)
        {
            return a.fullscreen == b.fullscreen
                && a.windowW == b.windowW
                && a.windowH == b.windowH
                && a.windowSizeUserSet == b.windowSizeUserSet;
        }
    };

    class Transaction
    {
    public:
        static constexpr double kTimeoutSeconds = 15.0;

        bool begin(const State& before, const State& candidate)
        {
            if (m_active) return false;
            m_before = before;
            m_candidate = candidate;
            m_remaining = kTimeoutSeconds;
            m_timedOut = false;
            m_active = true;
            return true;
        }

        bool active() const { return m_active; }
        double remainingSeconds() const { return m_remaining; }
        const State& before() const { return m_before; }
        const State& candidate() const { return m_candidate; }

        // Devuelve true desde el frame que agota el reloj hasta que el caller
        // consume la salida con revert(). dt<=0 y NaN no adelantan el tiempo.
        bool tick(double dtSeconds)
        {
            if (!m_active || !(dtSeconds > 0.0)) return false;
            if (m_timedOut) return true;
            if (dtSeconds >= m_remaining)
            {
                m_remaining = 0.0;
                m_timedOut = true;
                return true;
            }
            m_remaining -= dtSeconds;
            return false;
        }

        bool confirm(State& outCandidate)
        {
            if (!m_active || m_timedOut) return false;
            outCandidate = m_candidate;
            close();
            return true;
        }

        bool revert(State& outBefore)
        {
            if (!m_active) return false;
            outBefore = m_before;
            close();
            return true;
        }

    private:
        void close()
        {
            m_active = false;
            m_remaining = kTimeoutSeconds;
            m_timedOut = false;
        }

        State m_before{};
        State m_candidate{};
        double m_remaining = kTimeoutSeconds;
        bool m_active = false;
        bool m_timedOut = false;
    };
}
