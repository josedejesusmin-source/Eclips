// ============================================================================
// ECLIPS ENGINE - StateStack (Pila de Estados)
// ============================================================================
// Documento de diseño: Sección 10.11
//
// La pila de estados es lo que permite la transición fluida entre
// Visual Novel y Gameplay 2D sin destruir nada.
//
// Flujo ejemplo:
//   [VN]                           ← Jugador leyendo
//   [VN] [Gameplay]                ← Push: gameplay encima, VN pausada
//   [VN] [Gameplay] [DialogOverlay]← Push: diálogo sobre gameplay
//   [VN] [Gameplay]                ← Pop: diálogo termina
//   [VN]                           ← Pop: gameplay termina, VN continúa
//
// En cada frame:
//   1. Solo el estado de la cima recibe input
//   2. Se renderizan de abajo hacia arriba (los transparentes dejan ver abajo)
//   3. Se actualizan según allowsUpdateBelow()
// ============================================================================

#pragma once

#include "GameState.h"
#include <vector>
#include <memory>
#include <algorithm>   // std::remove, std::find (resume-neto de la cola)
#include <iostream>    // std::cout (log de cmdPop descartado)

namespace eclips
{
    class StateStack
    {
    public:
        StateStack() = default;
        ~StateStack() = default;

        // ── Operaciones de pila ──

        // Apilar un nuevo estado encima
        void push(std::unique_ptr<GameState> state)
        {
            // Pausar el estado actual (si hay uno)
            if (!m_states.empty())
            {
                m_states.back()->onPause();
            }

            // Entrar al nuevo estado
            state->onEnter();
            m_states.push_back(std::move(state));
        }

        // ── Cola de comandos del stack (F3, sesión Ciclo-de-Vida-B) ──────────
        // Estas operaciones ENCOLAN; se drenan en orden FIFO al inicio del
        // próximo update() (processQueue). Resuelven en el PRÓXIMO TICK, no el
        // próximo frame (P2.6). Disuelven los 4 dialectos de mutación diferida
        // que documentó la auditoría PASADA 2.

        // Encola un push del estado dado.
        void cmdPush(std::unique_ptr<GameState> state)
        {
            m_queue.push_back({ CmdType::Push, std::move(state), nullptr });
        }

        // Encola un pop CON IDENTIDAD: al drenar, solo se ejecuta si
        // `solicitante` sigue siendo el tope. Si un push se metió en medio (o
        // ya se popeó), se descarta + log. Disuelve de un golpe la carrera
        // posicional, la idempotencia, y los interleavings con push() inmediato.
        void cmdPop(GameState* solicitante)
        {
            m_queue.push_back({ CmdType::Pop, nullptr, solicitante });
        }

        // Encola pop hasta el fondo (size==1). Estructural, sin identidad: su
        // destino no depende de quién esté arriba, y es naturalmente idempotente.
        void cmdPopToBottom()
        {
            m_queue.push_back({ CmdType::PopToBottom, nullptr, nullptr });
        }

        // Encola un pop de TODO lo que esté por ENCIMA de `target` (por identidad),
        // dejándolo como tope. Para "volver al juego desde un menú anidado" sin
        // asumir profundidad: a diferencia de cmdPopToBottom, NO toca lo de DEBAJO
        // de target (ej. un título bajo el juego). target ausente → no-op + log
        // (mismo espíritu que el cmdPop con identidad perdida).
        void cmdPopAbove(GameState* target)
        {
            m_queue.push_back({ CmdType::PopAbove, nullptr, target });
        }

        // Encola un replace del tope por el estado dado.
        void cmdReplace(std::unique_ptr<GameState> state)
        {
            m_queue.push_back({ CmdType::Replace, std::move(state), nullptr });
        }

        // Encola un clear total. Al procesarse CORTA el resto del lote: los
        // comandos encolados después de un cmdClear en el mismo tick se
        // descartan (P2.5 generalizado).
        void cmdClear()
        {
            m_queue.push_back({ CmdType::Clear, nullptr, nullptr });
        }

        // ── Wrappers de compatibilidad (D2, sesión Ciclo-de-Vida-B) ──────────
        // requestPop/requestPopToBottom SOBREVIVEN, re-especificados sobre la
        // cola. NO se borraron: ~25 call-sites periféricos (gameplay 2D,
        // transiciones, save-menu, settings) los usan y se auditarán en su
        // propia sesión. Como todo self-popper ES el tope cuando llama
        // requestPop(), capturar el tope actual como solicitante equivale a
        // cmdPop(this) exacto → gana la identidad gratis, cero cambio de
        // comportamiento. "Pop al tope actual" es una operación legítima y
        // permanente. Los flujos de lifecycle usan la API cmd* explícita.
        void requestPopToBottom() { cmdPopToBottom(); }
        void requestPop()
        {
            cmdPop(m_states.empty() ? nullptr : m_states.back().get());
        }

