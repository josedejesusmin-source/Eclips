// ============================================================================
// SaveCodec.h — Codec percent de dos pisos para los saves de Eclips
// ============================================================================
//
// El save dejó de ser un accidente de strings (auditoría integral §7.90). Este
// codec es el CIMIENTO del contrato DISENO_SAVE_CONTRATO.md: convierte cualquier
// dato del jugador/creador en algo que sobrevive un round-trip a disco EXACTO,
// sin que el contenido pueda disfrazarse de estructura.
//
// DOS PISOS, UN MECANISMO (percent-encoding, estilo URL):
//
//   Piso 1 — LÍNEA. Dueño: SaveManager/MetaStore al escribir el "clave=valor".
//     Escapa  % \n \r  sobre el VALOR entero. Mata el newline-inyecta-clave
//     (A1.6/H6): un on_save() que retorne "hp=5\nmana=3" ya no parte la línea
//     ni inyecta la clave "mana" al keyspace global.
//
//   Piso 2 — CAMPO. Dueño: los helpers de composición (join/split).
//     Escapa  % | ;  por campo, antes de unir. Mata el "|" del jugador que
//     partía varsStr (A1.5/P3.4) y la ambigüedad de marcador (A1.3/A1.4): el
//     dato y la estructura dejan de compartir alfabeto.
//
// ¿Por qué "%" y no backslash?  El backslash dobla todos los "\" de los paths
// de Windows (C:\chars\luna.png → ilegible). El "%" es invisible hasta que hace
// falta: el contenido normal queda byte-idéntico a ojo (§F2 del contrato).
//
// GRAMÁTICA DE COMPUESTOS:
//   lista   = entradas unidas con  '|'
//   entrada = campos  unidos con  ';'
//   todo campo pasa por el Piso 2 antes de unirse.
// El "campo" es la UNIDAD ATÓMICA: es el único nivel que escapa. Unir con '|' o
// ';' literales es seguro porque el contenido de cada campo ya escapó los suyos.
// El des-escape ocurre EXACTAMENTE UNA VEZ por campo, en splitFields.
//
// UNESCAPE TOLERANTE + ESTRICTO POR PISO:
//   Cada piso decodifica SOLO sus propios escapes; cualquier otro "%XX" (o
//   "%"+no-hex) pasa literal. Esto:
//     · hace correcto el DOBLE ESCAPE entre pisos por construcción (un '%' de
//       contenido → %25 en piso 2 → %2525 en piso 1; el des-escape invierte el
//       orden peelando una capa por pasada), y
//     · protege el hand-editing: un "%7C" tecleado a mano en un valor que solo
//       cruza el piso 1 NO se convierte silenciosamente en '|'.
//
// Contrato completo, forks firmados y plan Opus: DISENO_SAVE_CONTRATO.md.
// Este archivo es CP0: cero callers, nada cambia en runtime todavía.
// ============================================================================

#pragma once

#include <string>
#include <vector>

namespace eclips::savecodec
{
    // ── Interno: valor hex de un dígito, o -1 si no es hex ──────────────────
    inline int hexVal(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        return -1;
    }

