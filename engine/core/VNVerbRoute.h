// ============================================================================
// VNVerbRoute.h - LA CAPA 3 DEL VN: DE VERBOS A ACCIONES
// ============================================================================
//
// `CP1` + `CP1b` de `DISENO_ATOMO_TECLADO.md` (§14), con las enmiendas de sus
// §17, §18 y §19.
//
// **Este archivo tiene DOS leyes hermanas, y conviene saber cual buscas:**
//
//   `vnAccionDeVerbos`  (`CP1`)  verbo + fase -> `InputAction`. Lo que despacha
//                                la CAJA de dialogo. Corre en TRES fases.
//   `vnAtajoDeVerbos`   (`CP1b`) verbo + rol + sellos -> efecto + causa. Lo que
//                                despacha el STATE. Corre en TODAS.
//
// Son dos y no una porque sus dominios de fase son distintos; fusionarlas
// obligaria a una a mentir sobre cuando aplica.
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// `translateSDLEventToAction` comparaba `SDLK_*` literales: once comparaciones
// que mezclaban las tres capas del contrato en una sola cascada. La tabla del
// `CP0` ya traduce tecla -> verbo (Capa 1, `InputManager.h`); lo que faltaba era
// el otro lado: **verbo + fase -> `InputAction`**, que es Capa 3.
//
// Este header es esa mitad, y es pura: no toca SDL, no conoce keycodes, no sabe
// que existe un teclado. Recibe la lista de verbos que el manager resolvio y
// contesta que accion despacha el VN.
//
// ── LA LEY 5 ES LA RAZON DE QUE ESTO RECIBA UNA LISTA ───────────────────────
//
// **Un evento activa VARIOS verbos** (`E-5` del contrato, §7.468). `ARRIBA`
// resuelve a DOS en el dominio `vn`: `foco_arriba` (el anillo, `CP5`) y
// `historial_arriba`. **Al 2026-08-07 los dos tienen consumidor y los desempata
// la FASE** (ley 6, abajo); hasta `CP4` el primero no tenia ninguno, que es por
// lo que la trampa de abajo estuvo armada cinco lotes. La §3.1 del contrato firmo
// `std::string resolve(...)`, que devuelve uno, y el `InputManager` guarda sus
// verbos en un `std::map` -o sea en orden ALFABETICO-, asi que el primero de
// `ARRIBA` es `foco_arriba`.
//
// **Usar `resolver()` aca seria el bug mudo del lote:** el scroll del historial
// se moriria y nada se pondria rojo, porque `foco_arriba` es un verbo legitimo
// que hoy no despacha nadie. Por eso la firma toma `resolverTodos` y el orden de
// esta funcion es de PRIORIDAD, no de llegada.
//
// ── LO QUE NO ENTRA ACA, Y ES DELIBERADO ────────────────────────────────────
//
// **El QTE no pasa por esta funcion.** `qteReclamaTecla` sigue siendo un
// early-return **arriba** de la resolucion, en `translateSDLEventToAction`, y
// eso es `E-11` textual: el QTE no tiene un verbo del dominio `vn` -su tecla la
// escribe el creador linea por linea, no el jugador en su config- asi que
// `resolverTodos` no lo puede expresar. Si alguien lo baja aca o lo mueve abajo
// de la cascada, `ESPACIO`, `ESC`, `ARRIBA` y `A` vuelven a ser inescribibles
// como tecla de QTE, que es justo lo que `CP0` acababa de arreglar.
// Su guardian es `harness_qte` §1.bis y §6; el de que siga ARRIBA, la §2 del
// harness de este archivo.
//
// **La seccion 2 de `handleEvent` tampoco pasa por ESTA funcion.**
// `guardado_rapido`, `carga_rapida`, `menu_guardar`, `menu_cargar` y
// `saltar_texto` **resuelven a un verbo y `vnAccionDeVerbos` contesta `None`
// igual**, para que el evento siga cayendo a los atajos globales. Ese `None` es
// la mitad del guardian de no-op -sin el, la caja se comeria la `K` y el
// quicksave dejaria de funcionar- y vive en la §5 del harness.
//
// > ⚠ **Este parrafo decia «La seccion 2 tampoco» a secas, y con `CP1b` eso
// > paso a ser FALSO leido de arriba: la ley de la seccion 2 vive en ESTE
// > archivo** (`vnAtajoDeVerbos`, mas abajo). Lo que sigue siendo cierto -y es
// > lo que el parrafo queria decir- es que no pasa por `vnAccionDeVerbos`: son
// > **dos leyes hermanas y separadas**, porque tienen dominios de fase
// > distintos. `vnAccionDeVerbos` corre en tres fases; `vnAtajoDeVerbos` corre
// > en todas. Fusionarlas obligaria a una de las dos a mentir sobre cuando
// > aplica.
// >
// > `saltar_texto` es el unico de los cinco que sigue afuera de las dos, y por
// > una razon de otra especie: **no es un evento**, se polea en `update()`
// > (`D-45` puerta D). Ver `B-1` de la §19 del contrato.
//
// ── LAS SEIS LEYES ──────────────────────────────────────────────────────────
//
//  1  `avanzar` gana sobre todo lo demas, en CUALQUIER fase. Es la unica regla
//     sin gate: la cascada vieja comparaba ESPACIO/ENTER/Z antes que nada, y el
//     clic izquierdo entraba por su propia rama sin mirar la fase.
//
//  2  `opcion_N` solo despacha en `WaitingChoice`. Fuera de esa fase los
//     numeros 1-4 no son nada, igual que antes.
//
//  3  El sello del motor se evalua **POST-traduccion**. `auto` bajo un modal
//     requerido no despacha, y esa condicion se lee aca -no adentro de la
//     traduccion, como estaba en `:2449`-, asi que `resolver` se queda **puro**
//     como la §3.1 lo firmo. Es la primera mitad de `SELLO-MOTOR-SOBRE-TECLA`
//     (`E-3`); la otra -los `SDLK_k/l/F5/F9` de la seccion 2- es de `CP1b`.
//
//  4  La familia del historial se desempata por el ESTADO DEL LOG, no por la
//     tecla. Es lo unico que la cascada vieja resolvia con un `if` adentro de la
//     rama de la rueda, y es la razon de que `RUEDA_ARRIBA` este ligada a DOS
//     verbos en la tabla: abre si esta cerrado, scrollea si esta abierto.
//
//  5  Un verbo que nadie despacha contesta `None` y **no es un error**. Los
//     cinco de la seccion 2 los come otro dueno, y `foco_izquierda/derecha`
//     todavia no tienen ninguno (son de `CP6`). `None` significa "que siga su
//     camino", que es exactamente lo que la cascada vieja hacia al no matchear.
//
//  6  **El anillo de foco desempata por FASE** (`CP5`). `foco_arriba/abajo`
//     despachan en `WaitingChoice` y con el log cerrado; en cualquier otra
//     combinacion la tecla sigue siendo del historial, como antes. Es la nota
//     de fidelidad de la §5 del contrato -*"la fase decide"*- cobrada, y son
//     las dos unicas celdas del barrido de `harness_ruta` §1 que este lote
//     cambia (su §1.3 las cuenta).
//
// ── LO QUE NO PUEDE VER ─────────────────────────────────────────────────────
//
// El CABLE: que `translateSDLEventToAction` llame de verdad a `resolverTodos` y
// a esta funcion, en ese orden y debajo del early-return del QTE. Esa TU es el
// header gigante (misma pared que `MERGE-DUPLICADO`). Lo cubre `sonda_qte.ecl`,
// que ya existe y lee rutas, mas el feel-test del corpus.
// ============================================================================

