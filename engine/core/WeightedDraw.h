// ============================================================================
// WeightedDraw.h — el SORTEO: contrato puro del eje CUAL del azar
// ============================================================================
//
// Nucleo de la pierna B de `DISENO_AZAR_ECLIPS.md` (el bloque `azar:`).
//
// ── QUE ES, Y POR QUE NO VIVE EN `TrialClock.h` ─────────────────────────────
//
// El mapa del azar tiene cuatro ejes (CUANDO · CUAL · MODULACION · OCUPACION).
// La pierna A declaro el CUANDO y su header lo dice en la primera linea. Este
// archivo es el CUAL: elegir UNA de N. Meterlo dentro de `TrialClock.h`
// volveria mentiroso el titulo de ese header, que es exactamente la enfermedad
// que el drenaje SYN viene deshaciendo (4 parsers de hex, 3 leyes de desempate
// bilingue, 4 leyes de comillas, 5 parsers de corchetes, 61 `stof` sin `&pos`).
//
// Y hay un consumidor futuro que lo pide separado: `DISENO_MUTAR_PALABRAS` usa
// el SORTEO sin el RELOJ (su `F6` era este primitivo escrito adentro de un
// caller — la sexta copia, cazada ANTES de shipear, con costo de migracion
// cero). Con los dos ejes en headers distintos, incluye solo lo que usa.
//
// ── LO QUE SI SE COMPARTE: LA TIRADA ────────────────────────────────────────
//
// `trialRoll01` vive en `TrialClock.h` y aca se REUSA, no se copia. El span+1
// que tiene adentro no es cosmetico (sin el, la tirada puede dar exactamente
// 1.0), y una segunda copia de esa sutileza seria la septima copia de la
// enfermedad de arriba. Si algun dia aparece un tercer consumidor de la tirada
// sin reloj y sin sorteo (candidato: `_azar`, seccion 4.6 del contrato), la
// tirada se merece casa propia; con dos, mudarla cuesta mas de lo que aclara.
//
// ── LAS LEYES QUE VIVEN AQUI (DISENO_AZAR_ECLIPS seccion 5) ─────────────────
//
//   ley 3   la exclusividad la carga el BLOQUE: un `azar:` es UN ensayo con N
//           salidas. Lineas sueltas serian N ensayos independientes
//   ley 4   LOS PESOS NO SUMAN NADA. "No sale nada" ya tiene dueno —la
//           `probabilidad` del eje CUANDO—, asi que un `azar:` con alguna
//           entrada viva SIEMPRE produce una salida. `peso=` es un reparto
//           relativo: escalar todos los pesos no cambia una sola tirada
//   ley 5   el modo de muestreo es del BLOQUE y se declara CON VALOR:
//             `repetir=si` -> distribucion (con reposicion)  -> `weightedPick`
//             `repetir=no` -> bolsa (sin reposicion, rebaraja) -> `bagPick`
//           y bajo bolsa `peso=` significa OTRA COSA: gobierna el ORDEN del
//           ciclo, no la frecuencia (cada elegible sale exactamente una vez
//           por ciclo). Por eso el default es del bloque y no del motor: no
//           hay un default correcto para las dos formas
//   ley 7   EL SORTEO JAMAS SE SERIALIZA. `DrawBag` es estado de corrida:
//           `bagClear` al cargar slot o script. "Load = barajado fresco",
//           observado en DDLC real y ya escrito por `EffectInstance.rngState`
//   ley 12  la semilla es reproducible: una llamada consume EXACTAMENTE una
//           tirada, asi que la misma semilla da la misma corrida
//
// El PARSER habla (con numero de linea) y este nucleo CLAMPEA — la misma
// reparticion que la pierna A dejo escrita. Aca no hay un solo `warn`: un peso
// podrido se apaga y el bloque sigue repartiendo entre los sanos.
//
// Lo prueba `Resumenes_Sesiones/Harness/harness_peso.cpp`.
// ============================================================================

#pragma once

#include "TrialClock.h"
#include "Types.h"

#include <cmath>
#include <vector>

