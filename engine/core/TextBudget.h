// ============================================================================
// TextBudget.h — LA UNIDAD DEL TEXTO: quien es dueno de la conversion
// ============================================================================
//
// Nucleo de la ficha `TYPEWRITER-WRAP` (PATRONES 7.410).
//
// ── QUE ARREGLA ─────────────────────────────────────────────────────────────
//
// El typewriter contaba su presupuesto sobre el texto ORIGINAL y el render lo
// gastaba sobre el texto ENVUELTO. Nadie era dueno de la conversion entre los
// dos, y el desajuste se desangraba en silencio:
//
//   `total = (int)m_displayText.size()`     -> BYTES del original
//   `charIdx++` por cada '\n' del envuelto  -> CODEPOINTS + los saltos que
//                                              INSERTO el wrap
//
// El saldo es `bytes_extra_UTF8 - saltos_insertados`, y tiene DOS caras que son
// el mismo numero con signo distinto:
//
//   saldo < 0   faltan caracteres al final de la linea (1 por salto)
//   saldo > 0   TICKS MUERTOS: el texto ya esta completo y el typewriter sigue
//               "escribiendo" nada. No es cosmetico: `chars >= total` es lo que
//               enciende el indicador, destraba el auto-mode y contesta
//               isWaiting()
//
// Medido sobre el corpus vivo al abrir el lote (983 lineas de dialogo):
// 267 lineas perdian al menos un caracter (373 en total) y 170 arrastraban
// ticks muertos (344 = 11.5s a 30 chars/s). Robusto a +-8 columnas de ancho
// de wrap: entre 194 y 337 lineas.
//
// ── POR QUE UN HEADER APARTE ────────────────────────────────────────────────
//
// Porque la conversion YA existia escrita a mano SEIS veces, en tres archivos,
// y la unica copia con nombre (`VNScriptState::countCodepoints`) es un static
// privado del archivo mas inalcanzable del repo: `VNScriptState.h` arrastra
// SDL/GL/Lua/audio, asi que una ley que viva ahi adentro NO SE PUEDE PROBAR
// (lo midio `sonda_alcance_scenelayer.cpp` en RT-1). El consumidor nuevo vive
// en `TextRenderer.cpp`, que arrastra GL: misma pared.
//
// Escrito inline habria sido la septima copia de la enfermedad que el drenaje
// SYN viene deshaciendo (4 parsers de hex, 3 leyes de desempate bilingue, 4 de
// comillas, 5 de corchetes, 61 `stof` sin `&pos`).
//
// `harness_presupuesto.cpp` prueba este archivo.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   la unidad es el CODEPOINT, no el byte. Es la unidad que el creador
//           ve y la unica que hace que el typewriter corra al mismo ritmo en
//           los dos alfabetos. Contar bytes haria que una 'a' con tilde tarde
//           DOS ticks en aparecer: el defecto elegiria idioma, otra vez
//   ley 2   un '\n' que INSERTO el wrap no cobra presupuesto. No estaba en el
//           texto que el creador escribio, asi que no puede gastar de un
//           presupuesto que se conto sobre ese texto
//   ley 3   un '\n' que escribio el CREADOR si cobra. Es un caracter suyo, esta
//           en el original, y el typewriter ya lo conto. Es la frontera que
//           separa la ley 2 de "los saltos no cuentan nunca", que seria falso
//   ley 4   el centinela `-1` = sin limite sobrevive intacto. Es el valor que
//           pasan la consola y BulletHell, los dos unicos callers que hoy son
//           inmunes al defecto, y tienen que seguir siendo bit-identicos
//
// ── LO QUE ESTE HEADER NO HACE ──────────────────────────────────────────────
//
// No envuelve. El wrap necesita metricas de glifo (advance por codepoint), que
// son del atlas y por lo tanto de `TextRenderer`. Este archivo recibe el
// resultado del wrap y su lista de saltos insertados, y contesta hasta donde
// hay que recorrerlo. La frontera es a proposito: el wrap mide, esto cuenta.
//
// > SELLO (`WRAP-MIDE-BYTES`, 2026-08-28). La frontera SIGUE EN PIE y su RAZON
// > CAMBIO, asi que el parrafo de arriba quedaria mintiendo sin esta linea.
// > Da por hecho que una ley que necesita metricas no se puede probar, y eso
// > valia mientras las metricas se pidieran ADENTRO. `engine/core/TextWrap.h`
// > las toma POR PARAMETRO -- el mismo patron que `inputHeldDeTabla` con su
// > predicado y `VNPalabrasPagina` con sus anchos --, asi que la ley del CORTE
// > ya es pura y la bateria la ve (`harness_wrap.cpp`).
// >
// > Lo que sigue separando los dos archivos NO es la testabilidad: son dos
// > preguntas distintas, y COMPONEN. `TextWrap` decide DONDE corta y emite los
// > `saltosInsertados`; `wrappedStop`, aca abajo, los consume para contestar
// > cuanto del presupuesto se gasto sobre el texto ya cortado. Separadas, un
// > rojo dice CUAL de las dos se rompio.
// ============================================================================

