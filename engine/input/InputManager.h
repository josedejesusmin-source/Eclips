// ============================================================================
// InputManager.h - LA CAPA 1: BINDING (tecla -> verbo)
// ============================================================================
//
// CP0 del contrato `DISENO_ATOMO_TECLADO.md` (plan Opus §14).
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// El motor pregunta hoy por TECLAS. `SDLK_k` significa "guardar" en 145 sitios
// repartidos entre siete duenos, y esa igualdad no esta escrita en ningun lado:
// esta tipeada. Este header es la tabla que la escribe, y es la Capa 1 del
// modelo de tres capas del contrato (§3):
//
//     Capa 1  BINDING    tecla -> verbo        <- ESTE ARCHIVO
//     Capa 2  CONTEXTO   permitido(verbo)?     <- `VNActionSeal.h` + las guardas
//     Capa 3  DESPACHO   el switch del state
//
// El manager TRADUCE. No despacha, no conoce fases, no consume nada: devolver
// el verbo es todo su trabajo (`F1` = 1b del contrato). Por eso `resolver` es
// `const` y puro - quien decide si el evento se come es el dueno del momento.
//
// ── EL STUB CUMPLIO SU DESTINO ──────────────────────────────────────────────
//
// Este archivo tenia 42 lineas y decia, desde la Fase 1 historica: *"El codigo
// del juego pregunta por 'confirmar', 'saltar', 'mover_arriba'. NUNCA por
// teclas especificas."* Era la filosofia exacta de este contrato, escrita mucho
// antes y con todos sus metodos devolviendo `false`. El contrato lo ficho como
// `H1` y lo llamo "la segunda vez que el mapa miente con el mismo truco" (la
// primera fue `Renderer`). La API vieja (`pressed/held/released/bind`) no
// sobrevive: nunca tuvo un caller, y la firma que si tiene contrato es la de
// §3.1.
//
// ── LO QUE ESTE HEADER *NO* HACE, Y POR QUE ─────────────────────────────────
//
// `held()`, `eje()`, `loadPlayerLayer()` y `rebindPlayer()` estan firmados en
// §3.1 del contrato y **no se construyen en CP0**. No es olvido: es §7.118. Sus
// callers nacen en CP1 (el skip poleado), CP2 (los ejes de gameplay) y CP4 (la
// capa del jugador), y las tres piden FUNCIONES de SDL - `SDL_GetKeyboardState`,
// `SDL_GetScancodeFromKey` -, que obligarian a LINKEAR SDL en el harness. Hoy
// este archivo solo necesita los HEADERS de SDL (structs y constantes), asi que
// la bateria lo cubre entero sin una excepcion en el `.bat`. **Cuando `held`
// llegue con su caller, se lleva su seccion del harness.**
//
// > ⚠ **`held()` y `eje()` LLEGARON con `CP2` (2026-08-06), asi que la lista de
// > arriba quedo con dos de cuatro.** `loadPlayerLayer()` y `rebindPlayer()`
// > siguen sin construirse, y por la razon original: sus callers son de `CP4`.
// >
// > **Y al 2026-08-07 (`CP4`) queda UNA, con una correccion de forma:**
// > `rebindPlayer()` sigue sin construirse —su caller es el widget
// > `control tipo=tecla`, o sea `CP9`— pero la CAPACIDAD de `loadPlayerLayer()`
// > ya existe **y no como metodo de esta clase**: la §3.1 la firma abriendo el
// > archivo, y este header no abre archivos. Se partio igual que `held`/`eje`:
// > ley pura aca (`inputParsearCapaJugador`), transporte en `Settings` y cable
// > en `Engine::aplicarTeclasDelJugador`. Enmienda `G-2` del contrato.
// >
// > **Y la premisa que se cayo fue la del medio.** El parrafo da por hecho que
// > construirlas obliga a linkear SDL en el harness — y eso valia mientras la
// > funcion fuera *una sola cosa*. `CP2` la partio (fork 3b del fundador): la
// > **LEY** (`inputHeldDeTabla` / `inputEjeDeTabla`, abajo de la clase) toma el
// > predicado como parametro y es pura; el **CABLE** que lee el teclado de SDL
// > vive en `InputManager.cpp`. La bateria sigue sin linkear SDL, y ademas gano
// > cobertura: `harness_juego` §6 prueba el eje **incluido su signo**, que es la
// > mitad que compila y camina al reves.
// >
// > La ultima frase igual se cumplio, y al pie de la letra: `held` llego con su
// > caller y **se llevo su seccion del harness** - solo que a `harness_juego`,
// > que es el del dominio donde vive ese caller, y no a `harness_teclas`.
//
// > ⚠ **CERRADO el 2026-08-12 (`CP9b`): la lista de cuatro esta COMPLETA.** El
// > rebind llego con su caller —el widget `control tecla.<dominio>.<accion>`—,
// > y se partio igual que sus tres hermanos: **LEY** aca
// > (`inputRebindJugador`, al lado de su inversa `inputParsearCapaJugador`,
// > porque las dos tienen que saber los mismos prefijos) y **CABLE** en
// > `VNScriptState`. `harness_teclas` §19-§21.
// >
// > **Dos correcciones de forma a lo que este bloque venia diciendo**, y las dos
// > salieron de implementarlo: el caller **no** se llama `control tipo=tecla`
// > —esa clave estaba ocupada por el override de render del propio `control`, y
// > `accion=` por `ranura`—, y la firma **no** es `rebindPlayer(verbo, dominio,
// > teclas)` sino una funcion libre sobre los pares crudos, porque lo que hay
// > que decidir no es *"que tecla"* sino **cual LINEA del archivo se toca**
// > (§7.499).
// >
// > **Y la regla que este bloque se escribio a si mismo se cumplio por CUARTA
// > vez seguida** —*"decilo como fecha, no como estado"*—: cada vez que alguien
// > enumero aca lo que el header *no* hace, enumero exactamente lo que el
// > proximo CP iba a agregar. Con la lista cerrada, la deuda se termino: **no
// > queda nada firmado por la §3.1 sin construir.**
//
// Tampoco vive aca el bloque `teclas {}` del creador (CP3) ni `tecla.*` de
// `settings.eclips` (CP4). Lo que SI vive es **el hueco donde se enchufan**: las
// dos capas de override existen, estan vacias, y `aplicarCascada` ya sabe
// mezclarlas. Un CP que solo tiene que llenar un `std::map` es un CP barato.
//
// > ⚠ **La primera mitad VENCIO en `CP3` (2026-08-06): el parser del bloque
// > `teclas {}` vive aca**, 500 lineas mas abajo (`inputParsearCapaCreador` y
// > compania). Sigue siendo cierto que `tecla.*` de `settings.eclips` no vive
// > aca - eso es `CP4` -, y sigue siendo cierto que el CP fue barato.
// >
// > **Y esta es la SEGUNDA vez seguida que este bloque miente por la misma
// > razon** (la anterior fue el `held()`/`eje()` de `CP2`, arriba). No es mala
// > suerte: es la forma del parrafo. Una seccion titulada *"lo que este header
// > NO hace"* enumera **exactamente lo que el proximo CP va a agregar**, asi que
// > se vence por diseno, un lote a la vez. Quien escriba la proxima: **decilo
// > como fecha, no como estado** — *"esto llega en CP4"* envejece; *"esto no
// > existe"* se vuelve falso.
//
// > ⚠ **TERCERA vez seguida, 2026-08-07 (`CP4`), y el sello de arriba se cayo
// > POR SU PROPIA REGLA.** Escribio *"sigue siendo cierto que `tecla.*` de
// > `settings.eclips` no vive aca - eso es `CP4`"*, que es un ESTADO con fecha
// > de vencimiento pegada al lado, y `CP4` fue el dia siguiente:
// > `inputPartirClaveJugador` e `inputParsearCapaJugador` viven aca abajo.
// >
// > **Escrito como la regla pide, o sea como fecha:** al 2026-08-07 este header
// > tiene las TRES capas de la cascada — el default, el parser del bloque del
// > creador (`CP3`) y el partidor de la clave del jugador (`CP4`). Lo que no
// > vive aca es **abrir archivos**: ninguna de las tres lee un byte de disco, y
// > de eso depende que la bateria afirme las voces sin cazar stderr.
//
// ── EL VOCABULARIO DE VERBOS NO ES DE ESTE ARCHIVO ──────────────────────────
//
// Los nombres de los verbos del dominio `vn` **ya viven en
// `engine/core/VNActionSeal.h`**, escritos, bilingues y con su inversa, porque
// el lote `deshabilitar-acciones` los necesito un mes antes que este. Copiarlos
// aca seria exactamente §7.442: dos tablas del mismo vocabulario se
// desincronizan, y la que se desincroniza es la que nadie mira.
//
// Este header guarda **datos**: que teclas trae cada verbo. Los NOMBRES de los
// verbos son strings, y el guardian de que las dos tablas siguen de acuerdo es
// un assert del harness, no un `#include` que ate una capa a la otra. Se eligio
// el assert y no la fusion porque el manager tiene TRES dominios y el sello
// habla de UNO: fusionarlos volveria mentiroso el titulo de `VNActionSeal.h`,
// que es §7.353 - la ficha que ese mismo lote ya pago.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   OFF-POR-IDENTIDAD. La tabla default ES el censo de hoy. Con las dos
//           capas de override vacias -o sea, en todo proyecto que exista hoy-
//           resuelve exactamente lo que los `SDLK_*` crudos resuelven. Si esta
//           ley cae, CP0 no agrego una capa: le cambio las teclas al motor
//   ley 2   UNA TABLA, DOS DIRECCIONES (§7.205). `inputBindDe` y `inputNombreDe`
//           son dos caras de la MISMA estructura. Dos tablas divergen; una no
//           puede. El display de `${_tecla_*}` (CP9) es la cara que todavia no
//           tiene consumidor, y por eso el harness la corre de ida y vuelta
//   ley 3   NOMBRAR NO ES LIGAR. La tabla de NOMBRES es ancha (F1-F12, flechas,
//           REPAG/AVPAG, SUPR, mouse, ATRAS); la tabla DEFAULT es exactamente la
//           conducta de hoy. Ver el bloque de abajo.
//
//           > ⚠ **De las TRES que `CP0` dejo nombradas-y-no-ligadas, `CP7`
//           > (2026-08-10) ligo DOS** -`ENTER_NUM` -> `avanzar`, `ATRAS` ->
//           > `menu` en los dos dominios-. **Queda UNA, y su razon es mas fina
//           > que la de `CP0`: `TAB` no se puede ligar a ningun verbo que
//           > EXISTA.** Sus dos sitios vivos significan *"alternar entre DOS
//           > botones horizontales"*, que `CP6` expreso como
//           > `foco_izquierda`/`foco_derecha`; la convencion universal de TAB
//           > es *"el siguiente"*, que en el anillo es `foco_abajo`. Y las dos
//           > lecturas se rompen entre si: con `foco_derecha`, TAB **moveria el
//           > slider focado** en `config.ecl` (izq/der AJUSTAN el control,
//           > `I-4`); con `foco_abajo`, el modal de display deja de alternar su
//           > cursor, porque ese bloque solo pregunta por izq/der.
//           >
//           > Es §2.9 cuarta pregunta sobre un VERBO en vez de una clave: el
//           > nombre esta libre y aun asi **no esta disponible**. Ficha
//           > `TAB-SIN-VERBO`, gatillo: el `CP` que le de al anillo un concepto
//           > de *"siguiente focable"* distinto de *"el de abajo"*.
//   ley 4   MERGE POR-ACCION (`F3`). Un verbo presente en una capa REEMPLAZA su
//           lista entera; ausente, hereda. `ninguna`/`none` desliga. No se
//           mezclan listas: si el creador escribe `interactuar: X`, el default
//           `E, ENTER` se va - lo contrario haria imposible SACAR una tecla
//   ley 5   UN EVENTO PUEDE RESOLVER A VARIOS VERBOS, Y ESO ES CORRECTO.
//           `ARRIBA` es `historial_arriba` **y** `foco_arriba` en el mismo
//           dominio. El contrato lo dice en la nota de fidelidad de su §5 ("la
//           fase decide") y la API que firmo -`std::string resolve(...)`- no lo
//           puede expresar. Por eso el primitivo es `resolverTodos`, y
//           `resolver` es la comodidad para los dominios sin colision
//
// ── LAS TECLAS QUE EL CONTRATO NO TENIA (hallazgo de la auditoria) ──────────
//
// La §5 del contrato se firmo contra v0.6.27 y se declara "conducta actual,
// byte-identica". Medida contra v0.8.31 le faltan teclas VIVAS:
//
//   `ENTER_NUM` (`SDLK_KP_ENTER`)  3 sitios, sinonimo de ENTER
//   `TAB`                          2 sitios, alterna el cursor de los modales
//   `ATRAS`     (`SDLK_AC_BACK`)   3 sitios, sinonimo de ESC
//   `SHIFT_DER` / `CTRL_DER`       el codigo vivo mira LSHIFT **o** RSHIFT y
//                                  LCTRL **o** RCTRL; la §5 escribe `SHIFT` y
//                                  `CTRL` a secas, que son solo los izquierdos
//
// **Las dos ultimas entran al default**, porque no ponerlas seria perder una
// tecla que hoy funciona. **Las tres primeras no**: sus duenos son bloques que
// CP0 no toca -los dos modales de confirmacion y el editor de `entrada`-, y
// meterlas en el default seria inventar conducta. `ATRAS -> menu` haria que el
// boton Atras de Android abriera la pausa el dia que CP1 consuma la tabla, que
// es justo lo que "off-por-identidad" promete no hacer. **Firma del fundador
// (2026-08-05, salida (a)): se nombran ahora, se ligan en CP7 con su costura.**
//
// ── LO QUE ESTE HEADER NO PUEDE VER ─────────────────────────────────────────
//
// El CABLE, que es todo CP1 en adelante: que `translateSDLEventToAction` delegue
// de verdad, que los ejes de gameplay salgan de `eje()`, que el QTE consuma esta
// tabla. Lo unico que CP0 cablea es el QTE (`parseKeyName` reenvia aca), y eso
// vive en `VNScriptState.h`, que arrastra GL: el guardian de que reenvia igual
// es la §1 del harness, corriendo la ley VIEJA replicada al lado de la nueva.
//
// > ⚠ **Esa lista de tres era un PENDIENTE, y a 2026-08-06 esta completa.** El
// > QTE lo cableo `CP0`; `translateSDLEventToAction` delega desde `CP1`/`CP1b`
// > (`harness_ruta`); los ejes de gameplay salen de `eje()` desde `CP2`
// > (`harness_juego`). Sigue siendo cierto lo que el parrafo dice de FONDO —
// > **este header no puede ver ninguno de los tres**, porque los tres viven en
// > TUs que arrastran GL—; lo que cambio es que ya no son promesas. Quien
// > agregue un cable nuevo, que lo agregue a esta lista **y** le busque su
// > instrumento, porque un cable sin instrumento aca es un cable que nadie ve.
// ============================================================================

