// ============================================================================
// BoxPersistence.h - LA CAJA QUE SOBREVIVE A LA ELECCION
// ============================================================================
//
// Nucleo de la ficha `ELEGIR-CON-CAJA` (`F-04`, 2a mitad).
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// Hasta este lote, un `elegir` hacia desaparecer la caja de dialogo. Y no la
// ocultaba: NADIE LA DIBUJABA. La cadena de render es un `else if`
// (`VNScriptState.h:2193`), asi que `WaitingChoice` y `WaitingInput` se
// excluyen por construccion. Encima `startChoices` BORRA el texto
// (`VNDialogBox.cpp:171`), asi que aunque se agregara la llamada la caja
// saldria vacia. Son DOS actos deliberados, no un hueco.
//
// Lo que el fundador pidio es la variante que las visual novels usan para un
// aviso: la ultima linea sigue leyendose y las opciones aparecen arriba.
//
//     "iniciar protocolo?"
//     mostrar_caja
//     elegir:
//       "aceptar" -> si
//       "negar"   -> no
//
// ── POR QUE `mostrar_caja` Y NO UNA CLAVE NUEVA ─────────────────────────────
//
// Se midieron cuatro formas. Gano la que no inventa vocabulario:
//
//   · el motor YA tiene la palabra para "la caja esta visible", y con el ciclo
//     de vida correcto (`startDialogue` la re-muestra sola);
//   · `mostrar_caja` tiene DOS callers en 165 `.ecl`, los dos sondas, CERO en
//     contenido de juego: darle este significado no puede romper un guion;
//   · una clave en el bloque (`elegir [caja]`) fue descartada por el fundador
//     con el argumento correcto: ese nombre dice "las opciones van DENTRO de la
//     caja", que es la feature VECINA (la de gameplay) y probablemente entre
//     algun dia. Una clave que le roba el nombre a su vecina es la sintaxis
//     mintiendo;
//   · una clave de TEMA se descarto por §7.118: el default nunca se voltea, asi
//     que su unico valor escrito seria el default. Capacidad sin caller.
//
// ── UN COMANDO, UN TRABAJO (peticion explicita del fundador) ────────────────
//
// Esto PRESERVA la caja tal cual esta. NO le quita el chrome, NO sella nada.
// Los dos comportamientos vecinos ya tienen dueno y NO son de aca:
//
//   la caja sin botones  -> `botones_visibles no` en un `caja_dialogo` con
//                           nombre (`Theme.h:1262`), + `usar_caja`
//   no se puede guardar  -> `modalidad-aviso` (`VNScriptState.h:991`), que hoy
//                           se arma con `enlace ... requerido` y no con un
//                           `elegir`. Ficha aparte.
//
// Consecuencia declarada, no defecto: durante la eleccion los seis botones se
// DIBUJAN y no responden, porque su hit-test exige fase de dialogo
// (`VNDialogBox.cpp:1367`). Eso es correcto — `elegir` solo deja clickear las
// opciones. La inercia existe desde siempre; lo unico que cambia es que ahora
// se ve.
//
// ── POR QUE UN HEADER APARTE, Y PURO ────────────────────────────────────────
//
// Mismo motivo que `AppearPolicy.h` (RO-5) y `GroupAlpha.h`: el consumidor es
// `VNDialogBox.cpp`, que arrastra GL, asi que una ley que viva adentro NO SE
// PUEDE PROBAR. Aca no depende ni de `Types.h` — es un bool con reglas — y
// `harness_persistencia.cpp` la prueba entera.
//
// Vive junto a `AppearPolicy.h` porque es su hermana de concepto: las dos son
// politicas de la caja A LO LARGO DEL TIEMPO. La aparicion dice como ENTRA;
// esta dice si SOBREVIVE.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   NO-OP POR DEFECTO. Sin pedido explicito, la caja NO sobrevive a la
//           eleccion. Es la conducta de hoy y la de los 55 `elegir` del corpus.
//           Si esta ley cae, el lote no agrego una variante: le cambio la
//           conducta a todos
//   ley 2   EL PEDIDO ES PRESENCIA, NO ESTADO. "La caja esta visible" NO es el
//           pedido: `startDialogue` la muestra sola, asi que el estado no sabe
//           distinguir "lo pediste" de "es lo normal". Es el mismo problema que
//           `{pausa=0}` (RO-3) y se resuelve igual: un bit aparte
//   ley 3   VIDA UTIL DE UNA LINEA. Una linea de dialogo nueva apaga el pedido.
//           Es LA MISMA vida util que ya tiene `m_hidden`
//           (`VNDialogBox.cpp:117`), y esa es la razon: dos relojes en el mismo
//           objeto es como se desincronizan las cosas. El modo de falla es
//           blando — escribir `mostrar_caja` ANTES de la linea en vez de
//           despues da la conducta de HOY, no una rota
//   ley 4   `ocultar_caja` APAGA EL PEDIDO. Pedir esconder y que reaparezca en
//           la eleccion seria el comando contradiciendose
//   ley 5   EL RELOJ DEL DIALOGO NO CORRE FUERA DEL DIALOGO. El bloque de
//           typewriter/auto/skip (`VNDialogBox.cpp:289`) estaba gateado por
//           "hay texto" — y el texto se borraba, asi que la fase nunca hizo
//           falta. Al dejar de borrarlo, ese gate deja de alcanzar: sin este
//           cambio, `{interrumpir}` o una `A` apretada emiten `Advance` en
//           plena eleccion. La allowlist de §7.74 igual lo atajaria, pero el
//           `return` temprano se come el tick del timer del `elegir`.
//           **Es el guardian del bug que el fundador recuerda**: el `.ecl` que
//           se cerraba solo al clickear en cualquier lado
//
//   OJO: la ley 5 se PARTIO EN DOS el 2026-08-04 (`D-37`, contrato
//        `DISENO_ELEGIR_BEAT.md`). Su frase queda arriba como se escribio, y
//        era mas gruesa que la real: lo que no puede correr fuera del dialogo
//        es el AVANCE, no el reloj. La ley viva son `typewriterRuns` y
//        `advanceClockRuns`, abajo, y el porque esta ahi.
// ============================================================================

