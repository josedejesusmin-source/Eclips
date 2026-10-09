// ============================================================================
// ECLIPS ENGINE - FontAtlasExpand (CP-R5 · Ampliacion dinamica)
// ============================================================================
// API consumida por harness_atlas_cp_r5.cpp.
// Forks firmados: P1=1a (solo initProfile), P2=2a (rebake 2048), P3=3a
// (estado en TextRenderer), P4 diferido (Ensure sincrono; Begin/Finish
// estructurales), P5=5a (no unificar legacy parcial).
// ============================================================================
#pragma once

#include "TextRenderer.h"
#include "FontSourceStore.h"

#include <SDL_opengl.h>

#include "../../third_party/stb_truetype.h"

#include "FontManager.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eclips
{

struct AtlasExpandStats
{
    int bakeCount = 0;
    int uploadCount = 0;
    int rasterizedGlyphs = 0;
    int requestedUnique = 0;
    int transactions = 0;
    int lastProvenanceSourceIndex = -1;
    bool omittedWhileWaiting = false;
    int omitted = 0;
    int avisos = 0;
    int exceptionalGrowOps = 0;
    bool mixedUvTable = false;
    int droppedAnonymousAtlases = 0;
    std::int64_t ttfDeltaBytes = 0;
};

// CTL-LATENCIA (DA-28 / CP-R5 9.1): min / distribucion / max por fase.
// Gateado por el macro: el harness compila el assert solo cuando existe.
#define ECLIPS_ATLAS_LATENCY_REPORT 1

struct AtlasLatencyPhase
{
    std::string nombre;
    double minMs = 0.0;
    double p50Ms = 0.0;
    double maxMs = 0.0;
    int muestras = 0;
};

struct AtlasLatencyReport
{
    std::vector<AtlasLatencyPhase> fases;
};

namespace detail_latency
{
inline std::unordered_map<std::string, std::vector<double>>& samples()
{
    static std::unordered_map<std::string, std::vector<double>> s;
    return s;
}

inline void note(const char* phase, double ms)
{
    // Ausencia de trabajo != muestra en cero (HO-R5-009 / DA-28 §9).
    if (ms <= 0.0)
        return;
    samples()[phase].push_back(ms);
}

inline double percentile50(std::vector<double> v)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if ((n & 1u) == 1u)
        return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
} // namespace detail_latency

inline AtlasLatencyReport atlasLatencyReport()
{
    AtlasLatencyReport rep;
    static const char* kPhases[] = {
        "evento", "preparacion_cpu", "upload_publicacion", "frame_trabajo"};
    for (const char* name : kPhases)
    {
        AtlasLatencyPhase f;
        f.nombre = name;
        auto it = detail_latency::samples().find(name);
        if (it == detail_latency::samples().end() || it->second.empty())
        {
            // Semilla honesta: sin muestras el harness falla el assert.
            rep.fases.push_back(f);
            continue;
        }
        const auto& v = it->second;
        f.muestras = static_cast<int>(v.size());
        f.minMs = *std::min_element(v.begin(), v.end());
        f.maxMs = *std::max_element(v.begin(), v.end());
        f.p50Ms = detail_latency::percentile50(v);
        rep.fases.push_back(f);
    }
    return rep;
}

// Estado vivo de un rebake excepcional repartido entre Finish (P4=4a).
// Vive en heap via shared_ptr porque Prep se pasa por valor en el bombeo.
struct DeferredExpandState
{
    enum Phase : int
    {
        Build = 0,
        Bake = 1,
        Rgba = 2,
        Upload = 3,
        Commit = 4,
        Done = 5
    };
    Phase phase = Build;
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
    std::vector<unsigned char> atlasPixels;
    std::vector<unsigned char> rgba;
    std::unordered_map<i32, GlyphInfo> newGlyphs;
    std::size_t bakeIndex = 0;
    std::size_t rgbaIndex = 0;
    std::size_t uploadRow = 0;
    int penX = 2;
    int penY = 2;
    int rowHeight = 0;
    u32 newTex = 0;
    bool texAllocated = false;
    std::uint64_t genAntes = 0;
    bool genPrepared = false;
    AtlasExpandStats stats;
    bool failed = false;
};

struct AtlasExpandPrep
{
    bool pending = false;
    bool committed = false;
    bool starved = false;
    TextRenderer* renderer = nullptr;
    std::vector<i32> codepoints;
    std::shared_ptr<DeferredExpandState> defer;
};

struct AtlasUvSnapshot
{
    std::vector<i32> codepoints;
    std::vector<GlyphInfo> glyphs; // paralelo a codepoints
};

// Acceso friend a TextRenderer (declarado en TextRenderer.h).
struct FontAtlasExpandAccess
{
    static bool packLive(const TextRenderer& tr) { return tr.m_packLive; }
    static i32& packPenX(TextRenderer& tr) { return tr.m_packPenX; }
    static i32& packPenY(TextRenderer& tr) { return tr.m_packPenY; }
    static i32& packRowHeight(TextRenderer& tr) { return tr.m_packRowHeight; }
    static i32 packPenX(const TextRenderer& tr) { return tr.m_packPenX; }
    static i32 packPenY(const TextRenderer& tr) { return tr.m_packPenY; }
    static i32 packRowHeight(const TextRenderer& tr) { return tr.m_packRowHeight; }
    static bool& packLiveMut(TextRenderer& tr) { return tr.m_packLive; }

    static u32& atlasTex(TextRenderer& tr) { return tr.m_atlasTexID; }
    static i32& atlasW(TextRenderer& tr) { return tr.m_atlasW; }
    static i32& atlasH(TextRenderer& tr) { return tr.m_atlasH; }
    static i32 atlasW(const TextRenderer& tr) { return tr.m_atlasW; }
    static i32 atlasH(const TextRenderer& tr) { return tr.m_atlasH; }

    static f32 fontSize(const TextRenderer& tr) { return tr.m_fontSize; }
    static f32 kerning(const TextRenderer& tr) { return tr.m_kerning; }
    static FontMeasure measure(const TextRenderer& tr) { return tr.m_fontMeasure; }
    static FontSourceStore* store(TextRenderer& tr) { return tr.m_fontSourceStore; }
    static FontSourceStore* store(const TextRenderer& tr) { return tr.m_fontSourceStore; }

    static TextProfilePlanHandle& plan(TextRenderer& tr) { return tr.m_profilePlan; }
    static const TextProfilePlanHandle& plan(const TextRenderer& tr) { return tr.m_profilePlan; }

    static std::unordered_map<i32, GlyphInfo>& glyphs(TextRenderer& tr) { return tr.m_glyphs; }
    static const std::unordered_map<i32, GlyphInfo>& glyphs(const TextRenderer& tr) { return tr.m_glyphs; }

    static text::RegistroDeAusentes& ausentes(TextRenderer& tr) { return tr.m_ausentes; }
    static void avisar(TextRenderer& tr, i32 cp) { tr.avisarAusente(cp); }

    static f32& ascent(TextRenderer& tr) { return tr.m_ascent; }
    static f32& lineHeight(TextRenderer& tr) { return tr.m_lineHeight; }
    static std::string& fontPath(TextRenderer& tr) { return tr.m_fontPath; }
};

inline AtlasUvSnapshot atlasSnapshotUvs(const TextRenderer& tr)
{
    AtlasUvSnapshot snap;
    const auto& map = FontAtlasExpandAccess::glyphs(tr);
    snap.codepoints.reserve(map.size());
    for (const auto& kv : map)
        snap.codepoints.push_back(kv.first);
    std::sort(snap.codepoints.begin(), snap.codepoints.end());
    snap.glyphs.reserve(snap.codepoints.size());
    for (const i32 cp : snap.codepoints)
        snap.glyphs.push_back(map.at(cp));
    return snap;
}

inline bool atlasUvPrefixIdentical(const AtlasUvSnapshot& before,
                                   const AtlasUvSnapshot& after)
{
    if (before.codepoints.size() > after.codepoints.size())
        return false;
    for (std::size_t i = 0; i < before.codepoints.size(); ++i)
    {
        if (before.codepoints[i] != after.codepoints[i])
            return false;
        const GlyphInfo& a = before.glyphs[i];
        const GlyphInfo& b = after.glyphs[i];
        if (std::memcmp(&a, &b, sizeof(GlyphInfo)) != 0)
            return false;
    }
    return true;
}

namespace detail_expand
{

struct PendingGlyph
{
    i32 cp = 0;
    int sourceIndex = -1;
    const stbtt_fontinfo* font = nullptr;
    f32 scale = 0.0f;
    int glyph = 0;
    int x0 = 0, y0 = 0, gw = 0, gh = 0;
    int advance = 0;
    int penX = 0, penY = 0;
};

inline TextProfilePlanHandle probePlan(TextRenderer& tr, i32 cp)
{
    FontSourceStore* store = FontAtlasExpandAccess::store(tr);
    const TextProfilePlanHandle& cur = FontAtlasExpandAccess::plan(tr);
    if (!store || !cur)
        return {};
    // Ya conocido en el plan actual (cubierto o omitido memoizado).
    if (cur.containsCodepoint(cp))
        return cur;

    TextProfileSpec probe = cur.spec();
    probe.repertoire = {cp};
    return store->acquirePlan(probe);
}

inline bool tryPlaceOne(int atlasSize, int& penX, int& penY, int& rowHeight,
                        int gw, int gh, int& outX, int& outY)
{
    if (penX + gw + 2 >= atlasSize)
    {
        penX = 2;
        penY += rowHeight + 2;
        rowHeight = 0;
    }
    if (penY + gh + 2 >= atlasSize)
        return false;
    outX = penX;
    outY = penY;
    penX += gw + 2;
    rowHeight = std::max(rowHeight, gh + 2);
    return true;
}

inline GlyphInfo makeInfo(const PendingGlyph& d, int atlasW, int atlasH, f32 kerning)
{
    GlyphInfo info{};
    info.uvX0 = static_cast<f32>(d.penX) / atlasW;
    info.uvY0 = static_cast<f32>(d.penY) / atlasH;
    info.uvX1 = static_cast<f32>(d.penX + d.gw) / atlasW;
    info.uvY1 = static_cast<f32>(d.penY + d.gh) / atlasH;
    info.uvX0i = (static_cast<f32>(d.penX) + 0.5f) / atlasW;
    info.uvY0i = (static_cast<f32>(d.penY) + 0.5f) / atlasH;
    info.uvX1i = (static_cast<f32>(d.penX + d.gw) - 0.5f) / atlasW;
    info.uvY1i = (static_cast<f32>(d.penY + d.gh) - 0.5f) / atlasH;
    info.offsetX = static_cast<f32>(d.x0);
    info.offsetY = static_cast<f32>(d.y0);
    info.width = static_cast<f32>(d.gw);
    info.height = static_cast<f32>(d.gh);
    info.advance = d.advance * d.scale + kerning;
    return info;
}

inline void uploadSubGlyph(TextRenderer& tr, const PendingGlyph& d,
                           AtlasExpandStats& stats)
{
    const int gw = d.gw;
    const int gh = d.gh;
    if (gw <= 0 || gh <= 0)
    {
        FontAtlasExpandAccess::glyphs(tr)[d.cp] =
            makeInfo(d, FontAtlasExpandAccess::atlasW(tr),
                     FontAtlasExpandAccess::atlasH(tr),
                     FontAtlasExpandAccess::kerning(tr));
        return;
    }

    std::vector<unsigned char> alpha(
        static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), 0);
    stbtt_MakeGlyphBitmap(d.font, alpha.data(), gw, gh, gw, d.scale, d.scale,
                          d.glyph);
    ++stats.bakeCount;
    ++stats.rasterizedGlyphs;

    std::vector<unsigned char> rgba(
        static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh) * 4u);
    for (int i = 0; i < gw * gh; ++i)
    {
        rgba[static_cast<std::size_t>(i) * 4u + 0] = 255;
        rgba[static_cast<std::size_t>(i) * 4u + 1] = 255;
        rgba[static_cast<std::size_t>(i) * 4u + 2] = 255;
        rgba[static_cast<std::size_t>(i) * 4u + 3] = alpha[static_cast<std::size_t>(i)];
    }

    glBindTexture(GL_TEXTURE_2D, FontAtlasExpandAccess::atlasTex(tr));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, d.penX, d.penY, gw, gh, GL_RGBA,
                    GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    ++stats.uploadCount;

    FontAtlasExpandAccess::glyphs(tr)[d.cp] =
        makeInfo(d, FontAtlasExpandAccess::atlasW(tr),
                 FontAtlasExpandAccess::atlasH(tr),
                 FontAtlasExpandAccess::kerning(tr));
}

