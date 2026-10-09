// ============================================================================
// SpriteSheet.h — Sistema de Sprite Sheets y Animaciones para Eclips Engine
// ============================================================================
//
// Carga un spritesheet (una imagen con todos los frames en grilla) y permite
// definir animaciones con nombre, velocidad y loop.
//
// Pensado para Aseprite: exporta tu sprite como "Sheet Type: By Rows"
// con un tamanio de celda fijo (ej: 128x128 por frame).
//
// Uso basico:
//
//   SpriteSheet Luna;
//   Luna.load("assets/personajes/Luna_sheet.png", 128, 128);
//
//   // Definir animaciones (indices de frames en la grilla, izq→der, arriba→abajo)
//   Luna.addAnimation("idle",       {0, 1, 2, 3},       8.0f, true);
//   Luna.addAnimation("respirar",   {4, 5, 6, 7},       4.0f, true);
//   Luna.addAnimation("bostezo",    {8, 9, 10, 11, 12}, 6.0f, false);
//   Luna.addAnimation("cafe",       {13, 14, 15, 16},   5.0f, false);
//
//   Luna.play("idle");
//
//   // En update:
//   Luna.update(dt);
//
//   // En render:
//   Luna.draw(x, y, drawWidth, drawHeight);
//   Luna.draw(x, y, drawWidth, drawHeight, 0.8f);  // con alpha
//
// Tambien soporta frames individuales (archivos separados):
//
//   SpriteSheet Luna;
//   Luna.loadFrames({
//       "assets/Luna/idle_0.png",
//       "assets/Luna/idle_1.png",
//       "assets/Luna/idle_2.png"
//   });
//   Luna.addAnimation("idle", {0, 1, 2}, 8.0f, true);
//
// Y creacion procedural (sin archivos):
//
//   unsigned char pixels[256*128*4];  // 4 cols x 2 rows de 64x64
//   // ... llenar pixeles ...
//   sheet.loadFromPixels(pixels, 256, 128, 64, 64, 4);
//
// ============================================================================

#pragma once

#include "engine/render/Texture.h"
#include "engine/render/Draw2D.h"
#include "engine/core/Types.h"

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>

namespace eclips
{
    // ── Un frame dentro del spritesheet ─────────────────────────────────────

    struct SpriteFrame
    {
        f32 u0, v0;  // UV superior-izquierda
        f32 u1, v1;  // UV inferior-derecha
        i32 pixelW, pixelH;  // Tamanio del frame en pixeles
    };

    // ── Una animacion con nombre ────────────────────────────────────────────

    struct SpriteAnimation
    {
        std::string name;
        std::vector<int> frameIndices;  // Indices en la lista de frames
        f32  fps       = 8.0f;          // Frames por segundo
        bool loop      = true;          // Repetir al terminar?
        bool finished  = false;         // Se termino? (solo para no-loop)
    };

    // ── SpriteSheet: textura + frames + animaciones ─────────────────────────

    class SpriteSheet
    {
    public:
        SpriteSheet() = default;
        ~SpriteSheet() = default;

        // ── Cargar spritesheet en grilla ────────────────────────────────────
        // filepath: ruta a la imagen (PNG exportado de Aseprite)
        // frameW, frameH: tamanio de cada celda en pixeles
        // Calcula automaticamente filas y columnas.
        bool load(const std::string& filepath, i32 frameW, i32 frameH)
        {
            if (!m_texture.load(filepath))
            {
                std::cerr << "[SpriteSheet] Error al cargar: " << filepath << std::endl;
                return false;
            }

            m_frameW = frameW;
            m_frameH = frameH;
            m_mode = Mode::Sheet;

            calculateFrames(m_texture.getWidth(), m_texture.getHeight(),
                            frameW, frameH);

            std::cout << "[SpriteSheet] Cargado: " << filepath
                      << " (" << m_frames.size() << " frames de "
                      << frameW << "x" << frameH << ")" << std::endl;

            return true;
        }

