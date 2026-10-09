// ============================================================================
// Gameplay2DVerbRoute.h - LA CAPA 3 DEL GAMEPLAY 2D: DE VERBOS A EFECTOS
// ============================================================================
//
// `CP2` de `DISENO_ATOMO_TECLADO.md` (§14), alcance firmado **1c**: la tabla
// del dominio `juego` se completa para sus TRES consumidores vivos, y el
// CONSUMO lo paga uno solo -`Gameplay2DState`-. Los otros dos quedan fichados.
//
// Hermano de `VNVerbRoute.h`, y a proposito: la §19 punto 4 del contrato dice
// que `vnAtajoDeVerbos` **es el molde y no un caso especial del VN**. De ahi
// salen las tres piezas que este archivo copia sin pensarlas de nuevo: ley pura
// con efecto + `consume`, contexto en un struct, y guardian de no-op sobre el
// dominio entero.
//
// ── POR QUE ESTE ARCHIVO ES DEL ESTADO Y NO DEL DOMINIO ─────────────────────
//
// Se llama `Gameplay2D...` y no `Juego...` porque el dominio `juego` tiene tres
// consumidores y **los efectos de este archivo son de uno**: `CancelarConstruccion`
// no significa nada en un pasillo, y el `X` de cancelar de la batalla no
// significa nada aca. Cuando `CorridorState` y `BulletHellState` consuman, cada
// uno trae su ley con sus efectos; lo que comparten es la TABLA, que es Capa 1.
//
// ── LO QUE LA §5 DECIA, Y LO QUE EL CODIGO DIJO ─────────────────────────────
//
// La ficha `TABLA-JUEGO-INCOMPLETA` traia UN hueco medido (`atacar` sin tecla
// en topdown). Contando desde el consumidor -los tres estados, no el que el
// contrato estaba mirando- eran mas, y uno cambia de especie: el menu de
// batalla de `BulletHellState` no es vocabulario nuevo de `juego`, es el anillo
// de foco del dominio `vn` (fork 2b). El detalle esta en `InputManager.h`,
// pegado a las filas.
//
// ── LA UNICA DESVIACION DE "BYTE-IDENTICA", Y ES DECLARADA ──────────────────
//
// La doctrina del plan §14 es **off-por-identidad**: hasta `CP3`, jugando no
// cambia nada. `CP2` la rompe en **una** celda, y no se puede evitar:
//
//     hoy   topdown + `F`  ->  NADA (el `if (SDLK_f)` esta gateado por
//                              `m_isSideScroll`, asi que la tecla cae muerta)
//     ahora topdown + `F`  ->  ATACA
//
// Es consecuencia directa de arreglar la tabla. `atacar` en topdown necesita
// una tecla; la unica que tenia era `ESPACIO`, y darle tambien `F` -que es SU
// tecla en el otro modo- es lo que vuelve verdadero al dominio. **Dejar `F`
// muerta en topdown exigiria que la Capa 3 mire el KEYCODE**, que es
// exactamente lo que `CP0`-`CP2` existen para borrar.
//
// Asi que la desviacion se declara, se mide y **tiene assert propio** (§7 del
// harness): el barrido exige identidad en todo el dominio MENOS ahi, y si el
// dia de manana la lista de excepciones crece, el barrido se pone rojo.
//
// ── EL ORDEN DE LOS BLOQUES NO ES EL DE LA CASCADA VIEJA, Y ES LA LEY ───────
//
// La cascada de `handleEvent` compara `SDLK_*` **disjuntos**: `ESC`, `E/ENTER`,
// `ESPACIO`, `SHIFT`, `F`. Ninguno puede matchear dos ramas, asi que su orden
// no significa nada. Sobre VERBOS si significa: `ESPACIO` resuelve a TRES
// (`atacar`, `interactuar`, `saltar`) y el desempate es esta lista.
//
//     `menu`         primero, y consume: es la unica salida garantizada (§8)
//     `saltar`       antes que `atacar`: en side-scroll `ESPACIO` salta
//     `atacar`       antes que `interactuar`: en topdown `ESPACIO` ataca
//                    aunque haya un NPC al lado, que es la conducta de hoy
//     `interactuar`  se queda con `E`/`ENTER`, que no resuelven a nada mas
//     `dash`
//
// **La fila de `interactuar` es la que parece de mas y no lo es:** su `ESPACIO`
// lo trae `CorridorState`, que todavia no consume. Aca es INERTE por este
// orden, y esa inercia esta afirmada (§5 del harness) en vez de quedar de
// palabra - que es la diferencia entre una ficha y un guardian.
//
// ── EL `consume` ES LA MITAD SILENCIOSA ─────────────────────────────────────
//
// De las seis ramas vivas, **solo dos consumen**: los dos `ESC` y el
// `interactuar` que encontro NPC. Todo lo demas cae al hook `key_press` de Lua
// al final de `handleEvent` (Tier 4C.5), que dispara justo porque nadie hizo
// `return true`. Devolver `consume = true` de mas mata ese hook para el
// creador **sin romper nada visible** - el salto sigue saltando.
//
// ── LO QUE ESTA LEY NO DECIDE ───────────────────────────────────────────────
//
// Los gates INTERNOS del efecto: `m_hasDash`, `m_canAttack`, `m_attackTimer`,
// `m_isOnGround`, `m_jumpsRemaining`. Esos siguen en el state, igual que el VN
// decide `GuardarRapido` y el state escribe el archivo. La ley contesta QUE
// efecto y SI consume; el state contesta si puede.
//
// ── LO QUE NO PUEDE VER ─────────────────────────────────────────────────────
//
// El CABLE: que `Gameplay2DState::handleEvent` llame de verdad a
// `resolverTodos(e, "juego")` y a esta ley, y que `update` saque su `inputX`/
// `inputY` de `eje()`. Esa TU arrastra GL (misma pared que `MERGE-DUPLICADO`).
// Lo cubre `test_teclado_juego_feel.ecl` (raiz del repo), corrido y verde el
// 2026-08-06: sus seis pruebas son lo unico que vio este cable.
//
// ── `CP8` (2026-08-10): LO QUE CAE A LUA, Y POR QUE VIVE ACA ────────────────
//
// El diagrama de la §3 del contrato escribe *"Lo NO consumido cae a Lua"*
// ADENTRO de la caja de Capa 3, asi que esto no es un archivo nuevo: es el
// renglon que a esta ley le faltaba escribir. Fork 3a, firmado.
//
// El hook `on("accion")` de la §11 recibe **(verbo, dominio, ejecutado)**, y el
// tercer parametro es lo que costo medir. Un bit calculado solo con esta ley
// significaria *"la ley lo eligio"* y NO *"paso"*, porque los gates internos del
// efecto -`m_hasDash`, `m_canAttack`, `m_isOnGround`...- viven en el state y
// esta cabecera ya lo declaraba doce lineas mas arriba. Un creador que escriba
// `if verbo=="saltar" and ejecutado then world.sound("jump")` oiria el sonido en
// un salto que **no salto**: el falso positivo que el bit fue a matar, mudado de
// lugar (§7.397).
//
// Por eso el state pasa una CAUSA y la ley la consume (fork 4b). Es el mismo
// reparto que el fork 3b de `CP2` -`inputHeldDeTabla` toma el predicado por
// parametro y por eso el eje se prueba sin linkear SDL-: la ley sigue pura, el
// harness la barre entera, y lo unico que queda de cable es *que el state
// clasifique bien*.
//
// **La causa es un enum y no un string, y esa decision tiene caller** (fork 5b):
// el creador pidio que si el efecto no corre, la consola diga POR QUE. Un enum
// se afirma en el harness -causa y voz, byte a byte-; una cadena armada en el
// sitio solo se puede verificar mirandola, y `VOZ-AHOGADA` ya midio cuanto vale
// mirar (4 lineas utiles entre 158). Es la cuarta vez que este repo nombra las
// causas de un silencio: `InputAvisoCausa`, `VNAtajoCausa`, `TrialOutcome`.
//
// **La voz sale por `stderr`** (fork 6a): `Gameplay2DState.h` tiene 122 `cout`
// contra 6 `cerr`, o sea que la separacion "aviso al creador / telemetria" que
// el VN si respeta (95/121) nunca cruzo a este archivo. Este es su primer aviso
// del lado correcto. No cierra `VOZ-AHOGADA`: toma partido adentro.
//
// ⚠ **Y hay una asimetria que conviene saber antes de leer los asserts:** este
// canal NO reemplaza a `key_press`. Para una tecla que no esta ligada a ningun
// verbo -la `1` y la `2` de `test_td_completo.lua`, que eligen torre- la Capa 1
// devuelve lista **vacia**, asi que un hook de verbos dispara **cero veces**. No
// es un hueco de vocabulario: un hook de verbos no tiene nada que decir sobre
// una tecla que no es un verbo. Son dos poblaciones distintas, y por eso
// `key_press` no puede morir en este `CP` (ficha `VERBOS-LIBRES-LUA`).
// ============================================================================

