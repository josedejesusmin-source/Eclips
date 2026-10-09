// ============================================================================
// VNSaveMenu.h — Menú de guardado/carga universal (VN + Gameplay 2D)
// ============================================================================
//
// Overlay transparente que se apila encima de VNScriptState o Gameplay2DState.
// No para el juego por debajo — la escena sigue renderizando de fondo.
//
// ── Controles ────────────────────────────────────────────────────────────────
//
//   ↑ / ↓ / W / S    → mover cursor entre slots
//   Enter / Z         → confirmar
//   1-5               → selección rápida de slot
//   ESC               → cancelar / cerrar
//   Click izquierdo   → seleccionar y confirmar
//   Click fuera       → cerrar
//   Mouse hover       → resaltar slot
//
// ── Modos y contextos (Tier 4C.6 Fase B) ─────────────────────────────────────
//
//   Mode::Save  / Mode::Load       → qué hace el menú.
//   Context::VN                    → preview de escena + diálogo.
//   Context::Gameplay              → preview de mapa + HP/oleada/etc.
//                                    Slot 0 queda reservado como autoguardado.
//
// Para cada slot se muestra un badge informativo:
//   [VN]     → solo VN
//   [GP]     → solo gameplay
//   [VN+GP]  → híbrido (ambos coexisten)
//
// ── Uso desde VNScriptState (retrocompat) ───────────────────────────────────
//
//   m_stateStack->push(std::make_unique<VNSaveMenu>(
//       m_window, m_stateStack, Mode::Save,
//       [this](int slot){ saveToSlot(slot); },
//       [this](int slot){ loadFromSlot(slot); }
//   ));
//
// ── Uso desde Gameplay2DState ───────────────────────────────────────────────
//
//   m_stateStack->push(std::make_unique<VNSaveMenu>(
//       m_window, m_stateStack, Mode::Save,
//       [this](int s){ saveGameToSlot(s); },
//       [this](int s){ loadGameFromSlot(s); },
//       Context::Gameplay,
//       m_mapData.savePolicy.saveFile,   // "" = archivo compartido
//       m_mapData.savePolicy.slotsVisible
//   ));
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Types.h"
#include "engine/core/Settings.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/core/SaveManager.h"
#include "engine/core/EclipsParse.h"   // ← micro-lote: truncateCodepoints (§1.45)
#include "engine/core/LangStore.h"     // ← idiomas CP3: nombres de mes/día

#include <SDL.h>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <ctime>

namespace eclips
{
    // Información de un slot para mostrar en pantalla
    struct VNSlotInfo
    {
        bool        occupied = false;
        std::string scene;       // Nombre de la escena (VN)
        std::string preview;     // Último diálogo (VN)
        std::string timestamp;   // Metadata genérica — última modificación
        long long   timeRaw = 0; // Epoch crudo (CP2); 0 = save viejo sin crudo
        std::string scriptPath;  // Ruta del script activo al guardar (VN)

        // ── Info contextual del gameplay 2D (Tier 4C.6 Fase B) ──
        bool        hasVN = false;      // ¿Tiene bloque VN guardado?
        bool        hasGP = false;      // ¿Tiene bloque gameplay 2D guardado?
        std::string gpPreview;          // "Mapa: bosque - HP 3/5"
        std::string gpMapPath;          // Ruta del .eclmap activo al guardar

        // Etapa 3: alguna mitad supera su techo (VN v4 / Gameplay v2).
        bool        incompatible = false;
    };

    class VNSaveMenu : public GameState
    {
    public:

        enum class Mode { Save, Load };
        enum class Context { VN, Gameplay };

        static constexpr int NUM_SLOTS = 5;
        static constexpr int VN_FORMAT_VERSION = 4;
        static constexpr const char* SAVE_FILE = "saves/vn_saves.eclips";

        // ── La fecha de una tarjeta de slot (idiomas CP3, fork 4) ─────────
        //
        // Este menú no tiene acceso ni al tema ni al LangStore: los dos se los
        // pasa quien lo abre. Con el default (`formato` vacío) la conducta es
        // EXACTAMENTE la de siempre — el string que `strftime` persistió al
        // guardar —, y eso es lo que mantiene byte-idéntico a todo caller que
        // todavía no lo cableó.
        //
        // El fork 4 se firmó sabiendo lo que cuesta: hasta hoy esta pantalla
        // **nunca honró `ranura_fecha_formato`**, así que cablearla no es
        // traducirla, es cambiar qué muestra. La alternativa era peor: la
        // ranura `.ecl` en inglés y este menú en español, en la misma sesión.
        struct FechaSlot
        {
            std::string       formato;   // `ranura_fecha_formato` del tema
            eclips::LangFecha lang;      // idioma de los nombres (knob resuelto)
        };

        // ── Constructor extendido (Paso 2b) ───────────────────────────────
        VNSaveMenu(SDL_Window* window,
            StateStack* stack,
            FontSourceStore* fontSources,
            Mode         mode,
            std::function<void(int)> onSave,
            std::function<void(int)> onLoad,
            Context      context = Context::VN,
            const std::string& saveFilePath = "",
            int          slotsVisible = NUM_SLOTS,
            const FechaSlot& fecha = FechaSlot{})
            : m_window(window)
            , m_stack(stack)
            , m_fontSources(fontSources)
            , m_mode(mode)
            , m_onSave(std::move(onSave))
            , m_onLoad(std::move(onLoad))
            , m_context(context)
            , m_saveFilePath(saveFilePath)
            , m_fecha(fecha)
        {
            m_font.setSourceStore(m_fontSources);
            m_small.setSourceStore(m_fontSources);
            // Acotar al rango [1, NUM_SLOTS] para evitar overflow en el render
            m_slotsVisible = std::max(1, std::min(slotsVisible, NUM_SLOTS));
        }

        // Overlay: la escena sigue dibujándose debajo
        bool isTransparent()    const override { return true; }
        bool allowsUpdateBelow()const override { return false; }

        // ── Lectura estática de slots (sin instancia) ──────────────────────
        //
        // saveFilePath opcional. Vacío o omitido → usa SAVE_FILE compartido.
        // Niveles con archivo propio (SavePolicy::saveFile) pasan su path.
        //
        static VNSlotInfo readSlot(int slot, const std::string& saveFilePath = "")
        {
            VNSlotInfo info;
            SaveManager sm;
            const std::string& path = saveFilePath.empty() ? std::string(SAVE_FILE)
                : saveFilePath;
            if (!sm.readFromFile(path)) return info;

            std::string prefix = "slot_" + std::to_string(slot) + "_";
            info.hasVN = sm.hasKey(prefix + "scene");
            info.hasGP = sm.hasKey(prefix + "gp_mappath")
                && !sm.getString(prefix + "gp_mappath").empty();
            if (!info.hasVN && !info.hasGP) return info;

            info.occupied = true;
            info.scene = sm.getString(prefix + "scene");
            info.preview = sm.getString(prefix + "preview");
            info.timestamp = sm.getString(prefix + "time");
            {
                std::string r = sm.getString(prefix + "time_raw");
                if (!r.empty()) { try { info.timeRaw = std::stoll(r); } catch (...) {} }
            }
            info.scriptPath = sm.getString(prefix + "script");
            info.gpPreview = sm.getString(prefix + "gp_preview");
            info.gpMapPath = sm.getString(prefix + "gp_mappath");

            // Etapa 3 (§4.3): si cualquiera de las dos mitades declara gramática
            // del futuro, el slot es ilegible — se marca incompatible. Los
            // bails ya protegen funcional; esto solo lo hace VISIBLE (acept. #5).
            // Reloj-diálogo CP1: la mitad VN llega a v4; GP sigue en v2.
            {
                int vnVer = sm.getInt(prefix + "ver", 1);
                int gpVer = sm.getInt(prefix + "gp_ver", 1);
                info.incompatible = (vnVer > VN_FORMAT_VERSION) || (gpVer > 2);
            }
            return info;
        }

