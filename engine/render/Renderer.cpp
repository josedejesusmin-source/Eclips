// ============================================================================
// ECLIPS ENGINE - Renderer (Implementación)
// ============================================================================
// CP1: ownership de contexto GL, GLAD y SpriteBatch. Rollback solo de lo
// adquirido aqui; nunca DestroyWindow.
// ============================================================================

// glad PRIMERO — antes de cualquier header que pueda tirar de SDL_opengl.h
#include <glad/glad.h>

#include <SDL.h>

#include "Renderer.h"
#include "SpriteBatch.h"
#include "Draw2D.h"

#include <iostream>

namespace eclips
{
    Renderer::Renderer() = default;

    Renderer::~Renderer()
    {
        if (!m_shutdownDone)
            shutdown();
    }

    void Renderer::applyCompat31Attributes()
    {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    }

    void Renderer::applyES20Attributes()
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }

    SpriteBatch* Renderer::batch()
    {
        return m_batch.get();
    }

    bool Renderer::init(SDL_Window* window)
    {
        if (!window)
        {
            std::cerr << "[Eclips] Renderer::init: window nulo." << std::endl;
            return false;
        }

        // Re-init no soportado en CP1.
        if (m_context || m_batch)
        {
            std::cerr << "[Eclips] Renderer::init: ya inicializado." << std::endl;
            return false;
        }

        m_shutdownDone = false;

        m_context = SDL_GL_CreateContext(window);
        if (!m_context)
        {
            std::cerr << "[Eclips] Error al crear contexto OpenGL: "
                << SDL_GetError() << std::endl;
            return false;
        }

        // CRITICO: glad inmediatamente despues del contexto, antes de
        // cualquier llamada GL (incluso glGetString).
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        {
            std::cerr << "[Eclips] Error: gladLoadGLLoader fallo. "
                "No se pueden cargar funciones GL modernas." << std::endl;
            SDL_GL_DeleteContext(m_context);
            m_context = nullptr;
            return false;
        }
        std::cout << "[Eclips] glad cargado OK (GL "
            << GLVersion.major << "." << GLVersion.minor << ")" << std::endl;

        std::cout << "[Eclips] OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "[Eclips] OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "[Eclips] OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

        m_batch = std::make_unique<SpriteBatch>();
        if (!m_batch->init())
        {
            std::cerr << "[Eclips] Error: SpriteBatch::init fallo." << std::endl;
            m_batch.reset();
            SDL_GL_DeleteContext(m_context);
            m_context = nullptr;
            return false;
        }

        // Publicar g_batch solo tras batch valido.
        draw2d::setSpriteBatch(m_batch.get());
        return true;
    }

    void Renderer::shutdown()
    {
        if (m_shutdownDone)
            return;
        m_shutdownDone = true;

        // Desinyectar ANTES de destruir el batch.
        draw2d::setSpriteBatch(nullptr);

        if (m_batch)
        {
            m_batch->shutdown();
            m_batch.reset();
        }

        if (m_context)
        {
            SDL_GL_DeleteContext(m_context);
            m_context = nullptr;
        }
    }

    void Renderer::beginFrame()
    {
        // Delimitador de frame presentado. Sin clear, sin setupViewport.
    }

    void Renderer::endFrame()
    {
        if (m_batch)
            m_batch->flushPendingForFrame();
    }

} // namespace eclips
