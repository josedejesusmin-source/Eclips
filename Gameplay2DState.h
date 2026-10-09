// ============================================================================
// Gameplay2DState.h — Gameplay 2D data-driven (v2 — Entidades)
// ============================================================================
//
// Carga niveles desde .eclmap. Soporta:
//   - Mapa de tamaño libre con cámara
//   - NPCs con diálogos y choices
//   - Enemigos con movimiento y HP
//   - Items con pickup y acciones
//   - Triggers de área con condiciones
//   - HP del jugador, daño, ataque melee
//   - HUD configurable
//   - Variables compartidas con el VN
//
// ── Uso desde EclipScript ───────────────────────────────────────────────────
//
//   gameplay topdown "niveles/bosque.eclmap"
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "engine/core/Gameplay2DVerbRoute.h"   // CP2: la Capa 3 de este estado
#include "engine/input/InputManager.h"         // CP2: la Capa 1 (tecla -> verbo)
#include "engine/core/Types.h"
#include "engine/core/SaveManager.h"
#include "engine/core/SaveCodec.h"   // G5: pirata escribe slot_N_vars en v2
#include "engine/core/Settings.h"
#include "engine/render/Texture.h"
#include "engine/render/SpriteSheet.h"
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/audio/AudioManager.h"
#include "DialogOverlayState.h"
#include "Tilemap.h"
#include "MapData.h"
#include "Gameplay2DFrames.h"
#include "Gameplay2DHud.h"
#include "Gameplay2DSpatial.h"
#include "PauseMenuState.h"
#include "LevelEndState.h"
#include "engine\scripting\LuaManager.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include "engine/core/LangStore.h"   // idiomas CP3: langUI() del gpPreview
#include "engine/core/LangPaths.h"   // espejo de .eclmap: solo el archivo que se abre

#include <ctime>
#include <tuple>
#include <utility>

namespace eclips
{
    // ── NPC en runtime (existente) ────────────────────────────────────────
    struct NPCRuntime
    {
        TileAABB     box;
        TileAABB     interactZone;
        std::string  name;
        char         tile = '?';
        DialogScript script;
        f32          r, g, b;
        bool         talked = false;
        std::vector<MapVarSet> onTalkSets;
        std::vector<std::vector<MapVarSet>> choiceSets;
    };

    // Instancia runtime neutral de un uso. Vive local a Gameplay2DState:
    // torre y enemigo comparten forma, no elegibilidad ni fase.
    struct Gameplay2DAbilityRuntime
    {
        std::string useId;
        std::string abilityId;
        Gameplay2DAbilityValues effective;
        Gameplay2DActivityIntervalFactorSet activityIntervalFactors;
        Gameplay2DActivityRangeFactorSet activityRangeFactors;
        Gameplay2DActivityDirectDamageDeltaSet activityDirectDamageDeltas;
        Gameplay2DActivityPreferenceOverrideSet activityPreferenceOverrides;
        bool         implicitLegacy = false;
        float        fireTimer = 0.0f;
        Gameplay2DActorAddress targetAddress;

        float activityInterval() const
        {
            return activityIntervalFactors.effectiveInterval(
                effective.fireRate);
        }

        float activityRange() const
        {
            return activityRangeFactors.effectiveRange(effective.range);
        }

        int activityDirectDamage() const
        {
            return effective.directDamage
                ? activityDirectDamageDeltas.effectiveDirectDamage(
                    *effective.directDamage)
                : 0;
        }

        Gameplay2DTargetPreference activityPreference() const
        {
            return activityPreferenceOverrides.effectivePreference(
                effective.preference);
        }
    };

    struct Gameplay2DActivityCostBinding
    {
        int handle = -1;
        std::string useId;
        Gameplay2DActivityCommitCost cost;
    };

    struct Gameplay2DLocomotionEncounterRuntime
    {
        Gameplay2DActorAddress blockerAddress;
    };

    // ── Enemigo en runtime ────────────────────────────────────────────────
    struct EnemyRuntime
    {
        TileAABB     box;
        std::string  name;
        char         tile = '?';
        f32          r, g, b;

        int          hp = 1, maxHP = 1;
        int          damage = 1;
        float        speed = 100.0f;
        MapMovement  movement;
        float        knockback = 200.0f;
        bool         invulnerable = false;
        bool         alive = true;
        int          id = 0;           // ID único (Tier 4)

        // Patrulla
        float        originX = 0, originY = 0;
        float        patrolRange = 0;
        int          patrolDir = 1;

        // Follow/Flee
        float        range = 0;

        // Daño recibido (flash)
        float        hitFlash = 0;

        // Knockback con velocidad
        float        knockVelX = 0, knockVelY = 0;

        // Disparo: segundo owner real de usos de habilidad.
        std::vector<Gameplay2DAbilityRuntime> abilityUses;
        Gameplay2DActivityInhibitionSet activityInhibitions;
        Gameplay2DLocomotionEncounterRuntime locomotionEncounter;

        // Waypoints (Tier 3)
        std::vector<std::pair<float, float>> pathPoints; // Posiciones en píxeles
        int          pathIndex = 0;
        int          pathDir = 1;    // +1 forward, -1 backward (ping-pong)
        MapRouteTraversal pathTraversal = MapRouteTraversal::Loop;
        bool         pathComplete = false;

        // Oleadas (Tier 3)
        bool         isWaveEnemy = false;

        // Tier 4C.3: Kingdom Rush avanzado — ataque a torres
        bool         attacksTowers = false;
        float        towerAttackRange = 60.0f;
        float        towerAttackRate = 1.0f;
        int          attackingTowerId = -1;   // -1 = no atacando, >0 = id de torre
        float        towerAttackTimer = 0;

        std::vector<MapAction> onDeath;
    };

    // Causa interna de retirada. No forma parte de la sintaxis pública:
    // permite que una llegada a Base deje de fingir una derrota sin cambiar
    // todavía el contrato general de destinos.
    enum class EnemyRemovalCause
    {
        Defeated,
        ReachedBase,
        ScriptDestroyedLegacy
    };

    // X1 separa vida de presencia espacial. Custodied conserva la instancia
    // y su estado runtime, pero no ofrece posicion, simulacion ni presentacion
    // al mundo hasta un redeploy explicito.
    enum class Gameplay2DPlacedPresence
    {
        Deployed,
        Custodied
    };

    // ── Torre en runtime (Tier 4C) ─────────────────────────────────────────
    // Similar a EnemyRuntime pero bando aliado, estática, con auto-targeting.
    // NO comparte m_enemies — vive en su propio vector m_towers.
    struct TowerRuntime
    {
        TileAABB     box;
        std::string  name;
        char         tile = '?';
        f32          r, g, b;
        std::string  spritePath;

        int          hp = 0, maxHP = 0;      // 0 = indestructible
        int          cost = 0;
        std::vector<Gameplay2DAbilityRuntime> abilityUses;
        Gameplay2DActivityInhibitionSet activityInhibitions;
        std::vector<Gameplay2DActivityCostBinding> activityCosts;
        std::string  whereSlot;
        bool         obstructsLocomotion = false;

        bool         alive = true;
        Gameplay2DPlacedPresence presence =
            Gameplay2DPlacedPresence::Deployed;
        int          id = 0;
        float        hitFlash = 0;           // Para Capa 3 (daño recibido)

        std::vector<MapAction> onPlace;
        std::vector<MapAction> onDestroy;

        bool isDeployed() const
        {
            return alive &&
                presence == Gameplay2DPlacedPresence::Deployed;
        }
    };

    // ── Item en runtime ───────────────────────────────────────────────────
    struct ItemRuntime
    {
        TileAABB     box;
        std::string  name;
        char         tile = '?';
        f32          r, g, b;
        bool         collected = false;
        std::vector<MapAction> onPickup;
    };

    // ── Trigger en runtime ────────────────────────────────────────────────
    struct TriggerRuntime
    {
        TileAABB     box;
        char         tile = '?';
        bool         once = false;
        bool         activated = false;
        std::string  condVar, condOp, condVal;
        std::vector<MapAction> onEnter;
    };

    // ── Marcador espacial neutral en runtime ───────────────────────────────
    struct SpatialMarkerRuntime
    {
        std::string   id;
        TileAABB      box;
        std::set<int> insideEnemyIds;
    };

    struct SpatialMarkerEntry
    {
        std::string markerId;
        int         enemyId = 0;
    };

    struct SpatialEnemySnapshot
    {
        int      enemyId = 0;
        TileAABB box;
    };

    // ── Proyectil en runtime ───────────────────────────────────────────────────
    struct ProjectileRuntime
    {
        TileAABB box;
        float    velX = 0, velY = 0;
        int      damage = 1;
        float    lifetime = 3.0f;
        float    timer = 0;
        bool     alive = true;
        bool     friendly = false;    // true = aliado (daña enemigos), false = enemigo (daña jugador)
        bool     pierceWalls = false;
        bool     hasGravity = false;
        float    r = 1, g = 0.5f, b = 0.1f;
    };

    // ── Notificación ──────────────────────────────────────────────────────
    struct Notification
    {
        std::string text;
        float timer    = 0;
        float duration = 2.5f;
    };


    class Gameplay2DState : public GameState
    {
        enum class Dir { Idle, Up, Down, Left, Right };

    public:

        // `input` va SIN valor por defecto, y es `A-6` de la §18 aplicado tal
        // cual: un `= nullptr` habria dejado compilar los tres sitios de
        // construccion sin tocarlos, y el gameplay se quedaba sin teclado **en
        // silencio**. Sin default, el compilador verifica el cable.
        Gameplay2DState(SDL_Window*                 window,
                        SDL_GLContext               glContext,
                        StateStack*                 stateStack,
                        FontSourceStore*            fontSources,
                        const eclips::InputManager* input,
                        std::function<void()>       onFinish = nullptr)
            : m_window(window)
            , m_glContext(glContext)
            , m_stateStack(stateStack)
            , m_fontSources(fontSources)
            , m_input(input)
            , m_onFinish(onFinish)
        {
            m_hudText.setSourceStore(m_fontSources);
            m_dialogText.setSourceStore(m_fontSources);
            m_dialogName.setSourceStore(m_fontSources);
            m_dialogChoice.setSourceStore(m_fontSources);
        }

        // ── Configuración pública ─────────────────────────────────────────
        std::string m_mapPath;
        std::map<std::string, std::string>* m_sharedVars = nullptr;
        std::map<std::string, std::string> m_localVars;  // Fallback si m_sharedVars es null

        // CP3 (snapshot GP->VN): el VN que lanzo este gameplay inyecta este
        // callback (hermano de m_sharedVars, mismo carril de construccion). Al
        // guardar un slot mixto, el gameplay lo invoca ANTES de escribir gp_*
        // -> el VN de abajo se snapshotea completo (writeSlot: escena+vars+anims,
        // y de paso borra gp_* rancio). Patron Slice B / §7.178. Standalone
        // (sin VN abajo) = null -> solo gp_*, sin slot_N_vars.
        std::function<void(int, const std::string&)> m_vnSaveCb = nullptr;

        // ── Restore pendiente desde un slot (Tier 4C.6) ───────────────────
        // Si se setea antes del push al StateStack, onEnter() aplicará el
        // restore DESPUÉS de cargar el mapa y correr el Lua. Esto hace que
        // un "cargar slot" desde el menú principal arranque el gameplay
        // exactamente en el estado guardado. La estructura se declara como
        // optional via el flag pendingHasData.
        struct PendingSlotRestore
        {
            bool        hasData = false;
            float       posX = 0.0f;
            float       posY = 0.0f;
            int         playerHP = 0;
            int         playerMaxHP = 0;
            float       checkpointX = 0.0f;
            float       checkpointY = 0.0f;
            bool        hasCheckpoint = false;
            std::string varsStr;
            int         ver = 1;   // slot_N_gp_ver de origen (ruteo §7.212)
        };
        PendingSlotRestore m_pendingRestore;

        // ══════════════════════════════════════════════════════════════════
        // onEnter
        // ══════════════════════════════════════════════════════════════════

        void onEnter() override
        {
            std::cout << "[Gameplay2D] =================================" << std::endl;

            if (!m_mapPath.empty())
                loadFromFile(m_mapPath);
            else
                loadHardcoded();

            loadPlayerPresentation();

            // Fuente
            std::string fontPath = m_fontPath.empty()
                ? Settings::defaultFontPath() : m_fontPath;
            m_hasFont = m_hudText.init(fontPath, 28.0f);

            // Las fuentes del diálogo, al entrar y no en cada charla (E-48),
            // y sólo si el mapa tiene con quién hablar.
            if (!m_npcs.empty()) bakeDialogFonts();

            // Audio
            m_hasAudio = m_audio.init();
            if (m_hasAudio && !m_musicPath.empty())
            {
                m_audio.playMusic(m_musicPath, true);
                m_audio.setMusicVolume(Settings::musicVolume());
                m_audio.setSoundVolume(Settings::sfxVolume());
            }

            SDL_GetWindowSize(m_window, &m_windowW, &m_windowH);

            // ── Lua (Tier 4) ──
            m_lua.init();
            registerWorldBindings();
            if (!m_mapData.luaScript.empty())
            {
                std::string scriptDir = m_mapPath.empty() ? "" :
                    std::filesystem::path(m_mapPath).parent_path().string();
                m_lua.executeFile(m_mapData.luaScript, scriptDir);
            }

            std::cout << "[Gameplay2D] Nivel: "
                << m_tilemap.gridW() << "x" << m_tilemap.gridH()
                << ", " << m_npcs.size() << " NPCs"
                << ", " << m_enemies.size() << " enemigos"
                << ", " << m_items.size() << " items"
                << ", " << m_triggers.size() << " triggers"
                << ", " << m_spatialMarkers.size() << " marcadores"
                << std::endl;

            // ── Tier 4C.6: Aplicar restore pendiente (si hay) ──────────────
            // Se ejecuta DESPUÉS del Lua executeFile, para que los valores
            // guardados pisen cualquier set_var inicial del script.
            if (m_pendingRestore.hasData)
            {
                m_posX = m_pendingRestore.posX;
                m_posY = m_pendingRestore.posY;
                m_playerHP = m_pendingRestore.playerHP;
                if (m_pendingRestore.playerMaxHP > 0)
                    m_playerMaxHP = m_pendingRestore.playerMaxHP;
                m_checkpointX = m_pendingRestore.checkpointX;
                m_checkpointY = m_pendingRestore.checkpointY;
                m_hasCheckpoint = m_pendingRestore.hasCheckpoint;
                restoreVarsFromString(m_pendingRestore.varsStr, m_pendingRestore.ver);

                std::cout << "[Gameplay2D] Restore aplicado: pos=("
                    << m_posX << "," << m_posY << ") HP="
                    << m_playerHP << "/" << m_playerMaxHP << std::endl;

                m_notifications.push_back({ "Partida restaurada", 0, 2.5f });

                // Consumir el restore: próximos onEnter no lo re-aplican
                m_pendingRestore = PendingSlotRestore{};
            }

            std::cout << "[Gameplay2D] =================================" << std::endl;
        }

        void onExit() override
        {
            unloadPlayerPresentation();
            m_hooks.clear();
            m_avisadosLua.clear();   // CP8: mapa nuevo, avisos utiles de nuevo
            m_lua.reset();
            m_nextEnemyId = 1;
            m_nextTowerId = 1;
            m_nextActivityInhibitionHandle = 1;
            m_nextActivityCostHandle = 1;
            m_nextActivityIntervalFactorHandle = 1;
            m_nextActivityRangeFactorHandle = 1;
            m_nextActivityDirectDamageDeltaHandle = 1;
            m_nextActivityPreferenceOverrideHandle = 1;
            m_luaExitFlag = false;
            m_insideHook = false;
            m_hasUpdateHook = false;
            m_currentTex = nullptr;
            m_hudText.shutdown();
            releaseDialogFonts();
            m_audio.shutdown();
            m_npcs.clear(); m_enemies.clear();
            m_towers.clear();
            m_buildModeTowerName.clear();
            m_buildInvalidFlash = 0.0f;
            m_items.clear(); m_triggers.clear();
            m_spatialMarkers.clear();
            m_notifications.clear(); m_wallColliders.clear();
            m_projectiles.clear(); m_platformColliders.clear();
            m_texAttack.unload();
            m_baseHP = 0; m_baseMaxHP = 0;
            m_baseDestroyed = false; m_baseHitFlash = 0;
        }

        void onPause() override
        {
            if (m_hasAudio) m_audio.pauseMusic();
        }