        // ── Conteo de slots ocupados (sin instancia, 1 sola lectura) ───────
        //
        // Espeja el criterio de "ocupado" de readSlot (hasVN || hasGP), pero
        // lee el archivo UNA vez y recorre los NUM_SLOTS — en vez de 5x readSlot
        // (5 lecturas). Lo consume resolveVariable para la variable de sistema
        // `_saves` (el menú oculta "Cargar" sin partidas con [si: _saves > 0]).
        // Archivo inexistente → 0 (primera ejecución, sin crash).
        static int countSaves(const std::string& saveFilePath = "")
        {
            SaveManager sm;
            const std::string& path = saveFilePath.empty() ? std::string(SAVE_FILE)
                : saveFilePath;
            if (!sm.readFromFile(path)) return 0;

            int count = 0;
            for (int slot = 0; slot < NUM_SLOTS; ++slot)
            {
                std::string prefix = "slot_" + std::to_string(slot) + "_";
                bool hasVN = sm.hasKey(prefix + "scene");
                bool hasGP = sm.hasKey(prefix + "gp_mappath")
                    && !sm.getString(prefix + "gp_mappath").empty();
                if (hasVN || hasGP) ++count;
            }
            return count;
        }

        // ── Clave ORDENABLE desde un timestamp legible ─────────────────────
        //
        // ⚠️ ACOPLE escritura↔lectura (hermano de §1.40 measure⊥draw): el
        // timestamp se PERSISTE con strftime("%d/%m/%Y  %H:%M") en
        // VNScriptState::saveToSlot — formato día/mes/año, que NO ordena como
        // string ("31/01" > "01/12" lexicográficamente). Esta función parsea
        // ESE formato a una clave comparable (yyyymmddhhmm). Si el strftime
        // cambia de ORDEN o nº de campos, este parser se actualiza en lockstep.
        // Tolerante a separadores (/ : y espacios dobles); resolución = minuto.
        static long long tsToKey(const std::string& ts)
        {
            // Extrae las cifras en orden de aparición: dd, mm, yyyy, hh, min.
            int vals[5] = { 0, 0, 0, 0, 0 };
            int n = 0;
            long long cur = 0; bool inNum = false;
            for (char c : ts)
            {
                if (c >= '0' && c <= '9') { cur = cur * 10 + (c - '0'); inNum = true; }
                else if (inNum) { if (n < 5) vals[n++] = (int)cur; cur = 0; inNum = false; }
            }
            if (inNum && n < 5) vals[n++] = (int)cur;
            if (n < 5) return 0;   // formato inesperado / vacío → el más viejo
            const int dd = vals[0], mm = vals[1], yyyy = vals[2],
                hh = vals[3], mi = vals[4];
            return (long long)yyyy * 100000000LL + (long long)mm * 1000000LL
                + (long long)dd * 10000LL + (long long)hh * 100LL + mi;
        }

        // ── Epoch Unix → MISMA escala que tsToKey (yyyymmddhhmm) ────────────
        //
        // CP1 del desacople de fecha: los saves nuevos persisten el epoch crudo
        // (slot_N_time_raw); los viejos solo tienen el string legible. Para que
        // ambos ordenen JUNTOS hay que proyectarlos a una escala comun. Esta
        // proyecta el epoch a yyyymmddhhmm (la de tsToKey) — conversion
        // determinista de un numero, sin parsear ningun string de display.
        static long long epochToKey(long long epoch)
        {
            std::time_t t = (std::time_t)epoch;
            std::tm tmv{};
#ifdef _WIN32
            localtime_s(&tmv, &t);
#else
            tmv = *std::localtime(&t);
#endif
            return (long long)(tmv.tm_year + 1900) * 100000000LL
                + (long long)(tmv.tm_mon + 1) * 1000000LL
                + (long long)tmv.tm_mday * 10000LL
                + (long long)tmv.tm_hour * 100LL
                + (long long)tmv.tm_min;
        }

        // ── Clave ordenable unificada (epoch crudo o string legacy) ────────
        //
        // Saves nuevos → epoch crudo proyectado (robusto, no parsea display).
        // Saves viejos (sin time_raw) → tsToKey del string, como siempre.
        // tsToKey queda relegado a legacy: su fragilidad ya no toca saves
        // nuevos. Crudo corrupto → cae al string por seguridad.
        static long long slotSortKey(const SaveManager& sm,
            const std::string& prefix)
        {
            std::string raw = sm.getString(prefix + "time_raw");
            if (!raw.empty())
            {
                try { return epochToKey(std::stoll(raw)); }
                catch (...) { /* crudo ilegible → fallback al string */ }
            }
            return tsToKey(sm.getString(prefix + "time"));
        }

        // ── Formatea un epoch con plantilla de tokens {x} ──────────────────
        //
        // CP2 del desacople: tokens NUMÉRICOS bilingües sobre el epoch del save,
        // reusando el vocabulario de resolveVariable sin el _ de sistema. El
        // dispatch lineal deja hueco para CP3 (nombres mes/dia) sin reescribir.
        // Token desconocido o { sin cierre → se deja literal (no crashea).
        //
        // ── CP3 (DISENO_IDIOMAS §4) ──────────────────────────────────────
        // El tercer parámetro es el contexto de idioma de los NOMBRES. Va con
        // default para que un caller que sólo usa tokens numéricos no cambie
        // una letra: con `LangFecha{}` la función es byte-idéntica a la de CP2.
        // Sigue siendo pura y estática — el harness la cubre entera.
        static std::string formatTimestamp(long long epoch, const std::string& fmt,
                                           const eclips::LangFecha& ctx = eclips::LangFecha{})
        {
            std::time_t t = (std::time_t)epoch;
            std::tm tv{};
#ifdef _WIN32
            localtime_s(&tv, &t);
#else
            tv = *std::localtime(&t);
#endif
            auto pad2 = [](int v) {
                std::string s = std::to_string(v);
                return s.size() < 2 ? "0" + s : s;
                };

            std::string out;
            for (size_t i = 0; i < fmt.size(); )
            {
                if (fmt[i] == '{')
                {
                    size_t end = fmt.find('}', i + 1);
                    if (end != std::string::npos)
                    {
                        std::string tk = fmt.substr(i + 1, end - i - 1);
                        if (tk == "dia" || tk == "day")        out += std::to_string(tv.tm_mday);
                        else if (tk == "dia_pad" || tk == "day_pad")    out += pad2(tv.tm_mday);
                        else if (tk == "mes" || tk == "month")      out += std::to_string(tv.tm_mon + 1);
                        else if (tk == "mes_pad" || tk == "month_pad")  out += pad2(tv.tm_mon + 1);
                        else if (tk == "anio" || tk == "year")       out += std::to_string(tv.tm_year + 1900);
                        else if (tk == "hora" || tk == "hour")       out += std::to_string(tv.tm_hour);
                        else if (tk == "hora_pad" || tk == "hour_pad")   out += pad2(tv.tm_hour);
                        else if (tk == "minuto" || tk == "minute")     out += std::to_string(tv.tm_min);
                        else if (tk == "minuto_pad" || tk == "minute_pad") out += pad2(tv.tm_min);
                        else if (tk == "dia_semana" || tk == "weekday")    out += std::to_string(tv.tm_wday + 1);
                        // ── CP3: los dos tokens de NOMBRE (§4) ────────────
                        // La regla de resolución vive en LangStore, no acá: el
                        // formatter sigue sin saber de idiomas, sólo pasa el
                        // contexto. Sin contexto caen a la tabla `es`, que es
                        // la proporcionalidad fina que el §4 firma.
                        else if (tk == "mes_nombre" || tk == "month_name")
                            out += eclips::langNombreMes(tv.tm_mon, ctx);
                        else if (tk == "dia_nombre" || tk == "weekday_name")
                            out += eclips::langNombreDia(tv.tm_wday, ctx);
                        else { out += '{'; out += tk; out += '}'; }
                        i = end + 1;
                        continue;
                    }
                }
                out += fmt[i++];
            }
            return out;
        }

