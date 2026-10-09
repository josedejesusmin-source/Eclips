// ============================================================================
// ECLIPS ENGINE - VNCompare  (ficha SI-NO-COMPARA-VARIABLES)
// ============================================================================
//
// LA PREGUNTA QUE ESTE HEADER CONTESTA, Y LA PARIO UN JUEGO
//
//     Monika 1 · Sayori 3 · Yuri 7 · Natsuki 9   ->   gana Natsuki
//
// El test integral de `CP4` (DISENO_MUTAR_PALABRAS §12.4) termina en "¿quien
// gano?" y descubrio que el motor NO PUEDE EXPRESAR ESA PREGUNTA: en un `si`
// el lado izquierdo se resuelve como variable y el derecho se toma como
// LITERAL, asi que `si sayori > natsuki` es `sayori > 0` y las tres ramas de
// ganador corren a la vez. `mutar` tiene la misma frontera con otro default
// (`catch` -> `delta = 1`). Las dos fallaban EN VERDE. -> PATRONES §7.532
//
// ----------------------------------------------------------------------------
// LO QUE ESTE HEADER **NO** HACE, Y ES LA MITAD DE LA DECISION
// ----------------------------------------------------------------------------
// NO inventa sintaxis. El motor ya tiene UN operador de lectura y el manual lo
// dice con esas palabras (`ECLIPSCRIPT_DOCS §10`): *"`entrada` ESCRIBE,
// `${var}` LEE"*. La respuesta es dejarlo entrar donde nunca entro:
//
//     si sayori > ${natsuki}          <- comparar dos contadores
//     si sayori == ${natsuki}         <- el EMPATE
//     mutar top   = ${monika}         <- copiar un valor
//     mutar dif   - ${natsuki}        <- aritmetica entre variables
//
// La interpolacion NO vive aca: vive en `VNScriptState::interpolateVars`,
// porque necesita `m_variables`, el MetaStore y el catalogo de idiomas. Este
// header recibe los dos lados YA RESUELTOS y pone la ley de encima. Es el
// mismo reparto que `VNMutateTarget.h`: el tokenizador reparte sin juzgar, y
// cada caller pone su ley.
//
// ----------------------------------------------------------------------------
// POR QUE EL MARCADOR NO ES ADORNO -- MEDIDO SOBRE EL CORPUS VIVO
// ----------------------------------------------------------------------------
// El fork tentador era "que `si sayori > natsuki` funcione solo". Lo rompe el
// propio fixture del arco (`Harness/fixtures/palabras_cp4/juego.ecl`), que en
// UN archivo usa los mismos nombres de las dos maneras:
//
//     :26  mutar sayori = 0            <- `sayori` es VARIABLE
//     :129 mutar gano   = sayori       <- `sayori` es LITERAL
//     :138 si    gano  == sayori       <- LITERAL
//     :72  si _palabra_var == sayori   <- LITERAL
//     :128 si    sayori >= 3           <- VARIABLE, del lado izquierdo
//
// Son OCHO lineas que una regla sin marcador rompe en silencio, y encima
// justo el patron del flag `gano` que §7.532 recomienda. **El lado derecho es
// un namespace de LITERALES por construccion.** El marcador es lo unico que
// separa "la palabra natsuki" de "el valor de natsuki".
//
// Y la posicion esta libre: CERO lineas del corpus vivo llevan `${` dentro de
// un `si`, un `mutar`/`set`, un gate `[si:]` o un destino `->`. Igual que
// `-> vida + 1` en `mutar-cp0`, esto no le cambia la conducta a nada que
// funcione hoy: REVIVE UNA FORMA MUERTA.
//
// ----------------------------------------------------------------------------
// LA VOZ (F4): SYN-1a LLEGO AL PARSER Y NUNCA CRUZO AL RUNTIME
// ----------------------------------------------------------------------------
// `parseIntStrict` (EclipsParse.h) es de 2026-08 y esta sellado en
// `harness_num.cpp`. `EclipScript.h`, `MapData.h` y `Gameplay2DState.h` ya lo
// usan. `si` y `mutar` seguian con `std::stoi` crudo y un `catch` que dejaba
// un numero inventado -- que es exactamente "un default silencioso convierte
// un error de escritura del creador en CONDUCTA" (§7.532, familia de §1.60).
//
// Este header no inventa la ley: la IMPORTA. Y hereda su distincion mas cara:
//
//     Empty     -> 0, EN SILENCIO.  Un contador que nadie seteo vale cero, y
//                  eso es conducta viva y documentada. Avisar aca seria ruido
//                  en cada script que pregunta antes de sumar.
//     Malformed -> 0, CON VOZ.      Un valor escrito con basura no puede
//                  sonar igual que un campo que nadie escribio.
//
// Radio medido antes de firmarla: de los 87 `si` del corpus vivo, el 100% de
// la rama de orden tiene lado derecho numerico; de las 33 lineas
// `mutar/set + -`, ninguna tiene operando no numerico. LA VOZ NACE CALLADA.
//
// ----------------------------------------------------------------------------
// LAS DOS ASIMETRIAS QUE SON LEY, NO DESCUIDO
// ----------------------------------------------------------------------------
// (1) `==` y `!=` COMPARAN TEXTO Y NO SE TOCAN. `si _palabra == lluvia` y
//     `si ruta == unirse` son 40+ lineas del corpus vivo y son correctas. Ahi
//     no hay numero que parsear, asi que no hay basura que denunciar. La voz
//     vive SOLO en la rama de orden (`> < >= <=`), donde el valor TIENE que
//     ser numero.
//
// (2) MALFORMED SIEMPRE DA FALSO, no importa el operador. Es fail-closed
//     (§7.89): el cuerpo del `si` no corre. Y no es simetrico en conducta --
//     `si x < basura` ya daba falso (`x < 0`), asi que ahi no cambia nada;
//     el que cambia es `si x > basura`, que daba VERDADERO para cualquier x
//     positivo. Ese es, literalmente, el bug de las tres ramas.
//
// ----------------------------------------------------------------------------
// LO QUE ESTE HEADER NO PUEDE VER
// ----------------------------------------------------------------------------
// El CABLE. Que `evaluateCondition` de verdad interpole antes de llamar aca,
// y que `executeSet` haga lo mismo, vive dentro de `VNScriptState.h` -- el
// header gigante que ningun harness compila (pared de `MERGE-DUPLICADO`). Eso
// lo mide una SONDA, no este archivo.
//
// Y la POLITICA DE REPETICION de la voz es del caller (§7.493): este header
// devuelve la causa y arma el texto; quien decide si se dice una vez o cada
// vez es quien tiene el estado. Un `si` adentro de un bucle `ir_a` habla cinco
// veces si nadie lo deduplica, y la causa es permanente.
// ============================================================================

