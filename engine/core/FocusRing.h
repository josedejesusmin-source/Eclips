// ============================================================================
// FocusRing.h - EL ANILLO DE FOCO, FASE 1: LA COLUMNA DE OPCIONES
// ============================================================================
//
// `CP5` de `DISENO_ATOMO_TECLADO.md` (§9 + §14), alcance firmado **1a**: los
// CHOICES. Los `enlace`, las `ranura` y los `control` son de `CP6`.
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// Los cuatro `foco_*` estan en la tabla del `CP0` desde el 2026-08-05 y no los
// despacha nadie. Lo que faltaba nunca fue el binding: es el CONSUMIDOR. Este
// header es la ley de ese consumidor, y es pura - no toca SDL, no conoce rects,
// no sabe que existe una caja de dialogo.
//
// **El caller real, medido antes de escribir una linea:** el menu de arranque
// del demo (`vnscript/test_confirmar_salir.ecl:45`) tiene CINCO opciones, dos
// de ellas tras un `[si: _saves > 0]`. Con un save en disco, `Salir` es la
// quinta - y `opcion_1..4` llega hasta la cuarta. **Hoy esa opcion es
// mouse-only.** Era uno de **tres** bloques del corpus que pasaban de cuatro
// opciones **antes de este lote** (66 bloques `elegir` en total, medidos el
// 2026-08-07), y el unico que el jugador ve al arrancar.
//
// *El numero lleva su fecha y su "antes" porque **el feel-test de este lote lo
// movio**: sus seis bloques de cinco opciones dejan el corpus en 73/9. Es la
// sonda contandose a si misma, igual que `medir_apariciones` - y la unica forma
// de que la cifra siga significando algo es decir cual de las dos es.*
//
// ── POR QUE UNA LISTA Y NO LA GEOMETRIA QUE EL §9 FIRMA ─────────────────────
//
// *(Escrito el 2026-08-07 por `CP5`, y sigue siendo la razon de la lista. Lo
// que ese dia se supuso sobre `CP6` -que escribiria la geometria- lo desmintio
// la medicion del 2026-08-08; ver el bloque siguiente.)*
//
// La §9 escribe *"el candidato en la direccion pedida con mayor solapamiento en
// el eje perpendicular y menor distancia"*. Medido sobre los choices
// (`VNDialogBox::getChoiceRects`, `:1409-1421`): los rects comparten X y ANCHO y
// se apilan con un gap uniforme, o sea que son una COLUMNA. Sobre una columna
// esa ley da **exactamente** el vecino de arriba/abajo, asi que la fase 1
// escribe la lista -la misma ley, expresada donde no sobra nada-.
//
// ── Y POR QUE `CP6` TAMPOCO ESCRIBIO LA GEOMETRIA (2026-08-08) ──────────────
//
// `CP5` dejo escrito que `CP6` la escribiria "cuando llegue su primer caller
// posicionado libre". El caller llego -`config.ecl`, la aceptacion firmada de
// `CP6`- y lo que la medicion mostro es que bajo la ley geometrica del §9 ese
// caller queda ROTO. Los rects reales (canvas 1920x1080):
//
//   musica_volumen / sonido_volumen / velocidad_texto   X 140..560
//   pantalla_completa / skip_no_visto                   X 140..540
//   resolucion                                          X 760..1360
//
// Solapamiento en X entre `resolucion` y la columna: **cero**. La ley pide
// "mayor solapamiento en el eje perpendicular" y no hay ninguno, asi que
// ARRIBA/ABAJO no pueden alcanzarlo NUNCA. Y DERECHA si lo alcanzaria -se
// solapan 10px en Y- salvo que el §9 le da izq/der al `control` focado, y el
// vecino ES un control. **`control resolucion` queda mouse-only en la pantalla
// que acepta el `CP`.**
//
// El §9 firma dos cosas que juntas no cierran, y la paridad que el propio §9
// invoca desempata: `SettingsState.h:174-195` navega con ARRIBA/ABAJO por
// **lista en orden de declaracion** (`nextUnlockedItem`) y usa izq/der para
// `adjustValue`. El menu con el que el contrato se manda a comparar no usa
// geometria. Asi que la lista se queda, extendida a las tres familias:
//
//   - es reachability-complete: un ciclo sobre todos llega a todos. La
//     geometria no lo garantiza, y el contraejemplo esta en el corpus.
//   - el orden lo AUTORA el creador, moviendo sus lineas en el `.ecl`. Con
//     geometria el anillo dependeria de los `x=`/`y=` que el creador toca para
//     que se vea bonito.
//
// La geometria del §9 queda fichada, con gatillo: el primer caller con layout
// 2D que pida navegacion espacial de verdad. Hoy no hay ninguno, y §7.118 dice
// que un caller hipotetico no es una demanda.
//
// ── QUE HACEN IZQUIERDA Y DERECHA ───────────────────────────────────────────
//
// **No mueven: AJUSTAN** el valor del `control` focado (`ControlAdjust.h`). Es
// literal lo que la etiqueta de vencimiento de `harness_ruta` 13.6 decia desde
// `CP5` -*"el §9 se las reserva para AJUSTAR el valor de un `control` focado"*-,
// asi que ese guardian se invierte hacia donde ya apuntaba. Darles el
// significado "moverse" les pondria el dueno equivocado, que es §7.449 sobre un
// verbo en vez de sobre una clave.
//
// Por eso `focoMover` las trata como NO-OP explicito en vez de dejarlas caer al
// ternario: sin esa guarda, un `Izquierda` que se filtrara al anillo se
// comportaria como `Arriba` -compila, camina, y mueve para el lado equivocado-.
//
// ── EL ANILLO ENVUELVE, Y ESO ES PRECEDENTE MEDIDO ──────────────────────────
//
// No es gusto: los tres menus C++ del motor lo hacen con la misma linea
// -`(cursor +- 1 + n) % n` en `MainMenuState.h:192`, `PauseMenuState.h:201`,
// `SettingsState.h:176`- y los dos anillos horneados del VN tambien (`1 -
// cursor`, que sobre dos elementos ES el modulo). **Cinco sitios, una sola
// conducta.** Un anillo nuevo que no envolviera seria el unico raro del motor.
//
// ── LA CONVERGENCIA ES LA MITAD CARA (§9: "el MISMO indice") ────────────────
//
// El §9 pide que el hover del mouse y el foco del teclado sean **un solo
// indice**, y acerto por una razon que no escribio: sobre `m_choiceHover` ya
// cuelgan TRES consumidores que el teclado hereda gratis -el pintado y el juice
// de escala (`VNDialogBox.cpp:1075` / `:317`), el SONIDO de hover por flanco
// (`VNScriptState.h:1489`) y el despertador del skip-frame
// (`VNScriptState.h:1208`)-. Dos indices habrian obligado a cablear los tres
// otra vez, y a que se desincronizaran despues.
//
// Lo que el §9 no podia saber es que ese indice **se re-deriva del mouse cada
// frame**: el state polea `SDL_GetMouseState` en su `update`
// (`VNScriptState.h:1420`) y la caja recalcula el hover desde ahi
// (`VNDialogBox.cpp:291`). Es PULL, no push - su hermano `m_hoveredSlot` se
// declara "calco de m_choiceHover" (`VNScriptState.h:852`) y es justo la mitad
// que no copio. Un foco escrito por teclado se pisa **al frame siguiente**, y
// por eso la ley 2 existe: el pull pasa a ser condicional.
//
//   el puntero manda  ->  el indice es el que esta bajo el puntero (o -1)
//   el teclado mando  ->  el indice se queda quieto hasta el proximo evento
//                         de puntero
//
// **Y "evento de puntero" incluye el CLICK, no solo el movimiento.** Sin eso, un
// click en el vacio con un foco encendido ELEGIRIA la opcion focada en vez de
// completar el caption, que es la regresion de `D-36` (`elegir-beat-lote`,
// v0.8.30) sobre una feature que ya shipeo. La mide el control negativo (§6.2).
//
// ── LA LEY 3 NO ES NUEVA: ES EL HIT-TEST DE HOY, LEIDO DESDE EL INDICE ──────
//
// Con las leyes 1 y 2 puestas, el confirm se simplifica a *"activa el indice si
// hay uno"*, y eso es **byte-identico** al hit-test que
// `VNDialogBox::handleAction` hace hoy (`:1494`) mientras el teclado no toque el
// anillo: si el puntero manda, el indice ES lo que ese hit-test devolveria. El
// guardian de no-op (§1) lo barre por todas las posiciones del mouse contra la
// cascada vieja replicada.
//
// ── LO QUE NO PUEDE VER ─────────────────────────────────────────────────────
//
// El CABLE, y son CUATRO: que `vnAccionDeVerbos` despache los dos verbos, que la
// caja llame a `focoMover` y deje de pisar el indice, que `handleAction`
// pregunte por la ley 3, y que el state avise del puntero. Las dos TUs son el
// header gigante y `VNDialogBox.cpp` (que arrastra GL) - misma pared que
// `MERGE-DUPLICADO`. Lo cubre `test_foco_feel.ecl`, que lleva **cinco opciones
// siempre** — justamente para NO depender de que quien lo corra tenga un save.
// Esa dependencia es del MENU REAL, no del feel-test: sobre
// `test_confirmar_salir.ecl` con la partida vacia hay tres opciones y las tres
// ya tienen tecla, asi que ahi el verde no significaria nada (§7.485).
// ============================================================================