        // ── El texto de fecha de una tarjeta (idiomas CP3, fork 4) ────────
        //
        // Dos caminos, y el segundo NO es un caso raro: un save guardado antes
        // de que existiera `time_raw` no tiene epoch crudo, así que su única
        // fecha es el string que `strftime` dejó escrito. Ahí no hay nada que
        // reformatear ni que traducir — se muestra tal cual, congelado, y eso
        // es honesto: el dato para hacerlo mejor no está en el disco.
        //
        // Sin formato configurado (el default) siempre cae al string, que es la
        // conducta byte-idéntica a la de antes de este lote.
        std::string slotDateText(const VNSlotInfo& info) const
        {
            if (info.timeRaw > 0 && !m_fecha.formato.empty())
                return formatTimestamp(info.timeRaw, m_fecha.formato, m_fecha.lang);
            return info.timestamp;
        }

        // ── Slot ocupado más reciente (sin instancia, 1 sola lectura) ──────
        //
        // Hermano de countSaves: lee el archivo UNA vez, escanea NUM_SLOTS y
        // devuelve el ÍNDICE del slot ocupado con timestamp más nuevo, o -1 si
        // no hay ninguno. Lo consume el verbo de ciclo `continuar` (cargar la
        // partida más reciente directo, sin pantalla de Load). Mismo criterio
        // de "ocupado" que readSlot/countSaves (hasVN || hasGP). Empate por
        // minuto → gana el slot de índice mayor (determinista, vía >=).
        static int mostRecentSlot(const std::string& saveFilePath = "")
        {
            SaveManager sm;
            const std::string& path = saveFilePath.empty() ? std::string(SAVE_FILE)
                : saveFilePath;
            if (!sm.readFromFile(path)) return -1;

            int best = -1;
            long long bestKey = -1;
            for (int slot = 0; slot < NUM_SLOTS; ++slot)
            {
                std::string prefix = "slot_" + std::to_string(slot) + "_";
                bool hasVN = sm.hasKey(prefix + "scene");
                bool hasGP = sm.hasKey(prefix + "gp_mappath")
                    && !sm.getString(prefix + "gp_mappath").empty();
                if (!hasVN && !hasGP) continue;          // vacío: no candidato
                long long key = slotSortKey(sm, prefix);
                if (key >= bestKey) { bestKey = key; best = slot; }
            }
            return best;
        }

        static bool deleteSlot(int slot, const std::string& saveFilePath = "")
        {
            std::string path = saveFilePath.empty() ? SAVE_FILE : saveFilePath;
            SaveManager sm;
            if (!sm.readFromFile(path)) return false;
            std::string prefix = "slot_" + std::to_string(slot) + "_";
            size_t n = sm.eraseKeysWithPrefix(prefix);
            if (n == 0) return false;
            sm.writeToFile(path);

            // H9 + fix conflacion: borrar el sidecar (thumbnail) SOLO si el slot
            // vive en SAVE_FILE, el unico archivo que ESCRIBE thumbs (via
            // saveToSlot->captureThumbnail; el gameplay NO captura). Los thumbs
            // son namespace COMPARTIDO por indice (saves/thumbs/slot_N.png, ruta
            // fija, no per-archivo): un delete sobre archivo custom NO debe
            // tocarlos, o borraria el thumb del slot N de OTRO archivo (bug real:
            // borrar slots de gameplay nukeaba los thumbs del VN). Ruta 0-based,
            // no-op si no existe. [Namespacing per-archivo = horizonte (opcion C).]
            if (path == SAVE_FILE)
            {
                std::error_code ec;
                std::filesystem::remove(
                    "saves/thumbs/slot_" + std::to_string(slot) + ".png", ec);
            }

            std::cout << "[VNSaveMenu] Slot " << slot + 1
                << " eliminado (" << n << " campos)" << std::endl;
            return true;
        }

        // ── Escritura estática de slot VN ──────────────────────────────────
        static void writeSlot(int slot,
            const std::string& scene,
            int                cmdIndex,
            const std::string& bgPath,
            const std::string& musicPath,
            const std::string& activeCharsStr,
            const std::string& preview,
            const std::string& timestamp,
            const std::string& varsStr = "",
            const std::string& layersStr = "",
            const std::string& hotspotsStr = "",
            const std::string& eventsStr = "",
            const std::string& clicksStr = "",
            int                phase = 0,
            bool               parallaxOn = true,
            bool               eventsActive = false,
            const std::string& scriptPath = "",
            const std::string& charDefsStr = "",
            const std::string& luaStateStr = "",
            bool               speakerHL = false,
            const std::string& animsStr = "",
            float              vignetaVal = 0.0f,
            bool               boxHidden = false,
            const std::string& boxOverride = "",
            const std::string& saveFilePath = "",
            long long          savedAtEpoch = 0,
            const std::string& evScene = "",
            int                evCmd = 0,
            const std::string& guionHash = "",
            const std::string& bgTint = "")
        {
            SaveManager sm;
            const std::string& path = saveFilePath.empty() ? std::string(SAVE_FILE)
                : saveFilePath;
            sm.readFromFile(path); // OK si falla (primera vez)

            std::string p = "slot_" + std::to_string(slot) + "_";

            // Un save VN declara el slot como VN: borrar gameplay rancio.
            // El slot mixto legitimo lo escribe writeGameplaySlot DESPUES
            // (preserva las llaves VN); el orden inverso es siempre stale.
            sm.eraseKeysWithPrefix(p + "gp_");

            sm.setInt(p + "ver", VN_FORMAT_VERSION); // CP1: T4/E4; P y el
                                                      // resto conserva gramática.

            sm.setString(p + "scene", scene);
            sm.setInt(p + "cmd", cmdIndex);
            sm.setString(p + "bg", bgPath);
            sm.setString(p + "music", musicPath);
            sm.setString(p + "chars", activeCharsStr);
            sm.setString(p + "preview", preview);
            sm.setString(p + "time", timestamp);
            sm.setString(p + "time_raw", std::to_string(savedAtEpoch));
            sm.setString(p + "vars", varsStr);

            sm.setString(p + "layers", layersStr);
            sm.setString(p + "hotspots", hotspotsStr);
            sm.setString(p + "events", eventsStr);
            sm.setString(p + "clicks", clicksStr);
            sm.setInt(p + "phase", phase);
            sm.setBool(p + "parallax", parallaxOn);
            sm.setBool(p + "evactive", eventsActive);
            sm.setString(p + "script", scriptPath);
            sm.setString(p + "chardefs", charDefsStr);
            sm.setString(p + "luastate", luaStateStr);
            sm.setBool(p + "speakerhl", speakerHL);
            sm.setString(p + "anims", animsStr);
            sm.setFloat(p + "vigneta", vignetaVal);
            sm.setBool(p + "boxhidden", boxHidden);
            sm.setString(p + "boxoverride", boxOverride);

            // Ancla de retorno de evento random (revival): DOS claves
            // ADITIVAS (§7.253) — se escriben SOLO con ancla activa (guardar
            // dentro de la escena de un evento). Borrar-siempre primero: un
            // re-save del slot sin ancla no debe heredar las del save
            // anterior. El lector viejo no las pide; el save viejo no las
            // tiene → cero bump.
            sm.eraseKey(p + "evscene");
            sm.eraseKey(p + "evcmd");
            if (!evScene.empty())
            {
                sm.setString(p + "evscene", evScene);
                sm.setInt(p + "evcmd", evCmd);
            }

            // Sello de guion (H-B8): clave ADITIVA, mismas leyes que el ancla —
            // borrar-siempre, escribir-si-hay. El lector viejo no la pide; el
            // save viejo no la tiene (→ "sin sello"). Cero bump.
            sm.eraseKey(p + "guion_hash");
            if (!guionHash.empty())
                sm.setString(p + "guion_hash", guionHash);

            // Tinte del fondo (aditiva, mismas leyes).
            sm.eraseKey(p + "bgtint");
            if (!bgTint.empty())
                sm.setString(p + "bgtint", bgTint);

            // Crear carpetas padre si no existen
            {
                std::filesystem::path fp(path);
                auto parent = fp.parent_path();
                if (!parent.empty())
                {
                    std::error_code ec;
                    std::filesystem::create_directories(parent, ec);
                }
            }

            sm.writeToFile(path);
        }

