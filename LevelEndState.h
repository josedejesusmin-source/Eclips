// ============================================================================
// LevelEndState.h — Pantalla de fin de nivel (Tier 4D)
// ============================================================================
//
// Overlay transparente que se pushea cuando un nivel 2D termina con resultado
// (victoria o derrota). Configurable desde el bloque `fin:` del .eclmap.
//
// ── Diseño ─────────────────────────────────────────────────────────────────
//
// - Construido con un LevelEndPreset + isVictory + callbacks + varLookup.
// - isTransparent() = true → el mundo 2D se ve detrás, tinted.
// - allowsUpdateBelow() = false → el juego se congela al abrirse.
// - ESC no cierra (no puedes escapar de un resultado).
// - Botones dinámicos: 2 (derrota/sin siguiente) o 3 (victoria + siguiente).
//
// ── Estilos soportados (Fase 3) ────────────────────────────────────────────
//
// - Classic: título + botones. El default.
// - KO:      pantalla dramática con shake + tint flash + fade-in.
// - Stars:   1-3 medallas doradas con cascada según criterios evaluados.
// - Tally:   desglose de stats con contadores animados de 0 → target.
//
// ── Animaciones de entrada (Fase 4) ────────────────────────────────────────
//
// El panel entra a escena con la animación configurada en `m_preset.animation`.
// Duración configurable vía `m_preset.entryDuration` (default 0.4s, parseado
// del campo `duracion_entrada`/`entry_duration` del .eclmap, clamp 0.05-3.0).
// Tipos:
//
// - None       → aparece instantáneo (sin animación).
// - Fade       → alpha 0 → 1, todo el contenido fade-in junto.
// - FromTop    → offsetY -VH → 0 con easing out-back (cae con overshoot).
// - FromBottom → offsetY +VH → 0 con easing out-back (sube con overshoot).
// - Zoom       → scale 0 → 1 con easing out-back (texto oculto durante la
//                entrada para evitar mismatch — los TextRenderer son bitmap).
//
// La cascada interna (estrellas, filas tally) usa `contentTime()` que
// descuenta la duración de entrada en FromTop/FromBottom/Zoom. En None y
// Fade, el contenido empieza desde t=0 (Fade lo hace todo junto con el
// panel; None es instantáneo).
//
// El dim overlay de fondo NO hereda la animación — aparece instantáneo
// para que el "click-through" no engañe al usuario durante la transición.
//
// ── Dependencias externas ──────────────────────────────────────────────────
//
// - varLookup(name): función que retorna el valor string de una var.
//   - Vars especiales del engine: "hp", "hp_max", "hp_base", "hp_base_max",
//     "oleada", "oleada_max", "tiempo".
//   - Vars custom del creador (setadas con `set:` o `world.set_var`).
//   - Fallback "0" si la var no existe.
//   - Si varLookup es null, Stars/Tally degradan graceful (0 estrellas / lista vacía).
//
// ── Sonido de entrada ──────────────────────────────────────────────────────
//
// El sonido `entry_sound` del preset se dispara desde
// `Gameplay2DState::pushLevelEnd()` (NO desde este state) usando el
// AudioManager existente. Esto mantiene a LevelEndState agnóstico al
// sistema de audio y reusa el binding `world.sound()`.
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Types.h"
#include "engine/core/Settings.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "MapData.h"

#include <SDL.h>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include "engine/core/LangStore.h"   // idiomas CP3: langUI() del chrome

#include <cmath>
#include <cstdio>
#include <algorithm>

namespace eclips
{

    class LevelEndState : public GameState
    {
    public:

        using VarLookup = std::function<std::string(const std::string&)>;

        // ── Constructor ────────────────────────────────────────────────────
        LevelEndState(SDL_Window*  window,
                      StateStack*  stack,
                      FontSourceStore* fontSources,
                      LevelEndPreset preset,
                      bool         isVictory,
                      std::function<void()> onRetry,
                      std::function<void()> onMainMenu,
                      std::function<void()> onNext = nullptr,
                      VarLookup    varLookup = nullptr)
            : m_window(window)
            , m_stack(stack)
            , m_fontSources(fontSources)
            , m_preset(std::move(preset))
            , m_isVictory(isVictory)
            , m_onRetry(std::move(onRetry))
            , m_onMainMenu(std::move(onMainMenu))
            , m_onNext(std::move(onNext))
            , m_varLookup(std::move(varLookup))
        {
            m_font.setSourceStore(m_fontSources);
            m_titleFont.setSourceStore(m_fontSources);
            m_small.setSourceStore(m_fontSources);
            m_hasNextButton = m_isVictory
                           && m_onNext
                           && !m_preset.nextMap.empty();

            // Inicializar transform según animación. Si es None, ya estamos
            // en estado final desde el primer frame. Si no, arranca en cero.
            initPanelTransform();
        }