#pragma once

namespace eclips {

    // `CP5` nacio con dos valores; `CP6` (2026-08-08) agrego los otros dos, y
    // **no con la geometria sino como no-op del anillo**: izq/der ajustan el
    // valor de un `control` focado, no mueven el foco. El enum crecio, no se
    // renombro - los dos primeros conservan su orden y su significado.
    enum class FocoDir : unsigned char { Arriba, Abajo, Izquierda, Derecha };

    // ── LEY 1 - EL MOVIMIENTO ───────────────────────────────────────────────
    //
    // `actual` puede ser -1 ("nadie focado"), que es como nace cada eleccion
    // (fork 4a): quien no toca las flechas ve la conducta de siempre, y el
    // motor no le pre-elige una opcion que el creador no pidio.
    //
    // Entrar desde -1 no es un caso raro, es EL caso: `Abajo` entra por el
    // primero y `Arriba` por el ultimo, o sea que el cursor llega desde afuera
    // en la direccion en que lo empujaste.
    //
    // Un `actual` fuera de rango se trata como -1 en vez de clampearse: si la
    // lista se encogio debajo del anillo, clampear dejaria el foco parado sobre
    // una opcion que el jugador nunca eligio.
    inline int focoMover(int actual, int n, FocoDir dir)
    {
        if (n <= 0) return -1;
        // `CP6`: izq/der NO son del anillo. Va DESPUES del `n <= 0` para no
        // tocar esa ley (sin focables el indice es -1, venga de donde venga) y
        // ANTES del ternario, que es lo que las haria comportarse como `Arriba`.
        if (dir == FocoDir::Izquierda || dir == FocoDir::Derecha) return actual;
        if (actual < 0 || actual >= n)
            return (dir == FocoDir::Abajo) ? 0 : (n - 1);
        return (dir == FocoDir::Abajo) ? ((actual + 1) % n)
                                       : ((actual - 1 + n) % n);
    }

