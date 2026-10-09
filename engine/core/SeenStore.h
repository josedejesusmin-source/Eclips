// ============================================================================
// SeenStore.h — Registro de líneas ya vistas (skip-de-leído) de Eclips
// ============================================================================
//
// HERMANO de MetaStore, NO parte de él. Misma plomería (FileUtil atómico,
// dueño Engine, carga al boot, vive en RAM), semántica DISTINTA:
//
//   MetaStore → flags CON SIGNIFICADO del juego ("cg_playa=true", "ruta=yuri").
//               Pocas, legibles, diff-friendly. El humano las lee.
//   SeenStore → caché de PROGRESO por el texto: "¿ya mostré esta línea?".
//               Miles, opacas (hashes), solo-máquina. Nadie las lee a mano.
//
// Por eso NO viven juntas (Fork A): meter miles de hashes en meta.eclips
// ahogaría justo lo que hace a meta legible, y cada flush reescribiría todo.
// Dos vidas distintas = dos archivos. Reusamos el plumbing, no la semántica.
//
// ── Nivel de persistencia ───────────────────────────────────────────────────
// El MISMO que meta: archivo en la RAÍZ (NO en saves/), sobrevive a borrar
// saves y a new game. "Texto que leíste, leído queda" — como el persistent de
// Ren'Py. Borrar una partida no te obliga a re-leer todo.
//
// ── Batching (Fork E) ───────────────────────────────────────────────────────
// Skip avanza cada ~50ms → hasta 20 líneas/s. Write-through martillaría el
// disco. Por eso mark() NO toca disco: solo inserta en RAM + marca dirty.
// flush() escribe (atómico) SOLO si hay cambios. Es el "save() explícito" que
// el comentario de MetaStore::set anticipó. El caller hace flush en las
// transiciones de escena + en save/menú/shutdown. Sin timer (YAGNI).
//
// ── Estructura ──────────────────────────────────────────────────────────────
// std::set<uint64> (no unordered): O(log n) sobra para esta escala y queda
// con salida ORDENADA → archivo estable entre corridas. La clave la deriva
// VNScriptState (él sabe qué ES "una línea": path+escena+índice+texto crudo);
// este store es un set de hashes TONTO, no entiende de qué son.
// ============================================================================

#pragma once

#include "FileUtil.h"

#include <set>
#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

namespace eclips
{
    class SeenStore
    {
    public:
        SeenStore() = default;

        // Raíz del proyecto, sibling de meta.eclips.
        static constexpr const char* SEEN_FILE = "seen.eclips";

        // ══════════════════════════════════════════════════════════════════
        // CARGA — al boot. Si no existe, arranca vacío (nadie ha leído nada
        // aún): NO es error. Parse tolerante: salta header/comentarios/vacías;
        // una línea que no sea hex válido se ignora (no rompe un archivo de
        // una versión futura con sintaxis extra).
        // ══════════════════════════════════════════════════════════════════
        bool load(const std::string& filepath = SEEN_FILE)
        {
            m_path = filepath;

            std::ifstream file(filepath);
            if (!file.is_open())
            {
                m_seen.clear();
                m_dirty = false;
                std::cout << "[Eclips] seen.eclips no existe aun "
                          << "(nada marcado como visto todavia)."
                          << std::endl;
                return false;
            }

            m_seen.clear();
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                if (line.empty() || line[0] == '[' || line[0] == '#')
                    continue;

                // Parse hex tolerante: línea basura → ignorada.
                try {
                    uint64_t key = std::stoull(line, nullptr, 16);
                    m_seen.insert(key);
                } catch (...) {
                    // no-op: línea no parseable, la saltamos.
                }
            }

            m_dirty = false;
            std::cout << "[Eclips] seen.eclips cargado (" << m_seen.size()
                      << " lineas vistas)." << std::endl;
            return true;
        }

        // ══════════════════════════════════════════════════════════════════
        // FLUSH — escribe a disco SOLO si hay cambios (atómico). El caller lo
        // llama en transiciones de escena + save/menú/shutdown. Si no hay nada
        // sucio, es un no-op barato. Retorna true si el disco quedó ok (o si
        // no había nada que escribir).
        // ══════════════════════════════════════════════════════════════════
        bool flush()
        {
            if (!m_dirty)
                return true;

            std::ostringstream oss;
            oss << "[Eclips Seen v1]\n";
            oss << "# Lineas ya vistas (hashes hex). Generado por el motor; no editar.\n";
            oss << std::hex;                  // hashes en hexadecimal
            for (uint64_t key : m_seen)       // set ordenado → salida estable
                oss << key << "\n";

            bool ok = writeTextFileAtomic(m_path.empty() ? SEEN_FILE : m_path,
                                          oss.str());
            if (ok)
                m_dirty = false;
            return ok;
        }

        // ══════════════════════════════════════════════════════════════════
        // API — mark NO toca disco (batched); has consulta membresía.
        // ══════════════════════════════════════════════════════════════════

        // Marcar una línea como vista. Solo RAM + dirty; persiste en flush().
        void mark(uint64_t key)
        {
            // insert().second == false si ya estaba → no ensuciar de gusto.
            if (m_seen.insert(key).second)
                m_dirty = true;
        }

        // ¿Ya se vio esta línea? (P2/P3: latch de skip + grisar botón Skip).
        bool has(uint64_t key) const
        {
            return m_seen.find(key) != m_seen.end();
        }

        size_t size() const { return m_seen.size(); }

    private:
        std::set<uint64_t> m_seen;
        std::string        m_path = SEEN_FILE;
        bool               m_dirty = false;
    };

} // namespace eclips