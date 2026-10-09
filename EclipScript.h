// ============================================================================
// EclipScript.h — Parser de archivos .ecl para Eclips Engine
// ============================================================================
//
// Convierte un archivo de texto .ecl en estructuras de datos que el motor
// puede ejecutar. El creador nunca toca C++: solo escribe escenas en texto
// plano con una sintaxis diseñada para ser legible como un guión de cine.
//
// ── BILINGÜE ────────────────────────────────────────────────────────────────
//
//   Todos los comandos se aceptan tanto en español como en inglés.
//   El creador puede mezclar idiomas libremente dentro del mismo script.
//
//   Español                  │  English
//   ─────────────────────────┼─────────────────────────────
//   escena inicio            │  scene start
//   personaje Luna "..."   │  character Luna "..."
//   fondo "..."              │  background "..."
//   mostrar Luna centro    │  show Luna center
//   ocultar Luna           │  hide Luna
//   musica "..."             │  music "..."
//   sonido "..."             │  sound "..."
//   parar_musica             │  stop_music
//   transicion fade          │  transition fade
//   ir_a final               │  goto ending
//   fin                      │  end
//   elegir:                  │  choose:
//   set var = valor          │  set var = value
//   si var == val            │  if var == val
//   sino                     │  else
//   fin_si                   │  end_if
//   qte [tecla: E]           │  qte [key: E]
//     exito -> escena        │    success -> scene
//     fallo -> escena        │    fail -> scene
//   capa "n" "r" z=0 x=0..   │  layer "n" "r" z=0 x=0..
//   capa_z "n" 250           │  layer_z "n" 250
//   capa_visible "n" si      │  layer_visible "n" yes
//   hotspot "n" x=0 y=0..    │  hotspot "n" x=0 y=0..
//   esperar_click:           │  wait_click:
//     "nombre" -> escena     │    "name" -> scene
//   evento_random "n" ...    │  random_event "n" ...
//   iniciar_eventos          │  start_events
//   detener_eventos          │  stop_events
//   parallax si/no           │  parallax yes/no
//   personaje_compuesto "a" "c/"│  composite_character "a" "f/"
//   cargar_script "r" [escena]  │  load_script "r" [scene]
//
// ── POSICIONES ──────────────────────────────────────────────────────────────
//
//   izquierda | centro_izq | centro | centro_der | derecha
//   left      | center_left| center | center_right| right
//
// ── SINTAXIS COMPLETA (español) ─────────────────────────────────────────────
//
//   # Comentario (se ignora)
//
//   personaje Luna "assets/personajes/Luna.png"
//   personaje Sol   "assets/personajes/Sol.png"
//
//   escena inicio
//
//   fondo "assets/fondos/aula.png"
//   musica "assets/audio/tema.ogg"
//   mostrar Luna centro
//
//   Luna: "Hola, bienvenido al club."
//   Luna: "Aquí escribimos... y soñamos."
//
//   "Una voz llegó desde el pasillo."        ← Narrador (sin personaje)
//
//   mostrar Sol izquierda
//   Sol: "¿Quieres unirte?"
//
//   elegir:
//     "Sí, me uno"     -> ruta_si
//     "Necesito pensar" -> ruta_duda
//
//   escena ruta_si
//   Luna: "¡Sabía que dirías que sí!"
//   sonido "assets/audio/campana.wav"
//   ir_a final
//
//   escena ruta_duda
//   Sol: "...lo entiendo."
//   ir_a final
//
//   escena final
//   ocultar Luna
//   ocultar Sol
//   parar_musica
//   "Y así comenzó todo."
//   fin
//
// ── VARIABLES Y CONDICIONALES ──────────────────────────────────────────────
//
//   set ruta = curiosidad          ← asignar valor
//   set puntos + 1                 ← incrementar (valor numérico)
//   set puntos - 1                 ← decrementar
//   set nombre = "Juan Carlos"     ← valor con espacios entre comillas
//
//   si ruta == curiosidad          ← comparar (==, !=, >, <, >=, <=)
//     Luna: "¡Sabía que eras curioso!"
//   sino
//     Luna: "Entiendo..."
//   fin_si
//
// ── CHOICES CON TIMER ──────────────────────────────────────────────────────
//
//   elegir [timer: 5s, default: "No reaccionar"]:
//     "¡Esquivar!"     -> esquiva_exitosa
//     "¡Bloquear!"     -> bloqueo
//     "No reaccionar"  -> golpe
//
//   (Si el timer expira, se elige la opción default automáticamente)
//
// ── QUICK-TIME EVENTS ──────────────────────────────────────────────────────
//
//   qte [tecla: E, tiempo: 2s]:
//     exito -> escapa
//     fallo -> atrapado
//
//   (Muestra "Presiona E!" con barra de tiempo. Éxito o fallo según reacción)
//
// ── POSICIONES DE PERSONAJES ─────────────────────────────────────────────────
//
//   izquierda | centro_izq | centro | centro_der | derecha
//   left      | center_left| center | center_right| right
//
//   (Sin posición = centro/center por defecto)
//
// ── EXPRESIONES ──────────────────────────────────────────────────────────────
//
//   Luna feliz: "¡Esto es genial!"
//   (Carga "Luna_feliz.png" si existe, si no usa "Luna.png" como fallback)
//
// ============================================================================

#pragma once

#include "engine/core/ProjectManifest.h"
#include "engine/core/EclipsParse.h"   // parseBool centralizado (§1.45)
#include "engine/core/ScenePatch.h"    // RT-1: vocabulario de la ley del UPDATE
#include "engine/core/TrialClock.h"    // SYN-4b: el ensayo (rangos de `cada=`/`probabilidad=`)
#include "engine/core/VNActionSeal.h" // D-50: el vocabulario del sello de acciones
#include "engine/core/VNMutateTarget.h" // CP0 mutar: el tokenizador del triple `var op valor`
#include "engine/render/TweenSystem.h" // C-CP1: la curva se resuelve en parse-time
#include "engine/render/EffectSystem.h" // E-CP0: los params del efecto, también
                                       // (el aviso trae número de línea). Hoja sin deps.
#include "engine/render/FontMeasure.h"
#include "engine/core/FontDeclaration.h" // CP-R3: la ley pura de `fuente`, compartida con el manifest
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <optional>
#include <cstdint>
#include "engine/core/Types.h"

namespace eclips
{
    // ── Tipos de comando ──────────────────────────────────────────────────────

    enum class EclipCmdType
    {
        Personaje,    // Definir alias → ruta de imagen
        Escena,       // Declarar inicio de escena (no se ejecuta, es etiqueta)
        AmbitoBegin,  // ambito/scope nombre { — generación léxica de recursos
        AmbitoEnd,    // } — cierra la adscripción léxica, sin retirar recursos
        Fondo,        // Cambiar imagen de fondo
        Mostrar,      // Poner personaje en pantalla con posición
        Ocultar,      // Quitar personaje de pantalla
        Dialogo,      // Línea de diálogo con personaje
        Narrador,     // Texto sin personaje (narración)
        Musica,       // Reproducir música de fondo (loop)
        Sonido,       // Reproducir efecto de sonido (one-shot)
        PararMusica,  // Detener música
        Elegir,       // Bloque de opciones (choices)
        IrA,          // Saltar a otra escena
        Transicion,   // Efecto de transición visual
        Fin,          // Terminar el script / escena
        Set,          // Asignar / modificar variable:  set var = val | + n | - n
        MetaSet,      // Igual que Set pero PERSISTE en meta.eclips (inter-save)
        MetaClear,    // Vaciar meta.eclips:  meta_borrar / meta_reset
        Si,           // Condicional:  si var op val → salta a sino/fin_si si falso
        Sino,         // Rama else → salta a fin_si si se llega desde rama true
        FinSi,        // Cierre del bloque condicional
        Qte,          // Quick-Time Event:  qte [tecla: X, tiempo: Xs]: exito/fallo
        Capa,         // Capa/layer:  capa "nombre" "ruta" z=N x=N y=N w=N h=N
        Texto,        // Texto-capa:  texto [id] "contenido" x=N y=N color=#RGB z=N
        Fuente,       // Declarar fuente: fuente nombre "ruta" tamaño=N (GL-safe, ejecutable)
        CapaZ,        // Cambiar z:   capa_z "nombre" N
        CapaVisible,  // Visibilidad: capa_visible "nombre" si/no
        CapaFade,     // Fade alpha:  capa_fade "nombre" aparecer|desaparecer N
        CapaMover,    // Move pos:    capa_mover id a (x,y) en N [una_vez|loop] [suave]
        HotspotDef,   // Definir:     hotspot "nombre" x=N y=N w=N h=N
        Enlace,       // Texto clickable: enlace <id> [-> destino]  ("" = continuar)
        EsperarClick, // Bloquear:    esperar_click: "nombre" -> escena
        EsperarInterfaz, // Hold de UI: esperar_interfaz / wait_ui, sin dialogo ni duracion
        EventoRandom, // Def evento:  evento_random id cada=5s probabilidad=0.35 cooldown=60 -> escena
        IniciarEventos, // iniciar_eventos / start_events
        DetenerEventos, // detener_eventos / stop_events
        AzarSemilla,    // azar_semilla N / random_seed N — semilla del azar (sin N: reloj)
        AzarTraza,      // azar_traza si/no / random_trace yes/no — la voz del silencio
        Azar,           // azar [repetir=si|no]: — el bloque del eje CUAL (pierna B)
        Palabras,       // palabras N [repetir=]: — el pool clickeable (MUTAR_PALABRAS CP1)
        ParallaxToggle, // parallax si/no — pausa global del efecto
        SpeakerHighlight, // resaltar_hablante si/no
        OcultarCaja,    // ocultar_caja / hide_box — esconder UI del diálogo
        MostrarCaja,    // mostrar_caja / show_box — re-mostrar UI del diálogo
        DeshabilitarAcciones, // deshabilitar <lista> / disable <list> — sella verbos del jugador
        HabilitarAcciones,    // habilitar <lista>    / enable  <list> — los devuelve
        UsarCaja,       // usar_caja X / use_box X — cambiar plantilla de caja mid-escena
        PersonajeCompuesto, // personaje_compuesto alias "carpeta/"
        Lua,          // lua "script.lua" / ejecutar_lua "script.lua"
        CargarScript, // cargar_script "ruta.ecl" [escena] / load_script "path.ecl" [scene]
        Animar,           // animar alias efecto [Ns] [intensidad=N]
        DetenerAnimacion, // detener_animacion alias / stop_animation alias
        EsperarAnimacion, // esperar_animacion / wait_animation
        EsperarTiempo,    // esperar N / wait N — pausa por tiempo
        IrAPasillo,       // ir_a_pasillo [bg] [npc] [nombre] / go_to_corridor
        Vigneta,          // vigneta N [duracion] / vignette N [duration]
        IniciarBatalla,   // iniciar_batalla [alias] [hp:N] / start_battle
        CargarTema,       // cargar_tema "ruta" / load_theme "path"
        Gameplay,         // gameplay topdown "nivel.eclmap" / gameplay sidescroll "nivel.eclmap"
        Entrada,      // entrada <var> "prompt" — captura texto del jugador (B2)
        CinematicaBegin,  // cinematica [saltable=si|no] { — abre bloque que sella el input
        CinematicaEnd,    // } — cierra el bloque cinematica
        ConsolaCrear,     // consola crear <id> x= y= w= h= color_fondo= color_texto= tamano= fuente= [cursor=]
        ConsolaEscribir,  // consola_escribir <id> "..." — appendea + typewriter (input)
        ConsolaImprimir,  // consola_imprimir <id> "..." — appendea de golpe (output)
        ConsolaLimpiar,   // consola_limpiar <id> — vacía el buffer
        ConsolaBorrar,    // consola_borrar <id> [N | todo] — retract char-por-char
        Control,          // control <setting> x= y= ancho= min= max= paso= [estilo=] — widget de setting (Capa 2)
        Ranura,           // ranura <N> accion= x= y= ancho= alto= [estilo=] — slot-widget Tier-A (Ola 2)
        Unknown       // Error de parseo
    };

    // ── Opción de un bloque elegir: ───────────────────────────────────────────

    // ── EL GATE `[si:]`, UNA SOLA VEZ PARA TODO EL MOTOR ─────────────────────
    //
    // Estos cuatro campos vivían sueltos adentro de `EclipChoice`, y la pierna
    // C los necesitaba idénticos sobre un COMANDO (`evento_random`, que no es
    // una opción de bloque y nunca va a tener label ni destino). Copiarlos
    // habría sido exactamente el "segundo tipo casi igual" que el comentario de
    // `EclipChoice` ya había advertido — así que en vez de copiarlos, salieron.
    //
    // Es la misma cirugía que SYN-3 le hizo a los corchetes y SYN-4a a las
    // comillas: la ley vive en un lugar, y quien la necesita la INCLUYE.
    struct EclipGate
    {
        bool        has = false;  // hubo un `[si:]` y estaba bien escrito
        std::string var;
        std::string op;          // == != > < >= <=  (allowlist, la de SYN-2)
        std::string val;
    };

    struct EclipChoice
    {
        std::string label;   // Texto que ve el jugador
        std::string target;  // Nombre de la escena destino
        EclipGate   gate;    // `[si: var op val]` delante de la opción
        // ── Pierna B del azar: el peso de esta entrada dentro de un `azar:` ──
        // Vive acá y no en un struct nuevo porque `DISENO_MUTAR_PALABRAS` ya
        // escribió el plan así ("items sobre `EclipChoice` extendido con
        // `peso`").
        //
        // Default 1.0: un `azar:` sin un solo `peso=` reparte parejo, que es
        // lo que cualquiera espera al escribir tres líneas sin adornos.
        // Los comandos que NO son `azar:` (elegir, esperar_click) lo dejan en
        // 1.0 y nadie lo lee: campo aditivo, cero conducta cambiada.
        float       peso = 1.0f;
        // Runtime only: owner of the active named-scope generation. It is not
        // part of save text or the public EclipScript syntax.
        std::uint64_t scopeOwner = 0;
    };

    // secuencia / sequence: un renglon del bloque elegir (DISENO_SECUENCIA SQ-1)
    struct EclipSecuencia
    {
        EclipGate                gate;
        std::vector<std::string> teclas;  // nombres CRUDOS, como los escribio el creador
        std::string              target;
        int                      sourceLine = 0;
    };
    // ── Comando individual del script ─────────────────────────────────────────

    // ── E-CP5: una línea de EFECTO dentro de una receta nombrada ────────────
    // El QUÉ de una primitiva, resuelto en parse-time: claves canonizadas a
    // español, valores validados/clampeados (el mismo validador que la línea
    // `animar`). La intensidad es TIPADA (G4) y por-línea: multiplica con la
    // universal del `animar` al instanciar.
    struct EclipFxLine
    {
        FxKind kind = FxKind::None;
        std::map<std::string, std::string> params;
        float  intensidad = 1.0f;
        // E-CP6: nombre del efecto Lua (solo kind == FxKind::Lua). La línea
        // viaja resuelta como las demás; la función la resuelve el runtime
        // contra el registro vivo, al instanciar.
        std::string luaName;
    };

    // La definición de una receta nombrada (registro de parse, fork 1a):
    // canales (C-CP5) + líneas de efecto (E-CP5). Vive SOLO en el parser —
    // `animar` la expande RESUELTA dentro del cmd, jamás viaja al runtime.
    struct EclipAnimDef
    {
        std::vector<AnimTrack>   tracks;
        std::vector<EclipFxLine> fx;
        std::optional<ScalarEnvelope> fxIntensityEnvelope;
        bool empty() const
        {
            // Una envolvente sola no tiene consumidor: la receta sigue vacía
            // hasta contener al menos un canal o efecto real.
            return tracks.empty() && fx.empty();
        }
    };

    // ── RT-1 · PRESENCIA de campo (auditoria de semantica de runtime) ──────
    // Un `float = 0` no distingue "el creador escribio cero" de "no escribio la
    // clave", y esa confusion ES R-RT-01/02: en UPDATE los defaults de CREATE
    // salian como ORDENES, asi que un `capa placa "b.png"` sobre una capa ya
    // colocada la mandaba a (0,0) con z=0, tamano 0x0 (invisible: `elem->w/h`
    // van crudos al draw) y sin tinte.
    //
    // La presencia no se inventa aca: `kvFloat`/`kvInt` YA devuelven desde
    // SYN-1a un bool que dice "la clave estaba y se leyo bien", y los callers
    // lo tiraban al piso. Esto solo deja de descartarlo.
    //
    // Composicion con SYN-1a, a proposito: un valor MALO (`x=perro`) devuelve
    // false, o sea que NO marca presencia. La ley de 1a es "descarto el valor,
    // no el comando"; con presencia, "descartar el valor" significa exactamente
    // "el campo queda como estaba". Las dos leyes se suman sin pelearse.
    enum class EclipField : u32
    {
        X         = 1u << 0,
        Y         = 1u << 1,
        W         = 1u << 2,
        H         = 1u << 3,
        Z         = 1u << 4,
        Parallax  = 1u << 5,
        Tinte     = 1u << 6,
        Color     = 1u << 7,
        Fuente    = 1u << 8,
        Estilo    = 1u << 9,
        GlowColor = 1u << 10,
        GlowRadio = 1u << 11,
        GlowInten = 1u << 12,
        BordeColor  = 1u << 13,
        BordeGrosor = 1u << 14,
    };

    struct EclipCmd
    {
        EclipCmdType type = EclipCmdType::Unknown;
        std::string  arg1;      // Alias personaje, ruta archivo, nombre escena, variable...
        std::string  arg2;      // Texto diálogo, posición, operador...
        std::string  arg3;      // Uso futuro, valor de comparación...
        bool         arg1Literal = false; // true=arg1 venía con comillas (ruta literal); false=nombre lógico o vacío. Lo consume el ejecutor (Pasada 3).
        std::vector<EclipChoice> choices; // Solo para Elegir
        std::vector<EclipSecuencia> secuencias; // Solo para Elegir

        // ── `D-33`: LA FORMA PEGADA — `"linea" elegir:` ─────────────────────
        // Lo pone el parser sobre la LÍNEA (`Dialogo`/`Narrador`), nunca sobre
        // el `elegir`: la decisión se toma al EJECUTAR la línea (no parar, y
        // dejar que el `elegir` de al lado caiga en el mismo `update()`), y el
        // `elegir` que viene detrás es un comando normal que no sabe nada.
        //
        // Sólo es `true` si el bloque pegado trajo opciones de verdad: un
        // `"linea" elegir:` sin cuerpo avisa por el camino de siempre y la
        // línea queda como la de hoy, en vez de fusionarse contra el comando
        // que casualmente venga después (`D-38`, fail-closed).
        bool         fusedChoice = false;

        int          sourceLine = 0;      // Para mensajes de error útiles
        int          jumpTarget = -1;     // Para Si/Sino: índice a donde saltar
        // Parser-only lexical placement: 0 = host, non-zero = named scope.
        // Runtime ownership is assigned by enterAmbito(), not by this tag.
        std::uint64_t lexicalScopeOwner = 0;
        float        timerSeconds = 0.0f; // Para Elegir con timer (0 = sin timer)
        int          defaultIndex = -1;   // Índice de la opción default si el timer expira
        std::string  qteKey;              // Para Qte: tecla requerida ("E", "Space"...)
        std::string  qteSuccess;          // Escena destino si acierta
        std::string  qteFail;             // Escena destino si falla

        // ── RT-1: que campos NOMBRO la linea (ver EclipField arriba) ──
        // 0 = la linea no nombro ninguno. Lo consume el UPDATE de capa/texto;
        // el CREATE lo ignora y sigue rellenando defaults, que es su trabajo.
        u32          presentMask = 0;

        void mark(EclipField f)            { presentMask |= static_cast<u32>(f); }
        void mark(EclipField f, bool on)   { if (on) mark(f); }
        bool has (EclipField f) const      { return (presentMask & static_cast<u32>(f)) != 0; }

        // ── RT-1 · que le pide este `capa` a una capa que YA existe ─────────
        // El tinte llega ya parseado porque `parseHexColor` vive mas abajo, en
        // `EclipParser`, y `EclipCmd` no deberia conocerlo.
        //
        // LA LEY (CP-B): solo viaja lo que la linea NOMBRO. Lo callado queda
        // vacio, y `applyLayerPatch` lo conserva.
        //
        // El `tinte` no puede QUITARSE por omision, y eso es deliberado (mesa
        // RT-1, opcion D1): omitir dejo de significar "borralo". La capacidad
        // existe —`applyLayerPatch` borra con el optional enganchado en ""—,
        // lo que no tiene grafia asignada es como pedirlo; cuando alguien la
        // quiera es una linea de este lado. Ficha para SYN-4, que es el dueno
        // del vocabulario publico.
        LayerPatch toLayerPatch(f32 tr = 1.0f, f32 tg = 1.0f, f32 tb = 1.0f) const
        {
            LayerPatch p;
            if (has(EclipField::X))        p.x        = layerX;
            if (has(EclipField::Y))        p.y        = layerY;
            if (has(EclipField::W))        p.w        = layerW;
            if (has(EclipField::H))        p.h        = layerH;
            if (has(EclipField::Z))        p.z        = layerZ;
            if (has(EclipField::Parallax)) p.parallax = parallaxFactor;
            if (has(EclipField::Tinte))
            {
                p.tintHex = tintHex;
                p.tintR = tr; p.tintG = tg; p.tintB = tb;
            }
            return p;
        }

        // ── Capa / Hotspot (key=value params) ──
        float        layerX = 0, layerY = 0, layerW = 0, layerH = 0;
        int          layerZ = 0;
        float        parallaxFactor = 0.0f;  // Para capa con parallax=N
        bool         moveLoop = false;   // capa_mover: true=loop con wrap, false=una_vez
        EasingId     moveCurve = EasingId::Lineal;  // capa_mover: curva de la tabla SSOT
        std::string  tintHex;            // fondo/capa tinte=#rrggbb ("" = sin tinte)

        // ── COLOR-SOLIDO: `capa id "#rrggbb"` — el slot de la ruta, con color ──
        // "" = la capa es una IMAGEN (arg2 lleva la ruta, como siempre).
        // No-vacio = la capa es un RECTANGULO SOLIDO de este color, y entonces
        // `arg2` queda VACIO a proposito: si el color viajara tambien ahi, el
        // ejecutor podria intentar cargar una textura llamada "#000000" y
        // fallar mudo — un fallo que se leeria como "el color no se ve".
        // Excluyentes por construccion, no por convencion.
        //
        // Distinto de `tintHex`: el color dice QUE ES la capa; el tinte es un
        // filtro ENCIMA (y sobre una capa de color multiplica igual que sobre
        // una textura — sin caso especial, porque un caso especial es una ley
        // nueva que nadie pidio).
        std::string  layerColorHex;      // capa "#rrggbb" ("" = es imagen)

        // ── Consola (CP4): estilo de 'consola crear'. id→arg1, x/y/w/h→layerX..,
        //    z→layerZ, fuente→fontName, tamano→fontSize. ──
        f32          consoleBgR = 0.0f, consoleBgG = 0.0f, consoleBgB = 0.0f, consoleBgA = 0.6f;
        f32          consoleTxR = 1.0f, consoleTxG = 1.0f, consoleTxB = 1.0f;
        bool         consoleCursor = false;

        // ── Random events — el ENSAYO (SYN-4b, DISENO_AZAR_ECLIPS) ──
        // `probabilidad` no era un nombre equivocado: le faltaba el ensayo al
        // que pertenecer. Con el ritmo declarado, esto es la probabilidad de
        // UN ensayo, y la palabra dice la verdad a cualquier framerate.
        float        eventChance  = 0.0f;    // Probabilidad POR ENSAYO, [0,1]
        float        eventEvery   = 1.0f;    // `cada=` — segundos entre ensayos
        float        eventCooldown = 0.0f;   // Segundos entre activaciones
        std::string  eventTarget;            // Escena destino
        // ── El GATE `[si:]` del evento (pierna C, contrato §4.3) ──
        // `evento_random lluvia [si: _hora >= 18] cada=5s probabilidad=0.3 -> x`
        // El MISMO tipo que usan las opciones de bloque: es el mismo gate, con
        // la misma gramática y la misma puerta de parseo. Un evento sin `[si:]`
        // lo deja en su default (`has=false`) y nadie lo lee.
        EclipGate    eventGate;

        // ── azar_semilla / random_seed (ley 15) ──
        // Sin superficie para pedirla, la ley 12 exigía reproducibilidad y no
        // decía cómo: la sonda de distribución que el gate pide no podía existir.
        unsigned int azarSeed     = 0;       // La semilla, si `azarSeedFija`
        bool         azarSeedFija = false;   // false = volver a semilla de reloj

        // ── El bloque `azar:` — el eje CUAL (pierna B, ley 5) ──
        // El MODO es del BLOQUE y se declara con valor. El default es `si`
        // (distribución) porque `azar:` existe para muestrear; `palabras` nace
        // con `no` porque existe para agotar un pool. No hay un default del
        // motor correcto para las dos formas: por eso la clave lleva valor y
        // no es una bandera pelada.
        bool         azarRepetir = true;     // `repetir=` — true = con reposición

        // ── El bloque `palabras` — el pool clickeable (MUTAR_PALABRAS CP1) ──
        // CUANTAS se muestran de una, que NO es cuantas hay en el pool: el
        // bloque declara el pool entero y saca N. Sin declarar = 0, y eso es
        // fail-closed (SYN-2): un bloque que no dice cuantas mostrar no tiene
        // conducta razonable por default.
        //
        // El MODO (`repetir=`) viaja en `azarRepetir`, compartido con `azar:`
        // a proposito — es la misma ley con distinto DEFAULT por bloque, y eso
        // ya estaba escrito arriba desde la pierna B.
        int          palabrasN = 0;

        // ── Efecto de personaje (mostrar/ocultar) ──
        int          charEffect = 0;      // 0=fade(default), 1=instant, 2=slide

        // ── Animación / Tween ──
        float        animDuration = 0.0f;   // Segundos (0 = usar default del efecto)
        // ANIMAR-DESPUES-01: demora de la INVOCACION completa, no del track.
        // La ausencia conserva exactamente el reloj inicial 0.
        float        animAfter = 0.0f;
        bool         animAfterSpecified = false;
        float        animIntensity = 1.0f;   // Multiplicador de efecto
        // C-CP3: `vaiven` (Nudge) YA leía period/axisVertical en CharAnim, pero
        // el parser nunca los llenaba → eran inalcanzables desde EclipScript.
        // Esto no agrega capacidad: le abre la superficie a la que ya existía.
        float        animPeriod = 0.0f;        // periodo= (0 = default del preset)
        bool         animAxisVertical = false; // eje= (x|horizontal · y|vertical)

        // ── C-CP5/E-CP5: animación NOMBRADA (resuelta en PARSE-time) ──
        // namedName vacío = no es nombrada. En Animar, namedTracks (canales) y
        // namedFx (líneas de efecto) viajan RESUELTOS: el cmd es auto-
        // contenido, espejo del `T;`/`E;` del save — ni el runtime ni el load
        // necesitan la definición. En DetenerAnimacion, namedName marca que
        // arg2 es una receta (el runtime baja el conjunto por origen).
        std::string namedName;
        std::vector<AnimTrack> namedTracks;
        std::vector<EclipFxLine> namedFx;
        // Reloj-diálogo CP2: la envolvente GRUPAL de intensidad viaja RESUELTA
        // igual que namedFx — modula ctx.intensity de cada efecto de la receta
        // por avances de diálogo (§4/§8). Vacía = la receta no la declaró.
        std::optional<ScalarEnvelope> namedFxEnvelope;

        // ── E-CP6: efecto LUA (cuarta especie de `animar`/`detener`) ──
        // Vacío = no es Lua. El nombre se resolvió en parse-time contra las
        // declaraciones `efecto_lua` (la voz §7.89 sigue viva); la FUNCIÓN se
        // resuelve en runtime contra el registro (cargado al boot).
        std::string luaFxName;

        // ── E-CP0: params de una PRIMITIVA de efecto (franja, ...). Vacío para
        // los presets, que tienen campos tipados propios (arriba). Las claves ya
        // vienen CANONIZADAS a español desde el parser (§1.40): el creador puede
        // escribir speed= o velocidad=, pero aquí y en disco hay UNA sola grafía.
        std::map<std::string, std::string> fxParams;

        // ── Esperar por tiempo ──
        float        waitDuration = 0.0f;   // Para EsperarTiempo: segundos a esperar

        // ── Fuente nombrada (comando 'fuente' + 'fuente=' en texto) ──
        std::string  fontName;              // nombre que el creador inventa
        f32          fontSize = 0.0f;       // tamaño del atlas (0 = no especificado)
        f32          fontKerning = 0.0f;    // espaciado_letras del comando 'fuente' (px, signado; horneado como el tamaño)
        FontMeasure  fontMeasure = FontMeasure::Height; // ausencia = identidad legacy
        bool         fontMeasureSpecified = false;      // medida= nombrada por el creador

        // ── Glow de texto-capa (sesion glow) ──
        // Defaults en 0 = "sin glow". El parser solo los toca si el creador
        // escribe glow_*/brillo_*. Color RGB puro (alpha separado via intensidad).
        f32          glowR = 0.0f, glowG = 0.0f, glowB = 0.0f;  // color del halo
        f32          glowRadius = 0.0f;       // 0 = sin glow; default util = 2
        f32          glowIntensity = 0.0f;    // 0 = sin glow; default util = 0.5
        std::string  glowColorHex;            // hex crudo, parseado en cmdTexto

        // ── Borde de texto-capa (OUTLINE-TEXTO-CAPA) ──
        // Hermano del glow y con la misma forma: defaults en 0 = "sin borde",
        // el parser solo los toca si el creador escribe borde_*/outline_*.
        // El hex viaja CRUDO (como el del glow y el del color) y se resuelve al
        // ejecutar; por eso no hay r/g/b aca.
        //
        // La capacidad de render ya existia y tenia callers INTERNOS: la caja
        // de dialogo dibuja el nombre y el cuerpo con borde desde siempre
        // (`VNDialogBox.cpp:646/662` -> `drawTextOutlined*`), y el tema ya se lo
        // deja elegir al creador (`nombre_outline_grosor` en `Theme.h`). Lo que
        // NO existia era el cable al texto-capa y a las opciones, y el propio
        // `ProjectManifest.h` lo declaraba: "Outline tampoco (aun no cableado a
        // texto-capa)". Esto es ese cable (7.383: primitiva con callers
        // internos y cero del creador = cable que falta, no capacidad nueva).
        std::string  bordeColorHex;           // hex crudo ("" = sin borde)
        f32          bordeGrosor = 0.0f;      // px; 0 = sin borde; default util = 2

        // ── Estilo nombrado ('estilo=' en texto) ──
        // Nombre del paquete (fuente+color+glow) declarado en proyecto.eclips.
        // Vacio = sin estilo. El merge per-propiedad vive en cmdTexto: lo que
        // el creador escribio inline pisa lo que trae el estilo.
        std::string  styleName;

        // ── Entrada de texto (comando 'entrada'/'input', sesion B2) ──
        // arg1 = variable destino · arg2 = prompt · arg3 = valor por defecto.
        // inputMaxLen: 0 = sin limite. arg3 + inputMaxLen se llenan en P5
        // (params max/default); P1 solo captura var + prompt.
        int          inputMaxLen = 0;
        bool         inputRequired = false;  // 'requerido'/'required': prohíbe confirmar vacío

        // ── Enlace requerido (modalidad-aviso) ──
        // 'requerido'/'required' sobre un 'enlace'-OK: mientras ese enlace esté
        // vivo, la caja-aviso es modal (sella skip/auto/save + avance plano; solo
        // el OK sale). Espeja inputRequired, pero para el aviso SIN input.
        bool         linkRequired = false;

        // ── Cinemática (CP1, sesión cinemática) ──
        // 'cinematica saltable=si|no {' — solo lo lee CinematicaBegin.
        // false (default) = no-saltable: el bloque sella el skip mientras dura.
        bool         cineSaltable = false;

        // ── El sello de acciones (`D-48`/`D-50`) ──
        // Los verbos que la línea nombró, YA resueltos a bits en parse-time.
        // Sólo lo leen DeshabilitarAcciones/HabilitarAcciones, y el ejecutor no
        // vuelve a mirar strings: la tabla de vocabulario vive en un solo lugar
        // (`engine/core/VNActionSeal.h`), y dos tablas del mismo vocabulario es
        // cómo se desincronizan las grafías (§7.442).
        eclips::VNActionMask accionesMask = 0u;

        // ── Control de setting (Capa 2, comando 'control') ──
        // x/y reusan layerX/layerY; el id del setting reusa arg1; el estilo
        // reusa styleName. Modelo de valor: -1 = "no especificado" → el ejecutor
        // (cmdControl) usa el default del setting (registro Tier-A).
        float        controlMin = -1.0f;
        float        controlMax = -1.0f;
        float        controlStep = -1.0f;
        float        controlWidth = -1.0f;
        std::string  controlType;          // tipo= override (D2): "toggle"/"barra"; vacío = default por kind

        // ── Ranura / slot-widget (Capa 2, comando 'ranura', Ola 2) ──
        // N (1-based del creador) reusa arg1; x/y reusan layerX/layerY; ancho
        // reusa controlWidth; estilo reusa styleName. La acción es propia:
        std::string  slotAction;           // accion= : "cargar"/"guardar"/"borrar" (C1: solo cargar cableado)
        bool         autoHeight = false;   // ranura alto=auto → medir el stack (A3: default en vertical)
    };

    // ── Escena: lista ordenada de comandos ────────────────────────────────────

    struct EclipScene
    {
        std::string           name;
        std::vector<EclipCmd> cmds;
    };

    // ── Script completo parseado ──────────────────────────────────────────────

    struct EclipScriptData
    {
        // Mapa de escenas: nombre → escena
        std::map<std::string, EclipScene> scenes;

        // Mapa global de personajes: alias → ruta base
        // (ej: "Luna" → "assets/personajes/Luna.png")
        std::map<std::string, std::string> characters;

        // Personajes compuestos : alias → carpeta base
        // (ej: "Luna" → "assets/ddlc/Luna/")
        std::set<std::string> compositeChars;

        // [Paso 2 hot-reload] Procedencia: aliases cuya definición vino del
        // MANIFEST y NO fue overrideada localmente por este .ecl. El paso 3
        // (imágenes en vivo) consulta esto para saber si puede refrescar un
        // personaje en F8 sin re-parsear. Se nombra por PROCEDENCIA (un hecho),
        // no por capacidad. Estado DERIVADO: se reconstruye en cada parseo, no
        // se serializa en saves.
        // TODO(modo-creador): se compila fuera en build de export.
        std::set<std::string> charactersFromManifest;

        // [Paso 2 hot-reload] Mergea SOLO los tres mapas de personajes desde
        // 'other' (characters + compositeChars + charactersFromManifest). NO
        // toca scenes/firstScene (eso es reemplazo std::move, semántica
        // distinta). Punto único de verdad del merge: lo usan loadNewScript y
        // loadFromSlot, que antes duplicaban esto. Regla: 'other' manda sobre
        // los aliases que toca; los demás conservan su estado.
        // TODO(modo-creador): se compila fuera en build de export.
        void mergeCharactersFrom(const EclipScriptData& other)
        {
            for (const auto& [alias, charPath] : other.characters)
            {
                characters[alias] = charPath;
                if (other.charactersFromManifest.count(alias))
                    charactersFromManifest.insert(alias);
                else
                    charactersFromManifest.erase(alias);
            }
            for (const auto& alias : other.compositeChars)
                compositeChars.insert(alias);
        }

        // ── E-CP6: efectos Lua DECLARADOS (`efecto_lua nombre "ruta.lua"`),
        // en orden de archivo. La declaración es la que hace posible la
        // resolución en parse-time (la voz §7.89) Y la carga al BOOT del
        // estado — por eso el registro ya existe cuando un load salta a
        // mitad de escena (§7.290: el invariante tiene quien lo restaure).
        std::vector<std::pair<std::string, std::string>> luaEffects;

        // ── ATLAS-DE-FUENTE CP-R3: las declaraciones `fuente` del archivo,
        // en orden de archivo. Es TRANSPORTE, igual que `luaEffects`: el
        // parseo entero termina antes de `jumpToScene`, así que el state puede
        // materializar los perfiles en fase GL segura ANTES de ejecutar el
        // primer comando. Eso es lo que retira la dependencia de orden — hasta
        // este CP, una selección escrita arriba de la declaración caía a base y
        // una escrita abajo funcionaba.
        //
        // El `EclipCmdType::Fuente` sigue emitiéndose y sigue siendo ejecutable,
        // pero ya no decide cuándo nace el perfil: quedó como marca idempotente.
        std::vector<fontdecl::Declaracion> fuentes;

        // Nombre de la primera escena declarada (punto de entrada)
        std::string firstScene;

        // Estado del parseo
        bool        valid = false;
        std::string error;
    };


    // ════════════════════════════════════════════════════════════════════════
    // SYN-1c-ii — EL INVENTARIO DE CLAVES `k=v`
    // ════════════════════════════════════════════════════════════════════════
    //
    // Nueve comandos leen sus parametros de un mapa `clave=valor`. Hasta aca
    // cada uno conocia su vocabulario DE MEMORIA, repartido en sus propias
    // lineas de lectura, y eso dejaba dos agujeros MUDOS:
    //
    //   1. una clave que el comando no conoce se descartaba en silencio
    //      (`capa fondo "x.png" anchoo=200` no hacia nada y no lo decia);
    //   2. si el creador escribia las DOS grafias de un parametro bilingue
    //      (`ancho=200 width=300`), quien ganaba lo decidia EL ORDEN DE DOS
    //      LINEAS DE C++ — y no era el mismo orden en todos los comandos:
    //      `consola crear` resolvia por la grafia ES (`getKV(a,b)` corta en
    //      la primera) y `control` por la EN (dos `kvFloat` seguidos, pisa el
    //      segundo). Tres leyes de desempate distintas en nueve comandos.
    //
    // El modelo NO se inventa aca: es el de los efectos
    // (`fxKeysOf`/`fxAcceptsKey`/`fxKeysHelp`, `EffectSystem.h`), que ya
    // resolvio esto para la familia `animar` — y el de `fondo`, que ya tenia
    // el aviso inline a cuatro lineas de su propio call-site.
    //
    // ── LA UNIDAD ES EL GRUPO, NO LA CLAVE SUELTA ───────────────────────────
    //
    // Cada parametro es un GRUPO de grafias: `{"ancho","width"}` es UN
    // parametro escrito de dos maneras, no dos parametros. Modelar el grupo
    // es lo unico que permite ver el choque y desempatarlo; una lista plana
    // de claves validas habria tapado el agujero (1) y dejado el (2) intacto.
    //
    //   >>> LA LEY (mesa SYN-1c-ii): GANA LA PRIMERA GRAFIA DEL GRUPO, Y EL
    //   >>> MOTOR AVISA IGUAL. Vale para las dos formas del choque: dos
    //   >>> grafias del mismo parametro (`ancho=` + `width=`) y la misma
    //   >>> clave repetida (`x=1 x=2`). Una frase, sin excepciones.
    //
    // Por que "avisa igual": escribir las dos grafias SIEMPRE es un error del
    // creador. Lo que el motor debe no es adivinar cual quiso — es elegir
    // siempre lo mismo Y decirlo. Un desempate callado es la version educada
    // del bug que teniamos.
    //
    // `grafias[0]` es la CANONICA: la que gana, la unica que se lista en la
    // ayuda, y la unica que ven los lectores de abajo — `kvParams` canoniza
    // el mapa EN LA PUERTA, una sola vez, igual que `canonFxKey` en la linea
    // `animar`. Por eso los `if (kv.count("width"))` duplicados
    // DESAPARECIERON de los callers: la grafia EN ya llega traducida.
    //
    // El orden ES-primero no es gusto: el motor ya canoniza a espanol en los
    // efectos (§7.249), y con una sola grafia por grupo la ayuda queda en un
    // idioma en vez de escupir `tamano= tamano= size=`.
    //
    // ── LO QUE ESTA TABLA **NO** ES ─────────────────────────────────────────
    //
    // No es la ley del VALOR: el tipo, el rango y la finitud siguen viviendo
    // en `kvFloat`/`kvInt`/`fxValidValue` (SYN-1a). Aca solo vive el
    // VOCABULARIO: que palabras conoce cada comando. Y no cubre las otras
    // mini-gramaticas de parametros (los corchetes `[timer:, default:]` de
    // `elegir`/`qte`/`entrada` tragan claves desconocidas igual de mudos) —
    // esas son de SYN-3, que es dueno de la gramatica de bloques.
    // ════════════════════════════════════════════════════════════════════════

    // Grupos de grafias que conoce cada comando. `grafias[0]` = la canonica.
    // Vacio = el comando no tiene vocabulario `k=v` (no pasa por `kvParams`).
    inline const std::vector<std::vector<std::string>>& kvKeysOf(EclipCmdType t)
    {
        static const std::vector<std::vector<std::string>> kVacio;
        static const std::map<EclipCmdType,
                              std::vector<std::vector<std::string>>> kTabla = {
            { EclipCmdType::Fondo, {
                { "tinte", "tint" } } },

            { EclipCmdType::HotspotDef, {
                { "x" }, { "y" }, { "w" }, { "h" } } },

            { EclipCmdType::Control, {
                { "x" }, { "y" },
                { "ancho", "width" },
                { "min" }, { "max" },
                { "paso", "step" },
                { "estilo", "style" },
                { "tipo", "type" } } },

            { EclipCmdType::Ranura, {
                { "x" }, { "y" },
                { "ancho", "width" },
                { "alto", "height" },
                { "accion", "action" },
                { "estilo", "style" } } },

            { EclipCmdType::Capa, {
                { "z" }, { "x" }, { "y" }, { "w" }, { "h" },
                { "parallax" },
                { "tinte", "tint" } } },

            // ── CP-R3: ESTA FILA YA NO SE ESCRIBE ACA ───────────────────────
            // Los tres grupos viven en `engine/core/FontDeclaration.h`, porque
            // desde `CP-R3` la MISMA cabecera se escribe en dos dialectos —el
            // `.ecl` y el catalogo top-level de `proyecto.eclips`— y dos tablas
            // dejarian divergir el alias ingles el dia uno. Ahi tambien esta
            // narrado el bug de la enie doble-encodeada que este comentario
            // llevaba: `tamaño` con la enie de verdad NUNCA matcheo, porque el
            // `u8"..."` re-codificaba un literal que ya era UTF-8.
            { EclipCmdType::Fuente, fontdecl::gruposParametro() },

            { EclipCmdType::Texto, {
                { "x" }, { "y" }, { "z" },
                { "color" },
                { "fuente", "font" },
                { "estilo", "style" },
                // ── LAS DOS QUIMERAS, RETIRADAS (SYN-4c-ii, Y-VN-01) ────────
                // El glow nacio con grafias MEZCLADAS vivas: `glow_radio` y
                // `glow_intensidad` son prefijo INGLES + sufijo ESPANOL, o sea
                // un tercer idioma que el manual nunca prometio — promete DOS.
                // El renglon que vivia aca decia "se conservan las tres:
                // retirarlas seria migracion de vocabulario, y eso es SYN-4":
                // ficha con dueno escrito, cobrada por su dueno.
                //
                // Migradas 6 lineas del corpus (todas en sondas/bancos, cero en
                // lo que shippea) y 4 renglones del manual. `glow_color` NO se
                // toca y ese es el criterio: `color` se escribe igual en los
                // dos idiomas, asi que no es una mezcla — es una grafia EN
                // legitima. El test es si la palabra pertenece a UN idioma, no
                // si empieza con `glow_`.
                { "brillo_color", "glow_color" },
                { "brillo_radio", "glow_radius" },
                { "brillo_intensidad", "glow_intensity" },
                // ── EL BORDE (OUTLINE-TEXTO-CAPA), y por que se llama asi ───
                // La grafia ES es `borde_*`. `outline_grosor` habria sido
                // prefijo INGLES + sufijo ESPANOL: exactamente la quimera que
                // el lote de arriba acaba de retirar. El TEMA si la escribe
                // asi (`nombre_outline_grosor`, `Theme.h`) y no se copio a
                // proposito — ese vocabulario queda fichado para migrar, no
                // propagado. El test es el mismo de siempre: si la palabra
                // pertenece a UN idioma.
                //   `borde_grosor`      ES+ES  ok
                //   `outline_thickness` EN+EN  ok
                //   `outline_grosor`    EN+ES  quimera -> NO existe
                { "borde_color", "outline_color" },
                { "borde_grosor", "outline_thickness" } } },

            { EclipCmdType::EventoRandom, {
                // R-VN-07 CERRADO (SYN-4b, 2026-07-25). El destino no fue
                // `tasa/rate`: `probabilidad` no era un nombre equivocado, le
                // faltaba el ENSAYO. `cada=` lo declara y la palabra se vuelve
                // cierta sin migrar una linea del corpus.
                { "cada", "every" },
                { "probabilidad", "chance" },
                { "cooldown" } } },

            { EclipCmdType::Azar, {
                // El bloque solo declara su MODO; el `peso=` es de cada
                // ENTRADA y lo lee el lector del cuerpo, no esta puerta.
                { "repetir", "repeat" } } },

            { EclipCmdType::Palabras, {
                // Misma clave y misma ley que su hermano `azar:` — lo unico
                // que difiere es el DEFAULT, y por eso la clave lleva valor.
                // La `N` NO entra aca: es posicional, no `k=v`.
                { "repetir", "repeat" },
                // ── `CP2a`: EL AREA Y EL ESTILO. CERO PALABRAS NUEVAS ────────
                // Las seis ya existen en el motor, y eso es el punto: §2.9 dice
                // que la mejor forma de no equivocarse al nombrar es no tener
                // que nombrar. `columnas` NO entra — el acomodo envuelve contra
                // `ancho`, asi que la palabra no hace falta.
                //
                // `ancho`/`alto` y no `w`/`h`, y el desempate es MEDIDO: el
                // motor dice `w`/`h` en `capa`/`hotspot`/`consola` y
                // `ancho`/`alto` en `ranura` y `control` — que son justo los dos
                // que ademas llevan `estilo=`, o sea la familia a la que este
                // bloque se parece.
                { "x" }, { "y" }, { "z" },
                { "ancho", "width" },
                { "alto", "height" },
                { "estilo", "style" } } },

            { EclipCmdType::ConsolaCrear, {
                { "z" }, { "x" }, { "y" }, { "w" }, { "h" },
                { "color_fondo", "bg_color" },
                { "color_texto", "text_color" },
                { "tamano", "tama\xC3\xB1o", "size" },   // ver la nota de `fuente`
                { "fuente", "font" },
                { "cursor" } } },
        };

        auto it = kTabla.find(t);
        return (it == kTabla.end()) ? kVacio : it->second;
    }

    // La grafia CANONICA de `k` para ese comando, o "" si no la conoce.
    inline std::string kvCanonKey(EclipCmdType t, const std::string& k)
    {
        for (const auto& grupo : kvKeysOf(t))
            for (const auto& g : grupo)
                if (g == k) return grupo[0];
        return "";
    }

    inline bool kvAcceptsKey(EclipCmdType t, const std::string& k)
    {
        return !kvCanonKey(t, k).empty();
    }

    // Para el mensaje de ayuda: "x= y= ancho= ...". Solo las CANONICAS —
    // listar las tres grafias de `tamaño=` no ensena, satura.
    inline std::string kvKeysHelp(EclipCmdType t)
    {
        std::string s;
        for (const auto& grupo : kvKeysOf(t)) s += grupo[0] + "= ";
        if (!s.empty()) s.pop_back();
        return s;
    }

    // ════════════════════════════════════════════════════════════════════════
    // SYN-4c-ii (ficha G2) · EL MAPA QUE SE ACUERDA DE QUE GRAFIA ENTRO
    //
    //   >>> UN AVISO SOBRE UN VALOR NOMBRA LA GRAFIA QUE EL CREADOR ESCRIBIO,
    //   >>> NUNCA LA CANONICA A LA QUE EL MOTOR LA TRADUJO.
    //
    // 1c-ii canonizo el mapa EN LA PUERTA y eso fue lo correcto: borro los
    // nueve `if (kv.count("width"))` duplicados y dejo UNA ley de desempate.
    // Pero canonizar tira informacion, y la que tiro era la unica que el AVISO
    // necesitaba: los lectores de abajo dejaron de saber que habia tecleado el
    // creador, asi que empezaron a citarle una palabra que nunca escribio.
    //
    //     fuente titulo "serif.ttf" size=perro
    //     -> "fuente: tamano= espera un numero, recibio 'perro'."
    //                ^^^^^^ el creador escribio `size=`
    //
    // Es la deuda que la propia sesion de 1c-ii ficho al cerrar. No se arregla
    // des-canonizando —eso reabriria los nueve callers y las tres leyes de
    // desempate— sino haciendo que la puerta se ACUERDE: adentro sigue viajando
    // UNA sola grafia (`vals`), y al lado va el recibo de cual entro
    // (`grafias`). Los 32 call-sites de `kvFloat`/`kvInt` no cambiaron ni una
    // letra: la ley entro donde entra siempre, en la puerta.
    //
    // Por que un tipo y no un segundo parametro en los 32 call-sites: porque un
    // parametro se puede olvidar en el call-site 33. El mapa y su recibo no se
    // pueden separar si son el mismo objeto.
    struct KvMap
    {
        std::map<std::string, std::string> vals;      // canonica -> valor
        std::map<std::string, std::string> grafias;   // canonica -> la escrita

        using const_iterator =
            std::map<std::string, std::string>::const_iterator;

        const_iterator find(const std::string& k) const { return vals.find(k); }
        const_iterator begin() const { return vals.begin(); }
        const_iterator end()   const { return vals.end(); }
        size_t count(const std::string& k) const { return vals.count(k); }
        bool   empty() const { return vals.empty(); }

        // A diferencia de `std::map::operator[]`, este NO inserta: preguntar
        // por un parametro ausente devuelve "" y deja el mapa como estaba.
        const std::string& operator[](const std::string& k) const
        {
            static const std::string kVacio;
            auto it = vals.find(k);
            return (it == vals.end()) ? kVacio : it->second;
        }

        // >>> LA LEY. Si el grupo tiene una sola grafia, escrita == canonica y
        // el aviso no cambia ni una letra (son 24 de los 32 call-sites: ese es
        // el control positivo del harness). Devuelve por VALOR a proposito: los
        // callers pasan literales, y una referencia a un temporal seria una
        // trampa esperando al call-site 33.
        std::string escrita(const std::string& canon) const
        {
            auto it = grafias.find(canon);
            return (it == grafias.end()) ? canon : it->second;
        }
    };

    // ════════════════════════════════════════════════════════════════════════
    // SYN-3 · EL INVENTARIO DE LOS CORCHETES — `[clave: valor]`
    //
    // Gemelo de `kvKeysOf`, para la OTRA mini-gramatica de parametros. Misma
    // ley (§7.322-323): cada comando declara que claves conoce, la unidad es
    // el GRUPO de grafias, `grafias[0]` es la canonica y gana el desempate, y
    // la clave que no esta en la lista se descarta CON VOZ.
    //
    // ── POR QUE HACIA FALTA UN SEGUNDO INVENTARIO Y NO ALCANZABA `kvKeysOf` ──
    //
    // Son gramaticas distintas, no dos usos de la misma: `k=v` se separa por
    // ESPACIOS y no admite valor con espacios; `[k: v, k: v]` se separa por
    // COMAS y su valor citado puede tener los dos. Meter las dos en una tabla
    // habria obligado a que el tokenizador adivine cual esta leyendo.
    //
    // Lo que si se comparte es la LEY, y por eso este archivo tiene ahora dos
    // tablas con la misma forma en vez de dos parsers con leyes distintas.
    //
    // ── LO QUE ESTA TABLA ARREGLA, MEDIDO ───────────────────────────────────
    //
    // La auditoria conto los corchetes como UNA mini-gramatica (Y-VN-02). El
    // codigo tenia CINCO parsers a mano, y `parseBlockParams` estaba
    // COMPARTIDO entre `elegir` y `qte`, asi que cada uno recibia el
    // vocabulario del otro y lo tiraba mudo:
    //
    //   · `qte [... default: "algo"]`  -> el default iba a un `dummy`;
    //   · `elegir [... tecla: E]`      -> la tecla iba a `cmd.qteKey`, que solo
    //                                     se lee en el case de QTE.
    //
    // Y `esperar_click` no parseaba corchetes en absoluto: se tragaba la linea
    // entera despues del keyword sin una palabra. Su tabla queda VACIA a
    // proposito — la ausencia es explicita, no un descuido que se lea como
    // "acepta cualquier cosa" (mismo patron que `kvKeysOf(Mostrar)`).
    //
    // >>> EL QTE NO LLEVA `default:` Y NO ES UN OLVIDO: ya tiene uno, y se
    // >>> llama `fallo`. Al expirar el timer, un `elegir` tiene N salidas y
    // >>> ninguna se llama "la del timeout" —por eso necesita que le digan
    // >>> cual—; un `qte` tiene UNA, declarada por nombre dos renglones mas
    // >>> abajo en su propio cuerpo. `default:` no es "el valor por defecto de
    // >>> algo": es el DESEMPATE de una ambiguedad, y el qte no la tiene.
    // ════════════════════════════════════════════════════════════════════════

    struct BracketKey
    {
        std::vector<std::string> grafias;  // [0] = la canonica
        bool bandera = false;              // se escribe sin `: valor` ([requerido])
    };

    inline const std::vector<BracketKey>& bracketKeysOf(EclipCmdType t)
    {
        static const std::vector<BracketKey> kVacio;
        static const std::map<EclipCmdType, std::vector<BracketKey>> kTabla = {
            { EclipCmdType::Elegir, {
                { { "tiempo", "timer", "time" } },
                // v0.8.8: `defecto` es NUEVA — y es el bump de SYN-3. La
                // cabecera del manual promete que todo se acepta en espanol e
                // ingles, y esta clave era ES-menos: `entrada` ya tenia las dos
                // y `elegir` solo la inglesa. Con el GRUPO como unidad (1c-ii),
                // cerrar el hueco es una palabra en una lista.
                { { "defecto", "default" } } } },

            { EclipCmdType::Qte, {
                { { "tiempo", "timer", "time" } },
                { { "tecla", "key" } } } },

            { EclipCmdType::Entrada, {
                { { "maximo", "max" } },
                { { "defecto", "default" } },
                { { "requerido", "required" }, /*bandera=*/true } } },

            // `mostrar`/`ocultar` comparten `extractCharEffect`, y comparten
            // tabla porque comparten la grafia. El VALOR tiene su propio
            // vocabulario (`bracketEffectValue`): es el unico corchete del
            // motor que se tragaba mudos los dos lados.
            { EclipCmdType::Mostrar, { { { "efecto", "effect" } } } },
            { EclipCmdType::Ocultar, { { { "efecto", "effect" } } } },
        };

        auto it = kTabla.find(t);
        return (it == kTabla.end()) ? kVacio : it->second;
    }

    // La grafia CANONICA de `k` para ese comando, o "" si no la conoce.
    inline std::string bracketCanonKey(EclipCmdType t, const std::string& k)
    {
        for (const auto& bk : bracketKeysOf(t))
            for (const auto& g : bk.grafias)
                if (g == k) return bk.grafias[0];
        return "";
    }

    inline bool bracketAcceptsKey(EclipCmdType t, const std::string& k)
    {
        return !bracketCanonKey(t, k).empty();
    }

    // ¿Esa clave se escribe SIN `: valor`? (hoy solo `[requerido]`.)
    inline bool bracketIsFlag(EclipCmdType t, const std::string& k)
    {
        for (const auto& bk : bracketKeysOf(t))
            for (const auto& g : bk.grafias)
                if (g == k) return bk.bandera;
        return false;
    }

    // Para el mensaje de ayuda: "tiempo: default:". Solo las CANONICAS — las
    // banderas van sin `:`, porque asi se escriben.
    inline std::string bracketKeysHelp(EclipCmdType t)
    {
        std::string s;
        for (const auto& bk : bracketKeysOf(t))
            s += bk.grafias[0] + (bk.bandera ? " " : ": ");
        if (!s.empty()) s.pop_back();
        return s;
    }

    // ── El VALOR de `[efecto: X]` ───────────────────────────────────────────
    //
    // `fade` estaba DOCUMENTADO (`ECLIPSCRIPT_DOCS:4064`,
    // `[efecto: fade|instant|slide]`) y el codigo no lo matcheaba nunca: caia
    // al `return 0` final de `extractCharEffect`. O sea que funcionaba por
    // accidente y `fadee` daba exactamente lo mismo. Al hacer hablar al valor
    // desconocido hay que reconocerlo de verdad, o el manual queda mintiendo.
    inline bool bracketEffectValue(const std::string& v, int& out)
    {
        static const std::vector<std::pair<std::vector<std::string>, int>> kTabla = {
            // `desvanecer` entro por el ticket #8 (`cobertura-bilingue`). NO se
            // invento: el motor YA la escribe para este concepto exacto en otro
            // dialecto -- `MapData.h:3663` dice `lo == "desvanecer" || lo ==
            // "fade"` para el valor de `animacion_entrada`. `fade` era el UNICO
            // de los tres sin grafia espanola, y sus dos hermanos si la tienen.
            // Se deja `fade` primero a proposito: es el DEFAULT, y el aviso de
            // abajo dice "Se usa fade." -- ese texto no cambia en este lote.
            { { "fade", "desvanecer" },                 0 },
            { { "instantaneo", "instant", "instante" }, 1 },
            { { "deslizar", "slide" },                  2 },
        };
        for (const auto& [grafias, valor] : kTabla)
            for (const auto& g : grafias)
                if (g == v) { out = valor; return true; }
        return false;
    }

    // SEGUNDO SITIO de la tabla de arriba, y esta escrito A MANO: si alguien
    // agrega una grafia alla y no la copia aca, el aviso le nombra al creador
    // una lista incompleta y el motor queda ensenando menos de lo que acepta.
    // Es la misma enfermedad que la ficha `NUMEROS-EN-PROSA`, en codigo.
    // (Ticket #8: `desvanecer` entro en los dos sitios, en el mismo diff.)
    inline std::string bracketEffectHelp()
    {
        return "fade/desvanecer instantaneo deslizar";
    }


    // ════════════════════════════════════════════════════════════════════════
    // EclipParser — Lee un .ecl y produce un EclipScriptData
    // ════════════════════════════════════════════════════════════════════════

    class EclipParser
    {
    public:
        // Los SEIS operadores de comparación del motor. Es literalmente la
        // allowlist que SYN-2 le puso al comando `si` — y está acá arriba, en
        // una función, porque tenerla escrita dos veces es cómo un lado empieza
        // a aceptar lo que el otro rechaza.
        static bool esOperadorDeCondicion(const std::string& op)
        {
            return op == "==" || op == "!=" || op == ">" || op == "<" ||
                   op == ">=" || op == "<=";
        }

        // Qué pasó al intentar pelar un gate del principio de una línea.
        // Son TRES estados y no dos: "no había gate" y "había uno y está roto"
        // piden cosas opuestas del caller, y con un bool se veían igual.
        enum class GateParse
        {
            Ninguno,      // la línea no empieza con `[si:`/`[if:`
            Ok,           // gate válido, pelado de `line` y escrito en `g`
            Malformado    // había gate y NO se entiende — ya se avisó con línea
        };

        // ── Pela un `[si: var op val]` / `[if: ...]` del principio de `line` ──
        //
        // PUERTA ÚNICA (pierna C, fork 3b firmado 2026-07-30): la usan las
        // opciones de bloque (`elegir`, `esperar_click`) y el `evento_random`.
        //
        // **El operador se valida ACÁ, y antes no se validaba en ningún lado.**
        // SYN-2 le puso allowlist al comando `si` y lo MATA si no la cumple
        // (`si: forma invalida`), pero el gate —que es el mismo `si` en
        // corchetes— aceptaba cualquier cosa: `[si: x <> 5]` parseaba, y el
        // error salía en RUNTIME, una vez POR EVALUACIÓN. En una opción de
        // `elegir` eso es un renglón en consola; sobre un `evento_random` con
        // `cada=0.05s` son veinte renglones por segundo, para siempre. Medido
        // antes de tocarlo: el corpus tiene **6 gates vivos y los 6 son
        // legales**, así que cerrar la puerta cuesta cero migración.
        //
        // Y un gate roto **mata la línea** (el caller no la emite) en vez de
        // dejarla pasar sin gate: la ley 1a —descarto el valor, no el comando—
        // vale para un parámetro opcional, y un gate no lo es. Si un typo lo
        // desactivara en silencio, el evento dispararía **siempre**, que es
        // justo el lado en el que no querés equivocarte. Es la misma cura que
        // el cuerpo de bloque ya usaba para un corchete que no entiende.
        static GateParse parseConditionModifier(std::string& line, EclipGate& g,
                                                int lineNum, const char* cmdName)
        {
            const bool es = startsWith(line, "[si:");
            const bool en = startsWith(line, "[if:");
            if (!es && !en) return GateParse::Ninguno;

            // G2: el aviso nombra la grafía que el creador ESCRIBIÓ.
            const std::string grafia = es ? "[si:" : "[if:";

            const size_t close = line.find(']');
            if (close == std::string::npos)
            {
                warn(lineNum, std::string(cmdName) + ": el gate " + grafia
                    + " abre corchete y no lo cierra. La forma es "
                    "[si: variable operador valor]. Se ignora esa linea.");
                return GateParse::Malformado;
            }

            const size_t prefixLen = 4;   // "[si:" y "[if:" miden lo mismo
            const std::string cuerpo =
                trim(line.substr(prefixLen, close - prefixLen));

            EclipGate leido;
            std::istringstream iss(cuerpo);
            iss >> leido.var >> leido.op;
            std::string resto;
            std::getline(iss, resto);
            resto = trim(resto);
            if (!resto.empty() && resto[0] == '"')
                extractQuoted(resto, leido.val);
            else
                leido.val = resto;

            if (leido.var.empty() || leido.op.empty() || leido.val.empty())
            {
                warn(lineNum, std::string(cmdName) + ": el gate " + grafia + " "
                    + cuerpo + "] esta incompleto. La forma es "
                    "[si: variable operador valor] (ej: [si: llave == 1]). "
                    "Se ignora esa linea.");
                return GateParse::Malformado;
            }

            if (!esOperadorDeCondicion(leido.op))
            {
                warn(lineNum, std::string(cmdName) + ": el gate " + grafia
                    + " usa el operador '" + leido.op + "', que no existe. "
                    "Los operadores son: == != > < >= <=. Se ignora esa linea.");
                return GateParse::Malformado;
            }

            leido.has = true;
            g = leido;
            line = trim(line.substr(close + 1));
            return GateParse::Ok;
        }

        // Parsea "#RRGGBB" (o "#RRGGBBAA" si el caller PIDE alpha) → canales
        // en [0,1]. Helper publico (lo usa VNScriptState para texto-capa).
        // Local a EclipScript para no acoplar Theme.h por una sola funcion.
        //
        // La ley (R-VN-08 / QUICK-VN — antes esto perdia el RGB con 8 digitos
        // y `std::stoi` consumia prefijos: "#0000FZ" daba azul 15/255 mudo):
        //   · el '#' es opcional (tolerancia historica del motor);
        //   · SEIS digitos exactos, u OCHO **solo si `a != nullptr`**;
        //   · cada caracter tiene que ser un nibble hex — nada de prefijos
        //     parciales, signos ni "0x";
        //   · si algo no cuadra: retorna false y NO toca ningun canal, para
        //     que el caller pueda avisar y conservar su default;
        //   · seis digitos con `a` pedido dejan el alpha INTACTO: el AA es
        //     opcional, y no debe pisar por la espalda la opacidad que ya
        //     tenia el campo (la consola documenta su default en 60%).
        //
        // Un campo SIN alpha rechaza los 8 digitos en vez de ignorar el AA:
        // aceptar en silencio algo que el motor no va a pintar seria mentir.
        //
        // Hermanos con la misma forma (nibble estricto + bool): `MapData.h`
        // (`parseHexColor`, 6|8) y `TextRenderer.cpp` (`parseHex6_local`, 6).
        // `Theme.h::parseColor` sigue con su ley propia (>=6 + formato float
        // 0-1 legacy). Unificar los cuatro es ficha de SYN-1, no de este lote.
        static bool parseHexColor(const std::string& hex,
                                  f32& r, f32& g, f32& b,
                                  f32* a = nullptr)
        {
            std::string h = trim(hex);
            if (!h.empty() && h[0] == '#') h = h.substr(1);

            const bool seis  = (h.size() == 6);
            const bool ocho  = (h.size() == 8) && (a != nullptr);
            if (!seis && !ocho) return false;

            auto nibble = [](char c, int& out) -> bool {
                if (c >= '0' && c <= '9') { out = c - '0';      return true; }
                if (c >= 'a' && c <= 'f') { out = c - 'a' + 10; return true; }
                if (c >= 'A' && c <= 'F') { out = c - 'A' + 10; return true; }
                return false;
            };
            auto byte = [&](size_t pos, int& out) -> bool {
                int hi = 0, lo = 0;
                if (!nibble(h[pos], hi) || !nibble(h[pos + 1], lo)) return false;
                out = hi * 16 + lo;
                return true;
            };

            int ri = 0, gi = 0, bi = 0, ai = 0;
            if (!byte(0, ri) || !byte(2, gi) || !byte(4, bi)) return false;
            if (ocho && !byte(6, ai)) return false;

            // Todo valido: recien AQUI se tocan los canales (fail-closed —
            // un hex a medias no deja medio color escrito).
            r = ri / 255.0f; g = gi / 255.0f; b = bi / 255.0f;
            if (ocho) *a = ai / 255.0f;
            return true;
        }

        // Convierte el escape literal "\n" (backslash+n) en un salto real.
        // Scoped a la CONSOLA (es multi-línea por diseño — un terminal). El
        // resto del motor NO procesa escapes (wrap-only); el escape-processing
        // global es otra ficha. TextRenderer ya rompe línea en '\n' real.
        static std::string applyConsoleEscapes(const std::string& s)
        {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n')
                {
                    out += '\n';
                    ++i;
                }
                else out += s[i];
            }
            return out;
        }

        // ── Punto de entrada principal ──
        //
        // Si manifest != nullptr, sus personajes se siembran en
        // result.characters/compositeChars ANTES de procesar el .ecl.
        // Si el .ecl redeclara uno ya sembrado, se emite aviso pedagógico
        // y la versión del .ecl gana localmente para ese script (Caso 1
        // override local — sesión proyecto.eclips, decisión B).
        //
        // Default nullptr mantiene compatibilidad con todos los call-sites
        // que no tienen acceso al manifest (modo "test rápido sin proyecto").
        static EclipScriptData parse(const std::string& filepath,
            const ProjectManifest* manifest = nullptr)
        {
            EclipScriptData result;

            // ── Sembrar personajes desde el manifest ──────────────────
            // Se hace ANTES de abrir el archivo para que estén disponibles
            // incluso si el parseo falla más adelante.
            if (manifest != nullptr)
            {
                for (const auto& [alias, p] : manifest->personajes)
                {
                    result.characters[alias] = p.path;
                    if (p.compuesto)
                        result.compositeChars.insert(alias);
                    // [Paso 2] Sembrado del manifest → refrescable en F8 hasta
                    // que el .ecl lo overridee con valor distinto.
                    result.charactersFromManifest.insert(alias);
                }
                if (!manifest->personajes.empty())
                {
                    std::cout << "[EclipScript] Personajes sembrados desde manifest: "
                        << manifest->personajes.size() << std::endl;
                }
            }

            std::ifstream file(filepath);
            if (!file.is_open())
            {
                result.error = "No se pudo abrir: " + filepath;
                std::cerr << "[EclipScript] ERROR: " << result.error << std::endl;
                return result;
            }

            // SYN-1c (R-VN-02): el comentario inline se pela ACA, al leer el
            // archivo, y no en el despacho. Este vector tiene CUATRO lectores
            // —el loop principal de abajo y los look-ahead de `elegir`, `qte` y
            // `esperar_click`, que leen `lines[i+1]` crudo—, asi que pelarlo en
            // el loop dejaria a los hijos de un bloque con su comentario puesto:
            // `"Quedarse" -> escena_a  # la segura` guardaria el comentario
            // DENTRO del nombre de la escena destino. Un solo sitio, y los
            // cuatro lectores ven lo mismo.
            //
            // La ley (Opcion B2, por posicion) y por que no la de contenido de
            // `Theme.h`, en el cabezal de `stripInlineComment` (`EclipsParse.h`).
            std::vector<std::string> lines;
            std::string raw;
            while (std::getline(file, raw))
                lines.push_back(stripInlineComment(raw));
            file.close();

            std::cout << "[EclipScript] Parseando '" << filepath
                      << "' (" << lines.size() << " lineas)" << std::endl;

            EclipScene* currentScene = nullptr;
            // SYN-2 (R-VN-06): sumidero para el body de una `escena` duplicada.
            // keep-first: la 2a declaracion no pisa la 1a; sus lineas caen aca y
            // nunca entran a result.scenes (ni se borran ni se fusionan).
            EclipScene discardScene;
            int lineNum = 0;
            bool inCinematica = false;  // CP1: dentro de un bloque 'cinematica {' 
            int  cineOpenLine = 0;      // SYN-3: DONDE abrio (es lo que hay que buscar)
            bool inAmbito = false;      // DDLC-AMBITO-01: dentro de ambito/scope
            int  ambitoOpenLine = 0;

            // ── CP-R3: el bloque de respaldos de `fuente` ───────────────────
            // Estado propio y consultado ARRIBA DE TODO el despacho, para que
            // el `}` que lo cierra no se lo coma la cinematica, el ambito, el
            // `elegir` ni el QTE — la trampa que el prevuelo advertia.
            bool fuenteBloqueAbierta = false;
            bool fuenteBloqueEnSumidero = false;
            int  fuenteBloqueLinea = 0;
            fontdecl::Declaracion fuenteEnCurso;
            EclipCmd fuenteCmdPendiente;

            // Dentro del mismo archivo: repeticion identica = no-op; conflicto
            // = gana la primera valida y la voz cita las dos lineas.
            auto commitFuenteEcl = [&](const fontdecl::Declaracion& d)
            {
                for (const auto& f : result.fuentes)
                {
                    if (f.name != d.name) continue;
                    if (f.mismaFormaQue(d)) return;
                    warn(d.sourceLine, "fuente: '" + d.name +
                        "' ya se declaro en la linea " +
                        std::to_string(f.sourceLine) +
                        " con otra forma. Se conserva la primera.");
                    return;
                }
                result.fuentes.push_back(d);
            };

            // ── C-CP5/E-CP5: recetas nombradas — registro DE PARSE ──────────
            // Vive y muere con el parse: `animar <alias> <nombre>` expande la
            // receta ya RESUELTA dentro del cmd (auto-contenido). Orden de
            // ARCHIVO, como `fuente`: usar antes de definir = "no conozco",
            // con línea (§7.89).
            std::map<std::string, EclipAnimDef> namedAnims;

            // ── E-CP6: ¿el nombre es un efecto Lua DECLARADO? (efecto_lua)
            // Misma ley de orden de archivo que las recetas: declarar antes
            // de usar. La lista vive en result.luaEffects (viaja al runtime,
            // que carga los archivos al boot); esto es solo la consulta.
            auto esLuaFx = [&result](const std::string& n) -> bool
            {
                for (const auto& le : result.luaEffects)
                    if (le.first == n) return true;
                return false;
            };

            // CP0 reloj-dialogo: tercer scope de intensidad, reservado antes
            // de que una receta o un efecto Lua pueda apropiarse del nombre.
            // La línea pública se habilita recién en CP2, con runtime/save.
            auto esIntensidadEfectos = [](const std::string& n) -> bool
            {
                return n == "intensidad_efectos" || n == "effects_intensity";
            };

            // ── E-CP5: el validador de UN param de primitiva — COMPARTIDO ───
            // entre la línea `animar` (E-CP0/CP1/CP3) y las líneas de efecto
            // del bloque `animacion`. Dos validadores habrían derivado en
            // silencio — la enfermedad del harness-réplica, en el parser.
            // Canoniza, valida tipo Y rango (con voz y línea), avisa el costo
            // (n/nx/ny), y guarda UNA grafía. Las claves de DRIVER e
            // `intensidad` NO entran acá: cada caller las gatea ANTES con su
            // propia voz (difieren: `animar` las consume, la receta las
            // rechaza).
            auto fxValidaParam = [&](int ln, const std::string& ctx,
                FxKind fk, const std::string& kCruda, const std::string& vCruda,
                std::map<std::string, std::string>& out)
            {
                const std::string k = canonFxKey(kCruda);

                // E-CP3 reservó dest= para las regiones nombradas; la mesa
                // E-CP6 falló B1: las regiones NO nacen (la transparencia del
                // PNG ya recorta — El Testigo estampa full-canvas). dest=
                // sigue fichado, sin gatillo. Decir "no conozco" seria mentir.
                if (k == "dest" && fk == FxKind::Sustituto)
                {
                    warn(ln, ctx + ": dest= no aplica al sustituto (la transparencia del PNG ya recorta). El "
                        "sustituto tapa el elemento entero.");
                    return;
                }

                if (!fxAcceptsKey(fk, k))
                {
                    warn(ln, ctx + ": no conozco el parametro '" + kCruda +
                        "='. Parametros: " + fxKeysHelp(fk) + ".");
                    return;
                }

                // El TIPO es parte del vocabulario (§2.1): la clave inválida
                // NO se guarda — al runtime solo llegan valores sanos.
                const FxKeyType kt = fxKeyType(k);

                // E-CP3: textura= puede venir entre comillas (§7.41); se
                // pelan — en el cmd (y en el `E;`) viaja UNA grafía, limpia.
                std::string vv = vCruda;
                if (kt == FxKeyType::Textura && vv.size() >= 2 &&
                    vv.front() == '"' && vv.back() == '"')
                    vv = vv.substr(1, vv.size() - 2);

                if (!fxValidValue(kt, vv))
                {
                    warn(ln, ctx + ": " + k + "= espera " + fxKeyTypeName(kt) +
                        ", recibio '" + vCruda + "'. Se usa el valor por "
                        "defecto.");
                    return;
                }

                // E-CP1: el RANGO es parte del vocabulario — el clamp callado
                // es media mentira; acá está la voz. Se GUARDA clampeado.
                if (kt == FxKeyType::Numero && fxKeyHasRange(k))
                {
                    const float v = std::stof(vv);
                    const float vc = fxClampKey(k, v);
                    if (vc != v)
                    {
                        warn(ln, ctx + ": " + k + "=" + vCruda + " esta fuera "
                            "de rango (" + fxKeyRangeName(k) + "). Se usa " +
                            fxNumStr(vc) + ".");
                        out[k] = fxNumStr(vc);   // UNA grafía, también en disco
                        return;
                    }

                    // ── El COSTO: avisar, NO clampear (§11.2.3). Umbral
                    // DERIVADO: 3 (pasadas de franja) x 32 = 96 ≈ 108 (§4).
                    if (k == "n" && v > (float)kFxNAviso)
                        warn(ln, ctx + ": n=" + vCruda + " son muchas "
                            "rebanadas. Con una franja encima son " +
                            std::to_string((int)(v * 3)) + " quads sobre UN "
                            "elemento, y el presupuesto del motor es ~" +
                            std::to_string(kFxPresupuestoQuads) +
                            ". Se respeta igual: vos mandas.");

                    // E-CP3: la grilla crece por PRODUCTO (nx*ny por pasada).
                    if ((k == "nx" || k == "ny") && v > (float)kFxNAviso)
                        warn(ln, ctx + ": " + k + "=" + vCruda + " hace "
                            "crecer la grilla rapido (nx*ny celdas por pasada; "
                            "el presupuesto del motor es ~" +
                            std::to_string(kFxPresupuestoQuads) +
                            " quads). Se respeta igual: vos mandas.");
                }

                out[k] = vv;
            };

            // ── E-CP5: el parse del DRIVER en la línea `animar` — compartido
            // entre el camino primitiva y el camino receta (§1.2: el CUÁNDO
            // viaja en el `animar`, venga lo que venga después). Valida en=
            // (una_vez) y media/varianza/frames/desde (esporadico), con la
            // regla de colisión: una línea, UN driver — gana esporadico.
            auto fxParseDriver = [&](int ln, const std::string& ctx,
                const std::map<std::string, std::string>& kvIn,
                std::map<std::string, std::string>& out,
                float& durEnOut,
                bool& invalidDurationOut) -> bool
            {
                bool hasEsp = false;

                // SYN-4c-ii: el read-twice se fue a `fxTomarPar` — una sola
                // lectura, el choque con voz, y el aviso cita lo escrito.
                const auto parEn = fxTomarPar(kvIn, "en", "in", ln, ctx);
                if (parEn.hay)
                {
                    DurationParseOptions durationOptions;
                    durationOptions.allowBareSeconds = true;
                    durationOptions.allowDialogueAdvance = false;
                    const DurationParseResult parsedDuration =
                        parseDurationSpec(parEn.valor, durationOptions);
                    if (!parsedDuration)
                    {
                        warn(ln, ctx + ": " + parEn.escrita + "='" + parEn.valor +
                            "' no es una duracion valida en segundos: " +
                            durationParseErrorText(parsedDuration.error) +
                            ". Ese comando se ignora.");
                        invalidDurationOut = true;
                    }
                    else
                        durEnOut = parsedDuration.value.amount;
                }

                // esporadico: sus claves, validadas y clampeadas con voz
                // (mismo trato que un param de primitiva).
                for (const auto& e : kvIn)
                {
                    const std::string k = canonFxKey(e.first);
                    if (k != "media" && k != "varianza" &&
                        k != "frames" && k != "desde") continue;

                    if (!fxValidNumero(e.second))
                    {
                        warn(ln, ctx + ": " + k + "='" + e.second +
                            "' no es un numero. Se ignora.");
                        continue;
                    }
                    float v = std::stof(e.second);
                    const float vc = fxClampKey(k, v);
                    if (vc != v)
                    {
                        warn(ln, ctx + ": " + k + "=" + e.second + " esta "
                            "fuera de rango (" + fxKeyRangeName(k) +
                            "). Se usa " + fxNumStr(vc) + ".");
                        v = vc;
                    }
                    out[k] = fxNumStr(v);
                    hasEsp = true;
                }

                if (hasEsp && durEnOut > 0.0f)
                {
                    warn(ln, "animar: en= es del driver una_vez y "
                        "media=/varianza=/frames=/desde= del esporadico "
                        "- una linea, UN driver. Gana esporadico.");
                    durEnOut = 0.0f;
                }
                return hasEsp;
            };

            // E-CP5: la firma de una receta para el eco ("2 canales + 1
            // efecto") — una sola grafía para las dos especies.
            auto descDef = [](const EclipAnimDef& d) -> std::string
            {
                std::string s = "(";
                if (!d.tracks.empty())
                    s += std::to_string(d.tracks.size()) +
                        (d.tracks.size() == 1 ? " canal" : " canales");
                if (!d.tracks.empty() && !d.fx.empty()) s += " + ";
                if (!d.fx.empty())
                    s += std::to_string(d.fx.size()) +
                        (d.fx.size() == 1 ? " efecto" : " efectos");
                return s + ")";
            };

            for (int i = 0; i < (int)lines.size(); ++i)
            {
                lineNum = i + 1;
                std::string line = trim(lines[i]);

                // Ignorar vacías y comentarios. Desde SYN-1c el comentario ya
                // vino pelado al leer el archivo, así que una línea de puro
                // comentario llega VACÍA y la caza el `empty()`; el `[0]=='#'`
                // queda como cinturón (hoy inalcanzable: un `#` en la columna 0
                // siempre corta, el estado de comillas no llega a importar).
                if (line.empty() || line[0] == '#')
                    continue;

                // ── CP-R3: dentro del bloque de respaldos, y ANTES de todo ──
                // Va acá arriba a propósito: si esto estuviera abajo, el `}`
                // que cierra el perfil lo cazaría primero la cinemática, el
                // ámbito, el `elegir` o el QTE, que también leen `}`.
                if (fuenteBloqueAbierta)
                {
                    if (line == "}")
                    {
                        if (!fuenteBloqueEnSumidero)
                        {
                            if (currentScene)
                                currentScene->cmds.push_back(fuenteCmdPendiente);
                            commitFuenteEcl(fuenteEnCurso);
                        }
                        fuenteBloqueAbierta = false;
                        fuenteBloqueEnSumidero = false;
                        fuenteEnCurso = fontdecl::Declaracion{};
                        continue;
                    }

                    std::string rutaRespaldo;
                    bool esPalabraRespaldo = false;
                    if (fontdecl::partirRespaldo(line, rutaRespaldo,
                                                 &esPalabraRespaldo))
                    {
                        // Orden autoral, sin reordenar ni deduplicar: una ruta
                        // repetida conserva su posición (`DA-21`).
                        fuenteEnCurso.routes.push_back(rutaRespaldo);
                    }
                    else if (esPalabraRespaldo)
                    {
                        warn(lineNum, "fuente: 'respaldo' espera UNA ruta entre "
                            "comillas (ej: respaldo \"NotoSans.ttf\"). "
                            "Linea ignorada.");
                    }
                    else
                    {
                        warn(lineNum, "fuente: dentro del bloque de '" +
                            fuenteEnCurso.name + "' solo se escriben lineas "
                            "'respaldo \"ruta.ttf\"' (o 'fallback'). "
                            "Linea ignorada: '" + line + "'.");
                    }
                    continue;
                }

                // ════════════════════════════════════════════════════════════
                // Tabla de despacho (semilla §7.49) — comandos "1 arg con
                // comillas → 1 EclipCmd". El trío fondo/musica/sonido tenía 3
                // bloques byte-idénticos salvo el type; acá colapsan a 3 filas.
                // El offset es kw.size()+1 calculado de la fila — sin el legado
                // `?N:N` copiado. Corre ANTES del if-chain; lo no-matcheado cae
                // al chain viejo intacto (coexistencia = esencia de "semilla").
                // ════════════════════════════════════════════════════════════
                {
                    struct CmdDescriptor {
                        const char* kwEs;   // keyword español
                        const char* kwEn;   // alias inglés
                        EclipCmdType  type;
                    };
                    static const CmdDescriptor kSimpleCmds[] = {
                        { "fondo ",  "background ", EclipCmdType::Fondo  },
                        { "musica ", "music ",      EclipCmdType::Musica },
                        { "sonido ", "sound ",      EclipCmdType::Sonido },
                    };

                    bool matched = false;
                    for (const auto& d : kSimpleCmds)
                    {
                        const char* kw = nullptr;
                        if (startsWith(line, d.kwEs))      kw = d.kwEs;
                        else if (startsWith(line, d.kwEn)) kw = d.kwEn;
                        if (!kw) continue;

                        std::string rest = trim(line.substr(std::strlen(kw)));

                        EclipCmd cmd;
                        cmd.type = d.type;
                        cmd.sourceLine = lineNum;

                        // `fondo` gana params k=v (hoy: tinte=). Se pelan del
                        // rest ANTES de extraer ruta/nombre — tras la 2ª comilla
                        // si la ruta es literal (sus espacios no se tocan), tras
                        // el primer token si es nombre lógico. musica/sonido
                        // siguen 1-arg puro.
                        if (d.type == EclipCmdType::Fondo)
                        {
                            std::string zonaParams;
                            size_t q1 = rest.find('"');
                            size_t q2 = (q1 != std::string::npos)
                                ? rest.find('"', q1 + 1) : std::string::npos;
                            if (q2 != std::string::npos)
                            {
                                zonaParams = rest.substr(q2 + 1);
                                rest = rest.substr(0, q2 + 1);
                            }
                            else
                            {
                                size_t sp = rest.find(' ');
                                if (sp != std::string::npos)
                                {
                                    zonaParams = rest.substr(sp + 1);
                                    rest = rest.substr(0, sp);
                                }
                            }
                            // SYN-1c-ii: `fondo` YA tenia el aviso de clave
                            // desconocida inline — era el modelo, y era el
                            // unico. Ahora lo hereda de `kvParams` como los
                            // otros ocho: una sola voz, una sola ley.
                            auto kv = kvParams(zonaParams, EclipCmdType::Fondo,
                                               lineNum, "fondo");
                            if (kv.count("tinte"))
                            {
                                const std::string& v = kv["tinte"];
                                if (fxValidValue(FxKeyType::Color, v))
                                    cmd.tintHex = v;
                                else
                                    warn(lineNum, "fondo: " + kv.escrita("tinte") +
                                        "= espera un color "
                                        "(#rrggbb), recibio '" + v + "'. Se ignora.");
                            }
                        }

                        std::string path;
                        // extractQuoted devuelve true si había par "..." →
                        // ruta literal. false → nombre lógico (o vacío).
                        cmd.arg1Literal = extractQuoted(rest, path);
                        cmd.arg1 = cmd.arg1Literal ? path : rest;

                        if (!currentScene)
                        {
                            warn(lineNum, std::string(kw) +
                                "fuera de una escena. Ignorado.");
                        }
                        else
                        {
                            currentScene->cmds.push_back(cmd);
                        }
                        matched = true;
                        break;
                    }
                    if (matched) continue;
                }

                // ════════════════════════════════════════════════════════════
                // Tabla de despacho de CAPAS (Unit B, §7.49 / §7.50 / §7.42).
                // Comandos "id + params" — descriptor con handler inyectado
                // (Opción A). El ritual común (match bilingüe, offset por
                // strlen, LA LEY DEL ID, guard de escena) corre UNA vez; el
                // handler aporta solo SUS params.
                // Corre ANTES del if-chain → shadowea los 4 bloques viejos
                // (Pasada 1: coexisten intactos; Pasada 2 los borra).
                //
                // SYN-4a: el campo `QuotePolicy` MURIÓ acá. Repartía a estos
                // ocho entre SoftDeprecate (los 3 viejos, que shippearon con
                // comillas) y RejectHard (capa_fade y los nacidos después) — un
                // reparto CRONOLÓGICO que desde el libreto no se puede predecir.
                // Ahora los ocho pasan por `tomarId`, igual que los otros cuatro
                // comandos con id que ni siquiera estaban en esta tabla.
                // ════════════════════════════════════════════════════════════
                {
                    struct LayerCmd {
                        const char*  kwEs;
                        const char*  kwEn;
                        EclipCmdType type;
                        // Handler: recibe el `rest` DESPUÉS del id ya extraído y
                        // llena SUS params en `cmd`. Lambda sin captura → puntero
                        // a función (puede llamar a los helpers estáticos).
                        bool (*handler)(const std::string& rest,
                                        EclipCmd& cmd, int lineNum);
                    };

                    static const LayerCmd kLayerCmds[] = {
                        // capa_z "id" N → id + 1 entero (z)
                        { "capa_z ", "layer_z ", EclipCmdType::CapaZ,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              // Posicional, no k=v: la ley es la misma, pero
                              // la voz la arma el caller (no hay clave que
                              // nombrar). Antes `capa_z pizarra perro` movía
                              // la capa al z=0 sin decir nada.
                              const std::string tok = trim(rest);
                              if (parseIntStrict(tok, cmd.layerZ) != NumParse::Ok)
                              {
                                  warn(lineNum, "capa_z: espera un entero (sin "
                                      "decimales), recibio '" + tok +
                                      "'. Se ignora.");
                                  cmd.layerZ = 0;
                              }
                              return true;
                          } },
                        // capa_visible "id" si/no → id + bool (parseBool §1.45)
                        { "capa_visible ", "layer_visible ", EclipCmdType::CapaVisible,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              std::string ctx = "capa_visible L" + std::to_string(lineNum);
                              cmd.arg2 = parseBool(trim(rest), false, ctx.c_str()) ? "1" : "0";
                              return true;
                          } },
                        // capa_fade [id] aparecer|desaparecer N → id + dir + float
                        { "capa_fade ", "layer_fade ", EclipCmdType::CapaFade,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              std::string r = trim(rest);
                              size_t sp = r.find(' ');
                              std::string dir = toLower(r.substr(0, sp));
                              std::string dur = (sp != std::string::npos)
                                  ? trim(r.substr(sp + 1)) : "";
                              cmd.arg2 = (dir == "aparecer" || dir == "in" ||
                                          dir == "aparece"  || dir == "entrar")
                                         ? "1" : "0";
                              DurationParseOptions durationOptions;
                              durationOptions.allowBareSeconds = true;
                              durationOptions.allowDialogueAdvance = false;
                              const DurationParseResult parsedDuration =
                                  parseDurationSpec(dur, durationOptions);
                              if (!parsedDuration)
                              {
                                  warn(lineNum, "capa_fade: duracion '" + dur +
                                      "' invalida: " +
                                      durationParseErrorText(parsedDuration.error) +
                                      ". Se esperan segundos; la linea se ignora.");
                                  return false;
                              }
                              cmd.animDuration = parsedDuration.value.amount;
                              return true;
                          } },
                        // capa_mover [id] a (x,y) en N [una_vez|loop] [suave] →
                        // destino + duración + ciclo de vida + easing (CP2).
                        { "capa_mover ", "layer_move ", EclipCmdType::CapaMover,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              std::string r = trim(rest);

                              // Destino (x,y) entre paréntesis.
                              size_t p1 = r.find('(');
                              size_t p2 = (p1 != std::string::npos)
                                  ? r.find(')', p1) : std::string::npos;
                              if (p1 != std::string::npos && p2 != std::string::npos)
                              {
                                  std::string pair = r.substr(p1 + 1, p2 - p1 - 1);
                                  size_t comma = pair.find(',');
                                  // SYN-2: el destino (x,y) es el ARGUMENTO
                                  // requerido de capa_mover, no un campo opcional.
                                  // 1a descartaba la coord basura y el cmd viajaba
                                  // al default (0,0) — el feel-test de 1a, fichado
                                  // aca. Destino incompleto (sin coma, o una coord
                                  // que no parsea) -> no se emite. (1a dejo la
                                  // aridad para este lote a proposito.)
                                  if (comma == std::string::npos)
                                  {
                                      warn(lineNum, "capa_mover: el destino necesita "
                                          "dos coordenadas: (x, y).");
                                      return false;
                                  }
                                  const std::string xs = trim(pair.substr(0, comma));
                                  const std::string ys = trim(pair.substr(comma + 1));
                                  if (parseFloatStrict(xs, cmd.layerX) != NumParse::Ok)
                                  {
                                      warn(lineNum, "capa_mover: destino x invalido: '"
                                          + xs + "'.");
                                      return false;
                                  }
                                  if (parseFloatStrict(ys, cmd.layerY) != NumParse::Ok)
                                  {
                                      warn(lineNum, "capa_mover: destino y invalido: '"
                                          + ys + "'.");
                                      return false;
                                  }
                                  r = trim(r.substr(0, p1) + " " + r.substr(p2 + 1));
                              }
                              else
                              {
                                  // SYN-2: sin destino viajaria a (0,0) (default
                                  // activo, no reposo — el feel-test de 1a).
                                  warn(lineNum, "capa_mover: falta el destino (x,y).");
                                  return false;
                              }

                              // Duración tras 'en'/'in' + ciclo de vida + easing.
                              std::istringstream iss(r);
                              std::string tok; bool grabNext = false;
                              while (iss >> tok)
                              {
                                  std::string low = toLower(tok);
                                  if (grabNext)
                                  {
                                      DurationParseOptions durationOptions;
                                      durationOptions.allowBareSeconds = true;
                                      durationOptions.allowDialogueAdvance = false;
                                      const DurationParseResult parsedDuration =
                                          parseDurationSpec(tok, durationOptions);
                                      if (!parsedDuration)
                                      {
                                          warn(lineNum, "capa_mover: duracion '" +
                                              tok + "' invalida: " +
                                              durationParseErrorText(parsedDuration.error) +
                                              ". Se esperan segundos.");
                                          return false;
                                      }
                                      cmd.animDuration = parsedDuration.value.amount;
                                      grabNext = false;
                                  }
                                  else if (low == "en"   || low == "in")    grabNext = true;
                                  else if (low == "loop" || low == "bucle") cmd.moveLoop = true;
                                  else if (low == "una_vez" || low == "once") cmd.moveLoop = false;
                                  else if (low == "a" || low == "to") { /* preposición de la
                                      // gramática (capa_mover id A (x,y)): ruido sintáctico,
                                      // no un token con sentido. Se ignora a propósito. */
                                  }
                                  else
                                  {
                                      // C-CP1: curva por NOMBRE contra la tabla SSOT.
                                      // Antes, un token desconocido se comía en silencio;
                                      // ahora el motor explica en vez de tolerar callado.
                                      EasingId c = parseEasing(low);
                                      if (c != EasingId::None) cmd.moveCurve = c;
                                      else
                                          warn(lineNum, "capa_mover: no entiendo '" + tok +
                                              "'. Curvas: lineal|suave|salida|resorte|rebote|cubica "
                                              "(linear|smooth|ease_out|spring|bounce_out|cubic).");
                                  }
                              }
                              if (cmd.animDuration <= 0.0f)
                              {
                                  warn(lineNum, "capa_mover: falta 'en <segundos>' (o es 0).");
                                  return false;
                              }
                              if (cmd.moveLoop && cmd.moveCurve != EasingId::Lineal)
                                  warn(lineNum, "capa_mover: loop ignora la curva (siempre lineal).");
                              return true;
                          } },
                        // hotspot [id] x=N y=N w=N h=N → id + 4 named params
                        // (hotspot no tiene split es/en; kwEn duplica kwEs, inocuo)
                        { "hotspot ", "hotspot ", EclipCmdType::HotspotDef,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              auto kv = kvParams(rest, EclipCmdType::HotspotDef,
                                                 lineNum, "hotspot");
                              kvFloat(kv, "x", cmd.layerX, lineNum, "hotspot");
                              kvFloat(kv, "y", cmd.layerY, lineNum, "hotspot");
                              kvFloat(kv, "w", cmd.layerW, lineNum, "hotspot");
                              kvFloat(kv, "h", cmd.layerH, lineNum, "hotspot");
                              return true;
                          } },
                        // enlace [id] [-> destino] → id + target. Sin flecha =
                        // continuar; "-> X" navega/abre shell vía dispatchTarget (CP3).
                        // Nuevo, sin legado → RejectHard (como capa_fade).
                        { "enlace ", "link ", EclipCmdType::Enlace,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              std::string r = trim(rest);

                              // imagen_hover="ruta" / hover_image="ruta" → arg3:
                              // PNG "encendido" del enlace-IMAGEN (swap al hover).
                              // Opcional; ausente → enlace de texto normal (arg3 "").
                              for (const char* key : { "imagen_hover", "hover_image" })
                              {
                              size_t kp = r.find(key);
                              if (kp == std::string::npos) continue;
                              size_t q1 = r.find('"', kp);
                              if (q1 == std::string::npos) break;
                              size_t q2 = r.find('"', q1 + 1);
                              if (q2 == std::string::npos) break;
                              cmd.arg3 = r.substr(q1 + 1, q2 - q1 - 1);   // la ruta
                              // quitar el token para que no estorbe al "->"
                              r = trim(r.substr(0, kp) + " " + r.substr(q2 + 1));
                              break;
                              }

                              // ── LOS PARAMETROS DEL ENLACE VAN ANTES DE LA
                              //    FLECHA ─────────────────────────────────────
                              // `requerido`/`required` (modalidad-aviso) es un
                              // parámetro, y vive donde ya vivía `imagen_hover=`:
                              // la zona de params. Lo que va después de `->` es
                              // el DESTINO, y no lo vuelve a leer nadie.
                              //
                              // Hasta `flag-requerido-mudo` era un sufijo FINAL
                              // pegado al destino, y eso costaba DOS defectos que
                              // salían del mismo `rfind`: puesto antes de la
                              // flecha se ignoraba EN SILENCIO, y desde `CP0`
                              // —que volvió el destino un valor arbitrario— se
                              // comía cualquier valor terminado en la palabra
                              // (`-> ruta = requerido` dejaba `ruta =`, variable
                              // en vacío + modal que nadie pidió). Por eso el
                              // sufijo se retira DURO: un retiro blando conserva
                              // el stripper, y con el stripper vive el segundo.
                              //
                              // Y así las dos formas son UNA ley: el flag va
                              // pegado al id, haya flecha o no.
                              const size_t arrow = r.find("->");
                              {
                                  const std::string zona = (arrow == std::string::npos)
                                      ? r : trim(r.substr(0, arrow));
                                  std::istringstream ps(zona);
                                  std::string tok;
                                  while (ps >> tok)
                                      // FOLD-PARTIDO: la bandera es vocabulario
                                      // del motor y la posicion es PURA (el id
                                      // ya se consumio arriba), asi que
                                      // `Requerido` es la misma palabra.
                                      if (vocabLower(tok) == "requerido" ||
                                          vocabLower(tok) == "required")
                                          cmd.linkRequired = true;
                              }

                              if (arrow != std::string::npos)
                              {
                                  cmd.arg2 = trim(r.substr(arrow + 2)); // destino

                                  // El sufijo viejo, retirado CON VOZ: el ÚLTIMO
                                  // token del destino es el flag, sin comillas.
                                  //
                                  // La primera versión de esta guarda pedía
                                  // EXACTAMENTE dos tokens —para no gritarle a un
                                  // valor que legítimamente termine en la
                                  // palabra— y se midió MUDA justo sobre la única
                                  // línea que existía en el repo
                                  // (`-> ruta = explorar requerido`, el fixture
                                  // del feel-test): la frontera cuidaba un caller
                                  // hipotético y callaba al real. El aviso dice
                                  // la verdad en los dos casos —la palabra SE
                                  // ESTÁ leyendo como parte del destino— y el
                                  // que de verdad quiere el valor literal ya
                                  // tiene su escape: las comillas.
                                  //
                                  // Un destino de UN solo token (`-> requerido`,
                                  // una escena llamada así) no es ambiguo y
                                  // sigue mudo.
                                  std::istringstream ds(cmd.arg2);
                                  std::vector<std::string> toks;
                                  std::string t;
                                  while (ds >> t) toks.push_back(t);
                                  if (toks.size() >= 2 && (toks.back() == "requerido"
                                                        || toks.back() == "required"))
                                  {
                                      const std::string flag = toks.back();
                                      std::string limpio;
                                      for (size_t i = 0; i + 1 < toks.size(); ++i)
                                      {
                                          if (!limpio.empty()) limpio += " ";
                                          limpio += toks[i];
                                      }
                                      // Si sacarle la palabra deja un triple SIN
                                      // VALOR, la linea es genuinamente ambigua
                                      // —querian el flag y les falto el valor, o
                                      // querian el valor literal— y el aviso
                                      // nombra las dos salidas. Sugerir la linea
                                      // "corregida" ahi escupiria un `-> ruta =`
                                      // sin valor, que es peor que no sugerir.
                                      //
                                      // Se pregunta por el VALOR y no por
                                      // `MutaForma::Rota`, y eso se midio: bajo
                                      // `=` un valor vacio es VALIDO (asi se
                                      // vacia una variable, ley de `set`), asi
                                      // que `ruta =` clasifica Valida y la
                                      // pregunta obvia deja el caso mudo.
                                      const eclips::MutaTriple resto =
                                          eclips::mutaClasificarTarget(limpio);
                                      if (resto.forma != MutaForma::NoEsMutacion
                                          && resto.valor.empty())
                                          warn(lineNum, "enlace: '" + flag + "' al "
                                              "final es ambiguo. Como FLAG va antes "
                                              "de la flecha (enlace " + cmd.arg1 +
                                              " " + flag + " -> ...); como VALOR va "
                                              "entre comillas (-> " + limpio +
                                              " \"" + flag + "\"). Aca se lee como "
                                              "parte del destino.");
                                      else
                                          warn(lineNum, "enlace: el flag '" + flag +
                                              "' va ANTES de la flecha; aca se lee "
                                              "como parte del destino. Escribi: enlace " +
                                              cmd.arg1 + " " + flag + " -> " + limpio);
                                  }
                              }
                              return true;
                        } },
                        // control <setting> x=N y=N ancho=N min=N max=N paso=N [estilo=nombre]
                        // → setting-id (arg1) + modelo-valor. Nuevo, sin legado →
                        // RejectHard (como enlace/capa_fade). Render/arrastre/lock
                        // viven en VNScriptState (CP2-4); aquí solo se parsea.
                        { "control ", "control ", EclipCmdType::Control,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              // SYN-1c-ii: la grafia EN llega CANONIZADA desde
                              // `kvParams`, asi que el segundo `kvFloat` de
                              // cada par desaparecio. Ese par era la ley de
                              // desempate accidental de este comando: pisaba
                              // el segundo, o sea ganaba el INGLES — al reves
                              // que `consola crear`, que cortaba en el primero.
                              auto kv = kvParams(rest, EclipCmdType::Control,
                                                 lineNum, "control");
                              kvFloat(kv, "x",     cmd.layerX,       lineNum, "control");
                              kvFloat(kv, "y",     cmd.layerY,       lineNum, "control");
                              kvFloat(kv, "ancho", cmd.controlWidth, lineNum, "control");
                              kvFloat(kv, "min",   cmd.controlMin,   lineNum, "control");
                              kvFloat(kv, "max",   cmd.controlMax,   lineNum, "control");
                              kvFloat(kv, "paso",  cmd.controlStep,  lineNum, "control");
                              if (kv.count("estilo")) cmd.styleName   = kv["estilo"];
                              if (kv.count("tipo"))   cmd.controlType = kv["tipo"];
                              return true;
                        } },
                        // ranura <N> accion=cargar x=N y=N ancho=N [estilo=nombre]
                        // → N (arg1, 1-based) + accion (slotAction). Slot-widget
                        // Tier-A (Ola 2). Render/hit/clic viven en VNScriptState
                        // (CP2-3); aquí solo se parsea. RejectHard como control.
                        { "ranura ", "slot ", EclipCmdType::Ranura,
                          [](const std::string& rest, EclipCmd& cmd, int lineNum) {
                              auto kv = kvParams(rest, EclipCmdType::Ranura,
                                                 lineNum, "ranura");
                              kvFloat(kv, "x",     cmd.layerX,       lineNum, "ranura");
                              kvFloat(kv, "y",     cmd.layerY,       lineNum, "ranura");
                              kvFloat(kv, "ancho", cmd.controlWidth, lineNum, "ranura");
                              // alto: número fijo, o "auto" (mide el stack de la
                              // tarjeta; A3 también es el default en vertical).
                              // "auto" reventaba std::stof y caía a 72 → overflow §7.185.
                              // SYN-1a: `auto` sigue siendo un valor LEGÍTIMO —
                              // la ley del número gobierna la otra rama, no ésta.
                              // (Por eso no puede ser un `kvFloat` pelado: avisaría
                              // de una palabra que el comando sí entiende.)
                              // SYN-1c-ii: `height=auto` entra por acá también —
                              // ya llegó canonizado, así que la rama duplicada se fue.
                              // FOLD-PARTIDO: `auto` es vocabulario del motor y
                              // la ranura es PURA -un numero o `auto`, nada que
                              // pueda depender de un case-, asi que `Auto` es la
                              // misma palabra. Sin esto la guarda de arriba se
                              // cumple al pie de la letra y aun asi produce el
                              // aviso que su comentario juraba evitar.
                              if (kv.count("alto")) {
                                  if (vocabLower(kv["alto"]) == "auto") cmd.autoHeight = true;
                                  else kvFloat(kv, "alto", cmd.layerH, lineNum, "ranura");
                              }
                              if (kv.count("accion")) cmd.slotAction = kv["accion"];
                              if (kv.count("estilo")) cmd.styleName  = kv["estilo"];
                              return true;
                        } },
                    };

                    bool matched = false;
                    for (const auto& d : kLayerCmds)
                    {
                        const char* kw = nullptr;
                        if (startsWith(line, d.kwEs))      kw = d.kwEs;
                        else if (startsWith(line, d.kwEn)) kw = d.kwEn;
                        if (!kw) continue;

                        std::string rest = trim(line.substr(std::strlen(kw)));

                        // ── SYN-4a: UNA sola ley de comillas sobre el id ──────
                        // Acá vivía `QuotePolicy`, que repartía a los ocho
                        // comandos de esta tabla entre "tolero y aviso" y
                        // "rechazo" — y el criterio del reparto era la EDAD del
                        // comando, no su categoría (§7.42 fue el primer rechazo
                        // firmado; lo nacido antes quedó tolerando). Coherente
                        // para quien escribió el parser, invisible desde el
                        // libreto. La ley única vive en `tomarId` (§7.44).
                        std::string id;
                        if (!tomarId(rest, id, trim(kw), lineNum))
                        {
                            matched = true; break;   // matcheó pero se descarta
                        }

                        EclipCmd cmd;
                        cmd.type       = d.type;
                        cmd.arg1       = id;
                        cmd.sourceLine = lineNum;
                        const bool paramsValid =
                            d.handler(rest, cmd, lineNum); // params específicos

                        if (id.empty())
                            warn(lineNum, std::string(kw) + "falta nombre");
                        else if (!paramsValid)
                        {
                            // El handler ya dio una explicación específica.
                        }
                        else if (!currentScene)
                            warn(lineNum, std::string(kw) +
                                "fuera de una escena. Ignorado.");
                        else
                            currentScene->cmds.push_back(cmd);

                        matched = true;
                        break;
                    }
                    if (matched) continue;
                }

                // ── escena nombre / scene name ───────────────────────────
                if (startsWith(line, "escena ") || startsWith(line, "scene "))
                {
                    size_t prefixLen = startsWith(line, "escena ") ? 7 : 6;
                    std::string name = trim(line.substr(prefixLen));
                    if (name.empty())
                    {
                        warn(lineNum, "escena sin nombre, ignorada");
                        continue;
                    }
                    if (result.scenes.count(name))
                    {
                        // SYN-2 (R-VN-06): una 2a `escena X` NO pisa la primera.
                        // keep-first; el body de la duplicada va al sumidero.
                        warn(lineNum, "escena '" + name + "' duplicada; se conserva "
                            "la primera y se ignoran esta y sus lineas.");
                        discardScene.cmds.clear();
                        currentScene = &discardScene;
                        continue;
                    }
                    result.scenes[name] = EclipScene{ name, {} };
                    currentScene = &result.scenes[name];
                    if (result.firstScene.empty())
                        result.firstScene = name;

                    std::cout << "[EclipScript]   Escena: '" << name << "'" << std::endl;
                    continue;
                }

                // ── personaje alias "ruta" / character alias "path" ──────
                // (puede aparecer antes de la primera escena o dentro)
                if (startsWith(line, "personaje ") || startsWith(line, "character "))
                {
                    size_t prefixLen = startsWith(line, "personaje ") ? 10 : 10;
                    std::string rest = trim(line.substr(prefixLen));
                    std::string alias, path;

                    // Extraer alias (primera palabra) y ruta (entre comillas)
                    size_t sp = rest.find(' ');
                    if (sp != std::string::npos)
                    {
                        alias = trim(rest.substr(0, sp));
                        extractQuoted(rest, path);
                    }

                    if (!alias.empty() && !path.empty())
                    {
                        // Aviso si redefine uno sembrado del manifest con valor distinto
                        auto it = result.characters.find(alias);
                        if (it != result.characters.end() && it->second != path)
                        {
                            std::cerr << "[EclipScript] Aviso linea " << lineNum
                                << ": personaje '" << alias << "' redefinido en '"
                                << filepath << "' (era '" << it->second
                                << "', ahora '" << path << "'). "
                                << "Override local OK; revisa si fue intencional."
                                << std::endl;

                            // [Paso 2] Override LOCAL real (valor difiere) → ya
                            // no es del manifest: sale del set. Una redeclaración
                            // IDÉNTICA no entra a este if → NO lo saca.
                            result.charactersFromManifest.erase(alias);
                        }
                        result.characters[alias] = path;
                        std::cout << "[EclipScript]   Personaje: '"
                            << alias << "' -> " << path << std::endl;
                    }
                    else
                    {
                        warn(lineNum, "personaje mal formado: " + line);
                    }

                    // También agregar como cmd si estamos en escena
                    // SYN-2: solo si el personaje es valido; malformado ya aviso.
                    if (currentScene && !alias.empty() && !path.empty())
                    {
                        EclipCmd cmd;
                        cmd.type = EclipCmdType::Personaje;
                        cmd.arg1 = alias;
                        cmd.arg2 = path;
                        cmd.sourceLine = lineNum;
                        currentScene->cmds.push_back(cmd);
                    }
                    continue;
                }

                // ── personaje_compuesto alias "carpeta/" ──────────────────
                // ── composite_character alias "folder/" ──────────────────
                 if (startsWith(line, "personaje_compuesto ") ||
                     startsWith(line, "composite_character "))
                 {
                     size_t prefixLen = startsWith(line, "personaje_compuesto ") ? 20 : 20;
                     std::string rest = trim(line.substr(prefixLen));
                     std::string alias, path;

                     size_t sp = rest.find(' ');
                     if (sp != std::string::npos)
                     {
                         alias = trim(rest.substr(0, sp));
                         extractQuoted(rest, path);
                     }

                     if (!alias.empty() && !path.empty())
                     {
                         // Asegurar que termina en /
                         if (path.back() != '/' && path.back() != '\\')
                             path += '/';

                         // Aviso si redefine uno sembrado del manifest con valor distinto
                         auto it = result.characters.find(alias);
                         if (it != result.characters.end() && it->second != path)
                         {
                             std::cerr << "[EclipScript] Aviso linea " << lineNum
                                 << ": personaje compuesto '" << alias
                                 << "' redefinido en '" << filepath
                                 << "' (era '" << it->second << "', ahora '"
                                 << path << "'). Override local OK; "
                                 << "revisa si fue intencional." << std::endl;
                         }

                         result.characters[alias] = path;
                         result.compositeChars.insert(alias);
                         std::cout << "[EclipScript]   Personaje compuesto: '"
                             << alias << "' -> " << path << std::endl;
                     }
                     else
                     {
                         warn(lineNum, "personaje_compuesto mal formado: " + line);
                     }

                     // SYN-2: solo si el compuesto es valido; malformado ya aviso.
                     if (currentScene && !alias.empty() && !path.empty())
                     {
                         EclipCmd cmd;
                         cmd.type = EclipCmdType::PersonajeCompuesto;
                         cmd.arg1 = alias;
                         cmd.arg2 = path;
                         cmd.sourceLine = lineNum;
                         currentScene->cmds.push_back(cmd);
                     }
                     continue;
                 }




                // ── efecto_lua/lua_effect <nombre> "ruta.lua" (E-CP6) ───────
                // DECLARACIÓN, no acción — la especie de `personaje`/`fuente`/
                // `animacion`: el nombre entra al vocabulario de `animar` en
                // PARSE-time (la voz §7.89 sigue viva para los typos) y el
                // archivo carga al BOOT del estado, no al correr una escena —
                // por eso el registro ya existe cuando un load salta a mitad
                // de escena (fork A1 de la mesa E-CP6). Orden de archivo.
                if (startsWith(line, "efecto_lua ") ||
                    startsWith(line, "lua_effect "))
                {
                    // (los dos prefijos miden 11)
                    std::string restL = trim(line.substr(11));

                    // El nombre SIN comillas (id que el creador inventa); la
                    // ruta CON comillas (literal de archivo). Regla §7.41,
                    // mismo rechazo pedagógico que `fuente`.
                    if (!restL.empty() && restL[0] == '"')
                    {
                        warn(lineNum, "efecto_lua: el nombre va SIN comillas "
                            "(ej: efecto_lua testigo \"fx/testigo.lua\"). Las "
                            "comillas son para la ruta del archivo, no para "
                            "el nombre que inventas.");
                        continue;
                    }

                    std::string nombreL;
                    {
                        size_t sp = restL.find(' ');
                        nombreL = toLower(restL.substr(0, sp));
                        restL = (sp != std::string::npos)
                            ? trim(restL.substr(sp + 1)) : "";
                    }
                    std::string rutaL;
                    extractQuoted(restL, rutaL);

                    if (nombreL.empty() || rutaL.empty())
                    {
                        warn(lineNum, "efecto_lua: falta nombre o ruta (ej: "
                            "efecto_lua testigo \"fx/testigo.lua\").");
                        continue;
                    }

                    if (esIntensidadEfectos(nombreL))
                    {
                        warn(lineNum, "efecto_lua: '" + nombreL + "' esta "
                            "reservado por el motor para la envolvente grupal "
                            "de intensidad del motor). Renombra el efecto.");
                        continue;
                    }

                    // Presets y primitivas son palabras RESERVADAS (§5.1) —
                    // y un nombre no puede vivir en dos especies: `animar`
                    // resuelve por orden y la sombra sería silenciosa.
                    if (parseAnimType(nombreL) != AnimType::None ||
                        parseFxKind(nombreL) != FxKind::None)
                    {
                        warn(lineNum, "efecto_lua: '" + nombreL + "' es del "
                            "motor (preset/primitiva). Renombra el tuyo.");
                        continue;
                    }
                    if (namedAnims.count(nombreL))
                    {
                        warn(lineNum, "efecto_lua: '" + nombreL + "' ya es "
                            "una receta (animacion). Un nombre, una especie.");
                        continue;
                    }

                    // Redeclarar reemplaza la ruta EN SU LUGAR (~ en el eco).
                    bool redeclL = false;
                    for (auto& le : result.luaEffects)
                        if (le.first == nombreL)
                        { le.second = rutaL; redeclL = true; break; }
                    if (!redeclL) result.luaEffects.emplace_back(nombreL, rutaL);

                    std::cout << "[EclipScript]   Efecto Lua "
                        << (redeclL ? "~ '" : "'") << nombreL << "' -> \""
                        << rutaL << "\"" << std::endl;
                    continue;
                }

                // ── animacion/animation <nombre> [canal k=v...] (C-CP5) ─────
                // Línea-basada, sin gramática de bloque (contrato C §5.2):
                // cada línea agrega/reemplaza UN canal de la receta; la línea
                // pelada (solo nombre) declara — o RESETEA (edición iterativa,
                // el ejemplo del contrato de efectos §2). Es DEFINICIÓN, no
                // acción → vive fuera de escena (como `personaje`) y no emite
                // EclipCmd: el registro es del parse y muere con él.
                if (startsWith(line, "animacion ") || startsWith(line, "animation "))
                {
                    // (los dos prefijos miden 10)
                    std::string restN = trim(line.substr(10));

                    std::map<std::string, std::string> kvN;
                    std::vector<std::string> posN;
                    {
                        std::istringstream issN(restN);
                        std::string tokN;
                        while (issN >> tokN)
                        {
                            size_t eq = tokN.find('=');
                            if (eq == std::string::npos) { posN.push_back(tokN); continue; }
                            kvN[toLower(tokN.substr(0, eq))] = tokN.substr(eq + 1);
                        }
                    }

                    if (posN.empty())
                    {
                        warn(lineNum, "animacion: falta el nombre. Forma: "
                            "animacion <nombre> <canal> [de= a= amplitud= en= "
                            "periodo= curva= modo=].");
                        continue;
                    }

                    // El nombre vive en minúsculas: `animar` canoniza su efecto
                    // a lower (cmd.arg2), y la receta tiene que casar ahí.
                    const std::string nombre = toLower(posN[0]);

                    if (esIntensidadEfectos(nombre))
                    {
                        warn(lineNum, "animacion: '" + nombre + "' esta "
                            "reservado por el motor para una linea miembro de "
                            "receta; no puede ser nombre de receta.");
                        continue;
                    }

                    // Presets y primitivas son palabras RESERVADAS (§5.1).
                    if (parseAnimType(nombre) != AnimType::None ||
                        parseFxKind(nombre) != FxKind::None)
                    {
                        warn(lineNum, "animacion: '" + nombre + "' es del motor "
                            "(preset/primitiva). Renombra la tuya.");
                        continue;
                    }
                    // E-CP6: espejo del rechazo de efecto_lua — un nombre no
                    // vive en dos especies (la sombra sería silenciosa).
                    if (esLuaFx(nombre))
                    {
                        warn(lineNum, "animacion: '" + nombre + "' ya es un "
                            "efecto Lua (efecto_lua). Un nombre, una especie.");
                        continue;
                    }

                    // Línea pelada: declara — o resetea a vacía.
                    if (posN.size() == 1 && kvN.empty())
                    {
                        const bool existia = namedAnims.count(nombre) != 0;
                        namedAnims[nombre] = EclipAnimDef{};
                        std::cout << "[EclipScript]   Animacion '" << nombre
                            << (existia ? "' reseteada" : "' declarada")
                            << std::endl;
                        continue;
                    }

                    const std::string canalStr =
                        (posN.size() >= 2) ? toLower(posN[1]) : "";
                    if (posN.size() > 2)
                        warn(lineNum, "animacion: sobra '" + posN[2] +
                            "'. Forma: animacion <nombre> <canal> [k=v...].");

                    if (esIntensidadEfectos(canalStr))
                    {
                        // Reloj-diálogo CP2: la envolvente GRUPAL de intensidad.
                        // Tercer scope (§4): multiplica ctx.intensity de CADA
                        // efecto de la receta por avances de diálogo. Mismo reloj
                        // y leyes que el track de escala (una_vez + DialogueAdvance
                        // entero). Los segundos quedan DIFERIDOS: solo avances.
                        ScalarEnvelope env;
                        bool hasFrom = false, hasTo = false, hasEn = false;
                        bool badEnv = false;

                        auto leerNumEnv = [&](const std::string& v, float& out)
                        {
                            if (!fxValidNumero(v))
                            {
                                warn(lineNum, "animacion " + nombre + ": "
                                    "intensidad_efectos '" + v + "' no es un "
                                    "numero. Esa linea se ignora.");
                                return false;
                            }
                            out = std::stof(v);
                            return true;
                        };

                        for (const auto& e : kvN)
                        {
                            const std::string& k = e.first;
                            const std::string& v = e.second;
                            if (k == "de" || k == "from")
                            { if (!leerNumEnv(v, env.from)) { badEnv = true; break; } hasFrom = true; }
                            else if (k == "a" || k == "to")
                            { if (!leerNumEnv(v, env.to)) { badEnv = true; break; } hasTo = true; }
                            else if (k == "en" || k == "over")
                            {
                                DurationParseOptions o;
                                o.allowBareSeconds = false;
                                o.allowSeconds = false;   // segundos diferidos
                                o.allowDialogueAdvance = true;
                                const DurationParseResult r =
                                    parseDurationSpec(v, o);
                                if (!r)
                                {
                                    warn(lineNum, "animacion " + nombre + ": "
                                        "intensidad_efectos en='" + v + "' "
                                        "invalido: " +
                                        durationParseErrorText(r.error) +
                                        " (solo avances de dialogo). Esa linea "
                                        "se ignora.");
                                    badEnv = true;
                                    break;
                                }
                                env.duration = r.value;
                                hasEn = true;
                            }
                            else if (k == "curva" || k == "curve")
                            {
                                const EasingId c2 = parseEasing(toLower(v));
                                if (c2 == EasingId::None)
                                    warn(lineNum, "animacion " + nombre + ": "
                                        "intensidad_efectos curva='" + v + "' no "
                                        "existe. Se usa lineal.");
                                else env.curve = c2;
                            }
                            else if (k == "modo" || k == "mode")
                            {
                                if (!parseLifecycle(toLower(v), env.mode))
                                    warn(lineNum, "animacion " + nombre + ": "
                                        "intensidad_efectos modo='" + v + "' no "
                                        "existe. Se usa una_vez.");
                            }
                            else
                                warn(lineNum, "animacion " + nombre + ": "
                                    "intensidad_efectos no conoce '" + k + "='. "
                                    "Parametros: de= a= en= curva= modo=.");
                        }
                        if (badEnv) continue;

                        // DialogueAdvance solo admite una_vez (§2): loop/vaiven
                        // sobre avances quedan diferidos.
                        if (env.duration.clock == ClockDomain::DialogueAdvance &&
                            env.mode != Lifecycle::UnaVez)
                        {
                            warn(lineNum, "animacion " + nombre + ": "
                                "intensidad_efectos por avances de dialogo solo "
                                "admite modo=una_vez. Esa linea se ignora.");
                            continue;
                        }
                        if (!hasTo)
                        {
                            warn(lineNum, "animacion " + nombre + ": "
                                "intensidad_efectos necesita a= (destino). Esa "
                                "linea se ignora.");
                            continue;
                        }
                        if (!hasEn)
                        {
                            warn(lineNum, "animacion " + nombre + ": "
                                "intensidad_efectos necesita en= (avances de "
                                "dialogo > 0). Esa linea se ignora.");
                            continue;
                        }
                        // de omitido = identidad del multiplicador (1.0): en k=0
                        // la intensidad no se altera y rampa hacia a=.
                        if (!hasFrom) env.from = 1.0f;
                        env.progress = 0.0f;

                        namedAnims[nombre].fxIntensityEnvelope = env;
                        std::cout << "[EclipScript]   Animacion '" << nombre
                            << "' ~ intensidad_efectos de=" << env.from
                            << " a=" << env.to << " en=" << env.duration.amount
                            << "avances_dialogo curva="
                            << easingToString(env.curve) << " modo="
                            << lifecycleToString(env.mode) << std::endl;
                        continue;
                    }

                    if (canalStr == "tinte" || canalStr == "tint")
                    {
                        warn(lineNum, "animacion " + nombre + ": el canal "
                            "'tinte' aun no esta disponible (tinte animado). Esa linea "
                            "se ignora.");
                        continue;
                    }

                    // ── E-CP5: ¿la línea es una PRIMITIVA de efecto? ─────────
                    // Mismas líneas que en `animar`, dentro del bloque (§2 del
                    // contrato de efectos). `glitch` (preset sellado) también
                    // vale: es un nombre más del vocabulario FX.
                    const FxKind fkLinea = parseFxKind(canalStr);
                    if (fkLinea != FxKind::None)
                    {
                        EclipFxLine fxl;
                        fxl.kind = fkLinea;

                        for (const auto& e : kvN)
                        {
                            const std::string k = canonFxKey(e.first);

                            // intensidad POR LÍNEA: tipada (G4), multiplica
                            // con la universal del `animar` al instanciar.
                            if (k == "intensidad")
                            {
                                if (!fxValidNumero(e.second))
                                {
                                    warn(lineNum, "animacion " + nombre +
                                        ": intensidad='" + e.second + "' no es "
                                        "un numero. Se usa 1.");
                                    continue;
                                }
                                float vI = std::stof(e.second);
                                const float ic = fxClampKey("intensidad", vI);
                                if (ic != vI)
                                {
                                    warn(lineNum, "animacion " + nombre +
                                        ": intensidad=" + e.second + " esta "
                                        "fuera de rango (" +
                                        fxKeyRangeName("intensidad") +
                                        "). Se usa " + fxNumStr(ic) + ".");
                                    vI = ic;
                                }
                                fxl.intensidad = vI;
                                continue;
                            }

                            // El driver NO vive en la receta (§1.2): la receta
                            // dice QUÉ; el guion dice CUÁNDO — en el `animar`.
                            if (fxKeyEsDriver(k))
                            {
                                warn(lineNum, "animacion " + nombre + ": " + k +
                                    "= es del DRIVER, y el driver va en la "
                                    "linea `animar`, no en la receta (la "
                                    "receta dice que; el guion dice cuando). "
                                    "Se ignora.");
                                continue;
                            }

                            fxValidaParam(lineNum,
                                "animacion " + nombre + " (" +
                                fxKindToString(fkLinea) + ")",
                                fkLinea, e.first, e.second, fxl.params);
                        }

                        // Ley E-CP3: sustituto sin textura= no estampa nada.
                        if (fkLinea == FxKind::Sustituto &&
                            !fxl.params.count("textura"))
                        {
                            warn(lineNum, "animacion " + nombre + ": sustituto "
                                "sin textura= (su parametro obligatorio). Esa "
                                "linea se ignora.");
                            continue;
                        }

                        // Re-emitir la MISMA primitiva reemplaza ESA línea (§2).
                        auto& defF = namedAnims[nombre];
                        bool reemplazoF = false;
                        for (auto& f : defF.fx)
                            if (f.kind == fkLinea) { f = fxl; reemplazoF = true; break; }
                        if (!reemplazoF) defF.fx.push_back(fxl);

                        std::cout << "[EclipScript]   Animacion '" << nombre
                            << "' " << (reemplazoF ? "~ " : "+ ")
                            << fxKindToString(fkLinea);
                        for (const auto& p : fxl.params)
                            std::cout << " " << p.first << "=" << p.second;
                        if (fxl.intensidad != 1.0f)
                            std::cout << " intensidad=" << fxl.intensidad;
                        std::cout << " " << descDef(defF) << std::endl;
                        continue;
                    }

                    // ── E-CP6: ¿la línea es un efecto LUA declarado? Entra a
                    // la tubería de la receta como una primitiva más (§5:
                    // tubería unificada). Sus claves son LIBRES — el
                    // vocabulario es del efecto y el motor no lo conoce; lo
                    // que SÍ es del motor: la grafía canónica, las comillas
                    // de ruta (§7.41), el RANGO de las claves de la tabla
                    // común, y el rechazo del driver (la receta dice qué).
                    if (esLuaFx(canalStr))
                    {
                        EclipFxLine fxl;
                        fxl.kind = FxKind::Lua;
                        fxl.luaName = canalStr;

                        for (const auto& e : kvN)
                        {
                            const std::string k = canonFxKey(e.first);

                            if (k == "intensidad")
                            {
                                if (!fxValidNumero(e.second))
                                {
                                    warn(lineNum, "animacion " + nombre +
                                        ": intensidad='" + e.second + "' no es "
                                        "un numero. Se usa 1.");
                                    continue;
                                }
                                float vI = std::stof(e.second);
                                const float ic = fxClampKey("intensidad", vI);
                                if (ic != vI)
                                {
                                    warn(lineNum, "animacion " + nombre +
                                        ": intensidad=" + e.second + " esta "
                                        "fuera de rango (" +
                                        fxKeyRangeName("intensidad") +
                                        "). Se usa " + fxNumStr(ic) + ".");
                                    vI = ic;
                                }
                                fxl.intensidad = vI;
                                continue;
                            }

                            if (fxKeyEsDriver(k))
                            {
                                warn(lineNum, "animacion " + nombre + ": " + k +
                                    "= es del DRIVER, y el driver va en la "
                                    "linea `animar`, no en la receta (la "
                                    "receta dice que; el guion dice cuando). "
                                    "Se ignora.");
                                continue;
                            }

                            std::string vvL = e.second;
                            if (vvL.size() >= 2 && vvL.front() == '"' &&
                                vvL.back() == '"')
                                vvL = vvL.substr(1, vvL.size() - 2);
                            if (fxKeyHasRange(k) && fxValidNumero(vvL))
                            {
                                const float vN = std::stof(vvL);
                                const float vC = fxClampKey(k, vN);
                                if (vC != vN)
                                {
                                    warn(lineNum, "animacion " + nombre + ": " +
                                        k + "=" + e.second + " esta fuera de "
                                        "rango (" + fxKeyRangeName(k) +
                                        "). Se usa " + fxNumStr(vC) + ".");
                                    vvL = fxNumStr(vC);
                                }
                            }
                            fxl.params[k] = vvL;
                        }

                        // Re-emitir el MISMO efecto Lua reemplaza ESA línea
                        // (misma ley §2 que las primitivas).
                        auto& defL = namedAnims[nombre];
                        bool reempL = false;
                        for (auto& fl : defL.fx)
                            if (fl.kind == FxKind::Lua &&
                                fl.luaName == canalStr)
                            { fl = fxl; reempL = true; break; }
                        if (!reempL) defL.fx.push_back(fxl);

                        std::cout << "[EclipScript]   Animacion '" << nombre
                            << "' " << (reempL ? "~ " : "+ ") << canalStr
                            << " (lua)";
                        for (const auto& p : fxl.params)
                            std::cout << " " << p.first << "=" << p.second;
                        if (fxl.intensidad != 1.0f)
                            std::cout << " intensidad=" << fxl.intensidad;
                        std::cout << " " << descDef(defL) << std::endl;
                        continue;
                    }

                    Channel canalT = Channel::OffsetY;
                    if (!parseTrackChannel(canalStr, canalT))
                    {
                        warn(lineNum, "animacion " + nombre + ": no conozco '"
                            + canalStr + "'. Canales: mover_x, mover_y, "
                            "escala, alpha. Efectos: franja, tajadas, "
                            "parpadeo, bloques, estatica, sustituto, glitch - "
                            "o un efecto Lua declarado con efecto_lua.");
                        continue;
                    }

                    // ── Params: tipo Y semántica validados con voz ──────────
                    float de = 0.0f, hasta = 0.0f, amplitud = 0.0f;
                    float en = 0.0f, periodo = 0.0f;
                    bool hasDe = false, hasA = false, hasAmp = false;
                    bool hasEn = false, hasPeriodo = false, hasCurva = false;
                    bool invalidTrackDuration = false;
                    ClockDomain clockEn = ClockDomain::Seconds;
                    EasingId  curvaT = EasingId::Lineal;
                    Lifecycle modoT = Lifecycle::UnaVez;

                    for (const auto& e : kvN)
                    {
                        const std::string& k = e.first;
                        const std::string& val = e.second;

                        auto leerNum = [&](float& out, bool& has)
                        {
                            if (!parseFiniteFloatFull(val, out))
                            {
                                warn(lineNum, "animacion " + nombre + ": " +
                                    k + "='" + val + "' no es un numero finito. "
                                    "Se ignora.");
                                return;
                            }
                            has = true;
                        };

                        auto leerSegundos = [&](float& out, bool& has)
                        {
                            DurationParseOptions durationOptions;
                            durationOptions.allowBareSeconds = true;
                            durationOptions.allowDialogueAdvance = false;
                            const DurationParseResult parsedDuration =
                                parseDurationSpec(val, durationOptions);
                            if (!parsedDuration)
                            {
                                warn(lineNum, "animacion " + nombre + ": " +
                                    k + "='" + val + "' no es una duracion "
                                    "valida en segundos: " +
                                    durationParseErrorText(parsedDuration.error) +
                                    ". Esa linea se ignora.");
                                invalidTrackDuration = true;
                                return;
                            }
                            out = parsedDuration.value.amount;
                            has = true;
                        };

                        auto leerEn = [&](float& out, bool& has)
                        {
                            DurationParseOptions durationOptions;
                            durationOptions.allowBareSeconds = true;
                            durationOptions.allowDialogueAdvance = true;
                            const DurationParseResult parsedDuration =
                                parseDurationSpec(val, durationOptions);
                            if (!parsedDuration)
                            {
                                warn(lineNum, "animacion " + nombre + ": " +
                                    k + "='" + val + "' no es una duracion "
                                    "valida: " +
                                    durationParseErrorText(parsedDuration.error) +
                                    ". Esa linea se ignora.");
                                invalidTrackDuration = true;
                                return;
                            }
                            out = parsedDuration.value.amount;
                            clockEn = parsedDuration.value.clock;
                            has = true;
                        };

                        if      (k == "de" || k == "from")            leerNum(de, hasDe);
                        else if (k == "a" || k == "to")               leerNum(hasta, hasA);
                        else if (k == "amplitud" || k == "amplitude") leerNum(amplitud, hasAmp);
                        else if (k == "en" || k == "over")            leerEn(en, hasEn);
                        else if (k == "periodo" || k == "period")     leerSegundos(periodo, hasPeriodo);
                        else if (k == "curva" || k == "curve")
                        {
                            const EasingId c2 = parseEasing(toLower(val));
                            if (c2 == EasingId::None)
                                warn(lineNum, "animacion " + nombre + ": curva='"
                                    + val + "' no existe. Curvas: lineal, suave, "
                                    "salida, entrada, resorte, rebote, cubica. Se usa "
                                    "lineal.");
                            else { curvaT = c2; hasCurva = true; }
                        }
                        else if (k == "modo" || k == "mode")
                        {
                            if (!parseLifecycle(toLower(val), modoT))
                                warn(lineNum, "animacion " + nombre + ": modo='"
                                    + val + "' no existe. Modos: una_vez, loop, "
                                    "vaiven. Se usa una_vez.");
                        }
                        else
                            warn(lineNum, "animacion " + nombre + ": no conozco "
                                "el parametro '" + k + "='. Parametros: de= a= "
                                "amplitud= en= periodo= curva= modo=.");
                    }

                    // La duración VISTE al modo: una_vez usa en=, loop/vaiven
                    // usan periodo=. Cruzarlas se dice — y sin duración el
                    // track sería una constante disfrazada: se descarta.
                    if (invalidTrackDuration) continue;
                    float durT = 0.0f;
                    if (clockEn == ClockDomain::DialogueAdvance &&
                        modoT != Lifecycle::UnaVez)
                    {
                        warn(lineNum, "animacion " + nombre + ": "
                            "avances_dialogo/dialogue_advances solo admite "
                            "modo=una_vez. Esa linea se ignora.");
                        continue;
                    }
                    if (clockEn == ClockDomain::DialogueAdvance && hasPeriodo)
                    {
                        warn(lineNum, "animacion " + nombre + ": en= por "
                            "avances de dialogo no puede convivir con periodo=. "
                            "Esa linea se ignora.");
                        continue;
                    }
                    if (clockEn == ClockDomain::DialogueAdvance &&
                        canalT != Channel::Scale)
                    {
                        warn(lineNum, "animacion " + nombre + ": avances_dialogo/dialogue_advances solo valen "
                            "en el "
                            "canal escala/scale de personaje. Esa linea se "
                            "ignora.");
                        continue;
                    }
                    if (modoT == Lifecycle::UnaVez)
                    {
                        if (hasPeriodo && !hasEn)
                        {
                            warn(lineNum, "animacion " + nombre + ": periodo= "
                                "es de loop/vaiven (¿falta modo=?). Para "
                                "una_vez usa en=. Esa linea se ignora.");
                            continue;
                        }
                        if (!hasEn || en <= 0.0f)
                        {
                            warn(lineNum, "animacion " + nombre + ": falta en= "
                                "(segundos o avances de dialogo > 0). Esa linea "
                                "se ignora.");
                            continue;
                        }
                        durT = en;
                    }
                    else
                    {
                        if (hasEn && !hasPeriodo)
                        {
                            warn(lineNum, "animacion " + nombre + ": en= es de "
                                "una_vez. Para loop/vaiven usa periodo=. Esa "
                                "linea se ignora.");
                            continue;
                        }
                        if (!hasPeriodo || periodo <= 0.0f)
                        {
                            warn(lineNum, "animacion " + nombre + ": falta "
                                "periodo= (segundos > 0). Esa linea se ignora.");
                            continue;
                        }
                        durT = periodo;
                    }

                    if (hasAmp && (hasDe || hasA))
                    {
                        warn(lineNum, "animacion " + nombre + ": amplitud= es "
                            "azucar de de=/a= - vinieron los dos caminos. "
                            "Ganan de=/a=.");
                        hasAmp = false;
                    }
                    if (hasAmp)
                        amplitudToDeA(canalT, amplitud, de, hasta);
                    else
                    {
                        if (!hasA)
                        {
                            warn(lineNum, "animacion " + nombre + ": falta a= "
                                "(o amplitud=). Esa linea se ignora.");
                            continue;
                        }
                        if (!hasDe) de = channelIdentity(canalT);  // reposo
                    }

                    if (modoT == Lifecycle::Loop && hasCurva)
                    {
                        warn(lineNum, "animacion " + nombre + ": loop es LINEAL "
                            "por ley (un easing pulsaria en cada vuelta). "
                            "curva= se ignora.");
                        // Y se GUARDA lineal: si quedara en el track, el eco y
                        // el `T;` del save listarian una curva que no corre.
                        curvaT = EasingId::Lineal;
                    }
                    if (canalT == Channel::Alpha && curvaT == EasingId::Resorte)
                        warn(lineNum, "animacion " + nombre + ": 'resorte' "
                            "sobrepasa 1.0 y un alpha no puede: se clampa al "
                            "consumirse (meseta en el pico).");

                    AnimTrack trN;
                    trN.canal = canalT;
                    trN.de = de;
                    trN.a = hasta;
                    trN.dur = durT;
                    trN.curva = curvaT;
                    trN.modo = modoT;
                    trN.clock = (modoT == Lifecycle::UnaVez)
                        ? clockEn : ClockDomain::Seconds;

                    // Redefinir el MISMO canal reemplaza ESA línea, en su
                    // lugar (§5.2 — edición iterativa; espejo de emitirEfecto).
                    auto& defN = namedAnims[nombre];
                    bool reemplazo = false;
                    for (auto& t : defN.tracks)
                        if (t.canal == canalT) { t = trN; reemplazo = true; break; }
                    if (!reemplazo) defN.tracks.push_back(trN);

                    // El eco con la grafía CANÓNICA: lo que va a ejecutar.
                    std::cout << "[EclipScript]   Animacion '" << nombre << "' "
                        << (reemplazo ? "~ " : "+ ") << trackChannelToString(canalT)
                        << " de=" << de << " a=" << hasta
                        << (modoT == Lifecycle::UnaVez ? " en=" : " periodo=") << durT
                        << ((trN.clock == ClockDomain::DialogueAdvance)
                            ? "avances_dialogo" : "s")
                        << " curva=" << easingToString(curvaT)
                        << " modo=" << lifecycleToString(modoT)
                        << " " << descDef(defN) << std::endl;
                    continue;
                }

                // A partir de aquí necesitamos estar dentro de una escena
                if (!currentScene)
                {
                    warn(lineNum, "comando fuera de escena, ignorado: " + line);
                    continue;
                }

                // ── mostrar/show alias [expresion] [posicion] ──────────────
                // Formatos:
                //   mostrar Luna              → base, centro
                //   show Luna center          → base, center
                //   mostrar Luna feliz centro → expresión feliz, centro
                if (startsWith(line, "mostrar ") || startsWith(line, "show "))
                {
                    size_t prefixLen = startsWith(line, "mostrar ") ? 8 : 5;
                    std::string rest = trim(line.substr(prefixLen));

                    // Extraer [efecto: X] si presente (lo quita de 'rest')
                    int effect = extractCharEffect(rest, EclipCmdType::Mostrar,
                                                   "mostrar", lineNum);

                    std::istringstream iss(rest);
                    std::string alias, tok2, tok3;
                    iss >> alias >> tok2 >> tok3;

                    std::string expr, posStr;

                    if (tok2.empty())
                    {
                        posStr = "centro";
                    }
                    else if (isPositionWord(tok2))
                    {
                        posStr = tok2;
                    }
                    else
                    {
                        expr = tok2;
                        posStr = tok3.empty() ? "centro" : tok3;
                    }

                    if (alias.empty())    // SYN-2: mostrar sin personaje -> no emite
                    {
                        warn(lineNum, "mostrar: falta el personaje (alias).");
                        continue;
                    }
                    EclipCmd cmd; cmd.type = EclipCmdType::Mostrar;
                    cmd.arg1 = alias;
                    cmd.arg2 = posStr;
                    cmd.arg3 = expr;
                    cmd.charEffect = effect;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── ocultar alias / hide alias ───────────────────────────
                if (startsWith(line, "ocultar ") || startsWith(line, "hide "))
                {
                    size_t prefixLen = startsWith(line, "ocultar ") ? 8 : 5;
                    std::string rest = trim(line.substr(prefixLen));

                    // Extraer [efecto: X] si presente
                    int effect = extractCharEffect(rest, EclipCmdType::Ocultar,
                                                   "ocultar", lineNum);

                    std::string alias = trim(rest);
                    if (alias.empty())    // SYN-2: ocultar sin personaje -> no emite
                    {
                        warn(lineNum, "ocultar: falta el personaje (alias).");
                        continue;
                    }
                    EclipCmd cmd; cmd.type = EclipCmdType::Ocultar;
                    cmd.arg1 = alias;
                    cmd.charEffect = effect;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── parar_musica / stop_music ──────────────────────────
                if (line == "parar_musica" || line == "stop_music")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::PararMusica;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── transicion [tipo] / transition [type] ──────────────
                if (matchKeyword(line, "transicion") || matchKeyword(line, "transition"))
                {
                    size_t prefixLen = startsWith(line, "transicion") ? 10 : 10;
                    // FOLD-PARTIDO: el tipo es vocabulario CERRADO del motor
                    // (fade/dissolve/disolver/slide_*), o sea ranura PURA. Se
                    // pliega ACA y no en `parseTransitionType` porque ese vive
                    // en `VNScriptState.h`, del otro lado de MERGE-DUPLICADO, y
                    // el parse es donde el resto del motor pone sus folds.
                    // Antes, `transicion Dissolve` caia al default `fade` SIN
                    // UNA PALABRA: el creador pedia un disolver y le salia un
                    // fundido.
                    std::string tipo = vocabLower(trim(line.substr(prefixLen)));
                    EclipCmd cmd; cmd.type = EclipCmdType::Transicion;
                    cmd.arg1 = tipo.empty() ? "fade" : tipo;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── ir_a escena / goto scene ─────────────────────────────
                if (startsWith(line, "ir_a ") || startsWith(line, "goto "))
                {
                    size_t prefixLen = startsWith(line, "ir_a ") ? 5 : 5;
                    std::string target = trim(line.substr(prefixLen));
                    EclipCmd cmd; cmd.type = EclipCmdType::IrA;
                    cmd.arg1 = target; cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── lua "script.lua" / ejecutar_lua "script.lua" ──────────
                if (startsWith(line, "lua ") || startsWith(line, "ejecutar_lua "))
                {
                    size_t prefixLen = startsWith(line, "ejecutar_lua ") ? 13 : 4;
                    std::string rest = trim(line.substr(prefixLen));
                    std::string path;
                    extractQuoted(rest, path);

                    EclipCmd cmd; cmd.type = EclipCmdType::Lua;
                    cmd.sourceLine = lineNum;

                    if (!path.empty())
                    {
                        cmd.arg1 = path;
                        std::cout << "[EclipScript]   Lua: '" << path << "'" << std::endl;
                        currentScene->cmds.push_back(cmd); // SYN-2: solo con ruta
                    }
                    else
                    {
                        warn(lineNum, "lua: falta ruta del script");
                    }
                    continue;
                }

                // ── cargar_script "ruta" [escena] / load_script "path" [scene] ──
                if (startsWith(line, "cargar_script ") || startsWith(line, "load_script "))
                {
                    size_t prefixLen = startsWith(line, "cargar_script ") ? 14 : 12;
                    std::string rest = trim(line.substr(prefixLen));

                    // Extraer ruta entre comillas
                    std::string path;
                    extractQuoted(rest, path);

                    // Escena destino opcional (después de las comillas)
                    std::string targetScene;
                    size_t q2 = rest.find('"', rest.find('"') + 1);
                    if (q2 != std::string::npos)
                        targetScene = trim(rest.substr(q2 + 1));

                    EclipCmd cmd; cmd.type = EclipCmdType::CargarScript;
                    cmd.arg1 = path;          // Ruta del nuevo script
                    cmd.arg2 = targetScene;   // Escena destino (vacío = primera)
                    cmd.sourceLine = lineNum;

                    if (path.empty())
                        warn(lineNum, "cargar_script: falta ruta del script");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── fin / end ────────────────────────────────────────────
                if (line == "fin" || line == "end")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::Fin;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── elegir: / choose: ──────────────────────────────────
                if (matchKeyword(line, "elegir") || matchKeyword(line, "choose"))
                {
                    // Extraer parametros opcionales entre [ ]
                    std::string params;
                    size_t bOpen  = line.find('[');
                    size_t bClose = line.find(']');
                    if (bOpen != std::string::npos && bClose != std::string::npos)
                        params = line.substr(bOpen + 1, bClose - bOpen - 1);

                    EclipCmd cmd;
                    if (armarElegir(lines, i, params, lineNum, cmd))
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── qte [tecla: X, tiempo: Xs]: ─────────────────────────────
                //   exito -> escena_exito
                //   fallo -> escena_fallo
                if (matchKeyword(line, "qte"))
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::Qte;
                    cmd.sourceLine = lineNum;

                    // Extraer parámetros entre [ ]
                    size_t bOpen  = line.find('[');
                    size_t bClose = line.find(']');
                    if (bOpen != std::string::npos && bClose != std::string::npos)
                    {
                        std::string params = line.substr(bOpen + 1, bClose - bOpen - 1);
                        // El `dummy` que vivia aca era el `default:` del qte
                        // tirado a la basura, mudo. Ahora la puerta lo rechaza
                        // apuntando a `fallo`, que es su default de verdad.
                        std::string defaultNoUsado;
                        parseBlockParams(params, EclipCmdType::Qte, "qte",
                                         cmd.timerSeconds, defaultNoUsado,
                                         cmd.qteKey, lineNum);
                    }

                    if (cmd.timerSeconds <= 0) cmd.timerSeconds = 3.0f; // default 3s
                    if (cmd.qteKey.empty())    cmd.qteKey = "E";       // default E

                    // Leer exito -> y fallo ->
                    //
                    // SYN-3, dos arreglos en el mismo cuerpo:
                    //
                    //  1. EL CUPO. Era un `for (r < 4)` fijo, y una linea en
                    //     blanco o un comentario GASTABA una vuelta: tres
                    //     comentarios y el `fallo` no se leia nunca. El qte
                    //     avisaba "falta exito o fallo", que es el mensaje
                    //     equivocado para lo que de verdad paso. El `while`
                    //     corta solo, en la primera linea que no es del bloque.
                    //
                    //  2. LA FRONTERA. SYN-1b cablo `matchKeyword` en los
                    //     cuatro DESPACHOS de bloque y no llego a los CUERPOS:
                    //     aca seguia el `startsWith` pelado, asi que
                    //     `exitoso ->` matcheaba `exito`. El pegado se detecta
                    //     comparando las dos leyes —la vieja matchea y la nueva
                    //     no— y por eso el aviso puede ser preciso en vez de
                    //     generico.
                    while (i + 1 < (int)lines.size())
                    {
                        const int childLine = i + 2;
                        std::string next = trim(lines[i + 1]);
                        if (next.empty() || next[0] == '#') { ++i; continue; }

                        const bool esExito = matchKeyword(next, "exito") ||
                                             matchKeyword(next, "success");
                        const bool esFallo = matchKeyword(next, "fallo") ||
                                             matchKeyword(next, "fail");

                        if (esExito || esFallo)
                        {
                            ++i;
                            size_t arrow = next.find("->");
                            if (arrow == std::string::npos)
                                warn(childLine, "qte: '" + next + "' no tiene "
                                    "flecha. La forma es 'exito -> escena' / "
                                    "'fallo -> escena'.");
                            else if (esExito)
                                cmd.qteSuccess = trim(next.substr(arrow + 2));
                            else
                                cmd.qteFail = trim(next.substr(arrow + 2));
                            continue;
                        }

                        const bool pegado =
                            startsWith(next, "exito") || startsWith(next, "success") ||
                            startsWith(next, "fallo") || startsWith(next, "fail");
                        if (pegado)
                        {
                            ++i;
                            warn(childLine, "qte: '" + next + "' no es 'exito' "
                                "ni 'fallo': una palabra pegada no cuenta como "
                                "el keyword. Se ignora esa linea.");
                            continue;
                        }

                        break;    // fin del bloque
                    }

                    if (cmd.qteSuccess.empty() || cmd.qteFail.empty())
                        warn(lineNum, "qte: falta exito o fallo");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── set / mutar · meta_set / meta_mutar ───────────────────
                //
                //   set var = valor       → asignar        (mutar var = valor)
                //   set var = "con espacios"
                //   set var + 1           → incrementar    (mutar var + 1)
                //   set var - 1           → decrementar
                //
                // `mutar-cp0` (F3 de DISENO_MUTAR_PALABRAS) hace DOS cosas acá,
                // y conviene leerlas separadas:
                //
                // (1) NACEN LOS GEMELOS ESPAÑOLES. `set` y `meta_set` eran los
                //     únicos comandos-verbo del dialecto `.ecl` sin grafía ES
                //     -el hueco `A3` del contrato-, contra la ley medida de que
                //     los comandos SÍ se traducen (`D-32`: se traduce lo
                //     cotidiano, se deja la jerga; un comando no es jerga).
                //
                // (2) MUERE EL TODO DE §7.49, que esta misma rama pedía por
                //     escrito: *"con el refactor a tabla de despacho, set y
                //     meta_set compartirán el parse de var/op/val"*. El reparto
                //     vive ahora en `eclips::mutaTokenizar` (VNMutateTarget.h),
                //     que es el MISMO punto que consulta la rama de mutación de
                //     `dispatchTarget`. Sin eso, la forma `-> var op val` sería
                //     una CUARTA copia del mismo tokenizador.
                //
                // Lo que NO se unificó, y es a propósito: la rama de `si`
                // (abajo) tiene este mismo cuerpo con la allowlist de
                // COMPARACIÓN. Adoptar el tokenizador ahí es una línea, pero es
                // la rama más caliente del corpus y no es de este contrato →
                // ficha `TRIPLE-EN-SI`.
                //
                // La VOZ se arma con la grafía que el creador escribió, nunca
                // con la del gemelo: es la ley de SYN-4c-ii que custodia
                // `harness_grafia`. Los dos textos quedan byte-idénticos a los
                // de antes de este lote — incluida la asimetría de que la
                // familia `meta_` no imprime el `Recibi:`.
                {
                    const char* kw    = nullptr;
                    size_t      kwLen = 0;
                    bool        esMeta = false;

                    if      (startsWith(line, "set "))        { kw = "set";        kwLen = 4;  }
                    else if (startsWith(line, "mutar "))      { kw = "mutar";      kwLen = 6;  }
                    else if (startsWith(line, "meta_set "))   { kw = "meta_set";   kwLen = 9;  esMeta = true; }
                    else if (startsWith(line, "meta_mutar ")) { kw = "meta_mutar"; kwLen = 11; esMeta = true; }

                    if (kw)
                    {
                        const std::string rest = trim(line.substr(kwLen));
                        const eclips::MutaTokens t = eclips::mutaTokenizar(rest);

                        // SYN-2: aridad + operador (allowlist medida vs corpus).
                        //   var = valor      (=, el valor puede ser cadena vacia)
                        //   var + n | - n    (+/- exigen operando)
                        if (!eclips::mutaAridadOk(t))
                        {
                            const std::string g = kw;
                            std::string voz = g + ": forma invalida. Usa '" + g +
                                " var = valor', '" + g + " var + n' o '" + g +
                                " var - n'.";
                            if (!esMeta) voz += " Recibi: " + rest;
                            warn(lineNum, voz);
                            continue;
                        }

                        EclipCmd cmd;
                        cmd.type = esMeta ? EclipCmdType::MetaSet : EclipCmdType::Set;
                        cmd.arg1 = t.var;    // Nombre de la variable
                        cmd.arg2 = t.op;     // "=" | "+" | "-"
                        cmd.arg3 = t.valor;  // Valor
                        cmd.sourceLine = lineNum;
                        currentScene->cmds.push_back(cmd);
                        continue;
                    }
                }

                // ── meta_borrar / meta_reset → vaciar meta.eclips ─────────
                // Único camino a resetear el progreso permanente (borrar_saves
                // NO lo toca). Bilingüe.
                if (line == "meta_borrar" || line == "meta_reset")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::MetaClear;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── si variable operador valor / if var op val ──────────
                if (startsWith(line, "si ") || startsWith(line, "if "))
                {
                    size_t prefixLen = startsWith(line, "si ") ? 3 : 3;
                    std::string rest = trim(line.substr(prefixLen));
                    std::istringstream iss(rest);
                    std::string varName, op, val;
                    iss >> varName >> op;
                    std::string remainder;
                    std::getline(iss, remainder);
                    remainder = trim(remainder);

                    if (!remainder.empty() && remainder[0] == '"')
                        extractQuoted(remainder, val);
                    else
                        val = remainder;

                    // SYN-2: aridad + operador de comparacion (allowlist).
                    const bool siOk = !varName.empty() && !val.empty() &&
                        (op == "==" || op == "!=" || op == ">" || op == "<" ||
                         op == ">=" || op == "<=");
                    if (!siOk)
                    {
                        warn(lineNum, "si: forma invalida. Usa 'si var == valor' "
                            "(operadores: == != > < >= <=).");
                        continue;
                    }
                    EclipCmd cmd; cmd.type = EclipCmdType::Si;
                    cmd.arg1 = varName;  // Variable a evaluar
                    cmd.arg2 = op;       // "==" | "!=" | ">" | "<" | ">=" | "<="
                    cmd.arg3 = val;      // Valor de comparación
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── sino / else ───────────────────────────────────────────
                if (line == "sino" || line == "else")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::Sino;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── fin_si / end_if ───────────────────────────────────────
                if (line == "fin_si" || line == "end_if")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::FinSi;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── capa "nombre" "ruta" z=N x=N y=N w=N h=N ──────────
                // ── layer "name" "path" z=N x=N y=N w=N h=N ──────────
                if (startsWith(line, "capa ") || startsWith(line, "layer "))
                {
                    size_t prefixLen = startsWith(line, "capa ") ? 5 : 6;
                    std::string rest = trim(line.substr(prefixLen));

                    // ── Nombre: id-sin-comillas (§7.44, puerta única) ──────
                    // SYN-4a: acá vivía la deprecación suave ("acepta + avisa
                    // que migre"). Era la más cara de las cuatro leyes viejas:
                    // 20 de las 64 líneas del corpus colgaban de ella, y el
                    // manual la enseñaba en su propia FIRMA (`capa/layer "nombre"
                    // "ruta" ...`). La deuda quedó cobrada; la ley es `tomarId`.
                    std::string name;
                    if (!tomarId(rest, name, "capa", lineNum)) continue;

                    // Extraer la ruta (siempre entre comillas — es literal).
                    std::string path;
                    size_t pq1 = rest.find('"');
                    size_t pq2 = (pq1 != std::string::npos) ? rest.find('"', pq1 + 1) : std::string::npos;
                    if (pq1 != std::string::npos && pq2 != std::string::npos)
                    {
                        path = rest.substr(pq1 + 1, pq2 - pq1 - 1);
                        rest = trim(rest.substr(pq2 + 1));
                    }

                    // ── COLOR-SOLIDO: el slot de la ruta, con un color ──────
                    // El discriminador es el `#` INICIAL, no "contiene `#`":
                    // un archivo puede llamarse `raro#2.png` y tiene que
                    // seguir cargando. La comilla es la que hace esto posible
                    // — sin ella, `#FF0000` es COMENTARIO (ley de
                    // `harness_comentarios`), y por eso esta forma vive dentro
                    // del literal y no como token pelado.
                    //
                    // Si empieza con `#`, el creador quiso un COLOR. Un hex
                    // roto NO cae a "ruta": no existe ningun archivo llamado
                    // `#GG0000`, y avisar de una textura que no carga en vez
                    // del color mal escrito seria la sintaxis mintiendo.
                    // Fail-closed con voz, en los terminos correctos.
                    bool esColor = false;
                    if (!path.empty() && path[0] == '#')
                    {
                        f32 cr, cg, cb, ca;
                        if (parseHexColor(path, cr, cg, cb, &ca))
                            esColor = true;
                        else
                        {
                            warn(lineNum, "capa: '" + path + "' no es un color "
                                 "valido (usa #RRGGBB o #RRGGBBAA). "
                                 "La linea se ignora.");
                            continue;
                        }
                    }

                    // Parsear key=value params
                    auto kv = kvParams(rest, EclipCmdType::Capa, lineNum, "capa");

                    EclipCmd cmd; cmd.type = EclipCmdType::Capa;
                    cmd.arg1 = name;
                    // Excluyentes POR CONSTRUCCION: si es color, `arg2` queda
                    // vacio, asi que ningun lector de abajo puede intentar
                    // cargar una textura llamada "#000000" y fallar mudo.
                    if (esColor) cmd.layerColorHex = path;
                    else         cmd.arg2          = path;
                    cmd.sourceLine = lineNum;

                    // RT-1: el bool que estos ya devolvian desde SYN-1a ("la
                    // clave estaba y se leyo bien") deja de tirarse al piso.
                    // Ojo `parallax`: era el UNICO campo que hoy se preserva en
                    // UPDATE, y lo hacia comparando contra 0 — por eso
                    // `parallax=0` no podia apagarlo. Con presencia real, el
                    // cero escrito es una orden como cualquier otra.
                    cmd.mark(EclipField::X,        kvFloat(kv, "x",        cmd.layerX,         lineNum, "capa"));
                    cmd.mark(EclipField::Y,        kvFloat(kv, "y",        cmd.layerY,         lineNum, "capa"));
                    cmd.mark(EclipField::W,        kvFloat(kv, "w",        cmd.layerW,         lineNum, "capa"));
                    cmd.mark(EclipField::H,        kvFloat(kv, "h",        cmd.layerH,         lineNum, "capa"));
                    cmd.mark(EclipField::Z,        kvInt  (kv, "z",        cmd.layerZ,         lineNum, "capa"));
                    cmd.mark(EclipField::Parallax, kvFloat(kv, "parallax", cmd.parallaxFactor, lineNum, "capa"));

                    // Tinte estático (micro-lote): mismo vocabulario de color
                    // que los fx (#rrggbb, validado en parse-time con línea).
                    if (kv.count("tinte"))
                    {
                        const std::string& v = kv["tinte"];
                        if (fxValidValue(FxKeyType::Color, v))
                        {
                            cmd.tintHex = v;
                            cmd.mark(EclipField::Tinte);   // RT-1
                        }
                        else
                            warn(lineNum, "capa: " + kv.escrita("tinte") +
                                "= espera un color (#rrggbb), "
                                "recibio '" + v + "'. Se ignora.");
                    }

                    if (name.empty() || path.empty())
                        warn(lineNum, "capa: falta nombre o ruta");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── fuente nombre "ruta" tamaño=N ──────────────────────
                // ── font   name   "path" size=N ───────────────────────
                // Declara una fuente nombrada (un atlas (ttf,tamaño)).
                // Comando EJECUTABLE: carga el atlas en fase GL-segura, no
                // en parse-time (§1.34). El nombre va SIN comillas (es un id
                // que el creador inventa); la ruta CON comillas (literal de
                // archivo, puede tener espacios). Regla de comillas §7.41:
                // literal/ruta → comillas, token/id/param → sin.
                if (startsWith(line, "fuente ") || startsWith(line, "font "))
                {
                    size_t prefixLen = startsWith(line, "fuente ") ? 7 : 5;
                    std::string rest = trim(line.substr(prefixLen));

                    // El nombre NO lleva comillas. Si las trae, rechazo
                    // pedagogico (coherente con capa_fade, regla §7.10/§7.42).
                    if (!rest.empty() && rest[0] == '"')
                    {
                        warn(lineNum, fontdecl::cuerpoError(
                            fontdecl::Error::NombreConComillas));
                        continue;
                    }

                    // nombre = primer token (hasta el espacio)
                    std::string name;
                    {
                        size_t sp = rest.find(' ');
                        name = rest.substr(0, sp);
                        rest = (sp != std::string::npos)
                            ? trim(rest.substr(sp + 1)) : "";
                    }

                    // ruta = lo que va entre comillas
                    std::string path;
                    extractQuoted(rest, path);

                    // params key=value despues del string cerrado
                    size_t q2 = rest.find('"', rest.find('"') + 1);
                    std::string paramStr = (q2 != std::string::npos)
                        ? trim(rest.substr(q2 + 1)) : "";

                    // CP-R3: un `{` FINAL abre la cadena de respaldos. La ley
                    // del `{` es la misma que la del manifest (`pelarLlave`),
                    // asi que una llave con cola es invalida en los dos.
                    bool abreBloque = false;
                    {
                        const fontdecl::Error errLlave =
                            fontdecl::pelarLlave(paramStr, abreBloque);
                        if (errLlave != fontdecl::Error::Ninguno)
                        {
                            warn(lineNum, fontdecl::cuerpoError(errLlave));
                            continue;
                        }
                    }

                    auto kv = kvParams(paramStr, EclipCmdType::Fuente,
                                       lineNum, "fuente");

                    // ── LA VALIDACION YA NO VIVE ACA (CP-R3) ───────────────
                    // Los defaults, la requeridad del tamaño y los CUERPOS de
                    // los avisos salen de `fontdecl`, que es lo mismo que
                    // consume el catalogo de `proyecto.eclips`.
                    //
                    // Lo que este dialecto CONSERVA es su lectura de valores, y
                    // no por comodidad: `kvFloat` nombra en el aviso LA GRAFIA
                    // QUE EL CREADOR ESCRIBIO (`size=`, `letter_spacing=`, la
                    // enie de verdad), que es la ley G2 de SYN-4c-ii. Sacarla
                    // puso rojos tres asserts de `harness_grafia` y uno de
                    // `harness_enie` —la frontera 1a/SYN-2— en la primera
                    // corrida de la bateria de este CP: el cuerpo canonico dice
                    // QUE esta mal, y esta voz dice CUAL palabra corregir.
                    f32 sz = 0.0f;
                    kvFloat(kv, fontdecl::claveTamano(), sz, lineNum, "fuente");
                    f32 kern = 0.0f;
                    kvFloat(kv, fontdecl::claveEspaciado(), kern, lineNum,
                            "fuente");

                    FontMeasure medida = FontMeasure::Height;
                    bool medidaEscrita = false;
                    if (kv.count(fontdecl::claveMedida()))
                    {
                        if (!parseFontMeasure(kv[fontdecl::claveMedida()],
                                              medida))
                        {
                            warn(lineNum, fontdecl::cuerpoError(
                                fontdecl::Error::MedidaInvalida,
                                kv[fontdecl::claveMedida()]));
                            continue;
                        }
                        medidaEscrita = true;
                    }

                    fontdecl::Declaracion decl;
                    const fontdecl::Error err =
                        fontdecl::construirDesdeValores(name, path, sz, kern,
                                                        medida, medidaEscrita,
                                                        decl);
                    if (err != fontdecl::Error::Ninguno)
                    {
                        warn(lineNum, fontdecl::cuerpoError(err));
                        continue;
                    }
                    decl.sourceFile = filepath;
                    decl.sourceLine = lineNum;

                    EclipCmd cmd; cmd.type = EclipCmdType::Fuente;
                    cmd.fontName = name;
                    cmd.arg1 = path;        // ruta del .ttf
                    cmd.sourceLine = lineNum;
                    cmd.fontSize = decl.size;
                    cmd.fontKerning = decl.kerning;
                    cmd.fontMeasure = decl.measure;
                    cmd.fontMeasureSpecified = decl.measureSpecified;

                    // SYN-2: el cuerpo de una `escena` duplicada va al
                    // sumidero, y una declaracion de fuente escrita ahi NO es
                    // "un atlas de mas": es un nombre que una escena VALIDA
                    // podria resolver, declarado en lineas que el parser acaba
                    // de decir que ignora. El bloque igual se ABRE, para que su
                    // `}` no se lo coma otra gramatica; lo que no ocurre es el
                    // commit.
                    const bool enSumidero = (currentScene == &discardScene);

                    if (abreBloque)
                    {
                        fuenteBloqueAbierta = true;
                        fuenteBloqueLinea = lineNum;
                        fuenteEnCurso = decl;
                        fuenteCmdPendiente = cmd;
                        fuenteBloqueEnSumidero = enSumidero;
                    }
                    else if (!enSumidero)
                    {
                        currentScene->cmds.push_back(cmd);
                        commitFuenteEcl(decl);
                    }
                    continue;
                }

                // ── texto [id] "contenido" x=N y=N color=#RGB z=N ──────
                // ── text  [id] "content"   x=N y=N color=#RGB z=N ──────
                // Heuristica: si el resto empieza con comilla → no hay id.
                //             si empieza con palabra → es el id.
                if (startsWith(line, "texto ") || startsWith(line, "text "))
                {
                    size_t prefixLen = startsWith(line, "texto ") ? 6 : 5;
                    std::string rest = trim(line.substr(prefixLen));

                    // Decidir id por el primer caracter
                    std::string id;
                    if (!rest.empty() && rest[0] != '"')
                    {
                        // Primer token (hasta el espacio) = id
                        size_t sp = rest.find(' ');
                        id   = rest.substr(0, sp);
                        rest = (sp != std::string::npos)
                            ? trim(rest.substr(sp + 1)) : "";
                    }
                    else
                    {
                        // Empieza con comilla → la forma SIN id, que es válida:
                        // un solo literal, el contenido, y el id se autogenera.
                        // Eso es §7.44 funcionando bien, no la grieta.
                        //
                        // SYN-4a: pero si hay DOS literales, el creador escribió
                        // `texto "id" "contenido"` — un id CITADO. Hasta este
                        // lote el segundo literal se caía MUDO (un token sin `=`
                        // se descarta sin voz en `parseKeyValueParams`) y el
                        // motor dibujaba el ID en pantalla. Salida equivocada,
                        // cero avisos: de los cuatro casos, el peor.
                        const size_t qA = rest.find('"');
                        const size_t qB = (qA != std::string::npos)
                            ? rest.find('"', qA + 1) : std::string::npos;
                        if (qB != std::string::npos)
                        {
                            const std::string cola = trim(rest.substr(qB + 1));
                            if (!cola.empty() && cola[0] == '"')
                            {
                                warn(lineNum, "texto: el id va SIN comillas (ej: "
                                    "texto nombre \"contenido\"). Las comillas son "
                                    "para texto que se ve o rutas de archivo, no "
                                    "para nombres.");
                                continue;
                            }
                        }
                    }

                    // Contenido = lo que va entre comillas
                    std::string content;
                    extractQuoted(rest, content);

                    // Params key=value despues del string cerrado
                    size_t q2 = rest.find('"', rest.find('"') + 1);
                    std::string paramStr = (q2 != std::string::npos)
                        ? trim(rest.substr(q2 + 1)) : "";
                    auto kv = kvParams(paramStr, EclipCmdType::Texto,
                                       lineNum, "texto");

                    EclipCmd cmd; cmd.type = EclipCmdType::Texto;
                    cmd.arg1 = id;        // vacio si no hay id (se autogenera al ejecutar)
                    cmd.arg2 = content;   // lo que se dibuja
                    cmd.sourceLine = lineNum;

                    // RT-1: presencia. Es el mismo bool de SYN-1a; ver EclipField.
                    cmd.mark(EclipField::X, kvFloat(kv, "x", cmd.layerX, lineNum, "texto"));
                    cmd.mark(EclipField::Y, kvFloat(kv, "y", cmd.layerY, lineNum, "texto"));
                    cmd.mark(EclipField::Z, kvInt  (kv, "z", cmd.layerZ, lineNum, "texto"));
                    if (kv.count("color")) { cmd.arg3 = kv["color"]; cmd.mark(EclipField::Color); }  // hex, parseado al ejecutar
                    if (kv.count("fuente")) { cmd.fontName = kv["fuente"]; cmd.mark(EclipField::Fuente); }  // fuente nombrada (opcional)
                    // Estilo nombrado (opcional): paquete declarado en proyecto.eclips.
                    // Solo capturamos el nombre; el merge per-propiedad vive en cmdTexto.
                    if (kv.count("estilo")) { cmd.styleName = kv["estilo"]; cmd.mark(EclipField::Estilo); }
                    // Glow: bilingue brillo_*/glow_*. SYN-1c-ii las declaró en
                    // la tabla y las hizo llegar acá como una sola; SYN-4c-ii
                    // RETIRÓ las dos mezcladas (`glow_radio`/`glow_intensidad`,
                    // prefijo EN + sufijo ES), así que los pares son limpios.
                    if (kv.count("brillo_color")) { cmd.glowColorHex = kv["brillo_color"]; cmd.mark(EclipField::GlowColor); }
                    cmd.mark(EclipField::GlowRadio,
                             kvFloat(kv, "brillo_radio", cmd.glowRadius, lineNum, "texto"));
                    // Intensidad: acepta "60%" o "0.6". Normalizamos a [0..1].
                    // SYN-1a: el `%` es sintaxis LEGÍTIMA y documentada — se le
                    // quita y la ley del número gobierna el resto. Antes `60%abc`
                    // valía 0.6 (el prefijo colaba) y `perro` se ignoraba mudo.
                    // RT-1: devuelve si el valor entro (para marcar presencia).
                    // Un `%` mal escrito NO marca: 1a descarta el valor, y con
                    // presencia eso significa "el campo queda como estaba".
                    // SYN-4c-ii: `key` es la grafia ESCRITA, no la canonica —
                    // este lector cita la clave a mano, asi que la ley de G2
                    // entra por su parametro en vez de por `kvFloat`.
                    auto parseIntensity = [lineNum](const std::string& raw,
                                                    const std::string& key, f32& out) -> bool {
                        std::string s = trim(raw);
                        const bool pct = (!s.empty() && s.back() == '%');
                        if (pct) s = trim(s.substr(0, s.size() - 1));

                        float v = 0.0f;
                        if (parseFloatStrict(s, v) != NumParse::Ok)
                        {
                            warn(lineNum, std::string("texto: ") + key +
                                "= espera un numero o un porcentaje (ej: 0.6 o "
                                "60%), recibio '" + raw + "'. Se ignora.");
                            return false;
                        }
                        out = pct ? v / 100.0f : v;
                        return true;
                        };
                    if (kv.count("brillo_intensidad"))
                        cmd.mark(EclipField::GlowInten,
                                 parseIntensity(kv["brillo_intensidad"],
                                                kv.escrita("brillo_intensidad"),
                                                cmd.glowIntensity));

                    // ── Borde (OUTLINE-TEXTO-CAPA) ──────────────────────────
                    // El grosor entra por `kvFloat`, o sea que hereda gratis la
                    // ley del numero (consumo total, finitud) y la de la grafia
                    // (el aviso cita la palabra ESCRITA, no la canonica).
                    cmd.mark(EclipField::BordeGrosor,
                             kvFloat(kv, "borde_grosor", cmd.bordeGrosor,
                                     lineNum, "texto"));

                    // El color se VALIDA aca, no al ejecutar, y esa es una
                    // diferencia deliberada con `color=` (que viaja crudo y, si
                    // esta roto, sale blanco en silencio). El precedente es
                    // COLOR-SOLIDO: si el hex esta mal, el creador se entera en
                    // los terminos correctos — "no es un color valido" — y no
                    // por un borde que simplemente no aparece. Fail-closed: un
                    // hex roto NO deja medio borde puesto.
                    if (kv.count("borde_color"))
                    {
                        const std::string hex = kv["borde_color"];
                        f32 br, bg, bb, ba;
                        if (parseHexColor(hex, br, bg, bb, &ba))
                        {
                            cmd.bordeColorHex = hex;
                            cmd.mark(EclipField::BordeColor);
                        }
                        else
                        {
                            warn(lineNum, "texto: " + kv.escrita("borde_color") +
                                 "= '" + hex + "' no es un color valido "
                                 "(usa #RRGGBB o #RRGGBBAA). Se ignora.");
                        }
                    }

                    // FORK 3a — borde y glow son EXCLUYENTES, gana el borde.
                    // Se avisa ACA, en parse-time, cuando el choque es visible
                    // en UNA linea (el creador tiene el archivo delante). El
                    // choque que involucra al estilo no se puede ver hasta el
                    // merge, y lo resuelve `makeTextPatch` con la misma regla.
                    //
                    // Por que es una LEY y no un detalle: no existe una llamada
                    // que dibuje los dos (`SceneLayer.h` elige entre
                    // `drawTextGlow_noBatch` y `drawText_noBatch`). Sin esta
                    // regla, cual de los dos sobrevive dependeria del orden de
                    // un `if` — un fallo mudo.
                    {
                        const bool pidioBorde = cmd.has(EclipField::BordeColor) ||
                                                cmd.has(EclipField::BordeGrosor);
                        const bool pidioGlow  = cmd.has(EclipField::GlowColor) ||
                                                cmd.has(EclipField::GlowRadio) ||
                                                cmd.has(EclipField::GlowInten);
                        if (pidioBorde && pidioGlow)
                            warn(lineNum, "texto: el borde y el brillo no se "
                                 "pueden usar juntos en la misma linea. Se usa "
                                 "el borde.");
                    }

                    if (content.empty())
                        warn(lineNum, "texto: falta el contenido entre comillas");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── entrada <var> "prompt" / input <var> "prompt" ──────
                // Captura de texto del jugador (B2). El juego muestra el
                // prompt, el jugador escribe, confirma, y el texto se guarda
                // en <var> (se lee luego con ${var}). La variable SIEMPRE
                // queda definida tras un 'entrada' (Esc/vacio -> default o
                // ""), asi ${var} nunca explota.
                //
                // <var> SIN comillas (es un id, como el nombre de 'fuente').
                // prompt CON comillas (literal que el jugador lee). §7.44.
                //
                // P1: solo var + prompt. Fase de captura (P2), render (P3),
                // escritura (P4) y params max/default (P5) llegan despues.
                // Por ahora el runtime cae al default del switch y AVANZA.
                if (startsWith(line, "entrada ") || startsWith(line, "input "))
                {
                    size_t prefixLen = startsWith(line, "entrada ") ? 8 : 6;
                    std::string rest = trim(line.substr(prefixLen));

                    // La variable NO lleva comillas. Si las trae, rechazo
                    // pedagogico (coherente con 'fuente', §7.42).
                    if (!rest.empty() && rest[0] == '"')
                    {
                        warn(lineNum, "entrada: la variable va SIN comillas "
                            "(ej: entrada nombre \"Como te llamas?\"). Las "
                            "comillas son para el prompt que el jugador lee, "
                            "no para el nombre de la variable.");
                        continue;
                    }

                    // var = primer token (hasta el espacio)
                    std::string var;
                    {
                        size_t sp = rest.find(' ');
                        var = rest.substr(0, sp);
                        rest = (sp != std::string::npos)
                            ? trim(rest.substr(sp + 1)) : "";
                    }

                    // prompt = lo que va entre comillas (puede ir vacio:
                    // el creador puede dibujar su propia etiqueta con 'texto').
                    std::string prompt;
                    extractQuoted(rest, prompt);

                    EclipCmd cmd; cmd.type = EclipCmdType::Entrada;
                    cmd.arg1 = var;      // variable destino
                    cmd.arg2 = prompt;   // texto que el jugador lee
                    cmd.sourceLine = lineNum;

                    // Params opcionales [max: N, default: "..."] (corchetes,
                    // como elegir/qte). max → inputMaxLen (codepoints, 0=sin
                    // límite); default → arg3 (multi-palabra entre comillas OK).
                    // Se buscan DESPUÉS del prompt (un '[' dentro del prompt no
                    // cuenta). Bilingüe: maximo/defecto.
                    {
                        size_t firstQ = rest.find('"');
                        size_t closeQ = (firstQ != std::string::npos)
                            ? rest.find('"', firstQ + 1) : std::string::npos;
                        std::string afterPrompt = (closeQ != std::string::npos)
                            ? rest.substr(closeQ + 1) : rest;
                        size_t bOpen = afterPrompt.find('[');
                        size_t bClose = afterPrompt.find(']');
                        if (bOpen != std::string::npos &&
                            bClose != std::string::npos && bClose > bOpen)
                        {
                            // SYN-3: el loop a mano que vivia aca (tercera copia
                            // del mismo idioma) subio a `bracketParams`. La
                            // bandera `requerido` viaja en la misma tabla con un
                            // bit: es vocabulario del comando, no otra especie.
                            std::string params = afterPrompt.substr(
                                bOpen + 1, bClose - bOpen - 1);
                            const auto pk = bracketParams(
                                params, EclipCmdType::Entrada, lineNum, "entrada");

                            if (auto it = pk.find("maximo"); it != pk.end())
                            {
                                // SYN-1a: `max: 20px` daba 20 y `max: perro`
                                // caía a "sin limite" en silencio — que es lo
                                // contrario de lo que el creador pidió.
                                if (parseIntStrict(it->second, cmd.inputMaxLen) != NumParse::Ok)
                                    warn(lineNum, "entrada: max/maximo espera "
                                        "un entero, recibio '" + it->second +
                                        "'. Se ignora (sin limite).");
                            }
                            if (auto it = pk.find("defecto"); it != pk.end())
                            {
                                const std::string& v = it->second;
                                if (!v.empty() && v[0] == '"')
                                    extractQuoted(v, cmd.arg3);
                                else
                                    cmd.arg3 = v;
                            }
                            if (pk.count("requerido")) cmd.inputRequired = true;
                        }
                    }

                    if (var.empty())
                        warn(lineNum, "entrada: falta el nombre de la variable "
                            "(ej: entrada nombre \"Como te llamas?\")");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }


                // ── capa_z / capa_visible / capa_fade / hotspot ───────────
                // Migrados a la tabla de despacho de capas (kLayerCmds, arriba
                // del if-chain) en Unit B / Pasada 2 — §7.49 / §7.50 / §7.42.
                // El parseo de estos 4 vive en la tabla; los bloques if viejos
                // se borraron acá (coexistencia → migración, §7.51).
     

                // ── esperar_click: / wait_click: ──────────────────────
                //   "carta" -> escena_carta
                //   "libro" -> escena_libro
                if (matchKeyword(line, "esperar_click") || matchKeyword(line, "wait_click"))
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::EsperarClick;
                    cmd.sourceLine = lineNum;

                    // SYN-3: este comando NO parseaba corchetes en absoluto —
                    // se tragaba la linea entera despues del keyword sin una
                    // palabra. Su tabla esta VACIA a proposito (no tiene timer,
                    // asi que la respuesta correcta no es aceptarlos: es decir
                    // que no existen), y la puerta hace justo eso.
                    {
                        size_t bOpen  = line.find('[');
                        size_t bClose = line.find(']');
                        if (bOpen != std::string::npos &&
                            bClose != std::string::npos && bClose > bOpen)
                        {
                            bracketParams(
                                line.substr(bOpen + 1, bClose - bOpen - 1),
                                EclipCmdType::EsperarClick, lineNum,
                                "esperar_click");
                        }
                    }

                    // Leer las siguientes líneas: "nombre" -> escena
                    // (mismo lector que `elegir` — era una copia, §7.323)
                    leerOpcionesDeBloque(lines, i, cmd.choices, "esperar_click");

                    if (cmd.choices.empty())
                        warn(lineNum, "esperar_click: sin mapeos");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── evento_random "id" probabilidad=N cooldown=N -> escena ──
                // ── random_event "id" chance=N cooldown=N -> scene ───────────
                if (startsWith(line, "evento_random ") || startsWith(line, "random_event "))
                {
                    size_t prefixLen = startsWith(line, "evento_random ") ? 14 : 13;
                    std::string rest = trim(line.substr(prefixLen));

                    // ── El id, SIN comillas (§7.44, puerta única) ──────────
                    // SYN-4a: este comando las EXIGÍA, y era el único. No por
                    // decisión: es de lo primero del motor (Bloque 2, ver
                    // DISENO_EVENTO_RANDOM §1) y su parser nunca recibió el
                    // retrofit de la tabla — sacaba el nombre con un `find('"')`
                    // crudo, que es como se saca un nombre de una línea que
                    // además trae `->` y `k=v`. La evidencia de que fue omisión
                    // y no firma: sin comillas sólo decía "falta nombre o escena
                    // destino", un error genérico. Una decisión deja pedagogía.
                    std::string name;
                    if (!tomarId(rest, name, "evento_random", lineNum)) continue;

                    // ── EL GATE `[si:]` (pierna C) ──────────────────────────
                    // Se pela ACÁ: después del id y **antes de buscar la
                    // flecha**. El orden no es cosmético — `->` se busca con un
                    // `find` crudo, así que un gate con flecha adentro
                    // (`[si: ruta == "a->b"]`) partiría la línea por el lugar
                    // equivocado si el corchete siguiera puesto. Pelándolo
                    // primero, ese caso no existe.
                    //
                    // G2 desde el diff: el aviso nombra la grafía ESCRITA.
                    const char* verbo = (prefixLen == 14) ? "evento_random"
                                                          : "random_event";
                    EclipGate gate;
                    if (parseConditionModifier(rest, gate, lineNum, verbo)
                            == GateParse::Malformado)
                        continue;   // fail-closed: ya avisó, y el evento NO se emite

                    // Extraer escena destino (después de ->)
                    std::string target;
                    size_t arrow = rest.find("->");
                    if (arrow != std::string::npos)
                    {
                        target = trim(rest.substr(arrow + 2));
                        rest   = trim(rest.substr(0, arrow));
                    }

                    // Un `[` que sigue vivo es un gate en el lugar equivocado
                    // (al final de la línea, o después de las claves). Ningún
                    // parámetro de este comando lleva corchetes —los tres son
                    // números y el destino es un id—, así que no hay falso
                    // positivo. Sin esto, `... -> escena [si: x == 1]` se iba
                    // ENTERO adentro del destino y el evento moría mucho más
                    // tarde y con otra voz ("la escena destino no existe").
                    if (rest.find('[') != std::string::npos ||
                        target.find('[') != std::string::npos)
                    {
                        warn(lineNum, std::string(verbo) + ": el gate va justo "
                            "despues del nombre, antes de las claves (ej: "
                            "evento_random lluvia [si: _hora >= 18] cada=5s "
                            "probabilidad=0.3 -> escena_lluvia). Se ignora esa "
                            "linea.");
                        continue;
                    }

                    // Parsear key=value params
                    auto kv = kvParams(rest, EclipCmdType::EventoRandom,
                                       lineNum, "evento_random");

                    EclipCmd cmd; cmd.type = EclipCmdType::EventoRandom;
                    cmd.arg1 = name;
                    cmd.eventTarget = target;
                    cmd.eventGate = gate;
                    cmd.sourceLine = lineNum;

                    // El RANGO es parte del vocabulario (misma ley que los
                    // efectos). El parser habla con línea; el clamp lo pone
                    // `TrialClock.h`, que es donde vive la ley — y el parser lo
                    // aplica también, para que el AVISO y el campo emitido digan
                    // lo mismo: un aviso que promete 0.05 sobre un comando que
                    // viaja con 0.01 es el aviso mintiendo.
                    // Mismo validador que los efectos: "5abc" no es un número
                    // (§1.57).
                    auto leerTasa = [&](const char* clave, float& destino)
                    {
                        if (!kv.count(clave)) return;
                        const std::string& v = kv[clave];
                        // SYN-4c-ii: `clave` sigue siendo la canonica (busca en
                        // el mapa y decide la pedagogia); `dicha` es la que se
                        // imprime. `probabilidad`/`chance` es el unico par
                        // bilingue de este comando — `cooldown` es monolingue y
                        // por eso su aviso queda byte por byte igual.
                        const std::string dicha = kv.escrita(clave);
                        if (!fxValidNumero(v))
                        {
                            warn(lineNum, "evento_random: " + dicha
                                + "= espera un numero, recibio '" + v
                                + "'. Se usa 0.");
                            return;
                        }
                        float f = std::stof(v);
                        if (f < 0.0f)
                        {
                            // La consecuencia solo es cierta para la tasa:
                            // cooldown=0 significa "sin espera", no "jamas".
                            const bool esTasa = std::string(clave) != "cooldown";
                            warn(lineNum, "evento_random: " + dicha
                                + "= no puede ser negativo (recibio " + v + "). "
                                + (esTasa ? "Con 0 el evento jamas dispara. " : "")
                                + "Se usa 0.");
                            f = 0.0f;
                        }
                        destino = f;
                    };
                    // SYN-1c-ii: `chance=` llega canonizado a `probabilidad=`.
                    // La segunda llamada era el desempate accidental (ganaba el
                    // INGLES). SYN-4b cerró R-VN-07: con `cada=` declarado, la
                    // palabra pública nombra la probabilidad de UN ENSAYO, y es
                    // cierta a cualquier framerate.
                    leerTasa("probabilidad", cmd.eventChance);
                    leerTasa("cooldown", cmd.eventCooldown);

                    // ── `cada=` / `every=`: EL RITMO (ley 1) ────────────────
                    // Sin ritmo declarado no hay ensayo, y sin ensayo
                    // `probabilidad` no puede ser una probabilidad de nada: el
                    // motor elegía "cada frame" como default mudo y de ahí salía
                    // la tasa (0.2 daba 18.1%/s, no 20%). El default es 1s: el
                    // único que hace verdadera la palabra sin migrar el corpus.
                    if (kv.count("cada"))
                    {
                        const std::string& v = kv["cada"];
                        eclips::DurationParseOptions opts;
                        // `cada=5` y `cada=5s` son LA MISMA duración, y las dos
                        // se aceptan — como en las otras once claves de tiempo
                        // del motor (`esperar`, `en=`, `periodo=`…). Medido: el
                        // corpus escribe 116 duraciones desnudas contra 24 con
                        // unidad, y `cooldown=`, que viaja en ESTA misma línea,
                        // ni siquiera admite la `s`.
                        //
                        // No es tolerancia a un marcador de tipo (§7.327): la
                        // `s` distingue segundos de `avances_dialogo`, y ese
                        // dominio acá está cerrado a propósito, así que las dos
                        // grafías no pueden chocar en ninguna línea — no hay
                        // empate que desempatar. Lo que sí protege la regla de
                        // CP0 sigue intacto: consumo total, así que `cada=8patos`
                        // muere y `cada=8avances_dialogo` recibe su pedagogía.
                        //
                        // **El manual enseña UNA sola forma** (la desnuda). El
                        // parser perdona; la doc manda. Cuál es la forma canónica
                        // del tiempo en TODA la superficie es una pregunta
                        // global, fichada con gatillo para SYN-4c: contestarla
                        // dentro de `cada=` sería parchar un mapa en una esquina.
                        opts.allowBareSeconds     = true;
                        opts.allowSeconds         = true;
                        opts.allowDialogueAdvance = false;  // sería INERTE, ver abajo
                        const auto r = eclips::parseDurationSpec(v, opts);

                        if (r.error == eclips::DurationParseError::UnsupportedClockDomain)
                        {
                            // Pedagogía, no un "no admite" seco: el tick del
                            // ensayo corre durante `esperar_click`, donde no hay
                            // avances de diálogo, así que la clave no fallaría —
                            // sería MUDA. Misma especie a la que SYN-3 le dio voz.
                            warn(lineNum, "evento_random: " + kv.escrita("cada") +
                                "= no admite "
                                "avances_dialogo. El ensayo corre durante "
                                "esperar_click, donde no hay avances de dialogo: "
                                "la clave seria inerte. Usa segundos (ej: cada=5s). "
                                "Se usa 1s.");
                        }
                        else if (!r)
                        {
                            warn(lineNum, "evento_random: " + kv.escrita("cada") +
                                "= espera una duracion en segundos, recibio '" + v
                                + "'. Se usa 1s.");
                        }
                        else
                        {
                            // El piso vive en TrialClock (copiado de
                            // `fxSorteoIntervalo`, con su razón escrita).
                            const f64 saneado = eclips::clampTrialPeriod(r.value.amount);
                            if (saneado != (f64)r.value.amount)
                                warn(lineNum, "evento_random: " +
                                    kv.escrita("cada") + "="
                                    + v + " es mas rapido que el piso de 0.05s. "
                                    "Se usa 0.05s.");
                            cmd.eventEvery = (float)saneado;
                        }
                    }

                    // ── El rango de la probabilidad, con voz en los DOS
                    //    extremos (ley 9) ─────────────────────────────────────
                    // El negativo ya hablaba desde el revival. El > 1 no: bajo
                    // la ley vieja era legal (una tasa vive en [0,∞) y 5/s tenía
                    // sentido). Con el ensayo, una probabilidad mayor que 1 no
                    // significa nada — y callarla dejaría al creador creyendo
                    // que pidió "más que siempre".
                    if (cmd.eventChance > 1.0f)
                    {
                        warn(lineNum, "evento_random: " +
                            kv.escrita("probabilidad") + "= "
                            "no puede pasar de 1 (recibio "
                            + kv["probabilidad"] + "). Con 1 dispara en cada "
                            "beat. Se usa 1.");
                        cmd.eventChance = eclips::clampTrialChance(cmd.eventChance);
                    }

                    // ── LEY 14: la clave OMITIDA avisa ──────────────────────
                    // `eventChance` nace en 0 y `leerTasa` retorna temprano si
                    // la clave no está, así que hasta hoy
                    // `evento_random cafe cooldown=10 -> escena` no disparaba
                    // JAMAS, en silencio — y el manual la listaba en una tabla
                    // de parámetros sin decir que fuera obligatoria. Es el mismo
                    // agujero que la ley 6 del revival cerró para el negativo:
                    // aquella vio al creador que escribe algo MAL y no al que no
                    // escribe nada. Cero ocurrencias vivas: es profiláctico, y
                    // su gatillo es la promesa pública de la tabla (§7.322).
                    if (!kv.count("probabilidad"))
                        warn(lineNum, "evento_random: falta probabilidad=. "
                            "Con 0 el evento jamas dispara. Escribe "
                            "probabilidad=0.2 (20% en cada ensayo).");

                    if (name.empty() || target.empty())
                        warn(lineNum, "evento_random: falta nombre o escena destino");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── iniciar_eventos / start_events ──────────────────────────
                if (line == "iniciar_eventos" || line == "start_events")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::IniciarEventos;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── detener_eventos / stop_events ──────────────────────────
                if (line == "detener_eventos" || line == "stop_events")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::DetenerEventos;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── azar_semilla N / random_seed N (ley 15) ────────────────
                // La ley 12 pedía que la semilla fuera reproducible y NO decía
                // cómo se pide: con `m_rng{ std::random_device{}() }` no hay
                // forma, así que la sonda de distribución que el propio gate
                // exige no podía existir. Es comando de guion —y no un hook de
                // harness— por tres razones escritas en el contrato: la sonda
                // tiene que correr en el motor real, le sirve al creador igual
                // que a nosotros (repetir una tirada mientras balancea pesos), y
                // sin argumento vuelve a semilla de reloj ("dame azar de verdad
                // otra vez"). `matchKeyword` y no `startsWith`: la frontera de
                // palabra es ley desde SYN-1b, así que `azar_semillax` no cae acá.
                if (matchKeyword(line, "azar_semilla") ||
                    matchKeyword(line, "random_seed"))
                {
                    const size_t sp = line.find(' ');
                    const std::string rest =
                        (sp == std::string::npos) ? std::string() : trim(line.substr(sp + 1));

                    EclipCmd cmd; cmd.type = EclipCmdType::AzarSemilla;
                    cmd.sourceLine = lineNum;
                    cmd.arg1 = rest;

                    if (!rest.empty())
                    {
                        int n = 0;
                        // Ley 1a: el valor malo muere, el comando vive — y sin
                        // valor la forma YA significa algo ("volvé al reloj"),
                        // así que degradar a eso es honesto, no un default mudo.
                        if (eclips::parseIntStrict(rest, n) != eclips::NumParse::Ok || n < 0)
                        {
                            warn(lineNum, std::string("azar_semilla: espera un "
                                "entero >= 0, recibio '") + rest
                                + "'. Se vuelve a la semilla de reloj.");
                        }
                        else
                        {
                            cmd.azarSeed = (unsigned int)n;
                            cmd.azarSeedFija = true;
                        }
                    }

                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── ocultar_caja / hide_box ────────────────────────────────
                if (line == "ocultar_caja" || line == "hide_box")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::OcultarCaja;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── mostrar_caja / show_box ────────────────────────────────
                if (line == "mostrar_caja" || line == "show_box")
                {
                    EclipCmd cmd; cmd.type = EclipCmdType::MostrarCaja;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── deshabilitar <lista> / habilitar <lista> ───────────────
                //     disable <list>      / enable <list>
                //
                // EL SELLO DE ACCIONES (`DISENO_DESHABILITAR_ACCIONES.md`).
                // *Es diferente ocultar a que realmente no hagan su funcion* —
                // el tema sigue siendo dueno de la VISIBILIDAD
                // (`botones_visibles`), y el `.ecl` gana la FUNCION.
                //
                //     deshabilitar acciones          # el grueso: META + RITMO
                //     habilitar historial            # lo refina
                //     deshabilitar guardado_rapido, menu_guardar   # o solo fino
                //
                // La forma no se invento: la copia de la que el motor ya ensena
                // del lado de la visibilidad (`botones_visibles no` +
                // `boton_menu_visible si`), con el orden secuencial decidiendo
                // (§7.16). La lista con comas tiene precedente en el atomo
                // (`interactuar: E, ENTER, X`).
                //
                // El vocabulario NO vive aca: vive en `VNActionSeal.h`, para que
                // un harness lo pruebe sin levantar este archivo, y para que no
                // haya dos tablas del mismo vocabulario (§7.442).
                {
                    const size_t espTok = line.find_first_of(" \t");
                    const std::string primerTok = line.substr(0, espTok);
                    const std::string tokLow = vocabLower(primerTok);
                    const bool esSellar   = (tokLow == "deshabilitar" || tokLow == "disable");
                    const bool esLiberar  = (tokLow == "habilitar"    || tokLow == "enable");

                    if (esSellar || esLiberar)
                    {
                        const std::string resto =
                            (espTok == std::string::npos)
                                ? std::string()
                                : trim(line.substr(espTok + 1));

                        if (resto.empty())
                        {
                            // Fail-closed (SYN-2): la forma pelada no se emite.
                            // Sin esto seria un comando que corre y no hace
                            // nada, que es el silencio que §6 del contrato del
                            // azar prohibe.
                            warn(lineNum, primerTok + ": falta la lista de acciones "
                                 "(ej: `" + primerTok + " acciones` o `" + primerTok +
                                 " historial, menu`). Ese comando no hara nada.");
                            continue;
                        }

                        eclips::VNActionMask mask = 0u;
                        size_t ini = 0;
                        while (ini <= resto.size())
                        {
                            const size_t coma = resto.find(',', ini);
                            const std::string crudo = trim(
                                resto.substr(ini, coma == std::string::npos
                                                      ? std::string::npos
                                                      : coma - ini));
                            ini = (coma == std::string::npos)
                                      ? resto.size() + 1
                                      : coma + 1;

                            if (crudo.empty()) continue;   // coma suelta: se ignora

                            const eclips::VNActionName v =
                                eclips::vnLookupAction(vocabLower(crudo));

                            switch (v.kind)
                            {
                            case eclips::VNActionLookup::Verbo:
                            case eclips::VNActionLookup::Grueso:
                                mask |= v.bits;
                                break;

                            // ── `D-46`: LA VOZ DE LA ULTIMA SALIDA ──────────
                            // Va en PARSE-TIME porque el creador esta
                            // ESCRIBIENDO cuando se equivoca. Y es una voz
                            // distinta de la del typo a proposito: escribio
                            // bien, y el motor se niega.
                            case eclips::VNActionLookup::SalidaProtegida:
                                warn(lineNum, primerTok + ": `" + crudo + "` NO se "
                                     "puede deshabilitar. Avanzar y elegir son la "
                                     "salida del jugador: sellarlas lo dejaria "
                                     "encerrado. Se ignora esa palabra.");
                                break;

                            case eclips::VNActionLookup::SinSelloPropio:
                                warn(lineNum, primerTok + ": `" + crudo + "` no tiene "
                                     "sello propio (el scroll solo existe con el log "
                                     "abierto). Usa `historial` y el log no abre. Se "
                                     "ignora esa palabra.");
                                break;

                            default:
                                warn(lineNum, primerTok + ": no conozco la accion `" +
                                     crudo + "`. Acciones: acciones (todas) | "
                                     "guardado_rapido carga_rapida menu_guardar "
                                     "menu_cargar historial menu config | auto "
                                     "saltar_texto. Se ignora esa palabra.");
                                break;
                            }
                        }

                        // Fail-closed: ninguna palabra valida -> no se emite. Ya
                        // hablo arriba, palabra por palabra.
                        if (mask == 0u) continue;

                        EclipCmd cmd;
                        cmd.type = esSellar ? EclipCmdType::DeshabilitarAcciones
                                            : EclipCmdType::HabilitarAcciones;
                        cmd.accionesMask = mask;
                        cmd.sourceLine = lineNum;
                        currentScene->cmds.push_back(cmd);
                        continue;
                    }
                }

                // ── usar_caja X / use_box X ─────────────────────────────────
                if (startsWith(line, "usar_caja ") || startsWith(line, "use_box "))
                {
                    std::string name = trim(line.substr(line.find(' ') + 1));
                    // Quitar comillas si las puso (cortesía)
                    if (name.size() >= 2 && name.front() == '"' && name.back() == '"')
                        name = name.substr(1, name.size() - 2);
                    EclipCmd cmd; cmd.type = EclipCmdType::UsarCaja;
                    cmd.arg1 = name; // "default" = reset a caja del theme
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── azar_traza si/no / random_trace yes/no ────────────────
                // LA VOZ DEL SILENCIO (DISENO_AZAR_ECLIPS §6). Con el ensayo
                // encima, "no pasó nada" tiene varias causas y ninguna tenía
                // voz: el creador escribe un evento, juega cinco minutos, no ve
                // nada, y no puede saber si fue mala suerte o un gate mal
                // escrito. Es opt-in y RUIDOSA a propósito — con `cada=1` y
                // cinco eventos son cinco renglones por segundo, y cuando estás
                // depurando "no pasa nada" querés ver CADA beat.
                if (startsWith(line, "azar_traza ") ||
                    startsWith(line, "random_trace "))
                {
                    const size_t sp = line.find(' ');
                    const std::string val = trim(line.substr(sp + 1));
                    EclipCmd cmd; cmd.type = EclipCmdType::AzarTraza;
                    std::string ctx = "azar_traza L" + std::to_string(lineNum);
                    cmd.arg1 = parseBool(val, false, ctx.c_str()) ? "1" : "0";
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── azar [repetir=si|no]: / random [repeat=yes|no]: ───────
                // EL BLOQUE DEL EJE **CUAL** (pierna B de DISENO_AZAR_ECLIPS).
                //
                //   azar repetir=no:
                //     peso=5   "no lo creo"
                //     peso=1 -> sala_rara
                //
                // La ley 3 vive acá: la exclusividad la carga el BLOQUE. Tres
                // líneas sueltas serían tres ensayos independientes; un `azar:`
                // es UN ensayo con N salidas, y por eso los pesos no suman nada
                // (ley 4) — "no sale nada" ya tiene dueño en el otro eje.
                //
                // El `k=v` en el header con los dos puntos al final es fork
                // firmado en mesa (2026-07-27): deja UNA sola gramática en todo
                // el constructo —el header y el cuerpo se leen igual— y libera
                // el corchete para el gate `[si:]` de la pierna C.
                if (!line.empty() && line.back() == ':')
                {
                    const std::string cabecera =
                        trim(line.substr(0, line.size() - 1));
                    const bool esEs = matchKeyword(cabecera, "azar");
                    const bool esEn = matchKeyword(cabecera, "random");

                    if (esEs || esEn)
                    {
                        EclipCmd cmd; cmd.type = EclipCmdType::Azar;
                        cmd.sourceLine = lineNum;

                        const std::string rest =
                            trim(cabecera.substr(esEs ? 4 : 6));
                        if (!rest.empty())
                        {
                            auto kv = kvParams(rest, EclipCmdType::Azar,
                                               lineNum, "azar");
                            auto it = kv.find("repetir");
                            if (it != kv.end())
                            {
                                // `parseBool` es la puerta de §1.45: trae el
                                // aviso anti-silencio puesto, así que un
                                // `repetir=quiza` habla en vez de caer mudo al
                                // default.
                                // SYN-4c-ii: el contexto que ve el creador lleva
                                // la grafia que escribio (`repeat=` si escribio
                                // en ingles), no la canonica.
                                std::string ctx = "azar " +
                                    kv.escrita("repetir") + "= L" +
                                    std::to_string(lineNum);
                                cmd.azarRepetir =
                                    parseBool(it->second, true, ctx.c_str());
                            }
                        }

                        leerEntradasDeAzar(lines, i, cmd.choices, "azar");

                        // Fail-closed estructural (SYN-2): un bloque sin
                        // entradas no tiene qué sortear, así que no se emite.
                        if (cmd.choices.empty())
                        {
                            warn(lineNum, "azar: sin entradas. La forma es "
                                "peso=N \"tu texto\" o peso=N -> escena, una "
                                "por linea y con sangria.");
                            continue;
                        }

                        // Todas apagadas: el bloque existe y no puede producir
                        // nada. NO mata el comando —es un problema de VALOR,
                        // no de estructura (ley 1a)—, pero callarlo sería el
                        // mismo agujero que la ley 14 cerró para
                        // `probabilidad=` omitida.
                        {
                            float suma = 0.0f;
                            for (const auto& c : cmd.choices)
                                if (c.peso > 0.0f) suma += c.peso;
                            if (suma <= 0.0f)
                                warn(lineNum, "azar: todas las entradas estan "
                                    "en peso=0, asi que el bloque no puede "
                                    "elegir ninguna y nunca pasa nada.");
                        }

                        // Ley 5, con la razón MEDIDA (enmienda 2026-07-27).
                        // El contrato decía "con dos items es alternancia
                        // estricta A,B,A,B: totalmente predecible", y
                        // `sonda_costura_bolsa` lo desmintió: la corrida real
                        // es ABBAABBABA..., el 25% repite. Lo que SI es cierto
                        // —y es mejor razon para el aviso— es que bajo bolsa
                        // cada entrada sale exactamente una vez por ciclo pase
                        // lo que pase, o sea que con dos entradas `peso=90` y
                        // `peso=10` dan 50/50 exacto: la clave casi no hace
                        // nada, y en silencio.
                        if (!cmd.azarRepetir && cmd.choices.size() < 3)
                            warn(lineNum, "azar: con repetir=no y menos de tres "
                                "entradas, cada una sale exactamente una vez "
                                "por ciclo, asi que peso= solo decide el orden "
                                "y no cuantas veces sale (con dos entradas el "
                                "reparto es 50/50 aunque escribas peso=90 y "
                                "peso=10). Usa repetir=si si querias una "
                                "distribucion.");

                        currentScene->cmds.push_back(cmd);
                        continue;
                    }

                    // ── palabras N [repetir=]: / words N [repeat=]: ───────
                    // EL POOL CLICKEABLE (`DISENO_MUTAR_PALABRAS` CP1).
                    //
                    //   palabras 10 repetir=no:
                    //     "amor" natsuki                    # azucar
                    //     [si: acto == 1] "festival" sayori
                    //     peso=0.01 "rara" -> rotura
                    //
                    // Comparte header con `azar:` a proposito: `k=v` pelado
                    // con los dos puntos al final, que es fork firmado en mesa
                    // (2026-07-27) y deja el CORCHETE libre para el gate. El
                    // contrato lo dibujaba con corchete en las dos cosas a la
                    // vez; gana el codigo (§1.2).
                    //
                    // La `N` es POSICIONAL y obligatoria: es cuantas se
                    // muestran, NO cuantas hay en el pool.
                    {
                        const bool palEs = matchKeyword(cabecera, "palabras");
                        const bool palEn = matchKeyword(cabecera, "words");
                        if (palEs || palEn)
                        {
                            EclipCmd cmd; cmd.type = EclipCmdType::Palabras;
                            cmd.sourceLine = lineNum;
                            // El default del BLOQUE es `no` (agotar el pool),
                            // al reves que `azar:`. Ver EclipScript.h:460.
                            cmd.azarRepetir = false;

                            std::string rest =
                                trim(cabecera.substr(palEs ? 8 : 5));
                            const char* verbo = palEs ? "palabras" : "words";

                            std::string tokN;
                            {
                                std::istringstream hs(rest);
                                hs >> tokN;
                            }
                            int n = 0;
                            const bool nOk =
                                !tokN.empty() &&
                                parseIntStrict(tokN, n) == NumParse::Ok &&
                                n > 0;

                            if (nOk)
                            {
                                cmd.palabrasN = n;
                                const std::string tras =
                                    trim(rest.substr(tokN.size()));
                                if (!tras.empty())
                                {
                                    auto kv = kvParams(tras,
                                        EclipCmdType::Palabras, lineNum, verbo);
                                    auto it = kv.find("repetir");
                                    if (it != kv.end())
                                    {
                                        std::string ctx = std::string(verbo) +
                                            " " + kv.escrita("repetir") +
                                            "= L" + std::to_string(lineNum);
                                        cmd.azarRepetir = parseBool(
                                            it->second, false, ctx.c_str());
                                    }

                                    // ── `CP2a`: EL AREA Y EL ESTILO ──────────
                                    // Se MARCA la presencia en vez de mirar si
                                    // el valor quedo en 0: un `x=0` es una orden
                                    // (el borde del lienzo) y `sin x=` es "usa
                                    // el default", y un centinela no sabe
                                    // distinguirlos. Es `{pausa=0}`/`RO-3`
                                    // aplicado a un area.
                                    cmd.mark(EclipField::X,
                                        kvFloat(kv, "x",     cmd.layerX, lineNum, verbo));
                                    cmd.mark(EclipField::Y,
                                        kvFloat(kv, "y",     cmd.layerY, lineNum, verbo));
                                    cmd.mark(EclipField::W,
                                        kvFloat(kv, "ancho", cmd.layerW, lineNum, verbo));
                                    cmd.mark(EclipField::H,
                                        kvFloat(kv, "alto",  cmd.layerH, lineNum, verbo));
                                    cmd.mark(EclipField::Z,
                                        kvInt  (kv, "z",     cmd.layerZ, lineNum, verbo));
                                    if (kv.count("estilo")) {
                                        cmd.styleName = kv["estilo"];
                                        cmd.mark(EclipField::Estilo);
                                    }
                                }
                            }

                            // El cuerpo se lee SIEMPRE, aunque la N este mal:
                            // si no, las palabras del pool caerian al guion
                            // como narraciones sueltas y el creador veria su
                            // pool RECITADO en pantalla.
                            leerEntradasDePalabras(lines, i, cmd.choices, verbo);

                            if (!nOk)
                            {
                                warn(lineNum, std::string(verbo) + ": falta "
                                    "cuantas palabras mostrar, o no es un "
                                    "entero (recibi '" + tokN + "'). La forma "
                                    "es `" + verbo + " 10:` — el numero es "
                                    "cuantas se muestran, no cuantas hay.");
                                continue;   // fail-closed (SYN-2)
                            }

                            if (cmd.choices.empty())
                            {
                                warn(lineNum, std::string(verbo) + ": sin "
                                    "palabras. La forma es \"tu texto\" "
                                    "variable, una por linea y con sangria.");
                                continue;   // fail-closed estructural
                            }

                            // La N contra el POOL, y sólo bajo `repetir=no`.
                            // Es problema de VALOR y no de forma (ley 1a): el
                            // bloque se emite igual, pero callarlo dejaría al
                            // creador pidiendo diez y viendo tres sin una
                            // palabra del motor. Misma especie que el aviso de
                            // la bolsa corta de `azar:`.
                            //
                            // Bajo `repetir=si` NO sale: ahí pedir más que el
                            // pool es legítimo, porque se repiten.
                            if (!cmd.azarRepetir &&
                                (int)cmd.choices.size() < cmd.palabrasN)
                                warn(lineNum, std::string(verbo) + ": pediste " +
                                    std::to_string(cmd.palabrasN) + " y el pool "
                                    "tiene " + std::to_string(cmd.choices.size()) +
                                    ". Con repetir=no cada palabra sale una sola "
                                    "vez por ciclo, asi que se van a ver " +
                                    std::to_string(cmd.choices.size()) + ". Usa "
                                    "repetir=si si querias que se repitan.");

                            currentScene->cmds.push_back(cmd);
                            continue;
                        }
                    }
                }

                // ── parallax si/no / parallax yes/no ──────────────────────
                if (startsWith(line, "parallax "))
                {
                    std::string val = trim(line.substr(9));
                    EclipCmd cmd; cmd.type = EclipCmdType::ParallaxToggle;
                    // parseBool central (§1.45): ASCII-safe + warning anti-silencio.
                    std::string ctx = "parallax L" + std::to_string(lineNum);
                    cmd.arg1 = parseBool(val, false, ctx.c_str()) ? "1" : "0";
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── iniciar_batalla [alias] [hp:N] / start_battle ────────
                // iniciar_batalla                  → HP=5, sin portrait
                // iniciar_batalla Estrella          → portrait del alias
                // iniciar_batalla Estrella hp:3     → alias + HP
                // iniciar_batalla hp:3             → solo HP
                if (startsWith(line, "iniciar_batalla") ||
                    startsWith(line, "start_battle"))
                {
                    size_t prefixLen = startsWith(line, "iniciar_batalla") ? 15 : 12;
                    std::string rest = (line.size() > prefixLen)
                        ? trim(line.substr(prefixLen)) : "";

                    std::string alias = "";
                    int hp = 5;

                    // Extraer hp:N si existe
                    size_t hpPos = rest.find("hp:");
                    if (hpPos != std::string::npos)
                    {
                        std::string hpStr = trim(rest.substr(hpPos + 3));
                        size_t sp = hpStr.find(' ');
                        if (sp != std::string::npos) hpStr = hpStr.substr(0, sp);
                        try { hp = std::stoi(hpStr); }
                        catch (...) {}
                        rest = trim(rest.substr(0, hpPos)); // alias limpio
                    }

                    // Lo que queda es el alias del personaje (opcional)
                    if (!rest.empty()) alias = rest;

                    EclipCmd cmd; cmd.type = EclipCmdType::IniciarBatalla;
                    cmd.arg1 = alias;
                    cmd.arg2 = std::to_string(hp);
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── ir_a_pasillo "bg" "npc" "nombre" / go_to_corridor ────
                if (startsWith(line, "ir_a_pasillo") ||
                    startsWith(line, "go_to_corridor"))
                {
                    // Extraer hasta 3 strings entre comillas
                    auto extractNth = [](const std::string& s, int n) -> std::string
                        {
                            std::string out;
                            int count = 0;
                            size_t pos = 0;
                            while (pos < s.size())
                            {
                                size_t q1 = s.find('"', pos);
                                if (q1 == std::string::npos) break;
                                size_t q2 = s.find('"', q1 + 1);
                                if (q2 == std::string::npos) break;
                                if (count == n) { out = s.substr(q1 + 1, q2 - q1 - 1); break; }
                                count++;
                                pos = q2 + 1;
                            }
                            return out;
                        };

                    EclipCmd cmd; cmd.type = EclipCmdType::IrAPasillo;
                    cmd.arg1 = extractNth(line, 0); // fondo
                    cmd.arg2 = extractNth(line, 1); // sprite NPC
                    cmd.arg3 = extractNth(line, 2); // nombre NPC
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── gameplay topdown "ruta.eclmap" / gameplay sidescroll ─────
                if (startsWith(line, "gameplay "))
                {
                    std::string rest = trim(line.substr(9)); // después de "gameplay "
                    std::string mode = "topdown";
                    std::string path;

                    // Primer token = modo (topdown/sidescroll), segundo = ruta
                    size_t sp = rest.find(' ');
                    if (sp != std::string::npos)
                    {
                        mode = trim(rest.substr(0, sp));
                        std::string remainder = trim(rest.substr(sp + 1));
                        extractQuoted(remainder, path);
                        if (path.empty()) path = remainder; // sin comillas
                    }
                    else
                    {
                        // Solo un argumento — ¿es ruta o modo?
                        if (rest.find('.') != std::string::npos)
                            path = rest; // es ruta
                        else
                            mode = rest; // es modo sin ruta
                    }

                    EclipCmd cmd; cmd.type = EclipCmdType::Gameplay;
                    cmd.arg1 = mode;  // "topdown" o "sidescroll"
                    cmd.arg2 = path;  // ruta al .eclmap
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);

                    std::cout << "[EclipScript]   Gameplay: modo=" << mode
                        << " mapa='" << path << "'" << std::endl;
                    continue;
                }

                // ── cargar_tema "ruta" / load_theme "path" ──────────────
                if (startsWith(line, "cargar_tema ") ||
                    startsWith(line, "load_theme "))
                {
                    std::string path;
                    extractQuoted(line, path);
                    EclipCmd cmd; cmd.type = EclipCmdType::CargarTema;
                    cmd.arg1 = path; cmd.sourceLine = lineNum;
                    if (path.empty())                       // SYN-2: fail-closed + voz
                        warn(lineNum, "cargar_tema: falta la ruta del tema entre "
                            "comillas (ej: cargar_tema \"temas/noche\").");
                    else
                        currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── vigneta N [Ts] / vignette N [Ts] ────────────────────
                // vigneta 0.8          → 80% instantáneo
                // vigneta 0.8 2s       → fade a 80% en 2 segundos
                // vigneta off          → apagar instantáneo
                // vigneta off 1.5s     → apagar en 1.5s
                if (startsWith(line, "vigneta ") || startsWith(line, "vignette ") ||
                    line == "vigneta" || line == "vignette")
                {
                    size_t prefixLen = startsWith(line, "vigneta") ? 7 : 8;
                    std::string rest = (line.size() > prefixLen)
                        ? trim(line.substr(prefixLen)) : "";

                    std::string intensityStr = "0";
                    float duration = 0.0f;
                    bool durationValid = true;

                    auto parseVignetteDuration = [&](const std::string& token)
                    {
                        DurationParseOptions durationOptions;
                        durationOptions.allowBareSeconds = true;
                        durationOptions.allowDialogueAdvance = false;
                        const DurationParseResult parsedDuration =
                            parseDurationSpec(token, durationOptions);
                        if (!parsedDuration)
                        {
                            warn(lineNum, "vigneta: duracion '" + token +
                                "' invalida: " +
                                durationParseErrorText(parsedDuration.error) +
                                ". Se esperan segundos; la linea se ignora.");
                            durationValid = false;
                            return;
                        }
                        duration = parsedDuration.value.amount;
                    };

                    // "off" como alias de 0
                    //
                    // FOLD-PARTIDO: ranura PURA -o `off`, o una intensidad
                    // (numero o porcentaje)-, asi que `OFF` es la misma palabra.
                    // Se pliega SOLO para esta prueba y no `rest` entero: el
                    // resto sigue siendo la intensidad y su duracion, que tienen
                    // su propia ley (`parseDurationSpec`) y no son de aca.
                    // Lo encontro el SEGUNDO barrido, despues de que la tabla
                    // del guardian ya estaba verde: una tabla enumerada prueba
                    // lo que enumera, y enumerar no es lo mismo que barrer.
                    const std::string restOff = vocabLower(rest);
                    bool isOff = (restOff == "off" || startsWith(restOff, "off "));
                    if (isOff)
                    {
                        intensityStr = "0";
                        std::string durPart = trim(rest.substr(3));
                        if (!durPart.empty()) parseVignetteDuration(durPart);
                    }
                    else
                    {
                        size_t sp = rest.find(' ');
                        if (sp != std::string::npos)
                        {
                            intensityStr = trim(rest.substr(0, sp));
                            std::string ds = trim(rest.substr(sp + 1));
                            parseVignetteDuration(ds);
                        }
                        else
                        {
                            intensityStr = rest;
                        }
                    }

                    if (!durationValid) continue;

                    EclipCmd cmd; cmd.type = EclipCmdType::Vigneta;
                    cmd.arg1 = intensityStr;
                    cmd.animDuration = duration;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── resaltar_hablante si/no / highlight_speaker yes/no ───
                if (startsWith(line, "resaltar_hablante ") ||
                    startsWith(line, "highlight_speaker "))
                {
                    size_t sp = line.find(' ');
                    std::string val = trim(line.substr(sp + 1));
                    EclipCmd cmd; cmd.type = EclipCmdType::SpeakerHighlight;
                    // parseBool central (§1.45): ASCII-safe + warning anti-silencio.
                    std::string ctx = "resaltar_hablante L" + std::to_string(lineNum);
                    cmd.arg1 = parseBool(val, false, ctx.c_str()) ? "1" : "0";
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── Narrador: línea que empieza con comillas ───────────────
                // Ej: "Una voz llegó desde el pasillo."
                if (!line.empty() && line[0] == '"')
                {
                    // `D-33`: la forma pegada se separa ANTES de leer el texto.
                    // Sin esto, `extractQuotedGreedy` se comería el `elegir:` de
                    // la cola como parte de la narración.
                    std::string cabeza, paramsElegir;
                    const bool pegado = separarElegirPegado(line, cabeza,
                                                            paramsElegir);
                    const std::string& fuente = pegado ? cabeza : line;

                    std::string text;
                    if (!extractQuotedGreedy(fuente, text))
                    {
                        warn(lineNum, "narracion con comilla sin cerrar: " + line);
                        continue;
                    }

                    // El `elegir` se arma PRIMERO: si el bloque no trajo
                    // opciones no hay nada con qué fusionar, y la línea tiene
                    // que quedar como la de hoy en vez de pegarse al comando
                    // que venga después (`D-38`).
                    EclipCmd eleccion;
                    const bool hayEleccion =
                        pegado && armarElegir(lines, i, paramsElegir, lineNum,
                                              eleccion);

                    EclipCmd cmd; cmd.type = EclipCmdType::Narrador;
                    cmd.arg1 = text; cmd.sourceLine = lineNum;
                    cmd.fusedChoice = hayEleccion;
                    currentScene->cmds.push_back(cmd);
                    if (hayEleccion) currentScene->cmds.push_back(eleccion);

                    // `D-34`: lo que no es `elegir` no se pega — y se dice.
                    if (!pegado) avisarPegadoInvalido(line, lineNum);
                    continue;
                }

                // ── animar/animate alias efecto [Ns] [intensidad=N] ──────
                if (startsWith(line, "animar ") || startsWith(line, "animate "))
                {
                    size_t prefixLen = startsWith(line, "animar ") ? 7 : 8;
                    std::string rest = trim(line.substr(prefixLen));

                    // ── C-CP3: TODOS los key=value primero, posicionales después.
                    // El parse viejo buscaba "intensidad=" y TRUNCABA ahí
                    // (rest.substr(0, ipos)): con un segundo parámetro se habría
                    // comido al resto según el ORDEN en que el creador escribiera.
                    // Tokenizar primero es lo único que no depende del orden.
                    //
                    // ── E-CP0: y el tokenizado deja de JUZGAR. Antes la lista blanca
                    // (intensidad/periodo/eje) se aplicaba DENTRO del bucle, lo que
                    // daba por supuesto que todos los efectos aceptan las mismas
                    // claves. `vaiven` y `franja` no: qué es válido DEPENDE del
                    // efecto — y el efecto es posicional, llega después. Así que se
                    // junta todo crudo, se resuelve el efecto, y RECIÉN AHÍ se valida.
                    // Mismo orden de siempre, un paso más tarde.
                    std::map<std::string, std::string> kv;
                    std::vector<std::string> positional;
                    {
                        std::istringstream issA(rest);
                        std::string tokA;
                        while (issA >> tokA)
                        {
                            size_t eq = tokA.find('=');
                            if (eq == std::string::npos) { positional.push_back(tokA); continue; }
                            kv[toLower(tokA.substr(0, eq))] = tokA.substr(eq + 1);
                        }
                    }

                    // Posicionales: alias efecto [Ns] o la acotacion teatral
                    // `despues de N` / `after N`. La forma key=value retirada
                    // se detecta aparte y nunca se reinterpreta como alias.
                    std::string alias, effect, durStr, afterStr;
                    bool hasAfterClause = false;
                    bool malformedAfter = false;
                    bool hasRetiredAfterKey = false;
                    for (const char* retiredKey : { "despues", "after" })
                    {
                        if (kv.count(retiredKey))
                        {
                            warn(lineNum, "animar: '" + std::string(retiredKey) +
                                "=' esta retirada; usa 'despues de <duracion>' "
                                "o 'after <duration>'.");
                            hasRetiredAfterKey = true;
                        }
                    }
                    if (positional.size() >= 1) alias = positional[0];
                    if (positional.size() >= 2) effect = positional[1];
                    if (positional.size() >= 3)
                    {
                        const std::string marker = vocabLower(positional[2]);
                        const bool spanishAfter =
                            marker == "despues" || marker == "después";
                        if (spanishAfter || marker == "after")
                        {
                            hasAfterClause = true;
                            const bool exactShape = spanishAfter
                                ? positional.size() == 5 &&
                                  vocabLower(positional[3]) == "de"
                                : positional.size() == 4;
                            if (!exactShape)
                            {
                                warn(lineNum, "animar: forma diferida invalida. "
                                    "Usa 'despues de <duracion>' o "
                                    "'after <duration>'.");
                                malformedAfter = true;
                            }
                            else
                                afterStr = spanishAfter
                                    ? positional[4] : positional[3];
                        }
                        else
                        {
                            durStr = positional[2];
                            if (positional.size() > 3)
                                warn(lineNum, "animar: sobra '" + positional[3] +
                                    "'. Forma: animar <alias> <efecto> [Ns] [params].");
                        }
                    }

                    if (hasRetiredAfterKey || malformedAfter)
                        continue;

                    EclipCmd cmd;
                    cmd.type = EclipCmdType::Animar;
                    cmd.arg1 = alias;       // "Luna" o "pantalla"/"screen"
                    cmd.arg2 = toLower(effect);
                    cmd.sourceLine = lineNum;

                    // Los DOS vocabularios, resueltos aquí — en parse-time, con
                    // número de línea. Mismo precedente que C-CP1 con las curvas.
                    // Son disjuntos: `glitch` es preset, `franja` es primitiva.
                    const AnimType atPreset = parseAnimType(cmd.arg2);
                    const FxKind   fkPrim = parseFxKind(cmd.arg2);

                    // C-CP5: la TERCERA especie — receta nombrada. Se resuelve
                    // acá también (el orden del contrato: preset → primitiva →
                    // nombrada; y `animacion` ya rechazó bautizar recetas con
                    // palabras del motor, así que no hay sombra posible).
                    const auto itNamed =
                        (atPreset == AnimType::None && fkPrim == FxKind::None)
                        ? namedAnims.find(cmd.arg2) : namedAnims.end();

                    float intensity = 1.0f;
                    float period = 0.0f;
                    bool  axisVertical = false;
                    bool  hasPeriod = false, hasAxis = false;
                    float durEn = 0.0f;   // E-CP2: driver una_vez (`en=`)
                    bool invalidDriverDuration = false;

                    // `intensidad=` es UNIVERSAL (§2 del contrato): la leen los dos
                    // caminos. Vive en cmd.animIntensity — UN solo hogar (§1.40);
                    // el runtime la materializa al FxParams desde ahí, no la duplica.
                    {
                        // SYN-4c-ii: el segundo read-twice del archivo, misma
                        // puerta que el `en=`/`in=` del driver.
                        const auto parInt =
                            fxTomarPar(kv, "intensidad", "intensity",
                                       lineNum, "animar");
                        if (parInt.hay)
                        {
                            // G7: stof("5abc") devuelve 5 y NO lanza — el catch no
                            // corría nunca. fxValidNumero exige consumir todo el
                            // token: a medias no vale.
                            if (fxValidNumero(parInt.valor))
                            {
                                intensity = std::stof(parInt.valor);

                                // E-CP1: el RANGO es parte del vocabulario. Una
                                // intensidad negativa terminaba multiplicando un
                                // alpha hasta el otro lado del cero. El runtime la
                                // clampea igual (getF); esto es la VOZ, con línea.
                                const float ic = fxClampKey("intensidad", intensity);
                                if (ic != intensity)
                                {
                                    warn(lineNum, "animar: " + parInt.escrita + "=" +
                                        parInt.valor +
                                        " esta fuera de rango (" +
                                        fxKeyRangeName("intensidad") + "). Se usa " +
                                        fxNumStr(ic) + ".");
                                    intensity = ic;
                                }
                            }
                            else
                                warn(lineNum, "animar: " + parInt.escrita + "='" +
                                    parInt.valor +
                                    "' no es un numero. Se usa 1.");
                        }
                    }

                    if (fkPrim != FxKind::None)
                    {
                        // ── Claves de DRIVER (§1.2 / §7.249): el CUÁNDO viaja en
                        // la línea `animar`, no en el vocabulario de la
                        // primitiva. `en=` (E-CP2) implica una_vez; media=/
                        // varianza=/frames=/desde= (E-CP4) implican esporadico.
                        // Viajan en el k=v (así llegan al `E;`, mesa-bump §4)
                        // pero se consumen ACÁ: la primitiva jamás las ve.
                        bool hasEsporadico = false;
                        {
                            // §7.249, el aviso de COLISIÓN: si una primitiva
                            // futura declara una clave reservada como param
                            // propio, esta voz la delata. Hoy no tiene punto de
                            // aplicación — esa es exactamente la idea.
                            static const char* kDriverKeys[] =
                                { "en", "media", "varianza", "frames", "desde" };
                            for (const char* dk : kDriverKeys)
                                if (fxAcceptsKey(fkPrim, dk))
                                    warn(lineNum, std::string("COLISION de vocabulario: '")
                                        + fxKindToString(fkPrim) + "' declara '" + dk
                                        + "=' que es clave RESERVADA de driver "
                                        "La primitiva nunca la va a "
                                        "recibir: renombrala.");

                            // E-CP5: el cuerpo del parse vive en fxParseDriver,
                            // compartido con el camino receta (un solo punto).
                            hasEsporadico = fxParseDriver(lineNum,
                                "animar " + cmd.arg2, kv, cmd.fxParams, durEn,
                                invalidDriverDuration);
                        }

                        // ── Camino PRIMITIVA (E-CP0). Las claves se CANONIZAN a
                        // español: el creador escribe speed= o velocidad=, pero en
                        // el cmd (y mañana en el payload `E;` del save) hay UNA sola
                        // grafía. Si guardáramos la cruda, el mismo efecto tendría
                        // dos representaciones en disco.
                        for (const auto& e : kv)
                        {
                            std::string k = canonFxKey(e.first);
                            if (k == "intensidad") continue;   // ya leída arriba
                            if (fxKeyEsDriver(k)) continue;    // consumidas arriba

                            // E-CP5: el cuerpo (tipo, rango, costo, canon, UNA
                            // grafía) vive en fxValidaParam — compartido con
                            // las líneas de efecto del bloque `animacion`.
                            fxValidaParam(lineNum, "animar " + cmd.arg2,
                                fkPrim, e.first, e.second, cmd.fxParams);
                        }

                        // E-CP3: `sustituto` sin textura= no tiene NADA que
                        // estampar. Aviso + descarte: el eco no lista lo que
                        // no corre, y guardar un comando muerto seria mentir.
                        if (fkPrim == FxKind::Sustituto &&
                            !cmd.fxParams.count("textura"))
                        {
                            warn(lineNum, "animar sustituto: falta textura= (su "
                                "parametro obligatorio). Ese comando no hara nada.");
                            continue;
                        }
                    }
                    else if (atPreset != AnimType::None)
                    {
                        // ── Camino PRESET: la lista blanca de siempre, intacta.
                        for (const auto& e : kv)
                        {
                            const std::string  key = canonAnimKey(e.first);
                            const std::string& val = e.second;

                            // ── El vocabulario es POR ANIMACIÓN, no global ────────
                            // Antes, la cadena de else-if no preguntaba CUÁL anim
                            // era: `animar m flotar periodo=2 eje=y` pasaba el
                            // filtro y no hacía NADA, en silencio. Espejo exacto de
                            // `fxAcceptsKey` (E-CP0). Ahora el aviso además lista
                            // los parámetros DE ESA animación, no los de todas.
                            if (!animAcceptsKey(atPreset, key))
                            {
                                warn(lineNum, "animar " + cmd.arg2 + ": no conozco el "
                                    "parametro '" + e.first + "='. Parametros: " +
                                    animKeysHelp(atPreset) + ".");
                                continue;
                            }

                            if (key == "intensidad")
                            {
                                // ya leída arriba (universal)
                            }
                            else if (key == "periodo")
                            {
                                DurationParseOptions durationOptions;
                                durationOptions.allowBareSeconds = true;
                                durationOptions.allowDialogueAdvance = false;
                                const DurationParseResult parsedDuration =
                                    parseDurationSpec(val, durationOptions);
                                if (!parsedDuration)
                                {
                                    warn(lineNum, "animar: periodo='" + val +
                                        "' no es una duracion valida en segundos: " +
                                        durationParseErrorText(parsedDuration.error) +
                                        ". Ese comando se ignora.");
                                    invalidDriverDuration = true;
                                }
                                else
                                {
                                    period = parsedDuration.value.amount;
                                    hasPeriod = true;
                                }
                            }
                            else if (key == "eje")
                            {
                                std::string v = toLower(val);
                                if (v == "y" || v == "vertical") { axisVertical = true;  hasAxis = true; }
                                else if (v == "x" || v == "horizontal") { axisVertical = false; hasAxis = true; }
                                else warn(lineNum, "animar: eje='" + val +
                                    "': uso 'x'/'horizontal' o 'y'/'vertical'.");
                            }
                        }
                    }
                    else if (itNamed != namedAnims.end())
                    {
                        // ── Camino RECETA (C-CP5 + E-CP5): viaja RESUELTA.
                        if (itNamed->second.empty())
                        {
                            warn(lineNum, "animar: la receta '" + cmd.arg2 +
                                "' esta vacia (declarada sin lineas). Ese "
                                "comando no hara nada.");
                        }
                        else
                        {
                            cmd.namedName = cmd.arg2;
                            cmd.namedTracks = itNamed->second.tracks;
                            cmd.namedFx = itNamed->second.fx;
                            // CP2: la envolvente grupal viaja RESUELTA con la
                            // receta (vacía si no se declaró).
                            cmd.namedFxEnvelope = itNamed->second.fxIntensityEnvelope;

                            const bool tieneFxN = !cmd.namedFx.empty();

                            // E-CP5: el DRIVER y la intensidad viajan en el
                            // `animar` y aplican a las líneas de EFECTO de la
                            // receta (§1.2: la receta dice qué; el guion dice
                            // cuándo). Los canales traen su tiempo adentro
                            // (en=/periodo= por línea).
                            if (tieneFxN)
                                fxParseDriver(lineNum, "animar " + cmd.arg2,
                                    kv, cmd.fxParams, durEn,
                                    invalidDriverDuration);

                            for (const auto& e : kv)
                            {
                                const std::string kN = canonFxKey(e.first);
                                const bool esDeInstancia =
                                    (kN == "intensidad") || fxKeyEsDriver(kN);
                                if (esDeInstancia && tieneFxN)
                                    continue;   // consumidas: driver/intensidad
                                if (esDeInstancia)
                                    warn(lineNum, "animar " + cmd.arg2 + ": '" +
                                        e.first + "=' aplica a las lineas de "
                                        "EFECTO de una receta, y '" + cmd.arg2 +
                                        "' no tiene (solo canales). Se ignora.");
                                else
                                    warn(lineNum, "animar " + cmd.arg2 + ": '" +
                                        e.first + "=' no aplica a una receta "
                                        "(los params van en el bloque "
                                        "animacion). Se ignora.");
                            }

                            // Duración posicional: con efectos es la del
                            // driver una_vez (espejo del camino primitiva);
                            // sin efectos no hay a quién dársela.
                            if (!durStr.empty() && !tieneFxN)
                                warn(lineNum, "animar " + cmd.arg2 + ": la "
                                    "duracion vive en la receta (en=/periodo= "
                                    "por linea). Se ignora '" + durStr + "'.");
                        }
                    }
                    else if (esLuaFx(cmd.arg2))
                    {
                        // ── E-CP6: camino EFECTO LUA (cuarta especie). El
                        // nombre se resolvió acá, con línea (la declaración
                        // efecto_lua lo hizo posible); la función se resuelve
                        // en runtime contra el registro. El CUÁNDO viaja en
                        // el animar, venga lo que venga después (§1.2).
                        cmd.luaFxName = cmd.arg2;

                        fxParseDriver(lineNum, "animar " + cmd.arg2, kv,
                            cmd.fxParams, durEn, invalidDriverDuration);

                        for (const auto& e : kv)
                        {
                            const std::string k = canonFxKey(e.first);
                            if (k == "intensidad") continue;   // ya leída arriba
                            if (fxKeyEsDriver(k)) continue;    // consumidas arriba

                            // Claves LIBRES: el vocabulario es del efecto Lua
                            // y el motor no lo conoce — fingir un "no conozco
                            // el parametro" seria mentir. Lo que SÍ es del
                            // motor: grafía canónica, comillas peladas
                            // (§7.41) y el RANGO de las claves comunes.
                            std::string vv = e.second;
                            if (vv.size() >= 2 && vv.front() == '"' &&
                                vv.back() == '"')
                                vv = vv.substr(1, vv.size() - 2);
                            if (fxKeyHasRange(k) && fxValidNumero(vv))
                            {
                                const float vN = std::stof(vv);
                                const float vC = fxClampKey(k, vN);
                                if (vC != vN)
                                {
                                    warn(lineNum, "animar " + cmd.arg2 + ": " +
                                        k + "=" + e.second + " esta fuera de "
                                        "rango (" + fxKeyRangeName(k) +
                                        "). Se usa " + fxNumStr(vC) + ".");
                                    vv = fxNumStr(vC);
                                }
                            }
                            cmd.fxParams[k] = vv;
                        }
                    }
                    else if (!effect.empty())
                    {
                        // ── E-CP0, arreglo gratis: hasta hoy `animar Luna xyz` no
                        // decía NADA. parseAnimType daba None y el despacho de
                        // VNScriptState (`if (at != AnimType::None)`) simplemente no
                        // entraba — en silencio. El motor deja de mentir por omisión.
                        // Es la misma lección del token 'a' y del zoom sobre pantalla.
                        // C-CP5: y si era una receta, la pista más probable es el
                        // ORDEN — se definen antes de usarse, en orden de archivo.
                        warn(lineNum, "animar: no conozco el efecto '" + cmd.arg2 +
                            "' (ni preset, ni primitiva, ni receta, ni efecto "
                            "Lua - recetas y efectos Lua se definen ANTES de "
                            "usarse, en orden de archivo). Ese comando no "
                            "hara nada.");
                    }

                    if (invalidDriverDuration)
                        continue;

                    // ANIMAR-DESPUES-01: la demora es una propiedad de la
                    // invocacion completa. Solo una receta pura de tracks
                    // Seconds/UnaVez puede cruzar esta puerta; cualquier
                    // especie excluida falla cerrada, nunca cae a inmediato.
                    if (hasAfterClause)
                    {
                        if (cmd.namedName.empty())
                        {
                            warn(lineNum, "animar: 'despues de/after' solo se "
                                "admite sobre recetas nombradas puras de tracks "
                                "una_vez con reloj Seconds. El comando se ignora.");
                            continue;
                        }

                        bool recetaPuraSecondsUnaVez =
                            !cmd.namedTracks.empty() && cmd.namedFx.empty();
                        for (const auto& tr : cmd.namedTracks)
                        {
                            if (tr.modo != Lifecycle::UnaVez ||
                                tr.clock != ClockDomain::Seconds)
                            {
                                recetaPuraSecondsUnaVez = false;
                                break;
                            }
                        }
                        if (!recetaPuraSecondsUnaVez)
                        {
                            warn(lineNum, "animar " + cmd.namedName +
                                ": 'despues de/after' requiere una receta "
                                "pura de tracks una_vez con reloj Seconds; "
                                "FX, Lua, loop, vaiven y otros relojes se "
                                "rechazan. El comando se ignora.");
                            continue;
                        }

                        DurationParseOptions afterOptions;
                        afterOptions.allowBareSeconds = true;
                        afterOptions.allowDialogueAdvance = false;
                        const DurationParseResult parsedAfter =
                            parseDurationSpec(afterStr, afterOptions);
                        bool afterValid = static_cast<bool>(parsedAfter);
                        float afterSeconds = 0.0f;

                        // parseDurationSpec conserva la politica historica de
                        // duraciones estrictamente positivas. Esta superficie
                        // firma N>=0: acepta cero segundos, sin abrir otro
                        // parser ni admitir -0.
                        if (afterValid)
                            afterSeconds = parsedAfter.value.amount;
                        else if (parsedAfter.error == DurationParseError::NonPositive &&
                                 parsedAfter.value.clock == ClockDomain::Seconds)
                        {
                            std::string zeroToken = trimDurationToken(afterStr);
                            if (durationTokenEndsWith(zeroToken, "s") ||
                                durationTokenEndsWith(zeroToken, "S"))
                                zeroToken.pop_back();
                            float zeroValue = 0.0f;
                            if (parseFiniteFloatFull(zeroToken, zeroValue) &&
                                zeroValue == 0.0f && !std::signbit(zeroValue))
                            {
                                afterValid = true;
                                afterSeconds = 0.0f;
                            }
                        }

                        if (!afterValid)
                        {
                            warn(lineNum, "animar: despues de/after '" + afterStr +
                                "' invalido: " +
                                durationParseErrorText(parsedAfter.error) +
                                ". Se esperan segundos finitos N >= 0 (0.5s; "
                                "tambien se acepta 0s). El comando se ignora.");
                            continue;
                        }

                        cmd.animAfter = afterSeconds;
                        cmd.animAfterSpecified = true;
                    }

                    cmd.animIntensity = intensity;
                    cmd.animPeriod = period;
                    cmd.animAxisVertical = axisVertical;

                    // CP0 reloj-dialogo: un solo parser estricto. Este caller
                    // conserva segundos desnudos por compatibilidad, pero NO
                    // admite DialogueAdvance (los presets/FX no ganan ese
                    // lifetime por accidente). Nunca se acepta prefijo parcial.
                    if (!durStr.empty())
                    {
                        DurationParseOptions durationOptions;
                        durationOptions.allowBareSeconds = true;
                        durationOptions.allowDialogueAdvance = false;
                        const DurationParseResult parsedDuration =
                            parseDurationSpec(durStr, durationOptions);
                        if (!parsedDuration)
                        {
                            warn(lineNum, "animar: duracion '" + durStr +
                                "' invalida: " +
                                durationParseErrorText(parsedDuration.error) +
                                ". Se esperan segundos (0.5s; legacy 0.5). "
                                "Ese comando se ignora.");
                            continue;
                        }
                        cmd.animDuration = parsedDuration.value.amount;
                    }

                    // E-CP2: `en=` es la grafía canónica del driver; si además
                    // vino la posicional, se dice y gana la del driver.
                    if (durEn > 0.0f)
                    {
                        if (cmd.animDuration > 0.0f)
                            warn(lineNum, "animar: la duracion vino dos veces "
                                "(posicional '" + durStr + "' y en=). Gana en=.");
                        cmd.animDuration = durEn;
                    }

                    // Honestidad pedagógica: periodo=/eje= SOLO los lee `vaiven`
                    // (Nudge). Pedírselos a otro preset no hace nada → el motor lo
                    // DICE, en vez de tolerarlo callado (la lección del token 'a').
                    // (En el camino primitiva no hace falta: periodo= ya cayó en el
                    //  "no conozco el parametro" de fxAcceptsKey, con su ayuda.)
                    if ((hasPeriod || hasAxis) &&
                        cmd.arg2 != "vaiven" && cmd.arg2 != "nudge")
                    {
                        warn(lineNum, "animar: '" + cmd.arg2 + "' ignora periodo=/eje= "
                            "(solo los usa 'vaiven'). Su ritmo esta horneado en el preset.");
                    }

                    // ── El ECO es el manifiesto de lo que se va a EJECUTAR ───────
                    // `animar m xyz` avisaba "ese comando no hara nada"... y dos
                    // líneas después lo imprimía en el eco Y lo metía en cmds. Un
                    // comando muerto que viaja, se despacha (sin hacer nada, en
                    // silencio) y queda listado como si hubiera aterrizado. El
                    // creador escanea el eco y le CREE — porque el eco es justo la
                    // línea a la que le enseñamos a creerle.
                    //
                    // Lo que no corre, no se lista.
                    const bool efectoVivo =
                        (atPreset != AnimType::None) || (fkPrim != FxKind::None)
                        || !cmd.namedName.empty()    // C-CP5: receta con líneas
                        || !cmd.luaFxName.empty();   // E-CP6: efecto lua declarado

                    if (alias.empty() || effect.empty())
                    {
                        warn(lineNum, "animar: falta alias o efecto");
                    }
                    else if (!efectoVivo)
                    {
                        // Ya avisamos arriba ("no conozco el efecto"). Silencio acá:
                        // repetirlo sería ruido, y echarlo sería mentir.
                    }
                    else
                    {
                        // F3 (mesa-bump): el eco imprime la grafía CANONIZADA a
                        // español — lo que va a ejecutar, no lo tecleado. El
                        // camino preset espera su espejo español (writer v3).
                        std::cout << "[EclipScript]   Animar: " << alias << " "
                            << (fkPrim != FxKind::None
                                ? fxKindToString(fkPrim) : effect)
                            << (cmd.luaFxName.empty() ? "" : " (lua)");
                        // (receta SIN efectos: duracion/intensidad se avisaron
                        //  como ignoradas — el eco no lista lo que no corre.
                        //  CON efectos si corren: driver/multiplicador, E-CP5.)
                        const bool ecoInstancia =
                            cmd.namedName.empty() || !cmd.namedFx.empty();
                        if (ecoInstancia && cmd.animDuration > 0)
                            std::cout << " " << cmd.animDuration << "s";
                        if (!cmd.namedName.empty() && cmd.animAfterSpecified)
                            std::cout << " despues de " << cmd.animAfter << "s";
                        if (ecoInstancia && intensity != 1.0f)
                            std::cout << " intensidad=" << intensity;
                        if (hasPeriod) std::cout << " periodo=" << period;
                        if (hasAxis)   std::cout << " eje=" << (axisVertical ? "y" : "x");

                        // ── El eco del camino FX tiraba sus parámetros ───────────
                        // `animar m franja color1=#00ff00 color2=#ff00ff separacion=8`
                        // se veía como `Animar: m franja`, pelado — el creador no
                        // podía saber si el motor le había entendido. Y es JUSTO la
                        // línea que tiene que mostrarle las dos cosas que E-CP1 acaba
                        // de meter: la clave CANONIZADA (`speed=` → `velocidad=`) y
                        // el valor CLAMPEADO (`velocidad=-1` → `velocidad=0`).
                        //
                        // El AVISO dice qué estuvo mal. El ECO dice qué QUEDÓ.
                        // (Sale ordenado alfabéticamente: `cmd.fxParams` es un map.
                        //  Determinista y legible; el orden de tipeo no importa.)
                        for (const auto& p : cmd.fxParams)
                            std::cout << " " << p.first << "=" << p.second;

                        // C-CP5/E-CP5: la receta dice cuánto expandió — el
                        // creador ve que el motor la resolvió, y a qué.
                        if (!cmd.namedName.empty())
                            std::cout << " " << descDef(itNamed->second);

                        std::cout << std::endl;
                        currentScene->cmds.push_back(cmd);
                    }
                    continue;
                }

                // ── detener_animacion/stop_animation alias ────────────────
                if (startsWith(line, "detener_animacion ") ||
                    startsWith(line, "stop_animation "))
                {
                    size_t prefixLen = startsWith(line, "detener_animacion ") ? 18 : 15;
                    std::string rest = trim(line.substr(prefixLen));

                    // C-CP2 (F4): `detener_animacion <alias> [tipo]`. Sin tipo,
                    // baja TODO el stack (lo de siempre). Con tipo, baja solo esa
                    // — la contraparte del apilamiento: sin esto, apilar sería
                    // una calle de una sola vía.
                    std::string alias = rest, tipo;
                    size_t spDet = rest.find(' ');
                    if (spDet != std::string::npos)
                    {
                        alias = trim(rest.substr(0, spDet));
                        tipo = toLower(trim(rest.substr(spDet + 1)));
                    }

                    EclipCmd cmd;
                    cmd.type = EclipCmdType::DetenerAnimacion;
                    cmd.arg1 = alias;
                    cmd.arg2 = tipo;          // "" = todas
                    cmd.sourceLine = lineNum;

                    // C-CP5: el tipo puede ser una RECETA (tercera especie) —
                    // y desde E-CP6, un efecto LUA declarado (cuarta).
                    // Se resuelve acá, con la línea; si no es de NINGÚN
                    // vocabulario, se dice y no viaja — lo que no corre no se
                    // lista (la ley del eco). El aviso runtime queda de
                    // cinturón, ya inalcanzable desde un .ecl.
                    if (!tipo.empty()
                        && parseAnimType(tipo) == AnimType::None
                        && parseFxKind(tipo) == FxKind::None)
                    {
                        if (namedAnims.count(tipo))
                            cmd.namedName = tipo;
                        else if (esLuaFx(tipo))
                            cmd.luaFxName = tipo;
                        else
                        {
                            warn(lineNum, "detener_animacion " + alias + ": no "
                                "conozco '" + tipo + "' (ni preset, ni "
                                "primitiva, ni receta, ni efecto Lua). Sin "
                                "segundo argumento se detienen todas. Ese "
                                "comando no hara nada.");
                            continue;
                        }
                    }

                    currentScene->cmds.push_back(cmd);
                    std::cout << "[EclipScript]   Detener animacion: " << alias
                        << (tipo.empty() ? "" : (" (" + tipo + ")")) << std::endl;
                    continue;
                }

                // ── esperar_animacion/wait_animation ──────────────────────
                if (line == "esperar_animacion" || line == "wait_animation")
                {
                    EclipCmd cmd;
                    cmd.type = EclipCmdType::EsperarAnimacion;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    std::cout << "[EclipScript]   Esperar animacion" << std::endl;
                    continue;
                }

                // ── consola crear <id> x= y= w= h= color_fondo=#RRGGBBAA
                //    color_texto=#RRGGBB tamano= fuente= [cursor=si] (CP4) ─────
                // La consola es un SceneElement (familia capa) → hereda
                // capa_mover/animar/capa_visible/z por id, gratis.
                if (startsWith(line, "consola crear ") ||
                    startsWith(line, "console create "))
                {
                    size_t sp1 = line.find(' ');               // tras "consola"
                    size_t sp2 = line.find(' ', sp1 + 1);      // tras "crear"
                    std::string rest = (sp2 != std::string::npos)
                        ? trim(line.substr(sp2 + 1)) : "";

                    // ── id = primer token, SIN comillas (§7.44, puerta única) ─
                    // SYN-4a: acá el primer token se tomaba TAL CUAL, así que
                    // un `consola crear "log" ...` dejaba el id valiendo
                    // literalmente `"log"` —con las comillas adentro— y sin una
                    // sola palabra del motor. Después nada volvía a encontrar
                    // esa consola por su nombre.
                    std::string id;
                    if (!tomarId(rest, id, "consola crear", lineNum)) continue;

                    auto kv = kvParams(rest, EclipCmdType::ConsolaCrear,
                                       lineNum, "consola crear");
                    // SYN-1c-ii: `getKV(a,b)` era el desempate accidental de
                    // ESTE comando —cortaba en la primera, o sea ganaba el
                    // ESPANOL, al reves que `control`—. Ahora la grafia ya
                    // llega canonizada y solo queda el lookup.
                    auto getKV = [&](const char* k) -> std::string {
                        auto it = kv.find(k);
                        return (it == kv.end()) ? std::string() : it->second;
                    };

                    EclipCmd cmd; cmd.type = EclipCmdType::ConsolaCrear;
                    cmd.arg1 = id;
                    cmd.sourceLine = lineNum;

                    kvInt  (kv, "z", cmd.layerZ, lineNum, "consola crear");
                    kvFloat(kv, "x", cmd.layerX, lineNum, "consola crear");
                    kvFloat(kv, "y", cmd.layerY, lineNum, "consola crear");
                    kvFloat(kv, "w", cmd.layerW, lineNum, "consola crear");
                    kvFloat(kv, "h", cmd.layerH, lineNum, "consola crear");

                    // color_fondo: #RRGGBB con AA opcional (8 díg) — el fondo
                    // es el ÚNICO campo del guión VN con alpha. El helper lo
                    // resuelve entero; ya no hay un segundo extractor de AA
                    // que pudiera discrepar del primero (R-VN-08).
                    std::string bg = getKV("color_fondo");
                    if (!bg.empty() &&
                        !parseHexColor(bg, cmd.consoleBgR, cmd.consoleBgG,
                                       cmd.consoleBgB, &cmd.consoleBgA))
                        warn(lineNum, "consola crear: " +
                             kv.escrita("color_fondo") + "='" + bg +
                             "' no es un color valido (usa #RRGGBB o "
                             "#RRGGBBAA). Se conserva el fondo por defecto.");

                    // color_texto NO tiene alpha: ocho dígitos se rechazan en
                    // vez de ignorar el AA en silencio.
                    std::string tx = getKV("color_texto");
                    if (!tx.empty() &&
                        !parseHexColor(tx, cmd.consoleTxR, cmd.consoleTxG,
                                       cmd.consoleTxB))
                        warn(lineNum, "consola crear: " +
                             kv.escrita("color_texto") + "='" + tx +
                             "' no es un color valido (usa #RRGGBB - este "
                             "campo no lleva alpha). Se conserva el blanco.");

                    // tamano / fuente (espeja 'fuente'): tamano= sized atlas, fuente= nombrada.
                    kvFloat(kv, "tamano", cmd.fontSize, lineNum, "consola crear");
                    { std::string f = getKV("fuente"); if (!f.empty()) cmd.fontName = f; }

                    { std::string cu = getKV("cursor");
                      if (!cu.empty())
                      {
                          std::string ctx = "consola cursor L" + std::to_string(lineNum);
                          cmd.consoleCursor = parseBool(cu, false, ctx.c_str());
                      } }

                    if (id.empty())
                        warn(lineNum, "consola crear: falta el id");
                    else
                    {
                        currentScene->cmds.push_back(cmd);
                        std::cout << "[EclipScript]   Consola crear: " << id << std::endl;
                    }
                    continue;
                }

                // ── consola_escribir <id> "..." — appendea + typewriter ──────
                if (startsWith(line, "consola_escribir ") ||
                    startsWith(line, "console_write "))
                {
                    size_t sp = line.find(' ');
                    std::string rest = trim(line.substr(sp + 1));
                    std::string id;
                    size_t idsp = rest.find(' ');
                    id = rest.substr(0, idsp);
                    rest = (idsp != std::string::npos) ? trim(rest.substr(idsp + 1)) : "";
                    std::string text;
                    extractQuotedGreedy(rest, text);

                    EclipCmd cmd; cmd.type = EclipCmdType::ConsolaEscribir;
                    cmd.arg1 = id; cmd.arg2 = applyConsoleEscapes(text);
                    cmd.sourceLine = lineNum;
                    if (id.empty()) warn(lineNum, "consola_escribir: falta el id");
                    else currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── consola_imprimir <id> "..." — appendea de golpe ──────────
                if (startsWith(line, "consola_imprimir ") ||
                    startsWith(line, "console_print "))
                {
                    size_t sp = line.find(' ');
                    std::string rest = trim(line.substr(sp + 1));
                    std::string id;
                    size_t idsp = rest.find(' ');
                    id = rest.substr(0, idsp);
                    rest = (idsp != std::string::npos) ? trim(rest.substr(idsp + 1)) : "";
                    std::string text;
                    extractQuotedGreedy(rest, text);

                    EclipCmd cmd; cmd.type = EclipCmdType::ConsolaImprimir;
                    cmd.arg1 = id; cmd.arg2 = applyConsoleEscapes(text);
                    cmd.sourceLine = lineNum;
                    if (id.empty()) warn(lineNum, "consola_imprimir: falta el id");
                    else currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── consola_limpiar <id> — vacía el buffer ───────────────────
                if (startsWith(line, "consola_limpiar ") ||
                    startsWith(line, "console_clear "))
                {
                    size_t sp = line.find(' ');
                    std::string id = trim(line.substr(sp + 1));
                    EclipCmd cmd; cmd.type = EclipCmdType::ConsolaLimpiar;
                    cmd.arg1 = id; cmd.sourceLine = lineNum;
                    if (id.empty()) warn(lineNum, "consola_limpiar: falta el id");
                    else currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── consola_borrar <id> [N | todo] — retract char-por-char ───
                if (startsWith(line, "consola_borrar ") ||
                    startsWith(line, "console_erase "))
                {
                    size_t sp = line.find(' ');
                    std::string rest = trim(line.substr(sp + 1));
                    std::string id;
                    size_t idsp = rest.find(' ');
                    id = rest.substr(0, idsp);
                    // arg2 = "N" | "todo" | "" (vacío = 1 char). Se interpreta runtime.
                    std::string amount = (idsp != std::string::npos)
                        ? trim(rest.substr(idsp + 1)) : "";

                    EclipCmd cmd; cmd.type = EclipCmdType::ConsolaBorrar;
                    cmd.arg1 = id; cmd.arg2 = toLower(amount); cmd.sourceLine = lineNum;
                    if (id.empty()) warn(lineNum, "consola_borrar: falta el id");
                    else currentScene->cmds.push_back(cmd);
                    continue;
                }

                // ── Ambito/scope: bloque léxico nombrado (FO-DDLC-2A) ────────
                // La llave solo delimita el conjunto de declaraciones. La
                // asociación runtime permanece activa hasta un salto válido o
                // hasta que otro bloque homónimo la reemplace.
                if (!line.empty() && line.back() == '{')
                {
                    std::string before = trim(line.substr(0, line.size() - 1));
                    size_t sp = before.find_first_of(" \t");
                    std::string kw = (sp == std::string::npos)
                        ? before : before.substr(0, sp);
                    if (kw == "ambito" || kw == "scope")
                    {
                        if (!currentScene)
                        {
                            warn(lineNum, "'" + kw + " {' fuera de una escena. Ignorado.");
                            continue;
                        }
                        if (inAmbito)
                        {
                            warn(lineNum, "bloques ambito/scope anidados no soportados: " + line);
                            continue;
                        }

                        const std::string name = (sp == std::string::npos)
                            ? std::string() : trim(before.substr(sp + 1));
                        if (name.empty() || name.find_first_of(" \t") != std::string::npos)
                        {
                            warn(lineNum, "ambito/scope requiere un nombre unico antes de '{'.");
                            continue;
                        }

                        EclipCmd cmd;
                        cmd.type = EclipCmdType::AmbitoBegin;
                        cmd.arg1 = name;
                        cmd.sourceLine = lineNum;
                        currentScene->cmds.push_back(cmd);
                        inAmbito = true;
                        ambitoOpenLine = lineNum;
                        std::cout << "[EclipScript]   Ambito '" << name << "' {" << std::endl;
                        continue;
                    }
                }

                // ── Cinemática: bloque que sella el input (CP1) ──────────────
                // β: header en la apertura. 'cinematica [saltable=si|no] {'.
                // Sella el skip mientras dura → 'esperar' corre por reloj y el
                // click no salta. El cuerpo son comandos normales entre Begin/End.
                if (!line.empty() && line.back() == '{')
                {
                    std::string before = trim(line.substr(0, line.size() - 1));
                    std::string kw = before;
                    size_t sp = before.find_first_of(" \t");
                    if (sp != std::string::npos) kw = before.substr(0, sp);

                    if (kw == "cinematica" || kw == "cinematic")
                    {
                        if (!currentScene)
                        {
                            warn(lineNum, "'cinematica {' fuera de una escena. Ignorado.");
                            continue;
                        }
                        if (inCinematica)
                            warn(lineNum, "'cinematica {' anidado no soportado: " + line);

                        std::string header = (sp == std::string::npos)
                            ? std::string() : trim(before.substr(sp + 1));

                        bool saltable = false; // default: no-saltable
                        if (!header.empty())
                        {
                            size_t eq = header.find('=');
                            std::string hk = trim(header.substr(0,
                                eq == std::string::npos ? header.size() : eq));
                            if (hk == "saltable" || hk == "skippable")
                            {
                                std::string hv = (eq == std::string::npos)
                                    ? std::string() : trim(header.substr(eq + 1));
                                std::string ctx = "saltable en cinematica (linea "
                                    + std::to_string(lineNum) + ")";
                                saltable = parseBool(hv, false, ctx.c_str());
                            }
                            else
                            {
                                warn(lineNum, "campo desconocido en 'cinematica': '"
                                    + header + "' (solo 'saltable=si|no'). Ignorado.");
                            }
                        }

                        EclipCmd cmd; cmd.type = EclipCmdType::CinematicaBegin;
                        cmd.cineSaltable = saltable;
                        cmd.sourceLine = lineNum;
                        currentScene->cmds.push_back(cmd);
                        inCinematica = true;
                        cineOpenLine = lineNum;
                        std::cout << "[EclipScript]   Cinematica { (saltable="
                            << (saltable ? "si" : "no") << ")" << std::endl;
                        continue;
                    }
                }

                // ── Cinemática: cierre del bloque ────────────────────────────
                if (line == "}")
                {
                    if (inAmbito)
                    {
                        EclipCmd cmd;
                        cmd.type = EclipCmdType::AmbitoEnd;
                        cmd.sourceLine = lineNum;
                        currentScene->cmds.push_back(cmd);
                        inAmbito = false;
                        std::cout << "[EclipScript]   Ambito }" << std::endl;
                        continue;
                    }
                    if (!inCinematica)
                    {
                        warn(lineNum, "'}' suelto sin 'cinematica {' abierto. Ignorado.");
                        continue;
                    }
                    EclipCmd cmd; cmd.type = EclipCmdType::CinematicaEnd;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    inCinematica = false;
                    std::cout << "[EclipScript]   Cinematica }" << std::endl;
                    continue;
                }

                // ── esperar_interfaz / wait_ui — hold explícito de UI ───────
                // La igualdad completa es deliberada: `esperar_interfaz_x` no
                // es una grafía futura ni un prefijo aceptado.
                if (line == "esperar_interfaz" || line == "wait_ui")
                {
                    EclipCmd cmd;
                    cmd.type = EclipCmdType::EsperarInterfaz;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    std::cout << "[EclipScript]   Esperar interfaz" << std::endl;
                    continue;
                }

                // ── esperar N / wait N — pausa por tiempo ────────────────
                if ((line.size() > 8 && line.substr(0, 8) == "esperar ") ||
                    (line.size() > 5 && line.substr(0, 5) == "wait "))
                {
                    std::string rest;
                    if (line.substr(0, 8) == "esperar ")
                        rest = trim(line.substr(8));
                    else
                        rest = trim(line.substr(5));

                    DurationParseOptions durationOptions;
                    durationOptions.allowBareSeconds = true;
                    durationOptions.allowDialogueAdvance = false;
                    const DurationParseResult parsedDuration =
                        parseDurationSpec(rest, durationOptions);
                    if (!parsedDuration)
                    {
                        warn(lineNum, "esperar/wait: duracion '" + rest +
                            "' invalida: " +
                            durationParseErrorText(parsedDuration.error) +
                            ". Se esperan segundos (1.5s; legacy 1.5). "
                            "El comando se ignora.");
                        continue;
                    }
                    const float seconds = parsedDuration.value.amount;

                    EclipCmd cmd;
                    cmd.type = EclipCmdType::EsperarTiempo;
                    cmd.waitDuration = seconds;
                    cmd.sourceLine = lineNum;
                    currentScene->cmds.push_back(cmd);
                    std::cout << "[EclipScript]   Esperar " << seconds << "s" << std::endl;
                    continue;
                }

                // ── Diálogo: alias [expresion]: "texto" ───────────────────
                // Detectar por la presencia de : seguido (eventualmente) de "
                {
                    // `D-33`: la forma pegada se separa ANTES de buscar los dos
                    // puntos del hablante. Si no, en `m: "hola" elegir:` los DOS
                    // `:` compiten — y el que abre el bloque llegaría al alias.
                    // Separando primero, `cabeza` es `m: "hola"`, o sea la forma
                    // que este parser ya sabe leer desde siempre.
                    std::string cabeza, paramsElegir;
                    const bool pegado = separarElegirPegado(line, cabeza,
                                                            paramsElegir);
                    const std::string& fuente = pegado ? cabeza : line;

                    size_t colonPos = fuente.find(':');
                    if (colonPos != std::string::npos)
                    {
                        std::string leftPart  = trim(fuente.substr(0, colonPos));
                        std::string rightPart = trim(fuente.substr(colonPos + 1));

                        // Verificar que rightPart tenga comillas (es diálogo)
                        if (!rightPart.empty() && rightPart.find('"') != std::string::npos)
                        {
                            std::string dialogText;
                            if (!extractQuotedGreedy(rightPart, dialogText))
                            {
                                warn(lineNum, "dialogo con comilla sin cerrar: " + line);
                                continue;
                            }

                            // leftPart puede ser "Luna" o "Luna feliz"
                            std::istringstream iss(leftPart);
                            std::string alias, expr;
                            iss >> alias;
                            std::getline(iss, expr);
                            expr = trim(expr);

                            // El `elegir` primero: sin opciones no hay fusión
                            // (`D-38`) y la línea queda como la de hoy.
                            EclipCmd eleccion;
                            const bool hayEleccion =
                                pegado && armarElegir(lines, i, paramsElegir,
                                                      lineNum, eleccion);

                            EclipCmd cmd; cmd.type = EclipCmdType::Dialogo;
                            cmd.arg1 = alias;       // Alias del personaje
                            cmd.arg2 = dialogText;  // Texto a mostrar
                            cmd.arg3 = expr;        // Expresión (opcional)
                            cmd.sourceLine = lineNum;
                            cmd.fusedChoice = hayEleccion;
                            currentScene->cmds.push_back(cmd);
                            if (hayEleccion)
                                currentScene->cmds.push_back(eleccion);

                            // `D-34`: lo que no es `elegir` no se pega.
                            if (!pegado) avisarPegadoInvalido(line, lineNum);
                            continue;
                        }
                    }
                }

                // secuencia/sequence suelto: su propia voz (P8), no "linea no reconocida"
                if (matchKeyword(line, "secuencia") || matchKeyword(line, "sequence"))
                {
                    const std::string palabra =
                        matchKeyword(line, "secuencia") ? "secuencia" : "sequence";
                    warn(lineNum, "'" + palabra + "' solo vale dentro de un "
                        "bloque elegir. Se ignora esa linea.");
                    continue;
                }

                // Si llegamos aquí, línea desconocida
                warn(lineNum, "linea no reconocida: " + line);
            }

            // ── SYN-3: la cinematica sin cerrar ─────────────────────────────
            //
            // Este era el UNICO aviso del parser que no pasaba por `warn`, o
            // sea el unico SIN numero de linea — y justo el que mas lo
            // necesita, porque lo que hay que ir a buscar es donde ABRIO el
            // bloque, no donde se acabo el archivo.
            //
            // El `result.valid = true` que la auditoria ficha se conserva a
            // proposito (decision de mesa): invalidar el guion entero por una
            // llave es mas caro que el bug, y R-VN-04 pedia que dejara de ser
            // mudo, no que se volviera fatal. Lo que si se hace es CERRAR el
            // bloque, para que el sello de input quede balanceado en vez de
            // colgado.
            if (inCinematica)
            {
                warn(cineOpenLine, "'cinematica {' sin cerrar (falta el '}'). "
                    "Se cierra sola al final del archivo para que el sello de "
                    "input no quede colgado.");
                if (currentScene)
                {
                    EclipCmd cierre; cierre.type = EclipCmdType::CinematicaEnd;
                    cierre.sourceLine = cineOpenLine;
                    currentScene->cmds.push_back(cierre);
                }
                inCinematica = false;
            }

            if (inAmbito)
            {
                warn(ambitoOpenLine, "'ambito/scope {' sin cerrar (falta el '}'). "
                    "Se cierra al final del archivo.");
                inAmbito = false;
            }

            // CP-R3: un bloque de fuente sin '}' descarta el PERFIL COMPLETO,
            // y a diferencia de la cinematica NO se cierra solo. La razon es
            // que degradar a la base sola le daria al creador una fuente sin
            // los respaldos que escribio: misma ruta, otra cobertura, y en
            // silencio. Ni el comando ni la declaracion entran.
            if (fuenteBloqueAbierta)
            {
                warn(fuenteBloqueLinea, "fuente: el bloque de respaldos de '" +
                    fuenteEnCurso.name + "' no se cerro con '}'. "
                    "El perfil completo se descarta (no cae a la base sola).");
                fuenteBloqueAbierta = false;
                fuenteEnCurso = fontdecl::Declaracion{};
            }

            if (result.scenes.empty())

            {
                result.error = "El script no contiene ninguna escena";
                return result;
            }

            // FO-DDLC-2A guardian metadata. This is parse-time observability
            // for harnesses only; the runtime owner is still the generation
            // created by enterAmbito() and cleared by AmbitoEnd/jumpToScene().
            for (auto& [sceneName, scene] : result.scenes)
            {
                (void)sceneName;
                std::uint64_t lexicalOwner = 0;
                for (auto& cmd : scene.cmds)
                {
                    cmd.lexicalScopeOwner = lexicalOwner;
                    if (cmd.type == EclipCmdType::AmbitoBegin)
                        lexicalOwner = 1;
                    else if (cmd.type == EclipCmdType::AmbitoEnd)
                        lexicalOwner = 0;
                }
            }

            // ── Resolver saltos de si/sino/fin_si ─────────────────────────
            // Recorre cada escena y empareja los bloques condicionales
            // usando una pila. Si -> salta a Sino o FinSi (si falso).
            // Sino -> salta a FinSi (al terminar la rama true).
            for (auto& [sceneName, scene] : result.scenes)
            {
                resolveConditionals(scene.cmds, sceneName);
            }

            result.valid = true;
            std::cout << "[EclipScript] OK - "
                      << result.scenes.size() << " escena(s), "
                      << result.characters.size() << " personaje(s) definidos."
                      << std::endl;

            return result;
        }

        // ── Convierte string de posición al enum ──
        static VNPosition parsePosition(const std::string& s)
        {
            std::string sl = toLower(s);
            if (sl == "izquierda"  || sl == "left")    return VNPosition::Izquierda;
            if (sl == "centro_izq" ||
                sl == "centro_izquierda" ||
                sl == "center_left")                    return VNPosition::CentroIzq;
            if (sl == "centro_der" ||
                sl == "centro_derecha" ||
                sl == "center_right")                   return VNPosition::CentroDer;
            if (sl == "derecha"    || sl == "right")    return VNPosition::Derecha;
            return VNPosition::Centro; // Default (centro/center)
        }

        // ── X virtual para cada posición (basado en 1920px) ──
        static float positionToX(VNPosition pos, float virtualW)
        {
            switch (pos)
            {
                case VNPosition::Izquierda:  return virtualW * 0.18f;
                case VNPosition::CentroIzq:  return virtualW * 0.33f;
                case VNPosition::Centro:     return virtualW * 0.50f;
                case VNPosition::CentroDer:  return virtualW * 0.67f;
                case VNPosition::Derecha:    return virtualW * 0.82f;
            }
            return virtualW * 0.50f;
        }

    private:

        // ── Resolver bloques si/sino/fin_si en una lista de comandos ──────
        // Usa una pila para emparejar bloques anidados.
        //   Si   → jumpTarget apunta a Sino (si existe) o FinSi
        //   Sino → jumpTarget apunta a FinSi
        //   FinSi → no necesita jumpTarget
        static void resolveConditionals(std::vector<EclipCmd>& cmds,
                                         const std::string& sceneName)
        {
            // Pila de índices de comandos Si pendientes de resolver
            std::vector<int> siStack;

            for (int i = 0; i < (int)cmds.size(); ++i)
            {
                switch (cmds[i].type)
                {
                case EclipCmdType::Si:
                    siStack.push_back(i);
                    break;

                case EclipCmdType::Sino:
                    if (siStack.empty())
                    {
                        std::cerr << "[EclipScript] AVISO escena '"
                                  << sceneName << "' linea "
                                  << cmds[i].sourceLine
                                  << ": 'sino' sin 'si'" << std::endl;
                        break;
                    }
                    {
                        // El Si más reciente salta aquí+1 si la condición es falsa
                        // (entrará en la rama sino)
                        int siIdx = siStack.back();
                        cmds[siIdx].jumpTarget = i + 1; // Salta DESPUÉS del sino
                        // Ahora el sino necesita saber dónde está su fin_si
                        // Lo reemplazamos en la pila
                        siStack.back() = i; // Ahora rastreamos el sino
                    }
                    break;

                case EclipCmdType::FinSi:
                    if (siStack.empty())
                    {
                        std::cerr << "[EclipScript] AVISO escena '"
                                  << sceneName << "' linea "
                                  << cmds[i].sourceLine
                                  << ": 'fin_si' sin 'si'" << std::endl;
                        break;
                    }
                    {
                        int topIdx = siStack.back();
                        siStack.pop_back();

                        // Si topIdx es un Si (sin sino) → salta al fin_si+1
                        // Si topIdx es un Sino → el sino salta al fin_si+1
                        cmds[topIdx].jumpTarget = i + 1;
                    }
                    break;

                default:
                    break;
                }
            }

            // Si quedaron Si sin cerrar
            if (!siStack.empty())
            {
                for (int idx : siStack)
                {
                    std::cerr << "[EclipScript] AVISO escena '"
                              << sceneName << "' linea "
                              << cmds[idx].sourceLine
                              << ": 'si' sin 'fin_si'" << std::endl;
                    // Fallback: salta al final de la escena
                    cmds[idx].jumpTarget = (int)cmds.size();
                }
            }
        }

        // `mutar-cp0`: el cuerpo se mudó a `eclips::trimBlancos` (EclipsParse.h)
        // para que una ley de `core/` pueda repartir `var op valor` sin
        // replicarlo. Esto queda como reenvío: los ~200 call-sites de adentro
        // de la clase no se movieron.
        static std::string trim(const std::string& s)
        {
            return ::eclips::trimBlancos(s);
        }

        // Delega en el fold ASCII-only de §1.45 (`EclipsParse.h`). El
        // `std::transform(..., ::tolower)` que vivia aca era la recaida exacta
        // de ese gotcha: pasar un `char` negativo a `::tolower` es UB, y con
        // otra locale corrompe UTF-8 byte a byte — y esta funcion normaliza las
        // CLAVES de `parseKeyValueParams`, entre ellas `tamaño`. Con la locale
        // "C" por defecto de MSVC no era dano observable (medido: `tamaño`
        // entraba y salia identica), pero era la fuente del C4244 que ensuciaba
        // el build de todo harness que incluya este header. Ficha (11).
        static std::string toLower(std::string s)
        {
            return asciiToLower(std::move(s));
        }

        // ── SYN-4c-i: EL FOLD PARA VOCABULARIO DEL MOTOR ──────────────────
        //
        // `toLower` + el juego espanol. **NO es un reemplazo de `toLower`, y
        // la diferencia es la ley del lote**: solo se usa donde el token que
        // se compara es vocabulario del MOTOR (una clave `k=v`, una clave de
        // corchete), NUNCA donde el token es un nombre que el creador invento.
        //
        // Medido al abrir la sesion: este archivo tiene 22 `toLower`, y al
        // menos uno resuelve un nombre inventado (la receta de
        // `animar <alias> <receta>`). Bajar el fold a `toLower` habria hecho
        // que `Corrupcion` y `corrupcion` con enie fueran el MISMO nombre —
        // lo contrario de lo que el manual promete y de lo que Ni1 midio.
        // Por eso el cableado son DOS sitios elegidos a mano, no una linea en
        // la funcion de arriba. Los guardianes de eso viven en la seccion 6
        // de `harness_enie.cpp`.
        static std::string vocabLower(std::string s)
        {
            return vocabToLower(std::move(s));
        }

        static bool startsWith(const std::string& s, const std::string& prefix)
        {
            return s.size() >= prefix.size() &&
                   s.substr(0, prefix.size()) == prefix;
        }

        // ── ¿Es una palabra de posición conocida? ──
        static bool isPositionWord(const std::string& s)
        {
            std::string sl = toLower(s);
            return sl == "izquierda"  || sl == "centro_izq" ||
                   sl == "centro_izquierda" || sl == "centro" ||
                   sl == "centro_der" || sl == "centro_derecha" ||
                   sl == "derecha"    ||
                   sl == "left"       || sl == "center_left" ||
                   sl == "center"     || sl == "center_right" ||
                   sl == "right";
        }

        // Extrae el contenido de la primera cadena entre comillas.
        // `mutar-cp0`: cuerpo mudado a `eclips::extraerEntreComillas`
        // (EclipsParse.h). Reenvío — mismos 15 call-sites, misma conducta.
        static bool extractQuoted(const std::string& s, std::string& out)
        {
            return ::eclips::extraerEntreComillas(s, out);
        }

        // Extractor GLOTON: el contenido es todo lo que hay entre la PRIMERA
        // y la ULTIMA comilla; las comillas internas se muestran literales.
        // Solo para dialogo/narrador, donde el resto de la linea ES contenido
        // que lee el jugador (un personaje puede citar a otro: "Dijo "hola"").
        // NO usar donde haya params despues del cierre (fondo/texto/etc): esos
        // siguen con extractQuoted (primer par), que respeta el token corto.
        // Devuelve false con 0 o 1 comilla (impar = sin cerrar) -> el caller
        // avisa. El literal vacio "" es valido (q1 != q2) y devuelve true.
        static bool extractQuotedGreedy(const std::string& s, std::string& out)
        {
            size_t q1 = s.find('"');
            if (q1 == std::string::npos) return false;
            size_t q2 = s.rfind('"');
            if (q2 == q1) return false;   // una sola comilla -> desbalanceada
            out = s.substr(q1 + 1, q2 - q1 - 1);
            return true;
        }

        static void warn(int line, const std::string& msg)
        {
            std::cerr << "[EclipScript] AVISO linea " << line << ": " << msg << std::endl;
        }

        // ── Parsear parámetros key=value de capa/hotspot ────────────────
        // Formato: z=100 x=200 y=600 w=1520 h=400
        // Acepta tokens separados por espacio con formato key=value.
        // Retorna un mapa de key → value (string).
        static std::map<std::string, std::string> parseKeyValueParams(
            const std::string& rest,
            std::vector<std::string>* repetidas = nullptr)
        {
            std::map<std::string, std::string> result;
            std::istringstream iss(rest);
            std::string token;
            while (iss >> token)
            {
                size_t eq = token.find('=');
                if (eq != std::string::npos && eq > 0 && eq < token.size() - 1)
                {
                    // PUERTA 2 de 3 del fold (SYN-4c-i): una clave `k=v` es
                    // vocabulario del motor, asi que `TAMANO=` con enie
                    // mayuscula baja y matchea. Ver `vocabLower`.
                    std::string key = vocabLower(token.substr(0, eq));
                    std::string val = token.substr(eq + 1);
                    // SYN-1c-ii: GANA LA PRIMERA. El `result[key] = val` que
                    // vivia aca dejaba ganar la ULTIMA, muda — y esa mudez
                    // fue lo que volvio DESTRUCTIVO al comentario inline: un
                    // `w=999` escrito dentro del comentario pisaba el `w=100`
                    // real (§7.320). Ahora el choque tiene voz, y la misma
                    // ley que el choque de grafias: gana el primero.
                    if (result.count(key))
                    {
                        if (repetidas) repetidas->push_back(key);
                        continue;
                    }
                    result[key] = val;
                }
            }
            return result;
        }

        // ── SYN-1c-ii: el mapa `k=v` YA REVISADO Y CANONIZADO ──────────────
        //
        // La puerta UNICA de los nueve comandos con vocabulario. En orden:
        //
        //   1. tokeniza `k=v` por espacios (que un VALOR no pueda llevar
        //      espacios entre comillas sigue siendo 1c-iii, no esto);
        //   2. desempata el choque de grafias del mismo parametro — gana la
        //      PRIMERA del grupo — y lo AVISA;
        //   3. avisa la clave que el comando no conoce, con lista de ayuda,
        //      y la descarta. Ley 1a: un campo opcional malo descarta el
        //      VALOR, no el comando; matar la linea es para el argumento
        //      requerido, y eso ya lo firmo SYN-2 (§7.319);
        //   4. devuelve el mapa con UNA sola grafia por parametro, la
        //      canonica — por eso los lectores de abajo ya no preguntan por
        //      el alias EN.
        //
        // El desempate se resuelve recorriendo LA TABLA, no el mapa: el mapa
        // esta ordenado alfabeticamente y su orden no significa nada (por eso
        // "gana la primera" no puede querer decir "la primera que escribiste"
        // — esa informacion ya se perdio en el tokenizador, y prometerla
        // seria mentir).
        //
        // SYN-4c-ii: y devuelve un `KvMap`, no un mapa pelado — el valor
        // canonizado viaja con el recibo de QUE GRAFIA lo escribio, que es lo
        // unico que el aviso necesitaba y lo unico que el paso 4 tiraba.
        static KvMap kvParams(
            const std::string& rest, EclipCmdType tipo,
            int lineNum, const char* cmdName)
        {
            std::vector<std::string> repetidas;
            const std::map<std::string, std::string> crudo =
                parseKeyValueParams(rest, &repetidas);

            for (const auto& k : repetidas)
                warn(lineNum, std::string(cmdName) + ": '" + k +
                    "=' esta escrito dos veces en la misma linea. Se usa el "
                    "primero.");

            KvMap out;
            std::set<std::string> consumidas;

            for (const auto& grupo : kvKeysOf(tipo))
            {
                std::vector<std::string> presentes;
                for (const auto& g : grupo)
                    if (crudo.count(g)) presentes.push_back(g);
                if (presentes.empty()) continue;

                out.vals[grupo[0]]    = crudo.at(presentes[0]);
                // El recibo de G2. `presentes[0]` es la que gano el desempate,
                // que es tambien la unica que el motor va a usar: nombrar
                // cualquier otra en el aviso seria hablar de un valor que se
                // descarto.
                out.grafias[grupo[0]] = presentes[0];
                for (const auto& g : presentes) consumidas.insert(g);

                if (presentes.size() > 1)
                {
                    std::string lista;
                    for (size_t i = 0; i < presentes.size(); ++i)
                        lista += (i ? ", '" : "'") + presentes[i] + "='";
                    warn(lineNum, std::string(cmdName) + ": " + lista +
                        " son el mismo parametro escrito de varias maneras. "
                        "Se usa '" + presentes[0] + "=" +
                        crudo.at(presentes[0]) + "'.");
                }
            }

            for (const auto& [k, v] : crudo)
            {
                (void)v;
                if (!consumidas.count(k))
                    warn(lineNum, std::string(cmdName) +
                        ": no conozco el parametro '" + k +
                        "='. Parametros: " + kvKeysHelp(tipo) + ".");
            }

            return out;
        }

        // ── SYN-3: LA PUERTA UNICA DE LOS CORCHETES ───────────────────────
        //
        // Gemela de `kvParams`, para `[clave: valor, clave: valor]`. Hace lo
        // mismo y en el mismo orden:
        //
        //   1. tokeniza por comas RESPETANDO COMILLAS (`eclips::splitQuoteAware`
        //      — eso es 1c-iii, que la ficha mandaba aca);
        //   2. parte cada token por el primer `:` FUERA de comillas (el valor
        //      puede llevar los suyos: `default: "Tu nombre:"` es legitimo);
        //   3. canoniza la grafia y desempata: gana la PRIMERA del grupo, y
        //      avisa igual — la misma frase para las dos formas del choque
        //      (dos grafias, o la misma clave repetida);
        //   4. la clave que el comando no conoce se descarta CON VOZ y con la
        //      lista de ayuda, y el comando SIGUE VIVO (ley 1a: muere el
        //      valor, no el comando; matar el comando es para el argumento
        //      requerido, y eso lo firmo SYN-2, §7.319).
        //
        // Las banderas (`[requerido]`) son claves sin `:`. Van en la MISMA
        // tabla con un bit, no en una lista aparte: son vocabulario del mismo
        // comando, y separarlas habria dado dos lugares donde olvidarse de
        // agregar una palabra.
        //
        // La clave se compara en minusculas, igual que `parseKeyValueParams`
        // (que hace `toLower` desde siempre). Las dos mini-gramaticas comparten
        // la ley: si `ANCHO=` funciona en un `k=v`, `[TIEMPO: 5]` funciona aca.
        // SYN-4c-ii: devuelve el MISMO `KvMap` que su gemela `kvParams`. Que el
        // tipo se reuse tal cual, sin una linea de adaptacion, es la senal de
        // que la ley es una sola y no dos parecidas: las dos mini-gramaticas
        // canonizan en la puerta, y las dos tienen que acordarse de que grafia
        // entro. (El corchete era el caso mas facil de dejar afuera, porque su
        // unico aviso con clave no MENTIA — nombraba las dos, `timer/tiempo:`.
        // Pero nombrar las dos sigue siendo nombrarte una que no escribiste.)
        static KvMap bracketParams(
            const std::string& params, EclipCmdType tipo,
            int lineNum, const char* cmdName)
        {
            // ── 1 + 2: tokenizar y partir clave/valor ──
            std::map<std::string, std::string> crudo;   // grafia cruda -> valor
            std::vector<std::string> orden;             // para desempatar
            std::vector<std::string> repetidas;

            for (const std::string& bruto : eclips::splitQuoteAware(params, ','))
            {
                const std::string token = trim(bruto);
                if (token.empty()) continue;

                // El primer `:` FUERA de comillas separa clave de valor.
                size_t sep = std::string::npos;
                bool enComillas = false;
                for (size_t i = 0; i < token.size(); ++i)
                {
                    if (token[i] == '"') { enComillas = !enComillas; continue; }
                    if (token[i] == ':' && !enComillas) { sep = i; break; }
                }

                // PUERTA 3 de 3 del fold (SYN-4c-i). Ninguna clave de corchete
                // lleva un byte no-ASCII HOY, asi que esta linea no cambia una
                // sola conducta viva — va igual porque las dos mini-gramaticas
                // comparten la ley por contrato (la nota de arriba: "si
                // `ANCHO=` funciona en un `k=v`, `[TIEMPO: 5]` funciona aca").
                // Dejarla afuera seria firmar que se separan el dia que
                // alguien agregue una clave con enie.
                const std::string key = vocabLower(trim(
                    sep == std::string::npos ? token : token.substr(0, sep)));
                const std::string val = (sep == std::string::npos)
                    ? std::string() : trim(token.substr(sep + 1));
                if (key.empty()) continue;

                if (crudo.count(key)) { repetidas.push_back(key); continue; }
                crudo[key] = val;
                orden.push_back(key);
            }

            for (const auto& k : repetidas)
                warn(lineNum, std::string(cmdName) + ": '" + k +
                    ":' esta escrito dos veces en el mismo corchete. Se usa el "
                    "primero.");

            // ── 3: canonizar y desempatar (recorriendo LA TABLA, como kvParams) ──
            KvMap out;
            std::set<std::string> consumidas;

            for (const auto& bk : bracketKeysOf(tipo))
            {
                std::vector<std::string> presentes;
                for (const auto& g : bk.grafias)
                    if (crudo.count(g)) presentes.push_back(g);
                if (presentes.empty()) continue;

                out.vals[bk.grafias[0]]    = crudo.at(presentes[0]);
                out.grafias[bk.grafias[0]] = presentes[0];   // el recibo (G2)
                for (const auto& g : presentes) consumidas.insert(g);

                if (presentes.size() > 1)
                {
                    std::string lista;
                    for (size_t i = 0; i < presentes.size(); ++i)
                        lista += (i ? ", '" : "'") + presentes[i] + ":'";
                    warn(lineNum, std::string(cmdName) + ": " + lista +
                        " son el mismo parametro escrito de varias maneras. "
                        "Se usa '" + presentes[0] + ":" +
                        crudo.at(presentes[0]) + "'.");
                }
            }

            // ── 4: la clave desconocida habla ──
            for (const auto& k : orden)
            {
                if (consumidas.count(k)) continue;

                std::string ayuda = bracketKeysHelp(tipo);
                std::string extra;

                // El qte NO lleva `default:`, y su aviso no puede ser generico:
                // su default ya existe y se llama `fallo`. Una decision deja
                // pedagogia; un olvido deja un error generico (§7.44 / SYN-4a).
                if (tipo == EclipCmdType::Qte && k == "default")
                    extra = " Un qte no necesita default: al acabarse el tiempo "
                            "va a su `fallo`.";

                if (ayuda.empty())
                    warn(lineNum, std::string(cmdName) + ": no conozco el "
                        "parametro '" + k + ":' (este comando no lleva "
                        "parametros entre corchetes)." + extra);
                else
                    warn(lineNum, std::string(cmdName) + ": no conozco el "
                        "parametro '" + k + ":'. Parametros: " + ayuda + "." + extra);
            }

            return out;
        }

        // ── SYN-4a: LA PUERTA ÚNICA DEL ID ────────────────────────────────
        //
        // **Un id que el creador inventa va SIN comillas.** La comilla dice
        // "esto es un literal" —texto que se ve, o una ruta de archivo—; un id
        // no lo es. Si las dos formas valieran, la comilla dejaría de llevar
        // información: eso no amplía el idioma, le quita el significado al signo.
        //
        // NO es una ley nueva. §7.44 ya la había firmado como ley general del
        // motor *y había fichado a sus propias excepciones como deuda*:
        //
        //   "La grieta legada de capa_z/capa_visible (id con comillas) queda
        //    como deuda fichada (derivada #5), no como contraejemplo: SON LOS
        //    QUE ESTÁN MAL, NO LA REGLA."
        //
        // Esto la cobra. Hasta hoy había CUATRO leyes para la misma categoría,
        // y el criterio del reparto era CRONOLÓGICO, no semántico:
        //
        //   · toleraban con aviso (nacidos antes de §7.42): capa, capa_z,
        //     capa_visible, hotspot — 64 líneas del corpus sobre la forma vieja;
        //   · rechazaban (nacidos después): capa_fade, capa_mover, enlace,
        //     control, ranura — `enlace` lo dice en su propio comentario,
        //     "Nuevo, sin legado -> RejectHard (como capa_fade)";
        //   · la EXIGÍA: evento_random, y no por decisión — su parser nunca
        //     recibió el retrofit de la tabla y sacaba el nombre con un
        //     `find('"')` crudo. La evidencia de que fue omisión y no firma:
        //     `capa_fade` rechaza CON PEDAGOGÍA y esto sólo decía "falta
        //     nombre" (un error genérico). Una decisión deja pedagogía;
        //     un olvido deja un error genérico;
        //   · se la tragaban MUDOS: `consola crear` (el id quedaba con las
        //     comillas adentro) y `texto "id" "contenido"` (el segundo literal
        //     se caía sin voz en `parseKeyValueParams` — un token sin `=` se
        //     descarta— y el motor dibujaba el ID).
        //
        // La frontera con §7.322-323, que es lo que hace que las dos leyes
        // convivan: **la tolerancia se le concede a un SINÓNIMO, nunca a un
        // MARCADOR DE TIPO.** `ancho=`/`width=` son dos maneras de decir lo
        // mismo, el choque se escribe en una línea y hay algo que desempatar
        // (gana la primera, y avisa). `"pizarra"` vs `pizarra` no son dos
        // maneras de decir lo mismo, y el choque ni se puede escribir: no hay
        // nada que desempatar, hay que decidir qué SIGNIFICA el signo.
        //
        // Devuelve false si el id venía citado → el caller NO emite.
        // `rest` se consume: sale sin el id y sin el espacio que lo seguía.
        static bool tomarId(std::string& rest, std::string& outId,
                            const std::string& cmdName, int lineNum)
        {
            if (!rest.empty() && rest[0] == '"')
            {
                // UNA sola frase para los doce comandos. El núcleo —"el id va
                // SIN comillas"— es lo que el harness clava; el resto es
                // pedagogía y se puede pulir sin volverlo rojo.
                warn(lineNum, cmdName + ": el id va SIN comillas (ej: " +
                    cmdName + " nombre ...). Las comillas son para texto que se "
                    "ve o rutas de archivo, no para nombres.");
                return false;
            }
            const size_t sp = rest.find(' ');
            outId = rest.substr(0, sp);
            rest  = (sp != std::string::npos) ? trim(rest.substr(sp + 1))
                                              : std::string();
            return true;
        }

        // ── Leer un parámetro NUMÉRICO de un mapa k=v (SYN-1a) ─────────────
        //
        // Reemplaza el idioma que estaba copiado ~45 veces en este archivo:
        //
        //     try { if (kv.count("x")) cmd.layerX = std::stof(kv["x"]); } catch (...) {}
        //
        // Dos agujeros por línea: `x=perro` caía al default **mudo**, y
        // `x=1.5abc` entraba como 1.5 (el prefijo numérico se aceptaba como
        // éxito). La ley vive centralizada en `EclipsParse.h`; acá arriba solo
        // se le agrega la VOZ, que es lo que este archivo tiene y el helper
        // no: el número de línea del creador.
        //
        // Contrato (ley 1a de SYN-1a):
        //   clave ausente        → false, `out` intacto, SIN voz (un parámetro
        //                          opcional que nadie escribió no es un error);
        //   clave con basura     → false, `out` INTACTO, y AVISO con línea;
        //   clave con valor sano → true, `out` escrito.
        //
        // El comando SE SIGUE EMITIENDO: descartar el valor no es descartar el
        // comando. Fail-closed es el gate de SYN-2, y no se adelanta acá.
        //
        // SYN-4c-ii: `key` sigue siendo la CANONICA — es la que busca en el
        // mapa, y es lo que hace que los 32 call-sites no se hayan tocado. Lo
        // que cambio es la boca: el aviso cita `kv.escrita(key)`.
        static bool kvFloat(const KvMap& kv,
                            const std::string& key, float& out,
                            int lineNum, const char* cmdName)
        {
            auto it = kv.find(key);
            if (it == kv.end()) return false;

            float v = out;
            if (parseFloatStrict(trim(it->second), v) == NumParse::Ok)
            {
                out = v;
                return true;
            }
            if (!trim(it->second).empty())
                warn(lineNum, std::string(cmdName) + ": " + kv.escrita(key) +
                    "= espera un numero, recibio '" + it->second +
                    "'. Se ignora.");
            return false;
        }

        // Gemelo entero. La ley 3a vive acá: `z=1.5` NO vale 1 — el `.5`
        // sobrante delata que el creador quiso decir otra cosa, y el aviso se
        // lo dice con esas palabras en vez de truncar en silencio.
        static bool kvInt(const KvMap& kv,
                          const std::string& key, int& out,
                          int lineNum, const char* cmdName)
        {
            auto it = kv.find(key);
            if (it == kv.end()) return false;

            int v = out;
            if (parseIntStrict(trim(it->second), v) == NumParse::Ok)
            {
                out = v;
                return true;
            }
            // La ley de G2 se cablea DOS veces porque las dos familias tienen
            // frases distintas. Hoy no hay clave entera bilingue (`z` es
            // monolingue en los tres comandos que la llevan), asi que esta
            // linea no cambia una sola conducta viva — va igual, porque el dia
            // que exista una la alternativa es descubrirlo por un aviso raro.
            if (!trim(it->second).empty())
                warn(lineNum, std::string(cmdName) + ": " + kv.escrita(key) +
                    "= espera un entero (sin decimales), recibio '" +
                    it->second + "'. Se ignora.");
            return false;
        }

        // ── SYN-4c-ii: EL PAR ES/EN DE LA MINI-GRAMATICA DE `animar` ──────
        //
        // `animar`/`animacion` NO pasan por `kvParams`: arman su mapa a mano
        // (tokenizan y despues resuelven el efecto, porque QUE claves son
        // validas depende del efecto, que es posicional y llega despues).
        // Por eso quedaron fuera del barrido de 1c-ii, y por eso dos de sus
        // claves seguian resolviendo el alias LEYENDO EL MAPA DOS VECES:
        //
        //     auto itE = kv.find("en");
        //     if (itE == kv.end()) itE = kv.find("in");
        //
        // Eso tiene los DOS agujeros que §7.323 legislo, no uno:
        //   · el aviso citaba `en=` aunque el creador hubiera escrito `in=`;
        //   · y el choque (`en=1 in=2`) era **mudo** — el orden de dos lineas
        //     de C++ decidia quien ganaba, que es exactamente la ley que nadie
        //     firmo y que `kvParams` cerro para sus nueve comandos.
        //
        // La conducta NO cambia: gana la primera grafia del par, que aca es la
        // ESPANOLA, igual que `grafias[0]` en la tabla. Lo unico que cambia es
        // que deja de ser un accidente y pasa a ser una ley con voz — y la
        // frase es LA MISMA que la de `kvParams`, palabra por palabra: una ley,
        // una oracion (§7.323).
        struct FxPar
        {
            bool        hay = false;
            std::string valor;
            std::string escrita;   // la grafia que el creador tecleo
        };

        static FxPar fxTomarPar(const std::map<std::string, std::string>& m,
                                const char* canon, const char* alias,
                                int lineNum, const std::string& ctx)
        {
            FxPar r;
            r.escrita = canon;

            auto itC = m.find(canon);
            auto itA = m.find(alias);

            if (itC != m.end() && itA != m.end())
                warn(lineNum, ctx + ": '" + canon + "=', '" + alias +
                    "=' son el mismo parametro escrito de varias maneras. "
                    "Se usa '" + canon + "=" + itC->second + "'.");

            if (itC != m.end())
            {
                r.hay = true; r.valor = itC->second; r.escrita = canon;
            }
            else if (itA != m.end())
            {
                r.hay = true; r.valor = itA->second; r.escrita = alias;
            }
            return r;
        }

        // ── SYN-3: ¿ESTA LINEA ES UNA OPCION DE BLOQUE? ───────────────────
        //
        // >>> LA FLECHA DE UNA OPCION VA **FUERA** DE LAS COMILLAS.
        // >>> LA DE UNA NARRACION VA **ADENTRO**.
        //
        // Es la misma ley de 1c-iii (`splitQuoteAware`) aplicada al `->`, y no
        // es cosmetica: el look-ahead viejo preguntaba
        // `nextTrim.find("->") != npos` sobre la linea ENTERA, asi que una
        // narracion legitima se comia como opcion. Caso VIVO a una linea de
        // distancia — `vnscript/test_hotreload.ecl:18`:
        //
        //   "El 'fantasma' no salio (su PNG no existe). Crealo en disco -> F8
        //    -> debe aparecer."
        //
        // vive tres renglones debajo de un `esperar_click`, y solo la salva que
        // el `mostrar` de la linea 17 corta el bloque primero. Sin esa linea,
        // hoy se convertia en un mapeo con label de parrafo entero y destino
        // `F8 -> debe aparecer."`. En silencio.
        //
        // ── LA MEDICION QUE FIJO LA REGLA (y que tumbo mi primera version) ──
        //
        // 605 lineas del corpus empiezan con comilla:
        //
        //   · 140 tienen cola despues de la ultima comilla, y las 140 empiezan
        //     con `->`  ......................................  son opciones;
        //   ·   0 tienen cola que NO sea una flecha  ......  cero ambiguedad;
        //   · 465 no tienen cola  ........................  son narracion pura.
        //
        // Mi primera version de esta ley decia "una linea citada tras el bloque
        // avisa y SE DESCARTA". Con la medicion en la mano eso habria BORRADO
        // esas 465 si alguna quedaba pegada a un bloque. La cola es el
        // discriminador, no la posicion.
        //
        // Y las 140 tienen EXACTAMENTE dos comillas, asi que tomar la ultima
        // como frontera da el mismo label que el `extractQuoted` viejo para
        // todas: cero conducta cambiada donde ya funcionaba.
        //
        // `quisoSerOpcion` sale true cuando hay cola pero no es flecha: eso ya
        // no es narracion (una narracion no deja basura despues del cierre),
        // es una opcion a la que le falta el `->`.
        static bool esOpcionDeBloque(const std::string& linea,
                                     std::string& outLabel,
                                     std::string& outTarget,
                                     bool& quisoSerOpcion)
        {
            quisoSerOpcion = false;
            if (linea.empty() || linea[0] != '"') return false;

            const size_t ultima = linea.rfind('"');
            if (ultima == 0) return false;      // comilla sin cerrar

            const std::string cola = trim(linea.substr(ultima + 1));
            if (cola.empty()) return false;     // narracion pura

            if (cola.size() < 2 || cola[0] != '-' || cola[1] != '>')
            {
                quisoSerOpcion = true;          // cola que no es flecha
                return false;
            }

            outLabel  = linea.substr(1, ultima - 1);
            outTarget = trim(cola.substr(2));
            return true;
        }

        // ── `D-33` · LA FORMA PEGADA: `"linea" elegir:` ────────────────────
        //
        // Son un beat porque son un renglón: la sintaxis no lo declara, lo ES.
        // Extiende el discriminador de cola de SYN-3 (de acá arriba) en vez de
        // invertirlo, y por eso el diseño entero no inventa una palabra nueva.
        //
        // ── POR QUE NO SE PARTE POR LA ULTIMA COMILLA ────────────────────────
        //
        // Lo obvio sería `rfind('"')`, igual que `esOpcionDeBloque`. Medido
        // sobre el corpus, eso se rompe en 11 sitios vivos — uno es
        // `vnscript/intro.ecl:140`, contenido del fundador:
        //
        //     "linea" elegir [timer: 5s, default: "Quedarse quieto"]:
        //                                         ^^^^^^^^^^^^^^^^^
        // La última comilla es la del DEFAULT, no la que cierra el texto, así
        // que partir por ahí daría un texto de `linea" elegir [... "Quedarse`.
        // Allá `rfind` es correcto porque las 140 opciones del corpus tienen
        // EXACTAMENTE dos comillas; acá la cola trae las suyas.
        //
        // Por eso el ancla es el FINAL del renglón y se retrocede:
        // `:` → `[...]` opcional → la palabra → y recién ahí la comilla que
        // cierra el literal. De yapa, retroceder respeta la ley greedy
        // (`m: "el dijo "hola" y ya"`), que un toggle desde la izquierda
        // rompería — ver la arista de `EclipsParse.h`.
        //
        // Devuelve la CABEZA sin tocar (`m: "hola"` o `"hola"`), que es lo que
        // el parser de línea ya sabe leer, y los parámetros del corchete tal
        // cual, que es lo que `armarElegir` ya sabe leer. Cero código nuevo del
        // otro lado de la costura.
        static bool separarElegirPegado(const std::string& linea,
                                        std::string& outCabeza,
                                        std::string& outParams)
        {
            outCabeza.clear();
            outParams.clear();

            std::string s = trim(linea);
            if (s.empty() || s[s.size() - 1] != ':') return false;
            s.erase(s.size() - 1);              // el `:` que abre el bloque
            s = trim(s);

            // El corchete es opcional. `rfind` y no `find`: el TEXTO puede
            // llevar corchetes adentro (`"tomo la [llave]" elegir [tiempo: 5s]:`)
            // y el que importa es el último.
            if (!s.empty() && s[s.size() - 1] == ']')
            {
                const size_t abre = s.rfind('[');
                if (abre == std::string::npos) return false;
                outParams = s.substr(abre + 1, s.size() - abre - 2);
                s = trim(s.substr(0, abre));
            }

            // La palabra, COMO PALABRA: `...elegirfoo:` no cuenta. Es la misma
            // frontera de `matchKeyword`, mirada del otro lado.
            const char* kws[] = { "elegir", "choose" };
            bool hallado = false;
            for (const char* kw : kws)
            {
                const size_t n = std::strlen(kw);
                if (s.size() <= n) continue;
                if (s.compare(s.size() - n, n, kw) != 0) continue;
                const char anterior = s[s.size() - n - 1];
                if (anterior != ' ' && anterior != '\t') continue;
                s = trim(s.substr(0, s.size() - n));
                hallado = true;
                break;
            }
            if (!hallado) return false;

            // Y lo que queda tiene que SER una línea citada: cierra con comilla
            // y hay otra que la abre. Sin esto, `escena x elegir:` entraría acá.
            if (s.size() < 2 || s[s.size() - 1] != '"') return false;
            if (s.find('"') == s.size() - 1)          return false;

            outCabeza = s;
            return true;
        }

        // ── `D-34` · EL BORDE, Y EL MOTOR LO DICE ───────────────────────────
        //
        // Hoy `elegir` es el ÚNICO comando que puede ir pegado a una línea.
        // El riesgo no es de mecánica: es que el creador pruebe
        // `"linea" sonido "x.ogg"` SUPONIENDO que también anda. Eso no es
        // dificultad, es inconsistencia — y sin esta voz, sale mudo.
        //
        // La lista se sacó del despacho VIVO (los 68 `startsWith`, los 6
        // `matchKeyword`, la tabla de `fondo`/`musica`/`sonido` y el `esperar`
        // de `substr`), no de la memoria de nadie. **Sólo se usa para AVISAR**,
        // así que su modo de falla es quedarse corta —silencio, o sea la
        // conducta de hoy—, nunca mentir sobre lo que el motor ejecuta.
        static bool esComandoDelMotor(const std::string& token)
        {
            static const char* kComandos[] = {
                "animacion", "animar", "animate", "animation", "azar_semilla",
                "azar_traza", "background", "capa", "cargar_script",
                "cargar_tema", "character", "choose", "composite_character",
                "consola_borrar", "consola_escribir", "consola_imprimir",
                "consola_limpiar", "console_clear", "console_erase",
                "console_print", "console_write", "deshabilitar",
                "detener_animacion",
                "detener_eventos", "disable", "efecto_lua", "ejecutar_lua",
                "elegir",
                "else", "enable", "end", "end_if", "entrada", "escena", "esperar",
                "esperar_animacion", "esperar_click", "esperar_interfaz",
                "evento_random", "fin",
                "fin_si", "fondo", "font", "fuente", "gameplay", "habilitar",
                "go_to_corridor", "goto", "hide", "hide_box",
                "highlight_speaker", "if", "iniciar_batalla", "iniciar_eventos",
                "input", "ir_a", "ir_a_pasillo", "layer", "load_script",
                "load_theme", "lua", "lua_effect", "meta_borrar", "meta_reset",
                "meta_mutar", "meta_set", "mostrar", "mostrar_caja", "music",
                "musica", "mutar",
                "ocultar", "ocultar_caja", "palabras", "parallax",
                "parar_musica",
                "personaje", "personaje_compuesto", "qte", "random_event",
                "random_seed", "random_trace", "resaltar_hablante", "scene",
                "set", "show", "show_box", "si", "sino", "sonido", "sound",
                "start_battle", "start_events", "stop_animation", "stop_events",
                "stop_music", "text", "texto", "transicion", "transition",
                "usar_caja", "use_box", "vigneta", "vignette", "wait",
                "wait_animation", "wait_click", "wait_ui", "words",
            };
            for (const char* c : kComandos)
                if (token == c) return true;
            return false;
        }

        // Se llama SOLO sobre una línea que ya se decidió que NO es la forma
        // pegada. Recorre los tramos que quedan FUERA de las comillas y habla
        // si alguno arranca con un comando del motor, o si sobró cola al final.
        //
        // ── LA AMBIGUEDAD ES REAL, Y SE RESUELVE HABLANDO ───────────────────
        //
        // `"linea" sonido "x.ogg"` y `"el dijo "si" y ya"` son la MISMA forma:
        // un tramo de un solo token entre dos tramos citados. No hay estructura
        // que los separe — lo único que los separa es que `sonido` es un
        // comando del motor y `no mires atras` no lo es. La primera versión de
        // esto pedía además que el tramo trajera argumento, y con eso dejaba
        // mudo justo el caso que hay que cazar: el argumento de `sonido` está
        // en el tramo citado SIGUIENTE, no en éste. Lo destapó el assert 12.8.
        //
        // Así que el motor AVISA sobre la palabra, y el aviso dice cómo
        // ignorarlo si era prosa. Es la elección del fundador, textual: *"suena
        // a que el engine debería hacer ruido"*. Un renglón de consola de más
        // es más barato que un comando que no se ejecuta y no lo dice.
        //
        // Medido: sobre los 5 renglones del corpus con comillas anidadas
        // (`buenos dias`, `no mires atras`, `corre`) esto da CERO avisos.
        static void avisarPegadoInvalido(const std::string& linea, int lineNum)
        {
            std::vector<size_t> comillas;
            for (size_t p = 0; p < linea.size(); ++p)
                if (linea[p] == '"') comillas.push_back(p);
            if (comillas.size() < 2) return;

            // Los tramos de afuera arrancan en cada comilla de CIERRE (índices
            // impares) y terminan en la siguiente de apertura, o en el final.
            for (size_t k = 1; k < comillas.size(); k += 2)
            {
                const size_t ini = comillas[k] + 1;
                const bool   esCola = (k + 1 >= comillas.size());
                const size_t fin = esCola ? linea.size() : comillas[k + 1];
                const std::string fuera = trim(linea.substr(ini, fin - ini));
                if (fuera.empty()) continue;

                size_t largo = 0;
                while (largo < fuera.size() && isKeywordChar(fuera[largo]))
                    ++largo;
                const std::string token = fuera.substr(0, largo);

                // `elegir` pegado pero mal escrito: el diagnóstico útil es el
                // que le falta, no la regla general.
                if (token == "elegir" || token == "choose")
                {
                    warn(lineNum, "'" + token + "' esta pegado a la linea pero "
                        "al renglon le falta terminar en ':'. Escribi "
                        "\"tu linea\" " + token + ": y abajo las opciones "
                        "indentadas.");
                    return;
                }

                if (esComandoDelMotor(token))
                {
                    warn(lineNum, "'" + token + "' esta pegado a una linea de "
                        "texto y NO se ejecuta: hoy el unico comando que puede "
                        "ir en el mismo renglon que una linea es 'elegir'. " +
                        (esCola ? std::string("Lo pegado se descarta.")
                                : std::string("Lo pegado se esta leyendo como "
                                              "parte del texto.")) +
                        " Ponelo en su propio renglon. Y si '" + token +
                        "' era una palabra de tu texto entre comillas, ignora "
                        "este aviso.");
                    return;
                }

                if (esCola)
                {
                    warn(lineNum, "'" + fuera + "' quedo despues de la linea de "
                        "texto y se descarta: hoy el unico comando que puede ir "
                        "en el mismo renglon es 'elegir'.");
                    return;
                }
            }
        }

        // ── PIERNA B: EL CUERPO DE UN BLOQUE `azar:` ──────────────────────
        //
        //   azar [repetir=no]:
        //     peso=5   "no lo creo"
        //     peso=1 -> sala_rara
        //
        // ── POR QUE NO REUSA `esOpcionDeBloque`, QUE ES LO QUE EL CONTRATO
        //    DECIA (DISENO_AZAR_ECLIPS 4.2) ───────────────────────────────
        //
        // El contrato afirmaba que las dos formas del cuerpo eran "ya
        // discriminables por `esOpcionDeBloque`". Medido, NO lo son: esa
        // funcion arranca con `if (linea[0] != '"') return false` y exige
        // label citado Y flecha — o sea la forma de `elegir`, que trae las dos
        // cosas. Las entradas de `azar:` traen UNA U OTRA, y ademas empiezan
        // con `peso=`. Las dos dan `false` ahi adentro.
        //
        // Lo que SI se hereda de SYN-3 no es la funcion: es LA LEY. El
        // discriminador tiene que ser quote-aware, porque una entrada de texto
        // puede llevar `->` adentro — que es el bug vivo que SYN-3 caza en
        // `test_hotreload.ecl:18` (una narracion comida como opcion). Por eso
        // aca tampoco se pregunta `find("->")` sobre la linea entera.
        //
        // Un renglon roto AVISA y se descarta; el bloque SIGUE (ley de 1a en
        // el cuerpo: muere el renglon, no el comando).
        static void leerEntradasDeAzar(const std::vector<std::string>& lines,
                                       int& i,
                                       std::vector<EclipChoice>& out,
                                       const char* cmdName)
        {
            while (i + 1 < (int)lines.size())
            {
                const int childLine = i + 2;    // lineNum del hijo (i es 0-based)
                std::string nextTrim = trim(lines[i + 1]);

                if (nextTrim.empty() || nextTrim[0] == '#')
                {
                    ++i;    // vacias/comentarios dentro del bloque no cuentan
                    continue;
                }

                // ── El `peso=` se pela del FRENTE, a mano ──────────────────
                // NO puede ir por `parseKeyValueParams`: parte por espacios y
                // descarta MUDO todo token sin `=`, asi que `peso=5 "hola"`
                // perderia el texto en silencio. Es exactamente el bug que
                // SYN-4a cerro en `texto "id" "contenido"`, donde el segundo
                // literal se caia sin voz y el motor dibujaba el ID.
                EclipChoice entrada;
                bool  traeClave = false;    // la clave ESTABA, sana o podrida
                std::string resto = nextTrim;

                for (const char* clave : { "peso=", "weight=" })
                {
                    const size_t largo = std::strlen(clave);
                    if (resto.size() <= largo ||
                        toLower(resto.substr(0, largo)) != clave) continue;

                    const size_t corte = resto.find_first_of(" \t", largo);
                    const std::string valor = (corte == std::string::npos)
                        ? resto.substr(largo)
                        : resto.substr(largo, corte - largo);

                    float p = 1.0f;
                    if (parseFloatStrict(valor, p) == NumParse::Ok)
                    {
                        // Parseable pero fuera de rango: se CLAMPEA con voz,
                        // igual que `probabilidad=-5` en la ley 9. El 0 no es
                        // un castigo inventado — en este eje ya significa algo
                        // firmado: apagada, sin borrarla.
                        if (p < 0.0f)
                        {
                            warn(childLine, std::string(cmdName) + ": peso=" +
                                valor + " es negativo, y un peso negativo no "
                                "significa 'menos que nunca'. Se usa 0, que "
                                "apaga la entrada sin borrarla.");
                            p = 0.0f;
                        }
                        entrada.peso = p;
                    }
                    else
                    {
                        // No parseable: muere el VALOR, no el renglon (ley 1a).
                        // Cae al default, no a 0 — no sabemos que quiso decir,
                        // y apagarle la entrada en silencio seria decidir por
                        // el.
                        warn(childLine, std::string(cmdName) + ": peso= espera "
                            "un numero, recibio '" + valor + "'. Se usa 1.");
                    }

                    traeClave = true;
                    resto = (corte == std::string::npos)
                        ? std::string() : trim(resto.substr(corte));
                    break;
                }

                // ── LA CLAVE ES LO QUE HACE QUE UN RENGLON SEA UNA ENTRADA ──
                //
                // Sin `peso=`/`weight=` esto NO es del bloque: el bloque
                // termina acá y la línea la lee el guion normalmente.
                //
                // Firmado en mesa el 2026-07-28, y arreglando un bug que
                // introduje yo el mismo día: mi primera versión aceptaba un
                // `"texto"` pelado como entrada de peso 1, "por comodidad".
                // Con eso, **una narración escrita justo debajo del bloque es
                // indistinguible de una entrada**, y el motor se la comía y la
                // sorteaba. Lo destapó la sonda de aceptación: un bloque de
                // tres entradas trazaba `quedan 3 de 4`.
                //
                // Es la misma especie que SYN-3 cazó en `test_hotreload.ecl:18`,
                // INVERTIDA: allá una narración se comía como opción por un
                // `find("->")` flojo; acá se comía como entrada porque el texto
                // pelado era legal. La cura es la misma que usa `elegir`: una
                // entrada se reconoce por su FORMA, no por su posición.
                if (!traeClave) break;
                // (sigue el cuerpo de `leerEntradasDeAzar`)

                // ── Lo que queda es "texto" O `-> destino`, nunca las dos ──
                if (!resto.empty() && resto[0] == '"')
                {
                    const size_t ultima = resto.rfind('"');
                    if (ultima == 0)
                    {
                        ++i;
                        warn(childLine, std::string(cmdName) + ": '" + nextTrim +
                            "' abre comillas y no las cierra. Se ignora esa "
                            "linea.");
                        continue;
                    }

                    const std::string cola = trim(resto.substr(ultima + 1));
                    if (!cola.empty())
                    {
                        // La cola despues del cierre es la firma de `elegir`
                        // (`"label" -> destino`), y en un `azar:` una entrada
                        // es texto O salto, no las dos. Se dice con esas
                        // palabras en vez de tragarse una de las mitades.
                        ++i;
                        warn(childLine, std::string(cmdName) + ": una entrada "
                            "es un texto O un salto, no las dos. Escribi "
                            "peso=N \"tu texto\" o peso=N -> escena. Se ignora "
                            "esa linea.");
                        continue;
                    }

                    ++i;
                    entrada.label = resto.substr(1, ultima - 1);
                    out.push_back(entrada);
                    continue;
                }

                if (resto.size() >= 2 && resto[0] == '-' && resto[1] == '>')
                {
                    ++i;
                    entrada.target = trim(resto.substr(2));
                    if (entrada.target.empty())
                    {
                        warn(childLine, std::string(cmdName) + ": la flecha no "
                            "apunta a ninguna escena. Se ignora esa linea.");
                        continue;
                    }
                    out.push_back(entrada);
                    continue;
                }

                // Un `[` es un gate `[si:]`, que en las entradas de un `azar:`
                // NO existe todavia (la pierna C lo pone sobre el EVENTO, no
                // sobre la entrada). Se dice, en vez de tragarselo mudo.
                if (!resto.empty() && resto[0] == '[')
                {
                    ++i;
                    warn(childLine, std::string(cmdName) + ": las entradas no "
                        "llevan gate todavia. Sacale el corchete o pone el "
                        "gate en el evento. Se ignora esa linea.");
                    continue;
                }

                // Cualquier otra cosa NO es del bloque: es la linea siguiente
                // del guion, y el bloque termina acá sin comerse nada.
                break;
            }
        }

        // ── EL CUERPO DE UN BLOQUE `palabras` (MUTAR_PALABRAS CP1) ────────
        //
        //   palabras 10 repetir=no:
        //     "amor" natsuki                      # azucar de: -> natsuki + 1
        //     [si: acto == 1] "festival" sayori
        //     peso=0.01 "rara" -> rotura
        //
        // ── POR QUE NO REUSA A NINGUNO DE SUS DOS HERMANOS ────────────────
        //
        // `leerEntradasDeAzar` exige `peso=` en CADA entrada —la clave es lo
        // que hace que un renglon sea entrada— y aca el peso es opcional; y
        // `esOpcionDeBloque` exige FLECHA, que es justo lo que el azucar no
        // lleva. Lo que se hereda no son las funciones: es la ley, tres veces.
        //
        //   1. Los parametros van ADELANTE del texto, y en CUALQUIER orden
        //      (§7.523 + la firma del 2026-08-15). La parte que se come el
        //      resto del renglon —el texto y su accion— va ultima.
        //   2. Un renglon es entrada por su FORMA, no por su posicion.
        //   3. El discriminador es quote-aware: nunca se pregunta `find("->")`
        //      sobre la linea entera, porque un texto puede llevar una flecha
        //      adentro (el bug que SYN-3 caza en `test_hotreload.ecl:18`).
        //
        // ── DONDE TERMINA EL BLOQUE (fork C, firmado 2026-08-15) ──────────
        //
        // El azucar `"texto" cola` colisiona con dos formas VIVAS, y las dos
        // estan contadas: 17 lineas del corpus tienen forma `"texto" <cola>`,
        // y 16 son el `elegir` PEGADO de `D-33` (incluida `ddlc_aviso.ecl:82`,
        // contenido del fundador). Por eso el cierre mira la COLA:
        //
        //   · cola vacia            -> es una narracion. El bloque termina.
        //   · cola que termina en ':' -> es un `elegir` pegado. Termina.
        //
        // Sin esa regla, una narracion escrita debajo del pool se sortea como
        // si fuera una palabra — que es EXACTAMENTE el bug que `azar:` tuvo
        // durante un dia (2026-07-28) y que su mesa arreglo prohibiendo el
        // texto pelado. Aca no se puede prohibir: el azucar ES la feature.
        static void leerEntradasDePalabras(const std::vector<std::string>& lines,
                                           int& i,
                                           std::vector<EclipChoice>& out,
                                           const char* cmdName)
        {
            while (i + 1 < (int)lines.size())
            {
                const int childLine = i + 2;
                std::string nextTrim = trim(lines[i + 1]);

                if (nextTrim.empty() || nextTrim[0] == '#')
                {
                    ++i;
                    continue;
                }

                EclipChoice entrada;
                std::string  resto = nextTrim;
                bool huboPeso = false, huboGate = false, gateRoto = false;

                // ── Los params, del frente y en cualquier orden ───────────
                for (;;)
                {
                    bool pelo = false;

                    if (!huboPeso)
                    {
                        for (const char* clave : { "peso=", "weight=" })
                        {
                            const size_t largo = std::strlen(clave);
                            if (resto.size() <= largo ||
                                toLower(resto.substr(0, largo)) != clave)
                                continue;

                            const size_t corte =
                                resto.find_first_of(" \t", largo);
                            const std::string valor = (corte == std::string::npos)
                                ? resto.substr(largo)
                                : resto.substr(largo, corte - largo);

                            float p = 1.0f;
                            if (parseFloatStrict(valor, p) == NumParse::Ok)
                            {
                                if (p < 0.0f)
                                {
                                    warn(childLine, std::string(cmdName) +
                                        ": peso=" + valor + " es negativo, y un "
                                        "peso negativo no significa 'menos que "
                                        "nunca'. Se usa 0, que apaga la palabra "
                                        "sin borrarla.");
                                    p = 0.0f;
                                }
                                entrada.peso = p;
                            }
                            else
                                warn(childLine, std::string(cmdName) + ": peso= "
                                    "espera un numero, recibio '" + valor +
                                    "'. Se usa 1.");

                            resto = (corte == std::string::npos)
                                ? std::string() : trim(resto.substr(corte));
                            huboPeso = true; pelo = true;
                            break;
                        }
                    }

                    if (!huboGate && !resto.empty() && resto[0] == '[')
                    {
                        EclipGate g;
                        if (parseConditionModifier(resto, g, childLine, cmdName)
                                == GateParse::Malformado)
                        {
                            gateRoto = true;   // ya aviso; muere el renglon
                            break;
                        }
                        entrada.gate = g;
                        resto = trim(resto);
                        huboGate = true; pelo = true;
                    }

                    if (!pelo) break;
                }

                if (gateRoto) { ++i; continue; }   // ley 1a: sigue el bloque

                // ── Y recien ahora, el texto ─────────────────────────────
                if (resto.empty() || resto[0] != '"') break;

                const size_t ultima = resto.rfind('"');
                if (ultima == 0)
                {
                    ++i;
                    warn(childLine, std::string(cmdName) + ": '" + nextTrim +
                        "' abre comillas y no las cierra. Se ignora esa linea.");
                    continue;
                }

                const std::string cola = trim(resto.substr(ultima + 1));

                // EL CIERRE (fork C). Las dos formas medidas del corpus.
                if (cola.empty())     break;   // narracion
                if (cola.back() == ':') break; // `elegir` pegado (D-33)

                entrada.label = resto.substr(1, ultima - 1);

                if (cola.size() >= 2 && cola[0] == '-' && cola[1] == '>')
                {
                    entrada.target = trim(cola.substr(2));
                    if (entrada.target.empty())
                    {
                        ++i;
                        warn(childLine, std::string(cmdName) + ": la flecha no "
                            "apunta a nada. Se ignora esa linea.");
                        continue;
                    }
                }
                else
                {
                    // ── EL AZUCAR: una palabra pelada es `-> palabra + 1` ──
                    // Se expande ACA, en parse-time, para que la rama de `CP0`
                    // la vea como cualquier otra mutacion. Sin expandir,
                    // `natsuki` seria el nombre de una escena.
                    //
                    // Y tiene que ser UN token: `"amor" natsuki extra` no es
                    // azucar de nada, y expandirlo daria `natsuki extra + 1`.
                    if (cola.find_first_of(" \t") != std::string::npos)
                    {
                        ++i;
                        if (cola.find("[si:") != std::string::npos ||
                            cola.find("[if:") != std::string::npos)
                            warn(childLine, std::string(cmdName) + ": el gate "
                                "va DELANTE del texto, no despues ([si: acto "
                                "== 1] \"saludo\" personaje_a). Se ignora esa "
                                "linea.");
                        else
                            warn(childLine, std::string(cmdName) + ": no "
                                "entiendo '" + cola + "'. Despues del texto va "
                                "UNA variable (\"afinidad\" personaje_b) o una flecha "
                                "(\"afinidad\" -> personaje_b + 2). Se ignora esa "
                                "linea.");
                        continue;
                    }
                    entrada.target = cola + " + 1";
                }

                ++i;
                out.push_back(entrada);
            }
        }

        // ── SYN-3: EL CUERPO DE UN BLOQUE DE OPCIONES ─────────────────────
        //
        // `elegir` y `esperar_click` tenian este look-ahead COPIADO, con la
        // misma ley y las mismas dos fallas. Vive una sola vez (§7.323: la ley
        // vive en un lugar), y `i` avanza por referencia igual que antes.
        //
        // Un renglon que quiso ser opcion y no lo logro AVISA y se descarta —
        // el bloque SIGUE, asi que una opcion sana debajo no se pierde con la
        // rota. Es la ley de 1a en el cuerpo del bloque: muere el renglon, no
        // el comando (matar el comando es para el argumento requerido, SYN-2).
        // Parte nombres de tecla de un renglon secuencia: coma y espacio
        // separan igual. Local al parser: NO incluir InputManager.h (HO-K2).
        static std::vector<std::string> partirTeclasSecuencia(const std::string& s)
        {
            std::vector<std::string> out;
            std::string cur;
            for (char c : s)
            {
                if (c == ',' || c == ' ' || c == '\t')
                {
                    if (!cur.empty()) { out.push_back(cur); cur.clear(); }
                }
                else
                    cur.push_back(c);
            }
            if (!cur.empty()) out.push_back(cur);
            return out;
        }

        // `linea` empieza por la palabra (secuencia/sequence). Rellena
        // teclas+target. false = ya aviso; el caller descarta el renglon.
        static bool parsearCuerpoSecuencia(const std::string& linea,
                                           const std::string& palabra,
                                           EclipSecuencia& out,
                                           int lineNum,
                                           const char* cmdName)
        {
            const std::string resto = trim(linea.substr(palabra.size()));
            const size_t arrow = resto.find("->");
            if (arrow == std::string::npos)
            {
                warn(lineNum, std::string(cmdName) + ": '" + palabra +
                    "' sin flecha. La forma es " + palabra +
                    " TECLA TECLA -> destino. Se ignora esa linea.");
                return false;
            }
            const std::string dest = trim(resto.substr(arrow + 2));
            if (dest.empty())
            {
                warn(lineNum, std::string(cmdName) + ": '" + palabra +
                    "' con destino vacio. La forma es " + palabra +
                    " TECLA TECLA -> destino. Se ignora esa linea.");
                return false;
            }
            out.teclas = partirTeclasSecuencia(trim(resto.substr(0, arrow)));
            if (out.teclas.size() < 2)
            {
                warn(lineNum, std::string(cmdName) + ": '" + palabra +
                    "' pide al menos dos teclas. Se ignora esa linea.");
                return false;
            }
            out.target = dest;
            return true;
        }

        static void leerOpcionesDeBloque(const std::vector<std::string>& lines,
                                          int& i,
                                          std::vector<EclipChoice>& out,
                                          const char* cmdName,
                                          std::vector<EclipSecuencia>* outSecs = nullptr)
        {
            while (i + 1 < (int)lines.size())
            {
                const int childLine = i + 2;    // lineNum del hijo (i es 0-based)
                std::string nextTrim = trim(lines[i + 1]);

                if (nextTrim.empty() || nextTrim[0] == '#')
                {
                    ++i;    // vacias/comentarios dentro del bloque no cuentan
                    continue;
                }

                EclipGate gate;
                const GateParse gp = parseConditionModifier(nextTrim, gate,
                                                            childLine, cmdName);
                if (gp == GateParse::Malformado)
                {
                    // Ya aviso, con linea y con la forma correcta. Aca solo se
                    // descarta el renglon, y el bloque SIGUE: es la ley 1a en
                    // el cuerpo del bloque (muere la opcion, no el comando).
                    ++i;
                    continue;
                }

                std::string label, target;
                bool quiso = false;
                if (esOpcionDeBloque(nextTrim, label, target, quiso))
                {
                    ++i;
                    EclipChoice choice;
                    choice.gate   = gate;
                    choice.label  = label;
                    choice.target = target;
                    out.push_back(choice);
                    continue;
                }

                if (quiso)
                {
                    ++i;
                    warn(childLine, std::string(cmdName) + ": '" + nextTrim +
                        "' quiso ser una opcion y le falta la flecha. La forma "
                        "es \"texto que se ve\" -> escena_destino. Se ignora "
                        "esa linea.");
                    continue;
                }

                // Un `[` que sobrevivio a `parseConditionModifier` es un gate
                // mal escrito (`[sii:`, o sin el `:`). Ningun comando del motor
                // empieza con corchete, asi que no hay falso positivo posible.
                // Los que SI arrancan con `[si:`/`[if:` ya murieron arriba, con
                // un aviso que nombra su falla exacta (pierna C): lo que llega
                // hasta aca es lo que ni siquiera se parece a un gate.
                if (!nextTrim.empty() && nextTrim[0] == '[')
                {
                    ++i;
                    warn(childLine, std::string(cmdName) + ": '" + nextTrim +
                        "' no es un gate valido. La forma es [si: variable "
                        "operador valor] delante de la opcion. Se ignora esa "
                        "linea.");
                    continue;
                }

                // secuencia / sequence: por palabra, misma frontera de
                // mayusculas que el resto de los comandos (matchKeyword).
                // Antes del break de fin de bloque (P1-P10).
                const bool esSecEs = matchKeyword(nextTrim, "secuencia");
                const bool esSecEn = matchKeyword(nextTrim, "sequence");
                if (esSecEs || esSecEn)
                {
                    const std::string palabra = esSecEs ? "secuencia" : "sequence";
                    ++i;
                    if (!outSecs)
                    {
                        // esperar_click u otro caller que no acepta secuencias
                        warn(childLine, std::string(cmdName) + ": '" + palabra +
                            "' solo vale dentro de un elegir. Se ignora esa "
                            "linea.");
                        continue;
                    }
                    EclipSecuencia sec;
                    sec.gate = gate;
                    sec.sourceLine = childLine;
                    if (!parsearCuerpoSecuencia(nextTrim, palabra, sec,
                                                childLine, cmdName))
                        continue;
                    outSecs->push_back(sec);
                    continue;
                }

                break;    // fin del bloque
            }
        }

        // ── `elegir`: EL COMANDO ENTERO, EN UN SOLO LUGAR ──────────────────
        //
        // Lo llaman los DOS sitios: el `elegir` suelto de la cadena de despacho
        // y la forma pegada de `D-33` (`"linea" elegir:`). Ésa es la razón de
        // que la forma nueva no traiga reglas propias — el corchete, el
        // `default:` que no matchea, el `default:` sin `tiempo:` y el bloque
        // vacío son los MISMOS objetos, no una copia que se desincronice
        // (§7.323, la ley vive en un lugar).
        //
        // Devuelve `false` cuando el bloque no trajo opciones: ahí el comando
        // no se emite —conducta de siempre— y el caller de la forma pegada
        // además desarma la fusión, que es `D-38` en su caso REAL (el
        // degenerado que el contrato nombraba, `ocultar_caja` + pegado, no se
        // puede ni escribir: no hay renglón donde meterlo).
        static bool armarElegir(const std::vector<std::string>& lines,
                                int& i,
                                const std::string& params,
                                int lineNum,
                                EclipCmd& out)
        {
            out = EclipCmd();
            out.type = EclipCmdType::Elegir;
            out.sourceLine = lineNum;

            if (!params.empty())
            {
                // `elegir` NO lleva tecla: el sumidero es local y se descarta.
                // Si alguien escribe `tecla:` acá, la puerta avisa que no es de
                // este comando (antes se escribía mudo en `cmd.qteKey`, que
                // nadie lee para un Elegir).
                std::string teclaNoUsada;
                parseBlockParams(params, EclipCmdType::Elegir, "elegir",
                                 out.timerSeconds, out.arg3,
                                 teclaNoUsada, lineNum); // arg3 = default label
            }

            // Leer las siguientes líneas que sean opciones
            // Formato: [espacios] "label" -> target
            leerOpcionesDeBloque(lines, i, out.choices, "elegir", &out.secuencias);

            // Resolver defaultIndex: buscar cuál choice coincide con arg3
            if (!out.arg3.empty())
            {
                for (int ci = 0; ci < (int)out.choices.size(); ++ci)
                {
                    if (out.choices[ci].label == out.arg3)
                    {
                        out.defaultIndex = ci;
                        break;
                    }
                }
                // SYN-3: si no matchea, sigue cayendo en la ULTIMA — la
                // conducta de juego NO cambia— pero ahora lo DICE y lista los
                // labels que si existen. Vivo en `test_save_fases.ecl:45`
                // (`default: "Tiempo agotado"`, que no es ninguna opcion), y
                // la sesion anterior lo conto como sano.
                //
                // La confusion es razonable: el creador escribe lo que ESPERA
                // QUE PASE, o el nombre de la escena, porque todo lo demas del
                // bloque toma escena (`-> destino`). El `default:` es el unico
                // que pide una copia literal del label — y encima el match es
                // contra el label CRUDO, antes de interpolar `${var}`.
                if (out.defaultIndex < 0 && !out.choices.empty())
                {
                    std::string lista;
                    for (size_t ci = 0; ci < out.choices.size(); ++ci)
                        lista += (ci ? ", \"" : "\"") +
                                 out.choices[ci].label + "\"";
                    warn(lineNum, "elegir: el default \"" + out.arg3 +
                        "\" no es ninguna de las opciones. Opciones: " +
                        lista + ". Al acabarse el tiempo se usa la "
                        "ultima.");
                    out.defaultIndex = (int)out.choices.size() - 1;
                }

                // `default:` no es una feature de `elegir`: es una feature del
                // `tiempo:`. Sin timer, el bloque de `VNDialogBox` que lo
                // consume NUNCA corre, asi que la clave es inerte — escrita,
                // parseada y jamas usada.
                if (out.timerSeconds <= 0.0f)
                    // ASCII puro en el literal: este archivo es UTF-8 SIN BOM y
                    // MSVC lo lee como ANSI, asi que un guion largo sale `â€”`
                    // en la consola del creador. Es la misma trampa que la enie
                    // de 1c-ii (§1.60), y la caza correr la sonda en el motor
                    // real.
                    warn(lineNum, "elegir: 'default:' sin 'tiempo:' no "
                        "hace nada: el default solo se usa cuando se "
                        "acaba el tiempo. Agregale 'tiempo: Ns'.");
            }

            if (out.choices.empty())
            {
                warn(lineNum, "elegir: sin opciones");
                return false;
            }
            return true;
        }

        // ── Parsear parámetros entre [ ] de elegir/qte ────────────────────
        // Formato: tiempo: 5s, default: "No reaccionar", tecla: E
        //
        // SYN-3: el vocabulario y el tokenizador subieron a `bracketParams`.
        // Lo que queda acá es LEER las claves que este comando conoce — y por
        // eso ahora recibe el `tipo`: era UNA funcion para DOS comandos, así
        // que le daba a cada uno el vocabulario del otro y lo tiraba mudo
        // (`qte` recibia `default:` en un `dummy`, `elegir` recibia `tecla:`
        // en un campo que solo lee el case de QTE).
        static void parseBlockParams(const std::string& params,
                                      EclipCmdType tipo,
                                      const char* cmdName,
                                      float& outTimer,
                                      std::string& outDefault,
                                      std::string& outKey,
                                      int lineNum)
        {
            const auto kv = bracketParams(params, tipo, lineNum, cmdName);

            if (auto it = kv.find("tiempo"); it != kv.end())
            {
                DurationParseOptions durationOptions;
                durationOptions.allowBareSeconds = true;
                durationOptions.allowDialogueAdvance = false;
                const DurationParseResult parsedDuration =
                    parseDurationSpec(it->second, durationOptions);
                if (!parsedDuration)
                {
                    // SYN-4c-ii: decia "timer/tiempo:", o sea las DOS grafias.
                    // Era la forma honesta que se podia escribir sin el recibo
                    // —no mentia—, pero igual te nombraba una palabra que no
                    // habias escrito, y encima obligaba a leer una barra para
                    // encontrar la tuya. Ahora dice la que escribiste, y ya.
                    warn(lineNum, kv.escrita("tiempo") + ": duracion '" +
                        it->second + "' invalida: " +
                        durationParseErrorText(parsedDuration.error) +
                        ". Se usa el fallback historico de 5s.");
                    outTimer = 5.0f;
                }
                else
                    outTimer = parsedDuration.value.amount;
            }

            // La canonica es `defecto` (ES primero, como todo grupo de grafias
            // desde 1c-ii): `default:` sigue valiendo y llega ya traducida.
            if (auto it = kv.find("defecto"); it != kv.end())
            {
                const std::string& val = it->second;
                if (!val.empty() && val[0] == '"')
                    extractQuoted(val, outDefault);
                else
                    outDefault = val;
            }

            if (auto it = kv.find("tecla"); it != kv.end())
            {
                // Normalizar a mayúsculas por el fold ASCII-only de §1.45
                // (el `::toupper` byte-a-byte que vivía acá era la segunda
                // fuente del C4244 del header — ficha (11)).
                outKey = asciiToUpper(it->second);
            }
        }

        // ── Extraer [efecto: X] / [effect: X] de una línea ──────────────
        // Modifica 'line' quitando el bloque [...].
        // Retorna: 0=fade(default), 1=instant, 2=slide
        //
        // SYN-3: era el UNICO corchete del motor que se tragaba mudos los DOS
        // lados — la clave (`[efeto: instant]` -> fade) y el valor
        // (`[efecto: temblar]` -> fade). Ahora la clave pasa por la puerta
        // comun y el valor tiene su propio vocabulario, que ademas reconoce
        // `fade` DE VERDAD: el manual lo documenta desde siempre y este codigo
        // no lo matcheaba nunca (caia al `return 0` final, o sea que valia por
        // accidente y `fadee` daba lo mismo).
        static int extractCharEffect(std::string& line, EclipCmdType tipo,
                                     const char* cmdName, int lineNum)
        {
            size_t bStart = line.find('[');
            if (bStart == std::string::npos) return 0;
            size_t bEnd = line.find(']', bStart);
            if (bEnd == std::string::npos || bEnd < bStart) return 0;

            std::string inside = trim(line.substr(bStart + 1, bEnd - bStart - 1));
            line = trim(line.substr(0, bStart)); // Quitar [...] de la línea

            const auto kv = bracketParams(inside, tipo, lineNum, cmdName);
            auto it = kv.find("efecto");
            if (it == kv.end()) return 0;   // la puerta ya avisó si sobraba algo

            int efecto = 0;
            const std::string val = toLower(it->second);
            if (!bracketEffectValue(val, efecto))
            {
                warn(lineNum, std::string(cmdName) + ": no conozco el efecto '" +
                    val + "'. Efectos: " + bracketEffectHelp() +
                    ". Se usa fade.");
                return 0;
            }
            return efecto;
        }


    };

    // ════════════════════════════════════════════════════════════════════════
    // RT-1 · `makeTextPatch` — que le pide un `texto` a un texto que YA existe
    // ════════════════════════════════════════════════════════════════════════
    //
    // Vive ACA, como funcion libre despues de `EclipParser`, por dos razones:
    //
    //   1. Necesita `EclipParser::parseHexColor` (los hex viajan crudos desde el
    //      parser y se resuelven al ejecutar), asi que no puede ser miembro de
    //      `EclipCmd`, que se declara mucho mas arriba.
    //   2. Necesita ser ALCANZABLE por el harness. Vivia dentro de `cmdTexto`
    //      (`VNScriptState.h`), que arrastra SDL/Lua/audio y no se puede
    //      incluir en una TU standalone — o sea que el merge de estilo, que es
    //      la parte con reglas de verdad, no tenia un solo assert.
    //      `EstiloManifest` viene de `ProjectManifest.h`, que este header ya
    //      incluia y no depende del render: se alcanza sin arrastrar nada.
    //
    // LA REGLA DE PRESENCIA, que es la unica parte nueva: una propiedad esta
    // presente si la linea la nombro INLINE **o** si la linea trajo un
    // `estilo=` que la declara. Un estilo es una forma de nombrar varias
    // propiedades de un saque, no un tercer estado.
    //
    // El merge per-propiedad (inline pisa estilo) y los defaults utiles del
    // glow ("puse color -> veo glow": radio 2, intensidad 0.5) son los de
    // siempre, movidos tal cual.
    //
    // `est` puede ser nullptr (sin `estilo=`, o estilo inexistente — ese caso
    // ya avisa en `cmdTexto` y cae a inline/plano; aca solo significa "no
    // aporta propiedades").
    inline TextPatch makeTextPatch(const EclipCmd& cmd, const EstiloManifest* est)
    {
        TextPatch p;

        if (cmd.has(EclipField::X)) p.x = cmd.layerX;
        if (cmd.has(EclipField::Y)) p.y = cmd.layerY;
        if (cmd.has(EclipField::Z)) p.z = cmd.layerZ;

        // ── Color del texto ──
        const bool colorInline = cmd.has(EclipField::Color);
        const bool colorEstilo = (est && !est->colorHex.empty());
        if (colorInline || colorEstilo)
        {
            const std::string& hex = colorInline ? cmd.arg3 : est->colorHex;
            TextPatch::Rgb c;
            EclipParser::parseHexColor(hex, c.r, c.g, c.b);
            p.color = c;
        }

        // ── Fuente nombrada ──
        const bool fuenteInline = cmd.has(EclipField::Fuente);
        const bool fuenteEstilo = (est && !est->fontName.empty());
        if (fuenteInline || fuenteEstilo)
            p.fontName = fuenteInline ? cmd.fontName : est->fontName;

        // ── Glow: UN concepto para el creador, un bloque para la ley ──
        const bool glowInline = cmd.has(EclipField::GlowColor) ||
                                cmd.has(EclipField::GlowRadio) ||
                                cmd.has(EclipField::GlowInten);
        const bool glowEstilo = (est && (!est->glowColorHex.empty() ||
                                         est->glowRadius > 0.0f ||
                                         est->glowIntensity > 0.0f));
        if (glowInline || glowEstilo)
        {
            const std::string hex = cmd.has(EclipField::GlowColor)
                ? cmd.glowColorHex
                : (est ? est->glowColorHex : std::string());
            const f32 radius = cmd.has(EclipField::GlowRadio)
                ? cmd.glowRadius : (est ? est->glowRadius : 0.0f);
            const f32 intensity = cmd.has(EclipField::GlowInten)
                ? cmd.glowIntensity : (est ? est->glowIntensity : 0.0f);

            TextPatch::Glow g;
            if (!hex.empty())
                EclipParser::parseHexColor(hex, g.r, g.g, g.b);
            g.radius    = radius > 0.0f ? radius    : 2.0f;   // default util
            g.intensity = intensity > 0.0f ? intensity : 0.5f;   // default util
            p.glow = g;
        }

        // ── Borde/lista (TRAZOS-LISTA, A1+B1) ─────────────────────────────
        // El borde legado es azúcar de una lista de un elemento. Un borde
        // inline gana a la lista completa del estilo; sin inline, la lista
        // base gana al borde legado del estilo.
        auto resolveStroke = [](const TextStrokeSpec& spec) {
            ResolvedTextStroke out;
            if (!spec.colorHex.empty())
                EclipParser::parseHexColor(spec.colorHex, out.r, out.g,
                                           out.b, &out.a);
            out.grosor = spec.grosor;
            out.dx = spec.dx;
            out.dy = spec.dy;
            return out;
        };
        auto setFirstStroke = [](TextPatch::Borde& b) {
            if (b.strokes.empty()) return;
            const auto& first = b.strokes.front();
            b.r = first.r; b.g = first.g; b.b = first.b; b.a = first.a;
            b.grosor = first.grosor;
        };

        const bool bordeInline = cmd.has(EclipField::BordeColor) ||
                                 cmd.has(EclipField::BordeGrosor);
        const bool listaEstilo = (est && !est->trazos.empty());
        const bool bordeLegacyEstilo = (est && (!est->bordeColorHex.empty() ||
                                                est->bordeGrosor > 0.0f));
        if (bordeInline || listaEstilo || bordeLegacyEstilo)
        {
            TextPatch::Borde b;
            if (bordeInline)
            {
                TextStrokeSpec spec;
                spec.colorHex = cmd.has(EclipField::BordeColor)
                    ? cmd.bordeColorHex : "#000000";
                const bool grosorNombrado = cmd.has(EclipField::BordeGrosor);
                spec.grosor = grosorNombrado ? cmd.bordeGrosor : 2.0f;
                b.strokes.push_back(resolveStroke(spec));
            }
            else if (listaEstilo)
            {
                for (const auto& spec : est->trazos)
                    b.strokes.push_back(resolveStroke(spec));
            }
            else
            {
                TextStrokeSpec spec;
                spec.colorHex = !est->bordeColorHex.empty()
                    ? est->bordeColorHex : "#000000";
                spec.grosor = est->bordeGrosor > 0.0f
                    ? est->bordeGrosor : 2.0f;
                b.strokes.push_back(resolveStroke(spec));
            }
            setFirstStroke(b);
            p.borde = b;

            // FORK 3a: borde y glow son excluyentes; gana la lista/borde.
            p.glow.reset();
        }

        // ── Hover: solo desde el estilo (CP1: sin override inline) ──
        // Defaults = los valores BASE ya resueltos, asi el swap es completo.
        if (est && est->hasHover)
        {
            TextPatch::Hover h;
            h.text  = p.color ? *p.color : TextPatch::Rgb{};
            h.glow  = p.glow  ? *p.glow  : TextPatch::Glow{};
            h.borde = p.borde ? *p.borde : TextPatch::Borde{};

            if (!est->hoverColorHex.empty())
                EclipParser::parseHexColor(est->hoverColorHex,
                                           h.text.r, h.text.g, h.text.b);

            const bool hovGlowColor = !est->hoverGlowColorHex.empty();
            if (hovGlowColor)
                EclipParser::parseHexColor(est->hoverGlowColorHex,
                                           h.glow.r, h.glow.g, h.glow.b);
            if (est->hoverGlowRadius    > 0.0f) h.glow.radius    = est->hoverGlowRadius;
            if (est->hoverGlowIntensity > 0.0f) h.glow.intensity = est->hoverGlowIntensity;

            // "puse glow_color hover -> veo glow" (si el base no tenia glow)
            if (hovGlowColor)
            {
                if (h.glow.radius    <= 0.0f) h.glow.radius    = 2.0f;
                if (h.glow.intensity <= 0.0f) h.glow.intensity = 0.5f;
            }

            // ── Borde de hover — espeja al glow de arriba, campo por campo ──
            if (!est->trazosHover.empty())
            {
                h.borde.strokes.clear();
                for (const auto& spec : est->trazosHover)
                    h.borde.strokes.push_back(resolveStroke(spec));
                setFirstStroke(h.borde);
            }
            else
            {
                const bool hovBordeColor = !est->hoverBordeColorHex.empty();
                if (hovBordeColor)
                    EclipParser::parseHexColor(est->hoverBordeColorHex,
                                               h.borde.r, h.borde.g, h.borde.b,
                                               &h.borde.a);
                // 0 = no lo declare: hereda el grosor BASE ya resuelto.
                if (est->hoverBordeGrosor > 0.0f)
                    h.borde.grosor = est->hoverBordeGrosor;
                if (hovBordeColor && h.borde.grosor <= 0.0f)
                    h.borde.grosor = 2.0f;
                if (h.borde.strokes.size() == 1)
                {
                    h.borde.strokes[0].r = h.borde.r;
                    h.borde.strokes[0].g = h.borde.g;
                    h.borde.strokes[0].b = h.borde.b;
                    h.borde.strokes[0].a = h.borde.a;
                    h.borde.strokes[0].grosor = h.borde.grosor;
                }
            }

            p.hover = h;
        }

        return p;
    }

} // namespace eclips