        bool isTransparent()     const override { return true;  }
        bool allowsUpdateBelow() const override { return false; }

        // ── Ciclo de vida ──────────────────────────────────────────────────

        void onEnter() override
        {
            m_hasFont  = m_font.init(Settings::defaultFontPath(), 30.0f);
            m_hasTitle = m_titleFont.init(Settings::defaultFontPath(), 56.0f);
            m_hasSmall = m_small.init(Settings::defaultFontPath(), 22.0f);

            m_cursor = 0;
            m_time   = 0.0;

            SDL_GetWindowSize(m_window, &m_winW, &m_winH);

            // Pre-procesar según estilo
            if (m_preset.style == LevelEndStyle::Stars)
            {
                // Override de Lua tiene prioridad. -1 = usar criterios del .eclmap.
                m_stars = (m_preset.starsOverride >= 0)
                    ? m_preset.starsOverride
                    : computeStars();
            }

            if (m_preset.style == LevelEndStyle::Tally)
                preprocessTally();

            std::cout << "[LevelEnd] Pantalla de fin abierta ("
                      << (m_isVictory ? "victoria" : "derrota")
                      << ", estilo=" << styleName(m_preset.style)
                      << ", anim=" << animName(m_preset.animation);
            if (m_preset.style == LevelEndStyle::Stars)
                std::cout << ", estrellas=" << m_stars;
            if (m_preset.style == LevelEndStyle::Tally)
                std::cout << ", filas=" << m_tallyLabels.size();
            std::cout << ")" << std::endl;
        }

        void onExit() override
        {
            m_font.shutdown();
            m_titleFont.shutdown();
            m_small.shutdown();
            std::cout << "[LevelEnd] Pantalla de fin cerrada" << std::endl;
        }

        // ── Input ──────────────────────────────────────────────────────────

        bool handleEvent(const void* ev) override
        {
            const SDL_Event* e = static_cast<const SDL_Event*>(ev);

            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
            {
                float vx, vy;
                mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);
                int hit = hitTestButton(vx, vy);
                if (hit >= 0)
                {
                    m_cursor = hit;
                    executeAction(hit);
                }
                return true;
            }

            if (e->type == SDL_MOUSEMOTION)
            {
                float vx, vy;
                mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);
                int hit = hitTestButton(vx, vy);
                if (hit >= 0) m_cursor = hit;
                return true;
            }

            if (e->type != SDL_KEYDOWN) return true;
            SDL_Keycode k = e->key.keysym.sym;

            if (k == SDLK_ESCAPE) return true;