        void onResume() override
        {
            // Reconsultar tamaño de ventana por si cambió durante la pausa
            // (PauseMenu/SaveMenu/Settings consumen los eventos de resize
            // mientras están encima, así que al volver el tamaño puede ser stale)
            SDL_GetWindowSize(m_window, &m_windowW, &m_windowH);

            if (m_retryRequested)
            {
                m_retryRequested = false;
                resetLevel();
                return;
            }

            if (!m_pendingMapLoad.empty())
            {
                // Tier 4D: cargar siguiente nivel desde bloque fin.victoria.siguiente
                std::string nextPath = m_pendingMapLoad;
                m_pendingMapLoad.clear();
                m_mapPath = nextPath;
                resetLevel();
                return;
            }

            if (m_returnToVN)
            {
                if (m_onFinish) m_onFinish();
                m_stateStack->requestPop();
                return;
            }
            if (m_returnToMenu)
            {
                m_stateStack->requestPop();
                return;
            }

            // ── Al volver de un diálogo NPC ──
            if (m_talkingToNPC >= 0 && m_talkingToNPC < (int)m_npcs.size())
            {
                auto& npc = m_npcs[m_talkingToNPC];
                npc.talked = true;
                applyVar("_npc_" + std::string(1, npc.tile) + "_talked", "=", "1");
                for (const auto& vs : npc.onTalkSets)
                    applyVar(vs.var, vs.op, vs.val);
                if (m_exitMode == MapExitMode::TalkAll) checkTalkAllExit();
                m_talkingToNPC = -1;
            }

            if (m_hasAudio)
            {
                m_audio.resumeMusic();
                m_audio.setMusicVolume(Settings::musicVolume());
                m_audio.setSoundVolume(Settings::sfxVolume());
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // handleEvent
        // ══════════════════════════════════════════════════════════════════

        bool handleEvent(const void* event) override
        {
            if (m_exiting) return true;

            const SDL_Event* e = static_cast<const SDL_Event*>(event);

            if (e->type == SDL_WINDOWEVENT &&
                e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                SDL_GetWindowSize(m_window, &m_windowW, &m_windowH);
                return false;
            }

            if (e->type == SDL_KEYDOWN)
            {
                // ══════════════════════════════════════════════════════════
                // CP2 — LAS TRES CAPAS, EN ESE ORDEN
                // ══════════════════════════════════════════════════════════
                //
                // Antes de esto había seis comparaciones de `SDLK_*` que
                // mezclaban binding y despacho. Ahora la Capa 1 traduce, la
                // Capa 3 decide, y este bloque sólo EJECUTA.
                //
                // `resolverTodos` y no `resolver`: el `ESPACIO` activa tres
                // verbos en este dominio y el singular contestaría `atacar`
                // alfabéticamente, matando el salto sin un warning.
                //
                // El `m_input` no puede ser nulo: el parámetro va sin valor
                // por defecto. Un `if (m_input)` acá convertiría un error de
                // cableado en un gameplay sin teclado.
                eclips::G2DContexto ctxVerbo;
                ctxVerbo.sideScroll       = m_isSideScroll;
                ctxVerbo.modoConstruccion = !m_buildModeTowerName.empty();
                ctxVerbo.cercaDeNPC       = (m_nearNPC >= 0 &&
                                             m_nearNPC < (int)m_npcs.size());

                // CP8: los verbos se hoistean porque el hook `on("action")`
                // los necesita ENTEROS. `g2dAccionDeVerbos` se queda con uno
                // -el que rutea- y el creador tiene que ver los tres que el
                // `ESPACIO` activa, no solo el ganador.
                const std::vector<std::string> verbos =
                    m_input->resolverTodos(e, "juego");
                const eclips::G2DAccion accion =
                    eclips::g2dAccionDeVerbos(verbos, ctxVerbo);

                // CP8: por que el efecto ruteado no corrio. Arranca en
                // `NoRuteado` porque ese es el estado honesto cuando la ley
                // contesto `Ninguno`: no hay gate que reportar.
                eclips::G2DGateCausa causaGate =
                    eclips::G2DGateCausa::NoRuteado;

                // ── Tier 4C.4: Escape cancela modo construcción ──
                if (accion.efecto == eclips::G2DEfecto::CancelarConstruccion)
                {
                    std::cout << "[Gameplay2D] Build mode cancelado (Escape)"
                        << std::endl;
                    m_buildModeTowerName.clear();
                    return true;   // consumido
                }

                // ESC → Pausa
                if (accion.efecto == eclips::G2DEfecto::AbrirPausa)
                {
                    // "Menú Principal": siempre disponible, va al root via StateStack
                    auto mainMenuCb = [this] { m_stateStack->requestPopToBottom(); };

                    // "Salir del nivel": solo si fuimos lanzados desde VN
                    std::function<void()> returnToVNCb = m_onFinish
                        ? std::function<void()>([this] { m_returnToVN = true; })
                        : nullptr;

                    std::function<void()> saveCb = nullptr;
                    std::function<void()> loadCb = nullptr;
                    if (canSaveManual())
                    {
                        saveCb = [this] { openGameplaySaveMenu(VNSaveMenu::Mode::Save); };
                        loadCb = [this] { openGameplaySaveMenu(VNSaveMenu::Mode::Load); };
                    }

                    m_stateStack->push(std::make_unique<PauseMenuState>(
                        m_window, m_stateStack, m_fontSources,
                        saveCb, loadCb, mainMenuCb, returnToVNCb
                    ));
                    return true;
                }

                // E / Enter → Interactuar con NPC
                //
                // La ley ya preguntó por el NPC (`cercaDeNPC` va en su
                // contexto), así que este efecto sólo sale cuando hay con
                // quién hablar — y por eso consume. Sin NPC contesta `Ninguno`
                // y la tecla sigue hasta el hook `key_press`, igual que antes.
                if (accion.efecto == eclips::G2DEfecto::Interactuar)
                {
                    if (m_hasAudio)
                        m_audio.playSound(Settings::defaultDialogueSoundPath());
                    auto& npc = m_npcs[m_nearNPC];
                    m_talkingToNPC = m_nearNPC;

                    DialogScript scriptCopy = npc.script;
                    int npcIdx = m_nearNPC;
                    scriptCopy.onChoice = [this, npcIdx](int choiceIdx) {
                        if (m_hasAudio)
                            m_audio.playSound(Settings::defaultDialogueSoundPath());
                        if (npcIdx < (int)m_npcs.size() &&
                            choiceIdx < (int)m_npcs[npcIdx].choiceSets.size())
                            for (const auto& vs : m_npcs[npcIdx].choiceSets[choiceIdx])
                                applyVar(vs.var, vs.op, vs.val);
                    };

                    // E-48: una vez por vida del estado. Si entró sin NPCs y
                    // una puerta o `resetLevel` lo trajo hasta acá, se hornean
                    // ahora y quedan.
                    bakeDialogFonts();
                    auto dialog = std::make_unique<DialogOverlayState>(
                        m_window, m_stateStack, m_fontSources, scriptCopy);
                    dialog->lendFonts(m_dialogFonts);
                    m_stateStack->push(std::move(dialog));
                    return true;
                }

                // Salto (side-scroll). El desempate con `atacar` —los dos
                // llevan `ESPACIO`— lo hizo la ley por `m_isSideScroll`; acá
                // sólo quedan los gates INTERNOS del efecto (suelo, coyote,
                // saltos restantes), que nunca fueron ruteo.
                if (accion.efecto == eclips::G2DEfecto::Saltar)
                {
                    // Wall-jump: si estás en wall-slide, saltar diagonal opuesto
                    if (m_isWallSliding && m_hasWallJump)
                    {
                        m_gravityVelY = -m_jumpForce;
                        m_velX = -m_wallSlideSide * m_speed * 0.8f;
                        m_isWallSliding = false;
                        m_isOnGround = false;
                        m_coyoteTimer = 0;
                        m_jumpsRemaining = std::max(0, m_jumpsRemaining - 1);
                        if (m_hasAudio) m_audio.playSound(Settings::defaultInteractSoundPath());
                        causaGate = eclips::G2DGateCausa::Ocurrio;
                    }
                    // Salto normal / doble salto
                    else if (m_isOnGround || m_coyoteTimer > 0)
                    {
                        m_gravityVelY = -m_jumpForce;
                        m_isOnGround = false;
                        m_coyoteTimer = 0;
                        m_jumpsRemaining = m_maxJumps - 1;
                        if (m_hasAudio) m_audio.playSound(Settings::defaultInteractSoundPath());
                        causaGate = eclips::G2DGateCausa::Ocurrio;
                    }
                    else if (m_jumpsRemaining > 0)
                    {
                        // Doble salto (airborne)
                        m_gravityVelY = -m_jumpForce * 0.85f;
                        m_jumpsRemaining--;
                        if (m_hasAudio) m_audio.playSound(Settings::defaultInteractSoundPath());
                        causaGate = eclips::G2DGateCausa::Ocurrio;
                    }
                    // CP8: las tres aceptaciones de arriba son suelo, coyote y
                    // salto restante; caer aca es no tener ninguna de las tres.
                    else
                        causaGate = eclips::G2DGateCausa::SinSueloNiSaltos;
                }

                // Ataque. Antes esto estaba escrito DOS veces —el `else` del
                // `ESPACIO` (topdown) y la rama de la `F` (side-scroll)— y esa
                // duplicación era la ficha `TABLA-JUEGO-INCOMPLETA`: `atacar`
                // en topdown no tenía tecla en la tabla, así que sólo existía
                // como el `else` de un salto.
                //
                // ⚠ Es la ÚNICA celda donde CP2 se sale de «byte-idéntica»:
                // en topdown la `F` pasa de muerta a atacar. Está declarado y
                // medido en `harness_juego` §1.3 y §7.5.
                if (accion.efecto == eclips::G2DEfecto::Atacar)
                {
                    if (m_canAttack && m_attackTimer <= 0)
                    {
                        performAttack();
                        causaGate = eclips::G2DGateCausa::Ocurrio;
                    }
                    // CP8: capacidad y cooldown son causas distintas -una es
                    // del `.eclmap`, la otra es del momento- y el creador que
                    // depura necesita saber cual de las dos lo freno.
                    else if (!m_canAttack)
                        causaGate = eclips::G2DGateCausa::SinCapacidad;
                    else
                        causaGate = eclips::G2DGateCausa::EnCooldown;
                }

                // Dash (side-scroll). El gate de modo lo hizo la ley; los de
                // capacidad y cooldown se quedan acá, que es donde viven.
                if (accion.efecto == eclips::G2DEfecto::Dash)
                {
                    if (m_hasDash && !m_isDashing && m_dashCooldownTimer <= 0)
                    {
                        m_isDashing = true;
                        m_dashTimer = m_dashDuration;
                        m_dashCooldownTimer = m_dashCooldown;
                        m_dashDirX = m_facingRight ? 1 : -1;
                        m_invincibleTimer = m_dashDuration; // Invulnerable durante dash
                        if (m_hasAudio) m_audio.playSound(Settings::defaultDashSoundPath());
                        causaGate = eclips::G2DGateCausa::Ocurrio;
                    }
                    // CP8: las tres razones por las que un dash no sale, en el
                    // mismo orden en que el gate las evalua.
                    else if (!m_hasDash)
                        causaGate = eclips::G2DGateCausa::SinCapacidad;
                    else if (m_isDashing)
                        causaGate = eclips::G2DGateCausa::YaEnCurso;
                    else
                        causaGate = eclips::G2DGateCausa::EnCooldown;
                }

                // ── Tier 4C.5: hook key_press para Lua ──────────────────
                // Se dispara solo en presses iniciales (no en key-repeat)
                // y solo para teclas que NINGÚN handler previo consumió
                // (por haber llegado hasta aquí sin return true).
                // Diálogo NPC y menú de pausa ya gatean automáticamente
                // vía StateStack: cuando están arriba, este handleEvent
                // ni siquiera se ejecuta.
                if (!e->key.repeat)
                {
                    // ── CP8: `on("action")` — el canal de VERBOS ────────────
                    //
                    // Va ANTES que `key_press` a proposito: es el canal
                    // ascendido, y quien escuche los dos deberia recibir
                    // primero la abstraccion y despues la tecla cruda.
                    //
                    // Llegar hasta aca YA significa que nadie consumio (las
                    // tres ramas que consumen hacen `return true` arriba), asi
                    // que el gate del `consume` adentro de `g2dVerbosParaLua`
                    // es redundante EN ESTE SITIO y deliberado: la ley es
                    // total, y `harness_juego` §1.2 es lo que mantiene iguales
                    // al `consume` de la ley y a los `return true` del sitio.
                    //
                    // ⚠ Esto NO reemplaza a `key_press`: para una tecla que no
                    // esta ligada a ningun verbo la lista sale VACIA y el hook
                    // no dispara ni una vez (`harness_juego` 9.8/9.9). Las dos
                    // teclas que el corpus escucha hoy -la `1` y la `2` de
                    // `test_td_completo.lua`- son exactamente ese caso.
                    const std::vector<eclips::G2DVerboCaido> caidos =
                        eclips::g2dVerbosParaLua(verbos, accion, causaGate);
                    for (const eclips::G2DVerboCaido& c : caidos)
                        fireHook("action", c.verbo, std::string("juego"),
                                 c.ejecutado);

                    // La voz del gate, por `stderr` (fork 6a). Es el PRIMER
                    // aviso al creador de este archivo del lado correcto: hay
                    // 122 `cout` de telemetria contra 6 `cerr`, y la
                    // separacion que el VN si respeta nunca cruzo aca.
                    // Cadena vacia = no hubo gate que reportar.
                    emitirVozDelGate(
                        eclips::g2dVerboDelEfecto(accion.efecto), causaGate);

                    const char* kn = SDL_GetKeyName(e->key.keysym.sym);

                    // CP8 / fork 9c: si esta tecla ES un verbo y el script solo
                    // escucha `key_press`, decirselo una vez. Va ANTES del
                    // disparo para que el aviso quede pegado a la linea que el
                    // creador esta por ver salir de su propio handler.
                    avisarTeclaEsVerbo(std::string(kn ? kn : ""), caidos);

                    fireHook("key_press", std::string(kn ? kn : ""));
                }

            }

            // ── Mouse motion: actualizar cursor de build mode ──
            if (e->type == SDL_MOUSEMOTION && !m_buildModeTowerName.empty())
            {
                int col, row;
                m_buildCursorInMap = screenToTile(e->motion.x, e->motion.y,
                    col, row);
                if (m_buildCursorInMap)
                {
                    m_buildCursorCol = col; m_buildCursorRow = row;
                }
            }

            // ── Mouse click ──
            if (e->type == SDL_MOUSEBUTTONDOWN)
            {
                int col, row;
                bool inMap = screenToTile(e->button.x, e->button.y, col, row);

                // (A) Build mode activo: los clicks los procesa el engine,
                //     NO se disparan hooks tile_click
                if (!m_buildModeTowerName.empty())
                {
                    if (e->button.button == SDL_BUTTON_RIGHT)
                    {
                        std::cout << "[Gameplay2D] Build mode cancelado (click der)"
                            << std::endl;
                        m_buildModeTowerName.clear();
                        return true;
                    }

                    if (e->button.button == SDL_BUTTON_LEFT && inMap)
                    {
                        if (placeTowerAt(m_buildModeTowerName, col, row))
                        {
                            // Éxito: sale de build mode
                            m_buildModeTowerName.clear();
                        }
                        else
                        {
                            // Falló: parpadeo rojo, se queda en build mode
                            m_buildInvalidFlash = 0.25f;
                        }
                        return true;
                    }
                    return true;   // absorbe cualquier otro click
                }

                // (B) Build mode NO activo: comportamiento normal
                if (inMap)
                {
                    int btn = 1;
                    if (e->button.button == SDL_BUTTON_RIGHT)  btn = 2;
                    else if (e->button.button == SDL_BUTTON_MIDDLE) btn = 3;

                    fireHook("tile_click", col, row, btn);
                }
            }

            return false;
        }


        // ── Conversión screen → tile (Tier 4C) ──
        //
        // Convierte las coordenadas del evento SDL (píxeles de ventana)
        // a (col, row) del grid. Respeta letterboxing, cámara y zoom.
        // Retorna false si el click cayó fuera del viewport virtual.
        //
        bool screenToTile(int sx, int sy, int& outCol, int& outRow) const
        {
            // 1. Letterboxing → coordenadas virtuales
            float targetAspect = (float)viewport::width() /
                viewport::height();
            float winAspect = (float)m_windowW / m_windowH;
            int vpX = 0, vpY = 0, vpW = m_windowW, vpH = m_windowH;
            if (winAspect > targetAspect)
            {
                vpW = (int)(m_windowH * targetAspect);
                vpX = (m_windowW - vpW) / 2;
            }
            else
            {
                vpH = (int)(m_windowW / targetAspect);
                vpY = (m_windowH - vpH) / 2;
            }

            // Click fuera del área virtual (barras negras)
            if (sx < vpX || sx >= vpX + vpW ||
                sy < vpY || sy >= vpY + vpH) return false;

            float scaleX = (float)viewport::width() / vpW;
            float scaleY = (float)viewport::height() / vpH;
            float vx = (sx - vpX) * scaleX;
            float vy = (sy - vpY) * scaleY;

            // 2. Compensar zoom de cámara
            if (m_cameraZoom > 0.001f && m_cameraZoom != 1.0f)
            {
                vx /= m_cameraZoom;
                vy /= m_cameraZoom;
            }

            // 3. Sumar cámara → coords de mundo
            float wx = vx + m_camX;
            float wy = vy + m_camY;

            // 4. Mundo → tile
            outCol = (int)(wx / Tilemap::TILE_SIZE);
            outRow = (int)(wy / Tilemap::TILE_SIZE);
            return true;
        }


        // ══════════════════════════════════════════════════════════════════
        // update
        // ══════════════════════════════════════════════════════════════════

        void update(f64 dt) override
        {
            if (m_exiting)
            {
                m_fadeOutAlpha += 1.5f * (f32)dt;
                if (m_fadeOutAlpha >= 1.0f)
                {
                    if (m_onFinish) m_onFinish();
                    m_stateStack->requestPop();
                }
                return;
            }

            // ── Transición entre mapas ──
            if (m_transitioning)
            {
                m_fadeOutAlpha += 1.5f * (f32)dt;
                if (m_fadeOutAlpha >= 1.0f)
                    performMapTransition();
                return;
            }

            // ── Input del jugador ──
            //
            // CP2: los cuatro scancodes se fueron. `eje()` lee los verbos
            // `mover_*` de la tabla y devuelve -1..1 — el signo es el de acá
            // (positivo a la derecha, positivo hacia abajo) y está clavado en
            // `harness_juego` §6.2/§6.4 porque invertirlo compila y camina al
            // revés.
            //
            // El `if (!m_isSideScroll)` se queda: que el eje Y exista no
            // significa que este modo lo escuche. Eso es Capa 3, igual que el
            // desempate del `ESPACIO`.
            f32 inputX = 0, inputY = 0;

            inputX = m_input->eje("mover_x", "juego");

            if (!m_isSideScroll)
            {
                // Topdown: 4 direcciones
                inputY = m_input->eje("mover_y", "juego");
            }

            // Dirección y sprite
            if (m_isSideScroll)
            {
                if (inputX != 0)
                {
                    m_lastDir = (inputX > 0) ? Dir::Right : Dir::Left;
                    m_facingRight = (inputX > 0);
                }

            }
            else
            {
                if (inputX != 0 || inputY != 0)
                {
                    if (std::abs(inputX) > std::abs(inputY))
                        m_lastDir = (inputX > 0) ? Dir::Right : Dir::Left;
                    else
                        m_lastDir = (inputY > 0) ? Dir::Down : Dir::Up;
                }

            }

            updatePlayerPresentation(dt, inputX, inputY);

            // ── Propiedades del tile bajo el jugador ──
            const MapTileDef* currentTile = findTileDefAt(m_posX, m_posY);
            bool onSlippery = currentTile && currentTile->type == MapTileType::Slippery;
            float speedMult = 1.0f;
            if (currentTile && currentTile->type == MapTileType::Slow)
                speedMult = currentTile->speedMult;

            // ── Dash cooldown ──
            if (m_dashCooldownTimer > 0)
                m_dashCooldownTimer -= (f32)dt;

            // ── Wait de acciones ──
            if (m_waitingAction)
            {
                m_waitTimer -= (f32)dt;
                if (m_waitTimer <= 0)
                    m_waitingAction = false;
            }

            // ── Cálculo de movimiento ──
            f32 mx = 0, my = 0;

            if (m_isSideScroll)
            {
                // ── Dash override ──
                if (m_isDashing)
                {
                    m_dashTimer -= (f32)dt;
                    if (m_dashTimer <= 0)
                    {
                        m_isDashing = false;
                        m_velX = m_dashDirX * m_speed * 0.3f; // Inercia post-dash
                    }
                    else
                    {
                        mx = m_dashDirX * m_dashSpeed * (f32)dt;
                        my = 0;
                        m_gravityVelY = 0; // Sin gravedad durante dash
                    }
                }
                else
                {
                    // ── Side-scroll: horizontal con tiles + gravedad ──
                    if (onSlippery)
                    {
                        float accel = m_speed * 3.0f;
                        float fric = currentTile->friction;
                        m_velX += inputX * accel * (f32)dt;
                        if (inputX == 0)
                        {
                            float decay = 1.0f - fric * (f32)dt * 10.0f;
                            if (decay < 0) decay = 0;
                            m_velX *= decay;
                        }
                        float maxVel = m_speed;
                        m_velX = std::max(-maxVel, std::min(m_velX, maxVel));
                        if (std::abs(m_velX) < 5.0f) m_velX = 0;
                        mx = m_velX * (f32)dt;
                    }
                    else
                    {
                        mx = inputX * m_speed * speedMult * (f32)dt;
                        // Mantener velX del wall-jump si hay inercia
                        if (std::abs(m_velX) > 1.0f)
                        {
                            float decay = 1.0f - 5.0f * (f32)dt;
                            if (decay < 0) decay = 0;
                            m_velX *= decay;
                            mx += m_velX * (f32)dt;
                        }
                        else
                            m_velX = 0;
                    }

                    // Wall-slide
                    m_isWallSliding = false;
                    if (m_hasWallSlide && !m_isOnGround && m_gravityVelY > 0)
                    {
                        // Chequear contacto lateral con pared
                        TileAABB testL = { m_posX - 2, m_posY + 4, 2, m_ballSize - 8 };
                        TileAABB testR = { m_posX + m_ballSize, m_posY + 4, 2, m_ballSize - 8 };
                        bool touchLeft = false, touchRight = false;

                        for (const auto& c : m_wallColliders)
                        {
                            if (testL.overlaps(c)) touchLeft = true;
                            if (testR.overlaps(c)) touchRight = true;
                        }

                        if (touchLeft && inputX < 0)
                        {
                            m_isWallSliding = true;
                            m_wallSlideSide = -1;
                        }
                        else if (touchRight && inputX > 0)
                        {
                            m_isWallSliding = true;
                            m_wallSlideSide = 1;
                        }
                    }

                    // Gravedad (reducida si wall-sliding)
                    if (m_isWallSliding)
                    {
                        m_gravityVelY = std::min(m_gravityVelY + m_gravity * 0.1f * (f32)dt,
                                                  m_wallSlideMaxSpeed);
                    }
                    else
                    {
                        m_gravityVelY += m_gravity * (f32)dt;
                    }
                    if (m_gravityVelY > m_maxFallSpeed)
                        m_gravityVelY = m_maxFallSpeed;
                    my = m_gravityVelY * (f32)dt;
                }
            }
            else
            {
                // ── Topdown: 4 direcciones con tiles ──
                if (onSlippery)
                {
                    float accel = m_speed * 3.0f;
                    float fric = currentTile->friction;

                    m_velX += inputX * accel * (f32)dt;
                    m_velY += inputY * accel * (f32)dt;

                    if (inputX == 0)
                    {
                        float decay = 1.0f - fric * (f32)dt * 10.0f;
                        if (decay < 0) decay = 0;
                        m_velX *= decay;
                    }
                    if (inputY == 0)
                    {
                        float decay = 1.0f - fric * (f32)dt * 10.0f;
                        if (decay < 0) decay = 0;
                        m_velY *= decay;
                    }

                    float maxVel = m_speed;
                    m_velX = std::max(-maxVel, std::min(m_velX, maxVel));
                    m_velY = std::max(-maxVel, std::min(m_velY, maxVel));
                    if (std::abs(m_velX) < 5.0f) m_velX = 0;
                    if (std::abs(m_velY) < 5.0f) m_velY = 0;

                    mx = m_velX * (f32)dt;
                    my = m_velY * (f32)dt;
                }
                else
                {
                    mx = inputX * m_speed * speedMult * (f32)dt;
                    my = inputY * m_speed * speedMult * (f32)dt;
                    m_velX = 0;
                    m_velY = 0;
                }
            }

            // ── Knockback velocity decay ──
            if (std::abs(m_knockVelX) > 1.0f || std::abs(m_knockVelY) > 1.0f)
            {
                mx += m_knockVelX * (f32)dt;
                my += m_knockVelY * (f32)dt;
                float decay = 1.0f - m_knockDecay * (f32)dt;
                if (decay < 0) decay = 0;
                m_knockVelX *= decay;
                m_knockVelY *= decay;
                if (std::abs(m_knockVelX) < 1.0f) m_knockVelX = 0;
                if (std::abs(m_knockVelY) < 1.0f) m_knockVelY = 0;
            }

            // ── Colisión X (ambos modos) ──
            TileAABB pb = playerBox();

            m_posX += mx;
            pb = playerBox();
            for (const auto& c : m_wallColliders)
            {
                if (pb.overlaps(c))
                {
                    // Intentar empujar bloque
                    Dir pushDir = (mx > 0) ? Dir::Right : Dir::Left;
                    if (!tryPushBlock(c.x, c.y, pushDir))
                    {
                        if (mx > 0) m_posX = c.left() - m_ballSize;
                        else if (mx < 0) m_posX = c.right();
                        m_velX = 0;
                    }
                    pb = playerBox();
                }
            }

            // ── Colisión Y ──
            if (m_isSideScroll)
            {
                // Side-scroll: detección de suelo
                bool wasOnGround = m_isOnGround;
                m_isOnGround = false;
                f32 prevY = m_posY;

                m_posY += my;
                pb = playerBox();

                // Colisión con paredes sólidas normales
                for (const auto& c : m_wallColliders)
                {
                    if (pb.overlaps(c))
                    {
                        if (my > 0)
                        {
                            m_posY = c.top() - m_ballSize;
                            m_gravityVelY = 0;
                            m_isOnGround = true;
                        }
                        else if (my < 0)
                        {
                            m_posY = c.bottom();
                            m_gravityVelY = 0;
                        }
                        pb = playerBox();
                    }
                }

                // Colisión con plataformas one-way (solo desde arriba)
                for (const auto& c : m_platformColliders)
                {
                    if (pb.overlaps(c) && m_gravityVelY >= 0)
                    {
                        // Solo colisionar si la base del jugador estaba arriba
                        f32 prevBottom = prevY + m_ballSize;
                        if (prevBottom <= c.top() + 4.0f) // pequeño margen
                        {
                            m_posY = c.top() - m_ballSize;
                            m_gravityVelY = 0;
                            m_isOnGround = true;
                            pb = playerBox();
                        }
                    }
                }

                // Coyote time
                if (wasOnGround && !m_isOnGround && m_gravityVelY >= 0)
                    m_coyoteTimer = m_coyoteTime;
                if (m_coyoteTimer > 0)
                    m_coyoteTimer -= (f32)dt;

                // Reset saltos al tocar suelo
                if (m_isOnGround)
                {
                    m_jumpsRemaining = m_maxJumps;
                    m_isWallSliding = false;
                }
            }
            else
            {
                // Topdown: empujar fuera de paredes
                m_posY += my;
                pb = playerBox();
                for (const auto& c : m_wallColliders)
                {
                    if (pb.overlaps(c))
                    {
                        Dir pushDir = (my > 0) ? Dir::Down : Dir::Up;
                        if (!tryPushBlock(c.x, c.y, pushDir))
                        {
                            if (my > 0) m_posY = c.top() - m_ballSize;
                            else if (my < 0) m_posY = c.bottom();
                            m_velY = 0;
                        }
                        pb = playerBox();
                    }
                }
            }

            // Clamp al mundo
            m_posX = std::max(0.0f, std::min(m_posX, m_tilemap.worldW() - m_ballSize));
            m_posY = std::max(0.0f, std::min(m_posY, m_tilemap.worldH() - m_ballSize));

            // ── Efectos de tile ──

            // Daño por tile (tick cada 0.5s)
            if (currentTile && currentTile->type == MapTileType::Damage && m_playerMaxHP > 0)
            {
                m_tileDamageTimer += (f32)dt;
                if (m_tileDamageTimer >= 0.5f)
                {
                    m_tileDamageTimer = 0;
                    int dmg = std::max(1, currentTile->damage);
                    m_playerHP -= dmg;
                    m_cameraShakeTimer = 0.15f;
                    std::cout << "[Gameplay2D] Tile daño: -" << dmg
                        << " HP: " << m_playerHP << "/" << m_playerMaxHP << std::endl;
                    fireHook("player_damage", dmg);

                    if (m_playerHP <= 0)
                    {
                        pushLevelEnd(false);
                        return;
                    }
                }
            }
            else
            {
                m_tileDamageTimer = 0;
            }

            // Checkpoint
            if (currentTile && currentTile->type == MapTileType::Checkpoint)
            {
                if (!m_hasCheckpoint ||
                    std::abs(m_checkpointX - m_posX) > 1.0f ||
                    std::abs(m_checkpointY - m_posY) > 1.0f)
                {
                    // Solo loguear la primera vez que pisas este checkpoint
                    if (!m_hasCheckpoint ||
                        m_tilemap.pixelToCol(m_checkpointX) != m_tilemap.pixelToCol(m_posX) ||
                        m_tilemap.pixelToRow(m_checkpointY) != m_tilemap.pixelToRow(m_posY))
                    {
                        m_checkpointX = m_posX;
                        m_checkpointY = m_posY;
                        m_hasCheckpoint = true;
                        m_notifications.push_back({ "Checkpoint!", 0, 1.5f });
                        std::cout << "[Gameplay2D] Checkpoint guardado en ("
                            << m_posX << ", " << m_posY << ")" << std::endl;

                        // Tier 4C.6: auto-save si la policy lo permite
                        triggerAutoSaveIfAllowed();
                    }
                }
            }

            // Puerta / transición a otro mapa
            if (currentTile && currentTile->type == MapTileType::Door
                && !m_transitioning && !m_exiting)
            {
                // Verificar condición
                if (!checkConditionString(currentTile->condition))
                {
                    // Puerta cerrada — mostrar mensaje una sola vez
                    if (!m_doorMessageShown)
                    {
                        std::string msg = currentTile->lockedMsg.empty()
                            ? "Cerrada." : currentTile->lockedMsg;
                        m_notifications.push_back({ msg, 0, 2.0f });
                        m_doorMessageShown = true;
                    }
                }
                else if (!currentTile->destMap.empty())
                {
                    // Iniciar transición
                    m_pendingMapPath = currentTile->destMap;
                    m_pendingSpawnX = currentTile->destX * Tilemap::TILE_SIZE;
                    m_pendingSpawnY = currentTile->destY * Tilemap::TILE_SIZE;
                    m_transitioning = true;
                    m_fadeOutAlpha = 0;
                    std::cout << "[Gameplay2D] Puerta → " << m_pendingMapPath
                        << " (" << currentTile->destX << ","
                        << currentTile->destY << ")" << std::endl;
                }
            }
            else
            {
                m_doorMessageShown = false;
            }

            // ── NPC cercano ──
            m_nearNPC = -1;
            pb = playerBox();
            for (int i = 0; i < (int)m_npcs.size(); i++)
                if (pb.overlaps(m_npcs[i].interactZone))
                { m_nearNPC = i; break; }

            // ── Enemigos: movimiento ──
            std::vector<SpatialEnemySnapshot> spatialEnemiesBeforeMove;
            if (!m_spatialMarkers.empty())
                spatialEnemiesBeforeMove = snapshotSpatialEnemies();

            updateEnemies(dt);
            updateEnemyAbilities(dt);

            // ── Marcadores espaciales neutrales ──
            if (!m_spatialMarkers.empty())
                commitSpatialMarkerEntries(spatialEnemiesBeforeMove);

            // ── Colisiones jugador-enemigo ──
            checkEnemyCollisions(dt);

            // ── Colisiones jugador-item ──
            checkItemPickups();

            // ── Triggers ──
            checkTriggers();

            // ── Torres (Tier 4C.2: auto-targeting + disparo) ──
            updateTowers(dt);

            // ── Proyectiles ──
            updateProjectiles(dt);

            // ── Timers ──
            if (m_invincibleTimer > 0)
                m_invincibleTimer -= (f32)dt;
            if (m_attackTimer > 0)
                m_attackTimer -= (f32)dt;
            if (m_attackVisualTimer > 0)
                m_attackVisualTimer -= (f32)dt;
            if (m_cameraShakeTimer > 0)
                m_cameraShakeTimer -= (f32)dt;
            if (m_baseHitFlash > 0)
                m_baseHitFlash -= (f32)dt;
            if (m_buildInvalidFlash > 0)
                m_buildInvalidFlash -= (f32)dt;

            // Notificaciones
            for (auto& n : m_notifications)
                n.timer += (f32)dt;
            m_notifications.erase(
                std::remove_if(m_notifications.begin(), m_notifications.end(),
                    [](const Notification& n) { return n.timer >= n.duration; }),
                m_notifications.end());

            // ── Oleadas ──
            updateWaves(dt);

            // ── Lua update hook ──
            if (m_hasUpdateHook) fireHook("update", (float)dt);

            // ── Condiciones de salida ──
            if (m_exitMode == MapExitMode::ReachExit)  checkReachExit();
            if (m_exitMode == MapExitMode::KillAll)    checkKillAllExit();
            if (m_exitMode == MapExitMode::CollectAll) checkCollectAllExit();
            if (m_exitMode == MapExitMode::Variable)   checkVariableExit();
            if (m_exitMode == MapExitMode::Timer)      checkTimerExit(dt);
            if (m_exitMode == MapExitMode::Waves)      checkWavesExit();
            if (m_exitMode == MapExitMode::Lua)        checkLuaExit();

            // ── Cámara ──
            updateCamera();

            m_totalTime += dt;

            if (m_fadeInAlpha > 0)
            {
                m_fadeInAlpha -= 2.5f * (f32)dt;
                if (m_fadeInAlpha < 0) m_fadeInAlpha = 0;
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // render
        // ══════════════════════════════════════════════════════════════════

        void render(f64 alpha) override
        {
            const f32 VW = (f32)viewport::width();
            const f32 VH = (f32)viewport::height();

            draw2d::setupViewport(m_windowW, m_windowH,
                                  viewport::width(),
                                  viewport::height());
            draw2d::setupProjection(VW, VH);

            // Camera zoom
            if (m_cameraZoom != 1.0f)
            {
                f32 zoomVW = VW / m_cameraZoom;
                f32 zoomVH = VH / m_cameraZoom;
                draw2d::setupProjection(zoomVW, zoomVH);
            }

            // Camera shake offset
            f32 shakeX = 0, shakeY = 0;
            if (m_cameraShakeTimer > 0)
            {
                shakeX = ((f32)(rand()%20) - 10.0f) * (m_cameraShakeTimer * 3.0f);
                shakeY = ((f32)(rand()%20) - 10.0f) * (m_cameraShakeTimer * 3.0f);
            }
            f32 cx = m_camX + shakeX, cy = m_camY + shakeY;

            // 1. Fondo
            f32 bgR = 0.12f + (f32)std::sin(m_totalTime*0.08)*0.02f;
            f32 bgG = 0.30f + (f32)std::sin(m_totalTime*0.12)*0.03f;
            f32 bgB = 0.14f + (f32)std::sin(m_totalTime*0.10)*0.02f;
            draw2d::drawRect(0, 0, VW, VH, bgR, bgG, bgB);

            // 2. Tilemap
            m_tilemap.render(m_totalTime, cx, cy);

            // 3. Items (debajo de entidades)
            for (const auto& item : m_items)
            {
                if (item.collected) continue;
                f32 sx = item.box.x - cx;
                f32 sy = item.box.y - cy;
                if (sx + item.box.w < 0 || sx > VW ||
                    sy + item.box.h < 0 || sy > VH) continue;

                // Flotación suave
                f32 floatY = (f32)std::sin(m_totalTime * 3.0 + item.tile) * 4.0f;
                f32 itemSize = item.box.w * 0.6f;
                f32 ix = sx + (item.box.w - itemSize) * 0.5f;
                f32 iy = sy + (item.box.h - itemSize) * 0.5f + floatY;

                // Brillo pulsante
                f32 glow = (f32)std::sin(m_totalTime * 4.0) * 0.15f + 0.85f;
                draw2d::drawRect(ix, iy, itemSize, itemSize,
                                 item.r * glow, item.g * glow, item.b * glow, 0.9f);
                draw2d::drawRectBorder(ix, iy, itemSize, itemSize,
                                        1.0f, 1.0f, 1.0f, 0.5f * glow, 1.5f);

                // Nombre
                if (m_hasFont)
                    m_hudText.drawText(item.name,
                        sx + item.box.w*0.5f - item.name.size()*4.5f,
                        sy - 18, 1,1,1, 0.7f);
            }

            // 4. Enemigos
            for (const auto& enemy : m_enemies)
            {
                if (!enemy.alive) continue;
                f32 sx = enemy.box.x - cx;
                f32 sy = enemy.box.y - cy;
                if (sx + enemy.box.w < 0 || sx > VW ||
                    sy + enemy.box.h < 0 || sy > VH) continue;

                // Flash de daño
                f32 fr = enemy.r, fg = enemy.g, fb = enemy.b;
                if (enemy.hitFlash > 0)
                { fr = 1.0f; fg = 0.3f; fb = 0.3f; }

                draw2d::drawRect(sx, sy, enemy.box.w, enemy.box.h,
                                 fr, fg, fb, 0.9f);
                draw2d::drawRectBorder(sx, sy, enemy.box.w, enemy.box.h,
                                        1,0.2f,0.2f, 0.7f, 2.0f);

                // Barra de HP (solo si ha recibido daño)
                if (enemy.hp < enemy.maxHP && enemy.hp > 0)
                {
                    f32 barW = enemy.box.w;
                    f32 barH = 6.0f;
                    f32 barY = sy - 10;
                    f32 hpRatio = (f32)enemy.hp / (f32)enemy.maxHP;
                    draw2d::drawRect(sx, barY, barW, barH, 0.2f,0,0, 0.8f);
                    draw2d::drawRect(sx, barY, barW * hpRatio, barH,
                                     0.1f,0.9f,0.1f, 0.9f);
                }

                // Nombre
                if (m_hasFont)
                    m_hudText.drawText(enemy.name,
                        sx + enemy.box.w*0.5f - enemy.name.size()*4.5f,
                        sy - 22, 1,0.4f,0.4f, 0.8f);
            }

            // 4a. Torres (Tier 4C) — borde azul aliado + borde interior
            for (const auto& tower : m_towers)
            {
                if (!tower.isDeployed()) continue;
                f32 sx = tower.box.x - cx;
                f32 sy = tower.box.y - cy;
                if (sx + tower.box.w < 0 || sx > VW ||
                    sy + tower.box.h < 0 || sy > VH) continue;

                // Flash de daño (Capa 3)
                f32 fr = tower.r, fg = tower.g, fb = tower.b;
                if (tower.hitFlash > 0)
                {
                    fr = 1.0f; fg = 0.3f; fb = 0.3f;
                }

                // Cuerpo
                draw2d::drawRect(sx, sy, tower.box.w, tower.box.h,
                    fr, fg, fb, 0.9f);
                // Borde exterior azul aliado (distingue de enemigos)
                draw2d::drawRectBorder(sx, sy, tower.box.w, tower.box.h,
                    0.3f, 0.6f, 1.0f, 0.9f, 2.0f);
                // Borde interior blanco (distingue de enemigo del mismo color)
                draw2d::drawRectBorder(sx + 4, sy + 4,
                    tower.box.w - 8, tower.box.h - 8,
                    1, 1, 1, 0.5f, 1.5f);

                // Barra de HP (solo si tiene HP y ha recibido daño)
                if (tower.maxHP > 0 && tower.hp < tower.maxHP && tower.hp > 0)
                {
                    f32 barW = tower.box.w;
                    f32 barH = 6.0f;
                    f32 barY = sy - 10;
                    f32 hpRatio = (f32)tower.hp / (f32)tower.maxHP;
                    draw2d::drawRect(sx, barY, barW, barH, 0.2f, 0, 0, 0.8f);
                    draw2d::drawRect(sx, barY, barW * hpRatio, barH,
                        0.2f, 0.7f, 1.0f, 0.9f);
                }

                // Nombre
                if (m_hasFont)
                    m_hudText.drawText(tower.name,
                        sx + tower.box.w * 0.5f - tower.name.size() * 4.5f,
                        sy - 22, 0.3f, 0.7f, 1.0f, 0.9f);
            }

            // 4c. Overlay de modo construcción (Tier 4C.4) ───────────────
            if (!m_buildModeTowerName.empty() && m_buildCursorInMap)
            {
                const MapTowerDef* def = findTowerDef(m_buildModeTowerName);
                if (def)
                {
                    bool canPlace = canPlaceTower(m_buildModeTowerName,
                        m_buildCursorCol,
                        m_buildCursorRow);

                    f32 px = m_buildCursorCol * Tilemap::TILE_SIZE - cx;
                    f32 py = m_buildCursorRow * Tilemap::TILE_SIZE - cy;
                    f32 ts = (f32)Tilemap::TILE_SIZE;

                    // Color base: verde si válido, rojo si inválido
                    f32 tr = canPlace ? 0.2f : 1.0f;
                    f32 tg = canPlace ? 1.0f : 0.2f;
                    f32 tb = canPlace ? 0.2f : 0.2f;

                    // Parpadeo rojo extra si recién hubo intento fallido
                    if (m_buildInvalidFlash > 0)
                    {
                        tr = 1.0f; tg = 0.1f; tb = 0.1f;
                    }

                    // Círculo de rango (relleno muy tenue + borde)
                    f32 towerCx = px + ts * 0.5f;
                    f32 towerCy = py + ts * 0.5f;
                    std::vector<float> visibleRanges;
                    for (const auto& use : def->abilityUses)
                    {
                        const float range = use.effective.range;
                        if (!std::isfinite(range) || range <= 0.0f)
                            continue;
                        if (std::find(visibleRanges.begin(),
                                      visibleRanges.end(),
                                      range) != visibleRanges.end())
                            continue;

                        visibleRanges.push_back(range);
                        draw2d::drawCircle(towerCx, towerCy, range,
                            tr, tg, tb, 0.10f, 32);
                        draw2d::drawCircleBorder(towerCx, towerCy, range,
                            tr, tg, tb, 0.55f, 32, 2.0f);
                    }

                    // Fantasma de la torre sobre el tile
                    draw2d::drawRect(px, py, ts, ts,
                        def->r, def->g, def->b, 0.45f);
                    draw2d::drawRectBorder(px, py, ts, ts,
                        tr, tg, tb, 0.9f, 3.0f);
                    draw2d::drawRectBorder(px + 4, py + 4, ts - 8, ts - 8,
                        1, 1, 1, 0.4f, 1.5f);

                    // Label con el nombre de la torre
                    if (m_hasFont)
                        m_hudText.drawText(def->name,
                            px + ts * 0.5f - def->name.size() * 4.5f,
                            py - 22,
                            tr, tg, tb, 0.95f);
                }
            }

            // 4b. Proyectiles
            for (const auto& proj : m_projectiles)
            {
                if (!proj.alive) continue;
                f32 px = proj.box.x - cx;
                f32 py = proj.box.y - cy;
                if (px + proj.box.w < 0 || px > VW ||
                    py + proj.box.h < 0 || py > VH) continue;

                f32 glow = 0.7f + 0.3f * (f32)std::sin(m_totalTime * 8.0 + proj.timer * 5.0);
                draw2d::drawRect(px, py, proj.box.w, proj.box.h,
                                 proj.r * glow, proj.g * glow, proj.b * glow, 0.95f);
                draw2d::drawRectBorder(px, py, proj.box.w, proj.box.h,
                                        proj.r, proj.g, proj.b, 0.7f, 1.5f);

                // Estela (rect más pequeño detrás)
                f32 trailX = px - proj.velX * 0.02f;
                f32 trailY = py - proj.velY * 0.02f;
                f32 ts = proj.box.w * 0.6f;
                f32 to = (proj.box.w - ts) * 0.5f;
                draw2d::drawRect(trailX + to, trailY + to, ts, ts,
                                 proj.r * 0.5f, proj.g * 0.5f, proj.b * 0.5f, 0.4f);
            }

            // 5. NPCs
            for (int i = 0; i < (int)m_npcs.size(); i++)
            {
                const auto& npc = m_npcs[i];
                f32 sx = npc.box.x - cx;
                f32 sy = npc.box.y - cy;
                if (sx + npc.box.w < 0 || sx > VW ||
                    sy + npc.box.h < 0 || sy > VH) continue;

                f32 pulse = (f32)std::sin(m_totalTime*2 + i*1.5) * 0.05f + 0.95f;
                f32 a = npc.talked ? 0.4f : 0.9f;
                draw2d::drawRect(sx, sy, npc.box.w, npc.box.h,
                                 npc.r*pulse, npc.g*pulse, npc.b*pulse, a);
                draw2d::drawRectBorder(sx, sy, npc.box.w, npc.box.h,
                                        npc.r, npc.g, npc.b, 0.8f, 2.0f);

                if (npc.talked && m_hasFont)
                    m_hudText.drawText("OK", sx+npc.box.w*0.3f, sy+npc.box.h*0.3f,
                                       0.5f,0.9f,0.5f, 0.7f);

                if (i == m_nearNPC && m_hasFont)
                {
                    std::string prompt = "[E] " + npc.name +
                        (npc.talked ? " (hablaste)" : "");
                    m_hudText.drawText(prompt,
                        sx + npc.box.w*0.5f - prompt.size()*5.5f,
                        sy - 24, 1,1,0.7f, 0.9f);
                }
            }

            // 6. Sombra del jugador
            f32 psx = m_posX - cx;
            f32 psy = m_posY - cy;
            f32 sw = m_ballSize * 0.7f, sh = m_ballSize * 0.15f;
            draw2d::drawRect(psx + (m_ballSize-sw)/2, psy + m_ballSize - sh*0.3f,
                             sw, sh, 0,0,0, 0.2f);

            // 7. Jugador (parpadea si invencible)
            bool showPlayer = true;
            if (m_invincibleTimer > 0)
                showPlayer = ((int)(m_totalTime * 10) % 2 == 0);

            if (showPlayer)
            {
                if (m_hasSprites && playerAnimation(m_playerFrameState))
                    playerAnimation(m_playerFrameState)->draw(
                        psx, psy, m_ballSize, m_ballSize);
                else if (m_hasSprites && m_currentTex)
                    draw2d::drawTexture(*m_currentTex, psx, psy,
                                        m_ballSize, m_ballSize);
                else
                {
                    draw2d::drawRect(psx, psy, m_ballSize, m_ballSize,
                                     0.1f,0.1f,0.1f);
                    f32 es = m_ballSize * 0.15f;
                    f32 ey = psy + m_ballSize * 0.3f;
                    draw2d::drawRect(psx + m_ballSize*0.25f, ey, es, es, 1,1,1);
                    draw2d::drawRect(psx + m_ballSize*0.60f, ey, es, es, 1,1,1);
                }
            }

            // 7b. Wall-slide visual
            if (m_isWallSliding && showPlayer)
            {
                f32 sparkX = (m_wallSlideSide < 0) ? psx - 4 : psx + m_ballSize;
                for (int si = 0; si < 3; si++)
                {
                    f32 sy2 = psy + m_ballSize * 0.3f + si * 12.0f;
                    f32 sparkAlpha = 0.5f + 0.3f * (f32)std::sin(m_totalTime * 15.0 + si);
                    draw2d::drawRect(sparkX, sy2, 4, 4, 1,1,0.5f, sparkAlpha);
                }
            }

            // 7c. Dash visual
            if (m_isDashing && showPlayer)
            {
                f32 trailAlpha = 0.4f * (m_dashTimer / m_dashDuration);
                f32 trailOffX = -m_dashDirX * 20.0f;
                draw2d::drawRect(psx + trailOffX, psy, m_ballSize, m_ballSize,
                                 0.3f, 0.6f, 1.0f, trailAlpha);
                draw2d::drawRect(psx + trailOffX * 2, psy, m_ballSize, m_ballSize,
                                 0.3f, 0.6f, 1.0f, trailAlpha * 0.5f);
            }

            // 8. Visual del ataque
            if (m_attackVisualTimer > 0)
            {
                TileAABB atk = getAttackBox();
                f32 ax = atk.x - cx, ay = atk.y - cy;
                f32 aa = m_attackVisualTimer / m_attackVisualDuration;

                if (m_hasAttackSprite)
                {
                    draw2d::drawTexture(m_texAttack, ax, ay, atk.w, atk.h);
                }
                else
                {
                    draw2d::drawRect(ax, ay, atk.w, atk.h, 1,1,0.5f, 0.5f*aa);
                    draw2d::drawRectBorder(ax, ay, atk.w, atk.h, 1,1,0, 0.8f*aa, 2);
                }
            }

            // 9. D-pad indicador
            f32 indS = 90, indX = VW - indS - 20, indY = VH - indS - 20;
            draw2d::drawRect(indX, indY, indS, indS, 0,0,0, 0.35f);
            draw2d::drawRectBorder(indX, indY, indS, indS,
                                    0.3f,0.6f,0.9f,0.5f, 1.5f);
            f32 ds = 14, dcx = indX+indS/2-ds/2, dcy = indY+indS/2-ds/2, ofst = 22;
            auto dc = [&](Dir d)->f32 { return (m_lastDir==d) ? 0.95f : 0.25f; };
            draw2d::drawRect(dcx, dcy-ofst, ds, ds, 0.3f, dc(Dir::Up), 1);
            draw2d::drawRect(dcx, dcy+ofst, ds, ds, 0.3f, dc(Dir::Down), 1);
            draw2d::drawRect(dcx-ofst, dcy, ds, ds, 0.3f, dc(Dir::Left), 1);
            draw2d::drawRect(dcx+ofst, dcy, ds, ds, 0.3f, dc(Dir::Right), 1);
            f32 ib = (m_lastDir == Dir::Idle) ? 0.6f : 0.15f;
            draw2d::drawRect(dcx, dcy, ds, ds, ib, ib, ib);

            // Restaurar proyección para HUD/fades (sin zoom)
            if (m_cameraZoom != 1.0f)
                draw2d::setupProjection(VW, VH);

            // 10. HUD
            renderHUD(VW, VH);

            // 11. Notificaciones
            for (int i = 0; i < (int)m_notifications.size() && i < 3; i++)
            {
                const auto& n = m_notifications[i];
                f32 na = 1.0f - (n.timer / n.duration);
                f32 ny = 200 + i * 40;
                if (m_hasFont)
                {
                    f32 tw = n.text.size() * 10.0f;
                    f32 nx = VW * 0.5f - tw * 0.5f;
                    draw2d::drawRect(nx - 10, ny - 4, tw + 20, 34, 0,0,0, 0.6f*na);
                    m_hudText.drawText(n.text, nx, ny, 1,1,0.6f, na);
                }
            }

            // 12. Fades
            if (m_fadeInAlpha > 0.0f)
                draw2d::drawRect(0, 0, VW, VH, 0,0,0, m_fadeInAlpha);
            if (m_fadeOutAlpha > 0.0f)
                draw2d::drawRect(0, 0, VW, VH, 0,0,0, m_fadeOutAlpha);
        }


    private:

        TileAABB playerBox() const
        { return { m_posX, m_posY, m_ballSize, m_ballSize }; }

        Texture* playerStaticTexture(Gameplay2DFrameState state)
        {
            switch (state)
            {
                case Gameplay2DFrameState::Idle:  return &m_texIdle;
                case Gameplay2DFrameState::Up:    return &m_texUp;
                case Gameplay2DFrameState::Down:  return &m_texDown;
                case Gameplay2DFrameState::Left:  return &m_texLeft;
                case Gameplay2DFrameState::Right: return &m_texRight;
            }
            return &m_texIdle;
        }

        SpriteSheet* playerAnimation(Gameplay2DFrameState state)
        {
            switch (state)
            {
                case Gameplay2DFrameState::Idle:  return m_animIdle.get();
                case Gameplay2DFrameState::Up:    return m_animUp.get();
                case Gameplay2DFrameState::Down:  return m_animDown.get();
                case Gameplay2DFrameState::Left:  return m_animLeft.get();
                case Gameplay2DFrameState::Right: return m_animRight.get();
            }
            return nullptr;
        }

        void setPlayerFrameState(Gameplay2DFrameState state,
                                 bool forceFirstFrame = false)
        {
            const bool changed = state != m_playerFrameState;
            if (!changed && !forceFirstFrame)
                return;

            m_playerFrameState = state;
            m_currentTex = playerStaticTexture(state);
            if (SpriteSheet* animation = playerAnimation(state))
                animation->play("automatic", true);
        }

        void updatePlayerPresentation(f64 dt, f32 inputX, f32 inputY)
        {
            const Gameplay2DFrameState state =
                gameplay2DResolvePlayerFrameState(
                    m_isSideScroll,
                    m_isOnGround,
                    m_gravityVelY,
                    inputX,
                    inputY);
            setPlayerFrameState(state);

            // Salida y puerta retornan antes de llegar aqui. Durante el fade
            // de entrada cambia la intencion, pero no avanza la fase visual.
            if (m_fadeInAlpha > 0.0f)
                return;

            if (SpriteSheet* animation = playerAnimation(state))
                animation->update(dt);
        }

        std::unique_ptr<SpriteSheet> loadPlayerAnimationCandidate(
            const Gameplay2DFrameSequence& sequence,
            Gameplay2DFrameState state,
            f32 fps,
            int baseWidth,
            int baseHeight)
        {
            if (sequence.kind == Gameplay2DFrameSequenceKind::Static)
                return nullptr;

            if (sequence.kind == Gameplay2DFrameSequenceKind::Invalid)
            {
                std::cerr << "[Gameplay2D] Frames '"
                          << gameplay2DFrameStateStem(state)
                          << "' invalidos; usando PNG base"
                          << std::endl;
                return nullptr;
            }

            auto candidate = std::make_unique<SpriteSheet>();
            if (!candidate->loadFrames(sequence.frames))
            {
                std::cerr << "[Gameplay2D] No se pudo cargar el ciclo '"
                          << gameplay2DFrameStateStem(state)
                          << "'; usando PNG base"
                          << std::endl;
                return nullptr;
            }

            if (!gameplay2DFrameCanvasMatches(
                    baseWidth,
                    baseHeight,
                    candidate->frameWidth(),
                    candidate->frameHeight()))
            {
                std::cerr << "[Gameplay2D] El ciclo '"
                          << gameplay2DFrameStateStem(state)
                          << "' usa lienzo "
                          << candidate->frameWidth() << "x"
                          << candidate->frameHeight()
                          << " pero su PNG base usa "
                          << baseWidth << "x" << baseHeight
                          << "; usando PNG base"
                          << std::endl;
                return nullptr;
            }

            std::vector<int> indices;
            indices.reserve(sequence.frames.size());
            for (std::size_t i = 0; i < sequence.frames.size(); ++i)
                indices.push_back(static_cast<int>(i));
            candidate->addAnimation("automatic", indices, fps, true);
            candidate->play("automatic", true);
            return candidate;
        }

        bool loadPlayerPresentation()
        {
            std::string basePath = m_mapData.playerDef.spritePath;
            if (basePath.empty())
            {
                std::cerr << "[Gameplay2D] Sin sprite de jugador en el mapa; "
                             "no hay dibujo de respaldo del lab\n";
                setPlayerFrameState(Gameplay2DFrameState::Idle, true);
                return false;
            }

            Texture idle;
            Texture up;
            Texture down;
            Texture left;
            Texture right;
            const bool basesLoaded =
                idle.load(basePath + "idle.png") &&
                up.load(basePath + "up.png") &&
                down.load(basePath + "down.png") &&
                left.load(basePath + "left.png") &&
                right.load(basePath + "right.png");
            if (!basesLoaded)
            {
                std::cerr << "[Gameplay2D] Presentacion del jugador incompleta; "
                             "se conserva el set anterior"
                          << std::endl;
                setPlayerFrameState(Gameplay2DFrameState::Idle, true);
                return false;
            }

            std::unique_ptr<SpriteSheet> animIdle;
            std::unique_ptr<SpriteSheet> animUp;
            std::unique_ptr<SpriteSheet> animDown;
            std::unique_ptr<SpriteSheet> animLeft;
            std::unique_ptr<SpriteSheet> animRight;

            std::error_code pathError;
            if (std::filesystem::is_directory(basePath, pathError) &&
                !pathError)
            {
                const f32 fps = m_mapData.playerDef.animationFps;
                animIdle = loadPlayerAnimationCandidate(
                    gameplay2DResolveFrameSequence(
                        basePath, Gameplay2DFrameState::Idle),
                    Gameplay2DFrameState::Idle, fps,
                    idle.getWidth(), idle.getHeight());
                animUp = loadPlayerAnimationCandidate(
                    gameplay2DResolveFrameSequence(
                        basePath, Gameplay2DFrameState::Up),
                    Gameplay2DFrameState::Up, fps,
                    up.getWidth(), up.getHeight());
                animDown = loadPlayerAnimationCandidate(
                    gameplay2DResolveFrameSequence(
                        basePath, Gameplay2DFrameState::Down),
                    Gameplay2DFrameState::Down, fps,
                    down.getWidth(), down.getHeight());
                animLeft = loadPlayerAnimationCandidate(
                    gameplay2DResolveFrameSequence(
                        basePath, Gameplay2DFrameState::Left),
                    Gameplay2DFrameState::Left, fps,
                    left.getWidth(), left.getHeight());
                animRight = loadPlayerAnimationCandidate(
                    gameplay2DResolveFrameSequence(
                        basePath, Gameplay2DFrameState::Right),
                    Gameplay2DFrameState::Right, fps,
                    right.getWidth(), right.getHeight());
            }

            idle.swapWith(m_texIdle);
            up.swapWith(m_texUp);
            down.swapWith(m_texDown);
            left.swapWith(m_texLeft);
            right.swapWith(m_texRight);
            m_animIdle = std::move(animIdle);
            m_animUp = std::move(animUp);
            m_animDown = std::move(animDown);
            m_animLeft = std::move(animLeft);
            m_animRight = std::move(animRight);
            m_hasSprites = true;
            setPlayerFrameState(Gameplay2DFrameState::Idle, true);
            return true;
        }

        void unloadPlayerPresentation()
        {
            m_texIdle.unload();
            m_texUp.unload();
            m_texDown.unload();
            m_texLeft.unload();
            m_texRight.unload();
            m_animIdle.reset();
            m_animUp.reset();
            m_animDown.reset();
            m_animLeft.reset();
            m_animRight.reset();
            m_playerFrameState = Gameplay2DFrameState::Idle;
            m_currentTex = nullptr;
            m_hasSprites = false;
        }

        // ── Las fuentes del diálogo NPC (LAG-2D, E-48) ──────────────────────
        // El DialogOverlayState horneaba tres atlas 1024² en cada onEnter:
        // 18–50 ms por charla (DISENO_PLAN_DE_PUBLICACION E-47). Ahora se
        // hornean acá, una vez: la primera llamada hornea y las demás no hacen
        // nada. Si un init falla, el diálogo queda sin ese texto, como antes.
        void bakeDialogFonts()
        {
            if (m_dialogFonts.complete()) return;
            const std::string font = Settings::defaultFontPath();
            m_dialogText.init(font, 36.0f);
            m_dialogName.init(font, 28.0f);
            m_dialogChoice.init(font, 32.0f);
            m_dialogFonts = { &m_dialogText, &m_dialogName, &m_dialogChoice };
        }

        void releaseDialogFonts()
        {
            if (!m_dialogFonts.complete()) return;
            m_dialogText.shutdown();
            m_dialogName.shutdown();
            m_dialogChoice.shutdown();
            m_dialogFonts = {};
        }


        // ══════════════════════════════════════════════════════════════════
        // Variables compartidas
        // ══════════════════════════════════════════════════════════════════

        void applyVar(const std::string& var, const std::string& op,
                      const std::string& val)
        {
            auto& vars = m_sharedVars ? *m_sharedVars : m_localVars;

            if (op == "=")
            { vars[var] = val; }
            else if (op == "+")
            {
                int cur = 0;
                auto it = vars.find(var);
                if (it != vars.end())
                    try { cur = std::stoi(it->second); } catch (...) {}
                int add = 0;
                try { add = std::stoi(val); } catch (...) {}
                vars[var] = std::to_string(cur + add);
            }
            else if (op == "-")
            {
                int cur = 0;
                auto it = vars.find(var);
                if (it != vars.end())
                    try { cur = std::stoi(it->second); } catch (...) {}
                int sub = 0;
                try { sub = std::stoi(val); } catch (...) {}
                vars[var] = std::to_string(cur - sub);
            }

            std::cout << "[Gameplay2D] set: " << var << " " << op << " "
                      << val << " -> " << vars[var] << std::endl;
        }

        std::string getVar(const std::string& var) const
        {
            auto& vars = m_sharedVars ? *m_sharedVars : m_localVars;
            auto it = vars.find(var);
            return (it != vars.end()) ? it->second : "";
        }

        std::vector<Gameplay2DCommitResourceBalance>
        snapshotActivityCommitResources(
            const std::vector<Gameplay2DTargetRequest>& requests) const
        {
            std::vector<Gameplay2DCommitResourceBalance> balances;
            for (const auto& request : requests)
            {
                for (const auto& cost : request.commitCosts)
                {
                    const bool alreadyPresent = std::any_of(
                        balances.begin(), balances.end(),
                        [&](const Gameplay2DCommitResourceBalance& entry) {
                            return entry.resourceKey == cost.resourceKey;
                        });
                    if (alreadyPresent)
                        continue;

                    int parsed = 0;
                    const std::string authored = getVar(cost.resourceKey);
                    const std::int64_t amount =
                        parseIntStrict(authored, parsed) == NumParse::Ok
                            ? static_cast<std::int64_t>(parsed)
                            : -1;
                    balances.push_back({ cost.resourceKey, amount });
                }
            }
            std::sort(
                balances.begin(), balances.end(),
                [](const Gameplay2DCommitResourceBalance& a,
                   const Gameplay2DCommitResourceBalance& b) {
                    return a.resourceKey < b.resourceKey;
                });
            return balances;
        }

        bool commitActivityResources(
            const std::vector<Gameplay2DActivityCommitCost>& costs)
        {
            if (costs.empty())
                return true;

            auto& vars = m_sharedVars ? *m_sharedVars : m_localVars;
            std::vector<std::pair<std::string, std::int64_t>> writes;
            writes.reserve(costs.size());

            // Las decisiones llegan agregadas por clave. Se vuelve a validar
            // el lote completo antes de escribir para conservar atomicidad aun
            // si una futura fase introdujera deriva entre snapshot y commit.
            for (const auto& cost : costs)
            {
                const auto current = vars.find(cost.resourceKey);
                int parsed = 0;
                if (current == vars.end() || cost.amount <= 0 ||
                    parseIntStrict(current->second, parsed) != NumParse::Ok ||
                    parsed < 0 ||
                    cost.amount > static_cast<std::int64_t>(parsed))
                {
                    return false;
                }
                writes.push_back({
                    cost.resourceKey,
                    static_cast<std::int64_t>(parsed) - cost.amount
                });
            }

            for (const auto& write : writes)
            {
                vars[write.first] = std::to_string(write.second);
                std::cout << "[Gameplay2D] commit resource: "
                          << write.first << " -> " << write.second
                          << std::endl;
            }
            return true;
        }


        // ══════════════════════════════════════════════════════════════════
        // Ejecución de acciones (sistema genérico)
        // ══════════════════════════════════════════════════════════════════

        void executeActions(const std::vector<MapAction>& actions,
                            float entityX = 0, float entityY = 0)
        {
            for (const auto& act : actions)
            {
                switch (act.type)
                {
                case MapActionType::SetVar:
                    applyVar(act.varSet.var, act.varSet.op, act.varSet.val);
                    break;
                case MapActionType::Sound:
                    if (m_hasAudio) m_audio.playSound(act.str1);
                    break;
                case MapActionType::Music:
                    if (m_hasAudio) m_audio.playMusic(act.str1, true);
                    break;
                case MapActionType::Notify:
                    m_notifications.push_back({ act.str1, 0, 2.5f });
                    break;
                case MapActionType::Dialog:
                    m_notifications.push_back({ act.str1, 0, 3.0f });
                    break;
                case MapActionType::CameraShake:
                    m_cameraShakeTimer = act.duration;
                    break;
                case MapActionType::Spawn:
                    spawnEntities(act.entityChar, act.count,
                                 act.str2, act.posX, act.posY);
                    break;
                case MapActionType::Drop:
                    spawnEntities(act.entityChar, 1, "at", entityX, entityY);
                    break;
                case MapActionType::Vanish:
                    // Manejado por el caller (marca collected/alive)
                    break;
                case MapActionType::CameraPan:
                    m_cameraPanning = true;
                    m_cameraFollowPlayer = false;
                    m_cameraPanStartX = m_camX;
                    m_cameraPanStartY = m_camY;
                    m_cameraPanTargetX = act.posX * Tilemap::TILE_SIZE - (f32)viewport::width() * 0.5f;
                    m_cameraPanTargetY = act.posY * Tilemap::TILE_SIZE - (f32)viewport::height() * 0.5f;
                    m_cameraPanDuration = act.duration;
                    m_cameraPanTimer = 0;
                    break;
                case MapActionType::CameraFollow:
                    m_cameraFollowPlayer = true;
                    m_cameraPanning = false;
                    break;
                case MapActionType::CameraZoom:
                    m_cameraZoomStart = m_cameraZoom;
                    m_cameraZoomTarget = act.value;
                    m_cameraZoomDuration = act.duration;
                    m_cameraZoomTimer = 0;
                    if (m_cameraZoomDuration <= 0)
                        m_cameraZoom = m_cameraZoomTarget; // Instant
                    break;
                case MapActionType::Wait:
                    m_waitingAction = true;
                    m_waitTimer = act.duration;
                    break;
                default:
                    break;
                case MapActionType::DestroyTile:
                {
                    int col = (int)act.posX;
                    int row = (int)act.posY;
                    m_tilemap.setCharAt(row, col, '.', m_tilemap.getCustomVisuals());
                    // Sincronizar wall colliders
                    m_wallColliders = m_tilemap.getColliders();
                    m_platformColliders = m_tilemap.getPlatformColliders();
                    std::cout << "[Gameplay2D] DestroyTile (" << col << "," << row << ")" << std::endl;
                    break;
                }
                case MapActionType::CreateTile:
                {
                    int col = (int)act.posX;
                    int row = (int)act.posY;
                    m_tilemap.setCharAt(row, col, act.entityChar, m_tilemap.getCustomVisuals());
                    m_wallColliders = m_tilemap.getColliders();
                    m_platformColliders = m_tilemap.getPlatformColliders();
                    std::cout << "[Gameplay2D] CreateTile (" << col << "," << row << ") = '" << act.entityChar << "'" << std::endl;
                    break;
                }
                case MapActionType::RunLua:
                {
                    std::string scriptDir = m_mapPath.empty() ? "" :
                        std::filesystem::path(m_mapPath).parent_path().string();
                    m_lua.executeFile(act.str1, scriptDir);
                    break;
                }
                case MapActionType::LoadMap:
                    m_pendingMapPath = act.str1;
                    m_pendingSpawnX = act.posX * Tilemap::TILE_SIZE;
                    m_pendingSpawnY = act.posY * Tilemap::TILE_SIZE;
                    m_transitioning = true;
                    m_fadeOutAlpha = 0;
                    std::cout << "[Gameplay2D] LoadMap → " << act.str1 << std::endl;
                    break;
                }
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // Ataque melee
        // ══════════════════════════════════════════════════════════════════

        TileAABB getAttackBox() const
        {
            f32 range = m_attackRange;
            f32 size  = m_ballSize * 0.8f;
            f32 ax = m_posX, ay = m_posY;

            switch (m_lastDir) {
                case Dir::Up:    ax += (m_ballSize-size)*0.5f; ay -= range; break;
                case Dir::Down:  ax += (m_ballSize-size)*0.5f; ay += m_ballSize; break;
                case Dir::Left:  ax -= range; ay += (m_ballSize-size)*0.5f; break;
                case Dir::Right: ax += m_ballSize; ay += (m_ballSize-size)*0.5f; break;
                default:         ax += m_ballSize; ay += (m_ballSize-size)*0.5f; break;
            }

            f32 w = (m_lastDir == Dir::Up || m_lastDir == Dir::Down) ? size : range;
            f32 h = (m_lastDir == Dir::Up || m_lastDir == Dir::Down) ? range : size;
            return { ax, ay, w, h };
        }

        void performAttack()
        {
            // Ranged: disparar proyectil en vez de melee
            if ((m_attackType == "ranged" || m_attackType == "rango") &&
                !m_playerProjectileTile.empty())
            {
                m_attackTimer = m_playerFireRate;
                spawnProjectile(m_playerProjectileTile, m_posX, m_posY, true);
                if (m_hasAudio) m_audio.playSound(Settings::defaultInteractSoundPath());
                return;
            }

            // Both: melee + disparo
            if (m_attackType == "both" || m_attackType == "ambos")
            {
                if (!m_playerProjectileTile.empty())
                    spawnProjectile(m_playerProjectileTile, m_posX, m_posY, true);
            }

            m_attackTimer = m_attackCooldown;
            m_attackVisualTimer = m_attackVisualDuration;

            TileAABB atkBox = getAttackBox();
            for (auto& enemy : m_enemies)
            {
                if (!enemy.alive || enemy.invulnerable) continue;
                if (atkBox.overlaps(enemy.box))
                {
                    enemy.hp -= m_attackDamage;
                    enemy.hitFlash = 0.15f;

                    // Knockback al enemigo (Tier 2)
                    {
                        f32 dx = enemy.box.x - m_posX;
                        f32 dy = enemy.box.y - m_posY;
                        f32 dist = std::sqrt(dx*dx + dy*dy);
                        if (dist > 0.01f)
                        {
                            enemy.knockVelX = (dx/dist) * 300.0f;
                            enemy.knockVelY = (dy/dist) * 300.0f;
                        }
                    }

                    if (m_hasAudio && m_attackDamage > 0 && enemy.hp > 0)
                        m_audio.playSound(Settings::defaultEnemyHitSoundPath());

                    std::cout << "[Gameplay2D] Ataque -> " << enemy.name
                              << " HP: " << enemy.hp << "/" << enemy.maxHP
                              << std::endl;

                    if (enemy.hp <= 0)
                    {
                        commitEnemyRemoval(enemy, EnemyRemovalCause::Defeated);
                        std::cout << "[Gameplay2D] " << enemy.name
                            << " derrotado!" << std::endl;
                    }
                }
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Bloques empujables
        // ══════════════════════════════════════════════════════════════════

        bool tryPushBlock(f32 hitX, f32 hitY, Dir pushDir)
        {
            // ¿Qué tile estamos empujando?
            int col = m_tilemap.pixelToCol(hitX + Tilemap::TILE_SIZE * 0.5f);
            int row = m_tilemap.pixelToRow(hitY + Tilemap::TILE_SIZE * 0.5f);
            char c = m_tilemap.getCharAt(row, col);

            // ¿Es empujable?
            const MapTileDef* td = nullptr;
            for (const auto& t : m_mapData.tileDefs)
                if (t.tile == c && t.type == MapTileType::Pushable) { td = &t; break; }
            if (!td) return false;

            // Calcular destino
            int destCol = col, destRow = row;
            switch (pushDir) {
            case Dir::Up:    destRow--; break;
            case Dir::Down:  destRow++; break;
            case Dir::Left:  destCol--; break;
            case Dir::Right: destCol++; break;
            default: return false;
            }

            // ¿Destino libre?
            char destChar = m_tilemap.getCharAt(destRow, destCol);
            if (destChar != '.' && destChar != '>' && destChar != 'P') return false;

            // ¿Hay alguna entidad en el destino? (enemigo, NPC, item)
            f32 destPx = destCol * Tilemap::TILE_SIZE;
            f32 destPy = destRow * Tilemap::TILE_SIZE;
            TileAABB destBox = { destPx, destPy, Tilemap::TILE_SIZE, Tilemap::TILE_SIZE };
            for (const auto& e : m_enemies)
                if (e.alive && e.box.overlaps(destBox)) return false;
            for (const auto& n : m_npcs)
                if (n.box.overlaps(destBox)) return false;

            // Empujar: mover tile
            m_tilemap.setCharAt(row, col, '.', m_tilemap.getCustomVisuals());
            m_tilemap.setCharAt(destRow, destCol, c, m_tilemap.getCustomVisuals());

            // Sincronizar colliders
            m_wallColliders = m_tilemap.getColliders();
            m_platformColliders = m_tilemap.getPlatformColliders();

            // Remover colliders de entidades (como en buildWallColliders)
            for (const auto& e : m_enemies)
                if (e.alive) removeColliderAt(e.box.x, e.box.y);
            for (const auto& it : m_items)
                if (!it.collected) removeColliderAt(it.box.x, it.box.y);
            for (const auto& tr : m_triggers)
                removeColliderAt(tr.box.x, tr.box.y);

            if (m_hasAudio) m_audio.playSound(Settings::defaultInteractSoundPath());

            std::cout << "[Gameplay2D] Push (" << col << "," << row
                << ") -> (" << destCol << "," << destRow << ")" << std::endl;

            // Chequear triggers bajo el bloque
            for (auto& tr : m_triggers)
            {
                if (tr.activated && tr.once) continue;
                if (tr.box.overlaps(destBox))
                {
                    if (!tr.condVar.empty())
                    {
                        std::string cv = getVar(tr.condVar);
                        bool pass = false;
                        if (tr.condOp == "==" || tr.condOp == "=") pass = (cv == tr.condVal);
                        else if (tr.condOp == "!=") pass = (cv != tr.condVal);
                        if (!pass) continue;
                    }
                    tr.activated = true;
                    executeActions(tr.onEnter, destPx, destPy);
                }
            }

            return true;
        }


        // ══════════════════════════════════════════════════════════════════
        // Enemigos — Movimiento
        // ══════════════════════════════════════════════════════════════════

        struct Gameplay2DLocomotionObstructionHit
        {
            TowerRuntime* blocker = nullptr;
            float time = 1.0f;
        };

        TowerRuntime* resolveLocomotionBlocker(
            const Gameplay2DActorAddress& address)
        {
            if (!address.valid() ||
                address.domain != Gameplay2DActorDomain::Placed)
            {
                return nullptr;
            }

            const auto it = std::find_if(
                m_towers.begin(), m_towers.end(),
                [&](const TowerRuntime& tower) {
                    return tower.id == address.localId &&
                        tower.isDeployed() &&
                        tower.obstructsLocomotion;
                });
            return it == m_towers.end() ? nullptr : &*it;
        }

        Gameplay2DLocomotionObstructionHit findLocomotionObstruction(
            EnemyRuntime& enemy,
            const TileAABB& proposed)
        {
            TowerRuntime* retained = resolveLocomotionBlocker(
                enemy.locomotionEncounter.blockerAddress);
            if (retained)
            {
                const auto sweep = sweepGameplay2DAABB(
                    enemy.box, proposed, retained->box,
                    Gameplay2DSweepBoundary::Closed);
                if (sweep.hit)
                    return { retained, sweep.time };
            }

            enemy.locomotionEncounter.blockerAddress = {};

            Gameplay2DLocomotionObstructionHit best;
            Gameplay2DAddressedSweepHit bestAddressed;
            bool found = false;

            for (auto& tower : m_towers)
            {
                if (!tower.isDeployed() || !tower.obstructsLocomotion)
                    continue;

                const auto sweep = sweepGameplay2DAABB(
                    enemy.box, proposed, tower.box,
                    Gameplay2DSweepBoundary::Closed);
                if (!sweep.hit)
                    continue;

                const Gameplay2DAddressedSweepHit candidate = {
                    { Gameplay2DActorDomain::Placed, tower.id },
                    sweep.time
                };
                if (!found ||
                    gameplay2DAddressedSweepHitPrecedes(
                        candidate, bestAddressed))
                {
                    found = true;
                    best = { &tower, sweep.time };
                    bestAddressed = candidate;
                }
            }

            if (found)
            {
                enemy.locomotionEncounter.blockerAddress =
                    bestAddressed.address;
            }
            return best;
        }

        void commitStoppedLocomotion(
            EnemyRuntime& enemy,
            const TileAABB& proposed)
        {
            const auto obstruction =
                findLocomotionObstruction(enemy, proposed);
            if (!obstruction.blocker)
            {
                enemy.box = proposed;
                return;
            }

            const float dx = proposed.x - enemy.box.x;
            const float dy = proposed.y - enemy.box.y;
            enemy.box.x += dx * obstruction.time;
            enemy.box.y += dy * obstruction.time;
        }

        void updateEnemies(f64 dt)
        {
            for (auto& e : m_enemies)
            {
                if (!e.alive) continue;

                // Decay de flash SIEMPRE, incluso si el enemigo está
                // clavado atacando una torre (Tier 4C.3)
                if (e.hitFlash > 0)
                    e.hitFlash -= (f32)dt;

                // ── Tier 4C.3: Kingdom Rush — ataque a torres ──
                // Si el enemigo tiene attacksTowers y hay una torre cerca,
                // se detiene y la golpea cada towerAttackRate. Este bloque
                // gobierna locomoción; sus habilidades continúan después en
                // updateEnemyAbilities con relojes independientes.
                if (e.alive && e.attacksTowers && !m_towers.empty())
                {
                    // Validar blanco actual (puede haber muerto ya)
                    TowerRuntime* target = nullptr;
                    if (e.attackingTowerId > 0)
                    {
                        for (auto& t : m_towers)
                            if (t.isDeployed() &&
                                t.id == e.attackingTowerId)
                            {
                                target = &t; break;
                            }
                        if (!target) e.attackingTowerId = -1;   // torre ya murió
                    }

                    // Si no tengo blanco, buscar uno nuevo
                    if (!target)
                    {
                        target = findNearestTowerInRange(e, e.towerAttackRange);
                        if (target)
                        {
                            e.attackingTowerId = target->id;
                            e.towerAttackTimer = 0;   // primer golpe: al instante
                        }
                    }

                    // Si hay blanco, atacar y saltar el resto del frame
                    if (target)
                    {
                        e.towerAttackTimer -= (f32)dt;
                        if (e.towerAttackTimer <= 0)
                        {
                            damageTower(*target, e.damage);
                            e.towerAttackTimer = e.towerAttackRate;
                        }
                        // Knockback residual decae aunque no nos movamos
                        float decay = 1.0f - 8.0f * (f32)dt;
                        if (decay < 0) decay = 0;
                        e.knockVelX *= decay; e.knockVelY *= decay;
                        continue;   // NO mover ni chocar con Base este frame
                    }
                }

                if (!e.alive) continue;

                // Hit flash decay
                if (e.hitFlash > 0)
                    e.hitFlash -= (f32)dt;

                switch (e.movement.type)
                {
                case MapMovementType::PatrolH:
                {
                    f32 prevX = e.box.x;
                    e.box.x += e.speed * (f32)dt * e.patrolDir;

                    // Límite de patrulla
                    f32 dist = e.box.x - e.originX;
                    if (dist > e.patrolRange)
                    {
                        e.box.x = e.originX + e.patrolRange; e.patrolDir = -1;
                    }
                    else if (dist < -e.patrolRange)
                    {
                        e.box.x = e.originX - e.patrolRange; e.patrolDir = 1;
                    }

                    // Colisión con paredes → invertir y restaurar
                    if (m_tilemap.isSolidInArea(e.box.x, e.box.y, e.box.w, e.box.h))
                    {
                        e.box.x = prevX; e.patrolDir *= -1;
                    }
                    break;
                }
                case MapMovementType::PatrolV:
                {
                    f32 prevY = e.box.y;
                    e.box.y += e.speed * (f32)dt * e.patrolDir;

                    f32 dist = e.box.y - e.originY;
                    if (dist > e.patrolRange)
                    {
                        e.box.y = e.originY + e.patrolRange; e.patrolDir = -1;
                    }
                    else if (dist < -e.patrolRange)
                    {
                        e.box.y = e.originY - e.patrolRange; e.patrolDir = 1;
                    }

                    if (m_tilemap.isSolidInArea(e.box.x, e.box.y, e.box.w, e.box.h))
                    {
                        e.box.y = prevY; e.patrolDir *= -1;
                    }
                    break;
                }
                case MapMovementType::Follow:
                {
                    f32 dx = m_posX - e.box.x;
                    f32 dy = m_posY - e.box.y;
                    f32 dist = std::sqrt(dx*dx + dy*dy);
                    float detRange = (e.range > 0) ? e.range : e.movement.param;
                    if (dist < detRange && dist > 2.0f)
                    {
                        f32 nx = dx/dist, ny = dy/dist;
                        f32 mx = nx * e.speed * (f32)dt;
                        f32 my = ny * e.speed * (f32)dt;

                        // Conservar la respuesta legacy a tiles, pero calcularla
                        // sobre una propuesta: O3/R3 decide antes del commit.
                        TileAABB proposed = e.box;
                        proposed.x += mx;
                        if (m_tilemap.isSolidInArea(
                                proposed.x, proposed.y,
                                proposed.w, proposed.h))
                        {
                            proposed.x = e.box.x;
                        }

                        proposed.y += my;
                        if (m_tilemap.isSolidInArea(
                                proposed.x, proposed.y,
                                proposed.w, proposed.h))
                        {
                            proposed.y = e.box.y;
                        }

                        commitStoppedLocomotion(e, proposed);
                    }
                    else
                        e.locomotionEncounter.blockerAddress = {};
                    break;
                }
                case MapMovementType::Flee:
                {
                    f32 dx = e.box.x - m_posX;
                    f32 dy = e.box.y - m_posY;
                    f32 dist = std::sqrt(dx*dx + dy*dy);
                    float detRange = (e.range > 0) ? e.range : e.movement.param;
                    if (dist < detRange && dist > 2.0f)
                    {
                        f32 nx = dx/dist, ny = dy/dist;
                        e.box.x += nx * e.speed * (f32)dt;
                        e.box.y += ny * e.speed * (f32)dt;
                    }
                    break;
                }
                case MapMovementType::Bounce:
                {
                    // Dirección inicial (diagonal)
                    if (e.patrolDir == 1) { e.box.x += e.speed*(f32)dt; e.box.y += e.speed*(f32)dt*0.7f; }
                    else { e.box.x -= e.speed*(f32)dt; e.box.y -= e.speed*(f32)dt*0.7f; }

                    bool bounced = false;
                    if (m_tilemap.isSolidInArea(e.box.x, e.box.y, e.box.w, e.box.h))
                    { e.patrolDir *= -1; bounced = true; }

                    if (e.box.x <= 0 || e.box.x >= m_tilemap.worldW() - e.box.w ||
                        e.box.y <= 0 || e.box.y >= m_tilemap.worldH() - e.box.h)
                    { if (!bounced) e.patrolDir *= -1; }

                    e.box.x = std::max(0.f, std::min(e.box.x, m_tilemap.worldW()-e.box.w));
                    e.box.y = std::max(0.f, std::min(e.box.y, m_tilemap.worldH()-e.box.h));
                    break;
                }
                case MapMovementType::Circle:
                {
                    float angle = (f32)m_totalTime * e.speed * 0.02f;
                    float radius = e.movement.param * Tilemap::TILE_SIZE;
                    e.box.x = e.originX + std::cos(angle) * radius;
                    e.box.y = e.originY + std::sin(angle) * radius;
                    break;
                }
                case MapMovementType::Path:
                {
                    if (e.pathPoints.size() < 2 || e.pathComplete)
                    {
                        e.locomotionEncounter.blockerAddress = {};
                        break;
                    }

                    auto [tx, ty] = e.pathPoints[e.pathIndex];
                    f32 dx = tx - e.box.x;
                    f32 dy = ty - e.box.y;
                    f32 dist = std::sqrt(dx * dx + dy * dy);

                    if (dist < 4.0f) // Llegó al punto
                    {
                        e.locomotionEncounter.blockerAddress = {};
                        e.box.x = tx; e.box.y = ty;

                        if (e.pathTraversal == MapRouteTraversal::Loop)
                        {
                            e.pathIndex = (e.pathIndex + 1) % (int)e.pathPoints.size();
                        }
                        else if (e.pathTraversal == MapRouteTraversal::PingPong)
                        {
                            e.pathIndex += e.pathDir;
                            if (e.pathIndex >= (int)e.pathPoints.size())
                            {
                                e.pathIndex = (int)e.pathPoints.size() - 2; e.pathDir = -1;
                            }
                            else if (e.pathIndex < 0)
                            {
                                e.pathIndex = 1; e.pathDir = 1;
                            }
                        }
                        else // Once: el ultimo waypoint completa el recorrido.
                        {
                            if (e.pathIndex + 1 >= (int)e.pathPoints.size())
                                e.pathComplete = true;
                            else
                                e.pathIndex++;
                        }
                    }
                    else
                    {
                        f32 nx = dx / dist, ny = dy / dist;
                        TileAABB proposed = e.box;
                        proposed.x += nx * e.speed * (f32)dt;
                        proposed.y += ny * e.speed * (f32)dt;
                        commitStoppedLocomotion(e, proposed);
                    }
                    break;
                }
                default:
                    break;
                }

                // ── Knockback del enemigo (decay) ──
                if (std::abs(e.knockVelX) > 1.0f || std::abs(e.knockVelY) > 1.0f)
                {
                    f32 prevEX = e.box.x, prevEY = e.box.y;
                    e.box.x += e.knockVelX * (f32)dt;
                    e.box.y += e.knockVelY * (f32)dt;

                    // Colisión con paredes durante knockback
                    if (m_tilemap.isSolidInArea(e.box.x, e.box.y, e.box.w, e.box.h))
                    {
                        e.box.x = prevEX; e.box.y = prevEY; e.knockVelX = 0; e.knockVelY = 0;
                    }

                    float decay = 1.0f - 8.0f * (f32)dt;
                    if (decay < 0) decay = 0;
                    e.knockVelX *= decay;
                    e.knockVelY *= decay;
                }

                // ── Colisión con tile Base (Tier 4C Tower Defense) ──
                // Detecta enemigos que TOCAN el tile base (sin necesidad
                // de pisarlo). El contacto cuenta como llegada: retira al
                // enemigo, pero no ejecuta al_morir ni enemy_death.
                if (m_baseMaxHP > 0 && !m_baseDestroyed && e.damage > 0)
                {
                    bool hitBase = false;
                    // Escanear tiles adyacentes con tolerancia de 2px.
                    // Así el enemigo "golpea" la base cuando está pegado
                    // a ella (sin necesidad de overlap real, ya que el
                    // sistema de colisión no le deja pisarla).
                    const f32 tol = 2.0f;
                    TileAABB eExpanded = {
                        e.box.x - tol, e.box.y - tol,
                        e.box.w + tol * 2, e.box.h + tol * 2
                    };
                    const int c0 = m_tilemap.pixelToCol(eExpanded.x);
                    const int c1 = m_tilemap.pixelToCol(eExpanded.x + eExpanded.w - 0.01f);
                    const int r0 = m_tilemap.pixelToRow(eExpanded.y);
                    const int r1 = m_tilemap.pixelToRow(eExpanded.y + eExpanded.h - 0.01f);

                    for (int r = r0; r <= r1 && !hitBase; r++)
                    {
                        for (int c = c0; c <= c1 && !hitBase; c++)
                        {
                            char ch = m_tilemap.getCharAt(r, c);
                            for (const auto& td : m_mapData.tileDefs)
                            {
                                if (td.type == MapTileType::Base && td.tile == ch)
                                {
                                    hitBase = true; break;
                                }
                            }
                        }
                    }

                    if (hitBase)
                    {
                        std::cout << "[Gameplay2D] " << e.name
                            << " alcanzó la base!" << std::endl;
                        // La retirada se compromete antes de los hooks de Base:
                        // un callback reentrante no puede convertir la llegada
                        // en una muerte y otorgar recompensas por accidente.
                        commitEnemyRemoval(e, EnemyRemovalCause::ReachedBase);
                        damageBase(e.damage);
                    }
                }
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Habilidades de enemigo — segundo owner
        // ══════════════════════════════════════════════════════════════════
        //
        // Esta fase comparte instancia, resolvedor y selector con las torres,
        // no su conjunto de elegibles. El dialecto sin adquisición conserva
        // exactamente su adapter jugador-only. Una habilidad que declara
        // adquisición ve jugador + colocados desde el mismo snapshot.
        // Sus i-frames evitan daño, no adquisición.
        //
        // Igual que en torres: avanzar oportunidades -> decidir -> comprometer
        // todos los relojes/targets -> recién entonces publicar efectos.

        enum class Gameplay2DDirectDamagePublicationKind
        {
            Player,
            EnemyDefeated,
            TowerDestroyed
        };

        struct Gameplay2DDirectDamagePublication
        {
            Gameplay2DDirectDamagePublicationKind kind =
                Gameplay2DDirectDamagePublicationKind::Player;
            int amount = 0;
            std::string name;
            int id = -1;
            float x = 0.0f;
            float y = 0.0f;
            std::vector<MapAction> actions;
            bool playerDefeated = false;
        };

        void applyDirectDamageBatch(
            const std::vector<Gameplay2DDirectDamageEffect>& effects)
        {
            std::vector<Gameplay2DDirectDamagePublication> publications;
            publications.reserve(effects.size());

            // Primero se compromete el lote mecánico completo. Un receptor
            // eliminado por un efecto anterior ya no acepta los posteriores.
            for (const auto& effect : effects)
            {
                if (!effect.sourceAddress.valid() ||
                    !effect.targetAddress.valid() ||
                    effect.amount <= 0)
                {
                    continue;
                }

                if (effect.targetAddress.domain ==
                    Gameplay2DActorDomain::Player)
                {
                    if (effect.targetAddress.localId != 0 ||
                        m_playerHP <= 0 ||
                        m_invincibleTimer > 0.0f)
                    {
                        continue;
                    }

                    m_playerHP =
                        std::max(0, m_playerHP - effect.amount);
                    m_invincibleTimer = m_invincibleDuration;
                    m_cameraShakeTimer = 0.2f;

                    Gameplay2DDirectDamagePublication publication;
                    publication.kind =
                        Gameplay2DDirectDamagePublicationKind::Player;
                    publication.amount = effect.amount;
                    publication.playerDefeated = m_playerHP <= 0;
                    publications.push_back(std::move(publication));
                    continue;
                }

                if (effect.targetAddress.domain ==
                    Gameplay2DActorDomain::Enemy)
                {
                    const auto enemy = std::find_if(
                        m_enemies.begin(), m_enemies.end(),
                        [&](const EnemyRuntime& candidate) {
                            return candidate.id ==
                                effect.targetAddress.localId;
                        });
                    if (enemy == m_enemies.end() ||
                        !enemy->alive || enemy->invulnerable)
                    {
                        continue;
                    }

                    enemy->hp -= effect.amount;
                    enemy->hitFlash = 0.15f;
                    if (enemy->hp > 0)
                        continue;

                    enemy->hp = 0;
                    enemy->alive = false;

                    Gameplay2DDirectDamagePublication publication;
                    publication.kind =
                        Gameplay2DDirectDamagePublicationKind::EnemyDefeated;
                    publication.name = enemy->name;
                    publication.id = enemy->id;
                    publication.x = enemy->box.x;
                    publication.y = enemy->box.y;
                    publication.actions = enemy->onDeath;
                    publications.push_back(std::move(publication));
                    continue;
                }

                if (effect.targetAddress.domain ==
                    Gameplay2DActorDomain::Placed)
                {
                    const auto tower = std::find_if(
                        m_towers.begin(), m_towers.end(),
                        [&](const TowerRuntime& candidate) {
                            return candidate.id ==
                                effect.targetAddress.localId;
                        });
                    if (tower == m_towers.end() ||
                        !tower->isDeployed() || tower->maxHP <= 0)
                    {
                        continue;
                    }

                    tower->hp -= effect.amount;
                    tower->hitFlash = 0.15f;
                    if (tower->hp > 0)
                        continue;

                    tower->hp = 0;
                    tower->alive = false;

                    Gameplay2DDirectDamagePublication publication;
                    publication.kind =
                        Gameplay2DDirectDamagePublicationKind::TowerDestroyed;
                    publication.name = tower->name;
                    publication.id = tower->id;
                    publication.x = tower->box.x;
                    publication.y = tower->box.y;
                    publication.actions = tower->onDestroy;
                    publications.push_back(std::move(publication));
                }
            }

            // Sólo después del lote mecánico se publican acciones y callbacks.
            for (const auto& publication : publications)
            {
                if (publication.kind ==
                    Gameplay2DDirectDamagePublicationKind::Player)
                {
                    fireHook("player_damage", publication.amount);
                    if (publication.playerDefeated)
                        pushLevelEnd(false);
                    continue;
                }

                executeActions(
                    publication.actions,
                    publication.x,
                    publication.y);

                if (publication.kind ==
                    Gameplay2DDirectDamagePublicationKind::EnemyDefeated)
                {
                    fireHook(
                        "enemy_death",
                        publication.name,
                        publication.id);
                }
                else
                {
                    fireHook(
                        "tower_destroyed",
                        publication.name,
                        publication.id);
                    std::cout << "[Gameplay2D] Torre destruida: '"
                              << publication.name << "' id="
                              << publication.id << std::endl;
                }
            }
        }

        void updateEnemyAbilities(f64 dt)
        {
            constexpr int playerTargetId = 0;

            Gameplay2DTargetCandidate playerCandidate;
            playerCandidate.address = {
                Gameplay2DActorDomain::Player,
                playerTargetId
            };
            playerCandidate.centerX = m_posX + m_ballSize * 0.5f;
            playerCandidate.centerY = m_posY + m_ballSize * 0.5f;
            playerCandidate.currentHp = m_playerHP;
            playerCandidate.alive = m_playerHP > 0;
            playerCandidate.invulnerable = false;
            playerCandidate.relation =
                Gameplay2DTargetRelation::Opposing;
            if (m_playerMaxHP > 0)
            {
                playerCandidate.capabilities.push_back(
                    Gameplay2DTargetCapability::Damageable);
            }

            std::vector<Gameplay2DTargetCandidate> candidates;
            candidates.reserve(1 + m_towers.size());
            candidates.push_back(playerCandidate);
            for (const auto& tower : m_towers)
            {
                Gameplay2DTargetCandidate candidate;
                candidate.address = {
                    Gameplay2DActorDomain::Placed,
                    tower.id
                };
                candidate.centerX =
                    tower.box.x + tower.box.w * 0.5f;
                candidate.centerY =
                    tower.box.y + tower.box.h * 0.5f;
                candidate.currentHp = tower.hp;
                candidate.alive = tower.isDeployed();
                candidate.invulnerable = false;
                candidate.relation =
                    Gameplay2DTargetRelation::Opposing;
                if (tower.maxHP > 0)
                {
                    candidate.capabilities.push_back(
                        Gameplay2DTargetCapability::Damageable);
                }
                candidates.push_back(std::move(candidate));
            }

            std::vector<Gameplay2DTargetRequest> requests;

            for (auto& enemy : m_enemies)
            {
                if (!enemy.alive)
                    continue;

                for (auto& use : enemy.abilityUses)
                {
                    const float activityInterval = use.activityInterval();
                    if (activityInterval <= 0.0f ||
                        !gameplay2DAbilityHasExecutableEffect(
                            use.effective) ||
                        gameplay2DAbilityHasConflictingEffects(
                            use.effective))
                        continue;

                    use.fireTimer += (f32)dt;
                    if (enemy.activityInhibitions.blocks(use.useId))
                    {
                        // I1/Enemy impide commits futuros sin pausar al actor
                        // ni acumular oportunidades retroactivas.
                        use.fireTimer = std::min(
                            use.fireTimer, activityInterval);
                        continue;
                    }
                    if (!gameplay2DActivityReady(
                            use.fireTimer, activityInterval))
                        continue;

                    Gameplay2DTargetRequest request;
                    request.actorId = enemy.id;
                    request.useId = use.useId;
                    request.sourceX =
                        enemy.box.x + enemy.box.w * 0.5f;
                    request.sourceY =
                        enemy.box.y + enemy.box.h * 0.5f;
                    request.range = use.effective.range;
                    request.preference = use.activityPreference();
                    request.acquisition =
                        use.effective.acquisition;
                    const auto binding =
                        use.effective.acquisition.binding.value_or(
                            Gameplay2DTargetBinding::Independent);
                    if (binding == Gameplay2DTargetBinding::
                            CurrentLocomotionBlocker)
                    {
                        request.requiredTargetAddress =
                            enemy.locomotionEncounter.blockerAddress;
                    }
                    request.coordination =
                        use.effective.coordination;
                    request.directDamage =
                        use.effective.directDamage.value_or(0);

                    if (!hasGameplay2DAcquisition(
                            use.effective.acquisition))
                    {
                        // Compatibilidad: el dialecto sin adquisicion sigue
                        // viendo solo Player, pero participa del unico orden
                        // y ledger de esta fase.
                        request.requiredTargetDomain =
                            Gameplay2DActorDomain::Player;
                    }
                    requests.push_back(std::move(request));
                }
            }

            const std::vector<Gameplay2DTargetDecision> decisions =
                selectGameplay2DTargets(requests, candidates);

            struct CommittedEnemyShot
            {
                float originX = 0.0f;
                float originY = 0.0f;
                float directionX = 0.0f;
                float directionY = 0.0f;
                std::string projectile;
            };
            std::vector<CommittedEnemyShot> shots;
            shots.reserve(decisions.size());
            std::vector<Gameplay2DDirectDamageEffect> directEffects;
            directEffects.reserve(decisions.size());

            for (const auto& decision : decisions)
            {
                auto enemyIt = std::find_if(
                    m_enemies.begin(), m_enemies.end(),
                    [&](const EnemyRuntime& enemy) {
                        return enemy.id == decision.actorId;
                    });
                if (enemyIt == m_enemies.end() || !enemyIt->alive)
                    continue;

                auto useIt = std::find_if(
                    enemyIt->abilityUses.begin(),
                    enemyIt->abilityUses.end(),
                    [&](const Gameplay2DAbilityRuntime& use) {
                        return use.useId == decision.useId;
                    });
                if (useIt == enemyIt->abilityUses.end())
                    continue;

                useIt->targetAddress = decision.targetAddress;
                if (!decision.targetAddress.valid())
                {
                    useIt->fireTimer = std::min(
                        useIt->fireTimer,
                        useIt->activityInterval());
                    continue;
                }

                const auto target = std::find_if(
                    candidates.begin(), candidates.end(),
                    [&](const Gameplay2DTargetCandidate& candidate) {
                        return candidate.address ==
                            decision.targetAddress;
                    });
                if (target == candidates.end())
                {
                    useIt->targetAddress = {};
                    useIt->fireTimer = std::min(
                        useIt->fireTimer,
                        useIt->activityInterval());
                    continue;
                }

                useIt->fireTimer = 0.0f;

                if (gameplay2DAbilityHasDirectDamage(
                        useIt->effective))
                {
                    directEffects.push_back({
                        { Gameplay2DActorDomain::Enemy, enemyIt->id },
                        useIt->useId,
                        decision.targetAddress,
                        *useIt->effective.directDamage
                    });
                    continue;
                }

                CommittedEnemyShot shot;
                shot.originX = enemyIt->box.x;
                shot.originY = enemyIt->box.y;
                shot.projectile = useIt->effective.projectile;

                if (useIt->implicitLegacy)
                {
                    // El dialecto Tier 2 apuntaba top-left a top-left.
                    shot.directionX = m_posX - enemyIt->box.x;
                    shot.directionY = m_posY - enemyIt->box.y;
                }
                else
                {
                    const float sourceX =
                        enemyIt->box.x + enemyIt->box.w * 0.5f;
                    const float sourceY =
                        enemyIt->box.y + enemyIt->box.h * 0.5f;
                    shot.directionX =
                        target->centerX - sourceX;
                    shot.directionY =
                        target->centerY - sourceY;
                }
                shots.push_back(std::move(shot));
            }

            for (const auto& shot : shots)
            {
                spawnProjectile(
                    shot.projectile,
                    shot.originX,
                    shot.originY,
                    /*friendly=*/false,
                    shot.directionX,
                    shot.directionY);
            }

            applyDirectDamageBatch(directEffects);
        }

        // ══════════════════════════════════════════════════════════════════
        // Marcadores espaciales neutrales
        // ══════════════════════════════════════════════════════════════════
        //
        // La detección se separa de la publicación Lua:
        //   1. se fija qué actores pertenecen al tick;
        //   2. se mueve el mundo;
        //   3. se confirma y guarda el lote completo;
        //   4. recién entonces se publican los hechos en orden explícito.
        //
        // Esto evita callbacks reentrantes dentro del vector de enemigos y
        // garantiza que una regla disparada por el primer cruce no pueda borrar
        // retroactivamente otro cruce ya confirmado en el mismo tick.

        std::vector<SpatialEnemySnapshot> snapshotSpatialEnemies() const
        {
            std::vector<SpatialEnemySnapshot> snapshots;
            snapshots.reserve(m_enemies.size());
            for (const auto& enemy : m_enemies)
                if (enemy.alive)
                    snapshots.push_back({ enemy.id, enemy.box });

            std::sort(snapshots.begin(), snapshots.end(),
                [](const SpatialEnemySnapshot& a,
                   const SpatialEnemySnapshot& b) {
                    return a.enemyId < b.enemyId;
                });
            return snapshots;
        }

        static bool sweptAABBOverlaps(const TileAABB& from,
                                      const TileAABB& to,
                                      const TileAABB& marker)
        {
            return sweepGameplay2DAABB(
                from, to, marker,
                Gameplay2DSweepBoundary::Open).hit;
        }

        void commitSpatialMarkerEntries(
            const std::vector<SpatialEnemySnapshot>& enemiesBeforeMove)
        {
            std::vector<SpatialMarkerEntry> entries;

            for (auto& marker : m_spatialMarkers)
            {
                std::set<int> nowInside;

                for (const auto& before : enemiesBeforeMove)
                {
                    const EnemyRuntime* actor = nullptr;
                    for (const auto& enemy : m_enemies)
                    {
                        if (enemy.id == before.enemyId)
                        {
                            actor = &enemy;
                            break;
                        }
                    }

                    // Un actor que estaba vivo al comenzar el tick conserva el
                    // hecho espacial aunque otra regla (por ejemplo Base) lo
                    // retire después de moverlo durante ese mismo tick.
                    if (!actor) continue;

                    const bool endsInside = actor->box.overlaps(marker.box);
                    if (endsInside)
                        nowInside.insert(before.enemyId);

                    const bool wasInside =
                        marker.insideEnemyIds.find(before.enemyId) !=
                        marker.insideEnemyIds.end();
                    const bool crossed =
                        sweptAABBOverlaps(before.box, actor->box, marker.box);

                    // La presencia inicial solapada también es una entrada:
                    // el sweep confirma cruces completos entre observaciones,
                    // mientras el overlap final cubre desplazamiento cero.
                    if (!wasInside && (endsInside || crossed))
                    {
                        entries.push_back({ marker.id, before.enemyId });
                    }
                }

                // Salir elimina la pareja y rearma una futura reentrada.
                marker.insideEnemyIds.swap(nowInside);
            }

            std::sort(entries.begin(), entries.end(),
                [](const SpatialMarkerEntry& a,
                   const SpatialMarkerEntry& b) {
                    if (a.markerId != b.markerId)
                        return a.markerId < b.markerId;
                    return a.enemyId < b.enemyId;
                });

            for (const auto& entry : entries)
                fireHook("marker_enter", entry.markerId,
                         std::string("enemy"), entry.enemyId);
        }


        // ══════════════════════════════════════════════════════════════════
        // Colisiones jugador-enemigo (daño)
        // ══════════════════════════════════════════════════════════════════

        void checkEnemyCollisions(f64 dt)
        {
            if (m_playerHP <= 0 || m_invincibleTimer > 0) return;

            TileAABB pb = playerBox();
            for (auto& e : m_enemies)
            {
                if (!e.alive) continue;
                if (pb.overlaps(e.box))
                {
                    // Daño
                    m_playerHP -= e.damage;
                    m_invincibleTimer = m_invincibleDuration;
                    m_cameraShakeTimer = 0.2f;

                    std::cout << "[Gameplay2D] Jugador recibe " << e.damage
                              << " daño de " << e.name
                              << ". HP: " << m_playerHP << "/" << m_playerMaxHP
                              << std::endl;
                    fireHook("player_damage", e.damage);

                    // Knockback basado en velocidad (Tier 2)
                    if (e.knockback > 0)
                    {
                        f32 dx = m_posX - e.box.x;
                        f32 dy = m_posY - e.box.y;
                        f32 dist = std::sqrt(dx*dx + dy*dy);
                        if (dist > 0.01f)
                        {
                            m_knockVelX = (dx/dist) * e.knockback;
                            m_knockVelY = (dy/dist) * e.knockback;
                            // En side-scroll, knockback vertical más sutil
                            if (m_isSideScroll)
                            {
                                m_knockVelY = std::min(m_knockVelY, -200.0f);
                                m_gravityVelY = m_knockVelY;
                                m_knockVelY = 0;
                            }
                        }
                    }

                    if (m_playerHP <= 0)
                    {
                        pushLevelEnd(false);
                    }
                    break; // Solo un hit por frame
                }
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // Colisiones jugador-item (pickup)
        // ══════════════════════════════════════════════════════════════════

        void checkItemPickups()
        {
            TileAABB pb = playerBox();
            for (auto& item : m_items)
            {
                if (item.collected) continue;
                if (pb.overlaps(item.box))
                {
                    item.collected = true;

                    // Ejecutar acciones de pickup
                    executeActions(item.onPickup, item.box.x, item.box.y);

                    // Manejar set: hp especial (curar al jugador)
                    for (const auto& act : item.onPickup)
                    {
                        if (act.type == MapActionType::SetVar &&
                            act.varSet.var == "hp")
                        {
                            if (act.varSet.op == "+")
                            {
                                int heal = 0;
                                try { heal = std::stoi(act.varSet.val); } catch (...) {}
                                m_playerHP = std::min(m_playerHP + heal, m_playerMaxHP);
                                std::cout << "[Gameplay2D] Curado +" << heal
                                          << " HP: " << m_playerHP << std::endl;
                            }
                        }
                    }

                    std::cout << "[Gameplay2D] Item '" << item.name
                              << "' recogido." << std::endl;
                    fireHook("item_pickup", item.name);
                }
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // Triggers de área
        // ══════════════════════════════════════════════════════════════════

        void checkTriggers()
        {
            TileAABB pb = playerBox();
            for (auto& trig : m_triggers)
            {
                if (trig.once && trig.activated) continue;
                if (!pb.overlaps(trig.box)) continue;

                // Condición
                if (!trig.condVar.empty())
                {
                    std::string val = getVar(trig.condVar);
                    bool pass = false;
                    if (trig.condOp == "==" || trig.condOp == "=")
                        pass = (val == trig.condVal);
                    else if (trig.condOp == "!=")
                        pass = (val != trig.condVal);
                    else if (trig.condOp == ">=")
                    {
                        int a = 0, b = 0;
                        try { a = std::stoi(val); } catch (...) {}
                        try { b = std::stoi(trig.condVal); } catch (...) {}
                        pass = (a >= b);
                    }
                    else if (trig.condOp == "<=")
                    {
                        int a = 0, b = 0;
                        try { a = std::stoi(val); } catch (...) {}
                        try { b = std::stoi(trig.condVal); } catch (...) {}
                        pass = (a <= b);
                    }
                    if (!pass) continue;
                }

                trig.activated = true;
                executeActions(trig.onEnter, trig.box.x, trig.box.y);
                std::cout << "[Gameplay2D] Trigger '" << trig.tile
                          << "' activado." << std::endl;
                fireHook("trigger", std::string(1, trig.tile));
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // Base de Tower Defense (Tier 4C)
        // ══════════════════════════════════════════════════════════════════

        // Aplica daño a la base. Si HP llega a 0, dispara onDestroy y exit.
        // Idempotente: después de m_baseDestroyed no hace nada.
        void damageBase(int amount)
        {
            if (m_baseMaxHP <= 0 || m_baseDestroyed || amount <= 0) return;

            m_baseHP -= amount;
            if (m_baseHP < 0) m_baseHP = 0;
            m_baseHitFlash = 0.25f;
            m_cameraShakeTimer = std::max(m_cameraShakeTimer, 0.15f);

            std::cout << "[Gameplay2D] Base dañada -" << amount
                << " (HP: " << m_baseHP << "/" << m_baseMaxHP << ")"
                << std::endl;

            fireHook("base_damaged", amount, m_baseHP);

            if (m_baseHP <= 0)
            {
                m_baseDestroyed = true;

                // Ejecutar onDestroy del tile Base (centrado en un tile base)
                float bx = 0, by = 0;
                findBaseTilePos(bx, by);
                for (const auto& td : m_mapData.tileDefs)
                {
                    if (td.type == MapTileType::Base)
                    {
                        executeActions(td.onDestroy, bx, by);
                        break;
                    }
                }

                fireHook("base_destroyed");

                // La caída de una base con HP es siempre derrota (Tier 4D).
                // El hook base_destroyed de arriba corre primero para que
                // el Lua pueda reaccionar (logs, vars globales, etc.). Si
                // el creador quiere skippear LevelEndState y manejar el
                // fin desde Lua, puede llamar world.exit() en el hook —
                // m_exiting quedará true y pushLevelEnd será no-op.
                pushLevelEnd(false);
            }
        }

        // Busca el primer tile Base en el grid y retorna su posición en píxeles.
        // Útil para centrar efectos o draw overlays. Retorna false si no hay base.
        bool findBaseTilePos(float& outX, float& outY) const
        {
            for (int r = 0; r < m_tilemap.gridH(); r++)
            {
                for (int c = 0; c < m_tilemap.gridW(); c++)
                {
                    char ch = m_tilemap.getCharAt(r, c);
                    for (const auto& td : m_mapData.tileDefs)
                    {
                        if (td.type == MapTileType::Base && td.tile == ch)
                        {
                            outX = c * Tilemap::TILE_SIZE;
                            outY = r * Tilemap::TILE_SIZE;
                            return true;
                        }
                    }
                }
            }
            outX = 0; outY = 0;
            return false;
        }


        // ══════════════════════════════════════════════════════════════════
        // Spawn de entidades en runtime
        // ══════════════════════════════════════════════════════════════════

        void spawnEntities(char entityChar, int count,
            const std::string& posMode, float px, float py)
        {
            // Buscar definición en el MapData
            for (const auto& def : m_mapData.enemies)
            {
                if (def.tile != entityChar) continue;
                for (int n = 0; n < count; n++)
                {
                    EnemyRuntime e;
                    float spawnX, spawnY;
                    if (posMode == "at")
                    {
                        spawnX = px + n * 60;
                        spawnY = py;
                    }
                    else
                    {
                        auto [rx, ry] = randomFloorPos();
                        spawnX = rx;
                        spawnY = ry;
                    }
                    initEnemyFromDef(e, def, spawnX, spawnY);
                    m_enemies.push_back(e);
                }
                std::cout << "[Gameplay2D] Spawneados " << count << "x "
                    << def.name << std::endl;
                return;
            }

            for (const auto& def : m_mapData.items)
            {
                if (def.tile != entityChar) continue;
                for (int n = 0; n < count; n++)
                {
                    ItemRuntime it;
                    float spawnX, spawnY;
                    if (posMode == "at")
                    {
                        spawnX = px + n * 60;
                        spawnY = py;
                    }
                    else
                    {
                        auto [rx, ry] = randomFloorPos();
                        spawnX = rx;
                        spawnY = ry;
                    }
                    initItemFromDef(it, def, spawnX, spawnY);
                    m_items.push_back(it);
                }
                return;
            }
        }

        // ── Posición aleatoria en suelo — valida X e Y JUNTOS ──
        //
        // Nota: randomFloorX/Y antiguos validaban cada eje por separado,
        // lo que permitía combinaciones (X,Y) sobre tiles sólidos. Esta
        // función elige un tile completo y devuelve ambas coordenadas.
        //
        std::pair<float, float> randomFloorPos() const
        {
            for (int attempt = 0; attempt < 100; attempt++)
            {
                int col = rand() % m_tilemap.gridW();
                int row = rand() % m_tilemap.gridH();
                if (!m_tilemap.isSolidAt(col, row))
                    return { col * Tilemap::TILE_SIZE,
                             row * Tilemap::TILE_SIZE };
            }
            return { 120.0f, 120.0f }; // fallback
        }

        // ══════════════════════════════════════════════════════════════════
        // Oleadas
        // ══════════════════════════════════════════════════════════════════

        void updateWaves(f64 dt)
        {
            if (!m_mapData.waveConfig.defined || m_wavesComplete) return;

            auto& waves = m_mapData.waveConfig.waves;
            if (waves.empty()) return;

            // ¿Primera vez? Iniciar oleada 0
            if (!m_waveActive && m_currentWave == 0)
            {
                m_waveActive = true;
                m_waveSpawned = false;
                m_waveTimer = 0;
                m_waveAdvance = m_mapData.waveConfig.advance;
            }

            if (m_currentWave >= (int)waves.size())
            {
                m_wavesComplete = true;
                std::cout << "[Gameplay2D] Todas las oleadas completadas!" << std::endl;
                return;
            }

            auto& wave = waves[m_currentWave];

            // Delay antes de spawnear
            if (!m_waveSpawned)
            {
                m_waveTimer += (f32)dt;
                if (m_waveTimer < wave.delay) return;

                // ¡Spawnear!
                for (const auto& sp : wave.spawns)
                {
                    std::string mode = (sp.mode == "at") ? "at" : "random";
                    float px = sp.posX * Tilemap::TILE_SIZE;
                    float py = sp.posY * Tilemap::TILE_SIZE;
                    spawnEntities(sp.entityChar, sp.count, mode, px, py);
                }

                // Marcar los recién spawneados como wave enemies
                // (son los últimos que se agregaron a m_enemies)
                int totalSpawned = 0;
                for (const auto& sp : wave.spawns)
                    totalSpawned += sp.count;

                int startIdx = std::max(0, (int)m_enemies.size() - totalSpawned);
                for (int i = startIdx; i < (int)m_enemies.size(); i++)
                    m_enemies[i].isWaveEnemy = true;

                m_waveSpawned = true;
                m_waveTimer = 0;

                m_notifications.push_back({
                    "Oleada " + std::to_string(wave.number) + "/"
                    + std::to_string((int)waves.size()), 0, 2.5f });

                std::cout << "[Gameplay2D] Oleada " << wave.number
                    << " spawneada (" << totalSpawned << " enemigos)"
                    << std::endl;
                return;
            }

            // Chequear avance
            bool advance = false;

            if (m_waveAdvance == "eliminar" || m_waveAdvance == "eliminate")
            {
                // ¿Quedan wave enemies vivos?
                bool anyAlive = false;
                for (const auto& e : m_enemies)
                    if (e.isWaveEnemy && e.alive) { anyAlive = true; break; }
                if (!anyAlive) advance = true;
            }
            else if (m_waveAdvance == "tiempo" || m_waveAdvance == "time")
            {
                m_waveTimer += (f32)dt;
                if (m_waveTimer >= m_mapData.waveConfig.interval)
                    advance = true;
            }

            if (advance)
            {
                // Ejecutar al_completar de esta oleada
                executeActions(wave.onComplete);
                fireHook("wave_complete", m_currentWave + 1);

                // Limpiar flag de wave enemies muertos
                for (auto& e : m_enemies)
                    if (e.isWaveEnemy && !e.alive)
                        e.isWaveEnemy = false;

                m_currentWave++;
                m_waveSpawned = false;
                m_waveTimer = 0;

                if (m_currentWave >= (int)waves.size())
                {
                    m_wavesComplete = true;
                    std::cout << "[Gameplay2D] Todas las oleadas completadas!" << std::endl;
                }
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Condiciones de salida
        // ══════════════════════════════════════════════════════════════════

        void checkTalkAllExit()
        {
            for (const auto& npc : m_npcs)
                if (!npc.talked) return;
            std::cout << "[Gameplay2D] Salida: todos los NPCs hablados." << std::endl;
            triggerExit();
        }

        void checkReachExit()
        {
            TileAABB pb = playerBox();
            for (const auto& ep : m_tilemap.getExitPositions())
            {
                TileAABB exitBox = { ep.x, ep.y,
                                     Tilemap::TILE_SIZE, Tilemap::TILE_SIZE };
                if (pb.overlaps(exitBox))
                {
                    std::cout << "[Gameplay2D] Salida: tile '>' alcanzado." << std::endl;
                    triggerExit();
                    return;
                }
            }
        }

        void checkKillAllExit()
        {
            for (const auto& e : m_enemies)
                if (e.alive) return;
            if (m_enemies.empty()) return;
            std::cout << "[Gameplay2D] Salida: todos los enemigos derrotados." << std::endl;
            triggerExit();
        }

        void checkCollectAllExit()
        {
            for (const auto& i : m_items)
                if (!i.collected) return;
            if (m_items.empty()) return;
            std::cout << "[Gameplay2D] Salida: todos los items recogidos." << std::endl;
            triggerExit();
        }

        void checkVariableExit()
        {
            std::string val = getVar(m_mapData.exitVar);
            bool pass = false;
            if (m_mapData.exitOp == "==" || m_mapData.exitOp == "=")
                pass = (val == m_mapData.exitVal);
            else if (m_mapData.exitOp == "!=")
                pass = (val != m_mapData.exitVal);
            if (pass)
            {
                std::cout << "[Gameplay2D] Salida: variable "
                          << m_mapData.exitVar << " " << m_mapData.exitOp
                          << " " << m_mapData.exitVal << std::endl;
                triggerExit();
            }
        }

        void checkTimerExit(f64 dt)
        {
            m_exitTimerCurrent += (f32)dt;
            if (m_exitTimerCurrent >= m_mapData.exitTimer)
            {
                std::cout << "[Gameplay2D] Salida: timer " << m_mapData.exitTimer
                          << "s completado." << std::endl;
                triggerExit();
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Commit interno de retirada de enemigos
        // ══════════════════════════════════════════════════════════════════
        //
        // Solo una derrota mecánica ejecuta al_morir + enemy_death.
        // ReachedBase retira sin fingir una muerte. ScriptDestroyedLegacy
        // conserva el comportamiento histórico de world.destroy_enemy:
        // ejecuta al_morir, pero no dispara el hook enemy_death.

        void commitEnemyRemoval(EnemyRuntime& enemy, EnemyRemovalCause cause)
        {
            if (!enemy.alive) return;

            enemy.alive = false;

            if (cause == EnemyRemovalCause::Defeated ||
                cause == EnemyRemovalCause::ScriptDestroyedLegacy)
            {
                executeActions(enemy.onDeath, enemy.box.x, enemy.box.y);
            }

            if (cause == EnemyRemovalCause::Defeated)
            {
                fireHook("enemy_death", enemy.name, enemy.id);
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Fire Hook — dispara callbacks Lua registrados con world.on()
        // Lock simple: hooks no disparan otros hooks para evitar recursión.
        // ══════════════════════════════════════════════════════════════════

        template<typename... Args>
        void fireHook(const std::string& event, Args&&... args)
        {
            if (m_insideHook)
            {
                // C++ sigue normal, solo se skipea el callback Lua
                return;
            }
            auto it = m_hooks.find(event);
            if (it == m_hooks.end() || it->second.empty()) return;

            m_insideHook = true;
            for (auto& fn : it->second)
            {
                auto result = fn(std::forward<Args>(args)...);
                if (!result.valid())
                {
                    sol::error err = result;
                    std::cerr << "[Lua] Hook '" << event
                        << "' error: " << err.what() << std::endl;
                }
            }
            m_insideHook = false;
        }

        // ══════════════════════════════════════════════════════════════════
        // CP8 — el aviso del polling, UNA VEZ POR NOMBRE
        // ══════════════════════════════════════════════════════════════════
        //
        // **Warn-once y no siempre, y es la decisión OPUESTA a la del gate del
        // teclado (fork 6a) por la razón opuesta.** Aquel sitio corre una vez
        // por pulsación física, y ahí medí que el volumen era chico. Éste lo
        // llama el hook `update` de Lua: **60 veces por segundo**. Avisar
        // siempre serían 60 líneas por segundo — justo el desastre que
        // `VOZ-AHOGADA` describe, fabricado por nosotros.
        //
        // Mismo canal que el gate igual: `stderr`, no `stdout` (fork 6a).
        //
        // ⚠ Es la PRIMERA maquinaria de warn-once del repo (medido al firmar
        // el fork: cero precedentes). Son tres líneas a propósito — si algún
        // día hace falta en otro lado, esto se sube, no se copia.
        void avisarNombreLuaDesconocido(const char* fn,
                                        const std::string& nombre,
                                        const char* devuelve)
        {
            const std::string clave = std::string(fn) + "/" + nombre;
            if (m_avisadosLua.count(clave)) return;
            m_avisadosLua.insert(clave);

            std::cerr << "[Gameplay2D] world." << fn << "('" << nombre
                      << "'): no conozco ese nombre en el dominio 'juego'. "
                      << "Devuelvo " << devuelve << ".\n"
                      << "  Los ejes son 'mover_x'/'move_x' y "
                      << "'mover_y'/'move_y'; las acciones son las del bloque "
                      << "`teclas juego {}`.\n"
                      << "  (Se avisa una sola vez por nombre.)" << std::endl;
        }

        // ══════════════════════════════════════════════════════════════════
        // CP8 / fork 11b — la voz del gate, y CUÁNTAS veces
        // ══════════════════════════════════════════════════════════════════
        //
        // Una causa **permanente** (la config del `.eclmap`) avisa una vez por
        // verbo; una **transitoria** (el momento) avisa siempre, porque ahí
        // cada ocurrencia es el dato y callar se leería como *«esta vez sí
        // salió»*.
        //
        // Lo midió un caller real: sobre `test_td_completo.eclmap` —que no
        // declara `puede_atacar`— el `SPACE` imprimía la misma línea **siete
        // veces en doce teclas**. En el mapa vacío de la sonda no aparecía.
        void emitirVozDelGate(const std::string& verbo,
                              eclips::G2DGateCausa causa)
        {
            const std::string voz = eclips::g2dVozDelGate(verbo, causa);
            if (voz.empty()) return;   // `Ocurrio` / `NoRuteado`

            if (eclips::g2dCausaEsPermanente(causa))
            {
                const std::string clave = "gate_permanente/" + verbo;
                if (m_avisadosLua.count(clave)) return;
                m_avisadosLua.insert(clave);
            }

            std::cerr << voz << std::endl;
        }

        // ══════════════════════════════════════════════════════════════════
        // CP8 / fork 9c — la «deprecación suave», ACOTADA POR LA MEDICIÓN
        // ══════════════════════════════════════════════════════════════════
        //
        // El §11 del contrato firmó *«`key_press` → deprecación suave: los
        // scripts nuevos usan acciones»*. **Medido, eso es falso como regla
        // general:** los TRES callers vivos escuchan `R`, `T`, `1`, `2`, `O`,
        // ninguna es un verbo del dominio, y `on("action")` dispara CERO veces
        // para ellos (`harness_juego` 9.8/9.9). Deprecar el canal entero
        // mandaría al creador a un hook que no puede recibir su input — y
        // dejaría sin canal a las ~150 teclas que la tabla del motor no nombra,
        // empezando por la tecla Windows.
        //
        // Lo que SÍ es cierto es el caso estrecho: cuando la tecla que llega a
        // `key_press` **sí** es un verbo, escucharla por tecla es un bug
        // latente — se rompe cuando el jugador remapea. `ECLIPSCRIPT_DOCS` ya
        // lo documenta; esto lo dice en el momento en vez de esperar a que lo
        // lean.
        //
        // **Calla si el script ya escucha `action`.** La heurística es *«no
        // sabe que el canal existe»*, no *«lo está usando mal»*: quien usa los
        // dos a propósito no necesita el empujón.
        void avisarTeclaEsVerbo(const std::string& tecla,
                                const std::vector<eclips::G2DVerboCaido>& caidos)
        {
            if (caidos.empty())                              return;
            if (m_hooks.find("key_press") == m_hooks.end())   return;
            if (m_hooks.find("action")    != m_hooks.end())   return;

            const std::string clave = "key_press_verbo/" + tecla;
            if (m_avisadosLua.count(clave)) return;
            m_avisadosLua.insert(clave);

            std::string verbos;
            for (const eclips::G2DVerboCaido& c : caidos)
            {
                if (!verbos.empty()) verbos += "', '";
                verbos += c.verbo;
            }

            std::cerr << "[Gameplay2D] world.on('key_press') recibio '" << tecla
                      << "', que en este proyecto es la accion '" << verbos
                      << "'.\n"
                      << "  Escuchar la TECLA se rompe cuando el jugador la "
                      << "remapea; on('action') te entrega el VERBO y sobrevive "
                      << "al remapeo.\n"
                      << "  (key_press sigue siendo el canal correcto para "
                      << "teclas sin accion. Se avisa una sola vez por tecla.)"
                      << std::endl;
        }

        // ══════════════════════════════════════════════════════════════════
        // Lua exit check (Tier 4)
        // ══════════════════════════════════════════════════════════════════

        void checkLuaExit()
        {
            if (m_luaExitFlag)
            {
                m_exiting = true;
                m_fadeOutAlpha = 0;
                std::cout << "[Gameplay2D] Lua exit triggered" << std::endl;
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Lua World Bindings (Tier 4A)
        // ══════════════════════════════════════════════════════════════════
        //
        // Tabla "world" — API para manipular el mundo 2D desde Lua.
        // Cada función es un lambda que llama funcionalidad YA existente
        // en C++. El creador accede a todo desde:
        //
        //   world.get_player_pos()
        //   world.spawn("E", 3, 100, 200)
        //   world.camera_shake(0.5)
        //   world.exit()
        //   etc.
        //
        // ════════════════════════════════════════════════════════════════════

        void registerWorldBindings()
        {
            sol::state& lua = m_lua.state();
            sol::table world = lua.create_named_table("world");

            // ── Jugador ────────────────────────────────────────────────

            world.set_function("get_player_pos",
                [this]() -> std::tuple<float, float> {
                    return { m_posX, m_posY };
                });

            world.set_function("set_player_pos",
                [this](float x, float y) {
                    m_posX = x; m_posY = y;
                    std::cout << "[Lua] world.set_player_pos("
                        << x << ", " << y << ")" << std::endl;
                });

            world.set_function("get_player_hp",
                [this]() -> int { return m_playerHP; });

            world.set_function("set_player_hp",
                [this](int hp) {
                    m_playerHP = std::max(0, std::min(hp, m_playerMaxHP));
                    std::cout << "[Lua] world.set_player_hp("
                        << m_playerHP << ")" << std::endl;
                });

            world.set_function("damage_player",
                [this](int n) {
                    if (m_invincibleTimer > 0) return;
                    m_playerHP -= n;
                    if (m_playerHP < 0) m_playerHP = 0;
                    m_invincibleTimer = m_invincibleDuration;
                    m_cameraShakeTimer = 0.2f;
                    std::cout << "[Lua] world.damage_player(" << n
                        << ") → HP=" << m_playerHP << std::endl;
                });

            // ── Entidades ──────────────────────────────────────────────

            world.set_function("spawn",
                [this](const std::string& charStr, int count,
                    sol::optional<float> x, sol::optional<float> y) {
                        if (charStr.empty()) return;
                        char c = charStr[0];
                        if (x.has_value() && y.has_value())
                            spawnEntities(c, count, "at",
                                x.value(), y.value());
                        else
                            spawnEntities(c, count, "aleatorio", 0, 0);
                        std::cout << "[Lua] world.spawn('" << charStr
                            << "', " << count << ")" << std::endl;
                });

            world.set_function("enemy_count",
                [this]() -> int {
                    int count = 0;
                    for (const auto& e : m_enemies)
                        if (e.alive) count++;
                    return count;
                });

            world.set_function("destroy_enemy",
                [this](sol::object arg) -> bool {
                    if (arg.is<int>())
                    {
                        int id = arg.as<int>();
                        for (auto& e : m_enemies)
                        {
                            if (e.alive && e.id == id)
                            {
                                commitEnemyRemoval(
                                    e, EnemyRemovalCause::ScriptDestroyedLegacy);
                                std::cout << "[Lua] world.destroy_enemy(id="
                                    << id << ") → " << e.name << std::endl;
                                return true;
                            }
                        }
                    }
                    else if (arg.is<std::string>())
                    {
                        std::string name = arg.as<std::string>();
                        for (auto& e : m_enemies)
                        {
                            if (e.alive && e.name == name)
                            {
                                commitEnemyRemoval(
                                    e, EnemyRemovalCause::ScriptDestroyedLegacy);
                                std::cout << "[Lua] world.destroy_enemy('"
                                    << name << "')" << std::endl;
                                return true;
                            }
                        }
                    }
                    return false;
                });

            world.set_function("move_enemy",
                [this](sol::object arg, float x, float y) -> bool {
                    if (arg.is<int>())
                    {
                        int id = arg.as<int>();
                        for (auto& e : m_enemies)
                            if (e.alive && e.id == id)
                            {
                                e.box.x = x; e.box.y = y; return true;
                            }
                    }
                    else if (arg.is<std::string>())
                    {
                        std::string name = arg.as<std::string>();
                        for (auto& e : m_enemies)
                            if (e.alive && e.name == name)
                            {
                                e.box.x = x; e.box.y = y; return true;
                            }
                    }
                    return false;
                });

            world.set_function("get_enemy_pos",
                [this](sol::object arg) -> std::tuple<float, float> {
                    if (arg.is<int>())
                    {
                        int id = arg.as<int>();
                        for (const auto& e : m_enemies)
                            if (e.alive && e.id == id)
                                return { e.box.x, e.box.y };
                    }
                    else if (arg.is<std::string>())
                    {
                        std::string name = arg.as<std::string>();
                        for (const auto& e : m_enemies)
                            if (e.alive && e.name == name)
                                return { e.box.x, e.box.y };
                    }
                    return { -1.0f, -1.0f };
                });

            world.set_function("find_enemies",
                [this](const std::string& name) -> sol::table {
                    sol::table result = m_lua.state().create_table();
                    int idx = 1;
                    for (const auto& e : m_enemies)
                        if (e.alive && e.name == name)
                            result[idx++] = e.id;
                    return result;
                });

            world.set_function("get_enemy_hp",
                [this](sol::object arg) -> std::tuple<int, int> {
                    if (arg.is<int>())
                    {
                        int id = arg.as<int>();
                        for (const auto& e : m_enemies)
                            if (e.alive && e.id == id)
                                return { e.hp, e.maxHP };
                    }
                    else if (arg.is<std::string>())
                    {
                        std::string name = arg.as<std::string>();
                        for (const auto& e : m_enemies)
                            if (e.alive && e.name == name)
                                return { e.hp, e.maxHP };
                    }
                    return { 0, 0 };
                });

            // ── Mundo ──────────────────────────────────────────────────

            world.set_function("destroy_tile",
                [this](int col, int row) {
                    m_tilemap.setCharAt(row, col, '.',
                        m_tilemap.getCustomVisuals());
                    m_wallColliders = m_tilemap.getColliders();
                    m_platformColliders = m_tilemap.getPlatformColliders();
                    std::cout << "[Lua] world.destroy_tile("
                        << col << ", " << row << ")" << std::endl;
                });

            world.set_function("create_tile",
                [this](int col, int row, const std::string& ch) {
                    if (ch.empty()) return;
                    m_tilemap.setCharAt(row, col, ch[0],
                        m_tilemap.getCustomVisuals());
                    m_wallColliders = m_tilemap.getColliders();
                    m_platformColliders = m_tilemap.getPlatformColliders();
                    std::cout << "[Lua] world.create_tile("
                        << col << ", " << row << ", '"
                        << ch[0] << "')" << std::endl;
                });

            world.set_function("get_tile",
                [this](int col, int row) -> std::string {
                    if (row < 0 || row >= m_tilemap.gridH() ||
                        col < 0 || col >= m_tilemap.gridW())
                        return "";
                    return std::string(1, m_tilemap.getCharAt(row, col));
                });

            world.set_function("spawn_projectile",
                [this](const std::string& name,
                    float x, float y,
                    float dirX, float dirY,
                    sol::optional<bool> friendly) {
                        const MapProjectileDef* def = nullptr;
                        for (const auto& p : m_mapData.projectiles)
                            if (std::string(1, p.tile) == name ||
                                p.name == name)
                            {
                                def = &p; break;
                            }
                        if (!def)
                        {
                            std::cout << "[Lua] spawn_projectile: '"
                                << name << "' no encontrado" << std::endl;
                            return;
                        }

                        ProjectileRuntime proj;
                        float hs = def->size * 0.5f;
                        proj.box = { x - hs, y - hs, def->size, def->size };
                        proj.damage = def->damage;
                        proj.lifetime = def->lifetime;
                        proj.friendly = friendly.value_or(true);
                        proj.pierceWalls = def->pierceWalls;
                        proj.hasGravity = def->hasGravity;
                        proj.r = def->r; proj.g = def->g; proj.b = def->b;

                        float dist = std::sqrt(dirX * dirX + dirY * dirY);
                        if (dist > 0.01f)
                        {
                            proj.velX = (dirX / dist) * def->speed;
                            proj.velY = (dirY / dist) * def->speed;
                        }
                        else { proj.velX = def->speed; proj.velY = 0; }

                        m_projectiles.push_back(proj);
                        std::cout << "[Lua] world.spawn_projectile('"
                            << name << "')" << std::endl;
                });

            // ── Cámara ─────────────────────────────────────────────────

            world.set_function("camera_shake",
                [this](float duration) {
                    m_cameraShakeTimer = duration;
                });

            world.set_function("camera_pan",
                [this](float col, float row,
                    sol::optional<float> duration) {
                        m_cameraPanning = true;
                        m_cameraFollowPlayer = false;
                        m_cameraPanStartX = m_camX;
                        m_cameraPanStartY = m_camY;
                        m_cameraPanTargetX = col * Tilemap::TILE_SIZE
                            - (f32)viewport::width() * 0.5f;
                        m_cameraPanTargetY = row * Tilemap::TILE_SIZE
                            - (f32)viewport::height() * 0.5f;
                        m_cameraPanDuration = duration.value_or(1.0f);
                        m_cameraPanTimer = 0;
                });

            world.set_function("camera_zoom",
                [this](float level, sol::optional<float> duration) {
                    m_cameraZoomStart = m_cameraZoom;
                    m_cameraZoomTarget = level;
                    m_cameraZoomDuration = duration.value_or(0.5f);
                    m_cameraZoomTimer = 0;
                    if (m_cameraZoomDuration <= 0)
                        m_cameraZoom = m_cameraZoomTarget;
                });

            world.set_function("camera_follow",
                [this]() {
                    m_cameraFollowPlayer = true;
                    m_cameraPanning = false;
                });

            // ── General ────────────────────────────────────────────────

            world.set_function("notify",
                [this](const std::string& msg) {
                    m_notifications.push_back({ msg, 0, 2.5f });
                });

            world.set_function("sound",
                [this](const std::string& path) {
                    if (m_hasAudio) m_audio.playSound(path);
                });

            world.set_function("music",
                [this](const std::string& path) {
                    if (m_hasAudio) m_audio.playMusic(path, true);
                });

            world.set_function("stop_music",
                [this]() {
                    if (m_hasAudio) m_audio.stopMusic();
                });

            world.set_function("get_var",
                [this](const std::string& name) -> std::string {
                    return getVar(name);
                });

            world.set_function("set_var",
                [this](const std::string& name,
                    const std::string& val) {
                        applyVar(name, "=", val);
                });

            world.set_function("add_var",
                [this](const std::string& name, int n) {
                    applyVar(name, "+", std::to_string(n));
                });

            world.set_function("wave",
                [this]() -> int {
                    return m_currentWave + 1;
                });

            world.set_function("exit",
                [this]() {
                    m_luaExitFlag = true;
                    std::cout << "[Lua] world.exit() — flag set"
                        << std::endl;
                });

            // ── Base de Tower Defense (Tier 4C) ──────────────────────────

            world.set_function("get_base_hp",
                [this]() -> int { return m_baseHP; });

            world.set_function("get_base_max_hp",
                [this]() -> int { return m_baseMaxHP; });

            world.set_function("damage_base",
                [this](int amount) { damageBase(amount); });

            world.set_function("set_base_hp",
                [this](int hp) {
                    if (m_baseMaxHP <= 0) return;
                    m_baseHP = std::max(0, std::min(hp, m_baseMaxHP));
                    if (m_baseHP <= 0 && !m_baseDestroyed)
                        damageBase(m_baseMaxHP); // trigger destrucción
                });

            // ── Tower Defense: colocación de torres (Tier 4C.2) ────────
            world.set_function("place_tower",
                [this](const std::string& nameOrTile, int col, int row) -> bool {
                    bool ok = placeTowerAt(nameOrTile, col, row);
                    std::cout << "[Lua] world.place_tower('" << nameOrTile
                        << "', " << col << ", " << row << ") → "
                        << (ok ? "true" : "false") << std::endl;
                    return ok;
                });

            world.set_function("enter_build_mode",
                [this](const std::string& nameOrTile) -> bool {
                    if (!findTowerDef(nameOrTile))
                    {
                        std::cout << "[Lua] enter_build_mode: torre '"
                            << nameOrTile << "' no existe" << std::endl;
                        return false;
                    }
                    m_buildModeTowerName = nameOrTile;
                    m_buildCursorInMap = false; // hasta que llegue un mousemotion
                    std::cout << "[Lua] world.enter_build_mode('"
                        << nameOrTile << "')" << std::endl;
                    return true;
                });

            world.set_function("exit_build_mode",
                [this]() {
                    m_buildModeTowerName.clear();
                    std::cout << "[Lua] world.exit_build_mode()" << std::endl;
                });

            world.set_function("can_place",
                [this](const std::string& nameOrTile, int col, int row) -> bool {
                    return canPlaceTower(nameOrTile, col, row);
                });

            // ── Tier 4C.5: API Lua final de Tower Defense ────────────────

            // remove_tower(id, silent=false)
            //   Destruye una torre por ID. Si silent=false (default) ejecuta
            //   onDestroy y dispara el hook tower_destroyed, igual que si
            //   muriera por daño. Si silent=true, limpia sin efectos — útil
            //   para resets o destrucción programática que no debe notificar.
            //   Retorna true si se encontró y destruyó la torre.
            world.set_function("remove_tower",
                [this](int id, sol::optional<bool> silent) -> bool {
                    bool quiet = silent.value_or(false);
                    for (auto& t : m_towers)
                    {
                        if (t.alive && t.id == id)
                        {
                            t.alive = false;
                            t.hp = 0;
                            if (!quiet)
                            {
                                executeActions(t.onDestroy, t.box.x, t.box.y);
                                fireHook("tower_destroyed", t.name, t.id);
                            }
                            std::cout << "[Lua] world.remove_tower(" << id
                                << ")" << (quiet ? " [silent]" : "")
                                << std::endl;
                            return true;
                        }
                    }
                    std::cout << "[Lua] world.remove_tower(" << id
                        << ") — no encontrada" << std::endl;
                    return false;
                });

            // get_tower_cost(name_or_tile) → int
            //   Lee el campo `costo` del def. Retorna 0 si no existe o si
            //   la torre no define costo. Útil para economía: antes de
            //   entrar a build mode, verifica que el jugador tenga oro.
            world.set_function("get_tower_cost",
                [this](const std::string& nameOrTile) -> int {
                    const MapTowerDef* def = findTowerDef(nameOrTile);
                    return def ? def->cost : 0;
                });

            // R1 — restauracion y reemplazo conservan contratos distintos.
            // El coste sigue perteneciendo al caller Lua: estas primitivas
            // solo comprometen el estado mecanico despues de validar.
            world.set_function("get_placed_hp",
                [this](int id) -> std::tuple<int, int> {
                    const TowerRuntime* placed = findLivePlaced(id);
                    if (!placed) return { -1, -1 };
                    return { placed->hp, placed->maxHP };
                });

            world.set_function("restore_placed_hp",
                [this](int id) -> bool {
                    const bool restored = restorePlacedHp(id);
                    std::cout << "[Lua] world.restore_placed_hp(" << id
                        << ") -> " << (restored ? "true" : "false")
                        << std::endl;
                    return restored;
                });

            world.set_function("replace_placed",
                [this](int id) -> int {
                    const int replacementId = replacePlaced(id);
                    std::cout << "[Lua] world.replace_placed(" << id
                        << ") -> " << replacementId << std::endl;
                    return replacementId;
                });

            // X1 — la custodia conserva identidad/estado pero separa al
            // Placed del espacio y de la simulacion hasta redeploy explicito.
            world.set_function("extract_placed",
                [this](int id) -> bool {
                    const bool extracted = extractPlaced(id);
                    std::cout << "[Lua] world.extract_placed(" << id
                        << ") -> " << (extracted ? "true" : "false")
                        << std::endl;
                    return extracted;
                });

            world.set_function("redeploy_placed",
                [this](int id, int col, int row) -> bool {
                    const bool redeployed = redeployPlaced(id, col, row);
                    std::cout << "[Lua] world.redeploy_placed(" << id
                        << ", " << col << ", " << row << ") -> "
                        << (redeployed ? "true" : "false")
                        << std::endl;
                    return redeployed;
                });

            // I1 — inhibicion identificada de actividades de un Placed.
            // F1 / perfil distribuido: QUEDA. Mutacion determinista, local al
            // mapa y sin autoridad sobre tiempo, azar, seleccion o input.
            //
            // inhibit_placed_activities(id [, use_id]) -> handle
            //   Sin use_id bloquea todos los usos; con use_id bloquea solo ese
            //   uso nombrado. Retorna -1 si el Placed/uso no existe.
            world.set_function("inhibit_placed_activities",
                [this](int id, sol::optional<std::string> useId) -> int {
                    const auto tower = std::find_if(
                        m_towers.begin(), m_towers.end(),
                        [&](const TowerRuntime& candidate) {
                            return candidate.alive && candidate.id == id;
                        });
                    if (tower == m_towers.end())
                    {
                        std::cout
                            << "[Lua] world.inhibit_placed_activities(" << id
                            << ") — Placed no encontrado" << std::endl;
                        return -1;
                    }

                    std::vector<std::string> useIds;
                    if (useId)
                    {
                        if (useId->empty())
                        {
                            std::cout
                                << "[Lua] inhibit_placed_activities: use_id vacio"
                                << std::endl;
                            return -1;
                        }
                        const bool exists = std::any_of(
                            tower->abilityUses.begin(),
                            tower->abilityUses.end(),
                            [&](const Gameplay2DAbilityRuntime& use) {
                                return use.useId == *useId;
                            });
                        if (!exists)
                        {
                            std::cout
                                << "[Lua] inhibit_placed_activities: uso '"
                                << *useId << "' no existe en Placed " << id
                                << std::endl;
                            return -1;
                        }
                        useIds.push_back(*useId);
                    }

                    const int handle = m_nextActivityInhibitionHandle++;
                    if (!tower->activityInhibitions.add(
                            handle, std::move(useIds)))
                    {
                        return -1;
                    }
                    std::cout
                        << "[Lua] world.inhibit_placed_activities(" << id;
                    if (useId)
                        std::cout << ", '" << *useId << "'";
                    std::cout << ") -> " << handle << std::endl;
                    return handle;
                });

            // I1/Enemy - segundo owner publico de la inhibicion identificada.
            // F1 / perfil distribuido: QUEDA. M1 o cualquier otra regla Lua
            // puede aportar la causa; esta API solo impide commits futuros.
            //
            // inhibit_enemy_activities(id [, use_id]) -> handle
            //   Sin use_id bloquea todos los usos; con use_id bloquea solo ese
            //   uso nombrado. Retorna -1 si el Enemy/uso no existe.
            world.set_function("inhibit_enemy_activities",
                [this](int id, sol::optional<std::string> useId) -> int {
                    EnemyRuntime* enemy = findLiveEnemy(id);
                    if (!enemy)
                    {
                        std::cout
                            << "[Lua] world.inhibit_enemy_activities(" << id
                            << ") - Enemy no encontrado" << std::endl;
                        return -1;
                    }

                    std::vector<std::string> useIds;
                    if (useId)
                    {
                        if (useId->empty())
                        {
                            std::cout
                                << "[Lua] inhibit_enemy_activities: use_id vacio"
                                << std::endl;
                            return -1;
                        }
                        const bool exists = std::any_of(
                            enemy->abilityUses.begin(),
                            enemy->abilityUses.end(),
                            [&](const Gameplay2DAbilityRuntime& use) {
                                return use.useId == *useId;
                            });
                        if (!exists)
                        {
                            std::cout
                                << "[Lua] inhibit_enemy_activities: uso '"
                                << *useId << "' no existe en Enemy " << id
                                << std::endl;
                            return -1;
                        }
                        useIds.push_back(*useId);
                    }

                    const int handle = m_nextActivityInhibitionHandle;
                    if (!enemy->activityInhibitions.add(
                            handle, std::move(useIds)))
                    {
                        return -1;
                    }
                    ++m_nextActivityInhibitionHandle;
                    std::cout
                        << "[Lua] world.inhibit_enemy_activities(" << id;
                    if (useId)
                        std::cout << ", '" << *useId << "'";
                    std::cout << ") -> " << handle << std::endl;
                    return handle;
                });

            // remove_activity_inhibition(handle) -> bool
            //   Retira exactamente una causa de Placed o Enemy; no limpia
            //   otras inhibiciones.
            world.set_function("remove_activity_inhibition",
                [this](int handle) -> bool {
                    for (auto& tower : m_towers)
                    {
                        if (tower.alive &&
                            tower.activityInhibitions.remove(handle))
                        {
                            std::cout
                                << "[Lua] world.remove_activity_inhibition("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    for (auto& enemy : m_enemies)
                    {
                        if (enemy.alive &&
                            enemy.activityInhibitions.remove(handle))
                        {
                            std::cout
                                << "[Lua] world.remove_activity_inhibition("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    std::cout
                        << "[Lua] world.remove_activity_inhibition(" << handle
                        << ") — no encontrada" << std::endl;
                    return false;
                });

            // S1-C — coste autorado por commit de un uso de Placed.
            // La variable pertenece al caller; el motor solo coordina
            // asequibilidad, reserva y consumo con el mismo lote que A1.
            //
            // add_placed_activity_cost(id, use_id, variable, amount) -> handle
            //   Retorna -1 si Placed/uso no existe, la clave esta vacia o la
            //   cantidad no es positiva. El saldo puede nacer o cambiar luego.
            world.set_function("add_placed_activity_cost",
                [this](int id, const std::string& useId,
                       const std::string& resourceKey, int amount) -> int {
                    TowerRuntime* tower = findLivePlaced(id);
                    if (!tower)
                    {
                        std::cout
                            << "[Lua] world.add_placed_activity_cost(" << id
                            << ") — Placed no encontrado" << std::endl;
                        return -1;
                    }
                    if (useId.empty() || resourceKey.empty() || amount <= 0)
                    {
                        std::cout
                            << "[Lua] add_placed_activity_cost: argumentos invalidos"
                            << std::endl;
                        return -1;
                    }
                    const bool useExists = std::any_of(
                        tower->abilityUses.begin(), tower->abilityUses.end(),
                        [&](const Gameplay2DAbilityRuntime& use) {
                            return use.useId == useId;
                        });
                    if (!useExists)
                    {
                        std::cout
                            << "[Lua] add_placed_activity_cost: uso '"
                            << useId << "' no existe en Placed " << id
                            << std::endl;
                        return -1;
                    }

                    const int handle = m_nextActivityCostHandle++;
                    tower->activityCosts.push_back({
                        handle,
                        useId,
                        { resourceKey, static_cast<std::int64_t>(amount) }
                    });
                    std::cout
                        << "[Lua] world.add_placed_activity_cost(" << id
                        << ", '" << useId << "', '" << resourceKey
                        << "', " << amount << ") -> " << handle
                        << std::endl;
                    return handle;
                });

            // remove_activity_cost(handle) -> bool
            //   Retira una causa exacta. Restore/X1 conservan el handle;
            //   reemplazo o destruccion consumen la identidad que lo poseia.
            world.set_function("remove_activity_cost",
                [this](int handle) -> bool {
                    for (auto& tower : m_towers)
                    {
                        if (!tower.alive)
                            continue;
                        const auto binding = std::find_if(
                            tower.activityCosts.begin(),
                            tower.activityCosts.end(),
                            [&](const Gameplay2DActivityCostBinding& entry) {
                                return entry.handle == handle;
                            });
                        if (binding == tower.activityCosts.end())
                            continue;

                        tower.activityCosts.erase(binding);
                        std::cout
                            << "[Lua] world.remove_activity_cost("
                            << handle << ")" << std::endl;
                        return true;
                    }
                    std::cout
                        << "[Lua] world.remove_activity_cost(" << handle
                        << ") — no encontrado" << std::endl;
                    return false;
                });

            // S1-D1 — factor runtime sobre el intervalo de un uso de Placed.
            // El factor no observa HP ni recursos: Lua decide la causa.
            //
            // add_placed_activity_interval_factor(id, use_id, factor)
            //   -> handle|-1
            world.set_function("add_placed_activity_interval_factor",
                [this](int id, const std::string& useId,
                       double factor) -> int {
                    TowerRuntime* tower = findLivePlaced(id);
                    if (!tower)
                    {
                        std::cout
                            << "[Lua] world.add_placed_activity_interval_factor("
                            << id << ") — Placed no encontrado" << std::endl;
                        return -1;
                    }
                    if (useId.empty() || !std::isfinite(factor) ||
                        factor <= 0.0)
                    {
                        std::cout
                            << "[Lua] add_placed_activity_interval_factor: "
                               "argumentos invalidos" << std::endl;
                        return -1;
                    }

                    auto use = std::find_if(
                        tower->abilityUses.begin(), tower->abilityUses.end(),
                        [&](const Gameplay2DAbilityRuntime& candidate) {
                            return candidate.useId == useId;
                        });
                    if (use == tower->abilityUses.end())
                    {
                        std::cout
                            << "[Lua] add_placed_activity_interval_factor: uso '"
                            << useId << "' no existe en Placed " << id
                            << std::endl;
                        return -1;
                    }

                    const int handle =
                        m_nextActivityIntervalFactorHandle;
                    if (!use->activityIntervalFactors.add(
                            handle, static_cast<float>(factor),
                            use->effective.fireRate))
                    {
                        std::cout
                            << "[Lua] add_placed_activity_interval_factor: "
                               "el intervalo compuesto no es valido"
                            << std::endl;
                        return -1;
                    }
                    ++m_nextActivityIntervalFactorHandle;
                    std::cout
                        << "[Lua] world.add_placed_activity_interval_factor("
                        << id << ", '" << useId << "', " << factor
                        << ") -> " << handle << std::endl;
                    return handle;
                });

            // S1-D1/Enemy - segundo owner publico del mismo factor runtime.
            // La causa sigue en Lua y modifica solo el intervalo del uso.
            //
            // add_enemy_activity_interval_factor(id, use_id, factor)
            //   -> handle|-1
            world.set_function("add_enemy_activity_interval_factor",
                [this](int id, const std::string& useId,
                       double factor) -> int {
                    EnemyRuntime* enemy = findLiveEnemy(id);
                    if (!enemy)
                    {
                        std::cout
                            << "[Lua] world.add_enemy_activity_interval_factor("
                            << id << ") - Enemy no encontrado" << std::endl;
                        return -1;
                    }
                    if (useId.empty() || !std::isfinite(factor) ||
                        factor <= 0.0)
                    {
                        std::cout
                            << "[Lua] add_enemy_activity_interval_factor: "
                               "argumentos invalidos" << std::endl;
                        return -1;
                    }

                    auto use = std::find_if(
                        enemy->abilityUses.begin(), enemy->abilityUses.end(),
                        [&](const Gameplay2DAbilityRuntime& candidate) {
                            return candidate.useId == useId;
                        });
                    if (use == enemy->abilityUses.end())
                    {
                        std::cout
                            << "[Lua] add_enemy_activity_interval_factor: uso '"
                            << useId << "' no existe en Enemy " << id
                            << std::endl;
                        return -1;
                    }

                    const int handle =
                        m_nextActivityIntervalFactorHandle;
                    if (!use->activityIntervalFactors.add(
                            handle, static_cast<float>(factor),
                            use->effective.fireRate))
                    {
                        std::cout
                            << "[Lua] add_enemy_activity_interval_factor: "
                               "el intervalo compuesto no es valido"
                            << std::endl;
                        return -1;
                    }
                    ++m_nextActivityIntervalFactorHandle;
                    std::cout
                        << "[Lua] world.add_enemy_activity_interval_factor("
                        << id << ", '" << useId << "', " << factor
                        << ") -> " << handle << std::endl;
                    return handle;
                });

            // remove_activity_interval_factor(handle) -> bool
            // Restore/X1 conservan la causa Placed. Reemplazo/destruccion de
            // Placed y cualquier retirada de Enemy consumen la identidad y
            // vuelven obsoleto su handle.
            world.set_function("remove_activity_interval_factor",
                [this](int handle) -> bool {
                    for (auto& tower : m_towers)
                    {
                        if (!tower.alive)
                            continue;
                        for (auto& use : tower.abilityUses)
                        {
                            if (!use.activityIntervalFactors.remove(handle))
                                continue;
                            std::cout
                                << "[Lua] world.remove_activity_interval_factor("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    for (auto& enemy : m_enemies)
                    {
                        if (!enemy.alive)
                            continue;
                        for (auto& use : enemy.abilityUses)
                        {
                            if (!use.activityIntervalFactors.remove(handle))
                                continue;
                            std::cout
                                << "[Lua] world.remove_activity_interval_factor("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    std::cout
                        << "[Lua] world.remove_activity_interval_factor("
                        << handle << ") — no encontrado" << std::endl;
                    return false;
                });

            // S1-D2 - factor runtime sobre el alcance de un uso de Placed.
            // Solo afecta adquisiciones futuras; Lua decide la causa y F1
            // permanece distribuida como capacidad neutral del uso runtime.
            //
            // add_placed_activity_range_factor(id, use_id, factor)
            //   -> handle|-1
            world.set_function("add_placed_activity_range_factor",
                [this](int id, const std::string& useId,
                       double factor) -> int {
                    TowerRuntime* tower = findLivePlaced(id);
                    if (!tower)
                    {
                        std::cout
                            << "[Lua] world.add_placed_activity_range_factor("
                            << id << ") - Placed no encontrado" << std::endl;
                        return -1;
                    }
                    if (useId.empty() || !std::isfinite(factor) ||
                        factor <= 0.0)
                    {
                        std::cout
                            << "[Lua] add_placed_activity_range_factor: "
                               "argumentos invalidos" << std::endl;
                        return -1;
                    }

                    auto use = std::find_if(
                        tower->abilityUses.begin(), tower->abilityUses.end(),
                        [&](const Gameplay2DAbilityRuntime& candidate) {
                            return candidate.useId == useId;
                        });
                    if (use == tower->abilityUses.end())
                    {
                        std::cout
                            << "[Lua] add_placed_activity_range_factor: uso '"
                            << useId << "' no existe en Placed " << id
                            << std::endl;
                        return -1;
                    }

                    const int handle = m_nextActivityRangeFactorHandle;
                    if (!use->activityRangeFactors.add(
                            handle, static_cast<float>(factor),
                            use->effective.range))
                    {
                        std::cout
                            << "[Lua] add_placed_activity_range_factor: "
                               "el alcance compuesto no es valido"
                            << std::endl;
                        return -1;
                    }
                    ++m_nextActivityRangeFactorHandle;
                    std::cout
                        << "[Lua] world.add_placed_activity_range_factor("
                        << id << ", '" << useId << "', " << factor
                        << ") -> " << handle << std::endl;
                    return handle;
                });

            // remove_activity_range_factor(handle) -> bool
            // Restore/X1 conservan la causa; reemplazo/destruccion consumen
            // la identidad y vuelven obsoleto su handle.
            world.set_function("remove_activity_range_factor",
                [this](int handle) -> bool {
                    for (auto& tower : m_towers)
                    {
                        if (!tower.alive)
                            continue;
                        for (auto& use : tower.abilityUses)
                        {
                            if (!use.activityRangeFactors.remove(handle))
                                continue;
                            std::cout
                                << "[Lua] world.remove_activity_range_factor("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    std::cout
                        << "[Lua] world.remove_activity_range_factor("
                        << handle << ") - no encontrado" << std::endl;
                    return false;
                });

            // S1-D3 - delta entero runtime sobre el dano directo futuro de un
            // uso de Placed. No modifica carriers ni el valor autorado.
            //
            // add_placed_activity_direct_damage_delta(id, use_id, delta)
            //   -> handle|-1
            world.set_function("add_placed_activity_direct_damage_delta",
                [this](int id, const std::string& useId,
                       double delta) -> int {
                    TowerRuntime* tower = findLivePlaced(id);
                    if (!tower)
                    {
                        std::cout
                            << "[Lua] world.add_placed_activity_direct_damage_delta("
                            << id << ") - Placed no encontrado" << std::endl;
                        return -1;
                    }
                    if (useId.empty() || !std::isfinite(delta) ||
                        std::trunc(delta) != delta || delta == 0.0 ||
                        delta < std::numeric_limits<int>::min() ||
                        delta > std::numeric_limits<int>::max())
                    {
                        std::cout
                            << "[Lua] add_placed_activity_direct_damage_delta: "
                               "delta entero no valido" << std::endl;
                        return -1;
                    }

                    auto use = std::find_if(
                        tower->abilityUses.begin(), tower->abilityUses.end(),
                        [&](const Gameplay2DAbilityRuntime& candidate) {
                            return candidate.useId == useId;
                        });
                    if (use == tower->abilityUses.end())
                    {
                        std::cout
                            << "[Lua] add_placed_activity_direct_damage_delta: uso '"
                            << useId << "' no existe en Placed " << id
                            << std::endl;
                        return -1;
                    }
                    if (!gameplay2DAbilityHasDirectDamage(use->effective) ||
                        gameplay2DAbilityHasConflictingEffects(use->effective))
                    {
                        std::cout
                            << "[Lua] add_placed_activity_direct_damage_delta: uso '"
                            << useId << "' no posee dano directo base"
                            << std::endl;
                        return -1;
                    }

                    const int handle =
                        m_nextActivityDirectDamageDeltaHandle;
                    const int integerDelta = static_cast<int>(delta);
                    if (!use->activityDirectDamageDeltas.add(
                            handle, integerDelta,
                            *use->effective.directDamage))
                    {
                        std::cout
                            << "[Lua] add_placed_activity_direct_damage_delta: "
                               "el subconjunto no conserva [1, INT_MAX]"
                            << std::endl;
                        return -1;
                    }
                    ++m_nextActivityDirectDamageDeltaHandle;
                    std::cout
                        << "[Lua] world.add_placed_activity_direct_damage_delta("
                        << id << ", '" << useId << "', " << integerDelta
                        << ") -> " << handle << std::endl;
                    return handle;
                });

            // remove_activity_direct_damage_delta(handle) -> bool
            // Restore/X1 conservan la causa; reemplazo/destruccion consumen
            // la identidad y vuelven obsoleto su handle.
            world.set_function("remove_activity_direct_damage_delta",
                [this](int handle) -> bool {
                    for (auto& tower : m_towers)
                    {
                        if (!tower.alive)
                            continue;
                        for (auto& use : tower.abilityUses)
                        {
                            if (!use.activityDirectDamageDeltas.remove(handle))
                                continue;
                            std::cout
                                << "[Lua] world.remove_activity_direct_damage_delta("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    std::cout
                        << "[Lua] world.remove_activity_direct_damage_delta("
                        << handle << ") - no encontrado" << std::endl;
                    return false;
                });

            // find_nearest_enemy(x, y, max_range) → id, ex, ey
            //   Busca el enemigo vivo no-invulnerable más cercano al punto
            //   (x, y) dentro de max_range. Si no hay, retorna id=-1, 0, 0.
            //   Uso típico desde Lua:
            //     local id, ex, ey = world.find_nearest_enemy(px, py, 200)
            //     if id >= 0 then ... end
            // S1-D4 - override absoluto runtime sobre la preferencia futura de
            // un uso de Placed. No compone criterios ni cambia elegibilidad.
            // El ultimo override insertado que siga vivo gobierna.
            //
            // add_placed_activity_preference_override(
            //     id, use_id, preference) -> handle|-1
            world.set_function("add_placed_activity_preference_override",
                [this](int id, const std::string& useId,
                       const std::string& preferenceToken) -> int {
                    TowerRuntime* tower = findLivePlaced(id);
                    if (!tower)
                    {
                        std::cout
                            << "[Lua] world.add_placed_activity_preference_override("
                            << id << ") - Placed no encontrado" << std::endl;
                        return -1;
                    }
                    if (useId.empty())
                    {
                        std::cout
                            << "[Lua] add_placed_activity_preference_override: "
                               "use_id vacio" << std::endl;
                        return -1;
                    }

                    auto use = std::find_if(
                        tower->abilityUses.begin(), tower->abilityUses.end(),
                        [&](const Gameplay2DAbilityRuntime& candidate) {
                            return candidate.useId == useId;
                        });
                    if (use == tower->abilityUses.end())
                    {
                        std::cout
                            << "[Lua] add_placed_activity_preference_override: uso '"
                            << useId << "' no existe en Placed " << id
                            << std::endl;
                        return -1;
                    }

                    const auto preference =
                        parsePublicGameplay2DTargetPreference(preferenceToken);
                    if (!preference)
                    {
                        std::cout
                            << "[Lua] add_placed_activity_preference_override: "
                               "preferencia publica desconocida '"
                            << preferenceToken << "'" << std::endl;
                        return -1;
                    }

                    const int handle =
                        m_nextActivityPreferenceOverrideHandle;
                    if (!use->activityPreferenceOverrides.add(
                            handle, *preference))
                    {
                        return -1;
                    }
                    ++m_nextActivityPreferenceOverrideHandle;
                    std::cout
                        << "[Lua] world.add_placed_activity_preference_override("
                        << id << ", '" << useId << "', '"
                        << preferenceToken << "') -> " << handle
                        << std::endl;
                    return handle;
                });

            // S1-D4/Enemy - segundo owner publico del mismo override runtime.
            // Cubeta F1: queda. Es una capacidad nombrada, estrecha y mediada;
            // no expone direcciones genericas ni recursos del anfitrion.
            //
            // add_enemy_activity_preference_override(
            //     enemy_id, use_id, preference) -> handle|-1
            world.set_function("add_enemy_activity_preference_override",
                [this](int id, const std::string& useId,
                       const std::string& preferenceToken) -> int {
                    EnemyRuntime* enemy = findLiveEnemy(id);
                    if (!enemy)
                    {
                        std::cout
                            << "[Lua] world.add_enemy_activity_preference_override("
                            << id << ") - Enemy no encontrado" << std::endl;
                        return -1;
                    }
                    if (useId.empty())
                    {
                        std::cout
                            << "[Lua] add_enemy_activity_preference_override: "
                               "use_id vacio" << std::endl;
                        return -1;
                    }

                    auto use = std::find_if(
                        enemy->abilityUses.begin(), enemy->abilityUses.end(),
                        [&](const Gameplay2DAbilityRuntime& candidate) {
                            return candidate.useId == useId;
                        });
                    if (use == enemy->abilityUses.end())
                    {
                        std::cout
                            << "[Lua] add_enemy_activity_preference_override: uso '"
                            << useId << "' no existe en Enemy " << id
                            << std::endl;
                        return -1;
                    }

                    const auto preference =
                        parsePublicGameplay2DTargetPreference(preferenceToken);
                    if (!preference)
                    {
                        std::cout
                            << "[Lua] add_enemy_activity_preference_override: "
                               "preferencia publica desconocida '"
                            << preferenceToken << "'" << std::endl;
                        return -1;
                    }

                    const int handle =
                        m_nextActivityPreferenceOverrideHandle;
                    if (!use->activityPreferenceOverrides.add(
                            handle, *preference))
                    {
                        return -1;
                    }
                    ++m_nextActivityPreferenceOverrideHandle;
                    std::cout
                        << "[Lua] world.add_enemy_activity_preference_override("
                        << id << ", '" << useId << "', '"
                        << preferenceToken << "') -> " << handle
                        << std::endl;
                    return handle;
                });

            // remove_activity_preference_override(handle) -> bool
            // Placed conserva la causa por Restore/X1; replace/destruccion y
            // cualquier retirada de Enemy consumen identidad y vuelven
            // obsoleto su handle.
            world.set_function("remove_activity_preference_override",
                [this](int handle) -> bool {
                    for (auto& tower : m_towers)
                    {
                        if (!tower.alive)
                            continue;
                        for (auto& use : tower.abilityUses)
                        {
                            if (!use.activityPreferenceOverrides.remove(handle))
                                continue;
                            std::cout
                                << "[Lua] world.remove_activity_preference_override("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    for (auto& enemy : m_enemies)
                    {
                        if (!enemy.alive)
                            continue;
                        for (auto& use : enemy.abilityUses)
                        {
                            if (!use.activityPreferenceOverrides.remove(handle))
                                continue;
                            std::cout
                                << "[Lua] world.remove_activity_preference_override("
                                << handle << ")" << std::endl;
                            return true;
                        }
                    }
                    std::cout
                        << "[Lua] world.remove_activity_preference_override("
                        << handle << ") - no encontrado" << std::endl;
                    return false;
                });

            world.set_function("find_nearest_enemy",
                [this](float x, float y, float maxRange)
                -> std::tuple<int, float, float> {
                    f32 r2 = maxRange * maxRange;
                    EnemyRuntime* best = nullptr;
                    f32 bestD2 = 0;
                    for (auto& e : m_enemies)
                    {
                        if (!e.alive || e.invulnerable) continue;
                        f32 ecx = e.box.x + e.box.w * 0.5f;
                        f32 ecy = e.box.y + e.box.h * 0.5f;
                        f32 dx = ecx - x;
                        f32 dy = ecy - y;
                        f32 d2 = dx * dx + dy * dy;
                        if (d2 > r2) continue;
                        if (!best || d2 < bestD2) { best = &e; bestD2 = d2; }
                    }
                    if (!best) return { -1, 0.0f, 0.0f };
                    f32 ecx = best->box.x + best->box.w * 0.5f;
                    f32 ecy = best->box.y + best->box.h * 0.5f;
                    return { best->id, ecx, ecy };
                });

            // ── Fin de nivel (Tier 4D Fase 4) ──────────────────────────
            // Pushean LevelEndState con el preset configurado en el
            // .eclmap. Idempotentes: si ya hay un fin de nivel pusheado
            // (m_exiting=true), no hacen nada.

            // world.victory()
            //   Termina el nivel como victoria. Útil para mecánicas custom
            //   (ej: matar al boss, recolectar todas las llaves, sobrevivir
            //   N segundos). Equivalente a que el engine detecte "oleadas
            //   completas" pero disparado por el creador.
            world.set_function("victory",
                [this]() { pushLevelEnd(true); });

            // world.defeat()
            //   Termina el nivel como derrota. Útil para condiciones de
            //   fallo custom (ej: tiempo agotado, NPC clave murió, recurso
            //   crítico llegó a 0).
            world.set_function("defeat",
                [this]() { pushLevelEnd(false); });

            // ── CP8: POLLING — el creador PREGUNTA por acciones ────────
            //
            // La otra mitad del §11. `on("action")` es el canal de EVENTO;
            // esto es el de ESTADO, para lo que se mantiene apretado.
            //
            // Los dos canonizan el nombre en la puerta y avisan si no lo
            // conocen, y ése es el fork 8c: hasta este CP, un `move_x`
            // —lo que le sale natural a quien escribió `keys game
            // { move_left A }`, que `CP3` sí acepta— devolvía **0.0 en
            // silencio**. El personaje no caminaba y nada decía por qué.

            // world.axis("mover_x" | "move_x") -> -1..1
            //   Los ejes no son verbos: son la RESTA de dos
            //   (`mover_izquierda`/`mover_derecha`). Por eso no se pueden
            //   ligar a una tecla y por eso viven en su propia tabla.
            world.set_function("axis",
                [this](const std::string& nombre) -> float {
                    if (!eclips::inputEjeExiste(nombre))
                    {
                        avisarNombreLuaDesconocido("axis", nombre, "0");
                        return 0.0f;
                    }
                    return m_input->eje(nombre, "juego");
                });

            // world.action_active("saltar" | "jump") -> bool (held)
            //   Canoniza con la MISMA función que el parser del bloque
            //   `teclas`, así que lo que el creador puede escribir ahí es
            //   exactamente lo que puede preguntar acá. Dos superficies, un
            //   vocabulario.
            world.set_function("action_active",
                [this](const std::string& verbo) -> bool {
                    const std::string canon =
                        eclips::inputVerboDe(verbo, "juego");
                    if (canon.empty())
                    {
                        avisarNombreLuaDesconocido("action_active", verbo,
                                                   "false");
                        return false;
                    }
                    return m_input->held(canon, "juego");
                });

            // ── Hooks ──────────────────────────────────────────────────

            world.set_function("on",
                [this](const std::string& event, sol::function callback) {
                    m_hooks[event].push_back(callback);
                    if (event == "update") m_hasUpdateHook = true;
                    std::cout << "[Lua] world.on('" << event
                        << "') registrado" << std::endl;
                });

            // Hooks disponibles (disparados desde C++):
            //   "enemy_death"     (name, id)
            //   "player_damage"   (amount)
            //   "wave_complete"   (num)
            //   "item_pickup"     (name)
            //   "trigger"         (tile)
            //   "marker_enter"    (marker_id, actor_kind, actor_id)
            //   "update"          (dt)
            //   "tile_click"      (col, row, button)    — desde handleEvent
            //   "base_damaged"    (amount, hp_remaining) — Tier 4C
            //   "base_destroyed"  ()                     — Tier 4C
            //   "tower_placed"    (name, id, col, row)   — Tier 4C
            //   "tower_destroyed" (name, id)             — Tier 4C
            //   "key_press"       (key_name)             — Tier 4C.5
            //   "action"          (verbo, dominio, ejecutado) — CP8
            //
            // `action` es `key_press` subido de capa: entrega VERBOS, no
            // teclas, así que sobrevive a que el jugador remapee. Dispara una
            // vez por cada verbo que la tecla resolvió y que el motor no
            // consumió; `ejecutado` dice cuál de ellos corrió de verdad (los
            // gates internos — cooldown, suelo, capacidad — pueden frenarlo, y
            // en ese caso el motor explica por qué en `stderr`).
            //
            // El verbo llega en su grafía CANÓNICA (español), aunque el bloque
            // `teclas`/`keys` se haya escrito en inglés: es un identificador
            // para comparar, no un texto para leer.
            //
            // ⚠ NO reemplaza a `key_press`, y no es una transición a medias:
            // una tecla que no está ligada a ningún verbo resuelve a una lista
            // vacía, así que `action` no dispara para ella. Las dos poblaciones
            // son distintas.
            //   "victory"         (stars_default)        — Tier 4D, retorno-tabla opcional
            //   "defeat"          ()                     — Tier 4D, retorno-tabla opcional
            //
            // Retorno-tabla de victory/defeat (todo opcional, claves
            // bilingües):
            //   { estrellas/stars = N        -- override del cálculo automático (solo victory)
            //   , texto/text     = "..."     -- override del título del panel
            //   , tally          = {         -- override de tallyFields
            //       { nombre/name="...", valor/value=... },
            //       ...
            //     }
            //   }
            // Retornar nil o nada deja el preset tal cual.

            std::cout << "[Gameplay2D] Lua world bindings registrados"
                << " (" << 65 << " funciones)" << std::endl;
        }

        void checkWavesExit()
        {
            if (m_wavesComplete)
            {
                std::cout << "[Gameplay2D] Salida: oleadas completadas." << std::endl;
                pushLevelEnd(true);
            }
        }

        void triggerExit()
        {
            if (m_exiting) return;
            m_exiting = true;
            m_fadeOutAlpha = 0.0f;
            std::cout << "[Gameplay2D] Iniciando fade out de salida." << std::endl;
        }

        // ══════════════════════════════════════════════════════════════════
        // Tier 4D — Pantalla de fin de nivel
        // ══════════════════════════════════════════════════════════════════
        //
        // Pushea LevelEndState con el preset configurado (victoria/derrota).
        // Si el .eclmap no tiene bloque fin:, usa el preset default (Classic
        // con texto genérico). Idempotente: no hace push duplicado si ya hay
        // un LevelEndState arriba (detectado via m_exiting).
        //
        void pushLevelEnd(bool isVictory)
        {
            if (m_exiting) return;
            m_exiting = true;

            // Copia (no referencia) — los hooks Lua pueden mutar este preset
            // sin afectar el original en m_mapData.endConfig.
            LevelEndPreset preset = isVictory
                ? m_mapData.endConfig.victory
                : m_mapData.endConfig.defeat;

            std::cout << "[Gameplay2D] Fin de nivel: "
                << (isVictory ? "VICTORIA" : "DERROTA") << std::endl;

            // varLookup: resuelve vars especiales del engine + vars custom del creador.
            // Pasado al LevelEndState para evaluar criterios de estrellas y llenar tally.
            auto varLookup = [this](const std::string& name) -> std::string {
                // ── Vars especiales del engine ──
                if (name == "hp")          return std::to_string(m_playerHP);
                if (name == "hp_max")      return std::to_string(m_playerMaxHP);
                if (name == "hp_base")     return std::to_string(m_baseHP);
                if (name == "hp_base_max") return std::to_string(m_baseMaxHP);
                if (name == "oleada")
                {
                    int total = (int)m_mapData.waveConfig.waves.size();
                    int cur = m_wavesComplete ? total : (m_currentWave + 1);
                    return std::to_string(cur);
                }
                if (name == "oleada_max")
                    return std::to_string((int)m_mapData.waveConfig.waves.size());
                if (name == "tiempo")
                    return std::to_string((int)m_totalTime);

                // ── Vars custom del creador ──
                std::string v = getVar(name);
                if (v.empty()) return "0";
                return v;
                };

            // ── Hook Lua "victory"/"defeat" (Tier 4D Fase 4) ──
            // Para victoria: calcular stars_default usando los criterios del
            // .eclmap (espejo de LevelEndState::computeStars). Se le pasa al
            // hook para que pueda decidir "respetar y/o ajustar".
            int starsDefault = 0;
            if (isVictory)
            {
                auto evalCrit = [&varLookup](const LevelEndStarCriterion& c) -> bool {
                    if (c.always) return true;
                    std::string lhsStr = varLookup(c.var);
                    float left = 0.0f, right = 0.0f;
                    try { left = std::stof(lhsStr); }
                    catch (...) {}
                    try { right = std::stof(c.val); }
                    catch (...) {}
                    if (c.op == ">=") return left >= right;
                    if (c.op == "<=") return left <= right;
                    if (c.op == ">")  return left > right;
                    if (c.op == "<")  return left < right;
                    if (c.op == "==") return left == right;
                    if (c.op == "!=") return left != right;
                    return false;
                    };
                for (const auto& c : preset.starCriteria)
                {
                    if (evalCrit(c)) { starsDefault = c.stars; break; }
                }
            }

            // Invocar el hook si está registrado. Captura el retorno-tabla
            // y muta la copia local del preset si trae overrides.
            // Convención de nombres: bilingüe (estrellas/stars, texto/text,
            // nombre/name, valor/value). El nombre del hook es solo inglés
            // por consistencia con los otros 11 hooks (enemy_death, etc).
            const std::string hookName = isVictory ? "victory" : "defeat";
            auto hit = m_hooks.find(hookName);
            // Nota: NO usamos m_insideHook como guard aquí. pushLevelEnd
            // puede ser invocado desde DENTRO de otro hook (típico:
            // key_press → world.victory()), y en ese contexto m_insideHook
            // ya es true. Bloquearlo causaría que el override de Lua se
            // ignore silenciosamente. La protección contra recursión real
            // la da m_exiting al inicio de esta función.
            if (hit != m_hooks.end() && !hit->second.empty())
            {
                for (auto& fn : hit->second)
                {
                    sol::protected_function_result result = isVictory
                        ? fn(starsDefault)
                        : fn();

                    if (!result.valid())
                    {
                        sol::error err = result;
                        std::cerr << "[Lua] Hook '" << hookName
                            << "' error: " << err.what() << std::endl;
                        continue;
                    }

                    // Si retornó una tabla, procesar overrides. Si retornó
                    // nil o nada, se usa el preset tal cual.
                    sol::object ret = result;
                    if (!ret.is<sol::table>()) continue;
                    sol::table t = ret;

                    // Estrellas (solo aplicable a victoria). Usamos
                    // sol::object + .is<T>() en vez de sol::optional<T> =
                    // proxy porque la asignación encadenada del proxy es
                    // ambigua para el compilador. Mismo patrón que tally.
                    if (isVictory)
                    {
                        sol::object starsObj = t["estrellas"];
                        if (!starsObj.valid()) starsObj = t["stars"];
                        // Lua trata todos los números como double — aceptamos
                        // ambos para no fallar silenciosamente con `stars + 1`.
                        if (starsObj.is<int>())
                            preset.starsOverride = starsObj.as<int>();
                        else if (starsObj.is<double>())
                            preset.starsOverride = (int)starsObj.as<double>();
                    }

                    // Texto del título
                    sol::object textoObj = t["texto"];
                    if (!textoObj.valid()) textoObj = t["text"];
                    if (textoObj.is<std::string>())
                        preset.text = textoObj.as<std::string>();

                    // Tally — array de tablas {nombre, valor}. Los valores
                    // ya son literales finales (no varNames del engine).
                    sol::object tallyObj = t["tally"];
                    if (tallyObj.is<sol::table>())
                    {
                        sol::table tallyT = tallyObj;
                        preset.tallyOverride.clear();
                        for (auto& kv : tallyT)
                        {
                            sol::object v = kv.second;
                            if (!v.is<sol::table>()) continue;
                            sol::table row = v;

                            sol::object nameObj = row["nombre"];
                            if (!nameObj.valid()) nameObj = row["name"];
                            if (!nameObj.is<std::string>()) continue;

                            sol::object valObj = row["valor"];
                            if (!valObj.valid()) valObj = row["value"];

                            std::string nameStr = nameObj.as<std::string>();
                            std::string valStr;
                            if (valObj.is<std::string>())
                                valStr = valObj.as<std::string>();
                            else if (valObj.is<double>())
                            {
                                double d = valObj.as<double>();
                                if (std::abs(d - std::round(d)) < 0.001)
                                    valStr = std::to_string((int)std::round(d));
                                else
                                {
                                    char buf[32];
                                    std::snprintf(buf, sizeof(buf), "%.1f", d);
                                    valStr = buf;
                                }
                            }

                            preset.tallyOverride.emplace_back(nameStr, valStr);
                        }
                    }
                }
            }

            // nextMap se captura DESPUÉS del hook por si en el futuro
            // queremos permitir override del siguiente nivel desde Lua.
            std::string nextMap = preset.nextMap;

            // Sonido de entrada (Tier 4D Fase 4). Se dispara aquí — no
            // desde el LevelEndState — reusando el AudioManager existente.
            // Funciona uniforme para los 3 caminos: triggers internos
            // (oleadas/base/muerte), world.victory() y world.defeat().
            if (!preset.entrySound.empty() && m_hasAudio)
                m_audio.playSound(preset.entrySound);

            m_stateStack->push(std::make_unique<LevelEndState>(
                m_window, m_stateStack, m_fontSources,
                preset, isVictory,
                [this] { m_retryRequested = true; },
                [this] { m_returnToMenu = true; },
                nextMap.empty() ? std::function<void()>(nullptr)
                : std::function<void()>(
                    [this, nextMap] { m_pendingMapLoad = nextMap; }),
                varLookup
            ));
        }


        // ══════════════════════════════════════════════════════════════════
        // HUD
        // ══════════════════════════════════════════════════════════════════

        void renderHUD(f32 VW, f32 VH)
        {
            // ── Barra superior de controles ──
            draw2d::drawRect(10, 10, 620, 40, 0,0,0, 0.5f);
            draw2d::drawRectBorder(10, 10, 620, 40, 0.3f,0.7f,1,0.6f, 2);
            if (m_hasFont)
            {
                std::string controls = m_canAttack
                    ? "WASD=mover  E=hablar  ESPACIO=atacar  ESC=pausa"
                    : "Flechas/WASD=mover  E=hablar  ESC=pausa";
                m_hudText.drawText(controls, 20, 14, 0.8f,0.9f,1, 0.9f);
            }

            // ── Posición ──
            if (m_hasFont)
            {
                draw2d::drawRect(10, VH-45, 200, 35, 0,0,0, 0.4f);
                m_hudText.drawText("Pos: " + std::to_string((int)m_posX) + ", "
                    + std::to_string((int)m_posY), 18, VH-42, 0.6f,0.8f,0.6f, 0.8f);
            }

            // ── HP del jugador ──
            if (m_mapData.hudDef.showHP && m_playerMaxHP > 0)
            {
                f32 hpX = VW - 310, hpY = 14;
                draw2d::drawRect(hpX - 10, 10, 300, 40, 0,0,0, 0.5f);
                draw2d::drawRectBorder(hpX - 10, 10, 300, 40, 0.7f,0.3f,0.3f,0.6f, 2);

                if (m_hasFont)
                    m_hudText.drawText("HP:", hpX, hpY, 1,0.3f,0.3f, 0.9f);

                f32 barX = hpX + 50, barW = 220, barH = 24;
                f32 hpRatio = (f32)m_playerHP / (f32)m_playerMaxHP;
                hpRatio = std::max(0.0f, std::min(1.0f, hpRatio));

                draw2d::drawRect(barX, hpY, barW, barH, 0.15f,0,0, 0.8f);
                f32 gr = (hpRatio > 0.3f) ? 0.1f : 0.9f;
                f32 gg = (hpRatio > 0.3f) ? 0.8f : 0.1f;
                draw2d::drawRect(barX, hpY, barW * hpRatio, barH, gr, gg, 0.1f, 0.9f);
                draw2d::drawRectBorder(barX, hpY, barW, barH, 0.5f,0.5f,0.5f,0.6f, 1.5f);

                if (m_hasFont)
                    m_hudText.drawText(std::to_string(m_playerHP) + "/"
                        + std::to_string(m_playerMaxHP),
                        barX + barW*0.5f - 20, hpY, 1,1,1, 0.9f);
            }

            // ── NPC progress ──
            if (!m_npcs.empty())
            {
                int totalNPC = (int)m_npcs.size();
                int talkedNPC = 0;
                for (const auto& n : m_npcs) if (n.talked) talkedNPC++;

                draw2d::drawRect(220, VH-45, 200, 35, 0,0,0, 0.4f);
                std::string progress = "NPCs: " + std::to_string(talkedNPC)
                    + "/" + std::to_string(totalNPC);
                if (m_hasFont)
                {
                    f32 pr = (talkedNPC == totalNPC) ? 0.3f : 0.8f;
                    f32 pg = (talkedNPC == totalNPC) ? 0.9f : 0.8f;
                    m_hudText.drawText(progress, 228, VH-42, pr, pg, 0.3f, 0.8f);
                }
            }

            // ── Enemigos restantes ──
            if (!m_enemies.empty())
            {
                int alive = 0;
                for (const auto& e : m_enemies) if (e.alive) alive++;
                draw2d::drawRect(430, VH-45, 200, 35, 0,0,0, 0.4f);
                if (m_hasFont)
                    m_hudText.drawText("Enemigos: " + std::to_string(alive),
                        438, VH-42, 1,0.4f,0.4f, 0.8f);
            }

            // ── HP de la Base (Tier 4C Tower Defense) ──
            if (m_mapData.hudDef.showBaseHP && m_baseMaxHP > 0)
            {
                f32 bhX = VW - 310, bhY = 54;
                f32 flashAlpha = (m_baseHitFlash > 0) ? (0.3f + m_baseHitFlash) : 0;
                // Fondo (rojo pulsante si la base está siendo dañada)
                draw2d::drawRect(bhX - 10, 50, 300, 36,
                    0.15f + flashAlpha, 0, 0, 0.55f);
                draw2d::drawRectBorder(bhX - 10, 50, 300, 36,
                    0.95f, 0.75f, 0.25f, 0.7f, 2);

                if (m_hasFont)
                    m_hudText.drawText("BASE:", bhX, bhY, 1.0f, 0.85f, 0.3f, 0.95f);

                f32 barX = bhX + 74, barW = 196, barH = 20;
                f32 ratio = (f32)m_baseHP / (f32)m_baseMaxHP;
                ratio = std::max(0.0f, std::min(1.0f, ratio));

                draw2d::drawRect(barX, bhY + 2, barW, barH, 0.15f, 0, 0, 0.85f);
                f32 br = (ratio > 0.4f) ? 0.95f : 0.95f;
                f32 bg = (ratio > 0.4f) ? 0.75f : 0.25f;
                f32 bb = 0.25f;
                draw2d::drawRect(barX, bhY + 2, barW * ratio, barH, br, bg, bb, 0.95f);
                draw2d::drawRectBorder(barX, bhY + 2, barW, barH, 0.5f, 0.5f, 0.5f, 0.6f, 1.5f);

                if (m_hasFont)
                    m_hudText.drawText(std::to_string(m_baseHP) + "/"
                        + std::to_string(m_baseMaxHP),
                        barX + barW * 0.5f - 20, bhY + 2, 1, 1, 1, 0.95f);
            }

            // ── Oleada actual ──
            if (m_mapData.waveConfig.defined && m_mapData.hudDef.showWave && m_hasFont)
            {
                int total = (int)m_mapData.waveConfig.waves.size();
                int current = m_wavesComplete ? total : m_currentWave + 1;
                std::string waveText = "Oleada " + std::to_string(current)
                    + "/" + std::to_string(total);
                draw2d::drawRect(VW * 0.5f - 80, 56, 160, 34, 0, 0, 0, 0.5f);
                draw2d::drawRectBorder(VW * 0.5f - 80, 56, 160, 34, 0.9f, 0.6f, 0.2f, 0.7f, 2);
                m_hudText.drawText(waveText, VW * 0.5f - 65, 60, 1, 0.8f, 0.3f, 0.95f);
            }

            // ── Score / contador custom ──
            if (!m_mapData.hudDef.scoreVar.empty() && m_hasFont)
            {
                std::string val = getVar(m_mapData.hudDef.scoreVar);
                if (val.empty()) val = "0";
                std::string label = m_mapData.hudDef.scoreLabel.empty()
                    ? m_mapData.hudDef.scoreVar : m_mapData.hudDef.scoreLabel;
                draw2d::drawRect(VW - 240, VH-45, 230, 35, 0,0,0, 0.4f);
                m_hudText.drawText(label + ": " + val,
                    VW - 232, VH-42, 1,1,0.5f, 0.9f);
            }

            // ── Contador declarativo (G2D-HUD1) ──
            // Cuarto slot de la banda inferior: evita el D-pad de la esquina
            // derecha y no cambia la posicion historica de `score`.
            if (!m_mapData.hudDef.counterVar.empty() && m_hasFont)
            {
                const auto counterText = gameplay2DFormatHudCounter(
                    m_mapData.hudDef.counterFormat,
                    getVar(m_mapData.hudDef.counterVar));
                if (counterText)
                {
                    constexpr f32 counterX = 640.0f;
                    const f32 counterY = VH - 45.0f;
                    draw2d::drawRect(
                        counterX, counterY, 230, 35, 0,0,0, 0.4f);
                    m_hudText.drawText(
                        *counterText, counterX + 8, counterY + 3,
                        1,1,0.5f, 0.9f);
                }
            }

            // ── Texto fijo ──
            if (!m_mapData.hudDef.fixedText.empty() && m_hasFont)
            {
                f32 tw = m_mapData.hudDef.fixedText.size() * 10.0f;
                f32 tx = VW*0.5f - tw*0.5f;
                draw2d::drawRect(tx - 10, 56, tw + 20, 34, 0,0,0, 0.4f);
                m_hudText.drawText(m_mapData.hudDef.fixedText,
                    tx, 60, 0.9f,0.9f,0.7f, 0.8f);
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // Cámara
        // ══════════════════════════════════════════════════════════════════

        void updateCamera()
        {
            const f32 VW = (f32)viewport::width();
            const f32 VH = (f32)viewport::height();
            f32 mapW = m_tilemap.worldW();
            f32 mapH = m_tilemap.worldH();

            // Zoom interpolation
            if (m_cameraZoomDuration > 0 && m_cameraZoomTimer < m_cameraZoomDuration)
            {
                m_cameraZoomTimer += 1.0f / 60.0f; // Approx dt
                f32 t = std::min(1.0f, m_cameraZoomTimer / m_cameraZoomDuration);
                t = t * t * (3.0f - 2.0f * t); // SmoothStep
                m_cameraZoom = m_cameraZoomStart + (m_cameraZoomTarget - m_cameraZoomStart) * t;
            }

            f32 effectiveVW = VW / m_cameraZoom;
            f32 effectiveVH = VH / m_cameraZoom;

            if (m_cameraPanning)
            {
                // Pan interpolation
                m_cameraPanTimer += 1.0f / 60.0f;
                f32 t = (m_cameraPanDuration > 0)
                    ? std::min(1.0f, m_cameraPanTimer / m_cameraPanDuration)
                    : 1.0f;
                t = t * t * (3.0f - 2.0f * t); // SmoothStep

                m_camX = m_cameraPanStartX + (m_cameraPanTargetX - m_cameraPanStartX) * t;
                m_camY = m_cameraPanStartY + (m_cameraPanTargetY - m_cameraPanStartY) * t;

                if (t >= 1.0f) m_cameraPanning = false;
            }
            else if (m_cameraFollowPlayer)
            {
                f32 targetX = m_posX + m_ballSize*0.5f - effectiveVW*0.5f;
                f32 targetY = m_posY + m_ballSize*0.5f - effectiveVH*0.5f;

                m_camX = (mapW <= effectiveVW) ? (mapW-effectiveVW)*0.5f
                    : std::max(0.0f, std::min(targetX, mapW-effectiveVW));
                m_camY = (mapH <= effectiveVH) ? (mapH-effectiveVH)*0.5f
                    : std::max(0.0f, std::min(targetY, mapH-effectiveVH));
            }
            // Else: cámara estática (ni pan ni follow)
        }

        // ── Ejecutar la carga del nuevo mapa ────────────────────────────
        void performMapTransition()
        {
            std::cout << "[Gameplay2D] Cargando mapa: " << m_pendingMapPath << std::endl;

            // Limpiar estado runtime
            m_enemies.clear();
            m_towers.clear();
            m_buildModeTowerName.clear();
            m_buildInvalidFlash = 0.0f;
            m_items.clear();
            m_triggers.clear();
            m_spatialMarkers.clear();
            m_npcs.clear();
            m_notifications.clear();
            m_wallColliders.clear();

            // Reset del jugador (mantener HP entre mapas)
            m_lastDir = Dir::Idle;
            m_currentTex = &m_texIdle;
            m_invincibleTimer = 0;
            m_attackTimer = 0;
            m_attackVisualTimer = 0;
            m_cameraShakeTimer = 0;
            m_tileDamageTimer = 0;
            m_velX = 0;
            m_velY = 0;
            m_nearNPC = -1;
            m_talkingToNPC = -1;
            m_exitTimerCurrent = 0;
            m_totalTime = 0;
            m_knockVelX = 0; m_knockVelY = 0;
            m_isDashing = false;
            m_dashTimer = 0; m_dashCooldownTimer = 0;
            m_isWallSliding = false;
            m_projectiles.clear();
            m_cameraPanning = false;
            m_cameraFollowPlayer = true;
            m_cameraZoom = 1.0f;
            m_cameraZoomDuration = 0;
            m_waitingAction = false;
            m_baseHitFlash = 0;
            m_baseDestroyed = false;

            // Limpiar checkpoint (nuevo mapa = nuevo checkpoint)
            m_hasCheckpoint = false;

            // Preservar HP entre mapas
            int savedHP = m_playerHP;

            // Reset Lua para nuevo mapa (Tier 4)
            m_hooks.clear();
            m_avisadosLua.clear();   // CP8: mapa nuevo, avisos utiles de nuevo
            m_lua.reset();
            m_nextEnemyId = 1;
            m_nextTowerId = 1;
            m_nextActivityInhibitionHandle = 1;
            m_nextActivityCostHandle = 1;
            m_nextActivityIntervalFactorHandle = 1;
            m_nextActivityRangeFactorHandle = 1;
            m_nextActivityDirectDamageDeltaHandle = 1;
            m_nextActivityPreferenceOverrideHandle = 1;
            m_luaExitFlag = false;
            m_insideHook = false;
            m_hasUpdateHook = false;

            // Cargar el nuevo mapa
            m_mapPath = m_pendingMapPath;
            loadFromFile(m_mapPath);
            loadPlayerPresentation();

            // Re-register Lua bindings y ejecutar script del nuevo mapa
            registerWorldBindings();
            if (!m_mapData.luaScript.empty())
            {
                std::string scriptDir = m_mapPath.empty() ? "" :
                    std::filesystem::path(m_mapPath).parent_path().string();
                m_lua.executeFile(m_mapData.luaScript, scriptDir);
            }

            // Restaurar HP (no resetear al cambiar de mapa)
            if (savedHP > 0)
                m_playerHP = std::min(savedHP, m_playerMaxHP);

            // Restaurar estado "talked" de NPCs desde variables
            for (auto& npc : m_npcs)
            {
                std::string talkVar = "_npc_" + std::string(1, npc.tile) + "_talked";
                if (getVar(talkVar) == "1")
                    npc.talked = true;
            }

            // Override posición de spawn
            if (m_pendingSpawnX > 0 || m_pendingSpawnY > 0)
            {
                m_posX = m_pendingSpawnX;
                m_posY = m_pendingSpawnY;
            }

            // Re-aplicar audio
            if (m_hasAudio && !m_musicPath.empty())
            {
                m_audio.playMusic(m_musicPath, true);
                m_audio.setMusicVolume(Settings::musicVolume());
                m_audio.setSoundVolume(Settings::sfxVolume());
            }

            // Transición completada
            m_transitioning = false;
            m_exiting = false;
            m_fadeOutAlpha = 0;
            m_fadeInAlpha = 1.0f; // Fade in al nuevo mapa

            std::cout << "[Gameplay2D] Mapa cargado. Spawn: ("
                << m_posX << ", " << m_posY << ")" << std::endl;
        }

        // ── Colocar torre en (col, row) ─────────────────────────────────
        // Tier 4C. Valida y coloca una torre. Retorna true si tuvo éxito.
        // Usado desde world.place_tower(...) y (futuro) modo construcción.
        // ── Buscar una torre por letra de tile o por nombre (Tier 4C) ──
        const MapTowerDef* findTowerDef(const std::string& nameOrTile) const
        {
            if (nameOrTile.size() == 1)
            {
                for (const auto& t : m_mapData.towers)
                    if (t.tile == nameOrTile[0]) return &t;
            }
            for (const auto& t : m_mapData.towers)
                if (t.name == nameOrTile) return &t;
            return nullptr;
        }

        // ── Validación pura: ¿se puede colocar aquí? ──────────────────
        // Sin efectos secundarios. Usado por render, Lua (can_place), y
        // internamente por placeTowerAt.
        // verbose=true → escupe prints de por qué falló. Úsalo solo al
        // intentar colocar, NO cada frame.
        bool canPlaceTower(const std::string& nameOrTile, int col, int row,
            const MapTowerDef** outDef = nullptr,
            bool verbose = false) const
        {
            const MapTowerDef* def = findTowerDef(nameOrTile);
            if (outDef) *outDef = def;
            if (!def)
            {
                if (verbose) std::cout << "[Gameplay2D] Torre no encontrada: '"
                    << nameOrTile << "'" << std::endl;
                return false;
            }

            if (col < 0 || col >= m_tilemap.gridW() ||
                row < 0 || row >= m_tilemap.gridH())
            {
                if (verbose) std::cout << "[Gameplay2D] place_tower: ("
                    << col << "," << row
                    << ") fuera del mapa" << std::endl;
                return false;
            }

            char ch = m_tilemap.getCharAt(row, col);
            if (m_tilemap.isSolidAt(col, row))
            {
                if (verbose) std::cout << "[Gameplay2D] place_tower: tile ('"
                    << ch << "') no es colocable" << std::endl;
                return false;
            }

            if (!def->whereSlot.empty())
            {
                bool slotOk = false;
                for (const auto& td : m_mapData.tileDefs)
                {
                    if (td.tile == ch && td.type == MapTileType::TowerSlot)
                    {
                        slotOk = true; break;
                    }
                }
                if (!slotOk)
                {
                    if (verbose) std::cout << "[Gameplay2D] place_tower: '"
                        << def->name << "' requiere slot_torre"
                        << std::endl;
                    return false;
                }
            }

            // Colisión con otra torre en la misma celda
            f32 px = col * Tilemap::TILE_SIZE;
            f32 py = row * Tilemap::TILE_SIZE;
            for (const auto& t : m_towers)
            {
                if (t.isDeployed() &&
                    std::abs(t.box.x - px) < 1.0f &&
                    std::abs(t.box.y - py) < 1.0f)
                {
                    if (verbose) std::cout << "[Gameplay2D] place_tower: ya hay torre en ("
                        << col << "," << row << ")" << std::endl;
                    return false;
                }
            }

            return true;
        }

        static Gameplay2DAbilityRuntime makeAbilityRuntime(
            const Gameplay2DAbilityUseDef& authoredUse)
        {
            Gameplay2DAbilityRuntime runtimeUse;
            runtimeUse.useId = authoredUse.id;
            runtimeUse.abilityId = authoredUse.abilityId;
            runtimeUse.effective = authoredUse.effective;
            runtimeUse.implicitLegacy = authoredUse.implicitLegacy;
            return runtimeUse;
        }

        static TowerRuntime makeTowerRuntime(
            const MapTowerDef& def,
            const TileAABB& box,
            int id)
        {
            TowerRuntime placed;
            placed.box = box;
            placed.name = def.name;
            placed.tile = def.tile;
            placed.r = def.r;
            placed.g = def.g;
            placed.b = def.b;
            placed.spritePath = def.spritePath;
            placed.maxHP = def.hp;
            placed.hp = def.hp;
            placed.cost = def.cost;
            placed.abilityUses.reserve(def.abilityUses.size());
            for (const auto& authoredUse : def.abilityUses)
                placed.abilityUses.push_back(
                    makeAbilityRuntime(authoredUse));
            placed.whereSlot = def.whereSlot;
            placed.obstructsLocomotion = def.obstructsLocomotion;
            placed.onPlace = def.onPlace;
            placed.onDestroy = def.onDestroy;
            placed.id = id;
            return placed;
        }

        EnemyRuntime* findLiveEnemy(int id)
        {
            const auto enemy = std::find_if(
                m_enemies.begin(), m_enemies.end(),
                [&](const EnemyRuntime& candidate) {
                    return candidate.alive && candidate.id == id;
                });
            return enemy == m_enemies.end() ? nullptr : &*enemy;
        }

        TowerRuntime* findLivePlaced(int id)
        {
            const auto placed = std::find_if(
                m_towers.begin(), m_towers.end(),
                [&](const TowerRuntime& candidate) {
                    return candidate.alive && candidate.id == id;
                });
            return placed == m_towers.end() ? nullptr : &*placed;
        }

        bool restorePlacedHp(int id)
        {
            TowerRuntime* placed = findLivePlaced(id);
            if (!placed || placed->maxHP <= 0 || placed->hp <= 0 ||
                placed->hp >= placed->maxHP)
            {
                return false;
            }

            // Misma identidad: no se tocan timers, targets, inhibiciones,
            // costes, factores, deltas u overrides de actividad, referencias,
            // presentacion ni callbacks.
            placed->hp = placed->maxHP;
            return true;
        }

        bool extractPlaced(int id)
        {
            TowerRuntime* placed = findLivePlaced(id);
            if (!placed || !placed->isDeployed())
                return false;

            // No toca vida, posicion recordada, cooldowns, targets,
            // inhibiciones, costes, factores, deltas u overrides de actividad,
            // coste de
            // definicion, presentacion base ni callbacks.
            placed->presence = Gameplay2DPlacedPresence::Custodied;
            return true;
        }

        bool redeployPlaced(int id, int col, int row)
        {
            TowerRuntime* placed = findLivePlaced(id);
            if (!placed ||
                placed->presence != Gameplay2DPlacedPresence::Custodied)
            {
                return false;
            }

            const MapTowerDef* def = nullptr;
            if (!canPlaceTower(
                    placed->name, col, row, &def, /*verbose=*/true) ||
                !def || def->tile != placed->tile)
            {
                return false;
            }

            // Validar primero; posicion y presencia se comprometen juntas.
            placed->box = {
                col * (f32)Tilemap::TILE_SIZE,
                row * (f32)Tilemap::TILE_SIZE,
                (f32)Tilemap::TILE_SIZE,
                (f32)Tilemap::TILE_SIZE
            };
            placed->presence = Gameplay2DPlacedPresence::Deployed;
            return true;
        }

        int replacePlaced(int id)
        {
            const auto placed = std::find_if(
                m_towers.begin(), m_towers.end(),
                [&](const TowerRuntime& candidate) {
                    return candidate.alive && candidate.id == id;
                });
            if (placed == m_towers.end() || !placed->isDeployed() ||
                placed->maxHP <= 0 ||
                placed->hp <= 0 || placed->hp >= placed->maxHP)
            {
                return -1;
            }

            // R1 minimo rematerializa la misma definicion. Aceptar otra forma
            // abriria transformacion/progresion y su propia compatibilidad.
            const MapTowerDef* def =
                findTowerDef(std::string(1, placed->tile));
            if (!def)
                return -1;

            const std::size_t oldIndex = static_cast<std::size_t>(
                std::distance(m_towers.begin(), placed));
            const int replacementId = m_nextTowerId;
            TowerRuntime replacement = makeTowerRuntime(
                *def, placed->box, replacementId);

            // Construir/validar precede al commit. No se publican callbacks
            // ni existe una ventana de update/render entre ambos pasos.
            m_towers.push_back(std::move(replacement));
            m_towers[oldIndex].alive = false;
            m_towers[oldIndex].hp = 0;
            ++m_nextTowerId;
            return replacementId;
        }

        // ── Colocar torre (con efectos: onPlace + hook) ───────────────
        bool placeTowerAt(const std::string& nameOrTile, int col, int row)
        {
            const MapTowerDef* def = nullptr;
            if (!canPlaceTower(nameOrTile, col, row, &def, /*verbose=*/true))
                return false;

            f32 px = col * Tilemap::TILE_SIZE;
            f32 py = row * Tilemap::TILE_SIZE;

            TowerRuntime t = makeTowerRuntime(
                *def,
                { px, py,
                  (f32)Tilemap::TILE_SIZE,
                  (f32)Tilemap::TILE_SIZE },
                m_nextTowerId++);
            m_towers.push_back(t);

            executeActions(t.onPlace, t.box.x, t.box.y);
            fireHook("tower_placed", t.name, t.id, col, row);

            std::cout << "[Gameplay2D] Torre colocada: '" << t.name
                << "' id=" << t.id << " en (" << col << "," << row << ")"
                << std::endl;
            return true;
        }

        // ══════════════════════════════════════════════════════════════════
        // Selección neutral de objetivos — primer caller: torres
        // ══════════════════════════════════════════════════════════════════
        //
        // El adapter de torre fotografía enemigos una sola vez. La política
        // pura recibe candidatos neutrales y no conoce torres, SDL, Lua ni
        // MapData.

        std::vector<Gameplay2DTargetCandidate>
        snapshotAbilityTargets() const
        {
            std::vector<Gameplay2DTargetCandidate> candidates;
            candidates.reserve(m_enemies.size());
            for (const auto& enemy : m_enemies)
            {
                Gameplay2DTargetCandidate candidate;
                candidate.address = {
                    Gameplay2DActorDomain::Enemy,
                    enemy.id
                };
                candidate.centerX =
                    enemy.box.x + enemy.box.w * 0.5f;
                candidate.centerY =
                    enemy.box.y + enemy.box.h * 0.5f;
                candidate.currentHp = enemy.hp;
                candidate.alive = enemy.alive;
                candidate.invulnerable = enemy.invulnerable;
                candidate.hasRoute = !enemy.pathPoints.empty();
                candidate.routeIndex = enemy.pathIndex;
                candidate.relation =
                    Gameplay2DTargetRelation::Opposing;
                candidate.capabilities.push_back(
                    Gameplay2DTargetCapability::Damageable);
                candidates.push_back(candidate);
            }
            return candidates;
        }

        // ══════════════════════════════════════════════════════════════════
        // Tier 4C.3: Daño a torres + muerte
        // ══════════════════════════════════════════════════════════════════

        void damageTower(TowerRuntime& t, int amount)
        {
            if (!t.isDeployed() || t.maxHP <= 0) return;
            t.hp -= amount;
            t.hitFlash = 0.15f;
            if (t.hp <= 0)
            {
                t.hp = 0;
                t.alive = false;
                executeActions(t.onDestroy, t.box.x, t.box.y);
                fireHook("tower_destroyed", t.name, t.id);
                std::cout << "[Gameplay2D] Torre destruida: '" << t.name
                    << "' id=" << t.id << std::endl;
            }
        }

        // Busca la torre viva más cercana al enemigo. Retorna nullptr si
        // no hay ninguna a ≤ maxRange del centro del enemigo.
        TowerRuntime* findNearestTowerInRange(const EnemyRuntime& e, float maxRange)
        {
            f32 ecx = e.box.x + e.box.w * 0.5f;
            f32 ecy = e.box.y + e.box.h * 0.5f;
            f32 r2 = maxRange * maxRange;

            TowerRuntime* best = nullptr;
            f32 bestD2 = 0;
            for (auto& t : m_towers)
            {
                if (!t.isDeployed() || t.maxHP <= 0)
                    continue;   // Tier 4C.5: ignorar indestructibles
                f32 tcx = t.box.x + t.box.w * 0.5f;
                f32 tcy = t.box.y + t.box.h * 0.5f;
                f32 dx = tcx - ecx;
                f32 dy = tcy - ecy;
                f32 d2 = dx * dx + dy * dy;
                if (d2 > r2) continue;
                if (!best || d2 < bestD2) { best = &t; bestD2 = d2; }
            }
            return best;
        }

        // ══════════════════════════════════════════════════════════════════
        // Tier 4C.2: Update de torres (auto-targeting + disparo)
        // ══════════════════════════════════════════════════════════════════

        void updateTowers(f64 dt)
        {
            const std::vector<Gameplay2DTargetCandidate> candidates =
                snapshotAbilityTargets();
            std::vector<Gameplay2DTargetRequest> requests;

            // Fase 1: avanzar relojes y fijar todas las oportunidades del tick.
            for (auto& t : m_towers)
            {
                if (!t.isDeployed()) continue;

                if (t.hitFlash > 0) t.hitFlash -= (f32)dt;

                for (auto& use : t.abilityUses)
                {
                    const float activityInterval =
                        use.activityInterval();
                    if (activityInterval <= 0.0f ||
                        !gameplay2DAbilityHasExecutableEffect(
                            use.effective) ||
                        gameplay2DAbilityHasConflictingEffects(
                            use.effective))
                        continue;

                    use.fireTimer += (f32)dt;
                    if (t.activityInhibitions.blocks(use.useId))
                    {
                        // I1 inhibe la nueva emision, no inventa otro reloj.
                        // El uso conserva hasta una oportunidad lista y no
                        // acumula una rafaga mientras permanece inhibido.
                        use.fireTimer = std::min(
                            use.fireTimer, activityInterval);
                        continue;
                    }
                    if (!gameplay2DActivityReady(
                            use.fireTimer, activityInterval))
                        continue;

                    Gameplay2DTargetRequest request;
                    request.actorId = t.id;
                    request.useId = use.useId;
                    request.sourceX = t.box.x + t.box.w * 0.5f;
                    request.sourceY = t.box.y + t.box.h * 0.5f;
                    request.range = use.activityRange();
                    request.preference = use.activityPreference();
                    request.acquisition =
                        use.effective.acquisition;
                    const auto binding =
                        use.effective.acquisition.binding.value_or(
                            Gameplay2DTargetBinding::Independent);
                    if (binding == Gameplay2DTargetBinding::
                            CurrentLocomotionBlocker)
                    {
                        // Una torre no posee encuentro de locomocion. La
                        // direccion invalida presente conserva el contrato
                        // estricto y evita seleccionar otro candidato.
                        request.requiredTargetAddress =
                            Gameplay2DActorAddress{};
                    }
                    request.coordination =
                        use.effective.coordination;
                    request.directDamage =
                        use.activityDirectDamage();
                    for (const auto& binding : t.activityCosts)
                    {
                        if (binding.useId == use.useId)
                            request.commitCosts.push_back(binding.cost);
                    }
                    requests.push_back(std::move(request));
                }
            }

            const std::vector<Gameplay2DCommitResourceBalance>
                resourceBalances =
                    snapshotActivityCommitResources(requests);
            const std::vector<Gameplay2DTargetDecision> decisions =
                selectGameplay2DTargets(
                    requests, candidates, resourceBalances);

            struct CommittedShot
            {
                float originX = 0.0f;
                float originY = 0.0f;
                float sourceX = 0.0f;
                float sourceY = 0.0f;
                std::string projectile;
                Gameplay2DActorAddress targetAddress;
            };
            std::vector<CommittedShot> shots;
            shots.reserve(decisions.size());
            std::vector<Gameplay2DDirectDamageEffect> directEffects;
            directEffects.reserve(decisions.size());

            // Fase 2: comprometer el lote completo. Ningún efecto se publica
            // hasta que todos los pares actor/uso ya tienen su decisión.
            for (const auto& decision : decisions)
            {
                auto towerIt = std::find_if(
                    m_towers.begin(), m_towers.end(),
                    [&](const TowerRuntime& tower) {
                        return tower.id == decision.actorId;
                    });
                if (towerIt == m_towers.end() ||
                    !towerIt->isDeployed())
                    continue;

                auto useIt = std::find_if(
                    towerIt->abilityUses.begin(),
                    towerIt->abilityUses.end(),
                    [&](const Gameplay2DAbilityRuntime& use) {
                        return use.useId == decision.useId;
                    });
                if (useIt == towerIt->abilityUses.end())
                    continue;

                useIt->targetAddress = decision.targetAddress;
                if (!decision.targetAddress.valid())
                {
                    // Conserva la respuesta inmediata al aparecer un blanco,
                    // sin acumular un burst ilimitado.
                    useIt->fireTimer = std::min(
                        useIt->fireTimer,
                        useIt->activityInterval());
                    continue;
                }

                if (!commitActivityResources(decision.committedCosts))
                {
                    // Invariante defensiva: entre snapshot y esta fase no hay
                    // callbacks. Si una futura costura introduce deriva, el
                    // uso conserva su oportunidad y no publica efecto parcial.
                    useIt->targetAddress = {};
                    useIt->fireTimer = std::min(
                        useIt->fireTimer,
                        useIt->activityInterval());
                    std::cout
                        << "[Gameplay2D] coste de actividad cambio durante el lote; "
                        << "commit omitido para Placed " << towerIt->id
                        << " uso '" << useIt->useId << "'" << std::endl;
                    continue;
                }

                useIt->fireTimer = 0.0f;
                if (gameplay2DAbilityHasDirectDamage(
                        useIt->effective))
                {
                    const auto committedRequest = std::find_if(
                        requests.begin(), requests.end(),
                        [&](const Gameplay2DTargetRequest& request) {
                            return request.actorId == decision.actorId &&
                                request.useId == decision.useId;
                        });
                    if (committedRequest == requests.end() ||
                        committedRequest->directDamage <= 0)
                    {
                        continue;
                    }
                    directEffects.push_back({
                        { Gameplay2DActorDomain::Placed, towerIt->id },
                        useIt->useId,
                        decision.targetAddress,
                        committedRequest->directDamage
                    });
                    continue;
                }

                CommittedShot shot;
                shot.originX = towerIt->box.x;
                shot.originY = towerIt->box.y;
                shot.sourceX =
                    towerIt->box.x + towerIt->box.w * 0.5f;
                shot.sourceY =
                    towerIt->box.y + towerIt->box.h * 0.5f;
                shot.projectile = useIt->effective.projectile;
                shot.targetAddress = decision.targetAddress;
                shots.push_back(std::move(shot));
            }

            // Fase 3: publicar efectos desde el mismo snapshot inmutable.
            for (const auto& shot : shots)
            {
                const auto target = std::find_if(
                    candidates.begin(), candidates.end(),
                    [&](const Gameplay2DTargetCandidate& candidate) {
                        return candidate.address ==
                            shot.targetAddress;
                    });
                if (target == candidates.end())
                    continue;

                spawnProjectile(
                    shot.projectile,
                    shot.originX,
                    shot.originY,
                    /*friendly=*/true,
                    target->centerX - shot.sourceX,
                    target->centerY - shot.sourceY);
            }

            applyDirectDamageBatch(directEffects);
        }

        // ══════════════════════════════════════════════════════════════════
        // Proyectiles
        // ══════════════════════════════════════════════════════════════════

        // Tier 4C.2: Ahora acepta dirección opcional (dirX, dirY).
        // - Si ambos son 0 → usa lógica legacy (player usa m_lastDir,
        //   enemy apunta al jugador).
        // - Si viene una dirección → normaliza y aplica en esa dirección
        //   (usado por torres para apuntar al blanco).
        void spawnProjectile(const std::string& tileName,
            float originX, float originY, bool friendly,
            float dirX = 0.0f, float dirY = 0.0f)
        {
            const MapProjectileDef* def = nullptr;
            for (const auto& p : m_mapData.projectiles)
            {
                if (std::string(1, p.tile) == tileName || p.name == tileName)
                {
                    def = &p; break;
                }
            }

            if (!def)
            {
                std::cout << "[Gameplay2D] Proyectil no encontrado: " << tileName << std::endl;
                return;
            }

            ProjectileRuntime proj;
            float halfSize = def->size * 0.5f;
            proj.box = { originX + Tilemap::TILE_SIZE * 0.5f - halfSize,
                         originY + Tilemap::TILE_SIZE * 0.5f - halfSize,
                         def->size, def->size };
            proj.damage = def->damage;
            proj.lifetime = def->lifetime;
            proj.friendly = friendly;
            proj.pierceWalls = def->pierceWalls;
            proj.hasGravity = def->hasGravity;
            proj.r = def->r; proj.g = def->g; proj.b = def->b;

            // ── Caso 1: dirección explícita (torres apuntando a blanco) ──
            if (dirX != 0.0f || dirY != 0.0f)
            {
                f32 dist = std::sqrt(dirX * dirX + dirY * dirY);
                if (dist > 0.01f)
                {
                    proj.velX = (dirX / dist) * def->speed;
                    proj.velY = (dirY / dist) * def->speed;
                }
                else
                {
                    proj.velX = def->speed; proj.velY = 0;
                }
            }
            // ── Caso 2: legacy (player usa m_lastDir; enemy apunta al jugador) ──
            else if (friendly)
            {
                switch (m_lastDir) {
                case Dir::Up:    proj.velX = 0;           proj.velY = -def->speed; break;
                case Dir::Down:  proj.velX = 0;           proj.velY = def->speed;  break;
                case Dir::Left:  proj.velX = -def->speed; proj.velY = 0;           break;
                case Dir::Right: proj.velX = def->speed;  proj.velY = 0;           break;
                default:         proj.velX = def->speed;  proj.velY = 0;           break;
                }
                if (m_isSideScroll)
                {
                    proj.velX = m_facingRight ? def->speed : -def->speed;
                    proj.velY = 0;
                }
            }
            else
            {
                f32 dx = m_posX - originX;
                f32 dy = m_posY - originY;
                f32 dist = std::sqrt(dx * dx + dy * dy);
                if (dist > 0.01f)
                {
                    proj.velX = (dx / dist) * def->speed; proj.velY = (dy / dist) * def->speed;
                }
                else
                {
                    proj.velX = def->speed; proj.velY = 0;
                }
            }

            m_projectiles.push_back(proj);
        }

        void updateProjectiles(f64 dt)
        {
            TileAABB pb = playerBox();

            for (auto& p : m_projectiles)
            {
                if (!p.alive) continue;

                p.box.x += p.velX * (f32)dt;
                p.box.y += p.velY * (f32)dt;

                if (p.hasGravity)
                    p.velY += m_gravity * 0.5f * (f32)dt;

                p.timer += (f32)dt;
                if (p.timer >= p.lifetime) { p.alive = false; continue; }

                if (p.box.x < -100 || p.box.x > m_tilemap.worldW() + 100 ||
                    p.box.y < -100 || p.box.y > m_tilemap.worldH() + 100)
                { p.alive = false; continue; }

                if (!p.pierceWalls)
                {
                    if (m_tilemap.isSolidInArea(p.box.x, p.box.y, p.box.w, p.box.h))
                    {
                        p.alive = false; continue;
                    }
                }

                if (p.friendly)
                {
                    for (auto& e : m_enemies)
                    {
                        if (!e.alive || e.invulnerable) continue;
                        if (p.box.overlaps(e.box))
                        {
                            e.hp -= p.damage;
                            e.hitFlash = 0.15f;
                            f32 dx = e.box.x - p.box.x;
                            f32 dy = e.box.y - p.box.y;
                            f32 dist = std::sqrt(dx*dx + dy*dy);
                            if (dist > 0.01f)
                            { e.knockVelX = (dx/dist) * 200.0f; e.knockVelY = (dy/dist) * 200.0f; }

                            if (e.hp <= 0)
                            {
                                commitEnemyRemoval(
                                    e, EnemyRemovalCause::Defeated);
                            }

                            p.alive = false;
                            break;
                        }
                    }
                }
                else
                {
                    // ── Tier 4C.3: proyectiles enemigos también dañan torres ──
                    bool hitTower = false;
                    for (auto& t : m_towers)
                    {
                        if (!t.isDeployed() || t.maxHP <= 0)
                            continue;   // indestructibles ignoran
                        if (p.box.overlaps(t.box))
                        {
                            damageTower(t, p.damage);
                            p.alive = false;
                            hitTower = true;
                            break;
                        }
                    }
                    if (hitTower) continue;

                    if (m_playerHP > 0 && m_invincibleTimer <= 0 && pb.overlaps(p.box))
                    {
                        m_playerHP -= p.damage;
                        m_invincibleTimer = m_invincibleDuration;
                        m_cameraShakeTimer = 0.2f;
                        fireHook("player_damage", p.damage);

                        f32 dx = m_posX - p.box.x;
                        f32 dy = m_posY - p.box.y;
                        f32 dist = std::sqrt(dx*dx + dy*dy);
                        if (dist > 0.01f)
                        { m_knockVelX = (dx/dist) * 250.0f; m_knockVelY = (dy/dist) * 250.0f; }

                        p.alive = false;

                        if (m_playerHP <= 0)
                        {
                            pushLevelEnd(false);
                        }
                    }
                }
            }

            m_projectiles.erase(
                std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                    [](const ProjectileRuntime& p) { return !p.alive; }),
                m_projectiles.end());
        }

        // ── Reset del nivel (retry) ─────────────────────────────────────
        void resetLevel()
        {
            std::cout << "[Gameplay2D] Reiniciando nivel..." << std::endl;

            // Limpiar estado runtime
            m_enemies.clear();
            m_towers.clear();
            m_buildModeTowerName.clear();
            m_buildInvalidFlash = 0.0f;
            m_items.clear();
            m_triggers.clear();
            m_spatialMarkers.clear();
            m_npcs.clear();
            m_notifications.clear();
            m_wallColliders.clear();

            // Reset del jugador
            m_lastDir = Dir::Idle;
            m_currentTex = &m_texIdle;
            m_invincibleTimer = 0;
            m_attackTimer = 0;
            m_attackVisualTimer = 0;
            m_cameraShakeTimer = 0;
            m_tileDamageTimer = 0;
            m_velX = 0;
            m_velY = 0;
            // NO reseteamos checkpoint — sobrevive al retry
            m_gravityVelY = 0;
            m_isOnGround = false;
            m_coyoteTimer = 0;
            m_knockVelX = 0; m_knockVelY = 0;
            m_isDashing = false;
            m_dashTimer = 0; m_dashCooldownTimer = 0;
            m_isWallSliding = false;
            m_jumpsRemaining = m_maxJumps;
            m_projectiles.clear();
            m_cameraPanning = false;
            m_cameraFollowPlayer = true;
            m_cameraZoom = 1.0f;
            m_cameraZoomDuration = 0;
            m_waitingAction = false;
            m_currentWave = 0;
            m_waveTimer = 0;
            m_waveActive = false;
            m_waveSpawned = false;
            m_wavesComplete = false;
            m_baseHitFlash = 0;
            m_baseDestroyed = false;

            // Reset de estado de nivel
            m_fadeInAlpha = 1.0f;
            m_fadeOutAlpha = 0.0f;
            m_exiting = false;
            m_exitTimerCurrent = 0;
            m_totalTime = 0;
            m_nearNPC = -1;
            m_talkingToNPC = -1;
            m_transitioning = false;
            m_doorMessageShown = false;

            // Reset Lua (Tier 4)
            m_hooks.clear();
            m_avisadosLua.clear();   // CP8: mapa nuevo, avisos utiles de nuevo
            m_lua.reset();
            m_nextEnemyId = 1;
            m_nextTowerId = 1;
            m_nextActivityInhibitionHandle = 1;
            m_nextActivityCostHandle = 1;
            m_nextActivityIntervalFactorHandle = 1;
            m_nextActivityRangeFactorHandle = 1;
            m_nextActivityDirectDamageDeltaHandle = 1;
            m_nextActivityPreferenceOverrideHandle = 1;
            m_luaExitFlag = false;
            m_insideHook = false;
            m_hasUpdateHook = false;

            // Recargar el mapa (re-parsea y reconstruye todo)
            if (!m_mapPath.empty())
                loadFromFile(m_mapPath);
            else
                loadHardcoded();
            loadPlayerPresentation();

            // Re-register Lua bindings y ejecutar script
            registerWorldBindings();
            if (!m_mapData.luaScript.empty())
            {
                std::string scriptDir = m_mapPath.empty() ? "" :
                    std::filesystem::path(m_mapPath).parent_path().string();
                m_lua.executeFile(m_mapData.luaScript, scriptDir);
            }

            // Si hay checkpoint, usarlo como spawn
            if (m_hasCheckpoint)
            {
                m_posX = m_checkpointX;
                m_posY = m_checkpointY;
            }

            // Re-aplicar audio
            if (m_hasAudio && !m_musicPath.empty())
            {
                m_audio.playMusic(m_musicPath, true);
                m_audio.setMusicVolume(Settings::musicVolume());
                m_audio.setSoundVolume(Settings::sfxVolume());
            }

            std::cout << "[Gameplay2D] Nivel reiniciado. HP: "
                << m_playerHP << "/" << m_playerMaxHP << std::endl;
        }

        // ══════════════════════════════════════════════════════════════════
        // Save / Load (Tier 4C.6)
        // ══════════════════════════════════════════════════════════════════
        //
        // Serialización del estado del nivel 2D a un slot del archivo
        // compartido vn_saves.eclips. Se guardan los campos slot_N_gp_*:
        //
        //   mappath    — ruta del .eclmap activo (para recargar al load)
        //   posX/posY  — posición del jugador
        //   hp/maxhp   — HP actual y máximo
        //   cpX/cpY    — checkpoint actual
        //   hasCP      — ¿hay checkpoint válido?
        //   vars       — variables compartidas (VN↔2D) serializadas
        //   preview    — texto mostrado en el menú de slots
        //   time       — timestamp del guardado
        //
        // Lo que NO se guarda (decisión Nivel 2): enemigos vivos, torres
        // colocadas, oleada en curso, proyectiles en vuelo, base HP.
        // Al cargar, el nivel se reinicia fresco (enemigos, torres, oleadas
        // reseteados) y solo el player + checkpoint + vars se restauran.
        //
        // ══════════════════════════════════════════════════════════════════

        // ¿La policy del .eclmap permite guardar de alguna forma?
        bool canSaveAny() const
        {
            return m_mapData.savePolicy.allow != SaveAllow::Never;
        }

        // ¿La policy permite que el jugador guarde MANUALMENTE desde el menú?
        bool canSaveManual() const
        {
            return m_mapData.savePolicy.allow == SaveAllow::Always;
        }

        // Serializa shared vars a "key;val|key;val|..." (v2, piso 2).
        // A1.5 muerto: el '|' del valor se escapa a %7C; el ':' queda literal.
        std::string serializeVars() const
        {
            const auto& vars = m_sharedVars ? *m_sharedVars : m_localVars;
            std::vector<std::string> entries;
            for (const auto& [k, v] : vars)
                entries.push_back(eclips::savecodec::joinFields({ k, v }));
            return eclips::savecodec::joinEntries(entries);
        }

        // Parsea vars y reemplaza el contenido de shared vars.
        // Rutea por estampa slot_N_gp_ver (§7.212): ver<2 = lector legacy
        // CONGELADO (key:val|..., primer-':'); ver>=2 = v2 (key;val, piso 2).
        void restoreVarsFromString(const std::string& varsStr, int ver = 1)
        {
            auto& vars = m_sharedVars ? *m_sharedVars : m_localVars;
            vars.clear();
            if (varsStr.empty()) return;

            if (ver < 2)
            {
                // ── Lector legacy CONGELADO (§7.212): key:val|... primer-':' ──
                std::stringstream ss(varsStr);
                std::string pair;
                while (std::getline(ss, pair, '|'))
                {
                    size_t colon = pair.find(':');
                    if (colon != std::string::npos)
                    {
                        std::string key = pair.substr(0, colon);
                        std::string val = pair.substr(colon + 1);
                        vars[key] = val;
                    }
                }
            }
            else
            {
                // ── v2: key;val|... (piso 2). A1.5 muerto; ':' queda literal. ──
                for (const auto& e : eclips::savecodec::splitEntries(varsStr))
                {
                    if (e.empty()) continue;
                    auto f = eclips::savecodec::splitFields(e);
                    if (f.empty()) continue;
                    vars[f[0]] = (f.size() >= 2) ? f[1] : "";
                }
            }
        }

        // Guardar partida al slot N.
        void saveGameToSlot(int slot)
        {
            if (!canSaveAny())
            {
                std::cout << "[Gameplay2D] Save rechazado: policy=Never"
                    << std::endl;
                return;
            }

            // ── Preview: "Mapa: nombre · HP X/Y" ──
            std::string mapName;
            if (!m_mapPath.empty())
                mapName = std::filesystem::path(m_mapPath).stem().string();
            if (mapName.empty()) mapName = "(sin mapa)";

            // Idiomas CP3: UNA plantilla de tres huecos, no cuatro fragmentos.
            // El §5 firma que este preview es una FOTO del momento: se genera
            // al guardar y persiste, así que queda en el idioma que estaba
            // activo entonces. Misma política que el preview de la VN.
            std::string preview = eclips::langPoner(
                eclips::langUI("Mapa: {mapa} - HP {hp}/{hp_max}",
                               "Map: {mapa} - HP {hp}/{hp_max}"),
                {{"mapa",   mapName},
                 {"hp",     std::to_string(m_playerHP)},
                 {"hp_max", std::to_string(m_playerMaxHP)}});

            // ── Timestamp legible ──
            time_t now = time(nullptr);
            char tsBuf[32] = { 0 };
            std::tm localTm;
#ifdef _WIN32
            localtime_s(&localTm, &now);
#else
            localTm = *std::localtime(&now);
#endif
            std::strftime(tsBuf, sizeof(tsBuf), "%d/%m/%Y  %H:%M", &localTm);

            // ── Serializar y escribir ──
            std::string varsStr = serializeVars();

            // Resolver archivo: policy.saveFile si está seteado, si no compartido.
            const std::string& saveFile = m_mapData.savePolicy.saveFile;

            // CP3 — snapshot GP->VN (orden VN->GP, §6). Si hay VN abajo (slot
            // mixto), el VN se snapshotea COMPLETO primero: writeSlot escribe
            // scene/chars/layers/vars/anims, escribe slot_N_vars legitimo (de la
            // MISMA shared map que gp_vars -> UNA verdad, no dos: H7 muerto) y
            // borra gp_* rancio. Luego writeGameplaySlot escribe gp_* encima.
            // Standalone (m_vnSaveCb null): solo gp_*, sin slot_N_vars (§6).
            if (m_vnSaveCb) m_vnSaveCb(slot, saveFile);

            VNSaveMenu::writeGameplaySlot(slot,
                m_mapPath,
                m_posX, m_posY,
                m_playerHP, m_playerMaxHP,
                m_checkpointX, m_checkpointY, m_hasCheckpoint,
                varsStr, preview, tsBuf,
                saveFile, static_cast<long long>(now));

            std::cout << "[Gameplay2D] Save -> slot " << slot
                << " | " << preview << std::endl;

            m_notifications.push_back({ "Partida guardada", 0, 2.0f });
        }

        // Cargar partida desde el slot N.
        void loadGameFromSlot(int slot)
        {
            auto d = VNSaveMenu::loadSlotData(slot, m_mapData.savePolicy.saveFile);

            // §4.3: mitad GP con gramática del futuro (gp_ver>2) — rechazo
            // ruidoso por slot, jamás medio-parse (gemelo del bail VN de 6298).
            if (d.gpVer > 2)
            {
                std::cout << "[Gameplay2D] Slot " << slot
                    << " incompatible: slot_N_gp_ver=" << d.gpVer
                    << " (este engine lee hasta v2). No se carga." << std::endl;
                m_notifications.push_back({ "Slot incompatible", 0, 2.0f });
                return;
            }

            if (!d.gpHasGameplay)
            {
                std::cout << "[Gameplay2D] Slot " << slot
                    << " no tiene gameplay guardado" << std::endl;
                m_notifications.push_back({ "Slot sin partida", 0, 2.0f });
                return;
            }

            // En Paso 3: solo permitimos cargar si el mapa coincide.
            // El caso de "cargar otro mapa" lo resuelve el Paso 4 (loader
            // universal desde VN).
            if (d.gpMapPath != m_mapPath)
            {
                std::cout << "[Gameplay2D] Load abortado: el slot es de otro mapa ("
                    << d.gpMapPath << " vs " << m_mapPath << ")" << std::endl;
                m_notifications.push_back({ "Mapa distinto", 0, 2.0f });
                return;
            }

            // Reset del nivel (enemigos frescos, torres frescas, oleada en 0)
            resetLevel();

            // Restaurar estado del jugador
            m_posX = d.gpPosX;
            m_posY = d.gpPosY;
            m_playerHP = d.gpPlayerHP;
            if (d.gpPlayerMaxHP > 0)
                m_playerMaxHP = d.gpPlayerMaxHP;
            m_checkpointX = d.gpCheckpointX;
            m_checkpointY = d.gpCheckpointY;
            m_hasCheckpoint = d.gpHasCheckpoint;

            // Restaurar shared vars
            restoreVarsFromString(d.gpVarsStr, d.gpVer);

            std::cout << "[Gameplay2D] Load <- slot " << slot
                << " | pos=(" << m_posX << "," << m_posY << ")"
                << " HP=" << m_playerHP << "/" << m_playerMaxHP
                << " vars=" << d.gpVarsStr.size() << "B" << std::endl;

            m_notifications.push_back({ "Partida cargada", 0, 2.0f });
        }

        // Se llama automáticamente al tocar un tile tipo: checkpoint.
        // Si la policy es CheckpointOnly o Always, dispara auto-save al slot 0.
        void triggerAutoSaveIfAllowed()
        {
            if (m_mapData.savePolicy.allow == SaveAllow::CheckpointOnly ||
                m_mapData.savePolicy.allow == SaveAllow::Always)
            {
                std::cout << "[Gameplay2D] Auto-save disparado por checkpoint"
                    << std::endl;
                saveGameToSlot(0); // Slot 0 = quick save dedicado
            }
        }

        // Abre el menú dual con contexto Gameplay. Respeta policy.saveFile
        // y policy.slotsVisible del mapa actual.
        void openGameplaySaveMenu(VNSaveMenu::Mode mode)
        {
            m_stateStack->push(std::make_unique<VNSaveMenu>(
                m_window, m_stateStack, m_fontSources, mode,
                [this](int s) { saveGameToSlot(s); },
                [this](int s) { loadGameFromSlot(s); },
                VNSaveMenu::Context::Gameplay,
                m_mapData.savePolicy.saveFile,
                m_mapData.savePolicy.slotsVisible
            ));
        }

        // ══════════════════════════════════════════════════════════════════
        // Carga de nivel
        // ══════════════════════════════════════════════════════════════════

        // ── Construir mapa de visuals para Tilemap ──────────────────────
        std::map<char, TileVisualInfo> buildTileVisuals() const
        {
            std::map<char, TileVisualInfo> visuals;
            for (const auto& td : m_mapData.tileDefs)
            {
                TileVisualInfo vis;
                vis.solid = !td.walkable;

                switch (td.type)
                {
                case MapTileType::Damage:
                    vis.r = 0.8f; vis.g = 0.15f; vis.b = 0.1f;
                    vis.animated = true;
                    break;
                case MapTileType::Slippery:
                    vis.r = 0.3f; vis.g = 0.6f; vis.b = 0.9f;
                    break;
                case MapTileType::Slow:
                    vis.r = 0.7f; vis.g = 0.6f; vis.b = 0.1f;
                    break;
                case MapTileType::Checkpoint:
                    vis.r = 0.1f; vis.g = 0.85f; vis.b = 0.3f;
                    vis.animated = true;
                    break;
                case MapTileType::Door:
                    vis.r = 0.6f; vis.g = 0.2f; vis.b = 0.8f;
                    vis.animated = true;
                    break;
                case MapTileType::Pushable:
                    vis.r = 0.55f; vis.g = 0.40f; vis.b = 0.15f;
                    break;
                case MapTileType::Platform:
                    vis.r = 0.5f; vis.g = 0.5f; vis.b = 0.3f;
                    vis.platform = true;
                    break;
                case MapTileType::Floor:
                    vis.r = 0.3f; vis.g = 0.35f; vis.b = 0.25f;
                    break;
                case MapTileType::Solid:
                    vis.r = 0.35f; vis.g = 0.28f; vis.b = 0.22f;
                    break;
                case MapTileType::Base:
                    // La base se pinta como un tile dorado/brillante
                    // (pulsa para llamar la atención del jugador)
                    vis.r = 0.95f; vis.g = 0.75f; vis.b = 0.25f;
                    vis.animated = true;
                    break;
                }

                visuals[td.tile] = vis;
            }
            return visuals;
        }

        // ── Buscar la definición de tile bajo una posición ──────────────
        const MapTileDef* findTileDefAt(f32 px, f32 py) const
        {
            int col = m_tilemap.pixelToCol(px + m_ballSize * 0.5f);
            int row = m_tilemap.pixelToRow(py + m_ballSize * 0.5f);
            char c = m_tilemap.getCharAt(row, col);

            for (const auto& td : m_mapData.tileDefs)
                if (td.tile == c) return &td;
            return nullptr;
        }

        // ── Verificar condición de texto "variable op valor" ────────────
        bool checkConditionString(const std::string& cond) const
        {
            if (cond.empty()) return true; // Sin condición = siempre pasa

            std::istringstream iss(cond);
            std::string var, op, val;
            iss >> var >> op >> val;

            std::string current = getVar(var);

            if (op == "==" || op == "=") return current == val;
            if (op == "!=")             return current != val;

            // Comparaciones numéricas
            int a = 0, b = 0;
            try { a = std::stoi(current); }
            catch (...) {}
            try { b = std::stoi(val); }
            catch (...) {}

            if (op == ">=") return a >= b;
            if (op == "<=") return a <= b;
            if (op == ">")  return a > b;
            if (op == "<")  return a < b;

            return false;
        }

        // Carga de nivel

        void loadFromFile(const std::string& path)
        {
            // Solo elige que archivo se abre. Quien llama conserva la ruta con
            // la que entro: el save y el lua siguen en el mapa original.
            const std::string archivo = eclips::langRutaMapa(path);
            std::cout << "[Gameplay2D] Cargando: " << archivo << std::endl;
            if (archivo.compare(0, 8, "idiomas/") == 0)
                std::cout << "[Gameplay2D] Espejo de: " << path << std::endl;

            m_mapData = MapLoader::load(archivo);
            if (!m_mapData.valid)
            {
                std::cout << "[Gameplay2D] ERROR: " << m_mapData.error << std::endl;
                loadHardcoded();
                return;
            }

            m_speed     = m_mapData.playerSpeed;
            m_ballSize  = m_mapData.playerSize;
            m_musicPath = m_mapData.musicPath;
            m_fontPath  = m_mapData.fontPath;
            m_exitMode  = m_mapData.exitMode;

            m_tilemap.loadGrid(m_mapData.grid, buildTileVisuals());
            m_posX = m_tilemap.getPlayerStartX();
            m_posY = m_tilemap.getPlayerStartY();

            // Configuración del jugador
            if (m_mapData.playerDef.defined)
            {
                m_playerHP         = m_mapData.playerDef.hp;
                m_playerMaxHP      = m_mapData.playerDef.hp;
                m_invincibleDuration = m_mapData.playerDef.invincibleTime;
                m_canAttack        = m_mapData.playerDef.canAttack;
                m_attackRange      = m_mapData.playerDef.attackRange;
                m_attackDamage     = m_mapData.playerDef.attackDamage;
                m_attackCooldown   = m_mapData.playerDef.attackCooldown;

                // Side-scroll config
                m_isSideScroll = (m_mapData.mode == "sidescroll");
                if (m_isSideScroll)
                {
                    m_gravity = m_mapData.gravity;
                    m_jumpForce = m_mapData.jumpForce;
                    m_coyoteTime = m_mapData.coyoteTime;
                    m_gravityVelY = 0;
                    m_isOnGround = false;

                    // Tier 2: doble salto, dash, wall-slide
                    m_maxJumps = m_mapData.doubleJump ? 2 : 1;
                    m_jumpsRemaining = m_maxJumps;

                    m_hasDash = m_mapData.dash;
                    m_dashSpeed = m_mapData.dashSpeed;
                    m_dashDuration = m_mapData.dashDuration;
                    m_dashCooldown = m_mapData.dashCooldown;

                    m_hasWallSlide = m_mapData.wallSlide;
                    m_hasWallJump = m_mapData.wallJump;
                }

                // Tier 2: tipo de ataque
                m_attackType = m_mapData.playerDef.attackType;
                m_playerProjectileTile = m_mapData.playerDef.projectileTile;
                m_playerFireRate = m_mapData.playerDef.fireRate;

                // Sprite de ataque
                if (!m_mapData.playerDef.attackSprite.empty())
                    m_hasAttackSprite = m_texAttack.load(m_mapData.playerDef.attackSprite);
            }

            // Construir entidades
            buildEntities();
            buildSpatialMarkers();

            // ── Base de Tower Defense (Tier 4C) ──
            m_baseHP = 0; m_baseMaxHP = 0;
            m_baseDestroyed = false;
            m_baseHitFlash = 0;
            for (const auto& td : m_mapData.tileDefs)
            {
                if (td.type == MapTileType::Base && td.hp > 0)
                {
                    m_baseMaxHP = td.hp;
                    m_baseHP = td.hp;
                    std::cout << "[Gameplay2D] Base detectada (tile '"
                        << td.tile << "', HP=" << td.hp << ")" << std::endl;
                    break; // Una base por nivel
                }
            }
        }

        void loadHardcoded()
        {
            m_speed = 400; m_ballSize = 48;
            m_musicPath.clear();
            std::cerr << "[Gameplay2D] loadHardcoded: sin musica por defecto del lab\n";
            m_exitMode = MapExitMode::None;
            m_mapData = MapData(); // Reset
            m_spatialMarkers.clear();

            m_tilemap.load({
                "################################",
                "#..............................#",
                "#..L...........==..............#",
                "#..............==..............#",
                "#......####..........B.........#",
                "#......#..#....................#",
                "#......#..#.......S............#",
                "#......####....................#",
                "#..............................#",
                "#..P...........................#",
                "#...............####...........#",
                "#...............#..#...........#",
                "#..BB...........#..#.....===...#",
                "#..BB...........####.....===...#",
                "#..............................#",
                "#.....B....................E...#",
                "#..............................#",
                "################################"
            });
            m_posX = m_tilemap.getPlayerStartX();
            m_posY = m_tilemap.getPlayerStartY();
            buildNPCsHardcoded();
            buildWallColliders();
        }


        // ══════════════════════════════════════════════════════════════════
        // Construcción de entidades
        // ══════════════════════════════════════════════════════════════════

        void buildEntities()
        {
            m_npcs.clear(); m_enemies.clear();
            m_items.clear(); m_triggers.clear();

            // Coleccionar qué letras son de cada tipo
            auto isEnemyChar = [&](char c) {
                for (const auto& d : m_mapData.enemies) if (d.tile == c) return true;
                return false;
            };
            auto isItemChar = [&](char c) {
                for (const auto& d : m_mapData.items) if (d.tile == c) return true;
                return false;
            };
            auto isTriggerChar = [&](char c) {
                for (const auto& d : m_mapData.triggers) if (d.tile == c) return true;
                return false;
            };

            for (const auto& pos : m_tilemap.getNPCPositions())
            {
                if (isEnemyChar(pos.tile))
                {
                    for (const auto& def : m_mapData.enemies)
                    {
                        if (def.tile == pos.tile)
                        {
                            EnemyRuntime e;
                            initEnemyFromDef(e, def, pos.x, pos.y);
                            m_enemies.push_back(e);
                            break;
                        }
                    }
                }
                else if (isItemChar(pos.tile))
                {
                    for (const auto& def : m_mapData.items)
                    {
                        if (def.tile == pos.tile)
                        {
                            ItemRuntime it;
                            initItemFromDef(it, def, pos.x, pos.y);
                            m_items.push_back(it);
                            break;
                        }
                    }
                }
                else if (isTriggerChar(pos.tile))
                {
                    for (const auto& def : m_mapData.triggers)
                    {
                        if (def.tile == pos.tile)
                        {
                            TriggerRuntime tr;
                            tr.tile = def.tile;
                            tr.box = { pos.x, pos.y, def.width, def.height };
                            tr.once = def.once;
                            tr.condVar = def.condVar;
                            tr.condOp  = def.condOp;
                            tr.condVal = def.condVal;
                            tr.onEnter = def.onEnter;
                            m_triggers.push_back(tr);
                            break;
                        }
                    }
                }
                else
                {
                    // Default: tratar como NPC
                    buildSingleNPC(pos);
                }
            }

            buildWallColliders();

            // Items ya recogidos: si su variable ya tiene el valor, no reaparecen
            for (auto& item : m_items)
            {
                for (const auto& act : item.onPickup)
                {
                    if (act.type == MapActionType::SetVar && act.varSet.op == "=")
                    {
                        if (getVar(act.varSet.var) == act.varSet.val)
                        {
                            item.collected = true;
                            break;
                        }
                    }
                }
            }

        }

        void buildSpatialMarkers()
        {
            m_spatialMarkers.clear();
            m_spatialMarkers.reserve(m_mapData.spatialMarkers.size());

            for (const auto& def : m_mapData.spatialMarkers)
            {
                SpatialMarkerRuntime marker;
                marker.id = def.id;
                marker.box = { def.x, def.y, def.width, def.height };
                m_spatialMarkers.push_back(std::move(marker));
            }
        }

        void initEnemyFromDef(EnemyRuntime& e, const MapEnemyDef& def,
                              float x, float y)
        {
            e.box = { x, y, Tilemap::TILE_SIZE, Tilemap::TILE_SIZE };
            e.tile = def.tile;
            e.name = def.name;
            e.id = m_nextEnemyId++;
            e.r = def.r; e.g = def.g; e.b = def.b;
            e.hp = def.hp; e.maxHP = def.hp;
            e.damage = def.damage;
            e.speed = def.speed;
            e.movement = def.movement;
            e.knockback = def.knockback;
            e.invulnerable = def.invulnerable;
            e.onDeath = def.onDeath;
            e.originX = x; e.originY = y;
            e.range = def.range;
            e.abilityUses.reserve(def.abilityUses.size());
            for (const auto& authoredUse : def.abilityUses)
                e.abilityUses.push_back(makeAbilityRuntime(authoredUse));
            e.attacksTowers = def.attacksTowers;
            e.towerAttackRange = def.towerAttackRange;
            e.towerAttackRate = def.towerAttackRate;

            // Calcular rango de patrulla en pixels
            if (e.movement.type == MapMovementType::PatrolH ||
                e.movement.type == MapMovementType::PatrolV)
                e.patrolRange = e.movement.param * Tilemap::TILE_SIZE;

            // Resolver ruta a posiciones en píxeles
            if (e.movement.type == MapMovementType::Path &&
                !e.movement.pathName.empty())
            {
                for (const auto& route : m_mapData.routes)
                {
                    if (route.name == e.movement.pathName)
                    {
                        e.pathTraversal = route.traversal;
                        for (const auto& pt : route.points)
                            e.pathPoints.push_back({
                                pt.first * Tilemap::TILE_SIZE,
                                pt.second * Tilemap::TILE_SIZE
                                });
                        break;
                    }
                }
                if (e.pathPoints.empty())
                    std::cout << "[Gameplay2D] Ruta '" << e.movement.pathName
                    << "' no encontrada para " << e.name << std::endl;
            }
        }

        void initItemFromDef(ItemRuntime& it, const MapItemDef& def,
                             float x, float y)
        {
            it.box = { x, y, Tilemap::TILE_SIZE, Tilemap::TILE_SIZE };
            it.tile = def.tile;
            it.name = def.name;
            it.r = def.r; it.g = def.g; it.b = def.b;
            it.onPickup = def.onPickup;
        }

        void buildSingleNPC(const TileNPCPos& pos)
        {
            NPCRuntime npc;
            f32 margin = 40.0f;
            npc.box = { pos.x, pos.y, Tilemap::TILE_SIZE, Tilemap::TILE_SIZE };
            npc.interactZone = { pos.x-margin, pos.y-margin,
                Tilemap::TILE_SIZE+margin*2, Tilemap::TILE_SIZE+margin*2 };
            npc.tile = pos.tile;

            bool found = false;
            for (const auto& def : m_mapData.npcs)
            {
                if (def.tile == pos.tile)
                {
                    npc.name = def.name;
                    npc.r = def.r; npc.g = def.g; npc.b = def.b;
                    npc.script.speaker = def.dialog.speaker;
                    npc.script.lines   = def.dialog.lines;
                    for (const auto& mc : def.dialog.choices)
                    {
                        DialogChoice dc;
                        dc.label  = mc.label;
                        dc.result = mc.result;
                        npc.script.choices.push_back(dc);
                        npc.choiceSets.push_back(mc.sets);
                    }
                    npc.onTalkSets = def.sets;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                npc.name = std::string(1, pos.tile);
                npc.r = npc.g = npc.b = 0.5f;
                npc.script.speaker = npc.name;
                npc.script.lines = { "..." };
            }

            m_npcs.push_back(npc);
        }

        // ── Construir colliders de pared ──────────────────────────────────
        // Copia los colliders del tilemap EXCEPTO posiciones de
        // enemigos, items y triggers (esos se manejan dinámicamente).

        void buildWallColliders()
        {
            m_wallColliders = m_tilemap.getColliders();
            m_platformColliders = m_tilemap.getPlatformColliders();

            // Remover colliders de enemigos (se mueven)
            for (const auto& e : m_enemies)
                removeColliderAt(e.box.x, e.box.y);

            // Remover colliders de items (caminables)
            for (const auto& it : m_items)
                removeColliderAt(it.box.x, it.box.y);

            // Remover colliders de triggers (invisibles, caminables)
            for (const auto& tr : m_triggers)
                removeColliderAt(tr.box.x, tr.box.y);

            std::cout << "[Gameplay2D] Wall colliders: " << m_wallColliders.size()
                      << " (de " << m_tilemap.getColliders().size()
                      << " originales)" << std::endl;
        }

        void removeColliderAt(float x, float y)
        {
            m_wallColliders.erase(
                std::remove_if(m_wallColliders.begin(), m_wallColliders.end(),
                    [x, y](const TileAABB& c) {
                        return std::abs(c.x - x) < 1.0f &&
                               std::abs(c.y - y) < 1.0f;
                    }),
                m_wallColliders.end());
        }

        void buildNPCsHardcoded()
        {
            m_npcs.clear();
            f32 margin = 40.0f;
            for (const auto& spawn : m_tilemap.getNPCSpawns())
            {
                NPCRuntime npc;
                npc.box = { spawn.x, spawn.y,
                            Tilemap::TILE_SIZE, Tilemap::TILE_SIZE };
                npc.interactZone = { spawn.x-margin, spawn.y-margin,
                    Tilemap::TILE_SIZE+margin*2, Tilemap::TILE_SIZE+margin*2 };
                npc.name = spawn.name;
                npc.r = spawn.r; npc.g = spawn.g; npc.b = spawn.b;
                npc.script.speaker = spawn.name;
                npc.script.lines   = spawn.lines;
                m_npcs.push_back(npc);
            }
        }


        // ══════════════════════════════════════════════════════════════════
        // Miembros
        // ══════════════════════════════════════════════════════════════════

        SDL_Window*   m_window     = nullptr;
        SDL_GLContext m_glContext  = nullptr;
        StateStack*   m_stateStack = nullptr;
        FontSourceStore* m_fontSources = nullptr;
        // CP2 — la Capa 1. Dueño: `Engine` (§18 `A-6`, fork 1a de `CP1`).
        const eclips::InputManager* m_input = nullptr;
        std::function<void()> m_onFinish;
        int m_windowW = 0, m_windowH = 0;

        Texture  m_texIdle, m_texUp, m_texDown, m_texLeft, m_texRight;
        std::unique_ptr<SpriteSheet> m_animIdle;
        std::unique_ptr<SpriteSheet> m_animUp;
        std::unique_ptr<SpriteSheet> m_animDown;
        std::unique_ptr<SpriteSheet> m_animLeft;
        std::unique_ptr<SpriteSheet> m_animRight;
        Gameplay2DFrameState m_playerFrameState =
            Gameplay2DFrameState::Idle;
        Texture* m_currentTex = nullptr;
        bool     m_hasSprites = false;
        TextRenderer m_hudText;
        bool     m_hasFont = false;

        // Las tres fuentes del diálogo NPC (E-48): una vez por vida del
        // estado, prestadas a cada DialogOverlayState.
        TextRenderer m_dialogText;     // 36 px
        TextRenderer m_dialogName;     // 28 px
        TextRenderer m_dialogChoice;   // 32 px
        DialogFonts  m_dialogFonts;    // vacío hasta el primer horneado
        AudioManager m_audio;
        bool     m_hasAudio = false;

        std::string m_musicPath;
        std::string m_fontPath;
        MapExitMode m_exitMode = MapExitMode::None;
        MapData     m_mapData;

        Tilemap                 m_tilemap;
        std::vector<TileAABB>   m_wallColliders;
        std::vector<NPCRuntime> m_npcs;
        std::vector<EnemyRuntime> m_enemies;
        std::vector<TowerRuntime> m_towers;      // Tier 4C: Tower Defense
        std::vector<ItemRuntime>  m_items;
        std::vector<TriggerRuntime> m_triggers;
        std::vector<SpatialMarkerRuntime> m_spatialMarkers;
        std::vector<Notification>   m_notifications;
        int                     m_nearNPC = -1;
        int                     m_talkingToNPC = -1;

        f32 m_posX = 0, m_posY = 0;
        f32 m_speed = 400, m_ballSize = 48;
        Dir m_lastDir = Dir::Idle;

        f32 m_camX = 0, m_camY = 0;
        f32 m_cameraShakeTimer = 0;

        // Jugador — HP y combate
        int   m_playerHP       = 0;
        int   m_playerMaxHP    = 0;
        float m_invincibleTimer    = 0;
        float m_invincibleDuration = 1.5f;
        bool  m_canAttack          = false;
        float m_attackRange        = 60.0f;
        int   m_attackDamage       = 1;
        float m_attackCooldown     = 0.3f;
        float m_attackTimer        = 0;
        float m_attackVisualTimer  = 0;
        float m_attackVisualDuration = 0.12f;

        // Timer de salida
        float m_exitTimerCurrent = 0;

        f64 m_totalTime = 0;
        f32 m_fadeInAlpha  = 1.0f;
        f32 m_fadeOutAlpha = 0.0f;
        bool m_exiting      = false;
        bool m_returnToMenu = false;
        bool m_returnToVN   = false;
        bool m_retryRequested = false;
        std::string m_pendingMapLoad;  // Tier 4D: siguiente nivel a cargar (vacío = ninguno)

        // Tile effects
        float m_tileDamageTimer = 0;
        float m_velX = 0, m_velY = 0;           // Velocidad (para slippery)

        // Knockback con velocidad (Tier 2)
        float m_knockVelX = 0, m_knockVelY = 0;
        float m_knockDecay = 10.0f;

        // Side-scroll
        bool  m_isSideScroll = false;
        float m_gravityVelY = 0;
        float m_gravity = 1800.0f;
        float m_jumpForce = 700.0f;
        float m_maxFallSpeed = 1200.0f;
        float m_coyoteTime = 0.12f;
        float m_coyoteTimer = 0;
        bool  m_isOnGround = false;
        bool  m_facingRight = true;

        // Doble salto (Tier 2)
        int   m_jumpsRemaining = 1;
        int   m_maxJumps = 1;              // 1 = normal, 2 = doble salto

        // Dash (Tier 2)
        bool  m_hasDash = false;
        bool  m_isDashing = false;
        float m_dashSpeed = 1200.0f;
        float m_dashDuration = 0.15f;
        float m_dashTimer = 0;
        float m_dashCooldown = 0.4f;
        float m_dashCooldownTimer = 0;
        int   m_dashDirX = 0;              // Dirección del dash

        // Wall-slide / Wall-jump (Tier 2)
        bool  m_hasWallSlide = false;
        bool  m_hasWallJump = true;
        bool  m_isWallSliding = false;
        int   m_wallSlideSide = 0;         // -1 = izquierda, 1 = derecha
        float m_wallSlideMaxSpeed = 80.0f;

        float m_checkpointX = 0, m_checkpointY = 0;
        bool  m_hasCheckpoint = false;

        // Transición entre mapas
        bool        m_transitioning = false;
        std::string m_pendingMapPath;
        float       m_pendingSpawnX = 0;
        float       m_pendingSpawnY = 0;
        bool        m_doorMessageShown = false;

        // Proyectiles (Tier 2)
        std::vector<ProjectileRuntime> m_projectiles;
        std::vector<TileAABB>          m_platformColliders;

        // Ataque ranged / sprite (Tier 2)
        std::string m_attackType = "melee";    // "melee" | "ranged" | "both"
        std::string m_playerProjectileTile;    // Letra del template
        float       m_playerFireRate = 0.3f;
        Texture     m_texAttack;               // Sprite del efecto de ataque
        bool        m_hasAttackSprite = false;

        // Cámara avanzada (Tier 2)
        bool  m_cameraFollowPlayer = true;
        bool  m_cameraPanning = false;
        float m_cameraPanStartX = 0, m_cameraPanStartY = 0;
        float m_cameraPanTargetX = 0, m_cameraPanTargetY = 0;
        float m_cameraPanDuration = 0, m_cameraPanTimer = 0;

        float m_cameraZoom = 1.0f;
        float m_cameraZoomStart = 1.0f;
        float m_cameraZoomTarget = 1.0f;
        float m_cameraZoomDuration = 0, m_cameraZoomTimer = 0;

        // Wait en acciones (Tier 2)
        bool  m_waitingAction = false;
        float m_waitTimer = 0;

        // Oleadas (Tier 3)
        int   m_currentWave = 0;
        float m_waveTimer = 0;
        bool  m_waveActive = false;
        bool  m_waveSpawned = false;
        bool  m_wavesComplete = false;
        std::string m_waveAdvance = "eliminar";

        // ── Tower Defense: Base con HP (Tier 4C) ──
        int   m_baseHP = 0;      // HP actual
        int   m_baseMaxHP = 0;      // HP máximo (también: flag de si hay base)
        bool  m_baseDestroyed = false; // Se disparó onDestroy?
        float m_baseHitFlash = 0;      // Flash al recibir daño

        // ── Lua (Tier 4) ──
        LuaManager   m_lua;
        int          m_nextEnemyId = 1;
        int          m_nextTowerId = 1;          // Tier 4C: Tower Defense
        int          m_nextActivityInhibitionHandle = 1;
        int          m_nextActivityCostHandle = 1;
        int          m_nextActivityIntervalFactorHandle = 1;
        int          m_nextActivityRangeFactorHandle = 1;
        int          m_nextActivityDirectDamageDeltaHandle = 1;
        int          m_nextActivityPreferenceOverrideHandle = 1;
        bool         m_luaExitFlag = false;
        bool         m_insideHook = false;
        bool         m_hasUpdateHook = false;
        std::map<std::string, std::vector<sol::function>> m_hooks;

        // CP8: nombres ya avisados a la Lua del creador, para que el aviso
        // salga UNA vez por nombre. Ver `avisarNombreLuaDesconocido`.
        std::set<std::string> m_avisadosLua;

        // ── Tier 4C.4: Modo construcción ──────────────────────────────
        std::string  m_buildModeTowerName;    // vacío = no activo
        int          m_buildCursorCol = -1;
        int          m_buildCursorRow = -1;
        bool         m_buildCursorInMap = false;
        float        m_buildInvalidFlash = 0.0f; // Parpadeo rojo al click fallido

    };

} // namespace eclips
