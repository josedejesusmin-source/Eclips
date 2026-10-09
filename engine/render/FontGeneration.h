// ============================================================================
// ECLIPS ENGINE - FontGeneration (ATLAS-DE-FUENTE CP-R4)
// ============================================================================
// La puerta de generaciones activa/pendiente, huella efectiva y el seam de
// identidad. No es un dueno nuevo: el store sigue poseyendo bytes/planes y
// FontManager los atlas. Este header es el __has_include del harness y el
// contrato de las funciones libres que el rojo nombra.
// ============================================================================
#pragma once

#include "FontCatalog.h"
#include "FontManager.h"
#include "FontSourceStore.h"
#include "../core/ProjectManifest.h"
#include "../core/LangPaths.h"
#include "../../Theme.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#define ECLIPS_FONT_TRANSACTION_V2 1

// `R1`: la pendiente tiene DUENO. El publicador que la abre puede morir antes
// del corte (un menu en rol Menu con `ESC`), y hasta aca nadie anotaba quien la
// habia abierto, asi que `onExit` no tenia nada correcto que hacer. Lo define
// produccion; el harness lo usa para compilar de los dos lados del arreglo.
#define ECLIPS_FONT_PENDING_OWNER 1

namespace eclips
{
    enum class FontPublisherDoor
    {
        Arranque,
        Selector,
        ApplyTheme,
        Restore,
        LoadNewScript
    };

    inline std::uint64_t fontActiveGeneration(const FontSourceStore& store)
    {
        return store.activeGeneration();
    }

    inline std::uint64_t fontPendingGeneration(const FontSourceStore& store)
    {
        return store.pendingGeneration();
    }

    inline std::uint64_t fontPrepareGeneration(FontSourceStore& store,
                                               std::uint64_t owner = 0)
    {
        return store.prepareGeneration(owner);
    }

    inline bool fontFinalizePending(FontSourceStore& store)
    {
        return store.finalizePending();
    }

    inline bool fontCommitGeneration(FontSourceStore& store)
    {
        return store.commitGeneration();
    }

    inline bool fontRollbackGeneration(FontSourceStore& store)
    {
        return store.rollbackGeneration();
    }

    inline std::uint64_t fontSubscribeGeneration(
        FontSourceStore& store,
        std::function<bool(std::uint64_t)> onPrepare,
        std::function<void(std::uint64_t, std::uint64_t)> onCommit,
        std::function<void(std::uint64_t)> onRollback = {})
    {
        return store.subscribeGeneration(
            std::move(onPrepare), std::move(onCommit), std::move(onRollback));
    }

    inline void fontUnsubscribeGeneration(FontSourceStore& store, std::uint64_t token)
    {
        store.unsubscribeGeneration(token);
    }

    inline void fontNotifyConsumerDropped(FontSourceStore& store)
    {
        store.notifyConsumerDropped();
    }

    inline std::size_t fontProcessResidentTtfBytes(const FontSourceStore& store)
    {
        return store.processResidentTtfBytes();
    }

    inline bool fontRetireGeneration(FontSourceStore& store, std::uint64_t generation)
    {
        return store.retireGeneration(generation);
    }

    inline std::size_t fontPlanUseCount(const FontSourceStore& store,
                                        std::uint64_t generation)
    {
        return store.planUseCount(generation);
    }

    inline void fontForceNextPlanIdentity(FontSourceStore& store,
                                          const void* identity)
    {
        store.forceNextPlanIdentity(identity);
    }

    inline std::string fontEffectiveFingerprint(FontSourceStore& store,
                                                const TextProfileSpec& spec)
    {
        return store.effectiveFingerprint(spec);
    }

    inline std::string fontFingerprintReached(
        const FontSourceStore& store, const TextProfilePlanHandle& plan)
    {
        return store.fingerprintReached(plan);
    }

    inline const TextRenderer* fontAtlasByFingerprint(FontManager& fm,
                                                      const std::string& fingerprint)
    {
        return fm.atlasPorHuella(fingerprint);
    }

    inline std::string fontCacheKey(FontManager& fm, const TextRenderer& renderer)
    {
        return fm.cacheKeyOf(renderer);
    }

    inline void fontReadoptarGlobales(
        FontCatalog& catalog,
        const std::vector<fontdecl::Declaracion>& decls)
    {
        catalog.readoptarGlobales(decls);
    }

    inline bool fontManifestParseValid(const ProjectManifest& manifest)
    {
        return manifest.parseValid;
    }

    inline bool fontBakeThemeBases(
        FontSourceStore& store,
        TextRenderer& textOut,
        TextRenderer& nameOut,
        const std::string& fontPath,
        f32 kerning)
    {
        if (fontPath.empty()) return true;
        textOut.setSourceStore(&store);
        nameOut.setSourceStore(&store);
        auto bake = [&](TextRenderer& renderer, f32 size) -> bool {
            TextProfileSpec spec;
            spec.generation = store.pendingGeneration() != 0
                ? store.pendingGeneration()
                : store.activeGeneration();
            spec.routes = { fontPath };
            spec.repertoire = repertorioBootstrap();
            spec.fontSize = size;
            spec.kerning = kerning;
            spec.measure = FontMeasure::Height;
            if (!renderer.initProfile(spec)) return false;
            return renderer.hasProfilePlan() &&
                !renderer.profilePlan().contributingSlices().empty();
        };
        return bake(textOut, 34.0f) && bake(nameOut, 26.0f);
    }

    inline void fontSetCandidateLanguage(
        FontSourceStore& store, const LangStore* candidateLang)
    {
        store.setCandidateLanguage(candidateLang);
    }

