// ============================================================================
// Gameplay2DAbility.h — habilidad, uso y seleccion pura de Gameplay 2D
// ============================================================================
//
// Casa local y neutral para la primitiva actor + habilidad + objetivo.
// No depende de SDL, Lua, MapData ni de una clase de torre.
//
// F1 / perfil distribuido: QUEDA.
// Es una capacidad determinista, declarativa y reimplementable sin depender
// del anfitrion. No abre filesystem, red, proceso ni una API publica de QA.
// ============================================================================

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace eclips
{

enum class Gameplay2DTargetPreference
{
    Nearest,
    Farthest,
    HighestCurrentHp,
    LowestCurrentHp,
    HighestRouteIndex,
    LowestRouteIndex
};

// Vocabulario publico compartido por .eclmap y bindings runtime. Las otras
// cuatro preferencias del enum existen solo para el adapter legacy y no deben
// filtrarse accidentalmente como nombres nuevos.
inline std::optional<Gameplay2DTargetPreference>
parsePublicGameplay2DTargetPreference(const std::string& token)
{
    std::string value = token;
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) {
            return c >= 'A' && c <= 'Z'
                ? static_cast<char>(c - 'A' + 'a')
                : static_cast<char>(c);
        });
    if (value == "mas_cercano" || value == "nearest")
        return Gameplay2DTargetPreference::Nearest;
    if (value == "mayor_hp_actual" || value == "highest_current_hp")
        return Gameplay2DTargetPreference::HighestCurrentHp;
    return std::nullopt;
}

enum class Gameplay2DTargetRelation
{
    Opposing
};

enum class Gameplay2DTargetCapability
{
    Damageable
};

enum class Gameplay2DTargetBinding
{
    Independent,
    CurrentLocomotionBlocker
};

enum class Gameplay2DCommittedDirectDamageCoordination
{
    Independent,
    SkipCoveredTarget
};

struct Gameplay2DCoordination
{
    // Ausencia conserva el dialecto historico independiente. El enum explicito
    // permite que un uso vuelva a `independiente` sobre una definicion opt-in.
    std::optional<Gameplay2DCommittedDirectDamageCoordination>
        committedDirectDamage;
};

// Dominio de direccion, no clase de actor, faccion ni taxonomia universal.
// Desambigua espacios de ID locales que pueden reutilizar el mismo entero.
enum class Gameplay2DActorDomain
{
    Player = 0,
    Enemy,
    Placed
};

struct Gameplay2DActorAddress
{
    Gameplay2DActorDomain domain = Gameplay2DActorDomain::Player;
    int localId = -1;

    bool valid() const
    {
        return localId >= 0;
    }
};

inline bool operator==(const Gameplay2DActorAddress& a,
                       const Gameplay2DActorAddress& b)
{
    return a.domain == b.domain && a.localId == b.localId;
}

inline bool operator!=(const Gameplay2DActorAddress& a,
                       const Gameplay2DActorAddress& b)
{
    return !(a == b);
}

inline bool operator<(const Gameplay2DActorAddress& a,
                      const Gameplay2DActorAddress& b)
{
    // Desempate firmado: menor ID local; solo si empata, orden de dominio.
    if (a.localId != b.localId)
        return a.localId < b.localId;
    return static_cast<int>(a.domain) < static_cast<int>(b.domain);
}

// Conjunto neutral de causas que impiden a un owner emitir uno o varios usos.
// El handle lo asigna el adapter runtime: este nucleo solo conserva identidad,
// scope y retirada aislada. Un scope vacio significa "todos los usos".
class Gameplay2DActivityInhibitionSet
{
public:
    struct Entry
    {
        int handle = -1;
        std::vector<std::string> useIds;
    };

    bool add(int handle, std::vector<std::string> useIds)
    {
        if (handle <= 0)
            return false;
        if (std::any_of(
                m_entries.begin(), m_entries.end(),
                [&](const Entry& entry) {
                    return entry.handle == handle;
                }))
        {
            return false;
        }
        if (std::any_of(
                useIds.begin(), useIds.end(),
                [](const std::string& useId) {
                    return useId.empty();
                }))
        {
            return false;
        }

        std::sort(useIds.begin(), useIds.end());
        useIds.erase(
            std::unique(useIds.begin(), useIds.end()),
            useIds.end());
        m_entries.push_back({ handle, std::move(useIds) });
        return true;
    }

