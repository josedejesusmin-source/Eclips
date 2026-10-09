// ============================================================================
// ECLIPS ENGINE - Motor Principal (Implementación)
// ============================================================================

#include "Engine.h"
#include "WindowResolution.h"
#include "Settings.h"   // ← CP4 del teclado: los pares crudos de `tecla.*`
#include "LangPaths.h"  // ← ficha `F8-SIN-IDIOMA`: langMemoLimpiar() tras recargar
                        //   el idioma. Engine.h ya traía LangStore.h, no éste.

// glad PRIMERO de todo lo GL. Sobrescribe los punteros antes de
// que SDL_opengl.h declare los mismos simbolos como extern de
// fixed function. El resto del motor (Texture.h, Draw2D.h, etc.)
// sigue incluyendo SDL_opengl.h sin problemas — glad ya gana.
#include <glad/glad.h>

#include "../render/Renderer.h"
#include "../render/SpriteBatch.h"
#include "../render/Draw2D.h"
#include "../render/FontGeneration.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <filesystem>  // current_path / exists — ancla del CWD al paquete (A2)
#include <cstring>     // strstr — check de extensión GL (termómetro CP2)

#ifdef _WIN32
    #include <direct.h>  // _getcwd
#else
    #include <unistd.h>  // getcwd
#endif

namespace eclips
{
    Engine::Engine()
    {
    }

    Engine::~Engine()
    {
        // Red de seguridad: desmontar si shutdown() no se llamó explícitamente
        // (p.ej. un path que no pase por main). Usamos m_shutdownDone, NO
        // m_running: al llegar aquí m_running YA es false (el loop terminó), así
        // que el guard viejo if(m_running) nunca disparaba esta red. (§gotcha)
        if (!m_shutdownDone)
        {
            shutdown();
        }
    }

    // ========================================================================
    // Inicialización
    // ========================================================================
    // ========================================================================
    // La capa del creador del teclado (átomo de teclado, CP3)
    // ========================================================================
    //
    // El adaptador entre las dos mitades: `ProjectManifest` transporta TEXTO
    // (`TeclaFila`) y `InputManager.h` tiene la LEY. Las dos estructuras dicen
    // lo mismo y son distintas a propósito — el porqué está escrito en
    // `TeclaFila`, y se resume en que el manifest no debe arrastrar SDL.
    //
    // Y esta función es la que HABLA. Las dos capas de abajo devuelven sus
    // avisos como datos (`inputVozDelAviso`, `dominiosRescatados`) para que la
    // batería los pueda afirmar byte a byte sin cazar stderr; acá es donde eso
    // se convierte en consola.
    void Engine::aplicarTeclasDelManifest()
    {
        if (m_manifest.teclas.empty())
        {
            // Sin bloque `teclas`, la capa se vacía igual. Importa en el
            // hot-reload: si el creador BORRA su bloque y aprieta F8, la capa
            // vieja tiene que irse, no quedar pegada.
            m_input.setCapaCreador(InputManager::Tabla{});
            return;
        }

        std::vector<InputFilaCreador> filas;
        filas.reserve(m_manifest.teclas.size());
        for (const ProjectManifest::TeclaFila& t : m_manifest.teclas)
        {
            InputFilaCreador f;
            f.dominio = t.dominio;
            f.verbo   = t.verbo;
            f.teclas  = t.teclas;
            f.linea   = t.linea;
            filas.push_back(f);
        }

        const InputCapaCreador capa = inputParsearCapaCreador(filas);

        const std::string origen =
            m_manifest.sourcePath.empty() ? std::string("proyecto.eclips")
                                          : m_manifest.sourcePath;

        for (const InputAviso& a : capa.avisos)
            std::cerr << inputVozDelAviso(a, origen) << std::endl;

        m_input.setCapaCreador(capa.tabla);

        // La red §8: el motor inventa, pero NO en silencio.
        for (const std::string& dom : m_input.dominiosRescatados())
        {
            InputAviso r;
            r.causa   = InputAvisoCausa::RedDeSeguridad;
            r.dominio = dom;
            std::cerr << inputVozDelAviso(r, "teclas") << std::endl;
        }
    }

    // ========================================================================
    // La capa del jugador del teclado (átomo de teclado, CP4)
    // ========================================================================
    //
    // El mismo adaptador que el de arriba, sobre el otro archivo: `Settings`
    // transporta los pares crudos de `settings.eclips` y `InputManager.h`
    // decide qué significan. Lo único que agrega esta capa sobre la del
    // creador es la FORMA de la clave; el resto de la ley es la misma función.
    //
    // El `origen` que se le pasa a la voz es el nombre del archivo, para que el
    // aviso mande al jugador al lugar correcto: es el mismo parámetro que `CP3`
    // dejó puesto justo para esto.
    void Engine::aplicarTeclasDelJugador()
    {
        const auto& crudas = Settings::teclas();
        if (crudas.empty())
        {
            // Sin claves `tecla.*` la capa se vacía igual, por lo mismo que la
            // del creador: si el jugador BORRA sus líneas, la capa vieja tiene
            // que irse en el próximo arranque, no quedar pegada.
            m_input.setCapaJugador(InputManager::Tabla{});
            return;
        }

        const InputCapaJugador capa = inputParsearCapaJugador(crudas);

        for (const InputAviso& a : capa.avisos)
            std::cerr << inputVozDelAviso(a, "settings.eclips") << std::endl;

        m_input.setCapaJugador(capa.tabla);

        // La red §8 otra vez, y no es copia: corre sobre la tabla RESUELTA, así
        // que el rescate puede dispararlo el jugador tanto como el creador.
        for (const std::string& dom : m_input.dominiosRescatados())
        {
            InputAviso r;
            r.causa   = InputAvisoCausa::RedDeSeguridad;
            r.dominio = dom;
            std::cerr << inputVozDelAviso(r, "teclas") << std::endl;
        }
    }

