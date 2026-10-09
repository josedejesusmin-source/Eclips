// ============================================================================
// TweenSystem.h — Sistema de animación/tweening para Eclips Engine
// ============================================================================
//
// Lógica pura de animación: structs + funciones de update.
// No depende de VNScriptState ni de rendering.
// VNScriptState almacena CharAnim por personaje y llama updateAnim().
//
// ── Efectos disponibles ────────────────────────────────────────────────────
//
//   Shake      — vibración horizontal rápida (decae suavemente)
//   Bounce     — salto parabólico con gravedad natural
//   ZoomIn     — escala sube y SE MANTIENE (hasta zoom_out o detener)
//   ZoomOut    — escala baja a 1.0
//   Float      — sube y baja suavemente (loop, aterriza suave al detener)
//   Pulse      — alpha oscila (loop, aterriza suave al detener)
//   Nudge      — vaivén suave en eje X/Y (loop, DDLC ctc). Offset puro.
//   TintFlash  — flash blanco encima del sprite/pantalla (Taiyoken)
//   Drop       — sprite baja y SE QUEDA (tristeza, derrota). Landing suave.
//   Rise       — sprite sube y SE QUEDA (sorpresa, determinación). Landing suave.
//   Hop        — brinquito rápido, vuelve a normal (emoción, alegría)
//   LeanBack   — salto hacia atrás + vuelve a normal (invasión de espacio)
//
// ── Núcleo componible (C-CP0) ──────────────────────────────────────────────
//
//   Tabla de curvas SSOT (EasingId): lineal · suave · salida · resorte · rebote · cubica
//   AnimOutput + compose(): offsets SUMAN, factores MULTIPLICAN, flash por MAX
//   AnimTrack + evalTrack(): canal crudo, f(t) pura (consumible con dt=0)
//
//   Los 13 presets de arriba NO se reescriben: son recetas selladas (§7.209).
//   Preset = receta multi-canal con nombre; track = canal crudo. Mismo río.
//
// ============================================================================

#pragma once

#include "engine/core/AnimationClock.h"
#include <cmath>
#include <string>
#include <algorithm>   // C-CP0: std::max en compose()
#include <vector>      // C-CP2: el stack de animaciones de un elemento

namespace eclips
{

    // ── Tipos de animación ─────────────────────────────────────────────────

    enum class AnimType
    {
        None = 0,
        Shake,
        Bounce,
        ZoomIn,
        ZoomOut,
        Float,      // Loop
        Pulse,      // Loop
        Nudge,      // Loop — vaivén suave eje X/Y (DDLC ctc)
        TintFlash,
        // ── `Glitch` MURIÓ AQUÍ (E-CP1) ──────────────────────────────────
        // Dejó de ser un preset de CANAL con render bespoke y pasó a ser
        // `FxKind::Glitch`: una receta SELLADA sobre la tubería de pasadas
        // (EffectSystem.h), hecha de `franja` + `parpadeo` + `tajadas`.
        // Sin enum, sin bandera en AnimOutput, sin renderGlitchChar.
        // El .ecl NO cambia: `animar monika glitch 2` sigue andando — ahora
        // lo resuelve `parseFxKind`, no `parseAnimType`.
        Drop,       // Persiste (como ZoomIn). Landing suave al detener.
        Rise,       // Persiste (como ZoomIn). Landing suave al detener.
        Hop,        // One-shot
        LeanBack    // One-shot
    };

    // ── Datos de una animación activa ──────────────────────────────────────

    struct CharAnim
    {
        AnimType type       = AnimType::None;
        float    duration   = 0.0f;
        float    elapsed    = 0.0f;
        float    intensity  = 1.0f;
        float    fromScale = 1.0f;   // Para ZoomOut: scale inicial real

        // ── Nudge (vaivén): parámetros propios ───────────────────────
        float    period = 0.0f;   // Nudge: ciclo en seg (0 = default 1.5s)
        bool     axisVertical = false;  // Nudge: true=eje Y, false=eje X

        // ── Landing suave para loops y persistentes ──────────────────
        bool     landing    = false;   // true = aterrizando
        float    landFrom   = 0.0f;    // valor al momento de pedir detener
        float    landTime   = 0.0f;    // tiempo acumulado del aterrizaje

        // ── Offsets calculados (aplicar durante render) ────────────────
        float    offsetX    = 0.0f;
        float    offsetY    = 0.0f;
        float    scale      = 1.0f;
        float    alpha      = 1.0f;
        float    tint       = 1.0f;
        float    flashAlpha = 0.0f;    // Intensidad del flash blanco (0 = sin flash)

        bool isActive() const { return type != AnimType::None; }
        bool isLoop()   const
        {
            return !landing && (
                type == AnimType::Float ||
                type == AnimType::Pulse ||
                type == AnimType::Nudge
                );
        }
        bool isPersistent() const
        {
            return type == AnimType::ZoomIn ||
                   type == AnimType::Drop ||
                   type == AnimType::Rise;
        }
        bool isDone()   const
        {
            if (!isActive()) return true;
            if (isLoop())    return false;
            if (isPersistent() && elapsed >= duration) return false;
            return elapsed >= duration;
        }
    };

