// ============================================================================
// MapData.h — Formato de nivel .eclmap y parser (v2 — Entidades)
// ============================================================================
//
// Un archivo .eclmap define un nivel 2D completo de forma legible:
//   - Modo de juego (topdown / sidescroll)
//   - Configuración del jugador (sprite, HP, ataque)
//   - Tiles custom con propiedades (daño, puerta, plataforma)
//   - Mapa ASCII de tamaño libre
//   - Entidades: NPCs, enemigos, items, triggers
//   - Condiciones de salida extendidas
//   - Configuración de HUD
//
// ── Ejemplo mínimo ─────────────────────────────────────────────────────────
//
//   modo: topdown
//   musica: "assets/audio/bosque.ogg"
//   velocidad: 400
//   salida: hablar_todos
//
//   mapa:
//   ##########
//   #..P.....#
//   #....A...#
//   #........>
//   ##########
//
//   npc A "Luna" #9966BB:
//     "Hola! Soy Luna."
//
// ── Ejemplo con enemigos + items ───────────────────────────────────────────
//
//   modo: topdown
//   salida: matar_todos
//
//   jugador:
//     hp: 5
//     puede_atacar: true
//
//   hud:
//     hp: visible
//
//   mapa:
//   ##########
//   #..P.....#
//   #....E...#
//   #......I.#
//   ##########
//
//   enemigo E "Slime" #00FF00:
//     hp: 3
//     daño: 1
//     movimiento: patrulla_h 3
//     al_morir:
//       set: score + 100
//       sonido: "assets/boom.ogg"
//
//   item I "Poción" #FF6688:
//     al_recoger:
//       set: hp + 1
//       notificar: "Recuperaste 1 HP!"
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>
#include "Gameplay2DAbility.h"
#include "Gameplay2DHud.h"
#include "engine/core/EclipsParse.h"   // parseBool centralizado (§1.45)

namespace eclips
{

// ════════════════════════════════════════════════════════════════════════════
// ENUMS
// ════════════════════════════════════════════════════════════════════════════

enum class MapExitMode
{
    None,           // Sin salida automática (solo ESC/pausa)
    TalkAll,        // Hablar con todos los NPCs
    ReachExit,      // Pisar el tile '>'
    KillAll,        // Derrotar a todos los enemigos
    CollectAll,     // Recoger todos los items
    Timer,          // Sobrevivir N segundos
    Variable,       // Cuando una variable tenga cierto valor
    Waves,          // Completar todas las oleadas (Tier 3)
    BaseDestroyed,  // Derrota: la base cae a 0 HP (Tier 4C)
    Lua             // Lua decide (Tier 4)
};

enum class MapMovementType
{
    Still,         // No se mueve
    PatrolH,       // Patrulla horizontal (param = tiles)
    PatrolV,       // Patrulla vertical (param = tiles)
    Follow,        // Sigue al jugador (param = rango de detección)
    Flee,          // Huye del jugador (param = rango de detección)
    Bounce,        // Rebota en paredes
    Random,        // Movimiento aleatorio (param = intervalo en segundos)
    Circle,        // Órbita su posición (param = radio) — Tier 2
    Path           // Sigue waypoints — Tier 3
};

enum class MapActionType
{
    SetVar,        // set: var op val
    Sound,         // sonido/sound: "path"
    Music,         // musica/music: "path"
    Notify,        // notificar/notify: "text"
    Dialog,        // dialogo: "text"
    Spawn,         // spawnear/spawn: LETTER COUNT [position]
    Vanish,        // desaparecer/vanish: true
    Drop,          // soltar/drop: LETTER
    LoadMap,       // cargar_mapa/load_map: "path" X, Y
    CameraShake,   // camara/camera: shake TIME
    CameraPan,     // camara/camera: pan X, Y TIME — Tier 2
    CameraFollow,  // camara/camera: seguir/follow — Tier 2
    CameraZoom,    // camara/camera: zoom Z TIME — Tier 2
    Wait,          // esperar/wait: TIME — Tier 2
    RunVN,         // vn: "script.ecl" [scene] — Tier 2
    RunLua,        // lua: "script.lua" — Tier 4
    DestroyTile,   // destruir_tile: TX, TY — Tier 3
    CreateTile     // crear_tile: TX, TY CHAR — Tier 3
};

enum class MapTileType
{
    Solid,         // Pared normal (no caminable)
    Floor,         // Suelo (caminable, sin efecto)
    Damage,        // Daño por contacto
    Slippery,      // Derrape / inercia
    Slow,          // Reduce velocidad
    Platform,      // Sólida desde arriba (side-scroll)
    Checkpoint,    // Punto de guardado
    Door,          // Transición a otro nivel
    Pushable,      // Bloque empujable por el jugador
    Base,          // Tower Defense: objetivo con HP que enemigos atacan
    Spawn,         // Tower Defense: punto de aparición de oleadas
    TowerSlot      // Tower Defense: tile válido para colocar torres
};

// ── LevelEnd (Tier 4D) ────────────────────────────────────────────────────

enum class LevelEndStyle
{
    Classic,   // clasico/basic (default): título + mensaje + botones
    Stars,     // estrellas/stars: 1-3 estrellas según criterios
    Tally,     // tally/puntaje: desglose de stats con contadores animados
    KO         // ko: pantalla dramática (shake + tint + fade)
};

enum class LevelEndAnimation
{
    Fade,        // desvanecer/fade (default)
    FromTop,     // desde_arriba/from_top: cae desde fuera de pantalla arriba
    FromBottom,  // desde_abajo/from_bottom: sube desde fuera de pantalla abajo
    Zoom,        // zoom: crece desde el centro con escala
    None         // ninguna/none: aparece instantáneo
};

enum class LevelEndPosition
{
    Center,     // centro/center (default)
    Top,        // arriba/top
    Bottom      // abajo/bottom
};

// ════════════════════════════════════════════════════════════════════════════
// DATA STRUCTS
// ════════════════════════════════════════════════════════════════════════════

// ── Cambio de variable ─────────────────────────────────────────────────────

struct MapVarSet
{
    std::string var;   // Nombre de la variable
    std::string op;    // "=" | "+" | "-"
    std::string val;   // Valor
};

// ── Acción genérica ────────────────────────────────────────────────────────
// Usada en bloques de evento (al_morir, al_recoger, al_entrar, etc.)

struct MapAction
{
    MapActionType type;

    // Campos usados según el tipo:
    MapVarSet   varSet;         // SetVar
    std::string str1;           // Sound/Music/Notify/Dialog/LoadMap/RunVN/RunLua path/text
    std::string str2;           // LoadMap: posición, RunVN: escena, Spawn: modo ("aleatorio"/"en")
    char        entityChar = 0; // Spawn/Drop: letra de la entidad
    int         count      = 0; // Spawn: cantidad
    float       posX       = 0; // Spawn en posición / CameraPan / DestroyTile X
    float       posY       = 0; // Spawn en posición / CameraPan / DestroyTile Y
    float       duration   = 0; // CameraShake/Pan/Zoom/Wait: tiempo en segundos
    float       value      = 0; // CameraZoom: nivel de zoom
    bool        flag       = false; // Vanish: true/false
};

// ── Diálogos (existente, sin cambios) ──────────────────────────────────────

struct MapDialogChoice
{
    std::string              label;
    std::vector<std::string> result;
    std::vector<MapVarSet>   sets;
};

struct MapDialogScript
{
    std::string                   speaker;
    std::vector<std::string>      lines;
    std::vector<MapDialogChoice>  choices;
};

// ── Movimiento de entidad ──────────────────────────────────────────────────

struct MapMovement
{
    MapMovementType type  = MapMovementType::Still;
    float           param = 0.0f;    // Significado depende del tipo
    std::string     pathName;        // Para Path: nombre de la ruta (Tier 3)
};

// ── NPC (existente, sin cambios) ───────────────────────────────────────────

struct MapNPCDef
{
    char            tile = '?';
    std::string     name;
    float           r = 0.7f, g = 0.7f, b = 0.7f;
    std::string     spritePath;
    MapDialogScript dialog;
    std::vector<MapVarSet> sets;
};

// ── Enemigo ────────────────────────────────────────────────────────────────

struct MapEnemyDef
{
    char            tile = '?';
    std::string     name;
    float           r = 0.7f, g = 0.7f, b = 0.7f;
    std::string     spritePath;

    int             hp          = 1;
    int             damage      = 1;       // Daño de contacto al jugador
    float           speed       = 100.0f;
    MapMovement     movement;
    float           range       = 0.0f;    // Rango de detección (follow/flee)
    float           knockback   = 200.0f;  // Fuerza de empuje
    bool            invulnerable = false;

    std::vector<MapAction> onDeath;

    // Autoría de habilidad. Igual que en torre: durante parse contiene usos
    // explícitos y al terminar MapLoader::load queda materializada.
    // El adapter legacy conserva el disparo histórico hacia el jugador.
    std::vector<Gameplay2DAbilityUseDef> abilityUses;
    Gameplay2DLegacyAbilitySource legacyAbility;
    bool explicitAbilityBlockSeen = false;

    // Tier 4C.3: Kingdom Rush avanzado — enemigo ataca torres
    bool            attacksTowers = false;  // Se detiene junto a torres y las ataca
    float           towerAttackRange = 60.0f;  // Distancia para detenerse (px)
    float           towerAttackRate = 1.0f;   // Segundos entre golpes a la torre
};

// ── Torre (Tower Defense) ──────────────────────────────────────────────────
// Una torre es una entidad estática con auto-targeting. Reusa la maquinaria
// de enemigos internamente, pero con bando aliado y sin colisión con el
// jugador. Las torres NO se colocan en el mapa ASCII — se colocan en runtime
// vía Lua o modo construcción.

struct MapTowerDef
{
    char            tile = '?';
    std::string     name;
    float           r = 0.7f, g = 0.7f, b = 0.7f;
    std::string     spritePath;

    int             cost = 0;       // Oro sugerido (opcional)
    int             hp = 0;       // 0 = indestructible (default)
    std::string     whereSlot;             // "slot_torre" o vacío = cualquier floor

    // Autoría de habilidad. Durante el parseo contiene los usos explícitos;
    // al terminar MapLoader::load queda materializada como la lista efectiva.
    std::vector<Gameplay2DAbilityUseDef> abilityUses;
    Gameplay2DLegacyAbilitySource legacyAbility;
    bool explicitAbilityBlockSeen = false;
    bool obstructsLocomotion = false;

    std::vector<MapAction> onPlace;
    std::vector<MapAction> onDestroy;
};

// ── Item ───────────────────────────────────────────────────────────────────

struct MapItemDef
{
    char            tile = '?';
    std::string     name;
    float           r = 0.7f, g = 0.7f, b = 0.7f;
    std::string     spritePath;

    bool            inventory = false;  // false = auto-pickup, true = va al inventario

    std::vector<MapAction> onPickup;
    std::vector<MapAction> onUse;       // Tier 2+ (items de inventario)
};

// ── Trigger ────────────────────────────────────────────────────────────────

struct MapTriggerDef
{
    char            tile = '?';
    float           width  = 48.0f;
    float           height = 48.0f;
    bool            once      = false;
    std::string     condition;           // "variable == valor" (vacío = sin condición)
    std::string     condVar;             // Parseado de condition: nombre
    std::string     condOp;              // Parseado: "==" | "!=" | ">=" | "<="
    std::string     condVal;             // Parseado: valor

    std::vector<MapAction> onEnter;
};

// ── Marcador espacial neutral ──────────────────────────────────────────────
//
// Es una entidad espacial identificada, sin acciones ni consecuencia propia.
// La geometría se expresa en coordenadas del mundo (píxeles).

struct MapSpatialMarkerDef
{
    std::string id;
    float       x = 0.0f;
    float       y = 0.0f;
    float       width = 0.0f;
    float       height = 0.0f;
};

// ── Proyectil ─────────────────────────────────────────────────────────────────

struct MapProjectileDef
{
    char        tile = '?';
    std::string name;
    float       r = 1.0f, g = 0.5f, b = 0.1f;
    std::string spritePath;

    float       speed       = 300.0f;
    int         damage      = 1;
    float       size        = 16.0f;
    float       lifetime    = 3.0f;
    bool        pierceWalls = false;
    bool        hasGravity  = false;   // Arco parabólico
};

// ── Ruta / Waypoints ─────────────────────────────────────────────────────

enum class MapRouteTraversal
{
    Loop,       // Ultimo punto -> primero
    PingPong,   // Ultimo punto -> recorrido inverso
    Once        // Ultimo punto -> recorrido completo; el actor permanece
};

struct MapRoute
{
    std::string name;
    std::vector<std::pair<int, int>> points; // (col, row) en tiles
    MapRouteTraversal traversal = MapRouteTraversal::Loop;
};

// ── Oleadas / Waves ──────────────────────────────────────────────────────

struct MapWaveSpawn
{
    char        entityChar = '?';
    int         count = 1;
    std::string mode;           // "aleatorio"/"random" | "en"/"at"
    float       posX = 0, posY = 0;
};

struct MapWaveDef
{
    int                       number = 0;
    float                     delay = 0;     // Espera antes de spawnear
    std::vector<MapWaveSpawn> spawns;
    std::vector<MapAction>    onComplete;
};

struct MapWaveConfig
{
    std::string advance = "eliminar"; // "eliminar"/"eliminate" | "tiempo"/"time"
    float       interval = 30.0f;
    bool        defined = false;
    std::vector<MapWaveDef> waves;
};

// ── Tile custom ────────────────────────────────────────────────────────────

struct MapTileDef
{
    char            tile = '?';
    std::string     spritePath;
    MapTileType     type = MapTileType::Solid;
    int             damage = 0;       // Para Damage
    float           speedMult = 1.0f;    // Para Slow (0.0-1.0)
    float           friction = 0.3f;    // Para Slippery

    // Para Door
    std::string     destMap;
    int             destX = 0, destY = 0;
    std::string     condition;
    std::string     lockedMsg;

    // Para Checkpoint
    std::string     sound;

    // Para Base (Tower Defense)
    int             hp = 0;                      // HP máximo; 0 = sin HP (no crítica)
    std::vector<MapAction> onDestroy;            // Acciones al caer a 0 HP

    bool            walkable = false;    // true para Floor, Damage, Slippery, Slow
};

// ── Configuración del jugador ──────────────────────────────────────────────

struct MapPlayerDef
{
    std::string     spritePath;
    float           animationFps         = 8.0f;
    bool            animationFpsExplicit = false;
    int             hp              = 0;     // 0 = sin sistema de HP (inmortal)
    bool            contactDamage   = true;  // ¿Recibe daño al tocar enemigos?
    float           invincibleTime  = 1.5f;  // Segundos de invencibilidad post-daño
    bool            canAttack       = false;
    float           attackRange     = 60.0f;
    int             attackDamage    = 1;
    float           attackCooldown  = 0.3f;
    std::string     attackType      = "melee";   // "melee" | "ranged" | "both"
    std::string     attackSprite;                 // Sprite del efecto de ataque
    std::string     projectileTile;               // Letra del proyectil (ranged)
    float           fireRate        = 0.3f;       // Cadencia de disparo (ranged)

