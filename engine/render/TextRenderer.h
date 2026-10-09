// ============================================================================
// ECLIPS ENGINE - TextRenderer (doc 10.2)
// ============================================================================
// Renderiza texto usando un Texture Atlas de fuentes (stb_truetype).
//
// Al cargar una fuente, genera una textura grande con todos los glifos
// ya rasterizados. Dibujar texto es simplemente dibujar rectangulos
// con las coordenadas UV correctas del atlas.
//
// Soporta Rich Text con tags inline:
//   {wavy}      / {ondulado}       — texto con onda sinusoidal
//   {shake}     / {temblar}        — texto con temblor aleatorio
//   {glitch}                       — tramo corrupto (cognado ES/EN, E-CP7):
//                                    revoltijo de glifos del mismo atlas +
//                                    jitter + franja roja/azul per-glifo.
//                                    El AVANCE es el del char original:
//                                    wrap y métricas no se enteran.
//   {color=#RRGGBB}                — color del texto (cognado ES/EN, sin variante)
//   {speed=N}   / {velocidad=N}    — multiplicador local de velocidad
//   {velocidad_global=N} / {global_speed=N} — override absoluto de la línea
//                                              (ignora cadena de fallback;
//                                               composable con {speed=N})
//   Cierre opcional: {/wavy}, {/shake}, {/color}, {/speed} (o equivalentes)
//   Si no se cierra, el efecto aplica hasta el final del texto.
// ============================================================================
#pragma once
#include "../core/Types.h"
#include "../core/TextStroke.h"
#include "../core/TextSpeed.h"
#include "../core/TextWrap.h"   // `WRAP-MIDE-BYTES`: la ley del corte (SDL-free)
#include "../core/TextGlifo.h"  // `GLIFO-FALTANTE-DIVERGE`: qué se contesta sin glifo
#include "FontMeasure.h"
#include "FontSourceStore.h"
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

namespace eclips
{
    class FontManager; // fwd: fuente inline resuelve nombre→TextRenderer* vía fm en render

    // ── Efectos de texto ──────────────────────────────────────────────────

    enum class TextFX : u8 { None = 0, Wavy, Shake, Glitch };

    struct CharEffect
    {
        TextFX fx = TextFX::None;
        float  speedMult = 1.0f;   // multiplicador de velocidad del typewriter
        float  pauseAfter = 0.0f;  // {pausa=N}: pausa N segundos tras este char
        bool   waitClickAfter = false; // {esperar_click}: pausa esperando click tras este char
        // ── {color=#RRGGBB}: pisa SOLO el RGB; el alpha lo hereda de la línea ──
        bool   hasColor = false;       // false = usa el color de línea (tema/estilo)
        float  cr = 1.0f, cg = 1.0f, cb = 1.0f;
        // ── {fuente=X}/{font=X}: nombre de fuente nombrada (FontManager).
        //    Vacío = fuente de línea (el renderer base). Se resuelve a
        //    TextRenderer* en render-time (robusto a hot-reload, coherente
        //    con texto-capa). Solo en bordes de rango → 1-3 lookups/línea.
        std::string fontName;
        // CP-R3 · identidad tardía 2 de 3. `VNScriptState` liga cada tramo a
        // la identidad del archivo que lo escribió ANTES de que el RichText
        // llegue a `VNDialogBox` (que se lo copia entero en `startDialogue`).
        // El nombre crudo de arriba queda para diagnóstico y para el texto que
        // nació sin catálogo.
        std::string fontKey;
        // ── {tam=N}/{size=N}: tamaño px absoluto del tramo. 0 = tamaño de
        //    línea (sentinela). Se aplica a la fuente activa (base o {fuente=X})
        //    en render-time vía FontManager::getSized — mismo primitivo que el
        //    tamaño per-caja de FASE 2 (§7.76), ahora per-glifo.
        float fontSize = 0.0f;
    };

    struct RichText
    {
        std::string              displayText;  // texto limpio sin tags
        std::vector<CharEffect>  effects;      // 1 entrada por byte de displayText
        std::optional<TextSpeed> lineSpeedOverride;
        bool                     interrupts = false;       // {interrumpir}: al completar el
        // typewriter, salta sin click (auto-advance forzado, pisa el modo auto)