    // ── Easing functions ───────────────────────────────────────────────────

    namespace easing
    {
        inline float linear(float t) { return t; }

        // Espejo temporal de easeOutQuad (veredicto §13, harness_ctc 4/4):
        // en un vaivén la vuelta es la ida reproducida hacia atrás, así que
        // "easeIn ida + easeOut vuelta" = ESTA curva sola sobre el triángulo.
        // Con de=-amp a=0 modo=vaiven reproduce el ctc del Nudge exacto.
        inline float easeInQuad(float t) { return t * t; }

        inline float easeOutQuad(float t) { return t * (2.0f - t); }

        inline float easeInOutSine(float t)
        {
            return 0.5f * (1.0f - std::cos(t * 3.14159265f));
        }

        inline float easeOutBack(float t)
        {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
        }

        // La ÚNICA función matemática nueva del contrato C. 4 tramos (Penner).
        // Cae y rebota; nunca sobrepasa 1.0 (a diferencia de easeOutBack).
        inline float easeOutBounce(float t)
        {
            const float n1 = 7.5625f;
            const float d1 = 2.75f;
            if (t < 1.0f / d1) { return n1 * t * t; }
            else if (t < 2.0f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
            else if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
            else { t -= 2.625f / d1; return n1 * t * t + 0.984375f; }
        }

        // Cúbica simétrica histórica de Ren'Py/ATL. Mantiene el progreso
        // continuo: la cuantización de coordenadas, si algún caller la pide,
        // no pertenece a la función de easing.
        inline float easeInOutCubic(float t)
        {
            if (t < 0.5f)
                return 4.0f * t * t * t;
            const float u = 1.0f - t;
            return 1.0f - 4.0f * u * u * u;
        }
    }

    // ── Tabla de curvas: la SSOT (§7.209 · §7.205 una tabla, dos direcciones) ──
    //
    // Quien quiera una curva la pide POR NOMBRE contra esta tabla. Se comparte
    // la TABLA, no el sistema: capa_mover la consume sin heredar CharAnim.

    enum class EasingId
    {
        Lineal = 0, Suave, Salida, Resorte, Rebote, Entrada, Cubica, None
    };

    inline EasingId parseEasing(const std::string& name)
    {
        if (name == "lineal" || name == "linear")     return EasingId::Lineal;
        if (name == "suave" || name == "smooth")     return EasingId::Suave;
        if (name == "salida" || name == "ease_out")   return EasingId::Salida;
        if (name == "entrada" || name == "ease_in")    return EasingId::Entrada;
        if (name == "resorte" || name == "spring")     return EasingId::Resorte;
        if (name == "rebote" || name == "bounce_out") return EasingId::Rebote;
        if (name == "cubica" || name == "cubic")       return EasingId::Cubica;
        return EasingId::None;   // el caller avisa (§7.89), no adivina
    }

    inline std::string easingToString(EasingId e)
    {
        switch (e)
        {
        case EasingId::Lineal:  return "lineal";
        case EasingId::Suave:   return "suave";
        case EasingId::Salida:  return "salida";
        case EasingId::Entrada: return "entrada";
        case EasingId::Resorte: return "resorte";
        case EasingId::Rebote:  return "rebote";
        case EasingId::Cubica:  return "cubica";
        default:                return "lineal";
        }
    }

    // None degrada a lineal: una curva desconocida nunca produce NaN ni cuelga.
    inline float applyEasing(EasingId e, float t)
    {
        switch (e)
        {
        case EasingId::Suave:   return easing::easeInOutSine(t);
        case EasingId::Salida:  return easing::easeOutQuad(t);
        case EasingId::Entrada: return easing::easeInQuad(t);
        case EasingId::Resorte: return easing::easeOutBack(t);
        case EasingId::Rebote:  return easing::easeOutBounce(t);
        case EasingId::Cubica:  return easing::easeInOutCubic(t);
        case EasingId::Lineal:
        default:                return easing::linear(t);
        }
    }

    // ── Álgebra de composición (§7.208) ──────────────────────────────────
    //
    // Desplazamientos SUMAN, factores MULTIPLICAN y el flash compone por MAX
    // (es overlay, no factor). El glitch componía por OR — y dejó de estar: en
    // E-CP1 se mudó a la tubería de pasadas, que tiene su PROPIA álgebra (los
    // canales componen sin orden; las pasadas se encadenan).
    // La identidad del álgebra ES el off-por-identidad: componer UNA sola
    // animación devuelve el valor de siempre, bit a bit.

    struct AnimOutput
    {
        float dx = 0.0f, dy = 0.0f;          // px, aditivos
        float scale = 1.0f, alpha = 1.0f;    // factores, multiplicativos
        float tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;
        float flashAlpha = 0.0f;             // especial: compone por MAX
        // `glitch` (presencia-OR) murió en E-CP1: el preset ya no es un canal.
    };

    inline AnimOutput compose(const AnimOutput& a, const AnimOutput& b)
    {
        AnimOutput o;
        o.dx = a.dx + b.dx;
        o.dy = a.dy + b.dy;
        o.scale = a.scale * b.scale;
        o.alpha = a.alpha * b.alpha;
        o.tintR = a.tintR * b.tintR;
        o.tintG = a.tintG * b.tintG;
        o.tintB = a.tintB * b.tintB;
        o.flashAlpha = std::max(a.flashAlpha, b.flashAlpha);
        return o;
    }

