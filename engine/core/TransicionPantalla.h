// ============================================================================
// TransicionPantalla.h - LA TRANSICION DE PANTALLA TIENE DUENO
// ============================================================================
//
// Nucleo de la ficha `FADE-DE-ESTADO-SIN-PERILLA` y del lote `el-fade-tiene-
// dueno` (`DISENO_MENU_RENPY §11`, forma firmada en `D-14`/`§11.7d`), mas el
// lote B `el-menu-se-pinta-encima` (`D-15`, medicion en `§11.9`), que trajo el
// tipo `encima` y la clave `cerrar`.
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// Todo `VNScriptState` que entra pinta un rectangulo NEGRO a pantalla completa
// con alpha 1 y lo baja a cero en medio segundo. Los tres ejes -que ocurra, el
// color y la duracion- estaban hardcodeados, y `VNScriptState.h:436` prendia el
// fundido SIN UN SOLO `if` alrededor. O sea que abrir la pausa desde el juego y
// abrir `guardar` desde la pausa costaban lo mismo:
//
//     A1  ESC  partida -> pausa      367 ms, pasando por negro
//     A2  F5   pausa   -> guardar    367 ms, pasando por negro   <- el rojo
//     D        escena  -> escena       0 ms, instantaneo
//
// (Medido con `correr_sonda_fade.ps1`. El tercero es el DISCRIMINADOR: donde el
// creador manda no hay transicion, y donde no manda hay una que no puede tocar.)
//
// Aca vive la ley que separa esos dos casos y la tabla que el creador escribe.
//
// ── POR QUE ESTE HEADER ES PURO, Y POR QUE NO IMPORTA `VNRole` ──────────────
//
// `VNRole` vive adentro de `VNScriptState.h`, que arrastra GL: una ley que lo
// tomara por parametro seria INALCANZABLE para la bateria. La salida no es
// mudar el enum -eso fue `§7.433` para `InputAction`, y ahi hacia falta porque
// el enum ERA el valor de retorno-: aca alcanza con la forma que `VNVerbRoute.h`
// ya usa, que es tomar BOOLS EN UN STRUCT NOMBRADO (`VNAtajoContexto`). Sueltos
// en la firma, dos intercambiados compilan en silencio y el harness los probaria
// en el orden equivocado con la misma cara de verde.
//
// ── LO QUE ESTE HEADER NO TIENE, Y NO ES UN OLVIDO ──────────────────────────
//
// `DISENO_MENU_RUTA §5`: **el dato nunca acepta lo que no dispara.** El lote
// `guardar-llega-al-norte` existio porque esa ley se rompio por su reflejo -el
// dato rechazaba lo que SI disparaba- y su guardian cruza whitelist contra
// routing EN LAS DOS DIRECCIONES. Asi que aca se declara UNICAMENTE lo que hay
// cableado:
//
//   * `color=`  NO esta, y sigue sin estar despues del lote B. Es parametro del
//     TIPO `fade`, no del renglon (`D-14`), y `D-15` NO lo firma: `encima` no
//     atraviesa nada solido, asi que el unico tipo que podria querer un color
//     es el velo, y el velo sigue siendo negro. Su guardian es `6.5`.
//
// **La forma firmada contempla los tres** (`§11.7d`) y el tipo es vocabulario
// abierto, asi que `mascara` (Katawa) entra el dia que se pague sin tocar el
// bloque ni migrar nada. Lo que NO se hace es aceptar hoy lo que no dispara:
// una palabra que el parser toma y el motor ignora es exactamente el defecto
// que `harness_pantallas` existe para cazar.
//
// ── LO QUE EL LOTE B AGREGO, Y POR QUE `cerrar` NO ES UN MOMENTO ────────────
//
// `el-menu-se-pinta-encima` (`D-15`, `DISENO_MENU_RENPY §11.9`) trajo el tipo
// `encima` y la clave `cerrar`. La segunda obligo a partir un enum en dos, y esa
// particion es la mitad interesante del lote:
//
//     TransicionClave     lo que el CREADOR escribe como cabeza de renglon.
//                         TRES: abrir | navegar | cerrar
//     TransicionMomento   lo que la LEY DERIVA de como naciste.
//                         DOS: abrir | navegar -- y no crece
//
// La ley contesta desde `TransicionContexto`, que describe UN NACIMIENTO
// (`destinoEsMenu`, `hayOrigen`, `origenEsMenu`). **El que se va no esta
// naciendo**, asi que un tercer valor del momento no tendria de donde
// derivarse: `cerrar` no es una fila que faltaba en la ley, es una pregunta que
// la ley no hace. Meterlo igual habria puesto rojo a `2.1` y `3.2` del harness
// -que cuentan cuantas celdas del nacimiento son `navegar`- sin ganar nada.
//
// ── LA TRAMPA, PARA EL LOTE QUE SIGA ────────────────────────────────────────
//
// Los tipos se enumeran DESDE LA TABLA (`transicionTiposAceptados`), no a mano.
// `EclipScript.h:1093` documenta el precio de la otra forma: la tabla de
// `[efecto:]` tiene un SEGUNDO SITIO escrito a mano, y si alguien agrega una
// grafia alla y no la copia aca, el aviso le nombra al creador una lista
// incompleta y el motor queda ensenando menos de lo que acepta.
// ============================================================================

