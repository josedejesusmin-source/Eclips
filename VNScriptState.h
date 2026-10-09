// ============================================================================
// VNScriptState.h — Runtime de EclipScript para Eclips Engine
// ============================================================================
//
// Es el GameState que carga un archivo .ecl y lo ejecuta.
// El creador escribe el script; este estado lo convierte en imagen + sonido.
//
// Uso desde main.cpp (o desde otro estado):
//
//   #include "VNScriptState.h"
//   // ...
//   m_stateStack->push(
//       std::make_unique<VNScriptState>(window, glCtx, stack,
//                                       engine.getInputManager(),
//                                       "assets/intro.ecl")
//   );
//
// ── FLUJO DE EJECUCIÓN ────────────────────────────────────────────────────────
//
//   Los comandos "instantáneos" (fondo, mostrar, ocultar, musica...) se
//   ejecutan solos y pasan al siguiente sin esperar.
//
//   Los comandos de "espera" (dialogo, narrador, elegir) detienen la
//   ejecución hasta que el jugador interactúa.
//
//   Controles:
//     Space / Enter / Click izquierdo → avanzar diálogo / completar typewriter
//     1 / 2 / 3                       → elegir opción
//     Rueda arriba / Page Up          → abrir historial de diálogos
//     Rueda / PgUp / PgDn             → navegar historial
//     ESC / Click / Rueda al final    → cerrar historial
//     A                               → toggle Auto-mode (avanza solo tras 2s)
//     Ctrl (mantener)                 → Skip (avance rápido)
//     K / L                           → quick save / quick load
//     F5 / F9                         → menú guardar / cargar
//     F3                              → toggle debug hotspots
//
// ============================================================================

#pragma once

#include "engine/core/GameState.h"
#include "engine/core/StateStack.h"
#include "PauseMenuState.h"
#include "SettingsState.h"   // verbo `config` nombra SettingsState directo
#include "engine/core/Types.h"
#include "engine/render/Texture.h"
#include "stb_image_write.h"   // 6.1 CP0: escritor PNG (impl en third_party/stb_impl.cpp)
#include <filesystem>          // 6.1 CP0: create_directories para saves/thumbs
#include <set>
#include "engine/render/Draw2D.h"
#include "engine/render/TextRenderer.h"
#include "engine/render/InlineTags.h"   // ← FO-14: openingTagRunEnd (dónde abre la línea)
#include "engine/render/FontManager.h"
#include "engine/render/FontCatalog.h"  // CP-R3: identidades logicas del catalogo
#include "engine/render/FontGeneration.h"
#include "engine/audio/AudioManager.h"
#include "engine/core/SaveManager.h"
#include "engine/core/SaveCodec.h"   // ← CP2: ruteo v2 de slot (splitEntries/Fields)
#include <cstdint>
#include "engine/core/VNExecutionPolicy.h" // reloj-dialogo CP0: gate testeable del batch
#include "engine/core/QteGate.h"     // ← `QTE-SIN-PUERTA`: la puerta que faltaba
#include "engine/input/InputManager.h" // ← atomo-teclado CP0: la tabla tecla↔nombre (una sola)
#include "engine/core/VNVerbRoute.h"   // ← atomo-teclado CP1: verbo + fase → InputAction
#include "engine/core/VNSystemFlow.h"  // FLF-01: llamada y ticket SDL-free
#include "engine/core/ControlAdjust.h" // ← atomo-teclado CP6: el ajuste sin puntero
#include "engine/core/PointerPresence.h" // ← atomo-teclado CP7: el puntero con dueño
#include "engine/core/WeightedDraw.h" // ← azar pierna B: el SORTEO (eje CUAL)
#include "engine/core/VNPalabrasPagina.h" // ← mutar-palabras CP2a: cuales N y donde
#include "engine/core/VNCompare.h"   // ← SI-NO-COMPARA-VARIABLES: comparar dos variables
#include "engine/core/VNSecuencia.h" // DISENO_SECUENCIA SQ-12: ley pura de secuencia
#include "engine/core/VNNombreReservado.h" // ← SET-NO-PROTEGE-EL-GUION-BAJO: de quién es el nombre
#include "engine/core/TextBudget.h"  // ← §7.410: la unidad del texto (countCodepoints)
#include "engine/core/TransicionPantalla.h" // ← FADE-DE-ESTADO-SIN-PERILLA: la transición tiene dueño
#include "engine/core/MetaStore.h"   // ← nuevo
#include "engine/core/SeenStore.h"   // ← P1
#include "engine/core/LangStore.h"   // idiomas CP1: nullptr = sistema off
#include "EclipScript.h"
#include "engine/render/EffectSystem.h"  // E-CP0: la lista de pasadas
#include "VNSaveMenu.h"
#include "engine/core/AssetManager.h"
#include "engine/render/SceneLayer.h"
#include "Hotspot.h"
#include "engine/scripting/LuaManager.h"
#include "engine/core/Settings.h"
#include "engine/core/WindowResolution.h"
#include "engine/core/DisplayChangeTransaction.h"
#include "engine/render/TweenSystem.h"
#include "Vignette.h"
#include "Theme.h"
#if __has_include("BulletHellState.h") || __has_include(<BulletHellState.h>)
#include "BulletHellState.h"
#endif
#include "CorridorState.h"
#include "Gameplay2DState.h"
#include "engine/render/VNDialogBox.h"

#include <filesystem>   // Para resolución de rutas Lua
#include <SDL.h>
#include <SDL_opengl.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <iostream>
#include <ctime>
#include <sstream>
#include <fstream>      // sello de guion H-B8: hashear el .ecl
#include <deque>
#include <algorithm>
#include <random>
#include <functional>
#include <cassert>

// ── FBO function pointers (OpenGL 3.0+ core, no expuestas por SDL_opengl.h) ──
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER            0x8D40
#define GL_COLOR_ATTACHMENT0      0x8CE0
#define GL_FRAMEBUFFER_COMPLETE   0x8CD5
#define GL_FRAMEBUFFER_BINDING    0x8CA6
#endif

typedef void   (APIENTRY* PFN_glGenFramebuffers)(GLsizei, GLuint*);
typedef void   (APIENTRY* PFN_glDeleteFramebuffers)(GLsizei, const GLuint*);
typedef void   (APIENTRY* PFN_glBindFramebuffer)(GLenum, GLuint);
typedef void   (APIENTRY* PFN_glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum(APIENTRY* PFN_glCheckFramebufferStatus)(GLenum);

static PFN_glGenFramebuffers         pfn_glGenFramebuffers = nullptr;
static PFN_glDeleteFramebuffers      pfn_glDeleteFramebuffers = nullptr;
static PFN_glBindFramebuffer         pfn_glBindFramebuffer = nullptr;
static PFN_glFramebufferTexture2D    pfn_glFramebufferTexture2D = nullptr;
static PFN_glCheckFramebufferStatus  pfn_glCheckFramebufferStatus = nullptr;

static bool loadFBOFunctions()
{
    pfn_glGenFramebuffers = (PFN_glGenFramebuffers)SDL_GL_GetProcAddress("glGenFramebuffers");
    pfn_glDeleteFramebuffers = (PFN_glDeleteFramebuffers)SDL_GL_GetProcAddress("glDeleteFramebuffers");
    pfn_glBindFramebuffer = (PFN_glBindFramebuffer)SDL_GL_GetProcAddress("glBindFramebuffer");
    pfn_glFramebufferTexture2D = (PFN_glFramebufferTexture2D)SDL_GL_GetProcAddress("glFramebufferTexture2D");
    pfn_glCheckFramebufferStatus = (PFN_glCheckFramebufferStatus)SDL_GL_GetProcAddress("glCheckFramebufferStatus");

    bool ok = pfn_glGenFramebuffers && pfn_glDeleteFramebuffers &&
        pfn_glBindFramebuffer && pfn_glFramebufferTexture2D &&
        pfn_glCheckFramebufferStatus;
    if (!ok) std::cerr << "[Eclips] FBO functions not available" << std::endl;
    return ok;
}

// ── Multitextura de funcion fija (GL 1.3): la MASCARA de silueta ─────────────
// Una pasada con textura AJENA y OPACA (hoy la unica: el ruido de `estatica`,
// a=255 en los 65536 texels) estampa el RECTANGULO del elemento, padding
// transparente incluido. La unidad de textura 1 lleva EL ELEMENTO y su TexEnv
// COMBINE multiplica el alpha del elemento al de la pasada: el ruido sale
// recortado a la SILUETA.
//
// Es el mismo mundo de funcion fija en el que ya vive todo el path de efectos
// (glBegin/glEnd entre un g_batch->end() y un g_batch->begin()) — no abre una
// superficie GL nueva. Y no estan en opengl32.dll (que exporta GL 1.1): se
// cargan por SDL como las de FBO.
#ifndef GL_TEXTURE0
#define GL_TEXTURE0               0x84C0
#define GL_TEXTURE1               0x84C1
#endif
#ifndef GL_COMBINE
#define GL_COMBINE                0x8570
#define GL_COMBINE_RGB            0x8571
#define GL_COMBINE_ALPHA          0x8572
#define GL_PREVIOUS               0x8578
#endif
#ifndef GL_SRC0_RGB
#define GL_SRC0_RGB               0x8580
#define GL_SRC0_ALPHA             0x8588
#define GL_SRC1_ALPHA             0x8589
#define GL_OPERAND0_RGB           0x8590
#define GL_OPERAND0_ALPHA         0x8598
#define GL_OPERAND1_ALPHA         0x8599
#endif

typedef void (APIENTRY* PFN_glActiveTextureFx)(GLenum);
typedef void (APIENTRY* PFN_glMultiTexCoord2fFx)(GLenum, GLfloat, GLfloat);

static PFN_glActiveTextureFx    pfn_glActiveTextureFx = nullptr;
static PFN_glMultiTexCoord2fFx  pfn_glMultiTexCoord2fFx = nullptr;

static bool loadMultiTexFunctions()
{
    pfn_glActiveTextureFx =
        (PFN_glActiveTextureFx)SDL_GL_GetProcAddress("glActiveTexture");
    pfn_glMultiTexCoord2fFx =
        (PFN_glMultiTexCoord2fFx)SDL_GL_GetProcAddress("glMultiTexCoord2f");

    const bool ok = pfn_glActiveTextureFx && pfn_glMultiTexCoord2fFx;
    if (!ok)
        std::cerr << "[VN] AVISO: multitextura no disponible - `estatica` sobre "
                     "personaje pintara su RECTANGULO, no la silueta (jamas "
                     "crash, jamas silencio)." << std::endl;
    return ok;
}

namespace eclips
{
    // ── Personaje activo en pantalla ──────────────────────────────────────────

    struct VNActiveChar
    {
        std::string alias;
        std::string expression;   // Normal: "feliz". Compuesto: "a" (face part)
        std::string spritePath;   // Normal: ruta completa. Compuesto: bodyPath
        VNPosition  position = VNPosition::Centro;

        // ── Personaje compuesto (cuerpo + cara separados) ──
        bool        composite = false;
        std::string bodyPart;     // "1r", "2l", etc.
        std::string facePart;     // "a", "b", etc.
        std::string bodyPath;     // "assets/ddlc/Luna/1r.png"
        std::string facePath;     // "assets/ddlc/Luna/a.png"

        // ── 3 capas DDLC (izquierda + derecha + cara) ──
        bool        threeLayer = false;
        std::string leftPart;     // "1l"
        std::string leftPath;     // "assets/ddlc/Luna/1l.png"
        std::string rightPart;    // "1r"
        std::string rightPath;    // "assets/ddlc/Luna/1r.png"

        // ── Fade in/out ──
        float       fadeAlpha = 1.0f;  // Alpha actual (0 = invisible, 1 = visible)
        float       fadeTarget = 1.0f;  // Alpha destino (0 = fading out, 1 = fading in)

        // ── Slide in/out ──
        float       slideX = 0.0f;        // Offset X actual (pixels virtuales)
        float       slideTargetX = 0.0f;   // Offset X destino (0 = en posición)
        bool        slidingOut = false;     // Si true, se elimina al completar slide

        // ── FBO cache (compuestos) ──
        GLuint      cachedTex = 0;      // Textura cacheada del composite
        int         cachedTexW = 0;     // Ancho de la textura cacheada
        int         cachedTexH = 0;     // Alto de la textura cacheada
        bool        compositeDirty = true; // true → recomponer en próximo render
    };

    // ── Entrada del historial de diálogos ──────────────────────────────────

    //struct VNHistoryEntry
    //{
        //std::string speaker;   // Vacío = narrador
        //std::string text;
    //};

    // ── Definición de evento random ─────────────────────────────────────────

    struct RandomEventDef
    {
        std::string name;       // "cafe", "bostezo"
        std::string target;     // Escena destino
        // El ENSAYO (SYN-4b): ritmo, tirada, cooldown y fase viven juntos en el
        // núcleo puro, que es donde el harness los alcanza. Antes esto era
        // `chance` sola —una TASA por segundo, 0.1 ≈ 9.5%/s y 1 ≈ 63%/s— porque
        // nadie había declarado el ensayo al que la probabilidad pertenece.
        eclips::TrialClock trial;
        // ── El GATE `[si:]` (pierna C) ──
        // Vive ACÁ y no adentro de `TrialClock` porque no es reloj: el header
        // del ensayo se declara "el eje CUANDO" en su primera línea y sólo
        // recibe la DECISIÓN, como predicado. Los datos son del evento.
        //
        // Y viaja al SAVE (campos 7/8/9): un `evento_random` se serializa
        // entero y al cargar se reconstruye desde el archivo —la línea del
        // `.ecl` que lo definió ya pasó y no vuelve a correr—, así que un gate
        // que no viajara se EVAPORARÍA en el primer load, y el evento pasaría a
        // dispararse justo cuando el creador dijo que no.
        eclips::EclipGate gate;
        int         order = 0;    // Orden de registro: el orden de disparo es
                                  // el de definición, no el del mapa (alfabético)
    };

    // ── Tipos de transición visual ────────────────────────────────────────

    enum class VNTransitionType
    {
        None,           // Sin transición (cambio instantáneo)
        Fade,           // Dip to black y aparecer
        Dissolve,       // Crossfade entre fondo viejo y nuevo
        SlideLeft,      // Nuevo fondo entra desde la derecha
        SlideRight      // Nuevo fondo entra desde la izquierda
    };

    // =========================================================================
    // VNScriptState
    // =========================================================================

    // Rol del VN, asignado al construir según quién lo lanzó y por qué (§7.84).
    // El archivo .ecl queda inocente: un menu.ecl es un .ecl normal; el rol vive
    // en la instancia, no en el archivo. Game = juego; Menu = el .ecl actúa como
    // menú (suprime pausa/save-in-place, redefine load).
    enum class VNRole { Game, Menu };

    class VNScriptState : public GameState
    {
    public:
        // ── `input` NO TIENE VALOR POR DEFECTO, Y ESO ES LA DOCTRINA ─────────
        //
        // Va en el grupo de infraestructura (ventana, contexto, stack) porque
        // es eso: una tabla del motor, una por juego, propiedad de `Engine`.
        //
        // Y va SIN default a propósito. Un `= nullptr` habría dejado que los
        // diez sitios de construcción compilaran sin tocarse, y el VN se
        // quedaría sin teclado **en silencio** en los que se olvidaran — que es
        // §7.471 palabra por palabra: *los eslabones que faltan en silencio son
        // los que tienen valor por defecto*. Poniéndolo acá, sin default, un
        // call-site incompleto **no compila**.
        VNScriptState(SDL_Window* window,
            SDL_GLContext  glContext,
            StateStack* stateStack,
            FontSourceStore* fontSources,
            eclips::InputManager* input,
            eclips::LangStore* langStore,
            const std::string& scriptPath,
            int initialLoadSlot = -1,
            const ProjectManifest* manifest = nullptr,
            MetaStore* metaStore = nullptr,
            SeenStore* seenStore = nullptr,
            VNRole role = VNRole::Game,
            VNScriptState* pausedGame = nullptr,
            eclips::system_flow::TicketRetornoPtr regresoAlOrigen = nullptr)
            : m_window(window)
            , m_glContext(glContext)
            , m_stateStack(stateStack)
            , m_fontSources(fontSources)
            , m_input(input)
            , m_lang(langStore)
            , m_scriptPath(scriptPath)
            , m_initialLoadSlot(initialLoadSlot)
            , m_manifest(manifest)
            , m_meta(metaStore)
            , m_seen(seenStore)
        {
            m_textRenderer.setSourceStore(m_fontSources);
            m_nameRenderer.setSourceStore(m_fontSources);
            m_candidateTextRenderer.setSourceStore(m_fontSources);
            m_candidateNameRenderer.setSourceStore(m_fontSources);
            m_fontManager.setSourceStore(m_fontSources);
            if (m_fontSources)
            {
                m_generationSubscription = m_fontSources->subscribeGeneration(
                    [this](std::uint64_t pending) {
                        return onFontGenerationPrepare(pending);
                    },
                    [this](std::uint64_t active, std::uint64_t previous) {
                        onFontGenerationCommitted(active, previous);
                    },
                    [this](std::uint64_t dropped) {
                        onFontGenerationRolledBack(dropped);
                    });
            }
            m_role = role;          // en cuerpo: no depende del orden de miembros
            // Ola 3 · Slice B: back-pointer al juego que esta pausa cubre.
            // null = menú normal (título/config); no-null ⇒ pausa-sobre-juego.
            m_pausedGame = pausedGame;
            m_regresoAlOrigen = std::move(regresoAlOrigen);
        }

        // ── GameState ─────────────────────────────────────────────────────────

        void onEnter() override
        {
            std::cout << "[VNScriptState] Cargando: " << m_scriptPath << std::endl;

            // Parsear el .ecl
            // Si tenemos manifest, sus personajes se siembran y el .ecl
            // puede redefinirlos localmente con aviso (sesión proyecto.eclips).
            m_script = EclipParser::parse(m_scriptPath, m_manifest);
            if (!m_script.valid)
            {
                std::cerr << "[VNScriptState] Error en script: "
                    << m_script.error << std::endl;
                // §1.10: NO usar pop() aquí — destruiría al state que nos
                // pusheó (MainMenuState aún en su handleEvent). En su lugar,
                // marcamos fallo + pedimos pop diferido. update() y
                // handleEvent() salen temprano gracias a m_failed.
                m_failed = true;
               m_stateStack->cmdPop(this);
                return;
            }

            // Fuentes
            m_hasFont = initBaseRenderer(
                m_textRenderer, Settings::defaultFontPath(), 34.0f, 0.0f, "texto");
            m_hasNameFont = initBaseRenderer(
                m_nameRenderer, Settings::defaultFontPath(), 26.0f, 0.0f, "chrome");

            // ── Aplicar tema_default del manifest si existe (D7) ──────
            // Si el .ecl tiene `cargar_tema "X"` después, ese gana —
            // el comando se ejecutará en su orden y sobreescribirá m_theme.
            // Patrón coherente con personajes: manifest siembra, .ecl puede
            // overridear local.
            if (m_manifest && !m_manifest->temaDefault.empty())
            {
                Theme defaultTheme = Theme::loadFromFile(m_manifest->temaDefault);
                if (defaultTheme.defaultDialogBox.hasCustomFont())
                {
                    m_hasFont = initBaseRenderer(
                        m_textRenderer, defaultTheme.defaultDialogBox.fontPath,
                        34.0f, defaultTheme.defaultDialogBox.kerning, "texto");
                    m_hasNameFont = initBaseRenderer(
                        m_nameRenderer, defaultTheme.defaultDialogBox.fontPath,
                        26.0f, defaultTheme.defaultDialogBox.kerning, "chrome");
                }
                m_theme = defaultTheme;
                m_variables["_tema_activo"] = m_manifest->temaDefault;
                std::cout << "[VNScriptState] tema_default aplicado desde manifest: '"
                    << m_manifest->temaDefault << "'" << std::endl;
            }

            // Pinear sprites PNG de las cajas del theme (default + nominadas).
            // Mismo principio que el fondo: assets persistentes durante la
            // vida del theme. Cambio de caja / speaker no recarga del disco.
            pinThemeSprites(m_theme);
            pinControlStyles();

            // Configurar VNDialogBox
            m_dialogBox.setTextRenderers(&m_textRenderer, &m_nameRenderer);
            m_dialogBox.setFontManager(&m_fontManager); // fuente inline {fuente=X}
            m_dialogBox.setTheme(&m_theme);
            m_dialogBox.setAssetManager(&m_assets);
            m_dialogBox.setVirtualResolution(
                (float)viewport::width(),
                (float)viewport::height());

            // Audio
            m_hasAudio = m_audio.init();
            if (m_hasAudio)
            {
                m_audio.setMusicVolume(Settings::musicVolume());
                m_audio.setSoundVolume(Settings::sfxVolume());
            }

            // Lua
            m_lua.init();
            registerLuaBindings();

            // E-CP6: los .lua declarados con `efecto_lua` corren al BOOT —
            // post-bindings, pre-primera-escena (fork A1 de la mesa). Por
            // esto el registro ya existe cuando un load salta a mitad de
            // escena y un `E;` con generador Lua tiene quien lo reviva
            // (§7.290: el invariante necesita quien lo restaure).
            loadDeclaredLuaEffects(m_script.luaEffects);

            // FBO para composite characters
            m_hasFBO = loadFBOFunctions();

            // Multitextura para la mascara de silueta de las pasadas con
            // textura ajena (`estatica`). Su ausencia degrada a la imagen de
            // siempre (el rectangulo), con aviso — nunca impide arrancar.
            m_hasMultiTex = loadMultiTexFunctions();

            // Fade in
            //
            // OJO: esto ARMA el fundido, no lo garantiza. El primer update()
            // puede cancelarlo si el state nacio TAPADO (buscar `m_primerUpdate`
            // — intro-fade-doble, §7.402). Aca no se puede decidir: `push()`
            // corre este onEnter ANTES de apilar a quien nos tapa, asi que en
            // esta linea la pregunta "¿hay algo encima?" siempre da false.
            //
            // El puntero va porque la mitad que desarma esta promesa vive a mil
            // lineas de aca y no menciona este sitio — que es EXACTAMENTE como
            // `isTransparent()` sobrevivio anos roto (§7.396).
            //
            // ── Y ACA ESTABA LA FICHA `FADE-DE-ESTADO-SIN-PERILLA` ──────────
            //
            // Hasta v0.8.49 estas dos lineas iban SIN UN SOLO `if` alrededor, o
            // sea que abrir la pausa desde el juego y abrir `guardar` desde la
            // pausa costaban lo mismo. Medido: 367 ms los dos, pasando por
            // negro, mientras un cambio de escena dentro del mismo state es
            // instantaneo. El creador no tenia perilla donde manda y sufria una
            // donde no manda.
            //
            // El contexto se DERIVA aca y no llega por parametro, y esa es la
            // mitad linda del arreglo: el comentario de arriba dice que en esta
            // linea "¿hay algo encima?" siempre da false — pero "¿quien estaba
            // ANTES?" siempre da la verdad, por la MISMA razon (`push()` corre
            // este `onEnter` antes de apilar). La ley completa y su trampa
            // viven en `StateStack::topeEsPantallaDeMenu`.
            {
                // `origenDe(this)` y no "el tope": por la cola el state YA esta
                // apilado cuando corre su `onEnter`, asi que mirar el tope pelado
                // es mirarse a si mismo. Los dos caminos de push tienen el ciclo
                // de vida invertido y el detalle vive en `StateStack::origenDe`.
                const StateStack::OrigenDelNacimiento origen =
                    m_stateStack ? m_stateStack->origenDe(this)
                                 : StateStack::OrigenDelNacimiento{};

                TransicionContexto tctx;
                tctx.destinoEsMenu = (m_role == VNRole::Menu);
                tctx.hayOrigen     = origen.hay;
                tctx.origenEsMenu  = origen.esMenu;

                static const TransicionesTabla kSinManifiesto{};
                const TransicionesTabla& tabla =
                    m_manifest ? m_manifest->transiciones : kSinManifiesto;

                m_transicionSpec =
                    transicionSpecDe(tabla, transicionMomentoDe(tctx));

                if (m_transicionSpec.tipo == TransicionTipo::Ninguna
                    || m_transicionSpec.duracion <= 0.0f)
                {
                    m_fadeAlpha = 0.0f;
                    m_fadingIn  = false;
                }
                else
                {
                    m_fadeAlpha = 1.0f;
                    m_fadingIn  = true;
                }
            }

            SDL_GetWindowSize(m_window, &m_winW, &m_winH);

            // CP-R3: el catálogo de fuentes nace ANTES del primer comando.
            // CP-R4: arranque es publicador — prepara y hornea el paquete
            // candidato; el commit del Engine sólo intercambia objetos listos.
            if (m_fontSources)
            {
                fontRequestPublisher(
                    *m_fontSources, FontPublisherDoor::Arranque,
                    m_generationSubscription);
                if (!fontFinalizePublisher(*m_fontSources))
                    m_fontSources->rollbackGeneration();
            }
            else
            {
                materializarCatalogoFuentes();
            }

            // Arrancar en la primera escena
            jumpToScene(m_script.firstScene);

            // Si se pidió cargar un slot al iniciar (desde menú principal)
            if (m_initialLoadSlot >= 0)
            {
                loadFromSlot(m_initialLoadSlot);
                m_initialLoadSlot = -1;
            }
        }

        void onExit() override
        {
            // -- LA POLITICA `SALIR-DEL-MENU-DESCARTA-O-APLICA` (firmada 2026-09-04) --
            //
            // El jugador movio el selector; irse del menu NO lo deshace. Si esta
            // pantalla es la DUENA de una pendiente abierta, la PUBLICA antes de
            // irse. La red del store sigue debajo: el que se muera sin publicar
            // igual no deja la transaccion colgada (`R1`, seccion 16 del harness).
            //
            // EL ORDEN ES LA MITAD DE LA LEY, y por eso va ANTES del unsubscribe:
            // el `onCommit` de ESTA pantalla es el unico que publica el idioma
            // vivo -pide `m_hasCandidateLanguage`, que ninguna otra tiene-, asi
            // que desuscribirse primero dejaria al commit sin su publicador.
            //
            // Sin esto el resultado dependia de si el `while` de update corria en
            // ese frame: 9 de 10 corridas con el motor despierto, el cambio de
            // idioma se perdia EN SILENCIO. Guardian de runtime:
            // `correr_probe_r1_pausa.ps1`, que con esta politica deja de ser una
            // carrera; guardian de fuente: seccion 17 del harness de CP-R4.
            if (m_fontSources && m_generationSubscription != 0 &&
                m_fontSources->pendingOwner() == m_generationSubscription)
            {
                if (fontFinalizePublisher(*m_fontSources))
                    fontCommitPublisher(*m_fontSources);
                else
                    m_fontSources->rollbackGeneration();
            }

            if (m_fontSources && m_generationSubscription != 0)
            {
                m_fontSources->unsubscribeGeneration(m_generationSubscription);
                m_generationSubscription = 0;
            }
            m_fontManager.syncLiveLocalIdentities({});
            m_fontManager.clear();
            if (m_fontSources) m_fontSources->notifyConsumerDropped();

            // R4 display: salir JAMAS confirma un candidato temporal. La
            // restauracion ocurre antes del save completo de Settings.
            if (m_displayChange.active())
                finishDisplayChange(false, "salida de config");

            // ── Persistencia del menú autorable (Fork 1a) ─────────────────
            // Un menú que se cierra guarda lo que el jugador eligió. El .ecl
            // aplica cada control en vivo (writeControlValue) pero nunca
            // persiste — éste es el único save fuera del SettingsState C++.
            // onExit() corre tanto al popear el overlay (config "Volver") como
            // al cerrar la app (StateStack::clear → onExit). Guard a rol Menu:
            // sólo quien toca settings persiste; el juego (rol Game) no.
            // save() escribe el snapshot completo e idempotente, así que da
            // igual cuál control cambió (Settings.h:167).
            if (m_role == VNRole::Menu)
                Settings::save();

            // Unpinear texturas de capas
            for (const auto& [name, path] : m_layerPaths)
                m_assets.unpin(path);
            m_layerPaths.clear();
            m_layerPathOwners.clear();
            for (const auto& [name, path] : m_layerHoverPaths)
                 m_assets.unpin(path);
            m_layerHoverPaths.clear();
            m_layerHoverOwners.clear();
            m_sceneLayers.clear();
            m_hotspots.clear();
            m_controls.clear();
            m_teclaWidgets.clear();   // `CP9b`
            m_focoTecla = -1;
            cerrarCaptura();          // un cambio de escena no deja una captura viva
            m_slotWidgets.clear();
            m_slotThumbs.clear();   // 6.1 CP1: soltar las Texture de thumbs (GPU)
            m_slotThumbOwners.clear();
            m_hoveredSlot = -1;            // juice-hover: sin slot tras el reset
            // `CP6`: el anillo unificado se apaga junto con sus focables. Si el
            // foco sobreviviera al cambio de pantalla, la primera flecha
            // activaría un índice de la pantalla ANTERIOR.
            m_focoPlano = -1;
            m_focoControl = -1;
            m_hoveredControl = -1;
            // P2-3 (CONTROL-SKIN-BORDES-01): el CUARTO campo del anillo, que
            // este bloque olvidaba. Su gemelo retirarAmbito ya lo apagaba; este
            // y su copia en loadFromSlot no, y desde el lote del skin hay un
            // consumidor que lo INDEXA (controlPart) sobre un m_controls ya
            // vaciado. No es un campo mas: es el unico del anillo que quedaba
            // afuera, y el harness lo mide sitio por sitio.
            m_draggingControl = -1;
            m_focoDelPuntero = true;
            m_clickMappings.clear();
            m_scopeGenerations.clear();
            m_activeScopeOwner = 0;
            m_nextScopeGeneration = 1;
            m_randomEvents.clear();
            m_eventsActive = false;
            m_azarBolsas.clear();   // ley 7: el sorteo no sobrevive al guion
            // `CP2a`: la misma vida útil. Acá no hace falta apagar las capas —el
            // `clear()` de abajo se las lleva todas—, sólo soltar los ids.
            m_palabrasIds.clear();
            m_palabrasActivas = false;

            // Unpin sprites del theme antes del clear total — simetria con
            // el pin de onEnter. m_assets.clear() los liberaria igual, pero
            // el unpin explicito mantiene el invariante visible.
            unpinThemeSprites(m_theme);
            unpinControlStyles();

            m_assets.clear();
            releaseAllCachedTex();
            m_activeChars.clear();

            // Limpiar FBO de composición
            if (m_compFBO && pfn_glDeleteFramebuffers)
            {
                pfn_glDeleteFramebuffers(1, &m_compFBO);
                m_compFBO = 0;
            }
            // 6.1 CP0: FBO + textura del thumbnail
            if (m_captureFBO && pfn_glDeleteFramebuffers)
            {
                pfn_glDeleteFramebuffers(1, &m_captureFBO);
                m_captureFBO = 0;
            }
            if (m_captureTex)
            {
                glDeleteTextures(1, &m_captureTex);
                m_captureTex = 0;
            }
            // E-CP3: la textura de ruido de `estatica` (generada, sin asset)
            if (m_fxRuidoTex)
            {
                glDeleteTextures(1, &m_fxRuidoTex);
                m_fxRuidoTex = 0;
            }

            m_textRenderer.shutdown();
            m_nameRenderer.shutdown();

            if (m_hasAudio)
            {
                m_audio.stopMusic();
                m_audio.shutdown();
            }
        }

        void onPause() override
        {
            if (m_hasAudio) m_audio.pauseMusic();
            // Apagar skip/auto al entrar a gameplay —
            // no tiene sentido que sigan activos mientras el jugador camina
            m_skipMode = false;
            m_autoMode = false;
        }

        void onResume() override
        {
            // El origen sólo recibe este ticket en el resume-neto del tope
            // final. Moverlo y limpiar el miembro antes de aplicar el efecto
            // hace que un segundo onResume no pueda repetir el avance.
            auto llamada = std::move(m_llamadaPendiente);
            m_llamadaPendiente.reset();
            if (llamada)
            {
                const eclips::system_flow::RegresoEfecto efecto =
                    eclips::system_flow::consumirRetorno(*llamada);
                aplicarRetornoPantalla(efecto);
            }

            m_waitingForGameplay = false;

            // Limpiar personajes que estaban siendo ocultados cuando se pausó.
            // Si Luna tenía fadeTarget=0 y el corredor interrumpió el fade,
            // al reanudar debe estar completamente fuera — no visible a medias.
            for (auto& ac : m_activeChars)
                if (ac.fadeTarget <= 0.0f) releaseCachedTex(ac);
            m_activeChars.erase(
                std::remove_if(m_activeChars.begin(), m_activeChars.end(),
                    [](const VNActiveChar& ac) { return ac.fadeTarget <= 0.0f; }),
                m_activeChars.end()
            );

            if (m_hasAudio)
            {
                m_audio.resumeMusic();
                m_audio.setMusicVolume(Settings::musicVolume());
                m_audio.setSoundVolume(Settings::sfxVolume());
            }

            // ── Refresco del menú al volver (B · conciencia de saves) ───────
            // Un menú reanuda CONGELADO: el `elegir`/`esperar_click` ya filtro
            // sus opciones cuando se ejecuto (boot), y onResume no re-corre la
            // escena. Sin esto, `[si: _saves > 0] "Cargar"` quedaria stale tras
            // jugar→guardar→titulo (oculta "Cargar" aunque ya exista un save).
            //
            // Como en WaitingChoice/WaitingClick m_cmdIndex apunta al comando de
            // espera vivo, re-ejecutarlo re-evalua sus condiciones y re-arma la
            // MISMA espera — sin avanzar, sin replayear fondo/capas/audio/intro.
            // Solo rol Menu: en un juego, re-armar un `elegir` a medio elegir al
            // volver de un overlay seria intrusivo (resetea el cursor); el menu
            // es justo donde `_saves` cambia entre visitas.
            if (m_role == VNRole::Menu
                && (m_phase == VNPhase::WaitingChoice
                    || m_phase == VNPhase::WaitingClick))
            {
                executeNextCmd();
            }
        }

        // ── Input ─────────────────────────────────────────────────────────────

        bool handleEvent(const void* event) override
        {
            // §1.10: Si onEnter() falló, ignorar cualquier evento. Eventos
            // SDL pueden encolarse antes del primer update() que procesa el
            // requestPop, llegando a este state ya muerto en lo lógico.
            if (m_failed) return false;

            // LOTE B: mientras la pantalla se está yendo no acepta nada, y
            // CONSUME (`true`) en vez de dejar pasar. Es el molde de `m_failed`
            // de arriba, con una diferencia de intención: aquél blinda contra
            // un state ya muerto en lo lógico; éste evita que el jugador
            // clickee un menú que ya decidió irse -- y que un segundo `fin`
            // durante la rampa encole un pop de más.
            if (m_cerrando) return true;

            const SDL_Event* e = static_cast<const SDL_Event*>(event);

            // ── `CP7`: LA POSICIÓN DEL PUNTERO SE ACTUALIZA ARRIBA DE TODO ───
            //
            // **Y «arriba de todo» es parte de la ley, no comodidad.** Lo que
            // esto reemplaza es un `SDL_GetMouseState`, y un polleo **no sabe
            // que hay un modal**: durante un `entrada`, un historial abierto o
            // el modal de display, la posición seguía actualizándose igual.
            // Ponerlo debajo de esos tres bloques —que hacen `return true`—
            // habría CONGELADO el puntero mientras un modal vive, que es una
            // conducta nueva que nadie pidió y que ningún harness ve.
            //
            // Lo que sí va debajo es la DERIVACIÓN (`refrescarHoverDelPuntero`),
            // porque ésa siempre estuvo gateada: es el `linksFrozen` de `CP3`.
            // Separar las dos es todo el punto — la posición es un hecho, el
            // hover es una decisión.
            //
            // El orden interno es el del contrato: primero la posición
            // (`PointerPresence.h` ley 1), después quién manda (ley 2 de
            // `FocusRing.h`, más abajo), y recién ahí la derivación.
            {
                const eclips::PunteroEvento pe = eclips::punteroEventoDe(e);
                if (pe.gesto != eclips::PunteroGesto::Ninguno)
                {
                    float pvx, pvy;
                    mouseToVirtual((float)pe.x, (float)pe.y, pvx, pvy);
                    m_punteroWinX = pe.x;
                    m_punteroWinY = pe.y;
                    m_puntero = eclips::punteroAplicarEvento(m_puntero, pe, pvx, pvy);
                }
            }

            // R4 display: modal transaccional con prioridad maxima. Mientras
            // vive, todo input pertenece a confirmar/revertir el candidato.
            if (m_displayChange.active())
            {
                if (e->type == SDL_KEYDOWN)
                {
                    // ── `CP6` (fork 7a'): ESTE MODAL PREGUNTA POR VERBOS ─────
                    //
                    // **Por que le tocaba a este lote, y no es la ficha
                    // `ANILLO-HORNEADO-X2` completa:** los dos controles que
                    // levantan este modal son `pantalla_completa` y
                    // `resolucion` (`writeControlValue` -> `beginDisplayChange`,
                    // `:7437`/`:7450`), y los dos viven en `config.ecl`, que es
                    // la aceptacion de `CP6`. Hasta hoy la costura no existia
                    // porque esos controles eran mouse-only: cuando el modal
                    // aparecia, el jugador ya tenia el mouse en la mano. En
                    // cuanto el teclado llega al control, el flujo es *llego con
                    // el teclado -> cambio -> salta el modal -> y aca mi remapeo
                    // deja de valer*, con el reloj de reversion corriendo
                    // (`:1341`). O sea: se revierte solo y se ve como que el
                    // motor lo ignoro. **`CP6` no hereda este caller: lo
                    // fabrica.**
                    //
                    // ── DE LAS TRES CRUDAS QUEDA UNA, Y EL VENCIMIENTO SE CUMPLIO
                    //
                    // *(Acá decía: «`TAB`, `ENTER_NUM` y `ATRAS` se pueden
                    // nombrar y NO están en la tabla default… **el día que
                    // `CP7` las ligue, estas tres líneas se BORRAN**».)*
                    //
                    // **`CP7` (2026-08-10) ligó dos y estas dos líneas se
                    // borraron**: `ATRAS -> menu` y `ENTER_NUM -> avanzar`
                    // entraron al default, así que el modal las recibe ya
                    // traducidas y con el remapeo del jugador puesto.
                    //
                    // **`TAB` se queda cruda, y su razón CAMBIÓ.** `CP6` la
                    // dejaba afuera porque ligarla habría inventado conducta;
                    // hoy la razón es más fina y más incómoda: **no hay verbo
                    // al que ligarla.** Acá TAB significa *«alternar entre estos
                    // DOS botones»* —que es `foco_izquierda`/`foco_derecha`— y
                    // en el resto del anillo significaría *«el siguiente»*, que
                    // es `foco_abajo`. Ligarla a la primera le movería el slider
                    // al jugador en `config.ecl`; a la segunda, este bloque
                    // dejaría de alternar. Ficha `TAB-SIN-VERBO`
                    // (`InputManager.h`, ley 3).
                    //
                    // El `ESC` ya no esta escrito a mano: si el jugador remapeo
                    // `menu`, el modal obedece su tecla. Nadie queda encerrado
                    // porque la red del §8 garantiza que `menu` SIEMPRE tiene
                    // binding.
                    const std::vector<std::string> vModal =
                        m_input->resolverTodos(e, "vn");
                    const SDL_Keycode k = e->key.keysym.sym;

                    if (vnHayVerbo(vModal, "menu"))
                        finishDisplayChange(false, "cancelado");
                    else if (vnHayVerbo(vModal, "foco_izquierda")
                        || vnHayVerbo(vModal, "foco_derecha")
                        || k == SDLK_TAB)
                        m_displayConfirmCursor = 1 - m_displayConfirmCursor;
                    else if (vnHayVerbo(vModal, "avanzar"))
                        displayConfirmChoose(m_displayConfirmCursor);
                    return true;
                }
                if (e->type == SDL_MOUSEMOTION)
                {
                    float vx, vy;
                    mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);
                    int hb = hitTestDisplayConfirmButton(vx, vy);
                    if (hb >= 0) m_displayConfirmCursor = hb;
                    return true;
                }
                if (e->type == SDL_MOUSEBUTTONDOWN
                    && e->button.button == SDL_BUTTON_LEFT)
                {
                    float vx, vy;
                    mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);
                    int hb = hitTestDisplayConfirmButton(vx, vy);
                    if (hb >= 0) displayConfirmChoose(hb);
                    return true;
                }
                return true;
            }

            // ═══════════════════════════════════════════════════════════
            // Confirm modal de borrado (Ola 2 · B2 · CP2): PRIORIDAD MAXIMA.
            // Mientras vive, el modal se traga TODO el input — flechas/Tab
            // alternan Si/No, Enter/Espacio/clic confirman, ESC/AC_BACK
            // cancelan. Nada de abajo (slots, ESC->pausa, atajos) responde.
            // "Si" hoy solo cierra (el borrado real + refresco llegan en CP3).
            // ═══════════════════════════════════════════════════════════
            if (m_confirmActive)
            {
                if (e->type == SDL_KEYDOWN)
                {
                    SDL_Keycode k = e->key.keysym.sym;
                    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK)
                        closeConfirmModal();                       // ESC = cancela (No)
                    else if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_TAB)
                        m_confirmCursor = 1 - m_confirmCursor;     // alterna Si/No
                    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER
                        || k == SDLK_SPACE || k == SDLK_z)
                        confirmModalChoose(m_confirmCursor);
                    return true;                                   // consumir TODA tecla
                }
                if (e->type == SDL_MOUSEMOTION)
                {
                    // Hover: el cursor sigue al mouse si esta sobre un boton.
                    float vx, vy;
                    mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);
                    int hb = hitTestConfirmButton(vx, vy);
                    if (hb >= 0) m_confirmCursor = hb;
                    return true;
                }
                if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
                {
                    float vx, vy;
                    mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);
                    int hb = hitTestConfirmButton(vx, vy);
                    if (hb >= 0) confirmModalChoose(hb);
                    return true;                                   // clic fuera: consume, no-op
                }
                return true;   // cualquier otro evento: consumir (el modal atrapa todo)
            }

            // ═══════════════════════════════════════════════════════════
            // `CP9b`: MODO CAPTURA. Mientras vive, el teclado es CRUDO.
            //
            // **Vive ARRIBA de la Capa 1 a proposito** (`L-5` punto 4, como el
            // early-return del QTE): toma el `InputBind` del evento, no el
            // verbo. Si preguntara por verbos, capturar `K` te devolveria
            // `guardado_rapido` en vez de la tecla.
            //
            // La UNICA pregunta por verbo es la cancelacion, y es el fork 4b:
            // cancela `menu`, no un `ESC` escrito a mano. `CP6` borro ese ESC
            // hardcodeado del modal de display cuarenta lineas mas arriba, con
            // la razon puesta — seguir la letra del §7 lo reintroduciria. La red
            // del §8 garantiza que `menu` SIEMPRE tiene tecla, asi que no hay
            // forma de quedarse encerrado en la captura.
            // ═══════════════════════════════════════════════════════════
            if (m_capturaTecla >= 0)
            {
                // Nada se captura hasta que se SUELTE lo que abrio la captura.
                // Sin esto, activar la fila de "Avanzar" con ESPACIO -que es el
                // verbo `avanzar`- ligaria ESPACIO a la accion que ibas a
                // remapear, y activarla con el mouse ligaria el click.
                if (!m_capturaArmada)
                {
                    if (e->type == SDL_KEYUP || e->type == SDL_MOUSEBUTTONUP)
                        m_capturaArmada = true;
                    return true;
                }

                if (e->type == SDL_KEYDOWN && e->key.repeat == 0)
                {
                    if (vnHayVerbo(m_input->resolverTodos(e, "vn"), "menu"))
                    {
                        cerrarCaptura();
                        return true;
                    }

                    // Fork 7a: `SUPR` vacia la fila. Queda cruda como el `TAB`
                    // del modal de display, y por la misma razon escrita alli:
                    // no hay verbo que signifique *"borrar este binding"*, y
                    // inventarle uno seria un nombre sin caller fuera de esta
                    // pantalla. Costo declarado: `SUPR` no se puede ligar desde
                    // aca. Hoy no la usa ningun default.
                    if (e->key.keysym.sym == SDLK_DELETE)
                    {
                        aplicarRebind(m_capturaTecla, "");
                        cerrarCaptura();
                        return true;
                    }

                    const InputBind b = inputBindDeEvento(e);

                    // Fork 5a: solo teclas. La rueda y el click quedan afuera de
                    // la captura -no de la TABLA, que los sigue soportando-, y
                    // la perdida esta declarada: `historial` viene ligado a
                    // `RUEDA_ARRIBA`, asi que se puede mover a una tecla y no
                    // volver a la rueda desde esta pantalla.
                    if (b.kind != InputBind::Kind::Tecla)
                    {
                        std::cerr << "[teclas] esa entrada no es una tecla: la "
                            "captura solo toma teclas por ahora." << std::endl;
                        return true;
                    }

                    // Y la guarda que evita una perdida MUDA: la tabla de
                    // nombres son 44 filas + A-Z + 0-9, y fuera de ahi
                    // `inputNombreDe` contesta vacio. Escribir ese vacio
                    // significaria DESLIGAR la accion — o sea que apretar F13
                    // durante la captura te dejaba sin tecla y sin aviso.
                    const std::string nombre = inputNombreDe(b);
                    if (nombre.empty())
                    {
                        std::cerr << "[teclas] esa tecla no tiene nombre en el "
                            "motor, asi que no se puede escribir en "
                            "`settings.eclips`. Probá con otra." << std::endl;
                        return true;
                    }

                    aplicarRebind(m_capturaTecla, nombre);
                    cerrarCaptura();
                    return true;
                }

                // Un click en cualquier lado cancela, que es lo que un jugador
                // espera de un modal chico. El `return true` de abajo se traga
                // todo lo demas: mientras la captura vive, nada corre debajo.
                if (e->type == SDL_MOUSEBUTTONDOWN) cerrarCaptura();
                return true;
            }

            // ── Control de settings (CP3): mientras se arrastra, el motion/up
            //    van SOLO al control (prioridad total sobre el resto). ─────────
            if (m_draggingControl >= 0)
            {
                if (e->type == SDL_MOUSEMOTION)
                {
                    float vx, vy;
                    mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);
                    applyControlDrag(m_draggingControl, vx);
                    return true;
                }
                if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT)
                {
                    m_draggingControl = -1;
                    return true;
                }
            }

            // ═══════════════════════════════════════════════════════════
            // 0. Entrada de texto (B2). Prioridad MÁXIMA: mientras el
            //    jugador escribe, este bloque se traga CASI todo para que
            //    teclear no dispare atajos (k=quicksave, a=auto, 1-4=choices,
            //    espacio=avanzar). Solo actúa sobre TEXTINPUT, backspace,
            //    enter y esc; el resto se consume. Va ANTES que los atajos.
            // ═══════════════════════════════════════════════════════════
            if (m_phase == VNPhase::WaitingTextInput)
            {
                if (e->type == SDL_TEXTINPUT)
                {
                    // e->text.text es UTF-8 (puede traer varios bytes por
                    // codepoint). max en CODEPOINTS, no bytes (§1.45). 0 = sin
                    // límite. Si está lleno, ignoramos la entrada (el jugador
                    // ve que no entra más — feedback inmediato).
                    if (m_inputMaxLen <= 0 ||
                        countCodepoints(m_inputBuffer) < m_inputMaxLen)
                    {
                        m_inputBuffer += e->text.text;
                        // Recorte defensivo si el incoming traía multi-codepoint
                        // (paste / IME) y se pasó del límite.
                        while (m_inputMaxLen > 0 &&
                            countCodepoints(m_inputBuffer) > m_inputMaxLen)
                        {
                            size_t i = m_inputBuffer.size();
                            do { --i; } while (i > 0 &&
                                ((unsigned char)m_inputBuffer[i] & 0xC0) == 0x80);
                            m_inputBuffer.erase(i);
                        }
                    }
                    return true;
                }
                if (e->type == SDL_KEYDOWN)
                {
                    SDL_Keycode k = e->key.keysym.sym;
                    if (k == SDLK_BACKSPACE)
                    {
                        // Borrar UN codepoint UTF-8 completo, no un byte
                        // ('é' = 2 bytes, emoji = 4). §1.45: retroceder sobre
                        // bytes de continuación 10xxxxxx hasta el byte líder.
                        if (!m_inputBuffer.empty())
                        {
                            size_t i = m_inputBuffer.size();
                            do { --i; } while (i > 0 &&
                                (static_cast<unsigned char>(m_inputBuffer[i])
                                    & 0xC0) == 0x80);
                            m_inputBuffer.erase(i);
                        }
                        return true;
                    }
                    if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                    {
                        // 'requerido': no se confirma vacío (DDLC no te deja
                        // empezar sin nombre). El OK gris ya lo señala.
                        if (m_inputRequired && m_inputBuffer.empty())
                            return true;
                        if (m_hasAudio) {   // confirmar = select (reusa el de choice)
                            const std::string& cs = m_dialogBox.activeChoiceClickSound();
                            if (!cs.empty()) m_audio.playSound(cs);
                        }
                        commitTextInput(m_inputBuffer);
                        return true;
                    }
                    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK)
                    {
                        // Cancelar = "" (en P5, el default). PERO bajo 'requerido'
                        // Esc commitearía vacío y bypassearía la regla → se ignora
                        // (desktop). En Android, AC_BACK bajo 'requerido' rutea al
                        // back-handler de la app (salir, NO avanzar vacío): fichado.
                        if (m_inputRequired)
                            return true;   // no-op: ni commit-vacío ni atajo
                        commitTextInput("");
                        return true;
                    }
                }
                // ── OK clickable: hover (visual + sonido) y click ─────────
                // Mismo rect que el render (§7.100). Deshabilitado si
                // 'requerido' y buffer vacío → ni hover ni click.
                const bool okDisabled = (m_inputRequired && m_inputBuffer.empty());
                if (e->type == SDL_MOUSEMOTION)
                {
                    float vx, vy;
                    mouseToVirtual((float)e->motion.x, (float)e->motion.y, vx, vy);
                    auto ok = m_dialogBox.getTextInputOkRect(
                        (float)viewport::width(), (float)viewport::height());
                    bool inOk = !okDisabled &&
                        vx >= ok.x && vx <= ok.x + ok.w &&
                        vy >= ok.y && vy <= ok.y + ok.h;
                    if (inOk != m_inputOkHover)
                    {
                        // Flanco: sonido 1× al entrar (reusa el de choice, §7.6).
                        if (inOk && m_hasAudio)
                        {
                            const std::string& hs = m_dialogBox.activeChoiceHoverSound();
                            if (!hs.empty()) m_audio.playSound(hs);
                        }
                        m_inputOkHover = inOk;
                    }
                    return true;
                }
                if (e->type == SDL_MOUSEBUTTONDOWN &&
                    e->button.button == SDL_BUTTON_LEFT)
                {
                    if (!okDisabled)
                    {
                        float vx, vy;
                        mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);
                        auto ok = m_dialogBox.getTextInputOkRect(
                            (float)viewport::width(), (float)viewport::height());
                        if (vx >= ok.x && vx <= ok.x + ok.w &&
                            vy >= ok.y && vy <= ok.y + ok.h)
                        {
                            if (m_hasAudio) {   // confirmar = select (reusa el de choice)
                                const std::string& cs = m_dialogBox.activeChoiceClickSound();
                                if (!cs.empty()) m_audio.playSound(cs);
                            }
                            commitTextInput(m_inputBuffer);   // mismo exit que Enter
                            return true;
                        }
                    }
                    return true;   // click fuera del OK: tragar (modal)
                }
                // Cualquier otra cosa (otras teclas, wheel): tragar.
                // Esto es lo que impide que 'k','a','1' disparen atajos.
                return true;
            }

            // ── `CP1b`: LA CAPA 1 SE PREGUNTA UNA SOLA VEZ ───────────────────
            //
            // `CP1` resolvía adentro de `translateSDLEventToAction`, que sólo
            // corre en tres fases. La sección 2 corre en TODAS, así que al
            // moverla a verbos había dos formas: resolver dos veces por evento,
            // o resolver una y bajar la lista. Va la segunda (fork 2b) — dos
            // resoluciones del mismo evento pueden divergir el día que alguien
            // toque una, y nada se pondría rojo.
            //
            // **Y va ACÁ y no en el tope de la función**: arriba está el modal
            // de `entrada`, que traga todo a propósito (es lo que impide que
            // 'k','a','1' disparen atajos mientras el jugador escribe). Subirla
            // más sería resolver eventos que ese modal ya decidió comerse.
            //
            // Debajo de esto, el `SDL_Event` crudo sólo lo miran tres cosas: el
            // early-return del QTE (`E-11`, que necesita el `type`), la costura
            // de hotspots de la sección 3 (que es geometría, no teclas) y el
            // `F3` (fork 3b: dominio `dev`, el VN resuelve uno solo).
            const std::vector<std::string> verbos =
                m_input->resolverTodos(e, "vn");

            // ═══════════════════════════════════════════════════════════
            // 0. Historial modal: mientras el backlog está abierto captura TODO
            //    el input (scroll, cerrar) SIN importar la fase de fondo. Antes
            //    el input del historial dependía de que m_phase estuviera en el
            //    set delegado (sección 1), así que abrirlo desde WaitingClick
            //    (verbo 'historial' de un hotspot) dejaba la rueda muerta y el
            //    ESC abría el menú de pausa en vez de cerrar el log. Un overlay
            //    modal captura su propio input. Va ANTES del ESC global y de la
            //    costura de hotspots (mismo idiom modal que WaitingTextInput).
            // ═══════════════════════════════════════════════════════════
            if (m_dialogBox.isShowingHistory())
            {
                // Acá NO hace falta pasar la tecla cruda, y no es un olvido: es
                // la ley 5 de `QteGate.h`. Con el log abierto el QTE no reclama
                // la tecla (`qteReclamaTecla` lo gatea), así que un `QTEKey` no
                // puede llegar hasta este punto — y si llegara, este overlay se
                // lo comería sin cerrarse y dejaría al jugador adentro (§7.88).
                InputAction action = translateSDLEventToAction(e, m_phase, verbos);
                if (action != InputAction::None)
                    m_dialogBox.handleAction(action);
                return true; // el log se traga todo mientras está abierto
            }

            // ── Ranura / slot-widget (juice-hover): tracking del hover por PUSH
            //    (VNScriptState no cachea el mouse → no hay pull como el choice).
            //    El motion actualiza m_hoveredSlot; incondicional y barato
            //    (hitTestSlotWidget devuelve -1 con el vector vacio → no-op sin
            //    slots). NO consume: el motion sigue su curso (otros hovers). Va
            //    TRAS los modales (confirm/textinput/historial, que retornan
            //    antes) → no resalta detras de un modal. Calco de m_choiceHover.
            // *(Acá vivía el push de `m_hoveredSlot`, que era la ÚNICA familia
            // con hover fresco al momento del click. Se fue a
            // `refrescarHoverDelPuntero` junto con las otras tres, que ahora lo
            // tienen por el mismo mecanismo.)*

            // ── `CP5`: EL PUNTERO RECUPERA EL ANILLO ────────────────────────
            //
            // Este es el cable que sólo el state puede poner: es el único que
            // ve el `SDL_Event` crudo, y la caja recibe `InputAction` —donde un
            // click y un ESPACIO son **el mismo** `Confirm`, porque `avanzar`
            // lleva las cuatro teclas Y el `CLIC` en la tabla del `CP0`.
            //
            // **El `BUTTONDOWN` no es simetría con el `MOTION`:** sin él, un
            // click en el vacío con un foco de teclado encendido llegaría con
            // el índice intacto y ELEGIRÍA la opción focada en vez de completar
            // el caption — la regresión de `D-36` sobre una feature que ya
            // shipeó. Lo mide `harness_foco` §6.3 contra §6.4.
            //
            // Va DESPUÉS de los modales (que retornan antes) por la misma razón
            // que su vecino de arriba: no le devuelve el anillo al puntero
            // mientras un modal se está comiendo el input.
            if (e->type == SDL_MOUSEMOTION ||
                e->type == SDL_MOUSEBUTTONDOWN)
            {
                m_dialogBox.punteroManda();
                m_focoDelPuntero = true;   // `CP6`: la misma ley, cuatro familias

                // ── `CP7`: Y ACÁ SE DERIVA, ANTES DE QUE NADIE CLICKEE ───────
                //
                // Es la mitad de TEMPO. `processEvents()` corre ANTES de
                // `update()` (`Engine.cpp:481` vs `:497`), así que hasta hoy los
                // tres caminos de click que leen un hover DERIVADO —el `enlace`
                // (`hoveredInteractive`), el `choice` (`m_choiceHover`) y los
                // seis del chrome (`m_mouseVX`)— leían el del batch ANTERIOR.
                //
                // Con un mouse eso era invisible: uno mueve, pasan frames,
                // después clickea. **Con un dedo no hay «mover antes»** —SDL
                // sintetiza MOTION+DOWN+UP en el mismo batch—, así que el tap
                // acertaba al objetivo del tap ANTERIOR. Lo mide
                // `harness_puntero` 6.3 contra 4.1-4.3.
                //
                // *`CP5` escribió que acá no había skew (`VNDialogBox.cpp:1536`)
                // y la frase era cierta: comparaba la ley vieja con la nueva,
                // las dos igual de viejas. Lo que venció es su conclusión.*
                refrescarHoverDelPuntero();
            }

            // ═══════════════════════════════════════════════════════════
            // ── Control de settings: agarrar arrastre. Intercepta el
            //    mousedown ANTES del enlace/advance si cae sobre un control. ───
            //    SIN gate de fase (β menú · CP0): el grab ya está hit-test-guarded
            //    (hitTestControl >= 0 abajo) → solo agarra ENCIMA de un slider; un
            //    clic que no es control cae limpio al choice/hotspot. Quitar la
            //    exclusión WaitingClick/Choice/QTE deja que los sliders sobrevivan
            //    el hold natural del menú (esperar_click/elegir). First-hit-wins;
            //    no fusionar fase con posición (§7.117/§7.6).
            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
            {
                float vx, vy;
                mouseToVirtual((float)e->button.x, (float)e->button.y, vx, vy);

                // `CP9b`: la familia de tecla se pregunta ANTES, y es
                // first-hit-wins igual que el resto. Un widget de tecla no
                // arrastra ni ajusta: abre la captura y se va.
                const int hitTecla = hitTestTeclaWidget(vx, vy);
                if (hitTecla >= 0)
                {
                    abrirCaptura(hitTecla);
                    return true;
                }

                int hit = hitTestControl(vx, vy);
                if (hit >= 0)
                {
                    BoundControl& bc = m_controls[hit];
                    if (isControlLocked(bc.desc->key))
                        return true;             // locked: traga el click, no actúa
                    if (bc.render == CtrlRender::Toggle)
                    {
                        // Flip discreto: lee el estado VIVO (§7.132, no bc.value que
                        // es de hace 1 frame) y lo invierte. Sin drag.
                        bool on = readControlValue(bc.desc->key) >= 0.5f;
                        float next = on ? 0.0f : 1.0f;
                        writeControlValue(bc.desc->key, next);
                        bc.value = next;         // refleja ya; el read-back lo confirma
                    }
                    else if (bc.render == CtrlRender::Arrows)
                    {
                        // Clic discreto por zona (sin drag). ◄ −1, ► +1, clamp a
                        // [vmin,vmax]. Estado VIVO (§7.132). Centro = no-op.
                        int dir = arrowDir(bc, vx);
                        if (dir != 0)
                        {
                            int cur = (int)(readControlValue(bc.desc->key) + 0.5f);
                            int nxt = cur + dir;
                            if ((float)nxt < bc.vmin) nxt = (int)bc.vmin;
                            if ((float)nxt > bc.vmax) nxt = (int)bc.vmax;
                            writeControlValue(bc.desc->key, (float)nxt);
                            bc.value = (float)nxt;
                        }
                    }
                    else if (bc.render == CtrlRender::Boxes)
                    {
                        // Selección DIRECTA (no ±1 como flechas): clic en la caja
                        // i escribe el índice i absoluto. Sin drag, sin estado vivo
                        // (el índice es absoluto). boxIndexAt clampa a [0,N-1].
                        int bi = boxIndexAt(bc, vx);
                        if (bi >= 0)
                        {
                            writeControlValue(bc.desc->key, (float)bi);
                            bc.value = (float)bi;
                        }
                    }
                    else
                    {
                        m_draggingControl = hit;
                        applyControlDrag(hit, vx);   // salta al valor del click
                    }
                    return true;                 // consume: no enlace/advance debajo
                }

                // ── Ranura / slot-widget (Ola 2 · CP3): clic directo en una
                //    tarjeta → su accion al slot N. Reusa el vx/vy ya resuelto;
                //    first-hit-wins tras el control (no se solapan en practica:
                //    una pantalla de slots no lleva sliders). hit-test-guarded
                //    igual → un clic fuera de toda tarjeta cae limpio al
                //    enlace/advance de abajo.
                int slotHit = hitTestSlotWidget(vx, vy);
                if (slotHit >= 0)
                {
                    dispatchSlotAction(slotHit);
                    return true;                 // consume
                }
            }

            // 0.5 Click sobre un enlace (CP3 átomo de texto). ANTES de la
            //     delegación al dialog box → le gana al Advance. §7.38: este
            //     es el orden de intercepción. Fuera de WaitingChoice/QTE/
            //     WaitingClick (modales, con dueño propio de input).
            // ═══════════════════════════════════════════════════════════
            if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT
                && m_phase != VNPhase::WaitingChoice
                && m_phase != VNPhase::WaitingQTE
                && m_phase != VNPhase::WaitingClick)
            {
                // UI-hold puede recibir un click sintético sin un frame de
                // movimiento previo. Recalcular el mismo hover con el punto
                // fresco del evento conserva la ruta única de enlaces, pero
                // evita depender de un hover de frame anterior.
                if (m_phase == VNPhase::WaitingInterface)
                {
                    float clickVX, clickVY;
                    mouseToVirtual((float)e->button.x, (float)e->button.y,
                                   clickVX, clickVY);
                    const float normX = (float)e->button.x
                        / (float)std::max(m_winW, 1);
                    const float normY = (float)e->button.y
                        / (float)std::max(m_winH, 1);
                    m_sceneLayers.updateInteractiveHover(
                        clickVX, clickVY, normX, normY,
                        m_hasFont ? &m_textRenderer : nullptr, &m_fontManager);
                }
                if (SceneElement* link = m_sceneLayers.hoveredInteractive())
                {
                    // ── `CP2a`: EL PICK DEL POOL, ANTES DE LA LEY DEL AVISO ──
                    //
                    // **Éste es el "revisar aquí" del comentario de abajo**, y
                    // llegó: un pool es un modal con N salidas. Va antes porque
                    // el desenlace es otro —el bloque resuelve su propia espera—
                    // y porque un enlace AJENO no puede cerrar el pool: se
                    // consume el click y se sigue esperando un pick.
                    if (m_palabrasActivas)
                    {
                        if (!esPalabraDelPool(link)) return true;
                        if (m_hasAudio)
                        {
                            const std::string& cs =
                                m_dialogBox.activeChoiceClickSound();
                            if (!cs.empty()) m_audio.playSound(cs);
                        }
                        return picarPalabra(link);
                    }

                    // modalidad-aviso: clickear el enlace-OK levanta la modalidad
                    // (era la única salida). Se apaga aunque navegue o continúe.
                    // Asume aviso = una salida (el OK); si un modal tuviera varios
                    // enlaces, revisar aquí.
                    m_modalRequired = false;
                    m_modalScopeOwner = 0;

                    // Sonido de click (renderer-pure: la capa reporta, el state
                    // toca). Reusa el sonido de click del choice (familia UI, §7.6).
                    if (m_hasAudio)
                    {
                        const std::string& cs = m_dialogBox.activeChoiceClickSound();
                        if (!cs.empty()) m_audio.playSound(cs);
                    }
                    if (m_phase == VNPhase::WaitingInterface && link->target.empty())
                    {
                        ++m_cmdIndex;
                        m_phase = VNPhase::Executing;
                        return true;
                    }
                    if (!link->target.empty())
                    {
                        const std::string destino = link->target;  // copia: un salto invalida `link`
                        const eclips::TargetVeredicto ver = dispatchTarget(
                            destino, eclips::system_flow::PantallaCara::Enlace);
                        // CP0 `mutar` · F2: el "continuar" del enlace es
                        // IDÉNTICO al de un enlace sin flecha — o sea NO
                        // consumir, para caer al avance normal de abajo. Ésta es
                        // la cara que el fork 1b no podía expresar: el
                        // `return true` vive acá, no en el dispatcher.
                        if (ver != eclips::TargetVeredicto::Mutacion &&
                            ver != eclips::TargetVeredicto::MutacionRota)
                            return true;                // consumir: NO avanza
                    }
                    // target vacío = continuar: NO consumir → cae al avance normal
                    // abajo (el OK avanza la línea como un avance estilizado).
                }
            }

            // ═══════════════════════════════════════════════════════════
            // Secuencias del elegir (DISENO_SECUENCIA SQ-9): despues del
            // historial y antes de la caja. Solo KEYDOWN sin repeat en
            // WaitingChoice. Completar consume; el medio no (SQ-6 / R2).
            if (m_phase == VNPhase::WaitingChoice
                && e->type == SDL_KEYDOWN
                && e->key.repeat == 0
                && !m_secuenciasArmadas.empty())
            {
                const std::int32_t tecla =
                    static_cast<std::int32_t>(e->key.keysym.sym);
                const int gana = vnSecuenciasAvanzar(m_secuenciasArmadas, tecla);
                if (gana >= 0)
                {
                    const std::string target =
                        m_secuenciasArmadas[static_cast<size_t>(gana)].target;
                    m_secuenciasArmadas.clear();
                    // SQ-7.1: cerrar lista y reloj ANTES de despachar (R1/R10).
                    m_dialogBox.endChoices();
                    m_currentChoices.clear();
                    const TargetVeredicto ver = dispatchTarget(
                        target, system_flow::PantallaCara::Eleccion);
                    // Sin sonido de clic. Sin on_choice_made (R8).
                    if (ver == TargetVeredicto::Mutacion ||
                        ver == TargetVeredicto::MutacionRota)
                    {
                        ++m_cmdIndex;
                        m_phase = VNPhase::Executing;
                    }
                    return true; // consumir la tecla que completo (R2: A no prende auto)
                }
                // No completo: no consumir (las flechas siguen al foco).
            }
            // 1. Delegar al nuevo sistema de diálogo (VNDialogBox)
            //    Tiene prioridad absoluta durante fases interactivas.
            // ═══════════════════════════════════════════════════════════
            // WaitingClick NO entra aquí: durante esperar_click los clics deben
            // llegar a la costura de hotspots (sección 3, abajo) → dispatchTarget,
            // NO al dialog box (que los volvería un Advance). El dialog box sí
            // recibe input en WaitingInput/Choice/QTE (advance, opciones, tecla QTE).
            if (m_phase == VNPhase::WaitingInput || m_phase == VNPhase::WaitingChoice ||
                m_phase == VNPhase::WaitingInterface ||
                m_phase == VNPhase::WaitingQTE)
            {
                InputAction action = translateSDLEventToAction(e, m_phase, verbos);

                // ── `CP6`: EL ANILLO SE ATIENDE ACÁ, ANTES DE LA CAJA ────────
                //
                // Va **antes** de delegar porque el anillo dejó de ser de la
                // caja: su lista cruza `control`, `ranura` y `enlace`, que la
                // caja no ve. Lo que la caja recibe es su segmento ya resuelto.
                //
                // El `false` de `manejarFoco` (sin focables) deja que la tecla
                // siga su camino a los atajos globales, igual que el `false` de
                // `moverFoco` en `CP5`.
                if (action == InputAction::FocusUp   ||
                    action == InputAction::FocusDown ||
                    action == InputAction::FocusLeft ||
                    action == InputAction::FocusRight)
                {
                    if (manejarFoco(action)) return true;
                    action = InputAction::None;   // que caiga como caía antes
                }

                // Ley 3 para las familias que no son choice. Si el foco está en
                // un choice —o lo maneja el puntero— esto contesta `false` y el
                // camino sigue siendo el de `CP5`, byte por byte.
                if (action == InputAction::Confirm && confirmarFocado())
                    return true;

                if (action != InputAction::None)
                {
                    // ── `QTE-SIN-PUERTA`: LA CARGA (eslabón 2) ───────────────
                    //
                    // Acá decía `handleAction(action)`, y la firma es
                    // `(action, choiceIndex = -1, qteKey = 0)`: **la tecla cruda
                    // llegaba en 0**, así que el comparador de la caja comparaba
                    // 0 contra la tecla pedida y fallaba siempre. Arreglar el
                    // productor solo no habría cambiado nada.
                    //
                    // Sale por `inputBindDeEvento` (átomo de teclado CP0) y no
                    // por `e->key.keysym.sym` pelado, y eso NO es ceremonia: es
                    // `H10`. Esa función mira el `type` ANTES de tocar el union,
                    // así que un evento que no es de teclado contesta `Ninguna`
                    // en vez de devolver basura del `keysym`. Éste es su primer
                    // caller de producción.
                    const eclips::InputBind bind = eclips::inputBindDeEvento(e);
                    const uint32_t teclaCruda =
                        (bind.kind == eclips::InputBind::Kind::Tecla)
                            ? static_cast<uint32_t>(bind.tecla)
                            : 0u;

                    bool consumed = m_dialogBox.handleAction(action, -1, teclaCruda);
                    if (consumed)
                        return true; // El componente lo ha gestionado
                }
            }

            // ═══════════════════════════════════════════════════════════
            // 2. Atajos globales (menú, guardado rápido) que deben funcionar
            //    incluso si no estamos en fase de diálogo.
            // ═══════════════════════════════════════════════════════════
            // ── `CP1b` DEL ATOMO DE TECLADO: ACA VIVIA LA SEGUNDA CASCADA ────
            //
            // Seis comparaciones de `SDLK_*` que mezclaban las tres capas otra
            // vez, y con el agravante de que el sello del MOTOR estaba escrito
            // sobre cuatro keycodes: `(isModalRequired() || m_cinematicaActive)
            // && (k == SDLK_k || k == SDLK_l || k == SDLK_F5 || k == SDLK_F9)`.
            // El día que `CP4` deje remapear `guardado_rapido` a `Ñ`, ese sello
            // quedaba cuidando una puerta que el jugador ya movió — que es
            // palabra por palabra la aceptación fundadora del contrato, y hasta
            // este lote **habría fallado** (`E-3`). Acá se cierra la ficha
            // `SELLO-MOTOR-SOBRE-TECLA`: ahora nombra los cuatro VERBOS.
            //
            // Son SEIS teclas y no cinco: la §18 del contrato dejó el `ESC`
            // afuera de su lista, y `menu` también vive acá. Es de otra especie
            // que los otros — `vnAccionDeVerbos` **sí** lo despacha (a `Cancel`),
            // así que el `ESC` tiene dos consumidores en cascada: primero la
            // caja, y si no lo consumió, esto.
            //
            // La ley es pura y vive en `VNVerbRoute.h` (`vnAtajoDeVerbos`), con
            // su guardián de no-op en la §9 de `harness_ruta` — 16 combinaciones
            // de contexto × 7 máscaras × 30 eventos. Acá abajo no queda una sola
            // decisión: queda el EFECTO y la VOZ, que es lo único que un header
            // puro no puede hacer.
            //
            // **La voz sale de la ley y no de acá**, y no es ceremonia: un
            // guardián que compare sólo "qué acción salió" da verde con el aviso
            // muerto, y el aviso es lo único que el creador tiene para saber por
            // qué su `F5` no abre. La §11 del harness las afirma byte a byte.
            {
                const eclips::VNAtajo atajo =
                    eclips::vnAtajoDeVerbos(verbos, atajoContexto());

                const std::string voz =
                    eclips::vnVozDeAtajo(atajo.causa, atajo.sujeto);
                if (!voz.empty()) std::cerr << voz << "\n";

                switch (atajo.efecto)
                {
                case eclips::VNAtajoEfecto::Popear:
                    // Rol Menu: volver, revela lo de abajo. **Sin consultar el
                    // sello, y es deliberado** — la ley lo decidió arriba: esto
                    // no ABRE nada, y sellarlo deja al jugador encerrado en un
                    // submenú (§8 del átomo y `D-46`). Guardián: §10.4.
                    cerrarPantalla();   // lote B: sin bloque, es el pop de siempre
                    break;
                case eclips::VNAtajoEfecto::AbrirPausa:
                    openPauseMenu();
                    break;
                case eclips::VNAtajoEfecto::GuardarRapido:
                    saveToSlot(0);
                    break;
                case eclips::VNAtajoEfecto::CargarRapido:
                    loadFromSlot(0);
                    break;
                case eclips::VNAtajoEfecto::AbrirMenuGuardar:
                    openSaveMenu(VNSaveMenu::Mode::Save);
                    break;
                case eclips::VNAtajoEfecto::AbrirMenuCargar:
                    openSaveMenu(VNSaveMenu::Mode::Load);
                    break;
                case eclips::VNAtajoEfecto::Ninguno:
                    break;   // o no era mío, o alguien lo vetó y ya habló
                }

                if (atajo.consume) return true;
            }

            // Debug hotspots (F3). **Queda hardcodeada por firma del fundador
            // (fork 3b) y no es un olvido:** es dominio `dev`, o sea
            // export-strip, y este state resuelve UN dominio (`vn`). El día que
            // el `dev` tenga dueño, se muda entero. Guardián de que no se coló a
            // la ley: la §9.3 de `harness_ruta`.
            if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F3)
            {
                m_debugHotspots = !m_debugHotspots;
                return true;
            }

            // ── `CP7`: EL INYECTOR DE TAPS (dominio `dev`, fork 8a) ──────────
            //
            // **Existe porque el feel-test del fundador no puede dar esta
            // prueba: su máquina no tiene pantalla táctil.** Sin esto, el cable
            // de los SIETE consumidores del puntero no lo ve nadie — la batería
            // afirma la ley pura y no alcanza estas dos TUs.
            //
            // `F6` toca ARRIBA y `F7` toca ABAJO, en coordenadas VIRTUALES
            // fijas. Que sean fijas y no *"donde está el cursor"* es lo que hace
            // que la sonda mida algo: mover el cursor emitiría un `MOUSEMOTION`
            // **de mouse** y dejaría el hover ya resuelto antes del tap, que es
            // justo la condición que el defecto necesita para no aparecer. El
            // tap táctil de verdad llega **sin nada previo**, y así se reproduce.
            //
            // Va acá, junto al `F3`, por dos razones: es su mismo dominio y su
            // mismo trato (comparación cruda — `dev` no lo resuelve nadie,
            // ficha `DEV-SIN-DUENO`), y tiene que quedar ARRIBA de la sección 3,
            // que durante `esperar_click` se traga todo.
            // ── ⚠ POR QUE VA DETRAS DE UNA CONSTANTE Y SUS VECINAS NO ───────
            //
            // `F3`, `F8` y `F10` **son inofensivas**: alternan un overlay,
            // recargan texturas, cambian vsync. Éstas no — **inyectan input de
            // jugador**. En un binario distribuido, apretar `F6` activaría el
            // enlace que esté en esa posición y navegaría de escena, y el
            // jugador no tendría forma de entender por qué. Es una clase de
            // riesgo distinta a la de sus vecinas del dominio `dev`, así que no
            // hereda su trato.
            //
            // Compila a NADA con la constante en `false`, que es como queda.
            // Correr la sonda ya pide tocar `main.cpp` y recompilar; esto es una
            // línea más en ese mismo ritual, y a cambio el injector no existe en
            // lo que se entrega. *(Lo encontró la pregunta de cierre del
            // fundador, con el lote ya verde.)*
            constexpr bool kSondaTouch = false;
            if (kSondaTouch && e->type == SDL_KEYDOWN &&
                (e->key.keysym.sym == SDLK_F6 || e->key.keysym.sym == SDLK_F7))
            {
                const float tvx = 780.0f;
                const float tvy = (e->key.keysym.sym == SDLK_F6) ? 380.0f : 700.0f;
                int vpX, vpY, vpW, vpH;
                getVirtualViewport(vpX, vpY, vpW, vpH);
                const int wx = (int)(tvx * (float)vpW
                                   / (float)viewport::width()) + vpX;
                const int wy = (int)(tvy * (float)vpH
                                   / (float)viewport::height()) + vpY;
                std::cout << "[sonda-touch] tap tactil en virtual ("
                          << tvx << "," << tvy << ") -> ventana ("
                          << wx << "," << wy << ")" << std::endl;
                empujarTapTactil(wx, wy);
                return true;
            }

            // ═══════════════════════════════════════════════════════════
            // 3. Hotspots click (esperar_click) — manejo directo mientras
            //    no esté migrado a VNDialogBox.
            // ═══════════════════════════════════════════════════════════
            if (m_phase == VNPhase::WaitingClick)
            {
                if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
                {
                    float vx, vy;
                    int mx = e->button.x, my = e->button.y;
                    mouseToVirtual((float)mx, (float)my, vx, vy);

                    std::string hit = m_hotspots.testClick(vx, vy);
                    if (!hit.empty())
                    {
                        for (const auto& mapping : m_clickMappings)
                        {
                            if (mapping.label == hit)
                            {
                                std::cout << "[VNScriptState] Hotspot '" << hit << "' -> destino '" << mapping.target << "'" << std::endl;
                                // Átomo: misma costura unificada que choice. Push
                                // de overlay desde handleEvent es seguro (el ESC ya
                                // pushea así, shippeado); solo pop diferiría.
                                const std::string destino = mapping.target;  // copia: un salto puede rearmar los mappings
                                const eclips::TargetVeredicto ver = dispatchTarget(
                                    destino, eclips::system_flow::PantallaCara::EsperarClick);
                                // CP0 `mutar` · F2: el "continuar" del hotspot es
                                // resolver la espera. `m_cmdIndex` apunta al
                                // `esperar_click` mismo, así que avanza uno.
                                if (ver == eclips::TargetVeredicto::Mutacion ||
                                    ver == eclips::TargetVeredicto::MutacionRota)
                                {
                                    ++m_cmdIndex;
                                    m_phase = VNPhase::Executing;
                                }
                                return true;
                            }
                        }
                    }
                }
                return true; // Consumir todo durante esperar_click
            }

            // Si llegamos aquí, el evento no nos interesa
            return false;
        }

        // ── Skip-frame (Perfilado §6.3, CP2.0): ¿el VN está en reposo visual? ──
        // CONSERVADOR POR DISEÑO. El modo de falla peligroso es el falso-POSITIVO
        // (saltar mientras algo anima → congelado); el inofensivo es el falso-
        // negativo (no saltar estando quieto → solo se pierde el premio). Por eso:
        // default a NO-quiescente, y true SOLO si TODO lo conocido dice quietud.
        // Cada checkpoint CP2.x precisa una over-aproximación (marcadas ↯),
        // aflojando SOLO cuando la query fina existe — nunca al revés (§polaridad).
        //   Alcance: AMBOS roles (CP2.7 quitó el gate de Menu). El reposo de un
        //   VN rol Game es WaitingInput con caja visible — lo gobiernan el
        //   indicador per-tema (CP2.6) y los efectos de texto, no el rol.
        // ── FADE-DE-ESTADO-SIN-PERILLA: quién es menú y quién es juego ───────
        // El rol ya existía y lo decide el caller (`VNRole`); esto sólo lo
        // publica hacia `StateStack`, que es SDL-free y donde la pregunta se
        // puede hacer sin arrastrar este header.
        bool esPantallaDeMenu() const override { return m_role == VNRole::Menu; }

        // ── LOTE B · `el-menu-se-pinta-encima` (`D-15`) ──────────────────────
        //
        // `m_fadeAlpha` NO cambió de tipo ni de rango: cambió de LECTURA. Antes
        // se leía como *«cuánto velo negro hay encima»*; ahora se lee como
        // **«cuánto NO se me ve»**, que es lo mismo para `fade` y además sirve
        // para las dos direcciones — la entrada corre 1→0 y la salida 0→1.
        //
        // Esa relectura es la que hace que el lote no necesite una segunda
        // variable, y con eso **el renglón de quiescencia ya estaba escrito**:
        // `isQuiescent` pregunta `m_fadingIn || m_fadeAlpha != 0.0f` (:1593) y
        // eso cubre la rampa de salida sin agregar una línea. Con una variable
        // propia habría que acordarse, y olvidarse congela la salida a mitad de
        // camino — la forma exacta de `FADE-TAPADO-MUDO`.

        // El tipo que gobierna la rampa que está corriendo AHORA.
        TransicionTipo tipoDeRampaViva() const
        {
            return m_cerrando ? m_cierreSpec.tipo : m_transicionSpec.tipo;
        }

        bool rampaViva() const
        {
            return m_fadingIn || m_cerrando || m_fadeAlpha > 0.0f;
        }

        // ⚠ LAS TRES CONDICIONES SON NECESARIAS, y la tercera es la que no se
        // ve venir: **`limpiar=false` es una DEUDA** — alguien más abajo tiene
        // que limpiar el frame, o el buffer acumula basura (`Draw2D.h:71`). Si
        // somos el state más bajo no hay nadie, así que ahí `encima` no aplica
        // y se pinta como siempre. `origenDe(this)` es la MISMA pregunta que
        // usó la ley del nacimiento: una fuente, dos consumidores.
        bool pintaEncima() const
        {
            if (!rampaViva()) return false;
            if (tipoDeRampaViva() != TransicionTipo::Encima) return false;
            return m_stateStack && m_stateStack->origenDe(this).hay;
        }

        // Y de acá cuelgan las DOS mitades que `§11.7c` daba por una sola: que
        // la pila descienda hasta el de abajo, Y que nosotros no lo borremos un
        // paso después (el `limpiar=` del render). Derivadas del mismo
        // predicado a propósito — es la forma que el splash ya shipeó, y por la
        // que `isTransparent()` no puede volver a separarse de su consumidor.
        bool isTransparent() const override { return pintaEncima(); }

        // ── EL CIERRE, y POR QUÉ NO LO USAN LOS CATORCE POPS ─────────────────
        //
        // `VNScriptState` tiene CATORCE sitios que popean, y son TRES ESPECIES
        // con destinos opuestos (censadas en `§11.9a`). Por acá pasan sólo las
        // CUATRO del jugador -- `ESC`/atrás, `fin`, el verbo `fin` y la escena
        // que se acaba. Las otras diez NO, y cada grupo por su razón:
        //
        //   * los CUATRO de error (parse falló, escena inexistente...) tienen
        //     que morir rápido: un guion roto que se desvanece lentamente tarda
        //     más en avisarle al creador que se rompió;
        //   * los SEIS estructurales (`cmdPopToBottom`, `cmdPopAbove`) matan
        //     states que NUNCA PIDIERON IRSE, así que el que tendría que
        //     esperar la rampa no es el que se va sino el que mata. Eso es otro
        //     mecanismo y `D-15.2` lo deja afuera **declarándolo**.
        //
        // No lleva gate de rol a propósito: `abrir` tampoco lo tiene -- el
        // arranque del juego es un `abrir` -- y una pantalla que se va es una
        // pantalla que se va. Un gate acá sería una regla que el creador no
        // puede ver desde su `.ecl`.
        void cerrarPantalla()
        {
            // Idempotente: un segundo `fin` durante la rampa no la reinicia.
            // (Hoy `handleEvent` ya no deja entrar nada mientras cerramos, así
            // que esto es cinturón sobre un camino interno, no sobre el input.)
            if (m_cerrando) return;

            // Sólo el cierre normal del jugador arma el recibo. onExit queda
            // fuera porque también lo disparan error, replace y desmontajes
            // estructurales; el camino instantáneo debe armarlo antes del pop.
            if (m_regresoAlOrigen && !m_regresoAlOrigen->cierreNormalSolicitado)
                m_regresoAlOrigen->cierreNormalSolicitado = true;

            static const TransicionesTabla kSinManifiesto{};
            const TransicionesTabla& tabla =
                m_manifest ? m_manifest->transiciones : kSinManifiesto;

            m_cierreSpec = transicionSpecDeCierre(tabla);

            // El default es corte seco (`ninguna`), o sea que TODO proyecto que
            // existe hoy cae por acá y se comporta exactamente igual que antes:
            // el pop se encola en el acto. Es `D-13.3` en el tercer renglón.
            if (m_cierreSpec.tipo == TransicionTipo::Ninguna
                || m_cierreSpec.duracion <= 0.0f)
            {
                m_stateStack->cmdPop(this);
                return;
            }

            m_cerrando  = true;
            m_fadeAlpha = 0.0f;   // arranca visible; la rampa lo lleva a 1
        }

        bool isQuiescent() const override
        {
            // Fase: solo fases de reposo. Executing procesa; Fading/QTE/TextInput
            // animan (transición / timer / cursor que parpadea). Una interfaz
            // estática también puede dormir, aunque conserve foco y destinos.
            if (!vnPhaseCanQuiesce(m_phase)) return false;

            // Indicador de avance, per-tema (CP2.6): la caja dibuja el indicador
            // con typewriter completo (o {esperar_click}), y su animación está
            // GATED POR EL TEMA (vaivén/parpadeo, default off — §6.4). La query
            // espeja la condición de draw completa (§7.282) y es FASE-agnóstica:
            // el indicador también se dibuja en WaitingClick (VNDialogBox.cpp:677),
            // no solo en WaitingInput. Tema estático → quiesce; ctc DDLC → no.
            if (m_dialogBox.isIndicatorAnimating()) return false;
            // …y el texto ya revelado puede seguir ANIMANDO ({ondulado}/{temblar}
            // leen el reloj en el draw — fuente que ni §6.3 ni la auditoría CP2.0
            // listaban; la destapó CP2.6 al permitir cajas visibles en reposo).
            if (m_dialogBox.hasAnimatedTextFX()) return false;
            // …y la caja puede estar ENTRANDO (`caja-entrada`): mientras su
            // alpha no llegó a 1, cada frame cambia píxeles. NINGUNO de los
            // frenos de acá lo cubría: una línea de tres letras termina de
            // escribirse antes de que la caja termine de entrar, y un
            // `mostrar_caja` sin diálogo nuevo no enciende ni el typewriter ni
            // el indicador ni los efectos. Sin esta línea la entrada se congela
            // a mitad de camino y se lee como "a veces no funciona" — la forma
            // exacta de `FADE-TAPADO-MUDO`. Con la clave del tema sin declarar
            // devuelve false siempre y la quiescencia queda idéntica a hoy.
            if (m_dialogBox.isAppearing()) return false;

            // Typewriter aún revelando. OJO (§7.38): isWaiting() == chars<size
            // ("sigue escribiendo"), NO "esperando input" — el nombre engaña.
            if (m_dialogBox.isWaiting()) return false;

            // Auto / skip armados (ambos lados: el state y la caja).
            if (m_autoMode || m_skipMode) return false;
            if (m_dialogBox.isAutoMode() || m_dialogBox.isSkipMode()) return false;

            // Transición / fade de pantalla / cinemática en curso.
            if (m_activeTransition  != VNTransitionType::None) return false;
            if (m_pendingTransition != VNTransitionType::None) return false;
            if (m_fadingIn || m_fadeAlpha != 0.0f) return false;
            if (m_cinematicaActive) return false;

            // Overlays / modales / interacción activa.
            if (m_showingHistory || m_dialogBox.isShowingHistory()) return false;
            if (isModalRequired()) return false;
            if (m_displayChange.active()) return false;
            if (m_confirmActive) return false;
            if (m_draggingControl != -1) return false;

            // Timers que cuentan (toast, esperar N, esperar_animacion, →gameplay).
            if (m_toastTimer > 0.0f) return false;
            if (m_waitTimer  > 0.0f) return false;
            if (m_waitingForAnim) return false;
            if (m_waitingForGameplay) return false;

            // Random events activos: un timer que puede mutar la escena sola.
            if (m_eventsActive) return false;

            // Juice-hover: slot del menú o choice bajo el cursor.
            if (m_hoveredSlot != -1) return false;
            if (m_dialogBox.currentChoiceHover() != -1) return false;
            // …y el ease-OUT del juice tras salir el cursor: la escala regresa a
            // reposo asintóticamente. Quiescer aquí la congela → dos opciones
            // pegadas en hover (bug del feel-test CP2.0). Espera a que asiente.
            if (!m_dialogBox.isChoiceJuiceSettled()) return false;

            // ── Personajes (m_activeChars): fade/slide en curso, o FBO sucio ──
            // A diferencia del juice de choices (asintótico → pide epsilon), el
            // fade/slide de personaje es LINEAL CON CLAMP exacto al target
            // (update: += step y clamp, :1361/:1379) → la comparación exacta es
            // correcta, y es LA MISMA que usa el propio update (:1375).
            //   compositeDirty solo cuenta para compuestos/3-capas: es lo que
            //   dispara la recomposición del FBO (:6251 lo apaga). En un personaje
            //   SIMPLE nace true y nunca se lee ni se apaga → chequearlo a secas
            //   mataría el premio de TODO VN con un PNG. Gated a propósito.
            for (const auto& ac : m_activeChars)
            {
                if (ac.fadeAlpha != ac.fadeTarget)   return false;
                if (ac.slideX    != ac.slideTargetX) return false;
                if ((ac.composite || ac.threeLayer) && ac.compositeDirty) return false;
            }

            // ── Anims de personaje / pantalla: ¿alguna sigue CAMBIANDO píxeles? ──
            // OJO (§7.38): CharAnim::isDone() es una query de CICLO DE VIDA ("¿se
            // poda?"), NO de "¿sigue cambiando?". Una PERSISTENTE (ZoomIn/Drop/
            // Rise) que ya llegó devuelve isDone()==false: sigue viva pero sostiene
            // un valor FIJO. Escribir `if (!a.isDone()) return false;` —el check
            // obvio— mataría el premio en silencio para todo VN con un zoom.
            //   Cambia píxeles = aterrizando (landing) | loop (Float/Pulse/Nudge:
            //   jamás paran — el personaje que respira) | one-shot o persistente
            //   TODAVÍA en curso (elapsed < duration).
            auto animCambia = [](const CharAnim& a) -> bool
            {
                if (!a.isActive()) return false;
                if (a.landing)     return true;
                if (a.isLoop())    return true;
                if (a.isPersistent() && a.elapsed >= a.duration) return false;
                return a.elapsed < a.duration;
            };
            for (const auto& kv : m_charAnims)
                for (const auto& a : kv.second)
                    if (animCambia(a)) return false;
            for (const auto& a : m_screenAnims)
                if (animCambia(a)) return false;

            // ── C-CP5: tracks nombrados — fuente NUEVA del censo (§7.287:
            // cada superficie que se suma trae las suyas). trackCambia lee el
            // cuerpo (§7.282): loop/vaiven respiran siempre; una_vez en vuelo
            // cambia; terminado sostiene un valor FIJO → concede quietud, y su
            // flanco false→true lo presenta el flush de borde (§7.285).
            for (const auto& [aliasT, ts] : m_charTracks)
                for (const auto& ti : ts)
                    if (trackCambia(ti.track)) return false;
            for (const auto& ti : m_screenTracks)
                if (trackCambia(ti.track)) return false;

            // ── Efectos de render (m_charFx): "pasada activa" (CP2.5) ──
            // Con el cuerpo leído (§7.282), no por nombre:
            //  · esporadico DORMIDO (ventana==0) no transforma nada — es
            //    invisible para la tubería (ventanaActiva(), EffectSystem.h).
            //    Concede quietud. Su disparo despierta solo: updateEffect corre
            //    aunque el render duerma (el skip no congela la lógica), el
            //    predicado voltea a false y el loop presenta en ≤4ms. El CIERRE
            //    de su ventana (activo→quiescente sin evento SDL) lo presenta
            //    el flush de borde de Engine::run — sin ese frame el front-
            //    buffer quedaría con el glitch congelado.
            //  · una_vez bloquea SIEMPRE, aun con pasada constante: su
            //    expiración cambia píxeles con el predicado ya en true, y un
            //    true→true no tiene flanco que presentar. Es finita de todos
            //    modos — el premio perdido es acotado.
            //  · continuo: solo `sustituto` emite una pasada CONSTANTE
            //    (fxSustitutoCore no lee ctx.t; TODAS las demás primitivas sí,
            //    preset glitch incluido) → solo él concede quietud.
            //  · E-CP6: un efecto LUA continuo NIEGA quietud por la misma
            //    regla (kind != Sustituto): el motor no puede saber si el
            //    transformador del creador lee ctx.t — conservador, §7.281.
            //    Dormido bajo esporadico concede arriba, por driver.
            auto fxCambia = [](const EffectInstance& fx) -> bool
            {
                if (fx.driver == FxDriver::Esporadico) return fx.ventana > 0;
                if (fx.driver == FxDriver::UnaVez)     return true;
                return fx.kind != FxKind::Sustituto;   // continuo
            };
            for (const auto& [blanco, fxStack] : m_charFx)
                for (const auto& fx : fxStack)
                    if (fxCambia(fx)) return false;

            // Capas: fade/move/sprite/consola/hover (isAnimating conservador; el
            // update de capas tickea justo esos).
            if (m_sceneLayers.isAnimating()) return false;

            // Todo lo conocido dice quietud → quiescente. Red de seguridad: el
            // heartbeat (CP3) fuerza 1 render/s por si una fuente se escapó →
            // "1s stale", nunca congelado eterno.
            return true;
        }

        // ── Update ────────────────────────────────────────────────────────────

        void update(f64 dt) override
        {
            // §1.10: Si onEnter() falló, no operar sobre datos inválidos
            // (m_dialogBox sin renderers, m_script vacío, etc.). El
            // requestPop() pendiente en StateStack nos sacará al inicio de
            // este mismo update del stack, así que en la práctica este
            // return es inalcanzable — pero blinda contra cambios futuros.
            if (m_failed) return;

            // -- EL PEDIDO DIFERIDO DE `R2`, Y POR QUE VIVE ACA --------------
            //
            // `applyTheme` no se pudo sumar a la pendiente porque ya estaba
            // APROBADA, y una aprobacion es del que la gano. Su candidato quedo
            // armado; lo unico que le falta es un corte que SI sea suyo.
            //
            // Reintentar ACA y no adentro de `applyTheme` es la mitad que hace
            // que esto sea DIFERIR y no reintentar en el vacio:
            // `commitPendingPresentation` corre DESPUES del lazo de update
            // (`Engine.cpp:554`), asi que el frame siguiente arranca con la
            // pendiente ya cerrada y el pedido entra limpio y abre la suya.
            //
            // El guard pide `m_hasCandidateTheme` a proposito: si el candidato
            // ya se consumio o se cayo, no hay nada que diferir y el flag se
            // apaga solo en el commit y en el rollback (`19.5`).
            if (m_temaDiferido && m_fontSources && m_hasCandidateTheme)
            {
                if (fontRequestPublisher(
                        *m_fontSources, FontPublisherDoor::ApplyTheme,
                        m_generationSubscription))
                    m_temaDiferido = false;
            }

            // -- Y EL MISMO DIFERIDO PARA EL SELECTOR (`PUERTAS-QUE-NO-DIFIEREN`)
            //
            // Vive aca por la misma razon que el de arriba: el corte del Engine
            // corre DESPUES del lazo de update, asi que el frame siguiente
            // arranca con la pendiente ya cerrada y el pedido abre la suya.
            //
            // RE-REGISTRA el candidato antes de pedir, y no es adorno:
            // `commitGeneration` pone `m_candidateLanguage = nullptr`
            // (`FontSourceStore.h:595`) y el rollback hace lo mismo, asi que
            // para cuando el reintento llega el store ya no lo tiene. Pedir sin
            // re-registrar hornearia con el idioma VIVO -- el mismo defecto un
            // frame mas tarde, y mudo. Guardian: `21.6`.
            if (m_idiomaDiferido && m_fontSources && m_hasCandidateLanguage &&
                m_candidateLangStore)
            {
                if (fontRequestPublisher(
                        *m_fontSources, FontPublisherDoor::Selector,
                        m_generationSubscription))
                {
                    fontSetCandidateLanguage(
                        *m_fontSources, m_candidateLangStore.get());
                    m_idiomaDiferido = false;
                }
            }

            // ── LOTE B: LA RAMPA DE SALIDA, y por qué la corre EL QUE SE VA ──
            //
            // Medido en la apertura (`§11.9c`): un `cmdPop(this)` desde
            // `handleEvent` destruye este state **en el mismo frame de la
            // tecla, antes de su propio render**. No sobrevive un frame:
            // sobrevive CERO. `processQueue()` es la primera línea de
            // `StateStack::update` y el render viene después.
            //
            // Por eso el único que puede conservar la pantalla durante el
            // cierre es ella misma: `cerrarPantalla()` NO encola el pop, prende
            // esta rampa, y el pop se pide recién cuando termina.
            //
            // Va ARRIBA del modal de display y con `return`: mientras la
            // pantalla se va no corre nada más — ni auto/skip, ni eventos
            // random, ni el reloj del diálogo. Es la misma política que
            // `allowsUpdateBelow=false` aplica hacia abajo, aplicada a nosotros.
            if (m_cerrando)
            {
                SDL_GetWindowSize(m_window, &m_winW, &m_winH);

                if (m_cierreSpec.duracion > 0.0f)
                    m_fadeAlpha += (float)dt / m_cierreSpec.duracion;
                else
                    m_fadeAlpha = 1.0f;

                if (m_fadeAlpha >= 1.0f)
                {
                    m_fadeAlpha = 1.0f;
                    // El flag no sobra: el pop es DIFERIDO, así que entre
                    // pedirlo y morir puede correr otro update si alguien mete
                    // un comando en la cola antes. Pedirlo dos veces haría que
                    // el segundo `cmdPop` se descarte con log (el solicitante
                    // ya no sería el tope) — ruido que se lee como un defecto.
                    if (!m_cierrePedido)
                    {
                        m_cierrePedido = true;
                        m_stateStack->cmdPop(this);
                    }
                }
                return;
            }

            // R4 display: el modal posee tambien el UPDATE. Solo avanzan su
            // reloj monotono y el tamano real de ventana para el viewport;
            // auto/skip/eventos/transiciones de abajo quedan congelados.
            if (m_displayChange.active())
            {
                SDL_GetWindowSize(m_window, &m_winW, &m_winH);
                if (m_displayChange.tick(dt))
                    finishDisplayChange(false, "tiempo agotado");
                return;
            }

            // F8 ya publicó tema/fuentes/hablante en onManifestReloaded
            // (corte prerender). Acá sólo queda el pending de texturas.
            syncLiveFontIdentities();

            // (3) Hot-reload de texturas (F8). INDEPENDIENTE del manifest (señal
            //     propia ungated): el creador repinta un sprite sin tocar
            //     proyecto.eclips, y aún sin manifest (modo test §7.14) debe
            //     verlo. §1.34: el trabajo GL (Texture::load) corre acá.
            //     TODO(modo-creador): se compila fuera en build de export.
            if (m_textureReloadPending)
            {
                m_textureReloadPending = false;
                m_assets.reloadAll();

                // Cabo #2: el composite cachea su resultado en cachedTex (FBO);
                // recargar las capas no lo invalida solo → marcar dirty para que
                // ensureComposite re-componga con las texturas frescas. Sprites
                // simples y caja 9-slice re-get() cada render → se refrescan solos.
                for (auto& ac : m_activeChars)
                    ac.compositeDirty = true;

                std::cout << "[VNScriptState] F8: texturas recargadas, "
                    << m_activeChars.size()
                    << " personaje(s) marcado(s) para recomponer." << std::endl;
            }

            m_totalTime += dt;
            m_vignette.update((f32)dt);
            SDL_GetWindowSize(m_window, &m_winW, &m_winH);

            // `CP7`: acá se poleaba `SDL_GetMouseState` -el primero de cinco- y
            // se le pasaba a la caja. Ahora la posición la empuja el evento, y
            // la derivación entera vive en un solo sitio. Esta llamada es la
            // SEGUNDA del frame (la primera la hizo el evento, si hubo alguno);
            // es idempotente a propósito, y de eso depende que se pueda.
            refrescarHoverDelPuntero();

            // ── Skip = HOLD real (Ctrl mantenido) ──
            // Poléo el estado del teclado en vivo y se lo paso al DialogBox.
            // Sin estado pegado: soltar Ctrl para el skip ESTE frame.
            // En WaitingChoice forzamos false (Ren'Py-style): el skip no
            // atropella elecciones. Esto también neutraliza Ctrl+R→skip:
            // al recargar soltás Ctrl al toque, el hold dura 1 frame.
            // CP2: era `keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]`
            // sobre `SDL_GetKeyboardState` — **la última lectura de teclado a
            // mano que le quedaba al VN**. Ahora pregunta por el VERBO.
            //
            // Byte-idéntico: `vn.saltar_texto` es `CTRL_IZQ, CTRL_DER`, o sea
            // exactamente esos dos keycodes, y el guardián de no-op que lo
            // prueba es `harness_juego` §8 (barre las 27 teclas del dominio).
            //
            // No es cosmético: el sello de `CP1b` ya cuidaba este verbo por
            // BOOLEANOS, así que la ficha nunca lo reclamó — pero la LECTURA
            // seguía tecla-indexada, y con ella el `CP4` que deje remapear
            // `saltar_texto` **no habría funcionado, sin poner nada rojo**.
            bool ctrlHeld = m_input->held("saltar_texto", "vn");

            // ── Skip-de-leído (Ren'Py "skip unseen text") ──
            // El skip (Ctrl) solo procede si la línea ACTUAL ya fue vista, o si
            // el jugador/tema permiten skipear texto no visto. Se evalúa contra
            // m_currentLineKey cada frame → frena en seco al topar texto nuevo y
            // es robusto ante soltar/re-presionar Ctrl (el texto no visto nunca
            // se vuelve skipeable hasta leerlo una vez). Sin latch ni estado extra.
            bool skipUnseenAllowed = m_theme.skipUnseenLocked
                ? m_theme.skipUnseen
                : eclips::Settings::skipUnseen(m_theme.skipUnseen);
            bool lineSeen = (m_seen && m_seen->has(m_currentLineKey));
            bool skipAvailable = skipUnseenAllowed || lineSeen;

            // P4: held crudo + gate aparte. La caja combina held||toggle y aplica
            // el gate (skipActive). El gate junta "puedo skipear" (visto/unseen)
            // con "no estoy en choice".
            // modalidad-aviso (cerradura-bypass): un enlace-OK requerido sella el
            // skip mientras viva (no se skipea un aviso obligatorio).
            // ── PUERTA D: el skip no es un evento, es POLLING ─────────────────
            // `saltar_texto` no entra por `handleEvent` ni por `handleAction`:
            // se polea acá cada frame. Su permiso YA era una variable, así que
            // el sello es un `&&` más — la suma literal de `D-45`, sin sitio
            // nuevo. (El toggle del botón sí pasa por la caja, y ahí también
            // está gateado.)
            bool skipAllowed = skipAvailable && (m_phase != VNPhase::WaitingChoice)
                && !isModalRequired() && !m_cinematicaActive   // CP1: cinematica sella skip
                && accionPermitida(eclips::VNAction::SaltarTexto);
            m_dialogBox.setSkipHeld(ctrlHeld);
            m_dialogBox.setSkipAllowed(skipAllowed);

            // El sello viaja a la caja cada frame (puerta B). Va ANTES del
            // `update` para que la ley 7 -el ritmo se apaga, no se traba- corra
            // sobre el mismo frame en que el `deshabilitar` se ejecutó.
            m_dialogBox.setActionsSealed(m_accionesSelladas);

            // Actualizar el sistema de diálogo a través del componente
            auto dialogResult = m_dialogBox.update(static_cast<float>(dt));

            // CP4: sonido de hover en el FLANCO (1× al entrar a un botón nuevo).
            // Solo en fase de elección; fuera, reseteo prev para re-armar el
            // flanco. La caja reporta el índice; el state toca (renderer puro).
            // B2: sellado bajo el confirm modal — el hover del choice de abajo
            // no suena mientras el modal vive (el modal congela el input pero NO
            // el update; sin esto, el "Volver" tapado seguía sonando al hover).
            if (m_phase == VNPhase::WaitingChoice && !m_confirmActive) {
                int hov = m_dialogBox.currentChoiceHover();
                if (hov != m_choiceHoverPrev) {
                    if (hov >= 0 && m_hasAudio) {
                        const std::string& hs = m_dialogBox.activeChoiceHoverSound();
                        if (!hs.empty()) m_audio.playSound(hs);
                    }
                    m_choiceHoverPrev = hov;
                }
            }
            else {
                m_choiceHoverPrev = -1;
            }

            // Procesar eventos del diálogo.
            // Advance solo se acepta cuando estamos esperando input del jugador:
            // durante Executing (procesando un 'esperar N' u otro comando que
            // tarde más de un frame), skip/auto siguen emitiendo Advance espurios
            // que dispararían m_cmdIndex fuera de rango.
            switch (dialogResult.event) {
            case VNDialogBox::Event::Advance:
                // Avanzar SOLO en diálogo normal (WaitingInput). Antes era un
                // blocklist (!= Executing && != WaitingTextInput) que dejaba
                // colar Advance en WaitingChoice/WaitingQTE/WaitingClick → un
                // clic/tecla/auto/skip en esas fases hacía m_cmdIndex++ y
                // terminaba el script sin 'fin'. Allowlist es lo correcto: las
                // fases bloqueantes resuelven por su PROPIO evento (ChoiceMade /
                // QTEFinished / costura de hotspot), nunca por Advance. §7.74:
                // auditar las fases bloqueantes como conjunto, no parchar una
                // por una. m_cmdIndex solo se toca aquí por input → guardar este
                // único punto es comprehensivo.
                // modalidad-aviso (cerradura-avance): un enlace-OK requerido sella
                // el avance plano AQUÍ — el punto único donde el input mueve
                // m_cmdIndex (§7.74). Cubre los 3 vectores (Confirm/auto/skip) de
                // una vez, incluido el "auto ya encendido". El OK SÍ avanza: limpió
                // m_modalRequired en handleEvent (sección 0.5) ANTES de este update.
                if (vnAcceptsDialogueAdvance(m_phase, isModalRequired())) {
                    // C2: marcar como vista la línea que dejamos atrás (solo en
                    // avance real; WaitingInput solo lo ponen Dialogo/Narrador →
                    // m_currentLineKey siempre válida).
                    if (m_seen) m_seen->mark(m_currentLineKey);
                    onDialogueAdvance();
                    m_cmdIndex++;
                    m_phase = VNPhase::Executing;
                }
                break;
            case VNDialogBox::Event::ChoiceMade:
                if (dialogResult.choiceIndex >= 0 && dialogResult.choiceIndex < (int)m_currentChoices.size()) {
                    // CP4: sonido de click de la opción (si el tema lo declaró).
                    if (m_hasAudio) {
                        const std::string& cs = m_dialogBox.activeChoiceClickSound();
                        if (!cs.empty()) m_audio.playSound(cs);
                    }
                    // Copias, no referencias: si el destino MUTA, abajo se
                    // limpia `m_currentChoices` y el hook leería memoria muerta.
                    const std::string target = m_currentChoices[dialogResult.choiceIndex].target;
                    const std::string label  = m_currentChoices[dialogResult.choiceIndex].label;
                    // Átomo: la elección rutea por el dispatcher unificado. El
                    // hook on_choice_made es específico de choice → se queda aquí,
                    // NO entra a dispatchTarget (un hotspot no es una "elección").
                    const eclips::TargetVeredicto ver = dispatchTarget(
                        target, eclips::system_flow::PantallaCara::Eleccion);
                    callLuaHook("on_choice_made", label, target);

                    // ── CP0 `mutar` · F2: el "continuar" de ESTA cara ───────
                    // La mutación no salta, así que la elección la resuelve
                    // acá: se apaga la lista (o el countdown de `tiempo:`
                    // seguiría vivo) y el guion sigue en el comando de al lado.
                    // `m_cmdIndex` apunta AL `elegir` mientras se espera -misma
                    // ley que `esperar_click`-, por eso avanza uno.
                    if (ver == eclips::TargetVeredicto::Mutacion ||
                        ver == eclips::TargetVeredicto::MutacionRota)
                    {
                        m_dialogBox.endChoices();
                        m_currentChoices.clear();
                        ++m_cmdIndex;
                        m_phase = VNPhase::Executing;
                    }
                }
                break;
            case VNDialogBox::Event::QTEFinished:
                if (dialogResult.qteSuccess) {
                    jumpToScene(m_qteSuccess);
                }
                else {
                    jumpToScene(m_qteFail);
                }
                break;
            case VNDialogBox::Event::OpenSave:
                openSaveMenu(VNSaveMenu::Mode::Save);
                break;
            case VNDialogBox::Event::OpenLoad:
                openSaveMenu(VNSaveMenu::Mode::Load);
                break;
            case VNDialogBox::Event::OpenLog:
                m_dialogBox.pushHistory(m_currentSpeaker, m_lastDialogue); // Opcional: actualizar historial
                // El historial se abre dentro de VNDialogBox
                break;
            case VNDialogBox::Event::OpenMenu:
                openPauseMenu();
                break;
            case VNDialogBox::Event::AutoToggled:
                // Podrías mostrar un toast como hacías antes
                break;
            case VNDialogBox::Event::SkipToggled:
                break;
            default:
                break;
            }

            // Tier 4C.6 Fase B: consumir push diferido de Gameplay2D.
            // Se setea en loadFromSlot; acá se ejecuta recién cuando el menú
            // que disparó el load ya se pop-eó.
            if (m_pendingGameplayLaunch)
            {
                m_pendingGameplayLaunch = false;
                launchGameplayFromSlot(m_pendingGameplayData);
                return; // Gameplay2D tomó control; no seguir este frame
            }

            // ── NACER TAPADO: el fundido de entrada no tiene trabajo ─────────
            // (intro-fade-doble, 2026-08-01)
            //
            // El fade negro de entrada existe para tapar la carga PROPIA de
            // este state. Si algo ya estaba tapando la pantalla mientras
            // cargabamos, no hay nada que tapar — y encima ESTORBA: sobre un
            // .ecl que abre claro, el rect negro lo arrastra hacia oscuro
            // mientras el velo blanco de la intro se retira, y la suma se
            // hunde a 193/255. Medido, y visible a ojo ("tambalea").
            //
            // POR QUE ACA Y NO EN onEnter: `StateStack::push` corre onEnter en
            // el acto, y el boot apila ESTE state ANTES que la intro (main.cpp:
            // el .ecl del menu primero, el splash encima). Al nacer, entonces,
            // NADIE nos tapa nunca — la pregunta ahi siempre da false. El
            // primer update, en cambio, bajo la intro cae recien en el handoff
            // (hasta ahi `allowsUpdateBelow` nos tiene congelados), y para ese
            // momento el splash ya esta encima. El orden lo sella
            // harness_intro.cpp seccion 6.3.
            //
            // Sin intro no cambia NADA: solos en la pila somos la cima, la
            // consulta da false y el fundido corre como siempre (assert 6.2).
            //
            // ── QUE ESTA GUARDADO Y QUE NO (decilo, no lo supongas) ──────────
            // harness_intro §6 cubre el PREDICADO (`hayAlgoEncima`): sus cinco
            // asserts viven en una TU standalone porque `StateStack.h` es
            // SDL-free. ESTE CABLEADO NO ESTA CUBIERTO POR NINGUN HARNESS —
            // `VNScriptState.h` arrastra SDL/Lua/audio y no entra en una TU
            // suelta (misma pared que `MERGE-DUPLICADO`). Se verifico con una
            // sonda temporal, medida en las DOS ramas y retirada:
            //     intro ON  -> hayAlgoEncima=1, CANCELADO, fadeAlpha aun en 1
            //     intro OFF -> hayAlgoEncima=0, sin cancelar
            // O sea: si alguien mueve este bloque, invierte la condicion o toca
            // `m_primerUpdate`, la bateria sigue VERDE. Lo que se rompe se ve
            // en pantalla, no en el tally — y sobre un `.ecl` que abre OSCURO
            // no se ve ni en pantalla (§7.404). Re-medir con sonda si se toca.
            if (m_primerUpdate)
            {
                m_primerUpdate = false;
                if (m_fadingIn && m_stateStack && m_stateStack->hayAlgoEncima(this))
                {
                    m_fadeAlpha = 0.0f;
                    m_fadingIn = false;
                }
            }

            // Fade in al entrar
            //
            // `dt / duracion` GENERALIZA el `dt * 2.0f` que estuvo acá hasta
            // v0.8.49 sin cambiarlo: con la duración de fábrica (0.5 s) el
            // factor es exactamente 2.0, así que un proyecto sin bloque
            // `transiciones {}` corre la misma rampa que siempre. El `> 0.0f`
            // es cinturón: `onEnter` ya apaga `m_fadingIn` cuando la duración
            // es cero, y la división no llega acá.
            if (m_fadingIn)
            {
                if (m_transicionSpec.duracion > 0.0f)
                    m_fadeAlpha -= (float)dt / m_transicionSpec.duracion;
                else
                    m_fadeAlpha = 0.0f;
                if (m_fadeAlpha <= 0.0f) { m_fadeAlpha = 0.0f; m_fadingIn = false; }
            }

            // Transición de fondo activa
            if (m_activeTransition != VNTransitionType::None)
            {
                m_transProgress += (float)dt / TRANS_SPEED;
                if (m_transProgress >= 1.0f)
                {
                    m_transProgress = 1.0f;
                    m_activeTransition = VNTransitionType::None;
                    m_assets.unpin(m_transOldBgPath); // Liberar fondo viejo del pin
                    m_transOldBgPath.clear();
                }
            }

            // Hover de hotspots (CP5 átomo): actualiza el estado hovered cada
            // frame durante WaitingClick con mouseToVirtual. updateHover es
            // enabled-aware: un hotspot deshabilitado por el gate [si:] nunca
            // queda hovered.
            if (m_phase == VNPhase::WaitingClick)
            {
                // `CP7`: el segundo polleo. El CLICK de hotspot no cambia:
                // siempre leyó `e->button.x/y`, o sea que ya era fresco.
                //
                // **El gate es `presente` y NO el centinela, y la diferencia
                // importa.** La ley 3 dice que `(-1,-1)` no lo contiene ningún
                // rect, y eso es cierto **por convención, no por construcción**:
                // un `hotspot x=-40 ancho=100` es escribible por el creador y lo
                // contendría. La caja se salva porque su guarda es explícita
                // (`vx > 0 && vy > 0`); acá se hace explícita también, igual que
                // en `control` y en `ranura`. El centinela queda como respaldo,
                // no como única defensa.
                if (m_puntero.presente)
                    m_hotspots.updateHover(m_puntero.vx, m_puntero.vy);
                else
                    m_hotspots.updateHover(-1.0f, -1.0f);

                // Random events (revival): el tick corre SOLO aquí — ley de
                // DOCS §25 ("solo durante esperar_click"). Ancla activa = ya
                // hay un evento en curso: un-evento-a-la-vez, o el segundo
                // disparo pisaría el ancla del primero. Los modales in-state
                // (historial, modalidad-aviso) poseen la pantalla igual que
                // una fase: un evento no interrumpe al que lee.
                if (m_eventsActive && m_preEventScene.empty()
                    && !m_dialogBox.isShowingHistory() && !m_modalRequired)
                    tickRandomEvents(dt);
            }

            // ── `CP7`: EL HOVER DEL ENLACE SE DERIVA ARRIBA, ACÁ SÓLO SUENA ──
            //
            // *(Acá vivían el gate de `linksFrozen`, el tercer polleo de
            // `SDL_GetMouseState` y el pull condicional de `CP6`. Los tres se
            // mudaron a `refrescarHoverDelPuntero`, que ya corrió este frame.
            // La ley que custodiaban no cambió una coma — `harness_foco` §6.1
            // sigue siendo su guardián.)*
            //
            // **El gate de congelado ya no hace falta ACÁ, y eso no es un
            // recorte:** cuando los enlaces están congelados, la derivación
            // llamó a `clearInteractiveHover()`, así que `hoveredInteractive()`
            // contesta `nullptr`, el nombre sale vacío y el sonido no dispara —
            // exactamente lo que el `if (linksFrozen)` conseguía escribiéndolo
            // dos veces.
            //
            // El sonido NO se gatea por `m_focoDelPuntero`, y eso sí es de
            // `CP6`: lee el MISMO campo, así que suena por flanco venga del
            // mouse o de una flecha — que es lo que el §9 quiere decir con "un
            // solo índice".
            {
                SceneElement* hl = m_sceneLayers.hoveredInteractive();
                const std::string hovName = hl ? hl->name : std::string();
                if (!hovName.empty() && hovName != m_enlaceHoverPrev && m_hasAudio)
                {
                    const std::string& hs = m_dialogBox.activeChoiceHoverSound();
                    if (!hs.empty()) m_audio.playSound(hs);
                }
                m_enlaceHoverPrev = hovName;
            }

            // ── `CP6`: LA CONVERGENCIA (ley 2), del lado del puntero ─────────
            //
            // *(Esta llamada se mudó a `refrescarHoverDelPuntero` el 2026-08-10.
            // La LEY no cambió: mientras el puntero manda, el índice unificado se
            // re-deriva de los hit-tests vivos, y en cuanto el teclado mueve el
            // anillo `m_focoDelPuntero` baja y deja de pisarlo. `harness_foco`
            // §8-§9 sigue siendo su guardián, sin tocar un assert.)*
            //
            // **Y su razón de estar acá VENCIÓ, que es lo que vale anotar**
            // (§7.328). Decía: *«va acá y no en `handleEvent` porque el hover del
            // enlace se calcula arriba, en este mismo `update`: leerlo antes
            // daría el del frame anterior»*. Cierto entonces, y era la
            // descripción del defecto en vez de su arreglo — «el del frame
            // anterior» es exactamente lo que `CP7` fue a sacar. Hoy el hover
            // del enlace se calcula **con el evento**, así que leerlo en
            // `handleEvent` da el de ESTE batch, y por eso el tap acierta a la
            // primera. La condición que la frase protegía sigue viva: la
            // derivación del enlace corre ANTES que `focoBajoPuntero`, sólo que
            // ahora las dos viven en la misma función.

            // Actualizar animaciones de capas
            m_sceneLayers.update(dt);

            // ── Fade in/out de personajes ────────────────────────────────
            {
                float step = (float)dt * CHAR_FADE_SPEED;
                for (auto& ac : m_activeChars)
                {
                    if (ac.fadeAlpha < ac.fadeTarget)
                    {
                        ac.fadeAlpha += step;
                        if (ac.fadeAlpha > ac.fadeTarget) ac.fadeAlpha = ac.fadeTarget;
                    }
                    else if (ac.fadeAlpha > ac.fadeTarget)
                    {
                        ac.fadeAlpha -= step;
                        if (ac.fadeAlpha < ac.fadeTarget) ac.fadeAlpha = ac.fadeTarget;
                    }
                }

                // ── Slide in/out de personajes ──
                float slideStep = (float)dt * CHAR_SLIDE_SPEED;
                for (auto& ac : m_activeChars)
                {
                    if (ac.slideX != ac.slideTargetX)
                    {
                        if (ac.slideX < ac.slideTargetX)
                        {
                            ac.slideX += slideStep;
                            if (ac.slideX > ac.slideTargetX)
                                ac.slideX = ac.slideTargetX;
                        }
                        else
                        {
                            ac.slideX -= slideStep;
                            if (ac.slideX < ac.slideTargetX)
                                ac.slideX = ac.slideTargetX;
                        }

                        // Slide out completado → marcar para eliminación
                        if (ac.slidingOut && ac.slideX == ac.slideTargetX)
                        {
                            ac.fadeAlpha = 0.0f;
                            ac.fadeTarget = 0.0f;
                        }
                    }
                }

                // Liberar cached tex de personajes que van a desaparecer
                for (auto& ac : m_activeChars)
                    if (ac.fadeAlpha <= 0.0f && ac.fadeTarget <= 0.0f)
                        releaseCachedTex(ac);

                // Eliminar personajes que terminaron de desaparecer (fade out completo)
                // G3: los stacks mueren CON el personaje. Los alias se capturan
                // AQUÍ DENTRO, no después del remove_if: los elementos del rango
                // sobrante quedan moved-from (contenido no especificado) y
                // `ac.alias` ya sería "". Es el mismo motivo por el que el unpin
                // de texturas siempre estuvo acá adentro.
                std::vector<std::string> muertos;
                auto it = std::remove_if(m_activeChars.begin(), m_activeChars.end(),
                    [this, &muertos](const VNActiveChar& ac)
                    {
                        if (ac.fadeAlpha <= 0.0f && ac.fadeTarget <= 0.0f)
                        {
                            muertos.push_back(ac.alias);
                            // Unpinear texturas al eliminar
                            if (ac.threeLayer)
                            {
                                if (!ac.leftPath.empty())  m_assets.unpin(ac.leftPath);
                                if (!ac.rightPath.empty()) m_assets.unpin(ac.rightPath);
                            }
                            else if (!ac.spritePath.empty())
                            {
                                m_assets.unpin(ac.spritePath);
                            }
                            if (ac.composite && !ac.facePath.empty())
                                m_assets.unpin(ac.facePath);
                            return true;
                        }
                        return false;
                    });
                if (it != m_activeChars.end())
                    m_activeChars.erase(it, m_activeChars.end());

                // Aquí, y no en `ocultar`: mientras hace fade-out el personaje
                // sigue vivo y visible, y podarle las animaciones lo congelaría
                // a mitad del desvanecimiento. Sin esto, un efecto perenne sobre
                // un personaje ya invisible tickea para siempre. El huérfano de
                // m_charAnims era preexistente — lo repliqué en vez de verlo.
                for (const auto& alias : muertos)
                {
                    m_charAnims.erase(alias);
                    m_charFx.erase(alias);
                    m_charTracks.erase(alias);   // C-CP5: misma ley, 3ª especie
                }
            }

            // ── Actualizar animaciones de personajes (C-CP2: stack) ───
            // La entrada muerta se poda de su stack; el alias sin stack sale
            // del mapa. Misma ley que antes, un nivel más adentro.
            // ── E-CP0: los efectos corren su reloj igual que las anims. Misma
            // ley de poda: la instancia muerta sale de su stack; el blanco sin
            // stack sale del mapa.
            for (auto fit = m_charFx.begin(); fit != m_charFx.end(); )
            {
                auto& fxStack = fit->second;
                for (auto it = fxStack.begin(); it != fxStack.end(); )
                {
                    // E-CP6: instancia Lua cuyo registro murió (el fn falló y
                    // se des-registró, o el script encadenado ya no lo trae)
                    // — mismo trato que la ley de miss del load: voz + poda.
                    if (it->kind == FxKind::Lua && !m_luaFx.count(it->luaName))
                    {
                        std::cerr << "[VN] AVISO: efecto lua '" << it->luaName
                            << "' ya no esta registrado. Se quita de '"
                            << fit->first << "'." << std::endl;
                        it = fxStack.erase(it);
                    }
                    else if (!updateEffect(*it, (float)dt))
                        it = fxStack.erase(it);
                    else
                        ++it;
                }
                if (fxStack.empty()) fit = m_charFx.erase(fit);
                else                 ++fit;
            }

            for (auto ait = m_charAnims.begin(); ait != m_charAnims.end(); )
            {
                auto& stack = ait->second;
                for (auto it = stack.begin(); it != stack.end(); )
                {
                    if (!updateAnim(*it, (float)dt)) it = stack.erase(it);
                    else                             ++it;
                }
                if (stack.empty()) ait = m_charAnims.erase(ait);
                else               ++ait;
            }
            for (auto it = m_screenAnims.begin(); it != m_screenAnims.end(); )
            {
                if (!updateAnim(*it, (float)dt)) it = m_screenAnims.erase(it);
                else                             ++it;
            }

            // ── C-CP5: los tracks corren su reloj. SIN poda: loop/vaiven son
            // perennes, y un una_vez terminado SOSTIENE su valor final (el
            // logo que cayó se queda donde cayó) — solo detener/ocultar/load
            // los bajan. El estado es elapsed y nada más (§7.207).
            for (auto& [aliasT, ts] : m_charTracks)
                for (auto& ti : ts)
                    tickTrackSeconds(ti.track, (float)dt);
            for (auto& ti : m_screenTracks)
                tickTrackSeconds(ti.track, (float)dt);

            // ── esperar_animacion: avanzar cuando todas terminen ──────
            if (m_waitingForAnim)
            {
                bool hasDialoguePending = false;
                // §7.38 (hallazgo de paso, C-CP5): un PERSISTENTE que ya llegó
                // (acercar/caer/subir) devolvía isActive() && !isLoop() PARA
                // SIEMPRE → `animar m acercar` + `esperar_animacion` era un
                // guion COLGADO desde que los persistentes existen. Misma
                // especie que la ceguera a m_charFx que E-CP1 tapó en este
                // mismo bloque: el que ya llegó no se espera.
                auto animEnVuelo = [](const CharAnim& a)
                {
                    return a.isActive() && !a.isLoop() &&
                        !(a.isPersistent() && a.elapsed >= a.duration);
                };
                bool anyActive = false;
                for (const auto& a : m_screenAnims)
                    if (animEnVuelo(a)) { anyActive = true; break; }
                if (!anyActive)
                {
                    for (const auto& pair : m_charAnims)
                    {
                        for (const auto& a : pair.second)
                            if (animEnVuelo(a)) { anyActive = true; break; }
                        if (anyActive) break;
                    }
                }

                // ── C-CP5: los tracks una_vez EN VUELO se esperan; loop y
                // vaiven jamás (perennes, como isLoop arriba), y el una_vez
                // terminado sostiene un valor fijo — ya llegó, no se espera.
                if (!anyActive)
                {
                    for (const auto& ti : m_screenTracks)
                    {
                        hasDialoguePending = hasDialoguePending ||
                            dialogueTrackPending(ti.track);
                        if (trackBlocksAnimationWait(ti.track)) anyActive = true;
                    }
                    for (const auto& pair : m_charTracks)
                        for (const auto& ti : pair.second)
                        {
                            hasDialoguePending = hasDialoguePending ||
                                dialogueTrackPending(ti.track);
                            if (trackBlocksAnimationWait(ti.track)) anyActive = true;
                        }
                }
                // TRACK-CAPA-XY-01: SceneElement es el dueno del stack, pero
                // `esperar_animacion` sigue siendo la frontera comun. Las
                // capas solo admiten Seconds, asi que no agregan pendiente de
                // DialogueAdvance ni pueden crear el deadlock advertido arriba.
                if (!anyActive && m_sceneLayers.tracksBlockAnimationWait())
                    anyActive = true;
                // ── E-CP1: los EFECTOS también cuentan. Esta espera era CIEGA a
                // m_charFx: `animar m franja 0.5s` + `esperar_animacion` avanzaba
                // de una. Y al mudar el glitch acá, `animar m glitch 0.5s` habría
                // DEJADO de esperar — una regresión introducida por el propio CP.
                // Un efecto PERENNE no se espera, igual que un loop no se espera.
                if (!anyActive)
                {
                    for (const auto& pair : m_charFx)
                    {
                        for (const auto& fx : pair.second)
                            if (fx.isActive() && !fx.isPerenne()) { anyActive = true; break; }
                        if (anyActive) break;
                    }
                }
                if (!anyActive)
                {
                    if (hasDialoguePending)
                        std::cerr << "[VN] AVISO: esperar_animacion no espera "
                            "tracks gobernados por avances de dialogo; hacerlo "
                            "impediria el evento que los completa. El guion "
                            "continua." << std::endl;
                    m_waitingForAnim = false;
                    m_cmdIndex++;
                }
            }

            // ── esperar N: decrementar timer y avanzar al llegar a 0 ──
            if (m_waitTimer > 0.0f)
            {
                m_waitTimer -= (float)dt;
                if (m_waitTimer <= 0.0f)
                {
                    m_waitTimer = 0.0f;
                    m_cmdIndex++;
                }
            }

            // Toast de notificación (desaparece solo)
            if (m_toastTimer > 0.0f)
                m_toastTimer -= (float)dt;

            // Smooth scroll del historial
            if (m_showingHistory)
            {
                float diff = m_historyScrollTarget - m_historyScroll;
                m_historyScroll += diff * std::min((float)dt * 12.0f, 1.0f);
            }

            // Ejecutar comandos instantáneos en cadena
            // (procesamos varios por frame para no ver parpadeos)
            if (m_phase == VNPhase::Executing &&
                vnInstantBatchCanContinue(m_waitingForGameplay,
                    m_waitingForAnim, m_waitTimer))
            {
                for (int guard = 0; guard < 64 && m_phase == VNPhase::Executing
                    && vnInstantBatchCanContinue(m_waitingForGameplay,
                        m_waitingForAnim, m_waitTimer); ++guard)
                {
                    // CP0 reloj-dialogo: esperar_animacion debe cortar el batch
                    // en el mismo dispatch que activa el flag. Si alguien quita
                    // el gate del for, Debug cae antes de repetir el comando.
                    assert(!m_waitingForAnim);
                    executeNextCmd();
                }
            }
        }

        // ── Render ────────────────────────────────────────────────────────────

        void render(f64 /*alpha*/) override
        {
            // `caja-entrada`: abre el frame DE RENDER de la caja. Va acá y no
            // en `update()` porque el skip-frame puede saltarse frames enteros
            // mientras la caja espera input: la caja sigue en la pantalla, pero
            // nadie la redibuja. Contado desde el render, "¿estaba en el último
            // frame dibujado?" = "¿el jugador la está viendo?". Medido con
            // `sonda_caja_entrada.ecl` beat 2; el detalle vive en
            // `VNDialogBox::frameBegin`.
            m_dialogBox.frameBegin();

            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();

            // ── LOTE B: el clear es CONDICIONAL ──────────────────────────
            //
            // `setupViewport` no sólo configura el viewport: BORRA la pantalla
            // entera a negro opaco. Como el walk de `StateStack::render` va de
            // abajo hacia arriba, el de arriba borra lo que el de abajo acaba
            // de dibujar — o sea que declararse transparente NO ALCANZA
            // (`§7.396`, y es la mitad que `§11.7c` no tenía).
            //
            // Sale del MISMO predicado que `isTransparent()`, no de una copia:
            // separarlos es exactamente cómo esa ficha sobrevivió años.
            draw2d::setupViewport(m_winW, m_winH,
                viewport::width(), viewport::height(),
                /*limpiar=*/ !pintaEncima());
            draw2d::setupProjection(VW, VH);

            // ── LOTE B: el fundido del STATE ENTERO ──────────────────────
            //
            // `GroupAlpha` es la soldadura de `caja-entrada` a mayor escala: lo
            // que se dibuje adentro del ámbito comparte el fundido. Se abre
            // SIEMPRE y en reposo vale 1.0, que por la ley 1 del header
            // devuelve el argumento con el mismo bit — o sea que un proyecto
            // sin `encima` dibuja bit-idéntico a antes de este lote, sin un
            // `if` alrededor que alguien pueda mover.
            //
            // ⚠ NO CUBRE TODO, y está medido (`§11.9d`): `renderFxQuad` y
            // `Vignette::draw` escriben `glColor4f` crudo, así que un personaje
            // con `efecto` y la viñeta NO funden con el resto. Declarado como
            // residual del lote, no arreglado de paso.
            const draw2d::GroupAlphaScope grupoDelState(
                pintaEncima() ? (1.0f - m_fadeAlpha) : 1.0f);

            // ── Screen shake: offset global ──────────────────────────────
             // Sincroniza fixed-function y SpriteBatch via setupProjectionOffset.
             // Sin esto el fondo (migrado a SpriteBatch en paso 2) no shakearia
             // mientras personajes/texto (todavia immediate mode) si.
            // C-CP2: la pantalla compone su stack igual que un personaje.
            AnimOutput screenAo = composeStack(m_screenAnims);
            // C-CP5: y sus tracks nombrados, por el mismo álgebra.
            if (!m_screenTracks.empty())
                screenAo = compose(screenAo, composeTracks(m_screenTracks));
            float screenShakeX = screenAo.dx;
            float screenShakeY = screenAo.dy;
            if (screenShakeX != 0.0f || screenShakeY != 0.0f)
            {
                draw2d::setupProjectionOffset(
                    screenShakeX, VW + screenShakeX,
                    VH + screenShakeY, screenShakeY);
            }

            // F2.B.3: begin batch a nivel state — agrupa fondo + capas +
            // personajes + flash en un solo begin/end. Reduce ~10 begin/end → 1.
            draw2d::g_batch->begin();

            // ── Fondo (con transiciones) ─────────────────────────────────────
            if (m_activeTransition != VNTransitionType::None &&
                !m_transOldBgPath.empty() && !m_currentBgPath.empty())
            {
                Texture* oldBg = m_assets.get(m_transOldBgPath);
                Texture* newBg = m_assets.get(m_currentBgPath);
                if (oldBg && newBg)
                    renderBackgroundTransition(*oldBg, *newBg, VW, VH);
                else if (newBg)
                    draw2d::drawBackgroundTinted_noBatch(*newBg, VW, VH,
                        m_bgTintR, m_bgTintG, m_bgTintB);
            }
            else if (!m_currentBgPath.empty())
            {
                Texture* bg = m_assets.get(m_currentBgPath);
                if (bg)
                {
                    // ── E-CP2: efectos sobre `fondo` — mismo patrón immediate
                    // que los personajes (un flush). Durante una transición el
                    // quad es de la transición; los fx retoman al terminar.
                    auto fitBg = m_charFx.find("fondo");
                    if (fitBg != m_charFx.end() && !fitBg->second.empty())
                    {
                        draw2d::g_batch->end();
                        renderFxQuad(bg->getID(), 0.0f, 0.0f, VW, VH,
                            m_bgTintR, m_bgTintG, m_bgTintB, 1.0f,
                            fitBg->second);
                        draw2d::g_batch->begin();
                    }
                    else
                        // Tinte en identidad (1,1,1) = la llamada de siempre.
                        draw2d::drawBackgroundTinted_noBatch(*bg, VW, VH,
                            m_bgTintR, m_bgTintG, m_bgTintB);
                }
                else
                    draw2d::drawRect_noBatch(0, 0, VW, VH, 0.1f, 0.08f, 0.18f);
            }
            else
            {
                draw2d::drawRect_noBatch(0, 0, VW, VH, 0.1f, 0.08f, 0.18f);
            }

            // ── Capas Z-Order (si hay) ─────────────────────────────────
            // Parallax: pausado durante diálogos/choices, activo en exploración
            {
                float mnx = 0.5f, mny = 0.5f;
                // `CP7`: el quinto polleo, y el único que vivía en `render()`.
                // **El `m_puntero.presente` es el fork 7(a)**: sin puntero el
                // parallax vuelve al CENTRO, que es el valor que estas dos
                // líneas de arriba ya usan para decir «sin parallax». El
                // centinela no se inventó: estaba escrito acá desde siempre.
                // La alternativa —congelarlo donde tocó el dedo— dejaba el
                // fondo torcido para siempre después de un tap, que es el
                // fantasma más visible de los siete.
                if (m_parallaxEnabled && m_puntero.presente &&
                    m_phase != VNPhase::WaitingInput &&
                    m_phase != VNPhase::WaitingChoice &&
                    m_phase != VNPhase::WaitingQTE)
                {
                    mnx = (float)m_punteroWinX / (float)std::max(m_winW, 1);
                    mny = (float)m_punteroWinY / (float)std::max(m_winH, 1);
                }
                m_sceneLayers.render(mnx, mny,
                    m_hasFont ? &m_textRenderer : nullptr,
                    &m_fontManager,
                    // ── E-CP2: hook de efectos por capa. Pinta la tubería EN el
                    // lugar z de la capa (mismo flush único que los personajes)
                    // y hereda tinte estático + alpha como pasada [base].
                    [this, VW, VH](const SceneElement& elem, const Texture& tex,
                                   float px, float py) -> bool
                    {
                        auto fit = m_charFx.find(elem.name);
                        if (fit == m_charFx.end() || fit->second.empty())
                            return false;
                        draw2d::g_batch->end();
                        renderFxQuad(tex.getID(), px, py, elem.w, elem.h,
                            elem.tintR, elem.tintG, elem.tintB, elem.alpha,
                            fit->second);
                        draw2d::g_batch->begin();
                        return true;
                    });
            }

            // ── Personajes activos (sistema VN clásico + compuestos) ──────
             // Speaker highlight: personaje que habla = brillo normal + zoom,
             //                    los demás = oscurecidos
            bool hasSpeaker = (m_speakerHighlight &&
                m_phase == VNPhase::WaitingInput &&
                !m_currentSpeaker.empty());

            for (auto& ac : m_activeChars)
            {
                if (ac.fadeAlpha <= 0.0f) continue;

                float tint = 1.0f;
                float scale = 1.0f;

                if (hasSpeaker)
                {
                    if (ac.alias == m_currentSpeaker)
                    {
                        tint = 1.0f;
                        scale = 1.05f;
                    }
                    else
                    {
                        tint = 0.55f;
                        scale = 1.0f;
                    }
                }

                // C-CP2: el stack del alias colapsa a UN AnimOutput (§7.208).
                // Con una sola animación, bit-idéntico a lo de antes.
                auto ait = m_charAnims.find(ac.alias);
                const std::vector<CharAnim>* stack =
                    (ait != m_charAnims.end()) ? &ait->second : nullptr;
                AnimOutput ao = stack ? composeStack(*stack) : AnimOutput{};

                // C-CP5: los tracks nombrados entran al MISMO álgebra — un
                // compose más, no un camino nuevo (§7.208 es asociativa).
                {
                    auto tit = m_charTracks.find(ac.alias);
                    if (tit != m_charTracks.end())
                        ao = compose(ao, composeTracks(tit->second));
                }

                float animOX = ao.dx;
                float animOY = ao.dy;
                float animSc = ao.scale;
                float animAl = ao.alpha;
                float animTn = ao.tintR;   // los presets tiñen uniforme (RGB = CP6)

                float finalTint = tint * animTn;
                // C-ESCALA: escala_base del manifest. Leída en draw-time (fuente
                // unica, no duplicada en VNActiveChar) -> inmune a save/load y
                // hot-reload. Mismo patron que resolveSpeakerName. Uniforme en W y
                // H -> agrandar nunca deforma (§7.30). Sin manifest o char no
                // definido -> 1.0f (identico a hoy). Componible con zoom: el canal
                // animSc del tween sigue multiplicando aparte.
                float baseScale = 1.0f;
                float anclaY = 0.0f;
                if (const PersonajeManifest* pm = resolveSpeakerManifest(ac.alias))
                {
                    baseScale = pm->escalaBase;
                    anclaY = pm->anclaY;
                }
                float finalScale = baseScale * scale * animSc;
                float finalAlpha = ac.fadeAlpha * animAl;
                float finalOffX = ac.slideX + animOX;
                float finalOffY = animOY + anclaY;   // C-ANCLA: anclaje + animacion (espejo de finalScale)

                // ── E-CP1: UN solo camino. `glitch` ya no es una anim con render
                // bespoke que le gana el draw a los efectos: es un EffectInstance
                // más, y entra por la misma tubería que `franja` y `tajadas`.
                // Las dos ramas de convivencia se fueron con él.
                auto fit = m_charFx.find(ac.alias);
                const std::vector<EffectInstance>* fxs =
                    (fit != m_charFx.end()) ? &fit->second : nullptr;
                const bool hasFx = (fxs && !fxs->empty());

                if (hasFx && !ac.composite)
                {
                    // Immediate mode entre un end() y un begin(): un solo flush.
                    draw2d::g_batch->end();
                    Texture* tex = m_assets.get(ac.spritePath);
                    if (tex)
                        renderFxChar(tex->getID(),
                            (float)tex->getWidth(), (float)tex->getHeight(),
                            ac.position, VW, VH,
                            finalAlpha, finalOffX, finalTint, finalScale,
                            finalOffY, *fxs);
                    draw2d::g_batch->begin();
                }
                else if (hasFx && ac.composite && m_hasFBO)
                {
                    // ensureCachedComposite dibuja las capas al FBO por el
                    // SpriteBatch (draw+flush), y flush() hace glDrawElements SIN
                    // religar el VAO — depende de que begin() lo tenga ligado.
                    // g_batch->end() lo desliga (para el glBegin fijo de
                    // renderFxChar). Por eso el compose va ANTES del end(): si va
                    // despues, el flush del compose corre con el VAO en 0 y el FBO
                    // sale VACIO — la base del personaje desaparece (era el "flash
                    // negro post-load" Y el "cuadrado sin personaje" de composites
                    // mostrados CON efecto de arranque, cualquier N de capas).
                    const bool okComp = ensureCachedComposite(ac);
                    draw2d::g_batch->end();
                    if (okComp)
                    {
                        renderFxChar(ac.cachedTex,
                            (float)ac.cachedTexW, (float)ac.cachedTexH,
                            ac.position, VW, VH,
                            finalAlpha, finalOffX, finalTint, finalScale,
                            finalOffY, *fxs);
                    }
                    draw2d::g_batch->begin();
                }
                else if (ac.composite)
                {
                    drawCompositeChar(ac, VW, VH, finalTint, finalScale,
                        finalOffX, finalOffY, finalAlpha);
                }
                else
                {
                    Texture* tex = m_assets.get(ac.spritePath);
                    if (tex)
                        drawSingleSprite(*tex, ac.position, VW, VH,
                            finalAlpha, finalOffX, finalTint, finalScale,
                            finalOffY);
                }

                // ── Flash blanco sobre el personaje ──────────────────
                if (ao.flashAlpha > 0.0f)
                {
                    Texture* flashTex = nullptr;
                    float ftw = 0, fth = 0;
                    if (ac.composite)
                    {
                        // Usar dimensiones del body
                        flashTex = m_assets.get(ac.spritePath);
                    }
                    else
                    {
                        flashTex = m_assets.get(ac.spritePath);
                    }
                    if (flashTex)
                    {
                        float maxH = VH * 0.85f;
                        float sc = 1.0f;
                        ftw = (float)flashTex->getWidth();
                        fth = (float)flashTex->getHeight();
                        if (fth > maxH) { sc = maxH / fth; ftw *= sc; fth *= sc; }
                        ftw *= finalScale;
                        fth *= finalScale;
                        float cx = EclipParser::positionToX(ac.position, VW);
                        float fx = std::floor(cx - ftw * 0.5f + finalOffX);
                        float fy = std::floor(VH - fth + finalOffY);

                        // Camino A.C: era glBegin(GL_QUADS) directo con
                        // glDisable(GL_TEXTURE_2D)+glColor4f. Ahora rect
                        // blanco tinted por alpha via SpriteBatch.
                        draw2d::drawRect_noBatch(fx, fy, ftw, fth,
                            1.0f, 1.0f, 1.0f, ao.flashAlpha);
                    }
                }

            }

            // F2.B.3: end batch — vignette es immediate (GL_TRIANGLE_STRIP).
             // Cierra el grupo: fondo + capas + personajes + flash.
            draw2d::g_batch->end();

            // ── Viñeta (encima de escena, debajo de UI) ──────────────────
            m_vignette.draw(VW, VH);

            // ── Caja de diálogo / choices / QTE / hotspots ──────────────
            //if (m_phase == VNPhase::WaitingInput)
                //renderDialogBox(VW, VH);
            //else if (m_phase == VNPhase::WaitingChoice)
                //renderChoiceBox(VW, VH);
            //else if (m_phase == VNPhase::WaitingQTE)
                //renderQTE(VW, VH);

            // ── Caja de diálogo / choices / QTE / hotspots ──────────────
            if (m_phase == VNPhase::WaitingInput)
                m_dialogBox.renderDialogBox(VW, VH);
            else if (m_phase == VNPhase::WaitingChoice)
            {
                // ── `ELEGIR-CON-CAJA` (`F-04`, 2ª mitad) ────────────────────
                //
                // Acá esta cadena era un `else if` puro, y por eso durante una
                // elección la caja no se ocultaba: NADIE LA DIBUJABA. Con
                // `mostrar_caja` escrito entre la línea y el `elegir`, la caja
                // se queda con su última línea y las opciones salen arriba.
                //
                // ORDEN: la caja PRIMERO, las opciones encima. Con el tema del
                // fundador no se solapan (caja en y≈852..1071, opciones en
                // ≈484..596), pero el z-order se declara igual: la capa
                // interactiva va última.
                //
                // El fundido de la caja NO se les contagia: `renderDialogBox`
                // abre su `GroupAlphaScope` RAII y lo cierra al salir, que es
                // justo la propiedad de frontera que `harness_grupo` custodia.
                //
                // Sin el pedido explícito esto es un no-op exacto: se llama
                // `renderChoices` y nada más, igual que siempre.
                if (m_dialogBox.boxSurvivesChoice())
                    m_dialogBox.renderDialogBox(VW, VH);
                m_dialogBox.renderChoices(VW, VH);
            }
            else if (m_phase == VNPhase::WaitingQTE)
                m_dialogBox.renderQTE(VW, VH);
            else if (m_phase == VNPhase::WaitingTextInput)
                // B2: solo el campo (box de diálogo oculto → look centrado
                // sobre la escena, Layout B). VN pasa el dato; box renderiza.
                m_dialogBox.renderTextInput(VW, VH, m_inputPrompt, m_inputBuffer,
                    m_inputRequired, m_inputOkHover);

            // ── Screen flash blanco (Taiyoken) ──────────────────────────
            AnimOutput screenFlashAo = composeStack(m_screenAnims);
            // C-CP5: los tracks de pantalla no emiten flash, pero componer
            // el stack entero mantiene UNA sola verdad de composición.
            if (!m_screenTracks.empty())
                screenFlashAo = compose(screenFlashAo,
                    composeTracks(m_screenTracks));
            if (screenFlashAo.flashAlpha > 0.0f)
            {
                // Camino A.C: era glBegin(GL_QUADS) directo fullscreen con
                // glDisable(GL_TEXTURE_2D)+glColor4f. Ahora rect blanco
                // tinted por alpha via SpriteBatch.
                draw2d::drawRect(0, 0, VW, VH,
                    1.0f, 1.0f, 1.0f, screenFlashAo.flashAlpha);
            }

            // Debug render de hotspots (solo si hay hotspots activos)
            if (m_phase == VNPhase::WaitingClick && m_debugHotspots)
                m_hotspots.debugRender(m_hasNameFont ? &m_nameRenderer : nullptr);

            // Hover outline de hotspots (CP5 átomo): feedback para el jugador,
            // themeable, SIN guard de debug (no requiere F3). Default off
            // (alpha 0) → no dibuja nada hasta que el tema ponga
            // hotspot_hover_color.
            if (m_phase == VNPhase::WaitingClick)
                m_hotspots.renderHoverOutline(
                    m_theme.hotspotHoverR, m_theme.hotspotHoverG,
                    m_theme.hotspotHoverB, m_theme.hotspotHoverA,
                    m_theme.hotspotHoverThickness);

            // ── Historial de diálogos (overlay completo) ───────────────────
            //if (m_showingHistory)
                //renderHistory(VW, VH);

            // ── Historial de diálogos (overlay completo) ───────────────────
            if (m_dialogBox.isShowingHistory())
                m_dialogBox.renderHistoryOverlay(VW, VH);

            // F2.C: begin batch — agrupa fade + toast + auto/skip indicator.
            draw2d::g_batch->begin();

            // ── Overlay de fade ────────────────────────────────────────────
            //
            // LOTE B: el velo es del TIPO `fade`, no del renglón. Con `encima`
            // no se dibuja nada acá — el fundido ya lo puso el ámbito de grupo
            // sobre todo lo de arriba, y meter además un rect negro sería
            // pintar el velo que el fundador pidió sacar (*«sin oscuridad»*).
            //
            // El tipo sale de `tipoDeRampaViva()` y no de `m_transicionSpec`
            // pelado, porque la MISMA línea dibuja la rampa de entrada y la de
            // salida: un proyecto puede pedir `abrir encima` con `cerrar fade`.
            if (tipoDeRampaViva() == TransicionTipo::Fade && m_fadeAlpha > 0.001f)
                draw2d::drawRect_noBatch(0, 0, VW, VH, 0, 0, 0, m_fadeAlpha);

            // ── Toast "Partida cargada" ────────────────────────────────────
            if (m_toastTimer > 0.0f && m_hasNameFont)
            {
                // Fade out suave en el último segundo
                float alpha = std::min(m_toastTimer, 1.0f);
                float tw = m_nameRenderer.measureWidth(m_toastMsg);
                float th = m_nameRenderer.getLineHeight();
                float tx = VW - tw - 22.0f;   // esquina inferior derecha
                float ty = VH - th - 18.0f;

                // Fondo oscuro del toast
                draw2d::drawRect_noBatch(tx - 12.0f, ty - 6.0f,
                    tw + 24.0f, th + 12.0f,
                    0.06f, 0.07f, 0.12f, 0.82f * alpha);
                draw2d::drawRectBorder_noBatch(tx - 12.0f, ty - 6.0f,
                    tw + 24.0f, th + 12.0f,
                    0.30f, 0.55f, 1.0f, 0.55f * alpha, 1.5f);

                m_nameRenderer.drawText_noBatch(m_toastMsg, tx, ty,
                    0.70f, 0.88f, 1.0f, alpha);
            }

            // ── Indicador de modo Auto / Skip (esquina superior derecha) ───
            if ((m_autoMode || m_skipMode) && m_hasNameFont)
            {
                // FORK 2: este indicador NO es el de los seis botones del
                // chrome. Aquéllos son `uiBtnLabels` (`Theme.h:383`), texto
                // autorable por `boton_labels` y por eso quedan fuera del censo
                // con el precedente de `ranura_vacia_etiqueta`. Éste es un
                // literal horneado que el tema NO alcanza, así que dejarlo
                // afuera "porque es del tema" sería dejarlo afuera por una
                // razón que a él no le aplica.
                std::string label = m_autoMode ? eclips::langUI("AUTO", "AUTO")
                                               : eclips::langUI("SKIP", "SKIP");
                float lw = m_nameRenderer.measureWidth(label);
                float lh = m_nameRenderer.getLineHeight();
                float lx = VW - lw - 20.0f;
                float ly = 14.0f;

                // Fondo sutil
                draw2d::drawRect_noBatch(lx - 8.0f, ly - 4.0f,
                    lw + 16.0f, lh + 8.0f,
                    0.08f, 0.10f, 0.18f, 0.70f);
                draw2d::drawRectBorder_noBatch(lx - 8.0f, ly - 4.0f,
                    lw + 16.0f, lh + 8.0f,
                    0.40f, 0.65f, 1.0f, 0.50f, 1.5f);

                // Color diferente: auto = verde suave, skip = amarillo
                float cr = m_autoMode ? 0.55f : 1.0f;
                float cg = m_autoMode ? 0.90f : 0.85f;
                float cb = m_autoMode ? 0.65f : 0.35f;

                m_nameRenderer.drawText_noBatch(label, lx, ly, cr, cg, cb, 0.9f);
            }

            // F2.C: end batch — cierra grupo fade + toast + auto/skip.
            draw2d::g_batch->end();

            // ── Reset screen shake ───────────────────────────────────────
            if (screenShakeX != 0.0f || screenShakeY != 0.0f)
                draw2d::setupProjection(VW, VH);

            // ── Controles de settings (CP2): barras read-only, top-most ──────
            renderControls();
            renderTeclaWidgets();   // `CP9b`
            // ── Ranuras / slot-widgets (Ola 2): tarjetas de slot, top-most ───
            renderSlotWidgets();
            // ── Confirm modal de borrado (Ola 2 · B2): overlay sobre todo ────
            renderConfirmModal();
            // R4 display: ultimo overlay; resolución/fullscreen nunca quedan
            // detrás de otro control o modal del libreto.
            renderDisplayConfirmModal();

        }

    private:
        bool hasHotspotNamed(const std::string& name) const
        {
            for (const auto& hotspot : m_hotspots.getAll())
                if (hotspot.name == name) return true;
            return false;
        }

        bool hasEnabledHotspotNamed(const std::string& name) const
        {
            for (const auto& hotspot : m_hotspots.getAll())
                if (hotspot.name == name && hotspot.enabled) return true;
            return false;
        }

        std::string liveHotspotNamesForVoice() const
        {
            std::vector<std::string> unique;
            for (const auto& hotspot : m_hotspots.getAll())
            {
                if (std::find(unique.begin(), unique.end(), hotspot.name)
                    == unique.end())
                    unique.push_back(hotspot.name);
            }

            if (unique.empty()) return "ninguno";

            constexpr std::size_t kMaxNames = 8;
            std::ostringstream names;
            const std::size_t shown = std::min(kMaxNames, unique.size());
            for (std::size_t i = 0; i < shown; ++i)
            {
                if (i > 0) names << ", ";
                names << unique[i];
            }
            if (unique.size() > kMaxNames)
                names << ", y " << (unique.size() - kMaxNames) << " mas";
            return names.str();
        }

        void warnMissingHotspotForWait(const EclipChoice& entry,
                                       int headerLine) const
        {
            std::cerr << "[VNScriptState] esperar_click: hotspot '"
                      << entry.label << "' no existe para destino '"
                      << entry.target << "' (linea header " << headerLine
                      << "); mapping ignorado. Repara con: hotspot "
                      << entry.label << " x= y= w= h=. Hotspots vivos: "
                      << liveHotspotNamesForVoice() << std::endl;
        }

        void warnMissingHotspotOnLoad(const EclipChoice& mapping) const
        {
            std::cerr << "[VNScriptState] cargar: mapping de hotspot '"
                      << mapping.label << "' -> destino '" << mapping.target
                      << "' descartado: no existe hotspot restaurado. "
                      << "Hotspots vivos: " << liveHotspotNamesForVoice()
                      << std::endl;
        }

        InputAction translateSDLEventToAction(const SDL_Event* e, VNPhase phase,
                                              const std::vector<std::string>& verbos)
        {
            if (e->type == SDL_KEYDOWN)
            {
                // (El `SDL_Keycode k` que se leía acá se fue con la cascada:
                //  desde `CP1` la única que mira keycodes es la Capa 1.)
                //
                // ── `QTE-SIN-PUERTA`: EL PRODUCTOR QUE FALTABA ───────────────
                //
                // Acá arrancaba la cascada de atajos, y por eso el QTE nunca
                // veía su tecla: `ESPACIO` se la llevaba `Confirm` dos líneas
                // abajo, `ESC` el `Cancel`, `A` el auto y las flechas el
                // historial. `InputAction::QTEKey` NO lo emitía nadie en 43k
                // líneas, así que la rama del éxito era inalcanzable (§7.470).
                //
                // Durante `WaitingQTE` una tecla es CANDIDATA del QTE. El
                // veredicto NO se escribe acá -esto sigue siendo Capa 1, pura
                // traducción-: la caja es la dueña de `m_qteKeycode` y compara
                // en `VNDialogBox.cpp`. **Una tecla que no acierta vuelve
                // `false` desde `handleAction` y sigue su camino a los atajos
                // globales de la sección 2**, así que la pausa sigue abriendo,
                // `K` sigue guardando, y todo lo que no es la tecla pedida se
                // comporta idéntico a antes de este lote. Y por el MISMO
                // mecanismo, sin una sola excepción escrita a mano,
                // `qte [tecla: ESC]` funciona: ahí el ESC acierta.
                //
                // El segundo término es la ley 5, y es §7.88: el historial se
                // abre con la RUEDA también durante un QTE, y ese overlay se
                // traga todo lo que llega. Sin la guarda, el ESC entraría como
                // `QTEKey`, el log se lo comería sin cerrarse, y el jugador
                // quedaría adentro sin salida.
                if (eclips::qteReclamaTecla(phase, m_dialogBox.isShowingHistory()))
                    return InputAction::QTEKey;
            }

            // ── `CP1` DEL ATOMO DE TECLADO: ACA VIVIA LA CASCADA ─────────────
            //
            // Once comparaciones de `SDLK_*` que mezclaban las tres capas del
            // contrato en un solo bloque: el binding (qué tecla), el sello (el
            // `!isModalRequired()` del auto) y el despacho (qué acción sale).
            //
            // Ahora son dos preguntas, una por capa:
            //
            //   Capa 1 · `resolverTodos(e,"vn")`  — la tabla del `CP0` traduce
            //            el evento a los VERBOS que activa. Pura, sin fases.
            //   Capa 3 · `vnAccionDeVerbos(...)`  — qué despacha el VN con esos
            //            verbos, esta fase, el log y el modal. Pura también, y
            //            por eso `harness_ruta` la puede correr entera.
            //
            // **`resolverTodos` y no `resolver`, y de eso depende el historial.**
            // `ARRIBA` activa DOS verbos (`foco_arriba` de `CP5` y
            // `historial_arriba`), y el manager los guarda alfabéticamente: el
            // primero es el que NO tiene consumidor. Pedir uno solo acá compila,
            // corre, y mata el scroll del log sin un warning (`E-5`; medido en
            // el 8.1 de `harness_ruta`).
            //
            // **El early-return del QTE sigue ARRIBA, y no se puede mover.**
            // Es `E-11`: el QTE no tiene un verbo del dominio `vn` —su tecla la
            // escribe el creador línea por línea, no el jugador en su config—
            // así que la tabla no lo puede expresar. Si baja de acá, `ESPACIO`,
            // `ESC`, `ARRIBA` y `A` vuelven a ser inescribibles como tecla de
            // QTE, que es justo lo que `CP0` había arreglado en el parse.
            //
            // Lo que este bloque NO se lleva: los atajos de la sección 2
            // (`K`/`L`/`F5`/`F9`/`F3`). Sus verbos resuelven, y la ruta contesta
            // `None` a propósito para que sigan cayendo ahí — alcance firmado
            // del lote (fork 2c). La §5 de `harness_ruta` es su guardián.
            // Sin guarda de nulo a propósito: el constructor lo exige sin valor
            // por defecto, así que `m_input` no puede ser nulo acá. Un
            // `if (m_input)` habría convertido un error de cableado en un VN
            // mudo —sin teclado y sin una línea de aviso—, que es el modo de
            // falla que este lote está tratando de no repetir.
            // `CP1b`: la lista ya viene resuelta desde `handleEvent` (fork 2b).
            // Antes se resolvía acá adentro; se subió porque la sección 2 la
            // necesita también y esta función no corre en todas las fases.
            return eclips::vnAccionDeVerbos(
                verbos,
                phase,
                m_dialogBox.isShowingHistory(),
                isModalRequired());

            // ── PODA DE `H10` (CP0 del atomo de teclado) ──────────────────────
            //
            // Aca vivia un bloque HUERFANO -sin `if`, sin `else if`- que repetia
            // las dos lineas de la rueda. Al no colgar de la cadena de arriba,
            // corria para CUALQUIER evento que llegara hasta el final, y leia
            // `e->wheel.y` de un union donde vivia otra cosa: en un KEYDOWN esos
            // bytes son parte del `keysym`, asi que una tecla no mapeada con un
            // modificador puesto podia devolver un scroll de historial que nadie
            // pidio. El contrato lo ficho como `H10` el 2026-07-02 y seguia vivo.
            //
            // El guardian de que la forma no vuelva es la §10 de
            // `harness_teclas`: en `inputBindDeEvento` la lectura del union vive
            // DENTRO de su `case`, que es la forma que este bug no tiene.
            //
            // **Y `CP1` lo vuelve imposible por construcción, no por cuidado:**
            // el `return None` que cerraba esta función se fue con la cascada.
            // Hoy no hay "después de la cadena" donde un bloque pueda quedar
            // huérfano — la única lectura del union del evento vive en
            // `inputBindDeEvento`, adentro de su `switch`. El `None` sigue
            // existiendo: lo devuelve la ley 5 de `vnAccionDeVerbos`.
        }

        VNDialogBox m_dialogBox;

        // ── Clave de línea para skip-de-leído (C2) ────────────────────────────
        // Identifica UNA línea de diálogo/narración de forma estable entre
        // corridas. FNV-1a 64-bit sobre (scriptPath | escena | cmdIndex |
        // speaker | textoCRUDO). Decisiones de diseño:
        //   · NO std::hash: no garantiza el mismo valor entre builds/plataformas,
        //     y este hash se PERSISTE a disco — tiene que ser determinista.
        //   · Texto y speaker CRUDOS (cmd.arg2/arg1, sin interpolar): así la
        //     misma línea es "la misma" sin importar el ${nombre} del jugador, y
        //     renombrar el display name de un personaje no fuerza re-leer.
        //   · cmdIndex dentro de la clave: desambigua dos líneas de texto
        //     idéntico en la misma escena (mil "..." de parodia no colisionan).
        //   · Editar el TEXTO de una línea cambia su hash → se trata como no
        //     vista → el skip frena ahí. En dev es lo correcto (texto nuevo se
        //     lee); en juego shippeado el script está congelado → estable.
        //   · Separador \x1f (US): nunca aparece en texto normal → evita que
        //     "ab"+"c" y "a"+"bc" colisionen.
        uint64_t computeLineKey(const std::string& speaker,
            const std::string& rawText) const
        {
            std::string s;
            s.reserve(m_scriptPath.size() + m_currentScene.size()
                + speaker.size() + rawText.size() + 24);
            s += m_scriptPath;               s += '\x1f';
            s += m_currentScene;             s += '\x1f';
            s += std::to_string(m_cmdIndex); s += '\x1f';
            s += speaker;                    s += '\x1f';
            s += rawText;

            uint64_t h = 1469598103934665603ULL;   // FNV offset basis
            for (unsigned char c : s) {
                h ^= c;
                h *= 1099511628211ULL;              // FNV prime
            }
            return h;
        }

        // ── Ejecutar el comando actual y avanzar ──────────────────────────────

        void executeNextCmd()
        {
            // Buscar la escena actual
            auto sceneIt = m_script.scenes.find(m_currentScene);
            if (sceneIt == m_script.scenes.end())
            {
                std::cerr << "[VNScriptState] Escena no encontrada: '"
                    << m_currentScene << "'" << std::endl;
               m_stateStack->cmdPop(this);
                return;
            }

            const std::vector<EclipCmd>& cmds = sceneIt->second.cmds;

            // ¿Se acabaron los comandos? Con ancla de evento activa, la escena
            // que termina es la de un evento random: RETORNO en vez de fin del
            // VN (sin esto, el "bostezo" canónico sacaba al jugador del juego).
            if (m_cmdIndex >= (int)cmds.size())
            {
                if (!m_preEventScene.empty()) { returnFromEvent(); return; }

                std::cout << "[VNScriptState] Escena '" << m_currentScene
                    << "' terminada (sin fin explicito)." << std::endl;
               cerrarPantalla();   // lote B: sin bloque, es el pop de siempre
                return;
            }

            const EclipCmd& cmd = cmds[m_cmdIndex];

            switch (cmd.type)
            {
                // ── Comandos instantáneos ──────────────────────────────────────

            case EclipCmdType::Personaje:
                // Solo registro — ya fue procesado en el parser globalmente
                m_cmdIndex++;
                break;

            case EclipCmdType::Escena:
                // Etiqueta de escena dentro de la lista de cmds, ignorar
                m_cmdIndex++;
                break;

            case EclipCmdType::AmbitoBegin:
                enterAmbito(cmd.arg1);
                m_cmdIndex++;
                break;

            case EclipCmdType::AmbitoEnd:
                // The lexical boundary ends the current attribution only.
                // Retirement remains the responsibility of the next
                // homonymous enterAmbito(); a close never deletes a page.
                m_activeScopeOwner = 0;
                m_cmdIndex++;
                break;

                // ── Cinemática: Begin sella el input, End lo libera (CP1) ──────
                // Espeja modalidad-aviso (m_modalRequired): mientras m_cinematicaActive,
                // el gate de skip (update) cae a false → 'esperar' corre completo y el
                // click no salta. saltable=si → no sella (deja fast-forward).
            case EclipCmdType::CinematicaBegin:
                m_cinematicaActive = !cmd.cineSaltable;
                if (m_cinematicaActive) m_autoMode = false; // limpia auto (futuro-safe)
                m_cmdIndex++;
                break;

            case EclipCmdType::CinematicaEnd:
                m_cinematicaActive = false;
                m_cmdIndex++;
                break;

            case EclipCmdType::Fondo:
            {
                std::string ruta = resolverImagen(cmd.arg1, cmd.arg1Literal);
                if (!cmd.arg1Literal && ruta.empty())
                {
                    // Nombre lógico no definido en el manifest. Aviso pedagógico
                    // y mantener fondo actual (§7.34: no reemplazar estado vivo
                    // con basura — aquí, no pintar negro). loadBackground("") ya
                    // hace return, pero el aviso explícito enseña el typo.
                    std::cerr << "[VNScriptState] Aviso linea " << cmd.sourceLine
                        << ": fondo '" << cmd.arg1 << "' no esta definido en el "
                        "bloque imagenes de proyecto.eclips. Mantengo el fondo "
                        "actual. (Para ruta literal usa comillas: fondo \""
                        << cmd.arg1 << "\")" << std::endl;
                }
                else
                {
                    // El fondo saliente conserva su tinte durante la transición.
                    m_transOldBgTintHex = m_bgTintHex;
                    m_transOldBgTintR = m_bgTintR;
                    m_transOldBgTintG = m_bgTintG;
                    m_transOldBgTintB = m_bgTintB;

                    loadBackground(ruta);

                    // Solo si el fondo de verdad quedó en `ruta` (el load pudo
                    // fallar y mantener el actual — no re-tintar lo que no se
                    // pidió). La línea manda: sin tinte= = fondo limpio.
                    if (m_currentBgPath == ruta)
                        setBgTint(cmd.tintHex);
                }
                m_cmdIndex++;
                break;
            }

            case EclipCmdType::Mostrar:
                cmdMostrar(cmd.arg1, cmd.arg3, cmd.arg2, cmd.charEffect);
                m_cmdIndex++;
                break;

            case EclipCmdType::Ocultar:
                cmdOcultar(cmd.arg1, cmd.charEffect);
                m_cmdIndex++;
                break;

            case EclipCmdType::Musica:
                if (m_hasAudio) m_audio.playMusic(cmd.arg1, true);
                m_currentMusicPath = cmd.arg1;
                m_cmdIndex++;
                break;

            case EclipCmdType::Sonido:
                if (m_hasAudio) m_audio.playSound(cmd.arg1);
                m_cmdIndex++;
                break;

            case EclipCmdType::PararMusica:
                if (m_hasAudio) m_audio.stopMusic();
                // Sin musica no hay ruta que guardar: el save la escribia y el
                // load la restauraba en loop.
                m_currentMusicPath.clear();
                m_cmdIndex++;
                break;

            case EclipCmdType::Transicion:
                m_pendingTransition = parseTransitionType(cmd.arg1);
                m_cmdIndex++;
                break;

            case EclipCmdType::IrA:
                jumpToScene(cmd.arg1);
                return; // jumpToScene modifica m_cmdIndex, salir del switch

            case EclipCmdType::Fin:
                std::cout << "[VNScriptState] fin — cerrando escena '"
                    << m_currentScene << "'" << std::endl;
               cerrarPantalla();   // lote B: sin bloque, es el pop de siempre
                return;

                // ── Comandos que esperan input ─────────────────────────────────

            case EclipCmdType::Dialogo:
                m_currentSpeaker = cmd.arg1;          // Aún se usa para el speaker highlight
                {
                    // Interpolar ${var} ANTES de parsear RichText: el valor
                    // sustituido fluye al typewriter/wrap/efectos y las metricas
                    // se calculan sobre el texto final. "shown" = lo que ve el
                    // jugador (display + historial + preview consistentes).
                    std::string shown = interpolateVars(t(cmd.arg2));
                    // Resolver alias → display name (Pasada 2C).
                    // m_currentSpeaker conserva el alias (para lookups internos).
                    // displayName solo se usa para lo que ve el jugador.
                    const std::string displayName = resolveSpeakerName(cmd.arg1);
                    // P1: comillas de presentación del personaje. El motor las
                    // ENVUELVE alrededor del contenido ya extraído; spk nullptr
                    // (narrador / sin manifest / no definido) → sin comillas.
                    // Reusa spk para velocidad + comillas (una sola resolución).
                    const PersonajeManifest* spk = resolveSpeakerManifest(cmd.arg1);
                    std::string quotedShown = wrapWithQuotes(shown, spk);
                    // Activar cadena de velocidad: pasar PersonajeManifest del
                    // speaker al dialog box. nullptr (alias no definido) es
                    // válido: cae a niveles posteriores de la cadena.
                    m_dialogBox.setActiveSpeaker(spk);
                    auto rich = eclips::TextRenderer::parseRichText(quotedShown);
                    ligarFuentesRichText(rich);
                    // Historial y preview de save = texto VISIBLE (rich.displayText,
                    // ya sin {tags}). El VN dibuja desde 'rich' (tags activos), pero
                    // el backlog/preview guardan el texto limpio. Antes guardaban
                    // quotedShown crudo → "{temblor}" salia literal en el log.
                    // Historial = rich.displayText (CON comillas de presentacion,
                    // fiel a lo visto). Preview de slot = glimpse, NO transcripcion:
                    // parsea 'shown' (SIN comillas) → el recorte por elipsis no deja
                    // la comilla de apertura huerfana ("...dialogo…  sin cierre).
                    m_lastDialogue = eclips::TextRenderer::parseRichText(shown).displayText;
                    // ── ¡Nuevo! Pasar los datos al componente ──
                    m_dialogBox.startDialogue(displayName, rich);
                    if (!cmd.arg3.empty())
                        swapExpression(cmd.arg1, cmd.arg3);
                    m_phase = VNPhase::WaitingInput;
                    m_dialogBox.pushHistory(displayName, rich.displayText);
                    // C2: clave de ESTA línea (cmdIndex aún estable; el avance
                    // del índice ocurre en el Advance). Se marca al avanzar.
                    m_currentLineKey = computeLineKey(cmd.arg1, cmd.arg2);
                    // `D-33`: si el renglón traía el `elegir` pegado, la línea
                    // no para acá — pisa el `WaitingInput` de arriba y sigue.
                    if (cmd.fusedChoice) fusionarConEleccion(rich.interrupts);
                }
                break;

            case EclipCmdType::Narrador:
                // El cuerpo vive en `mostrarNarracion` porque el bloque `azar:`
                // (pierna B) muestra EXACTAMENTE lo mismo cuando la entrada
                // sorteada es un texto: una narración es una narración, la
                // haya elegido el guion o el azar. Copiarlo habría sido la
                // enfermedad que este drenaje viene deshaciendo (§7.323).
                mostrarNarracion(cmd.arg1, cmd.fusedChoice);
                break;

                // ── Variables y condicionales (instantáneos) ────────────────

            case EclipCmdType::Set:
                executeSet(cmd);
                m_cmdIndex++;
                break;

            case EclipCmdType::MetaSet:
                executeMetaSet(cmd);
                m_cmdIndex++;
                break;

            case EclipCmdType::MetaClear:
                executeMetaClear();
                m_cmdIndex++;
                break;

            case EclipCmdType::Si:
                if (!evaluateCondition(cmd))
                {
                    // Condición falsa → saltar a sino/fin_si
                    if (cmd.jumpTarget >= 0)
                        m_cmdIndex = cmd.jumpTarget;
                    else
                        m_cmdIndex++; // Fallback si no se resolvió
                }
                else
                {
                    m_cmdIndex++; // Condición verdadera → entrar al bloque
                }
                break;

            case EclipCmdType::Sino:
                // Si llegamos aquí naturalmente, la rama true terminó
                // → saltar al fin_si (no ejecutar la rama else)
                if (cmd.jumpTarget >= 0)
                    m_cmdIndex = cmd.jumpTarget;
                else
                    m_cmdIndex++;
                break;

            case EclipCmdType::FinSi:
                // Marcador de cierre, solo avanzar
                m_cmdIndex++;
                break;

            case EclipCmdType::Elegir:
            {
                // Interpolar ${var} en los labels (texto que el jugador lee, igual
                // que el dialogo). El target (-> escena) no se toca.
                std::vector<EclipChoice> visibleChoices;
                int runtimeDefaultIndex = -1;

                for (int i = 0; i < (int)cmd.choices.size(); ++i)
                {
                    EclipChoice c = cmd.choices[i];
                    if (c.gate.has && !evaluateCondition(c.gate.var, c.gate.op, c.gate.val))
                    {
                        std::cout << "[VNScriptState] Opcion '" << c.label
                                  << "' oculta (condicion '" << c.gate.var
                                  << " " << c.gate.op << " " << c.gate.val
                                  << "' evaluo a falso)." << std::endl;
                        continue;
                    }
                    
                    c.label = interpolateVars(t(c.label));
                    
                    if (i == cmd.defaultIndex)
                    {
                        runtimeDefaultIndex = (int)visibleChoices.size();
                    }
                    visibleChoices.push_back(c);
                }

                // Si el default anterior se filtró, asignamos -1 (o el ultimo)
                if (cmd.defaultIndex >= 0 && runtimeDefaultIndex == -1 && !visibleChoices.empty())
                {
                    runtimeDefaultIndex = (int)visibleChoices.size() - 1;
                }

                // Soft-lock guard: si el gate [si:] oculto TODAS las opciones, el
                // jugador queda atascado en WaitingChoice sin forma de avanzar. El
                // motor no inventa salida (eso es del creador); solo lo nombra.
                if (visibleChoices.empty() && !cmd.choices.empty())
                {
                    std::cerr << "[VNScriptState] elegir: TODAS las opciones quedaron "
                        "ocultas por su gate [si:] (linea " << cmd.sourceLine
                        << "). El jugador no puede avanzar (soft-lock). Deja al "
                        "menos una opcion sin condicion." << std::endl;
                }

                // Secuencias del elegir (DISENO_SECUENCIA SQ-1 / SQ-11):
                // reasignar SIEMPRE, aunque quede vacia (R3: no heredar).
                {
                    std::vector<VNSecuencia> nuevas;
                    for (const EclipSecuencia& sec : cmd.secuencias)
                    {
                        if (sec.gate.has &&
                            !evaluateCondition(sec.gate.var, sec.gate.op, sec.gate.val))
                            continue;

                        std::vector<std::int32_t> teclasOk;
                        bool mala = false;
                        for (const std::string& nm : sec.teclas)
                        {
                            const SecuenciaMiembro mi = inputMiembroDeSecuencia(nm);
                            if (mi.motivo != SecuenciaMotivo::Ok)
                            {
                                // Una voz por renglon en toda la vida del state (R11).
                                if (m_secuenciaAvisadas.insert(sec.sourceLine).second)
                                {
                                    std::cerr << "[VNScriptState] secuencia: '"
                                        << nm
                                        << "' no es una tecla de secuencia (linea "
                                        << sec.sourceLine
                                        << "). Se ignora el renglon." << std::endl;
                                }
                                mala = true;
                                break; // el renglon muere entero
                            }
                            teclasOk.push_back(static_cast<std::int32_t>(mi.tecla));
                        }
                        if (mala || teclasOk.size() < 2)
                            continue;
                        nuevas.push_back(vnSecuenciaArmar(teclasOk, sec.target));
                    }
                    m_secuenciasArmadas = std::move(nuevas);
                }
                m_currentChoices = visibleChoices;           // Temporal para input numérico
                m_dialogBox.startChoices(visibleChoices, cmd.timerSeconds, runtimeDefaultIndex);
                m_phase = VNPhase::WaitingChoice;
                break;
            }

            case EclipCmdType::Qte:
            {
                SDL_Keycode kc = parseKeyName(cmd.qteKey);
                m_dialogBox.startQTE(cmd.qteKey, static_cast<uint32_t>(kc), cmd.timerSeconds,
                    cmd.qteSuccess, cmd.qteFail);
                // Acá se copiaban `kc` y el nombre a `m_qteKeycode`/`m_qteKeyName`
                // del state, y **no los leía nadie**: la caja ya es la dueña de
                // los dos. Dos campos que PARECEN la fuente de la tecla mientras
                // la fuente vive en otro objeto es cómo nace la próxima §7.442,
                // así que se van con el lote que por fin usa esa tecla.
                m_qteSuccess = cmd.qteSuccess;
                m_qteFail = cmd.qteFail;
                m_phase = VNPhase::WaitingQTE;
            }
            break;

            // ── Capas y Hotspots (Bloque 1 — Metaverse IDE) ──────────

            case EclipCmdType::Capa:
                cmdCapa(cmd);
                m_cmdIndex++;
                break;

            case EclipCmdType::Control:
                cmdControl(cmd);
                m_cmdIndex++;
                break;

            case EclipCmdType::Ranura:
                cmdRanura(cmd);
                m_cmdIndex++;
                break;

            case EclipCmdType::Texto:
                cmdTexto(cmd);
                m_cmdIndex++;   // está aquí en mi copia
                break;

            case EclipCmdType::Fuente:
                cmdFuente(cmd);
                m_cmdIndex++;          // ← esto faltaba
                break;

            case EclipCmdType::CapaZ:
                m_sceneLayers.setZOrder(cmd.arg1, cmd.layerZ);
                m_cmdIndex++;
                break;

            case EclipCmdType::CapaVisible:
                m_sceneLayers.setVisible(cmd.arg1, cmd.arg2 == "1");
                m_cmdIndex++;
                break;

            case EclipCmdType::CapaFade:
                m_sceneLayers.startFade(cmd.arg1, cmd.arg2 == "1",
                    cmd.animDuration);
                m_cmdIndex++;
                break;

            case EclipCmdType::CapaMover:
                m_sceneLayers.startMove(cmd.arg1, cmd.layerX, cmd.layerY,
                    cmd.animDuration, cmd.moveLoop, cmd.moveCurve);
                m_cmdIndex++;
                break;

            // ── Consola (CP4): es un SceneElement → capa_mover/animar/capa_z
            //    funcionan sobre su id sin comandos nuevos. ──
            case EclipCmdType::ConsolaCrear:
            {
                ConsoleData style;
                style.bgR = cmd.consoleBgR; style.bgG = cmd.consoleBgG;
                style.bgB = cmd.consoleBgB; style.bgA = cmd.consoleBgA;
                style.textR = cmd.consoleTxR; style.textG = cmd.consoleTxG;
                style.textB = cmd.consoleTxB;
                style.fontName = cmd.fontName;
                // CP-R3 · identidad tardía 3 de 3: la consola se queda con la
                // fuente del archivo que la creó, aunque después se encadene
                // otro que reuse el nombre.
                style.fontKey = catalogoDeResolucion().resolver(cmd.fontName);
                style.fontSize = cmd.fontSize;
                style.cursor = cmd.consoleCursor;
                m_sceneLayers.addConsole(cmd.arg1, cmd.layerZ,
                    cmd.layerX, cmd.layerY, cmd.layerW, cmd.layerH, style,
                    m_activeScopeOwner);
                m_cmdIndex++;
                break;
            }

            case EclipCmdType::ConsolaEscribir:
                m_sceneLayers.consoleWrite(cmd.arg1, cmd.arg2);
                m_cmdIndex++;
                break;

            case EclipCmdType::ConsolaImprimir:
                m_sceneLayers.consolePrint(cmd.arg1, cmd.arg2);
                m_cmdIndex++;
                break;

            case EclipCmdType::ConsolaLimpiar:
                m_sceneLayers.consoleClear(cmd.arg1);
                m_cmdIndex++;
                break;

            case EclipCmdType::ConsolaBorrar:
            {
                const std::string& a = cmd.arg2;   // "N" | "todo" | ""
                if (a == "todo" || a == "all")
                    m_sceneLayers.consoleErase(cmd.arg1, 0, true);
                else
                {
                    int n = 1;   // vacío = 1 char (backspace)
                    if (!a.empty()) { try { n = std::stoi(a); } catch (...) { n = 1; } }
                    m_sceneLayers.consoleErase(cmd.arg1, n, false);
                }
                m_cmdIndex++;
                break;
            }

            case EclipCmdType::HotspotDef:
                m_hotspots.add(cmd.arg1, cmd.layerX, cmd.layerY,
                    cmd.layerW, cmd.layerH, m_activeScopeOwner);
                m_cmdIndex++;
                break;

            case EclipCmdType::Enlace:
                // Átomo clickable: marca una capa ya existente (texto O imagen)
                // como interactiva. arg2 = destino ("" = continuar). arg3 =
                // imagen_hover opcional (PNG "encendido" del enlace-imagen).
                if (SceneElement* le = m_sceneLayers.get(cmd.arg1))
                {
                    le->interactive = true;
                    le->target = cmd.arg2;   // "" = continuar

                    // modalidad-aviso: un enlace-OK 'requerido' vuelve la caja
                    // modal mientras viva (se apaga al despacharlo o cambiar de
                    // escena). Solo si el enlace existió (estamos dentro del if).
                    if (cmd.linkRequired)
                    {
                        m_modalRequired = true;
                        m_modalScopeOwner = m_activeScopeOwner;
                    }
                    
                    // Enlace-IMAGEN: cargar el PNG "encendido" (mismo patrón
                    // que cmdCapa: get + pin + trackear para unpin).
                    if (!cmd.arg3.empty())
                    {
                        Texture * htex = m_assets.get(cmd.arg3);
                        if (htex)
                        {
                            m_assets.pin(cmd.arg3);
                            le->hoverTexture = htex;
                            m_layerHoverPaths[cmd.arg1] = cmd.arg3;
                            m_layerHoverOwners[cmd.arg1] = m_activeScopeOwner;
                        }
                         else
                         {
                            std::cerr << "[VNScriptState] enlace: no se pudo cargar "
                                 "imagen_hover '" << cmd.arg3 << "' para la capa \""
                                 << cmd.arg1 << "\". Queda clickable sin swap visual."
                                 << std::endl;
                         }
                    }
                }
                else
                {
                    // Anti-gemelo-mudo (§7.89): enlace sin texto que matchee no
                    // es un no-op silencioso.
                    std::cerr << "[VNScriptState] enlace: no existe una capa con id \""
                        << cmd.arg1 << "\". Declarala antes con 'capa " << cmd.arg1
                        << " \"...\"' (imagen) o 'texto " << cmd.arg1
                        << " \"...\"' para volverla clickable. Ignorado." << std::endl;
                }
                m_cmdIndex++;
                break;

            case EclipCmdType::EsperarClick:
            {
                m_clickMappings.clear();
                int missingHotspots = 0;
                int gateDisabledHotspots = 0;
                for (const auto& entry : cmd.choices)
                {
                    // La existencia es la primera frontera diagnostica: un
                    // id ausente no debe evaluar su gate ni producir una voz
                    // secundaria por una comparacion malformada.
                    if (!hasHotspotNamed(entry.label))
                    {
                        ++missingHotspots;
                        warnMissingHotspotForWait(entry, cmd.sourceLine);
                        continue;
                    }

                    const bool gatePass =
                        !entry.gate.has
                        || evaluateCondition(entry.gate.var, entry.gate.op,
                                             entry.gate.val);
                    const int matches = m_hotspots.setEnabled(
                        entry.label, gatePass);
                    if (matches == 0)
                    {
                        // Conserva la defensa si el manager cambia entre la
                        // comprobacion de existencia y la mutacion.
                        ++missingHotspots;
                        warnMissingHotspotForWait(entry, cmd.sourceLine);
                        continue;
                    }

                    if (!gatePass)
                    {
                        std::cout << "[VNScriptState] Hotspot destino '" << entry.target
                                  << "' omitido (condicion '" << entry.gate.var
                                  << " " << entry.gate.op << " " << entry.gate.val
                                  << "' evaluo a falso). Hotspot deshabilitado." << std::endl;
                        ++gateDisabledHotspots;
                    }
                    else
                    {
                        EclipChoice mapping = entry;
                        mapping.scopeOwner = m_activeScopeOwner;
                        m_clickMappings.push_back(mapping);
                    }
                }

                // A later gate can disable every hotspot for a label that was
                // already admitted above.  Filter only after all gates so
                // true -> false -> true keeps the existing target order.
                m_clickMappings.erase(
                    std::remove_if(m_clickMappings.begin(), m_clickMappings.end(),
                        [this](const EclipChoice& mapping) {
                            return !hasEnabledHotspotNamed(mapping.label);
                        }),
                    m_clickMappings.end());

                // Soft-lock guard: si el gate [si:] deshabilito TODOS los hotspots
                // mapeados, no queda nada clickeable y el jugador se atasca en
                // WaitingClick. Mismo principio que en elegir: avisar, no inventar.
                if (m_clickMappings.empty() && !cmd.choices.empty())
                {
                    std::cerr << "[VNScriptState] esperar_click: cero salidas "
                        "efectivas (linea " << cmd.sourceLine << ")";
                    if (missingHotspots > 0 && gateDisabledHotspots > 0)
                        std::cerr << "; mezcla de " << missingHotspots
                            << " ausentes y " << gateDisabledHotspots
                            << " gates";
                    else if (missingHotspots > 0)
                        std::cerr << "; todos los hotspots nombrados estan ausentes";
                    else if (gateDisabledHotspots > 0)
                        std::cerr << "; todos los hotspots existentes quedaron "
                            "deshabilitados por gate";
                    else
                        std::cerr << "; no hay un hotspot habilitado";
                    std::cerr << ". El jugador no puede avanzar (soft-lock)."
                        << std::endl;
                }

                m_phase = VNPhase::WaitingClick;
                break;
            }

            case EclipCmdType::EsperarInterfaz:
                // Hold explícito: no diálogo, no timer y sin incrementar el
                // índice. Enlaces, controles, ranuras y teclas siguen pasando
                // por sus cables de input/foco ya existentes.
                m_phase = VNPhase::WaitingInterface;
                break;

            case EclipCmdType::EventoRandom:
            {
                RandomEventDef ev;
                ev.name = cmd.arg1;
                ev.target = cmd.eventTarget;
                ev.gate = cmd.eventGate;   // pierna C: viaja tal cual del parser
                // Runtime protege (el parser ya habló, con línea): el rango se
                // clampea en UN lugar —`TrialClock.h`, la ley— y ninguna otra
                // pieza vuelve a validarlo.
                ev.trial.chance   = eclips::clampTrialChance(cmd.eventChance);
                ev.trial.period   = eclips::clampTrialPeriod((f64)cmd.eventEvery);
                ev.trial.cooldown = std::max(0.0, (f64)cmd.eventCooldown);
                ev.trial.cooldownRemaining = 0.0;
                // LEY 10 — fase inicial aleatoria por evento. Sin esto, tres
                // eventos con `cada=5` tiran todos en el mismo beat y se oye el
                // pulso. (FNAF le da a cada animatrónico un intervalo
                // casi-igual-pero-no por la misma razón; la fase es más barata
                // y más general.) Definir es sortear: una redefinición trae
                // fase nueva, porque la fase es sorteo y el sorteo no se conserva.
                ev.trial.beatRemaining =
                    eclips::trialPhase(ev.trial.period, m_rng);
                // Redefinir conserva el lugar en el orden de disparo
                // ("reiniciar" no significa "reordenar", misma ley que
                // emitirEfecto). Nombre nuevo: al final.
                auto prev = m_randomEvents.find(cmd.arg1);
                ev.order = (prev != m_randomEvents.end())
                    ? prev->second.order : m_eventOrderNext++;
                m_randomEvents[cmd.arg1] = ev;
                std::cout << "[VNScriptState] Evento random: '" << cmd.arg1
                    << "' probabilidad=" << ev.trial.chance
                    << " cada=" << ev.trial.period << "s"
                    << " cooldown=" << ev.trial.cooldown;
                if (ev.gate.has)
                    std::cout << " [si: " << ev.gate.var << " " << ev.gate.op
                              << " " << ev.gate.val << "]";
                std::cout << " -> " << cmd.eventTarget << std::endl;
                m_cmdIndex++;
                break;
            }

            case EclipCmdType::IniciarEventos:
                m_eventsActive = true;
                if (m_randomEvents.empty())
                    std::cerr << "[VNScriptState] iniciar_eventos: no hay ningun "
                        "evento definido (todavia). Define alguno con "
                        "evento_random o esto no hara nada." << std::endl;
                // LEY 16 — al re-arrancar, cada evento vuelve a sortear su fase.
                // Conservarla haría que un `detener`/`iniciar` los RE-SINCRONICE
                // a todos, que es exactamente el defecto que la ley 7 evita en el
                // load. La fase es sorteo, y el sorteo no se conserva.
                for (auto& [nombre, ev] : m_randomEvents)
                {
                    (void)nombre;
                    ev.trial.beatRemaining =
                        eclips::trialPhase(ev.trial.period, m_rng);
                }
                std::cout << "[VNScriptState] Eventos random activados" << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::DetenerEventos:
                m_eventsActive = false;
                std::cout << "[VNScriptState] Eventos random desactivados" << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::AzarTraza:
                // Interruptor de depuración. NO se serializa: es una decisión
                // del guion que se está corriendo, no estado de la partida.
                m_azarTraza = (cmd.arg1 == "1");
                std::cout << "[VNScriptState] Traza del azar: "
                    << (m_azarTraza ? "ENCENDIDA (un renglon por beat)" : "apagada")
                    << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::Azar:
            {
                // ── EL BLOQUE `azar:` — el eje CUAL (pierna B) ─────────────
                // Ley 3: la exclusividad la carga el BLOQUE. Esto es UN ensayo
                // con N salidas, así que se elige UNA y se ejecuta.
                std::vector<f32> pesos;
                pesos.reserve(cmd.choices.size());
                for (const auto& c : cmd.choices) pesos.push_back(c.peso);

                // La bolsa es estado de CORRIDA (ley 7: el reloj se guarda, el
                // sorteo no). La clave es escena+línea porque es lo único
                // estable entre visitas al mismo bloque; nunca toca el disco, y
                // se limpia en los tres lugares que la ley nombra: onExit,
                // loadFromSlot y loadNewScript.
                const std::string claveBolsa =
                    m_currentScene + ":" + std::to_string(cmd.sourceLine);

                int elegido = eclips::kDrawNone;
                if (cmd.azarRepetir)
                    elegido = eclips::weightedPick(pesos, m_rng);
                else
                    elegido = eclips::bagPick(m_azarBolsas[claveBolsa],
                                              pesos, m_rng);

                if (elegido < 0)
                {
                    // El parser ya avisó con número de línea (todas en peso=0).
                    // Acá no se inventa una salida: se sigue, que es la ley 13
                    // —avisa y se desarma— y no un crash ni un silencio.
                    if (m_azarTraza)
                        std::cout << "[azar] bloque L" << cmd.sourceLine
                            << " - ninguna entrada elegible (todas en peso=0)"
                            << std::endl;
                    m_cmdIndex++;
                    break;
                }

                const auto& salida = cmd.choices[(size_t)elegido];

                if (m_azarTraza)
                {
                    // Los dos modos NO se narran igual, porque `peso=` no
                    // significa lo mismo en los dos (ley 5). En distribucion el
                    // peso se lee contra el total: `50/110` es "45.5% de las
                    // veces". En bolsa el total no dice nada —cada entrada sale
                    // una vez por ciclo—, y lo que el creador necesita ver es
                    // DONDE VA EL CICLO. Ademas hace observable la ley 7: tras
                    // un load, "quedan" vuelve al maximo.
                    std::cout << "[azar] bloque L" << cmd.sourceLine;
                    if (cmd.azarRepetir)
                    {
                        f32 total = 0.0f;
                        for (const f32 w : pesos)
                            if (w > 0.0f) total += w;
                        std::cout << " - distribucion - peso "
                            << salida.peso << "/" << total;
                    }
                    else
                    {
                        int elegibles = 0;
                        for (const f32 w : pesos)
                            if (w > 0.0f) ++elegibles;
                        std::cout << " - bolsa - peso " << salida.peso
                            << " - quedan "
                            << m_azarBolsas[claveBolsa].restantes
                            << " de " << elegibles;
                    }
                    std::cout << " -> " << (salida.target.empty()
                            ? ("\"" + salida.label + "\"") : salida.target)
                        << std::endl;
                }

                // Las dos formas del cuerpo. El salto sale por la MISMA puerta
                // que `ir_a` (`jumpToScene`), así que un destino inexistente
                // hereda la ley que ya existe en vez de estrenar una segunda
                // (ley 13).
                if (!salida.target.empty())
                {
                    jumpToScene(salida.target);
                    return;     // jumpToScene mueve m_cmdIndex
                }

                // Texto: es una narración, y la muestra el mismo camino que
                // `narrador`. El `m_cmdIndex++` lo hace el Advance, igual que
                // cualquier línea que espera input.
                mostrarNarracion(salida.label);
                break;
            }

            case EclipCmdType::Palabras:
            {
                // ── EL POOL CLICKEABLE — `CP2a`, forks A2 · B · C1 ─────────
                // A2: el bloque SINTETIZA `texto` + `enlace`, o sea escribe por
                // el creador las ~200 líneas que `A7` describe a mano. Por eso
                // acá no hay una línea de render: el render ya estaba pagado
                // (§7.383, tercera vez en el arco).

                // 1 · los pesos, tal cual su hermano `azar:`.
                std::vector<f32> pesosBase;
                pesosBase.reserve(cmd.choices.size());
                for (const auto& c : cmd.choices) pesosBase.push_back(c.peso);

                // 1.bis · EL PRESENT-IF (`CP3`, ley 0 de `VNPalabrasPagina.h`).
                //     El gate se evalúa ACÁ y no en la ley pura porque
                //     `evaluateCondition` lee los tres namespaces del state
                //     (sistema → partida → meta); lo que baja a la ley es el
                //     veredicto ya tomado, uno por palabra. Un ítem sin `[si:]`
                //     cuenta como abierto, que es lo que hace que todo pool sin
                //     condiciones —o sea el corpus entero de hoy— pase por acá
                //     sin cambiar un peso (`harness_palabras` 8.7).
                std::vector<bool> gateAbierto;
                gateAbierto.reserve(cmd.choices.size());
                for (const auto& c : cmd.choices)
                    gateAbierto.push_back(!c.gate.has ||
                        evaluateCondition(c.gate.var, c.gate.op, c.gate.val));

                std::vector<f32> pesos;
                const eclips::PalabrasCuentas cuentas =
                    eclips::palabrasPesosEfectivos(pesosBase, gateAbierto, pesos);

                // 2 · la página (ley 1). Misma clave de bolsa que `azar:`
                //     —escena + línea—, que es lo único estable entre visitas.
                const std::string claveBolsa =
                    m_currentScene + ":" + std::to_string(cmd.sourceLine);
                const std::vector<int> pagina = eclips::palabrasElegirPagina(
                    pesos, cmd.palabrasN, cmd.azarRepetir,
                    m_azarBolsas[claveBolsa], m_rng);

                if (pagina.empty())
                {
                    // El parser ya avisó de lo estructural; acá no se abre una
                    // pantalla vacía que espere para siempre —eso sería el
                    // soft-lock que `elegir` y `esperar_click` avisan—: se
                    // nombra y se sigue (ley 13 del eje CUAL).
                    //
                    // **Las dos causas van con su número, y esa es la parte que
                    // `CP3` cambió:** hasta hoy esta línea decía «todas en
                    // peso=0» siempre, y con un gate en el pool eso puede ser
                    // falso. Una causa manda a leer el `.ecl` y la otra a mirar
                    // el estado de la partida — un aviso que las confunde manda
                    // a leer el archivo equivocado.
                    std::cerr << "[VNScriptState] palabras: linea "
                        << cmd.sourceLine << ": ninguna palabra es elegible ("
                        << cuentas.ocultas << " ocultas por su gate [si:], "
                        << cuentas.apagadas << " en peso=0). El bloque se "
                        "saltea." << std::endl;
                    m_cmdIndex++;
                    break;
                }

                // 2.bis · la voz del gate (`CP3`, fork 3b). UNA por bloque y no
                //     una por palabra: `elegir` habla por opción oculta
                //     ([:3229]) y ahí son cinco, pero el pool que este contrato
                //     imagina tiene ~200 — serían 200 líneas (§7.493: causa
                //     permanente, una sola voz).
                //
                //     Y la segunda mitad tapa un agujero que el parser NO puede
                //     ver: su aviso «pediste N y el pool tiene M» cuenta el pool
                //     ENTERO ([EclipScript.h:4545]), gateadas incluidas, porque
                //     es de parse-time. El pool efectivo es de runtime.
                if (cuentas.ocultas > 0)
                {
                    std::cerr << "[VNScriptState] palabras: linea "
                        << cmd.sourceLine << ": " << cuentas.ocultas << " de "
                        << cmd.choices.size() << " palabras ocultas por su gate "
                        "[si:]";
                    if (!cmd.azarRepetir &&
                        (int)pagina.size() < cmd.palabrasN)
                        std::cerr << ", asi que se muestran " << pagina.size()
                            << " y no las " << cmd.palabrasN << " que pediste";
                    std::cerr << "." << std::endl;
                }

                // 3 · las capas, por `cmdTexto`. **El mismo merge de estilo que
                //     un `texto` escrito a mano** — fuente, color, glow, borde y
                //     la variante de HOVER. Escribirlo de nuevo acá sería la
                //     tercera copia de `MERGE-DUPLICADO`.
                limpiarPalabras();          // higiene: re-entrar re-sortea (`F4`)
                m_palabrasIds.clear();

                for (size_t k = 0; k < pagina.size(); ++k)
                {
                    const EclipChoice& item = cmd.choices[(size_t)pagina[k]];
                    const std::string id = "__palabra_L" +
                        std::to_string(cmd.sourceLine) + "_" +
                        std::to_string(k);

                    EclipCmd t;
                    t.type = EclipCmdType::Texto;
                    t.arg1 = id;
                    t.arg2 = item.label;
                    t.styleName = cmd.styleName;
                    t.mark(EclipField::Estilo, cmd.has(EclipField::Estilo));
                    t.layerZ = cmd.has(EclipField::Z) ? cmd.layerZ : 0;
                    t.mark(EclipField::Z);
                    // Nacen en el origen y se COLOCAN abajo, ya medidas: el
                    // acomodo necesita el ancho, y el ancho lo da la fuente que
                    // este mismo merge acaba de resolver.
                    t.layerX = 0.0f;
                    t.layerY = 0.0f;
                    t.mark(EclipField::X);
                    t.mark(EclipField::Y);
                    cmdTexto(t);

                    // Las dos llamadas cubren las DOS ramas de `cmdTexto`: crear
                    // y PARCHAR. Sin ellas, un pool re-entrado parcharía el texto
                    // de una capa que `limpiarPalabras` dejó invisible, y el
                    // creador vería la pantalla vacía.
                    m_sceneLayers.setVisible(id, true);
                    m_sceneLayers.setAlpha(id, 1.0f);

                    if (SceneElement* le = m_sceneLayers.get(id))
                    {
                        le->interactive = true;
                        // El AZÚCAR se expandió en parse-time (`CP1`), así que
                        // esto ya es lo que `dispatchTarget` espera: una escena,
                        // un verbo o el triple `var op valor`.
                        le->target = item.target;
                        le->hovered = false;
                        m_palabrasIds.push_back(id);
                    }
                }

                // 4 · el acomodo (ley 2), medido con la MISMA ley que el
                //     hit-test (`medirInteractivo`). Si fueran dos medidas, el
                //     jugador clickearía en un lugar y la palabra estaría en
                //     otro — §7.100.
                {
                    const f32 VW = (f32)eclips::viewport::width();
                    const f32 VH = (f32)eclips::viewport::height();

                    eclips::PalabrasArea area;
                    area.x = cmd.has(EclipField::X) ? cmd.layerX
                           : VW * eclips::kPalabrasAreaXRatio;
                    area.y = cmd.has(EclipField::Y) ? cmd.layerY
                           : VH * eclips::kPalabrasAreaYRatio;
                    area.ancho = cmd.has(EclipField::W) ? cmd.layerW
                               : VW * eclips::kPalabrasAreaAnchoRatio;
                    area.alto = cmd.has(EclipField::H) ? cmd.layerH
                              : VH * eclips::kPalabrasAreaAltoRatio;

                    std::vector<f32> anchos;
                    anchos.reserve(m_palabrasIds.size());
                    f32 altoLinea = 0.0f;
                    for (const std::string& id : m_palabrasIds)
                    {
                        f32 w = 0.0f, h = 0.0f;
                        const SceneElement* le = m_sceneLayers.get(id);
                        if (le && m_sceneLayers.medirInteractivo(*le,
                                m_hasFont ? &m_textRenderer : nullptr,
                                &m_fontManager, w, h))
                        {
                            if (h > altoLinea) altoLinea = h;
                        }
                        else w = 0.0f;
                        anchos.push_back(w);
                    }

                    const eclips::PalabrasAcomodo ac =
                        eclips::palabrasAcomodar(area, anchos, altoLinea);

                    for (size_t k = 0; k < m_palabrasIds.size() &&
                                       k < ac.celdas.size(); ++k)
                        m_sceneLayers.setPosition(m_palabrasIds[k],
                            ac.celdas[k].x, ac.celdas[k].y);

                    // Una voz, una vez, con su salida escrita (§7.493: causa
                    // permanente = una sola línea). NO se recortan palabras: el
                    // pool es lo que el creador escribió.
                    if (ac.desborda)
                        std::cerr << "[VNScriptState] palabras: linea "
                            << cmd.sourceLine << ": las "
                            << m_palabrasIds.size() << " palabras necesitan "
                            << ac.filas << " filas y no entran en el area. Se "
                            "dibujan igual, desbordadas: subi `alto=` o pedi "
                            "menos." << std::endl;
                }

                // 5 · la espera. `WaitingInput` + `m_modalRequired` no es una
                //     elección de estilo: es lo que la **ley 7** de
                //     `VNVerbRoute.h` exige para que el anillo de foco llegue a
                //     los enlaces, y el modal es lo que sella el avance plano
                //     —así la única salida es un pick—.
                //
                //     `m_cmdIndex` NO se toca: la espera se queda EN el bloque,
                //     misma ley que `elegir` y `esperar_click`, y es eso lo que
                //     hace que cargar un save re-ejecute el bloque y re-sortee
                //     (`A10` / §7.13) sin una línea de código de save.
                //
                //     La caja de diálogo conserva su última línea debajo del
                //     pool (nadie la borró), que es la conducta de
                //     `mostrar_caja`. Y sus seis botones de chrome siguen vivos,
                //     igual que bajo `modalidad-aviso` hoy: sellarlos durante el
                //     minijuego es `deshabilitar acciones`, o sea `CP4` (§7.5).
                m_palabrasActivas = true;
                m_modalRequired = true;
                m_focoPlano = -1;
                m_focoDelPuntero = true;
                m_phase = VNPhase::WaitingInput;
                break;
            }

            case EclipCmdType::AzarSemilla:
                // Ley 15 (DISENO_AZAR_ECLIPS): la semilla se declara. Sin esto,
                // la ley 12 pedía reproducibilidad y no había forma de pedirla —
                // y la sonda de distribución que el gate exige no podía existir.
                // Sin argumento vuelve al reloj: "dame azar de verdad otra vez".
                // La semilla NO se serializa (ley 7: el reloj se guarda, el
                // sorteo no).
                if (cmd.azarSeedFija)
                {
                    m_rng.seed(cmd.azarSeed);
                    std::cout << "[VNScriptState] Semilla del azar fijada en "
                        << cmd.azarSeed << std::endl;
                }
                else
                {
                    m_rng.seed(std::random_device{}());
                    std::cout << "[VNScriptState] Semilla del azar: reloj "
                        "(azar sin repetir)" << std::endl;
                }
                m_cmdIndex++;
                break;

            case EclipCmdType::PersonajeCompuesto:
            {
                // Registrar en runtime (para scripts que lo definen dentro de escena)
                std::string path = cmd.arg2;
                if (path.back() != '/' && path.back() != '\\')
                    path += '/';
                m_script.characters[cmd.arg1] = path;
                m_script.compositeChars.insert(cmd.arg1);
                m_cmdIndex++;
                break;
            }

            case EclipCmdType::ParallaxToggle:
                m_parallaxEnabled = (cmd.arg1 == "1");
                std::cout << "[VNScriptState] Parallax "
                    << (m_parallaxEnabled ? "ON" : "OFF") << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::SpeakerHighlight:
                m_speakerHighlight = (cmd.arg1 == "1");
                std::cout << "[VNScriptState] Speaker highlight "
                    << (m_speakerHighlight ? "ON" : "OFF") << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::OcultarCaja:
                m_dialogBox.hide();
                std::cout << "[VNScriptState] Caja oculta" << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::MostrarCaja:
                m_dialogBox.show();
                std::cout << "[VNScriptState] Caja visible" << std::endl;
                m_cmdIndex++;
                break;

            // ── EL SELLO DE ACCIONES (`DISENO_DESHABILITAR_ACCIONES.md`) ────
            // El parser ya resolvió las palabras a bits (`cmd.accionesMask`),
            // así que acá no se mira un solo string: es álgebra. El orden en
            // que el guion escribe los renglones ES el orden en que corren,
            // que es la ley 4 (`D-48`) — misma forma que `botones_visibles no`
            // + `boton_menu_visible si` del lado de la visibilidad.
            case EclipCmdType::DeshabilitarAcciones:
                m_accionesSelladas =
                    eclips::vnSeal(m_accionesSelladas, cmd.accionesMask);
                std::cout << "[VNScriptState] deshabilitar -> sello 0x"
                    << std::hex << m_accionesSelladas << std::dec << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::HabilitarAcciones:
                m_accionesSelladas =
                    eclips::vnUnseal(m_accionesSelladas, cmd.accionesMask);
                std::cout << "[VNScriptState] habilitar -> sello 0x"
                    << std::hex << m_accionesSelladas << std::dec << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::UsarCaja:
                m_dialogBox.setBoxOverride(cmd.arg1);
                std::cout << "[VNScriptState] usar_caja -> '"
                    << cmd.arg1 << "'" << std::endl;
                m_cmdIndex++;
                break;

            case EclipCmdType::Lua:
            {
                // Resolver directorio del script actual para rutas relativas
                std::string scriptDir;
                auto slashPos = m_scriptPath.find_last_of("/\\");
                if (slashPos != std::string::npos)
                    scriptDir = m_scriptPath.substr(0, slashPos);

                m_lua.executeFile(cmd.arg1, scriptDir);

                // Procesar acciones diferidas de Lua (goto / load_script)
                if (!m_luaPendingGoto.empty())
                {
                    std::string target = m_luaPendingGoto;
                    m_luaPendingGoto.clear();
                    jumpToScene(target);
                    return;
                }
                if (!m_luaPendingLoadScript.empty())
                {
                    std::string path = m_luaPendingLoadScript;
                    std::string scene = m_luaPendingLoadScriptScene;
                    m_luaPendingLoadScript.clear();
                    m_luaPendingLoadScriptScene.clear();
                    loadNewScript(path, scene);
                    return;
                }

                m_cmdIndex++;
                break;
            }

            case EclipCmdType::CargarScript:
                loadNewScript(cmd.arg1, cmd.arg2);
                return; // loadNewScript maneja m_cmdIndex y m_currentScene

            case EclipCmdType::Animar:
            {
                // ── C-CP5/E-CP5: receta nombrada — llega RESUELTA del parse ─
                // (fork 1a: el cmd es auto-contenido; acá solo se instancia.)
                // Dos especies en un nombre: canales → m_charTracks; líneas
                // de efecto → m_charFx. Cada blanco admite lo suyo, con voz.
                if (!cmd.namedName.empty())
                {
                    const bool esPantallaN =
                        (cmd.arg1 == "pantalla" || cmd.arg1 == "screen");
                    const bool esFondoN = !esPantallaN &&
                        (cmd.arg1 == "fondo" || cmd.arg1 == "background");
                    const SceneElement* capaN = (!esPantallaN && !esFondoN)
                        ? m_sceneLayers.get(cmd.arg1) : nullptr;

                    bool esCharVivoN = false;
                    if (!esPantallaN && !esFondoN)
                        for (const auto& ac : m_activeChars)
                            if (ac.alias == cmd.arg1) { esCharVivoN = true; break; }

                    // (Si el nombre es un personaje VIVO, el personaje gana —
                    //  misma regla que E-CP2.)
                    const bool esCapaOFondoN = !esCharVivoN && !esPantallaN &&
                        (esFondoN || capaN != nullptr);

                    bool aplicaTracks = !cmd.namedTracks.empty();
                    bool aplicaFx = !cmd.namedFx.empty();

                    if (esPantallaN)
                    {
                        // Los efectos fullscreen son ficha (b) — misma voz que
                        // el camino primitiva (escena → FBO → efecto, Track-N).
                        if (aplicaFx)
                        {
                            std::cerr << "[VN] AVISO: 'animar pantalla "
                                << cmd.namedName << "': los EFECTOS de la "
                                << "receta no corren sobre la pantalla "
                                << "completa todavia (ficha aparte). "
                                << (aplicaTracks ? "Los canales si corren."
                                                 : "Ese comando no hara nada.")
                                << std::endl;
                            aplicaFx = false;
                        }
                        // F5: la pantalla dibuja desplazamiento y flash;
                        // escala no tiene draw-site. Se DICE, no se tolera.
                        if (aplicaTracks)
                        {
                            bool tieneTrackAdmitido = false;
                            bool tieneDialogueAdvance = false;
                            for (const auto& tr : cmd.namedTracks)
                            {
                                if (tr.clock == ClockDomain::DialogueAdvance)
                                {
                                    tieneDialogueAdvance = true;
                                    continue; // CP1: caller solo personaje
                                }
                                tieneTrackAdmitido = true;
                                if (tr.canal == Channel::Scale)
                                {
                                    std::cerr << "[VN] AVISO: 'animar pantalla "
                                        << cmd.namedName << "': el canal escala "
                                        << "no tiene render en pantalla todavia. "
                                        << "Los demas canales corren."
                                        << std::endl;
                                }
                            }
                            if (tieneDialogueAdvance)
                                std::cerr << "[VN] AVISO: 'animar pantalla "
                                    << cmd.namedName << "': el reloj "
                                    "por avances de dialogo solo vale en personajes. "
                                    "Esos tracks se ignoran." << std::endl;
                            aplicaTracks = tieneTrackAdmitido;
                        }
                    }
                    else if (esCapaOFondoN)
                    {
                        // Una capa de texto/consola no tiene textura que
                        // re-estampar (ley E-CP2): el efecto JAMAS se veria.
                        if (aplicaFx && capaN && !capaN->texture)
                        {
                            std::cerr << "[VN] AVISO: 'animar " << cmd.arg1
                                << " " << cmd.namedName << "': la capa '"
                                << cmd.arg1 << "' no es de imagen. Los efectos "
                                << "de render corren sobre capas de imagen "
                                << "(texto/consola: ficha)." << std::endl;
                            aplicaFx = false;
                        }
                        // TRACK-CAPA-XY-01: una capa REAL admite solo offsets
                        // XY con reloj en segundos. La validacion es atomica:
                        // mezclar un canal/reloj excluido baja TODO el subgrupo
                        // de canales; los efectos conservan su especie aparte.
                        // El blanco especial fondo sigue fuera del puente.
                        if (aplicaTracks)
                        {
                            if (esFondoN)
                            {
                                std::cerr << "[VN] AVISO: 'animar " << cmd.arg1
                                    << " " << cmd.namedName << "': los CANALES "
                                    << "de la receta sobre el fondo especial "
                                    << "siguen fuera de TRACK-CAPA-XY-01"
                                    << (aplicaFx ? "; los efectos si corren."
                                                 : ". Ese comando no hara nada.")
                                    << std::endl;
                                aplicaTracks = false;
                            }
                            else
                            {
                                bool tracksCapaValidos = (capaN != nullptr);
                                for (const auto& tr : cmd.namedTracks)
                                {
                                    const bool canalXY =
                                        tr.canal == Channel::OffsetX ||
                                        tr.canal == Channel::OffsetY;
                                    if (!canalXY || tr.clock != ClockDomain::Seconds)
                                    {
                                        tracksCapaValidos = false;
                                        break;
                                    }
                                }
                                if (!tracksCapaValidos)
                                {
                                    std::cerr << "[VN] AVISO: 'animar "
                                        << cmd.arg1 << " " << cmd.namedName
                                        << "': TRACK-CAPA-XY-01 admite sobre "
                                        << "capas solo mover_x/mover_y con "
                                        << "reloj en segundos. Como la receta "
                                        << "mezcla un canal o reloj excluido, "
                                        << "ninguno de sus canales se aplica"
                                        << (aplicaFx ? "; los efectos si corren."
                                                     : ". Ese comando no hara nada.")
                                        << std::endl;
                                    aplicaTracks = false;
                                }
                            }
                        }
                        // Fondo sin fondo puesto: pre-staging con voz.
                        if (aplicaFx && esFondoN && m_currentBgPath.empty())
                            std::cerr << "[VN] AVISO: 'animar fondo "
                                << cmd.namedName << "': no hay fondo puesto. "
                                << "Los efectos quedan aplicados y se veran "
                                << "cuando haya fondo." << std::endl;
                    }
                    else if (!esCharVivoN)
                    {
                        // Fuera de escena se aplica igual (pre-staging), pero
                        // no en silencio — misma voz que presets y efectos.
                        std::cerr << "[VN] AVISO: 'animar " << cmd.arg1
                            << " " << cmd.namedName << "': '" << cmd.arg1
                            << "' no esta en escena (¿typo, o aparece "
                            << "despues?). La animacion queda aplicada y "
                            << "se vera cuando el personaje aparezca."
                            << std::endl;
                    }

                    if (!aplicaTracks && !aplicaFx)
                    {
                        m_cmdIndex++;
                        break;
                    }

                    // ── Canales → tracks. Re-animar el MISMO nombre reemplaza
                    // la instancia entera (params y reloj nuevos, §2); nombres
                    // DISTINTOS apilan por el álgebra (§7.208). El erase es
                    // correcto acá porque los canales componen SIN orden — a
                    // diferencia de los efectos, abajo (§7.258).
                    size_t nTracks = 0;
                    if (aplicaTracks)
                    {
                        if (esCapaOFondoN && capaN)
                        {
                            nTracks = m_sceneLayers.replaceTracks(cmd.arg1,
                                cmd.namedName, cmd.namedTracks, cmd.animAfter);
                        }
                        else
                        {
                            auto& tstack = esPantallaN
                                ? m_screenTracks
                                : m_charTracks[cmd.arg1];

                            tstack.erase(std::remove_if(tstack.begin(), tstack.end(),
                                [&cmd](const TrackInstance& t)
                                { return t.origen == cmd.namedName; }),
                                tstack.end());

                            for (const auto& tr : cmd.namedTracks)
                            {
                                if (esPantallaN &&
                                    tr.clock == ClockDomain::DialogueAdvance)
                                    continue;
                                tstack.push_back(startTrackInstance(
                                    tr, cmd.namedName, cmd.animAfter));
                                ++nTracks;
                            }
                        }
                    }

                    // ── Líneas de efecto → m_charFx, POR emitirEfecto: la
                    // misma primitiva re-emitida reinicia el reloj y PRESERVA
                    // su lugar en la tubería (ley E-CP1; acá NO se borra por
                    // origen — reordenar seria un bug importado, §7.258).
                    size_t nFx = 0;
                    if (aplicaFx)
                    {
                        const std::string blancoN =
                            esFondoN ? "fondo" : cmd.arg1;

                        const bool espoN =
                            cmd.fxParams.count("media") ||
                            cmd.fxParams.count("varianza") ||
                            cmd.fxParams.count("frames") ||
                            cmd.fxParams.count("desde");

                        // E-CP4 + E-CP5: la receta pulsa JUNTA. UNA semilla
                        // por instanciación, compartida entre sus líneas:
                        // mismos params + misma semilla = mismos sorteos =
                        // una sola ventana compartida. La 'corrupcion'
                        // esporádica es UN evento, no tres parpadeos sueltos.
                        uint32_t seedN = 0;
                        if (espoN)
                        {
                            static uint32_t s_fxSeedReceta = 0xC0FFEE5Du;
                            seedN = fxMix32(++s_fxSeedReceta * 2654435761u
                                ^ (uint32_t)SDL_GetTicks());
                        }

                        for (const auto& fl : cmd.namedFx)
                        {
                            EffectInstance fx;
                            fx.kind = fl.kind;
                            fx.luaName = fl.luaName;   // E-CP6: línea lua
                            fx.params.raw = fl.params;
                            // El driver viaja en el `animar` (§1.2): sus
                            // claves se superponen a las de la línea.
                            for (const auto& dk : cmd.fxParams)
                                fx.params.raw[dk.first] = dk.second;
                            // Intensidad: la de línea × la universal (G4,
                            // tipada — jamás cruza por string).
                            fx.intensidad = fl.intensidad * cmd.animIntensity;
                            fx.duration = cmd.animDuration;   // en= | posicional
                            fx.elapsed = 0.0f;
                            fx.origen = cmd.namedName;
                            // Reloj-diálogo CP2: la envolvente grupal se repite
                            // en cada hermana (§8); su progress avanza por
                            // diálogo (onDialogueAdvance), no aquí.
                            fx.envelope = cmd.namedFxEnvelope;

                            if (espoN)
                            {
                                fx.driver = FxDriver::Esporadico;
                                fx.duration = 0.0f;   // un driver por línea
                                fx.rngState = seedN;  // compartida: pulsa junta
                            }
                            else if (fx.duration > 0.0f)
                                fx.driver = FxDriver::UnaVez;

                            // E-CP6: línea lua sin registro vivo = esa línea
                            // no se aplica, con voz (la ley de miss).
                            if (fx.kind == FxKind::Lua &&
                                !m_luaFx.count(fx.luaName))
                            {
                                std::cerr << "[VN] AVISO: 'animar " << blancoN
                                    << " " << cmd.namedName << "': el efecto "
                                    << "lua '" << fx.luaName << "' no esta "
                                    << "registrado. Esa linea no se aplica."
                                    << std::endl;
                                continue;
                            }

                            // E-CP3: textura que no carga = línea removida AL
                            // NACER, con voz (jamas crash, jamas silencio).
                            const std::string texFx =
                                fx.params.getS("textura", "");
                            if (!texFx.empty() && !m_assets.get(texFx))
                            {
                                std::cerr << "[VN] AVISO: 'animar " << blancoN
                                    << " " << cmd.namedName << "': no se pudo "
                                    << "cargar textura= '" << texFx << "' ("
                                    << fxGenName(fx) << "). Esa "
                                    << "linea no se aplica." << std::endl;
                                continue;
                            }

                            emitirEfecto(m_charFx[blancoN], fx);
                            ++nFx;
                        }
                    }

                    std::cout << "[VN] Animar " << cmd.arg1 << " '"
                        << cmd.namedName << "' (" << nTracks << " canales + "
                        << nFx << " efectos)" << std::endl;

                    m_cmdIndex++;
                    break;
                }

                // ── E-CP0: dos vocabularios DISJUNTOS. Las primitivas de efecto
                // (franja, ...) van a su propio stack; los presets (glitch,
                // vaiven, ...) siguen por el camino de siempre. El parser ya
                // resolvió cuál es cuál — y ya avisó si no era ninguno.
                // E-CP6: el efecto LUA (cuarta especie) entra por el MISMO
                // camino que una primitiva — solo cambian el kind y de dónde
                // sale la transformación (el registro vivo, no la tabla C++).
                FxKind fk = parseFxKind(cmd.arg2);
                const bool esLuaCmd =
                    (fk == FxKind::None && !cmd.luaFxName.empty());
                if (fk != FxKind::None || esLuaCmd)
                {
                    if (cmd.arg1 == "pantalla" || cmd.arg1 == "screen")
                    {
                        // `pantalla` sigue siendo ficha (presupuesto (b): escena
                        // → FBO → efecto, Track-N). El motor lo DICE.
                        std::cerr << "[VN] AVISO: 'animar pantalla " << cmd.arg2
                            << "' todavia no existe: los efectos de render corren "
                            << "sobre personajes, fondo y capas. La pantalla "
                            << "completa es ficha aparte." << std::endl;
                        m_cmdIndex++;
                        break;
                    }

                    // ── E-CP2 (F3): blancos nuevos — `fondo` y capas de imagen.
                    const bool esFondo =
                        (cmd.arg1 == "fondo" || cmd.arg1 == "background");
                    const SceneElement* capaFx =
                        esFondo ? nullptr : m_sceneLayers.get(cmd.arg1);

                    if (capaFx && !capaFx->texture)
                    {
                        // Una capa de texto/consola no tiene textura que
                        // re-estampar: el efecto JAMÁS se vería. Se descarta
                        // con voz — guardarlo sería mentir.
                        std::cerr << "[VN] AVISO: 'animar " << cmd.arg1 << " "
                            << cmd.arg2 << "': la capa '" << cmd.arg1
                            << "' no es de imagen. Los efectos de render corren "
                            << "sobre capas de imagen (texto/consola: ficha)."
                            << std::endl;
                        m_cmdIndex++;
                        break;
                    }

                    // El blanco viaja con UNA grafía al stack (y mañana al `E;`).
                    const std::string blanco = esFondo ? "fondo" : cmd.arg1;

                    // Blanco fuera de escena: se aplica igual (pre-staging — se
                    // ve cuando aparezca; `ocultar` lo limpia), pero tickea
                    // invisible y eso se DICE, no se tolera callado.
                    bool enEscena = esFondo ? !m_currentBgPath.empty()
                                            : (capaFx != nullptr);
                    if (!enEscena)
                        for (const auto& ac : m_activeChars)
                            if (ac.alias == cmd.arg1) { enEscena = true; break; }
                    if (!enEscena)
                    {
                        if (esFondo)
                            std::cerr << "[VN] AVISO: 'animar fondo " << cmd.arg2
                                << "': no hay fondo puesto. El efecto queda "
                                << "aplicado y se vera cuando haya fondo."
                                << std::endl;
                        else
                            std::cerr << "[VN] AVISO: 'animar " << cmd.arg1 << " "
                                << cmd.arg2 << "': '" << cmd.arg1 << "' no esta en "
                                << "escena (¿typo, o aparece despues?). El efecto "
                                << "queda aplicado y se vera cuando el personaje "
                                << "aparezca." << std::endl;
                    }

                    // E-CP6: la FUNCIÓN se resuelve contra el registro VIVO.
                    // Declarado pero no registrado (su .lua no corrió, falló,
                    // o no llamó register_effect) = aviso + no se aplica —
                    // misma voz que la textura que no carga.
                    if (esLuaCmd && !m_luaFx.count(cmd.luaFxName))
                    {
                        std::cerr << "[VN] AVISO: 'animar " << cmd.arg1 << " "
                            << cmd.arg2 << "': el efecto lua no esta "
                            << "registrado (su archivo no corrio, fallo, o "
                            << "no llamo eclips.register_effect). No se "
                            << "aplica." << std::endl;
                        m_cmdIndex++;
                        break;
                    }

                    EffectInstance fx;
                    fx.kind = esLuaCmd ? FxKind::Lua : fk;
                    fx.luaName = cmd.luaFxName;
                    fx.params.raw = cmd.fxParams;   // claves ya canonizadas en parse-time
                    // `intensidad` es universal y vive TIPADA (§1.40). Antes daba
                    // la vuelta por to_string() -> 6 decimales -> vuelta a float:
                    // intensidad=1.23456789 llegaba como 1.234568. Un float no
                    // tiene por qué pasar por un string para cruzar dos structs.
                    fx.intensidad = cmd.animIntensity;
                    fx.duration = cmd.animDuration;   // 0 = perenne
                    fx.elapsed = 0.0f;

                    // ── E-CP4: el driver. Esporadico = presencia de sus claves
                    // (ya validadas y canonizadas en el k=v); nace dormido y la
                    // primera cita se sortea en su primer tick. La semilla es
                    // por-corrida — JAMAS parte del formato (mesa F1).
                    if (fx.params.has("media") || fx.params.has("varianza") ||
                        fx.params.has("frames") || fx.params.has("desde"))
                    {
                        fx.driver = FxDriver::Esporadico;
                        fx.duration = 0.0f;   // un driver por línea: perenne
                        static uint32_t s_fxSeed = 0x51ed0badu;
                        fx.rngState = fxMix32(++s_fxSeed * 2654435761u
                            ^ (uint32_t)SDL_GetTicks());
                    }
                    else if (fx.duration > 0.0f)
                    {
                        fx.driver = FxDriver::UnaVez;
                    }

                    // E-CP3: una textura pedida que no carga = efecto removido
                    // AL NACER, con voz (§3: jamas crash, jamas silencio). Asi
                    // el runner nunca se encuentra el miss.
                    {
                        const std::string texFx = fx.params.getS("textura", "");
                        if (!texFx.empty() && !m_assets.get(texFx))
                        {
                            std::cerr << "[VN] AVISO: 'animar " << blanco << " "
                                << cmd.arg2 << "': no se pudo cargar textura= '"
                                << texFx << "'. El efecto no se aplica."
                                << std::endl;
                            m_cmdIndex++;
                            break;
                        }
                    }

                    auto& fxStack = m_charFx[blanco];

                    // ── E-CP1: re-emitir REINICIA el reloj y PRESERVA la posición.
                    // NO es el espejo de C-CP2. Allá `erase + push_back` era
                    // correcto porque los canales componen SIN orden (§7.208): no
                    // había orden que preservar. Acá las pasadas se ENCADENAN.
                    // Copiar la mecánica habría importado un bug que allá no
                    // existía (§7.258): "reiniciar" no significa "reordenar".
                    //
                    // (Honestidad: la paleta de HOY conmuta — el harness lo midió,
                    //  T16 — así que esto es PROFILAXIS, no corrección. Deja de
                    //  serlo en E-CP3, con `estatica`/`sustituto`/`bloques`.)
                    emitirEfecto(fxStack, fx);

                    // El aviso de convivencia glitch↔efectos MURIÓ acá: `glitch` ya
                    // no tiene render propio que tape a nadie. Es un efecto más, y
                    // compone con los otros como cualquiera.

                    std::cout << "[VN] Efecto " << blanco << " "
                        << fxGenName(fx) << " (stack fx: " << fxStack.size()
                        << ")" << std::endl;

                    m_cmdIndex++;
                    break;
                }

                AnimType at = parseAnimType(cmd.arg2);
                if (at != AnimType::None)
                {
                    const bool esPantalla =
                        (cmd.arg1 == "pantalla" || cmd.arg1 == "screen");

                    // F5 (hallazgo §7.38): la pantalla solo DIBUJA desplazamiento
                    // y flash. `scale` no se lee en NINGÚN draw-site → acercar/
                    // alejar/retroceder sobre pantalla no hacían nada, en silencio.
                    // El motor deja de mentir: explica en vez de tolerar callado.
                    if (esPantalla && ownsScale(at))
                    {
                        std::cerr << "[VN] AVISO: 'animar pantalla " << cmd.arg2
                            << "' no hace nada todavia: la pantalla dibuja "
                            << "desplazamiento (temblar/vaiven) y destello, pero "
                            << "el zoom de pantalla aun no tiene render."
                            << std::endl;
                        m_cmdIndex++;
                        break;
                    }

                    if (!esPantalla)
                    {
                        bool esCharVivo = false;
                        for (const auto& ac : m_activeChars)
                            if (ac.alias == cmd.arg1) { esCharVivo = true; break; }

                        // ── E-CP2 (F3): los CANALES/presets sobre fondo y capas
                        // siguen siendo de capa_mover/capa_fade (el rebase al
                        // Animator está fichado con gatillo). Los EFECTOS sí
                        // corren sobre ellos — camino de arriba. Aviso pedagógico
                        // en vez de un stack que tickea sin draw-site. (Si el
                        // nombre es un personaje VIVO, el personaje gana.)
                        const bool esFondoAnim =
                            (cmd.arg1 == "fondo" || cmd.arg1 == "background");
                        if (!esCharVivo &&
                            (esFondoAnim || m_sceneLayers.get(cmd.arg1)))
                        {
                            std::cerr << "[VN] AVISO: 'animar " << cmd.arg1 << " "
                                << cmd.arg2 << "': los presets/canales sobre "
                                << "fondo y capas van por capa_mover/capa_fade. "
                                << "Los EFECTOS (franja, tajadas, ...) si corren "
                                << "sobre fondo/capas." << std::endl;
                            m_cmdIndex++;
                            break;
                        }

                        // Misma voz que el camino FX: fuera de escena se aplica
                        // igual, pero no en silencio.
                        if (!esCharVivo)
                            std::cerr << "[VN] AVISO: 'animar " << cmd.arg1 << " "
                                << cmd.arg2 << "': '" << cmd.arg1 << "' no esta en "
                                << "escena (¿typo, o aparece despues?). La animacion "
                                << "queda aplicada y se vera cuando el personaje "
                                << "aparezca." << std::endl;
                    }

                    std::vector<CharAnim>& stack = esPantalla
                        ? m_screenAnims
                        : m_charAnims[cmd.arg1];

                    CharAnim anim;
                    anim.type = at;
                    anim.intensity = cmd.animIntensity;
                    anim.duration = (cmd.animDuration > 0.0f)
                        ? cmd.animDuration : animDefaultDuration(at);
                    anim.elapsed = 0.0f;
                    anim.period = cmd.animPeriod;              // C-CP3 (0 = default)
                    anim.axisVertical = cmd.animAxisVertical;  // C-CP3

                    if (ownsScale(at))
                    {
                        // F2 — dueño exclusivo del canal escala. Hereda la escala
                        // COMPUESTA (no la de una anim suelta) y poda a los otros
                        // dueños: si apilaran, sus escalas se multiplicarían.
                        if (at == AnimType::ZoomOut || at == AnimType::LeanBack)
                        {
                            float actual = composeStack(stack).scale;
                            anim.fromScale = actual;
                            anim.scale = actual;
                        }
                        stack.erase(std::remove_if(stack.begin(), stack.end(),
                            [](const CharAnim& a) { return ownsScale(a.type); }),
                            stack.end());
                    }
                    else
                    {
                        // Re-emitir el MISMO tipo lo reinicia (espejo de la regla
                        // de capa_mover); un tipo DISTINTO apila. Así "flota Y
                        // pulsa a la vez" deja de ser imposible.
                        stack.erase(std::remove_if(stack.begin(), stack.end(),
                            [at](const CharAnim& a) { return a.type == at; }),
                            stack.end());
                    }

                    stack.push_back(anim);

                    // (El aviso G5 de convivencia glitch↔efectos murió en E-CP1:
                    //  ya no hay dos renders peleando por el mismo personaje.)

                    std::cout << "[VN] Animar " << cmd.arg1 << " " << cmd.arg2
                        << " (stack: " << stack.size() << ")" << std::endl;
                }
                m_cmdIndex++;
                break;
            }

            case EclipCmdType::DetenerAnimacion:
            {
                const bool esPantallaDet =
                    (cmd.arg1 == "pantalla" || cmd.arg1 == "screen");
                bool esCharVivoDet = false;
                if (!esPantallaDet)
                    for (const auto& ac : m_activeChars)
                        if (ac.alias == cmd.arg1)
                        { esCharVivoDet = true; break; }
                SceneElement* capaDet = (!esPantallaDet && !esCharVivoDet)
                    ? m_sceneLayers.get(cmd.arg1) : nullptr;

                // ── E-CP0: ahora hay DOS stacks que apagar, no uno. Sin 2º
                // argumento baja todo: presets Y efectos. Con nombre, baja solo
                // ese — y el nombre puede venir de cualquiera de los dos
                // vocabularios. (Antes, `detener_animacion Luna franja` habría
                // dicho "no conozco ese efecto", porque solo miraba AnimType.)
                const AnimType soloAnim = cmd.arg2.empty()
                    ? AnimType::None : parseAnimType(cmd.arg2);
                const FxKind   soloFx = cmd.arg2.empty()
                    ? FxKind::None : parseFxKind(cmd.arg2);

                // ── C-CP5: receta — baja el CONJUNTO por origen. Corte seco
                // (el aterrizaje de tracks espera caller, igual que el de
                // efectos): quitar la fuente deja al elemento en su reposo.
                if (!cmd.namedName.empty())
                {
                    auto podarTracks = [&cmd](std::vector<TrackInstance>& ts)
                    {
                        ts.erase(std::remove_if(ts.begin(), ts.end(),
                            [&cmd](const TrackInstance& t)
                            { return t.origen == cmd.namedName; }),
                            ts.end());
                    };
                    if (esPantallaDet) podarTracks(m_screenTracks);
                    else if (capaDet)
                        m_sceneLayers.removeTracks(cmd.arg1, cmd.namedName);
                    else
                    {
                        auto itT = m_charTracks.find(cmd.arg1);
                        if (itT != m_charTracks.end())
                        {
                            podarTracks(itT->second);
                            if (itT->second.empty()) m_charTracks.erase(itT);
                        }
                    }

                    // E-CP5: y los efectos del conjunto, por origen — el
                    // blanco vive bajo UNA grafía (background → fondo).
                    if (!esPantallaDet)
                    {
                        const std::string blancoDetN =
                            (cmd.arg1 == "background") ? "fondo" : cmd.arg1;
                        auto itfN = m_charFx.find(blancoDetN);
                        if (itfN != m_charFx.end())
                        {
                            auto& fxs = itfN->second;
                            fxs.erase(std::remove_if(fxs.begin(), fxs.end(),
                                [&cmd](const EffectInstance& e)
                                { return e.origen == cmd.namedName; }),
                                fxs.end());
                            if (fxs.empty()) m_charFx.erase(itfN);
                        }
                    }
                    m_cmdIndex++;
                    break;
                }

                // E-CP6: ¿bajar SOLO un efecto lua? (cuarta especie, resuelta
                // en parse-time como las demás).
                const bool soloLua = !cmd.luaFxName.empty();

                if (!cmd.arg2.empty() && !soloLua
                    && soloAnim == AnimType::None && soloFx == FxKind::None)
                {
                    // C-CP5: cinturón — el parser ya resuelve las especies
                    // y no deja viajar un nombre desconocido; esta voz quedó
                    // inalcanzable desde un .ecl.
                    std::cerr << "[VN] AVISO: detener_animacion " << cmd.arg1
                        << " '" << cmd.arg2 << "': no conozco ese efecto. "
                        << "Sin segundo argumento se detienen todas."
                        << std::endl;
                    m_cmdIndex++;
                    break;
                }

                // C-CP5: sin 2º argumento también bajan los tracks nombrados
                // (corte seco — el landing suave es de los presets).
                if (cmd.arg2.empty())
                {
                    if (esPantallaDet) m_screenTracks.clear();
                    else if (capaDet)  m_sceneLayers.clearTracks(cmd.arg1);
                    else               m_charTracks.erase(cmd.arg1);
                }

                // ── Presets: el camino de siempre, con su landing suave.
                // Pedir 'franja' (o un efecto lua) no los toca.
                if (soloFx == FxKind::None && !soloLua)
                {
                    std::vector<CharAnim>* stackDet = nullptr;
                    if (esPantallaDet) stackDet = &m_screenAnims;
                    else
                    {
                        auto itd = m_charAnims.find(cmd.arg1);
                        if (itd != m_charAnims.end()) stackDet = &itd->second;
                    }
                    if (stackDet)
                    {
                        // F4: sin 2º argumento baja TODO el stack (lo de siempre).
                        for (auto& a : *stackDet)
                            if (soloAnim == AnimType::None || a.type == soloAnim)
                                startLanding(a);
                    }
                }

                // ── Efectos: corte seco. El aterrizaje suave es trabajo de los
                // DRIVERS (E-CP5), que todavía no existen — y una capacidad sin
                // caller no existe (§7.118). Pedir 'glitch' no los toca.
                if (soloAnim == AnimType::None && !esPantallaDet)
                {
                    // E-CP2: el stack de `fondo` vive bajo UNA grafía.
                    const std::string blancoDet =
                        (cmd.arg1 == "background") ? "fondo" : cmd.arg1;
                    auto itf = m_charFx.find(blancoDet);
                    if (itf != m_charFx.end())
                    {
                        auto& fxStack = itf->second;
                        if (soloLua)
                            // E-CP6: por NOMBRE — dos lua comparten kind.
                            fxStack.erase(std::remove_if(fxStack.begin(), fxStack.end(),
                                [&cmd](const EffectInstance& e)
                                { return e.kind == FxKind::Lua &&
                                         e.luaName == cmd.luaFxName; }),
                                fxStack.end());
                        else if (soloFx == FxKind::None) fxStack.clear();
                        else
                            fxStack.erase(std::remove_if(fxStack.begin(), fxStack.end(),
                                [soloFx](const EffectInstance& e) { return e.kind == soloFx; }),
                                fxStack.end());
                        if (fxStack.empty()) m_charFx.erase(itf);
                    }
                }

                m_cmdIndex++;
                break;
            }

            case EclipCmdType::EsperarAnimacion:
                m_waitingForAnim = true;
                break;

            case EclipCmdType::EsperarTiempo:
                // CP1 fix: leer m_cinematicaActive DIRECTO (no via dialogBox). El
                // gate de skip del dialogBox se refresca 1×/frame ANTES del execute
                // loop; si el bloque abre con skip ya activo, ese estado queda stale
                // este frame y un esperar se colaria. El flag es la verdad fresca.
                if (m_dialogBox.isSkipMode() && !m_cinematicaActive)
                {
                    // Skip atraviesa esperar inmediatamente (Opción B).
                    m_cmdIndex++;
                }
                else
                {
                    m_waitTimer = cmd.waitDuration;
                    // NO incrementar m_cmdIndex — lo hace el update cuando termina.
                    // El guard del execute loop también para porque m_waitTimer > 0.
                }
                break;

            case EclipCmdType::IrAPasillo:
            {
                auto corridor = std::make_unique<CorridorState>(
                    m_window, m_glContext, m_stateStack, m_fontSources,
                    [this]()
                    {
                        m_cmdIndex++; // avanzar al diálogo post-pasillo
                        std::cout << "[VN] Salida del pasillo." << std::endl;
                    }
                );
                corridor->m_bgPath = cmd.arg1;
                corridor->m_npcPath = cmd.arg2;
                corridor->m_npcName = cmd.arg3.empty() ? "???" : cmd.arg3;
                // NO avanzar m_cmdIndex aquí — lo hace el callback
                m_stateStack->push(std::move(corridor));
                m_waitingForGameplay = true;   // ← frenar el execute loop
                break;
            }

            case EclipCmdType::IniciarBatalla:
            {
#if __has_include("BulletHellState.h") || __has_include(<BulletHellState.h>)
                #include "BulletHellState.h"
                // Leer HP
                int hp = 5;
                try { if (!cmd.arg2.empty()) hp = std::stoi(cmd.arg2); }
                catch (...) {}

                // Buscar portrait del personaje alias (si se definió en el script)
                std::string portraitPath;
                if (!cmd.arg1.empty())
                {
                    auto it = m_script.characters.find(cmd.arg1);
                    if (it != m_script.characters.end())
                        portraitPath = it->second;
                }

                // Crear el estado de batalla
                auto bh = std::make_unique<BulletHellState>(
                    m_window, m_glContext, m_stateStack, m_fontSources,
                    [this](bool won)
                    {
                        // Setear variable de resultado accesible desde Si/FinSi
                        m_variables["_batalla_ganada"] = won ? "1" : "0";
                        // Avanzar al comando siguiente ANTES del pop
                        m_cmdIndex++;
                        std::cout << "[VN] Batalla terminada: "
                            << (won ? "Victoria" : "Derrota") << std::endl;
                    }
                );
                bh->m_maxHp = hp;
                bh->m_portraitPath = portraitPath;

                // NO avanzar m_cmdIndex aquí — lo hace el callback al terminar
                m_stateStack->push(std::move(bh));
                m_waitingForGameplay = true;   // ← frenar el execute loop
#else
                // Sin BulletHellState.h (fuente publicada): avisa, derrota, avanza el guion.
                // No poner m_waitingForGameplay: no hay estado que lo suelte (E-30 / assert v).
                m_variables["_batalla_ganada"] = "0";
                std::cerr << "[VN] iniciar_batalla: sin BulletHellState.h; continuo\n";
                m_cmdIndex++;
#endif
                break;
            }

            case EclipCmdType::Gameplay:
            {
                std::string mapPath = cmd.arg2;

                // Resolución de ruta relativa (igual que cargar_script)
                if (!mapPath.empty() &&
                    mapPath.find('/') == std::string::npos &&
                    mapPath.find('\\') == std::string::npos)
                {
                    std::filesystem::path scriptDir =
                        std::filesystem::path(m_scriptPath).parent_path();
                    if (!scriptDir.empty())
                        mapPath = (scriptDir / mapPath).string();
                }

                auto gp = std::make_unique<Gameplay2DState>(
                    m_window, m_glContext, m_stateStack, m_fontSources, m_input,
                    [this]()
                    {
                        m_cmdIndex++;
                        std::cout << "[VN] Regreso de gameplay 2D." << std::endl;
                    }
                );
                gp->m_mapPath = mapPath;
                gp->m_sharedVars = &m_variables;
                // CP3: snapshot GP->VN (A1). Mismo carril que m_sharedVars; al
                // guardar un slot mixto el gameplay snapshotea el VN de abajo.
                gp->m_vnSaveCb = [this](int slot, const std::string& file)
                    { saveToSlot(slot, file); };
                m_stateStack->push(std::move(gp));
                m_waitingForGameplay = true;
                break;
            }

            case EclipCmdType::CargarTema:
                if (!cmd.arg1.empty())
                {
                    applyTheme(cmd.arg1);   // §1.28 + §7.26 viven dentro del helper
                    m_themeOverriddenByScript = true;  // el script gobierna → F8 no lo pisa
                }
                m_cmdIndex++;
                break;

            case EclipCmdType::Vigneta:
            {
                f32 intensity = 0.0f;
                try { intensity = std::stof(cmd.arg1); }
                catch (...) {}
                intensity = std::max(0.0f, std::min(1.0f, intensity));
                if (cmd.animDuration > 0.0f)
                    m_vignette.setTarget(intensity, cmd.animDuration);
                else
                    m_vignette.setInstant(intensity);
                m_cmdIndex++;
                break;
            }

            case EclipCmdType::Entrada:
            {
                // B2 — captura de texto. Entramos a WaitingTextInput: el
                // execute loop se frena solo (corre mientras phase==Executing).
                // NO incrementamos m_cmdIndex aquí; el avance ocurre al
                // confirmar/cancelar (en handleEvent).

                // ── SET-NO-PROTEGE-EL-GUION-BAJO, puerta 2 de 5 ──────────────
                //
                // **El rechazo va ACÁ y no en `commitTextInput`, donde está la
                // escritura**, y la diferencia la sufre el jugador: abajo el
                // teclado ya se abrió y él ya tecleó, así que negarse ahí es
                // pedirle que escriba su nombre para tirarlo. Acá el comando
                // no llega a existir.
                //
                // Es la puerta PEOR del censo, y es EclipScript puro —no Lua—:
                // `entrada _tema_activo "..."` dejaba que **el jugador** pisara
                // estado del motor, y eso viaja adentro del save.
                //
                // El `m_cmdIndex++` es obligatorio: el camino normal NO avanza
                // (avanza al confirmar/cancelar), así que sin él el guion se
                // clava en este comando para siempre.
                if (nombreConDuenio(cmd.arg1, "entrada",
                        eclips::AlmacenDeNombre::Partida))
                {
                    m_cmdIndex++;
                    break;
                }

                m_inputBuffer.clear();
                m_inputVar = cmd.arg1;                   // variable destino
                m_inputPrompt = interpolateVars(t(cmd.arg2));  // prompt con ${var}
                m_inputMaxLen = cmd.inputMaxLen;            // 0 = sin límite
                m_inputDefault = cmd.arg3;                   // fallback (vacío/cancel)
                m_inputRequired = cmd.inputRequired;         // sella confirmar-vacío
                m_phase = VNPhase::WaitingTextInput;
                SDL_StartTextInput();   // en Android: abre el teclado en pantalla
                std::cout << "[entrada] esperando texto -> var '"
                    << m_inputVar << "' | prompt: '"
                    << m_inputPrompt << "'" << std::endl;
                break;
            }

            case EclipCmdType::Unknown:
            default:
                // Ignorar comandos desconocidos
                m_cmdIndex++;
                break;
            }
        }

        // ── Resolver alias → display name del personaje ─────────────────────
        // Lookup del manifest. Si no hay manifest, o el alias no está
        // definido, o el personaje no tiene 'nombre' custom → fallback al
        // alias como display name. Narrador (alias vacío) retorna alias.
        //
        // Importante: solo se usa para LO QUE EL JUGADOR VE. El alias
        // sigue siendo la clave interna para lookups (sprites, highlight,
        // expresiones). Esto separa identidad técnica de identidad visual.
        // CP2: traduccion display-time. El store ausente conserva identidad,
        // util para estados embebidos y pruebas que no inyectan idiomas.
        std::string t(const std::string& crudo) const
        {
            return m_lang ? m_lang->translate(crudo) : crudo;
        }

        // ── El contexto de idioma de una FECHA (idiomas CP3, §4) ────────────
        //
        // Tres códigos, no uno: el store trae el activo y el original, y el
        // tema trae el knob. `formatTimestamp` sigue pura y ciega al idioma —
        // sólo recibe esto.
        eclips::LangFecha fechaCtx(const DialogBoxStyle& d) const
        {
            eclips::LangFecha ctx;
            ctx.store   = m_lang;
            ctx.nombres = d.slotDateLanguage;
            return ctx;
        }

        // La fecha de una ranura, en UN solo lugar para los dos layouts.
        //
        // El orden es la ley del addendum firmado del §4: **la plantilla
        // completa pasa por `t()` ANTES de formatear**. Traducir después sería
        // traducir una fecha ya armada — imposible de reordenar, y con el
        // filler español pegado adentro. Traduciéndola antes, un `en.lang` que
        // escriba `"{dia} de {mes_nombre}" = "{mes_nombre} {dia}"` voltea el
        // orden y borra el "de" con el mecanismo que ya existía: los tokens
        // viajan crudos dentro de la clave, igual que `${}` y `{tags}` en §2.
        //
        // Toma los dos escalares y no el `SlotWidget` entero a propósito: así
        // no depende de un tipo que se declara 2000 líneas más abajo, y la ley
        // queda al lado de `t()`, que es de quien depende de verdad.
        std::string fechaDeRanura(long long timeRaw,
                                  const std::string& timestamp,
                                  const DialogBoxStyle& d) const
        {
            if (timeRaw <= 0) return timestamp;   // save viejo sin epoch crudo
            return VNSaveMenu::formatTimestamp(
                timeRaw, t(d.slotDateFormat), fechaCtx(d));
        }

        std::string resolveSpeakerName(const std::string& alias) const
        {
            std::string visible = alias;
            if (m_manifest)
            {
                auto it = m_manifest->personajes.find(alias);
                if (it != m_manifest->personajes.end()
                    && !it->second.nombre.empty())
                    visible = it->second.nombre;
            }
            return t(visible);
        }
        // ── Resolver alias → puntero a PersonajeManifest ────────────────────
        // Paralelo a resolveSpeakerName pero retorna el manifest entero del
        // personaje. Usado por VNDialogBox.setActiveSpeaker para activar la
        // cadena de 6 niveles de velocidad de texto.
        //
        // Retorna nullptr en cualquier caso donde no haya info de personaje:
        //   - alias vacío (narrador)
        //   - sin manifest cargado (modo test sin proyecto.eclips)
        //   - alias no definido en el manifest
        // El dialog box trata nullptr como "saltar niveles 2 y 4" → cae a
        // theme + engine default.
        const PersonajeManifest* resolveSpeakerManifest(const std::string& alias) const
        {
            if (alias.empty()) return nullptr;  // Narrador
            if (!m_manifest)   return nullptr;  // Sin manifest
            auto it = m_manifest->personajes.find(alias);
            if (it == m_manifest->personajes.end()) return nullptr;  // No definido
            return &it->second;
        }

        // ── Resolver apariencia (C-ALIAS) → cadena compuesta cruda ──────────
        // Espejo de resolverImagen: el parser guarda el map, el ejecutor resuelve.
        //   1. expr vacio  -> usar defaultApariencia del personaje (si hay).
        //   2. nombre en apariencias -> expandir a la cadena compuesta.
        //   3. no encontrado -> devolver crudo (fall-through, backward-compat:
        //      "1l_2r_a" directo y las caras de dialogo "a" pasan intactas).
        // Sin manifest / char no definido / ni EXPR ni default -> expr tal cual.
        std::string resolveApariencia(const std::string& alias,
            const std::string& expr) const
        {
            const PersonajeManifest* pm = resolveSpeakerManifest(alias);
            if (!pm) return expr;

            std::string name = expr;
            if (name.empty()) name = pm->defaultApariencia;  // 1
            if (name.empty()) return expr;                   // ni EXPR ni default

            auto it = pm->apariencias.find(name);            // 2
            if (it != pm->apariencias.end()) return it->second;

            return name;                                      // 3 (crudo o default crudo)
        }

        // ── Partir una cadena UTF-8 en sus code-points ──────────────────────
        // Cada elemento = los BYTES de un code-point (no decodifica a int: los
        // preserva para re-emitir tal cual). Para el split abre/cierra de las
        // comillas de presentación (§P1). Multi-byte safe; bytes truncados o
        // inválidos degradan a slice de 1 byte (no rompe el wrap).
        static std::vector<std::string> splitCodepoints(const std::string& s)
        {
            std::vector<std::string> out;
            size_t i = 0;
            while (i < s.size())
            {
                unsigned char c = static_cast<unsigned char>(s[i]);
                size_t len = 1;
                if ((c & 0x80) == 0x00) len = 1;  // ASCII
                else if ((c & 0xE0) == 0xC0) len = 2;  // 2 bytes (incl. «»)
                else if ((c & 0xF0) == 0xE0) len = 3;  // 3 bytes (incl. '' "")
                else if ((c & 0xF8) == 0xF0) len = 4;  // 4 bytes
                if (i + len > s.size()) len = s.size() - i;  // truncado: clamp
                out.push_back(s.substr(i, len));
                i += len;
            }
            return out;
        }

        // ── Envolver diálogo en las comillas de presentación del personaje ──
        // El motor agrega comillas VISIBLES alrededor del contenido ya extraído
        // (§P1, §7.45 — cuelga del personaje, ortogonal al contenido glotón).
        //   spk nullptr / comillas vacío → sin comillas (default, narrador incl.).
        //   1 glifo → simétrico; 2 → abre/cierra; >2 → 1ro/último + aviso 1×.
        //
        // ── `HO-18` / `FO-14` (vocabulario-lote3): DÓNDE va la comilla ──────
        // La comilla de apertura va **después** de la corrida de tags con que
        // el creador abrió la línea, no antes. Los `{tags}` no son texto: son
        // instrucciones, y una de ellas —`{pausa=N}`— sólo cuenta como *de la
        // línea* si nada la precede.
        //
        // Antes de este lote la comilla se ponía al frente, y como el motor la
        // INYECTA (el creador no la escribió), convertía al primer tag en
        // mid-línea: `{pausa=N}` quedaba colgado de la comilla. Medido: 2 de
        // los 3 personajes de `project.eclips` declaran `comillas`, y las dos
        // únicas líneas del corpus que abren con tag son de uno de ellos
        // (`test_ddlc.ecl:76`, `test_ddlc_mejoras.ecl:69`) — o sea que el
        // retraso de apertura que `caja-entrada` shipeó nunca llegó a
        // funcionar ahí. La sonda no lo vio porque su hablante (`sonda`) no
        // está declarado y por eso nunca pasaba por acá.
        //
        // Lo que el jugador ve NO cambia: la comilla sigue siendo el primer
        // carácter visible (`harness_tags` §7.5/§7.6 lo clavan). Lo que cambia
        // es de qué lado del tag queda. El *dónde termina la corrida* lo
        // contesta `tags::openingTagRunEnd`, que vive en un header puro para
        // que un harness pueda probarlo sin arrastrar este archivo.
        std::string wrapWithQuotes(const std::string& text,
            const PersonajeManifest* spk) const
        {
            if (!spk || spk->comillas.empty()) return text;
            std::vector<std::string> cps = splitCodepoints(spk->comillas);
            if (cps.empty()) return text;

            std::string open, close;
            if (cps.size() == 1) { open = cps[0]; close = cps[0]; }
            else if (cps.size() == 2) { open = cps[0]; close = cps[1]; }
            else
            {
                open = cps.front();
                close = cps.back();
                static std::set<std::string> s_warned;
                if (s_warned.insert(spk->comillas).second)
                    std::cerr << "[VN] Aviso: 'comillas' del personaje '"
                    << spk->alias << "' tiene mas de 2 glifos ('"
                    << spk->comillas << "'); uso el primero para abrir y el "
                    << "ultimo para cerrar. Recomendado: exactamente 2 glifos."
                    << std::endl;
            }
            // `FO-14`: la apertura entra DESPUÉS de los tags con que el creador
            // abrió la línea. Sin tags, `corte` es 0 y esto es `open + text +
            // close` byte por byte — el mismo string de siempre.
            const std::size_t corte = tags::openingTagRunEnd(text);
            return text.substr(0, corte) + open + text.substr(corte) + close;
        }

        // ── Resolver nombre de imagen → ruta (registro imagenes del manifest) ─
        // Hermano de resolveSpeakerName, para el bucket `imagenes {}` (§13).
        // Execute-time a propósito (§proyecto.eclips v2): NO se hornea la ruta en
        // parse-time, así F8 re-resuelve contra el manifest vivo al re-ejecutar.
        //
        //   literal (venía con comillas) → devolver tal cual, sin lookup.
        //   lógico + encontrado          → ruta del manifest.
        //   lógico + NO encontrado       → "" (señal de fallo). El llamador avisa
        //                                  y mantiene el estado actual (loadBackground
        //                                  con path vacío ya hace return — §7.34).
        std::string resolverImagen(const std::string& nombre, bool esLiteral) const
        {
            if (esLiteral) return nombre;        // ruta literal: la ruta ya es la ruta
            if (nombre.empty()) return nombre;   // nada que resolver
            if (!m_manifest)   return nombre;    // modo test sin proyecto.eclips:
            // trata el nombre como ruta (paridad
            // con el comportamiento pre-feature)
            auto it = m_manifest->imagenes.find(nombre);
            if (it == m_manifest->imagenes.end())
                return "";                       // no definido → señal de fallo
            return it->second;                   // nombre lógico → ruta
        }

        // ── Saltar a escena por nombre ─────────────────────────────────────────

        void retirarAmbito(std::uint64_t scopeOwner)
        {
            if (scopeOwner == 0) return;

            // Los managers son los dueños de sus elementos y no reciben ids
            // internos desde el creador.
            m_sceneLayers.removeByOwner(scopeOwner);
            m_hotspots.removeByOwner(scopeOwner);

            // Soltar los pines que no viven dentro de SceneElement.
            for (auto it = m_layerPaths.begin(); it != m_layerPaths.end();)
            {
                const auto ownerIt = m_layerPathOwners.find(it->first);
                if (ownerIt != m_layerPathOwners.end() &&
                    ownerIt->second == scopeOwner)
                {
                    m_assets.unpin(it->second);
                    m_layerPathOwners.erase(ownerIt);
                    it = m_layerPaths.erase(it);
                }
                else
                    ++it;
            }
            for (auto it = m_layerHoverPaths.begin(); it != m_layerHoverPaths.end();)
            {
                const auto ownerIt = m_layerHoverOwners.find(it->first);
                if (ownerIt != m_layerHoverOwners.end() &&
                    ownerIt->second == scopeOwner)
                {
                    m_assets.unpin(it->second);
                    m_layerHoverOwners.erase(ownerIt);
                    it = m_layerHoverPaths.erase(it);
                }
                else
                    ++it;
            }

            m_clickMappings.erase(
                std::remove_if(m_clickMappings.begin(), m_clickMappings.end(),
                    [scopeOwner](const EclipChoice& mapping) {
                        return mapping.scopeOwner == scopeOwner;
                    }),
                m_clickMappings.end());

            m_controls.erase(
                std::remove_if(m_controls.begin(), m_controls.end(),
                    [scopeOwner](const BoundControl& control) {
                        return control.scopeOwner == scopeOwner;
                    }),
                m_controls.end());
            m_teclaWidgets.erase(
                std::remove_if(m_teclaWidgets.begin(), m_teclaWidgets.end(),
                    [scopeOwner](const TeclaWidget& widget) {
                        return widget.scopeOwner == scopeOwner;
                    }),
                m_teclaWidgets.end());
            m_slotWidgets.erase(
                std::remove_if(m_slotWidgets.begin(), m_slotWidgets.end(),
                    [scopeOwner](const SlotWidget& widget) {
                        return widget.scopeOwner == scopeOwner;
                    }),
                m_slotWidgets.end());

            for (auto it = m_slotThumbOwners.begin(); it != m_slotThumbOwners.end();)
            {
                if (it->second == scopeOwner)
                {
                    m_slotThumbs.erase(it->first);
                    it = m_slotThumbOwners.erase(it);
                }
                else
                    ++it;
            }

            // Todo el estado derivado apunta a una pantalla viva o a nada.
            m_focoPlano = -1;
            m_focoControl = -1;
            m_hoveredControl = -1;
            m_focoTecla = -1;
            m_focoDelPuntero = true;
            m_hoveredSlot = -1;
            m_draggingControl = -1;
            cerrarCaptura();

            if (m_modalScopeOwner == scopeOwner)
            {
                m_modalRequired = false;
                m_modalScopeOwner = 0;
            }
        }

        void enterAmbito(const std::string& name)
        {
            auto previous = m_scopeGenerations.find(name);
            if (previous != m_scopeGenerations.end())
                retirarAmbito(previous->second);

            if (m_nextScopeGeneration == 0)
                m_nextScopeGeneration = 1;
            const std::uint64_t owner = m_nextScopeGeneration++;
            m_scopeGenerations[name] = owner;
            m_activeScopeOwner = owner;

            std::cout << "[VNScriptState] Ambito '" << name
                << "' activo (generacion " << owner << ")" << std::endl;
        }

        // Reloj-diálogo CP1: ÚNICO punto que consume un Advance aceptado.
        // Manual, auto, skip e Interrumpir ya convergen en el evento de la caja;
        // ninguno mantiene un contador paralelo. Solo los personajes participan
        // en CP1: pantalla/fondo/capas son callers diferidos.
        void onDialogueAdvance()
        {
            bool visualChanged = false;
            for (auto& [alias, tracks] : m_charTracks)
            {
                (void)alias;
                for (auto& ti : tracks)
                {
                    AnimTrack& tr = ti.track;
                    if (!dialogueTrackPending(tr)) continue;
                    const float before = evalTrack(tr, tr.elapsed);
                    if (!advanceDialogueTrack(tr)) continue;
                    const float after = evalTrack(tr, tr.elapsed);
                    visualChanged = visualChanged || (before != after);
                }
            }

            // Reloj-diálogo CP2: la envolvente grupal de cada efecto avanza en
            // el MISMO evento. fx.elapsed (segundos) no se toca; solo el
            // progreso discreto de la envolvente. Un efecto continuo ya pinta
            // solo; el pulso es el cinturón para el caso quiescente.
            for (auto& [blancoFx, fxStack] : m_charFx)
            {
                (void)blancoFx;
                for (auto& fx : fxStack)
                {
                    if (!fx.envelope || !envelopePending(*fx.envelope)) continue;
                    const float before = evalEnvelope(*fx.envelope);
                    if (!advanceEnvelope(*fx.envelope)) continue;
                    const float after = evalEnvelope(*fx.envelope);
                    visualChanged = visualChanged || (before != after);
                }
            }

            // El latch coalesce varios tracks en una sola presentación y se
            // consume en Engine, nunca desde un render auxiliar del state.
            if (visualChanged && m_stateStack)
                m_stateStack->requestRenderOnce();
        }

        void jumpToScene(const std::string& sceneName)
        {
            // Fork E: persistir lo leído antes de cambiar de escena. flush() es
             // no-op si nada cambió (no martillea el disco). "Una escena leída =
             // una escritura."
            if (m_seen) m_seen->flush();

            // Defensivo: si por algún camino futuro salimos a mitad de una
            // entrada de texto, apagar el teclado/IME. No-op si no estaba activo.
            if (m_phase == VNPhase::WaitingTextInput) SDL_StopTextInput();

            if (m_script.scenes.find(sceneName) == m_script.scenes.end())
            {
                std::cerr << "[VNScriptState] ERROR: escena destino '"
                    << sceneName << "' no existe." << std::endl;
               m_stateStack->cmdPop(this);
                return;
            }

            // Un salto válido abandona la adscripción actual, pero no retira
            // ninguna generación: la página vive hasta su reentrada homónima.
            m_activeScopeOwner = 0;
            std::cout << "[VNScriptState] -> Escena: '" << sceneName << "'" << std::endl;

            // Random events — el salto explícito toma el volante: quien salta,
            // rutea. TODO salto de guion (`->`, elegir, click ruteado, QTE,
            // Lua) pasa por aquí y cancela el auto-retorno pendiente; sin
            // esto, un salto a mitad de historia teletransportaría al jugador
            // a un ancla vieja. El tick arma su ancla DESPUÉS de llamar aquí,
            // y el retorno mismo no pasa por aquí (consume, no salta).
            m_preEventScene.clear();
            m_preEventCmd = 0;

            m_waitTimer = 0.0f;
            m_waitingForAnim = false;
            m_modalRequired = false;   // modalidad-aviso: nueva escena, sin modal colgado
            m_modalScopeOwner = 0;
            // `CP2a`: un pool colgado es PEOR que un modal colgado — sus capas
            // no se van con la escena, así que quedarían clickeables encima de
            // la escena nueva. Va acá y no sólo en el `Salto` del pick porque
            // ésta es la puerta ÚNICA de los saltos (`ir_a`, un `elegir`, Lua).
            limpiarPalabras();
            m_palabrasIds.clear();
            m_palabrasActivas = false;
            m_cinematicaActive = false; // CP1: nueva escena, sin sello colgado
            resetSelloAcciones();       // `D-43` corte 1: nueva escena
            m_currentScene = sceneName;
            m_cmdIndex = 0;
            m_phase = VNPhase::Executing;
            callLuaHook("on_scene_enter", sceneName);
        }

        // ── Retorno de evento random: RE-EJECUCIÓN, no restauración ─────────
        // El ancla (escena, cmd) apunta al comando de espera que estaba VIVO
        // al disparar (en WaitingClick, m_cmdIndex apunta al esperar_click
        // mismo — la misma ley que usa el refresco del menú en onResume).
        // Volver en fase Executing re-ejecuta ese comando: re-evalúa sus
        // [si:] y re-arma mappings/hotspots FRESCOS. Restaurar la fase
        // directamente habría conservado los mappings del EVENTO en la espera
        // original — un bug de mappings imposible por construcción así.
        // No llama on_scene_enter (es un resume a mitad de escena, no una
        // entrada; el precedente de onResume tampoco lo llama) y no pasa por
        // jumpToScene (que resetea cmd a 0 y borra anclas por la ley del
        // volante — este salto CONSUME el ancla, no rutea).
        void returnFromEvent()
        {
            const std::string scene = m_preEventScene;
            const int cmd = m_preEventCmd;
            m_preEventScene.clear();
            m_preEventCmd = 0;

            if (m_script.scenes.find(scene) == m_script.scenes.end())
            {
                // Un encadenamiento de scripts pudo dejar el ancla huérfana:
                // degradar CON VOZ al comportamiento de siempre (fin del VN).
                std::cerr << "[VNScriptState] retorno de evento: la escena '"
                    << scene << "' ya no existe. Fin del guion." << std::endl;
                m_stateStack->cmdPop(this);
                return;
            }

            if (m_seen) m_seen->flush();
            std::cout << "[VNScriptState] <- Retorno de evento: '" << scene
                << "' cmd " << cmd << std::endl;
            m_waitTimer = 0.0f;
            m_waitingForAnim = false;
            m_modalRequired = false;   // higiene de salida, espejo de jumpToScene
            m_modalScopeOwner = 0;
            limpiarPalabras();         // `CP2a`: espejo exacto de jumpToScene
            m_palabrasIds.clear();
            m_palabrasActivas = false;
            m_cinematicaActive = false;
            resetSelloAcciones();      // espejo exacto de jumpToScene (ver ficha)
            m_currentScene = scene;
            m_cmdIndex = cmd;
            m_phase = VNPhase::Executing;
        }

        // ── Átomo clickable: dispatcher unificado de destino ────────────────
        //
        // Punto único de unificación. Tanto la costura de choice (elegir, en
        // update) como la de hotspot (esperar_click, en handleEvent) rutean por
        // aquí. Resuelve la derecha del '->' con precedencia escena-gana:
        //   1. ¿hay escena con ese nombre? → navegar (jumpToScene).
        //   2. ¿es verbo shell reservado?   → helper shell (modal, solo push).
        //   3. else → error "destino desconocido".
        // La escena real SIEMPRE gana → agregar verbos nunca tapa una escena
        // real del creador (backward-compat-seguro, Fork B).
        //
        // Verbos shell (bilingües): guardar/cargar/menu/historial/config abren un
        // overlay; salir empuja SDL_QUIT (cierra la app, no toca el stack).
        // Pushear overlay desde handleEvent es seguro (StateStack.push es
        // inmediato; solo el pop difiere, y lo difiere el stack por su cuenta).
        // ── CP0 `mutar`: el dispatcher DEVUELVE QUÉ HIZO (fork 1a) ──────────
        //
        // Devolvía `void`, y sus cuatro call-sites asumían "saltó o falló": el
        // choice hace `break` sin tocar fase ni índice, el hotspot y los dos
        // enlaces hacen `return true` (consumir). Esa suposición es correcta
        // mientras el único desenlace posible sea un salto — y deja de serlo
        // con una mutación, que no salta a ningún lado: sin veredicto, la
        // elección queda clavada en `WaitingChoice` PARA SIEMPRE.
        //
        // El fork 1b (que el dispatcher resolviera la espera él mismo, mirando
        // la fase) se descartó MEDIDO: para el enlace, "continuar" significa NO
        // CONSUMIR —el `return true/false` es del caller, no de acá (ver el
        // comentario de `:1204`)—, así que hay un caso que no puede expresar.
        void aplicarRetornoPantalla(eclips::system_flow::RegresoEfecto efecto)
        {
            switch (efecto)
            {
            case eclips::system_flow::RegresoEfecto::Enlace:
                if (m_seen && m_currentLineKey != 0)
                    m_seen->mark(m_currentLineKey);
                onDialogueAdvance();
                ++m_cmdIndex;
                m_phase = VNPhase::Executing;
                break;

            case eclips::system_flow::RegresoEfecto::EsperarClick:
                ++m_cmdIndex;
                m_phase = VNPhase::Executing;
                break;

            case eclips::system_flow::RegresoEfecto::Eleccion:
                m_dialogBox.endChoices();
                m_currentChoices.clear();
                ++m_cmdIndex;
                m_phase = VNPhase::Executing;
                break;

            case eclips::system_flow::RegresoEfecto::Palabras:
                resolverPalabras();
                break;

            case eclips::system_flow::RegresoEfecto::NoOp:
                break;
            }
        }

        bool continuacionPantallaDisponible(
            eclips::system_flow::PantallaRetorno politica,
            eclips::system_flow::PantallaCara cara) const
        {
            if (politica != eclips::system_flow::PantallaRetorno::Continuar)
                return true;
            switch (cara)
            {
            case eclips::system_flow::PantallaCara::Enlace:
                return m_phase == VNPhase::WaitingInput;
            case eclips::system_flow::PantallaCara::EsperarClick:
                return m_phase == VNPhase::WaitingClick;
            case eclips::system_flow::PantallaCara::Eleccion:
                return m_phase == VNPhase::WaitingChoice;
            case eclips::system_flow::PantallaCara::Palabras:
                return m_phase == VNPhase::WaitingInput && m_palabrasActivas;
            }
            return false;
        }

        eclips::TargetVeredicto dispatchTarget(
            const std::string& target,
            eclips::system_flow::PantallaCara cara)
        {
            // 0. ¿Es una MUTACIÓN? La FORMA decide, y por eso se pregunta
            //    ANTES que nada: una escena y un verbo son siempre 1 token, así
            //    que un destino con operador no puede ser ninguno de los dos.
            //    (Medido: 479 declaraciones de escena en el corpus, cero con
            //    nombre multi-token. La ley y su frontera viven en
            //    `VNMutateTarget.h`, donde un harness las puede probar.)
            {
                const eclips::MutaTriple m = eclips::mutaClasificarTarget(target);
                if (m.forma == eclips::MutaForma::Valida)
                {
                    // F1: la rama llama al MISMO `executeSet` del comando, así
                    // que el hook Lua `on_variable_changed` viaja gratis (A2).
                    EclipCmd cmd;
                    cmd.type = EclipCmdType::Set;
                    cmd.arg1 = m.var;
                    cmd.arg2 = m.op;
                    cmd.arg3 = m.valor;
                    executeSet(cmd);
                    return eclips::TargetVeredicto::Mutacion;
                }
                if (m.forma == eclips::MutaForma::Rota)
                {
                    // Tenía forma de mutación y no lo era. Habla con su propia
                    // voz en vez de caer al error genérico de destino: ahí el
                    // creador leería "no es una escena" sobre algo que nunca
                    // quiso que fuera una escena.
                    std::cerr << "[VNScriptState] destino '" << target
                        << "': parece una mutacion pero le falta el operando. "
                        "Usa '-> var = valor', '-> var + n' o '-> var - n'."
                        << std::endl;
                    return eclips::TargetVeredicto::MutacionRota;
                }
            }

            const eclips::system_flow::ParseDestinoResultado pantalla =
                eclips::system_flow::parseDestinoPantalla(target);
            if (!pantalla.ok)
            {
                std::cerr << "[VNScriptState] llamada a pantalla '" << target
                    << "' invalida: " << pantalla.detalle << std::endl;
                return eclips::TargetVeredicto::NoResuelto;
            }

            const std::string& nombrePantalla = pantalla.destino.nombre;
            auto abrirPantallaLibre = [&](const std::string& lookup,
                                          bool explicita)
                -> eclips::TargetVeredicto
            {
                if (!m_manifest)
                {
                    std::cerr << "[VNScriptState] pantalla libre '" << lookup
                        << "' requiere proyecto.eclips; no se abre." << std::endl;
                    return eclips::TargetVeredicto::NoResuelto;
                }

                // Esta es la puerta generica: no inventa callers nuevos y no
                // confunde el mapa de nativas con la tabla de destinos.
                auto it = m_manifest->pantallas.find(target);
                if (lookup != target || it == m_manifest->pantallas.end())
                    it = m_manifest->pantallas.find(lookup);
                if (it == m_manifest->pantallas.end()
                    || !eclips::system_flow::esNombrePantallaLibre(lookup))
                {
                    if (explicita)
                        std::cerr << "[VNScriptState] destino '" << target
                            << "' con politica de regreso no es una pantalla libre"
                            << " registrada; no hay fall-through." << std::endl;
                    return eclips::TargetVeredicto::NoResuelto;
                }

                if (!continuacionPantallaDisponible(pantalla.destino.politica, cara))
                {
                    std::cerr << "[VNScriptState] pantalla '" << lookup
                        << "': continuar no esta disponible en esta fase; no se abre."
                        << std::endl;
                    return eclips::TargetVeredicto::NoResuelto;
                }

                const std::string resolved =
                    eclips::resolveScriptPath(it->second, m_manifest);
                if (resolved.empty())
                {
                    std::cerr << "[VN pantalla] flujo libre '" << lookup
                        << "' no existe: '" << it->second
                        << "'. Origen intacto; no se abre." << std::endl;
                    return eclips::TargetVeredicto::PantallaSistema;
                }

                auto ticket = std::make_shared<eclips::system_flow::TicketRetorno>();
                ticket->politica = pantalla.destino.politica;
                ticket->cara = cara;
                m_llamadaPendiente = ticket;

                VNScriptState* juegoCubierto = nullptr;
                if (m_role == VNRole::Game) juegoCubierto = this;
                else if (m_role == VNRole::Menu) juegoCubierto = m_pausedGame;

                m_stateStack->cmdPush(std::make_unique<VNScriptState>(
                    m_window, m_glContext, m_stateStack, m_fontSources,
                    m_input, m_lang,
                    resolved, -1, m_manifest, m_meta, m_seen, VNRole::Menu,
                    juegoCubierto, ticket));
                return eclips::TargetVeredicto::PantallaSistema;
            };

            // Una politica explícita desambigua la llamada antes de escena,
            // shell o error. Una pantalla reservada no obtiene un escape.
            if (pantalla.destino.politicaExplicita)
                return abrirPantallaLibre(nombrePantalla, true);

            // 1. Escena: gana siempre.
            if (m_script.scenes.find(target) != m_script.scenes.end())
            {
                if (m_manifest
                    && m_manifest->pantallas.find(target) != m_manifest->pantallas.end()
                    && eclips::system_flow::esNombrePantallaLibre(target))
                {
                    std::cerr << "[VNScriptState] colision pantalla/escena '"
                        << target << "': gana la escena local; usa politica explicita"
                        << " para abrir el flujo registrado." << std::endl;
                }
                jumpToScene(target);
                return eclips::TargetVeredicto::Salto;
            }

            // ── PUERTA C DEL SELLO: los verbos shell ─────────────────────────
            //
            // **Ésta es la puerta que el contrato no vio, y donde `D-44` chocó
            // con el código.** `D-44` dice *«si el creador escribe `guardar` en
            // su `.ecl`, eso corre aunque `menu_guardar` esté deshabilitado»*,
            // y su miedo era que el creador se apagara su propio comando. Pero
            // **no existe un comando `guardar`**: no está en `esComandoDelMotor`
            // ni tiene `EclipCmdType`. La ÚNICA forma de que `guardar` aparezca
            // en un `.ecl` es como destino de un `enlace`, un hotspot o una
            // opción de `elegir` — o sea, de algo que el JUGADOR clickea, que
            // es exactamente lo que este sello existe para apagar.
            //
            // Firmado por el fundador (fork 1B): la puerta se sella **y habla**.
            // Su miedo lo paga la voz, no la exención — y bajo la lectura
            // literal el sello tenía un agujero justo donde el creador construyó
            // la superficie, que además se ve idéntico en pantalla.
            //
            // Los que NO se sellan, y por qué: `salir`, `titulo`, `fin`,
            // `nuevo_juego` y `continuar` son NAVEGACIÓN Y SALIDA, no servicios
            // — familia Salida, y `D-46` los protege.
            if (target == "guardar" || target == "save") {
                if (!selloVeta(eclips::VNAction::MenuGuardar))
                    openSaveMenu(VNSaveMenu::Mode::Save);
                return eclips::TargetVeredicto::VerboShell;
            }
            if (target == "cargar" || target == "load") {
                if (!selloVeta(eclips::VNAction::MenuCargar))
                    openSaveMenu(VNSaveMenu::Mode::Load);
                return eclips::TargetVeredicto::VerboShell;
            }
            if (target == "menu") {
                if (!selloVeta(eclips::VNAction::Menu)) openPauseMenu();
                return eclips::TargetVeredicto::VerboShell;
            }
            if (target == "historial" || target == "history") {
                if (!selloVeta(eclips::VNAction::Historial)) openHistory();
                return eclips::TargetVeredicto::VerboShell;
            }
            if (target == "config" || target == "settings") {
                if (!selloVeta(eclips::VNAction::Config)) openSettings();
                return eclips::TargetVeredicto::VerboShell;
            }
            if (target == "salir" || target == "quit") { quitApp();      return eclips::TargetVeredicto::VerboShell; }
            // Verbos de ciclo de vida (CP5 Ciclo-de-Vida-B):
            if (target == "nuevo_juego" || target == "new_game") { startNewGame();                  return eclips::TargetVeredicto::VerboShell; }
            if (target == "continuar" || target == "continue") { continueGame();                   return eclips::TargetVeredicto::VerboShell; }
            if (target == "titulo" || target == "title") { m_stateStack->cmdPopToBottom();  return eclips::TargetVeredicto::VerboShell; }
            if (target == "fin" || target == "end") { cerrarPantalla();                 return eclips::TargetVeredicto::VerboShell; }

            // 5. Pantalla libre registrada: la forma simple conserva Reponer.
            if (m_manifest && m_manifest->pantallas.find(target)
                != m_manifest->pantallas.end())
                return abrirPantallaLibre(target, false);

            // 3. Ni escena ni verbo. No crashea: la escena no cambia, el
            //    clickable simplemente no hace nada (error pedagógico en consola).
            std::cerr << "[VNScriptState] ERROR: destino '" << target
                << "' no es una escena ni un verbo shell reservado." << std::endl;
            return eclips::TargetVeredicto::NoResuelto;
        }

        // Abre el menú de pausa. Factorizado de los dos sitios que lo pusheaban
        // idéntico (ESC en handleEvent + Event::OpenMenu en update); ahora el
        // dispatcher del átomo lo reusa para el verbo `menu`.
        void openPauseMenu()
        {
            // §3.3 guard de cinturón: un menú no se pausa a sí mismo. Cubre ESC,
            // el verbo `menu`, y cualquier botón de chrome que llegue aquí.
            if (m_role == VNRole::Menu)
            {
                std::cerr << "[VN rol Menu] pausa suprimida: un menu no se pausa "
                    "a si mismo (ESC/`menu` no aplican en rol Menu).\n";
                return;
            }

            // Ola 3 · Slice A: override autorable de la pantalla de pausa. Gemelo
            // exacto de openSettings: ¿el creador registró 'pausa'/'pause'?
            //   sí → cmdPush su .ecl en rol Menu (no se auto-pausa; su `fin`/`titulo`
            //        revelan el juego de abajo = Reanudar/Menú principal; `guardar`
            //        suprimido → guardar-desde-pausa es Slice B). cmdPush, NO push
            //        inmediato: diferir el VNScriptState para drenar en orden (D1).
            //   no / ruta rota → fallback exacto al PauseMenuState C++ (§7.118:
            //        roto != desaparecido).
            if (m_manifest)
            {
                auto it = m_manifest->pantallas.find("pausa");
                if (it == m_manifest->pantallas.end())
                    it = m_manifest->pantallas.find("pause");
                if (it != m_manifest->pantallas.end() && !it->second.empty())
                {
                    std::string resolved =
                        eclips::resolveScriptPath(it->second, m_manifest);
                    if (!resolved.empty())
                    {
                        m_stateStack->cmdPush(std::make_unique<VNScriptState>(
                            m_window, m_glContext, m_stateStack, m_fontSources,
                            m_input, m_lang,
                            resolved, -1, m_manifest, m_meta, m_seen, VNRole::Menu,
                            this   // Slice B: el juego que esta pausa cubre
                        ));
                        return;
                    }
                    std::cerr << "[VN pausa] Override '" << it->second
                        << "' no existe; uso el PauseMenuState por defecto.\n";
                }
            }

            // ── LA CUARTA PUERTA: los callbacks del PauseMenuState C++ ───────
            //
            // **La encontró el audit por EFECTO que §7.462 manda hacer**, ya con
            // el lote cableado y verde: grepear `openSaveMenu`/`openSettings`
            // devuelve estos tres, y ninguno pasa por las otras tres puertas.
            //
            // Bajo el grueso el agujero no se ve —`deshabilitar acciones` sella
            // `menu`, así que esta pantalla ni se abre—, y por eso ni el contrato
            // ni la sonda lo tocaban. **Aparece con la forma fina**, que es un
            // caso legítimo del creador: *«podés pausar, pero no guardar»*
            // (`deshabilitar menu_guardar` dejando `menu` vivo). Sin estas tres
            // líneas, el botón Guardar de la pausa seguía guardando.
            //
            // El «Menú principal» (`cmdPopToBottom`) NO se sella: es navegación
            // y salida, la misma familia que `titulo`/`fin`.
            m_stateStack->push(std::make_unique<PauseMenuState>(
                m_window, m_stateStack, m_fontSources,
                [this] { if (!selloVeta(eclips::VNAction::MenuGuardar))
                             openSaveMenu(VNSaveMenu::Mode::Save); },
                [this] { if (!selloVeta(eclips::VNAction::MenuCargar))
                             openSaveMenu(VNSaveMenu::Mode::Load); },
                [this] { m_stateStack->cmdPopToBottom(); },
                nullptr,    // onReturnToVN: no aplica desde VN
                & m_theme,   // theme actual → SettingsState lo conoce
                [this] { if (!selloVeta(eclips::VNAction::Config))
                             openSettings(); }   // Fork C: respeta override
            ));
        }

        // Abre el backlog/historial (overlay interno de VNDialogBox, no es push
        // de state). Misma acción que el botón de chrome HISTORIAL y el atajo de
        // teclado. Historial vacío → no abre (guard interno de la caja).
        void openHistory()
        {
            m_dialogBox.handleAction(InputAction::HistoryOpen);
        }

        // Abre Configuración (overlay modal). Mismo push que hace PauseMenu, pero
        // directo desde el VN para el verbo `config`. SettingsState conoce el
        // theme actual (&m_theme) para renderizar consistente.
        void openSettings()
        {
            // β menú · CP2: ¿el creador registró un override de 'config'?
            //   sí → push su .ecl como pantalla en rol Menu. Va por cmdPush (la
            //        cola del ciclo), NO push inmediato: pushear un VNScriptState
            //        desde dentro del dispatch debe diferirse, igual que
            //        startNewGame/loadGameFromMenu. Rol Menu = no se auto-pausa,
            //        no ofrece guardar; el 'volver'→fin la popea y regresa al
            //        juego de abajo. slot -1 = sin partida (es una pantalla).
            //   no / ruta rota → fallback exacto al SettingsState C++ (el creador
            //        nunca se queda sin config; §7.118: roto != desaparecido).
            if (m_manifest)
            {
                auto it = m_manifest->pantallas.find("config");
                if (it != m_manifest->pantallas.end() && !it->second.empty())
                {
                    std::string resolved =
                        eclips::resolveScriptPath(it->second, m_manifest);
                    if (!resolved.empty())
                    {
                        m_stateStack->cmdPush(std::make_unique<VNScriptState>(
                            m_window, m_glContext, m_stateStack, m_fontSources,
                            m_input, m_lang,
                            resolved, -1, m_manifest, m_meta, m_seen, VNRole::Menu
                        ));
                        return;
                    }
                    std::cerr << "[VN config] Override '" << it->second
                        << "' no existe; uso el SettingsState por defecto.\n";
                }
            }

            m_stateStack->push(std::make_unique<SettingsState>(
                m_window, m_stateStack, m_fontSources, &m_theme));
        }

        // Cierra la app limpiamente: empuja SDL_QUIT, que el loop del Engine
        // recoge (→ m_running = false). Mismo patron que el Quit del menu de
        // pausa. NO toca el StateStack (no es overlay).
        void quitApp()
        {
            SDL_Event quit{};
            quit.type = SDL_QUIT;
            SDL_PushEvent(&quit);
        }

        // ── Cargar un nuevo script .ecl (encadenamiento de scripts) ─────────
        //
        // Hot-swap del guión: reemplaza las escenas pero preserva todo el
        // estado visual y lógico (variables, personajes, fondo, música,
        // capas, hotspots, eventos, historial).
        //
        // Si targetScene está vacío, salta a la primera escena del nuevo script.
        //
        // ── `D-33`: LA LÍNEA NO PARA ────────────────────────────────────────
        //
        // El `case Dialogo` termina en `WaitingInput` y **no toca el índice**:
        // el índice se mueve recién en el `Advance` (`VNScriptState.h:1441`).
        // Por eso las opciones estaban del otro lado de un click — el defecto
        // que el fundador cazó escribiendo la feature en su propio archivo,
        // *«tengo que dar dos clicks»*.
        //
        // Acá la línea hace lo que haría el `Advance`, MENOS el avance mismo:
        //
        //   · **marca la línea como leída.** Sin esto quedaría "no vista" para
        //     siempre —el único sitio que marca es el `Advance`— y el skip se
        //     frenaría en ella en CADA corrida;
        //   · mueve el índice y vuelve a `Executing`, así que el `elegir` que
        //     viene pegado cae en el **mismo `update()`**: el batch de comandos
        //     instantáneos corre hasta 64 por frame y sólo para en una fase de
        //     espera;
        //   · pide que la caja sobreviva, porque unirse a una línea implica que
        //     se ve (si no, el renglón no significaría nada).
        //
        // **Lo que NO hace, a propósito: `onDialogueAdvance()`.** Esas pistas
        // avanzan cuando el jugador DEJA la línea, y en la forma pegada la
        // sigue leyendo mientras elige: moverlas acá cambiaría el dibujo justo
        // cuando el caption aparece. La marca de leída es otra cosa —dice "esta
        // línea se mostró"— y por eso sí va.
        void fusionarConEleccion(bool tagInterrumpir)
        {
            if (m_seen) m_seen->mark(m_currentLineKey);
            m_dialogBox.keepForChoice();
            m_cmdIndex++;
            m_phase = VNPhase::Executing;

            // `D-38`: `{interrumpir}` pide un avance automático al completar el
            // typewriter, y `D-37` acaba de prohibir el avance durante la
            // elección. Queda inerte — y lo que no aplica se dice.
            if (tagInterrumpir)
                std::cerr << "[VNScriptState] AVISO: {interrumpir} en una linea "
                    "con el 'elegir' pegado no hace nada: esa linea no avanza "
                    "sola, se resuelve eligiendo una opcion." << std::endl;
        }

        // ── La narración, en un solo lugar ──────────────────────────────────
        // La comparten `narrador` (el guion la escribió) y el bloque `azar:`
        // (el sorteo la eligió). `textoCrudo` es el texto ANTES de interpolar:
        // la clave de línea se calcula sobre él, igual que hacía el case de
        // `Narrador`, para que el marcado de "ya leída" no dependa del valor
        // que tenían las variables esa vez.
        // `fusionaConEleccion` (`D-33`) lo pone SOLO el `case Narrador`: el
        // bloque `azar:` sortea texto, nunca un renglón pegado.
        void mostrarNarracion(const std::string& textoCrudo,
                              bool fusionaConEleccion = false)
        {
            m_currentSpeaker = "";
            std::string shown = interpolateVars(t(textoCrudo));
            // Sin info de personaje. Reseteamos explícitamente para que el
            // dialog box no use el speaker del diálogo previo.
            m_dialogBox.setActiveSpeaker(nullptr);
            auto rich = eclips::TextRenderer::parseRichText(shown);
            ligarFuentesRichText(rich);
            // Historial y preview = texto visible (sin {tags}); ver Dialogo.
            m_lastDialogue = rich.displayText;
            m_dialogBox.startDialogue("", rich);
            m_phase = VNPhase::WaitingInput;
            m_dialogBox.pushHistory("", rich.displayText);
            // C2: clave de ESTA línea de narración. Se marca al avanzar.
            m_currentLineKey = computeLineKey("", textoCrudo);
            // `D-33`: la clave tiene que estar calculada ANTES (la marca de
            // leída la consume), y el índice todavía sin mover.
            if (fusionaConEleccion) fusionarConEleccion(rich.interrupts);
        }

        void loadNewScript(std::string path, std::string targetScene)
        {
            // Fork E: persistir lo leído antes de cambiar de script (este camino
             // hace su propio salto inline, no pasa por jumpToScene). No-op si
             // nada cambió.
            if (m_seen) m_seen->flush();

            // Defensivo: si salíamos a mitad de una entrada de texto, apagar
            // el teclado/IME. No-op si no estaba activo.
            if (m_phase == VNPhase::WaitingTextInput) SDL_StopTextInput();

            // path y targetScene se reciben POR VALOR (copias).
            // Esto es intencional: las referencias originales (cmd.arg1/arg2)
            // viven dentro de m_script.scenes, que reemplazamos más adelante.
            // Sin la copia → use-after-free → crash.

            // ── Resolver ruta relativa ───────────────────────────────────
            // Si la ruta no tiene separador de directorio, es relativa al
            // script actual. Ej: si estamos en "vnscript/acto1.ecl" y el
            // comando dice cargar_script "acto2.ecl", buscamos
            // "vnscript/acto2.ecl".
            std::string resolvedPath = path;
            if (path.find('/') == std::string::npos &&
                path.find('\\') == std::string::npos)
            {
                // Extraer directorio del script actual
                size_t lastSep = m_scriptPath.find_last_of("/\\");
                if (lastSep != std::string::npos)
                {
                    resolvedPath = m_scriptPath.substr(0, lastSep + 1) + path;
                }
            }

            std::cout << "[VNScriptState] === Encadenando script: '"
                << resolvedPath << "' ===" << std::endl;

            // Parsear el nuevo script
            EclipScriptData newScript = EclipParser::parse(resolvedPath, m_manifest);
            if (!newScript.valid)
            {
                std::cerr << "[VNScriptState] ERROR: script '"
                    << resolvedPath << "' invalido: " << newScript.error << std::endl;
                // No romper el juego — seguir con el script actual
                m_cmdIndex++;
                return;
            }

            // ── Merge de personajes ──────────────────────────────────────
             // Helper único (paso 2): characters + compositeChars + el set de
             // procedencia, sincronizados de una. Antes esto se duplicaba acá y
             // en loadFromSlot — gemelos a mano, riesgo de desincronizar el set.
            m_script.mergeCharactersFrom(newScript);

            // ── Reemplazar escenas ───────────────────────────────────────
            // Las escenas viejas se descartan — el nuevo script es ahora
            // el universo activo. Esto evita colisiones de nombres
            // (dos actos pueden tener "inicio") y libera memoria.
            m_script.scenes = std::move(newScript.scenes);
            m_script.firstScene = newScript.firstScene;
            // CP-R3: las declaraciones de fuente son del ARCHIVO, así que se
            // reemplazan con él. Si se acumularan, el catálogo local de un
            // script viejo seguiría resolviendo nombres bajo el ámbito nuevo.
            m_script.fuentes = std::move(newScript.fuentes);

            // Actualizar la ruta del script activo
            m_scriptPath = resolvedPath;

            // CP-R3: el catálogo del archivo nuevo, antes de su primer comando.
            // Las capas, tramos Rich Text y consolas que sobreviven al
            // encadenado conservan la identidad del archivo que las creó — no
            // se re-resuelven por nombre.
            materializarCatalogoFuentes();

            // E-CP6: las declaraciones `efecto_lua` del script nuevo entran
            // al censo (override por nombre, como los personajes) y sus .lua
            // corren YA — el registro debe existir antes de cualquier
            // `animar` del script encadenado. (Rutas relativas al script
            // NUEVO: por eso esto va después de actualizar m_scriptPath.)
            mergeLuaEffectDecls(newScript.luaEffects);
            loadDeclaredLuaEffects(newScript.luaEffects);

            // ── Saltar a la escena destino ───────────────────────────────
            std::string dest = targetScene.empty()
                ? m_script.firstScene
                : targetScene;

            if (m_script.scenes.find(dest) == m_script.scenes.end())
            {
                std::cerr << "[VNScriptState] ERROR: escena destino '"
                    << dest << "' no existe en '" << resolvedPath << "'"
                    << std::endl;
                // Fallback: usar la primera escena del nuevo script
                dest = m_script.firstScene;
            }

            std::cout << "[VNScriptState] Script cargado OK ("
                << m_script.scenes.size() << " escenas). "
                << "-> Escena: '" << dest << "'" << std::endl;

            m_waitTimer = 0.0f;
            m_waitingForAnim = false;
            m_dialogBox.setBoxOverride(""); // reset al cargar nuevo script
            m_cinematicaActive = false;     // CP1: script nuevo, sin sello colgado
            resetSelloAcciones();           // `D-43` corte 2: cargar_script
            // Ley 7: el sorteo se limpia al cargar script. Y acá además hace
            // falta por identidad: la clave de la bolsa es escena+línea, y en
            // un script nuevo esa misma coordenada es OTRO bloque.
            m_azarBolsas.clear();
            m_palabrasIds.clear();      // `CP2a`: misma vida útil que la bolsa
            m_palabrasActivas = false;
            m_currentScene = dest;
            m_cmdIndex = 0;
            m_phase = VNPhase::Executing;
        }

        // ════════════════════════════════════════════════════════════════════
        // Lua Bindings — Expone funciones del motor a scripts Lua
        // ════════════════════════════════════════════════════════════════════
        //
        // Registra la tabla global "eclips" con funciones que Lua puede llamar:
        //
        //   eclips.set("var", "val")        — asignar variable
        //   eclips.get("var")               — leer variable (retorna string)
        //   eclips.add("var", n)            — incrementar variable numérica
        //   eclips.sub("var", n)            — decrementar variable numérica
        //   eclips.show("alias", "pos")     — mostrar personaje (pos opcional)
        //   eclips.hide("alias")            — ocultar personaje
        //   eclips.expression("alias","exp")— cambiar expresión
        //   eclips.background("ruta")       — cambiar fondo
        //   eclips.music("ruta")            — reproducir música (loop)
        //   eclips.sound("ruta")            — reproducir efecto de sonido
        //   eclips.stop_music()             — detener música
        //   eclips.notify("msg")            — toast de notificación
        //   eclips.goto("escena")           — saltar a escena (diferido)
        //   eclips.load_script("r", "esc")  — cargar script (diferido)
        //   eclips.scene()                  — nombre de escena actual
        //   eclips.char_count()             — personajes activos en pantalla
        //
        // ════════════════════════════════════════════════════════════════════

        void registerLuaBindings()
        {
            sol::state& lua = m_lua.state();

            // Crear tabla "eclips"
            sol::table eclips = lua.create_named_table("eclips");

            // ── Variables ──────────────────────────────────────────────────

            // ── SET-NO-PROTEGE-EL-GUION-BAJO, puertas 3-5 de 5 ──────────────
            //
            // Los tres verbos escriben a `m_variables` con un nombre que viene
            // del creador, igual que `set` — y hasta este lote entraban **sin
            // mirar nada**. La ley del guion se podía esquivar escribiendo la
            // misma línea en Lua, que llega al mismo mapa.
            //
            // La voz los nombra con su prefijo `eclips.` porque el creador que
            // los escribió no tiene un `set` en su `.ecl` que ir a buscar.
            eclips.set_function("set", [this](const std::string& var,
                const std::string& val) {
                    if (nombreConDuenio(var, "eclips.set",
                        eclips::AlmacenDeNombre::Partida)) return;
                    m_variables[var] = val;
                    std::cout << "[Lua] eclips.set('" << var << "', '"
                        << val << "')" << std::endl;
                });

            eclips.set_function("get", [this](const std::string& var)
                -> std::string {
                    // DI-8: Lua gana las variables de sistema, y SOLO ellas.
                    // Delegar el resto a resolveVariable abriría MetaStore de
                    // rebote; los nombres sin `_` conservan la API de hoy.
                    if (!var.empty() && var[0] == '_')
                        return resolveVariable(var);
                    auto it = m_variables.find(var);
                    if (it != m_variables.end())
                        return it->second;
                    return "";
                });

            eclips.set_function("add", [this](const std::string& var, int n) {
                if (nombreConDuenio(var, "eclips.add",
                    eclips::AlmacenDeNombre::Partida)) return;
                int current = 0;
                auto it = m_variables.find(var);
                if (it != m_variables.end())
                {
                    try { current = std::stoi(it->second); }
                    catch (...) { current = 0; }
                }
                current += n;
                m_variables[var] = std::to_string(current);
                std::cout << "[Lua] eclips.add('" << var << "', " << n
                    << ") = " << current << std::endl;
                });

            eclips.set_function("sub", [this](const std::string& var, int n) {
                if (nombreConDuenio(var, "eclips.sub",
                    eclips::AlmacenDeNombre::Partida)) return;
                int current = 0;
                auto it = m_variables.find(var);
                if (it != m_variables.end())
                {
                    try { current = std::stoi(it->second); }
                    catch (...) { current = 0; }
                }
                current -= n;
                m_variables[var] = std::to_string(current);
                std::cout << "[Lua] eclips.sub('" << var << "', " << n
                    << ") = " << current << std::endl;
                });

            // ── Variables meta (persistentes inter-save) — espejo de set/get/add/sub ──
            //
            // ── META-SET-MAS-ESTRICTO-QUE-SET, las puertas 2-4 de 4 ─────────
            //
            // El censo desde el EFECTO —grep de `m_meta->set(` sobre este
            // archivo— da CINCO escrituras con nombre del creador: las dos de
            // `executeMetaSet` (que comparten guardián) y estas TRES. La ficha
            // nombraba UNA sola (`eclips.meta_set`), que es el mismo error de
            // contar desde el nombre en vez de contar desde el efecto que
            // §7.541 acababa de documentar para `m_variables[`.
            //
            // Hasta este lote no preguntaban NADA, y su modo de falla es peor
            // que el que cerró el lote anterior: un `eclips.meta_set("_hora",
            // "5")` escribe a `meta.eclips` un dato que `${_hora}` nunca va a
            // devolver, y que **sobrevive a borrar los saves**. La escritura
            // muerta de `nombres-guion-bajo`, pero permanente y muda.
            //
            // Los tres van por separado a propósito: son copias del mismo
            // idiom, y cablear dos y olvidarse del tercero no rompe nada
            // visible (misma lección que `eclips.set/add/sub`).
            eclips.set_function("meta_set", [this](const std::string& var,
                const std::string& val) {
                    if (nombreConDuenio(var, "eclips.meta_set",
                        eclips::AlmacenDeNombre::Meta)) return;
                    if (!m_meta) {
                        std::cerr << "[Lua] eclips.meta_set: MetaStore no disponible."
                            << std::endl;
                        return;
                    }
                    m_meta->set(var, val);
                    std::cout << "[Lua] eclips.meta_set('" << var << "', '"
                        << val << "')  (persistido)" << std::endl;
                });

            eclips.set_function("meta_get", [this](const std::string& var)
                -> std::string {
                    if (!m_meta) return "";
                    return m_meta->get(var);
                });

            eclips.set_function("meta_add", [this](const std::string& var, int n) {
                if (nombreConDuenio(var, "eclips.meta_add",
                    eclips::AlmacenDeNombre::Meta)) return;
                if (!m_meta) return;
                int current = 0;
                try { current = std::stoi(m_meta->get(var, "0")); }
                catch (...) { current = 0; }
                current += n;
                m_meta->set(var, std::to_string(current));
                std::cout << "[Lua] eclips.meta_add('" << var << "', " << n
                    << ") = " << current << std::endl;
                });

            eclips.set_function("meta_sub", [this](const std::string& var, int n) {
                if (nombreConDuenio(var, "eclips.meta_sub",
                    eclips::AlmacenDeNombre::Meta)) return;
                if (!m_meta) return;
                int current = 0;
                try { current = std::stoi(m_meta->get(var, "0")); }
                catch (...) { current = 0; }
                current -= n;
                m_meta->set(var, std::to_string(current));
                std::cout << "[Lua] eclips.meta_sub('" << var << "', " << n
                    << ") = " << current << std::endl;
                });

            // ── Personajes ─────────────────────────────────────────────────

            eclips.set_function("show", [this](const std::string& alias,
                sol::optional<std::string> pos,
                sol::optional<int> effect) {
                    std::string posStr = pos.value_or("centro");
                    cmdMostrar(alias, "", posStr, effect.value_or(0));
                    std::cout << "[Lua] eclips.show('" << alias << "', '"
                        << posStr << "')" << std::endl;
                });

            eclips.set_function("hide", [this](const std::string& alias,
                sol::optional<int> effect) {
                    cmdOcultar(alias, effect.value_or(0));
                    std::cout << "[Lua] eclips.hide('" << alias << "')"
                        << std::endl;
                });

            eclips.set_function("expression", [this](const std::string& alias,
                const std::string& expr) {
                    swapExpression(alias, expr);
                    std::cout << "[Lua] eclips.expression('" << alias << "', '"
                        << expr << "')" << std::endl;
                });

            // ── Fondo ──────────────────────────────────────────────────────

            eclips.set_function("background", [this](const std::string& path) {
                loadBackground(path);
                std::cout << "[Lua] eclips.background('" << path << "')"
                    << std::endl;
                });

            // ── Audio ──────────────────────────────────────────────────────

            eclips.set_function("music", [this](const std::string& path) {
                if (m_hasAudio) m_audio.playMusic(path, true);
                m_currentMusicPath = path;
                std::cout << "[Lua] eclips.music('" << path << "')"
                    << std::endl;
                });

            eclips.set_function("sound", [this](const std::string& path) {
                if (m_hasAudio) m_audio.playSound(path);
                std::cout << "[Lua] eclips.sound('" << path << "')"
                    << std::endl;
                });

            eclips.set_function("stop_music", [this]() {
                if (m_hasAudio) m_audio.stopMusic();
                m_currentMusicPath.clear();
                std::cout << "[Lua] eclips.stop_music()" << std::endl;
                });

            // ── Notificación ───────────────────────────────────────────────

            eclips.set_function("notify", [this](const std::string& msg) {
                m_toastMsg = t(msg);
                m_toastTimer = 2.0f;
                std::cout << "[Lua] eclips.notify('" << msg << "')"
                    << std::endl;
                });

            // ── Navegación (DIFERIDA) ──────────────────────────────────────
            // Estas no se ejecutan inmediatamente — ponen flags que el
            // handler de EclipCmdType::Lua procesa al terminar el script.
            // Esto evita conflictos con m_cmdIndex.

            eclips.set_function("jump", [this](const std::string& scene) {
                m_luaPendingGoto = scene;
                std::cout << "[Lua] eclips.goto('" << scene
                    << "') [diferido]" << std::endl;
                });

            eclips.set_function("load_script", [this](const std::string& path,
                sol::optional<std::string> scene) {
                    m_luaPendingLoadScript = path;
                    m_luaPendingLoadScriptScene = scene.value_or("");
                    std::cout << "[Lua] eclips.load_script('" << path << "', '"
                        << m_luaPendingLoadScriptScene
                        << "') [diferido]" << std::endl;
                });

            // ── Consultas (read-only) ──────────────────────────────────────

            eclips.set_function("scene", [this]() -> std::string {
                return m_currentScene;
                });

            eclips.set_function("char_count", [this]() -> int {
                return (int)m_activeChars.size();
                });

            // ── E-CP6: efectos de render del creador ──────────────────────
            // eclips.register_effect("nombre", fn) — fn(ctx, params, pasadas)
            // corre por frame mientras el driver diga activo: MISMO contrato
            // que una primitiva (transformador de la lista). El NOMBRE lo
            // declara el .ecl (`efecto_lua`); esto aporta la función.
            eclips.set_function("register_effect", [this](std::string name,
                sol::protected_function fn) {
                    for (auto& c : name)
                        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';

                    if (parseAnimType(name) != AnimType::None ||
                        parseFxKind(name) != FxKind::None)
                    {
                        std::cerr << "[Lua] register_effect('" << name
                            << "'): es del motor (preset/primitiva). "
                            << "Renombra el tuyo. No se registra."
                            << std::endl;
                        return;
                    }
                    if (!fn.valid())
                    {
                        std::cerr << "[Lua] register_effect('" << name
                            << "'): falta la funcion. No se registra."
                            << std::endl;
                        return;
                    }

                    bool declarado = false;
                    for (const auto& le : m_script.luaEffects)
                        if (le.first == name) { declarado = true; break; }
                    if (!declarado)
                        std::cerr << "[Lua] AVISO: register_effect('" << name
                            << "'): no esta declarado con `efecto_lua` en el "
                            << ".ecl — `animar` no lo va a resolver (los "
                            << "nombres se resuelven al parsear). Se registra "
                            << "igual." << std::endl;

                    m_luaFx[name] = fn;
                    std::cout << "[Lua] eclips.register_effect('" << name
                        << "')" << std::endl;
                });

            std::cout << "[LuaManager] Bindings registrados (tabla 'eclips')"
                << std::endl;
        }

        // ── E-CP6: merge de declaraciones `efecto_lua` (script encadenado /
        // save de otro script). Override por nombre, como los personajes.
        void mergeLuaEffectDecls(
            const std::vector<std::pair<std::string, std::string>>& nuevos)
        {
            for (const auto& [nombre, ruta] : nuevos)
            {
                bool visto = false;
                for (auto& le : m_script.luaEffects)
                    if (le.first == nombre)
                    { le.second = ruta; visto = true; break; }
                if (!visto) m_script.luaEffects.emplace_back(nombre, ruta);
            }
        }

        // ── E-CP6: correr los .lua declarados (boot / encadenado / load) ──
        // executeFile ya es pcall (safe_script_file): un archivo roto habla
        // y no tumba nada. Un archivo que corre y NO registra su nombre se
        // dice — el silencio seria la grieta de siempre.
        void loadDeclaredLuaEffects(
            const std::vector<std::pair<std::string, std::string>>& decls)
        {
            if (decls.empty()) return;

            std::string scriptDir;
            const auto slashPos = m_scriptPath.find_last_of("/\\");
            if (slashPos != std::string::npos)
                scriptDir = m_scriptPath.substr(0, slashPos);

            for (const auto& [nombre, ruta] : decls)
            {
                m_lua.executeFile(ruta, scriptDir);
                if (!m_luaFx.count(nombre))
                    std::cerr << "[VN] AVISO: efecto_lua '" << nombre
                        << "': \"" << ruta << "\" corrio pero no registro '"
                        << nombre << "' (falta eclips.register_effect(\""
                        << nombre << "\", fn)). `animar " << nombre
                        << "` no va a hacer nada." << std::endl;
            }
        }

        // ════════════════════════════════════════════════════════════════════
        // Lua Hooks — El motor llama a Lua en momentos clave
        // ════════════════════════════════════════════════════════════════════
        //
        // El creador define estas funciones en Lua (opcionales):
        //
        //   function on_scene_enter(scene)        — al entrar a una escena
        //   function on_choice_made(label, target) — al elegir una opción
        //   function on_variable_changed(var, old, new_val) — al cambiar variable
        //   function on_save()                    — retorna string con estado Lua
        //   function on_load(data)                — recibe el string para restaurar
        //
        // Si la función no existe en Lua, el hook se ignora silenciosamente.
        //
        // ════════════════════════════════════════════════════════════════════

        // ── Helper: llamar función Lua si existe ────────────────────────────
        // Retorna true si la función existía y se ejecutó.
        template<typename... Args>
        bool callLuaHook(const std::string& funcName, Args&&... args)
        {
            if (!m_lua.isInitialized()) return false;

            sol::state& lua = m_lua.state();
            sol::object fn = lua[funcName];
            if (!fn.valid() || fn.get_type() != sol::type::function)
                return false;

            sol::protected_function pfn = fn.as<sol::protected_function>();
            auto result = pfn(std::forward<Args>(args)...);

            if (!result.valid())
            {
                sol::error err = result;
                std::cerr << "[LuaHook] ERROR en " << funcName << "(): "
                    << err.what() << std::endl;
                return false;
            }
            return true;
        }

        // ── Hook: on_save() → string ────────────────────────────────────────
        // Llama a on_save() en Lua. Si retorna un string, ese es el estado
        // serializado que se guardará en el slot.
        std::string callLuaOnSave()
        {
            if (!m_lua.isInitialized()) return "";

            sol::state& lua = m_lua.state();
            sol::object fn = lua["on_save"];
            if (!fn.valid() || fn.get_type() != sol::type::function)
                return "";

            sol::protected_function pfn = fn.as<sol::protected_function>();
            auto result = pfn();

            if (!result.valid())
            {
                sol::error err = result;
                std::cerr << "[LuaHook] ERROR en on_save(): "
                    << err.what() << std::endl;
                return "";
            }

            // Esperar un string de retorno
            sol::object ret = result;
            if (ret.is<std::string>())
            {
                std::string data = ret.as<std::string>();
                std::cout << "[LuaHook] on_save() -> " << data.size()
                    << " bytes" << std::endl;
                return data;
            }

            return "";
        }

        // ── Cargar fondo (con transición si hay una pendiente) ──────────────

        // Tinte estático del fondo ("" = quitar). La grafía se guarda para el
        // save; los floats son derivados para el draw.
        void setBgTint(const std::string& hex)
        {
            m_bgTintHex = hex;
            m_bgTintR = m_bgTintG = m_bgTintB = 1.0f;
            if (!hex.empty())
                EclipParser::parseHexColor(hex, m_bgTintR, m_bgTintG, m_bgTintB);
        }

        void loadBackground(const std::string& path)
        {
            if (path.empty()) return;

            if (m_pendingTransition != VNTransitionType::None && !m_currentBgPath.empty())
            {
                // Cargar el nuevo fondo al caché
                Texture* newBg = m_assets.get(path);
                if (!newBg)
                {
                    std::cerr << "[VNScriptState] No se pudo cargar fondo: "
                        << path << std::endl;
                    m_pendingTransition = VNTransitionType::None;
                    return;
                }

                // El fondo viejo se mantiene pinned durante la transición
                m_transOldBgPath = m_currentBgPath;
                m_assets.pin(m_transOldBgPath);

                // Pinear el nuevo y unpinear el viejo después (en update)
                m_assets.pin(path);

                // Iniciar transición
                m_activeTransition = m_pendingTransition;
                m_pendingTransition = VNTransitionType::None;
                m_transProgress = 0.0f;
                m_currentBgPath = path;

                std::cout << "[VNScriptState] Transicion iniciada" << std::endl;
            }
            else
            {
                // Sin transición — cambio instantáneo
                std::string oldPath = m_currentBgPath;

                Texture* bg = m_assets.get(path);
                if (!bg)
                {
                    std::cerr << "[VNScriptState] No se pudo cargar fondo: "
                        << path << std::endl;
                }
                else
                {
                    // Unpinear fondo anterior, pinear nuevo
                    if (!oldPath.empty()) m_assets.unpin(oldPath);
                    m_assets.pin(path);
                    m_currentBgPath = path;
                }

                m_pendingTransition = VNTransitionType::None;
            }
        }

        // ── mostrar alias [expresion] posicion ────────────────────────────────

        void cmdMostrar(const std::string& alias, const std::string& exprRaw,
            const std::string& posStr, int effect = 0)
        {
            auto charIt = m_script.characters.find(alias);
            if (charIt == m_script.characters.end())
            {
                std::cerr << "[VNScriptState] Personaje desconocido: '"
                    << alias << "'" << std::endl;
                return;
            }

            // ── D2: ¿ya está en pantalla? ────────────────────────────────────
            // La respuesta decide DOS cosas, así que se calcula antes que nada:
            // si `look_default` rellena, y si un `mostrar` pelón toca el sprite.
            // `mostrar` es en realidad DOS verbos con un nombre — "entra en
            // escena" (crear) y "cámbiale algo a quien ya está" (update); el
            // segundo nunca se había diseñado y a cada rama le salió distinto.
            VNActiveChar* target = nullptr;
            for (auto& ac : m_activeChars)
            {
                if (ac.alias == alias) { target = &ac; break; }
            }

            // C-ALIAS: resolver apariencia (alias->compuesto, o default si vacio)
            // ANTES de que las dos ramas usen expr. El resto de la funcion usa
            // 'expr' (ahora local resuelto) sin cambios. Cubre 'mostrar' explicito
            // Y el atajo de dialogo (los dos pasan por aqui).
            //
            // D2/§7.305: `look_default` es el valor INICIAL, así que solo rellena
            // al CREAR. Sobre alguien ya visible, expresión vacía significa "no
            // dije nada de la cara" — NO "reseteala al default". Antes, un
            // `mostrar sayori izquierda` la movía y de paso le secaba las
            // lágrimas.
            std::string expr = (target && exprRaw.empty())
                ? std::string()
                : resolveApariencia(alias, exprRaw);

            VNPosition pos = EclipParser::parsePosition(posStr);
            bool isComposite = m_script.compositeChars.count(alias) > 0;

            // ── D2: la ley única del update ──────────────────────────────────
            // `mostrar` sobre alguien que ya está en pantalla cambia SOLO lo que
            // la línea nombró. Sin expresión no hay sprite que tocar — ni textura
            // que cargar, ni pin que mover: solo la posición. Vale igual para
            // simples, compuestos y compuestos sin `look_default` (que antes
            // eran las tres conductas distintas de esta misma línea).
            if (target && expr.empty())
            {
                moveAnchor(*target, pos);
                cancelExit(*target);
                return;
            }

            if (isComposite)
            {
                std::string folder = charIt->second;

                if (target)
                {
                    // Update existente — el helper detecta modo por heurística DDLC
                    if (!applyCompositeVisual(*target, folder, expr)) return;
                    moveAnchor(*target, pos);
                    cancelExit(*target);
                }
                else
                {
                    // Crear nuevo
                    if (expr.empty())
                    {
                        std::cerr << "[VN] 'mostrar " << alias << "' sin expresion "
                            << "y sin look_default: no hay que dibujar. Dale una "
                            << "pose (ej. 'mostrar " << alias << " 1r_a') o defini "
                            << "'look_default' en el bloque del personaje."
                            << std::endl;
                        return;
                    }
                    VNActiveChar fresh;
                    fresh.alias = alias;
                    if (!applyCompositeVisual(fresh, folder, expr)) return;
                    fresh.position = pos;
                    fresh.fadeAlpha = (effect == 1 || effect == 2) ? 1.0f : 0.0f;
                    fresh.fadeTarget = 1.0f;
                    fresh.slideX = (effect == 2) ? slideStartX(pos) : 0.0f;
                    fresh.slideTargetX = 0.0f;
                    fresh.slidingOut = false;
                    m_activeChars.push_back(fresh);
                }
            }
            else
            {
                // ── Personaje normal (código original, intacto) ──
                std::string path = charIt->second;
                if (!expr.empty())
                    path = buildExprPath(charIt->second, expr);

                Texture* tex = m_assets.get(path);
                if (!tex && !expr.empty())
                {
                    std::cout << "[VNScriptState] Expresion '"
                        << expr << "' no encontrada para '"
                        << alias << "', usando base." << std::endl;
                    path = charIt->second;
                    tex = m_assets.get(path);
                }
                if (!tex)
                {
                    std::cerr << "[VNScriptState] Error cargando textura: "
                        << path << std::endl;
                    return;
                }

                m_assets.pin(path);

                std::string resolvedExpr = (path == charIt->second) ? "" : expr;

                for (auto& ac : m_activeChars)
                {
                    if (ac.alias == alias)
                    {
                        if (ac.spritePath != path)
                            m_assets.unpin(ac.spritePath);
                        moveAnchor(ac, pos);
                        ac.expression = resolvedExpr;
                        ac.spritePath = path;
                        ac.composite = false;
                        cancelExit(ac);
                        return;
                    }
                }
                VNActiveChar ac;
                ac.alias = alias;
                ac.expression = resolvedExpr;
                ac.spritePath = path;
                ac.position = pos;
                ac.composite = false;
                ac.fadeAlpha = (effect == 1) ? 1.0f : (effect == 2) ? 1.0f : 0.0f;
                ac.fadeTarget = 1.0f;
                ac.slideX = (effect == 2) ? slideStartX(pos) : 0.0f;
                ac.slideTargetX = 0.0f;
                ac.slidingOut = false;
                m_activeChars.push_back(ac);
            }
        }

        // ── Mover el ancla de alguien que YA está en pantalla ─────────────────
        // Un personaje se dibuja en `positionToX(position) + slideX`: el ancla
        // dice dónde PERTENECE, y `slideX` cuánto le falta para pertenecer.
        // Cambiar el ancla sin tocar `slideX` le cambia el SIGNIFICADO por la
        // espalda — los mismos 800px que eran "a la derecha de centro_der"
        // pasan a ser "a la derecha de izquierda", que es otro punto del mundo,
        // y el personaje da un tirón. Es una conversión de coordenadas que
        // faltaba, no un detalle visual.
        //
        // Solo se compensa si hay un slide EN VUELO (`slideX != 0`). Con el
        // personaje quieto —que es el caso normal— `mostrar` sigue siendo el
        // corte instantáneo que firmó la mesa D2: las dos leyes conviven en
        // vez de pelearse. Un personaje asentado SIEMPRE tiene `slideX == 0`
        // (una entrada termina en 0, y una salida que termina se borra), así
        // que la condición dice exactamente "está a mitad de un vuelo".
        static void moveAnchor(VNActiveChar& ac, VNPosition nueva)
        {
            if (ac.slideX != 0.0f)
            {
                const float vw = (float)viewport::width();
                ac.slideX += EclipParser::positionToX(ac.position, vw)
                           - EclipParser::positionToX(nueva, vw);
            }
            ac.position = nueva;
        }

        // ── Rescatar a alguien que venía SALIENDO de escena ───────────────────
        // `ocultar` tiene dos sabores y este cancela los dos. Antes solo vivía
        // el `fadeTarget = 1.0f`, con el comentario "cancela fade-out si estaba
        // ocultándose" — medio verdad: cancelaba el sabor FADE, pero el sabor
        // SLIDE seguía armado (`slidingOut` + destino fuera de pantalla), así
        // que el personaje aparecía, se deslizaba afuera y se borraba igual
        // (:1523). El rescate no rescataba.
        //
        // Los dos canales se cancelan RE-APUNTANDO, no dando un salto: el fade
        // ya lo hacía así, y el slide ahora también vuelve a su ancla gliding.
        // El censo de quiescencia ya cubre ese regreso (`slideX != slideTargetX`,
        // :1072), así que el render se mantiene despierto solo — sin latch nuevo.
        static void cancelExit(VNActiveChar& ac)
        {
            ac.fadeTarget   = 1.0f;
            ac.slidingOut   = false;
            ac.slideTargetX = 0.0f;   // de vuelta a su ancla
        }

        // ── Construir ruta de expresión desde la ruta base ────────────────────
        // "assets/personajes/Luna.png" + "feliz" → "assets/personajes/luna_feliz.png"
        static std::string buildExprPath(const std::string& basePath,
            const std::string& expr)
        {
            size_t dot = basePath.rfind('.');
            if (dot == std::string::npos) return basePath + "_" + expr;
            return basePath.substr(0, dot) + "_" + expr + basePath.substr(dot);
        }

        // ── Parsear tipo de transición desde string ───────────────────────────

        static VNTransitionType parseTransitionType(const std::string& s)
        {
            if (s == "dissolve" || s == "disolver")
                return VNTransitionType::Dissolve;
            if (s == "slide_izquierda" || s == "slide_left")
                return VNTransitionType::SlideLeft;
            if (s == "slide_derecha" || s == "slide_right")
                return VNTransitionType::SlideRight;
            // Default: fade
            return VNTransitionType::Fade;
        }

        // ── Renderizar transición de fondo ─────────────────────────────────────

        void renderBackgroundTransition(const Texture& oldBg, const Texture& newBg,
            float VW, float VH)
        {
            float t = m_transProgress;

            // Ease in-out para suavizar
            float eased = t * t * (3.0f - 2.0f * t); // smoothstep

            // F2.B.4: todas las llamadas _noBatch — esta funcion se ejecuta
            // dentro del begin/end del state (F2.B.3). Las versiones publicas
            // mataban el shader con su end() interno.
            // El saliente conserva SU tinte; el entrante trae el suyo. En
            // identidad (1,1,1) las llamadas son las de siempre.
            const f32 oR = m_transOldBgTintR, oG = m_transOldBgTintG,
                      oB = m_transOldBgTintB;
            const f32 nR = m_bgTintR, nG = m_bgTintG, nB = m_bgTintB;

            switch (m_activeTransition)
            {
            case VNTransitionType::Dissolve:
                draw2d::drawTextureTinted_noBatch(oldBg, 0, 0, VW, VH,
                    oR, oG, oB, 1.0f - eased);
                draw2d::drawTextureTinted_noBatch(newBg, 0, 0, VW, VH,
                    nR, nG, nB, eased);
                break;

            case VNTransitionType::Fade:
            {
                if (t < 0.5f)
                {
                    draw2d::drawBackgroundTinted_noBatch(oldBg, VW, VH, oR, oG, oB);
                    float blackAlpha = t * 2.0f;
                    draw2d::drawRect_noBatch(0, 0, VW, VH, 0, 0, 0, blackAlpha);
                }
                else
                {
                    draw2d::drawBackgroundTinted_noBatch(newBg, VW, VH, nR, nG, nB);
                    float blackAlpha = (1.0f - t) * 2.0f;
                    draw2d::drawRect_noBatch(0, 0, VW, VH, 0, 0, 0, blackAlpha);
                }
                break;
            }

            case VNTransitionType::SlideLeft:
            {
                float offset = eased * VW;
                draw2d::drawTextureTinted_noBatch(oldBg, -offset, 0, VW, VH,
                    oR, oG, oB, 1.0f);
                draw2d::drawTextureTinted_noBatch(newBg, VW - offset, 0, VW, VH,
                    nR, nG, nB, 1.0f);
                break;
            }

            case VNTransitionType::SlideRight:
            {
                float offset = eased * VW;
                draw2d::drawTextureTinted_noBatch(oldBg, offset, 0, VW, VH,
                    oR, oG, oB, 1.0f);
                draw2d::drawTextureTinted_noBatch(newBg, -VW + offset, 0, VW, VH,
                    nR, nG, nB, 1.0f);
                break;
            }

            default:
                draw2d::drawBackgroundTinted_noBatch(newBg, VW, VH, nR, nG, nB);
                break;
            }
        }

        // ── Helper compartido: aplicar visual de personaje compuesto ─────────
        // Detecta modo automáticamente por convención DDLC ("primer carácter
        // dígito = pose, primer carácter letra = cara"):
        //   - Empieza con dígito + 0 guiones (ej. "3a"):    sprite completo
        //   - Empieza con dígito + 1 guión   (ej. "1r_a"):  2 capas (body+face)
        //   - Empieza con dígito + 2 guiones (ej. "1l_1r_a"): 3 capas
        //   - Empieza con letra (ej. "b", "feliz"):         face-only.
        //     Solo aplica si ac.composite=true (hay capas que conservar).
        //
        // Limpia paths anteriores (unpin) y carga nuevos (pin). NO modifica
        // position, fade*, slide* (eso es responsabilidad del caller).
        // Retorna true si OK, false si falló (ac no modificado).
        bool applyCompositeVisual(VNActiveChar& ac,
            const std::string& folder,
            const std::string& expr)
        {
            if (expr.empty()) return false;

            // Heurística DDLC: primer char digit → modo pose; letter → face-only.
            const char c = expr[0];
            const bool isPose = (c >= '0' && c <= '9');

            // Helper interno: limpiar todos los paths previos del ac.
            // (pin es bool, no refcount; unpin/pin múltiple es seguro.)
            auto unpinAllPrevious = [&]()
                {
                    if (!ac.leftPath.empty())   m_assets.unpin(ac.leftPath);
                    if (!ac.rightPath.empty())  m_assets.unpin(ac.rightPath);
                    if (!ac.bodyPath.empty())   m_assets.unpin(ac.bodyPath);
                    if (!ac.facePath.empty())   m_assets.unpin(ac.facePath);
                    if (!ac.spritePath.empty()) m_assets.unpin(ac.spritePath);
                };

            if (!isPose)
            {
                // ── Empieza con letra → face-only ──
                // Solo aplica si actualmente hay capas activas.
                if (!ac.composite)
                {
                    std::cerr << "[VN] cara sola '" << expr << "' ignorada: '"
                        << ac.alias << "' esta en sprite completo (sin cuerpo que "
                        << "conservar). Pone una pose antes (ej. '" << ac.alias
                        << " 1r_" << expr << "') o usa un sprite completo."
                        << std::endl;
                    return false;
                }

                std::string newFacePath = folder + expr + ".png";
                Texture* faceTex = m_assets.get(newFacePath);
                if (!faceTex)
                {
                    std::cerr << "[VN] Cara no encontrada: " << newFacePath
                        << std::endl;
                    return false;
                }

                if (ac.facePath != newFacePath)
                {
                    if (!ac.facePath.empty()) m_assets.unpin(ac.facePath);
                    m_assets.pin(newFacePath);
                    ac.facePath = newFacePath;
                    ac.compositeDirty = true;
                }
                ac.facePart = expr;
                ac.expression = expr;
                return true;
            }

            // ── Empieza con dígito → modo pose. Contar guiones bajos. ──
            int underscores = 0;
            size_t sep1 = std::string::npos, sep2 = std::string::npos;
            for (size_t i = 0; i < expr.size(); ++i)
            {
                if (expr[i] == '_')
                {
                    underscores++;
                    if (underscores == 1) sep1 = i;
                    else if (underscores == 2) sep2 = i;
                }
            }

            if (underscores >= 2)
            {
                // ── 3 capas: left_right_face ──
                std::string leftPart = expr.substr(0, sep1);
                std::string rightPart = expr.substr(sep1 + 1, sep2 - sep1 - 1);
                std::string facePart = expr.substr(sep2 + 1);

                std::string lPath = folder + leftPart + ".png";
                std::string rPath = folder + rightPart + ".png";
                std::string fPath = folder + facePart + ".png";

                Texture* leftTex = m_assets.get(lPath);
                Texture* rightTex = m_assets.get(rPath);
                Texture* faceTex = m_assets.get(fPath);
                if (!leftTex) { std::cerr << "[VN] Left no encontrado: " << lPath << std::endl; return false; }
                if (!rightTex) { std::cerr << "[VN] Right no encontrado: " << rPath << std::endl; return false; }
                if (!faceTex) { std::cerr << "[VN] Cara no encontrada: " << fPath << std::endl; return false; }

                unpinAllPrevious();
                m_assets.pin(lPath);
                m_assets.pin(rPath);
                m_assets.pin(fPath);

                ac.composite = true;
                ac.threeLayer = true;
                ac.leftPart = leftPart;
                ac.leftPath = lPath;
                ac.rightPart = rightPart;
                ac.rightPath = rPath;
                ac.facePart = facePart;
                ac.facePath = fPath;
                ac.bodyPart.clear();
                ac.bodyPath.clear();
                ac.spritePath = lPath;
                ac.expression = facePart;
                ac.compositeDirty = true;
                return true;
            }
            else if (underscores == 1)
            {
                // ── 2 capas: body_face ──
                std::string bodyPart = expr.substr(0, sep1);
                std::string facePart = expr.substr(sep1 + 1);

                std::string bPath = folder + bodyPart + ".png";
                std::string fPath = folder + facePart + ".png";

                Texture* bodyTex = m_assets.get(bPath);
                Texture* faceTex = m_assets.get(fPath);
                if (!bodyTex) { std::cerr << "[VN] Cuerpo no encontrado: " << bPath << std::endl; return false; }
                if (!faceTex) { std::cerr << "[VN] Cara no encontrada: " << fPath << std::endl; return false; }

                unpinAllPrevious();
                m_assets.pin(bPath);
                m_assets.pin(fPath);

                ac.composite = true;
                ac.threeLayer = false;
                ac.bodyPart = bodyPart;
                ac.bodyPath = bPath;
                ac.facePart = facePart;
                ac.facePath = fPath;
                ac.leftPart.clear();
                ac.leftPath.clear();
                ac.rightPart.clear();
                ac.rightPath.clear();
                ac.spritePath = bPath;
                ac.expression = facePart;
                ac.compositeDirty = true;
                return true;
            }
            else
            {
                // ── 0 guiones, empieza con dígito: sprite completo ──
                std::string path = folder + expr + ".png";
                Texture* tex = m_assets.get(path);
                if (!tex)
                {
                    std::cerr << "[VN] Sprite no encontrado: " << path
                        << std::endl;
                    return false;
                }

                unpinAllPrevious();
                m_assets.pin(path);

                releaseCachedTex(ac);   // era compuesto → ya no, liberar caché
                ac.composite = false;
                ac.threeLayer = false;
                ac.bodyPart.clear();   ac.bodyPath.clear();
                ac.facePart.clear();   ac.facePath.clear();
                ac.leftPart.clear();   ac.leftPath.clear();
                ac.rightPart.clear();  ac.rightPath.clear();
                ac.spritePath = path;
                ac.expression = expr;
                return true;
            }
        }

        // ── Cambiar expresión de un personaje activo ──────────────────────────
        // Carga la textura de expresión si no existe, con fallback al base.
        void swapExpression(const std::string& alias, const std::string& exprRaw)
        {
            // C-ALIAS: resolver apariencia tambien aqui (path de cara en dialogo
            // 1618 + Lua expression 2466). Sin esto, 's asustada:' trata "asustada"
            // como cara cruda -> PNG no encontrado. Fall-through preservado: un
            // token que NO es alias (cara real "a"/"feliz") pasa intacto -> face-only
            // como hoy. Cierra C-ALIAS en los dos entry points hermanos de cmdMostrar.
            std::string expr = resolveApariencia(alias, exprRaw);

            // Buscar si el personaje está en pantalla
            VNActiveChar* target = nullptr;
            for (auto& ac : m_activeChars)
            {
                if (ac.alias == alias)
                {
                    if (ac.expression == expr) return; // Ya tiene esta expresión
                    target = &ac;
                    break;
                }
            }
            if (!target) return; // Personaje no visible, ignorar

            auto charIt = m_script.characters.find(alias);
            if (charIt == m_script.characters.end()) return;

            // ── Compuesto: delegar al helper (detecta modo por heurística DDLC) ──
            if (m_script.compositeChars.count(alias) > 0)
            {
                applyCompositeVisual(*target, charIt->second, expr);
                return;
            }

            // Construir ruta de la expresión
            std::string path = buildExprPath(charIt->second, expr);

            // Cargar vía AssetManager
            Texture* tex = m_assets.get(path);
            if (!tex)
            {
                std::cout << "[VNScriptState] Expresion '"
                    << expr << "' no encontrada, manteniendo actual."
                    << std::endl;
                return;
            }

            // Unpinear sprite anterior, pinear nuevo
            m_assets.unpin(target->spritePath);
            m_assets.pin(path);

            target->expression = expr;
            target->spritePath = path;
        }

        // ── capa "nombre" "ruta" z=N x=N y=N w=N h=N ────────────────────
        // Carga la textura y la agrega como capa al SceneLayerManager.

        void cmdTexto(const EclipCmd& cmd)
        {
            // id: si viene vacio, autogeneramos uno interno (fire-and-forget)
            std::string id = cmd.arg1;
            if (id.empty())
                id = "__texto_" + std::to_string(++m_autoTextId);

            // Interpolar ${var} (snapshot al ejecutar; sirve el splash de boot
            // ${_hora}:${_minuto}). Reloj que tictaquea en vivo = flag futuro.
            const std::string content = interpolateVars(t(cmd.arg2));

            // ── Merge de estilo (sesion estilo) ──────────────────────────────
            // Si el creador puso estilo=X, buscamos el paquete en el manifest y
            // resolvemos valores efectivos per-propiedad: inline-si-esta ->
            // estilo-si-esta -> centinela vacio. Lo inline SIEMPRE pisa al
            // estilo (override por campo, no por bloque).
            const EstiloManifest* est = nullptr;
            if (!cmd.styleName.empty())
            {
                if (m_manifest)
                {
                    auto it = m_manifest->estilos.find(cmd.styleName);
                    if (it != m_manifest->estilos.end())
                        est = &it->second;
                }
                if (!est)
                {
                    // Estilo inexistente: aviso pedagogico, cae a plano/inline,
                    // NO crashea (coherente con fuente=X).
                    std::cout << "[VNScriptState] texto: el estilo \""
                        << cmd.styleName << "\" no fue declarado en proyecto.eclips. "
                        << "Uso lo que venga inline (o texto plano)." << std::endl;
                }
            }

            std::string effFontName = !cmd.fontName.empty() ? cmd.fontName
                : (est ? est->fontName : std::string());
            std::string effColorHex = !cmd.arg3.empty() ? cmd.arg3
                : (est ? est->colorHex : std::string());
            std::string effGlowColorHex = !cmd.glowColorHex.empty() ? cmd.glowColorHex
                : (est ? est->glowColorHex : std::string());
            f32 effGlowRadius = cmd.glowRadius > 0.0f ? cmd.glowRadius
                : (est ? est->glowRadius : 0.0f);
            f32 effGlowIntensity = cmd.glowIntensity > 0.0f ? cmd.glowIntensity
                : (est ? est->glowIntensity : 0.0f);

            // Aviso pedagogico: si la fuente efectiva no fue declarada, caera al
            // renderer default. Avisamos UNA vez aqui (no en render, 60x/seg).
            // La identidad del texto puede pertenecer al candidato: el ambito
            // vivo de FontManager aun es el anterior hasta el commit prerender.
            const std::string effFontKey = catalogoDeResolucion().resolver(effFontName);
            if (!effFontName.empty() &&
                (effFontKey.empty() || !m_fontManager.getExacto(effFontKey)))
            {
                std::cout << "[VNScriptState] texto: la fuente \""
                    << effFontName << "\" no fue declarada con 'fuente "
                    << effFontName << " \"...\" tamano=N'. "
                    << "Usando la fuente por defecto." << std::endl;
            }

            // Color: default blanco; si hay color (inline o estilo), lo parseamos
            f32 r = 1.0f, g = 1.0f, b = 1.0f;
            if (!effColorHex.empty())
                EclipParser::parseHexColor(effColorHex, r, g, b);

            // Glow: resolver color + defaults utiles aqui (no en el parser).
            // Corre sobre los valores YA mezclados (estilo+inline). Regla "puse
            // color → veo glow": si quedo glow_color pero sin radio/intensidad,
            // subimos a defaults (radio=2, intensidad=0.5) en vez de dejar 0.
            f32 glowR = 1.0f, glowG = 1.0f, glowB = 1.0f;
            f32 glowRadius = effGlowRadius;
            f32 glowIntensity = effGlowIntensity;
            bool hasGlow = !effGlowColorHex.empty()
                || glowRadius > 0.0f || glowIntensity > 0.0f;
            if (hasGlow)
            {
                if (!effGlowColorHex.empty())
                    EclipParser::parseHexColor(effGlowColorHex, glowR, glowG, glowB);
                if (glowRadius <= 0.0f)    glowRadius = 2.0f;     // default util
                if (glowIntensity <= 0.0f) glowIntensity = 0.5f;  // default util
            }

            f32 bordeR = 0.0f, bordeG = 0.0f, bordeB = 0.0f, bordeA = 1.0f;
            f32 bordeGrosor = 0.0f;
            std::vector<ResolvedTextStroke> bordeStrokes;
            const TextPatch resolvedPatch = makeTextPatch(cmd, est);
            if (resolvedPatch.borde)
            {
                bordeStrokes = resolvedPatch.borde->strokes;
                bordeR = resolvedPatch.borde->r;
                bordeG = resolvedPatch.borde->g;
                bordeB = resolvedPatch.borde->b;
                bordeA = resolvedPatch.borde->a;
                bordeGrosor = resolvedPatch.borde->grosor;

                // Fork 3a: gana el borde y el glow se apaga ENTERO.
                hasGlow = false;
                glowRadius = 0.0f;
                glowIntensity = 0.0f;
            }

            // ── Variante de hover (átomo enlace) ─────────────────────────────
            // Solo desde el estilo (CP1: sin override inline de hover). Defaults =
            // valores base, así el swap es completo. Sin hover en el estilo →
            // hasHover=false → el enlace no cambia ("no miente").
            bool effHasHover = (est && est->hasHover);
            f32 hovR = r, hovG = g, hovB = b;
            f32 hovGlowR = glowR, hovGlowG = glowG, hovGlowB = glowB;
            f32 hovGlowRadius = glowRadius, hovGlowIntensity = glowIntensity;
            f32 hovBordeR = bordeR, hovBordeG = bordeG, hovBordeB = bordeB,
                hovBordeA = bordeA;
            f32 hovBordeGrosor = bordeGrosor;
            std::vector<ResolvedTextStroke> hovBordeStrokes = bordeStrokes;
            if (effHasHover)
            {
                if (!est->hoverColorHex.empty())
                    EclipParser::parseHexColor(est->hoverColorHex, hovR, hovG, hovB);
                bool hovGlowColor = !est->hoverGlowColorHex.empty();
                if (hovGlowColor)
                    EclipParser::parseHexColor(est->hoverGlowColorHex, hovGlowR, hovGlowG, hovGlowB);
                if (est->hoverGlowRadius > 0.0f)    hovGlowRadius = est->hoverGlowRadius;
                if (est->hoverGlowIntensity > 0.0f) hovGlowIntensity = est->hoverGlowIntensity;
                // "puse glow_color hover → veo glow" (si el base no tenía glow)
                if (hovGlowColor)
                {
                    if (hovGlowRadius <= 0.0f)    hovGlowRadius = 2.0f;
                    if (hovGlowIntensity <= 0.0f) hovGlowIntensity = 0.5f;
                }

                // El merge puro resuelve la lista hover y su herencia.
                if (resolvedPatch.hover)
                {
                    hovBordeStrokes = resolvedPatch.hover->borde.strokes;
                    hovBordeR = resolvedPatch.hover->borde.r;
                    hovBordeG = resolvedPatch.hover->borde.g;
                    hovBordeB = resolvedPatch.hover->borde.b;
                    hovBordeA = resolvedPatch.hover->borde.a;
                    hovBordeGrosor = resolvedPatch.hover->borde.grosor;
                }
            }

            // Si ya existe una capa-texto con ese id, ACTUALIZAR (no re-crear).
            // RT-1: la ley del update vive en `SceneLayerManager` y el merge de
            // estilo en `makeTextPatch` (`EclipScript.h`) — las dos del lado
            // alcanzable por el harness. Aca solo queda el dispatch.
            //
            // Antes esta rama escribia x/y/z/fuente/glow/hover SIEMPRE, aunque
            // la linea no los nombrara: por eso `texto cartel "SAYORI" ...
            // glow_color=#FF6BAA` (test_fuentes.ecl:47) le borraba en silencio
            // los 72px que le habia puesto la :24.
            SceneElement* existing = m_sceneLayers.get(id);
            if (existing)
            {
                const auto parche = makeTextPatch(cmd, est);
                m_sceneLayers.applyTextPatch(id, content, parche);
                // CP-R3: la identidad se rehace SOLO si la línea nombró una
                // fuente. Rehacerla siempre le cambiaría el atlas a una capa
                // que sobrevivió a un `cargar_script` y que esta línea no
                // estaba tocando — que es exactamente lo que el CP evita.
                if (parche.fontName)
                {
                    if (SceneElement* e = m_sceneLayers.get(id))
                        e->fontKey = catalogoDeResolucion().resolver(*parche.fontName);
                }
                return;
            }

            m_sceneLayers.addTextLayer(id, content, cmd.layerZ,
                cmd.layerX, cmd.layerY, r, g, b, 1.0f, effFontName,
                m_activeScopeOwner);

            // Setear glow en la capa recien creada (addTextLayer no lo toma).
            if (SceneElement* created = m_sceneLayers.get(id))
            {
                created->scopeOwner = m_activeScopeOwner;
                // CP-R3 · identidad tardía 1 de 3.
                created->fontKey = catalogoDeResolucion().resolver(effFontName);
                created->glowR = glowR; created->glowG = glowG; created->glowB = glowB;
                created->glowRadius = glowRadius;
                created->glowIntensity = glowIntensity;
                created->hasHover = effHasHover;
                created->hoverTextR = hovR; created->hoverTextG = hovG; created->hoverTextB = hovB;
                created->hoverGlowR = hovGlowR; created->hoverGlowG = hovGlowG; created->hoverGlowB = hovGlowB;
                created->hoverGlowRadius = hovGlowRadius;
                created->hoverGlowIntensity = hovGlowIntensity;
                created->bordeR = bordeR; created->bordeG = bordeG;
                created->bordeB = bordeB; created->bordeA = bordeA;
                created->bordeGrosor = bordeGrosor;
                created->bordeStrokes = bordeStrokes;
                created->hoverBordeR = hovBordeR; created->hoverBordeG = hovBordeG;
                created->hoverBordeB = hovBordeB; created->hoverBordeA = hovBordeA;
                created->hoverBordeGrosor = hovBordeGrosor;
                created->hoverBordeStrokes = hovBordeStrokes;
            }
        }

        // ── ATLAS-DE-FUENTE CP-R3 · el catalogo, ANTES del primer comando ───
        // El parseo entero del `.ecl` termina antes de `jumpToScene`, asi que
        // los perfiles se pueden hornear en fase GL segura antes de ejecutar
        // nada. Eso es lo que retira la dependencia de orden que tenia el
        // motor: hasta este CP, una seleccion escrita ARRIBA de su declaracion
        // caia a la fuente base y una escrita ABAJO funcionaba.
        //
        // Se llama en los TRES caminos que cambian de script —`onEnter`,
        // `loadNewScript` y `loadFromSlot`—, y siempre DESPUES de actualizar
        // `m_scriptPath`: la identidad local es `{ruta de script, nombre}`.
        bool materializarCatalogoFuentes()
        {
            // cargar_script puede correr en el mismo update que preparo el
            // arranque. La resolucion y el commit deben usar el NUEVO archivo,
            // sin publicar el catalogo vivo por delante de la generacion.
            if (m_hasCandidateCatalog && m_fontSources &&
                m_fontSources->pendingGeneration() != 0)
            {
                m_candidateCatalog.adoptarLocales(m_scriptPath, m_script.fuentes);
                if (m_hasCandidateLocales)
                {
                    m_candidateLocales = m_script.fuentes;
                    m_candidateScriptPath = m_scriptPath;
                }
                const bool ok = m_candidateCatalog.materializar(m_fontManager, false);
                syncLiveFontIdentities();
                return ok;
            }
            if (m_manifest) m_fontCatalog.adoptarGlobales(m_manifest->fuentes);
            m_fontCatalog.adoptarLocales(m_scriptPath, m_script.fuentes);
            const bool ok = m_fontCatalog.materializar(m_fontManager);
            syncLiveFontIdentities();
            return ok;
        }

        const FontCatalog& catalogoDeResolucion() const
        {
            return m_hasCandidateCatalog ? m_candidateCatalog : m_fontCatalog;
        }

        void syncLiveFontIdentities()
        {
            std::vector<std::string> live;
            for (const SceneElement* elem : m_sceneLayers.getSortedLayers())
            {
                if (!elem) continue;
                if (!elem->fontKey.empty()) live.push_back(elem->fontKey);
                if (elem->isConsole && !elem->console.fontKey.empty())
                    live.push_back(elem->console.fontKey);
            }
            m_dialogBox.collectLiveFontKeys(live);
            m_fontManager.syncLiveLocalIdentities(live);
        }

        // Liga cada tramo Rich Text a la identidad del archivo que lo escribio
        // (identidad tardia 2 de 3). Corre ANTES de `startDialogue`, que copia
        // el vector de efectos entero adentro de `VNDialogBox`: si esperara al
        // dibujo, el tramo ya viajo con el nombre crudo y nada mas.
        void ligarFuentesRichText(eclips::RichText& rich)
        {
            for (auto& e : rich.effects)
            {
                if (e.fontName.empty()) continue;
                e.fontKey = catalogoDeResolucion().resolver(e.fontName);
            }
        }

        void cmdFuente(const EclipCmd& cmd)
        {
            // CP-R3: el perfil YA NACIO, en `materializarCatalogoFuentes`.
            // Este comando quedo como marca idempotente — sigue siendo
            // ejecutable y sigue estando en la escena, pero ya no decide
            // cuando existe la fuente.
            const std::string clave = catalogoDeResolucion().resolver(cmd.fontName);
            if (!clave.empty() && m_fontManager.getExacto(clave)) return;

            // **La "red" legacy se RETIRO, y no por prolijidad.** `load()`
            // registra bajo el nombre PELADO, o sea en el espacio GLOBAL: un
            // nombre que el catalogo resolvio como LOCAL se habria vuelto
            // global por la puerta de atras, rompiendo la ley de ambito que
            // este CP entero existe para establecer. Y con el sumidero de la
            // escena duplicada ya cerrado, el unico caso que la justificaba
            // desaparecio.
            //
            // Lo que queda es voz: si el creador escribio una declaracion y el
            // perfil no llego a existir, eso es informacion suya.
            std::cout << "[VNScriptState] la fuente \"" << cmd.fontName
                << "\" fue declarada pero su perfil no llego a cargarse ("
                << cmd.arg1 << " @ " << cmd.fontSize << "px). "
                << "El texto que la pida usara la fuente por defecto."
                << std::endl;
        }

        // ── Fuente base del VN + chrome: custom con fallback real ─────────────
        // TextRenderer::init libera el atlas anterior ANTES de abrir el TTF
        // nuevo. Si el tema apunta a una fuente ausente, no basta conservar el
        // bool viejo: el recurso ya murió. Re-hornear la fuente base evita un
        // menú interactivo pero mudo (controles, choices, historial).
        bool initBaseRenderer(TextRenderer& renderer,
                              const std::string& requestedPath,
                              float size, float kerning,
                              const char* role)
        {
            auto tryProfile = [&](const std::string& path) -> bool {
                if (!m_fontSources) return renderer.init(path, size, kerning);
                TextProfileSpec spec;
                spec.generation = m_fontSources->pendingGeneration() != 0
                    ? m_fontSources->pendingGeneration()
                    : m_fontSources->activeGeneration();
                spec.routes = { path };
                spec.repertoire = repertorioBootstrap();
                spec.fontSize = size;
                spec.kerning = kerning;
                spec.measure = FontMeasure::Height;
                return renderer.initProfile(spec);
            };

            if (tryProfile(requestedPath))
                return true;

            const std::string fallback = Settings::defaultFontPath();
            if (requestedPath == fallback)
                return false;

            std::cerr << "[VNScriptState] Aviso: la fuente custom de " << role
                << " no cargo ('" << requestedPath
                << "'); se usa la fuente base '" << fallback << "'."
                << std::endl;
            return tryProfile(fallback);
        }

        bool onFontGenerationPrepare(std::uint64_t pending)
        {
            (void)pending;
            const std::vector<fontdecl::Declaracion>* fuentes =
                m_fontSources->candidateFuentes();
            if (!fuentes && m_manifest) fuentes = &m_manifest->fuentes;
            const std::string* temaDefault = m_fontSources->candidateTemaDefault();
            if (!temaDefault && m_manifest) temaDefault = &m_manifest->temaDefault;

            m_candidateCatalog = FontCatalog();
            if (fuentes) m_candidateCatalog.readoptarGlobales(*fuentes);
            const std::vector<fontdecl::Declaracion>& locales =
                m_hasCandidateLocales ? m_candidateLocales : m_script.fuentes;
            const std::string& candidatePath =
                m_hasCandidateLocales ? m_candidateScriptPath : m_scriptPath;
            m_candidateCatalog.adoptarLocales(candidatePath, locales);
            bool ok = m_candidateCatalog.materializar(
                m_fontManager, /*publishLive*/ false);

            if (!m_hasCandidateTheme && !m_themeOverriddenByScript && temaDefault)
            {
                if (!temaDefault->empty())
                {
                    auto it = m_variables.find("_tema_activo");
                    bool changed = (it == m_variables.end()
                        || it->second != *temaDefault);
                    if (changed)
                    {
                        m_candidateTheme = Theme::loadFromFile(*temaDefault);
                        m_candidateThemePath = *temaDefault;
                        m_hasCandidateTheme = true;
                    }
                }
                else
                {
                    auto it = m_variables.find("_tema_activo");
                    bool wasCustom = (it != m_variables.end() && !it->second.empty());
                    if (wasCustom)
                    {
                        m_candidateTheme = Theme::defaults();
                        m_candidateThemePath.clear();
                        m_hasCandidateTheme = true;
                    }
                }
            }

            m_hasCandidateBases = false;
            if (ok && m_hasCandidateTheme)
            {
                const bool custom =
                    m_candidateTheme.defaultDialogBox.hasCustomFont();
                if (custom)
                {
                    ok = fontBakeThemeBases(
                        *m_fontSources,
                        m_candidateTextRenderer,
                        m_candidateNameRenderer,
                        m_candidateTheme.defaultDialogBox.fontPath,
                        m_candidateTheme.defaultDialogBox.kerning);
                    m_hasCandidateBases = ok;
                }
                else if (m_theme.defaultDialogBox.hasCustomFont())
                {
                    ok = fontBakeThemeBases(
                        *m_fontSources,
                        m_candidateTextRenderer,
                        m_candidateNameRenderer,
                        Settings::defaultFontPath(),
                        0.0f);
                    m_hasCandidateBases = ok;
                }
            }

            m_hasCandidateCatalog = ok;
            if (!ok)
            {
                m_hasCandidateTheme = false;
                m_hasCandidateBases = false;
            }
            return ok;
        }

        void onFontGenerationCommitted(std::uint64_t active, std::uint64_t previous)
        {
            (void)active;
            if (m_hasCandidateCatalog)
            {
                m_fontCatalog = std::move(m_candidateCatalog);
                m_fontCatalog.publicarLive(m_fontManager);
                m_hasCandidateCatalog = false;
            }
            if (m_hasCandidateLocales)
            {
                m_script.fuentes = std::move(m_candidateLocales);
                m_hasCandidateLocales = false;
                m_candidateScriptPath.clear();
            }
            // `PUERTAS-QUE-NO-DIFIEREN`: un idioma DIFERIDO no es de este corte.
            // Publicarlo aca lo pondria vivo con los atlas del anterior, que es
            // exactamente el dano que la ficha nombra. Guardian: `21.5`.
            if (m_hasCandidateLanguage && !m_idiomaDiferido && m_lang && m_manifest)
            {
                std::filesystem::path raiz =
                    std::filesystem::path(m_manifest->sourcePath).parent_path();
                if (raiz.empty()) raiz = ".";
                const std::string original = m_manifest->idiomas.empty()
                    ? m_candidateLanguage : m_manifest->idiomas.front();
                m_lang->load(m_candidateLanguage, raiz.string(), original);
                langMemoLimpiar();
                fontMemoEspejoLimpiar();
                Settings::setLanguage(m_candidateLanguage);
                m_hasCandidateLanguage = false;
                m_candidateLangStore.reset();
            }
            // `R2`: UN TEMA DIFERIDO NO ES DE ESTE CORTE. Aplicarlo aca lo
            // pondria vivo SIN haber horneado sus bases -- `m_hasCandidateBases`
            // es false porque su prepare nunca corrio --, o sea `m_theme` nuevo
            // con los renderers de texto viejos. Un tema a medio aplicar, en
            // silencio, que es peor que el defecto que `R2` vino a cerrar.
            // Guardian: `19.5`.
            if (m_hasCandidateTheme && !m_temaDiferido)
            {
                m_applyingManifestPresentation = true;
                if (m_hasCandidateBases)
                {
                    m_textRenderer.stealReadyFrom(m_candidateTextRenderer);
                    m_nameRenderer.stealReadyFrom(m_candidateNameRenderer);
                    m_hasFont = m_textRenderer.isLoaded();
                    m_hasNameFont = m_nameRenderer.isLoaded();
                    m_hasCandidateBases = false;
                }
                unpinThemeSprites(m_theme);
                m_theme = std::move(m_candidateTheme);
                pinThemeSprites(m_theme);
                pinControlStyles();
                refreshTextSpeedControls();
                m_variables["_tema_activo"] = m_candidateThemePath;
                m_applyingManifestPresentation = false;
                m_hasCandidateTheme = false;
            }
            m_fontManager.dropReplacedGeneration(previous);
            syncLiveFontIdentities();
        }

        void onFontGenerationRolledBack(std::uint64_t dropped)
        {
            m_hasCandidateCatalog = false;
            m_candidateCatalog = FontCatalog();
            // `R2`: el rollback de un paquete AJENO no borra el candidato del
            // tema DIFERIDO. Borrarlo seria descartar el cambio del creador por
            // la puerta de atras -- justo la rama del fork que no se firmo.
            // El diferido se apaga solo cuando consigue SU corte (`update`) o
            // cuando se cae su propio paquete. Guardian: `19.5`.
            if (!m_temaDiferido) m_hasCandidateTheme = false;
            m_hasCandidateBases = false;
            // `PUERTAS-QUE-NO-DIFIEREN`: gemelo de la guarda del tema, y con una
            // mitad mas. Borrar el idioma diferido seria descartar en silencio
            // un pedido del JUGADOR; y el `reset()` ademas le sacaria al
            // reintento el `LangStore` que necesita para re-registrarse en el
            // store, o sea que lo dejaria pidiendo sin candidato. Guardian:
            // `21.5`.
            if (!m_idiomaDiferido)
            {
                m_hasCandidateLanguage = false;
                m_candidateLangStore.reset();
            }
            m_hasCandidateLocales = false;
            m_candidateLocales.clear();
            m_candidateScriptPath.clear();
            m_fontManager.dropGeneration(dropped);
        }

        // ── Registro Tier-A: tabla-descriptor (D3a) ──────────────────────────
        // Single source of truth de QUÉ settings del motor existen + su modelo de
        // valor + su 'kind'. Mata las 4 cadenas if-else paralelas keyed al mismo
        // string (§7.72 un nivel arriba: si read conoce un setting y resolve no,
        // bug silencioso). El bind resuelve el descriptor UNA vez y lo guarda en
        // BoundControl → read/write/lock/render switchean sobre la key sin
        // re-matchear string por frame. Tier-B (Lua) entrará como fila/key nueva.
        enum class CtrlSetting { Unknown, MusicVol, SfxVol, TextSpeed, SkipUnseen, Fullscreen, Resolution, Language };
        enum class CtrlKind { Range, Discrete, Bool, Enum };   // Enum → catálogo nombrado (render flechas en su CP). Bool → toggle (D2)
        enum class CtrlRender { Bar, Toggle, Arrows, Boxes };     // barra/pill/flechas/cajas. Arrows=◄label► · Boxes=fila de N cajas (Enum, tipo=cajas)
        // Geometría compartida render↔hit-test (§7.72: medida y dibujo resuelven
        // con la MISMA constante, o el clic y el pixel se desincronizan).
        static constexpr float kPillH = 28.0f;   // alto del riel del toggle
        static constexpr float kBarTrackH = 8.0f;    // alto del track de la barra
        static constexpr float kBarGrab = 14.0f;   // margen de agarre vertical de la barra
        // Flechas (Enum): geometría compartida render↔hit (§7.148, misma constante).
        static constexpr float kArrowW = 44.0f;   // ancho de cada zona de flecha ◄/►
        static constexpr float kArrowH = 36.0f;   // alto de la fila (hit + render)
        // Cajas (Enum, tipo=cajas): geometría compartida render↔hit↔input (§7.148).
        // Celdas de ancho igual (bc.width/N, Fork A); la caja pintada encoge kBoxPad
        // por lado dentro de su celda → aire visual sin zonas muertas en el hit.
        static constexpr float kBoxH = 40.0f;   // alto de la fila de cajas (hit + render)
        static constexpr float kBoxPad = 4.0f;    // aire entre la celda y la caja pintada

        struct CtrlDesc {
            const char* idEs; const char* idEn;
            CtrlSetting key;  CtrlKind kind;
            float dmin, dmax, dstep;
        };
        static constexpr CtrlDesc kCtrlTable[] = {
            { "musica_volumen",    "music_volume", CtrlSetting::MusicVol,   CtrlKind::Range,    0.0f, 100.0f, 1.0f },
            { "sonido_volumen",    "sound_volume", CtrlSetting::SfxVol,     CtrlKind::Range,    0.0f, 100.0f, 1.0f },
            { "velocidad_texto",   "text_speed",   CtrlSetting::TextSpeed,  CtrlKind::Discrete, 0.0f,   0.0f, 1.0f },
            { "skip_no_visto",     "skip_unseen",  CtrlSetting::SkipUnseen, CtrlKind::Bool,     0.0f,   1.0f, 1.0f },
            { "pantalla_completa", "fullscreen",   CtrlSetting::Fullscreen, CtrlKind::Bool,     0.0f,   1.0f, 1.0f },
            // Enum: dmax es placeholder; el catálogo (y su max real) se deriva
            // del lienzo en runtime en cmdControl → buildResOptions. (CP1)
            { "resolucion",        "resolution",   CtrlSetting::Resolution, CtrlKind::Enum,     0.0f,   0.0f, 1.0f },
            // `CP5`: el SEGUNDO Enum, y el primero que prueba que el catálogo
            // entra por el descriptor. Mismo placeholder de dmax: el catálogo
            // sale de `m_manifest->idiomas` en el bind (`llenarOpciones`).
            { "idioma",            "language",     CtrlSetting::Language,   CtrlKind::Enum,     0.0f,   0.0f, 1.0f },
        };
        // Único sitio de match string→descriptor (bilingüe). nullptr = desconocido.
        static const CtrlDesc* resolveCtrlDesc(const std::string& id)
        {
            for (const auto& d : kCtrlTable)
                if (id == d.idEs || id == d.idEn) return &d;
            return nullptr;
        }
        // Render (D2): el override tipo= gana; sin él, Bool→toggle, resto→barra.
        static CtrlRender resolveRender(const std::string& tipoOverride, CtrlKind kind)
        {
            std::string t = asciiToLower(tipoOverride);
            if (t == "toggle" || t == "interruptor") return CtrlRender::Toggle;
            if (t == "barra" || t == "bar")         return CtrlRender::Bar;
            if (t == "flechas" || t == "arrows")    return CtrlRender::Arrows;
            if (t == "cajas" || t == "boxes")     return CtrlRender::Boxes;
            if (kind == CtrlKind::Bool) return CtrlRender::Toggle;
            if (kind == CtrlKind::Enum) return CtrlRender::Arrows;
            return CtrlRender::Bar;
        }
        static const char* ctrlRenderName(CtrlRender render)
        {
            switch (render)
            {
            case CtrlRender::Bar:    return "barra";
            case CtrlRender::Toggle: return "toggle";
            case CtrlRender::Arrows: return "flechas";
            case CtrlRender::Boxes:  return "cajas";
            default:                 return "desconocido";
            }
        }
        // Fwd-decl: renderBar/renderToggle reciben BoundControl& en su FIRMA (se
        // parsea en orden, no en complete-class context como el cuerpo). La def
        // vive ~6244 → sin esto la firma no resuelve. §gotcha: firma ⊥ cuerpo.
        struct BoundControl;
        struct ControlBarGeometry
        {
            float valueStart = 0.0f, valueEnd = 0.0f;
            float trackY = 0.0f, trackH = kBarTrackH;
            float thumbW = 14.0f, thumbH = 22.0f;
            float hitHalfX = 7.0f, hitHalfY = 11.0f;
            const ControlStylePart* track = nullptr;
            const ControlStylePart* fill = nullptr;
            const ControlStylePart* thumb = nullptr;
        };
        // Mismo gotcha: renderSlotCard recibe SlotWidget& en su FIRMA y la def
        // vive ~6750 → sin fwd-decl la firma no resuelve.
        struct SlotWidget;
        // Y por TERCERA vez, `CP9b`: `textoDeFila`/`renderTeclaWidgets` reciben
        // `TeclaWidget&` en su firma. El compilador no dijo "falta el tipo":
        // dijo `falta el especificador de tipo; se presupone int` y despues
        // `textoDeFila(const int)`, que es el mismo diagnostico desviado que
        // este bloque existe para no volver a leer.
        struct TeclaWidget;
        void reconcileTextSpeedPresentation(BoundControl& control);

        // ── cmdControl: registra un control de setting Tier-A ────────────────
        // Resuelve el setting (arg1) contra la tabla, aplica el modelo (override
        // del comando o defaults del descriptor), lee el valor actual y guarda el
        // BoundControl con su 'desc'. Render (CP2) elige barra/toggle por
        // desc->kind; input (CP3) forkea por kind.
        // Single voice for every surface that cannot consume a control skin.
        // Keeping it here makes tecla/ranura diagnostics testable without
        // entering their slot or input flows.
#ifdef ECLIPS_TEST_SEAM
    public:
#endif
        void voiceUnsupportedControlSurface(const std::string& surface,
                                            const std::string& alias) const
        {
            std::cerr << "[control] alias='" << alias
                << "' propiedad='estilo_control' estado='base': superficie " << surface
                << " no soportada; consecuencia: se conserva " << surface
                << " sin skin; reparacion: use una barra horizontal.\n";
        }

        void cmdControl(const EclipCmd& cmd)
        {
            const std::string& id = cmd.arg1;

            // `CP9b`, fork 1b: EL DISCRIMINADOR. Un `control` cuyo argumento
            // empieza con `tecla.`/`key.` es un widget de TECLA, no un setting
            // Tier-A. Misma forma que `color-solido` (*"una `capa` cuyo
            // argumento empieza con `#`"*) y por la misma razon: la familia se
            // reconoce por la FORMA del argumento, sin gastar una clave — que
            // era el problema, porque `tipo=` y `accion=` ya estaban ocupadas.
            if (inputPartirClaveJugador(id).nuestra)
            {
                cmdControlTecla(cmd);
                return;
            }

            const CtrlDesc* desc = resolveCtrlDesc(id);
            if (!desc)
            {
                // `CP5`: la lista sale de `kCtrlTable`, que está tres líneas
                // más arriba y es `constexpr`. Estaba tipeada a mano y decía
                // CINCO cuando la tabla tenía SEIS —le faltaba `resolucion`
                // desde que shipeó—, así que con `idioma` habría quedado en 5
                // de 7. Se mata la especie y no el caso: misma enfermedad que
                // `ESPERADOS` contra las dos prosas del `CLAUDE.md`.
                std::string tierA;
                for (const auto& d : kCtrlTable)
                {
                    if (!tierA.empty()) tierA += ", ";
                    tierA += d.idEs;
                }
                std::cerr << "[control] setting desconocido: '" << id
                    << "' (Tier-A: " << tierA << "). Ignorado." << std::endl;
                return;
            }

            BoundControl bc;
            bc.desc = desc;
            bc.requestedRender = resolveRender(cmd.controlType, desc->kind); // D2
            bc.render = bc.requestedRender;
            bc.requestedRender = bc.render;
            bc.settingId = id;
            bc.x = cmd.layerX;
            bc.y = cmd.layerY;
            bc.vmin = (cmd.controlMin >= 0.0f) ? cmd.controlMin : desc->dmin;
            bc.vmax = (cmd.controlMax >= 0.0f) ? cmd.controlMax : desc->dmax;
            bc.vstep = (cmd.controlStep > 0.0f) ? cmd.controlStep : desc->dstep;
            bc.pasoExplicito = (cmd.controlStep > 0.0f);   // CP6: el recibo
            const bool numericTextSpeed =
                desc->key == CtrlSetting::TextSpeed &&
                m_theme.textSpeedDomain.isNumeric();
            if (numericTextSpeed)
            {
                bc.textSpeedDomain = m_theme.textSpeedDomain;
                const TextSpeedDomain& domain = bc.textSpeedDomain;
                const bool customDomain =
                    (cmd.controlMin >= 0.0f && cmd.controlMin != domain.min) ||
                    (cmd.controlMax >= 0.0f && cmd.controlMax != domain.max) ||
                    (cmd.controlStep >= 0.0f && cmd.controlStep != domain.step);
                if (customDomain)
                {
                    std::cerr << "[control] '" << id
                        << "' repite un dominio distinto al del theme; se usa "
                        << textSpeedFormatCps(domain.min) << ".."
                        << textSpeedFormatCps(domain.max) << " paso "
                        << textSpeedFormatCps(domain.step)
                        << (domain.includeInstant ? " con instantaneo" : " sin instantaneo")
                        << ". Borra min/max/paso del control.\n";
                }
                bc.vmin = domain.min;
                bc.vmax = textSpeedDomainEndpoint(domain);
                bc.vstep = domain.step;
                bc.pasoExplicito = true;
            }
            // Enum (resolución): el catálogo se deriva del lienzo en runtime; su
            // 'max' no es constexpr → se fija aquí, pisando el placeholder del
            // descriptor. El override min/max del comando no aplica a un Enum.
            else if (desc->kind == CtrlKind::Enum || desc->key == CtrlSetting::TextSpeed)
            {
                llenarOpciones(bc);                       // CP5 fork 1b
                // `CP5`: un selector de idioma con menos de dos códigos no es
                // un selector — es un cartel. El §3 firma que hace falta ≥2, y
                // el creador que escribió el widget con uno solo tiene un error
                // de proyecto, no una preferencia: se le dice. La guarda es de
                // `idioma` y NO de `resolucion` a propósito — un monitor chico
                // deja el catálogo de resolución en una sola opción y ése SÍ se
                // sigue bindeando, que es la conducta de hoy y la que el
                // guardián de no-op custodia.
                if (desc->key == CtrlSetting::Language && bc.opciones.size() < 2)
                {
                    std::cerr << "[control] '" << id << "' necesita al menos 2 idiomas "
                        "declarados en project.eclips (hay " << bc.opciones.size()
                        << "). Ignorado." << std::endl;
                    return;
                }
                if (desc->key == CtrlSetting::TextSpeed)
                {
                    const bool customDomain =
                        (cmd.controlMin >= 0.0f && cmd.controlMin != 0.0f) ||
                        (cmd.controlMax >= 0.0f &&
                         cmd.controlMax != (float)(bc.opciones.size() - 1)) ||
                        (cmd.controlStep > 0.0f && cmd.controlStep != 1.0f);
                    if (customDomain)
                    {
                        std::cerr << "[control] '" << id
                            << "' usa un dominio de velocidad incompatible; "
                            << "se sustituye por 0..N-1/paso 1." << std::endl;
                    }
                }
                bc.vmin = 0.0f;
                bc.vmax = bc.opciones.empty() ? 0.0f : (float)(bc.opciones.size() - 1);
                bc.vstep = 1.0f;
                // El `paso=` del creador ya quedó descartado dos líneas arriba
                // (un Enum se mueve de a una opción), así que el recibo también:
                // dejarlo en `true` haría que el ajuste por teclado creyera que
                // este 1.0 lo pidió alguien.
                bc.pasoExplicito = false;
            }
            reconcileTextSpeedPresentation(bc);
            // Default de ancho por tipo: pill compacto (56) vs barra (400).
            {
                float defW = (bc.render == CtrlRender::Toggle) ? 56.0f : 400.0f;
                bc.width = (cmd.controlWidth >= 0.0f) ? cmd.controlWidth : defW;
            }
            bc.styleName = cmd.styleName;
            if (!bc.styleName.empty())
            {
                if (!m_manifest)
                    std::cerr << "[control] alias='" << bc.styleName
                        << "' propiedad='estilo_control' estado='base': manifest ausente;"
                        << " consecuencia: fallback legacy; reparacion: declare el alias en proyecto.eclips.\n";
                else
                {
                    auto it = m_manifest->estilosControl.find(bc.styleName);
                    if (it != m_manifest->estilosControl.end())
                        bc.controlStyle = (bc.render == CtrlRender::Bar) ? &it->second : nullptr;
                    else if (m_manifest->estilos.find(bc.styleName) != m_manifest->estilos.end())
                    std::cerr << "[control] estilo '" << bc.styleName
                        << "' alias='" << bc.styleName << "' propiedad='estilo_control': solo existe como texto;"
                        << " consecuencia: fallback legacy; reparacion: use estilo_control.\n";
                    else if (it == m_manifest->estilosControl.end())
                        std::cerr << "[control] alias='" << bc.styleName
                            << "' propiedad='estilo_control' estado='base': alias ausente; consecuencia: fallback legacy;"
                            << " reparacion: declare estilo_control en proyecto.eclips.\n";
                    if (bc.render != CtrlRender::Bar)
                        voiceUnsupportedControlSurface(ctrlRenderName(bc.render), bc.styleName);
                }
            }
            bc.value = numericTextSpeed
                ? textSpeedDomainCoordinate(bc.textSpeedDomain,
                    effectiveTextSpeedForControl())
                : readControlValue(desc->key);
            bc.scopeOwner = m_activeScopeOwner;
            m_controls.push_back(bc);

            std::cout << "[control] '" << id << "' bound, value=" << bc.value
                << " (render=" << ctrlRenderName(bc.render)
                << " min=" << bc.vmin << " max=" << bc.vmax
                << " paso=" << bc.vstep << " x=" << bc.x << " y=" << bc.y
                << " locked=" << (isControlLocked(desc->key) ? "si" : "no") << ")"
                << std::endl;
        }

        // ── buildResOptions: deriva el catálogo de resoluciones de VENTANA ───
        // del lienzo (viewport) × tabla de escalas, filtrando las que no caben
        // en el monitor. Mismo aspect que el lienzo → letterbox sin barras.
        void buildResOptions()
        {
            m_resOptions.clear();
            const int dw = viewport::width();
            const int dh = viewport::height();
            if (dw <= 0 || dh <= 0) return;

            // Límite WINDOWED: área que deja libre el SO para una ventana con
            // chrome. DisplayBounds ofrecía 1920x1080 en un monitor 1080p y la
            // barra de título quedaba fuera de pantalla.
            int maxW = 0, maxH = 0;
            int disp = m_window ? SDL_GetWindowDisplayIndex(m_window) : 0;
            if (disp < 0) disp = 0;
            SDL_Rect usable{};
            if (SDL_GetDisplayUsableBounds(disp, &usable) == 0)
            {
                maxW = usable.w;
                maxH = usable.h;
            }
            else
            {
                // Degradación segura: no permitir crecer más que la ventana
                // actual si SDL no puede describir el área útil.
                if (m_window) SDL_GetWindowSize(m_window, &maxW, &maxH);
                if (maxW <= 0 || maxH <= 0) { maxW = dw; maxH = dh; }
                std::cerr << "[control] Aviso: no se pudo consultar el area util "
                    "del monitor; el catalogo no crecera sobre "
                    << maxW << "x" << maxH << ": " << SDL_GetError()
                    << std::endl;
            }

            // `CP5` (fork 1b): el catálogo y su etiqueta salen de una sola
            // llamada. Lo de acá arriba —viewport y usable-bounds— es el CABLE
            // de SDL, que es justo lo que ningún harness puede correr; la LEY
            // vive en el header y la custodia `harness_resolucion` §5.
            m_resOptions =
                window_resolution::buildCatalogoEtiquetado(dw, dh, maxW, maxH);
        }

        // ── llenarOpciones (CP5, fork 1b): EL PROVEEDOR ──────────────────────
        //
        // El punto único donde un `CtrlKind::Enum` consigue su catálogo. Antes
        // el widget leía `m_resOptions` directo, o sea que agregar un segundo
        // Enum mostraba RESOLUCIONES en el selector de idioma, en silencio
        // (§15.8a). Ahora el bind llena `bc.opciones` y el widget —render,
        // `vmax`, hit-test— no sabe cuál Enum está dibujando.
        //
        // El `switch` va SIN `default` a propósito, igual que el de `CtrlRender`
        // en `ajustarControl`: si `CtrlSetting` gana una fila Enum, el
        // compilador pone el rojo que ningún harness puede poner, porque este
        // enum vive en la clase que arrastra GL.
        TextSpeed effectiveTextSpeedForControl() const
        {
            const std::optional<TextSpeed> theme = m_theme.textSpeed;
            const TextSpeedResolution resolution = resolveTextSpeed(
                std::nullopt, std::nullopt, false,
                theme, m_theme.textSpeedLocked,
                Settings::textSpeedOverride());
            if (resolution.source == TextSpeedSource::Player &&
                m_theme.textSpeedDomain.isNumeric())
                return Settings::projectTextSpeedForDomain(
                    m_theme.textSpeedDomain);
            return resolution.value;
        }

        void llenarOpciones(BoundControl& bc)
        {
            bc.opciones.clear();
            switch (bc.desc->key)
            {
            case CtrlSetting::Resolution:
                buildResOptions();
                for (const auto& o : m_resOptions)
                    bc.opciones.push_back(o.etiqueta);
                break;
            case CtrlSetting::Language:
                // Sale más barato que el de `resolucion` y por eso no lleva
                // plomería: el catálogo ya vive adentro del state (§16.7 punto
                // 3), mientras que el otro necesita viewport + usable-bounds.
                // La etiqueta es el ENDÓNIMO, no el código: el jugador que no
                // entiende la pantalla igual reconoce su idioma.
                if (m_manifest)
                    for (const std::string& codigo : m_manifest->idiomas)
                        bc.opciones.push_back(eclips::langEndonimo(codigo));
                break;
            case CtrlSetting::TextSpeed:
            {
                const TextSpeed effective = effectiveTextSpeedForControl();
                bc.textSpeedOptions = textSpeedCatalog(effective);
                for (const auto& option : bc.textSpeedOptions)
                    bc.opciones.push_back(eclips::langTr(eclips::langActivo(),
                                                         option.labelEs.c_str(),
                                                         option.labelEn.c_str()));
                break;
            }
            case CtrlSetting::Unknown:
            case CtrlSetting::MusicVol:
            case CtrlSetting::SfxVol:
            case CtrlSetting::SkipUnseen:
            case CtrlSetting::Fullscreen:
                break;   // no son Enum: no tienen catálogo que llenar
            }
        }

        // ── renderControls (CP2): dispatch por render-type (barra/pill) ──────
        // Proyección limpia propia (sin offset de parallax) → UI fija. El live
        // read-back vive aquí (común a todos los tipos): si NO se arrastra, el
        // control refleja el setting real → vista viva. El dibujo forkea por
        // bc.render → renderBar / renderToggle.
        bool hasValidDistinctFocusVariant(const BoundControl& bc) const
        {
            if (!bc.controlStyle) return false;
            for (const char* name : {"track", "fill", "thumb"})
            {
                auto it = bc.controlStyle->variants.find(std::string(name) + "_al_enfocar");
                if (it == bc.controlStyle->variants.end()) continue;
                const auto& p = it->second;
                if (!p.declared || !p.valid) continue;
                if (!p.none && !p.color &&
                    (p.value.empty() || !const_cast<VNScriptState*>(this)->m_assets.get(p.value)))
                    continue;
                const ControlStylePart* base = name == std::string("track") ? &bc.controlStyle->track :
                    name == std::string("fill") ? &bc.controlStyle->fill : &bc.controlStyle->thumb;
                if (p.none != base->none || p.color != base->color)
                    return true;
                if (p.none && base->none)
                    continue;
                if (p.color)
                {
                    std::string pv = p.value, bv = base->value;
                    std::transform(pv.begin(), pv.end(), pv.begin(),
                        [](unsigned char c){ return (char)std::tolower(c); });
                    std::transform(bv.begin(), bv.end(), bv.begin(),
                        [](unsigned char c){ return (char)std::tolower(c); });
                    if (pv != bv) return true;
                }
                else if (p.value != base->value)
                    return true;
            }
            return false;
        }

        void renderControls()
        {
            if (m_controls.empty()) return;
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            draw2d::setupProjection(VW, VH);   // sin offset de parallax: UI fija

            for (int i = 0; i < (int)m_controls.size(); ++i)
            {
                BoundControl& bc = m_controls[i];
                if (i != m_draggingControl)
                    bc.value = readControlValue(bc.desc->key);

                const bool locked = isControlLocked(bc.desc->key);

                // ── `CP6`: LA BARRA DE FOCO ──────────────────────────────────
                //
                // **El `control` es la única de las cuatro familias sin señal de
                // hover**, y el §9 promete *"cero render nuevo"* porque su lista
                // nombra las de las otras tres (`choiceBtnHover*`, chrome de
                // ranura, barra §7.185, swap del enlace). Medido: `renderControls`
                // forkeaba sólo por `bc.render` y por `locked`. Un control focado
                // sería **invisible**, y un foco que no se ve es una feature que
                // el feel-test no puede dar por verde.
                //
                // Va HORNEADA y sin clave de tema (fork 5-iii): nombrar una
                // familia `control_foco_*` es sintaxis de creador y **bumpearía**
                // (regla 7), y reusar `ranura_hover_barra_*` haría que una clave
                // que dice `ranura` pinte un `control` — la sintaxis mentiría.
                // Es el corolario de §7.449: si nombrar se pone difícil, mirá si
                // se resuelve sin nombrar nada. La themabilidad queda fichada.
                //
                // El grosor NO se hereda de `slotCardHoverBarThickness`, y esa es
                // la parte medida: ese default es **0 = off** (`Theme.h:536`) y en
                // `tema_ddlc.eclips` las dos líneas están comentadas, así que
                // heredarlo shipearía el foco apagado.
                const bool styledFocusVariant = !m_focoDelPuntero && hasValidDistinctFocusVariant(bc);
                if (i == m_focoControl && (!bc.controlStyle || m_draggingControl != i) && !locked &&
                    (!bc.controlStyle || !m_focoDelPuntero) && !styledFocusVariant)
                {
                    const float bw = 6.0f, gap = 10.0f;
                    const float bh = (bc.render == CtrlRender::Toggle) ? kPillH
                                   : (bc.render == CtrlRender::Arrows) ? kArrowH
                                   : (bc.render == CtrlRender::Boxes)  ? kBoxH
                                                                       : kBarTrackH;
                    draw2d::drawRect(bc.x - gap - bw, bc.y, bw, bh,
                                     0.31f, 0.76f, 0.86f, 0.95f);
                }
                if (bc.render == CtrlRender::Toggle)
                    renderToggle(bc, bc.value >= 0.5f, locked);
                else if (bc.render == CtrlRender::Arrows)
                    renderArrows(bc, locked);
                else if (bc.render == CtrlRender::Boxes)
                    renderBoxes(bc, locked);
                else
                    renderBar(bc, locked);
            }
        }

        // `FORMAS_01`: `rombo` viste la PRIMITIVA, nunca el arte. El early-return
        // de textura de abajo es lo que sostiene `HC-BOR-07` sin una linea de
        // mas -- un PNG ya trae su silueta en los pixeles y no se le recorta
        // nada encima-. Y para que eso no sea MAGIA, el parser AVISA cuando el
        // creador declara `perilla_forma` sobre una perilla que resolvio a
        // imagen: la forma no se aplica, y el motor lo dice.
        bool drawControlPart(const ControlStylePart* part, float x, float y,
            float w, float h, float alpha = 1.0f, bool rombo = false)
        {
            if (!part || !part->declared || !part->valid || part->none) return false;
            if (!part->color && !part->value.empty())
            {
                if (Texture* tex = m_assets.get(part->value))
                {
                    draw2d::drawTextureTinted(*tex, x, y, w, h, 1, 1, 1, alpha);
                    return true;
                }
                return false;
            }
            float r=1,g=1,b=1;
            if (part->color) EclipParser::parseHexColor(part->value, r, g, b);
            // Un fan de CUATRO segmentos inscripto en la misma caja `(x,y,w,h)`:
            // los vertices caen en 0/90/180/270 grados, o sea un rombo de `w` por
            // `h`. La caja NO cambia, que es lo que deja el hit-test quieto
            // (`F1(a)`, y `D-55` sigue siendo la unica verdad espacial).
            //
            // Mezclar `glBegin` con el batch es seguro ACA y conviene saber por
            // que: `draw2d::drawRect` es el wrapper que hace su propio
            // `begin()`/`end()` (`Draw2D.h:201`), asi que flushea en cada
            // llamada y no queda un batch abierto por encima. Los `_noBatch` son
            // los que exigen que el caller lo sostenga, y aca no se usan.
            if (rombo)
                draw2d::drawCircle(x + w * 0.5f, y + h * 0.5f, w * 0.5f,
                    r, g, b, alpha, 4, h * 0.5f);
            else
                draw2d::drawRect(x, y, w, h, r, g, b, alpha);
            return true;
        }

        float controlThumbHalfWidth(const BoundControl& bc) const
        {
            float half = 7.0f;
            if (!bc.controlStyle) return half;
            // `D-55`: el numero declarado mueve el PISO del hit, no solo el
            // render. Sin esta linea `perilla_ancho 22` dibuja 22 y captura 14
            // -cuatro pixeles muertos por lado-, que es exactamente el defecto
            // que `CONTROL-SKIN-AUTORABLE-01` se firmo para reparar, entrando
            // de nuevo por la puerta de al lado. El `max` de abajo se conserva:
            // una variante de estado MAS ANCHA sigue ganando (§7.3).
            if (bc.controlStyle->perillaAnchoDeclarada)
                half = bc.controlStyle->perillaAncho * 0.5f;
            auto scan = [&](const ControlStylePart& p) {
                if (p.declared && p.valid && !p.none && !p.color)
                    if (Texture* tex = const_cast<VNScriptState*>(this)->m_assets.get(p.value))
                        half = std::max(half, tex->getWidth() * 0.5f);
            };
            scan(bc.controlStyle->thumb);
            for (const auto& v : bc.controlStyle->variants)
                if (v.first.rfind("thumb_", 0) == 0) scan(v.second);
            return half;
        }

        float controlThumbHalfHeight(const BoundControl& bc) const
        {
            float half = 11.0f;
            if (!bc.controlStyle) return half;
            // Gemela de `controlThumbHalfWidth`, y por la misma razon (`D-55`).
            if (bc.controlStyle->perillaAlturaDeclarada)
                half = bc.controlStyle->perillaAltura * 0.5f;
            auto scan = [&](const ControlStylePart& p) {
                if (p.declared && p.valid && !p.none && !p.color)
                    if (Texture* tex = const_cast<VNScriptState*>(this)->m_assets.get(p.value))
                        half = std::max(half, tex->getHeight() * 0.5f);
            };
            scan(bc.controlStyle->thumb);
            for (const auto& v : bc.controlStyle->variants)
                if (v.first.rfind("thumb_", 0) == 0) scan(v.second);
            return half;
        }

        // ── renderBar: slider clásico y paquete v3 (riel/relleno/perilla) ──
        void renderBar(const BoundControl& bc, bool locked)
        {
            float range = bc.vmax - bc.vmin;
            float fill = (range > 0.0f) ? (bc.value - bc.vmin) / range : 0.0f;
            if (fill < 0.0f) fill = 0.0f; else if (fill > 1.0f) fill = 1.0f;
            const ControlBarGeometry g = resolveControlBarGeometry(bc);
            const ControlStylePart* track = g.track;
            const ControlStylePart* fillPart = g.fill;
            const ControlStylePart* thumb = g.thumb;
            const float trackH = g.trackH;
            const float ty = g.trackY;
            bool trackDrawn = false;
            if (track && track->declared && !track->none)
                trackDrawn = drawControlPart(track, bc.x, ty, bc.width, trackH,
                    locked ? 0.55f : 1.0f);
            if (!trackDrawn && (!track || !track->declared || !track->none || !track->valid))
                draw2d::drawRect(bc.x, ty, bc.width, trackH, 0.20f, 0.22f, 0.28f, 0.85f);
            if (fillPart && fillPart->declared && !fillPart->none)
            {
                bool drawn = false;
                if (fillPart->valid && !fillPart->color && !fillPart->value.empty())
                    if (Texture* tex = m_assets.get(fillPart->value))
                    {
                        draw2d::drawSpriteFrame(*tex, bc.x, ty, bc.width * fill, trackH,
                            0.0f, 1.0f, fill, 0.0f, locked ? 0.55f : 1.0f);
                        drawn = true;
                    }
                if (!drawn) drawn = drawControlPart(fillPart, bc.x, ty, bc.width * fill,
                    trackH, locked ? 0.55f : 1.0f);
                if (!drawn) draw2d::drawRect(bc.x, ty, bc.width * fill, trackH,
                    locked ? 0.38f : 0.31f, locked ? 0.38f : 0.76f,
                    locked ? 0.42f : 0.86f, locked ? 0.55f : 0.95f);
            }
            else if (!fillPart || !fillPart->declared)
                draw2d::drawRect(bc.x, ty, bc.width * fill, trackH,
                    locked ? 0.38f : 0.31f, locked ? 0.38f : 0.76f,
                    locked ? 0.42f : 0.86f, locked ? 0.55f : 0.95f);
            // ── `BORDE_Y_FORMAS_01` · `A1`+`B1` · EL BORDE DEL RIEL ─────────
            //
            // Va DESPUES del relleno, a proposito. El relleno se pinta sobre la
            // MISMA caja `(bc.x, ty, bc.width, trackH)` recortada por `fill`,
            // asi que un borde dibujado antes quedaria comido por la mitad en la
            // zona llena. El borde es el CONTORNO DE LA CAJA DEL RIEL, y un
            // contorno va arriba de lo que contiene.
            //
            // `B1`: UN solo sitio cubre los dos caminos de pintado -la pieza con
            // estilo y el riel de fallback-, porque los dos usan esa misma caja.
            // Por eso el borde son 2 sitios de cableado y no 5.
            //
            // La frontera arte/primitiva (la ley que propuso Codex en
            // `HC-BOR-07` y firmo el fundador): estas claves visten la PRIMITIVA
            // resuelta. Si la pieza resolvio a TEXTURA, sus pixeles ya poseen su
            // borde y no se les dibuja un rectangulo limpio encima; si resolvio
            // a `ninguno`, no hay caja que vestir y no aparece un rectangulo
            // fantasma solo por haber declarado el color.
            //
            // Y `drawRectBorder` pinta las cuatro bandas HACIA ADENTRO de la
            // caja (`Draw2D.h:212-220`), asi que el borde NO agranda el riel:
            // ni el hit-test ni el arrastre se enteran, y la verdad espacial
            // sigue siendo una sola (§7.100).
            auto piezaEsTextura = [&](const ControlStylePart* p) {
                return p && p->declared && p->valid && !p->none && !p->color &&
                       !p->value.empty() && m_assets.get(p->value) != nullptr;
            };
            auto piezaEsNinguno = [&](const ControlStylePart* p) {
                return p && p->declared && p->none;
            };
            if (bc.controlStyle && bc.controlStyle->rielBordeColorDeclarada &&
                !piezaEsTextura(track) && !piezaEsNinguno(track))
            {
                float br = 1, bg = 1, bb = 1;
                EclipParser::parseHexColor(bc.controlStyle->rielBordeColor, br, bg, bb);
                draw2d::drawRectBorder(bc.x, ty, bc.width, trackH, br, bg, bb,
                    locked ? 0.55f : 1.0f, bc.controlStyle->rielBordeGrosor);
            }
            float knobW = g.thumbW, knobH = g.thumbH;
            // ── `D-55` · EL CENTRO SALE DEL EJE, NO DE `bc.x + ancho*fill` ───
            // Los dos daban lo mismo mientras el eje no se podia mover, y por
            // eso esta linea es identidad byte a byte sin `perilla_en_extremos
            // dentro`. Con `dentro`, es la unica forma de que el pixel y el
            // clic sigan hablando del MISMO numero: `applyControlDrag` ya leia
            // `valueStart`/`valueEnd`, y el render los ignoraba.
            //
            // NOTA de alcance: el RELLENO sigue midiendo `bc.width * fill` y no
            // se movio en este lote. Con `dentro` eso lo deja hasta medio ancho
            // de perilla desfasado del pomo en los extremos. No se toco porque
            // cambiar como se ve el relleno no lo firmo nadie, y el look de
            // Ren'Py -que es el caller de este lote- lleva `relleno ninguno`.
            // Ficha: `RELLENO-NO-SIGUE-LA-PERILLA-CON-DENTRO`.
            const float centro = g.valueStart + (g.valueEnd - g.valueStart) * fill;
            float kx = centro - knobW * 0.5f;
            float ky = ty + trackH * 0.5f - knobH * 0.5f;
            // ── `FORMAS_01` · LA SILUETA DE LA PERILLA ──────────────────────
            // Viste los TRES caminos de pintado -el de estilo, el de `locked` y
            // el normal-, o sea tambien el pomo de FALLBACK. Eso NO es de mas:
            // es la misma ley que `B1` firmo ayer para el borde -*decora la caja
            // de la pieza AUNQUE la pieza no este declarada*-, y sin ella un
            // creador que escribe `perilla_forma rombo` sin declarar color ve un
            // rectangulo y no tiene como saber por que.
            const bool perillaRombo = bc.controlStyle &&
                bc.controlStyle->perillaForma == ControlFormaPieza::Rombo;
            // La caja `(kx, ky, knobW, knobH)` es la MISMA para los tres, igual
            // que en el borde. Por eso esto es un solo sitio y no tres.
            auto pintaPomo = [&](float pr, float pg, float pb, float pa) {
                if (perillaRombo)
                    draw2d::drawCircle(kx + knobW * 0.5f, ky + knobH * 0.5f,
                        knobW * 0.5f, pr, pg, pb, pa, 4, knobH * 0.5f);
                else
                    draw2d::drawRect(kx, ky, knobW, knobH, pr, pg, pb, pa);
            };
            if (thumb && thumb->declared && thumb->none) {}
            else if (thumb && thumb->declared && !thumb->none &&
                drawControlPart(thumb, kx, ky, knobW, knobH,
                    locked ? 0.70f : 1.0f, perillaRombo)) {}
            else if (locked)
                pintaPomo(0.50f, 0.50f, 0.54f, 0.70f);
            else pintaPomo(0.90f, 0.95f, 1.0f, 1.0f);
            // ── `BORDE_Y_FORMAS_01` · EL BORDE DE LA PERILLA ────────────────
            // Misma ley y misma caja compartida: `(kx, ky, knobW, knobH)` es la
            // que usan los TRES caminos de pintado del pomo -el de estilo, el
            // de `locked` y el normal-, asi que un solo sitio los cubre a los
            // tres. Es el caller de DDLC: perilla clara con contorno fino.
            if (bc.controlStyle && bc.controlStyle->perillaBordeColorDeclarada &&
                !piezaEsTextura(thumb) && !piezaEsNinguno(thumb))
            {
                float br = 1, bg = 1, bb = 1;
                EclipParser::parseHexColor(bc.controlStyle->perillaBordeColor, br, bg, bb);
                // `FORMAS_01`: el contorno SIGUE a la silueta, y no por simetria
                // -- si el relleno fuera rombo y el borde rectangulo, una perilla
                // con borde declarado saldria con un rombo adentro de una caja.
                if (perillaRombo)
                    draw2d::drawCircleBorder(kx + knobW * 0.5f, ky + knobH * 0.5f,
                        knobW * 0.5f, br, bg, bb, locked ? 0.70f : 1.0f, 4,
                        bc.controlStyle->perillaBordeGrosor, knobH * 0.5f);
                else
                    draw2d::drawRectBorder(kx, ky, knobW, knobH, br, bg, bb,
                        locked ? 0.70f : 1.0f, bc.controlStyle->perillaBordeGrosor);
            }
        }

        // ── renderToggle (D1): pill de 2 posiciones (knob izq=off / der=on) ──
        // Reusa los primitivos del slider: riel coloreado por estado + knob que
        // snapea a los extremos. Sin texto baked-in (el creador compone su label
        // con `texto`). Gris si locked. Esquinas rectas por ahora; redondeo vía
        // estilo=/PNG se fichan aparte.
        void renderToggle(const BoundControl& bc, bool on, bool locked)
        {
            const float pillH = kPillH;
            float ty = bc.y;
            // riel: gris si locked; cian si on; oscuro si off
            if (locked)
                draw2d::drawRect(bc.x, ty, bc.width, pillH, 0.30f, 0.30f, 0.34f, 0.65f);
            else if (on)
                draw2d::drawRect(bc.x, ty, bc.width, pillH, 0.31f, 0.76f, 0.86f, 0.95f);
            else
                draw2d::drawRect(bc.x, ty, bc.width, pillH, 0.20f, 0.22f, 0.28f, 0.90f);
            // knob: izquierda=off, derecha=on
            const float pad = 3.0f;
            const float knobW = pillH - pad * 2.0f;
            const float knobH = knobW;
            float kx = on ? (bc.x + bc.width - pad - knobW) : (bc.x + pad);
            float ky = ty + pad;
            if (locked)
                draw2d::drawRect(kx, ky, knobW, knobH, 0.55f, 0.55f, 0.58f, 0.80f);
            else
                draw2d::drawRect(kx, ky, knobW, knobH, 0.92f, 0.96f, 1.0f, 1.0f);
        }

        // ── renderArrows (Enum): ◄ label ► — catálogo nombrado. Label de
        // `bc.opciones`, que el proveedor llenó en el bind: este render ya NO
        // sabe cuál Enum está dibujando (CP5, fork 1b). Flechas atenuadas en los extremos
        // (clamp visual). Texto centrado con m_nameRenderer (patrón del toast).
        void renderArrows(const BoundControl& bc, bool locked)
        {
            const float h = kArrowH;
            const float y = bc.y;
            const int idx = (int)(bc.value + 0.5f);
            const bool atMin = (bc.value <= bc.vmin + 0.001f);
            const bool atMax = (bc.value >= bc.vmax - 0.001f);

            // Las _noBatch (rect + texto) requieren un batch abierto, como el
            // toast (§:1644). draw2d::drawRect auto-gestiona el suyo, pero el
            // texto _noBatch no → sin este begin/end, flechas y label no se
            // renderizan aunque el rect sí (§7.38: setupProjection no lo regala).
            draw2d::g_batch->begin();
            draw2d::drawRect_noBatch(bc.x, y, bc.width, h, 0.16f, 0.18f, 0.22f, 0.75f);

            if (m_hasNameFont)
            {
                const float lh = m_nameRenderer.getLineHeight();
                const float ty = y + (h - lh) * 0.5f;

                auto drawGlyph = [&](const char* g, float zx, bool dim) {
                    float gw = m_nameRenderer.measureWidth(g);
                    float gx = zx + (kArrowW - gw) * 0.5f;
                    float a = (dim || locked) ? 0.30f : 0.95f;
                    m_nameRenderer.drawText_noBatch(g, gx, ty, 0.92f, 0.96f, 1.0f, a);
                    };
                drawGlyph("<", bc.x, atMin);
                drawGlyph(">", bc.x + bc.width - kArrowW, atMax);

                std::string label = (idx >= 0 && idx < (int)bc.opciones.size())
                    ? bc.opciones[idx] : std::string("-");
                float lw = m_nameRenderer.measureWidth(label);
                float lx = bc.x + (bc.width - lw) * 0.5f;
                float la = locked ? 0.55f : 1.0f;
                m_nameRenderer.drawText_noBatch(label, lx, ty, 0.92f, 0.96f, 1.0f, la);
            }
            draw2d::g_batch->end();
        }

        // ── boxCount/boxCell (Enum, tipo=cajas): geometría ÚNICA de las celdas ─
        // N sale del modelo (vmin/vmax, vstep=1). boxCell da x+ancho de la CELDA
        // i (zona de hit/layout); el dibujo encoge kBoxPad dentro. Single source
        // compartido por render (CP1) + hit-test + input (CP2) — §7.148.
        int boxCount(const BoundControl& bc) const
        {
            int n = (int)(bc.vmax - bc.vmin + 0.5f) + 1;
            return n < 1 ? 1 : n;
        }
        void boxCell(const BoundControl& bc, int i, float& cx, float& cw) const
        {
            int n = boxCount(bc);
            cw = (n > 0) ? bc.width / (float)n : bc.width;
            cx = bc.x + (float)i * cw;
        }

        // ── renderBoxes (Enum, tipo=cajas): fila de N cajas, la del índice
        // actual resaltada (cian). Celdas de ancho igual (Fork A, vía boxCell);
        // cada caja encoge kBoxPad → aire visual. Label centrado por caja; si no
        // cabe se encima (Fork A no recorta → usá ancho= holgado o dominio corto).
        // Labels de `bc.opciones`, como renderArrows (CP5, fork 1b). _noBatch
        // exige batch abierto (§7.152). Clic-directo selecciona en CP2.
        void renderBoxes(const BoundControl& bc, bool locked)
        {
            const float h = kBoxH;
            const float y = bc.y;
            const int sel = (int)(bc.value + 0.5f);
            const int n = boxCount(bc);

            draw2d::g_batch->begin();

            const bool haveFont = m_hasNameFont;
            const float lh = haveFont ? m_nameRenderer.getLineHeight() : 0.0f;
            const float ty = y + (h - lh) * 0.5f;

            for (int i = 0; i < n; ++i)
            {
                float cx, cw;
                boxCell(bc, i, cx, cw);
                const float bx = cx + kBoxPad;
                const float bw = cw - kBoxPad * 2.0f;
                const bool isSel = (i == sel);

                if (locked)
                    draw2d::drawRect_noBatch(bx, y, bw, h, 0.24f, 0.24f, 0.28f, 0.70f);
                else if (isSel)
                    draw2d::drawRect_noBatch(bx, y, bw, h, 0.31f, 0.76f, 0.86f, 0.95f);
                else
                    draw2d::drawRect_noBatch(bx, y, bw, h, 0.16f, 0.18f, 0.22f, 0.85f);

                if (haveFont)
                {
                    std::string label = (i >= 0 && i < (int)bc.opciones.size())
                        ? bc.opciones[i] : std::to_string(i);
                    float lw = m_nameRenderer.measureWidth(label);
                    float lx = bx + (bw - lw) * 0.5f;
                    float la = locked ? 0.55f : 1.0f;
                    if (isSel && !locked)   // texto oscuro sobre la caja cian
                        m_nameRenderer.drawText_noBatch(label, lx, ty, 0.10f, 0.12f, 0.16f, la);
                    else
                        m_nameRenderer.drawText_noBatch(label, lx, ty, 0.92f, 0.96f, 1.0f, la);
                }
            }
            draw2d::g_batch->end();
        }

        // ── hitTestControl (CP3): índice del control bajo (vx,vy), o -1 ───────
        // Hit-area por tipo: el pill ocupa su alto real (kPillH); la barra usa
        // track + agarre generoso (kBarGrab). Mismas constantes que el render.
        int hitTestControl(float vx, float vy) const
        {
            for (int i = 0; i < (int)m_controls.size(); ++i)
            {
                const auto& bc = m_controls[i];
                const ControlBarGeometry g = (bc.render == CtrlRender::Bar)
                    ? resolveControlBarGeometry(bc) : ControlBarGeometry{};
                const float half = (bc.render == CtrlRender::Bar) ? g.hitHalfX : 0.0f;
                // `D-55`: para una barra, el rango en X sale del EJE DE VALOR.
                // Identidad exacta sin `dentro` (ahi el eje ES `bc.x..bc.x+ancho`);
                // con `dentro`, el area clickeable sigue siendo la barra entera,
                // porque `valueStart - half` vuelve a caer justo en `bc.x`.
                // Los otros renders no tienen eje y conservan su rango de hoy.
                const float ejeIni = (bc.render == CtrlRender::Bar) ? g.valueStart : bc.x;
                const float ejeFin = (bc.render == CtrlRender::Bar) ? g.valueEnd : bc.x + bc.width;
                if (vx < ejeIni - half || vx > ejeFin + half) continue;
                if (bc.render == CtrlRender::Toggle)
                {
                    if (vy >= bc.y && vy <= bc.y + kPillH) return i;
                }
                else if (bc.render == CtrlRender::Arrows)
                {
                    if (vy >= bc.y && vy <= bc.y + kArrowH) return i;
                }
                else if (bc.render == CtrlRender::Boxes)
                {
                    if (vy >= bc.y && vy <= bc.y + kBoxH) return i;
                }
                else
                {
                    const float grab = std::max(kBarGrab, std::max(g.trackH * 0.5f,
                        g.hitHalfY - g.trackH * 0.5f));
                    if (vy >= g.trackY - grab && vy <= g.trackY + g.trackH + grab) return i;
                }
            }
            return -1;
        }

        // ════════════════════════════════════════════════════════════════════
        // `CP9b`: EL WIDGET DE TECLA - registro, render, hit-test y captura
        // ════════════════════════════════════════════════════════════════════
        //
        // Geometria compartida render↔hit-test, igual que sus hermanos (§7.72).
        static constexpr float kTeclaH = 36.0f;

        // ── cmdControlTecla: registra un `control tecla.<dominio>.<accion>` ──
        //
        // **No hay clave nueva ni comando nuevo, y eso fue el fork 1b.** El §7
        // del contrato firma `control accion=X dominio=Y tipo=tecla`, y medido:
        // `tipo=` YA es el override de render (`toggle`/`barra`/`flechas`/
        // `cajas`) y `accion=` YA es de `ranura` (`cargar`/`guardar`/`borrar`).
        // Dos de las tres palabras estaban ocupadas.
        //
        // La forma que quedo reusa el string que el jugador escribe en
        // `settings.eclips` y que el creador ya pinta con `${_tecla.vn.avanzar}`
        // — una estructura y tres caras (§7.205). El discriminador *"arg1
        // empieza con `tecla.`/`key.`"* tiene la forma de `color-solido`, con su
        // frontera ya medida (§7.384).
        void cmdControlTecla(const EclipCmd& cmd)
        {
            const InputClaveJugador c = inputPartirClaveJugador(cmd.arg1);
            if (!c.formada)
            {
                std::cerr << "[control] `" << cmd.arg1 << "` no nombra una "
                    "accion: la forma es `tecla.<dominio>.<accion>` (tres "
                    "partes). Control ignorado." << std::endl;
                return;
            }

            const InputDominioRes d = inputDominioDe(c.dominio);
            if (d.estado == InputDominioEstado::NoRemapeable)
            {
                // `DEV-SIN-DUENO`, cobrada por CUARTA vez. La razon es la de
                // `CP9a` y es mas fuerte aca: sus teclas se compilan fuera en
                // build de export, asi que un widget que las remapea escribe en
                // el `settings.eclips` del jugador una linea que su binario no
                // va a leer.
                std::cerr << "[control] el dominio `dev` no se remapea: sus "
                    "teclas se compilan fuera en build de export. Control "
                    "ignorado." << std::endl;
                return;
            }
            if (d.estado == InputDominioEstado::Desconocido)
            {
                std::cerr << "[control] dominio desconocido: '" << c.dominio
                    << "' (validos: vn, juego/game). Control ignorado."
                    << std::endl;
                return;
            }

            const std::string verbo = inputVerboDe(c.verbo, d.canonico);
            if (verbo.empty())
            {
                std::cerr << "[control] `" << c.verbo << "` no es una accion de `"
                    << d.canonico << "`. Control ignorado." << std::endl;
                return;
            }

            TeclaWidget tw;
            tw.dominio   = d.canonico;
            tw.verbo     = verbo;
            // La grafia que el CREADOR escribio decide la palabra del vacio,
            // igual que en el display (fork 4a de `CP9a`, §7.492).
            tw.enIngles  = (cmd.arg1.rfind("key.", 0) == 0);
            tw.x         = cmd.layerX;
            tw.y         = cmd.layerY;
            tw.width     = (cmd.controlWidth >= 0.0f) ? cmd.controlWidth : 240.0f;
            tw.styleName = cmd.styleName;
            if (!tw.styleName.empty())
                voiceUnsupportedControlSurface("tecla", tw.styleName);
            tw.scopeOwner = m_activeScopeOwner;
            m_teclaWidgets.push_back(tw);
        }

        // ── La palabra que la fila muestra HOY ───────────────────────────────
        // Sin cache: `teclasDe` contesta despues de las tres capas, y un rebind
        // la mueve en el acto.
        std::string textoDeFila(const TeclaWidget& tw) const
        {
            // DI-7: el idioma del JUGADOR, no `tw.enIngles` (la grafía del
            // creador). Ese recibo sigue existiendo y lo sigue usando la voz.
            return inputDisplayDeTeclas(
                m_input->teclasDe(tw.verbo, tw.dominio), eclips::langActivo());
        }

        void renderTeclaWidgets()
        {
            if (m_teclaWidgets.empty()) return;

            draw2d::g_batch->begin();
            for (int i = 0; i < (int)m_teclaWidgets.size(); ++i)
            {
                const TeclaWidget& tw = m_teclaWidgets[i];
                const bool capturando = (m_capturaTecla == i);
                const bool focado     = (m_focoTecla == i);

                // Tres estados y tres fondos: reposo, focado (hover del mouse O
                // foco del teclado - convergen en el mismo campo, §9) y
                // capturando. El ultimo es el unico que cambia el TEXTO.
                float r = 0.16f, g = 0.18f, b = 0.22f, a = 0.75f;
                if (capturando) { r = 0.42f; g = 0.30f; b = 0.12f; a = 0.92f; }
                else if (focado) { r = 0.24f; g = 0.28f; b = 0.36f; a = 0.90f; }
                draw2d::drawRect_noBatch(tw.x, tw.y, tw.width, kTeclaH, r, g, b, a);

                if (m_hasNameFont)
                {
                    const float lh = m_nameRenderer.getLineHeight();
                    const float ty = tw.y + (kTeclaH - lh) * 0.5f;
                    const std::string txt = capturando
                        ? (tw.enIngles ? std::string("press a key...")
                                       : std::string("presiona una tecla..."))
                        : textoDeFila(tw);
                    const float w = m_nameRenderer.measureWidth(txt);
                    const float tx = tw.x + (tw.width - w) * 0.5f;
                    m_nameRenderer.drawText_noBatch(txt, tx, ty,
                                                    0.92f, 0.96f, 1.0f, 1.0f);
                }
            }
            draw2d::g_batch->end();
        }

        int hitTestTeclaWidget(float vx, float vy) const
        {
            for (int i = 0; i < (int)m_teclaWidgets.size(); ++i)
            {
                const TeclaWidget& tw = m_teclaWidgets[i];
                if (vx < tw.x || vx > tw.x + tw.width) continue;
                if (vy >= tw.y && vy <= tw.y + kTeclaH) return i;
            }
            return -1;
        }

        // ── La captura ───────────────────────────────────────────────────────
        //
        // `m_capturaArmada` es la ley *"el evento que ABRE no es el que se
        // captura"*, y no es paranoia: `avanzar` trae `ESPACIO` **y `CLIC`**, o
        // sea las dos formas de activar la fila. Sin esta guarda, abrir la fila
        // de "Avanzar" con ESPACIO te ligaria ESPACIO a lo que ibas a remapear,
        // y abrirla con el mouse te ligaria el click.
        void abrirCaptura(int idx)
        {
            if (idx < 0 || idx >= (int)m_teclaWidgets.size()) return;
            m_capturaTecla  = idx;
            m_capturaArmada = false;   // se arma en el PROXIMO evento
        }

        void cerrarCaptura()
        {
            m_capturaTecla  = -1;
            m_capturaArmada = false;
        }

        // ── El rebind: la unica escritura, y pasa por la ley de `CP9b` ───────
        //
        // `nombreNuevo` vacio = vaciar (fork 7a, `SUPR`): la accion queda sin
        // tecla y el display dice `ninguna`, que es la regla Cuphead del §12
        // llegando por fin a su estado.
        void aplicarRebind(int idx, const std::string& nombreNuevo)
        {
            if (idx < 0 || idx >= (int)m_teclaWidgets.size()) return;
            const TeclaWidget& tw = m_teclaWidgets[idx];

            // 1 · la lista nueva (fork 6a: la primera cambia, el resto queda)
            const std::vector<std::string> nueva = inputListaConPrimera(
                m_input->teclasDe(tw.verbo, tw.dominio), nombreNuevo);

            std::string valor;
            if (nueva.empty())
                valor = tw.enIngles ? "none" : "ninguna";
            else
                for (size_t i = 0; i < nueva.size(); ++i)
                {
                    if (i) valor += ", ";
                    valor += nueva[i];
                }

            // 2 · el par crudo, con la ley que sabe cual linea tocar
            const InputRebindRes r = inputRebindJugador(
                Settings::teclas(), tw.dominio, tw.verbo, valor);
            Settings::setTeclas(r.pares);

            // 3 · re-parsear y aplicar. Es `G-6` al pie de la letra: *"el camino
            //     del load corrido otra vez"*. El `+ persist` NO va aca — corre
            //     en `onExit` con rol Menu (`L-4`), que es la firma de `G-6`:
            //     persistir al cerrar, no por cambio.
            const InputCapaJugador capa = inputParsearCapaJugador(r.pares);
            for (const InputAviso& a : capa.avisos)
                std::cerr << inputVozDelAviso(a, "settings.eclips") << std::endl;

            m_input->setCapaJugador(capa.tabla);

            // 4 · la red §8. Sale gratis -`aplicarCascada` la corre siempre-,
            //     pero la VOZ es de quien cablea, igual que en
            //     `Engine::aplicarTeclasDelJugador`. La ley no se duplica; el
            //     bucle que la imprime, si, y se declara.
            for (const std::string& dom : m_input->dominiosRescatados())
            {
                InputAviso red;
                red.causa   = InputAvisoCausa::RedDeSeguridad;
                red.dominio = dom;
                std::cerr << inputVozDelAviso(red, "teclas") << std::endl;
            }

            // 5 · las colisiones, que el §13 promete y no existian. **Habla y
            //     deja pasar**: compartir tecla es legal y a veces querido
            //     (`ARRIBA` sirve al historial y al foco), y quien desempata es
            //     la fase. El aviso existe porque hasta hoy fabricar una
            //     colision pedia editar un archivo a mano, y esto la pone a dos
            //     clicks.
            if (!nueva.empty())
            {
                const std::vector<std::string> comparten =
                    inputVerbosQueComparten(
                        m_input->verbosDe(inputBindDe(nueva[0]), tw.dominio),
                        tw.verbo);
                if (!comparten.empty())
                {
                    std::cerr << "[teclas] `" << nueva[0] << "` ahora sirve a `"
                        << tw.verbo << "` y tambien a";
                    for (const std::string& v : comparten)
                        std::cerr << " `" << v << "`";
                    std::cerr << " en el dominio `" << tw.dominio
                        << "`. Es legal -desempata la fase-, pero si no era lo "
                        "que querias, remapea una de las dos." << std::endl;
                }
            }
        }

        // Zona de flecha bajo vx para un Arrows: -1 izq, +1 der, 0 centro (label).
        // §7.148: mismas kArrowW que renderArrows.
        int arrowDir(const BoundControl& bc, float vx) const
        {
            if (vx >= bc.x && vx <= bc.x + kArrowW) return -1;
            if (vx >= bc.x + bc.width - kArrowW && vx <= bc.x + bc.width) return +1;
            return 0;
        }

        // Caja bajo vx para un Boxes: índice 0..N-1, o -1 fuera del ancho. Mapea
        // a la CELDA completa (sin kBoxPad) → sin zonas muertas entre cajas; el
        // padding es solo dibujo (§7.148: hit más generoso que el render = diseño).
        int boxIndexAt(const BoundControl& bc, float vx) const
        {
            if (vx < bc.x || vx > bc.x + bc.width) return -1;
            int n = boxCount(bc);
            float cw = (n > 0) ? bc.width / (float)n : bc.width;
            if (cw <= 0.0f) return -1;
            int i = (int)((vx - bc.x) / cw);
            if (i < 0) i = 0;
            if (i >= n) i = n - 1;
            return i;
        }

        // ═════════════════════════════════════════════════════════════════════
        // `CP6` — EL ANILLO DE FOCO UNIFICADO (`FocusRing.h` ley 4)
        // ═════════════════════════════════════════════════════════════════════
        //
        // El §9 dice *"el state construye la lista de focables del momento"*, y
        // acá es donde eso pasa. `CP5` dejó el anillo adentro de `VNDialogBox`
        // porque sus focables eran los choices, que son de la caja; los de `CP6`
        // —`control`, `ranura`, `enlace`— viven acá y en `SceneLayer.h`.
        //
        // **Y conviven, contra lo que la nota de `CP5` daba por sentado.**
        // `linksFrozen` sí congela los enlaces en `WaitingChoice`, pero el
        // control y la ranura son phase-free A PROPÓSITO (ver el comentario del
        // grab, arriba), y las tres pantallas de la aceptación son justo el caso:
        // `config.ecl` (6 control + 1 choice), `load.ecl` (1 ranura + 1 choice),
        // `pausa.ecl` (4 ranuras + 4 choices).
        //
        // **`m_choiceHover` NO se toca.** Conserva su significado y sus tres
        // consumidores; lo que recibe es su segmento ya resuelto
        // (`setFocoChoice`). Un índice gemelo era peor que el bug (§7.486).

        // ════════════════════════════════════════════════════════════════════
        // `CP2a` de `DISENO_MUTAR_PALABRAS` — EL POOL ABIERTO
        // ════════════════════════════════════════════════════════════════════
        //
        // Tres helpers y una razon comun: **un pool es un modal con N salidas, y
        // el aviso (`modalidad-aviso`) tiene UNA**. El comentario de la seccion
        // 0.5 pedia revisar esto el dia que llegara un modal con varios enlaces
        // (*"asume aviso = una salida"*) — llego, y lo que cambia no es la ley
        // del aviso sino quien resuelve la espera: aca la resuelve EL BLOQUE.
        //
        // Si el pick cayera al avance plano como cae el OK del aviso, el
        // `m_seen->mark(m_currentLineKey)` del `update` marcaria como vista una
        // linea que no es de este comando, y esa invariante esta escrita ahi
        // mismo (*"WaitingInput solo lo ponen Dialogo/Narrador"*).

        bool esPalabraDelPool(const SceneElement* le) const
        {
            if (!m_palabrasActivas || !le) return false;
            for (const std::string& id : m_palabrasIds)
                if (id == le->name) return true;
            return false;
        }

        // Apaga las palabras del pool. **`setVisible(false)` y no borrado**, y no
        // es pereza: `SceneLayerManager` no tiene forma de borrar UNA capa —solo
        // `clear()`, que se llevaria el fondo y los personajes—. Alcanza porque
        // los tres filtros del interactivo ya respetan la visibilidad
        // (`esFocable`, `updateInteractiveHover` y `hoveredInteractive`), asi que
        // una palabra apagada queda INERTE: no se dibuja, no se focaliza, no se
        // clickea. Costo declarado: las capas siguen en `m_elements` y en el
        // save, acotado por BLOQUE (los ids son deterministas, asi que una
        // re-entrada las parcha) y no por tiempo de juego. Ficha
        // `CAPA-SIN-BORRAR-UNA`.
        void limpiarPalabras()
        {
            for (const std::string& id : m_palabrasIds)
            {
                if (SceneElement* le = m_sceneLayers.get(id))
                {
                    le->interactive = false;
                    le->hovered = false;
                }
                m_sceneLayers.setVisible(id, false);
            }
        }

        // Cierra el pool y devuelve el guion al comando de al lado. Espejo exacto
        // de lo que `ChoiceMade` hace para un `elegir` resuelto por mutacion
        // (`m_cmdIndex` apunta AL bloque mientras se espera, por eso avanza uno).
        void resolverPalabras()
        {
            limpiarPalabras();
            m_palabrasIds.clear();
            m_palabrasActivas = false;
            m_modalRequired = false;
            // El anillo no puede quedar parado sobre un fantasma: las palabras
            // que lo poblaban acaban de apagarse.
            m_focoPlano = -1;
            m_focoDelPuntero = true;
            ++m_cmdIndex;
            m_phase = VNPhase::Executing;
        }

        // El desenlace de un pick. Consume SIEMPRE (un click o un ESPACIO dentro
        // del pool no puede caer al avance de abajo), y reparte por veredicto
        // **igual que sus tres hermanas** — o sea sin ley nueva:
        //
        //   Salto                     `jumpToScene` ya movio escena y cmdIndex, y
        //                             ya apago el pool. Aca no se toca nada mas.
        //   Mutacion / MutacionRota   el "continuar" del `F2`: el pool se cierra
        //   destino vacio             y el guion sigue en el comando siguiente.
        //   VerboShell / NoResuelto   el pool SIGUE ABIERTO, igual que un
        //                             `elegir` cuyo destino era `guardar` o un
        //                             typo. Se consume el click y se espera otro.
        bool picarPalabra(SceneElement* le)
        {
            const std::string destino = le->target;   // copia: un salto lo invalida

            // ── `${_palabra}` / `${_palabra_var}` — `CP3`, `F4`, fork 2b ─────
            //
            // **Se pueblan ANTES de despachar, y el orden no es estilo:** un
            // ítem que navega (`"..." -> rotura`) hace que `dispatchTarget`
            // salte de escena ahí nomás, y la escena de destino tiene que poder
            // leer QUÉ palabra la trajo. Escribirlas después las dejaría vacías
            // justo en la palabra-que-rompe-todo, que es el caso que el `F4`
            // nombra por su nombre.
            //
            // **Viven en `m_variables` y no en un campo del state**, o sea la
            // forma de `_tema_activo` ([:9285], «se guarda en saves») y no la de
            // `_hora`/`_saves`. Las dos formas están vivas en el motor y
            // difieren exactamente en esto: así el valor sobrevive a un load,
            // igual que ya sobrevive el efecto del mismo pick (`natsuki + 1`).
            // Que se guarde el efecto y se pierda la causa es la asimetría que
            // el jugador siente al cargar. Y `resolveVariable` las alcanza por
            // su fall-through, sin una rama nueva.
            //
            // `le->text` y no el label crudo: es lo que el jugador VIO — ya
            // traducido e interpolado por `cmdTexto` ([:6507]) —, que es lo que
            // un `si _palabra == "amor"` quiere comparar.
            m_variables["_palabra"] = le->text;

            const eclips::MutaTriple mt = eclips::mutaClasificarTarget(destino);
            m_variables["_palabra_var"] =
                (mt.forma == eclips::MutaForma::Valida) ? mt.var : std::string();

            if (destino.empty()) { resolverPalabras(); return true; }

            const eclips::TargetVeredicto ver = dispatchTarget(
                destino, eclips::system_flow::PantallaCara::Palabras);

            if (ver == eclips::TargetVeredicto::Salto) return true;
            if (ver == eclips::TargetVeredicto::Mutacion ||
                ver == eclips::TargetVeredicto::MutacionRota)
            {
                resolverPalabras();
                return true;
            }
            return true;
        }

        eclips::FocoCuentas focoCuentas() const
        {
            eclips::FocoCuentas c;
            if (m_phase == VNPhase::WaitingInterface)
            {
                // UI-hold: el anillo cruza las familias vivas del menú, pero
                // no agrega choices ni convierte la espera en diálogo.
                c.controles = (int)m_controls.size();
                c.teclas    = (int)m_teclaWidgets.size();
                c.ranuras   = (int)m_slotWidgets.size();
                c.enlaces   = m_sceneLayers.interactiveCount();
            }
            else if (m_phase == VNPhase::WaitingChoice)
            {
                c.controles = (int)m_controls.size();
                c.teclas    = (int)m_teclaWidgets.size();   // `CP9b`
                c.ranuras   = (int)m_slotWidgets.size();
                c.choices   = m_dialogBox.choiceCount();
            }
            else if (m_phase == VNPhase::WaitingInput && m_modalRequired)
            {
                // `modalidad-aviso`: el avance está sellado
                // (`vnAcceptsDialogueAdvance`) y la única salida es el enlace-OK,
                // que hasta este lote era mouse-only. Es el soft-lock que la
                // ley 7 de la ruta abre.
                c.enlaces = m_sceneLayers.interactiveCount();
            }
            return c;
        }

        // Escribe el foco del teclado sobre los MISMOS campos que el mouse usa,
        // uno por familia. De ahí sale el *"cero render nuevo"* del §9: el juice
        // del choice, el chrome de la ranura y el swap del enlace ya cuelgan de
        // estos campos. El `control` es la excepción y por eso tiene el suyo.
        void proyectarFoco()
        {
            const eclips::FocoCuentas c = focoCuentas();
            const eclips::Focable f = eclips::focoFocable(c, m_focoPlano);

            m_dialogBox.setFocoChoice(
                (f.familia == eclips::FocoFamilia::Choice) ? f.indice : -1);
            m_hoveredSlot =
                (f.familia == eclips::FocoFamilia::Ranura) ? f.indice : -1;
            m_focoControl =
                (f.familia == eclips::FocoFamilia::Control) ? f.indice : -1;
            m_focoTecla =
                (f.familia == eclips::FocoFamilia::Tecla) ? f.indice : -1;
            if (c.enlaces > 0)
                m_sceneLayers.setInteractiveFocus(
                    (f.familia == eclips::FocoFamilia::Enlace) ? f.indice : -1);
        }

        // ── `CP7`: EL TAP SINTÉTICO DE LA SONDA (fork 8a) ───────────────────
        //
        // Empuja a la cola de SDL los TRES eventos que un dedo genera, con
        // `which == SDL_TOUCH_MOUSEID`, que es lo que SDL marca cuando emula
        // mouse desde touch. `SDL_PushEvent` los deja al final de la cola, y
        // como `Engine::processEvents` polea con un `while`, **los tres entran
        // en el MISMO batch** — que es exactamente la forma que tiene un tap de
        // verdad y la que hace visible el defecto de tempo.
        //
        // No hay `SDL_MOUSEWHEEL` acá a propósito: un dedo no scrollea, y la
        // ley del puntero manda la rueda a `Ninguno` (`harness_puntero` 2.6).
        void empujarTapTactil(int wx, int wy)
        {
            SDL_Event t{};
            t.type = SDL_MOUSEMOTION;
            t.motion.which = SDL_TOUCH_MOUSEID;
            t.motion.x = wx; t.motion.y = wy;
            SDL_PushEvent(&t);

            SDL_Event d{};
            d.type = SDL_MOUSEBUTTONDOWN;
            d.button.which = SDL_TOUCH_MOUSEID;
            d.button.button = SDL_BUTTON_LEFT;
            d.button.state = SDL_PRESSED;
            d.button.clicks = 1;
            d.button.x = wx; d.button.y = wy;
            SDL_PushEvent(&d);

            SDL_Event u{};
            u.type = SDL_MOUSEBUTTONUP;
            u.button.which = SDL_TOUCH_MOUSEID;
            u.button.button = SDL_BUTTON_LEFT;
            u.button.state = SDL_RELEASED;
            u.button.clicks = 1;
            u.button.x = wx; u.button.y = wy;
            SDL_PushEvent(&u);
        }

        // ── `CP7`: LA DERIVACIÓN DEL PUNTERO, EN UN SOLO SITIO ──────────────
        //
        // Es lo que el `update` hacía con CINCO polleos de `SDL_GetMouseState`.
        // Ahora es una función pura de `m_puntero` + la geometría viva, y por
        // eso se puede correr **dos veces en el mismo frame sin cambiar nada**:
        // una al llegar el evento —que es lo que le da al click su posición
        // fresca, o sea la mitad de TEMPO (§7.488)— y otra en el `update`, que
        // es donde el resto del motor la espera.
        //
        // **La idempotencia no es elegancia: es la condición para llamarla dos
        // veces.** Si dejara de serlo, el hover pasaría a depender de cuántos
        // eventos entraron ese frame — que es el bug que este lote está
        // sacando, con otro disfraz.
        //
        // Todo lo que escribe va gateado por `m_focoDelPuntero` **salvo
        // `setMousePos`**, y esa excepción tiene dueño: de `m_mouseVX` cuelga
        // también `m_uiBtnHover` (los seis botones del chrome), que no es parte
        // del anillo y tiene que seguir al cursor mande quien mande. La caja ya
        // gatea su propio pull de choice por dentro (`VNDialogBox.cpp:326`).
        // Small seam for the control ownership calculation. It deliberately
        // contains only the production hit-test assignment; dialog, scene and
        // link hover consumers remain outside this fixture-safe probe.
#ifdef ECLIPS_TEST_SEAM
    public:
#endif
        void refrescarHoverControlDelPuntero()
        {
            if (m_focoDelPuntero)
                m_hoveredControl = m_puntero.presente
                    ? hitTestControl(m_puntero.vx, m_puntero.vy) : -1;
            else
                m_hoveredControl = -1;
        }
        void refrescarHoverDelPuntero()
        {
            // Con el puntero ausente esto entrega (-1,-1), y la guarda que la
            // caja YA escribe -`m_mouseVX > 0 && m_mouseVY > 0`- apaga sus dos
            // hit-tests sola. Ley 3: cero consumidores nuevos.
            m_dialogBox.setMousePos(m_puntero.vx, m_puntero.vy);

            if (m_focoDelPuntero)
                m_hoveredSlot = m_puntero.presente
                              ? hitTestSlotWidget(m_puntero.vx, m_puntero.vy)
                              : -1;
            refrescarHoverControlDelPuntero();

            // `CP9b`: el widget de tecla usa UN solo campo para el hover del
            // mouse y el foco del teclado, que es la convergencia del §9 sin
            // campo de mas. Va bajo el mismo gate `m_focoDelPuntero` que su
            // hermana de arriba: cuando manda el teclado, el mouse no le pisa
            // el foco (ley 2 de `FocusRing.h`), y cuando el puntero se ausenta
            // -un dedo que se levanta, `CP7`- el resalte se apaga en vez de
            // quedarse pegado.
            if (m_focoDelPuntero)
                m_focoTecla = m_puntero.presente
                            ? hitTestTeclaWidget(m_puntero.vx, m_puntero.vy)
                            : -1;

            // El gate de `linksFrozen` es el de `CP3`, sin cambios: un enlace
            // que no se puede activar tampoco se resalta (§7.100/§7.101).
            const bool linksFrozen =
                m_dialogBox.isShowingHistory() ||
                m_phase == VNPhase::WaitingChoice ||
                m_phase == VNPhase::WaitingQTE ||
                m_phase == VNPhase::WaitingClick ||
                m_phase == VNPhase::WaitingTextInput;

            if (linksFrozen)
            {
                m_sceneLayers.clearInteractiveHover(); // apaga swaps colgados
                m_enlaceHoverPrev.clear();             // re-entrar replays el sonido 1×
            }
            else if (m_focoDelPuntero)
            {
                if (m_puntero.presente)
                {
                    const float emnx =
                        (float)m_punteroWinX / (float)std::max(m_winW, 1);
                    const float emny =
                        (float)m_punteroWinY / (float)std::max(m_winH, 1);
                    m_sceneLayers.updateInteractiveHover(
                        m_puntero.vx, m_puntero.vy, emnx, emny,
                        m_hasFont ? &m_textRenderer : nullptr, &m_fontManager);
                }
                else
                {
                    // Sin puntero no hay enlace bajo él. Es el no-persist del
                    // §10.1, y acá se ve por qué un `clear` suelto no alcanzaba:
                    // esta rama corre CADA frame, así que la que decide es la
                    // ausencia, no un borrado de una vez.
                    m_sceneLayers.clearInteractiveHover();
                }
            }

            if (m_focoDelPuntero) m_focoPlano = focoBajoPuntero();
        }

        // Ley 2, del lado del puntero: cuál sería el índice bajo el cursor. El
        // orden de consulta replica el del click (`handleEvent`): control,
        // ranura, enlace, choice — first-hit-wins. Cada familia contesta con su
        // PROPIO hit-test vivo; acá no se replica geometría.
        int focoBajoPuntero()
        {
            const eclips::FocoCuentas c = focoCuentas();
            if (eclips::focoTotal(c) <= 0) return -1;

            if (c.controles > 0 && m_puntero.presente)
            {
                // `CP7`: era un polleo de `SDL_GetMouseState` -el quinto-. Con
                // el puntero ausente ni se pregunta: ningún control está bajo
                // un dedo que ya se levantó.
                const int h = hitTestControl(m_puntero.vx, m_puntero.vy);
                if (h >= 0)
                    return eclips::focoIndicePlano(
                        c, eclips::FocoFamilia::Control, h);
            }
            if (c.ranuras > 0 && m_hoveredSlot >= 0)
                return eclips::focoIndicePlano(
                    c, eclips::FocoFamilia::Ranura, m_hoveredSlot);
            if (c.enlaces > 0)
            {
                const int h = m_sceneLayers.hoveredInteractiveIndex();
                if (h >= 0)
                    return eclips::focoIndicePlano(
                        c, eclips::FocoFamilia::Enlace, h);
            }
            if (c.choices > 0)
            {
                const int h = m_dialogBox.currentChoiceHover();
                if (h >= 0)
                    return eclips::focoIndicePlano(
                        c, eclips::FocoFamilia::Choice, h);
            }
            return -1;
        }

        // El AJUSTE de un `control` focado (`ControlAdjust.h`). No rutea al
        // camino del click porque ese camino no existe sin puntero: tres de los
        // cuatro renders derivan el valor de la X del mouse.
        void ajustarControlFocado(int idx, int dir)
        {
            if (idx < 0 || idx >= (int)m_controls.size()) return;
            BoundControl& bc = m_controls[idx];
            if (isControlLocked(bc.desc->key)) return;   // locked: no actúa

            eclips::CtrlModelo m;
            m.vmin = bc.vmin;
            m.vmax = bc.vmax;
            m.vstep = bc.vstep;
            m.pasoExplicito = bc.pasoExplicito;
            // `switch` SIN `default` a propósito: si `CtrlRender` gana una fila,
            // el compilador pone el rojo que ningún harness puede poner (el enum
            // vive en esta clase, que arrastra GL).
            switch (bc.render)
            {
            case CtrlRender::Bar:    m.render = eclips::CtrlRenderKind::Bar;    break;
            case CtrlRender::Toggle: m.render = eclips::CtrlRenderKind::Toggle; break;
            case CtrlRender::Arrows: m.render = eclips::CtrlRenderKind::Arrows; break;
            case CtrlRender::Boxes:  m.render = eclips::CtrlRenderKind::Boxes;  break;
            }

            // Estado VIVO (§7.132), igual que el flip del mouse: `bc.value` es
            // de hace un frame.
            const float vivo = readControlValue(bc.desc->key);
            const float nxt = eclips::controlAjustar(m, vivo, dir);
            if (nxt == vivo) return;                    // topeado: sin escritura
            writeControlValue(bc.desc->key, nxt);       // punto único, igual que el mouse
            bc.value = nxt;
        }

        // El despacho de los cuatro verbos de foco. Devuelve si consumió.
        bool manejarFoco(InputAction a)
        {
            const eclips::FocoCuentas c = focoCuentas();
            const int total = eclips::focoTotal(c);
            // Sin focables la tecla SIGUE su camino en vez de morir acá — es el
            // `false` que `moverFoco` daba en `CP5`, conservado.
            if (total <= 0) return false;

            if (a == InputAction::FocusUp || a == InputAction::FocusDown)
            {
                m_focoDelPuntero = false;
                m_focoPlano = eclips::focoMover(
                    m_focoPlano, total,
                    (a == InputAction::FocusDown) ? eclips::FocoDir::Abajo
                                                  : eclips::FocoDir::Arriba);
                proyectarFoco();
                return true;
            }

            // izq/der: son del `control` focado. **Consumen igual aunque no haya
            // control**, por la misma razón que `CP5`: dejarlas caer las mandaría
            // a los atajos globales, y una flecha durante una elección es del
            // anillo la haya movido o no.
            const eclips::Focable f = eclips::focoFocable(c, m_focoPlano);
            if (f.familia == eclips::FocoFamilia::Control && f.indice >= 0)
                ajustarControlFocado(f.indice,
                                     (a == InputAction::FocusRight) ? +1 : -1);
            return true;
        }

        // Ley 3 para las familias que no son choice: activar el focado ruteando
        // al camino de click que YA existe. Devuelve si consumió.
        bool confirmarFocado()
        {
            if (m_focoDelPuntero) return false;   // el puntero manda: camino viejo
            const eclips::FocoCuentas c = focoCuentas();
            const eclips::Focable f = eclips::focoFocable(c, m_focoPlano);
            if (f.indice < 0) return false;

            switch (f.familia)
            {
            case eclips::FocoFamilia::Ranura:
                dispatchSlotAction(f.indice);      // el mismo que el click
                return true;
            case eclips::FocoFamilia::Enlace:
            {
                SceneElement* link = m_sceneLayers.interactiveAt(f.indice);
                if (!link) return false;

                // `CP2a`: espejo exacto de la sección 0.5, y por la misma razón
                // —el pool resuelve su propia espera—. Acá el enlace ajeno no
                // hace falta filtrarlo: el anillo sólo ofrece los focables, y
                // durante el pool los focables son las palabras.
                if (m_palabrasActivas && esPalabraDelPool(link))
                {
                    if (m_hasAudio)
                    {
                        const std::string& cs =
                            m_dialogBox.activeChoiceClickSound();
                        if (!cs.empty()) m_audio.playSound(cs);
                    }
                    return picarPalabra(link);
                }

                m_modalRequired = false;           // espeja el click (sección 0.5)
                if (m_hasAudio)
                {
                    const std::string& cs = m_dialogBox.activeChoiceClickSound();
                    if (!cs.empty()) m_audio.playSound(cs);
                }
                if (m_phase == VNPhase::WaitingInterface && link->target.empty())
                {
                    ++m_cmdIndex;
                    m_phase = VNPhase::Executing;
                    return true;
                }
                if (!link->target.empty())
                {
                    const std::string destino = link->target;  // copia: un salto invalida `link`
                    const eclips::TargetVeredicto ver = dispatchTarget(
                        destino, eclips::system_flow::PantallaCara::Enlace);
                    // CP0 `mutar` · F2: espejo exacto del click (sección 0.5) —
                    // una mutación NO consume, así que cae al avance normal.
                    if (ver == eclips::TargetVeredicto::Mutacion ||
                        ver == eclips::TargetVeredicto::MutacionRota)
                        return false;
                    return true;
                }
                return false;   // target vacío = continuar: cae al avance normal
            }
            case eclips::FocoFamilia::Control:
                // Un `control` no se "activa": se ajusta con izq/der. Consumir
                // igual evita que el ESPACIO avance el guion por debajo de la
                // pantalla de settings.
                return true;
            case eclips::FocoFamilia::Tecla:
                // **Y ESTA es la frase que `CP9b` invierte** (`L-5` punto 5): un
                // widget de tecla SI se activa, y activarlo abre la captura.
                abrirCaptura(f.indice);
                return true;
            case eclips::FocoFamilia::Choice:
            default:
                return false;   // la caja lo resuelve con su ley 3 (`CP5`)
            }
        }

        // ── cmdRanura (Ola 2 · CP2): registra un slot-widget Tier-A ───────────
        // Sintaxis: ranura <N> accion=cargar x= y= ancho= alto= [estilo=]. N es 1-based
        // (lo que ve el jugador, "Slot 1"); se convierte a 0-based interno aquí —
        // la única frontera (readSlot/loadGameFromMenu hablan 0-based). La meta se
        // lee UNA vez (readSlot) y se cachea: a diferencia del valor de un control
        // (vivo, compartible), la meta de un slot es ESTÁTICA mientras la pantalla
        // vive (cargar empuja un estado nuevo, no reescribe el slot) → cero I/O por
        // frame. La acción se guarda cruda; el clic→acción es CP3.
        void cmdRanura(const EclipCmd& cmd)
        {
            int n = 0;
            try { n = std::stoi(cmd.arg1); }
            catch (...) {
                std::cerr << "[ranura] indice invalido: '" << cmd.arg1
                    << "' (esperaba 1.." << VNSaveMenu::NUM_SLOTS << "). Ignorada."
                    << std::endl;
                return;
            }
            const int slot0 = n - 1;            // 1-based del creador → 0-based interno
            if (slot0 < 0 || slot0 >= VNSaveMenu::NUM_SLOTS)
            {
                std::cerr << "[ranura] indice fuera de rango: " << n
                    << " (valido 1.." << VNSaveMenu::NUM_SLOTS << "). Ignorada."
                    << std::endl;
                return;
            }

            SlotWidget sw;
            sw.scopeOwner = m_activeScopeOwner;
            sw.slot = slot0;
            sw.action = asciiToLower(cmd.slotAction);   // "" si no puso accion=
            sw.x = cmd.layerX;
            sw.y = cmd.layerY;
            if (cmd.controlWidth >= 0.0f) sw.width = cmd.controlWidth;
            // alto: la decisión baja tras cargar el thumb + cachear meta (el
            // medidor los necesita). Aquí solo el estilo.
            sw.styleName = cmd.styleName;
            if (!sw.styleName.empty())
                voiceUnsupportedControlSurface("ranura", sw.styleName);

            // Meta cacheada (1 lectura). Mismo lector que el VNSaveMenu C++ →
            // misma nocion de "ocupado" (hasVN || hasGP) sin re-derivarla.
            VNSlotInfo info = VNSaveMenu::readSlot(slot0);
            sw.occupied = info.occupied;
            sw.scene = info.scene;
            sw.preview = info.preview;
            sw.timestamp = info.timestamp;
            sw.timeRaw = info.timeRaw;

            // 6.1 CP1: cargar el thumbnail derivado del slot (fuera del LRU).
            // Fail-safe §7.34: cargo a temp; solo si load() ok lo dejo (swapWith,
            // como reloadAll). Sin archivo (save viejo / no-FBO) → sin entrada.
            {
                std::string tp = "saves/thumbs/slot_" + std::to_string(slot0) + ".png";
                Texture fresh;
                if (fresh.load(tp))
                {
                    m_slotThumbs[slot0].swapWith(fresh);
                    m_slotThumbOwners[slot0] = m_activeScopeOwner;
                }
                else
                {
                    m_slotThumbs.erase(slot0);
                    m_slotThumbOwners.erase(slot0);
                }
            }

            // alto: explícito > auto (medido) > default. auto = A3: también el
            // default en vertical (72 no contiene el stack thumb+líneas; el
            // creador ya no pisa el overflow §7.185). Se mide AQUÍ, con el thumb
            // ya cargado y la meta cacheada → el mismo sw.height lo leen hitbox
            // y render (§1.40, imposible desincronizar).
            {
                const DialogBoxStyle& ds = m_dialogBox.currentStyle();
                const bool verticalLayout = (ds.slotLayout == SlotLayout::Vertical);
                if (cmd.autoHeight)          sw.height = measureSlotCardHeight(sw, ds);
                else if (cmd.layerH > 0.0f)  sw.height = cmd.layerH;                    // explícito manda
                else if (verticalLayout)     sw.height = measureSlotCardHeight(sw, ds); // A3
                // else: inline sin alto → default 72 (byte-idéntico)
            }

            m_slotWidgets.push_back(sw);

            std::cout << "[ranura] slot " << n << " (interno " << slot0
                << ") bound, accion='" << sw.action << "' ocupado="
                << (sw.occupied ? "si" : "no") << " x=" << sw.x << " y=" << sw.y
                << " ancho=" << sw.width << " alto=" << sw.height << std::endl;
        }

        // ── renderSlotWidgets (CP2): las tarjetas, proyección limpia (UI fija) ─
        void renderSlotWidgets()
        {
            if (m_slotWidgets.empty()) return;
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            draw2d::setupProjection(VW, VH);
            // Index loop: el hover es por-indice (i == m_hoveredSlot). Se pasa el
            // bool ya resuelto → renderSlotCard no necesita conocer su indice.
            for (int i = 0; i < (int)m_slotWidgets.size(); ++i)
                renderSlotCard(m_slotWidgets[i], i == m_hoveredSlot);
        }

        // ── renderSlotCard: una tarjeta. Ocupado → escena + fecha + preview;
        // vacío → "Vacío" atenuado y centrado. Patrón _noBatch de renderBoxes
        // (§7.152: panel/borde/texto en UN batch abierto). Sin estilo= aún
        // (defaults; estilo=/PNG es transversal, fichado).

        // ── ellipsizeToWidth: recorta a maxW con elipsis "…" (glyph U+2026 ya
        // horneado en el atlas). Respeta fronteras UTF-8 — measureWidth decodifica
        // por codepoint, así que quito codepoints COMPLETOS del final (primero los
        // bytes de continuación 10xxxxxx, luego el líder) o partiría un acento.
        // Caja de altura fija = 1 línea → recortar es mejor que desbordar.
        std::string ellipsizeToWidth(const std::string& s, float maxW) const
        {
            if (!m_hasNameFont || s.empty()) return s;
            if (m_nameRenderer.measureWidth(s) <= maxW) return s;

            static const std::string kEllipsis = "\xE2\x80\xA6";   // U+2026 "…"
            const float ellW = m_nameRenderer.measureWidth(kEllipsis);

            std::string out = s;
            while (!out.empty() &&
                m_nameRenderer.measureWidth(out) + ellW > maxW)
            {
                // Quita 1 codepoint completo del final.
                while (!out.empty() && (((unsigned char)out.back() & 0xC0) == 0x80))
                    out.pop_back();          // bytes de continuación
                if (!out.empty()) out.pop_back();   // byte líder / ASCII
            }
            return out + kEllipsis;
        }

        // ── Constantes de layout de la tarjeta (C1, §1.40): el DIBUJO y la
        // MEDIDA (alto=auto) leen las MISMAS → el hitbox nunca mide distinto de
        // lo que se pinta. Mismos valores que los literales de antes → inline
        // byte-idéntico.
        static constexpr float kSlotPad = 12.0f;  // margen interno (lados/arriba/abajo)
        static constexpr float kSlotThumbGap = 8.0f;   // aire thumb→1a línea (vertical)
        static constexpr float kSlotLineGap = 4.0f;   // aire entre líneas de texto

        // ── measureSlotCardHeight (alto=auto, A3): alto de contenido exacto,
        // espejando los incrementos de renderSlotCard con las constantes C1.
        // Vertical: pad + thumb(+gap) + N líneas(+gaps) + pad. Inline: 2·pad +
        // N líneas. Vacía (B1): 2·pad + 1 línea (la etiqueta centrada). Sin
        // fuente → las líneas aportan 0 (igual que el dibujo, que las omite).
        // Se mide en el BIND del comando (estilo activo entonces); layout y
        // aspecto son theme-static → no cambian entre bind y render.
        float measureSlotCardHeight(const SlotWidget& sw, const DialogBoxStyle& d)
        {
            const bool vertical = (d.slotLayout == SlotLayout::Vertical);
            const float lh = m_hasNameFont ? m_nameRenderer.getLineHeight() : 0.0f;

            // B1: la vacía es 1 línea centrada (renderSlotCard no le da stack).
            if (!sw.occupied) return kSlotPad * 2.0f + lh;

            if (vertical)
            {
                float thH = 0.0f;
                auto thIt = m_slotThumbs.find(sw.slot);
                if (thIt != m_slotThumbs.end() && thIt->second.getWidth() > 0)
                {
                    const Texture& th = thIt->second;
                    const float thW = sw.width - kSlotPad * 2.0f;
                    const float ba = (d.slotThumbAspect > 0.0f)
                        ? d.slotThumbAspect
                        : ((float)th.getWidth() / (float)th.getHeight());
                    thH = thW / ba + kSlotThumbGap;   // + aire tras el thumb
                }
                float textH = 0.0f;
                if (m_hasNameFont)
                {
                    // Espeja los guards del dibujo: escena siempre; fecha y
                    // preview solo si hay dato.
                    std::string dateStr = fechaDeRanura(sw.timeRaw, sw.timestamp, d);
                    int lines = 1;
                    if (!dateStr.empty()) ++lines;
                    if (!sw.preview.empty()) ++lines;
                    textH = lines * lh + (lines - 1) * kSlotLineGap;
                }
                return kSlotPad + thH + textH + kSlotPad;
            }
            else
            {
                // Inline: el thumb escala a la altura (no la fija); el alto lo dan
                // las 2 líneas posibles (escena/fecha en 1, preview en 2).
                float textH = 0.0f;
                if (m_hasNameFont)
                {
                    int lines = sw.preview.empty() ? 1 : 2;
                    textH = lines * lh + (lines - 1) * kSlotLineGap;
                }
                return kSlotPad * 2.0f + textH;
            }
        }

        void renderSlotCard(const SlotWidget& sw, bool hover)
        {
            const float h = sw.height;
            const float pad = kSlotPad;
            const DialogBoxStyle& d = m_dialogBox.currentStyle();
            const bool vertical = (d.slotLayout == SlotLayout::Vertical);
            float thumbShift = 0.0f;      // inline: corre el texto a la derecha del thumb
            float textTopY = sw.y + pad;  // vertical: baja el texto bajo el thumb

            // ── 6.6: estado "inactiva" de la vacia. Es INERTE cuando su accion
            // es no-op sobre vacio (cargar/borrar; guardar SI es valida → viva).
            // Si ademas el tema pidio atenuar (opacidad<1) → deshabilitada:
            // atenua (emptyA) + enmudece (effHover=false → idle siempre, sin
            // barra). Espeja el no-op de dispatchSlotAction: la funcion ya esta
            // muda, esto es solo su senal visual (§7.117). Off por identidad:
            // opacidad 1.0 → inactiveEmpty=false → byte-identico.
            const std::string& slotAct = sw.action;
            const bool inertOnEmpty = (slotAct == "cargar" || slotAct == "load"
                || slotAct == "borrar" || slotAct == "delete");
            const bool inactiveEmpty = !sw.occupied && inertOnEmpty
                && d.slotCardEmptyInactiveOpacity < 1.0f;
            // Clamp de piso (el techo lo garantiza el guard <1.0); alpha negativo
            // seria indefinido en el rasterizer.
            const float emptyA = inactiveEmpty
                ? (d.slotCardEmptyInactiveOpacity < 0.0f ? 0.0f
                    : d.slotCardEmptyInactiveOpacity)
                : 1.0f;
            const bool effHover = inactiveEmpty ? false : hover;

            draw2d::g_batch->begin();

            if (sw.occupied)
            {
                // Sprite con swap idle/hover (como choice): sin hover-sprite cae
                // al idle. drawButtonBg cae a color (rect+borde) si btnTex=null →
                // sin sprite, idéntico a CP1. Texturas pineadas (CP2.3) → get O(1).
                Texture* slotTex = d.hasSlotCardSprite()
                    ? m_assets.get(d.slotCardSpritePath) : nullptr;
                Texture* slotHoverTex = d.hasSlotCardHoverSprite()
                    ? m_assets.get(d.slotCardHoverSpritePath) : nullptr;
                Texture* btnTex = hover ? (slotHoverTex ? slotHoverTex : slotTex)
                    : slotTex;

                draw2d::drawButtonBg(sw.x, sw.y, sw.width, h,
                    btnTex, d.hasSlotCard9Slice(),
                    d.slotCardSliceLeft, d.slotCardSliceRight,
                    d.slotCardSliceTop, d.slotCardSliceBottom,
                    hover ? d.slotCardHoverBgR : d.slotCardBgR,
                    hover ? d.slotCardHoverBgG : d.slotCardBgG,
                    hover ? d.slotCardHoverBgB : d.slotCardBgB,
                    hover ? d.slotCardHoverBgA : d.slotCardBgA,
                    hover ? d.slotCardHoverBorderR : d.slotCardBorderR,
                    hover ? d.slotCardHoverBorderG : d.slotCardBorderG,
                    hover ? d.slotCardHoverBorderB : d.slotCardBorderB,
                    hover ? d.slotCardHoverBorderA : d.slotCardBorderA,
                    d.slotCardBorderThickness);

                // 6.1 CP1: thumbnail inline-izquierda (estilo TF). Rect izq dentro
                // del pad; alto = h-2pad, ancho = alto × aspecto REAL de la captura
                // (sin 16:9 hardcodeado → sin distorsión, fit exacto). Corre el texto.
                auto thIt = m_slotThumbs.find(sw.slot);
                if (thIt != m_slotThumbs.end() && thIt->second.getWidth() > 0)
                {
                    const Texture& th = thIt->second;
                    if (vertical)
                    {
                        // Vertical: thumb ANCHO arriba. El ancho lo fija la tarjeta;
                        // el alto sale del aspecto (auto = aspecto nativo). El texto
                        // baja bajo el thumb (textTopY), no a la derecha.
                        const float thW = sw.width - pad * 2.0f;
                        const float ba = (d.slotThumbAspect > 0.0f)
                            ? d.slotThumbAspect
                            : ((float)th.getWidth() / (float)th.getHeight());
                        const float thH = thW / ba;
                        if (d.slotThumbAspect <= 0.0f)
                            draw2d::drawTexture_noBatch(th, sw.x + pad, sw.y + pad, thW, thH);
                        else
                            draw2d::drawTextureFit_noBatch(th, sw.x + pad, sw.y + pad,
                                thW, thH, d.slotThumbFit);
                        textTopY = sw.y + pad + thH + kSlotThumbGap;
                    }
                    else
                    {
                        // Inline (hoy, intacto): thumb izq, texto corrido a la derecha.
                        const float thH = h - pad * 2.0f;
                        float thW;
                        if (d.slotThumbAspect <= 0.0f)
                        {
                            thW = thH * ((float)th.getWidth() / (float)th.getHeight());
                            draw2d::drawTexture_noBatch(th, sw.x + pad, sw.y + pad, thW, thH);
                        }
                        else
                        {
                            thW = thH * d.slotThumbAspect;
                            draw2d::drawTextureFit_noBatch(th, sw.x + pad, sw.y + pad,
                                thW, thH, d.slotThumbFit);
                        }
                        thumbShift = thW + 8.0f;
                    }
                }
            }
            else
            {
                // 6.6: effHover enmudece la inerte (idle siempre); emptyA atenua
                // el alpha (fondo+borde). RGB intacto: Ren'Py baja OPACIDAD, no
                // oscurece — blanco-tenue sobre el azul = el gris de "empty slot".
                draw2d::drawRect_noBatch(sw.x, sw.y, sw.width, h,
                    effHover ? d.slotCardEmptyHoverBgR : d.slotCardEmptyBgR,
                    effHover ? d.slotCardEmptyHoverBgG : d.slotCardEmptyBgG,
                    effHover ? d.slotCardEmptyHoverBgB : d.slotCardEmptyBgB,
                    (effHover ? d.slotCardEmptyHoverBgA : d.slotCardEmptyBgA) * emptyA);
                draw2d::drawRectBorder_noBatch(sw.x, sw.y, sw.width, h,
                    effHover ? d.slotCardEmptyHoverBorderR : d.slotCardEmptyBorderR,
                    effHover ? d.slotCardEmptyHoverBorderG : d.slotCardEmptyBorderG,
                    effHover ? d.slotCardEmptyHoverBorderB : d.slotCardEmptyBorderB,
                    (effHover ? d.slotCardEmptyHoverBorderA : d.slotCardEmptyBorderA) * emptyA,
                    d.slotCardEmptyBorderThickness);
            }

            // ── Barra de hover (capa [4]): 4a senal de foco, comun a ocupada/
            // vacia. Off por grosor 0. Rect vertical pegado al borde izq, alto
            // completo; el texto arranca en +pad (12) → no lo toca. Asset-free.
            // 6.6: effHover (no hover) → la barra de foco NO sale en la vacia
            // inerte (muda). Para ocupada/vacia-activa effHover==hover → sin cambio.
            if (effHover && d.slotCardHoverBarThickness > 0.0f)
                draw2d::drawRect_noBatch(sw.x, sw.y, d.slotCardHoverBarThickness, h,
                    d.slotCardHoverBarR, d.slotCardHoverBarG,
                    d.slotCardHoverBarB, d.slotCardHoverBarA);

            if (m_hasNameFont)
            {
                const float lh = m_nameRenderer.getLineHeight();
                if (sw.occupied)
                {
                    // Fecha: comun a ambos layouts (helper compartido, F1). Del epoch
                    // crudo con la plantilla del tema; save viejo sin epoch → string.
                    // Desde CP3 la plantilla pasa por t() antes de formatear y los
                    // nombres de mes/dia siguen el idioma del jugador.
                    std::string dateStr = fechaDeRanura(sw.timeRaw, sw.timestamp, d);

                    if (vertical)
                    {
                        // Vertical (DDLC/Ren'Py): stack bajo el thumb. La fecha
                        // PROTAGONIZA por posicion (1a linea, ancho completo); el
                        // creador ya la tine con las keys de color. Orden:
                        // fecha -> escena -> preview. Izquierda (§7.118 centrado).
                        const float tx = sw.x + pad;
                        const float innerW = sw.width - pad * 2.0f;
                        float ty = textTopY;
                        if (!dateStr.empty())
                        {
                            std::string date = ellipsizeToWidth(dateStr, innerW);
                            m_nameRenderer.drawText_noBatch(date, tx, ty,
                                hover ? d.slotCardHoverDateR : d.slotCardDateR,
                                hover ? d.slotCardHoverDateG : d.slotCardDateG,
                                hover ? d.slotCardHoverDateB : d.slotCardDateB,
                                hover ? d.slotCardHoverDateA : d.slotCardDateA);
                            ty += lh + kSlotLineGap;
                        }
                        std::string scene = ellipsizeToWidth(sw.scene, innerW);
                        m_nameRenderer.drawText_noBatch(scene, tx, ty,
                            hover ? d.slotCardHoverSceneR : d.slotCardSceneR,
                            hover ? d.slotCardHoverSceneG : d.slotCardSceneG,
                            hover ? d.slotCardHoverSceneB : d.slotCardSceneB,
                            hover ? d.slotCardHoverSceneA : d.slotCardSceneA);
                        ty += lh + kSlotLineGap;
                        if (!sw.preview.empty())
                        {
                            std::string prev = ellipsizeToWidth(sw.preview, innerW);
                            m_nameRenderer.drawText_noBatch(prev, tx, ty,
                                hover ? d.slotCardHoverPreviewR : d.slotCardPreviewR,
                                hover ? d.slotCardHoverPreviewG : d.slotCardPreviewG,
                                hover ? d.slotCardHoverPreviewB : d.slotCardPreviewB,
                                hover ? d.slotCardHoverPreviewA : d.slotCardPreviewA);
                        }
                    }
                    else
                    {
                        // Inline (hoy, byte-identico): escena+fecha en linea 1,
                        // preview en linea 2. dateStr compartido arriba.
                        const float innerW = sw.width - pad * 2.0f - thumbShift;
                        const float textX = sw.x + pad + thumbShift;
                        float tsW = dateStr.empty()
                            ? 0.0f : m_nameRenderer.measureWidth(dateStr);
                        const float gap = (tsW > 0.0f) ? 12.0f : 0.0f;
                        std::string scene = ellipsizeToWidth(sw.scene, innerW - tsW - gap);
                        m_nameRenderer.drawText_noBatch(scene,
                            textX, sw.y + pad,
                            hover ? d.slotCardHoverSceneR : d.slotCardSceneR,
                            hover ? d.slotCardHoverSceneG : d.slotCardSceneG,
                            hover ? d.slotCardHoverSceneB : d.slotCardSceneB,
                            hover ? d.slotCardHoverSceneA : d.slotCardSceneA);
                        if (!dateStr.empty())
                            m_nameRenderer.drawText_noBatch(dateStr,
                                sw.x + sw.width - pad - tsW, sw.y + pad,
                                hover ? d.slotCardHoverDateR : d.slotCardDateR,
                                hover ? d.slotCardHoverDateG : d.slotCardDateG,
                                hover ? d.slotCardHoverDateB : d.slotCardDateB,
                                hover ? d.slotCardHoverDateA : d.slotCardDateA);
                        if (!sw.preview.empty())
                        {
                            std::string prev = ellipsizeToWidth(sw.preview, innerW);
                            m_nameRenderer.drawText_noBatch(prev,
                                textX, sw.y + pad + lh + kSlotLineGap,
                                hover ? d.slotCardHoverPreviewR : d.slotCardPreviewR,
                                hover ? d.slotCardHoverPreviewG : d.slotCardPreviewG,
                                hover ? d.slotCardHoverPreviewB : d.slotCardPreviewB,
                                hover ? d.slotCardHoverPreviewA : d.slotCardPreviewA);
                        }
                    }
                }
                else
                {
                    std::string label = d.slotCardEmptyLabel;
                    float lw = m_nameRenderer.measureWidth(label);
                    // 6.6: effHover (muda) + emptyA atenua el alpha del texto →
                    // "empty slot" en gris-tenue (blanco bajado sobre el azul).
                    m_nameRenderer.drawText_noBatch(label,
                        sw.x + (sw.width - lw) * 0.5f, sw.y + (h - lh) * 0.5f,
                        effHover ? d.slotCardEmptyHoverTextR : d.slotCardEmptyTextR,
                        effHover ? d.slotCardEmptyHoverTextG : d.slotCardEmptyTextG,
                        effHover ? d.slotCardEmptyHoverTextB : d.slotCardEmptyTextB,
                        (effHover ? d.slotCardEmptyHoverTextA : d.slotCardEmptyTextA) * emptyA);
                }
            }

            draw2d::g_batch->end();
        }

        // ── R4 display: captura/apply/commit/revert ─────────────────────────
        display_change::State currentDisplayState() const
        {
            display_change::State state;
            state.fullscreen = Settings::fullscreen();
            state.windowW = Settings::storedWindowWidth();
            state.windowH = Settings::storedWindowHeight();
            state.windowSizeUserSet = Settings::windowSizeUserSet();
            return state;
        }

        void commitDisplaySettings(const display_change::State& state)
        {
            Settings::setFullscreen(state.fullscreen);
            if (state.windowSizeUserSet)
                Settings::setWindowSize(state.windowW, state.windowH);
            else
                Settings::clearWindowSizeOverride();
        }

        bool applyDisplayState(const display_change::State& state)
        {
            if (!m_window || state.windowW <= 0 || state.windowH <= 0)
            {
                std::cerr << "[display] No se puede aplicar un estado sin ventana "
                    "o con tamano invalido.\n";
                return false;
            }

            int rc = 0;
            if (state.fullscreen)
            {
                // Conservar la preferencia windowed debajo del modo desktop.
                SDL_SetWindowSize(m_window, state.windowW, state.windowH);
                rc = SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            else
            {
                rc = SDL_SetWindowFullscreen(m_window, 0);
                SDL_SetWindowSize(m_window, state.windowW, state.windowH);
            }

            SDL_GetWindowSize(m_window, &m_winW, &m_winH);
            if (rc != 0)
            {
                std::cerr << "[display] SDL rechazo el cambio: "
                    << SDL_GetError() << '\n';
                return false;
            }
            return true;
        }

        void beginDisplayChange(const display_change::State& candidate)
        {
            const display_change::State before = currentDisplayState();
            if (candidate == before) return;
            if (!m_displayChange.begin(before, candidate)) return;

            m_displayConfirmCursor = 1; // cada apertura vuelve a default-No
            if (!applyDisplayState(candidate))
            {
                display_change::State restore;
                m_displayChange.revert(restore);
                applyDisplayState(restore);
                commitDisplaySettings(restore);
                std::cerr << "[display] Candidato descartado porque SDL no pudo "
                    "aplicarlo; se restauro el estado confirmado.\n";
            }
        }

        void finishDisplayChange(bool keep, const char* reason)
        {
            display_change::State state;
            if (keep)
            {
                if (!m_displayChange.confirm(state)) return;
                commitDisplaySettings(state);
                std::cout << "[display] Cambio confirmado: "
                    << (state.fullscreen ? "fullscreen " : "windowed ")
                    << state.windowW << 'x' << state.windowH << ".\n";
            }
            else
            {
                if (!m_displayChange.revert(state)) return;
                applyDisplayState(state);
                commitDisplaySettings(state);
                std::cout << "[display] Cambio revertido (" << reason << "): "
                    << (state.fullscreen ? "fullscreen " : "windowed ")
                    << state.windowW << 'x' << state.windowH << ".\n";
            }
            m_displayConfirmCursor = 1;
        }

        void displayConfirmPanelRect(float& dX, float& dY,
            float& dW, float& dH) const
        {
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            dW = VW * 0.36f;
            dH = 230.0f;
            dX = (VW - dW) * 0.5f;
            dY = (VH - dH) * 0.5f;
        }

        void displayConfirmButtonRect(int i, float& bx, float& by,
            float& bw, float& bh) const
        {
            float dX, dY, dW, dH;
            displayConfirmPanelRect(dX, dY, dW, dH);
            bw = 110.0f;
            bh = 46.0f;
            const float gap = 28.0f;
            const float totalW = bw * 2.0f + gap;
            bx = dX + (dW - totalW) * 0.5f + i * (bw + gap);
            by = dY + dH - bh - 20.0f;
        }

        int hitTestDisplayConfirmButton(float vx, float vy) const
        {
            for (int i = 0; i < 2; ++i)
            {
                float bx, by, bw, bh;
                displayConfirmButtonRect(i, bx, by, bw, bh);
                if (vx >= bx && vx <= bx + bw && vy >= by && vy <= by + bh)
                    return i;
            }
            return -1;
        }

        void displayConfirmChoose(int choice)
        {
            finishDisplayChange(choice == 0,
                choice == 0 ? "confirmado" : "cancelado");
        }

        void renderDisplayConfirmModal()
        {
            if (!m_displayChange.active()) return;

            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            draw2d::setupProjection(VW, VH);

            float dX, dY, dW, dH;
            displayConfirmPanelRect(dX, dY, dW, dH);

            draw2d::g_batch->begin();
            draw2d::drawRect_noBatch(0, 0, VW, VH, 0, 0, 0, 0.62f);
            draw2d::drawRect_noBatch(dX, dY, dW, dH,
                0.07f, 0.09f, 0.14f, 0.98f);
            draw2d::drawRectBorder_noBatch(dX, dY, dW, dH,
                0.31f, 0.76f, 0.86f, 0.96f, 2.5f);

            if (m_hasNameFont)
            {
                const float lh = m_nameRenderer.getLineHeight();
                const display_change::State& before = m_displayChange.before();
                const display_change::State& candidate = m_displayChange.candidate();

                const std::string title = eclips::langUI("Conservar estos cambios?",
                                                         "Keep these changes?");
                std::string target;
                if (candidate.fullscreen != before.fullscreen)
                    target = candidate.fullscreen
                        ? eclips::langUI("Modo: pantalla completa", "Mode: fullscreen")
                        : eclips::langUI("Modo: ventana",           "Mode: windowed");
                else
                    target = eclips::langPoner(
                        eclips::langUI("Resolucion: {ancho}x{alto}",
                                       "Resolution: {ancho}x{alto}"),
                        {{"ancho", std::to_string(candidate.windowW)},
                         {"alto",  std::to_string(candidate.windowH)}});

                const double remaining = m_displayChange.remainingSeconds();
                int seconds = (int)remaining;
                if ((double)seconds < remaining) ++seconds;

                // FORK 8a: DOS plantillas enteras. El contador hace ceil y baja
                // hasta 1, así que la forma vieja decía "en 1 segundos" — un
                // error de español que existía desde antes de este lote y que
                // CP3 destapa, porque esa frase pasa a ser la primera clave que
                // lee un traductor. Cubre `es` y `en`; los idiomas con más de
                // dos formas plurales (el ruso tiene tres) quedan fichados.
                const std::string countdown = eclips::langPoner(
                    (seconds == 1)
                        ? eclips::langUI("Se restauraran en {segundos} segundo.",
                                         "Reverting in {segundos} second.")
                        : eclips::langUI("Se restauraran en {segundos} segundos.",
                                         "Reverting in {segundos} seconds."),
                    {{"segundos", std::to_string(seconds)}});

                float tw = m_nameRenderer.measureWidth(title);
                m_nameRenderer.drawText_noBatch(title,
                    dX + (dW - tw) * 0.5f, dY + 20.0f,
                    0.90f, 0.96f, 1.0f, 1.0f);

                float targetW = m_nameRenderer.measureWidth(target);
                m_nameRenderer.drawText_noBatch(target,
                    dX + (dW - targetW) * 0.5f, dY + 20.0f + lh + 5.0f,
                    0.72f, 0.86f, 0.94f, 1.0f);

                float countW = m_nameRenderer.measureWidth(countdown);
                m_nameRenderer.drawText_noBatch(countdown,
                    dX + (dW - countW) * 0.5f, dY + 20.0f + lh * 2.0f + 10.0f,
                    0.96f, 0.78f, 0.48f, 1.0f);

                const std::string labels[2] = { eclips::langUI("Si", "Yes"),
                                                eclips::langUI("No", "No") };
                for (int i = 0; i < 2; ++i)
                {
                    float bx, by, bw, bh;
                    displayConfirmButtonRect(i, bx, by, bw, bh);
                    const bool selected = (i == m_displayConfirmCursor);
                    if (selected)
                        draw2d::drawRect_noBatch(bx, by, bw, bh,
                            i == 0 ? 0.16f : 0.22f,
                            i == 0 ? 0.42f : 0.18f,
                            i == 0 ? 0.48f : 0.38f, 0.94f);
                    else
                        draw2d::drawRect_noBatch(bx, by, bw, bh,
                            0.12f, 0.10f, 0.18f, 0.72f);
                    if (selected)
                        draw2d::drawRectBorder_noBatch(bx, by, bw, bh,
                            0.50f, 0.75f, 1.0f, 1.0f, 2.0f);

                    float labelW = m_nameRenderer.measureWidth(labels[i]);
                    m_nameRenderer.drawText_noBatch(labels[i],
                        bx + (bw - labelW) * 0.5f, by + (bh - lh) * 0.5f,
                        0.92f, 0.94f, 0.98f, 1.0f);
                }
            }

            draw2d::g_batch->end();
        }

        // ── confirmPanelRect / confirmButtonRect (CP2): geometria del modal,
        // UNA sola fuente de verdad que render Y hit-test consumen (§1.40 a
        // nivel de caja → imposible desincronizar como los m_delBtn* del C++).
        void confirmPanelRect(float& dX, float& dY, float& dW, float& dH) const
        {
            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            dW = VW * 0.34f;
            dH = 180.0f;
            dX = (VW - dW) * 0.5f;
            dY = (VH - dH) * 0.5f;
        }

        // i: 0=Si, 1=No. Layout espejo del C++ (dos botones centrados abajo).
        void confirmButtonRect(int i, float& bx, float& by, float& bw, float& bh) const
        {
            float dX, dY, dW, dH;
            confirmPanelRect(dX, dY, dW, dH);
            bw = 110.0f;
            bh = 46.0f;
            const float gap = 28.0f;
            const float totalW = bw * 2.0f + gap;
            const float btnStartX = dX + (dW - totalW) * 0.5f;
            bx = btnStartX + i * (bw + gap);
            by = dY + dH - bh - 20.0f;
        }

        // ── hitTestConfirmButton (CP2): 0=Si, 1=No, -1=ninguno. Misma
        // confirmButtonRect que el render → clic y dibujo nunca divergen.
        int hitTestConfirmButton(float vx, float vy) const
        {
            for (int i = 0; i < 2; ++i)
            {
                float bx, by, bw, bh;
                confirmButtonRect(i, bx, by, bw, bh);
                if (vx >= bx && vx <= bx + bw && vy >= by && vy <= by + bh)
                    return i;
            }
            return -1;
        }

        // ── closeConfirmModal (CP2): baja el modal y restaura default-No para
        // la proxima vez. Salida unica de No/ESC/cancelar.
        void closeConfirmModal()
        {
            m_confirmActive = false;
            m_confirmSlot = -1;
            m_confirmCursor = 1;
        }

        // ── confirmModalChoose (CP2): el jugador eligio (0=Si, 1=No). CP2 solo
        // cierra; el borrado real (deleteSlot + vaciar el SlotWidget) lo conecta
        // CP3 en la rama Si. Asi el input se verifica sin tocar disco.
        void confirmModalChoose(int choice)
        {
            if (choice == 0 && m_confirmSlot >= 0)
            {
                // Borrado real (Fork 3A): borra en disco (deleteSlot ya existe,
                // estatica), luego VACIA el cache de TODAS las ranuras que
                // apunten a ese slot (puede haber mas de una) → la tarjeta
                // muestra "Vacio" al instante, sin re-leer disco ni re-correr la
                // escena. Espejo del C++ executeDelete (m_slots[i] = VNSlotInfo{}).
                bool ok = VNSaveMenu::deleteSlot(m_confirmSlot);
                if (ok)
                {
                    for (auto& w : m_slotWidgets)
                        if (w.slot == m_confirmSlot)
                        {
                            w.occupied = false;
                            w.scene.clear();
                            w.preview.clear();
                            w.timestamp.clear();
                        }
                    // 6.1 CP1: el slot quedó vacío → soltar su thumbnail (libera GPU;
                    // la tarjeta vacía no lo dibuja igual). El PNG en disco queda
                    // (higiene opcional filada); el próximo save lo sobrescribe.
                    m_slotThumbs.erase(m_confirmSlot);
                    m_slotThumbOwners.erase(m_confirmSlot);
                }
                else
                    std::cerr << "[confirm] deleteSlot(" << (m_confirmSlot + 1)
                    << ") fallo (sin cambios).\n";
            }
            closeConfirmModal();
        }

        // ── renderConfirmModal (Ola 2 · B2 · CP1): overlay del confirm de
        // borrado. Top-most (se llama tras renderSlotWidgets → la pantalla de
        // slots queda visible ATENUADA detras, look modal DDLC). Espejo del
        // VNSaveMenu C++ renderConfirmDelete + patron del toast (un batch:
        // panel/borde/texto). Default-No (m_confirmCursor=1). Sin input aun
        // (CP2): para ver el look, forzar m_confirmActive=true + m_confirmSlot=N.
        void renderConfirmModal()
        {
            if (!m_confirmActive) return;

            const float VW = (float)viewport::width();
            const float VH = (float)viewport::height();
            draw2d::setupProjection(VW, VH);

            // Panel centrado (geometria compartida con el hit-test, CP2).
            float dX, dY, dW, dH;
            confirmPanelRect(dX, dY, dW, dH);

            draw2d::g_batch->begin();

            // Dim sobre toda la pantalla (los slots siguen detras, atenuados).
            draw2d::drawRect_noBatch(0, 0, VW, VH, 0, 0, 0, 0.55f);

            // Panel + borde rojo (borrar es destructivo).
            draw2d::drawRect_noBatch(dX, dY, dW, dH, 0.09f, 0.06f, 0.08f, 0.97f);
            draw2d::drawRectBorder_noBatch(dX, dY, dW, dH, 0.78f, 0.32f, 0.34f, 0.92f, 2.5f);

            if (m_hasNameFont)
            {
                const float lh = m_nameRenderer.getLineHeight();

                // Titulo: N en 1-based (lo que ve el jugador). FORK 7a: la
                // frase dice `ranura`, la palabra que el motor ya usaba con el
                // creador. Misma clave que el modal C++ de `VNSaveMenu`.
                std::string title = eclips::langPoner(
                    eclips::langUI("Eliminar ranura {ranura}?",
                                   "Delete slot {ranura}?"),
                    {{"ranura", std::to_string(m_confirmSlot + 1)}});
                float tw = m_nameRenderer.measureWidth(title);
                m_nameRenderer.drawText_noBatch(title,
                    dX + (dW - tw) * 0.5f, dY + 22.0f,
                    0.95f, 0.78f, 0.78f, 1.0f);

                // Subtitulo: advertencia (no se puede deshacer).
                std::string sub = "Esta accion no se puede deshacer.";
                float subw = m_nameRenderer.measureWidth(sub);
                m_nameRenderer.drawText_noBatch(sub,
                    dX + (dW - subw) * 0.5f, dY + 22.0f + lh + 8.0f,
                    0.72f, 0.62f, 0.66f, 0.95f);

                // Botones Si / No. Geometria via confirmButtonRect → la MISMA
                // caja que el hit-test lee (CP2): render y clic no divergen.
                const std::string labels[2] = { eclips::langUI("Si", "Yes"),
                                                eclips::langUI("No", "No") };
                for (int i = 0; i < 2; ++i)
                {
                    float bx, btnY, btnW, btnH;
                    confirmButtonRect(i, bx, btnY, btnW, btnH);
                    const bool sel = (i == m_confirmCursor);

                    if (sel)
                    {
                        // "Si" seleccionado = tinte rojo (destructivo); "No" azul.
                        if (i == 0)
                            draw2d::drawRect_noBatch(bx, btnY, btnW, btnH, 0.40f, 0.15f, 0.18f, 0.92f);
                        else
                            draw2d::drawRect_noBatch(bx, btnY, btnW, btnH, 0.22f, 0.18f, 0.38f, 0.92f);
                        draw2d::drawRectBorder_noBatch(bx, btnY, btnW, btnH, 0.50f, 0.65f, 1.0f, 1.0f, 2.0f);
                    }
                    else
                        draw2d::drawRect_noBatch(bx, btnY, btnW, btnH, 0.12f, 0.10f, 0.18f, 0.72f);

                    float lw = m_nameRenderer.measureWidth(labels[i]);
                    m_nameRenderer.drawText_noBatch(labels[i],
                        bx + (btnW - lw) * 0.5f, btnY + (btnH - lh) * 0.5f,
                        0.92f, 0.94f, 0.98f, 1.0f);
                }
            }

            draw2d::g_batch->end();
        }

        // ── hitTestSlotWidget (CP2): índice del slot bajo (vx,vy), o -1. El
        // clic→acción es CP3. Misma geometría que el render (§7.148).
        int hitTestSlotWidget(float vx, float vy) const
        {
            for (int i = 0; i < (int)m_slotWidgets.size(); ++i)
            {
                const auto& sw = m_slotWidgets[i];
                if (vx >= sw.x && vx <= sw.x + sw.width &&
                    vy >= sw.y && vy <= sw.y + sw.height)
                    return i;
            }
            return -1;
        }

        // ── dispatchSlotAction (Ola 2 · CP3): ejecuta la accion de una ranura.
        // C1 cablea SOLO 'cargar' (guardar/borrar avisan aun-no, §7.118). El
        // reparto por rol de 'cargar' espeja a openSaveMenu(Load) (§7.6): desde
        // un menu empuja un juego fresco; a mitad de partida carga in-place.
        // Slot vacio + cargar = no-op: loadGameFromMenu caeria al script_inicial
        // por su fallback (arrancaria un juego nuevo, NO lo que pide un clic en
        // "Vacio") → el guard de occupied lo corta.
        void dispatchSlotAction(int idx)
        {
            if (idx < 0 || idx >= (int)m_slotWidgets.size()) return;
            const SlotWidget& sw = m_slotWidgets[idx];
            const std::string& a = sw.action;

            if (a == "cargar" || a == "load")
            {
                if (!sw.occupied)
                {
                    std::cerr << "[ranura] slot " << (sw.slot + 1)
                        << " vacio: nada que cargar.\n";
                    return;
                }
                if (isPauseOverGame())
                {
                    // Ola 3 · Slice B: cargar in-place en el juego CONGELADO de
                    // abajo, luego desmontar TODAS las capas de menú hasta él
                    // (cmdPopAbove, depth-independiente: el juego puede tener un
                    // título debajo → cmdPopToBottom over-popearía al título).
                    m_pausedGame->loadFromSlot(sw.slot);
                    m_stateStack->cmdPopAbove(m_pausedGame);
                }
                else if (m_role == VNRole::Menu)
                {
                    // Título → load: popear la pantalla de Load ANTES de empujar el
                    // juego (FIFO: pop loadscreen -> push juego = [titulo, juego]).
                    // cmdPop(this) = el verbo 'fin'. (this sobrevive: pop diferido.)
                    m_stateStack->cmdPop(this);
                    loadGameFromMenu(sw.slot);
                }
                else
                    loadFromSlot(sw.slot);       // rol Game: in-place, sin pop/push
                return;
            }
            if (a == "guardar" || a == "save")
            {
                // Ola 3 · Slice B: guardar-desde-pausa. La ranura vive en un
                // save-screen autorable (rol Menu) que heredó m_pausedGame → el
                // snapshot es del juego congelado de abajo, no de esta pantalla.
                if (isPauseOverGame())
                {
                    m_pausedGame->saveToSlot(sw.slot);
                    // Refresco inverso al de borrar: el slot pasó de vacío a
                    // ocupado → re-leer meta y actualizar TODAS las ranuras de
                    // ese slot (sw es const ref; el contenedor manda).
                    VNSlotInfo info = VNSaveMenu::readSlot(sw.slot);
                    for (auto& w : m_slotWidgets)
                        if (w.slot == sw.slot)
                        {
                            w.occupied = info.occupied;
                            w.scene = info.scene;
                            w.preview = info.preview;
                            w.timestamp = info.timestamp;
                        }
                    // 6.1 CP1: recargar el thumb del slot recién sobrescrito = la
                    // invalidación (captureThumbnail ya escribió el PNG nuevo).
                    // §7.34 keep-on-fail: no blanqueo un thumb bueno si load falla.
                    {
                        std::string tp = "saves/thumbs/slot_"
                            + std::to_string(sw.slot) + ".png";
                        Texture fresh;
                        if (fresh.load(tp))
                        {
                            m_slotThumbs[sw.slot].swapWith(fresh);
                            m_slotThumbOwners[sw.slot] = sw.scopeOwner;
                        }
                    }
                }
                else
                    std::cerr << "[ranura] 'guardar' sin juego abajo: una pantalla "
                    "rol Menu sin pausa no tiene partida que guardar.\n";
                return;
            }
            if (a == "borrar" || a == "delete")
            {
                // Slot vacio = nada que borrar (no-op, espejo del guard de
                // cargar-vacio). Borrar OCUPADO es destructivo → SIEMPRE
                // confirma: el modal es horneado, no hay borrado sin "Si"
                // (§7.6). El borrado real corre en confirmModalChoose(0).
                if (!sw.occupied)
                {
                    std::cerr << "[ranura] slot " << (sw.slot + 1)
                        << " vacio: nada que borrar.\n";
                    return;
                }
                m_confirmActive = true;
                m_confirmSlot = sw.slot;
                m_confirmCursor = 1;            // default-No
                return;
            }
            std::cerr << "[ranura] accion vacia o desconocida: '" << a
                << "' (valida hoy: cargar). Slot ignorado.\n";
        }

        // ── applyControlDrag (CP3): valor desde mouse-x, snap a paso, escribe ─
        void applyControlDrag(int idx, float vx)
        {
            if (idx < 0 || idx >= (int)m_controls.size()) return;
            BoundControl& bc = m_controls[idx];
            const ControlBarGeometry g = (bc.render == CtrlRender::Bar)
                ? resolveControlBarGeometry(bc) : ControlBarGeometry{};
            const float start = (bc.render == CtrlRender::Bar) ? g.valueStart : bc.x;
            const float end = (bc.render == CtrlRender::Bar) ? g.valueEnd : bc.x + bc.width;
            float t = (end > start) ? (vx - start) / (end - start) : 0.0f;
            if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
            float raw = bc.vmin + t * (bc.vmax - bc.vmin);
            if (bc.vstep > 0.0f)
            {
                float steps = (raw - bc.vmin) / bc.vstep;
                steps = (float)(long)(steps + 0.5f);   // round (steps>=0), sin <cmath>
                raw = bc.vmin + steps * bc.vstep;
            }
            if (raw < bc.vmin) raw = bc.vmin; else if (raw > bc.vmax) raw = bc.vmax;
            bc.value = raw;
            writeControlValue(bc.desc->key, raw);
        }

        // ── writeControlValue: aplica controles en vivo ──────────────────────
        // Audio/texto escriben Settings al instante. Display (R4) abre una
        // transaccion: SDL recibe el candidato ahora; Settings solo tras "Si".
        void writeControlValue(CtrlSetting key, float displayValue)
        {
            switch (key)
            {
            case CtrlSetting::MusicVol: {
                float v01 = displayValue / 100.0f;
                Settings::setMusicVolume(v01);
                m_audio.setMusicVolume(v01);
            } break;
            case CtrlSetting::SfxVol: {
                float v01 = displayValue / 100.0f;
                Settings::setSfxVolume(v01);
                m_audio.setSoundVolume(v01);
            } break;
            case CtrlSetting::TextSpeed: {
                const TextSpeedDomain* domain = nullptr;
                for (const BoundControl& control : m_controls)
                    if (control.desc && control.desc->key == CtrlSetting::TextSpeed)
                    {
                        domain = &control.textSpeedDomain;
                        break;
                    }
                if (domain && domain->isNumeric())
                {
                    Settings::setTextSpeed(textSpeedFromDomainCoordinate(
                        *domain, displayValue));
                    break;
                }
                int idx = (int)(displayValue + 0.5f);
                const std::vector<TextSpeedCatalogEntry>* options = nullptr;
                for (const BoundControl& control : m_controls)
                    if (control.desc && control.desc->key == CtrlSetting::TextSpeed)
                    {
                        options = &control.textSpeedOptions;
                        break;
                    }
                const std::vector<TextSpeedCatalogEntry> fallback =
                    textSpeedCatalog(effectiveTextSpeedForControl());
                const auto& catalog = options ? *options : fallback;
                if (catalog.empty()) break;
                if (idx < 0) idx = 0;
                if (idx >= (int)catalog.size()) idx = (int)catalog.size() - 1;
                Settings::setTextSpeed(catalog[idx].value);
            } break;
            case CtrlSetting::SkipUnseen:
                Settings::setSkipUnseen(displayValue >= 0.5f);
                break;
            case CtrlSetting::Fullscreen: {
                display_change::State candidate = currentDisplayState();
                candidate.fullscreen = displayValue >= 0.5f;
                beginDisplayChange(candidate);
            } break;
            case CtrlSetting::Resolution: {
                int idx = (int)(displayValue + 0.5f);
                if (idx < 0) idx = 0;
                if (idx >= (int)m_resOptions.size()) idx = (int)m_resOptions.size() - 1;
                if (idx >= 0 && idx < (int)m_resOptions.size())
                {
                    const window_resolution::Opcion& o = m_resOptions[idx];
                    display_change::State candidate = currentDisplayState();
                    candidate.windowW = o.w;
                    candidate.windowH = o.h;
                    candidate.windowSizeUserSet = true;
                    beginDisplayChange(candidate);
                }
            } break;
            // ── CP-R4: EL IDIOMA SE PUBLICA CON SU GENERACION ───────────────
            // El selector construye un LangStore candidato y no modifica el
            // objeto vivo. Engine consume el paquete en el corte prerender;
            // todos los suscriptores hornean contra el mismo árbol espejo.
            case CtrlSetting::Language: {
                if (!m_lang || !m_manifest) break;
                const std::vector<std::string>& cat = m_manifest->idiomas;
                if (cat.empty()) break;
                int idx = (int)(displayValue + 0.5f);
                if (idx < 0) idx = 0;
                if (idx >= (int)cat.size()) idx = (int)cat.size() - 1;
                const std::string codigo = cat[idx];

                // LA GUARDA DE «NO CAMBIÓ», y vive acá y no en los call-sites
                // porque son CINCO y sólo el de teclado la tiene (`:7392`): los
                // tres del mouse y el drag escriben sin comparar. Sin esto, un
                // clic repetido sobre la misma caja paga un `clear()` + hasta
                // 1+N archivos con `directory_iterator` + un latido en consola.
                // Lo que NO mata es el barrido con flechas, donde cada paso sí
                // es un código distinto y sí cuesta su carga: con catálogos de
                // 2 a 7 es pagable, y queda escrito para que nadie lo
                // redescubra creyendo que la guarda lo cubría.
                if (codigo == m_lang->code()) break;

                // Candidato: el LangStore / memo / Settings vivos no se tocan
                // hasta el commit. El store candidato queda registrado durante
                // TODO el prepare: cada suscriptor ve el mismo espejo.
                // EL DUENO DE LA PENDIENTE (`R1`). Esta pantalla puede morir
                // antes del corte -- `ESC` en rol Menu popea desde `handleEvent` y
                // `processQueue` la destruye en el MISMO frame, antes del
                // `commitPendingPresentation()`-- asi que el token de suscripcion
                // viaja hasta `prepareGeneration` y el que se va aborta SU
                // transaccion, no la de otro. Sonda: `correr_probe_r1_pausa.ps1`.
                m_candidateLanguage = codigo;
                m_hasCandidateLanguage = true;
                if (m_fontSources)
                {
                    std::filesystem::path raiz =
                        std::filesystem::path(m_manifest->sourcePath).parent_path();
                    if (raiz.empty()) raiz = ".";
                    const std::string original = m_manifest->idiomas.empty()
                        ? codigo : m_manifest->idiomas.front();
                    m_candidateLangStore = std::make_unique<LangStore>();
                    m_candidateLangStore->load(codigo, raiz.string(), original);

                    // `PUERTAS-QUE-NO-DIFIEREN`: el mismo trato que `applyTheme`
                    // aprendio en `R2`. Si la pendiente ya estaba APROBADA no
                    // somos parte de ESTE corte, y seguir de largo publicaria el
                    // idioma con los atlas del anterior. El candidato de arriba
                    // queda ARMADO y el pedido se reintenta en `update`, ya
                    // pasado el corte. Guardianes `21.1`/`21.2`.
                    //
                    // Y EL REGISTRO EN EL STORE VA DESPUES DEL PEDIDO -- es lo
                    // unico que este molde NO comparte con el del tema. El tema
                    // difiere un candidato que vive entero adentro del VN;
                    // `fontSetCandidateLanguage` en cambio deja el idioma nuevo
                    // en el resolver que usa el HORNEADO
                    // (`fontFinalizePublisher` lo toma con `LangCandidatoGuard`).
                    // Registrarlo sin estar en el corte es hacer que el paquete
                    // AJENO hornee con nuestro idioma: el mismo defecto que
                    // veniamos a cerrar, entrando por la otra puerta.
                    if (fontRequestPublisher(
                            *m_fontSources, FontPublisherDoor::Selector,
                            m_generationSubscription))
                        fontSetCandidateLanguage(
                            *m_fontSources, m_candidateLangStore.get());
                    else
                        m_idiomaDiferido = true;
                }
            } break;
            default: break;
            }
        }

        // ── readControlValue: valor de display del setting (live) ─────────────
        // Punto único de lectura, por key. Lockeables → valor 'efectivo': locked →
        // el theme manda; si no → el override del jugador.
        float readControlValue(CtrlSetting key) const
        {
            switch (key)
            {
            case CtrlSetting::MusicVol: return Settings::musicVolume() * 100.0f;
            case CtrlSetting::SfxVol:   return Settings::sfxVolume() * 100.0f;
            case CtrlSetting::TextSpeed: {
                const TextSpeedDomain* domain = nullptr;
                for (const BoundControl& control : m_controls)
                    if (control.desc && control.desc->key == CtrlSetting::TextSpeed)
                    {
                        domain = &control.textSpeedDomain;
                        break;
                    }
                const TextSpeed effective = effectiveTextSpeedForControl();
                if (domain && domain->isNumeric())
                    return textSpeedDomainCoordinate(*domain, effective);
                std::vector<TextSpeedCatalogEntry> catalog;
                for (const BoundControl& control : m_controls)
                    if (control.desc && control.desc->key == CtrlSetting::TextSpeed)
                    {
                        catalog = control.textSpeedOptions;
                        break;
                    }
                if (catalog.empty()) catalog = textSpeedCatalog(effective);
                const int index = textSpeedCatalogIndex(catalog, effective);
                return index < 0 ? 0.0f : (float)index;
            }
            case CtrlSetting::SkipUnseen: {
                bool on = m_theme.skipUnseenLocked
                    ? m_theme.skipUnseen
                    : Settings::skipUnseen(m_theme.skipUnseen);
                return on ? 1.0f : 0.0f;
            }
            case CtrlSetting::Fullscreen:
                if (m_displayChange.active())
                    return m_displayChange.candidate().fullscreen ? 1.0f : 0.0f;
                return Settings::fullscreen() ? 1.0f : 0.0f;
            case CtrlSetting::Resolution: {
                // Índice lógico desde el W×H ELEGIDO (Settings), NO del tamaño
                // físico: el WM clampea / DPI-escala SDL_GetWindowSize y el índice
                // se volvía errático (no retrocedía, se atascaba). Settings guarda
                // el W×H exacto del catálogo (lo escribe writeControlValue) → match
                // exacto y estable. Sin override → tamaño de boot como semilla.
                if (m_resOptions.empty()) return 0.0f;
                int cw = 0, ch = 0;
                if (m_displayChange.active())
                {
                    cw = m_displayChange.candidate().windowW;
                    ch = m_displayChange.candidate().windowH;
                }
                else
                {
                    if (m_window) SDL_GetWindowSize(m_window, &cw, &ch);
                    cw = Settings::windowWidth(cw);
                    ch = Settings::windowHeight(ch);
                }
                int best = 0; long bestD = -1;
                for (int i = 0; i < (int)m_resOptions.size(); ++i)
                {
                    if (m_resOptions[i].w == cw && m_resOptions[i].h == ch)
                        return (float)i;
                    long dx = (long)m_resOptions[i].w - cw;
                    long dy = (long)m_resOptions[i].h - ch;
                    long d = dx * dx + dy * dy;
                    if (bestD < 0 || d < bestD) { bestD = d; best = i; }
                }
                return (float)best;
            }
            // `CP5`: índice EXACTO, y ésta es la diferencia con `resolucion`
            // que justifica que el read no se pueda compartir. Allá la
            // distancia W×H significa algo -1279x719 está "cerca" de
            // 1280x720-; entre dos códigos de idioma no significa nada. Sin
            // match, el original: es lo que `langSeleccionar` ya garantiza que
            // el efectivo sea, así que este 0 es la misma respuesta.
            case CtrlSetting::Language: {
                if (!m_lang || !m_manifest) return 0.0f;
                const std::vector<std::string>& cat = m_manifest->idiomas;
                for (int i = 0; i < (int)cat.size(); ++i)
                    if (cat[i] == m_lang->code()) return (float)i;
                return 0.0f;
            }
            default: return 0.0f;
            }
        }

        // ── isControlLocked: ¿el theme lockea este setting? ──────────────────
        // Volumen/sonido/fullscreen no se lockean; solo los de comportamiento.
        // Reusa los flags vivos de Theme (los que honra el SettingsState de C++).
        bool isControlLocked(CtrlSetting key) const
        {
            switch (key)
            {
            case CtrlSetting::TextSpeed:  return m_theme.textSpeedLocked;
            case CtrlSetting::SkipUnseen: return m_theme.skipUnseenLocked;
            default: return false;
            }
        }

        void cmdCapa(const EclipCmd& cmd)
        {
            const std::string& name = cmd.arg1;
            const std::string& path = cmd.arg2;

            // ── COLOR-SOLIDO: la capa es un rect de color, no una imagen ────
            // Sale ANTES de tocar el AssetManager: no hay ruta que cargar ni
            // que pinear. `arg2` viene vacío desde el parser justamente para
            // que este camino no pueda equivocarse (excluyentes por
            // construcción, no por convención).
            if (!cmd.layerColorHex.empty())
            {
                f32 cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
                EclipParser::parseHexColor(cmd.layerColorHex, cr, cg, cb, &ca);

                f32 tr = 1.0f, tg = 1.0f, tb = 1.0f;
                if (!cmd.tintHex.empty())
                    EclipParser::parseHexColor(cmd.tintHex, tr, tg, tb);

                // UPDATE (RT-1): la línea cambia solo lo que nombró. El color
                // SIEMPRE se nombró — es el argumento, no una clave opcional —
                // así que se pisa; el resto lo decide `toLayerPatch`.
                SceneElement* prev = m_sceneLayers.get(name);
                if (prev)
                {
                    prev->colorHex = cmd.layerColorHex;
                    prev->colorR = cr; prev->colorG = cg;
                    prev->colorB = cb; prev->colorA = ca;
                    prev->texture = nullptr;   // deja de ser imagen
                    m_sceneLayers.applyLayerPatch(name, nullptr,
                        cmd.toLayerPatch(tr, tg, tb));
                    std::cout << "[VNScriptState] capa color actualizada: '"
                        << name << "'" << std::endl;
                    return;
                }

                m_sceneLayers.addColorLayer(name, cmd.layerColorHex,
                    cr, cg, cb, ca, cmd.layerZ,
                    cmd.layerX, cmd.layerY, cmd.layerW, cmd.layerH,
                    1.0f, m_activeScopeOwner);

                if (cmd.parallaxFactor != 0.0f)
                    m_sceneLayers.setParallax(name, cmd.parallaxFactor);
                if (!cmd.tintHex.empty())
                    m_sceneLayers.setTint(name, cmd.tintHex, tr, tg, tb);
                return;
            }

            // Cargar textura vía AssetManager
            Texture* tex = m_assets.get(path);
            if (!tex)
            {
                std::cerr << "[VNScriptState] capa: no se pudo cargar '"
                    << path << "'" << std::endl;
                return;
            }
            m_assets.pin(path);

            // Tinte estático (la línea manda: sin tinte= = capa limpia).
            f32 tr = 1.0f, tg = 1.0f, tb = 1.0f;
            if (!cmd.tintHex.empty())
                EclipParser::parseHexColor(cmd.tintHex, tr, tg, tb);

            // Si ya existe una capa con ese nombre, ACTUALIZAR (no re-crear).
            // RT-1: la ley del update vive en `SceneLayerManager` — el dueno
            // del estado que se pisa es el dueno de la regla que decide si se
            // pisa (§7.323). Aca solo queda el dispatch.
            SceneElement* existing = m_sceneLayers.get(name);
            if (existing)
            {
                m_sceneLayers.applyLayerPatch(name, tex, cmd.toLayerPatch(tr, tg, tb));
                std::cout << "[VNScriptState] capa actualizada: '"
                    << name << "'" << std::endl;
                return;
            }

            // Agregar nueva capa
            m_sceneLayers.addLayer(name, tex, cmd.layerZ,
                cmd.layerX, cmd.layerY,
                cmd.layerW, cmd.layerH, 1.0f, m_activeScopeOwner);

            // Parallax si se especificó
            if (cmd.parallaxFactor != 0.0f)
                m_sceneLayers.setParallax(name, cmd.parallaxFactor);

            if (!cmd.tintHex.empty())
                m_sceneLayers.setTint(name, cmd.tintHex, tr, tg, tb);

            // Trackear la ruta para poder unpinear en cleanup
            m_layerPaths[name] = path;
            m_layerPathOwners[name] = m_activeScopeOwner;
        }

        // ── ocultar alias ──────────────────────────────────────────────────────

        void cmdOcultar(const std::string& alias, int effect = 0)
        {
            for (auto& ac : m_activeChars)
            {
                if (ac.alias == alias)
                {
                    if (effect == 1)
                    {
                        // Instant — eliminar inmediatamente
                        ac.fadeAlpha = 0.0f;
                        ac.fadeTarget = 0.0f;
                        // Se limpia en el remove_if del update()
                    }
                    else if (effect == 2)
                    {
                        // Slide out — deslizar fuera, eliminar al completar
                        ac.slideTargetX = slideEndX(ac.position);
                        ac.slidingOut = true;
                    }
                    else
                    {
                        // Fade out (default)
                        ac.fadeTarget = 0.0f;
                    }
                    return;
                }
            }
        }

        // ── Calcular offset de entrada para slide ───────────────────────────
        // Personajes del lado izquierdo entran desde la izquierda,
        // los del derecho desde la derecha.
        static float slideStartX(VNPosition pos)
        {
            switch (pos)
            {
            case VNPosition::Izquierda:
            case VNPosition::CentroIzq:
                return -1920.0f; // Entra desde la izquierda
            default:
                return  1920.0f; // Entra desde la derecha
            }
        }

        // ── Calcular offset de salida para slide ────────────────────────────
        static float slideEndX(VNPosition pos)
        {
            switch (pos)
            {
            case VNPosition::Izquierda:
            case VNPosition::CentroIzq:
                return -1920.0f; // Sale por la izquierda
            default:
                return  1920.0f; // Sale por la derecha
            }
        }




        // ── FBO cache helpers ──────────────────────────────────────────
        void releaseCachedTex(VNActiveChar& ac)
        {
            if (ac.cachedTex)
            {
                glDeleteTextures(1, &ac.cachedTex);
                ac.cachedTex = 0;
                ac.cachedTexW = 0;
                ac.cachedTexH = 0;
            }
        }

        void releaseAllCachedTex()
        {
            for (auto& ac : m_activeChars)
                releaseCachedTex(ac);
        }

        void refreshTextSpeedControls()
        {
            for (BoundControl& control : m_controls)
            {
                if (!control.desc || control.desc->key != CtrlSetting::TextSpeed)
                    continue;
                control.textSpeedDomain = m_theme.textSpeedDomain;
                reconcileTextSpeedPresentation(control);
                if (control.textSpeedDomain.isNumeric())
                {
                    control.textSpeedOptions.clear();
                    control.opciones.clear();
                    control.vmin = control.textSpeedDomain.min;
                    control.vmax = textSpeedDomainEndpoint(control.textSpeedDomain);
                    control.vstep = control.textSpeedDomain.step;
                    control.pasoExplicito = true;
                    control.value = textSpeedDomainCoordinate(
                        control.textSpeedDomain, effectiveTextSpeedForControl());
                }
                else
                {
                    const TextSpeed effective = effectiveTextSpeedForControl();
                    control.textSpeedOptions = textSpeedCatalog(effective);
                    control.opciones.clear();
                    for (const auto& option : control.textSpeedOptions)
                        control.opciones.push_back(eclips::langTr(
                            eclips::langActivo(), option.labelEs.c_str(), option.labelEn.c_str()));
                    control.vmin = 0.0f;
                    control.vmax = control.opciones.empty() ? 0.0f :
                        static_cast<float>(control.opciones.size() - 1);
                    control.vstep = 1.0f;
                    control.pasoExplicito = false;
                    control.value = textSpeedCatalogIndex(
                        control.textSpeedOptions, effective);
                }
            }
        }

        // Aplica un theme desde disco en vivo: carga, re-init de fuente SOLO si
        // cambió (evita leak §1.28), unpin/pin de sprites (invariante §7.26),
        // y registra _tema_activo para saves. Llamado por el comando cargar_tema
        // y por la recarga del manifest (capa 2 del hot-reload).
        void applyTheme(const std::string& path)
        {
            Theme newTheme = Theme::loadFromFile(path);
            m_candidateTheme = std::move(newTheme);
            m_candidateThemePath = path;
            m_hasCandidateTheme = true;
            if (m_fontSources && !m_applyingManifestPresentation)
            {
                // `R2`: si la pendiente ya estaba APROBADA no somos parte de
                // ESTE corte -- la ley del store es que una aprobacion es del
                // que la gano. Diferido: el candidato de arriba queda ARMADO y
                // el pedido se reintenta despues del corte, en `update`. Cuesta
                // un frame de latencia y el cambio del creador no se pierde.
                //
                // Descartarlo era la otra rama del fork y NO se firmo: perder
                // en silencio un comando del creador es exactamente la familia
                // de la que salio `R1` (`SALIR-DEL-MENU-DESCARTA-O-APLICA`).
                if (!fontRequestPublisher(
                        *m_fontSources, FontPublisherDoor::ApplyTheme,
                        m_generationSubscription))
                    m_temaDiferido = true;
            }
        }

        // Suelta el theme activo y vuelve al default de Eclips (Theme::defaults(),
        // la struct genérica sin rostro — §0 Modo Creador). Hermano de applyTheme
        // pero SIN disco: el "default de Eclips" no es un archivo. Lo usa la recarga
        // del manifest (F8) cuando el creador BORRA tema_default y el script no tomó
        // el control. Re-init de fuente en el caso INVERSO a applyTheme (el viejo
        // tenía custom, el default no → volver a la del sistema), unpin/pin de
        // sprites (§7.26), y _tema_activo a "" (coherente con onEnter sin tema).
        // TODO(modo-creador): se compila fuera en build de export.
        void applyDefaultTheme()
        {
            Theme newTheme = Theme::defaults();
            if (m_theme.defaultDialogBox.hasCustomFont() &&
                !newTheme.defaultDialogBox.hasCustomFont())
            {
                m_hasFont = initBaseRenderer(
                    m_textRenderer, Settings::defaultFontPath(), 34.0f, 0.0f, "texto");
                m_hasNameFont = initBaseRenderer(
                    m_nameRenderer, Settings::defaultFontPath(), 26.0f, 0.0f, "chrome");
            }
            unpinThemeSprites(m_theme);
            m_theme = newTheme;
            pinThemeSprites(m_theme);
            pinControlStyles();
            refreshTextSpeedControls();
            m_variables["_tema_activo"] = "";
        }

        // Hot-reload del manifest (F8). Engine lo llama desde
        // commitPendingPresentation, ANTES del render y con GL vivo, aunque
        // un overlay impida update(). Publica hablante, tema y estilos en
        // el mismo corte que la generación — cero frames mixtos.
        void onManifestReloaded() override
        {
            m_applyingManifestPresentation = true;
            m_dialogBox.setActiveSpeaker(
                m_currentSpeaker.empty()
                    ? nullptr
                    : resolveSpeakerManifest(m_currentSpeaker));
            m_dialogBox.setSpeakerName(resolveSpeakerName(m_currentSpeaker));
            refreshControlStylesAfterManifestReload();
            m_applyingManifestPresentation = false;
            m_manifestReloadPending = false;
        }

        // El LangStore nació con un F8 (ficha `F8-SIN-IDIOMA`, fork 4c). Este
        // state cacheó el puntero en su ctor —desde main.cpp— y si el proyecto
        // no declaraba `idiomas`, lo cacheó en nullptr; sin esto el selector de
        // `CP5` se queda mudo (`if (!m_lang || !m_manifest) break;`) hasta que
        // la escena se re-empuje.
        //
        // NO es diferido como sus hermanos, y la asimetría es a propósito: los
        // otros dos levantan un flag porque su reacción real hace trabajo GL
        // (§1.34). Esto es una asignación de puntero — diferirla sólo agregaría
        // un frame en el que `m_lang` sigue rancio.
        void onLangStoreChanged(eclips::LangStore* store) override
        {
            m_lang = store;
        }

        // Hot-reload de texturas (F8). Igual que onManifestReloaded corre dentro
        // de processEvents → §1.34 prohíbe GL acá. Solo levanta el flag; el
        // reloadAll real vive en update(). Ungated: sin gate de manifest.
        void onReloadTextures() override
        {
            m_textureReloadPending = true;
        }

        // ── Sprites PNG de las cajas de diálogo del theme ─────────────────
        // Pin/unpin de TODOS los sprites declarados en TODAS las cajas del
        // theme (default + nominadas). Llamado en cargar_tema y onEnter/onExit.
        //
        // Razón: la caja activa puede cambiar sin comando explícito (cambio de
        // speaker -> caja del personaje activo). Pinear todo upfront evita
        // glitches por evicción LRU intermedia y simplifica el modelo: el
        // theme es una unidad de assets, no piezas independientes.
        //
        // Pin/unpin son idempotentes (flag bool en AssetManager), por lo que
        // paths duplicados entre default y nominadas no causan problemas.
        //
        // Firma non-const (Theme&) desde la sesión de 9-slice: tras cargar
        // cada sprite, se valida que los bordes 9-slice declarados quepan
        // dentro de las dimensiones reales del PNG. Si bL+bR > texW (o
        // análogo en Y), el creador puso bordes imposibles. Acción:
        //   - log warn pedagógico una vez (en pin time, no en render).
        //   - clear los 4 bordes de ese style a 0 (fallback stretch puro).
        // De ahí la mutabilidad del Theme.
        const ControlStylePart* controlPart(const BoundControl& bc,
            const std::string& component) const
        {
            if (!bc.controlStyle) return nullptr;
            std::string suffix;
            const bool locked = isControlLocked(bc.desc ? bc.desc->key : CtrlSetting::Unknown);
            if (locked) suffix = "_bloqueado";
            else if (m_draggingControl >= 0 && &bc == &m_controls[m_draggingControl]) suffix = "_arrastrando";
            else if (m_focoDelPuntero && m_hoveredControl >= 0 && &bc == &m_controls[m_hoveredControl]) suffix = "_al_pasar";
            else if (!m_focoDelPuntero && m_focoControl >= 0 && &bc == &m_controls[m_focoControl]) suffix = "_al_enfocar";
            if (!suffix.empty())
            {
                auto it = bc.controlStyle->variants.find(component + suffix);
                if (it != bc.controlStyle->variants.end() && it->second.declared && it->second.valid)
                {
                    if (it->second.none || it->second.color ||
                        const_cast<VNScriptState*>(this)->m_assets.get(it->second.value))
                        return &it->second;
                }
            }
            if (component == "track") return &bc.controlStyle->track;
            if (component == "fill") return &bc.controlStyle->fill;
            return &bc.controlStyle->thumb;
        }

        ControlBarGeometry resolveControlBarGeometry(const BoundControl& bc) const
        {
            ControlBarGeometry g;
            g.valueStart = bc.x; g.valueEnd = bc.x + bc.width;
            g.trackY = bc.y;
            g.track = controlPart(bc, "track");
            g.fill = controlPart(bc, "fill");
            g.thumb = controlPart(bc, "thumb");
            const ControlStylePart* base = bc.controlStyle ? &bc.controlStyle->track : nullptr;
            if (base && base->declared && base->valid && !base->none && !base->color)
                if (Texture* tex = const_cast<VNScriptState*>(this)->m_assets.get(base->value))
                    g.trackH = (float)tex->getHeight();
            if (g.thumb && g.thumb->declared && g.thumb->valid && !g.thumb->none && !g.thumb->color)
                if (Texture* tex = const_cast<VNScriptState*>(this)->m_assets.get(g.thumb->value))
                    g.thumbW = (float)tex->getWidth(), g.thumbH = (float)tex->getHeight();
            // ── `D-52`..`D-56` · EL NUMERO ESCRITO GANA SOBRE LA TEXTURA ─────
            // Precedencia: numero declarado > tamano nativo del PNG > default.
            // Es lo que permite COMPONER las dos cosas -vestir con arte y
            // ademas decidir a que tamano se dibuja- en vez de obligar a elegir
            // una. Sin declarar nada, estas tres lineas son un no-op y manda el
            // bloque de arriba, igual que antes (`D-56`: identidad).
            if (bc.controlStyle)
            {
                if (bc.controlStyle->rielAlturaDeclarada)
                    g.trackH = bc.controlStyle->rielAltura;
                if (bc.controlStyle->perillaAnchoDeclarada)
                    g.thumbW = bc.controlStyle->perillaAncho;
                if (bc.controlStyle->perillaAlturaDeclarada)
                    g.thumbH = bc.controlStyle->perillaAltura;
            }
            g.hitHalfX = controlThumbHalfWidth(bc);
            g.hitHalfY = controlThumbHalfHeight(bc);
            // ── `D-55` · EL EJE DE VALOR, y por que se calcula AL FINAL ──────
            // `dentro` mete el recorrido de la perilla medio ancho hacia
            // adentro, para que su caja visual no sobresalga en los extremos.
            // Necesita `g.thumbW` YA resuelto, asi que va despues del bloque de
            // arriba y no arriba con el resto de la geometria.
            //
            // Sin `dentro` esto no corre, y el eje sigue siendo exactamente
            // `bc.x .. bc.x+ancho` — el que `ECLIPSCRIPT_DOCS` firmo en v0.8.54.
            if (bc.controlStyle && bc.controlStyle->perillaDentro)
            {
                const float mitad = g.thumbW * 0.5f;
                if (bc.width > g.thumbW)
                {
                    g.valueStart = bc.x + mitad;
                    g.valueEnd   = bc.x + bc.width - mitad;
                }
                else
                {
                    // La perilla no cabe en el riel. El eje COLAPSA al centro
                    // en vez de invertirse: un eje invertido haria que
                    // arrastrar hacia la derecha BAJARA el valor, que es un
                    // sintoma mucho peor que un control que no se mueve.
                    g.valueStart = g.valueEnd = bc.x + bc.width * 0.5f;
                }
            }
            return g;
        }

        // Rebind the live controls after replacing the manifest. This is the
        // single production path used by F8 and by the test seam; it keeps
        // non-Bar controls unstyled and restores Theme ownership after the
        // skin set has been replaced.
#ifdef ECLIPS_TEST_SEAM
    public:
#endif
        void refreshControlStylesAfterManifestReload()
        {
            for (auto& control : m_controls)
            {
                control.controlStyle = nullptr;
                if (m_manifest && !control.styleName.empty() && control.render == CtrlRender::Bar)
                {
                    auto sit = m_manifest->estilosControl.find(control.styleName);
                    if (sit != m_manifest->estilosControl.end())
                        control.controlStyle = &sit->second;
                }
            }
            pinControlStyles();
            pinThemeSprites(m_theme);
        }

        void unpinControlStyles()
        {
            for (const auto& p : m_pinnedControlStyles) m_assets.unpin(p);
            m_pinnedControlStyles.clear();
        }

        void pinControlStyles()
        {
            unpinControlStyles();
            if (!m_manifest) return;
            std::map<std::string, std::vector<ControlStylePart>> unique;
            for (const auto& kv : m_manifest->estilosControl)
            {
                const auto& s = kv.second;
                auto take = [&](const ControlStylePart& p) {
                    if (p.declared && p.valid && !p.none && !p.color && !p.value.empty())
                        unique[p.value].push_back(p);
                };
                take(s.track); take(s.fill); take(s.thumb);
                for (const auto& v : s.variants) take(v.second);
            }
            for (const auto& entry : unique)
            {
                const std::string& p = entry.first;
                if (m_assets.get(p)) { m_assets.pin(p); m_pinnedControlStyles.push_back(p); }
                else for (const auto& use : entry.second)
                    std::cerr << "[control] fuente='" << use.sourcePath << "' linea " << use.line
                        << " alias='" << use.styleName << "' propiedad='" << use.property
                        << "' estado='" << use.stateName << "': ruta invalida/no encontrada '" << p
                        << "'; consecuencia: hereda BASE/fallback; reparacion: corrija la ruta.\n";
            }
        }

        void pinThemeSprites(Theme& theme)
        {
            // Lambda granular: pinea un PNG + valida bordes 9-slice contra
            // el tamaño real de la textura. Las refs a los 4 sliceXxx
            // permiten clear in-place al detectar overflow.
            auto pinOne = [&](const std::string& path,
                f32& bL, f32& bR, f32& bT, f32& bB,
                const std::string& alias, const std::string& tag) {
                    if (path.empty()) return;
                    Texture* tex = m_assets.get(path);
                    if (!tex) {
                        std::cout << "[Theme] Aviso: sprite '" << path
                            << "' no se pudo cargar, usando primitivas." << std::endl;
                        return;
                    }
                    m_assets.pin(path);
                    std::cout << "[Theme] Sprite cargado: " << path << std::endl;

                    // 9-slice source-overflow check. Solo aplica si el creador
                    // declaró bordes (>0); con todos en 0 el path stretch
                    // ignora estos campos y no hay nada que validar.
                    const bool has9Slice = (bL > 0.0f || bR > 0.0f
                        || bT > 0.0f || bB > 0.0f);
                    if (!has9Slice) return;

                    const f32 texW = (f32)tex->getWidth();
                    const f32 texH = (f32)tex->getHeight();
                    if (bL + bR > texW || bT + bB > texH) {
                        std::cout << "[Theme] Aviso: bordes 9-slice de " << tag
                            << " '" << alias << "' (izq+der=" << (bL + bR)
                            << ", arriba+abajo=" << (bT + bB)
                            << ") exceden el sprite ("
                            << tex->getWidth() << "x" << tex->getHeight()
                            << "). 9-slice deshabilitado; fallback a stretch."
                            << std::endl;
                        bL = bR = bT = bB = 0.0f;
                    }
                };

            // Lambda mid-level para sprites simples (sin 9-slice): el indicador
            // de avance se dibuja a tamaño nativo, no se estira por esquinas, así
            // que no pasa por pinOne (que exige los 4 refs de borde 9-slice).
            // Mismo núcleo: skip si vacío, get, warn+return si falla, pin + log.
            auto pinSimple = [&](const std::string& path) {
                if (path.empty()) return;
                Texture* tex = m_assets.get(path);
                if (!tex) {
                    std::cout << "[Theme] Aviso: sprite '" << path
                        << "' no se pudo cargar, usando primitivas." << std::endl;
                    return;
                }
                m_assets.pin(path);
                std::cout << "[Theme] Sprite cargado: " << path << std::endl;
                };

            // Lambda mid-level: encapsula los PNGs de un DialogBoxStyle
            // (caja + nameplate 9-slice; indicador simple) bajo un mismo alias.
            auto pinBox = [&](DialogBoxStyle& s, const std::string& alias) {
                pinOne(s.boxSpritePath,
                    s.boxSliceLeft, s.boxSliceRight,
                    s.boxSliceTop, s.boxSliceBottom,
                    alias, "caja");
                pinOne(s.namePlateSpritePath,
                    s.namePlateSliceLeft, s.namePlateSliceRight,
                    s.namePlateSliceTop, s.namePlateSliceBottom,
                    alias, "nameplate");
                // Cara PNG del botón de choice: idle + hover comparten los
                // mismos bordes 9-slice (mismo arte, distinto estado). Si hover
                // hace overflow, el clear in-place baja también el idle a stretch
                // → look consistente (ambos son el mismo botón).
                pinOne(s.choiceBtnSpritePath,
                    s.choiceBtnSliceLeft, s.choiceBtnSliceRight,
                    s.choiceBtnSliceTop, s.choiceBtnSliceBottom,
                    alias, "choice");
                pinOne(s.choiceBtnHoverSpritePath,
                    s.choiceBtnSliceLeft, s.choiceBtnSliceRight,
                    s.choiceBtnSliceTop, s.choiceBtnSliceBottom,
                    alias, "choice_hover");
                // Cara PNG de la ranura ocupada (idle + hover comparten 9-slice,
                // como choice). Solo ocupada; la vacia es color puro.
                pinOne(s.slotCardSpritePath,
                    s.slotCardSliceLeft, s.slotCardSliceRight,
                    s.slotCardSliceTop, s.slotCardSliceBottom,
                    alias, "ranura");
                pinOne(s.slotCardHoverSpritePath,
                    s.slotCardSliceLeft, s.slotCardSliceRight,
                    s.slotCardSliceTop, s.slotCardSliceBottom,
                    alias, "ranura_hover");
                pinSimple(s.advanceIndicatorImagePath);
                // Marco del aviso (entrada): PNG opcional detrás del cluster.
                pinOne(s.inputFrameSpritePath,
                    s.inputFrameSliceLeft, s.inputFrameSliceRight,
                    s.inputFrameSliceTop, s.inputFrameSliceBottom,
                    alias, "entrada_marco");
                // Cara PNG del OK del aviso (idle + hover comparten 9-slice,
                // como choice). Ficha viva del namebox, foldeada en este pase.
                pinOne(s.inputOkSpritePath,
                    s.inputOkSliceLeft, s.inputOkSliceRight,
                    s.inputOkSliceTop, s.inputOkSliceBottom,
                    alias, "entrada_ok");
                pinOne(s.inputOkHoverSpritePath,
                    s.inputOkSliceLeft, s.inputOkSliceRight,
                    s.inputOkSliceTop, s.inputOkSliceBottom,
                    alias, "entrada_ok_hover");
                };

            pinBox(theme.defaultDialogBox, "default");
            for (auto& [name, box] : theme.namedDialogBoxes) {
                pinBox(box, name);
            }
        }

        void unpinThemeSprites(const Theme& theme)
        {
            auto unpinOne = [&](const std::string& path) {
                if (!path.empty()) m_assets.unpin(path);
                };

            unpinOne(theme.defaultDialogBox.boxSpritePath);
            unpinOne(theme.defaultDialogBox.namePlateSpritePath);
            unpinOne(theme.defaultDialogBox.choiceBtnSpritePath);
            unpinOne(theme.defaultDialogBox.choiceBtnHoverSpritePath);
            unpinOne(theme.defaultDialogBox.slotCardSpritePath);
            unpinOne(theme.defaultDialogBox.slotCardHoverSpritePath);
            unpinOne(theme.defaultDialogBox.chromeBtnSpritePath);
            unpinOne(theme.defaultDialogBox.chromeBtnHoverSpritePath);
            unpinOne(theme.defaultDialogBox.advanceIndicatorImagePath);
            unpinOne(theme.defaultDialogBox.inputFrameSpritePath);
            unpinOne(theme.defaultDialogBox.inputOkSpritePath);
            unpinOne(theme.defaultDialogBox.inputOkHoverSpritePath);
            for (const auto& [name, box] : theme.namedDialogBoxes) {
                unpinOne(box.boxSpritePath);
                unpinOne(box.namePlateSpritePath);
                unpinOne(box.choiceBtnSpritePath);
                unpinOne(box.choiceBtnHoverSpritePath);
                unpinOne(box.slotCardSpritePath);
                unpinOne(box.slotCardHoverSpritePath);
                unpinOne(box.chromeBtnSpritePath);
                unpinOne(box.chromeBtnHoverSpritePath);
                unpinOne(box.advanceIndicatorImagePath);
                unpinOne(box.inputFrameSpritePath);
                unpinOne(box.inputOkSpritePath);
                unpinOne(box.inputOkHoverSpritePath);
            }
        }

        // Asegura que ac.cachedTex tenga el composite actualizado.
        // Retorna true si hay textura válida para dibujar.
        bool ensureCachedComposite(VNActiveChar& ac)
        {
            if (!m_hasFBO) return false;
            if (!ac.compositeDirty && ac.cachedTex) return true;

            // Recopilar capas
            std::vector<Texture*> layers;
            if (ac.threeLayer)
            {
                Texture* l = m_assets.get(ac.leftPath);
                Texture* r = m_assets.get(ac.rightPath);
                Texture* f = m_assets.get(ac.facePath);
                if (l) layers.push_back(l);
                if (r) layers.push_back(r);
                if (f) layers.push_back(f);
            }
            else
            {
                Texture* b = m_assets.get(ac.bodyPath);
                Texture* f = m_assets.get(ac.facePath);
                if (b) layers.push_back(b);
                if (f) layers.push_back(f);
            }
            if (layers.empty()) return false;

            int texW = layers[0]->getWidth();
            int texH = layers[0]->getHeight();

            // Asegurar textura cacheada
            if (!ac.cachedTex || ac.cachedTexW != texW || ac.cachedTexH != texH)
            {
                if (!ac.cachedTex) glGenTextures(1, &ac.cachedTex);
                glBindTexture(GL_TEXTURE_2D, ac.cachedTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindTexture(GL_TEXTURE_2D, 0);
                ac.cachedTexW = texW;
                ac.cachedTexH = texH;
            }

            if (!m_compFBO) pfn_glGenFramebuffers(1, &m_compFBO);

            // ── Guardar estado GL ──
            GLint prevViewport[4];
            glGetIntegerv(GL_VIEWPORT, prevViewport);
            GLint prevScissor[4];
            glGetIntegerv(GL_SCISSOR_BOX, prevScissor);
            GLboolean scissorOn = glIsEnabled(GL_SCISSOR_TEST);

            f32 prevProjL, prevProjR, prevProjB, prevProjT;
            draw2d::g_batch->getProjection(prevProjL, prevProjR, prevProjB, prevProjT);

            // ── Renderizar capas al FBO → textura del personaje ──
            draw2d::g_batch->flush();
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, m_compFBO);
            pfn_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, ac.cachedTex, 0);
            glViewport(0, 0, texW, texH);
            if (scissorOn) glDisable(GL_SCISSOR_TEST);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, texW, texH, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            draw2d::g_batch->setProjection(0.0f, (float)texW, (float)texH, 0.0f);

            for (Texture* tex : layers)
                draw2d::drawTexture_noBatch(*tex, 0, 0, (float)texW, (float)texH);

            draw2d::g_batch->flush();

            // ── Restaurar estado GL ──
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();

            pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(prevViewport[0], prevViewport[1],
                prevViewport[2], prevViewport[3]);
            if (scissorOn)
            {
                glEnable(GL_SCISSOR_TEST);
                glScissor(prevScissor[0], prevScissor[1],
                    prevScissor[2], prevScissor[3]);
            }
            glClearColor(0, 0, 0, 1);

            draw2d::g_batch->setProjection(prevProjL, prevProjR, prevProjB, prevProjT);

            ac.compositeDirty = false;
            return true;
        }

        void drawCompositeChar(VNActiveChar& ac, float VW, float VH,
            float tint = 1.0f, float spkScale = 1.0f,
            float overrideOffX = -9999.0f, float animOffY = 0.0f,
            float overrideAlpha = -1.0f)
        {
            float useAlpha = (overrideAlpha >= 0.0f) ? overrideAlpha : ac.fadeAlpha;
            float useOffX = (overrideOffX > -9998.0f) ? overrideOffX : ac.slideX;

            // Sin FBO → fallback directo
            if (!m_hasFBO)
            {
                if (ac.threeLayer)
                {
                    Texture* l = m_assets.get(ac.leftPath);
                    Texture* r = m_assets.get(ac.rightPath);
                    Texture* f = m_assets.get(ac.facePath);
                    if (l) drawSingleSprite(*l, ac.position, VW, VH, useAlpha, useOffX, tint, spkScale, animOffY);
                    if (r) drawSingleSprite(*r, ac.position, VW, VH, useAlpha, useOffX, tint, spkScale, animOffY);
                    if (f) drawSingleSprite(*f, ac.position, VW, VH, useAlpha, useOffX, tint, spkScale, animOffY);
                }
                else
                {
                    Texture* b = m_assets.get(ac.bodyPath);
                    Texture* f = m_assets.get(ac.facePath);
                    if (b) drawSingleSprite(*b, ac.position, VW, VH, useAlpha, useOffX, tint, spkScale, animOffY);
                    if (f) drawSingleSprite(*f, ac.position, VW, VH, useAlpha, useOffX, tint, spkScale, animOffY);
                }
                return;
            }

            // Asegurar caché válido
            if (!ensureCachedComposite(ac)) return;

            // ── Dibujar resultado cacheado ──
            float tw = (float)ac.cachedTexW;
            float th = (float)ac.cachedTexH;
            float maxH = VH * 0.85f;
            if (th > maxH)
            {
                float sc = maxH / th;
                tw = std::floor(tw * sc);
                th = std::floor(th * sc);
            }

            if (spkScale != 1.0f)
            {
                tw *= spkScale;
                th *= spkScale;
            }

            float cx = EclipParser::positionToX(ac.position, VW);
            float x = std::floor(cx - tw * 0.5f + useOffX);
            float y = std::floor(VH - th + animOffY);

            draw2d::drawTextureTinted_noBatch(ac.cachedTex, x, y, tw, th,
                tint, tint, tint, useAlpha);
        }

        void drawSingleSprite(const Texture& tex, VNPosition pos, float VW, float VH,
            float alpha = 1.0f, float offsetX = 0.0f,
            float tint = 1.0f, float spkScale = 1.0f,
            float animOffY = 0.0f)
        {
            float maxH = VH * 0.85f;
            float scale = 1.0f;
            float tw = (float)tex.getWidth();
            float th = (float)tex.getHeight();

            if (th > maxH)
            {
                scale = maxH / th;
                tw *= scale;
                th *= scale;
            }

            // Speaker zoom: escalar desde bottom-center
            if (spkScale != 1.0f)
            {
                tw *= spkScale;
                th *= spkScale;
            }

            float cx = EclipParser::positionToX(pos, VW);
            float x = std::floor(cx - tw * 0.5f + offsetX);
            float y = std::floor(VH - th + animOffY);  // Anclado al fondo + animacion

            // Render Core Fase 1, paso 4.B: migrado a SpriteBatch via drawTextureTinted.
            // El bloque glBegin/glEnd con glColor4f(tint, tint, tint, alpha) fue
            // reemplazado por una llamada al wrapper de SpriteBatch. La lógica de
            // scaling (maxH, spkScale) y posicionamiento (cx, x, y) se preserva
            // intacta. Usa el overload con Texture& (consistente con resto de draw2d::*).
            draw2d::drawTextureTinted_noBatch(tex, x, y, tw, th,
                tint, tint, tint, alpha);
        }


        // ── Runner de PASADAS (E-CP0) ───────────────────────────────────
        // Traduce una ListaPasadas a quads. Es el ÚNICO sitio del motor que
        // sabe de OpenGL en toda esta historia: EffectSystem.h no lo toca, y
        // por eso el parser puede incluirlo para validar en parse-time.
        //
        // Hereda el patrón de renderGlitchChar (immediate mode entre un
        // g_batch->end() y un g_batch->begin()); no inventa superficie GL nueva.
        //
        // Con una lista de [base] sola, esto dibuja EXACTAMENTE el mismo quad
        // que drawSingleSprite. Un efecto solo mueve lo que toca; lo demás lo
        // hereda en identidad.
        // `tecleando` se fue del ctx (§7.118): se llenaba desde isWaiting() y
        // NINGUNA primitiva v1 lo leía. Vuelve en E-CP6, con El Testigo, que sí
        // lo pide — junto a `regiones` y el mouse (ctx v2).
        void renderFxChar(GLuint texID, float sprW, float sprH,
            VNPosition pos, float VW, float VH,
            float alpha, float offsetX, float tint, float spkScale,
            float animOffY, const std::vector<EffectInstance>& fxs)
        {
            // Geometría natural — idéntica a renderGlitchChar / drawSingleSprite.
            float maxH = VH * 0.85f;
            float sc = 1.0f;
            float tw = sprW, th = sprH;
            if (th > maxH) { sc = maxH / th; tw *= sc; th *= sc; }
            if (spkScale != 1.0f) { tw *= spkScale; th *= spkScale; }

            float cx = EclipParser::positionToX(pos, VW);
            float baseX = std::floor(cx - tw * 0.5f + offsetX);
            float baseY = std::floor(VH - th + animOffY);

            renderFxQuad(texID, baseX, baseY, tw, th,
                tint, tint, tint, alpha, fxs);
        }

        // ── E-CP6: el transformador LUA — mismo contrato que una primitiva ──
        // fn(ctx, params, pasadas) mientras el driver diga activo. El puente
        // reusa tablas entre frames (disciplina GC, contrato §5); el error
        // degrada con aviso + el efecto se DES-REGISTRA y el update poda sus
        // instancias con la ley de miss — jamás crash, jamás silencio.
        //
        // Vive AQUÍ y no en applyFx porque EffectSystem.h es un header puro
        // que el parser incluye: sol2 no puede entrar ahí. El runner es el
        // único que conoce a los dos mundos.
        void applyLuaFx(const EffectInstance& fx, const FxCtx& ctx,
            ListaPasadas& lista)
        {
            auto itFn = m_luaFx.find(fx.luaName);
            if (itFn == m_luaFx.end()) return;   // roto/borrado: el update poda

            sol::state& lua = m_lua.state();
            if (!m_luaFxTablasListas)
            {
                m_luaFxCtx = lua.create_table();
                m_luaFxLista = lua.create_table();
                m_luaFxTablasListas = true;
            }

            // ctx v1 — lo dictó El Testigo, ni un campo más (§7.6). Claves en
            // INGLÉS: la convención viva de los bindings (enmienda §3.1(d)).
            // `typing` = "el typewriter está revelando" (isWaiting(), §1.59).
            // Sin `regiones` (mesa E-CP6 B1: la transparencia del PNG ya
            // recorta), sin dt (cero lectores), sin mouse (La Grieta, ctx v2).
            m_luaFxCtx["t"] = ctx.t;
            m_luaFxCtx["width"] = ctx.ancho;
            m_luaFxCtx["height"] = ctx.alto;
            m_luaFxCtx["intensity"] = ctx.intensidad;
            m_luaFxCtx["typing"] = m_dialogBox.isWaiting();

            // params: tabla chica por llamada (strings del k=v, tal cual el
            // .ecl los canonizó). La disciplina GC protege la LISTA, que es
            // la que crece; esto son 3-6 claves.
            sol::table params = lua.create_table();
            for (const auto& e : fx.params.raw)
                params[e.first] = e.second;

            // Pasadas → Lua. El pool reusa tablas; TODAS las claves se
            // escriben SIEMPRE (una tabla reusada con claves rancias de otro
            // frame mentiría).
            const size_t nIn = lista.size();
            while (m_luaFxPool.size() < nIn)
                m_luaFxPool.push_back(lua.create_table());
            for (size_t i = 0; i < nIn; ++i)
            {
                sol::table& t = m_luaFxPool[i];
                const Pasada& p = lista[i];
                t["u0"] = p.u0; t["v0"] = p.v0;
                t["u1"] = p.u1; t["v1"] = p.v1;
                t["dx"] = p.dx; t["dy"] = p.dy;
                t["sx"] = p.sx; t["sy"] = p.sy;
                t["r"] = p.r; t["g"] = p.g; t["b"] = p.b;
                t["alpha"] = p.alpha;
                t["texture"] = p.textura;          // "" = el elemento
                t["blend"] = (p.blend == Blend::Aditivo) ? "additive"
                    : (p.blend == Blend::Multiplicar) ? "multiply" : "alpha";
                t["dest"] = p.hasDest;
                t["dest_x"] = p.destX; t["dest_y"] = p.destY;
                t["dest_w"] = p.destW; t["dest_h"] = p.destH;
                // Recorte a la silueta del elemento. Viaja en las DOS
                // direcciones o un efecto Lua que solo lea la lista y la
                // devuelva BORRARÍA la máscara en silencio — y el cuadrado
                // volvería por la puerta de atrás.
                t["mask"] = p.mascara;
                m_luaFxLista[i + 1] = t;
            }
            // Recortar la cola si la lista del frame previo era más larga
            // (la lista Lua es una SECUENCIA: sin huecos). OJO: una clave
            // ausente reporta type::none, NO lua_nil — romper con cualquiera
            // de los dos o este loop no termina jamás (mordió en CP-A).
            for (size_t i = nIn + 1; ; ++i)
            {
                const sol::object oTrim = m_luaFxLista[i];
                const sol::type ty = oTrim.get_type();
                if (ty == sol::type::lua_nil || ty == sol::type::none) break;
                m_luaFxLista[i] = sol::lua_nil;
            }

            auto res = itFn->second(m_luaFxCtx, params, m_luaFxLista);
            if (!res.valid())
            {
                sol::error err = res;
                std::cerr << "[VN] AVISO: efecto lua '" << fx.luaName
                    << "' fallo: " << err.what() << " — se remueve (jamas "
                    << "crash, jamas silencio)." << std::endl;
                m_luaFx.erase(fx.luaName);
                return;   // la lista queda como estaba; el update poda
            }

            // Lua → pasadas: la lista ES lo que el efecto dejó (transformador
            // total: muta, agrega o quita). Clave ausente = identidad de la
            // Pasada — un efecto solo toca lo que necesita.
            lista.clear();
            for (size_t i = 1; ; ++i)
            {
                sol::optional<sol::table> ot = m_luaFxLista[i];
                if (!ot) break;
                sol::table t = *ot;
                Pasada p;
                p.u0 = t.get_or("u0", 0.0f);  p.v0 = t.get_or("v0", 0.0f);
                p.u1 = t.get_or("u1", 1.0f);  p.v1 = t.get_or("v1", 1.0f);
                p.dx = t.get_or("dx", 0.0f);  p.dy = t.get_or("dy", 0.0f);
                p.sx = t.get_or("sx", 1.0f);  p.sy = t.get_or("sy", 1.0f);
                p.r = t.get_or("r", 1.0f);    p.g = t.get_or("g", 1.0f);
                p.b = t.get_or("b", 1.0f);
                p.alpha = t.get_or("alpha", 1.0f);
                p.textura = t.get_or("texture", std::string());
                const std::string bl =
                    t.get_or("blend", std::string("alpha"));
                p.blend = (bl == "additive") ? Blend::Aditivo
                    : (bl == "multiply") ? Blend::Multiplicar : Blend::Alpha;
                p.hasDest = t.get_or("dest", false);
                p.destX = t.get_or("dest_x", 0.0f);
                p.destY = t.get_or("dest_y", 0.0f);
                p.destW = t.get_or("dest_w", 0.0f);
                p.destH = t.get_or("dest_h", 0.0f);
                p.mascara = t.get_or("mask", false);
                lista.push_back(p);
            }
        }

        // ── La MÁSCARA de silueta: unidad 1 = el elemento, alpha MODULADO ────
        // Arma (o desarma) la unidad de textura 1 para que las pasadas del
        // grupo salgan recortadas al alpha del elemento:
        //   RGB   = REPLACE de PREVIOUS  → lo que dejó la unidad 0 pasa intacto
        //                                   (la textura de la pasada × glColor)
        //   ALPHA = PREVIOUS × TEXTURE   → el recorte, y NADA más
        //
        // Se llama SIEMPRE fuera de un glBegin (cambiar de unidad activa o de
        // TexEnv adentro es error GL) y deja la unidad activa de vuelta en 0 —
        // el bind de la unidad 0 que viene después cuenta con eso.
        //
        // No toca parámetros de la textura del elemento a propósito: el uv de
        // la máscara vive en [0,1] por construcción, así que el wrap da igual,
        // y ese objeto lo comparte el SpriteBatch (§7.297: dejar el GL como
        // uno lo encontró no es cortesía, es lo que evita el bug silencioso).
        void setFxMascara(bool on, GLuint elemTex)
        {
            if (!m_hasMultiTex) return;

            pfn_glActiveTextureFx(GL_TEXTURE1);
            if (on)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, elemTex);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glDisable(GL_TEXTURE_2D);
            }
            pfn_glActiveTextureFx(GL_TEXTURE0);
        }

        // ── E-CP2: el corazón del runner, agnóstico de geometría ─────────────
        // (tubería + pintado por grupos de blend). renderFxChar calcula la
        // geometría de personaje y delega; `fondo` (0,0,VW,VH) y las capas
        // (px,py,w,h con parallax) traen su propio quad — y su tinte estático
        // entra como rgb de la pasada [base] (con personajes r=g=b=tint, la
        // misma base de siempre, byte-idéntica).
        void renderFxQuad(GLuint texID, float baseX, float baseY,
            float tw, float th,
            float tintR, float tintG, float tintB, float alpha,
            const std::vector<EffectInstance>& fxs)
        {
            // ── LA TUBERÍA. La lista arranca en [base] y cada efecto la
            // transforma. El ctx se materializa AQUÍ (§1.40): es el primer punto
            // donde el tamaño PROYECTADO (tw/th) ya existe — y `tajadas` y
            // `bloques` lo van a necesitar para cortar en fracciones reales.
            Pasada base;
            base.r = tintR; base.g = tintG; base.b = tintB;
            base.alpha = alpha;
            ListaPasadas lista{ base };
            FxCtx ctx;
            ctx.ancho = tw;
            ctx.alto = th;
            for (const auto& fx : fxs)
            {
                // E-CP4: un esporadico DORMIDO conserva su lugar en la tubería
                // (in-place, E-CP1) pero no transforma nada.
                if (!fx.ventanaActiva()) continue;
                ctx.t = fx.elapsed;            // cada efecto corre SU reloj
                ctx.intensidad = fxEffectiveIntensity(fx);  // CP2: × envolvente
                // E-CP6: el generador Lua no vive en el header puro (sol2 no
                // entra al parser) — el runner lo despacha aparte.
                if (fx.kind == FxKind::Lua) applyLuaFx(fx, ctx, lista);
                else applyFx(fx.kind, ctx, fx.params, lista);
            }

            // E-CP3: resolver las texturas ANTES de pintar — la de ruido se
            // crea lazy, y crear/bindear dentro de un glBegin es un error GL.
            std::vector<GLuint> texRes(lista.size());
            for (size_t i = 0; i < lista.size(); i++)
                texRes[i] = resolveFxTexture(lista[i].textura, texID);

            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);

            // Un glBegin por GRUPO contiguo — y desde E-CP3 el grupo es
            // (blend, TEXTURA): `estatica` y `sustituto` traen texturas
            // propias, y cambiar de textura cierra el glBegin igual que
            // cambiar de blend. glColor sí puede ir adentro; estos dos no.
            Blend cur = Blend::Alpha;
            GLuint curTex = 0;
            bool open = false;
            // ¿La unidad 1 está armada? Entra al GRUPO: cambiar de máscara
            // cierra el glBegin igual que cambiar de blend o de textura. No
            // fragmenta nada en la práctica — la pasada de ruido ya abría su
            // propio grupo por traer textura propia.
            bool curMask = false;

            for (size_t i = 0; i < lista.size(); i++)
            {
                const Pasada& p = lista[i];
                const GLuint pTex = texRes[i];
                // Un elemento sin textura (texID 0) no tiene silueta que dar.
                const bool pMask = p.mascara && m_hasMultiTex && texID != 0;

                if (!open || p.blend != cur || pTex != curTex || pMask != curMask)
                {
                    if (open) glEnd();
                    // Solo al CAMBIAR: sin ninguna pasada enmascarada esto no
                    // toca el GL ni una vez — la unidad 1 se queda como estaba
                    // (apagada) y el camino de siempre sale byte-idéntico.
                    if (pMask != curMask)
                    {
                        setFxMascara(pMask, texID);   // vuelve a la unidad 0
                        curMask = pMask;
                    }
                    if (!open || pTex != curTex)
                    {
                        glBindTexture(GL_TEXTURE_2D, pTex);
                        if (!p.textura.empty())
                        {
                            // Solo el RUIDO se muestrea con WRAP (el uv de
                            // `estatica` se pasa de [0,1] a propósito: tile).
                            // E-CP3 asumía "con uv [0,1] REPEAT rinde igual
                            // que CLAMP" — FALSO con filtrado bilineal: el
                            // texel del borde inferior sangra al superior a
                            // través de la costura del wrap (una línea
                            // horizontal fantasma). El ruido lo escondía; la
                            // primera textura FIGURATIVA del runner (El
                            // Testigo, feel-test CP-B) lo delató.
                            const GLint wrapFx = (p.textura == kFxRuido)
                                ? GL_REPEAT : GL_CLAMP_TO_EDGE;
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapFx);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapFx);
                        }
                        curTex = pTex;
                    }
                    switch (p.blend)
                    {
                    case Blend::Aditivo:
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
                    case Blend::Multiplicar:
                        glBlendFunc(GL_DST_COLOR, GL_ZERO); break;
                    default:
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
                    }
                    glBegin(GL_QUADS);
                    cur = p.blend;
                    open = true;
                }

                glColor4f(p.r, p.g, p.b, p.alpha);

                // La pasada estampa SU porción de la textura en SU lugar natural
                // — salvo dest explícito (E-CP3: estatica/sustituto), que pisa
                // la proyección y deja el uv SOLO para muestrear.
                float qx, qy, qw, qh;
                if (p.hasDest)
                {
                    qx = baseX + p.destX + p.dx;
                    qy = baseY + p.destY + p.dy;
                    qw = p.destW;
                    qh = p.destH;
                }
                else
                {
                    qx = baseX + p.u0 * tw + p.dx;
                    qy = baseY + p.v0 * th + p.dy;
                    qw = (p.u1 - p.u0) * tw;
                    qh = (p.v1 - p.v0) * th;
                }

                if (p.sx != 1.0f || p.sy != 1.0f)   // escala alrededor del centro
                {
                    float ccx = qx + qw * 0.5f;
                    float ccy = qy + qh * 0.5f;
                    qw *= p.sx; qh *= p.sy;
                    qx = ccx - qw * 0.5f;
                    qy = ccy - qh * 0.5f;
                }

                float uvT = 1.0f - p.v0;   // V flipped, como en todo el motor
                float uvB = 1.0f - p.v1;

                // uv de la MÁSCARA: el rect de la pasada en espacio del
                // ELEMENTO, SIN dx/dy ni escala. Anclarla al quad FINAL
                // rompería el desgarro — `tajadas` mueve la rebanada de ruido
                // junto con la del cuerpo, y el recorte tiene que ser el de ESA
                // rebanada del cuerpo, que es la que la pasada base muestrea
                // (uv sin desplazar) y estampa desplazada.
                //   Sin dest, la proyección natural YA es el uv del elemento:
                //   la máscara y la pasada coinciden por identidad.
                float mu0 = p.u0, mu1 = p.u1, mvT = uvT, mvB = uvB;
                if (curMask && p.hasDest)
                {
                    mu0 = (tw != 0.0f) ? p.destX / tw : 0.0f;
                    mu1 = (tw != 0.0f) ? (p.destX + p.destW) / tw : 1.0f;
                    const float mv0 = (th != 0.0f) ? p.destY / th : 0.0f;
                    const float mv1 = (th != 0.0f) ? (p.destY + p.destH) / th : 1.0f;
                    mvT = 1.0f - mv0;
                    mvB = 1.0f - mv1;
                }

                auto vert = [&](float u, float v, float mu, float mv,
                    float x, float y)
                {
                    glTexCoord2f(u, v);
                    if (curMask) pfn_glMultiTexCoord2fFx(GL_TEXTURE1, mu, mv);
                    glVertex2f(x, y);
                };
                vert(p.u0, uvT, mu0, mvT, qx, qy);
                vert(p.u1, uvT, mu1, mvT, qx + qw, qy);
                vert(p.u1, uvB, mu1, mvB, qx + qw, qy + qh);
                vert(p.u0, uvB, mu0, mvB, qx, qy + qh);
            }
            if (open) glEnd();
            if (curMask) setFxMascara(false, 0);   // el GL, como lo encontramos

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_TEXTURE_2D);
        }

        // ── E-CP3: resolver la textura de una pasada ─────────────────────────
        // "" = el elemento · kFxRuido = la generada · ruta = AssetManager
        // (un miss ya se avisó al aplicar el efecto; acá cae al elemento).
        GLuint resolveFxTexture(const std::string& name, GLuint elemTex)
        {
            if (name.empty()) return elemTex;
            if (name == kFxRuido) return ensureFxRuidoTex();
            Texture* t = m_assets.get(name);
            return t ? t->getID() : elemTex;
        }

        // La textura de ruido de `estatica`: 256x256 gris, generada UNA vez,
        // sin asset (§3 del contrato — este header sí conoce GL; EffectSystem
        // no, por eso viaja el centinela). NEAREST = grano crocante; REPEAT
        // porque el uv se pasa de [0,1] a propósito. Muere en onExit.
        GLuint ensureFxRuidoTex()
        {
            if (m_fxRuidoTex) return m_fxRuidoTex;

            std::vector<unsigned char> px((size_t)256 * 256 * 4);
            std::mt19937 rng(0xEC115EEDu);   // semilla fija: cosmético estable
            for (size_t i = 0; i < (size_t)256 * 256; i++)
            {
                const unsigned char v = (unsigned char)(rng() & 0xFFu);
                px[i * 4 + 0] = v;
                px[i * 4 + 1] = v;
                px[i * 4 + 2] = v;
                px[i * 4 + 3] = 255;
            }

            glGenTextures(1, &m_fxRuidoTex);
            glBindTexture(GL_TEXTURE_2D, m_fxRuidoTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, px.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            return m_fxRuidoTex;
        }

        // ── `renderGlitchChar` MURIÓ ACÁ (E-CP1) ─────────────────────────────
        // Era un render bespoke con TODO horneado: rojo/azul, 5px, 11/17 Hz,
        // 10 rebanadas, `seed % 3`, umbrales 0.8 y 1.5. Dos juegos hechos con
        // Eclips tenían EL MISMO glitch — el §0 del contrato en una función.
        //
        // Ahora el preset es `fxGlitchPreset` (EffectSystem.h): la MISMA imagen,
        // hecha de tres primitivas que el creador puede pedir sueltas, teñir,
        // y componer. `harness_pasadas T15` prueba que es byte a byte la misma
        // (err = 0, no "dentro de tolerancia").
        //
        // Y ese harness es lo que SELLA al preset: un `monika;glitch;2.0` en
        // disco describe esta receta, no un estado. Quien toque una constante
        // pone T15 en rojo — y tiene que enfrentarse al save antes de seguir.

        // ── Calcular viewport virtual para hit-test del mouse ─────────────────

        void getVirtualViewport(int& vpX, int& vpY, int& vpW, int& vpH) const
        {
            float targetAspect = (float)viewport::width() / viewport::height();
            float winAspect = (float)m_winW / m_winH;

            vpX = 0; vpY = 0; vpW = m_winW; vpH = m_winH;

            if (winAspect > targetAspect)
            {
                vpW = (int)(m_winH * targetAspect);
                vpX = (m_winW - vpW) / 2;
            }
            else
            {
                vpH = (int)(m_winW / targetAspect);
                vpY = (m_winH - vpH) / 2;
            }
        }

        // ── Mouse → coordenadas virtuales ────────────────────────────────────

        void mouseToVirtual(float mx, float my, float& vx, float& vy) const
        {
            int vpX, vpY, vpW, vpH;
            getVirtualViewport(vpX, vpY, vpW, vpH);
            vx = (mx - vpX) * ((float)viewport::width() / vpW);
            vy = (my - vpY) * ((float)viewport::height() / vpH);
        }



        // ── Guardar / Cargar ──────────────────────────────────────────────────

        // ── captureThumbnail (6.1 · CP0): snapshot de la escena → PNG en disco.
        // A2: re-invoca el render() del propio juego dentro de un FBO off-screen,
        // así captura la escena LIMPIA aunque el menú de save la tape en pantalla
        // (el menú es otro state → no entra en este render; el bug muere por
        // construcción). Additivo: sin FBO, no-op. El flip lo endereza el writer
        // (stbi_flip_vertically_on_write) → PNG derecho como cualquier asset.
        void captureThumbnail(int slot)
        {
            if (!m_hasFBO || m_winW <= 0 || m_winH <= 0) return;
            const int W = m_winW, H = m_winH;   // FBO = ventana (render() letterboxea adentro)

            // Lazy-gen / resize del FBO + textura color (espejo de m_compFBO).
            if (!m_captureFBO) pfn_glGenFramebuffers(1, &m_captureFBO);
            if (!m_captureTex || m_captureTexW != W || m_captureTexH != H)
            {
                if (!m_captureTex) glGenTextures(1, &m_captureTex);
                glBindTexture(GL_TEXTURE_2D, m_captureTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindTexture(GL_TEXTURE_2D, 0);
                m_captureTexW = W; m_captureTexH = H;
            }

            GLint prevViewport[4];
            glGetIntegerv(GL_VIEWPORT, prevViewport);

            draw2d::g_batch->flush();
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
            pfn_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, m_captureTex, 0);

            // Limpiar todo el FBO (las barras de letterbox quedan negras).
            glViewport(0, 0, W, H);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            // Re-render del juego (escena + su propia caja de diálogo). render()
            // fija su viewport letterbox dentro del FBO; en escena congelada los
            // composites usan cache warm → sin bind anidado.
            render(0.0);
            draw2d::g_batch->flush();

            // Leer el FBO → buffer RGBA (bottom-up; el writer lo endereza).
            std::vector<unsigned char> pixels((size_t)W * H * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            // Volver a pantalla + restaurar viewport (no tocamos el FB default).
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(prevViewport[0], prevViewport[1],
                prevViewport[2], prevViewport[3]);

            // Ruta fija por slot (0-based, calza con las llaves slot_N_ del save).
            std::error_code ec;
            std::filesystem::create_directories("saves/thumbs", ec);
            std::string path = "saves/thumbs/slot_" + std::to_string(slot) + ".png";

            stbi_flip_vertically_on_write(1);   // glReadPixels da bottom-up
            int ok = stbi_write_png(path.c_str(), W, H, 4, pixels.data(), W * 4);
            if (!ok) std::cerr << "[thumb] no se pudo escribir " << path << "\n";
            else std::cout << "[thumb] slot " << (slot + 1) << " -> " << path
                << " (" << W << "x" << H << ")\n";
        }

        void saveToSlot(int slot, const std::string& saveFilePath = "")
        {
            // Serializar personajes activos (v2 — tag en campo 0, piso 2):
            //   N;alias;expr;pos · C;alias;body;face;pos · C3;alias;left;right;face;pos
            std::string charsStr;
            {
                std::vector<std::string> entries;
                for (const auto& ac : m_activeChars)
                {
                    // A1.7: un personaje en pleno ocultar no se serializa — al
                    // cargar resucitaría sólido (defaults fadeAlpha/Target = 1).
                    if (ac.fadeTarget <= 0.0f) continue;
                    if (ac.threeLayer)
                        entries.push_back(eclips::savecodec::joinFields({
                            "C3", ac.alias, ac.leftPart, ac.rightPart,
                            ac.facePart, positionToString(ac.position) }));
                    else if (ac.composite)
                        entries.push_back(eclips::savecodec::joinFields({
                            "C", ac.alias, ac.bodyPart, ac.facePart,
                            positionToString(ac.position) }));
                    else
                        entries.push_back(eclips::savecodec::joinFields({
                            "N", ac.alias, ac.expression,
                            positionToString(ac.position) }));
                }
                charsStr = eclips::savecodec::joinEntries(entries);
            }


            // Timestamp legible
            std::time_t now = std::time(nullptr);
            char tsBuf[32];
            std::strftime(tsBuf, sizeof(tsBuf), "%d/%m/%Y  %H:%M", std::localtime(&now));

            // Preview = primeros 50 GLIFOS del último diálogo/narración.
            // Codepoints, no bytes (§1.45): el substr crudo de antes partía una
            // `é` al medio y grababa el byte huérfano EN EL SAVE — el mojibake
            // sobrevivía al round-trip. Hogar del recorte: EclipsParse.h.
            // El rtrim va acá y no en el consumidor: el disco queda limpio y los
            // dos lectores (menú C++ y `ranura`) heredan la misma verdad (§1.40).
            std::string prev = rtrimSpaces(truncateCodepoints(m_lastDialogue, 50));

            // ── Serializar variables (v2 — key;val, piso 2) ─────────────────
            std::string varsStr;
            {
                std::vector<std::string> entries;
                for (const auto& [k, v] : m_variables)
                    entries.push_back(eclips::savecodec::joinFields({ k, v }));
                varsStr = eclips::savecodec::joinEntries(entries);
            }

            // ── Serializar capas (v2 — 9 campos + tinte, piso 2) ────────────
            //   name;path;z;x;y;w;h;parallax;visible;tinte[;color]
            //   El campo 10 es ADITIVO: el lector v2 exige >=9 e ignora los
            //   extras (tolerancia por construcción — precedente `events`).
            //
            // ── PARCHE-COLOR-PREVIEW (0.9.0) ────────────────────────────────
            // Campo 11, aditivo (mismo truco que el tinte del campo 10): el color
            // de las capas "#rrggbb", tal cual lo escribió el guion. Sólo lo
            // llevan las capas de color; una escena de imágenes sale byte a byte
            // igual que antes, y un 0.8.x ve la ruta vacía y salta la capa como
            // siempre. Sin esto, cargar una partida apagaba el fondo blanco de la
            // demo y los versos #1a1a1a se quedaban hablando solos en la oscuridad.
            //
            // Es un parche y lo sabemos. El save todavía NO guarda texto-capa,
            // consolas, fades ni hover: si cargas a mitad de un poema, los versos
            // de esa escena se quedan en el tintero hasta la siguiente.
            //
            // El arreglo de verdad ya tiene contrato: DISENO_SAVE_ESCENA_VN.md
            // (VN v5), una tabla junto a SceneElement que guarda TODO lo que la
            // capa sabe, con reglas para fades, consola y fuentes. Ese lote va a
            // seguir leyendo este campo 11 (los saves de la preview no se tiran),
            // y este comentario se jubila con honores.
            // ─────────────────────────────────────────────────────────────────
            std::string layersStr;
            {
                std::vector<std::string> entries;
                const auto& sorted = m_sceneLayers.getSortedLayers();
                for (const auto* elem : sorted)
                {
                    auto pathIt = m_layerPaths.find(elem->name);
                    std::string path = (pathIt != m_layerPaths.end())
                        ? pathIt->second : "";
                    // Una capa de color a mitad de un `desaparecer` se guarda
                    // como va a quedar: oculta. El save no lleva el alpha, y si
                    // volviera visible cargaría opaca, con su `capa_fade` ya
                    // atrás en el guion y nadie que la apague (el `blanco` de
                    // `despertar` tapando la demo el resto de la partida). Las
                    // imágenes guardan `visible` tal cual: su fade, y el de
                    // todo lo demás, lo asienta el contrato v5.
                    const bool desvaneciendo = !elem->colorHex.empty()
                        && elem->fadeActive && elem->fadeHideAtEnd;
                    std::vector<std::string> fields = {
                        elem->name, path,
                        std::to_string(elem->zOrder),
                        std::to_string(elem->x), std::to_string(elem->y),
                        std::to_string(elem->w), std::to_string(elem->h),
                        std::to_string(elem->parallax),
                        ((elem->visible && !desvaneciendo) ? "1" : "0"),
                        elem->tintHex };
                    if (!elem->colorHex.empty())
                        fields.push_back(elem->colorHex);
                    entries.push_back(eclips::savecodec::joinFields(fields));
                }
                layersStr = eclips::savecodec::joinEntries(entries);
            }

            // ── Serializar hotspots (v2 — name;x;y;w;h, piso 2) ─────────────
            std::string hotspotsStr;
            {
                std::vector<std::string> entries;
                const auto& all = m_hotspots.getAll();
                for (const auto& hs : all)
                    entries.push_back(eclips::savecodec::joinFields({
                        hs.name, std::to_string(hs.x), std::to_string(hs.y),
                        std::to_string(hs.w), std::to_string(hs.h) }));
                hotspotsStr = eclips::savecodec::joinEntries(entries);
            }

            // ── Serializar random events ────────────────────────────────────
            // v2 — name;target;chance;cooldown;cada;fase;gateVar;gateOp;gateVal
            //
            // Los campos 7/8/9 son el GATE `[si:]` (pierna C), y entran por el
            // MISMO argumento que dejó escrito la pierna A para el 5 y el 6 —
            // no por analogía: se volvió a verificar en los dos lectores.
            // Un evento sin gate emite tres campos vacíos; `joinFields` escapa
            // `;` y `|`, así que un valor con separadores adentro tampoco
            // rompe la gramática.
            //
            // POR QUÉ TIENEN QUE VIAJAR, y no es simetría: un `evento_random`
            // se reconstruye AL CARGAR desde este string, porque la línea del
            // `.ecl` que lo definió ya pasó y no vuelve a ejecutarse. Un gate
            // que no viajara se evaporaría en el primer load — y el evento
            // quedaría disparando justo cuando el creador dijo que no.
            //
            // CERO BUMP DE DISCO, verificado en los DOS lectores: el v2 hace
            // `if (f.size() < 4) continue;` y lee f[0..3], así que los campos
            // extra se ignoran por construcción; el LECTOR CONGELADO (`ver < 2`)
            // hace cuatro `getline` y para, así que tampoco los mira. **El
            // lector congelado NO SE TOCA** — se dice en voz alta, regla 3.
            //
            // LEY 7 — el RELOJ se guarda, el SORTEO no:
            //   · `cada` y la FASE del beat viajan (campos 5 y 6). Si la fase se
            //     lavara, al cargar todos los eventos re-fasearían al mismo
            //     instante y se SINCRONIZARÍAN, deshaciendo la ley 10.
            //   · el COOLDOWN sigue lavándose — es promesa shipeada y
            //     documentada del revival ("puede disparar antes de lo esperado
            //     una vez tras cargar").
            //   · la SEMILLA no está acá y no puede estarlo: no vive en el
            //     struct. Load = barajado fresco.
            // Las ENTRADAS se emiten en orden de definición, no de mapa: el
            // orden del bloque es el único canal por el que el orden de
            // disparo sobrevive al load SIN tocar la gramática (los dos
            // lectores ya procesan secuencial). Save viejo → alfabético,
            // que es lo que ese save siempre hizo.
            std::string eventsStr;
            {
                std::vector<const RandomEventDef*> porDefinicion;
                porDefinicion.reserve(m_randomEvents.size());
                for (const auto& [name, ev] : m_randomEvents)
                    porDefinicion.push_back(&ev);
                std::stable_sort(porDefinicion.begin(), porDefinicion.end(),
                    [](const RandomEventDef* a, const RandomEventDef* b)
                    { return a->order < b->order; });

                std::vector<std::string> entries;
                for (const RandomEventDef* ev : porDefinicion)
                    entries.push_back(eclips::savecodec::joinFields({
                        ev->name, ev->target,
                        std::to_string(ev->trial.chance),
                        std::to_string(ev->trial.cooldown),
                        std::to_string(ev->trial.period),
                        std::to_string(ev->trial.beatRemaining),
                        ev->gate.var, ev->gate.op, ev->gate.val }));
                eventsStr = eclips::savecodec::joinEntries(entries);
            }

            // ── Serializar click mappings (v2 — label;target, piso 2) ───────
            std::string clicksStr;
            {
                std::vector<std::string> entries;
                for (const auto& mapping : m_clickMappings)
                    entries.push_back(eclips::savecodec::joinFields({
                        mapping.label, mapping.target }));
                clicksStr = eclips::savecodec::joinEntries(entries);
            }

            // ── Serializar definiciones de personajes ────────────────────────
            // Formato: "alias=path|alias=path|..." (C: prefix para compuestos)
            // Esto garantiza que al cargar un save de un script encadenado,
            // los personajes del script anterior siguen disponibles.
            std::string charDefsStr;
            {
                std::vector<std::string> entries;
                for (const auto& [alias, path] : m_script.characters)
                {
                    bool isComp = m_script.compositeChars.count(alias) > 0;
                    entries.push_back(eclips::savecodec::joinFields({
                        alias, (isComp ? "C" : "N"), path }));
                }
                charDefsStr = eclips::savecodec::joinEntries(entries);
            }

            // ── Serializar estado Lua ────────────────────────────────────────
            std::string luaStateStr = callLuaOnSave();

            // ── Serializar animaciones (v4 — bloque TAGUEADO P;/T;/E;) ───────
            // La clave sigue siendo slot_N_anims; slot_N_ver=4 selecciona T4/E4.
            // P conserva su gramática v3. Grafía ESPAÑOLA (fallo F3), una
            // por valor (fxNumStr). El aviso una-vez "los efectos no sobreviven"
            // MURIÓ acá — este CP es su funeral (§5 del contrato del bump).
            //   P; — presets, pantalla incluida (H9), multi-entrada (C-CP2).
            //   E; — efectos: TODOS viajan, el glitch como su especie real
            //        (H-B5b; la línea `alias;glitch;I` queda del lado del
            //        lector v2 CONGELADO). CP-C: +campo 7 origen (aditivo).
            //   T; — tracks nombrados (CP-C de C-CP5): TODOS viajan — loop y
            //        vaiven son perennes, y el una_vez PERSISTE por diseño
            //        (su valor final es parte de la escena). Sin elapsed: la
            //        fase se repone por modo al cargar (contrato C §7).
            std::string animsStr;
            {
                std::vector<std::string> entries;

                auto emitirP = [&entries](const std::string& target,
                    const CharAnim& a)
                {
                    // Campos vacíos = default (bump §4): solo viaja lo no-trivial.
                    entries.push_back(eclips::savecodec::joinFields({
                        "P", target, animTypeToStringEs(a.type),
                        fxNumStr(a.intensity),
                        (a.duration > 0.0f) ? fxNumStr(a.duration) : "",
                        (a.period > 0.0f) ? fxNumStr(a.period) : "",
                        a.axisVertical ? "y" : "",
                        "" }));   // modo: sin override runtime (default del tipo)
                };

                for (const auto& [alias, stack] : m_charAnims)
                    for (const auto& anim : stack)
                        if (anim.isLoop() || anim.isPersistent())
                            emitirP(alias, anim);
                for (const auto& anim : m_screenAnims)   // H9: pantalla entra
                    if (anim.isLoop() || anim.isPersistent())
                        emitirP("pantalla", anim);

                // Reloj-diálogo CP1: T4 conserva clock + progreso discreto k.
                auto emitirT4 = [&entries](const std::string& target,
                    const TrackInstance& ti)
                {
                    std::vector<std::string> fields;
                    if (trackToT4Fields(target, ti, fields))
                        entries.push_back(eclips::savecodec::joinFields(fields));
                    else
                        std::cerr << "[VNScriptState] save: track de '" << target
                            << "' no serializable como T4. Se descarta con voz."
                            << std::endl;
                };
                for (const auto& [aliasT, ts] : m_charTracks)
                    for (const auto& ti : ts)
                        emitirT4(aliasT, ti);
                for (const auto& ti : m_screenTracks)
                    emitirT4("pantalla", ti);

                // E4 nace con siete placeholders env_* vacíos. CP2 conectará la
                // envolvente grupal sin otro bump de formato.
                // El ORDEN de las E; por blanco ES la tubería (§7.271): el mapa
                // itera blancos en orden estable y cada stack se emite EN SU
                // ORDEN — el lector reconstruye con emitirEfecto (append).
                for (const auto& [blanco, fxStack] : m_charFx)
                    for (const auto& fx : fxStack)
                        if (fx.kind != FxKind::None)
                        {
                            std::vector<std::string> fields;
                            if (fxToE4Fields(blanco, fx,
                                    fx.envelope ? &*fx.envelope : nullptr, fields))
                                entries.push_back(
                                    eclips::savecodec::joinFields(fields));
                            else
                                std::cerr << "[VNScriptState] save: efecto de '"
                                    << blanco << "' no serializable como E4. "
                                    "Se descarta con voz." << std::endl;
                        }

                animsStr = eclips::savecodec::joinEntries(entries);
            }

            // Sello de guion (H-B8): hash del .ecl tal como está HOY en disco —
            // la misma resolución de path que usará el load al re-parsear.
            const std::string guionHash =
                hashGuion(eclips::resolveScriptPath(m_scriptPath, m_manifest));

            // ── Escribir todo al slot ────────────────────────────────────────
            VNSaveMenu::writeSlot(slot,
                m_currentScene, m_cmdIndex,
                m_currentBgPath, m_currentMusicPath,
                charsStr, prev, tsBuf, varsStr,
                layersStr, hotspotsStr, eventsStr, clicksStr,
                static_cast<int>(m_phase),
                m_parallaxEnabled, m_eventsActive,
                m_scriptPath, charDefsStr,
                luaStateStr,
                m_speakerHighlight,
                animsStr,
                m_vignette.intensity(),
                m_dialogBox.isHidden(),
                m_dialogBox.boxOverride(),
                saveFilePath, static_cast<long long>(now),
                m_preEventScene, m_preEventCmd,
                guionHash, m_bgTintHex);

            std::cout << "[VNScriptState] Quick save -> slot " << slot + 1
                << " (capas:" << m_sceneLayers.layerCount()
                << " hotspots:" << m_hotspots.count()
                << " eventos:" << m_randomEvents.size()
                << ")" << std::endl;

            // 6.1 CP0: snapshot de la escena para el thumbnail del slot. Additivo,
            // al final: si falla, el save de texto ya está en disco.
            captureThumbnail(slot);
        }

        // ── Sello de guion (H-B8): FNV-1a 32 sobre los bytes crudos del .ecl.
        // No es cripto — es identidad: ¿el guion de HOY es el del save? Archivo
        // ilegible → "" (= sin sello; nunca se finge saber).
        static std::string hashGuion(const std::string& path)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f) return "";
            uint32_t h = 2166136261u;
            char c;
            while (f.get(c))
                h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%08x", h);
            return buf;
        }

        void loadFromSlot(int slot)
        {
            auto d = VNSaveMenu::loadSlotData(slot);

            // Tier 4C.6: el slot puede tener solo VN, solo gameplay, o ambos.
            // Solo es "vacío" si no hay ni escena VN ni mapa de gameplay.
            if (d.scene.empty() && !d.gpHasGameplay)
            {
                std::cout << "[VNScriptState] Slot " << slot + 1
                    << " vacio, no se puede cargar." << std::endl;
                return;
            }

            // §4.3: un slot VN con gramática del futuro (slot_N_ver > 4) se
            // rechaza RUIDOSO por slot — jamás medio-parse. El menú lo marca
            // incompatible (readSlot, Etapa 3). Reloj-diálogo: techo v4.
            if (d.ver > VNSaveMenu::VN_FORMAT_VERSION)
            {
                std::cerr << "[VNScriptState] Slot " << slot + 1
                    << " incompatible: slot_N_ver=" << d.ver
                    << " (este engine lee hasta v"
                    << VNSaveMenu::VN_FORMAT_VERSION
                    << "). No se carga." << std::endl;
                return;
            }

            // Caso especial: slot con SOLO gameplay (sin VN previo).
            // Diferimos el push al siguiente tick para que el menú alcance
            // a pop-earse antes (si no, el menú queda encima del Gameplay2D).
            if (d.scene.empty() && d.gpHasGameplay)
            {
                std::cout << "[VNScriptState] Slot " << slot + 1
                    << " es gameplay puro — Gameplay2D se lanza en siguiente tick."
                    << std::endl;
                m_pendingGameplayData = d;
                m_pendingGameplayLaunch = true;
                return;
            }

            // ── Sello de guion (H-B8): ¿el guion de HOY es el del save? ──────
            // No arregla el ancla posicional — DEJA DE MENTIR: si el .ecl
            // cambió desde que se guardó, cmd=N puede apuntar a otra línea.
            // Sin sello (save viejo) o guion ilegible → silencio: un aviso que
            // suena siempre es un aviso muerto.
            if (!d.guionHash.empty())
            {
                const std::string pathGuion = d.scriptPath.empty()
                    ? m_scriptPath : d.scriptPath;
                const std::string hashHoy =
                    hashGuion(eclips::resolveScriptPath(pathGuion, m_manifest));
                if (!hashHoy.empty() && hashHoy != d.guionHash)
                {
                    std::cerr << "[VNScriptState] AVISO: este save es de OTRA "
                        << "version del guion '" << pathGuion << "'. La posicion "
                        << "(cmd=" << d.cmdIndex << ") puede no ser la que era. "
                        << "Personajes y variables se restauran igual." << std::endl;
                }
            }

            // ── Si el save viene de otro script, re-parsear ─────────────
            if (!d.scriptPath.empty() && d.scriptPath != m_scriptPath)
            {
                // ── Fix Pasada 3: el path guardado en el save es típicamente
                // relativo (ej "test_vigneta.ecl"), pero EclipParser::parse
                // necesita un path resoluble. Pasamos por resolveScriptPath
                // (consulta scriptDir del manifest + fallback vnscript/).
                std::string resolvedSavePath = eclips::resolveScriptPath(d.scriptPath, m_manifest);
                if (resolvedSavePath.empty())
                {
                    std::cerr << "[VNScriptState] ERROR: el save apunta al script '"
                        << d.scriptPath << "' que no existe en raiz, "
                        << "scriptDir, ni vnscript/. Save no se cargo completo."
                        << std::endl;
                    return;
                }

                std::cout << "[VNScriptState] Save usa script '"
                    << d.scriptPath << "' (actual: '"
                    << m_scriptPath << "'), re-parseando como '"
                    << resolvedSavePath << "'..."
                    << std::endl;

                EclipScriptData newScript = EclipParser::parse(resolvedSavePath, m_manifest);
                if (!newScript.valid)
                {
                    std::cerr << "[VNScriptState] ERROR: script '"
                        << resolvedSavePath << "' invalido: "
                        << newScript.error << std::endl;
                    return;
                }

                // Primero se hornea el paquete completo sin tocar el script,
                // la ruta ni el catálogo vivos. Si un suscriptor falla, el
                // slot se descarta y el estado anterior queda intacto.
                m_candidateLocales = std::move(newScript.fuentes);
                m_candidateScriptPath = resolvedSavePath;
                m_hasCandidateLocales = true;
                if (m_fontSources)
                {
                    fontRequestPublisher(
                        *m_fontSources, FontPublisherDoor::Restore,
                        m_generationSubscription);
                    if (!fontFinalizePublisher(*m_fontSources))
                    {
                        m_fontSources->rollbackGeneration();
                        return;
                    }
                }

                // El candidato ya está listo: recién ahora cambia lo vivo.
                // El commit prerender publica catálogo y bindings en el mismo
                // corte que la generación.
                m_script.mergeCharactersFrom(newScript);
                m_script.scenes = std::move(newScript.scenes);
                m_script.firstScene = newScript.firstScene;
                m_scriptPath = resolvedSavePath;
                if (!m_fontSources)
                {
                    m_script.fuentes = std::move(m_candidateLocales);
                    m_hasCandidateLocales = false;
                    m_candidateScriptPath.clear();
                }

                // E-CP6: los efectos Lua del script del save — cargar YA,
                // antes de que el bloque anims reviva un `E;` con generador
                // Lua (el registro es el que decide la ley de miss).
                mergeLuaEffectDecls(newScript.luaEffects);
                loadDeclaredLuaEffects(newScript.luaEffects);
            }

            // ── Limpiar estado anterior ─────────────────────────────────────
            // Unpinear texturas de capas
            for (const auto& [name, path] : m_layerPaths)
                m_assets.unpin(path);
            m_layerPaths.clear();
            m_layerPathOwners.clear();
            for (const auto& [name, path] : m_layerHoverPaths)
                 m_assets.unpin(path);
            m_layerHoverPaths.clear();
            m_layerHoverOwners.clear();
            m_sceneLayers.clear();
            m_hotspots.clear();
            m_controls.clear();
            m_teclaWidgets.clear();   // `CP9b`
            m_focoTecla = -1;
            cerrarCaptura();          // un cambio de escena no deja una captura viva
            m_slotWidgets.clear();
            m_slotThumbs.clear();   // 6.1 CP1: soltar las Texture de thumbs (GPU)
            m_slotThumbOwners.clear();
            m_hoveredSlot = -1;            // juice-hover: sin slot tras el reset
            // `CP6`: el anillo unificado se apaga junto con sus focables. Si el
            // foco sobreviviera al cambio de pantalla, la primera flecha
            // activaría un índice de la pantalla ANTERIOR.
            m_focoPlano = -1;
            m_focoControl = -1;
            m_hoveredControl = -1;
            // P2-3 (CONTROL-SKIN-BORDES-01): el CUARTO campo del anillo, que
            // este bloque olvidaba. Su gemelo retirarAmbito ya lo apagaba; este
            // y su copia en loadFromSlot no, y desde el lote del skin hay un
            // consumidor que lo INDEXA (controlPart) sobre un m_controls ya
            // vaciado. No es un campo mas: es el unico del anillo que quedaba
            // afuera, y el harness lo mide sitio por sitio.
            m_draggingControl = -1;
            m_focoDelPuntero = true;
            m_clickMappings.clear();
            m_scopeGenerations.clear();
            m_activeScopeOwner = 0;
            m_nextScopeGeneration = 1;
            m_randomEvents.clear();
            m_eventsActive = false;
            // Ley 7, la mitad que ninguno de los dos contratos tenía escrita:
            // el RELOJ se guarda (el beat vuelve donde estaba) y el SORTEO no.
            // *Load = barajado fresco*, que es lo observado en DDLC real.
            m_azarBolsas.clear();
            // `CP2a`: y por eso un pool guardado NO necesita código de save — el
            // load vuelve a `Executing` sobre el `m_cmdIndex` del bloque, así que
            // el bloque se re-ejecuta y re-sortea. Es `A10`, gratis.
            m_palabrasIds.clear();
            m_palabrasActivas = false;
            m_charAnims.clear();
            m_screenAnims.clear();
            m_charFx.clear();   // E-CP0 (el payload `E;` del save llega en E-CP4)
            m_charTracks.clear();    // C-CP5 (su `T;` llega en el CP-C de esta
            m_screenTracks.clear();  // misma superficie)
            m_waitTimer = 0.0f;
            m_waitingForAnim = false;
            m_cinematicaActive = false;  // CP1: cargar save, sin sello colgado
            resetSelloAcciones();        // `D-43` corte 3 + `D-51`: el sello NO
                                         // viaja al save, así que un save cargado
                                         // llega con la máscara virgen. Precedente
                                         // exacto: la línea de acá arriba.
            

            // ── Restaurar animaciones persistentes ────────────────────────
            if (!d.animsStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): alias;tipo;intensity|...
                    //    Solo el stof gana try/catch (regalo H10). Nada más se toca (E1).
                    std::istringstream anss(d.animsStr);
                std::string entry;
                while (std::getline(anss, entry, '|'))
                {
                    if (entry.empty()) continue;
                    size_t s1 = entry.find(';');
                    size_t s2 = entry.find(';', s1 + 1);
                    if (s1 == std::string::npos || s2 == std::string::npos) continue;

                    std::string alias = entry.substr(0, s1);
                    std::string tname = entry.substr(s1 + 1, s2 - s1 - 1);
                    float inten = 0.0f;
                    try { inten = std::stof(entry.substr(s2 + 1)); }
                    catch (...) { continue; }   // regalo H10 (§7.212): save corrupto no crashea

                    // ── E-CP1: congelar un lector congela su GRAMÁTICA, no le da
                    // permiso a que su SEMÁNTICA se pudra. `glitch` se mudó de mapa
                    // y el lector frozen sigue a la palabra: no gana una feature,
                    // se queda donde estaba. (Sin esto, parseAnimType daría None y
                    // el save v1 metería un CharAnim muerto tickeando para siempre.)
                    if (tname == "glitch")
                    {
                        EffectInstance gfx;
                        gfx.kind = FxKind::Glitch;
                        gfx.intensidad = inten;
                        gfx.duration = 0.0f;
                        gfx.elapsed = 0.0f;
                        emitirEfecto(m_charFx[alias], gfx);
                        continue;
                    }

                    AnimType at = parseAnimType(tname);
                    CharAnim a;
                    a.type = at;
                    a.intensity = inten;

                    if (at == AnimType::ZoomIn)
                    {
                        a.duration = animDefaultDuration(at);
                        a.elapsed = a.duration;
                        a.scale = 1.0f + 0.15f * inten;
                    }
                    else if (at == AnimType::Drop)
                    {
                        a.duration = animDefaultDuration(at);
                        a.elapsed = a.duration;
                        a.offsetY = 30.0f * inten;
                    }
                    else if (at == AnimType::Rise)
                    {
                        a.duration = animDefaultDuration(at);
                        a.elapsed = a.duration;
                        a.offsetY = -30.0f * inten;
                    }
                    else
                    {
                        a.elapsed = 0.0f;
                    }
                    // Cambio MECÁNICO forzado por el tipo, no una mejora: el
                    // lector legacy sigue congelado (§7.233). Un save v1 trae
                    // una entrada por alias → el stack queda de tamaño 1.
                    m_charAnims[alias].push_back(a);
                }
                }
                else if (d.ver == 2)
                {
                    // ── v2: alias;tipo;intensity|... (piso 2, escapado). ──
                    // ⚠ CONGELADO por el CP-BUMP (§7.233): desde slot_N_ver=3
                    // este lector es legacy. Su gramática no se toca.
                    for (const auto& entry : eclips::savecodec::splitEntries(d.animsStr))
                    {
                        if (entry.empty()) continue;
                        auto f = eclips::savecodec::splitFields(entry);
                        if (f.size() < 3) continue;
                        const std::string& alias = f[0];
                        const std::string& tname = f[1];
                        float inten = 0.0f;
                        try { inten = std::stof(f[2]); }
                        catch (...) { continue; }

                        // ── E-CP1: `glitch` cambió de casa, no de nombre. La misma
                        // cadena en disco; otro mapa en memoria. Sin esto, un save
                        // hecho ayer perdería su glitch en silencio.
                        if (tname == "glitch")
                        {
                            EffectInstance gfx;
                            gfx.kind = FxKind::Glitch;
                            gfx.intensidad = inten;
                            gfx.duration = 0.0f;   // solo el perenne se guardaba
                            gfx.elapsed = 0.0f;    // la fase se reinicia (como hoy)
                            emitirEfecto(m_charFx[alias], gfx);
                            continue;
                        }

                        AnimType at = parseAnimType(tname);
                        CharAnim a;
                        a.type = at;
                        a.intensity = inten;

                        if (at == AnimType::ZoomIn)
                        {
                            a.duration = animDefaultDuration(at);
                            a.elapsed = a.duration;
                            a.scale = 1.0f + 0.15f * inten;
                        }
                        else if (at == AnimType::Drop)
                        {
                            a.duration = animDefaultDuration(at);
                            a.elapsed = a.duration;
                            a.offsetY = 30.0f * inten;
                        }
                        else if (at == AnimType::Rise)
                        {
                            a.duration = animDefaultDuration(at);
                            a.elapsed = a.duration;
                            a.offsetY = -30.0f * inten;
                        }
                        else
                        {
                            a.elapsed = 0.0f;
                        }
                        // C-CP2 (F3): push_back, no `=`. Varias entradas del
                        // mismo alias reconstruyen el stack apilado.
                        m_charAnims[alias].push_back(a);
                    }
                }
                else
                {
                    // ── v3/v4: bloque TAGUEADO P;/T;/E;, español ─────────────
                    // Ley de miss: nombre/tag desconocido → aviso + descarte.
                    // Ley de tolerancia: campos extra se ignoran. Política de
                    // fase: por driver / por especie. La estampa decide codec;
                    // jamás se intenta leer un T4/E4 con el lector v3.

                    // CP-C: sal de esta carga para las semillas por GRUPO —
                    // las hermanas de una receta esporádica re-sortean IGUAL
                    // (blanco+origen) y siguen pulsando JUNTAS post-load
                    // (T23). Por-load y jamás del disco (mesa F1).
                    const uint32_t loadFxSalt =
                        fxMix32(0xE5E5E5E5u ^ (uint32_t)SDL_GetTicks());

                    // E-CP6: el registro VIVO — cargado al boot (y re-cargado
                    // arriba si el save re-parseó su script) — para que un
                    // `E;` con generador Lua reviva. El header puro no puede
                    // conocerlo; se lo pasamos (T26).
                    std::vector<std::string> luaRegistrados;
                    luaRegistrados.reserve(m_luaFx.size());
                    for (const auto& lf : m_luaFx)
                        luaRegistrados.push_back(lf.first);

                    for (const auto& entry : eclips::savecodec::splitEntries(d.animsStr))
                    {
                        if (entry.empty()) continue;
                        auto f = eclips::savecodec::splitFields(entry);
                        if (f.empty()) continue;
                        const std::string& tag = f[0];

                        if (tag == "P" && f.size() >= 3)
                        {
                            const std::string& target = f[1];
                            const AnimType at = parseAnimType(f[2]);
                            if (at == AnimType::None)
                            {
                                std::cerr << "[VNScriptState] load: preset '"
                                    << f[2] << "' desconocido. Se descarta "
                                    "(ley de miss)." << std::endl;
                                continue;
                            }

                            CharAnim a;
                            a.type = at;
                            try { a.intensity = (f.size() > 3 && !f[3].empty())
                                ? std::stof(f[3]) : 1.0f; }
                            catch (...) { a.intensity = 1.0f; }
                            try { a.duration = (f.size() > 4 && !f[4].empty())
                                ? std::stof(f[4]) : 0.0f; }
                            catch (...) { a.duration = 0.0f; }
                            try { a.period = (f.size() > 5 && !f[5].empty())
                                ? std::stof(f[5]) : 0.0f; }
                            catch (...) { a.period = 0.0f; }
                            a.axisVertical = (f.size() > 6 && f[6] == "y");
                            // f[7] (modo) sin override runtime; f[8]+ tolerados.

                            // Política de fase: persistentes restauran su
                            // estado FINAL; loops reinician (invisible).
                            if (a.isPersistent())
                            {
                                if (a.duration <= 0.0f)
                                    a.duration = animDefaultDuration(at);
                                a.elapsed = a.duration;
                                if (at == AnimType::ZoomIn)
                                    a.scale = 1.0f + 0.15f * a.intensity;
                                else if (at == AnimType::Drop)
                                    a.offsetY = 30.0f * a.intensity;
                                else if (at == AnimType::Rise)
                                    a.offsetY = -30.0f * a.intensity;
                            }
                            else
                                a.elapsed = 0.0f;

                            // H9: `pantalla` entra y se restaura.
                            if (target == "pantalla")
                                m_screenAnims.push_back(a);
                            else
                                m_charAnims[target].push_back(a);
                        }
                        else if (tag == "E")
                        {
                            std::string blanco;
                            EffectInstance fx;
                            bool reconstruible = false;
                            // Dialecto histórico: este literal no sigue al
                            // writer futuro. Un eventual v5 tendrá su rama.
                            if (d.ver == 4)
                            {
                                ScalarEnvelope envelope;
                                bool hasEnvelope = false;
                                reconstruible = fxFromE4Fields(f, blanco, fx,
                                    envelope, hasEnvelope, &luaRegistrados);
                                // CP2b: la envolvente grupal REVIVE sobre la
                                // instancia. Las hermanas del mismo (target,
                                // origen) se validan idénticas tras el loop.
                                if (reconstruible && hasEnvelope)
                                    fx.envelope = envelope;
                            }
                            else
                                reconstruible = fxFromE3Fields(f, blanco, fx,
                                    &luaRegistrados);

                            if (reconstruible)
                            {
                                if (fx.driver == FxDriver::Esporadico)
                                {
                                    // La semilla es por-corrida (jamás del
                                    // disco); elapsed+proxima YA vienen — el
                                    // reloj del terror no se lava.
                                    // CP-C: con origen, la semilla se DERIVA
                                    // de (blanco,origen)+sal — las hermanas
                                    // de la receta quedan EN FASE (T23); el
                                    // aplicar-directo sigue por-instancia.
                                    if (!fx.origen.empty())
                                    {
                                        uint32_t h = 2166136261u;   // FNV-1a
                                        for (char hc : blanco)
                                        { h ^= (unsigned char)hc; h *= 16777619u; }
                                        h ^= (unsigned char)';'; h *= 16777619u;
                                        for (char hc : fx.origen)
                                        { h ^= (unsigned char)hc; h *= 16777619u; }
                                        fx.rngState = fxMix32(h ^ loadFxSalt);
                                    }
                                    else
                                    {
                                        static uint32_t s_loadSeed = 0xBADC0FFEu;
                                        fx.rngState = fxMix32(++s_loadSeed
                                            * 2654435761u ^ (uint32_t)SDL_GetTicks());
                                    }
                                }
                                // Append en orden de archivo = orden de
                                // tubería (§7.271; el writer emitió en orden).
                                emitirEfecto(m_charFx[blanco], fx);
                            }
                            else if (f.size() >= 3)
                                std::cerr << "[VNScriptState] load: efecto '"
                                    << f[2] << "' no registrado (o driver "
                                    "desconocido). Se descarta (ley de miss)."
                                    << std::endl;
                        }
                        else if (tag == "T")
                        {
                            std::string targetT;
                            TrackInstance ti;
                            const bool reconstruible = (d.ver == 4)
                                ? trackFromT4Fields(f, targetT, ti)
                                : trackFromT3Fields(f, targetT, ti);
                            if (reconstruible &&
                                !(ti.track.clock == ClockDomain::DialogueAdvance &&
                                  (ti.track.canal != Channel::Scale ||
                                   targetT == "pantalla")))
                            {
                                if (targetT == "pantalla")
                                    m_screenTracks.push_back(ti);
                                else
                                    m_charTracks[targetT].push_back(ti);
                            }
                            else if (reconstruible)
                                std::cerr << "[VNScriptState] load: DialogueAdvance solo "
                                    "vale en escala de "
                                    "personaje. T4 se descarta entero."
                                    << std::endl;
                            else if (f.size() >= 3)
                                std::cerr << "[VNScriptState] load: track T; "
                                    "no reconstruible (canal/modo '" << f[2]
                                    << "/" << (f.size() > 8 ? f[8] : "?")
                                    << "'). Se descarta (ley de miss)."
                                    << std::endl;
                        }
                        else
                        {
                            std::cerr << "[VNScriptState] load: entrada '"
                                << tag << ";' desconocida en anims v" << d.ver
                                << ". Se "
                                "descarta (ley de miss)." << std::endl;
                        }
                    }
                }
            }

            // Reloj-diálogo CP2b §8: las E4 hermanas del mismo (target, origen)
            // repiten la MISMA envolvente. Una discrepancia (save editado a mano
            // o corrupto) invalida la del GRUPO — no gana primera ni última: se
            // limpia la envolvente de todas las de ese origen y los efectos
            // corren sin modulación, con voz.
            for (auto& [blancoFx, stackFx] : m_charFx)
            {
                for (std::size_t i = 0; i < stackFx.size(); ++i)
                {
                    const std::string& org = stackFx[i].origen;
                    if (org.empty()) continue;
                    bool discrepa = false;
                    for (std::size_t j = 0; j < stackFx.size() && !discrepa; ++j)
                    {
                        if (j == i || stackFx[j].origen != org) continue;
                        const bool ambasVacias =
                            !stackFx[i].envelope && !stackFx[j].envelope;
                        const bool ambasIguales =
                            stackFx[i].envelope && stackFx[j].envelope &&
                            envelopesEqual(*stackFx[i].envelope,
                                           *stackFx[j].envelope);
                        if (!ambasVacias && !ambasIguales) discrepa = true;
                    }
                    if (discrepa)
                    {
                        std::cerr << "[VNScriptState] load: grupo '" << blancoFx
                            << ":" << org << "' con envolventes de intensidad "
                            "discrepantes; se invalida la del grupo (efectos sin "
                            "modulacion)." << std::endl;
                        for (auto& fx : stackFx)
                            if (fx.origen == org) fx.envelope.reset();
                    }
                }
            }

            // Unpinear todo antes de restaurar
            m_assets.unpinAll();

            // ── Restaurar fondo ─────────────────────────────────────────────
            if (!d.bgPath.empty())
            {
                Texture* bg = m_assets.get(d.bgPath);
                if (bg) m_assets.pin(d.bgPath);
            }
            m_currentBgPath = d.bgPath; // vacío = sin fondo (limpia el anterior)
            setBgTint(d.bgTint);        // aditiva: save viejo → "" → sin tinte

            // Limpiar transición si había una en curso
            m_activeTransition = VNTransitionType::None;
            m_transOldBgPath.clear();

            // ── Restaurar música ────────────────────────────────────────────
            if (m_hasAudio)
            {
                if (!d.musicPath.empty() && d.musicPath != m_currentMusicPath)
                {
                    m_audio.stopMusic();
                    m_audio.playMusic(d.musicPath, true);
                }
                else if (d.musicPath.empty() && !m_currentMusicPath.empty())
                {
                    m_audio.stopMusic();
                }
            }
            m_currentMusicPath = d.musicPath;

            // ── Limpiar historial (pertenece a la sesión anterior) ──────────
            m_dialogBox.clearHistory();

            // ── Restaurar definiciones de personajes ─────────────────────
            // Esto es crítico para saves de scripts encadenados: si el save
            // fue en acto2.ecl pero Luna fue definida en acto1.ecl, las
            // definiciones guardadas aseguran que Luna sigue disponible.
            if (!d.charDefsStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): alias=[C:]path|... ──
                    std::istringstream cdss(d.charDefsStr);
                    std::string entry;
                    while (std::getline(cdss, entry, '|'))
                    {
                        if (entry.empty()) continue;
                        size_t eq = entry.find('=');
                        if (eq == std::string::npos) continue;

                        std::string alias = entry.substr(0, eq);
                        std::string path = entry.substr(eq + 1);

                        // Detectar compuesto (prefijo "C:")
                        if (path.size() >= 2 && path[0] == 'C' && path[1] == ':')
                        {
                            path = path.substr(2);
                            m_script.compositeChars.insert(alias);
                        }

                        m_script.characters[alias] = path;
                    }
                }
                else
                {
                    // ── v2: alias;N|C;path (tag en campo propio). A1.3 muerto. ──
                    for (const auto& e : eclips::savecodec::splitEntries(d.charDefsStr))
                    {
                        if (e.empty()) continue;
                        auto f = eclips::savecodec::splitFields(e);
                        if (f.size() < 3) continue;
                        const std::string& alias = f[0];
                        m_script.characters[alias] = f[2];
                        if (f[1] == "C") m_script.compositeChars.insert(alias);
                    }
                }
            }

            // ── Restaurar personajes ────────────────────────────────────────
            releaseAllCachedTex();
            m_activeChars.clear();
            if (!d.activeCharsStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): "alias:expr:pos" space-join
                    //    con sniff C3:/C:/normal. NO se toca (E1). Indent heredada.
                    std::istringstream iss(d.activeCharsStr);
                std::string token;
                while (iss >> token)
                {
                    // Formato: "alias:expr:pos" o "alias::pos" (sin expresión)
                    // Compatible con formato viejo "alias:pos"
                    size_t col1 = token.find(':');
                    if (col1 == std::string::npos) continue;

                    std::string alias = token.substr(0, col1);
                    std::string rest = token.substr(col1 + 1);

                    // Verificar si es 3 capas (formato: "alias:C3:left:right:face:pos")
                    if (rest.size() >= 3 && rest[0] == 'C' && rest[1] == '3' && rest[2] == ':')
                    {
                        rest = rest.substr(3); // quitar "C3:"
                        size_t s1 = rest.find(':');
                        size_t s2 = (s1 != std::string::npos) ? rest.find(':', s1 + 1) : std::string::npos;
                        size_t s3 = (s2 != std::string::npos) ? rest.find(':', s2 + 1) : std::string::npos;
                        if (s1 == std::string::npos || s2 == std::string::npos || s3 == std::string::npos) continue;

                        std::string leftPart = rest.substr(0, s1);
                        std::string rightPart = rest.substr(s1 + 1, s2 - s1 - 1);
                        std::string facePart = rest.substr(s2 + 1, s3 - s2 - 1);
                        std::string posStr = rest.substr(s3 + 1);

                        auto cit = m_script.characters.find(alias);
                        if (cit == m_script.characters.end()) continue;

                        std::string folder = cit->second;
                        std::string lPath = folder + leftPart + ".png";
                        std::string rPath = folder + rightPart + ".png";
                        std::string fPath = folder + facePart + ".png";

                        Texture* leftTex = m_assets.get(lPath);
                        Texture* rightTex = m_assets.get(rPath);
                        Texture* faceTex = m_assets.get(fPath);
                        if (leftTex && rightTex && faceTex)
                        {
                            m_assets.pin(lPath);
                            m_assets.pin(rPath);
                            m_assets.pin(fPath);
                            VNActiveChar ac;
                            ac.alias = alias;
                            ac.composite = true;
                            ac.threeLayer = true;
                            ac.leftPart = leftPart;
                            ac.leftPath = lPath;
                            ac.rightPart = rightPart;
                            ac.rightPath = rPath;
                            ac.facePart = facePart;
                            ac.facePath = fPath;
                            ac.spritePath = lPath;
                            ac.expression = facePart;
                            ac.position = EclipParser::parsePosition(posStr);
                            m_activeChars.push_back(ac);
                        }
                        else
                        {
                            std::cerr << "[VNScriptState] load: personaje '" << alias
                                << "' (C3) DESCARTADO — textura falta: "
                                << (!leftTex ? lPath : !rightTex ? rPath : fPath)
                                << std::endl;
                        }
                    }
                    // Verificar si es compuesto 2 capas (formato: "alias:C:body:face:pos")
                    else if (rest.size() >= 2 && rest[0] == 'C' && rest[1] == ':')
                    {
                        rest = rest.substr(2); // quitar "C:"
                        size_t s1 = rest.find(':');
                        size_t s2 = (s1 != std::string::npos) ? rest.find(':', s1 + 1) : std::string::npos;
                        if (s1 == std::string::npos || s2 == std::string::npos) continue;

                        std::string bodyPart = rest.substr(0, s1);
                        std::string facePart = rest.substr(s1 + 1, s2 - s1 - 1);
                        std::string posStr = rest.substr(s2 + 1);

                        auto cit = m_script.characters.find(alias);
                        if (cit == m_script.characters.end())
                        {
                            std::cerr << "[VNScriptState] load: personaje '" << alias
                                << "' DESCARTADO — alias no esta en characters "
                                "(¿manifest/script cambiado desde el save?)" << std::endl;
                            continue;
                        }

                        std::string folder = cit->second;
                        std::string bPath = folder + bodyPart + ".png";
                        std::string fPath = folder + facePart + ".png";

                        Texture* bodyTex = m_assets.get(bPath);
                        Texture* faceTex = m_assets.get(fPath);
                        if (bodyTex && faceTex)
                        {
                            m_assets.pin(bPath);
                            m_assets.pin(fPath);
                            VNActiveChar ac;
                            ac.alias = alias;
                            ac.composite = true;
                            ac.bodyPart = bodyPart;
                            ac.facePart = facePart;
                            ac.bodyPath = bPath;
                            ac.facePath = fPath;
                            ac.spritePath = bPath;
                            ac.expression = facePart;
                            ac.position = EclipParser::parsePosition(posStr);
                            m_activeChars.push_back(ac);
                        }
                    }
                    else
                    {
                        // Formato normal: "alias:expr:pos" o "alias:pos"
                        size_t col2 = rest.find(':');

                        std::string expr, posStr;
                        if (col2 != std::string::npos)
                        {
                            expr = rest.substr(0, col2);
                            posStr = rest.substr(col2 + 1);
                        }
                        else
                        {
                            posStr = rest;
                        }

                        auto cit = m_script.characters.find(alias);
                        if (cit == m_script.characters.end())
                        {
                            std::cerr << "[VNScriptState] load: personaje '" << alias
                                << "' DESCARTADO — alias no esta en characters "
                                "(¿manifest/script cambiado desde el save?)" << std::endl;
                            continue;
                        }

                        // Simetría con applyCompositeVisual sprite-completo (L3163):
                        // si la base es CARPETA (termina en separador), la expresión
                        // es un sprite completo: carpeta + expr + ".png".
                        // buildExprPath es solo para bases-archivo ("luna.png").
                        const bool baseEsCarpeta = !cit->second.empty() &&
                            (cit->second.back() == '/' || cit->second.back() == '\\');
                        std::string path = expr.empty()
                            ? cit->second
                            : (baseEsCarpeta ? cit->second + expr + ".png"
                                : buildExprPath(cit->second, expr));

                        Texture* tex = m_assets.get(path);
                        if (!tex && !expr.empty())
                        {
                            path = cit->second;
                            expr = "";
                            tex = m_assets.get(path);
                        }

                        if (tex)
                        {
                            m_assets.pin(path);
                            VNActiveChar ac;
                            ac.alias = alias;
                            ac.expression = expr;
                            ac.spritePath = path;
                            ac.position = EclipParser::parsePosition(posStr);
                            ac.composite = false;
                            m_activeChars.push_back(ac);
                        }
                        }
                }
              }
              else
              {
                  // ── v2: N;alias;expr;pos · C;alias;body;face;pos ·
                  //    C3;alias;left;right;face;pos (tag en campo 0). A1.4 muerto.
                  //    Cola de texturas propia (E1): el legacy no se respira.
                  for (const auto& entry : eclips::savecodec::splitEntries(d.activeCharsStr))
                  {
                      if (entry.empty()) continue;
                      auto f = eclips::savecodec::splitFields(entry);
                      if (f.empty()) continue;
                      const std::string& tag = f[0];

                      if (tag == "C3" && f.size() >= 6)
                      {
                          const std::string& alias = f[1];
                          const std::string& leftPart = f[2];
                          const std::string& rightPart = f[3];
                          const std::string& facePart = f[4];
                          const std::string& posStr = f[5];

                          auto cit = m_script.characters.find(alias);
                          if (cit == m_script.characters.end())
                          {
                              std::cerr << "[VNScriptState] load: personaje '" << alias
                                  << "' (C3) DESCARTADO — alias no esta en characters "
                                  "(¿manifest/script cambiado desde el save?)" << std::endl;
                              continue;
                          }

                          std::string folder = cit->second;
                          std::string lPath = folder + leftPart + ".png";
                          std::string rPath = folder + rightPart + ".png";
                          std::string fPath = folder + facePart + ".png";

                          Texture* leftTex = m_assets.get(lPath);
                          Texture* rightTex = m_assets.get(rPath);
                          Texture* faceTex = m_assets.get(fPath);
                          if (leftTex && rightTex && faceTex)
                          {
                              m_assets.pin(lPath);
                              m_assets.pin(rPath);
                              m_assets.pin(fPath);
                              VNActiveChar ac;
                              ac.alias = alias;
                              ac.composite = true;
                              ac.threeLayer = true;
                              ac.leftPart = leftPart;
                              ac.leftPath = lPath;
                              ac.rightPart = rightPart;
                              ac.rightPath = rPath;
                              ac.facePart = facePart;
                              ac.facePath = fPath;
                              ac.spritePath = lPath;
                              ac.expression = facePart;
                              ac.position = EclipParser::parsePosition(posStr);
                              m_activeChars.push_back(ac);
                          }
                          else
                          {
                              std::cerr << "[VNScriptState] load: personaje '" << alias
                                  << "' (C3) DESCARTADO — textura falta: "
                                  << (!leftTex ? lPath : !rightTex ? rPath : fPath)
                                  << std::endl;
                          }
                      }
                      else if (tag == "C" && f.size() >= 5)
                      {
                          const std::string& alias = f[1];
                          const std::string& bodyPart = f[2];
                          const std::string& facePart = f[3];
                          const std::string& posStr = f[4];

                          auto cit = m_script.characters.find(alias);
                          if (cit == m_script.characters.end())
                          {
                              std::cerr << "[VNScriptState] load: personaje '" << alias
                                  << "' DESCARTADO — alias no esta en characters "
                                  "(¿manifest/script cambiado desde el save?)" << std::endl;
                              continue;
                          }

                          std::string folder = cit->second;
                          std::string bPath = folder + bodyPart + ".png";
                          std::string fPath = folder + facePart + ".png";

                          Texture* bodyTex = m_assets.get(bPath);
                          Texture* faceTex = m_assets.get(fPath);
                          if (bodyTex && faceTex)
                          {
                              m_assets.pin(bPath);
                              m_assets.pin(fPath);
                              VNActiveChar ac;
                              ac.alias = alias;
                              ac.composite = true;
                              ac.bodyPart = bodyPart;
                              ac.facePart = facePart;
                              ac.bodyPath = bPath;
                              ac.facePath = fPath;
                              ac.spritePath = bPath;
                              ac.expression = facePart;
                              ac.position = EclipParser::parsePosition(posStr);
                              m_activeChars.push_back(ac);
                          }
                          else
                          {
                              std::cerr << "[VNScriptState] load: personaje '" << alias
                                  << "' DESCARTADO — textura falta: "
                                  << (!bodyTex ? bPath : fPath) << std::endl;
                          }
                      }
                      else if (tag == "N" && f.size() >= 4)
                      {
                          const std::string& alias = f[1];
                          std::string        expr = f[2];
                          const std::string& posStr = f[3];

                          auto cit = m_script.characters.find(alias);
                          if (cit == m_script.characters.end())
                          {
                              std::cerr << "[VNScriptState] load: personaje '" << alias
                                  << "' DESCARTADO — alias no esta en characters "
                                  "(¿manifest/script cambiado desde el save?)" << std::endl;
                              continue;
                          }

                          const bool baseEsCarpeta = !cit->second.empty() &&
                              (cit->second.back() == '/' || cit->second.back() == '\\');
                          std::string path = expr.empty()
                              ? cit->second
                              : (baseEsCarpeta ? cit->second + expr + ".png"
                                  : buildExprPath(cit->second, expr));

                          Texture* tex = m_assets.get(path);
                          if (!tex && !expr.empty())
                          {
                              path = cit->second;
                              expr = "";
                              tex = m_assets.get(path);
                          }

                          if (tex)
                          {
                              m_assets.pin(path);
                              VNActiveChar ac;
                              ac.alias = alias;
                              ac.expression = expr;
                              ac.spritePath = path;
                              ac.position = EclipParser::parsePosition(posStr);
                              ac.composite = false;
                              m_activeChars.push_back(ac);
                          }
                          else
                          {
                              std::cerr << "[VNScriptState] load: personaje '" << alias
                                  << "' DESCARTADO — textura falta: " << path
                                  << std::endl;
                          }
                      }
                      // tag desconocido / campos faltantes → se descarta (silencio duro)
                  }
                  }
            }

            // ── Restaurar variables ─────────────────────────────────────────
            m_variables.clear();
            m_warnedVars.clear();   // re-avisar interpolacion en la sesion cargada
            m_warnedCompare.clear();// idem para la voz de `si`/`mutar` (§7.493)
            if (!d.varsStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): key:val|... primer-':' ──
                    std::istringstream vss(d.varsStr);
                    std::string pair;
                    while (std::getline(vss, pair, '|'))
                    {
                        size_t col = pair.find(':');
                        if (col != std::string::npos)
                        {
                            std::string key = pair.substr(0, col);
                            std::string val = pair.substr(col + 1);
                            m_variables[key] = val;
                        }
                    }
                }
                else
                {
                    // ── v2: key;val|... (piso 2). A1.5 muerto; ':' queda literal. ──
                    for (const auto& e : eclips::savecodec::splitEntries(d.varsStr))
                    {
                        if (e.empty()) continue;
                        auto f = eclips::savecodec::splitFields(e);
                        if (f.empty()) continue;
                        m_variables[f[0]] = (f.size() >= 2) ? f[1] : "";
                    }
                }
            }

            // ═══════════════════════════════════════════════════════════════
            // BLOQUE 1-2: Restaurar capas, hotspots, eventos, etc.
            // (vacío en saves viejos → se salta, retrocompatible)
            // ═══════════════════════════════════════════════════════════════

            // ── Restaurar capas (z-layers) ──────────────────────────────────
            // Formato: "name;path;z;x;y;w;h;parallax;visible|..."
            if (!d.layersStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): 9 campos ';' sin unescape. ──
                    std::istringstream lss(d.layersStr);
                std::string layerEntry;
                while (std::getline(lss, layerEntry, '|'))
                {
                    if (layerEntry.empty()) continue;

                    // Parsear los 9 campos separados por ';'
                    std::istringstream fields(layerEntry);
                    std::string name, path, zStr, xStr, yStr, wStr, hStr, pxStr, visStr;
                    if (!std::getline(fields, name, ';')) continue;
                    if (!std::getline(fields, path, ';')) continue;
                    if (!std::getline(fields, zStr, ';')) continue;
                    if (!std::getline(fields, xStr, ';')) continue;
                    if (!std::getline(fields, yStr, ';')) continue;
                    if (!std::getline(fields, wStr, ';')) continue;
                    if (!std::getline(fields, hStr, ';')) continue;
                    if (!std::getline(fields, pxStr, ';')) continue;
                    if (!std::getline(fields, visStr, ';')) continue;

                    // Cargar la textura vía AssetManager
                    if (path.empty()) continue;
                    Texture* tex = m_assets.get(path);
                    if (!tex)
                    {
                        std::cerr << "[VNScriptState] load: capa '" << name
                            << "' textura no encontrada: " << path << std::endl;
                        continue;
                    }
                    m_assets.pin(path);
                    m_layerPaths[name] = path;
                    m_layerPathOwners[name] = 0;

                    // Convertir valores
                    int   z = 0;
                    float x = 0, y = 0, w = 0, h = 0, px = 0;
                    bool  vis = true;
                    try { z = std::stoi(zStr); }
                    catch (...) {}
                    try { x = std::stof(xStr); }
                    catch (...) {}
                    try { y = std::stof(yStr); }
                    catch (...) {}
                    try { w = std::stof(wStr); }
                    catch (...) {}
                    try { h = std::stof(hStr); }
                    catch (...) {}
                    try { px = std::stof(pxStr); }
                    catch (...) {}
                    vis = (visStr == "1");

                    // Agregar capa al manager
                    m_sceneLayers.addLayer(name, tex, z, x, y, w, h);
                    if (px != 0.0f)
                        m_sceneLayers.setParallax(name, px);
                    if (!vis)
                        m_sceneLayers.setVisible(name, false);
                }

                std::cout << "[VNScriptState] Restauradas "
                    << m_sceneLayers.layerCount() << " capas" << std::endl;
                }
                else
                {
                    // ── v2: name;path;z;x;y;w;h;parallax;visible|... (piso 2). ──
                    for (const auto& entry : eclips::savecodec::splitEntries(d.layersStr))
                    {
                        if (entry.empty()) continue;
                        auto f = eclips::savecodec::splitFields(entry);
                        if (f.size() < 9) continue;
                        const std::string& name = f[0];
                        const std::string& path = f[1];

                        int   z = 0;
                        float x = 0, y = 0, w = 0, h = 0, px = 0;
                        bool  vis = true;
                        try { z = std::stoi(f[2]); }
                        catch (...) {}
                        try { x = std::stof(f[3]); }
                        catch (...) {}
                        try { y = std::stof(f[4]); }
                        catch (...) {}
                        try { w = std::stof(f[5]); }
                        catch (...) {}
                        try { h = std::stof(f[6]); }
                        catch (...) {}
                        try { px = std::stof(f[7]); }
                        catch (...) {}
                        vis = (f[8] == "1");

                        // Campo 11 (aditivo): color. Es el PARCHE-COLOR-PREVIEW
                        // de `saveToSlot`. Va ANTES del `path.empty()` y gana sobre
                        // la ruta, igual que en el render (`SceneLayer.h`: el color
                        // se pinta antes de mirar la textura). Una capa que pasó de
                        // imagen a color arrastra su ruta vieja en `m_layerPaths`;
                        // esa ruta ya no se dibujaba, así que tampoco se carga.
                        // Grafía inválida: aviso y la capa no vuelve. Nunca un
                        // blanco inventado.
                        if (f.size() >= 11 && !f[10].empty())
                        {
                            f32 cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
                            if (!EclipParser::parseHexColor(f[10], cr, cg, cb, &ca))
                            {
                                std::cerr << "[VNScriptState] load: capa '" << name
                                    << "' color invalido: " << f[10]
                                    << " (la capa se omite)" << std::endl;
                                continue;
                            }
                            m_sceneLayers.addColorLayer(name, f[10],
                                cr, cg, cb, ca, z, x, y, w, h);
                        }
                        else
                        {
                            if (path.empty()) continue;
                            Texture* tex = m_assets.get(path);
                            if (!tex)
                            {
                                std::cerr << "[VNScriptState] load: capa '" << name
                                    << "' textura no encontrada: " << path << std::endl;
                                continue;
                            }
                            m_assets.pin(path);
                            m_layerPaths[name] = path;
                            m_layerPathOwners[name] = 0;

                            m_sceneLayers.addLayer(name, tex, z, x, y, w, h);
                        }
                        if (px != 0.0f)
                            m_sceneLayers.setParallax(name, px);
                        if (!vis)
                            m_sceneLayers.setVisible(name, false);

                        // Campo 10 (aditivo): tinte. Ausente o vacío = sin tinte
                        // (save pre-tinte carga idéntico).
                        if (f.size() >= 10 && !f[9].empty())
                        {
                            f32 tr = 1.0f, tg = 1.0f, tb = 1.0f;
                            EclipParser::parseHexColor(f[9], tr, tg, tb);
                            m_sceneLayers.setTint(name, f[9], tr, tg, tb);
                        }
                    }
                    std::cout << "[VNScriptState] Restauradas "
                        << m_sceneLayers.layerCount() << " capas (v2)" << std::endl;
                }
            }

            // ── Restaurar hotspots ──────────────────────────────────────────
            // Formato: "name;x;y;w;h|..."
            if (!d.hotspotsStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): name;x;y;w;h sin unescape. ──
                    std::istringstream hss(d.hotspotsStr);
                std::string hsEntry;
                while (std::getline(hss, hsEntry, '|'))
                {
                    if (hsEntry.empty()) continue;

                    std::istringstream fields(hsEntry);
                    std::string name, xStr, yStr, wStr, hStr;
                    if (!std::getline(fields, name, ';')) continue;
                    if (!std::getline(fields, xStr, ';')) continue;
                    if (!std::getline(fields, yStr, ';')) continue;
                    if (!std::getline(fields, wStr, ';')) continue;
                    if (!std::getline(fields, hStr, ';')) continue;

                    float x = 0, y = 0, w = 0, h = 0;
                    try { x = std::stof(xStr); }
                    catch (...) {}
                    try { y = std::stof(yStr); }
                    catch (...) {}
                    try { w = std::stof(wStr); }
                    catch (...) {}
                    try { h = std::stof(hStr); }
                    catch (...) {}

                    m_hotspots.add(name, x, y, w, h);
                }
                }
                else
                {
                    // ── v2: name;x;y;w;h|... (piso 2). ──
                    for (const auto& entry : eclips::savecodec::splitEntries(d.hotspotsStr))
                    {
                        if (entry.empty()) continue;
                        auto f = eclips::savecodec::splitFields(entry);
                        if (f.size() < 5) continue;
                        const std::string& name = f[0];
                        float x = 0, y = 0, w = 0, h = 0;
                        try { x = std::stof(f[1]); }
                        catch (...) {}
                        try { y = std::stof(f[2]); }
                        catch (...) {}
                        try { w = std::stof(f[3]); }
                        catch (...) {}
                        try { h = std::stof(f[4]); }
                        catch (...) {}
                        m_hotspots.add(name, x, y, w, h);
                    }
                }
            }

            // ── Restaurar random events ─────────────────────────────────────
            // Formato: "name;target;chance;cooldown|..."
            // clear() incondicional (espejo de m_clickMappings): un load
            // in-place no hereda eventos rancios de la sesión corriente.
            m_randomEvents.clear();
            if (!d.eventsStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): name;target;chance;cooldown. ──
                    std::istringstream ess(d.eventsStr);
                std::string evEntry;
                while (std::getline(ess, evEntry, '|'))
                {
                    if (evEntry.empty()) continue;

                    std::istringstream fields(evEntry);
                    std::string name, target, chStr, cdStr;
                    if (!std::getline(fields, name, ';')) continue;
                    if (!std::getline(fields, target, ';')) continue;
                    if (!std::getline(fields, chStr, ';')) continue;
                    if (!std::getline(fields, cdStr, ';')) continue;

                    RandomEventDef ev;
                    ev.name = name;
                    ev.target = target;
                    ev.trial.cooldownRemaining = 0.0;   // el cooldown se lava (ley 7)
                    try { ev.trial.chance = std::stof(chStr); }
                    catch (...) {}
                    try { ev.trial.cooldown = std::stof(cdStr); }
                    catch (...) {}
                    // Save sin ritmo: default 1s, que es la conducta de hoy.
                    // La fase se sortea fresca (el sorteo no se serializa).
                    ev.trial.beatRemaining =
                        eclips::trialPhase(ev.trial.period, m_rng);
                    // Campo de RAM del revival, NO gramática: el lector
                    // congelado sigue leyendo los mismos 4 campos (§7.264).
                    ev.order = m_eventOrderNext++;

                    m_randomEvents[name] = ev;
                }
                }
                else
                {
                    // ── v2: name;target;chance;cooldown|... (piso 2). ──
                    for (const auto& entry : eclips::savecodec::splitEntries(d.eventsStr))
                    {
                        if (entry.empty()) continue;
                        auto f = eclips::savecodec::splitFields(entry);
                        if (f.size() < 4) continue;
                        RandomEventDef ev;
                        ev.name = f[0];
                        ev.target = f[1];
                        ev.trial.cooldownRemaining = 0.0;  // el cooldown se lava (ley 7)
                        try { ev.trial.chance = std::stof(f[2]); }
                        catch (...) {}
                        try { ev.trial.cooldown = std::stof(f[3]); }
                        catch (...) {}

                        // Campos 5 y 6: el ritmo y la FASE del beat (ley 7).
                        // Un save viejo no los trae — entonces default 1s y
                        // fase fresca, que es exactamente la conducta que ese
                        // save siempre tuvo.
                        f32 periodoLeido = 0.0f;
                        if (f.size() >= 5 &&
                            eclips::parseFiniteFloatFull(f[4], periodoLeido))
                            ev.trial.period = eclips::clampTrialPeriod((f64)periodoLeido);

                        f32 faseLeida = 0.0f;
                        if (f.size() >= 6 &&
                            eclips::parseFiniteFloatFull(f[5], faseLeida) &&
                            faseLeida > 0.0f &&
                            (f64)faseLeida <= ev.trial.period)
                        {
                            // El reloj SÍ se restaura: si se lavara, todos los
                            // eventos re-fasearían al instante del load y se
                            // sincronizarían (ley 10 deshecha).
                            ev.trial.beatRemaining = (f64)faseLeida;
                        }
                        else
                        {
                            // Save viejo, o fase fuera de rango (archivo tocado
                            // a mano): se sortea. Nunca se hereda basura.
                            ev.trial.beatRemaining =
                                eclips::trialPhase(ev.trial.period, m_rng);
                        }
                        // ── Campos 7/8/9: el GATE `[si:]` (pierna C) ────────
                        // Un save es un archivo que el jugador puede abrir y
                        // editar (F9 de la mesa de confianza: "saves
                        // adversariales entran como ley"), así que el operador
                        // se vuelve a validar contra la MISMA allowlist del
                        // parser en vez de confiar en que lo escribimos
                        // nosotros. Y si el gate no se puede honrar, el evento
                        // NO se restaura: restaurarlo SIN gate lo pondría a
                        // dispararse siempre, que es el lado equivocado del
                        // error. Fail-closed, y con voz.
                        if (f.size() >= 7 && !f[6].empty())
                        {
                            if (f.size() >= 9 && !f[8].empty() &&
                                eclips::EclipParser::esOperadorDeCondicion(f[7]))
                            {
                                ev.gate.has = true;
                                ev.gate.var = f[6];
                                ev.gate.op  = f[7];
                                ev.gate.val = f[8];
                            }
                            else
                            {
                                std::cerr << "[VNScriptState] save: el evento '"
                                    << ev.name << "' trae un gate [si:] que no se "
                                    "entiende. El evento NO se restaura (dejarlo "
                                    "sin gate lo pondria a disparar siempre)."
                                    << std::endl;
                                continue;
                            }
                        }

                        // El orden de las ENTRADAS es el orden de definición
                        // (el escritor emite por order): re-numerar secuencial
                        // lo preserva a través del round-trip.
                        ev.order = m_eventOrderNext++;
                        m_randomEvents[ev.name] = ev;
                    }
                }
            }

            // ── Restaurar click mappings ────────────────────────────────────
            // Formato: "hotspot;escena|..."
            m_clickMappings.clear();
            auto restoreClickMapping = [this](const std::string& label,
                                              const std::string& target) {
                EclipChoice mapping;
                mapping.label = label;
                mapping.target = target;
                if (!hasHotspotNamed(mapping.label))
                {
                    warnMissingHotspotOnLoad(mapping);
                    return;
                }
                m_clickMappings.push_back(mapping);
            };
            if (!d.clicksStr.empty())
            {
                if (d.ver < 2)
                {
                    // ── Lector legacy CONGELADO (§7.212): hotspot;escena sin unescape. ──
                    std::istringstream css(d.clicksStr);
                std::string clEntry;
                while (std::getline(css, clEntry, '|'))
                {
                    if (clEntry.empty()) continue;

                    size_t semi = clEntry.find(';');
                    if (semi == std::string::npos) continue;

                    restoreClickMapping(clEntry.substr(0, semi),
                                        clEntry.substr(semi + 1));
                }
                }
                else
                {
                    // ── v2: label;target|... (piso 2). ──
                    for (const auto& entry : eclips::savecodec::splitEntries(d.clicksStr))
                    {
                        if (entry.empty()) continue;
                        auto f = eclips::savecodec::splitFields(entry);
                        if (f.size() < 2) continue;
                        restoreClickMapping(f[0], f[1]);
                    }
                }
            }

            // ── Restaurar flags ─────────────────────────────────────────────
            m_parallaxEnabled = d.parallaxOn;
            m_speakerHighlight = d.speakerHL;
            m_vignette.setInstant(d.vignetaIntensity);
            // Restaurar visibilidad de la caja (ocultar_caja persistente
            // en mid-cinemática). Estado discreto: timers de esperar/fades
            // NO se restauran — vuelven a 0 al cargar (decisión 5).
            if (d.boxHidden) m_dialogBox.hide();
            else             m_dialogBox.show();
            m_dialogBox.setBoxOverride(d.boxOverride);
            // Restaurar tema si había uno guardado
            {
                auto it = m_variables.find("_tema_activo");
                if (it != m_variables.end() && !it->second.empty())
                {
                    Theme restored = Theme::loadFromFile(it->second);
                    if (restored.defaultDialogBox.hasCustomFont())
                    {
                        m_hasFont = initBaseRenderer(
                            m_textRenderer, restored.defaultDialogBox.fontPath,
                            34.0f, restored.defaultDialogBox.kerning, "texto");
                        m_hasNameFont = initBaseRenderer(
                            m_nameRenderer, restored.defaultDialogBox.fontPath,
                            26.0f, restored.defaultDialogBox.kerning, "chrome");
                    }
                    m_theme = restored;
                }

                else
                {
                    // save sin tema → resetear colores Y fuente
                    if (m_theme.defaultDialogBox.hasCustomFont())
                    {
                        m_hasFont = initBaseRenderer(
                            m_textRenderer, Settings::defaultFontPath(), 34.0f, 0.0f, "texto");
                        m_hasNameFont = initBaseRenderer(
                            m_nameRenderer, Settings::defaultFontPath(), 26.0f, 0.0f, "chrome");
                    }
                    m_theme = Theme::defaults(); // save sin tema → resetear
                }

                refreshTextSpeedControls();
            }
            m_eventsActive = d.eventsActive;

            // Ancla de retorno de evento: clear INCONDICIONAL primero — un
            // load sin claves (save viejo, o guardado sin evento en curso) no
            // hereda un ancla de RAM de la sesión corriente.
            m_preEventScene.clear();
            m_preEventCmd = 0;

            // ── Saltar a escena y línea guardadas ───────────────────────────
            if (m_script.scenes.find(d.scene) != m_script.scenes.end())
            {
                m_currentScene = d.scene;
                m_cmdIndex = d.cmdIndex;

                // Restaurar el ancla de evento (claves aditivas): solo si la
                // escena de retorno sigue existiendo en el guion cargado.
                // Sin ella: comportamiento de siempre (la escena del evento
                // terminará en fin del guion) — degradación con voz.
                if (!d.evScene.empty())
                {
                    if (m_script.scenes.find(d.evScene) != m_script.scenes.end())
                    {
                        m_preEventScene = d.evScene;
                        m_preEventCmd = d.evCmd;
                    }
                    else
                        std::cerr << "[VNScriptState] cargar: el ancla de retorno "
                            "de evento apunta a '" << d.evScene << "', que ya no "
                            "existe en el guion. Se descarta." << std::endl;
                }

                // Restaurar la fase correcta
                // Si había capas y hotspots activos con click mappings,
                // y la fase guardada era WaitingClick, restaurar esa fase.
                VNPhase savedPhase = static_cast<VNPhase>(d.phase);
                if (savedPhase == VNPhase::WaitingClick &&
                    !m_clickMappings.empty() && m_hotspots.count() > 0)
                {
                    m_phase = VNPhase::WaitingClick;
                }
                else
                {
                    // Para cualquier otra fase, re-ejecutar desde el comando
                    // guardado — esto restaura WaitingInput/Choice/QTE
                    // naturalmente al procesar el comando.
                    m_phase = VNPhase::Executing;
                }

                // Toast discreto — aparece 2s en esquina y desaparece solo
                // Toast del MOTOR, no de Lua: por eso no pasa por `t()` (esa es
                // la frontera que CP2 firmó) sino por `langUI`, que es la
                // puerta de los strings del motor.
                m_toastMsg = eclips::langUI("Partida cargada", "Game loaded");
                m_toastTimer = 2.0f;
            }

            std::cout << "[VNScriptState] Cargado slot "
                << slot + 1 << " -> escena '" << d.scene
                << "' cmd " << d.cmdIndex
                << " (capas:" << m_sceneLayers.layerCount()
                << " hotspots:" << m_hotspots.count()
                << " fase:" << d.phase
                << ")" << std::endl;

            // Restaurar estado Lua
            if (!d.luaStateStr.empty())
                callLuaHook("on_load", d.luaStateStr);

            // Tier 4C.6: Si el slot tenía gameplay activo, diferir el push.
            // Se hace en el siguiente tick del update() para que el VNSaveMenu
            // alcance a pop-earse antes (fix Bug 1 de Fase B).
            if (d.gpHasGameplay)
            {
                std::cout << "[VNScriptState] Slot tiene gameplay activo — "
                    "Gameplay2D se lanza en siguiente tick." << std::endl;
                m_pendingGameplayData = d;
                m_pendingGameplayLaunch = true;
            }
            repinControlStylesAfterRestore();
        }

        // ── Tier 4C.6: Helper para lanzar Gameplay2D con restore ──────────
        // Extraído para reusar entre "slot puro gameplay" y "slot híbrido".
        // `loadFromSlot` termina después de unpinAll(): el conjunto propio del
        // manifest debe volver a quedar protegido antes del siguiente frame.
        void repinControlStylesAfterRestore()
        {
            pinControlStyles();
        }

        void launchGameplayFromSlot(const VNSaveMenu::SlotData& d)
        {
            // §4.3: mitad GP del futuro (gp_ver>2) — no se relanza el gameplay
            // (el bail VN de loadFromSlot cubre la mitad VN; esta, la GP).
            if (d.gpVer > 2)
            {
                std::cerr << "[VNScriptState] Gameplay del slot incompatible: "
                    << "slot_N_gp_ver=" << d.gpVer
                    << " (lee hasta v2). No se lanza." << std::endl;
                return;
            }

            auto gp = std::make_unique<Gameplay2DState>(
                m_window, m_glContext, m_stateStack, m_fontSources, m_input,
                [this]()
                {
                    m_cmdIndex++;
                    std::cout << "[VN] Regreso de gameplay 2D (post-load)."
                        << std::endl;
                }
            );
            gp->m_mapPath = d.gpMapPath;
            gp->m_sharedVars = &m_variables;
            // CP3: snapshot GP->VN (A1). Mismo carril que m_sharedVars; al
            // guardar un slot mixto el gameplay snapshotea el VN de abajo.
            gp->m_vnSaveCb = [this](int slot, const std::string& file)
                { saveToSlot(slot, file); };

            // Setear el restore pendiente — onEnter lo aplicará
            gp->m_pendingRestore.hasData = true;
            gp->m_pendingRestore.posX = d.gpPosX;
            gp->m_pendingRestore.posY = d.gpPosY;
            gp->m_pendingRestore.playerHP = d.gpPlayerHP;
            gp->m_pendingRestore.playerMaxHP = d.gpPlayerMaxHP;
            gp->m_pendingRestore.checkpointX = d.gpCheckpointX;
            gp->m_pendingRestore.checkpointY = d.gpCheckpointY;
            gp->m_pendingRestore.hasCheckpoint = d.gpHasCheckpoint;
            gp->m_pendingRestore.varsStr = d.gpVarsStr;
            gp->m_pendingRestore.ver = d.gpVer;

            m_stateStack->push(std::move(gp));
            m_waitingForGameplay = true;
        }

        // Ola 3 · Slice B — ¿este VN es una pausa-sobre-juego? rol Menu + un
        // back-pointer al juego congelado de abajo. Distingue el menú-título
        // (sin puntero: guardar suprimido, cargar empuja juego fresco) de la
        // pausa (con puntero: guardar/cargar operan sobre el juego de abajo).
        bool isPauseOverGame() const
        {
            return m_role == VNRole::Menu && m_pausedGame != nullptr;
        }

        void openSaveMenu(VNSaveMenu::Mode mode)
        {
            // §3.3 guard de cinturón (Save): no hay partida que guardar desde un
            // menú-TÍTULO. Cubre verbo `guardar`, F5, chrome de guardado. PERO una
            // pausa-sobre-juego (isPauseOverGame) SÍ guarda: snapshotea el juego
            // congelado de abajo (Slice B). Load NO se suprime: §3.4.
            if (m_role == VNRole::Menu && mode == VNSaveMenu::Mode::Save
                && !m_pausedGame)
            {
                std::cerr << "[VN rol Menu] 'guardar' suprimido: no hay partida "
                    "que guardar desde un menu (sin juego abajo).\n";
                return;
            }

            // Ola 3 · Slice B: guardar-desde-pausa. Espejo del bloque Load override
            // (abajo): ¿el creador registró 'guardar'/'save'? → push del save-screen
            // autorable en rol Menu, HEREDANDO m_pausedGame (el juego, no `this`)
            // para que su ranura alcance al juego de abajo. Ruta vacía/rota → cae al
            // VNSaveMenu C++ modo Save, cuyo saveCb (abajo) ya snapshotea abajo.
            if (isPauseOverGame() && mode == VNSaveMenu::Mode::Save && m_manifest)
            {
                auto it = m_manifest->pantallas.find("guardar");
                if (it == m_manifest->pantallas.end())
                    it = m_manifest->pantallas.find("save");
                if (it != m_manifest->pantallas.end() && !it->second.empty())
                {
                    std::string resolved =
                        eclips::resolveScriptPath(it->second, m_manifest);
                    if (!resolved.empty())
                    {
                        m_stateStack->cmdPush(std::make_unique<VNScriptState>(
                            m_window, m_glContext, m_stateStack, m_fontSources,
                            m_input, m_lang,
                            resolved, -1, m_manifest, m_meta, m_seen,
                            VNRole::Menu, m_pausedGame   // hereda el juego de abajo
                        ));
                        return;
                    }
                    std::cerr << "[VN guardar] Override '" << it->second
                        << "' no existe; uso el VNSaveMenu por defecto.\n";
                }
            }

            // Ola 2 · C1: override autorable de la pantalla de Load. Solo rol Menu
            // (cargar-desde-titulo); el push espeja a openSettings (rol Menu, via la
            // cola del ciclo). Bilingue: el creador pudo escribir 'cargar' o 'load'.
            // Ruta vacia/rota -> cae al VNSaveMenu C++ (§7.118: roto != desaparecido).
            // Rol Game (pausa->cargar, load in-place) NO se rutea aqui: enredado con
            // el rediseno de pausa (Ola 3) -> conserva el menu C++.
            if (m_role == VNRole::Menu && mode == VNSaveMenu::Mode::Load && m_manifest)
            {
                auto it = m_manifest->pantallas.find("cargar");
                if (it == m_manifest->pantallas.end())
                    it = m_manifest->pantallas.find("load");
                if (it != m_manifest->pantallas.end() && !it->second.empty())
                {
                    std::string resolved =
                        eclips::resolveScriptPath(it->second, m_manifest);
                    if (!resolved.empty())
                    {
                        m_stateStack->cmdPush(std::make_unique<VNScriptState>(
                            m_window, m_glContext, m_stateStack, m_fontSources,
                            m_input, m_lang,
                            resolved, -1, m_manifest, m_meta, m_seen,
                            VNRole::Menu, m_pausedGame   // hereda el juego (null si título)
                        ));
                        return;
                    }
                    std::cerr << "[VN cargar] Override '" << it->second
                        << "' no existe; uso el VNSaveMenu por defecto.\n";
                }
            }

            // §3.4 redefinición del callback de load por rol:
            //  · Game → loadFromSlot(s) in-place (cargar a mitad de partida).
            //  · Menu → empujar un VN-juego fresco desde el slot (cargar desde el
            //    menú NO transforma el menú en el juego; el menú queda al fondo).
            std::function<void(int)> loadCb;
            if (isPauseOverGame())
                // Slice B: cargar in-place en el juego de abajo + desmontar los
                // menús hasta él (el VNSaveMenu se auto-popea primero; cmdPopAbove
                // se lleva el resto). Depth-independiente.
                loadCb = [this](int s) {
                m_pausedGame->loadFromSlot(s);
                m_stateStack->cmdPopAbove(m_pausedGame);
                };
            else if (m_role == VNRole::Menu)
                loadCb = [this](int s) { loadGameFromMenu(s); };
            else
                loadCb = [this](int s) { loadFromSlot(s); };

            // Ola 3 · Slice B: el saveCb del fallback C++ debe snapshotear el
            // juego de ABAJO si somos pausa-sobre-juego (this = pausa, estado
            // vacío). En menú-título no se llega aquí en modo Save (guard arriba).
            std::function<void(int)> saveCb;
            if (isPauseOverGame())
                saveCb = [this](int s) { m_pausedGame->saveToSlot(s); };
            else
                saveCb = [this](int s) { saveToSlot(s); };

            // Idiomas CP3 (fork 4): este menú C++ dibujaba el string que
            // `strftime` persistió al guardar, así que nunca honró
            // `ranura_fecha_formato` y quedaba congelado en el idioma del
            // guardado. Se le pasa la plantilla del tema YA traducida y el
            // contexto de idioma; con un save viejo sin `time_raw` cae al
            // string de siempre, que es todo lo que ese disco tiene.
            const DialogBoxStyle& dsFecha = m_dialogBox.currentStyle();
            VNSaveMenu::FechaSlot fechaCfg;
            fechaCfg.formato = t(dsFecha.slotDateFormat);
            fechaCfg.lang    = fechaCtx(dsFecha);

            // D1: cmdPush, NO push inmediato (razonamiento del compuesto en CP2).
            m_stateStack->cmdPush(std::make_unique<VNSaveMenu>(
                m_window, m_stateStack, m_fontSources, mode,
                saveCb,
                loadCb,
                VNSaveMenu::Context::VN,
                std::string(),
                VNSaveMenu::NUM_SLOTS,
                fechaCfg
            ));
        }

        // §3.4 — Cargar un juego DESDE el menú (rol Menu). Empuja un VN-juego
        // fresco (rol Game) con el slot elegido; el menú-VN queda al fondo, así
        // que `fin`/`titulo` del juego regresan al menú. Espejo del flujo
        // m_pendingLoadSlot de MainMenuState (L137-167), adaptado: el fallback de
        // ruta es script_inicial del manifest (NO m_scriptPath, que aquí es el
        // menu.ecl). Dormido hasta que exista un VN rol Menu (CP4); se ejercita
        // en el ciclo rey (CP5).
        void loadGameFromMenu(int slot)
        {
            auto slotInfo = VNSaveMenu::readSlot(slot);
            std::string fallback = (m_manifest && !m_manifest->scriptInicial.empty())
                ? m_manifest->scriptInicial : m_scriptPath;
            std::string loadPath = (!slotInfo.scriptPath.empty())
                ? slotInfo.scriptPath : fallback;

            std::string resolved = eclips::resolveScriptPath(loadPath, m_manifest);
            if (resolved.empty())
            {
                std::cerr << "[VN rol Menu] El slot " << slot + 1
                    << " referencia un script inexistente: '" << loadPath << "'.\n";
                return;
            }

            m_stateStack->cmdPush(std::make_unique<VNScriptState>(
                m_window, m_glContext, m_stateStack, m_fontSources,
                m_input, m_lang,
                resolved, slot, m_manifest, m_meta, m_seen, VNRole::Game
            ));
        }

        // §4 — nuevo_juego: lanzar el JUEGO desde cero. cmdPopToBottom (baja al
        // fondo; no-opea a profundidad 1) + cmdPush(VN-juego fresco rol Game con
        // script_inicial). Espejo de MainMenuState::startNewGame, adaptado: usa
        // m_manifest->scriptInicial (NO m_scriptPath, que en un VN-menú es el
        // menu.ecl). Sin script_inicial → cerr pedagógico, sin tocar el stack.
        // Caso estrella de la cola: una sola implementación válida desde
        // cualquier profundidad.
        void startNewGame()
        {
            if (!m_manifest || m_manifest->scriptInicial.empty())
            {
                std::cerr << "[VN] 'nuevo_juego' sin 'script_inicial' en el "
                    "manifest: no hay juego que lanzar.\n";
                return;
            }
            std::string resolved =
                eclips::resolveScriptPath(m_manifest->scriptInicial, m_manifest);
            if (resolved.empty())
            {
                std::cerr << "[VN] 'nuevo_juego': script_inicial '"
                    << m_manifest->scriptInicial << "' no existe.\n";
                return;
            }
            m_stateStack->cmdPopToBottom();
            m_stateStack->cmdPush(std::make_unique<VNScriptState>(
                m_window, m_glContext, m_stateStack, m_fontSources,
                m_input, m_lang,
                resolved, -1, m_manifest, m_meta, m_seen, VNRole::Game
            ));
        }

        // ── continuar: cargar la partida MÁS RECIENTE directo, sin pantalla de
        // Load. La otra mitad de "Continuar-en-MainMenu": `_saves` cubre
        // ocultar/mostrar el botón (con [si: _saves > 0]); ESTO lo ejecuta.
        // Solo rol Menu — "continuar a mitad de partida" no tiene caller
        // (§7.118), así que desde rol Game da cerr y no toca el stack (espejo
        // inverso del guard de openPauseMenu). Reusa loadGameFromMenu ENTERO
        // (la maquinaria de carga ya existe, §7.6); aquí solo se elige el slot.
        void continueGame()
        {
            if (m_role != VNRole::Menu)
            {
                std::cerr << "[VN] 'continuar' solo desde un menu (rol Menu); "
                    "a mitad de partida no aplica.\n";
                return;
            }
            int best = VNSaveMenu::mostRecentSlot();
            if (best < 0)
            {
                std::cerr << "[VN] 'continuar': no hay partidas guardadas. "
                    "Tip: gatea el boton con [si: _saves > 0].\n";
                return;
            }
            loadGameFromMenu(best);
        }

        // ── Convertir VNPosition a string para serializar ─────────────────────

        static std::string positionToString(VNPosition p)
        {
            switch (p)
            {
            case VNPosition::Izquierda:  return "izquierda";
            case VNPosition::CentroIzq:  return "centro_izq";
            case VNPosition::CentroDer:  return "centro_der";
            case VNPosition::Derecha:    return "derecha";
            default:                     return "centro";
            }
        }

        // ── Historial de diálogos ─────────────────────────────────────────────

        //void pushHistory(const std::string& speaker, const std::string& text)
        //{
            //if (text.empty()) return;

            // Buffer circular: si está lleno, borra la entrada más antigua
            //if ((int)m_history.size() >= HISTORY_MAX)
                //m_history.pop_front();

            //m_history.push_back({ speaker, text });
        //}

        // ── Variables / Condicionales ──────────────────────────────────────────

        // Resolver variable: si empieza con _ es del sistema (hora, fecha),
        // si no, se busca en m_variables.
        std::string resolveVariable(const std::string& varName) const
        {
            // Variables del sistema — leídas del reloj en tiempo real
            if (!varName.empty() && varName[0] == '_')
            {
                // Idioma activo: sólo existe como dato cuando el sistema está
                // encendido. Sin catálogo m_lang es nullptr y se conserva la
                // respuesta vacía de cualquier variable de sistema desconocida.
                if (varName == "_idioma" || varName == "_language")
                    return m_lang ? m_lang->code() : std::string();

                // Partidas guardadas (independiente del reloj): el menú oculta
                // "Cargar" sin saves con [si: _saves > 0]. 1 sola lectura del
                // archivo (VNSaveMenu::countSaves). Bilingue _saves/_partidas.
                if (varName == "_saves" || varName == "_partidas")
                    return std::to_string(VNSaveMenu::countSaves());

                // VERSION-UNICA: numero canonico de Types.h (sin "v", sin etiqueta)
                if (varName == "_version_motor" || varName == "_engine_version")
                    return std::string(constants::ENGINE_VERSION);

                // ── `CP3`: la grafía inglesa del pick del pool ──────────────
                //
                // `_palabra` / `_palabra_var` NO están acá: viven en
                // `m_variables` y las alcanza el fall-through de abajo, que es
                // lo que las hace sobrevivir a un load (ver `picarPalabra`).
                // Lo que sí necesita una rama es el ALIAS, y son cuatro líneas
                // en vez de escribir las cuatro claves al guardar: con dos
                // entradas separadas, un `set _palabra = x` del creador movería
                // una grafía y no la otra — dos fuentes para el mismo dato, que
                // es la enfermedad que este repo viene deshaciendo. Una fuente,
                // dos grafías, igual que `_hora`/`_hour`.
                //
                // `palabra` se traduce porque es cotidiana y `var` no porque es
                // abreviatura (`D-32`).
                if (varName == "_word" || varName == "_word_var")
                {
                    const std::string canon = (varName == "_word")
                        ? "_palabra" : "_palabra_var";
                    auto itw = m_variables.find(canon);
                    return (itw != m_variables.end()) ? itw->second
                                                      : std::string();
                }

                // ── `CP9`: la tecla de una acción (`${_tecla.vn.avanzar}`) ──
                //
                // La otra cara de la tabla del `CP0` (§7.205). Toda la ley vive
                // en `InputManager.h` y es pura; acá sólo se pregunta.
                //
                // **Sin guarda de nulo, por la decisión escrita de `CP1`** (ver
                // `translateSDLEventToAction`): el constructor exige `m_input`
                // sin default, así que un `if` acá convertiría un error de
                // cableado en una pantalla de controles muda.
                //
                // Un nombre nuestro que NO resuelve devuelve `""` acá, y quien
                // lo convierte en literal-más-aviso es `interpolateVars` — que
                // es el único de los dos sitios que puede dejar un literal.
                {
                    const eclips::InputDisplayRes td =
                        eclips::inputPartirNombreDisplay(varName);
                    if (td.causa == eclips::InputDisplayCausa::Ok)
                        // DI-7: `${_tecla.*}` y `${_key.*}` pintan lo mismo —
                        // la grafía con que el creador pidió la variable no
                        // decide el idioma en que juega el jugador.
                        return eclips::inputDisplayDeTeclas(
                            m_input->teclasDe(td.verbo, td.dominio),
                            eclips::langActivo());
                    if (td.causa != eclips::InputDisplayCausa::NoEsNuestra)
                        return std::string();
                }

                std::time_t now = std::time(nullptr);
                std::tm* t = std::localtime(&now);
                if (!t) return "0";

                if (varName == "_hora" || varName == "_hour")
                    return std::to_string(t->tm_hour);       // 0-23
                if (varName == "_minuto" || varName == "_minute")
                    return std::to_string(t->tm_min);        // 0-59
                // Variantes con cero a la izquierda (para relojes "08:05").
                // _hora/_minuto siguen crudas para condicionales numericos.
                if (varName == "_hora_pad" || varName == "_hour_pad")
                {
                    char buf[3]; std::snprintf(buf, sizeof(buf), "%02d", t->tm_hour);
                    return std::string(buf);                 // 00-23
                }
                if (varName == "_minuto_pad" || varName == "_minute_pad")
                {
                    char buf[3]; std::snprintf(buf, sizeof(buf), "%02d", t->tm_min);
                    return std::string(buf);                 // 00-59
                }
                if (varName == "_dia" || varName == "_day")
                    return std::to_string(t->tm_mday);       // 1-31
                if (varName == "_mes" || varName == "_month")
                    return std::to_string(t->tm_mon + 1);    // 1-12
                if (varName == "_anio" || varName == "_year")
                    return std::to_string(t->tm_year + 1900); // 2025, 2026...
                if (varName == "_dia_semana" || varName == "_weekday")
                    return std::to_string(t->tm_wday + 1);   // 1-7 (1=domingo)
            }

            // Variable normal del script (save-local)
            auto it = m_variables.find(varName);
            if (it != m_variables.end())
                return it->second;

            // Nivel meta (MetaStore): persistente inter-save. Se consulta
            // DESPUÉS de save-local → en colisión, save-local gana (el contexto
            // de la partida activa manda). En la práctica no colisionan (P4
            // avisa 1x si un nombre vive en ambos mundos).
            if (m_meta && m_meta->has(varName))
                return m_meta->get(varName);

            return "";
        }

        // Interpolacion de variables en texto: ${nombre}
        // Sustituye ${nombre} por su valor (via resolveVariable), un solo pase.
        // - Usuario (m_variables) y sistema (_hora, _minuto, _dia, _mes, _anio...).
        // - Inexistente y SIN prefijo '_': deja el literal ${nombre} + aviso 1x
        //   (atrapa typos y nombres sin setear). Un '_nombre' no reconocido cae a
        //   vacio (el '_' es intencion explicita del creador).
        // - '${' sin cerrar: el resto queda literal (sin loop). '${otra}' dentro de
        //   un valor NO se re-expande. Un valor con {wavy} SI ondula (parseRichText).
        std::string interpolateVars(const std::string& text) const
        {
            if (text.find("${") == std::string::npos)
                return text;

            std::string out;
            out.reserve(text.size());

            size_t i = 0;
            while (i < text.size())
            {
                if (text[i] == '$' && i + 1 < text.size() && text[i + 1] == '{')
                {
                    size_t close = text.find('}', i + 2);
                    if (close == std::string::npos)
                    {
                        out.append(text, i, std::string::npos);
                        break;
                    }

                    std::string name = text.substr(i + 2, close - (i + 2));
                    size_t a = name.find_first_not_of(" \t");
                    size_t b = name.find_last_not_of(" \t");
                    if (a == std::string::npos) name.clear();
                    else name = name.substr(a, b - a + 1);

                    // ── `CP9`: la familia `_tecla.*` SÍ tiene censo ──────────
                    //
                    // El `||name[0]=='_'` de abajo exime al namespace de sistema
                    // de la caza de typos, y su razón está escrita: *«el `_` es
                    // intención explícita del creador»*. Esa exención vale
                    // porque el reloj **no se puede enumerar** — no hay lista
                    // contra la cual decir que `_hroa` está mal.
                    //
                    // **`_tecla.*` sí se enumera: son 31 filas de una tabla.**
                    // Así que acá la exención no aplica, y dejarla puesta cuesta
                    // caro por una razón del propio contrato: un typo saldría
                    // como cadena **vacía**, o sea *«Presioná  para saltar»* —
                    // el hueco de Cuphead que el §12 cita para descartarlo, y
                    // que además se ve idéntico a una acción legítimamente
                    // desligada. Dos situaciones distintas, una sola salida.
                    //
                    // Queda la ley que este mismo bloque ya tiene para todo lo
                    // que no empieza con `_`: literal + aviso 1× por nombre. La
                    // política de repetición es §7.493 contestada con sus dos
                    // preguntas — el sitio corre **una vez por comando
                    // ejecutado** (no por frame) y un typo en un `.ecl` es causa
                    // **permanente**.
                    {
                        const eclips::InputDisplayRes td =
                            eclips::inputPartirNombreDisplay(name);
                        if (td.causa != eclips::InputDisplayCausa::NoEsNuestra
                            && td.causa != eclips::InputDisplayCausa::Ok)
                        {
                            if (m_warnedVars.find(name) == m_warnedVars.end())
                            {
                                std::cout << eclips::inputVozDelDisplay(td, name)
                                          << std::endl;
                                m_warnedVars[name] = true;
                            }
                            out += "${"; out += name; out += "}";
                            i = close + 1;
                            continue;
                        }
                    }

                    bool exists = (m_variables.find(name) != m_variables.end())
                        || (!name.empty() && name[0] == '_')
                        || (m_meta && m_meta->has(name));   // meta también "existe"

                    if (exists)
                    {
                        out += resolveVariable(name);
                    }
                    else
                    {
                        if (m_warnedVars.find(name) == m_warnedVars.end())
                        {
                            std::cout << "[VNScriptState] interpolacion: la variable \""
                                << name << "\" no existe (ni de sistema ni seteada con "
                                << "'set'). Dejo el literal ${" << name << "}." << std::endl;
                            m_warnedVars[name] = true;
                        }
                        out += "${"; out += name; out += "}";
                    }

                    i = close + 1;
                }
                else
                {
                    out += text[i];
                    ++i;
                }
            }

            return out;
        }

        // B2 — cuenta codepoints UTF-8 (bytes líderes), no bytes. Lo usa el
        // límite 'max' (que cuenta caracteres, no bytes — §1.45).
        //
        // §7.410 — el cuerpo se mudó a `engine/core/TextBudget.h`, que es
        // alcanzable desde un harness (este archivo NO lo es: arrastra
        // SDL/GL/Lua/audio). Queda como delegación y no como copia porque el
        // typewriter de la caja necesita EXACTAMENTE esta cuenta, y dos cuerpos
        // idénticos en dos archivos es cómo se desincronizan en silencio.
        static int countCodepoints(const std::string& s)
        {
            return (int)eclips::text::countCodepoints(s);
        }

        // B2 — único punto de salida de la captura de texto ('entrada').
        // Garantiza que la variable SIEMPRE quede definida: confirmar escribe
        // el buffer, cancelar escribe "" (en P5, el 'default'). Apaga el
        // teclado/IME, avanza el comando y vuelve a Executing. Espeja
        // executeSet: captura oldVal y dispara on_variable_changed para que
        // un hook Lua que observe la variable reaccione igual que con 'set'.
        void commitTextInput(const std::string& value)
        {
            // Regla de oro: la variable SIEMPRE queda definida. Si el valor es
            // vacío (confirmó sin teclear, o canceló) → el default (que es ""
            // si el creador no puso uno). Punto único donde vive esto.
            std::string finalValue = value.empty() ? m_inputDefault : value;
            if (!m_inputVar.empty())
            {
                std::string oldVal;
                auto it = m_variables.find(m_inputVar);
                if (it != m_variables.end()) oldVal = it->second;

                m_variables[m_inputVar] = finalValue;
                callLuaHook("on_variable_changed", m_inputVar, oldVal, finalValue);
            }
            SDL_StopTextInput();
            std::cout << "[entrada] '" << m_inputVar << "' = '"
                << finalValue << "'" << std::endl;
            m_cmdIndex++;
            m_phase = VNPhase::Executing;
            m_inputOkHover = false;   // CP3: limpiar hover al salir de la captura
        }

        // ── LA PUERTA ÚNICA DE `SET-NO-PROTEGE-EL-GUION-BAJO` ────────────────
        //
        // Contesta `true` si el nombre tiene dueño — y en ese caso YA HABLÓ, así
        // que el caller sólo tiene que abortar su escritura.
        //
        // **Existe porque la ley custodiaba UNA de CINCO puertas.** El lote que
        // la escribió censó quién escribe *el nombre* y cableó `executeSet`; el
        // censo desde el EFECTO (`m_variables[` sobre este archivo) da **seis
        // escrituras con nombre del creador**, y las otras cuatro entraban sin
        // mirar nada: el comando `entrada` y los tres verbos Lua. Es §7.538 un
        // piso más arriba — contar desde el input daba una.
        //
        // El `comando` va por parámetro y **no tiene default a propósito**: la
        // voz nombra lo que el creador escribió, y un default la haría mandar a
        // leer el renglón equivocado del manual. Lo custodia `harness_nombres §9`.
        //
        // La política de repetición NO es nueva: `m_warnedCompare` es el mismo
        // mapa que usan las otras voces de este archivo (§7.493) — warn-once por
        // TEXTO y `clear()` en el load, para que no salga una voz por frame.
        //
        // **Y como el texto ahora lleva el comando adentro, el warn-once pasó a
        // ser por (comando, nombre) y no por nombre.** No es un efecto colateral
        // que se tolera: es la conducta correcta, y la medición la corrigió —
        // esta línea afirmaba lo opuesto hasta que la sonda contó 3 voces donde
        // el comentario prometía 1.
        //
        // El motivo es el mismo que el del parámetro: un creador que escribió
        // `set _hora = 5` **y** `entrada _hora "..."` tiene DOS líneas que
        // arreglar. Suprimir la segunda por tener el mismo nombre le avisa de
        // una, la arregla, y se va creyendo que terminó — que es exactamente el
        // fallo mudo que este lote vino a cerrar.
        //
        // Lo que NO cambió, y es lo que se puede regresionar: los DOS call-sites
        // de `executeSet` siguen compartiendo comando, así que siguen dando UNA
        // sola voz. `correr_sonda_nombres.ps1` mide las dos cosas por separado.
        // `almacen` entro con `meta-set-simetrico` y va SIN DEFAULT, igual que
        // `comando`: las cuatro puertas del MetaStore usan esta misma puerta y
        // la voz tiene que decir la verdad de SU store (`meta_set` no escribe
        // en el save). Con default, la quinta puerta que se agregue hereda el
        // almacen de la primera y nada se pone rojo.
        //
        // NO cambia la granularidad del warn-once en la practica: la clave es
        // el TEXTO, y el texto ya llevaba el comando adentro -- `set` y
        // `meta_set` son comandos distintos, asi que ya hablaban por separado.
        // El almacen lo hace explicito, no lo inventa.
        bool nombreConDuenio(const std::string& varName, const char* comando,
                             eclips::AlmacenDeNombre almacen)
        {
            const eclips::DuenioDeNombre duenio = eclips::duenioDeNombre(varName);
            if (duenio == eclips::DuenioDeNombre::Libre) return false;

            const std::string voz =
                eclips::vozNombreReservado(duenio, varName, comando, almacen);
            if (m_warnedCompare.find(voz) == m_warnedCompare.end())
            {
                std::cout << voz << std::endl;
                m_warnedCompare[voz] = true;
            }
            return true;
        }

        void executeSet(const EclipCmd& cmd)
        {
            const std::string& varName = cmd.arg1;
            const std::string& op = cmd.arg2;
            const std::string& val = cmd.arg3;

            // ── SET-NO-PROTEGE-EL-GUION-BAJO ─────────────────────────────────
            //
            // Va PRIMERO, antes de capturar `oldVal` y antes del hook de Lua:
            // si el nombre tiene dueño, acá no pasó nada. Un `on_variable_changed`
            // por una escritura que no ocurrió sería peor que el bug original,
            // porque le daría a un script Lua un cambio que el motor no hizo.
            //
            // La ley entera vive en `VNNombreReservado.h` y es SDL-free (mismo
            // reparto que `VNCompare.h`): allá se decide DE QUIÉN es el nombre,
            // acá se decide qué hacer con esa respuesta. Y `Libre` es la
            // respuesta para casi todo — incluidos `_palabra`, `_palabra_var`,
            // `_batalla_ganada` y cualquier `_nombre` que el creador invente,
            // que hoy se leen por el fall-through de `resolveVariable`.
            //
            // La política de repetición NO es nueva: `m_warnedCompare` es el
            // mismo mapa que usan las otras tres voces de este archivo (§7.493),
            // warn-once por texto y `clear()` en el load. Una voz por nombre
            // equivocado, no una por frame.
            //
            // (Este comentario decía *«`executeMetaSet` NO se toca en este
            // lote: sigue rechazando TODO `_`»*, y era cierto cuando se
            // escribió. `meta-set-simetrico` pagó esa ficha: hoy las CUATRO
            // puertas del MetaStore preguntan esta misma ley, con la misma
            // tabla y otra voz. Ya no hay dos leyes de nombres en el motor.)
            if (nombreConDuenio(varName, "set", eclips::AlmacenDeNombre::Partida)) return;

            // Capturar valor anterior para el hook
            std::string oldVal;
            auto oldIt = m_variables.find(varName);
            if (oldIt != m_variables.end())
                oldVal = oldIt->second;

            if (op == "=")
            {
                // `mutar copia = ${sayori}` COPIA el valor. Es la pieza que el
                // patrón de máximo necesita (`mutar top = ${monika}`), y de
                // paso mueve una línea del manual: hasta hoy el valor se
                // guardaba crudo y `${}` adentro de una variable no se
                // re-expandía nunca; ahora se expande AL ESCRIBIR. Medido:
                // cero líneas del corpus vivo guardan un `${}` sin expandir.
                m_variables[varName] = interpolateVars(val);
            }
            else if (op == "+" || op == "-")
            {
                std::string actual;
                auto it = m_variables.find(varName);
                if (it != m_variables.end()) actual = it->second;

                const std::string operando = interpolateVars(val);
                const eclips::MutarPaso paso =
                    eclips::mutarAritmetica(actual, op, operando);

                if (!paso.aplicar)
                {
                    // Fail-closed DURO, y más duro que el del `si` a propósito:
                    // una condición que da falso se vuelve a preguntar, una
                    // variable pisada con un número inventado ya perdió su
                    // valor. Antes de este lote el `catch` dejaba `delta = 1`
                    // y `mutar dif - natsuki` restaba uno sin decir nada.
                    const std::string voz =
                        eclips::mutarVoz(paso.causa, varName, actual, op, operando);
                    if (m_warnedCompare.find(voz) == m_warnedCompare.end())
                    {
                        std::cout << voz << std::endl;
                        m_warnedCompare[voz] = true;
                    }
                    return;
                }

                m_variables[varName] = std::to_string(paso.resultado);
            }
            else
            {
                std::cerr << "[VNScriptState] set: operador desconocido '"
                    << op << "'" << std::endl;
            }

            std::cout << "[VNScriptState] set " << varName << " = "
                << m_variables[varName] << std::endl;

            // Hook Lua
            callLuaHook("on_variable_changed", varName, oldVal, m_variables[varName]);
        }

        // Igual que executeSet pero escribe en el MetaStore (persistente
        // inter-save) en vez de m_variables. Write-through: cada cambio se
        // flushea a meta.eclips (atómico).
        void executeMetaSet(const EclipCmd& cmd)
        {
            const std::string& varName = cmd.arg1;
            const std::string& op = cmd.arg2;
            const std::string& val = cmd.arg3;

            // ── META-SET-MAS-ESTRICTO-QUE-SET ────────────────────────────────
            //
            // Va PRIMERO —antes incluso del chequeo del store— por la misma
            // razón que en `executeSet`: si el nombre tiene dueño, acá no pasó
            // nada. Y el orden importa para el que lee la consola: *«el
            // MetaStore no está disponible»* es un problema de cableado del
            // motor y *«ese nombre es del reloj»* es la línea que el creador
            // escribió. Si faltan las dos cosas, la accionable es la segunda.
            //
            // ANTES DE ESTE LOTE acá había un rechazo POR PREFIJO: todo `_`
            // afuera, *«reservado para variables de sistema»*. **En este store
            // no vive ninguna variable de sistema:** el motor escribe UNA sola
            // clave al MetaStore (`main.cpp:332` → `intro_motor_vista`) y no
            // lleva `_`. Custodiaba un conjunto vacío mientras
            // `eclips.meta_set/add/sub` escribían cualquier cosa por la puerta
            // de al lado, o sea que la regla del `.ecl` ya era esquivable.
            //
            // **El rechazo no desapareció: cambió de dueño.** Dejó de proteger
            // variables de sistema inexistentes y pasa a proteger al LECTOR —
            // `resolveVariable` consulta el MetaStore DESPUÉS del bloque del
            // `_`, así que un `_hora` guardado acá es invisible para `${}` y
            // para `si`, y encima sobrevive a borrar los saves. Lo que se
            // relajó es lo que la ficha pedía: un `_nombre` del creador ahora
            // puede ser persistente, igual que ya podía ser de partida.
            //
            // La voz dice `meta_set` aunque el creador haya escrito
            // `meta_mutar`, y es una limitación conocida, no un descuido:
            // `EclipCmd` no guarda la grafía que se escribió (los gemelos
            // colapsan en `EclipCmdType::MetaSet` en el parser). Le pasa lo
            // mismo a `set`/`mutar` desde el lote anterior. Está fichado —
            // arreglarlo toca la struct y el parser de todos los gemelos.
            if (nombreConDuenio(varName, "meta_set", eclips::AlmacenDeNombre::Meta))
                return;

            if (!m_meta)
            {
                std::cerr << "[VNScriptState] meta_set: MetaStore no disponible "
                    << "(modo test sin Engine). '" << varName << "' ignorada."
                    << std::endl;
                return;
            }

            // Warn-once de colisión: el nombre vive como variable de partida
            // (set) Y como meta. En lectura gana la de partida; avisamos 1x.
            if (m_variables.find(varName) != m_variables.end()
                && m_warnedMetaCollision.find(varName) == m_warnedMetaCollision.end())
            {
                std::cerr << "[VNScriptState] meta_set: '" << varName
                    << "' ya existe como variable de partida (set). En lectura "
                    << "gana la de partida; renombra una para evitar confusion."
                    << std::endl;
                m_warnedMetaCollision[varName] = true;
            }

            std::string oldVal = m_meta->get(varName);

            // El gemelo persistente de `executeSet`, y la misma ley: si los dos
            // no se movieran juntos, `mutar` y `meta_mutar` dejarían de ser
            // gemelos y el creador tendría que acordarse de cuál es cuál.
            if (op == "=")
            {
                m_meta->set(varName, interpolateVars(val));
            }
            else if (op == "+" || op == "-")
            {
                const std::string actual = m_meta->get(varName, "0");
                const std::string operando = interpolateVars(val);
                const eclips::MutarPaso paso =
                    eclips::mutarAritmetica(actual, op, operando);

                if (!paso.aplicar)
                {
                    const std::string voz =
                        eclips::mutarVoz(paso.causa, varName, actual, op, operando);
                    if (m_warnedCompare.find(voz) == m_warnedCompare.end())
                    {
                        std::cout << voz << std::endl;
                        m_warnedCompare[voz] = true;
                    }
                    return;   // §7.264 en miniatura: nada llega a `meta.eclips`
                }

                m_meta->set(varName, std::to_string(paso.resultado));
            }
            else
            {
                std::cerr << "[VNScriptState] meta_set: operador desconocido '"
                    << op << "'" << std::endl;
                return;
            }

            std::cout << "[VNScriptState] meta_set " << varName << " = "
                << m_meta->get(varName) << "  (persistido en meta.eclips)"
                << std::endl;

            // Mismo hook que set: una variable cambió (de partida o meta).
            callLuaHook("on_variable_changed", varName, oldVal, m_meta->get(varName));
        }

        // Vaciar TODO el progreso permanente (meta_borrar / meta_reset).
        void executeMetaClear()
        {
            if (!m_meta)
            {
                std::cerr << "[VNScriptState] meta_borrar: MetaStore no disponible "
                    << "(modo test)." << std::endl;
                return;
            }
            m_meta->clear();
            m_warnedMetaCollision.clear();   // re-evaluar colisiones tras el wipe
            std::cout << "[VNScriptState] meta_borrar: progreso permanente vaciado."
                << std::endl;
        }

        bool evaluateCondition(const std::string& varName, const std::string& op, const std::string& val) const
        {
            // Obtener valor actual de la variable
            // Variables del sistema (_hora, _minuto, etc) se resuelven en tiempo real
            std::string current = resolveVariable(varName);

            // ── `SI-NO-COMPARA-VARIABLES`: el lado derecho pasa por `${}` ───
            //
            // Hasta este lote el derecho era literal SIEMPRE, así que
            // `si sayori > natsuki` era `sayori > 0` con un `catch` mudo, y
            // las tres ramas de ganador corrían a la vez (§7.532).
            //
            // **No hay sintaxis nueva.** Entra el MISMO `${var}` que el motor
            // ya usa en diálogo, labels y prompts —el manual lo llama *«el que
            // LEE»* (`DOCS §10`)—, y el marcador no es adorno: el lado derecho
            // TAMBIÉN es un namespace de literales (`si gano == sayori`, con
            // `sayori` variable viva en el mismo archivo), y `${}` es lo único
            // que los separa. Medido: sin marcador, la regla mata 3 de las 4
            // ramas de ganador del fixture, en silencio.
            //
            // Un `val` sin `${` sale intacto de acá — el lote es ADITIVO, y es
            // lo que mide la §1 de `harness_comparar` sobre el corpus vivo.
            const std::string der = interpolateVars(val);

            const eclips::CompararResultado r =
                eclips::compararValores(current, op, der);

            if (r.causa != eclips::CompararCausa::Ok)
            {
                // §7.493: un typo en un `.ecl` es causa PERMANENTE, y un `si`
                // adentro de un bucle `ir_a` pasaría por acá cada vuelta. Una
                // vez por texto — no por línea, así que dos `si` con el mismo
                // error hablan una sola vez, que es lo correcto: el creador
                // tiene UN concepto para arreglar.
                const std::string voz =
                    eclips::compararVoz(r.causa, varName, current, op, der);
                if (m_warnedCompare.find(voz) == m_warnedCompare.end())
                {
                    std::cout << voz << std::endl;
                    m_warnedCompare[voz] = true;
                }
            }

            return r.valor;
        }

        bool evaluateCondition(const EclipCmd& cmd) const
        {
            return evaluateCondition(cmd.arg1, cmd.arg2, cmd.arg3);
        }

        // ── Random events: tick por frame durante WaitingClick ──────────────
        // El caller (update) garantiza el resto de la ley: fase WaitingClick,
        // m_eventsActive, sin ancla activa y sin modal in-state encima.

        void tickRandomEvents(f64 dt)
        {
            // El orden de disparo es el de DEFINICIÓN: si dos eventos pasan
            // su roll el mismo frame, gana el que el creador escribió
            // primero. Iterar el mapa directo era un accidente alfabético.
            std::vector<RandomEventDef*> porDefinicion;
            porDefinicion.reserve(m_randomEvents.size());
            for (auto& [name, ev] : m_randomEvents)
                porDefinicion.push_back(&ev);
            // stable_sort: empates de order (saves legacy, todo en 0) caen al
            // orden del mapa (alfabético) — determinista, y es EXACTAMENTE lo
            // que esos saves hacían antes del revival.
            std::stable_sort(porDefinicion.begin(), porDefinicion.end(),
                [](const RandomEventDef* a, const RandomEventDef* b)
                { return a->order < b->order; });

            // EL ENSAYO (SYN-4b). Antes esto era `roll < chance * dt` cada
            // frame: una tasa Poisson que nadie decidió, salida de no haber
            // declarado nunca a qué ensayo pertenece la probabilidad. La ley y
            // su aritmética viven ahora en `TrialClock.h`, que un harness sí
            // alcanza; acá queda el caller.
            //
            // Se recorren TODOS los eventos aunque uno gane: el reloj del ritmo
            // corre siempre (ley 11), y pararlo en los de atrás los iría
            // desfasando de a un frame por cada victoria ajena. Lo que el
            // ganador les quita es la TIRADA de ese frame — pero NO el beat:
            // se les RETIENE y lo cobran al frame siguiente. La diferencia no
            // es cosmética. Dos eventos con el mismo `cada` cuyas fases caen en
            // el mismo frame colisionarían en CADA ciclo (comparten periodo),
            // así que saltarles el beat mataría de hambre al segundo para toda
            // la sesión, en silencio. Reteniéndolo, además, se separan solos.
            // Lo destapó el feel-test del fundador; la medición vive en
            // `harness_azar` §5c, con su control negativo.
            RandomEventDef* ganador = nullptr;

            for (RandomEventDef* evp : porDefinicion)
            {
                RandomEventDef& ev = *evp;

                f64 roll = 0.0;
                // Sólo se vuelve true si el gate EXISTE y CORRIÓ y pasó. Por
                // eso alcanza para la traza: en un frame de arbitraje el
                // predicado ni se invoca, y decir "gate ok" ahí sería inventar
                // una evaluación que no pasó.
                bool gateOk = false;
                const eclips::TrialOutcome o =
                    eclips::advanceTrial(ev.trial, dt, m_rng,
                                         /*puedeTirar*/ ganador == nullptr,
                                         /*objetivoOcupado*/ false,
                                         &roll,
                                         [&]
                                         {
                                             // EL GATE (pierna C). Esto corre
                                             // DENTRO del beat: resolver la
                                             // variable no cuesta 59 frames de
                                             // cada 60, y con `cada=5s` cuesta
                                             // 1 de cada 300.
                                             if (!ev.gate.has) return true;
                                             gateOk = evaluateCondition(
                                                 ev.gate.var, ev.gate.op,
                                                 ev.gate.val);
                                             return gateOk;
                                         });

                // LA TRAZA (contrato §6). Solo en los BEATS: un renglón por
                // frame sin beat sería ruido puro. Literales en ASCII (§1.60:
                // los fuentes son UTF-8 sin BOM y MSVC los lee como ANSI, así
                // que un guion largo saldría mojibake — y el harness no puede
                // verlo).
                if (m_azarTraza && o != eclips::TrialOutcome::NoBeat)
                {
                    // Se arma en un ostringstream PROPIO, no directo a cout, y
                    // no es cosmético: `Engine.cpp:483` deja `std::fixed` +
                    // `setprecision(1)` pegados en `std::cout` al imprimir la
                    // línea de FPS y nunca los restaura, así que TODO float que
                    // el motor imprima después sale con un decimal. La primera
                    // corrida de esta traza salió `tirada 0.2 vs 0.0` — y con un
                    // decimal, un casi-acierto y una paliza se ven igual, que es
                    // exactamente la confusión que la traza viene a matar. Un
                    // stream local nace con sus propias banderas.
                    std::ostringstream t;
                    t.precision(6);
                    t << "[azar] " << ev.name << "  beat  ";
                    // El contrato §6 muestra el gate narrado aunque la causa
                    // sea otra (`beat · gate ok · cooldown 4.2s`). Se dice sólo
                    // cuando el evento TIENE gate: los eventos sin `[si:]` no
                    // pagan una palabra por una causa que no los toca.
                    if (gateOk) t << "gate ok  ";
                    switch (o)
                    {
                    case eclips::TrialOutcome::GateVeto:
                        t << "gate cerrado (" << ev.gate.var << " "
                          << ev.gate.op << " " << ev.gate.val << ") -> no tira";
                        break;
                    case eclips::TrialOutcome::CooldownVeto:
                        t << "cooldown " << ev.trial.cooldownRemaining
                          << "s -> no tira";
                        break;
                    case eclips::TrialOutcome::ArbitrajeVeto:
                        t << "otro evento gano el frame -> beat retenido";
                        break;
                    case eclips::TrialOutcome::OcupadoVeto:
                        t << "objetivo ocupado -> no tira";
                        break;
                    case eclips::TrialOutcome::Lost:
                        t << "tirada " << roll << " vs " << ev.trial.chance
                          << " -> pierde";
                        break;
                    case eclips::TrialOutcome::Won:
                        t << "tirada " << roll << " vs " << ev.trial.chance
                          << " -> GANA -> " << ev.target;
                        break;
                    default: break;
                    }
                    std::cout << t.str() << std::endl;
                }

                if (o == eclips::TrialOutcome::Won && ganador == nullptr)
                    ganador = evp;
            }

            if (ganador == nullptr) return;

            RandomEventDef& ev = *ganador;

            // Un destino roto NO puede cerrar el juego al azar (jumpToScene con
            // escena inexistente hace cmdPop): aviso + el evento se desarma —
            // la ley de los efectos (aviso y removido; jamás crash, jamás
            // silencio).
            if (m_script.scenes.find(ev.target) == m_script.scenes.end())
            {
                std::cerr << "[VNScriptState] evento_random '" << ev.name
                    << "': la escena destino '" << ev.target
                    << "' no existe. El evento se desactiva." << std::endl;
                m_randomEvents.erase(ev.name);
                return; // el erase invalida porDefinicion
            }

            std::cout << "[VNScriptState] Random event! '"
                << ev.name << "' -> " << ev.target << std::endl;

            // El cooldown arranca al consumir la victoria, no dentro del beat:
            // así la pierna D puede vetar una tirada ganada (objetivo ocupado)
            // sin gastar el cooldown de un evento que no salió.
            eclips::trialArmCooldown(ev.trial);

            // El ancla se arma DESPUÉS del salto: jumpToScene borra anclas (el
            // salto explícito toma el volante) — pero este salto es el del
            // EVENTO, no uno de guion. Capturar antes, saltar, anclar.
            const std::string origScene = m_currentScene;
            const int origCmd = m_cmdIndex;
            jumpToScene(ev.target);
            m_preEventScene = origScene;
            m_preEventCmd = origCmd;
        }

        // ── Convertir nombre de tecla del .ecl a SDL_Keycode ──────────────────

        // ── CP0 DEL ATOMO DE TECLADO: la tabla se mudo, la red se queda ──────
        //
        // Esta funcion tenia su propia tablita (tres nombres + las letras A-Z) y
        // era la UNICA del motor que sabia traducir un nombre de tecla escrito
        // por el creador. Ahora es la cara de lectura de la tabla unica de
        // `engine/input/InputManager.h` - §7.205, "una tabla, dos direcciones":
        // la otra cara es el display de `${_tecla_*}` que llega en CP9, y dos
        // tablas para lo mismo es como se desincronizan las grafias.
        //
        // **El QTE hereda gratis el vocabulario ampliado** (§4 del contrato):
        // `tecla=F5`, `tecla=ARRIBA`, `tecla=ESC` y `tecla=1` antes caian todas
        // a la misma tecla (E) y ahora dicen lo que dicen. Lo que NO cambia es
        // la red: un nombre que la tabla no conoce sigue cayendo a `SDLK_e`,
        // porque eso es conducta de creador vigente y CP0 se declara
        // off-por-identidad. El guardian es la §1 de `harness_teclas`, que corre
        // la ley VIEJA al lado de esta y exige el mismo keycode.
        static SDL_Keycode parseKeyName(const std::string& name)
        {
            return eclips::inputKeycodeDeODefault(name, SDLK_e);
        }

        // ── Datos del script ──────────────────────────────────────────────────

        SDL_Window* m_window = nullptr;
        SDL_GLContext    m_glContext = nullptr;
        StateStack* m_stateStack = nullptr;
        FontSourceStore* m_fontSources = nullptr;
        std::uint64_t    m_generationSubscription = 0;
        // `R2`: un `cargar_tema` que llego cuando la pendiente ya estaba
        // APROBADA no se descarta -- se DIFIERE al frame siguiente. Este flag
        // es lo que separa "mi candidato espera SU corte" de "mi candidato es
        // de ESTE corte", y sin el los callbacks de commit y de rollback se lo
        // llevan puesto (guardian `19.5`).
        bool             m_temaDiferido = false;
        // `PUERTAS-QUE-NO-DIFIEREN` (2026-09-05): el gemelo del de arriba para
        // el SELECTOR de idioma, que es la otra puerta cuyo candidato es un
        // pedido de alguien -- del JUGADOR esta vez -- y publicarlo sin hornear
        // lo dejaria con los atlas del idioma anterior. Guardianes `21.x`.
        bool             m_idiomaDiferido = false;
        bool             m_applyingManifestPresentation = false;
        FontCatalog      m_candidateCatalog;
        Theme            m_candidateTheme;
        std::string      m_candidateThemePath;
        TextRenderer     m_candidateTextRenderer;
        TextRenderer     m_candidateNameRenderer;
        std::unique_ptr<LangStore> m_candidateLangStore;
        std::string      m_candidateLanguage;
        std::vector<fontdecl::Declaracion> m_candidateLocales;
        std::string      m_candidateScriptPath;
        bool             m_hasCandidateCatalog = false;
        bool             m_hasCandidateTheme = false;
        bool             m_hasCandidateBases = false;
        bool             m_hasCandidateLanguage = false;
        bool             m_hasCandidateLocales = false;

        // La tabla tecla→verbo. Propiedad de `Engine` (fork 1a del `CP1`); acá
        // sólo se lee, y por eso es `const`: la Capa 1 traduce y no despacha.
        // `CP9b`: dejo de ser `const`. Este state es el que aloja el widget
        // `control tecla.*`, y un rebind escribe la capa del jugador
        // (`setCapaJugador`) - que es el obstaculo que `L-5` midio como *"el
        // real, y no es el widget"*. Los demas consumidores siguen recibiendo
        // el `const` de `Engine::getInputManager()`; el cambio se hizo por
        // FIRMA y no con un puntero opcional, para que ningun call-site quede
        // a medias compilando igual (§7.471).
        eclips::InputManager* m_input = nullptr;
        // Idiomas CP1: pass-through obligatorio por constructor; nullptr sólo
        // cuando el manifest no declaró catálogo. CP2 lo consumirá en `t()`.
        eclips::LangStore* m_lang = nullptr;
        std::string      m_scriptPath;
        VNRole           m_role = VNRole::Game;   // §3.2 — rol por construcción
        // Ola 3 · Slice B — back-pointer al juego de abajo cuando este VN es una
        // pausa-sobre-juego (rol Menu + puntero ⇒ isPauseOverGame()). Vida segura
        // por disciplina de stack: la pausa se popea antes que el juego que cubre.
        VNScriptState* m_pausedGame = nullptr;
        // FLF-01: relación de regreso recibida y llamada pendiente propia son
        // tickets distintos; ambos comparten sólo el recibo, nunca un callback.
        eclips::system_flow::TicketRetornoPtr m_regresoAlOrigen = nullptr;
        eclips::system_flow::TicketRetornoPtr m_llamadaPendiente = nullptr;
        int              m_initialLoadSlot = -1; // -1 = nueva partida

        // ── Manifest del proyecto (puntero opcional, default nullptr) ──
        // Pasado desde MainMenuState. Se reenvía a EclipParser::parse para
        // que siembre los personajes. Patrón §1.5 (puntero como fuente única).
        const ProjectManifest* m_manifest = nullptr;

        // MetaStore: variables persistentes inter-save. Read-write (P4 escribe
        // con meta_set). Opcional (nullptr en modo test sin Engine completo).
        MetaStore* m_meta = nullptr;

        // C2 (skip-de-leído): clave de la línea actualmente mostrada. La setea
        // Dialogo/Narrador al mostrar; el Advance la marca como vista. 0 inicial
        // (nunca se marca sin que un Dialogo/Narrador la haya seteado antes).
        uint64_t m_currentLineKey = 0;

        // SeenStore: líneas ya vistas (skip-de-leído). Opcional (nullptr en modo
        // test sin Engine). P2 lo usa para mark/has.
        SeenStore* m_seen = nullptr;

        // Warn-once: nombres ya avisados por colisión meta↔save-local. Mismo
        // patrón que m_warnedVars de la interpolación.
        std::map<std::string, bool> m_warnedMetaCollision;

        // ── Hot-reload del manifest (F8, Modo Creador / dev-only) ──
        // §1.34: onManifestReloaded() corre dentro de processEvents → NO puede
        // hacer trabajo GL ni tocar punteros recién invalidados. Solo levanta
        // este flag; update() (fase GL-segura) lo consume. Mismo patrón que el
        // pop diferido del StateStack.
        // TODO(modo-creador): se compila fuera en build de export.
        bool             m_manifestReloadPending = false;

        bool             m_textureReloadPending = false;

        // ¿El script tomó el control del tema con cargar_tema? Si sí, F8 NO
        // pisa su elección con el tema_default del manifest. Distingue
        // "el script eligió X" de "el creador cambió el default del manifest"
        // — que se ven IGUAL comparando strings. El flag captura la INTENCIÓN.
        bool             m_themeOverriddenByScript = false;

        // §1.10: Si onEnter() detectó un fallo (parse del .ecl, archivo
        // ausente, etc.), m_failed queda en true y onEnter() también dispara
        // m_stateStack->requestPop(). El flag es defense-in-depth: aunque el
        // requestPop debería sacarnos del stack antes del próximo update,
        // evitamos operar sobre datos inválidos si por cualquier motivo
        // (orden de procesamiento futuro, llamada manual a update, etc.) se
        // intentara avanzar. update() y handleEvent() salen temprano si está set.
        bool             m_failed = false;

        EclipScriptData  m_script;

        // ── Estado de ejecución ───────────────────────────────────────────────

        std::string      m_currentScene;
        int              m_cmdIndex = 0;
        VNPhase          m_phase = VNPhase::Executing;

        // Diálogo actual
        std::string      m_currentSpeaker;
        std::string      m_currentText;
        std::string      m_displayText;
        std::vector<eclips::CharEffect> m_charEffects;
        int              m_typewriterChars = 0;
        f64              m_typewriterTimer = 0;
        f64              m_totalTime = 0;
        Vignette         m_vignette;           // Efecto de viñeta radial
        Theme            m_theme;                  // Tema visual activo

        // Choices actuales
        std::vector<EclipChoice> m_currentChoices;
        // Secuencias del elegir activo (DISENO_SECUENCIA SQ-1). Se reasignan
        // en cada Elegir, aunque queden vacias (R3: no heredar).
        std::vector<VNSecuencia> m_secuenciasArmadas;
        // sourceLine de renglones con miembro malo ya avisados (R11: una vez
        // por renglon en toda la vida del state).
        std::set<int>            m_secuenciaAvisadas;
        float                    m_choiceStartY = 0.0f;

        // ── Tracking para save/load ───────────────────────────────────────────

        std::string      m_currentBgPath;
        std::string      m_currentMusicPath;
        std::string      m_lastDialogue;   // Última línea mostrada (para preview)

        // ── Tinte estático del fondo (fondo ... tinte=#rrggbb) ────────────────
        // La grafía viaja al save (clave aditiva slot_N_bgtint); los floats son
        // derivados para el draw. Identidad en 1 (§7.208) — sin tinte = la
        // misma llamada al batch de siempre.
        std::string      m_bgTintHex;          // "" = sin tinte
        f32              m_bgTintR = 1.0f, m_bgTintG = 1.0f, m_bgTintB = 1.0f;

        // ── Variables del script (flags/contadores) ───────────────────────────

        std::map<std::string, std::string> m_variables;
        // Dedup de avisos de interpolacion: una vez por nombre inexistente.
        mutable std::map<std::string, bool> m_warnedVars;

        // `SI-NO-COMPARA-VARIABLES`: dedup de la voz de `si`/`mutar`, con la
        // clave puesta en el TEXTO del aviso y no en la línea. Dos `si` con el
        // mismo error hablan una vez sola, que es lo correcto — el creador
        // tiene UN concepto para arreglar, no dos renglones. `mutable` porque
        // `evaluateCondition` es `const`, igual que `m_warnedVars`.
        mutable std::map<std::string, bool> m_warnedCompare;

        // ── Recursos de render ────────────────────────────────────────────────

        AssetManager     m_assets{ 32 };                    // Caché LRU (32 texturas)
        std::vector<VNActiveChar>      m_activeChars;     // personajes en pantalla

        TextRenderer     m_textRenderer;
        TextRenderer     m_nameRenderer;
        bool             m_hasFont = false;
        bool             m_hasNameFont = false;
        FontManager      m_fontManager;   // Fuentes nombradas declaradas por 'fuente'
        // ATLAS-DE-FUENTE CP-R3 · el registro lógico de identidades. NO es un
        // dueño nuevo: el manifest y el `.ecl` transportan, el `FontSourceStore`
        // de `Engine` posee la verdad compilada y el `FontManager` de arriba
        // posee los atlas. Esto sólo decide qué identidad le toca a cada nombre.
        FontCatalog      m_fontCatalog;

        // ── Audio ─────────────────────────────────────────────────────────────

        AudioManager     m_audio;
        bool             m_hasAudio = false;

        // CP4: índice de choice hovereado el frame anterior. Detecta el FLANCO
        // de hover para tocar el sonido 1× al entrar a un botón (no cada frame).
        int              m_choiceHoverPrev = -1;
        std::string      m_enlaceHoverPrev;       // CP3: flanco del sonido de hover del enlace

        // ── Fade ──────────────────────────────────────────────────────────────

        float            m_fadeAlpha = 0.0f;
        bool             m_fadingIn = false;

        // El spec que `onEnter` resolvió para ESTA entrada. Su default es la
        // conducta previa al lote (`fade`, 0.5 s), así que aunque `onEnter` no
        // llegara a escribirlo, la rampa de `update` corre al ritmo de siempre.
        TransicionSpec   m_transicionSpec{};

        // ── Lote B · el cierre (`D-15.2`) ────────────────────────────────────
        //
        // El spec de SALIDA se resuelve en `cerrarPantalla()` y no en `onEnter`
        // a propósito: entre que la pantalla nace y que se va, el creador pudo
        // recargar el manifiesto con F8. Leerlo al irse es leer lo que el
        // creador tiene escrito AHORA.
        //
        // Su default es `ninguna`, o sea corte seco, o sea la conducta de todo
        // proyecto que existe hoy.
        TransicionSpec   m_cierreSpec{ TransicionTipo::Ninguna,
                                       kTransicionDuracionDeFabrica };
        bool             m_cerrando     = false;
        bool             m_cierrePedido = false;

        // ¿Todavia no corrio ni un update? Lo consume UNA vez, y solo sirve
        // para preguntar "¿naci tapado?" en el unico momento en que esa
        // pregunta se puede contestar (ver el bloque en update()). No es un
        // flag de ciclo de vida general: si algun dia hace falta uno, este no
        // es — este muere en el primer tick pase lo que pase.
        bool             m_primerUpdate = true;

        // ── Transiciones ─────────────────────────────────────────────────────

        VNTransitionType m_pendingTransition = VNTransitionType::None;  // Tipo pendiente
        VNTransitionType m_activeTransition = VNTransitionType::None;  // En progreso
        std::string      m_transOldBgPath;    // Ruta del fondo anterior (pinned durante transición)
        // El fondo saliente conserva SU tinte mientras dura la transición.
        std::string      m_transOldBgTintHex;
        f32              m_transOldBgTintR = 1.0f, m_transOldBgTintG = 1.0f,
                         m_transOldBgTintB = 1.0f;
        float            m_transProgress = 0.0f;  // 0 → 1
        static constexpr float TRANS_SPEED = 1.5f; // Segundos para completar
        static constexpr float CHAR_FADE_SPEED = 3.0f; // ~0.33s para fade in/out de personajes
        static constexpr float CHAR_SLIDE_SPEED = 4000.0f; // pixels/s (~0.48s full screen)

        // ── Toast de notificación ─────────────────────────────────────────────

        std::string      m_toastMsg;
        float            m_toastTimer = 0.0f;  // segundos restantes (0 = oculto)

        // ── Historial / Rollback de texto ─────────────────────────────────────

        static constexpr int HISTORY_MAX = 100;  // Entradas máximas (~20KB)
        //std::deque<VNHistoryEntry> m_history;     // Buffer circular
        bool             m_showingHistory = false;
        float            m_historyScroll = 0.0f; // Offset de scroll (en pixels virtuales)
        float            m_historyScrollTarget = 0.0f; // Para scroll suave

        // ── Auto-mode y Skip ──────────────────────────────────────────────────

        bool             m_autoMode = false;
        bool             m_skipMode = false;
        bool             m_cinematicaActive = false; // CP1: dentro de 'cinematica' no-saltable → sella skip

        // ── EL SELLO DE ACCIONES (`DISENO_DESHABILITAR_ACCIONES.md`) ──────────
        //
        // Un bit por verbo del jugador que el guion apagó. **Nace en 0 y ésa es
        // la ley 1 (`D-40`):** sin un solo `deshabilitar` escrito, la conducta
        // del motor es byte-idéntica.
        //
        // `D-51` — NO VIAJA AL SAVE. Cero claves nuevas en el keyspace, cero
        // bump. Vive acá y no en el snapshot a propósito, con el precedente
        // exacto al lado: `m_cinematicaActive = false` al cargar (`:8893`).
        //
        // `D-43` — vida útil: se apaga en los TRES cortes (escena nueva,
        // `cargar_script`, cargar save), que es la conducta de `cinematica`
        // copiada entera. Los tres llaman a `resetSelloAcciones()`.
        eclips::VNActionMask m_accionesSelladas = 0u;
        // (El typewriter/skip/auto vivos están en VNDialogBox — los dos inquilinos
        //  parkeados del update() y sus timers murieron con el micro-lote de higiene.)

        // ── Quick-Time Event ──────────────────────────────────────────────────
        // (Los timers de choices/QTE viven en VNDialogBox — los gemelos
        // vestigiales de este lado murieron con el revival de evento_random.)

        // `m_qteKeycode` y `m_qteKeyName` vivían acá y eran WRITE-ONLY: se
        // asignaban en el `case Qte` y no los leía nadie. La tecla la guarda
        // `VNDialogBox` (`m_qteKeycode`), que es quien la compara, y el nombre
        // lo guarda para dibujarlo. Retirados en `qte-sin-puerta`.
        std::string      m_qteSuccess;
        std::string      m_qteFail;

        // ── Ventana ───────────────────────────────────────────────────────────

        int              m_winW = 1280;
        int              m_winH = 720;

        // ── Capas Z-Order (Bloque 1) ────────────────────────────────────────

        SceneLayerManager m_sceneLayers;
        int m_autoTextId = 0;   // Contador para ids de texto sin id explicito
        std::map<std::string, std::string> m_layerPaths; // nombre → ruta (para unpin)
        std::map<std::string, std::uint64_t> m_layerPathOwners;
        std::map<std::string, std::string> m_layerHoverPaths; // nombre → ruta del PNG-hover del enlace-imagen (unpin)
        std::map<std::string, std::uint64_t> m_layerHoverOwners;

        // ── Hotspots (Bloque 1) ─────────────────────────────────────────────

        HotspotManager   m_hotspots;
        std::vector<EclipChoice> m_clickMappings; // esperar_click: nombre → escena
        bool             m_debugHotspots = false; // F3 para toggle debug

        // ── Controles de settings (Capa 2, comando 'control') ───────────────
        // Cada 'control <setting>' registra uno: setting Tier-A bindeado +
        // modelo de valor (min/max/paso) + geometría. CP1 solo registra y lee
        // el valor inicial; render (CP2), arrastre+escritura (CP3), discreto+
        // lock (CP4) llegan después.
        struct BoundControl {
            std::uint64_t scopeOwner = 0;                   // generación dueña
            const CtrlDesc* desc = nullptr;                  // fila Tier-A resuelta (key/kind/modelo)
            CtrlRender render = CtrlRender::Bar;             // barra o pill (resuelto en bind, D2)
            CtrlRender requestedRender = CtrlRender::Bar;    // preferencia del creador, estable en rebind/refresh
            bool numericPresentationWarned = false;          // una voz por dominio numérico y control
            TextSpeedDomain numericPresentationWarningDomain = textSpeedLegacyDomain();
            std::string settingId;                           // id literal del creador (log/debug)
            float x = 0.0f, y = 0.0f, width = 400.0f;
            float vmin = 0.0f, vmax = 100.0f, vstep = 1.0f;  // modelo resuelto (desc u override)
            // `CP6`: el RECIBO de si `vstep` lo escribió el creador (`paso=`) o
            // salió del catálogo. `vstep` hace dos trabajos —para el mouse es la
            // PRECISIÓN del drag, para una tecla es el INCREMENTO— y sin este
            // bool el ajuste por teclado no puede distinguir "el default no
            // servía" de "el creador pidió esto". Es `{pausa=0}`/`RO-3`: un
            // valor escrito no es lo mismo que uno que no se escribió.
            bool  pasoExplicito = false;
            // `CP5` (fork 1b): EL CATALOGO DEL DESCRIPTOR. Un `CtrlKind::Enum`
            // lo recibe en el bind (`llenarOpciones`) y el widget lo lee sin
            // preguntar cuál setting es. Vacío = no es Enum, o el proveedor no
            // tenía nada que ofrecer. El `valor` de cada opción es su POSICIÓN
            // —es lo que las dos filas Enum ya son, un índice—, así que no se
            // guarda: guardarlo sería un campo sin caller (§7.118).
            std::vector<std::string> opciones;
            std::vector<TextSpeedCatalogEntry> textSpeedOptions;
            TextSpeedDomain textSpeedDomain = textSpeedLegacyDomain();
            std::string styleName;                           // estilo= (defaults por ahora)
            const ControlStyleManifest* controlStyle = nullptr;
            float value = 0.0f;                              // valor actual leído del setting
        };

        std::vector<BoundControl> m_controls;
        int                       m_draggingControl = -1;  // índice en arrastre, -1 = ninguno
        int                       m_hoveredControl = -1;   // hover propio del puntero
        std::vector<std::string>  m_pinnedControlStyles;

        // ── Ranura (Ola 2 · slot-widget Tier-A): primitiva hermana del control.
        // No comparte data NI input con BoundControl (§7.162 al límite: aquí ni el
        // modelo coincide) → familia paralela, m_controls intacto. La meta se
        // cachea al registrar (cmdRanura→readSlot): estática mientras la pantalla
        // vive; cuando borrar (CP futuro) mute un slot, se refresca ESE cache.
        struct SlotWidget {
            std::uint64_t scopeOwner = 0;                   // generación dueña
            int slot = 0;                       // índice 0-based interno (creador escribe 1-based)
            std::string action;                 // "cargar"/"guardar"/"borrar" (C1: solo cargar en CP3)
            float x = 0.0f, y = 0.0f;
            float width = 360.0f, height = 72.0f;
            std::string styleName;              // estilo= (defaults por ahora)
            // ── meta cacheada (readSlot al registrar) ──
            bool occupied = false;
            std::string scene, preview, timestamp;
            long long timeRaw = 0;              // epoch crudo (CP2); 0 = save viejo
        };
        // ── `CP9b`: EL WIDGET DE TECLA - la tercera familia paralela ─────────
        //
        // `control tecla.<dominio>.<accion>` muestra la tecla de una accion y la
        // deja capturar. **No es un `BoundControl`** (`L-5`): aquel es un float
        // con `min/max/paso` atado a un `CtrlDesc*` del catalogo Tier-A, y esto
        // es una lista de strings sin modelo numerico. Es la misma frase que
        // `ranura` -*"aqui ni el modelo coincide"*- y la misma solucion.
        //
        // **No cachea la tecla**, al reves que `SlotWidget`: el valor lo contesta
        // `m_input->teclasDe()` en el render, porque un rebind lo cambia en el
        // acto y un cache seria un segundo sitio que puede quedar viejo.
        struct TeclaWidget {
            std::uint64_t scopeOwner = 0;                   // generación dueña
            std::string dominio;            // canonico: "vn" / "juego"
            std::string verbo;              // canonico
            bool        enIngles = false;   // la grafia que el CREADOR escribio
            float x = 0.0f, y = 0.0f, width = 240.0f;
            std::string styleName;
        };
        std::vector<TeclaWidget> m_teclaWidgets;
        // Hover del mouse y foco del teclado CONVERGEN en este campo, que es el
        // §9 al pie de la letra. El `control` necesito dos (`m_focoControl`
        // aparte) porque su hit-test corre a demanda en el click; este widget
        // nace con la nocion de foco puesta, asi que no repite ese gasto.
        int  m_focoTecla     = -1;
        // La fila en modo captura, -1 = ninguna. Es un modal in-state, hermano
        // de `m_confirmActive`: mientras vive, se traga el input crudo.
        int  m_capturaTecla  = -1;
        // El evento que ABRE la captura no es el que se captura. Sin esto,
        // activar la fila con ESPACIO -que es `avanzar`- te ligaria ESPACIO a lo
        // que estabas remapeando, y el click te ligaria `CLIC`.
        bool m_capturaArmada = false;

        std::vector<SlotWidget> m_slotWidgets;
        // 6.1 CP1: thumbnails de slots, propios del menú (fuera del LRU global).
        // Clave = slot 0-based. RAII: las Texture se liberan por dtor / clear().
        std::map<int, Texture> m_slotThumbs;
        std::map<int, std::uint64_t> m_slotThumbOwners;
        // Juice-hover: indice del slot bajo el cursor (-1 = ninguno). Push por
        // MOUSEMOTION (hitTestSlotWidget); reset en los 2 clear() de m_slotWidgets.
        // Calco de m_choiceHover (VNDialogBox), aqui por evento (no cacheamos mouse).
        int m_hoveredSlot = -1;

        // ── `CP6`: EL ANILLO UNIFICADO ───────────────────────────────────────
        // `m_focoPlano` es el indice sobre la lista que cruza familias
        // (`FocusRing.h` ley 4); `m_focoDelPuntero` es la ley 2 -quien manda-,
        // subida aca desde la caja porque ahora gobierna cuatro familias y no
        // una. `m_focoControl` es la unica PROYECCION que no existia: los
        // choices tienen `m_choiceHover`, las ranuras `m_hoveredSlot` y los
        // enlaces `elem.hovered`, pero el `control` no tenia noción de hover
        // -su hit-test corre a demanda en el click-, asi que un control focado
        // no se veria. Ver `renderControls`.
        int  m_focoPlano      = -1;
        bool m_focoDelPuntero = true;
        int  m_focoControl    = -1;

        // ── `CP7`: EL PUNTERO TIENE DUEÑO (`PointerPresence.h`) ──────────────
        // Antes, la POSICIÓN se poleaba con `SDL_GetMouseState` en CINCO sitios
        // vivos de este archivo y se empujaba en dos más: siete consumidores y
        // ningún dueño. `CP5`/`CP6` le dieron dueño al ÍNDICE (§7.486) y la
        // posición quedó afuera — que es lo que hace imposible el no-persist
        // táctil, porque un `clear` se re-deriva al frame siguiente.
        //
        // `m_punteroWin` son las coordenadas CRUDAS de ventana, y existen por
        // un solo consumidor: `updateInteractiveHover` pide las normalizadas
        // (`x / winW`) para compensar el parallax, y de una virtual no se puede
        // volver sin invertir `mouseToVirtual`. La ley pura no las lleva porque
        // no son suyas: son un detalle del viewport, que es del state.
        eclips::Puntero m_puntero;
        int m_punteroWinX = 0;
        int m_punteroWinY = 0;

        // ── Confirm modal de borrado (Ola 2 · B2): overlay in-state que la
        // accion destructiva 'accion=borrar' levanta. NO es un verbo autorable
        // (su unico caller honesto es borrar → exponer 'confirmar' seria §7.118);
        // es mecanismo HORNEADO en la accion. Look espejo del VNSaveMenu C++
        // renderConfirmDelete: dim + panel con borde rojo, default-No, ESC=cancela.
        // Borrar-especifico (§7.6): el titulo lleva el N, "Si" llamara deleteSlot
        // directo (sin sistema de callbacks — esa generalidad la pediria un 2º
        // caller). CP1 = solo estado + render; input en CP2; borrado+refresco en CP3.
        bool m_confirmActive = false;        // modal levantado
        int  m_confirmSlot = -1;           // slot 0-based a borrar (-1 = ninguno)
        int  m_confirmCursor = 1;            // 0=Si, 1=No. Default-No (espejo C++ :1268)

        // R4: display se aplica temporalmente y solo toca Settings tras "Si".
        // Es estado separado del confirm de borrado: comparte gramática visual,
        // no acción ni ciclo de vida.
        display_change::Transaction m_displayChange;
        int m_displayConfirmCursor = 1;       // 0=Si, 1=No; siempre default-No

        // Catálogo del CtrlKind::Enum 'resolucion': pares W×H derivados del
        // lienzo, filtrados por monitor. Se llena en buildResOptions (CP1).
        // `CP5` (fork 1b): la construcción del catálogo CON su etiqueta bajó a
        // `WindowResolution.h` para que un harness la pueda custodiar
        // (`harness_resolucion` §5, el guardián de no-op que era la condición
        // de la firma). Este vector deja de ser lo que el WIDGET lee —eso pasó
        // a `bc.opciones`— y se queda con lo único que el SETTING necesita: el
        // W×H que `read`/`writeControlValue` usan para el match por distancia,
        // que es distinto del índice exacto con el que resuelve `idioma`.
        std::vector<window_resolution::Opcion> m_resOptions;

        // ── Parallax (Bloque 2) ────────────────────────────────────────────

        bool             m_parallaxEnabled = true; // parallax si/no
        bool             m_speakerHighlight = false; // resaltar_hablante si/no

        // ── Random Events (Bloque 2) ────────────────────────────────────────

        std::map<std::string, RandomEventDef> m_randomEvents;
        int              m_eventOrderNext = 0;     // contador de registro (orden de disparo)
        bool             m_eventsActive = false;   // iniciar_eventos / detener_eventos
        bool             m_azarTraza = false;      // azar_traza — la voz del silencio (§6)
        std::mt19937     m_rng{ std::random_device{}() };

        // ── El sorteo del bloque `azar:` (pierna B, ley 7) ──────────────────
        // JAMÁS se serializa: es la mitad "el sorteo no" de la ley que separa
        // reloj de sorteo. *Load = barajado fresco*, observado en DDLC real y
        // ya escrito por `EffectInstance.rngState`. Clave: escena + línea del
        // bloque. Se vacía en `onExit`, `loadFromSlot` y `loadNewScript`.
        std::map<std::string, eclips::DrawBag> m_azarBolsas;

        // ── El pool abierto del bloque `palabras` (`CP2a`) ──────────────────
        // Estado de CORRIDA, como la bolsa de arriba y por la misma razón: la
        // página se re-sortea al re-entrar al comando (`F4`), y cargar un slot
        // **re-ejecuta el bloque** (`loadFromSlot` vuelve a `Executing` sobre el
        // `m_cmdIndex` guardado), así que un pool guardado se rearma solo con
        // barajado fresco. Eso es `A10` del contrato, y sale gratis.
        //
        // `m_palabrasIds` son los ids de las capas sintetizadas, y son lo único
        // que distingue *"me clickearon una palabra del pool"* de *"me
        // clickearon otro `enlace` de la escena"*.
        bool                     m_palabrasActivas = false;
        std::vector<std::string> m_palabrasIds;

        // Ancla de retorno del random event: (escena, cmd) del comando de
        // espera vivo al disparar. Vacía = sin evento en curso. La consume
        // returnFromEvent; la borra cualquier salto de guion (jumpToScene).
        // POSICIONAL como slot_N_cmd (§7.257): el día del ancla semántica,
        // adopta la etiqueta en el mismo movimiento.
        std::string      m_preEventScene;
        int              m_preEventCmd = 0;

        // ── Lua scripting (Fase 3) ───────────────────────────────────────────

        LuaManager       m_lua;

        // ── Lua: acciones diferidas ──────────────────────────────────────────

        std::string      m_luaPendingGoto;
        std::string      m_luaPendingLoadScript;
        std::string      m_luaPendingLoadScriptScene;

        // ── FBO para composición de personajes compuestos ─────────────────

        bool             m_hasFBO = false;
        GLuint           m_compFBO = 0;
        // Mascara de silueta (unidad de textura 1). false = el path de efectos
        // pinta como siempre; una pasada con `mascara` degrada al rectangulo.
        bool             m_hasMultiTex = false;
        // 6.1 CP0: FBO + textura para el snapshot del thumbnail (espejo del de
        // composición). Lazy-gen en captureThumbnail; liberado en el cleanup.
        GLuint           m_captureFBO = 0;
        GLuint           m_captureTex = 0;
        int              m_captureTexW = 0;
        int              m_captureTexH = 0;

        // E-CP3: textura de ruido de `estatica` (256x256 gris, generada sin
        // asset). Lazy-gen en ensureFxRuidoTex; liberada en onExit.
        GLuint           m_fxRuidoTex = 0;

        // ── Animaciones / Tweens ──────────────────────────────────────────
        // C-CP2: apilables. Un alias corre VARIAS animaciones a la vez (flotar
        // + pulsar); el render las compone por el álgebra (§7.208).
        std::map<std::string, std::vector<CharAnim>> m_charAnims;  // alias → stack

        // ── E-CP0: efectos de render. Especie APARTE de las animaciones.
        // Una CharAnim mueve el elemento (dx/dy/escala/alpha); un efecto cambia
        // COMO SE DIBUJA (la lista de pasadas). Se indexa por BLANCO, no por
        // alias de personaje: "monika" hoy; "fondo" y las capas en E-CP2, sin
        // cambiar el tipo.
        std::map<std::string, std::vector<EffectInstance>> m_charFx;  // blanco → stack fx

        // ── E-CP6: efectos Lua ──────────────────────────────────────────────
        // El registro VIVO (nombre → transformador). El NOMBRE lo declara el
        // .ecl (`efecto_lua`, viaja en m_script.luaEffects y carga al boot);
        // la FUNCIÓN la aporta eclips.register_effect. EffectInstance lleva
        // solo el nombre — EffectSystem.h sigue puro (el parser lo incluye).
        std::map<std::string, sol::protected_function> m_luaFx;
        // El puente C++↔Lua del runner REUSA tablas entre frames (disciplina
        // GC del contrato §5): ctx + lista + pool de pasadas, creadas lazy.
        sol::table m_luaFxCtx;
        sol::table m_luaFxLista;
        std::vector<sol::table> m_luaFxPool;
        bool m_luaFxTablasListas = false;

        // ── C-CP5: tracks nombrados. Especie APARTE (fork 3a): un preset es
        // una receta multi-canal horneada (CharAnim); un track es UN canal
        // crudo resuelto. Comparten el álgebra (§7.208) — el render compone
        // los dos stacks — pero no el struct ni el save (P; vs T;). `origen`
        // = la receta que lo parió (identidad de grupo para detener).
        std::map<std::string, std::vector<TrackInstance>> m_charTracks;  // alias → stack
        std::vector<TrackInstance> m_screenTracks;   // pantalla: mismo modelo

        std::vector<CharAnim> m_screenAnims;   // pantalla: mismo modelo, vector
        // aparte (F1: `pantalla` es palabra
        // reservada del verbo; no comparte
        // keyspace con los alias)
        bool     m_waitingForAnim = false;             // esperar_animacion activo
        float    m_waitTimer = 0.0f;                   // esperar N: segundos restantes
        bool     m_waitingForGameplay = false;         // IrAPasillo / IniciarBatalla activo

        // Tier 4C.6 Fase B: push diferido de Gameplay2D al cargar slot híbrido.
        // No hacemos push dentro del callback del VNSaveMenu porque el menú
        // aún no se ha pop-eado y quedaría encima del Gameplay2D.
        bool                    m_pendingGameplayLaunch = false;
        VNSaveMenu::SlotData    m_pendingGameplayData;

        // ── Entrada de texto (comando 'entrada', B2) ───────────────────────
        // m_inputVar    = variable destino (cmd.arg1)
        // m_inputPrompt = prompt ya interpolado (cmd.arg2 con ${var})
        // m_inputBuffer = lo que el jugador lleva tecleado (UTF-8 crudo)
        // Activos solo durante VNPhase::WaitingTextInput. Render (P3),
        // escritura (P4), max/default (P5).
        std::string  m_inputBuffer;
        std::string  m_inputVar;
        std::string  m_inputPrompt;
        int          m_inputMaxLen = 0;   // 0 = sin límite (en codepoints)
        std::string  m_inputDefault;      // fallback si confirma vacío o cancela
        bool         m_inputRequired = false;  // 'requerido': no se confirma vacío
        bool         m_inputOkHover = false;   // CP3: mouse sobre el OK (hover)

        // ── Modalidad-aviso (enlace requerido) ──
        // true mientras un 'enlace'-OK marcado 'requerido' esté vivo: la caja-aviso
        // es modal. Se prende al aplicar el enlace requerido y se apaga al
        // despacharlo o al cambiar de escena. Lo leen las cerraduras.
        bool         m_modalRequired = false;
        std::uint64_t m_modalScopeOwner = 0;
        bool isModalRequired() const { return m_modalRequired; }

        // DDLC-AMBITO-01: nombre -> generación viva. El owner 0 es host.
        std::map<std::string, std::uint64_t> m_scopeGenerations;
        std::uint64_t m_nextScopeGeneration = 1;
        std::uint64_t m_activeScopeOwner = 0;

        // ── LA CONSULTA DEL SELLO, Y SUS CUATRO PUERTAS ──────────────────────
        //
        // `D-45`: esto es UN término de la conjunción, no la conjunción entera.
        // Los call-sites lo ponen en `&&` con lo que ya guardaban (rol, modal,
        // cinemática) y esas guardas NO se tocaron — por eso un `habilitar`
        // nunca otorga un permiso que otra capa negaba.
        //
        // **Hay CUATRO puertas de entrada, y el contrato nombraba dos.** Medido:
        //   A · los atajos crudos `SDLK_*` de `handleEvent` (K/L/F5/F9/ESC);
        //   B · `VNDialogBox::handleAction` — los seis botones del chrome NO
        //       llegan traducidos, se resuelven por hit-test adentro de la
        //       caja, así que el sello se le EMPUJA (igual que `setSkipAllowed`
        //       ya hacía) en vez de consultarse desde acá;
        //   C · `dispatchTarget` — los verbos shell que un `enlace`, un hotspot
        //       o una opción de `elegir` disparan. **Ésta es la que el contrato
        //       no vio**, y es donde `D-44` chocó con el código: no existe un
        //       comando `guardar` en el flujo del guion — `guardar` sólo es
        //       destino de algo que el JUGADOR clickea. Firmado por el fundador
        //       (fork 1B): esa puerta se sella, y habla.
        //   D · los **callbacks del `PauseMenuState` C++** (`openPauseMenu`, más
        //       abajo). **No tienen input**: son lambdas que otro state invoca,
        //       así que ningún razonamiento sobre teclas o clicks llega a ellas
        //       — las encontró el grep por el EFECTO (§7.462) sobre el lote ya
        //       verde. **Invisible bajo el grueso** (que sella `menu`, y entonces
        //       esa pantalla ni se abre) y alcanzable sólo con la forma FINA:
        //       *«podés pausar, pero no guardar»*.
        //
        // *(El skip es aparte y no cuenta como puerta: se POLEA en `update()`, y
        //  su permiso ya era una variable — el sello es un `&&` más.)*
        //
        // Un sello escrito en una sola de las cuatro cubre una fracción, y la que
        // quede afuera **se ve idéntica en pantalla** porque el botón ya estaba
        // oculto. Ése es el modo de falla mudo de este lote.
        bool accionPermitida(eclips::VNAction a) const
        {
            return eclips::vnActionAllowed(m_accionesSelladas, a);
        }

        // Azúcar con VOZ para los call-sites: contesta "¿lo veta el sello?" y,
        // si sí, lo dice. La voz importa tanto como el veto — sin ella, el
        // creador que se pregunta por qué su F5 no abre no tiene dónde mirar,
        // y es exactamente la queja que `D-44` temía por el otro lado.
        bool selloVeta(eclips::VNAction a) const
        {
            if (accionPermitida(a)) return false;
            std::cerr << eclips::vnVozSelloVeta(eclips::vnNombreDeAccion(a)) << "\n";
            return true;
        }

        // `CP1b`: los cinco estados que la ley de atajos necesita, en un struct
        // con los campos NOMBRADOS. Sueltos en la firma serían cuatro `bool`
        // seguidos, y dos intercambiados **compilan en silencio** — el harness
        // los probaría en el orden equivocado con la misma cara de verde.
        //
        // `puedePopear` es el único que no es del VN: es del `StateStack`, y por
        // eso se lee acá y no adentro de la ley (que es pura y no lo conoce).
        eclips::VNAtajoContexto atajoContexto() const
        {
            eclips::VNAtajoContexto c;
            c.rolMenu          = (m_role == VNRole::Menu);
            c.puedePopear      = (m_stateStack->size() > 1);
            // ── AQUÍ VIVE EL LOTE, y es esta línea entera ────────────────
            //
            // Decía `isModalRequired()` a secas, y ese flag tiene DOS dueños
            // desde `CP2a`: `modalidad-aviso` (que sí quiere sellar save/load,
            // porque el aviso es la única salida) y el pool `palabras`, que lo
            // prende en [:3839] para sellar el avance plano y para que le
            // llegue el anillo de foco — **nunca para prohibir guardar**.
            //
            // Medido antes de tocar esto: durante un pool, `K`/`L`/`F5` se
            // sellaban y hablaban con la voz de `modalidad-aviso`, nombrando
            // una feature que no estaba en pantalla. Y el sello era además
            // **incoherente**: `ESC`→pausa→Guardar llega al mismo lugar y
            // nunca estuvo sellado, así que el motor no prohibía guardar —
            // prohibía UN CAMINO de guardar.
            //
            // Los otros seis lectores de `m_modalRequired` NO se tocan: el
            // pool los necesita a los dos que le importan (el sello del
            // avance en [:1876] y el anillo de foco en [:7695]).
            c.avisoRequerido   = isModalRequired() && !m_palabrasActivas;
            c.cinematicaActive = m_cinematicaActive;
            c.sellada          = m_accionesSelladas;
            return c;
        }

        // `D-43`: los TRES cortes. Se llama desde escena nueva, `cargar_script`
        // y cargar save — la conducta de `cinematica`, copiada entera. Es un
        // punto único a propósito: la asimetría que este contrato midió
        // (`modalidad-aviso` se limpia en UNO de los tres, ficha
        // `MODALIDAD-SIN-CORTE`) nació de tener el reset escrito tres veces.
        void resetSelloAcciones()
        {
            m_accionesSelladas = 0u;
        }

#ifdef ECLIPS_TEST_SEAM
    public:
        // Seam de aceptacion: la sonda llama los mismos bind/write/read y los
        // mismos caminos de mouse/foco que usa el state vivo. No se compila en
        // el target normal; existe solo para que el arnes no replique la ley
        // privada de un widget GL-heavy en una tabla paralela.
        struct TextSpeedBindProbe
        {
            int optionCount = 0;
            bool bound = false;
            bool renderBar = false;
            bool domainFallback = false;
            bool bindDidNotWrite = false;
            bool temporalOption = false;
            bool label6000 = false;
            bool value6000 = false;
            float boundIndex = -1.0f;
        };

        struct TextSpeedInputProbe
        {
            float mouseIndex = -1.0f;
            float keyboardIndex = -1.0f;
            float gamepadIndex = -1.0f;
        };

        static void bindTextSpeedControlForProbe(VNScriptState& state)
        {
            state.m_phase = VNPhase::WaitingInterface;
            Settings::setTextSpeed(*TextSpeed::tryCps(6000.0f));

            EclipCmd cmd;
            cmd.type = EclipCmdType::Control;
            cmd.arg1 = "velocidad_texto";
            cmd.layerX = 100.0f;
            cmd.layerY = 200.0f;
            cmd.controlWidth = 400.0f;
            cmd.controlMin = 0.0f;
            cmd.controlMax = 200.0f;
            cmd.controlStep = 5.0f;
            cmd.controlType = "toggle";
            state.cmdControl(cmd);
        }

        static TextSpeedBindProbe probeTextSpeedBind()
        {
            TextSpeedBindProbe result;
            VNScriptState state(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "");
            Settings::setTextSpeed(*TextSpeed::tryCps(6000.0f));
            const TextSpeed beforeBind = Settings::textSpeedOverride().value();
            bindTextSpeedControlForProbe(state);

            if (state.m_controls.empty()) return result;
            BoundControl& control = state.m_controls.front();
            result.bound = control.desc && control.desc->key == CtrlSetting::TextSpeed;
            result.renderBar = control.render == CtrlRender::Bar;
            result.optionCount = static_cast<int>(control.textSpeedOptions.size());
            result.domainFallback = control.vmin == 0.0f &&
                control.vstep == 1.0f &&
                control.vmax == static_cast<float>(result.optionCount - 1);
            result.bindDidNotWrite = Settings::textSpeedOverride().has_value() &&
                Settings::textSpeedOverride().value() == beforeBind;
            result.boundIndex = control.value;
            const int index = static_cast<int>(control.value + 0.5f);
            if (index >= 0 && index < result.optionCount)
            {
                const TextSpeedCatalogEntry& option =
                    control.textSpeedOptions[static_cast<size_t>(index)];
                result.temporalOption = option.temporal;
                result.label6000 = option.labelEs == "6000 cps" &&
                    option.labelEn == "6000 cps" &&
                    control.opciones[static_cast<size_t>(index)] == "6000 cps";
                result.value6000 = option.value.isCps() &&
                    option.value.cps() == 6000.0f;
            }
            return result;
        }

        static TextSpeedInputProbe probeTextSpeedInputs()
        {
            TextSpeedInputProbe result;
            VNScriptState state(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "");
            Settings::setTextSpeed(*TextSpeed::tryCps(30.0f));
            bindTextSpeedControlForProbe(state);
            if (state.m_controls.empty()) return result;
            BoundControl& control = state.m_controls.front();

            // Mouse: el drag llega a la misma opcion 2 que las dos rutas de foco.
            Settings::setTextSpeed(*TextSpeed::tryCps(30.0f));
            state.applyControlDrag(0, control.x + control.width * 0.5f);
            result.mouseIndex = state.readControlValue(CtrlSetting::TextSpeed);

            // Teclado: izquierda/derecha del control focado, no una ley paralela.
            Settings::setTextSpeed(*TextSpeed::tryCps(30.0f));
            state.m_phase = VNPhase::WaitingInterface;
            state.m_focoPlano = 0;
            state.m_focoDelPuntero = false;
            state.ajustarControlFocado(0, +1);
            result.keyboardIndex = state.readControlValue(CtrlSetting::TextSpeed);

            // Gamepad: el caller de plataforma entrega el mismo InputAction de
            // foco al state; se ejercita el dispatcher real de foco.
            Settings::setTextSpeed(*TextSpeed::tryCps(30.0f));
            state.m_focoPlano = 0;
            state.m_focoDelPuntero = false;
            state.manejarFoco(InputAction::FocusRight);
            result.gamepadIndex = state.readControlValue(CtrlSetting::TextSpeed);
            return result;
        }

        // Seam de la ruta de musica: corre `musica` (y, si se pide, `parar_musica`)
        // por el mismo executeNextCmd del state vivo, sin audio ni ventana, y
        // devuelve lo que el save escribiria como musica actual.
        static std::string probeRutaMusica(bool conParar)
        {
            VNScriptState state(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "");
            EclipScene escena;
            escena.name = "a";
            EclipCmd musica;
            musica.type = EclipCmdType::Musica;
            musica.arg1 = "assets/musica/intro_b.ogg";
            escena.cmds.push_back(musica);
            if (conParar)
            {
                EclipCmd parar;
                parar.type = EclipCmdType::PararMusica;
                escena.cmds.push_back(parar);
            }
            state.m_script.scenes["a"] = escena;
            state.m_currentScene = "a";
            state.m_cmdIndex = 0;
            for (size_t i = 0; i < escena.cmds.size(); ++i) state.executeNextCmd();
            return state.m_currentMusicPath;
        }

        struct TextSpeedDomainBindProbe
        {
            bool bound = false;
            bool renderBar = false;
            bool numericDomain = false;
            bool bindDidNotWrite = false;
            float min = 0.0f;
            float max = 0.0f;
            float step = 0.0f;
            float value = 0.0f;
        };

        static void bindTextSpeedDomainControlForProbe(VNScriptState& state)
        {
            Theme theme = Theme::defaults();
            theme.textSpeed = *TextSpeed::tryCps(70.0f);
            theme.textSpeedDomain = *tryNumericTextSpeedDomain(
                1.0f, 150.0f, 1.0f, true);
            state.m_theme = theme;
            state.m_phase = VNPhase::WaitingInterface;
            EclipCmd cmd;
            cmd.type = EclipCmdType::Control;
            cmd.arg1 = "velocidad_texto";
            cmd.layerX = 100.0f;
            cmd.layerY = 200.0f;
            cmd.controlWidth = 400.0f;
            cmd.controlType = "barra";
            state.cmdControl(cmd);
        }

        static TextSpeedDomainBindProbe probeTextSpeedDomainBind()
        {
            TextSpeedDomainBindProbe result;
            VNScriptState state(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "");
            Settings::setTextSpeed(*TextSpeed::tryCps(90.0f));
            const TextSpeed beforeBind = Settings::textSpeedOverride().value();
            bindTextSpeedDomainControlForProbe(state);
            if (state.m_controls.empty()) return result;
            const BoundControl& control = state.m_controls.front();
            result.bound = control.desc && control.desc->key == CtrlSetting::TextSpeed;
            result.renderBar = control.render == CtrlRender::Bar;
            result.numericDomain = control.textSpeedDomain.isNumeric();
            result.min = control.vmin;
            result.max = control.vmax;
            result.step = control.vstep;
            result.value = control.value;
            result.bindDidNotWrite = Settings::textSpeedOverride().has_value() &&
                Settings::textSpeedOverride().value() == beforeBind;
            return result;
        }

        static TextSpeedInputProbe probeTextSpeedDomainInputs()
        {
            TextSpeedInputProbe result;
            VNScriptState state(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "");
            bindTextSpeedDomainControlForProbe(state);
            if (state.m_controls.empty()) return result;
            BoundControl& control = state.m_controls.front();
            const float target = 71.0f;
            const float t = (target - control.vmin) /
                (control.vmax - control.vmin);

            Settings::setTextSpeed(*TextSpeed::tryCps(70.0f));
            state.applyControlDrag(0, control.x + control.width * t);
            result.mouseIndex = state.readControlValue(CtrlSetting::TextSpeed);

            Settings::setTextSpeed(*TextSpeed::tryCps(70.0f));
            state.m_phase = VNPhase::WaitingInterface;
            state.m_focoPlano = 0;
            state.m_focoDelPuntero = false;
            state.ajustarControlFocado(0, +1);
            result.keyboardIndex = state.readControlValue(CtrlSetting::TextSpeed);

            Settings::setTextSpeed(*TextSpeed::tryCps(70.0f));
            state.m_focoPlano = 0;
            state.m_focoDelPuntero = false;
            state.manejarFoco(InputAction::FocusRight);
            result.gamepadIndex = state.readControlValue(CtrlSetting::TextSpeed);
            return result;
        }

        struct TextSpeedPresentationProbe
        {
            int numericBarRender = -1;
            int numericArrowsRender = -1;
            int numericBoxesRender = -1;
            int numericToggleRender = -1;
            int numericArrowsOptions = -1;
            int numericBoxesOptions = -1;
            int legacyArrowsRender = -1;
            int legacyBoxesRender = -1;
            int legacyToggleRender = -1;
            int numericAfterLegacyRender = -1;
            int legacyAfterNumericRender = -1;
        };

        static TextSpeedPresentationProbe probeTextSpeedPresentations()
        {
            TextSpeedPresentationProbe result;
            VNScriptState state(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "");
            state.m_phase = VNPhase::WaitingInterface;

            const auto numericDomain = *tryNumericTextSpeedDomain(
                20.0f, 200.0f, 1.0f, false);
            Theme numericTheme = Theme::defaults();
            numericTheme.textSpeed = *TextSpeed::tryCps(70.0f);
            numericTheme.textSpeedDomain = numericDomain;
            state.m_theme = numericTheme;

            auto bind = [&](const char* type, int& render, int* options = nullptr) {
                state.m_controls.clear();
                EclipCmd cmd;
                cmd.type = EclipCmdType::Control;
                cmd.arg1 = "velocidad_texto";
                cmd.controlType = type;
                cmd.controlMin = -1.0f;
                cmd.controlMax = -1.0f;
                cmd.controlStep = -1.0f;
                cmd.controlWidth = 400.0f;
                state.cmdControl(cmd);
                if (state.m_controls.empty()) return;
                const BoundControl& control = state.m_controls.front();
                render = static_cast<int>(control.render);
                if (options) *options = static_cast<int>(control.textSpeedOptions.size());
            };

            bind("barra", result.numericBarRender);
            bind("flechas", result.numericArrowsRender, &result.numericArrowsOptions);
            bind("cajas", result.numericBoxesRender, &result.numericBoxesOptions);
            bind("toggle", result.numericToggleRender);

            state.m_theme = Theme::defaults();
            state.m_theme.textSpeed = *TextSpeed::tryCps(30.0f);
            state.m_theme.textSpeedDomain = textSpeedLegacyDomain();
            bind("flechas", result.legacyArrowsRender);
            bind("cajas", result.legacyBoxesRender);
            bind("toggle", result.legacyToggleRender);
            bind("cajas", result.legacyBoxesRender);

            state.m_theme = numericTheme;
            state.refreshTextSpeedControls();
            if (!state.m_controls.empty())
                result.numericAfterLegacyRender =
                    static_cast<int>(state.m_controls.front().render);

            state.m_theme = Theme::defaults();
            state.m_theme.textSpeed = *TextSpeed::tryCps(30.0f);
            state.m_theme.textSpeedDomain = textSpeedLegacyDomain();
            state.refreshTextSpeedControls();
            if (!state.m_controls.empty())
                result.legacyAfterNumericRender =
                    static_cast<int>(state.m_controls.front().render);
            return result;
        }
#endif

    };

    inline void VNScriptState::reconcileTextSpeedPresentation(
        VNScriptState::BoundControl& control)
    {
        if (!control.desc || control.desc->key != CtrlSetting::TextSpeed)
            return;

        if (control.textSpeedDomain.isLegacy())
        {
            control.numericPresentationWarned = false;
            control.numericPresentationWarningDomain = textSpeedLegacyDomain();
            control.render = control.requestedRender;
            if (control.requestedRender == CtrlRender::Toggle)
            {
                std::cerr << "[control] '" << control.settingId
                    << "' no admite tipo=toggle con mas de dos velocidades; "
                    << "se usa el render barra." << std::endl;
                control.render = CtrlRender::Bar;
            }
            return;
        }

        control.render = CtrlRender::Bar;
        if (control.requestedRender == CtrlRender::Bar)
            return;
        if (!control.numericPresentationWarned ||
            control.numericPresentationWarningDomain != control.textSpeedDomain)
        {
            std::cerr << "[control] '" << control.settingId
                << "' tipo=" << ctrlRenderName(control.requestedRender)
                << " no admite dominio numerico; se usa render barra."
                << std::endl;
            control.numericPresentationWarned = true;
            control.numericPresentationWarningDomain = control.textSpeedDomain;
        }
    }

} // namespace eclips