        // Reemplazar el estado de la cima por otro (inmediato). Sin callers hoy;
        // la versión encolada es cmdReplace. P2.5 muere por construcción: ya no
        // hay flags pendientes que puedan sobrevivir a un replace.
        void replace(std::unique_ptr<GameState> state)
        {
            if (!m_states.empty())
            {
                m_states.back()->onExit();
                m_states.pop_back();
            }

            state->onEnter();
            m_states.push_back(std::move(state));
        }

        // Limpiar todos los estados (al cerrar el motor). Vacía también la cola:
        // comandos pendientes no tienen sentido contra una pila destruida.
        void clear()
        {
            m_queue.clear();
            m_renderRequested = false;
            while (!m_states.empty())
            {
                m_states.back()->onExit();
                m_states.pop_back();
            }
        }

        // ── Game loop ──

        // Actualizar estados (llamado desde el Fixed Timestep)
        void update(f64 dt)
        {
            // Drenar la cola de comandos ANTES del update loop. Es el punto
            // seguro: no estamos dentro del update/handleEvent de ningún estado,
            // así que mutar la pila aquí no destruye ningún `this` vivo. Los
            // comandos resuelven en el PRÓXIMO TICK, no el próximo frame (P2.6).
            processQueue();

            if (m_states.empty()) return;

            // Encontrar desde dónde actualizar
            // (algunos estados permiten que los de abajo se actualicen)
            i32 startIndex = static_cast<i32>(m_states.size()) - 1;

            // Buscar hacia abajo mientras allowsUpdateBelow sea true
            while (startIndex > 0 && m_states[startIndex]->allowsUpdateBelow())
            {
                startIndex--;
            }

            // Actualizar desde startIndex hasta la cima
            for (i32 i = startIndex; i < static_cast<i32>(m_states.size()); i++)
            {
                m_states[i]->update(dt);
            }
        }

        // Renderizar estados (llamado cada frame)
        void render(f64 alpha)
        {
            if (m_states.empty()) return;

            // Encontrar desde dónde renderizar
            // (los estados transparentes dejan ver los de abajo)
            i32 startIndex = static_cast<i32>(m_states.size()) - 1;

            // Buscar hacia abajo mientras isTransparent sea true
            while (startIndex > 0 && m_states[startIndex]->isTransparent())
            {
                startIndex--;
            }

            // Renderizar de abajo hacia arriba (el orden importa para overlays)
            //
            // OJO (intro-handoff, 2026-08-01): que un state entre a este
            // recorrido NO alcanza para que se VEA. `setupViewport` borra la
            // pantalla entera, asi que el de arriba puede tapar al de abajo un
            // paso despues de que este walk lo incluyo. Quien se declare
            // transparente tiene que pasar `limpiar=false` (ver Draw2D.h).
            for (i32 i = startIndex; i < static_cast<i32>(m_states.size()); i++)
            {
                m_states[i]->render(alpha);
            }
        }

        // ── Skip-frame (Perfilado §6): ¿el conjunto VISIBLE está quiescente? ──
        // El motor salta render+swap solo si NADA visible cambiaría. "Visible" =
        // exactamente el conjunto que render() recorre: desde el primer estado no
        // transparente (de arriba hacia abajo) hasta la cima. Replica ESE MISMO
        // walk a propósito — un estado tapado (bajo un no-transparente) no se
        // dibuja, así que su quiescencia es irrelevante.
        //   Conservador: pila vacía ⇒ NO quiescente (que el loop siga su curso;
        //   ya se apaga solo vía isEmpty()). Off por identidad mientras ningún
        //   GameState override isQuiescent() (default false ⇒ este método false).
        bool isVisibleSetQuiescent() const
        {
            if (m_states.empty()) return false;

            i32 startIndex = static_cast<i32>(m_states.size()) - 1;
            while (startIndex > 0 && m_states[startIndex]->isTransparent())
                startIndex--;

            for (i32 i = startIndex; i < static_cast<i32>(m_states.size()); i++)
                if (!m_states[i]->isQuiescent())
                    return false;
            return true;
        }