inline bool buildDescriptor(TextRenderer& tr, i32 cp, PendingGlyph& out,
                            AtlasExpandStats& stats)
{
    TextProfilePlanHandle probed = probePlan(tr, cp);
    if (!probed)
    {
        ++stats.omitted;
        const std::size_t before = FontAtlasExpandAccess::ausentes(tr).cuantos();
        FontAtlasExpandAccess::avisar(tr, cp);
        if (FontAtlasExpandAccess::ausentes(tr).cuantos() > before)
            ++stats.avisos;
        return false;
    }

    const int src = probed.sourceIndex(cp);
    const stbtt_fontinfo* font = probed.fontInfoFor(cp);
    if (src < 0 || !font)
    {
        ++stats.omitted;
        const std::size_t before = FontAtlasExpandAccess::ausentes(tr).cuantos();
        FontAtlasExpandAccess::avisar(tr, cp);
        if (FontAtlasExpandAccess::ausentes(tr).cuantos() > before)
            ++stats.avisos;
        return false;
    }

    out.cp = cp;
    out.sourceIndex = src;
    out.font = font;
    out.scale = probed.scaleFor(cp);
    out.glyph = stbtt_FindGlyphIndex(font, cp);
    if (out.glyph == 0 && cp != 32)
    {
        ++stats.omitted;
        const std::size_t before = FontAtlasExpandAccess::ausentes(tr).cuantos();
        FontAtlasExpandAccess::avisar(tr, cp);
        if (FontAtlasExpandAccess::ausentes(tr).cuantos() > before)
            ++stats.avisos;
        return false;
    }

    int leftBearing = 0;
    stbtt_GetGlyphHMetrics(font, out.glyph, &out.advance, &leftBearing);
    int x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(font, out.glyph, out.scale, out.scale, &out.x0,
                            &out.y0, &x1, &y1);
    out.gw = x1 - out.x0;
    out.gh = y1 - out.y0;
    stats.lastProvenanceSourceIndex = src;
    return true;
}

