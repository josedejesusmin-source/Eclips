// ============================================================================
// DialogBox.h — Componente de diálogo reutilizable para Eclips Engine
// ============================================================================
//
// Este componente maneja TODO lo relacionado con mostrar texto, choices,
// QTEs, historial e indicadores durante una sesión de diálogo.
//
// Es PORTABLE: no incluye SDL. Usa el sistema de InputAction para que
// VNScriptState (u otros estados) le pasen eventos ya traducidos desde
// la plataforma concreta (teclado, gamepad, touch, etc.).
//
// Uso desde VNScriptState:
//   - Se instancia como miembro (o como puntero único).
//   - En onEnter: m_dialogBox.setTheme(m_theme);
//                 m_dialogBox.setTextRenderers(&m_textRenderer, &m_nameRenderer);
//   - En executeNextCmd: m_dialogBox.startDialogue(...) / startChoices(...) / startQTE(...)
//   - En update: auto result = m_dialogBox.update(dt, isActive, isPaused);
//                switch (result.event) { ... }
//   - En render: m_dialogBox.renderDialogBox(VW, VH);
//   - En handleEvent: si la fase es de diálogo, traducir SDL_Event a InputAction
//                     y llamar a m_dialogBox.handleAction(...).
//
// ============================================================================
// ============================================================================
// DialogBox.h — Componente de diálogo reutilizable para Eclips Engine (v2)
// ============================================================================
// Ahora todos los valores de geometría y comportamiento vienen de Theme.
// No hay constantes mágicas.
// ============================================================================

#pragma once

#include "engine/render/TextRenderer.h" // Para CharEffect, TextRenderer
#include "engine/core/TextBudget.h"     // §7.410: el typewriter cuenta CARACTERES
#include "engine/render/AppearPolicy.h" // `RO-5`: la aparición, donde un harness la ve
#include "engine/render/BoxPersistence.h" // `ELEGIR-CON-CAJA`: si la caja sobrevive
#include "engine/core/VNActionSeal.h"   // `D-47`: el sello, sobre la acción
#include "engine/core/QteGate.h"        // `QTE-SIN-PUERTA`: el juez y la puerta única
#include "engine/core/VNInputAction.h"  // `CP1`: el enum, donde un harness lo ve
#include "engine/core/FocusRing.h"      // `CP5`: el anillo, donde un harness lo ve
// FontManager se forward-declara abajo (fuente inline); el include real va en el .cpp
#include "EclipScript.h"    // Para EclipChoice, VNPosition
#include "Theme.h"          // Tema visual

#include <string>
#include <vector>
#include <deque>
#include <array>
#include <optional>

namespace eclips {

    // Forward declaration: VNDialogBox guarda un puntero opcional al
    // PersonajeManifest del speaker activo, pero solo accede a sus campos
    // en el .cpp (helper de cadena de velocidad). El header no necesita
    // ver la definición.
    struct PersonajeManifest;

    // Forward declaration: VNDialogBox guarda un puntero a AssetManager
    // para resolver paths de sprites de caja (boxSpritePath, namePlateSpritePath)
    // a Texture* en render time. Los sprites son pineados por el dueño del
    // AssetManager (VNScriptState) en el lifecycle del theme — VNDialogBox
    // solo consulta. Puntero-SSOT (§1.5), paralelo a m_theme.
    class AssetManager;
    class FontManager; // fuente inline: la caja pasa el fm al render de texto rico
    class Texture;     // cara PNG del botón de choice (drawButtonBg)

    // ── `InputAction` SE MUDO A `engine/core/VNInputAction.h` (CP1) ─────────
    //
    // Vivia aca, y este archivo incluye `TextRenderer.h` -> `<SDL_opengl.h>`.
    // Ese arrastre hacia INALCANZABLE para la bateria cualquier ley que
    // devolviera un `InputAction`, y la ley central de `CP1` devuelve
    // exactamente eso: "un verbo mas una fase dan la misma accion que la
    // cascada de `SDLK_*` daba antes" (`VNVerbRoute.h`).
    //
    // El enum ya era puro -no toca SDL, GL ni un tipo del render-: estaba del
    // otro lado de la pared por donde NACIO, no por lo que necesita. §7.433.
    //
    // Mismo namespace y mismos valores en el mismo orden, asi que los 45
    // call-sites de los tres archivos que lo usan no cambiaron una letra.
    // El include va arriba, con los otros headers de ley.

    class VNDialogBox {
    public:
        enum class Phase { Dialogue, Choice, QTE };
        void setPhase(Phase p) { m_phase = p; }
        // ── `CP7`: RECIBIR LA POSICIÓN ES DERIVAR ───────────────────────────
        //
        // Hasta este lote esto sólo guardaba dos floats, y el hover se derivaba
        // en `update()` — o sea **después** de que `handleEvent` decidiera un
        // click. Con un mouse no se notaba (uno mueve, pasan frames, después
        // clickea); con un dedo, que llega MOTION+DOWN en el mismo batch, el
        // tap acertaba al objetivo del tap ANTERIOR.
        //
        // Ahora la posición y lo que de ella se deriva no se pueden separar en
        // el tiempo. `derivarHoverDePuntero` sigue corriendo también en
        // `update()`, y es idempotente: es función pura de la posición, los
        // rects y `m_focoDelPuntero`.
        void setMousePos(float vx, float vy)
        {
            m_mouseVX = vx; m_mouseVY = vy;
            derivarHoverDePuntero();
        }

