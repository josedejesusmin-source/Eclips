// ============================================================================
// Vignette.h — Viñeta radial sin shaders
// ============================================================================
//
// Oscurece los bordes de la pantalla con un gradiente radial suave.
// No requiere shaders: usa GL_TRIANGLE_STRIP con colores por vértice
// (centro transparente → borde oscuro).
//
// ── Cómo funciona ─────────────────────────────────────────────────────────
//
//   Se dibuja un "anillo" de triángulos:
//     · Radio interior (innerRadius): completamente transparente
//     · Radio exterior (>= diagonal de pantalla): completamente opaco
//
//   Al ser el radio exterior mayor que la diagonal de la pantalla,
//   las esquinas quedan cubiertas correctamente. El degradado entre
//   los dos radios crea el efecto de viñeta.
//
//      +────────────────────────+
//      │░░░░░░░░░░░░░░░░░░░░░░│  ← borde oscuro (radio exterior)
//      │░░░░░░+──────+░░░░░░░│
//      │░░░░░│ zona  │░░░░░░░│  ← zona limpia (radio interior)
//      │░░░░░│ clara │░░░░░░░│
//      │░░░░░+──────+░░░░░░░│
//      │░░░░░░░░░░░░░░░░░░░░░░│
//      +────────────────────────+
//
// ── Uso básico ────────────────────────────────────────────────────────────
//
//   Vignette m_vignette;  // miembro de tu GameState
//
//   // En update():
//   m_vignette.update((f32)dt);
//
//   // En render(), DESPUÉS de la escena, ANTES de la UI/diálogo:
//   m_vignette.draw(VW, VH);
//
//   // Para activar desde código:
//   m_vignette.setInstant(0.75f);           // instantáneo
//   m_vignette.setTarget(0.75f, 2.0f);      // fade de 2 segundos
//   m_vignette.setTarget(0.0f, 1.5f);       // desaparecer en 1.5s
//
// ── Uso desde EclipScript ─────────────────────────────────────────────────
//
//   vigneta 0.8           # activar al 80% instantáneo
//   vigneta 0.8 2.0       # fade a 80% en 2 segundos
//   vigneta 0             # apagar instantáneamente
//   vigneta 0 1.5         # desaparecer en 1.5 segundos
//   vignette 0.6 3.0      # inglés: fade a 60% en 3 segundos
//
// ── Parámetros ────────────────────────────────────────────────────────────
//
//   intensity (0.0–1.0) : qué tan oscuros los bordes
//     0.0 = invisible        → sin efecto
//     0.4 = suave            → monólogo interno, misterio
//     0.7 = intenso          → tensión, horror
//     0.9 = muy marcado      → escenas de muerte/crisis
//
//   innerRadius (0.0–1.0): fracción de la diagonal donde empieza el degradado
//     0.35 = viñeta muy cerrada (angustiante)
//     0.55 = default cinematográfico
//     0.75 = muy suave, apenas perceptible
//
//   color (r,g,b): color de los bordes
//     (0,0,0) = negro  ← default, el más natural para VN
//     (0.2,0,0.1) = rojo oscuro, para escenas de sangre/peligro
//     (0,0,0.2) = azul oscuro, para sueños/nostalgia
//
// ============================================================================

#pragma once

#include "engine/core/Types.h"

#include <SDL_opengl.h>
#include <cmath>
#include <algorithm>

namespace eclips
{

class Vignette
{
public:

    // ── Propiedades ─────────────────────────────────────────────────────────

    f32 m_innerRadius = 0.55f;  // fracción de la diagonal (zona central limpia)
    f32 m_colorR = 0.0f;        // color del borde (default: negro)
    f32 m_colorG = 0.0f;
    f32 m_colorB = 0.0f;

    // ── API pública ─────────────────────────────────────────────────────────