#pragma once

#include <string>
#include <vector>

namespace eclips {

    // Mismo azucar que `vnHayVerbo`, y duplicado a proposito: incluir
    // `VNVerbRoute.h` desde aca arrastraria `VNInputAction.h`,
    // `VNExecutionPolicy.h` y `VNActionSeal.h` -las fases y el sello del VN-
    // adentro de una ley de gameplay que no tiene nada que ver con ellos.
    inline bool g2dHayVerbo(const std::vector<std::string>& verbos,
                            const char* verbo)
    {
        for (const std::string& v : verbos)
            if (v == verbo) return true;
        return false;
    }

    // Lo que el state EJECUTA. `Ninguno` no es un error: es "esto no era mio",
    // y es lo que deja vivo el hook `key_press`.
    enum class G2DEfecto : unsigned char {
        Ninguno,
        CancelarConstruccion,   // ESC con modo construccion activo (Tier 4C.4)
        AbrirPausa,             // ESC
        Interactuar,            // NPC al lado -> DialogOverlayState
        Saltar,                 // side-scroll: salto / doble / wall-jump
        Atacar,                 // performAttack()
        Dash,                   // side-scroll
    };

    // Los cuatro bools van en un struct A PROPOSITO - es `B-5` de la §19, y la
    // razon no cambia por estar en otro dominio: sueltos en la firma, dos
    // intercambiados **compilan en silencio** y el harness los probaria en el
    // orden equivocado con la misma cara de verde. Aca el riesgo es peor que en
    // el VN, porque `sideScroll` y `cercaDeNPC` son los dos el mismo tipo y los
    // dos gatean ramas que se ven parecidas desde afuera.
    struct G2DContexto {
        bool sideScroll       = false;  // `m_isSideScroll`
        bool modoConstruccion = false;  // `!m_buildModeTowerName.empty()`
        bool cercaDeNPC       = false;  // `m_nearNPC` en rango valido
    };

