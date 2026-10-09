// ============================================================================
// PointerPresence.h - EL PUNTERO TIENE DUENO, Y SABE ESTAR AUSENTE
// ============================================================================
//
// `CP7` de `DISENO_ATOMO_TECLADO.md` (§10.1 + §14), pierna 1: las costuras de
// touch. Ley pura - no toca SDL salvo para LEER un evento, no conoce rects, no
// sabe que existe una caja de dialogo.
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// El §10.1 del contrato dice, palabra por palabra: *"SDL marca los eventos de
// mouse sintetizados con `which == SDL_TOUCH_MOUSEID` -> tras despachar un tap,
// no persistir hover/foco (limpiar indice). La ficha lo nombraba; la deteccion
// es un `if`"*.
//
// **La deteccion ES un `if`, y esta abajo. El no-persist NO lo es**, y la razon
// se midio el 2026-08-10 antes de escribir una linea: la POSICION del puntero
// no tiene dueno. Se polea `SDL_GetMouseState` en CINCO sitios vivos de
// `VNScriptState.h`, y hay DOS mas que se empujan desde el evento:
//
//     :1518  poll  -> `m_dialogBox.setMousePos`  (chrome x6 + hover de choice)
//     :1759  poll  -> `m_hotspots.updateHover`   (WaitingClick)
//     :1795  poll  -> `updateInteractiveHover`   (los `enlace`)
//     :2207  poll  -> el PARALLAX, dentro de `render()`
//     :6700  poll  -> `focoBajoPuntero`, hit-test del `control`
//     :908   push  -> `m_hoveredSlot`            (las `ranura`)
//     :651/:692/:716  push -> los cursores de los tres modales
//
// **Siete consumidores, ningun dueno.** `CP5` y `CP6` le dieron dueno al INDICE
// -es la ley 2 de `FocusRing.h`, y §7.486 la escribio- y la POSICION quedo
// afuera. Un `if` que limpie el hover una sola vez no alcanza: los cinco
// polleos lo re-derivan AL FRAME SIGUIENTE, que es literal el bug que `CP5`
// arreglo para los choices, una capa mas abajo.
//
// ── Y HAY UNA MITAD ANTERIOR: EL TAP SE RUTEA MAL ANTES DE PERSISTIR ────────
//
// `processEvents()` corre ANTES de `update()` (`Engine.cpp:481` vs `:497`), asi
// que todo hover derivado en el `update` es del batch ANTERIOR. Con un mouse eso
// es invisible: uno mueve, pasan frames, despues clickea - el hover llega tibio.
// **Con touch no hay "mover antes"**: SDL sintetiza MOTION + BUTTONDOWN + BUTTONUP
// en el mismo batch, asi que a la hora de decidir el click el hover es el del
// TAP ANTERIOR.
//
// Y los seis caminos de click del VN no leen lo mismo:
//
//     `hotspot`  (`VNScriptState.h:1206`)     e->button.x/y      <- fresco
//     `control`  (`VNScriptState.h:958`)      e->button.x/y      <- fresco
//     `ranura`   (`VNScriptState.h:1007`)     e->button.x/y      <- fresco
//     `enlace`   (`VNScriptState.h:1020`)     hoveredInteractive()   <- rancio
//     `choice`   (`VNDialogBox.cpp:1528`)     m_choiceHover          <- rancio
//     chrome x6  (`VNDialogBox.cpp:1490`)     m_mouseVX/VY           <- rancio
//
// **`CP5` escribio que no habia skew** (`VNDialogBox.cpp:1536`: *"el `m_mouseVX`
// que este bloque leia a la hora del click tambien era el del frame anterior"*).
// La frase es CIERTA y su conclusion vencio: comparaba la ley vieja con la nueva
// -las dos igual de viejas-, no afirmaba que la posicion estuviera fresca. Es
// §7.488 con otro sustantivo: el mismo dato, un lector nuevo que lo mide en otro
// TEMPO.
//
// ── POR QUE LA PIERNA NO SE PUEDE PARTIR ────────────────────────────────────
//
// Poner solo el regimen -*"tras el tap se apaga todo"*- convierte *"activa el
// objetivo anterior"* en *"no activa nada, nunca"*: `focoConfirmar(-1)` no elige,
// `hoveredInteractive()` devuelve `nullptr` y el chrome no dispara. **Media
// pierna 1 deja el touch PEOR que hoy**, y por eso las dos mitades entran juntas.
//
// La buena noticia es que son EL MISMO cambio, no dos: darle dueno a la posicion
// es pasarla de POLLEO a EMPUJE, y recien ahi `presente = false` se vuelve
// expresable - `SDL_GetMouseState` no lo puede devolver nunca.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   EL PUNTERO SOLO SE MUEVE POR EVENTO. Sin evento de puntero, la
//           posicion se queda donde estaba. Es lo que borra el skew de tempo:
//           el click y el hover leen el MISMO valor en el MISMO frame
//   ley 2   UN DEDO QUE SE LEVANTA NO DEJA HOVER. `Solto` tactil -> ausente.
//           `Solto` de mouse -> presente, porque el cursor sigue ahi. Es el
//           unico lugar donde el `if` de `SDL_TOUCH_MOUSEID` cambia una conducta
//   ley 3   AUSENTE ES UNA POSICION QUE NINGUN RECT CONTIENE. No hay un `bool`
//           nuevo que cada consumidor tenga que consultar: el centinela (-1,-1)
//           apaga los siete de una vez. **El motor ya escribia esa guarda**
//           -`m_mouseVX > 0 && m_mouseVY > 0` (`VNDialogBox.cpp:284` y `:315`)-
//           y era SIEMPRE cierta, porque el cursor siempre esta en algun lado.
//           Esta ley le da a esa guarda el significado que su autor le quiso dar
//
// ── LO QUE NO PUEDE VER ─────────────────────────────────────────────────────
//
// El CABLE, y son SIETE - los siete consumidores de arriba leyendo esta posicion
// en vez de polear la suya. Las dos TUs son el header gigante y `VNDialogBox.cpp`
// (que arrastra GL): misma pared que `MERGE-DUPLICADO`. Lo cubre la sonda de
// touch, que inyecta eventos con `which == SDL_TOUCH_MOUSEID` por `SDL_PushEvent`
// - **y esa sonda existe porque el feel-test del fundador no puede dar esta
// prueba: su maquina no tiene pantalla tactil.**
//
// *No confundir con "el touch no clickea": un tap SIGUE resolviendo a `CLIC` y
// por lo tanto a `avanzar` (la tabla del `CP0` lo liga desde el primer dia). Lo
// que este archivo le saca al tap es el HOVER, no el click.*
// ============================================================================