    bool remove(int handle)
    {
        const auto entry = std::find_if(
            m_entries.begin(), m_entries.end(),
            [&](const Entry& candidate) {
                return candidate.handle == handle;
            });
        if (entry == m_entries.end())
            return false;
        m_entries.erase(entry);
        return true;
    }

    bool blocks(const std::string& useId) const
    {
        return std::any_of(
            m_entries.begin(), m_entries.end(),
            [&](const Entry& entry) {
                return entry.useIds.empty() ||
                    std::binary_search(
                        entry.useIds.begin(), entry.useIds.end(), useId);
            });
    }

    bool empty() const
    {
        return m_entries.empty();
    }

    std::size_t size() const
    {
        return m_entries.size();
    }

private:
    std::vector<Entry> m_entries;
};

// Factores runtime sobre el intervalo de una actividad. No conocen HP,
// recursos, presentacion ni la causa que los aplica. La composicion es
// multiplicativa y se ordena por handle para que dos causas apiladas no
// dependan del orden incidental del caller.
class Gameplay2DActivityIntervalFactorSet
{
public:
    struct Entry
    {
        int handle = -1;
        float factor = 1.0f;
    };

    bool add(int handle, float factor, float baseInterval)
    {
        if (handle <= 0 ||
            !std::isfinite(factor) || factor <= 0.0f ||
            !std::isfinite(baseInterval) || baseInterval <= 0.0f)
        {
            return false;
        }
        if (std::any_of(
                m_entries.begin(), m_entries.end(),
                [&](const Entry& entry) {
                    return entry.handle == handle;
                }))
        {
            return false;
        }

        long double candidate = static_cast<long double>(baseInterval) *
            static_cast<long double>(factor);
        for (const auto& entry : m_entries)
            candidate *= static_cast<long double>(entry.factor);
        const float composed = static_cast<float>(candidate);
        if (!std::isfinite(composed) || composed <= 0.0f)
            return false;

        m_entries.push_back({ handle, factor });
        std::sort(
            m_entries.begin(), m_entries.end(),
            [](const Entry& a, const Entry& b) {
                return a.handle < b.handle;
            });
        return true;
    }

    bool remove(int handle)
    {
        const auto entry = std::find_if(
            m_entries.begin(), m_entries.end(),
            [&](const Entry& candidate) {
                return candidate.handle == handle;
            });
        if (entry == m_entries.end())
            return false;
        m_entries.erase(entry);
        return true;
    }

    float effectiveInterval(float baseInterval) const
    {
        long double result = static_cast<long double>(baseInterval);
        for (const auto& entry : m_entries)
            result *= static_cast<long double>(entry.factor);
        const float composed = static_cast<float>(result);
        return std::isfinite(composed) && composed > 0.0f
            ? composed
            : baseInterval;
    }

    bool empty() const
    {
        return m_entries.empty();
    }

    std::size_t size() const
    {
        return m_entries.size();
    }

private:
    std::vector<Entry> m_entries;
};

// Factores runtime sobre el alcance de una actividad. Es una magnitud distinta
// del intervalo: modifica solo la elegibilidad espacial de la proxima
// adquisicion. No conoce HP, cooldown, recursos, presentacion ni efectos ya
// comprometidos. Una base cero se rechaza porque multiplicarla crearia un
// handle valido pero mecanicamente inerte; I1 sigue siendo la compuerta para
// impedir una actividad completa.
class Gameplay2DActivityRangeFactorSet
{
public:
    struct Entry
    {
        int handle = -1;
        float factor = 1.0f;
    };

