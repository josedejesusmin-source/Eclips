// ============================================================================
// EffectSystem.h — Efectos de render componibles (E-CP0 · E-CP1)
//   Contrato: DISENO_EFECTOS_RENDER.md
//
// LEY DE LA LISTA (§1.1, §7.246):
//   Un elemento se dibuja como una LISTA DE PASADAS. La lista arranca en
//   [base] (el elemento tal cual) y cada efecto es un TRANSFORMADOR de esa
//   lista: la lee y devuelve otra. Cero efectos = [base] = el draw de hoy.
//   Los efectos se COMPONEN porque se ENCADENAN, no porque se sumen.
//
// EL ORDEN DE LA TUBERIA (E-CP1) — y una correccion al contrato:
//   El orden de la tuberia ES el orden de emision de los comandos, y en GENERAL
//   es semantico (pintar es ordenado). PERO la paleta de E-CP1 **CONMUTA**, y
//   el contrato afirmaba lo contrario ("en E-CP1 sera observable"). Lo desmintio
//   el harness (T16): `franja`, `tajadas` y `parpadeo` tocan CANALES DISTINTOS,
//   y esos canales conmutan — `dx` SUMA, `alpha`/`color` MULTIPLICAN, y el
//   v-range solo lo parte `tajadas` (las otras lo heredan). tajadas(franja(x))
//   == franja(tajadas(x)), pasada por pasada, BIT A BIT.
//
//   Deja de conmutar en E-CP3, cuando una primitiva AGREGUE pasadas que las
//   siguientes transforman (`estatica`, `sustituto`) o PERMUTE uv (`bloques`).
//   T16 fija la conmutacion HOY para que ese dia el harness se ponga rojo y
//   quien aterrice E-CP3 tenga que mirar el orden a la cara, con evidencia.
//
//   Aun asi, re-emitir un efecto REINICIA su reloj y PRESERVA su posicion:
//   "reiniciar" no significa "reordenar". Hoy es profilaxis (no se observa);
//   en E-CP3 es correccion. Cuesta lo mismo, y es la ley correcta.
//
// POR QUE ESTE HEADER NO CONOCE OPENGL:
//   La validacion de parametros ocurre en PARSE-TIME (EclipScript.h lo
//   incluye, mismo precedente que TweenSystem.h en C-CP1: "la curva se
//   resuelve en parse-time"). Un header que el parser incluye no puede
//   arrastrar GL. El RUNNER (quien traduce Pasada -> quads) vive en
//   VNScriptState.
//
// EL NORTE (§0 del contrato): el glitch no puede ser una FIRMA. En E-CP1
//   renderGlitchChar MUERE: el preset renace como una receta SELLADA sobre
//   estas mismas primitivas, y el creador ya puede tener tajadas sin franja,
//   franja verde, o 4 tajadas.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <utility>
#include <optional>

// CP-C (C-CP5/E-CP5): la entrada `T;` del slot v3 vive ABAJO, junto a la `E;`
// — la familia de campos comparte codec, formateo (fxNumStr) y las tres leyes
// (miss/tolerancia/fase). El track en si es especie de TweenSystem (puro,
// mismo precedente de pureza que este header).
#include "TweenSystem.h"
#include "engine/core/EclipsParse.h"  // SYN-1a: la ley unica del numero

namespace eclips
{
    // ── El modo de mezcla de una pasada ──────────────────────────────────────
    enum class Blend { Alpha = 0, Aditivo, Multiplicar };

    // ────────────────────────────────────────────────────────────────────────
    //  LA PASADA (§1.1) — la parte irreversible del contrato.
    //  Todo campo en IDENTIDAD = el draw de siempre. Un efecto solo toca lo
    //  que necesita; lo demas lo hereda intacto de la pasada que copia.
    // ────────────────────────────────────────────────────────────────────────
    struct Pasada
    {
        // Que porcion de la textura se muestrea (uv normalizado, 0..1)
        float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;

        // Sustituto (E-CP3): otra textura en lugar del elemento. Vacio = el elemento.
        std::string textura;

        // Recorte por la SILUETA del elemento (mascara de alpha). Solo la pide
        // una pasada con textura AJENA y OPACA: las que muestrean el elemento
        // ya las recorta su propio alpha, y `sustituto` trae el suyo. Hoy la
        // unica textura que el motor FABRICA es el ruido de `estatica`
        // (a=255 en los 65536 texels) — y por eso estampaba el RECTANGULO del
        // sprite, padding transparente incluido.
        // OFF POR IDENTIDAD (§7.208): sobre un elemento opaco (`fondo`) la
        // mascara da la MISMA imagen — no hace falta que el call-site declare
        // si es personaje o pantalla.
        bool  mascara = false;

        // Donde se estampa, RELATIVO a la proyeccion natural del elemento
        float dx = 0.0f, dy = 0.0f;     // desplazamiento
        float sx = 1.0f, sy = 1.0f;     // escala alrededor del centro

        // Destino explicito (E-CP3): pisa la proyeccion natural. Off por defecto.
        bool  hasDest = false;
        float destX = 0.0f, destY = 0.0f, destW = 0.0f, destH = 0.0f;

        // Color: tinte MULTIPLICATIVO sobre lo que ya traia la pasada
        float r = 1.0f, g = 1.0f, b = 1.0f;
        float alpha = 1.0f;

        Blend blend = Blend::Alpha;
    };

    using ListaPasadas = std::vector<Pasada>;

    // La lista SIEMPRE arranca aqui: el elemento tal cual lo dibujaria el motor.
    inline Pasada pasadaBase(float tint, float alpha)
    {
        Pasada p;
        p.r = p.g = p.b = tint;
        p.alpha = alpha;
        return p;   // el resto, identidad
    }

    // Presupuesto declarado del contrato (§4): 3 (franja) x 36 (bloques 6x6).
    // No es una intuicion: CAE de la ley multiplicativa de la lista (§7.246).
    inline constexpr int kFxPresupuestoQuads = 108;
    // Umbral de aviso para `n`. DERIVADO, no inventado: 3 x 32 = 96, que es el
    // presupuesto entero. AVISA; no clampea — el creador manda (§11.2.3).
    inline constexpr int kFxNAviso = 32;

    // ────────────────────────────────────────────────────────────────────────
    //  VALIDACION DE VALORES  (E-CP0: el TIPO · E-CP1: el RANGO)
    //
    //  E-CP0 hizo del TIPO parte del vocabulario. E-CP1 hace lo mismo con el
    //  RANGO, y no por simetria: `velocidad=-1` y `proporcion=-1` terminan en
    //  un cast a unsigned => COMPORTAMIENTO INDEFINIDO. Es la peor falla
    //  posible, y nace de un typo plausible.
    //
    //  DOS PISOS, como siempre:
    //    - el PARSER habla   (aviso con numero de linea, §11.2.1)
    //    - el RUNTIME protege (getF clampea SIEMPRE, en UN solo lugar, §1.40)
    //  Ninguna primitiva puede olvidarse de clampear porque NINGUNA primitiva
    //  clampea: lo hace el unico punto por el que el valor entra al motor.
    // ────────────────────────────────────────────────────────────────────────
    enum class FxKeyType { Numero = 0, Color, Textura };

    inline FxKeyType fxKeyType(const std::string& canonKey)
    {
        if (canonKey == "color1" || canonKey == "color2") return FxKeyType::Color;
        if (canonKey == "textura")                        return FxKeyType::Textura;
        return FxKeyType::Numero;   // intensidad, separacion, alpha, velocidad, n, ...
    }

    // ── EL RANGO (E-CP1) ────────────────────────────────────────────────────
    // Indexado por CLAVE, no por par (primitiva, clave): UNA PALABRA, UN
    // SIGNIFICADO. Si `alpha` es un factor de opacidad en `franja`, lo es en
    // `parpadeo` y en `estatica`. El vocabulario unificado es lo que hace que
    // esta tabla tenga una fila por CONCEPTO en vez de una por sinonimo.
    struct FxRange { float min, max; };

    inline FxRange fxKeyRange(const std::string& canonKey)
    {
        const float INF = std::numeric_limits<float>::infinity();

        // Factores de opacidad y fracciones: [0,1]. Identidad de un factor = 1.
        if (canonKey == "alpha" || canonKey == "proporcion")
            return { 0.0f, 1.0f };

        // Relojes y escalas: no negativos. El cast a unsigned de un negativo es
        // UB; el clamp es lo que lo hace IMPOSIBLE.
        // velocidad = 0 CONGELA el efecto en su frame 0. Es una FEATURE
        // (§11.2.4): la corrupcion que no se mueve inquieta mas que la que tiembla.
        if (canonKey == "velocidad" || canonKey == "intensidad")
            return { 0.0f, INF };

        // Claves de DRIVER (E-CP4): segundos, no negativos.
        if (canonKey == "media" || canonKey == "varianza" || canonKey == "desde")
            return { 0.0f, INF };

        // Conteos: al menos uno. n=1 NO es identidad — es la pasada entera
        // tratada como UNA rebanada, y puede romperse (§11.2.2).
        // `frames` (driver esporadico): una ventana de 0 frames no dispara nada.
        if (canonKey == "n" || canonKey == "nx" || canonKey == "ny" ||
            canonKey == "frames")
            return { 1.0f, INF };

        // Escala del grano de `estatica`: px virtuales por texel de ruido.
        // grano=0 dividiria por cero; sub-px no tiene significado visual.
        if (canonKey == "grano")
            return { 1.0f, INF };

        // `fuerza` y `separacion` son libres: el negativo solo invierte el
        // sentido, y eso es una eleccion, no un error.
        return { -INF, INF };
    }

    inline float fxClampKey(const std::string& canonKey, float v)
    {
        const FxRange r = fxKeyRange(canonKey);
        if (v < r.min) return r.min;
        if (v > r.max) return r.max;
        return v;
    }