        // `caja-entrada`: segundos que la línea espera ANTES de la primera
        // letra. 0 = ninguno. Nace de `{pausa=N}` escrito al principio de la
        // línea, que hasta este lote se IGNORABA con un aviso ("al inicio sin
        // char previo") porque la pausa se cuelga del carácter ANTERIOR y ahí
        // no hay ninguno.
        //
        // No era una carencia teórica: `test_ddlc.ecl:76` y
        // `test_ddlc_mejoras.ecl:69` ya escriben `"{pausa=1.0}Vamos,…"` — el
        // corpus lo pedía y el motor lo tiraba. Viaja aparte de `effects`
        // justamente porque no pertenece a ningún carácter: pertenece a la
        // línea.
        float                    openingPause = 0.0f;

        // `vocabulario-lote3` (`R-03`): la PRESENCIA, aparte del valor.
        //
        // Un float con centinela `0` no puede decir la diferencia entre *«no lo
        // escribí»* y *«escribí cero»*, y esa diferencia es justo la que el
        // creador necesita: `{pausa=0}` significa **«esta línea arranca YA»** y
        // tiene que ganarle al `caja_aparicion_texto_retraso` del tema.
        //
        // La forma no se inventó: es el gemelo exacto de `hasColor` + `cr/cg/cb`
        // veinte líneas más arriba, en esta misma struct (`CLAUDE.md §2.9` —
        // inventariar el vocabulario que el motor ya tiene antes de nombrar).
        // Un centinela `-1` habría sido una TERCERA convención de ausencia acá
        // adentro: `lineSpeedOverride` y `fontSize` ya usan el `0`.
        //
        // Sólo aplica al tag de APERTURA. Mid-línea, `{pausa=0}` es un no-op de
        // verdad —no hay nada heredado que cancelar— y por eso `pauseAfter`
        // sigue siendo un float pelado.
        bool                     hasOpeningPause = false;
    };

    // ── Glifo ─────────────────────────────────────────────────────────────

    struct GlyphInfo
    {
        f32 uvX0, uvY0, uvX1, uvY1;     // UVs completas (edge-to-edge): texto/outline/fill
        f32 uvX0i, uvY0i, uvX1i, uvY1i; // UVs insetadas 1/2 texel: SOLO stamps del glow
        f32 offsetX, offsetY;            // Offset desde posicion base
        f32 width, height;               // Tamano del glifo en pixeles
        f32 advance;                     // Avance horizontal — YA INCLUYE el kerning de la fuente (plegado en init, ver TextRenderer.cpp). NO re-sumar m_kerning al leer este campo.
    };

    // ── TextRenderer ──────────────────────────────────────────────────────

    // CP-R5: acceso friend para ampliacion dinamica (FontAtlasExpand.h).
    struct FontAtlasExpandAccess;
    class TextRenderer
    {
    public:
        TextRenderer() = default;
        explicit TextRenderer(FontSourceStore* sourceStore)
            : m_fontSourceStore(sourceStore) {}
        ~TextRenderer();

        bool init(const std::string& fontPath, f32 fontSize, f32 kerning = 0.0f,
                  FontMeasure measure = FontMeasure::Height);
        bool initProfile(const TextProfileSpec& spec);
        void setSourceStore(FontSourceStore* sourceStore)
        {
            m_fontSourceStore = sourceStore;
        }

        // El commit sólo intercambia un renderer ya horneado. Roba el atlas
        // GPU y el plan; el origen queda vacío para que su destructor no
        // borre la textura.
        void stealReadyFrom(TextRenderer& ready)
        {
            if (this == &ready) return;
            shutdown();
            m_atlasTexID = ready.m_atlasTexID;
            ready.m_atlasTexID = 0;
            m_atlasW = ready.m_atlasW;
            m_atlasH = ready.m_atlasH;
            ready.m_atlasW = 0;
            ready.m_atlasH = 0;
            m_fontSize = ready.m_fontSize;
            m_lineHeight = ready.m_lineHeight;
            m_ascent = ready.m_ascent;
            m_kerning = ready.m_kerning;
            m_fontMeasure = ready.m_fontMeasure;
            m_fontPath = std::move(ready.m_fontPath);
            m_fontSourceStore = ready.m_fontSourceStore;
            m_profilePlan = std::move(ready.m_profilePlan);
            m_glyphs = std::move(ready.m_glyphs);
            m_ausentes = std::move(ready.m_ausentes);
            ready.m_profilePlan = {};
            ready.m_glyphs.clear();
            ready.m_ausentes.olvidar();
            m_packLive = ready.m_packLive;
            m_packPenX = ready.m_packPenX;
            m_packPenY = ready.m_packPenY;
            m_packRowHeight = ready.m_packRowHeight;
            ready.m_packLive = false;
            ready.m_packPenX = 2;
            ready.m_packPenY = 2;
            ready.m_packRowHeight = 0;
        }