        // ── Escritura estática de slot Gameplay 2D ─────────────────────────
        //
        // Escribe SOLO los campos gp_* del slot. Preserva los vn_* intactos.
        // El timestamp se escribe como metadata genérica (slot_N_time, sin
        // prefijo _gp_) para que readSlot muestre siempre la última
        // modificación, venga del VN o del gameplay.
        //
        static void writeGameplaySlot(int slot,
            const std::string& mapPath,
            float              posX,
            float              posY,
            int                playerHP,
            int                playerMaxHP,
            float              checkpointX,
            float              checkpointY,
            bool               hasCheckpoint,
            const std::string& varsStr,
            const std::string& preview,
            const std::string& timestamp,
            const std::string& saveFilePath = "",
            long long          savedAtEpoch = 0)
        {
            SaveManager sm;
            const std::string& path = saveFilePath.empty() ? std::string(SAVE_FILE)
                : saveFilePath;
            sm.readFromFile(path); // OK si falla (primera vez)

            std::string baseP = "slot_" + std::to_string(slot) + "_";
            std::string p = baseP + "gp_";

            sm.setString(p + "mappath", mapPath);
            sm.setFloat(p + "posX", posX);
            sm.setFloat(p + "posY", posY);
            sm.setInt(p + "hp", playerHP);
            sm.setInt(p + "maxhp", playerMaxHP);
            sm.setFloat(p + "cpX", checkpointX);
            sm.setFloat(p + "cpY", checkpointY);
            sm.setBool(p + "hasCP", hasCheckpoint);
            sm.setString(p + "vars", varsStr);
            sm.setInt(p + "ver", 2);   // §7.214: estampa gramática v2 de la mitad GP
            sm.setString(p + "preview", preview);

            // Metadata genérica (no pisa el slot_N_preview del VN)
            sm.setString(baseP + "time", timestamp);
            sm.setString(baseP + "time_raw", std::to_string(savedAtEpoch));

            // Crear carpetas padre si no existen
            {
                std::filesystem::path fp(path);
                auto parent = fp.parent_path();
                if (!parent.empty())
                {
                    std::error_code ec;
                    std::filesystem::create_directories(parent, ec);
                }
            }

            sm.writeToFile(path);

            std::cout << "[VNSaveMenu] Gameplay slot " << slot
                << " guardado (mapa=" << mapPath
                << ", archivo=" << path << ")" << std::endl;
        }

        // ── Carga estática de datos de un slot ─────────────────────────────
        struct SlotData
        {
            // slot_N_ver — estampa de gramática de la mitad VN (§7.214).
            // 1/ausente = legacy; 2 = codec escapado; 3 = P/T/E;
            // 4 = P + T4/E4 (reloj-diálogo, envolvente reservada para CP2).
            int         ver = 1;

            // ── VN clásica ──
            std::string scene;
            int         cmdIndex = 0;
            std::string bgPath;
            std::string musicPath;
            std::string activeCharsStr;
            std::string varsStr;

            // ── Bloque 1-2 ──
            std::string layersStr;
            std::string hotspotsStr;
            std::string eventsStr;
            std::string clicksStr;
            int         phase = 0;
            bool        parallaxOn = true;
            bool        speakerHL = false;
            bool        eventsActive = false;
            std::string scriptPath;
            std::string charDefsStr;
            std::string luaStateStr;
            std::string animsStr;
            float       vignetaIntensity = 0.0f;
            bool        boxHidden = false;
            std::string boxOverride; // usar_caja mid-escena ("" = sin override)
            std::string evScene;     // ancla de retorno de evento random
            int         evCmd = 0;   // ("" = sin ancla; claves aditivas)
            std::string guionHash;   // sello de guion H-B8 ("" = sin sello; aditiva)
            std::string bgTint;      // fondo tinte= ("" = sin tinte; aditiva)

            // ── Gameplay 2D (Tier 4C.6) ──
            // slot_N_gp_ver — estampa de gramática de la mitad GP (§7.214).
            // 1/ausente = legacy (key:val, lector congelado §7.212); 2 = v2 (key;val).
            int          gpVer = 1;
            bool         gpHasGameplay = false;
            std::string  gpMapPath;
            float        gpPosX = 0.0f;
            float        gpPosY = 0.0f;
            int          gpPlayerHP = 0;
            int          gpPlayerMaxHP = 0;
            float        gpCheckpointX = 0.0f;
            float        gpCheckpointY = 0.0f;
            bool         gpHasCheckpoint = false;
            std::string  gpVarsStr;
            std::string  gpPreview;
        };

        static SlotData loadSlotData(int slot, const std::string& saveFilePath = "")
        {
            SlotData d;
            SaveManager sm;
            const std::string& path = saveFilePath.empty() ? std::string(SAVE_FILE)
                : saveFilePath;
            if (!sm.readFromFile(path)) return d;

            std::string p = "slot_" + std::to_string(slot) + "_";

            d.ver = sm.getInt(p + "ver", 1);   // §7.214: se lee o no existe

            d.scene = sm.getString(p + "scene");
            d.cmdIndex = sm.getInt(p + "cmd", 0);
            d.bgPath = sm.getString(p + "bg");
            d.musicPath = sm.getString(p + "music");
            d.activeCharsStr = sm.getString(p + "chars");
            d.varsStr = sm.getString(p + "vars");

            d.layersStr = sm.getString(p + "layers");
            d.hotspotsStr = sm.getString(p + "hotspots");
            d.eventsStr = sm.getString(p + "events");
            d.clicksStr = sm.getString(p + "clicks");
            d.phase = sm.getInt(p + "phase", 0);
            d.parallaxOn = sm.getBool(p + "parallax", true);
            d.speakerHL = sm.getBool(p + "speakerhl", false);
            d.eventsActive = sm.getBool(p + "evactive", false);
            d.scriptPath = sm.getString(p + "script");
            d.charDefsStr = sm.getString(p + "chardefs");
            d.luaStateStr = sm.getString(p + "luastate");
            d.animsStr = sm.getString(p + "anims");
            d.vignetaIntensity = sm.getFloat(p + "vigneta", 0.0f);
            d.boxHidden = sm.getBool(p + "boxhidden", false);
            d.boxOverride = sm.getString(p + "boxoverride");
            d.evScene = sm.getString(p + "evscene");  // aditivas: save viejo → ""
            d.evCmd = sm.getInt(p + "evcmd", 0);
            d.guionHash = sm.getString(p + "guion_hash");  // aditiva: viejo → ""
            d.bgTint = sm.getString(p + "bgtint");         // aditiva: viejo → ""

            std::string gp = p + "gp_";
            d.gpVer = sm.getInt(gp + "ver", 1);   // §7.214: se lee o no existe
            d.gpHasGameplay = sm.hasKey(gp + "mappath")
                && !sm.getString(gp + "mappath").empty();
            d.gpMapPath = sm.getString(gp + "mappath");
            d.gpPosX = sm.getFloat(gp + "posX", 0.0f);
            d.gpPosY = sm.getFloat(gp + "posY", 0.0f);
            d.gpPlayerHP = sm.getInt(gp + "hp", 0);
            d.gpPlayerMaxHP = sm.getInt(gp + "maxhp", 0);
            d.gpCheckpointX = sm.getFloat(gp + "cpX", 0.0f);
            d.gpCheckpointY = sm.getFloat(gp + "cpY", 0.0f);
            d.gpHasCheckpoint = sm.getBool(gp + "hasCP", false);
            d.gpVarsStr = sm.getString(gp + "vars");
            d.gpPreview = sm.getString(gp + "preview");

            return d;
        }

