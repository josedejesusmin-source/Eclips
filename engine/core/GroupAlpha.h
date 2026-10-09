// ============================================================================
// GroupAlpha.h — LA SOLDADURA: un fundido que comparte un grupo de dibujos
// ============================================================================
//
// Nucleo de la ficha `CAJA-ENTRADA`.
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// La caja de dialogo NO es un sprite: es un grupo de ~20 dibujos con cuatro
// resoluciones de fuente distintas (cuerpo, placa, chrome, qte). Fundirla
// "entera" con un multiplicador escrito a mano en cada sitio son VEINTE
// oportunidades de olvidarse una — y olvidarse una no rompe el build ni pone
// un harness rojo: deja un boton que no funde. Fallo mudo.
//
// Aca el multiplicador vive en UN lugar y lo consume el pipeline:
//
//   `SpriteBatch::draw`          -> todo lo que pasa por el batch, o sea el
//                                   sprite de la caja, la placa, el indicador,
//                                   los botones, y TODO el texto (incluidas las
//                                   nueve pasadas del borde)
//   los prims de `glBegin`       -> los rounded rect, que siguen en immediate
//
// Con eso, "que se funda todo junto" deja de ser una lista que hay que mantener
// y pasa a ser una propiedad del AMBITO: lo que se dibuje adentro comparte el
// fundido, lo de afuera no. El ambito ES la soldadura.
//
// ── POR QUE UN HEADER APARTE, Y PURO ────────────────────────────────────────
//
// Mismo motivo que `TextBudget.h`: los dos consumidores arrastran GL
// (`SpriteBatch.cpp`, `Draw2D.h` incluye `SDL_opengl.h`), asi que una ley que
// viva adentro de ellos NO SE PUEDE PROBAR. Aca es SDL-free y GL-free — solo
// `Types.h` — y `harness_grupo.cpp` la prueba.
//
// ── LO QUE ESTE HEADER NO HACE, Y POR QUE ───────────────────────────────────
//
// No lleva DESPLAZAMIENTO (dx/dy) ni escala. El crecimiento natural es que el
// ambito cargue un `AnimOutput` entero — el tipo ya existe en `TweenSystem.h`,
// con su `compose()` y su identidad por canal, y es el mismo algebra con la que
// ya se animan personajes y pantalla. Cuando haya una perilla que mueva la caja
// (entrada deslizada), esto se cambia por `AnimOutput` en los mismos tres
// sitios.
//
// Hoy no esta porque hoy no hay caller: un canal sin perilla es capacidad
// indistinguible de no existir (§7.118), y ademas seria un camino de codigo que
// ningun assert recorre.
//
// OJO para cuando llegue: un desplazamiento aca es offset de RENDER, NO
// geometria. `getDialogBtnRects`/`getChoiceRects` siguen siendo la fuente unica
// del hit-test (§1.40) — la misma politica que el juice de hover de los choices
// ya declara en `VNDialogBox.h`. Una entrada deslizada dibuja corrido y clickea
// en reposo (invisible en 0.3s); el modo creador moviendo la caja es lo
// CONTRARIO: tiene que mover la geometria, o los botones quedan donde no se ven.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   IDENTIDAD. En reposo el factor es 1.0 y `applyGroupAlpha(a)`
//           devuelve `a` EXACTAMENTE (mismo bit). Es el guardian de no-op: si
//           esta ley cae, el lote no agrego un fundido, le cambio el alpha a
//           todo el motor
//   ley 2   EL ANIDADO MULTIPLICA. Es el algebra que `AnimOutput::compose` ya
//           usa para el canal alpha (`o.alpha = a.alpha * b.alpha`). Dos
//           ambitos de 0.5 dan 0.25, no 0.5
//   ley 3   AL SALIR SE RESTAURA EL VALOR ANTERIOR, no 1.0. Restaurar a 1.0
//           haria que un ambito interno le APAGUE el fundido al de afuera al
//           cerrarse — y como el de afuera sigue dibujando, se veria como un
//           salto de opacidad a mitad de la entrada
//   ley 4   EL CLAMP ES AL ENTRAR, no al aplicar. Asi el estado vivo siempre es
//           sano y ningun sitio de dibujo puede leer un alpha fuera de rango,
//           aunque el tema escriba -3. Un valor no finito cae a 1.0 (identidad):
//           un numero roto no puede BORRAR la interfaz. La VOZ de ese error es
//           del parser del tema, no de aca: este es el ultimo cinturon
// ============================================================================

#pragma once

#include "Types.h"

namespace eclips
{
    namespace draw2d
    {
        // ── El estado vivo ──────────────────────────────────────────────────
        //
        // Global de render, mono-hilo, igual que `g_batch`. No se toca a mano:
        // lo mueve `GroupAlphaScope`, que garantiza el par entrada/salida.
        // Arranca en la identidad, asi que un motor que nunca abre un ambito
        // dibuja bit-identico a antes de que este archivo existiera.
        inline f32 g_groupAlpha = 1.0f;

        // ── ley 4: el saneador ──────────────────────────────────────────────
        //
        // El `!(x >= 0.0f)` no es un `x < 0.0f` mal escrito: asi tambien atrapa
        // NaN, que compara falso contra todo.
        inline f32 sanitizeGroupAlpha(f32 a)
        {
            if (!(a >= 0.0f)) return 1.0f;   // negativo o NaN -> identidad
            if (a > 1.0f)     return 1.0f;
            return a;
        }

        // ── ley 2: la composicion ───────────────────────────────────────────
        //
        // Pura y aparte del ambito para que el harness pueda probarla sin
        // construir nada. Mismo algebra que `AnimOutput::compose` para alpha.
        inline f32 composeGroupAlpha(f32 outer, f32 inner)
        {
            return sanitizeGroupAlpha(outer) * sanitizeGroupAlpha(inner);
        }

        // ── ley 1: el punto unico que consumen los sitios de dibujo ─────────
        //
        // Los tres consumidores llaman ESTO y nada mas. En reposo devuelve su
        // argumento sin tocarlo (`a * 1.0f == a`, exacto en IEEE-754 para todo
        // `a` finito).
        inline f32 applyGroupAlpha(f32 a)
        {
            return a * g_groupAlpha;
        }

        // ── ley 3: el ambito ────────────────────────────────────────────────
        //
        // RAII para que no exista la version rota: no hay forma de abrir sin
        // cerrar, ni siquiera con un `return` temprano a mitad del render (la
        // caja tiene varios). No copiable a proposito — dos copias restaurarian
        // dos veces.
        struct GroupAlphaScope
        {
            explicit GroupAlphaScope(f32 a)
                : m_prev(g_groupAlpha)
            {
                g_groupAlpha = composeGroupAlpha(m_prev, a);
            }

            ~GroupAlphaScope() { g_groupAlpha = m_prev; }

            GroupAlphaScope(const GroupAlphaScope&) = delete;
            GroupAlphaScope& operator=(const GroupAlphaScope&) = delete;

        private:
            f32 m_prev;
        };

    } // namespace draw2d
} // namespace eclips
