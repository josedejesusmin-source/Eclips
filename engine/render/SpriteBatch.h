// ============================================================================
// ECLIPS ENGINE - SpriteBatch (Render Core Fase 1)
// ============================================================================
// Agrupa quads texturados en un solo VBO y los manda a la GPU en
// pocas draw calls. Reemplaza el immediate mode (glBegin/glEnd) que
// vivia en Draw2D para todo lo que sea textura.
//
// Decisiones de diseno (ver HORIZONTES_ECLIPS, seccion Render Core
// Fase 1, 11 decisiones):
//   - Vertex layout: pos(2f) + uv(2f) + color(4f) = 32 bytes.
//   - Buffer fijo: MAX_QUADS = 1000. NO crece dinamicamente.
//   - Z-order: FIFO. SceneLayer.h sigue siendo la capa de layering
//     por encima del batch.
//   - Shader unico para sprites + texto (texto = sprite con atlas
//     de glifos). Inline en el .cpp como const char*.
//   - GLSL 330 core. Convive con codigo legacy (glad compatibility
//     profile).
//   - Auto-flush por cambio de textura o buffer lleno.
//   - Blend mode fijo: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA.
//
// Uso tipico (futuro, via wrappers en Draw2D.h):
//   batch.begin();
//   batch.draw(texID, x, y, w, h, 0,0, 1,1, 1,1,1,1);
//   batch.draw(texID, x, y, w, h, 0,0, 1,1, 1,1,1,1);
//   batch.end();
//
// La proyeccion se setea una sola vez en init() a VIRTUAL_WIDTH x
// VIRTUAL_HEIGHT. Cuando llegue paso 4 (drawTexture migration) y
// haya que renderizar dentro de un FBO con coords distintas
// (VNScriptState compositing), se anadira setProjection() o similar.
// Decision diferida explicitamente.
// ============================================================================

#pragma once

#include "../core/Types.h"

namespace eclips
{
    // ── Limite de quads por batch ───────────────────────────────────────
    // 1000 quads cubre escenas reales con margen 10x (la peor escena VN
    // medida tiene ~10 quads; bullet hell denso teorico ~900). NO crece
    // dinamicamente — si se llena, se auto-flushea y se sigue. Justo
    // como nos dicta el principio de frugalidad (§7.18 PATRONES).
    //
    // Memoria estatica:
    //   VBO: 1000 * 4 verts * 32 bytes = 128 KB
    //   EBO: 1000 * 6 indices * 2 bytes (u16) = 12 KB
    // Total: 140 KB en GPU. Despreciable.
    constexpr u32 MAX_QUADS = 1000;

    // ── Layout del vertex (32 bytes) ─────────────────────────────────────
    // Sigue el orden declarado en HORIZONTES. No reordenar sin
    // actualizar tambien el shader inline en el .cpp.
    struct SpriteVertex
    {
        f32 x, y;        // pos en virtual coords (1920x1080 por defecto)
        f32 u, v;        // UV [0..1]. V con stb-flip (V=0 abajo).
        f32 r, g, b, a;  // tint multiplicativo
    };

    // ── SpriteBatch ─────────────────────────────────────────────────────
    class SpriteBatch
    {
    public:
        // Inicializa VAO, VBO, EBO, compila shader. Setea projection
        // a VIRTUAL_WIDTH x VIRTUAL_HEIGHT una vez (top-down ortho).
        // Retorna false si shader no compila o GL falla.
        bool init();

        // Libera todos los recursos GL.
        void shutdown();

        // Comienza un batch. Bindea shader, VAO, blend func.
        // Resetea contador interno de quads pendientes.
        void begin();

        // Cierra el batch. Flush de quads pendientes + unbind.
        void end();

        // Empuja un quad al buffer. Auto-flush si:
        //   - se alcanza MAX_QUADS
        //   - textureID cambia respecto al quad anterior
        // Sistema de coordenadas: top-down. (x,y) es esquina sup-izq.
        void draw(u32 textureID,
            f32 x, f32 y, f32 w, f32 h,
            f32 u0, f32 v0, f32 u1, f32 v1,
            f32 r, f32 g, f32 b, f32 a);