        // Pulso de presentacion independiente de quiescencia. Una mutacion
        // discreta puede ocurrir y asentarse dentro del mismo fixed update
        // (true -> true para isQuiescent); en ese caso el flanco no existe y
        // el heartbeat llegaria tarde. Varias solicitudes antes del render se
        // fusionan, y solo Engine consume el pulso al decidir el frame.
        void requestRenderOnce() noexcept { m_renderRequested = true; }
        bool consumeRenderRequest() noexcept
        {
            const bool requested = m_renderRequested;
            m_renderRequested = false;
            return requested;
        }

        // ── Consultas ──
        bool isEmpty() const { return m_states.empty(); }
        u32 size() const { return static_cast<u32>(m_states.size()); }

        // ── ¿DE DONDE VIENE el state que esta naciendo? ──────────────────────
        //
        // Lo consume la ley de `TransicionPantalla.h`: menu naciendo sobre menu
        // es `navegar` y todo lo demas es `abrir`.
        //
        // ⚠ LOS DOS CAMINOS DE PUSH TIENEN EL CICLO DE VIDA AL REVES, Y ESO NO
        // ESTABA ESCRITO EN NINGUN LADO:
        //
        //     push()          onEnter() -> push_back()   el state NO esta en la pila
        //     cmdPush (cola)  push_back() -> onEnter()   el state YA esta en la pila
        //
        // Y el que se usa es el segundo: `push()` tiene UN SOLO caller vivo (el
        // boot de `main.cpp`), todo lo demas encola. Una pregunta que asuma un
        // orden contesta bien en un camino y **se lee a si misma** en el otro.
        // Medido con el A/B de `correr_sonda_fade.ps1`: la primera version de
        // esto miraba el tope pelado, y `pausa.ecl` naciendo SOBRE EL JUEGO
        // contestaba "vengo de un menu" -- porque el menu que veia era el.
        //
        // Por eso el parametro NO sobra: se salta a quien pregunta. Con eso la
        // respuesta es la misma por los dos caminos, que es la unica forma de
        // que la ley no dependa de cual se uso.
        //
        // El comentario de `VNScriptState.h:435` sigue siendo cierto en su
        // CONCLUSION -al nacer no se puede saber quien te tapa- pero su razon
        // ("`push()` corre este onEnter ANTES de apilar") describe el camino que
        // ese sitio ya no usa. La conclusion se salva sola: por la cola, el que
        // nace es el ULTIMO, asi que `hayAlgoEncima` tampoco encuentra nada.
        //
        // ⚠ SEGUNDA TRAMPA, PARA EL LOTE QUE ENCHUFE `cmdReplace`: ese camino
        // popea al origen ANTES de correr `onEnter`, asi que para el la
        // respuesta es la del de mas abajo. Hoy `replace`/`cmdReplace` tienen
        // CERO callers; el dia que tengan uno, la pregunta va antes del pop.
        struct OrigenDelNacimiento
        {
            bool hay    = false;   // habia algo debajo
            bool esMenu = false;   // y ese algo es una pantalla de menu
        };

        // Pila vacia (o con solo quien pregunta) = `hay=false`, que es el boot:
        // nacer sobre nada es `abrir`.
        OrigenDelNacimiento origenDe(const GameState* quienNace) const
        {
            for (size_t i = m_states.size(); i > 0; --i)
            {
                const GameState* s = m_states[i - 1].get();
                if (s == quienNace) continue;   // soy yo: no soy mi propio origen
                return OrigenDelNacimiento{ true, s->esPantallaDeMenu() };
            }
            return OrigenDelNacimiento{};
        }

        // ¿Hay algun estado POR ENCIMA de `s`? (s ausente o s == cima -> false;
        // nullptr -> false.)
        //
        // OJO CON LO QUE ESTA PREGUNTA *NO* ES (intro-fade-doble, 2026-08-01).
        // Su consumidor quiere saber otra cosa: "¿algo me estuvo TAPANDO la
        // pantalla mientras yo no podia correr?" — y eso es HISTORIA, no un
        // instante. Esta consulta es un PROXY de aquello, deliberado:
        //
        //   · Lo literal seria mirar isTransparent() del de arriba. NO SIRVE:
        //     el splash de la intro se declara transparente JUSTO durante el
        //     handoff (revelaLoDeAbajo), que es el unico momento en que el de
        //     abajo llega a preguntar — o sea que el instrumento da "no me
        //     tapa" exactamente cuando si tapo. Es la forma de §7.396.
        //   · Estar debajo de alguien SI implica que ese alguien se dibujo
        //     despues que vos (el walk de render va de abajo hacia arriba),
        //     y que vos no fuiste el tope. Para "no fui yo el que se vio",
        //     alcanza.
        //
        // Conservadora por el lado seguro: si contesta true de mas, el unico
        // costo es que alguien se saltee un fundido de entrada que igual nadie
        // habria visto.
        bool hayAlgoEncima(const GameState* s) const
        {
            if (!s) return false;
            for (size_t i = 0; i + 1 < m_states.size(); ++i)
                if (m_states[i].get() == s) return true;
            return false;   // es la cima, o no esta en esta pila
        }