inline void refreshPlan(TextRenderer& tr)
{
    FontSourceStore* store = FontAtlasExpandAccess::store(tr);
    const TextProfilePlanHandle& cur = FontAtlasExpandAccess::plan(tr);
    if (!store || !cur)
        return;
    TextProfileSpec expanded = cur.spec();
    expanded.repertoire.clear();
    for (const auto& kv : FontAtlasExpandAccess::glyphs(tr))
        expanded.repertoire.push_back(kv.first);
    std::sort(expanded.repertoire.begin(), expanded.repertoire.end());
    expanded.repertoire.erase(
        std::unique(expanded.repertoire.begin(), expanded.repertoire.end()),
        expanded.repertoire.end());
    FontAtlasExpandAccess::plan(tr) = store->acquirePlan(expanded);
    FontAtlasExpandAccess::ascent(tr) = FontAtlasExpandAccess::plan(tr).ascent();
    FontAtlasExpandAccess::lineHeight(tr) =
        FontAtlasExpandAccess::plan(tr).lineHeight();
}

// P2=2a: rebake completo a 2048 (misma via que initProfile al desbordar).
inline bool exceptionalRebake2048(TextRenderer& tr,
                                  const std::vector<PendingGlyph>& newcomers,
                                  AtlasExpandStats& stats)
{
    TextProfilePlanHandle& plan = FontAtlasExpandAccess::plan(tr);
    if (!plan)
        return false;

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
    descriptors.reserve(FontAtlasExpandAccess::glyphs(tr).size() +
                        newcomers.size());

    auto pushUnique = [&](Descriptor d) {
        for (const auto& e : descriptors)
            if (e.cp == d.cp)
                return;
        descriptors.push_back(d);
    };

    // Glifos ya publicados: resolver via plan actual.
    for (const auto& kv : FontAtlasExpandAccess::glyphs(tr))
    {
        const i32 cp = kv.first;
        const stbtt_fontinfo* font = plan.fontInfoFor(cp);
        if (!font)
            continue;
        Descriptor d;
        d.cp = cp;
        d.font = font;
        d.scale = plan.scaleFor(cp);
        d.glyph = stbtt_FindGlyphIndex(font, cp);
        if (d.glyph == 0 && cp != 32)
            continue;
        int leftBearing = 0;
        stbtt_GetGlyphHMetrics(font, d.glyph, &d.advance, &leftBearing);
        int x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBox(font, d.glyph, d.scale, d.scale, &d.x0, &d.y0,
                                &x1, &y1);
        d.gw = x1 - d.x0;
        d.gh = y1 - d.y0;
        pushUnique(d);
    }
    // Newcomers ya traen font/metrics resueltos (buildDescriptor).
    for (const auto& n : newcomers)
    {
        Descriptor d;
        d.cp = n.cp;
        d.font = n.font;
        d.scale = n.scale;
        d.glyph = n.glyph;
        d.x0 = n.x0;
        d.y0 = n.y0;
        d.gw = n.gw;
        d.gh = n.gh;
        d.advance = n.advance;
        pushUnique(d);
    }

    std::stable_sort(descriptors.begin(), descriptors.end(),
                     [](const Descriptor& a, const Descriptor& b) {
                         if (a.gh != b.gh)
                             return a.gh > b.gh;
                         if (a.gw != b.gw)
                             return a.gw > b.gw;
                         return a.cp < b.cp;
                     });

    const int atlasSize = 2048;
    int penX = 2, penY = 2, rowHeight = 0;
    for (auto& d : descriptors)
    {
        if (penX + d.gw + 2 >= atlasSize)
        {
            penX = 2;
            penY += rowHeight + 2;
            rowHeight = 0;
        }
        if (penY + d.gh + 2 >= atlasSize)
            return false;
        d.penX = penX;
        d.penY = penY;
        penX += d.gw + 2;
        rowHeight = std::max(rowHeight, d.gh + 2);
    }

    std::vector<unsigned char> atlasPixels(
        static_cast<std::size_t>(atlasSize) * atlasSize, 0);
    std::unordered_map<i32, GlyphInfo> newGlyphs;
    const f32 kern = FontAtlasExpandAccess::kerning(tr);
    const auto tCpu0 = std::chrono::steady_clock::now();
    for (const auto& d : descriptors)
    {
        if (d.gw > 0 && d.gh > 0)
        {
            stbtt_MakeGlyphBitmap(
                d.font, atlasPixels.data() + d.penX + d.penY * atlasSize, d.gw,
                d.gh, atlasSize, d.scale, d.scale, d.glyph);
            ++stats.bakeCount;
            ++stats.rasterizedGlyphs;
        }
        PendingGlyph tmp;
        tmp.cp = d.cp;
        tmp.penX = d.penX;
        tmp.penY = d.penY;
        tmp.gw = d.gw;
        tmp.gh = d.gh;
        tmp.x0 = d.x0;
        tmp.y0 = d.y0;
        tmp.advance = d.advance;
        tmp.scale = d.scale;
        newGlyphs[d.cp] = makeInfo(tmp, atlasSize, atlasSize, kern);
    }
    detail_latency::note(
        "preparacion_cpu",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - tCpu0)
            .count());

    // P2=2a: abrir generacion nueva (prepare/finalize/commit de CP-R4)
    // ANTES de publicar. La textura vieja sigue valida hasta el commit.
    FontSourceStore* store = FontAtlasExpandAccess::store(tr);
    const std::uint64_t genAntes =
        store ? store->activeGeneration() : 0;
    if (store)
    {
        const std::uint64_t opened = store->prepareGeneration();
        if (opened == 0 && store->pendingGeneration() == 0)
            return false;
    }

    const auto tUpload0 = std::chrono::steady_clock::now();
    u32 newTex = 0;
    glGenTextures(1, &newTex);
    glBindTexture(GL_TEXTURE_2D, newTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::vector<unsigned char> rgba(
        static_cast<std::size_t>(atlasSize) * atlasSize * 4u);
    for (std::size_t i = 0; i < atlasPixels.size(); ++i)
    {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = atlasPixels[i];
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlasSize, atlasSize, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    ++stats.uploadCount;
    detail_latency::note(
        "upload_publicacion",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - tUpload0)
            .count());

    // HO-R5-010: NO mutar el renderer vivo hasta commit exitoso.
    // Antes se tocaban ~11 campos y el rollback solo restauraba atlasTex.
    if (store)
    {
        if (!store->finalizePending() || !store->commitGeneration())
        {
            store->rollbackGeneration();
            if (newTex != 0)
                glDeleteTextures(1, &newTex);
            return false;
        }
    }

    // Commit prospero: publicar al renderer. La textura vieja sigue viva
    // hasta el glDelete de abajo (CP-R4).
    const u32 oldTex = FontAtlasExpandAccess::atlasTex(tr);
    FontAtlasExpandAccess::atlasTex(tr) = newTex;
    FontAtlasExpandAccess::atlasW(tr) = atlasSize;
    FontAtlasExpandAccess::atlasH(tr) = atlasSize;
    FontAtlasExpandAccess::glyphs(tr).swap(newGlyphs);

    FontAtlasExpandAccess::packPenX(tr) = penX;
    FontAtlasExpandAccess::packPenY(tr) = penY;
    FontAtlasExpandAccess::packRowHeight(tr) = rowHeight;
    FontAtlasExpandAccess::packLiveMut(tr) = true;

    // HL-R5-005: rebuild excepcional SI olvida; el delta ordinario no.
    FontAtlasExpandAccess::ausentes(tr).olvidar();

    // refreshPlan DESPUES del commit: m_profilePlan retenia el plan viejo
    // (use_count>1) durante retireOrphans, asi no se vacia el TTF de la
    // gen viva. Conserva spec.generation del perfil (HALLAZGO evitado).
    refreshPlan(tr);

    // Corte publicado: ahora si se puede retirar la textura anterior.
    if (oldTex != 0)
        glDeleteTextures(1, &oldTex);

    // HL-R5-004: contabilizar (y soltar) atlas anonimos getSized de la gen previa.
    if (store)
    {
        if (FontManager* fm = fontManagerForStore(store))
            stats.droppedAnonymousAtlases =
                fm->dropAnonymousAtlasesOf(genAntes);
    }

    ++stats.exceptionalGrowOps;
    stats.mixedUvTable = false;
    return true;
}