    bool            defined = false;         // ¿Se definió bloque jugador: ?
};

// ── Configuración de HUD ───────────────────────────────────────────────────

struct MapHUDDef
{
    bool            showHP = false;
    bool            showBaseHP = false;   // Tier 4C: HP de la base (TD)
    bool            showInventory = false;
    bool            showWave = false;
    std::string     scoreVar;              // Variable a mostrar como score
    std::string     scoreLabel;            // Label del score
    std::string     counterVar;            // Variable para contador
    std::string     counterFormat;         // Formato: "Monedas: {}"
    std::string     fixedText;             // Texto estático
    bool            defined = false;
};

// ══════════════════════════════════════════════════════════════════════════
// SavePolicy (Tier 4C.6) — Política de save del nivel 2D
// ══════════════════════════════════════════════════════════════════════════
//
// Define si el jugador puede guardar la partida mientras juega este nivel,
// y en qué condiciones. Se parsea desde el bloque `guardar:` en el .eclmap.
//
// Default (si el bloque se omite): SaveAllow::Never — el menú de pausa
// oculta Guardar/Cargar. Para habilitarlo el creador opta explícitamente:
//
//   guardar:
//     permitir: siempre                # Muestra Guardar/Cargar en el menú
//     slots: 5                         # Número de slots (default 5)
//     archivo: "saves/bosque.eclips"   # Opcional, default saves/vn_saves.eclips
//
//   guardar:
//     permitir: solo_checkpoint        # Solo guarda al tocar tile tipo: checkpoint
//
// El campo `archivo:` permite que el nivel use su propio archivo de saves,
// aislado del compartido VN↔Gameplay. Útil para juegos estilo Cuphead/PvZ
// donde cada mundo es autónomo. Limitación actual: sin entrada propia en
// MainMenu, el jugador vuelve al nivel relanzándolo desde VN (gameplay ...).
// ══════════════════════════════════════════════════════════════════════════

enum class SaveAllow
{
    Never,              // Menú oculta save/load. Sin save.
    CheckpointOnly,     // Solo al tocar tile tipo: checkpoint (auto)
    Always              // Menú de pausa muestra Guardar y Cargar
};

struct SavePolicy
{
    SaveAllow   allow = SaveAllow::Never;
    int         slotsVisible = 5;    // Consistencia con VN
    std::string saveFile;            // Opcional. Vacío = usa archivo compartido VN.
};

// ══════════════════════════════════════════════════════════════════════════
// LevelEndConfig (Tier 4D) — Pantalla de victoria/derrota del nivel
// ══════════════════════════════════════════════════════════════════════════
//
// Define la pantalla mostrada cuando el nivel termina con resultado. Se
// parsea desde el bloque `fin:` en el .eclmap. Dos sub-presets independientes:
// `victoria:` y `derrota:`. Cada preset elige su propio estilo, texto,
// colores, animación y sonido.
//
// Default (si el bloque se omite): preset Classic automático con texto
// genérico ("Victoria" / "Nivel completado"). Niveles viejos que disparen
// triggers de fin (base_destroyed, oleadas completas, muerte del player)
// muestran el preset default sin editar el .eclmap.
//
// Customización de Nivel 2: colores, animación de entrada, posición,
// sonido. Nivel 3 (timelines custom tipo Kingdom Rush) queda reservado
// para una sesión de diseño futura.
//
// ── Ejemplo ──
//   fin:
//     victoria:
//       estilo: estrellas
//       texto: "¡Defensa exitosa!"
//       criterios:
//         tres_estrellas: hp_base >= 18
//         dos_estrellas: hp_base >= 10
//         una_estrella: siempre
//       animacion_entrada: desde_arriba
//       sonido_entrada: "assets/fanfarria.ogg"
//       colores:
//         fondo: #001133
//         acento: #FFD700
//       siguiente: "nivel2.eclmap"
//     derrota:
//       estilo: ko
//       texto: "La base cayó..."
// ══════════════════════════════════════════════════════════════════════════

struct LevelEndStarCriterion
{
    int         stars = 1;       // 3, 2, o 1
    std::string var;              // "hp_base", "oro", etc.
    std::string op;               // ">=", ">", "<=", "<", "==", "!="
    std::string val;              // "18" como string (eval en runtime)
    bool        always = false;   // true si el criterio es "siempre"/"always"
};

struct LevelEndColors
{
    // Defaults: tema oscuro con acento dorado
    float bgR = 0.05f, bgG = 0.05f, bgB = 0.08f, bgA = 0.90f;
    float textR = 1.00f, textG = 1.00f, textB = 1.00f, textA = 1.00f;
    float accentR = 1.00f, accentG = 0.84f, accentB = 0.00f, accentA = 1.00f;
    float btnR = 0.15f, btnG = 0.18f, btnB = 0.25f, btnA = 0.90f;
    float btnHovR = 0.30f, btnHovG = 0.40f, btnHovB = 0.60f, btnHovA = 1.00f;
};

struct LevelEndPreset
{
    bool                               defined = false;
    LevelEndStyle                      style = LevelEndStyle::Classic;
    std::string                        text;            // Título/mensaje principal
    LevelEndPosition                   position = LevelEndPosition::Center;
    LevelEndAnimation                  animation = LevelEndAnimation::Fade;
    float                              entryDuration = 0.4f; // Segundos. Clamp 0.05-3.0 al parsear.
    std::string                        entrySound;      // Ruta a .ogg/.wav (opcional)
    std::string                        nextMap;         // Siguiente .eclmap (opcional)
    LevelEndColors                     colors;
    std::vector<LevelEndStarCriterion> starCriteria;    // Solo usado si style==Stars
    std::vector<std::pair<std::string, std::string>> tallyFields;   // Para estilo Tally: campos a mostrar en orden (label → varName)

    // ── Overrides de Lua (Tier 4D Fase 4) ──
    // Mutados por el hook "victory"/"defeat" si retorna una tabla.
    // No se parsean del .eclmap — solo viven en memoria durante la
    // construcción del LevelEndState. Por eso no aparecen en
    // parseEndPresetBlock.
    int                                                starsOverride = -1;  // Si >= 0, sobrescribe el cálculo de criterios.
    std::vector<std::pair<std::string, std::string>>   tallyOverride;       // Si no vacío, sobrescribe tallyFields. Pares <label, valor_literal_ya_resuelto>.
};

struct LevelEndConfig
{
    bool            defined = false;
    LevelEndPreset  victory;
    LevelEndPreset  defeat;
};

// ── Datos completos del nivel ──────────────────────────────────────────────

struct MapData
{
    // ── Config base (existente) ──
    std::string  mode        = "topdown";
    std::string  musicPath;
    std::string  fontPath;
    std::string  luaScript;           // Tier 4: Lua global del nivel
    float        playerSpeed = 400.0f;
    float        playerSize  = 48.0f;
    MapExitMode  exitMode    = MapExitMode::None;
    float        exitTimer   = 0.0f;           // Para Timer
    std::string  exitVar, exitOp, exitVal;     // Para Variable

    // ── Side-scroll config ──
    float        gravity      = 1800.0f;
    float        jumpForce    = 700.0f;
    float        coyoteTime   = 0.12f;
    bool         doubleJump   = false;
    bool         dash         = false;
    float        dashSpeed    = 1200.0f;
    float        dashDuration = 0.15f;
    bool         wallSlide    = false;
    bool         wallJump     = true;     // Default: wall_slide implica wall_jump
    float        dashCooldown = 0.4f;

    // ── Mapa ──
    std::vector<std::string> grid;
    int gridWidth  = 0;
    int gridHeight = 0;

    // ── Entidades ──
    std::vector<MapNPCDef>     npcs;
    std::vector<MapEnemyDef>   enemies;
    std::vector<MapItemDef>    items;
    std::vector<MapTriggerDef> triggers;
    std::vector<MapSpatialMarkerDef> spatialMarkers;
    std::vector<MapTowerDef>   towers;    // Tier 4C: Tower Defense
    std::vector<Gameplay2DAbilityDef> abilities;

    // ── Tiles custom ──
    std::vector<MapTileDef>    tileDefs;
    std::vector<MapProjectileDef> projectiles;
    std::vector<MapRoute>         routes;
    MapWaveConfig waveConfig;

    // ── Save policy (Tier 4C.6) ──
    SavePolicy    savePolicy;

    // ── LevelEnd config (Tier 4D) ──
    LevelEndConfig  endConfig;

    // ── Config del jugador y HUD ──
    MapPlayerDef  playerDef;
    MapHUDDef     hudDef;

    // ── Estado del parser ──
    bool        valid = false;
    std::string error;
};


// ════════════════════════════════════════════════════════════════════════════
// MapLoader
// ════════════════════════════════════════════════════════════════════════════

class MapLoader
{
public:

    static MapData load(const std::string& path)
    {
        MapData data;

        std::ifstream file(path);
        if (!file.is_open())
        {
            data.error = "No se pudo abrir: " + path;
            std::cout << "[MapLoader] " << data.error << std::endl;
            return data;
        }

        std::vector<std::string> lines;
        {
            std::string line;
            while (std::getline(file, line))
                lines.push_back(line);
        }
        file.close();

        int i = 0, total = (int)lines.size();

        while (i < total)
        {
            std::string trimmed = trim(lines[i]);

            if (trimmed.empty() || isComment(trimmed))
            { i++; continue; }

            // ── Mapa ──
            if (trimmed == "mapa:" || trimmed == "map:")
            { i++; parseMapBlock(lines, i, total, data); continue; }

            // ── Entidades ──
            if (startsWith(trimmed, "npc "))
            { parseNPCBlock(lines, i, total, data); continue; }

            if (startsWith(trimmed, "enemigo ") || startsWith(trimmed, "enemy "))
            { parseEnemyBlock(lines, i, total, data); continue; }

            if (!isIndented(lines[i]) &&
                (startsWith(trimmed, "habilidad ") ||
                 startsWith(trimmed, "ability ")))
            { parseAbilityBlock(lines, i, total, data); continue; }

            if (startsWith(trimmed, "torre ") || startsWith(trimmed, "tower "))
            { parseTowerBlock(lines, i, total, data); continue; }

            if (startsWith(trimmed, "item ") || startsWith(trimmed, "objeto "))
            { parseItemBlock(lines, i, total, data); continue; }

            if (startsWith(trimmed, "trigger ") || startsWith(trimmed, "zona "))
            { parseTriggerBlock(lines, i, total, data); continue; }

            if (startsWith(trimmed, "marcador ") || startsWith(trimmed, "marker "))
            { parseSpatialMarkerBlock(lines, i, total, data); continue; }

            if (startsWith(trimmed, "proyectil ") || startsWith(trimmed, "projectile "))
            { parseProjectileBlock(lines, i, total, data); continue; }

            if (startsWith(trimmed, "ruta ") || startsWith(trimmed, "route "))
            {parseRouteBlock(lines, i, total, data); continue;}

            if (trimmed == "oleadas:" || trimmed == "waves:")
            {i++; parseWaveBlock(lines, i, total, data); continue;}

            // ── Tiles custom ──
            if (startsWith(trimmed, "tile ") || startsWith(trimmed, "casilla "))
            { parseTileDefBlock(lines, i, total, data); continue; }

            // ── Bloques de config ──
            if (trimmed == "jugador:" || trimmed == "player:")
            { i++; parsePlayerBlock(lines, i, total, data); continue; }

            if (trimmed == "hud:")
            { i++; parseHUDBlock(lines, i, total, data); continue; }

            if (trimmed == "guardar:" || trimmed == "save:")
            { i++; parseSaveBlock(lines, i, total, data); continue; }

            if (trimmed == "fin:" || trimmed == "end:")
            { i++; parseEndBlock(lines, i, total, data); continue; }

            // ── Líneas de config simples ──
            if (parseConfigLine(trimmed, data))
            { i++; continue; }

            std::cout << "[MapLoader] Linea " << (i+1)
                      << " ignorada: " << trimmed << std::endl;
            i++;
        }

        resolveEnemyAbilityUses(data);
        resolveTowerAbilityUses(data);

        data.gridHeight = (int)data.grid.size();
        data.gridWidth  = 0;
        for (const auto& row : data.grid)
            if ((int)row.size() > data.gridWidth)
                data.gridWidth = (int)row.size();

        data.valid = (data.gridHeight > 0 && data.gridWidth > 0);

        if (data.valid)
        {
            std::cout << "[MapLoader] '" << path << "': "
                      << data.gridWidth << "x" << data.gridHeight
                      << ", " << data.npcs.size()     << " NPCs"
                      << ", " << data.enemies.size()   << " enemigos"
                      << ", " << data.items.size()     << " items"
                      << ", " << data.triggers.size()  << " triggers"
                      << ", " << data.spatialMarkers.size() << " marcadores"
                      << ", " << data.abilities.size() << " habilidades"
                      << ", " << data.tileDefs.size()  << " tiles custom"
                      << ", " << data.projectiles.size() << " proyectiles"
                      << ", " << data.routes.size() << " rutas"
                      << ", salida=" << exitModeStr(data.exitMode) << std::endl;
        }
        else
        {
            data.error = "Mapa vacio o invalido en: " + path;
            std::cout << "[MapLoader] " << data.error << std::endl;
        }

        return data;
    }

private:

    // ════════════════════════════════════════════════════════════════════════
    // Utilidades
    // ════════════════════════════════════════════════════════════════════════

    static std::string trim(const std::string& s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static bool startsWith(const std::string& s, const std::string& prefix)
    {
        return s.size() >= prefix.size() &&
               s.compare(0, prefix.size(), prefix) == 0;
    }

    static bool isComment(const std::string& trimmed)
    {
        return !trimmed.empty() && trimmed[0] == '#' &&
               (trimmed.size() == 1 || trimmed[1] == ' ' || trimmed[1] == '\t');
    }

    static std::string extractQuoted(const std::string& s, size_t from = 0)
    {
        size_t q1 = s.find('"', from);
        if (q1 == std::string::npos) return "";
        size_t q2 = s.find('"', q1 + 1);
        if (q2 == std::string::npos) return "";
        return s.substr(q1 + 1, q2 - q1 - 1);
    }

    static size_t indentLevel(const std::string& raw)
    {
        size_t n = 0;
        for (char c : raw)
        {
            if (c == ' ')       n++;
            else if (c == '\t') n += 4;
            else                break;
        }
        return n;
    }

    static void parseHexColor(const std::string& hex,
                              float& r, float& g, float& b)
    {
        r = g = b = 0.7f;
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);
        if (h.size() < 6) return;
        try {
            r = std::stoi(h.substr(0,2), nullptr, 16) / 255.0f;
            g = std::stoi(h.substr(2,2), nullptr, 16) / 255.0f;
            b = std::stoi(h.substr(4,2), nullptr, 16) / 255.0f;
        } catch (...) {}
    }

    static std::string exitModeStr(MapExitMode m)
    {
        switch (m) {
            case MapExitMode::TalkAll:    return "hablar_todos";
            case MapExitMode::ReachExit:  return "llegar";
            case MapExitMode::KillAll:    return "matar_todos";
            case MapExitMode::CollectAll: return "recoger_todos";
            case MapExitMode::Timer:      return "timer";
            case MapExitMode::Variable:   return "variable";
            case MapExitMode::Waves:      return "oleadas";
            case MapExitMode::Lua:        return "lua";
            default:                      return "ninguna";
        }
    }

    // Bilingüe: compara contra variante español e inglés
    static bool isKw(const std::string& word,
                     const std::string& es, const std::string& en)
    {
        return word == es || word == en;
    }

    // ── Extraer key: value de una línea ────────────────────────────────────
    // Retorna true si encontró ':', y llena key/val.

    static bool splitKV(const std::string& line,
                        std::string& key, std::string& val)
    {
        size_t colon = line.find(':');
        if (colon == std::string::npos) return false;
        // ── FOLD-PARTIDO: la clave del `.eclmap` es vocabulario del MOTOR ──
        //
        // Hasta hoy salia byte-exacta y `isKw` la comparaba byte a byte, o sea
        // que `HP:` no era `hp:`. Plegar aca -en la unica puerta, no en las 115
        // comparaciones- es legitimo por las dos medidas de siempre: los 115
        // literales de clave distintos de este archivo ya estan TODOS en
        // minuscula (colision al plegar: CERO), y NINGUN nombre inventado por el
        // creador pasa por esta variable (cero inserciones con `[key]`).
        //
        // El VALOR no se toca aca: puede ser una ruta, un id que el creador
        // invento o prosa. Los valores que SI son vocabulario se pliegan uno por
        // uno en su sitio.
        key = eclips::vocabToLower(trim(line.substr(0, colon)));
        val = trim(line.substr(colon + 1));
        return true;
    }