    // ── Interno: decodifica %XX SOLO para los bytes en 'allowed'.
    //   Tolerante: '%'+no-hex, '%' al final, o un %XX cuyo byte no está en
    //   'allowed' → pasan literal (se emite el '%' y se sigue escaneando). ─────
    inline std::string percentDecodeStrict(const std::string& s,
                                            const std::string& allowed)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); )
        {
            if (s[i] == '%' && i + 2 < s.size())
            {
                int hi = hexVal(s[i + 1]);
                int lo = hexVal(s[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    char b = static_cast<char>((hi << 4) | lo);
                    if (allowed.find(b) != std::string::npos)
                    {
                        out += b;
                        i += 3;
                        continue;
                    }
                }
            }
            out += s[i];
            ++i;
        }
        return out;
    }

    // ════════════════════════════════════════════════════════════════════════
    // PISO 1 — LÍNEA (valor entero de un "clave=valor")
    //   Escapa: %→%25 · \n→%0A · \r→%0D
    // ════════════════════════════════════════════════════════════════════════
    inline std::string escapeLine(const std::string& value)
    {
        std::string out;
        out.reserve(value.size());
        for (char c : value)
        {
            switch (c)
            {
                case '%':  out += "%25"; break;
                case '\n': out += "%0A"; break;
                case '\r': out += "%0D"; break;
                default:   out += c;     break;
            }
        }
        return out;
    }

    inline std::string unescapeLine(const std::string& value)
    {
        // Bytes reconocidos por el piso 1: '%' (0x25), '\n' (0x0A), '\r' (0x0D).
        return percentDecodeStrict(value, std::string("%\n\r"));
    }

    // ════════════════════════════════════════════════════════════════════════
    // PISO 2 — CAMPO (unidad atómica de un compuesto)
    //   Escapa: %→%25 · |→%7C · ;→%3B
    //   NOTA: ':' NO se escapa jamás — es el char más común en contenido real
    //   (URLs, horas, C:\...). La gramática uniforme lo sacó de la estructura.
    // ════════════════════════════════════════════════════════════════════════
    inline std::string escapeField(const std::string& field)
    {
        std::string out;
        out.reserve(field.size());
        for (char c : field)
        {
            switch (c)
            {
                case '%': out += "%25"; break;
                case '|': out += "%7C"; break;
                case ';': out += "%3B"; break;
                default:  out += c;     break;
            }
        }
        return out;
    }

    inline std::string unescapeField(const std::string& field)
    {
        // Bytes reconocidos por el piso 2: '%' (0x25), '|' (0x7C), ';' (0x3B).
        return percentDecodeStrict(field, std::string("%|;"));
    }

    // ════════════════════════════════════════════════════════════════════════
    // COMPOSICIÓN
    //   joinFields  = escapa cada campo (piso 2) y los une con ';'
    //   splitFields = parte por ';' literal y des-escapa cada campo (1 sola vez)
    //   joinEntries = une entradas ya construidas con '|' literal (CONFÍA: cada
    //                 campo ya escapó su '|', así que ninguna entrada trae un
    //                 '|' crudo). No re-escapa: doble-escaparía los ';' internos.
    //   splitEntries= parte por '|' literal. NO des-escapa: cada entrada se
    //                 re-parte con splitFields por su cuenta.
    // ════════════════════════════════════════════════════════════════════════
    inline std::string joinFields(const std::vector<std::string>& fields)
    {
        std::string out;
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i) out += ';';
            out += escapeField(fields[i]);
        }
        return out;
    }

    inline std::vector<std::string> splitFields(const std::string& s)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s)
        {
            if (c == ';') { out.push_back(unescapeField(cur)); cur.clear(); }
            else            cur += c;
        }
        out.push_back(unescapeField(cur));
        return out;
    }

    inline std::string joinEntries(const std::vector<std::string>& entries)
    {
        std::string out;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i) out += '|';
            out += entries[i];
        }
        return out;
    }

    inline std::vector<std::string> splitEntries(const std::string& s)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s)
        {
            if (c == '|') { out.push_back(cur); cur.clear(); }
            else            cur += c;
        }
        out.push_back(cur);
        return out;
    }

    // ════════════════════════════════════════════════════════════════════════
    // CLAVES — validación ruidosa (§4.1 del contrato)
    //   Las claves son ENGINE-GENERATED: el piso 1 NO las escapa, las VALIDA.
    //   Una clave inválida es un bug del motor (o un archivo corrupto), no dato
    //   del jugador → se rechaza ruidoso en el write-path, no se sanitiza.
    //   Inválida si: vacía · contiene '=' (parte el clave=valor) · contiene
    //   '\n'/'\r' (parte la línea) · empieza con '[' o '#' (se disfraza de
    //   header/comentario). Si algún día una clave necesita esos chars, el
    //   aviso lo delata en desarrollo — off-por-identidad al alfabeto.
    // ════════════════════════════════════════════════════════════════════════
    inline bool isValidKey(const std::string& key)
    {
        if (key.empty())               return false;
        if (key[0] == '[' || key[0] == '#') return false;
        for (char c : key)
            if (c == '=' || c == '\n' || c == '\r') return false;
        return true;
    }

    // ════════════════════════════════════════════════════════════════════════
    // HEADER — la versión se lee o no existe (§4.3 del contrato)
    //   Parser genérico: '[Eclips <Familia> v<N>]' → N. El nombre de familia es
    //   documental (Save/Meta/Settings); solo la N rutea el codec. Cualquier
    //   línea que no calce el molde exacto → 0 (señal de "no es header de
    //   versión"): el lector la trata como ausente ⇒ v1 crudo (legacy).
    // ════════════════════════════════════════════════════════════════════════
    inline int parseHeaderVersion(const std::string& line)
    {
        if (line.size() < 4 || line.front() != '[' || line.back() != ']')
            return 0;
        const std::string prefix = "[Eclips ";
        if (line.rfind(prefix, 0) != 0)   // no empieza con "[Eclips "
            return 0;
        size_t vpos = line.rfind(" v");   // el " v" que precede al número
        if (vpos == std::string::npos)
            return 0;
        size_t start = vpos + 2;          // primer dígito
        size_t end = line.size() - 1;   // posición del ']'
        if (start >= end) return 0;       // " v]" sin número
        int n = 0;
        for (size_t i = start; i < end; ++i)
        {
            if (line[i] < '0' || line[i] > '9') return 0;  // no todo dígitos
            n = n * 10 + (line[i] - '0');
        }
        return n;
    }

} // namespace eclips::savecodec
