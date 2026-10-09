// ============================================================================
// ECLIPS ENGINE - TextRenderer (Implementacion)
// ============================================================================

#include "TextRenderer.h"
#include "FontManager.h" // fuente inline {fuente=X}: resolver nombre→TextRenderer en render
#include "OutlineCircle.h"
#include "Draw2D.h"  // draw2d::g_batch para SpriteBatch (Render Core Fase 1, paso 6.B)
#include "../core/TextBudget.h" // §7.410: la unidad del presupuesto del typewriter
#include "../core/TextWrap.h"   // `WRAP-MIDE-BYTES`: la ley del corte, una sola vez
#include "../core/TextGlifo.h"  // `GLIFO-FALTANTE-DIVERGE`: la política del ausente
#include "InlineTags.h"  // vocabulario-lote3: el parser de tags vive aca

#include <SDL_opengl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdio>   // el `U+XXXX` del aviso de glifo ausente
#include <cmath>

// stb_truetype para generar el atlas de fuentes
#include "stb_truetype.h"

namespace eclips
{
    TextRenderer::~TextRenderer()
    {
        if (m_atlasTexID != 0) shutdown();
    }

    bool TextRenderer::init(const std::string& fontPath, f32 fontSize, f32 kerning,
                            FontMeasure measure)
    {

        if (m_atlasTexID != 0) shutdown();   // Fix: liberar atlas anterior (doble-init seguro)

        m_fontSize = fontSize;
        m_kerning = kerning;   // se pliega en el advance de cada glifo más abajo
        m_fontMeasure = measure;
        m_fontPath = fontPath; // FASE 2: recordar el ttf para re-hornearlo a otro tamaño
        m_profilePlan = {};

        // ── Leer archivo de fuente ──
        FontSourceHandle retained;
        std::vector<unsigned char> fontData;
        const unsigned char* fontBytes = nullptr;
        if (m_fontSourceStore)
        {
            retained = m_fontSourceStore->acquire(kFontLegacyGeneration, fontPath);
            fontBytes = retained.data();
        }
        else
        {
            std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
            if (file.is_open())
            {
                const auto fileSize = file.tellg();
                file.seekg(0, std::ios::beg);
                if (fileSize > 0)
                {
                    fontData.resize(static_cast<size_t>(fileSize));
                    if (file.read(reinterpret_cast<char*>(fontData.data()),
                                  fileSize))
                        fontBytes = fontData.data();
                }
            }
        }
        if (!fontBytes)
        {
            std::cerr << "[Eclips] Error: No se pudo abrir fuente: " << fontPath << std::endl;
            return false;
        }

        // ── Inicializar stb_truetype ──
        stbtt_fontinfo fontInfo;
        const int fontOffset = stbtt_GetFontOffsetForIndex(fontBytes, 0);
        if (fontOffset < 0 || !stbtt_InitFont(&fontInfo, fontBytes, fontOffset))
        {
            std::cerr << "[Eclips] Error: Fuente invalida: " << fontPath << std::endl;
            return false;
        }

        const f32 scale = (measure == FontMeasure::Em)
            ? stbtt_ScaleForMappingEmToPixels(&fontInfo, fontSize)
            : stbtt_ScaleForPixelHeight(&fontInfo, fontSize);

        // Obtener metricas verticales
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
        m_ascent = ascent * scale;
        m_lineHeight = (ascent - descent + lineGap) * scale;

        // ── Generar atlas ──
        // Caracteres a incluir: ASCII basico + caracteres latinos extendidos
        // (acentos, ñ, ¿, ¡, etc. para español)
        m_atlasW = 1024;
        m_atlasH = 1024;
        std::vector<unsigned char> atlasPixels(m_atlasW * m_atlasH, 0);

        int penX = 2, penY = 2;
        int rowHeight = 0;

        // Rango: 32-126 (ASCII) + 160-255 (Latin-1 extendido) + algunos extra.
        // CP-R3: la lista se mudo a `repertorioBootstrap()` porque el catalogo
        // hornea sus perfiles por `initProfile` y los dos caminos tienen que
        // pedir los MISMOS codepoints; dos copias divergirian en silencio.
        const std::vector<i32>& codepoints = repertorioBootstrap();

        // (fork D, 2026-08-28) Las dos puertas a la ausencia que se pueden ver
        // ACÁ, y que hasta hoy no decían nada: la fuente que no tiene el glifo
        // y el atlas que se desborda. La tercera —el codepoint que el texto usa
        // y el repertorio no pide— se avisa en `avisarAusente`, al usarlo.
        int sinGlifoEnLaFuente = 0;
        bool seDesbordo = false;

        for (int cp : codepoints)
        {
            int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, cp);
            if (glyphIndex == 0 && cp != 32) { ++sinGlifoEnLaFuente; continue; } // la FUENTE no lo tiene

            // Obtener metricas del glifo
            int advanceWidth, leftBearing;
            stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceWidth, &leftBearing);

            int x0, y0, x1, y1;
            stbtt_GetGlyphBitmapBox(&fontInfo, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);

            int gw = x1 - x0;
            int gh = y1 - y0;

            // Siguiente linea si no cabe
            if (penX + gw + 2 >= m_atlasW)
            {
                penX = 2;
                penY += rowHeight + 2;
                rowHeight = 0;
            }

            // Si se sale del atlas verticalmente, parar
            if (penY + gh + 2 >= m_atlasH) { seDesbordo = true; break; }

            // Rasterizar glifo directamente en el atlas
            if (gw > 0 && gh > 0)
            {
                stbtt_MakeGlyphBitmap(&fontInfo,
                    atlasPixels.data() + penX + penY * m_atlasW,
                    gw, gh, m_atlasW, scale, scale, glyphIndex);
            }

            // Guardar info del glifo
            GlyphInfo info;
            // UVs COMPLETAS (edge-to-edge): mapean el glifo borde a borde del quad.
            // Lo que quiere el texto normal — sin esto se recortaba el anillo
            // externo de cada glifo (S/O "boxeadas", () cortados). El quad es 1:1
            // pixel-texel, asi que GL_LINEAR ya muestrea centros de texel = nitido.
            info.uvX0 = static_cast<f32>(penX) / m_atlasW;
            info.uvY0 = static_cast<f32>(penY) / m_atlasH;
            info.uvX1 = static_cast<f32>(penX + gw) / m_atlasW;
            info.uvY1 = static_cast<f32>(penY + gh) / m_atlasH;
            // UVs INSETADAS medio texel (centro del 1er texel al centro del ultimo).
            // SOLO para los stamps del glow: con 24 copias acumuladas, el borde que
            // interpola con el padding (alpha 0) sumaba un "borde fantasma"
            // rectangular en el halo. El inset lo mata (half-texel correction
            // estandar). El texto normal NO la usa — recortaria los glifos.
            info.uvX0i = (static_cast<f32>(penX) + 0.5f) / m_atlasW;
            info.uvY0i = (static_cast<f32>(penY) + 0.5f) / m_atlasH;
            info.uvX1i = (static_cast<f32>(penX + gw) - 0.5f) / m_atlasW;
            info.uvY1i = (static_cast<f32>(penY + gh) - 0.5f) / m_atlasH;
            info.offsetX = static_cast<f32>(x0);
            info.offsetY = static_cast<f32>(y0);
            info.width = static_cast<f32>(gw);
            info.height = static_cast<f32>(gh);
            // El kerning de la fuente se pliega AQUÍ, en el único origen del advance.
            // Consecuencia: draw/measure/word-wrap leen un solo número → no pueden
            // desincronizarse. (Ren'Py lo suma entre pares en runtime; nosotros lo
            // horneamos al construir el atlas — mismo efecto, sin tocar los call-sites.)
            info.advance = advanceWidth * scale + m_kerning;

