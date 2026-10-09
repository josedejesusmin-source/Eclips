// ============================================================================
// EclipsSplashState.h — Intro del motor: el eclipse que cede la luz
// ============================================================================
// Contrato de diseño: DISENO_INTRO_ECLIPS.md
//
// Splash boot-only, 100% libre de assets (geometría + luz + tiempo + sonido
// sintético). Un paso de eclipse completo (~5s) que termina CEDIENDO la luz:
// el blanco final NO es un fade-out propio, es el menú del creador apareciendo.
//
// Estado OPACO (isTransparent=false): pinta su propio fondo; al popearse REVELA
// el menú que el boot pusheó debajo.
//
// Roadmap (DISENO_INTRO_ECLIPS §6.6):
//   CP1 ✅ estado que compila + render con primitivas reales + setupProjection.
//   CP2 (este) audio procedural propio (swell/ping/golpe) + pool que cierra leak.
//   CP3 política F4 (intro_motor / una_vez / auto-skip dev) + gating del boot.
//   CP4 pulido (luz suave §1.38, respiración §8.1.3, Akira anclado, viewport).
// ============================================================================
#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/IntroTimeline.h"
#include "engine/core/Settings.h"
#include "engine/core/Types.h"
#include "engine/render/Draw2D.h"
#include "Vignette.h"
#include "engine/audio/AudioManager.h"