    // Puerta unica de solicitud de los cuatro publicadores. Produccion y
    // harness llaman esta misma funcion; LoadNewScript queda deliberadamente
    // fuera porque materializa dentro de la activa.
    // `owner` es el token de suscripcion del que pide. Default 0 = nadie la
    // reclama, que es lo que hace un harness suelto; produccion pasa el suyo
    // para que `unsubscribeGeneration` sepa a quien pertenece la transaccion.
    // El nombre de la puerta, para la voz del rechazo. No entra en ninguna
    // decision: existe para que un aviso diga CUAL publicador llego tarde en
    // vez de obligar a deducirlo del contexto.
    inline const char* fontNombreDePuerta(FontPublisherDoor door)
    {
        switch (door)
        {
        case FontPublisherDoor::Arranque:      return "Arranque";
        case FontPublisherDoor::Selector:      return "Selector";
        case FontPublisherDoor::ApplyTheme:    return "ApplyTheme";
        case FontPublisherDoor::Restore:       return "Restore";
        case FontPublisherDoor::LoadNewScript: return "LoadNewScript";
        }
        return "?";
    }

    inline bool fontRequestPublisher(
        FontSourceStore& store, FontPublisherDoor door,
        std::uint64_t owner = 0)
    {
        if (door == FontPublisherDoor::LoadNewScript) return false;

        // `R2`: devuelve FALSE si NO te pudiste sumar, porque la pendiente ya
        // estaba APROBADA y una aprobacion es del que la gano. No es un error:
        // es la senal para DIFERIR el pedido al frame siguiente en vez de
        // perder lo tuyo. El que la ignore y siga adelante le deja su
        // candidato a un corte que no lo horneo.
        if (store.prepareGeneration(owner) != 0) return true;

        // ── Y EL RECHAZO HABLA ──────────────────────────────────────────────
        //
        // `R2` le enseño a `prepareGeneration` a RECHAZAR, y de las cuatro
        // puertas solo `ApplyTheme` aprendio que hacer con un `false`. Las
        // otras tres siguen de largo con su candidato ya armado, asi que un
        // corte ajeno se lo puede consumir SIN haberlo horneado: el idioma se
        // publicaria con los atlas del anterior, el restore perderia sus
        // locales, y una pantalla quedaria sin catalogo.
        //
        // Las tres son angostas -- piden que alguien haya aprobado ANTES en el
        // mismo frame -- pero eran MUDAS, o sea indiagnosticables. Y es un modo
        // de falla que este mismo arreglo INTRODUJO: hasta `R2` esta funcion no
        // rechazaba nunca.
        //
        // La voz no decide nada y no es politica: convierte tres caminos mudos
        // en uno que se lee. Y paga algo que al lote le faltaba -- es el UNICO
        // observable de consola de la postergacion, que hasta aca solo se podia
        // ver instrumentando produccion a mano y recompilando.
        //
        // Va SOLO en el rechazo: ampliar una pendiente todavia sin aprobar es
        // la conducta normal de `CP-R4` y sigue muda. Guardianes: `18.5` (que
        // hable y nombre la puerta) y `18.6` (que la legitima no grite).
        std::cerr << "[FontSourceStore] Aviso: la puerta '"
                  << fontNombreDePuerta(door)
                  << "' llego tarde a una generacion ya APROBADA; su pedido no "
                     "entra en este corte. Si esa puerta no difiere lo suyo, su "
                     "candidato puede publicarse sin haberse horneado."
                  << std::endl;
        return false;
    }

    // El candidato de idioma registrado en el store vive alrededor de TODO el
    // bucle de listeners. El override es el candidato estrecho de F8.
    inline bool fontFinalizePublisher(
        FontSourceStore& store, const LangStore* candidateOverride = nullptr)
    {
        const LangStore* candidate = candidateOverride
            ? candidateOverride : store.candidateLanguage();
        // ESPEJO-INUTILIZABLE 2a: el memo candidato de fuentes hereda la
        // obligacion del generico. Limpiar al entrar y al salir del horneado
        // para que Y no resuelva con las rutas de X.
        fontMemoEspejoCandidatoLimpiar();
        LangCandidatoGuard guard(candidate);
        const bool ok = store.finalizePending();
        fontMemoEspejoCandidatoLimpiar();
        return ok;
    }

    inline bool fontCommitPublisher(FontSourceStore& store)
    {
        return store.commitGenerationIfPending();
    }

    // Puerta transaccional de los cuatro publicadores. El paquete se hornea
    // en el candidato; el commit sólo publica objetos ya listos. Un fallo
    // descarta el paquete sin tocar catálogo, tema ni idioma vivos.
    inline bool fontPublish(FontSourceStore& store, FontPublisherDoor door)
    {
        if (!fontRequestPublisher(store, door)) return false;
        if (!fontFinalizePublisher(store))
        {
            store.rollbackGeneration();
            return false;
        }
        return fontCommitPublisher(store);
    }

    inline void fontReleaseLocalIdentity(FontManager& fm, const std::string& clave)
    {
        fm.releaseLocalIdentity(clave);
    }

    inline void fontSyncLiveLocalIdentities(
        FontManager& fm, const std::vector<std::string>& sceneKeys)
    {
        fm.syncLiveLocalIdentities(sceneKeys);
    }

    inline void fontDropReplacedGeneration(FontManager& fm, std::uint64_t previous)
    {
        fm.dropReplacedGeneration(previous);
    }

    inline void fontDropGeneration(FontManager& fm, std::uint64_t generation)
    {
        fm.dropGeneration(generation);
    }

    inline void fontSetCandidatePresentation(
        FontSourceStore& store,
        const std::vector<fontdecl::Declaracion>* fuentes,
        const std::string* temaDefault)
    {
        store.setCandidatePresentation(fuentes, temaDefault);
    }
}