    static bool parseBareAbilityId(const std::string& token,
                                   std::string& out)
    {
        const std::string candidate = trim(token);
        if (candidate.empty()) return false;

        for (char c : candidate)
        {
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
                c == '"' || c == '#' || c == ':')
            {
                return false;
            }
        }

        out = candidate;
        return true;
    }

    static bool parseAbilityHeaderId(const std::string& header,
                                     std::string& out)
    {
        size_t prefixLength = 0;
        if (startsWith(header, "habilidad "))
            prefixLength = 10;
        else if (startsWith(header, "ability "))
            prefixLength = 8;
        else
            return false;

        if (header.empty() || header.back() != ':')
            return false;

        return parseBareAbilityId(
            header.substr(prefixLength,
                          header.size() - prefixLength - 1),
            out);
    }

    static bool claimAbilityField(Gameplay2DAbilityFieldMask& fields,
                                  Gameplay2DAbilityField field,
                                  const std::string& context)
    {
        const Gameplay2DAbilityFieldMask bit =
            gameplay2DAbilityFieldBit(field);
        if ((fields & bit) != 0u)
        {
            std::cerr << "[MapLoader] " << context
                      << ": campo duplicado; se conserva el primero"
                      << std::endl;
            return false;
        }
        fields |= bit;
        return true;
    }

    static bool parseAbilityNonNegativeFloat(const std::string& token,
                                             const std::string& context,
                                             float& out)
    {
        float parsed = 0.0f;
        if (parseFloatStrict(token, parsed) != NumParse::Ok ||
            parsed < 0.0f)
        {
            std::cerr << "[MapLoader] " << context
                      << ": numero no negativo invalido '"
                      << token << "'; se conserva el valor heredado"
                      << std::endl;
            return false;
        }
        out = parsed;
        return true;
    }

    static bool parseAbilityPositiveInt(const std::string& token,
                                        const std::string& context,
                                        int& out)
    {
        int parsed = 0;
        if (parseIntStrict(token, parsed) != NumParse::Ok ||
            parsed <= 0)
        {
            std::cerr << "[MapLoader] " << context
                      << ": entero positivo invalido '"
                      << token << "'; se conserva el valor heredado"
                      << std::endl;
            return false;
        }
        out = parsed;
        return true;
    }

    // El vocabulario publico empieza deliberadamente pequeno. Las otras
    // cuatro preferencias siguen disponibles solo para adaptar mapas legacy.
    static bool parsePublicAbilityPreference(
        const std::string& token,
        Gameplay2DTargetPreference& out)
    {
        const auto parsed = parsePublicGameplay2DTargetPreference(token);
        if (!parsed)
            return false;
        out = *parsed;
        return true;
    }

    static bool parsePublicCommittedDirectDamageCoordination(
        const std::string& token,
        Gameplay2DCommittedDirectDamageCoordination& out)
    {
        const std::string value = asciiToLower(token);
        if (value == "independiente" || value == "independent")
        {
            out =
                Gameplay2DCommittedDirectDamageCoordination::Independent;
            return true;
        }
        if (value == "omitir_objetivo_cubierto" ||
            value == "skip_covered_target")
        {
            out = Gameplay2DCommittedDirectDamageCoordination::
                SkipCoveredTarget;
            return true;
        }
        return false;
    }

    static bool parsePublicTargetRelation(
        const std::string& token,
        Gameplay2DTargetRelation& out)
    {
        const std::string value = asciiToLower(token);
        if (value == "opuesto" || value == "opposing")
        {
            out = Gameplay2DTargetRelation::Opposing;
            return true;
        }
        return false;
    }

    static bool parsePublicTargetCapability(
        const std::string& token,
        Gameplay2DTargetCapability& out)
    {
        const std::string value = asciiToLower(token);
        if (value == "recibe_dano" || value == "damageable")
        {
            out = Gameplay2DTargetCapability::Damageable;
            return true;
        }
        return false;
    }

    static bool parsePublicTargetBinding(
        const std::string& token,
        Gameplay2DTargetBinding& out)
    {
        const std::string value = asciiToLower(token);
        if (value == "independiente" || value == "independent")
        {
            out = Gameplay2DTargetBinding::Independent;
            return true;
        }
        if (value == "obstructor_locomocion_actual" ||
            value == "current_locomotion_blocker")
        {
            out = Gameplay2DTargetBinding::CurrentLocomotionBlocker;
            return true;
        }
        return false;
    }

    static bool parseLegacyTowerPreference(
        const std::string& token,
        Gameplay2DTargetPreference& out)
    {
        const std::string value = asciiToLower(token);
        if (value == "cercano" || value == "nearest")
            out = Gameplay2DTargetPreference::Nearest;
        else if (value == "lejano" || value == "farthest")
            out = Gameplay2DTargetPreference::Farthest;
        else if (value == "primero" || value == "first")
            out = Gameplay2DTargetPreference::HighestRouteIndex;
        else if (value == "ultimo" || value == "last")
            out = Gameplay2DTargetPreference::LowestRouteIndex;
        else if (value == "fuerte" || value == "strongest")
            out = Gameplay2DTargetPreference::HighestCurrentHp;
        else if (value == "debil" || value == "weakest")
            out = Gameplay2DTargetPreference::LowestCurrentHp;
        else
            return false;
        return true;
    }

    // ── Extraer la letra de entidad de un header ───────────────────────────
    // "enemigo E "Slime" #00FF00:" -> tile='E', name="Slime", color parsed

    static void parseEntityHeader(const std::string& header, size_t startPos,
                                  char& tile, std::string& name,
                                  float& r, float& g, float& b)
    {
        size_t pos = startPos;
        while (pos < header.size() && header[pos] == ' ') pos++;

        tile = '?';
        if (pos < header.size() && header[pos] >= 'A' && header[pos] <= 'Z')
            tile = header[pos];

        name = extractQuoted(header);
        if (name.empty()) name = std::string(1, tile);

        size_t hashPos = header.find('#', pos + 1);
        if (hashPos != std::string::npos)
        {
            size_t colorEnd = hashPos + 1;
            while (colorEnd < header.size() && std::isxdigit(header[colorEnd]))
                colorEnd++;
            parseHexColor(header.substr(hashPos, colorEnd - hashPos), r, g, b);
        }
    }

    // ── Verificar si una línea está indentada (parte de un bloque) ─────────

    static bool isIndented(const std::string& raw)
    {
        return !raw.empty() && (raw[0] == ' ' || raw[0] == '\t');
    }

    // Cuenta indentación en espacios-equivalentes (tab = 4 espacios).
    // Usado por parsers multi-nivel (ej: fin: > victoria: > colores:).
    static int countIndent(const std::string& raw)
    {
        int n = 0;
        for (char c : raw)
        {
            if (c == ' ')  n += 1;
            else if (c == '\t') n += 4;
            else break;
        }
        return n;
    }

    // ════════════════════════════════════════════════════════════════════════
    // Parser: set:
    // ════════════════════════════════════════════════════════════════════════