    bool Engine::init(const std::string& windowTitle, i32 windowWidth, i32 windowHeight)
    {
        // ── Cargar ProjectManifest (D2) ──
        // Solo lectura de disco; no toca SDL. Si no existe proyecto.eclips
        // ni project.eclips, m_manifest.loaded queda false y el motor sigue
        // con los parámetros por defecto pasados a init() (modo "test
        // rápido sin proyecto").
        m_manifest = ProjectManifest::tryLoad();

        // ── Idioma efectivo del jugador (DISENO_IDIOMAS CP1) ──
        // Settings ya transportó `lang` antes de init(); recién acá existen
        // las dos fuentes y se puede validar contra el catálogo del manifest.
        // Sin key `idiomas` la selección pura devuelve off y NO se construye
        // LangStore: cero probes y cero logs, que es el voto 3 del contrato.
        const LangSeleccion lang = langSeleccionar(
            m_manifest.idiomas,
            Settings::storedLanguage(),
            Settings::languageUserSet());
        if (lang.sistemaActivo)
        {
            if (lang.preferenciaInvalida)
            {
                std::cerr << "[idiomas] El idioma guardado '"
                          << Settings::storedLanguage()
                          << "' ya no existe en el catalogo. Uso el original '"
                          << lang.original << "'." << std::endl;
            }

            m_langStore = std::make_unique<LangStore>();
            std::filesystem::path raiz =
                std::filesystem::path(m_manifest.sourcePath).parent_path();
            if (raiz.empty()) raiz = ".";
            // El tercer argumento es CP3: hasta ahora `lang.original` moría con
            // este scope, y la regla 2 del §4 lo necesita vivo para resolver el
            // nombre de mes de un tercer idioma ("si el activo no tiene tabla,
            // la base sale de la tabla del ORIGINAL").
            m_langStore->load(lang.efectivo, raiz.string(), lang.original);
        }
        else
        {
            m_langStore.reset();
        }

        // CP3 (fork 9b): el chrome del motor lee de acá con `langUI()`, igual
        // que lee `Settings::textSpeed()`. Engine sigue siendo el DUEÑO —esto
        // es una vista, no una segunda copia— y sin catálogo registra nullptr,
        // que es el mismo apagado por identidad del voto 3.
        langRegistrarActivo(m_langStore.get());

        // ── La capa del creador del teclado (átomo, CP3) ──
        // Va acá arriba, junto al manifest del que sale, y ANTES de SDL_Init a
        // propósito: el manager no necesita SDL corriendo (usa sus headers, no
        // sus funciones), y así el remapeo ya está resuelto cuando el primer
        // state se construye. Sin manifest, `m_manifest.teclas` viene vacío y
        // esto es un no-op exacto.
        aplicarTeclasDelManifest();

        // ── La capa del jugador del teclado (átomo, CP4) ──
        // Justo después, y no antes: primero se lee el archivo del creador y
        // después el del jugador, que es el orden de la cascada y el orden en
        // que un ojo espera leer los avisos. `Settings::load()` ya corrió —
        // main.cpp lo llama antes de `init()` para abrir la ventana en su
        // tamaño guardado— así que los pares crudos ya están en memoria.
        aplicarTeclasDelJugador();

        // ── Sembrar la resolución de diseño en runtime (A1) ──
        // Si el manifest define resolucion_diseno, manda; si no, el holder
        // conserva su default (1920×1080). Debe correr ANTES del primer frame.
        if (m_manifest.loaded)
            viewport::set(m_manifest.designW, m_manifest.designH);

        // ── Cargar MetaStore (variables persistentes inter-save) ──
        // Solo lectura de disco; no toca SDL. Si meta.eclips no existe (primer
        // arranque del jugador), el store queda vacío y se creará al primer
        // meta_set. Sobrevive a borrar saves — por eso vive aparte del save.
        m_metaStore.load();

        // ── Cargar SeenStore (líneas ya vistas; mismo nivel de persistencia) ──
        m_seenStore.load();

        // Si el manifest define un título, gana sobre el parámetro de init().
        // Esto permite a main.cpp pasar un default y que proyecto.eclips
        // lo override sin tocar main.cpp. Ver comentario en main.cpp.
        const std::string effectiveTitle =
            (m_manifest.loaded && !m_manifest.titulo.empty())
            ? m_manifest.titulo
            : windowTitle;

        // ── Inicializar SDL2 ──
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        {
            std::cerr << "[Eclips] Error SDL_Init: " << SDL_GetError() << std::endl;
            return false;
        }

        // ── Validar tamaño WINDOWED contra el área UTILIZABLE ─────────────
        // La ventana se centra en display 0, así que ese mismo display decide
        // el límite antes de que SDL_CreateWindow exista. DisplayBounds incluye
        // barra de tareas/chrome reservado; DisplayUsableBounds no.
        SDL_Rect usable{};
        if (SDL_GetDisplayUsableBounds(0, &usable) == 0)
        {
            const window_resolution::Size requested{ windowWidth, windowHeight };
            const window_resolution::Size repaired =
                window_resolution::repairOversized(
                    requested,
                    viewport::width(), viewport::height(),
                    usable.w, usable.h);
            if (!(repaired == requested))
            {
                std::cout << "[Eclips] Tamano windowed reparado: "
                    << requested.w << "x" << requested.h << " -> "
                    << repaired.w << "x" << repaired.h
                    << " (area util " << usable.w << "x" << usable.h << ")"
                    << std::endl;
                windowWidth = repaired.w;
                windowHeight = repaired.h;
            }
        }
        else
        {
            std::cerr << "[Eclips] Aviso: no se pudo consultar el area util del "
                "monitor; se conserva el tamano pedido: " << SDL_GetError()
                << std::endl;
        }

        m_windowWidth = windowWidth;
        m_windowHeight = windowHeight;

        // ── Configurar OpenGL (atributos en Renderer; CreateWindow aqui) ──
        // En desktop: Compatibility 3.1 primero (fixed pipeline Fase 1).
        // Fallback: OpenGL ES 2.0. Renderer aplica los SDL_GL_SetAttribute.
        Renderer::applyCompat31Attributes();

        // ── Crear ventana ──
        m_window = SDL_CreateWindow(
            effectiveTitle.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            windowWidth,
            windowHeight,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );

        if (m_window)
        {
            std::cout << "[Eclips] Usando OpenGL 3.1 Compatibility" << std::endl;
        }
        else
        {
            // Fallback: intentar OpenGL ES 2.0 (para plataformas móviles o ANGLE)
            Renderer::applyES20Attributes();

            m_window = SDL_CreateWindow(
                effectiveTitle.c_str(),
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                windowWidth,
                windowHeight,
                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
            );

            if (!m_window)
            {
                std::cerr << "[Eclips] Error al crear ventana: " << SDL_GetError() << std::endl;
                SDL_Quit();
                return false;
            }

            std::cout << "[Eclips] Usando OpenGL ES 2.0" << std::endl;
        }

        // ── Renderer: contexto + glad + SpriteBatch (no crea ventana) ──
        m_renderer = std::make_unique<Renderer>();
        if (!m_renderer->init(m_window))
        {
            // Renderer ya revirtio solo lo que adquirio (contexto/batch).
            m_glContext = nullptr;
            m_renderer.reset();
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            SDL_Quit();
            return false;
        }
        m_glContext = m_renderer->context();  // cache no-propietario

        // VSync activado (1 = activado, 0 = desactivado).
        // §1.40 + retorno chequeado (leccion del F10, CP3.2): el driver
        // puede rechazar el intervalo en silencio; reportar SU estado.
        if (SDL_GL_SetSwapInterval(1) != 0)
            std::cout << "[Perf] SetSwapInterval(1) en init FALLO: "
            << SDL_GetError() << std::endl;
        std::cout << "[Perf] VSync inicial: "
            << (SDL_GL_GetSwapInterval() == 1 ? "ON" : "OFF")
            << " (driver)" << std::endl;

        // ── Configurar directorio de trabajo (ancla del paquete · A2) ──────
        // El .exe shippeado vive JUNTO a sus assets (assets/, proyecto.eclips,
        // temas, .ecl). En dev vive en out/build/... lejos de la raíz, y VS
        // (VS_DEBUGGER_WORKING_DIRECTORY) o PowerShell (cd) ya fijan el CWD a
        // la raíz. Por eso NO anclamos siempre — solo si detectamos que somos
        // un paquete autocontenido (un proyecto/project.eclips al lado del exe).
        // Off por identidad: en dev no hay marcador → CWD intacto → byte-idéntico;
        // en el paquete anclamos y el doble-clic resuelve assets desde cualquier CWD.
        char* basePath = SDL_GetBasePath();
        if (basePath)
        {
            std::cout << "[Eclips] Directorio del ejecutable: " << basePath << std::endl;

            namespace fs = std::filesystem;
            std::error_code ec;
            const fs::path base(basePath);
            const bool esPaquete =
                fs::exists(base / "proyecto.eclips", ec) ||
                fs::exists(base / "project.eclips", ec);

            if (esPaquete)
            {
                fs::current_path(base, ec);
                if (ec)
                    std::cerr << "[Eclips] Aviso: no se pudo anclar el CWD a '"
                    << basePath << "' (" << ec.message() << ")" << std::endl;
                else
                    std::cout << "[Eclips] CWD anclado al paquete." << std::endl;
            }

            SDL_free(basePath);
        }

        // Imprimir directorio de trabajo actual para debug
        char cwd[1024];
        #ifdef _WIN32
            _getcwd(cwd, sizeof(cwd));
        #else
            getcwd(cwd, sizeof(cwd));
        #endif
            std::cout << "[Eclips] Directorio de trabajo: " << cwd << std::endl;

            // SpriteBatch + g_batch: publicados por Renderer::init.

            // TODO: Inicializar el audio (miniaudio)
            // TODO: Inicializar el input (sistema de acciones)

            m_running = true;
            return true;
    }

