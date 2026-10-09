// ============================================================================
// InlineTags.h - LOS TAGS INLINE DE UNA LINEA, y donde empieza esa linea
// ============================================================================
//
// Nucleo del lote 3 de `AUDITORIA_VOCABULARIO_ECLIPSCRIPT_VN.md` (punto 3 de
// §11): `R-03` (`{pausa=0}` cuenta como override PRESENTE y cancela el retraso
// heredado) + `RO-3` (arreglar las bocas mudas del lector inline).
//
// ── POR QUE UN HEADER APARTE ────────────────────────────────────────────────
//
// `parseRichText` vivia en `TextRenderer.cpp`, que incluye `<SDL_opengl.h>`:
// la pared de `MERGE-DUPLICADO`. Ningun harness podia llegar, asi que las seis
// leyes de los tags inline no las custodiaba nadie.
//
// Pero la funcion ya era `static` y ya era PURA -no toca un solo miembro de
// instancia-, o sea que la pared era de UBICACION, no de acoplamiento: estaba
// del lado GL solo por en que `.cpp` quedo escrita. Por eso este archivo es un
// MOVE y no un rewrite, y `harness_tags` lo clava punto por punto.
//
// Es `RO-5` aplicada a la superficie siguiente -*"que aterrice donde un harness
// la alcance"*-, igual que `TextBudget.h`, `GroupAlpha.h` y `AppearPolicy.h`.
//
// Vive en `engine/render` y no en `core` porque consume `RichText`/`CharEffect`,
// que son de `TextRenderer.h`. Meterlo en `core` invertiria la capa. Mismo
// argumento, palabra por palabra, que `AppearPolicy.h` con `TweenSystem.h`.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   UN EFECTO POR CODEPOINT, no por byte (§7.410, `TextBudget.h`). Los
//           bytes de continuacion UTF-8 no empujan un clon del efecto de su
//           lider. Viene heredada del move; se custodia igual, porque un move
//           que la pierde no rompe el build
//   ley 2   UN TAG NO ES UN CARACTER. Ningun tag empuja un efecto: `effects`
//           crece solo con texto visible. De aca cuelga la ley 3
//   ley 3   LA APERTURA DE LA LINEA ES ANTES DE LA PRIMERA LETRA, no antes del
//           primer byte. `{pausa=N}` escrito ahi no pertenece a ningun
//           caracter: pertenece a la LINEA. `openingTagRunEnd` es quien contesta
//           donde termina esa corrida de tags, y existe porque el motor INYECTA
//           las comillas de presentacion (`wrapWithQuotes`) y esa comilla no la
//           escribio el creador — ver `HO-18` abajo
//   ley 4   PRESENCIA NO ES VALOR. `{pausa=0}` es un override ESCRITO cuyo
//           valor es cero, y eso no se puede decir con un float centinela. El
//           bit viaja aparte (`hasOpeningPause`), gemelo exacto de `hasColor`
//           en esta misma struct: la forma no se invento, se cito (§2.9)
//   ley 5   LA LEY DEL NUMERO TAMBIEN RIGE ADENTRO DE UN TAG. El valor pasa por
//           `parseFloatStrict` (`EclipsParse.h`), asi que consumo total y
//           finitud valen igual que en un `k=v`. Mata las TRES bocas mudas de
//           un saque — ver abajo
//   ley 6   EL DOMINIO ES DEL CALLER (ley 2a de `EclipsParse.h`). `-1` es un
//           float bien FORMADO; que los segundos no puedan ser negativos lo
//           decide este archivo, avisa, y lo cuenta como PRESENTE con valor 0:
//           lo dudoso es el valor, no que el creador haya escrito el tag
//
// ── LAS TRES BOCAS MUDAS, Y POR QUE ERAN TRES ───────────────────────────────
//
// `RO-3` nombro dos. Medido al abrir el lote, eran tres, y la tercera es la
// peor porque ACEPTA en vez de tirar:
//
//   {pausa=0}      cero legitimo   -> lo borraba el gate `if (seconds > 0.0f)`
//   {pausa=abc}    basura          -> `catch` -> 0.0f -> mismo gate, sin voz
//   {pausa=0.5s}   basura ACEPTADA -> `std::stof` de UN argumento consume el
//                                     prefijo y tira el resto: valia 0.5, en
//                                     silencio y con el valor equivocado
//                                     convertido en valido
//
// La tercera es literalmente lo que `EclipsParse.h:313` llama "el corazon" de
// su ley: *"sin esto, `8patos` son 8 y `100px` son 100"*. El motor ya tenia el
// primitivo que las mata a las tres; lo que faltaba era llamarlo.
//
// ── `HO-18`: POR QUE `openingTagRunEnd` VIVE ACA Y NO EN EL CALLER ──────────
//
// `VNScriptState.h:2561` envuelve el texto en las comillas del personaje ANTES
// de parsearlo. Con `comillas` declaradas, la comilla queda como caracter 0, y
// "esto es apertura" -que se contestaba con `effects.empty()`- se volvia falso
// antes del primer tag: `{pausa=N}` caia colgado de la COMILLA.
//
// Medido: 2 de los 3 personajes de `project.eclips` declaran comillas (`m`,
// `s`), y las dos unicas lineas del corpus que empiezan con tag y son de un
// personaje con comillas son justo `{pausa=1.0}`. La sonda no lo vio porque su
// hablante (`sonda`) no esta declarado, o sea que `wrapWithQuotes` devolvia el
// texto intacto: el instrumento valido la feature por la unica forma de
// hablante que no la ejercita.
//
// El arreglo es del envolvedor -la comilla va DESPUES de la corrida de tags de
// apertura-, pero la PREGUNTA *"donde termina esa corrida"* es de este archivo:
// asi el harness puede probar la ley sin arrastrar `VNScriptState.h`, que es el
// archivo mas inalcanzable del repo.
//
// ── LO QUE ESTE HEADER NO HACE ──────────────────────────────────────────────
//
// No dibuja, no mide glifos y no envuelve. Y no llama a `wrapWithQuotes`: eso
// es el CABLE, vive en `VNScriptState.h` y esta del otro lado de la pared —
// misma frontera que `GroupAlpha.h` declara con sus tres sitios. Lo cubre
// `sonda_caja_entrada.ecl`.
// ============================================================================

