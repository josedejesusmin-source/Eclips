// ============================================================================
// Hotspot.h — Sistema de Hotspots Clickeables para Eclips Engine
// ============================================================================
//
// Zonas invisibles (o visibles en debug) que detectan clicks/taps del mouse
// y disparan acciones. Cada hotspot tiene un nombre, un rectangulo, y un
// callback o identificador de accion.
//
// Uso basico:
//
//   HotspotManager hotspots;
//
//   hotspots.add("carta", 350, 800, 150, 120);
//   hotspots.add("libro", 1400, 750, 160, 130);
//
//   // En handleEvent:
//   if (mouse click) {
//       std::string hit = hotspots.testClick(mouseX, mouseY);
//       if (!hit.empty()) {
//           // "carta" o "libro" fue clickeado
//       }
//   }
//
//   // Para debug: dibujar los rectangulos con nombres
//   hotspots.debugRender(textRenderer);
//
// ============================================================================

#pragma once

#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/core/Types.h"

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdint>

namespace eclips
{
    // ── Un hotspot individual ────────────────────────────────────────────────

    struct Hotspot
    {
        std::string name;       // Identificador ("carta", "libro")
        std::uint64_t scopeOwner = 0; // 0=host; >0=ambito nombrado
        f32 x, y, w, h;        // Rectangulo en coordenadas virtuales
        bool enabled = true;    // Se puede clickear?
        bool hovered = false;   // Mouse encima?
    };

    // ── Manager de hotspots ─────────────────────────────────────────────────

    class HotspotManager
    {
    public:
        HotspotManager() = default;

        // ── Agregar hotspots ────────────────────────────────────────────

        void add(const std::string& name, f32 x, f32 y, f32 w, f32 h,
                 std::uint64_t scopeOwner = 0)
        {
            Hotspot hs;
            hs.name = name;
            hs.scopeOwner = scopeOwner;
            hs.x = x; hs.y = y;
            hs.w = w; hs.h = h;
            hs.enabled = true;
            hs.hovered = false;
            m_hotspots.push_back(hs);

            std::cout << "[Hotspot] Agregado: \"" << name
                      << "\" (" << x << "," << y << " "
                      << w << "x" << h << ")" << std::endl;
        }

        // ── Test de click ───────────────────────────────────────────────

        std::string testClick(f32 mx, f32 my) const
        {
            for (int i = static_cast<int>(m_hotspots.size()) - 1; i >= 0; --i)
            {
                const Hotspot& hs = m_hotspots[i];
                if (!hs.enabled) continue;
                if (mx >= hs.x && mx <= hs.x + hs.w &&
                    my >= hs.y && my <= hs.y + hs.h)
                    return hs.name;
            }
            return "";
        }

        // ── Actualizar hover ────────────────────────────────────────────

        void updateHover(f32 mx, f32 my)
        {
            m_hoveredName.clear();
            for (auto& hs : m_hotspots)
            {
                if (!hs.enabled) { hs.hovered = false; continue; }
                hs.hovered = (mx >= hs.x && mx <= hs.x + hs.w &&
                              my >= hs.y && my <= hs.y + hs.h);
                if (hs.hovered)
                    m_hoveredName = hs.name;
            }
        }

        const std::string& getHovered() const { return m_hoveredName; }

        // ── Acceso directo para edicion ─────────────────────────────────

        Hotspot* get(const std::string& name)
        {
            for (auto& hs : m_hotspots)
                if (hs.name == name) return &hs;
            return nullptr;
        }

        Hotspot* getByIndex(int idx)
        {
            if (idx >= 0 && idx < static_cast<int>(m_hotspots.size()))
                return &m_hotspots[idx];
            return nullptr;
        }

        // ── Habilitar/deshabilitar ──────────────────────────────────────

        int setEnabled(const std::string& name, bool enabled)
        {
            int matches = 0;
            for (auto& hs : m_hotspots)
            {
                if (hs.name != name) continue;
                hs.enabled = enabled;
                ++matches;
            }
            return matches;
        }

        // ── Render de debug ─────────────────────────────────────────────
        // Verde = habilitado, amarillo = hover, rojo = deshabilitado.
        // Si se pasa un TextRenderer, dibuja el nombre dentro del hotspot.

        void debugRender(TextRenderer* textRenderer = nullptr) const
        {
            for (const auto& hs : m_hotspots)
            {
                f32 r, g, b, a;
                if (!hs.enabled)
                    { r=1.0f; g=0.2f; b=0.2f; a=0.25f; }
                else if (hs.hovered)
                    { r=1.0f; g=1.0f; b=0.2f; a=0.35f; }
                else
                    { r=0.2f; g=1.0f; b=0.2f; a=0.2f; }

                draw2d::drawRect(hs.x, hs.y, hs.w, hs.h, r, g, b, a);
                draw2d::drawRectBorder(hs.x, hs.y, hs.w, hs.h,
                                       r, g, b, a + 0.3f, 2.0f);

                // Nombre dentro del hotspot
                if (textRenderer)
                {
                    textRenderer->drawText(hs.name,
                        hs.x + 5, hs.y + 3,
                        r, g, b, 0.9f);
                }
            }
        }

        // ── Outline de hover (CP5 átomo) ────────────────────────────────
        // A diferencia de debugRender (TODOS los hotspots, F3, herramienta
        // de dev), esto pinta SOLO el hotspot bajo el mouse — feedback para
        // el jugador. Si alpha <= 0 no dibuja nada (default off, themeable).
        // El guard hs.enabled es defensa en profundidad: updateHover ya
        // garantiza que un hotspot deshabilitado por el gate [si:] no esté
        // hovered, pero lo reafirmamos aquí.
        void renderHoverOutline(f32 r, f32 g, f32 b, f32 a, f32 thickness) const
        {
            if (a <= 0.0f) return;
            for (const auto& hs : m_hotspots)
            {
                if (hs.enabled && hs.hovered)
                    draw2d::drawRectBorder(hs.x, hs.y, hs.w, hs.h, r, g, b, a, thickness);
            }
        }

        // ── Consultas ───────────────────────────────────────────────────

        int count() const { return static_cast<int>(m_hotspots.size()); }
        const std::vector<Hotspot>& getAll() const { return m_hotspots; }
        std::vector<std::string> getNames() const
        {
            std::vector<std::string> names;
            for (const auto& hs : m_hotspots)
                names.push_back(hs.name);
            return names;
        }

        // Retira una generación completa; no depende de los ids del creador.
        void removeByOwner(std::uint64_t scopeOwner)
        {
            if (scopeOwner == 0) return;
            m_hotspots.erase(
                std::remove_if(m_hotspots.begin(), m_hotspots.end(),
                    [scopeOwner](const Hotspot& hs) {
                        return hs.scopeOwner == scopeOwner;
                    }),
                m_hotspots.end());
            m_hoveredName.clear();
        }

        void clear() { m_hotspots.clear(); m_hoveredName.clear(); }

    private:
        std::vector<Hotspot> m_hotspots;
        std::string          m_hoveredName;
    };

} // namespace eclips