        // ── Skip como HOLD real (no toggle) ──
        // VNScriptState poléa el teclado cada frame y nos dice si Ctrl está
        // presionada AHORA. update() lo consume directo: no hay estado que se
        // quede pegado (mata el bug del KEYUP huérfano en choices). El caller
        // puede forzar false en WaitingChoice para parar skip en elecciones.
        void setSkipHeld(bool held) { m_skipHeld = held; }
        void setSkipAllowed(bool allowed) { m_skipAllowed = allowed; }

        // ── PUERTA B DEL SELLO DE ACCIONES ──────────────────────────────────
        //
        // El sello se EMPUJA acá, no se consulta desde el state, y la razón es
        // medida: **los seis botones del chrome no llegan traducidos**. Un clic
        // entra como `Confirm` + posición del mouse y se resuelve por hit-test
        // ADENTRO de `handleAction` (`VNDialogBox.cpp:1414`), así que un sello
        // escrito del lado del state —después de `translateSDLEventToAction`—
        // cubriría auto y la rueda, y **ninguno de los seis botones**.
        //
        // El patrón no es nuevo: `setSkipAllowed` ya empuja un permiso que el
        // state calcula y la caja obedece. Éste es su hermano.
        //
        // ── LEY 7: EL RITMO SE APAGA, NO SE TRABA ───────────────────────────
        //
        // Se llama CADA FRAME desde `VNScriptState::update`, y por eso puede
        // sostener el invariante en vez de reaccionar a un flanco: `auto` y
        // `skip` son MODOS, no eventos, y trabarles el interruptor dejaría
        // corriendo el que ya estaba encendido cuando el sello entró. El camino
        // que usa `modalidad-aviso` para el mismo problema (sellar el `Advance`,
        // "incluido el auto ya encendido") acá está PROHIBIDO, porque `avanzar`
        // es familia Salida y no se sella nunca (`D-41`/`D-46`).
        //
        // `habilitar auto` devuelve el PERMISO, no el estado: el jugador vuelve
        // a prenderlo, que es lo que "el jugador puede hacer X" significa.
        void setActionsSealed(eclips::VNActionMask sealed)
        {
            m_accionesSelladas = sealed;
            m_autoMode = eclips::vnRitmoQueda(sealed, eclips::VNAction::Auto,
                                              m_autoMode);
            m_skipToggle = eclips::vnRitmoQueda(sealed, eclips::VNAction::SaltarTexto,
                                                m_skipToggle);
        }

        bool accionPermitida(eclips::VNAction a) const
        {
            return eclips::vnActionAllowed(m_accionesSelladas, a);
        }

        // Gemelo exacto del de `VNScriptState`, y comparten la FRASE
        // (`vnVozSelloVeta`) a propósito: las tres puertas hablan igual, así
        // que el creador busca un solo texto en la consola.
        //
        // Lo cazó `sonda_deshabilitar.ecl`: esta puerta sellaba en silencio.
        bool selloVeta(eclips::VNAction a) const;

        // ── El índice del chrome ↔ su verbo, UNA sola vez ────────────────────
        // Los seis botones se identifican por índice (`UIBtnRect::index`), y
        // ese índice lo consultan DOS lugares: el gate de `handleAction` y el
        // pintado de `D-42`. Escribir el mapa dos veces es cómo un botón queda
        // gris y funcionando, o vivo y apagado — el fallo mudo de este lote con
        // otra cara. Índices: 0 AUTO · 1 SKIP · 2 SAVE · 3 LOAD · 4 LOG · 5 MENU.
        static eclips::VNAction verboDeBoton(int index)
        {
            switch (index) {
            case 0:  return eclips::VNAction::Auto;
            case 1:  return eclips::VNAction::SaltarTexto;
            case 2:  return eclips::VNAction::MenuGuardar;
            case 3:  return eclips::VNAction::MenuCargar;
            case 4:  return eclips::VNAction::Historial;
            default: return eclips::VNAction::Menu;   // 5
            }
        }

        enum class Event {
            None,
            Advance,
            ChoiceMade,
            QTEFinished,
            OpenSave,
            OpenLoad,
            OpenLog,
            OpenMenu,
            AutoToggled,
            SkipToggled
        };

        struct UpdateResult {
            Event event = Event::None;
            int choiceIndex = -1;
            bool qteSuccess = false;
        };

        // ── Rectángulo de botón UI (con índice para identificar) ──────────
        struct UIBtnRect {
            float x, y, w, h;
            int   index;  // 0:AUTO, 1:SKIP, 2:SAVE, 3:LOAD, 4:LOG, 5:MENU
        };

        VNDialogBox() : m_theme(&fallbackTheme()) {}
        ~VNDialogBox() = default;

        // setTheme acepta nullptr de forma segura — cae al theme fallback
        // para mantener la invariante "m_theme nunca es nullptr en runtime".
        void setTheme(const Theme* theme) { m_theme = theme ? theme : &fallbackTheme(); }

        // Setea el personaje activo para resolución de la cadena de velocidad.
        // nullptr es válido (narrador, o sin manifest cargado): cae a niveles
        // posteriores de la cadena (theme + engine default).
        // Patrón paralelo a setTheme — puntero como single source of truth (§1.5).
        void setActiveSpeaker(const PersonajeManifest* speaker) { m_activeSpeaker = speaker; }

        // Setea el AssetManager para resolver paths de sprites de caja a
        // Texture* en render time. nullptr es válido: la caja simplemente
        // cae a primitivas en todos los casos (path declarado o no).
        // Patrón paralelo a setTheme — puntero-SSOT (§1.5), una sola llamada
        // en onEnter porque la instancia es estable durante la vida del state.
        void setAssetManager(AssetManager* assets) { m_assets = assets; }