        // ─────────────────────────────────────────────────────────────────────

        void onEnter() override
        {
            m_hasFont = m_font.init(Settings::defaultFontPath(), 28.0f);
            m_hasSmall = m_small.init(Settings::defaultFontPath(), 22.0f);

            // Leer todos los slots del archivo configurado
            for (int i = 0; i < NUM_SLOTS; ++i)
                m_slots[i] = readSlot(i, m_saveFilePath);

            // En modo Load, si no hay ningún slot ocupado, avisar
            if (m_mode == Mode::Load)
            {
                bool any = false;
                for (int i = 0; i < m_slotsVisible; ++i)
                    if (m_slots[i].occupied) any = true;
                if (!any)
                    std::cout << "[VNSaveMenu] Todos los slots estan vacios."
                    << std::endl;
            }

            // Cursor inicial: si contexto gameplay + save, evitar slot 0 (reservado)
            if (m_context == Context::Gameplay && m_mode == Mode::Save
                && m_slotsVisible > 1)
                m_cursor = 1;
            else
                m_cursor = 0;
        }

        void onResume() override
        {
            SDL_GetWindowSize(m_window, &m_winW, &m_winH);
        }

        bool handleEvent(const void* event) override
        {
            const SDL_Event& ev = *static_cast<const SDL_Event*>(event);
            if (ev.type == SDL_WINDOWEVENT &&
                ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                m_winW = ev.window.data1;
                m_winH = ev.window.data2;
                return true;
            }

            // Si el diálogo de eliminar está activo, toda input va allí
            if (m_confirmDelete)
                return handleConfirmDeleteInput(&ev);

            // ── Mouse ────────────────────────────────────────────────────────
            if (ev.type == SDL_MOUSEMOTION)
            {
                float vx, vy;
                mouseToVirtual((float)ev.motion.x, (float)ev.motion.y, vx, vy);
                int hit = hitTestSlot(vx, vy);
                if (hit >= 0) m_cursor = hit;
                m_hoveredTrash = hitTestTrashIcon(vx, vy);  // ← NUEVO
                return true;
            }

            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT)
            {
                float vx, vy;
                mouseToVirtual((float)ev.button.x, (float)ev.button.y, vx, vy);

                // Click en ícono de papelera → abrir diálogo
                int trashHit = hitTestTrashIcon(vx, vy);
                if (trashHit >= 0)
                {
                    m_slotPendingDelete = trashHit;
                    m_confirmDelete = true;
                    m_confirmDeleteCursor = 1;     // default "No"
                    return true;
                }

                if (isOutsidePanel(vx, vy))
                {
                    m_stack->requestPop();
                    return true;
                }

                int hit = hitTestSlot(vx, vy);
                if (hit >= 0)
                {
                    m_cursor = hit;
                    confirmSlot(hit);
                }
                return true;
            }

            // ── Teclado ──────────────────────────────────────────────────────
            if (ev.type == SDL_KEYDOWN)
            {
                switch (ev.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    m_stack->requestPop();
                    return true;

                case SDLK_UP: case SDLK_w:
                    m_cursor = (m_cursor - 1 + m_slotsVisible) % m_slotsVisible;
                    return true;

                case SDLK_DOWN: case SDLK_s:
                    m_cursor = (m_cursor + 1) % m_slotsVisible;
                    return true;

                case SDLK_RETURN: case SDLK_z:
                    confirmSlot(m_cursor);
                    return true;

                case SDLK_DELETE: case SDLK_BACKSPACE:
                    if (m_cursor >= 0 && m_cursor < m_slotsVisible &&
                        m_slots[m_cursor].occupied)
                    {
                        m_slotPendingDelete = m_cursor;
                        m_confirmDelete = true;
                        m_confirmDeleteCursor = 1;
                    }
                    return true;

                case SDLK_1: case SDLK_2: case SDLK_3:
                case SDLK_4: case SDLK_5:
                {
                    int s = ev.key.keysym.sym - SDLK_1;
                    if (s >= m_slotsVisible) return true; // fuera de rango visible
                    m_cursor = s;
                    confirmSlot(s);
                    return true;
                }

                default: break;
                }
            }

            return true;
        }

        void update(f64) override {}