namespace eclips
{
    // "Este bloque no puede producir nada": todas las entradas apagadas, o el
    // bloque esta vacio. No es un indice valido y el caller no debe tratarlo
    // como uno — quien avisa es el parser, que tiene el numero de linea.
    inline constexpr int kDrawNone = -1;

    // ── Saneamiento del peso (el nucleo clampea) ────────────────────────────
    // Un peso que no es un numero no es "muy chico": es una entrada que no se
    // entiende, y una entrada que no se entiende no compite. Cae a 0, que en
    // este eje ya tiene significado firmado — APAGADA, no borrada — asi que el
    // caso podrido reusa una ley que ya existe en vez de inventar una segunda.
    //
    // El negativo cae al mismo lugar por la misma razon que en la ley 9 del
    // eje CUANDO: un peso negativo no significa "menos que nunca".
    inline f32 clampDrawWeight(f32 raw)
    {
        if (!std::isfinite(raw)) return 0.0f;
        if (raw < 0.0f)          return 0.0f;
        return raw;
    }

    // ── El sorteo ponderado (ley 4) ─────────────────────────────────────────
    //
    // Reparto RELATIVO: `peso=5 / peso=50 / peso=55` da 4.5% / 45.5% / 50%, y
    // multiplicar los tres por diez da la corrida identica. Eso es lo que
    // distingue `peso=` de una probabilidad, y es justo la confusion que mato
    // al `[prob:]` de `mutar-palabras` (*"peso relativo, no promesa de
    // porcentaje exacto"*, confesado en su propio parentesis).
    //
    // Consume UNA tirada, siempre, tambien cuando hay una sola entrada viva:
    // un consumo variable romperia la ley 12 en cuanto alguien cambiara un
    // peso, que es precisamente el momento en que la semilla sirve (balancear
    // pesos repitiendo la corrida).
    template <class Rng>
    inline int weightedPick(const std::vector<f32>& pesos, Rng& rng)
    {
        f64 total = 0.0;
        for (const f32 w : pesos) total += (f64)clampDrawWeight(w);

        const f64 objetivo = trialRoll01(rng) * total;
        if (total <= 0.0) return kDrawNone;

        f64 acc = 0.0;
        int ultimoVivo = kDrawNone;
        for (int i = 0; i < (int)pesos.size(); ++i)
        {
            const f64 w = (f64)clampDrawWeight(pesos[i]);
            if (w <= 0.0) continue;         // apagada: no compite
            ultimoVivo = i;
            acc += w;
            if (objetivo < acc) return i;
        }

        // Guarda de redondeo, no rama muerta: `acc` se acumula en pasos y
        // puede quedar un ULP por debajo de `total`. Sin esto, una tirada
        // pegada al techo devolveria kDrawNone en un bloque perfectamente
        // sano, y la ley 4 dice que SIEMPRE sale algo.
        return ultimoVivo;
    }

    // ── La bolsa (ley 5, modo `repetir=no`) ─────────────────────────────────
    //
    // Estado de CORRIDA, jamas de disco (ley 7). Va paralelo al vector de
    // pesos: `sacado[i]` es "la entrada i ya salio en este ciclo".
    struct DrawBag
    {
        std::vector<u8> sacado;
        i32             restantes = 0;      // elegibles que quedan en el ciclo
    };

    // Load de slot, cambio de script: el ciclo viejo no significa nada.
    inline void bagClear(DrawBag& b)
    {
        b.sacado.clear();
        b.restantes = 0;
    }

    // Rebarajar = todos vuelven a estar disponibles. Los apagados no cuentan:
    // `peso=0` saca a la entrada del ciclo entero, no la pone al final.
    inline void bagRefill(DrawBag& b, const std::vector<f32>& pesos)
    {
        b.sacado.assign(pesos.size(), (u8)0);
        b.restantes = 0;
        for (const f32 w : pesos)
            if (clampDrawWeight(w) > 0.0f) ++b.restantes;
    }

