// ============================================================================
// ECLIPS ENGINE - Asset Manager (Caché LRU de texturas)
// ============================================================================
//
// Sistema centralizado de carga y descarga automática de texturas.
// Mantiene la RAM constante sin importar cuántas escenas tenga el juego,
// descargando automáticamente lo que no se usa.
//
// Diseño:
//   - Cada textura se identifica por su ruta de archivo (clave única)
//   - Cuando el caché está lleno, se expulsa la textura menos usada (LRU)
//   - Las texturas "pinned" (en uso activo) nunca se expulsan
//   - El contador lógico evita problemas con resolución de time()
//
// Uso:
//   AssetManager assets(32);             // Pool de hasta 32 texturas
//   Texture* bg = assets.get("fondo.png"); // Carga o devuelve del caché
//   assets.pin("fondo.png");               // Proteger de evicción
//   assets.unpin("fondo.png");             // Permitir evicción cuando no se use
//   assets.clear();                        // Liberar todo (al cerrar)
//
// Integración con VNScriptState:
//   - loadBackground() → get() + pin nuevo, unpin viejo
//   - cmdMostrar()     → get() + pin sprite activo
//   - cmdOcultar()     → unpin sprite oculto
//   - onExit()         → clear()
//
// RAM estimada del manager: ~1KB overhead (sin contar texturas).
// Cada textura en GPU depende de su resolución:
//   1920x1080 RGBA = ~8MB, 960x540 RGBA = ~2MB
//
// ============================================================================

#pragma once

#include "engine/render/Texture.h"
#include "engine/core/LangPaths.h"
#include "engine/core/Types.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <limits>

namespace eclips
{
    class AssetManager
    {
    public:

        // ── Constructor ─────────────────────────────────────────────────────
        // maxTextures: número máximo de texturas en el pool.
        // El fondo + personajes activos cuentan contra este límite.
        // Recomendado: 24-48 para VNs típicas.

        explicit AssetManager(size_t maxTextures = 32)
            : m_maxTextures(maxTextures)
        {}

        ~AssetManager() { clear(); }

        // No copiar (las texturas de OpenGL no se copian)
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        // ── Obtener textura ─────────────────────────────────────────────────
        // Si ya está en caché, la devuelve y actualiza su timestamp LRU.
        // Si no, la carga desde disco. Si el pool está lleno, evicta la
        // textura menos usada que no esté pinned.
        // Retorna nullptr si no se pudo cargar.

        Texture* get(const std::string& pathPedida)
        {
            if (pathPedida.empty()) return nullptr;

            // ── CP4: el asset espejo, ANTES de las dos memorias ────────────
            // §6 del contrato de idiomas: "la resolución ocurre ANTES del
            // caché: la ruta resuelta ES la clave de caché". Sin catálogo
            // -o en el idioma original- esto devuelve la MISMA cadena sin
            // tocar el disco, que es la byte-identidad del voto 3.
            //
            // Las OTRAS CUATRO puertas de esta casa (pin/unpin/isPinned/has)
            // resuelven igual, y no es simetría: si sólo resolviera aquí, el
            // caché quedaría tecleado por la ruta espejo y los 50 call-sites
            // de pin/unpin -que pasan rutas GUARDADAS en el state- fallarían
            // el find y no harían nada, EN SILENCIO. Lo mide `harness_espejo`
            // sección 7.
            const std::string path = langResolverRuta(pathPedida);

            // ── Path ya marcado como fallido? Silente. ─────────────────────
            // §7.28: memoización de fallos. El primer intento ya logueó
            // pedagógicamente; los siguientes vuelven nullptr sin ruido
            // (evita spam de cerr a 60 fps en hot loop).
            if (m_failedPaths.find(path) != m_failedPaths.end())
                return nullptr;

            // ── Ya en caché? ──
            auto it = m_cache.find(path);
            if (it != m_cache.end())
            {
                it->second.lastUsed = ++m_accessCounter;
                return &it->second.texture;
            }

            // ── Pool lleno? Evictar ──
            if (m_cache.size() >= m_maxTextures)
            {
                if (!evictLRU())
                {
                    // No se pudo evictar nada (todo pinned).
                    // Permitir crecimiento temporal con warning.
                    std::cerr << "[AssetManager] WARN: pool lleno y todo pinned ("
                              << m_cache.size() << "/" << m_maxTextures
                              << "). Crecimiento temporal." << std::endl;
                }
            }

            // ── Cargar nueva textura ──
            CacheEntry entry;
            entry.lastUsed = ++m_accessCounter;
            entry.pinned   = false;

            if (!entry.texture.load(path))
            {
                std::cerr << "[AssetManager] Error cargando: " << path
                    << " (no se reintentará)" << std::endl;
                m_failedPaths.insert(path);
                return nullptr;
            }

            auto [inserted, ok] = m_cache.emplace(path, std::move(entry));
            if (!ok) return nullptr;

            std::cout << "[AssetManager] Cargada: " << path
                      << " (" << m_cache.size() << "/" << m_maxTextures << ")"
                      << std::endl;

            return &inserted->second.texture;
        }

