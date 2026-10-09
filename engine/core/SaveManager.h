// ============================================================================
// SaveManager.h — Sistema de guardado/carga de Eclips
// ============================================================================
//
// Guarda el estado del juego en un archivo de texto simple.
// Formato: clave=valor, una por linea, con las claves ORDENADAS al escribir
// (sort-on-write: salida determinista, diffs legibles). Ejemplo:
//
//   [Eclips Save v2]
//   choice_Luna=2
//   mode=topdown
//   npc_Luna_talked=1
//   posX=500
//   posY=300
//
// La cabecera [Eclips <familia> v2] es el PISO 1 del formato (SaveCodec.h).
// La familia la elige el escritor: Save, Meta, Seen...
//
//   v2 → los valores viajan percent-escapados a nivel de LINEA: los bytes que
//        romperian el "una clave por linea" (%, \n, \r) se guardan como
//        %25 %0A %0D. Un valor multilinea es seguro (un hook `on_save()` de
//        Lua puede devolver varias lineas sin corromper el archivo).
//   v1 → archivos viejos, sin escape. Se leen TAL CUAL, sin desescapar: el
//        lector legacy queda CONGELADO en su epoca. Cargar nunca reescribe.
//   sin cabecera → se asume v1 (saves anteriores al versionado).
//
// OJO: este es el piso del ARCHIVO. La gramatica de cada ranura (VN o gameplay)
// tiene su PROPIO sello — slot_N_ver / slot_N_gp_ver. Dos escritores, dos
// sellos. Ver DISENO_SAVE_CONTRATO.md.
//
// ¿Por que texto y no binario?
//   - Es legible por humanos (el creador puede debuggear)
//   - Es facil de editar manualmente
//   - Para juegos 2D, la cantidad de datos es minima
//   - No necesitamos libreria de serialización
//
// Ubicacion del archivo: saves/save.eclips
//   (al lado del ejecutable, en la carpeta saves/)
//
// Uso:
//   SaveManager save;
//   save.setFloat("posX", 500.0f);
//   save.setString("mode", "topdown");
//   save.setBool("npc_Luna_talked", true);
//   save.writeToFile("saves/save.eclips");
//
//   SaveManager load;
//   load.readFromFile("saves/save.eclips");
//   float x = load.getFloat("posX", 0.0f);  // 0.0f = default si no existe
//
// ============================================================================

#pragma once

#include "Types.h"
#include "FileUtil.h"    // ← escritura atómica compartida
#include "SaveCodec.h"   // ← CP1: piso 1 (escapeLine) + header + isValidKey

#include <string>
#include <unordered_map>
#include <map>           // ← CP1: sort-on-write (salida determinista)
#include <vector>        // ← CP1: buffer de pares crudos en readFromFile
#include <algorithm>     // ← CP4 teclado: keysWithPrefix devuelve ordenado
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

namespace eclips
{
    class SaveManager
    {
    public:
        SaveManager() = default;

        // ══════════════════════════════════════════════════════════
        // ESCRIBIR VALORES
        // ══════════════════════════════════════════════════════════
        void setString(const std::string& key, const std::string& value)
        {
            m_data[key] = value;
        }

        void setFloat(const std::string& key, f32 value)
        {
            m_data[key] = std::to_string(value);
        }

        void setInt(const std::string& key, i32 value)
        {
            m_data[key] = std::to_string(value);
        }

        void setBool(const std::string& key, bool value)
        {
            m_data[key] = value ? "1" : "0";
        }

        // ══════════════════════════════════════════════════════════
        // LEER VALORES (con defaults si la clave no existe)
        // ══════════════════════════════════════════════════════════
        std::string getString(const std::string& key, const std::string& defaultVal = "") const
        {
            auto it = m_data.find(key);
            return (it != m_data.end()) ? it->second : defaultVal;
        }

        f32 getFloat(const std::string& key, f32 defaultVal = 0.0f) const
        {
            auto it = m_data.find(key);
            if (it != m_data.end())
            {
                try { return std::stof(it->second); }
                catch (...) { return defaultVal; }
            }
            return defaultVal;
        }

        i32 getInt(const std::string& key, i32 defaultVal = 0) const
        {
            auto it = m_data.find(key);
            if (it != m_data.end())
            {
                try { return std::stoi(it->second); }
                catch (...) { return defaultVal; }
            }
            return defaultVal;
        }

        bool getBool(const std::string& key, bool defaultVal = false) const
        {
            auto it = m_data.find(key);
            return (it != m_data.end()) ? (it->second == "1") : defaultVal;
        }

        bool hasKey(const std::string& key) const
        {
            return m_data.find(key) != m_data.end();
        }

