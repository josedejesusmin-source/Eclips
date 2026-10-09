// ============================================================================
// PauseMenuState.h — Menú de pausa universal para Eclips Engine
// ============================================================================
//
// Funciona como overlay transparente sobre cualquier estado (VN, 2D, etc.)
// Se pushea en el StateStack cuando el jugador presiona ESC.
//
// ── Diseño ─────────────────────────────────────────────────────────────────
//
// Los botones que aparecen dependen de qué callbacks pasa el estado padre:
//   - onSave/onLoad: solo si el padre tiene save system (VN siempre, 2D opcional)
//   - onMainMenu: para volver al menú principal
//   - Quit siempre aparece (usa SDL_QUIT directamente)
//   - Resume siempre aparece (pop del PauseMenu)
//
// ── Uso desde VNScriptState ────────────────────────────────────────────────
//
//   m_stateStack->push(std::make_unique<PauseMenuState>(
//       m_window, m_stateStack,
//       [this]{ openSaveMenu(VNSaveMenu::Mode::Save); }, // onSave
//       [this]{ openSaveMenu(VNSaveMenu::Mode::Load); }, // onLoad
//       [this]{ m_returnToMenu = true; }                  // onMainMenu
//   ));
//
// ── Uso desde Gameplay2DState ──────────────────────────────────────────────
//
//   m_stateStack->push(std::make_unique<PauseMenuState>(
//       m_window, m_stateStack,
//       [this]{ saveGame(); },         // onSave (si tiene save)
//       [this]{ loadGame(); },         // onLoad
//       [this]{ m_returnToMenu = true; } // onMainMenu
//   ));
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Types.h"
#include "engine/core/Settings.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "SettingsState.h"
#include "Theme.h"

#include <SDL.h>
#include <string>
#include <vector>
#include "engine/core/LangStore.h"   // idiomas CP3: langUI() del chrome

#include <functional>
#include <iostream>
#include <cmath>

namespace eclips
{

    class PauseMenuState : public GameState
    {
    public:

        // ── Constructor ────────────────────────────────────────────────────
        //
        // Pasa nullptr en cualquier callback para ocultar ese botón.
        // Resume y Quit siempre aparecen.
        //
        PauseMenuState(SDL_Window* window,
            StateStack* stack,
            FontSourceStore* fontSources,
            std::function<void()> onSave = nullptr,
            std::function<void()> onLoad = nullptr,
            std::function<void()> onMainMenu = nullptr,
            std::function<void()> onReturnToVN = nullptr,
            const Theme* theme = nullptr,
            std::function<void()> onSettings = nullptr)
            : m_window(window)
            , m_stack(stack)
            , m_fontSources(fontSources)
            , m_onSave(std::move(onSave))
            , m_onLoad(std::move(onLoad))
            , m_onMainMenu(std::move(onMainMenu))
            , m_onReturnToVN(std::move(onReturnToVN))
            , m_onSettings(std::move(onSettings))
            , m_theme(theme)
        {
            m_font.setSourceStore(m_fontSources);
            m_small.setSourceStore(m_fontSources);
        }

        // Overlay: el juego se ve debajo, oscurecido
        bool isTransparent()     const override { return true;  }
        bool allowsUpdateBelow() const override { return false; }

        // ── Ciclo de vida ──────────────────────────────────────────────────