inline AtlasExpandStats ensureImpl(TextRenderer& tr, const std::vector<i32>& raw)
{
    const auto tFrame0 = std::chrono::steady_clock::now();
    const auto tEvent0 = tFrame0;
    AtlasExpandStats stats;
    // HO-R5-001: cero transacciones si no hubo trabajo real.
    stats.transactions = 0;

    std::vector<i32> unique;
    unique.reserve(raw.size());
    std::unordered_set<i32> seen;
    for (const i32 cp : raw)
    {
        if (seen.insert(cp).second)
            unique.push_back(cp);
    }
    stats.requestedUnique = static_cast<int>(unique.size());
    detail_latency::note(
        "evento",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - tEvent0)
            .count());

    if (!tr.isLoaded() || !FontAtlasExpandAccess::packLive(tr) ||
        !FontAtlasExpandAccess::plan(tr) || !FontAtlasExpandAccess::store(tr))
    {
        // Camino legacy (init, sin store/colocacion): P1=1a no migra, pero
        // DA-28 exige OMITIR + AVISAR — callarse esta prohibido (HO-R5-004).
        for (const i32 cp : unique)
        {
            if (FontAtlasExpandAccess::glyphs(tr).count(cp))
                continue;
            ++stats.omitted;
            const std::size_t before =
                FontAtlasExpandAccess::ausentes(tr).cuantos();
            FontAtlasExpandAccess::avisar(tr, cp);
            if (FontAtlasExpandAccess::ausentes(tr).cuantos() > before)
                ++stats.avisos;
            std::cerr << "[FontAtlasExpand] Aviso: ampliacion dinamica no "
                         "disponible en renderer legacy (sin initProfile); "
                         "codepoint U+"
                      << std::hex << cp << std::dec << " omitido."
                      << std::endl;
        }
        detail_latency::note(
            "frame_trabajo",
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tFrame0)
                .count());
        return stats;
    }

    FontSourceStore* store = FontAtlasExpandAccess::store(tr);
    const std::size_t ttfBefore =
        store->metrics(store->activeGeneration()).residentTtfBytes;

    std::vector<PendingGlyph> pending;
    for (const i32 cp : unique)
    {
        if (FontAtlasExpandAccess::glyphs(tr).count(cp))
            continue; // ya presente: cero bake/upload

        PendingGlyph g;
        if (!buildDescriptor(tr, cp, g, stats))
            continue;
        pending.push_back(g);
    }

    if (pending.empty())
    {
        stats.ttfDeltaBytes = static_cast<std::int64_t>(
            store->metrics(store->activeGeneration()).residentTtfBytes) -
            static_cast<std::int64_t>(ttfBefore);
        detail_latency::note(
            "frame_trabajo",
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tFrame0)
                .count());
        return stats;
    }

    // Hubo glifos nuevos que publicar: una transaccion real (HO-R5-001).
    stats.transactions = 1;

    // Intento incremental sobre el cursor vivo (UV previas intactas).
    int penX = FontAtlasExpandAccess::packPenX(tr);
    int penY = FontAtlasExpandAccess::packPenY(tr);
    int rowH = FontAtlasExpandAccess::packRowHeight(tr);
    const int atlasSize = FontAtlasExpandAccess::atlasW(tr);
    bool fits = true;
    for (auto& g : pending)
    {
        if (!tryPlaceOne(atlasSize, penX, penY, rowH, g.gw, g.gh, g.penX,
                         g.penY))
        {
            fits = false;
            break;
        }
    }

    if (!fits)
    {
        // P2=2a: si ya estamos en 2048 y no cabe, starved conceptual;
        // si estamos en 1024, rebake excepcional a 2048.
        if (atlasSize >= 2048)
        {
            // No hay generacion mayor en este lote; omitir los que no caben.
            for (const auto& g : pending)
            {
                ++stats.omitted;
                (void)g;
            }
            stats.ttfDeltaBytes = static_cast<std::int64_t>(
                store->metrics(store->activeGeneration()).residentTtfBytes) -
                static_cast<std::int64_t>(ttfBefore);
            detail_latency::note(
                "frame_trabajo",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - tFrame0)
                    .count());
            return stats;
        }
        if (!exceptionalRebake2048(tr, pending, stats))
        {
            for (std::size_t i = 0; i < pending.size(); ++i)
                ++stats.omitted;
        }
        else
        {
            // lastProvenance from last newcomer
            if (!pending.empty())
                stats.lastProvenanceSourceIndex = pending.back().sourceIndex;
        }
        stats.ttfDeltaBytes = static_cast<std::int64_t>(
            store->metrics(store->activeGeneration()).residentTtfBytes) -
            static_cast<std::int64_t>(ttfBefore);
        detail_latency::note(
            "frame_trabajo",
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tFrame0)
                .count());
        return stats;
    }

    // Incremental: publicar glifo a glifo con SubImage (UV previas identicas).
    const auto tCpu0 = std::chrono::steady_clock::now();
    for (auto& g : pending)
        uploadSubGlyph(tr, g, stats);
    detail_latency::note(
        "preparacion_cpu",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - tCpu0)
            .count());

    FontAtlasExpandAccess::packPenX(tr) = penX;
    FontAtlasExpandAccess::packPenY(tr) = penY;
    FontAtlasExpandAccess::packRowHeight(tr) = rowH;
    // Delta ordinario: NO olvidar ausentes (HL-R5-005).
    refreshPlan(tr);

    if (!pending.empty())
        stats.lastProvenanceSourceIndex = pending.back().sourceIndex;

    stats.ttfDeltaBytes = static_cast<std::int64_t>(
        store->metrics(store->activeGeneration()).residentTtfBytes) -
        static_cast<std::int64_t>(ttfBefore);
    detail_latency::note(
        "frame_trabajo",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - tFrame0)
            .count());
    return stats;
}