    // SYN-1a — delega en la ley única del número (`EclipsParse.h`).
    //
    // Esta función YA tenía la parte difícil: el consumo total, que ganó en el
    // arco de efectos (G7: *"stof('5abc') devuelve 5 y NO lanza — el catch no
    // corría nunca"*). Le faltaban las dos baratas, y no eran teóricas:
    //
    //   · FINITUD — `intensidad=nan` pasaba la validación **entera** (`stof`
    //     consume "nan" completo) y le entregaba un NaN al sistema de efectos,
    //     que después lo pasea por `fxClampKey`. Es, palabra por palabra, el
    //     R-VN-03 de la auditoría de sintaxis: *"algunas ramas aceptan
    //     negativos, `nan` o `inf` porque no verifican finitud y dominio"*.
    //   · HEX FLOAT — `intensidad=0x10` daba 16 con consumo total (`strtod`
    //     parsea hex floats), y el clamp lo disfrazaba de máximo legítimo.
    //
    // Nueve callers cuelgan de acá (los params de efecto, la intensidad de
    // `animar`, las líneas de receta nombrada). Se cierran los nueve con una
    // delegación, no con nueve parches — que es la tesis de §1.45, la misma
    // que nos dio `parseBool`.
    inline bool fxValidNumero(const std::string& v)
    {
        float ignored = 0.0f;
        return parseFloatStrict(v, ignored) == NumParse::Ok;
    }

    inline bool fxValidColor(const std::string& v)
    {
        std::string s = v;
        if (!s.empty() && s[0] == '#') s.erase(0, 1);
        if (s.size() != 6) return false;
        for (char c : s)
            if (!std::isxdigit((unsigned char)c)) return false;
        return true;
    }

    inline bool fxValidValue(FxKeyType t, const std::string& v)
    {
        switch (t)
        {
        case FxKeyType::Color:   return fxValidColor(v);
        case FxKeyType::Textura: return !v.empty();
        default:                 return fxValidNumero(v);
        }
    }

    // Para el mensaje del parser: "separacion= espera un numero, recibio 'hola'"
    inline const char* fxKeyTypeName(FxKeyType t)
    {
        switch (t)
        {
        case FxKeyType::Color:   return "un color (#rrggbb)";
        case FxKeyType::Textura: return "una ruta de imagen";
        default:                 return "un numero";
        }
    }

    // ¿Esta clave tiene un rango que el creador pueda violar? (para el aviso)
    inline bool fxKeyHasRange(const std::string& canonKey)
    {
        const FxRange r = fxKeyRange(canonKey);
        return std::isfinite(r.min) || std::isfinite(r.max);
    }

