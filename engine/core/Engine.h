// ============================================================================
// ECLIPS ENGINE - Motor Principal
// ============================================================================
// Esta es la clase central del motor. Se encarga de:
//   1. Inicializar SDL2 + OpenGL
//   2. Ejecutar el game loop con Fixed Timestep (doc 10.5)
//   3. Coordinar todos los subsistemas (render, audio, input)
//   4. Manejar el StateStack (doc 10.11)
//
// El game loop sigue el patrón Fixed Timestep:
//   - Lógica/física corre a 60Hz FIJO (determinista, siempre igual)
//   - Renderizado corre tan rápido como pueda la GPU
//   - Interpolación suaviza el movimiento visual
// ============================================================================

#pragma once

#include "Types.h"
#include "StateStack.h"
#include "ProjectManifest.h"
#include "LangStore.h"   // DISENO_IDIOMAS CP1: catalogo opcional del jugador
#include "MetaStore.h"   // ← nuevo
#include "SeenStore.h"   // ← P1 skip-de-leído
#include "engine/input/InputManager.h"   // ← átomo de teclado, CP1
#include "engine/render/FontSourceStore.h" // ATLAS CP-R2: bytes/planes de proyecto

#include <memory>

// Forward declaration de tipos SDL para no incluir SDL.h en el header
struct SDL_Window;
typedef void* SDL_GLContext;

namespace eclips
{

    class Renderer;     // Forward declare: vive en heap via unique_ptr;
    // definicion completa solo en Engine.cpp (no incluir Renderer.h aqui).
    class SpriteBatch;  // Forward declare: getters prestan el batch del Renderer.

    class Engine
    {
    public:
        Engine();
        ~Engine();

        // Prohibir copia (solo puede haber un motor)
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // ── Ciclo de vida ──

        // Inicializa SDL2, OpenGL, audio, input.
        // windowTitle: título de la ventana
        // windowWidth/Height: tamaño de la ventana (NO la resolución virtual)
        bool init(const std::string& windowTitle, i32 windowWidth, i32 windowHeight);

        // Ejecuta el game loop principal (bloquea hasta que se cierra)
        void run();

        // Limpia todo y libera recursos
        void shutdown();

        // Pedir que el motor se cierre al final del frame actual
        void quit();

        // ── Acceso al StateStack ──
        StateStack& getStateStack() { return m_stateStack; }

        // ── Info ──
        bool isRunning() const { return m_running; }
        f64 getFPS() const { return m_currentFPS; }

        // ── Acceso a subsistemas (para estados que necesiten dibujar) ──
        SDL_Window* getWindow() { return m_window; }
        // Delegan al Renderer (null-safe). Cuerpos en Engine.cpp — Renderer
        // esta forward-declared aqui (contraste Astra V2 §D).
        SDL_GLContext getGLContext() const;
        SpriteBatch* getSpriteBatch();
        FontSourceStore* getFontSourceStore() { return &m_fontSources; }
        i32 getWindowWidth() const { return m_windowWidth; }
        i32 getWindowHeight() const { return m_windowHeight; }


        // ── Acceso al ProjectManifest (D1) ──
        // Retorna nullptr si no había proyecto.eclips/project.eclips en
        // la raíz al arrancar (modo "test rápido sin proyecto"). Los
        // estados que lo consulten deben respetar ese caso.
        const ProjectManifest* getProjectManifest() const
        {
            return m_manifest.loaded ? &m_manifest : nullptr;
        }

        // ── Acceso al LangStore (idioma efectivo del jugador) ──
        // nullptr ES la ley de off: sin key `idiomas` no hay instancia,
        // probes de disco ni logs. Con catálogo existe aunque el paquete del
        // idioma esté vacío; translate() cae al original.
        LangStore* getLangStore() { return m_langStore.get(); }
        const LangStore* getLangStore() const { return m_langStore.get(); }

        // ── Acceso al MetaStore (variables persistentes inter-save) ──
        // Read-WRITE (a diferencia del manifest, que es solo lectura):
        // meta_set escribe a través de este puntero. Siempre válido — el
        // store existe aunque esté vacío (primer arranque). Vive en Engine
        // para sobrevivir a los cambios de state (un meta_set en gameplay se
        // ve en VN y en el menú).
        MetaStore* getMetaStore() { return &m_metaStore; }

        // ── Acceso al SeenStore (líneas ya vistas, skip-de-leído) ──
        SeenStore* getSeenStore() { return &m_seenStore; }

        // ── Acceso al InputManager (átomo de teclado, CP1) ──
        //
        // Vive en Engine por la misma razón que el MetaStore: la tabla de
        // teclas es UNA por juego, no una por pantalla. `CP3` la va a cargar
        // desde `proyecto.eclips` —que este mismo objeto ya lee en `init()`— y
        // `CP4` desde `settings.eclips`; si cada state tuviera la suya, un
        // remapeo aplicaría en la pantalla donde se hizo y en ninguna otra.
        //
        // Siempre válido: el manager nace con el default puro en su
        // constructor y las dos capas de override vacías (ley 1 del `CP0`).
        // Por eso el getter no puede devolver nullptr, y por eso los
        // consumidores lo reciben SIN valor por defecto — un puntero opcional
        // acá sería §7.471 en su forma exacta: el call-site incompleto
        // compilaría igual y el VN se quedaría sin teclado en silencio.
        const InputManager* getInputManager() const { return &m_input; }

