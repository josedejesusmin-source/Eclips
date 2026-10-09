// ============================================================================
// DialogOverlayState.h — Dialogo con sistema de Choices (Fase 2)
// ============================================================================
//
// NUEVO: Sistema de elecciones (choices) que ramifican la historia.
//
// ¿Como funciona?
//   1. El NPC dice sus lineas normales (typewriter)
//   2. Aparecen 2-3 opciones en pantalla
//   3. El jugador elige con 1, 2, o 3
//   4. Se reproducen las lineas de la opcion elegida
//   5. El dialogo termina
//
// Esto es exactamente como funcionan Doki Doki, Clannad, Corpse Party:
//   El jugador toma decisiones que afectan lo que pasa despues.
//
// Estructura:
//   DialogScript {
//       speaker: "Luna"
//       lines: ["Hola!", "Tengo una pregunta..."]   ← Se dicen primero
//       choices: [                                    ← Luego aparecen
//           { label: "Opcion A", result: ["Elegiste A!", "..."] },
//           { label: "Opcion B", result: ["Elegiste B!", "..."] }
//       ]
//   }
//
// Si no hay choices, funciona igual que antes (dialogo lineal).
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Types.h"
#include "engine/core/Settings.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/core/TextBudget.h"   // §7.410: el typewriter cuenta CARACTERES

#include <SDL.h>
#include <iostream>
#include <string>
#include <vector>
#include <functional>

namespace eclips
{
    // ── Opcion de dialogo ──
    struct DialogChoice
    {
        std::string label;                  // Lo que ve el jugador
        std::vector<std::string> result;    // Lo que pasa al elegir
    };

    // ── Script completo de un dialogo ──
    struct DialogScript
    {
        std::string speaker;
        std::vector<std::string> lines;     // Lineas antes de la eleccion
        std::vector<DialogChoice> choices;  // Opciones (vacio = sin eleccion)

        // Callback opcional cuando se elige (para afectar el mundo)
        // El parametro es el indice de la opcion elegida (0, 1, 2...)
        std::function<void(int)> onChoice;
    };

    // ── Fuentes prestadas por el tramo (LAG-2D, E-48) ──
    // Hornear los tres atlas en cada onEnter era el tirón de cada charla del
    // 2D: 18–50 ms (DISENO_PLAN_DE_PUBLICACION E-47). El Gameplay2DState los
    // hornea una vez por tramo y se los presta al diálogo, que ni los hornea
    // ni los suelta. Sin préstamo (los dos constructores de siempre), el
    // diálogo hornea los suyos como antes.
    struct DialogFonts
    {
        TextRenderer* text   = nullptr;   // 36 px
        TextRenderer* name   = nullptr;   // 28 px
        TextRenderer* choice = nullptr;   // 32 px
        bool complete() const { return text && name && choice; }
    };

    class DialogOverlayState : public GameState
    {
    public:
        // Constructor simple (sin choices, compatible con version anterior)
        DialogOverlayState(SDL_Window* window, StateStack* stateStack,
                           FontSourceStore* fontSources,
                           const std::string& npcName,
                           const std::vector<std::string>& lines)
            : m_window(window), m_stateStack(stateStack),
              m_fontSources(fontSources)
        {
            m_textRenderer.setSourceStore(m_fontSources);
            m_nameRenderer.setSourceStore(m_fontSources);
            m_choiceRenderer.setSourceStore(m_fontSources);
            m_script.speaker = npcName;
            m_script.lines = lines;
            // Sin choices — dialogo lineal
        }

        // Constructor completo (con choices)
        DialogOverlayState(SDL_Window* window, StateStack* stateStack,
                           FontSourceStore* fontSources,
                           const DialogScript& script)
            : m_window(window), m_stateStack(stateStack),
              m_fontSources(fontSources), m_script(script)
        {
            m_textRenderer.setSourceStore(m_fontSources);
            m_nameRenderer.setSourceStore(m_fontSources);
            m_choiceRenderer.setSourceStore(m_fontSources);
        }

        bool isTransparent() const override { return true; }
        bool allowsUpdateBelow() const override { return false; }

        // Antes del push. Los tres o ninguno: con alguno nulo hornea los suyos.
        void lendFonts(const DialogFonts& fonts) { m_lent = fonts; }