    // ── El preset entra al álgebra (§4.3) ────────────────────────────────
    //
    // updateAnim() ya dejó los canales calculados dentro del CharAnim; esto
    // solo los PRESENTA como AnimOutput. Preset = receta multi-canal con
    // nombre; track = canal crudo. Mismo río. Los presets tiñen uniforme →
    // el tint va a los tres componentes (el canal RGB separado es CP6).
    inline AnimOutput animOutput(const CharAnim& a)
    {
        AnimOutput o;
        o.dx = a.offsetX;
        o.dy = a.offsetY;
        o.scale = a.scale;
        o.alpha = a.alpha;
        o.tintR = o.tintG = o.tintB = a.tint;
        o.flashAlpha = a.flashAlpha;
        return o;
    }

    // C-CP2: el stack de un elemento colapsa a UN AnimOutput. Con una sola
    // animación el resultado es bit-idéntico al de antes — la identidad del
    // álgebra ES el off-por-identidad (§7.208). Stack vacío → identidad.
    inline AnimOutput composeStack(const std::vector<CharAnim>& stack)
    {
        AnimOutput out;
        for (const auto& a : stack)
            out = compose(out, animOutput(a));
        return out;
    }

    // `findAnim` murió con el Glitch (E-CP1): existía SOLO para los tipos-bandera,
    // y Glitch era el único que había. Cero callers = no existe (§7.118).

    // ¿Este preset gobierna el canal escala? (F2 — dueño exclusivo)
    // acercar/alejar/retroceder NO apilan entre sí: sus escalas se
    // MULTIPLICARÍAN y el sprite quedaría en un tamaño que nadie pidió.
    // Los de offset y alpha (flotar, pulso, temblar, vaiven…) apilan libres.
    inline bool ownsScale(AnimType t)
    {
        return t == AnimType::ZoomIn ||
            t == AnimType::ZoomOut ||
            t == AnimType::LeanBack;
    }

    // ── Track genérico + evaluador puro (§7.207) ─────────────────────────
    //
    // La animación es f(t). El estado vive en el ciclo de vida (elapsed), no
    // en la curva → evalTrack se puede llamar con el reloj inyectado y dt=0
    // (el patrón que el indicador de avance ya usa a mano).

    enum class Channel { OffsetX, OffsetY, Scale, Alpha, Tint };
    enum class Lifecycle { UnaVez = 0, Loop, Vaiven };

    // Identidad del canal: 0 para los aditivos, 1 para los multiplicativos.
    // Es lo que convierte `amplitud=N` (azúcar del creador) en `de`/`a` al
    // CONSTRUIR el track (§1.40) — por eso el track guarda de/a ya resueltos
    // y evalTrack no necesita saber qué canal es.
    //   mover_y amplitud=10  → de=0  a=10   (en reposo NO desplaza)
    //   alpha   amplitud=0.3 → de=1  a=0.7  (en reposo NO atenúa — ley de Pulse)
    inline float channelIdentity(Channel c)
    {
        return (c == Channel::OffsetX || c == Channel::OffsetY) ? 0.0f : 1.0f;
    }

    struct AnimTrack
    {
        Channel   canal = Channel::OffsetY;
        float     de = 0.0f;   // valor al inicio (resuelto)
        float     a = 0.0f;   // valor al final / extremo del vaivén
        float     dur = 0.0f;   // cantidad del clock (una_vez) | periodo en s
        EasingId  curva = EasingId::Lineal;
        Lifecycle modo = Lifecycle::UnaVez;
        // Reloj-diálogo: CP1 expone DialogueAdvance solo para escala directa;
        // Seconds sigue siendo el default bit-compatible de todo caller viejo.
        ClockDomain clock = ClockDomain::Seconds;
        float     elapsed = 0.0f;   // el ÚNICO estado
    };

    // Envolvente escalar genérica: definición y estado discreto en un solo
    // tipo. CP0 la usa para congelar E4; CP2 la conectará a la intensidad de
    // efectos. DurationSpec mantiene cantidad+dominio inseparables.
    struct ScalarEnvelope
    {
        float        from = 1.0f;
        float        to = 1.0f;
        DurationSpec duration;
        float        progress = 0.0f;
        EasingId     curve = EasingId::Lineal;
        Lifecycle    mode = Lifecycle::UnaVez;
    };

