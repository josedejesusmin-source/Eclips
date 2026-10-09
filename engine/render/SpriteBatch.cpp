// ============================================================================
// ECLIPS ENGINE - SpriteBatch (Implementacion real - Render Core Fase 1)
// ============================================================================
// Reemplaza el stub del paso 1.0. Implementacion completa del pipeline
// GLSL 330 core con orphaning manual del VBO.
//
// Lifecycle esperado:
//   1. init()      una vez en startup del motor
//   2. begin()     por frame (o por capa de render)
//   3. draw() N veces
//   4. end()       cierra el batch del frame
//   5. shutdown()  una vez en exit
//
// Convencion UV (importante, contraintuitivo):
//   En Eclips V=1 esta arriba, V=0 esta abajo (stb-flip + top-down ortho).
//   Para full texture: caller pasa (u0=0, v0=1, u1=1, v1=0).
//   SpriteSheet ya calcula sus UVs en esta convencion.
//   Documentado en SpriteSheet.h "NOTA UV".
// ============================================================================

#include "SpriteBatch.h"

// `caja-entrada`: el factor de fundido del GRUPO. Header puro (SDL-free,
// GL-free), probado por harness_grupo. Este es UNO de los tres consumidores;
// los otros dos son los prims de glBegin en Draw2D.h. Ver GroupAlpha.h.
#include "../core/GroupAlpha.h"

// glad PRIMERO, antes de cualquier otro header GL.
#include <glad/glad.h>

#include <iostream>
#include <cstddef>  // offsetof

namespace eclips
{

    // ── Shaders inline (GLSL 330 core) ─────────────────────────────────────────
    // Un solo shader para sprites + texto (texto = sprite con atlas de
    // glifos, conceptualmente identico). Decision documentada en HORIZONTES.
    //
    // Nota para paso 5 (texto outlined batched): si el atlas de glifos
    // es single-channel (GL_RED), habra que swizzlear en el fragment shader
    // (vec4(1,1,1, texture(uTexture, vUV).r) * vColor) o cargar el atlas
    // como GL_RGBA con canales redundantes. Por ahora, RGBA full.

    static const char* kVertexShaderSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

void main()
{
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vUV    = aUV;
    vColor = aColor;
}
)GLSL";

    static const char* kFragmentShaderSrc = R"GLSL(
#version 330 core
in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 FragColor;