inline bool wouldNeedExceptional(TextRenderer& tr, const std::vector<i32>& raw)
{
    if (!tr.isLoaded() || !FontAtlasExpandAccess::packLive(tr) ||
        !FontAtlasExpandAccess::plan(tr) || !FontAtlasExpandAccess::store(tr))
        return false;
    if (FontAtlasExpandAccess::atlasW(tr) >= 2048)
        return false;

    std::vector<i32> unique;
    std::unordered_set<i32> seen;
    for (const i32 cp : raw)
        if (seen.insert(cp).second)
            unique.push_back(cp);

    // Sondeo silencioso: no avisar ni mutar m_ausentes (solo cabida).
    std::vector<PendingGlyph> pending;
    for (const i32 cp : unique)
    {
        if (FontAtlasExpandAccess::glyphs(tr).count(cp))
            continue;
        TextProfilePlanHandle probed = probePlan(tr, cp);
        if (!probed)
            continue;
        const stbtt_fontinfo* font = probed.fontInfoFor(cp);
        if (!font)
            continue;
        PendingGlyph g;
        g.cp = cp;
        g.font = font;
        g.scale = probed.scaleFor(cp);
        g.glyph = stbtt_FindGlyphIndex(font, cp);
        if (g.glyph == 0 && cp != 32)
            continue;
        int leftBearing = 0;
        stbtt_GetGlyphHMetrics(font, g.glyph, &g.advance, &leftBearing);
        int x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBox(font, g.glyph, g.scale, g.scale, &g.x0, &g.y0,
                                &x1, &y1);
        g.gw = x1 - g.x0;
        g.gh = y1 - g.y0;
        pending.push_back(g);
    }
    if (pending.empty())
        return false;

    int penX = FontAtlasExpandAccess::packPenX(tr);
    int penY = FontAtlasExpandAccess::packPenY(tr);
    int rowH = FontAtlasExpandAccess::packRowHeight(tr);
    const int atlasSize = FontAtlasExpandAccess::atlasW(tr);
    for (auto& g : pending)
    {
        if (!tryPlaceOne(atlasSize, penX, penY, rowH, g.gw, g.gh, g.penX,
                         g.penY))
            return true;
    }
    return false;
}


