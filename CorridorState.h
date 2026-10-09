// ============================================================================
// CorridorState.h — Pasillo 2D (movimiento izquierda/derecha únicamente)
// ============================================================================
//
// Estado de gameplay minimalista para conectar escenas VN con batalla.
// El jugador camina por un pasillo de izquierda a derecha hasta encontrar
// un NPC al final. Al interactuar (E), regresa al VNScriptState que lo llamó.
//
// ── Diseño de pantalla (1920×1080) ────────────────────────────────────────
//
//   ┌──────────────────────────────────────────────────────────────────────┐
//   │                                                                      │
//   │   fondo: imagen o gradiente oscuro                                   │
//   │                                                                      │
//   │                                                                      │
//   │  [Player]──────────────────────────────────────────────────[NPC]    │
//   │  ───────────────────── SUELO ─────────────────────────────────────  │
//   │                                                                      │
//   │  ← A/Flecha Izq     Flecha Der/D →           [E] Hablar             │
//   └──────────────────────────────────────────────────────────────────────┘
//
// ── Uso desde VNScriptState (EclipScript) ────────────────────────────────
//
//   ir_a_pasillo                                         → sin assets
//   ir_a_pasillo "assets/bg/pasillo.png"                 → con fondo
//   ir_a_pasillo "bg.png" "npc.png" "Estrella"           → completo
//   go_to_corridor "bg.png" "npc.png" "Estrella"         → inglés
//
//   El VNScriptState pausa aquí y reanuda automáticamente cuando
//   el jugador interactúa con el NPC (presiona E o Enter).
//
// ── Uso directo desde C++ ────────────────────────────────────────────────
//
//   auto corridor = std::make_unique<CorridorState>(
//       window, glCtx, &stateStack,
//       [this]() { m_cmdIndex++; } // callback al interactuar
//   );
//   corridor->m_bgPath   = "assets/bg/pasillo.png";
//   corridor->m_npcPath  = "assets/characters/estrella_normal.png";
//   corridor->m_npcName  = "Estrella";
//   stateStack.push(std::move(corridor));
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Types.h"
#include "engine/core/Settings.h"
#include "engine/render/Texture.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/audio/AudioManager.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <cmath>
#include <string>
#include <functional>
#include <iostream>

namespace eclips
{

class CorridorState : public GameState
{
public:

    // ── Constructor ───────────────────────────────────────────────────────────

    CorridorState(SDL_Window*            window,
                  SDL_GLContext          glContext,
                  StateStack*            stateStack,
                  FontSourceStore*       fontSources,
                  std::function<void()>  onInteract = nullptr)
        : m_window(window)
        , m_glContext(glContext)
        , m_stateStack(stateStack)
        , m_fontSources(fontSources)
        , m_onInteract(onInteract)
    {
        m_font.setSourceStore(m_fontSources);
    }

    // ── Configuración pública (setear antes del push) ─────────────────────────

    std::string m_bgPath;           // ruta al fondo del pasillo (opcional)
    std::string m_npcPath;          // ruta al sprite del NPC (opcional)
    std::string m_npcName = "???";  // nombre mostrado en el prompt de interacción

    // ── Ciclo de vida ─────────────────────────────────────────────────────────

    void onEnter() override
    {
        std::cout << "[Corredor] =================================" << std::endl;
        std::cout << "[Corredor] Pasillo iniciado."                << std::endl;

        // Fuente
        m_hasFont = m_font.init(Settings::defaultFontPath(), 26.0f);

        // Assets opcionales
        if (!m_bgPath.empty())
            m_hasBg = m_bgTex.load(m_bgPath);

        if (!m_npcPath.empty())
            m_hasNpc = m_npcTex.load(m_npcPath);

        // Jugador comienza en la izquierda
        m_playerX = PLAYER_START_X;
        m_playerY = FLOOR_Y - PLAYER_H;

        m_fadeInAlpha  = 1.0f;
        m_fadeOutAlpha = 0.0f;

        SDL_GetWindowSize(m_window, &m_windowW, &m_windowH);

        std::cout << "[Corredor] NPC: '" << m_npcName << "' en x=" << NPC_X << std::endl;
    }