    bool add(int handle, float factor, float baseRange)
    {
        if (handle <= 0 ||
            !std::isfinite(factor) || factor <= 0.0f ||
            !std::isfinite(baseRange) || baseRange <= 0.0f)
        {
            return false;
        }
        if (std::any_of(
                m_entries.begin(), m_entries.end(),
                [&](const Entry& entry) {
                    return entry.handle == handle;
                }))
        {
            return false;
        }

        long double candidate = static_cast<long double>(baseRange) *
            static_cast<long double>(factor);
        for (const auto& entry : m_entries)
            candidate *= static_cast<long double>(entry.factor);
        const float composed = static_cast<float>(candidate);
        if (!std::isfinite(composed) || composed <= 0.0f)
            return false;

        m_entries.push_back({ handle, factor });
        std::sort(
            m_entries.begin(), m_entries.end(),
            [](const Entry& a, const Entry& b) {
                return a.handle < b.handle;
            });
        return true;
    }

    bool remove(int handle)
    {
        const auto entry = std::find_if(
            m_entries.begin(), m_entries.end(),
            [&](const Entry& candidate) {
                return candidate.handle == handle;
            });
        if (entry == m_entries.end())
            return false;
        m_entries.erase(entry);
        return true;
    }

    float effectiveRange(float baseRange) const
    {
        long double result = static_cast<long double>(baseRange);
        for (const auto& entry : m_entries)
            result *= static_cast<long double>(entry.factor);
        const float composed = static_cast<float>(result);
        return std::isfinite(composed) && composed > 0.0f
            ? composed
            : baseRange;
    }

    bool empty() const
    {
        return m_entries.empty();
    }

    std::size_t size() const
    {
        return m_entries.size();
    }

private:
    std::vector<Entry> m_entries;
};

// Deltas runtime sobre el dano directo entero de una actividad. No conocen HP,
// recursos, partes, cooldown ni la causa que los aplica. La admision comprueba
// por separado el subconjunto negativo y el positivo: asi retirar cualquier
// combinacion de handles conserva siempre el rango cerrado [1, INT_MAX], sin
// depender de que una causa de signo contrario siga viva para compensarla.
class Gameplay2DActivityDirectDamageDeltaSet
{
public:
    struct Entry
    {
        int handle = -1;
        int delta = 0;
    };

    bool add(int handle, int delta, int baseDamage)
    {
        if (handle <= 0 || delta == 0 || baseDamage <= 0)
            return false;
        if (std::any_of(
                m_entries.begin(), m_entries.end(),
                [&](const Entry& entry) {
                    return entry.handle == handle;
                }))
        {
            return false;
        }

        std::int64_t negativeSum = delta < 0 ? delta : 0;
        std::int64_t positiveSum = delta > 0 ? delta : 0;
        for (const auto& entry : m_entries)
        {
            if (entry.delta < 0)
                negativeSum += entry.delta;
            else
                positiveSum += entry.delta;
        }

        const std::int64_t base = baseDamage;
        if (base + negativeSum < 1 ||
            base + positiveSum > std::numeric_limits<int>::max())
        {
            return false;
        }

        m_entries.push_back({ handle, delta });
        std::sort(
            m_entries.begin(), m_entries.end(),
            [](const Entry& a, const Entry& b) {
                return a.handle < b.handle;
            });
        return true;
    }

    bool remove(int handle)
    {
        const auto entry = std::find_if(
            m_entries.begin(), m_entries.end(),
            [&](const Entry& candidate) {
                return candidate.handle == handle;
            });
        if (entry == m_entries.end())
            return false;
        m_entries.erase(entry);
        return true;
    }

    int effectiveDirectDamage(int baseDamage) const
    {
        if (baseDamage <= 0)
            return baseDamage;

        std::int64_t result = baseDamage;
        for (const auto& entry : m_entries)
            result += entry.delta;
        return result >= 1 &&
               result <= std::numeric_limits<int>::max()
            ? static_cast<int>(result)
            : baseDamage;
    }

    bool empty() const
    {
        return m_entries.empty();
    }

    std::size_t size() const
    {
        return m_entries.size();
    }

private:
    std::vector<Entry> m_entries;
};