        void render(f64) override
        {
            SDL_GetWindowSize(m_window, &m_winW, &m_winH);
            if (!m_hasFont) return;

            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();

            draw2d::setupViewport(m_winW, m_winH,
                viewport::width(), viewport::height());
            draw2d::setupProjection(VW, VH);

            draw2d::drawRect(0, 0, VW, VH, 0, 0, 0, 0.55f);

            // ── Panel central ──────────────────────────────────────────────
            float panW, panH, panX, panY;
            getPanelRect(VW, VH, panW, panH, panX, panY);

            draw2d::drawRect(panX, panY, panW, panH,
                0.07f, 0.07f, 0.13f, 0.92f);
            draw2d::drawRectBorder(panX, panY, panW, panH,
                0.28f, 0.50f, 0.95f, 0.75f, 2.0f);

            // Título con contexto.
            //
            // CUATRO FRASES ENTERAS, no dos fragmentos concatenados: el verbo y
            // el sustantivo no van en el mismo orden en todos los idiomas, así
            // que `"GUARDAR" + " PARTIDA"` es intraducible por construcción.
            const bool esGP = (m_context == Context::Gameplay);
            std::string title;
            if (m_mode == Mode::Save)
                title = esGP ? eclips::langUI("GUARDAR PARTIDA 2D", "SAVE 2D GAME")
                             : eclips::langUI("GUARDAR PARTIDA",    "SAVE GAME");
            else
                title = esGP ? eclips::langUI("CARGAR PARTIDA 2D",  "LOAD 2D GAME")
                             : eclips::langUI("CARGAR PARTIDA",     "LOAD GAME");
            float titleW = m_font.measureWidth(title);
            m_font.drawText(title,
                panX + (panW - titleW) / 2.0f, panY + 18.0f,
                0.70f, 0.85f, 1.0f, 1.0f);

            // ── Slots ──────────────────────────────────────────────────────
            float slotH = (panH - 80.0f) / m_slotsVisible;
            float slotX = panX + 16.0f;
            float slotW = panW - 32.0f;
            float startY = panY + 64.0f;

            for (int i = 0; i < m_slotsVisible; ++i)
            {
                float sy = startY + i * slotH;
                bool  sel = (i == m_cursor);

                bool isAutoSlot = (m_context == Context::Gameplay && i == 0);
                bool saveBlocked = (isAutoSlot && m_mode == Mode::Save);
                bool loadNoGPData = (m_context == Context::Gameplay
                    && m_mode == Mode::Load
                    && m_slots[i].occupied
                    && !m_slots[i].hasGP);
                bool grayed = saveBlocked || loadNoGPData
                    || m_slots[i].incompatible;

                // Fondo del slot (tinte dorado si autoSlot)
                if (sel)
                {
                    if (isAutoSlot)
                        draw2d::drawRect(slotX, sy, slotW, slotH - 4.0f,
                            0.18f, 0.16f, 0.10f, 0.85f);
                    else
                        draw2d::drawRect(slotX, sy, slotW, slotH - 4.0f,
                            0.12f, 0.15f, 0.28f, 0.85f);
                }
                else
                {
                    if (isAutoSlot)
                        draw2d::drawRect(slotX, sy, slotW, slotH - 4.0f,
                            0.13f, 0.11f, 0.07f, 0.60f);
                    else
                        draw2d::drawRect(slotX, sy, slotW, slotH - 4.0f,
                            0.08f, 0.09f, 0.16f, 0.60f);
                }

                // Borde del slot seleccionado
                if (sel)
                {
                    if (isAutoSlot)
                        draw2d::drawRectBorder(slotX, sy, slotW, slotH - 4.0f,
                            0.90f, 0.75f, 0.35f, 1.0f, 2.0f);
                    else
                        draw2d::drawRectBorder(slotX, sy, slotW, slotH - 4.0f,
                            0.45f, 0.72f, 1.0f, 1.0f, 2.0f);
                }

                // Número (o "A" para autosave)
                std::string numStr = isAutoSlot ? std::string("A")
                    : std::to_string(i + 1);
                float nw = m_font.measureWidth(numStr);
                float numX = slotX + 20.0f - nw / 2.0f;
                float numY = sy + (slotH - 4.0f - m_font.getLineHeight()) / 2.0f;

                float numR, numG, numB;
                if (isAutoSlot)
                {
                    numR = sel ? 1.00f : 0.85f;
                    numG = sel ? 0.92f : 0.75f;
                    numB = sel ? 0.55f : 0.40f;
                }
                else
                {
                    numR = sel ? 1.0f : 0.6f;
                    numG = sel ? 1.0f : 0.7f;
                    numB = 1.0f;
                }
                m_font.drawText(numStr, numX, numY, numR, numG, numB, 1.0f);

                // Separador vertical
                draw2d::drawRect(slotX + 34.0f, sy + 6.0f, 1.5f, slotH - 16.0f,
                    0.30f, 0.55f, 1.0f, 0.4f);

                float contentX = slotX + 42.0f;
                float textA = grayed ? 0.50f : 1.0f;

                if (m_slots[i].occupied)
                {
                    // ── Línea principal ──────────────────────────────────
                    std::string mainLabel;
                    if (m_context == Context::Gameplay)
                    {
                        if (saveBlocked)
                        {
                            mainLabel = eclips::langUI(
                                "Autoguardado (no sobrescribible)",
                                "Autosave (not overwritable)");
                        }
                        else if (m_slots[i].hasGP)
                        {
                            std::string mp = m_slots[i].gpMapPath;
                            size_t ls = mp.find_last_of("/\\");
                            if (ls != std::string::npos) mp = mp.substr(ls + 1);
                            size_t dot = mp.find_last_of('.');
                            if (dot != std::string::npos) mp = mp.substr(0, dot);
                            mainLabel = mp;
                        }
                        else
                        {
                            mainLabel = (m_mode == Mode::Save)
                                ? eclips::langUI("Sobrescribir (solo VN)",
                                                 "Overwrite (VN only)")
                                : eclips::langUI("Sin gameplay guardado",
                                                 "No gameplay saved");
                        }
                    }
                    else // Context::VN
                    {
                        if (!m_slots[i].scriptPath.empty())
                        {
                            std::string sp = m_slots[i].scriptPath;
                            size_t ls = sp.find_last_of("/\\");
                            if (ls != std::string::npos) sp = sp.substr(ls + 1);
                            size_t dot = sp.find_last_of('.');
                            if (dot != std::string::npos) sp = sp.substr(0, dot);
                            mainLabel = sp + " > " + m_slots[i].scene;
                        }
                        else
                        {
                            // El valor es un nombre que escribió el CREADOR, y
                            // entra como texto: `langPoner` no lo re-mira.
                            mainLabel = eclips::langPoner(
                                eclips::langUI("Escena: {escena}",
                                               "Scene: {escena}"),
                                {{"escena", m_slots[i].scene}});
                        }
                    }

                    if (m_slots[i].incompatible)
                        mainLabel = eclips::langUI(
                            "Ranura incompatible (formato mas nuevo)",
                            "Incompatible slot (newer format)");

                    m_font.drawText(mainLabel, contentX, sy + 10.0f,
                        textA, textA, textA, 1.0f);

                    // ── Preview pequeño ─────────────────────────────────
                    if (m_hasSmall)
                    {
                        std::string prev;
                        if (m_context == Context::Gameplay)
                        {
                            if (!saveBlocked && m_slots[i].hasGP)
                                prev = m_slots[i].gpPreview;
                        }
                        else
                        {
                            prev = m_slots[i].preview;
                        }

                        if (!prev.empty())
                        {
                            // Recorte UTF-8-safe (§1.45): 50 GLIFOS, no 50 bytes.
                            // ACOPLADO al recorte de saveToSlot: con el límite en
                            // codepoints, un preview con tildes pasa los 52 bytes
                            // rutinariamente, así que el corte crudo de acá
                            // empezaría a partir acentos en pantalla. Cubre además
                            // gp_preview ("Mapa: <nombre> - HP n/m"), que NADIE
                            // recorta al guardar.
                            {
                                std::string cut = truncateCodepoints(prev, 50);
                                // La decisión de elipsis se toma ANTES del rtrim:
                                // si no, una cadena que cabía entera pero terminaba
                                // en espacio se encogería y fingiría un truncado
                                // (elipsis mentirosa). Ver EclipsParse.h.
                                bool truncated = (cut.size() < prev.size());
                                cut = rtrimSpaces(cut);
                                if (truncated) cut += "...";
                                prev = cut;
                            }
                            m_small.drawText("\"" + prev + "\"",
                                contentX,
                                sy + 10.0f + m_font.getLineHeight() + 2.0f,
                                0.75f * textA, 0.78f * textA,
                                0.85f * textA, 1.0f);
                        }
                    }

                    // ── Badge informativo (esquina arriba-derecha) ──────
                    if (m_hasSmall)
                    {
                        std::string badge;
                        if (m_slots[i].hasVN && m_slots[i].hasGP) badge = "[VN+GP]";
                        else if (m_slots[i].hasVN)                badge = "[VN]";
                        else if (m_slots[i].hasGP)                badge = "[GP]";
                        if (!badge.empty())
                        {
                            float bw = m_small.measureWidth(badge);
                            // La papelera (44x44) ocupa la esquina derecha; el badge se
                           // desplaza a su izquierda (44 + 10 margen derecha + 8 gap)
                            float bx = slotX + slotW - bw - 62.0f;
                            float by = sy + 8.0f;
                            m_small.drawText(badge, bx, by,
                                0.50f, 0.80f, 1.0f, 0.9f);
                        }
                    }

                    // ── Timestamp (esquina abajo-derecha) ───────────────
                    const std::string tsTexto = slotDateText(m_slots[i]);
                    if (m_hasSmall && !tsTexto.empty())
                    {
                        float tsW = m_small.measureWidth(tsTexto);
                        float tsX = slotX + slotW - tsW - 10.0f;
                        float tsY = sy + slotH - 4.0f - m_small.getLineHeight() - 6.0f;
                        m_small.drawText(tsTexto, tsX, tsY,
                            0.55f, 0.60f, 0.70f, 1.0f);
                    }

                    // ── Ícono de papelera (esquina arriba-derecha) ──────
                    float tx, ty, ts;
                    getTrashRect(i, tx, ty, ts);
                    renderTrashIcon(tx, ty, ts, m_hoveredTrash == i);
                }
                else
                {
                    // Slot vacío
                    std::string emptyStr;
                    if (isAutoSlot)
                        emptyStr = (m_mode == Mode::Save)
                        ? eclips::langUI("- Autoguardado (vacio) -", "- Autosave (empty) -")
                        : eclips::langUI("- Sin autoguardado -",     "- No autosave -");
                    else
                        emptyStr = (m_mode == Mode::Save)
                        ? eclips::langUI("- ranura vacia -", "- empty slot -")
                        : eclips::langUI("- sin guardar -",  "- no save -");
                    m_font.drawText(emptyStr,
                        contentX,
                        sy + (slotH - 4.0f - m_font.getLineHeight()) / 2.0f,
                        0.35f, 0.37f, 0.45f, 1.0f);
                }
            }

            // ── Ayuda inferior ────────────────────────────────────────────
            if (m_hasSmall)
            {
                const std::string hint = eclips::langUI(
                    "Click o Enter = confirmar   DEL = eliminar   ESC = cancelar",
                    "Click or Enter = confirm   DEL = delete   ESC = cancel");
                float hw = m_small.measureWidth(hint);
                m_small.drawText(hint, panX + (panW - hw) / 2.0f,
                    panY + panH - 22.0f,
                    0.45f, 0.50f, 0.65f, 1.0f);
            }

            // Diálogo de confirmación de eliminar (encima de todo)
            if (m_confirmDelete)
                renderConfirmDelete(VW, VH);
        }

private:

