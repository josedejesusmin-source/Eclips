// ============================================================================
// ControlAdjust.h - EL AJUSTE DE UN `control` SIN PUNTERO
// ============================================================================
//
// `CP6` de `DISENO_ATOMO_TECLADO.md` (§9 + §14), alcance firmado 1a: los
// `control` y las `ranura` se navegan con el teclado. Este header es la mitad
// del VALOR; `FocusRing.h` es la mitad del INDICE.
//
// ── QUE RESUELVE, Y POR QUE NO ES "RUTEAR AL CAMINO DEL CLICK" ──────────────
//
// El §9 escribe *"`avanzar` activa el focado ruteando al camino de click
// existente"*, y para el choice, la ranura y el enlace eso es cierto: una vez
// que tenes el indice, sus caminos no dependen de donde esta el mouse. Medido,
// para el `control` es FALSO - tres de sus cuatro renders derivan el VALOR de
// la X del puntero:
//
//   Bar     `applyControlDrag(hit, vx)`   salta al valor del click
//   Arrows  `arrowDir(bc, vx)`            en que zona (izq/centro/der) cayo
//   Boxes   `boxIndexAt(bc, vx)`          indice absoluto bajo el cursor
//   Toggle  flip                          la unica libre de posicion
//
// Un teclado no tiene `vx`. No hay camino al que rutear: hay que escribir la
// ley del incremento, y es esta.
//
// ── LA PARIDAD QUE EL §9 INVOCA, MEDIDA ─────────────────────────────────────
//
// El §9 pide *"paridad exacta con el `SettingsState` C++ (A/D)"*. Fui a leerlo
// (`SettingsState.h:602-639`, `adjustValue(item, dir)`):
//
//   Bool      `m_fullscreen = !m_fullscreen`        FLIP, e IGNORA `dir`
//   Discrete  `clamp(idx + dir, 0, N-1)`            entero +-1
//   Range     `clamp(v + dir*0.1f, 0, 1)` + round   el DIEZ POR CIENTO
//
// La primera fila contesta la pregunta que el contrato dejaba abierta: sobre un
// toggle, izquierda y derecha hacen **lo mismo**. No es `izq=off / der=on`. El
// §9 ya decia "toggle flip"; la paridad lo confirma.
//
// ── EL HALLAZGO: `vstep` HACE DOS TRABAJOS, Y SOLO UNO ERA SUYO ─────────────
//
// La tercera fila no se puede replicar con el modelo del `.ecl`, y ahi esta el
// defecto. `musica_volumen` es `0..100` con `vstep=1` (`VNScriptState.h:6096`),
// asi que un +-`vstep` pelado son **cien** teclazos de punta a punta, contra
// los diez del menu C++ con el que el contrato se manda a comparar.
//
// El numero no estaba mal: estaba haciendo dos trabajos. Para el MOUSE `vstep`
// es la PRECISION -el drag snapea a su multiplo (`applyControlDrag:7398`)- y
// con `1` sobre `0..100` eso es exactamente lo que se quiere. Para una TECLA el
// mismo numero es el INCREMENTO, y ahi `1` es el numero equivocado. El mouse
// nunca lo noto porque arrastra; la tecla no arrastra.
//
// Es la forma de §7.486 sobre un float en vez de sobre un indice -un valor con
// dos duenos- y la de `TYPEWRITER-WRAP`: contado en una unidad, gastado en
// otra. La salida es la misma: darle dueno, no un gemelo.
//
//   paso de tecla = el `paso=` que el creador ESCRIBIO, si lo escribio
//                   si no, max(dstep del catalogo, rango/10)
//
// **El `rango/10` no es gusto: reproduce la paridad en las SIETE filas del
// catalogo vivo**, que es la unica razon por la que se puede firmar un numero
// magico. Medido contra `kCtrlTable` (`VNScriptState.h:6471-6484`):
//
// ⚠ Esa cita decia `:6096-6104` cuando la tabla vivia en `:6471-6480`: un
// ancla de COMENTARIO podrida, o sea la ficha `ANCLAS-EN-COMENTARIOS`
// -`chequeos.ps1` verifica las anclas `DOCS:NNNN` del manual y NADIE verifica
// estas-. Se corrigio al agregar la septima fila, que es cuando se leyo.
//
//   musica_volumen     0..100  vstep 1  ->  max(1, 10.0) = 10   10 teclazos
//   sonido_volumen     0..100  vstep 1  ->  max(1, 10.0) = 10   10 teclazos
//   velocidad_texto    0..3    vstep 1  ->  max(1,  0.3) =  1    4 posiciones
//   skip_no_visto      0..1    vstep 1  ->  max(1,  0.1) =  1   (flip igual)
//   pantalla_completa  0..1    vstep 1  ->  max(1,  0.1) =  1   (flip igual)
//   resolucion         0..N-1  vstep 1  ->  max(1, <1  ) =  1   1 opcion
//   idioma             0..N-1  vstep 1  ->  max(1, <1  ) =  1   1 opcion
//
// O sea: en las dos filas Range da el 10% del menu C++, y en las otras CINCO
// **no cambia nada**. Un numero que se cae solo donde no hace falta - y la
// septima fila lo confirma: `idioma` entra con el MISMO numero que su hermana
// Enum, asi que `CP5` no movio este magico, solo lo volvio a medir.
//
// ── Y POR QUE EL `paso=` EXPLICITO GANA SIEMPRE ─────────────────────────────
//
// Porque si no, el motor le corrige la mano al creador: quien escribe `paso=1`
// sobre un `0..100` lo escribio QUERIENDO control fino, y un `max` se lo
// pisaria. La distincion ya existe en el bind (`:6170`, `cmd.controlStep > 0`)
// y se perdia un renglon despues, al colapsar en un float sin recibo. Es
// `{pausa=0}` / `RO-3` otra vez: **un valor que el creador escribio no es lo
// mismo que uno que no escribio, y un float con centinela no sabe la
// diferencia** (`harness_tags` §7). Por eso `pasoExplicito` viaja con el
// modelo: cuesta un `bool` y es lo unico que separa "el default no servia" de
// "el creador pidio esto".
//
// ── LO QUE NO PUEDE VER ─────────────────────────────────────────────────────
//
// El CABLE: que `writeControlValue` reciba de verdad lo que esta ley devuelve,
// y que el bind lleve el `pasoExplicito` desde `cmdControl`. Vive en
// `VNScriptState.h`, el header gigante - misma pared que `MERGE-DUPLICADO`.
// Lo cubre el feel-test sobre `config.ecl`.
// ============================================================================