        // Visibilidad de la caja de diálogo (ocultar_caja / mostrar_caja desde script).
        // Solo afecta renderDialogBox; choices y QTE ignoran el flag.
        // Auto-reset a visible al iniciar un nuevo diálogo (decisión 1A).
        // `ocultar_caja` tira una aparición armada que nadie llegó a ver (ley 7
        // de `AppearPolicy.h`): `renderDialogBox` sale temprano con la caja
        // oculta, así que esa aparición no se puede consumir nunca — y dejarla
        // viva la haría reclamar, muchos frames después, una que no es suya.
        //
        // `ELEGIR-CON-CAJA` (ley 4 de `BoxPersistence.h`): además apaga el
        // pedido de sobrevivir a la elección. Pedir esconder y que la caja
        // reaparezca en el `elegir` sería el comando contradiciéndose.
        void hide() { m_hidden = true; m_appear.cancelPending(); m_persist.drop(); }

        // ── `mostrar_caja` PELADO: instantáneo (`D-08` + `FO-7`) ────────────
        //
        // La regla, en una frase: **el fundido es lo que la caja hace sola;
        // `mostrar_caja` es cómo el creador pide que NO lo haga.** Una
        // aparición automática —una línea de diálogo con la caja no visible,
        // que son 239 de las 240 del corpus— hereda el tema; el comando
        // explícito es la excepción explícita.
        //
        // `instant()` es `(0, 0)`: ni fundido ni retraso de texto. Que el texto
        // también sea inmediato es `FO-7`, y la razón es de alcanzabilidad: bajo
        // la otra opción el retraso heredado no se podría cancelar por línea,
        // porque el escape sería `{pausa=0}` y eso muere en el parser sin voz
        // (`HO-5`) hasta que aterrice su lote. "Caja de golpe + texto retrasado"
        // sigue vivo por el camino automático, que es donde el TEMA lo pide.
        //
        // Y esto NO alcanza solo: sin la ley 5 de `AppearPolicy.h` la línea que
        // viene pegada abajo corre en el mismo `update()`, vuelve a armar, y le
        // devuelve el fundido del tema — el comando mentiría en la forma más
        // común de escribirlo (`HO-3`). Por eso `D-10` mandó las dos mitades en
        // el mismo lote.
        //
        // ── `ELEGIR-CON-CAJA`: el SEGUNDO significado, y por qué vive acá ────
        //
        // `mostrar_caja` es también cómo el creador pide que la caja SOBREVIVA
        // al `elegir` que viene (ley 2 de `BoxPersistence.h`). Es un pedido de
        // PRESENCIA y no de estado: `startDialogue` pone `m_hidden = false`
        // sola, así que "la caja se ve" es cierto casi siempre y no distingue
        // "lo pediste" de "es lo normal" — el mismo problema de `{pausa=0}`
        // (`RO-3`), resuelto igual, con un bit aparte.
        //
        // No le agrega trabajo al comando: NO quita el chrome y NO sella
        // guardado. Eso es `botones_visibles` (`Theme.h`) y `modalidad-aviso`,
        // que ya existen y son de otro dueño.
        void show()
        {
            m_hidden = false;
            m_appear.arm(AppearPolicy::instant());
            m_persist.keep();
        }
        bool isHidden() const { return m_hidden; }

        // ── `D-33`: LA FORMA PEGADA PIDE LA CAJA, Y NADA MAS ────────────────
        //
        // `"linea" elegir:` implica que la caja se ve: si no, la línea correría
        // invisible y el renglón no significaría nada. Pero pide SOLO la
        // persistencia — no toca `m_hidden` ni arma una aparición, porque la
        // línea que acaba de correr (`startDialogue`, dos llamadas antes en el
        // mismo `update()`) ya resolvió las dos cosas.
        //
        // **Y por eso no puede reusar `show()`**, que era lo obvio: `show()`
        // arma `AppearPolicy::instant()` a propósito (`D-08`/`FO-7`), así que
        // le cancelaría el fundido de entrada **al caso más común de esta
        // feature** — un aviso que abre con la caja todavía no visible. El
        // segundo significado de `mostrar_caja` sirve para el beat dramático,
        // donde el creador lo escribe; acá el pedido es implícito y tiene que
        // costar exactamente un bit.
        void keepForChoice() { m_persist.keep(); }

        // El punto único que consume la cadena de render de `VNScriptState`.
        // Puro a propósito: `m_hidden` NO se mezcla acá porque
        // `renderDialogBox` ya sale temprano con la caja oculta — una sola
        // fuente de verdad para "se ve", en vez de dos que se desincronicen.
        bool boxSurvivesChoice() const { return m_persist.survivesChoice(); }

        // Override mid-escena de la plantilla de caja (usar_caja / use_box).
        // "default" o "" = sin override, recalcula desde personaje/theme.
        void setBoxOverride(const std::string& name) {
            m_overrideBoxName = (name == "default") ? "" : name;
        }
        const std::string& boxOverride() const { return m_overrideBoxName; }

        // [Capa 3 hot-reload] Actualiza SOLO el display name del hablante, sin
        // reiniciar el typewriter ni tocar el texto en curso. Lo usa el
        // hot-reload (F8): si el manifest cambió el 'nombre' del personaje que
        // está hablando, el namebox se refresca en vivo mientras el diálogo
        // actual sigue su curso. startDialogue NO sirve acá — reinicia toda la
        // animación de escritura.
        // TODO(modo-creador): se compila fuera en build de export.
        void setSpeakerName(const std::string& name) { m_currentSpeaker = name; }