#pragma once

#include "Types.h"

#include <string>
#include <vector>

namespace eclips
{
    namespace text
    {
        // ── El primitivo: reconocer un byte de continuacion UTF-8 ───────────
        //
        // Un byte 10xxxxxx es continuacion. El predicado sigue siendo util
        // para caminatas de frontera, pero desde CP-R1 NO alcanza para contar:
        // un lider puede estar truncado, ser overlong, formar un surrogate o
        // tragarse el ASCII de al lado. La autoridad de validez es el decoder.
        inline bool isUtf8Continuation(unsigned char c)
        {
            return (c & 0xC0) == 0x80;
        }

        // ── EL DECODER, CON NOMBRE ──────────────────────────────────────────
        //
        // Lee UNA unidad desde `pos` y contesta cuantos BYTES consume. `cp`
        // queda en -1 si la unidad no forma un escalar Unicode valido.
        //
        // Politica de avance firmada en ATLAS-DE-FUENTE CP-R1:
        // - secuencia valida de 1..4 bytes: consume completa y devuelve cp;
        // - continuacion equivocada: consume solo el lider, para no tragarse
        //   el ASCII posterior;
        // - prefijo truncado con continuaciones validas: consume lo disponible;
        // - overlong, surrogate o > U+10FFFF completo: consume su ancho y
        //   conserva cp=-1;
        // - lider/continuacion sueltos: consume uno y conserva cp=-1.
        //
        // Los cuatro consumidores (`TextWrap` y tres en `TextRenderer.cpp`)
        // llaman esta unica ley. No hay decoder paralelo que mantener.
        inline size_t utf8Decodificar(const std::string& s, size_t pos, i32& cp)
        {
            cp = -1;
            if (pos >= s.size()) return 0;

            const unsigned char c = static_cast<unsigned char>(s[pos]);
            const size_t fin = s.size();

            if (c < 0x80) { cp = (i32)c; return 1; }

            size_t width = 0;
            i32 value = 0;
            i32 minimum = 0;
            if (c >= 0xC0 && c <= 0xDF)
            {
                width = 2;
                value = static_cast<i32>(c & 0x1F);
                minimum = 0x80;
            }
            else if (c >= 0xE0 && c <= 0xEF)
            {
                width = 3;
                value = static_cast<i32>(c & 0x0F);
                minimum = 0x800;
            }
            else if (c >= 0xF0 && c <= 0xF7)
            {
                width = 4;
                value = static_cast<i32>(c & 0x07);
                minimum = 0x10000;
            }
            else
            {
                return 1;
            }

            for (size_t i = 1; i < width; ++i)
            {
                if (pos + i >= fin) return i;
                const unsigned char tail =
                    static_cast<unsigned char>(s[pos + i]);
                if (!isUtf8Continuation(tail)) return 1;
                value = (value << 6) | static_cast<i32>(tail & 0x3F);
            }

            const bool overlong = value < minimum;
            const bool surrogate = value >= 0xD800 && value <= 0xDFFF;
            const bool beyondUnicode = value > 0x10FFFF;
            if (!overlong && !surrogate && !beyondUnicode) cp = value;
            return width;
        }