        // ── Cargar spritesheet desde pixeles crudos ─────────────────────────
        // pixels: datos RGBA/RGB en orden natural (fila 0 = arriba).
        //         Texture::loadFromPixels se encarga del flip para OpenGL.
        // texW, texH: dimensiones totales de la textura en pixeles.
        // frameW, frameH: tamanio de cada celda en la grilla.
        // channels: 3=RGB, 4=RGBA.
        //
        // Util para tests procedurales y texturas generadas en runtime.
        bool loadFromPixels(const unsigned char* pixels,
                            i32 texW, i32 texH,
                            i32 frameW, i32 frameH,
                            i32 channels = 4)
        {
            if (!m_texture.loadFromPixels(pixels, texW, texH, channels))
            {
                std::cerr << "[SpriteSheet] Error al crear desde pixeles" << std::endl;
                return false;
            }

            m_frameW = frameW;
            m_frameH = frameH;
            m_mode = Mode::Sheet;

            calculateFrames(texW, texH, frameW, frameH);

            std::cout << "[SpriteSheet] Creado desde pixeles: "
                      << m_frames.size() << " frames de "
                      << frameW << "x" << frameH << std::endl;

            return true;
        }

        // ── Cargar frames individuales (archivos separados) ─────────────────
        // Menos eficiente que un spritesheet pero mas simple para el artista.
        // Todos los frames deben tener el mismo tamanio.
        bool loadFrames(const std::vector<std::string>& filepaths)
        {
            if (filepaths.empty()) return false;

            m_individualTextures.clear();
            m_individualTextures.resize(filepaths.size());
            m_frames.clear();
            m_mode = Mode::Individual;

            for (size_t i = 0; i < filepaths.size(); ++i)
            {
                if (!m_individualTextures[i].load(filepaths[i]))
                {
                    std::cerr << "[SpriteSheet] Error frame " << i
                              << ": " << filepaths[i] << std::endl;
                    return false;
                }

                SpriteFrame frame;
                frame.u0 = 0.0f; frame.v0 = 1.0f;
                frame.u1 = 1.0f; frame.v1 = 0.0f;
                frame.pixelW = m_individualTextures[i].getWidth();
                frame.pixelH = m_individualTextures[i].getHeight();
                m_frames.push_back(frame);
            }

            m_frameW = m_frames[0].pixelW;
            m_frameH = m_frames[0].pixelH;

            std::cout << "[SpriteSheet] Cargados " << m_frames.size()
                      << " frames individuales" << std::endl;

            return true;
        }

        // ── Definir una animacion ───────────────────────────────────────────
        // name: nombre para referencia ("idle", "respirar", etc.)
        // frameIndices: indices de frames a usar (0-based)
        // fps: velocidad de reproduccion
        // loop: repetir al terminar?
        void addAnimation(const std::string& name,
                          const std::vector<int>& frameIndices,
                          f32 fps = 8.0f,
                          bool loop = true)
        {
            SpriteAnimation anim;
            anim.name = name;
            anim.frameIndices = frameIndices;
            anim.fps = fps;
            anim.loop = loop;
            anim.finished = false;
            m_animations[name] = anim;

            // Si es la primera animacion, hacerla default
            if (m_currentAnim.empty())
            {
                m_currentAnim = name;
            }
        }

        // ── Reproducir una animacion ────────────────────────────────────────
        // Si ya esta reproduciendose, resetea desde el inicio.
        // Si force=false y ya esta activa, no resetea.
        void play(const std::string& name, bool force = false)
        {
            auto it = m_animations.find(name);
            if (it == m_animations.end())
            {
                std::cerr << "[SpriteSheet] Animacion no encontrada: "
                          << name << std::endl;
                return;
            }

            if (m_currentAnim == name && !force)
                return;

            m_currentAnim = name;
            m_currentFrame = 0;
            m_frameTimer = 0.0f;
            it->second.finished = false;
        }