#pragma once

#include "engine/core/LangStore.h"   // DI-7: el DISPLAY sigue al jugador
#include "engine/core/EclipsParse.h" // FOLD-PARTIDO: `vocabToLower`

#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>

#include <functional>   // CP2 fork 3b: el predicado que separa la ley del cable
#include <map>
#include <string>
#include <utility>      // CP4: los pares crudos de `settings.eclips`
#include <vector>

namespace eclips
{
    // ── QUE SE PUEDE LIGAR ──────────────────────────────────────────────────
    //
    // No todo lo ligable es un `SDL_Keycode`. El contrato firmo "mouse = teclas
    // ligables" (§3.1: `CLIC`, `CLIC_DER`, `RUEDA_ARRIBA`...), y un boton de
    // mouse no tiene keycode: inventarle uno seria meter valores falsos en un
    // espacio que SDL administra. Asi que el atomo de la tabla es esto, no un
    // entero.
    struct InputBind
    {
        enum class Kind : unsigned char { Ninguna, Tecla, Boton, Rueda };

        Kind          kind  = Kind::Ninguna;
        SDL_Keycode   tecla = SDLK_UNKNOWN;   // Kind::Tecla
        unsigned char boton = 0u;             // Kind::Boton - SDL_BUTTON_*
        signed char   rueda = 0;              // Kind::Rueda - +1 arriba, -1 abajo
    };

    inline bool operator==(const InputBind& a, const InputBind& b)
    {
        if (a.kind != b.kind) return false;
        switch (a.kind) {
        case InputBind::Kind::Tecla: return a.tecla == b.tecla;
        case InputBind::Kind::Boton: return a.boton == b.boton;
        case InputBind::Kind::Rueda: return a.rueda == b.rueda;
        default:                     return true;   // Ninguna == Ninguna
        }
    }

    inline bool operator!=(const InputBind& a, const InputBind& b) { return !(a == b); }

    inline InputBind bindTecla(SDL_Keycode k)
    {
        InputBind b; b.kind = InputBind::Kind::Tecla; b.tecla = k; return b;
    }
    inline InputBind bindBoton(unsigned char btn)
    {
        InputBind b; b.kind = InputBind::Kind::Boton; b.boton = btn; return b;
    }
    inline InputBind bindRueda(signed char dir)
    {
        InputBind b; b.kind = InputBind::Kind::Rueda; b.rueda = dir; return b;
    }

    // ── LA TABLA DE NOMBRES (ley 2: una tabla, dos direcciones) ─────────────
    //
    // `es` es la grafia CANONICA - la que `inputNombreDe` devuelve y la que un
    // display le muestra al jugador. `en` es su par ingles, o `nullptr` cuando
    // la palabra es la misma en los dos idiomas (`TAB`, `F1`, `ESC`).
    //
    // `alias == true` marca las entradas que se ACEPTAN pero no se DEVUELVEN.
    // Hay tres, y las tres existen por identidad con el `parseKeyName` viejo:
    // `SHIFT`, `CTRL` y `ALT` a secas, que ese parser resolvia al modificador
    // IZQUIERDO. Sin el flag, `inputNombreDe(SDLK_LSHIFT)` podria contestar
    // `SHIFT` y perder la distincion izq/der que el default necesita.
    struct InputKeyName
    {
        const char* es;
        const char* en;      // nullptr = misma palabra en los dos idiomas
        InputBind   bind;
        bool        alias;   // se acepta al leer; nunca se devuelve al escribir
    };

    // Las letras A-Z y los digitos 0-9 NO estan en la tabla: son una REGLA (ver
    // `inputBindDe`). Meter 36 filas para expresar dos lineas de codigo es la
    // clase de tabla que despues se edita a medias.
    inline const std::vector<InputKeyName>& inputTablaNombres()
    {
        static const std::vector<InputKeyName> kTabla = {
            // ── Confirmacion / edicion ──
            { "ESPACIO",     "SPACE",        bindTecla(SDLK_SPACE),     false },
            { "ENTER",       "RETURN",       bindTecla(SDLK_RETURN),    false },
            { "ENTER_NUM",   "KP_ENTER",     bindTecla(SDLK_KP_ENTER),  false },
            { "ESC",         "ESCAPE",       bindTecla(SDLK_ESCAPE),    false },
            { "TAB",         nullptr,        bindTecla(SDLK_TAB),       false },
            { "RETROCESO",   "BACKSPACE",    bindTecla(SDLK_BACKSPACE), false },
            { "SUPR",        "DELETE",       bindTecla(SDLK_DELETE),    false },

            // ── Navegacion ──
            { "ARRIBA",      "UP",           bindTecla(SDLK_UP),        false },
            { "ABAJO",       "DOWN",         bindTecla(SDLK_DOWN),      false },
            { "IZQUIERDA",   "LEFT",         bindTecla(SDLK_LEFT),      false },
            { "DERECHA",     "RIGHT",        bindTecla(SDLK_RIGHT),     false },
            { "REPAG",       "PAGEUP",       bindTecla(SDLK_PAGEUP),    false },
            { "AVPAG",       "PAGEDOWN",     bindTecla(SDLK_PAGEDOWN),  false },
            { "INICIO",      "HOME",         bindTecla(SDLK_HOME),      false },
            { "FIN",         "END",          bindTecla(SDLK_END),       false },

            // ── Modificadores ──
            //
            // El codigo vivo mira los DOS lados (`LCTRL || RCTRL` en el skip
            // poleado, `LSHIFT || RSHIFT` en el dash), asi que los dos tienen
            // nombre propio. Las formas a secas quedan como ALIAS por identidad
            // con el parser viejo, que resolvia `SHIFT` -> `SDLK_LSHIFT`.
            { "SHIFT_IZQ",   "LSHIFT",       bindTecla(SDLK_LSHIFT),    false },
            { "SHIFT_DER",   "RSHIFT",       bindTecla(SDLK_RSHIFT),    false },
            { "SHIFT",       nullptr,        bindTecla(SDLK_LSHIFT),    true  },
            { "CTRL_IZQ",    "LCTRL",        bindTecla(SDLK_LCTRL),     false },
            { "CTRL_DER",    "RCTRL",        bindTecla(SDLK_RCTRL),     false },
            { "CTRL",        nullptr,        bindTecla(SDLK_LCTRL),     true  },
            { "ALT_IZQ",     "LALT",         bindTecla(SDLK_LALT),      false },
            { "ALT_DER",     "RALT",         bindTecla(SDLK_RALT),      false },
            { "ALT",         nullptr,        bindTecla(SDLK_LALT),      true  },

            // ── Funcion ──
            { "F1",  nullptr, bindTecla(SDLK_F1),  false },
            { "F2",  nullptr, bindTecla(SDLK_F2),  false },
            { "F3",  nullptr, bindTecla(SDLK_F3),  false },
            { "F4",  nullptr, bindTecla(SDLK_F4),  false },
            { "F5",  nullptr, bindTecla(SDLK_F5),  false },
            { "F6",  nullptr, bindTecla(SDLK_F6),  false },
            { "F7",  nullptr, bindTecla(SDLK_F7),  false },
            { "F8",  nullptr, bindTecla(SDLK_F8),  false },
            { "F9",  nullptr, bindTecla(SDLK_F9),  false },
            { "F10", nullptr, bindTecla(SDLK_F10), false },
            { "F11", nullptr, bindTecla(SDLK_F11), false },
            { "F12", nullptr, bindTecla(SDLK_F12), false },

            // ── Android ──
            //
            // `SDLK_AC_BACK` ya llega a tres bloques del VN como sinonimo de
            // ESC. Se nombra; NO se liga (ley 3, salida (a) del fundador).
            { "ATRAS",       "BACK",         bindTecla(SDLK_AC_BACK),   false },

            // ── Mouse (§3.1: "mouse = teclas ligables") ──
            { "CLIC",         "CLICK",        bindBoton(SDL_BUTTON_LEFT),   false },
            { "CLIC_DER",     "RIGHT_CLICK",  bindBoton(SDL_BUTTON_RIGHT),  false },
            { "CLIC_MED",     "MIDDLE_CLICK", bindBoton(SDL_BUTTON_MIDDLE), false },
            { "RUEDA_ARRIBA", "WHEEL_UP",     bindRueda(1),                 false },
            { "RUEDA_ABAJO",  "WHEEL_DOWN",   bindRueda(-1),                false },

            // ── La letra que la REGLA no alcanza (CP4) ──
            //
            // Las letras son regla y no fila (`inputBindDe`: `SDLK_a + (c -
            // 'A')`), y esa regla es ASCII de punta a punta: pide `size() == 1`
            // y el rango `A-Z`. La ENIE ocupa DOS bytes en UTF-8, asi que no
            // entra por ahi ni de canto - y era la unica tecla del teclado
            // espanol que este motor no podia ligar. Lo destapo `CP4` porque la
            // aceptacion fundadora del arco esta escrita con ella.
            //
            // **Va como FILA y no como fold**: foldear el juego espanol aca le
            // agregaria una dependencia al header por UN caso, y este dialecto
            // compara byte a byte a proposito (ficha `FOLD-PARTIDO`). La
            // minuscula entra como ALIAS, que es el mecanismo que la tabla ya
            // tiene para `SHIFT` -> `SHIFT_IZQ`. Mismo criterio que
            // `MapData.h:1502`, que enumera las dos grafias de `tamano` a mano.
            //
            // El keycode es el CODEPOINT: SDL2 le da a las teclas imprimibles el
            // valor Unicode del caracter, y la enie minuscula es U+00F1 = 241.
            // No hay un `SDLK_` con nombre para ella. **Esta mitad -nombre ->
            // 241- la prueba el harness; que 241 LLEGUE desde el teclado del
            // jugador lo prueba el feel-test, y depende de su layout.**
            { "\xC3\x91", nullptr, bindTecla(static_cast<SDL_Keycode>(0xF1)), false },
            { "\xC3\xB1", nullptr, bindTecla(static_cast<SDL_Keycode>(0xF1)), true  },
        };
        return kTabla;
    }

    // Mayusculas ASCII, y solo ASCII. **No folda el juego espanol**
    // (`vocabToLower` de SYN-4c-i) a proposito: el dialecto compara byte a byte
    // y bajar el fold aca lo volveria el unico rincon que perdona el shift.
    //
    // > ⚠ **Hasta el 2026-08-07 este parrafo decia "no hay un solo nombre de
    // > tecla con enie ni tilde... el dia que aparezca uno, este es el lugar".
    // > `CP4` agrego la ENIE y el dia llego** - pero la salida NO fue el fold:
    // > fueron dos filas, canonica + alias, que es como la tabla ya resolvia
    // > `SHIFT`. La prediccion acerto el dia y erro el lugar. Que las
    // > mayusculas y las tildes esten partidas entre los cinco dialectos es la
    // > ficha `FOLD-PARTIDO`, y cuando se resuelva se resuelve para todos.
    inline std::string inputMayus(const std::string& s)
    {
        std::string r = s;
        for (char& c : r)
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        return r;
    }

    // nombre -> bind. `Kind::Ninguna` si el nombre no existe.
    //
    // Acepta las dos grafias y las tres formas alias. Las letras y los digitos
    // son REGLA, no fila: es lo que hacia `parseKeyName` (`SDLK_a + (c - 'A')`)
    // y se conserva palabra por palabra.
    inline InputBind inputBindDe(const std::string& nombre)
    {
        const std::string up = inputMayus(nombre);
        if (up.empty()) return InputBind{};

        // `ninguna` / `none`: desliga (ley 4). Es un bind valido y VACIO, no un
        // error - contesta `Ninguna` igual que un nombre desconocido, y quien
        // parsea la capa del creador distingue los dos casos por la palabra, no
        // por el resultado (CP3, con su aviso pedagogico).
        if (up == "NINGUNA" || up == "NONE") return InputBind{};

        for (const InputKeyName& e : inputTablaNombres()) {
            if (up == e.es) return e.bind;
            if (e.en && up == e.en) return e.bind;
        }

        if (up.size() == 1) {
            const char c = up[0];
            if (c >= 'A' && c <= 'Z')
                return bindTecla(static_cast<SDL_Keycode>(SDLK_a + (c - 'A')));
            if (c >= '0' && c <= '9')
                return bindTecla(static_cast<SDL_Keycode>(SDLK_0 + (c - '0')));
        }

        return InputBind{};
    }

    // bind -> nombre canonico (la otra cara, ley 2). "" si no tiene nombre.
    inline std::string inputNombreDe(const InputBind& b)
    {
        if (b.kind == InputBind::Kind::Ninguna) return std::string();

        for (const InputKeyName& e : inputTablaNombres()) {
            if (e.alias) continue;              // se acepta, no se devuelve
            if (e.bind == b) return std::string(e.es);
        }

        if (b.kind == InputBind::Kind::Tecla) {
            if (b.tecla >= SDLK_a && b.tecla <= SDLK_z)
                return std::string(1, static_cast<char>('A' + (b.tecla - SDLK_a)));
            if (b.tecla >= SDLK_0 && b.tecla <= SDLK_9)
                return std::string(1, static_cast<char>('0' + (b.tecla - SDLK_0)));
        }
        return std::string();
    }