        void onEnter() override
        {
            m_hasFont  = m_font.init(Settings::defaultFontPath(), 30.0f);
            m_hasSmall = m_small.init(Settings::defaultFontPath(), 22.0f);

            // Construir lista de items dinámicamente
            m_items.clear();

            m_items.push_back({ eclips::langUI("Reanudar", "Resume"),
                                ItemAction::Resume });

            if (m_onSave)
                m_items.push_back({ eclips::langUI("Guardar", "Save"),
                                    ItemAction::Save });

            if (m_onLoad)
                m_items.push_back({ eclips::langUI("Cargar", "Load"),
                                    ItemAction::Load });

            m_items.push_back({ eclips::langUI("Configuracion", "Settings"),
                                ItemAction::Settings });

            if (m_onReturnToVN)
                m_items.push_back({ eclips::langUI("Salir del nivel", "Exit Level"),
                                    ItemAction::ReturnToVN });

            if (m_onMainMenu)
                m_items.push_back({ eclips::langUI("Menu Principal", "Main Menu"),
                                    ItemAction::MainMenu });

            m_items.push_back({ eclips::langUI("Salir del Juego", "Quit Game"),
                                ItemAction::Quit });

            m_cursor = 0;
            m_confirmQuit = false;
            m_confirmCursor = 1; // Default "No" para seguridad

            SDL_GetWindowSize(m_window, &m_winW, &m_winH);

            std::cout << "[PauseMenu] Abierto" << std::endl;
        }

        void onExit() override
        {
            m_font.shutdown();
            m_small.shutdown();
            std::cout << "[PauseMenu] Cerrado" << std::endl;
        }

        // ── Input ──────────────────────────────────────────────────────────

        bool handleEvent(const void* ev) override
        {
            const SDL_Event* e = static_cast<const SDL_Event*>(ev);

            // ── Modo confirmación de Quit ──────────────────────────────────
            if (m_confirmQuit)
                return handleConfirmInput(e);

            // ── Click del mouse en items ──────────────────────────────────
            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
            {
                float vx, vy;
                mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);

                int hit = hitTestItem(vx, vy);
                if (hit >= 0)
                {
                    m_cursor = hit;
                    executeItem(m_items[hit].action);
                    return true;
                }

                // Click fuera del panel = Resume
                {
                    const float VW = (float)viewport::width();
                    const float VH = (float)viewport::height();
                    int count = (int)m_items.size();
                    float itemH = 52.0f, padding = 24.0f, titleH = 50.0f, sepH = 16.0f;
                    float panW = VW * 0.32f;
                    float panH = titleH + sepH + count * itemH + padding * 2.0f;
                    float panX = (VW - panW) / 2.0f;
                    float panY = (VH - panH) / 2.0f;

                    if (vx < panX || vx > panX + panW || vy < panY || vy > panY + panH)
                    {
                        m_stack->cmdPop(this);  // cerrar pausa (click fuera)
                        return true;
                    }
                }
            }

            // ── Hover del mouse ───────────────────────────────────────────
            if (e->type == SDL_MOUSEMOTION)
            {
                float vx, vy;
                mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);

                int hit = hitTestItem(vx, vy);
                if (hit >= 0)
                    m_cursor = hit;
                return true; // Consumir motion siempre
            }

            if (e->type != SDL_KEYDOWN) return true; // Consumir todo

            SDL_Keycode k = e->key.keysym.sym;

            // ESC = Resume
            if (k == SDLK_ESCAPE)
            {
                m_stack->cmdPop(this);  // cerrar pausa (ESC)
                return true;
            }

            // Navegación
            int count = (int)m_items.size();
            if (k == SDLK_UP || k == SDLK_w)
            {
                m_cursor = (m_cursor - 1 + count) % count;
                return true;
            }
            if (k == SDLK_DOWN || k == SDLK_s)
            {
                m_cursor = (m_cursor + 1) % count;
                return true;
            }