            int n = buttonCount();
            if (k == SDLK_UP || k == SDLK_w)
            { m_cursor = (m_cursor - 1 + n) % n; return true; }
            if (k == SDLK_DOWN || k == SDLK_s)
            { m_cursor = (m_cursor + 1) % n; return true; }
            if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_z)
            { executeAction(m_cursor); return true; }

            return true;
        }

        void update(f64 dt) override
        {
            m_time += dt;
            updatePanelTransform();
        }

        // ── Render ─────────────────────────────────────────────────────────

        void render(f64 /*alpha*/) override
        {
            if (!m_hasFont) return;

            SDL_GetWindowSize(m_window, &m_winW, &m_winH);
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();

            draw2d::setupViewport(m_winW, m_winH,
                                  viewport::width(), viewport::height());
            draw2d::setupProjection(VW, VH);

            const auto& c = m_preset.colors;

            // Dim overlay de fondo. NO hereda la animación de entrada — es
            // instantáneo. Razón: si el dim también fade-in, durante 0.4s
            // el usuario podría clickear elementos del juego "detrás" y
            // confundirse. El dim los bloquea visualmente desde t=0.
            draw2d::drawRect(0, 0, VW, VH, c.bgR, c.bgG, c.bgB, c.bgA * 0.7f);

            // Dispatch por estilo
            switch (m_preset.style)
            {
                case LevelEndStyle::KO:     renderKO(VW, VH);     break;
                case LevelEndStyle::Stars:  renderStars(VW, VH);  break;
                case LevelEndStyle::Tally:  renderTally(VW, VH);  break;
                case LevelEndStyle::Classic:
                default:                    renderClassic(VW, VH, 0, 0, 0); break;
            }
        }

    private:

        // ══════════════════════════════════════════════════════════════════
        // Animación de entrada del panel (Fase 4)
        // ══════════════════════════════════════════════════════════════════

        void initPanelTransform()
        {
            // Estado inicial según el tipo de animación. Para None, ya
            // estamos en el estado final desde el frame 0.
            switch (m_preset.animation)
            {
                case LevelEndAnimation::None:
                    m_panelOffX = 0; m_panelOffY = 0;
                    m_panelScale = 1.0f; m_panelAlpha = 1.0f;
                    break;
                case LevelEndAnimation::Fade:
                    m_panelOffX = 0; m_panelOffY = 0;
                    m_panelScale = 1.0f; m_panelAlpha = 0.0f;
                    break;
                case LevelEndAnimation::FromTop:
                    m_panelOffX = 0;
                    m_panelOffY = -(float)viewport::height();
                    m_panelScale = 1.0f; m_panelAlpha = 1.0f;
                    break;
                case LevelEndAnimation::FromBottom:
                    m_panelOffX = 0;
                    m_panelOffY = (float)viewport::height();
                    m_panelScale = 1.0f; m_panelAlpha = 1.0f;
                    break;
                case LevelEndAnimation::Zoom:
                    m_panelOffX = 0; m_panelOffY = 0;
                    m_panelScale = 0.0f; m_panelAlpha = 1.0f;
                    break;
            }
        }

        void updatePanelTransform()
        {
            // Progreso 0→1 a lo largo de la duración configurada en el preset.
            // Guard contra duraciones cero (parseo defensivo) — caería en
            // división por cero si llegara aquí.
            double dur = (double)m_preset.entryDuration;
            if (dur < 0.001) dur = 0.001;

            double t = m_time / dur;
            if (t >= 1.0) t = 1.0;

            switch (m_preset.animation)
            {
                case LevelEndAnimation::None:
                    m_panelOffX = 0; m_panelOffY = 0;
                    m_panelScale = 1.0f; m_panelAlpha = 1.0f;
                    break;

                case LevelEndAnimation::Fade:
                    // Lineal — el ojo no nota easing en alpha, mejor simple.
                    m_panelOffX = 0; m_panelOffY = 0; m_panelScale = 1.0f;
                    m_panelAlpha = (float)t;
                    break;

                case LevelEndAnimation::FromTop:
                {
                    float e = easeOutBack((float)t);
                    m_panelOffX = 0;
                    m_panelOffY = (e - 1.0f) * (float)viewport::height();
                    m_panelScale = 1.0f; m_panelAlpha = 1.0f;
                    break;
                }

                case LevelEndAnimation::FromBottom:
                {
                    float e = easeOutBack((float)t);
                    m_panelOffX = 0;
                    m_panelOffY = (1.0f - e) * (float)viewport::height();
                    m_panelScale = 1.0f; m_panelAlpha = 1.0f;
                    break;
                }

                case LevelEndAnimation::Zoom:
                {
                    float e = easeOutBack((float)t);
                    m_panelScale = e;
                    m_panelOffX = 0; m_panelOffY = 0; m_panelAlpha = 1.0f;
                    break;
                }
            }
        }

        // Easing out-back: termina con un pequeño overshoot, settle en 1.0.
        // c1 controla la cantidad de overshoot (~10% más que el target).
        static float easeOutBack(float t)
        {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            float tm1 = t - 1.0f;
            return 1.0f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
        }

        bool isPanelEntering() const
        {
            if (m_preset.animation == LevelEndAnimation::None) return false;
            return m_time < (double)m_preset.entryDuration;
        }

        // Si el panel está animándose con scale, el contenido (texto bitmap
        // que no escala bien) se oculta hasta que la entrada termina.
        bool shouldDrawContent() const
        {
            if (m_preset.animation == LevelEndAnimation::Zoom && isPanelEntering())
                return false;
            return true;
        }

        // Tiempo "interno" del contenido. Para FromTop/FromBottom/Zoom, el
        // contenido espera a que el panel termine de entrar antes de
        // empezar sus propias cascadas (estrellas, filas tally).
        // Para None y Fade, el contenido empieza desde t=0.
        double contentTime() const
        {
            if (m_preset.animation == LevelEndAnimation::None ||
                m_preset.animation == LevelEndAnimation::Fade)
                return m_time;
            return std::max(0.0, m_time - (double)m_preset.entryDuration);
        }

        // ══════════════════════════════════════════════════════════════════
        // Render: estilo Classic (base reutilizada por todos)
        // ══════════════════════════════════════════════════════════════════
        //
        // extraTopSpace: espacio reservado entre separador y botones (para
        // que Stars/Tally pinten sus decoraciones sobre esa área).
        // panelShakeX/Y: offset adicional del panel entero (efecto KO).
        //   El offset de la animación de entrada (m_panelOffX/Y) se suma
        //   a estos para que ambos efectos coexistan limpiamente.

        void renderClassic(float VW, float VH,
                           float extraTopSpace,
                           float panelShakeX,
                           float panelShakeY)
        {
            const auto& c = m_preset.colors;
            const float pa = m_panelAlpha;  // alias corto para alpha multiplier

            float itemH    = 54.0f;
            float padding  = 28.0f;
            float titleH   = 90.0f;
            float sepH     = 20.0f;
            int   nButtons = buttonCount();

            // Tamaño base del panel
            float panW0 = VW * 0.34f;
            float panH0 = titleH + sepH + extraTopSpace
                        + nButtons * itemH + padding * 2.0f;

            // Tamaño efectivo (escalado por animación Zoom)
            float panW = panW0 * m_panelScale;
            float panH = panH0 * m_panelScale;

            // Posición base — el escalado mantiene el panel centrado.
            float panX = (VW - panW) / 2.0f + panelShakeX + m_panelOffX;
            float panY = (VH - panH) / 2.0f + panelShakeY + m_panelOffY;

            if (m_preset.position == LevelEndPosition::Top)
                panY = VH * 0.10f + panelShakeY + m_panelOffY;
            else if (m_preset.position == LevelEndPosition::Bottom)
                panY = VH - panH - VH * 0.10f + panelShakeY + m_panelOffY;

            // Panel (rect + border) — siempre se dibuja, incluso durante
            // Zoom para que el rect se vea creciendo.
            draw2d::drawRect(panX, panY, panW, panH,
                             c.bgR, c.bgG, c.bgB, 0.96f * pa);
            draw2d::drawRectBorder(panX, panY, panW, panH,
                                   c.accentR, c.accentG, c.accentB,
                                   c.accentA * pa, 2.5f);

            // Si estamos en Zoom durante la entrada, el contenido (texto,
            // botones, etc.) no se dibuja — sólo el rect creciendo. Esto
            // evita que el texto bitmap aparezca "saliéndose" del panel
            // mientras éste todavía es pequeño.
            if (!shouldDrawContent())
            {
                // Apagar el hit-test de botones durante la entrada (botones
                // no visibles → no clickeables).
                m_btnStartY = -1e9f; m_btnX = 0; m_btnW = 0; m_btnItemH = 0;
                m_extraAreaX = m_extraAreaY = m_extraAreaW = m_extraAreaH = 0;
                return;
            }

            // ── Contenido del panel (escala = 1 a partir de aquí) ──
            // Cuando llegamos aquí en Zoom, m_panelScale ya == 1, así que
            // panW y panH son los tamaños "finales".

            // Título con pulso (rojo dramático en derrota, suave en victoria)
            std::string title = m_preset.text.empty()
                              ? (m_isVictory ? eclips::langUI("Victoria", "Victory")
                                             : eclips::langUI("Derrota", "Defeat"))
                              : m_preset.text;
            if (m_hasTitle)
            {
                float tw = m_titleFont.measureWidth(title);
                float pulseSpeed = m_isVictory ? 1.4f : 2.2f;
                float pulseAmp   = m_isVictory ? 0.15f : 0.30f;
                float pulse = (1.0f - pulseAmp)
                            + pulseAmp * (float)std::sin(m_time * pulseSpeed);
                m_titleFont.drawText(title,
                    panX + (panW - tw) / 2.0f, panY + padding,
                    c.accentR * pulse, c.accentG * pulse, c.accentB * pulse,
                    c.accentA * pa);
            }

            // Separador
            float sepY = panY + padding + titleH - 6.0f;
            draw2d::drawRect(panX + 16.0f, sepY, panW - 32.0f, 1.5f,
                             c.accentR, c.accentG, c.accentB, 0.5f * pa);

            // ── Área extra (para Stars/Tally) ──
            float extraY = sepY + sepH;
            m_extraAreaY = extraY;
            m_extraAreaX = panX + 16.0f;
            m_extraAreaW = panW - 32.0f;
            m_extraAreaH = extraTopSpace;

            // Botones después del área extra
            float startY = extraY + extraTopSpace;
            m_btnStartY = startY;
            m_btnX      = panX + 16.0f;
            m_btnW      = panW - 32.0f;
            m_btnItemH  = itemH;

            renderButtons();

            // Hint
            if (m_hasSmall)
            {
                std::string hint = eclips::langUI(
                    "Flechas + Enter = Navegar  |  Click = Seleccionar",
                    "Arrows + Enter = Navigate  |  Click = Select");
                float hw = m_small.measureWidth(hint);
                m_small.drawText(hint,
                    (VW - hw) / 2.0f, panY + panH + 14.0f,
                    c.textR * 0.5f, c.textG * 0.5f, c.textB * 0.5f, 0.6f * pa);
            }
        }

        void renderButtons()
        {
            const auto& c = m_preset.colors;
            const float pa = m_panelAlpha;
            int nButtons = buttonCount();

            for (int i = 0; i < nButtons; ++i)
            {
                float iy   = m_btnStartY + i * m_btnItemH;
                bool  sel  = (i == m_cursor);
                float btnH = m_btnItemH - 6.0f;

                if (sel)
                {
                    draw2d::drawRect(m_btnX, iy, m_btnW, btnH,
                                     c.btnHovR, c.btnHovG, c.btnHovB,
                                     c.btnHovA * pa);
                    draw2d::drawRectBorder(m_btnX, iy, m_btnW, btnH,
                                           c.accentR, c.accentG, c.accentB,
                                           1.0f * pa, 2.0f);
                }
                else
                {
                    draw2d::drawRect(m_btnX, iy, m_btnW, btnH,
                                     c.btnR, c.btnG, c.btnB, c.btnA * pa);
                }

                const std::string label = buttonLabel(i);
                float tw = m_font.measureWidth(label);
                float tx = m_btnX + (m_btnW - tw) / 2.0f;
                float ty = iy + (btnH - m_font.getLineHeight()) / 2.0f;

                if (sel)
                    m_font.drawText(label, tx, ty,
                                    c.textR, c.textG, c.textB, c.textA * pa);
                else
                    m_font.drawText(label, tx, ty,
                                    c.textR * 0.7f, c.textG * 0.7f,
                                    c.textB * 0.7f, c.textA * 0.8f * pa);
            }

            // Indicador ">" parpadeante
            {
                float iy   = m_btnStartY + m_cursor * m_btnItemH;
                float btnH = m_btnItemH - 6.0f;
                float tw   = m_font.measureWidth(buttonLabel(m_cursor));
                float ix   = m_btnX + (m_btnW - tw) / 2.0f - 24.0f;
                float iy2  = iy + (btnH - m_font.getLineHeight()) / 2.0f;
                float blink = 0.6f + 0.4f * (float)std::sin(m_time * 4.0);
                m_font.drawText(">", ix, iy2,
                                c.accentR, c.accentG, c.accentB, blink * pa);
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Render: estilo KO (pantalla dramática)
        // ══════════════════════════════════════════════════════════════════
        //
        // El shake interno del KO se SUMA al offset de la animación de
        // entrada vía panelShakeX/Y → se combinan limpio. "FromTop + KO"
        // = panel cae con shake; "Zoom + KO" = panel zoomea y al terminar
        // shakea; "Fade + KO" = panel fade-in con flash y shake superpuestos.

        void renderKO(float VW, float VH)
        {
            const float pa = m_panelAlpha;

            // Shake del panel los primeros 0.5s
            float shakeX = 0, shakeY = 0;
            if (m_time < 0.5)
            {
                float intensity = (float)((0.5 - m_time) / 0.5);
                shakeX = (float)std::sin(m_time * 60.0) * intensity * 14.0f;
                shakeY = (float)std::cos(m_time * 55.0) * intensity * 9.0f;
            }

            renderClassic(VW, VH, 0, shakeX, shakeY);

            // Tint flash blanco los primeros 0.2s — heredamos m_panelAlpha
            // para que en Fade el flash también aparezca progresivo.
            if (m_time < 0.2)
            {
                float flashA = (float)((0.2 - m_time) / 0.2);
                draw2d::drawRect(0, 0, VW, VH, 1, 1, 1, flashA * 0.6f * pa);
            }

            // Tinte rojo dramático sobre todo (sutil, persistente).
            draw2d::drawRect(0, 0, VW, VH, 0.6f, 0.0f, 0.0f, 0.15f * pa);
        }

        // ══════════════════════════════════════════════════════════════════
        // Render: estilo Stars (medallas con cascada)
        // ══════════════════════════════════════════════════════════════════
        //
        // La cascada usa contentTime() en vez de m_time, así las medallas
        // esperan a que el panel termine de entrar antes de empezar a
        // aparecer. En Fade, contentTime()==m_time, así que medallas y
        // panel hacen fade juntos.

        void renderStars(float VW, float VH)
        {
            const float starAreaH = 110.0f;

            renderClassic(VW, VH, starAreaH, 0, 0);

            // Si el contenido no se dibuja todavía (Zoom durante entrada),
            // tampoco las medallas.
            if (!shouldDrawContent()) return;

            const auto& c = m_preset.colors;
            const float pa = m_panelAlpha;
            const double ct = contentTime();

            const int totalStars = 3;
            float spacing = m_extraAreaW / (float)(totalStars + 1);
            float radius  = 38.0f;
            float cy      = m_extraAreaY + starAreaH * 0.5f;

            for (int i = 0; i < totalStars; ++i)
            {
                float cx = m_extraAreaX + spacing * (i + 1);

                // Cascada: estrella i aparece en t = 0.3 + i*0.35
                double startT = 0.3 + i * 0.35;
                float  scale  = 0.0f;

                if (ct >= startT)
                {
                    double elapsed = ct - startT;
                    if (elapsed < 0.25)
                    {
                        float t = (float)(elapsed / 0.25);
                        scale = t * 1.35f;
                    }
                    else if (elapsed < 0.40)
                    {
                        float t = (float)((elapsed - 0.25) / 0.15);
                        scale = 1.35f - t * 0.35f;
                    }
                    else
                    {
                        scale = 1.0f;
                    }
                }

                if (scale <= 0.01f) continue;

                float rad = radius * scale;
                bool earned = (i < m_stars);

                if (earned)
                {
                    // Halo
                    draw2d::drawCircle(cx, cy, rad + 6.0f,
                                       c.accentR, c.accentG, c.accentB, 0.3f * pa);
                    // Medalla dorada
                    draw2d::drawCircle(cx, cy, rad,
                                       c.accentR, c.accentG, c.accentB,
                                       c.accentA * pa);
                    // Anillo interior oscuro
                    draw2d::drawCircleBorder(cx, cy, rad * 0.75f,
                                             c.accentR * 0.4f, c.accentG * 0.4f,
                                             c.accentB * 0.4f, 0.9f * pa, 32, 2.0f);
                    // Brillo central
                    draw2d::drawCircle(cx - rad * 0.25f, cy - rad * 0.3f,
                                       rad * 0.22f, 1, 1, 1, 0.55f * pa);
                }
                else
                {
                    // Medalla vacía
                    draw2d::drawCircle(cx, cy, rad,
                                       0.12f, 0.12f, 0.14f, 0.7f * pa);
                    draw2d::drawCircleBorder(cx, cy, rad,
                                             0.35f, 0.35f, 0.38f, 0.6f * pa, 32, 2.5f);
                }
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Render: estilo Tally (lista con contadores animados)
        // ══════════════════════════════════════════════════════════════════
        //
        // Usa contentTime() — las filas esperan a que el panel termine de
        // entrar (excepto en Fade, donde aparecen junto con el panel).

        void renderTally(float VW, float VH)
        {
            int nRows = (int)m_tallyLabels.size();
            float rowH = 36.0f;
            float tallyAreaH = 12.0f + nRows * rowH + 12.0f;
            if (nRows == 0) tallyAreaH = 24.0f;

            renderClassic(VW, VH, tallyAreaH, 0, 0);

            if (!shouldDrawContent()) return;
            if (nRows == 0) return;

            const auto& c = m_preset.colors;
            const float pa = m_panelAlpha;
            const double ct = contentTime();
            float startY = m_extraAreaY + 12.0f;

            for (int i = 0; i < nRows; ++i)
            {
                double rowStart = 0.3 + i * 0.20;
                if (ct < rowStart) continue;

                double elapsed = ct - rowStart;

                std::string valueStr;
                if (m_tallyIsNumeric[i])
                {
                    float target = m_tallyTargets[i];
                    float animated;
                    if (elapsed < 0.6)
                    {
                        // Ease out cubic
                        float t = (float)(elapsed / 0.6);
                        t = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                        animated = target * t;
                    }
                    else
                    {
                        animated = target;
                    }

                    if (std::abs(target - std::round(target)) < 0.001f)
                        valueStr = std::to_string((int)std::round(animated));
                    else
                    {
                        char buf[32];
                        std::snprintf(buf, sizeof(buf), "%.1f", animated);
                        valueStr = buf;
                    }
                }
                else
                {
                    valueStr = m_tallyRawStrings[i];
                }

                // Fade-in de la fila en los primeros 0.3s
                float rowAlpha = 1.0f;
                if (elapsed < 0.3)
                    rowAlpha = (float)(elapsed / 0.3);

                float y = startY + i * rowH;

                m_font.drawText(m_tallyLabels[i],
                    m_extraAreaX + 4.0f, y,
                    c.textR, c.textG, c.textB, rowAlpha * pa);

                float vw = m_font.measureWidth(valueStr);
                m_font.drawText(valueStr,
                    m_extraAreaX + m_extraAreaW - 4.0f - vw, y,
                    c.accentR, c.accentG, c.accentB, rowAlpha * pa);
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Evaluación de criterios (Stars)
        // ══════════════════════════════════════════════════════════════════

        int computeStars() const
        {
            // El creador ordena criterios de más alto a más bajo en .eclmap.
            // El primero que matchea gana.
            for (const auto& c : m_preset.starCriteria)
                if (evaluateCriterion(c))
                    return c.stars;
            return 0;
        }

        bool evaluateCriterion(const LevelEndStarCriterion& c) const
        {
            if (c.always) return true;
            if (!m_varLookup) return false;

            std::string lhsStr = m_varLookup(c.var);
            float left = 0.0f, right = 0.0f;
            try { left  = std::stof(lhsStr); } catch (...) {}
            try { right = std::stof(c.val); } catch (...) {}

            if (c.op == ">=") return left >= right;
            if (c.op == "<=") return left <= right;
            if (c.op == ">")  return left >  right;
            if (c.op == "<")  return left <  right;
            if (c.op == "==") return left == right;
            if (c.op == "!=") return left != right;
            return false;
        }

        // ══════════════════════════════════════════════════════════════════
        // Preprocesado de Tally
        // ══════════════════════════════════════════════════════════════════

        void preprocessTally()
        {
            m_tallyLabels.clear();
            m_tallyTargets.clear();
            m_tallyIsNumeric.clear();
            m_tallyRawStrings.clear();

            // Si Lua llenó tallyOverride, esa fuente gana. Sus pares
            // <label, valor> ya son texto literal resuelto (no varNames),
            // por eso NO se hace varLookup aquí — se usan tal cual.
            // Si el valor literal parsea como número, se anima de 0→target;
            // si es string como "1:23", se muestra fijo.
            const auto& source = !m_preset.tallyOverride.empty()
                ? m_preset.tallyOverride
                : m_preset.tallyFields;

            const bool fromLua = !m_preset.tallyOverride.empty();

            for (const auto& field : source)
            {
                const std::string& label = field.first;
                const std::string& second = field.second;

                // Para tallyFields normal: second es un varName → lookup.
                // Para tallyOverride: second ya es el valor literal final.
                std::string raw = fromLua
                    ? second
                    : (m_varLookup ? m_varLookup(second) : "");

                m_tallyLabels.push_back(label);
                m_tallyRawStrings.push_back(raw);

                bool isNum = false;
                float num = 0.0f;
                try { num = std::stof(raw); isNum = true; }
                catch (...) {}

                m_tallyTargets.push_back(num);
                m_tallyIsNumeric.push_back(isNum);
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Botones
        // ══════════════════════════════════════════════════════════════════

        int buttonCount() const { return m_hasNextButton ? 3 : 2; }

        // Devuelve por VALOR desde CP3: `langUI` construye la cadena traducida,
        // así que un `const char*` apuntaría a un temporal muerto.
        std::string buttonLabel(int index) const
        {
            if (m_hasNextButton)
            {
                if (index == 0) return eclips::langUI("Siguiente", "Next");
                if (index == 1) return eclips::langUI("Reintentar", "Retry");
                return eclips::langUI("Menu Principal", "Main Menu");
            }
            if (index == 0) return eclips::langUI("Reintentar", "Retry");
            return eclips::langUI("Menu Principal", "Main Menu");
        }

        void executeAction(int index)
        {
            if (m_hasNextButton)
            {
                if (index == 0) { if (m_onNext)     { m_onNext();     m_stack->requestPop(); } return; }
                if (index == 1) { if (m_onRetry)    { m_onRetry();    m_stack->requestPop(); } return; }
                if (index == 2) { if (m_onMainMenu) { m_onMainMenu(); m_stack->requestPop(); } return; }
            }
            else
            {
                if (index == 0) { if (m_onRetry)    { m_onRetry();    m_stack->requestPop(); } return; }
                if (index == 1) { if (m_onMainMenu) { m_onMainMenu(); m_stack->requestPop(); } return; }
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Hit testing + utilidades
        // ══════════════════════════════════════════════════════════════════

        void mouseToVirtual(float mx, float my, float& vx, float& vy) const
        {
            float targetAspect = (float)viewport::width() / viewport::height();
            float winAspect    = (float)m_winW / m_winH;
            int vpX = 0, vpY = 0, vpW = m_winW, vpH = m_winH;
            if (winAspect > targetAspect)
            { vpW = (int)(m_winH * targetAspect); vpX = (m_winW - vpW) / 2; }
            else
            { vpH = (int)(m_winW / targetAspect); vpY = (m_winH - vpH) / 2; }
            vx = (mx - vpX) * (float)viewport::width()  / vpW;
            vy = (my - vpY) * (float)viewport::height() / vpH;
        }

        int hitTestButton(float vx, float vy) const
        {
            int n = buttonCount();
            for (int i = 0; i < n; ++i)
            {
                float iy  = m_btnStartY + i * m_btnItemH;
                float btnH = m_btnItemH - 6.0f;
                if (vx >= m_btnX && vx <= m_btnX + m_btnW &&
                    vy >= iy     && vy <= iy + btnH)
                    return i;
            }
            return -1;
        }

        static const char* styleName(LevelEndStyle s)
        {
            switch (s)
            {
                case LevelEndStyle::Classic: return "classic";
                case LevelEndStyle::Stars:   return "stars";
                case LevelEndStyle::Tally:   return "tally";
                case LevelEndStyle::KO:      return "ko";
            }
            return "?";
        }

        static const char* animName(LevelEndAnimation a)
        {
            switch (a)
            {
                case LevelEndAnimation::None:       return "none";
                case LevelEndAnimation::Fade:       return "fade";
                case LevelEndAnimation::FromTop:    return "from_top";
                case LevelEndAnimation::FromBottom: return "from_bottom";
                case LevelEndAnimation::Zoom:       return "zoom";
            }
            return "?";
        }

        // ══════════════════════════════════════════════════════════════════
        // Datos
        // ══════════════════════════════════════════════════════════════════

        SDL_Window*    m_window  = nullptr;
        StateStack*    m_stack   = nullptr;
        FontSourceStore* m_fontSources = nullptr;

        LevelEndPreset m_preset;
        bool           m_isVictory     = false;
        bool           m_hasNextButton = false;

        std::function<void()> m_onRetry;
        std::function<void()> m_onMainMenu;
        std::function<void()> m_onNext;
        VarLookup             m_varLookup;

        int  m_cursor = 0;
        int  m_stars  = 0;

        std::vector<std::string> m_tallyLabels;
        std::vector<float>       m_tallyTargets;
        std::vector<bool>        m_tallyIsNumeric;
        std::vector<std::string> m_tallyRawStrings;

        TextRenderer m_font;
        TextRenderer m_titleFont;
        TextRenderer m_small;
        bool m_hasFont  = false;
        bool m_hasTitle = false;
        bool m_hasSmall = false;
        f64  m_time     = 0.0;
        int  m_winW     = 1280;
        int  m_winH     = 720;

        // Transform de la animación de entrada (calculados en update)
        float m_panelOffX  = 0.0f;
        float m_panelOffY  = 0.0f;
        float m_panelScale = 1.0f;
        float m_panelAlpha = 1.0f;

        // Layout cacheado
        mutable float m_btnStartY  = 0;
        mutable float m_btnX       = 0;
        mutable float m_btnW       = 0;
        mutable float m_btnItemH   = 0;
        mutable float m_extraAreaX = 0;
        mutable float m_extraAreaY = 0;
        mutable float m_extraAreaW = 0;
        mutable float m_extraAreaH = 0;
    };

} // namespace eclips
