#pragma once
// ============================================================================
// TextRepertorio.h - CP-R1: QUE ESCALARES PUEDEN LLEGAR A CADA CONSUMIDOR
// ============================================================================
//
// Ley pura del arco ATLAS-DE-FUENTE. No conoce manifest, carpetas, fuentes,
// TTF, cmap, atlas ni GL. El caller ya decidio que un texto alcanza una
// superficie dibujable y entrega los consumidores posibles con ids opacos.
//
// TextRepertorio conserva escalares Unicode validos, ordenados y unicos.
// Entrada UTF-8 invalida se reporta por offset/ancho pero no se convierte en
// codepoint: no puede fabricar cobertura ni procedencia.
// ============================================================================

#include "TextBudget.h"
#include "Types.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace eclips
{
    namespace text
    {
        inline bool esEscalarUnicode(i32 cp)
        {
            return cp >= 0 && cp <= 0x10FFFF &&
                   !(cp >= 0xD800 && cp <= 0xDFFF);
        }

        struct TextEntradaInvalida
        {
            size_t offset = 0;
            size_t bytes = 0;
        };

        struct TextAgregarResultado
        {
            size_t nuevos = 0;
            std::vector<TextEntradaInvalida> invalidos;
        };

        class TextRepertorio
        {
        public:
            bool agregarEscalar(i32 cp)
            {
                if (!esEscalarUnicode(cp)) return false;
                return m_valores.insert(cp).second;
            }

            TextAgregarResultado agregarTexto(const std::string& texto)
            {
                TextAgregarResultado out;
                size_t pos = 0;
                while (pos < texto.size())
                {
                    i32 cp = -1;
                    const size_t step = utf8Decodificar(texto, pos, cp);
                    if (step == 0) break;
                    if (cp >= 0)
                    {
                        if (agregarEscalar(cp)) ++out.nuevos;
                    }
                    else
                    {
                        out.invalidos.push_back(TextEntradaInvalida{ pos, step });
                    }
                    pos += step;
                }
                return out;
            }

            size_t unir(const TextRepertorio& otro)
            {
                size_t nuevos = 0;
                for (i32 cp : otro.m_valores)
                    if (m_valores.insert(cp).second) ++nuevos;
                return nuevos;
            }

            bool contiene(i32 cp) const
            {
                return m_valores.find(cp) != m_valores.end();
            }

            size_t size() const { return m_valores.size(); }
            bool empty() const { return m_valores.empty(); }

            std::vector<i32> valores() const
            {
                return std::vector<i32>(m_valores.begin(), m_valores.end());
            }

            void limpiar() { m_valores.clear(); }

        private:
            std::set<i32> m_valores;
        };

        // Opaque to this layer: the caller owns what 1, 2, ... mean. Keeping
        // it numeric prevents this pure collector from inferring names, paths
        // or atlas identities that only exist in later CPs.
        struct TextConsumidorId
        {
            std::uint32_t valor = 0;

            bool operator<(const TextConsumidorId& other) const
            {
                return valor < other.valor;
            }

            bool operator==(const TextConsumidorId& other) const
            {
                return valor == other.valor;
            }
        };

        class TextProyeccionRepertorio
        {
        public:
            TextAgregarResultado agregarTexto(
                const std::string& texto,
                std::initializer_list<TextConsumidorId> consumidores)
            {
                TextRepertorio pieza;
                TextAgregarResultado out = pieza.agregarTexto(texto);
                out.nuevos = m_total.unir(pieza);
                for (const TextConsumidorId id : consumidores)
                    m_porConsumidor[id].unir(pieza);
                return out;
            }

            const TextRepertorio& total() const { return m_total; }

            const TextRepertorio* para(TextConsumidorId id) const
            {
                const auto it = m_porConsumidor.find(id);
                return it == m_porConsumidor.end() ? nullptr : &it->second;
            }

            std::vector<TextConsumidorId> consumidores() const
            {
                std::vector<TextConsumidorId> out;
                out.reserve(m_porConsumidor.size());
                for (const auto& pair : m_porConsumidor)
                    out.push_back(pair.first);
                return out;
            }

            void limpiar()
            {
                m_total.limpiar();
                m_porConsumidor.clear();
            }

        private:
            TextRepertorio m_total;
            std::map<TextConsumidorId, TextRepertorio> m_porConsumidor;
        };
    }
}
