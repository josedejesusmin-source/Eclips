// ============================================================================
// Tilemap.h — Sistema de niveles basado en grids (v3 — Tiles Custom)
// ============================================================================
//
// Caracteres del mapa:
//   '#' = pared sólida     '=' = pared alternativa     'B' = caja
//   '.' = suelo            'P' = posición del jugador
//   '>' = salida (tile donde termina el nivel)
//   A-Z = NPCs genéricos (excepto B y P)
//   Cualquier otro char = tile custom (definido en .eclmap con tileDefs)
//
// Tiles custom se pasan como TileVisualInfo al llamar loadGrid().
// Tilemap NO depende de MapData — solo sabe chars, colores y solidez.
//
// Tamaño libre — la cámara sigue al jugador y solo renderiza tiles visibles.
//
// ============================================================================

#pragma once

#include "engine/core/Types.h"
#include "engine/render/Draw2D.h"

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace eclips
{
    // ── AABB ─────────────────────────────────────────────────────────────────
    struct TileAABB
    {
        f32 x, y, w, h;
        f32 left()   const { return x; }
        f32 right()  const { return x + w; }
        f32 top()    const { return y; }
        f32 bottom() const { return y + h; }

        bool overlaps(const TileAABB& o) const
        {
            return left() < o.right() && right() > o.left() &&
                   top() < o.bottom() && bottom() > o.top();
        }
    };

    // ── Tipos de tile base ────────────────────────────────────────────────────
    enum class TileType
    {
        Empty,      // '.'
        Wall,       // '#'
        WallAlt,    // '='
        Crate,      // 'B'
        Player,     // 'P'
        NPC,        // A-Z (excepto B,P)
        Exit,       // '>' — tile de salida
        Custom      // Tile definido por el creador en tileDefs
    };

    // ── Visual de un tile custom ──────────────────────────────────────────────
    //
    // Tilemap no sabe de MapTileDef ni de propiedades de gameplay.
    // Solo sabe: color, si es sólido, y si debe animarse (pulso).
    //
    struct TileVisualInfo
    {
        float r = 0.5f, g = 0.5f, b = 0.5f;
        bool  solid    = false;    // true = genera collider
        bool  animated = false;    // true = pulso visual (tiles de daño, etc.)
        bool  platform = false;   // true = sólida solo desde arriba (one-way)
    };

    // ── Posición de NPC genérica ─────────────────────────────────────────────
    struct TileNPCPos
    {
        f32  x, y;
        char tile;
    };

    // ── Info de NPC hardcodeada (backward compat) ────────────────────────────
    struct TileNPCSpawn
    {
        f32 x, y;
        std::string name;
        std::vector<std::string> lines;
        f32 r, g, b;
    };

    // ── Posición de tile de salida ───────────────────────────────────────────
    struct TileExitPos
    {
        f32 x, y;
    };


    // ════════════════════════════════════════════════════════════════════════
    // Tilemap
    // ════════════════════════════════════════════════════════════════════════

    class Tilemap
    {
    public:
        static constexpr f32 TILE_SIZE = 60.0f;
        static constexpr int DEFAULT_W = 32;
        static constexpr int DEFAULT_H = 18;

        Tilemap() = default;

        int  gridW()  const { return m_gridW; }
        int  gridH()  const { return m_gridH; }
        f32  worldW() const { return m_gridW * TILE_SIZE; }
        f32  worldH() const { return m_gridH * TILE_SIZE; }


        // ── loadGrid — Con soporte para tiles custom ──────────────────────────
        //
        // customVisuals: mapa de char → visual info. Si un char del grid tiene
        // entrada aquí, se trata como Custom (con collider si solid=true).
        // Si no tiene entrada, se usa charToTile() por defecto.
        //

        void loadGrid(const std::vector<std::string>& grid,
                       const std::map<char, TileVisualInfo>& customVisuals = {})
        {
            m_tiles.clear();
            m_rawChars.clear();
            m_colliders.clear();
            m_platformColliders.clear();
            m_npcPositions.clear();
            m_exitPositions.clear();
            m_customVisuals = customVisuals;
            m_playerStartX = 0;
            m_playerStartY = 0;

            m_gridH = (int)grid.size();
            m_gridW = 0;
            for (const auto& row : grid)
                if ((int)row.size() > m_gridW)
                    m_gridW = (int)row.size();

            if (m_gridW == 0 || m_gridH == 0)
            {
                std::cout << "[Tilemap] Grid vacio." << std::endl;
                return;
            }

            m_tiles.resize(m_gridH, std::vector<TileType>(m_gridW, TileType::Empty));
            m_rawChars.resize(m_gridH, std::vector<char>(m_gridW, '.'));

            int customCount = 0;

            for (int row = 0; row < m_gridH; row++)
            {
                const std::string& line = grid[row];
                for (int col = 0; col < m_gridW && col < (int)line.size(); col++)
                {
                    char c = line[col];
                    m_rawChars[row][col] = c;

                    f32 px = col * TILE_SIZE;
                    f32 py = row * TILE_SIZE;

                    // ¿Es un tile custom?
                    auto it = m_customVisuals.find(c);
                    if (it != m_customVisuals.end())
                    {
                        m_tiles[row][col] = TileType::Custom;
                        customCount++;

                        if (it->second.solid)
                            m_colliders.push_back({ px, py, TILE_SIZE, TILE_SIZE });
                        else if (it->second.platform)
                            m_platformColliders.push_back({ px, py, TILE_SIZE, TILE_SIZE });

                        continue; // No procesar como tile base
                    }

                    // Tile base
                    TileType type = charToTile(c);
                    m_tiles[row][col] = type;

                    // Colisiones para tiles sólidos (Exit NO es sólido)
                    if (type == TileType::Wall ||
                        type == TileType::WallAlt ||
                        type == TileType::Crate ||
                        type == TileType::NPC)
                    {
                        m_colliders.push_back({ px, py, TILE_SIZE, TILE_SIZE });
                    }

                    if (type == TileType::Player)
                    {
                        m_playerStartX = px;
                        m_playerStartY = py;
                    }

                    if (type == TileType::NPC)
                        m_npcPositions.push_back({ px, py, c });

                    if (type == TileType::Exit)
                        m_exitPositions.push_back({ px, py });
                }
            }

            std::cout << "[Tilemap] Grid: " << m_gridW << "x" << m_gridH
                      << " (" << m_colliders.size() << " colliders, "
                      << m_npcPositions.size() << " NPCs, "
                      << m_exitPositions.size() << " salidas, "
                      << customCount << " custom, "
                      << m_platformColliders.size() << " platforms)" << std::endl;
        }


        // ── load — Backward compat (SideScrollState, etc.) ──────────────────

        void load(const std::vector<std::string>& mapData)
        {
            loadGrid(mapData);  // Sin custom visuals

            m_npcSpawns.clear();
            for (const auto& pos : m_npcPositions)
            {
                TileNPCSpawn spawn;
                spawn.x = pos.x;
                spawn.y = pos.y;

                switch (pos.tile)
                {
                case 'L':
                    spawn.name = "Luna";
                    spawn.lines = { "Hola! Soy Luna.",
                        "Este nivel fue creado con un tilemap!",
                        "Cada '#' es una pared, cada '.' es suelo.",
                        "Disenar niveles ahora es como dibujar!" };
                    spawn.r = 0.6f; spawn.g = 0.4f; spawn.b = 0.7f;
                    break;
                case 'S':
                    spawn.name = "Sol";
                    spawn.lines = { "Soy Sol! Mira estas paredes.",
                        "Antes estaban hardcodeadas en el codigo.",
                        "Ahora un string define todo el nivel.",
                        "Eso es un tilemap!" };
                    spawn.r = 0.9f; spawn.g = 0.7f; spawn.b = 0.2f;
                    break;
                case 'E':
                    spawn.name = "Estrella";
                    spawn.lines = { "...",
                        std::to_string(m_gridW) + " x " + std::to_string(m_gridH)
                            + " tiles.",
                        "Cada una es 60x60 pixeles virtuales." };
                    spawn.r = 0.3f; spawn.g = 0.8f; spawn.b = 0.9f;
                    break;
                default:
                    spawn.name = std::string(1, pos.tile);
                    spawn.lines = { "..." };
                    spawn.r = spawn.g = spawn.b = 0.7f;
                    break;
                }
                m_npcSpawns.push_back(spawn);
                m_colliders.push_back({ pos.x, pos.y, TILE_SIZE, TILE_SIZE });
            }
        }


        // ── render — Con cámara, tile culling, y tiles custom ───────────────

        void render(f64 totalTime, f32 camX = 0.0f, f32 camY = 0.0f) const
        {
            const f32 VW = (f32)viewport::width();
            const f32 VH = (f32)viewport::height();

            int startCol = std::max(0, (int)(camX / TILE_SIZE));
            int startRow = std::max(0, (int)(camY / TILE_SIZE));
            int endCol   = std::min(m_gridW, (int)((camX + VW) / TILE_SIZE) + 2);
            int endRow   = std::min(m_gridH, (int)((camY + VH) / TILE_SIZE) + 2);

            for (int row = startRow; row < endRow; row++)
            {
                for (int col = startCol; col < endCol; col++)
                {
                    f32 px = col * TILE_SIZE - camX;
                    f32 py = row * TILE_SIZE - camY;

                    TileType type = m_tiles[row][col];

                    switch (type)
                    {
                    case TileType::Wall:
                        draw2d::drawRect(px, py, TILE_SIZE, TILE_SIZE,
                                         0.28f, 0.22f, 0.18f, 0.9f);
                        draw2d::drawRectBorder(px, py, TILE_SIZE, TILE_SIZE,
                                               0.4f, 0.35f, 0.28f, 0.6f, 1.5f);
                        break;

                    case TileType::WallAlt:
                        draw2d::drawRect(px, py, TILE_SIZE, TILE_SIZE,
                                         0.18f, 0.25f, 0.38f, 0.9f);
                        draw2d::drawRectBorder(px, py, TILE_SIZE, TILE_SIZE,
                                               0.3f, 0.4f, 0.6f, 0.6f, 1.5f);
                        break;

                    case TileType::Crate:
                        draw2d::drawRect(px, py, TILE_SIZE, TILE_SIZE,
                                         0.45f, 0.35f, 0.15f, 0.9f);
                        draw2d::drawRectBorder(px, py, TILE_SIZE, TILE_SIZE,
                                               0.6f, 0.5f, 0.2f, 0.8f, 2.0f);
                        draw2d::drawRect(px + TILE_SIZE*0.45f, py+4,
                                         TILE_SIZE*0.1f, TILE_SIZE-8,
                                         0.55f, 0.45f, 0.2f, 0.5f);
                        draw2d::drawRect(px+4, py + TILE_SIZE*0.45f,
                                         TILE_SIZE-8, TILE_SIZE*0.1f,
                                         0.55f, 0.45f, 0.2f, 0.5f);
                        break;

                    case TileType::Exit:
                    {
                        f32 pulse = (f32)std::sin(totalTime * 3.0) * 0.15f + 0.85f;
                        draw2d::drawRect(px, py, TILE_SIZE, TILE_SIZE,
                                         0.15f * pulse, 0.4f * pulse, 0.15f * pulse,
                                         0.7f);
                        draw2d::drawRectBorder(px, py, TILE_SIZE, TILE_SIZE,
                                               0.3f, 0.9f, 0.3f, pulse, 2.0f);
                        f32 ax = px + TILE_SIZE * 0.3f;
                        f32 ay = py + TILE_SIZE * 0.35f;
                        f32 aw = TILE_SIZE * 0.4f;
                        f32 ah = TILE_SIZE * 0.3f;
                        draw2d::drawRect(ax, ay, aw, ah,
                                         0.3f, 0.9f, 0.3f, pulse * 0.6f);
                        break;
                    }

                    case TileType::Custom:
                    {
                        char c = m_rawChars[row][col];
                        auto it = m_customVisuals.find(c);
                        if (it != m_customVisuals.end())
                        {
                            const auto& vis = it->second;

                            if (vis.animated)
                            {
                                // Pulso para tiles peligrosos (daño, etc.)
                                f32 pulse = 0.7f + 0.3f * (f32)std::sin(totalTime * 3.0);
                                draw2d::drawRect(px, py, TILE_SIZE, TILE_SIZE,
                                                 vis.r * pulse, vis.g * pulse, vis.b * pulse,
                                                 0.7f);
                                draw2d::drawRectBorder(px, py, TILE_SIZE, TILE_SIZE,
                                                       vis.r, vis.g, vis.b, pulse * 0.8f, 1.5f);
                            }
                            else
                            {
                                draw2d::drawRect(px, py, TILE_SIZE, TILE_SIZE,
                                                 vis.r, vis.g, vis.b, 0.7f);
                                draw2d::drawRectBorder(px, py, TILE_SIZE, TILE_SIZE,
                                                       vis.r * 1.3f, vis.g * 1.3f, vis.b * 1.3f,
                                                       0.5f, 1.0f);
                            }
                        }
                        else
                        {
                            // Fallback: gris genérico
                            draw2d::drawRect(px, py, TILE_SIZE, TILE_SIZE,
                                             0.4f, 0.4f, 0.4f, 0.5f);
                        }
                        break;
                    }

                    case TileType::NPC:
                        break; // NPCs se renderizan aparte

                    case TileType::Empty:
                    case TileType::Player:
                    default:
                        draw2d::drawRectBorder(px, py, TILE_SIZE, TILE_SIZE,
                                               0.08f, 0.2f, 0.1f, 0.08f, 0.5f);
                        break;
                    }
                }
            }
        }

        // ── Acceso a datos ───────────────────────────────────────────────────

        const std::vector<TileAABB>&    getColliders()         const { return m_colliders; }
        const std::vector<TileAABB>&    getPlatformColliders() const { return m_platformColliders; }
        const std::map<char, TileVisualInfo>& getCustomVisuals() const { return m_customVisuals; }
        const std::vector<TileNPCPos>&  getNPCPositions()  const { return m_npcPositions; }
        const std::vector<TileExitPos>& getExitPositions() const { return m_exitPositions; }
        const std::vector<TileNPCSpawn>&getNPCSpawns()     const { return m_npcSpawns; }
        f32 getPlayerStartX() const { return m_playerStartX; }
        f32 getPlayerStartY() const { return m_playerStartY; }

        // ── Consultar el char crudo en una posición del grid ─────────────────
        //
        // Retorna '.' si la posición está fuera de rango.
        //
        char getCharAt(int row, int col) const
        {
            if (row < 0 || row >= m_gridH || col < 0 || col >= m_gridW)
                return '.';
            return m_rawChars[row][col];
        }

        // Convierte posición en píxeles a fila/columna del grid
        int pixelToCol(f32 px) const { return (int)(px / TILE_SIZE); }
        int pixelToRow(f32 py) const { return (int)(py / TILE_SIZE); }

        // ── Mutación de tiles en runtime (Tier 3) ────────────────────────
        void setCharAt(int row, int col, char newChar,
            const std::map<char, TileVisualInfo>& customVisuals = {})
        {
            if (row < 0 || row >= m_gridH || col < 0 || col >= m_gridW) return;

            char oldChar = m_rawChars[row][col];
            if (oldChar == newChar) return;

            f32 px = col * TILE_SIZE;
            f32 py = row * TILE_SIZE;

            // Remover collider viejo si era sólido
            bool wasSolid = (m_tiles[row][col] == TileType::Wall ||
                m_tiles[row][col] == TileType::WallAlt ||
                m_tiles[row][col] == TileType::Crate);
            if (!wasSolid)
            {
                auto it = m_customVisuals.find(oldChar);
                if (it != m_customVisuals.end() && it->second.solid)
                    wasSolid = true;
            }
            if (wasSolid)
                removeColliderAtPos(px, py);

            // Actualizar char y tipo
            m_rawChars[row][col] = newChar;

            // ¿Es custom?
            auto it = m_customVisuals.find(newChar);
            if (it != m_customVisuals.end())
            {
                m_tiles[row][col] = TileType::Custom;
                if (it->second.solid)
                    m_colliders.push_back({ px, py, TILE_SIZE, TILE_SIZE });
                else if (it->second.platform)
                    m_platformColliders.push_back({ px, py, TILE_SIZE, TILE_SIZE });
            }
            else
            {
                TileType type = charToTile(newChar);
                m_tiles[row][col] = type;
                if (type == TileType::Wall || type == TileType::WallAlt || type == TileType::Crate)
                    m_colliders.push_back({ px, py, TILE_SIZE, TILE_SIZE });
            }

            std::cout << "[Tilemap] setCharAt(" << row << "," << col
                << ") '" << oldChar << "' -> '" << newChar << "'" << std::endl;
        }

        // ── Grid lookup de solidez (Tier 4C — rendimiento O(1)) ──────────────
        //
        // Consultan solidez directamente del grid, sin iterar m_colliders.
        // Equivalente a iterar m_wallColliders (con NPCs removidos), pero O(1).
        //
        // NOTA: No consideran TileType::NPC como sólido — los caracteres A-Z
        // son posiciones de spawn de entidades, no muros. Gameplay2DState
        // maneja sus propias colisiones contra entidades.
        //
        bool isSolidAt(int col, int row) const
        {
            if (col < 0 || col >= m_gridW || row < 0 || row >= m_gridH)
                return true; // Fuera del mapa = pared implícita
            TileType t = m_tiles[row][col];
            if (t == TileType::Wall || t == TileType::WallAlt || t == TileType::Crate)
                return true;
            if (t == TileType::Custom)
            {
                char c = m_rawChars[row][col];
                auto it = m_customVisuals.find(c);
                if (it != m_customVisuals.end()) return it->second.solid;
            }
            return false;
        }

        bool isPlatformAt(int col, int row) const
        {
            if (col < 0 || col >= m_gridW || row < 0 || row >= m_gridH) return false;
            if (m_tiles[row][col] != TileType::Custom) return false;
            char c = m_rawChars[row][col];
            auto it = m_customVisuals.find(c);
            return (it != m_customVisuals.end() && it->second.platform);
        }

        // ── Check por AABB en píxeles — útil para entidades ──
        //
        // Devuelve true si algún tile del grid que overlapea con el AABB
        // es sólido. Típicamente toca 1-4 tiles según tamaño/posición.
        //
        bool isSolidInArea(f32 x, f32 y, f32 w, f32 h) const
        {
            if (w <= 0 || h <= 0) return false;
            int c0 = (int)std::floor(x / TILE_SIZE);
            int c1 = (int)std::floor((x + w - 0.01f) / TILE_SIZE);
            int r0 = (int)std::floor(y / TILE_SIZE);
            int r1 = (int)std::floor((y + h - 0.01f) / TILE_SIZE);
            for (int r = r0; r <= r1; r++)
                for (int c = c0; c <= c1; c++)
                    if (isSolidAt(c, r)) return true;
            return false;
        }

    private:

        void removeColliderAtPos(f32 px, f32 py)
        {
            m_colliders.erase(
                std::remove_if(m_colliders.begin(), m_colliders.end(),
                    [px, py](const TileAABB& c) {
                        return std::abs(c.x - px) < 1.0f && std::abs(c.y - py) < 1.0f;
                    }),
                m_colliders.end());
            m_platformColliders.erase(
                std::remove_if(m_platformColliders.begin(), m_platformColliders.end(),
                    [px, py](const TileAABB& c) {
                        return std::abs(c.x - px) < 1.0f && std::abs(c.y - py) < 1.0f;
                    }),
                m_platformColliders.end());
        }

        TileType charToTile(char c) const
        {
            switch (c)
            {
                case '#': return TileType::Wall;
                case '=': return TileType::WallAlt;
                case 'B': return TileType::Crate;
                case 'P': return TileType::Player;
                case '>': return TileType::Exit;
                default:
                    if (c >= 'A' && c <= 'Z') return TileType::NPC;
                    return TileType::Empty;
            }
        }

        int m_gridW = 0, m_gridH = 0;
        std::vector<std::vector<TileType>> m_tiles;
        std::vector<std::vector<char>>     m_rawChars;      // Grid de chars crudos
        std::map<char, TileVisualInfo>     m_customVisuals;  // Visual de tiles custom
        std::vector<TileAABB>       m_colliders;
        std::vector<TileAABB>       m_platformColliders;
        std::vector<TileNPCPos>     m_npcPositions;
        std::vector<TileExitPos>    m_exitPositions;
        std::vector<TileNPCSpawn>   m_npcSpawns;
        f32 m_playerStartX = 0, m_playerStartY = 0;
    };

} // namespace eclips
