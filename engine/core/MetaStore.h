// ============================================================================
// MetaStore.h — Variables globales persistentes inter-save de Eclips
// ============================================================================
//
// El TERCER nivel de persistencia del motor:
//   - Settings  → preferencias del jugador (volumen, velocidad).
//   - Saves     → estado de UNA partida (muere al borrar el save).
//   - MetaStore → estado del JUEGO, cruzando TODAS las partidas.
//                 Sobrevive a borrar saves, sobrevive a new game.
//
// "Las variables son del juego, no del save."
//
// Casos: tracking de rutas DDLC, logros propios, galería/CGs desbloqueados,
// "intro ya vista", stats cruza-partida. El creador decide CUÁNDO algo cuenta
// (pone meta_set en el punto que él elige); el motor solo guarda y lee. El
// motor no entiende "actos" — el creador los arma con meta + condicionales.
//
// Archivo: "meta.eclips" en la RAÍZ del proyecto (NO en saves/) — a propósito:
//   no es save data, y separarlo físicamente refuerza que ningún borrado de
//   saves lo toca. Sibling de proyecto.eclips: uno dice qué ES el juego, el
//   otro qué ha HECHO el jugador a lo largo del tiempo.
//
// Formato flat key=value idéntico a SaveManager. Mapa ORDENADO (std::map) para
// salida determinista/legible/diff-friendly — es el archivo más longevo del
// motor, conviene que un humano pueda leerlo y compararlo.
//
// Escritura ATÓMICA vía FileUtil (.tmp + rename): un crash a mitad nunca trunca
// el progreso permanente.
//
// Dueño: Engine (vive SOBRE los states, así un meta_set en gameplay se ve en VN
// y en el menú). Se carga una vez al boot y vive en RAM (es minúsculo).
//
// Lectura desde EclipScript: TRANSPARENTE — resolveVariable consulta este store
// como un nivel más (P3), así `si ruta` y `${ruta}` leen meta sin meta_get. La
// escritura sí es explícita (meta_set), por diseño (Fork #1, opción A).
// ============================================================================

#pragma once

#include "FileUtil.h"
#include "SaveCodec.h"   // ← CP1: piso 1 (escapeLine) + header + isValidKey

#include <string>
#include <map>
#include <vector>        // ← CP1: buffer de pares crudos en load()
#include <fstream>
#include <iostream>
#include <sstream>

namespace eclips
{
    class MetaStore
    {
    public:
        MetaStore() = default;

        // Ruta por defecto: raíz del proyecto (al lado del ejecutable).
        static constexpr const char* META_FILE = "meta.eclips";

        // ══════════════════════════════════════════════════════════════════
        // CARGA — al boot. Si el archivo no existe, arranca vacío: NO es error,
        // es el primer arranque del jugador. Parse tolerante (igual robustez
        // que SaveManager): salta header, comentarios, vacías y líneas sin '='.
        // ══════════════════════════════════════════════════════════════════
        bool load(const std::string& filepath = META_FILE)
        {
            m_path = filepath;

            std::ifstream file(filepath);
            if (!file.is_open())
            {
                m_data.clear();
                std::cout << "[Eclips] meta.eclips no existe aun "
                          << "(primer arranque, sin progreso permanente)."
                          << std::endl;
                return false;
            }

            // 1ª pasada: detectar versión del header + bufferizar pares crudos;
            // decodificar al final (idéntico patrón que SaveManager).
            int  fileVersion = 1;      // ausente/ilegible ⇒ v1 (legacy crudo)
            bool headerSeen = false;
            std::vector<std::pair<std::string, std::string>> raw;
            std::string line;
            while (std::getline(file, line))
            {
                // Tolerar \r de archivos hand-editados en Windows (Notepad).
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                if (line.empty())
                    continue;

                // ¿Header de versión? (solo el primero cuenta)
                int hv = savecodec::parseHeaderVersion(line);
                if (hv > 0)
                {
                    if (!headerSeen) { fileVersion = hv; headerSeen = true; }
                    continue;
                }

                // Comentarios y otros '[...]' que no son header de versión.
                if (line[0] == '[' || line[0] == '#')
                    continue;

                size_t eq = line.find('=');
                if (eq != std::string::npos)
                    raw.emplace_back(line.substr(0, eq), line.substr(eq + 1));
                // Línea sin '=' → ignorada (tolerante; no rompe meta viejo de
                // una versión futura con sintaxis extra).
            }

            // Versión futura ⇒ rechazo ruidoso del archivo entero (§4.3).
            if (fileVersion > 2)
            {
                std::cerr << "[Eclips] meta.eclips version v" << fileVersion
                    << " no soportada (este motor lee hasta v2)." << std::endl;
                m_data.clear();
                return false;
            }

            // Decodificar: v2 ⇒ unescape piso 1; v1/ausente ⇒ crudo (legacy).
            m_data.clear();
            for (auto& kv : raw)
                m_data[kv.first] = (fileVersion >= 2)
                ? savecodec::unescapeLine(kv.second)
                : kv.second;

            std::cout << "[Eclips] meta.eclips cargado (" << m_data.size()
                << " variables, v" << fileVersion << ")." << std::endl;
            return true;
        }

