// ============================================================================
// AppearPolicy.h - LA APARICION DE LA CAJA: su POLITICA y su maquina de estados
// ============================================================================
//
// Nucleo del lote 2 de `AUDITORIA_VOCABULARIO_ECLIPSCRIPT_VN.md` (`D-08` +
// `D-10` + `R-01`/`R-02`), y de la ficha `CAJA-ENTRADA`.
//
// ── QUE RESUELVE, Y COMO SE MIDIO ───────────────────────────────────────────
//
// `appearAlpha()` leia la duracion del tema CADA FRAME, asi que la rampa era
// `t/dur` con el denominador cambiando por abajo. El reloj no se reiniciaba
// -eso ya estaba bien- pero un `usar_caja` mientras la caja entraba movia el
// alpha sin que el reloj se moviera.
//
// El gate `FO-2` lo midio en pantalla (beats 7-12 de `sonda_caja_entrada.ecl`,
// alpha del borde sobre los pixeles de la captura):
//
//   beat 8    1.00 -> 0.06   cambiar a una caja mas LENTA deshace la entrada
//   beat 10   0.05 -> 1.00   cambiar a una caja de GOLPE la termina de un salto
//   beat 12   0.05 -> 0.06   MISMA duracion, otro borde: la rampa NO se mueve
//
// El beat 12 es el que hace que los otros dos signifiquen algo: el borde
// cambio -o sea que `usar_caja` aterrizo- y la opacidad siguio su curva. La
// causa es el denominador vivo, no `usar_caja` rearmando la entrada.
//
// Y el beat 8 corrigio el diagnostico por el lado que lo agranda: la caja NO
// estaba a media entrada, estaba ENTERA. `m_appearTimer` no se apaga nunca,
// asi que la ventana de dano no es "mientras la entrada corre" sino
// `timer < duracion_nueva` — segundos DESPUES de que la caja termino de entrar.
//
// ── POR QUE UN HEADER APARTE, Y PURO ────────────────────────────────────────
//
// Mismo motivo que `GroupAlpha.h` y `TextBudget.h`, y ademas una peticion
// escrita de la mesa (`RO-5`, aceptada en `D-14`): *"cuando la politica de
// aparicion se capture, que aterrice donde un harness la alcance"*. Adentro de
// `VNDialogBox.cpp` esta ley NO SE PUEDE PROBAR — esa TU arrastra GL, que es la
// pared de `MERGE-DUPLICADO`. Aca es GL-free (solo `TweenSystem.h`, que ya es
// SDL-free y lo prueban `harness_anim` y `harness_ctc`) y la custodia
// `harness_aparicion.cpp`.
//
// Por eso el header no lleva SOLO la rampa: lleva la maquina de estados
// entera. La mitad cara de este lote no es dividir por la duracion correcta —
// es *"una aparicion se resuelve una sola vez"*, y eso es estado, no aritmetica.
//
// ── POR QUE VIVE EN engine/render Y NO EN engine/core ───────────────────────
//
// Porque consume `TweenSystem.h`, que vive aca. Meterlo en `core` invertiria la
// capa (hoy render -> core, nunca al reves) o -peor- obligaria a copiar la
// curva, que es exactamente el replicante que `CLAUDE.md` regla 5 prohibe.
//
// ── LAS LEYES QUE VIVEN AQUI ────────────────────────────────────────────────
//
//   ley 1   IDENTIDAD. `fadeInDuration <= 0` devuelve 1.0 EXACTO, y un estado
//           sin armar tambien. Es el guardian de no-op: un tema que no nombra
//           la clave dibuja bit-identico a antes de que esto existiera
//   ley 2   LA CURVA NO SE INVENTA ACA. Es un `AnimTrack` del canal Alpha en
//           modo `una_vez` con la curva `suave`, evaluado con el MISMO
//           `evalTrack` que usa el parpadeo del indicador. Un solo algebra de
//           animacion en el motor (§1.40), y la entrada hereda gratis la tabla
//           de curvas que `harness_anim` ya custodia con 81 asserts
//   ley 3   LA POLITICA SE CAPTURA AL ARMAR. Lo que decide como entra la caja
//           se fija UNA VEZ, cuando la aparicion empieza. Cambiar de caja, de
//           hablante o de tema despues cambia el LOOK y no la entrada en curso
//   ley 4   UNA ENTRADA TERMINADA NO SE DESHACE. Corolario de la 3 que vale la
//           pena escribir aparte, porque es el sintoma que se midio: pasada su
//           duracion el alpha es 1.0 y se queda, aunque despues aparezca una
//           duracion mas grande
//   ley 5   UNA APARICION, UN DUENO (`D-10`, §5.2 puntos 1-2). Mientras haya
//           una aparicion armada que NADIE RENDERIZO todavia, un segundo
//           intento de armar no la rearma ni le cambia la politica: se
//           incorpora. Es lo que hace que `mostrar_caja` pelado no mienta —
//           sin esto, la linea de dialogo que viene pegada abajo corre en el
//           mismo `update()`, vuelve a armar, y le devuelve el fundido del tema
//   ley 6   EL RELOJ ES EL DE RENDER, NO EL DE UPDATE (§7.418). La rotacion de
//           "me dibujaron" vive en `frameBegin()`, que llama el RENDER. Vivio
//           en el update y la sonda lo desmintio: con el skip-frame la pantalla
//           queda congelada mostrando la caja mientras el update sigue
//           corriendo, asi que un flag rotado ahi se apaga solo y la caja funde
//           entre dos lineas seguidas
//   ley 7   UNA APARICION QUE NADIE VA A VER SE CANCELA. `ocultar_caja` tira lo
//           pendiente: `renderDialogBox` sale temprano con la caja oculta, asi
//           que esa aparicion armada no se puede consumir nunca — y dejarla
//           viva la haria reclamar, muchos frames despues, una aparicion que no
//           es la suya. Es `R-02` ("un no-op visible no deja estado pendiente")
//           aplicada al otro lado
//   ley 8   EL MAS ESPECIFICO GANA, Y "ESPECIFICO" ES *ESCRITO*, NO *DISTINTO
//           DE CERO* (`R-03`, lote 3). Una linea que escribio su `{pausa=N}` de
//           apertura manda sobre el retraso del tema — siempre, sea aparicion o
//           no, porque el creador lo puso en ESA linea a proposito. El del tema
//           solo se arma en la APARICION. Lo que el lote 3 cambia es el
//           predicado: antes era `linePause > 0`, o sea que `{pausa=0}` no podia
//           ganar; ahora es la PRESENCIA, asi que cero significa *"esta linea
//           arranca ya"* y cancela el retraso heredado
//
// ── LO QUE ESTE HEADER NO HACE ──────────────────────────────────────────────
//
// No dibuja ni multiplica alphas: entrega un factor ya resuelto y `GroupAlpha.h`
// lo reparte. Esa frontera ya estaba declarada — la §8 de `harness_grupo` dice
// *"este archivo NO sabe de tiempo ni de curvas: el reloj es del caller"*. Este
// es el caller.
//
// Y no sabe de temas: recibe una `AppearPolicy` ya armada. Quien la construye
// desde `DialogBoxStyle` es `VNDialogBox`, para que este archivo no arrastre
// `Theme.h` ni tenga que enterarse de la cadena `usar_caja` > personaje >
// default.
// ============================================================================