        // ── Actualizar animacion ────────────────────────────────────────────
        void update(f64 dt)
        {
            if (m_currentAnim.empty()) return;

            auto it = m_animations.find(m_currentAnim);
            if (it == m_animations.end()) return;

            SpriteAnimation& anim = it->second;
            if (anim.finished) return;
            if (anim.frameIndices.empty()) return;

            m_frameTimer += static_cast<f32>(dt);
            f32 frameDuration = 1.0f / anim.fps;

            while (m_frameTimer >= frameDuration)
            {
                m_frameTimer -= frameDuration;
                m_currentFrame++;

                if (m_currentFrame >= static_cast<int>(anim.frameIndices.size()))
                {
                    if (anim.loop)
                    {
                        m_currentFrame = 0;
                    }
                    else
                    {
                        m_currentFrame = static_cast<int>(anim.frameIndices.size()) - 1;
                        anim.finished = true;
                    }
                }
            }
        }

        // ── Dibujar el frame actual ─────────────────────────────────────────
        void draw(f32 x, f32 y, f32 w, f32 h, f32 alpha = 1.0f) const
        {
            if (!drawCommon(x, y, w, h, alpha, false)) return;
        }

        // F2.B.3: variante para batch externo (SceneLayer dentro de state begin/end)
        void drawNoBatch(f32 x, f32 y, f32 w, f32 h, f32 alpha = 1.0f) const
        {
            if (!drawCommon(x, y, w, h, alpha, true)) return;
        }

    private:
        bool drawCommon(f32 x, f32 y, f32 w, f32 h, f32 alpha, bool noBatch) const
        {
            int frameIdx = getCurrentFrameIndex();
            if (frameIdx < 0 || frameIdx >= static_cast<int>(m_frames.size()))
                return false;

            const SpriteFrame& frame = m_frames[frameIdx];
            const Texture& tex = getTextureForFrame(frameIdx);

            if (!tex.isLoaded()) return false;

            if (noBatch)
                draw2d::drawSpriteFrame_noBatch(tex, x, y, w, h,
                    frame.u0, frame.v0, frame.u1, frame.v1, alpha);
            else
                draw2d::drawSpriteFrame(tex, x, y, w, h,
                    frame.u0, frame.v0, frame.u1, frame.v1, alpha);
            return true;
        }

    public:

        // ── Dibujar centrado en pantalla (estilo VN) ────────────────────────
        void drawCentered(f32 vw, f32 vh, f32 scale = 1.0f,
                          f32 alpha = 1.0f) const
        {
            f32 tw = static_cast<f32>(m_frameW) * scale;
            f32 th = static_cast<f32>(m_frameH) * scale;

            if (th > vh * 0.85f)
            {
                f32 fitScale = (vh * 0.85f) / th;
                tw *= fitScale;
                th *= fitScale;
            }

            f32 x = (vw - tw) / 2.0f;
            f32 y = vh - th;

            draw(x, y, tw, th, alpha);
        }

        // ── Dibujar en posicion VN (izquierda, centro, derecha) ─────────────
        void drawAtPosition(VNPosition pos, f32 vw, f32 vh,
                            f32 scale = 1.0f, f32 alpha = 1.0f) const
        {
            f32 tw = static_cast<f32>(m_frameW) * scale;
            f32 th = static_cast<f32>(m_frameH) * scale;

            if (th > vh * 0.85f)
            {
                f32 fitScale = (vh * 0.85f) / th;
                tw *= fitScale;
                th *= fitScale;
            }

            f32 x = 0.0f;
            switch (pos)
            {
                case VNPosition::Izquierda: x = vw * 0.15f - tw * 0.5f; break;
                case VNPosition::Centro:    x = (vw - tw) / 2.0f;       break;
                case VNPosition::Derecha:   x = vw * 0.85f - tw * 0.5f; break;
            }

            f32 y = vh - th;
            draw(x, y, tw, th, alpha);
        }

        // ── Consultas ───────────────────────────────────────────────────────

        bool isFinished() const
        {
            if (m_currentAnim.empty()) return true;
            auto it = m_animations.find(m_currentAnim);
            if (it == m_animations.end()) return true;
            return it->second.finished;
        }

        const std::string& currentAnimation() const { return m_currentAnim; }

        bool hasAnimation(const std::string& name) const
        {
            return m_animations.find(name) != m_animations.end();
        }

        int currentFrameNumber() const { return m_currentFrame; }