    // ── Núcleo compartido de la rampa escalar (SSOT §1.40) ──────────────────
    // evalTrack y evalEnvelope colapsan AQUÍ. Dos copias de esta lógica se
    // desincronizarían en silencio — la enfermedad del harness-réplica (§4),
    // dentro de la matemática. `t`/`dur` son progreso/duración en las unidades
    // del caller: segundos para un track temporal, avances `k`/`N` para uno
    // discreto o para la envolvente CP2.
    inline float evalScalarRamp(float from, float to, float dur,
                                EasingId curve, Lifecycle mode, float t)
    {
        if (dur <= 0.0f) return to;   // sin división por cero, sin NaN

        switch (mode)
        {
        case Lifecycle::Loop:
        {
            // Monótono con wrap: fmod preserva el sub-frame. LINEAL por ley
            // (§4.4) — un easing pulsaría en cada vuelta. La curva se ignora
            // a propósito; el parser ya avisa si la pediste.
            float x = std::fmod(t / dur, 1.0f);
            if (x < 0.0f) x += 1.0f;
            return from + (to - from) * x;
        }
        case Lifecycle::Vaiven:
        {
            // Ping-pong dentro del periodo: de → a → de.
            float ph = std::fmod(t / dur, 1.0f);
            if (ph < 0.0f) ph += 1.0f;
            float tri = (ph < 0.5f) ? (ph * 2.0f) : (2.0f - ph * 2.0f);
            return from + (to - from) * applyEasing(curve, tri);
        }
        case Lifecycle::UnaVez:
        default:
        {
            // Corre una vez y el valor final PERSISTE (clamp, no reinicio).
            float x = t / dur;
            if (x < 0.0f) x = 0.0f;
            if (x > 1.0f) x = 1.0f;
            return from + (to - from) * applyEasing(curve, x);
        }
        }
    }

    inline float evalTrack(const AnimTrack& tr, float t)
    {
        return evalScalarRamp(tr.de, tr.a, tr.dur, tr.curva, tr.modo, t);
    }

    // ── Reloj-diálogo CP2: la envolvente colapsa al MISMO núcleo (SSOT) ──────
    // progress/duration.amount hacen de t/dur. CP2 la usa una_vez +
    // DialogueAdvance; loop/vaiven quedan disponibles para un caller de
    // segundos (el codec E4 ya los admite).
    inline float evalEnvelope(const ScalarEnvelope& env)
    {
        return evalScalarRamp(env.from, env.to, env.duration.amount,
                              env.curve, env.mode, env.progress);
    }

    // CP2b §8: dos envolventes del mismo grupo (target, origen) deben ser
    // IDÉNTICAS en disco — comparación exacta de todos los campos que viajan en
    // el E4, incluidos reloj y progreso. Una discrepancia invalida el grupo.
    inline bool envelopesEqual(const ScalarEnvelope& a, const ScalarEnvelope& b)
    {
        return a.from == b.from && a.to == b.to &&
            a.duration.amount == b.duration.amount &&
            a.duration.clock == b.duration.clock &&
            a.progress == b.progress &&
            a.curve == b.curve && a.mode == b.mode;
    }

    // ── C-CP5: amplitud= es AZÚCAR — de/a resueltos por identidad de canal ──
    // Se resuelve al CONSTRUIR el track (§1.40: un solo punto de entrada);
    // evalTrack no sabe de canales. Validada en harness_anim (bloque 10) desde
    // `animacion-componible`; entra a producción con su primer caller real.
    //   aditivo (mover_*):     de=0, a=amplitud   — en reposo NO desplaza
    //   factor (escala/alpha): de=1, a=1-amplitud — ley de Pulse: amplitud=0.3
    //   atenúa hasta 0.7; negativa CRECE (escala amplitud=-0.2 → a=1.2)
    inline void amplitudToDeA(Channel c, float amplitud, float& de, float& a)
    {
        de = channelIdentity(c);
        a  = (channelIdentity(c) == 0.0f) ? amplitud : (1.0f - amplitud);
    }

    // ── C-CP5: los nombres del track (bilingüe, tabla única §7.205) ─────────
    // tinte/tint NO está a propósito: es CP6, y el caller lo dice con su
    // propia voz (caer al "no conozco" genérico sería mentir a medias).
    inline bool parseTrackChannel(const std::string& name, Channel& out)
    {
        if (name == "mover_x" || name == "move_x") { out = Channel::OffsetX; return true; }
        if (name == "mover_y" || name == "move_y") { out = Channel::OffsetY; return true; }
        if (name == "escala"  || name == "scale")  { out = Channel::Scale;   return true; }
        if (name == "alpha")                       { out = Channel::Alpha;   return true; }
        return false;
    }

    // Grafía CANÓNICA (español-v3, mesa-bump F3): va al eco y al `T;` del save.
    inline std::string trackChannelToString(Channel c)
    {
        switch (c)
        {
        case Channel::OffsetX: return "mover_x";
        case Channel::OffsetY: return "mover_y";
        case Channel::Scale:   return "escala";
        case Channel::Alpha:   return "alpha";
        case Channel::Tint:    return "tinte";
        }
        return "mover_y";
    }

    // La ley de 3 palabras (contrato C §4.4). `bucle` ya era el par de
    // capa_mover; `pingpong` es el término de asset-pipeline (Aseprite/Unity).
    inline bool parseLifecycle(const std::string& name, Lifecycle& out)
    {
        if (name == "una_vez" || name == "once")     { out = Lifecycle::UnaVez; return true; }
        if (name == "loop"    || name == "bucle")    { out = Lifecycle::Loop;   return true; }
        if (name == "vaiven"  || name == "pingpong") { out = Lifecycle::Vaiven; return true; }
        return false;
    }

    inline std::string lifecycleToString(Lifecycle m)
    {
        switch (m)
        {
        case Lifecycle::Loop:   return "loop";
        case Lifecycle::Vaiven: return "vaiven";
        case Lifecycle::UnaVez:
        default:                return "una_vez";
        }
    }