    static bool parseSetLine(const std::string& trimmed, MapVarSet& out)
    {
        if (!startsWith(trimmed, "set:") && !startsWith(trimmed, "set :"))
            return false;

        size_t colonPos = trimmed.find(':');
        std::string rest = trim(trimmed.substr(colonPos + 1));

        std::istringstream iss(rest);
        std::string var, op, val;
        iss >> var >> op;
        std::string remainder;
        std::getline(iss, remainder);
        val = trim(remainder);

        if (!val.empty() && val.front() == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);

        if (var.empty() || op.empty()) return false;

        out.var = var;
        out.op  = op;
        out.val = val;
        return true;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Acciones (bloques de evento)
    // ════════════════════════════════════════════════════════════════════════
    //
    // Parsea líneas como:
    //   set: score + 100
    //   sonido: "boom.ogg"
    //   notificar: "Texto"
    //   spawnear: E 3 aleatorio
    //   desaparecer: true
    //   soltar: M
    //   cargar_mapa: "nivel2.eclmap" 5, 3
    //   camara: shake 0.5

    static bool parseActionLine(const std::string& trimmed, MapAction& action)
    {
        std::string key, val;

        // set: tiene su propio parser
        MapVarSet vs;
        if (parseSetLine(trimmed, vs))
        {
            action.type   = MapActionType::SetVar;
            action.varSet = vs;
            return true;
        }

        if (!splitKV(trimmed, key, val)) return false;

        // sonido/sound
        if (isKw(key, "sonido", "sound"))
        {
            action.type = MapActionType::Sound;
            action.str1 = extractQuoted(trimmed);
            if (action.str1.empty()) action.str1 = val;
            return true;
        }

        // musica/music
        if (isKw(key, "musica", "music"))
        {
            action.type = MapActionType::Music;
            action.str1 = extractQuoted(trimmed);
            if (action.str1.empty()) action.str1 = val;
            return true;
        }

        // notificar/notify
        if (isKw(key, "notificar", "notify"))
        {
            action.type = MapActionType::Notify;
            action.str1 = extractQuoted(trimmed);
            if (action.str1.empty()) action.str1 = val;
            return true;
        }

        // dialogo/dialog
        if (isKw(key, "dialogo", "dialog"))
        {
            action.type = MapActionType::Dialog;
            action.str1 = extractQuoted(trimmed);
            if (action.str1.empty()) action.str1 = val;
            return true;
        }

        // desaparecer/vanish
        if (isKw(key, "desaparecer", "vanish"))
        {
            action.type = MapActionType::Vanish;
            action.flag = parseBool(val, false, "desaparecer/vanish");
            return true;
        }

        // soltar/drop
        if (isKw(key, "soltar", "drop"))
        {
            action.type = MapActionType::Drop;
            std::string v = trim(val);
            if (!v.empty()) action.entityChar = v[0];
            return true;
        }

        // spawnear/spawn: E 3 [aleatorio | en X, Y]
        if (isKw(key, "spawnear", "spawn"))
        {
            action.type = MapActionType::Spawn;
            std::istringstream iss(val);
            std::string letter;
            iss >> letter;
            if (!letter.empty()) action.entityChar = letter[0];
            iss >> action.count;
            if (action.count <= 0) action.count = 1;

            // Posición (opcional)
            std::string posMode;
            iss >> posMode;
            std::string pmLower = posMode;
            std::transform(pmLower.begin(), pmLower.end(), pmLower.begin(), ::tolower);
            if (pmLower == "aleatorio" || pmLower == "random")
                action.str2 = "random";
            else if (pmLower == "en" || pmLower == "at")
            {
                action.str2 = "at";
                // leer X, Y
                char comma;
                iss >> action.posX >> comma >> action.posY;
            }
            return true;
        }

        // cargar_mapa/load_map: "nivel2.eclmap" X, Y
        if (isKw(key, "cargar_mapa", "load_map"))
        {
            action.type = MapActionType::LoadMap;
            action.str1 = extractQuoted(trimmed);

            // Buscar posición después de las comillas
            size_t q2 = trimmed.rfind('"');
            if (q2 != std::string::npos && q2 + 1 < trimmed.size())
            {
                std::string posStr = trim(trimmed.substr(q2 + 1));
                std::istringstream piss(posStr);
                char comma;
                piss >> action.posX >> comma >> action.posY;
            }
            return true;
        }

        // camara/camera: shake TIME | pan X,Y TIME | seguir | zoom Z TIME
        if (isKw(key, "camara", "camera"))
        {
            std::istringstream iss(val);
            std::string subCmd;
            iss >> subCmd;
            std::string scLower = subCmd;
            std::transform(scLower.begin(), scLower.end(), scLower.begin(), ::tolower);

            if (scLower == "shake" || scLower == "sacudir")
            {
                action.type = MapActionType::CameraShake;
                iss >> action.duration;
                if (action.duration <= 0) action.duration = 0.5f;
            }
            else if (scLower == "pan" || scLower == "mover")
            {
                action.type = MapActionType::CameraPan;
                char comma;
                iss >> action.posX >> comma >> action.posY >> action.duration;
            }
            else if (scLower == "seguir" || scLower == "follow")
            {
                action.type = MapActionType::CameraFollow;
            }
            else if (scLower == "zoom")
            {
                action.type = MapActionType::CameraZoom;
                iss >> action.value >> action.duration;
            }
            else
            {
                std::cout << "[MapLoader] camara subcomando desconocido: "
                          << subCmd << std::endl;
                return false;
            }
            return true;
        }

        // esperar/wait
        if (isKw(key, "esperar", "wait"))
        {
            action.type = MapActionType::Wait;
            try { action.duration = std::stof(val); } catch (...) {}
            return true;
        }

        // vn
        if (key == "vn")
        {
            action.type = MapActionType::RunVN;
            action.str1 = extractQuoted(trimmed);
            // Escena opcional (segunda palabra después de las comillas)
            size_t q2 = trimmed.rfind('"');
            if (q2 != std::string::npos && q2 + 1 < trimmed.size())
                action.str2 = trim(trimmed.substr(q2 + 1));
            return true;
        }

        // lua
        if (key == "lua")
        {
            action.type = MapActionType::RunLua;
            action.str1 = extractQuoted(trimmed);
            if (action.str1.empty()) action.str1 = val;
            return true;
        }
        
        // destruir_tile/destroy_tile: COL, ROW
        if (isKw(key, "destruir_tile", "destroy_tile"))
        {
            action.type = MapActionType::DestroyTile;
            std::istringstream iss(val);
            char comma;
            iss >> action.posX >> comma >> action.posY;
            return true;
        }

        // crear_tile/create_tile: COL, ROW CHAR
        if (isKw(key, "crear_tile", "create_tile"))
        {
            action.type = MapActionType::CreateTile;
            std::istringstream iss(val);
            char comma;
            iss >> action.posX >> comma >> action.posY;
            std::string charStr;
            iss >> charStr;
            if (!charStr.empty()) action.entityChar = charStr[0];
            else action.entityChar = '#'; // Default: pared
            return true;
        }

        return false;
    }

    // ── Parsear bloque de evento indentado ─────────────────────────────────
    // Lee acciones hasta que el indent vuelve al nivel del keyword padre.

    static void parseEventBlock(const std::vector<std::string>& lines,
                                int& i, int total, size_t parentIndent,
                                std::vector<MapAction>& actions)
    {
        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            size_t lineIndent = indentLevel(raw);
            if (lineIndent <= parentIndent) break;

            MapAction action;
            if (parseActionLine(trimmed, action))
                actions.push_back(action);
            else
                std::cout << "[MapLoader] Accion desconocida: " << trimmed << std::endl;

            i++;
        }
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Movimiento
    // ════════════════════════════════════════════════════════════════════════

    static MapMovement parseMovement(const std::string& val)
    {
        MapMovement mov;
        std::istringstream iss(val);
        std::string typeStr;
        iss >> typeStr;

        std::string tLower = typeStr;
        std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);

        if (tLower == "quieto"     || tLower == "still")
            mov.type = MapMovementType::Still;
        else if (tLower == "patrulla_h" || tLower == "patrol_h")
        { mov.type = MapMovementType::PatrolH; iss >> mov.param; }
        else if (tLower == "patrulla_v" || tLower == "patrol_v")
        { mov.type = MapMovementType::PatrolV; iss >> mov.param; }
        else if (tLower == "seguir"     || tLower == "follow")
        { mov.type = MapMovementType::Follow; iss >> mov.param; }
        else if (tLower == "huir"       || tLower == "flee")
        { mov.type = MapMovementType::Flee; iss >> mov.param; }
        else if (tLower == "rebote"     || tLower == "bounce")
            mov.type = MapMovementType::Bounce;
        else if (tLower == "aleatorio"  || tLower == "random")
        { mov.type = MapMovementType::Random; iss >> mov.param; }
        else if (tLower == "circular"   || tLower == "circle")
        { mov.type = MapMovementType::Circle; iss >> mov.param; }
        else if (tLower == "ruta"       || tLower == "path")
        { mov.type = MapMovementType::Path; iss >> mov.pathName; }
        else
            std::cout << "[MapLoader] Movimiento desconocido: " << typeStr << std::endl;

        return mov;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Configuración (key: value en raíz)
    // ════════════════════════════════════════════════════════════════════════

    static bool parseConfigLine(const std::string& line, MapData& data)
    {
        std::string key, val;
        if (!splitKV(line, key, val)) return false;

        // Evitar false positives con keywords que tienen ':'
        if (key == "mapa" || key == "map" ||
            key == "elegir" || key == "choose" ||
            key == "set" ||
            key == "hud" || key == "jugador" || key == "player" ||
            key == "guardar" || key == "save")
            return false;

        // ── Existentes ──
        if (isKw(key, "modo", "mode"))
        { data.mode = val; return true; }

        if (isKw(key, "musica", "music"))
        { data.musicPath = extractQuoted(line);
          if (data.musicPath.empty()) data.musicPath = val; return true; }

        if (isKw(key, "fuente", "font"))
        { data.fontPath = extractQuoted(line);
          if (data.fontPath.empty()) data.fontPath = val; return true; }

        if (isKw(key, "velocidad", "speed"))
        { try { data.playerSpeed = std::stof(val); } catch (...) {} return true; }

        if (key == "lua" || key == "script")
        { data.luaScript = extractQuoted(line);
          if (data.luaScript.empty()) data.luaScript = val; return true; }

        if (key == "tamano" || key == "tamaño" || key == "size")
        { try { data.playerSize = std::stof(val); } catch (...) {} return true; }

        // ── Salida (extendida) ──
        if (isKw(key, "salida", "exit"))
        {
            std::string lower = val;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if (lower == "hablar_todos" || lower == "talk_all")
                data.exitMode = MapExitMode::TalkAll;
            else if (lower == "llegar" || lower == "reach")
                data.exitMode = MapExitMode::ReachExit;
            else if (lower == "matar_todos" || lower == "kill_all")
                data.exitMode = MapExitMode::KillAll;
            else if (lower == "recoger_todos" || lower == "collect_all")
                data.exitMode = MapExitMode::CollectAll;
            else if (lower == "oleadas" || lower == "waves")
                data.exitMode = MapExitMode::Waves;
            else if (lower == "base_destruida" || lower == "base_destroyed")
                data.exitMode = MapExitMode::BaseDestroyed;
            else if (lower == "lua")
                data.exitMode = MapExitMode::Lua;
            else if (startsWith(lower, "timer"))
            {
                data.exitMode = MapExitMode::Timer;
                std::istringstream iss(val);
                std::string skip;
                iss >> skip >> data.exitTimer;
            }
            else if (startsWith(lower, "variable"))
            {
                data.exitMode = MapExitMode::Variable;
                // "variable victoria == 1"
                std::istringstream iss(val);
                std::string skip;
                iss >> skip >> data.exitVar >> data.exitOp >> data.exitVal;
            }
            else
                std::cout << "[MapLoader] Salida desconocida: " << val << std::endl;
            return true;
        }

        // ── Side-scroll config ──
        if (isKw(key, "gravedad", "gravity"))
        { try { data.gravity = std::stof(val); } catch (...) {} return true; }

        if (isKw(key, "salto", "jump"))
        { try { data.jumpForce = std::stof(val); } catch (...) {} return true; }

        if (key == "coyote")
        { try { data.coyoteTime = std::stof(val); } catch (...) {} return true; }

        if (isKw(key, "doble_salto", "double_jump"))
        { data.doubleJump = parseBool(val, false, "doble_salto/double_jump"); return true; }

        if (key == "dash")
        { data.dash = parseBool(val, false, "dash"); return true; }

        if (isKw(key, "dash_velocidad", "dash_speed"))
        { try { data.dashSpeed = std::stof(val); } catch (...) {} return true; }

        if (isKw(key, "dash_duracion", "dash_duration"))
        { try { data.dashDuration = std::stof(val); } catch (...) {} return true; }

        if (key == "wall_slide")
        { data.wallSlide = parseBool(val, false, "wall_slide"); return true; }

        if (key == "wall_jump")
        { data.wallJump = parseBool(val, false, "wall_jump"); return true; }

        if (isKw(key, "dash_cooldown", "dash_cooldown"))
        { try { data.dashCooldown = std::stof(val); } catch (...) {} return true; }

        return false;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Mapa ASCII
    // ════════════════════════════════════════════════════════════════════════

    static bool isMapChar(char c)
    {
        // Tiles base
        if (c == '#' || c == '.' || c == '=' || c == ' ' || c == '>') return true;
        // Entidades (A-Z)
        if (c >= 'A' && c <= 'Z') return true;
        // Tiles custom permitidos
        if (c == '~' || c == '-' || c == '^' || c == '+' || c == '<' ||
            c == '*' || c == '@' || c == '!' || c == ':' || c == ';' ||
            c == '|' || c == '_' || c == '/' || c == '\\' || c == ',' ||
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' ||
            c == '}' || c == '?' || c == '$' || c == '%' || c == '&' ||
            c == '\'' || c == '`') return true;
        return false;
    }

    static bool isMapRow(const std::string& trimmed)
    {
        if (trimmed.empty()) return false;
        for (char c : trimmed)
            if (!isMapChar(c)) return false;
        return true;
    }

    static void parseMapBlock(const std::vector<std::string>& lines,
                              int& i, int total, MapData& data)
    {
        while (i < total)
        {
            std::string trimmed = trim(lines[i]);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }
            if (isMapRow(trimmed)) { data.grid.push_back(trimmed); i++; }
            else break;
        }
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: NPC (existente — backward compatible)
    // ════════════════════════════════════════════════════════════════════════

    static void parseNPCBlock(const std::vector<std::string>& lines,
                              int& i, int total, MapData& data)
    {
        std::string header = trim(lines[i]);
        i++;

        MapNPCDef npc;
        parseEntityHeader(header, 4, npc.tile, npc.name, npc.r, npc.g, npc.b);
        npc.dialog.speaker = npc.name;

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            // set:
            MapVarSet vs;
            if (parseSetLine(trimmed, vs))
            { npc.sets.push_back(vs); i++; continue; }

            // sprite:
            std::string key, val;
            if (splitKV(trimmed, key, val) && isKw(key, "sprite", "sprite"))
            {
                npc.spritePath = extractQuoted(trimmed);
                if (npc.spritePath.empty()) npc.spritePath = val;
                i++; continue;
            }

            // elegir/choose
            if (trimmed == "elegir:" || trimmed == "choose:")
            {
                size_t ci = indentLevel(raw);
                i++;
                parseChoices(lines, i, total, ci, npc);
                continue;
            }

            // Línea de diálogo
            std::string text;
            if (trimmed.front() == '"') text = extractQuoted(trimmed);
            else text = trimmed;
            if (!text.empty()) npc.dialog.lines.push_back(text);
            i++;
        }

        data.npcs.push_back(npc);
        std::cout << "[MapLoader] NPC '" << npc.tile << "' = " << npc.name
                  << " (" << npc.dialog.lines.size() << " lineas, "
                  << npc.dialog.choices.size() << " choices, "
                  << npc.sets.size() << " sets)" << std::endl;
    }

    static void parseChoices(const std::vector<std::string>& lines,
                             int& i, int total, size_t chooseIndent,
                             MapNPCDef& npc)
    {
        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            size_t lineIndent = indentLevel(raw);
            if (lineIndent <= chooseIndent) break;

            size_t arrowPos = trimmed.find("->");
            if (arrowPos != std::string::npos && trimmed.front() == '"')
            {
                MapDialogChoice choice;
                choice.label = extractQuoted(trimmed);
                size_t optionIndent = lineIndent;
                i++;

                while (i < total)
                {
                    const std::string& rraw = lines[i];
                    std::string rt = trim(rraw);
                    if (rt.empty() || isComment(rt)) { i++; continue; }
                    size_t ri = indentLevel(rraw);
                    if (ri <= optionIndent) break;

                    MapVarSet vs;
                    if (parseSetLine(rt, vs))
                    { choice.sets.push_back(vs); i++; continue; }

                    std::string text;
                    if (rt.front() == '"') text = extractQuoted(rt);
                    else text = rt;
                    if (!text.empty()) choice.result.push_back(text);
                    i++;
                }

                npc.dialog.choices.push_back(choice);
            }
            else
            {
                std::cout << "[MapLoader] Choice mal formado: "
                          << trimmed << std::endl;
                i++;
            }
        }
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Enemigo
    // ════════════════════════════════════════════════════════════════════════

    static void parseEnemyBlock(const std::vector<std::string>& lines,
                                int& i, int total, MapData& data)
    {
        std::string header = trim(lines[i]);
        const size_t enemyIndent = indentLevel(lines[i]);
        i++;

        MapEnemyDef enemy;
        // El disparo legacy enemigo nunca tuvo alcance: conservar esa semántica
        // requiere un candidato efectivamente sin límite en el adapter nuevo.
        enemy.legacyAbility.values.range =
            std::numeric_limits<float>::max();
        // "enemigo " = 8 chars, "enemy " = 6 chars
        size_t skip = startsWith(header, "enemigo") ? 8 : 6;
        parseEntityHeader(header, skip, enemy.tile, enemy.name,
                          enemy.r, enemy.g, enemy.b);

        while (i < total)
        {
            const std::string& raw = lines[i];
            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }
            if (indentLevel(raw) <= enemyIndent) break;

            std::string key, val;

            if (startsWith(trimmed, "habilidad ") ||
                startsWith(trimmed, "ability "))
            {
                parseAbilityUseBlock(
                    lines, i, total,
                    "Enemigo", enemy.name,
                    enemy.abilityUses,
                    enemy.explicitAbilityBlockSeen);
                continue;
            }

            // Evento: al_morir/on_death
            if (trimmed == "al_morir:" || trimmed == "on_death:")
            {
                size_t evIndent = indentLevel(raw);
                i++;
                parseEventBlock(lines, i, total, evIndent, enemy.onDeath);
                continue;
            }

            // Propiedades
            if (splitKV(trimmed, key, val))
            {
                if (isKw(key, "sprite", "sprite"))
                { enemy.spritePath = extractQuoted(trimmed);
                  if (enemy.spritePath.empty()) enemy.spritePath = val; }
                else if (key == "hp")
                { try { enemy.hp = std::stoi(val); } catch (...) {} }
                else if (isKw(key, "daño", "damage") || key == "dano")
                { try { enemy.damage = std::stoi(val); } catch (...) {} }
                else if (isKw(key, "velocidad", "speed"))
                { try { enemy.speed = std::stof(val); } catch (...) {} }
                else if (isKw(key, "movimiento", "movement"))
                { enemy.movement = parseMovement(val); }
                else if (isKw(key, "rango", "range"))
                { try { enemy.range = std::stof(val); } catch (...) {} }
                else if (key == "knockback")
                { try { enemy.knockback = std::stof(val); } catch (...) {} }
                else if (isKw(key, "invulnerable", "invulnerable"))
                { enemy.invulnerable = parseBool(val, false, "invulnerable"); }
                else if (isKw(key, "cadencia", "fire_rate"))
                {
                    enemy.legacyAbility.authoredFields |=
                        gameplay2DAbilityFieldBit(
                            Gameplay2DAbilityField::FireRate);
                    parseAbilityNonNegativeFloat(
                        val, "Enemigo '" + enemy.name + "' / cadencia legacy",
                        enemy.legacyAbility.values.fireRate);
                }
                else if (isKw(key, "proyectil", "projectile"))
                {
                    enemy.legacyAbility.authoredFields |=
                        gameplay2DAbilityFieldBit(
                            Gameplay2DAbilityField::Projectile);
                    enemy.legacyAbility.values.projectile =
                        extractQuoted(trimmed);
                    if (enemy.legacyAbility.values.projectile.empty())
                        enemy.legacyAbility.values.projectile = val;
                }
                else if (isKw(key, "atacan_torres", "attacks_towers") ||
                         isKw(key, "atacar_torres", "attack_towers"))
                { enemy.attacksTowers = parseBool(val, false, "atacan_torres/attacks_towers"); }
                else if (isKw(key, "rango_ataque_torre", "tower_attack_range"))
                { try { enemy.towerAttackRange = std::stof(val); } catch (...) {} }
                else if (isKw(key, "cadencia_torre", "tower_attack_rate"))
                { try { enemy.towerAttackRate = std::stof(val); } catch (...) {} }
                else
                    std::cout << "[MapLoader] Enemigo prop desconocida: "
                              << key << std::endl;
            }

            i++;
        }

        data.enemies.push_back(enemy);
        std::cout << "[MapLoader] Enemigo '" << enemy.tile << "' = " << enemy.name
                  << " (HP:" << enemy.hp << " DMG:" << enemy.damage
                  << " SPD:" << enemy.speed << " "
                  << enemy.abilityUses.size() << " usos explicitos, "
                  << enemy.onDeath.size() << " acciones al_morir)" << std::endl;
    }

    // ════════════════════════════════════════════════════════════════════════
    // Parser: Habilidad neutral de Gameplay 2D
    // ════════════════════════════════════════════════════════════════════════
    //
    //   habilidad pulso:
    //     alcance: 300
    //     cadencia: 1.0
    //     proyectil: F
    //     preferencia: mas_cercano
    //     coordinacion:
    //       daño_directo_comprometido: omitir_objetivo_cubierto

    static void parseAbilityAcquisitionBlock(
        const std::vector<std::string>& lines,
        int& i, int total,
        const std::string& context,
        Gameplay2DAcquisition& acquisition,
        Gameplay2DAbilityFieldMask& seenFields)
    {
        const size_t blockIndent = indentLevel(lines[i]);
        i++;

        while (i < total)
        {
            const std::string& raw = lines[i];
            const std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed))
            {
                i++;
                continue;
            }
            if (indentLevel(raw) <= blockIndent)
                break;

            std::string key, val;
            if (!splitKV(trimmed, key, val))
            {
                std::cerr << "[MapLoader] " << context
                          << " / adquisicion: linea desconocida '"
                          << trimmed << "'" << std::endl;
                i++;
                continue;
            }

            if (isKw(key, "relacion", "relation"))
            {
                if (claimAbilityField(
                        seenFields,
                        Gameplay2DAbilityField::AcquisitionRelation,
                        context + " / adquisicion / relacion"))
                {
                    Gameplay2DTargetRelation parsed;
                    if (parsePublicTargetRelation(val, parsed))
                        acquisition.relation = parsed;
                    else
                        std::cerr << "[MapLoader] " << context
                                  << ": relacion de adquisicion desconocida '"
                                  << val << "'" << std::endl;
                }
            }
            else if (isKw(key, "capacidad", "capability"))
            {
                if (claimAbilityField(
                        seenFields,
                        Gameplay2DAbilityField::AcquisitionCapability,
                        context + " / adquisicion / capacidad"))
                {
                    Gameplay2DTargetCapability parsed;
                    if (parsePublicTargetCapability(val, parsed))
                        acquisition.capability = parsed;
                    else
                        std::cerr << "[MapLoader] " << context
                                  << ": capacidad de adquisicion desconocida '"
                                  << val << "'" << std::endl;
                }
            }
            else if (isKw(key, "vinculo", "binding"))
            {
                if (claimAbilityField(
                        seenFields,
                        Gameplay2DAbilityField::AcquisitionBinding,
                        context + " / adquisicion / vinculo"))
                {
                    Gameplay2DTargetBinding parsed;
                    if (parsePublicTargetBinding(val, parsed))
                        acquisition.binding = parsed;
                    else
                        std::cerr << "[MapLoader] " << context
                                  << ": vinculo de adquisicion desconocido '"
                                  << val << "'" << std::endl;
                }
            }
            else
            {
                std::cerr << "[MapLoader] " << context
                          << " / adquisicion: propiedad desconocida '"
                          << key << "'" << std::endl;
            }

