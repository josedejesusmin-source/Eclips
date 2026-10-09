// ============================================================================
// QteGate.h - LA PUERTA DEL QTE
// ============================================================================
//
// Nucleo de la ficha `QTE-SIN-PUERTA` (§7.470).
//
// ── QUE RESUELVE ────────────────────────────────────────────────────────────
//
// El QTE se dibujaba, corria su reloj y **siempre caia a `fallo`**: ninguna
// tecla lo resolvia. Lo trajo el fundador al pedirsele el feel-test del CP0 del
// atomo de teclado, y al medirlo aparecio que no era una linea faltante. La
// rama del exito estaba cortada en **CUATRO eslabones seguidos**, y cada uno
// solo bastaba para dejarla muerta:
//
//   1  el PRODUCTOR   `translateSDLEventToAction` nunca devolvia `QTEKey`
//   2  la CARGA       los dos call-sites llamaban `handleAction(action)`, y la
//                     firma es `(action, choiceIndex = -1, qteKey = 0)`: la
//                     tecla cruda llegaba en 0, asi que el comparador de
//                     `VNDialogBox.cpp` comparaba 0 contra la tecla pedida
//   3  la SALIDA      el acierto ponia `m_qteTimer = 0`, y `QTEFinished` se
//                     emitia ADENTRO de `if (m_qteTimer > 0)`. Con el reloj ya
//                     en cero esa rama no volvia a correr: **acertar no emitia
//                     ningun evento y la fase quedaba en `WaitingQTE` para
//                     siempre**. El exito era un cuelgue
//   4  el VEREDICTO   `result.qteSuccess = false` estaba hardcodeado, y
//                     `qteSucceeded()` tenia cero callers: aunque el evento
//                     saliera, el state leia `false` y saltaba a `fallo`
//
// Seis capas de superficie -parser, gramatica de bloque, render propio, tres
// claves de tema, fuente propia, seccion del manual y asserts en dos
// harnesses- encima de un camino de input que no existia. §7.470 en vivo.
//
// ── POR QUE UN HEADER APARTE, Y PURO ────────────────────────────────────────
//
// Mismo motivo que `AppearPolicy.h` (RO-5), `BoxPersistence.h` y
// `GroupAlpha.h`: los DOS consumidores son inalcanzables para la bateria
// -`VNScriptState.h` es el header gigante y `VNDialogBox.cpp` arrastra GL-, asi
// que una ley que viva adentro **no se puede probar**. Aca no depende de SDL:
// la tecla es un `uint32_t`, que es exactamente el tipo que `handleAction` ya
// recibia. Lo unico que se importa es `VNPhase`, que ya vive en un header
// liviano por la misma razon.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   EL QTE RECLAMA EN SU FASE Y EN NINGUNA OTRA. `qteReclamaTecla` es
//           el productor entero. Si esta ley cae, el lote no arreglo el QTE:
//           le cambio el teclado a las otras seis fases del interprete
//   ley 2   EL QUE NO ACIERTA **NO SE CONSUME**. Es la ley que mantiene vivo
//           al `ESC`, y es la firma del fundador (fork 1(a), 2026-08-05). Una
//           tecla que no es la del QTE vuelve `false` desde `handleAction` y
//           sigue su camino a los atajos globales: la pausa sigue abriendo, el
//           guardado rapido sigue guardando, todo identico a hoy. Y por el
//           mismo mecanismo, `qte [tecla: ESC]` FUNCIONA: ahi el ESC acierta,
//           se consume, y la pausa no abre. **Un comparador, no dos**: el
//           traductor no sabe que tecla pidio el QTE - solo dice "esto es
//           candidato", y el juez vive aca
//   ley 3   UN QTE SE RESUELVE UNA SOLA VEZ. Hermana de "una aparicion se
//           resuelve UNA vez" (`AppearPolicy`, `D-10`). Sin ella, el reloj que
//           sigue corriendo despues de un acierto lo pisaria con un `fallo`
//   ley 4   UNA SOLA PUERTA PARA EL EVENTO. Las DOS salidas -acierto y reloj
//           vencido- pasan por `hayEvento()`/`exito()`, y `update` las emite en
//           el mismo sitio que `m_pendingAdvance` y `m_pendingChoice`. El
//           eslabon 3 existio porque habia dos caminos y uno no estaba
//           construido; con una puerta no se puede repetir.
//           **Costo declarado:** el vencimiento emite UN FRAME despues (el
//           reloj llega a cero a mitad de `update`, y la puerta esta arriba).
//           Son 16 ms al final de un QTE de 3000: invisible, y comprado a
//           cambio de que las dos salidas no puedan desincronizarse
//   ley 5   UN OVERLAY ENCIMA LE GANA AL QTE. Con el historial abierto -y se
//           abre con la rueda TAMBIEN durante un QTE- el comando NO reclama la
//           tecla. Es §7.88 (*"un overlay modal captura su propio input por
//           esta-abierto, no por la fase de fondo"*), y sin la guarda el ESC
//           entraria como `QTEKey`, el log se lo comeria sin cerrarse y el
//           jugador quedaria adentro sin salida. **Aparecio escribiendo el
//           cable, no auditando**
//
// ── LO QUE ESTE HEADER NO PUEDE VER ─────────────────────────────────────────
//
// El CABLE, y son los cuatro eslabones:
//   · que `translateSDLEventToAction` pregunte de verdad por `qteReclamaTecla`;
//   · que el call-site pase la tecla cruda (via `inputBindDeEvento`, que es
//     H10-safe por construccion: lee el union dentro de su `case`);
//   · que `handleAction` juzgue con `qteVeredicto` y devuelva `qteConsume`;
//   · que `update` emita por la puerta unica.
// Misma pared que `MERGE-DUPLICADO`. Lo cubre la pantalla: escribir un `qte` y
// apretar la tecla.
// ============================================================================