#pragma once

#include "engine/core/EclipsParse.h"

#include <string>

namespace eclips
{

    // ── Que paso al comparar ────────────────────────────────────────────────
    enum class CompararCausa
    {
        Ok,                  // la comparacion es legitima; `valor` manda
        DerechaMalformada,   // `> < >= <=` con lado derecho no numerico
        IzquierdaMalformada, // `> < >= <=` con la variable no numerica
        OperadorDesconocido  // no es ninguno de los seis
    };

    struct CompararResultado
    {
        bool          valor = false;
        CompararCausa causa = CompararCausa::Ok;
    };

    // ── Los seis operadores, y de que familia es cada uno ────────────────────
    // `si` NO comparte la allowlist de `mutar` a proposito (ver
    // VNMutateTarget.h): mezclarlas dejaria `-> vida == 3` mutando.
    inline bool compararEsIgualdad(const std::string& op)
    {
        return op == "==" || op == "!=";
    }

    inline bool compararEsOrden(const std::string& op)
    {
        return op == ">" || op == "<" || op == ">=" || op == "<=";
    }

    // ── EL NUCLEO ───────────────────────────────────────────────────────────
    //
    // Los dos lados llegan YA RESUELTOS: `izq` es el valor de la variable
    // (`resolveVariable`) y `der` es el literal del guion despues de pasar por
    // `interpolateVars`. Este header no sabe -- ni tiene por que saber -- si
    // el creador escribio `3` o `${natsuki}`.
    inline CompararResultado compararValores(const std::string& izq,
                                             const std::string& op,
                                             const std::string& der)
    {
        CompararResultado r;

        // Igualdad: comparacion de TEXTO, y nunca hay basura que denunciar.
        if (op == "==") { r.valor = (izq == der); return r; }
        if (op == "!=") { r.valor = (izq != der); return r; }

        if (!compararEsOrden(op))
        {
            r.causa = CompararCausa::OperadorDesconocido;
            return r;   // fail-closed
        }

        // Orden: los dos lados tienen que ser numeros. `Empty` cae al 0 en
        // silencio (contador sin setear); `Malformed` habla y da falso.
        int iIzq = 0, iDer = 0;

        const NumParse pIzq = parseIntStrict(izq, iIzq);
        if (pIzq == NumParse::Malformed)
        {
            r.causa = CompararCausa::IzquierdaMalformada;
            return r;
        }

        const NumParse pDer = parseIntStrict(der, iDer);
        if (pDer == NumParse::Malformed)
        {
            r.causa = CompararCausa::DerechaMalformada;
            return r;
        }

        if (op == ">")  r.valor = iIzq >  iDer;
        else if (op == "<")  r.valor = iIzq <  iDer;
        else if (op == ">=") r.valor = iIzq >= iDer;
        else                 r.valor = iIzq <= iDer;

        return r;
    }

