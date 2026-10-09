// ============================================================================
// VNPalabrasPagina.h - LA PAGINA DEL POOL: CUALES N, Y DONDE VA CADA UNA
// ============================================================================
//
// `CP2a` de `DISENO_MUTAR_PALABRAS.md`, con los tres forks firmados por el
// fundador el 2026-08-15:
//
//   A2  el bloque SINTETIZA `texto` + `enlace` (capas), en vez de vivir
//       adentro de la caja de dialogo como una columna de choices.
//   B   el SORTEO entra en `CP2`, no en `CP3`.
//   C1  el anillo de foco se queda con la LISTA; la geometria es `CP2b`.
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// El `CP1` dejo el bloque `palabras` PARSEANDO y sin hacer nada: el runtime lo
// saltea por el `default: ignorar` del switch (`VNScriptState.h:4546`). Para que
// dibuje hay que contestar dos preguntas, y son las dos que viven aca:
//
//   1. de un pool de M, CUALES N se muestran   (consume `WeightedDraw.h`)
//   2. DONDE va cada una dentro de un area     (el "auto-grid" del `F5`)
//
// Ninguna de las dos toca SDL, ni fuentes, ni capas: los anchos de las palabras
// entran POR PARAMETRO, que es la misma forma con la que `inputHeldDeTabla` toma
// su predicado y `LangPaths` su disco. Sin eso, la ley viviria en una TU con GL
// y no la custodiaria nadie.
//
// ── POR QUE EXISTE APARTE DE `harness_peso`, Y NO ES POR TAMANO ─────────────
//
// `harness_peso` se declara dueno del **eje CUAL** (el sorteo ponderado) y del
// **parser** del bloque. El acomodo es un eje distinto -DONDE-, y mezclarlos
// significa que un rojo deja de decir cual de los dos se rompio. El de aca es
// `harness_palabras`, el 44o de la bateria.
//
// ── LA COSTURA DE LA BOLSA, Y POR QUE ACA SE RESUELVE AL REVES QUE EN `azar:` ─
//
// `sonda_costura_bolsa.cpp` ya midio el empalme entre un ciclo que se agota y el
// que rebaraja: es el UNICO lugar donde `repetir=no` puede repetir, con techo
// estructural de 2 y ~1 de cada 400 lineas en N=20. Para `azar:` eso se declaro
// aceptable, y con razon: son dos narraciones en beats distintos.
//
// **Aca la misma costura pone la misma palabra DOS VECES EN PANTALLA, al mismo
// tiempo.** Eso no es una racha: es algo que el jugador ve y lee como un bug. Asi
// que la pagina manda sobre el ciclo (ley 2.c), y el costo va declarado en vez de
// escondido: una entrada que la costura saltea queda SACADA del ciclo nuevo sin
// haberse mostrado en el, o sea que puede perderse UN ciclo. Se paga eso a cambio
// de que no haya duplicados en pantalla. Guardian: `harness_palabras` seccion 4;
// control negativo (la ley sin esto): seccion 7.
//
// ── LO QUE ESTE ARCHIVO **NO** DECIDE ───────────────────────────────────────
//
//   - el gate `[si:]` del item. Se parsea desde `CP1` y no lo evalua nadie; el
//     present-if (item ausente del sorteo) es `CP3` y entra como un peso 0 antes
//     de llamar aca. La ley de la pagina no lo tiene que saber.
//     > ⚠ LA ULTIMA FRASE ES FALSA, y lo midio la apertura de `CP3`
//     > (2026-08-16). El peso 0 alcanza para el SORTEO y no para la BOLSA: su
//     > `restantes` se cuenta con los pesos del momento del refill, y el gate es
//     > el primer caller que los mueve entre visitas sobre la misma bolsa. Sin
//     > `bagResync`, 4158 de 80588 visitas dan pagina corta (2589 vacias) y
//     > 35950 rebarajan antes de tiempo. La ley de la pagina SI lo tiene que
//     > saber -una linea-, y quien evalua el gate sigue siendo el caller.
//     > Guardianes: seccion 8; control negativo con las dos leyes: seccion 9.
//   - `${_palabra}` / `${_palabra_var}` (`F4`): tambien `CP3`.
//   - el CENTRADO de las filas. `CP2a` acomoda a la izquierda, que es como se lee
//     una hoja. Ficha `PALABRAS-SIN-CENTRAR`, gatillo: el feel-test del fundador.
//   - la geometria del anillo de foco (`C2`/`CP2b`): con la lista, `abajo` va al
//     de al lado. Es el gatillo de `GEOMETRIA-FOCO-SIN-CALLER`, cobrado y
//     diferido con firma.
//
// ── LO QUE NO PUEDE VER ─────────────────────────────────────────────────────
//
// El CABLE, y son cuatro puntos, los cuatro en `VNScriptState.h` (que arrastra
// SDL/GL/Lua, misma pared que `MERGE-DUPLICADO`):
//   1. que el `case EclipCmdType::Palabras` llame a estas dos leyes;
//   2. que sintetice las capas por `cmdTexto` -o sea con el MISMO merge de
//      estilo que un `texto` escrito a mano- y las marque `interactive`;
//   3. que la espera sea `WaitingInput` + `m_modalRequired` (que es lo que la
//      ley 7 de `VNVerbRoute.h` exige para que el anillo llegue);
//   4. que el pick resuelva la espera EL MISMO (espejo de `ChoiceMade`,
//      `VNScriptState.h:1885`) en vez de caer al avance plano - si cayera, el
//      `mark(m_currentLineKey)` de `:1856` marcaria como vista una linea que no
//      es de este comando, y esa invariante esta escrita en `:1854`.
// Lo cubren `vnscript/test_palabras_feel.ecl` + `ver_palabras.bat`.
//
// ASCII PURO a proposito, igual que sus hermanos de `engine/core`.
// ============================================================================

