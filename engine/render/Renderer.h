// ============================================================================
// ECLIPS ENGINE - Renderer
// ============================================================================
// Ownership CP1: contexto GL, GLAD y SpriteBatch. La ventana/SDL, el
// predicado de render, queries, stats y swap siguen en Engine.
// setupViewport es static inline aqui (un solo cuerpo) para que atlas /
// CP0 / sondas no necesiten enlazar Renderer.obj solo por viewport.
// ============================================================================

#pragma once

#include "../core/Types.h"

#include <memory>

// Forward declarations SDL (equivalente a Engine.h) — SDL_opengl.h no
// aporta SDL_Window / SDL_GLContext.
struct SDL_Window;
typedef void* SDL_GLContext;

// Mismas llamadas GL que Draw2D usaba para setupViewport.
#include <SDL_opengl.h>

namespace eclips
{
    class SpriteBatch;  // forward: definicion completa solo en Renderer.cpp

    class Renderer
    {
    public:
        Renderer();
        ~Renderer();  // llama shutdown si hace falta

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        // Atributos GL antes de CreateWindow (Engine crea/reintenta la ventana).
        static void applyCompat31Attributes();
        static void applyES20Attributes();

        // Un solo cuerpo: letterbox/pillarbox + clear opcional.
        // Truncado entero hacia cero; orden GL identico al Draw2D historico.
        static inline void setupViewport(int windowW, int windowH,
                                         int virtualW, int virtualH,
                                         bool limpiar = true)
        {
            f32 targetAspect = static_cast<f32>(virtualW) / static_cast<f32>(virtualH);
            f32 windowAspect = static_cast<f32>(windowW) / static_cast<f32>(windowH);

            int vpX = 0, vpY = 0, vpW = windowW, vpH = windowH;

            if (windowAspect > targetAspect)
            {
                vpW = static_cast<int>(windowH * targetAspect);
                vpX = (windowW - vpW) / 2;
            }
            else
            {
                vpH = static_cast<int>(windowW / targetAspect);
                vpY = (windowH - vpH) / 2;
            }

            if (limpiar)
            {
                glViewport(0, 0, windowW, windowH);
                glScissor(0, 0, windowW, windowH);
                glEnable(GL_SCISSOR_TEST);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            glViewport(vpX, vpY, vpW, vpH);
            glScissor(vpX, vpY, vpW, vpH);
            glEnable(GL_SCISSOR_TEST);
        }

        // Contexto + glad + SpriteBatch. NO crea ni destruye la ventana.
        bool init(SDL_Window* window);
        void shutdown();  // idempotente; no DestroyWindow

        void beginFrame();  // delimitador; sin clear / sin letterbox
        void endFrame();    // flushPendingForFrame del batch

        SDL_GLContext context() const { return m_context; }
        SpriteBatch* batch();  // nullptr si no valido

    private:
        SDL_GLContext m_context = nullptr;
        std::unique_ptr<SpriteBatch> m_batch;
        bool m_shutdownDone = false;
    };

} // namespace eclips
