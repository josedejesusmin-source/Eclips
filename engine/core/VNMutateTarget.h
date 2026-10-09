// ============================================================================
// ECLIPS ENGINE - VNMutateTarget  (CP0 de DISENO_MUTAR_PALABRAS.md: LA FAMILIA)
// ============================================================================
//
// El `F1` del contrato: `dispatchTarget` gana una rama, y LA FORMA DECIDE.
//
//     "Quedarse"  -> escena_a        <- 1 token: escena o verbo shell (hoy)
//     "Ayudarla"  -> afecto + 1      <- 3 tokens con operador: MUTACION (esto)
//
// Las tres caras del atomo clickable -choice, hotspot, enlace- rutean por un
// solo `dispatchTarget`, asi que la rama las alcanza a las tres de una vez.
//
// ----------------------------------------------------------------------------
// POR QUE ESTA LEY VIVE ACA Y NO ADENTRO DE `VNScriptState.h`
// ----------------------------------------------------------------------------
// `VNScriptState.h` es el header gigante y arrastra GL: ningun harness de la
// bateria lo puede compilar. Una ley escrita ahi adentro no la prueba nadie -
// es la pared de la ficha `MERGE-DUPLICADO`, y es la misma razon por la que
// `LangPaths.h`, `QteGate.h`, `VNActionSeal.h` y `FocusRing.h` existen.
//
// Aca es SDL-free (solo <sstream>, <string> y `EclipsParse.h`), y su linea del
// `.bat` -fuera del bloque EXTRAINC- es el guardian de que siga siendolo.
//
// ----------------------------------------------------------------------------
// LO QUE LA AUDITORIA ENCONTRO, Y QUE EL CONTRATO NO SABIA (2026-08-15)
// ----------------------------------------------------------------------------
// (1) LA FORMA YA ES EXPRESABLE Y ESTA MUERTA. El destino se parsea como
//     `trim(rest.substr(arrow + 2))` -la linea entera despues de la flecha-,
//     asi que `-> vida + 1` YA se escribe, YA llega a `dispatchTarget`, y ahi
//     imprime "no es una escena ni un verbo shell" y no hace nada. Esta ley no
//     le cambia la conducta a nada que funcione hoy: revive una forma muerta.
//
// (2) EL TOKENIZADOR DEL TRIPLE ESTABA ESCRITO TRES VECES, en el mismo archivo
//     y con el mismo cuerpo de siete lineas: `set` (EclipScript.h:3125),
//     `meta_set` (:3173) y `si` (:3218). Lo que difiere entre ellos NO es el
//     reparto: es la ALLOWLIST de operadores (`= + -` los dos primeros,
//     `== != > < >= <=` el tercero) y la ley de aridad. Por eso este header
//     parte en dos lo que parecia uno: `mutaTokenizar` reparte SIN JUZGAR, y
//     cada caller pone su propia ley encima.
//
//     `set` y `meta_set` adoptan el tokenizador en este CP (fork 2a firmado).
//     `si` NO se toca -es la rama mas caliente del corpus y no es de este
//     contrato-: queda la ficha `TRIPLE-EN-SI`, con el tokenizador ya escrito
//     para que su adopcion sea una linea.
//
//     Y hay DOS copias mas en el otro dialecto: `MapData.h:1143` (`set:` de un
//     `.eclmap`, monolingue) y `:1527` (`salida: variable v == 1`, que reparte
//     el triple con el mismo `iss >>`). Son zona de Gameplay 2D/Codex y NO se
//     tocan aca; quedan fichadas.
//
// (3) "UNA ESCENA ES SIEMPRE 1 TOKEN" ES CONVENCION, NO CONSTRUCCION. El
//     parser toma TODO el resto de la linea como nombre de escena
//     (`EclipScript.h:2001`), asi que `escena vida = 10` seria una escena legal
//     de tres tokens y esta ley se la comeria. Medido sobre el corpus vivo:
//     479 declaraciones de escena en 196 archivos, CERO con nombre
//     multi-token. La premisa aguanta, y por eso el §5 del harness la afirma
//     como MEDICION en vez de dejarla como comentario.
//
// ----------------------------------------------------------------------------
// LAS TRES FORMAS, Y POR QUE SON TRES Y NO DOS
// ----------------------------------------------------------------------------
//   NoEsMutacion -> el destino no lleva operador de la familia. El orden viejo
//                   queda INTACTO: escena -> verbo shell -> error. Es el caso
//                   de los 479 destinos que existen hoy.
//   Valida       -> `var op valor` con la ley de SYN-2. Muta y continua.
//   Rota         -> LLEVA operador de la familia pero no cumple la ley
//                   (`-> vida +`, sin operando). No cae al camino de escena:
//                   eso lo dejaria mudo bajo el error generico "no es una
//                   escena". Se resuelve fail-closed y CON VOZ, que es la ley
//                   §7.89 del resto del parser.
//
// El discriminador de la Rota es deliberadamente estrecho: pide que el segundo
// token sea EXACTAMENTE uno de los tres operadores. `-> vida * 3` NO es una
// mutacion rota, es un destino que no existe - porque reclamar todo lo que
// tenga tres tokens seria robarle el nombre a una escena que el motor permite
// declarar (ver el hallazgo 3).
// ============================================================================

#pragma once

#include "engine/core/EclipsParse.h"

#include <sstream>
#include <string>

namespace eclips
{