#pragma once

#include "Types.h"
#include "WeightedDraw.h"

#include <cmath>
#include <vector>

namespace eclips
{
    // ── EL AREA POR DEFECTO, DERIVADA Y NO INVENTADA ────────────────────────
    //
    // El default tiene que cumplir UNA cosa que se puede medir: **no taparle la
    // caja de dialogo al jugador**. Con los defaults del tema
    // (`DialogBoxStyle::height = 185`, `marginBottom = 30`, `widthRatio = 0.88`,
    // `Theme.h:343-345`) y el lienzo de diseno (1920x1080, `Types.h:34`):
    //
    //     techo de la caja  =  1080 - 185 - 30  =  865   -> 0.801 de VH
    //     borde izq. caja   =  (1 - 0.88) / 2         =  0.06 de VW
    //
    // De ahi salen los cuatro: el pool se alinea con la caja en X -para que se
    // lea como la misma columna de pantalla- y termina 33 px arriba de su techo.
    //
    //     x  0.06 * 1920 =  115.2      ancho  0.88 * 1920 = 1689.6
    //     y  0.12 * 1080 =  129.6      alto   0.65 * 1080 =  702.0
    //     fondo del area = 129.6 + 702 = 831.6   <   865   (la caja respira)
    //
    // Si alguien cambia esos defaults del tema, este comentario queda viejo y el
    // area puede pisar la caja: lo custodia `harness_palabras` 6.7, que corre la
    // aritmetica contra los valores vivos en vez de contra estos numeros.
    inline constexpr f32 kPalabrasAreaXRatio     = 0.06f;
    inline constexpr f32 kPalabrasAreaYRatio     = 0.12f;
    inline constexpr f32 kPalabrasAreaAnchoRatio = 0.88f;
    inline constexpr f32 kPalabrasAreaAltoRatio  = 0.65f;

    // Separacion entre palabras y entre filas. Horneadas en `CP2a` a proposito:
    // son dos numeros mas que nadie pidio todavia, y §2.9 dice que la mejor forma
    // de no equivocarse al nombrar es no tener que nombrar. Ficha
    // `PALABRAS-SIN-ESPACIADO`, gatillo: el primer creador que quiera apretarlas.
    inline constexpr f32 kPalabrasGapX = 28.0f;
    inline constexpr f32 kPalabrasGapY = 14.0f;

    // ════════════════════════════════════════════════════════════════════════
    // LEY 0 - EL PRESENT-IF: quienes COMPITEN en esta visita  (`CP3`)
    // ════════════════════════════════════════════════════════════════════════
    //
    // El gate `[si:]` de cada item lo evalua el CALLER -`evaluateCondition` es
    // del state y lee tres namespaces (sistema, partida, meta)-, y aca entra el
    // veredicto ya tomado, uno por palabra. Misma forma con la que
    // `inputHeldDeTabla` toma su predicado y `LangPaths` su disco: sin eso, esta
    // ley viviria en una TU con GL y no la custodiaria nadie.
    //
    // **El present-if APAGA, no borra**, y eso no es comodidad: el indice es la
    // identidad de la entrada dentro de la bolsa, asi que sacar una del vector
    // le corre el ciclo a todas las demas. Medido: compactar rebaraja 54498 de
    // 80588 visitas, o sea que `repetir=no` deja de agotar el pool
    // (`harness_palabras` 9.4). Apagar reusa la ley que `peso=0` ya tenia
    // firmada -APAGADA, no borrada- en vez de inventar una segunda.
    //
    // Las dos causas se cuentan APARTE aunque las dos produzcan un 0, porque el
    // motor tiene que poder decir cual fue: *"todas en peso=0"* manda a leer el
    // `.ecl` y *"todas ocultas por su gate"* manda a mirar el estado de la
    // partida. Un aviso que las confunde manda a leer el archivo equivocado.
    struct PalabrasCuentas
    {
        int ocultas   = 0;   // gate cerrado EN ESTA VISITA
        int apagadas  = 0;   // `peso=0` escrito por el creador
        int elegibles = 0;   // las que de verdad compiten
    };