    struct G2DAccion {
        G2DEfecto efecto  = G2DEfecto::Ninguno;
        bool      consume = false;
    };

    // `verbos` = lo que `InputManager::resolverTodos(e, "juego")` devolvio.
    //
    // Toma la LISTA y no `resolver()`, por la misma razon que el VN: `ESPACIO`
    // activa tres verbos y el `std::map` los da alfabeticamente, o sea que el
    // primero es `atacar` **tambien en side-scroll**. Usar el singular aca
    // mataria el salto sin un warning.
    inline G2DAccion g2dAccionDeVerbos(const std::vector<std::string>& verbos,
                                       const G2DContexto& ctx)
    {
        // Ley A - el `ESC`, y son DOS ramas en cascada, igual que el sitio:
        //   `if (ESC && !m_buildModeTowerName.empty())` primero, `if (ESC)`
        //   despues. El orden es el contrato del fork: build-cancel > pausa.
        //   Las dos consumen.
        if (g2dHayVerbo(verbos, "menu"))
        {
            if (ctx.modoConstruccion)
                return { G2DEfecto::CancelarConstruccion, true };
            return     { G2DEfecto::AbrirPausa,           true };
        }

        // Ley B - `saltar`, y **va antes que `atacar`**: en side-scroll el
        //   `ESPACIO` resuelve a los dos y el sitio salta. Fuera de side-scroll
        //   `saltar` no existe (el `if (m_isSideScroll)` del sitio), y ese
        //   `else` es justo el que ataca -o sea que la fila de abajo lo toma-.
        //   **No consume:** el sitio no hace `return true` ahi.
        if (g2dHayVerbo(verbos, "saltar") && ctx.sideScroll)
            return { G2DEfecto::Saltar, false };

        // Ley C - `atacar`, en los DOS modos. Es la mitad que arregla la ficha
        //   `TABLA-JUEGO-INCOMPLETA`, y la unica celda donde `CP2` se sale de
        //   "byte-identica": en topdown la `F` pasa de muerta a atacar. El
        //   porque esta arriba, en la cabecera; el assert, en el harness §7.
        if (g2dHayVerbo(verbos, "atacar"))
            return { G2DEfecto::Atacar, false };

        // Ley D - `interactuar`. **Consume solo si habia NPC**, que es la forma
        //   exacta del sitio: el `return true` vive DENTRO del `if (m_nearNPC
        //   >= 0 && ...)`, asi que una `E` en el aire cae al hook de Lua.
        //   Confundir eso por un `consume` incondicional le apaga `key_press`
        //   al creador para la `E` y la `ENTER` sin que se note jugando.
        if (g2dHayVerbo(verbos, "interactuar"))
        {
            if (ctx.cercaDeNPC) return { G2DEfecto::Interactuar, true };
            return { G2DEfecto::Ninguno, false };
        }

        // Ley E - `dash`, solo side-scroll (`if (m_isSideScroll && m_hasDash
        //   && ...)`). `m_hasDash` y el cooldown se quedan en el state: son
        //   gates del efecto, no del ruteo.
        if (g2dHayVerbo(verbos, "dash") && ctx.sideScroll)
            return { G2DEfecto::Dash, false };

        // Ley F - no era mio, y **NO CONSUME**. Es lo que mantiene vivo el hook
        //   `key_press` de Lua para todo lo que el gameplay no ata.
        return {};
    }

