// ============================================================================
// VNActionSeal.h - EL SELLO DE ACCIONES
// ============================================================================
//
// Nucleo del contrato `DISENO_DESHABILITAR_ACCIONES.md` (`D-40`...`D-51`).
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// Ocultar un boton nunca fue apagarlo, y el motor ya lo decia por escrito:
// `Theme.h:1259` declara que `botones_visibles` es "solo visibilidad
// (render+hit-test). NO toca funcion - skip(Ctrl) / auto(A) / save(K/F5) siguen
// por teclado". Este header es la mitad que faltaba.
//
//     deshabilitar acciones          # el grueso: META + RITMO
//     habilitar historial            # lo refina
//
//     deshabilitar guardado_rapido, menu_guardar    # o solo fino
//
// ── POR QUE UN HEADER APARTE, Y NO DENTRO DE `VNExecutionPolicy.h` ──────────
//
// La seccion 5 del contrato ("como abre el primer lote") recomendaba meter el
// predicado en `VNExecutionPolicy.h`, y su argumento era bueno: ese archivo es
// SDL-free y ya lo incluye un harness, asi que el predicado nacia probable sin
// una TU nueva. **Se midio y no alcanza para justificarlo:**
//
//   · el lote necesita harness propio de todos modos (el guardian de no-op de
//     la trampa 3 y el control negativo de la trampa 5 son un archivo entero),
//     asi que lo unico que se ahorraba era una linea del `.bat`;
//   · `VNExecutionPolicy.h` se titula "reglas puras del PUMP DE COMANDOS VN" y
//     son 42 lineas. Meterle adentro una tabla de vocabulario de nueve verbos
//     vuelve mentiroso su propio titulo - que es exactamente §7.353, la ficha
//     que este repo ya pago: *una palabra que nombra dos objetos hace que una
//     promesa PAREZCA cubierta*.
//
// Vive junto a `VNExecutionPolicy.h` y es su vecino de concepto, no su
// inquilino: aquel contesta "este Advance commitea?", este contesta "este verbo
// procede?". Mismo patron que `AppearPolicy.h` / `BoxPersistence.h`.
//
// ── LA TRAMPA 2 DEL CONTRATO NO SE DISPARA (medido) ─────────────────────────
//
// El contrato anotaba que `vnAcceptsDialogueAdvance` iba a cambiar de firma y
// que lo incluye `harness_reloj_dialogo`. **No cambia**: `avanzar` es familia
// Salida (`D-41`) y no se sella nunca, asi que ese predicado no se toca. Lo que
// si se sella de la familia RITMO -auto y skip- se apaga en su PROPIA fuente,
// no en el choke-point del avance. Se anota para que nadie lo busque.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   NO-OP POR DEFECTO (`D-40`). Una mascara recien nacida es 0: nada
//           sellado. Sin un solo `deshabilitar` escrito la conducta del motor
//           es byte-identica. Si esta ley cae, el lote no agrego una feature:
//           le cambio los permisos a todo el corpus
//   ley 2   EL SELLO ES SOBRE LA ACCION, NUNCA SOBRE LA TECLA (`D-47` = `F4`
//           del atomo). `F8` ya es `dev_recargar` y la rueda ya es tecla
//           ligable: un sello escrito sobre `SDLK_F9` cuida una puerta que el
//           creador puede mudar. Y de yapa UN verbo tapa las TRES puertas del
//           historial (rueda, boton, tecla), donde tres sellos por tecla dejan
//           una afuera **que se ve identica en pantalla**
//   ley 3   TRES FAMILIAS, Y SOLO DOS SON SELLABLES (`D-41`). `acciones` =
//           META + RITMO. La familia SALIDA (`avanzar`, `opcion_1..4`,
//           `foco_*`) esta **estructuralmente fuera**: no tiene bit. No es una
//           lista que alguien pueda editar - no hay donde escribirla
//   ley 4   GRUESO + REFINABLE (`D-48`). El grueso y el fino son el MISMO
//           algebra, y el orden secuencial decide (§7.16), igual que
//           `botones_visibles no` + `boton_menu_visible si` del lado de la
//           visibilidad
//   ley 5   LOS SELLOS SE SUMAN, NUNCA RESTAN (`D-45`).
//               permitido = rol && modal && cinematica && sello
//           Un `habilitar` **nunca otorga** un permiso que otra capa negaba:
//           las otras tres capas viven en sus propios sitios y este predicado
//           es solo un `&&` mas. `habilitar guardado_rapido` en un menu-titulo
//           NO destraba el guard §3.3 (no hay partida que guardar)
//   ley 6   EL MOTOR NO DEJA SELLAR LA ULTIMA SALIDA, Y AVISA (`D-46`). Un
//           `deshabilitar opcion_1` no se ejecuta y habla. La voz es de
//           PARSE-TIME porque el creador esta escribiendo cuando se equivoca
//   ley 7   SELLAR UN VERBO DE RITMO LO APAGA, NO SOLO TRABA SU INTERRUPTOR.
//           **No estaba en el contrato: salio al cablear, y sin ella el lote
//           tiene un agujero mudo.** META y RITMO no son la misma clase de
//           cosa: un verbo META es un EVENTO (abrir un menu) y sellarlo es no
//           dejar que ocurra; un verbo RITMO es un MODO (el texto avanza solo)
//           y trabarle el interruptor deja corriendo el que ya estaba
//           encendido. `modalidad-aviso` resuelve el mismo problema sellando el
//           Advance ("incluido el auto ya encendido", `VNScriptState.h:1430`),
//           camino que aca esta PROHIBIDO por la ley 3. Ver `vnRitmoQueda`
//
// ── LO QUE ESTE HEADER NO DECIDE ────────────────────────────────────────────
//
// La VIDA UTIL (`D-43`: escena nueva / `cargar_script` / cargar save apagan el
// sello) y el hecho de que **no viaje al save** (`D-51`, cero claves, cero
// bump) son del dueno de la mascara, o sea `VNScriptState`. Aca no hay reloj.
// ============================================================================