#pragma once

#include "Types.h"
#include "EclipsParse.h"

#include <string>
#include <vector>

namespace eclips
{
    // ── El TIPO de la transicion ────────────────────────────────────────────
    //
    // `Fade` y `Encima` son DOS MECANISMOS, no dos ajustes del mismo, y la
    // diferencia la trajo el fundador mirando DDLC (`§11.7c`):
    //
    //     fade     se dibuja el DESTINO y encima un rect opaco a pantalla
    //              completa con alpha 1 -> 0. El origen se corta de golpe, y
    //              atravesas un color solido.
    //     encima   el DESTINO se dibuja con su propio alpha 0 -> 1 SOBRE el de
    //              abajo, que sigue ahi. No hay nada solido en el medio.
    //
    // La sonda ya lo habia probado sin que nadie lo leyera asi: `113 ms ->
    // 0,0,192` (pausa, azul pleno) contra `130 ms -> 0,6,0`. El azul se va
    // ENTERO EN UN CUADRO. Si se pintara encima habria cuadros con azul y verde
    // mezclados, y no hay ninguno.
    enum class TransicionTipo
    {
        Ninguna,   // corte seco
        Fade,      // velo: rect opaco a pantalla completa, alpha 1 -> 0
        Encima     // el destino sube su propio alpha 0 -> 1 sobre el de abajo
    };

    // ── El MOMENTO: lo que la LEY DERIVA ────────────────────────────────────
    // DOS, y no crece: los dos son formas de NACER. Ver la nota de la cabecera
    // sobre por que `cerrar` no entra aca.
    enum class TransicionMomento
    {
        Abrir,     // una pantalla nace encima de otra cosa, o de la nada
        Navegar    // una pantalla de MENU nace encima de otra pantalla de MENU
    };

    // ── La CLAVE: lo que el CREADOR escribe ─────────────────────────────────
    // TRES. Es la cabeza del renglon del bloque, y el unico lugar donde
    // `cerrar` existe. No se deriva de nada: se lee del manifiesto.
    enum class TransicionClave
    {
        Abrir,
        Navegar,
        Cerrar     // la pantalla se va: el lote B (`D-15.2`)
    };

    // ── LA DURACION DE FABRICA NO ES UN GUSTO: ES LA MEDIDA ─────────────────
    //
    // `VNScriptState.h:2009` baja el alpha con `dt * 2.0f`, o sea 1.0 -> 0.0 en
    // exactamente medio segundo. La rampa medida en pantalla sube +6.4 por
    // cuadro sobre 192 -> 30 cuadros a 60 Hz -> 0.5 s. Coincide.
    //
    // OJO al leer una traza: los DOS PRIMEROS cuadros despues del `onEnter` se
    // comen ~30% de la rampa, porque la carga del `.ecl` deja `dt` acumulado.
    // Por eso la sonda mide 367 ms visibles y la constante igual es 0.5.
    inline constexpr f32 kTransicionDuracionDeFabrica = 0.5f;