    void onExit() override
    {
        std::cout << "[Corredor] Saliendo del pasillo." << std::endl;
    }

    void onPause() override {}
    void onResume() override {}

    // ── Update ────────────────────────────────────────────────────────────────

    void update(f64 dt) override
    {
        SDL_GetWindowSize(m_window, &m_windowW, &m_windowH); // ← esta línea
        m_totalTime += dt;

        // ── Fade in ──
        if (m_fadeInAlpha > 0.0f)
        {
            m_fadeInAlpha -= 2.5f * (f32)dt;
            if (m_fadeInAlpha < 0.0f) m_fadeInAlpha = 0.0f;
        }

        // ── Fade out → interacción confirmada ──
        if (m_exiting)
        {
            m_fadeOutAlpha += 2.5f * (f32)dt;
            if (m_fadeOutAlpha >= 1.0f)
            {
                // Notificar al VNScriptState y salir
                if (m_onInteract) m_onInteract();
                m_stateStack->requestPop();
                return; // this destruido, no tocar nada más
            }
            return; // no procesar input mientras sale
        }

        // ── Movimiento del jugador ──
        updatePlayer((f32)dt);

        // ── Proximidad al NPC ──
        f32 playerCX = m_playerX + PLAYER_W * 0.5f;
        f32 npcCX    = NPC_X + NPC_W * 0.5f;
        m_nearNPC = std::abs(playerCX - npcCX) < INTERACT_RANGE;
    }

    // ── Render ────────────────────────────────────────────────────────────────

    void render(f64 alpha) override
    {
        const f32 VW = (f32)viewport::width();
        const f32 VH = (f32)viewport::height();

        draw2d::setupViewport(m_windowW, m_windowH,
                              viewport::width(), viewport::height());
        draw2d::setupProjection(VW, VH);

        // ── 1. Fondo ──
        if (m_hasBg)
        {
            draw2d::drawTexture(m_bgTex, 0, 0, VW, VH);
        }
        else
        {
            // Fallback: gradiente de corredor oscuro (simulado con dos rects)
            draw2d::drawRect(0, 0, VW, VH * 0.65f, 0.06f, 0.06f, 0.10f);
            draw2d::drawRect(0, VH * 0.65f, VW, VH * 0.35f, 0.10f, 0.09f, 0.07f);
        }

        // ── 2. Suelo ──
        draw2d::drawRect(0, FLOOR_Y, VW, 8.0f, 0.5f, 0.45f, 0.35f, 0.9f);
        // Sombra del suelo
        draw2d::drawRect(0, FLOOR_Y + 8.0f, VW, 40.0f, 0.0f, 0.0f, 0.0f, 0.4f);

        // ── 3. NPC ──
        renderNPC();

        // ── 4. Jugador ──
        renderPlayer();

        // ── 5. Prompt de interacción ──
        if (m_nearNPC && m_hasFont && !m_exiting)
            renderInteractPrompt();

        // ── 6. Hint de movimiento (solo al principio) ──
        if (m_totalTime < 4.0 && m_hasFont)
            renderMovementHint();

        // ── 7. Fades ──
        if (m_fadeInAlpha > 0.0f)
            draw2d::drawRect(0, 0, VW, VH, 0, 0, 0, m_fadeInAlpha);
        if (m_fadeOutAlpha > 0.0f)
            draw2d::drawRect(0, 0, VW, VH, 0, 0, 0, m_fadeOutAlpha);
    }

    // ── Eventos ───────────────────────────────────────────────────────────────