        void setVirtualResolution(float w, float h) { m_virtualW = w; m_virtualH = h; }
        void setTextRenderers(TextRenderer* textRenderer, TextRenderer* nameRenderer);
        void setFontManager(FontManager* fm) { m_fm = fm; } // fuente inline {fuente=X}

        // Funciones de inicio de modos
        // startDialogue ahora recibe RichText completo: además de displayText
        // y effects, captura lineSpeedOverride para la cadena de 6 niveles.
        void startDialogue(const std::string& speaker, const RichText& rich);

        static void collectLiveFontKeysFromEffects(
            const std::vector<CharEffect>& effects,
            std::vector<std::string>& out)
        {
            for (const auto& effect : effects)
            {
                if (!effect.fontKey.empty())
                    out.push_back(effect.fontKey);
            }
        }

        void collectLiveFontKeys(std::vector<std::string>& out) const
        {
            collectLiveFontKeysFromEffects(m_charEffects, out);
        }
        void startChoices(const std::vector<EclipChoice>& choices,
            float timer = 0.0f, int defaultIndex = -1);
        // ── CP0 `mutar`: la SALIDA de la fase de elección que no salta ───────
        //
        // Hasta este lote, una elección sólo se apagaba porque el destino era
        // una escena y lo que venía después llamaba a `startDialogue`/`startQTE`
        // (los dos limpian `m_choices`). Un destino que MUTA una variable no
        // salta a ningún lado, así que sin esto las opciones se quedarían
        // dibujadas — y peor: el countdown de `tiempo:` está gateado por
        // `!m_choices.empty()` y no por la fase (`VNDialogBox.cpp:505`), o sea
        // que dispararía un `ChoiceMade` fantasma sobre una elección que el
        // jugador ya resolvió.
        void endChoices();
        void startQTE(const std::string& keyName, uint32_t keyCode,
            float timer, const std::string& successScene,
            const std::string& failScene);

        UpdateResult update(float dt, bool isPaused = false);

        // ── `caja-entrada`: abre el frame DE RENDER ─────────────────────────
        //
        // Lo llama el dueño al principio de SU render, una vez por frame
        // dibujado. Es lo que hace que la pregunta del disparador sea
        // *"¿estaba yo en el último frame que se DIBUJÓ?"* y no *"¿me
        // dibujaron el update pasado?"*.
        //
        // LA DIFERENCIA NO ES COSMÉTICA, Y LA MIDIÓ LA SONDA. Esto vivía en
        // `update()` y el beat 2 de `sonda_caja_entrada.ecl` —dos líneas
        // seguidas, la caja no se tiene que mover— fundía igual. La causa no
        // era la fase: es el SKIP-FRAME. Mientras la caja espera input sin
        // nada animando, el motor deja de renderizar (`isQuiescent`), la caja
        // sigue ahí en la pantalla, y un flag que se limpia en el update se
        // apaga solo. La pregunta se volvía "¿me redibujaron?" cuando lo que
        // importa es "¿me está viendo?" — y con la pantalla congelada en el
        // último frame dibujado, las dos sólo coinciden si la rotación vive
        // acá. A/B que lo separó: con `indicador_avance_parpadeo si` (que
        // hace `isQuiescent` falso para siempre) el beat 2 dejó de fundir.
        //
        // Corolario feo si esto se olvida en un caller nuevo: la caja funde
        // UNA vez y no vuelve a fundir nunca — el flag se queda pegado en
        // "me dibujaron". Falla muda, y del lado que no se nota.
        void frameBegin() { m_appear.frameBegin(); }

        void renderDialogBox(float VW, float VH);
        void renderChoices(float VW, float VH);
        void renderQTE(float VW, float VH);
        // B2 — campo de captura de texto ('entrada'). VN dueño del dato: el
        // prompt (ya interpolado) y el buffer se pasan como params; el box
        // no los guarda (renderer puro, Fork 1B). Flaco: prompt centrado +
        // buffer + cursor "|". Hermanos futuros (plate PNG, botones, posición
        // themeable) cuelgan aquí sin engordar la primitiva.
        void renderTextInput(float VW, float VH,
            const std::string& prompt, const std::string& buffer,
            bool required = false, bool okHover = false);
        // Geometría del botón OK de 'entrada'. MISMO ancla (pos_y) y layout
        // que renderTextInput (bajo el buffer) → render y hit-test beben de
        // ESTA fuente única (§7.100). offset = control absoluto del creador.
        UIBtnRect getTextInputOkRect(float VW, float VH) const;

        void renderHistoryOverlay(float VW, float VH);
        void renderIndicators(float VW, float VH); // TODO: implement — reservada para indicadores visuales

        bool handleAction(InputAction action, int choiceIndex = -1, uint32_t qteKey = 0);

        // ── `CP5`: EL ANILLO DE FOCO ────────────────────────────────────────
        //
        // **El puntero recupera el mando en cada evento de puntero**, y el
        // state es quien puede decir eso porque es el único que ve el
        // `SDL_Event` crudo. Se llama en el MOTION y en el BUTTONDOWN, y el
        // segundo no es simetría: sin él, un click en el vacío con un foco
        // encendido elegiría la opción focada en vez de completar el caption —
        // la regresión de `D-36`. Guardián: `harness_foco` §6.3/§6.4.
        //
        // Barato y sin gate de fase a propósito: fuera de una elección
        // `m_choices` está vacío y `update()` apaga el índice igual.
        void punteroManda() { m_focoDelPuntero = true; }

