// ============================================================================
// EclipsParse.h — Parsing bilingüe centralizado de Eclips
// ============================================================================
//
// Hogar único del parseo de booleanos del motor. Nace de §1.45 de
// PATRONES_GOTCHAS: `parseBool` estaba duplicado ~17 veces (Theme,
// ProjectManifest, EclipScript, MapData) con listas de afirmativos
// INCONSISTENTES — algunos aceptaban `1`, otros `on`, casi ninguno `sí`.
// La misma decisión escrita N veces se desincroniza (hermano de §1.6).
//
// La cura NO es repetir la generosidad en cada lugar; es CENTRALIZAR aquí
// + AVISAR cuando el valor está presente pero no se reconoce. El warning
// elimina la clase entera del gotcha del silencio (un `wall_slide perro`
// ya no cae a `false` calladito: deja rastro).
//
// Dos disciplinas heredadas de la sesión `captura-nombre` (B2). La primera
// sigue en pie; la segunda estuvo AL REVÉS durante meses y la corrigió
// SYN-4c-i (2026-07-30) — se conservan las dos escritas porque la que falló
// es la que enseña:
//
//   1. UTF-8-safe. El bug raíz de §1.45 era un `tolower` byte-a-byte: la
//      `í` de `sí` es multibyte (0xC3 0xAD) y foldear byte-a-byte la
//      corrompe. Acá `asciiToLower` toca SOLO A-Z y deja intactos los
//      bytes >0x7F — nunca rompe UTF-8. **Sigue siendo cierto, y por eso el
//      fold del español entró como HERMANO (`vocabToLower`) y no bajando a
//      esta función**: `asciiToLower` es la puerta de 27 sitios, y algunos
//      resuelven nombres que el creador inventa, donde `MAÑANA` y `mañana`
//      son dos ids distintos **a propósito**.
//
//   2. ~~Literal `u8"sí"`. Garantiza los bytes UTF-8~~ — **FALSO, y era el
//      bug.** El prefijo `u8` no garantiza: RE-CODIFICA lo que el compilador
//      ya decodificó. Con este archivo en UTF-8 sin BOM y MSVC sin `/utf-8`,
//      `u8"sí"` compilaba a CINCO bytes (73 C3 83 C2 AD) contra los TRES que
//      trae un `.ecl` real (73 C3 AD), así que **`sí` nunca fue afirmativo**.
//      La forma que sí garantiza los bytes es ESCRIBIRLOS (`"s\xC3\xAD"`),
//      que es lo que §1.60 ya había firmado para `tamaño=` y esta línea no
//      alcanzó a heredar. Regla: en este repo, **un literal con un byte
//      no-ASCII se escribe en hexadecimal, nunca con `u8`**.
//
// Modelo de 3 estados (Fork B): `parseBoolStrict` distingue True / False /
// Unrecognized — sin asumir default, sin avisar. El wrapper `parseBool`
// pone el default y el warning encima (cubre el 95% de los sitios en una
// línea). El `context` del warning lo arma el caller (Fork D): el helper
// no impone formato — campo+línea donde se tenga, campo+path donde no.
//
// Destino futuro del aviso (MODO_CREADOR §12.3): toast rojo pedagógico en
// pantalla, no `cerr`. Por ahora `cerr`, como el resto de avisos del motor.
// ============================================================================

#pragma once

#include <string>
#include <iostream>
#include <cmath>     // isfinite — la finitud es parte de la ley del numero
#include <cstddef>
#include <vector>

namespace eclips
{
    // tolower SOLO sobre A-Z. Deja intactos los bytes >0x7F → nunca rompe
    // secuencias UTF-8. (No usar std::tolower/std::transform sobre el string
    // entero: esa es la causa raíz de §1.45.)
    inline std::string asciiToLower(std::string s)
    {
        for (char& c : s)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        return s;
    }