#pragma once

#include "engine/core/VNInputAction.h"
#include "engine/core/VNExecutionPolicy.h"   // VNPhase
#include "engine/core/VNActionSeal.h"        // CP1b: VNAction, vnActionAllowed, las voces

#include <string>
#include <vector>

namespace eclips {

    // Azucar de lectura. Existe para que el cuerpo de abajo se lea como la
    // cascada que reemplaza, en vez de como cinco `std::find`.
    inline bool vnHayVerbo(const std::vector<std::string>& verbos,
                           const char* verbo)
    {
        for (const std::string& v : verbos)
            if (v == verbo) return true;
        return false;
    }

    // ── LA LEY ──────────────────────────────────────────────────────────────
    //
    // `verbos`          lo que `InputManager::resolverTodos(e, "vn")` devolvio.
    // `phase`           la fase del VN, igual que la recibia la cascada vieja.
    // `logAbierto`      `m_dialogBox.isShowingHistory()`.
    // `modalRequerido`  `isModalRequired()` - el sello del motor, ley 3.
    //
    // El orden de los `if` ES la ley: replica el de la cascada de `SDLK_*` que
    // vivia en `translateSDLEventToAction`, y cada bloque lleva pegado el suyo.
    inline InputAction vnAccionDeVerbos(const std::vector<std::string>& verbos,
                                        VNPhase phase,
                                        bool logAbierto,
                                        bool modalRequerido)
    {
        // Ley 1 - la cascada vieja abria con
        //   `k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_z` (sin gate de
        //   fase) y con la rama `SDL_MOUSEBUTTONDOWN` del clic izquierdo. Los
        //   dos son el verbo `avanzar`, que en la tabla lleva las cuatro:
        //   ESPACIO, ENTER, Z, CLIC.
        if (vnHayVerbo(verbos, "avanzar"))
            return InputAction::Confirm;

        // Ley 2 - `WaitingChoice` conserva sus cuatro opciones y
        // `WaitingInterface` comparte sólo el anillo de foco. La espera de UI
        // no inventa choices ni delega en el dialog box.
        if (phase == VNPhase::WaitingChoice || phase == VNPhase::WaitingInterface)
        {
            if (phase == VNPhase::WaitingChoice)
            {
                if (vnHayVerbo(verbos, "opcion_1")) return InputAction::Choice1;
                if (vnHayVerbo(verbos, "opcion_2")) return InputAction::Choice2;
                if (vnHayVerbo(verbos, "opcion_3")) return InputAction::Choice3;
                if (vnHayVerbo(verbos, "opcion_4")) return InputAction::Choice4;
            }

            // Ley 6 (`CP5`) - EL ANILLO DE FOCO, y es el desempate que la ley 5
            //   venia esperando. `ARRIBA` resuelve a DOS verbos desde el `CP0`
            //   -`foco_arriba` y `historial_arriba`- y hasta hoy ganaba el
            //   segundo SIEMPRE, porque el primero no tenia consumidor. La nota
            //   de fidelidad de la §5 del contrato dice desde el 2026-07-02 que
            //   *"la fase decide"*, y esta es la fase.
            //
            //   **El gate del log no es simetria decorativa:** durante una
            //   eleccion el jugador puede abrir el backlog para releer antes de
            //   decidir, y si el foco le ganara ahi el log quedaria ilegible
            //   justo cuando mas se usa. Con el log abierto esto no dispara y la
            //   familia del historial (ley 4, abajo) se lleva la tecla, igual
            //   que antes de este lote.
            //
            //   **Y cuelga de `foco_arriba`, NO de `historial_arriba`.** Son la
            //   misma tecla, asi que escribirlo sobre la familia del historial
            //   compila y "anda" - pero ahi `REPAG` moveria el foco, y `REPAG`
            //   no es una tecla de foco en ninguna tabla. Guardian: §13.5.
            //
            //   *(El renglon de `CP5` decia: "`foco_izquierda/derecha` NO se
            //   despachan: una columna no tiene vecino horizontal, y el §9 se
            //   las reserva para AJUSTAR el valor de un `control` focado en
            //   `CP6`. Guardian con vencimiento firmado: §13.6". **Vencio el
            //   2026-08-08 y se invirtio hacia donde apuntaba**: se despachan, y
            //   lo que llevan es el ajuste.)*
            //
            //   **Las cuatro comparten el gate del log**, y eso no es simetria
            //   decorativa: durante una eleccion el jugador abre el backlog para
            //   releer antes de decidir, y ahi las flechas son del historial.
            //   Guardian: §13.6.bis.
            //
            //   Que un `FocusLeft` no mueva el anillo NO se decide aca: lo
            //   decide `focoMover`, que las trata como no-op explicito. La ruta
            //   entrega el verbo; el consumidor sabe que hacer con el.
            if (!logAbierto)
            {
                if (vnHayVerbo(verbos, "foco_arriba"))     return InputAction::FocusUp;
                if (vnHayVerbo(verbos, "foco_abajo"))      return InputAction::FocusDown;
                if (vnHayVerbo(verbos, "foco_izquierda"))  return InputAction::FocusLeft;
                if (vnHayVerbo(verbos, "foco_derecha"))    return InputAction::FocusRight;
            }
        }

        // Ley 7 (`CP6`) - EL ANILLO BAJO `modalidad-aviso`, y es la unica fase
        //   fuera de `WaitingChoice` donde el foco le gana a la familia del
        //   historial.
        //
        //   **Por que existe, medido:** `vnAcceptsDialogueAdvance` es
        //   `phase == WaitingInput && !modalRequerido`
        //   (`VNExecutionPolicy.h:31`), asi que durante un `enlace ok requerido`
        //   el avance esta SELLADO y el archivo del acceptance lo declara: *"la
        //   UNICA salida es el OK"*. Ese OK se activa en un solo sitio, detras
        //   de `hoveredInteractive()` (`VNScriptState.h:975-992`) - o sea con el
        //   MOUSE. Un jugador de teclado se queda clavado en un content-warning.
        //   Es el caso que el §8 del contrato llama *"el atasco seria total"*,
        //   sobre feature viva.
        //
        //   Va DESPUES de la ley 2 y ANTES de la 4 a proposito: los enlaces y
        //   los choices **nunca coexisten** (`linksFrozen` los congela en
        //   `WaitingChoice`, `VNScriptState.h:1706`; y bajo el aviso no hay
        //   bloque `elegir`), asi que el orden entre ellos no se puede observar
        //   - pero contra el HISTORIAL si: sin este bloque, `ARRIBA` durante un
        //   aviso modal abre el backlog en vez de mover el foco al OK.
        //
        //   `foco_izquierda/derecha` NO se despachan aca, y no es un olvido: un
        //   `enlace` no tiene valor que ajustar. Guardian: §13.10.
        if (phase == VNPhase::WaitingInput && modalRequerido && !logAbierto)
        {
            if (vnHayVerbo(verbos, "foco_arriba")) return InputAction::FocusUp;
            if (vnHayVerbo(verbos, "foco_abajo"))  return InputAction::FocusDown;
        }

        // `k == SDLK_ESCAPE` -> `Cancel`. Quien decide que significa un Cancel
        //   -cerrar el log, cancelar una eleccion, caer a la seccion 2 para
        //   abrir la pausa- sigue siendo el consumidor, sin cambios.
        if (vnHayVerbo(verbos, "menu"))
            return InputAction::Cancel;

        // Ley 3 - era `k == SDLK_a && !isModalRequired()`, o sea Capa 2 metida
        //   adentro de Capa 1. El termino no cambia; cambia DONDE se lee.
        if (vnHayVerbo(verbos, "auto"))
            return modalRequerido ? InputAction::None : InputAction::ToggleAuto;

        // Ley 4 - la familia del historial. `historial` (RUEDA_ARRIBA) es el
        //   verbo de ABRIR y solo despacha con el log cerrado; con el abierto
        //   la misma rueda cae al scroll de abajo, que es lo que la rama
        //   `if (!m_dialogBox.isShowingHistory())` hacia adentro del wheel.
        if (vnHayVerbo(verbos, "historial") && !logAbierto)
            return InputAction::HistoryOpen;
        if (vnHayVerbo(verbos, "historial_arriba"))
            return InputAction::HistoryScrollUp;
        if (vnHayVerbo(verbos, "historial_abajo"))
            return InputAction::HistoryScrollDown;

        // Ley 5 - `foco_*` (CP5), los cinco de la seccion 2, y todo lo que no
        //   este ligado. La cascada vieja terminaba en `return None` y esto es
        //   lo mismo: "no lo consumi, que siga".
        return InputAction::None;
    }