        // `CP7`: la derivación del hover desde la posición del puntero — los
        // seis botones del chrome y el índice de choice. Vivía suelta en
        // `update()`; sale a método para que `setMousePos` la pueda correr en
        // el acto y el click no lea el índice del batch anterior. Pública no:
        // sus dos callers son de esta clase.
    private:
        void derivarHoverDePuntero();
    public:

        // ── `CP6`: EL ANILLO SUBIO AL STATE, Y ESTO ES SU PROYECCION ─────────
        //
        // *(El renglon de `CP5` decia: «El teclado toma el anillo. Devuelve si
        // consumió — siempre `true` mientras haya opciones», y la firma era
        // `bool moverFoco(FocoDir)`. Se retiro el 2026-08-08.)*
        //
        // **Por que se movio:** los focables de `CP6` -`control`, `ranura`,
        // `enlace`- viven en el state y en `SceneLayer.h`, no en la caja, y en
        // `config.ecl`/`load.ecl`/`pausa.ecl` CONVIVEN con los choices en la
        // misma fase. Un anillo que vive en la caja no puede recorrer una lista
        // cuya mayoria no ve. Asi que el state es dueno del indice unificado y
        // la caja recibe **el suyo ya resuelto**, o `-1` si el foco esta en otra
        // familia.
        //
        // **Lo que NO cambio, y es el punto entero:** `m_choiceHover` conserva su
        // significado exacto y sus TRES consumidores (pintado + juice de escala,
        // el sonido de hover por flanco, el despertador del skip-frame). Esto le
        // escribe el mismo campo que el pull del mouse le escribe — un solo
        // indice, dos fuentes, que es lo que el §9 pide y lo que §7.486 exige.
        // Un `m_choiceFocus` gemelo habria obligado a re-cablear los tres.
        void setFocoChoice(int idx) {
            m_focoDelPuntero = false;
            m_choiceHover = (idx >= 0 && idx < (int)m_choices.size()) ? idx : -1;
        }

        int choiceCount() const { return (int)m_choices.size(); }

        void pushHistory(const std::string& speaker, const std::string& text);
        void clearHistory();
        bool isShowingHistory() const { return m_showingHistory; }

        bool isWaiting() const;
        bool isSkipMode() const { return skipActive(); }
        bool isAutoMode() const { return m_autoMode; }

        // Skip-frame (Perfilado §6): ¿el juice de hover de choices está en reposo?
        // La escala por-opción se ACERCA asintóticamente a su target (nunca exacto,
        // ver update), así que "asentado" = todas a epsilon del reposo (1.0). Si NO,
        // el motor no debe quiescer: congelaría la escala a media animación → dos
        // opciones pegadas en hover (el bug que cazó el feel-test del fundador).
        // Con juice off (choiceHoverScale=1.0) las escalas nunca salen de 1.0 →
        // siempre asentado, costo cero. Sin <cmath> en el header: abs manual.
        bool isChoiceJuiceSettled() const {
            for (float s : m_choiceScale) {
                const float d = s - 1.0f;
                if (d > 0.001f || d < -0.001f) return false;
            }
            return true;
        }
        // Skip-frame (CP2.6): ¿la caja está dibujando un indicador de avance
        // que CAMBIA píxeles? Espejo EXACTO de la reachability del draw
        // (renderDialogBox: early-outs !m_hasFont/m_hidden; la rama del
        // indicador pide typewriter completo o {esperar_click}, sin
        // interrupts, y en autoMode la eclipsa la barra de auto) + los flags
        // del TEMA: la animación del indicador está gated por el tema (§6.4,
        // vaivén y parpadeo default OFF). Un tema con indicador ESTÁTICO
        // (glifo o imagen fijos) puede quiescer; un ctc pulsante estilo-DDLC
        // no debe — correcto por diseño, no una limitación.
        // (El state bloquea auto/skip aparte; el false de autoMode aquí solo
        //  dice "no es el INDICADOR el que anima".)
        // §7.410 / `TextBudget.h` ley 1 — el largo de la línea EN CARACTERES.
        // PUNTO ÚNICO: antes cada sitio escribía `(int)m_displayText.size()`,
        // que son BYTES, y `m_typewriterChars` se comparaba contra eso. En una
        // línea acentuada eso dejaba ticks muertos con el texto ya completo —
        // y el indicador de avance cuelga justo de esta comparación.
        int displayChars() const {
            return (int)eclips::text::countCodepoints(m_displayText);
        }

        // ── LA ENTRADA DE LA CAJA (`caja-entrada`) ──────────────────────────
        //
        // El factor de fundido de ESTE frame: 1.0 = puesta del todo (o sea, lo
        // de siempre). Lo consume `renderDialogBox` abriendo un
        // `GroupAlphaScope`, y `isAppearing()` lo espeja para el skip-frame.
        //
        // `dur <= 0` (la clave no está en el tema) devuelve 1.0 por la primera
        // rama de `evalScalarRamp`, así que un tema que no la nombra dibuja
        // bit-idéntico a antes de que esto existiera.
        //
        // La curva no se inventa: es un `AnimTrack` del canal Alpha, `una_vez`,
        // evaluado con el mismo `evalTrack` que ya usa el parpadeo del
        // indicador treinta líneas más abajo. Un solo álgebra de animación en
        // el motor (§1.40).
        float appearAlpha() const { return m_appear.alpha(); }