    // ── LA TABLA DEL CREADOR ────────────────────────────────────────────────
    struct TransicionSpec
    {
        TransicionTipo tipo     = TransicionTipo::Fade;
        f32            duracion = kTransicionDuracionDeFabrica;
    };

    struct TransicionesTabla
    {
        // Los defaults SON LA CONDUCTA DE HOY (`D-13.3`): sin bloque en el
        // manifiesto, el motor se ve exactamente igual que antes del lote. El
        // arreglo que el feel-test pidio es UNA linea del creador:
        // `navegar ninguna`.
        TransicionSpec abrir{};
        TransicionSpec navegar{};

        // ⚠ `cerrar` NO HEREDA EL DEFAULT DE SUS DOS HERMANOS, y eso no es una
        // asimetria: es `D-13.3` aplicada al tercer renglon. Hoy el cierre es
        // CORTE SECO -medido: el state muere en el MISMO frame de la tecla,
        // antes de su propio render (`§11.9c`)-, asi que un `TransicionSpec{}`
        // pelado le pondria medio segundo de fundido de salida a TODO proyecto
        // que existe, sin que nadie lo escriba.
        //
        // La duracion de fabrica igual queda cargada a proposito: apagar el
        // tipo no tiene por que perder el numero, y asi `cerrar encima` sin
        // `duracion=` arranca en el mismo medio segundo que sus hermanos.
        // Su guardian es `1.5`.
        TransicionSpec cerrar{ TransicionTipo::Ninguna,
                               kTransicionDuracionDeFabrica };

        // Distinto de "quedo con los defaults": dice si el manifiesto trajo el
        // bloque. Es la misma distincion que `{pausa=0}` contra `{pausa=}`
        // (`RO-3`) - presencia, no valor.
        bool declarado = false;
    };

    // ── EL CONTEXTO ─────────────────────────────────────────────────────────
    //
    // Bools en struct nombrado, forma de `VNAtajoContexto`. El comentario de
    // aquel vale igual acá: sueltos en la firma, dos intercambiados compilan en
    // silencio.
    //
    // Los tres los sabe QUIEN EMPUJA, y ese es el punto de `D-13.2`: la
    // procedencia se expresa desde el caller y no ensenandole al state como
    // nacio. `StateStack::push` y `replace` llaman al MISMO `onEnter()` pelado,
    // asi que el state no puede contestar ninguna de las tres.
    struct TransicionContexto
    {
        bool destinoEsMenu = false;  // el que ENTRA es rol Menu
        bool hayOrigen     = false;  // habia algo en la pila debajo
        bool origenEsMenu  = false;  // y ese algo es rol Menu
    };

    // ── LA LEY ──────────────────────────────────────────────────────────────
    //
    // `Navegar` es el caso ESTRECHO y se nombra por lo que es: menu sobre menu.
    // Todo lo demas es `Abrir`, incluido el boot (sin origen) y el arranque de
    // la partida (destino rol Game sobre el menu) - ese ultimo es el que hace
    // que la ley NO pueda mirar solo al de abajo: desde el menu principal se
    // empuja el JUEGO, y eso no es navegar aunque el origen sea un menu.
    //
    // No se abre un tercer momento (`arrancar`) a proposito: sus sitios estan
    // medidos, pero el fundador no nombro un deseo distinto para el arranque, y
    // abrir el enum sin ese caller es lo que tuvo a `HO-07` cerrada dos meses.
    inline TransicionMomento transicionMomentoDe(const TransicionContexto& ctx)
    {
        if (!ctx.destinoEsMenu) return TransicionMomento::Abrir;
        if (!ctx.hayOrigen)     return TransicionMomento::Abrir;
        if (!ctx.origenEsMenu)  return TransicionMomento::Abrir;
        return TransicionMomento::Navegar;
    }

    inline const TransicionSpec& transicionSpecDe(const TransicionesTabla& t,
                                                  TransicionMomento momento)
    {
        return (momento == TransicionMomento::Navegar) ? t.navegar : t.abrir;
    }