#include <SDL.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace eclips
{
    class EclipsSplashState : public GameState
    {
    public:
        EclipsSplashState(SDL_Window* window, StateStack* stack)
            : m_window(window), m_stack(stack) {
        }

        // ── El estado es OPACO y de pantalla completa ──
        // Durante el handoff el splash deja ver y actualizar el menú ya cargado
        // debajo; solo conserva un velo blanco que cae hasta cero.
        //
        // LA ENTREGA ES UNA SOLA DECISIÓN, CON TRES CONSUMIDORES (intro-handoff,
        // 2026-08-01). Antes eran tres expresiones sueltas —dos acá y el clear
        // implícito adentro de `setupViewport`—, y esa tercera nadie la veía:
        // el reloj daba permiso de revelar y el render borraba lo revelado el
        // paso siguiente. Medido: el `.ecl` de abajo dibujaba (`rgba=0,153,0`)
        // y el frame salía `93,93,93`. Con una sola fuente no pueden divergir.
        bool revelaLoDeAbajo()   const { return m_timeline.inHandoff(); }

        bool isTransparent()     const override { return revelaLoDeAbajo(); }
        bool allowsUpdateBelow() const override { return revelaLoDeAbajo(); }

        // ── Ciclo de vida ───────────────────────────────────────────────────
        void onEnter() override
        {
            m_timeline.reset();
            m_time = 0.0;
            // La luna ENTRA en diagonal desde fuera de pantalla (arriba-derecha).
            m_moonX = (f32)viewport::width() * 1.35f;
            m_moonY = (f32)viewport::height() * -0.35f;

            // Audio propio (patrón VNScriptState): init aquí, shutdown en onExit.
            // El AudioManager retiene las voces procedurales y las libera solo.
            m_hasAudio = m_audio.init();
            if (m_hasAudio)
            {
                // Preferencia pura del jugador: misma ley que VNScriptState.
                m_audio.setSoundVolume(Settings::sfxVolume());

                // CP5: toda asignación/generación ocurre durante el preroll.
                // En cada beat solo arranca una voz ya residente.
                m_swellVoice = m_audio.prepareProceduralTone(
                    kSwellFreq, kSwellDur, kSwellVol,
                    Waveform::Triangle, kSwellAttack);
                m_pingVoice = m_audio.prepareProceduralTone(
                    kPingFreq, kPingDur, kPingVol,
                    Waveform::Pulse, kPingAttack, kPingDuty);
                m_thumpVoice = m_audio.prepareProceduralTone(
                    kThumpFreq, kThumpDur, kThumpVol,
                    Waveform::Triangle, kThumpAttack, 0.5f, kThumpRelease);

                if (m_swellVoice < 0 || m_pingVoice < 0 || m_thumpVoice < 0)
                    std::cerr << "[EclipsSplash] AVISO: no se pudieron preparar "
                              << "todas las voces de la firma." << std::endl;
            }
        }

        void onExit() override
        {
            // Libera el engine y, con él, el pool de voces procedurales (orden
            // seguro dentro de shutdown(): sonidos antes que el engine).
            if (m_hasAudio) m_audio.shutdown();
        }

        void update(f64 dt) override
        {
            m_timeline.advance(dt);
            m_time = m_timeline.presentationTime();

            // Preroll técnico: dispositivo abierto + voces residentes, pero el
            // arco narrativo sigue exactamente en t=0 y la pantalla queda negra.
            if (m_timeline.inPreRoll())
                return;

            m_vignette.update((f32)dt);

            // Skip elegante (a partir de t>kSkipFrom): NO corta seco a negro —
            // salta a "ceder la luz" para que el menú aparezca con el fade a
            // blanco, y silencia los disparos de audio que falten.
            if (m_skipped && m_time < kCedeStart)
            {
                m_timeline.skipTo(kCedeStart);
                m_time = m_timeline.presentationTime();
            }

            updatePhases();

            if (m_timeline.finished())
            {
                // Cola segura con identidad: solo se popea si seguimos en el tope.
                // Al popearse REVELA el menú que el boot pusheó debajo.
                m_stack->cmdPop(this);
            }
        }

        bool handleEvent(const void* event) override
        {
            const SDL_Event* e = static_cast<const SDL_Event*>(event);
            if (e->type == SDL_KEYDOWN || e->type == SDL_MOUSEBUTTONDOWN)
            {
                if (m_time > kSkipFrom)
                {
                    m_skipped = true;
                    return true;  // consumir
                }
            }
            return false;
        }

        // ── Render ───────────────────────────────────────────────────────────
        void render(f64 /*alpha*/) override
        {
            const f32 W = (f32)viewport::width();
            const f32 H = (f32)viewport::height();
            const f32 cx = W * 0.5f;
            const f32 cy = H * 0.5f;

            // CRÍTICO: DOS setups, como todos los states (§7.38: no se regalan).
            //  · setupViewport: letterbox al tamaño FÍSICO actual de la ventana.
            //    Sin esto el splash no se re-acomoda al resize (se "rompe" al
            //    agrandar). Era el que faltaba aparte de setupProjection.
            //  · setupProjection: mapea coords virtuales (1920×1080) al viewport.
            int winW = 0, winH = 0;
            SDL_GetWindowSize(m_window, &winW, &winH);

            // El clear va atado al MISMO predicado que la transparencia: si
            // este state revela lo de abajo, no puede borrarlo un paso despues.
            // Pasarlo como argumento (en vez de duplicar la condicion) es lo
            // que impide que las dos mitades se separen — que es exactamente
            // como nacio el bug que este lote arregla.
            draw2d::setupViewport(winW, winH,
                viewport::width(), viewport::height(),
                /*limpiar=*/ !revelaLoDeAbajo());

            draw2d::setupProjection(W, H);

            // CP5: el menú ya se renderiza debajo. Este state solo conserva el
            // blanco heredado del pico y lo retira; no vuelve a pintar negro.
            if (m_timeline.inHandoff())
            {
                const f32 a = m_timeline.handoffWhiteAlpha();
                if (a > 0.001f)
                    draw2d::drawRect(0.0f, 0.0f, W, H, 1.0f, 1.0f, 1.0f, a);
                return;
            }

            // (1) Fondo negro — BATCH (drawRect abre/cierra su propio begin/end).
            draw2d::drawRect(0.0f, 0.0f, W, H, 0.0f, 0.0f, 0.0f, 1.0f);

            // ── De aquí en adelante: primitivas IMMEDIATE-mode (drawCircle/
            //    drawCircleBorder, Vignette). No intercalar drawRect hasta el fade. ──

            // (2) Sol: disco de luz con desvanecimiento radial REAL (§1.38). Tres
            //     glows aditivos (cálido→amarillo→blanco) que caen a cero en el
            //     borde — luz celestial, no discos de alpha duro escalonados.
            if (m_sunRadius > 0.5f)
            {
                draw2d::drawCircleGlow(cx, cy, m_sunRadius * 1.8f, 1.0f, 0.80f, 0.40f, 0.22f);
                draw2d::drawCircleGlow(cx, cy, m_sunRadius * 1.2f, 1.0f, 0.92f, 0.66f, 0.40f);
                draw2d::drawCircleGlow(cx, cy, m_sunRadius * 0.8f, 1.0f, 1.00f, 0.92f, 0.85f);
            }

            // (3) Vignette (oscurecimiento global) — immediate.
            m_vignette.draw(W, H);

            // (3.5) HALO DE CORONA — va ANTES de la luna a propósito: el glow
            //       grande emana del centro y la luna negra (paso 4) lo recorta,
            //       dejando un resplandor que sale del filo del disco = corona
            //       real (§1.38 + B1). Tres capas aditivas que caen a cero.
            if (m_coronaGlow > 0.001f)
            {
                const f32 flicker = 0.90f
                    + 0.05f * std::sin((f32)m_time * 15.0f)
                    + 0.03f * std::sin((f32)m_time * 23.0f);
                const f32 a = m_coronaGlow * flicker;
                draw2d::drawCircleGlow(m_moonX, m_moonY, kMoonRadius * 2.2f, 1.0f, 0.93f, 0.74f, a * 0.30f);
                draw2d::drawCircleGlow(m_moonX, m_moonY, kMoonRadius * 1.7f, 1.0f, 0.90f, 0.70f, a * 0.45f);
                draw2d::drawCircleGlow(m_moonX, m_moonY, kMoonRadius * 1.3f, 1.0f, 0.96f, 0.82f, a * 0.60f);
            }

            // (4) Luna: disco negro perfecto que ocluye. En fase 1 está fuera de
            //     pantalla (posición de onEnter), así que dibujarla siempre es seguro.
            //     circleSegments mata la faceta poligonal de la totalidad (tu
            //     hallazgo: "es un círculo pero se ven pixeles en los lados").
            draw2d::drawCircle(m_moonX, m_moonY, kMoonRadius, 0.0f, 0.0f, 0.0f, 1.0f,
                draw2d::circleSegments(kMoonRadius));

            // (5) Anillo de diamante: el último hilo de luz colapsa en UN punto
            //     sobre el borde de la luna (fase 3).
            //     ── EL SECRETO DE AKIRA (§2): este punto cae donde algún día irá
            //        su mirada. Hoy astronomía; mañana ella. Offset fino = CP4. ──
            if (m_diamondFlash > 0.001f)
            {
                const f32 fx = m_moonX + kDiamondOffX;
                const f32 fy = m_moonY + kDiamondOffY;
                draw2d::drawCircleGlow(fx, fy, 70.0f * m_diamondFlash, 1.0f, 1.0f, 1.0f, m_diamondFlash);
                draw2d::drawCircleGlow(fx, fy, 28.0f * m_diamondFlash, 1.0f, 1.0f, 1.0f, std::min(1.0f, m_diamondFlash * 1.5f));
            }

            // (6) [ELIMINADO] El realce era un drawCircleGlow tras la luna. GOTCHA:
            //     un glow radial brilla MÁS en el CENTRO, no en el borde; dibujado
            //     DESPUÉS de la luna y aditivo, iluminaba el disco negro → la
            //     totalidad parecía traslúcida. La corona ya la da el halo (3.5),
            //     que va ANTES de la luna: ésta lo recorta, el centro queda negro
            //     puro. Un filo MÁS nítido pediría una primitiva de ANILLO real
            //     (brillo a un radio, cae a cero a ambos lados) — YAGNI hoy.

            // (7) Ceder la luz: fade a blanco final — BATCH. Este blanco ES el
            //     fade-in del menú del creador (no un fade-out de Eclips).
            if (m_fadeToWhite > 0.001f)
                draw2d::drawRect(0.0f, 0.0f, W, H, 1.0f, 1.0f, 1.0f, m_fadeToWhite);
        }

    private:
        // ── Umbrales de fase (constexpr, una sola fuente de verdad — §1.40) ──
        static constexpr f64 kP1End = 1.0;  // disco de luz respira
        static constexpr f64 kP2End = 2.5;  // luna entra en diagonal + vignette
        static constexpr f64 kP3End = 2.8;  // anillo de diamante
        static constexpr f64 kP4End = 3.8;  // totalidad (corona se enciende)
        static constexpr f64 kP5End = 4.2;  // el pulso (respiración de corona)
        static constexpr f64 kCedeStart = 4.2;  // = kP5End: empieza a ceder la luz
        static constexpr f64 kEclipseEnd = IntroTimeline::kEclipseDur;
        static constexpr f64 kSkipFrom = 1.5;  // saltable desde aquí

        static constexpr f32 kMoonRadius = 330.0f;
        static constexpr f32 kMaxSunRadius = 300.0f;  // < luna: la totalidad la cubre

        // ── Respiración (§8.1.3) — perilla sensorial, afinar a ojo ──
        static constexpr f32 kBreathAmp = 0.035f; // amplitud del vaivén (3.5%)
        static constexpr f32 kBreathRate = 3.2f;   // velocidad del latido (rad/s)
        static constexpr f32 kSettleFrac = 0.70f;  // % de fase 4 hasta tope → quietud
        // El secreto de Akira: borde superior-derecho de la luna (CP4 afina).
        static constexpr f32 kDiamondOffX = 233.0f;   // ~kMoonRadius * cos(45°)
        static constexpr f32 kDiamondOffY = -233.0f;  // ~kMoonRadius * sin(45°), arriba

        // ── La firma sonora (§7.1) — DIRECCIÓN CHIPTUNE deliberada (8/16-bit) ──
        //   Elección de identidad: unifica con el logo pixelado. La Game Boy es
        //   reconocible POR su sonido → chiptune = la vía directa a lo icónico.
        //   Cama:  swell TRIANGULAR grave, attack LARGO = crescendo (no pluck).
        //   Nota1: ping PULSE cristalino (lead estilo Game Boy) en el destello.
        //   Nota2: golpe TRIANGULAR grave, percusivo, en el pico del pulso.
        //   Triangle = el bajo de NES: limpio y presente, sin "click de guitarra".
        static constexpr f32 kSwellFreq = 105.0f;
        static constexpr f32 kSwellDur = 3.0f;
        static constexpr f32 kSwellVol = 0.50f;
        static constexpr f32 kSwellAttack = 0.80f;   // attack LARGO = crescendo suave (no pluck)

        static constexpr f32 kPingFreq = 1180.0f;
        static constexpr f32 kPingDur = 1.00f;
        // Era 0.48f: −10 dB (0.48 × 0.316), firma de Jesús 2026-09-28: «la actual
        // está bien en todo excepto ese ding». Medido, era la voz más fuerte de la
        // firma (−6.9 LUFS contra −10.5 del swell y −11.9 del golpe). Con puntas:
        // la sesión de la intro puede rehacerla (E-69, paso 5).
        static constexpr f32 kPingVol = 0.152f;
        static constexpr f32 kPingAttack = 0.004f;  // instantáneo = cristalino
        static constexpr f32 kPingDuty = 0.50f;   // 0.50=square · prueba 0.25 = sabor GB nasal
        static constexpr f32 kPingFlashThresh = 0.60f;  // dispara cerca del pico del destello

        static constexpr f32 kThumpFreq = 110.0f;
        static constexpr f32 kThumpDur = 1.0f;   // ↑ cola más larga = resuelve, no se trunca
        static constexpr f32 kThumpVol = 0.68f;
        static constexpr f32 kThumpAttack = 0.006f;  // percusivo pero no seco
        static constexpr f32 kThumpRelease = 0.90f;  // ← PERILLA: cola que DESVANECE
        //   0.015 = como antes (corte seco) · 0.30 = campana que muere natural.
        //   Barré 0.20→0.40 a oído. El golpe dura 1.0s; esto suaviza el final.
        static constexpr f64 kThumpAt = 3.95;  // ≈ pico del pulso de corona (~4.0s)

        // Easings inline (CP4 puede migrar la entrada del disco a TweenSystem).
        static f32 easeInOutQuad(f32 t)
        {
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
        }
        static f32 easeOutQuad(f32 t) { return 1.0f - (1.0f - t) * (1.0f - t); }
        static f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

        void updatePhases()
        {
            const f32 W = (f32)viewport::width();
            const f32 H = (f32)viewport::height();
            const f32 cx = W * 0.5f;
            const f32 cy = H * 0.5f;

            if (m_time <= kP1End)
            {
                // Fase 1 — el disco de luz RESPIRA: crece desde el silencio Y late
                // (§8.1.3). El crecimiento base (sin·π/2) lleva el arco; un seno
                // lento de baja amplitud superpuesto da el inhala-exhala. No es
                // estático: es algo vivo asomando antes de que entre la luna.
                const f32 p = (f32)(m_time / kP1End);
                const f32 grow = std::sin(p * 1.5707963f);            // 0→1 suave
                const f32 breath = 1.0f + kBreathAmp
                    * std::sin((f32)m_time * kBreathRate);            // vaivén sutil
                m_sunRadius = kMaxSunRadius * grow * breath;
                m_diamondFlash = 0.0f;
                m_coronaGlow = 0.0f;
                m_fadeToWhite = 0.0f;

                // Cama: el swell arranca DESDE EL SILENCIO INICIAL (§1.1), con la
                // luna asomando. Tu crescendo lo vuelve audible justo cuando entra.
                if (audioReady() && !m_swellStarted)
                {
                    m_audio.playPreparedTone(m_swellVoice);
                    m_swellStarted = true;
                }
            }
            else if (m_time <= kP2End)
            {
                // Fase 2 — la luna entra en DIAGONAL hacia el centro; vignette sube.
                const f32 p = easeInOutQuad((f32)((m_time - kP1End) / (kP2End - kP1End)));
                m_moonX = lerp(W * 1.35f, cx, p);
                m_moonY = lerp(H * -0.35f, cy, p);
                m_sunRadius = kMaxSunRadius;  // el sol sigue ahí; la luna lo va tapando
                if (m_vignette.intensity() < 0.85f)
                    m_vignette.setTarget(0.90f, 1.4f);
            }
            else if (m_time <= kP3End)
            {
                // Fase 3 — anillo de diamante: el sol colapsa, queda UN punto.
                m_moonX = cx; m_moonY = cy;
                const f32 p = (f32)((m_time - kP2End) / (kP3End - kP2End));
                m_sunRadius = 0.0f;
                m_diamondFlash = (p < 0.45f) ? (p / 0.45f) : std::max(0.0f, 1.0f - (p - 0.45f) / 0.55f);

                // Nota 1: ping cristalino EN el destello (sincronía a validar, §7.3).
                if (audioReady() && !m_pingPlayed && m_diamondFlash > kPingFlashThresh)
                {
                    m_audio.playPreparedTone(m_pingVoice);
                    m_pingPlayed = true;
                }
            }
            else if (m_time <= kP4End)
            {
                // Fase 4 — totalidad: la corona se enciende y luego QUEDA QUIETA.
                // Llega a tope al 70% de la fase (kSettleFrac); el resto es
                // stillness — el beat de silencio que le da peso al ta-dum (§8.1.3).
                m_moonX = cx; m_moonY = cy;
                m_diamondFlash = 0.0f;
                const f32 p = (f32)((m_time - kP3End) / (kP4End - kP3End));
                const f32 settle = std::min(1.0f, p / kSettleFrac);
                m_coronaGlow = easeOutQuad(settle);   // tope antes de fin → quietud
            }
            else if (m_time <= kP5End)
            {
                // Fase 5 — el pulso: UNA respiración de la corona (el "ta-dum" visual).
                m_moonX = cx; m_moonY = cy;
                const f32 p = (f32)((m_time - kP4End) / (kP5End - kP4End));
                m_coronaGlow = 0.80f + 0.20f * std::sin(p * 3.14159265f);

                // Nota 2: golpe grave resonante en el pico del pulso (sincronía §7.3).
                if (audioReady() && !m_thumpPlayed && m_time >= kThumpAt)
                {
                    m_audio.playPreparedTone(m_thumpVoice);
                    m_thumpPlayed = true;
                }
            }
            else if (m_time <= kEclipseEnd)
            {
                // Fase 6 — ceder la luz: la luna sale por el borde OPUESTO (abajo-
                // izquierda), la corona se apaga, el blanco inunda (= menú entrando).
                // Interpolación ABSOLUTA por m_time (no incremental — §6.5).
                const f32 p = (f32)((m_time - kCedeStart) / (kEclipseEnd - kCedeStart));
                const f32 e = easeInOutQuad(std::min(1.0f, p));
                m_moonX = lerp(cx, W * -0.35f, e);
                m_moonY = lerp(cy, H * 1.35f, e);
                m_coronaGlow = 0.80f * (1.0f - e);
                m_fadeToWhite = std::min(1.0f, p);
            }
            else
            {
                // Fase 7 — handoff: render() ya pinta solo el velo blanco sobre
                // el menú. Mantener aquí los residuos visuales en identidad.
                m_coronaGlow = 0.0f;
                m_diamondFlash = 0.0f;
                m_fadeToWhite = m_timeline.handoffWhiteAlpha();
            }
        }

        // Audio listo Y no se skipeó (el skip silencia los disparos que falten).
        bool audioReady() const { return m_hasAudio && !m_skipped; }

        // ── Estado ───────────────────────────────────────────────────────────
        SDL_Window* m_window = nullptr;
        StateStack* m_stack = nullptr;
        Vignette    m_vignette;
        AudioManager m_audio;
        IntroTimeline m_timeline;

        f64  m_time = 0.0;
        bool m_skipped = false;
        bool m_hasAudio = false;
        bool m_swellStarted = false;
        bool m_pingPlayed = false;
        bool m_thumpPlayed = false;
        i32 m_swellVoice = -1;
        i32 m_pingVoice = -1;
        i32 m_thumpVoice = -1;

        // Variables visuales
        f32 m_sunRadius = 0.0f;
        f32 m_moonX = 0.0f;
        f32 m_moonY = 0.0f;
        f32 m_coronaGlow = 0.0f;
        f32 m_diamondFlash = 0.0f;
        f32 m_fadeToWhite = 0.0f;
    };
}