        // Skip-frame: ¿la caja está ENTRANDO? Mientras el alpha no llegó a 1,
        // cada frame cambia píxeles y el motor NO puede quiescer.
        //
        // No es una precaución teórica: `VNScriptState::isQuiescent` frena por
        // typewriter, indicador y efectos de texto — ninguno de los tres cubre
        // esto. Una línea de tres letras termina de escribirse antes de que la
        // caja termine de entrar, y un `mostrar_caja` sin diálogo nuevo no
        // enciende NINGUNO de los tres. Sin esta consulta, la entrada se
        // congela a medio camino y se ve como "a veces no funciona".
        //
        // Espeja la alcanzabilidad del draw (los mismos early-outs que
        // `renderDialogBox`), igual que `isIndicatorAnimating`.
        bool isAppearing() const {
            if (!m_hasFont || m_hidden) return false;   // la caja no dibuja
            return m_appear.isAppearing();
        }

        bool isIndicatorAnimating() const {
            if (!m_hasFont || m_hidden) return false;   // la caja no dibuja
            if (m_autoMode) return false;               // barra de auto, no indicador
            if (m_lineInterrupts) return false;
            if (m_typewriterChars < displayChars() && !m_waitingMidClick)
                return false;                           // aún revelando (no se dibuja)
            const DialogBoxStyle& d = activeStyle();
            return d.advanceIndicatorNudge || d.advanceIndicatorBlink;
        }

        // Skip-frame (CP2.6): ¿el texto visible lleva efectos per-char ANIMADOS?
        // {ondulado}/{temblar} leen m_totalTime en el draw → una línea completada
        // sigue cambiando píxeles PARA SIEMPRE; quiescerla congelaría la onda a
        // media animación. OJO (§7.282): m_charEffects mezcla especies — color/
        // fuente/tam/velocidad/pausa son ESTÁTICOS y no cuentan; solo Wavy/Shake
        // animan. Tramos aún no revelados dan igual: mid-reveal isWaiting() ya
        // bloquea. (Único call-site rich: el cuerpo de la caja — capas de texto
        // y choices dibujan plano, auditado CP2.6.)
        bool hasAnimatedTextFX() const {
            if (m_hidden || !m_hasFont) return false;   // no se dibuja nada
            for (const auto& ce : m_charEffects)
                if (ce.fx != TextFX::None) return true;
            return false;
        }
        int  selectedChoice() const { return m_selectedChoice; }
        bool qteSucceeded() const { return m_qte.exito(); }

        // CP4 audio: el state lee estos y dispara playSound en el flanco de
        // hover / al click. La caja nunca toca audio (renderer puro).
        int  currentChoiceHover() const { return m_choiceHover; }
        const std::string& activeChoiceHoverSound() const { return activeStyle().choiceBtnHoverSound; }
        const std::string& activeChoiceClickSound() const { return activeStyle().choiceBtnClickSound; }

        // El state lee el estilo activo para chrome no-caja (la tarjeta de ranura
        // vive en VNScriptState). Passthrough a activeStyle() (priv): misma cadena
        // usar_caja > personaje.caja > default que los choices → un solo origen de
        // estilo (§1.40), sin duplicar la resolución.
        const DialogBoxStyle& currentStyle() const { return activeStyle(); }

    private:

        // Dibuja SOLO la placa del nombre (sprite/9-slice/primitivas), no el
        // texto. Contrato de batch: entra según batchOpen; SALE con el batch
        // ABIERTO (lo que el resto de renderDialogBox espera). Extraído para
        // permitir z-order configurable (placa antes o después de la caja).
        void drawNamePlate(const DialogBoxStyle& d,
            float nX, float nY, float nW, float nH,
            bool batchOpen);

        // drawButtonBg vive ahora en draw2d::drawButtonBg (Draw2D.h) — primitivo
        // asset-free compartido (CP2 · A2). Se llama calificado desde aqui.

        // Geometría de botones UI (usa m_theme)
        void getDialogBtnRects(std::vector<UIBtnRect>& out, float VW, float VH) const;
        void getChoiceRects(std::vector<UIBtnRect>& out, float VW, float VH) const;

        // Cadena de prioridad: usar_caja > personaje.caja > theme.defaultDialogBox
        const DialogBoxStyle& activeStyle() const;

        // Resuelve el TextRenderer a usar en cuerpo/nameplate según la
        // caja activa (caja_fuente). Si la caja no declara fontName o el
        // FontManager no la tiene cargada, devuelve `fallback` (la fuente
        // base del juego, que el caller pasa como m_text o m_name).
        // Aviso pedagógico 1× por nombre faltante (§1.31).
        TextRenderer* resolveBoxFont(TextRenderer* fallback) const;

        // Resuelve el TextRenderer SOLO de la placa de nombre (C-NOMBRE).
        // Precedencia: nombre_fuente > caja_fuente > fallback. Si la caja no
        // declara nameFontName (o no carga), delega en resolveBoxFont — así la
        // placa hereda caja_fuente cuando no diverge (compat). Aviso 1× propio.
        TextRenderer* resolveNameFont(TextRenderer* fallback) const;

        // Resuelve el TextRenderer de los botones de elegir (C-FUENTE-CHROME).
        // Precedencia: opciones_fuente > fallback (base). A diferencia de
        // resolveNameFont, NO delega en resolveBoxFont: los choices nunca
        // siguieron caja_fuente (usaban m_text crudo) → caer directo a base
        // preserva el comportamiento previo. Aviso 1× propio.
        TextRenderer* resolveChoiceFont(TextRenderer* fallback) const;