        void onEnter() override
        {
            std::cout << "[Eclips] DialogOverlay: " << m_script.speaker << std::endl;
            m_phase = Phase::Lines;
            m_lineIndex = 0;
            m_typewriterChars = 0;
            m_typewriterTimer = 0;
            m_choiceIndex = -1;
            m_resultLineIndex = 0;

            if (m_lent.complete())
            {
                m_hasFont = m_lent.text->isLoaded();
                m_hasNameFont = m_lent.name->isLoaded();
                m_hasChoiceFont = m_lent.choice->isLoaded();
            }
            else
            {
                m_hasFont = m_textRenderer.init(Settings::defaultFontPath(), 36.0f);
                m_hasNameFont = m_nameRenderer.init(Settings::defaultFontPath(), 28.0f);
                m_hasChoiceFont = m_choiceRenderer.init(Settings::defaultFontPath(), 32.0f);
            }

            SDL_GetWindowSize(m_window, &m_windowW, &m_windowH);

            // Si no hay lineas previas, ir directo a choices
            if (m_script.lines.empty() && !m_script.choices.empty())
            {
                m_phase = Phase::Choosing;
            }
            else if (!m_script.lines.empty())
            {
                std::cout << "  [" << m_script.speaker << "] " << m_script.lines[0] << std::endl;
            }
        }

        void onExit() override
        {
            // Las prestadas son del tramo: las suelta él, en su onExit. Y los
            // renderers propios sin hornear no se tocan: su shutdown barre el
            // FontSourceStore igual (TextRenderer.cpp:1044).
            if (!m_lent.complete())
            {
                m_textRenderer.shutdown();
                m_nameRenderer.shutdown();
                m_choiceRenderer.shutdown();
            }
            std::cout << "[Eclips] DialogOverlay: Fin." << std::endl;
        }

        bool handleEvent(const void* event) override
        {
            const SDL_Event* e = static_cast<const SDL_Event*>(event);

            // ══════════════════════════════════════════════════════════
            // FASE: ELEGIR (choices en pantalla)
            // ══════════════════════════════════════════════════════════
            if (m_phase == Phase::Choosing && e->type == SDL_KEYDOWN)
            {
                int numChoices = static_cast<int>(m_script.choices.size());

                int picked = -1;
                if (e->key.keysym.sym == SDLK_1 && numChoices >= 1) picked = 0;
                if (e->key.keysym.sym == SDLK_2 && numChoices >= 2) picked = 1;
                if (e->key.keysym.sym == SDLK_3 && numChoices >= 3) picked = 2;

                if (picked >= 0)
                {
                    m_choiceIndex = picked;
                    std::cout << "  >>> Elegido: " << m_script.choices[picked].label << std::endl;

                    // Ejecutar callback si existe
                    if (m_script.onChoice)
                        m_script.onChoice(picked);

                    // ¿Hay lineas de resultado?
                    if (!m_script.choices[picked].result.empty())
                    {
                        m_phase = Phase::Result;
                        m_resultLineIndex = 0;
                        m_typewriterChars = 0;
                        m_typewriterTimer = 0;

                        std::cout << "  [" << m_script.speaker << "] "
                                  << m_script.choices[picked].result[0] << std::endl;
                    }
                    else
                    {
                        // Sin resultado → cerrar
                        m_wantsPop = true;
                    }
                    return true;
                }

                // ESC durante choices → cancelar
                if (e->key.keysym.sym == SDLK_ESCAPE)
                {
                    m_wantsPop = true;
                    return true;
                }

                return true; // Consumir todo
            }

            // ══════════════════════════════════════════════════════════
            // FASE: LINEAS NORMALES o RESULTADO
            // ══════════════════════════════════════════════════════════
            bool advance = false;
            if (e->type == SDL_KEYDOWN)
            {
                if (e->key.keysym.sym == SDLK_SPACE || e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_e)
                    advance = true;
                if (e->key.keysym.sym == SDLK_ESCAPE)
                { m_wantsPop = true; return true; }
            }
            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
                advance = true;

            if (advance)
            {
                if (m_phase == Phase::Lines)
                {
                    // Completar typewriter o avanzar
                    int total = (m_lineIndex < (int)m_script.lines.size())
                        ? (int)eclips::text::countCodepoints(m_script.lines[m_lineIndex]) : 0;

                    if (m_typewriterChars < total)
                    {
                        m_typewriterChars = total;
                        return true;
                    }

                    m_lineIndex++;
                    m_typewriterChars = 0;
                    m_typewriterTimer = 0;

                    if (m_lineIndex >= (int)m_script.lines.size())
                    {
                        // ¿Hay choices?
                        if (!m_script.choices.empty())
                        {
                            m_phase = Phase::Choosing;
                            std::cout << "  [Opciones disponibles]" << std::endl;
                            for (int i = 0; i < (int)m_script.choices.size(); i++)
                                std::cout << "    [" << (i+1) << "] " << m_script.choices[i].label << std::endl;
                        }
                        else
                        {
                            m_wantsPop = true;
                        }
                    }
                    else
                    {
                        std::cout << "  [" << m_script.speaker << "] " << m_script.lines[m_lineIndex] << std::endl;
                    }
                    return true;
                }
                else if (m_phase == Phase::Result)
                {
                    const auto& resultLines = m_script.choices[m_choiceIndex].result;

                    int total = (m_resultLineIndex < (int)resultLines.size())
                        ? (int)eclips::text::countCodepoints(resultLines[m_resultLineIndex]) : 0;

                    if (m_typewriterChars < total)
                    {
                        m_typewriterChars = total;
                        return true;
                    }

                    m_resultLineIndex++;
                    m_typewriterChars = 0;
                    m_typewriterTimer = 0;

                    if (m_resultLineIndex >= (int)resultLines.size())
                    {
                        m_wantsPop = true;
                    }
                    else
                    {
                        std::cout << "  [" << m_script.speaker << "] "
                                  << resultLines[m_resultLineIndex] << std::endl;
                    }
                    return true;
                }
            }

            return true; // Consumir todo
        }