        // ── Pinning ─────────────────────────────────────────────────────────
        // Las texturas pinned NO se evictan. Pinear lo que está en pantalla:
        // fondo actual, transición vieja (si hay), personajes activos.

        // CP4: las cuatro puertas de IDENTIDAD resuelven igual que `get`. Ver
        // el comentario de arriba: el par pin/unpin llega con rutas guardadas
        // en el state (m_transOldBgPath, ac.spritePath, ac.facePath...), y un
        // find fallido acá no rompe nada visible - deja la textura evictable.
        void pin(const std::string& path)
        {
            auto it = m_cache.find(langResolverRuta(path));
            if (it != m_cache.end())
                it->second.pinned = true;
        }

        void unpin(const std::string& path)
        {
            auto it = m_cache.find(langResolverRuta(path));
            if (it != m_cache.end())
                it->second.pinned = false;
        }

        void unpinAll()
        {
            for (auto& [key, entry] : m_cache)
                entry.pinned = false;
        }

        bool isPinned(const std::string& path) const
        {
            auto it = m_cache.find(langResolverRuta(path));
            return it != m_cache.end() && it->second.pinned;
        }

        // ── Consultas ───────────────────────────────────────────────────────

        bool has(const std::string& path) const
        {
            return m_cache.find(langResolverRuta(path)) != m_cache.end();
        }

        size_t count()    const { return m_cache.size(); }
        size_t maxSize()  const { return m_maxTextures; }

        size_t pinnedCount() const
        {
            size_t n = 0;
            for (const auto& [k, e] : m_cache)
                if (e.pinned) ++n;
            return n;
        }

        // ── Limpiar todo ────────────────────────────────────────────────────
        // Libera todas las texturas de la GPU. Llamar al cerrar el estado.

        void clear()
        {
            for (auto& [path, entry] : m_cache)
                entry.texture.unload();
            m_cache.clear();
            m_failedPaths.clear();
            // CP4 (fork 3a): el memo de idioma muere JUNTO con el caché y no
            // al cambiar de idioma. Si cambiara de respuesta a mitad de la
            // vida de este caché, un `unpin(rutaGuardada)` dejaría de
            // encontrar lo que su `pin` guardó. Así cada generación del caché
            // vive entera en un solo idioma — que es, además, la política que
            // el §3 ya firmaba: "hasta el próximo load natural".
            langMemoLimpiar();
            m_accessCounter = 0;
            std::cout << "[AssetManager] Cache limpiada." << std::endl;
        }

        // ── Evictar manualmente texturas no pinned ──────────────────────────
        // Útil después de un cambio de escena grande.