    // ── C-CP5: instancia viva de un track — el espejo de EffectInstance ─────
    // El track viaja RESUELTO (de/a materializados: ni el runtime ni el load
    // necesitan la definición — contrato C §7, "auto-contenidas"). `origen` es
    // la identidad de GRUPO: `detener <alias> <nombre>` baja el conjunto por
    // origen, y el nombre sobrevive al load como campo aditivo del `T;` (fork
    // firmado en mesa E-CP5: un guion que aplica la receta y la detiene tres
    // escenas después no debe romperse porque el jugador guardó en medio).
    struct TrackInstance
    {
        AnimTrack   track;
        std::string origen;
    };

    // ANIMAR-DESPUES-01: unica puerta de inicio para personaje, pantalla y
    // capa. Una demora vive en la instancia completa; todos sus canales
    // comparten la misma epoca y el evaluador existente sostiene `de` mientras
    // elapsed permanece negativo. La ausencia y cero conservan +0 exacto.
    inline TrackInstance startTrackInstance(const AnimTrack& source,
                                            const std::string& origin,
                                            float afterSeconds = 0.0f)
    {
        TrackInstance instance;
        instance.track = source;
        instance.track.elapsed = (afterSeconds > 0.0f)
            ? -afterSeconds : 0.0f;
        instance.origen = origin;
        return instance;
    }

    // Un track colapsa a AnimOutput por su canal. Alpha y escala se clampean
    // AQUÍ — el único punto por el que el valor entra al álgebra (§1.40):
    // `resorte` sobrepasa 1.0 A PROPÓSITO, y en un offset ese exceso es el
    // gracejo; en un factor sería un alpha >1 o una escala negativa que nadie
    // pidió. El parser avisa (alpha+resorte); esto es el cinturón.
    inline AnimOutput trackOutput(const AnimTrack& tr, float t)
    {
        const float v = evalTrack(tr, t);
        AnimOutput o;
        switch (tr.canal)
        {
        case Channel::OffsetX: o.dx = v; break;
        case Channel::OffsetY: o.dy = v; break;
        case Channel::Scale:   o.scale = (v < 0.0f) ? 0.0f : v; break;
        case Channel::Alpha:
            o.alpha = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
            break;
        case Channel::Tint:    break;   // CP6: sin superficie — identidad
        }
        return o;
    }

    // El stack de tracks de un blanco colapsa a UN AnimOutput — misma álgebra,
    // mismo río que composeStack (§7.208; stack vacío = identidad = off).
    inline AnimOutput composeTracks(const std::vector<TrackInstance>& tracks)
    {
        AnimOutput out;
        for (const auto& ti : tracks)
            out = compose(out, trackOutput(ti.track, ti.track.elapsed));
        return out;
    }

    // ── Reloj-diálogo CP1: leyes de progreso del track ─────────────────────
    // El runtime no decide el dominio con `if`s repartidos. Estas cuatro
    // operaciones son la frontera única entre ciclo de vida, espera y reposo.
    inline void tickTrackSeconds(AnimTrack& tr, float dt)
    {
        if (tr.clock == ClockDomain::Seconds)
            tr.elapsed += dt;
    }

    // true = el estado discreto avanzó. El caller puede comparar evalTrack
    // antes/después para decidir si hubo una contribución visual distinta.
    inline bool advanceDialogueTrack(AnimTrack& tr)
    {
        if (tr.clock != ClockDomain::DialogueAdvance ||
            tr.modo != Lifecycle::UnaVez || tr.dur <= 0.0f ||
            tr.elapsed >= tr.dur)
            return false;

        tr.elapsed = std::min(tr.elapsed + 1.0f, tr.dur);
        return true;
    }

    inline bool dialogueTrackPending(const AnimTrack& tr)
    {
        return tr.clock == ClockDomain::DialogueAdvance &&
            tr.modo == Lifecycle::UnaVez && tr.dur > 0.0f &&
            tr.elapsed < tr.dur;
    }

    // ── Reloj-diálogo CP2: leyes de progreso de la envolvente ───────────────
    // Espejo exacto de advanceDialogueTrack/dialogueTrackPending sobre la
    // envolvente. El `fx.elapsed` (segundos) del efecto dueño es independiente
    // de este progreso: ctx.t sigue fluido, ctx.intensity sube por escalones.
    inline bool advanceEnvelope(ScalarEnvelope& env)
    {
        if (env.duration.clock != ClockDomain::DialogueAdvance ||
            env.mode != Lifecycle::UnaVez || env.duration.amount <= 0.0f ||
            env.progress >= env.duration.amount)
            return false;

        env.progress = std::min(env.progress + 1.0f, env.duration.amount);
        return true;
    }

    inline bool envelopePending(const ScalarEnvelope& env)
    {
        return env.duration.clock == ClockDomain::DialogueAdvance &&
            env.mode == Lifecycle::UnaVez && env.duration.amount > 0.0f &&
            env.progress < env.duration.amount;
    }

    inline bool trackBlocksAnimationWait(const AnimTrack& tr)
    {
        return tr.clock == ClockDomain::Seconds &&
            tr.modo == Lifecycle::UnaVez && tr.dur > 0.0f &&
            tr.elapsed < tr.dur;
    }

