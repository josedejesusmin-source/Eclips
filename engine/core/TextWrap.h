// ============================================================================
// TextWrap.h — LA LEY DEL CORTE: donde se parte una linea, y con que unidad
// ============================================================================
//
// Nucleo de la ficha `WRAP-MIDE-BYTES`. Hermano de `TextBudget.h`, que cuenta
// el presupuesto SOBRE el texto ya cortado; este decide DONDE se corta.
//
// ── QUE ARREGLA ─────────────────────────────────────────────────────────────
//
// Las CUATRO funciones de wrap de `TextRenderer.cpp` acumulaban el ancho de
// una palabra iterando BYTES:
//
//     getGlyph(static_cast<unsigned char>(c))
//
// asi que un codepoint multibyte pedia UN GLIFO POR BYTE. `measureWidth` habia
// sido arreglada en su momento -- su comentario nombra la enie y su 0xC3 0xB1 --
// y ese arreglo NUNCA llego a sus cuatro hermanas. Un arreglo parcial que
// quedo verde porque nadie probo en espanol.
//
// El defecto es SIGNADO, no "siempre mide de mas", y la razon es que el atlas
// hornea 32-126 y 160-383: los bytes que caen en el hueco 127-159 valen CERO.
// Medido sobre `Aller_Rg` a 34 px, kerning 0:
//
//     compania          +0.0 %   <- ASCII: por eso nadie lo vio en un ano
//     companiia (enie) +14.5 %   <- dos glifos donde el dibujo pone uno
//     que (con ?)      +56.6 %
//     la RAYA U+2014   -48.3 %   <- 0xE2 pesa como 'a-circunfleja'; 0x80 y
//                                   0x94 caen en el hueco y no pesan nada
//
// Y no es una diferencia de decimales: sobre el corpus vivo con la geometria
// del tema del fundador (Aller 41 px, kerning +0.8, ancho util 1112), SEIS
// lineas reales cambian de cantidad de lineas, TRES por lado. Las tres que
// hoy se cortan de MENOS son las tres que llevan una raya.
//
// ── POR QUE UN HEADER APARTE, Y NO ADENTRO DE `TextBudget.h` ────────────────
//
// Porque son dos preguntas y COMPONEN: aca se decide donde va cada corte y se
// emiten los `saltosInsertados`; `wrappedStop` los consume para contestar
// cuanto del presupuesto se gasto. Separadas, un rojo dice CUAL se rompio.
//
// Y porque la frontera que `TextBudget.h` declara -- "no envuelve, el wrap
// necesita metricas de glifo" -- es la razon por la que ESE archivo es
// probable. Meterle una ley que toma metricas la volveria falsa. Aca las
// metricas entran POR PARAMETRO, que es el mismo patron con el que
// `inputHeldDeTabla` toma su predicado y `VNPalabrasPagina` sus anchos: la ley
// queda pura, la bateria la ve, y SDL/GL se quedan del otro lado de la pared.
//
// `harness_wrap.cpp` prueba este archivo.
//
// ── EL HALLAZGO QUE LO HACE BARATO ──────────────────────────────────────────
//
// El texto envuelto es EL ORIGINAL CON '\n' INSERTADOS. El lazo viejo nunca
// borraba ni reordenaba un byte: los concatenaba en orden y agregaba saltos.
//
// Por eso la ley no devuelve un string: devuelve OFFSETS SOBRE EL ORIGINAL, y
// los cuatro consumidores reconstruyen con `aplicarCortes` -- que es pura y no
// necesita una sola metrica. Lo que queda del otro lado de la pared de GL son
// tres lineas por sitio, y lo demas lo custodia la bateria.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   la unidad del ancho es el CODEPOINT, no el byte. Es la misma ley 1
//           de `TextBudget.h`, en el otro eje: alla la unidad del PRESUPUESTO,
//           aca la del ANCHO. Que fueran distintas es como el motor terminaba
//           dibujando una cosa y midiendo otra
//   ley 2   el corte va SIEMPRE antes del primer byte de lo que quedo
//           pendiente. Nunca parte un codepoint, porque nunca cae adentro de
//           una palabra: cae en la frontera que el separador dejo abierta
//   ley 3   el ESPACIO se mide aparte, con la fuente BASE. No es prolijidad:
//           es la conducta viva de `drawTextWrappedRich_noBatch`, que resuelve
//           la fuente por caracter para el contenido y NO para el espacio
//           (`spaceGlyph = getGlyph(' ')` sobre `this`). Pasarlo por el
//           callback "por simetria" mueve el layout de toda caja con {fuente=}
//   ley 4   el glifo que falta OMITE, no sustituye. Es lo que hacen hoy el
//           wrap y `measureWidth`; `draw` sustituye por '?'. Esa divergencia
//           es la ficha `GLIFO-FALTANTE-DIVERGE` (PATRONES 7.584) y ESTE LOTE
//           NO LA TOCA -- el callback de produccion devuelve 0 para el glifo
//           ausente, igual que antes. Si algun dia se unifica la terna, este
//           header cambia A PROPOSITO y con su assert invertido
//
// ── LO QUE ESTE HEADER NO HACE ──────────────────────────────────────────────
//
// No mide glifos: los recibe. No dibuja. No sabe que es un atlas, una fuente
// ni un tag inline -- `aplicarCortesFX` es template justo para no tener que
// conocer `CharEffect`, que vive en una TU con GL.
// ============================================================================

