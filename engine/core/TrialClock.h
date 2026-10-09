// ============================================================================
// TrialClock.h — el ENSAYO: contrato puro del eje CUANDO del azar
// ============================================================================
//
// Nucleo de la pierna A de `DISENO_AZAR_ECLIPS.md` (SYN-4b / R-VN-07).
//
// ── QUE ARREGLA ─────────────────────────────────────────────────────────────
//
// `probabilidad` nunca fue la palabra equivocada: le faltaba el ENSAYO. Una
// probabilidad necesita un ensayo al que pertenecer, y el motor nunca lo
// declaro — eligio "cada frame" como default mudo (`roll < chance * dt`), y la
// TASA salio como consecuencia de esa omision, no como decision. De ahi la nota
// al pie del 63%: existia para explicar un accidente.
//
// Declarar el ritmo (`cada=`, default 1s) vuelve las dos palabras literalmente
// ciertas: `probabilidad=0.2` es 20% POR BEAT, exacto, a cualquier framerate.
//
//   ley vieja:  probabilidad=1 -> 63.5% a 60fps, 63.7% a 30fps, y saturaba
//   ley nueva:  probabilidad=1 -> dispara en cada beat, siempre, en todas
//
// ── POR QUE ES UN HEADER APARTE ─────────────────────────────────────────────
//
// Igual que `AnimationClock.h` (CP0 del reloj de dialogo) y `IntroTimeline.h`:
// `VNScriptState.h` arrastra SDL/GL/Lua/audio/estados y NO es alcanzable desde
// una TU standalone, asi que una ley que viva ahi adentro no se puede probar
// (lo midio `sonda_alcance_scenelayer.cpp` en RT-1). El gate de esta pierna
// pide harness rojo->verde MAS sonda de distribucion con semilla: con el tick
// inline, ese gate no se podia cumplir. `harness_azar.cpp` prueba este archivo.
//
// Y no es solo por el test: las piernas B (bloque `azar:`) y C (gate `[si:]`)
// cuelgan del mismo primitivo. Escrito inline seria la septima copia de la
// enfermedad que el drenaje SYN viene deshaciendo (4 parsers de hex, 3 leyes de
// desempate, 4 de comillas, 5 de corchetes, 61 `stof` sin `&pos`).
//
// ── LAS LEYES QUE VIVEN AQUI (DISENO_AZAR_ECLIPS seccion 5) ─────────────────
//
//   ley 1   el ensayo es explicito: `cada=` da el ritmo, `probabilidad=` la
//           tirada. Sin `cada`, el default es 1s
//   ley 7   el RELOJ se guarda (`beatRemaining`), el SORTEO no: la semilla NO
//           vive en este struct, la pone el caller
//   ley 9   rango con voz: probabilidad en [0,1], `cada` con piso de 0.05s
//           (copiado de `fxSorteoIntervalo`, con su razon escrita: "media chica
//           + varianza grande no ametralla"), MAXIMO UNA TIRADA POR FRAME
//   ley 10  primer beat DESPUES de un intervalo, con fase inicial aleatoria por
//           evento: sin ella, tres eventos con cada=5s tiran juntos y se oye el
//           pulso
//   ley 11  el cooldown veta LA TIRADA, no EL BEAT: el reloj del ritmo corre
//           siempre
//   4.3     el GATE `[si:]` por evento (pierna C, seccion 4.3 del contrato): se
//           evalua EN EL BEAT — despues de consumirlo, antes de la tirada — y
//           entra como PREDICADO, que es lo que hace ciertas las dos mitades
//           por construccion en vez de por promesa del caller
//   ley 12  la semilla es reproducible — y aca eso significa algo mas fuerte
//           que "hay una semilla": la tirada se consume POR BEAT, no por frame,
//           asi que la misma semilla da la misma corrida a cualquier fps
//
// El PARSER habla (con numero de linea) y este nucleo CLAMPEA. Es la misma
// reparticion que `evento_random` ya tenia escrita: "el rango se clampea en UN
// lugar, y ninguna otra pieza vuelve a validarlo".
// ============================================================================

#pragma once

#include "Types.h"

#include <cmath>
#include <cstdint>

namespace eclips
{
    // El default que hace verdadera la palabra: con `cada=1s`,
    // `probabilidad=0.2` significa 20% por segundo, y lo significa EXACTO.
    inline constexpr f64 kTrialDefaultPeriod = 1.0;

    // Piso del ritmo. No es un numero inventado: es el mismo de
    // `fxSorteoIntervalo` (EffectSystem.h), que ya lo tenia por la misma razon.
    inline constexpr f64 kTrialMinPeriod = 0.05;