    // ── C-CP5 × skip-frame: ¿el track sigue cambiando píxeles? ──────────────
    // Los tracks son fuente NUEVA del censo de quiescencia (§7.287: cada
    // superficie que se suma trae las suyas). Con el cuerpo leído (§7.282):
    // loop/vaiven respiran siempre; una_vez EN VUELO cambia; una_vez terminado
    // sostiene un valor FIJO → concede quietud, y su flanco de terminación lo
    // presenta el flush de borde de Engine::run (§7.285 — cero mecanismo nuevo).
    inline bool trackCambia(const AnimTrack& tr)
    {
        // DialogueAdvance no cambia con dt: entre dos eventos está quieto aun
        // cuando no haya llegado a su endpoint. El evento pide su propio frame.
        if (tr.clock == ClockDomain::DialogueAdvance) return false;
        if (tr.dur <= 0.0f) return false;               // evalTrack: constante
        if (tr.modo != Lifecycle::UnaVez) return true;  // loop/vaiven respiran
        return tr.elapsed < tr.dur;                     // en vuelo | terminado
    }

    // ── Constantes ───────────────────────────────────────────────────────

    constexpr float LAND_DURATION = 0.3f;  // Duración del aterrizaje suave

    // ── Update de animación ────────────────────────────────────────────────

    inline bool updateAnim(CharAnim& a, float dt)
    {
        if (a.type == AnimType::None) return false;

        // ── Aterrizaje suave para loops y persistentes ───────────────
        if (a.landing)
        {
            a.landTime += dt;
            float t = a.landTime / LAND_DURATION;
            if (t >= 1.0f)
            {
                // Aterrizaje completo
                a.type = AnimType::None;
                a.offsetX   = 0.0f;
                a.offsetY   = 0.0f;
                a.scale     = 1.0f;
                a.alpha     = 1.0f;
                a.landing   = false;
                return false;
            }

            float eased = easing::easeOutQuad(t);

            if (a.type == AnimType::Float ||
                a.type == AnimType::Drop ||
                a.type == AnimType::Rise)
            {
                a.offsetY = a.landFrom * (1.0f - eased);
            }
            else if (a.type == AnimType::Pulse)
            {
                a.alpha = a.landFrom + (1.0f - a.landFrom) * eased;
            }

            return true;
        }

        a.elapsed += dt;

        // Reset por defecto
        a.offsetX    = 0.0f;
        a.offsetY    = 0.0f;
        a.scale      = 1.0f;
        a.alpha      = 1.0f;
        a.tint       = 1.0f;
        a.flashAlpha = 0.0f;

        float t = 0.0f;

        switch (a.type)
        {
            case AnimType::Shake:
            {
                if (a.elapsed >= a.duration)
                {
                    a.type = AnimType::None;
                    return false;
                }
                t = a.elapsed / a.duration;
                float decay   = (1.0f - t) * (1.0f - t);
                if (decay < 0.001f)
                {
                    a.type = AnimType::None;
                    return false;
                }
                float freq    = 25.0f;
                float maxDisp = 8.0f * a.intensity;
                a.offsetX = std::sin(a.elapsed * freq * 6.28f) * maxDisp * decay;
                break;
            }

            case AnimType::Bounce:
            {
                if (a.elapsed >= a.duration)
                {
                    a.type = AnimType::None;
                    return false;
                }
                t = a.elapsed / a.duration;
                float height = 20.0f * a.intensity;
                a.offsetY = -height * 4.0f * t * (1.0f - t);
                break;
            }

            case AnimType::ZoomIn:
            {
                float targetScale = 1.0f + 0.15f * a.intensity;
                if (a.elapsed >= a.duration)
                {
                    a.scale = targetScale;
                    return true;
                }
                t = a.elapsed / a.duration;
                float eased = easing::easeOutQuad(t);
                a.scale = 1.0f + (targetScale - 1.0f) * eased;
                break;
            }

            case AnimType::ZoomOut:
            {
                if (a.elapsed >= a.duration)
                {
                    a.scale = 1.0f;
                    a.type = AnimType::None;
                    return false;
                }
                t = a.elapsed / a.duration;
                float eased = easing::easeOutQuad(t);
                float startScale = a.fromScale;
                a.scale = startScale + (1.0f - startScale) * eased;
                break;
            }

            case AnimType::Float:
            {
                float amplitude = 10.0f * a.intensity;
                float freq = 1.5f;
                a.offsetY = -std::sin(a.elapsed * freq * 6.28f) * amplitude;
                break;
            }

            case AnimType::Pulse:
            {
                float minAlpha = 1.0f - 0.5f * a.intensity;
                if (minAlpha < 0.1f) minAlpha = 0.1f;
                float wave = 0.5f + 0.5f * std::sin(a.elapsed * 3.0f * 6.28f);
                a.alpha = minAlpha + (1.0f - minAlpha) * wave;
                break;
            }

            case AnimType::Nudge:
            {
                // Vaivén suave entre 0 y -amp (px). easeIn al salir, easeOut al
                // volver — el par DDLC ctc (Ren'Py easeout/easein van nombrados
                // al revés del estándar). Offset PURO: el alpha lo compone el
                // blink del consumidor, no esta anim.
                float period = (a.period > 0.0f) ? a.period : 1.5f; // ciclo DDLC
                float phase = std::fmod(a.elapsed, period) / period; // 0..1
                float amp = a.intensity;                           // px
                float disp;
                if (phase < 0.5f) {
                    float lt = phase / 0.5f;               // salida: easeIn
                    disp = -amp * (1.0f - lt * lt);        // -amp → 0
                }
                else {
                    float lt = (phase - 0.5f) / 0.5f;      // vuelta: easeOut
                    disp = -amp * easing::easeOutQuad(lt); // 0 → -amp
                }
                if (a.axisVertical) a.offsetY = disp;
                else                a.offsetX = disp;
                break;
            }

            case AnimType::TintFlash:
            {
                if (a.elapsed >= a.duration)
                {
                    a.flashAlpha = 0.0f;
                    a.type = AnimType::None;
                    return false;
                }
                t = a.elapsed / a.duration;
                float flash;
                if (t < 0.15f)
                    flash = t / 0.15f;
                else
                    flash = 1.0f - (t - 0.15f) / 0.85f;
                a.flashAlpha = flash * a.intensity;
                if (a.flashAlpha > 1.0f) a.flashAlpha = 1.0f;
                break;
            }

            // `case AnimType::Glitch` murió (E-CP1). Su ciclo de vida lo lleva
            // ahora `updateEffect` (EffectSystem.h): misma ley, otro piso.

            // ── Drop: sprite baja y SE QUEDA ─────────────────────────
            case AnimType::Drop:
            {
                float targetY = 30.0f * a.intensity;
                if (a.elapsed >= a.duration)
                {
                    a.offsetY = targetY;
                    return true;  // Persiste
                }
                t = a.elapsed / a.duration;
                a.offsetY = targetY * easing::easeOutQuad(t);
                break;
            }

            // ── Rise: sprite sube y SE QUEDA ─────────────────────────
            case AnimType::Rise:
            {
                float targetY = -30.0f * a.intensity;
                if (a.elapsed >= a.duration)
                {
                    a.offsetY = targetY;
                    return true;  // Persiste
                }
                t = a.elapsed / a.duration;
                a.offsetY = targetY * easing::easeOutQuad(t);
                break;
            }

            // ── Hop: brinquito rápido, vuelve a normal ───────────────
            case AnimType::Hop:
            {
                if (a.elapsed >= a.duration)
                {
                    a.type = AnimType::None;
                    return false;
                }
                t = a.elapsed / a.duration;
                float height = 12.0f * a.intensity;
                a.offsetY = -height * 4.0f * t * (1.0f - t);
                break;
            }

            // ── LeanBack: salto hacia atrás + vuelve a normal ────────
            // Salta hacia arriba mientras se aleja (scale baja),
            // luego aterriza de vuelta en posición normal.
            case AnimType::LeanBack:
            {
                if (a.elapsed >= a.duration)
                {
                    a.scale = 1.0f;
                    a.offsetY = 0.0f;
                    a.type = AnimType::None;
                    return false;
                }
                t = a.elapsed / a.duration;

                // Arco parabólico: salta hacia arriba y aterriza
                float jumpH = 25.0f * a.intensity;
                float arc = 4.0f * t * (1.0f - t);

                a.offsetY = -jumpH * arc;

                // Scale: de fromScale (acercado) a 1.0, siguiendo el arco
                // En el pico del salto ya está casi en 1.0
                float progress = easing::easeOutQuad(t);
                a.scale = a.fromScale + (1.0f - a.fromScale) * progress;

                break;
            }

            default:
                break;
        }

        return true;
    }

