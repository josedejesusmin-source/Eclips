// ============================================================================
// SettingsState.h — Menú de configuración para Eclips Engine
// ============================================================================
//
// Overlay que se pushea desde PauseMenuState.
// Permite ajustar: text speed, auto delay, volúmenes, fullscreen.
// Los cambios se guardan automáticamente al cerrar.
//
// ── Theme opcional ───────────────────────────────────────────────────────────
//
// Si recibe un Theme* válido, los sliders muestran el VALOR EFECTIVO
// (override del jugador si existe; en su defecto, el del theme). Además
// aparece el botón "Restaurar valores del juego" cuando hay overrides
// activos — al pulsarlo se limpian las preferencias del jugador y los
// campos del patrón vuelven al theme.
//
// Sin theme (nullptr), el menú se comporta como editor de defaults globales:
// muestra los valores del motor sin fallback, y el botón Restore nunca
// aparece (no hay tema al cual restaurar).
//
// ── Locks (theme_locked) ─────────────────────────────────────────────────────
//
// Si el theme marca un campo como locked (ej. textSpeedLocked=true), el
// slider se muestra GRIS con un candado al lado del label. No responde a
// teclado (← →) ni a clicks (◀ ▶ ni en el área del valor). El cursor salta
// el ítem locked al navegar con UP/DOWN. Cuando hay lock, el slider muestra
// el valor del THEME ignorando el override del jugador (si hubiera) — el
// override sigue persistido pero inerte; revive si el creador quita el lock.
//
// El lock también se honra en runtime: los helpers effective_textSpeed_full,
// effective_autoDelay y effective_skipInterval en VNDialogBox.cpp consultan
// el flag locked del theme antes de leer Settings. Si el lock está activo,
// el override del jugador queda inerte tanto en la UI como en el runtime,
// aunque siga persistido en settings.eclips. La cadena de 6 niveles del
// speed hierarchy integra el theme_lock como kill-switch del nivel 5.
//
// ── Controles ────────────────────────────────────────────────────────────────
//
//   ↑ / ↓             → seleccionar opción
//   ← / →             → ajustar valor
//   R                 → restaurar valores del juego (si hay theme + overrides)
//   ESC / Click fuera → cerrar (guarda automáticamente)
//   Click en ◀ / ▶    → ajustar valor
//   Click en barra    → ajustar volumen directamente
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Types.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/core/Settings.h"
#include "Theme.h"

#include <SDL.h>
#include <string>
#include <vector>
#include "engine/core/LangStore.h"   // idiomas CP3: langUI() del chrome

#include <iostream>
#include <cmath>
#include <algorithm>

namespace eclips
{

    class SettingsState : public GameState
    {
    public:

        SettingsState(SDL_Window* window,
            StateStack* stack,
            FontSourceStore* fontSources,
            const Theme* theme = nullptr)
            : m_window(window)
            , m_stack(stack)
            , m_fontSources(fontSources)
            , m_theme(theme)
        {
            m_font.setSourceStore(m_fontSources);
            m_small.setSourceStore(m_fontSources);
        }

        bool isTransparent()     const override { return true; }
        bool allowsUpdateBelow() const override { return false; }

        // ── Ciclo de vida ──────────────────────────────────────────────────

        void onEnter() override
        {
            m_hasFont = m_font.init(Settings::defaultFontPath(), 28.0f);
            m_hasSmall = m_small.init(Settings::defaultFontPath(), 20.0f);

            // Leer valores actuales de Settings
            syncFromSettings();

            // Asegurar que el cursor inicia en un ítem no-locked.
            // Si ITEM_TEXT_SPEED (default 0) está locked, saltamos al siguiente.
            m_cursor = firstUnlockedItem();

            SDL_GetWindowSize(m_window, &m_winW, &m_winH);
            std::cout << "[Settings] Menu de configuracion abierto"
                << (m_theme ? " (con tema activo)" : " (sin tema)")
                << std::endl;
        }

        void onExit() override
        {
            // Guardar al cerrar
            applyToSettings();
            Settings::save();

            m_font.shutdown();
            m_small.shutdown();
            std::cout << "[Settings] Guardado y cerrado" << std::endl;
        }

        // ── Input ──────────────────────────────────────────────────────────

        bool handleEvent(const void* ev) override
        {
            const SDL_Event* e = static_cast<const SDL_Event*>(ev);

            // ── Click del mouse ───────────────────────────────────────────
            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
            {
                float vx, vy;
                mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);

                // Click en el botón Restore (si está visible)
                if (shouldShowRestoreButton() && hitTestRestoreButton(vx, vy))
                {
                    executeRestore();
                    return true;
                }

                // Check arrow buttons and slider bars
                if (handleMouseClick(vx, vy))
                    return true;

                // Click fuera del panel = cerrar
                if (isOutsidePanel(vx, vy))
                {
                    m_stack->requestPop();
                    return true;
                }

                return true;
            }