// Overrides runtime absolutos sobre la preferencia de una actividad. No son
// una lista de criterios compuestos: cada causa aporta un valor completo y la
// ultima insertada que siga viva gobierna. El handle conserva identidad para
// retirada exacta, pero su valor numerico no constituye precedencia.
class Gameplay2DActivityPreferenceOverrideSet
{
public:
    struct Entry
    {
        int handle = -1;
        Gameplay2DTargetPreference preference =
            Gameplay2DTargetPreference::Nearest;
    };

    bool add(int handle, Gameplay2DTargetPreference preference)
    {
        if (handle <= 0 || !isValidPreference(preference))
            return false;
        if (std::any_of(
                m_entries.begin(), m_entries.end(),
                [&](const Entry& entry) {
                    return entry.handle == handle;
                }))
        {
            return false;
        }

        m_entries.push_back({ handle, preference });
        return true;
    }

    bool remove(int handle)
    {
        const auto entry = std::find_if(
            m_entries.begin(), m_entries.end(),
            [&](const Entry& candidate) {
                return candidate.handle == handle;
            });
        if (entry == m_entries.end())
            return false;
        m_entries.erase(entry);
        return true;
    }

    Gameplay2DTargetPreference effectivePreference(
        Gameplay2DTargetPreference basePreference) const
    {
        return m_entries.empty()
            ? basePreference
            : m_entries.back().preference;
    }

    bool empty() const
    {
        return m_entries.empty();
    }

    std::size_t size() const
    {
        return m_entries.size();
    }

private:
    static bool isValidPreference(Gameplay2DTargetPreference preference)
    {
        switch (preference)
        {
        case Gameplay2DTargetPreference::Nearest:
        case Gameplay2DTargetPreference::Farthest:
        case Gameplay2DTargetPreference::HighestCurrentHp:
        case Gameplay2DTargetPreference::LowestCurrentHp:
        case Gameplay2DTargetPreference::HighestRouteIndex:
        case Gameplay2DTargetPreference::LowestRouteIndex:
            return true;
        }
        return false;
    }

    // El orden de insercion es semantica LIFO deliberada.
    std::vector<Entry> m_entries;
};

inline bool gameplay2DActivityReady(
    float elapsed, float effectiveInterval)
{
    return std::isfinite(elapsed) && elapsed >= 0.0f &&
        std::isfinite(effectiveInterval) && effectiveInterval > 0.0f &&
        elapsed >= effectiveInterval;
}

// La habilidad solicita estos filtros. El adapter del mundo aporta los hechos
// relation/capabilities de cada candidato desde el mismo snapshot.
struct Gameplay2DAcquisition
{
    std::optional<Gameplay2DTargetRelation> relation;
    std::optional<Gameplay2DTargetCapability> capability;
    // Ausencia conserva la adquisicion historica. El valor Independent
    // permite que un uso desactive un vinculo heredado de su definicion.
    std::optional<Gameplay2DTargetBinding> binding;
};

inline bool hasGameplay2DAcquisition(
    const Gameplay2DAcquisition& acquisition)
{
    return acquisition.relation.has_value() ||
        acquisition.capability.has_value() ||
        acquisition.binding.has_value();
}

enum class Gameplay2DAbilityField : std::uint32_t
{
    Range = 0,
    FireRate,
    Projectile,
    DirectDamage,
    Preference,
    AcquisitionRelation,
    AcquisitionCapability,
    AcquisitionBinding,
    CoordinationCommittedDirectDamage
};

using Gameplay2DAbilityFieldMask = std::uint32_t;

constexpr Gameplay2DAbilityFieldMask gameplay2DAbilityFieldBit(
    Gameplay2DAbilityField field)
{
    return Gameplay2DAbilityFieldMask{ 1u } <<
        static_cast<Gameplay2DAbilityFieldMask>(field);
}

constexpr bool gameplay2DAbilityFieldWasAuthored(
    Gameplay2DAbilityFieldMask mask,
    Gameplay2DAbilityField field)
{
    return (mask & gameplay2DAbilityFieldBit(field)) != 0u;
}

struct Gameplay2DAbilityValues
{
    float range = 200.0f;
    float fireRate = 0.0f;
    std::string projectile;
    std::optional<int> directDamage;
    Gameplay2DTargetPreference preference =
        Gameplay2DTargetPreference::Nearest;
    Gameplay2DAcquisition acquisition;
    Gameplay2DCoordination coordination;
};