    // ========================================================================
    // CP1b - LA SEGUNDA LEY: LOS ATAJOS GLOBALES DE LA SECCION 2
    // ========================================================================
    //
    // `vnAccionDeVerbos` (arriba) es dueno de lo que la CAJA despacha, y solo
    // corre en tres fases. Esta ley es duena de lo que el STATE despacha, y
    // corre en todas: es la seccion 2 de `handleEvent` -los atajos globales-
    // reexpresada sobre verbos. Con ella se cierra `SELLO-MOTOR-SOBRE-TECLA`.
    //
    // ── POR QUE SON SEIS TECLAS Y NO CINCO ──────────────────────────────────
    //
    // La §18 del contrato listo CINCO verbos como "lo que `CP1b` hereda"
    // (`guardado_rapido`, `carga_rapida`, `menu_guardar`, `menu_cargar`,
    // `saltar_texto`) y **se dejo el `ESC` afuera**. `menu` tambien vive en la
    // seccion 2, y es de otra especie que los cinco: `vnAccionDeVerbos` **si**
    // lo despacha (a `Cancel`), asi que el `ESC` tiene DOS consumidores en
    // cascada -primero la caja, y si no lo consume, esta ley-.
    //
    // Y `saltar_texto` es de una TERCERA especie: no es un evento, se POLEA en
    // `update()` (`D-45` puerta D), asi que no entra aca ni podria. Su sello ya
    // esta sobre booleanos y no sobre la tecla, o sea que la ficha no lo
    // reclama; lo unico tecla-indexado suyo es LEER el CTRL, y eso es `held()`,
    // que llega con `CP2` (su otro caller es el eje del WASD).
    //
    // ── LA ASIMETRIA DEL `ESC`, QUE ES LO MAS FACIL DE ROMPER ───────────────
    //
    // El mismo verbo `menu` tiene DOS conductas de sello segun el rol, y esta
    // firmado en el sitio (`VNScriptState::handleEvent`, seccion 2):
    //
    //     rol Menu -> POPEA ("volver"), y **no se sella nunca**
    //     rol Game -> abre la pausa,   y **si** consulta el sello del creador
    //
    // Sellar el primero deja al jugador encerrado en un submenu, que es el
    // "atasco total" que la §8 del atomo y `D-46` prohiben cada uno por su
    // lado. Por eso el rol se pregunta ANTES que el sello y no despues: si la
    // ley consultara el sello una sola vez arriba, la asimetria se perderia y
    // nada se pondria rojo. Su guardian es la §10 del harness.
    //
    // ── POR QUE DEVUELVE UNA CAUSA Y NO SOLO UN EFECTO ──────────────────────
    //
    // Las VOCES son conducta observable. La seccion 2 imprime cinco mensajes
    // distintos -cinematica, modalidad-aviso, rol Menu raiz, rol Menu suprime,
    // y el del sello del creador-, y un guardian de no-op que comparara solo
    // "que efecto salio" daria **verde con la voz cambiada o muerta**. Es la
    // misma forma que `TrialOutcome::GateVeto` en `harness_gate_evento`: la
    // causa tiene nombre propio, asi que se puede afirmar.
    //
    // El texto exacto vive en `vnVozDeAtajo`, aparte, para que esta ley decida
    // y la otra hable. El harness afirma las dos.
    //
    // ── LO QUE NO PUEDE VER ─────────────────────────────────────────────────
    //
    // El CABLE: que la seccion 2 llame de verdad a esta ley, que ejecute el
    // efecto que devuelve, que imprima la voz y que respete el `consume`. Esa
    // TU es el header gigante (misma pared que `MERGE-DUPLICADO`).
    // ========================================================================