    inline std::string inputNombreDe(SDL_Keycode k) { return inputNombreDe(bindTecla(k)); }

    // ── La firma que el contrato pide en §3.1, sobre el mismo cuerpo ────────
    //
    // Es lo que el QTE consume (CP0 cablea ese unico caller). Un nombre de mouse
    // no tiene keycode: contesta `SDLK_UNKNOWN`, y el caller decide. El QTE, que
    // hoy cae a `SDLK_e` ante cualquier cosa que no entiende, conserva esa red
    // en SU lado y no aca - un default arbitrario metido en la tabla se lo
    // comeria tambien la capa del creador, que necesita distinguir "no la
    // conozco" para poder avisar (CP3).
    inline SDL_Keycode inputKeycodeDe(const std::string& nombre)
    {
        const InputBind b = inputBindDe(nombre);
        return (b.kind == InputBind::Kind::Tecla) ? b.tecla : SDLK_UNKNOWN;
    }

    // ── LA RED DEL QTE (el unico cable que CP0 mueve) ───────────────────────
    //
    // El `parseKeyName` del QTE cae a `SDLK_e` ante cualquier nombre que no
    // entiende, desde siempre. Esa red **no se toca**: `tecla=` es sintaxis de
    // creador vigente, y romperla en un CP que se declara off-por-identidad
    // seria faltar a la unica promesa que CP0 hace.
    //
    // Vive aca y no en el call-site para que el harness pueda correr EL CABLE y
    // no una replica suya - `VNScriptState.h` arrastra GL y la bateria no lo
    // alcanza. Con esto, el cuerpo que queda en produccion es una linea.
    inline SDL_Keycode inputKeycodeDeODefault(const std::string& nombre,
                                              SDL_Keycode porDefecto)
    {
        const SDL_Keycode k = inputKeycodeDe(nombre);
        return (k != SDLK_UNKNOWN) ? k : porDefecto;
    }

    // ── EL EVENTO -> BIND ───────────────────────────────────────────────────
    //
    // Puro y sin estado: lee campos del union del evento y arma el atomo. Es la
    // unica parte de `resolver` que toca `SDL_Event`, y esta aparte para que el
    // harness la pueda correr sin fabricar un manager.
    //
    // **Mira el `type` ANTES de tocar el union, y eso no es estilo: es `H10`.**
    // El bug que CP0 poda en `translateSDLEventToAction` es exactamente leer
    // `e->wheel.y` en un evento que no es de rueda. Aca la lectura vive dentro
    // de su `case` y no puede pasar.
    //
    // La rueda con `y == 0` (scroll horizontal puro) contesta `Ninguna`: hoy el
    // motor no la mira, y traducirla a un verbo seria inventar conducta.
    inline InputBind inputBindDeEvento(const SDL_Event* e)
    {
        if (!e) return InputBind{};
        switch (e->type) {
        case SDL_KEYDOWN:
            return bindTecla(e->key.keysym.sym);
        case SDL_MOUSEBUTTONDOWN:
            return bindBoton(e->button.button);
        case SDL_MOUSEWHEEL:
            if (e->wheel.y > 0) return bindRueda(1);
            if (e->wheel.y < 0) return bindRueda(-1);
            return InputBind{};
        default:
            return InputBind{};
        }
    }

    // ── LA TABLA DEFAULT (ley 1: ES el censo de hoy) ────────────────────────
    //
    // Se escribe con la MISMA sintaxis que el bloque `teclas {}` del creador
    // (§5 del contrato: "forma del default interno, misma sintaxis"), y no es
    // cosmetica: el splitter que lee estas filas es el que CP3 reusa para leer
    // `proyecto.eclips`. Un default expresado en otra forma que la del creador
    // es un default que puede decir cosas que el creador no puede escribir.
    //
    // Cada fila trae PEGADO el sitio vivo del que sale, porque una tabla que
    // dice "esta es la conducta de hoy" sin decir donde se lee es una promesa
    // que nadie puede verificar (ficha `D-REF`).
    //
    // ── LAS ANCLAS SON NOMBRES DE FUNCION, Y ESO SE APRENDIO CARO ───────────
    //
    // En CP0 estas anclas eran NUMEROS DE LINEA, y duraron **un lote**: el
    // siguiente (`qte-sin-puerta`) inserto 27 lineas en `VNScriptState.h` y las
    // dieciseis anclas del dominio `vn` pasaron a apuntar a otra cosa - una a un
    // `return true;`, otra a un comentario sobre sprites. Las de `juego` y `dev`
    // sobrevivieron por suerte: nadie toco esos dos archivos.
    //
    // Nada se puso rojo, y no podia: la §4 del harness afirma el CONTENIDO de la
    // tabla, no sus anclas. Es la ficha `D-REF` en su forma mas barata - un
    // numero en prosa que nadie re-corre - cobrada sobre la superficie que la
    // invento.
    //
    // El remedio es el que `harness_bloques` ya usaba para citar el manual:
    // **se cita por NOMBRE DE FUNCION, no por linea**. Una funcion se puede
    // renombrar, pero eso rompe el build; una linea se corre sola y calla.
    // ── LA COLUMNA `verbo_en` ES LA LEY 2 APLICADA A LOS VERBOS (CP3) ──────
    //
    // `CP3` deja al creador escribir `keys game { jump: ... }`, asi que hace
    // falta traducir nombre-de-verbo -> canonico. La alternativa era una tabla
    // bilingue aparte, y eso es **§7.442 pedido por escrito**: dos tablas del
    // mismo vocabulario se desincronizan, y la que se desincroniza es la que
    // nadie mira. Aca la grafia inglesa es una COLUMNA de la fila que ya
    // existia - una tabla, dos direcciones, imposible de editar a medias.
    //
    // `nullptr` = la misma palabra en los dos idiomas (`menu`, `auto`, `dash`),
    // misma convencion que `InputKeyName`.
    //
    // Las grafias del dominio `vn` **no se eligieron aca**: son las que
    // `VNActionSeal.h` ya aceptaba (`vnLookupAction`), y el guardian de que
    // siguen de acuerdo es la §6 del harness, que corre en las dos direcciones.
    struct InputDefaultRow
    {
        const char* dominio;
        const char* verbo;      // canonico ES - la clave de la tabla
        const char* verbo_en;   // nullptr = misma palabra en los dos idiomas
        const char* teclas;
    };

    inline const std::vector<InputDefaultRow>& inputTablaDefault()
    {
        static const std::vector<InputDefaultRow> kDefault = {
            // ── dominio `vn` - VNScriptState.h ──
            //    `tr`  = VNScriptState::translateSDLEventToAction
            //    `he2` = VNScriptState::handleEvent, seccion 2 (atajos globales)
            // ── `CP7`: LAS DOS QUE `CP0` NOMBRO Y NO LIGO ──────────────────
            //
            // `ENTER_NUM` y `ATRAS` entran al default por firma del fundador
            // (2026-08-10), y con eso se borran las lineas crudas que `CP6`
            // habia dejado marcadas en `VNScriptState.h`. `TAB` **no entra**, y
            // no por olvido: ver el bloque de la ley 3 mas arriba.
            //
            //  · `ENTER_NUM` es sinonimo de ENTER en sus 3 sitios vivos, y hoy
            //    el Enter del numpad **no avanza el dialogo** -solo existe
            //    dentro de los modales-. Ligarlo cierra ese hueco.
            //  · `ATRAS` (`SDLK_AC_BACK`) es el boton fisico de Android y ya
            //    llegaba a 3 bloques del VN como sinonimo de ESC. En desktop esa
            //    tecla practicamente no se emite, asi que la conducta que cambia
            //    es la del port, que es para lo que la §10.2 la pedia.
            { "vn", "avanzar",          "advance",    "ESPACIO, ENTER, ENTER_NUM, Z, CLIC" }, // tr
            { "vn", "menu",             nullptr,      "ESC, ATRAS"              }, // tr + he2
            { "vn", "opcion_1",         "choice_1",   "1"                       }, // tr
            { "vn", "opcion_2",         "choice_2",   "2"                       }, // tr
            { "vn", "opcion_3",         "choice_3",   "3"                       }, // tr
            { "vn", "opcion_4",         "choice_4",   "4"                       }, // tr
            { "vn", "auto",             nullptr,      "A"                       }, // tr
            { "vn", "saltar_texto",     "skip",       "CTRL_IZQ, CTRL_DER"      }, // update (poleado)
            { "vn", "guardado_rapido",  "quicksave",  "K"                       }, // he2
            { "vn", "carga_rapida",     "quickload",  "L"                       }, // he2
            { "vn", "menu_guardar",     "save_menu",  "F5"                      }, // he2
            { "vn", "menu_cargar",      "load_menu",  "F9"                      }, // he2

            // ── LA FAMILIA DEL HISTORIAL, Y LAS DOS FILAS QUE FALTABAN ──
            //
            // `A-2`/`A-3` de la auditoria de CP1, y las dos son `E-1` otra vez:
            // esta tabla se titula "conducta actual, byte-identica" y le
            // faltaba la RUEDA de los dos lados.
            //
            //  · `RUEDA_ABAJO` no estaba en ninguna fila, con el motor
            //    scrolleando el log para abajo con la rueda desde siempre. Con
            //    la tabla asi, delegar mataba ese scroll.
            //
            //  · `RUEDA_ARRIBA` estaba en UNA fila, y sirve a DOS verbos: abre
            //    el log si esta cerrado y lo scrollea si esta abierto. Es la
            //    misma colision de la ley 5 que `ARRIBA`, y la tabla la
            //    modelaba como un verbo porque el motor la resolvia con un `if`
            //    adentro de la rama del wheel. **Quien desempata es el estado
            //    del log, y eso es Capa 3** (`VNVerbRoute.h`, ley 4).
            //
            // Las cazo el guardian de no-op de `harness_ruta` §1 antes de que
            // se tocara produccion: nacio rojo en estas dos filas y en ninguna
            // otra de las treinta.
            { "vn", "historial",        "history",      "RUEDA_ARRIBA"                }, // tr
            { "vn", "historial_arriba", "history_up",   "ARRIBA, REPAG, RUEDA_ARRIBA" }, // tr
            { "vn", "historial_abajo",  "history_down", "ABAJO, AVPAG, RUEDA_ABAJO"   }, // tr

            // Los cuatro `foco_*` son los UNICOS verbos de esta tabla sin sitio
            // vivo: el anillo nace en CP5. Estan aca porque la §5 del contrato
            // los firmo, y porque sacarlos obligaria a CP5 a re-abrir la tabla
            // para agregarlos - con la colision `ARRIBA` de la ley 5 sin
            // custodiar hasta entonces. Un verbo sin sitio no cambia conducta:
            // todavia no resuelve nadie.
            { "vn", "foco_arriba",      "focus_up",    "ARRIBA"    }, // CP5
            { "vn", "foco_abajo",       "focus_down",  "ABAJO"     }, // CP5
            { "vn", "foco_izquierda",   "focus_left",  "IZQUIERDA" }, // CP5
            { "vn", "foco_derecha",     "focus_right", "DERECHA"   }, // CP5

            // ── dominio `juego` - TRES estados vivos, no uno ──
            //
            // `CP2` midio esta seccion desde el CONSUMIDOR (§7.475) y encontro
            // que la §5 del contrato la habia enumerado desde `Gameplay2DState`
            // solo. Son tres los estados que leen este dominio, y los tres se
            // abren desde un `.ecl`:
            //
            //    `g2d` = Gameplay2DState::handleEvent / ::update   (cmd `gameplay`)
            //    `cor` = CorridorState::handleEvent  / ::update    (cmd `ir_a_pasillo`)
            //    `bh`  = BulletHellState::handleEvent / ::update   (cmd `iniciar_batalla`)
            //
            // `SideScrollState`, `TestState` y `ZLayerTestState` tambien polean,
            // y NO estan aca a proposito: nadie los construye. Un default que
            // enumere codigo muerto miente en la direccion contraria.
            //
            // **`bh` NO aporta verbos nuevos a este dominio, y eso es el fork 2b
            // del fundador:** su menu de batalla (confirmar `Z/ENTER/ESPACIO`,
            // cancelar `X/ESC`, cursor `W/S/A/D`+flechas) es el ANILLO DE FOCO
            // del dominio `vn` -`avanzar`, `menu`, `foco_*`- aplicado adentro de
            // un estado de gameplay, no una familia nueva. Lo que lo midio: su
            // confirm es `Z || ENTER || ESPACIO`, byte por byte el conjunto de
            // `vn.avanzar` menos el `CLIC`. Inventarle un `confirmar` a `juego`
            // habria sido dos nombres para un concepto que ya tiene el suyo
            // (§2.9 cuarta pregunta). Su `X` es lo unico sin verbo hoy, y queda
            // fichado - no se le cuelga a `vn.menu`, que le pondria una segunda
            // tecla de pausa a la novela visual.
            { "juego", "mover_izquierda", "move_left",  "A, IZQUIERDA"         }, // g2d/cor/bh update (poleado)
            { "juego", "mover_derecha",   "move_right", "D, DERECHA"           }, // g2d/cor/bh update
            { "juego", "mover_arriba",    "move_up",    "W, ARRIBA"            }, // g2d/bh update
            { "juego", "mover_abajo",     "move_down",  "S, ABAJO"             }, // g2d/bh update
            { "juego", "saltar",          "jump",       "ESPACIO"              }, // g2d handleEvent
            // `ESPACIO` lo trae `cor`, que interactua con las tres (§7.475: la
            // §5 decia "E, ENTER" porque miraba `g2d`). En `g2d` es INERTE por
            // orden de la ley -`saltar`/`atacar` se preguntan antes-, y esa
            // inercia tiene assert propio en vez de quedar de palabra.
            { "juego", "interactuar",     "interact",   "E, ENTER, ESPACIO"    }, // g2d + cor handleEvent
            { "juego", "dash",            nullptr,      "SHIFT_IZQ, SHIFT_DER" }, // g2d handleEvent
            // `ESPACIO` es el ataque del TOPDOWN, y su ausencia era la ficha
            // `TABLA-JUEGO-INCOMPLETA`: `atacar` no tenia tecla en ese modo, asi
            // que delegar fiel a la §5 lo mataba en silencio. El desempate con
            // `saltar` lo hace la Capa 3 por `m_isSideScroll`, que es la ley 4
            // de `VNVerbRoute.h` aplicada a otro dominio.
            { "juego", "atacar",          "attack",     "F, ESPACIO"           }, // g2d handleEvent
            // `CP7` (fork 4b): `ATRAS` tambien aca. El boton Atras de Android
            // tiene que abrir la pausa en gameplay igual que en la novela; los
            // dos dominios tenian `menu: ESC` y los sitios son simetricos.
            { "juego", "menu",            nullptr,      "ESC, ATRAS"           }, // g2d/cor/bh handleEvent

            // ── dominio `dev` - export-strip ya fichado como TODO ──
            //
            // **NADIE resuelve este dominio.** Medido en `CP3`: el string
            // `"dev"` aparece en el motor UNICAMENTE en estas tres filas; los
            // tres sitios siguen comparando keycodes crudos (`Engine.cpp` F8 y
            // F10, `VNScriptState.h` F3). Y ningun `CP` del plan §14 lo toma.
            // Por eso `CP3` lo rechaza como dominio REMAPEABLE - ficha
            // `DEV-SIN-DUENO`, y el rechazo se INVIERTE el dia que tenga dueno.
            { "dev", "dev_recargar", "dev_reload", "F8"  },   // Engine::processEvents
            { "dev", "dev_vsync",    nullptr,      "F10" },   // Engine::processEvents
            { "dev", "dev_hotspots", nullptr,      "F3"  },   // VNScriptState::handleEvent, sec. 2
        };
        return kDefault;
    }

