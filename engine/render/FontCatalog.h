// ============================================================================
// ECLIPS ENGINE - FontCatalog (ATLAS-DE-FUENTE CP-R3)
// ============================================================================
// EL REGISTRO LOGICO DE IDENTIDADES, y no es un dueno nuevo.
//
// El reparto firmado no se mueve: `ProjectManifest` y `EclipScriptData`
// TRANSPORTAN declaraciones, el `FontSourceStore` de `Engine` posee la verdad
// compilada, y `FontManager`/`TextRenderer` poseen atlas. Esta clase es el
// pegamento: decide QUE identidad le corresponde a cada nombre y le pide a
// `FontManager::declararPerfil` que la hornee. No abre un TTF, no guarda un
// atlas y no tiene estado que sobreviva a su `FontManager`.
//
// La ley de ambito es `PV-R3-F2-A`, firmada:
//
//   * el catalogo del manifest es GLOBAL y AUTORITATIVO;
//   * una declaracion `.ecl` identica a la global es NO-OP silencioso;
//   * una declaracion `.ecl` distinta con un nombre ya global AVISA citando
//     las dos procedencias y NO pisa al manifest;
//   * un nombre que el manifest no declara forma catalogo LOCAL AL ARCHIVO,
//     con identidad interna `{ruta de script resuelta byte a byte, nombre}`.
//     Sin canonicalizar: esa superficie es de `DISENO_MODELO_CONFIANZA`.
//
// Por que el ambito existe, medido y no supuesto: en `vnscript/` el nombre
// `titulo` significa TRES fuentes distintas en tres archivos, asi que un mapa
// global por nombre no es compatible ni con el corpus raiz. Y `cargar_script`
// conserva el `FontManager` y las capas, asi que sin identidad interna una
// capa nacida bajo el archivo A cambiaria de fuente al encadenar B.
// ============================================================================
#pragma once

#include "../core/FontDeclaration.h"
#include "FontManager.h"

#include <cstddef>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace eclips
{
    class FontCatalog
    {
    public:
        // Catalogo global del proyecto. Se adopta una vez, al abrir el state.
        void adoptarGlobales(const std::vector<fontdecl::Declaracion>& decls)
        {
            for (const auto& d : decls)
            {
                if (d.name.empty()) continue;
                // keep-first: el manifest ya avisa su propio conflicto, aca
                // solo se conserva la primera valida.
                m_globales.emplace(d.name, d);
            }
        }

        // Re-adopcion EXPLICITA para F8. No es un clear() de la ruta normal:
        // keep-first de adoptarGlobales se conserva. F8 arma el catalogo
        // candidato y llama esto dentro de la transaccion de commit.
        void readoptarGlobales(const std::vector<fontdecl::Declaracion>& decls)
        {
            m_globales.clear();
            m_globalesMaterializados.clear();
            adoptarGlobales(decls);
        }

        // Catalogo de compatibilidad del archivo `.ecl` que va a ejecutarse.
        // REEMPLAZA al anterior: encadenar un script cambia el ambito activo,
        // no acumula nombres de archivos que ya no corren.
        void adoptarLocales(const std::string& scriptPath,
                            const std::vector<fontdecl::Declaracion>& decls)
        {
            m_ambitoPath = scriptPath;
            m_locales.clear();
            m_localesMaterializados.clear();

            for (const auto& d : decls)
            {
                if (d.name.empty()) continue;

                const auto global = m_globales.find(d.name);
                if (global != m_globales.end())
                {
                    if (global->second.mismaFormaQue(d))
                        continue;   // identica: no-op, y sin ruido
                    ++m_avisos;
                    std::cout << "[FontCatalog] la fuente \"" << d.name
                              << "\" ya esta declarada en '"
                              << global->second.sourceFile << "' linea "
                              << global->second.sourceLine
                              << " con otra forma; gana la declaracion del "
                                 "proyecto y se ignora la de '"
                              << d.sourceFile << "' linea " << d.sourceLine
                              << "." << std::endl;
                    continue;
                }

                const auto local = m_locales.find(d.name);
                if (local != m_locales.end())
                {
                    if (local->second.mismaFormaQue(d)) continue;
                    ++m_avisos;
                    std::cout << "[FontCatalog] la fuente \"" << d.name
                              << "\" se declara dos veces con formas distintas "
                                 "en '" << local->second.sourceFile
                              << "' lineas " << local->second.sourceLine
                              << " y " << d.sourceLine
                              << "; se conserva la primera." << std::endl;
                    continue;
                }

                m_locales.emplace(d.name, d);
            }
        }

        // Hornea los perfiles que todavia no existen y deja el ambito activo
        // apuntado. GL-safe only: llamalo en `update`/`render`, nunca en
        // parse-time. Idempotente — volver a llamarlo no cuesta un atlas.
        bool materializar(FontManager& fm, bool publishLive = true)
        {
            bool todo = true;
            for (const auto& kv : m_globales)
            {
                if (m_globalesMaterializados.count(kv.first)) continue;
                if (fm.declararPerfil(kv.first, kv.second))
                    m_globalesMaterializados.insert(kv.first);
                else
                    todo = false;
            }
            for (const auto& kv : m_locales)
            {
                if (m_localesMaterializados.count(kv.first)) continue;
                const std::string clave =
                    FontManager::claveLocal(m_ambitoPath, kv.first);
                if (fm.declararPerfil(clave, kv.second))
                    m_localesMaterializados.insert(kv.first);
                else
                    todo = false;
            }
            if (publishLive)
            {
                fm.setAmbitoActivo(m_ambitoPath);
                std::vector<std::string> live;
                live.reserve(m_globales.size() + m_locales.size());
                for (const auto& kv : m_globales) live.push_back(kv.first);
                for (const auto& kv : m_locales)
                    live.push_back(FontManager::claveLocal(m_ambitoPath, kv.first));
                fm.setCatalogLive(live);
            }
            return todo;
        }

        void publicarLive(FontManager& fm) const
        {
            fm.setAmbitoActivo(m_ambitoPath);
            std::vector<std::string> live;
            live.reserve(m_globales.size() + m_locales.size());
            for (const auto& kv : m_globales) live.push_back(kv.first);
            for (const auto& kv : m_locales)
                live.push_back(FontManager::claveLocal(m_ambitoPath, kv.first));
            fm.setCatalogLive(live);
        }

        // Nombre autoral -> identidad interna. "" = ese nombre no lo declara
        // nadie, y el caller conserva su aviso pedagogico de siempre.
        std::string resolver(const std::string& name) const
        {
            if (name.empty()) return std::string();
            if (m_globales.count(name)) return name;
            if (m_locales.count(name) && !m_ambitoPath.empty())
                return FontManager::claveLocal(m_ambitoPath, name);
            return std::string();
        }

        const std::string& ambito() const { return m_ambitoPath; }
        std::size_t globales() const { return m_globales.size(); }
        std::size_t locales() const { return m_locales.size(); }
        std::size_t avisosPrecedencia() const { return m_avisos; }

    private:
        std::map<std::string, fontdecl::Declaracion> m_globales;
        std::map<std::string, fontdecl::Declaracion> m_locales;
        std::set<std::string> m_globalesMaterializados;
        std::set<std::string> m_localesMaterializados;
        std::string m_ambitoPath;
        std::size_t m_avisos = 0;
    };
}