    bool handleEvent(const void* ev) override
    {
        const SDL_Event* e = static_cast<const SDL_Event*>(ev);
        if (e->type == SDL_KEYDOWN)
        {
            SDL_Keycode k = e->key.keysym.sym;

            // Interactuar con NPC
            if ((k == SDLK_e || k == SDLK_RETURN || k == SDLK_SPACE) && m_nearNPC)
            {
                if (!m_exiting)
                {
                    std::cout << "[Corredor] Interaccion con NPC: " << m_npcName << std::endl;
                    m_exiting = true;
                }
                return true;
            }

            // ESC: salir sin interactuar (solo debug)
            if (k == SDLK_ESCAPE)
            {
                m_stateStack->requestPop();
                return true;
            }
        }
        return false;
    }

private:

    // ============================================================================
    // Constantes de layout
    // ============================================================================

    static constexpr f32 FLOOR_Y        = 840.0f;  // Y donde está el suelo
    static constexpr f32 PLAYER_W       = 70.0f;
    static constexpr f32 PLAYER_H       = 145.0f;
    static constexpr f32 PLAYER_START_X = 80.0f;
    static constexpr f32 PLAYER_SPEED   = 380.0f;  // px/s

    static constexpr f32 NPC_X          = 1680.0f; // posición izquierda del NPC
    static constexpr f32 NPC_W          = 90.0f;
    static constexpr f32 NPC_H          = 180.0f;

    static constexpr f32 INTERACT_RANGE = 230.0f;  // distancia de interacción

    // ============================================================================
    // Lógica
    // ============================================================================

    void updatePlayer(f32 dt)
    {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        f32 dx = 0.0f;
        if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) dx -= PLAYER_SPEED * dt;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) dx += PLAYER_SPEED * dt;

        m_playerX += dx;
        if (dx != 0.0f) m_facingRight = (dx > 0.0f);

        // Clampar a los límites del pasillo
        const f32 VW = (f32)viewport::width();
        m_playerX = std::max(0.0f, std::min(m_playerX, VW - PLAYER_W - 10.0f));

