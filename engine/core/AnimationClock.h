// ============================================================================
// AnimationClock.h — contrato puro de dominios y duraciones de animación
// ============================================================================
//
// Núcleo del reloj por avance de diálogo. CP0 fijó el tipo/parser y CP1 conectó
// el dominio al track directo de escala; CP2 reutilizará el mismo contrato para
// la envolvente de intensidad. El objetivo principal es que un token como
// `8avances_dialogo` jamás pueda degradar a ocho segundos por el prefijo que
// std::stof alcanzó a consumir.
//
// La marca de disco es deliberadamente distinta de las palabras públicas. Un
// renombre futuro de `avances_dialogo` no cambia el contrato de Save v4.
// ============================================================================

#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace eclips
{
    enum class ClockDomain : std::uint8_t
    {
        Seconds = 0,
        DialogueAdvance = 1
    };

    struct DurationSpec
    {
        float       amount = 0.0f;
        ClockDomain clock = ClockDomain::Seconds;
    };

    enum class DurationParseError : std::uint8_t
    {
        None = 0,
        Empty,
        MissingUnit,
        UnknownUnit,
        InvalidNumber,
        NonFinite,
        NonPositive,
        FractionalDialogueAdvance,
        DialogueAdvanceTooLarge,
        UnsupportedClockDomain
    };

    // `DurationSpec` usa float para convivir con AnimTrack. Hasta 2^24-1 cada
    // entero y cada incremento de +1 siguen siendo exactos; 2^24 ya no puede
    // avanzar al entero siguiente. El parser y Save v4 comparten este techo.
    inline constexpr float kMaxExactDialogueAdvance = 16777215.0f;

    // Cada caller decide su compatibilidad. La sintaxis nueva usa el default
    // estricto; una superficie legacy puede permitir temporalmente segundos
    // desnudos y emitir su propia advertencia con usedBareSeconds.
    struct DurationParseOptions
    {
        bool allowBareSeconds = false;
        bool allowSeconds = true;
        bool allowDialogueAdvance = true;
    };

    struct DurationParseResult
    {
        DurationSpec      value;
        DurationParseError error = DurationParseError::Empty;
        bool              usedBareSeconds = false;

        explicit operator bool() const
        {
            return error == DurationParseError::None;
        }
    };

    inline bool durationTokenEndsWith(const std::string& value,
                                      const std::string& suffix)
    {
        return value.size() >= suffix.size() &&
            value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    inline std::string trimDurationToken(const std::string& value)
    {
        const std::size_t begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return {};
        const std::size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    // Números internos de codec: sin unidad, consumo total y siempre finitos.
    // No usar std::stof directo en formatos persistentes: `8basura` no es 8.
    inline bool parseFiniteFloatFull(const std::string& token, float& out)
    {
        if (token.empty()) return false;
        std::size_t consumed = 0;
        try
        {
            out = std::stof(token, &consumed);
        }
        catch (...)
        {
            return false;
        }
        return consumed == token.size() && std::isfinite(out);
    }

    inline DurationParseResult parseDurationSpec(
        const std::string& raw,
        const DurationParseOptions& options = DurationParseOptions{})
    {
        DurationParseResult result;
        const std::string token = trimDurationToken(raw);
        if (token.empty())
        {
            result.error = DurationParseError::Empty;
            return result;
        }

        static const std::string kDialogueEs = "avances_dialogo";
        static const std::string kDialogueEn = "dialogue_advances";

        std::string numberPart;
        if (durationTokenEndsWith(token, kDialogueEs))
        {
            result.value.clock = ClockDomain::DialogueAdvance;
            numberPart = token.substr(0, token.size() - kDialogueEs.size());
        }
        else if (durationTokenEndsWith(token, kDialogueEn))
        {
            result.value.clock = ClockDomain::DialogueAdvance;
            numberPart = token.substr(0, token.size() - kDialogueEn.size());
        }
        else if (durationTokenEndsWith(token, "s") ||
                 durationTokenEndsWith(token, "S"))
        {
            result.value.clock = ClockDomain::Seconds;
            numberPart = token.substr(0, token.size() - 1);
        }
        else
        {
            // Distingue un número desnudo de una unidad desconocida sin aceptar
            // el prefijo numérico como éxito.
            std::size_t consumed = 0;
            try
            {
                (void)std::stof(token, &consumed);
            }
            catch (...)
            {
                result.error = DurationParseError::InvalidNumber;
                return result;
            }

            if (consumed != token.size())
            {
                result.error = DurationParseError::UnknownUnit;
                return result;
            }
            if (!options.allowBareSeconds)
            {
                result.error = DurationParseError::MissingUnit;
                return result;
            }

            result.value.clock = ClockDomain::Seconds;
            result.usedBareSeconds = true;
            numberPart = token;
        }

        if (numberPart.empty())
        {
            result.error = DurationParseError::InvalidNumber;
            return result;
        }

        std::size_t consumed = 0;
        try
        {
            result.value.amount = std::stof(numberPart, &consumed);
        }
        catch (...)
        {
            result.error = DurationParseError::InvalidNumber;
            return result;
        }

        if (consumed != numberPart.size())
        {
            result.error = DurationParseError::InvalidNumber;
            return result;
        }
        if (!std::isfinite(result.value.amount))
        {
            result.error = DurationParseError::NonFinite;
            return result;
        }
        if (result.value.amount <= 0.0f)
        {
            result.error = DurationParseError::NonPositive;
            return result;
        }
        if (result.value.clock == ClockDomain::DialogueAdvance &&
            std::floor(result.value.amount) != result.value.amount)
        {
            result.error = DurationParseError::FractionalDialogueAdvance;
            return result;
        }
        if (result.value.clock == ClockDomain::DialogueAdvance &&
            result.value.amount > kMaxExactDialogueAdvance)
        {
            result.error = DurationParseError::DialogueAdvanceTooLarge;
            return result;
        }
        if ((result.value.clock == ClockDomain::Seconds && !options.allowSeconds) ||
            (result.value.clock == ClockDomain::DialogueAdvance &&
                !options.allowDialogueAdvance))
        {
            result.error = DurationParseError::UnsupportedClockDomain;
            return result;
        }

        result.error = DurationParseError::None;
        return result;
    }

    inline const char* durationParseErrorText(DurationParseError error)
    {
        switch (error)
        {
        case DurationParseError::None: return "sin error";
        case DurationParseError::Empty: return "falta la duracion";
        case DurationParseError::MissingUnit: return "falta la unidad (s)";
        case DurationParseError::UnknownUnit: return "unidad desconocida";
        case DurationParseError::InvalidNumber: return "cantidad invalida";
        case DurationParseError::NonFinite: return "la cantidad debe ser finita";
        case DurationParseError::NonPositive: return "la cantidad debe ser mayor que cero";
        case DurationParseError::FractionalDialogueAdvance:
            return "los avances de dialogo deben ser enteros";
        case DurationParseError::DialogueAdvanceTooLarge:
            return "la cantidad de avances excede el rango entero exacto";
        case DurationParseError::UnsupportedClockDomain:
            return "ese comando no admite este dominio de reloj";
        }
        return "error de duracion desconocido";
    }

    // Tags internos de Save v4. No son aliases públicos de EclipScript.
    inline const char* clockDomainDiskTag(ClockDomain clock)
    {
        switch (clock)
        {
        case ClockDomain::Seconds:         return "seconds";
        case ClockDomain::DialogueAdvance: return "dialogue_advance";
        }
        return "";
    }

    inline bool parseClockDomainDiskTag(const std::string& tag, ClockDomain& out)
    {
        if (tag == "seconds")
        {
            out = ClockDomain::Seconds;
            return true;
        }
        if (tag == "dialogue_advance")
        {
            out = ClockDomain::DialogueAdvance;
            return true;
        }
        return false;
    }
}