    // Lo que el state EJECUTA. `Ninguno` no es un error: es "esto no era mio".
    enum class VNAtajoEfecto : unsigned char {
        Ninguno,
        Popear,             // rol Menu: volver, revela lo de abajo
        AbrirPausa,
        GuardarRapido,      // slot 0
        CargarRapido,       // slot 0
        AbrirMenuGuardar,
        AbrirMenuCargar,
    };

    // Por que NO hubo efecto. Es lo que vuelve afirmables las voces.
    enum class VNAtajoCausa : unsigned char {
        Ninguna,                 // o hubo efecto, o el evento no era de aca
        MenuRaiz,                // rol Menu en la raiz: popear cerraria la app
        SelloCreador,            // el `deshabilitar` del guion (`D-47`)
        SelloMotorCinematica,    // bloque no-saltable activo
        SelloMotorModal,         // enlace-OK requerido activo
        RolMenuSuprime,          // quicksave/quickload dentro de un menu
    };

    // Los cuatro bools van en un struct A PROPOSITO. Sueltos en la firma, dos
    // intercambiados **compilan en silencio** y el harness los probaria en el
    // orden equivocado con la misma cara de verde.
    struct VNAtajoContexto {
        bool         rolMenu          = false;  // `m_role == VNRole::Menu`
        bool         puedePopear      = false;  // `m_stateStack->size() > 1`
        // ── SE LLAMABA `modalRequerido`, Y EL NOMBRE ERA EL BUG ─────────────
        //
        // Era `isModalRequired()` a secas, y cuando esta ley se escribio
        // (`CP1b`, 2026-08-06) ese flag tenia UN SOLO dueno: el `enlace ...
        // requerido` de `modalidad-aviso`, que SI quiere sellar save/load
        // porque el aviso es la unica salida.
        //
        // Nueve dias despues, `CP2a` le puso un SEGUNDO dueno: el pool
        // `palabras` prende el mismo flag ([VNScriptState.h:3839]) para otra
        // cosa -sellar el avance plano y hacer que le llegue el anillo de
        // foco- y se llevo el sello de save/load **de regalo, sin que nadie
        // lo firmara**. El juego de poemas quedo prohibiendo guardar por
        // efecto lateral, con un aviso que nombraba la feature equivocada.
        //
        // **Y el guardian de no-op de `harness_ruta` §9 no podia verlo:** ese
        // barrido le pone el bit A MANO y exige el efecto, o sea que custodia
        // que hace la ley CON el flag, nunca QUIEN lo prende. Un guardian que
        // barre un flag no custodia a sus duenos.
        //
        // El campo nombra ahora lo que de verdad decide. Quien lo prende es
        // cable, y lo custodia `correr_probe_save_en_pool.ps1`.
        bool         avisoRequerido   = false;  // SOLO `modalidad-aviso`
        bool         cinematicaActive = false;  // `m_cinematicaActive`
        VNActionMask sellada          = 0u;     // `m_accionesSelladas`
    };