#pragma once

#include <string>

namespace eclips
{
    // La mascara: un bit por verbo sellable. Nace en 0 (ley 1).
    using VNActionMask = unsigned int;

    // ── Los verbos del dominio `vn` que un sello PUEDE apagar ───────────────
    //
    // Los nombres canonicos y su par en ingles salen de la tabla del atomo
    // (`DISENO_ATOMO_TECLADO.md` §4), que los firmo un mes antes que este lote.
    // La UNICA que no esta en esa tabla es `Config`, y esta aca por firma del
    // fundador (fork 2A de la sesion `lote-deshabilitar`): no tiene tecla, asi
    // que un censo de TECLAS no podia verla, pero es un servicio META puro y se
    // alcanza por la misma puerta que `guardar` y `cargar`.
    //
    // **La familia SALIDA no aparece.** Esa es la ley 3, y esta escrita como
    // ausencia a proposito: lo que no tiene bit no se puede sellar ni por
    // error, ni por un find-replace, ni por una lista que alguien amplie.
    enum class VNAction : VNActionMask
    {
        // ── META / SERVICIOS ──
        GuardadoRapido = 1u << 0,   // guardado_rapido / quicksave
        CargaRapida    = 1u << 1,   // carga_rapida    / quickload
        MenuGuardar    = 1u << 2,   // menu_guardar    / save_menu
        MenuCargar     = 1u << 3,   // menu_cargar     / load_menu
        Historial      = 1u << 4,   // historial       / history
        Menu           = 1u << 5,   // menu
        Config         = 1u << 6,   // config          / settings
        // ── RITMO ──
        Auto           = 1u << 7,   // auto
        SaltarTexto    = 1u << 8    // saltar_texto    / skip
    };

    inline constexpr VNActionMask vnBit(VNAction a)
    {
        return static_cast<VNActionMask>(a);
    }

    // ── Las dos familias sellables, y el grueso ─────────────────────────────
    //
    // El grueso se COMPONE de las dos familias en vez de escribirse como un
    // literal (`0x1FF`): asi, el dia que entre un verbo nuevo, entra al grueso
    // solo. Un literal se desincroniza en silencio, que es la enfermedad de
    // §7.309 en su version mas barata.
    inline constexpr VNActionMask vnMetaMask()
    {
        return vnBit(VNAction::GuardadoRapido) | vnBit(VNAction::CargaRapida)
             | vnBit(VNAction::MenuGuardar)    | vnBit(VNAction::MenuCargar)
             | vnBit(VNAction::Historial)      | vnBit(VNAction::Menu)
             | vnBit(VNAction::Config);
    }

    inline constexpr VNActionMask vnRitmoMask()
    {
        return vnBit(VNAction::Auto) | vnBit(VNAction::SaltarTexto);
    }