        // ── Eventos ──
        // Pasa un evento al estado de la cima
        // Retorna true si el estado consumió el evento
        bool handleEvent(const void* event)
        {
            if (!m_states.empty())
            {
                return m_states.back()->handleEvent(event);
            }
            return false;
        }

        // Propaga el aviso de recarga del manifest a TODO el stack.
        // A diferencia de handleEvent (top-first, se consume), un reload de
        // manifest es un evento GLOBAL: un state pausado debajo de un menú
        // (Pausa/Save) también tiene punteros derivados del manifest que
        // colgarían. Cada state solo levanta su flag pending → avisar a todos
        // es inofensivo (los que no overridean onManifestReloaded son no-op).
        void notifyManifestReloaded()
        {
            for (auto& s : m_states)
                s->onManifestReloaded();
        }

        // Propaga el store recién nacido a TODO el stack (ficha `F8-SIN-IDIOMA`,
        // fork 4c). Global por la misma razón que sus dos hermanos: un state
        // pausado debajo de un menú también cacheó el puntero en su ctor, y es
        // el que va a quedar arriba cuando el de encima popee.
        //
        // El Engine lo llama SOLO en el caso `Encender` del plan: recargar o
        // apagar no cambian el objeto, así que no hay nada que re-apuntar.
        void notifyLangStoreChanged(LangStore* store)
        {
            for (auto& s : m_states)
                s->onLangStoreChanged(store);
        }

        // Propaga el pedido de recarga de texturas a TODO el stack. Igual que
        // notifyManifestReloaded es global (un state pausado abajo también puede
        // tener texturas que el creador repintó). Ungated a propósito: se dispara
        // en F8 SIEMPRE, no solo cuando hay proyecto.eclips válido.
        void notifyReloadTextures()
        {
            for (auto& s : m_states)
                s->onReloadTextures();
        }

    private:
        // ── Cola de comandos: tipos internos ─────────────────────────────────
        enum class CmdType { Push, Pop, PopToBottom, PopAbove, Replace, Clear };
        struct StackCmd
        {
            CmdType type;
            std::unique_ptr<GameState> state;   // Push/Replace; null en el resto
            GameState* solicitante;             // Pop (identidad); null en el resto
        };

        // pop() síncrono — DEPRECADO como API pública (CP2, Ciclo-de-Vida-B).
         // Movido a private para neutralizar el footgun §1.10: un pop síncrono
         // desde handleEvent/update/onEnter destruye el `this` del caller.
         // Sin callers tras CP2 (processQueue hace sus pops inline, sin onResume
         // por-pop). Se conserva como ejecutor síncrono interno por si se necesita.
        void pop()
        {
            if (!m_states.empty())
            {
                m_states.back()->onExit();
                m_states.pop_back();
                if (!m_states.empty())
                    m_states.back()->onResume();
            }
        }