    // Parte "ESPACIO, ENTER, Z" en sus nombres. Es el splitter que CP3 hereda,
    // asi que tolera separadores de sobra y elementos vacios: una coma colgando
    // no es un error, es un renglon que alguien esta editando.
    //
    // ── EL ESPACIO TAMBIEN SEPARA, Y ESO ES CP3 (fork 2a) ──────────────────
    //
    // Hasta `CP2` el espacio se **descartaba**, asi que `E ENTER X` colapsaba a
    // un solo token `EENTERX`: un nombre desconocido, y un aviso que nombraba
    // basura. No molestaba mientras la unica fuente de estas cadenas fuera la
    // tabla de aca abajo, escrita con comas. **Deja de no molestar cuando la
    // escribe el creador**, porque en `.eclips` los multi-valor se separan con
    // ESPACIOS: `resolucion_diseno 1920 1080` (`ProjectManifest.h`). Un creador
    // que escribe `interactuar E ENTER X` esta escribiendo el dialecto bien.
    //
    // **Es no-op sobre la tabla default**, y eso no es un detalle: sus filas se
    // escriben `"A, IZQUIERDA"`, o sea coma Y espacio, asi que los dos
    // regimenes dan la misma lista. Si ese guardian cayera, este cambio no
    // agrego una comodidad: le cambio las teclas al motor. Lo clava el harness,
    // y **el conteo lo imprime en vez de escribirlo**: esta linea decia "las 35
    // filas" y eran 31.
    inline std::vector<std::string> inputPartirLista(const std::string& lista)
    {
        std::vector<std::string> out;
        std::string actual;
        for (const char c : lista) {
            if (c == ',' || c == ' ' || c == '\t') {
                if (!actual.empty()) out.push_back(actual);
                actual.clear();
                continue;
            }
            actual.push_back(c);
        }
        if (!actual.empty()) out.push_back(actual);
        return out;
    }


    // -- SECUENCIA: un nombre -> miembro (DISENO_SECUENCIA SQ-3 / SQ-10) -----
    //
    // Junto a `inputBindDe` / `inputPartirLista`: resuelve un nombre de tecla
    // para un renglon `secuencia`. Motivos:
    //   Ok           tecla fisica valida
    //   Desconocida  vacio / NINGUNA / NONE / nombre que no existe
    //   Mouse        boton o rueda (SQ-3: sin mouse)
    //   Dev          tecla que aparece en alguna fila `dominio == "dev"` de la
    //                tabla PASADA (exclusion por referencia, no lista a mano)
    //
    // La sobrecarga de dos argumentos es plantilla a proposito: si fuera
    // funcion ordinaria, ADL sobre `vector<InputDefaultRow>` la meteria en
    // llamadas no calificadas de un namespace de control del harness (C-M3) y
    // chocaria con la sobrecarga local. Plantilla pierde ante no-plantilla.
    // La sobrecarga de un argumento (fabrica) queda ordinaria. Lo prueba
    // harness_secuencia_miembros.cpp (M1-M4).
    enum class SecuenciaMotivo : unsigned char { Ok, Desconocida, Mouse, Dev };

    struct SecuenciaMiembro
    {
        SecuenciaMotivo motivo = SecuenciaMotivo::Desconocida;
        SDL_Keycode     tecla  = SDLK_UNKNOWN;
    };

    template <typename TablaT>
    inline SecuenciaMiembro inputMiembroDeSecuencia(const std::string& nombre,
                                                    const TablaT& tabla)
    {
        const InputBind b = inputBindDe(nombre);
        if (b.kind == InputBind::Kind::Ninguna)
            return SecuenciaMiembro{ SecuenciaMotivo::Desconocida, SDLK_UNKNOWN };
        if (b.kind != InputBind::Kind::Tecla)
            return SecuenciaMiembro{ SecuenciaMotivo::Mouse, SDLK_UNKNOWN };

        for (const auto& f : tabla)
        {
            if (std::string(f.dominio) != "dev") continue;
            for (const std::string& n : inputPartirLista(f.teclas))
            {
                const InputBind db = inputBindDe(n);
                if (db.kind == InputBind::Kind::Tecla && db.tecla == b.tecla)
                    return SecuenciaMiembro{ SecuenciaMotivo::Dev, b.tecla };
            }
        }
        return SecuenciaMiembro{ SecuenciaMotivo::Ok, b.tecla };
    }

    inline SecuenciaMiembro inputMiembroDeSecuencia(const std::string& nombre)
    {
        return inputMiembroDeSecuencia(nombre, inputTablaDefault());
    }

    // ── LA TABLA, COMO TIPO PROPIO ──────────────────────────────────────────
    //
    // Vivia adentro de la clase (`InputManager::Tabla`, que sigue existiendo y
    // es este mismo tipo). Sale para que la ley de `CP3` -el parser y la red-
    // se pueda escribir ANTES de la clase, que es lo que necesita para que
    // `aplicarCascada` la llame. Mismo motivo por el que `inputHeldDeTabla`
    // vive despues: una ley pura no puede quedar del lado equivocado de su
    // unico caller.
    using InputTabla = std::map<std::string, std::map<std::string, std::vector<InputBind>>>;

    // ════════════════════════════════════════════════════════════════════════
    // CP3 - LA CAPA DEL CREADOR (bloque `teclas {}` de `proyecto.eclips`)
    // ════════════════════════════════════════════════════════════════════════
    //
    // ── LO QUE ESTE BLOQUE *NO* HACE: LEER EL ARCHIVO ───────────────────────
    //
    // No abre nada, no conoce `ProjectManifest`, no imprime una linea. Recibe
    // filas de TEXTO ya extraidas y devuelve una `InputTabla` + una lista de
    // avisos como DATOS. El precedente es `vnVozDelVeto` de `VNActionSeal.h`, y
    // las dos razones son las suyas: este header sigue sin `<iostream>`, y el
    // harness puede afirmar las frases **byte a byte** en vez de cazar stderr.
    //
    // Tambien es lo que mantiene a `ProjectManifest.h` sin SDL: si el parseo
    // viviera alla, ese header arrastraria `<SDL_keycode.h>` y `harness_grafia`
    // y `harness_borde` -que lo incluyen- necesitarian `EXTRAINC` en el `.bat`.
    //
    // ── LA FORMA QUE EL CONTRATO ESCRIBE NO SE PUEDE ESCRIBIR ───────────────
    //
    // La §6 del contrato dibuja el bloque asi:
    //
    //     teclas { vn { guardado_rapido: F2 } }
    //
    // y **ninguna de las dos mitades es expresable en un `.eclips`**:
    //
    //   · los `:` estan PROHIBIDOS en ese dialecto, con rechazo pedagogico
    //     propio (`ProjectManifest.h`) y regla documentada en el manual
    //     ("Sobre los `:` en EclipScript"). No es un hueco del parser: es ley.
    //   · el manifest NO ANIDA. Su maquina de estados tiene dos niveles
    //     (TopLevel / adentro-de-un-bloque) y un `{` adentro de un bloque es un
    //     error declarado, en los cuatro bloques que ya existen.
    //
    // La forma que si existe es la del dialecto vivo -`personaje X {`,
    // `estilo X {`-, o sea **el dominio como NOMBRE del bloque**:
    //
    //     teclas vn {                 keys game {
    //         guardado_rapido F2          interact E, ENTER, X
    //         auto ninguna            }
    //     }
    //
    // Cero anidamiento, cero `:`, y el mismo `k v` que el creador ya escribe.
    // Firma del fundador (fork 1a). Lo que se pierde es el agrupado visual del
    // contrato, que era cosmetico.

    // ── LOS DOMINIOS QUE EL CREADOR PUEDE ESCRIBIR ──────────────────────────
    //
    // `dev` NO es uno, y no es por prolijidad: **nadie resuelve ese dominio**
    // (sus tres sitios comparan keycodes crudos y ningun `CP` del plan §14 lo
    // toma), y ademas sus dos sitios de `Engine.cpp` llevan *"se compila fuera
    // en build de export"*. Aceptarlo firmaria una sintaxis con DOS CONDUCTAS
    // segun como compilaste, que es peor que una inerte. Ficha `DEV-SIN-DUENO`.
    //
    // **Se acepta como PALABRA y se rechaza como DOMINIO**, que es la
    // diferencia entre *"no existe"* y *"existe y no se toca"* - la pedagogia
    // entera esta en distinguir esas dos.
    enum class InputDominioEstado : unsigned char
    {
        Remapeable,     // `vn`, `juego`/`game`
        NoRemapeable,   // `dev` - existe, y el creador no lo escribe
        Desconocido     // no es un dominio
    };

    struct InputDominioRes
    {
        InputDominioEstado estado = InputDominioEstado::Desconocido;
        std::string        canonico;   // "" si no es un dominio
    };

    // **No folda mayusculas ni tildes, a proposito.** Es la conducta del
    // dialecto: `ProjectManifest.h` y `Theme.h` comparan sus claves byte a byte
    // (`key == "titulo"`), y bajar un fold solo para este bloque lo volveria el
    // unico rincon del `.eclips` que perdona el shift. La ficha `TEMA-SIN-FOLD`
    // es de esa familia y cuando se resuelva, se resuelve para todos.
    inline InputDominioRes inputDominioDe(const std::string& nombre)
    {
        InputDominioRes r;
        if (nombre == "vn") {
            r.estado = InputDominioEstado::Remapeable; r.canonico = "vn";
        }
        else if (nombre == "juego" || nombre == "game") {
            r.estado = InputDominioEstado::Remapeable; r.canonico = "juego";
        }
        else if (nombre == "dev") {
            r.estado = InputDominioEstado::NoRemapeable; r.canonico = "dev";
        }
        return r;
    }

    // nombre (ES o EN) -> verbo canonico ES de ese dominio. "" si no existe.
    //
    // La fuente es `inputTablaDefault()`, o sea **la misma tabla que dice que
    // teclas trae cada verbo**. Que el conjunto de verbos conocidos salga de
    // ahi no es economia: es lo que hace que el dia que el set se ensanche
    // (`VERBOS-LIBRES-LUA`, rama A) no haya una segunda lista que actualizar.
    inline std::string inputVerboDe(const std::string& nombre,
                                    const std::string& dominioCanonico)
    {
        for (const InputDefaultRow& r : inputTablaDefault()) {
            if (dominioCanonico != r.dominio) continue;
            if (nombre == r.verbo) return std::string(r.verbo);
            if (r.verbo_en && nombre == r.verbo_en) return std::string(r.verbo);
        }
        return std::string();
    }

    // `ninguna` / `none` - la palabra que desliga (ley 4).
    inline bool inputEsNinguna(const std::string& tok)
    {
        const std::string up = inputMayus(tok);
        return up == "NINGUNA" || up == "NONE";
    }

    // ── LAS CAUSAS, Y POR QUE SON UN ENUM Y NO SIETE `if` CON STRINGS ───────
    //
    // Porque **una de ellas tiene fecha de vencimiento firmada**. El dia que
    // lleguen las acciones custom de Lua (`VERBOS-LIBRES-LUA`, gatillo `CP8`),
    // `VerboDesconocido` deja de ser un error para una parte del vocabulario.
    // Con la causa nombrada, ese dia cambia UNA rama y el rojo del harness sale
    // selectivo y nombra el assert que hay que invertir con su razon y su fecha
    // (§7.328). Con siete `if` imprimiendo literales, ese dia se re-deriva el
    // parser entero.
    enum class InputAvisoCausa : unsigned char
    {
        DominioDesconocido,    // `teclas foo {`
        DominioNoRemapeable,   // `teclas dev {`
        VerboDesconocido,      // typo del creador - o una accion custom de CP8
        TeclaDesconocida,      // un nombre que la tabla de nombres no tiene
        NingunaMezclada,       // `auto A, ninguna` - desliga la lista ENTERA
        SinTeclas,             // ningun nombre valido, y no dijo `ninguna`
        VerboRepetido,         // dos renglones para la misma accion
        ClaveMalFormada,       // CP4: `tecla.vn=F2` - la clave no tiene 3 partes
        RedDeSeguridad         // §8: `menu` quedo sin tecla y se re-inyecto ESC
    };