#pragma once

#include "engine/render/TextRenderer.h"
#include "engine/core/TextBudget.h"
#include "engine/core/EclipsParse.h"

#include <iostream>
#include <string>

namespace eclips
{
    namespace tags
    {
        // ── Hex (#RRGGBB -> RGB float) ──────────────────────────────────────
        // Devuelve true solo si el hex es valido (lo necesitamos para el aviso).
        // NO depende de EclipParser: esto es capa de render y no debe mirar
        // arriba. Hermano de `EclipParser::parseHexColor` (`EclipScript.h`) — si
        // uno cambia de formato, revisar el otro. Desde QUICK-VN/R-VN-08 los dos
        // comparten la MISMA ley: nibble estricto, longitud exacta, bool, valores
        // intactos al fallar. La diferencia es deliberada: el tag `{color=}` no
        // lleva alpha, asi que aca solo viven 6 digitos.
        inline bool parseHex6(const std::string& s, float& r, float& g, float& b)
        {
            auto nib = [](char c, int& o) -> bool {
                if (c >= '0' && c <= '9') { o = c - '0';      return true; }
                if (c >= 'a' && c <= 'f') { o = c - 'a' + 10; return true; }
                if (c >= 'A' && c <= 'F') { o = c - 'A' + 10; return true; }
                return false;
                };
            size_t off = (!s.empty() && s[0] == '#') ? 1 : 0;
            if (s.size() - off != 6) return false;
            int v[6];
            for (int k = 0; k < 6; ++k) if (!nib(s[off + k], v[k])) return false;
            r = (v[0] * 16 + v[1]) / 255.0f;
            g = (v[2] * 16 + v[3]) / 255.0f;
            b = (v[4] * 16 + v[5]) / 255.0f;
            return true;
        }

