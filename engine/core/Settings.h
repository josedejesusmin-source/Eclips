// ============================================================================
// Settings.h — Preferencias globales de Eclips Engine
// ============================================================================
//
// Clase estática que almacena las preferencias del jugador.
// Cualquier sistema puede leer los valores con Settings::textSpeed(), etc.
// Se persisten en "saves/settings.eclips" vía SaveManager.
//
// ── Patrón "Theme propone, Settings sobreescribe" ──────────────────────────
//
// Para los campos de comportamiento (textSpeed, autoDelay, skipInterval),
// los getters aceptan un fallback opcional. Si el jugador NO ha tocado el
// valor, se devuelve el fallback (típicamente del Theme activo). Si el
// jugador SÍ lo tocó (o cargó settings con override), gana su valor.
//
//   // Desde el motor (con tema activo):
//   float delay = Settings::autoDelay(m_theme->autoDelay);
//
//   // Desde el menú de Settings (sin theme):
//   float delay = Settings::autoDelay();   // usa default global
//
// Los demás campos (audio, fullscreen) son preferencias puras del jugador
// y no aplican el patrón.
//
// ── Uso ────────────────────────────────────────────────────────────────────
//
//   // Al iniciar el motor (main.cpp):
//   Settings::load();
//
//   // Escribir desde SettingsState:
//   Settings::setMusicVolume(0.5f);
//   Settings::save();
//
// ============================================================================

#pragma once