    struct InputAviso
    {
        InputAvisoCausa causa   = InputAvisoCausa::VerboDesconocido;
        int             linea   = 0;   // 0 = sin origen en un archivo
        std::string     dominio;       // TAL CUAL lo escribio el creador
        std::string     verbo;         // idem
        std::string     detalle;       // el token que fallo, cuando aplica
    };

    // La voz, en un solo lugar y devuelta en vez de impresa.
    //
    // **La grafia que se nombra es la que el creador ESCRIBIO**, nunca la
    // canonica a la que el motor la tradujo: es la ficha `G2` / `Y-VN-01` que
    // `harness_grafia` custodia en el otro dialecto. Un aviso que contesta
    // `juego` cuando el creador escribio `game` lo manda a buscar una linea que
    // no existe en su archivo.
    inline std::string inputVozDelAviso(const InputAviso& a,
                                        const std::string& origen)
    {
        const std::string donde =
            (a.linea > 0) ? (" linea " + std::to_string(a.linea)) : std::string();

        switch (a.causa) {
        case InputAvisoCausa::DominioDesconocido:
            return "[" + origen + "] Aviso" + donde + ": no conozco el dominio de teclas '"
                 + a.dominio + "'.\n"
                   "  Los que el creador puede remapear son 'vn' y 'juego' ('game').\n"
                   "  Bloque ignorado.";

        case InputAvisoCausa::DominioNoRemapeable:
            return "[" + origen + "] Aviso" + donde + ": el dominio '" + a.dominio
                 + "' existe, pero NO se remapea.\n"
                   "  Son las teclas de desarrollo (F8 recargar, F10 vsync, F3 hotspots)\n"
                   "  y no viajan al build que juega tu jugador. Bloque ignorado.";

        case InputAvisoCausa::VerboDesconocido:
            return "[" + origen + "] Aviso" + donde + ": '" + a.verbo
                 + "' no es una accion del dominio '" + a.dominio + "'. Linea ignorada.";

        case InputAvisoCausa::TeclaDesconocida:
            return "[" + origen + "] Aviso" + donde + ": la accion '" + a.verbo
                 + "' pide la tecla '" + a.detalle + "', que no existe.\n"
                   "  Las demas teclas de esa linea si se ligaron.";

        case InputAvisoCausa::NingunaMezclada:
            return "[" + origen + "] Aviso" + donde + ": 'ninguna' desliga la accion '"
                 + a.verbo + "' entera, no se mezcla con otras teclas.\n"
                   "  Tomo la linea como '" + a.verbo + " ninguna'. Si querias ligar\n"
                   "  teclas, quita 'ninguna'.";

        case InputAvisoCausa::SinTeclas:
            return "[" + origen + "] Aviso" + donde + ": la accion '" + a.verbo
                 + "' del dominio '" + a.dominio + "' quedo sin ninguna tecla valida.\n"
                   "  Conserva las de fabrica. Si querias dejarla sin tecla, escribi\n"
                   "  '" + a.verbo + " ninguna'.";

        case InputAvisoCausa::VerboRepetido:
            return "[" + origen + "] Aviso" + donde + ": la accion '" + a.verbo
                 + "' ya tenia teclas en este bloque. Uso la ultima.";

        // CP4. Es la unica causa que nombra una CLAVE entera en vez de un
        // dominio o un verbo, porque en esta capa la clave la escribe el
        // jugador de punta a punta y el error puede estar en la FORMA, antes
        // de que haya un dominio o un verbo que nombrar. Por eso el aviso
        // ensena la forma completa con un ejemplo: es lo unico accionable
        // cuando lo que fallo es el molde.
        case InputAvisoCausa::ClaveMalFormada:
            return "[" + origen + "] Aviso: no entiendo la clave '" + a.detalle
                 + "'.\n"
                   "  Una tecla del jugador se escribe en TRES partes separadas\n"
                   "  por punto: 'tecla.<dominio>.<accion>=<teclas>'.\n"
                   "  Por ejemplo: 'tecla.vn.guardado_rapido=F2'. Linea ignorada.";

        case InputAvisoCausa::RedDeSeguridad:
        default:
            return "[" + origen + "] RED DE SEGURIDAD: 'menu' quedo sin ninguna tecla en '"
                 + a.dominio + "'.\n"
                   "  Le vuelvo a poner ESC: sin esa accion no hay como salir. Es la\n"
                   "  unica accion con piso garantizado; el resto se puede desligar.";
        }
    }

    // Una fila tal cual sale del archivo: nada interpretado todavia.
    struct InputFilaCreador
    {
        std::string dominio;   // el NOMBRE del bloque (`teclas vn {`)
        std::string verbo;     // la primera palabra del renglon
        std::string teclas;    // el resto del renglon, sin partir
        int         linea = 0;
    };

    struct InputCapaCreador
    {
        InputTabla              tabla;
        std::vector<InputAviso> avisos;
    };

    // ── EL PARSER ───────────────────────────────────────────────────────────
    //
    // Dos decisiones que no son obvias y que deciden como se siente un typo:
    //
    //  1. **Una tecla mala NO tira el renglon.** Se ligan las buenas y se avisa
    //     por la mala. Tirarlo entero devolveria el default en silencio, y el
    //     creador veria "mi remapeo no hizo nada" sin saber cual de las tres
    //     palabras estaba mal.
    //  2. **...pero si NINGUNA sirve, el verbo NO SE TOCA** (hereda de fabrica)
    //     en vez de quedar desligado. Un renglon entero mal escrito no puede
    //     tener el mismo efecto que `ninguna`: eso apagaria una accion que el
    //     creador cree haber remapeado, y el aviso ensena la forma deliberada.
    inline InputCapaCreador inputParsearCapaCreador(
        const std::vector<InputFilaCreador>& filas)
    {
        InputCapaCreador out;

        // Los dominios malos que ya hablaron. El aviso de dominio dice "Bloque
        // ignorado", o sea que es del BLOQUE - emitirlo por renglon convertiria
        // un `teclas dev {` de cinco lineas en cinco gritos identicos, y un
        // aviso que se repite se deja de leer (la misma muerte que la doc que
        // nadie abre). Se guarda la grafia CRUDA: `dev` y `DEV` son dos errores
        // distintos y los dos merecen su linea.
        std::vector<std::string> yaHablaron;

        // Asigna avisando si pisa. Los cuatro bloques que el manifest ya tiene
        // (`personaje`, `estilo`, `imagenes`, `pantallas`) avisan al duplicar;
        // sin esto, `teclas` seria el UNICO mudo del dialecto.
        auto asignar = [&out](const std::string& domCanon, const std::string& verbo,
                              const std::vector<InputBind>& binds,
                              const InputFilaCreador& f)
        {
            const auto d = out.tabla.find(domCanon);
            if (d != out.tabla.end() && d->second.count(verbo) > 0) {
                InputAviso a;
                a.causa = InputAvisoCausa::VerboRepetido;
                a.linea = f.linea; a.dominio = f.dominio; a.verbo = verbo;
                out.avisos.push_back(a);
            }
            out.tabla[domCanon][verbo] = binds;
        };

        for (const InputFilaCreador& f : filas)
        {
            const InputDominioRes dom = inputDominioDe(f.dominio);

            if (dom.estado != InputDominioEstado::Remapeable) {
                bool repetido = false;
                for (const std::string& d : yaHablaron)
                    if (d == f.dominio) { repetido = true; break; }
                if (!repetido) {
                    yaHablaron.push_back(f.dominio);
                    InputAviso a;
                    a.causa = (dom.estado == InputDominioEstado::NoRemapeable)
                            ? InputAvisoCausa::DominioNoRemapeable
                            : InputAvisoCausa::DominioDesconocido;
                    a.linea = f.linea; a.dominio = f.dominio; a.verbo = f.verbo;
                    out.avisos.push_back(a);
                }
                continue;
            }

            const std::string verbo = inputVerboDe(f.verbo, dom.canonico);
            if (verbo.empty()) {
                InputAviso a;
                a.causa = InputAvisoCausa::VerboDesconocido;
                a.linea = f.linea; a.dominio = f.dominio; a.verbo = f.verbo;
                out.avisos.push_back(a);
                continue;
            }

            const std::vector<std::string> toks = inputPartirLista(f.teclas);

            bool hayNinguna = false;
            for (const std::string& t : toks) if (inputEsNinguna(t)) hayNinguna = true;

            if (hayNinguna) {
                if (toks.size() > 1) {
                    InputAviso a;
                    a.causa = InputAvisoCausa::NingunaMezclada;
                    a.linea = f.linea; a.dominio = f.dominio; a.verbo = verbo;
                    out.avisos.push_back(a);
                }
                asignar(dom.canonico, verbo, {}, f);   // desligado, y EXISTE
                continue;
            }

            std::vector<InputBind> binds;
            for (const std::string& t : toks) {
                const InputBind b = inputBindDe(t);
                if (b.kind != InputBind::Kind::Ninguna) { binds.push_back(b); continue; }
                InputAviso a;
                a.causa = InputAvisoCausa::TeclaDesconocida;
                a.linea = f.linea; a.dominio = f.dominio; a.verbo = verbo; a.detalle = t;
                out.avisos.push_back(a);
            }

            if (binds.empty()) {
                InputAviso a;
                a.causa = InputAvisoCausa::SinTeclas;
                a.linea = f.linea; a.dominio = f.dominio; a.verbo = verbo;
                out.avisos.push_back(a);
                continue;   // NO se toca el verbo: hereda
            }

            asignar(dom.canonico, verbo, binds, f);
        }

        return out;
    }

    // ── LA RED DE SEGURIDAD (§8 del contrato) ───────────────────────────────
    //
    // *"Dejar un verbo sin tecla se permite. PERO si `menu` queda sin NINGUN
    // binding en su dominio, el engine re-inyecta ESC y avisa ruidoso."*
    //
    // Es el patron soft-lock §7.103 con una excepcion firmada: el motor avisa y
    // **ademas inventa**, porque aca el atasco seria total. `menu` es el unico
    // verbo con piso; el resto es libertad.
    //
    // **Corre sobre la tabla RESUELTA y no sobre la capa**, o sea despues de
    // las dos cascadas. Un creador que desliga `menu` y un jugador que se lo
    // vuelve a ligar no tienen por que gatillarla: lo que importa es como quedo.
    //
    // **Y los dominios salen del DEFAULT, no de una lista escrita a mano.** Hoy
    // son `vn` y `juego`; el dia que un dominio nuevo traiga `menu`, entra solo.
    // Es la misma razon por la que el set de verbos conocidos sale de ahi.
    inline std::vector<std::string> inputRedDeSeguridad(InputTabla& t)
    {
        std::vector<std::string> rescatados;

        for (const InputDefaultRow& r : inputTablaDefault()) {
            if (std::string(r.verbo) != "menu") continue;

            std::vector<InputBind>& lista = t[r.dominio]["menu"];
            bool vivo = false;
            for (const InputBind& b : lista)
                if (b.kind != InputBind::Kind::Ninguna) { vivo = true; break; }

            if (!vivo) {
                lista = { bindTecla(SDLK_ESCAPE) };
                rescatados.push_back(r.dominio);
            }
        }

        return rescatados;
    }

    // ════════════════════════════════════════════════════════════════════════
    // CP4 - LA CAPA DEL JUGADOR (claves `tecla.*` de `saves/settings.eclips`)
    // ════════════════════════════════════════════════════════════════════════
    //
    // ── LO QUE ESTE BLOQUE NO HACE, Y ES LO MISMO QUE NO HACE `CP3` ─────────
    //
    // No abre `settings.eclips`, no conoce `Settings` y no imprime una linea.
    // Recibe pares clave/valor ya leidos y devuelve tabla + avisos como DATOS.
    //
    // **La §3.1 del contrato firma `loadPlayerLayer()` como un metodo de esta
    // clase que lee el archivo, y esa firma no se puede sostener**: seria el
    // primer sitio del header que abre algo, y de que no lo haga depende que la
    // bateria pueda afirmar las voces byte a byte sin cazar stderr. La firma es
    // del 2026-07-02, anterior a esa propiedad. Enmienda anotada en el contrato.
    //
    // ── POR QUE NO HAY UN PARSER NUEVO ──────────────────────────────────────
    //
    // Porque la ley es LA MISMA. Una vez que la clave se parte en (dominio,
    // verbo), todo lo que queda -que el verbo exista, que la tecla exista,
    // `ninguna`, la tecla mala que no tira el renglon, el verbo repetido- ya lo
    // decide `inputParsearCapaCreador`. Decidirlo distinto aca haria que el
    // MISMO error se sintiera de dos formas segun quien lo escribio.
    //
    // Lo unico propio de esta capa es la FORMA DE LA CLAVE, y es lo unico que
    // vive aca abajo.

    struct InputClaveJugador
    {
        bool        nuestra = false;   // el prefijo es `tecla.` / `key.`
        bool        formada = false;   // ...y quedan exactamente dos partes mas
        std::string dominio;
        std::string verbo;
    };