#pragma once

namespace eclips {

    // Espejo de `VNScriptState::CtrlRender` (`:6076`). Vive aca duplicado
    // **como enum, no como ley**: ese enum es privado de una clase que arrastra
    // GL, y la bateria no puede incluirlo.
    //
    // **Lo que ningun harness puede ver es que el adaptador del call-site mapee
    // las cuatro filas bien** -para eso habria que incluir el enum de alla-, asi
    // que el adaptador se escribe con un `switch` SIN `default`: el compilador
    // pone el rojo que el harness no puede. Es la unica red que existe aca, y se
    // dice en vez de prometer un assert que no discrimina (§7.397).
    enum class CtrlRenderKind : unsigned char { Bar, Toggle, Arrows, Boxes };

    // El modelo resuelto de un `control`, sin nada del render ni del puntero.
    // `vstep` es el del bind (`BoundControl::vstep`): la PRECISION del drag.
    // `pasoExplicito` es el recibo de si el creador escribio `paso=`.
    struct CtrlModelo {
        float          vmin          = 0.0f;
        float          vmax          = 100.0f;
        float          vstep         = 1.0f;
        bool           pasoExplicito = false;
        CtrlRenderKind render        = CtrlRenderKind::Bar;
    };

    // ── EL PASO DE UNA TECLA ────────────────────────────────────────────────
    //
    // Nunca menor que `vstep`: bajar de la precision del modelo daria valores
    // que el drag no puede producir, o sea dos duenos otra vez.
    inline float controlPasoTeclado(const CtrlModelo& m)
    {
        if (m.pasoExplicito) return m.vstep;
        const float rango = m.vmax - m.vmin;
        const float decimo = (rango > 0.0f) ? (rango / 10.0f) : 0.0f;
        return (decimo > m.vstep) ? decimo : m.vstep;
    }

    // ── EL AJUSTE ───────────────────────────────────────────────────────────
    //
    // `dir` es -1 (izquierda) o +1 (derecha). Devuelve el valor NUEVO; no
    // escribe nada - quien escribe es el call-site, por el mismo punto unico
    // que usa el mouse (`writeControlValue`), o el read-back se desincroniza.
    //
    // El `Toggle` IGNORA `dir` a proposito: es la fila Bool de `adjustValue`,
    // medida. Si algun dia izquierda apagara y derecha prendiera, esto se
    // INVIERTE y se anota - no se parchea (§7.328).
    inline float controlAjustar(const CtrlModelo& m, float actual, int dir)
    {
        if (dir == 0) return actual;

        if (m.render == CtrlRenderKind::Toggle)
            return (actual >= 0.5f) ? 0.0f : 1.0f;   // flip, sin mirar `dir`

        const float d = (dir > 0) ? 1.0f : -1.0f;

        // Arrows y Boxes son indices: el valor ES el entero, y el mouse los
        // redondea antes de moverse (`arrowDir`/`boxIndexAt` -> `(int)(v+0.5)`).
        // Redondear aca es lo que hace que teclado y mouse coincidan sobre el
        // mismo control en vez de derivar medio paso.
        if (m.render == CtrlRenderKind::Arrows || m.render == CtrlRenderKind::Boxes)
        {
            const int cur = (int)(actual + 0.5f);
            int nxt = cur + (int)d;
            if ((float)nxt < m.vmin) nxt = (int)m.vmin;
            if ((float)nxt > m.vmax) nxt = (int)m.vmax;
            return (float)nxt;
        }

        float v = actual + d * controlPasoTeclado(m);
        if (v < m.vmin) v = m.vmin;
        if (v > m.vmax) v = m.vmax;
        return v;
    }

} // namespace eclips