        // ══════════════════════════════════════════════════════════════════
        // GUARDADO — atómico (FileUtil). Header versionado para tolerancia
        // futura (NO maquinaria de migración: eso es YAGNI hasta que exista v2).
        // ══════════════════════════════════════════════════════════════════
        bool save() const
        {
            // v2: valores line-escaped (piso 1). Sin piso 2 — meta es una clave
            // por variable, sin compuestos. m_data ya es std::map (ordenado):
            // no necesita sort-on-write como SaveManager.
            std::ostringstream oss;
            oss << "[Eclips Meta v2]\n";
            for (const auto& [k, v] : m_data)
            {
                // Clave engine-generated (via meta_set): se valida, no se escapa.
                if (!savecodec::isValidKey(k))
                {
                    std::cerr << "[Eclips] Clave meta invalida rechazada: '"
                        << k << "'" << std::endl;
                    continue;
                }
                oss << k << "=" << savecodec::escapeLine(v) << "\n";
            }

            return writeTextFileAtomic(m_path.empty() ? META_FILE : m_path,
                oss.str());
        }

        // ══════════════════════════════════════════════════════════════════
        // API DE VARIABLES — strings crudas, mismo modelo que m_variables del
        // VN (la coerción a int la hace el LECTOR, igual que set/si). Así la
        // lectura transparente desde resolveVariable (P3) es exacta.
        // ══════════════════════════════════════════════════════════════════

        // Setear y persistir (write-through; los eventos meta son raros en v1).
        // El batching para skip-de-leído es un agregado futuro barato: separar
        // un setRaw() sin flush + un save() explícito. NO se hace ahora.
        // Retorna true si el disco quedó ok.
        bool set(const std::string& key, const std::string& value)
        {
            m_data[key] = value;
            return save();
        }

        // Leer. defaultVal si no existe.
        std::string get(const std::string& key,
                        const std::string& defaultVal = "") const
        {
            auto it = m_data.find(key);
            return (it != m_data.end()) ? it->second : defaultVal;
        }

        // ¿Existe la clave? (lo usa resolveVariable/interpolación en P3).
        bool has(const std::string& key) const
        {
            return m_data.find(key) != m_data.end();
        }

        // Borrar una clave y persistir. Retorna true si existía.
        bool erase(const std::string& key)
        {
            bool existed = (m_data.erase(key) > 0);
            if (existed)
                save();
            return existed;
        }

        // Vaciar TODO el progreso permanente y persistir. Es el ÚNICO camino a
        // resetear meta (lo usará meta_borrar/meta_reset en P4) — a propósito:
        // borrar_saves NO toca esto.
        bool clear()
        {
            m_data.clear();
            return save();
        }

        // Acceso de solo-lectura al mapa entero (lo consulta resolveVariable
        // en P3 sin copiar).
        const std::map<std::string, std::string>& data() const { return m_data; }

        size_t size() const { return m_data.size(); }

    private:
        std::map<std::string, std::string> m_data;
        std::string m_path = META_FILE;
    };

} // namespace eclips