#pragma once

#include "engine/core/Types.h"
#include "engine/render/TweenSystem.h"

namespace eclips
{
    // ── LA POLITICA ─────────────────────────────────────────────────────────
    //
    // Lo que el motor decide UNA VEZ, cuando la aparicion empieza. Los nombres
    // son los mismos campos de `DialogBoxStyle` a proposito: la captura tiene
    // que leerse como lo que es -una copia- y no como una traduccion.
    //
    // Las claves de creador que la alimentan son `caja_aparicion_duracion` /
    // `box_entry_duration` y `caja_aparicion_texto_retraso` /
    // `box_entry_text_delay` (`D-09`, `D-17`).
    struct AppearPolicy
    {
        f32 fadeInDuration = 0.0f;   // segundos de la rampa de opacidad del grupo
        f32 textDelay      = 0.0f;   // segundos antes de la primera letra

        // `mostrar_caja` pelado: NINGUN efecto, ni de caja ni de texto.
        //
        // `D-08` firmo la caja instantanea; que el texto tambien sea inmediato
        // es `FO-7`, y la razon es de alcanzabilidad, no de simetria: bajo la
        // otra opcion el retraso heredado no se podria cancelar por linea,
        // porque el escape seria `{pausa=0}` y `{pausa=0}` muere en el parser
        // sin voz (`HO-5`, `TextRenderer.cpp:608`) hasta que aterrice su lote.
        // La combinacion "caja de golpe + texto retrasado" sigue viva por el
        // camino automatico, que es donde el TEMA la pide.
        static AppearPolicy instant() { return AppearPolicy{}; }
    };