struct Gameplay2DAbilityOverrides
{
    std::optional<float> range;
    std::optional<float> fireRate;
    std::optional<std::string> projectile;
    std::optional<int> directDamage;
    std::optional<Gameplay2DTargetPreference> preference;
    Gameplay2DAcquisition acquisition;
    Gameplay2DCoordination coordination;
};

struct Gameplay2DAbilityDef
{
    std::string id;
    Gameplay2DAbilityValues values;
};

struct Gameplay2DAbilityUseDef
{
    std::string id;
    std::string abilityId;
    Gameplay2DAbilityOverrides overrides;
    Gameplay2DAbilityValues effective;
    bool implicitLegacy = false;
};

struct Gameplay2DLegacyAbilitySource
{
    Gameplay2DAbilityValues values;
    Gameplay2DAbilityFieldMask authoredFields = 0u;
};

struct Gameplay2DAbilityResolution
{
    std::vector<Gameplay2DAbilityUseDef> uses;
    std::vector<std::string> missingAbilityIds;
    std::vector<std::string> conflictingEffectUseIds;
    bool mixedLegacyAndExplicit = false;
};

inline bool gameplay2DAbilityHasProjectile(
    const Gameplay2DAbilityValues& values)
{
    return !values.projectile.empty();
}

inline bool gameplay2DAbilityHasDirectDamage(
    const Gameplay2DAbilityValues& values)
{
    return values.directDamage.has_value() &&
        *values.directDamage > 0;
}

inline bool gameplay2DAbilityHasConflictingEffects(
    const Gameplay2DAbilityValues& values)
{
    return gameplay2DAbilityHasProjectile(values) &&
        gameplay2DAbilityHasDirectDamage(values);
}

inline bool gameplay2DAbilityHasExecutableEffect(
    const Gameplay2DAbilityValues& values)
{
    return gameplay2DAbilityHasProjectile(values) ||
        gameplay2DAbilityHasDirectDamage(values);
}

inline Gameplay2DAbilityValues applyGameplay2DAbilityOverrides(
    const Gameplay2DAbilityValues& base,
    const Gameplay2DAbilityOverrides& overrides)
{
    Gameplay2DAbilityValues effective = base;
    if (overrides.range)
        effective.range = *overrides.range;
    if (overrides.fireRate)
        effective.fireRate = *overrides.fireRate;
    if (overrides.projectile)
        effective.projectile = *overrides.projectile;
    if (overrides.directDamage)
        effective.directDamage = *overrides.directDamage;
    if (overrides.preference)
        effective.preference = *overrides.preference;
    if (overrides.acquisition.relation)
        effective.acquisition.relation =
            *overrides.acquisition.relation;
    if (overrides.acquisition.capability)
        effective.acquisition.capability =
            *overrides.acquisition.capability;
    if (overrides.acquisition.binding)
        effective.acquisition.binding =
            *overrides.acquisition.binding;
    if (overrides.coordination.committedDirectDamage)
        effective.coordination.committedDirectDamage =
            *overrides.coordination.committedDirectDamage;
    return effective;
}

inline Gameplay2DAbilityResolution resolveGameplay2DAbilityUses(
    const std::vector<Gameplay2DAbilityDef>& definitions,
    const std::vector<Gameplay2DAbilityUseDef>& authoredUses,
    const Gameplay2DLegacyAbilitySource& legacy,
    bool explicitBlockSeen)
{
    Gameplay2DAbilityResolution result;

    if (!explicitBlockSeen)
    {
        Gameplay2DAbilityUseDef implicit;
        implicit.id = "legacy_attack";
        implicit.effective = legacy.values;
        implicit.implicitLegacy = true;
        result.uses.push_back(std::move(implicit));
        return result;
    }

    result.mixedLegacyAndExplicit = legacy.authoredFields != 0u;
    result.uses.reserve(authoredUses.size());

    for (const auto& authored : authoredUses)
    {
        const auto definition = std::find_if(
            definitions.begin(), definitions.end(),
            [&](const Gameplay2DAbilityDef& candidate) {
                return candidate.id == authored.abilityId;
            });

        if (definition == definitions.end())
        {
            result.missingAbilityIds.push_back(authored.abilityId);
            continue;
        }

        Gameplay2DAbilityUseDef resolved = authored;
        resolved.effective = applyGameplay2DAbilityOverrides(
            definition->values, authored.overrides);
        resolved.implicitLegacy = false;
        if (gameplay2DAbilityHasConflictingEffects(resolved.effective))
        {
            result.conflictingEffectUseIds.push_back(resolved.id);
            continue;
        }
        result.uses.push_back(std::move(resolved));
    }

    return result;
}