    // Gemelo hacia arriba, misma disciplina. Nace en SYN-1c: la ficha (11)
    // culpaba SOLO a `EclipParser::toLower` del C4244 del header, y el
    // compilador nombró dos — el otro es el `std::transform(..., ::toupper)`
    // que normaliza la `tecla:` de `elegir`/`qte` en `parseBlockParams`. Misma
    // familia §1.45, mismo UB (pasar un `char` negativo a `::toupper`), y
    // `asciiToLower` llevaba desde entonces sin hermano hacia arriba.
    inline std::string asciiToUpper(std::string s)
    {
        for (char& c : s)
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - 'a' + 'A');
        return s;
    }

    // ── EL FOLD DEL VOCABULARIO DEL MOTOR (SYN-4c-i, ficha Ñ2) ──────────────
    //
    // `asciiToLower` + el juego español (ÑÁÉÍÓÚÜ). HERMANO de `asciiToLower`,
    // NO un reemplazo — y esa distinción es la ley de este lote, no un detalle
    // de implementación:
    //
    //   >>> ESTO SE APLICA AL VOCABULARIO DEL **MOTOR**, NUNCA A LO QUE EL
    //   >>> CREADOR INVENTA.
    //
    // Porque en lo del creador `MAÑANA` y `mañana` son DOS ids distintos — eso
    // está medido (Ñ1, 2026-07-24) y prometido en el manual. Foldear ahí los
    // volvería el mismo, que es lo contrario de la política. Medido al abrir
    // esta sesión: `asciiToLower` es la puerta de 22 `toLower` del parser MAS 5
    // sitios de `MapData.h`, y al menos uno de los 22 resuelve un nombre que el
    // creador inventa (la receta de `animar <alias> <receta>`). Por eso el fold
    // entra como hermano y se cablea en TRES puertas elegidas a mano, en vez de
    // bajar a `asciiToLower` y llegar gratis a las veintisiete.
    //
    // ── QUE ARREGLA, MEDIDO ─────────────────────────────────────────────────
    //
    // El vocabulario del motor es ASCII salvo DOS palabras (censo de Ñ1 +
    // el de esta sesión): la clave `tamaño=` y el valor booleano `sí`. Las dos
    // fallaban en mayúscula, y `asciiToLower` es A-Z a propósito (§1.45: pasar
    // bytes UTF-8 a `::tolower` es UB y con otra locale corrompe el texto).
    // Una raíz, y el manual las documentaba como dos notas separadas.
    //
    // ── LA TRAMPA DEL RANGO, Y POR QUE ESTO ENUMERA EN VEZ DE SUMAR ─────────
    //
    // Las mayúsculas acentuadas del español viven en C3 81..C3 9C y sus
    // minúsculas están exactamente +0x20 más arriba, así que un `if (c == 0xC3
    // && d >= 0x80 && d <= 0x9E) d += 0x20` parece la forma corta. NO lo es:
    // C3 97 es `×` (signo de multiplicación) y C3 B7 es `÷`, que NO son un par
    // de mayúscula/minúscula. Un rango convertiría `×` en `÷` calladito. Por
    // eso acá van las SIETE letras enumeradas: el juego español y nada más.
    inline std::string vocabToLower(std::string s)
    {
        for (size_t i = 0; i < s.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c >= 'A' && c <= 'Z')
            {
                s[i] = static_cast<char>(c - 'A' + 'a');
                continue;
            }
            if (c != 0xC3 || i + 1 >= s.size())
                continue;

            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            switch (d)
            {
                case 0x81:  // A con tilde
                case 0x89:  // E con tilde
                case 0x8D:  // I con tilde
                case 0x91:  // N con virgulilla
                case 0x93:  // O con tilde
                case 0x9A:  // U con tilde
                case 0x9C:  // U con dieresis
                    s[i + 1] = static_cast<char>(d + 0x20);
                    break;
                default:
                    break;  // `×` (C3 97) y todo lo demás: intacto.
            }
            ++i;            // el par UTF-8 ya se consumió, entero.
        }
        return s;
    }

    // Estado del parseo. Unrecognized se distingue de False A PROPÓSITO:
    // es lo que habilita el warning anti-silencio (distinguir un `no`
    // legítimo de basura como `xyz`).
    enum class BoolParse { True, False, Unrecognized };

    // Núcleo. NO asume default, NO avisa — el caller decide qué hacer con
    // Unrecognized. Listas canónicas (unión de todas las que había + `off`):
    //   afirmativo: 1 true si sí yes on
    //   negativo:   0 false no off
    inline BoolParse parseBoolStrict(const std::string& s)
    {
        // PUERTA 1 de 3 del fold (SYN-4c-i). Un valor booleano es vocabulario
        // del MOTOR — `si`/`yes`/`on` no los inventa nadie —, asi que aca el
        // fold es correcto y hace que `SI` con tilde mayuscula matchee.
        const std::string v = vocabToLower(s);
        //
        // ── LA TILDE VA EN BYTES EXPLICITOS, Y ERA UN BUG VIVO ──────────────
        // Aca decia `u8"sí"`, y estaba DOBLE-ENCODEADO: este archivo es UTF-8
        // SIN BOM, MSVC lo lee como ANSI y el prefijo `u8` re-codifica lo que
        // ya era UTF-8. Medido con sonda de bytes (2026-07-30):
        //
        //     disco (lo que trae un .ecl)   len=3   73 C3 AD
        //     u8"sí" tal cual estaba        len=5   73 C3 83 C2 AD
        //
        // O sea que `si` con tilde NUNCA fue afirmativo, ni en minuscula:
        // caia a Unrecognized, al default, con aviso. Es la MISMA especie que
        // SYN-1c-ii cazo en `tamaño=` (§1.60), sobrevivida aca porque este
        // primitivo no tenia un solo assert en la bateria — el unico de
        // `EclipsParse.h` sin harness. Y pegaba en el peor lugar posible:
        // `ECLIPSCRIPT_DOCS` promete `sí` como afirmativo *y ademas* lo receta
        // como la salida del hueco de mayusculas acentuadas.
        //
        // La disciplina 2 de la cabecera de este archivo decia que `u8` era la
        // garantia. Era exactamente al reves: `u8` es lo que rompia. La forma
        // que garantiza los bytes es escribirlos.
        if (v == "1" || v == "true" || v == "si" || v == "s\xC3\xAD" ||
            v == "yes" || v == "on")
            return BoolParse::True;
        if (v == "0" || v == "false" || v == "no" || v == "off")
            return BoolParse::False;
        return BoolParse::Unrecognized;
    }

    // Wrapper de conveniencia. Reconocido → su valor. No reconocido →
    // 'defaultVal', y si 'context' != nullptr (y el valor no venía vacío)
    // deja un aviso a cerr. Un valor vacío = "campo presente sin valor /
    // no especificado" → cae al default en silencio (avisar de "" sería
    // ruido en cada campo opcional no escrito).
    //
    // Firma ÚNICA a propósito: siempre 'defaultVal' explícito. Una sobrecarga
    // de 2 args con `const char*` crearía ambigüedad (un puntero convierte a
    // bool), así que los sitios que siempre caen a false llaman
    //   parseBool(val, false, "...")
    // Explícito, sin trampa.
    inline bool parseBool(const std::string& s, bool defaultVal,
                          const char* context = nullptr)
    {
        switch (parseBoolStrict(s))
        {
            case BoolParse::True:  return true;
            case BoolParse::False: return false;
            default: // Unrecognized
                if (context && !s.empty())
                    std::cerr << "[Eclips] Aviso: valor booleano '" << s
                              << "' no reconocido en " << context
                              << " — usando " << (defaultVal ? "si" : "no")
                              << std::endl;
                return defaultVal;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // NÚMEROS — el gemelo numérico de parseBool (SYN-1a, Familia §1.45)
    // ═══════════════════════════════════════════════════════════════════════
    //
    // Misma enfermedad, otra magnitud. `parseBool` nació porque la misma
    // decisión escrita N veces se desincroniza; el número nunca tuvo su cura:
    // medidos **61 `std::stof`/`std::stoi` en `EclipScript.h`, CERO con guarda
    // de consumo total**, y ~45 de ellos son este idioma copiado:
    //
    //     try { if (kv.count("x")) cmd.layerX = std::stof(kv["x"]); } catch (...) {}
    //
    // Dos agujeros en una línea: `x=perro` cae al default **mudo**, y
    // `x=1.5abc` entra como 1.5 — el prefijo numérico se acepta como éxito.
    // Es la familia del `std::stoi(…, 16)` que `quick-vn` cerró en el hex
    // (`#0000FZ` daba azul 15/255 en silencio) y la de R-VN-03 de la auditoría
    // de sintaxis.
    //
    // Tres leyes, firmadas en mesa (sesión `sintaxis-syn1a`):
    //
    //   1a) Basura → descartar + avisar, el campo queda en su default. Es la
    //       ley que `parseBool` ya tiene acá abajo; SYN-1a la copia, no
    //       inventa una. **Fail-closed (no emitir el comando) es el gate de
    //       SYN-2** y no se adelanta acá.
    //   2a) El núcleo valida FORMA, no dominio: consumo total + finitud. Los
    //       rangos (`w` no negativo, `tamaño` > 0) los pone el caller, cuando
    //       exista el caller que los pida (§1.44 first-caller, §7.49 "el
    //       descriptor arranca mínimo").
    //   3a) Un campo ENTERO rechaza el decimal: `z=1.5` no vale 1. Es el gate
    //       literal de SYN-1. Medido antes de firmarlo: cero ocurrencias de
    //       `z=` decimal en los 96 `.ecl` de `vnscript/`.
    //
    // El modelo de 3 estados es el mismo de `parseBoolStrict`, y por la misma
    // razón: **`Empty` NO es `Malformed`**. Un campo opcional que nadie
    // escribió no puede sonar igual que un campo escrito con basura — de esa
    // distinción cuelga el silencio del wrapper (avisar de `""` sería ruido en
    // cada parámetro no escrito).
    //
    // Sellado en `Resumenes_Sesiones/Harness/harness_num.cpp`.

    enum class NumParse { Ok, Empty, Malformed };

    // El token llega YA TRIMEADO — un espacio en cualquier extremo es
    // Malformed A PROPÓSITO. Sin esta regla la ley sería asimétrica: `strtod`
    // se come el espacio de la IZQUIERDA y no el de la derecha, así que `" 5"`
    // pasaría y `"5 "` no. Antes que dos comportamientos, uno explícito. (Y
    // evita un `trim` duplicado acá: el que hay vive en el caller — §1.45 en
    // miniatura.)
    inline bool numTokenHasEdgeSpace(const std::string& s)
    {
        auto isSp = [](char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        };
        return !s.empty() && (isSp(s.front()) || isSp(s.back()));
    }

    // `stof` delega en `strtod`, y `strtod` parsea HEX FLOATS: `0x10` da 16
    // **con consumo total**, así que pasa el corazón de la ley de abajo.
    // Medido, no supuesto — el assert se escribió sin esta guarda y salió rojo
    // en MSVC 14.51. Es el primo del prefijo `0x` que `quick-vn` cazó en el
    // parser de hex, pero peor: aquel moría en el consumo total, éste lo pasa.
    // Se rechaza por VOCABULARIO: `x=0x10` no es sintaxis de EclipScript, y un
    // creador que lo escribe se equivocó de idioma, no de valor.
    inline bool numTokenHasHexPrefix(const std::string& s)
    {
        std::size_t i = 0;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        return i + 1 < s.size() && s[i] == '0' &&
               (s[i + 1] == 'x' || s[i + 1] == 'X');
    }

    // Núcleo float. NO asume default, NO avisa — el caller decide.
    // `out` queda INTACTO salvo que el resultado sea Ok: el caller puede
    // pasarle el campo directo y confiar en que la basura no lo pisa.
    inline NumParse parseFloatStrict(const std::string& s, float& out)
    {
        if (s.empty())               return NumParse::Empty;
        if (numTokenHasEdgeSpace(s)) return NumParse::Malformed;
        if (numTokenHasHexPrefix(s)) return NumParse::Malformed;

        float       v        = 0.0f;
        std::size_t consumed = 0;
        try
        {
            v = std::stof(s, &consumed);
        }
        catch (...)   // invalid_argument (no es número) | out_of_range (1e999)
        {
            return NumParse::Malformed;
        }

        // EL corazón: `stof` devuelve éxito habiendo consumido solo el
        // prefijo. Sin esto, `8patos` son 8 y `100px` son 100.
        if (consumed != s.size()) return NumParse::Malformed;

        // `nan`/`inf` SÍ pasan el consumo total — son tokens que `stof`
        // reconoce enteros. Envenenarían el layout sin decir nada.
        if (!std::isfinite(v)) return NumParse::Malformed;

        out = v;
        return NumParse::Ok;
    }

    // Núcleo int. Base 10 EXPLÍCITA: con base 0 el `0x` y el octal entrarían
    // por la puerta de atrás (el mismo prefijo que mordió al hex en
    // `quick-vn`). Con base 10 + consumo total, `0x10` muere en la 'x' y
    // `010` vale diez, que es lo que el creador escribió.
    inline NumParse parseIntStrict(const std::string& s, int& out)
    {
        if (s.empty())               return NumParse::Empty;
        if (numTokenHasEdgeSpace(s)) return NumParse::Malformed;

        int         v        = 0;
        std::size_t consumed = 0;
        try
        {
            v = std::stoi(s, &consumed, 10);
        }
        catch (...)
        {
            return NumParse::Malformed;
        }

        // Acá vive la ley 3a: `stoi("1.5")` consume "1" y devuelve 1 tan
        // campante. El `.5` sobrante es lo que la delata.
        if (consumed != s.size()) return NumParse::Malformed;

        out = v;
        return NumParse::Ok;
    }

    // ── Wrappers con default + voz ─────────────────────────────────────────
    // Espejo exacto de `parseBool`: reconocido → su valor; no reconocido →
    // `defaultVal`, y si hay `context` deja aviso. Vacío → default en
    // silencio. El `context` lo arma el caller (campo + línea donde se tenga).
    inline float parseFloat(const std::string& s, float defaultVal,
                            const char* context = nullptr)
    {
        float v = defaultVal;
        switch (parseFloatStrict(s, v))
        {
            case NumParse::Ok:    return v;
            case NumParse::Empty: return defaultVal;
            default:
                if (context)
                    std::cerr << "[Eclips] Aviso: valor numerico '" << s
                              << "' invalido en " << context
                              << " — usando " << defaultVal << std::endl;
                return defaultVal;
        }
    }

    inline int parseInt(const std::string& s, int defaultVal,
                        const char* context = nullptr)
    {
        int v = defaultVal;
        switch (parseIntStrict(s, v))
        {
            case NumParse::Ok:    return v;
            case NumParse::Empty: return defaultVal;
            default:
                if (context)
                    std::cerr << "[Eclips] Aviso: valor entero '" << s
                              << "' invalido en " << context
                              << " — usando " << defaultVal << std::endl;
                return defaultVal;
        }
    }

    // ── truncateCodepoints — recorte UTF-8-safe (Familia §1.45) ─────────────
    //
    // Los límites del motor cuentan CARACTERES, no bytes: la misma ley que el
    // `max` de `entrada` (§1.45 — la `í` de `sí` son DOS bytes; tratarlos como
    // unidades es el bug raíz de esta familia). Un `substr(0, N)` crudo parte
    // una `é` al medio y deja un byte huérfano: mojibake en pantalla y, si el
    // recorte ocurre al SERIALIZAR, mojibake grabado en el disco.
    //
    // El bug es INTERMITENTE por naturaleza: cuando el byte N cae en frontera
    // por casualidad, el corte crudo acierta. Por eso sobrevivió tanto.
    //
    // Corta en frontera de codepoint: nunca parte un carácter.
    //   maxCp <= 0        → sin límite (identidad).
    //   <= maxCp glifos   → identidad (devuelve la cadena entera).
    //   bytes inválidos o secuencia truncada → degradan a 1 byte = 1 glifo (no
    //                       cuelga, no lee fuera de rango — misma tolerancia
    //                       defensiva que `splitCodepoints`).
    // La salida SIEMPRE es prefijo de la entrada, y UTF-8 válido si la entrada
    // lo era.
    inline std::string truncateCodepoints(const std::string& s, int maxCp)
    {
        if (maxCp <= 0 || s.empty()) return s;

        int    n = 0;      // glifos consumidos
        size_t i = 0;      // byte actual
        while (i < s.size())
        {
            if (n == maxCp) return s.substr(0, i);   // cupo lleno: cortar acá

            unsigned char c = static_cast<unsigned char>(s[i]);
            size_t len = 1;
            if ((c & 0x80) == 0x00) len = 1;    // ASCII
            else if ((c & 0xE0) == 0xC0) len = 2;    // 2 bytes (é, ñ, ¿)
            else if ((c & 0xF0) == 0xE0) len = 3;    // 3 bytes (…, comillas)
            else if ((c & 0xF8) == 0xF0) len = 4;    // 4 bytes (emoji)
            if (i + len > s.size()) len = s.size() - i;   // truncado: clamp

            i += len;
            ++n;
        }
        return s;   // cabe entera
    }

    // ── rtrimSpaces — poda de espacios colgantes (compañera del recorte) ────
    //
    // Un corte a N glifos aterriza a menudo JUSTO en un espacio (el espacio es
    // un glifo como cualquier otro), y el consumidor lo delata: el menú envuelve
    // el preview en comillas ("...voló ") y el `ranura` le pega la elipsis
    // ("...voló …"). El hueco se ve. Se poda ACÁ, en el hogar del recorte, para
    // que todos los consumidores hereden la misma decisión (§1.40).
    //
    // Solo espacio y tab ASCII. Nada de `\n`/`\r` (no sobreviven al piso 1) ni
    // de espacios Unicode (sin caller — §7.118).
    //
    // ORDEN AL COMPONER (gotcha real, verificado en harness): si el caller
    // decide "¿trunqué?" comparando tamaños, la comparación va ANTES del rtrim.
    // Si no, una cadena que cabía entera pero terminaba en espacio se encoge, el
    // caller cree que truncó y le pega una elipsis MENTIROSA: el trim se
    // disfraza de recorte.
    inline std::string rtrimSpaces(const std::string& s)
    {
        size_t end = s.size();
        while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t')) --end;
        return s.substr(0, end);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // LOS DOS HELPERS QUE ERAN PRIVADOS DEL PARSER (`mutar-cp0`, 2026-08-15)
    // ═══════════════════════════════════════════════════════════════════════
    //
    // `EclipParser::trim` y `EclipParser::extractQuoted` vivían como `static`
    // privados adentro de la clase, así que **nada fuera de `EclipScript.h`
    // podía llamarlos** — ni un harness, ni un header de `core/`. Eso es lo que
    // obligaba a que cualquier ley nueva que necesitara el reparto
    // `var op valor` los REPLICARA, que es la enfermedad de §7.309-314.
    //
    // Suben acá con el cuerpo **verbatim** y los de adentro de la clase quedan
    // como reenvío de una línea: cero call-sites movidos, cero conducta nueva,
    // una sola implementación. El guardián de no-op de `harness_mutar` §1 es
    // lo que sostiene ese "verbatim" en el tiempo.
    //
    // Llevan nombre propio (no `trim`/`extractQuoted` pelados) a propósito: hay
    // otras dos copias de `trim` vivas —`Theme.h:1861` y
    // `ProjectManifest.h:51`— y un nombre corto en `namespace eclips` las
    // volvería ambiguas para cualquiera que haga `using namespace`. Unificar
    // esas dos es su propio ticket (ficha `TRIM-TRIPLICADO`), no éste.

    // Poda blancos de los DOS extremos. Cuerpo verbatim de `EclipParser::trim`,
    // incluidos `\r` y `\n` — que importan: el `.ecl` puede venir en CRLF.
    inline std::string trimBlancos(const std::string& s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    // Extrae el contenido del PRIMER par de comillas. Cuerpo verbatim de
    // `EclipParser::extractQuoted`. Devuelve false si no hay par completo, y en
    // ese caso NO toca `out` — de eso depende que el caller pueda distinguir
    // "no había comillas" de "había un literal vacío".
    inline bool extraerEntreComillas(const std::string& s, std::string& out)
    {
        size_t q1 = s.find('"');
        if (q1 == std::string::npos) return false;
        size_t q2 = s.find('"', q1 + 1);
        if (q2 == std::string::npos) return false;
        out = s.substr(q1 + 1, q2 - q1 - 1);
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // FRONTERA DE KEYWORD — el despacho no confunde `qte` con `qte_extra`
    // (SYN-1b de la auditoría de sintaxis; R-VN-04, la pierna de "frontera")
    // ═══════════════════════════════════════════════════════════════════════
    //
    // El parser despacha comandos con `startsWith(line, "qte")`, y `startsWith`
    // no mira lo que viene DESPUÉS: `qte_extra` cae en la rama de QTE y
    // `transicionfoo` se parsea como `transicion foo`. Un keyword pegado a más
    // texto de identificador se traga la línea entera como si fuera el comando.
    //
    // La cura NO son 4 parches (uno por sitio): es una sola ley de FRONTERA. Un
    // keyword `kw` matchea solo si termina en frontera de palabra — o sea, el
    // carácter que le sigue NO continúa un identificador, o es fin de línea.
    // Caracteres de identificador: [A-Za-z0-9_]. Todo lo demás (espacio, `[`,
    // `:`, fin de línea) es frontera legítima, y con eso alcanza para las tres
    // formas que los comandos usan de verdad: pelado (`transicion` → fade), con
    // argumento (`transicion disolver`) y header de bloque (`qte [tecla:E]`,
    // `elegir:`).
    //
    // El precedente ya vivía en el código: `vigneta` se despacha con
    // `startsWith("vigneta ") || line == "vigneta"` — espacio-o-exacto, la misma
    // idea a mano. Esto la generaliza (un solo helper, y además admite el `[`/`:`
    // que los headers de bloque necesitan) en vez de copiarla 4 veces.
    //
    // ALCANCE, firmado en mesa (Opción 3): esta ley mata SOLO el pegado. NO toca
    // la honestidad de la COLA: `transicion:` sigue matcheando (el `:` es
    // frontera) y su handler todavía guarda tipo=":" — ese es el colon cosmético
    // de R-VN-01, que hoy vive igual en `escena config:`, y se arregla parejo
    // cuando toque R-VN-01, no a pedazos acá. La frontera es la frontera; la cola
    // es otro lote. (Es, además, la definición estándar de "word boundary" que
    // SYN-1c va a formalizar en el lexer.)
    //
    // Sellado en `Resumenes_Sesiones/Harness/harness_frontera.cpp`.

    inline bool isKeywordChar(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    }

    // `line` llega YA left-trimeada (misma precondición que `startsWith`, el
    // sitio de llamada). Devuelve true si `line` invoca al comando `kw`.
    inline bool matchKeyword(const std::string& line, const std::string& kw)
    {
        const std::size_t n = kw.size();
        if (line.compare(0, n, kw) != 0) return false;      // no empieza con kw
        return line.size() == n || !isKeywordChar(line[n]); // EOL o frontera
    }

    // ═══════════════════════════════════════════════════════════════════════
    // COMENTARIO INLINE — `comando ... # explicación` (SYN-1c / R-VN-02)
    // ═══════════════════════════════════════════════════════════════════════
    //
    // Hasta acá, el parser solo ignoraba una línea si su PRIMER carácter no
    // blanco era `#`. Un comentario al final de una instrucción no existía, y
    // el manual lo enseñaba igual — en cuatro renglones que no parseaban
    // (`esperar_animacion  # ...`, `iniciar_eventos  # ...`, `detener_eventos`,
    // `meta_borrar`) y en un `.ecl` VIVO: `vnscript/menu.ecl:16` decía
    // `ocultar_caja                      # menú sin barra de diálogo`, y como
    // el despacho de ese comando es `line == "ocultar_caja"`, la línea caía a
    // "no reconocida" y la caja NUNCA se ocultaba en el menú.
    //
    // Y el saneamiento numérico lo puso PEOR, a propósito: la auditoría decía
    // que `esperar 1 # nota` "parece funcionar porque `std::stof` acepta el
    // prefijo". Desde CP0/SYN-1a `parseDurationSpec` exige consumo total, así
    // que hoy avisa e ignora el comando entero.
    //
    // ── EL CHOQUE: `#` no es una sola cosa en este motor ────────────────────
    //
    // El `#` abre un comentario Y abre un color hex. Medido en los cuatro
    // parsers de color del motor, el reparto NO es parejo:
    //
    //   · `EclipParser::parseHexColor`, `TextRenderer::parseHex6_local` y
    //     `MapData::parseHexColor` → el `#` es OPCIONAL. Lo pelan y siguen:
    //     en un `.ecl`, `color=FF3399` ya funciona hoy. El `#` es decoración.
    //   · `Theme::parseColor` (`.eclips`) → el `#` es OBLIGATORIO, y es lo
    //     ÚNICO que distingue un hex del formato float `0.0-1.0` legacy. Ahí
    //     sí carga información — pero eso es OTRO parser y OTRO formato de
    //     archivo (el corolario de §7.316: dos tokens que se parecen no
    //     compiten si no comparten handler).
    //
    // O sea: la pelea `#`-comentario vs `#`-color es real en `.eclips` y es
    // DECORATIVA acá. Esta ley vive del lado decorativo.
    //
    // ── LA LEY, firmada en mesa (Opción B2, por POSICIÓN) ───────────────────
    //
    //     Un `#` abre comentario si está FUERA DE COMILLAS y viene precedido
    //     de espacio/tab o del inicio de la línea. Un `#` PEGADO a lo que
    //     tiene a la izquierda es parte del token que se está escribiendo.
    //
    // En una frase para el creador: *"un `#` con espacio antes es un
    // comentario; pegado, es parte de lo que estás escribiendo"*.
    //
    // Las dos condiciones hacen falta, y el corpus lo prueba con un caso cada
    // una: `color=#FF3399` se salva por la POSICIÓN (el `#` viene tras un
    // `=`), y `"Test del bug #3: fases de guardado."` se salva por las
    // COMILLAS (ahí el `#` SÍ viene tras un espacio).
    //
    // ── POR QUÉ NO LA OTRA (Opción B3, por CONTENIDO) ───────────────────────
    //
    // `Theme.h` ya trae un stripper hex-aware: corta en el `#` salvo que le
    // siga un dígito hex. Se evaluó copiarlo y se DESCARTÓ:
    //
    //   1. Mira el CONTENIDO para decidir la ESTRUCTURA — el lexer tendría que
    //      saber qué es un color. Este helper no sabe y no tiene que saber.
    //   2. Hereda su hueco: con esa ley `#final`, `#abc` y `#3` no son
    //      comentarios, porque `f`/`a`/`3` son dígitos hex. Un creador no
    //      puede predecir eso.
    //   3. Rompería una conducta viva: un `#FACADE` solo en su línea es un
    //      comentario HOY (el `line[0] == '#'`), y con la ley por contenido
    //      dejaría de serlo.
    //
    // La ley por posición, en cambio, SUBSUME la regla vieja de línea completa:
    // "inicio de línea" es un caso de "precedido de espacio". Una ley donde
    // había dos.
    //
    // ── MODELO DE COMILLAS, y su arista sellada ────────────────────────────
    //
    // Toggle simple: cada `"` alterna dentro/fuera. El motor no tiene escape
    // `\"` (el único escape vivo es el `\n` de la consola), así que no hay nada
    // que escapar.
    //
    // Con comillas IMPARES (literal sin cerrar) el toggle queda "adentro" hasta
    // el fin de línea → NO se corta nada. Es lo correcto: la línea llega entera
    // a su handler, que ya avisa "comilla sin cerrar" (fail-closed de §6 de la
    // auditoría, intacto).
    //
    // ARISTA CONOCIDA, cero ocurrencias vivas: `extractQuotedGreedy` usa
    // primera-a-última comilla, no toggle. Con comillas ANIDADAS los dos
    // modelos discrepan sobre qué está "adentro" — en
    // `m: "él dijo "hola # mundo" y ya"` el toggle dice que ese `#` está fuera
    // y corta. Medido en el corpus: las únicas 5 líneas con más de dos comillas
    // Y un `#` son comentarios de línea COMPLETA, que se cortan en el índice 0
    // antes de que el estado de comillas importe. Sellado como assert.
    //
    // ── CONTRATO ────────────────────────────────────────────────────────────
    //
    //   · sin comentario  → devuelve la línea IDÉNTICA, byte por byte;
    //   · con comentario  → devuelve el prefijo hasta el `#`, sin los espacios
    //                       colgantes que quedaron (rtrim solo si se cortó);
    //   · `# ...` en la columna 0 → devuelve "" (línea completa de comentario).
    //
    // Se aplica UNA vez, al leer el archivo, no en el despacho: `EclipScript.h`
    // tiene CUATRO lectores del vector de líneas —el loop principal y los
    // look-ahead de `elegir`, `qte` y `esperar_click`, que leen `lines[i+1]`
    // crudo—, así que pelarlo en el loop dejaría a los hijos de un bloque con
    // su comentario puesto.
    //
    // Sellado en `Resumenes_Sesiones/Harness/harness_comentarios.cpp`.

    inline bool isInlineCommentSpace(char c)
    {
        return c == ' ' || c == '\t';
    }

    // `line` llega CRUDA del archivo (sin trimear) — es el primer paso del
    // pipeline, antes que nadie la toque.
    inline std::string stripInlineComment(const std::string& line)
    {
        bool inQuotes = false;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            const char c = line[i];
            if (c == '"') { inQuotes = !inQuotes; continue; }
            if (c != '#' || inQuotes) continue;

            // Fuera de comillas: ¿está PEGADO a algo, o suelto?
            if (i != 0 && !isInlineCommentSpace(line[i - 1])) continue;

            return rtrimSpaces(line.substr(0, i));
        }
        return line;   // sin comentario → identidad
    }

    // ── SYN-3 / 1c-iii · PARTIR RESPETANDO LAS COMILLAS ─────────────────────
    //
    // `std::getline(stream, token, ',')` no sabe nada de comillas, así que
    // `default: "No, gracias"` se partía en `default: "No` y `gracias"` — y el
    // primero moría en `extractQuoted` (una sola comilla → false, y no escribe
    // nada), o sea que el valor desaparecía ENTERO y en silencio. Cero
    // ocurrencias vivas: es un *"no lo podés escribir"*, no un *"tu script está
    // roto"* — pero `ECLIPSCRIPT_DOCS:1660` promete literalmente que `default:`
    // "acepta varias palabras entre comillas", que es cierto hasta la primera
    // coma.
    //
    // Misma máquina de comillas que `stripInlineComment` (un `bool` que
    // alterna): un separador DENTRO de comillas es contenido, uno fuera separa.
    // No interpreta escapes — el motor no los tiene en ningún lado, y
    // agregarlos acá inventaría una gramática que nadie firmó.
    //
    // Las comillas se conservan en el token: quien las quita es `extractQuoted`,
    // aguas abajo, que es donde ya vivía esa decisión.
    //
    // Sellado en `Resumenes_Sesiones/Harness/harness_bloques.cpp` §6.
    inline std::vector<std::string> splitQuoteAware(const std::string& s, char sep)
    {
        std::vector<std::string> out;
        std::string actual;
        bool inQuotes = false;

        for (const char c : s)
        {
            if (c == '"') { inQuotes = !inQuotes; actual += c; continue; }
            if (c == sep && !inQuotes) { out.push_back(actual); actual.clear(); continue; }
            actual += c;
        }
        out.push_back(actual);
        return out;
    }

} // namespace eclips
