// ============================================================================
// DialogBox.cpp — Implementación del componente de diálogo (v2)
// ============================================================================

#include "VNDialogBox.h"
#include "engine/render/Draw2D.h"
#include "engine/core/Settings.h"
#include "engine/core/ProjectManifest.h"  // PersonajeManifest (cadena de velocidad)
#include "engine/core/AssetManager.h"     // Resolución de paths de sprites de caja (pasada 3 PNG)
#include "engine/render/Texture.h"        // Texture* devuelto por AssetManager::get
#include "engine/render/FontManager.h"    // fuente inline: m_fm->get en render de texto rico
#include "engine/render/TweenSystem.h"    // CP-vaiven: indicador consume updateAnim (Nudge)
#include <iostream>
#include "engine/core/LangStore.h"   // idiomas CP3: langUI() del chrome

#include <algorithm>
#include <cmath>
#include <set>

const eclips::Theme& eclips::VNDialogBox::fallbackTheme()
{
    static const Theme s_fallback = Theme::defaults();
    return s_fallback;
}

// ── Helpers de runtime que honran theme_locked ──────────────────────────────
//
// Reflejo del patrón "effectiveXxx" que SettingsState.h usa internamente.
// Aquí lo replicamos para que el dialog box en runtime también respete el
// lock del creador — sin esto, el menú mostraría "Rápido / 1 segundo"
// (locked) mientras el typewriter corre con el override viejo del jugador
// (ej. "instantáneo / 5 segundos") creado en otro tema sin locks.
//
// Cuando el lock está activo, devolvemos el valor del theme directamente,
// ignorando cualquier override persistido. El override sigue vivo en
// Settings — revive si el creador quita el lock.
//
// FUTURO: si crece el número de campos del patrón, esto migra a un header
// común (ej. ThemeRuntime.h o métodos en Theme.h). Hoy solo VNDialogBox
// los necesita, así que namespace anónimo local es lo correcto (§7.3).

namespace {
    inline eclips::TextSpeed effective_textSpeed_full(const eclips::Theme& t,
        const eclips::PersonajeManifest* speaker,
        const std::optional<eclips::TextSpeed>& lineSpeedOverride) {
        const std::optional<eclips::TextSpeed> character =
            speaker ? speaker->velocidadTexto : std::nullopt;
        const std::optional<eclips::TextSpeed> theme = t.textSpeed;
        const eclips::TextSpeedResolution resolution =
            eclips::resolveTextSpeed(lineSpeedOverride, character,
            speaker && speaker->velocidadLock, theme, t.textSpeedLocked,
            eclips::Settings::textSpeedOverride());
        if (resolution.source == eclips::TextSpeedSource::Player &&
            t.textSpeedDomain.isNumeric())
            return eclips::Settings::projectTextSpeedForDomain(t.textSpeedDomain);
        return resolution.value;
    }

    inline float effective_autoDelay(const eclips::Theme& t) {
        return t.autoDelayLocked
            ? t.autoDelay
            : eclips::Settings::autoDelay(t.autoDelay);
    }
    inline float effective_skipInterval(const eclips::Theme& t) {
        return t.skipIntervalLocked
            ? t.skipInterval
            : eclips::Settings::skipInterval(t.skipInterval);
    }
}

void eclips::VNDialogBox::setTextRenderers(TextRenderer* textRenderer, TextRenderer* nameRenderer)
{
    m_text = textRenderer;
    m_name = nameRenderer;
    m_hasFont = (m_text != nullptr);
    m_hasNameFont = (m_name != nullptr);
}

void eclips::VNDialogBox::startDialogue(const std::string& speaker,
    const RichText& rich)
{
    setPhase(Phase::Dialogue);
    m_choices.clear();
    m_pendingChoice = false;
    m_currentSpeaker = speaker;
    m_displayText = rich.displayText;
    m_charEffects = rich.effects;
    m_lineSpeedOverride = rich.lineSpeedOverride; // capturado del RichText
    m_lineInterrupts = rich.interrupts;           // {interrumpir}: auto-advance al completar
    m_typewriterChars = 0;
    m_typewriterTimer = 0.0f;
    m_pauseTimer = 0.0f;
    m_waitingMidClick = false;
    m_autoTimer = 0.0f;
    m_skipTimer = 0.0f;
    m_interruptTimer = 0.0f;                      // reset del respiro de {interrumpir}
    m_hidden = false; // auto-show al iniciar nuevo diálogo (decisión 1A)
    // `ELEGIR-CON-CAJA` (ley 3 de `BoxPersistence.h`): el pedido dura UNA
    // línea. Misma vida útil que `m_hidden` acá arriba, y ésa es la razón —
    // dos relojes en el mismo objeto es como se desincronizan las cosas.
    m_persist.newLine();

    // ── `caja-entrada`: la entrada, y el retraso del texto ──────────────────
    // Si el jugador no me estaba viendo, entro fundiendo. Entre dos líneas
    // seguidas esto es un no-op — el frame anterior me dibujó.
    // LA CAPTURA (`AppearPolicy.h` ley 3): la política se lee del estilo UNA
    // vez, acá, y de ahí en más la aparición es dueña de la suya. Antes de este
    // lote la duración se releía en cada frame del render, así que un
    // `usar_caja` a mitad movía la opacidad sin que el reloj se moviera — el
    // gate `FO-2` lo midió: 1.00 -> 0.06 hacia una caja más lenta.
    const DialogBoxStyle& estilo = activeStyle();
    m_appear.arm(AppearPolicy{ estilo.fadeInDuration, estilo.textDelay });

    // "Pertenece a una aparición" es armarla O incorporarse a una que nadie
    // renderizó todavía (§5.2 punto 2). Las dos comparten el mismo retraso de
    // texto, que es el capturado — no el que el tema diga en este instante.
    const bool esAparicion = m_appear.inAppearance();

    // EL PUNTO ÚNICO del retraso del texto. Dos fuentes, un solo mecanismo:
    // `m_pauseTimer`, el mismo que ya frena al typewriter para `{pausa=N}`
    // mid-línea. Escribirlo acá y no en dos lados es lo que evita que "el
    // texto arranca tarde" tenga dos implementaciones que se desincronicen.
    //
    // LA LEY: el más específico gana, igual que la cadena de velocidad. Una
    // línea que pide su retraso con `{pausa=N}` al principio manda sobre el
    // del tema — y manda SIEMPRE, sea aparición o no, porque el creador lo
    // escribió en ESA línea a propósito (es el efecto dramático, y puede no
    // tener nada que ver con que la caja aparezca).
    // El del tema, en cambio, sólo se arma en la APARICIÓN: si se armara en
    // cada línea sería un cambio de ritmo global, y eso ya tiene su perilla.
    //
    // `vocabulario-lote3` (`R-03`): la ley VIVE en `AppearPolicy.h` (ley 8) y
    // acá sólo se la llama. Lo que cambió es el predicado — antes era
    // `rich.openingPause > 0.0f`, o sea que **`{pausa=0}` no podía ganar** y la
    // línea caía al retraso del tema; ahora pregunta por la PRESENCIA, así que
    // un cero escrito significa *«esta línea arranca ya»* y cancela lo heredado.
    //
    // El gate de skip es el mismo que usa la pausa mid-línea al ARMARSE
    // (`VNDialogBox.cpp:302`): si no, saltear el guion pagaría el retraso en
    // cada línea y el skip dejaría de sentirse como skip. Vive acá y no en la
    // ley porque depende de estado de input que ese header no ve.
    if (!skipActive())
        m_pauseTimer = resolveTextDelay(rich.hasOpeningPause, rich.openingPause,
                                        esAparicion, m_appear.policy().textDelay);
}

void eclips::VNDialogBox::startChoices(const std::vector<EclipChoice>& choices,
    float timer, int defaultIndex)
{
    setPhase(Phase::Choice);
    m_autoMode = false;
    m_skipToggle = false;
    m_autoTimer = 0.0f;
    m_skipTimer = 0.0f;
    // ── `ELEGIR-CON-CAJA`: el texto YA NO SE BORRA ──────────────────────────
    //
    // Acá vivía `m_displayText.clear()` + `m_charEffects.clear()`, y era la
    // MITAD OCULTA de por qué la variante no se podía: aun agregando la llamada
    // de render, la caja habría salido vacía. El texto se congela para que la
    // última línea siga leyéndose junto a las opciones.
    //
    // Y esto NO es gratis: el bloque de typewriter/auto/skip de `update()`
    // estaba gateado por "hay texto" y funcionaba porque el texto desaparecía
    // acá. Con el texto vivo, ese gate pasa a ser la FASE — `dialogueClockRuns`
    // (ley 5). Sin eso, `{interrumpir}` o una `A` apretada emiten `Advance` en
    // plena elección: el camino de §7.74, el `.ecl` que se cerraba solo.
    //
    // Sin `mostrar_caja` nada de esto se ve: la caja no se dibuja igual, así
    // que el texto congelado es invisible y la conducta es la de siempre.
    m_choices = choices;
    m_choiceScale.assign(m_choices.size(), 1.0f); // F2a: arrancan en reposo (sin pop-in)
    m_choiceTimer = timer;
    m_choiceTimerMax = timer;
    m_choiceDefault = defaultIndex;
    m_selectedChoice = -1;
    // `CP5`: cada elección nace con el PUNTERO mandando y sin foco de teclado.
    // Las dos mitades importan y por razones distintas: el puntero mandando es
    // lo que hace que un cursor ya parado sobre una opción la resalte al
    // aparecer -la conducta de hoy-, y el foco en -1 (fork 4a) es lo que hace
    // que quien no toca las flechas no vea una opción pre-elegida que el
    // creador nunca pidió. Sin este reset, el foco de la elección anterior se
    // filtraría a la siguiente.
    m_focoDelPuntero = true;
    m_choiceHover = -1;
}

// ── CP0 `mutar`: cerrar la elección SIN saltar ──────────────────────────────
//
// Espejo exacto de `startChoices`: deshace lo que aquella arma, campo por
// campo, y devuelve la fase a `Dialogue`. Los dos que importan y no se ven:
//
//   - `m_choices.clear()` apaga el countdown de `tiempo:`, que está gateado por
//     la LISTA y no por la fase (`choiceTimerRuns`, abajo). Sin esto, un
//     `elegir [tiempo: 5]` resuelto por mutación seguiría contando y emitiría
//     un `ChoiceMade` fantasma sobre opciones que ya no existen.
//   - `m_pendingChoice = false` evita que un click en el mismo frame vuelva a
//     entrar por la puerta que acabamos de cerrar.
//
// NO toca `m_persist` ni el texto: si el creador pidió `mostrar_caja`, la línea
// viva sigue viva — que es justamente el caso donde esta salida se ve, y el
// `startDialogue` de la próxima línea es el que hace el `newLine()`.
void eclips::VNDialogBox::endChoices()
{
    setPhase(Phase::Dialogue);
    m_choices.clear();
    m_choiceScale.clear();
    m_choiceTimer = 0.0f;
    m_choiceTimerMax = 0.0f;
    m_choiceDefault = -1;
    m_selectedChoice = -1;
    m_pendingChoice = false;
    m_choiceHover = -1;
    m_focoDelPuntero = true;
}

void eclips::VNDialogBox::startQTE(const std::string& keyName, uint32_t keyCode,
    float timer, const std::string& successScene,
    const std::string& failScene)
{
    setPhase(Phase::QTE);
    m_choices.clear();
    m_pendingChoice = false;
    m_autoMode = false;
    m_skipToggle = false;
    m_autoTimer = 0.0f;
    m_skipTimer = 0.0f;
    m_displayText.clear();
    m_charEffects.clear();
    m_qteKeyName = keyName;
    m_qteKeycode = keyCode;
    m_qteTimer = timer;
    m_qteTimerMax = timer;
    m_qteSuccessScene = successScene;
    m_qteFailScene = failScene;
    m_qte.armar();   // sin veredicto y sin evento pendiente: no hereda el anterior
}