    // ── GL_TIME_ELAPSED (termómetro CP2) ────────────────────────────────
    // Token de GL 3.3 / ARB_timer_query. glad se generó para 3.1 → puede
    // no definirlo. El valor es estándar del registro GL.
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

    // ========================================================================
    // Game Loop - Fixed Timestep (doc 10.5)
    // ========================================================================
    // Este es el corazón del motor. La lógica del juego SIEMPRE corre a 60Hz.
    // El renderizado corre tan rápido como pueda.
    // Esto garantiza que el juego se siente idéntico en cualquier hardware.
    // ========================================================================
    void Engine::run()
    {
        using Clock = std::chrono::high_resolution_clock;
        using Duration = std::chrono::duration<f64>;

        auto previousTime = Clock::now();
        f64 accumulator = 0.0;
        f64 fpsTimer = 0.0;
        i32 frameCount = 0;
        f64 graceTimer = 0.0;   // ventana de gracia del skip-frame (§6.4)
        bool wasQuiescent = false;  // flush de borde (CP2.5): ver decisión de render
        f64 heartbeatTimer = 0.0;   // heartbeat 1Hz (CP2.7): ver decisión de render

        // ── Termómetro CPU (perfilado de consumo, CP1) ──────────────
        // Acumuladores de la ventana de 1s del [FPS]. 'swap' ≈ espera
        // de vblank con vsync ON (aprox: el driver puede diferir el
        // bloqueo). El trabajo GPU real llega en CP2 (timer query).
        f64 accEventsMs = 0.0;
        f64 accUpdateMs = 0.0;
        f64 accRenderMs = 0.0;
        f64 accSwapMs = 0.0;

        // ── Termómetro GPU (CP2): timer query con guard runtime ────────
        // Mide SOLO nuestro trabajo GL (DWM/compositor quedan fuera —
        // eso es justo el poder diagnóstico). Contexto 3.1 Compat →
        // la extensión puede faltar; sin ella el campo 'gpu' se omite.
        // Nanosegundos en 32 bits alcanzan ~4.29s/frame → basta
        // glGetQueryObjectuiv (GL 1.5, ya en glad); no hace falta ui64.
        const char* extStr =
            reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        const bool gpuTimerOk = extStr &&
            (std::strstr(extStr, "GL_ARB_timer_query") ||
                std::strstr(extStr, "GL_EXT_timer_query"));
        GLuint gpuQueries[4] = { 0, 0, 0, 0 };
        if (gpuTimerOk)
            glGenQueries(4, gpuQueries);
        u64 frameIdx = 0;
        f64 accGpuMs = 0.0;
        i32 gpuSamples = 0;
        u64 accFlushes = 0;   // draws del batch por ventana (CP3)
        u64 accQuads = 0;
        u64 accSkips = 0;     // frames SALTADOS por ventana (skip-frame CP1)
        u64 iterCount = 0;    // iteraciones del loop por ventana (CP2.7, ver [FPS])
        std::cout << "[Perf] Timer query GPU: "
            << (gpuTimerOk ? "disponible"
                : "NO disponible (campo gpu omitido)")
            << std::endl;

        while (m_running)
        {
            // ── Calcular tiempo transcurrido ──
            auto currentTime = Clock::now();
            f64 frameTime = Duration(currentTime - previousTime).count();
            previousTime = currentTime;

            // Clamp para evitar espiral de la muerte
            // (si el frame tardó más de 0.25s, fingimos que solo pasó 0.25s)
            if (frameTime > 0.25)
            {
                frameTime = 0.25;
            }

            accumulator += frameTime;

            // ── Procesar eventos SDL ──
            auto tEv0 = Clock::now();
            const bool hadEvent = processEvents();
            auto tEv1 = Clock::now();
            accEventsMs += Duration(tEv1 - tEv0).count() * 1000.0;

            // Ventana de gracia del skip-frame (§6.4): cualquier evento mantiene
            // el render vivo ~0.25s para que hovers/juice asienten sin lag de wake.
            if (hadEvent) graceTimer = 0.25;

            // ── Update a ritmo FIJO (60Hz) ──
            // Se ejecuta tantas veces como sea necesario para "ponerse al día"
            auto tUpd0 = Clock::now();
            i32 updates = 0;
            while (accumulator >= constants::FIXED_TIMESTEP &&
                   updates < constants::MAX_UPDATES_PER_FRAME)
            {
                // Aquí va toda la lógica del juego:
                // input, física, IA, scripts, etc.
                m_stateStack.update(constants::FIXED_TIMESTEP);

                accumulator -= constants::FIXED_TIMESTEP;
                updates++;
            }
            auto tUpd1 = Clock::now();
            accUpdateMs += Duration(tUpd1 - tUpd0).count() * 1000.0;

            // CP-R4: commit único ANTES del render. El while de update puede
            // haber corrido cero veces (vsync off / 144 Hz) y un overlay puede
            // dibujar el VN sin actualizarlo. Preparación pendiente + un solo
            // swap de manifest/generación aquí cierra la ventana de punteros
            // colgantes. Token que el harness busca.
            commitPendingPresentation(); // commitFontGeneration

            // ── Render a ritmo VARIABLE ──
            // alpha = qué tan lejos estamos entre dos updates de lógica
            // Se usa para interpolar posiciones y suavizar el movimiento
            f64 alpha = accumulator / constants::FIXED_TIMESTEP;

            // Limpiar pantalla (color de fondo oscuro - el "lienzo" de Eclips)
            // TODO: Esto se moverá al Renderer cuando exista
            // Por ahora usamos llamadas OpenGL directas
            // glClearColor(0.05f, 0.05f, 0.12f, 1.0f);
            // glClear(GL_COLOR_BUFFER_BIT);

            // ── Skip-frame (Perfilado §6, CP1): ¿todo el set visible quiescente? ──
            // VNScriptState ya implementa el predicado conservador; los estados
            // sin override conservan el default false de GameState.
            const bool quiescent = m_stateStack.isVisibleSetQuiescent();
            // CP0 reloj-dialogo: una mutación discreta puede quedar quiescente
            // antes y después del mismo update. El latch fuerza exactamente una
            // presentación sin fingir actividad ni extender graceTimer.
            const bool renderRequested = m_stateStack.consumeRenderRequest();
            // Render si NO está quiescente, si la gracia post-evento sigue viva,
            // o UN frame más en el flanco activo→quiescente (flush de borde,
            // CP2.5): una fuente que se apaga DURANTE update y SIN evento SDL
            // (la ventana de un esporadico que cierra, un clamp que llegó) no
            // tiene gracia que la presente — el predicado voltea a true en la
            // misma iteración y el frame con el estado ASENTADO nunca llegaría
            // al front-buffer (quedaría el penúltimo: un glitch congelado en
            // pantalla). El flanco presenta ese frame y recién entonces duerme.
            // …y el heartbeat 1Hz (CP2.7, §6.4): render forzado 1/s en
            // quiescencia. Diseño del modo de falla: si una fuente futura se
            // escapa del predicado, el peor caso pasa de "congelado para
            // siempre" a "1s stale". Con el gate de rol puesto el riesgo
            // estaba acotado a menús; abierto, el latido no es opcional.
            // (En reposo la línea [FPS] pasa de `fps 0` a `fps ~1`: el latido
            //  ES un frame presentado — el termómetro no miente, §7.283.)
            heartbeatTimer += frameTime;
            const bool doRender = renderRequested || !quiescent ||
                               graceTimer > 0.0 || !wasQuiescent ||
                               heartbeatTimer >= 1.0;
            wasQuiescent = quiescent;
            if (doRender) heartbeatTimer = 0.0;

            if (doRender)
            {
                // ── Frame PRESENTADO: render + swap (camino normal) ──
                auto tRen0 = Clock::now();
                if (gpuTimerOk)
                    glBeginQuery(GL_TIME_ELAPSED, gpuQueries[frameIdx % 4]);
                m_renderer->beginFrame();
                m_stateStack.render(alpha);
                m_renderer->endFrame();
                if (gpuTimerOk)
                    glEndQuery(GL_TIME_ELAPSED);
                auto tRen1 = Clock::now();
                accRenderMs += Duration(tRen1 - tRen0).count() * 1000.0;

                // Stats del batch a nivel frame (CP3): leer y resetear aqui,
                // una vez por frame — begin() ya no las toca.
                if (SpriteBatch* batch = m_renderer->batch())
                {
                    accFlushes += batch->getFrameFlushes();
                    accQuads += batch->getFrameQuads();
                    batch->resetFrameStats();
                }

                // Intercambiar buffers (mostrar el frame)
                SDL_GL_SwapWindow(m_window);
                auto tSwap1 = Clock::now();
                accSwapMs += Duration(tSwap1 - tRen1).count() * 1000.0;

                // Leer la query de hace 3 frames — ya terminó casi seguro,
                // así nunca bloqueamos esperando a la GPU. Los primeros 3
                // frames no tienen muestra (por eso gpuSamples aparte).
                // OJO (§7.38): el ring SOLO avanza en frames PRESENTADOS. Si un
                // frame saltado avanzara frameIdx, la lectura "3 atrás" caería en
                // un slot que nunca se abrió con glBeginQuery → basura/error GL.
                if (gpuTimerOk && frameIdx >= 3)
                {
                    GLuint qOld = gpuQueries[(frameIdx - 3) % 4];
                    GLuint avail = 0;
                    glGetQueryObjectuiv(qOld, GL_QUERY_RESULT_AVAILABLE, &avail);
                    if (avail)
                    {
                        GLuint ns = 0;
                        glGetQueryObjectuiv(qOld, GL_QUERY_RESULT, &ns);
                        accGpuMs += static_cast<f64>(ns) / 1.0e6;
                        gpuSamples++;
                    }
                }
                frameIdx++;
                frameCount++;   // 'fps' cuenta frames PRESENTADOS, no iteraciones
            }
            else
            {
                // ── Frame SALTADO: ni render ni swap. ──
                // La trampa del spin (§6.2): hoy el loop DUERME en SwapWindow
                // (vblank). Sin ese swap, el while(m_running) giraría a MHz →
                // CPU 100%. SDL_Delay acotado (~4ms) sustituye la espera → CPU no
                // sube, e input se sondea cada ~4ms (wake ≤4ms ≪ 1 frame). El
                // accumulator YA consumió sus updates arriba: la lógica NO se
                // congela, solo el repintado. accSwapMs y el ring de query NO se
                // tocan (no hubo ni ejecución GL ni swap que medir → sin mentir).
                SDL_Delay(4);
                accSkips++;
            }

            // Descontar la ventana de gracia con el tiempo real del frame.
            if (graceTimer > 0.0)
            {
                graceTimer -= frameTime;
                if (graceTimer < 0.0) graceTimer = 0.0;
            }

            // ── Calcular FPS ──
            iterCount++;
            fpsTimer += frameTime;
            if (fpsTimer >= 1.0)
            {
                m_currentFPS = static_cast<f64>(frameCount) / fpsTimer;
                // Promedios ms/frame de la ventana. OJO semántico:
                // 'rend' = CPU de submit (llamadas GL), NO ejecución GPU;
                // 'swap' ≈ espera de vblank con vsync ON.
                // CP2.7 (§7.283, segunda ronda): 'ev'/'upd' trabajan POR
                // ITERACIÓN del loop (polling y fixed-timestep corren aunque
                // el frame se salte); dividirlos por PRESENTADOS mentía bajo
                // el heartbeat (1 presentado + ~215 iteraciones → ev "5.7").
                // Sin skips, iteraciones == presentados → byte-idéntico a hoy.
                const f64 invF = (frameCount > 0)
                    ? 1.0 / static_cast<f64>(frameCount) : 0.0;
                const f64 invI = (iterCount > 0)
                    ? 1.0 / static_cast<f64>(iterCount) : 0.0;
                // Las banderas de formato de `std::cout` son GLOBALES y
                // PEGAJOSAS: `std::fixed` + `setprecision(1)` puestos acá se
                // quedaban puestos para el resto de la corrida, así que TODO
                // float que el motor imprimiera después de la primera línea de
                // FPS salía con un decimal. Lo destapó la traza del azar
                // (SYN-4b): decía `tirada 0.2 vs 0.0` donde el valor real era
                // `0.194116` — y con un decimal, un casi-acierto y una paliza
                // se ven igual. Se guardan y se restauran; el formato de esta
                // línea no cambia.
                const std::ios_base::fmtflags fmtPrevio = std::cout.flags();
                const std::streamsize        precPrevia = std::cout.precision();

                std::cout << "[FPS] " << std::fixed << std::setprecision(1)
                    << m_currentFPS << std::setprecision(2)
                    << " | ms/f: ev " << (accEventsMs * invI)
                    << " | upd " << (accUpdateMs * invI)
                    << " | rend " << (accRenderMs * invF)
                    << " | swap " << (accSwapMs * invF);
                if (gpuTimerOk && gpuSamples > 0)
                    std::cout << " | gpu "
                    << (accGpuMs / static_cast<f64>(gpuSamples));
                else if (!gpuTimerOk)
                    std::cout << " | gpu n/d";
                std::cout << std::setprecision(1)
                    << " | draws " << (static_cast<f64>(accFlushes) * invF)
                    << " | quads " << (static_cast<f64>(accQuads) * invF);
                // skip-frame (CP1): frames saltados en la ventana. En reposo total
                // sube a ~250 (SDL_Delay 4ms) y 'fps' cae a 0 (nada presentado);
                // en actividad es 0 y la línea es byte-idéntica a la de antes.
                std::cout << " | skip " << accSkips;
                std::cout << std::endl;

                std::cout.flags(fmtPrevio);
                std::cout.precision(precPrevia);
                frameCount = 0;
                fpsTimer = 0.0;
                accEventsMs = accUpdateMs = accRenderMs = accSwapMs = 0.0;
                accGpuMs = 0.0;
                gpuSamples = 0;
                accFlushes = 0;
                accQuads = 0;
                accSkips = 0;
                iterCount = 0;
            }

            // Si no hay estados, el motor no tiene nada que hacer
            if (m_stateStack.isEmpty())
            {
                m_running = false;
            }
        }

        if (gpuTimerOk)
            glDeleteQueries(4, gpuQueries);
    }