    // Un float, UNA grafía. `std::to_string(0.0f)` da "0.000000", y esa cadena
    // termina en el aviso Y en `cmd.fxParams` — o sea, mañana, en el payload `E;`
    // del save. Un mismo valor con dos escrituras posibles es una fuga de §1.40:
    // el mismo efecto tendría dos representaciones en disco.
    // (El guard del punto cubre "inf", que no lleva decimales.)
    inline std::string fxNumStr(float f)
    {
        std::string s = std::to_string(f);
        if (s.find('.') == std::string::npos) return s;
        while (s.size() > 1 && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        return s;
    }

    inline std::string fxKeyRangeName(const std::string& canonKey)
    {
        const FxRange r = fxKeyRange(canonKey);
        const bool hayMin = std::isfinite(r.min), hayMax = std::isfinite(r.max);
        if (hayMin && hayMax) return "entre " + fxNumStr(r.min) + " y " + fxNumStr(r.max);
        if (hayMin)           return "al menos " + fxNumStr(r.min);
        if (hayMax)           return "como mucho " + fxNumStr(r.max);
        return "";
    }

    // ────────────────────────────────────────────────────────────────────────
    //  PARAMETROS — map crudo k=v.
    //  Cae de lleno en el payload `E;` del save (§6) sin traduccion.
    //  Las CLAVES se canonizan a espanol en el parser (§1.40): el motor acepta
    //  `speed=` y `velocidad=`, pero en disco hay UNA sola grafia.
    //
    //  NO contiene `intensidad`: esa es UNIVERSAL y vive TIPADA en FxCtx y
    //  EffectInstance. Un float no tiene por que dar la vuelta por un string
    //  (to_string redondea a 6 decimales — G4).
    // ────────────────────────────────────────────────────────────────────────
    struct FxParams
    {
        std::map<std::string, std::string> raw;

        bool has(const std::string& k) const { return raw.count(k) != 0; }

        // EL UNICO punto por el que un numero del creador entra al motor — y por
        // eso el UNICO que clampea (§1.40). Una primitiva no puede olvidarse de
        // proteger su cast a unsigned porque no es su trabajo protegerlo.
        float getF(const std::string& k, float def) const
        {
            auto it = raw.find(k);
            if (it == raw.end()) return def;
            float v;
            try { v = std::stof(it->second); }
            catch (...) { return def; }   // inalcanzable desde un .ecl: el parser
            // ya rechazo el valor, con su linea.
            return fxClampKey(k, v);
        }

        std::string getS(const std::string& k, const std::string& def) const
        {
            auto it = raw.find(k);
            return (it == raw.end()) ? def : it->second;
        }

        // Ausente o malformado -> default. El malformado no llega hasta aqui
        // desde un .ecl (fxValidColor lo corto en parse-time, con linea).
        void getColor(const std::string& k, const float def[3], float out[3]) const
        {
            out[0] = def[0]; out[1] = def[1]; out[2] = def[2];
            auto it = raw.find(k);
            if (it == raw.end() || !fxValidColor(it->second)) return;

            std::string s = it->second;
            if (!s.empty() && s[0] == '#') s.erase(0, 1);
            auto hx = [&](int i) {
                return (float)std::stoi(s.substr(i, 2), nullptr, 16) / 255.0f;
                };
            out[0] = hx(0); out[1] = hx(2); out[2] = hx(4);
        }
    };

    // Lo que un transformador puede leer del mundo (v1: solo lo que se usa).
    struct FxCtx
    {
        float t = 0.0f;           // reloj del efecto (segundos desde que arranco)
        float dt = 0.0f;
        float ancho = 0.0f;       // tamano proyectado del elemento, en px virtuales
        float alto = 0.0f;
        float intensidad = 1.0f;  // clave UNIVERSAL (§2), tipada (G4)
        // `tecleando` se FUE (§7.118) y E-CP6 NO lo devolvio AQUI: el ctx
        // LUA (applyLuaFx, VNScriptState) lo lee directo de la caja como
        // `typing` — ninguna primitiva C++ lo pide, y este header no puede
        // conocer la caja. `regiones` no nacio (mesa E-CP6, fork B1: la
        // transparencia del PNG de cara ya recorta); el mouse espera a La
        // Grieta (ctx v2).
    };

    // ────────────────────────────────────────────────────────────────────────
    //  EL VOCABULARIO (§4)
    // ────────────────────────────────────────────────────────────────────────
    enum class FxKind
    {
        None = 0,
        Franja,     // E-CP0: aberracion cromatica. 2 copias tintadas a +-off.
        Tajadas,    // E-CP1: corta en n rebanadas; una proporcion se desplaza.
        Parpadeo,   // E-CP1: modula el alpha de TODA la lista.
        Glitch,     // E-CP1: el PRESET SELLADO. franja + parpadeo + tajadas.
        Bloques,    // E-CP3: grilla nx*ny; una proporcion muestrea OTRA celda.
        Estatica,   // E-CP3: agrega pasada de ruido (textura generada, wrap).
        Sustituto,  // E-CP3: agrega pasada de otra textura tapando el elemento.
        Lua         // E-CP6: transformador del CREADOR (eclips.register_effect).
                    //   El nombre vive en EffectInstance.luaName — namespace
                    //   ABIERTO (mesa-bump F2: los generadores nuevos agregan
                    //   NOMBRES, no gramatica). parseFxKind NO lo devuelve: la
                    //   resolucion es contra las declaraciones `efecto_lua` del
                    //   .ecl (parse-time) o contra el registro vivo (load).
    };

    inline FxKind parseFxKind(const std::string& name)
    {
        if (name == "franja" || name == "fringe")      return FxKind::Franja;
        if (name == "tajadas" || name == "slices")     return FxKind::Tajadas;
        if (name == "parpadeo" || name == "flicker")   return FxKind::Parpadeo;
        if (name == "glitch")                          return FxKind::Glitch;
        if (name == "bloques" || name == "blocks")     return FxKind::Bloques;
        if (name == "estatica" || name == "static")    return FxKind::Estatica;
        if (name == "sustituto" || name == "override") return FxKind::Sustituto;
        return FxKind::None;
    }

    // Grafia CANONICA (espanol). Es lo que va al log y al save.
    inline std::string fxKindToString(FxKind k)
    {
        switch (k)
        {
        case FxKind::Franja:    return "franja";
        case FxKind::Tajadas:   return "tajadas";
        case FxKind::Parpadeo:  return "parpadeo";
        case FxKind::Glitch:    return "glitch";
        case FxKind::Bloques:   return "bloques";
        case FxKind::Estatica:  return "estatica";
        case FxKind::Sustituto: return "sustituto";
        default:                return "";
        }
    }

    // ── Claves que cada primitiva conoce ────────────────────────────────────
    // `intensidad` es UNIVERSAL (§2): la entiende toda primitiva, default 1.
    inline bool fxKeyIsUniversal(const std::string& k)
    {
        return k == "intensidad";
    }

    // Canoniza la grafia bilingue -> espanol. Se aplica UNA vez, en el parser.
    inline std::string canonFxKey(const std::string& k)
    {
        if (k == "intensity")  return "intensidad";
        if (k == "separation") return "separacion";
        if (k == "speed")      return "velocidad";
        if (k == "strength")   return "fuerza";
        if (k == "proportion") return "proporcion";
        if (k == "texture")    return "textura";
        if (k == "grain")      return "grano";
        // Claves de DRIVER: misma ley de una-grafia. Son el CUANDO, no params
        // de primitiva — reservadas trans-primitivas (§7.249, ley abajo).
        if (k == "in")         return "en";
        if (k == "mean")       return "media";
        if (k == "variance")   return "varianza";
        if (k == "from")       return "desde";
        // frames / color1 / color2 / alpha / n: iguales en ambos idiomas
        return k;
    }

    // ── LA LEY §7.249 (E-CP4): claves de driver RESERVADAS trans-primitivas ──
    // `en, media, varianza, frames, desde`
    // pertenecen al CUANDO (los drivers §1.2), no al QUE (las primitivas).
    // NINGUNA primitiva puede declararlas como param propio: el parser las
    // consume ANTES del vocabulario de la primitiva, y si una primitiva futura
    // las declara en fxKeysOf, el aviso de colision la delata en parse-time.
    inline bool fxKeyEsDriver(const std::string& canonKey)
    {
        return canonKey == "en" || canonKey == "media" ||
               canonKey == "varianza" || canonKey == "frames" ||
               canonKey == "desde";
    }

    // ── UNA PALABRA, UN SIGNIFICADO (E-CP1) ─────────────────────────────────
    // El contrato §4 traia tres sinonimos encubiertos:
    //     `frecuencia`  era `velocidad`  (un reloj en Hz)
    //     `alpha_bajo`  era `alpha`      (un factor de opacidad)
    //     `desorden`    sera `proporcion`(fraccion del elemento que se corrompe)
    // Un vocabulario con sinonimos obliga al creador a MEMORIZAR en vez de
    // DEDUCIR — y obliga a la tabla de rangos a repetir la misma ley con dos
    // nombres distintos.
    //
    // El argumento duro es el algebra: TODO FACTOR TIENE IDENTIDAD EN 1
    // (§7.208). `parpadeo alpha=1` ES identidad, y se LEE como identidad.
    // `alpha_bajo=1` tambien lo era — pero el nombre no lo decia.
    inline std::vector<std::string> fxKeysOf(FxKind k)
    {
        switch (k)
        {
        case FxKind::Franja:
            return { "color1", "color2", "separacion", "alpha", "velocidad" };
        case FxKind::Tajadas:
            return { "n", "fuerza", "proporcion", "velocidad" };
        case FxKind::Parpadeo:
            return { "alpha", "velocidad" };
        case FxKind::Glitch:
            return {};   // preset SELLADO: su unica perilla es `intensidad`.
        case FxKind::Bloques:
            return { "nx", "ny", "proporcion", "fuerza", "velocidad" };
        case FxKind::Estatica:
            return { "alpha", "grano", "velocidad", "textura" };
        case FxKind::Sustituto:
            return { "textura" };   // dest= llega con las regiones (E-CP6)
        default:
            return {};
        }
    }

    inline bool fxAcceptsKey(FxKind k, const std::string& canonKey)
    {
        if (fxKeyIsUniversal(canonKey)) return true;
        for (const auto& s : fxKeysOf(k))
            if (s == canonKey) return true;
        return false;
    }

    // Para el mensaje de ayuda del parser: "Parametros: color1= color2= ... "
    inline std::string fxKeysHelp(FxKind k)
    {
        std::string s;
        for (const auto& key : fxKeysOf(k)) s += key + "= ";
        s += "intensidad=";
        return s;
    }

    // ────────────────────────────────────────────────────────────────────────
    //  NUCLEOS TIPADOS (E-CP1) — por que cada primitiva tiene DOS caras
    //
    //    - el NUCLEO   recibe floats ya resueltos
    //    - el ADAPTADOR los saca del FxParams (la cara del creador)
    //
    //  No es simetria por gusto: el PRESET SELLADO tiene que invocar a las
    //  primitivas con valores DERIVADOS de su intensidad. Si lo hiciera por
    //  FxParams, cada float daria la vuelta por to_string() y volveria
    //  redondeado a 6 decimales — que es EXACTAMENTE G4, la grieta que E-CP0
    //  cerro. El nucleo tipado la vuelve imposible por construccion.
    // ────────────────────────────────────────────────────────────────────────

    struct FranjaP
    {
        float c1[3] = { 1.0f, 0.0f, 0.0f };   // rojo
        float c2[3] = { 0.0f, 0.0f, 1.0f };   // azul
        float separacion = 5.0f;
        float alpha = 0.35f;
        float velocidad = 1.0f;
    };

    struct TajadasP
    {
        float n = 10.0f;
        float fuerza = 12.0f;
        // 1/3, NO 0.33: es lo que hacia el `seed % 3 == 0` de renderGlitchChar,
        // y es contra ESA binomial que se midio C5 (§11.1). Un solo valor, una
        // sola verdad (§1.40).
        float proporcion = 1.0f / 3.0f;
        float velocidad = 8.0f;
    };

    struct ParpadeoP
    {
        float alpha = 0.15f;      // FACTOR, no valor absoluto: compone con el fade
        float velocidad = 15.0f;  // Hz del reloj de frames
    };

    // ── El mezclador de C5 (§11.1) — lowbias32 (Wellons) ────────────────────
    //
    //  Avalancha: los bits ALTOS deciden QUIEN se rompe, los BAJOS CUANTO.
    //  CERO aritmetica modular sobre el seed => ningun par de constantes cuyo
    //  gcd decida en silencio si el efecto funciona. El `% 3` de hoy andaba por
    //  una coprimalidad —gcd(3,200)=1— que nadie sabia que existia y que ningun
    //  test protegia (§7.254). Aca la independencia es POR CONSTRUCCION.
    //
    //  Evidencia: sonda_hash.cpp / 2 / 3. Cambiaron la decision DOS veces.
    inline uint32_t fxMix32(uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    // ────────────────────────────────────────────────────────────────────────
    //  PRIMITIVA `franja` / `fringe`  (§4)
    //
    //  Es un TRANSFORMADOR: duplica la lista ENTERA dos veces (una por color, a
    //  +off y -off). Sobre [base] -> 3 pasadas. Sobre 3 -> 9.
    //
    //  Lo que NO vive aqui: el clamp `alpha > 0.8`. Ese es umbral interno del
    //  PRESET. Si viviera en la primitiva, un creador que pida alpha=0.9
    //  heredaria un techo que nadie eligio — y volveria a ser una firma.
    // ────────────────────────────────────────────────────────────────────────
    inline void fxFranjaCore(const FxCtx& ctx, const FranjaP& pp, ListaPasadas& lista)
    {
        const size_t n = lista.size();
        if (n == 0) return;

        const float I = ctx.intensidad;
        const float e = ctx.t;

        // La razon armonica 11/17 es la IDENTIDAD del efecto (su irregularidad).
        // `velocidad` la escala; no la reemplaza.
        const float off = I * pp.separacion * (std::sin(e * 11.0f * pp.velocidad)
            + 0.3f * std::sin(e * 17.0f * pp.velocidad));
        const float amul = pp.alpha * I;

        ListaPasadas copias;
        copias.reserve(n * 2);

        const float* cols[2] = { pp.c1, pp.c2 };
        const float  signo[2] = { +1.0f, -1.0f };

        for (int c = 0; c < 2; c++)
        {
            for (size_t i = 0; i < n; i++)
            {
                Pasada p = lista[i];              // hereda uv, dest, sx/sy, textura
                p.r *= cols[c][0];
                p.g *= cols[c][1];
                p.b *= cols[c][2];
                p.alpha *= amul;
                p.dx += signo[c] * off;
                p.blend = Blend::Aditivo;
                copias.push_back(p);
            }
        }

        // Las originales (blend alpha) primero; las copias aditivas despues.
        // Mismo orden que el GlitchPass passes[3] de hoy.
        lista.insert(lista.end(), copias.begin(), copias.end());
    }

    inline void fxFranja(const FxCtx& ctx, const FxParams& pr, ListaPasadas& lista)
    {
        FranjaP pp;
        const float defC1[3] = { 1.0f, 0.0f, 0.0f };
        const float defC2[3] = { 0.0f, 0.0f, 1.0f };
        pr.getColor("color1", defC1, pp.c1);
        pr.getColor("color2", defC2, pp.c2);
        pp.separacion = pr.getF("separacion", 5.0f);
        pp.alpha = pr.getF("alpha", 0.35f);
        pp.velocidad = pr.getF("velocidad", 1.0f);
        fxFranjaCore(ctx, pp, lista);
    }

    // ────────────────────────────────────────────────────────────────────────
    //  PRIMITIVA `tajadas` / `slices`  (§4, §11.1, §11.2)
    //
    //  Parte CADA pasada en `n` rebanadas horizontales; una `proporcion` de
    //  ellas se desplaza hasta +-`fuerza` px. La semilla se re-sortea a
    //  `velocidad` Hz.
    //
    //  TRES LEYES que el contrato no escribio y que este CP fija:
    //
    //  (1) La subdivision es RELATIVA al v-range de la pasada que recibe, no
    //      absoluta sobre [0,1]. Sobre [base] da lo mismo; sobre una pasada que
    //      YA fue recortada (bloques, E-CP3) es la diferencia entre componer y
    //      romperse. Se paga hoy, que es gratis.
    //
    //  (2) El offset se indexa por REBANADA (k), no por pasada. Las tres capas
    //      de una franja comparten el mismo corte => el desgarro es COHERENTE,
    //      que es lo que el ojo lee como "UNA imagen rota" y no como "tres
    //      imagenes rotas distintas". Es lo que hacia el glitch de hoy
    //      (sliceOX[] vivia FUERA del bucle de pasadas), sin decirlo nunca.
    //
    //  (3) El bucle EXTERNO es el de pasadas; el interno, el de rebanadas. No
    //      es estilo: mantiene contiguos los grupos de blend. Al reves, el
    //      runner abriria un glBegin por rebanada (30 en vez de 2).
    //
    //  n = 1 NO es identidad (§11.2.2, correccion al contrato): es la pasada
    //  entera tratada como UNA rebanada, y puede romperse. Fingir identidad
    //  seria la mentira de siempre. El preset simplemente NO emite `tajadas`
    //  por debajo de I=1.5 — que es exactamente lo que hace hoy.
    // ────────────────────────────────────────────────────────────────────────
    inline void fxTajadasCore(const FxCtx& ctx, const TajadasP& pp, ListaPasadas& lista)
    {
        const size_t np = lista.size();
        if (np == 0) return;

        const int n = (int)pp.n;      // ya clampeado a >= 1 por el rango
        if (n < 1) return;            // red de seguridad (nucleo llamado a mano)

        const float I = ctx.intensidad;

        // ctx.t >= 0 y velocidad >= 0 (rango) => el cast a unsigned es SEGURO.
        // velocidad = 0 => timeSeed = 0 SIEMPRE => las mismas rebanadas rotas,
        // quietas, para siempre: un GLITCH CONGELADO. Es una feature (§11.2.4).
        const uint32_t timeSeed = (uint32_t)(ctx.t * pp.velocidad);
        const uint32_t umbral = (uint32_t)(pp.proporcion * 65536.0f);
        // Bordes verificados: proporcion=1 -> umbral=65536 > todo (h>>16) => rompe
        // TODAS. proporcion=0 -> umbral=0 => no rompe NINGUNA. Sin off-by-one.

        // El offset por REBANADA — compartido por todas las pasadas (ley 2).
        std::vector<float> off((size_t)n, 0.0f);
        for (int k = 0; k < n; k++)
        {
            const uint32_t h = fxMix32(timeSeed ^ (uint32_t)k * 2654435761u);
            if ((h >> 16) < umbral)                                        // QUIEN
                off[(size_t)k] =
                ((float)(h & 0xFFFFu) / 32767.5f - 1.0f) * pp.fuerza * I;   // CUANTO
        }

        ListaPasadas out;
        out.reserve(np * (size_t)n);

        for (size_t i = 0; i < np; i++)                 // externo: pasadas (ley 3)
        {
            const Pasada& src = lista[i];
            const float vSpan = src.v1 - src.v0;

            for (int k = 0; k < n; k++)                 // interno: rebanadas
            {
                Pasada q = src;                          // hereda color, blend, dx...
                q.v0 = src.v0 + vSpan * ((float)k / (float)n);          // ley 1
                q.v1 = src.v0 + vSpan * ((float)(k + 1) / (float)n);
                if (src.hasDest)
                {
                    // E-CP3: una pasada con dest explicito (estatica/sustituto)
                    // rebana su DEST en el mismo movimiento — si no, las n
                    // rebanadas pintarian TODAS el dest completo, encimadas.
                    // (El camino sin dest queda byte-identico: T15 no se toca.)
                    q.destY = src.destY + src.destH * ((float)k / (float)n);
                    q.destH = src.destH / (float)n;
                }
                q.dx += off[(size_t)k];
                out.push_back(q);
            }
        }

        lista.swap(out);
    }

    inline void fxTajadas(const FxCtx& ctx, const FxParams& pr, ListaPasadas& lista)
    {
        TajadasP pp;
        pp.n = pr.getF("n", 10.0f);
        pp.fuerza = pr.getF("fuerza", 12.0f);
        pp.proporcion = pr.getF("proporcion", 1.0f / 3.0f);
        pp.velocidad = pr.getF("velocidad", 8.0f);
        fxTajadasCore(ctx, pp, lista);
    }

    // ────────────────────────────────────────────────────────────────────────
    //  PRIMITIVA `parpadeo` / `flicker`  (§4)
    //
    //  Modula el alpha de TODA la lista al patron de frames (%7 / %13 internos:
    //  dos periodos coprimos => el patron no se repite en 91 frames y el ojo lo
    //  lee como aleatorio).
    //
    //  `alpha` es un FACTOR (default 0.15), NO un valor absoluto: compone con
    //  el fade del personaje en vez de pisarlo. `alpha=1` es IDENTIDAD — la ley
    //  de la casa para todo factor (§7.208).
    //
    //  `intensidad` escala la PROFUNDIDAD de la atenuacion:
    //      a_efectivo = clamp01( alpha + (1 - alpha) * (1 - I) )
    //  I=0 => identidad · I=1 => alpha tal cual · I=2 => el doble de hondo.
    //  Sin esto, `intensidad` sobre parpadeo no haria NADA, en silencio — y una
    //  clave universal que una primitiva ignora calladamente es una mentira
    //  (§2: "cada ficha declara QUE escala").
    //
    //  ⚠ LA FORMA DE LA FORMULA NO ES COSMETICA. La algebraicamente equivalente
    //    `1 - (1 - alpha) * I`  da, en I=1 y alpha=0.15f:  0.149999976
    //    — un ULP por debajo de 0.15f, porque `1.0f - 0.85f != 0.15f`. Eso
    //    ROMPIA la exactitud byte-a-byte del preset contra renderGlitchChar
    //    (harness T15 lo cazo: err = 2.98e-08). Esta forma multiplica por CERO
    //    en I=1 y devuelve `alpha` intacto. §7.208: la identidad del algebra es
    //    el off-por-identidad, y "identidad" quiere decir BIT A BIT.
    // ────────────────────────────────────────────────────────────────────────
    inline void fxParpadeoCore(const FxCtx& ctx, const ParpadeoP& pp, ListaPasadas& lista)
    {
        if (lista.empty()) return;

        // ctx.t >= 0 y velocidad >= 0 (rango) => cast a unsigned SEGURO.
        // velocidad = 0 => ff = 0 => 0 % 7 == 0 => atenuado permanente: el MISMO
        // congelamiento que tajadas. El efecto se queda en su frame 0.
        // UNA LEY, DOS PRIMITIVAS.
        const uint32_t ff = (uint32_t)(ctx.t * pp.velocidad);
        if (!(ff % 7u == 0u || ff % 13u == 0u)) return;   // frame "encendido"

        float a = pp.alpha + (1.0f - pp.alpha) * (1.0f - ctx.intensidad);
        if (a < 0.0f) a = 0.0f;
        if (a > 1.0f) a = 1.0f;

        for (Pasada& p : lista) p.alpha *= a;
    }

    inline void fxParpadeo(const FxCtx& ctx, const FxParams& pr, ListaPasadas& lista)
    {
        ParpadeoP pp;
        pp.alpha = pr.getF("alpha", 0.15f);
        pp.velocidad = pr.getF("velocidad", 15.0f);
        fxParpadeoCore(ctx, pp, lista);
    }

    // ────────────────────────────────────────────────────────────────────────
    //  PRIMITIVA `bloques` / `blocks`  (§4, E-CP3)
    //
    //  Grilla nx*ny sobre CADA pasada; una `proporcion` de las celdas se
    //  corrompe: la celda PINTA en su lugar pero MUESTREA otra celda (el ojo
    //  aparece en el torso). `fuerza` px de offset opcional sobre las
    //  corruptas. `intensidad` escala proporcion Y fuerza — una clave
    //  universal que una primitiva ignora calladamente es una mentira (§2).
    //
    //  ESTA es la primitiva que ROMPE LA CONMUTACION de la paleta (T16 viejo):
    //  permuta uv, y lo que venga despues transforma celdas ya permutadas.
    //  bloques(franja(x)) != franja(bloques(x)) — el orden ES la tuberia.
    //
    //  La subdivision es RELATIVA al rango uv recibido (ley 1 de tajadas,
    //  generalizada a u y v). Una pasada con dest explicito (estatica,
    //  sustituto) subdivide su DEST en celdas — la misma grilla, en su mundo.
    // ────────────────────────────────────────────────────────────────────────
    struct BloquesP
    {
        float nx = 6.0f, ny = 6.0f;
        float proporcion = 0.15f;
        float fuerza = 0.0f;
        float velocidad = 8.0f;
    };

    inline void fxBloquesCore(const FxCtx& ctx, const BloquesP& pp, ListaPasadas& lista)
    {
        const size_t np = lista.size();
        if (np == 0) return;

        const int nx = (int)pp.nx, ny = (int)pp.ny;   // rango: >= 1
        if (nx < 1 || ny < 1) return;                 // red (nucleo a mano)
        const int total = nx * ny;

        const float I = ctx.intensidad;
        float prop = pp.proporcion * I;
        if (prop > 1.0f) prop = 1.0f;

        // ctx.t >= 0 y velocidad >= 0 (rango) => cast a unsigned SEGURO.
        // velocidad = 0 => celdas corruptas congeladas (una ley, todas).
        const uint32_t timeSeed = (uint32_t)(ctx.t * pp.velocidad);
        const uint32_t umbral = (uint32_t)(prop * 65536.0f);

        ListaPasadas out;
        out.reserve(np * (size_t)total);

        for (size_t i = 0; i < np; i++)
        {
            const Pasada& src = lista[i];
            const float uSpan = src.u1 - src.u0;
            const float vSpan = src.v1 - src.v0;

            for (int k = 0; k < total; k++)
            {
                const int cx = k % nx, cy = k / nx;

                // Mismo reparto que C5: bits altos deciden QUIEN. La celda
                // muestreada sale de una SEGUNDA avalancha — cero aritmetica
                // modular sobre el seed crudo (§7.254).
                const uint32_t h = fxMix32(timeSeed ^ (uint32_t)k * 2654435761u);
                const bool corrupta = (h >> 16) < umbral && total > 1;

                int j = k;
                if (corrupta)
                    j = (int)(fxMix32(h ^ 0x9e3779b9u) % (uint32_t)total);
                const int jx = j % nx, jy = j / nx;

                Pasada q = src;                       // hereda color/blend/dx
                // uv de la celda MUESTREADA, relativo al rango recibido (ley 1)
                q.u0 = src.u0 + uSpan * ((float)jx / (float)nx);
                q.u1 = src.u0 + uSpan * ((float)(jx + 1) / (float)nx);
                q.v0 = src.v0 + vSpan * ((float)jy / (float)ny);
                q.v1 = src.v0 + vSpan * ((float)(jy + 1) / (float)ny);

                if (src.hasDest)
                {
                    // El dest se subdivide en su propio mundo (px del elemento).
                    q.destX = src.destX + src.destW * ((float)cx / (float)nx);
                    q.destY = src.destY + src.destH * ((float)cy / (float)ny);
                    q.destW = src.destW / (float)nx;
                    q.destH = src.destH / (float)ny;
                }
                else
                {
                    // La proyeccion natural pinta donde el uv dice; dx/dy la
                    // corrigen al lugar REAL de la celda k.
                    q.dx += uSpan * ((float)(cx - jx) / (float)nx) * ctx.ancho;
                    q.dy += vSpan * ((float)(cy - jy) / (float)ny) * ctx.alto;
                }

                if (corrupta && pp.fuerza != 0.0f)
                {
                    q.dx += ((float)(h & 0xFFu) / 127.5f - 1.0f) * pp.fuerza * I;
                    q.dy += ((float)((h >> 8) & 0xFFu) / 127.5f - 1.0f) * pp.fuerza * I;
                }

                out.push_back(q);
            }
        }

        lista.swap(out);
    }

    inline void fxBloques(const FxCtx& ctx, const FxParams& pr, ListaPasadas& lista)
    {
        BloquesP pp;
        pp.nx = pr.getF("nx", 6.0f);
        pp.ny = pr.getF("ny", 6.0f);
        pp.proporcion = pr.getF("proporcion", 0.15f);
        pp.fuerza = pr.getF("fuerza", 0.0f);
        pp.velocidad = pr.getF("velocidad", 8.0f);
        fxBloquesCore(ctx, pp, lista);
    }

    // ────────────────────────────────────────────────────────────────────────
    //  PRIMITIVA `estatica` / `static`  (§4, E-CP3)
    //
    //  AGREGA una pasada de ruido sobre la lista (no multiplica). La textura
    //  la genera el RUNNER (256x256 gris, una vez, sin asset — este header no
    //  conoce GL) y se resuelve por el centinela kFxRuido; `textura=` del
    //  creador la reemplaza. El uv se re-sortea a `velocidad` Hz y se muestrea
    //  con WRAP (el uv puede pasarse de [0,1] a proposito: `grano` px
    //  virtuales por texel). `intensidad` escala la opacidad. La pasada
    //  hereda tinte y fade de la base: el ruido acompana al elemento.
    // ────────────────────────────────────────────────────────────────────────

    // Centinela de la textura generada. Empieza con '@' para que jamas
    // colisione con una ruta real de asset.
    inline const char* kFxRuido = "@estatica";

    struct EstaticaP
    {
        float alpha = 0.5f;
        float grano = 2.0f;       // px virtuales por texel de ruido (>= 1)
        float velocidad = 12.0f;
        std::string textura;      // "" = la generada (kFxRuido)
    };

    inline void fxEstaticaCore(const FxCtx& ctx, const EstaticaP& pp, ListaPasadas& lista)
    {
        if (lista.empty()) return;

        const float texel = 256.0f * pp.grano;
        const float uSpan = (texel > 0.0f) ? ctx.ancho / texel : 1.0f;
        const float vSpan = (texel > 0.0f) ? ctx.alto / texel : 1.0f;

        // Offset re-sorteado a `velocidad` Hz. velocidad = 0 => ruido
        // CONGELADO en su frame 0 (una ley, todas las primitivas).
        const uint32_t timeSeed = (uint32_t)(ctx.t * pp.velocidad);
        const uint32_t h = fxMix32(timeSeed * 2654435761u + 0x51ed270bu);

        float a = pp.alpha * ctx.intensidad;
        if (a > 1.0f) a = 1.0f;

        Pasada p;
        p.textura = pp.textura.empty() ? kFxRuido : pp.textura;
        p.hasDest = true;                          // pinta el elemento ENTERO
        p.mascara = true;                          // ...pero RECORTADO a su silueta
        p.destX = 0.0f; p.destY = 0.0f;
        p.destW = ctx.ancho; p.destH = ctx.alto;
        p.u0 = (float)(h & 0xFFFFu) / 65535.0f;    // ...muestreando el ruido
        p.v0 = (float)((h >> 16) & 0xFFFFu) / 65535.0f;
        p.u1 = p.u0 + uSpan;                       // (wrap: puede pasar de 1)
        p.v1 = p.v0 + vSpan;
        p.r = lista[0].r; p.g = lista[0].g; p.b = lista[0].b;
        p.alpha = a * lista[0].alpha;
        lista.push_back(p);
    }

    inline void fxEstatica(const FxCtx& ctx, const FxParams& pr, ListaPasadas& lista)
    {
        EstaticaP pp;
        pp.alpha = pr.getF("alpha", 0.5f);
        pp.grano = pr.getF("grano", 2.0f);
        pp.velocidad = pr.getF("velocidad", 12.0f);
        pp.textura = pr.getS("textura", "");
        fxEstaticaCore(ctx, pp, lista);
    }

    // ────────────────────────────────────────────────────────────────────────
    //  PRIMITIVA `sustituto` / `override`  (§4, E-CP3)
    //
    //  AGREGA una pasada de la textura dada TAPANDO el elemento entero (v1;
    //  `dest=` por region llega con las regiones nombradas de E-CP6 — hoy no
    //  tiene caller, §7.118). `textura=` es obligatoria: el parser ya avisa y
    //  descarta la linea si falta. `intensidad` = opacidad del sustituto
    //  (1 = tapa; 0.5 = fantasma encima). Hereda tinte y fade de la base.
    // ────────────────────────────────────────────────────────────────────────
    struct SustitutoP
    {
        std::string textura;
    };

    inline void fxSustitutoCore(const FxCtx& ctx, const SustitutoP& pp, ListaPasadas& lista)
    {
        if (lista.empty() || pp.textura.empty()) return;

        float a = ctx.intensidad;
        if (a > 1.0f) a = 1.0f;

        Pasada p;
        p.textura = pp.textura;
        p.hasDest = true;
        p.destX = 0.0f; p.destY = 0.0f;
        p.destW = ctx.ancho; p.destH = ctx.alto;
        p.r = lista[0].r; p.g = lista[0].g; p.b = lista[0].b;
        p.alpha = a * lista[0].alpha;
        lista.push_back(p);
    }

    inline void fxSustituto(const FxCtx& ctx, const FxParams& pr, ListaPasadas& lista)
    {
        SustitutoP pp;
        pp.textura = pr.getS("textura", "");
        fxSustitutoCore(ctx, pp, lista);
    }

    // ════════════════════════════════════════════════════════════════════════
    //  EL PRESET SELLADO `glitch`  (§4, §7.209)
    //
    //  renderGlitchChar MUERE aqui. El preset es UNA EffectInstance cuya unica
    //  perilla es `intensidad`, y que por dentro llama a las tres primitivas
    //  con params DERIVADOS de esa intensidad — con `intensidad = 1` en el ctx
    //  interno, porque la I ya viaja horneada en los params.
    //
    //  ORDEN: tajadas(parpadeo(franja([base]))).
    //    franja PRIMERO  => el parpadeo modula tambien sus copias (hoy: flickA*fringeA)
    //    tajadas AL FINAL => el desgarro corta las tres capas juntas
    //
    //  UMBRALES (ley interna del PRESET, no de las primitivas):
    //      parpadeo solo si I >= 0.8  ·  tajadas solo si I >= 1.5
    //
    //  ──────────────────────────────────────────────────────────────────────
    //  ⚠  ESTE PRESET ESTA CONGELADO POR EL SAVE, NO POR EL CONTRATO.
    //
    //  Un `monika;glitch;2.0` en disco NO describe un estado: describe ESTA
    //  receta. Si alguien "mejora" una constante de aca, la MISMA cadena en
    //  disco produce OTRA imagen en otro binario — y no hay estampa de version
    //  que lo detecte, porque no hubo bump. El round-trip es lossless en las
    //  dos direcciones JUSTAMENTE porque el preset es fiel.
    //
    //  Lo que lo sella no es este comentario: es harness_pasadas **T15**, que
    //  fija estas constantes valor a valor. Quien las toque pone el harness en
    //  ROJO y tiene que enfrentarse al save antes de seguir. Un comentario se
    //  ignora; un test verde que se pone rojo, no.
    //  ══════════════════════════════════════════════════════════════════════
    inline void fxGlitchPreset(const FxCtx& ctx, ListaPasadas& lista)
    {
        const float I = ctx.intensidad;

        FxCtx c = ctx;
        c.intensidad = 1.0f;   // la I ya viaja horneada en los params (c1)

        // ── franja: SIEMPRE.  off = I*5*(sin 11e + 0.3 sin 17e)
        FranjaP f;
        f.separacion = 5.0f * I;
        f.alpha = std::min(0.35f * I, 0.8f);   // EL CLAMP: ley del preset
        f.velocidad = 1.0f;
        fxFranjaCore(c, f, lista);

        // ── parpadeo: solo a intensidad media+
        if (I >= 0.8f)
        {
            ParpadeoP p;   // alpha=0.15, velocidad=15 — los defaults SON el preset
            fxParpadeoCore(c, p, lista);
        }

        // ── tajadas: solo a intensidad alta. Por debajo de 1.5 el glitch de hoy
        //    dibuja UNA rebanada sin desplazar => no emitir tajadas es EXACTO.
        if (I >= 1.5f)
        {
            TajadasP t;
            t.n = 10.0f;
            t.fuerza = 12.0f * I;
            t.proporcion = 1.0f / 3.0f;   // el `% 3` de renderGlitchChar
            t.velocidad = 8.0f;
            fxTajadasCore(c, t, lista);
        }
    }

    // ── Despacho ────────────────────────────────────────────────────────────
    inline void applyFx(FxKind k, const FxCtx& ctx, const FxParams& pr,
        ListaPasadas& lista)
    {
        switch (k)
        {
        case FxKind::Franja:    fxFranja(ctx, pr, lista);    break;
        case FxKind::Tajadas:   fxTajadas(ctx, pr, lista);   break;
        case FxKind::Parpadeo:  fxParpadeo(ctx, pr, lista);  break;
        case FxKind::Glitch:    fxGlitchPreset(ctx, lista);  break;  // sellado: sin params
        case FxKind::Bloques:   fxBloques(ctx, pr, lista);   break;
        case FxKind::Estatica:  fxEstatica(ctx, pr, lista);  break;
        case FxKind::Sustituto: fxSustituto(ctx, pr, lista); break;
        default: break;                    // None = identidad
        }
    }

    // ── El DRIVER (§1.2): el CUANDO de un efecto ────────────────────────────
    // continuo = hasta detener_animacion · una_vez = N segundos, auto-remove ·
    // esporadico = dormido; dispara ventanas de `frames` frames a intervalos
    // uniformes en [media-varianza, media+varianza] seg. Enumeracion CERRADA
    // (los drivers son especies del MOTOR, no del registro — mesa-bump §4).
    enum class FxDriver { Continuo = 0, UnaVez, Esporadico };

    // ────────────────────────────────────────────────────────────────────────
    //  UNA INSTANCIA VIVA — el espejo de CharAnim, un piso mas abajo.
    // ────────────────────────────────────────────────────────────────────────
    struct EffectInstance
    {
        FxKind   kind = FxKind::None;
        FxParams params;
        float    intensidad = 1.0f;   // universal, TIPADA (G4). No vive en params.
        float    elapsed = 0.0f;
        float    duration = 0.0f;     // 0 = perenne (como el Glitch loop de hoy)

        // ── E-CP4: estado del driver ─────────────────────────────────────────
        // Los PARAMS del esporadico (media/varianza/frames/desde) viven en el
        // k=v como cualquier param (asi viajan al `E;`, mesa-bump §4); aca solo
        // el ESTADO: elapsed + proxima son numeros serializables — el RNG es
        // por-corrida y JAMAS parte del formato (la politica de fase dice que
        // `proxima` NO se lava: el reloj del terror sobrevive al load).
        FxDriver driver = FxDriver::Continuo;
        float    proxima = -1.0f;     // <0 = sin sortear todavia (primer tick)
        int      ventana = 0;         // frames restantes de la ventana activa
        uint32_t rngState = 0;        // semilla por-corrida (cosmetica)

        // ── E-CP5: la receta que pario esta instancia ("" = aplicar-directo).
        // Identidad de GRUPO, no de reconstruccion: la instancia sigue siendo
        // resuelta (kind+params bastan para revivirla); el nombre existe para
        // que `detener <alias> <nombre>` baje el conjunto — y sobreviva al
        // load como campo aditivo del `E;` (fork 2, mesa E-CP5).
        std::string origen;

        // ── E-CP6: nombre del efecto Lua (solo kind == FxKind::Lua; "" para
        // toda primitiva/preset). ES el generador: viaja en el campo 3 del
        // `E;` tal cual (namespace abierto), y el runner lo resuelve contra
        // el registro vivo. El header sigue PURO: aca solo vive el nombre —
        // la funcion registrada es del estado (sol2 no entra al parser).
        std::string luaName;

        // ── Reloj-diálogo CP2: envolvente GRUPAL de intensidad (§4/§8) ──────
        // Copiada de la receta al instanciar; modula ctx.intensity por avances
        // de diálogo. Su progress es independiente de fx.elapsed (segundos):
        // ctx.t fluye, ctx.intensity escalona. Vacía = sin modulación.
        std::optional<ScalarEnvelope> envelope;

        bool isPerenne() const { return duration <= 0.0f; }
        bool isActive()  const { return isPerenne() || elapsed < duration; }

        // Un esporadico dormido es INVISIBLE para la tuberia: conserva su
        // lugar (in-place, E-CP1) pero no transforma nada.
        bool ventanaActiva() const
        {
            return driver != FxDriver::Esporadico || ventana > 0;
        }
    };

    // ── Reloj-diálogo CP2: la intensidad EFECTIVA en este frame ──────────────
    // Tercer factor de la ecuación (§4): intensidad de línea × invocación (ya
    // horneadas en fx.intensidad al instanciar) × envolvente grupal. Sin
    // envolvente el factor es 1. Punto ÚNICO del math (§1.40): render y harness
    // leen la misma cuenta.
    inline float fxEffectiveIntensity(const EffectInstance& fx)
    {
        return fx.intensidad *
            (fx.envelope ? evalEnvelope(*fx.envelope) : 1.0f);
    }

    // El GENERADOR de una instancia, con una sola grafia (log, eco y `E;`):
    // primitivas/preset por la tabla canonica; Lua por su nombre registrado.
    inline std::string fxGenName(const EffectInstance& fx)
    {
        return (fx.kind == FxKind::Lua) ? fx.luaName : fxKindToString(fx.kind);
    }

    // El sorteo del intervalo: uniforme en [media-varianza, media+varianza],
    // sobre el mismo mezclador C5 (avalancha, cero aritmetica modular sobre el
    // seed crudo). Piso de 0.05s: media chica + varianza grande no ametralla.
    inline float fxSorteoIntervalo(EffectInstance& fx, float media, float varianza)
    {
        fx.rngState = fxMix32(fx.rngState + 0x9e3779b9u);
        const float u = (float)fx.rngState / 4294967296.0f;   // [0,1)
        float iv = media + varianza * (2.0f * u - 1.0f);
        if (iv < 0.05f) iv = 0.05f;
        return iv;
    }

    // false = murio (el caller lo poda de su vector).
    inline bool updateEffect(EffectInstance& fx, float dt)
    {
        fx.elapsed += dt;

        if (fx.driver == FxDriver::Esporadico)
        {
            // Los params se leen del k=v (un solo hogar; defaults aca).
            const float media    = fx.params.getF("media", 8.0f);
            const float varianza = fx.params.getF("varianza", 0.0f);
            const float desde    = fx.params.getF("desde", 0.0f);
            const int   frames   = (int)fx.params.getF("frames", 2.0f);

            // Primer tick: la primera cita = desde + intervalo. `desde` es la
            // fase de confianza — JAMAS dispara antes de N seg de vida.
            if (fx.proxima < 0.0f)
                fx.proxima = desde + fxSorteoIntervalo(fx, media, varianza);

            if (fx.ventana > 0) fx.ventana--;      // la ventana consume frames
            if (fx.ventana == 0 && fx.elapsed >= fx.proxima)
            {
                fx.ventana = frames;               // DISPARA
                fx.proxima = fx.elapsed + fxSorteoIntervalo(fx, media, varianza);
            }
        }

        if (fx.isPerenne()) return true;
        return fx.elapsed < fx.duration;
    }

    // ════════════════════════════════════════════════════════════════════════
    //  RE-EMISION (E-CP1) — "reiniciar" NO significa "reordenar".
    //
    //  Los canales de animacion componen por ALGEBRA y SIN orden (§7.208): alli
    //  `erase + push_back` era correcto porque no habia orden que preservar. Las
    //  pasadas componen por TUBERIA, y el orden ES semantico. Copiar la mecanica
    //  de C-CP2 aca habria importado un bug que alla no existia (§7.258):
    //  re-emitir `franja` sobre [franja, tajadas] lo reordenaria a
    //  [tajadas, franja] — y la imagen CAMBIA, con un comando que el creador
    //  cree que solo reinicia.
    //
    //  MISMA MECANICA SOBRE ALGEBRAS DISTINTAS != MISMA LEY.
    //
    //  (Y de yapa: es lo que hara que los DRIVERS funcionen en E-CP4. Un efecto
    //   dormido bajo `esporadico` conserva su lugar en la tuberia y dispara
    //   donde el guion lo escribio, no donde lo re-emitieron por ultima vez.)
    // ════════════════════════════════════════════════════════════════════════
    inline void emitirEfecto(std::vector<EffectInstance>& stack,
        const EffectInstance& fx)
    {
        // E-CP6: los efectos Lua comparten kind — su identidad es el NOMBRE.
        // Sin esta clausula, re-emitir 'grieta' pisaria a 'testigo'.
        for (auto& e : stack)
            if (e.kind == fx.kind &&
                (fx.kind != FxKind::Lua || e.luaName == fx.luaName))
            { e = fx; return; }   // in-place: reloj nuevo, MISMO lugar

        stack.push_back(fx);                              // nuevo: al final de la tuberia
    }

    // ════════════════════════════════════════════════════════════════════════
    //  LA ENTRADA `E;` DEL SLOT v3 (CP-BUMP — DISENO_BUMP_V3 §4)
    //
    //      E;blanco;generador;driver;estado;intensidad;k=v,...
    //
    //  Puros (sin codec, sin GL): el caller pone los ';' con savecodec y
    //  el harness los prueba sin motor. Las TRES leyes viven aca:
    //    · MISS (constitutiva): generador no registrado o driver fuera de la
    //      enum CERRADA => false (el caller avisa y descarta).
    //    · TOLERANCIA: los campos 8+ se ignoran (un v3.5 futuro no rompe).
    //    · FASE: continuo REINICIA (elapsed=0, fase periodica invisible) ·
    //      una_vez restaura elapsed (muere a tiempo tras el load) ·
    //      esporadico restaura elapsed,proxima — el reloj del terror NO se
    //      lava (contraste a proposito: el cooldown de evento_random SI).
    //
    //  `en=` (duracion de una_vez) viaja en el k=v como toda clave de driver
    //  (§7.249 / mesa §4); `intensidad` es campo PROPIO (posicion 5, tipada —
    //  jamas cruza por to_string, G4). El RNG no viaja: es por-corrida.
    // ════════════════════════════════════════════════════════════════════════
    inline std::vector<std::string> fxToE3Fields(const std::string& blanco,
        const EffectInstance& fx)
    {
        std::string driver = "continuo";
        if (fx.driver == FxDriver::UnaVez)          driver = "una_vez";
        else if (fx.driver == FxDriver::Esporadico) driver = "esporadico";

        std::string estado = fxNumStr(fx.elapsed);
        if (fx.driver == FxDriver::Esporadico)
            estado += "," + fxNumStr(fx.proxima);

        std::string kvs;
        for (const auto& e : fx.params.raw)
        {
            if (!kvs.empty()) kvs += ",";
            kvs += e.first + "=" + e.second;
        }
        if (fx.driver == FxDriver::UnaVez && fx.duration > 0.0f)
        {
            if (!kvs.empty()) kvs += ",";
            kvs += "en=" + fxNumStr(fx.duration);
        }

        // E-CP6: el generador viaja con UNA grafia — nombre canonico para
        // primitivas/preset, nombre registrado para Lua (namespace abierto).
        const std::string gen = fxGenName(fx);

        // E-CP5 (fork 2, mesa): el ORIGEN viaja como campo 7, ADITIVO — el
        // lector que no lo conoce lo ignora (ley de tolerancia, ya escrita).
        // Solo viaja si existe: el aplicar-directo no paga el campo.
        if (!fx.origen.empty())
            return { "E", blanco, gen, driver, estado,
                     fxNumStr(fx.intensidad), kvs, fx.origen };

        return { "E", blanco, gen, driver, estado,
                 fxNumStr(fx.intensidad), kvs };
    }

    // false = la entrada NO se reconstruye (ley de miss; el caller pone la voz).
    //
    // E-CP6: `luaRegistrados` (opcional) es el registro VIVO de efectos Lua al
    // momento de cargar — el unico conocimiento que este header puro no puede
    // tener. Un generador que no es primitiva pero SI esta registrado
    // reconstruye como kind=Lua; sin registro (o nullptr), la ley de miss de
    // siempre. Con la declaracion `efecto_lua` cargada al boot, el registro
    // ya existe cuando el load corre — el miss queda para el caso honesto:
    // borraron la declaracion o el archivo.
    inline bool fxFromE3Fields(const std::vector<std::string>& f,
        std::string& blancoOut, EffectInstance& out,
        const std::vector<std::string>* luaRegistrados = nullptr)
    {
        if (f.size() < 6 || f[0] != "E") return false;
        blancoOut = f[1];

        out = EffectInstance{};
        out.kind = parseFxKind(f[2]);
        if (out.kind == FxKind::None)
        {
            bool esLua = false;
            if (luaRegistrados)
                for (const auto& n : *luaRegistrados)
                    if (n == f[2]) { esLua = true; break; }
            if (!esLua) return false;                   // miss de generador
            out.kind = FxKind::Lua;
            out.luaName = f[2];
        }

        if (f[3] == "continuo")        out.driver = FxDriver::Continuo;
        else if (f[3] == "una_vez")    out.driver = FxDriver::UnaVez;
        else if (f[3] == "esporadico") out.driver = FxDriver::Esporadico;
        else return false;                              // enum CERRADA (motor)

        try
        {
            const std::string& es = f[4];
            const size_t coma = es.find(',');
            if (out.driver == FxDriver::Esporadico)
            {
                out.elapsed = std::stof(es.substr(0, coma));
                out.proxima = (coma != std::string::npos)
                    ? std::stof(es.substr(coma + 1)) : -1.0f;
            }
            else if (out.driver == FxDriver::UnaVez)
                out.elapsed = std::stof(es);
            else
                out.elapsed = 0.0f;    // continuo REINICIA (fase invisible)
        }
        catch (...) { return false; }

        try { out.intensidad = std::stof(f[5]); }
        catch (...) { out.intensidad = 1.0f; }

        if (f.size() >= 7 && !f[6].empty())
        {
            const std::string& kvs = f[6];
            size_t pos = 0;
            while (pos < kvs.size())
            {
                size_t coma = kvs.find(',', pos);
                if (coma == std::string::npos) coma = kvs.size();
                const std::string par = kvs.substr(pos, coma - pos);
                const size_t eq = par.find('=');
                if (eq != std::string::npos && eq > 0)
                    out.params.raw[par.substr(0, eq)] = par.substr(eq + 1);
                pos = coma + 1;
            }
        }
        // E-CP5: campo 7 = origen (receta que pario la instancia; "" si fue
        // aplicar-directo). f[8]+ se IGNORAN: ley de tolerancia, constitutiva.
        if (f.size() >= 8)
            out.origen = f[7];

        if (out.driver == FxDriver::UnaVez)
            out.duration = out.params.getF("en", 0.0f);

        return true;
    }

    // ════════════════════════════════════════════════════════════════════════
    //  CONTRATO PURO `E;` DE SAVE v4 (reloj-dialogo CP0)
    //
    //      E;target;generator;driver;state;fixed_intensity;kvs;origin;
    //        env_clock;env_from;env_to;env_duration;env_progress;env_curve;env_mode
    //
    //  `origin` y los siete campos env_* SIEMPRE ocupan su posición. Sin
    //  envolvente quedan vacíos. Con envolvente son atómicos: un campo ausente
    //  invalida el registro completo. El writer/runtime vivos siguen en v3.
    // ════════════════════════════════════════════════════════════════════════
    inline bool fxEnvelopeV4StoredProgress(const ScalarEnvelope& env,
        float& storedProgress)
    {
        const bool validClock =
            env.duration.clock == ClockDomain::Seconds ||
            env.duration.clock == ClockDomain::DialogueAdvance;
        const bool validMode =
            env.mode == Lifecycle::UnaVez || env.mode == Lifecycle::Loop ||
            env.mode == Lifecycle::Vaiven;
        const bool validCurve =
            env.curve == EasingId::Lineal || env.curve == EasingId::Suave ||
            env.curve == EasingId::Salida || env.curve == EasingId::Entrada ||
            env.curve == EasingId::Resorte || env.curve == EasingId::Rebote ||
            env.curve == EasingId::Cubica;

        if (!validClock || !validMode || !validCurve ||
            !std::isfinite(env.from) || !std::isfinite(env.to) ||
            !std::isfinite(env.duration.amount) ||
            env.duration.amount <= 0.0f || !std::isfinite(env.progress) ||
            env.progress < 0.0f)
            return false;

        if (env.duration.clock == ClockDomain::DialogueAdvance)
        {
            if (env.mode != Lifecycle::UnaVez ||
                std::floor(env.duration.amount) != env.duration.amount ||
                env.duration.amount > kMaxExactDialogueAdvance ||
                std::floor(env.progress) != env.progress ||
                env.progress < 0.0f || env.progress > env.duration.amount)
                return false;
            storedProgress = env.progress;
            return true;
        }

        // Misma política Seconds que T4/v3: one-shot en endpoint; ciclos a 0.
        storedProgress = (env.mode == Lifecycle::UnaVez)
            ? env.duration.amount : 0.0f;
        return true;
    }

    inline bool fxToE4Fields(const std::string& target,
        const EffectInstance& fx, const ScalarEnvelope* envelope,
        std::vector<std::string>& out)
    {
        const int kindValue = static_cast<int>(fx.kind);
        const bool validKind =
            kindValue >= static_cast<int>(FxKind::Franja) &&
            kindValue <= static_cast<int>(FxKind::Lua) &&
            (fx.kind != FxKind::Lua || !fx.luaName.empty());
        const bool validDriver =
            fx.driver == FxDriver::Continuo ||
            fx.driver == FxDriver::UnaVez ||
            fx.driver == FxDriver::Esporadico;
        if (target.empty() || !validKind || !validDriver ||
            !std::isfinite(fx.intensidad) || !std::isfinite(fx.elapsed) ||
            (fx.driver == FxDriver::Esporadico && !std::isfinite(fx.proxima)) ||
            (fx.driver == FxDriver::UnaVez &&
                (!std::isfinite(fx.duration) || fx.duration <= 0.0f ||
                 fx.elapsed < 0.0f || fx.elapsed > fx.duration)))
            return false;

        std::vector<std::string> fields = fxToE3Fields(target, fx);
        if (fields.size() < 7 || fields.size() > 8 || fields[2].empty())
            return false;
        fields.resize(8);
        fields[7] = fx.origen; // placeholder obligatorio, también si es ""
        fields.resize(15);

        if (envelope)
        {
            if (fx.origen.empty()) return false;
            float storedProgress = 0.0f;
            if (!fxEnvelopeV4StoredProgress(*envelope, storedProgress))
                return false;
            fields[8] = clockDomainDiskTag(envelope->duration.clock);
            fields[9] = fxNumStr(envelope->from);
            fields[10] = fxNumStr(envelope->to);
            fields[11] = fxNumStr(envelope->duration.amount);
            fields[12] = fxNumStr(storedProgress);
            fields[13] = easingToString(envelope->curve);
            fields[14] = lifecycleToString(envelope->mode);
        }

        out = std::move(fields);
        return true;
    }

    inline bool fxFromE4Fields(const std::vector<std::string>& f,
        std::string& targetOut, EffectInstance& fxOut,
        ScalarEnvelope& envelopeOut, bool& hasEnvelopeOut,
        const std::vector<std::string>* luaRegistrados = nullptr)
    {
        if (f.size() < 15 || f[0] != "E" || f[1].empty() || f[2].empty())
            return false;

        FxDriver driver = FxDriver::Continuo;
        if (f[3] == "continuo") driver = FxDriver::Continuo;
        else if (f[3] == "una_vez") driver = FxDriver::UnaVez;
        else if (f[3] == "esporadico") driver = FxDriver::Esporadico;
        else return false;

        // V4 no hereda la lectura parcial de stof de E3.
        float stateA = 0.0f, stateB = 0.0f, fixedIntensity = 0.0f;
        if (!parseFiniteFloatFull(f[5], fixedIntensity)) return false;
        if (driver == FxDriver::Esporadico)
        {
            const std::size_t comma = f[4].find(',');
            if (comma == std::string::npos ||
                f[4].find(',', comma + 1) != std::string::npos ||
                !parseFiniteFloatFull(f[4].substr(0, comma), stateA) ||
                !parseFiniteFloatFull(f[4].substr(comma + 1), stateB))
                return false;
        }
        else if (!parseFiniteFloatFull(f[4], stateA))
            return false;

        if (driver == FxDriver::UnaVez)
        {
            bool foundDuration = false;
            std::size_t pos = 0;
            while (pos <= f[6].size())
            {
                std::size_t comma = f[6].find(',', pos);
                if (comma == std::string::npos) comma = f[6].size();
                const std::string pair = f[6].substr(pos, comma - pos);
                const std::size_t eq = pair.find('=');
                if (eq != std::string::npos && pair.substr(0, eq) == "en")
                {
                    float duration = 0.0f;
                    if (!parseFiniteFloatFull(pair.substr(eq + 1), duration) ||
                        duration <= 0.0f)
                        return false;
                    foundDuration = true;
                }
                if (comma == f[6].size()) break;
                pos = comma + 1;
            }
            if (!foundDuration) return false;
        }

        const std::vector<std::string> base(f.begin(), f.begin() + 8);
        EffectInstance candidateFx;
        std::string candidateTarget;
        if (!fxFromE3Fields(base, candidateTarget, candidateFx, luaRegistrados) ||
            !std::isfinite(candidateFx.intensidad) ||
            !std::isfinite(candidateFx.elapsed) ||
            (candidateFx.driver == FxDriver::Esporadico &&
                !std::isfinite(candidateFx.proxima)) ||
            (candidateFx.driver == FxDriver::UnaVez &&
                (candidateFx.elapsed < 0.0f ||
                 candidateFx.elapsed > candidateFx.duration)))
            return false;

        bool anyEnvelopeField = false;
        bool allEnvelopeFields = true;
        for (std::size_t i = 8; i < 15; ++i)
        {
            anyEnvelopeField = anyEnvelopeField || !f[i].empty();
            allEnvelopeFields = allEnvelopeFields && !f[i].empty();
        }
        if (anyEnvelopeField != allEnvelopeFields) return false;
        if (anyEnvelopeField && f[7].empty()) return false;

        ScalarEnvelope candidateEnvelope;
        if (anyEnvelopeField)
        {
            if (!parseClockDomainDiskTag(f[8], candidateEnvelope.duration.clock) ||
                !parseFiniteFloatFull(f[9], candidateEnvelope.from) ||
                !parseFiniteFloatFull(f[10], candidateEnvelope.to) ||
                !parseFiniteFloatFull(f[11], candidateEnvelope.duration.amount) ||
                !parseFiniteFloatFull(f[12], candidateEnvelope.progress) ||
                !parseLifecycle(f[14], candidateEnvelope.mode))
                return false;

            const EasingId curve = parseEasing(f[13]);
            candidateEnvelope.curve =
                (curve == EasingId::None) ? EasingId::Lineal : curve;

            float expectedProgress = 0.0f;
            if (!fxEnvelopeV4StoredProgress(candidateEnvelope,
                    expectedProgress) ||
                expectedProgress != candidateEnvelope.progress)
                return false;
        }

        targetOut = candidateTarget;
        fxOut = std::move(candidateFx);
        envelopeOut = candidateEnvelope;
        hasEnvelopeOut = anyEnvelopeField;
        return true;
    }

    // ════════════════════════════════════════════════════════════════════════
    //  LA ENTRADA `T;` DEL SLOT v3 (CP-C de C-CP5 — gramática FIRMADA en
    //  DISENO_SAVE_CONTRATO §4.4 y reconocida por el lector desde el CP-BUMP):
    //
    //      T;target;canal;de;a;amplitud;dur;curva;modo[;origen]
    //
    //  Puros, espejo exacto de la familia E; de arriba. Las tres leyes:
    //    · MISS (constitutiva): canal o modo fuera de sus enums CERRADAS =>
    //      false (el caller avisa y descarta). La curva es tabla ABIERTA:
    //      una curva desconocida degrada a lineal — preserva los extremos
    //      del track, que son su semántica; el gracejo es reemplazable.
    //    · TOLERANCIA: el campo 9 es `origen` (aditivo, fork 2 de la mesa);
    //      f[10]+ se ignoran.
    //    · FASE (contrato C §7): el track viaja SIN elapsed — una_vez
    //      restaura su estado FINAL (elapsed=dur: el logo que cayó se queda
    //      donde cayó, corrigiendo de paso el re-animar de persistentes);
    //      loop/vaiven reinician fase (invisible, política de siempre).
    //
    //  El writer emite de/a RESUELTOS y amplitud VACÍA (las nombradas
    //  serializan resueltas — mesa-bump §2); el lector acepta la forma
    //  amplitud-sin-de/a por la gramática firmada (channelIdentity la
    //  resuelve, misma ley que el parser).
    // ════════════════════════════════════════════════════════════════════════
    inline std::vector<std::string> trackToT3Fields(const std::string& target,
        const TrackInstance& ti)
    {
        const AnimTrack& tr = ti.track;
        std::vector<std::string> f = {
            "T", target, trackChannelToString(tr.canal),
            fxNumStr(tr.de), fxNumStr(tr.a),
            "",                                   // amplitud: ya resuelta
            fxNumStr(tr.dur),
            easingToString(tr.curva),
            lifecycleToString(tr.modo) };
        if (!ti.origen.empty()) f.push_back(ti.origen);
        return f;
    }

    // false = la entrada NO se reconstruye (ley de miss; el caller pone la voz).
    inline bool trackFromT3Fields(const std::vector<std::string>& f,
        std::string& targetOut, TrackInstance& out)
    {
        if (f.size() < 9 || f[0] != "T") return false;
        targetOut = f[1];

        out = TrackInstance{};
        AnimTrack& tr = out.track;

        if (!parseTrackChannel(f[2], tr.canal)) return false;   // miss de canal
        if (!parseLifecycle(f[8], tr.modo))     return false;   // enum cerrada

        try
        {
            tr.dur = f[6].empty() ? 0.0f : std::stof(f[6]);

            const bool hayDeA = !f[3].empty() || !f[4].empty();
            if (hayDeA)
            {
                tr.de = f[3].empty() ? channelIdentity(tr.canal)
                                     : std::stof(f[3]);
                tr.a  = f[4].empty() ? channelIdentity(tr.canal)
                                     : std::stof(f[4]);
            }
            else if (!f[5].empty())
                amplitudToDeA(tr.canal, std::stof(f[5]), tr.de, tr.a);
            else
                return false;   // sin de/a NI amplitud no hay track que armar
        }
        catch (...) { return false; }

        // Curva: tabla abierta — desconocida degrada a lineal (no es miss).
        const EasingId cv = parseEasing(f[7]);
        tr.curva = (cv == EasingId::None) ? EasingId::Lineal : cv;

        // Política de fase, por modo (ver cabecera).
        tr.elapsed = (tr.modo == Lifecycle::UnaVez) ? tr.dur : 0.0f;

        // Campo 9 = origen (aditivo); f[10]+ tolerados.
        if (f.size() >= 10)
            out.origen = f[9];

        return true;
    }

    // ════════════════════════════════════════════════════════════════════════
    //  CONTRATO PURO `T;` DE SAVE v4 (reloj-dialogo CP0)
    //
    //      T;target;canal;de;a;amplitud;dur;curva;modo;origen;clock;progreso
    //
    //  CP0 congela y prueba este codec, pero el writer/runtime vivos siguen en
    //  v3 hasta CP1. `origen` SIEMPRE ocupa el campo 9, aunque esté vacío; así
    //  clock/progreso nunca se desplazan. Los tags internos (`seconds`,
    //  `dialogue_advance`) no dependen del vocabulario público del .ecl.
    //
    //  Política Seconds heredada de v3:
    //    · una_vez se guarda/restaura en el endpoint;
    //    · loop/vaiven reinician en cero.
    //  DialogueAdvance solo admite una_vez y conserva el progreso entero real.
    // ════════════════════════════════════════════════════════════════════════
    inline bool trackToT4Fields(const std::string& target,
        const TrackInstance& ti, std::vector<std::string>& out)
    {
        const AnimTrack& tr = ti.track;
        const bool validChannel =
            tr.canal == Channel::OffsetX || tr.canal == Channel::OffsetY ||
            tr.canal == Channel::Scale || tr.canal == Channel::Alpha;
        const bool validMode =
            tr.modo == Lifecycle::UnaVez || tr.modo == Lifecycle::Loop ||
            tr.modo == Lifecycle::Vaiven;
        const bool validClock =
            tr.clock == ClockDomain::Seconds ||
            tr.clock == ClockDomain::DialogueAdvance;
        const bool validCurve =
            tr.curva == EasingId::Lineal || tr.curva == EasingId::Suave ||
            tr.curva == EasingId::Salida || tr.curva == EasingId::Entrada ||
            tr.curva == EasingId::Resorte || tr.curva == EasingId::Rebote ||
            tr.curva == EasingId::Cubica;
        if (target.empty() || tr.dur <= 0.0f || !std::isfinite(tr.dur) ||
            !std::isfinite(tr.de) || !std::isfinite(tr.a) ||
            !validChannel || !validMode || !validClock || !validCurve)
            return false;

        float storedProgress = 0.0f;
        if (tr.clock == ClockDomain::DialogueAdvance)
        {
            if (tr.modo != Lifecycle::UnaVez ||
                std::floor(tr.dur) != tr.dur ||
                tr.dur > kMaxExactDialogueAdvance ||
                !std::isfinite(tr.elapsed) ||
                std::floor(tr.elapsed) != tr.elapsed ||
                tr.elapsed < 0.0f || tr.elapsed > tr.dur)
                return false;
            storedProgress = tr.elapsed;
        }
        else
        {
            storedProgress = (tr.modo == Lifecycle::UnaVez) ? tr.dur : 0.0f;
        }

        out = {
            "T", target, trackChannelToString(tr.canal),
            fxNumStr(tr.de), fxNumStr(tr.a),
            "",                                   // amplitud: ya resuelta
            fxNumStr(tr.dur),
            easingToString(tr.curva),
            lifecycleToString(tr.modo),
            ti.origen,                             // SIEMPRE: también ""
            clockDomainDiskTag(tr.clock),
            fxNumStr(storedProgress)
        };
        return true;
    }

    // false = registro v4 inválido; el caller debe avisar y descartarlo entero.
    inline bool trackFromT4Fields(const std::vector<std::string>& f,
        std::string& targetOut, TrackInstance& out)
    {
        if (f.size() < 12 || f[0] != "T" || f[1].empty()) return false;

        TrackInstance candidate;
        AnimTrack& tr = candidate.track;
        if (!parseTrackChannel(f[2], tr.canal)) return false;
        if (!parseLifecycle(f[8], tr.modo)) return false;
        if (!parseClockDomainDiskTag(f[10], tr.clock)) return false;

        if (!parseFiniteFloatFull(f[6], tr.dur) || tr.dur <= 0.0f)
            return false;

        const bool hasFromTo = !f[3].empty() || !f[4].empty();
        if (hasFromTo)
        {
            tr.de = channelIdentity(tr.canal);
            tr.a = channelIdentity(tr.canal);
            if ((!f[3].empty() && !parseFiniteFloatFull(f[3], tr.de)) ||
                (!f[4].empty() && !parseFiniteFloatFull(f[4], tr.a)))
                return false;
        }
        else if (!f[5].empty())
        {
            float amplitude = 0.0f;
            if (!parseFiniteFloatFull(f[5], amplitude)) return false;
            amplitudToDeA(tr.canal, amplitude, tr.de, tr.a);
        }
        else
        {
            return false;
        }

        const EasingId curve = parseEasing(f[7]);
        tr.curva = (curve == EasingId::None) ? EasingId::Lineal : curve;
        candidate.origen = f[9];

        float storedProgress = 0.0f;
        if (!parseFiniteFloatFull(f[11], storedProgress)) return false;

        if (tr.clock == ClockDomain::DialogueAdvance)
        {
            if (tr.modo != Lifecycle::UnaVez ||
                std::floor(tr.dur) != tr.dur ||
                tr.dur > kMaxExactDialogueAdvance ||
                std::floor(storedProgress) != storedProgress ||
                storedProgress < 0.0f || storedProgress > tr.dur)
                return false;
            tr.elapsed = storedProgress;
        }
        else
        {
            const float expected =
                (tr.modo == Lifecycle::UnaVez) ? tr.dur : 0.0f;
            if (storedProgress != expected) return false;
            tr.elapsed = expected;
        }

        targetOut = f[1];
        out = candidate;
        return true;
    }

} // namespace eclips