    // ── ley 1 + ley 2: la rampa ─────────────────────────────────────────────
    //
    // Pura y aparte del estado para que el harness pueda probarla sin construir
    // nada, igual que `composeGroupAlpha`.
    //
    // `una_vez` clampea Y PERSISTE: pasada la duracion devuelve 1.0 para
    // siempre, asi que no hace falta apagar el reloj al terminar (ley 4).
    inline f32 appearAlphaAt(const AppearPolicy& p, f32 t)
    {
        if (p.fadeInDuration <= 0.0f) return 1.0f;   // el tema no pidio entrada

        eclips::AnimTrack entrada;
        entrada.canal = eclips::Channel::Alpha;
        entrada.modo  = eclips::Lifecycle::UnaVez;
        entrada.curva = eclips::EasingId::Suave;
        entrada.de    = 0.0f;
        entrada.a     = 1.0f;
        entrada.dur   = p.fadeInDuration;
        return eclips::evalTrack(entrada, t);
    }

    // ── ley 8: quien pone el retraso del texto de ESTA linea ────────────────
    //
    // Toma primitivos y no `RichText` a proposito: asi este header no arrastra
    // `TextRenderer.h`, y la ley queda probable desde los DOS harnesses.
    //
    // `lineHasPause` es la PRESENCIA del `{pausa=N}` de apertura, no su valor.
    // Esa es toda la diferencia del lote 3, y es la unica razon por la que este
    // parametro es un bool separado en vez de mirar `linePause > 0`.
    //
    // El gate de skip NO vive aca: es del caller, porque depende de estado de
    // input que este header no ve (`VNDialogBox.cpp`, mismo gate que la pausa
    // mid-linea al armarse).
    inline f32 resolveTextDelay(bool lineHasPause, f32 linePause,
                                bool inAppearance, f32 policyTextDelay)
    {
        if (lineHasPause)  return linePause;        // incluso si vale 0
        if (inAppearance)  return policyTextDelay;  // el heredado del tema
        return 0.0f;                                // ni linea ni aparicion
    }

    // ── EL ESTADO VIVO ──────────────────────────────────────────────────────
    //
    // Una instancia por caja de dialogo. `VNDialogBox` la tiene como miembro y
    // delega: no queda una segunda copia de ninguna de las siete leyes.
    class AppearState
    {
    public:
        // ── ley 6: abre el frame DE RENDER ──────────────────────────────────
        //
        // Lo llama el dueno al principio de SU render, una vez por frame
        // DIBUJADO. Es lo que hace que la pregunta sea *"¿estaba yo en el
        // ultimo frame que se dibujo?"* y no *"¿me dibujaron el update
        // pasado?"*. Ver ley 6 arriba: la diferencia la midio la sonda.
        void frameBegin()
        {
            m_drawnLastFrame = m_drawnThisFrame;
            m_drawnThisFrame = false;
        }