    // Fijar intensidad instantáneamente (sin transición)
    void setInstant(f32 intensity)
    {
        m_intensity       = std::max(0.0f, std::min(1.0f, intensity));
        m_targetIntensity = m_intensity;
        m_transitionSpeed = 0.0f;
    }

    // Iniciar transición suave hacia una intensidad objetivo
    //   intensity : valor objetivo (0.0–1.0)
    //   duration  : segundos aproximados para la transición completa (de 0 a 1)
    void setTarget(f32 intensity, f32 duration)
    {
        m_targetIntensity = std::max(0.0f, std::min(1.0f, intensity));
        // Velocidad = 1 unidad / duration segundos
        // Si duration = 2.0, va de 0→1 en 2s, de 0→0.8 en 1.6s
        m_transitionSpeed = (duration > 0.001f) ? (1.0f / duration) : 999.0f;
    }

    // Cambiar color del borde (default negro, normalmente no necesitas cambiar esto)
    void setColor(f32 r, f32 g, f32 b)
    {
        m_colorR = r;
        m_colorG = g;
        m_colorB = b;
    }

    // Acceso de solo lectura a intensidad actual
    f32 intensity() const { return m_intensity; }

    // ── Ciclo de vida ────────────────────────────────────────────────────────

    // Llamar desde update() del estado padre
    void update(f32 dt)
    {
        if (m_transitionSpeed <= 0.0f) return;
        if (std::abs(m_intensity - m_targetIntensity) < 0.001f)
        {
            m_intensity       = m_targetIntensity;
            m_transitionSpeed = 0.0f;
            return;
        }

        f32 dir  = (m_targetIntensity > m_intensity) ? 1.0f : -1.0f;
        f32 step = m_transitionSpeed * dt;

        m_intensity += dir * step;

        // Clampar al objetivo para no pasarse
        if (dir > 0.0f && m_intensity > m_targetIntensity) m_intensity = m_targetIntensity;
        if (dir < 0.0f && m_intensity < m_targetIntensity) m_intensity = m_targetIntensity;
    }

    // Dibujar la viñeta.
    //   VW, VH : resolución virtual del canvas (1920, 1080 en Eclips)
    //   Llamar en render() DESPUÉS de la escena y personajes, ANTES de la UI.
    void draw(f32 VW, f32 VH) const
    {
        if (m_intensity <= 0.001f) return;

        const f32 cx = VW * 0.5f;
        const f32 cy = VH * 0.5f;

        // Radio exterior: diagonal completa × margen (cubre las esquinas)
        const f32 diagonal = std::sqrt(VW * VW + VH * VH) * 0.5f * 1.06f;

        // Radio interior: donde empieza el oscurecimiento
        const f32 innerR = diagonal * m_innerRadius;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // GL_TRIANGLE_STRIP: cada par de vértices forma un quad del anillo
        // Alternamos: vértice interior (transparente) → vértice exterior (oscuro)
        constexpr int SEGMENTS = 64;
        constexpr f32 TWO_PI   = 6.28318530718f;

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= SEGMENTS; i++)
        {
            f32 angle = (f32)i * TWO_PI / (f32)SEGMENTS;
            f32 ca    = std::cos(angle);
            f32 sa    = std::sin(angle);

            // Vértice interior: transparente (centro de la pantalla)
            glColor4f(m_colorR, m_colorG, m_colorB, 0.0f);
            glVertex2f(cx + ca * innerR, cy + sa * innerR);

            // Vértice exterior: opaco según intensidad (bordes de pantalla)
            glColor4f(m_colorR, m_colorG, m_colorB, m_intensity);
            glVertex2f(cx + ca * diagonal, cy + sa * diagonal);
        }
        glEnd();

        // Restaurar color por defecto
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

private:
    f32 m_intensity       = 0.0f;  // intensidad actual (renderizada)
    f32 m_targetIntensity = 0.0f;  // intensidad objetivo (hacia donde interpola)
    f32 m_transitionSpeed = 0.0f;  // unidades/segundo (0 = sin transición activa)
};

} // namespace eclips