    // `tecla.vn.guardado_rapido` -> { nuestra, formada, "vn", "guardado_rapido" }
    //
    // El prefijo se acepta en las dos grafias -`tecla` es palabra cotidiana y
    // §7.445 manda traducirla-.
    //
    // ── ESTE PARRAFO DECIA LO CONTRARIO, Y SU VENCIMIENTO LLEGO ────────────
    //
    // > "...y se compara BYTE A BYTE, como el resto del dialecto: un `Tecla.`
    // >  no es nuestra y se va sin avisar. Eso es deliberado y tiene guardian
    // >  propio, para que el dia de `FOLD-PARTIDO` se INVIERTA y se anote en
    // >  vez de descubrirse."
    //
    // Ese dia es hoy (`sintaxis-ciega-a-mayusculas`, 2026-08-25). La ficha
    // `FOLD-PARTIDO` decia que el fold se resuelve para los CINCO dialectos de
    // un saque, y este era el quinto. Las TRES partes de la clave son
    // vocabulario del motor -el prefijo, el dominio (`vn`/`juego`/`dev`) y el
    // verbo, que `inputVerboDe` resuelve contra la tabla del motor-, o sea
    // ranura PURA: no hay nada aca que un creador invente. El `14.4` de
    // `harness_teclas` se invirtio y se anoto en el mismo diff (§7.328).
    //
    // Una clave AJENA sigue saliendo por el `return r` de abajo con
    // `nuestra=false`: plegar antes de mirar el prefijo no la toca, porque
    // `textspeed` tampoco empieza con `tecla.`.
    //
    // **El separador es un PUNTO, y no es cosmetica.** Todas las claves que este
    // formato escribe son `snake_case` con `_` (`slot_0_anims`, `text_speed`),
    // pero todas se CONSTRUYEN por concatenacion y se leen por coincidencia
    // exacta: ninguna se parte nunca. Esta es la primera que hay que DESARMAR,
    // y los verbos llevan `_` adentro (`guardado_rapido`, `mover_izquierda`),
    // asi que con `_` habria que partir "por los dos primeros" - lo que funciona
    // hoy SOLO porque ningun dominio tiene `_`, y se rompe en silencio el dia
    // que uno lo tenga. Ni los dominios ni los verbos llevan punto.
    inline InputClaveJugador inputPartirClaveJugador(const std::string& clave)
    {
        InputClaveJugador r;

        // FOLD-PARTIDO. Las tres partes son vocabulario del motor, asi que se
        // pliegan juntas y una sola vez, antes de mirar el prefijo.
        const std::string cl = vocabToLower(clave);

        std::string resto;
        if (cl.rfind("tecla.", 0) == 0)    resto = cl.substr(6);
        else if (cl.rfind("key.", 0) == 0) resto = cl.substr(4);
        else return r;

        r.nuestra = true;

        const size_t p = resto.find('.');
        if (p == std::string::npos) return r;

        const std::string dom = resto.substr(0, p);
        const std::string ver = resto.substr(p + 1);

        // TRES partes exactas: ni `tecla.vn` ni `tecla.vn.a.b`. Un punto de mas
        // no puede ser un verbo raro, porque ningun verbo de la tabla lo lleva.
        if (dom.empty() || ver.empty() || ver.find('.') != std::string::npos)
            return r;

        r.formada = true;
        r.dominio = dom;
        r.verbo   = ver;
        return r;
    }

    // ── `CP9`: EL DISPLAY DE UNA TECLA (`${_tecla.<dominio>.<accion>}`) ─────
    //
    // La otra cara de §7.205, y la ultima que faltaba: `inputBindDe` traduce
    // NOMBRE -> tecla para el que escribe un binding; esto traduce ACCION ->
    // nombre para el que lee un tutorial. Una tabla, dos direcciones.
    //
    // ── POR QUE LLEVA PUNTOS Y NO GUIONES BAJOS (firma del fundador) ────────
    //
    // El §12 del contrato escribe `${_tecla_<accion>}` con `vn` implicito y
    // `${_tecla_juego_<accion>}` para el otro dominio. **Se cambio a la forma
    // con punto**, y el argumento no es de gusto: esta escrito veinte lineas
    // mas arriba, por `CP4`, contra esa misma idea —
    //
    //     "El separador es un PUNTO... con `_` habria que partir por los dos
    //      primeros, lo que funciona hoy SOLO porque ningun dominio tiene `_`,
    //      y se rompe en silencio el dia que uno lo tenga."
    //
    // — y aca el riesgo es peor que alla, porque los VERBOS si llevan `_`
    // adentro (`guardado_rapido`, `mover_izquierda`) y **tres de ellos empiezan
    // con el nombre de su propio dominio** (`dev_recargar`, `dev_vsync`,
    // `dev_hotspots`). Con `_` como separador, `_tecla_dev_recargar` tiene dos
    // lecturas y ninguna regla las separa.
    //
    // Lo que la forma con punto compra, ademas de no romperse: **es el MISMO
    // string que el jugador escribe en `settings.eclips`**. El creador pinta
    // `${_tecla.vn.guardado_rapido}` y el jugador edita
    // `tecla.vn.guardado_rapido=F2` — la pantalla de controles y el archivo
    // dicen la misma palabra, que es lo que §7.205 pide de una tabla con dos
    // caras. Y no hay partidor nuevo: se le saca el `_` y se delega en
    // `inputPartirClaveJugador`, que ya esta escrito y ya lo custodia la §14.
    //
    // ── LAS CAUSAS SON UN ENUM PROPIO, Y NO ES DUPLICAR `InputAvisoCausa` ───
    //
    // Se solapan en tres de seis y aun asi son dos conjuntos, porque **las
    // voces no se pueden compartir**: las de alla terminan en "Linea ignorada"
    // / "Bloque ignorado" y nombran un archivo de configuracion; esta sale
    // adentro de una linea de dialogo, donde no hay linea que ignorar y lo
    // accionable es otra cosa. Es §7.493 sobre el texto y no sobre la
    // repeticion: la voz la decide el SITIO, no el error.
    enum class InputDisplayCausa : unsigned char
    {
        Ok,
        NoEsNuestra,          // no empieza con `_tecla.` / `_key.`: no es asunto nuestro
        ClaveMalFormada,      // `${_tecla.vn}` o `${_tecla.vn.a.b}`
        DominioDesconocido,   // `${_tecla.foo.x}`
        DominioNoMostrable,   // `${_tecla.dev.dev_recargar}`
        VerboDesconocido      // typo del creador
    };

    struct InputDisplayRes
    {
        InputDisplayCausa causa = InputDisplayCausa::NoEsNuestra;
        std::string dominio;        // CANONICO ("" si no se resolvio)
        std::string verbo;          // CANONICO ("" si no se resolvio)
        // La GRAFIA que escribio el creador, no la canonica: decide la palabra
        // del vacio (`ninguna` / `none`, fork 4a) y la voz. Es `G2`/`Y-VN-01`
        // otra vez — el lector de esto es una PERSONA leyendo su propio guion.
        bool        enIngles = false;
        std::string dominioCrudo;   // tal cual lo escribio, para la voz
        std::string verboCrudo;
    };

    // `_tecla.vn.guardado_rapido` -> { Ok, "vn", "guardado_rapido", false }
    //
    // **`dev` se rechaza, y no es simetria con `CP3` porque si:** sus dos sitios
    // de `Engine.cpp` llevan escrito "se compila fuera en build de export", asi
    // que un `${_tecla.dev.dev_recargar}` pintaria `F8` en el build del creador
    // y seguiria pintando `F8` en el del jugador, donde esa tecla ya no hace
    // nada. Un display que miente es exactamente lo que esta variable existe
    // para evitar. Se rechaza como DOMINIO y se reconoce como PALABRA, que es
    // la misma pedagogia que `inputDominioDe` ya firma.
    inline InputDisplayRes inputPartirNombreDisplay(const std::string& nombre)
    {
        InputDisplayRes r;

        // Todo lo que decide "es nuestra" lo decide `inputPartirClaveJugador`:
        // acá sólo se le saca el `_` de las variables de sistema y se delega.
        //
        // **Escribir aca una segunda comprobacion del prefijo era redundante, y
        // lo midio el intento de romperla:** con la guarda propia aflojada a
        // `_tecla` (sin punto) la §17 seguia VERDE entera, porque quien rechaza
        // `_teclas` es el `rfind("tecla.", 0)` de alla. Dos comprobaciones de la
        // misma cosa no son cinturon y tirantes: son la segunda tabla de §7.442,
        // y la que se desincroniza es la que nadie mira. Queda UNA.
        //
        // Lo que se hereda con ella, y no se re-declara: la comparacion BYTE A
        // BYTE. `${_Tecla.vn.x}` no es nuestra y se va sin avisar, por la misma
        // linea que hace mudo al `Tecla.` de `CP4` - misma ficha `FOLD-PARTIDO`,
        // y el dia que se resuelva se resuelve para las dos de una.
        if (nombre.empty() || nombre[0] != '_') return r;

        const InputClaveJugador c = inputPartirClaveJugador(nombre.substr(1));
        if (!c.nuestra) return r;

        r.enIngles = (nombre.rfind("_key.", 0) == 0);

        if (!c.formada) { r.causa = InputDisplayCausa::ClaveMalFormada; return r; }

        r.dominioCrudo = c.dominio;
        r.verboCrudo   = c.verbo;

        const InputDominioRes dom = inputDominioDe(c.dominio);
        if (dom.estado == InputDominioEstado::Desconocido) {
            r.causa = InputDisplayCausa::DominioDesconocido; return r;
        }
        if (dom.estado == InputDominioEstado::NoRemapeable) {
            r.causa = InputDisplayCausa::DominioNoMostrable; return r;
        }

        const std::string verbo = inputVerboDe(c.verbo, dom.canonico);
        if (verbo.empty()) {
            r.dominio = dom.canonico;   // el dominio SI resolvio: la voz lo usa
            r.causa   = InputDisplayCausa::VerboDesconocido;
            return r;
        }

        r.causa   = InputDisplayCausa::Ok;
        r.dominio = dom.canonico;
        r.verbo   = verbo;
        return r;
    }

    // La palabra que nombra el vacio, en la grafia que el creador escribio
    // (fork 4a). **La regla Cuphead del §12:** una accion sin tecla no se
    // muestra como un hueco —"salta con   ", honesto e indistinguible de un
    // bug de render— sino con la MISMA palabra que la desliga en el bloque
    // `teclas {}`. Igual de honesta y ademas depurable.
    //
    // El §12 firma "segun idioma activo" y **no hay idioma activo**:
    // `DISENO_IDIOMAS.md` sigue sin implementar. Lo que si existe y decide es
    // §7.492 — a una PERSONA se le contesta en SU grafia.
    // > ⚠ **Su gatillo llego** (`DI-7`, idiomas `CP3`, 2026-08-14). Y la ficha
    // > se pago PARTIENDO la funcion, no cambiandola: resulta que sus dos
    // > callers estan en lados OPUESTOS de `DI-7`.
    // >
    // >   - `inputDisplayDeTeclas` la usa para lo que LEE EL JUGADOR -> pasa a
    // >     seguir el idioma activo, y vive abajo con otro nombre.
    // >   - `inputVozDelDisplay` la usa dentro de un AVISO -> se queda tal
    // >     cual, porque `DI-7` firma que *el parser, el disco y los
    // >     diagnosticos conservan la grafia del CREADOR*. A una persona se le
    // >     contesta en SU grafia (7.492), y eso no cambio.
    // >
    // > O sea que `tw.enIngles` no era un bug ni se borra: **deja de ser el
    // > selector del idioma del jugador y sigue siendo el recibo de grafia.**
    //
    // ESTA es la mitad de DIAGNOSTICO. No la llames desde un display.
    inline std::string inputPalabraSinTecla(bool enIngles)
    {
        return enIngles ? std::string("none") : std::string("ninguna");
    }

    // ── DI-7: el nombre de una tecla, EN EL IDIOMA DEL JUGADOR ──────────────
    //
    // Entra la grafia CANONICA (la que `inputNombreDe` devuelve y la que vive
    // en `settings.eclips`) y sale lo que se pinta. Es un LOOKUP, no contenido
    // nuevo: la tabla ya trae las dos grafias desde `CP0` -26 de las 40 filas
    // canonicas con su par ingles, y 14 (`TAB`, `F1`-`F12`, la eñe) marcadas
    // `en == nullptr` porque son la misma palabra en los dos idiomas-. Un
    // tercer idioma las cubre desde su `.lang`, igual que al resto del chrome.
    //
    // Las letras y los digitos NO son fila sino REGLA, asi que caen al `return`
    // final y salen tal cual. Es correcto: `Q` es `Q` en todos los idiomas.
    inline std::string inputNombreDisplay(const std::string& canonico,
                                          const LangStore* store)
    {
        for (const InputKeyName& e : inputTablaNombres()) {
            if (e.alias) continue;
            if (canonico == e.es) return langTr(store, e.es, e.en);
        }
        return canonico;
    }

    // Que se pinta, dada la lista que la tabla contesta. Pura y sin manager:
    // la lista la trae el caller (`teclasDe`), igual que `inputHeldDeTabla`
    // recibe el predicado en vez de llamar a SDL.
    //
    // **La PRIMERA, y eso es una perdida declarada (fork 5a):** 13 de las 32
    // filas del default traen mas de una tecla —`avanzar` trae cinco—, asi que
    // un tutorial que diga "Presiona ESPACIO" es cierto e incompleto. Es lo que
    // un tutorial quiere (Cuphead nombra una sola) y es lo que el §12 firma.
    // Mostrar todas queda fichado con gatillo: el primer creador que lo pida.
    //
    // > ⚠ **`DI-7` le cambio el segundo parametro** (idiomas `CP3`): era
    // > `bool enIngles` -la grafia que escribio el CREADOR- y ahora es el
    // > idioma del JUGADOR. Es el corazon de la ficha `L-7`: el creador que
    // > escribe `control tecla.vn.avanzar` en ingles no decide en que idioma
    // > juega el que compro el juego.
    inline std::string inputDisplayDeTeclas(const std::vector<std::string>& teclas,
                                            const LangStore* store)
    {
        if (teclas.empty()) return langTr(store, "ninguna", "none");
        return inputNombreDisplay(teclas.front(), store);
    }

