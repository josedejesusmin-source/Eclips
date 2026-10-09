// ============================================================================
// ECLIPS ENGINE - FontDeclaration (ATLAS-DE-FUENTE CP-R3)
// ============================================================================
// LA LEY PURA DE UNA DECLARACION DE FUENTE. Sin GL, sin TTF, sin atlas.
//
// Existe por una razon medida y no por prolijidad: la MISMA cabecera
//
//     fuente <nombre> "<ruta>" tamano=N [espaciado_letras=N] [medida=em|altura]
//     fuente <nombre> "<ruta>" tamano=N { respaldo "<ruta>" ... }
//
// se escribe en DOS dialectos —`proyecto.eclips` (catalogo global) y `.ecl`
// (compatibilidad local del archivo)— y si cada uno la validara por su cuenta,
// el dia uno divergirian el default de kerning, la medida legacy, el alias
// ingles y la recuperacion de error. Aca vive UNA sola:
//
//   * la tabla de alias de los tres parametros  -> `gruposParametro()`
//   * los defaults firmados (kerning 0, medida altura)
//   * la validacion (nombre, ruta, tamano positivo y finito, medida valida)
//   * el CUERPO canonico de cada error          -> `cuerpoError()`
//   * la palabra hija del bloque                 -> `esRespaldo()`
//
// Lo unico que cada dialecto conserva es su TOKENIZADOR y su PREFIJO de voz:
// el `.ecl` dice `[EclipScript] AVISO linea N:` y el manifest
// `[proyecto.eclips] Aviso linea N:`. El cuerpo del mensaje sale de aca, asi
// que un harness puede exigir que los dos digan exactamente lo mismo.
//
// NO canonicaliza rutas: `DA-20` lo prohibe y esa superficie es de
// `DISENO_MODELO_CONFIANZA`. Una ruta repetida CONSERVA su posicion en la
// cadena (`DA-21`: gana la primera).
// ============================================================================
#pragma once

