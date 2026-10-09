// ============================================================================
// VNInputAction.h - EL VERBO QUE EL VN DESPACHA
// ============================================================================
//
// Aca vive `InputAction`, y NADA MAS. Es un `enum class` de 19 valores que
// hasta el `CP1` del atomo de teclado vivio adentro de `VNDialogBox.h`.
//
// ── POR QUE SE MUDO, Y POR QUE ESO NO ES COSMETICA ──────────────────────────
//
// `VNDialogBox.h` incluye `TextRenderer.h`, que arrastra `<SDL_opengl.h>`. Ese
// arrastre convertia al enum en **inalcanzable para la bateria**: cualquier ley
// que devolviera un `InputAction` no se podia probar, porque el harness que la
// incluyera tendria que linkear GL.
//
// Y la ley que `CP1` mas necesita custodiar devuelve exactamente eso: *"un
// verbo mas una fase dan LA MISMA `InputAction` que la cascada de `SDLK_*` daba
// antes"*. Sin este archivo, esa afirmacion se queda en prosa.
//
// **La pared era de UBICACION, no de acoplamiento** - §7.433, el mismo caso que
// `parseRichText` cuando bajo a `InlineTags.h`. El enum ya era puro: no toca SDL,
// no toca GL, no toca un solo tipo del render. Estaba del otro lado de la pared
// por donde nacio, no por lo que necesita.
//
// **El move es un NO-OP verificable y esa es toda su gracia:** mismo namespace
// (`eclips`), mismo nombre, mismos valores en el mismo orden. Los 45 call-sites
// de los tres archivos que lo usan -`VNScriptState.h`, `VNDialogBox.cpp` y
// `VNDialogBox.h`- **no cambian una letra**. Si alguno hubiera tenido que
// cambiar, el move estaria mal hecho.
//
// **El orden de los valores no es libre.** Nadie serializa este enum hoy, pero
// `UIBtnAuto..UIBtnMenu` se recorren por indice en el hit-test del chrome
// (`VNDialogBox.cpp`), asi que reordenarlos le cambia el boton a un click.
// Agregar valores va AL FINAL de su grupo.
//
// ── QUIEN ES EL DUENO ───────────────────────────────────────────────────────
//
// El enum es del **despacho** (Capa 3 del `DISENO_ATOMO_TECLADO.md`), no de la
// caja. La caja es un consumidor mas: el otro es la seccion 2 de
// `VNScriptState::handleEvent`. Por eso vive en `core` y no en `render`.
// ============================================================================

#pragma once

namespace eclips {

    enum class InputAction {
        None,
        Confirm,
        Choice1, Choice2, Choice3, Choice4,
        Cancel,
        HistoryOpen, HistoryScrollUp, HistoryScrollDown, HistoryClose,
        ToggleAuto, ToggleSkip,
        QTEKey,
        UIBtnAuto, UIBtnSkip, UIBtnSave, UIBtnLoad, UIBtnLog, UIBtnMenu,

        // `CP5` - el anillo de foco (`FocusRing.h`). Van al FINAL y no adentro
        // del grupo de los choices, que es donde "encajarian" por tema: los
        // `UIBtn*` se recorren por indice en el hit-test del chrome y meter un
        // valor antes les correria el boton a cada click. Que sean los ultimos
        // no dice nada de su importancia - dice que el orden de arriba ya tiene
        // dueno.
        FocusUp, FocusDown,

        // `CP6` - izq/der. **No mueven el foco: AJUSTAN** el valor del `control`
        // focado (`ControlAdjust.h`), que es lo que el §9 les reserva desde el
        // 2026-07-02. Van despues de sus hermanos por la misma razon de arriba,
        // y el nombre conserva la familia `Focus*` a proposito: el verbo que los
        // produce es `foco_izquierda`/`foco_derecha`, y renombrarlos aca partiria
        // la familia en dos justo donde la tabla la tiene junta.
        FocusLeft, FocusRight
    };

} // namespace eclips