// ── `CP7`: LOS DOS HOVERS DEL PUNTERO, EN UN SOLO SITIO ─────────────────────
//
// Vivían sueltos al principio de `update()`. Salen a método por una sola razón,
// y es de TEMPO: `setMousePos` los tiene que poder correr **en el acto**, porque
// `processEvents()` corre antes de `update()` (`Engine.cpp:481` vs `:497`) y
// hasta hoy el click leía el índice del batch anterior. Con un mouse eso no se
// veía —uno mueve, pasan frames, después clickea—; con un dedo, que llega
// MOTION+DOWN junto, el tap acertaba al objetivo del tap ANTERIOR.
//
// **Función pura de la posición, los rects y `m_focoDelPuntero`**, y por eso se
// puede llamar dos veces en el mismo frame. El cuerpo no cambió una línea: es
// el de `CP4` para el chrome y el de `CP5` para el choice, mudados enteros.
void eclips::VNDialogBox::derivarHoverDePuntero()
{
    // Hover de botones UI
    m_uiBtnHover = -1;
    if (m_mouseVX > 0 && m_mouseVY > 0) {
        std::vector<UIBtnRect> rects;
        getDialogBtnRects(rects, m_virtualW, m_virtualH);
        for (size_t i = 0; i < rects.size(); ++i) {
            if (m_mouseVX >= rects[i].x && m_mouseVX <= rects[i].x + rects[i].w &&
                m_mouseVY >= rects[i].y && m_mouseVY <= rects[i].y + rects[i].h) {
                // El botón Skip (1) no reacciona si no se permite skipear.
                if (rects[i].index == 1 && !m_skipAllowed) break;
                m_uiBtnHover = rects[i].index;
                break;
            }
        }
    }

    // Hover de choices (CP4 átomo) — espeja m_uiBtnHover con getChoiceRects.
    // Guard !m_choices.empty(): fuera de WaitingChoice queda -1 por construcción.
    //
    // ── `CP5`: ESTE PULL DEJA DE SER INCONDICIONAL ──────────────────────────
    //
    // Hasta ese lote el índice se re-derivaba del mouse **cada frame**, así
    // que un foco escrito por teclado duraba cero frames — es el defecto que
    // hacía imposible el anillo, y lo mide `harness_foco` §6.1. La ley 2
    // de `FocusRing.h` decide quién es dueño del índice; el hit-test de abajo
    // no cambió una línea, sólo dejó de escribir directo.
    //
    // El índice es UNO SOLO (§9: hover y foco convergen), y de eso cuelgan
    // gratis el pintado, el juice de escala, el sonido de hover por flanco
    // y el despertador del skip-frame: el teclado los hereda sin cable.
    //
    // *`CP7`: la guarda `m_mouseVX > 0 && m_mouseVY > 0` de acá arriba y de acá
    // abajo llevaba desde siempre el significado «no hay puntero», y era
    // SIEMPRE falsa porque el cursor siempre está en algún lado. Con la ley 3
    // de `PointerPresence.h` por fin puede ser cierta: un dedo levantado deja
    // (-1,-1), y las dos se apagan solas. Cero ramas nuevas.*
    {
        int bajoPuntero = -1;
        if (!m_choices.empty() && m_mouseVX > 0 && m_mouseVY > 0) {
            std::vector<UIBtnRect> rects;
            getChoiceRects(rects, m_virtualW, m_virtualH);
            for (const auto& r : rects) {
                if (m_mouseVX >= r.x && m_mouseVX <= r.x + r.w &&
                    m_mouseVY >= r.y && m_mouseVY <= r.y + r.h) {
                    bajoPuntero = r.index;
                    break;
                }
            }
        }
        if (m_choices.empty()) {
            // Fuera de una elección no hay anillo. Se apaga el índice **y** se
            // le devuelve el mando al puntero, o la próxima elección nacería
            // con el teclado dueño de un foco que nadie pidió.
            m_choiceHover    = -1;
            m_focoDelPuntero = true;
        } else {
            m_choiceHover = eclips::focoConverger(m_choiceHover,
                                                  m_focoDelPuntero, bajoPuntero);
        }
    }
}

eclips::VNDialogBox::UpdateResult eclips::VNDialogBox::update(float dt, bool isPaused)
{
    UpdateResult result;

    // `caja-entrada`: el reloj de la entrada. La ROTACIÓN del flag NO va acá —
    // va en `frameBegin()`, que llama el render. Ver el comentario grande en
    // `VNDialogBox.h`: acá adentro estuvo primero y la sonda lo desmintió.
    if (!isPaused) m_appear.tick(dt);

    if (isPaused) return result;

    if (m_showingHistory) {
        if (m_toastTimer > 0.0f) m_toastTimer -= dt;
        float diff = m_historyScrollTarget - m_historyScroll;
        m_historyScroll += diff * std::min(dt * 12.0f, 1.0f);
        return result;
    }
    m_totalTime += dt;

    if (m_pendingAdvance) {
        m_pendingAdvance = false;
        result.event = Event::Advance;
        return result;
    }
    if (m_pendingChoice) {
        m_pendingChoice = false;
        result.event = Event::ChoiceMade;
        result.choiceIndex = m_selectedChoice;
        return result;
    }
    // ── `QTE-SIN-PUERTA`: LA PUERTA ÚNICA DEL EVENTO (eslabón 3) ────────────
    //
    // Las DOS salidas del QTE -el acierto y el reloj vencido- pasan por acá, al
    // lado de sus hermanas. Antes el evento vivía ADENTRO del bloque del reloj
    // (abajo), así que sólo podía salir por vencimiento: el acierto ponía
    // `m_qteTimer = 0` y esa rama no volvía a correr nunca. **Acertar no emitía
    // nada y la fase quedaba en `WaitingQTE` para siempre.**
    //
    // Y el veredicto sale del mismo objeto que lo guardó: `result.qteSuccess`
    // estaba hardcodeado en `false` (eslabón 4), o sea que aunque el evento
    // hubiera salido, el state habría saltado a `fallo` igual.
    if (m_qte.hayEvento()) {
        m_qte.consumirEvento();
        result.event = Event::QTEFinished;
        result.qteSuccess = m_qte.exito();
        return result;
    }
    if (m_pendingUIEvent != Event::None) {
        result.event = m_pendingUIEvent;
        m_pendingUIEvent = Event::None;
        return result;
    }

    // `CP7`: los dos hovers del puntero se derivan en `derivarHoverDePuntero`,
    // que `setMousePos` ya corrió con la posición de ESTE evento. Se vuelve a
    // llamar acá porque es idempotente y porque el resto del `update` la
    // espera derivada — si la posición no cambió, no cambia nada.
    derivarHoverDePuntero();

    // Juice de hover (F1a): cada opción persigue su escala-objetivo con
    // aproximación exponencial frame-independiente: s += (obj - s)·(1 - e^(-k·dt)).
    // Reposo=1.0, hover=choiceHoverScale (clave de tema, F3). Default 1.0 =
    // apagado: target_hover == target_reposo → cero movimiento, MISMA ruta, sin
    // branch. El engine no impone nada; el tema opta por el juice (p.ej. DDLC).
    {
        const DialogBoxStyle& d = activeStyle();
        const float kHoverScale = d.choiceHoverScale; // F3: 1.0 = apagado
        const float kApproachK = 14.0f; // 1/seg — velocidad del acercamiento
        if (m_choiceScale.size() != m_choices.size())
            m_choiceScale.assign(m_choices.size(), 1.0f); // self-heal defensivo
        const float approach = 1.0f - std::exp(-kApproachK * dt);
        for (int i = 0; i < (int)m_choiceScale.size(); ++i) {
            float target = (i == m_choiceHover) ? kHoverScale : 1.0f;
            m_choiceScale[i] += (target - m_choiceScale[i]) * approach;
        }
    }

    // Typewriter + Auto / Skip
    //
    // `ELEGIR-CON-CAJA` (ley 5): el gate era `!m_displayText.empty()` a secas,
    // y alcanzaba SOLO porque `startChoices`/`startQTE` borraban el texto. Ese
    // borrado se fue, así que la fase entra al predicado.
    //
    // ── `D-37` (2026-08-04): LA PUERTA ES UNA, LAS LEYES SON DOS ────────────
    //
    // Este bloque hace dos cosas distintas y hasta hoy compartían gate: la rama
    // del medio **escribe letras**, y las otras tres (skip, `{interrumpir}`,
    // auto) **emiten `Advance`**. Con la forma pegada de `D-33` el caption se
    // escribe durante la elección, así que el typewriter SÍ corre ahí y el
    // avance NO — y con un solo predicado eso no se puede decir.
    // Guardián: `harness_persistencia` §5 (invertida) y su §7.5.
    const bool enDialogo = (m_phase == Phase::Dialogue);
    const bool hayTexto  = !m_displayText.empty();
    const bool avanceCorre = advanceClockRuns(enDialogo, hayTexto);

    if (typewriterRuns(enDialogo, hayTexto, m_persist.survivesChoice())) {
        // §7.410 — CARACTERES, no bytes. Con `size()` el typewriter cobraba un
        // tick por cada byte extra de UTF-8: en español el texto quedaba
        // completo y el indicador seguía apagado unos ticks más.
        int total = displayChars();
        if (skipActive()) {
            // El skip COMPLETA siempre, incluso en una elección fusionada:
            // pedido explícito del fundador (2026-08-04) — si venías salteando,
            // quedarte mirando el typewriter escribirse es lo contrario de lo
            // que pediste. Lo que no corre ahí es la emisión de `Advance`.
            m_typewriterChars = total;
            if (avanceCorre) {
                m_skipTimer += dt;
                if (m_skipTimer >= effective_skipInterval(*m_theme)) {
                    m_skipTimer = 0.0f;
                    result.event = Event::Advance;
                    return result;
                }
            }
        }
        else if (m_typewriterChars < total) {
            // Pausa por click ({esperar_click}): congelado hasta que llegue el click.
            // En modo auto, se trata como pausa de autoDelay (auto "clickea" solo).
            if (m_waitingMidClick) {
                if (m_autoMode) {
                    m_pauseTimer += dt;
                    if (m_pauseTimer >= effective_autoDelay(*m_theme)) {
                        m_waitingMidClick = false;
                        m_pauseTimer = 0.0f;
                    }
                }
                // En modo skip, atravesar sin parar:
                else if (skipActive()) {
                    m_waitingMidClick = false;
                }
                // Modo manual: esperar al click (handleAction lo destrabará).
            }
            // Pausa temporizada ({pausa=N}): decrementar timer.
            else if (m_pauseTimer > 0.0f) {
                m_pauseTimer -= dt;
                if (m_pauseTimer < 0.0f) m_pauseTimer = 0.0f;
            }
            // Avance normal del typewriter.
            else {
                m_typewriterTimer += dt;
                while (m_typewriterChars < total) {
                    float speedMult = 1.0f;
                    if (m_typewriterChars < (int)m_charEffects.size())
                        speedMult = m_charEffects[m_typewriterChars].speedMult;
                    const TextSpeed effectiveSpeed = effective_textSpeed_full(
                        *m_theme, m_activeSpeaker, m_lineSpeedOverride);
                    if (effectiveSpeed.isInstantaneo())
                    {
                        ++m_typewriterChars;
                    }
                    else
                    {
                        double interval = 1.0 /
                            (static_cast<double>(effectiveSpeed.cps()) * speedMult);
                        if (m_typewriterTimer < interval) break;
                        ++m_typewriterChars;
                        m_typewriterTimer -= static_cast<float>(interval);
                    }
                    // ¿El char recién mostrado tiene pausa o espera-click?
                    if (m_typewriterChars - 1 < (int)m_charEffects.size()) {
                        const CharEffect& ef = m_charEffects[m_typewriterChars - 1];
                        if (ef.pauseAfter > 0.0f && !skipActive()) {
                            m_pauseTimer = ef.pauseAfter;
                            break; // detener el while; el resto del frame ya no avanza
                        }
                        if (ef.waitClickAfter && !skipActive()) {
                            m_waitingMidClick = true;
                            break;
                        }
                    }
                }
            }
            if (m_typewriterChars >= total && m_autoMode)
                m_autoTimer = 0.0f;
        }

        // Línea con {interrumpir}: al completar el typewriter, salta solo tras
        // un respiro fijo (interruptDelay), PISANDO el modo auto. El respiro
        // garantiza que el texto se vea aunque la velocidad sea instantánea
        // (el timer cuenta desde aquí, no desde el inicio del typewriter).
        else if (avanceCorre && m_lineInterrupts) {
            m_interruptTimer += dt;
            if (m_interruptTimer >= m_theme->interruptDelay) {
                m_interruptTimer = 0.0f;
                result.event = Event::Advance;
                return result;
            }
        }
        else if (avanceCorre && m_autoMode) {
            m_autoTimer += dt;
            if (m_autoTimer >= effective_autoDelay(*m_theme)) {
                m_autoTimer = 0.0f;
                result.event = Event::Advance;
                return result;
            }
        }
    }

    // Choice timer — `choiceTimerRuns` (`BoxPersistence.h`) tiene la ley y el
    // porqué: con la forma pegada el countdown espera a que el caption termine
    // de escribirse, o `tiempo:` pasaría a medir el tiempo de LEER.
    const bool captionEscribiendose =
        m_persist.survivesChoice() && !m_displayText.empty() &&
        m_typewriterChars < displayChars();

    if (choiceTimerRuns(!m_choices.empty(), m_choiceTimer > 0.0f,
                        captionEscribiendose)) {
        m_choiceTimer -= dt;
        if (m_choiceTimer <= 0.0f) {
            m_choiceTimer = 0.0f;
            int idx = m_choiceDefault;
            if (idx < 0 || idx >= (int)m_choices.size())
                idx = (int)m_choices.size() - 1;
            result.event = Event::ChoiceMade;
            result.choiceIndex = idx;
            return result;
        }
    }

    // QTE timer. El evento NO sale de acá: sale por la puerta única de arriba,
    // que es lo que le da al acierto un camino (eslabón 3). Este bloque sólo
    // mueve el reloj y avisa que venció.
    //
    // Costo declarado: el vencimiento se anuncia UN FRAME después, porque la
    // puerta está arriba y este bloque corre abajo. Son 16 ms al final de un QTE
    // de 3000, comprados a cambio de que las dos salidas no puedan
    // desincronizarse — que es exactamente cómo nació el eslabón 3.
    if (m_qteTimer > 0.0f) {
        m_qteTimer -= dt;
        if (m_qteTimer <= 0.0f) {
            m_qteTimer = 0.0f;
            m_qte.vencer();
        }
    }

    if (m_toastTimer > 0.0f)
        m_toastTimer -= dt;

    if (m_showingHistory) {
        float diff = m_historyScrollTarget - m_historyScroll;
        m_historyScroll += diff * std::min(dt * 12.0f, 1.0f);
    }

    return result;
}

