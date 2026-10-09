// ============================================================================
// TextStroke.h — dato puro de una pasada de outline autorable.
// ============================================================================
#pragma once

#include "Types.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string>
#include <sstream>

namespace eclips
{
    struct TextStrokeSpec
    {
        std::string colorHex;
        f32 grosor = 0.0f;
        f32 dx = 0.0f;
        f32 dy = 0.0f;
    };

    struct ResolvedTextStroke
    {
        f32 r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
        f32 grosor = 0.0f;
        f32 dx = 0.0f, dy = 0.0f;
    };

    inline bool isTextStrokeColor(const std::string& raw)
    {
        const size_t begin = (!raw.empty() && raw.front() == '#') ? 1u : 0u;
        const size_t digits = raw.size() - begin;
        if (digits != 6u && digits != 8u) return false;
        for (size_t i = begin; i < raw.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(raw[i]);
            if (!std::isxdigit(c)) return false;
        }
        return true;
    }

    inline bool parseTextStrokeNumber(const std::string& token, f32& out)
    {
        if (token.empty()) return false;
        errno = 0;
        char* end = nullptr;
        const float value = std::strtof(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0' || errno == ERANGE ||
            !std::isfinite(value))
            return false;
        out = value;
        return true;
    }

    // Parseo atómico de `grosor color [dx dy]`.
    inline bool parseTextStrokeSpec(const std::string& payload,
                                    TextStrokeSpec& out,
                                    std::string& error)
    {
        std::istringstream in(payload);
        std::string thicknessToken, colorToken, dxToken, dyToken, extra;
        if (!(in >> thicknessToken >> colorToken))
        {
            error = "espera grosor color [dx dy]";
            return false;
        }

        f32 thickness = 0.0f;
        if (!parseTextStrokeNumber(thicknessToken, thickness) || thickness < 0.0f)
        {
            error = "el grosor debe ser un numero finito >= 0";
            return false;
        }
        if (!isTextStrokeColor(colorToken))
        {
            error = "el color debe ser #RRGGBB o #RRGGBBAA";
            return false;
        }

        f32 dx = 0.0f, dy = 0.0f;
        const bool hasDx = static_cast<bool>(in >> dxToken);
        const bool hasDy = static_cast<bool>(in >> dyToken);
        if (hasDx != hasDy || (hasDx &&
            (!parseTextStrokeNumber(dxToken, dx) ||
             !parseTextStrokeNumber(dyToken, dy))))
        {
            error = "los offsets requieren dos numeros finitos dx dy";
            return false;
        }
        if (in >> extra)
        {
            error = "sobran valores; la forma es grosor color [dx dy]";
            return false;
        }

        TextStrokeSpec parsed;
        parsed.colorHex = colorToken;
        parsed.grosor = thickness;
        parsed.dx = dx;
        parsed.dy = dy;
        out = parsed;
        return true;
    }
}