    struct VNAtajo {
        VNAtajoEfecto efecto  = VNAtajoEfecto::Ninguno;
        VNAtajoCausa  causa   = VNAtajoCausa::Ninguna;
        bool          consume = false;
        // Solo significa algo con `SelloCreador` o `RolMenuSuprime`: es el
        // verbo que la voz tiene que nombrar.
        VNAction      sujeto  = VNAction::Menu;
    };

    // El orden de los bloques ES la ley: replica el de la cascada de `SDLK_*`
    // de la seccion 2, y cada bloque lleva pegado el suyo.
    inline VNAtajo vnAtajoDeVerbos(const std::vector<std::string>& verbos,
                                   const VNAtajoContexto& ctx)
    {
        // Ley A - el `ESC`. Era `if (k == SDLK_ESCAPE)`, y va PRIMERO igual que
        //   antes: corre por encima del sello del motor, que no lo toca (el
        //   comentario del sitio lo declara: "ESC NO se sella aqui").
        //   **El rol se pregunta antes que el sello**, o la mitad Menu se
        //   sellaria y encerraria al jugador.
        if (vnHayVerbo(verbos, "menu"))
        {
            if (ctx.rolMenu)
            {
                if (ctx.puedePopear)
                    return { VNAtajoEfecto::Popear,  VNAtajoCausa::Ninguna,
                             true, VNAction::Menu };
                return     { VNAtajoEfecto::Ninguno, VNAtajoCausa::MenuRaiz,
                             true, VNAction::Menu };
            }
            if (!vnActionAllowed(ctx.sellada, VNAction::Menu))
                return { VNAtajoEfecto::Ninguno, VNAtajoCausa::SelloCreador,
                         true, VNAction::Menu };
            return     { VNAtajoEfecto::AbrirPausa, VNAtajoCausa::Ninguna,
                         true, VNAction::Menu };
        }

        // Ley B - EL SELLO DEL MOTOR, y es el cuerpo entero de la ficha
        //   `SELLO-MOTOR-SOBRE-TECLA`. Era
        //   `(isModalRequired() || m_cinematicaActive) &&
        //    (k == SDLK_k || k == SDLK_l || k == SDLK_F5 || k == SDLK_F9)`,
        //   o sea Capa 2 escrita sobre CUATRO KEYCODES: el dia que `CP4` deje
        //   remapear `guardado_rapido` a `N~`, ese sello quedaba cuidando una
        //   puerta que el jugador ya movio. Ahora nombra los cuatro VERBOS, y
        //   remapear no lo des-sella.
        //
        //   Cuando las dos condiciones son ciertas gana la voz de la
        //   cinematica, igual que el ternario del sitio.
        const bool esSaveLoad =
            vnHayVerbo(verbos, "guardado_rapido") ||
            vnHayVerbo(verbos, "carga_rapida")    ||
            vnHayVerbo(verbos, "menu_guardar")    ||
            vnHayVerbo(verbos, "menu_cargar");

        if (esSaveLoad && (ctx.avisoRequerido || ctx.cinematicaActive))
            return { VNAtajoEfecto::Ninguno,
                     ctx.cinematicaActive ? VNAtajoCausa::SelloMotorCinematica
                                          : VNAtajoCausa::SelloMotorModal,
                     true, VNAction::Menu };

        // Ley C - `K`. El sello del creador va ANTES de la guarda de rol, y el
        //   orden esta firmado en el sitio (`D-45`): las dos pueden ser ciertas
        //   y "lo apago tu guion" le sirve mas al creador que "el rol no lo
        //   permite".
        if (vnHayVerbo(verbos, "guardado_rapido"))
        {
            if (!vnActionAllowed(ctx.sellada, VNAction::GuardadoRapido))
                return { VNAtajoEfecto::Ninguno, VNAtajoCausa::SelloCreador,
                         true, VNAction::GuardadoRapido };
            if (ctx.rolMenu)
                return { VNAtajoEfecto::Ninguno, VNAtajoCausa::RolMenuSuprime,
                         true, VNAction::GuardadoRapido };
            return     { VNAtajoEfecto::GuardarRapido, VNAtajoCausa::Ninguna,
                         true, VNAction::GuardadoRapido };
        }

        // Ley D - `L`. Calcada de la C.
        if (vnHayVerbo(verbos, "carga_rapida"))
        {
            if (!vnActionAllowed(ctx.sellada, VNAction::CargaRapida))
                return { VNAtajoEfecto::Ninguno, VNAtajoCausa::SelloCreador,
                         true, VNAction::CargaRapida };
            if (ctx.rolMenu)
                return { VNAtajoEfecto::Ninguno, VNAtajoCausa::RolMenuSuprime,
                         true, VNAction::CargaRapida };
            return     { VNAtajoEfecto::CargarRapido, VNAtajoCausa::Ninguna,
                         true, VNAction::CargaRapida };
        }

        // Ley E - `F5`. **Sin guarda de rol, y no es un olvido**: los menus de
        //   guardado SI se abren en rol Menu; lo que `D-45` suprime ahi es el
        //   quicksave/quickload posicionado, no el menu. La §9 del harness lo
        //   barre en las dos direcciones.
        if (vnHayVerbo(verbos, "menu_guardar"))
        {
            if (!vnActionAllowed(ctx.sellada, VNAction::MenuGuardar))
                return { VNAtajoEfecto::Ninguno, VNAtajoCausa::SelloCreador,
                         true, VNAction::MenuGuardar };
            return     { VNAtajoEfecto::AbrirMenuGuardar, VNAtajoCausa::Ninguna,
                         true, VNAction::MenuGuardar };
        }

        // Ley F - `F9`. Calcada de la E.
        if (vnHayVerbo(verbos, "menu_cargar"))
        {
            if (!vnActionAllowed(ctx.sellada, VNAction::MenuCargar))
                return { VNAtajoEfecto::Ninguno, VNAtajoCausa::SelloCreador,
                         true, VNAction::MenuCargar };
            return     { VNAtajoEfecto::AbrirMenuCargar, VNAtajoCausa::Ninguna,
                         true, VNAction::MenuCargar };
        }

        // Ley G - no era mio, y **NO CONSUME**. Es la diferencia que mantiene
        //   vivos a los hotspots de `esperar_click` (seccion 3) y al `F3`, que
        //   por fork 3b sigue hardcodeado: es dominio `dev` y el VN resuelve
        //   uno solo.
        return {};
    }

