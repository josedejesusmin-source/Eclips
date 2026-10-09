// ============================================================================
// OUTLINE-CIRCULO - pure geometry shared by every outlined text variant.
//
// The helper owns only sample count and offsets. It does not emit the fill,
// blend alpha, allocate renderer state, or know about a caller.
// ============================================================================
#pragma once

#include "../core/Types.h"

#include <cmath>
#include <vector>

namespace eclips
{
    struct OutlineCircleOffset
    {
        f32 dx = 0.0f;
        f32 dy = 0.0f;
    };

    inline std::vector<OutlineCircleOffset> outlineCircleOffsets(f32 thickness)
    {
        if (!std::isfinite(thickness) || thickness <= 0.0f)
            return {};

        constexpr f32 kTwoPi = 6.28318530717958647692f;
        constexpr f32 kSampleSpacing = 2.0f;

        int count = static_cast<int>(
            std::ceil((kTwoPi * thickness) / kSampleSpacing));
        if (count < 8) count = 8;
        if (count > 48) count = 48;

        std::vector<OutlineCircleOffset> offsets;
        offsets.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const f32 angle = kTwoPi * static_cast<f32>(i) /
                              static_cast<f32>(count);
            offsets.push_back({
                std::cos(angle) * thickness,
                std::sin(angle) * thickness
            });
        }
        return offsets;
    }
}
