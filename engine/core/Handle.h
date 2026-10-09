// ============================================================================
// ECLIPS ENGINE - Sistema de Handles (Punteros seguros)
// ============================================================================
// Documento de diseño: Sección 10.10
//
// Este sistema existe para resolver un problema mortal:
// Si Lua recibe un puntero directo a memoria de C++, y C++ libera esa
// memoria, Lua crashea el motor (Segmentation Fault).
//
// La solución: Lua NUNCA recibe punteros. Solo recibe Handles (IDs numéricos).
// Cada Handle tiene un "index" (posición) y una "generation" (versión).
// La generación evita que un ID reciclado apunte a algo que ya no existe.
//
// Ejemplo:
//   Handle h = manager.create();  // Crea entidad, retorna Handle
//   Entity* e = manager.get(h);   // Obtiene la entidad (o nullptr si murió)
//   manager.destroy(h);           // Destruye la entidad
//   Entity* e2 = manager.get(h);  // Retorna nullptr (seguro, no crashea)
// ============================================================================

#pragma once

#include "Types.h"
#include <vector>
#include <queue>

namespace eclips
{
    // ── Handle: ID seguro que Lua puede guardar sin riesgo ──
    struct Handle
    {
        u32 index      = 0;  // Posición en el array interno
        u32 generation = 0;  // Versión (se incrementa al reciclar)

        // Un handle inválido tiene generation 0
        bool isValid() const { return generation != 0; }

        // Comparación (para poder usar Handles como keys en mapas)
        bool operator==(const Handle& other) const
        {
            return index == other.index && generation == other.generation;
        }

        bool operator!=(const Handle& other) const
        {
            return !(*this == other);
        }
    };

    // ── HandleManager: Administra la creación y validación de Handles ──
    // Tipo T es lo que quieras administrar (Entity, Sprite, AudioSource, etc.)
    template<typename T>
    class HandleManager
    {
    public:
        // Crea un nuevo elemento y retorna su Handle
        Handle create(const T& item)
        {
            Handle handle;

            if (!m_freeIndices.empty())
            {
                // Reciclar un slot libre
                handle.index = m_freeIndices.front();
                m_freeIndices.pop();
                handle.generation = m_generations[handle.index];
                m_items[handle.index] = item;
                m_alive[handle.index] = true;
            }
            else
            {
                // Crear un slot nuevo
                handle.index = static_cast<u32>(m_items.size());
                handle.generation = 1; // Primera generación
                m_items.push_back(item);
                m_generations.push_back(1);
                m_alive.push_back(true);
            }

            return handle;
        }

        // Obtiene el elemento por Handle. Retorna nullptr si no existe.
        // ESTA es la función que hace todo seguro:
        // Si el Handle es viejo (generación no coincide), retorna nullptr.
        T* get(Handle handle)
        {
            if (handle.index < m_items.size() &&
                m_alive[handle.index] &&
                m_generations[handle.index] == handle.generation)
            {
                return &m_items[handle.index];
            }
            return nullptr; // Seguro: no crashea, solo retorna "no existe"
        }

        // Versión const
        const T* get(Handle handle) const
        {
            if (handle.index < m_items.size() &&
                m_alive[handle.index] &&
                m_generations[handle.index] == handle.generation)
            {
                return &m_items[handle.index];
            }
            return nullptr;
        }

        // Destruye un elemento. El Handle deja de ser válido.
        void destroy(Handle handle)
        {
            if (handle.index < m_items.size() &&
                m_alive[handle.index] &&
                m_generations[handle.index] == handle.generation)
            {
                m_alive[handle.index] = false;
                m_generations[handle.index]++; // Incrementar generación invalida handles viejos
                m_freeIndices.push(handle.index);
            }
        }

        // ¿Está vivo este Handle?
        bool isAlive(Handle handle) const
        {
            return handle.index < m_items.size() &&
                   m_alive[handle.index] &&
                   m_generations[handle.index] == handle.generation;
        }

        // Cantidad de elementos activos
        u32 activeCount() const
        {
            u32 count = 0;
            for (bool alive : m_alive)
            {
                if (alive) count++;
            }
            return count;
        }

    private:
        std::vector<T>    m_items;        // Los elementos
        std::vector<u32>  m_generations;  // Generación de cada slot
        std::vector<bool> m_alive;        // ¿Está vivo este slot?
        std::queue<u32>   m_freeIndices;  // Slots libres para reciclar
    };

} // namespace eclips