    // La otra mitad: el TEXTO. Vive aparte de la ley para que la ley decida y
    // esta hable -y para que el harness pueda afirmar las dos por separado-.
    // Devuelve la voz SIN el salto de linea, igual que `vnVozSelloVeta`: el
    // `"\n"` lo pone el call-site, que es como ya se escribe en produccion.
    inline std::string vnVozDeAtajo(VNAtajoCausa causa, VNAction sujeto)
    {
        switch (causa)
        {
        case VNAtajoCausa::MenuRaiz:
            return "[VN rol Menu] ESC en menu raiz: sin accion "
                   "(popear cerraria la app).";
        case VNAtajoCausa::SelloCreador:
            return vnVozSelloVeta(vnNombreDeAccion(sujeto));
        case VNAtajoCausa::SelloMotorCinematica:
            return "[cinematica] atajo save/load sellado "
                   "(bloque no-saltable activo).";
        case VNAtajoCausa::SelloMotorModal:
            return "[modalidad-aviso] atajo save/load sellado "
                   "(enlace requerido activo).";
        case VNAtajoCausa::RolMenuSuprime:
            return (sujeto == VNAction::GuardadoRapido)
                 ? "[VN rol Menu] quicksave (k) suprimido."
                 : "[VN rol Menu] quickload (l) suprimido.";
        default:
            return std::string();
        }
    }

} // namespace eclips