void main()
{
    vec4 texColor = texture(uTexture, vUV);
    FragColor = texColor * vColor;
}
)GLSL";

    // ── Helpers internos (anonymous namespace) ─────────────────────────────────
    namespace
    {
        u32 compileShader(GLenum type, const char* src)
        {
            u32 shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            i32 ok = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                char log[1024] = {};
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                std::cerr << "[SpriteBatch] Error compilando shader "
                    << (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                    << ":\n" << log << std::endl;
                glDeleteShader(shader);
                return 0;
            }
            return shader;
        }

        u32 linkProgram(u32 vs, u32 fs)
        {
            u32 program = glCreateProgram();
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);

            i32 ok = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                char log[1024] = {};
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                std::cerr << "[SpriteBatch] Error linkeando programa:\n"
                    << log << std::endl;
                glDeleteProgram(program);
                return 0;
            }
            return program;
        }

        // Matriz ortho generica column-major (GL standard).
                // Convencion identica a glOrtho(left, right, bottom, top, -1, 1).
                // Hecho a mano para no traer glm como dependencia (frugalidad).
        void makeOrtho(f32 left, f32 right, f32 bottom, f32 top, f32 out[16])
        {
            for (int i = 0; i < 16; i++) out[i] = 0.0f;
            out[0] = 2.0f / (right - left);
            out[5] = 2.0f / (top - bottom);
            out[10] = -1.0f;
            out[12] = -(right + left) / (right - left);
            out[13] = -(top + bottom) / (top - bottom);
            out[15] = 1.0f;
        }

        // Wrapper top-down: (0, w, h, 0). Y crece hacia abajo.
        // (x=0, y=0) en esquina superior-izquierda — mismo convenio que
        // Draw2D y todo el resto del motor.
        void makeOrthoTopDown(f32 w, f32 h, f32 out[16])
        {
            makeOrtho(0.0f, w, h, 0.0f, out);
        }
    }

    // ── init ───────────────────────────────────────────────────────────────────
    bool SpriteBatch::init()
    {
        // 1. Compilar y linkear shader
        u32 vs = compileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
        u32 fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
        // A2: si uno compila y el otro falla, liberar el hermano exitoso
        // antes de return false (IDs locales; no depender del destructor).
        if (!vs || !fs)
        {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return false;
        }

        m_shader = linkProgram(vs, fs);
        glDeleteShader(vs);  // ya referenciados por el program
        glDeleteShader(fs);
        if (!m_shader) return false;

        // 2. Cachear uniform locations (evita glGetUniformLocation por frame)
        m_uProjectionLoc = glGetUniformLocation(m_shader, "uProjection");
        m_uTextureLoc = glGetUniformLocation(m_shader, "uTexture");

        // 3. Subir proyeccion ortho una sola vez. Si paso 4 (FBO compositing)
        //    requiere proyeccion variable, ahi anadimos setProjection().
        // F2.B.1: guardar valores iniciales para getProjection()
        m_projLeft = 0.0f;
        m_projRight = (f32)constants::VIRTUAL_WIDTH;
        m_projBottom = (f32)constants::VIRTUAL_HEIGHT;
        m_projTop = 0.0f;

        f32 proj[16];
        makeOrtho(m_projLeft, m_projRight, m_projBottom, m_projTop, proj);

        glUseProgram(m_shader);
        glUniformMatrix4fv(m_uProjectionLoc, 1, GL_FALSE, proj);
        glUniform1i(m_uTextureLoc, 0);  // sampler en slot 0
        glUseProgram(0);

        // 4. VAO + VBO + EBO
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        //    VBO: tamano fijo, GL_DYNAMIC_DRAW (orphaning en cada flush)
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
            sizeof(SpriteVertex) * MAX_QUADS * 4,
            nullptr,
            GL_DYNAMIC_DRAW);

        //    Atributo 0: aPos (vec2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            sizeof(SpriteVertex),
            (void*)offsetof(SpriteVertex, x));
        //    Atributo 1: aUV (vec2)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            sizeof(SpriteVertex),
            (void*)offsetof(SpriteVertex, u));
        //    Atributo 2: aColor (vec4)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
            sizeof(SpriteVertex),
            (void*)offsetof(SpriteVertex, r));

        // 5. EBO con indices precalculados para los 2 triangulos por quad.
        //    Quad N usa vertices [4N..4N+3]. Triangulos: (0,1,2) y (2,3,0).
        //    Indices estaticos — nunca cambian, GL_STATIC_DRAW.
        u16 indices[MAX_QUADS * 6];
        for (u32 q = 0; q < MAX_QUADS; q++)
        {
            u32 base = q * 4;
            u32 i = q * 6;
            indices[i + 0] = (u16)(base + 0);
            indices[i + 1] = (u16)(base + 1);
            indices[i + 2] = (u16)(base + 2);
            indices[i + 3] = (u16)(base + 2);
            indices[i + 4] = (u16)(base + 3);
            indices[i + 5] = (u16)(base + 0);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            sizeof(indices), indices, GL_STATIC_DRAW);

        // 6. Unbind defensivo (no afecta a fixed function legacy)
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // 7. Textura blanca 1x1 (Camino A — primitivas tinted via batch).
        //    Un quad sobre esta textura con tint = color de la primitiva
        //    produce un rect solido sin shader nuevo. RGBA 0xFFFFFFFF.
        //    Filtros NEAREST porque 1 texel nunca se interpola.
        glGenTextures(1, &m_whiteTexture);
        glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
        const u8 whitePixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "[SpriteBatch] Inicializado OK. MAX_QUADS=" << MAX_QUADS
            << ", VBO=" << (sizeof(SpriteVertex) * MAX_QUADS * 4 / 1024)
            << " KB, EBO=" << (sizeof(indices) / 1024) << " KB"
            << ", whiteTex=" << m_whiteTexture
            << std::endl;

        return true;
    }

    // ── setProjection ──────────────────────────────────────────────────────────
    void SpriteBatch::setProjection(f32 left, f32 right, f32 bottom, f32 top)
    {
        // Si hay quads pendientes con la projection vieja, flushearlos antes
        // de cambiar la matriz — sino se renderearian con la nueva.
        if (m_quadCount > 0) flush();

        // F2.B.1: guardar para save/restore (getProjection)
        m_projLeft = left; m_projRight = right;
        m_projBottom = bottom; m_projTop = top;

        f32 proj[16];
        makeOrtho(left, right, bottom, top, proj);

        // F2.B.3: no desvincular el shader. Si estamos dentro de un
         // begin/end activo, glUseProgram(0) mataria el shader y los
         // flush posteriores dibujarian sin el. Dejarlo activo es
         // inocuo: begin() lo re-vincula de todos modos, y la
         // fixed-function (vignette/glitch) corre despues de end().
        glUseProgram(m_shader);
        glUniformMatrix4fv(m_uProjectionLoc, 1, GL_FALSE, proj);
    }

    // ── shutdown ───────────────────────────────────────────────────────────────
    void SpriteBatch::shutdown()
    {
        if (m_whiteTexture) { glDeleteTextures(1, &m_whiteTexture); m_whiteTexture = 0; }
        if (m_ebo) { glDeleteBuffers(1, &m_ebo);       m_ebo = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo);       m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao);  m_vao = 0; }
        if (m_shader) { glDeleteProgram(m_shader);        m_shader = 0; }
    }

    // ── begin ──────────────────────────────────────────────────────────────────
    void SpriteBatch::begin()
    {
        m_quadCount = 0;
        m_currentTexture = 0;
        m_totalQuads = 0;
        m_totalFlushes = 0;

        glUseProgram(m_shader);
        glBindVertexArray(m_vao);
        glActiveTexture(GL_TEXTURE0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // ── end ────────────────────────────────────────────────────────────────────
    void SpriteBatch::end()
    {
        flush();
        glBindVertexArray(0);
        glUseProgram(0);
    }

    // ── flushPendingForFrame (CP1 — cierre de frame presentado) ────────────────
    void SpriteBatch::flushPendingForFrame()
    {
        // Sin pendientes: no-op de estado GL (no bind de programa/VAO/blend).
        if (m_quadCount == 0)
            return;

        // Hubo begin() sin end(): el flush vivo no re-bindea programa/VAO/
        // unidad/blend. Reaplicar lo que begin() pone, dibujar, unbind como end.
        glUseProgram(m_shader);
        glBindVertexArray(m_vao);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        flush();
        glBindVertexArray(0);
        glUseProgram(0);
    }

    // ── draw ───────────────────────────────────────────────────────────────────
    void SpriteBatch::draw(u32 textureID,
        f32 x, f32 y, f32 w, f32 h,
        f32 u0, f32 v0, f32 u1, f32 v1,
        f32 r, f32 g, f32 b, f32 a)
    {
        // `caja-entrada` — EL PUNTO UNICO del fundido de grupo. Va ACA y no en
        // los ~40 callers de Draw2D porque este es el cuello por el que pasa
        // todo lo que se dibuja con textura: sprites, rects (textura blanca),
        // 9-slice, y TODO el texto — incluidas las nueve pasadas del borde.
        // En reposo el factor es 1.0 y esto es bit-identico a no estar (ley 1,
        // harness_grupo §1). Lo unico que NO pasa por aca son los dos rounded
        // prims de glBegin, que lo aplican por su cuenta.
        a = draw2d::applyGroupAlpha(a);

        // Auto-flush por cambio de textura (textura=0 = primer draw, no flush)
        if (m_currentTexture != 0 && textureID != m_currentTexture)
            flush();

        // Auto-flush por buffer lleno
        if (m_quadCount == MAX_QUADS)
            flush();

        m_currentTexture = textureID;

        // Push 4 vertices. Orden: sup-izq, sup-der, inf-der, inf-izq.
        // Combina con los indices estaticos del EBO (0,1,2,2,3,0 por quad).
        SpriteVertex* v = &m_vertices[m_quadCount * 4];

        v[0].x = x;     v[0].y = y;
        v[0].u = u0;    v[0].v = v0;
        v[0].r = r;     v[0].g = g;     v[0].b = b;     v[0].a = a;

        v[1].x = x + w;   v[1].y = y;
        v[1].u = u1;    v[1].v = v0;
        v[1].r = r;     v[1].g = g;     v[1].b = b;     v[1].a = a;

        v[2].x = x + w;   v[2].y = y + h;
        v[2].u = u1;    v[2].v = v1;
        v[2].r = r;     v[2].g = g;     v[2].b = b;     v[2].a = a;

        v[3].x = x;     v[3].y = y + h;
        v[3].u = u0;    v[3].v = v1;
        v[3].r = r;     v[3].g = g;     v[3].b = b;     v[3].a = a;

        m_quadCount++;
    }

    // ── flush ──────────────────────────────────────────────────────────────────
    void SpriteBatch::flush()
    {
        if (m_quadCount == 0) return;

        // Orphaning manual: glBufferData(NULL) le dice al driver "abandona
        // el storage anterior, dame uno nuevo". Asi evita stalls por
        // sincronizacion CPU<->GPU cuando el frame N+1 escribe en el mismo
        // buffer que el frame N sigue dibujando. Patron estandar para
        // streaming buffers.
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
            sizeof(SpriteVertex) * MAX_QUADS * 4,
            nullptr,
            GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
            sizeof(SpriteVertex) * m_quadCount * 4,
            m_vertices);

        // Bindea textura del batch actual al slot 0
        glBindTexture(GL_TEXTURE_2D, m_currentTexture);

        // Una sola draw call para todos los quads acumulados
        glDrawElements(GL_TRIANGLES,
            m_quadCount * 6,
            GL_UNSIGNED_SHORT,
            nullptr);

        m_totalQuads += m_quadCount;
        m_totalFlushes += 1;
        m_frameQuads += m_quadCount;   // frame-level (CP3): sobrevive begin()
        m_frameFlushes += 1;
        m_quadCount = 0;
    }

} // namespace eclips