        // Lo llama el render DESPUES de sus early-outs, para que "oculta" y
        // "sin fuente" cuenten como no-vista. Consume lo pendiente: desde aca
        // la aparicion ya es de alguien y el jugador la esta viendo.
        void rendered()
        {
            m_drawnThisFrame = true;
            m_pending        = false;
        }

        // ── ley 5: armar, si es que corresponde ─────────────────────────────
        //
        // Devuelve si la entrada se armo, o sea si esto es una APARICION. El
        // bool no es adorno: el retraso del texto cuelga de la MISMA pregunta,
        // y preguntarla dos veces seria tener dos definiciones de "aparicion"
        // que se pueden separar en silencio.
        bool arm(const AppearPolicy& p)
        {
            if (m_drawnLastFrame) return false;  // el jugador me estaba viendo
            if (m_pending)        return false;  // ya hay una, y es de otro

            m_policy  = p;
            m_timer   = 0.0f;
            m_pending = true;
            return true;
        }

        // ── ley 7 ───────────────────────────────────────────────────────────
        void cancelPending() { m_pending = false; }

        // El reloj de la entrada. Lo mueve el update; que corra mientras el
        // motor quiesce es correcto y esta medido: la quiescencia saltea el
        // RENDER, no el UPDATE (`Engine.cpp:404-431`).
        void tick(f32 dt)
        {
            if (m_timer >= 0.0f) m_timer += dt;
        }

        // ── ley 1: el factor de ESTE frame ──────────────────────────────────
        f32 alpha() const
        {
            if (m_timer < 0.0f) return 1.0f;   // nunca entro: esta puesta
            return appearAlphaAt(m_policy, m_timer);
        }

        // Skip-frame: mientras el alpha no llego a 1, cada frame cambia pixeles
        // y el motor NO puede quiescer. El caller espeja ademas la
        // alcanzabilidad del draw (sin fuente / oculta).
        bool isAppearing() const { return alpha() < 1.0f; }

        // Lo que la aparicion en curso decidio al armarse. `VNDialogBox` lee de
        // aca el retraso del texto en vez de volver al tema: si volviera, la
        // ley 3 estaria escrita una vez y desobedecida en el otro campo.
        const AppearPolicy& policy() const { return m_policy; }

        // ── ¿ESTA LINEA PERTENECE A UNA APARICION? ──────────────────────────
        //
        // El predicado del que cuelga el retraso del texto. Es true si la linea
        // ARMO la aparicion o si SE INCORPORO a una que nadie renderizo todavia
        // (§5.2 punto 2) — las dos comparten el mismo retraso, el de `policy()`.
        //
        // No es un `arm()` que devuelve true en los dos casos a proposito: el
        // caller necesita distinguir "arme yo" de "me incorpore" para NO pisar
        // la politica ajena, y necesita el OR para el retraso. Dos preguntas,
        // dos respuestas.
        //
        // Es el mismo bit que `pending()` y eso no es casualidad: una aparicion
        // sin consumir es, por definicion, una que el jugador todavia no vio.
        // Se expone con nombre propio porque `m_pending` nombra el mecanismo y
        // esto nombra la PREGUNTA, que es lo que el call-site quiere leer.
        bool inAppearance() const { return m_pending; }

        // Consultas para el harness y para los guardianes del caller.
        f32  timer()          const { return m_timer; }
        bool pending()        const { return m_pending; }
        bool drawnLastFrame() const { return m_drawnLastFrame; }

    private:
        AppearPolicy m_policy{};
        f32  m_timer          = -1.0f;   // <0 = no hay entrada en curso
        bool m_pending        = false;   // armada y todavia sin renderizar
        bool m_drawnThisFrame = false;
        bool m_drawnLastFrame = false;
    };

} // namespace eclips
