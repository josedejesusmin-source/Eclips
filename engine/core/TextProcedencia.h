#pragma once
// ============================================================================
// TextProcedencia.h - CP-R1: QUIEN CUBRE CADA CODEPOINT
// ============================================================================
//
// Resolver puro sobre candidatos YA evaluados. Este archivo no abre rutas, no
// lee TTF/cmap y no decide identidad por contenido: esos owners nacen en R2.
// Recibe una identidad fisica opaca y el estado que el caller midio.
//
// Leyes:
// - gana el primer candidato fisico que cubre;
// - NoCubre es normal y no genera incidencia;
// - ruta/fuente/cmap rotos generan incidencia y la cadena continua;
// - una identidad repetida conserva su primera posicion y habla una vez;
// - si nadie cubre, se omite sin una fuente secreta del engine;
// - un codepoint se resuelve una vez dentro de este plan.
// ============================================================================

#include "Types.h"

#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace eclips
{
    namespace text
    {
        enum class TextEstadoCandidato
        {
            Cubre,
            NoCubre,
            RutaRota,
            FuenteInvalida,
            CmapInutilizable
        };

        struct TextCandidatoEvaluado
        {
            std::string fuenteFisica;
            TextEstadoCandidato estado = TextEstadoCandidato::NoCubre;
        };

        enum class TextCobertura
        {
            Cubierto,
            Omitido
        };

        enum class TextTipoIncidencia
        {
            RutaRota,
            FuenteInvalida,
            CmapInutilizable,
            CandidatoRepetido
        };

        struct TextIncidenciaProcedencia
        {
            TextTipoIncidencia tipo = TextTipoIncidencia::RutaRota;
            std::string fuenteFisica;
            size_t indice = 0;
        };

        struct TextProcedenciaResultado
        {
            i32 codepoint = -1;
            TextCobertura cobertura = TextCobertura::Omitido;
            std::string fuenteFisica;
            size_t indice = std::numeric_limits<size_t>::max();
            std::vector<TextIncidenciaProcedencia> incidencias;
        };

        class TextProcedencia
        {
        public:
            const TextProcedenciaResultado& resolver(
                i32 codepoint,
                const std::vector<TextCandidatoEvaluado>& candidatos)
            {
                const auto memo = m_resultados.find(codepoint);
                if (memo != m_resultados.end()) return memo->second;

                TextProcedenciaResultado out;
                out.codepoint = codepoint;

                std::set<std::string> vistos;
                std::set<std::string> repeticionInformada;
                for (size_t i = 0; i < candidatos.size(); ++i)
                {
                    const TextCandidatoEvaluado& candidato = candidatos[i];
                    if (!vistos.insert(candidato.fuenteFisica).second)
                    {
                        if (repeticionInformada.insert(
                                candidato.fuenteFisica).second)
                        {
                            out.incidencias.push_back(
                                TextIncidenciaProcedencia{
                                    TextTipoIncidencia::CandidatoRepetido,
                                    candidato.fuenteFisica,
                                    i });
                        }
                        continue;
                    }

                    switch (candidato.estado)
                    {
                    case TextEstadoCandidato::Cubre:
                        out.cobertura = TextCobertura::Cubierto;
                        out.fuenteFisica = candidato.fuenteFisica;
                        out.indice = i;
                        break;
                    case TextEstadoCandidato::NoCubre:
                        break;
                    case TextEstadoCandidato::RutaRota:
                        out.incidencias.push_back(TextIncidenciaProcedencia{
                            TextTipoIncidencia::RutaRota,
                            candidato.fuenteFisica,
                            i });
                        break;
                    case TextEstadoCandidato::FuenteInvalida:
                        out.incidencias.push_back(TextIncidenciaProcedencia{
                            TextTipoIncidencia::FuenteInvalida,
                            candidato.fuenteFisica,
                            i });
                        break;
                    case TextEstadoCandidato::CmapInutilizable:
                        out.incidencias.push_back(TextIncidenciaProcedencia{
                            TextTipoIncidencia::CmapInutilizable,
                            candidato.fuenteFisica,
                            i });
                        break;
                    }

                    if (out.cobertura == TextCobertura::Cubierto) break;
                }

                return m_resultados.emplace(codepoint, std::move(out))
                    .first->second;
            }

            size_t resoluciones() const { return m_resultados.size(); }
            void limpiar() { m_resultados.clear(); }

        private:
            std::map<i32, TextProcedenciaResultado> m_resultados;
        };
    }
}