        // ══════════════════════════════════════════════════════════
        // GUARDAR A ARCHIVO
        // ══════════════════════════════════════════════════════════
        // 'family' nombra el header ([Eclips <family> v2]) — Save por defecto,
        // "Settings" cuando lo invoca Settings::save (deja de firmar "Save").
        // SIEMPRE escribe v2: valores line-escaped (piso 1). La promoción de un
        // archivo v1 es automática y lossless (escapeLine es identidad sobre un
        // valor sin % \n \r, y un '%' literal → %25 que revierte exacto).
        bool writeToFile(const std::string& filepath,
            const std::string& family = "Save")
        {
            // Construir el contenido en memoria, luego escribir atómico.
            std::ostringstream oss;
            oss << "[Eclips " << family << " v2]\n";

            // Sort-on-write: m_data es unordered → orden no determinista. Se
            // copia a un ordenado para salida estable/diff-friendly (espejo del
            // std::map deliberado de MetaStore — diffs legibles al ojo).
            std::map<std::string, std::string> sorted(m_data.begin(), m_data.end());

            for (const auto& pair : sorted)
            {
                // Clave engine-generated: se valida, no se escapa. Una clave
                // rota es un bug del motor → se rechaza ruidoso, no se escribe.
                if (!savecodec::isValidKey(pair.first))
                {
                    std::cerr << "[Eclips] Clave invalida rechazada al guardar: '"
                        << pair.first << "' (contiene =, salto de linea, o abre "
                        << "con [ o #)" << std::endl;
                    continue;
                }
                oss << pair.first << "="
                    << savecodec::escapeLine(pair.second) << "\n";
            }

            if (!writeTextFileAtomic(filepath, oss.str()))
            {
                std::cerr << "[Eclips] Error: No se pudo guardar: "
                    << filepath << std::endl;
                return false;
            }

            std::cout << "[Eclips] Partida guardada: " << filepath
                << " (" << m_data.size() << " campos)" << std::endl;
            return true;
        }

        // ══════════════════════════════════════════════════════════
        // CARGAR DESDE ARCHIVO
        // ══════════════════════════════════════════════════════════
        bool readFromFile(const std::string& filepath)
        {
            std::ifstream file(filepath);
            if (!file.is_open())
            {
                // No es un error — simplemente no hay save
                return false;
            }

            // 1ª pasada: detectar la versión del header y bufferizar los pares
            // CRUDOS. Se decodifica al final, cuando ya se conoce la versión
            // (defensivo ante orden; en la práctica el header va primero).
            int  fileVersion = 1;      // ausente/ilegible ⇒ v1 (legacy crudo)
            bool headerSeen = false;
            std::vector<std::pair<std::string, std::string>> raw;
            std::string line;

            while (std::getline(file, line))
            {
                // Tolerar CRLF de archivos hand-editados en Windows (Notepad).
                // Un '\r' de DATO en v2 va escapado como %0D → nunca crudo aquí.
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

                // Buscar el '=' que separa clave=valor
                size_t eq = line.find('=');
                if (eq != std::string::npos)
                    raw.emplace_back(line.substr(0, eq), line.substr(eq + 1));
            }

            file.close();

            // Versión futura ⇒ rechazo ruidoso del ARCHIVO ENTERO: no se puede
            // adivinar el codec de los valores. Jamás medio-parse (§4.3).
            if (fileVersion > 2)
            {
                std::cerr << "[Eclips] Save version v" << fileVersion
                    << " no soportada (este motor lee hasta v2): "
                    << filepath << std::endl;
                m_data.clear();
                return false;
            }

            // Decodificar segun versión: v2 ⇒ unescape de piso 1; v1/ausente ⇒
            // crudo (lector legacy congelado, byte-idéntico a hoy).
            m_data.clear();
            for (auto& kv : raw)
                m_data[kv.first] = (fileVersion >= 2)
                ? savecodec::unescapeLine(kv.second)
                : kv.second;

            std::cout << "[Eclips] Partida cargada: " << filepath
                << " (" << m_data.size() << " campos, v" << fileVersion
                << ")" << std::endl;

            return true;
        }

        // ══════════════════════════════════════════════════════════
        // UTILIDADES
        // ══════════════════════════════════════════════════════════
        void clear() { m_data.clear(); }

        // Borrar una clave específica. Retorna true si existía.
        bool eraseKey(const std::string& key)
        {
            return m_data.erase(key) > 0;
        }

        // Las claves que empiezan con el prefijo dado, ORDENADAS. Es el gemelo
        // de LECTURA de `eraseKeysWithPrefix`, que vive acá abajo desde antes:
        // esta clase ya sabía operar por prefijo, le faltaba el lado que no
        // escribe. No toca el codec ni el lector: es un accesor const.
        //
        // **Ordenadas por la misma razón por la que `writeToFile` ordena**, y
        // acá además cambia una LEY, no sólo la prolijidad: `m_data` es
        // `unordered_map`, así que el recorrido no está especificado. Quien lee
        // una familia de claves y avisa por las que están mal necesita que el
        // aviso salga igual en cada corrida — y una regla de "si está repetida,
        // uso la última" sobre un orden no especificado es un sorteo.
        //
        // Caller: CP4 del átomo de teclado (`tecla.*` en settings.eclips).
        std::vector<std::string> keysWithPrefix(const std::string& prefix) const
        {
            std::vector<std::string> out;
            for (const auto& kv : m_data)
                if (kv.first.rfind(prefix, 0) == 0)
                    out.push_back(kv.first);

            std::sort(out.begin(), out.end());
            return out;
        }

        // Borrar todas las claves que empiezan con el prefijo dado.
        // Retorna el número de claves eliminadas.
        size_t eraseKeysWithPrefix(const std::string& prefix)
        {
            size_t removed = 0;
            for (auto it = m_data.begin(); it != m_data.end(); )
            {
                if (it->first.rfind(prefix, 0) == 0)
                {
                    it = m_data.erase(it);
                    ++removed;
                }
                else ++it;
            }
            return removed;
        }

        size_t size() const { return m_data.size(); }

        // Verificar si existe un archivo de guardado
        static bool saveExists(const std::string& filepath)
        {
            return std::filesystem::exists(filepath);
        }

    private:
        std::unordered_map<std::string, std::string> m_data;
    };

} // namespace eclips
