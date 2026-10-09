// ============================================================================
// TextGlifo.h — QUE CONTESTA EL MOTOR CUANDO EL GLIFO NO ESTA
// ============================================================================
//
// Nucleo de la ficha `GLIFO-FALTANTE-DIVERGE` (PATRONES 7.584). Tercer hermano
// de `TextBudget.h` (cuanto del texto se mostro) y `TextWrap.h` (donde se
// corta). Los tres toman las metricas POR PARAMETRO, asi que los tres son
// puros y la bateria los ve enteros.
//
// ── QUE ARREGLA ─────────────────────────────────────────────────────────────
//
// El motor dibuja una cosa y mide otra. Ante un codepoint que el atlas no
// tiene:
//
//     draw    (TextRenderer.cpp:270 y :573)   sustituye por '?'
//     measure (TextRenderer.cpp:707)          lo OMITE
//     wrap    (TextRenderer.cpp:329 y :490)   lo OMITE
//
// Medido: `measureWidth("E" + U+0334)` da 19.900 y `measureWidth("E?")` da
// 33.197, con el framebuffer BYTE-IDENTICO al de "E?". La medida dice una cosa
// y la pantalla muestra la otra, y de ahi salen texto descentrado, cajas
// desbordadas y cortes en el lugar equivocado.
//
// Y NO es hipotetico: `test_glitch.ecl:35` tiene DIECINUEVE codepoints
// ausentes en una sola linea de dialogo (los combinantes del efecto de DDLC),
// y `test_ddlc.ecl:233` tiene VEINTIUNO. Hoy se pintan diecinueve '?' que el
// wrap conto como cero.
//
// ── LAS TRES PUERTAS A LA AUSENCIA ──────────────────────────────────────────
//
// Este archivo contesta por las tres, porque las tres terminan en el mismo
// `getGlyph(cp) == nullptr`:
//
//   (a) el codepoint no esta en el repertorio de 325 que el motor hornea
//       (combinantes, flechas, emoji, kaomoji)     -> TextRenderer.cpp:83-93
//   (b) esta pedido, pero la FUENTE no lo tiene    -> TextRenderer.cpp:96
//   (c) el atlas SE DESBORDO y el lazo corto: desde ahi faltan TODOS los que
//       venian despues                             -> TextRenderer.cpp:119
//
// La (c) es el silencio que el arco `ATLAS-DE-FUENTE` vino a matar. Por eso
// esta ley no es cosmetica: es el unico lugar del motor donde ese desborde se
// vuelve observable.
//
// ── COMO NACIO ESTE ARCHIVO, QUE ES POR QUE SE LE PUEDE CREER ───────────────
//
// Nacio transcribiendo las DOS conductas de entonces -- `avanceParaDibujo`
// sustituia, `avanceParaMedir` omitia -- con cero callers y cero cambio de
// conducta, y `harness_glifo.cpp` corrio contra el: **29 ok / 11 FAIL**, rojo
// GENUINO y selectivo (el guardian de no-op, el decoder y el aviso verdes
// desde el arranque; solo cayeron las secciones 2 y 3, las de la divergencia).
// Recien despues las dos pasaron a delegar en `avanceDeCodepoint`.
//
// Los dos nombres siguen existiendo A PROPOSITO: son el guardian de que nadie
// los vuelva a separar. Ver el comentario que los acompana.
//
// ── LO QUE ESTE HEADER NO HACE ──────────────────────────────────────────────
//
// No busca glifos: los recibe. No dibuja, no conoce el atlas, no sabe que es
// una fuente. No decide el REPERTORIO -- esa es la ficha
// `REPERTORIO-FIJO-DEL-ATLAS`, y va detras de esta.
// ============================================================================

#pragma once

#include "Types.h"

#include <functional>
#include <set>

namespace eclips
{
    namespace text
    {
        // ── LO QUE EL ATLAS CONTESTA ────────────────────────────────────────
        //
        // `hay` distingue "no esta" de "esta y mide cero": el espacio de ancho
        // cero existe, y un `avance == 0` no puede ser la senal de ausencia.
        // Confundir las dos es como el motor termino omitiendo en silencio.
        struct Glifo
        {
            bool hay = false;
            f32  avance = 0.0f;
        };

        using BuscarGlifo = std::function<Glifo(i32 cp)>;

        // ── LA POLITICA, EN UN SOLO SITIO ───────────────────────────────────
        //
        // Las dos que habia aca abajo -- una que sustituia y otra que omitia --
        // eran la transcripcion del motor de antes del 2026-08-28, y el rojo de
        // `harness_glifo` fue exactamente que NO contestaban lo mismo.
        enum class PoliticaAusente
        {
            Omitir,      // lo que no esta, no pesa y no se pinta
            Sustituir    // lo que no esta se dibuja y se mide como '?'
        };