    // ========================================================================
    // `CP8` - LO QUE CAE A LUA
    // ========================================================================

    // Por que el efecto ruteado no corrio.
    //
    // `Ocurrio` y `NoRuteado` NO son fallas: son los dos "no hay nada que
    // reportar", y estan separados porque significan cosas distintas para quien
    // lee la consola -uno es "salio bien", el otro es "esto ni siquiera era
    // tuyo"-.
    //
    // Las cuatro causas reales salieron de contar los gates de los TRES efectos
    // que no consumen, o sea la poblacion entera que llega a Lua:
    //
    //     saltar   `m_isOnGround || m_coyoteTimer > 0 || m_jumpsRemaining > 0`
    //     atacar   `m_canAttack && m_attackTimer <= 0`
    //     dash     `m_hasDash && !m_isDashing && m_dashCooldownTimer <= 0`
    //
    // `interactuar` y los dos `menu` no aparecen porque CONSUMEN, asi que nunca
    // llegan a este canal. Que la lista de causas y la lista de efectos sin
    // consumo coincidan no es casualidad: es la misma frontera vista dos veces.
    // ── LA ESPECIE DE CADA CAUSA (fork 11b) ────────────────────────────────
    //
    // No son todas iguales, y la diferencia decide si REPETIR informa:
    //
    //   PERMANENTE  describe la CONFIGURACION del mapa. Decirlo una vez es un
    //               diagnostico; decirlo en cada pulsacion es spam sobre algo
    //               que el creador no puede cambiar sin editar el `.eclmap`.
    //   TRANSITORIA describe el MOMENTO. Ahi cada ocurrencia ES el dato, y
    //               callar seria el defecto: si el dash se frena tres veces por
    //               cooldown y solo avisa la primera, el silencio de la segunda
    //               se lee como "esta vez si salio".
    //
    // **Lo midio un caller real, no un razonamiento.** Corriendo el demo
    // (`test_td_completo.eclmap`, que no declara `puede_atacar`), el `SPACE`
    // -la tecla que mas se aprieta- imprimia la misma linea SIETE veces en doce
    // teclas. En el mapa vacio de la sonda eso no aparece.
    enum class G2DGateCausa : unsigned char {
        Ocurrio,           // el efecto corrio: no hay gate que reportar
        NoRuteado,         // la ley no eligio verbo (o eligio otro distinto)
        SinSueloNiSaltos,  // TRANSITORIA - saltar: en el aire y sin saltos
        SinCapacidad,      // PERMANENTE  - atacar: `!m_canAttack` · dash: `!m_hasDash`
        EnCooldown,        // TRANSITORIA - atacar/dash: el cooldown corriendo
        YaEnCurso,         // TRANSITORIA - dash: `m_isDashing`
    };