    // `acciones` / `actions` - el grueso de `D-48`. META + RITMO, y nada mas.
    inline constexpr VNActionMask vnAccionesMask()
    {
        return vnMetaMask() | vnRitmoMask();
    }

    // ── EL PREDICADO (ley 5) ────────────────────────────────────────────────
    //
    // Es UN termino de la conjuncion, no la conjuncion entera. Los call-sites
    // lo ponen en `&&` con lo que ya guardaban (rol, modal, cinematica) y esas
    // guardas NO se tocan: por eso un `habilitar` no puede otorgar nada.
    inline bool vnActionAllowed(VNActionMask sealed, VNAction a)
    {
        return (sealed & vnBit(a)) == 0u;
    }

    // ── EL ALGEBRA (ley 4) ──────────────────────────────────────────────────
    //
    // `deshabilitar` prende bits, `habilitar` los apaga, y el orden en que el
    // guion los escribe decide. El grueso no es un modo aparte: es la misma
    // operacion con `vnAccionesMask()` de argumento.
    inline VNActionMask vnSeal(VNActionMask sealed, VNActionMask which)
    {
        return sealed | which;
    }

    inline VNActionMask vnUnseal(VNActionMask sealed, VNActionMask which)
    {
        return sealed & ~which;
    }

    // ── LEY 7: EL RITMO QUE QUEDA ENCENDIDO ─────────────────────────────────
    //
    // Contesta "el modo `auto` (o el toggle de skip) sigue vivo despues de
    // aplicar este sello?". Se consulta CADA FRAME, no una sola vez al sellar:
    // asi el invariante se mantiene sin importar en que orden corrieron el
    // comando y el toggle, y un `habilitar auto` posterior no tiene que
    // acordarse de restaurar nada (no restaura: el jugador vuelve a prenderlo,
    // que es lo que "el jugador puede hacer X" significa).
    //
    // Devuelve el estado que el modo DEBE tener, no un permiso: el caller
    // asigna, no pregunta.
    inline bool vnRitmoQueda(VNActionMask sealed, VNAction ritmo, bool encendidoHoy)
    {
        return encendidoHoy && vnActionAllowed(sealed, ritmo);
    }

    // ── EL NOMBRE CANONICO DE UN VERBO (la inversa de la tabla) ─────────────
    //
    // Lo usan las tres puertas para armar su voz. Existe para que ningun
    // call-site escriba el nombre a mano: un literal suelto es como una voz
    // dice `menu_guardar` mientras la sintaxis pide `save_menu`. Devuelve
    // SIEMPRE la grafia espanola, que es la canonica de la tabla del atomo.
    //
    // El harness lo corre de ida y vuelta (nombre -> bit -> nombre), que es lo
    // que caza la deriva entre esta funcion y `vnLookupAction`.
    inline const char* vnNombreDeAccion(VNAction a)
    {
        switch (a) {
        case VNAction::GuardadoRapido: return "guardado_rapido";
        case VNAction::CargaRapida:    return "carga_rapida";
        case VNAction::MenuGuardar:    return "menu_guardar";
        case VNAction::MenuCargar:     return "menu_cargar";
        case VNAction::Historial:      return "historial";
        case VNAction::Menu:           return "menu";
        case VNAction::Config:         return "config";
        case VNAction::Auto:           return "auto";
        default:                       return "saltar_texto";
        }
    }

    // ── LA VOZ DEL VETO, EN UN SOLO LUGAR ───────────────────────────────────
    //
    // **La escriben DOS puertas** —`VNScriptState` (atajos crudos + verbos
    // shell) y `VNDialogBox` (los seis botones y los toggles)— y un creador la
    // busca POR TEXTO en la consola. Dos literales gemelos en dos archivos es
    // la forma barata de `MERGE-DUPLICADO`: el dia que alguien mejore la frase,
    // la mejora en uno.
    //
    // Devuelve la linea en vez de imprimirla para que este header siga sin
    // `<iostream>` y para que el harness pueda afirmar la frase exacta.
    //
    // **Existe porque la sonda cazo que la puerta B era MUDA.** Sellaba bien
    // -el auto no prendia- y no decia nada, mientras las otras dos hablaban.
    // Un sello que funciona en silencio es indistinguible de una tecla rota, y
    // era exactamente el miedo de `D-44` entrando por la ventana.
    inline std::string vnVozSelloVeta(const std::string& nombre)
    {
        return "[deshabilitar] accion '" + nombre + "' sellada por el guion.";
    }

