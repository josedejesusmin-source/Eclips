// ============================================================================
// Gameplay2DFrames.h - resolucion pura de frames direccionales del jugador
// ============================================================================
//
// G2D-FR1 descubre ciclos opcionales <estado>_1.png .. <estado>_N.png:
//   - el PNG base <estado>.png sigue siendo obligatorio y queda fuera del ciclo;
//   - cero numerados conserva presentacion estatica;
//   - uno, un hueco o una grafia no canonica invalida solo esa direccion;
//   - dos o mas numerados contiguos forman el ciclo en orden numerico.
//
// Esta casa no conoce SDL, OpenGL, SpriteSheet, MapData, reloj ni actores.
// ============================================================================

#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace eclips
{

enum class Gameplay2DFrameState
{
    Idle,
    Up,
    Down,
    Left,
    Right
};

enum class Gameplay2DFrameSequenceKind
{
    Static,
    Animated,
    Invalid
};

struct Gameplay2DFrameSequence
{
    Gameplay2DFrameSequenceKind kind =
        Gameplay2DFrameSequenceKind::Invalid;
    std::string fallbackFrame;
    std::vector<std::string> frames;
};

inline bool gameplay2DFrameCanvasMatches(int baseWidth,
                                         int baseHeight,
                                         int frameWidth,
                                         int frameHeight)
{
    return baseWidth > 0 && baseHeight > 0 &&
           frameWidth == baseWidth && frameHeight == baseHeight;
}

inline const char* gameplay2DFrameStateStem(Gameplay2DFrameState state)
{
    switch (state)
    {
        case Gameplay2DFrameState::Idle:  return "idle";
        case Gameplay2DFrameState::Up:    return "up";
        case Gameplay2DFrameState::Down:  return "down";
        case Gameplay2DFrameState::Left:  return "left";
        case Gameplay2DFrameState::Right: return "right";
    }
    return "idle";
}

inline Gameplay2DFrameSequence gameplay2DResolveFrameSequence(
    const std::string& basePath,
    Gameplay2DFrameState state)
{
    Gameplay2DFrameSequence result;

    const std::filesystem::path directory(basePath);
    const std::string stem = gameplay2DFrameStateStem(state);
    const std::filesystem::path fallback = directory / (stem + ".png");
    result.fallbackFrame = fallback.generic_string();

    std::error_code error;
    if (!std::filesystem::is_regular_file(fallback, error) || error)
        return result;

    std::vector<int> indices;
    bool malformedNumberedName = false;
    const std::string prefix = stem + "_";
    const std::string extension = ".png";

    std::filesystem::directory_iterator it(directory, error);
    const std::filesystem::directory_iterator end;
    while (!error && it != end)
    {
        const std::filesystem::directory_entry& entry = *it;
        const std::string filename = entry.path().filename().string();

        if (filename.size() >= prefix.size() + extension.size() &&
            filename.compare(0, prefix.size(), prefix) == 0 &&
            filename.compare(filename.size() - extension.size(),
                             extension.size(), extension) == 0)
        {
            const std::string numberText = filename.substr(
                prefix.size(),
                filename.size() - prefix.size() - extension.size());

            int number = 0;
            bool canonical = !numberText.empty() &&
                             !(numberText.size() > 1 &&
                               numberText.front() == '0');
            for (char c : numberText)
            {
                if (c < '0' || c > '9')
                {
                    canonical = false;
                    break;
                }

                const int digit = c - '0';
                if (number >
                    (std::numeric_limits<int>::max() - digit) / 10)
                {
                    canonical = false;
                    break;
                }
                number = number * 10 + digit;
            }

            if (!canonical || number <= 0)
                malformedNumberedName = true;
            else
                indices.push_back(number);
        }

        it.increment(error);
    }

    if (error || malformedNumberedName)
        return result;

    if (indices.empty())
    {
        result.kind = Gameplay2DFrameSequenceKind::Static;
        return result;
    }

    std::sort(indices.begin(), indices.end());
    if (indices.size() < 2 || indices.front() != 1)
        return result;

    for (std::size_t i = 0; i < indices.size(); ++i)
    {
        const int expected = static_cast<int>(i) + 1;
        if (indices[i] != expected)
            return result;
    }

    result.kind = Gameplay2DFrameSequenceKind::Animated;
    result.frames.reserve(indices.size());
    for (int index : indices)
    {
        result.frames.push_back(
            (directory / (stem + "_" + std::to_string(index) + ".png"))
                .generic_string());
    }
    return result;
}

inline Gameplay2DFrameState gameplay2DResolvePlayerFrameState(
    bool sideScroll,
    bool onGround,
    float verticalVelocity,
    float inputX,
    float inputY)
{
    if (sideScroll)
    {
        if (!onGround)
        {
            return verticalVelocity < 0.0f
                ? Gameplay2DFrameState::Up
                : Gameplay2DFrameState::Down;
        }

        if (inputX < 0.0f) return Gameplay2DFrameState::Left;
        if (inputX > 0.0f) return Gameplay2DFrameState::Right;
        return Gameplay2DFrameState::Idle;
    }

    if (inputX == 0.0f && inputY == 0.0f)
        return Gameplay2DFrameState::Idle;

    if (std::abs(inputX) > std::abs(inputY))
    {
        return inputX > 0.0f
            ? Gameplay2DFrameState::Right
            : Gameplay2DFrameState::Left;
    }

    return inputY > 0.0f
        ? Gameplay2DFrameState::Down
        : Gameplay2DFrameState::Up;
}

} // namespace eclips