#pragma once

#include "Types.h"
#include "TextBudget.h"

#include <functional>
#include <string>
#include <vector>

namespace eclips
{
    namespace text
    {
        // ── LA METRICA, POR PARAMETRO ───────────────────────────────────────
        //
        // El avance de UN codepoint, en pixeles. En produccion es
        // `g ? g->advance : 0` sobre el atlas (ley 4: el ausente omite); en el
        // harness es una tabla horneada con stb_truetype, o una sintetica.
        //
        // El segundo parametro es el INDICE DE CODEPOINT dentro del texto, y
        // no es decoracion: `drawTextWrappedRich_noBatch` resuelve la fuente
        // POR CARACTER ({fuente=X}, {tam=N}), asi que el mismo codepoint puede
        // medir distinto segun donde caiga. Los otros tres callers lo ignoran.
        using AvanceDeCodepoint = std::function<f32(i32 cp, size_t indiceCp)>;

        // ── EL PLAN ─────────────────────────────────────────────────────────
        //
        // `cortes` son offsets SOBRE EL TEXTO ORIGINAL: antes de ese byte va un
        // '\n' insertado. Van en orden y no se repiten.
        //
        // `lineas` es lo que `measureWrappedHeight` contaba por su cuenta, en
        // su propia copia del lazo. Ahora sale del mismo plan que el dibujo, y
        // por construccion vale `1 + cortes + los '\n' que escribio el autor`.
        struct PlanDeCorte
        {
            std::vector<size_t> cortes;
            i32                 lineas = 1;
        };

        // ── LA LEY DEL CORTE ────────────────────────────────────────────────
        //
        // Transcripcion del lazo que vivia CUATRO veces en `TextRenderer.cpp`
        // (:323-358, :382-416, :532-593, :824-853), con una sola diferencia
        // marcada abajo. Todo lo demas -- el orden de las guardas, el `> ` sin
        // `>=`, el `lineW > 0` que deja pasar la palabra mas larga que la caja,
        // el espacio que se cobra despues del flush -- esta como estaba, porque
        // cambiarlo moveria el layout de todo el corpus sin que nadie lo pida.
        inline PlanDeCorte planDeCorte(const std::string&        texto,
                                       f32                       anchoMax,
                                       f32                       anchoEspacio,
                                       const AvanceDeCodepoint&  avance)
        {
            PlanDeCorte plan;

            f32    lineW = 0.0f;      // ancho de lo ya confirmado en la linea
            f32    wordW = 0.0f;      // ancho de la palabra pendiente
            size_t inicioPendiente = 0;   // offset ORIGINAL del 1er byte pendiente
            bool   hayPendiente = false;
            size_t indiceCp = 0;      // indice de CODEPOINT, para `avance`

            for (size_t i = 0; i < texto.size(); ++i)
            {
                const char c = texto[i];

                if (c == ' ' || c == '\n')
                {
                    // El corte cae antes de lo pendiente. Si no hay palabra
                    // pendiente -- dos separadores seguidos con la linea ya
                    // pasada de ancho -- cae antes de ESTE separador, que es
                    // exactamente donde el lazo viejo lo ponia.
                    if (lineW + wordW > anchoMax && lineW > 0.0f)
                    {
                        plan.cortes.push_back(inicioPendiente);
                        ++plan.lineas;
                        lineW = 0.0f;
                    }

                    lineW += wordW;
                    wordW = 0.0f;
                    hayPendiente = false;

                    if (c == '\n') { ++plan.lineas; lineW = 0.0f; }
                    else           { lineW += anchoEspacio; }   // ley 3

                    inicioPendiente = i + 1;
                }
                else
                {
                    hayPendiente = true;

                    // ─── EL CAMBIO DEL LOTE, Y ES ESTE ──────────────────────
                    // UN GLIFO POR CODEPOINT (ley 1). Aca decia:
                    //
                    //     wordW += avance((i32)(unsigned char)c, indiceCp);
                    //
                    // o sea un glifo por BYTE, y por eso la enie pesaba dos y
                    // la raya pesaba menos que una letra.
                    //
                    // `i` salta al ULTIMO byte del codepoint, no al siguiente:
                    // el `++indiceCp` de abajo tiene que seguir disparando en
                    // el mismo lugar de siempre, y lo hace porque desde ahi el
                    // byte que sigue ya no es de continuacion.
                    //
                    // Un lider invalido (`cp < 0`) no suma nada y consume un
                    // byte, que es lo que `measureWidth` hace hoy.
                    i32          cp = 0;
                    const size_t largo = utf8Decodificar(texto, i, cp);
                    if (cp >= 0) wordW += avance(cp, indiceCp);
                    if (largo > 1) i += largo - 1;
                    // ────────────────────────────────────────────────────────
                }

                // El indice de codepoint avanza al CERRAR el codepoint, igual
                // que el `cpIn` del lazo rich: los bytes de continuacion
                // comparten el indice de su lider.
                if (i + 1 >= texto.size() ||
                    !isUtf8Continuation((unsigned char)texto[i + 1]))
                    ++indiceCp;
            }

            if (hayPendiente && lineW + wordW > anchoMax && lineW > 0.0f)
            {
                plan.cortes.push_back(inicioPendiente);
                ++plan.lineas;
            }

            return plan;
        }

