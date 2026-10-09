// ECLIPS ENGINE - Medida tipografica opt-in.
#pragma once

#include "../core/Types.h"
#include <string>

namespace eclips
{
    enum class FontMeasure : u8
    {
        Height = 0, // legacy: ascent - descent
        Em     = 1  // nominal units-per-em
    };

    inline bool parseFontMeasure(const std::string& token, FontMeasure& out)
    {
        if (token == "em")
        {
            out = FontMeasure::Em;
            return true;
        }
        if (token == "altura" || token == "height")
        {
            out = FontMeasure::Height;
            return true;
        }
        return false;
    }

    inline const char* fontMeasureName(FontMeasure measure)
    {
        return measure == FontMeasure::Em ? "em" : "altura";
    }
}