    // `gateAbierto` va en paralelo a `pesos`. Un indice sin veredicto cuenta
    // como ABIERTO: la ausencia de gate es la forma normal de un item, y es lo
    // que hace que un pool sin una sola condicion pase por aca sin cambiar nada.
    inline PalabrasCuentas palabrasPesosEfectivos(
        const std::vector<f32>& pesos,
        const std::vector<bool>& gateAbierto,
        std::vector<f32>& out)
    {
        PalabrasCuentas c;
        out = pesos;

        for (size_t i = 0; i < out.size(); ++i)
        {
            const bool abierto =
                (i >= gateAbierto.size()) ? true : gateAbierto[i];
            const bool teniaPeso = clampDrawWeight(out[i]) > 0.0f;

            if (!abierto)
            {
                // El orden importa para el conteo: una palabra que el creador
                // ya habia apagado con `peso=0` NO se cuenta dos veces, y se
                // reporta por la causa que el creador escribio.
                if (teniaPeso) ++c.ocultas;
                else           ++c.apagadas;
                out[i] = 0.0f;
                continue;
            }

            if (teniaPeso) ++c.elegibles;
            else           ++c.apagadas;
        }
        return c;
    }

    // ════════════════════════════════════════════════════════════════════════
    // LEY 1 - CUALES N (el sorteo de la pagina)
    // ════════════════════════════════════════════════════════════════════════
    //
    // Devuelve los indices elegidos, EN EL ORDEN en que se van a mostrar. Ese
    // orden importa mas de lo que parece: es el que el anillo de foco recorre
    // (`FocusRing.h` ley 4 usa el orden de declaracion de las capas, y las capas
    // las va a crear este vector).
    //
    //   a. `repetir=si` -> N tiradas de `weightedPick`. Repetir es legitimo y
    //      el motor calla; el parser ya lo declaro asi (`EclipScript.h:4506`).
    //   b. `repetir=no` -> TECHO en `min(N, elegibles)`. Pedir mas que el pool
    //      es imposible por definicion del modo, y el parser ya avisa con su
    //      numero de linea, asi que aca no se avisa de nuevo: se topa.
    //   c. `repetir=no` -> **una palabra no puede salir dos veces en la MISMA
    //      pagina**, ni cuando la bolsa rebaraja a mitad de camino. Ver la nota
    //      de la costura en la cabecera.
    //   d. todas en `peso=0` (o pool vacio) -> pagina vacia. No se inventa una
    //      salida, que es la ley 13 del eje CUAL, y el aviso es del parser.
    //
    // `bolsa` es estado de CORRIDA y jamas de disco (ley 7 del azar): la clave
    // la elige el caller, igual que `azar:` -escena + linea del bloque-.
    template <class Rng>
    inline std::vector<int> palabrasElegirPagina(const std::vector<f32>& pesos,
                                                int n, bool repetir,
                                                DrawBag& bolsa, Rng& rng)
    {
        std::vector<int> pagina;
        if (n <= 0 || pesos.empty()) return pagina;   // (d) y fail-closed doble

        if (repetir)
        {
            pagina.reserve((size_t)n);
            for (int k = 0; k < n; ++k)
            {
                const int i = weightedPick(pesos, rng);
                if (i == kDrawNone) break;            // (d)
                pagina.push_back(i);
            }
            return pagina;
        }

        // `CP3` fork 1a: el ciclo se re-cuenta contra los pesos VIVOS antes de
        // sortear. Sin esto, un gate que se movio entre visitas deja la cuenta
        // de la bolsa hablando de otro reparto — y lo paga la pagina, no la
        // bolsa. La razon completa vive en `WeightedDraw.h` (`bagResync`).
        bagResync(bolsa, pesos);

        int elegibles = 0;
        for (const f32 w : pesos)
            if (clampDrawWeight(w) > 0.0f) ++elegibles;

        const int techo = (n < elegibles) ? n : elegibles;   // (b)
        if (techo <= 0) return pagina;                       // (d)
        pagina.reserve((size_t)techo);

        // La cota NO es paranoia decorativa, es lo que hace terminable al `continue`
        // de la costura: mientras `pagina.size() < techo <= elegibles`, queda al
        // menos UNA entrada elegible fuera de la pagina, y cada vuelta consume
        // exactamente una entrada del ciclo (la marca `sacado` la pone `bagPick`).
        // O sea que la vuelta que la encuentra llega en a lo sumo `elegibles`
        // pasos por ciclo, y la pagina cruza como maximo una costura.
        const int tope = 2 * elegibles + techo;

        for (int vuelta = 0; (int)pagina.size() < techo && vuelta < tope; ++vuelta)
        {
            const int i = bagPick(bolsa, pesos, rng);
            if (i == kDrawNone) break;                // (d)

            bool yaEsta = false;
            for (const int j : pagina)
                if (j == i) { yaEsta = true; break; }
            if (yaEsta) continue;                     // (c) LA COSTURA

            pagina.push_back(i);
        }
        return pagina;
    }