    // El cierre NO toma momento, y ESA es la firma que lo dice todo: no hay
    // contexto que consultar porque no hay nacimiento que clasificar. Existe
    // como funcion y no como acceso directo al campo para que el consumidor del
    // cable sea grepeable, que es lo que `10.4` cuenta.
    inline const TransicionSpec& transicionSpecDeCierre(const TransicionesTabla& t)
    {
        return t.cerrar;
    }

    // ── EL VOCABULARIO, EN UN SOLO SITIO ────────────────────────────────────
    //
    // Las grafias van en tablas y los avisos se ARMAN desde ellas. Ver la nota
    // de la cabecera sobre el segundo sitio escrito a mano.
    struct TransicionGrafias
    {
        std::vector<std::string> grafias;
    };

    inline const std::vector<std::pair<TransicionGrafias, TransicionTipo>>&
    transicionTablaTipos()
    {
        static const std::vector<std::pair<TransicionGrafias, TransicionTipo>> kT = {
            // `desvanecer` no se inventa: el motor YA la escribe para este
            // concepto exacto en otros dos dialectos (`MapData.h:3663` para
            // `animacion_entrada`, y `EclipScript.h:1083` para `[efecto:]`).
            { { { "ninguna", "none" } },      TransicionTipo::Ninguna },
            { { { "fade", "desvanecer" } },   TransicionTipo::Fade    },
            // `encima`/`over` es del fundador mirando DDLC, y las dos son
            // palabras cotidianas: se traducen (`D-32`).
            { { { "encima", "over" } },       TransicionTipo::Encima  },
        };
        return kT;
    }

    inline const std::vector<std::pair<TransicionGrafias, TransicionClave>>&
    transicionTablaClaves()
    {
        static const std::vector<std::pair<TransicionGrafias, TransicionClave>> kM = {
            { { { "abrir",   "open"     } }, TransicionClave::Abrir   },
            { { { "navegar", "navigate" } }, TransicionClave::Navegar },
            { { { "cerrar",  "close"    } }, TransicionClave::Cerrar  },
        };
        return kM;
    }

    inline bool transicionTipoDe(const std::string& palabra, TransicionTipo& out)
    {
        const std::string v = vocabToLower(palabra);
        for (const auto& fila : transicionTablaTipos())
            for (const auto& g : fila.first.grafias)
                if (g == v) { out = fila.second; return true; }
        return false;
    }

    // Se llamaba `transicionMomentoClaveDe` hasta el lote B. El renombre no es
    // cosmetico: contestaba un `TransicionMomento` y ahora contesta una
    // `TransicionClave`, que son dos conjuntos distintos. Dejarle el nombre
    // viejo seria un nombre que miente, que es de lo que este repo se cuida.
    inline bool transicionClaveDe(const std::string& palabra,
                                  TransicionClave& out)
    {
        const std::string v = vocabToLower(palabra);
        for (const auto& fila : transicionTablaClaves())
            for (const auto& g : fila.first.grafias)
                if (g == v) { out = fila.second; return true; }
        return false;
    }

    // Las dos voces se ARMAN desde las tablas de arriba. Si manana entra
    // `encima`, el aviso lo nombra solo.
    inline std::string transicionTiposAceptados()
    {
        std::string s;
        for (const auto& fila : transicionTablaTipos())
            for (const auto& g : fila.first.grafias)
                s += (s.empty() ? "" : " | ") + g;
        return s;
    }

    inline std::string transicionClavesAceptadas()
    {
        std::string s;
        for (const auto& fila : transicionTablaClaves())
            for (const auto& g : fila.first.grafias)
                s += (s.empty() ? "" : " | ") + g;
        return s;
    }

    // ── EL RENGLON ──────────────────────────────────────────────────────────
    //
    // Forma:  <clave> <tipo> [duracion=N]
    // Ej:     abrir   encima  duracion=0.5
    //         navegar ninguna
    //         cerrar  encima  duracion=0.3
    //
    // Verbo de cabeza + `k=v` para lo opcional, que es la forma de `capa` y
    // `hotspot` (`capa bg "#C0C000" x=0 y=0 w=1920 h=1080 z=0`). La magnitud
    // queda ULTIMA sin esfuerzo, que es `D-27`.
    enum class TransicionCausa
    {
        Ok,
        ClaveDesconocida,     // el verbo de cabeza no es de la tabla
        TipoFaltante,         // hay clave y no hay tipo
        TipoDesconocido,      // el tipo no es de la tabla
        ParametroDesconocido, // un `k=v` que no conocemos
        DuracionInvalida      // `duracion=` con basura, o negativa
    };

