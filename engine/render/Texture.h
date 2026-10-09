// ============================================================================
// ECLIPS ENGINE - Texture (Carga de imágenes)
// ============================================================================
// Carga archivos de imagen (PNG, JPG, etc.) usando stb_image y los convierte
// en texturas de OpenGL listas para dibujar.
//
// Uso:
//   Texture tex;
//   tex.load("assets/fondo.png");
//   tex.bind();    // Activar para dibujar
//   tex.unbind();  // Desactivar
//   tex.unload();  // Liberar memoria
//
// Tambien soporta creacion desde pixeles crudos en memoria:
//   unsigned char pixels[64*64*4];  // RGBA
//   tex.loadFromPixels(pixels, 64, 64, 4);
// ============================================================================

#pragma once

#include "../core/Types.h"
#include <SDL_opengl.h>
#include <string>
namespace eclips
{
    class Texture
    {
    public:
        Texture() = default;
        ~Texture();

        // Cargar imagen desde archivo (PNG, JPG, BMP, etc.)
        bool load(const std::string& filepath);

        // Crear textura desde pixeles crudos en memoria.
        // data: puntero a pixeles RGBA/RGB (NO se libera, el caller es duenio).
        // width, height: dimensiones en pixeles.
        // channels: 3 = RGB, 4 = RGBA (default).
        // Util para texturas procedurales, test, render targets.
        bool loadFromPixels(const unsigned char* data, i32 width, i32 height,
                            i32 channels = 4);

        // Liberar la textura de la GPU
        void unload();

        // Activar/desactivar esta textura para dibujar
        void bind() const;
        void unbind() const;

        // Intercambiar contenido con otra textura (sin copiar ni destruir)
        void swapWith(Texture& other)
        {
            auto sw = [](auto& a, auto& b){ auto t = a; a = b; b = t; };
            sw(m_textureID, other.m_textureID);
            sw(m_width,     other.m_width);
            sw(m_height,    other.m_height);
            sw(m_channels,  other.m_channels);
        }

        void setFilter(bool nearest) const
        {
            glBindTexture(GL_TEXTURE_2D, m_textureID);
            GLenum f = nearest ? GL_NEAREST : GL_LINEAR;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
        }

        // Información
        i32 getWidth() const { return m_width; }
        i32 getHeight() const { return m_height; }
        u32 getID() const { return m_textureID; }
        bool isLoaded() const { return m_textureID != 0; }

    private:
        u32 m_textureID = 0;  // ID de OpenGL
        i32 m_width = 0;
        i32 m_height = 0;
        i32 m_channels = 0;   // 3 = RGB, 4 = RGBA
    };

} // namespace eclips