    // ========================================================================
    // reloadManifest — hot-reload del proyecto.eclips con fallo seguro
    // ========================================================================
    void Engine::reloadManifest()
    {
        // Cargar en un manifest TEMPORAL. Swap solo si salió bien: un
        // proyecto.eclips guardado a medias / borrado nunca debe reemplazar el
        // manifest vivo con basura (mismo footgun que cargar_tema con ruta
        // inexistente). loaded==true = el archivo abrió y el parser corrió completo.
        ProjectManifest fresh = ProjectManifest::tryLoad();

        if (!fresh.loaded)
        {
            if (!m_manifest.loaded)
                std::cout << "[Engine] Recarga: no hay proyecto.eclips (modo test). "
                "Nada que recargar." << std::endl;
            else
                std::cerr << "[Engine] Recarga FALLO: proyecto.eclips no encontrado o "
                "ilegible. Mantengo el manifest anterior." << std::endl;
            return;  // fallo seguro: NO se toca m_manifest
        }

        if (!fresh.parseValid)
        {
            std::cerr << "[Engine] Recarga FALLO: proyecto.eclips abrio pero el "
                "parseo no es valido (bloque sin cerrar). Mantengo el "
                "manifest anterior." << std::endl;
            return;
        }

        m_pendingManifest = std::move(fresh);
        m_hasPendingManifest = true;
        m_fontSources.prepareGeneration();
    }