        // ── ley 3: donde TERMINA la corrida de tags de apertura ─────────────
        //
        // Devuelve el offset en BYTES del primer caracter que NO es parte de un
        // `{tag}` de apertura. Para `"{a}{b}Hola"` devuelve el offset de la 'H';
        // para `"Hola"` devuelve 0; para una linea que es SOLO tags, `size()`.
        //
        // Un `{` sin `}` de cierre NO abre corrida: el parser de abajo lo trata
        // como texto literal, y las dos respuestas tienen que coincidir o el
        // envolvedor pondria la comilla en un lugar que el parser no reconoce.
        //
        // No juzga si el tag existe. Un `{tag_inventado}` tambien cuenta como
        // apertura — a proposito: el parser lo degrada a texto literal, pero la
        // comilla de presentacion sigue queriendo ir despues de lo que el
        // creador escribio como prefijo de linea, no en el medio.
        inline std::size_t openingTagRunEnd(const std::string& raw)
        {
            std::size_t i = 0;
            while (i < raw.size() && raw[i] == '{')
            {
                const std::size_t end = raw.find('}', i + 1);
                if (end == std::string::npos) break;   // sin cierre: es texto
                i = end + 1;
            }
            return i;
        }

        // ── ley 5 + ley 6: el valor de un tag numerico ──────────────────────
        //
        // `out` queda INTACTO salvo que la lectura sea buena, igual que
        // `parseFloatStrict`. Devuelve si el tag cuenta como ESCRITO (ley 4):
        // un valor malformado avisa y NO cuenta, porque no se puede saber que
        // quiso decir; un negativo avisa y SI cuenta, con valor 0, porque lo
        // unico dudoso ahi es el valor.
        //
        // El aviso va a `cout` y no a `cerr` para no separarse de los otros
        // avisos de este lector (`{color=}` ya usa `cout`, `TextRenderer.cpp`
        // linea 546 antes del move). Una sola voz por tag y por corrida.
        inline bool parseTagSeconds(const std::string& tagName,
                                    const std::string& valueText,
                                    float& out)
        {
            float v = 0.0f;
            if (parseFloatStrict(valueText, v) != NumParse::Ok)
            {
                std::cout << "[TextRenderer] {" << tagName << "=" << valueText
                          << "}: no es un numero. Se ignora." << std::endl;
                return false;
            }
            if (v < 0.0f)
            {
                std::cout << "[TextRenderer] {" << tagName << "=" << valueText
                          << "}: los segundos no pueden ser negativos. Uso 0."
                          << std::endl;
                out = 0.0f;
                return true;   // ley 6: escrito esta, el valor es lo dudoso
            }
            out = v;
            return true;
        }