        void update(f64 dt) override
        {
            m_totalTime += dt;
            SDL_GetWindowSize(m_window, &m_windowW, &m_windowH);

            // Typewriter
            if (m_phase == Phase::Lines && m_lineIndex < (int)m_script.lines.size())
            {
                int total = (int)eclips::text::countCodepoints(m_script.lines[m_lineIndex]);
                advanceTypewriter(dt, total);
            }
            else if (m_phase == Phase::Result && m_choiceIndex >= 0)
            {
                const auto& resultLines = m_script.choices[m_choiceIndex].result;
                if (m_resultLineIndex < (int)resultLines.size())
                {
                    int total = (int)eclips::text::countCodepoints(resultLines[m_resultLineIndex]);
                    advanceTypewriter(dt, total);
                }
            }

            // Pop
            if (m_wantsPop && m_stateStack)
            {
                m_stateStack->requestPop();
                return;
            }
        }

        void render(f64 alpha) override
        {
            const f32 VW = (f32)viewport::width();
            const f32 VH = (f32)viewport::height();

            draw2d::setupViewport(m_windowW, m_windowH, viewport::width(), viewport::height());
            draw2d::setupProjection(VW, VH);

            // ── 1. Oscurecer fondo ──
            draw2d::drawRect(0, 0, VW, VH, 0, 0, 0, 0.4f);

            // ── 2. Caja de dialogo ──
            f32 boxW = VW * 0.80f;
            f32 boxH = 180.0f;
            f32 boxX = (VW - boxW) / 2.0f;
            f32 boxY = VH - boxH - 50.0f;

            draw2d::drawRect(boxX, boxY, boxW, boxH, 0.05f, 0.05f, 0.12f, 0.88f);
            draw2d::drawRectBorder(boxX, boxY, boxW, boxH, 0.85f, 0.7f, 0.2f, 0.9f, 3.0f);

            // ── 3. Placa del nombre ──
            f32 nameW = 220, nameH = 38;
            f32 nameX = boxX + 20, nameY = boxY - nameH + 5;
            draw2d::drawRect(nameX, nameY, nameW, nameH, 0.6f, 0.45f, 0.1f, 0.9f);
            draw2d::drawRectBorder(nameX, nameY, nameW, nameH, 0.85f, 0.7f, 0.2f, 0.9f, 2);

            if (m_hasNameFont)
            {
                f32 ntw = nameFont().measureWidth(m_script.speaker);
                nameFont().drawText(m_script.speaker, nameX+(nameW-ntw)/2, nameY+3, 1,1,1,1);
            }

            // ── 4. Contenido segun fase ──
            if (m_phase == Phase::Lines)
            {
                // Texto normal con typewriter
                if (m_hasFont && m_lineIndex < (int)m_script.lines.size())
                {
                    textFont().drawTextWrapped(m_script.lines[m_lineIndex],
                        boxX+30, boxY+20, boxW-60, m_typewriterChars, 1,1,1,1);
                }

                // Indicador de continuar
                if (m_lineIndex < (int)m_script.lines.size())
                {
                    int total = (int)eclips::text::countCodepoints(m_script.lines[m_lineIndex]);
                    if (m_typewriterChars >= total)
                    {
                        f32 blink = (f32)std::sin(m_totalTime*3)*0.3f+0.7f;
                        draw2d::drawRect(boxX+boxW-40, boxY+boxH-35, 15, 15, 0.85f,0.7f,0.2f, blink);
                    }
                }
            }
            else if (m_phase == Phase::Choosing)
            {
                // ══════════════════════════════════════════════════════
                // CHOICES — Opciones en pantalla
                // ══════════════════════════════════════════════════════
                int numChoices = (int)m_script.choices.size();

                // Texto: "Elige una opcion:"
                if (m_hasFont)
                {
                    textFont().drawText("Elige una opcion:",
                        boxX+30, boxY+15, 0.85f, 0.7f, 0.2f, 0.9f);
                }

                // Dibujar cada opcion
                f32 optStartY = boxY + 60;
                f32 optH = 45;
                f32 optGap = 8;
                f32 optMargin = 40;

                for (int i = 0; i < numChoices && i < 3; i++)
                {
                    f32 optY = optStartY + i * (optH + optGap);
                    f32 optW = boxW - optMargin * 2;
                    f32 optX = boxX + optMargin;

                    // Color del fondo segun indice
                    f32 cr = 0, cg = 0, cb = 0;
                    if (i == 0) { cr = 0.15f; cg = 0.3f; cb = 0.15f; }      // Verde
                    else if (i == 1) { cr = 0.15f; cg = 0.15f; cb = 0.35f; } // Azul
                    else { cr = 0.35f; cg = 0.15f; cb = 0.15f; }             // Rojo

                    f32 blink = (f32)std::sin(m_totalTime * 3 + i * 0.8f) * 0.15f + 0.85f;

                    // Fondo de la opcion
                    draw2d::drawRect(optX, optY, optW, optH, cr, cg, cb, 0.8f);

                    // Borde brillante
                    f32 br = cr * 3, bg = cg * 3, bb = cb * 3;
                    if (br > 1) br = 1; if (bg > 1) bg = 1; if (bb > 1) bb = 1;
                    draw2d::drawRectBorder(optX, optY, optW, optH, br, bg, bb, blink, 2);

                    // Texto de la opcion
                    if (m_hasChoiceFont)
                    {
                        std::string label = "[" + std::to_string(i+1) + "] " + m_script.choices[i].label;
                        choiceFont().drawText(label, optX+15, optY+8, 1,1,1, blink);
                    }
                }
            }
            else if (m_phase == Phase::Result)
            {
                // Lineas de resultado con typewriter
                if (m_hasFont && m_choiceIndex >= 0)
                {
                    const auto& resultLines = m_script.choices[m_choiceIndex].result;
                    if (m_resultLineIndex < (int)resultLines.size())
                    {
                        textFont().drawTextWrapped(resultLines[m_resultLineIndex],
                            boxX+30, boxY+20, boxW-60, m_typewriterChars, 1,1,1,1);
                    }

                    // Indicador de continuar
                    if (m_resultLineIndex < (int)resultLines.size())
                    {
                        int total = (int)eclips::text::countCodepoints(resultLines[m_resultLineIndex]);
                        if (m_typewriterChars >= total)
                        {
                            f32 blink = (f32)std::sin(m_totalTime*3)*0.3f+0.7f;
                            draw2d::drawRect(boxX+boxW-40, boxY+boxH-35, 15, 15, 0.85f,0.7f,0.2f, blink);
                        }
                    }
                }
            }
        }

