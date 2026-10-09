// ============================================================================
// ECLIPS ENGINE - GameState (Estado de Juego)
// ============================================================================
// Documento de diseño: Sección 10.11
//
// Cada "modo" del juego es un GameState:
//   - La Visual Novel es un estado
//   - El gameplay top-down es un estado
//   - El menú de pausa es un estado
//   - Un overlay de diálogo sobre gameplay es un estado
//
// Los estados se apilan en el StateStack (ver StateStack.h).
// El estado en la cima recibe el input. Los de abajo pueden seguir
// renderizándose (para overlays) o pausarse.
// ============================================================================

#pragma once

#include "Types.h"

namespace eclips
{
    // Sólo para la firma de onLangStoreChanged (ficha `F8-SIN-IDIOMA`). Es una
    // forward decl y no un include a propósito: GameState.h lo incluye TODO el
    // motor, y `LangStore.h` arrastra <filesystem>.
    class LangStore;

    // Clase base abstracta. Todos los estados heredan de esta.
    class GameState
    {
    public:
        virtual ~GameState() = default;

        // ── Ciclo de vida ──
        //
        // CONTRATO de hooks (§2.4, sesión Ciclo-de-Vida-B):
        //  · P2.4 — onPause del estado de abajo y onEnter del nuevo pueden
        //    dispararse INLINE durante un push() inmediato (p. ej. desde
        //    handleEvent de un overlay modal). Manténlos triviales: sin mutar
        //    el stack, sin trabajo pesado.
        //  · Idempotencia de onPause — al drenar la cola, un lote pop-luego-push
        //    puede llamar onPause de un estado que YA estaba pausado, sin un
        //    onResume intermedio. onPause debe ser idempotente.
        //  · Resume-neto — el stack garantiza UNA reanudación por lote al tope
        //    final (ver invariante en StateStack::processQueue). No asumas un
        //    onResume por cada onPause.

        // Se llama una vez al entrar al estado
        virtual void onEnter() {}

        // Se llama una vez al salir del estado
        virtual void onExit() {}

        // Se llama cuando otro estado se apila encima (este se pausa)
        virtual void onPause() {}

        // Se llama cuando el estado de encima se retira (este se reanuda)
        virtual void onResume() {}

        // Aviso de que el ProjectManifest se recargó en caliente (F8, dev-only).
        // Default vacío: la mayoría de states no consumen el manifest. El que
        // sí (VNScriptState) lo override para re-aplicar lo derivado en vivo.
        // TODO(modo-creador): se compila fuera en build de export.
        virtual void onManifestReloaded() {}

        // El LangStore ACABA DE NACER (F8, dev-only. Ficha `F8-SIN-IDIOMA`,
        // fork 4c). Hermano de onManifestReloaded, y existe por un caso y uno
        // solo: el creador agrega la key `idiomas` a un proyecto que no la
        // tenía y aprieta F8. Ahí `Engine` construye el store por primera vez,
        // y los states YA VIVOS cachearon el `LangStore*` en su ctor —desde
        // main.cpp, que es el único inyector— con valor nullptr.
        //
        // NO se dispara al recargar ni al apagar, y no por ahorro: en esos dos
        // casos el objeto es EL MISMO (`load()` sobre el existente), así que el
        // puntero que los states tienen sigue siendo correcto. Un notify ahí
        // sería ruido que enseña a re-apuntar cuando no hace falta.
        //
        // Corre dentro de processEvents (vía Engine::reloadManifest), así que
        // §1.34 prohíbe trabajo GL acá: el override es UNA asignación.
        // TODO(modo-creador): se compila fuera en build de export.
        virtual void onLangStoreChanged(LangStore*) {}

        // Aviso de que el creador pidió recargar texturas de disco (F8, dev-only).
        // DISTINTO de onManifestReloaded: NO está gated por la validez del
        // proyecto.eclips. El creador repinta un sprite (mismo path) y F8 debe
        // refrescarlo aunque no haya manifest (modo test §7.14). Default no-op.
        // TODO(modo-creador): se compila fuera en build de export.
        virtual void onReloadTextures() {}

        // ── Game loop (Fixed Timestep) ──
        // Lógica a 60Hz fijo. dt siempre es FIXED_TIMESTEP.
        virtual void update(f64 dt) = 0;

        // Renderizado a ritmo variable. alpha es para interpolación.
        virtual void render(f64 alpha) = 0;

        // Procesar un evento SDL (input, ventana, etc.)
        // Retorna true si el evento fue consumido.
        virtual bool handleEvent(const void*) { return false; }

        // ── Configuración ──
        // ¿Debe renderizarse el estado de abajo cuando este está encima?
        // true = overlay (diálogo sobre gameplay, como Corpse Party)
        // false = pantalla completa (el estado de abajo no se ve)
        virtual bool isTransparent() const { return false; }

        // ¿Debe actualizarse el estado de abajo cuando este está encima?
        // Normalmente false (el estado de abajo se pausa)
        virtual bool allowsUpdateBelow() const { return false; }

        // ── Skip-frame (Perfilado §6): ¿este estado está en reposo visual? ──
        // Quiescente = nada de lo que este estado dibuja cambiaría si el motor
        // saltara el próximo frame (fase en espera, sin tweens/typewriter/
        // efectos/juice en curso). El StateStack agrega sobre el conjunto
        // VISIBLE: se salta render+swap solo si TODOS los visibles son quiescentes.
        //   Default false = OFF POR IDENTIDAD: mientras ningún estado lo
        //   override, el motor se comporta byte-idéntico a hoy (repinta 60fps).
        // El primer caller real es VNScriptState (CP2) con el predicado §6.3.
        virtual bool isQuiescent() const { return false; }

        // ── ¿Este estado es una PANTALLA DE MENU (y no el juego)? ────────────
        //
        // Lo consume la ley de `TransicionPantalla.h`: menu naciendo sobre menu
        // es `navegar`, y todo lo demas es `abrir`. Es la distincion que el
        // feel-test del fundador pidio -*"si estas en menu no se si deberia
        // actuar como si recien lo abrieras"*- y que `VNScriptState.h:436` no
        // podia hacer, porque prendia el fundido sin un solo `if` alrededor.
        //
        //   Default false = EL LADO CONSERVADOR, y esa eleccion importa: un
        //   estado que no contesta se trata como juego, con lo cual la
        //   transicion cae en `abrir`, que es la conducta de siempre. Un
        //   default `true` haria que un estado nuevo cambiara de transicion
        //   sin que nadie lo escribiera.
        virtual bool esPantallaDeMenu() const { return false; }
    };

} // namespace eclips