    // ── LEY 2 - LA CONVERGENCIA ─────────────────────────────────────────────
    //
    // `bajoPuntero` es -1 cuando el puntero no esta sobre ninguna opcion, y ese
    // -1 **se propaga**: el mouse saliendo de las opciones apaga el foco igual
    // que hoy apaga el hover. Lo que NO puede pasar es que apague un foco que
    // escribio el teclado, y de eso se ocupa `punteroManda`.
    inline int focoConverger(int actual, bool punteroManda, int bajoPuntero)
    {
        return punteroManda ? bajoPuntero : actual;
    }

    // ── LEY 3 - EL CONFIRM ──────────────────────────────────────────────────
    enum class FocoConfirm : unsigned char {
        Nada,               // consumir y no hacer nada (la conducta de hoy)
        Activar,            // elegir `indice`
        CompletarCaption,   // `D-36`: adelantar el texto que sigue escribiendose
    };

    struct FocoVeredicto {
        FocoConfirm que    = FocoConfirm::Nada;
        int         indice = -1;   // solo significa algo con `Activar`
    };

    // El orden ES la ley, y replica el de `handleAction` (`:1494-1528`): primero
    // la opcion, despues el caption, y **consumir siempre**. Ese consumir-igual
    // no es descuido: un `Confirm` que cayera de largo durante una eleccion
    // llegaria al avance generico y adelantaria el guion por debajo del `elegir`.
    inline FocoVeredicto focoConfirmar(int foco, bool cajaSobrevive,
                                       bool captionAMedias)
    {
        if (foco >= 0) return { FocoConfirm::Activar, foco };
        if (cajaSobrevive && captionAMedias)
            return { FocoConfirm::CompletarCaption, -1 };
        return {};
    }