        // ── LA CONVERSION (ley 1) ───────────────────────────────────────────
        //
        // Cuantos ESCALARES VALIDOS tiene este string, no cuantos bytes ni
        // cuantos lideres ocupa. Es la misma unidad que incrementa `charIdx`:
        // los retornos con cp < 0 se omiten y no compran un tick invisible.
        // Sobre todo texto bien formado conserva exactamente el conteo previo.
        inline i32 countCodepoints(const std::string& s)
        {
            i32 n = 0;
            size_t pos = 0;
            while (pos < s.size())
            {
                i32 cp = -1;
                const size_t step = utf8Decodificar(s, pos, cp);
                if (step == 0) break;
                pos += step;
                if (cp >= 0) ++n;
            }
            return n;
        }

        // Convierte un reloj en codepoints a frontera de byte sobre el string
        // original. Los invalidos se recorren pero no gastan una unidad, igual
        // que `countCodepoints` y `TextRenderer` con cp < 0. Es la unica ley
        // que usa la consola para render y para sus dos truncados.
        inline size_t utf8PrefijoBytes(const std::string& s, i32 codepoints)
        {
            if (codepoints < 0) return s.size();
            if (codepoints == 0) return 0;

            i32 visibles = 0;
            size_t pos = 0;
            while (pos < s.size())
            {
                i32 cp = -1;
                const size_t step = utf8Decodificar(s, pos, cp);
                if (step == 0) break;
                pos += step;
                if (cp >= 0 && ++visibles >= codepoints) return pos;
            }
            return s.size();
        }

        // ── EL TOPE (leyes 2, 3 y 4) ────────────────────────────────────────
        //
        // Dado un texto YA ENVUELTO y la lista de offsets (en bytes, sobre el
        // envuelto) de los '\n' que el wrap INSERTO, contesta:
        //
        //   "cuantos codepoints del ENVUELTO hay que recorrer para gastar
        //    `budget` codepoints del ORIGINAL"
        //
        // Eso es exactamente el valor que el loop de render necesita en su
        // `charIdx >= charCount`, asi que el cuerpo del loop NO cambia: cambia
        // el numero con el que se compara. Esa es la razon de que el arreglo
        // sea de dos lineas por sitio y no una reescritura del render.
        //
        // `insertedBreaks` va ORDENADO (el wrap los emite en orden), asi que se
        // recorre con un cursor y esto es O(n) sin buscar nada.
        //
        // OJO con el borde: si el presupuesto se agota justo antes de un salto
        // insertado, ese salto SI entra en el tope. Es deliberado y es
        // invisible — un '\n' al final mueve el cursor a una linea que no tiene
        // un solo glifo. Sacarlo costaria una pasada mas para no cambiar un
        // pixel. Pineado en el harness para que sea decision y no accidente.
        inline i32 wrappedStop(const std::string& wrapped,
                               const std::vector<size_t>& insertedBreaks,
                               i32 budget)
        {
            // ley 4: el centinela viaja intacto. Sin esto, `-1` se leeria como
            // "presupuesto cero" y la consola dejaria de dibujar.
            if (budget < 0) return -1;

            i32    stop  = 0;   // codepoints del ENVUELTO recorridos
            i32    spent = 0;   // codepoints del ORIGINAL gastados
            size_t ins   = 0;   // cursor sobre insertedBreaks

            for (size_t b = 0; b < wrapped.size(); ++b)
            {
                if (isUtf8Continuation((unsigned char)wrapped[b])) continue;

                const bool inserted =
                    (ins < insertedBreaks.size() && insertedBreaks[ins] == b);

                if (inserted)
                {
                    ++ins;          // ley 2: gratis, no toca `spent`
                }
                else
                {
                    // ley 3: todo lo demas cobra, incluido el '\n' del creador
                    if (spent >= budget) break;
                    ++spent;
                }
                ++stop;
            }
            return stop;
        }

    } // namespace text
} // namespace eclips
