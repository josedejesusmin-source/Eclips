// ============================================================================
// MainMenuState.h — Pantalla de título para Eclips Engine
// ============================================================================
//
// El primer estado que ve el jugador. Muestra el título del juego y
// opciones para empezar, cargar, o salir.
//
// ── Controles ────────────────────────────────────────────────────────────────
//
//   ↑ / ↓           → navegar opciones
//   Enter / Space    → confirmar
//   Click izquierdo  → seleccionar opción
//   ESC              → salir del juego
//
// ── Uso desde main.cpp ──────────────────────────────────────────────────────
//
//   #include "MainMenuState.h"
//   engine.getStateStack().push(
//       std::make_unique<eclips::MainMenuState>(
//           engine.getWindow(), engine.getGLContext(),
//           &engine.getStateStack(), "vnscript/intro.ecl"
//       )
//   );
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Types.h"
#include "engine/core/Settings.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/render/Texture.h"
#include "VNScriptState.h"
#include "VNSaveMenu.h"
#include "SettingsState.h"
#include "engine/core/ProjectManifest.h"
#include "engine/core/MetaStore.h"   // ← nuevo
#include "engine/core/SeenStore.h"   // ← P1
#include "engine/core/LangStore.h"   // idiomas CP1: pass-through al VN

#include <SDL.h>
#include <string>
#include <vector>
#include <iostream>
#include <cmath>
#include <functional>
#include <filesystem>
#include <filesystem>

namespace eclips
{

    class MainMenuState : public GameState
    {
    public:
        // `input` va sin valor por defecto y en el grupo de infraestructura, por
        // la misma razón que en `VNScriptState`: este menú no lo usa para sí
        // —no consume verbos todavía, su nav es C++ propia (§1.2 del contrato:
        // "NO migran")— pero **se lo tiene que pasar a las VN que crea**. Un
        // `= nullptr` acá habría dejado que "Nueva Partida" lanzara un VN sin
        // teclado, compilando perfecto. §7.471.
        MainMenuState(SDL_Window* window,
            SDL_GLContext  glContext,
            StateStack* stateStack,
            FontSourceStore* fontSources,
            eclips::InputManager* input,   // `CP9b`: pass-through al VN, que remapea
            eclips::LangStore* langStore,  // idiomas CP1: pass-through obligatorio
            const std::string& scriptPath,
            const std::string& bgPath = "",
            const std::string& title = "ECLIPS",
            const ProjectManifest* manifest = nullptr,
            MetaStore* metaStore = nullptr,
            SeenStore* seenStore = nullptr)
            : m_window(window)
            , m_glContext(glContext)
            , m_stateStack(stateStack)
            , m_fontSources(fontSources)
            , m_input(input)
            , m_lang(langStore)
            , m_scriptPath(scriptPath)
            , m_bgPath(bgPath)
            , m_title(title)
            , m_manifest(manifest)
            , m_meta(metaStore)
            , m_seen(seenStore)
        {
            m_titleFont.setSourceStore(m_fontSources);
            m_menuFont.setSourceStore(m_fontSources);
            m_smallFont.setSourceStore(m_fontSources);
            // Si el manifest define script_inicial, gana sobre el default
            // pasado por main.cpp (ver comentario en main.cpp).
            if (m_manifest && !m_manifest->scriptInicial.empty())
                m_scriptPath = m_manifest->scriptInicial;
        }

        // ── GameState ─────────────────────────────────────────────────────────

        // FADE-DE-ESTADO-SIN-PERILLA: el menú principal interno ES una pantalla
        // de menú. Hoy los dos sitios que empuja desde acá (Nueva Partida y
        // cargar) llevan rol Game, así que su transición cae en `abrir` por el
        // destino y esto no cambia nada; se declara igual porque **es cierto**,
        // y porque el día que este menú empuje una pantalla de menú autorada la
        // ley tiene que contestar `navegar` sin que nadie se acuerde de volver
        // acá. Los otros states C++ (`PauseMenuState`, `SettingsState`,
        // `VNSaveMenu`) se quedan con el default `false` a propósito: **ninguno
        // empuja un `VNScriptState`** —medido, los únicos pushers son este
        // archivo, `VNScriptState` y `main.cpp`— así que declararlo ahí sería
        // capacidad sin caller (§7.118).
        bool esPantallaDeMenu() const override { return true; }