    // ── Iniciar aterrizaje suave ─────────────────────────────────────────

    inline void startLanding(CharAnim& a)
    {
        if (!a.isActive()) return;

        // Loops con offset Y: landing suave
        if (a.type == AnimType::Float)
        {
            a.landing  = true;
            a.landFrom = a.offsetY;
            a.landTime = 0.0f;
        }
        // Pulse: landing suave de alpha
        else if (a.type == AnimType::Pulse)
        {
            a.landing  = true;
            a.landFrom = a.alpha;
            a.landTime = 0.0f;
        }
        // Persistentes con offset Y: landing suave de vuelta a 0
        else if (a.type == AnimType::Drop || a.type == AnimType::Rise)
        {
            a.landing  = true;
            a.landFrom = a.offsetY;
            a.landTime = 0.0f;
        }
        else
        {
            // Sin aterrizaje suave (shake, bounce, glitch, etc.)
            a.type = AnimType::None;
        }
    }

    // ── Helpers de parsing ──────────────────────────────────────────────────

    inline AnimType parseAnimType(const std::string& name)
    {
        if (name == "shake" || name == "temblar")         return AnimType::Shake;
        if (name == "bounce" || name == "saltar")         return AnimType::Bounce;
        if (name == "zoom_in" || name == "acercar")       return AnimType::ZoomIn;
        if (name == "zoom_out" || name == "alejar")       return AnimType::ZoomOut;
        if (name == "float" || name == "flotar")          return AnimType::Float;
        if (name == "pulse" || name == "pulso")           return AnimType::Pulse;
        if (name == "nudge" || name == "vaiven")          return AnimType::Nudge;
        if (name == "tint_flash" || name == "destello")   return AnimType::TintFlash;
        // "glitch" ya NO se resuelve aquí: lo agarra `parseFxKind` (E-CP1).
        if (name == "drop" || name == "caer")             return AnimType::Drop;
        if (name == "rise" || name == "subir")            return AnimType::Rise;
        if (name == "hop" || name == "brincar")           return AnimType::Hop;
        if (name == "lean_back" || name == "retroceder")  return AnimType::LeanBack;
        return AnimType::None;
    }

