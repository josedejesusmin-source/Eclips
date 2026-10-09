// ============================================================================
// FileUtil.h — Utilidades de escritura de archivos de Eclips
// ============================================================================
//
// writeTextFileAtomic: escribe contenido de texto a 'path' de forma que un
// crash a mitad de escritura NUNCA deje el archivo destino truncado.
//
// Cómo: escribe primero a 'path.tmp', y solo si eso tuvo éxito hace 'rename'
// sobre 'path'. El rename de un mismo volumen REEMPLAZA atómicamente en MSVC
// (MoveFileEx con replace) y en POSIX (rename(2)). Si el proceso muere a
// mitad de la escritura del .tmp, el destino conserva su versión anterior
// intacta y lo único que queda es un '.tmp' huérfano (inofensivo, se
// sobrescribe la próxima vez).
//
// Falla-seguro: si el rename fallara (caso raro), el destino viejo NO se toca
// y lo nuevo queda recuperable en el .tmp. Nunca se pierde data. Ese es el
// tier de robustez correcto para un archivo de config/save chico — ReplaceFileW
// con <windows.h> sería over-engineering para esto.
//
// Por qué importa: meta.eclips es el archivo más longevo del motor — le
// sobrevive a TODOS los saves. Perderlo por un crash = perder todo el progreso
// permanente. Los saves se benefician del mismo helper.
// ============================================================================

#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <system_error>

namespace eclips
{
    // Escribe 'content' a 'path' de forma atómica (.tmp + rename).
    // Crea el directorio padre si no existe. Retorna true si el destino
    // quedó escrito correctamente; false si falló (destino intacto).
    inline bool writeTextFileAtomic(const std::string& path,
                                    const std::string& content)
    {
        namespace fs = std::filesystem;

        // Crear directorio padre si hace falta
        fs::path target(path);
        if (target.has_parent_path())
        {
            std::error_code ecDir;
            fs::create_directories(target.parent_path(), ecDir);
            // ecDir ignorado a propósito: si el dir ya existe no es error;
            // si falla de verdad, el open de abajo lo delata.
        }

        const std::string tmpPath = path + ".tmp";

        // 1) Escribir a tmp y cerrar (el scope fuerza el cierre antes del rename)
        {
            std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                std::cerr << "[Eclips] writeTextFileAtomic: no se pudo abrir tmp: "
                          << tmpPath << std::endl;
                return false;
            }

            out << content;
            out.flush();

            if (!out.good())
            {
                std::cerr << "[Eclips] writeTextFileAtomic: fallo al escribir tmp: "
                          << tmpPath << std::endl;
                out.close();
                std::error_code ecRm;
                fs::remove(tmpPath, ecRm);
                return false;
            }
        } // out.~ofstream() aquí

        // 2) Rename atómico sobre el destino (reemplaza en MSVC y POSIX).
        std::error_code ecMv;
        fs::rename(tmpPath, target, ecMv);
        if (ecMv)
        {
            std::cerr << "[Eclips] writeTextFileAtomic: rename fallo ("
                      << ecMv.message() << "). El destino conserva su version "
                      << "previa; queda '" << tmpPath << "' con lo nuevo."
                      << std::endl;
            return false;
        }

        return true;
    }

} // namespace eclips