        void onEnter() override
        {
            m_hasTitle  = m_titleFont.init(Settings::defaultFontPath(), 72.0f);
            m_hasMenu   = m_menuFont.init(Settings::defaultFontPath(), 32.0f);
            m_hasSmall  = m_smallFont.init(Settings::defaultFontPath(), 20.0f);

            // Cargar fondo si se especificó
            if (!m_bgPath.empty())
                m_bgTex.load(m_bgPath);

            // Verificar si hay partidas guardadas (para mostrar "Cargar")
            m_hasSaves = false;
            for (int i = 0; i < VNSaveMenu::NUM_SLOTS; ++i)
            {
                auto info = VNSaveMenu::readSlot(i);
                if (info.occupied) { m_hasSaves = true; break; }
            }

            // Construir items del menú
            m_items.clear();
            // Idiomas CP3 §5: la clave de motor ES el español de fábrica, así
            // que un tercer idioma las cubre desde su `.lang` sin formato nuevo.
            // Sin catálogo, `langUI` devuelve el español → byte-idéntico.
            m_items.push_back({ eclips::langUI("Nueva Partida", "New Game"),
                                [this]{ startNewGame(); } });
            m_items.push_back({ eclips::langUI("Cargar Partida", "Load Game"),
                                [this]{ openLoadMenu(); } });
            m_items.push_back({ eclips::langUI("Configuracion", "Settings"),
                                [this] { openSettings(); } });
            m_items.push_back({ eclips::langUI("Salir", "Quit"),
                                [this] { m_stateStack->cmdPop(this); } });

            SDL_GetWindowSize(m_window, &m_winW, &m_winH);

            // Fade in
            m_fadeAlpha = 1.0f;

            std::cout << "[MainMenu] Eclips Engine - Menu principal" << std::endl;
        }

        void onExit() override
        {
            m_bgTex.unload();
            m_titleFont.shutdown();
            m_menuFont.shutdown();
            m_smallFont.shutdown();
        }

        // El LangStore nació con un F8 (ficha `F8-SIN-IDIOMA`, fork 4c).
        // Este state NO traduce con `m_lang` —su chrome usa `langUI()`, que lee
        // el store global— así que a primera vista el override sobra. No sobra:
        // `m_lang` es lo que este menú le PASA a cada VNScriptState que lanza
        // (`:182` cargar partida, `:465` nueva partida). Sin esto, el creador
        // agrega `idiomas`, aprieta F8 desde el menú, entra al juego, y la
        // escena nace con el selector muerto — un fallo mudo y a un salto de
        // distancia de su causa.
        void onLangStoreChanged(eclips::LangStore* store) override
        {
            m_lang = store;
        }

        void onResume() override
        {
            // Volvemos al menú (VN terminó o se canceló la carga)
            // Re-verificar saves
            m_hasSaves = false;
            for (int i = 0; i < VNSaveMenu::NUM_SLOTS; ++i)
            {
                auto info = VNSaveMenu::readSlot(i);
                if (info.occupied) { m_hasSaves = true; break; }
            }

            // Procesar carga pendiente (si el usuario eligió un slot)
            if (m_pendingLoadSlot >= 0)
            {
                int slot = m_pendingLoadSlot;
                m_pendingLoadSlot = -1;

                // Si el save fue hecho en otro script (encadenado),
                // usar esa ruta en vez de la default
                auto slotInfo = VNSaveMenu::readSlot(slot);
                std::string loadPath = (!slotInfo.scriptPath.empty())
                    ? slotInfo.scriptPath
                    : m_scriptPath;

                // Resolver path antes de pushear (mismo patrón que startNewGame).
                // Saves antiguos pueden traer paths sin prefijo "vnscript/".
                std::string resolved = resolveScriptPath(loadPath);
                if (resolved.empty())
                {
                    std::cerr << "[MainMenu] ERROR: El slot " << slot + 1
                        << " referencia un script que no existe: '"
                        << loadPath
                        << "' (probado en raiz y en vnscript/)."
                        << std::endl;
                    return;
                }

                m_stateStack->push(
                    std::make_unique<VNScriptState>(
                        m_window, m_glContext, m_stateStack, m_fontSources,
                        m_input, m_lang,
                        resolved, slot, m_manifest, m_meta, m_seen
                    )
                );
            }
        }

        // ── Input ─────────────────────────────────────────────────────────────