struct Gameplay2DDirectDamageEffect
{
    Gameplay2DActorAddress sourceAddress;
    std::string useId;
    Gameplay2DActorAddress targetAddress;
    int amount = 0;
};

struct Gameplay2DTargetCandidate
{
    Gameplay2DActorAddress address;
    float centerX = 0.0f;
    float centerY = 0.0f;
    int currentHp = 0;
    bool alive = true;
    bool invulnerable = false;
    bool hasRoute = false;
    int routeIndex = 0;
    std::optional<Gameplay2DTargetRelation> relation;
    std::vector<Gameplay2DTargetCapability> capabilities;
};

// Coste autorado y neutral de un commit. La clave no impone combustible,
// municion, energia u oro; el adapter aporta el snapshot numerico de su mundo.
struct Gameplay2DActivityCommitCost
{
    std::string resourceKey;
    std::int64_t amount = 0;
};

struct Gameplay2DCommitResourceBalance
{
    std::string resourceKey;
    std::int64_t amount = 0;
};

struct Gameplay2DTargetRequest
{
    int actorId = -1;
    std::string useId;
    float sourceX = 0.0f;
    float sourceY = 0.0f;
    float range = 0.0f;
    Gameplay2DTargetPreference preference =
        Gameplay2DTargetPreference::Nearest;
    Gameplay2DAcquisition acquisition;
    // Restriccion interna del adapter. No es una capacidad publica: conserva
    // el dialecto legacy player-only dentro del mismo orden de fase.
    std::optional<Gameplay2DActorDomain> requiredTargetDomain;
    // Restriccion exacta aportada por el contexto del owner. Una direccion
    // invalida presente significa deliberadamente "sin blanco": nunca hace
    // fallback a otro candidato elegible.
    std::optional<Gameplay2DActorAddress> requiredTargetAddress;
    Gameplay2DCoordination coordination;
    // Todo uso de dano directo aporta al ledger, aunque seleccione de manera
    // independiente. Proyectiles y usos sin efecto directo conservan cero.
    int directDamage = 0;
    std::vector<Gameplay2DActivityCommitCost> commitCosts;
};

struct Gameplay2DTargetDecision
{
    int actorId = -1;
    std::string useId;
    Gameplay2DActorAddress targetAddress;
    // Costes agregados y reservados por una decision valida. Vacio significa
    // actividad gratuita o decision rechazada/sin blanco.
    std::vector<Gameplay2DActivityCommitCost> committedCosts;
};