        // Resuelve el TextRenderer de QTE (tecla + hint) (C-FUENTE-CHROME).
        // Misma forma que resolveChoiceFont: qte_fuente > fallback, sin pasar
        // por caja_fuente. Aviso 1× propio.
        TextRenderer* resolveQteFont(TextRenderer* fallback) const;

        // Resuelve el TextRenderer del bottom-bar (boton_fuente). Misma forma
        // que resolveChoiceFont: boton_fuente > fallback, sin pasar por
        // caja_fuente (el chrome siempre usó m_name crudo). Aviso 1× propio.
        TextRenderer* resolveChromeFont(TextRenderer* fallback) const;

        // FASE 2 (tamaño per-caja). Filtro de TAMAÑO sobre la fuente YA resuelta
        // por resolveBoxFont/resolveNameFont. Compone con caja_fuente (Fork 3a):
        // reescala el ttf que la fuente resuelta trajo, no el baseline. size<=0
        // = sentinela ausente → la fuente resuelta tal cual; tamaño ya igual →
        // no-op; si no → atlas anónimo vía m_fm->getSized.
        TextRenderer* sizedFont(TextRenderer* resolved, f32 size) const;

        const Theme* m_theme;  // garantizado no-nullptr (ver constructor)

        // Theme fallback compartido con valores Theme::defaults().
        // Se usa cuando nadie llama setTheme() o cuando alguien lo setea a nullptr.
        // Patrón Null Object: elimina la posibilidad de UB sin null-checks dispersos.
        static const Theme& fallbackTheme();

        // Personaje activo del diálogo en curso. nullptr = sin info de personaje
        // (narrador, o no hay manifest cargado). El helper de cadena trata
        // nullptr como "saltar niveles 2 y 4" → fallback a theme + engine.
        const PersonajeManifest* m_activeSpeaker = nullptr;

        // AssetManager para resolver paths de sprites de caja. nullptr = sin
        // AssetManager configurado (caso onEnter sin setAssetManager, o state
        // de testing). Render fallea silenciosamente a primitivas. La instancia
        // es propiedad de VNScriptState (u otro estado dueño del lifecycle).
        AssetManager* m_assets = nullptr;

        // Override de línea capturado desde RichText.lineSpeedOverride en
        // startDialogue. Ausencia separada: no hay float centinela.
        std::optional<TextSpeed> m_lineSpeedOverride;
        // Interrupción de línea capturada desde RichText.interrupts en
        // startDialogue. Si true, al completar el typewriter el diálogo salta
        // solo (pisa el modo auto) tras m_theme->interruptDelay segundos. El
        // timer cuenta DESDE que el texto completa (igual que m_autoTimer), por
        // eso funciona con velocidad instantánea: el respiro fijo garantiza que
        // el texto se vea antes del salto. Ambos se resetean en startDialogue.
        bool  m_lineInterrupts = false;
        float m_interruptTimer = 0.0f;
        float m_virtualW = 1920.0f;
        float m_virtualH = 1080.0f;
        TextRenderer* m_text = nullptr;
        TextRenderer* m_name = nullptr;
        FontManager* m_fm = nullptr; // fuente inline: resuelve {fuente=X} en render
        bool m_hasFont = false;
        bool m_hasNameFont = false;
        // Caja de diálogo oculta por comando ocultar_caja. Solo afecta render
        // de la caja de diálogo (no choices, no QTE). Se resetea automáticamente
        // en startDialogue para que un nuevo diálogo siempre sea visible.
        bool m_hidden = false;
        // `ELEGIR-CON-CAJA`: el pedido explícito del guion de que la caja
        // sobreviva a la elección. Acompaña a `m_hidden`, no lo reemplaza:
        // `m_hidden` contesta "se ve ahora", éste "sobrevive a lo que viene".
        BoxPersistence m_persist;
        std::string m_overrideBoxName; // usar_caja mid-escena ("" = sin override)

        Phase m_phase = Phase::Dialogue;
        bool m_pendingAdvance = false;
        bool m_pendingChoice = false;
        Event m_pendingUIEvent = Event::None;
        float m_mouseVX = 0.0f, m_mouseVY = 0.0f;

        // Diálogo
        std::string m_displayText;
        std::vector<CharEffect> m_charEffects;
        std::string m_currentSpeaker;
        int m_typewriterChars = 0;
        float m_typewriterTimer = 0.0f;
        // Pausa intra-typewriter: cuando un char tiene pauseAfter > 0 o
        // waitClickAfter true, el typewriter se detiene momentáneamente.
        float m_pauseTimer = 0.0f;       // Segundos restantes de pausa por {pausa=N}
        bool  m_waitingMidClick = false; // true mientras espera click por {esperar_click}
        bool m_autoMode = false;
        bool m_skipHeld = false;     // Ctrl mantenido (crudo, de setSkipHeld)
        bool m_skipToggle = false;   // toggle del botón (persistente)
        bool m_skipAllowed = true;   // gate de VNScriptState (visto + !choice)

        // El sello de acciones que el guion escribió, empujado por el state
        // cada frame. 0 = nada sellado, y ésa es la ley 1 (`D-40`).
        eclips::VNActionMask m_accionesSelladas = 0u;
        float m_autoTimer = 0.0f;
        float m_skipTimer = 0.0f;
        int m_uiBtnHover = -1;
        int m_choiceHover = -1;   // CP4 átomo: índice de choice bajo el mouse (-1 = ninguno)
        // `CP5`: quién es dueño de `m_choiceHover` ahora mismo. Arranca en
        // `true` -y `startChoices` lo devuelve a `true`- para que cada elección
        // nazca con la conducta de siempre: el cursor parado sobre una opción
        // la resalta sin que nadie mueva nada.
        bool m_focoDelPuntero = true;

