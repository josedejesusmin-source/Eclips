// ============================================================================
// WindowResolution.h — Política pura de resolución de VENTANA
// ============================================================================
//
// No confundir con viewport:: (resolución de DISEÑO) ni con fullscreen.
// Este archivo no depende de SDL para que la misma ley que usa producción pueda
// probarse sin abrir una ventana.
//
// ============================================================================

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace eclips::window_resolution
{

    struct Size
    {
        int w = 0;
        int h = 0;
    };

    inline bool operator==(const Size& a, const Size& b)
    {
        return a.w == b.w && a.h == b.h;
    }

    inline Size proportionalFit(int designW, int designH, int maxW, int maxH)
    {
        if (designW <= 0 || designH <= 0 || maxW <= 0 || maxH <= 0)
            return { 1, 1 };

        const double sx = static_cast<double>(maxW) / static_cast<double>(designW);
        const double sy = static_cast<double>(maxH) / static_cast<double>(designH);
        const double scale = std::min(sx, sy);

        int w = static_cast<int>(std::floor(static_cast<double>(designW) * scale));
        int h = static_cast<int>(std::floor(static_cast<double>(designH) * scale));
        w = std::clamp(w, 1, maxW);
        h = std::clamp(h, 1, maxH);
        return { w, h };
    }

    inline std::vector<Size> buildCatalog(int designW, int designH,
                                          int maxW, int maxH)
    {
        std::vector<Size> out;
        if (designW <= 0 || designH <= 0 || maxW <= 0 || maxH <= 0)
            return out;

        // 2/3 alinea el catálogo con el boot 1280x720 cuando el lienzo es
        // 1920x1080. Sin ella, una tableta con ~1280x760 útiles veía solo
        // 960x540 aunque el tamaño de arranque sí cabía.
        static constexpr double kScales[] = {
            0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0
        };
        for (double scale : kScales)
        {
            const int w = static_cast<int>(
                std::floor(static_cast<double>(designW) * scale + 0.5));
            const int h = static_cast<int>(
                std::floor(static_cast<double>(designH) * scale + 0.5));
            if (w <= 0 || h <= 0 || w > maxW || h > maxH)
                continue;

            const Size candidate{ w, h };
            if (std::find(out.begin(), out.end(), candidate) == out.end())
                out.push_back(candidate);
        }

        // El selector jamás queda vacío. A diferencia del fallback viejo, este
        // tamaño TAMBIÉN respeta el límite aun si 0.5× no cabe.
        if (out.empty())
            out.push_back(proportionalFit(designW, designH, maxW, maxH));

        return out;
    }

    // ── El catálogo CON su etiqueta (DISENO_IDIOMAS §16, fork 1b) ───────────
    //
    // `CP5` mete `idioma` como segundo `CtrlKind::Enum`, así que el catálogo
    // pasa a entrar por el DESCRIPTOR y el widget deja de saber cuál Enum es.
    // Para que ese refactor tenga guardián, la construcción entera —catálogo +
    // etiqueta— tiene que vivir donde un harness la pueda incluir: hasta hoy la
    // etiqueta se armaba dentro de `VNScriptState::buildResOptions`, que
    // arrastra GL, o sea que nadie la custodiaba.
    //
    // ⚠ La guarda del lienzo del cable viejo (`if (dw<=0||dh<=0) return;`, la
    // primera línea de `buildResOptions`) NO se muda, y eso se MIDIÓ en vez de
    // razonarse: puesta acá, sacarla no ponía rojo a ningún assert, porque
    // `buildCatalog` ya rechaza el lienzo inválido en su propia primera línea
    // (:56). Era un verde en el vacío — la especie de §7.469. El invariante es
    // real y lo custodia el 5.7 de `harness_resolucion`; su DUEÑO es
    // `buildCatalog`, y por eso ese assert se pone rojo el día que alguien le
    // saque el early-return de allá (el fallback proporcional devolvería una
    // opción "1x1" y esta función la publicaría).
    struct Opcion
    {
        int w = 0;
        int h = 0;
        std::string etiqueta;
    };

    inline std::string etiquetaDe(const Size& s)
    {
        return std::to_string(s.w) + "x" + std::to_string(s.h);
    }

    inline std::vector<Opcion> buildCatalogoEtiquetado(int designW, int designH,
                                                       int maxW, int maxH)
    {
        std::vector<Opcion> out;
        // DERIVA DELIBERADA 2: la guarda del lienzo, sacada.
        for (const Size& s : buildCatalog(designW, designH, maxW, maxH))
            out.push_back({ s.w, s.h, etiquetaDe(s) });
        return out;
    }

    inline int nearestIndex(const std::vector<Size>& catalog, int w, int h)
    {
        if (catalog.empty())
            return -1;

        int best = 0;
        std::int64_t bestDistance = -1;
        for (int i = 0; i < static_cast<int>(catalog.size()); ++i)
        {
            if (catalog[i].w == w && catalog[i].h == h)
                return i;

            const std::int64_t dw = static_cast<std::int64_t>(catalog[i].w) - w;
            const std::int64_t dh = static_cast<std::int64_t>(catalog[i].h) - h;
            const std::int64_t distance = dw * dw + dh * dh;
            if (bestDistance < 0 || distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }
        return best;
    }

    inline Size repairOversized(Size requested, int designW, int designH,
                                int maxW, int maxH)
    {
        if (requested.w > 0 && requested.h > 0
            && requested.w <= maxW && requested.h <= maxH)
        {
            return requested;
        }

        const std::vector<Size> catalog =
            buildCatalog(designW, designH, maxW, maxH);
        const int best = nearestIndex(catalog, requested.w, requested.h);
        return best >= 0 ? catalog[best]
                         : proportionalFit(designW, designH, maxW, maxH);
    }

} // namespace eclips::window_resolution
