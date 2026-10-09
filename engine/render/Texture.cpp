// ============================================================================
// ECLIPS ENGINE - Texture (Implementación)
// ============================================================================

#include "Texture.h"

#include <SDL_opengl.h>
#include <iostream>
#include <vector>
#include <cstring>

// stb_image para decodificar PNG/JPG/etc.
// La implementación está en third_party/stb_impl.cpp
#include "stb_image.h"

namespace eclips
{
    Texture::~Texture()
    {
        if (m_textureID != 0)
        {
            unload();
        }
    }

    bool Texture::load(const std::string& filepath)
    {
        // Si ya tenía una textura cargada, liberarla primero
        if (m_textureID != 0)
        {
            unload();
        }

        // ── Cargar imagen con stb_image ──
        // stbi_set_flip: las imágenes se cargan de arriba a abajo,
        // pero OpenGL espera de abajo a arriba. Esto lo corrige.
        stbi_set_flip_vertically_on_load(true);

        unsigned char* pixels = stbi_load(
            filepath.c_str(),
            &m_width,
            &m_height,
            &m_channels,
            0  // 0 = usar los canales que tenga la imagen
        );

        if (!pixels)
        {
            std::cerr << "[Eclips] Error al cargar imagen: " << filepath
                      << " - " << stbi_failure_reason() << std::endl;
            return false;
        }

        // ── Crear textura OpenGL ──
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        // Filtrado: NEAREST para pixel art nítido, LINEAR para arte suave
        // Por defecto usamos LINEAR. Para pixel art, el creador puede cambiar.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Wrapping: CLAMP para que no se repita la textura
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Determinar formato según canales
        GLenum format = GL_RGBA;
        if (m_channels == 3) format = GL_RGB;
        else if (m_channels == 1) format = GL_RED;

        // Subir píxeles a la GPU
        glTexImage2D(
            GL_TEXTURE_2D,
            0,              // Mipmap level 0 (base)
            format,         // Formato interno
            m_width,
            m_height,
            0,              // Border (siempre 0)
            format,         // Formato de los datos
            GL_UNSIGNED_BYTE,
            pixels
        );

        // ── Liberar píxeles de la RAM ──
        // Ya están en la GPU, no los necesitamos en RAM
        stbi_image_free(pixels);

        std::cout << "[Eclips] Textura cargada: " << filepath
                  << " (" << m_width << "x" << m_height
                  << ", " << m_channels << " canales)" << std::endl;

        return true;
    }

    bool Texture::loadFromPixels(const unsigned char* data, i32 width, i32 height,
                                 i32 channels)
    {
        if (!data || width <= 0 || height <= 0)
        {
            std::cerr << "[Eclips] loadFromPixels: datos invalidos" << std::endl;
            return false;
        }

        // Liberar textura anterior si existía
        if (m_textureID != 0)
        {
            unload();
        }

        m_width = width;
        m_height = height;
        m_channels = channels;

        // ── Crear textura OpenGL ──
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        // NEAREST para pixeles nitidos (ideal para texturas procedurales/test)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLenum format = GL_RGBA;
        if (channels == 3) format = GL_RGB;
        else if (channels == 1) format = GL_RED;

        // Voltear verticalmente para igualar el comportamiento de stb_image.
        // El caller pasa pixeles en orden natural (fila 0 = arriba de la imagen).
        // OpenGL espera fila 0 = abajo. stb_image hace este flip internamente;
        // aqui lo hacemos manualmente para que los UVs sean consistentes.
        i32 rowBytes = width * channels;
        std::vector<unsigned char> flipped(rowBytes * height);
        for (i32 row = 0; row < height; ++row)
        {
            const unsigned char* src = data + row * rowBytes;
            unsigned char* dst = flipped.data() + (height - 1 - row) * rowBytes;
            std::memcpy(dst, src, rowBytes);
        }

        glTexImage2D(
            GL_TEXTURE_2D, 0, format,
            width, height, 0,
            format, GL_UNSIGNED_BYTE,
            flipped.data()
        );

        std::cout << "[Eclips] Textura creada desde pixeles: "
                  << width << "x" << height
                  << " (" << channels << " canales)" << std::endl;

        return true;
    }

    void Texture::unload()
    {
        if (m_textureID != 0)
        {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
            m_width = 0;
            m_height = 0;
            m_channels = 0;
        }
    }

    void Texture::bind() const
    {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
    }

    void Texture::unbind() const
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace eclips