// P4=4a: un paso presupuestado del rebake excepcional. Diferir = repartir,
// no descartar (HO-R5-008). Presupuesto de trabajo ~15 ms (margen bajo 16.67
// con glFinish del harness). Si un solo atomo irreducible excediera el
// presupuesto, eso seria HALLAZGO — no se cierra la prep vacia.
inline bool deferredExceptionalStep(TextRenderer& tr,
                                    DeferredExpandState& st,
                                    const std::vector<i32>& raw)
{
    constexpr double kBudgetMs = 10.0; // margen bajo 16.67 tras glFinish del harness
    constexpr int kAtlas = 2048;
    const auto tFrame0 = std::chrono::steady_clock::now();
    auto elapsed = [&]() {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - tFrame0)
            .count();
    };
    auto budgetLeft = [&]() { return elapsed() < kBudgetMs; };

    if (st.failed)
        return true;

    while (budgetLeft() && st.phase != DeferredExpandState::Done && !st.failed)
    {
        if (st.phase == DeferredExpandState::Build)
        {
            const auto tEvent0 = std::chrono::steady_clock::now();
            const auto t0 = tEvent0;
            TextProfilePlanHandle& plan = FontAtlasExpandAccess::plan(tr);
            if (!plan || !FontAtlasExpandAccess::store(tr))
            {
                st.failed = true;
                st.phase = DeferredExpandState::Done;
                break;
            }

            std::vector<i32> unique;
            std::unordered_set<i32> seen;
            for (const i32 cp : raw)
                if (seen.insert(cp).second)
                    unique.push_back(cp);
            st.stats.requestedUnique = static_cast<int>(unique.size());
            detail_latency::note(
                "evento",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - tEvent0)
                    .count());

            std::vector<PendingGlyph> newcomers;
            for (const i32 cp : unique)
            {
                if (FontAtlasExpandAccess::glyphs(tr).count(cp))
                    continue;
                PendingGlyph g;
                if (!buildDescriptor(tr, cp, g, st.stats))
                    continue;
                newcomers.push_back(g);
            }
            if (newcomers.empty())
            {
                st.phase = DeferredExpandState::Done;
                st.stats.transactions = 0;
                break;
            }
            st.stats.transactions = 1;

            st.descriptors.reserve(FontAtlasExpandAccess::glyphs(tr).size() +
                                   newcomers.size());
            auto pushUnique = [&](DeferredExpandState::Descriptor d) {
                for (const auto& e : st.descriptors)
                    if (e.cp == d.cp)
                        return;
                st.descriptors.push_back(d);
            };
            for (const auto& kv : FontAtlasExpandAccess::glyphs(tr))
            {
                const i32 cp = kv.first;
                const stbtt_fontinfo* font = plan.fontInfoFor(cp);
                if (!font)
                    continue;
                DeferredExpandState::Descriptor d;
                d.cp = cp;
                d.font = font;
                d.scale = plan.scaleFor(cp);
                d.glyph = stbtt_FindGlyphIndex(font, cp);
                if (d.glyph == 0 && cp != 32)
                    continue;
                int leftBearing = 0;
                stbtt_GetGlyphHMetrics(font, d.glyph, &d.advance, &leftBearing);
                int x1 = 0, y1 = 0;
                stbtt_GetGlyphBitmapBox(font, d.glyph, d.scale, d.scale, &d.x0,
                                        &d.y0, &x1, &y1);
                d.gw = x1 - d.x0;
                d.gh = y1 - d.y0;
                pushUnique(d);
            }
            for (const auto& n : newcomers)
            {
                DeferredExpandState::Descriptor d;
                d.cp = n.cp;
                d.font = n.font;
                d.scale = n.scale;
                d.glyph = n.glyph;
                d.x0 = n.x0;
                d.y0 = n.y0;
                d.gw = n.gw;
                d.gh = n.gh;
                d.advance = n.advance;
                pushUnique(d);
            }
            std::stable_sort(
                st.descriptors.begin(), st.descriptors.end(),
                [](const DeferredExpandState::Descriptor& a,
                   const DeferredExpandState::Descriptor& b) {
                    if (a.gh != b.gh)
                        return a.gh > b.gh;
                    if (a.gw != b.gw)
                        return a.gw > b.gw;
                    return a.cp < b.cp;
                });
            st.penX = 2;
            st.penY = 2;
            st.rowHeight = 0;
            for (auto& d : st.descriptors)
            {
                if (st.penX + d.gw + 2 >= kAtlas)
                {
                    st.penX = 2;
                    st.penY += st.rowHeight + 2;
                    st.rowHeight = 0;
                }
                if (st.penY + d.gh + 2 >= kAtlas)
                {
                    st.failed = true;
                    st.phase = DeferredExpandState::Done;
                    break;
                }
                d.penX = st.penX;
                d.penY = st.penY;
                st.penX += d.gw + 2;
                st.rowHeight = std::max(st.rowHeight, d.gh + 2);
            }
            if (st.failed)
                break;
            st.atlasPixels.assign(static_cast<std::size_t>(kAtlas) *
                                      static_cast<std::size_t>(kAtlas),
                                  0);
            st.rgba.assign(st.atlasPixels.size() * 4u, 0);
            st.bakeIndex = 0;
            st.rgbaIndex = 0;
            st.uploadRow = 0;
            st.phase = DeferredExpandState::Bake;
            detail_latency::note(
                "preparacion_cpu",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count());
            continue;
        }

        if (st.phase == DeferredExpandState::Bake)
        {
            const auto t0 = std::chrono::steady_clock::now();
            const f32 kern = FontAtlasExpandAccess::kerning(tr);
            while (st.bakeIndex < st.descriptors.size() && budgetLeft())
            {
                const auto& d = st.descriptors[st.bakeIndex++];
                if (d.gw > 0 && d.gh > 0)
                {
                    stbtt_MakeGlyphBitmap(
                        d.font,
                        st.atlasPixels.data() + d.penX + d.penY * kAtlas, d.gw,
                        d.gh, kAtlas, d.scale, d.scale, d.glyph);
                    ++st.stats.bakeCount;
                    ++st.stats.rasterizedGlyphs;
                }
                PendingGlyph tmp;
                tmp.cp = d.cp;
                tmp.penX = d.penX;
                tmp.penY = d.penY;
                tmp.gw = d.gw;
                tmp.gh = d.gh;
                tmp.x0 = d.x0;
                tmp.y0 = d.y0;
                tmp.advance = d.advance;
                tmp.scale = d.scale;
                st.newGlyphs[d.cp] = makeInfo(tmp, kAtlas, kAtlas, kern);
            }
            detail_latency::note(
                "preparacion_cpu",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count());
            if (st.bakeIndex < st.descriptors.size())
                break; // presupuesto agotado a mitad de Bake
            st.phase = DeferredExpandState::Rgba;
            continue;
        }

        if (st.phase == DeferredExpandState::Rgba)
        {
            const auto t0 = std::chrono::steady_clock::now();
            const std::size_t n = st.atlasPixels.size();
            constexpr std::size_t kQuota = 256u * 1024u;
            std::size_t done = 0;
            while (st.rgbaIndex < n && done < kQuota && budgetLeft())
            {
                const std::size_t i = st.rgbaIndex++;
                st.rgba[i * 4 + 0] = 255;
                st.rgba[i * 4 + 1] = 255;
                st.rgba[i * 4 + 2] = 255;
                st.rgba[i * 4 + 3] = st.atlasPixels[i];
                ++done;
            }
            detail_latency::note(
                "upload_publicacion",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count());
            if (st.rgbaIndex < n)
                break;
            st.atlasPixels.clear();
            st.atlasPixels.shrink_to_fit();
            st.phase = DeferredExpandState::Upload;
            continue;
        }

        if (st.phase == DeferredExpandState::Upload)
        {
            const auto t0 = std::chrono::steady_clock::now();
            if (!st.texAllocated)
            {
                glGenTextures(1, &st.newTex);
                glBindTexture(GL_TEXTURE_2D, st.newTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlas, kAtlas, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                st.texAllocated = true;
                st.uploadRow = 0;
                // Si la sola reserva ya comio el presupuesto, salir.
                if (!budgetLeft())
                {
                    detail_latency::note(
                        "upload_publicacion",
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - t0)
                            .count());
                    break;
                }
            }
            constexpr int kRows = 32;
            while (static_cast<int>(st.uploadRow) < kAtlas && budgetLeft())
            {
                const int rows =
                    std::min(kRows, kAtlas - static_cast<int>(st.uploadRow));
                const unsigned char* src =
                    st.rgba.data() +
                    (st.uploadRow * static_cast<std::size_t>(kAtlas) * 4u);
                glBindTexture(GL_TEXTURE_2D, st.newTex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0,
                                static_cast<int>(st.uploadRow), kAtlas, rows,
                                GL_RGBA, GL_UNSIGNED_BYTE, src);
                st.uploadRow += static_cast<std::size_t>(rows);
            }
            glBindTexture(GL_TEXTURE_2D, 0);
            detail_latency::note(
                "upload_publicacion",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count());
            if (static_cast<int>(st.uploadRow) < kAtlas)
                break;
            ++st.stats.uploadCount;
            st.rgba.clear();
            st.rgba.shrink_to_fit();
            st.phase = DeferredExpandState::Commit;
            continue;
        }

        if (st.phase == DeferredExpandState::Commit)
        {
            const auto t0 = std::chrono::steady_clock::now();
            FontSourceStore* store = FontAtlasExpandAccess::store(tr);
            st.genAntes = store ? store->activeGeneration() : 0;
            if (store && !st.genPrepared)
            {
                const std::uint64_t opened = store->prepareGeneration();
                if (opened == 0 && store->pendingGeneration() == 0)
                {
                    st.failed = true;
                    if (st.newTex != 0)
                        glDeleteTextures(1, &st.newTex);
                    st.newTex = 0;
                    st.phase = DeferredExpandState::Done;
                    break;
                }
                st.genPrepared = true;
            }

            // HO-R5-010: commit primero; mutaciones al renderer solo si prospera.
            // refreshPlan despues: plan vivo retenido (use_count>1) en retireOrphans.
            if (store)
            {
                if (!store->finalizePending() || !store->commitGeneration())
                {
                    store->rollbackGeneration();
                    if (st.newTex != 0)
                        glDeleteTextures(1, &st.newTex);
                    st.newTex = 0;
                    st.failed = true;
                    st.phase = DeferredExpandState::Done;
                    break;
                }
            }

            const u32 oldTex = FontAtlasExpandAccess::atlasTex(tr);
            FontAtlasExpandAccess::atlasTex(tr) = st.newTex;
            FontAtlasExpandAccess::atlasW(tr) = kAtlas;
            FontAtlasExpandAccess::atlasH(tr) = kAtlas;
            FontAtlasExpandAccess::glyphs(tr).swap(st.newGlyphs);
            FontAtlasExpandAccess::packPenX(tr) = st.penX;
            FontAtlasExpandAccess::packPenY(tr) = st.penY;
            FontAtlasExpandAccess::packRowHeight(tr) = st.rowHeight;
            FontAtlasExpandAccess::packLiveMut(tr) = true;
            FontAtlasExpandAccess::ausentes(tr).olvidar();
            refreshPlan(tr);

            if (oldTex != 0)
                glDeleteTextures(1, &oldTex);
            if (store)
            {
                if (FontManager* fm = fontManagerForStore(store))
                    st.stats.droppedAnonymousAtlases =
                        fm->dropAnonymousAtlasesOf(st.genAntes);
            }
            ++st.stats.exceptionalGrowOps;
            st.stats.mixedUvTable = false;
            st.newTex = 0;
            st.phase = DeferredExpandState::Done;
            detail_latency::note(
                "upload_publicacion",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count());
            break;
        }

        break; // fase desconocida
    }

    const double frameMs = elapsed();
    if (frameMs > 0.0)
        detail_latency::note("frame_trabajo", frameMs);

    return st.phase == DeferredExpandState::Done;
}


} // namespace detail_expand

