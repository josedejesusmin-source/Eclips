// ============================================================================
// Gameplay2DSpatial.h — consultas espaciales puras de Gameplay 2D
// ============================================================================
//
// No conoce actores, Tower Defense, SDL, Lua ni MapData. Opera sobre cualquier
// rectángulo con campos x/y/w/h y devuelve el tiempo normalizado del primer
// contacto del AABB móvil contra un AABB estático.
//
// La política de borde es explícita:
//   - Open: tocar sin entrar no cuenta (marcadores M1).
//   - Closed: el contacto cuenta, salvo que el móvil parta tocando y se aleje
//     inmediatamente (obstrucción O3/R3).
// ============================================================================

#pragma once

#include "Gameplay2DAbility.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace eclips
{

enum class Gameplay2DSweepBoundary
{
    Open,
    Closed
};

struct Gameplay2DSweepResult
{
    bool hit = false;
    float time = 1.0f;
};

struct Gameplay2DAddressedSweepHit
{
    Gameplay2DActorAddress address;
    float time = 1.0f;
};

inline bool gameplay2DAddressedSweepHitPrecedes(
    const Gameplay2DAddressedSweepHit& a,
    const Gameplay2DAddressedSweepHit& b)
{
    if (a.time != b.time)
        return a.time < b.time;
    return a.address < b.address;
}

template <typename Rect>
Gameplay2DSweepResult sweepGameplay2DAABB(
    const Rect& from,
    const Rect& to,
    const Rect& obstacle,
    Gameplay2DSweepBoundary boundary)
{
    constexpr float epsilon = 0.000001f;

    if (!std::isfinite(from.x) || !std::isfinite(from.y) ||
        !std::isfinite(from.w) || !std::isfinite(from.h) ||
        !std::isfinite(to.x) || !std::isfinite(to.y) ||
        !std::isfinite(obstacle.x) || !std::isfinite(obstacle.y) ||
        !std::isfinite(obstacle.w) || !std::isfinite(obstacle.h) ||
        from.w <= 0.0f || from.h <= 0.0f ||
        obstacle.w <= 0.0f || obstacle.h <= 0.0f)
    {
        return {};
    }

    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    if (std::abs(dx) < epsilon && std::abs(dy) < epsilon)
        return {};

    // Minkowski: expandir el obstáculo por el tamaño del móvil y barrer la
    // esquina superior izquierda del actor como un punto.
    const float minX = obstacle.x - from.w;
    const float maxX = obstacle.x + obstacle.w;
    const float minY = obstacle.y - from.h;
    const float maxY = obstacle.y + obstacle.h;

    float tEnter = 0.0f;
    float tExit = 1.0f;

    auto clipAxis = [&](float start, float delta,
                        float minValue, float maxValue) -> bool {
        if (std::abs(delta) < epsilon)
        {
            if (boundary == Gameplay2DSweepBoundary::Open)
                return start > minValue && start < maxValue;
            return start >= minValue && start <= maxValue;
        }

        float t1 = (minValue - start) / delta;
        float t2 = (maxValue - start) / delta;
        if (t1 > t2) std::swap(t1, t2);

        tEnter = std::max(tEnter, t1);
        tExit = std::min(tExit, t2);
        if (boundary == Gameplay2DSweepBoundary::Open)
            return tEnter < tExit;
        return tEnter <= tExit;
    };

    if (!clipAxis(from.x, dx, minX, maxX) ||
        !clipAxis(from.y, dy, minY, maxY))
    {
        return {};
    }

    if (boundary == Gameplay2DSweepBoundary::Open)
    {
        if (!(tEnter < tExit))
            return {};
    }
    else
    {
        // Un contacto que sólo existe en t=0 y se separa no retiene R3.
        if (!(tEnter <= tExit) || tExit <= epsilon)
            return {};
    }

    if (tEnter < 0.0f || tEnter > 1.0f)
        return {};

    return { true, std::max(0.0f, std::min(1.0f, tEnter)) };
}

} // namespace eclips
