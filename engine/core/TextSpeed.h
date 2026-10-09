// ============================================================================
// TextSpeed.h - valor honesto, precedencia y catalogo de velocidad de texto
// ============================================================================
// Este header es SDL/GL-free. Solo contiene la ley compartida por Settings,
// manifests/themes, RichText, el dialogo y los dos consumidores de UI.
// ============================================================================
#pragma once

#include "engine/core/EclipsParse.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace eclips
{

    struct TextSpeedInstantaneo {};

    class TextSpeed
    {
    public:
        static std::optional<TextSpeed> tryCps(float value)
        {
            if (!std::isfinite(value) || value <= 0.0f) return std::nullopt;
            return TextSpeed(value);
        }

        static TextSpeed instantaneo()
        {
            return TextSpeed(TextSpeedInstantaneo{});
        }

        bool isCps() const { return std::holds_alternative<float>(m_value); }
        bool isInstantaneo() const
        {
            return std::holds_alternative<TextSpeedInstantaneo>(m_value);
        }

        float cps() const { return std::get<float>(m_value); }

        bool operator==(const TextSpeed& other) const
        {
            if (isInstantaneo() || other.isInstantaneo())
                return isInstantaneo() && other.isInstantaneo();
            return cps() == other.cps();
        }

        bool operator!=(const TextSpeed& other) const { return !(*this == other); }

    private:
        explicit TextSpeed(float value) : m_value(value) {}
        explicit TextSpeed(TextSpeedInstantaneo value) : m_value(value) {}

        std::variant<float, TextSpeedInstantaneo> m_value;
    };

    inline TextSpeed textSpeedDefault()
    {
        return *TextSpeed::tryCps(30.0f);
    }

    inline std::optional<TextSpeed> parseTextSpeedCps(const std::string& token)
    {
        float value = 0.0f;
        if (parseFloatStrict(token, value) != NumParse::Ok)
            return std::nullopt;
        return TextSpeed::tryCps(value);
    }

    inline std::optional<TextSpeed> parseLegacyTextSpeed(const std::string& token)
    {
        float value = 0.0f;
        if (parseFloatStrict(token, value) != NumParse::Ok)
            return std::nullopt;
        // 9999 is interpreted here and only here: the legacy disk codec.
        if (value == 9999.0f) return TextSpeed::instantaneo();
        return TextSpeed::tryCps(value);
    }

    inline bool parsePositiveTextSpeedFactor(const std::string& token, float& out)
    {
        float value = 0.0f;
        if (parseFloatStrict(token, value) != NumParse::Ok || value <= 0.0f)
            return false;
        out = value;
        return true;
    }

    // El theme puede dejar la velocidad en el catalogo heredado o declarar un
    // intervalo continuo. El tipo es neutral: no conoce juegos, UI ni codec.
    enum class TextSpeedDomainKind
    {
        LegacyCatalog,
        Numeric
    };

    struct TextSpeedDomain
    {
        TextSpeedDomainKind kind = TextSpeedDomainKind::LegacyCatalog;
        float min = 0.0f;
        float max = 0.0f;
        float step = 1.0f;
        bool includeInstant = false;

        bool isLegacy() const { return kind == TextSpeedDomainKind::LegacyCatalog; }
        bool isNumeric() const { return kind == TextSpeedDomainKind::Numeric; }

        bool operator==(const TextSpeedDomain& other) const
        {
            return kind == other.kind && min == other.min && max == other.max &&
                step == other.step && includeInstant == other.includeInstant;
        }
        bool operator!=(const TextSpeedDomain& other) const { return !(*this == other); }
    };

    inline TextSpeedDomain textSpeedLegacyDomain()
    {
        return {};
    }

    inline std::optional<TextSpeedDomain> tryNumericTextSpeedDomain(
        float min, float max, float step, bool includeInstant)
    {
        if (!std::isfinite(min) || !std::isfinite(max) || !std::isfinite(step) ||
            min <= 0.0f || max <= min || step <= 0.0f || step > max - min)
            return std::nullopt;
        TextSpeedDomain out;
        out.kind = TextSpeedDomainKind::Numeric;
        out.min = min;
        out.max = max;
        out.step = step;
        out.includeInstant = includeInstant;
        return out;
    }

    enum class TextSpeedProjectionReason
    {
        None,
        BelowMinimum,
        AboveMaximum,
        InstantExcluded
    };

    struct TextSpeedProjection
    {
        TextSpeed stored;
        TextSpeed effective;
        TextSpeedProjectionReason reason = TextSpeedProjectionReason::None;

        bool projected() const { return reason != TextSpeedProjectionReason::None; }
    };

    inline TextSpeedProjection projectTextSpeedToDomain(
        const TextSpeed& stored, const TextSpeedDomain& domain)
    {
        if (domain.isLegacy()) return { stored, stored, TextSpeedProjectionReason::None };
        if (stored.isInstantaneo())
        {
            if (domain.includeInstant)
                return { stored, stored, TextSpeedProjectionReason::None };
            return { stored, *TextSpeed::tryCps(domain.max),
                TextSpeedProjectionReason::InstantExcluded };
        }
        if (stored.cps() < domain.min)
            return { stored, *TextSpeed::tryCps(domain.min),
                TextSpeedProjectionReason::BelowMinimum };
        if (stored.cps() > domain.max)
            return { stored, *TextSpeed::tryCps(domain.max),
                TextSpeedProjectionReason::AboveMaximum };
        return { stored, stored, TextSpeedProjectionReason::None };
    }

    inline float textSpeedDomainEndpoint(const TextSpeedDomain& domain)
    {
        return domain.max + (domain.includeInstant ? domain.step : 0.0f);
    }

    inline float textSpeedDomainCoordinate(const TextSpeedDomain& domain,
                                           const TextSpeed& effective)
    {
        if (domain.isLegacy()) return 0.0f;
        if (effective.isInstantaneo()) return domain.includeInstant
            ? textSpeedDomainEndpoint(domain) : domain.max;
        return std::max(domain.min, std::min(effective.cps(), domain.max));
    }

    inline float snapTextSpeedDomainCoordinate(const TextSpeedDomain& domain,
                                               float coordinate)
    {
        if (domain.isLegacy()) return coordinate;
        const float endpoint = textSpeedDomainEndpoint(domain);
        if (domain.includeInstant && coordinate >= endpoint)
            return endpoint;
        coordinate = std::max(domain.min, std::min(coordinate, domain.max));
        const float steps = std::round((coordinate - domain.min) / domain.step);
        return std::max(domain.min, std::min(domain.max,
            domain.min + steps * domain.step));
    }

    inline TextSpeed textSpeedFromDomainCoordinate(const TextSpeedDomain& domain,
                                                   float coordinate)
    {
        if (!domain.isNumeric()) return textSpeedDefault();
        const float snapped = snapTextSpeedDomainCoordinate(domain, coordinate);
        if (domain.includeInstant && snapped >= textSpeedDomainEndpoint(domain) - 0.0001f)
            return TextSpeed::instantaneo();
        return *TextSpeed::tryCps(snapped);
    }

    inline std::string textSpeedDescribe(const TextSpeed& value);

    struct TextSpeedDeclaration
    {
        TextSpeed value = textSpeedDefault();
        TextSpeedDomain domain = textSpeedLegacyDomain();
        bool english = false;
        std::string error;
        std::string repair = "30";
    };

    inline bool isTextSpeedInstantToken(const std::string& token)
    {
        return token == "instantaneo" || token == "instantáneo" || token == "instant";
    }

    inline bool isTextSpeedEnglishToken(const std::string& token)
    {
        return token == "between" || token == "and" || token == "in" ||
            token == "steps" || token == "of" || token == "with" || token == "instant";
    }

    inline bool isTextSpeedSpanishToken(const std::string& token)
    {
        return token == "entre" || token == "y" || token == "en" ||
            token == "pasos" || token == "de" || token == "con" ||
            token == "instantaneo" || token == "instantáneo";
    }

    // Parser de una declaracion completa. No modifica el snapshot del caller:
    // una linea solo se puede comprometer despues de que esta funcion termina.
    inline TextSpeedDeclaration parseTextSpeedDeclaration(const std::string& raw)
    {
        TextSpeedDeclaration out;
        std::istringstream input(raw);
        std::vector<std::string> tokens;
        std::string token;
        while (input >> token) tokens.push_back(token);
        if (tokens.empty())
        {
            out.error = "falta el valor";
            return out;
        }

        bool hasEnglish = false, hasSpanish = false;
        for (const std::string& t : tokens)
        {
            hasEnglish = hasEnglish || isTextSpeedEnglishToken(t);
            hasSpanish = hasSpanish || isTextSpeedSpanishToken(t);
        }
        out.english = hasEnglish && !hasSpanish;
        if (hasEnglish && hasSpanish)
        {
            out.error = "mezcla palabras ES/EN; usa una sola forma";
            out.repair = "50 entre 20 y 200";
            return out;
        }

        if (isTextSpeedInstantToken(tokens[0]))
            out.value = TextSpeed::instantaneo();
        else
        {
            const auto parsed = parseTextSpeedCps(tokens[0]);
            if (!parsed)
            {
                out.error = "el valor debe ser un CPS positivo finito o instantaneo";
                return out;
            }
            out.value = *parsed;
        }

        if (tokens.size() == 1)
        {
            out.domain = textSpeedLegacyDomain();
            out.repair = textSpeedDescribe(out.value);
            if (out.value.isInstantaneo()) out.repair = out.english ? "instant" : "instantaneo";
            return out;
        }

        const bool endpoint = out.english ?
            (tokens.size() == 7 && tokens[5] == "with" && tokens[6] == "instant") ||
            (tokens.size() == 11 && tokens[5] == "in" && tokens[6] == "steps" &&
             tokens[7] == "of" && tokens[9] == "with" && tokens[10] == "instant") :
            (tokens.size() == 7 && tokens[5] == "con" &&
             (tokens[6] == "instantaneo" || tokens[6] == "instantáneo")) ||
            (tokens.size() == 11 && tokens[5] == "en" && tokens[6] == "pasos" &&
             tokens[7] == "de" && tokens[9] == "con" &&
             (tokens[10] == "instantaneo" || tokens[10] == "instantáneo"));
        const bool steps = out.english ?
            (tokens.size() == 9 && tokens[5] == "in" && tokens[6] == "steps" &&
             tokens[7] == "of") ||
            (tokens.size() == 11 && tokens[5] == "in" && tokens[6] == "steps" &&
             tokens[7] == "of" && tokens[9] == "with") :
            (tokens.size() == 9 && tokens[5] == "en" && tokens[6] == "pasos" &&
             tokens[7] == "de") ||
            (tokens.size() == 11 && tokens[5] == "en" && tokens[6] == "pasos" &&
             tokens[7] == "de" && tokens[9] == "con");
        const bool baseRange = out.english ?
            tokens.size() >= 5 && tokens[1] == "between" && tokens[3] == "and" :
            tokens.size() >= 5 && tokens[1] == "entre" && tokens[3] == "y";
        if (!baseRange || (!endpoint && !steps && tokens.size() != 5) ||
            (endpoint && !steps && tokens.size() != 7) ||
            (steps && tokens.size() != 9 && tokens.size() != 11))
        {
            out.error = "la forma no coincide con entre MIN y MAX, pasos o instantaneo";
            out.repair = out.english ? "50 between 20 and 200" : "50 entre 20 y 200";
            return out;
        }

        float min = 0.0f, max = 0.0f, step = 1.0f;
        if (!parsePositiveTextSpeedFactor(tokens[2], min) ||
            !parsePositiveTextSpeedFactor(tokens[4], max))
        {
            out.error = "MIN y MAX deben ser numeros positivos finitos";
            out.repair = out.english ? "50 between 20 and 200" : "50 entre 20 y 200";
            return out;
        }
        if (max <= min)
        {
            out.error = "MAX debe ser mayor que MIN";
            out.repair = out.english ? "50 between 20 and 200" : "50 entre 20 y 200";
            return out;
        }
        if (steps)
        {
            const size_t stepIndex = 8;
            if (stepIndex >= tokens.size() ||
                !parsePositiveTextSpeedFactor(tokens[stepIndex], step))
            {
                out.error = "PASO debe ser un numero positivo finito";
                out.repair = out.english ? "50 between 20 and 200 in steps of 1" :
                    "50 entre 20 y 200 en pasos de 1";
                return out;
            }
        }
        if (steps && step > max - min)
        {
            out.error = "PASO no puede ser mayor que MAX-MIN";
            out.repair = out.english ? "50 between 20 and 200 in steps of 1" :
                "50 entre 20 y 200 en pasos de 1";
            return out;
        }

        const auto domain = tryNumericTextSpeedDomain(min, max, step, endpoint);
        if (!domain)
        {
            out.error = "el dominio no tiene limites validos";
            out.repair = out.english ? "50 between 20 and 200" : "50 entre 20 y 200";
            return out;
        }
        if (out.value.isInstantaneo() && !endpoint)
        {
            out.error = "el valor instantaneo exige el endpoint con instantaneo";
            out.repair = out.english ? "instant between 1 and 150 with instant" :
                "70 entre 1 y 150 con instantaneo";
            return out;
        }
        if (out.value.isCps() &&
            (out.value.cps() < min || out.value.cps() > max))
        {
            out.error = "el valor por defecto debe estar dentro de MIN..MAX";
            out.repair = out.english ? "50 between 20 and 200" : "50 entre 20 y 200";
            return out;
        }
        out.domain = *domain;
        out.repair = out.english ? "50 between 20 and 200" : "50 entre 20 y 200";
        return out;
    }

    inline std::string textSpeedFormatCps(float value)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<float>::max_digits10)
            << std::defaultfloat << value;
        return out.str();
    }

    inline std::string textSpeedDescribe(const TextSpeed& value)
    {
        return value.isInstantaneo() ? "Instantaneo" :
            textSpeedFormatCps(value.cps()) + " cps";
    }

    inline const char* textSpeedSourceName(int source)
    {
        switch (source)
        {
        case 1: return "linea";
        case 2: return "personaje_lock";
        case 3: return "theme_lock";
        case 4: return "jugador";
        case 5: return "personaje";
        case 6: return "theme";
        default: return "motor";
        }
    }

    enum class TextSpeedSource
    {
        EngineDefault = 0,
        Line = 1,
        CharacterLock = 2,
        ThemeLock = 3,
        Player = 4,
        Character = 5,
        Theme = 6
    };

    struct TextSpeedResolution
    {
        TextSpeed value;
        TextSpeedSource source = TextSpeedSource::EngineDefault;
    };

    inline TextSpeedResolution resolveTextSpeed(
        const std::optional<TextSpeed>& line,
        const std::optional<TextSpeed>& character,
        bool characterLock,
        const std::optional<TextSpeed>& theme,
        bool themeLock,
        const std::optional<TextSpeed>& player,
        const TextSpeed& engineDefault = textSpeedDefault())
    {
        if (line) return { *line, TextSpeedSource::Line };
        if (characterLock && character)
            return { *character, TextSpeedSource::CharacterLock };
        if (themeLock && theme)
            return { *theme, TextSpeedSource::ThemeLock };
        if (player) return { *player, TextSpeedSource::Player };
        if (character) return { *character, TextSpeedSource::Character };
        if (theme) return { *theme, TextSpeedSource::Theme };
        return { engineDefault, TextSpeedSource::EngineDefault };
    }

    struct TextSpeedCatalogEntry
    {
        TextSpeed value;
        std::string labelEs;
        std::string labelEn;
        bool temporal = false;
    };

    inline std::vector<TextSpeedCatalogEntry> textSpeedCatalog(
        const std::optional<TextSpeed>& effective)
    {
        std::vector<TextSpeedCatalogEntry> out;
        out.push_back({ *TextSpeed::tryCps(15.0f), "Lento", "Slow", false });
        out.push_back({ *TextSpeed::tryCps(30.0f), "Normal", "Normal", false });
        out.push_back({ *TextSpeed::tryCps(60.0f), "Rapido", "Fast", false });

        if (effective && effective->isCps())
        {
            bool canonical = false;
            for (const auto& e : out)
                if (e.value == *effective) canonical = true;
            if (!canonical)
            {
                const std::string label = textSpeedFormatCps(effective->cps()) + " cps";
                TextSpeedCatalogEntry temporary{ *effective, label, label, true };
                auto it = out.begin();
                while (it != out.end() && it->value.isCps() &&
                       it->value.cps() < effective->cps()) ++it;
                out.insert(it, std::move(temporary));
            }
        }

        out.push_back({ TextSpeed::instantaneo(), "Instantaneo", "Instant", false });
        return out;
    }

    inline int textSpeedCatalogIndex(const std::vector<TextSpeedCatalogEntry>& catalog,
                                     const TextSpeed& value)
    {
        for (int i = 0; i < static_cast<int>(catalog.size()); ++i)
            if (catalog[i].value == value) return i;
        return -1;
    }

    inline int textSpeedAdjustIndex(int current, int direction, int count)
    {
        if (count <= 0) return 0;
        const int next = current + direction;
        return std::max(0, std::min(next, count - 1));
    }

    // Ley pura del typewriter para una unidad sin pausa ni wait-click. El
    // caller conserva el timer para CPS y puede detenerse entre glifos por
    // efectos de RichText. Instantaneo ignora dt y revela el resto del lote.
    inline int revealTextSpeed(const TextSpeed& speed, int current, int total,
                               float dt, float& timer)
    {
        if (current < 0) current = 0;
        if (total < current) total = current;
        if (speed.isInstantaneo())
        {
            timer = 0.0f;
            return total;
        }

        if (dt > 0.0f) timer += dt;
        const double interval = 1.0 / static_cast<double>(speed.cps());
        while (current < total && timer >= interval)
        {
            ++current;
            timer -= static_cast<float>(interval);
        }
        return current;
    }

} // namespace eclips