        int frameWidth() const { return m_frameW; }
        int frameHeight() const { return m_frameH; }
        int frameCount() const { return static_cast<int>(m_frames.size()); }

        bool isLoaded() const
        {
            if (m_mode == Mode::Sheet) return m_texture.isLoaded();
            return !m_individualTextures.empty() && m_individualTextures[0].isLoaded();
        }

        // ── Callback para cuando una animacion termina ──────────────────────
        // Util para: "cuando Luna termine de bostezar, vuelve a idle"
        void setOnFinished(const std::string& nextAnim)
        {
            m_onFinishedAnim = nextAnim;
        }

        // Llamar despues de update() para auto-transicionar
        void checkAutoTransition()
        {
            if (!m_onFinishedAnim.empty() && isFinished())
            {
                play(m_onFinishedAnim, true);
                m_onFinishedAnim.clear();
            }
        }

        // ── Liberar recursos ────────────────────────────────────────────────
        void unload()
        {
            m_texture.unload();
            for (auto& tex : m_individualTextures)
                tex.unload();
            m_individualTextures.clear();
            m_frames.clear();
            m_animations.clear();
            m_currentAnim.clear();
            m_currentFrame = 0;
            m_frameTimer = 0.0f;
        }

    private:
        int getCurrentFrameIndex() const
        {
            if (m_currentAnim.empty()) return 0;
            auto it = m_animations.find(m_currentAnim);
            if (it == m_animations.end()) return 0;

            const auto& indices = it->second.frameIndices;
            if (indices.empty()) return 0;
            if (m_currentFrame < 0 || m_currentFrame >= static_cast<int>(indices.size()))
                return indices[0];

            return indices[m_currentFrame];
        }

        const Texture& getTextureForFrame(int frameIdx) const
        {
            if (m_mode == Mode::Sheet)
                return m_texture;

            if (frameIdx >= 0 && frameIdx < static_cast<int>(m_individualTextures.size()))
                return m_individualTextures[frameIdx];

            return m_texture;  // fallback
        }

        // ── Calcular frames de la grilla (compartido por load y loadFromPixels) ──
        void calculateFrames(i32 texW, i32 texH, i32 frameW, i32 frameH)
        {
            i32 cols = texW / frameW;
            i32 rows = texH / frameH;

            m_frames.clear();

            for (i32 row = 0; row < rows; ++row)
            {
                for (i32 col = 0; col < cols; ++col)
                {
                    SpriteFrame frame;

                    // UV coords normalizadas (0-1)
                    f32 rawU0 = static_cast<f32>(col * frameW) / static_cast<f32>(texW);
                    f32 rawU1 = static_cast<f32>((col + 1) * frameW) / static_cast<f32>(texW);
                    f32 rawV0 = static_cast<f32>(row * frameH) / static_cast<f32>(texH);
                    f32 rawV1 = static_cast<f32>((row + 1) * frameH) / static_cast<f32>(texH);

                    // Invertir V para nuestro sistema (stb flip + top-down ortho)
                    // Texture::loadFromPixels tambien hace flip, asi que esta
                    // inversion aplica identicamente para ambos paths.
                    frame.u0 = rawU0;
                    frame.u1 = rawU1;
                    frame.v0 = 1.0f - rawV0;  // Superior del frame
                    frame.v1 = 1.0f - rawV1;  // Inferior del frame

                    frame.pixelW = frameW;
                    frame.pixelH = frameH;

                    m_frames.push_back(frame);
                }
            }
        }

        enum class Mode { Sheet, Individual };

        Mode    m_mode = Mode::Sheet;
        Texture m_texture;                          // Spritesheet unico
        std::vector<Texture> m_individualTextures;  // Frames individuales

        std::vector<SpriteFrame> m_frames;
        i32 m_frameW = 0, m_frameH = 0;

        std::map<std::string, SpriteAnimation> m_animations;
        std::string m_currentAnim;
        int   m_currentFrame = 0;
        f32   m_frameTimer = 0.0f;

        std::string m_onFinishedAnim;  // Auto-transicionar al terminar
    };

} // namespace eclips