        // Texto simple (sin efectos)
        void drawText(const std::string& text, f32 x, f32 y,
                      f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f, f32 a = 1.0f);
        void drawTextPartial(const std::string& text, f32 x, f32 y,
                             i32 charCount,
                             f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f, f32 a = 1.0f);
        void drawTextWrapped(const std::string& text, f32 x, f32 y,
                             f32 maxWidth, i32 charCount = -1,
                             f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f, f32 a = 1.0f);

        // Rich Text (con efectos wavy/shake)
        void drawTextWrappedRich(const std::string& text, f32 x, f32 y,
                                 f32 maxWidth, i32 charCount, f32 time,
                                 const std::vector<CharEffect>& effects,
                                 f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f, f32 a = 1.0f);

        // ── Variantes con outline (DDLC-style stroke en el texto) ──
        // Implementación: muestreo circular uniforme. La ley pura calcula
        // N muestras sobre el radio `outlineThickness` (separación objetivo
        // de 2 px, acotada a 8..48) y deja una sola pasada final para el fill.
        // Las seis variantes outlined consumen la misma ley; no hay copias
        // cardinales/diagonales ni una réplica 3x3 por caller.
        //
        // Calidad: la separación máxima de las muestras queda controlada por
        // el radio, pero el criterio visual de cada tamaño y grosor se valida
        // con la sonda dedicada. El contrato automático no promete calidad
        // arbitraria fuera del clamp de la ley.
        //
        // Costo: (N + 1) draw calls del texto base por llamada. Es medible y
        // depende del grosor; si en el futuro se aplica a HUD numérico de
        // gameplay 2D que se redibuja a 60fps, medir antes de asumir que está
        // bien.
        //
        // Mejora futura: reemplazar por SDF (Signed Distance Field) cuando
        // Eclips migre de OpenGL Compatibility a Core / OpenGL ES. Esa
        // migración va a llegar con el port a Android (ES no soporta
        // glBegin/glEnd) — momento natural para reescribir TextRenderer
        // con shaders propios. SDF da outline perfecto a cualquier grosor
        // y costo de 1× draw call. La API pública de estos métodos puede
        // mantenerse idéntica; cambia solo la implementación.
        //
        // Si outlineThickness <= 0 o outlineA <= 0, equivalen al método base.
        void drawTextOutlined(const std::string& text, f32 x, f32 y,
            f32 r, f32 g, f32 b, f32 a,
            f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
            f32 outlineThickness);
        // Misma ley de muestreo circular que drawTextOutlined (ver comentario arriba).
        void drawTextWrappedOutlined(const std::string& text, f32 x, f32 y,
            f32 maxWidth, i32 charCount,
            f32 r, f32 g, f32 b, f32 a,
            f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
            f32 outlineThickness);
        // Misma ley de muestreo circular que drawTextOutlined (ver comentario arriba).
        void drawTextWrappedRichOutlined(const std::string& text, f32 x, f32 y,
            f32 maxWidth, i32 charCount, f32 time,
            const std::vector<CharEffect>& effects,
            f32 r, f32 g, f32 b, f32 a,
            f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
            f32 outlineThickness,
            FontManager* fm = nullptr); // fuente inline: resuelve {fuente=X} por glifo

        // Parser de tags → texto limpio + efectos
        static RichText parseRichText(const std::string& raw);

        f32 measureWidth(const std::string& text) const;
        f32 measureWrappedHeight(const std::string& text, f32 maxWidth,
                                 f32 lineSpacing = 0.0f) const;