inline bool reserveGameplay2DCommitResources(
    const std::vector<Gameplay2DActivityCommitCost>& costs,
    const std::vector<Gameplay2DCommitResourceBalance>& balances,
    std::vector<Gameplay2DCommitResourceBalance>& reservations,
    std::vector<Gameplay2DActivityCommitCost>& committedCosts)
{
    committedCosts.clear();
    if (costs.empty())
        return true;

    std::vector<Gameplay2DActivityCommitCost> aggregated;
    aggregated.reserve(costs.size());
    for (const auto& cost : costs)
    {
        if (cost.resourceKey.empty() || cost.amount <= 0)
            return false;

        const auto existing = std::find_if(
            aggregated.begin(), aggregated.end(),
            [&](const Gameplay2DActivityCommitCost& entry) {
                return entry.resourceKey == cost.resourceKey;
            });
        if (existing == aggregated.end())
        {
            aggregated.push_back(cost);
            continue;
        }
        if (cost.amount >
            std::numeric_limits<std::int64_t>::max() - existing->amount)
        {
            return false;
        }
        existing->amount += cost.amount;
    }
    std::sort(
        aggregated.begin(), aggregated.end(),
        [](const Gameplay2DActivityCommitCost& a,
           const Gameplay2DActivityCommitCost& b) {
            return a.resourceKey < b.resourceKey;
        });

    // Primero valida el conjunto completo; ninguna reserva parcial sobrevive
    // si falta una de las claves o su saldo no alcanza.
    for (const auto& cost : aggregated)
    {
        const auto balance = std::find_if(
            balances.begin(), balances.end(),
            [&](const Gameplay2DCommitResourceBalance& entry) {
                return entry.resourceKey == cost.resourceKey;
            });
        if (balance == balances.end() || balance->amount < 0)
            return false;

        const auto reserved = std::find_if(
            reservations.begin(), reservations.end(),
            [&](const Gameplay2DCommitResourceBalance& entry) {
                return entry.resourceKey == cost.resourceKey;
            });
        const std::int64_t reservedAmount =
            reserved == reservations.end() ? 0 : reserved->amount;
        if (reservedAmount > balance->amount ||
            cost.amount > balance->amount - reservedAmount)
        {
            return false;
        }
    }

    for (const auto& cost : aggregated)
    {
        const auto reserved = std::find_if(
            reservations.begin(), reservations.end(),
            [&](const Gameplay2DCommitResourceBalance& entry) {
                return entry.resourceKey == cost.resourceKey;
            });
        if (reserved == reservations.end())
            reservations.push_back({ cost.resourceKey, cost.amount });
        else
            reserved->amount += cost.amount;
    }
    committedCosts = std::move(aggregated);
    return true;
}

inline Gameplay2DActorAddress selectGameplay2DTarget(
    const Gameplay2DTargetRequest& request,
    const std::vector<Gameplay2DTargetCandidate>& candidates)
{
    if (!std::isfinite(request.sourceX) ||
        !std::isfinite(request.sourceY) ||
        !std::isfinite(request.range) ||
        request.range < 0.0f)
    {
        return {};
    }

    const float rangeSquared = request.range * request.range;
    bool found = false;
    float bestScore = 0.0f;
    Gameplay2DActorAddress bestAddress;

    for (const auto& candidate : candidates)
    {
        if (!candidate.address.valid() ||
            !candidate.alive || candidate.invulnerable ||
            !std::isfinite(candidate.centerX) ||
            !std::isfinite(candidate.centerY))
        {
            continue;
        }

        if (request.requiredTargetDomain &&
            candidate.address.domain !=
                *request.requiredTargetDomain)
        {
            continue;
        }

        if (request.requiredTargetAddress &&
            candidate.address != *request.requiredTargetAddress)
        {
            continue;
        }

        if (request.acquisition.relation &&
            candidate.relation != request.acquisition.relation)
        {
            continue;
        }

        if (request.acquisition.capability &&
            std::find(candidate.capabilities.begin(),
                      candidate.capabilities.end(),
                      *request.acquisition.capability) ==
                candidate.capabilities.end())
        {
            continue;
        }

        const float dx = candidate.centerX - request.sourceX;
        const float dy = candidate.centerY - request.sourceY;
        const float distanceSquared = dx * dx + dy * dy;
        if (!std::isfinite(distanceSquared) ||
            distanceSquared > rangeSquared)
        {
            continue;
        }

        float score = 0.0f;
        bool lowerWins = false;

        switch (request.preference)
        {
        case Gameplay2DTargetPreference::Nearest:
            score = distanceSquared;
            lowerWins = true;
            break;
        case Gameplay2DTargetPreference::Farthest:
            score = distanceSquared;
            break;
        case Gameplay2DTargetPreference::HighestCurrentHp:
            score = static_cast<float>(candidate.currentHp);
            break;
        case Gameplay2DTargetPreference::LowestCurrentHp:
            score = static_cast<float>(candidate.currentHp);
            lowerWins = true;
            break;
        case Gameplay2DTargetPreference::HighestRouteIndex:
            if (!candidate.hasRoute) continue;
            score = static_cast<float>(candidate.routeIndex);
            break;
        case Gameplay2DTargetPreference::LowestRouteIndex:
            if (!candidate.hasRoute) continue;
            score = static_cast<float>(candidate.routeIndex);
            lowerWins = true;
            break;
        }

        const bool betterScore = !found ||
            (lowerWins ? score < bestScore : score > bestScore);
        const bool tiedScore = found && score == bestScore;
        if (betterScore ||
            (tiedScore && candidate.address < bestAddress))
        {
            found = true;
            bestScore = score;
            bestAddress = candidate.address;
        }
    }

    return found ? bestAddress : Gameplay2DActorAddress{};
}