            m_glyphs[cp] = info;

            penX += gw + 2;
            if (gh + 2 > rowHeight) rowHeight = gh + 2;
        }

        // ── El motor HABLA cuando el atlas queda incompleto ────────────────
        // Sin esto, omitir sería omitir en silencio, que es peor que el '?' que
        // había antes. El aviso va al creador —que puede arreglarlo— y no al
        // jugador, que no puede. `GLIFO-FALTANTE-DIVERGE`, fork D.
        {
            std::string nombre = m_fontPath;
            const size_t barra = nombre.find_last_of("/\\");
            if (barra != std::string::npos) nombre = nombre.substr(barra + 1);
            if (nombre.empty()) nombre = "(fuente sin nombre)";

            if (sinGlifoEnLaFuente > 0)
            {
                std::cerr << "[Eclips] Aviso: '" << nombre << "' no tiene "
                          << sinGlifoEnLaFuente << " de los " << codepoints.size()
                          << " glifos del repertorio.\n";
            }
            if (seDesbordo)
            {
                std::cerr << "[Eclips] Aviso: el atlas de '" << nombre << "' a "
                          << (int)fontSize << " px se desbordo: entraron "
                          << m_glyphs.size() << " de " << codepoints.size()
                          << ". Los que faltan no se van a dibujar.\n";
            }
        }

        // El atlas es nuevo: lo que faltaba puede estar ahora. Si el registro
        // no se limpia, el creador arregla la fuente y el motor sigue callado
        // (o peor: sigue quejandose de un glifo que ya existe).
        m_ausentes.olvidar();

        // ── Crear textura OpenGL del atlas ──
        glGenTextures(1, &m_atlasTexID);
        glBindTexture(GL_TEXTURE_2D, m_atlasTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // El atlas se rasteriza como single-channel (alpha) por stb_truetype,
        // pero se sube a GPU como RGBA8 con canales RGB=255 y A=alpha real.
        //
        // Razón (Render Core Fase 1, paso 6.A, decisión D1-A):
        //   El shader del SpriteBatch hace texture(...) * vColor asumiendo
        //   RGBA. Si subiéramos el atlas como GL_ALPHA, el sample retornaría
        //   (0,0,0,a) y el texto saldría negro independiente del tint.
        //   Expandiendo a RGBA8 con RGB=255, el sample retorna (1,1,1,a) y
        //   el multiply con vColor da (r,g,b,a*tint_a) — color correcto.
        //
        // Costo: 4× memoria del atlas (1024×1024 = 1MB → 4MB). Dentro del
        // norte star sub-100MB. GL_ALPHA además es deprecated en Core
        // profile (futuro Android port).
        //
        // Alternativas consideradas y rechazadas:
        //   - GL_RED + swizzle en shader: rompe batching futuro (uniform
        //     uIsText) o introduce convención mágica frágil.
        //   - TextRenderer con shader propio: trabajo adelantado para
        //     arquitectura que cambia con SDF/MSDF post-Android.
        std::vector<unsigned char> rgbaPixels(m_atlasW * m_atlasH * 4);
        for (int i = 0; i < m_atlasW * m_atlasH; i++)
        {
            rgbaPixels[i * 4 + 0] = 255;
            rgbaPixels[i * 4 + 1] = 255;
            rgbaPixels[i * 4 + 2] = 255;
            rgbaPixels[i * 4 + 3] = atlasPixels[i];
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_atlasW, m_atlasH,
            0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels.data());

        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "[Eclips] Fuente cargada: " << fontPath
                  << " (" << fontSize << "px, "
                  << m_glyphs.size() << " glifos)" << std::endl;

        return true;
    }

    bool TextRenderer::initProfile(const TextProfileSpec& spec)
    {
        if (!m_fontSourceStore || spec.routes.empty())
            return false;
        if (m_atlasTexID != 0) shutdown();

        TextProfileSpec resolved = spec;
        for (auto& route : resolved.routes)
            route = fontResolverRutaEspejo(route);

        const TextProfilePlanHandle plan =
            m_fontSourceStore->acquirePlan(resolved);
        if (!plan) return false;

        struct Descriptor
        {
            i32 cp = -1;
            const stbtt_fontinfo* font = nullptr;
            f32 scale = 0.0f;
            int glyph = 0;
            int x0 = 0, y0 = 0, gw = 0, gh = 0;
            int penX = 0, penY = 0;
            int advance = 0;
        };
        std::vector<Descriptor> descriptors;
        descriptors.reserve(plan.spec().repertoire.size());
        for (const i32 cp : plan.spec().repertoire)
        {
            const stbtt_fontinfo* font = plan.fontInfoFor(cp);
            if (!font) continue;
            Descriptor d;
            d.cp = cp;
            d.font = font;
            d.scale = plan.scaleFor(cp);
            d.glyph = stbtt_FindGlyphIndex(font, cp);
            if (d.glyph == 0 && cp != 32) continue;
            int leftBearing = 0;
            stbtt_GetGlyphHMetrics(font, d.glyph, &d.advance, &leftBearing);
            int x1 = 0, y1 = 0;
            stbtt_GetGlyphBitmapBox(font, d.glyph, d.scale, d.scale,
                                    &d.x0, &d.y0, &x1, &y1);
            d.gw = x1 - d.x0;
            d.gh = y1 - d.y0;
            descriptors.push_back(d);
        }

        // P1 firmado: prepass y orden estable alto/ancho/codepoint.
        std::stable_sort(descriptors.begin(), descriptors.end(),
            [](const Descriptor& a, const Descriptor& b)
            {
                if (a.gh != b.gh) return a.gh > b.gh;
                if (a.gw != b.gw) return a.gw > b.gw;
                return a.cp < b.cp;
            });

        auto place = [&](int atlasSize) -> bool
        {
            int penX = 2, penY = 2, rowHeight = 0;
            for (auto& d : descriptors)
            {
                if (penX + d.gw + 2 >= atlasSize)
                {
                    penX = 2;
                    penY += rowHeight + 2;
                    rowHeight = 0;
                }
                if (penY + d.gh + 2 >= atlasSize) return false;
                d.penX = penX;
                d.penY = penY;
                penX += d.gw + 2;
                rowHeight = std::max(rowHeight, d.gh + 2);
            }
            m_packPenX = penX;
            m_packPenY = penY;
            m_packRowHeight = rowHeight;
            m_packLive = true;
            return true;
        };

        m_atlasW = m_atlasH = 1024;
        if (!place(1024))
        {
            m_atlasW = m_atlasH = 2048;
            if (!place(2048))
            {
                std::cerr << "[Eclips] Aviso: perfil tipografico excede atlas "
                          << "2048; se omite la publicacion.\n";
                return false;
            }
        }

        std::vector<unsigned char> atlasPixels(
            static_cast<std::size_t>(m_atlasW) * m_atlasH, 0);
        m_glyphs.clear();
        for (const auto& d : descriptors)
        {
            if (d.gw > 0 && d.gh > 0)
                stbtt_MakeGlyphBitmap(
                    d.font,
                    atlasPixels.data() + d.penX + d.penY * m_atlasW,
                    d.gw, d.gh, m_atlasW, d.scale, d.scale, d.glyph);

            GlyphInfo info{};
            info.uvX0 = static_cast<f32>(d.penX) / m_atlasW;
            info.uvY0 = static_cast<f32>(d.penY) / m_atlasH;
            info.uvX1 = static_cast<f32>(d.penX + d.gw) / m_atlasW;
            info.uvY1 = static_cast<f32>(d.penY + d.gh) / m_atlasH;
            info.uvX0i = (static_cast<f32>(d.penX) + 0.5f) / m_atlasW;
            info.uvY0i = (static_cast<f32>(d.penY) + 0.5f) / m_atlasH;
            info.uvX1i = (static_cast<f32>(d.penX + d.gw) - 0.5f) / m_atlasW;
            info.uvY1i = (static_cast<f32>(d.penY + d.gh) - 0.5f) / m_atlasH;
            info.offsetX = static_cast<f32>(d.x0);
            info.offsetY = static_cast<f32>(d.y0);
            info.width = static_cast<f32>(d.gw);
            info.height = static_cast<f32>(d.gh);
            info.advance = d.advance * d.scale + spec.kerning;
            m_glyphs[d.cp] = info;
        }

        m_fontSize = spec.fontSize;
        m_kerning = spec.kerning;
        m_fontMeasure = spec.measure;
        m_fontPath = resolved.routes.front();
        m_ascent = plan.ascent();
        m_lineHeight = plan.lineHeight();
        m_profilePlan = plan;
        m_ausentes.olvidar();

        glGenTextures(1, &m_atlasTexID);
        glBindTexture(GL_TEXTURE_2D, m_atlasTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        std::vector<unsigned char> rgba(
            static_cast<std::size_t>(m_atlasW) * m_atlasH * 4u);
        for (std::size_t i = 0; i < atlasPixels.size(); ++i)
        {
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = atlasPixels[i];
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_atlasW, m_atlasH,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    // ── Texto simple ──────────────────────────────────────────────────────

    void TextRenderer::drawText(const std::string& text, f32 x, f32 y,
                                f32 r, f32 g, f32 b, f32 a)
    {
        drawTextPartial(text, x, y, -1, r, g, b, a);
    }

    void TextRenderer::drawTextPartial(const std::string& text, f32 x, f32 y,
        i32 charCount,
        f32 r, f32 g, f32 b, f32 a)
    {
        if (!isLoaded()) return;
        if (!draw2d::g_batch) return;

        // Render Core Fase 1, paso 6.C — wrapper que abre/cierra batch.
        // La lógica de emisión vive en _noBatch para que drawTextOutlined
        // pueda envolver 9 llamadas en UN solo begin/end (batching real).
        draw2d::g_batch->begin();
        drawTextPartial_noBatch(text, x, y, charCount, r, g, b, a);
        draw2d::g_batch->end();
    }

    void TextRenderer::drawTextPartial_noBatch(const std::string& text, f32 x, f32 y,
        i32 charCount,
        f32 r, f32 g, f32 b, f32 a, bool useInset, f32 lineSpacing)
    {
        if (!isLoaded()) return;
        if (!draw2d::g_batch) return;

        // Render Core Fase 1, paso 6.C — Emisión sin begin/end.
        // Asume que el caller ya abrió g_batch->begin(). Usado por:
        //   - drawTextPartial (wrapper público, 1 begin + 1 end aquí).
        //   - drawTextOutlined (1 begin + 9 emisiones + 1 end).
        // UV mapping: atlas SIN stb-flip (paso 6.A), UVs directas.

        f32 curX = x;
        f32 curY = y + m_ascent; // Baseline
        size_t pos = 0;
        i32 charIdx = 0;

        while (pos < text.size())
        {
            if (charCount >= 0 && charIdx >= charCount) break;

            // `DECODER-UTF8-SIN-GUARDA`: acá había una copia del decoder de
            // `measureWidth` **sin sus guardas `ptr < end`**. Con una secuencia
            // UTF-8 truncada al final del string leía el terminador y dejaba el
            // puntero UNO más allá, y el `while (*ptr)` seguía leyendo fuera
            // del buffer. Ahora delega en el decoder con nombre, que ya tenía
            // guardián (`harness_wrap §2`, `harness_glifo §5`).
            //
            // La conducta no cambia en texto bien formado: un líder inválido
            // —incluidos los de CUATRO bytes— consume uno y no suma nada, que
            // es exactamente lo que hacía el `else { ptr++; continue; }`.
            // (Que el de 4 bytes no avance `charIdx` es `TICK-MUERTO-EN-4-BYTES`,
            // ficha aparte y con su gatillo: acá no se toca.)
            i32 cp = -1;
            const size_t paso = text::utf8Decodificar(text, pos, cp);
            if (paso == 0) break;
            pos += paso;
            if (cp < 0) continue;

            // `interlineado`: el paso lleva el extra; el ALTO DE CAJA (m_lineHeight,
            // lo que devuelve getLineHeight()) queda intacto. Con lineSpacing 0 —el
            // default de la firma— esto es la línea de siempre, bit por bit.
            if (cp == '\n') { curX = x; curY += m_lineHeight + lineSpacing; charIdx++; continue; }

            // §7.584 — la política del glifo ausente, la MISMA que mide y que
            // corta. Antes acá había un `getGlyph('?')` cableado que ninguna de
            // las otras cuatro conocía.
            const GlyphInfo* g_info = getGlyph(cp);
            if (!g_info)
            {
                avisarAusente(cp);
                if (text::sustituyeElAusente(text::politicaDeLaCasa()))
                    g_info = getGlyph('?');
                if (!g_info)
                {
                    curX += avanceDeEsteAtlas(cp);   // 0 con la política de hoy
                    charIdx++;
                    continue;
                }
            }

            f32 gx = curX + g_info->offsetX;
            f32 gy = curY + g_info->offsetY;
            f32 gw = g_info->width;
            f32 gh = g_info->height;

            if (gw > 0 && gh > 0)
            {
                // useInset → UVs del glow (mata el borde-fantasma del halo).
                // Resto (texto/outline/fill) → UVs completas.
                f32 u0 = useInset ? g_info->uvX0i : g_info->uvX0;
                f32 v0 = useInset ? g_info->uvY0i : g_info->uvY0;
                f32 u1 = useInset ? g_info->uvX1i : g_info->uvX1;
                f32 v1 = useInset ? g_info->uvY1i : g_info->uvY1;
                draw2d::g_batch->draw(
                    m_atlasTexID,
                    gx, gy, gw, gh,
                    u0, v0, u1, v1,
                    r, g, b, a);
            }

            curX += g_info->advance;
            charIdx++;
        }
    }

    // F2.C: drawText sin begin/end — delega a drawTextPartial_noBatch.
    void TextRenderer::drawText_noBatch(const std::string& text, f32 x, f32 y,
        f32 r, f32 g, f32 b, f32 a)
    {
        drawTextPartial_noBatch(text, x, y, -1, r, g, b, a);
    }

    // ── El plan de corte con las métricas de ESTE atlas ─────────────────────
    // La ley vive en `TextWrap.h`; acá sólo se le enchufan los advances.
    //
    // El avance sale de `avanceDeEsteAtlas`, que es la política de GLIFO
    // AUSENTE — la MISMA que usa `draw`. Antes acá había un `gi ? gi->advance
    // : 0` cableado y `draw` sustituía por '?': el motor medía una cosa y
    // dibujaba otra (`GLIFO-FALTANTE-DIVERGE` / §7.584, cerrada 2026-08-28).
    //
    // El espacio va aparte del callback a propósito: `drawTextWrappedRich_noBatch`
    // lo mide con la fuente BASE aunque el resto de la línea traiga {fuente=X},
    // y esa asimetría es conducta viva, no un descuido (ley 3 de `TextWrap.h`).
    text::PlanDeCorte TextRenderer::planDeCorteDeEsteAtlas(const std::string& texto,
                                                           f32 anchoMax) const
    {
        const f32 spaceW = avanceDeEsteAtlas(' ');

        return text::planDeCorte(
            texto, anchoMax, spaceW,
            [this](i32 cp, size_t) -> f32 { return avanceDeEsteAtlas(cp); });
    }

    void TextRenderer::drawTextWrapped(const std::string& text, f32 x, f32 y,
                                       f32 maxWidth, i32 charCount,
                                       f32 r, f32 g, f32 b, f32 a)
    {
        if (!isLoaded()) return;

        // `WRAP-MIDE-BYTES`: la ley del corte vive en `TextWrap.h` y la prueba
        // `harness_wrap.cpp`. Aca abajo habia una copia de ese lazo -- una de
        // cuatro --, y acumulaba el ancho POR BYTE.
        // §7.410 — ver `TextBudget.h`: los saltos que inserta este wrap no
        // gastan presupuesto, porque el presupuesto se contó sobre el original.
        std::string wrapped;
        std::vector<size_t> saltosInsertados;
        text::aplicarCortes(text, planDeCorteDeEsteAtlas(text, maxWidth),
                            wrapped, saltosInsertados);

        drawTextPartial(wrapped, x, y,
                        text::wrappedStop(wrapped, saltosInsertados, charCount),
                        r, g, b, a);
    }

    // F2.C: drawTextWrapped sin begin/end — word-wrap + drawTextPartial_noBatch.
    void TextRenderer::drawTextWrapped_noBatch(const std::string& text, f32 x, f32 y,
        f32 maxWidth, i32 charCount,
        f32 r, f32 g, f32 b, f32 a, f32 lineSpacing)
    {
        if (!isLoaded()) return;

        // §7.410 — gemela de drawTextWrapped: mismos saltos, misma ley. Y desde
        // `WRAP-MIDE-BYTES` eso es literal: comparten la ley, no una copia.
        std::string wrapped;
        std::vector<size_t> saltosInsertados;
        text::aplicarCortes(text, planDeCorteDeEsteAtlas(text, maxWidth),
                            wrapped, saltosInsertados);

        drawTextPartial_noBatch(wrapped, x, y,
                                text::wrappedStop(wrapped, saltosInsertados, charCount),
                                r, g, b, a,
                                /*useInset=*/false, lineSpacing);
    }

    // ── Rich Text: parser de tags ───────────────
    //
    // `vocabulario-lote3`: el parser SE FUE a `engine/render/InlineTags.h`, y
    // con el se fue `parseHex6_local`, que solo lo usaba el. Aca queda el
    // reenvio para que los tres call-sites de `VNScriptState.h` no cambien una
    // letra.
    //
    // El motivo es `RO-5`: esta TU incluye `<SDL_opengl.h>`, asi que mientras
    // la ley viviera aca adentro NINGUN harness podia alcanzarla - la pared de
    // `MERGE-DUPLICADO`. La funcion ya era `static` y ya era PURA (no tocaba
    // un solo miembro de instancia), o sea que la pared era de UBICACION y no de
    // acoplamiento: fue un *move*, no un rewrite. Lo custodia `harness_tags`.
    RichText TextRenderer::parseRichText(const std::string& raw)
    {
        return tags::parseRichText(raw);
    }

    // ── Rich Text: render con efectos ─────────────────────────────────────

    void TextRenderer::drawTextWrappedRich(const std::string& text, f32 x, f32 y,
        f32 maxWidth, i32 charCount, f32 time,
        const std::vector<CharEffect>& effects,
        f32 r, f32 g, f32 b, f32 a)
    {
        if (!isLoaded()) return;
        if (!draw2d::g_batch) return;

        // Render Core Fase 1, paso 6.C — wrapper que abre/cierra batch.
        // La lógica vive en _noBatch para que drawTextWrappedRichOutlined
        // pueda envolver 9 llamadas en UN solo begin/end (batching real).
        draw2d::g_batch->begin();
        drawTextWrappedRich_noBatch(text, x, y, maxWidth, charCount, time, effects, r, g, b, a);
        draw2d::g_batch->end();
    }

    void TextRenderer::drawTextWrappedRich_noBatch(const std::string& text, f32 x, f32 y,
        f32 maxWidth, i32 charCount, f32 time,
        const std::vector<CharEffect>& effects,
        f32 r, f32 g, f32 b, f32 a, bool honorCharColor,
        FontManager* fm, f32 lineSpacing)
    {
        if (!isLoaded()) return;
        if (!draw2d::g_batch) return;

        // Render Core Fase 1, paso 6.C — Emisión sin begin/end.
        // Asume caller ya abrió g_batch->begin(). Word-wrap (Fase 1) se
        // recalcula cada llamada — deuda chica CPU (nanosegundos para líneas
        // típicas de VN). Optimizar a 1× word-wrap es mejora futura.

        // ── Fase 1: Word-wrap + construir effects paralelos ──────────────
        std::string wrapped;
        std::vector<CharEffect> wrappedFX;

        // El espacio se mide con la fuente BASE aunque la línea traiga
        // {fuente=X}. Es conducta viva y no un descuido: pasarlo por el
        // callback de abajo "por simetría" le mueve el layout a toda caja con
        // fuente inline. Ley 3 de `TextWrap.h`.
        // El espacio va por la MISMA política que el resto, y sobre `this` —o
        // sea la fuente BASE— que es la ley 3 de `TextWrap.h` y conducta viva:
        // este lazo resuelve la fuente por carácter para el contenido y NO
        // para el espacio.
        const f32 spaceW = avanceDeEsteAtlas(' ');

        CharEffect noEffect; // default: None, speed 1.0

        // ── Fuente inline (B): resuelve el CharEffect → TextRenderer que lo
        //    mide y dibuja. Vacío → this. Nombrada presente → fm->get. Nombrada
        //    ausente (o sin fm) → this + aviso 1× (fallback decidido en diseño).
        //    Robusto a hot-reload: siempre pregunta a fm, nunca cachea instancia.
        auto resolveFont = [&](const CharEffect& e) -> TextRenderer* {
            // Paso 1: fuente BASE (this o nombrada). Comportamiento exacto del
            //    código viejo: vacío→this, nombrada hallada→named, ausente→this+aviso.
            TextRenderer* base = this;
            if (!e.fontKey.empty() || !e.fontName.empty()) {
                // CP-R3: identidad resuelta primero, nombre crudo despues.
                TextRenderer* named =
                    fm ? fm->getLigado(e.fontKey, e.fontName) : nullptr;
                if (named) {
                    base = named;
                }
                else {
                    static bool s_warnedFont = false;
                    if (!s_warnedFont) {
                        std::cout << "[TextRenderer] {fuente=" << e.fontName
                            << "}: fuente no encontrada. Uso la fuente de la linea."
                            << std::endl;
                        s_warnedFont = true;
                    }
                }
            }
            // Paso 2: tamaño inline sobre la base resuelta (§7.76, espejo
            //    per-glifo de sizedFont). Compone con {fuente=Y} gratis: el
            //    tamaño reescala el ttf que trajo la base, no el baseline.
            if (e.fontSize > 0.0f && fm && base->fontSize() != e.fontSize) {
                if (TextRenderer* sized =
                    fm->getSized(*base, e.fontSize, base->kerning(),
                                 base->fontMeasure()))
                    return sized;
            }
            return base;
            };

        // §7.410 — dónde caen los '\n' que INSERTA este wrap, en bytes sobre
        // `wrapped`. El presupuesto se contó sobre el texto ORIGINAL, así que
        // estos saltos no pueden cobrarle nada: `TextBudget.h` ley 2.
        std::vector<size_t> saltosInsertados;

        // `WRAP-MIDE-BYTES`: éste es el cuarto camino, y el único que NO puede
        // usar `planDeCorteDeEsteAtlas` — mide con la fuente del CARÁCTER, no
        // con la base. `effects` viene por CODEPOINT (parseRichText), que es
        // exactamente el índice que la ley entrega.
        //
        // Antes acá abajo había un lazo que iteraba BYTES y le pedía al atlas
        // un glifo por byte: la enie pesaba dos y la raya menos que una letra.
        const text::PlanDeCorte plan = text::planDeCorte(
            text, maxWidth, spaceW,
            [&](i32 cp, size_t indiceCp) -> f32
            {
                const CharEffect& e =
                    (indiceCp < effects.size()) ? effects[indiceCp] : noEffect;
                // §7.584: la política del ausente, la misma que `draw` usa
                // treinta líneas más abajo — y con la fuente del CARÁCTER, que
                // es lo que este camino tiene de propio.
                return resolveFont(e)->avanceDeEsteAtlas(cp);
            });

        // `wrappedFX` va indexado POR BYTE y no por codepoint, porque así lo
        // lee la fase 2 (`wrappedFX[startByte]`, más abajo). Los bytes de
        // continuación repiten el efecto de su líder. Emitir un efecto por
        // codepoint compila, no mueve un pixel de layout y desalinea TODOS los
        // tags inline desde el primer acento — lo mide `harness_wrap` §11.
        text::aplicarCortesFX(text, plan, effects, noEffect,
                              wrapped, wrappedFX, saltosInsertados);

        // ── Fase 2: Render glifo por glifo con efectos ───────────────────
        // Asume batch ya abierto por el caller (wrapper público o variante
        // outlined). Ver decisión D5-A del paso 6.C.

        // §7.410 — `charCount` es presupuesto en CODEPOINTS DEL ORIGINAL; el
        // loop de abajo recorre el ENVUELTO. `wrappedStop` hace la conversión
        // (y sólo ella): los saltos que insertó el wrap no gastan. El cuerpo
        // del loop no cambia — cambia el número contra el que compara.
        const i32 tope = text::wrappedStop(wrapped, saltosInsertados, charCount);

        f32 curX = x;
        f32 curY = y + m_ascent;
        size_t pos = 0;   // posicion en bytes del wrapped string (para lookup de effects)
        i32 charIdx = 0;  // cuenta caracteres renderizados (para el tope)
        i32 glyphNum = 0;  // cuenta glifos visibles (para desfase de onda)
        // B: alto de línea = máximo de las fuentes presentes en la línea.
        //    Se acumula al recorrer glifos; se consume y resetea en cada '\n'.
        f32 maxLineH = m_lineHeight;

        while (pos < wrapped.size())
        {
            if (tope >= 0 && charIdx >= tope) break;

            const int startByte = (int)pos;

            // `DECODER-UTF8-SIN-GUARDA`, el SEGUNDO sitio. La ficha nombraba
            // uno solo (`drawTextPartial_noBatch`) y eran DOS: éste tenía el
            // mismo cuerpo y la misma falta de `ptr < end`. Delega en el
            // decoder con nombre, igual que su hermano.
            //
            // `pos` reemplaza al `byteIdx` que se llevaba a mano en paralelo
            // con el puntero: eran dos cuentas del mismo número, y `wrappedFX`
            // se indexa con ella (va por BYTE, no por codepoint — `§11` de
            // `harness_wrap`).
            i32 cp = -1;
            const size_t paso = text::utf8Decodificar(wrapped, pos, cp);
            if (paso == 0) break;
            pos += paso;
            if (cp < 0) continue;

            if (cp == '\n')
            {
                curX = x;
                // B: salta por el alto máximo de ESTA línea. `interlineado` se
                // suma DESPUÉS del máximo, una sola vez: el extra es propiedad de
                // la CAJA, no de la fuente que ganó el maxLineH — si no, una línea
                // con {fuente=X} grande cobraría el interlineado dos veces.
                curY += maxLineH + lineSpacing;
                maxLineH = m_lineHeight;     // reset para la línea siguiente
                charIdx++;
                continue;
            }


            // ── Efecto del glifo (uno solo: fuente, fx, color salen de aquí) ──
            CharEffect eff = (startByte < (int)wrappedFX.size())
                ? wrappedFX[startByte] : noEffect;

            // B: la fuente del char resuelve glifo, atlas y métrica.
            //    El baseline (curY/m_ascent) sigue anclado a la fuente base.
            TextRenderer* src = resolveFont(eff);
            if (src->getLineHeight() > maxLineH) maxLineH = src->getLineHeight();

            // §7.584 — la misma política que el wrap de arriba, con la fuente
            // del carácter. `src` avisa por su cuenta: el ausente lo es de ESA
            // fuente y no de la base.
            const GlyphInfo* g_info = src->getGlyph(cp);
            if (!g_info)
            {
                src->avisarAusente(cp);
                if (text::sustituyeElAusente(text::politicaDeLaCasa()))
                    g_info = src->getGlyph('?');
                if (!g_info)
                {
                    curX += src->avanceDeEsteAtlas(cp);   // 0 con la política de hoy
                    charIdx++;
                    glyphNum++;
                    continue;
                }
            }

            f32 ox = 0, oy = 0;
            switch (eff.fx)
            {
            case TextFX::Wavy:
                oy = std::sin(time * 4.0f + glyphNum * 0.5f) * 3.0f;
                break;

            case TextFX::Shake:
            {
                // Pseudo-aleatorio basado en tiempo + indice del glifo
                // Cambia cada ~60ms para un temblor visible pero no epileptico
                unsigned int frame = (unsigned int)(time * 16.0f);
                unsigned int seed = (frame * 2654435761u) ^ (glyphNum * 340573321u);
                ox = ((seed % 100) / 50.0f - 1.0f) * 2.0f;
                seed = seed * 1103515245u + 12345u;
                oy = ((seed % 100) / 50.0f - 1.0f) * 2.0f;
                break;
            }

            case TextFX::Glitch:
                // Abajo: el revoltijo necesita `src` para el glifo sustituto.
                break;

            default: break;
            }

            // ── {glitch} (E-CP7): revoltijo + jitter + franja ─────────────
            // El glifo DIBUJADO es otro (pool ASCII del mismo atlas — si el
            // tramo trae {fuente=X}, del atlas de ESA fuente), re-sorteado a
            // ~12 Hz con la cuantización de Shake. El AVANCE es el del char
            // ORIGINAL: contenido ⊥ layout — el wrap, el typewriter y las
            // métricas no se enteran. Los 9 pases del outline comparten
            // `time` → mismo sorteo → el contorno abraza al glifo revuelto.
            // El espacio no se revuelve (respeta la respiración de la línea).
            const GlyphInfo* d_info = g_info;
            const bool esGlitch = (eff.fx == TextFX::Glitch);
            if (esGlitch && cp != ' ')
            {
                static const char kGlitchPool[] =
                    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
                    "0123456789#$%&/()=?!*+<>-";
                unsigned int frame = (unsigned int)(time * 12.0f);
                unsigned int seed = (frame * 2654435761u)
                    ^ ((unsigned int)glyphNum * 340573321u);
                // avalancha barata: sin ella, frame*const deja los bits
                // bajos correlacionados y el % del pool repite vecinos
                seed ^= seed >> 13; seed *= 0x5bd1e995u; seed ^= seed >> 15;

                const GlyphInfo* s_info = src->getGlyph(
                    kGlitchPool[seed % (sizeof(kGlitchPool) - 1)]);
                if (s_info) d_info = s_info;

                ox += ((seed % 100) / 50.0f - 1.0f) * 2.0f;
                seed = seed * 1103515245u + 12345u;
                oy += ((seed % 100) / 50.0f - 1.0f) * 2.0f;
            }

            f32 gx = curX + d_info->offsetX + ox;
            f32 gy = curY + d_info->offsetY + oy;
            f32 gw = d_info->width;
            f32 gh = d_info->height;

            // ── Color del glifo: el override del char (RGB) pisa el de línea.
            //    El alpha SIEMPRE se hereda de la línea (typewriter/fade).
            f32 dr = r, dg = g, db = b;
            if (honorCharColor && eff.hasColor) { dr = eff.cr; dg = eff.cg; db = eff.cb; }

            if (gw > 0 && gh > 0)
            {
                // Franja per-glifo del {glitch}: dos stamps tintados a ±2px,
                // SOLO en el pase de fill (honorCharColor) — en los 8 pases
                // de outline serían ruido del color del contorno y 3x costo.
                // Mismo batch, mismo atlas: cero superficie GL nueva.
                if (esGlitch && honorCharColor)
                {
                    draw2d::g_batch->draw(
                        src->m_atlasTexID,
                        gx - 2.0f, gy, gw, gh,
                        d_info->uvX0, d_info->uvY0,
                        d_info->uvX1, d_info->uvY1,
                        1.0f, 0.10f, 0.15f, a * 0.55f);
                    draw2d::g_batch->draw(
                        src->m_atlasTexID,
                        gx + 2.0f, gy, gw, gh,
                        d_info->uvX0, d_info->uvY0,
                        d_info->uvX1, d_info->uvY1,
                        0.15f, 0.25f, 1.0f, a * 0.55f);
                }

                draw2d::g_batch->draw(
                    src->m_atlasTexID,   // B: atlas de la fuente del char
                    gx, gy, gw, gh,
                    d_info->uvX0, d_info->uvY0,
                    d_info->uvX1, d_info->uvY1,
                    dr, dg, db, a);
            }

            curX += g_info->advance;
            charIdx++;
            glyphNum++;
        }
    }

    // ── Utilidades ────────────────────────────────────────────────────────

    f32 TextRenderer::measureWidth(const std::string& text) const
    {
        // §UTF-8: decodificar codepoints, NO bytes. El bug viejo iteraba byte
        // a byte: 'ñ' (0xC3 0xB1) se medía como DOS glifos (0xC3→Ã, 0xB1→±),
        // empujando el cursor con un hueco fantasma. (Ñ "parecía" andar porque
        // su 2º byte 0x91 cae en el rango de control no cargado → 0 ancho, y
        // tapaba el bug por casualidad.) Mismo decoder que drawText (§1.45).
        //
        // ⚠ ESE COMENTARIO ES HISTORIA Y SE DEJA COMO ESTABA (§7.328), pero el
        // cuerpo ya no vive acá: era el ÚNICO decoder del motor con guardas de
        // fin, y por eso se le dio nombre (`text::utf8Decodificar`, estrenado
        // por `wrap-mide-bytes`) y ahora lo comparten los tres. El `if (g)
        // w += g->advance` que había abajo era la mitad de
        // `GLIFO-FALTANTE-DIVERGE`: omitía mientras `draw` sustituía por '?'.
        f32 w = 0;
        size_t pos = 0;
        while (pos < text.size())
        {
            i32 cp = -1;
            const size_t paso = text::utf8Decodificar(text, pos, cp);
            if (paso == 0) break;
            pos += paso;
            if (cp < 0) continue;      // líder inválido: saltar, sin sumar
            w += avanceDeEsteAtlas(cp);
        }
        return w;
    }

    f32 TextRenderer::measureWrappedHeight(const std::string& text, f32 maxWidth,
                                           f32 lineSpacing) const
    {
        // Una línea sola no tiene ningún PASO, así que no cobra interlineado.
        // Es el mismo motivo por el que abajo son (lines - 1) extras y no lines:
        // el alto de N líneas es N cajas + (N-1) saltos. Cobrarlo N veces dejaría
        // el historial con un hueco de más al pie de cada entrada.
        if (!isLoaded() || text.empty()) return m_lineHeight;

        // Esto decía "misma lógica de word-wrap que drawTextWrapped" y era una
        // COPIA de ese lazo: coincidían por construcción, y las dos medían por
        // BYTES. Desde `WRAP-MIDE-BYTES` el alto sale del MISMO plan que el
        // dibujo, así que ya no pueden desincronizarse.
        const i32 lines = planDeCorteDeEsteAtlas(text, maxWidth).lineas;

        return lines * m_lineHeight + (lines - 1) * lineSpacing;
    }

    const GlyphInfo* TextRenderer::getGlyph(i32 cp) const
    {
        auto it = m_glyphs.find(cp);
        if (it != m_glyphs.end()) return &it->second;
        return nullptr;
    }

    // ── GLIFO-FALTANTE-DIVERGE (§7.584): la política, con el atlas enchufado ──
    //
    // Antes de esto, la respuesta al glifo ausente vivía en CINCO sitios de
    // este archivo y se contradecía: `draw` sustituía por '?' (`:270`, `:573`)
    // mientras `measureWidth` (`:707`) y las dos entradas del wrap (`:329`,
    // `:490`) lo omitían. El motor dibujaba una cosa y medía otra —336 px de
    // deuda en una sola línea de `test_glitch.ecl`, medidos—.
    //
    // Ahora los cinco piden el avance ACÁ, y la ley está en
    // `engine/core/TextGlifo.h`, que la batería sí puede ver.

    text::BuscarGlifo TextRenderer::buscadorDeEsteAtlas() const
    {
        return [this](i32 cp) -> text::Glifo
        {
            text::Glifo g;
            if (const GlyphInfo* gi = getGlyph(cp))
            {
                g.hay = true;
                g.avance = gi->advance;
            }
            return g;
        };
    }

    f32 TextRenderer::avanceDeEsteAtlas(i32 cp) const
    {
        // El camino caliente es el de siempre: una búsqueda y listo. La ley
        // sólo entra cuando el glifo NO está, que es la rama rara.
        if (const GlyphInfo* gi = getGlyph(cp)) return gi->advance;

        // El plan compartido ya emitio el aviso una vez por perfil/generacion.
        // Un codepoint dinamico fuera del plan conserva la voz local hasta R5.
        if (!m_profilePlan || !m_profilePlan.containsCodepoint(cp))
            avisarAusente(cp);
        return text::avanceDeCodepoint(cp, buscadorDeEsteAtlas(),
                                       m_fontSize * 0.5f, text::politicaDeLaCasa());
    }

    void TextRenderer::avisarAusente(i32 cp) const
    {
        if (cp < 0) return;
        if (!m_ausentes.primeraVez(cp)) return;   // una vez, no una por frame

        // El nombre pelado: la ruta completa no entra en una consola y el
        // creador reconoce el archivo, no el directorio.
        std::string nombre = m_fontPath;
        const size_t barra = nombre.find_last_of("/\\");
        if (barra != std::string::npos) nombre = nombre.substr(barra + 1);
        if (nombre.empty()) nombre = "(fuente sin nombre)";

        char cod[16];
        std::snprintf(cod, sizeof(cod), "U+%04X", (unsigned)cp);

        // El aviso dice lo que el motor HACE, no lo que la ley decía cuando se
        // escribió: si algún día la política pasa a sustituir, esta línea lo
        // cuenta sola.
        const bool sustituye = text::sustituyeElAusente(text::politicaDeLaCasa());
        std::cerr << "[Eclips] Aviso: '" << nombre << "' no tiene glifo para "
                  << cod << "; el texto lo "
                  << (sustituye ? "dibuja como '?'" : "omite") << ".\n";
    }

    void TextRenderer::shutdown()
    {
        if (m_atlasTexID != 0)
        {
            glDeleteTextures(1, &m_atlasTexID);
            m_atlasTexID = 0;
        }
        m_glyphs.clear();
        m_ausentes.olvidar();   // sin atlas no hay nada de que avisar
        m_profilePlan = {};
        m_atlasW = 0;
        m_atlasH = 0;
        m_packLive = false;
        m_packPenX = 2;
        m_packPenY = 2;
        m_packRowHeight = 0;
        if (m_fontSourceStore) m_fontSourceStore->notifyConsumerDropped();
    }

    void TextRenderer::drawTextOutlined(const std::string& text, f32 x, f32 y,
        f32 r, f32 g, f32 b, f32 a,
        f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
        f32 outlineThickness)
    {
        // Render Core Fase 1, paso 6.C — batching real cross-call.
        // Todas las muestras usan la ley pura de OUTLINE-CIRCULO; el relleno
        // sigue siendo una única emisión al final.
        if (!isLoaded()) return;
        if (!draw2d::g_batch) return;

        draw2d::g_batch->begin();
        if (outlineA > 0.0f) {
            for (const auto& offset : outlineCircleOffsets(outlineThickness))
                drawTextPartial_noBatch(text, x + offset.dx, y + offset.dy,
                    -1, outlineR, outlineG, outlineB, outlineA);
        }
        drawTextPartial_noBatch(text, x, y, -1, r, g, b, a);
        draw2d::g_batch->end();
    }

    // F2.C: drawTextOutlined sin begin/end — circular offsets + 1 fill.
    void TextRenderer::drawTextOutlined_noBatch(const std::string& text, f32 x, f32 y,
        f32 r, f32 g, f32 b, f32 a,
        f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
        f32 outlineThickness)
    {
        if (!isLoaded()) return;
        if (outlineA > 0.0f) {
            for (const auto& offset : outlineCircleOffsets(outlineThickness))
                drawTextPartial_noBatch(text, x + offset.dx, y + offset.dy,
                    -1, outlineR, outlineG, outlineB, outlineA);
        }
        drawTextPartial_noBatch(text, x, y, -1, r, g, b, a);
    }

    void TextRenderer::drawTextStrokes_noBatch(const std::string& text, f32 x, f32 y,
        f32 r, f32 g, f32 b, f32 a,
        const std::vector<ResolvedTextStroke>& strokes)
    {
        if (!isLoaded()) return;

        for (const auto& stroke : strokes)
        {
            if (stroke.a <= 0.0f) continue;

            const auto offsets = outlineCircleOffsets(stroke.grosor);
            if (offsets.empty())
            {
                // Grosor cero con offset sirve como sombra dura; (0,0) no
                // añade dibujo visible y no duplica el fill.
                if (stroke.grosor <= 0.0f &&
                    (stroke.dx != 0.0f || stroke.dy != 0.0f))
                {
                    drawTextPartial_noBatch(text, x + stroke.dx, y + stroke.dy,
                        -1, stroke.r, stroke.g, stroke.b, stroke.a * a);
                }
                continue;
            }

            for (const auto& offset : offsets)
            {
                drawTextPartial_noBatch(text,
                    x + stroke.dx + offset.dx,
                    y + stroke.dy + offset.dy,
                    -1, stroke.r, stroke.g, stroke.b, stroke.a * a);
            }
        }

        // La lista completa comparte un solo relleno final.
        drawTextPartial_noBatch(text, x, y, -1, r, g, b, a);
    }

    // Glow de una linea — sesion glow. 3 anillos × 8 stamps a radios
    // crecientes (r, 2r, 3r) con alpha decreciente, fill normal encima.
    // Todo dentro del batch externo (el caller abrio begin/end). Speaker
    // de cartel corto: 24 stamps + 1 fill, todos en el mismo flush.
    void TextRenderer::drawTextGlow_noBatch(const std::string& text, f32 x, f32 y,
        f32 r, f32 g, f32 b, f32 a,
        f32 glowR, f32 glowG, f32 glowB,
        f32 glowRadius, f32 glowIntensity)
    {
        if (!isLoaded()) return;

        if (glowRadius > 0.0f && glowIntensity > 0.0f)
        {
            // GLOW pasada 2 — CP2 (opcion A): muestras en CIRCULO real.
            // La ley circular evita el cuadrado histórico de 8 puntos y sus
            // diagonales a t*sqrt(2); el fill se conserva como una sola pasada.
            // y "copos" a radio grande. Ahora cada anillo reparte N muestras
            // uniformes en angulo via cos/sin a distancia t EXACTA, y N crece
            // con el radio para que las copias siempre se solapen → halo
            // continuo a cualquier radio de titulo. Mantiene 3 anillos y los
            // mismos pesos (no toca el "caracter" del falloff ya validado).
            const f32 ringWeights[3] = { 0.50f, 0.30f, 0.18f };
            const f32 kTwoPi = 6.28318530718f;
            const f32 kSampleSpacing = 2.0f;  // px deseados entre muestras

            for (int ring = 0; ring < 3; ++ring)
            {
                const f32 t = glowRadius * (f32)(ring + 1);   // r, 2r, 3r

                // N proporcional a la circunferencia (2*pi*t / spacing).
                // Clamp [8, 48]: piso = densidad de la version vieja; techo
                // acota el peor caso bien por debajo de MAX_QUADS.
                int n = (int)std::ceil((kTwoPi * t) / kSampleSpacing);
                if (n < 8)  n = 8;
                if (n > 48) n = 48;

                // Normalizacion 8/N: reparte la MISMA energia de la version
                // de 8 stamps entre N muestras (mas muestras solapan mas, sin
                // esto el halo se sobre-satura). ESTA es la perilla visual:
                // halo muy tenue → subir el 8.0f; muy brillante → bajarlo.
                const f32 ga = glowIntensity * ringWeights[ring] * a * (8.0f / (f32)n);

                for (int s = 0; s < n; ++s)
                {
                    const f32 ang = kTwoPi * (f32)s / (f32)n;
                    const f32 dx = std::cos(ang) * t;
                    const f32 dy = std::sin(ang) * t;
                    drawTextPartial_noBatch(text, x + dx, y + dy, -1,
                        glowR, glowG, glowB, ga, /*useInset=*/true);
                }
            }
        }

        // Fill normal encima del halo.
        drawTextPartial_noBatch(text, x, y, -1, r, g, b, a);
    }

    void TextRenderer::drawTextWrappedOutlined(const std::string& text, f32 x, f32 y,
        f32 maxWidth, i32 charCount,
        f32 r, f32 g, f32 b, f32 a,
        f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
        f32 outlineThickness)
    {
        if (outlineA > 0.0f) {
            for (const auto& offset : outlineCircleOffsets(outlineThickness))
                drawTextWrapped(text, x + offset.dx, y + offset.dy,
                    maxWidth, charCount, outlineR, outlineG, outlineB, outlineA);
        }
        drawTextWrapped(text, x, y, maxWidth, charCount, r, g, b, a);
    }

    // F2.C: drawTextWrappedOutlined sin begin/end — circular offsets + 1 fill.
    void TextRenderer::drawTextWrappedOutlined_noBatch(const std::string& text, f32 x, f32 y,
        f32 maxWidth, i32 charCount,
        f32 r, f32 g, f32 b, f32 a,
        f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
        f32 outlineThickness)
    {
        if (!isLoaded()) return;
        if (outlineA > 0.0f) {
            for (const auto& offset : outlineCircleOffsets(outlineThickness))
                drawTextWrapped_noBatch(text, x + offset.dx, y + offset.dy,
                    maxWidth, charCount, outlineR, outlineG, outlineB, outlineA);
        }
        drawTextWrapped_noBatch(text, x, y, maxWidth, charCount, r, g, b, a);
    }

    void TextRenderer::drawTextWrappedRichOutlined(const std::string& text, f32 x, f32 y,
        f32 maxWidth, i32 charCount, f32 time,
        const std::vector<CharEffect>& effects,
        f32 r, f32 g, f32 b, f32 a,
        f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
        f32 outlineThickness,
        FontManager* fm)
    {
        // Render Core Fase 1, paso 6.C — batching real cross-call.
        // La ley geométrica es compartida con las otras cinco variantes y el
        // relleno va una sola vez al final del batch.
        if (!isLoaded()) return;
        if (!draw2d::g_batch) return;

        draw2d::g_batch->begin();
        if (outlineA > 0.0f) {
            for (const auto& offset : outlineCircleOffsets(outlineThickness))
                drawTextWrappedRich_noBatch(text, x + offset.dx, y + offset.dy,
                    maxWidth, charCount, time, effects, outlineR, outlineG,
                    outlineB, outlineA, false, fm);
        }
        drawTextWrappedRich_noBatch(text, x, y, maxWidth, charCount, time, effects, r, g, b, a, true, fm);
        draw2d::g_batch->end();
    }


    // F2.C: drawTextWrappedRichOutlined sin begin/end — N offsets + 1 fill via _noBatch.
    void TextRenderer::drawTextWrappedRichOutlined_noBatch(const std::string& text, f32 x, f32 y,
        f32 maxWidth, i32 charCount, f32 time,
        const std::vector<CharEffect>& effects,
        f32 r, f32 g, f32 b, f32 a,
        f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
        f32 outlineThickness,
        FontManager* fm, f32 lineSpacing)
    {
        if (!isLoaded()) return;
        // El interlineado va a LAS NUEVE pasadas, no sólo al relleno: las ocho
        // estampas del borde recorren el mismo texto con otro origen, así que si
        // no saltaran igual, el borde se despegaría del relleno una línea más
        // abajo en cada salto. Es el mismo motivo por el que `fm` se propaga.
        if (outlineA > 0.0f) {
            for (const auto& offset : outlineCircleOffsets(outlineThickness))
                drawTextWrappedRich_noBatch(text, x + offset.dx, y + offset.dy,
                    maxWidth, charCount, time, effects, outlineR, outlineG,
                    outlineB, outlineA, false, fm, lineSpacing);
        }
        drawTextWrappedRich_noBatch(text, x, y, maxWidth, charCount, time, effects, r, g, b, a, true, fm, lineSpacing);
    }

}