    inline std::string animTypeToString(AnimType t)
    {
        switch (t)
        {
            case AnimType::Shake:     return "shake";
            case AnimType::Bounce:    return "bounce";
            case AnimType::ZoomIn:    return "zoom_in";
            case AnimType::ZoomOut:   return "zoom_out";
            case AnimType::Float:     return "float";
            case AnimType::Pulse:     return "pulse";
            case AnimType::Nudge:     return "nudge";
            case AnimType::TintFlash: return "tint_flash";
            case AnimType::Drop:      return "drop";
            case AnimType::Rise:      return "rise";
            case AnimType::Hop:       return "hop";
            case AnimType::LeanBack:  return "lean_back";
            default:                  return "none";
        }
    }

    // ── El espejo ESPAÑOL del writer v3 (CP-BUMP, fallo F3 de la mesa) ──────
    // animTypeToString (arriba) canoniza a inglés y queda como voz del lector
    // v2 CONGELADO y de los logs viejos. El writer v3 emite ESTAS grafías —
    // las mismas que parseAnimType ya conocía: no se inventa ni una palabra,
    // solo se elige la columna española de la tabla bilingüe.
    inline std::string animTypeToStringEs(AnimType t)
    {
        switch (t)
        {
            case AnimType::Shake:     return "temblar";
            case AnimType::Bounce:    return "saltar";
            case AnimType::ZoomIn:    return "acercar";
            case AnimType::ZoomOut:   return "alejar";
            case AnimType::Float:     return "flotar";
            case AnimType::Pulse:     return "pulso";
            case AnimType::Nudge:     return "vaiven";
            case AnimType::TintFlash: return "destello";
            case AnimType::Drop:      return "caer";
            case AnimType::Rise:      return "subir";
            case AnimType::Hop:       return "brincar";
            case AnimType::LeanBack:  return "retroceder";
            default:                  return "none";
        }
    }

    // ── El vocabulario de claves de CADA animación (espejo de `fxKeysOf`) ────
    //
    // El parser validaba las claves contra un vocabulario GLOBAL — una cadena de
    // `else if` que nunca preguntaba de QUÉ animación se trataba. Resultado:
    //
    //     animar m flotar periodo=2 eje=y    → aceptado. NO HACE NADA. En silencio.
    //     animar m temblar eje=y             → aceptado. Ignorado.
    //
    // Y el aviso mentía por su cuenta: a un `flotar` le decía que sus parámetros
    // eran `periodo=` y `eje=`, que no son suyos. Es EXACTAMENTE la grieta que
    // E-CP0 cerró del lado de los efectos, viva del lado de las animaciones
    // (§7.166: superficie que el parser acepta y ningún update lee).
    //
    // `periodo` y `eje` los lee SOLO `vaiven` (Nudge). Nadie más.
    // `intensidad` es universal, igual que en los efectos.
    inline std::vector<std::string> animKeysOf(AnimType t)
    {
        if (t == AnimType::Nudge) return { "periodo", "eje" };
        return {};
    }

    // Canoniza la grafía bilingüe → español. UNA sola grafía río abajo.
    inline std::string canonAnimKey(const std::string& k)
    {
        if (k == "intensity") return "intensidad";
        if (k == "period")    return "periodo";
        if (k == "axis")      return "eje";
        return k;
    }

    inline bool animAcceptsKey(AnimType t, const std::string& canonKey)
    {
        if (canonKey == "intensidad") return true;   // universal
        for (const auto& s : animKeysOf(t))
            if (s == canonKey) return true;
        return false;
    }

    inline std::string animKeysHelp(AnimType t)
    {
        std::string s;
        for (const auto& k : animKeysOf(t)) s += k + "= ";
        s += "intensidad=";
        return s;
    }

    inline float animDefaultDuration(AnimType t)
    {
        switch (t)
        {
            case AnimType::Shake:     return 0.5f;
            case AnimType::Bounce:    return 0.4f;
            case AnimType::ZoomIn:    return 0.3f;
            case AnimType::ZoomOut:   return 0.3f;
            case AnimType::Float:     return 0.0f;
            case AnimType::Pulse:     return 0.0f;
            case AnimType::Nudge:     return 0.0f;
            case AnimType::TintFlash: return 0.4f;
            case AnimType::Drop:      return 0.25f;
            case AnimType::Rise:      return 0.25f;
            case AnimType::Hop:       return 0.2f;
            case AnimType::LeanBack:  return 0.35f;
            default:                  return 0.5f;
        }
    }

} // namespace eclips