    // La voz, devuelta en vez de impresa, como su hermana de `CP3`/`CP4`.
    //
    // **Ninguna dice "linea ignorada", y ahi esta la diferencia con la otra:**
    // el creador no perdio un renglon de configuracion, tiene una palabra mal
    // escrita adentro de una linea que el jugador va a leer. Lo accionable es
    // la forma y el ejemplo, y por eso las cuatro terminan mostrandola.
    inline std::string inputVozDelDisplay(const InputDisplayRes& r,
                                          const std::string& nombre)
    {
        const std::string ej = r.enIngles ? "${_key.vn.quicksave}"
                                          : "${_tecla.vn.guardado_rapido}";

        switch (r.causa) {
        case InputDisplayCausa::ClaveMalFormada:
            return "[interpolacion] Aviso: no entiendo '${" + nombre + "}'.\n"
                   "  La tecla de una accion se escribe en TRES partes separadas\n"
                   "  por punto: '${_tecla.<dominio>.<accion>}'.\n"
                   "  Por ejemplo: '" + ej + "'. Dejo el literal.";

        case InputDisplayCausa::DominioDesconocido:
            return "[interpolacion] Aviso: no conozco el dominio de teclas '"
                 + r.dominioCrudo + "' en '${" + nombre + "}'.\n"
                   "  Los que se pueden mostrar son 'vn' y 'juego' ('game').\n"
                   "  Dejo el literal.";

        case InputDisplayCausa::DominioNoMostrable:
            return "[interpolacion] Aviso: el dominio '" + r.dominioCrudo
                 + "' existe, pero NO se muestra.\n"
                   "  Son las teclas de desarrollo (F8 recargar, F10 vsync, F3\n"
                   "  hotspots) y no viajan al build que juega tu jugador, asi que\n"
                   "  mostrarlas mentiria. Dejo el literal.";

        case InputDisplayCausa::VerboDesconocido:
        default:
            return "[interpolacion] Aviso: '" + r.verboCrudo
                 + "' no es una accion del dominio '" + r.dominio + "'.\n"
                   "  Dejo el literal '${" + nombre + "}' para que se vea en\n"
                   "  pantalla: una accion sin tecla se muestra '"
                 + inputPalabraSinTecla(r.enIngles) + "', no vacia.";
        }
    }

    struct InputCapaJugador
    {
        InputTabla              tabla;
        std::vector<InputAviso> avisos;
    };

    // Los pares crudos -> la capa, delegando la ley entera en `CP3`.
    //
    // **Los pares tienen que llegar ORDENADOS**, y de eso depende una ley, no
    // una comodidad: `tecla.vn.guardado_rapido` y `key.vn.quicksave` son la
    // MISMA accion escrita en dos idiomas, asi que el parser avisa
    // `VerboRepetido` y se queda con la ultima. Sin orden estable, cual gana
    // seria un sorteo distinto en cada corrida. Lo garantiza quien lee
    // (`SaveManager::keysWithPrefix`), por el mismo motivo por el que
    // `writeToFile` ordena al escribir.
    //
    // La linea va en 0 a proposito: `settings.eclips` se lee POR CLAVE y no por
    // renglon, asi que no hay numero honesto que dar. `inputVozDelAviso` ya
    // calla el "linea N" cuando es 0, y eso esta probado desde `CP3` (11.22).
    inline InputCapaJugador inputParsearCapaJugador(
        const std::vector<std::pair<std::string, std::string>>& pares)
    {
        InputCapaJugador out;
        std::vector<InputFilaCreador> filas;

        for (const std::pair<std::string, std::string>& par : pares)
        {
            const InputClaveJugador c = inputPartirClaveJugador(par.first);

            // Una clave que no es nuestra no es asunto nuestro: `music_vol` y
            // `window_w` viven en el mismo archivo y callarse es lo correcto.
            if (!c.nuestra) continue;

            if (!c.formada) {
                InputAviso a;
                a.causa   = InputAvisoCausa::ClaveMalFormada;
                a.detalle = par.first;
                out.avisos.push_back(a);
                continue;
            }

            InputFilaCreador f;
            f.dominio = c.dominio;
            f.verbo   = c.verbo;
            f.teclas  = par.second;
            f.linea   = 0;
            filas.push_back(f);
        }

        const InputCapaCreador capa = inputParsearCapaCreador(filas);
        out.tabla = capa.tabla;
        for (const InputAviso& a : capa.avisos) out.avisos.push_back(a);

        return out;
    }

    // ── CP9b: LA INVERSA DE ARRIBA - el rebind del jugador ──────────────────
    //
    // `inputParsearCapaJugador` toma los pares crudos y arma la capa; esto toma
    // los mismos pares y devuelve los pares NUEVOS con una accion remapeada.
    //
    // **Vive al lado de su inversa y no en un header propio, y esa ubicacion es
    // una decision que el codigo corrigio:** las dos funciones tienen que saber
    // los mismos dos prefijos (`tecla.` / `key.`) y la misma nocion de *"la
    // misma accion escrita en dos idiomas"*. Separarlas seria la segunda tabla
    // de §7.442, con el agravante de que aca la desincronizacion **no se ve**:
    // el rebind seguiria escribiendo, sobre la clave equivocada.
    //
    // ── POR QUE NO ES "agregar un renglon" ──────────────────────────────────
    //
    // Porque el jugador puede tener ya una linea para esa accion, y puede
    // tenerla **en la otra grafia**. Dos cosas distintas pasan si se ignora eso,
    // y solo una es cosmetica:
    //
    //   1. **Ensucia.** `inputParsearCapaJugador` ya cubre el choque -avisa
    //      `VerboRepetido` y se queda con la ultima, con el orden garantizado
    //      por quien lee-, asi que agregar siempre no rompe: deja un renglon
    //      mas por remapeo y un aviso por renglon en cada arranque.
    //
    //   2. **REVIERTE EL REMAPEO LA MITAD DE LAS VECES, y ESO si es mudo.**
    //
    //      El mecanismo no es "la vieja quedo debajo": `Settings::load` junta
    //      los dos prefijos y los **ordena alfabeticamente por la clave entera**
    //      (`Settings.h:205`), asi que el que gana -el ultimo- no es el que se
    //      agrego despues, **es el que ordena mas alto**. Y entre dos grafias de
    //      la misma accion eso lo decide el abecedario:
    //
    //          tecla.vn.guardado_rapido  <  tecla.vn.quicksave    -> gana la VIEJA
    //          tecla.vn.advance          <  tecla.vn.avanzar      -> gana la NUEVA
    //
    //      O sea que agregar-siempre no falla ni anda: **sortea**. El jugador
    //      remapea, lo ve aplicado en pantalla, cierra el juego, y al volver su
    //      tecla es la de antes o la nueva segun como se llame su verbo. Una
    //      falla consistente se caza en el primer feel-test; esta no.
    //
    //      **Por eso las sobrantes se dejan caer.** Es la unica parte de esta ley
    //      que evita una falla silenciosa, y el harness no escribe cuantos
    //      verbos estan del lado malo del sorteo: los **cuenta** (§7.397).
    //
    // ── LA GRAFIA ES DEL JUGADOR (§7.492), Y UNA LINEA NUEVA NO TIENE DUENO ──
    //
    // Si el archivo ya dice `key.game.jump`, se escribe sobre ESA linea y en ESA
    // grafia: es la eleccion de una persona sobre su propio archivo y no hay
    // razon para pisarla.
    //
    // **Cuando no hay ninguna linea, no hay eleccion del jugador que respetar**,
    // asi que la nueva sale canonica (`tecla.<dominio>.<verbo>`) y no se le
    // pregunta al widget. Se probo el camino contrario -que el `control` del
    // creador decidiera la grafia- y pedia una tabla inversa ES->EN que no
    // existe, para producir una linea en un archivo que el creador no lee. Es
    // el corolario de §2.9: la mejor forma de no equivocarse eligiendo es no
    // tener que elegir.
    //
    // Lo que NO se traduce es el VALOR: `inputNombreDe` contesta siempre la
    // grafia ES (`ESPACIO`, nunca `SPACE`), asi que sobre una linea inglesa
    // preexistente queda `key.game.jump=ESPACIO`. **Es la conducta que el
    // display YA tiene** -`${_key.game.jump}` pinta `ESPACIO` desde `CP9a`-, se
    // hereda en vez de ampliarse, y con la clave canonica de arriba el caso
    // nuevo no la toca. Ficha `NOMBRE-DE-TECLA-SIN-INGLES`.
    struct InputRebindRes
    {
        std::vector<std::pair<std::string, std::string>> pares;
        int  reemplazadas = 0;   // lineas existentes que hablaban de esta accion
        bool agregada     = false;
    };

    // `valor` es lo que va a la derecha del `=`, tal cual: `"F2"`, `"ESPACIO,W"`
    // o la palabra que desliga. Esta funcion no la interpreta -eso es de
    // `inputParsearCapaJugador`, que corre despues sobre el resultado-, para que
    // no haya dos sitios que decidan que significa un valor.
    inline InputRebindRes inputRebindJugador(
        const std::vector<std::pair<std::string, std::string>>& pares,
        const std::string& dominioCanonico,
        const std::string& verboCanonico,
        const std::string& valor)
    {
        InputRebindRes out;
        out.pares.reserve(pares.size() + 1);

        for (const std::pair<std::string, std::string>& par : pares)
        {
            const InputClaveJugador c = inputPartirClaveJugador(par.first);

            // "Habla de esta accion" se decide CANONICAMENTE, que es todo el
            // punto: `key.game.jump` y `tecla.juego.saltar` son la misma fila.
            bool mia = false;
            if (c.nuestra && c.formada)
            {
                const InputDominioRes d = inputDominioDe(c.dominio);
                if (d.estado == InputDominioEstado::Remapeable
                    && d.canonico == dominioCanonico
                    && !verboCanonico.empty()
                    && inputVerboDe(c.verbo, d.canonico) == verboCanonico)
                    mia = true;
            }

            if (!mia)
            {
                // Todo lo demas pasa intacto, y no solo las claves ajenas
                // (`music_vol`, `window_w`): tambien las `tecla.*` malformadas.
                // `CP4` las conserva a proposito -"el aviso le dice al jugador
                // que corregir y la linea sigue ahi"- y un rebind de otra accion
                // no es motivo para borrarle una linea que no nombro.
                out.pares.push_back(par);
                continue;
            }

            ++out.reemplazadas;
            if (out.reemplazadas == 1)
                out.pares.push_back({ par.first, valor });   // su clave, tal cual
            // las demas se caen: ver el punto 2 del comentario de arriba
        }

        if (out.reemplazadas == 0)
        {
            out.pares.push_back({ "tecla." + dominioCanonico + "." + verboCanonico,
                                  valor });
            out.agregada = true;
        }

        return out;
    }

    // ── CP9b: LA LISTA NUEVA - "reemplaza la primera, conserva el resto" ────
    //
    // El fork 6a, y lo decidio una medicion mas el ejemplo del fundador.
    //
    // **La medicion:** de las 15 filas que `controles.ecl` muestra, **ocho traen
    // mas de una tecla** (`avanzar` trae CINCO). La pantalla pinta LA PRIMERA
    // -ficha `TECLA-DISPLAY-SOLO-LA-PRIMERA`, firmada en `CP9a`-, asi que una
    // captura que reemplazara la lista entera borraria `ENTER`, `ENTER_NUM`, `Z`
    // y **`CLIC`** mostrando la palabra `ESPACIO`. O sea: **remapear "Avanzar"
    // mataria el click para avanzar**, que es como se juega una VN.
    //
    // Esa ficha decia *"un tutorial nombra una tecla, no una lista"*, y es cierto
    // para LEER. Para ESCRIBIR, mostrar una y reemplazar cinco es una mentira
    // con consecuencias.
    //
    // **El ejemplo del fundador, que es el argumento mas fuerte y mira al
    // futuro:** en un mando, `avanzar` va a ser
    // `[ESPACIO, ENTER, CLIC, BOTON_A]`. Con la ley destructiva, un jugador que
    // remapea la tecla del TECLADO le desliga el MANDO sin enterarse. Con esta,
    // eso es imposible por construccion. *(Y su corolario: un build de consola
    // recorta la lista a un boton sin tocar un `.ecl`.)*
    //
    // El dedup no es prolijidad: sin el, capturar `ENTER` sobre
    // `[ESPACIO, ENTER, ...]` dejaria `ENTER` dos veces en la misma fila.
    inline std::vector<std::string> inputListaConPrimera(
        const std::vector<std::string>& actuales, const std::string& nueva)
    {
        std::vector<std::string> out;
        if (nueva.empty()) return out;          // vaciar es el otro gesto (`SUPR`)

        out.push_back(nueva);
        for (size_t i = 1; i < actuales.size(); ++i)
            if (actuales[i] != nueva) out.push_back(actuales[i]);

        return out;
    }

    // ── CP9b: LAS COLISIONES, QUE EL §13 PROMETE Y EL CODIGO NO TENIA ───────
    //
    // El §13 escribe *"UI de conflictos de binding (dos verbos, misma tecla,
    // mismo dominio): v1 = ultima gana + aviso"*. **El aviso no existia** -cero
    // ocurrencias en el manager-, y hasta hoy daba igual: para fabricar una
    // colision habia que editar `settings.eclips` a mano. `CP9b` la pone a dos
    // clicks, asi que el aviso entra con su caller.
    //
    // **No detecta "es un bug": detecta "compartiste una tecla".** Compartirla
    // es LEGAL y a veces querido -`ARRIBA` sirve al historial y al foco, y la
    // ley 5 esta escrita para eso-, y quien desempata es la Capa 3 por FASE. Por
    // eso esto no bloquea el rebind: **habla y deja pasar**, que es el patron
    // soft-lock de §7.103 (*el motor avisa, no inventa*).
    //
    // Toma la lista que `verbosDe` ya devuelve, para no re-derivar la busqueda
    // ni conocer la tabla: aca solo vive *"sacate a vos mismo de la lista"*.
    inline std::vector<std::string> inputVerbosQueComparten(
        const std::vector<std::string>& verbosDeEsaTecla,
        const std::string& verboPropio)
    {
        std::vector<std::string> out;
        for (const std::string& v : verbosDeEsaTecla)
            if (v != verboPropio) out.push_back(v);
        return out;
    }

    // ── EL MANAGER ──────────────────────────────────────────────────────────
    //
    // Tres capas de datos y una resuelta. `aplicarCascada` es toda la ley 4, y
    // esta escrita UNA vez para las dos capas de override: el merge del creador
    // y el del jugador son la misma operacion aplicada en orden.
    class InputManager
    {
    public:
        using Tabla = InputTabla;

