// ============================================================================
// Draw2D.h — Funciones de dibujo 2D de Eclips
// ============================================================================
// Funciones inline para dibujo 2D con OpenGL.
//
// NOTA TECNICA: Las coordenadas UV de textura usan V invertida
// (1.0 arriba, 0.0 abajo) porque:
//   1. stbi_set_flip_vertically_on_load(true) en Texture.cpp
//      voltea los pixeles para OpenGL (fila 0 = abajo)
//   2. Nuestra proyeccion es top-down: glOrtho(0, w, h, 0, -1, 1)
//      donde Y=0 esta arriba de la pantalla
//   3. Para que la imagen se vea correcta, mapeamos:
//      vertice superior (Y pequeño) → texcoord V=1.0 (arriba de la imagen)
//      vertice inferior (Y grande)  → texcoord V=0.0 (abajo de la imagen)
// ============================================================================

#pragma once

#include "Texture.h"
#include "SpriteBatch.h"
#include "../core/Types.h"
// `caja-entrada`: el fundido de GRUPO (GroupAlphaScope + applyGroupAlpha).
// Header puro, probado por harness_grupo. Lo consumen los prims de glBegin de
// abajo; todo lo que pasa por el batch ya lo aplica SpriteBatch::draw.
#include "../core/GroupAlpha.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include "Renderer.h"
#include <algorithm>
#include <cmath>

namespace eclips
{
    namespace draw2d
    {
        // ── Inyeccion del SpriteBatch (Render Core Fase 1, paso 2) ──────────
        // Renderer setea este puntero en init() (ownership vive en Renderer
        // via unique_ptr). Las funciones de Draw2D que renderean texturas leen
        // de aqui para enrutar al pipeline moderno. Contrato:
        //   - Renderer es quien llama setSpriteBatch (Engine solo orquesta).
        //   - Antes de Engine::init terminar, esto es nullptr — ningun
        //     gamestate puede llamar drawBackground/drawCentered/etc antes.
        //   - setSpriteBatch(nullptr) corre en shutdown ANTES de destruir
        //     el SpriteBatch (Renderer::shutdown / Engine).
        // No hay getter publico — Draw2D encapsula su batch. Si alguien
        // necesita el batch directamente, lo pide a Engine.
        inline SpriteBatch* g_batch = nullptr;

        inline void setSpriteBatch(SpriteBatch* b) { g_batch = b; }
        // ── Resolucion virtual: viewport con letterboxing/pillarboxing ──
        //
        // `limpiar` (intro-handoff, 2026-08-01): esta funcion no solo configura
        // el viewport — tambien BORRA la pantalla entera a negro opaco. Como
        // todo GameState la llama como primera linea de su render(), y
        // `StateStack::render` dibuja de abajo hacia arriba, **el state de
        // arriba borra lo que el de abajo acaba de dibujar**: `isTransparent()`
        // no puede funcionar mientras el de arriba limpie.
        //
        // Medido con `sonda_handoff.ecl` + `glReadPixels`: el `.ecl` de abajo
        // dibujaba su lienzo (`rgba=0,153,0` al salir de su render) y el frame
        // compuesto salia `93,93,93` — gris puro, sin una gota de verde. Con
        // `limpiar=false` los canales R y B quedaron identicos digito por
        // digito y solo se movio el verde. Un A/B de una linea.
        //
        // El default es `true`: nadie cambia de conducta salvo quien lo pida.
        // Callers conocidos con `limpiar=false`: el splash durante su entrega
        // (`EclipsSplashState::revelaLoDeAbajo`) y VNScriptState. Los demas
        // states transparentes NO se tocaron en este lote — su radio real
        // esta sin medir, y son su propio ticket (ficha `TRANSPARENCIA-CLEAR`).
        //
        // Quien pase `false` asume una deuda: **alguien mas abajo tiene que
        // limpiar ese frame**, o el buffer acumula basura. Hoy se cumple solo
        // porque el state de abajo tambien llama a esta funcion.
        //
        // Cuerpo unico en Renderer::setupViewport (static inline). Esta
        // fachada conserva firma/default y delega — sin instancia.
        inline void setupViewport(int windowW, int windowH, int virtualW, int virtualH,
                                  bool limpiar = true)
        {
            Renderer::setupViewport(windowW, windowH, virtualW, virtualH, limpiar);
        }