// ── Historial ─────────────────────────────────────────────────────────
void eclips::VNDialogBox::pushHistory(const std::string& speaker, const std::string& text)
{
    if (text.empty()) return;
    while ((int)m_history.size() >= m_theme->historyMaxEntries)
        m_history.pop_front();
    m_history.push_back({ speaker, text });
}

void eclips::VNDialogBox::clearHistory()
{
    m_history.clear();
    m_showingHistory = false;
    m_historyScroll = 0.0f;
    m_historyScrollTarget = 0.0f;
}

const eclips::DialogBoxStyle& eclips::VNDialogBox::activeStyle() const
{
    static std::set<std::string> s_warned;

    // Nivel 1: usar_caja mid-escena
    if (!m_overrideBoxName.empty()) {
        auto it = m_theme->namedDialogBoxes.find(m_overrideBoxName);
        if (it != m_theme->namedDialogBoxes.end())
            return it->second;
        if (s_warned.insert("override:" + m_overrideBoxName).second)
            std::cout << "[VNDialogBox] Aviso: caja '" << m_overrideBoxName
            << "' no encontrada en theme, usando cadena de prioridad" << std::endl;
    }
    // Nivel 2: caja del personaje activo
    if (m_activeSpeaker && !m_activeSpeaker->caja.empty()) {
        auto it = m_theme->namedDialogBoxes.find(m_activeSpeaker->caja);
        if (it != m_theme->namedDialogBoxes.end())
            return it->second;
        if (s_warned.insert("char:" + m_activeSpeaker->caja).second)
            std::cout << "[VNDialogBox] Aviso: caja de personaje '" << m_activeSpeaker->caja
            << "' no encontrada en theme, usando default" << std::endl;
    }
    // Nivel 3: default del theme
    return m_theme->defaultDialogBox;
}

eclips::TextRenderer* eclips::VNDialogBox::resolveBoxFont(eclips::TextRenderer* fallback) const
{
    if (!m_fm) return fallback;
    const DialogBoxStyle& d = activeStyle();
    if (d.fontName.empty()) return fallback;
    if (TextRenderer* tr = m_fm->get(d.fontName)) return tr;
    // Fuente nombrada declarada en el tema pero no cargada por el .ecl.
    // Aviso 1× por nombre (§1.31, paralelo a activeStyle()).
    static std::set<std::string> s_warned;
    if (s_warned.insert(d.fontName).second)
        std::cout << "[VNDialogBox] Aviso: fuente '" << d.fontName
        << "' no encontrada, usando fuente base" << std::endl;
    return fallback;
}

// Fuente SOLO de la placa de nombre (C-NOMBRE). Capa fina sobre resolveBoxFont:
// nombre_fuente > caja_fuente > fallback (m_name crudo). El tamaño viene
// horneado en la fuente nombrada (§7.43), igual que caja_fuente.
eclips::TextRenderer* eclips::VNDialogBox::resolveNameFont(eclips::TextRenderer* fallback) const
{
    if (!m_fm) return resolveBoxFont(fallback);
    const DialogBoxStyle& d = activeStyle();
    if (d.nameFontName.empty()) return resolveBoxFont(fallback);
    if (TextRenderer* tr = m_fm->get(d.nameFontName)) return tr;
    // Declarada en el tema pero no cargada por el .ecl. Aviso 1× propio (set
    // separado del de caja: una fuente puede faltar como nombre y existir como
    // caja, o viceversa). Tras el aviso, cae a caja_fuente/base por delegación.
    static std::set<std::string> s_warnedName;
    if (s_warnedName.insert(d.nameFontName).second)
        std::cout << "[VNDialogBox] Aviso: fuente de nombre '" << d.nameFontName
        << "' no encontrada, usando caja_fuente/base" << std::endl;
    return resolveBoxFont(fallback);
}

// Fuente de los botones de elegir (C-FUENTE-CHROME). NO delega en
// resolveBoxFont: los choices nunca siguieron caja_fuente (renderChoices
// usaba m_text crudo) → cuando opciones_fuente está vacío, caer directo al
// fallback (base) preserva el comportamiento previo. Set de aviso propio.
eclips::TextRenderer* eclips::VNDialogBox::resolveChoiceFont(eclips::TextRenderer* fallback) const
{
    if (!m_fm) return fallback;
    const DialogBoxStyle& d = activeStyle();
    if (d.choiceFontName.empty()) return fallback;
    if (TextRenderer* tr = m_fm->get(d.choiceFontName)) return tr;
    static std::set<std::string> s_warnedChoice;
    if (s_warnedChoice.insert(d.choiceFontName).second)
        std::cout << "[VNDialogBox] Aviso: fuente de opciones '" << d.choiceFontName
        << "' no encontrada, usando fuente base" << std::endl;
    return fallback;
}

// Fuente de QTE (tecla + hint) (C-FUENTE-CHROME). Misma forma que
// resolveChoiceFont: qte_fuente > fallback, sin pasar por caja_fuente.
eclips::TextRenderer* eclips::VNDialogBox::resolveQteFont(eclips::TextRenderer* fallback) const
{
    if (!m_fm) return fallback;
    const DialogBoxStyle& d = activeStyle();
    if (d.qteFontName.empty()) return fallback;
    if (TextRenderer* tr = m_fm->get(d.qteFontName)) return tr;
    static std::set<std::string> s_warnedQte;
    if (s_warnedQte.insert(d.qteFontName).second)
        std::cout << "[VNDialogBox] Aviso: fuente de QTE '" << d.qteFontName
        << "' no encontrada, usando fuente base" << std::endl;
    return fallback;
}

// Fuente del bottom-bar (boton_fuente). Misma forma que resolveChoiceFont:
// boton_fuente > fallback, sin pasar por caja_fuente (el chrome siempre usó
// m_name crudo → caer a base preserva compat).
eclips::TextRenderer* eclips::VNDialogBox::resolveChromeFont(eclips::TextRenderer* fallback) const
{
    if (!m_fm) return fallback;
    const DialogBoxStyle& d = activeStyle();
    if (d.chromeFontName.empty()) return fallback;
    if (TextRenderer* tr = m_fm->get(d.chromeFontName)) return tr;
    static std::set<std::string> s_warnedChrome;
    if (s_warnedChrome.insert(d.chromeFontName).second)
        std::cout << "[VNDialogBox] Aviso: fuente de botones '" << d.chromeFontName
        << "' no encontrada, usando fuente base" << std::endl;
    return fallback;
}

// FASE 2 — filtro de tamaño SOBRE la fuente ya resuelta. Compone con caja_fuente
// (Fork 3a): el tamaño se aplica al ttf que trajo resolveBoxFont/resolveNameFont,
// no al baseline. Por eso lee fontPath()/kerning() del *resuelto*, no del base.
// Const-safe: m_fm es puntero, así que getSized (que muta el cache) es legal acá.
eclips::TextRenderer* eclips::VNDialogBox::sizedFont(eclips::TextRenderer* resolved, f32 size) const
{
    if (!resolved || size <= 0.0f) return resolved;        // sentinela ausente
    if (resolved->fontSize() == size) return resolved;     // ya es ese tamaño → no-op
    if (!m_fm) return resolved;                            // sin FontManager no hay dónde crearlo
    if (TextRenderer* tr = m_fm->getSized(*resolved, size,
                                          resolved->kerning(),
                                          resolved->fontMeasure()))
        return tr;
    return resolved;                                       // getSized falló → resuelto tal cual
}

// Dibuja solo la PLACA del nombre. Contrato: sale con el batch ABIERTO.
void eclips::VNDialogBox::drawNamePlate(const DialogBoxStyle& d,
    float nX, float nY, float nW, float nH,
    bool batchOpen)
{
    Texture* nameTex = (d.hasNamePlateSprite() && m_assets)
        ? m_assets->get(d.namePlateSpritePath)
        : nullptr;

    if (nameTex)
    {
        if (!batchOpen) draw2d::g_batch->begin();
        if (d.hasNamePlate9Slice())
        {
            draw2d::drawTexture9Slice_noBatch(*nameTex, nX, nY, nW, nH,
                d.namePlateSliceLeft, d.namePlateSliceRight,
                d.namePlateSliceTop, d.namePlateSliceBottom,
                1.0f, 1.0f, 1.0f, d.namePlateA);
        }
        else
        {
            draw2d::drawTextureTinted_noBatch(*nameTex, nX, nY, nW, nH,
                1.0f, 1.0f, 1.0f, d.namePlateA);
        }
    }
    else
    {
        if (batchOpen) draw2d::g_batch->end();
        draw2d::drawRoundedRect(nX, nY, nW, nH, d.namePlateRadius,
            d.namePlateR, d.namePlateG, d.namePlateB, d.namePlateA);
        draw2d::drawRoundedRectBorder(nX, nY, nW, nH, d.namePlateRadius,
            d.nameBorderR, d.nameBorderG, d.nameBorderB, d.nameBorderA, 1.5f);
        draw2d::g_batch->begin();
    }
}