            // ── Hover ─────────────────────────────────────────────────────
            if (e->type == SDL_MOUSEMOTION)
            {
                float vx, vy;
                mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);
                handleMouseHover(vx, vy);
                return true;
            }

            if (e->type != SDL_KEYDOWN) return true;

            SDL_Keycode k = e->key.keysym.sym;

            if (k == SDLK_ESCAPE)
            {
                m_stack->requestPop();
                return true;
            }

            // R = restaurar valores del juego (solo si hay theme + overrides)
            if (k == SDLK_r && shouldShowRestoreButton())
            {
                executeRestore();
                return true;
            }

            if (k == SDLK_UP || k == SDLK_w)
            {
                m_cursor = nextUnlockedItem(m_cursor, -1);
                return true;
            }
            if (k == SDLK_DOWN || k == SDLK_s)
            {
                m_cursor = nextUnlockedItem(m_cursor, +1);
                return true;
            }

            if (k == SDLK_LEFT || k == SDLK_a)
            {
                adjustValue(m_cursor, -1);
                return true;
            }
            if (k == SDLK_RIGHT || k == SDLK_d)
            {
                adjustValue(m_cursor, +1);
                return true;
            }

            // Enter toggles fullscreen, otherwise same as right
            if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_z)
            {
                if (m_cursor == ITEM_FULLSCREEN)
                    adjustValue(m_cursor, m_fullscreen ? -1 : +1);
                else
                    adjustValue(m_cursor, +1);
                return true;
            }

            return true;
        }

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

            // ── Panel ──────────────────────────────────────────────────────
            float panW, panH, panX, panY;
            getPanelRect(panW, panH, panX, panY);

            draw2d::drawRect(panX, panY, panW, panH,
                0.05f, 0.06f, 0.14f, 0.94f);
            draw2d::drawRectBorder(panX, panY, panW, panH,
                0.30f, 0.55f, 1.0f, 0.85f, 2.5f);

            // ── Título ─────────────────────────────────────────────────────
            {
                std::string title = eclips::langUI("CONFIGURACION", "SETTINGS");
                float tw = m_font.measureWidth(title);
                m_font.drawText(title,
                    panX + (panW - tw) / 2.0f, panY + 16.0f,
                    0.7f, 0.85f, 1.0f, 1.0f);
            }

            // Separador
            float sepY = panY + 54.0f;
            draw2d::drawRect(panX + 16.0f, sepY, panW - 32.0f, 1.5f,
                0.30f, 0.55f, 1.0f, 0.5f);

            // ── Items ──────────────────────────────────────────────────────
            float startY = sepY + 20.0f;
            float rowH = 58.0f;
            float labelX = panX + 30.0f;
            float valueX = panX + panW * 0.45f;
            float arrowL = valueX - 28.0f;
            float arrowR = panX + panW - 46.0f;
            float barX = valueX;
            float barW = arrowR - valueX - 10.0f;

            const std::string labels[NUM_ITEMS] = {
                 eclips::langUI("Velocidad de texto", "Text Speed"),
                 eclips::langUI("Espera auto-avance", "Auto-Advance Delay"),
                 eclips::langUI("Volumen musica",     "Music Volume"),
                 eclips::langUI("Volumen efectos",    "Sound Volume"),
                 eclips::langUI("Pantalla completa",  "Fullscreen"),
                 eclips::langUI("Saltar no visto",    "Skip Unseen")
            };

            for (int i = 0; i < NUM_ITEMS; ++i)
            {
                float ry = startY + i * rowH;
                bool sel = (i == m_cursor);
                bool locked = isLocked(i);

                // Multiplicador de alpha para el ítem locked.
                // Aplicado a label, arrows y value — todo el slider se ve "apagado".
                // Aproximadamente 30% de la opacidad normal (votado en sesión).
                float lockAlpha = locked ? 0.30f : 1.0f;

                // Fondo de row seleccionado (cursor nunca debería caer en locked,
                // pero por defensa consultamos locked antes de pintar el realce).
                if (sel && !locked)
                {
                    draw2d::drawRect(panX + 12.0f, ry - 4.0f, panW - 24.0f, rowH - 6.0f,
                        0.14f, 0.17f, 0.34f, 0.7f);
                    draw2d::drawRectBorder(panX + 12.0f, ry - 4.0f, panW - 24.0f, rowH - 6.0f,
                        0.35f, 0.55f, 1.0f, 0.6f, 1.5f);
                }

                // Label
                float labelY = ry + (rowH - 6.0f - m_font.getLineHeight()) / 2.0f;
                m_font.drawText(labels[i], labelX, labelY,
                    sel ? 1.0f : 0.65f,
                    sel ? 1.0f : 0.68f,
                    sel ? 1.0f : 0.75f,
                    (sel ? 1.0f : 0.85f) * lockAlpha);

                // ── Candado tras el label si locked ───────────────────────
                // Posicionado justo después del texto del label, con un pequeño
                // gap. Marca la fila completa como locked desde el inicio.
                // FUTURO: si Theme define lockIconTexture (no implementado aún),
                // sustituir drawLockIcon por un sprite quad. El call site queda
                // aislado para que la sustitución sea de 1 línea.
                if (locked)
                {
                    float labelW = m_font.measureWidth(labels[i]);
                    float lockSize = 18.0f;
                    float lockX = labelX + labelW + 10.0f;
                    float lockY = labelY + (m_font.getLineHeight() - lockSize) * 0.5f;
                    drawLockIcon(lockX, lockY, lockSize,
                        0.55f, 0.55f, 0.65f, 0.85f);
                }

                // ── Arrows ◀ ▶ ────────────────────────────────────────────
                // Las flechas NO se dibujan cuando el item está locked.
                // El candado ya comunica "no puedes ajustar"; mantener flechas
                // a alpha residual crea ambigüedad visual — para labels largos,
                // la flecha `<` cae cerca del body del candado y puede dar
                // ilusión de asimetría especular. El candado solo es suficiente.
                if (!locked)
                {
                    float arrowY = labelY;
                    float arrowAlpha = sel ? 0.9f : 0.4f;
                    m_font.drawText("<", arrowL, arrowY,
                        0.5f, 0.7f, 1.0f, arrowAlpha);
                    m_font.drawText(">", arrowR, arrowY,
                        0.5f, 0.7f, 1.0f, arrowAlpha);
                }

                // ── Value display ─────────────────────────────────────────
                float valY = labelY;

                if (i == ITEM_TEXT_SPEED)
                {
                    std::string val = getTextSpeedLabel();
                    float vw = m_font.measureWidth(val);
                    float cx = barX + barW / 2.0f - vw / 2.0f;
                    m_font.drawText(val, cx, valY, 1.0f, 1.0f, 1.0f, 0.95f * lockAlpha);
                }
                else if (i == ITEM_AUTO_DELAY)
                {
                    std::string val = getAutoDelayLabel();
                    float vw = m_font.measureWidth(val);
                    float cx = barX + barW / 2.0f - vw / 2.0f;
                    m_font.drawText(val, cx, valY, 1.0f, 1.0f, 1.0f, 0.95f * lockAlpha);
                }
                else if (i == ITEM_MUSIC_VOL || i == ITEM_SFX_VOL)
                {
                    float vol = (i == ITEM_MUSIC_VOL) ? m_musicVol : m_sfxVol;
                    float barH = 18.0f;
                    float barY = ry + (rowH - 6.0f) / 2.0f - barH / 2.0f;

                    // Background bar
                    draw2d::drawRect(barX, barY, barW, barH,
                        0.12f, 0.12f, 0.20f, 0.8f * lockAlpha);

                    // Filled portion
                    float fillW = barW * vol;
                    if (fillW > 0)
                    {
                        draw2d::drawRect(barX, barY, fillW, barH,
                            0.25f, 0.50f, 0.90f, 0.85f * lockAlpha);
                    }

                    // Border
                    draw2d::drawRectBorder(barX, barY, barW, barH,
                        0.35f, 0.55f, 1.0f, 0.5f * lockAlpha, 1.0f);

                    // Percentage text
                    int pct = (int)(vol * 100.0f + 0.5f);
                    std::string pctStr = std::to_string(pct) + "%";
                    float pw = m_font.measureWidth(pctStr);
                    m_font.drawText(pctStr,
                        barX + barW / 2.0f - pw / 2.0f, valY,
                        1.0f, 1.0f, 1.0f, 0.95f * lockAlpha);
                }
                else if (i == ITEM_FULLSCREEN)
                {
                    std::string val = m_fullscreen ? eclips::langUI("Si", "Yes")
                                                   : eclips::langUI("No", "No");
                    float vw = m_font.measureWidth(val);
                    float cx = barX + barW / 2.0f - vw / 2.0f;
                    m_font.drawText(val, cx, valY,
                        m_fullscreen ? 0.4f : 1.0f,
                        m_fullscreen ? 1.0f : 0.8f,
                        m_fullscreen ? 0.4f : 0.8f,
                        0.95f * lockAlpha);
                }
                else if (i == ITEM_SKIP_UNSEEN)
                {
                    std::string val = m_skipUnseen ? eclips::langUI("Si", "Yes")
                                                   : eclips::langUI("No", "No");
                    float vw = m_font.measureWidth(val);
                    float cx = barX + barW / 2.0f - vw / 2.0f;
                    m_font.drawText(val, cx, valY,
                        m_skipUnseen ? 0.4f : 1.0f,
                        m_skipUnseen ? 1.0f : 0.8f,
                        m_skipUnseen ? 0.4f : 0.8f,
                        0.95f * lockAlpha);
                }
            }

            // ── Indicador ">" ──────────────────────────────────────────────
            {
                float ry = startY + m_cursor * rowH;
                float labelY = ry + (rowH - 6.0f - m_font.getLineHeight()) / 2.0f;
                float blink = 0.6f + 0.4f * (float)std::sin(m_time * 4.0);
                m_font.drawText(">", panX + 14.0f, labelY,
                    0.50f, 0.70f, 1.0f, blink);
            }

            // ── Botón Restore (si hay theme + overrides activos) ───────────
            if (shouldShowRestoreButton())
                renderRestoreButton(panW, panH, panX, panY);

            // ── Hint dinámico ──────────────────────────────────────────────
            if (m_hasSmall)
            {
                std::string hint = shouldShowRestoreButton()
                    ? eclips::langUI(
                          "Flechas = ajustar   R = restaurar tema   ESC = cerrar",
                          "Arrows = adjust   R = restore theme   ESC = close")
                    : eclips::langUI(
                          "Flechas = ajustar   ESC = guardar y cerrar",
                          "Arrows = adjust   ESC = save and close");
                float hw = m_small.measureWidth(hint);
                m_small.drawText(hint,
                    panX + (panW - hw) / 2.0f, panY + panH - 20.0f,
                    0.40f, 0.44f, 0.55f, 0.6f);
            }
        }

    private:

        // ── Constants ──────────────────────────────────────────────────────

        static constexpr int ITEM_TEXT_SPEED = 0;
        static constexpr int ITEM_AUTO_DELAY = 1;
        static constexpr int ITEM_MUSIC_VOL = 2;
        static constexpr int ITEM_SFX_VOL = 3;
        static constexpr int ITEM_FULLSCREEN = 4;
        static constexpr int ITEM_SKIP_UNSEEN = 5;
        static constexpr int NUM_ITEMS = 6;

        // Auto delay presets (seconds)
        static constexpr float AUTO_DELAYS[] = { 1.0f, 2.0f, 3.0f, 5.0f };
        static constexpr int   NUM_AUTO_DELAYS = 4;

        // ── Sync with Settings ─────────────────────────────────────────────

        void syncFromSettings()
        {
            // Text speed: con fallback del theme si está disponible.
            // Cuando hay lock activo, effectiveTextSpeed devuelve el valor del
            // theme directamente, ignorando cualquier override del jugador
            // (que puede seguir persistido pero queda inerte mientras el lock
            // esté activo). Si el creador quita el lock, el override revive.
            const TextSpeed ts = effectiveTextSpeed();

            m_textSpeedDomain = m_theme ? m_theme->textSpeedDomain :
                textSpeedLegacyDomain();
            if (m_textSpeedDomain.isNumeric())
            {
                m_textSpeedCatalog.clear();
                m_textSpeedIdx = textSpeedDomainCoordinate(m_textSpeedDomain, ts);
            }
            else
            {
                m_textSpeedCatalog = textSpeedCatalog(ts);
                const int index = textSpeedCatalogIndex(m_textSpeedCatalog, ts);
                m_textSpeedIdx = index < 0 ? 0.0f : static_cast<float>(index);
            }

            // Auto delay: misma lógica con effectiveAutoDelay.
            float ad = effectiveAutoDelay();

            m_autoDelayIdx = 1;
            float bestDist = 1e9f;
            for (int i = 0; i < NUM_AUTO_DELAYS; ++i)
            {
                float d = std::abs(ad - AUTO_DELAYS[i]);
                if (d < bestDist) { bestDist = d; m_autoDelayIdx = i; }
            }

            // Volumen y fullscreen son preferencias puras del jugador,
            // no aplican el patrón override.
            m_musicVol = Settings::musicVolume();
            m_sfxVol = Settings::sfxVolume();
            m_fullscreen = Settings::fullscreen();

            // skipUnseen sigue el patrón override (tiene lock): arranca en el
            // valor EFECTIVO (tema como fallback), y snapshot para el diff.
            m_skipUnseen = Settings::skipUnseen(m_theme ? m_theme->skipUnseen : false);

            // Snapshot para diff en applyToSettings() (§7.8).
            m_initialTextSpeedIdx = m_textSpeedIdx;
            m_initialAutoDelayIdx = m_autoDelayIdx;
            m_initialSkipUnseen = m_skipUnseen;
        }

        void applyToSettings()
        {
            // Patrón override: solo escribir si el jugador tocó este slider.
            // Si no cambió desde el snapshot, no marcamos user-set y el tema
            // sigue ganando.
            if (m_textSpeedIdx != m_initialTextSpeedIdx)
            {
                if (m_textSpeedDomain.isNumeric())
                    Settings::setTextSpeed(textSpeedFromDomainCoordinate(
                        m_textSpeedDomain, m_textSpeedIdx));
                else if (m_textSpeedIdx >= 0 &&
                         m_textSpeedIdx < (int)m_textSpeedCatalog.size())
                    Settings::setTextSpeed(m_textSpeedCatalog[
                        static_cast<size_t>(m_textSpeedIdx)].value);
            }
            if (m_autoDelayIdx != m_initialAutoDelayIdx)
                Settings::setAutoDelay(AUTO_DELAYS[m_autoDelayIdx]);
            if (m_skipUnseen != m_initialSkipUnseen)
                Settings::setSkipUnseen(m_skipUnseen);

            // Sin patrón: aplicar siempre (preferencias puras del jugador)
            Settings::setMusicVolume(m_musicVol);
            Settings::setSfxVolume(m_sfxVol);
            Settings::setFullscreen(m_fullscreen);
        }

        // ── Restore (botón "Restaurar valores del juego") ──────────────────

        // El botón aparece sólo si: (a) hay theme activo Y (b) hay al menos un
        // override del patrón actualmente persistido. No depende del snapshot
        // diff actual del menú — un cambio en curso (todavía no aplicado)
        // no hace aparecer el botón. Decisión deliberada: el botón es para
        // limpiar overrides confirmados, no para deshacer cambios in-progress
        // (que se deshacen moviendo el slider de vuelta).
        bool shouldShowRestoreButton() const
        {
            return m_theme != nullptr && Settings::hasAnyOverride();
        }

        void executeRestore()
        {
            std::cout << "[Settings] Restaurando valores del juego (limpiando overrides)"
                << std::endl;
            Settings::restoreThemeDefaults();
            // Re-sincronizar para que los sliders muestren el valor del theme.
            // syncFromSettings actualiza también el snapshot, así applyToSettings
            // no detectará "cambio" al cerrar y los flags userSet quedarán
            // limpios persistidos.
            syncFromSettings();
            // Reset hover por si el botón desaparece tras el reset
            m_restoreHover = false;
        }

        void getRestoreButtonRect(float panW, float panH, float panX, float panY,
            float& bx, float& by, float& bw, float& bh) const
        {
            bw = panW * 0.65f;
            bh = 38.0f;
            bx = panX + (panW - bw) / 2.0f;
            // Posicionado entre los items y el hint inferior.
            // panH ya reservó 50px extra cuando este botón aplica.
            by = panY + panH - 38.0f - bh - 2.0f;
        }

        bool hitTestRestoreButton(float vx, float vy) const
        {
            float panW, panH, panX, panY;
            getPanelRect(panW, panH, panX, panY);
            float bx, by, bw, bh;
            getRestoreButtonRect(panW, panH, panX, panY, bx, by, bw, bh);
            return vx >= bx && vx <= bx + bw && vy >= by && vy <= by + bh;
        }

        void renderRestoreButton(float panW, float panH, float panX, float panY)
        {
            float bx, by, bw, bh;
            getRestoreButtonRect(panW, panH, panX, panY, bx, by, bw, bh);

            bool hover = m_restoreHover;

            // Fondo cálido — ámbar/dorado para distinguir del azul de los sliders.
            // Indica visualmente "esto es una acción, no un setting".
            float r = hover ? 0.30f : 0.22f;
            float g = hover ? 0.20f : 0.16f;
            float b = hover ? 0.12f : 0.10f;
            float a = hover ? 0.95f : 0.85f;

            draw2d::drawRect(bx, by, bw, bh, r, g, b, a);
            draw2d::drawRectBorder(bx, by, bw, bh,
                0.85f, 0.65f, 0.30f,
                hover ? 1.0f : 0.85f, 2.0f);

            // Texto centrado
            std::string label = eclips::langUI("Restaurar valores del juego",
                                               "Restore Game Defaults");
            float tw = m_font.measureWidth(label);
            float tx = bx + (bw - tw) / 2.0f;
            float ty = by + (bh - m_font.getLineHeight()) / 2.0f;
            m_font.drawText(label, tx, ty,
                hover ? 1.0f : 0.95f,
                hover ? 0.85f : 0.78f,
                hover ? 0.55f : 0.50f,
                1.0f);
        }

        // ── Value adjustment ───────────────────────────────────────────────

        void adjustValue(int item, int dir)
        {
            // Early return si el item está locked. Cubre las 3 vías de entrada:
            // teclado (← → desde handleEvent), click en flechas ◀ ▶ (handleMouseClick),
            // y click en el área del valor de la fila.
            if (isLocked(item)) return;

            switch (item)
            {
            case ITEM_TEXT_SPEED:
                if (m_textSpeedDomain.isNumeric())
                {
                    m_textSpeedIdx = std::clamp(
                        m_textSpeedIdx + dir * m_textSpeedDomain.step,
                        m_textSpeedDomain.min,
                        textSpeedDomainEndpoint(m_textSpeedDomain));
                }
                else
                {
                    m_textSpeedIdx = std::clamp(m_textSpeedIdx + dir, 0.0f,
                        static_cast<float>(std::max(0,
                            (int)m_textSpeedCatalog.size() - 1)));
                }
                break;

            case ITEM_AUTO_DELAY:
                m_autoDelayIdx = std::clamp(m_autoDelayIdx + dir, 0, NUM_AUTO_DELAYS - 1);
                break;

            case ITEM_MUSIC_VOL:
                m_musicVol = std::clamp(m_musicVol + dir * 0.1f, 0.0f, 1.0f);
                // Redondear a 1 decimal para evitar drift de float
                m_musicVol = std::round(m_musicVol * 10.0f) / 10.0f;
                break;

            case ITEM_SFX_VOL:
                m_sfxVol = std::clamp(m_sfxVol + dir * 0.1f, 0.0f, 1.0f);
                m_sfxVol = std::round(m_sfxVol * 10.0f) / 10.0f;
                break;

            case ITEM_FULLSCREEN:
                m_fullscreen = !m_fullscreen;
                applyFullscreen();
                break;

            case ITEM_SKIP_UNSEEN:
                m_skipUnseen = !m_skipUnseen;
                break;
            }
        }

        // ── Fullscreen toggle ──────────────────────────────────────────────

        void applyFullscreen()
        {
            if (m_fullscreen)
                SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            else
                SDL_SetWindowFullscreen(m_window, 0);
        }

        // ── Labels ─────────────────────────────────────────────────────────

        std::string getTextSpeedLabel() const
        {
            if (m_textSpeedDomain.isNumeric())
            {
                const TextSpeed value = textSpeedFromDomainCoordinate(
                    m_textSpeedDomain, m_textSpeedIdx);
                return value.isInstantaneo()
                    ? eclips::langUI("Instantaneo", "Instant")
                    : textSpeedDescribe(value);
            }
            const int index = static_cast<int>(m_textSpeedIdx + 0.5f);
            if (index >= 0 && index < (int)m_textSpeedCatalog.size())
            {
                const auto& entry = m_textSpeedCatalog[static_cast<size_t>(index)];
                // Los cuatro labels canonicos conservan sus llamadas literales
                // para el guardian del chrome; una entrada temporal usa el
                // texto dinamico del catalogo compartido.
                if (entry.value.isCps() && entry.value.cps() == 15.0f)
                    return eclips::langUI("Lento", "Slow");
                if (entry.value.isCps() && entry.value.cps() == 30.0f)
                    return eclips::langUI("Normal", "Normal");
                if (entry.value.isCps() && entry.value.cps() == 60.0f)
                    return eclips::langUI("Rapido", "Fast");
                if (entry.value.isInstantaneo())
                    return eclips::langUI("Instantaneo", "Instant");
                return eclips::langTr(eclips::langActivo(), entry.labelEs.c_str(),
                                      entry.labelEn.c_str());
            }
            return eclips::langUI("Normal", "Normal");
        }

        std::string getAutoDelayLabel() const
        {
            switch (m_autoDelayIdx)
            {
            // Frases ENTERAS y no `{segundos} segundos`: acá el número es un
            // enum fijo de cuatro valores, así que el singular ya está bien
            // escrito y no hay hueco que llenar. La plantilla con hueco vive
            // donde el número es dinámico (el countdown del display, fork 8a).
            case 0: return eclips::langUI("1 segundo",  "1 second");
            case 1: return eclips::langUI("2 segundos", "2 seconds");
            case 2: return eclips::langUI("3 segundos", "3 seconds");
            case 3: return eclips::langUI("5 segundos", "5 seconds");
            default: return eclips::langUI("2 segundos", "2 seconds");
            }
        }

        // ── Mouse handling ─────────────────────────────────────────────────

        void mouseToVirtual(float mx, float my, float& vx, float& vy) const
        {
            float targetAspect = (float)viewport::width() / viewport::height();
            float winAspect = (float)m_winW / m_winH;

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

            vx = (mx - vpX) * (float)viewport::width() / vpW;
            vy = (my - vpY) * (float)viewport::height() / vpH;
        }

        void getPanelRect(float& panW, float& panH, float& panX, float& panY) const
        {
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            panW = VW * 0.48f;
            panH = 54.0f + 20.0f + NUM_ITEMS * 58.0f + 40.0f;
            // Espacio extra para el botón Restore cuando está visible
            if (shouldShowRestoreButton())
                panH += 50.0f;
            panX = (VW - panW) / 2.0f;
            panY = (VH - panH) / 2.0f;
        }

        bool isOutsidePanel(float vx, float vy) const
        {
            float panW, panH, panX, panY;
            getPanelRect(panW, panH, panX, panY);
            return (vx < panX || vx > panX + panW ||
                vy < panY || vy > panY + panH);
        }

        bool handleMouseClick(float vx, float vy)
        {
            float panW, panH, panX, panY;
            getPanelRect(panW, panH, panX, panY);

            float sepY = panY + 54.0f;
            float startY = sepY + 20.0f;
            float rowH = 58.0f;
            float valueX = panX + panW * 0.45f;
            float arrowL = valueX - 28.0f;
            float arrowR = panX + panW - 46.0f;
            float barX = valueX;
            float barW = arrowR - valueX - 10.0f;

            for (int i = 0; i < NUM_ITEMS; ++i)
            {
                float ry = startY + i * rowH;
                float rowTop = ry - 4.0f;
                float rowBot = ry + rowH - 10.0f;

                if (vy < rowTop || vy > rowBot) continue;

                // Row locked: consume el evento (no cierra el menú por "click
                // fuera") pero no selecciona ni ajusta nada. Es invisible al
                // jugador — el candado ya comunica "no me toques".
                if (isLocked(i))
                    return true;

                m_cursor = i;

                // Left arrow click
                if (vx >= arrowL && vx <= arrowL + 24.0f)
                {
                    adjustValue(i, -1);
                    return true;
                }

                // Right arrow click
                if (vx >= arrowR && vx <= arrowR + 24.0f)
                {
                    adjustValue(i, +1);
                    return true;
                }

                // Volume bar direct click
                if ((i == ITEM_MUSIC_VOL || i == ITEM_SFX_VOL) &&
                    vx >= barX && vx <= barX + barW)
                {
                    float ratio = (vx - barX) / barW;
                    ratio = std::clamp(ratio, 0.0f, 1.0f);
                    ratio = std::round(ratio * 10.0f) / 10.0f;

                    if (i == ITEM_MUSIC_VOL) m_musicVol = ratio;
                    else                     m_sfxVol = ratio;
                    return true;
                }

                // Click anywhere else on the row = toggle/cycle
                if (vx >= valueX && vx <= arrowR)
                {
                    adjustValue(i, +1);
                    return true;
                }

                return true; // Consumed by row selection
            }

            return false;
        }

        void handleMouseHover(float vx, float vy)
        {
            float panW, panH, panX, panY;
            getPanelRect(panW, panH, panX, panY);

            // Hover sobre el botón Restore (si está visible)
            m_restoreHover = shouldShowRestoreButton() && hitTestRestoreButton(vx, vy);

            float sepY = panY + 54.0f;
            float startY = sepY + 20.0f;
            float rowH = 58.0f;

            for (int i = 0; i < NUM_ITEMS; ++i)
            {
                float ry = startY + i * rowH;
                if (vy >= ry - 4.0f && vy <= ry + rowH - 10.0f &&
                    vx >= panX + 12.0f && vx <= panX + panW - 12.0f)
                {
                    // Hover sobre row locked no mueve el cursor — análogo a
                    // botones disabled. El cursor se queda donde estaba.
                    if (isLocked(i)) return;
                    m_cursor = i;
                    return;
                }
            }
        }

        // ── Lock helpers ───────────────────────────────────────────────────

        // ¿El item está marcado como locked en el theme?
        // Solo aplica a los items del patrón override (text speed, auto delay).
        // Volumen y fullscreen son preferencias puras del jugador — no tienen
        // concepto de lock. ITEM_SKIP_INTERVAL no existe en este menú aún
        // (Theme lo tiene como skipIntervalLocked pero no hay slider para él).
        bool isLocked(int item) const
        {
            if (m_theme == nullptr) return false;
            switch (item)
            {
            case ITEM_TEXT_SPEED: return m_theme->textSpeedLocked;
            case ITEM_AUTO_DELAY: return m_theme->autoDelayLocked;
            case ITEM_SKIP_UNSEEN: return m_theme->skipUnseenLocked;
            default:              return false;
            }
        }

        // Valor efectivo a mostrar en el slider de text speed.
        // Cuando hay lock activo, devuelve el valor del theme directamente
        // (ignora override). Cuando no hay lock, devuelve el efectivo del
        // patrón override (jugador si tocó, theme si no, default global si
        // no hay theme). El override del jugador NUNCA se borra aquí — solo
        // queda inerte mientras el lock esté activo.
        TextSpeed effectiveTextSpeed() const
        {
            const std::optional<TextSpeed> theme = m_theme ?
                std::optional<TextSpeed>(m_theme->textSpeed) : std::nullopt;
            const TextSpeedResolution resolution = resolveTextSpeed(
                std::nullopt, std::nullopt, false,
                theme, m_theme && m_theme->textSpeedLocked,
                Settings::textSpeedOverride());
            if (resolution.source == TextSpeedSource::Player &&
                m_theme && m_theme->textSpeedDomain.isNumeric())
                return Settings::projectTextSpeedForDomain(
                    m_theme->textSpeedDomain);
            return resolution.value;
        }

        float effectiveAutoDelay() const
        {
            if (m_theme && m_theme->autoDelayLocked) return m_theme->autoDelay;
            return m_theme
                ? Settings::autoDelay(m_theme->autoDelay)
                : Settings::autoDelay();
        }

        // Primer item no-locked partiendo desde 0. Si TODOS estuvieran locked
        // (escenario imposible con el set actual de items, ya que volumen y
        // fullscreen no son lockables), devuelve 0 como fallback de seguridad.
        int firstUnlockedItem() const
        {
            for (int i = 0; i < NUM_ITEMS; ++i)
                if (!isLocked(i)) return i;
            return 0;
        }

        // Siguiente item no-locked en la dirección dada (+1 = abajo, -1 = arriba).
        // Wrap circular como antes. Salta items locked. Safety: si TODOS están
        // locked (imposible hoy, defensivo), devuelve el original sin cambio.
        int nextUnlockedItem(int from, int dir) const
        {
            for (int step = 1; step <= NUM_ITEMS; ++step)
            {
                int candidate = ((from + dir * step) % NUM_ITEMS + NUM_ITEMS) % NUM_ITEMS;
                if (!isLocked(candidate)) return candidate;
            }
            return from;
        }

        // Dibuja un candadito con primitivas draw2d. Tamaño en px de altura
        // total (body + arch). Se ve aproximadamente así (size=18):
        //
        //     ▄▀▀▄         <- arch (shackle)
        //     █  █
        //   ████████       <- body
        //   ████████
        //   ████████
        //
        // FUTURO: cuando Theme defina lockIconTexture, sustituir el cuerpo
        // de esta función por un sprite quad. El call site no cambia.
        void drawLockIcon(float x, float y, float size,
            float r, float g, float b, float a) const
        {
            // Proporciones del candado: body ~55% del alto, arch ~50% del alto.
            // El arch sale del body hacia arriba — solapa ~5% para que se vea
            // pegado sin gap.
            float bodyW = size * 0.85f;
            float bodyH = size * 0.55f;
            float bodyX = x + (size - bodyW) * 0.5f;
            float bodyY = y + size - bodyH;

            // Body sólido
            draw2d::drawRect(bodyX, bodyY, bodyW, bodyH, r, g, b, a);

            // Punto del agujero del candado (decorativo)
            float holeSize = size * 0.18f;
            float holeX = bodyX + (bodyW - holeSize) * 0.5f;
            float holeY = bodyY + bodyH * 0.30f;
            draw2d::drawRect(holeX, holeY, holeSize, holeSize,
                0.10f, 0.10f, 0.14f, a);

            // Arch (shackle) — dibujado como U invertida con 3 rects:
            // dos verticales a los lados + uno horizontal arriba.
            float archW = bodyW * 0.65f;
            float archH = size * 0.50f;
            float archX = x + (size - archW) * 0.5f;
            float archY = bodyY - archH + size * 0.05f;  // solapar con body
            float archThick = std::max(2.0f, size * 0.13f);

            // Top horizontal
            draw2d::drawRect(archX, archY, archW, archThick, r, g, b, a);
            // Left vertical
            draw2d::drawRect(archX, archY, archThick, archH, r, g, b, a);
            // Right vertical
            draw2d::drawRect(archX + archW - archThick, archY, archThick, archH,
                r, g, b, a);
        }

        // ── Datos ──────────────────────────────────────────────────────────

        SDL_Window* m_window = nullptr;
        StateStack* m_stack = nullptr;
        FontSourceStore* m_fontSources = nullptr;
        const Theme* m_theme = nullptr;  // Opcional: nullptr si no hay tema activo

        int   m_cursor = 0;
        std::vector<TextSpeedCatalogEntry> m_textSpeedCatalog;
        TextSpeedDomain m_textSpeedDomain = textSpeedLegacyDomain();
        float m_textSpeedIdx = 1.0f;
        int   m_autoDelayIdx = 1;  // 0=1s, 1=2s, 2=3s, 3=5s
        float m_musicVol = 0.7f;

        // Snapshots de sync — para detectar qué slider tocó realmente el jugador.
        // Sin esto, applyToSettings() marcaría todos los campos como user-set,
        // robándole el control al tema activo (§7.8).
        float m_initialTextSpeedIdx = 1.0f;
        int   m_initialAutoDelayIdx = 1;
        float m_sfxVol = 1.0f;
        bool m_fullscreen = false;
        bool m_skipUnseen = false;        // toggle skip-de-leído
        bool m_initialSkipUnseen = false; // snapshot para diff (patrón override)

        // Hover del botón Restore (visual feedback)
        bool m_restoreHover = false;

        TextRenderer m_font;
        TextRenderer m_small;
        bool m_hasFont = false;
        bool m_hasSmall = false;
        f64  m_time = 0.0;
        int  m_winW = 1280;
        int  m_winH = 720;
    };

} // namespace eclips
