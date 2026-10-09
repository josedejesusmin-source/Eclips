// ============================================================================
// Gameplay2DHud.h - resolucion pura de presentacion declarativa del HUD 2D
// ============================================================================
//
// G2D-HUD1 solo resuelve el texto de `contador`:
//   - exige exactamente un placeholder literal `{}`;
//   - sustituye el valor crudo de la variable;
//   - una variable ausente o vacia se presenta como `0`.
//
// No conoce SDL, render, MapData, Lua, save ni la posicion de la linea.
// ============================================================================

#pragma once

#include <optional>
#include <string>

namespace eclips
{

inline std::optional<std::string> gameplay2DFormatHudCounter(
    const std::string& format,
    const std::string& value)
{
    constexpr const char* placeholder = "{}";

    const std::size_t first = format.find(placeholder);
    if (first == std::string::npos)
        return std::nullopt;

    if (format.find(placeholder, first + 2) != std::string::npos)
        return std::nullopt;

    std::string text = format;
    text.replace(first, 2, value.empty() ? "0" : value);
    return text;
}

} // namespace eclips