        bool handleEvent(const void* event) override
        {
            const SDL_Event* e = static_cast<const SDL_Event*>(event);
            int numItems = (int)m_items.size();

            if (e->type == SDL_KEYDOWN)
            {
                SDL_Keycode k = e->key.keysym.sym;

                if (k == SDLK_UP || k == SDLK_w)
                {
                    m_cursor = (m_cursor - 1 + numItems) % numItems;
                    // Saltar "Cargar" si no hay saves
                    if (m_cursor == 1 && !m_hasSaves)
                        m_cursor = 0;
                    return true;
                }
                if (k == SDLK_DOWN || k == SDLK_s)
                {
                    m_cursor = (m_cursor + 1) % numItems;
                    // Saltar "Cargar" si no hay saves
                    if (m_cursor == 1 && !m_hasSaves)
                        m_cursor = 2;
                    return true;
                }
                if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_z)
                {
                    if (m_cursor >= 0 && m_cursor < numItems)
                        m_items[m_cursor].action();
                    return true;
                }
                if (k == SDLK_ESCAPE)
                {
                    m_stateStack->cmdPop(this);
                    return true;
                }
            }

            // Click en opciones
            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
            {
                float mx = (float)e->button.x;
                float my = (float)e->button.y;

                int vpX, vpY, vpW, vpH;
                getVirtualViewport(vpX, vpY, vpW, vpH);
                float scaleY = (float)viewport::height() / vpH;
                float vy = (my - vpY) * scaleY;

                for (int i = 0; i < numItems; ++i)
                {
                    if (i == 1 && !m_hasSaves) continue; // Skip disabled
                    float itemY = m_menuStartY + i * m_itemSpacing;
                    if (vy >= itemY && vy <= itemY + 42.0f)
                    {
                        m_cursor = i;
                        m_items[i].action();
                        return true;
                    }
                }
            }

            // Hover con mouse
            if (e->type == SDL_MOUSEMOTION)
            {
                float my = (float)e->motion.y;
                int vpX, vpY, vpW, vpH;
                getVirtualViewport(vpX, vpY, vpW, vpH);
                float scaleY = (float)viewport::height() / vpH;
                float vy = (my - vpY) * scaleY;

                for (int i = 0; i < numItems; ++i)
                {
                    if (i == 1 && !m_hasSaves) continue;
                    float itemY = m_menuStartY + i * m_itemSpacing;
                    if (vy >= itemY && vy <= itemY + 42.0f)
                    {
                        m_cursor = i;
                        break;
                    }
                }
            }

            return false;
        }

        // ── Update ────────────────────────────────────────────────────────────

        void update(f64 dt) override
        {
            SDL_GetWindowSize(m_window, &m_winW, &m_winH);

            // Fade in
            if (m_fadeAlpha > 0)
            {
                m_fadeAlpha -= (float)dt * 1.5f;
                if (m_fadeAlpha < 0) m_fadeAlpha = 0;
            }

            // Animación sutil del título
            m_time += dt;
        }

        // ── Render ────────────────────────────────────────────────────────────