#pragma once

#include "engine/core/VNExecutionPolicy.h"

#include <cstdint>

namespace eclips
{
    // ── 1 · EL PRODUCTOR (leyes 1 y 5) ──────────────────────────────────────
    //
    // Todo lo que el traductor necesita saber. NO recibe la tecla: si la
    // recibiera estaria juzgando, y el veredicto ya tiene dueno abajo. Que sea
    // una funcion y no un `==` suelto es para que el harness pueda barrer las
    // siete fases y dejar escrito que solo una reclama.
    //
    // El segundo parametro es la ley 5, y **aparecio escribiendo el cable, no
    // auditando**: el historial se abre con la rueda TAMBIEN durante un QTE
    // (`VNScriptState.h:2413`), y ese overlay se traga todo lo que llega
    // (`VNDialogBox.cpp:1428`). Sin esta guarda, el ESC entraria como `QTEKey`,
    // el overlay lo comeria sin cerrarse, y el jugador quedaria con el log
    // abierto y sin salida. Es §7.88 tal cual: **un overlay modal captura su
    // propio input por "esta-abierto", no por la fase de fondo** - y acá la fase
    // de fondo es justo la que quiere reclamar.
    inline bool qteReclamaTecla(VNPhase fase, bool overlayModalAbierto)
    {
        return fase == VNPhase::WaitingQTE && !overlayModalAbierto;
    }

    // ── 2 · EL JUEZ ─────────────────────────────────────────────────────────
    //
    // Tres valores y no dos, porque "no acerto" y "no habia QTE" se resuelven
    // igual en pantalla y **cuestan distinto**: el primero es una tecla que el
    // jugador apreto durante el QTE y que tiene que seguir su camino; el
    // segundo es un evento que nunca fue del QTE.
    enum class QteVeredicto : unsigned char
    {
        NoReclama,   // no hay QTE corriendo: esto no es asunto del comando
        Falla,       // hay QTE y no es su tecla -> NO se consume (ley 2)
        Acierta      // hay QTE y es su tecla   -> se consume y resuelve
    };

    // `teclaEvento == 0` es `SDLK_UNKNOWN`, y es lo que llega cuando el evento
    // no es de teclado (un clic, la rueda) o cuando el call-site se olvido de
    // pasar la tecla - que es EXACTAMENTE el eslabon 2. Nunca acierta, ni
    // siquiera contra una `teclaPedida` en 0: un QTE sin tecla no se gana
    // apretando cualquier cosa.
    inline QteVeredicto qteVeredicto(bool relojCorre,
                                     uint32_t teclaEvento,
                                     uint32_t teclaPedida)
    {
        if (!relojCorre) return QteVeredicto::NoReclama;
        if (teclaEvento != 0u && teclaEvento == teclaPedida)
            return QteVeredicto::Acierta;
        return QteVeredicto::Falla;
    }

    // ── 3 · LA LEY QUE MANTIENE VIVO AL ESC (ley 2) ─────────────────────────
    //
    // Una linea, y es la mitad del lote. Vive aparte del veredicto para que
    // cambiarla sea EDITAR ESTA FUNCION: la forma descartada del fork 1(c)
    // -"el QTE es un modal duro y traga todo"- es literalmente
    // `return v != QteVeredicto::NoReclama`, y el §7 del harness la corre para
    // mostrar que mata la pausa mientras el reloj baja.
    inline bool qteConsume(QteVeredicto v)
    {
        return v == QteVeredicto::Acierta;
    }

    // ── 4 · LA PUERTA UNICA DEL EVENTO (leyes 3 y 4) ────────────────────────
    //
    // No tiene reloj, y es a proposito: el reloj es de la caja
    // (`m_qteTimer`) y duplicarlo aca seria el pecado de siempre. Esto guarda
    // el VEREDICTO y si ya se anuncio, que es lo unico que el eslabon 3 no
    // tenia donde vivir.
    class QteResolucion
    {
    public:
        // `startQTE`. Un QTE nuevo nace sin veredicto: no hereda el del
        // anterior ni un evento que nadie llego a consumir.
        void armar()
        {
            m_resuelto  = false;
            m_pendiente = false;
            m_exito     = false;
        }

        void acertar() { resolver(true); }    // la tecla pedida
        void vencer()  { resolver(false); }   // el reloj llego a cero

        bool resuelto()  const { return m_resuelto;  }
        bool hayEvento() const { return m_pendiente; }
        bool exito()     const { return m_exito;     }

        // `update` ya lo emitio. Una vez y solo una.
        void consumirEvento() { m_pendiente = false; }

    private:
        // ley 3: el primero que llega manda. El segundo no es un error - es el
        // reloj que sigue bajando un frame despues del acierto.
        void resolver(bool exito)
        {
            if (m_resuelto) return;
            m_resuelto  = true;
            m_pendiente = true;
            m_exito     = exito;
        }

        bool m_resuelto  = false;
        bool m_pendiente = false;
        bool m_exito     = false;
    };

} // namespace eclips
