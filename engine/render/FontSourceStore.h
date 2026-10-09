#pragma once

#include "../core/TextProcedencia.h"
#include "../core/FontDeclaration.h"
#include "FontMeasure.h"
#include "../../third_party/stb_truetype.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace eclips
{
    class LangStore;

    // ── El repertorio de arranque, y es UNO SOLO (CP-R3) ─────────────────
    // Vivia tipeado dentro de `TextRenderer::init`; desde que el catalogo de
    // `CP-R3` hornea perfiles por `initProfile`, los DOS caminos tienen que
    // pedir exactamente los mismos codepoints o una declaracion simple del
    // manifest dejaria de ser byte-equivalente a la misma linea escrita en un
    // `.ecl`. Ese equivalente es un guardian del harness, no una esperanza.
    // Ampliar esto es `CP-R5`; aca solo se comparte lo que ya existia.
    inline const std::vector<i32>& repertorioBootstrap()
    {
        static const std::vector<i32> kRepertorio = []
        {
            std::vector<i32> cps;
            for (int i = 32; i <= 126; ++i) cps.push_back(i);
            for (int i = 160; i <= 383; ++i) cps.push_back(i); // Latin Extended-A
            cps.push_back(0x2014); // em dash
            cps.push_back(0x2018); // left single quote
            cps.push_back(0x2019); // right single quote
            cps.push_back(0x201C); // left double quote
            cps.push_back(0x201D); // right double quote
            cps.push_back(0x2026); // ellipsis
            return cps;
        }();
        return kRepertorio;
    }

    struct FontSourceMetrics
    {
        std::size_t residentTtfBytes = 0;
        std::size_t payloadCount = 0;
        std::size_t secondAliasResidentDelta = 0;
        std::size_t atlasCount = 0;
        std::size_t readCount = 0;
        std::size_t hashCount = 0;
        std::size_t exactCompareCount = 0;
    };

    // Generación de las 19 init directas PRESERVACIÓN: estable y no desalojable.
    // Distinta de la bootstrap publicable (1). CP-R4.
    constexpr std::uint64_t kFontLegacyGeneration = 0;

    namespace detail
    {
        struct FontPayload
        {
            std::uint64_t generation = 0;
            std::uint64_t hash = 0;
            std::vector<unsigned char> bytes;
        };
    }

    class FontSourceHandle
    {
    public:
        FontSourceHandle() = default;
        explicit operator bool() const { return static_cast<bool>(m_payload); }
        const void* payloadIdentity() const { return m_payload.get(); }
        const unsigned char* data() const
        {
            return m_payload && !m_payload->bytes.empty()
                ? m_payload->bytes.data() : nullptr;
        }
        std::size_t size() const
        {
            return m_payload ? m_payload->bytes.size() : 0;
        }

    private:
        explicit FontSourceHandle(std::shared_ptr<const detail::FontPayload> payload)
            : m_payload(std::move(payload)) {}

        std::shared_ptr<const detail::FontPayload> m_payload;
        friend class FontSourceStore;
    };

    struct TextProfileSpec
    {
        std::uint64_t generation = 1;
        std::vector<std::string> routes;
        std::vector<i32> repertoire;
        f32 fontSize = 34.0f;
        f32 kerning = 0.0f;
        FontMeasure measure = FontMeasure::Height;
    };

    namespace detail
    {
        inline std::uint32_t floatBits(f32 value)
        {
            std::uint32_t out = 0;
            static_assert(sizeof(out) == sizeof(value), "f32 inesperado");
            std::memcpy(&out, &value, sizeof(out));
            return out;
        }

        struct TextProfileKey
        {
            std::uint64_t generation = 0;
            std::vector<std::string> routes;
            std::vector<i32> repertoire;
            std::uint32_t sizeBits = 0;
            std::uint32_t kerningBits = 0;
            FontMeasure measure = FontMeasure::Height;

            bool operator<(const TextProfileKey& rhs) const
            {
                return std::tie(generation, routes, repertoire, sizeBits,
                                kerningBits, measure)
                    < std::tie(rhs.generation, rhs.routes, rhs.repertoire,
                               rhs.sizeBits, rhs.kerningBits, rhs.measure);
            }
        };

        struct PlanSource
        {
            enum class State { Unknown, Broken, Invalid, Ready };
            State state = State::Unknown;
            FontSourceHandle handle;
            stbtt_fontinfo font{};
            f32 scale = 0.0f;
            f32 ascent = 0.0f;
            f32 lineHeight = 0.0f;
            bool contributes = false;
        };

        struct FontPlanData
        {
            TextProfileSpec spec;
            text::TextProcedencia procedencia;
            std::vector<PlanSource> sources;
            std::map<i32, int> sourceByCodepoint;
            std::size_t missing = 0;
            std::size_t incidences = 0;
            f32 ascent = 0.0f;
            f32 lineHeight = 0.0f;
        };
    }

    class TextProfilePlanHandle
    {
    public:
        TextProfilePlanHandle() = default;
        explicit operator bool() const { return static_cast<bool>(m_plan); }
        const void* identity() const
        {
            return m_identityOverride ? m_identityOverride : m_plan.get();
        }
        const void* procedenciaIdentity() const
        {
            return m_plan ? static_cast<const void*>(&m_plan->procedencia) : nullptr;
        }
        int sourceIndex(i32 codepoint) const
        {
            if (!m_plan) return -1;
            const auto it = m_plan->sourceByCodepoint.find(codepoint);
            return it == m_plan->sourceByCodepoint.end() ? -1 : it->second;
        }
        bool containsCodepoint(i32 codepoint) const
        {
            return m_plan &&
                m_plan->sourceByCodepoint.find(codepoint) !=
                    m_plan->sourceByCodepoint.end();
        }
        std::size_t missingCount() const { return m_plan ? m_plan->missing : 0; }
        std::size_t incidenceCount() const
        {
            return m_plan ? m_plan->incidences : 0;
        }
        std::size_t routeCount() const
        {
            return m_plan ? m_plan->spec.routes.size() : 0;
        }
        std::size_t acquiredSourceCount() const
        {
            if (!m_plan) return 0;
            return static_cast<std::size_t>(std::count_if(
                m_plan->sources.begin(), m_plan->sources.end(),
                [](const detail::PlanSource& s) {
                    return s.state != detail::PlanSource::State::Unknown;
                }));
        }
        std::vector<f32> contributingLineHeights() const
        {
            std::vector<f32> out;
            if (!m_plan) return out;
            for (const auto& source : m_plan->sources)
                if (source.contributes) out.push_back(source.lineHeight);
            return out;
        }
        std::vector<f32> contributingAscents() const
        {
            std::vector<f32> out;
            if (!m_plan) return out;
            for (const auto& source : m_plan->sources)
                if (source.contributes) out.push_back(source.ascent);
            return out;
        }
        f32 ascent() const { return m_plan ? m_plan->ascent : 0.0f; }
        f32 lineHeight() const { return m_plan ? m_plan->lineHeight : 0.0f; }
        const TextProfileSpec& spec() const
        {
            static const TextProfileSpec empty;
            return m_plan ? m_plan->spec : empty;
        }

        const stbtt_fontinfo* fontInfoFor(i32 codepoint) const
        {
            const int index = sourceIndex(codepoint);
            if (!m_plan || index < 0 ||
                static_cast<std::size_t>(index) >= m_plan->sources.size())
                return nullptr;
            return &m_plan->sources[static_cast<std::size_t>(index)].font;
        }
        f32 scaleFor(i32 codepoint) const
        {
            const int index = sourceIndex(codepoint);
            if (!m_plan || index < 0 ||
                static_cast<std::size_t>(index) >= m_plan->sources.size())
                return 0.0f;
            return m_plan->sources[static_cast<std::size_t>(index)].scale;
        }

        struct ReachedSlice
        {
            const unsigned char* data = nullptr;
            std::size_t size = 0;
            const void* payload = nullptr;
        };

        std::vector<ReachedSlice> contributingSlices() const
        {
            std::vector<ReachedSlice> out;
            if (!m_plan) return out;
            for (const auto& source : m_plan->sources)
            {
                if (!source.contributes || !source.handle) continue;
                out.push_back({source.handle.data(), source.handle.size(),
                               source.handle.payloadIdentity()});
            }
            return out;
        }

        std::vector<std::string> reachedPhysicalRoutes() const
        {
            std::vector<std::string> out;
            if (!m_plan) return out;
            for (std::size_t i = 0; i < m_plan->sources.size(); ++i)
            {
                if (!m_plan->sources[i].contributes) continue;
                if (i < m_plan->spec.routes.size())
                    out.push_back(m_plan->spec.routes[i]);
            }
            return out;
        }

        const std::map<i32, int>& coverageMap() const
        {
            static const std::map<i32, int> empty;
            return m_plan ? m_plan->sourceByCodepoint : empty;
        }

        static bool sameReachedBytes(const TextProfilePlanHandle& a,
                                     const TextProfilePlanHandle& b)
        {
            const auto left = a.contributingSlices();
            const auto right = b.contributingSlices();
            if (left.size() != right.size()) return false;
            for (std::size_t i = 0; i < left.size(); ++i)
            {
                if (left[i].payload && left[i].payload == right[i].payload)
                    continue;
                if (left[i].size != right[i].size) return false;
                if (left[i].size == 0) continue;
                if (!left[i].data || !right[i].data) return false;
                if (std::memcmp(left[i].data, right[i].data, left[i].size) != 0)
                    return false;
            }
            return true;
        }

        // Comparación exacta de la huella efectiva normalizada: bytes
        // alcanzados, repertorio, procedencia, rutas físicas, tamaño,
        // medida y espaciado. Un hasher constante no puede fusionar
        // 34 px con 60 px ni dos TTF distintos.
        static bool sameEffectiveReached(const TextProfilePlanHandle& a,
                                         const TextProfilePlanHandle& b)
        {
            if (!sameReachedBytes(a, b)) return false;
            if (!a || !b) return !a && !b;
            const TextProfileSpec& sa = a.spec();
            const TextProfileSpec& sb = b.spec();
            if (detail::floatBits(sa.fontSize) != detail::floatBits(sb.fontSize))
                return false;
            if (detail::floatBits(sa.kerning) != detail::floatBits(sb.kerning))
                return false;
            if (sa.measure != sb.measure) return false;
            if (sa.repertoire != sb.repertoire) return false;
            if (a.reachedPhysicalRoutes() != b.reachedPhysicalRoutes())
                return false;
            if (a.coverageMap() != b.coverageMap()) return false;
            return true;
        }

    private:
        explicit TextProfilePlanHandle(std::shared_ptr<detail::FontPlanData> plan)
            : m_plan(std::move(plan)) {}

        std::shared_ptr<detail::FontPlanData> m_plan;
        const void* m_identityOverride = nullptr;
        friend class FontSourceStore;
    };

    class FontSourceStore
    {
    public:
        using HashFunction =
            std::function<std::uint64_t(const unsigned char*, std::size_t)>;

        explicit FontSourceStore(HashFunction hasher = {})
            : m_hasher(hasher ? std::move(hasher) : HashFunction(defaultHash))
        {}

        FontSourceHandle acquire(std::uint64_t generation,
                                 const std::string& exactResolvedPath)
        {
            const RouteKey routeKey{generation, exactResolvedPath};
            const auto memo = m_routeMemo.find(routeKey);
            if (memo != m_routeMemo.end()) return memo->second;

            GenerationMetrics& counters = m_metrics[generation];
            ++counters.publicMetrics.readCount;
            std::ifstream file(exactResolvedPath, std::ios::binary | std::ios::ate);
            if (!file)
            {
                m_routeMemo.emplace(routeKey, FontSourceHandle{});
                return {};
            }
            const std::streamsize length = file.tellg();
            if (length <= 0)
            {
                m_routeMemo.emplace(routeKey, FontSourceHandle{});
                return {};
            }
            file.seekg(0);
            std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
            if (!file.read(reinterpret_cast<char*>(bytes.data()), length))
            {
                m_routeMemo.emplace(routeKey, FontSourceHandle{});
                return {};
            }

            ++counters.publicMetrics.hashCount;
            const std::uint64_t hash = m_hasher(bytes.data(), bytes.size());
            const PayloadKey payloadKey{generation, bytes.size(), hash};
            auto& candidates = m_payloads[payloadKey];
            for (const auto& candidate : candidates)
            {
                ++counters.publicMetrics.exactCompareCount;
                if (candidate->bytes == bytes)
                {
                    FontSourceHandle result(candidate);
                    m_routeMemo.emplace(routeKey, result);
                    counters.publicMetrics.secondAliasResidentDelta = 0;
                    return result;
                }
            }

            auto payload = std::make_shared<detail::FontPayload>();
            payload->generation = generation;
            payload->hash = hash;
            payload->bytes = std::move(bytes);
            counters.publicMetrics.residentTtfBytes += payload->bytes.size();
            ++counters.publicMetrics.payloadCount;
            candidates.push_back(payload);
            FontSourceHandle result(payload);
            m_routeMemo.emplace(routeKey, result);
            return result;
        }

        FontSourceMetrics metrics(std::uint64_t generation) const
        {
            const auto it = m_metrics.find(generation);
            return it == m_metrics.end()
                ? FontSourceMetrics{} : it->second.publicMetrics;
        }

        std::uint64_t activeGeneration() const { return m_activeGeneration; }
        std::uint64_t pendingGeneration() const { return m_pendingGeneration; }
        // Quien la abrio. 0 = nadie la reclamo. Lo consulta el publicador que
        // se va, para publicar SU paquete y no el de otro (politica
        // `SALIR-DEL-MENU-DESCARTA-O-APLICA`, firmada 2026-09-04).
        std::uint64_t pendingOwner() const { return m_pendingOwner; }

        void setCandidatePresentation(
            const std::vector<fontdecl::Declaracion>* fuentes,
            const std::string* temaDefault)
        {
            m_candidateFuentes = fuentes;
            m_candidateTemaDefault = temaDefault;
        }
        const std::vector<fontdecl::Declaracion>* candidateFuentes() const
        {
            return m_candidateFuentes;
        }
        const std::string* candidateTemaDefault() const
        {
            return m_candidateTemaDefault;
        }
        void setCandidateLanguage(const LangStore* store)
        {
            m_candidateLanguage = store;
        }
        const LangStore* candidateLanguage() const
        {
            return m_candidateLanguage;
        }

        std::uint64_t subscribeGeneration(
            std::function<bool(std::uint64_t)> onPrepare,
            std::function<void(std::uint64_t, std::uint64_t)> onCommit,
            std::function<void(std::uint64_t)> onRollback = {})
        {
            GenerationListener listener;
            listener.token = m_nextListenerToken++;
            listener.onPrepare = std::move(onPrepare);
            listener.onCommit = std::move(onCommit);
            listener.onRollback = std::move(onRollback);
            const std::uint64_t token = listener.token;
            m_listeners.push_back(std::move(listener));
            return token;
        }

        // ── EL DUENO QUE SE VA ABORTA SU TRANSACCION (`R1`) ──────────────────
        //
        // La pendiente es una transaccion, y hasta acá no tenía dueño: nadie
        // anotaba quién la había abierto. El publicador que la abre PUEDE MORIR
        // antes del corte, y no es un borde -- es un menú en rol Menu con `ESC`:
        // `cerrarPantalla()` encola el `cmdPop` desde `handleEvent` y el
        // `processQueue()` de `StateStack::update` lo destruye en el MISMO
        // frame, antes del `commitPendingPresentation()` del Engine.
        // Reproducido 9 de 10 corridas por `correr_probe_r1_pausa.ps1`
        // (2026-09-03) con el motor despierto.
        //
        // Dejaba dos cosas rotas: el `const LangStore*` candidato colgando -lo
        // posee el state muerto, y solo commit/rollback lo limpiaban- y el
        // paquete huérfano commiteado VACÍO, o sea el cambio de idioma perdido
        // EN SILENCIO, sin una línea en consola.
        //
        // El que se va se retira PRIMERO y recién después se aborta: el
        // rollback no debe re-entrar en un state que está en pleno `onExit`.
        // Los demás suscriptores sí reciben su `onRollback`, que es lo correcto
        // para una transacción abortada.
        //
        // Y sólo el DUEÑO la cierra: un suscriptor cualquiera que se va no le
        // puede destruir el paquete a un publicador vivo. Guardián: `16.4` del
        // harness de `CP-R4`, que nació verde para custodiar justo eso.
        void unsubscribeGeneration(std::uint64_t token)
        {
            if (token == 0) return;
            for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
            {
                if (it->token != token) continue;
                m_listeners.erase(it);
                if (m_pendingGeneration != 0 && m_pendingOwner == token)
                    rollbackGeneration();
                return;
            }
        }

        // `owner` = el token de suscripcion del publicador que la abre. Se
        // guarda SOLO al abrirla: un segundo publicador amplia el paquete y no
        // le roba la propiedad, porque el que puede quedarse sin publicar -y
        // dejar el paquete huerfano- es el que la abrio.
        //
        // DEVUELVE: la generacion en la que quedaste, o **0 si no te pudiste
        // sumar** porque la pendiente ya estaba APROBADA (`R2`). El 0 no es un
        // error: es la senal que el publicador necesita para DIFERIR su pedido
        // al frame siguiente en vez de perder lo suyo.
        //
        // ── LA APROBACION ES DEL QUE LA GANO (`R2`) ──────────────────────────
        //
        // Hasta aca esta funcion reseteaba `m_pendingReady` SIN mirar si la
        // pendiente ya habia pasado su finalize. Eso dejaba que un publicador
        // tardio -un `cargar_tema` de la primera pantalla alcanza- des-aprobara
        // el paquete de otro, y si SU horneado fallaba,
        // `Engine::commitPendingPresentation` hacia rollback de la transaccion
        // ENTERA: con adentro un paquete que no habia fallado en nada.
        //
        // Medido antes de arreglarlo, y las dos mitades importan:
        //   · el mecanismo es EXACTAMENTE el que la revision de `CP-R4`
        //     describio -- con `R2` la descripcion se sostiene literal, al
        //     reves que con `R1`.
        //   · pero el gatillo estaba acotado de mas: la revision lo ataba a
        //     `loadFromSlot`, y `correr_probe_r2_tema.ps1` lo reproduce **sin
        //     save y sin carrera**, sobre el paquete del ARRANQUE.
        //
        // Por que no alcanzaba con "dejar pasar el fallo": el re-prepare es
        // DESTRUCTIVO DE ENTRADA. `onFontGenerationPrepare` reconstruye
        // `m_candidateCatalog` en su primera linea y al fallar apaga sus flags,
        // asi que para cuando devuelve `false` ya no queda candidato que
        // conservar. La unica correccion barata es que el re-prepare NO OCURRA.
        //
        // Y la ley se corta fina a proposito: lo que no se puede pisar es una
        // pendiente APROBADA. Una abierta y todavia sin finalize se sigue
        // ampliando como siempre -- es la conducta que `CP-R4` asume, y su
        // guardian es el `18.4`, que nace VERDE justo para eso.
        std::uint64_t prepareGeneration(std::uint64_t owner = 0)
        {
            if (m_pendingGeneration != 0)
            {
                // Ya aprobada: no sos parte de este corte. Difierase.
                if (m_pendingReady) return 0;

                // Todavia NO aprobada: un segundo publicador puede ampliar el
                // mismo paquete antes del corte. Obliga a repetir el prepare
                // de los suscriptores.
                m_pendingReady = false;
                return m_pendingGeneration;
            }
            m_pendingGeneration = m_nextGeneration++;
            m_pendingOwner = owner;
            m_pendingReady = false;
            return m_pendingGeneration;
        }

        // Hornea el candidato en la pendiente ANTES de volverla activa.
        // Si un suscriptor falla, la activa no cambia.
        bool finalizePending()
        {
            if (m_pendingGeneration == 0) return true;
            if (m_pendingReady) return true;
            if (m_finalizing) return m_pendingReady;
            m_finalizing = true;
            bool ok = true;
            const auto listeners = m_listeners;
            for (const auto& listener : listeners)
            {
                if (!listener.onPrepare) continue;
                if (!listener.onPrepare(m_pendingGeneration))
                {
                    ok = false;
                    break;
                }
            }
            m_finalizing = false;
            m_pendingReady = ok;
            return ok;
        }

        bool commitGeneration()
        {
            if (m_pendingGeneration == 0) return false;
            if (!m_pendingReady && !finalizePending())
            {
                rollbackGeneration();
                return false;
            }
            const std::uint64_t previous = m_activeGeneration;
            m_activeGeneration = m_pendingGeneration;
            m_pendingGeneration = 0;
            m_pendingOwner = 0;
            m_pendingReady = false;
            const auto listeners = m_listeners;
            for (const auto& listener : listeners)
            {
                if (listener.onCommit)
                    listener.onCommit(m_activeGeneration, previous);
            }
            m_candidateLanguage = nullptr;
            retireOrphans();
            return true;
        }

        bool commitGenerationIfPending()
        {
            if (m_pendingGeneration == 0) return false;
            return commitGeneration();
        }

        bool rollbackGeneration()
        {
            if (m_pendingGeneration == 0) return false;
            const std::uint64_t dropped = m_pendingGeneration;
            m_pendingGeneration = 0;
            m_pendingOwner = 0;
            m_pendingReady = false;
            const auto listeners = m_listeners;
            for (const auto& listener : listeners)
            {
                if (listener.onRollback) listener.onRollback(dropped);
            }
            m_candidateLanguage = nullptr;
            retireGeneration(dropped);
            return true;
        }

        void notifyConsumerDropped()
        {
            retireOrphans();
        }

        void retireOrphans()
        {
            if (m_retiringOrphans) return;
            m_retiringOrphans = true;
            gcOrphans();
            std::set<std::uint64_t> gens;
            for (const auto& kv : m_metrics) gens.insert(kv.first);
            for (const auto& kv : m_plans) gens.insert(kv.first.generation);
            for (const auto& kv : m_payloads) gens.insert(kv.first.generation);
            for (const auto g : gens)
            {
                if (g == kFontLegacyGeneration) continue;
                if (g == m_activeGeneration || g == m_pendingGeneration) continue;
                retireGeneration(g);
            }
            m_retiringOrphans = false;
        }

        std::size_t processResidentTtfBytes() const
        {
            std::size_t total = 0;
            for (const auto& kv : m_metrics)
                total += kv.second.publicMetrics.residentTtfBytes;
            return total;
        }

        std::size_t planUseCount(std::uint64_t generation) const
        {
            std::size_t maxCount = 0;
            for (const auto& kv : m_plans)
            {
                if (kv.first.generation != generation) continue;
                const auto count = static_cast<std::size_t>(kv.second.use_count());
                if (count > maxCount) maxCount = count;
            }
            return maxCount;
        }

        bool retireGeneration(std::uint64_t generation)
        {
            if (generation == kFontLegacyGeneration) return false;
            for (const auto& kv : m_plans)
            {
                if (kv.first.generation != generation) continue;
                if (kv.second.use_count() > 1) return false;
            }

            for (auto it = m_plans.begin(); it != m_plans.end(); )
            {
                if (it->first.generation == generation)
                    it = m_plans.erase(it);
                else
                    ++it;
            }
            for (auto it = m_routeMemo.begin(); it != m_routeMemo.end(); )
            {
                if (it->first.generation == generation)
                    it = m_routeMemo.erase(it);
                else
                    ++it;
            }
            GenerationMetrics& counters = m_metrics[generation];
            for (auto it = m_payloads.begin(); it != m_payloads.end(); )
            {
                if (it->first.generation != generation)
                {
                    ++it;
                    continue;
                }
                for (const auto& payload : it->second)
                {
                    if (counters.publicMetrics.residentTtfBytes >= payload->bytes.size())
                        counters.publicMetrics.residentTtfBytes -= payload->bytes.size();
                    else
                        counters.publicMetrics.residentTtfBytes = 0;
                    if (counters.publicMetrics.payloadCount > 0)
                        --counters.publicMetrics.payloadCount;
                }
                it = m_payloads.erase(it);
            }
            return true;
        }

        void forceNextPlanIdentity(const void* identity)
        {
            m_forcedIdentity = identity;
        }

        // Suelta planes/memo/payloads que ya no retiene ningún atlas.
        // No toca la generación legacy. Lo llama FontManager al retirar un
        // binding local cuyo último consumidor murió.
        void gcOrphans()
        {
            for (auto it = m_plans.begin(); it != m_plans.end(); )
            {
                if (it->first.generation == kFontLegacyGeneration)
                {
                    ++it;
                    continue;
                }
                if (it->second.use_count() <= 1)
                    it = m_plans.erase(it);
                else
                    ++it;
            }
            for (auto it = m_routeMemo.begin(); it != m_routeMemo.end(); )
            {
                if (it->first.generation == kFontLegacyGeneration)
                {
                    ++it;
                    continue;
                }
                bool needed = false;
                for (const auto& plan : m_plans)
                {
                    if (plan.first.generation != it->first.generation)
                        continue;
                    for (const auto& route : plan.first.routes)
                    {
                        if (route == it->first.path)
                        {
                            needed = true;
                            break;
                        }
                    }
                    if (needed) break;
                }
                if (!needed)
                    it = m_routeMemo.erase(it);
                else
                    ++it;
            }
            for (auto it = m_payloads.begin(); it != m_payloads.end(); )
            {
                if (it->first.generation == kFontLegacyGeneration)
                {
                    ++it;
                    continue;
                }
                GenerationMetrics& counters = m_metrics[it->first.generation];
                auto& vec = it->second;
                for (auto pit = vec.begin(); pit != vec.end(); )
                {
                    if ((*pit).use_count() > 1)
                    {
                        ++pit;
                        continue;
                    }
                    if (counters.publicMetrics.residentTtfBytes >= (*pit)->bytes.size())
                        counters.publicMetrics.residentTtfBytes -= (*pit)->bytes.size();
                    else
                        counters.publicMetrics.residentTtfBytes = 0;
                    if (counters.publicMetrics.payloadCount > 0)
                        --counters.publicMetrics.payloadCount;
                    pit = vec.erase(pit);
                }
                if (vec.empty())
                    it = m_payloads.erase(it);
                else
                    ++it;
            }
        }

        std::string fingerprintReached(const TextProfilePlanHandle& plan) const
        {
            std::vector<unsigned char> material;
            auto append = [&](const void* p, std::size_t n) {
                const auto* b = static_cast<const unsigned char*>(p);
                material.insert(material.end(), b, b + n);
            };
            if (plan)
            {
                for (const auto& slice : plan.contributingSlices())
                {
                    const std::uint64_t contentId =
                        m_hasher(slice.data, slice.size);
                    const std::uint64_t size =
                        static_cast<std::uint64_t>(slice.size);
                    append(&contentId, sizeof(contentId));
                    append(&size, sizeof(size));
                }
                for (const auto& route : plan.reachedPhysicalRoutes())
                    append(route.data(), route.size());
                for (const auto& kv : plan.coverageMap())
                {
                    append(&kv.first, sizeof(kv.first));
                    append(&kv.second, sizeof(kv.second));
                }
                const TextProfileSpec& spec = plan.spec();
                for (const i32 cp : spec.repertoire)
                    append(&cp, sizeof(cp));
                const std::uint32_t sizeBits = detail::floatBits(spec.fontSize);
                const std::uint32_t kerningBits = detail::floatBits(spec.kerning);
                const int measure = static_cast<int>(spec.measure);
                append(&sizeBits, sizeof(sizeBits));
                append(&kerningBits, sizeof(kerningBits));
                append(&measure, sizeof(measure));
            }
            static const unsigned char kEmpty = 0;
            const std::uint64_t digest = m_hasher(
                material.empty() ? &kEmpty : material.data(), material.size());
            std::ostringstream os;
            os << std::hex << digest;
            return os.str();
        }

        std::string effectiveFingerprint(const TextProfileSpec& requested)
        {
            return fingerprintReached(acquirePlan(requested));
        }

        TextProfilePlanHandle acquirePlan(const TextProfileSpec& requested)
        {
            TextProfileSpec spec = requested;
            std::sort(spec.repertoire.begin(), spec.repertoire.end());
            spec.repertoire.erase(
                std::unique(spec.repertoire.begin(), spec.repertoire.end()),
                spec.repertoire.end());

            detail::TextProfileKey key;
            key.generation = spec.generation;
            key.routes = spec.routes;
            key.repertoire = spec.repertoire;
            key.sizeBits = detail::floatBits(spec.fontSize);
            key.kerningBits = detail::floatBits(spec.kerning);
            key.measure = spec.measure;
            const auto memo = m_plans.find(key);
            if (memo != m_plans.end())
            {
                TextProfilePlanHandle handle(memo->second);
                handle.m_identityOverride = m_forcedIdentity;
                m_forcedIdentity = nullptr;
                return handle;
            }

            auto plan = std::make_shared<detail::FontPlanData>();
            plan->spec = spec;
            plan->sources.resize(spec.routes.size());

            for (const i32 codepoint : spec.repertoire)
            {
                std::vector<text::TextCandidatoEvaluado> evaluated;
                evaluated.reserve(spec.routes.size());
                for (std::size_t i = 0; i < spec.routes.size(); ++i)
                {
                    detail::PlanSource& source = plan->sources[i];
                    if (source.state == detail::PlanSource::State::Unknown)
                        prepareSource(*plan, i);

                    text::TextEstadoCandidato state =
                        text::TextEstadoCandidato::NoCubre;
                    switch (source.state)
                    {
                    case detail::PlanSource::State::Broken:
                        state = text::TextEstadoCandidato::RutaRota;
                        break;
                    case detail::PlanSource::State::Invalid:
                        state = text::TextEstadoCandidato::FuenteInvalida;
                        break;
                    case detail::PlanSource::State::Ready:
                        state = stbtt_FindGlyphIndex(&source.font, codepoint) != 0
                            ? text::TextEstadoCandidato::Cubre
                            : text::TextEstadoCandidato::NoCubre;
                        break;
                    case detail::PlanSource::State::Unknown:
                        break;
                    }
                    std::string physicalIdentity = spec.routes[i];
                    if (source.state == detail::PlanSource::State::Ready)
                    {
                        std::ostringstream token;
                        token << "payload@" << source.handle.payloadIdentity();
                        physicalIdentity = token.str();
                    }
                    evaluated.push_back({physicalIdentity, state});
                    if (state == text::TextEstadoCandidato::Cubre) break;
                }

                const text::TextProcedenciaResultado& result =
                    plan->procedencia.resolver(codepoint, evaluated);
                plan->incidences += result.incidencias.size();
                if (result.cobertura == text::TextCobertura::Cubierto)
                {
                    const int index = static_cast<int>(result.indice);
                    plan->sourceByCodepoint[codepoint] = index;
                    if (index >= 0 &&
                        static_cast<std::size_t>(index) < plan->sources.size())
                        plan->sources[static_cast<std::size_t>(index)].contributes = true;
                }
                else
                {
                    plan->sourceByCodepoint[codepoint] = -1;
                    ++plan->missing;
                }
            }

            for (const auto& source : plan->sources)
            {
                if (!source.contributes) continue;
                plan->ascent = std::max(plan->ascent, source.ascent);
                plan->lineHeight = std::max(plan->lineHeight, source.lineHeight);
            }
            emitDiagnosticsOnce(*plan);
            m_plans.emplace(std::move(key), plan);
            TextProfilePlanHandle handle(std::move(plan));
            handle.m_identityOverride = m_forcedIdentity;
            m_forcedIdentity = nullptr;
            return handle;
        }

    private:
        struct RouteKey
        {
            std::uint64_t generation = 0;
            std::string path;
            bool operator<(const RouteKey& rhs) const
            {
                return std::tie(generation, path) <
                       std::tie(rhs.generation, rhs.path);
            }
        };
        struct PayloadKey
        {
            std::uint64_t generation = 0;
            std::size_t size = 0;
            std::uint64_t hash = 0;
            bool operator<(const PayloadKey& rhs) const
            {
                return std::tie(generation, size, hash) <
                       std::tie(rhs.generation, rhs.size, rhs.hash);
            }
        };
        struct GenerationMetrics
        {
            FontSourceMetrics publicMetrics;
        };

        static std::uint64_t defaultHash(const unsigned char* data,
                                         std::size_t size)
        {
            std::uint64_t hash = UINT64_C(14695981039346656037);
            for (std::size_t i = 0; i < size; ++i)
            {
                hash ^= data[i];
                hash *= UINT64_C(1099511628211);
            }
            return hash;
        }

        void prepareSource(detail::FontPlanData& plan, std::size_t index)
        {
            detail::PlanSource& source = plan.sources[index];
            source.handle = acquire(plan.spec.generation, plan.spec.routes[index]);
            if (!source.handle)
            {
                source.state = detail::PlanSource::State::Broken;
                return;
            }
            const int offset =
                stbtt_GetFontOffsetForIndex(source.handle.data(), 0);
            if (offset < 0 ||
                !stbtt_InitFont(&source.font, source.handle.data(), offset))
            {
                source.state = detail::PlanSource::State::Invalid;
                return;
            }
            source.state = detail::PlanSource::State::Ready;
            source.scale = plan.spec.measure == FontMeasure::Em
                ? stbtt_ScaleForMappingEmToPixels(
                    &source.font, plan.spec.fontSize)
                : stbtt_ScaleForPixelHeight(
                    &source.font, plan.spec.fontSize);
            int ascent = 0, descent = 0, gap = 0;
            stbtt_GetFontVMetrics(&source.font, &ascent, &descent, &gap);
            source.ascent = ascent * source.scale;
            source.lineHeight = (ascent - descent + gap) * source.scale;
        }

        static void emitDiagnosticsOnce(const detail::FontPlanData& plan)
        {
            std::set<std::pair<text::TextTipoIncidencia, std::string>> emitted;
            for (const i32 codepoint : plan.spec.repertoire)
            {
                const auto& result =
                    const_cast<text::TextProcedencia&>(plan.procedencia).resolver(
                        codepoint, {});
                for (const auto& incidence : result.incidencias)
                {
                    if (!emitted.insert(
                            {incidence.tipo, incidence.fuenteFisica}).second)
                        continue;
                    const std::string& diagnosticRoute =
                        incidence.indice < plan.spec.routes.size()
                            ? plan.spec.routes[incidence.indice]
                            : incidence.fuenteFisica;
                    std::cerr << "[FontSourceStore] Aviso: fuente '"
                              << diagnosticRoute
                              << "' inutilizable en perfil bootstrap.\n";
                }
                if (result.cobertura == text::TextCobertura::Omitido)
                {
                    std::ostringstream cp;
                    cp << "U+" << std::uppercase << std::hex
                       << std::setw(codepoint > 0xFFFF ? 6 : 4)
                       << std::setfill('0') << codepoint;
                    std::cerr << "[TextRenderer] Aviso: glifo ausente "
                              << cp.str() << " -- OMITIR + AVISAR.\n";
                }
            }
        }

        struct GenerationListener
        {
            std::uint64_t token = 0;
            std::function<bool(std::uint64_t)> onPrepare;
            std::function<void(std::uint64_t, std::uint64_t)> onCommit;
            std::function<void(std::uint64_t)> onRollback;
        };

        HashFunction m_hasher;
        std::uint64_t m_activeGeneration = 1;
        std::uint64_t m_pendingGeneration = 0;
        // Quien ABRIO la pendiente. 0 = nadie la reclamo (harness suelto, o un
        // publicador que no es suscriptor). Ver `unsubscribeGeneration`.
        std::uint64_t m_pendingOwner = 0;
        std::uint64_t m_nextGeneration = 2;
        std::uint64_t m_nextListenerToken = 1;
        bool m_pendingReady = false;
        bool m_finalizing = false;
        bool m_retiringOrphans = false;
        const void* m_forcedIdentity = nullptr;
        const std::vector<fontdecl::Declaracion>* m_candidateFuentes = nullptr;
        const std::string* m_candidateTemaDefault = nullptr;
        const LangStore* m_candidateLanguage = nullptr;
        std::vector<GenerationListener> m_listeners;
        std::map<RouteKey, FontSourceHandle> m_routeMemo;
        std::map<PayloadKey,
                 std::vector<std::shared_ptr<const detail::FontPayload>>> m_payloads;
        std::map<std::uint64_t, GenerationMetrics> m_metrics;
        std::map<detail::TextProfileKey,
                 std::shared_ptr<detail::FontPlanData>> m_plans;
    };
}