        // Flush manual. Caller normal NO llama esto — la auto-flush
        // por cambio de textura es suficiente. Util si en algun caso
        // hay que separar artificialmente dos draws con misma textura
        // (cambio de capa logica, override de tint, etc.).
        void flush();

        // CP1: vaciar quads pendientes al cerrar un frame presentado
        // (Renderer::endFrame). Si m_quadCount==0 → no-op GL (sin bind).
        // Si hay pendientes: reaplicar estado de begin(), flush(), unbind
        // como end(). NO llama begin() (borraría los quads).
        void flushPendingForFrame();

        // Cambia la proyeccion ortografica del shader. Por defecto en init()
        // se setea a (0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, 0) — top-down ortho
        // plana. Llamar setProjection() para casos especiales:
        //   - Screen shake (offset xy en el rectangulo de proyeccion).
        //   - Render dentro de un FBO con coords distintas (paso 4 futuro).
        // Convencion identica a glOrtho: top suele ser < bottom en top-down.
        // El cambio toma efecto en el siguiente begin() o draw() del batch.
        void setProjection(f32 left, f32 right, f32 bottom, f32 top);

        void getProjection(f32& left, f32& right, f32& bottom, f32& top) const
        {
            left = m_projLeft; right = m_projRight;
            bottom = m_projBottom; top = m_projTop;
        }

        // ── Stats (para profiling y debug) ──────────────────────────────
        // Se resetan en begin(). Permiten medir cuanto se ahorra
        // vs immediate mode en cada checkpoint del plan.
        u32 getQuadsDrawn()    const { return m_totalQuads; }
        u32 getDrawCallCount() const { return m_totalFlushes; }

        // ── Stats frame-level (termometro CP3) ──────────────────────────
        // Los de arriba se resetean en cada begin(), y Draw2D abre/cierra
        // el batch N veces por frame → a nivel frame nadie podia leerlos
        // (§7.118: existian sin caller). Estos acumulan a traves de los
        // begin() y solo los resetea el Engine, una vez por frame.
        u32 getFrameQuads()    const { return m_frameQuads; }
        u32 getFrameFlushes()  const { return m_frameFlushes; }
        void resetFrameStats() { m_frameQuads = 0; m_frameFlushes = 0; }

        // ── Textura blanca 1x1 (Camino A — Render Core pre-Fase 2) ─────
        // Quad tinted sobre esta textura = primitiva solida. Permite que
        // drawRect / drawRectBorder pasen por el batch sin shader nuevo.
        // ID estable tras init(); usar para enrutar primitivas al batch.
        u32 getWhiteTexture() const { return m_whiteTexture; }

    private:
        // ── Handles GL ──────────────────────────────────────────────────
        u32 m_vao = 0;
        u32 m_vbo = 0;
        u32 m_ebo = 0;
        u32 m_shader = 0;
        i32 m_uProjectionLoc = -1;  // uniform location, cacheada en init()
        i32 m_uTextureLoc = -1;

        // ── Textura blanca 1x1 para primitivas tinted (Camino A) ────────
        // Inicializada en init(), destruida en shutdown(). Permite que
        // drawRect / drawRectBorder se enruten por este batch usando un
        // quad blanco con tint = color de la primitiva (shader es
        // texColor * vColor; (1,1,1,1) * color = color).
        u32 m_whiteTexture = 0;

        // ── Buffer en CPU ───────────────────────────────────────────────
        // Se llena en draw() y se sube al VBO en flush().
        // Tamano fijo. Si fuera vector<> habria realloc → stalls.
        SpriteVertex m_vertices[MAX_QUADS * 4];
        u32 m_quadCount = 0;

        // ── Estado actual del batch ─────────────────────────────────────
        u32 m_currentTexture = 0;  // textura del batch activo (0 = ninguno)

        // ── Stats acumulativas del begin/end actual ─────────────────────
        u32 m_totalQuads = 0;
        u32 m_totalFlushes = 0;

        // ── Stats frame-level (CP3): sobreviven begin(), reset del Engine ──
        u32 m_frameQuads = 0;
        u32 m_frameFlushes = 0;

        f32 m_projLeft = 0.0f, m_projRight = 0.0f;
        f32 m_projBottom = 0.0f, m_projTop = 0.0f;
    };

} // namespace eclips