inline std::vector<Gameplay2DTargetDecision> selectGameplay2DTargets(
    const std::vector<Gameplay2DTargetRequest>& requests,
    const std::vector<Gameplay2DTargetCandidate>& candidates,
    const std::vector<Gameplay2DCommitResourceBalance>& resourceBalances = {})
{
    std::vector<Gameplay2DTargetRequest> ordered = requests;
    std::sort(
        ordered.begin(), ordered.end(),
        [](const Gameplay2DTargetRequest& a,
           const Gameplay2DTargetRequest& b) {
            if (a.actorId != b.actorId)
                return a.actorId < b.actorId;
            return a.useId < b.useId;
        });

    std::vector<Gameplay2DTargetDecision> decisions;
    decisions.reserve(ordered.size());

    // Ledger local a esta llamada/fase. Se alimenta despues de cada decision
    // valida, nunca desde estado vivo ni desde impactos futuros.
    std::vector<std::pair<Gameplay2DActorAddress, std::int64_t>>
        committedDirectDamage;
    committedDirectDamage.reserve(ordered.size());
    std::vector<Gameplay2DCommitResourceBalance> resourceReservations;

    for (const auto& request : ordered)
    {
        const auto policy =
            request.coordination.committedDirectDamage.value_or(
                Gameplay2DCommittedDirectDamageCoordination::Independent);

        Gameplay2DActorAddress targetAddress;
        if (request.directDamage > 0 &&
            policy ==
            Gameplay2DCommittedDirectDamageCoordination::SkipCoveredTarget)
        {
            std::vector<Gameplay2DTargetCandidate> uncoveredCandidates;
            uncoveredCandidates.reserve(candidates.size());
            for (const auto& candidate : candidates)
            {
                const auto committed = std::find_if(
                    committedDirectDamage.begin(),
                    committedDirectDamage.end(),
                    [&](const auto& entry) {
                        return entry.first == candidate.address;
                    });
                if (committed != committedDirectDamage.end() &&
                    committed->second >=
                        static_cast<std::int64_t>(candidate.currentHp))
                {
                    continue;
                }
                uncoveredCandidates.push_back(candidate);
            }
            targetAddress =
                selectGameplay2DTarget(request, uncoveredCandidates);
        }
        else
        {
            targetAddress =
                selectGameplay2DTarget(request, candidates);
        }

        std::vector<Gameplay2DActivityCommitCost> committedCosts;
        if (targetAddress.valid() &&
            !reserveGameplay2DCommitResources(
                request.commitCosts,
                resourceBalances,
                resourceReservations,
                committedCosts))
        {
            // Una oportunidad no asequible no es un commit: no conserva
            // target, no reserva dano A1 y no afecta recursos posteriores.
            targetAddress = {};
        }

        decisions.push_back({
            request.actorId,
            request.useId,
            targetAddress,
            std::move(committedCosts)
        });

        if (!targetAddress.valid() || request.directDamage <= 0)
            continue;

        const auto committed = std::find_if(
            committedDirectDamage.begin(),
            committedDirectDamage.end(),
            [&](const auto& entry) {
                return entry.first == targetAddress;
            });
        if (committed == committedDirectDamage.end())
        {
            committedDirectDamage.push_back({
                targetAddress,
                static_cast<std::int64_t>(request.directDamage)
            });
        }
        else
        {
            committed->second +=
                static_cast<std::int64_t>(request.directDamage);
        }
    }
    return decisions;
}

} // namespace eclips