    // ── LEY 4 (`CP6`) - LA LISTA CRUZA FAMILIAS ─────────────────────────────
    //
    // El §9 dice *"el state construye la lista de focables del momento"*, y
    // `CP5` dejo anotado que las familias "nunca compiten, decidido por fase".
    // **Medido el 2026-08-08, eso vale para UNA de las tres.** `linksFrozen`
    // (`VNScriptState.h:1706`) si congela los `enlace` durante `WaitingChoice`,
    // pero el `control` y la `ranura` son phase-free A PROPOSITO
    // (`VNScriptState.h:892`: *"SIN gate de fase... deja que los sliders
    // sobrevivan el hold natural del menu"*), y las tres pantallas de la
    // aceptacion son justo el caso donde conviven:
    //
    //   config.ecl   WaitingChoice   6 `control` + 1 choice
    //   load.ecl     WaitingChoice   1 `ranura`  + 1 choice
    //   pausa.ecl    WaitingChoice   4 `ranura`  + 4 choices   <- la pausa real
    //
    // Asi que `CP6` no es "mover donde vive el indice": es UNA lista que cruza
    // familias, y el anillo de `CP5` pasa a ser un SEGMENTO de ella.
    // > ⚠ **`CP9b` (2026-08-12) suma la QUINTA familia: `Tecla`.** Es el widget
    // > `control tecla.<dominio>.<accion>`, y entra como familia propia y no
    // > como un `Control` mas por lo que `L-5` del contrato midio: un
    // > `BoundControl` es un float con `min/max/paso` atado a un `CtrlDesc*` del
    // > catalogo Tier-A, y una tecla es una lista de strings sin modelo
    // > numerico. Es el mismo caso que `ranura` -*"aqui ni el modelo
    // > coincide"*- y se resuelve igual: familia paralela.
    // >
    // > **Va segunda, pegada a `Control`, y el no-op sale por construccion:**
    // > hoy no hay un solo `.ecl` con un `control tecla.*`, asi que `c.teclas`
    // > vale 0 en las tres pantallas de la aceptacion y el indice plano de las
    // > otras cuatro familias no se mueve una posicion. Lo custodia
    // > `harness_foco` §8.
    enum class FocoFamilia : unsigned char { Control, Tecla, Ranura, Enlace, Choice };

    struct Focable {
        FocoFamilia familia = FocoFamilia::Choice;
        int         indice  = -1;   // indice DENTRO de su familia; -1 = ninguno
    };

    struct FocoCuentas {
        int controles = 0;
        int teclas    = 0;
        int ranuras   = 0;
        int enlaces   = 0;
        int choices   = 0;
    };

    inline int focoTotal(const FocoCuentas& c)
    {
        return c.controles + c.teclas + c.ranuras + c.enlaces + c.choices;
    }

    // El ORDEN es la ley, y es el de DECLARACION del `.ecl`, no el geometrico.
    //
    // **Los choices van ULTIMOS por construccion, no por gusto:** el `elegir`
    // es un HOLD -parquea la escena en `WaitingChoice`- asi que todo lo que el
    // creador escriba despues de el no llega a registrarse mientras esa
    // pantalla vive. Un `control` declarado bajo un `elegir` no existe. Las
    // tres pantallas reales lo confirman: las seis del `config`, la del `load`
    // y las cuatro de la `pausa` estan todas ARRIBA de su bloque.
    //
    // Entre `enlace` y `choice` el orden relativo es INOBSERVABLE -no coexisten
    // nunca: en `WaitingChoice` los enlaces estan congelados, y bajo
    // `modalidad-aviso` (la unica fase donde `CP6` los focaliza) no hay
    // choices-. Se elige uno y se declara que no se puede medir, en vez de
    // dejar que parezca una decision.
    inline Focable focoFocable(const FocoCuentas& c, int indice)
    {
        if (indice < 0) return {};
        int i = indice;
        if (i < c.controles) return { FocoFamilia::Control, i };
        i -= c.controles;
        if (i < c.teclas)    return { FocoFamilia::Tecla,   i };
        i -= c.teclas;
        if (i < c.ranuras)   return { FocoFamilia::Ranura,  i };
        i -= c.ranuras;
        if (i < c.enlaces)   return { FocoFamilia::Enlace,  i };
        i -= c.enlaces;
        if (i < c.choices)   return { FocoFamilia::Choice,  i };
        return {};   // fuera de la lista
    }

    // La inversa, para que la CONVERGENCIA pueda hablar: cada familia tiene su
    // propio hit-test vivo (`hitTestControl`, `hitTestSlotWidget`,
    // `hoveredInteractive`) y el puntero contesta en coordenadas de familia.
    // Sin esto habria que replicar geometria aca, que es justo lo que la ley
    // pura no hace.
    inline int focoIndicePlano(const FocoCuentas& c, FocoFamilia fam, int idx)
    {
        if (idx < 0) return -1;
        switch (fam) {
        case FocoFamilia::Control: return (idx < c.controles) ? idx : -1;
        case FocoFamilia::Tecla:   return (idx < c.teclas)
                                        ? c.controles + idx : -1;
        case FocoFamilia::Ranura:  return (idx < c.ranuras)
                                        ? c.controles + c.teclas + idx : -1;
        case FocoFamilia::Enlace:  return (idx < c.enlaces)
                                        ? c.controles + c.teclas + c.ranuras + idx : -1;
        case FocoFamilia::Choice:  return (idx < c.choices)
                                        ? c.controles + c.teclas + c.ranuras
                                          + c.enlaces + idx
                                        : -1;
        }
        return -1;
    }

} // namespace eclips