#include "engine/core/SaveManager.h"
#include "engine/core/TextSpeed.h"
#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace eclips
{

    class Settings
    {
    public:

        // ── Text (patrón Theme propone, Settings sobreescribe) ────────────

        static TextSpeed textSpeed(const TextSpeed& fallback = textSpeedDefault()) {
            return s_textSpeedOverride ? *s_textSpeedOverride : fallback;
        }
        static const std::optional<TextSpeed>& textSpeedOverride() {
            return s_textSpeedOverride;
        }
        static TextSpeed projectTextSpeedForDomain(const TextSpeedDomain& domain)
        {
            if (!s_textSpeedOverride)
            {
                s_textSpeedProjectionVoiceKey.clear();
                return textSpeedDefault();
            }

            const TextSpeedProjection projection =
                projectTextSpeedToDomain(*s_textSpeedOverride, domain);
            if (!projection.projected())
            {
                s_textSpeedProjectionVoiceKey.clear();
                return projection.effective;
            }

            std::string key = textSpeedDescribe(projection.stored) + "|" +
                std::to_string(static_cast<int>(domain.kind)) + "|" +
                std::to_string(domain.min) + "|" + std::to_string(domain.max) + "|" +
                std::to_string(domain.step) + "|" +
                (domain.includeInstant ? "instant" : "sin-instant");
            if (key != s_textSpeedProjectionVoiceKey)
            {
                std::cerr << "[Settings] Aviso: override guardado "
                    << textSpeedDescribe(projection.stored)
                    << " queda fuera del dominio del theme ("
                    << textSpeedFormatCps(domain.min) << ".."
                    << textSpeedFormatCps(domain.max)
                    << (domain.includeInstant ? ", con instantaneo" : ", sin instantaneo")
                    << "); efectivo " << textSpeedDescribe(projection.effective)
                    << ". Se conserva el override guardado y revive si el dominio vuelve a admitirlo."
                    << std::endl;
                s_textSpeedProjectionVoiceKey = key;
            }
            return projection.effective;
        }
        static float autoDelay(float fallback = 2.0f) {
            return s_autoDelayUserSet ? s_autoDelay : fallback;
        }
        static float skipInterval(float fallback = 0.05f) {
            return s_skipIntervalUserSet ? s_skipInterval : fallback;
        }
        static bool skipUnseen(bool fallback = false) {
            return s_skipUnseenUserSet ? s_skipUnseen : fallback;
        }

        // ── Idioma del jugador (DISENO_IDIOMAS CP1) ──────────────────────
        // `Settings::load()` corre antes de que Engine lea el manifest, así
        // que acá sólo se transporta la preferencia cruda. Engine valida el
        // código contra el catálogo y decide el efectivo cuando posee ambos.
        static std::string language(const std::string& fallback = "") {
            return s_langUserSet ? s_lang : fallback;
        }
        static const std::string& storedLanguage() { return s_lang; }
        static bool languageUserSet() { return s_langUserSet; }
        static void setLanguage(const std::string& codigo) {
            s_lang = codigo;
            s_langUserSet = true;
        }
        static void clearLanguageOverride() {
            s_lang.clear();
            s_langUserSet = false;
        }

        // Setters — marcan user-set = true automáticamente
        static void setTextSpeed(const TextSpeed& v)
        {
            s_textSpeedOverride = v;
            s_textSpeedProjectionVoiceKey.clear();
        }
        static void setTextSpeed(float v)
        {
            const auto parsed = TextSpeed::tryCps(v);
            if (!parsed)
            {
                std::cerr << "[Settings] Aviso: CPS invalido. Se conserva la velocidad anterior."
                    << std::endl;
                return;
            }
            setTextSpeed(*parsed);
        }
        static void setAutoDelay(float v) { s_autoDelay = v;    s_autoDelayUserSet = true; }
        static void setSkipInterval(float v) { s_skipInterval = v; s_skipIntervalUserSet = true; }
        static void setSkipUnseen(bool v) { s_skipUnseen = v; s_skipUnseenUserSet = true; }

        // ── Clear de overrides (volver al theme) ──────────────────────────

        static void clearTextSpeedOverride()
        {
            s_textSpeedOverride.reset();
            s_textSpeedProjectionVoiceKey.clear();
        }
        static void clearAutoDelayOverride() { s_autoDelayUserSet = false; }
        static void clearSkipIntervalOverride() { s_skipIntervalUserSet = false; }
        static void clearSkipUnseenOverride() { s_skipUnseenUserSet = false; }

        // Helper: limpiar todos los overrides del patrón de una sola vez.
        // El botón "Restaurar valores del juego" del menú llama aquí.
        static void restoreThemeDefaults()
        {
            s_textSpeedOverride.reset();
            s_textSpeedProjectionVoiceKey.clear();
            s_autoDelayUserSet = false;
            s_skipIntervalUserSet = false;
            s_skipUnseenUserSet = false;
        }

        // Query: ¿hay algún override activo?
        // El menú lo usa para decidir si mostrar el botón "Restaurar".
        static bool hasAnyOverride()
        {
            return s_textSpeedOverride.has_value()
                || s_autoDelayUserSet
                || s_skipIntervalUserSet
                || s_skipUnseenUserSet;
        }

        // ── Audio (preferencia pura del jugador) ──────────────────────────

        static float musicVolume() { return s_musicVol; }
        static float sfxVolume() { return s_sfxVol; }

        static void setMusicVolume(float v) { s_musicVol = v; }
        static void setSfxVolume(float v) { s_sfxVol = v; }

        // ── Display (preferencia pura del jugador) ────────────────────────

        static bool fullscreen() { return s_fullscreen; }
        static void setFullscreen(bool v) { s_fullscreen = v; }

        // ── Window size (preferencia pura del jugador) ────────────────────
        // Patrón override por una razón distinta a textSpeed: el "default" de
        // tamaño NO se conoce en compile-time (es el tamaño con que el motor
        // abrió la ventana). El flag distingue "nunca tocado → usa el boot"
        // de "el jugador eligió → usa lo guardado". NO es theme-overridable
        // (no entra en hasAnyOverride/restoreThemeDefaults).
        static int  windowWidth(int fallback) { return s_windowUserSet ? s_windowW : fallback; }
        static int  windowHeight(int fallback) { return s_windowUserSet ? s_windowH : fallback; }
        static bool windowSizeUserSet() { return s_windowUserSet; }
        static int  storedWindowWidth() { return s_windowW; }
        static int  storedWindowHeight() { return s_windowH; }

        static void setWindowSize(int w, int h) { s_windowW = w; s_windowH = h; s_windowUserSet = true; }
        static void clearWindowSizeOverride() { s_windowUserSet = false; }

        // ── Teclas del jugador (átomo de teclado, CP4) ────────────────────
        //
        // `tecla.<dominio>.<accion>=<lista>`. Se guardan **CRUDAS**: este
        // archivo transporta texto y no interpreta una letra — quién es un
        // dominio, qué es un verbo y qué nombres de tecla existen lo decide
        // `inputParsearCapaJugador` (`engine/input/InputManager.h`), que es
        // donde vive la ley y donde la batería la alcanza. Mismo reparto que
        // `ProjectManifest::TeclaFila` para la capa del creador.
        //
        // ── POR QUÉ `Settings` LAS ADOPTA EN VEZ DE IGNORARLAS ────────────
        //
        // Porque `save()` escribe un **snapshot fresco**: arma un `SaveManager`
        // vacío, le mete lo que conoce y escribe. Toda clave que no conozca
        // **desaparece**. Antes de este lote eso era inofensivo (no había
        // claves ajenas); en cuanto el jugador escribe una tecla a mano, la
        // pierde en el próximo guardado — y `save()` corre al cerrar el menú de
        // config, al cerrar la app en rol Menu, y hasta al reparar el tamaño de
        // ventana en el boot. O sea que se perdía **antes de jugarla**.
        //
        // La alternativa era que `save()` hiciera merge con el disco, y eso
        // rompe algo que ya funciona: "Restaurar valores del juego" persiste
        // **por ausencia** (limpia el flag y la clave deja de escribirse). Con
        // merge, la clave volvería del archivo viejo y el restaurar se vería
        // bien en pantalla y volvería en el próximo boot.
        //
        // Se guarda el TEXTO tal cual entró, incluso si está mal escrito: el
        // aviso le dice al jugador qué corregir **y la línea sigue ahí** para
        // que la corrija. Borrársela sería peor que no avisarle.
        static const std::vector<std::pair<std::string, std::string>>& teclas()
        {
            return s_teclas;
        }

        // ── CP9b: el otro lado, para el `control tecla.*` ────────────────────
        //
        // No hay `setTecla(clave, valor)` a proposito: **quien decide QUE linea
        // se toca es `inputRebindJugador`** (`InputManager.h`), porque esa
        // decision necesita saber que `key.game.jump` y `tecla.juego.saltar` son
        // la misma fila. Un setter por-clave invitaria a que el call-site
        // re-derivara esa ley, que es como se fabrica la segunda tabla de
        // §7.442. Aca entra la lista YA resuelta y este archivo no la
        // interpreta - sigue siendo el transporte que `CP4` firmo.
        //
        // No persiste: `Settings::save()` corre en `onExit` con rol Menu
        // (`VNScriptState.h:462`), que es el `+ persist` que `G-6` difirio a
        // este CP y `L-4` encontro ya construido.
        static void setTeclas(const std::vector<std::pair<std::string, std::string>>& t)
        {
            s_teclas = t;
        }

        // ── Defaults del motor (no editables por el jugador) ──────────────

        static const char* defaultFontPath() { return "third_party/fonts/nunito/Nunito-Regular.ttf"; }

        // Sonido de interaccion del motor (E-28 / F4.2a). Nombre y carpeta propios;
        // no reusa la ruta del efecto del lab bajo assets/test/.
        static const char* defaultInteractSoundPath() { return "assets/motor/interaccion.wav"; }

        // Distinct feedback for dialogue, dash and surviving melee hits.
        static const char* defaultDialogueSoundPath() { return "assets/motor/dialogo.wav"; }
        static const char* defaultDashSoundPath() { return "assets/motor/dash.wav"; }
        static const char* defaultEnemyHitSoundPath() { return "assets/motor/golpe_enemigo.wav"; }

        // ── Costura de test (RO-5 del vocabulario: la ley tiene que ser
        //    probable fuera del motor) ──────────────────────────────────────
        //
        // Redirige el archivo. Es la misma costura que `SeenStore` y
        // `MetaStore` ya tienen (`m_path.empty() ? SEEN_FILE : m_path`), y
        // existe por una razón concreta: sin ella, el harness que prueba que
        // la adopción de `tecla.*` funciona tendría que escribir sobre
        // `saves/settings.eclips` **de verdad**, o sea sobre el archivo del
        // jugador. Vacío = el de siempre.
        static void setFilePathForTest(const std::string& p) { s_path = p; }

        // ── Persistence ───────────────────────────────────────────────────

        static void load()
        {
            SaveManager sm;
            // Una carga sin key no puede revivir el override de una carga
            // anterior en el mismo proceso.
            s_textSpeedOverride.reset();
            s_textSpeedProjectionVoiceKey.clear();
            s_teclas.clear();
            s_lang.clear();
            s_langUserSet = false;
            if (!sm.readFromFile(rutaArchivo()))
            {
                std::cout << "[Settings] No settings file found, using defaults."
                    << std::endl;
                return;
            }

            // Las teclas del jugador, crudas y en orden estable. Los dos
            // prefijos son la misma familia (`tecla` es palabra cotidiana y se
            // traduce), así que se juntan y se ordenan **como una sola**: el
            // orden es alfabético sobre la clave entera, que es exactamente el
            // orden en que están escritas en el archivo (`writeToFile` ordena
            // al escribir). Así los avisos salen en el mismo orden en que el
            // jugador ve sus líneas al abrirlo.
            //
            // De esto depende una LEY, no la prolijidad: si la misma acción
            // viene escrita en los dos idiomas, el parser avisa y **se queda
            // con la última**. Sobre un orden no especificado eso sería un
            // sorteo distinto en cada arranque.
            for (const char* pref : { "tecla.", "key." })
                for (const std::string& k : sm.keysWithPrefix(pref))
                    s_teclas.emplace_back(k, sm.getString(k));

            std::sort(s_teclas.begin(), s_teclas.end(),
                [](const std::pair<std::string, std::string>& a,
                   const std::pair<std::string, std::string>& b)
                { return a.first < b.first; });

            // `settings_version` versiona el esquema de preferencias. Es un
            // piso distinto del header `[Eclips Settings v2]` de SaveManager.
            const bool hasSettingsVersion = sm.hasKey("settings_version");
            int settingsVersion = 0;
            const bool settingsV2 = hasSettingsVersion &&
                parseIntStrict(sm.getString("settings_version"), settingsVersion) == NumParse::Ok &&
                settingsVersion == 2;

            if (hasSettingsVersion && !settingsV2)
            {
                std::cerr << "[Settings] Aviso: settings_version='"
                    << sm.getString("settings_version")
                    << "' desconocida; se ignora el override de velocidad."
                    << std::endl;
            }

            if (!hasSettingsVersion)
            {
                // Legacy: solo `text_speed` tiene significado. Las keys v2
                // huerfanas hablan y no abren una segunda ley.
                for (const char* key : { "text_speed_mode", "text_speed_cps" })
                    if (sm.hasKey(key))
                        std::cerr << "[Settings] Aviso: key v2 huerfana '" << key
                        << "'; se ignora porque falta settings_version=2."
                        << std::endl;

                if (sm.hasKey("text_speed"))
                {
                    const std::string raw = sm.getString("text_speed");
                    const auto legacy = parseLegacyTextSpeed(raw);
                    if (legacy)
                        s_textSpeedOverride = *legacy;
                    else
                        std::cerr << "[Settings] Aviso: text_speed legacy invalido '"
                        << raw << "'; se ignora el override." << std::endl;
                }
            }
            else if (settingsV2)
            {
                const bool hasMode = sm.hasKey("text_speed_mode");
                const bool hasCps = sm.hasKey("text_speed_cps");
                if (hasMode || hasCps)
                {
                    const std::string mode = sm.getString("text_speed_mode");
                    if (mode == "instant" && hasMode)
                    {
                        s_textSpeedOverride = TextSpeed::instantaneo();
                        if (hasCps)
                            std::cerr << "[Settings] Aviso: text_speed_cps sobrante "
                            << "con mode=instant; se ignora." << std::endl;
                    }
                    else if (mode == "cps" && hasMode && hasCps)
                    {
                        const auto cps = parseTextSpeedCps(sm.getString("text_speed_cps"));
                        if (cps)
                            s_textSpeedOverride = *cps;
                        else
                            std::cerr << "[Settings] Aviso: text_speed_cps invalido '"
                            << sm.getString("text_speed_cps")
                            << "'; se ignora el override." << std::endl;
                    }
                    else
                    {
                        std::cerr << "[Settings] Aviso: familia v2 de velocidad incompleta "
                            << "o desconocida; se ignora el override." << std::endl;
                    }
                }
            }
            if (sm.hasKey("auto_delay")) { s_autoDelay = sm.getFloat("auto_delay", 2.0f); s_autoDelayUserSet = true; }
            if (sm.hasKey("skip_interval")) { s_skipInterval = sm.getFloat("skip_interval", 0.05f); s_skipIntervalUserSet = true; }
            if (sm.hasKey("skip_unseen")) { s_skipUnseen = sm.getBool("skip_unseen", false); s_skipUnseenUserSet = true; }
            if (sm.hasKey("lang")) { s_lang = sm.getString("lang"); s_langUserSet = true; }
            if (sm.hasKey("window_w") && sm.hasKey("window_h")) {
                s_windowW = sm.getInt("window_w", 1280);
                s_windowH = sm.getInt("window_h", 720);
                s_windowUserSet = true;
            }

            // Campos sin patrón: cargar siempre (con default si no existe).
            s_musicVol = sm.getFloat("music_vol", 0.7f);
            s_sfxVol = sm.getFloat("sfx_vol", 1.0f);
            s_fullscreen = sm.getBool("fullscreen", false);

            std::cout << "[Settings] Loaded:"
                << " textSpeed=" << (s_textSpeedOverride
                    ? (s_textSpeedOverride->isInstantaneo()
                        ? std::string("Instantaneo")
                        : textSpeedFormatCps(s_textSpeedOverride->cps()))
                    : std::string("<theme>"))
                << " autoDelay=" << s_autoDelay << (s_autoDelayUserSet ? "*" : "")
                << " skipInterval=" << s_skipInterval << (s_skipIntervalUserSet ? "*" : "")
                << " skipUnseen=" << s_skipUnseen << (s_skipUnseenUserSet ? "*" : "")
                << " lang=" << (s_langUserSet ? s_lang : "<manifest>")
                << " music=" << s_musicVol
                << " sfx=" << s_sfxVol
                << " fullscreen=" << s_fullscreen
                << "  (* = override del jugador)"
                << std::endl;
        }

        static void save()
        {
            SaveManager sm;

            // El esquema de preferencias es v2. Nunca vuelve a escribir la
            // key legacy `text_speed`.
            sm.setString("settings_version", "2");
            if (s_textSpeedOverride)
            {
                if (s_textSpeedOverride->isInstantaneo())
                    sm.setString("text_speed_mode", "instant");
                else
                {
                    sm.setString("text_speed_mode", "cps");
                    sm.setString("text_speed_cps",
                        textSpeedFormatCps(s_textSpeedOverride->cps()));
                }
            }
            if (s_autoDelayUserSet)    sm.setFloat("auto_delay", s_autoDelay);
            if (s_skipIntervalUserSet) sm.setFloat("skip_interval", s_skipInterval);
            if (s_skipUnseenUserSet)   sm.setBool("skip_unseen", s_skipUnseen);
            if (s_langUserSet)         sm.setString("lang", s_lang);
            if (s_windowUserSet) {
                sm.setInt("window_w", s_windowW);
                sm.setInt("window_h", s_windowH);
            }

            // Campos sin patrón: escribir siempre.
            sm.setFloat("music_vol", s_musicVol);
            sm.setFloat("sfx_vol", s_sfxVol);
            sm.setBool("fullscreen", s_fullscreen);

            // Las teclas del jugador, tal cual entraron. Sin esto el snapshot
            // fresco se las lleva — ver el comentario largo en `teclas()`.
            for (const std::pair<std::string, std::string>& t : s_teclas)
                sm.setString(t.first, t.second);

            sm.writeToFile(rutaArchivo(), "Settings");
            std::cout << "[Settings] Saved." << std::endl;
        }

    private:

        static constexpr const char* SETTINGS_FILE = "saves/settings.eclips";

        // Vacío = el de siempre. Mismo patrón que `SeenStore`/`MetaStore`.
        static inline std::string s_path;
        static std::string rutaArchivo()
        {
            return s_path.empty() ? std::string(SETTINGS_FILE) : s_path;
        }

        // Las teclas del jugador, crudas. NO entran en `hasAnyOverride` ni en
        // `restoreThemeDefaults`, y no es un caso especial: es el segundo
        // miembro de una categoría que este archivo ya tenía, la del tamaño de
        // ventana. El tema no propone teclas, así que no hay override que
        // restaurar. Un "Restaurar controles" es otra cosa y tiene dueño: la
        // pantalla de `CP9`.
        //
        // > ⚠ **`CP9` shipeó (2026-08-12) y NO lo trajo.** La pantalla remapea y
        // > vacía; **restaurar sigue sin existir**, y se difirió por alcance con
        // > el fundador enterado, no por olvido. Ficha
        // > `RESTABLECER-TECLAS-SIN-COMANDO` en `HORIZONTES`, con su diseño ya
        // > medido — y con la parte que sólo se ve desde acá: **"predeterminado"
        // > NO es "de fábrica"**. Restaurar tiene que **borrar la línea del
        // > jugador** para que el verbo caiga a lo que el CREADOR puso; escribir
        // > los valores de fábrica haría que la pantalla de opciones le pise el
        // > remapeo a su propio proyecto.
        static inline std::vector<std::pair<std::string, std::string>> s_teclas;

        // ── Defaults ──────────────────────────────────────────────────────
        static inline float s_autoDelay = 2.0f;    // segundos
        static inline float s_skipInterval = 0.05f;   // segundos entre chars en skip
        static inline bool  s_skipUnseen = false;      // ¿skip atraviesa texto no visto?
        static inline std::string s_lang;
        static inline float s_musicVol = 0.7f;
        static inline float s_sfxVol = 1.0f;
        static inline bool  s_fullscreen = false;
        static inline int   s_windowW = 1280;
        static inline int   s_windowH = 720;
        static inline bool  s_windowUserSet = false;

        // Flags "user-set" para el patrón override.
        // false = nunca tocado, getter usa el fallback (Theme).
        // true  = el jugador lo cambió, getter devuelve el valor del jugador.
        static inline std::optional<TextSpeed> s_textSpeedOverride;
        static inline std::string s_textSpeedProjectionVoiceKey;
        static inline bool s_autoDelayUserSet = false;
        static inline bool s_skipIntervalUserSet = false;
        static inline bool s_skipUnseenUserSet = false;
        static inline bool s_langUserSet = false;
    };

} // namespace eclips