    private:
        // Las prestadas por el tramo, o las propias (E-48).
        TextRenderer& textFont()   { return m_lent.complete() ? *m_lent.text   : m_textRenderer; }
        TextRenderer& nameFont()   { return m_lent.complete() ? *m_lent.name   : m_nameRenderer; }
        TextRenderer& choiceFont() { return m_lent.complete() ? *m_lent.choice : m_choiceRenderer; }

        void advanceTypewriter(f64 dt, int total)
        {
            if (m_typewriterChars < total)
            {
                m_typewriterTimer += dt;
                while (m_typewriterTimer >= m_typewriterSpeed && m_typewriterChars < total)
                {
                    m_typewriterChars++;
                    m_typewriterTimer -= m_typewriterSpeed;
                }
            }
        }

        enum class Phase { Lines, Choosing, Result };

        SDL_Window* m_window = nullptr;
        StateStack* m_stateStack = nullptr;
        FontSourceStore* m_fontSources = nullptr;
        int m_windowW = 0, m_windowH = 0;

        DialogScript m_script;
        Phase m_phase = Phase::Lines;

        int m_lineIndex = 0;
        int m_choiceIndex = -1;
        int m_resultLineIndex = 0;

        TextRenderer m_textRenderer;
        TextRenderer m_nameRenderer;
        TextRenderer m_choiceRenderer;
        bool m_hasFont = false, m_hasNameFont = false, m_hasChoiceFont = false;
        DialogFonts m_lent;   // prestadas por el tramo (E-48), o vacías

        int m_typewriterChars = 0;
        f64 m_typewriterTimer = 0;
        f64 m_typewriterSpeed = 1.0 / 30.0;

        bool m_wantsPop = false;
        f64 m_totalTime = 0;
    };

} // namespace eclips