    // Las causas del silencio, que la traza (seccion 6 del contrato) necesita
    // poder nombrar: con cuatro ejes encima, "no paso nada" tiene cinco causas
    // y ninguna tenia voz.
    enum class TrialOutcome : std::uint8_t
    {
        NoBeat = 0,     // el beat todavia no llego (silencio normal, no es falla)
        ArbitrajeVeto,  // otro evento gano ESTE frame: el beat se RETIENE
        GateVeto,       // hubo beat, el gate `[si:]` dijo que no (pierna C)
        CooldownVeto,   // hubo beat, el cooldown veto la tirada (ley 11)
        OcupadoVeto,    // hubo beat, el objetivo esta ocupado: se SALTA (ley 2)
        Lost,           // tiro y perdio
        Won             // tiro y gano
    };

    // El gate SIEMPRE abierto: el default, y lo que usan los eventos sin `[si:]`.
    // Es un tipo vacio a proposito — un evento sin gate no paga ni una llamada
    // indirecta, porque el `if (!gate())` se pliega en compilacion.
    struct TrialGateAbierto
    {
        bool operator()() const { return true; }
    };

    // El ensayo de UN evento. Todo lo que no es reloj vive afuera (el nombre, el
    // destino, el gate) — y el sorteo tambien: ver ley 7.
    //
    // Pierna C no rompe esa frase, la precisa: los DATOS del gate (la variable,
    // el operador, el valor) siguen viviendo afuera, en `RandomEventDef`. Lo
    // que entra aca es la DECISION ya tomada, igual que `objetivoOcupado` —
    // solo que como predicado y no como bool, para que se evalue en el BEAT y
    // no en cada frame (contrato seccion 4.3). Ver `advanceTrial`.
    struct TrialClock
    {
        f64 period            = kTrialDefaultPeriod; // `cada=`, ya saneado
        f32 chance            = 0.0f;                // `probabilidad=`, ya en [0,1]
        f64 cooldown          = 0.0;                 // `cooldown=` configurado
        f64 beatRemaining     = 0.0;                 // FASE — se serializa (ley 7)
        f64 cooldownRemaining = 0.0;                 // se lava al cargar (ley 7)
    };

    // ── Saneamiento (ley 9) ─────────────────────────────────────────────────
    // Un no-numero no es "muy chico": cae al DEFAULT, no al piso. Un numero
    // valido pero por debajo del piso si sube al piso — eso es un ritmo que el
    // creador pidio, solo que mas rapido de lo que el motor sostiene sin
    // ametrallar.
    inline f64 clampTrialPeriod(f64 raw)
    {
        if (!std::isfinite(raw))    return kTrialDefaultPeriod;
        if (raw < kTrialMinPeriod)  return kTrialMinPeriod;
        return raw;
    }

    // Fail-closed en el no-numero: un evento que no se entiende no dispara.
    inline f32 clampTrialChance(f32 raw)
    {
        if (!std::isfinite(raw)) return 0.0f;
        if (raw < 0.0f)          return 0.0f;
        if (raw > 1.0f)          return 1.0f;
        return raw;
    }

    // ── La tirada, en [0,1) ─────────────────────────────────────────────────
    // El span+1 no es cosmetico: con `rng() / rng.max()` la tirada PUEDE dar
    // exactamente 1.0, y entonces `probabilidad=1` fallaria una vez cada 4 mil
    // millones de beats. "Con 1 dispara en cada beat" es una ley firmada — y es
    // justo el criterio de aceptacion que la mesa del revival dio por bueno
    // cuando era falso. Mismo idioma que `fxSorteoIntervalo`.
    template <class Rng>
    inline f64 trialRoll01(Rng& rng)
    {
        const f64 lo   = (f64)Rng::min();
        const f64 span = (f64)Rng::max() - lo + 1.0;
        return ((f64)rng() - lo) / span;
    }

    // ── La fase inicial (ley 10) ────────────────────────────────────────────
    // Cae en (0, period]: nunca 0 — el primer beat llega DESPUES de un
    // intervalo, no en el primer frame — y nunca mas alla de un periodo, para
    // que `cada=30s` no haga esperar 45s al primero.
    template <class Rng>
    inline f64 trialPhase(f64 period, Rng& rng)
    {
        return period * (1.0 - trialRoll01(rng));
    }