    // ── Geometría del ícono de papelera ────────────────────────────────
    // Devuelve las coordenadas del ícono para un slot dado, para que
    // render y hit-test compartan exactamente la misma posición.
    void getTrashRect(int slotIdx,
        float& tx, float& ty, float& ts) const
    {
        const float VW = (float)viewport::width();
        const float VH = (float)viewport::height();
        float panW, panH, panX, panY;
        getPanelRect(VW, VH, panW, panH, panX, panY);

        float slotH = (panH - 80.0f) / m_slotsVisible;
        float slotX = panX + 16.0f;
        float slotW = panW - 32.0f;
        float startY = panY + 64.0f;
        float sy = startY + slotIdx * slotH;

        ts = 44.0f;                     // tap-friendly para Android
        tx = slotX + slotW - ts - 10.0f;
        ty = sy + 8.0f;
    }

    // ── Dibujar ícono de papelera con primitivas ───────────────────────
    void renderTrashIcon(float x, float y, float s, bool hovered)
    {
        float r = hovered ? 0.95f : 0.65f;
        float g = hovered ? 0.35f : 0.65f;
        float b = hovered ? 0.35f : 0.70f;
        float a = 0.95f;

        // Asa de la tapa (rect corto que sobresale arriba)
        draw2d::drawRect(x + s * 0.38f, y + s * 0.15f,
            s * 0.24f, s * 0.08f,
            r, g, b, a);

        // Tapa horizontal (la línea ancha)
        draw2d::drawRect(x + s * 0.10f, y + s * 0.24f,
            s * 0.80f, s * 0.07f,
            r, g, b, a);

        // Cuerpo principal (contenedor)
        float bodyX = x + s * 0.20f;
        float bodyY = y + s * 0.36f;
        float bodyW = s * 0.60f;
        float bodyH = s * 0.52f;
        draw2d::drawRect(bodyX, bodyY, bodyW, bodyH,
            r, g, b, a);

        // 3 rayas verticales más oscuras dentro del cuerpo
        float dr = r * 0.55f;
        float dg = g * 0.55f;
        float db = b * 0.55f;
        float rayaW = 2.5f;
        float rayaH = bodyH * 0.72f;
        float rayaY = bodyY + bodyH * 0.14f;
        for (int k = 0; k < 3; ++k)
        {
            float rx = bodyX + bodyW * (0.22f + 0.28f * k);
            draw2d::drawRect(rx, rayaY, rayaW, rayaH,
                dr, dg, db, a);
        }
    }

    // ── Hit-test del ícono: retorna slot o -1 ──────────────────────────
    // Solo "golpea" si el slot está ocupado (slots vacíos no muestran
    // papelera, así que no deben ser clickeables).
    int hitTestTrashIcon(float vx, float vy) const
    {
        for (int i = 0; i < m_slotsVisible; ++i)
        {
            if (!m_slots[i].occupied) continue;
            float tx, ty, ts;
            getTrashRect(i, tx, ty, ts);
            if (vx >= tx && vx <= tx + ts &&
                vy >= ty && vy <= ty + ts)
                return i;
        }
        return -1;
    }

    // ── Ejecutar eliminación ───────────────────────────────────────────
    // Llama a deleteSlot y refresca el slot en memoria para que el
    // render inmediato muestre el slot vacío.
    void executeDelete()
    {
        if (m_slotPendingDelete < 0 ||
            m_slotPendingDelete >= m_slotsVisible) return;

        bool ok = deleteSlot(m_slotPendingDelete, m_saveFilePath);
        if (ok)
        {
            // Refrescar a estado vacío
            m_slots[m_slotPendingDelete] = VNSlotInfo{};
        }
        m_confirmDelete = false;
        m_slotPendingDelete = -1;
    }

    // ── Render del diálogo de confirmación ─────────────────────────────
    void renderConfirmDelete(float VW, float VH)
    {
        // Dim extra sobre el menú
        draw2d::drawRect(0, 0, VW, VH, 0, 0, 0, 0.40f);

        // Panel
        float dW = VW * 0.32f;
        float dH = 170.0f;
        float dX = (VW - dW) / 2.0f;
        float dY = (VH - dH) / 2.0f;

        bool isAutoSlot = (m_context == Context::Gameplay
            && m_slotPendingDelete == 0);

        draw2d::drawRect(dX, dY, dW, dH,
            0.09f, 0.06f, 0.08f, 0.96f);
        // Borde rojo (eliminar es destructivo) — más fuerte si autoSlot
        float borderR = isAutoSlot ? 0.95f : 0.70f;
        float borderG = isAutoSlot ? 0.30f : 0.35f;
        draw2d::drawRectBorder(dX, dY, dW, dH,
            borderR, borderG, 0.35f, 0.9f, 2.5f);

        // Título
        if (m_hasFont)
        {
            // FORK 7a: dice `ranura`, no `slot`. El motor le decía `ranura` al
            // creador (40 claves de tema + el comando `ranura`/`slot`) y `slot`
            // al jugador; la clave española es la que ve cada traductor, así que
            // consagraba el anglicismo justo donde más se copia.
            std::string title = isAutoSlot
                ? eclips::langUI("Eliminar el autoguardado?", "Delete the autosave?")
                : eclips::langPoner(
                      eclips::langUI("Eliminar ranura {ranura}?",
                                     "Delete slot {ranura}?"),
                      {{"ranura", std::to_string(m_slotPendingDelete + 1)}});
            float tw = m_font.measureWidth(title);
            m_font.drawText(title,
                dX + (dW - tw) / 2.0f, dY + 18.0f,
                0.95f, 0.75f, 0.75f, 1.0f);
        }

        // Subtítulo
        if (m_hasSmall)
        {
            std::string sub = isAutoSlot
                ? eclips::langUI("Perderas tu ultimo checkpoint.",
                                 "You will lose your last checkpoint.")
                : eclips::langUI("Esta accion no se puede deshacer.",
                                 "This action cannot be undone.");
            float sw = m_small.measureWidth(sub);
            m_small.drawText(sub,
                dX + (dW - sw) / 2.0f, dY + 56.0f,
                0.70f, 0.60f, 0.65f, 1.0f);
        }

        // Botones Sí / No
        float btnW = 100.0f;
        float btnH = 44.0f;
        float btnY = dY + dH - btnH - 18.0f;
        float gap = 30.0f;
        float totalW = btnW * 2 + gap;
        float btnStartX = dX + (dW - totalW) / 2.0f;

        m_delBtnY = btnY;
        m_delBtnH = btnH;
        m_delBtnW = btnW;
        m_delBtnStartX = btnStartX;
        m_delBtnGap = gap;

        const std::string labels[2] = { eclips::langUI("Si", "Yes"),
                                        eclips::langUI("No", "No") };
        for (int i = 0; i < 2; ++i)
        {
            float bx = btnStartX + i * (btnW + gap);
            bool sel = (i == m_confirmDeleteCursor);

            if (sel)
            {
                // "Sí" seleccionado tiene tinte rojo (destructivo)
                if (i == 0)
                    draw2d::drawRect(bx, btnY, btnW, btnH,
                        0.40f, 0.15f, 0.18f, 0.9f);
                else
                    draw2d::drawRect(bx, btnY, btnW, btnH,
                        0.22f, 0.18f, 0.38f, 0.9f);
                draw2d::drawRectBorder(bx, btnY, btnW, btnH,
                    0.50f, 0.65f, 1.0f, 1.0f, 2.0f);
            }
            else
            {
                draw2d::drawRect(bx, btnY, btnW, btnH,
                    0.12f, 0.10f, 0.18f, 0.7f);
            }

            if (m_hasFont)
            {
                float tw = m_font.measureWidth(labels[i]);
                float tx = bx + (btnW - tw) / 2.0f;
                float ty = btnY + (btnH - m_font.getLineHeight()) / 2.0f;
                if (sel)
                    m_font.drawText(labels[i], tx, ty,
                        1.0f, 1.0f, 1.0f, 1.0f);
                else
                    m_font.drawText(labels[i], tx, ty,
                        0.55f, 0.55f, 0.60f, 0.8f);
            }
        }
    }