            i++;
        }
    }

    static void parseAbilityCoordinationBlock(
        const std::vector<std::string>& lines,
        int& i, int total,
        const std::string& context,
        Gameplay2DCoordination& coordination,
        Gameplay2DAbilityFieldMask& seenFields)
    {
        const size_t blockIndent = indentLevel(lines[i]);
        i++;

        while (i < total)
        {
            const std::string& raw = lines[i];
            const std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed))
            {
                i++;
                continue;
            }
            if (indentLevel(raw) <= blockIndent)
                break;

            std::string key, val;
            if (!splitKV(trimmed, key, val))
            {
                std::cerr << "[MapLoader] " << context
                          << " / coordinacion: linea desconocida '"
                          << trimmed << "'" << std::endl;
                i++;
                continue;
            }

            if (isKw(
                    key,
                    "daño_directo_comprometido",
                    "committed_direct_damage") ||
                key == "dano_directo_comprometido")
            {
                if (claimAbilityField(
                        seenFields,
                        Gameplay2DAbilityField::
                            CoordinationCommittedDirectDamage,
                        context +
                            " / coordinacion / "
                            "daño_directo_comprometido"))
                {
                    Gameplay2DCommittedDirectDamageCoordination parsed;
                    if (parsePublicCommittedDirectDamageCoordination(
                            val, parsed))
                    {
                        coordination.committedDirectDamage = parsed;
                    }
                    else
                    {
                        std::cerr << "[MapLoader] " << context
                                  << ": coordinacion de daño directo "
                                     "comprometido desconocida '"
                                  << val
                                  << "'; se conserva el valor heredado"
                                  << std::endl;
                    }
                }
            }
            else
            {
                std::cerr << "[MapLoader] " << context
                          << " / coordinacion: propiedad desconocida '"
                          << key << "'" << std::endl;
            }

            i++;
        }
    }

    static void parseAbilityBlock(const std::vector<std::string>& lines,
        int& i, int total, MapData& data)
    {
        const std::string header = trim(lines[i]);
        const size_t blockIndent = indentLevel(lines[i]);
        i++;

        Gameplay2DAbilityDef ability;
        const bool validId = parseAbilityHeaderId(header, ability.id);
        if (!validId)
        {
            std::cerr << "[MapLoader] Habilidad: encabezado invalido '"
                      << header << "'; se descarta la definicion"
                      << std::endl;
        }

        Gameplay2DAbilityFieldMask seenFields = 0u;
        while (i < total)
        {
            const std::string& raw = lines[i];
            const std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed))
            {
                i++;
                continue;
            }
            if (indentLevel(raw) <= blockIndent)
                break;

            if (trimmed == "adquisicion:" ||
                trimmed == "acquisition:")
            {
                parseAbilityAcquisitionBlock(
                    lines, i, total,
                    "Habilidad '" + ability.id + "'",
                    ability.values.acquisition,
                    seenFields);
                continue;
            }

            if (trimmed == "coordinacion:" ||
                trimmed == "coordination:")
            {
                parseAbilityCoordinationBlock(
                    lines, i, total,
                    "Habilidad '" + ability.id + "'",
                    ability.values.coordination,
                    seenFields);
                continue;
            }

            std::string key, val;
            if (!splitKV(trimmed, key, val))
            {
                std::cerr << "[MapLoader] Habilidad '" << ability.id
                          << "': linea desconocida '" << trimmed << "'"
                          << std::endl;
                i++;
                continue;
            }

            if (isKw(key, "alcance", "range"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::Range,
                        "Habilidad '" + ability.id + "' / alcance"))
                {
                    parseAbilityNonNegativeFloat(
                        val, "Habilidad '" + ability.id + "' / alcance",
                        ability.values.range);
                }
            }
            else if (isKw(key, "cadencia", "fire_rate"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::FireRate,
                        "Habilidad '" + ability.id + "' / cadencia"))
                {
                    parseAbilityNonNegativeFloat(
                        val, "Habilidad '" + ability.id + "' / cadencia",
                        ability.values.fireRate);
                }
            }
            else if (isKw(key, "proyectil", "projectile"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::Projectile,
                        "Habilidad '" + ability.id + "' / proyectil"))
                {
                    std::string projectileId;
                    if (parseBareAbilityId(val, projectileId))
                        ability.values.projectile = projectileId;
                    else
                        std::cerr << "[MapLoader] Habilidad '" << ability.id
                                  << "': referencia de proyectil invalida '"
                                  << val << "'" << std::endl;
                }
            }
            else if (isKw(key, "daño_directo", "direct_damage") ||
                     key == "dano_directo")
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::DirectDamage,
                        "Habilidad '" + ability.id + "' / daño_directo"))
                {
                    int parsed = 0;
                    if (parseAbilityPositiveInt(
                            val,
                            "Habilidad '" + ability.id +
                                "' / daño_directo",
                            parsed))
                    {
                        ability.values.directDamage = parsed;
                    }
                }
            }
            else if (isKw(key, "preferencia", "preference"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::Preference,
                        "Habilidad '" + ability.id + "' / preferencia"))
                {
                    Gameplay2DTargetPreference parsed;
                    if (parsePublicAbilityPreference(val, parsed))
                        ability.values.preference = parsed;
                    else
                        std::cerr << "[MapLoader] Habilidad '" << ability.id
                                  << "': preferencia publica desconocida '"
                                  << val << "'; se conserva mas_cercano"
                                  << std::endl;
                }
            }
            else
            {
                std::cerr << "[MapLoader] Habilidad '" << ability.id
                          << "': propiedad desconocida '" << key << "'"
                          << std::endl;
            }

            i++;
        }

        if (!validId)
            return;

        if (gameplay2DAbilityHasConflictingEffects(ability.values))
        {
            std::cerr << "[MapLoader] Habilidad '" << ability.id
                      << "': proyectil y daño_directo son excluyentes; "
                         "se descarta la definicion"
                      << std::endl;
            return;
        }

        const auto duplicate = std::find_if(
            data.abilities.begin(), data.abilities.end(),
            [&](const Gameplay2DAbilityDef& current) {
                return current.id == ability.id;
            });
        if (duplicate != data.abilities.end())
        {
            std::cerr << "[MapLoader] Habilidad duplicada '" << ability.id
                      << "'; se conserva la primera" << std::endl;
            return;
        }

        data.abilities.push_back(ability);
        std::cout << "[MapLoader] Habilidad '" << ability.id
                  << "' (RNG:" << ability.values.range
                  << " FR:" << ability.values.fireRate << ")"
                  << std::endl;
    }

    static void parseAbilityUseBlock(
        const std::vector<std::string>& lines,
        int& i, int total,
        const std::string& ownerKind,
        const std::string& ownerName,
        std::vector<Gameplay2DAbilityUseDef>& abilityUses,
        bool& explicitAbilityBlockSeen)
    {
        const std::string header = trim(lines[i]);
        const size_t blockIndent = indentLevel(lines[i]);
        explicitAbilityBlockSeen = true;
        i++;

        Gameplay2DAbilityUseDef use;
        const bool validId = parseAbilityHeaderId(header, use.id);
        if (!validId)
        {
            std::cerr << "[MapLoader] " << ownerKind << " '" << ownerName
                      << "': encabezado de uso invalido '" << header
                      << "'; el bloque explicito gobierna pero se descarta"
                      << std::endl;
        }

        bool typeSeen = false;
        bool validType = false;
        Gameplay2DAbilityFieldMask seenFields = 0u;

        while (i < total)
        {
            const std::string& raw = lines[i];
            const std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed))
            {
                i++;
                continue;
            }
            if (indentLevel(raw) <= blockIndent)
                break;

            if (trimmed == "adquisicion:" ||
                trimmed == "acquisition:")
            {
                parseAbilityAcquisitionBlock(
                    lines, i, total,
                    "Uso '" + use.id + "'",
                    use.overrides.acquisition,
                    seenFields);
                continue;
            }

            if (trimmed == "coordinacion:" ||
                trimmed == "coordination:")
            {
                parseAbilityCoordinationBlock(
                    lines, i, total,
                    "Uso '" + use.id + "'",
                    use.overrides.coordination,
                    seenFields);
                continue;
            }

            std::string key, val;
            if (!splitKV(trimmed, key, val))
            {
                std::cerr << "[MapLoader] Uso '" << use.id
                          << "': linea desconocida '" << trimmed << "'"
                          << std::endl;
                i++;
                continue;
            }

            if (isKw(key, "tipo", "type"))
            {
                if (typeSeen)
                {
                    std::cerr << "[MapLoader] Uso '" << use.id
                              << "': tipo duplicado; se conserva el primero"
                              << std::endl;
                }
                else
                {
                    typeSeen = true;
                    validType = parseBareAbilityId(val, use.abilityId);
                    if (!validType)
                        std::cerr << "[MapLoader] Uso '" << use.id
                                  << "': referencia de tipo invalida '"
                                  << val << "'" << std::endl;
                }
            }
            else if (isKw(key, "alcance", "range"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::Range,
                        "Uso '" + use.id + "' / alcance"))
                {
                    float parsed = 0.0f;
                    if (parseAbilityNonNegativeFloat(
                            val, "Uso '" + use.id + "' / alcance", parsed))
                        use.overrides.range = parsed;
                }
            }
            else if (isKw(key, "cadencia", "fire_rate"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::FireRate,
                        "Uso '" + use.id + "' / cadencia"))
                {
                    float parsed = 0.0f;
                    if (parseAbilityNonNegativeFloat(
                            val, "Uso '" + use.id + "' / cadencia", parsed))
                        use.overrides.fireRate = parsed;
                }
            }
            else if (isKw(key, "proyectil", "projectile"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::Projectile,
                        "Uso '" + use.id + "' / proyectil"))
                {
                    std::string projectileId;
                    if (parseBareAbilityId(val, projectileId))
                        use.overrides.projectile = projectileId;
                    else
                        std::cerr << "[MapLoader] Uso '" << use.id
                                  << "': referencia de proyectil invalida '"
                                  << val << "'" << std::endl;
                }
            }
            else if (isKw(key, "daño_directo", "direct_damage") ||
                     key == "dano_directo")
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::DirectDamage,
                        "Uso '" + use.id + "' / daño_directo"))
                {
                    int parsed = 0;
                    if (parseAbilityPositiveInt(
                            val, "Uso '" + use.id + "' / daño_directo",
                            parsed))
                    {
                        use.overrides.directDamage = parsed;
                    }
                }
            }
            else if (isKw(key, "preferencia", "preference"))
            {
                if (claimAbilityField(
                        seenFields, Gameplay2DAbilityField::Preference,
                        "Uso '" + use.id + "' / preferencia"))
                {
                    Gameplay2DTargetPreference parsed;
                    if (parsePublicAbilityPreference(val, parsed))
                        use.overrides.preference = parsed;
                    else
                        std::cerr << "[MapLoader] Uso '" << use.id
                                  << "': preferencia publica desconocida '"
                                  << val << "'; se hereda la definicion"
                                  << std::endl;
                }
            }
            else
            {
                std::cerr << "[MapLoader] Uso '" << use.id
                          << "': propiedad desconocida '" << key << "'"
                          << std::endl;
            }

            i++;
        }

        if (!validId || !typeSeen || !validType)
        {
            if (!typeSeen)
                std::cerr << "[MapLoader] Uso '" << use.id
                          << "': falta tipo; el bloque se descarta"
                          << std::endl;
            return;
        }

        const auto duplicate = std::find_if(
            abilityUses.begin(), abilityUses.end(),
            [&](const Gameplay2DAbilityUseDef& current) {
                return current.id == use.id;
            });
        if (duplicate != abilityUses.end())
        {
            std::cerr << "[MapLoader] " << ownerKind << " '" << ownerName
                      << "': uso duplicado '" << use.id
                      << "'; se conserva el primero" << std::endl;
            return;
        }

        abilityUses.push_back(use);
    }

    // ════════════════════════════════════════════════════════════════════════
    // Parser: Torre — primer owner de usos de habilidad
    // ════════════════════════════════════════════════════════════════════════
    //
    // Los cuatro campos planos de ataque siguen entrando como dialecto legacy.
    // resolveTowerAbilityUses los materializa después en el mismo modelo.

    static void parseTowerBlock(const std::vector<std::string>& lines,
        int& i, int total, MapData& data)
    {
        std::string header = trim(lines[i]);
        const size_t towerIndent = indentLevel(lines[i]);
        i++;

        MapTowerDef tower;
        // "torre " = 6 chars, "tower " = 6 chars
        size_t skip = 6;
        parseEntityHeader(header, skip, tower.tile, tower.name,
            tower.r, tower.g, tower.b);

        while (i < total)
        {
            const std::string& raw = lines[i];
            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }
            if (indentLevel(raw) <= towerIndent) break;

            std::string key, val;

            if (startsWith(trimmed, "habilidad ") ||
                startsWith(trimmed, "ability "))
            {
                parseAbilityUseBlock(
                    lines, i, total,
                    "Torre", tower.name,
                    tower.abilityUses,
                    tower.explicitAbilityBlockSeen);
                continue;
            }

            // Evento: al_colocar/on_place
            if (trimmed == "al_colocar:" || trimmed == "on_place:")
            {
                size_t evIndent = indentLevel(raw);
                i++;
                parseEventBlock(lines, i, total, evIndent, tower.onPlace);
                continue;
            }

            // Evento: al_destruir/on_destroy
            if (trimmed == "al_destruir:" || trimmed == "on_destroy:")
            {
                size_t evIndent = indentLevel(raw);
                i++;
                parseEventBlock(lines, i, total, evIndent, tower.onDestroy);
                continue;
            }

            // Propiedades
            if (splitKV(trimmed, key, val))
            {
                if (isKw(key, "sprite", "sprite"))
                {
                    tower.spritePath = extractQuoted(trimmed);
                    if (tower.spritePath.empty()) tower.spritePath = val;
                }
                else if (isKw(key, "costo", "cost"))
                {
                    try { tower.cost = std::stoi(val); }
                    catch (...) {}
                }
                else if (key == "hp")
                {
                    try { tower.hp = std::stoi(val); }
                    catch (...) {}
                }
                else if (isKw(key, "rango", "range"))
                {
                    tower.legacyAbility.authoredFields |=
                        gameplay2DAbilityFieldBit(
                            Gameplay2DAbilityField::Range);
                    parseAbilityNonNegativeFloat(
                        val, "Torre '" + tower.name + "' / rango legacy",
                        tower.legacyAbility.values.range);
                }
                else if (isKw(key, "cadencia", "fire_rate"))
                {
                    tower.legacyAbility.authoredFields |=
                        gameplay2DAbilityFieldBit(
                            Gameplay2DAbilityField::FireRate);
                    parseAbilityNonNegativeFloat(
                        val, "Torre '" + tower.name + "' / cadencia legacy",
                        tower.legacyAbility.values.fireRate);
                }
                else if (isKw(key, "proyectil", "projectile"))
                {
                    tower.legacyAbility.authoredFields |=
                        gameplay2DAbilityFieldBit(
                            Gameplay2DAbilityField::Projectile);
                    tower.legacyAbility.values.projectile =
                        extractQuoted(trimmed);
                    if (tower.legacyAbility.values.projectile.empty())
                        tower.legacyAbility.values.projectile = val;
                }
                else if (isKw(key, "objetivo", "target"))
                {
                    tower.legacyAbility.authoredFields |=
                        gameplay2DAbilityFieldBit(
                            Gameplay2DAbilityField::Preference);
                    if (!parseLegacyTowerPreference(
                            val, tower.legacyAbility.values.preference))
                        std::cout << "[MapLoader] Torre: objetivo desconocido '"
                        << val << "' (usando cercano)" << std::endl;
                }
                else if (isKw(key, "donde", "where"))
                {
                    tower.whereSlot = val;
                }
                else if (isKw(
                             key,
                             "obstruye_locomocion",
                             "obstructs_locomotion"))
                {
                    tower.obstructsLocomotion = parseBool(
                        val, false,
                        "obstruye_locomocion/obstructs_locomotion");
                }
                else
                    std::cout << "[MapLoader] Torre prop desconocida: "
                    << key << std::endl;
            }

            i++;
        }

        data.towers.push_back(tower);
        std::cout << "[MapLoader] Torre '" << tower.tile << "' = " << tower.name
            << " (HP:" << tower.hp << " "
            << tower.abilityUses.size() << " usos explicitos, "
            << tower.onPlace.size() << " al_colocar, "
            << tower.onDestroy.size() << " al_destruir)" << std::endl;
    }

    static void resolveOwnerAbilityUses(
        const std::string& ownerKind,
        const std::string& ownerName,
        const std::vector<Gameplay2DAbilityDef>& definitions,
        std::vector<Gameplay2DAbilityUseDef>& abilityUses,
        const Gameplay2DLegacyAbilitySource& legacyAbility,
        bool explicitAbilityBlockSeen)
    {
        Gameplay2DAbilityResolution resolution =
            resolveGameplay2DAbilityUses(
                definitions,
                abilityUses,
                legacyAbility,
                explicitAbilityBlockSeen);

        if (resolution.mixedLegacyAndExplicit)
        {
            std::cerr << "[MapLoader] " << ownerKind << " '" << ownerName
                      << "': mezcla campos legacy con usos explicitos; "
                         "gobiernan los usos explicitos"
                      << std::endl;
        }

        for (const auto& missing : resolution.missingAbilityIds)
        {
            std::cerr << "[MapLoader] " << ownerKind << " '" << ownerName
                      << "': habilidad referenciada inexistente '"
                      << missing << "'; se omite el uso"
                      << std::endl;
        }

        for (const auto& conflict :
             resolution.conflictingEffectUseIds)
        {
            std::cerr << "[MapLoader] " << ownerKind << " '" << ownerName
                      << "': uso '" << conflict
                      << "' hereda proyectil y daño_directo; "
                         "se omite el uso"
                      << std::endl;
        }

        abilityUses = std::move(resolution.uses);
    }

    static void resolveEnemyAbilityUses(MapData& data)
    {
        for (auto& enemy : data.enemies)
        {
            resolveOwnerAbilityUses(
                "Enemigo", enemy.name,
                data.abilities,
                enemy.abilityUses,
                enemy.legacyAbility,
                enemy.explicitAbilityBlockSeen);
        }
    }

    static void resolveTowerAbilityUses(MapData& data)
    {
        for (auto& tower : data.towers)
        {
            resolveOwnerAbilityUses(
                "Torre", tower.name,
                data.abilities,
                tower.abilityUses,
                tower.legacyAbility,
                tower.explicitAbilityBlockSeen);
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // Parser: Item
    // ════════════════════════════════════════════════════════════════════════

    static void parseItemBlock(const std::vector<std::string>& lines,
                               int& i, int total, MapData& data)
    {
        std::string header = trim(lines[i]);
        i++;

        MapItemDef item;
        // "item " = 5 chars, "objeto " = 7 chars
        size_t skip = startsWith(header, "objeto") ? 7 : 5;
        parseEntityHeader(header, skip, item.tile, item.name,
                          item.r, item.g, item.b);

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            std::string key, val;

            // Evento: al_recoger/on_pickup
            if (trimmed == "al_recoger:" || trimmed == "on_pickup:")
            {
                size_t evIndent = indentLevel(raw);
                i++;
                parseEventBlock(lines, i, total, evIndent, item.onPickup);
                continue;
            }

            // Evento: al_usar/on_use
            if (trimmed == "al_usar:" || trimmed == "on_use:")
            {
                size_t evIndent = indentLevel(raw);
                i++;
                parseEventBlock(lines, i, total, evIndent, item.onUse);
                continue;
            }

            // Propiedades
            if (splitKV(trimmed, key, val))
            {
                if (isKw(key, "sprite", "sprite"))
                { item.spritePath = extractQuoted(trimmed);
                  if (item.spritePath.empty()) item.spritePath = val; }
                else if (isKw(key, "tipo", "type"))
                {
                    std::string tl = val;
                    std::transform(tl.begin(), tl.end(), tl.begin(), ::tolower);
                    item.inventory = (tl == "inventario" || tl == "inventory");
                }
                else
                    std::cout << "[MapLoader] Item prop desconocida: "
                              << key << std::endl;
            }

            i++;
        }

        data.items.push_back(item);
        std::cout << "[MapLoader] Item '" << item.tile << "' = " << item.name
                  << " (" << item.onPickup.size() << " acciones al_recoger)"
                  << std::endl;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Marcador espacial neutral
    // ════════════════════════════════════════════════════════════════════════

    static void parseSpatialMarkerBlock(
        const std::vector<std::string>& lines,
        int& i, int total, MapData& data)
    {
        const int headerLine = i + 1;
        std::string header = trim(lines[i]);
        i++;

        size_t sp = header.find(' ');
        std::string id;
        if (sp != std::string::npos)
        {
            id = trim(header.substr(sp + 1));
            if (!id.empty() && id.back() == ':')
                id.pop_back();
            id = trim(id);
        }

        const bool validId = !id.empty() &&
            id.find_first_of(" \t\r\n") == std::string::npos;

        MapSpatialMarkerDef marker;
        marker.id = id;
        bool rectangleSeen = false;
        bool rectangleValid = false;

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string t = trim(raw);
            if (t.empty() || isComment(t)) { i++; continue; }

            std::string key, val;
            if (!splitKV(t, key, val))
            {
                std::cout << "[MapLoader] Marcador '" << id
                          << "' linea ignorada: " << t << std::endl;
                i++;
                continue;
            }

            if (key == "rectangulo" || key == "rectángulo" ||
                key == "rectangle")
            {
                if (rectangleSeen)
                {
                    std::cout << "[MapLoader] Marcador '" << id
                              << "': rectangulo/rectangle duplicado; "
                              << "se conserva el primero." << std::endl;
                    i++;
                    continue;
                }

                rectangleSeen = true;
                std::istringstream iss(val);
                char comma1 = 0, comma2 = 0, comma3 = 0;
                float x = 0, y = 0, width = 0, height = 0;

                const bool parsed =
                    static_cast<bool>(iss >> x >> comma1 >> y
                                          >> comma2 >> width
                                          >> comma3 >> height);
                iss >> std::ws;
                const bool consumedAll = iss.eof();

                rectangleValid = parsed && consumedAll &&
                    comma1 == ',' && comma2 == ',' && comma3 == ',' &&
                    std::isfinite(x) && std::isfinite(y) &&
                    std::isfinite(width) && std::isfinite(height) &&
                    width > 0.0f && height > 0.0f;

                if (rectangleValid)
                {
                    marker.x = x;
                    marker.y = y;
                    marker.width = width;
                    marker.height = height;
                }
                else
                {
                    std::cout << "[MapLoader] Marcador '" << id
                              << "': rectangulo/rectangle invalido; se esperan "
                              << "x, y, ancho, alto finitos con tamano positivo."
                              << std::endl;
                }
            }
            else
            {
                std::cout << "[MapLoader] Marcador '" << id
                          << "' prop desconocida: " << key << std::endl;
            }

            i++;
        }

        if (!validId)
        {
            std::cout << "[MapLoader] Linea " << headerLine
                      << ": marcador descartado; el id debe ser un token "
                      << "no vacio y sin espacios." << std::endl;
            return;
        }

        if (!rectangleSeen || !rectangleValid)
        {
            std::cout << "[MapLoader] Marcador '" << id
                      << "' descartado; falta un rectangulo valido."
                      << std::endl;
            return;
        }

        const auto duplicate = std::find_if(
            data.spatialMarkers.begin(), data.spatialMarkers.end(),
            [&](const MapSpatialMarkerDef& other) {
                return other.id == marker.id;
            });
        if (duplicate != data.spatialMarkers.end())
        {
            std::cout << "[MapLoader] Marcador duplicado '" << marker.id
                      << "' descartado; se conserva la primera declaracion."
                      << std::endl;
            return;
        }

        data.spatialMarkers.push_back(marker);
        std::cout << "[MapLoader] Marcador '" << marker.id << "' rect=("
                  << marker.x << ", " << marker.y << ", "
                  << marker.width << ", " << marker.height << ")"
                  << std::endl;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Trigger
    // ════════════════════════════════════════════════════════════════════════

    static void parseTriggerBlock(const std::vector<std::string>& lines,
                                  int& i, int total, MapData& data)
    {
        std::string header = trim(lines[i]);
        i++;

        MapTriggerDef trigger;
        // "trigger " = 8 chars, "zona " = 5 chars
        size_t skip = startsWith(header, "trigger") ? 8 : 5;

        // Trigger puede no tener nombre ni color: "trigger T:"
        size_t pos = skip;
        while (pos < header.size() && header[pos] == ' ') pos++;
        if (pos < header.size() && header[pos] >= 'A' && header[pos] <= 'Z')
            trigger.tile = header[pos];

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            std::string key, val;

            // Evento: al_entrar/on_enter
            if (trimmed == "al_entrar:" || trimmed == "on_enter:")
            {
                size_t evIndent = indentLevel(raw);
                i++;
                parseEventBlock(lines, i, total, evIndent, trigger.onEnter);
                continue;
            }

            // Propiedades
            if (splitKV(trimmed, key, val))
            {
                if (key == "tamano" || key == "tamaño" || key == "size")
                {
                    // "96, 96" o "96"
                    std::istringstream iss(val);
                    iss >> trigger.width;
                    char comma;
                    if (iss >> comma >> trigger.height) {}
                    else trigger.height = trigger.width;
                }
                else if (isKw(key, "una_vez", "once"))
                { trigger.once = parseBool(val, false, "una_vez/once"); }
                else if (isKw(key, "condicion", "condition"))
                {
                    trigger.condition = val;
                    // Parsear "variable == valor"
                    std::istringstream iss(val);
                    iss >> trigger.condVar >> trigger.condOp >> trigger.condVal;
                }
                else
                    std::cout << "[MapLoader] Trigger prop desconocida: "
                              << key << std::endl;
            }

            i++;
        }

        data.triggers.push_back(trigger);
        std::cout << "[MapLoader] Trigger '" << trigger.tile << "' ("
                  << trigger.width << "x" << trigger.height
                  << (trigger.once ? " una_vez" : "")
                  << " " << trigger.onEnter.size() << " acciones)"
                  << std::endl;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Proyectil
    // ════════════════════════════════════════════════════════════════════════

    static void parseProjectileBlock(const std::vector<std::string>& lines,
                                      int& i, int total, MapData& data)
    {
        std::string header = trim(lines[i]);
        i++;

        MapProjectileDef proj;
        // "proyectil " = 10 chars, "projectile " = 11 chars
        size_t skip = startsWith(header, "proyectil") ? 10 : 11;
        parseEntityHeader(header, skip, proj.tile, proj.name,
                          proj.r, proj.g, proj.b);

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            std::string key, val;
            if (splitKV(trimmed, key, val))
            {
                if (isKw(key, "sprite", "sprite"))
                { proj.spritePath = extractQuoted(trimmed);
                  if (proj.spritePath.empty()) proj.spritePath = val; }
                else if (isKw(key, "velocidad", "speed"))
                { try { proj.speed = std::stof(val); } catch (...) {} }
                else if (isKw(key, "daño", "damage") || key == "dano")
                { try { proj.damage = std::stoi(val); } catch (...) {} }
                else if (key == "tamano" || key == "tamaño" || key == "size")
                { try { proj.size = std::stof(val); } catch (...) {} }
                else if (isKw(key, "tiempo_vida", "lifetime"))
                { try { proj.lifetime = std::stof(val); } catch (...) {} }
                else if (isKw(key, "atraviesa_paredes", "pierce_walls"))
                { proj.pierceWalls = parseBool(val, false, "atraviesa_paredes/pierce_walls"); }
                else if (isKw(key, "gravedad", "gravity"))
                { proj.hasGravity = parseBool(val, false, "gravedad/gravity"); }
                else
                    std::cout << "[MapLoader] Proyectil prop desconocida: "
                              << key << std::endl;
            }

            i++;
        }

        data.projectiles.push_back(proj);
        std::cout << "[MapLoader] Proyectil '" << proj.tile << "' = " << proj.name
                  << " (SPD:" << proj.speed << " DMG:" << proj.damage
                  << " SIZE:" << proj.size << " LIFE:" << proj.lifetime
                  << ")" << std::endl;
    }

    // ── Ruta / Waypoints ──────────────────────────────────────────────────

    static void parseRouteBlock(const std::vector<std::string>& lines,
        int& i, int total, MapData& data)
    {
        // Header: "ruta nombre:" o "route nombre:"
        std::string header = trim(lines[i]);
        // Quitar "ruta " / "route "
        size_t sp = header.find(' ');
        if (sp == std::string::npos) { i++; return; }
        std::string rest = header.substr(sp + 1);
        // Quitar ":" final
        if (!rest.empty() && rest.back() == ':')
            rest.pop_back();
        rest = trim(rest);

        MapRoute route;
        route.name = rest;
        bool hasExplicitTraversal = false;

        i++;
        while (i < total)
        {
            std::string raw = lines[i];
            if (indentLevel(raw) < 2 && !trim(raw).empty())
                break;

            std::string t = trim(raw);
            if (t.empty() || isComment(t)) { i++; continue; }

            // loop: true/false
            if (startsWith(t, "loop:") || startsWith(t, "bucle:"))
            {
                std::string val = trim(t.substr(t.find(':') + 1));
                bool legacyLoop = parseBool(val, false, "loop/bucle");
                if (!hasExplicitTraversal)
                {
                    route.traversal = legacyLoop
                        ? MapRouteTraversal::Loop
                        : MapRouteTraversal::PingPong;
                }
                i++; continue;
            }

            // recorrido: ciclo | ida_vuelta | una_vez
            // traversal: loop | ping_pong | once
            if (startsWith(t, "recorrido:") || startsWith(t, "traversal:"))
            {
                std::string val = trim(t.substr(t.find(':') + 1));
                std::transform(val.begin(), val.end(), val.begin(), ::tolower);

                bool valid = true;
                if (val == "ciclo" || val == "loop")
                    route.traversal = MapRouteTraversal::Loop;
                else if (val == "ida_vuelta" || val == "ping_pong" ||
                         val == "pingpong")
                    route.traversal = MapRouteTraversal::PingPong;
                else if (val == "una_vez" || val == "once")
                    route.traversal = MapRouteTraversal::Once;
                else
                {
                    valid = false;
                    std::cout << "[MapLoader] Recorrido desconocido en ruta '"
                        << route.name << "': " << val << std::endl;
                }

                if (valid) hasExplicitTraversal = true;
                i++; continue;
            }

            // Punto: "COL, ROW"
            size_t comma = t.find(',');
            if (comma != std::string::npos)
            {
                int col = std::stoi(trim(t.substr(0, comma)));
                int row = std::stoi(trim(t.substr(comma + 1)));
                route.points.push_back({ col, row });
            }

            i++;
        }

        if (route.points.size() >= 2)
        {
            const char* traversalLabel = "(loop)";
            if (route.traversal == MapRouteTraversal::PingPong)
                traversalLabel = "(ida y vuelta)";
            else if (route.traversal == MapRouteTraversal::Once)
                traversalLabel = "(una vez)";

            std::cout << "[MapLoader] Ruta '" << route.name
                << "': " << route.points.size() << " puntos"
                << " " << traversalLabel
                << std::endl;
            data.routes.push_back(route);
        }
        else
        {
            std::cout << "[MapLoader] Ruta '" << route.name
                << "' ignorada: necesita al menos 2 puntos" << std::endl;
        }
    }

    // ── Oleadas / Waves ───────────────────────────────────────────────────

    static void parseWaveBlock(const std::vector<std::string>& lines,
        int& i, int total, MapData& data)
    {
        data.waveConfig.defined = true;
        size_t blockIndent = 2;  // Contenido indentado bajo "oleadas:"

        while (i < total)
        {
            std::string raw = lines[i];
            std::string t = trim(raw);
            if (t.empty() || isComment(t)) { i++; continue; }
            if (indentLevel(raw) < blockIndent && !t.empty()) break;

            // avance: eliminar/eliminate | tiempo/time
            if (startsWith(t, "avance:") || startsWith(t, "advance:"))
            {
                std::string val = trim(t.substr(t.find(':') + 1));
                data.waveConfig.advance = val;
                i++; continue;
            }

            // intervalo: N (para modo tiempo)
            if (startsWith(t, "intervalo:") || startsWith(t, "interval:"))
            {
                std::string val = trim(t.substr(t.find(':') + 1));
                try { data.waveConfig.interval = std::stof(val); }
                catch (...) {}
                i++; continue;
            }

            // oleada N: / wave N:
            if (startsWith(t, "oleada ") || startsWith(t, "wave "))
            {
                MapWaveDef wave;
                // Extraer número
                std::string rest = t.substr(t.find(' ') + 1);
                if (!rest.empty() && rest.back() == ':')
                    rest.pop_back();
                try { wave.number = std::stoi(trim(rest)); }
                catch (...) { wave.number = (int)data.waveConfig.waves.size() + 1; }

                i++;
                size_t waveIndent = 4; // Contenido indentado bajo "oleada N:"

                while (i < total)
                {
                    std::string wRaw = lines[i];
                    std::string wt = trim(wRaw);
                    if (wt.empty() || isComment(wt)) { i++; continue; }
                    if (indentLevel(wRaw) < waveIndent && !wt.empty()) break;

                    // tiempo/delay: N
                    if (startsWith(wt, "tiempo:") || startsWith(wt, "delay:"))
                    {
                        std::string val = trim(wt.substr(wt.find(':') + 1));
                        try { wave.delay = std::stof(val); }
                        catch (...) {}
                        i++; continue;
                    }

                    // spawnear/spawn: CHAR COUNT MODE [X, Y]
                    if (startsWith(wt, "spawnear:") || startsWith(wt, "spawn:"))
                    {
                        std::string val = trim(wt.substr(wt.find(':') + 1));
                        std::istringstream iss(val);
                        MapWaveSpawn sp;

                        std::string charStr;
                        iss >> charStr;
                        if (!charStr.empty()) sp.entityChar = charStr[0];

                        iss >> sp.count;

                        std::string mode;
                        iss >> mode;

                        if (mode == "aleatorio" || mode == "random")
                        {
                            sp.mode = "random";
                        }
                        else if (mode == "en" || mode == "at")
                        {
                            sp.mode = "at";
                            // Leer posición: X, Y
                            std::string posStr;
                            std::getline(iss, posStr);
                            posStr = trim(posStr);
                            size_t comma = posStr.find(',');
                            if (comma != std::string::npos)
                            {
                                try {
                                    sp.posX = std::stof(trim(posStr.substr(0, comma)));
                                    sp.posY = std::stof(trim(posStr.substr(comma + 1)));
                                }
                                catch (...) {}
                            }
                        }
                        else
                        {
                            sp.mode = "random"; // Default
                        }

                        wave.spawns.push_back(sp);
                        i++; continue;
                    }

                    // al_completar/on_complete:
                    if (wt == "al_completar:" || wt == "on_complete:")
                    {
                        i++;
                        size_t evIndent = indentLevel(wRaw);
                        parseEventBlock(lines, i, total, evIndent, wave.onComplete);
                        continue;
                    }

                    i++;
                }

                data.waveConfig.waves.push_back(wave);
                std::cout << "[MapLoader] Oleada " << wave.number
                    << ": " << wave.spawns.size() << " spawns"
                    << ", delay=" << wave.delay << "s"
                    << ", " << wave.onComplete.size() << " acciones al_completar"
                    << std::endl;
                continue;
            }

            i++;
        }

        std::cout << "[MapLoader] Oleadas: " << data.waveConfig.waves.size()
            << " oleadas, avance=" << data.waveConfig.advance << std::endl;
    }

    // ════════════════════════════════════════════════════════════════════════
    // Parser: Tile custom
    // ════════════════════════════════════════════════════════════════════════
    // Formato:
    //   tile ~ "assets/agua.png":          (con bloque de propiedades)
    //     tipo: daño
    //     daño: 1
    //   tile . "assets/pasto.png"          (single line, solo textura)

    static void parseTileDefBlock(const std::vector<std::string>& lines,
                                  int& i, int total, MapData& data)
    {
        std::string header = trim(lines[i]);
        i++;

        MapTileDef tileDef;

        // Encontrar el carácter del tile después de "tile " o "casilla "
        size_t skip = startsWith(header, "casilla") ? 8 : 5;
        size_t pos = skip;
        while (pos < header.size() && header[pos] == ' ') pos++;
        if (pos < header.size())
            tileDef.tile = header[pos];

        // Sprite path
        tileDef.spritePath = extractQuoted(header);

        // ¿Tiene bloque de propiedades? (termina con ':')
        bool hasBlock = !header.empty() && header.back() == ':';

        if (hasBlock)
        {
            while (i < total)
            {
                const std::string& raw = lines[i];
                if (!isIndented(raw)) break;

                std::string trimmed = trim(raw);
                if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

                std::string key, val;
                if (splitKV(trimmed, key, val))
                {
                    if (isKw(key, "tipo", "type"))
                    {
                        std::string tl = val;
                        std::transform(tl.begin(), tl.end(), tl.begin(), ::tolower);
                        if (tl == "daño"        || tl == "damage" || tl == "dano")
                        { tileDef.type = MapTileType::Damage; tileDef.walkable = true; }
                        else if (tl == "resbaloso"   || tl == "slippery")
                        { tileDef.type = MapTileType::Slippery; tileDef.walkable = true; }
                        else if (tl == "lento"       || tl == "slow")
                        { tileDef.type = MapTileType::Slow; tileDef.walkable = true; }
                        else if (tl == "plataforma"  || tl == "platform")
                        { tileDef.type = MapTileType::Platform; tileDef.walkable = true; }
                        else if (tl == "checkpoint")
                        { tileDef.type = MapTileType::Checkpoint; tileDef.walkable = true; }
                        else if (tl == "puerta"      || tl == "door")
                        { tileDef.type = MapTileType::Door; tileDef.walkable = true; }
                        else if (tl == "empujable" || tl == "pushable")
                        {tileDef.type = MapTileType::Pushable; tileDef.walkable = false;}
                        else if (tl == "suelo"       || tl == "floor")
                        { tileDef.type = MapTileType::Floor; tileDef.walkable = true; }
                        else if (tl == "solido"      || tl == "solid" || tl == "pared" || tl == "wall")
                        { tileDef.type = MapTileType::Solid; tileDef.walkable = false; }
                        else if (tl == "base")
                        { tileDef.type = MapTileType::Base; tileDef.walkable = false; }
                        else if (tl == "spawn")
                        { tileDef.type = MapTileType::Spawn; tileDef.walkable = true; }
                        else if (tl == "slot_torre" || tl == "tower_slot")
                        { tileDef.type = MapTileType::TowerSlot; tileDef.walkable = true; }
                    }
                    else if (key == "hp")
                    { try { tileDef.hp = std::stoi(val); } catch (...) {} }
                    else if (trimmed == "al_destruir:" || trimmed == "on_destroy:")
                    {
                        size_t evIndent = indentLevel(raw);
                        i++;
                        parseEventBlock(lines, i, total, evIndent, tileDef.onDestroy);
                        continue; // parseEventBlock ya avanzó i
                    }
                    else if (isKw(key, "daño", "damage") || key == "dano")
                    { try { tileDef.damage = std::stoi(val); } catch (...) {} }
                    else if (isKw(key, "velocidad", "speed"))
                    { try { tileDef.speedMult = std::stof(val); } catch (...) {} }
                    else if (isKw(key, "friccion", "friction"))
                    { try { tileDef.friction = std::stof(val); } catch (...) {} }
                    else if (isKw(key, "destino", "destination"))
                    { tileDef.destMap = extractQuoted(trimmed);
                      if (tileDef.destMap.empty()) tileDef.destMap = val; }
                    else if (isKw(key, "posicion", "position"))
                    {
                        std::istringstream iss(val);
                        char comma;
                        iss >> tileDef.destX >> comma >> tileDef.destY;
                    }
                    else if (isKw(key, "condicion", "condition"))
                    { tileDef.condition = val; }
                    else if (isKw(key, "cerrada", "locked_msg"))
                    { tileDef.lockedMsg = extractQuoted(trimmed);
                      if (tileDef.lockedMsg.empty()) tileDef.lockedMsg = val; }
                    else if (isKw(key, "sonido", "sound"))
                    { tileDef.sound = extractQuoted(trimmed);
                      if (tileDef.sound.empty()) tileDef.sound = val; }
                }

                i++;
            }
        }
        else
        {
            // Single line sin propiedades: suelo con textura o pared con textura
            // Si el char es '#' o '=', es sólido; si es '.', es suelo
            if (tileDef.tile == '#' || tileDef.tile == '=')
            { tileDef.type = MapTileType::Solid; tileDef.walkable = false; }
            else
            { tileDef.type = MapTileType::Floor; tileDef.walkable = true; }
        }

        data.tileDefs.push_back(tileDef);
        std::cout << "[MapLoader] Tile '" << tileDef.tile << "' = "
                  << tileDef.spritePath << std::endl;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: Jugador
    // ════════════════════════════════════════════════════════════════════════

    static void parsePlayerBlock(const std::vector<std::string>& lines,
                                 int& i, int total, MapData& data)
    {
        data.playerDef.defined = true;

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            std::string key, val;
            if (splitKV(trimmed, key, val))
            {
                if (isKw(key, "sprite", "sprite"))
                { data.playerDef.spritePath = extractQuoted(trimmed);
                  if (data.playerDef.spritePath.empty()) data.playerDef.spritePath = val; }
                else if (key == "fps")
                {
                    float parsed = 0.0f;
                    if (parseFloatStrict(val, parsed) == NumParse::Ok &&
                        parsed > 0.0f && parsed <= 240.0f)
                    {
                        data.playerDef.animationFps = parsed;
                        data.playerDef.animationFpsExplicit = true;
                    }
                    else
                    {
                        data.playerDef.animationFps = 8.0f;
                        data.playerDef.animationFpsExplicit = false;
                        std::cerr << "[MapLoader] Jugador fps invalido '"
                                  << val << "'; usando default 8"
                                  << std::endl;
                    }
                }
                else if (key == "hp")
                { try { data.playerDef.hp = std::stoi(val); } catch (...) {} }
                else if (isKw(key, "daño_contacto", "contact_damage") || key == "dano_contacto")
                { data.playerDef.contactDamage = parseBool(val, false, "daño_contacto/contact_damage"); }
                else if (isKw(key, "invencible", "invincible"))
                { try { data.playerDef.invincibleTime = std::stof(val); } catch (...) {} }
                else if (isKw(key, "puede_atacar", "can_attack"))
                { data.playerDef.canAttack = parseBool(val, false, "puede_atacar/can_attack"); }
                else if (isKw(key, "ataque_rango", "attack_range"))
                { try { data.playerDef.attackRange = std::stof(val); } catch (...) {} }
                else if (isKw(key, "ataque_daño", "attack_damage") || key == "ataque_dano")
                { try { data.playerDef.attackDamage = std::stoi(val); } catch (...) {} }
                else if (key == "ataque_cooldown" || key == "attack_cooldown")
                { try { data.playerDef.attackCooldown = std::stof(val); } catch (...) {} }
                else if (isKw(key, "ataque_tipo", "attack_type"))
                { data.playerDef.attackType = val; }
                else if (isKw(key, "ataque_sprite", "attack_sprite"))
                { data.playerDef.attackSprite = extractQuoted(trimmed);
                  if (data.playerDef.attackSprite.empty()) data.playerDef.attackSprite = val; }
                else if (isKw(key, "proyectil", "projectile"))
                { data.playerDef.projectileTile = extractQuoted(trimmed);
                  if (data.playerDef.projectileTile.empty()) data.playerDef.projectileTile = val; }
                else if (isKw(key, "cadencia", "fire_rate"))
                { try { data.playerDef.fireRate = std::stof(val); } catch (...) {} }
                else
                    std::cout << "[MapLoader] Jugador prop desconocida: "
                              << key << std::endl;
            }

            i++;
        }

        std::cout << "[MapLoader] Jugador: HP=" << data.playerDef.hp
                  << " ataque=" << (data.playerDef.canAttack ? "si" : "no")
                  << std::endl;
    }


    // ════════════════════════════════════════════════════════════════════════
    // Parser: HUD
    // ════════════════════════════════════════════════════════════════════════

    static void parseHUDBlock(const std::vector<std::string>& lines,
                              int& i, int total, MapData& data)
    {
        data.hudDef.defined = true;

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            std::string key, val;
            if (splitKV(trimmed, key, val))
            {
                // FOLD-PARTIDO: `visible`/`true`/`si` son vocabulario del motor
                // y la ranura es PURA, asi que `Visible` es la misma palabra.
                // Se pliega UNA vez y las cuatro claves la comparten: escrito
                // cuatro veces se desincroniza (hermano de §1.6).
                //
                // NOTA de auditoria, fichada aparte y NO tocada aca: esta lista
                // de afirmativos es propia y NO pasa por `parseBoolStrict`, o
                // sea que `1`/`yes`/`on` -que el resto del motor acepta- no
                // valen, y un valor no reconocido cae a `false` sin voz. Eso es
                // §1.45 sobreviviendo en un rincon, y arreglarlo aca habria
                // cambiado que acepta el `.eclmap` sin que nadie lo firmara.
                const std::string vHud = eclips::vocabToLower(val);
                const bool visible =
                    (vHud == "visible" || vHud == "true" || vHud == "si");
                if (key == "hp")
                { data.hudDef.showHP = visible; }
                else if (isKw(key, "hp_base", "base_hp"))
                { data.hudDef.showBaseHP = visible; }
                else if (isKw(key, "inventario", "inventory"))
                { data.hudDef.showInventory = visible; }
                else if (isKw(key, "oleada", "wave"))
                { data.hudDef.showWave = visible; }
                else if (key == "score")
                {
                    // score: "Puntos"  → variable = Puntos, label = "Puntos"
                    // score: variable "Label"
                    data.hudDef.scoreVar = val;
                    std::string q = extractQuoted(trimmed);
                    if (!q.empty()) { data.hudDef.scoreLabel = q; data.hudDef.scoreVar = val; }
                    else data.hudDef.scoreLabel = val;
                }
                else if (isKw(key, "texto", "text"))
                { data.hudDef.fixedText = extractQuoted(trimmed);
                  if (data.hudDef.fixedText.empty()) data.hudDef.fixedText = val; }
                else if (isKw(key, "contador", "counter"))
                {
                    // contador: "monedas" "Monedas: {}"
                    size_t q1end = trimmed.find('"', trimmed.find('"') + 1);
                    const std::string counterVar = extractQuoted(trimmed);
                    const std::string counterFormat =
                        q1end == std::string::npos
                            ? std::string{}
                            : extractQuoted(trimmed, q1end + 1);

                    if (counterVar.empty() ||
                        !gameplay2DFormatHudCounter(counterFormat, "0"))
                    {
                        data.hudDef.counterVar.clear();
                        data.hudDef.counterFormat.clear();
                        std::cerr
                            << "[MapLoader] contador invalido: requiere una "
                               "variable y exactamente un placeholder '{}': "
                            << trimmed << std::endl;
                    }
                    else
                    {
                        data.hudDef.counterVar = counterVar;
                        data.hudDef.counterFormat = counterFormat;
                    }
                }
                else
                    std::cout << "[MapLoader] HUD prop desconocida: "
                              << key << std::endl;
            }

            i++;
        }

        std::cout << "[MapLoader] HUD:"
                  << (data.hudDef.showHP ? " HP" : "")
                  << (data.hudDef.showInventory ? " INV" : "")
                  << (data.hudDef.showWave ? " WAVE" : "")
                  << (!data.hudDef.scoreVar.empty() ? " SCORE" : "")
                  << (!data.hudDef.counterVar.empty() ? " COUNTER" : "")
                  << (!data.hudDef.fixedText.empty() ? " TEXT" : "")
                  << std::endl;
    }

    // ══════════════════════════════════════════════════════════════════════
    // parseSaveBlock — Política de save del nivel (Tier 4C.6)
    // ══════════════════════════════════════════════════════════════════════
    static void parseSaveBlock(const std::vector<std::string>& lines,
        int& i, int total, MapData& data)
    {
        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            std::string key, val;
            if (splitKV(trimmed, key, val))
            {
                if (isKw(key, "permitir", "allow"))
                {
                    std::string lower = val;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == "nunca" || lower == "never")
                        data.savePolicy.allow = SaveAllow::Never;
                    else if (lower == "solo_checkpoint" || lower == "checkpoint_only"
                        || lower == "checkpoint")
                        data.savePolicy.allow = SaveAllow::CheckpointOnly;
                    else if (lower == "siempre" || lower == "always")
                        data.savePolicy.allow = SaveAllow::Always;
                    else
                        std::cout << "[MapLoader] guardar.permitir desconocido: "
                        << val << std::endl;
                }
                else if (key == "slots")
                {
                    try { data.savePolicy.slotsVisible = std::stoi(val); }
                    catch (...) {}
                    if (data.savePolicy.slotsVisible < 0)  data.savePolicy.slotsVisible = 0;
                    if (data.savePolicy.slotsVisible > 10) data.savePolicy.slotsVisible = 10;
                }
                else if (isKw(key, "archivo", "file"))
                {
                    std::string path = val;
                    bool hasOpen = !path.empty() && path.front() == '"';
                    bool hasClose = path.size() >= 2 && path.back() == '"';
                    if (hasOpen && hasClose)
                    {
                        path = path.substr(1, path.size() - 2);
                        data.savePolicy.saveFile = path;
                    }
                    else if (hasOpen || hasClose)
                    {
                        std::cout << "[MapLoader] guardar.archivo: comillas mal "
                            << "balanceadas en " << val
                            << " - ignorado, usando archivo compartido."
                            << std::endl;
                        // Queda saveFile vacío → cae al archivo compartido
                    }
                    else
                    {
                        // Sin comillas, usar tal cual
                        data.savePolicy.saveFile = path;
                    }
                }
                else
                    std::cout << "[MapLoader] guardar prop desconocida: "
                    << key << std::endl;
            }

            i++;
        }

        const char* allowStr = "Never";
        if (data.savePolicy.allow == SaveAllow::CheckpointOnly) allowStr = "CheckpointOnly";
        else if (data.savePolicy.allow == SaveAllow::Always)    allowStr = "Always";

        std::cout << "[MapLoader] SavePolicy: allow=" << allowStr
            << " slots=" << data.savePolicy.slotsVisible;
        if (!data.savePolicy.saveFile.empty())
            std::cout << " archivo=" << data.savePolicy.saveFile;
        std::cout << std::endl;
    }

    // ══════════════════════════════════════════════════════════════════════
    // parseEndBlock — Pantalla de fin de nivel (Tier 4D)
    // ══════════════════════════════════════════════════════════════════════
    // Estructura:
    //   fin:
    //     victoria:
    //       estilo: estrellas
    //       texto: "..."
    //       criterios: {sub-bloque}
    //       colores: {sub-bloque}
    //       animacion_entrada: desde_arriba
    //       sonido_entrada: "..."
    //       siguiente: "..."
    //     derrota:
    //       {mismo formato}

    static void parseEndBlock(const std::vector<std::string>& lines,
        int& i, int total, MapData& data)
    {
        data.endConfig.defined = true;

        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            int curIndent = countIndent(raw);

            if (trimmed == "victoria:" || trimmed == "victory:")
            {
                data.endConfig.victory.defined = true;
                i++;
                parseEndPresetBlock(lines, i, total,
                    data.endConfig.victory, curIndent);
                continue;
            }
            if (trimmed == "derrota:" || trimmed == "defeat:")
            {
                data.endConfig.defeat.defined = true;
                i++;
                parseEndPresetBlock(lines, i, total,
                    data.endConfig.defeat, curIndent);
                continue;
            }

            std::cout << "[MapLoader] fin: prop desconocida: "
                << trimmed << std::endl;
            i++;
        }

        std::cout << "[MapLoader] LevelEndConfig:"
            << (data.endConfig.victory.defined ? " victoria" : "")
            << (data.endConfig.defeat.defined ? " derrota" : "")
            << std::endl;
    }

    // Parsea el contenido de un preset (victoria/derrota).
    // parentIndent = nivel del keyword "victoria:"/"derrota:".
    // Aceptamos líneas con indent ESTRICTAMENTE mayor.
    static void parseEndPresetBlock(const std::vector<std::string>& lines,
        int& i, int total,
        LevelEndPreset& preset, int parentIndent)
    {
        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            int curIndent = countIndent(raw);
            if (curIndent <= parentIndent) break;

            // Sub-bloques sin valor después de ":"
            if (trimmed == "colores:" || trimmed == "colors:")
            {
                i++;
                parseEndColorsBlock(lines, i, total, preset.colors, curIndent);
                continue;
            }
            if (trimmed == "criterios:" || trimmed == "criteria:")
            {
                i++;
                parseEndCriteriaBlock(lines, i, total,
                    preset.starCriteria, curIndent);
                continue;
            }

            if (trimmed == "mostrar:" || trimmed == "show:")
            {
                i++;
                parseEndShowBlock(lines, i, total,
                    preset.tallyFields, curIndent);
                continue;
            }

            // Keys simples
            std::string key, val;
            if (!splitKV(trimmed, key, val)) { i++; continue; }

            if (isKw(key, "estilo", "style"))
            {
                std::string lo = val;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                if (lo == "clasico" || lo == "basic" || lo == "classic")
                    preset.style = LevelEndStyle::Classic;
                else if (lo == "estrellas" || lo == "stars")
                    preset.style = LevelEndStyle::Stars;
                else if (lo == "tally" || lo == "puntaje")
                    preset.style = LevelEndStyle::Tally;
                else if (lo == "ko")
                    preset.style = LevelEndStyle::KO;
                else
                    std::cout << "[MapLoader] fin.estilo desconocido: "
                    << val << std::endl;
            }
            else if (isKw(key, "texto", "text"))
            {
                std::string q = extractQuoted(trimmed);
                preset.text = q.empty() ? val : q;
            }
            else if (isKw(key, "posicion", "position"))
            {
                std::string lo = val;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                if (lo == "centro" || lo == "center")
                    preset.position = LevelEndPosition::Center;
                else if (lo == "arriba" || lo == "top")
                    preset.position = LevelEndPosition::Top;
                else if (lo == "abajo" || lo == "bottom")
                    preset.position = LevelEndPosition::Bottom;
                else
                    std::cout << "[MapLoader] fin.posicion desconocida: "
                    << val << std::endl;
            }
            else if (isKw(key, "animacion_entrada", "entry_animation"))
            {
                std::string lo = val;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                if (lo == "desvanecer" || lo == "fade")
                    preset.animation = LevelEndAnimation::Fade;
                else if (lo == "desde_arriba" || lo == "from_top")
                    preset.animation = LevelEndAnimation::FromTop;
                else if (lo == "desde_abajo" || lo == "from_bottom")
                    preset.animation = LevelEndAnimation::FromBottom;
                else if (lo == "zoom")
                    preset.animation = LevelEndAnimation::Zoom;
                else if (lo == "ninguna" || lo == "none")
                    preset.animation = LevelEndAnimation::None;
                else
                    std::cout << "[MapLoader] fin.animacion_entrada desconocida: "
                    << val << std::endl;
            }
            else if (isKw(key, "sonido_entrada", "entry_sound"))
            {
                std::string q = extractQuoted(trimmed);
                preset.entrySound = q.empty() ? val : q;
            }
            else if (isKw(key, "duracion_entrada", "entry_duration"))
            {
                // Segundos. Clamp 0.05-3.0 (más rápido = teletransporte;
                // más lento = se siente roto).
                float d = 0.4f;
                try { d = std::stof(val); } catch (...) {
                    std::cout << "[MapLoader] fin.duracion_entrada inválida: "
                        << val << " (usando default 0.4)" << std::endl;
                }
                if (d < 0.05f) d = 0.05f;
                if (d > 3.0f)  d = 3.0f;
                preset.entryDuration = d;
            }
            else if (isKw(key, "siguiente", "next"))
            {
                std::string q = extractQuoted(trimmed);
                preset.nextMap = q.empty() ? val : q;
            }
            else
            {
                std::cout << "[MapLoader] fin.preset prop desconocida: "
                    << key << std::endl;
            }
            i++;
        }
    }

    // Sub-bloque colores:
    //   fondo: #001133
    //   texto: #FFFFFF
    //   acento: #FFD700
    //   boton: #1A2845
    //   boton_hover: #2D4A8F
    static void parseEndColorsBlock(const std::vector<std::string>& lines,
        int& i, int total,
        LevelEndColors& colors, int parentIndent)
    {
        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            int curIndent = countIndent(raw);
            if (curIndent <= parentIndent) break;

            std::string key, val;
            if (!splitKV(trimmed, key, val)) { i++; continue; }

            float r, g, b, a;
            if (!parseHexColor(val, r, g, b, a))
            {
                std::cout << "[MapLoader] fin.colores." << key
                    << ": color inválido '" << val
                    << "' (usar #RRGGBB o #RRGGBBAA)" << std::endl;
                i++; continue;
            }

            if (isKw(key, "fondo", "background"))
            {
                colors.bgR = r;     colors.bgG = g;     colors.bgB = b;     colors.bgA = a;
            }
            else if (isKw(key, "texto", "text"))
            {
                colors.textR = r;   colors.textG = g;   colors.textB = b;   colors.textA = a;
            }
            else if (isKw(key, "acento", "accent"))
            {
                colors.accentR = r; colors.accentG = g; colors.accentB = b; colors.accentA = a;
            }
            else if (isKw(key, "boton", "button"))
            {
                colors.btnR = r;    colors.btnG = g;    colors.btnB = b;    colors.btnA = a;
            }
            else if (isKw(key, "boton_hover", "button_hover"))
            {
                colors.btnHovR = r; colors.btnHovG = g; colors.btnHovB = b; colors.btnHovA = a;
            }
            else
                std::cout << "[MapLoader] fin.colores key desconocido: "
                << key << std::endl;

            i++;
        }
    }

    // Sub-bloque criterios (para estilo Stars):
    //   tres_estrellas: hp_base >= 18
    //   dos_estrellas: hp_base >= 10
    //   una_estrella: siempre
    static void parseEndCriteriaBlock(const std::vector<std::string>& lines,
        int& i, int total,
        std::vector<LevelEndStarCriterion>& out,
        int parentIndent)
    {
        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            int curIndent = countIndent(raw);
            if (curIndent <= parentIndent) break;

            std::string key, val;
            if (!splitKV(trimmed, key, val)) { i++; continue; }

            int stars = 0;
            if (key == "tres_estrellas" || key == "three_stars") stars = 3;
            else if (key == "dos_estrellas" || key == "two_stars")   stars = 2;
            else if (key == "una_estrella" || key == "one_star")    stars = 1;
            else
            {
                std::cout << "[MapLoader] fin.criterios key desconocido: "
                    << key << std::endl;
                i++; continue;
            }

            LevelEndStarCriterion c;
            c.stars = stars;
            parseCriterionExpr(val, c);
            out.push_back(c);

            i++;
        }
    }

    // Sub-bloque mostrar (para estilo Tally):
    //   "Oro sobrante": oro
    //   "HP restante": hp_base
    //   "Enemigos matados": enemigos_matados
    //
    // Formato por línea: "label": varName (label entre comillas,
    // varName sin comillas). El LevelEndState leerá varName via
    // varLookup en runtime y mostrará label + valor.
    static void parseEndShowBlock(const std::vector<std::string>& lines,
        int& i, int total,
        std::vector<std::pair<std::string, std::string>>& out,
        int parentIndent)
    {
        while (i < total)
        {
            const std::string& raw = lines[i];
            if (!isIndented(raw)) break;

            std::string trimmed = trim(raw);
            if (trimmed.empty() || isComment(trimmed)) { i++; continue; }

            int curIndent = countIndent(raw);
            if (curIndent <= parentIndent) break;

            // Extraer "label"
            std::string label = extractQuoted(trimmed);
            if (label.empty())
            {
                std::cout << "[MapLoader] fin.mostrar: falta \"label\" en: "
                    << trimmed << std::endl;
                i++; continue;
            }

            // Buscar ':' después de la segunda comilla
            size_t q1 = trimmed.find('"');
            size_t q2 = trimmed.find('"', q1 + 1);
            if (q2 == std::string::npos) { i++; continue; }

            size_t colonPos = trimmed.find(':', q2);
            if (colonPos == std::string::npos)
            {
                std::cout << "[MapLoader] fin.mostrar: falta ':' después del label: "
                    << trimmed << std::endl;
                i++; continue;
            }

            std::string varName = trim(trimmed.substr(colonPos + 1));
            if (varName.empty())
            {
                std::cout << "[MapLoader] fin.mostrar: varName vacío: "
                    << trimmed << std::endl;
                i++; continue;
            }

            out.emplace_back(label, varName);
            i++;
        }
    }

    // Parsea "hp_base >= 18" o "siempre"/"always".
    // Operadores soportados (orden importa: 2 chars antes que 1 char):
    //   >=, <=, ==, !=, >, 
    static void parseCriterionExpr(const std::string& expr,
        LevelEndStarCriterion& out)
    {
        std::string s = trim(expr);
        std::string lo = s;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);

        if (lo == "siempre" || lo == "always")
        {
            out.always = true;
            return;
        }

        size_t opPos = std::string::npos;
        size_t opLen = 0;

        for (const char* op : { ">=", "<=", "==", "!=" })
        {
            size_t p = s.find(op);
            if (p != std::string::npos)
            {
                opPos = p; opLen = 2; out.op = op; break;
            }
        }
        if (opPos == std::string::npos)
        {
            for (const char* op : { ">", "<" })
            {
                size_t p = s.find(op);
                if (p != std::string::npos)
                {
                    opPos = p; opLen = 1; out.op = op; break;
                }
            }
        }

        if (opPos == std::string::npos)
        {
            std::cout << "[MapLoader] criterio sin operador: '" << expr
                << "' (usa >=, <=, >, <, ==, !=, o 'siempre')"
                << std::endl;
            out.always = true;  // Fallback seguro
            return;
        }

        out.var = trim(s.substr(0, opPos));
        out.val = trim(s.substr(opPos + opLen));
    }

    // Parsea color hex #RRGGBB o #RRGGBBAA. Retorna false si inválido.
    static bool parseHexColor(const std::string& s,
        float& r, float& g, float& b, float& a)
    {
        if (s.empty() || s[0] != '#') return false;
        std::string hex = s.substr(1);
        if (hex.size() != 6 && hex.size() != 8) return false;

        auto fromHex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
            };

        auto byte = [&](int pos) -> int {
            int hi = fromHex(hex[pos]);
            int lo = fromHex(hex[pos + 1]);
            if (hi < 0 || lo < 0) return -1;
            return hi * 16 + lo;
            };

        int br = byte(0), bg = byte(2), bb = byte(4);
        if (br < 0 || bg < 0 || bb < 0) return false;
        r = br / 255.0f; g = bg / 255.0f; b = bb / 255.0f;
        a = 1.0f;

        if (hex.size() == 8)
        {
            int ba = byte(6);
            if (ba < 0) return false;
            a = ba / 255.0f;
        }
        return true;
    }

};

} // namespace eclips