    // ── Resincronizar el ciclo (MUTAR_PALABRAS CP3, fork 1a) ────────────────
    //
    // `restantes` se cuenta en `bagRefill` CON LOS PESOS DE ESE MOMENTO, y eso
    // alcanzo mientras los pesos fueron FIJOS: el parser los escribe una vez y
    // el bloque `azar:` no los mueve nunca. El gate `[si:]` del pool de
    // `palabras` es **el primer caller que los cambia ENTRE VISITAS sobre la
    // MISMA bolsa** -la bolsa vive tecleada por escena+linea-, y ahi esa cuenta
    // pasa a hablar de un mundo que ya no existe. Por los DOS lados, y las dos
    // caras se midieron:
    //
    //   de mas    el ciclo se agota sin que `restantes` llegue a 0, asi que no
    //             se rebaraja y todas las que quedan pesan 0: la pagina sale
    //             CORTA, o vacia -y una vacia cae en el aviso "todas en peso=0"
    //             siendo falso-. 4158 de 80588 visitas, 2589 vacias.
    //   de menos  `restantes` llega a 0 con elegibles sin sacar todavia, asi
    //             que se rebaraja ANTES DE TIEMPO y una palabra ya vista
    //             vuelve. 35950 de 80588.
    //
    // Esto NO repara un defecto de la bolsa: le devuelve su invariante cuando
    // el vector de pesos cambio por abajo. `sacado` no se toca -el indice ES la
    // identidad de la entrada, y moverlo seria compactar, que es el fork que
    // se descarto por perder el ciclo (`harness_palabras` 9.4)-: solo recuenta.
    //
    // Sin caller en `azar:` a proposito: alla los pesos no se mueven, asi que
    // llamarlo seria trabajo por frame para restaurar una invariante intacta.
    // Guardian de que siga sin moverle la corrida: `harness_palabras` 8.0.
    inline void bagResync(DrawBag& b, const std::vector<f32>& pesos)
    {
        // Tamano distinto = otro bloque: `bagPick` ya rebaraja solo, y recontar
        // sobre vectores de largo distinto seria leer fuera del `sacado`.
        if (b.sacado.size() != pesos.size()) return;

        i32 r = 0;
        for (size_t i = 0; i < pesos.size(); ++i)
            if (!b.sacado[i] && clampDrawWeight(pesos[i]) > 0.0f) ++r;
        b.restantes = r;
    }

    // Una tirada de la bolsa: sortea POR PESO entre las que todavia no
    // salieron. De ahi sale, sin codigo extra, la mitad rara de la ley 5 —
    // *bajo bolsa el peso gobierna el ORDEN y no la frecuencia*: la frecuencia
    // ya esta fijada en "una vez por ciclo" para todas, asi que lo unico que
    // el peso puede mover es CUANDO le toca a cada una.
    template <class Rng>
    inline int bagPick(DrawBag& b, const std::vector<f32>& pesos, Rng& rng)
    {
        // El tamano cambio (hot-reload, otro script): un ciclo que habla de
        // entradas que ya no existen es peor que ninguno.
        if (b.sacado.size() != pesos.size()) bagRefill(b, pesos);
        if (b.restantes <= 0)                bagRefill(b, pesos);

        f64 total = 0.0;
        for (size_t i = 0; i < pesos.size(); ++i)
            if (!b.sacado[i]) total += (f64)clampDrawWeight(pesos[i]);

        const f64 objetivo = trialRoll01(rng) * total;
        if (total <= 0.0) return kDrawNone;     // todas apagadas

        f64 acc = 0.0;
        int ultimoVivo = kDrawNone;
        for (int i = 0; i < (int)pesos.size(); ++i)
        {
            if (b.sacado[i]) continue;
            const f64 w = (f64)clampDrawWeight(pesos[i]);
            if (w <= 0.0) continue;
            ultimoVivo = i;
            acc += w;
            if (objetivo < acc)
            {
                b.sacado[(size_t)i] = (u8)1;
                --b.restantes;
                return i;
            }
        }

        if (ultimoVivo >= 0)                    // misma guarda de redondeo
        {
            b.sacado[(size_t)ultimoVivo] = (u8)1;
            --b.restantes;
        }
        return ultimoVivo;
    }
}
