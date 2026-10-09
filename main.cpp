// ============================================================================
// ECLIPS ENGINE - Punto de entrada
// ============================================================================
// MODO: Normal — MainMenuState como punto de entrada
// Para activar un test, comenta MainMenuState y descomenta el test deseado.
// ============================================================================

#include "engine/core/Engine.h"
#include "engine/core/Settings.h"

// ── MODO NORMAL ──
#include "MainMenuState.h"
#include "EclipsSplashState.h"

#include "Gameplay2DState.h"

// ── TESTS (descomentar para usar) ──
#include "VNScriptState.h"
// #include "ZLayerTestState.h"     // Test C++ directo de z-layers
// #include "SpriteAnimTestState.h" // Test de sprite sheets
#if __has_include("SondasDelLab.h") || __has_include(<SondasDelLab.h>)
#include "SondasDelLab.h"   // sondas del lab (kProbe*); no sale en LISTA_BLANCA
#endif

#include <iostream>
#include <string>
#include <memory>
#include <SDL.h>

int main(int argc, char* argv[])
{
    // ── Crear el motor ──
    eclips::Engine engine;

    // ── Inicializar (SDL2 + OpenGL + Audio + Input) ──
    // Cargar preferencias del jugador ANTES de crear la ventana, para abrirla
    // YA en el tamaño guardado. Si se crea en un default (1280×720) y luego se
    // redimensiona en vivo (SDL_SetWindowSize con la ventana ya visible), se ve
    // "crecer" — las franjas blancas del área que el GL aún no pintó — justo
    // cuando la intro arranca. Naciendo en el tamaño correcto, no hay redimensión.
    eclips::Settings::load();

    int initW = eclips::Settings::windowSizeUserSet()
        ? eclips::Settings::windowWidth(1280) : 1280;
    int initH = eclips::Settings::windowSizeUserSet()
        ? eclips::Settings::windowHeight(720) : 720;

    // ── Inicializar (SDL2 + OpenGL + Audio + Input) — ventana ya en su tamaño ──
    // VERSION-UNICA: fallback = name + number + optional etiqueta (creator titulo wins in Engine)
    std::string fallbackTitle = std::string(eclips::constants::ENGINE_NAME) + " " + eclips::constants::ENGINE_VERSION;
    if (eclips::constants::ENGINE_ETIQUETA[0] != '\0')
        fallbackTitle += std::string(" ") + eclips::constants::ENGINE_ETIQUETA;
    if (!engine.init(fallbackTitle, initW, initH))
    {
        std::cerr << "[Eclips] Error: No se pudo inicializar el motor." << std::endl;
        return -1;
    }

    // Si Engine reparó una preferencia vieja que ya no cabe en el área útil
    // (cambio de monitor, escala de Windows, barra de tareas), reflejar el W×H
    // efectivo en Settings. Así el siguiente boot nace sano otra vez.
    const int windowedBootW = engine.getWindowWidth();
    const int windowedBootH = engine.getWindowHeight();
    if (eclips::Settings::windowSizeUserSet()
        && (windowedBootW != initW || windowedBootH != initH))
    {
        eclips::Settings::setWindowSize(windowedBootW, windowedBootH);
        eclips::Settings::save();
        std::cout << "[Eclips] Preferencia de ventana reparada y persistida: "
            << windowedBootW << "x" << windowedBootH << std::endl;
    }

    // Aplicar pantalla completa si estaba guardada
    if (eclips::Settings::fullscreen())
        SDL_SetWindowFullscreen(engine.getWindow(), SDL_WINDOW_FULLSCREEN_DESKTOP);

    std::cout << "[Eclips] Motor inicializado correctamente." << std::endl;
    std::cout << "[Eclips] Resolucion de diseno: "
        << eclips::viewport::width() << "x" << eclips::viewport::height()
        << std::endl;

    // Tamaño REAL de la ventana al boot (ya con override/fullscreen aplicados),
    // no el hardcoded de antes que mentía si el jugador cambió la resolucion.
    int bootW = 0, bootH = 0;
    SDL_GetWindowSize(engine.getWindow(), &bootW, &bootH);
    std::cout << "[Eclips] Ventana: " << bootW << "x" << bootH << std::endl;
    std::cout << "[Eclips] Corre en una pluma. Crea sin limites." << std::endl;

#if __has_include("SondasDelLab.h") || __has_include(<SondasDelLab.h>)
    #include "SondasDelLab.h"
    if (eclips::sondas_lab::correrSiPedidas(engine))
        return 0;
#endif
    // ── BOOT: menú manifest-driven (F2 · CP4 Ciclo-de-Vida-B) ──────────────
    //
    // Con la key `menu` (.ecl válido) → el boot pushea ESE .ecl como menú
    // (rol Menu), directo; el MainMenu C++ ni aparece. Sin key (o archivo
    // roto) → MainMenu C++ interno, intacto. El piso 1 garantiza que SIEMPRE
    // se ve algo (commit-check por existencia con resolveScriptPath).
    //
    // Promesa vieja de L60-62 CUMPLIDA en parte: el manifest ya maneja el boot
    // del menú. `script_inicial` conserva su significado —el script del JUEGO,
    // lo que `nuevo_juego` lanzará (CP5)—, NO se recicla como boot-script.
    // `tema_default` ya lo aplica VNScriptState::onEnter (D7).
    //
    // Los params del MainMenu (script/fondo/"ECLIPS") siguen siendo
    // DEFAULTS de "modo test rápido sin proyecto.eclips".
    {
        const eclips::ProjectManifest* manifest = engine.getProjectManifest();
        bool bootedMenuEcl = false;

        if (manifest && !manifest->menu.empty())
        {
            std::string resolved = eclips::resolveScriptPath(manifest->menu, manifest);
            if (!resolved.empty())
            {
                std::cout << "[Eclips] Boot: menu-.ecl '" << resolved
                    << "' (rol Menu)." << std::endl;
                engine.getStateStack().push(
                    std::make_unique<eclips::VNScriptState>(
                        engine.getWindow(), engine.getGLContext(),
                        &engine.getStateStack(),
                        engine.getFontSourceStore(),
                        engine.getInputManagerMutable(),
                        engine.getLangStore(),
                        resolved, -1,
                        manifest, engine.getMetaStore(), engine.getSeenStore(),
                        eclips::VNRole::Menu
                    )
                );
                bootedMenuEcl = true;
            }
            else
            {
                std::cerr << "[Eclips] La key `menu` apunta a un .ecl inexistente: '"
                    << manifest->menu
                    << "'. Fallback al menu principal interno." << std::endl;
            }
        }

        if (!bootedMenuEcl)
        {
            // Sin key `menu` (o archivo roto) → MainMenu C++ (backward compat).
            // Último param ("ECLIPS") = título VISIBLE dentro del menú.
            std::cerr << "[Eclips] Menu C++ de respaldo: sin script ni fondo del lab\n";
            engine.getStateStack().push(
                std::make_unique<eclips::MainMenuState>(
                    engine.getWindow(), engine.getGLContext(),
                    &engine.getStateStack(),
                    engine.getFontSourceStore(),
                    engine.getInputManagerMutable(),
                    engine.getLangStore(),
                    "",
                    "",
                    "ECLIPS",
                    manifest,
                    engine.getMetaStore(),
                    engine.getSeenStore()
                )
            );
        }
    }

    // ── Intro del motor (CP3.2: gating por política F4). Va ENCIMA del menú ya
    //    pusheado: el splash se popea y lo REVELA. La política vive en el manifest
    //    (intro_motor); 'una_vez' persiste vía meta.eclips. ──
    {
        const eclips::ProjectManifest* mf = engine.getProjectManifest();
        eclips::MetaStore* meta = engine.getMetaStore();
        const bool yaVista = meta && meta->has("intro_motor_vista");

        bool mostrarIntro = eclips::introDebeMostrarse(mf, yaVista);
        // 'una_vez' se marca AL ARRANCAR (decisión 'a': la primera ejecución
        // cuenta, la salte o no). Separado de 'mostrar' para que el guard de dev
        // (CP3.3) pueda mostrar-sin-marcar.
        bool marcarVista = mostrarIntro && mf
            && mf->introMotor == eclips::IntroMotor::UnaVez;

        // ── CP3.3 (auto-skip dev) se insertará AQUÍ:
        //    en Debug → mostrarIntro = kForceIntroInDev; marcarVista = false. ──

        if (marcarVista)
            meta->set("intro_motor_vista", "true");

        if (mostrarIntro)
            engine.getStateStack().push(
                std::make_unique<eclips::EclipsSplashState>(
                    engine.getWindow(), &engine.getStateStack()));
    }

    // ── MODO TEST: Gameplay 2D standalone (.eclmap directo) ──
    // Comentado: ahora arrancamos el VN de prueba que LANZA este mapa como
    // slot MIXTO (snapshot GP->VN). Para volver al standalone: descomenta
    // este bloque y comenta el VN de abajo.
    // {
    //     auto gp = std::make_unique<eclips::Gameplay2DState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack()
    //     );
    //     gp->m_mapPath = "test_td_completo.eclmap";
    //     engine.getStateStack().push(std::move(gp));
    // }

    // ── MODO TEST: VN -> Gameplay -> VN (slot mixto, CP3) ──
    // Se usó para validar el slot mixto (snapshot GP->VN). Descomentar para
    // reproducir el round-trip; comentar el standalone de arriba al hacerlo.
    // {
    //     engine.getStateStack().push(
    //         std::make_unique<eclips::VNScriptState>(
    //             engine.getWindow(), engine.getGLContext(),
    //             &engine.getStateStack(),
    //             "test_mixto_save.ecl"
    //         )
    //     );
    // }

    // ── MODO TEST: Bloque 2 — Parallax + Hora + Random Events ──
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_bloque2.ecl"
    //     )
    // );

    // ── MODO TEST: revival de evento_random (tick + retorno + leyes) ──
    // Validado a ojo (2026-07-14): §5.1-5.7 del contrato verdes. Descomentar
    // para reproducir (el guion narra todo en consola).
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_eventos.ecl"
    //     )
    // );

    // ── MODO TEST: micro-lote de higiene VN (tinte= + voces + eco) ──
    // Validado a ojo (2026-07-14): tinte vivo, round-trip, sello. Descomentar
    // para reproducir.
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_microlote.ecl"
    //     )
    // );

    // ── MODO TEST: E-CP2 — efectos sobre fondo/capas + driver una_vez ──
    // Validado a ojo (2026-07-14): perennes, rotura v0, voces. Descomentar
    // para reproducir.
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_ecp2.ecl"
    //     )
    // );

    // ── MODO TEST: E-CP3 — bloques + estatica + sustituto (rotura completa) ──
    // Validado a ojo (2026-07-14): cubitos, estatica, rotura, voces.
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_ecp3.ecl"
    //     )
    // );

    // ── MODO TEST: E-CP4 — driver esporadico (receta `ausencia`) ──
    // Validado a ojo (2026-07-14): ausencia, latigo, voces.
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_ecp4.ecl"
    //     )
    // );

    // ── MODO TEST: CP-BUMP — slot v3 (aceptacion fundadora §10) ──
    // Validado a ojo (2026-07-14): los 8 checks del §10 PASARON. Descomentar
    // para reproducir el round-trip v3.
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_bump.ecl"
    //     )
    // );

    // ── MODO TEST: PERFILADO v2 (sondas medir_*.ecl; descomentar + elegir path) ──
    // Sondas de consumo (evidencia §6): medir_pj / medir_estatica / medir_rotura /
    // medir_4pj / medir_4comp / medir_ancla. Protocolo + tabla + sampler.ps1 en
    // RESUMEN_SESION_PERFILADO_V2. Régimen 720p/1080p vía saves/settings.eclips.
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/medir_4comp.ecl"
    //     )
    // );

    // ── MODO TEST: Metaverse IDE — VN + Pasillo + Batalla ──────────────
//     engine.getStateStack().push(
//         std::make_unique<eclips::VNScriptState>(
//             engine.getWindow(), engine.getGLContext(),
//             &engine.getStateStack(),
//             "metaverse_intro.ecl"
//         )
//     );

    // ── MODO TEST: Capas + Hotspots (Bloque 1) ──
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_capas.ecl"
    //     )
    // );

    // ── MODO TEST: Z-Layer (C++ directo) ──
    // engine.getStateStack().push(
    //     std::make_unique<eclips::ZLayerTestState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack()
    //     )
    // );

    // ── MODO TEST: Sprite Animation ──
    // engine.getStateStack().push(
    //     std::make_unique<eclips::SpriteAnimTestState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack()
    //     )
    // );

    // ── LEFTOVER de harness de test (COMENTADO para CP4/CP5): este segundo
    // push de MainMenuState(test_encadenar) se apilaba ENCIMA del boot de
    // arriba, tapando el menu.ecl (por eso había que dar ESC para verlo).
    // Descoméntalo solo si quieres volver al boot de test_encadenar.
    // engine.getStateStack().push(
    //     std::make_unique<eclips::MainMenuState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_encadenar.ecl",
    //         "assets/test/fondo.png",
    //         "ECLIPS",
    //         engine.getProjectManifest(),
    //         engine.getMetaStore(),
    //         engine.getSeenStore()
    //     )
    // );

    // ── MODO TEST: CP4 — la consola (crear/escribir/imprimir/borrar/limpiar) ──
    // engine.getStateStack().push(
    //     std::make_unique<eclips::VNScriptState>(
    //         engine.getWindow(), engine.getGLContext(),
    //         &engine.getStateStack(),
    //         "vnscript/test_consola.ecl"
    //     )
    // );

    // ── Ejecutar el game loop ──
    engine.run();

    // ── Limpiar y salir ──
    engine.shutdown();

    std::cout << "[Eclips] Motor cerrado limpiamente." << std::endl;
    return 0;
}