    // `ofensora` sale con la palabra que el CREADOR escribio, no con la
    // canonica a la que el motor la traduce - es `SYN-4c-ii` (`harness_grafia`):
    // un aviso sobre un valor nombra la grafia que se escribio.
    inline TransicionCausa transicionAplicarRenglon(const std::string& linea,
                                                    TransicionesTabla& tabla,
                                                    std::string& ofensora)
    {
        ofensora.clear();

        // Tokenizar por blancos.
        std::vector<std::string> tok;
        {
            std::string acc;
            for (char c : linea)
            {
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                {
                    if (!acc.empty()) { tok.push_back(acc); acc.clear(); }
                }
                else acc += c;
            }
            if (!acc.empty()) tok.push_back(acc);
        }
        if (tok.empty()) return TransicionCausa::Ok;   // renglon vacio: no-op

        TransicionClave clave{};
        if (!transicionClaveDe(tok[0], clave))
        {
            ofensora = tok[0];
            return TransicionCausa::ClaveDesconocida;
        }

        if (tok.size() < 2)
        {
            ofensora = tok[0];
            return TransicionCausa::TipoFaltante;
        }

        TransicionTipo tipo{};
        if (!transicionTipoDe(tok[1], tipo))
        {
            ofensora = tok[1];
            return TransicionCausa::TipoDesconocido;
        }

        TransicionSpec spec;
        spec.tipo = tipo;

        for (size_t i = 2; i < tok.size(); i++)
        {
            const size_t eq = tok[i].find('=');
            if (eq == std::string::npos)
            {
                ofensora = tok[i];
                return TransicionCausa::ParametroDesconocido;
            }
            const std::string k = vocabToLower(tok[i].substr(0, eq));
            const std::string v = tok[i].substr(eq + 1);

            if (k == "duracion" || k == "duration")
            {
                f32 f = 0.0f;
                // Estricto: consumo total y finitud. La ley del numero es
                // `EclipsParse.h` y no se replica (`harness_num` la custodia).
                if (parseFloatStrict(v, f) != NumParse::Ok || f < 0.0f)
                {
                    ofensora = tok[i];
                    return TransicionCausa::DuracionInvalida;
                }
                spec.duracion = f;
            }
            else
            {
                ofensora = tok[i].substr(0, eq);
                return TransicionCausa::ParametroDesconocido;
            }
        }

        // `ninguna` con `duracion=` no es un error: la duracion queda guardada
        // y no se usa. Rechazarla obligaria a que apagar y volver a prender una
        // transicion pierda el numero que el creador ya habia elegido.
        //
        // El `switch` va sin `default` a proposito, PERO EL COMPILADOR NO TE
        // CUIDA ACA, y esto esta medido: la primera version de este comentario
        // afirmaba que `/W4` sube el C4062 de un enumerador sin caso. **Es
        // falso** -- C4062 esta apagado por default en MSVC, y agregar un cuarto
        // enumerador compilo LIMPIO bajo `/W4 /WX` (probado el 2026-08-20).
        //
        // Lo que pasa si alguien agrega una clave y se olvida del caso: el
        // renglon contesta `Ok`, `declarado` queda en true y el spec **se
        // descarta en silencio**. El guardian de eso es `9.5` del harness, que
        // enumera la tabla y exige que cada clave mueva EXACTAMENTE UN balde.
        switch (clave)
        {
        case TransicionClave::Navegar: tabla.navegar = spec; break;
        case TransicionClave::Cerrar:  tabla.cerrar  = spec; break;
        case TransicionClave::Abrir:   tabla.abrir   = spec; break;
        }

        tabla.declarado = true;
        return TransicionCausa::Ok;
    }
}