        void render(f64 /*alpha*/) override
        {
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();

            draw2d::setupViewport(m_winW, m_winH,
                                  viewport::width(), viewport::height());
            draw2d::setupProjection(VW, VH);

            // ── Fondo ──────────────────────────────────────────────────────
            if (m_bgTex.isLoaded())
            {
                draw2d::drawBackground(m_bgTex, VW, VH);
                // Overlay oscuro para que el texto sea legible
                draw2d::drawRect(0, 0, VW, VH, 0.0f, 0.0f, 0.0f, 0.55f);
            }
            else
            {
                // Gradiente simulado con rectángulos
                draw2d::drawRect(0, 0, VW, VH, 0.04f, 0.03f, 0.10f);
                draw2d::drawRect(0, VH * 0.6f, VW, VH * 0.4f,
                                 0.02f, 0.02f, 0.06f, 0.8f);
            }

            // ── Líneas decorativas sutiles ─────────────────────────────────
            float lineAlpha = 0.12f;
            draw2d::drawRect(VW * 0.15f, VH * 0.38f, VW * 0.70f, 1.0f,
                             0.4f, 0.5f, 0.8f, lineAlpha);
            draw2d::drawRect(VW * 0.15f, VH * 0.80f, VW * 0.70f, 1.0f,
                             0.4f, 0.5f, 0.8f, lineAlpha);

            // ── Título ─────────────────────────────────────────────────────
            if (m_hasTitle)
            {
                float tw = m_titleFont.measureWidth(m_title);
                float tx = (VW - tw) / 2.0f;
                float ty = VH * 0.18f;

                // Resplandor sutil detrás del título
                float glow = 0.08f + 0.03f * (float)std::sin(m_time * 1.5);
                draw2d::drawRect(tx - 30.0f, ty - 10.0f,
                                 tw + 60.0f, m_titleFont.getLineHeight() + 20.0f,
                                 0.15f, 0.20f, 0.45f, glow);

                m_titleFont.drawText(m_title, tx, ty,
                                     0.85f, 0.90f, 1.0f, 1.0f);
            }

            // ── Subtítulo ──────────────────────────────────────────────────
            if (m_hasSmall)
            {
                std::string sub = "Corre en una pluma. Crea sin limites.";
                float sw = m_smallFont.measureWidth(sub);
                m_smallFont.drawText(sub,
                    (VW - sw) / 2.0f, VH * 0.30f,
                    0.50f, 0.55f, 0.70f, 0.7f);
            }

            // ── Opciones del menú ──────────────────────────────────────────
            if (m_hasMenu)
            {
                int n = (int)m_items.size();
                m_itemSpacing = 56.0f;
                float totalH = n * m_itemSpacing;
                m_menuStartY = VH * 0.48f;

                for (int i = 0; i < n; ++i)
                {
                    float iy = m_menuStartY + i * m_itemSpacing;
                    bool selected = (i == m_cursor);
                    bool disabled = (i == 1 && !m_hasSaves);

                    // Fondo del item seleccionado
                    if (selected && !disabled)
                    {
                        float bw = VW * 0.35f;
                        float bx = (VW - bw) / 2.0f;
                        draw2d::drawRect(bx, iy - 4.0f, bw, 44.0f,
                                         0.12f, 0.16f, 0.35f, 0.65f);
                        draw2d::drawRectBorder(bx, iy - 4.0f, bw, 44.0f,
                                               0.35f, 0.55f, 1.0f, 0.55f, 1.5f);
                    }

                    // Texto
                    float iw = m_menuFont.measureWidth(m_items[i].label);
                    float ix = (VW - iw) / 2.0f;

                    if (disabled)
                    {
                        // Gris para "Cargar" sin saves
                        m_menuFont.drawText(m_items[i].label, ix, iy,
                                            0.30f, 0.30f, 0.35f, 0.5f);
                    }
                    else if (selected)
                    {
                        m_menuFont.drawText(m_items[i].label, ix, iy,
                                            1.0f, 1.0f, 1.0f, 1.0f);
                    }
                    else
                    {
                        m_menuFont.drawText(m_items[i].label, ix, iy,
                                            0.60f, 0.65f, 0.75f, 0.85f);
                    }
                }
            }

            // ── Indicador de selección ">" ─────────────────────────────────
            if (m_hasMenu && m_cursor >= 0 && m_cursor < (int)m_items.size())
            {
                bool disabled = (m_cursor == 1 && !m_hasSaves);
                if (!disabled)
                {
                    float iy = m_menuStartY + m_cursor * m_itemSpacing;
                    float iw = m_menuFont.measureWidth(m_items[m_cursor].label);
                    float ix = (VW - iw) / 2.0f - 28.0f;

                    // Parpadeo sutil
                    float blink = 0.6f + 0.4f * (float)std::sin(m_time * 4.0);
                    m_menuFont.drawText(">", ix, iy,
                                        0.50f, 0.70f, 1.0f, blink);
                }
            }

            // ── Versión (esquina inferior derecha) ─────────────────────────
            if (m_hasSmall)
            {
                std::string ver = std::string(constants::ENGINE_VERSION);
                if (constants::ENGINE_ETIQUETA[0] != '\0')
                    ver += std::string(" ") + constants::ENGINE_ETIQUETA;
                float vw = m_smallFont.measureWidth(ver);
                m_smallFont.drawText(ver,
                    VW - vw - 16.0f, VH - 28.0f,
                    0.35f, 0.38f, 0.45f, 0.5f);
            }
        }

    private:

        // ── Acciones del menú ─────────────────────────────────────────────────

        // ── Resolver path del script (migrado a ProjectManifest.h Pasada 3) ──
        //
        // resolveScriptPath ahora vive como función libre en
        // engine/core/ProjectManifest.h. Consulta scriptDir del manifest
        // cuando existe, y conserva el fallback "vnscript/" sin él.
        // Esta función local es ahora un wrapper que pasa m_manifest.
        std::string resolveScriptPath(const std::string& path)
        {
            return eclips::resolveScriptPath(path, m_manifest);
        }