        // ── getLineHeight vs. lineSpacing: DOS preguntas distintas ──────────
        // `getLineHeight()` = ALTO DE CAJA de UNA línea, tal cual lo dicen las
        // métricas del ttf. Lo consumen ~30 sitios y NINGUNO apila líneas: son
        // centrado vertical ((h - lh) / 2) en botones, slots y filas de menú, o
        // stacking de chrome. `lineSpacing` NO lo toca, a propósito — si lo
        // tocara, todos esos centrados se moverían medio píxel en silencio.
        //
        // El PASO entre líneas sí lo mueve, y vive en tres sitios, los tres
        // dentro de este .cpp (el '\n' de drawTextPartial_noBatch, el de
        // drawTextWrappedRich_noBatch, y el conteo de measureWrappedHeight).
        // Esos tres son los únicos que leen el parámetro.
        f32 getLineHeight() const { return m_lineHeight; }
        // FASE 2 (tamaño per-caja): exponer el recurso (ttf, tamaño, kerning)
        // para pedirle a FontManager "mismo ttf, otro tamaño" (getSized, §7.43).
        const std::string& fontPath() const { return m_fontPath; }
        f32 fontSize() const { return m_fontSize; }
        f32 kerning() const { return m_kerning; }
        FontMeasure fontMeasure() const { return m_fontMeasure; }
        FontMeasure measure() const { return m_fontMeasure; }
        f32 lineHeight() const { return m_lineHeight; }
        const TextProfilePlanHandle& profilePlan() const { return m_profilePlan; }
        bool hasProfilePlan() const { return static_cast<bool>(m_profilePlan); }
        std::uintptr_t atlasIdentity() const
        {
            return static_cast<std::uintptr_t>(m_atlasTexID);
        }
        std::size_t atlasCpuBytes() const { return 0; }
        std::size_t atlasGpuBytes() const
        {
            return static_cast<std::size_t>(m_atlasW) *
                   static_cast<std::size_t>(m_atlasH) * 4u;
        }
        bool isLoaded() const { return m_atlasTexID != 0; }
        void shutdown();

    private:
        // CP-R5 P3=3a: cursor de colocacion del packer incremental (solo initProfile).
        bool m_packLive = false;
        i32 m_packPenX = 2;
        i32 m_packPenY = 2;
        i32 m_packRowHeight = 0;
        friend struct FontAtlasExpandAccess;

        u32 m_atlasTexID = 0;
        i32 m_atlasW = 0, m_atlasH = 0;
        f32 m_fontSize = 0, m_lineHeight = 0, m_ascent = 0;
        f32 m_kerning = 0;   // px sumados al advance de cada glifo (kerning/tracking de la fuente; plegado en init)
        FontMeasure m_fontMeasure = FontMeasure::Height;
        std::string m_fontPath;  // FASE 2: ttf de origen, para re-hornear el atlas a otro tamaño (getSized)
        FontSourceStore* m_fontSourceStore = nullptr; // no propietario; Engine vive mas
        TextProfilePlanHandle m_profilePlan;
        std::unordered_map<i32, GlyphInfo> m_glyphs;
        const GlyphInfo* getGlyph(i32 cp) const;

        // ── La política de GLIFO AUSENTE, con este atlas enchufado ─────────
        // `GLIFO-FALTANTE-DIVERGE` / §7.584. La ley vive en
        // `engine/core/TextGlifo.h` y la prueba `harness_glifo.cpp`; acá sólo
        // se le pasa el atlas. **Es el ÚNICO camino**: `draw`, `measureWidth`
        // y las dos entradas del wrap piden el avance por acá, que es lo que
        // hace imposible que vuelvan a contestar distinto.
        f32 avanceDeEsteAtlas(i32 cp) const;
        text::BuscarGlifo buscadorDeEsteAtlas() const;

        // El aviso del ausente, UNA vez por codepoint (fork D, 2026-08-28).
        // Omitir es legítimo sólo porque el motor habla: sin esto, un proyecto
        // pierde letras sin dejar rastro.
        void avisarAusente(i32 cp) const;

        // `mutable` porque `measureWidth` y el wrap son `const` y tienen que
        // poder avisar: la memoria del aviso es caché, no estado del render.
        mutable text::RegistroDeAusentes m_ausentes;

        // ── El plan de corte con las métricas de ESTE atlas ────────────────
        // `WRAP-MIDE-BYTES`: la ley del corte vive en `engine/core/TextWrap.h`
        // y la prueba `harness_wrap.cpp`; acá sólo se le enchufa el atlas.
        //
        // Existe como método privado, y no como lambda repetida en cada sitio,
        // por UNA razón concreta: el `gi ? gi->advance : 0` es la política de
        // GLIFO FALTANTE (el wrap OMITE, `draw` sustituye por '?'). Esa
        // divergencia es `GLIFO-FALTANTE-DIVERGE` / §7.584 y sigue ABIERTA —
        // el día que se cierre, se toca UNA línea y no tres.
        //
        // Lo usan los tres caminos que miden con la fuente base. El cuarto
        // (`drawTextWrappedRich_noBatch`) arma el suyo, porque resuelve la
        // fuente POR CARÁCTER y no puede compartir este callback.
        text::PlanDeCorte planDeCorteDeEsteAtlas(const std::string& texto,
                                                 f32 anchoMax) const;