        // Drena la cola en orden FIFO. INVARIANTE de resume-neto:
        //   un estado recibe onResume al final del lote si y solo si quedó como
        //   tope sin acabar de recibir onEnter, y o bien no era el tope al
        //   iniciar el lote, o fue pausado durante el lote.
        // Consecuencia: P2.3 (resume-fantasma de cero frames en pop+popToBottom)
        // es imposible por construcción — el estado intermedio recibe solo su
        // onExit. P2.4 (reentrancia) tampoco aplica a lo encolado: los hooks de
        // la cola nunca se anidan en el callstack de un estado.
        void processQueue()
        {
            if (m_queue.empty()) return;

            // Drenar sobre una COPIA local (swap). Si cualquier hook
            // (onEnter/onPause/onExit/onResume) encola un comando durante el
            // drenado, va a m_queue —ahora vacío— y se procesa el PRÓXIMO tick,
            // en lugar de reubicar el vector que estamos iterando: eso dejaría
            // `cmd` y los iteradores del for colgando → use-after-free →
            // corrupción de heap. Patrón estándar de command-queue reentrante.
            std::vector<StackCmd> batch;
            batch.swap(m_queue);

            GameState* topAntes = m_states.empty() ? nullptr : m_states.back().get();

            // Registros del lote. Punteros crudos a estados VIVOS solamente: al
            // matar un estado (pop/replace) lo borramos de ambos registros para
            // que un push posterior que recicle su dirección no dé un falso
            // positivo (nota de impl §2.3 del contrato).
            std::vector<GameState*> entered;   // recibieron onEnter este lote
            std::vector<GameState*> paused;    // recibieron onPause este lote

            auto forget = [&](GameState* dead)
                {
                    entered.erase(std::remove(entered.begin(), entered.end(), dead), entered.end());
                    paused.erase(std::remove(paused.begin(), paused.end(), dead), paused.end());
                };

            bool cut = false;   // un cmdClear corta el resto del lote
            for (auto& cmd : batch)
            {
                switch (cmd.type)
                {
                case CmdType::Pop:
                    if (!m_states.empty() && m_states.back().get() == cmd.solicitante)
                    {
                        GameState* dying = m_states.back().get();
                        dying->onExit();
                        forget(dying);
                        m_states.pop_back();
                    }
                    else
                    {
                        std::cout << "[StateStack] cmdPop descartado: el "
                            "solicitante ya no es el tope." << std::endl;
                    }
                    break;

                case CmdType::PopToBottom:
                    while (m_states.size() > 1)
                    {
                        GameState* dying = m_states.back().get();
                        dying->onExit();
                        forget(dying);
                        m_states.pop_back();
                    }
                    break;

                case CmdType::PopAbove:
                {
                    // ¿target sigue en el stack? Si no, no-op (identidad perdida).
                    bool found = false;
                    for (auto& s : m_states)
                        if (s.get() == cmd.solicitante) { found = true; break; }
                    if (!found)
                    {
                        std::cout << "[StateStack] cmdPopAbove descartado: el "
                            "objetivo ya no esta en el stack." << std::endl;
                        break;
                    }
                    // Popear hasta que target sea el tope. El resume-neto del final
                    // del lote le da UN onResume (queda como topDespues != topAntes).
                    while (!m_states.empty() &&
                        m_states.back().get() != cmd.solicitante)
                    {
                        GameState* dying = m_states.back().get();
                        dying->onExit();
                        forget(dying);
                        m_states.pop_back();
                    }
                    break;
                }

                case CmdType::Push:
                    if (!m_states.empty())
                    {
                        m_states.back()->onPause();
                        paused.push_back(m_states.back().get());
                    }
                    m_states.push_back(std::move(cmd.state));
                    m_states.back()->onEnter();
                    entered.push_back(m_states.back().get());
                    break;

                case CmdType::Replace:
                    if (!m_states.empty())
                    {
                        GameState* dying = m_states.back().get();
                        dying->onExit();
                        forget(dying);
                        m_states.pop_back();
                    }
                    m_states.push_back(std::move(cmd.state));
                    m_states.back()->onEnter();
                    entered.push_back(m_states.back().get());
                    break;

                case CmdType::Clear:
                    while (!m_states.empty())
                    {
                        GameState* dying = m_states.back().get();
                        dying->onExit();
                        forget(dying);
                        m_states.pop_back();
                    }
                    m_queue.clear();  // teardown: descartar comandos de próximo-tick
                    cut = true;       // P2.5 generalizado: corta el resto del lote
                    break;
                }
                if (cut) break;
            }

            // NO se limpia m_queue aquí: el swap ya lo vació, y lo que un hook
            // haya encolado durante este lote debe sobrevivir al próximo tick.

            // ── Resume-neto: UNA reanudación por lote, al tope final ─────────
            GameState* topDespues = m_states.empty() ? nullptr : m_states.back().get();
            if (topDespues)
            {
                const bool entro = std::find(entered.begin(), entered.end(), topDespues) != entered.end();
                const bool fuePausado = std::find(paused.begin(), paused.end(), topDespues) != paused.end();
                if (!entro && (topDespues != topAntes || fuePausado))
                    topDespues->onResume();
            }
        }

        std::vector<std::unique_ptr<GameState>> m_states;
        std::vector<StackCmd> m_queue;
        bool m_renderRequested = false;
    };

} // namespace eclips