        // ── CP9b: el mismo puntero, ESCRIBIBLE ───────────────────────────────
        //
        // `L-5` del contrato midio esto como *"el obstaculo real de `CP9b`, y no
        // es el widget"*: leer la tabla funcionaba, escribirla no, asi que el
        // `control tecla.*` no podia remapear sin un camino nuevo.
        //
        // **Es un accessor aparte y no un cambio del de arriba, a proposito.**
        // Los tres call-sites de `main.cpp` y los siete consumidores de lectura
        // siguen recibiendo el `const`: quien solo lee no gana permiso de
        // escritura por estar cerca. El unico que pide el mutable es el widget
        // de rebind.
        //
        // Lo que este puntero NO trae es la VOZ. `aplicarTeclasDelJugador()`
        // (privado, abajo) imprime los avisos del parser y los de la red §8
        // cuando la capa se monta en el boot; el rebind en vivo repite ese
        // patron de impresion en su propio sitio. **La LEY no se duplica** -es
        // `inputParsearCapaJugador` + `inputVozDelAviso`, una sola- ; lo que se
        // repite es el bucle que la imprime, y eso se declara en vez de
        // esconderse.
        InputManager* getInputManagerMutable() { return &m_input; }

        // ── Hot-reload del manifest (Modo Creador / dev-only) ──
        // Re-lee proyecto.eclips con FALLO SEGURO: si el archivo falta, no
        // abre, o abre pero no parsea válido, conserva el manifest anterior.
        // La publicación (swap + generación + notify) se COMMITEA una sola
        // vez en run(), antes del render, incluso con cero fixed-update.
        // TODO(modo-creador): se compila fuera en build de export (§12).
        void reloadManifest();

        // Commit único de presentación: manifest pendiente + generación
        // tipográfica. Lo llama run() antes de renderizar.
        void commitPendingPresentation();

    private:
        // ── La capa del creador del teclado (átomo, CP3) ──
        // Lee `m_manifest.teclas` (texto crudo), le pide la ley a
        // `InputManager.h`, imprime los avisos y monta la capa. Corre en
        // `init()` y otra vez en `reloadManifest()`: con eso, F8 re-aplica un
        // remapeo del creador EN VIVO, que es la tercera vida del setting
        // (§7.154) sin escribir una línea de más.
        void aplicarTeclasDelManifest();

        // ── La capa del jugador del teclado (átomo, CP4) ──
        // Gemela de la de arriba, con el mismo reparto: `Settings` transporta
        // el texto crudo de `settings.eclips`, `InputManager.h` tiene la ley, y
        // esta función es la que HABLA. Corre en `init()` justo después de la
        // del creador — el orden no cambia el resultado (la cascada recalcula
        // desde el default en cada set), pero sí el orden de los avisos, y que
        // primero se lea el archivo del creador y después el del jugador es lo
        // que un ojo espera.
        //
        // NO corre en `reloadManifest()`: F8 recarga el proyecto, no el
        // settings del jugador. La capa del jugador vive en `m_input` y
        // sobrevive al hot-reload sola.
        void aplicarTeclasDelJugador();

        // ── Subsistemas ──
        SDL_Window* m_window = nullptr;
        // Contexto GL vive en Renderer. Cache no-propietario para rollback /
        // getters: se setea tras init valido y se anula en shutdown/fallo.
        SDL_GLContext m_glContext = nullptr;

        // ── Render core (CP1) ────────────────────────────────────────────
        // Ownership: contexto GL + GLAD + SpriteBatch viven en Renderer.
        // Engine conserva ventana/SDL, predicado, queries, stats y swap.
        std::unique_ptr<Renderer> m_renderer;

        // ── ATLAS CP-R2: fuente física y planes bootstrap ───────────────
        // Vive más que StateStack; estados/FontManager sólo guardan punteros
        // no propietarios y sus atlas siguen siendo por vista.
        FontSourceStore m_fontSources;

        // ── Manifest del proyecto (cargado en init()) ──
        // Si proyecto.eclips no existe, m_manifest.loaded queda false.
        ProjectManifest m_manifest;
        bool m_hasPendingManifest = false;
        ProjectManifest m_pendingManifest;
        bool m_pendingNotifyManifest = false;

        // ── LangStore: sólo existe si el manifest declaró un catálogo. ──
        std::unique_ptr<LangStore> m_langStore;

        // ── MetaStore: variables persistentes inter-save (cargado en init()) ──
        // Tercer nivel de persistencia: estado del JUEGO que sobrevive a borrar
        // saves. Siempre presente (vacío en primer arranque).
        MetaStore m_metaStore;

        // ── SeenStore: líneas ya vistas (skip-de-leído). Mismo nivel que meta. ──
        SeenStore m_seenStore;

        // ── InputManager: la tabla tecla→verbo (átomo de teclado). ──
        // No se carga de disco todavía: su constructor arma el default puro y
        // deja las dos capas de override vacías, así que en `CP1` la tabla
        // resuelta ES el censo de hoy. `CP3` le va a llamar `setCapaCreador`
        // desde el manifest y `CP4` `setCapaJugador` desde settings.
        InputManager m_input;

        // ── Estado ──
        bool m_running = false;
        // Idempotencia de shutdown(): m_running NO sirve como "ya se desmontó"
        // porque se vuelve false al salir del game loop por cualquier razón
        // (SDL_QUIT, stack vacío). Este flag rastrea el desmontaje de verdad.
        bool m_shutdownDone = false;
        f64  m_currentFPS = 0.0;

        // ── Resolución virtual (doc 10.7) ──
        i32 m_windowWidth  = 1280;
        i32 m_windowHeight = 720;
        // La resolución virtual está definida en viewport::width()/HEIGHT

        // ── Game loop ──
        StateStack m_stateStack;

        // Métodos internos del game loop
        // Retorna true si se procesó AL MENOS un evento SDL esta pasada — lo usa
        // la ventana de gracia del skip-frame (§6.4): render ~0.25s tras input.
        bool processEvents();  // SDL events (cierre de ventana, resize, etc.)
    };

} // namespace eclips