        // LA POLITICA DE LA CASA. Fork B, firmado por el fundador (2026-08-28).
        //
        // Se eligio OMITIR por tres razones medidas, y ninguna es estetica:
        //
        //   1. es la unica que NO mueve el layout -- `measure` y `wrap` ya
        //      omitian, asi que el corte de linea queda donde estaba. Sustituir
        //      le movia el corte a la linea viva de `test_glitch.ecl` en 40 de
        //      88 anchos barridos (`harness_glifo` 4.2)
        //   2. el caso vivo son COMBINANTES, y un combinante mide CERO por
        //      definicion tipografica: el '?' es una respuesta falsa, y hoy se
        //      ve en pantalla -- 19 signos de pregunta en una sola linea
        //   3. escala con `REPERTORIO-FIJO-DEL-ATLAS`: recortar el repertorio
        //      llena la CONSOLA de avisos y no la pantalla de basura
        //
        // Y omitir solo es legitimo porque el motor HABLA: ver
        // `RegistroDeAusentes` mas abajo. Omitir en silencio seria peor que el
        // '?', y esa mitad no es opcional.
        //
        // Si algun dia se cambia, se cambia ACA -- una constante, no cinco
        // sitios -- y los asserts de `harness_glifo 2.x` se INVIERTEN Y SE
        // ANOTAN (7.328), no se parchean.
        inline PoliticaAusente politicaDeLaCasa()
        {
            return PoliticaAusente::Omitir;
        }

        // El avance de UN codepoint. Es la unica definicion; `draw`, `measure`
        // y el `wrap` la llaman todos.
        inline f32 avanceDeCodepoint(i32 cp, const BuscarGlifo& buscar, f32 medioEm,
                                     PoliticaAusente pol)
        {
            const Glifo g = buscar(cp);
            if (g.hay) return g.avance;

            if (pol == PoliticaAusente::Sustituir)
            {
                const Glifo interrogacion = buscar('?');
                if (interrogacion.hay) return interrogacion.avance;
                return medioEm;      // el tercer escalon es de ESTA politica
            }
            return 0.0f;
        }

        // ¿`draw` tiene que buscar un glifo sustituto? La misma constante que
        // decide el ancho decide el pixel: si la medida no lo cuenta, el
        // dibujo no lo pinta. Que fueran dos decisiones era el defecto.
        inline bool sustituyeElAusente(PoliticaAusente pol)
        {
            return pol == PoliticaAusente::Sustituir;
        }

        // ── LOS DOS NOMBRES DE LA TERNA, QUE SOBREVIVEN A PROPOSITO ─────────
        //
        // No son un envoltorio de cortesia: son EL GUARDIAN. Mientras los dos
        // existan, `harness_glifo 2.x` puede afirmar que contestan lo mismo, y
        // el dia que alguien vuelva a separarlos el rojo aparece solo. Si se
        // fusionan en uno, esa red se va con ellos.

        // `draw`: lo llaman `TextRenderer.cpp:270` y `:573`.
        inline f32 avanceParaDibujo(i32 cp, const BuscarGlifo& buscar, f32 medioEm)
        {
            return avanceDeCodepoint(cp, buscar, medioEm, politicaDeLaCasa());
        }

        // `measure` y `wrap`: los llaman `:707`, `:329` y `:490`.
        inline f32 avanceParaMedir(i32 cp, const BuscarGlifo& buscar)
        {
            return avanceDeCodepoint(cp, buscar, 0.0f, politicaDeLaCasa());
        }

        // ── EL AVISO: UNA VEZ POR CODEPOINT, NO UNA POR FRAME ───────────────
        //
        // El motor puede OMITIR sin mentir, pero no puede omitir EN SILENCIO:
        // omitir de callado es la enfermedad que este arco vino a matar. La
        // salida no es basura en la pantalla del jugador -- es una linea en la
        // consola del creador, que es el unico que puede arreglarlo.
        //
        // El canal ya existe (`std::cerr << "[Eclips] Aviso: ..."`, destino
        // futuro el toast de MODO_CREADOR 12.3). Lo que falta es la memoria:
        // un texto dibujado a 60 fps grita 60 veces por segundo si nadie
        // recuerda que ya lo dijo.
        //
        // Vive ACA y no adentro de `TextRenderer` para que la bateria lo vea:
        // el dedup es la parte que se puede romper en silencio -- un `set` que
        // no recuerda no rompe nada, solo inunda la consola hasta que el
        // creador deja de leerla, y ahi el aviso vale menos que nada.
        class RegistroDeAusentes
        {
        public:
            // `true` la PRIMERA vez que se ve este codepoint; `false` siempre
            // despues. El caller avisa solo cuando esto contesta `true`.
            bool primeraVez(i32 cp)
            {
                return m_vistos.insert(cp).second;
            }

            bool yaSeVio(i32 cp) const
            {
                return m_vistos.find(cp) != m_vistos.end();
            }

            size_t cuantos() const { return m_vistos.size(); }

            // El reload de fuentes (F8 del Modo Creador) cambia el atlas: lo
            // que faltaba puede estar ahora, y lo que estaba puede faltar. Sin
            // esto, el creador arregla la fuente y el motor sigue callado.
            void olvidar() { m_vistos.clear(); }

        private:
            std::set<i32> m_vistos;
        };
    }
}