    // ── Input del diálogo de confirmación ──────────────────────────────
    bool handleConfirmDeleteInput(const SDL_Event* e)
    {
        // Click en botones Sí/No
        if (e->type == SDL_MOUSEBUTTONDOWN &&
            e->button.button == SDL_BUTTON_LEFT)
        {
            float vx, vy;
            mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);
            for (int i = 0; i < 2; ++i)
            {
                float bx = m_delBtnStartX + i * (m_delBtnW + m_delBtnGap);
                if (vx >= bx && vx <= bx + m_delBtnW &&
                    vy >= m_delBtnY && vy <= m_delBtnY + m_delBtnH)
                {
                    if (i == 0) executeDelete();   // Sí
                    else {
                        m_confirmDelete = false;
                        m_slotPendingDelete = -1;
                    }
                    return true;
                }
            }
            // Click fuera = cancelar
            m_confirmDelete = false;
            m_slotPendingDelete = -1;
            return true;
        }

        // Hover actualiza cursor
        if (e->type == SDL_MOUSEMOTION)
        {
            float vx, vy;
            mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);
            for (int i = 0; i < 2; ++i)
            {
                float bx = m_delBtnStartX + i * (m_delBtnW + m_delBtnGap);
                if (vx >= bx && vx <= bx + m_delBtnW &&
                    vy >= m_delBtnY && vy <= m_delBtnY + m_delBtnH)
                {
                    m_confirmDeleteCursor = i;
                    break;
                }
            }
            return true;
        }

        // Teclado
        if (e->type == SDL_KEYDOWN)
        {
            switch (e->key.keysym.sym)
            {
            case SDLK_ESCAPE:
                m_confirmDelete = false;
                m_slotPendingDelete = -1;
                return true;
            case SDLK_LEFT: case SDLK_a:
                m_confirmDeleteCursor = 0;
                return true;
            case SDLK_RIGHT: case SDLK_d:
                m_confirmDeleteCursor = 1;
                return true;
            case SDLK_RETURN: case SDLK_z:
                if (m_confirmDeleteCursor == 0) executeDelete();
                else {
                    m_confirmDelete = false;
                    m_slotPendingDelete = -1;
                }
                return true;
            default: break;
            }
        }
        return true;
    }

        void confirmSlot(int slot)
        {
            if (m_mode == Mode::Save)
            {
                // Bloqueo de slot 0 en Context::Gameplay (reservado autosave)
                if (m_context == Context::Gameplay && slot == 0)
                {
                    std::cout << "[VNSaveMenu] Slot 0 reservado para autoguardado."
                        << std::endl;
                    return;
                }

                m_onSave(slot);
                // Refrescar info del slot guardado
                m_slots[slot] = readSlot(slot, m_saveFilePath);
                std::cout << "[VNSaveMenu] Guardado en slot " << slot + 1
                    << std::endl;
                m_stack->requestPop();
            }
            else // Load
            {
                if (!m_slots[slot].occupied)
                {
                    std::cout << "[VNSaveMenu] Slot " << slot + 1
                        << " vacio, no se puede cargar." << std::endl;
                    return;
                }
                // Bloqueo si estamos en Context::Gameplay y el slot no tiene GP
                if (m_context == Context::Gameplay && !m_slots[slot].hasGP)
                {
                    std::cout << "[VNSaveMenu] Slot " << slot + 1
                        << " no tiene gameplay guardado." << std::endl;
                    return;
                }
                // §3.4 (Ciclo-de-Vida-B): pop ANTES del callback. En load-desde-
                // menú, m_onLoad encola cmdPush(VN-juego); si popeáramos después,
                // el push aterrizaría primero y el pop del SaveMenu caería en
                // identidad-no-tope (SaveMenu clavado bajo el juego). Pop-primero
                // deja el FIFO: pop SaveMenu → push juego. En load-en-VN
                // (loadFromSlot in-place, síncrono) el orden es indiferente.
                m_stack->requestPop();
                m_onLoad(slot);
            }
        }

        // ── Coordenadas ───────────────────────────────────────────────────────

        void mouseToVirtual(float mx, float my, float& vx, float& vy) const
        {
            float targetAspect = (float)viewport::width() / viewport::height();
            float winAspect = (float)m_winW / m_winH;

            int vpX = 0, vpY = 0, vpW = m_winW, vpH = m_winH;
            if (winAspect > targetAspect)
            {
                vpW = (int)(m_winH * targetAspect);
                vpX = (m_winW - vpW) / 2;
            }
            else
            {
                vpH = (int)(m_winW / targetAspect);
                vpY = (m_winH - vpH) / 2;
            }

            vx = (mx - vpX) * (float)viewport::width() / vpW;
            vy = (my - vpY) * (float)viewport::height() / vpH;
        }

        void getPanelRect(float VW, float VH,
            float& panW, float& panH, float& panX, float& panY) const
        {
            panW = VW * 0.72f;
            panH = VH * 0.78f;
            panX = (VW - panW) / 2.0f;
            panY = (VH - panH) / 2.0f;
        }

        int hitTestSlot(float vx, float vy) const
        {
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            float panW, panH, panX, panY;
            getPanelRect(VW, VH, panW, panH, panX, panY);

            float slotH = (panH - 80.0f) / m_slotsVisible;
            float slotX = panX + 16.0f;
            float slotW = panW - 32.0f;
            float startY = panY + 64.0f;

            for (int i = 0; i < m_slotsVisible; ++i)
            {
                float sy = startY + i * slotH;
                if (vx >= slotX && vx <= slotX + slotW &&
                    vy >= sy && vy <= sy + slotH - 4.0f)
                    return i;
            }
            return -1;
        }

        bool isOutsidePanel(float vx, float vy) const
        {
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            float panW, panH, panX, panY;
            getPanelRect(VW, VH, panW, panH, panX, panY);

            return (vx < panX || vx > panX + panW ||
                vy < panY || vy > panY + panH);
        }

        // ── Miembros ──────────────────────────────────────────────────────────

        SDL_Window* m_window = nullptr;
        StateStack* m_stack = nullptr;
        FontSourceStore* m_fontSources = nullptr;
        Mode         m_mode;

        std::function<void(int)> m_onSave;
        std::function<void(int)> m_onLoad;

        VNSlotInfo   m_slots[NUM_SLOTS];
        int          m_cursor = 0;

        bool m_confirmDelete = false;
        int  m_confirmDeleteCursor = 1;  // default "No"
        int  m_slotPendingDelete = -1;

        float m_delBtnY = 0, m_delBtnH = 0, m_delBtnW = 0;
        float m_delBtnStartX = 0, m_delBtnGap = 0;

        int m_hoveredTrash = -1;

        // Config extendido (Paso 2b)
        Context      m_context = Context::VN;
        std::string  m_saveFilePath;           // Vacío = SAVE_FILE compartido
        int          m_slotsVisible = NUM_SLOTS;
        FechaSlot    m_fecha;                  // idiomas CP3: formato + idioma

        TextRenderer m_font;
        TextRenderer m_small;
        bool         m_hasFont = false;
        bool         m_hasSmall = false;

        int          m_winW = 1280;
        int          m_winH = 720;
    };

} // namespace eclips
