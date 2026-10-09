// ============================================================================
// ECLIPS ENGINE - FontManager
// ============================================================================
// Registro de fuentes nombradas para el VN. Mapea un nombre que el creador
// inventa ("titulo", "terminal") a un TextRenderer ya cargado (un atlas de
// glifos rasterizado a un tamaño concreto).
//
// Por qué existe: TextRenderer::init(path, size) hornea el tamaño en el
// atlas — no se puede reescalar sin perder nitidez. Por eso "otra fuente" y
// "otro tamaño" son el MISMO recurso: un par (ttf, tamaño) = un atlas =
// un TextRenderer. El FontManager posee N de esos, indexados por nombre.
//
// Propiedad: map<string, unique_ptr<TextRenderer>>. TextRenderer NO es
// copiable (posee un texID de GPU); el unique_ptr lo mantiene en un sitio
// estable y libera el atlas solo al morir el manager (shutdown gratis).
//
// Carga perezosa declarativa: solo se cargan las fuentes que el .ecl declara
// con el comando 'fuente'. Una fuente puesta en assets pero nunca declarada
// no consume RAM. Alineado con el norte de frugalidad del motor.
//
// GL-safety: load() sube una textura a la GPU → SOLO debe llamarse en fase
// GL-segura (update/render), nunca en parse-time. El comando 'fuente' es
// ejecutable (no declarativo-en-parse) justamente por esto (§1.34).
// ============================================================================
#pragma once
#include "../core/FontDeclaration.h"
#include "../core/LangPaths.h"
#include "FontRutaEspejo.h"
#include "../core/Types.h"
#include "TextRenderer.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace eclips
{
    class FontManager;

    // Registro store→manager para que el grow excepcional (FontAtlasExpand)
    // pueda contabilizar/soltar atlas anonimos getSized sin acoplar el API
    // de Ensure a FontManager. Un store : un manager vivo (harness y Engine).
    inline std::unordered_map<FontSourceStore*, FontManager*>& fontManagerByStore()
    {
        static std::unordered_map<FontSourceStore*, FontManager*> m;
        return m;
    }

    inline FontManager* fontManagerForStore(FontSourceStore* store)
    {
        if (!store) return nullptr;
        auto it = fontManagerByStore().find(store);
        return it == fontManagerByStore().end() ? nullptr : it->second;
    }

    class FontManager
    {
    public:
        FontManager() = default;
        explicit FontManager(FontSourceStore* sourceStore)
            : m_sourceStore(sourceStore)
        {
            if (m_sourceStore)
                fontManagerByStore()[m_sourceStore] = this;
        }
        ~FontManager()
        {
            if (!m_sourceStore) return;
            auto& m = fontManagerByStore();
            auto it = m.find(m_sourceStore);
            if (it != m.end() && it->second == this)
                m.erase(it);
        }
        FontManager(const FontManager&) = delete;
        FontManager& operator=(const FontManager&) = delete;
        void setSourceStore(FontSourceStore* sourceStore)
        {
            if (m_sourceStore && m_sourceStore != sourceStore)
            {
                auto& m = fontManagerByStore();
                auto it = m.find(m_sourceStore);
                if (it != m.end() && it->second == this)
                    m.erase(it);
            }
            m_sourceStore = sourceStore;
            if (m_sourceStore)
                fontManagerByStore()[m_sourceStore] = this;
        }

        // Declara y carga una fuente nombrada. GL-safe only (ver cabecera).
        // Si el nombre ya existe, NO recarga (idempotente — declarar dos veces
        // la misma fuente no cuesta un segundo atlas). Devuelve true si la
        // fuente quedó disponible (recién cargada o ya existente).
        // CP4 (DISENO_IDIOMAS §6): la RUTA se resuelve contra el árbol espejo;
        // la CLAVE de este mapa sigue siendo el NOMBRE que escribió el creador.
        // El §6 dice "la ruta resuelta ES la clave de caché" y acá eso es
        // falso — y está bien que lo sea: `fuente titulo assets/x.ttf` tiene
        // que seguir respondiendo a `titulo` aunque el `.ttf` venga de
        // `idiomas/ja/`. Lo que SÍ hereda de esa forma: `load` es idempotente
        // por nombre, así que un cambio de idioma en caliente no recarga la
        // fuente ya cargada — sale por la política del §3 (próximo load
        // natural), no por un caso especial.
        bool load(const std::string& name, const std::string& pathPedida, f32 size,
                  f32 kerning = 0.0f,
                  FontMeasure measure = FontMeasure::Height)
        {
            const std::string path = fontResolverRutaEspejo(pathPedida);

            if (name.empty())
            {
                std::cout << "[FontManager] nombre de fuente vacio, ignorado"
                          << std::endl;
                return false;
            }

            m_requestedRoutes[name] = {pathPedida};
            TextProfileSpec spec;
            spec.generation = generacionDeHorneado();
            spec.routes = {path};
            spec.repertoire = repertorioBootstrap();
            spec.fontSize = size;
            spec.kerning = kerning;
            spec.measure = measure;
            TextProfilePlanHandle plan;
            std::string fp;
            if (m_sourceStore)
            {
                plan = m_sourceStore->acquirePlan(spec);
                fp = m_sourceStore->fingerprintReached(plan);
            }
            else
                fp = fingerprintDeRuta(path, size, kerning, measure);
            if (TextRenderer* hit = vincularHuella(name, fp, &plan))
                return hit->isLoaded();

            auto tr = std::make_shared<TextRenderer>(m_sourceStore);
            const bool loaded = m_sourceStore
                ? tr->initProfile(spec)
                : tr->init(path, size, kerning, measure);
            if (!loaded)
            {
                std::cout << "[FontManager] FALLO al cargar fuente \"" << name
                    << "\" desde \"" << path << "\"" << std::endl;
                return false;
            }

            std::cout << "[FontManager] Fuente \"" << name << "\" cargada ("
                << path << " @ " << size << "px, kerning " << kerning
                << ", medida " << fontMeasureName(measure) << ")" << std::endl;

            guardarAtlas(name, std::move(tr), fp);
            return true;
        }

        // Devuelve el renderer de una fuente nombrada, o nullptr si no existe.
        // El caller decide el fallback (típicamente m_textRenderer default).
        //
        // CP-R3: primero el catálogo GLOBAL (el nombre pelado, que es lo que
        // declara `proyecto.eclips`) y sólo después el catálogo LOCAL del
        // archivo activo. Ese orden ES la precedencia firmada en `PV-R3-F2-A`:
        // el manifest es autoritativo y una declaración `.ecl` con el mismo
        // nombre no lo pisa. La caída al ámbito es lo que mantiene vivos a los
        // selectores que NO son del archivo —las cinco claves `*_fuente` del
        // tema y el `estilo { fuente X }` del manifest— sobre un nombre que
        // sólo existe en el `.ecl` que está corriendo.
        TextRenderer* get(const std::string& name)
        {
            if (TextRenderer* global = getExacto(name)) return global;
            if (m_ambito.empty()) return nullptr;
            return getExacto(claveLocal(m_ambito, name));
        }

        // Lookup SIN caída de ámbito. Es lo que consultan las tres superficies
        // de resolución tardía con la identidad que capturaron al nacer, y por
        // eso no puede mirar el ámbito: una capa creada bajo el archivo A tiene
        // que seguir dibujándose con la fuente de A después de `cargar_script`.
        TextRenderer* getExacto(const std::string& clave)
        {
            if (TextRenderer* hit = atlasDeBinding(clave)) return hit;
            return rebindDesdeGeneracionAnterior(clave);
        }

        // La puerta de las tres superficies tardías: identidad resuelta si la
        // hay, nombre crudo si no. El nombre crudo sigue vivo a propósito —
        // una capa nacida antes de que existiera el catálogo, o un `.eclmap`,
        // resuelven por ahí.
        // **Si hubo identidad, es ESA o NINGUNA.** La primera versión caía a
        // `get(name)` cuando la clave exacta no resolvía, y eso puede entregar
        // la fuente HOMÓNIMA del archivo activo: una capa nacida bajo A, cuyo
        // perfil no llegó a hornearse, se dibujaría con la fuente de B —
        // silenciosa y equivocada, que es el modo de falla que `PV-R3-F2`
        // existe para cerrar. Devolver `nullptr` deja al caller caer a su
        // renderer base, que es lo que hace desde siempre con una fuente que
        // no está. La caída por nombre crudo vale sólo cuando NUNCA hubo
        // identidad (texto nacido sin catálogo, o un `.eclmap`).
        TextRenderer* getLigado(const std::string& clave,
                                const std::string& name)
        {
            if (!clave.empty()) return getExacto(clave);
            if (name.empty()) return nullptr;
            return get(name);
        }

        // ── Identidad interna del catálogo local (CP-R3) ─────────────────
        // `{ruta de script resuelta byte a byte, nombre}`, SIN canonicalizar
        // (`DA-20`). El prefijo `\x03` sigue la convención de `getSized`: un
        // byte de control que ningún nombre inventado por el creador puede
        // tener, así que jamás colisiona con el catálogo global.
        static std::string claveLocal(const std::string& scriptPath,
                                      const std::string& name)
        {
            return std::string("\x03") + scriptPath + "\x1F" + name;
        }

        void setAmbitoActivo(const std::string& scriptPath)
        {
            m_ambito = scriptPath;
        }
        const std::string& ambitoActivo() const { return m_ambito; }

        bool loadProfile(const std::string& name, const TextProfileSpec& requested)
        {
            if (!m_sourceStore || name.empty()) return false;
            TextProfileSpec spec = requested;
            spec.generation = generacionDeHorneado();
            m_requestedRoutes[name] = spec.routes;
            const auto plan = m_sourceStore->acquirePlan(spec);
            const std::string fp = m_sourceStore->fingerprintReached(plan);
            if (TextRenderer* hit = vincularHuella(name, fp, &plan))
                return hit->isLoaded();
            auto tr = std::make_shared<TextRenderer>(m_sourceStore);
            if (!tr->initProfile(spec)) return false;
            guardarAtlas(name, std::move(tr), fp);
            return true;
        }

        // Atlas ANÓNIMO por (ttf, tamaño, kerning) — el recurso del tamaño
        // per-caja (FASE 2). A diferencia de load()/get() (nombrados por el
        // creador), la key se sintetiza y se prefija con '\x01' para no
        // colisionar JAMÁS con un nombre inventado vía 'fuente'. Get-or-create:
        // un solo recurso por terna (igual que §7.43). GL-safe only (sube textura).
        TextRenderer* getSized(const std::string& pathPedida, f32 size,
                               f32 kerning = 0.0f,
                               FontMeasure measure = FontMeasure::Height)
        {
            // CP4: acá la clave SÍ se sintetiza desde la ruta, así que resolver
            // antes de armarla es literal la letra del §6 — dos idiomas con
            // `.ttf` distintos son dos atlas distintos, sin una línea extra.
            const std::string path = fontResolverRutaEspejo(pathPedida);

            std::string key = std::string("\x01") + path + "@"
                + std::to_string(size) + "#" + std::to_string(kerning)
                + "@" + std::to_string(static_cast<int>(measure));

            TextProfilePlanHandle plan;
            std::string fp;
            if (m_sourceStore)
            {
                TextProfileSpec spec;
                spec.generation = generacionDeHorneado();
                spec.routes = {path};
                spec.repertoire = repertorioBootstrap();
                spec.fontSize = size;
                spec.kerning = kerning;
                spec.measure = measure;
                plan = m_sourceStore->acquirePlan(spec);
                fp = m_sourceStore->fingerprintReached(plan);
            }
            else
                fp = fingerprintDeRuta(path, size, kerning, measure);
            if (TextRenderer* hit = atlasPorHuella(fp, &plan)) return hit;

            auto tr = std::make_shared<TextRenderer>(m_sourceStore);
            const bool loaded = (m_sourceStore && plan)
                ? tr->initProfile(plan.spec())
                : tr->init(path, size, kerning, measure);
            if (!loaded)
            {
                std::cout << "[FontManager] getSized FALLO (" << path
                    << " @ " << size << "px)" << std::endl;
                return nullptr;
            }
            TextRenderer* raw = tr.get();
            m_requestedRoutes[key] = {pathPedida};
            guardarAtlas(key, std::move(tr), fp);
            return raw;
        }

        TextRenderer* getSized(const TextRenderer& base, f32 size,
                               f32 kerning = 0.0f,
                               FontMeasure measure = FontMeasure::Height)
        {
            if (!base.hasProfilePlan())
                return getSized(base.fontPath(), size, kerning, measure);
            if (!m_sourceStore) return nullptr;

            TextProfileSpec spec = base.profilePlan().spec();
            spec.fontSize = size;
            spec.kerning = kerning;
            spec.measure = measure;
            spec.generation = generacionDeHorneado();
            const auto plan = m_sourceStore->acquirePlan(spec);
            const std::string fp = m_sourceStore->fingerprintReached(plan);
            if (TextRenderer* hit = atlasPorHuella(fp, &plan)) return hit;
            if (!plan) return nullptr;
            auto tr = std::make_shared<TextRenderer>(m_sourceStore);
            if (!tr->initProfile(spec)) return nullptr;
            TextRenderer* raw = tr.get();
            const std::string key = std::string("\x02") + fp;
            m_requestedRoutes[key] = spec.routes;
            guardarAtlas(key, std::move(tr), fp);
            return raw;
        }

        // Deliberadamente NO const desde CP-R3: `has` tiene que contestar por
        // la MISMA ley que `get`, o el aviso "esa fuente no fue declarada"
        // saldría sobre un nombre que el catálogo local sí resuelve.
        bool has(const std::string& name)
        {
            return get(name) != nullptr;
        }

        // Declara un perfil del catálogo bajo una clave dada (global = nombre
        // pelado; local = `claveLocal`). Idempotente por clave, GL-safe only.
        // Es el ÚNICO sitio que traduce una declaración autoral a un plan del
        // store: el `.ecl` y el manifest entran los dos por acá.
        bool declararPerfil(const std::string& clave,
                            const fontdecl::Declaracion& d)
        {
            if (clave.empty() || d.routes.empty()) return false;
            m_requestedRoutes[clave] = d.routes;

            if (!m_sourceStore)
            {
                if (d.routes.size() > 1)
                {
                    std::cout << "[FontManager] perfil \"" << d.name
                              << "\" con respaldos necesita FontSourceStore"
                              << std::endl;
                    return false;
                }
                if (TextRenderer* existing = atlasDeBinding(clave))
                    return existing->isLoaded();
                auto tr = std::make_shared<TextRenderer>();
                if (!tr->init(fontResolverRutaEspejo(d.routes[0]), d.size,
                              d.kerning, d.measure))
                    return false;
                guardarAtlas(clave, std::move(tr),
                             fingerprintDeRuta(fontResolverRutaEspejo(d.routes[0]),
                                               d.size, d.kerning, d.measure));
                return true;
            }

            TextProfileSpec spec;
            spec.generation = generacionDeHorneado();
            spec.routes.reserve(d.routes.size());
            for (const auto& r : d.routes)
                spec.routes.push_back(fontResolverRutaEspejo(r));
            spec.repertoire = repertorioBootstrap();
            spec.fontSize = d.size;
            spec.kerning = d.kerning;
            spec.measure = d.measure;
            const auto plan = m_sourceStore->acquirePlan(spec);
            const std::string fp = m_sourceStore->fingerprintReached(plan);
            if (TextRenderer* hit = vincularHuella(clave, fp, &plan))
                return hit->isLoaded();

            auto tr = std::make_shared<TextRenderer>(m_sourceStore);
            if (!tr->initProfile(spec))
            {
                std::cout << "[FontManager] FALLO al hornear perfil \""
                          << d.name << "\" (" << d.routes[0] << " @ "
                          << d.size << "px)" << std::endl;
                return false;
            }
            std::cout << "[FontManager] Perfil \"" << d.name << "\" horneado ("
                      << d.routes[0] << " @ " << d.size << "px, kerning "
                      << d.kerning << ", medida " << fontMeasureName(d.measure)
                      << ", respaldos " << (d.routes.size() - 1) << ")"
                      << std::endl;
            guardarAtlas(clave, std::move(tr), fp);
            return true;
        }

        void clear()
        {
            m_bindings.clear();
            m_atlasCache.clear();
            m_requestedRoutes.clear();
            m_catalogLive.clear();
        }

        void releaseLocalIdentity(const std::string& clave)
        {
            for (auto it = m_bindings.begin(); it != m_bindings.end(); )
            {
                if (identidadDeBinding(it->first) == clave)
                    it = m_bindings.erase(it);
                else
                    ++it;
            }
            m_requestedRoutes.erase(clave);
            soltarAtlasSinBinding();
            if (m_sourceStore)
            {
                m_sourceStore->gcOrphans();
                m_sourceStore->retireOrphans();
            }
        }

        void setCatalogLive(const std::vector<std::string>& keys)
        {
            m_catalogLive.clear();
            m_catalogLive.insert(keys.begin(), keys.end());
        }

        void syncLiveLocalIdentities(const std::vector<std::string>& sceneKeys)
        {
            std::unordered_set<std::string> keep = m_catalogLive;
            keep.insert(sceneKeys.begin(), sceneKeys.end());
            // Un update puede limpiar antes del commit prerender. Los locales
            // preparados siguen siendo del candidato, aunque aun no sean live.
            const auto pending = m_sourceStore ? m_sourceStore->pendingGeneration() : 0;
            if (pending != 0)
            {
                for (const auto& kv : m_bindings)
                    if (generationOfBinding(kv.first) == pending)
                        keep.insert(identidadDeBinding(kv.first));
            }
            std::vector<std::string> drop;
            for (const auto& kv : m_bindings)
            {
                const std::string id = identidadDeBinding(kv.first);
                if (id.empty() || static_cast<unsigned char>(id[0]) != 0x03)
                    continue;
                if (!keep.count(id)) drop.push_back(id);
            }
            std::sort(drop.begin(), drop.end());
            drop.erase(std::unique(drop.begin(), drop.end()), drop.end());
            for (const auto& id : drop)
                releaseLocalIdentity(id);
        }

        void dropReplacedGeneration(std::uint64_t previous)
        {
            if (!m_sourceStore || previous == kFontLegacyGeneration) return;
            if (previous == m_sourceStore->activeGeneration()) return;
            std::unordered_set<std::string> liveNow;
            const auto active = m_sourceStore->activeGeneration();
            for (const auto& kv : m_bindings)
            {
                if (generationOfBinding(kv.first) == active)
                    liveNow.insert(identidadDeBinding(kv.first));
            }
            for (auto it = m_bindings.begin(); it != m_bindings.end(); )
            {
                if (generationOfBinding(it->first) == previous &&
                    liveNow.count(identidadDeBinding(it->first)))
                    it = m_bindings.erase(it);
                else
                    ++it;
            }
            soltarAtlasSinBinding();
            m_sourceStore->gcOrphans();
            m_sourceStore->retireOrphans();
        }

        void dropGeneration(std::uint64_t generation)
        {
            if (!m_sourceStore || generation == kFontLegacyGeneration) return;
            if (generation == m_sourceStore->activeGeneration()) return;
            for (auto it = m_bindings.begin(); it != m_bindings.end(); )
            {
                if (generationOfBinding(it->first) == generation)
                    it = m_bindings.erase(it);
                else
                    ++it;
            }
            soltarAtlasSinBinding();
            m_sourceStore->gcOrphans();
            m_sourceStore->retireOrphans();
        }

        // HL-R5-004 / CP-R5 7.4: suelta bindings anonimos getSized ('\x01')
        // de una generacion ya reemplazada y contabiliza cuantos eran.
        int dropAnonymousAtlasesOf(std::uint64_t previous)
        {
            if (!m_sourceStore || previous == kFontLegacyGeneration) return 0;
            if (previous == m_sourceStore->activeGeneration()) return 0;
            int dropped = 0;
            for (auto it = m_bindings.begin(); it != m_bindings.end(); )
            {
                if (generationOfBinding(it->first) != previous)
                {
                    ++it;
                    continue;
                }
                const std::string id = identidadDeBinding(it->first);
                if (!id.empty() &&
                    static_cast<unsigned char>(id[0]) == 0x01)
                {
                    it = m_bindings.erase(it);
                    ++dropped;
                    continue;
                }
                ++it;
            }
            if (dropped > 0)
            {
                soltarAtlasSinBinding();
                m_sourceStore->gcOrphans();
                m_sourceStore->retireOrphans();
            }
            return dropped;
        }

        TextRenderer* atlasPorHuella(
            const std::string& fingerprint,
            const TextProfilePlanHandle* plan = nullptr)
        {
            AtlasCacheEntry* entry = buscarEntrada(fingerprint, plan);
            return entry ? entry->atlas.get() : nullptr;
        }

        std::string cacheKeyOf(const TextRenderer& renderer) const
        {
            for (const auto& kv : m_atlasCache)
            {
                for (const auto& e : kv.second)
                {
                    if (e.atlas.get() == &renderer) return kv.first;
                }
            }
            return std::string();
        }

    private:
        FontSourceStore* m_sourceStore = nullptr; // no propietario
        std::string m_ambito;
        struct BindingTarget
        {
            std::string fingerprint;
            std::uint64_t entryId = 0;
        };
        struct AtlasCacheEntry
        {
            std::uint64_t id = 0;
            std::shared_ptr<TextRenderer> atlas;
        };
        std::unordered_map<std::string, BindingTarget> m_bindings;
        // Caché física por huella → bucket exacto de colisiones. Las entradas
        // son independientes de identidad y generación; cada binding apunta
        // a un id estable dentro del bucket.
        std::unordered_map<std::string, std::vector<AtlasCacheEntry>> m_atlasCache;
        std::uint64_t m_nextAtlasEntryId = 1;
        std::unordered_map<std::string, std::vector<std::string>> m_requestedRoutes;
        std::unordered_set<std::string> m_catalogLive;

        std::uint64_t generacionActiva() const
        {
            return m_sourceStore ? m_sourceStore->activeGeneration() : 1;
        }

        std::uint64_t generacionDeHorneado() const
        {
            if (!m_sourceStore) return 1;
            const auto pending = m_sourceStore->pendingGeneration();
            if (pending != 0) return pending;
            return m_sourceStore->activeGeneration();
        }

        static std::uint64_t generationOfBinding(const std::string& key)
        {
            const auto pos = key.find('\x1E');
            if (pos == std::string::npos || pos == 0) return 0;
            try { return static_cast<std::uint64_t>(std::stoull(key.substr(0, pos))); }
            catch (...) { return 0; }
        }

        static std::string claveBinding(std::uint64_t generation,
                                        const std::string& identity)
        {
            return std::to_string(generation) + "\x1E" + identity;
        }

        static std::string identidadDeBinding(const std::string& key)
        {
            const auto pos = key.find('\x1E');
            if (pos == std::string::npos) return key;
            return key.substr(pos + 1);
        }

        std::string fingerprintDeRuta(const std::string& path, f32 size,
                                      f32 kerning, FontMeasure measure)
        {
            if (!m_sourceStore)
                return std::string("\x01") + path + "@" + std::to_string(size);
            TextProfileSpec spec;
            spec.generation = generacionDeHorneado();
            spec.routes = {path};
            spec.repertoire = repertorioBootstrap();
            spec.fontSize = size;
            spec.kerning = kerning;
            spec.measure = measure;
            return m_sourceStore->effectiveFingerprint(spec);
        }

        static bool entradaCoincide(
            const AtlasCacheEntry& entry,
            const TextProfilePlanHandle* plan)
        {
            TextRenderer* renderer = entry.atlas.get();
            if (!renderer || !renderer->isLoaded()) return false;
            if (!plan || !*plan) return true;
            if (!renderer->hasProfilePlan()) return false;
            return TextProfilePlanHandle::sameEffectiveReached(
                renderer->profilePlan(), *plan);
        }

        AtlasCacheEntry* buscarEntrada(
            const std::string& fingerprint,
            const TextProfilePlanHandle* plan = nullptr)
        {
            auto it = m_atlasCache.find(fingerprint);
            if (it == m_atlasCache.end()) return nullptr;
            for (auto& entry : it->second)
                if (entradaCoincide(entry, plan)) return &entry;
            return nullptr;
        }

        AtlasCacheEntry* buscarEntradaPorId(const BindingTarget& target)
        {
            const auto it = m_atlasCache.find(target.fingerprint);
            if (it == m_atlasCache.end()) return nullptr;
            for (auto& entry : it->second)
                if (entry.id == target.entryId) return &entry;
            return nullptr;
        }

        void atar(std::uint64_t generation,
                  const std::string& identity,
                  const std::string& fingerprint,
                  std::uint64_t entryId)
        {
            m_bindings[claveBinding(generation, identity)] =
                BindingTarget{fingerprint, entryId};
        }

        TextRenderer* vincularHuella(
            const std::string& identity,
            const std::string& fingerprint,
            const TextProfilePlanHandle* plan = nullptr)
        {
            AtlasCacheEntry* entry = buscarEntrada(fingerprint, plan);
            if (!entry) return nullptr;
            atar(generacionDeHorneado(), identity, fingerprint, entry->id);
            return entry->atlas.get();
        }

        TextRenderer* atlasDeBinding(const std::string& identity)
        {
            if (m_sourceStore)
            {
                const auto pending = m_sourceStore->pendingGeneration();
                if (pending != 0)
                {
                    auto pit = m_bindings.find(claveBinding(pending, identity));
                    if (pit != m_bindings.end())
                    {
                        AtlasCacheEntry* entry = buscarEntradaPorId(pit->second);
                        if (entry && entry->atlas && entry->atlas->isLoaded())
                            return entry->atlas.get();
                    }
                }
            }
            auto it = m_bindings.find(claveBinding(generacionActiva(), identity));
            if (it == m_bindings.end()) return nullptr;
            AtlasCacheEntry* entry = buscarEntradaPorId(it->second);
            return entry && entry->atlas && entry->atlas->isLoaded()
                ? entry->atlas.get() : nullptr;
        }

        TextRenderer* rebindDesdeGeneracionAnterior(const std::string& identity)
        {
            const auto requested = m_requestedRoutes.find(identity);
            if (requested == m_requestedRoutes.end() || requested->second.empty())
                return nullptr;
            if (!m_sourceStore) return nullptr;
            TextProfileSpec spec;
            spec.generation = generacionActiva();
            spec.routes.reserve(requested->second.size());
            for (const auto& r : requested->second)
                spec.routes.push_back(fontResolverRutaEspejo(r));
            spec.repertoire = repertorioBootstrap();
            TextRenderer* old = nullptr;
            for (const auto& kv : m_bindings)
            {
                if (identidadDeBinding(kv.first) != identity) continue;
                AtlasCacheEntry* entry = buscarEntradaPorId(kv.second);
                old = entry ? entry->atlas.get() : nullptr;
                if (old) break;
            }
            if (old && old->hasProfilePlan())
            {
                spec.fontSize = old->profilePlan().spec().fontSize;
                spec.kerning = old->profilePlan().spec().kerning;
                spec.measure = old->profilePlan().spec().measure;
            }
            else if (old)
            {
                spec.fontSize = old->fontSize();
                spec.kerning = old->kerning();
                spec.measure = old->measure();
            }
            else return nullptr;
            const auto plan = m_sourceStore->acquirePlan(spec);
            const std::string fp = m_sourceStore->fingerprintReached(plan);
            if (TextRenderer* hit = vincularHuella(identity, fp, &plan))
                return hit;
            auto tr = std::make_shared<TextRenderer>(m_sourceStore);
            if (!tr->initProfile(spec)) return nullptr;
            TextRenderer* raw = tr.get();
            guardarAtlas(identity, std::move(tr), fp);
            return raw;
        }

        void guardarAtlas(const std::string& identity,
                          std::shared_ptr<TextRenderer> tr,
                          const std::string& fingerprint)
        {
            const std::uint64_t gen = generacionDeHorneado();
            if (!tr) return;
            auto& bucket = m_atlasCache[fingerprint];
            const TextProfilePlanHandle* plan =
                tr->hasProfilePlan() ? &tr->profilePlan() : nullptr;
            for (auto& entry : bucket)
            {
                if (entry.atlas.get() == tr.get() ||
                    entradaCoincide(entry, plan))
                {
                    atar(gen, identity, fingerprint, entry.id);
                    return;
                }
            }
            const std::uint64_t id = m_nextAtlasEntryId++;
            bucket.push_back({id, std::move(tr)});
            atar(gen, identity, fingerprint, id);
        }

        void soltarAtlasSinBinding()
        {
            std::unordered_set<std::uint64_t> used;
            used.reserve(m_bindings.size());
            for (const auto& kv : m_bindings)
                used.insert(kv.second.entryId);
            for (auto it = m_atlasCache.begin(); it != m_atlasCache.end(); )
            {
                auto& bucket = it->second;
                for (auto eit = bucket.begin(); eit != bucket.end(); )
                {
                    if (used.count(eit->id) == 0)
                        eit = bucket.erase(eit);
                    else
                        ++eit;
                }
                if (bucket.empty())
                    it = m_atlasCache.erase(it);
                else
                    ++it;
            }
        }
    };
}