        // ── EL DISPARADOR DE LA ENTRADA (`caja-entrada`) ────────────────────
        //
        // La caja funde cuando el jugador NO la estaba viendo. Nada más. No
        // pregunta por la fase del VN, y esa es toda la gracia:
        //
        // Entre dos líneas seguidas la fase SÍ sale de `WaitingInput` (el
        // `Advance` la manda a `Executing` y el comando siguiente la devuelve),
        // pero las dos cosas pasan DENTRO del mismo `update()` — el batch de
        // comandos instantáneos vive ahí mismo (`VNScriptState.h:1871`), y
        // `render()` es otro método: no puede meterse en el medio. O sea que un
        // disparador colgado de la FASE fundiría en CADA línea, y uno colgado de
        // "¿me dibujaron?" no.
        //
        // Medido sobre el corpus vivo al abrir el lote (134 `.ecl`, 1155 líneas
        // de diálogo): 232 apariciones de verdad contra 923 líneas seguidas. Por
        // la fase serían 1155 — CINCO VECES de más.
        //
        // Ojo con el orden, que es lo que lo hace correcto: el render del frame
        // anterior ya dejó su marca, y `frameBegin()` la rotó al empezar ESTE
        // frame — así que cuando el `Advance` llega a `startDialogue`, lo que
        // lee es el último frame que el jugador tiene en la pantalla.
        //
        // ── DÓNDE VIVE ESTO AHORA (lote 2) ──────────────────────────────────
        //
        // El párrafo de arriba explica POR QUÉ el disparador cuelga de «¿me
        // dibujaron?» y no de la fase, y esa razón es local: depende de que el
        // batch de comandos instantáneos de `VNScriptState` corra entero dentro
        // de un `update()`. **El MECANISMO, en cambio, ya no vive acá.**
        //
        // Los tres campos (`m_drawnThisFrame`, `m_drawnLastFrame`,
        // `m_appearTimer`) y `entrarSiNoMeVeias()` se mudaron a
        // `engine/render/AppearPolicy.h` por `RO-5`: adentro de esta clase la
        // ley NO SE PUEDE PROBAR —la TU arrastra GL— y allá `harness_aparicion`
        // la custodia con 46 asserts, incluida la de §7.418 que hasta ese lote
        // no custodiaba nadie.
        //
        // Quien rota los flags sigue siendo el RENDER (`frameBegin()`), y quien
        // prende «me dibujaron» sigue siendo `renderDialogBox` DESPUÉS de sus
        // early-outs, para que "oculta por `ocultar_caja`" y "sin fuente"
        // cuenten como no-vista sin una línea extra. Sólo que ahora eso se
        // llama `m_appear.rendered()`.
        //
        // Esta clase no conserva una copia de ninguna de las siete leyes:
        // delega. Lo único que queda acá es lo que el header no puede saber —
        // de qué estilo sale la política, y cuándo la caja no se dibuja.
        AppearState m_appear;

        // Skip efectivo = hay intención (Ctrl mantenido O toggle del botón) Y
        // está permitido en este contexto (texto ya visto / skipUnseen, y no en
        // un choice). El gate m_skipAllowed lo computa VNScriptState (tiene la
        // info de "visto" y de fase). Held y toggle se gatean juntos aquí.
        bool skipActive() const {
            return (m_skipHeld || m_skipToggle) && m_skipAllowed;
        }

        // Choices
        std::vector<EclipChoice> m_choices;
        // Juice de hover (F2a): escala animada por-opción (1.0 = reposo).
        // Paralela a m_choices: sembrada en startChoices, tickeada en update(),
        // leída en renderChoices() (CP2). Es offset de RENDER, no geometría —
        // getChoiceRects sigue siendo la fuente única (§1.40) y el hit-test usa
        // el rect de reposo.
        std::vector<float> m_choiceScale;
        float m_choiceTimer = 0.0f;
        float m_choiceTimerMax = 0.0f;
        int m_choiceDefault = -1;
        int m_selectedChoice = -1;

        // QTE
        float m_qteTimer = 0.0f;
        float m_qteTimerMax = 0.0f;
        uint32_t m_qteKeycode = 0;
        std::string m_qteKeyName;
        std::string m_qteSuccessScene;
        std::string m_qteFailScene;

        // `QTE-SIN-PUERTA`: el veredicto y su única puerta de salida. Reemplaza
        // al `bool m_qteResult` que vivía acá — ese bool sabía QUÉ pasó y no
        // sabía si ya se había anunciado, que es exactamente el eslabón 3: un
        // acierto ponía el reloj en cero y el evento colgaba de que el reloj
        // CRUZARA cero, así que no salía nunca y la fase quedaba trabada.
        eclips::QteResolucion m_qte;

        // Historial
        struct HistoryEntry { std::string speaker, text; };
        std::deque<HistoryEntry> m_history;
        bool m_showingHistory = false;
        float m_historyScroll = 0.0f;
        float m_historyScrollTarget = 0.0f;

        // Toast
        std::string m_toastMsg;
        float m_toastTimer = 0.0f;

        float m_totalTime = 0.0f;
    };

} // namespace eclips