    void Engine::commitPendingPresentation()
    {
        // Plan y contexto de idioma CANDIDATOS primero. El horneado de
        // fuentes usa ese resolver; el LangStore / memo / Settings vivos
        // no se tocan hasta el corte único de más abajo.
        std::unique_ptr<LangStore> candidateLang;
        const LangStore* bakeLang = nullptr;
        LangF8Plan langPlan{};
        std::filesystem::path langRaiz = ".";
        if (m_hasPendingManifest)
        {
            langPlan = langPlanF8(
                m_pendingManifest.idiomas,
                Settings::storedLanguage(),
                Settings::languageUserSet(),
                /*hayStore*/ m_langStore != nullptr);
            if (langPlan.accion != LangF8Accion::Nada)
            {
                langRaiz = std::filesystem::path(
                    m_pendingManifest.sourcePath).parent_path();
                if (langRaiz.empty()) langRaiz = ".";
                candidateLang = std::make_unique<LangStore>();
                if (langPlan.accion == LangF8Accion::Apagar)
                    candidateLang->load("", langRaiz.string(), langPlan.original);
                else
                    candidateLang->load(langPlan.efectivo, langRaiz.string(),
                                        langPlan.original);
                bakeLang = candidateLang.get();
            }
        }

        if (m_fontSources.pendingGeneration() != 0)
        {
            m_fontSources.setCandidatePresentation(
                m_hasPendingManifest ? &m_pendingManifest.fuentes : nullptr,
                m_hasPendingManifest ? &m_pendingManifest.temaDefault : nullptr);
            const bool baked = fontFinalizePublisher(
                m_fontSources, bakeLang);
            m_fontSources.setCandidatePresentation(nullptr, nullptr);
            if (!baked)
            {
                m_fontSources.rollbackGeneration();
                m_hasPendingManifest = false;
                m_pendingManifest = ProjectManifest{};
                return;
            }
        }

        const bool swappedManifest = m_hasPendingManifest;
        if (swappedManifest)
        {
            m_manifest = std::move(m_pendingManifest);
            m_hasPendingManifest = false;
            m_pendingNotifyManifest = true;

        // ── El teclado del creador, re-aplicado en vivo (átomo, CP3) ──
        // Barato-en-vivo como el título: la tabla se re-resuelve entera desde
        // el default (la cascada no acumula sedimento, ley 4), así que sacar
        // una línea del bloque y apretar F8 la saca de verdad.
        aplicarTeclasDelManifest();

        // Capa 1: lo barato-en-vivo. El título es lo único que el Engine consume
        // directo; personajes/tema los realiza la escena al re-parsearse (capa 2).
        if (m_window && !m_manifest.titulo.empty())
            SDL_SetWindowTitle(m_window, m_manifest.titulo.c_str());

        // Capa 1 (A1): la resolución de diseño también es barata-en-vivo.
        // Re-sembrar el holder → el frame siguiente todos los call-sites leen
        // la res nueva (re-escala con F8 sin reiniciar la escena).
        viewport::set(m_manifest.designW, m_manifest.designH);

        // ── El idioma, publicado en el mismo corte que las fuentes ────────
        // Ficha `F8-SIN-IDIOMA`. El horneado de arriba ya usó el candidato;
        // acá sólo se publica el objeto vivo. Recarga SIEMPRE, sin guarda
        // de «no cambió»: es la inversa deliberada de la guarda del selector.
        if (langPlan.accion != LangF8Accion::Nada)
        {
            if (langPlan.preferenciaInvalida)
            {
                std::cerr << "[idiomas] El idioma guardado '"
                          << Settings::storedLanguage()
                          << "' ya no existe en el catalogo. Uso el original '"
                          << langPlan.original << "'." << std::endl;
            }

            const bool nacio = (langPlan.accion == LangF8Accion::Encender);
            if (nacio)
            {
                m_langStore = std::move(candidateLang);
            }
            else if (m_langStore)
            {
                if (langPlan.accion == LangF8Accion::Apagar)
                    m_langStore->load("", langRaiz.string(), langPlan.original);
                else
                    m_langStore->load(langPlan.efectivo, langRaiz.string(),
                                      langPlan.original);
            }

            langMemoLimpiar();
            fontMemoEspejoLimpiar();

            if (nacio && m_langStore)
            {
                langRegistrarActivo(m_langStore.get());
                m_stateStack.notifyLangStoreChanged(m_langStore.get());
            }
        }

        std::cout << "[Engine] proyecto.eclips recargado." << std::endl;
        }

        fontCommitPublisher(m_fontSources);

        if (m_pendingNotifyManifest)
        {
            m_pendingNotifyManifest = false;
            // Capa 2: avisar al state activo para que re-aplique lo derivado del
            // manifest en vivo (VNScriptState re-aplica tema_default sin reset).
            m_stateStack.notifyManifestReloaded();
        }
    }