        // ── Emisión sin abrir/cerrar batch (Render Core Fase 1, paso 6.C) ──
        // Estos métodos asumen que el caller YA tiene draw2d::g_batch->begin()
        // activo y NO llamarán end() antes de que terminen. Usados por las
        // versiones públicas (que envuelven con begin/end) y por las variantes
        // outlined (que abren UN solo batch y emiten las N muestras adentro).
        // Decisión D5-A: ver RESUMEN_SESION_RENDER_FASE1_PASO5_PASO6 al cierre.
        void drawTextPartial_noBatch(const std::string& text, f32 x, f32 y,
            i32 charCount,
            f32 r, f32 g, f32 b, f32 a, bool useInset = false,
            f32 lineSpacing = 0.0f); // px sumados al paso entre líneas (0 = ttf)
        void drawTextWrappedRich_noBatch(const std::string& text, f32 x, f32 y,
            f32 maxWidth, i32 charCount, f32 time,
            const std::vector<CharEffect>& effects,
            f32 r, f32 g, f32 b, f32 a, bool honorCharColor = true,
            FontManager* fm = nullptr, // fuente inline (B): mide y dibuja por fuente
            f32 lineSpacing = 0.0f);   // px sumados al paso entre líneas (0 = ttf)


    public:
        // F2.C: variantes _noBatch para VNDialogBox dentro de batch externo.
        void drawText_noBatch(const std::string& text, f32 x, f32 y,
            f32 r, f32 g, f32 b, f32 a);

        // Glow (halo difuso): 3 anillos de 8 stamps a radios crecientes con
        // alpha decreciente, + fill final encima. Modela drawTextOutlined_noBatch
        // pero con anillos en vez de 1 contorno. Una linea (sin word-wrap) —
        // pensado para carteles/titulos cortos del texto-capa (decision #4 de
        // la sesion glow). El "brillo de letra de computadora" de DDLC Plus.
        // Si glowRadius <= 0 o glowIntensity <= 0, equivale a drawText_noBatch.
        void drawTextGlow_noBatch(const std::string& text, f32 x, f32 y,
            f32 r, f32 g, f32 b, f32 a,
            f32 glowR, f32 glowG, f32 glowB,
            f32 glowRadius, f32 glowIntensity);

        void drawTextOutlined_noBatch(const std::string& text, f32 x, f32 y,
            f32 r, f32 g, f32 b, f32 a,
            f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
            f32 outlineThickness);

        // Lista autorable: cada pasada conserva color, grosor y offset; todas
        // se emiten detrás y este método dibuja el fill exactamente una vez.
        void drawTextStrokes_noBatch(const std::string& text, f32 x, f32 y,
            f32 r, f32 g, f32 b, f32 a,
            const std::vector<ResolvedTextStroke>& strokes);

        void drawTextWrapped_noBatch(const std::string& text, f32 x, f32 y,
            f32 maxWidth, i32 charCount,
            f32 r, f32 g, f32 b, f32 a,
            f32 lineSpacing = 0.0f); // px sumados al paso entre líneas (0 = ttf)

        void drawTextWrappedOutlined_noBatch(const std::string& text, f32 x, f32 y,
            f32 maxWidth, i32 charCount,
            f32 r, f32 g, f32 b, f32 a,
            f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
            f32 outlineThickness);

        void drawTextWrappedRichOutlined_noBatch(const std::string& text, f32 x, f32 y,
            f32 maxWidth, i32 charCount, f32 time,
            const std::vector<CharEffect>& effects,
            f32 r, f32 g, f32 b, f32 a,
            f32 outlineR, f32 outlineG, f32 outlineB, f32 outlineA,
            f32 outlineThickness,
            FontManager* fm = nullptr, // fuente inline: propaga fm a cada pasada
            f32 lineSpacing = 0.0f);   // ...y el interlineado al fill final

    };
}