    // ════════════════════════════════════════════════════════════════════════
    // LEY 2 - DONDE VA CADA UNA (el acomodo)
    // ════════════════════════════════════════════════════════════════════════
    //
    // Flow con wrap: de izquierda a derecha desde el borde del area, y cuando la
    // proxima no entra, fila nueva.
    //
    // **Por que flow y no celdas uniformes, y esto es medicion y no gusto:** con
    // celdas de ancho fijo la celda deja de ser la palabra, y el hit-test del
    // camino de capas ES la palabra medida (`SceneLayer.h:993`, `measureWidth x
    // getLineHeight`). Serian DOS nociones de "clickeable" -la que se ve y la que
    // responde-, que es exactamente lo que §7.100 (render == hit-test) prohibe, y
    // el propio `SceneLayer` se lo escribe al foco del teclado: *"si el teclado
    // pudiera focar un enlace que el mouse no puede tocar, tendriamos dos nociones
    // de clickeable"* (`:1037`). Ademas celdas uniformes pediria la palabra
    // `columnas`, y el flow no nombra nada. Control negativo: seccion 7.
    //
    // Los anchos entran ya medidos, uno por palabra y en el mismo orden. Un ancho
    // que no es finito o es negativo cuenta como 0: no se descarta la palabra
    // -descartar en silencio es el defecto-, se la deja sin ancho y se ve.
    struct PalabrasArea
    {
        f32 x = 0.0f, y = 0.0f, ancho = 0.0f, alto = 0.0f;
    };

    struct PalabrasCelda
    {
        f32 x = 0.0f, y = 0.0f, ancho = 0.0f, alto = 0.0f;
        int fila = 0;
    };

    struct PalabrasAcomodo
    {
        std::vector<PalabrasCelda> celdas;
        int  filas    = 0;
        // `true` cuando la ultima fila pasa el fondo del area. NO se recorta: el
        // motor no sabe cuanto mide una fuente hasta que dibuja, y comerse
        // palabras del pool que el creador escribio es peor que mostrarlas
        // desbordadas. Es un aviso, y lo da el caller UNA vez (§7.493: causa
        // permanente, una sola voz).
        bool desborda = false;
    };

    inline f32 palabrasAnchoSano(f32 raw)
    {
        if (!std::isfinite(raw)) return 0.0f;
        if (raw < 0.0f)          return 0.0f;
        return raw;
    }

    inline PalabrasAcomodo palabrasAcomodar(const PalabrasArea& area,
                                            const std::vector<f32>& anchos,
                                            f32 altoLinea,
                                            f32 gapX = kPalabrasGapX,
                                            f32 gapY = kPalabrasGapY)
    {
        PalabrasAcomodo out;
        if (anchos.empty()) return out;

        const f32 alto = palabrasAnchoSano(altoLinea);
        const f32 gx   = palabrasAnchoSano(gapX);
        const f32 gy   = palabrasAnchoSano(gapY);
        const f32 derecha = area.x + palabrasAnchoSano(area.ancho);

        out.celdas.reserve(anchos.size());

        f32 cx  = area.x;
        int fila = 0;

        for (const f32 raw : anchos)
        {
            const f32 w = palabrasAnchoSano(raw);

            // El `cx > area.x` es la ley "nunca se descarta una palabra": si es
            // la primera de su fila entra igual, aunque sea mas ancha que el area
            // entera y desborde. Sin esa guarda, una palabra gigante haria wrap
            // para siempre sin llegar a colocarse nunca.
            if (cx > area.x && (cx + w) > derecha)
            {
                ++fila;
                cx = area.x;
            }

            PalabrasCelda c;
            c.x     = cx;
            c.y     = area.y + (f32)fila * (alto + gy);
            c.ancho = w;
            c.alto  = alto;
            c.fila  = fila;
            out.celdas.push_back(c);

            cx += w + gx;
        }

        out.filas = fila + 1;

        const f32 fondo = area.y + (f32)out.filas * alto + (f32)fila * gy;
        out.desborda = (palabrasAnchoSano(area.alto) > 0.0f) &&
                       (fondo > area.y + palabrasAnchoSano(area.alto));
        return out;
    }

} // namespace eclips