#pragma once

#include <SDL_events.h>
#include <SDL_touch.h>   // SDL_TOUCH_MOUSEID

namespace eclips {

    // ── EL ATOMO: DONDE ESTA, Y SI ESTA ─────────────────────────────────────
    //
    // `presente == false` no es "esta en (-1,-1)": es "no hay puntero". Los dos
    // se escriben igual a proposito (ley 3), para que un consumidor que solo
    // sabe comparar coordenadas quede apagado sin aprender un campo nuevo.
    struct Puntero
    {
        float vx       = -1.0f;
        float vy       = -1.0f;
        bool  presente = false;
    };

    // Que le hizo el jugador al puntero. `Ninguno` cubre todo lo demas -teclas,
    // rueda, ventana-, y no es lo mismo que "no paso nada": la rueda ES un
    // evento de puntero para la CAPA 1 (`inputBindDeEvento` la traduce a un
    // verbo) y NO mueve el cursor, asi que aca tiene que caer en `Ninguno` o
    // arrastraria la posicion a un (0,0) que el evento no trae.
    enum class PunteroGesto : unsigned char { Ninguno, Movio, Apreto, Solto };

    struct PunteroEvento
    {
        PunteroGesto gesto  = PunteroGesto::Ninguno;
        bool         tactil = false;
        int          x      = 0;   // coords de VENTANA, crudas del evento
        int          y      = 0;
    };

    // ── LEY 2 - LA DETECCION (el `if` que el §10.1 promete) ─────────────────
    //
    // Mira el `type` ANTES de tocar el union, igual que `inputBindDeEvento`, y
    // por la misma razon: es `H10`, el bug que `CP0` podo. Los tres eventos de
    // mouse tienen `which`; la rueda tambien lo tiene y aun asi cae en
    // `Ninguno`, porque no mueve el cursor.
    inline PunteroEvento punteroEventoDe(const SDL_Event* e)
    {
        PunteroEvento out;
        if (!e) return out;

        switch (e->type) {
        case SDL_MOUSEMOTION:
            out.gesto  = PunteroGesto::Movio;
            out.tactil = (e->motion.which == SDL_TOUCH_MOUSEID);
            out.x = e->motion.x;
            out.y = e->motion.y;
            return out;
        case SDL_MOUSEBUTTONDOWN:
            out.gesto  = PunteroGesto::Apreto;
            out.tactil = (e->button.which == SDL_TOUCH_MOUSEID);
            out.x = e->button.x;
            out.y = e->button.y;
            return out;
        case SDL_MOUSEBUTTONUP:
            out.gesto  = PunteroGesto::Solto;
            out.tactil = (e->button.which == SDL_TOUCH_MOUSEID);
            out.x = e->button.x;
            out.y = e->button.y;
            return out;
        default:
            return out;
        }
    }

    // ── LEY 1 + LEY 2 - LA APLICACION ───────────────────────────────────────
    //
    // `vx`/`vy` llegan YA convertidas a coordenadas virtuales: la conversion
    // (`mouseToVirtual`) depende del viewport, que es del state. Este archivo no
    // la replica - replicarla seria el replicante silencioso de la regla 5.
    //
    // `Movio` y `Apreto` dejan el puntero PRESENTE aunque sean tactiles, y eso
    // no es un descuido: mientras el dedo toca, el dedo ESTA ahi, y de eso
    // depende que el tap acierte su objetivo (la mitad del tempo). Lo que el
    // dedo no hace es QUEDARSE.
    inline Puntero punteroAplicar(const Puntero& prev, PunteroGesto gesto,
                                  bool tactil, float vx, float vy)
    {
        switch (gesto) {
        case PunteroGesto::Movio:
        case PunteroGesto::Apreto: {
            Puntero p; p.vx = vx; p.vy = vy; p.presente = true; return p;
        }
        case PunteroGesto::Solto:
            if (tactil) return Puntero{};          // el dedo se fue: sin hover
            { Puntero p; p.vx = vx; p.vy = vy; p.presente = true; return p; }
        default:
            return prev;                           // ley 1: sin evento, quieto
        }
    }

    // Comodidad para el call-site, que ya tiene el `SDL_Event` y las virtuales.
    inline Puntero punteroAplicarEvento(const Puntero& prev,
                                        const PunteroEvento& ev,
                                        float vx, float vy)
    {
        return punteroAplicar(prev, ev.gesto, ev.tactil, vx, vy);
    }

} // namespace eclips