        // No pasar al NPC
        if (m_playerX + PLAYER_W > NPC_X - 20.0f)
            m_playerX = NPC_X - PLAYER_W - 20.0f;
    }

    // ============================================================================
    // Render helpers
    // ============================================================================

    void renderPlayer()
    {
        f32 py = FLOOR_Y - PLAYER_H;

        // Sombra en el suelo
        draw2d::drawRect(
            m_playerX + PLAYER_W * 0.1f,
            FLOOR_Y,
            PLAYER_W * 0.8f, 10.0f,
            0, 0, 0, 0.3f
        );

        // Cuerpo del jugador (placeholder: rect redondeado simulado)
        // Se reemplazará con sprite en producción
        draw2d::drawRect(m_playerX, py, PLAYER_W, PLAYER_H,
                         0.2f, 0.35f, 0.7f); // azul (color neutro de prota)

        // Detalle: cabeza más clara
        draw2d::drawRect(
            m_playerX + PLAYER_W * 0.15f,
            py,
            PLAYER_W * 0.7f,
            PLAYER_H * 0.28f,
            0.55f, 0.45f, 0.35f
        );

        // Borde sutil
        draw2d::drawRectBorder(m_playerX, py, PLAYER_W, PLAYER_H,
                               0.4f, 0.6f, 1.0f, 0.5f, 1.5f);
    }

    void renderNPC()
    {
        f32 ny = FLOOR_Y - NPC_H;

        // Sombra
        draw2d::drawRect(
            NPC_X + NPC_W * 0.05f,
            FLOOR_Y,
            NPC_W * 0.9f, 12.0f,
            0, 0, 0, 0.35f
        );

        if (m_hasNpc)
        {
            // Sprite del personaje
            // Ajustar altura para sprites DDLC-style (más altos)
            f32 spriteH = NPC_H * 3.5f; // escalar para sprites de cuerpo completo
            f32 spriteW = NPC_W * 3.5f;
            f32 spriteX = NPC_X - (spriteW - NPC_W) * 0.5f;
            f32 spriteY = FLOOR_Y - spriteH;
            draw2d::drawTexture(m_npcTex, spriteX, spriteY, spriteW, spriteH);
        }
        else
        {
            // Placeholder: rect color rosa/salmon (NPC genérico)
            draw2d::drawRect(NPC_X, ny, NPC_W, NPC_H, 0.9f, 0.45f, 0.55f);

            // Cabeza
            draw2d::drawRect(
                NPC_X + NPC_W * 0.15f,
                ny,
                NPC_W * 0.7f,
                NPC_H * 0.3f,
                0.95f, 0.75f, 0.60f
            );

            draw2d::drawRectBorder(NPC_X, ny, NPC_W, NPC_H,
                                   1.0f, 0.6f, 0.7f, 0.6f, 1.5f);
        }

        // Nombre del NPC flotante (pulsa levemente)
        if (m_hasFont)
        {
            f32 pulse = (f32)std::sin(m_totalTime * 2.5) * 0.08f + 0.85f;
            f32 tw = m_font.measureWidth(m_npcName);
            f32 tx = NPC_X + NPC_W * 0.5f - tw * 0.5f;
            f32 ty = FLOOR_Y - NPC_H - 45.0f;

            // Fondo del nombre
            draw2d::drawRect(tx - 10, ty - 3, tw + 20, 34,
                             0, 0, 0, 0.5f);
            m_font.drawText(m_npcName, tx, ty, 1.0f, 0.85f, 0.9f, pulse);
        }
    }

    void renderInteractPrompt()
    {
        // Prompt "[E] Hablar" sobre el NPC
        std::string prompt = "[E] Hablar";
        f32 tw = m_font.measureWidth(prompt);
        f32 tx = NPC_X + NPC_W * 0.5f - tw * 0.5f;
        f32 ty = FLOOR_Y - NPC_H - 90.0f;

        f32 blink = (f32)std::sin(m_totalTime * 5.0) * 0.2f + 0.8f;

        draw2d::drawRect(tx - 14, ty - 4, tw + 28, 38, 0, 0, 0, 0.65f);
        draw2d::drawRectBorder(tx - 14, ty - 4, tw + 28, 38,
                               1.0f, 0.9f, 0.3f, 0.75f, 1.5f);
        m_font.drawText(prompt, tx, ty, 1.0f, 1.0f, 0.35f, blink);
    }

    void renderMovementHint()
    {
        // Desvanece después de 3 segundos
        f32 alpha = 1.0f - (f32)(m_totalTime / 3.5);
        if (alpha <= 0.0f) return;

        std::string hint = "← A / D →   Camina por el pasillo";
        f32 tw = m_font.measureWidth(hint);
        f32 tx = (f32)viewport::width() * 0.5f - tw * 0.5f;
        f32 ty = (f32)viewport::height() - 80.0f;

        draw2d::drawRect(tx - 16, ty - 4, tw + 32, 36, 0, 0, 0, 0.4f * alpha);
        m_font.drawText(hint, tx, ty, 0.8f, 0.8f, 0.8f, 0.7f * alpha);
    }

    // ============================================================================
    // Miembros
    // ============================================================================

    SDL_Window*   m_window     = nullptr;
    SDL_GLContext m_glContext  = nullptr;
    StateStack*   m_stateStack = nullptr;
    FontSourceStore* m_fontSources = nullptr;
    std::function<void()> m_onInteract;
    int m_windowW = 0, m_windowH = 0;

    TextRenderer m_font;
    bool m_hasFont = false;

    Texture m_bgTex;
    Texture m_npcTex;
    bool m_hasBg  = false;
    bool m_hasNpc = false;

    // Estado del jugador
    f32 m_playerX    = PLAYER_START_X;
    f32 m_playerY    = 0.0f;
    bool m_facingRight = true;

    // Interacción
    bool m_nearNPC = false;
    bool m_exiting = false;

    // Tiempo
    f64 m_totalTime = 0.0;

    // Transiciones
    f32 m_fadeInAlpha  = 1.0f;
    f32 m_fadeOutAlpha = 0.0f;
};

} // namespace eclips