    // ── EL VOCABULARIO DEL CREADOR (§2.9) ───────────────────────────────────
    //
    // Vive aca y no en el parser por la misma razon que todo lo demas de este
    // header: aca se puede probar sin levantar `EclipScript.h`. El parser
    // consulta esta tabla y no tiene una propia - dos tablas del mismo
    // vocabulario es como se desincronizan las grafias (§7.442).
    //
    // La palabra llega YA FOLDEADA por el caller (mismo trato que
    // `esComandoDelMotor`): el fold del vocabulario del motor es de
    // `vocabToLower`, no de aca.
    enum class VNActionLookup
    {
        Verbo,            // un verbo sellable; `bits` dice cual
        Grueso,           // `acciones` / `actions`; `bits` = vnAccionesMask()
        SalidaProtegida,  // ley 6 (`D-46`): existe, y el motor NO lo deja sellar
        SinSelloPropio,   // verbo real que no tiene sello INDIVIDUAL (ver abajo)
        Desconocida       // no es un verbo del dominio `vn`
    };

    struct VNActionName
    {
        VNActionLookup kind = VNActionLookup::Desconocida;
        VNActionMask   bits = 0u;
    };

    inline VNActionName vnLookupAction(const std::string& w)
    {
        auto hit = [](VNAction a) {
            VNActionName r; r.kind = VNActionLookup::Verbo; r.bits = vnBit(a); return r;
        };
        auto kind = [](VNActionLookup k) {
            VNActionName r; r.kind = k; return r;
        };

        // ── El grueso ──
        if (w == "acciones" || w == "actions") {
            VNActionName r;
            r.kind = VNActionLookup::Grueso;
            r.bits = vnAccionesMask();
            return r;
        }

        // ── META / SERVICIOS ──
        if (w == "guardado_rapido" || w == "quicksave") return hit(VNAction::GuardadoRapido);
        if (w == "carga_rapida"    || w == "quickload") return hit(VNAction::CargaRapida);
        if (w == "menu_guardar"    || w == "save_menu") return hit(VNAction::MenuGuardar);
        if (w == "menu_cargar"     || w == "load_menu") return hit(VNAction::MenuCargar);
        if (w == "historial"       || w == "history")   return hit(VNAction::Historial);
        if (w == "menu")                                return hit(VNAction::Menu);
        if (w == "config"          || w == "settings")  return hit(VNAction::Config);

        // ── RITMO ──
        if (w == "auto")                                return hit(VNAction::Auto);
        if (w == "saltar_texto"    || w == "skip")      return hit(VNAction::SaltarTexto);

        // ── FAMILIA SALIDA: existe, y no se sella (ley 6 / `D-46`) ──────────
        //
        // El peligro es el que el fundador nombro desde otra puerta: *"opciones
        // a elegir dentro de la caja... estas no deberian deshabilitarse"*. Su
        // pantalla de aviso sale UNICAMENTE por `PROCEDER`/`DENEGAR`; un grueso
        // que las hubiera incluido la dejaba sin salida.
        if (w == "avanzar"   || w == "advance"  ||
            w == "opcion_1"  || w == "opcion_2" || w == "opcion_3" || w == "opcion_4" ||
            w == "choice_1"  || w == "choice_2" || w == "choice_3" || w == "choice_4" ||
            w == "foco_arriba"     || w == "focus_up"    ||
            w == "foco_abajo"      || w == "focus_down"  ||
            w == "foco_izquierda"  || w == "focus_left"  ||
            w == "foco_derecha"    || w == "focus_right")
            return kind(VNActionLookup::SalidaProtegida);

        // ── LOS DOS QUE NO TIENEN SELLO PROPIO, Y POR QUE ───────────────────
        //
        // `D-41` los lista en la familia META junto a `historial`. **Medido, un
        // sello individual sobre ellos no puede hacer nada util:** el scroll
        // solo existe con el log ABIERTO, y sellar `historial` ya impide
        // abrirlo. Lo unico que un sello propio expresaria es "podes abrir el
        // log pero no subir", que es capacidad sin caller (§7.118) - y una
        // sintaxis que acepta una palabra para no hacer nada es la sintaxis
        // mintiendo. Se contesta con voz, no con un no-op silencioso.
        if (w == "historial_arriba" || w == "historial_abajo" ||
            w == "history_up"       || w == "history_down")
            return kind(VNActionLookup::SinSelloPropio);

        return kind(VNActionLookup::Desconocida);
    }

} // namespace eclips