inline AtlasExpandStats atlasExpandEnsure(TextRenderer& tr,
                                          const std::vector<i32>& codepoints)
{
    return detail_expand::ensureImpl(tr, codepoints);
}

inline AtlasExpandStats atlasExpandEnsure(TextRenderer& tr,
                                          std::initializer_list<i32> codepoints)
{
    return detail_expand::ensureImpl(
        tr, std::vector<i32>(codepoints.begin(), codepoints.end()));
}

inline AtlasExpandPrep atlasExpandBegin(TextRenderer& tr,
                                        std::initializer_list<i32> codepoints)
{
    AtlasExpandPrep prep;
    prep.renderer = &tr;
    prep.codepoints.assign(codepoints.begin(), codepoints.end());
    prep.pending = true;
    prep.committed = false;
    prep.starved = false;
    // P4 diferido: no publica aqui; la generacion activa sigue coherente.
    return prep;
}

inline AtlasExpandPrep atlasExpandBegin(TextRenderer& tr,
                                        const std::vector<i32>& codepoints)
{
    AtlasExpandPrep prep;
    prep.renderer = &tr;
    prep.codepoints = codepoints;
    prep.pending = true;
    prep.committed = false;
    prep.starved = false;
    return prep;
}

inline AtlasExpandPrep atlasExpandFinish(AtlasExpandPrep prep)
{
    if (!prep.renderer || !prep.pending)
    {
        prep.starved = true;
        return prep;
    }

    // P4=4a: rebake excepcional (~47 ms) se REPARTE en pasos <=16.67 ms.
    // Diferir = publicar en cuotas (HO-R5-008), no cerrar la prep vacia.
    const bool needsEx =
        prep.defer ||
        detail_expand::wouldNeedExceptional(*prep.renderer, prep.codepoints);
    if (needsEx)
    {
        if (!prep.defer)
            prep.defer = std::make_shared<DeferredExpandState>();
        const bool done = detail_expand::deferredExceptionalStep(
            *prep.renderer, *prep.defer, prep.codepoints);
        if (!done)
        {
            prep.pending = true;
            prep.committed = false;
            prep.starved = false;
            return prep;
        }
        prep.pending = false;
        prep.committed = !prep.defer->failed;
        prep.starved = prep.defer->failed ||
                       (prep.defer->stats.omitted > 0 &&
                        prep.defer->stats.rasterizedGlyphs == 0 &&
                        prep.defer->stats.exceptionalGrowOps == 0);
        if (prep.defer->stats.exceptionalGrowOps > 0 ||
            prep.defer->stats.rasterizedGlyphs > 0)
            prep.starved = false;
        prep.defer.reset();
        return prep;
    }

    const AtlasExpandStats stats =
        detail_expand::ensureImpl(*prep.renderer, prep.codepoints);
    prep.committed = true;
    prep.pending = false;
    prep.starved = (stats.omitted > 0 && stats.rasterizedGlyphs == 0 &&
                    stats.exceptionalGrowOps == 0 &&
                    stats.requestedUnique > 0);
    // Si algo se publico o no habia nada nuevo que hacer, no hay hambre.
    if (stats.rasterizedGlyphs > 0 || stats.exceptionalGrowOps > 0 ||
        stats.bakeCount == 0)
        prep.starved = false;
    return prep;
}