    // Si la causa describe la configuracion del mapa y no el momento. El
    // consumidor la usa para decidir si el aviso se repite; vive aca -y no en
    // el state- porque es una propiedad de la CAUSA, no del sitio, y asi el
    // harness la barre entera.
    inline bool g2dCausaEsPermanente(G2DGateCausa causa)
    {
        return causa == G2DGateCausa::SinCapacidad;
    }

    struct G2DVerboCaido {
        std::string verbo;
        bool        ejecutado = false;
    };

    // El verbo que la ley ruteo, o "" si no ruteo ninguno. Es la inversa de
    // `g2dAccionDeVerbos`, y vive en produccion -no en el harness- porque el bit
    // `ejecutado` la necesita para saber CUAL de los verbos caidos es el que el
    // state efectivamente atendio.
    //
    // ── EL IDIOMA DEL PAYLOAD ES EL CANONICO, Y ESO SE FIRMO MIDIENDO ───────
    //
    // Devuelve **el canonico ES** (`InputManager.h`: *"canonico ES - la clave de
    // la tabla"*), aunque `CP3` deje al creador escribir el bloque entero en
    // ingles (`keys game { jump: ... }`). O sea que quien escribio `jump`
    // recibe `"saltar"` en Lua. Firmado (fork 7a), y la razon es ESTABILIDAD y
    // no coherencia: esto es un identificador que un programa **compara**, no un
    // texto que una persona lee. Un canonico unico hace que `verbo == "saltar"`
    // sea un contrato; dos grafias lo vuelven una coincidencia.
    //
    // **Ojo con la simetria tentadora que NO aplica:** la ficha `G2`/`Y-VN-01`
    // firmo *"un aviso nombra la grafia que el creador escribio"*. Eso es para
    // AVISOS, donde el lector es humano y busca su propia linea. Aca el lector
    // es un `==`.
    //
    // **Y esto es la SEGUNDA cara de `LUA-NOMBRA-DISTINTO`** (gatillo `CP8`, o
    // sea ahora). Esa ficha se escribio sobre las TECLAS -`key_press` emite
    // `SDL_GetKeyName`, asi que Lua dice `Space` donde `${_tecla_*}` dira
    // `ESPACIO`-; el verbo es el mismo defecto en el otro eje. Las dos son el
    // motor entregandole a Lua un vocabulario que no es el del creador, y hasta
    // este `CP` nadie las habia visto como una sola. Lua es la primera
    // superficie donde el motor le entrega un verbo a un PROGRAMA: `${_tecla_*}`
    // tambien emite, pero a un humano y siguiendo el idioma activo (§12).
    inline const char* g2dVerboDelEfecto(G2DEfecto efecto)
    {
        switch (efecto) {
        case G2DEfecto::CancelarConstruccion:
        case G2DEfecto::AbrirPausa:           return "menu";
        case G2DEfecto::Interactuar:          return "interactuar";
        case G2DEfecto::Saltar:               return "saltar";
        case G2DEfecto::Atacar:               return "atacar";
        case G2DEfecto::Dash:                 return "dash";
        case G2DEfecto::Ninguno:              break;
        }
        return "";
    }