#pragma once

namespace eclips
{
    // ── El pedido del guion ─────────────────────────────────────────────────
    //
    // Un bool con reglas de encendido y apagado. Se llama "persistencia" y no
    // "visibilidad" a proposito: la visibilidad ya existe (`m_hidden`) y esto
    // NO la reemplaza — la acompana. `m_hidden` contesta "se ve ahora";
    // esta contesta "sobrevive a lo que viene".
    struct BoxPersistence
    {
        // `mostrar_caja` / `show_box`. ley 2: el pedido nace ACA y en ningun
        // otro lado. Ningun camino automatico lo enciende.
        void keep() { m_requested = true; }

        // `ocultar_caja` / `hide_box`. ley 4.
        void drop() { m_requested = false; }

        // `startDialogue`, o sea toda linea de dialogo nueva. ley 3.
        void newLine() { m_requested = false; }

        // La consulta, y el punto unico que consume el render. ley 1: sin
        // pedido devuelve false, que es la conducta de siempre.
        bool survivesChoice() const { return m_requested; }

    private:
        bool m_requested = false;
    };

    // ── ley 5, PARTIDA EN DOS POR `D-37` (2026-08-04) ───────────────────────
    //
    // Aca vivia `dialogueClockRuns(inDialoguePhase, hasText)`, una sola puerta
    // para el bloque entero de typewriter/auto/skip, y su frase era *"el reloj
    // del dialogo no corre fuera del dialogo"*.
    //
    // **Esa ley era mas GRUESA que la real, y se retira con su razon puesta**
    // (§7.328). Lo que no puede correr fuera del dialogo es el AVANCE, no el
    // reloj. Hasta `elegir-con-caja` los dos eran indistinguibles porque nunca
    // habia texto escribiendose durante una eleccion — el texto se borraba. La
    // forma pegada de `D-33` (`"linea" elegir:`) los separa: ahi el caption se
    // escribe CON las opciones ya en pantalla, que es la feature entera.
    //
    // Las dos siguen puras y aparte del struct para que el harness las pruebe
    // sin construir nada, igual que `composeGroupAlpha`.

    // EL TYPEWRITER — escribe letras. Corre en dialogo, y tambien durante una
    // eleccion SI la caja sobrevive: si no se dibuja, no hay nadie mirando y
    // escribir seria estado corriendo a ciegas.
    inline bool typewriterRuns(bool inDialoguePhase, bool hasText,
                               bool boxSurvivesChoice)
    {
        return hasText && (inDialoguePhase || boxSurvivesChoice);
    }

    // EL AVANCE — skip, auto y `{interrumpir}` emiten `Advance`. NUNCA fuera
    // del dialogo, y este termino es **exactamente la ley vieja**: es el
    // guardian del bug que el fundador recuerda (§7.74, el `.ecl` que se
    // cerraba solo al clickear en cualquier lado). Que conserve la firma de la
    // vieja es a proposito: deja al control negativo del harness correr la una
    // contra la otra con las mismas aserciones.
    inline bool advanceClockRuns(bool inDialoguePhase, bool hasText)
    {
        return inDialoguePhase && hasText;
    }

    // ── EL RELOJ DE LA ELECCION ESPERA AL CAPTION ───────────────────────────
    //
    // Firmado 2026-08-04, sobre `D-33`. Con la forma pegada las opciones salen
    // CON el texto, asi que un `elegir [tiempo: 5s]` empezaria a descontar
    // mientras el jugador todavia esta leyendo la linea. `tiempo:` significa
    // *"N segundos para DECIDIR"*, no para leer: el countdown arranca cuando el
    // caption termina de escribirse.
    //
    // Fuera de la forma pegada es un NO-OP exacto, y por eso no le cambia el
    // ritmo a los 10 `elegir` con timer del corpus: sin caja sobreviviente no
    // hay caption escribiendose, y con `mostrar_caja` el texto ya estaba
    // completo (el jugador clickeo para llegar hasta el `elegir`).
    //
    // Vive aca y no en `VNDialogBox.cpp` por `RO-5`: esa TU arrastra GL, y una
    // ley que vive adentro no se puede probar.
    inline bool choiceTimerRuns(bool hasChoices, bool timerArmed,
                                bool captionWriting)
    {
        return hasChoices && timerArmed && !captionWriting;
    }

} // namespace eclips