// Repertorio sintetico: cabe en 1024@72 pero deja poco margen; el extra
// fuerza el grow excepcional a 2048. Se afina tras la primera corrida.
inline std::vector<i32> atlasSyntheticOverflowRepertoire()
{
    // Determinista Aller@96: prefijo height-sort que LLENA 1024 (322 glifos).
    // El harness §7 usa fontSize=96; a 72 Aller cabe entero en 1024.
    static const i32 kBase[] = {
    290, 199, 366, 285, 287, 289, 291, 253, 375, 7923, 342, 254, 350, 536, 311, 303, 
    316, 260, 310, 370, 255, 325, 354, 538, 280, 8747, 315, 302, 40, 41, 124, 166, 
    197, 36, 307, 293, 123, 125, 106, 91, 93, 213, 195, 360, 209, 296, 309, 372, 
    7808, 7810, 7812, 210, 211, 212, 214, 334, 336, 192, 193, 194, 196, 258, 221, 284, 
    286, 288, 374, 376, 7922, 270, 217, 218, 219, 220, 262, 264, 266, 268, 364, 368, 
    292, 323, 327, 330, 340, 344, 356, 346, 348, 352, 8224, 8225, 308, 206, 300, 207, 
    314, 204, 205, 304, 64, 8719, 81, 377, 379, 381, 8721, 200, 201, 202, 203, 276, 
    278, 282, 313, 332, 256, 362, 298, 355, 539, 402, 274, 63119, 63070, 367, 63103, 229, 
    63057, 63105, 63106, 63046, 63083, 63092, 63110, 63111, 63118, 63067, 63086, 63079, 103, 63052, 121, 112, 
    281, 113, 326, 261, 231, 351, 537, 64260, 64259, 169, 174, 64256, 64258, 63050, 63096, 64257, 
    223, 63090, 63120, 181, 371, 162, 63080, 102, 343, 383, 62734, 62737, 338, 38, 208, 272, 
    271, 79, 216, 71, 68, 273, 329, 67, 295, 63044, 63051, 63097, 63100, 63497, 57, 82, 
    240, 66, 83, 98, 100, 63121, 63069, 63071, 80, 104, 167, 107, 63055, 357, 63081, 320, 
    318, 322, 108, 105, 63013, 8240, 62729, 37, 8536, 62731, 62733, 62735, 62732, 62736, 198, 87, 
    373, 7809, 7811, 63122, 63123, 63124, 63125, 294, 77, 86, 63089, 65, 88, 8364, 89, 52, 
    63492, 85, 72, 337, 8706, 63047, 63048, 63049, 63098, 56, 75, 78, 306, 369, 63093, 63094, 
    63095, 63112, 63496, 54, 84, 242, 243, 244, 245, 335, 358, 9674, 63494, 63126, 63127, 63128, 
    63129, 182, 232, 233, 234, 277, 279, 283, 63008, 63058, 63088, 63091, 63109, 63113, 63114, 63115, 
    63116, 63072, 63073, 63102, 63130, 222, 241, 249, 250, 251, 324, 328, 361, 365, 63054, 63056, 
    63101, 63, 224, 225, 226, 227, 259, 269, 63053, 263, 265, 267, 63104, 63107, 63108, 70, 
    347, 349
};
    return std::vector<i32>(kBase, kBase + sizeof(kBase) / sizeof(kBase[0]));
}

inline std::vector<i32> atlasSyntheticOverflowExtra()
{
    // Un glifo adicional ya desborda el cursor vivo a 1024@96.
    static const i32 kExtra[] = {
353, 63033, 63065, 63068, 63076, 47, 92, 297, 8725, 63082, 238, 345, 63077, 301, 74, 341

    };
    return std::vector<i32>(kExtra, kExtra + sizeof(kExtra) / sizeof(kExtra[0]));
}

// §9 CTL-LATENCIA: instrumentacion de produccion (min/dist/max por fase).
// El control positivo multi-proceso Release vive en el harness (9.0/9.2);
// atlasLatencyReport() publica las fases gateadas por ECLIPS_ATLAS_LATENCY_REPORT.
inline bool atlasLatencyPositiveControlOk()
{
    const auto rep = atlasLatencyReport();
    return !rep.fases.empty();
}
inline bool atlasLatencyReportsMinDistMax()
{
    const auto rep = atlasLatencyReport();
    if (rep.fases.size() < 4)
        return false;
    for (const auto& f : rep.fases)
        if (!(f.minMs <= f.p50Ms && f.p50Ms <= f.maxMs && f.muestras > 0))
            return false;
    return true;
}
inline bool atlasLatencyNoFrameAboveBudget(double ms)
{
    return ms <= 16.67;
}

} // namespace eclips