        // ── APLICAR EL PLAN (sin una sola metrica) ──────────────────────────
        //
        // `envuelto` = el original con los '\n' del plan insertados.
        // `saltosInsertados` = donde cayeron, EN BYTES SOBRE EL ENVUELTO, que
        // es la unidad que `TextBudget::wrappedStop` pide. Esa traduccion de
        // coordenadas es la unica razon por la que esta funcion existe, y es
        // justo la que estaba escrita a mano en los tres sitios que dibujan.
        inline void aplicarCortes(const std::string&   texto,
                                  const PlanDeCorte&   plan,
                                  std::string&         envuelto,
                                  std::vector<size_t>& saltosInsertados)
        {
            envuelto.clear();
            saltosInsertados.clear();
            envuelto.reserve(texto.size() + plan.cortes.size());
            saltosInsertados.reserve(plan.cortes.size());

            size_t c = 0;
            for (size_t b = 0; b < texto.size(); ++b)
            {
                while (c < plan.cortes.size() && plan.cortes[c] == b)
                {
                    saltosInsertados.push_back(envuelto.size());
                    envuelto += '\n';
                    ++c;
                }
                envuelto += texto[b];
            }
            while (c < plan.cortes.size())   // defensivo: un corte al final
            {
                saltosInsertados.push_back(envuelto.size());
                envuelto += '\n';
                ++c;
            }
        }

        // ── APLICAR EL PLAN, CON LOS EFECTOS INLINE ─────────────────────────
        //
        // Igual que la de arriba y ademas arrastra los `CharEffect`. Es
        // template para no conocer ese tipo: vive en `TextRenderer.h`, que
        // arrastra <SDL_opengl.h>, y una ley que lo nombrara no la podria
        // probar nadie.
        //
        // OJO -- `envueltoFX` va indexado POR BYTE y no por codepoint, porque
        // asi lo lee su consumidor (`wrappedFX[startByte]`). Los bytes de
        // continuacion repiten el efecto de su lider. Un arreglo que "itere
        // codepoints" y de paso emita un efecto por codepoint desalinea TODOS
        // los tags inline de la caja del VN, y no rompe el build.
        template <class Efecto>
        inline void aplicarCortesFX(const std::string&          texto,
                                    const PlanDeCorte&          plan,
                                    const std::vector<Efecto>&  efectosPorCp,
                                    const Efecto&               sinEfecto,
                                    std::string&                envuelto,
                                    std::vector<Efecto>&        envueltoFX,
                                    std::vector<size_t>&        saltosInsertados)
        {
            envuelto.clear();
            envueltoFX.clear();
            saltosInsertados.clear();
            envuelto.reserve(texto.size() + plan.cortes.size());
            envueltoFX.reserve(texto.size() + plan.cortes.size());
            saltosInsertados.reserve(plan.cortes.size());

            size_t c = 0;
            size_t indiceCp = 0;

            for (size_t b = 0; b < texto.size(); ++b)
            {
                while (c < plan.cortes.size() && plan.cortes[c] == b)
                {
                    saltosInsertados.push_back(envuelto.size());
                    envuelto += '\n';
                    envueltoFX.push_back(sinEfecto);   // el salto no es del autor
                    ++c;
                }

                envuelto += texto[b];
                envueltoFX.push_back(indiceCp < efectosPorCp.size()
                                     ? efectosPorCp[indiceCp]
                                     : sinEfecto);

                if (b + 1 >= texto.size() ||
                    !isUtf8Continuation((unsigned char)texto[b + 1]))
                    ++indiceCp;
            }

            while (c < plan.cortes.size())
            {
                saltosInsertados.push_back(envuelto.size());
                envuelto += '\n';
                envueltoFX.push_back(sinEfecto);
                ++c;
            }
        }

    } // namespace text
} // namespace eclips