    // Los verbos que el evento dejo caer a Lua, en el orden en que la Capa 1 los
    // devolvio.
    //
    // **Un evento consumido no deja caer NADA**, y eso es H8 literal: es la
    // misma condicion que hoy decide si el `fireHook("key_press", ...)` del
    // sitio se ejecuta (los `return true` de las ramas que consumen). Invertir
    // esta linea le daria al creador el `ESC` que abre la pausa.
    //
    // **Un evento sin verbos tampoco deja caer nada**, y esa es la asimetria de
    // la cabecera: no hay nada que traducir, la lista sale vacia, y `key_press`
    // sigue siendo el unico canal de esa tecla.
    inline std::vector<G2DVerboCaido> g2dVerbosParaLua(
            const std::vector<std::string>& verbos,
            const G2DAccion&                accion,
            G2DGateCausa                    causa)
    {
        std::vector<G2DVerboCaido> caidos;
        if (accion.consume) return caidos;

        const std::string ruteado = g2dVerboDelEfecto(accion.efecto);
        for (const std::string& v : verbos)
        {
            G2DVerboCaido c;
            c.verbo     = v;
            c.ejecutado = (!ruteado.empty() && v == ruteado &&
                           causa == G2DGateCausa::Ocurrio);
            caidos.push_back(c);
        }
        return caidos;
    }

    // La voz del gate, en UN solo lugar y DEVUELTA en vez de impresa - misma
    // forma que `inputVozDelAviso`, y por la misma razon: una cadena devuelta se
    // afirma byte a byte en el harness; un `std::cerr <<` desparramado en el
    // sitio solo se puede verificar mirandolo, y `VOZ-AHOGADA` ya midio cuanto
    // vale mirar (4 lineas utiles entre 158).
    //
    // Cadena vacia = no hay nada que decir, y las dos causas que la devuelven
    // vacia son exactamente las dos que no son fallas.
    inline std::string g2dVozDelGate(const std::string& verbo,
                                     G2DGateCausa      causa)
    {
        const std::string q = "'" + verbo + "'";
        switch (causa) {
        case G2DGateCausa::SinSueloNiSaltos:
            return "[Gameplay2D] " + q + " no corrio: en el aire, sin coyote y "
                   "sin saltos restantes.";
        case G2DGateCausa::SinCapacidad:
            // La unica PERMANENTE, y su texto lo dice: nombra el archivo donde
            // se arregla, y declara que no va a repetirse - para que su
            // ausencia despues no se lea como "ahora si funciona".
            return "[Gameplay2D] " + q + " no corrio: este personaje no lo tiene "
                   "habilitado en el .eclmap. (Se avisa una sola vez.)";
        case G2DGateCausa::EnCooldown:
            return "[Gameplay2D] " + q + " no corrio: todavia esta en cooldown.";
        case G2DGateCausa::YaEnCurso:
            return "[Gameplay2D] " + q + " no corrio: ya hay uno en curso.";
        case G2DGateCausa::Ocurrio:
        case G2DGateCausa::NoRuteado:
            break;
        }
        return std::string();
    }

} // namespace eclips