        void evictUnpinned()
        {
            auto it = m_cache.begin();
            while (it != m_cache.end())
            {
                if (!it->second.pinned)
                {
                    std::cout << "[AssetManager] Evictada (manual): "
                              << it->first << std::endl;
                    it->second.texture.unload();
                    it = m_cache.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // ── Recargar TODAS las texturas desde disco (hot-reload, dev-only) ──
        // Modo Creador / F8: el creador repinta un PNG (mismo path, contenido
        // nuevo) y quiere verlo sin recompilar. Recorre TODO el cache (pinned o
        // no: trivial con el techo de RAM, y evita que una unpinned-pero-cacheada
        // salga stale al volver a pantalla).
        //
        // Fail-safe §7.34: carga a una Texture temporal y solo swapWith si
        // load()==true. Si el archivo está borrado / a medio guardar (Aseprite
        // escribiendo), CONSERVA la textura viva en vez de blanquearla — load()
        // in-place destruiría la vieja antes de confirmar la nueva.
        //
        // §7.28: limpia m_failedPaths para reintentar un path que ANTES no
        // existía y se memoizó como fallido ("olvidé exportar el sprite, F8").
        //
        // TODO(modo-creador): se compila fuera en build de export.
        void reloadAll()
        {
            size_t reloaded = 0, kept = 0;
            for (auto& [path, entry] : m_cache)
            {
                Texture fresh;
                if (fresh.load(path))
                {
                    entry.texture.swapWith(fresh); // fresh queda con la vieja → su dtor la libera
                    ++reloaded;
                }
                else
                {
                    ++kept; // fresh quedó vacío (id=0); entry.texture intacta
                }
            }
            m_failedPaths.clear();

            // CP4 (fork 3a + §6: "invalidación del memo en clearFailed /
            // hot-reload F8"). El creador acaba de exportar un PNG: si lo tiró
            // en `idiomas/<código>/`, sin esto el memo seguiría contestando la
            // ruta original y F8 no se lo mostraría nunca.
            //
            // ⚠ LA CONSECUENCIA, DECLARADA, porque este sitio NO es como
            // `clear()`: `reloadAll` conserva las claves y los pins. Si el
            // asset espejo NACE entre dos F8, el próximo `get` va a resolver a
            // una clave NUEVA y la entrada vieja se queda pinneada hasta el
            // próximo `clear()` (o sea, hasta el `onExit` del state). Está
            // acotado por el pool y es dev-only (Modo Creador); se prefiere a
            // barrer el caché acá, que le tiraría los pins a la escena viva.
            langMemoLimpiar();

            std::cout << "[AssetManager] reloadAll: " << reloaded << " recargada(s)";
            if (kept) std::cout << ", " << kept << " conservada(s) (load fallo)";
            std::cout << ", failedPaths limpiado." << std::endl;
        }

        // ── Debug: imprimir estado del caché ────────────────────────────────

        void debugPrint() const
        {
            std::cout << "[AssetManager] Estado del cache ("
                << m_cache.size() << "/" << m_maxTextures << "):"
                << std::endl;
            for (const auto& [path, entry] : m_cache)
            {
                std::cout << "  " << (entry.pinned ? "[PIN] " : "      ")
                    << "LRU=" << entry.lastUsed
                    << "  " << entry.texture.getWidth()
                    << "x" << entry.texture.getHeight()
                    << "  " << path << std::endl;
            }

            if (!m_failedPaths.empty())
            {
                std::cout << "[AssetManager] Paths fallidos memoizados ("
                    << m_failedPaths.size() << "):" << std::endl;
                for (const auto& path : m_failedPaths)
                    std::cout << "  [FAIL] " << path << std::endl;
            }
        }

    private:

        // ── Entrada del caché ───────────────────────────────────────────────

        struct CacheEntry
        {
            Texture texture;
            u64     lastUsed = 0;   // Timestamp lógico (no wall-clock)
            bool    pinned   = false;

            // Move-only (Texture no se copia)
            CacheEntry() = default;
            CacheEntry(CacheEntry&& o) noexcept
                : lastUsed(o.lastUsed), pinned(o.pinned)
            {
                texture.swapWith(o.texture);
            }
            CacheEntry& operator=(CacheEntry&& o) noexcept
            {
                if (this != &o)
                {
                    texture.unload();
                    texture.swapWith(o.texture);
                    lastUsed = o.lastUsed;
                    pinned   = o.pinned;
                }
                return *this;
            }
            CacheEntry(const CacheEntry&) = delete;
            CacheEntry& operator=(const CacheEntry&) = delete;
        };

        // ── Evictar la textura con menor lastUsed que no esté pinned ────────

        bool evictLRU()
        {
            u64 minUsed = std::numeric_limits<u64>::max();
            std::string victim;

            for (const auto& [path, entry] : m_cache)
            {
                if (!entry.pinned && entry.lastUsed < minUsed)
                {
                    minUsed = entry.lastUsed;
                    victim  = path;
                }
            }

            if (victim.empty())
                return false; // Todo está pinned

            std::cout << "[AssetManager] Evictada (LRU): " << victim
                      << " (lastUsed=" << minUsed << ")" << std::endl;

            auto it = m_cache.find(victim);
            it->second.texture.unload();
            m_cache.erase(it);
            return true;
        }

        // ── Datos ───────────────────────────────────────────────────────────

        std::unordered_map<std::string, CacheEntry> m_cache;
        std::unordered_set<std::string>             m_failedPaths;  // §7.28: memoización de fallos
        u64    m_accessCounter = 0;      // Contador lógico para LRU
        size_t m_maxTextures = 32;     // Límite del pool
    };

} // namespace eclips