        // Arranca con el default puro y las dos capas vacias. En CP0 nadie llama
        // a los setters, asi que esta es la unica forma en que existe - que es la
        // ley 1 hecha constructor.
        InputManager() { aplicarCascada(); }

        // Capa del creador (CP3: bloque `teclas {}` de `proyecto.eclips`).
        void setCapaCreador(const Tabla& t) { m_creador = t; aplicarCascada(); }

        // Capa del jugador (CP4: `tecla.*` de `settings.eclips`). Gana.
        void setCapaJugador(const Tabla& t) { m_jugador = t; aplicarCascada(); }

        // ── Capa 1: traduccion ──────────────────────────────────────────────
        //
        // TODOS los verbos que ese evento activa en ese dominio (ley 5). El
        // orden es alfabetico y estable (`std::map`): quien consume elige por
        // FASE, no por suerte.
        std::vector<std::string> resolverTodos(const SDL_Event* e,
                                               const std::string& dominio) const
        {
            return verbosDe(inputBindDeEvento(e), dominio);
        }

        // El primero. Existe para los call-sites donde el dominio no tiene
        // colisiones. **Donde hay colision, usar esto es un bug** - por eso el
        // harness lo corre contra `ARRIBA` y deja el resultado escrito en un
        // assert.
        //
        // > ⚠ **Este parrafo decia "(todo `juego`, todo `dev`, y casi todo
        // > `vn`)" y `CP2` volvio falsa la primera mitad.** Con `atacar: F,
        // > ESPACIO` e `interactuar: E, ENTER, ESPACIO`, el `ESPACIO` del
        // > dominio `juego` resuelve a TRES verbos (`atacar`, `interactuar`,
        // > `saltar`) y el orden del `std::map` los da alfabeticamente, o sea
        // > que `resolver()` contestaria `atacar` **tambien en side-scroll**:
        // > el salto muerto sin un warning, calcado del bug que la ley 5
        // > describe para `ARRIBA` en `vn`. Lo unico que hoy sigue sin
        // > colisiones es `dev`. **`Gameplay2DVerbRoute.h` toma
        // > `resolverTodos`, igual que el VN.**
        std::string resolver(const SDL_Event* e, const std::string& dominio) const
        {
            const std::vector<std::string> v = resolverTodos(e, dominio);
            return v.empty() ? std::string() : v.front();
        }

        // Mismo camino, sin evento: lo que CP3/CP4 necesitan para validar, y lo
        // que le evita al harness fabricar un `SDL_Event` en cada assert.
        std::vector<std::string> verbosDe(const InputBind& b,
                                          const std::string& dominio) const
        {
            std::vector<std::string> out;
            if (b.kind == InputBind::Kind::Ninguna) return out;

            const auto dom = m_resuelta.find(dominio);
            if (dom == m_resuelta.end()) return out;

            for (const auto& par : dom->second)
                for (const InputBind& cand : par.second)
                    if (cand == b) { out.push_back(par.first); break; }

            return out;
        }

        // ── Introspeccion (`${_tecla.<dom>.<accion>}` y `control tecla.*`) ──
        //
        // > ⚠ **Este renglon decia `${_tecla_*}` y `control tipo=tecla`, y las
        // > DOS formas se descartaron al implementarlas** (`CP9a` §7.497 el
        // > separador, `CP9b` las claves: `tipo=` ya era el override de render
        // > del `control` y `accion=` es de `ranura`). Se corrige porque un
        // > comentario que nombra una sintaxis inexistente manda a buscar algo
        // > que no esta — y este es justo el sitio al que llega quien busca.
        //
        // Devuelve NOMBRES, no binds: quien lo consume es una pantalla. Vacio si
        // el verbo no existe o si quedo desligado.
        std::vector<std::string> teclasDe(const std::string& verbo,
                                          const std::string& dominio) const
        {
            std::vector<std::string> out;
            const auto dom = m_resuelta.find(dominio);
            if (dom == m_resuelta.end()) return out;
            const auto v = dom->second.find(verbo);
            if (v == dom->second.end()) return out;

            for (const InputBind& b : v->second) {
                const std::string n = inputNombreDe(b);
                if (!n.empty()) out.push_back(n);
            }
            return out;
        }

        bool tieneVerbo(const std::string& verbo, const std::string& dominio) const
        {
            const auto dom = m_resuelta.find(dominio);
            if (dom == m_resuelta.end()) return false;
            return dom->second.find(verbo) != dom->second.end();
        }

        const Tabla& tablaResuelta() const { return m_resuelta; }

        // Los dominios donde la red §8 tuvo que re-inyectar `ESC` en la ultima
        // cascada. Vacio en todo proyecto que no desligue `menu`.
        //
        // Se devuelve en vez de imprimirse por lo mismo que los avisos del
        // parser: este header no tiene `<iostream>`, y el harness necesita
        // afirmar QUE paso, no cazar stderr. Quien habla es el cable.
        const std::vector<std::string>& dominiosRescatados() const { return m_rescatados; }

        // La tabla default ya parseada. Separada de `m_resuelta` porque el
        // guardian de no-op del harness necesita comparar las dos.
        static const Tabla& tablaDefault()
        {
            static const Tabla kD = construirDefault();
            return kD;
        }

    private:
        static Tabla construirDefault()
        {
            Tabla t;
            for (const InputDefaultRow& r : inputTablaDefault()) {
                std::vector<InputBind> binds;
                for (const std::string& n : inputPartirLista(r.teclas)) {
                    const InputBind b = inputBindDe(n);
                    if (b.kind != InputBind::Kind::Ninguna) binds.push_back(b);
                }
                t[r.dominio][r.verbo] = binds;
            }
            return t;
        }

        // Ley 4, y es toda la cascada: por-ACCION, no por-tabla. Un verbo que la
        // capa de arriba no menciona hereda; uno que si menciona se REEMPLAZA,
        // lista incluida. Una lista VACIA es "desligado" y se guarda como tal:
        // es la diferencia entre `auto: ninguna` (el creador lo apago) y no
        // escribir `auto` (hereda) - dos cosas distintas que un `erase`
        // confundiria en una sola.
        // La red §8 corre **al final y siempre**, no solo cuando alguien carga
        // una capa: es "tras cada resolucion de cascada" al pie de la letra, y
        // eso incluye el constructor. Sobre el default es un NO-OP exacto
        // (`menu` trae `ESC` en los dos dominios), y ese no-op tiene guardian
        // propio: si cayera, el lote no agrego una red - le cambio la tabla al
        // motor entero.
        void aplicarCascada()
        {
            m_resuelta = tablaDefault();
            fundir(m_creador);
            fundir(m_jugador);
            m_rescatados = inputRedDeSeguridad(m_resuelta);
        }

        void fundir(const Tabla& capa)
        {
            for (const auto& dom : capa)
                for (const auto& verbo : dom.second)
                    m_resuelta[dom.first][verbo.first] = verbo.second;
        }

    public:
        // ── Capa 1: polleo (CP2, fork 3b) ───────────────────────────────────
        //
        // **Declaradas aca y DEFINIDAS en `InputManager.cpp`**, que es lo unico
        // de este header que no es `inline`. Las dos piden funciones de SDL
        // (`SDL_GetKeyboardState`, `SDL_GetScancodeFromKey`) y `E-6` firmo por
        // eso que "`InputManager.cpp` es su casa".
        //
        // **`CP2` enmienda esa firma a medias, y es el fork 3b:** lo que baja al
        // `.cpp` es el CABLE -leer el teclado de SDL-, no la LEY. La ley es
        // `inputHeldDeTabla`/`inputEjeDeTabla`, aca abajo, puras y con el
        // predicado como parametro: el harness las prueba **sin linkear SDL**.
        // `E-6` se firmo antes de que ese patron tuviera cinco precedentes en el
        // repo (`vnAtajoDeVerbos`, `QteGate`, `AppearPolicy`, `GroupAlpha`,
        // `TextBudget`); esto no es una desviacion silenciosa, es la enmienda.
        //
        // Lo que queda sin custodia es el cable, y son dos lineas por funcion.
        bool  held(const std::string& verbo,  const std::string& dominio) const;
        float eje (const std::string& nombre, const std::string& dominio) const;

    private:
        Tabla m_creador;   // CP3 - la llena `setCapaCreador` desde el manifest
        Tabla m_jugador;   // CP4 - vacia todavia
        Tabla m_resuelta;

        // Lo que la red §8 rescato en la ultima cascada (§8 del contrato).
        std::vector<std::string> m_rescatados;
    };

    // ── LA LEY DEL POLLEO, PURA (CP2, fork 3b) ──────────────────────────────
    //
    // `abajo` contesta si un bind esta presionado EN ESTE INSTANTE. En
    // produccion se lo arma `InputManager.cpp` contra `SDL_GetKeyboardState`;
    // en el harness es una lambda sobre un set fabricado, que es lo que vuelve
    // afirmable la ley sin arrastrar SDL.
    using InputPredicadoAbajo = std::function<bool(const InputBind&)>;

    // Un verbo esta "held" si CUALQUIERA de sus teclas lo esta. Es la misma ley
    // que el `LCTRL || RCTRL` del skip escribia a mano, generalizada: un verbo
    // con dos teclas responde a las dos.
    inline bool inputHeldDeTabla(const InputManager::Tabla& tabla,
                                 const std::string& verbo,
                                 const std::string& dominio,
                                 const InputPredicadoAbajo& abajo)
    {
        const auto dom = tabla.find(dominio);
        if (dom == tabla.end()) return false;
        const auto v = dom->second.find(verbo);
        if (v == dom->second.end()) return false;

        for (const InputBind& b : v->second)
            if (b.kind != InputBind::Kind::Ninguna && abajo(b)) return true;
        return false;
    }

    // ── LOS EJES: LECTURAS COMPUESTAS, NO ENTRADAS DE TABLA ─────────────────
    //
    // §3.1 lo firmo asi: `mover_x` no es un verbo, es la RESTA de dos. Por eso
    // esta tabla es chica y aparte - meter `mover_x` como fila del default
    // habria pedido ligarle una tecla, que es justo lo que un eje no tiene.
    //
    // **EL SIGNO ES LA MITAD QUE FALLA MUDA.** `pos - neg`, y quien manda es la
    // conducta viva de `Gameplay2DState::update`:
    //
    //     inputX += 1  con DERECHA   ->  `mover_x` positivo a la DERECHA
    //     inputY += 1  con ABAJO     ->  `mover_y` positivo hacia ABAJO
    //
    // Invertir `mover_y` compila, corre, y el personaje camina al reves. El
    // harness lo clava con su nombre.
    // ⚠ **LA COLUMNA `eje_en` LLEGO EN `CP8` (2026-08-11), Y CERRO EL UNICO
    // HUECO BILINGUE DE TODA LA SUPERFICIE DE INPUT.** Auditadas las dos tablas
    // grandes: 42 nombres de tecla + 31 verbos = 73 entradas, 22 sin alias, y
    // las 22 son legitimas -misma palabra en los dos idiomas (`TAB`, `SHIFT`,
    // `F1`..`F12`, `menu`, `auto`, `dev_vsync`) o jerga que `D-32` firmo que no
    // se traduce (`dash`, `hotspots`)-. **El eje no tenia alias por una razon
    // distinta y peor: esta struct no tenia DONDE ponerlo.** Un hueco por falta
    // de columna no se ve auditando valores, solo aparece cuando alguien
    // escribe el caller - que es lo que hizo `world.axis` de `CP8`.
    struct InputEjePar
    {
        const char* eje;
        const char* eje_en;   // nullptr = misma palabra en los dos idiomas
        const char* neg;      // resta
        const char* pos;      // suma
    };

    inline const std::vector<InputEjePar>& inputTablaEjes()
    {
        static const std::vector<InputEjePar> kEjes = {
            { "mover_x", "move_x", "mover_izquierda", "mover_derecha" },
            { "mover_y", "move_y", "mover_arriba",    "mover_abajo"   },
        };
        return kEjes;
    }

    // `CP8`: si el nombre denota un eje. Existe porque el consumidor de Lua
    // necesita distinguir *"pediste un eje que no tengo"* de *"el eje esta en
    // cero"*, y `inputEjeDeTabla` devuelve `0.0f` para los dos.
    //
    // Acepta las dos grafias, igual que la lectura.
    inline bool inputEjeExiste(const std::string& nombre)
    {
        for (const InputEjePar& p : inputTablaEjes())
            if (nombre == p.eje || (p.eje_en && nombre == p.eje_en))
                return true;
        return false;
    }

    // -1..1. Las dos teclas a la vez dan 0, igual que el `inputX` vivo, que
    // suma -1 y +1 sin desempatar.
    inline float inputEjeDeTabla(const InputManager::Tabla& tabla,
                                 const std::string& nombre,
                                 const std::string& dominio,
                                 const InputPredicadoAbajo& abajo)
    {
        for (const InputEjePar& p : inputTablaEjes())
        {
            // `CP8`: las DOS grafias. Antes comparaba solo contra `p.eje`, asi
            // que `move_x` -lo que le sale natural a quien escribio
            // `keys game { move_left A }`, que `CP3` SI acepta- devolvia cero
            // en silencio y el personaje no caminaba.
            if (nombre != p.eje && !(p.eje_en && nombre == p.eje_en)) continue;
            const float neg = inputHeldDeTabla(tabla, p.neg, dominio, abajo) ? 1.0f : 0.0f;
            const float pos = inputHeldDeTabla(tabla, p.pos, dominio, abajo) ? 1.0f : 0.0f;
            return pos - neg;
        }
        // Eje que no existe: 0, no un error - la ley es total a proposito.
        // **Quien avisa es el CONSUMIDOR**, con `inputEjeExiste`, y solo donde
        // el nombre lo escribe un humano (la Lua del creador). Meter el aviso
        // aca lo pondria en un sitio que corre 60 veces por segundo.
        return 0.0f;
    }

} // namespace eclips