    // ========================================================================
    // Procesar eventos de SDL
    // ========================================================================
    bool Engine::processEvents()
    {
        bool any = false;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            any = true;   // hubo input → alimenta la ventana de gracia (§6.4)
            // Eventos del sistema primero (quit y resize siempre se procesan)
            switch (event.type)
            {
                case SDL_QUIT:
                    m_running = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                    {
                        m_windowWidth = event.window.data1;
                        m_windowHeight = event.window.data2;
                    }
                    break;

                    // ── Hot-reload del manifest: F8 (Modo Creador / dev-only) ──
                    // Global, antes que el state. F8 libre (VN usa F5/F9/F3) y NO
                    // comparte tecla física con skip (Ctrl). Se consume (continue).
                    // Pointer-safe (§7.36): reloadManifest deja el manifest en
                    // pendiente. El commit único ANTES del render swap+hornea+
                    // notifica a TODO el stack (tema incluido). Sin colgantes y
                    // sin depender de update() — un overlay no deja el VN mixto.
                    // TODO(modo-creador): se compila fuera en build de export.
                    // TODO(input): con InputManager remapeable, binding configurable.
                case SDL_KEYDOWN:
#ifndef ECLIPS_EXPORT_BUILD
                    if (event.key.keysym.sym == SDLK_F8)
                    {
                        static volatile const char kEclipsF8Registered[] =
                            "ECLIPS_F8_REGISTERED";
                        (void)kEclipsF8Registered[0];
                        // reloadManifest es fail-safe y GATED (solo notifica si
                        // hay proyecto.eclips válido). El reload de texturas es
                        // UNGATED: el repaint de un sprite no depende del manifest
                        // ni de que exista. Por eso van como dos señales separadas.
                        reloadManifest();
                        m_stateStack.notifyReloadTextures();
                        continue;
                    }
#endif
                    // ── F10: toggle vsync (perilla de la matriz, CP3) ──
                    // TODO(modo-creador): se compila fuera en export, como F8.
                    // Uncapped revela el costo real por frame y obliga a la
                    // iGPU a subir de reloj (test del inflado del Task Manager).
                    if (event.key.keysym.sym == SDLK_F10 &&
                        event.key.repeat == 0)
                    {
                        // §1.40: sin bool sombra — leer el estado real del
                        // driver, togglear desde ahi, y reportar lo que el
                        // driver dice DESPUES, no lo que asumimos.
                        const int req =
                            (SDL_GL_GetSwapInterval() == 0) ? 1 : 0;
                        if (SDL_GL_SetSwapInterval(req) != 0)
                            std::cout << "[Perf] SetSwapInterval(" << req
                            << ") FALLO: " << SDL_GetError() << std::endl;
                        std::cout << "[Perf] VSync: "
                            << (SDL_GL_GetSwapInterval() == 1
                                ? "ON" : "OFF")
                            << " (driver)" << std::endl;
                        continue;
                    }
            }

            // Pasar evento al estado actual del StateStack
            // Si el estado consumió el evento, no procesarlo más
            bool consumed = false;
            if (!m_stateStack.isEmpty())
            {
                consumed = m_stateStack.handleEvent(&event);
            }

        }
        return any;
    }

    // ========================================================================
    // Getters que delegan al Renderer (tipo completo solo aqui)
    // ========================================================================
    SDL_GLContext Engine::getGLContext() const
    {
        return m_renderer ? m_renderer->context() : nullptr;
    }

    SpriteBatch* Engine::getSpriteBatch()
    {
        return m_renderer ? m_renderer->batch() : nullptr;
    }

    // ========================================================================
    // Limpieza
    // ========================================================================
    void Engine::shutdown()
    {
        // Idempotente: si ya se desmontó, no repetir (evita doble cleanup de
        // GL/SDL si shutdown() se llama explícito Y luego desde ~Engine()).
        if (m_shutdownDone)
            return;
        m_shutdownDone = true;

        // Persistir el progreso de lectura (skip-de-leído) antes de desmontar.
        // Corre tanto en cierre explícito como desde ~Engine(). No-op si nada
        // cambió. Es la durabilidad garantizada al cerrar la app.
        m_seenStore.flush();

        // Limpiar estados
        m_stateStack.clear();

        // ── Render core: desinyectar + Renderer (contexto+batch) ──
        // Orden: g_batch null → renderer->shutdown (batch luego contexto)
        // → null cache → DestroyWindow → SDL_Quit.
        draw2d::setSpriteBatch(nullptr);
        if (m_renderer)
        {
            m_renderer->shutdown();
            m_renderer.reset();
        }
        m_glContext = nullptr;  // cache no-propietario; DeleteContext ya en Renderer

        // TODO: Limpiar audio (miniaudio)

        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
        m_running = false;
    }

    void Engine::quit()
    {
        m_running = false;
    }

} // namespace eclips