        // ── Proyeccion ortografica 2D (Y=0 arriba, Y=height abajo) ──
                // Sincroniza fixed-function (matrix stack) Y el SpriteBatch.
        inline void setupProjection(f32 width, f32 height)
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0.0, static_cast<double>(width),
                static_cast<double>(height), 0.0,
                -1.0, 1.0);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            // Sincronizar SpriteBatch para que los wrappers (drawBackground,
            // etc.) renderen con la misma projection.
            if (g_batch) g_batch->setProjection(0.0f, width, height, 0.0f);
        }

        // ── Proyeccion con offset arbitrario (screen shake, FBO compositing) ──
        // Llamar despues de setupProjection cuando se necesite una projection
        // no-plana. Sincroniza fixed-function y SpriteBatch.
        inline void setupProjectionOffset(f32 left, f32 right, f32 bottom, f32 top)
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho((double)left, (double)right,
                (double)bottom, (double)top, -1.0, 1.0);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            if (g_batch) g_batch->setProjection(left, right, bottom, top);
        }

        // ── Scissor por-rect en coords VIRTUALES (clip duro: la consola, CP4) ──
        // Recorta el dibujo a un rect dado en coords virtuales (Y-down, origin
        // arriba-izq). Lee el viewport GL actual (glGetIntegerv) y escala por
        // viewport::width()/height() → robusto tanto si se renderiza al FBO
        // virtual (scale≈1) como directo a la ventana letterboxed. GL scissor
        // tiene origin abajo-izq, así que la Y se voltea. El SCISSOR_TEST ya
        // está activo a nivel de frame (Draw2D setup); esto solo angosta la caja.
        // OJO: el SpriteBatch bufferea; el caller debe FLUSHEAR (end/begin) para
        // que el draw ocurra con el scissor activo (patrón immediate, ver glitch).
        inline void pushScissorVirtual(f32 vx, f32 vy, f32 vw, f32 vh)
        {
            GLint vp[4];
            glGetIntegerv(GL_VIEWPORT, vp);
            f32 sx = (f32)vp[2] / (f32)viewport::width();
            f32 sy = (f32)vp[3] / (f32)viewport::height();
            GLint x = (GLint)std::floor(vp[0] + vx * sx);
            GLint y = (GLint)std::floor((vp[1] + vp[3]) - (vy + vh) * sy);
            GLint w = (GLint)std::ceil(vw * sx);
            GLint h = (GLint)std::ceil(vh * sy);
            if (w < 0) w = 0;
            if (h < 0) h = 0;
            glScissor(x, y, w, h);
        }

        // Restaura la caja de scissor al viewport completo (no desactiva el test).
        inline void popScissorVirtual()
        {
            GLint vp[4];
            glGetIntegerv(GL_VIEWPORT, vp);
            glScissor(vp[0], vp[1], vp[2], vp[3]);
        }

        // ── Primitivas tinted via SpriteBatch (Camino A — pre-Fase 2) ──
        // Los 4 wrappers siguientes enrutan rectangulos solidos y bordes
        // por el SpriteBatch usando la textura blanca 1x1 del propio batch
        // como portador del tint. Reemplazan el camino immediate mode
        // (glBegin/glEnd + toggle GL_TEXTURE_2D) que vivia aqui antes.
        //
        // Patron D5-A (heredado de paso 6, texto outlined):
        //   - drawRect / drawRectBorder       → wrapper con begin/end propios.
        //                                       Usar para una primitiva aislada.
        //   - drawRect_noBatch / *_noBatch    → solo emiten draw(). Asumen
        //                                       que el caller ya abrio batch.
        //                                       Para Fase 2: agrupar N
        //                                       primitivas consecutivas en
        //                                       un solo begin/end externo.
        //
        // Defensivo: si g_batch==nullptr (no deberia tras Engine::init),
        // las 4 retornan en silencio. Sin fallback immediate.

        // ── Rectangulo solido (variante para batch externo) ──
        inline void drawRect_noBatch(f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a = 1.0f)
        {
            if (!g_batch) return;
            g_batch->draw(g_batch->getWhiteTexture(),
                x, y, w, h,
                0.0f, 1.0f, 1.0f, 0.0f,   // full-texture, V=1 arriba
                r, g, b, a);
        }

        // ── Rectangulo solido (wrapper con begin/end propios) ──
        inline void drawRect(f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a = 1.0f)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawRect_noBatch(x, y, w, h, r, g, b, a);
            g_batch->end();
        }

        // ── Borde rectangular (variante para batch externo) ──
        // 4 rects (top/bottom/left/right). Sin begin/end propio.
        inline void drawRectBorder_noBatch(f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a = 1.0f,
            f32 thickness = 2.0f)
        {
            drawRect_noBatch(x, y, w, thickness, r, g, b, a);                  // top
            drawRect_noBatch(x, y + h - thickness, w, thickness, r, g, b, a);  // bottom
            drawRect_noBatch(x, y, thickness, h, r, g, b, a);                  // left
            drawRect_noBatch(x + w - thickness, y, thickness, h, r, g, b, a);  // right
        }

        // ── Borde rectangular (wrapper con begin/end propios) ──
        // Los 4 lados van en UN solo flush, no 4. Mejora real vs el path
        // immediate viejo (que hacia 4 glBegin/glEnd separados).
        inline void drawRectBorder(f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a = 1.0f,
            f32 thickness = 2.0f)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawRectBorder_noBatch(x, y, w, h, r, g, b, a, thickness);
            g_batch->end();
        }

        // ── Rectángulo con esquinas redondeadas ──
        // radius se clampa a min(w,h)/2. Si radius <= 0.5 cae a drawRect.
        // cornerSegments controla suavidad del arco (6 = barato y se ve liso).
        inline void drawRoundedRect(f32 x, f32 y, f32 w, f32 h, f32 radius,
            f32 r, f32 g, f32 b, f32 a = 1.0f,
            int cornerSegments = 6)
        {
            radius = std::min(radius, std::min(w, h) * 0.5f);
            if (radius <= 0.5f) { drawRect(x, y, w, h, r, g, b, a); return; }

            glDisable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            // Fundido de grupo. OJO: va DESPUES del early-return de arriba —
            // ese camino delega en drawRect, que entra al batch y ya lo aplica.
            // Aplicarlo tambien aca haria que una caja de radio chico funda al
            // CUADRADO, y en pantalla se veria como una entrada "mas rapida"
            // que la del resto de la caja, no como un bug.
            glColor4f(r, g, b, applyGroupAlpha(a));

            const f32 PI = 3.14159265f;
            const f32 HALF_PI = 1.5707963f;
            f32 cx_tl = x + radius, cy_tl = y + radius;
            f32 cx_tr = x + w - radius, cy_tr = y + radius;
            f32 cx_br = x + w - radius, cy_br = y + h - radius;
            f32 cx_bl = x + radius, cy_bl = y + h - radius;
            f32 pivotX = x + w * 0.5f, pivotY = y + h * 0.5f;

            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(pivotX, pivotY);
            // Convención: Y crece hacia abajo. Ángulo 0 = derecha, π/2 = abajo, π = izq, 3π/2 = arriba.
            // Top-left: π → 3π/2
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = PI + (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_tl + std::cos(ang) * radius, cy_tl + std::sin(ang) * radius);
            }
            // Top-right: 3π/2 → 2π
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = PI + HALF_PI + (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_tr + std::cos(ang) * radius, cy_tr + std::sin(ang) * radius);
            }
            // Bottom-right: 0 → π/2
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_br + std::cos(ang) * radius, cy_br + std::sin(ang) * radius);
            }
            // Bottom-left: π/2 → π
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = HALF_PI + (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_bl + std::cos(ang) * radius, cy_bl + std::sin(ang) * radius);
            }
            glVertex2f(x, y + radius);  // ← cierra el fan al primer vértice del arco TL
            glEnd();

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Borde redondeado (solo contorno) ──
        inline void drawRoundedRectBorder(f32 x, f32 y, f32 w, f32 h, f32 radius,
            f32 r, f32 g, f32 b, f32 a = 1.0f,
            f32 thickness = 2.0f, int cornerSegments = 6)
        {
            radius = std::min(radius, std::min(w, h) * 0.5f);
            if (radius <= 0.5f) { drawRectBorder(x, y, w, h, r, g, b, a, thickness); return; }

            glDisable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glLineWidth(thickness);
            // Fundido de grupo — igual que su hermano relleno, DESPUES del
            // early-return que delega en drawRectBorder (batch).
            glColor4f(r, g, b, applyGroupAlpha(a));

            const f32 PI = 3.14159265f;
            const f32 HALF_PI = 1.5707963f;
            f32 cx_tl = x + radius, cy_tl = y + radius;
            f32 cx_tr = x + w - radius, cy_tr = y + radius;
            f32 cx_br = x + w - radius, cy_br = y + h - radius;
            f32 cx_bl = x + radius, cy_bl = y + h - radius;

            glBegin(GL_LINE_LOOP);
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = PI + (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_tl + std::cos(ang) * radius, cy_tl + std::sin(ang) * radius);
            }
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = PI + HALF_PI + (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_tr + std::cos(ang) * radius, cy_tr + std::sin(ang) * radius);
            }
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_br + std::cos(ang) * radius, cy_br + std::sin(ang) * radius);
            }
            for (int i = 0; i <= cornerSegments; ++i) {
                f32 ang = HALF_PI + (f32)i / (f32)cornerSegments * HALF_PI;
                glVertex2f(cx_bl + std::cos(ang) * radius, cy_bl + std::sin(ang) * radius);
            }
            glEnd();

            glLineWidth(1.0f);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Círculo relleno (Tier 4C.4: rango de torres en modo construcción) ──
        // `radiusY` ESPEJA el otro eje cuando va en negativo, y ese default es lo
        // que deja byte-intactos los seis call-sites que ya existen: un circulo
        // es el caso `radius == radiusY`, no un caso aparte. Con dos radios
        // distintos el mismo fan da una ELIPSE, y con `segments = 4` da un ROMBO
        // de `2*radius` por `2*radiusY` -que es la perilla de Sunrider, medida al
        // pixel en 9x12 (`FORMAS_01`)-.
        //
        // La ROTACION no hace falta, y esta medido: el fan arranca en angulo 0,
        // asi que con cuatro segmentos los vertices caen en 0/90/180/270 grados y
        // eso YA es un rombo. La ficha `FORMAS-SIN-VOCABULARIO` decia que *"un
        // cuadrado girado 45 grados no es salida"*: cierto, e irrelevante.
        inline void drawCircle(f32 cx, f32 cy, f32 radius,
            f32 r, f32 g, f32 b, f32 a = 1.0f,
            int segments = 32, f32 radiusY = -1.0f)
        {
            if (radiusY < 0.0f) radiusY = radius;
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Fundido de grupo: los prims de glBegin no pasan por el batch, asi
            // que lo aplican de a uno. Que TODOS lo hagan (y no solo los dos que
            // usa la caja hoy) es lo que sostiene la ley del ambito — "lo de
            // adentro funde" no puede tener excepciones sin nombre.
            glColor4f(r, g, b, applyGroupAlpha(a));
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for (int i = 0; i <= segments; i++)
            {
                f32 angle = static_cast<f32>(i) * 2.0f * 3.14159265f
                    / static_cast<f32>(segments);
                glVertex2f(cx + std::cos(angle) * radius,
                    cy + std::sin(angle) * radiusY);
            }
            glEnd();

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Borde circular (solo contorno) ──
        // Gemelo de `drawCircle` y con su mismo `radiusY`, por una razon que no
        // es simetria: si el relleno pudiera ser rombo y el contorno no, una
        // perilla con borde declarado saldria con un rombo adentro de un circulo.
        inline void drawCircleBorder(f32 cx, f32 cy, f32 radius,
            f32 r, f32 g, f32 b, f32 a = 1.0f,
            int segments = 32, f32 thickness = 2.0f, f32 radiusY = -1.0f)
        {
            if (radiusY < 0.0f) radiusY = radius;
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glLineWidth(thickness);
            glColor4f(r, g, b, applyGroupAlpha(a));   // fundido de grupo
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < segments; i++)
            {
                f32 angle = static_cast<f32>(i) * 2.0f * 3.14159265f
                    / static_cast<f32>(segments);
                glVertex2f(cx + std::cos(angle) * radius,
                    cy + std::sin(angle) * radiusY);
            }
            glEnd();
            glLineWidth(1.0f);

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Segmentos proporcionales al radio (§1.40: una sola fuente) ──
        // ~1 segmento cada ~7px de perímetro (2πr·0.9 aprox). A r=330 → ~297,
        // faceta sub-perceptible. Clamp sano. Lo usan glow Y discos grandes.
        inline int circleSegments(f32 radius)
        {
            int s = (int)(radius * 1.6f);   // ~4px/segmento: sub-perceptible
            if (s < 48)   s = 48;
            if (s > 1024) s = 1024;          // techo alto: es boot one-shot (§4)
            return s;
        }

        // ── Círculo de LUZ con desvanecimiento radial real (§1.38) ──
        // Color por vértice: centro lleno → perímetro alpha CERO. A diferencia
        // de drawCircle (alpha uniforme, corta seco), esto cae a cero suave =
        // glow celestial. Blending ADITIVO: la luz se SUMA (ideal sobre negro).
        // segments<=0 → auto-proporcional al radio. Restaura blend normal al salir.
        inline void drawCircleGlow(f32 cx, f32 cy, f32 radius,
            f32 r, f32 g, f32 b, f32 aCenter,
            int segments = 0)
        {
            if (segments <= 0) segments = circleSegments(radius);

            glDisable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // aditivo

            glBegin(GL_TRIANGLE_FAN);
            // Fundido de grupo: solo el centro lo necesita — el borde ya es 0
            // y cero por cualquier factor sigue siendo cero.
            glColor4f(r, g, b, applyGroupAlpha(aCenter));  // centro: opaco
            glVertex2f(cx, cy);
            glColor4f(r, g, b, 0.0f);             // borde: transparente
            for (int i = 0; i <= segments; i++)
            {
                f32 angle = static_cast<f32>(i) * 2.0f * 3.14159265f
                    / static_cast<f32>(segments);
                glVertex2f(cx + std::cos(angle) * radius,
                    cy + std::sin(angle) * radius);
            }
            glEnd();

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // restaura normal
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Textura fullscreen (variante para batch externo) ──
        inline void drawBackground_noBatch(const Texture& tex, f32 vw, f32 vh)
        {
            if (!g_batch) return;
            g_batch->draw(tex.getID(),
                0.0f, 0.0f, vw, vh,
                0.0f, 1.0f, 1.0f, 0.0f,
                1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Textura que llena toda la pantalla (wrapper con begin/end propios) ──
        inline void drawBackground(const Texture& tex, f32 vw, f32 vh)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawBackground_noBatch(tex, vw, vh);
            g_batch->end();
        }

        // ── Textura centrada en pantalla (variante para batch externo) ──
                // Caller real único hoy: TestState.h. Los personajes VN del path
                // principal NO pasan por aquí (van por VNScriptState::drawSingleSprite
                // / drawCompositeChar — paso 4).
        inline void drawCentered_noBatch(const Texture& tex, f32 vw, f32 vh, f32 scale = 1.0f)
        {
            if (!g_batch) return;
            f32 tw = static_cast<f32>(tex.getWidth()) * scale;
            f32 th = static_cast<f32>(tex.getHeight()) * scale;

            if (th > vh * 0.85f)
            {
                f32 fitScale = (vh * 0.85f) / th;
                tw *= fitScale;
                th *= fitScale;
            }

            f32 x = (vw - tw) / 2.0f;
            f32 y = vh - th;

            g_batch->draw(tex.getID(),
                x, y, tw, th,
                0.0f, 1.0f, 1.0f, 0.0f,
                1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Textura centrada en pantalla (wrapper con begin/end propios) ──
        inline void drawCentered(const Texture& tex, f32 vw, f32 vh, f32 scale = 1.0f)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawCentered_noBatch(tex, vw, vh, scale);
            g_batch->end();
        }

        // ════════════════════════════════════════════════════════════════════
        // Funciones para Gameplay 2D (Fase 3)
        // ════════════════════════════════════════════════════════════════════

        // ── Textura en posicion especifica (variante para batch externo) ──
        inline void drawTexture_noBatch(const Texture& tex, f32 x, f32 y, f32 w, f32 h)
        {
            if (!g_batch) return;
            g_batch->draw(tex.getID(),
                x, y, w, h,
                0.0f, 1.0f, 1.0f, 0.0f,   // UV: V-flipped (stb-flip + top-down)
                1.0f, 1.0f, 1.0f, 1.0f);   // tint blanco opaco
        }

        // ── Textura en posicion especifica (wrapper con begin/end propios) ──
        inline void drawTexture(const Texture& tex, f32 x, f32 y, f32 w, f32 h)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawTexture_noBatch(tex, x, y, w, h);
            g_batch->end();
        }

        // ── Textura con opacidad (variante para batch externo) ──
        inline void drawTextureEx_noBatch(const Texture& tex, f32 x, f32 y, f32 w, f32 h,
            f32 alpha)
        {
            if (!g_batch) return;
            g_batch->draw(tex.getID(),
                x, y, w, h,
                0.0f, 1.0f, 1.0f, 0.0f,
                1.0f, 1.0f, 1.0f, alpha);
        }

        // ── Textura con opacidad (wrapper con begin/end propios) ──
        inline void drawTextureEx(const Texture& tex, f32 x, f32 y, f32 w, f32 h,
            f32 alpha)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawTextureEx_noBatch(tex, x, y, w, h, alpha);
            g_batch->end();
        }

        // ── Textura con tint+alpha (variante para batch externo) ──
        inline void drawTextureTinted_noBatch(GLuint texID, f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a)
        {
            if (!g_batch) return;
            g_batch->draw(texID,
                x, y, w, h,
                0.0f, 1.0f, 1.0f, 0.0f,
                r, g, b, a);
        }

        inline void drawTextureTinted_noBatch(const Texture& tex, f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a)
        {
            drawTextureTinted_noBatch(tex.getID(), x, y, w, h, r, g, b, a);
        }

        // ── Textura con tint+alpha (wrapper con begin/end propios) ──
        inline void drawTextureTinted(GLuint texID, f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawTextureTinted_noBatch(texID, x, y, w, h, r, g, b, a);
            g_batch->end();
        }

        inline void drawTextureTinted(const Texture& tex, f32 x, f32 y, f32 w, f32 h,
            f32 r, f32 g, f32 b, f32 a)
        {
            drawTextureTinted(tex.getID(), x, y, w, h, r, g, b, a);
        }

        // ── Ajuste de textura en una caja · fit modes (CP2, ranura-norte) ──
        // Mete la textura en la caja (bx,by,bw,bh) segun un modo, respetando
        // la convencion V-flip del batch (base u0,v0,u1,v1 = 0,1,1,0). Opaco
        // blanco como drawTexture_noBatch (sin caller de tint/alpha aun, §7.118).
        //   Estirar  = llena la caja, deforma (rect=caja, UV completa).
        //   Contener = cabe entero centrado, letterbox (rect menor, UV completa).
        //   Cubrir   = llena la caja, recorta el eje sobrante VIA UV (rect=caja) —
        //              el look Ren'Py/DDLC, sin scissor (§7.38: el batch ya toma UV).
        // (enum TextureFit vive en Types.h — vocabulario compartido con Theme.)
        inline void drawTextureFit_noBatch(const Texture& tex,
            f32 bx, f32 by, f32 bw, f32 bh, TextureFit mode)
        {
            if (!g_batch) return;
            const f32 tw = (f32)tex.getWidth();
            const f32 th = (f32)tex.getHeight();
            if (tw <= 0.0f || th <= 0.0f || bw <= 0.0f || bh <= 0.0f) return;

            f32 u0 = 0.0f, v0 = 1.0f, u1 = 1.0f, v1 = 0.0f;   // V-flip base
            f32 dx = bx, dy = by, dw = bw, dh = bh;

            const f32 ta = tw / th;   // aspecto de la textura
            const f32 ba = bw / bh;   // aspecto de la caja

            if (mode == TextureFit::Contener)
            {
                // Escala minima que cabe; rect menor, centrado (= min(bw/tw,bh/th)).
                const f32 s = (ba > ta) ? (bh / th) : (bw / tw);
                dw = tw * s; dh = th * s;
                dx = bx + (bw - dw) * 0.5f;
                dy = by + (bh - dh) * 0.5f;
            }
            else if (mode == TextureFit::Cubrir)
            {
                // Rect = caja; recorta el eje sobrante insetando la UV, centrado.
                if (ta > ba)   // textura mas ancha → recorta los lados (inset U)
                {
                    const f32 inset = (1.0f - ba / ta) * 0.5f;
                    u0 = inset; u1 = 1.0f - inset;
                }
                else           // textura mas alta → recorta arriba/abajo (inset V)
                {
                    const f32 inset = (1.0f - ta / ba) * 0.5f;
                    v0 = 1.0f - inset; v1 = inset;
                }
            }
            // Estirar = defaults (rect=caja, UV completa) → sin rama.

            g_batch->draw(tex.getID(), dx, dy, dw, dh,
                u0, v0, u1, v1, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        // ── Sub-region de textura (variante para batch externo) ──
        // u0,v0 = esquina superior-izquierda, u1,v1 = esquina inferior-derecha.
        // UVs ya invertidos por el caller (SpriteSheet).
        inline void drawSpriteFrame_noBatch(const Texture& tex,
            f32 x, f32 y, f32 w, f32 h,
            f32 u0, f32 v0, f32 u1, f32 v1,
            f32 alpha = 1.0f)
        {
            if (!g_batch) return;
            g_batch->draw(tex.getID(),
                x, y, w, h,
                u0, v0, u1, v1,
                1.0f, 1.0f, 1.0f, alpha);
        }

        // ── Sub-region de textura (wrapper con begin/end propios) ──
        inline void drawSpriteFrame(const Texture& tex,
            f32 x, f32 y, f32 w, f32 h,
            f32 u0, f32 v0, f32 u1, f32 v1,
            f32 alpha = 1.0f)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawSpriteFrame_noBatch(tex, x, y, w, h, u0, v0, u1, v1, alpha);
            g_batch->end();
        }

        // ─────────────────────────────────────────────────────────────────────
        // 9-slice: textura escalada preservando esquinas y bordes
        // (variante para batch externo)
        // ─────────────────────────────────────────────────────────────────────
        // Divide la textura fuente en 9 regiones (4 esquinas + 4 bordes + 1
        // centro) y las emite como 9 quads independientes en el batch.
        // Solución estándar de industria para UI escalable (cajas de diálogo,
        // botones, paneles con esquinas redondeadas que NO se deben estirar
        // al cambiar el tamaño del rect destino).
        //
        //   ┌────┬─────────┬────┐ ◄─ bT (px del source que NO se estira)
        //   │ TL │   TOP   │ TR │
        //   ├────┼─────────┼────┤
        //   │ L  │ CENTER  │ R  │  ← centro estira libre en ambas dimensiones
        //   ├────┼─────────┼────┤
        //   │ BL │ BOTTOM  │ BR │
        //   └────┴─────────┴────┘ ◄─ bB
        //   ▲   ▲                ▲
        //   │   bL              bR
        //   0   xL              xR
        //
        // Esquinas (TL, TR, BL, BR) mantienen tamaño nativo. Bordes laterales
        // (L, R) escalan solo en Y; bordes top/bottom (TOP, BOTTOM) escalan
        // solo en X. Centro estira libre.
        //
        // PARAMETROS:
        //   x, y, w, h       — rect de destino en virtual coords.
        //   bL, bR, bT, bB   — bordes en pixels del source (NO estirables).
        //   r, g, b, a       — tint multiplicativo.
        //
        // PRECONDICION: bL+bR <= texW y bT+bB <= texH validados por el caller
        // (pinThemeSprites en VNScriptState, una vez al cargar el theme).
        // Aquí solo se valida overflow del destination (bL+bR > w o bT+bB > h):
        // fallback silente a single-quad stretch para evitar quads degenerados
        // con dimensiones negativas. Sin log: este caso es runtime defensivo,
        // no error del creador (típico: widthRatio extremo o nameplate corto).
        //
        // BORDES PARCIALES: si solo un par está declarado (ej. bL=30, bR=30,
        // bT=0, bB=0), las 6 quads de la dimensión-0 son degeneradas (área
        // cero, GPU las descarta). Resultado efectivo: 3-slice horizontal.
        // Mismo principio para 3-slice vertical (bT/bB > 0, bL/bR = 0).
        //
        // CONVENCION UV: V=1 arriba, V=0 abajo (stb-flip + top-down ortho),
        // idéntica al resto de draw2d::*. Las 4 stops de U (0, uL, uR, 1) y
        // de V (1, vT, vB, 0) mapean al rect destino (x, xL, xR, x+w) y
        // (y, yT, yB, y+h).
        inline void drawTexture9Slice_noBatch(const Texture& tex,
            f32 x, f32 y, f32 w, f32 h,
            f32 bL, f32 bR, f32 bT, f32 bB,
            f32 r, f32 g, f32 b, f32 a)
        {
            if (!g_batch) return;

            // Destination overflow: bordes no caben en el rect → fallback silente.
            if (bL + bR > w || bT + bB > h)
            {
                g_batch->draw(tex.getID(),
                    x, y, w, h,
                    0.0f, 1.0f, 1.0f, 0.0f,
                    r, g, b, a);
                return;
            }

            const f32 texW = (f32)tex.getWidth();
            const f32 texH = (f32)tex.getHeight();

            // 4 stops X en destino + 4 stops U en source.
            const f32 xL = x + bL;
            const f32 xR = x + w - bR;
            const f32 uL = bL / texW;
            const f32 uR = 1.0f - bR / texW;

            // 4 stops Y en destino + 4 stops V en source (V=1 arriba, V=0 abajo).
            const f32 yT = y + bT;
            const f32 yB = y + h - bB;
            const f32 vT = 1.0f - bT / texH;
            const f32 vB = bB / texH;

            const u32 id = tex.getID();

            // Anchos/altos del centro (garantizados >= 0 tras el check de overflow).
            const f32 cw = xR - xL;
            const f32 ch = yB - yT;

            // ── Fila superior: TL, TOP, TR ──
            g_batch->draw(id, x, y, bL, bT, 0.0f, 1.0f, uL, vT, r, g, b, a);
            g_batch->draw(id, xL, y, cw, bT, uL, 1.0f, uR, vT, r, g, b, a);
            g_batch->draw(id, xR, y, bR, bT, uR, 1.0f, 1.0f, vT, r, g, b, a);

            // ── Fila central: LEFT, CENTER, RIGHT ──
            g_batch->draw(id, x, yT, bL, ch, 0.0f, vT, uL, vB, r, g, b, a);
            g_batch->draw(id, xL, yT, cw, ch, uL, vT, uR, vB, r, g, b, a);
            g_batch->draw(id, xR, yT, bR, ch, uR, vT, 1.0f, vB, r, g, b, a);

            // ── Fila inferior: BL, BOTTOM, BR ──
            g_batch->draw(id, x, yB, bL, bB, 0.0f, vB, uL, 0.0f, r, g, b, a);
            g_batch->draw(id, xL, yB, cw, bB, uL, vB, uR, 0.0f, r, g, b, a);
            g_batch->draw(id, xR, yB, bR, bB, uR, vB, 1.0f, 0.0f, r, g, b, a);
        }

        // ── 9-slice (wrapper con begin/end propios) ──
        inline void drawTexture9Slice(const Texture& tex,
            f32 x, f32 y, f32 w, f32 h,
            f32 bL, f32 bR, f32 bT, f32 bB,
            f32 r, f32 g, f32 b, f32 a)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawTexture9Slice_noBatch(tex, x, y, w, h, bL, bR, bT, bB, r, g, b, a);
            g_batch->end();
        }

        // ── Fondo de boton/panel: PNG (9-slice o stretch) si hay sprite, si no
        //    rect+borde. El texto lo pone el caller encima. Primitivo ASSET-FREE:
        //    el sprite es un ATAJO opcional; sin el, color+forma alcanzan el look
        //    (doctrina Eclips). Batch-neutral: requiere batch abierto, lo deja
        //    abierto (puro _noBatch). Consumidores: choices/chrome/marco/OK de
        //    entrada (VNDialogBox) y la tarjeta de ranura (VNScriptState · CP2).
        inline void drawButtonBg(f32 x, f32 y, f32 w, f32 h,
            Texture* sprite, bool use9Slice,
            f32 slL, f32 slR, f32 slT, f32 slB,
            f32 bgR, f32 bgG, f32 bgB, f32 bgA,
            f32 brR, f32 brG, f32 brB, f32 brA,
            f32 borderThickness)
        {
            if (sprite)
            {
                // PNG manda: tint blanco, alpha = bgA (multiplicador).
                if (use9Slice)
                    drawTexture9Slice_noBatch(*sprite, x, y, w, h,
                        slL, slR, slT, slB, 1.0f, 1.0f, 1.0f, bgA);
                else
                    drawTextureTinted_noBatch(*sprite, x, y, w, h,
                        1.0f, 1.0f, 1.0f, bgA);
            }
            else
            {
                drawRect_noBatch(x, y, w, h, bgR, bgG, bgB, bgA);
                drawRectBorder_noBatch(x, y, w, h,
                    brR, brG, brB, brA, borderThickness);
            }
        }

        // ── Sub-region con tint de color (para ciclo dia/noche) ─────────────
        inline void drawSpriteFrameTinted(const Texture& tex,
            f32 x, f32 y, f32 w, f32 h,
            f32 u0, f32 v0, f32 u1, f32 v1,
            f32 r, f32 g, f32 b, f32 a = 1.0f)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex.getID());
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4f(r, g, b, applyGroupAlpha(a));   // fundido de grupo
            glBegin(GL_QUADS);
            glTexCoord2f(u0, v0); glVertex2f(x, y);
            glTexCoord2f(u1, v0); glVertex2f(x + w, y);
            glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
            glTexCoord2f(u0, v1); glVertex2f(x, y + h);
            glEnd();

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glDisable(GL_TEXTURE_2D);
        }

        // ── Textura completa con tint de color (fondos con dia/noche) ───────
        // El cuerpo original era immediate-mode pre-batch: dentro del batch del
        // state habría pintado FUERA de orden. Mismo par _noBatch/wrapper que
        // drawBackground; el batch ya acepta rgba por vértice.
        inline void drawBackgroundTinted_noBatch(const Texture& tex, f32 vw, f32 vh,
            f32 r, f32 g, f32 b, f32 a = 1.0f)
        {
            drawTextureTinted_noBatch(tex, 0.0f, 0.0f, vw, vh, r, g, b, a);
        }

        inline void drawBackgroundTinted(const Texture& tex, f32 vw, f32 vh,
            f32 r, f32 g, f32 b, f32 a = 1.0f)
        {
            if (!g_batch) return;
            g_batch->begin();
            drawBackgroundTinted_noBatch(tex, vw, vh, r, g, b, a);
            g_batch->end();
        }

    } // namespace draw2d
} // namespace eclips