    // ── LA VOZ ──────────────────────────────────────────────────────────────
    //
    // Ensena la forma correcta EN ESPANOL, que es la ley de avisos del motor
    // (ECLIPSCRIPT_DOCS §2). Y ensena `${}` -- que es la razon por la que F1 y
    // F4 son un solo lote: sin la interpolacion puesta, esta voz tendria que
    // decir "no se puede", y con ella puede decir COMO SE ESCRIBE.
    //
    // `der` es el texto YA interpolado. Cuando el creador escribio `${pepe}` y
    // `pepe` no existe, `interpolateVars` deja el literal `${pepe}` y ya
    // hablo por su cuenta; aca llega como basura y la voz lo nombra tal cual,
    // que es lo que hace obvio el typo.
    inline std::string compararVoz(CompararCausa causa,
                                   const std::string& nombreVar,
                                   const std::string& valorIzq,
                                   const std::string& op,
                                   const std::string& der)
    {
        const std::string linea = nombreVar + " " + op + " " + der;

        switch (causa)
        {
        case CompararCausa::DerechaMalformada:
            return "[si] '" + linea + "': el lado derecho tiene que ser un "
                   "numero, y '" + der + "' no lo es. Si querias comparar con "
                   "la VARIABLE '" + der + "', escribi 'si " + nombreVar + " "
                   + op + " ${" + der + "}'. Por ahora la condicion da falso.";

        case CompararCausa::IzquierdaMalformada:
            return "[si] '" + linea + "': la variable '" + nombreVar + "' vale "
                   "'" + valorIzq + "', que no es un numero, asi que no se "
                   "puede comparar con " + op + ". Para comparar texto usa "
                   "'==' o '!='. Por ahora la condicion da falso.";

        case CompararCausa::OperadorDesconocido:
            return "[si] '" + linea + "': operador desconocido '" + op
                   + "'. Los que hay son: == != > < >= <=.";

        case CompararCausa::Ok:
        default:
            return std::string();
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // LA ARITMETICA DE `mutar` / `set` -- la otra mitad de la ficha
    // ════════════════════════════════════════════════════════════════════════
    //
    // Misma enfermedad, otro default: el `catch` dejaba `delta = 1`, asi que
    // `mutar dif - natsuki` restaba UNO y no avisaba. Y el valor actual de la
    // variable caia a 0, asi que `mutar relacion + 1` sobre una variable que
    // decia "buena" la convertia en 1 -- destruyendo un string en silencio.

    enum class MutarCausa
    {
        Ok,
        ActualMalformado,   // la variable no tiene un numero adentro
        DeltaMalformado     // el operando no es un numero
    };

    struct MutarPaso
    {
        bool       aplicar   = false;  // false = NO tocar la variable
        int        resultado = 0;
        MutarCausa causa     = MutarCausa::Ok;
    };

    // `op` es `+` o `-` (el `=` no pasa por aca: no hay aritmetica que hacer).
    // Fail-closed de verdad: si algo no parsea, la variable NO SE TOCA. Es
    // mas duro que la comparacion a proposito -- una comparacion que da falso
    // se puede volver a preguntar, una variable pisada con un numero
    // inventado ya perdio su valor.
    inline MutarPaso mutarAritmetica(const std::string& actual,
                                     const std::string& op,
                                     const std::string& valor)
    {
        MutarPaso p;

        int iActual = 0;
        if (parseIntStrict(actual, iActual) == NumParse::Malformed)
        {
            p.causa = MutarCausa::ActualMalformado;
            return p;
        }

        int iDelta = 0;
        if (parseIntStrict(valor, iDelta) == NumParse::Malformed)
        {
            p.causa = MutarCausa::DeltaMalformado;
            return p;
        }

        p.aplicar   = true;
        p.resultado = (op == "+") ? (iActual + iDelta) : (iActual - iDelta);
        return p;
    }

    inline std::string mutarVoz(MutarCausa causa,
                                const std::string& nombreVar,
                                const std::string& actual,
                                const std::string& op,
                                const std::string& valor)
    {
        const std::string linea = nombreVar + " " + op + " " + valor;

        switch (causa)
        {
        case MutarCausa::DeltaMalformado:
            return "[mutar] '" + linea + "': el operando tiene que ser un "
                   "numero, y '" + valor + "' no lo es. Si querias usar la "
                   "VARIABLE '" + valor + "', escribi 'mutar " + nombreVar
                   + " " + op + " ${" + valor + "}'. Por ahora '" + nombreVar
                   + "' no cambia.";

        case MutarCausa::ActualMalformado:
            return "[mutar] '" + linea + "': '" + nombreVar + "' vale '"
                   + actual + "', que no es un numero, asi que no se le puede "
                   + (op == "+" ? "sumar" : "restar") + ". Por ahora no "
                   "cambia. (Para reemplazar su valor usa '=', que no hace "
                   "cuentas.)";

        case MutarCausa::Ok:
        default:
            return std::string();
        }
    }

}  // namespace eclips