        void startNewGame()
        {
            std::string resolved = resolveScriptPath(m_scriptPath);
            if (resolved.empty())
            {
                // Fail-fast: NO pushear si el script no existe.
                // Antes del fix §1.10, pasar un .ecl inexistente provocaba un
                // crash dentro de VNScriptState::onEnter(); ahora ese fallo
                // queda contenido (gracias al m_failed flag), pero igual es
                // mejor evitarlo de raíz para no entrar/salir del state.
                std::cerr << "[MainMenu] ERROR: No se encontro el script '"
                    << m_scriptPath
                    << "' (probado en raiz y en vnscript/)."
                    << std::endl;
                return;
            }

            std::cout << "[MainMenu] Nueva Partida -> " << resolved << std::endl;
            m_stateStack->push(
                std::make_unique<VNScriptState>(
                    m_window, m_glContext, m_stateStack, m_fontSources,
                    m_input, m_lang,
                    resolved, -1, m_manifest, m_meta, m_seen
                )
            );
        }

        void openLoadMenu()
        {
            if (!m_hasSaves) return;

            // Abrir VNSaveMenu como overlay sobre el menú principal.
            // Cuando el jugador elige un slot, guardamos el número
            // y en onResume() creamos VNScriptState con ese slot.
            m_stateStack->push(std::make_unique<VNSaveMenu>(
                m_window, m_stateStack, m_fontSources,
                VNSaveMenu::Mode::Load,
                [](int){},  // onSave no se usa
                [this](int slot)
                {
                    m_pendingLoadSlot = slot;
                    std::cout << "[MainMenu] Cargar slot "
                              << slot + 1 << std::endl;
                }
            ));
        }

        void openSettings()
        {
            m_stateStack->push(std::make_unique<SettingsState>(
                m_window, m_stateStack, m_fontSources
            ));
        }


        // ── Viewport virtual ──────────────────────────────────────────────────

        void getVirtualViewport(int& vpX, int& vpY, int& vpW, int& vpH) const
        {
            float targetAspect = (float)viewport::width() / viewport::height();
            float winAspect    = (float)m_winW / m_winH;
            vpX = 0; vpY = 0; vpW = m_winW; vpH = m_winH;
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
        }

        // ── Datos ─────────────────────────────────────────────────────────────

        struct MenuItem
        {
            std::string label;
            std::function<void()> action;
        };

        SDL_Window* m_window = nullptr;
        SDL_GLContext  m_glContext = nullptr;
        StateStack* m_stateStack = nullptr;
        FontSourceStore* m_fontSources = nullptr;
        std::string    m_scriptPath;
        std::string    m_bgPath;
        std::string    m_title;

        // ── Manifest del proyecto (puntero opcional, default nullptr) ──
        // Se propaga a VNScriptState al pushear nueva partida o cargar
        // slot. Si != nullptr y define script_inicial, gana sobre el
        // m_scriptPath del constructor (ver constructor).
        // La tabla tecla→verbo, sólo para pasársela a las VN que este menú
        // lanza. Este state no la consume (nav C++ propia, en retirada).
        eclips::InputManager* m_input = nullptr;   // `CP9b`: ver el ctor
        eclips::LangStore* m_lang = nullptr;       // idiomas CP1: ver el ctor

        const ProjectManifest* m_manifest = nullptr;

        // MetaStore: se propaga a VNScriptState igual que el manifest.
        MetaStore* m_meta = nullptr;

        // SeenStore: se propaga a VNScriptState igual que meta.
        SeenStore* m_seen = nullptr;

        std::vector<MenuItem> m_items;
        int            m_cursor = 0;
        bool           m_hasSaves = false;
        int            m_pendingLoadSlot = -1;

        // ── Render ────────────────────────────────────────────────────────────

        Texture        m_bgTex;
        TextRenderer   m_titleFont;
        TextRenderer   m_menuFont;
        TextRenderer   m_smallFont;
        bool           m_hasTitle = false;
        bool           m_hasMenu  = false;
        bool           m_hasSmall = false;

        float          m_fadeAlpha  = 1.0f;
        f64            m_time       = 0.0;  // Para animaciones
        float          m_menuStartY = 0.0f;
        float          m_itemSpacing = 56.0f;

        int            m_winW = 1280;
        int            m_winH = 720;
    };

} // namespace eclips