        // ── EL PARSER ───────────────────────────────────────────────────────
        //
        // Move literal de `TextRenderer::parseRichText`, salvo el tag `pausa=` /
        // `pause=`, que es lo que este lote cambia (leyes 4, 5 y 6).
        inline RichText parseRichText(const std::string& raw)
        {
            RichText result;
            TextFX  currentFX    = TextFX::None;
            float   currentSpeed = 1.0f;

            bool    curHasColor = false;
            float   ccr = 1.0f, ccg = 1.0f, ccb = 1.0f;
            std::string curFontName;   // {fuente=X}/{font=X}: vacio = fuente de linea
            float       curFontSize = 0.0f; // {tam=N}/{size=N}: 0 = tamano de linea

            // Construye un CharEffect con TODO el estado actual (fx + speed +
            // color + fuente).
            auto pushEff = [&]() {
                CharEffect e;
                e.fx = currentFX;
                e.speedMult = currentSpeed;
                e.hasColor = curHasColor;
                e.cr = ccr; e.cg = ccg; e.cb = ccb;
                e.fontName = curFontName;
                e.fontSize = curFontSize;
                result.effects.push_back(e);
                };

            size_t i = 0;
            while (i < raw.size())
            {
                if (raw[i] == '{')
                {
                    // Buscar cierre
                    size_t end = raw.find('}', i + 1);
                    if (end == std::string::npos)
                    {
                        // Sin cierre -> tratar como texto literal
                        result.displayText += raw[i];
                        pushEff();
                        i++;
                        continue;
                    }

                    std::string tag = raw.substr(i + 1, end - i - 1);

                    // ── Tags de apertura ──
                    if (tag == "wavy" || tag == "ondulado")
                    {
                        currentFX = TextFX::Wavy;
                    }
                    else if (tag == "shake" || tag == "temblar")
                    {
                        currentFX = TextFX::Shake;
                    }
                    // "glitch" es cognado ES/EN (como "color") -> sin variante.
                    else if (tag == "glitch")
                    {
                        currentFX = TextFX::Glitch;
                    }
                    else if (tag.size() > 6 && tag.substr(0, 6) == "speed=")
                    {
                        float parsed = 0.0f;
                        if (parsePositiveTextSpeedFactor(tag.substr(6), parsed))
                            currentSpeed = parsed;
                        else
                            std::cout << "[TextRenderer] {speed=" << tag.substr(6)
                            << "}: multiplicador invalido; se conserva el anterior."
                            << std::endl;
                    }
                    else if (tag.size() > 10 && tag.substr(0, 10) == "velocidad=")
                    {
                        float parsed = 0.0f;
                        if (parsePositiveTextSpeedFactor(tag.substr(10), parsed))
                            currentSpeed = parsed;
                        else
                            std::cout << "[TextRenderer] {velocidad=" << tag.substr(10)
                            << "}: multiplicador invalido; se conserva el anterior."
                            << std::endl;
                    }
                    // ── Override absoluto de linea: {velocidad_global=N} ──
                    // Setea velocidad base de toda la linea (ignora cadena de
                    // fallback). Composable con {speed=N}: el multiplicador
                    // per-char sigue aplicando sobre este valor (final =
                    // lineSpeedOverride * speedMult). Position-agnostic (es
                    // line-level): convencionalmente al inicio, pero funcional en
                    // cualquier posicion. Si aparece varias veces, gana la
                    // ultima. Valores <= 0 se ignoran silenciosamente.
                    else if ((tag.size() > 17 && tag.substr(0, 17) == "velocidad_global=") ||
                        (tag.size() > 13 && tag.substr(0, 13) == "global_speed="))
                    {
                        size_t eqPos = tag.find('=');
                        const std::string value = tag.substr(eqPos + 1);
                        const auto parsed = parseTextSpeedCps(value);
                        if (parsed)
                            result.lineSpeedOverride = *parsed;
                        else
                            std::cout << "[TextRenderer] {velocidad_global=" << value
                            << "}: CPS invalido; se conserva la cadena de velocidad."
                            << std::endl;
                    }
                    // ── Color inline: {color=#RRGGBB} ──
                    // "color" es cognado ES/EN -> no necesita variante bilingue.
                    // Pisa SOLO el RGB del char; el alpha lo hereda de la linea.
                    else if (tag.size() > 6 && tag.substr(0, 6) == "color=")
                    {
                        float pr, pg, pb;
                        if (parseHex6(tag.substr(6), pr, pg, pb))
                        {
                            curHasColor = true;
                            ccr = pr; ccg = pg; ccb = pb;
                        }
                        else
                        {
                            // Voto B: rechaza ensenando, no tolera en silencio.
                            // Hex invalido -> no activa color, cae al de linea,
                            // avisa 1x.
                            static bool s_warnedColor = false;
                            if (!s_warnedColor)
                            {
                                std::cout << "[TextRenderer] {color=...}: hex invalido ('"
                                    << tag.substr(6) << "'). Formato: {color=#RRGGBB}. "
                                    << "Uso el color de la linea." << std::endl;
                                s_warnedColor = true;
                            }
                        }
                    }
                    // ── Fuente inline: {fuente=X} / {font=X} ──
                    // "fuente"/"font" NO son cognados -> ambas keywords. El
                    // nombre (sin comillas, como en texto-capa) referencia una
                    // fuente del FontManager. La existencia se valida en render.
                    else if (tag.size() > 7 && tag.substr(0, 7) == "fuente=")
                    {
                        curFontName = tag.substr(7);
                    }
                    else if (tag.size() > 5 && tag.substr(0, 5) == "font=")
                    {
                        curFontName = tag.substr(5);
                    }
                    // ── Tamano inline: {tam=N} / {size=N} ──
                    // "tam"/"size" NO son cognados -> ambas keywords. px absoluto.
                    //
                    // Basura/<=0 -> 0 (cae al tamano de linea), SIN warn. Eso NO
                    // es un descuido heredado: es ley firmada, y su caller la
                    // asevera en prosa (`test_tam_inline.ecl:46`, caso C6). El
                    // lote 3 la dejo intacta a proposito (`FO-11`): barrer los
                    // seis lectores habria volteado una ley firmada adentro de
                    // otro lote, que es lo que §13.5 decidio no hacer con
                    // `TEMA-SIN-FOLD`. Y ojo con "unificar por consistencia": en
                    // ESTE tag el `0` es centinela de RESET (caso C3), o sea el
                    // significado OPUESTO al que tiene en `pausa=`.
                    else if (tag.size() > 4 && tag.substr(0, 4) == "tam=")
                    {
                        try { curFontSize = std::stof(tag.substr(4)); }
                        catch (...) { curFontSize = 0.0f; }
                    }
                    else if (tag.size() > 5 && tag.substr(0, 5) == "size=")
                    {
                        try { curFontSize = std::stof(tag.substr(5)); }
                        catch (...) { curFontSize = 0.0f; }
                    }
                    // ── Tags de cierre ──
                    else if (tag == "/wavy" || tag == "/ondulado" ||
                             tag == "/shake" || tag == "/temblar" ||
                             tag == "/glitch")
                    {
                        currentFX = TextFX::None;
                    }
                    else if (tag == "/speed" || tag == "/velocidad")
                    {
                        currentSpeed = 1.0f;
                    }
                    else if (tag == "/color")
                    {
                        curHasColor = false;  // limpia SOLO el color
                    }
                    else if (tag == "/fuente" || tag == "/font")
                    {
                        curFontName.clear();  // vuelve a la fuente de linea
                    }
                    else if (tag == "/tam" || tag == "/size")
                    {
                        curFontSize = 0.0f;   // vuelve al tamano de linea
                    }
                    // ── Pausa temporizada: {pausa=N} / {pause=N} ──
                    //
                    // Leyes 4, 5 y 6. Antes del lote 3 este bloque leia con un
                    // `stof` pelado y tiraba todo lo que no fuera `> 0`, que son
                    // las tres bocas mudas de la cabecera.
                    else if ((tag.size() > 6 && tag.substr(0, 6) == "pausa=") ||
                        (tag.size() > 6 && tag.substr(0, 6) == "pause="))
                    {
                        const std::string nombre = tag.substr(0, 5);
                        float seconds = 0.0f;
                        if (parseTagSeconds(nombre, tag.substr(6), seconds))
                        {
                            if (!result.effects.empty())
                            {
                                // Mid-linea: la pausa se cuelga del caracter
                                // ANTERIOR. Cero aca es un no-op de verdad (no
                                // hay retraso heredado que cancelar), asi que no
                                // necesita bit de presencia.
                                result.effects.back().pauseAfter = seconds;
                            }
                            else
                            {
                                // ── ley 3 + ley 4 ──
                                // Al INICIO de la linea la pausa no pertenece a
                                // ningun caracter: pertenece a la LINEA, y es el
                                // retraso dramatico del texto. Se acumula por si
                                // hay varias pegadas — y la PRESENCIA se prende
                                // aunque el valor sea 0, que es todo el lote:
                                // `{pausa=0}` significa *"esta linea arranca ya"*
                                // y tiene que ganarle al retraso del tema.
                                result.openingPause += seconds;
                                result.hasOpeningPause = true;
                            }
                        }
                    }
                    // ── Espera por click inline: {esperar_click} / {wait_click} ──
                    else if (tag == "esperar_click" || tag == "wait_click")
                    {
                        if (!result.effects.empty())
                            result.effects.back().waitClickAfter = true;
                        else
                            std::cout << "[TextRenderer] {" << tag
                            << "} al inicio sin char previo, ignorado" << std::endl;
                    }

                    // ── Interrupcion de linea: {interrumpir} / {interrupt} ──
                    // Marca toda la linea: al completar el typewriter, salta al
                    // siguiente dialogo sin esperar click (pisa el modo auto).
                    // Line-level y position-agnostic (convencional al final).
                    else if (tag == "interrumpir" || tag == "interrupt")
                    {
                        result.interrupts = true;
                    }

                    // ── Tag desconocido -> texto literal ──
                    else
                    {
                        result.displayText += '{';
                        pushEff();
                        i++;  // solo avanzar el '{', el contenido sigue normal
                        continue;
                    }

                    i = end + 1; // saltar tag completo
                }
                else
                {
                    // ── ley 1 ──
                    // §7.410 / `TextBudget.h` — UN efecto por CODEPOINT, no por
                    // byte. Antes los bytes de continuacion UTF-8 empujaban un
                    // clon del efecto de su lider, y el vector quedaba indexado
                    // en BYTES: esa era la mitad byte/caracter de la confusion.
                    result.displayText += raw[i];
                    if (!text::isUtf8Continuation((unsigned char)raw[i]))
                        pushEff();
                    i++;
                }
            }

            return result;
        }

    } // namespace tags
} // namespace eclips
