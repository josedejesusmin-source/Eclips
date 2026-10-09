// ============================================================================
// LuaManager.h — Runtime Lua para Eclips Engine
// ============================================================================
//
// Wrapper sobre sol2 (que a su vez wrappea la API C de Lua) para ejecutar
// scripts Lua desde EclipScript. El estado Lua persiste entre llamadas:
// variables, funciones y tablas definidas en un script siguen vivas para
// el siguiente.
//
// ── USO DESDE ECLIPSCRIPT ──────────────────────────────────────────────────
//
//   lua "mi_logica.lua"           # Ejecuta el archivo
//   ejecutar_lua "mi_logica.lua"  # Sinónimo español
//
// ── USO DESDE C++ ──────────────────────────────────────────────────────────
//
//   eclips::LuaManager lua;
//   lua.init();                           // Abre estado Lua + libs estándar
//   lua.executeFile("scripts/test.lua");  // Ejecuta archivo
//   lua.executeString("print('hola')");   // Ejecuta string inline
//   lua.reset();                          // Destruye y recrea el estado
//
// ── FASE ACTUAL (Bloque 1) ─────────────────────────────────────────────────
//
//   Solo infraestructura: ejecutar scripts Lua y persistir estado.
//   SIN bindings al motor todavía — eso viene en Bloque 2.
//
// ============================================================================

#pragma once

#include <string>
#include <iostream>
#include <filesystem>

// sol2 — C++ binding para Lua, header-only
// Viene via vcpkg: sol2 + lua
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace eclips
{
    class LuaManager
    {
    public:
        LuaManager() = default;
        ~LuaManager() = default;

        // No copiar (sol::state no es copiable)
        LuaManager(const LuaManager&) = delete;
        LuaManager& operator=(const LuaManager&) = delete;

        // ── Inicializar el runtime Lua ──────────────────────────────────────
        // Abre las librerías estándar de Lua (math, string, table, io, os...)
        // y registra la función eclips_print para redirigir print() al log.
        void init()
        {
            if (m_initialized) return;

            m_lua.open_libraries(
                sol::lib::base,
                sol::lib::math,
                sol::lib::string,
                sol::lib::table,
                sol::lib::io,
                sol::lib::os
            );

            // Sobreescribir print() de Lua para que salga por std::cout
            // con prefijo [Lua] — así aparece en la consola de debug.
            m_lua.set_function("print", [](sol::variadic_args va) {
                std::cout << "[Lua] ";
                bool first = true;
                for (auto v : va)
                {
                    if (!first) std::cout << "\t";
                    first = false;

                    // Convertir cualquier tipo a string via tostring de Lua
                    sol::state_view lua(v.lua_state());
                    sol::function tostring = lua["tostring"];
                    std::string s = tostring(v.get<sol::object>());
                    std::cout << s;
                }
                std::cout << std::endl;
            });

            m_initialized = true;
            std::cout << "[LuaManager] Runtime Lua inicializado" << std::endl;
        }

        // ── Ejecutar un archivo .lua ────────────────────────────────────────
        // La ruta puede ser absoluta o relativa al working directory.
        // Si se proporciona scriptDir, rutas relativas se resuelven desde ahí.
        bool executeFile(const std::string& filepath,
                         const std::string& scriptDir = "")
        {
            if (!m_initialized)
            {
                std::cerr << "[LuaManager] ERROR: no inicializado" << std::endl;
                return false;
            }

            // Resolver ruta: si es relativa y tenemos directorio del script .ecl,
            // buscar relativo a ese directorio primero
            std::string resolvedPath = filepath;

            if (!scriptDir.empty() &&
                !std::filesystem::path(filepath).is_absolute())
            {
                std::string candidate = scriptDir + "/" + filepath;
                if (std::filesystem::exists(candidate))
                    resolvedPath = candidate;
            }

            std::cout << "[LuaManager] Ejecutando: " << resolvedPath << std::endl;

            auto result = m_lua.safe_script_file(resolvedPath,
                                                  sol::script_pass_on_error);

            if (!result.valid())
            {
                sol::error err = result;
                std::cerr << "[LuaManager] ERROR en '" << resolvedPath
                          << "': " << err.what() << std::endl;
                return false;
            }

            std::cout << "[LuaManager] OK: " << resolvedPath << std::endl;
            return true;
        }

        // ── Ejecutar un string de Lua directamente ──────────────────────────
        bool executeString(const std::string& code)
        {
            if (!m_initialized)
            {
                std::cerr << "[LuaManager] ERROR: no inicializado" << std::endl;
                return false;
            }

            auto result = m_lua.safe_script(code, sol::script_pass_on_error);

            if (!result.valid())
            {
                sol::error err = result;
                std::cerr << "[LuaManager] ERROR en string: "
                          << err.what() << std::endl;
                return false;
            }
            return true;
        }

        // ── Resetear el estado Lua ──────────────────────────────────────────
        // Destruye todo (variables, funciones) y reinicia limpio.
        void reset()
        {
            m_lua = sol::state();
            m_initialized = false;
            init();
            std::cout << "[LuaManager] Estado reseteado" << std::endl;
        }

        // ── Acceso directo al estado sol (para Bloque 2: bindings) ──────────
        sol::state& state() { return m_lua; }

        bool isInitialized() const { return m_initialized; }

    private:
        sol::state  m_lua;
        bool        m_initialized = false;
    };

} // namespace eclips