#include "Types.h"
#include "EclipsParse.h"
#include "../render/FontMeasure.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace eclips
{
namespace fontdecl
{
    // ── Una declaracion autoral YA VALIDADA ──────────────────────────────
    // `routes[0]` es la base; el resto son los respaldos en orden autoral.
    // `sourceFile`/`sourceLine` son PROCEDENCIA: el aviso de precedencia cita
    // las dos, y sin ellas no puede decir de donde vino cada mitad.
    struct Declaracion
    {
        std::string              name;
        std::vector<std::string> routes;
        f32                      size = 0.0f;
        f32                      kerning = 0.0f;
        FontMeasure              measure = FontMeasure::Height;
        bool                     measureSpecified = false;
        std::string              sourceFile;
        int                      sourceLine = 0;

        // Igualdad de FORMA (spec), sin procedencia: es lo que decide si una
        // declaracion `.ecl` repetida es no-op o conflicto.
        bool mismaFormaQue(const Declaracion& o) const
        {
            return name == o.name && routes == o.routes &&
                   size == o.size && kerning == o.kerning &&
                   measure == o.measure;
        }
    };

    enum class Error
    {
        Ninguno,
        NombreConComillas,
        FaltaNombreORuta,
        TamanoInvalido,
        MedidaInvalida,
        ColaInvalida
    };

    // ── La tabla de alias, UNICA ─────────────────────────────────────────
    // `grupo[0]` es la grafia canonica. `EclipScript.h` alimenta su fila de
    // `EclipCmdType::Fuente` desde aca, asi que no hay una segunda copia.
    //
    // ── LA ENIE VA EN BYTES EXPLICITOS, Y ES UN BUG ARREGLADO ────────────
    // El `u8"tamaño"` que vivia en `EclipScript.h` estaba DOBLE-ENCODEADO:
    // ese archivo es UTF-8 SIN BOM, MSVC lo lee como ANSI y el prefijo `u8`
    // re-codifica lo que ya era UTF-8. Medido con sonda de bytes: el literal
    // daba 9 bytes (74 61 6D 61 C3 83 C2 B1 6F) contra los 7 que trae un
    // `.ecl` real (74 61 6D 61 C3 B1 6F), asi que `tamaño=72` con la enie de
    // verdad NUNCA matcheo. La canonica es la ASCII a proposito: este archivo
    // no mete acentos en literales de C++, y esta palabra la IMPRIME la ayuda.
    inline const std::vector<std::vector<std::string>>& gruposParametro()
    {
        static const std::vector<std::vector<std::string>> kGrupos = {
            { "tamano", "tama\xC3\xB1o", "size" },
            { "espaciado_letras", "letter_spacing" },
            { "medida", "measure" }
        };
        return kGrupos;
    }

    inline const std::string& claveTamano()    { return gruposParametro()[0][0]; }
    inline const std::string& claveEspaciado() { return gruposParametro()[1][0]; }
    inline const std::string& claveMedida()    { return gruposParametro()[2][0]; }

    // ── La palabra hija del bloque (`PV-R3-F1`, firmada) ─────────────────
    // `respaldo|fallback`, y SOLO como hija de `fuente|font`. `backup` queda
    // fuera por redundante. Fuera del bloque estas dos palabras no significan
    // nada y cada dialecto las trata como linea desconocida y ruidosa: la
    // posicion es lo que las distingue del `respaldo` de archivo que firmo F11.
    //
    // **PLIEGA**, y es la mitad que la primera vuelta se comió: `RESPALDO` es
    // vocabulario del MOTOR, no un nombre que el creador invente, así que
    // `FOLD-PARTIDO` manda que una mayúscula no cambie nada. El `.ecl` ya
    // plegaba sus claves `k=v` (`parseKeyValueParams` → `vocabLower`), así que
    // dejar esto byte-exacto no era «consistente con los openers viejos»: era
    // una DIVERGENCIA entre los dos dialectos de la misma cabecera — justo lo
    // que este header existe para impedir.
    inline bool esRespaldo(const std::string& tok)
    {
        const std::string t = vocabToLower(tok);
        return t == "respaldo" || t == "fallback";
    }

    // ── El CUERPO canonico de cada error, sin prefijo de dialecto ────────
    // `detalle` es el valor que el creador escribio, cuando el dialecto lo
    // tiene a mano: lo usa la medida invalida, que sin el aviso nombraria el
    // problema pero no la palabra que hay que corregir.
    inline std::string cuerpoError(Error e, const std::string& detalle = "")
    {
        switch (e)
        {
            case Error::NombreConComillas:
                return "fuente: el nombre va SIN comillas "
                       "(ej: fuente titulo \"serif.ttf\" tamano=72). "
                       "Las comillas son para la ruta del archivo, "
                       "no para el nombre que inventas.";
            case Error::FaltaNombreORuta:
                return "fuente: falta nombre o ruta "
                       "(ej: fuente titulo \"serif.ttf\" tamano=72)";
            case Error::TamanoInvalido:
                return "fuente: falta tamano valido "
                       "(ej: tamano=72). El tamano se hornea en el atlas.";
            case Error::MedidaInvalida:
            {
                std::string s = "fuente: medida invalida";
                if (!detalle.empty()) s += " '" + detalle + "'";
                s += ". Se esperan em|altura (em|height). Se ignora la fuente.";
                return s;
            }
            case Error::ColaInvalida:
                return "fuente: el '{' abre la cadena de respaldos y va SOLO "
                       "al final de la linea.";
            case Error::Ninguno:
            default:
                return std::string();
        }
    }

    // ── Cabecera partida (la usa el dialecto que NO tiene tokenizador propio)
    struct Cabecera
    {
        std::string nombre;
        std::string ruta;
        std::string params;
        bool        abreBloque = false;
    };

    // Pela un `{` FINAL de la cola de parametros. Devuelve `ColaInvalida` si
    // aparece una llave en cualquier otra posicion: `{` con cola no abre nada.
    // Es la unica ley del `{` y la comparten los dos dialectos.
    inline Error pelarLlave(std::string& params, bool& abreBloque)
    {
        abreBloque = false;
        std::string s = trimBlancos(params);
        const size_t pos = s.find('{');
        if (pos == std::string::npos) { params = s; return Error::Ninguno; }
        if (pos != s.size() - 1) return Error::ColaInvalida;
        s.erase(pos);
        params = trimBlancos(s);
        abreBloque = true;
        return Error::Ninguno;
    }

    // `<nombre> "<ruta>" k=v ... [{]` -> Cabecera. Para `ProjectManifest`.
    inline Error partirCabecera(const std::string& rest, Cabecera& out)
    {
        out = Cabecera{};
        const std::string s = trimBlancos(rest);
        if (s.empty()) return Error::FaltaNombreORuta;
        if (s[0] == '"') return Error::NombreConComillas;

        const size_t sp = s.find(' ');
        if (sp == std::string::npos) return Error::FaltaNombreORuta;
        out.nombre = s.substr(0, sp);

        const std::string resto = trimBlancos(s.substr(sp + 1));
        const size_t q1 = resto.find('"');
        if (q1 == std::string::npos) return Error::FaltaNombreORuta;
        const size_t q2 = resto.find('"', q1 + 1);
        if (q2 == std::string::npos) return Error::FaltaNombreORuta;
        out.ruta = resto.substr(q1 + 1, q2 - q1 - 1);

        out.params = trimBlancos(resto.substr(q2 + 1));
        return pelarLlave(out.params, out.abreBloque);
    }

    // `k=v k=v` -> mapa con UNA grafia por parametro, la canonica. Las claves
    // que el comando no conoce salen por `desconocidas` y se descartan; el
    // choque de grafias gana la primera del grupo y sale por `choques`.
    inline std::map<std::string, std::string> canonizarParams(
        const std::string& params,
        std::vector<std::string>* desconocidas = nullptr,
        std::vector<std::string>* choques = nullptr)
    {
        std::map<std::string, std::string> crudo;
        {
            std::istringstream ss(params);
            std::string tok;
            while (ss >> tok)
            {
                const size_t eq = tok.find('=');
                if (eq == std::string::npos)
                {
                    if (desconocidas) desconocidas->push_back(tok);
                    continue;
                }
                // La CLAVE es vocabulario del motor y pliega; el VALOR no se
                // toca — puede ser un número, y en `medida=` es vocabulario
                // que hoy resuelve `parseFontMeasure` byte a byte, igual que
                // en el `.ecl`. Plegarlo acá haría divergir los dialectos por
                // el otro lado (ver el control de frontera del harness).
                const std::string k = vocabToLower(tok.substr(0, eq));
                if (crudo.count(k)) continue;   // repetida: gana la primera
                crudo[k] = tok.substr(eq + 1);
            }
        }

        std::map<std::string, std::string> out;
        std::vector<std::string> consumidas;
        for (const auto& grupo : gruposParametro())
        {
            std::vector<std::string> presentes;
            for (const auto& g : grupo)
                if (crudo.count(g)) presentes.push_back(g);
            if (presentes.empty()) continue;
            out[grupo[0]] = crudo.at(presentes[0]);
            for (const auto& g : presentes) consumidas.push_back(g);
            if (presentes.size() > 1 && choques) choques->push_back(presentes[0]);
        }
        for (const auto& kv : crudo)
        {
            bool usada = false;
            for (const auto& g : consumidas) if (g == kv.first) usada = true;
            if (!usada && desconocidas) desconocidas->push_back(kv.first);
        }
        return out;
    }

    // ── LA VALIDACION, y es la unica ─────────────────────────────────────
    // Recibe valores ya tipados para que cada dialecto conserve su propio
    // lector de numeros; lo que NO se duplica es que un tamano ausente o no
    // positivo descarta la declaracion completa, que el kerning ausente vale
    // 0 y que la medida legacy es `altura`.
    inline Error construirDesdeValores(const std::string& nombre,
                                       const std::string& ruta,
                                       f32 size, f32 kerning,
                                       FontMeasure measure,
                                       bool measureSpecified,
                                       Declaracion& out)
    {
        if (nombre.empty() || ruta.empty()) return Error::FaltaNombreORuta;
        if (!(size > 0.0f) || size != size) return Error::TamanoInvalido;

        out = Declaracion{};
        out.name = nombre;
        out.routes.push_back(ruta);
        out.size = size;
        out.kerning = kerning;
        out.measure = measure;
        out.measureSpecified = measureSpecified;
        return Error::Ninguno;
    }

    // Igual que la anterior, leyendo los tres parametros del mapa canonico.
    inline Error construir(const std::string& nombre, const std::string& ruta,
                           const std::map<std::string, std::string>& params,
                           Declaracion& out,
                           std::string* detalle = nullptr)
    {
        if (detalle) detalle->clear();
        f32 size = 0.0f;
        {
            const auto it = params.find(claveTamano());
            if (it == params.end()) return Error::TamanoInvalido;
            float v = 0.0f;
            if (parseFloatStrict(trimBlancos(it->second), v) != NumParse::Ok)
                return Error::TamanoInvalido;
            size = v;
        }

        f32 kerning = 0.0f;
        {
            const auto it = params.find(claveEspaciado());
            if (it != params.end())
            {
                float v = 0.0f;
                if (parseFloatStrict(trimBlancos(it->second), v) == NumParse::Ok)
                    kerning = v;
            }
        }

        FontMeasure measure = FontMeasure::Height;
        bool measureSpecified = false;
        {
            const auto it = params.find(claveMedida());
            if (it != params.end())
            {
                if (!parseFontMeasure(trimBlancos(it->second), measure))
                {
                    if (detalle) *detalle = it->second;
                    return Error::MedidaInvalida;
                }
                measureSpecified = true;
            }
        }

        return construirDesdeValores(nombre, ruta, size, kerning, measure,
                                     measureSpecified, out);
    }

    // ── La linea hija: `respaldo "<ruta>"` / `fallback "<ruta>"` ──────────
    // Devuelve false si la linea no es un respaldo BIEN FORMADO. `soloPalabra`
    // dice si al menos la palabra era la correcta, para que el dialecto pueda
    // separar "esto no es un respaldo" de "es un respaldo mal escrito".
    inline bool partirRespaldo(const std::string& line, std::string& rutaOut,
                               bool* soloPalabra = nullptr)
    {
        if (soloPalabra) *soloPalabra = false;
        const std::string s = trimBlancos(line);
        const size_t sp = s.find(' ');
        if (sp == std::string::npos) return false;
        if (!esRespaldo(s.substr(0, sp))) return false;
        if (soloPalabra) *soloPalabra = true;

        const std::string resto = trimBlancos(s.substr(sp + 1));
        if (resto.size() < 2 || resto.front() != '"') return false;
        const size_t q2 = resto.find('"', 1);
        if (q2 == std::string::npos) return false;
        // Cola despues de la comilla de cierre: invalido, no se adivina.
        if (!trimBlancos(resto.substr(q2 + 1)).empty()) return false;
        rutaOut = resto.substr(1, q2 - 1);
        return !rutaOut.empty();
    }

    // ¿La primera palabra de la linea es `respaldo`/`fallback`? Lo usan los
    // dos dialectos para dar voz contextual dentro del bloque.
    inline bool lineaEmpiezaConRespaldo(const std::string& line)
    {
        const std::string s = trimBlancos(line);
        const size_t sp = s.find(' ');
        return esRespaldo(sp == std::string::npos ? s : s.substr(0, sp));
    }

} // namespace fontdecl
} // namespace eclips