    // ── El beat ─────────────────────────────────────────────────────────────
    // Consume el rng SOLO cuando hay tirada (ley 12): por eso la misma semilla
    // da la misma corrida a 60 y a 30 fps, y por eso `azar_semilla` sirve para
    // balancear pesos y no solo para un test.
    //
    // DOS VETOS QUE PARECEN UNO Y NO LO SON. Los separo porque confundirlos
    // mata de hambre a un evento en silencio (lo destapo el feel-test del
    // fundador, 2026-07-25 — ver harness_azar seccion 5c):
    //
    //  · `puedeTirar=false` — ARBITRAJE. Otro evento gano ESTE frame. Dura un
    //    frame, asi que el beat se **RETIENE**: se cobra al siguiente. Si en
    //    cambio se saltara, dos eventos con el mismo `cada` cuyas fases caen en
    //    el mismo frame volverian a colisionar en cada ciclo —la colision es
    //    PERMANENTE, porque comparten periodo— y el segundo no dispararia
    //    JAMAS. Con la ley vieja no podia pasar: todos tiraban cada frame.
    //
    //  · `objetivoOcupado=true` — LEY 2, *un beat no llega a destiempo: se
    //    salta*. La ocupacion dura un RATO (Monika leyendo), y ahi encolar es
    //    peor que perder: la accion saldria fuera de contexto. El beat se
    //    **CONSUME**. Hoy no tiene caller —la ocupacion es global e implicita—;
    //    lo estrena la pierna D, cuando sea por objeto.
    //
    // En los dos casos el RELOJ corre igual, por la misma razon que bajo
    // cooldown: si se parara, al soltarse todos re-fasearian juntos y se oiria
    // el pulso que la ley 10 existe para evitar.
    // `rollOut` existe para LA TRAZA (contrato §6): sin el numero, "tiro y
    // perdio" y "el gate dijo que no" se ven igual desde afuera, que es
    // exactamente la confusion que la traza viene a matar. Solo se escribe
    // cuando hubo tirada de verdad.
    //
    // ── EL TERCER VETO: EL GATE (pierna C, contrato seccion 4.3) ────────────
    //
    // `gate` es un PREDICADO, no un bool, y esa es toda la diferencia entre una
    // ley por intencion y una por construccion:
    //
    //  · se invoca DESPUES de confirmar el beat, asi que "se evalua en el beat,
    //    no cada frame" no es una promesa del caller — es la forma de la
    //    funcion. Con un bool, el caller tendria que resolver la variable en
    //    los 59 frames que NO son beat para pasar el argumento;
    //  · se invoca ANTES de la tirada, asi que un gate cerrado NO consume rng.
    //    Filtrar despues del roll daria el mismo silencio y costaria distinto:
    //    con la misma semilla, la corrida de los DEMAS eventos cambiaria segun
    //    un gate que dijo que no. Eso rompe la ley 12, y en silencio.
    //
    // El beat SI se consume (se re-armo arriba): el reloj del ritmo corre
    // siempre, igual que bajo cooldown (ley 11). Por eso, cuando el gate abre,
    // el evento espera al proximo beat en fase en vez de disparar en el
    // instante — retenerlo obligaria ademas a evaluar cada frame.
    //
    // Y va ANTES que el cooldown a proposito: si las dos causas estan activas,
    // la que el creador puede arreglar es el gate, y la traza nombra una sola.
    template <class Rng, class GateFn = TrialGateAbierto>
    inline TrialOutcome advanceTrial(TrialClock& t, f64 dt, Rng& rng,
                                     bool puedeTirar = true,
                                     bool objetivoOcupado = false,
                                     f64* rollOut = nullptr,
                                     GateFn gate = GateFn{})
    {
        // El cooldown corre SIEMPRE, tambien en los frames sin beat.
        if (t.cooldownRemaining > 0.0)
        {
            t.cooldownRemaining -= dt;
            if (t.cooldownRemaining < 0.0) t.cooldownRemaining = 0.0;
        }

        // El reloj del ritmo corre SIEMPRE (ley 11): el cooldown no lo detiene.
        t.beatRemaining -= dt;
        if (t.beatRemaining > 0.0) return TrialOutcome::NoBeat;

        // Hay beat, pero otro evento ya gano el frame: se RETIENE sin re-armar,
        // asi que el proximo frame vuelve a haber beat y este evento cobra. Es
        // lo unico que impide que dos fases colisionadas se maten de hambre
        // para siempre (ver el comentario de arriba). Ademas los SEPARA: el
        // retenido dispara un frame despues y ya no vuelven a coincidir.
        if (!puedeTirar) return TrialOutcome::ArbitrajeVeto;

        // Re-armar la fase conservando el resto, para que el ritmo no derive
        // con el framerate...
        t.beatRemaining += t.period;
        // ...pero MAXIMO UNA TIRADA POR FRAME (ley 9): si el dt se comio varios
        // periodos (hitch, o `cada` por debajo del frame), los sobrantes se
        // DESCARTAN. Un frame gordo no es una rafaga acumulada.
        if (t.beatRemaining <= 0.0) t.beatRemaining = t.period;

        // El gate, ya con el beat consumido y todavia sin tocar el rng.
        if (!gate()) return TrialOutcome::GateVeto;

        if (t.cooldownRemaining > 0.0) return TrialOutcome::CooldownVeto;
        if (objetivoOcupado)           return TrialOutcome::OcupadoVeto;

        const f64 roll = trialRoll01(rng);
        if (rollOut) *rollOut = roll;
        return (roll < (f64)t.chance) ? TrialOutcome::Won : TrialOutcome::Lost;
    }

    // Al ganar: el cooldown arranca. Se deja aparte del beat a proposito — el
    // caller decide si la victoria realmente se consumio (en la pierna D, un
    // objetivo ocupado puede vetarla despues de la tirada).
    inline void trialArmCooldown(TrialClock& t)
    {
        t.cooldownRemaining = t.cooldown;
    }
}