// ── Renderizado ────────────────────────────────────────────────────────
//
// `appearAlpha()` vivía acá y se mudó a `engine/render/AppearPolicy.h` en el
// lote 2 (`RO-5`). El cuerpo no cambió —es el mismo `AnimTrack` Alpha/una_vez/
// suave, y `harness_aparicion` §2.1 lo compara punto por punto contra
// `evalTrack`— pero la DURACIÓN ya no se relee del estilo en cada frame: sale
// de la política que la aparición capturó al armarse. Ésa era la mitad medible
// del defecto que el gate `FO-2` fotografió.

void eclips::VNDialogBox::renderDialogBox(float VW, float VH)
{
    if (!m_hasFont) return;
    if (m_hidden) return; // ocultar_caja: skipea toda la UI del diálogo

    // ── `caja-entrada` ──────────────────────────────────────────────────────
    // Desde acá la caja se dibujó. Va DESPUÉS de los dos early-outs a
    // propósito: "oculta por `ocultar_caja`" y "sin fuente" tienen que contar
    // como NO vista, o `mostrar_caja` no fundiría nunca.
    m_appear.rendered();

    // LA SOLDADURA: todo lo que se dibuje desde acá hasta el final de la
    // función comparte el fundido — la caja, la placa, el nombre, el cuerpo con
    // sus nueve pasadas de borde, el indicador y los seis botones. No hay lista
    // que mantener: entran por estar adentro del ámbito (`GroupAlpha.h`).
    // Con la clave sin declarar el factor es 1.0 y esto es bit-idéntico a no
    // estar. Los choices NO están soldados a esto: viven en `renderChoices`,
    // que es otra función y otra fase.
    const eclips::draw2d::GroupAlphaScope entrada(appearAlpha());

    // Atajo a la caja default — paralelo al patrón del parser en Theme.h.
    // Evita repetir m_theme->defaultDialogBox.X en ~50 sitios. Es referencia
    // const, no copia, performance idéntica al acceso directo.
    const DialogBoxStyle& d = activeStyle();

    float bH = d.height;
    float bW = VW * d.widthRatio;
    float bX = (VW - bW) / 2.0f;
    float bY = VH - bH - d.marginBottom;

    // ── Pasada 3 PNG: caja como sprite si está declarado y cargado ──────
    // Resolución por path: get() es O(1) hashmap lookup post-pin (los
    // sprites del theme fueron pineados en VNScriptState::onEnter).
    // Si el path declarado falló al cargar, get() retorna nullptr y
    // caemos a primitivas — el warning pedagógico ya se imprimió en
    // pin time, no spammeamos cada frame.
    Texture* boxTex = (d.hasBoxSprite() && m_assets)
        ? m_assets->get(d.boxSpritePath)
        : nullptr;

    // ── Z-order: geometría del nameplate calculada una sola vez ─────────
    // (la usa tanto la placa-detrás como la placa-delante). Si va detrás,
    // se dibuja AQUÍ, antes de la caja, con el batch cerrado.
    float nW = 0, nH = 0, nX = 0, nY = 0;
    bool showName = (!m_currentSpeaker.empty() && m_hasNameFont);
    // FASE 2: fuente de la placa con tamaño aplicado (nombre_tamano sobre la
    // fuente ya resuelta). Resuelta 1× acá → la usan los TRES sitios de la
    // placa (ancho, centrado, draw) sin desincronizarse (§7.72). nullptr
    // mientras !showName (la placa no se toca).
    TextRenderer* nameFont = nullptr;
    if (showName)
    {
        nameFont = sizedFont(resolveNameFont(m_name), d.nameSize);
        nW = (d.namePlateWidth > 0.0f)
            ? d.namePlateWidth
            : (nameFont->measureWidth(m_currentSpeaker) + d.namePlatePaddingX * 2.0f);
        nH = d.namePlateHeight;
        nX = bX + d.namePlatePaddingX + d.namePlateOffsetX;
        nY = bY - nH + d.namePlateOffsetY;
        if (d.namePlateBehindBox)
        {
            // Placa detrás: el helper encola el quad (sprite) o lo dibuja en
            // immediate (primitivas). En ambos casos sale con el batch ABIERTO.
            // Debemos cerrarlo con end() AQUÍ para flushear el quad a la GPU
            // ANTES de que la caja haga begin() — begin() resetea m_quadCount
            // sin flush, y el sprite de la placa se perdería (gotcha batch).
            drawNamePlate(d, nX, nY, nW, nH, /*batchOpen=*/false);
            draw2d::g_batch->end();
        }
    }

    if (boxTex)
    {
        // Path PNG (decisiones pasada 3 + 9-slice):
        //   - Stretch al rect del layout si no hay 9-slice declarado;
        //     9-slice preserva esquinas/bordes si los campos > 0.
        //   - Tint blanco (boxR/G/B ignorados).
        //   - alpha_final = pngAlpha[pixel] × d.boxA (multiplicador).
        //   - Outline y radius skipeados — el PNG manda.
        draw2d::g_batch->begin();
        if (d.hasBox9Slice())
        {
            // 9-slice: preserva esquinas/bordes. Source overflow ya validado
            // en pinThemeSprites (cleared a 0 si overflow). Destination
            // overflow se valida silente dentro de drawTexture9Slice_noBatch.
            draw2d::drawTexture9Slice_noBatch(*boxTex, bX, bY, bW, bH,
                d.boxSliceLeft, d.boxSliceRight,
                d.boxSliceTop, d.boxSliceBottom,
                1.0f, 1.0f, 1.0f, d.boxA);
        }
        else
        {
            // Stretch puro (path original pasada 3): 1 quad tinted.
            draw2d::drawTextureTinted_noBatch(*boxTex, bX, bY, bW, bH,
                1.0f, 1.0f, 1.0f, d.boxA);
        }
    }
    else
    {
        // Path primitivas: comportamiento original pre-PNG. Rounded rects
        // en immediate mode, luego abrir batch para texto/UI.
        draw2d::drawRoundedRect(bX, bY, bW, bH, d.radius,
            d.boxR, d.boxG, d.boxB, d.boxA);
        draw2d::drawRoundedRectBorder(bX, bY, bW, bH, d.radius,
            d.boxBorderR, d.boxBorderG, d.boxBorderB, d.boxBorderA, 2.5f);

        // F2.C: begin batch — todo lo que sigue (texto, rects, botones) va
        // por SpriteBatch. Los drawRoundedRect de arriba son immediate mode.
        draw2d::g_batch->begin();
    }

    if (showName)
    {
        // Placa DELANTE de la caja (default). Si va detrás, ya se dibujó
        // antes de la caja (3b); aquí solo el texto. El batch está ABIERTO.
        if (!d.namePlateBehindBox)
            drawNamePlate(d, nX, nY, nW, nH, /*batchOpen=*/true);

        // El texto del nombre SIEMPRE va al final, sobre su placa, dentro
        // del batch abierto — visible esté la placa delante o detrás.
        float tw = nameFont->measureWidth(m_currentSpeaker);
        nameFont->drawTextOutlined_noBatch(m_currentSpeaker,
            nX + (nW - tw) / 2.0f,
            nY + d.namePlateTextOffsetY,
            d.nameTextR, d.nameTextG,
            d.nameTextB, d.nameTextA,
            d.nameOutlineR, d.nameOutlineG,
            d.nameOutlineB, d.nameOutlineA,
            d.nameOutlineThickness);
    }

    // FASE 2: fuente del cuerpo con tamaño aplicado (texto_tamano sobre la
    // fuente ya resuelta — compone con caja_fuente, Fork 3a). Resuelta 1× →
    // la comparte el indicador glifo-fallback de abajo (Fork 4a). El wrap
    // interno mide cada glifo con SU fuente (fuente-inline), así el line-height
    // sale correcto incluso con tamaños mezclados.
    TextRenderer* bodyFont = sizedFont(resolveBoxFont(m_text), d.textSize);
    bodyFont->drawTextWrappedRichOutlined_noBatch(m_displayText,
        bX + d.textPaddingLeft,
        bY + d.textPaddingTop,
        bW - d.textPaddingLeft - d.textPaddingRight,
        m_typewriterChars, (f32)m_totalTime,
        m_charEffects,
        d.textR, d.textG,
        d.textB, d.textA,
        d.textOutlineR, d.textOutlineG,
        d.textOutlineB, d.textOutlineA,
        d.textOutlineThickness,
        m_fm,             // fuente inline: resuelve {fuente=X} por glifo
        d.lineSpacing);   // `interlineado`: 0 = heredar del ttf (el default de siempre)

    // Indicador visible cuando: (a) el typewriter terminó la línea, o
    // (b) está pausado por {esperar_click} esperando el click del jugador.
    if ((m_typewriterChars >= displayChars() || m_waitingMidClick)
        && m_hasFont && !m_lineInterrupts)
    {
        if (m_autoMode)
        {
            float barW = d.autoBarWidth;
            float barH = d.autoBarHeight;
            float barX = bX + d.autoBarMarginX;
            float barY = bY + bH - d.autoBarMarginBottom;
            // Durante {esperar_click} en auto, el progreso lo da m_pauseTimer
            // (no m_autoTimer, que solo cuenta cuando el typewriter completó).
            float progress = m_waitingMidClick
                ? (float)(m_pauseTimer / effective_autoDelay(*m_theme))
                : (float)(m_autoTimer / effective_autoDelay(*m_theme));
            if (progress > 1.0f) progress = 1.0f;

            draw2d::drawRect_noBatch(barX, barY, barW, barH,
                d.autoBarBgR, d.autoBarBgG, d.autoBarBgB, d.autoBarBgA);
            draw2d::drawRect_noBatch(barX, barY, barW * progress, barH,
                d.autoBarFillR, d.autoBarFillG, d.autoBarFillB, d.autoBarFillA);
        }
        else
        {
            float ix = bX + d.advanceIndicatorOffsetX;
            float iy = bY + bH - d.advanceIndicatorOffsetBottom;

            // Vaivén (vaiven/nudge): consume la matemática de TweenSystem (un
            // CharAnim libre, alimentado por m_totalTime) en vez de re-inlinear
            // la fórmula. Offset PURO; compone con el blink (alpha) de abajo.
            // dt=0 → updateAnim solo computa, no acumula. Default off = sin cambio.
            float nudgeDisp = 0.0f;  // desplazamiento con signo en el eje activo (px); 0 = reposo
            if (d.advanceIndicatorNudge) {
                eclips::CharAnim na;
                na.type = eclips::AnimType::Nudge;
                na.intensity = d.advanceIndicatorNudgeAmplitude;  // px
                na.period = d.advanceIndicatorNudgePeriod;     // seg/ciclo
                na.axisVertical = d.advanceIndicatorNudgeVertical;   // X|Y
                na.elapsed = (float)m_totalTime;
                eclips::updateAnim(na, 0.0f);
                ix += na.offsetX;
                iy += na.offsetY;
                nudgeDisp = d.advanceIndicatorNudgeVertical ? na.offsetY : na.offsetX;
            }

            // Parpadeo: modular alpha con onda senoidal si está activo.
            // Rango [0.5, 1.0] del alpha base — pulsa visible sin desaparecer.
            float alpha = d.advanceIndicatorA;
            if (d.advanceIndicatorNudge && d.advanceIndicatorNudgeBrillo) {
                // SOLDADO (ctc DDLC): el alpha NACE de la posición del vaivén,
                // no de un reloj aparte → un solo oscilador, beat imposible.
                // disp ∈ [-amp,0]: 0 = reposo (brillante 1.0), -amp = extremo
                // (apagado 0.5). Rango [0.5,1.0] de DDLC. Manda sobre el blink.
                float amp = d.advanceIndicatorNudgeAmplitude;
                float norm = (amp > 0.0f) ? (nudgeDisp + amp) / amp : 1.0f;
                norm = std::max(0.0f, std::min(1.0f, norm));
                alpha = d.advanceIndicatorA * (0.5f + norm * 0.5f);
            }
            else if (d.advanceIndicatorBlink) {
                // C-CP4: el seno a mano se retira. PRIMER caller real de evalTrack
                // — sin él, AnimTrack/evalTrack (CP0) serían capacidad sin caller,
                // o sea: indistinguibles de no existir (§7.118).
                // Un track de alpha en vaivén, alimentado por el MISMO reloj, con
                // dt=0: no acumula estado porque la animación es f(t).
                // Misma onda que antes (rango [0.5,1.0], mismo periodo); solo
                // arranca un cuarto de ciclo corrido — en un loop sin inicio
                // definido, imperceptible.
                eclips::AnimTrack blink;
                blink.canal = eclips::Channel::Alpha;
                blink.modo = eclips::Lifecycle::Vaiven;
                blink.curva = eclips::EasingId::Suave;
                blink.de = 1.0f;   // identidad multiplicativa del canal (§1.40)
                blink.a = 0.5f;   // el suelo de siempre: pulsa sin desaparecer
                blink.dur = (d.advanceIndicatorBlinkSpeed > 0.0f)
                    ? (1.0f / d.advanceIndicatorBlinkSpeed)   // Hz → periodo (seg)
                    : 0.5f;
                alpha = d.advanceIndicatorA *
                    eclips::evalTrack(blink, (float)m_totalTime);
            }

            // C-CONTINUAR eje-1: si hay imagen declarada y cargó OK, se dibuja
            // como textura (tamaño nativo, tint blanco, alpha modulada por el
            // blink). Si falta o falló al cargar, fallback al glifo. Ambos anclan
            // por su esquina sup-izq en (ix, iy) → offset_abajo mueve los dos
            // igual (drawText_noBatch también toma (x,y) como top-left, no baseline).
            Texture* indTex = (d.hasAdvanceIndicatorImage() && m_assets)
                ? m_assets->get(d.advanceIndicatorImagePath)
                : nullptr;
            if (indTex)
            {
                draw2d::drawTextureTinted_noBatch(*indTex,
                    ix, iy,
                    (float)indTex->getWidth()* d.advanceIndicatorScale,
                    (float)indTex->getHeight()* d.advanceIndicatorScale,
                    1.0f, 1.0f, 1.0f, alpha);
            }
            else
            {
                bodyFont->drawText_noBatch(d.advanceIndicatorGlyph,
                    ix, iy,
                    d.advanceIndicatorR, d.advanceIndicatorG,
                    d.advanceIndicatorB, alpha);
            }
        }
    }

    // ── Botones de UI (dinámicos según tema) ────────────────────────
    if (m_hasNameFont)
    {
        std::vector<UIBtnRect> rects;
        getDialogBtnRects(rects, VW, VH);

        // boton_fuente: el chrome resuelve su propia fuente (> base, sin
        // caja_fuente). MISMA que mide getDialogBtnRects → anchos y letra
        // no se desincronizan (§7.72). boton_tamano reescala esa fuente (0 =
        // heredar) — el MISMO sizedFont que aplica getDialogBtnRects al medir.
        TextRenderer* chromeFont = sizedFont(resolveChromeFont(m_name), d.btnSize);

        const bool stylePlain = (d.uiBtnStyle == UIBtnStyle::TextOnly);

        // boton_imagen: cara PNG del chrome (nivel-tema, igual para los 6 →
        // resuelta 1×, espejo de choices). Pineadas en pinThemeSprites → O(1).
        // nullptr → fallback rect+borde de abajo (compat).
        Texture* chromeTex = (d.hasChromeBtnSprite() && m_assets)
            ? m_assets->get(d.chromeBtnSpritePath) : nullptr;
        Texture* chromeHoverTex = (d.hasChromeBtnHoverSprite() && m_assets)
            ? m_assets->get(d.chromeBtnHoverSpritePath) : nullptr;

        for (const auto& r : rects)
        {
            int i = r.index;
            // ── `D-42`: UN BOTON VISIBLE Y DESHABILITADO SE PINTA APAGADO ────
            //
            // La decisión decía *«no inventa una casilla: extiende a otra
            // familia una que ya tiene ley, aspecto y vocabulario»*, y citaba
            // el OK de `entrada` (`okDisabled`) como precedente. **Medido, es
            // más barato todavía: la casilla ya existía ACÁ.** Este `disabled`
            // -con su plate al 0.4, su fondo al 0.12, su borde al 0.15 y su
            // label gris- lleva lotes pintando el botón de SKIP cuando el texto
            // no está visto. Lo único que faltaba era la otra causa.
            //
            // Si esto no estuviera, el motor estaría mintiendo: seis botones
            // pintados normales que no hacen nada.
            bool disabled = (i == 1 && !m_skipAllowed)
                || !accionPermitida(verboDeBoton(i));
            bool hover = !disabled && (m_uiBtnHover == i);
            bool active = !disabled && ((i == 0 && m_autoMode) || (i == 1 && m_skipToggle));

            // ── Fondo: PNG plate (boton_imagen) manda; si no, rect+borde como hoy ──
            // El plate usa la cara hover si existe (si no, la idle). El estado
            // active/disabled sigue en el COLOR del label (abajo, sin cambio); el
            // plate solo apaga su alpha en disabled. Sin sprite → rama de siempre,
            // idéntica a antes (compat por construcción).
            Texture* plateTex = hover ? (chromeHoverTex ? chromeHoverTex : chromeTex)
                : chromeTex;
            if (plateTex)
            {
                float plateA = disabled ? 0.4f : 1.0f;
                draw2d::drawButtonBg(r.x, r.y, r.w, r.h, plateTex, d.hasChromeBtn9Slice(),
                    d.chromeBtnSliceLeft, d.chromeBtnSliceRight,
                    d.chromeBtnSliceTop, d.chromeBtnSliceBottom,
                    0.0f, 0.0f, 0.0f, plateA,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            }
            else if (!stylePlain)
            {
                float bgA = hover ? 0.45f : 0.25f;
                if (active) bgA = 0.55f;
                if (disabled) bgA = 0.12f;
                draw2d::drawRect_noBatch(r.x, r.y, r.w, r.h,
                    d.btnBgR, d.btnBgG, d.btnBgB, bgA);
                float brA = hover ? 0.7f : 0.35f;
                if (active) brA = 0.85f;
                if (disabled) brA = 0.15f;
                draw2d::drawRectBorder_noBatch(r.x, r.y, r.w, r.h,
                    d.btnBorderR, d.btnBorderG, d.btnBorderB, brA, 1.0f);
            }

            const std::string& label = d.uiBtnLabels[i];
            float tw = chromeFont->measureWidth(label);
            float lh = chromeFont->getLineHeight();
            float tx = r.x + (r.w - tw) / 2.0f;
            float ty = r.y + (r.h - lh) / 2.0f;

            float cr = d.btnTextR, cg = d.btnTextG,
                cb = d.btnTextB, ca = d.btnTextA;
            if (hover) { cr = 0.85f; cg = 0.90f; cb = 1.0f; ca = 1.0f; }
            if (active) { cr = 0.55f; cg = 0.95f; cb = 0.65f; ca = 1.0f; }
            if (disabled) { cr = 0.45f; cg = 0.45f; cb = 0.45f; ca = 0.4f; }

            chromeFont->drawText_noBatch(label, tx, ty, cr, cg, cb, ca);
        }
    }
    // F2.C: end batch — cierra el grupo texto+rects+botones.
    draw2d::g_batch->end();
}

// drawButtonBg se promovio a draw2d::drawButtonBg (Draw2D.h) — primitivo
// asset-free compartido (CP2 · A2). Sus consumidores aqui lo llaman calificado.

void eclips::VNDialogBox::renderChoices(float VW, float VH)
{
    if (!m_hasFont || m_choices.empty()) return;

    const DialogBoxStyle& d = activeStyle();

    // F2.C: begin batch — todo en choices es SpriteBatch (rects + texto).
    draw2d::g_batch->begin();

    std::vector<UIBtnRect> rects;
    getChoiceRects(rects, VW, VH);
    if (rects.empty()) { draw2d::g_batch->end(); return; }

    const float btnW = rects[0].w;
    const float startX = rects[0].x;
    const float startY = rects[0].y;

    if (m_choiceTimerMax > 0)
    {
        float barW = btnW;
        float barH = d.choiceTimerBarHeight;
        float barX = startX;
        float barY = startY - barH - d.choiceTimerBarOffsetY;
        float progress = (m_choiceTimerMax > 0) ? (float)(m_choiceTimer / m_choiceTimerMax) : 0.0f;
        if (progress < 0) progress = 0;

        float cr = (progress < 0.5f) ? 1.0f : progress * 2.0f;
        float cg = (progress > 0.5f) ? 1.0f : progress * 2.0f;
        if (cr > 1.0f) cr = 1.0f;
        if (cg > 1.0f) cg = 1.0f;

        draw2d::drawRect_noBatch(barX, barY, barW, barH,
            d.choiceTimerBarBgR, d.choiceTimerBarBgG,
            d.choiceTimerBarBgB, d.choiceTimerBarBgA);
        draw2d::drawRect_noBatch(barX, barY, barW * progress, barH,
            cr, cg, d.choiceTimerBarFillB, d.choiceTimerBarFillA);
        draw2d::drawRectBorder_noBatch(barX, barY, barW, barH,
            d.choiceTimerBarBorderR, d.choiceTimerBarBorderG,
            d.choiceTimerBarBorderB, d.choiceTimerBarBorderA,
            d.choiceTimerBarBorderThickness);
    }

    TextRenderer* cf = resolveChoiceFont(m_text);

    // Cara PNG (nivel-tema, igual para todos → resuelta 1×). Pineadas en
    // pinThemeSprites → get() O(1). nullptr → fallback rect.
    Texture* choiceTex = (d.hasChoiceBtnSprite() && m_assets)
        ? m_assets->get(d.choiceBtnSpritePath) : nullptr;
    Texture* choiceHoverTex = (d.hasChoiceBtnHoverSprite() && m_assets)
        ? m_assets->get(d.choiceBtnHoverSpritePath) : nullptr;
    const bool choice9Slice = d.hasChoiceBtn9Slice();

    for (const auto& r : rects)
    {
        int i = r.index;
        bool hover = (i == m_choiceHover);
        bool isDefault = (m_choiceTimerMax > 0 && i == m_choiceDefault);
        (void)isDefault; // TODO(default-del-timer): resaltado visual → ficha aparte
        // Precedencia hover > default se resolverá cuando exista el default-resaltado.

        // Paleta: hover lee choiceBtnHover*; normal lee choiceBtn*.
        float bgR = hover ? d.choiceBtnHoverBgR : d.choiceBtnBgR;
        float bgG = hover ? d.choiceBtnHoverBgG : d.choiceBtnBgG;
        float bgB = hover ? d.choiceBtnHoverBgB : d.choiceBtnBgB;
        float bgA = hover ? d.choiceBtnHoverBgA : d.choiceBtnBgA;
        float brR = hover ? d.choiceBtnHoverBorderR : d.choiceBtnBorderR;
        float brG = hover ? d.choiceBtnHoverBorderG : d.choiceBtnBorderG;
        float brB = hover ? d.choiceBtnHoverBorderB : d.choiceBtnBorderB;
        float brA = hover ? d.choiceBtnHoverBorderA : d.choiceBtnBorderA;
        float txR = hover ? d.choiceBtnHoverTextR : d.choiceBtnTextR;
        float txG = hover ? d.choiceBtnHoverTextG : d.choiceBtnTextG;
        float txB = hover ? d.choiceBtnHoverTextB : d.choiceBtnTextB;
        float txA = hover ? d.choiceBtnHoverTextA : d.choiceBtnTextA;

        // Fondo: PNG si el tema lo declaró (hover-sprite si existe; si no, el
        // idle estático — el sonido de CP4 da el feedback de hover); si no, rect.
        Texture* btnTex = hover ? (choiceHoverTex ? choiceHoverTex : choiceTex)
            : choiceTex;

        // Juice de hover (CP2 · F2a): escala SOLO la placa, centrada en el rect
        // de reposo. El centro es invariante bajo la escala → el texto (centrado
        // con r más abajo) queda firme y la placa "respira" a su alrededor.
        // r sigue siendo geometría de reposo (§1.40): esto es offset de RENDER,
        // el hit-test no lo ve. Guard de índice por si acaso.
        float s = (i >= 0 && i < (int)m_choiceScale.size()) ? m_choiceScale[i] : 1.0f;
        float pcx = r.x + r.w * 0.5f;
        float pcy = r.y + r.h * 0.5f;
        float pw = r.w * s;
        float ph = r.h * s;
        float px = pcx - pw * 0.5f;
        float py = pcy - ph * 0.5f;
        draw2d::drawButtonBg(px, py, pw, ph, btnTex, choice9Slice,
            d.choiceBtnSliceLeft, d.choiceBtnSliceRight,
            d.choiceBtnSliceTop, d.choiceBtnSliceBottom,
            bgR, bgG, bgB, bgA,
            brR, brG, brB, brA, d.choiceBtnBorderThickness);

        std::string num = std::to_string(i + 1) + ". ";
        std::string fullLabel = num + m_choices[i].label;
        float tw = cf->measureWidth(fullLabel);
        float tx = r.x + (r.w - tw) / 2.0f;
        float ty = r.y + (r.h - cf->getLineHeight()) / 2.0f;
        cf->drawText_noBatch(fullLabel, tx, ty, txR, txG, txB, txA);
    }

    // F2.C: end batch
    draw2d::g_batch->end();
}

void eclips::VNDialogBox::renderQTE(float VW, float VH)
{
    if (!m_hasFont) return;

    const DialogBoxStyle& d = activeStyle();

    // F2.C: begin batch
    draw2d::g_batch->begin();

    float barW = VW * d.qteBarWidthRatio;
    float barH = d.qteBarHeight;
    // Ancla horizontal del QTE entero (barra + caja + hint) en posX del lienzo.
    // Default 0.5 → (VW - X)/2 en los tres, idéntico al centro clásico.
    float anchorCX = VW * d.qteBarPosXRatio;
    float barX = anchorCX - barW / 2.0f;
    float barY = VH * d.qteBarPosYRatio;
    float progress = (m_qteTimerMax > 0) ? (float)(m_qteTimer / m_qteTimerMax) : 0.0f;

    float cr = (progress < 0.5f) ? 1.0f : progress * 2.0f;
    float cg = (progress > 0.5f) ? 1.0f : progress * 2.0f;
    if (cr > 1.0f) cr = 1.0f;
    if (cg > 1.0f) cg = 1.0f;

    draw2d::drawRect_noBatch(barX, barY, barW, barH, 0.15f, 0.15f, 0.20f, 0.7f);
    draw2d::drawRect_noBatch(barX, barY, barW * progress, barH, cr, cg, 0.15f, 0.9f);
    draw2d::drawRectBorder_noBatch(barX, barY, barW, barH, 0.5f, 0.5f, 0.6f, 0.5f, 2.0f);

    float boxSize = d.qteKeyBoxSize;
    float boxX = anchorCX - boxSize / 2.0f;
    float boxY = barY + barH + d.qteBarSpacing;

    draw2d::drawRect_noBatch(boxX, boxY, boxSize, boxSize, 0.08f, 0.10f, 0.22f, 0.88f);
    draw2d::drawRectBorder_noBatch(boxX, boxY, boxSize, boxSize, cr, cg, 0.3f, 0.9f, 3.0f);

    // C-FUENTE-CHROME: fuente de QTE (tecla). qte_fuente > base.
    // Medir=dibujar con el mismo renderer (§7.72).
    TextRenderer* qf = resolveQteFont(m_text);
    float kw = qf->measureWidth(m_qteKeyName);
    float kh = qf->getLineHeight();
    qf->drawText_noBatch(m_qteKeyName,
        boxX + (boxSize - kw) / 2.0f,
        boxY + (boxSize - kh) / 2.0f,
        1.0f, 1.0f, 1.0f, 1.0f);

    if (m_hasNameFont)
    {
        // DI-7: el hint del QTE lo lee el JUGADOR, así que sigue su idioma.
        // La grafía que el CREADOR escribió en `tecla:` viaja por otro lado y
        // no la toca este lote (parser, disco y diagnósticos la conservan).
        std::string hint = eclips::langUI("Presiona!", "Press!");
        // Hint por la MISMA fuente QTE (endurecer al hermano), fallback m_name.
        TextRenderer* qfh = resolveQteFont(m_name);
        float hw = qfh->measureWidth(hint);
        qfh->drawText_noBatch(hint,
            anchorCX - hw / 2.0f, boxY + boxSize + 14.0f,
            0.8f, 0.85f, 0.95f, 0.85f);
    }

    // F2.C: end batch
    draw2d::g_batch->end();
}

// B2 — campo de captura de texto ('entrada'). Flaco: prompt centrado,
// buffer debajo, cursor "|" con blink DURO (on/off) ~530ms. El dato (prompt
// ya interpolado + buffer) llega por params; el box no lo guarda (renderer
// puro, Fork 1B). Reusa fuente y color de cuerpo del tema (d.textR..A +
// m_text) → hereda el look sin imponer nada (§0).
//
// TODO(hermanos): cuelgan aquí sin engordar la primitiva (§7.40):
//   - Plate PNG detrás (reusar drawTexture9Slice_noBatch como renderDialogBox).
//   - Botones OK/Cancelar (reusar el sistema de botones del box).
//   - Posición/color/glifo del cursor themeables (hoy hardcode sano).
//   - Wrap del prompt largo (hoy una línea, caso barato; igual que glow).
void eclips::VNDialogBox::renderTextInput(float VW, float VH,
    const std::string& prompt, const std::string& buffer,
    bool required, bool okHover)
{
    if (!m_hasFont) return;

    const DialogBoxStyle& d = activeStyle();
    const float lineH = m_text->getLineHeight();   // base — lo usa el OK (chrome)

    // Fuente del aviso con entrada_tamano (0 = base, vía guard de sizedFont).
    // Escala SOLO prompt+buffer+cursor; el OK queda base (F2). Sin
    // resolveBoxFont: el aviso NO hereda caja_fuente (F1, anti silent-change).
    TextRenderer* avisoFont = sizedFont(m_text, d.inputSize);
    const float avisoLineH = avisoFont->getLineHeight();

    draw2d::g_batch->begin();

    // ── Marco/fondo del aviso (CP2) ───────────────────────────────────
    // Decoración DETRÁS del cluster: se encola PRIMERO → queda al fondo del
    // batch (painter's order; prompt/buffer/OK encima). Opt-in: default OFF
    // preserva el aviso flotante de hoy. drawButtonBg = el MISMO primitivo de
    // la caja y el OK: PNG (9-slice o stretch) si hay sprite cargado, si no
    // cuadrado nativo + borde. Geometría = control absoluto del creador
    // (W/H/offset); el default centra el marco sobre el span prompt→OK, con
    // el fondo del OK leído de su fuente única (getTextInputOkRect, §7.100).
    if (d.inputFrameVisible)
    {
        Texture* frameTex = (d.hasInputFrameSprite() && m_assets)
            ? m_assets->get(d.inputFrameSpritePath) : nullptr;

        float promptTopY = VH * d.inputPosYRatio;
        UIBtnRect ok = getTextInputOkRect(VW, VH);
        float clusterCenterY = (promptTopY + (ok.y + ok.h)) * 0.5f;

        float fx = (VW - d.inputFrameW) * 0.5f + d.inputFrameOffsetX;
        float fy = clusterCenterY - d.inputFrameH * 0.5f + d.inputFrameOffsetY;

        draw2d::drawButtonBg(fx, fy, d.inputFrameW, d.inputFrameH,
            frameTex, d.hasInputFrame9Slice(),
            d.inputFrameSliceLeft, d.inputFrameSliceRight,
            d.inputFrameSliceTop, d.inputFrameSliceBottom,
            d.inputFrameBgR, d.inputFrameBgG, d.inputFrameBgB, d.inputFrameBgA,
            d.inputFrameBorderR, d.inputFrameBorderG, d.inputFrameBorderB, d.inputFrameBorderA,
            d.inputFrameBorderThickness);
    }

    // Prompt centrado (una línea, sin wrap — caso barato).
    if (!prompt.empty())
    {
        float pw = avisoFont->measureWidth(prompt);
        float px = (VW - pw) / 2.0f;
        float py = VH * d.inputPosYRatio;   // centro-alto; themeable después
        avisoFont->drawText_noBatch(prompt, px, py,
            d.textR, d.textG, d.textB, d.textA);
    }

    // Buffer + cursor, centrados, debajo del prompt.
    float bufW = avisoFont->measureWidth(buffer);
    float bufX = (VW - bufW) / 2.0f;     // buffer vacío → cursor al centro
    float bufY = VH * d.inputPosYRatio + avisoLineH + 16.0f;
    if (!buffer.empty())
        avisoFont->drawText_noBatch(buffer, bufX, bufY,
            d.textR, d.textG, d.textB, d.textA);

    // Cursor "|": blink DURO on/off (no fade — un cursor es señal funcional,
    // no adorno; el fade senoidal queda para el indicador de avance). Reusa
    // m_totalTime (tickea cada frame en update()). Periodo ~1.06s (≈530ms
    // encendido + 530ms apagado), el clásico de un campo de texto.
    const float caretPeriod = 1.06f;
    bool caretOn = std::fmod(m_totalTime, caretPeriod) < (caretPeriod * 0.5f);
    if (caretOn)
    {
        float caretX = bufX + bufW;   // justo después del buffer
        avisoFont->drawText_noBatch("|", caretX, bufY,
            d.textR, d.textG, d.textB, d.textA);
    }

    // ── Botón OK (2º consumidor de drawButtonBg) ──────────────────────
    // Geometría desde getTextInputOkRect → render y hit-test (CP3) beben de
    // la MISMA fuente (§7.100). Estados CP2: normal / deshabilitado. El
    // deshab. lo dispara 'requerido' con buffer vacío (DDLC grisea el OK);
    // el hover llega en CP3 (necesita el mouse). Cara: rect (default del
    // motor); el PNG cuelga después con su pineo. Texto = m_text, centrado.
    {
        UIBtnRect ok = getTextInputOkRect(VW, VH);
        const bool okDisabled = required && buffer.empty();
        const bool okHov = okHover && !okDisabled;   // disabled gana sobre hover

        float bgR = okDisabled ? d.inputOkOffBgR : (okHov ? d.inputOkHoverBgR : d.inputOkBgR);
        float bgG = okDisabled ? d.inputOkOffBgG : (okHov ? d.inputOkHoverBgG : d.inputOkBgG);
        float bgB = okDisabled ? d.inputOkOffBgB : (okHov ? d.inputOkHoverBgB : d.inputOkBgB);
        float bgA = okDisabled ? d.inputOkOffBgA : (okHov ? d.inputOkHoverBgA : d.inputOkBgA);
        float brR = okDisabled ? d.inputOkOffBorderR : (okHov ? d.inputOkHoverBorderR : d.inputOkBorderR);
        float brG = okDisabled ? d.inputOkOffBorderG : (okHov ? d.inputOkHoverBorderG : d.inputOkBorderG);
        float brB = okDisabled ? d.inputOkOffBorderB : (okHov ? d.inputOkHoverBorderB : d.inputOkBorderB);
        float brA = okDisabled ? d.inputOkOffBorderA : (okHov ? d.inputOkHoverBorderA : d.inputOkBorderA);
        float txR = okDisabled ? d.inputOkOffTextR : (okHov ? d.inputOkHoverTextR : d.inputOkTextR);
        float txG = okDisabled ? d.inputOkOffTextG : (okHov ? d.inputOkHoverTextG : d.inputOkTextG);
        float txB = okDisabled ? d.inputOkOffTextB : (okHov ? d.inputOkHoverTextB : d.inputOkTextB);
        float txA = okDisabled ? d.inputOkOffTextA : (okHov ? d.inputOkHoverTextA : d.inputOkTextA);

        if (d.inputOkStyle == UIBtnStyle::Box)   // 'texto' → letra sola, sin caja
            draw2d::drawButtonBg(ok.x, ok.y, ok.w, ok.h,
                nullptr, false, 0.0f, 0.0f, 0.0f, 0.0f,   // rect (PNG después)
                bgR, bgG, bgB, bgA,
                brR, brG, brB, brA, d.inputOkBorderThickness);

        float tw = m_text->measureWidth(d.inputOkLabel);
        float tx = ok.x + (ok.w - tw) * 0.5f;
        float ty = ok.y + (ok.h - lineH) * 0.5f;
        m_text->drawText_noBatch(d.inputOkLabel, tx, ty, txR, txG, txB, txA);
    }

    draw2d::g_batch->end();
}

void eclips::VNDialogBox::getDialogBtnRects(std::vector<UIBtnRect>& out, float VW, float VH) const
{
    out.clear();

    const DialogBoxStyle& d = activeStyle();

    // boton_fuente: medir con la MISMA fuente que dibuja el loop de chrome
    // (§7.72) — medir en base y dibujar en boton_fuente desincronizaría los
    // anchos de botón de la letra. boton_tamano reescala (0 = heredar) —
    // el MISMO sizedFont que dibuja el loop → ancho calza con la letra.
    TextRenderer* chromeFont = sizedFont(resolveChromeFont(m_name), d.btnSize);

    float bH = d.height;
    float bW = VW * d.widthRatio;
    float bX = (VW - bW) / 2.0f;
    float bY = VH - bH - d.marginBottom;
    float btnY = bY + bH - d.uiBtnHeight - d.uiBtnMarginBottom;

    const bool vis[] = {
        d.uiBtnAutoVisible, d.uiBtnSkipVisible,
        d.uiBtnSaveVisible, d.uiBtnLoadVisible,
        d.uiBtnLogVisible,  d.uiBtnMenuVisible
    };

    // ── Pase 1: medir ancho total que ocuparán los botones visibles ──
    // Necesario para calcular el origen X cuando alineación es Center o Left.
    float totalW = 0.0f;
    int   visCount = 0;
    for (int i = 0; i < 6; ++i) {
        if (!vis[i]) continue;
        float tw = chromeFont->measureWidth(d.uiBtnLabels[i]);
        totalW += tw + d.uiBtnPaddingX * 2.0f;
        ++visCount;
    }
    if (visCount > 1) totalW += (visCount - 1) * d.uiBtnGap;

    // ── Pase 2: calcular origen X según alineación y emitir rects ────
    // Iteramos siempre i=0..5 (orden lógico Auto, Skip, Save, Load, Log, Menu)
    // y avanzamos a la derecha. Esto garantiza el mismo orden visible en
    // las 3 alineaciones; lo que cambia es solo el punto de inicio.
    float startX;
    switch (d.uiBtnAlign) {
    case UIBtnAlign::Left:
        startX = bX + d.uiBtnMarginRight; // reusamos margen como "lateral"
        break;
    case UIBtnAlign::Center:
        startX = bX + (bW - totalW) * 0.5f;
        break;
    case UIBtnAlign::Right:
    default:
        startX = bX + bW - d.uiBtnMarginRight - totalW;
        break;
    }

    float cursorX = startX;
    for (int i = 0; i < 6; ++i) {
        if (!vis[i]) continue;
        float tw = chromeFont->measureWidth(d.uiBtnLabels[i]);
        float btnW = tw + d.uiBtnPaddingX * 2.0f;
        out.push_back({ cursorX, btnY, btnW, d.uiBtnHeight, i });
        cursorX += btnW + d.uiBtnGap;
    }
}

eclips::VNDialogBox::UIBtnRect eclips::VNDialogBox::getTextInputOkRect(float VW, float VH) const
{
    const DialogBoxStyle& d = activeStyle();
    // Espejo EXACTO del layout de renderTextInput: el buffer cuelga del ancla
    // pos_y con la línea del AVISO (entrada_tamano aplicado) — MISMA fuente que
    // el render para que el OK no se desincronice del buffer al crecer la letra
    // (§7.100). El OK no escala (F2), pero su POSICIÓN sigue al buffer grande.
    const float avisoLineH = m_text
        ? sizedFont(m_text, d.inputSize)->getLineHeight() : 0.0f;

    float bufY = VH * d.inputPosYRatio + avisoLineH + 16.0f;
    float baseX = (VW - d.inputOkW) * 0.5f;
    float baseY = bufY + avisoLineH + 28.0f;

    UIBtnRect r;
    r.x = baseX + d.inputOkOffsetX;
    r.y = baseY + d.inputOkOffsetY;
    r.w = d.inputOkW;
    r.h = d.inputOkH;
    r.index = 0;
    return r;
}

void eclips::VNDialogBox::getChoiceRects(std::vector<UIBtnRect>& out, float VW, float VH) const
{
    out.clear();
    const DialogBoxStyle& d = activeStyle();
    int n = (int)m_choices.size();
    if (n == 0) return;

    float btnW = VW * d.choiceBtnWidthRatio;
    float btnH = d.choiceBtnHeight;
    float gap = d.choiceBtnGap;
    float totalH = n * (btnH + gap) - gap;
    // Ancla = centro del bloque en (posX, posY) del lienzo. Default 0.5/0.5
    // reproduce exacto el centro clásico: (VW-btnW)/2 y VH/2 - totalH/2.
    float startX = VW * d.choicePosXRatio - btnW / 2.0f;
    float startY = VH * d.choicePosYRatio - totalH / 2.0f;

    for (int i = 0; i < n; ++i)
    {
        float cy = startY + i * (btnH + gap);
        out.push_back({ startX, cy, btnW, btnH, i });
    }
}

bool eclips::VNDialogBox::selloVeta(eclips::VNAction a) const
{
    if (accionPermitida(a)) return false;
    std::cerr << eclips::vnVozSelloVeta(eclips::vnNombreDeAccion(a)) << "\n";
    return true;
}

bool eclips::VNDialogBox::handleAction(InputAction action, int choiceIndex, uint32_t qteKey)
{
    if (m_showingHistory)
    {
        if (action == InputAction::HistoryClose || action == InputAction::Cancel) {
            m_showingHistory = false;
            return true;
        }
        if (action == InputAction::HistoryScrollUp) { m_historyScrollTarget += 80.0f; return true; }
        if (action == InputAction::HistoryScrollDown) {
            if (m_historyScrollTarget <= 0.0f) {
                m_showingHistory = false;
                return true;
            }
            m_historyScrollTarget -= 80.0f;
            if (m_historyScrollTarget < 0.0f) m_historyScrollTarget = 0.0f;
            return true;
        }
        return true; // consumir cualquier otra acción
    }

    // ═══════════════════════════════════════════════
    // Clic en botones de UI (debe ser antes que Confirm genérico)
    // ═══════════════════════════════════════════════
    if (m_phase == Phase::Dialogue && action == InputAction::Confirm && m_mouseVX > 0 && m_mouseVY > 0)
    {
        std::vector<UIBtnRect> rects;
        getDialogBtnRects(rects, m_virtualW, m_virtualH);
        for (const auto& r : rects) {
            if (m_mouseVX >= r.x && m_mouseVX <= r.x + r.w &&
                m_mouseVY >= r.y && m_mouseVY <= r.y + r.h)
            {
                // ── EL SELLO, SOBRE LOS SEIS BOTONES DEL CHROME ─────────────
                // Acá es donde un clic se convierte en verbo: los `UIBtn*` NO
                // llegan de `translateSDLEventToAction`, nacen en este switch.
                // Por eso el sello vive adentro de la caja y no del lado del
                // state — el hit-test es el traductor real de esta puerta.
                //
                // Un botón sellado CONSUME el clic igual que uno permitido: si
                // dejara caer el evento, el clic seguiría de largo hasta el
                // avance de abajo y el botón "muerto" adelantaría el diálogo.
                // Un botón sellado consume el clic y no hace nada — el MISMO
                // `verboDeBoton` que decide cómo se pinta decide si corre, así
                // que "gris" y "muerto" no pueden desincronizarse.
                if (selloVeta(verboDeBoton(r.index))) return true;

                switch (r.index) {
                case 0: handleAction(InputAction::UIBtnAuto); break;
                case 1: if (m_skipAllowed) handleAction(InputAction::UIBtnSkip); break;
                case 2: m_pendingUIEvent = Event::OpenSave;  break;
                case 3: m_pendingUIEvent = Event::OpenLoad;  break;
                case 4: handleAction(InputAction::HistoryOpen); break;
                case 5: m_pendingUIEvent = Event::OpenMenu;  break;
                }
                return true;
            }
        }
    }

    // ═══════════════════════════════════════════════
    // Elección de opciones (clic)
    // ═══════════════════════════════════════════════
    if (!m_choices.empty() && action == InputAction::Confirm)
    {
        // ── `CP5`: EL HIT-TEST SE FUE, Y NO ES UN CAMBIO DE CONDUCTA ────────
        //
        // Acá vivía un `for` sobre `getChoiceRects` comparando `m_mouseVX/VY`.
        // Ahora se lee el ÍNDICE, y es lo mismo: mientras el puntero mande,
        // `m_choiceHover` **es** lo que ese hit-test devolvía —lo calcula el
        // mismo hit-test, en `update()`, desde la misma posición—. Y no hay
        // skew de frame: el `m_mouseVX` que este bloque leía a la hora del
        // click también era el del frame anterior (`setMousePos` corre en el
        // `update` del state, `VNScriptState.h:1424`).
        //
        // Lo que SÍ cambia es lo que el hit-test nunca pudo hacer: si el foco
        // lo movió el teclado, `avanzar` activa el focado (§9). Guardián de
        // no-op: `harness_foco` §1, 108 comparaciones sobre el dominio entero.
        //
        // ── `D-36`: EL CLICK FUERA DE LAS OPCIONES ADELANTA EL TEXTO ────────
        //
        // Sigue vivo, y ahora es la segunda rama de la ley en vez de un `if`
        // suelto abajo. Con la forma pegada de `D-33` el caption se escribe CON
        // las opciones ya en pantalla, así que el jugador necesita el mismo
        // gesto de siempre para completarlo — y que sea el mismo gesto es la
        // razón de que se sienta natural: no aprende nada nuevo. No avanza, no
        // elige, no consume la elección.
        //
        // **Su otra mitad la firmó el fundador y el motor ya la cumple**: la
        // opción gana sobre el caption porque va PRIMERA en la ley, así que un
        // click sobre una opción elige aunque el texto esté a medias. Ese orden
        // tiene guardián propio (`harness_foco`, la deriva que lo invierte cae
        // en 21 celdas de la §1).
        //
        // Gateado por `survivesChoice()` a propósito: si la caja no se dibuja
        // no hay caption que adelantar, y tocar el typewriter de un texto
        // invisible sería mover estado que nadie pidió.
        const eclips::FocoVeredicto v = eclips::focoConfirmar(
            m_choiceHover,
            m_persist.survivesChoice(),
            !m_displayText.empty() && m_typewriterChars < displayChars());

        if (v.que == eclips::FocoConfirm::Activar) {
            m_selectedChoice = v.indice;
            m_pendingChoice  = true;
        }
        else if (v.que == eclips::FocoConfirm::CompletarCaption) {
            m_typewriterChars = displayChars();
        }
        return true;   // consumir siempre: un `Confirm` que cayera de largo
                       // llegaría al avance genérico y adelantaría el guion
                       // por debajo del `elegir`.
    }

    // Avanzar / completar typewriter
    if (action == InputAction::Confirm)
    {
        // ── Click durante pausa por {esperar_click} → destrabar y continuar ──
        // No completa la línea ni avanza al siguiente diálogo, solo destraba.
        if (m_waitingMidClick) {
            m_waitingMidClick = false;
            m_pauseTimer = 0.0f;
            return false; // consumido — no avanzar línea
        }
        if (!m_displayText.empty() && m_typewriterChars < displayChars()) {
            m_typewriterChars = displayChars();
            return true;
        }
        m_pendingAdvance = true;
        return true;
    }

    // Atajos numéricos para choices
    if (!m_choices.empty()) {
        int idx = -1;
        if (action == InputAction::Choice1) idx = 0;
        else if (action == InputAction::Choice2) idx = 1;
        else if (action == InputAction::Choice3) idx = 2;
        else if (action == InputAction::Choice4) idx = 3;
        if (idx >= 0 && idx < (int)m_choices.size()) {
            m_selectedChoice = idx;
            m_pendingChoice = true;
            return true;
        }
    }

    // ── `CP5`: EL ANILLO DE FOCO, hermano de los atajos numéricos ───────────
    //
    // Va acá al lado y no arriba a propósito: son la misma familia -las dos
    // formas de llegar a una opción sin mouse- y la de los números **no se
    // toca**. El anillo se AGREGA: cubre de la quinta en adelante, que es donde
    // `opcion_1..4` se queda corta (el menú del demo tiene cinco).
    //
    // **⚠ `CP6` (2026-08-08): ESTE BLOQUE YA NO RECIBE LOS VERBOS DE FOCO.**
    //
    // *(Decía: «`moverFoco` devuelve `false` sin opciones, y ese `false`
    // importa: deja que la tecla siga su camino en vez de morir acá», y
    // despachaba `FocusUp`/`FocusDown` a `moverFoco`.)*
    //
    // El state los intercepta **antes** de delegar acá, porque el anillo dejó de
    // ser de la caja: en `config.ecl`/`load.ecl`/`pausa.ecl` la lista de
    // focables cruza `control`, `ranura` y choices, y la caja sólo ve el último
    // tercio. Lo que la caja recibe ahora es `setFocoChoice(idx)` — su segmento
    // ya resuelto.
    //
    // **No queda un `if` muerto acá a propósito.** Un branch que no puede
    // dispararse se lee como conducta viva y el próximo lote lo mantiene; lo
    // que sí queda es esta nota, que dice dónde mirar. Si algún día llega un
    // `FocusUp` hasta este punto, cae al `return false` del final y la flecha
    // sigue su camino — que es la misma red que el `false` viejo daba.

    // ── QTE — EL JUEZ (`QteGate.h`) ────────────────────────────────────────
    //
    // El comparador siempre estuvo bien escrito; lo que no existía era quien
    // le mandara un `QTEKey` con la tecla adentro. Ahora llega, y la ley del
    // veredicto vive en un header que la batería SÍ alcanza (esta TU arrastra
    // GL — misma pared que `MERGE-DUPLICADO`).
    //
    // **El `return` es `qteConsume`, y ahí está la mitad del lote** (ley 2,
    // firma del fundador): acá decía `return true; // tecla incorrecta
    // consumida`, y con el productor nuevo eso habría hecho que el QTE se
    // tragara TODO el teclado mientras el reloj baja — sin pausa, sin guardado
    // rápido. Devolviendo `false`, la tecla que no acierta sigue su camino a los
    // atajos globales y se comporta idéntico a antes de este lote. Y `ESC`
    // funciona como tecla de QTE por el mismo mecanismo, sin excepción escrita
    // a mano: cuando el QTE la pidió, acierta y se consume.
    if (action == InputAction::QTEKey) {
        const eclips::QteVeredicto v =
            eclips::qteVeredicto(m_qteTimer > 0.0f, qteKey, m_qteKeycode);
        if (v == eclips::QteVeredicto::Acierta) {
            m_qte.acertar();
            m_qteTimer = 0.0f;   // el reloj deja de contar; la ley 3 lo protege igual
        }
        return eclips::qteConsume(v);
    }

    // Toggles
    if (action == InputAction::ToggleAuto || action == InputAction::UIBtnAuto) {
        // La tecla `A` entra por acá traducida; el botón entra por el switch de
        // arriba, que ya consultó. Este gate es el que cubre la TECLA, y por
        // eso está escrito sobre el verbo y no sobre `SDLK_a` (`D-47`).
        if (selloVeta(eclips::VNAction::Auto)) return true;
        m_autoMode = !m_autoMode;
        // Auto y Skip pueden coexistir (paridad Ren'Py: Ctrl-hold + Auto ya
        // coexisten; el botón ahora también). No se apagan mutuamente.
        m_autoTimer = 0.0f;
        m_toastMsg = m_autoMode ? eclips::langUI("Auto ON", "Auto ON")
                                : eclips::langUI("Auto OFF", "Auto OFF");
        m_toastTimer = 1.5f;
        return true;
    }
    // Skip por toggle del botón (m_skipToggle), independiente del Ctrl-hold
     // (m_skipHeld). skipActive() = (held || toggle) && allowed. Held y toggle
     // conviven, y ambos conviven con Auto (paridad Ren'Py).
    if (action == InputAction::UIBtnSkip) {
        if (selloVeta(eclips::VNAction::SaltarTexto)) return true;
        m_skipToggle = !m_skipToggle;
        m_skipTimer = 0.0f;
        m_toastMsg = m_skipToggle ? eclips::langUI("Skip ON", "Skip ON")
                                  : eclips::langUI("Skip OFF", "Skip OFF");
        m_toastTimer = 1.0f;
        return true;
    }

    // El historial se abre por TRES puertas -rueda, botón y tecla- y las tres
    // convergen en este `HistoryOpen`. Ése es el argumento decisivo de `D-47`
    // hecho código: UN verbo sellado tapa las tres, donde tres sellos por tecla
    // habrían dejado una afuera **que se ve idéntica en pantalla**.
    if (action == InputAction::HistoryOpen &&
        selloVeta(eclips::VNAction::Historial)) return true;

    if (action == InputAction::HistoryOpen && !m_history.empty()) {
        m_showingHistory = true;
        m_historyScroll = 0.0f;
        m_historyScrollTarget = 0.0f;
        return true;
    }

    return false;
}

// TODO: implement — indicadores visuales (puntero "esperando avance", flecha, etc.)
void eclips::VNDialogBox::renderIndicators(float, float) {}

bool eclips::VNDialogBox::isWaiting() const {
    return !m_displayText.empty() && m_typewriterChars < displayChars();
}

void eclips::VNDialogBox::renderHistoryOverlay(float VW, float VH)
{
    if (!m_hasFont || m_history.empty()) return;

    // El historial no leía NADA del estilo hasta este lote (su look es hardcode
    // sano: overlay negro, colores fijos). Entra sólo por `interlineado`, que sí
    // es del creador — ver el porqué abajo, donde se calcula el alto total.
    const DialogBoxStyle& d = activeStyle();

    // F2.C: begin batch
    draw2d::g_batch->begin();

    // ── Overlay oscuro sobre toda la pantalla ──────────────────────
    draw2d::drawRect_noBatch(0, 0, VW, VH, 0.0f, 0.0f, 0.0f, 0.88f);

    // ── Título ─────────────────────────────────────────────────────
    if (m_hasNameFont)
    {
        std::string title = eclips::langUI("- Historial -", "- History -");
        float tw = m_name->measureWidth(title);
        m_name->drawText_noBatch(title,
            (VW - tw) / 2.0f, 18.0f,
            0.65f, 0.78f, 1.0f, 0.9f);
    }

    // ── Hint de controles ──────────────────────────────────────────
    if (m_hasNameFont)
    {
        std::string hint = eclips::langUI(
            "Rueda / PgUp / PgDn para navegar  |  ESC o Click para cerrar",
            "Wheel / PgUp / PgDn to scroll  |  ESC or Click to close");
        float hw = m_name->measureWidth(hint);
        m_name->drawText_noBatch(hint,
            (VW - hw) / 2.0f, VH - 32.0f,
            0.50f, 0.55f, 0.65f, 0.7f);
    }

    // ── Área de contenido ──────────────────────────────────────────
    float contentX = VW * 0.08f;
    float contentW = VW * 0.84f;
    float contentTop = 58.0f;
    float contentBottom = VH - 48.0f;

    float entryGap = 14.0f;
    float nameHeight = m_hasNameFont ? (m_name->getLineHeight() + 4.0f) : 0.0f;

    // Calcular altura total del historial
    // `interlineado` va también acá: el historial muestra el MISMO texto del
    // creador, y si la caja lo separa y el log no, la misma frase se lee distinta
    // en los dos lugares. Y va a las TRES llamadas (las dos medidas y el dibujo)
    // o el scroll calcularía sobre un alto que no es el que se pinta.
    float totalHeight = 0.0f;
    for (const auto& entry : m_history)
    {
        if (!entry.speaker.empty())
            totalHeight += nameHeight;
        float textH = m_text->measureWrappedHeight(
            entry.text, contentW - 32.0f, d.lineSpacing);
        totalHeight += textH + entryGap;
    }

    // Limitar scroll
    float maxScroll = totalHeight - (contentBottom - contentTop);
    if (maxScroll < 0) maxScroll = 0;
    if (m_historyScroll > maxScroll) m_historyScroll = maxScroll;
    if (m_historyScroll < 0)         m_historyScroll = 0;

    // ── Dibujar entradas desde abajo (más reciente = abajo) ────────
    //    Scroll 0       = vemos el final (lo más reciente)
    //    Scroll maxScroll = vemos el inicio (lo más antiguo)
    float cursorY;
    if (totalHeight <= (contentBottom - contentTop))
        cursorY = contentTop; // Todo cabe, empezar desde arriba
    else
        cursorY = contentBottom - totalHeight + m_historyScroll;

    // Clipping manual: solo dibujamos lo que está visible
    for (const auto& entry : m_history)
    {
        // Nombre del hablante
        if (!entry.speaker.empty() && m_hasNameFont)
        {
            if (cursorY + nameHeight > contentTop &&
                cursorY < contentBottom)
            {
                m_name->drawText_noBatch(entry.speaker,
                    contentX + 16.0f, cursorY,
                    0.45f, 0.72f, 1.0f, 1.0f);
            }
            cursorY += nameHeight;
        }

        // Texto del diálogo
        float textH = m_text->measureWrappedHeight(
            entry.text, contentW - 32.0f, d.lineSpacing);

        if (cursorY + textH > contentTop && cursorY < contentBottom)
        {
            // Color diferente para narrador vs diálogo
            float r = entry.speaker.empty() ? 0.75f : 0.92f;
            float g = entry.speaker.empty() ? 0.75f : 0.92f;
            float b = entry.speaker.empty() ? 0.80f : 0.92f;

            m_text->drawTextWrapped_noBatch(entry.text,
                contentX + 16.0f, cursorY,
                contentW - 32.0f,
                // §7.410 — el historial NO tiene typewriter: dibuja la entrada
                // entera. Decía `(int)entry.text.size()`, o sea un presupuesto
                // en BYTES, y los saltos del wrap se lo comían: a toda entrada
                // que envolvía le faltaba el final. `-1` es "sin límite", que
                // es lo que este sitio siempre quiso decir.
                -1,
                r, g, b, 0.95f,
                d.lineSpacing);
        }

        cursorY += textH + entryGap;

        // Si ya pasamos la zona visible por abajo, no dibujar más
        if (cursorY > contentBottom + 200.0f)
            break;
    }

    // ── Scrollbar visual (sutil) ───────────────────────────────────
    if (maxScroll > 0)
    {
        float barH = (contentBottom - contentTop);
        float thumbH = std::max(barH * barH / (totalHeight), 30.0f);
        float ratio = m_historyScroll / maxScroll;
        // Invertir: scroll 0 = thumb abajo (estamos al final)
        float thumbY = contentTop + (1.0f - ratio) * (barH - thumbH);
        float barX = VW - 18.0f;

        // Track
        draw2d::drawRect_noBatch(barX, contentTop, 6.0f, barH,
            0.2f, 0.2f, 0.3f, 0.3f);
        // Thumb
        draw2d::drawRect_noBatch(barX, thumbY, 6.0f, thumbH,
            0.4f, 0.5f, 0.9f, 0.6f);
    }

    // F2.C: end batch
    draw2d::g_batch->end();
}