            // Confirmar
            if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_z)
            {
                if (m_cursor >= 0 && m_cursor < count)
                    executeItem(m_items[m_cursor].action);
                return true;
            }

            return true; // Consumir todo input para que el juego no reaccione
        }

        // ── Update ─────────────────────────────────────────────────────────

        void update(f64 dt) override
        {
            m_time += dt;
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

            // ── Dim overlay ────────────────────────────────────────────────
            draw2d::drawRect(0, 0, VW, VH, 0, 0, 0, 0.55f);

            // ── Panel central ──────────────────────────────────────────────
            int count = (int)m_items.size();
            float itemH    = 52.0f;
            float padding  = 24.0f;
            float titleH   = 50.0f;
            float sepH     = 16.0f;
            float panW     = VW * 0.32f;
            float panH     = titleH + sepH + count * itemH + padding * 2.0f;
            float panX     = (VW - panW) / 2.0f;
            float panY     = (VH - panH) / 2.0f;

            // Fondo del panel
            draw2d::drawRect(panX, panY, panW, panH,
                             0.05f, 0.06f, 0.14f, 0.94f);
            draw2d::drawRectBorder(panX, panY, panW, panH,
                                   0.30f, 0.55f, 1.0f, 0.85f, 2.5f);

            // ── Título "PAUSA" ─────────────────────────────────────────────
            {
                std::string title = eclips::langUI("PAUSA", "PAUSED");
                float tw = m_font.measureWidth(title);
                m_font.drawText(title,
                    panX + (panW - tw) / 2.0f, panY + padding,
                    0.7f, 0.85f, 1.0f, 1.0f);
            }

            // Separador
            float sepY = panY + padding + titleH - 6.0f;
            draw2d::drawRect(panX + 16.0f, sepY, panW - 32.0f, 1.5f,
                             0.30f, 0.55f, 1.0f, 0.5f);

            // ── Items del menú ─────────────────────────────────────────────
            float startY = sepY + sepH;

            for (int i = 0; i < count; ++i)
            {
                float iy   = startY + i * itemH;
                bool  sel  = (i == m_cursor);
                float btnX = panX + 16.0f;
                float btnW = panW - 32.0f;
                float btnH = itemH - 6.0f;

                // Fondo del item
                if (sel)
                {
                    draw2d::drawRect(btnX, iy, btnW, btnH,
                                     0.18f, 0.22f, 0.42f, 0.88f);
                    draw2d::drawRectBorder(btnX, iy, btnW, btnH,
                                           0.45f, 0.72f, 1.0f, 1.0f, 2.0f);
                }
                else
                {
                    draw2d::drawRect(btnX, iy, btnW, btnH,
                                     0.09f, 0.11f, 0.22f, 0.6f);
                }

                // Texto centrado
                float tw = m_font.measureWidth(m_items[i].label);
                float tx = btnX + (btnW - tw) / 2.0f;
                float ty = iy + (btnH - m_font.getLineHeight()) / 2.0f;

                if (sel)
                    m_font.drawText(m_items[i].label, tx, ty,
                                    1.0f, 1.0f, 1.0f, 1.0f);
                else
                    m_font.drawText(m_items[i].label, tx, ty,
                                    0.60f, 0.65f, 0.75f, 0.85f);
            }

            // ── Indicador ">" parpadeante ──────────────────────────────────
            if (m_cursor >= 0 && m_cursor < count)
            {
                float iy  = startY + m_cursor * itemH;
                float btnX = panX + 16.0f;
                float btnH = itemH - 6.0f;
                float tw = m_font.measureWidth(m_items[m_cursor].label);
                float btnW = panW - 32.0f;
                float ix = btnX + (btnW - tw) / 2.0f - 24.0f;
                float iy2 = iy + (btnH - m_font.getLineHeight()) / 2.0f;

                float blink = 0.6f + 0.4f * (float)std::sin(m_time * 4.0);
                m_font.drawText(">", ix, iy2,
                                0.50f, 0.70f, 1.0f, blink);
            }

            // ── Hint en la parte inferior ──────────────────────────────────
            if (m_hasSmall)
            {
                std::string hint = "ESC = Reanudar  |  Flechas + Enter = Navegar";
                float hw = m_small.measureWidth(hint);
                m_small.drawText(hint,
                    (VW - hw) / 2.0f, panY + panH + 14.0f,
                    0.40f, 0.44f, 0.55f, 0.6f);
            }

            // ── Diálogo de confirmación de Quit ────────────────────────────
            if (m_confirmQuit)
                renderConfirmDialog(VW, VH);
        }

    private:

        // ── Tipos internos ─────────────────────────────────────────────────

        enum class ItemAction
        {
            Resume,
            Save,
            Load,
            Settings,
            ReturnToVN,
            MainMenu,
            Quit
        };

        struct MenuItem
        {
            std::string label;
            ItemAction  action;
        };

        // ── Ejecutar acción ────────────────────────────────────────────────

        void executeItem(ItemAction action)
        {
            switch (action)
            {
                case ItemAction::Resume:
                    m_stack->cmdPop(this);  // cerrar pausa
                    break;

                case ItemAction::Save:
                    if (m_onSave)
                    {
                        // §1.10 DISUELTO (D1, Ciclo-de-Vida-B): se acabó el baile
                        // de copia-local + pop síncrono. cmdPop(this) encola el pop
                        // del PauseMenu CON identidad; m_onSave → openSaveMenu hace
                        // cmdPush (ya no push inmediato), así que el drenado FIFO
                        // ejecuta pop-PauseMenu → push-SaveMenu y el SaveMenu
                        // aterriza sobre el VN limpio. `this` sigue vivo hasta el
                        // próximo tick, así que llamar m_onSave() después es seguro.
                        m_stack->cmdPop(this);
                        m_onSave();
                    }
                    break;

                case ItemAction::Load:
                    if (m_onLoad)
                    {
                        // §1.10 DISUELTO (D1): idéntico a Save. cmdPop(this) +
                        // cmdPush del SaveMenu(Load) drenan en orden FIFO.
                        m_stack->cmdPop(this);
                        m_onLoad();
                    }
                    break;

                case ItemAction::Settings:
                    // Ola 3 · Slice B · Fork C: si el padre dio una ruta de config
                    // (m_onSettings → openSettings del VN), úsala: respeta el
                    // override `config` del creador. Si no, push directo de
                    // SettingsState (fallback, ej. Gameplay2D; §7.118). En ambos
                    // casos es sub-overlay: al cerrar se revela la pausa.
                    if (m_onSettings)
                        m_onSettings();
                    else
                        m_stack->push(std::make_unique<SettingsState>(
                            m_window, m_stack, m_fontSources, m_theme));
                    break;

                case ItemAction::ReturnToVN:
                    if (m_onReturnToVN)
                    {
                        m_onReturnToVN();
                        m_stack->cmdPop(this);
                    }
                    break;

                case ItemAction::MainMenu:
                    if (m_onMainMenu)
                    {
                        // Combo disuelto (P2.3): UN solo cmdPopToBottom. m_onMainMenu
                        // es ahora cmdPopToBottom (cableado por el padre en
                        // openPauseMenu), que baja TODO al fondo incluyendo este
                        // PauseMenu. El requestPop propio era redundante — disparaba
                        // un descarte de identidad + el resume-fantasma de cero
                        // frames. Se elimina.
                        m_onMainMenu();
                    }
                    break;

                case ItemAction::Quit:
                    // Mostrar confirmación antes de salir
                    m_confirmQuit = true;
                    m_confirmCursor = 1; // Default "No"
                    break;
            }
        }

        // ── Confirmación de Quit ───────────────────────────────────────────

        bool handleConfirmInput(const SDL_Event* e)
        {
            // Click en botones Sí/No
            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
            {
                float vx, vy;
                mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);

                int hit = hitTestConfirm(vx, vy);
                if (hit == 0) // Sí
                {
                    // Cerrar el juego
                    SDL_Event quit;
                    quit.type = SDL_QUIT;
                    SDL_PushEvent(&quit);
                    return true;
                }
                if (hit == 1) // No
                {
                    m_confirmQuit = false;
                    return true;
                }
            }

            // Hover en botones Sí/No
            if (e->type == SDL_MOUSEMOTION)
            {
                float vx, vy;
                mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);

                int hit = hitTestConfirm(vx, vy);
                if (hit >= 0)
                    m_confirmCursor = hit;
                return true;
            }

            if (e->type != SDL_KEYDOWN) return true;

            SDL_Keycode k = e->key.keysym.sym;

            // ESC = cancelar quit
            if (k == SDLK_ESCAPE)
            {
                m_confirmQuit = false;
                return true;
            }

            // Izquierda/Derecha para navegar Sí/No
            if (k == SDLK_LEFT || k == SDLK_a)
            {
                m_confirmCursor = 0; // Sí
                return true;
            }
            if (k == SDLK_RIGHT || k == SDLK_d)
            {
                m_confirmCursor = 1; // No
                return true;
            }

            // Confirmar
            if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_z)
            {
                if (m_confirmCursor == 0)
                {
                    SDL_Event quit;
                    quit.type = SDL_QUIT;
                    SDL_PushEvent(&quit);
                }
                else
                {
                    m_confirmQuit = false;
                }
                return true;
            }

            return true;
        }

        void renderConfirmDialog(float VW, float VH)
        {
            // Dim extra sobre el pause menu
            draw2d::drawRect(0, 0, VW, VH, 0, 0, 0, 0.35f);

            // Panel de confirmación
            float dW = VW * 0.28f;
            float dH = 150.0f;
            float dX = (VW - dW) / 2.0f;
            float dY = (VH - dH) / 2.0f;

            draw2d::drawRect(dX, dY, dW, dH,
                             0.08f, 0.06f, 0.16f, 0.96f);
            draw2d::drawRectBorder(dX, dY, dW, dH,
                                   0.60f, 0.35f, 0.35f, 0.9f, 2.5f);

            // Texto "¿Seguro que quieres salir?"
            if (m_hasFont)
            {
                std::string msg = eclips::langUI("Seguro que quieres salir?",
                                                 "Are you sure you want to quit?");
                float tw = m_font.measureWidth(msg);
                m_font.drawText(msg,
                    dX + (dW - tw) / 2.0f, dY + 20.0f,
                    0.90f, 0.75f, 0.75f, 1.0f);
            }

            // Botones Sí / No
            float btnW = 100.0f;
            float btnH = 44.0f;
            float btnY = dY + dH - btnH - 20.0f;
            float gap  = 30.0f;
            float totalW = btnW * 2 + gap;
            float btnStartX = dX + (dW - totalW) / 2.0f;

            // Cache positions for hit testing
            m_confirmBtnY = btnY;
            m_confirmBtnH = btnH;
            m_confirmBtnW = btnW;
            m_confirmBtnStartX = btnStartX;
            m_confirmBtnGap = gap;

            // `Si`/`No` colisionan con el diálogo del creador en un solo
            // namespace: es `P-9`, firmada como pérdida aceptada. Cuando
            // significan lo mismo es gratis; muerde cuando el registro difiere.
            const std::string labels[2] = { eclips::langUI("Si", "Yes"),
                                            eclips::langUI("No", "No") };

            for (int i = 0; i < 2; ++i)
            {
                float bx = btnStartX + i * (btnW + gap);
                bool sel = (i == m_confirmCursor);

                if (sel)
                {
                    draw2d::drawRect(bx, btnY, btnW, btnH,
                                     0.22f, 0.18f, 0.38f, 0.9f);
                    draw2d::drawRectBorder(bx, btnY, btnW, btnH,
                                           0.50f, 0.65f, 1.0f, 1.0f, 2.0f);
                }
                else
                {
                    draw2d::drawRect(bx, btnY, btnW, btnH,
                                     0.12f, 0.10f, 0.22f, 0.7f);
                }

                if (m_hasFont)
                {
                    float tw = m_font.measureWidth(labels[i]);
                    float tx = bx + (btnW - tw) / 2.0f;
                    float ty = btnY + (btnH - m_font.getLineHeight()) / 2.0f;

                    if (sel)
                        m_font.drawText(labels[i], tx, ty, 1.0f, 1.0f, 1.0f, 1.0f);
                    else
                        m_font.drawText(labels[i], tx, ty, 0.55f, 0.55f, 0.60f, 0.8f);
                }
            }
        }

        // ── Hit testing ────────────────────────────────────────────────────

        // Convierte coordenadas de mouse al espacio de diseño (viewport::width()×height())
        void mouseToVirtual(float mx, float my, float& vx, float& vy) const
        {
            float targetAspect = (float)viewport::width() / viewport::height();
            float winAspect    = (float)m_winW / m_winH;

            int vpX = 0, vpY = 0, vpW = m_winW, vpH = m_winH;
            if (winAspect > targetAspect)
            {
                vpW = (int)(m_winH * targetAspect);
                vpX = (m_winW - vpW) / 2;
            }
            else
            {
                vpH = (int)(m_winW / targetAspect);
                vpY = (m_winH - vpH) / 2;
            }

            vx = (mx - vpX) * (float)viewport::width()  / vpW;
            vy = (my - vpY) * (float)viewport::height() / vpH;
        }

        int hitTestItem(float vx, float vy) const
        {
            int count = (int)m_items.size();
            float itemH   = 52.0f;
            float padding = 24.0f;
            float titleH  = 50.0f;
            float sepH    = 16.0f;
            float panW    = (float)viewport::width() * 0.32f;
            float panH    = titleH + sepH + count * itemH + padding * 2.0f;
            float panX    = ((float)viewport::width()  - panW) / 2.0f;
            float panY    = ((float)viewport::height() - panH) / 2.0f;
            float sepY    = panY + padding + titleH - 6.0f;
            float startY  = sepY + sepH;
            float btnX    = panX + 16.0f;
            float btnW    = panW - 32.0f;

            for (int i = 0; i < count; ++i)
            {
                float iy  = startY + i * itemH;
                float btnH = itemH - 6.0f;
                if (vx >= btnX && vx <= btnX + btnW &&
                    vy >= iy   && vy <= iy + btnH)
                    return i;
            }
            return -1;
        }

        int hitTestConfirm(float vx, float vy) const
        {
            if (vy < m_confirmBtnY || vy > m_confirmBtnY + m_confirmBtnH)
                return -1;

            for (int i = 0; i < 2; ++i)
            {
                float bx = m_confirmBtnStartX + i * (m_confirmBtnW + m_confirmBtnGap);
                if (vx >= bx && vx <= bx + m_confirmBtnW)
                    return i;
            }
            return -1;
        }

        // ── Datos ──────────────────────────────────────────────────────────

        SDL_Window*   m_window = nullptr;
        StateStack*   m_stack  = nullptr;
        FontSourceStore* m_fontSources = nullptr;

        // Callbacks del estado padre (nullptr = botón oculto)
        std::function<void()> m_onSave;
        std::function<void()> m_onLoad;
        std::function<void()> m_onReturnToVN;
        std::function<void()> m_onMainMenu;
        // Ola 3 · Slice B · Fork C — ruta de Configuración del padre. Si está, el
        // botón Settings la usa (→ respeta el override `config` del creador); si
        // no (ej. Gameplay2D), cae al push directo de SettingsState (§7.118).
        std::function<void()> m_onSettings;

        // Theme activo del estado padre — reenviado a SettingsState
        // para que el menú de configuración pueda mostrar valores efectivos
        // y ofrecer "Restaurar valores del juego". Puede ser nullptr
        // (estados sin theme: menú principal, gameplay 2D sin VN).
        const Theme* m_theme = nullptr;

        // Items del menú (construidos en onEnter)
        std::vector<MenuItem> m_items;
        int m_cursor = 0;

        // Confirmación de Quit
        bool  m_confirmQuit   = false;
        int   m_confirmCursor = 1; // 0=Sí, 1=No (default No)

        // Posiciones cacheadas del diálogo de confirmación (para hit testing)
        mutable float m_confirmBtnY      = 0;
        mutable float m_confirmBtnH      = 0;
        mutable float m_confirmBtnW      = 0;
        mutable float m_confirmBtnStartX = 0;
        mutable float m_confirmBtnGap    = 0;

        // Render
        TextRenderer m_font;
        TextRenderer m_small;
        bool m_hasFont  = false;
        bool m_hasSmall = false;
        f64  m_time     = 0.0;
        int  m_winW     = 1280;
        int  m_winH     = 720;
    };

} // namespace eclips