    // ── Que ES este destino ─────────────────────────────────────────────────
    enum class MutaForma
    {
        NoEsMutacion,   // no lleva operador de la familia -> escena / verbo
        Valida,         // `var op valor` cumpliendo SYN-2
        Rota            // lleva operador y NO cumple -> fail-closed con voz
    };

    // El reparto crudo, sin juicio. Los tres callers vivos del triple usan
    // exactamente este cuerpo; lo que cambia entre ellos es la ley de encima.
    struct MutaTokens
    {
        std::string var;
        std::string op;
        std::string valor;
    };

    struct MutaTriple
    {
        MutaForma   forma = MutaForma::NoEsMutacion;
        std::string var;
        std::string op;      // "=" | "+" | "-"
        std::string valor;
    };

    // ── EL TOKENIZADOR ──────────────────────────────────────────────────────
    //
    // Cuerpo verbatim de las tres copias de `EclipScript.h`. Las dos reglas que
    // NO son obvias y que el guardian de no-op custodia:
    //
    //   - el valor es TODO el resto de la linea, no el tercer token. Por eso
    //     `set ruta = camino largo` guarda "camino largo" y no "camino".
    //   - si el resto arranca con comilla, gana el PRIMER par de comillas
    //     (`extraerEntreComillas`, no el glotón). Asi `set mc = "Jesus"` guarda
    //     Jesus sin comillas, y `set g = "{wavy}A{/wavy}"` conserva los tags.
    //
    // Sin operador (un solo token) devuelve `op` vacio, que es lo que hace que
    // los destinos de hoy caigan solos en `NoEsMutacion`.
    inline MutaTokens mutaTokenizar(const std::string& rest)
    {
        MutaTokens t;
        std::istringstream iss(rest);
        iss >> t.var >> t.op;

        std::string remainder;
        std::getline(iss, remainder);
        remainder = trimBlancos(remainder);

        if (!remainder.empty() && remainder[0] == '"')
            extraerEntreComillas(remainder, t.valor);
        else
            t.valor = remainder;

        return t;
    }

    // ── LA ALLOWLIST DE LA FAMILIA ──────────────────────────────────────────
    // Tres operadores, y son los mismos que `set` acepta desde SYN-2. `si` NO
    // comparte esta lista a proposito: sus operadores son de COMPARACION, y
    // mezclarlas dejaria `-> vida == 3` mutando en vez de avisar.
    inline bool mutaEsOperador(const std::string& op)
    {
        return op == "=" || op == "+" || op == "-";
    }

    // ── LA LEY DE ARIDAD (SYN-2, verbatim de EclipScript.h:3149) ────────────
    // `=` admite valor vacio (`set nombre = ` deja la cadena vacia, y eso es
    // conducta viva). `+` y `-` EXIGEN operando: sin el, el `stoi` del runtime
    // caeria al default 1 y el creador tendria un incremento que no escribio.
    inline bool mutaAridadOk(const MutaTokens& t)
    {
        return !t.var.empty() &&
               (t.op == "=" ||
                ((t.op == "+" || t.op == "-") && !t.valor.empty()));
    }

    // ── LA LEY DEL TARGET: ¿este destino es una mutacion? ───────────────────
    //
    // El orden de las dos preguntas NO es indiferente, y es lo que hace que
    // esta ley sea aditiva: primero "¿lleva operador de la familia?", y recien
    // despues "¿cumple la aridad?". Al reves -validar primero y clasificar
    // despues- un destino cualquiera de tres tokens caeria en el camino de la
    // mutacion rota y se comeria el error viejo de destino inexistente, que es
    // el que hoy le habla al creador que escribio mal el nombre de una escena.
    //
    // `op` vacio (un solo token) sale por la primera puerta: son los 479
    // destinos que existen hoy, y por eso este lote no mueve un solo sitio.
    inline MutaTriple mutaClasificarTarget(const std::string& target)
    {
        const MutaTokens t = mutaTokenizar(target);

        MutaTriple r;
        if (!mutaEsOperador(t.op)) return r;   // NoEsMutacion: escena / verbo

        r.var   = t.var;
        r.op    = t.op;
        r.valor = t.valor;
        r.forma = mutaAridadOk(t) ? MutaForma::Valida : MutaForma::Rota;
        return r;
    }

    // ── EL VEREDICTO (fork 1a) ──────────────────────────────────────────────
    //
    // `dispatchTarget` devolvia `void`, y sus cuatro call-sites asumen "salto o
    // fallo": el choice hace `break` sin tocar fase ni indice, el hotspot y el
    // enlace hacen `return true` (consumir). Si la rama muta y NO salta, esa
    // suposicion deja al juego clavado en la pantalla de opciones.
    //
    // Por eso el dispatcher pasa a DECIR QUE HIZO y cada cara escribe su
    // "continuar" -que no es el mismo en las tres, y ahi murio el fork 1b:
    // para el enlace continuar significa NO CONSUMIR (`VNScriptState.h:1204`),
    // y el `return true/false` es del caller, no del dispatcher.
    enum class TargetVeredicto
    {
        Salto,            // era una escena: `jumpToScene` ya corrio
        VerboShell,       // era `guardar`/`menu`/`salir`/...: ya corrio (o lo veto el sello)
        Mutacion,         // se aplico el triple: la cara resuelve y sigue
        MutacionRota,     // tenia forma de mutacion y no lo era: ya aviso
        PantallaSistema,  // abrio o intento abrir una pantalla libre registrada
        NoResuelto        // ni escena ni verbo: ya aviso (conducta vieja)
    };

}  // namespace eclips
