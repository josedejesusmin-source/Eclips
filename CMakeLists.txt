# ============================================================================
# ECLIPS ENGINE - Build Configuration
# ============================================================================
# "Corre en una pluma. Crea sin límites."
#
# Este archivo le dice a CMake cómo compilar el proyecto.
# Visual Studio lo lee automáticamente al abrir la carpeta.
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# VERSION-UNICA: single numeric version source = engine/core/Types.h ENGINE_VERSION
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/engine/core/Types.h" _eclips_ver_line
     REGEX "ENGINE_VERSION[ \t]*=[ \t]*\"[0-9]+\\.[0-9]+\\.[0-9]+\"")
if(NOT _eclips_ver_line)
  message(FATAL_ERROR "Could not read ENGINE_VERSION from engine/core/Types.h")
endif()
string(REGEX REPLACE ".*ENGINE_VERSION[ \t]*=[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\".*" "\\1"
       ECLIPS_ENGINE_VERSION "${_eclips_ver_line}")

project(Eclips
    VERSION ${ECLIPS_ENGINE_VERSION}
    DESCRIPTION "Motor de videojuegos optimizado para Visual Novels y juegos 2D"
    LANGUAGES CXX
)

# ── Estándar C++17 (lo que decidimos en el documento) ──
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Opciones del proyecto ──
option(ECLIPS_BUILD_TOOLS "Compilar herramientas de desarrollo (editor)" OFF)
option(ECLIPS_BUILD_TESTS "Compilar tests" OFF)

# ============================================================================
# Dependencias externas (third_party/)
# ============================================================================
# SDL2 - Plataforma (ventana, input, filesystem)
# Por ahora usamos find_package. Ver docs/SETUP.md para instrucciones
# de instalación de SDL2 en tu sistema.
# ============================================================================

# Intentar encontrar SDL2 instalado en el sistema
find_package(SDL2 REQUIRED)

# Si SDL2 no se encuentra, CMake dará un error claro.
# Ver docs/SETUP.md para cómo instalar SDL2.

# Lua + sol2 (scripting)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

# Glad - Cargador de funciones OpenGL modernas (3.3+).
# Necesario para SpriteBatch y todo render core moderno.
# Compatibility profile: convive con código viejo immediate (Draw2D legacy).
find_package(glad CONFIG REQUIRED)

# ============================================================================
# Archivos del motor
# ============================================================================

# ── Archivos fuente del engine ──
set(ENGINE_SOURCES
    # Core - Game loop, estados, assets
    engine/core/Engine.cpp
    engine/core/StateStack.cpp

    # Render - SDL2 + OpenGL ES 2.0, sprites, texto
    engine/render/Renderer.cpp
    engine/render/SpriteBatch.cpp
    engine/render/TextRenderer.cpp
    engine/render/Texture.cpp
    engine/render/VNDialogBox.cpp

    # Audio - miniaudio
    engine/audio/AudioManager.cpp

    # Input - Sistema de acciones
    engine/input/InputManager.cpp

    # Third party implementations
    third_party/stb_impl.cpp
    third_party/miniaudio_impl.cpp


    # Scripting - Bridge C++/Lua (futuro)
    # engine/scripting/ScriptEngine.cpp
)

# ── Archivos de cabecera del engine ──
set(ENGINE_HEADERS
    engine/core/Engine.h
    engine/core/EclipsParse.h
    engine/core/StateStack.h
    engine/core/GameState.h
    engine/core/Handle.h
    engine/core/Types.h

    engine/render/Renderer.h
    engine/render/SpriteBatch.h
    engine/render/TextRenderer.h
    engine/render/Texture.h
    engine/render/Draw2D.h
    engine/render/SpriteSheet.h

    engine/audio/AudioManager.h

    engine/input/InputManager.h

    engine/core/SaveManager.h
    engine/core/SaveCodec.h
    engine/core/AnimationClock.h
    engine/core/IntroTimeline.h
    engine/core/VNExecutionPolicy.h
    engine/core/WindowResolution.h
    engine/core/DisplayChangeTransaction.h
    engine/core/AssetManager.h

    engine/scripting/LuaManager.h
)

# ============================================================================
# Ejecutable principal
# ============================================================================

add_executable(${PROJECT_NAME}
    main.cpp
    Gameplay2DState.h
    Tilemap.h
    # FadeTransition.h RETIRADO (2026-08-20, `D-15.3`, lote `el-menu-se-pinta-encima`).
    # Cerraba `TRANSICION-DE-ESTADO-SIN-CALLER` (HO-10) y se cerro por DECISION, no
    # por cable: nadie lo incluia -- o sea que esta linea era su unica mencion en todo
    # el arbol y el archivo NO SE COMPILABA-- y como estaba escrito no podia fundir
    # (se declaraba transparente y llamaba a `setupViewport` con el clear puesto, asi
    # que pintaba negro pleno las dos fases). Ademas era el mecanismo VELO, el que el
    # fundador descarto. El guardian del retiro es `harness_transiciones` 10.6.
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

# Que F5 ejecute desde la raíz del proyecto (donde están assets/)
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# /bigobj: necesario cuando los archivos .cpp incluyen headers grandes
# (Gameplay2DState.h, MapData.h, VNScriptState.h). Sin este flag MSVC
# revienta con C1128 al pasar de ~65,536 secciones por .obj. Es estándar
# en proyectos C++ grandes (Boost, Unreal, etc.) y no tiene efectos
# negativos: solo permite .obj más grandes en disco. No afecta al exe.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

# Variante de export (CP-R4): F8 no se registra. Los otros 13
# TODO(modo-creador) quedan RESERVADOS. Preset windows-export / package.bat.
option(ECLIPS_EXPORT_BUILD "Paquete de export: el registro de F8 no se compila" OFF)
if(ECLIPS_EXPORT_BUILD)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ECLIPS_EXPORT_BUILD=1)
endif()

# Incluir carpetas de headers
target_include_directories(${PROJECT_NAME} PRIVATE
target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR})
    ${CMAKE_CURRENT_SOURCE_DIR}          
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party  
)

# Encontrar OpenGL
find_package(OpenGL REQUIRED)

# Linkear SDL2 + OpenGL
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    OpenGL::GL
    glad::glad
    ${LUA_LIBRARIES}
    sol2::sol2
)

# En Windows, copiar SDL2.dll al directorio de salida
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# ============================================================================
# Reglas de instalación (para empaquetador)
# ============================================================================
# Uso: cmake --install build/cmake_Release --prefix build/package/MiJuego
#      o simplemente: package.bat "MiJuego"
# ============================================================================

# Runtime de MSVC junto al exe (vcruntime140.dll, msvcp140.dll…), para que el
# paquete corra AUN en una PC sin el "Visual C++ Redistributable" instalado. Sin
# esto, una PC recién formateada podría pedir esos DLLs de Microsoft al abrir.
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    include(InstallRequiredSystemLibraries)
endif()

# Ejecutable
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
)

# DLLs de runtime junto al exe ────────────────────────────────────────────────
# vcpkg deja junto al exe (en el BUILD) TODAS las DLLs que necesita: SDL2.dll,
# lua.dll, y cualquier dependencia futura. `cmake --install` a un prefix nuevo NO
# arrastra ese applocal deployment, así que lo pedimos explícito — copiando TODO
# `*.dll` que quedó al lado del exe, en vez de derivar cada ruta a mano.
#
# Por qué el glob y no la derivación: la versión anterior derivaba lua.dll de
# `LUA_LIBRARIES` (`.../lib/lua.lib` → `.../bin/lua.dll`) y **se rompía en
# silencio** —FindLua no expone la DLL como target y el `if(EXISTS)` caía al
# `else` mudo—, así que el paquete salía SIN lua.dll y el juego crasheaba con
# "falta lua.dll" en la PC del jugador (§ verificado en `sintaxis-syn2`). El glob
# del build dir toma lo que físicamente está al lado del exe: robusto y
# futuro-proof (una dependencia nueva viaja sola, sin tocar este archivo).
if(WIN32)
    install(CODE "file(GLOB _eclips_runtime_dlls \"$<TARGET_FILE_DIR:${PROJECT_NAME}>/*.dll\")
file(INSTALL \${_eclips_runtime_dlls} DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")")
endif()

# Assets del juego
install(DIRECTORY assets/
    DESTINATION assets
    PATTERN "*.psd" EXCLUDE      
    PATTERN "*.aseprite" EXCLUDE
    PATTERN "*.xcf" EXCLUDE
    PATTERN "Thumbs.db" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE
)

# Scripts EclipScript
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vnscript")
    install(DIRECTORY vnscript/
        DESTINATION vnscript
    )
endif()

# Idiomas: los paquetes .lang + el árbol de assets espejo ─────────────────────
# La carpeta `idiomas/` contiene los paquetes `.lang` y los recursos espejo:
# los `.lang` van directamente en ella, y cada recurso traducido conserva su ruta
# dentro de `idiomas/<código>/<misma ruta>`.
#
# Sin esta regla, `cmake --install` arma un paquete SIN traducciones y sin sus
# assets — y el juego CORRE IGUAL, porque el voto 2 dice que todo miss cae al
# original. O sea que el fallo es MUDO: se ve como un juego que nunca se tradujo.
#
# Faltaba desde CP0 y llevaba cuatro checkpoints sin que nadie la viera: la
# batería no compila CMake, el censo mira `.ecl` y el Release compila pero no
# instala. La cazó la pregunta de cierre del fundador al cerrar CP4.
#
# Los mismos EXCLUDE que `assets/`: el árbol espejo guarda arte, así que puede
# tener los mismos fuentes de edición al lado.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/idiomas")
    install(DIRECTORY idiomas/
        DESTINATION idiomas
        PATTERN "*.psd" EXCLUDE
        PATTERN "*.aseprite" EXCLUDE
        PATTERN "*.xcf" EXCLUDE
        PATTERN "Thumbs.db" EXCLUDE
        PATTERN ".DS_Store" EXCLUDE
    )
endif()

# Archivos .ecl sueltos en raíz
file(GLOB ECL_ROOT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.ecl")
if(ECL_ROOT_FILES)
    install(FILES ${ECL_ROOT_FILES}
        DESTINATION .
    )
endif()

# Manifiesto + temas de CONTENIDO (lista blanca explícita) ────────────────────
# NO usamos glob *.eclips ni tema_*.eclips a propósito: la raíz mezcla tres
# familias de .eclips — contenido (lo que el juego carga), fixtures de prueba
# del motor (tema_test_*, project_720, tema_undertale, test_pasada2…) y ESTADO
# del jugador (save, meta, seen, settings, vn_saves, mi_mundo). Solo viaja el
# contenido que el manifiesto declara. Si cambiás tema_default o sumás un tema
# real al juego (p.ej. tema_undertale para el showcase), agregalo a TEMAS_CONTENIDO.

# Manifiesto del proyecto (bilingüe: gana el que exista)
foreach(MANIFIESTO proyecto.eclips project.eclips)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MANIFIESTO}")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${MANIFIESTO}" DESTINATION .)
    endif()
endforeach()

# Temas de contenido: el mismo manifiesto que el motor (ProjectManifest —
# gana proyecto.eclips sobre project.eclips). Lee tema_default / default_theme
# y instala ese tema. La fuente y la copia pública pueden usar temas distintos,
# sin fijar aquí el nombre de uno.
set(_ECLIPS_MANIFIESTO "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/proyecto.eclips")
    set(_ECLIPS_MANIFIESTO "${CMAKE_CURRENT_SOURCE_DIR}/proyecto.eclips")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/project.eclips")
    set(_ECLIPS_MANIFIESTO "${CMAKE_CURRENT_SOURCE_DIR}/project.eclips")
endif()
set(TEMAS_CONTENIDO "")
if(_ECLIPS_MANIFIESTO)
    file(STRINGS "${_ECLIPS_MANIFIESTO}" _eclips_man_lines)
    foreach(_line IN LISTS _eclips_man_lines)
        if(_line MATCHES "^[ \t]*(tema_default|default_theme)[ \t]+\"([^\"]+)\"")
            set(TEMAS_CONTENIDO "${CMAKE_MATCH_2}")
        endif()
    endforeach()
endif()
foreach(TEMA ${TEMAS_CONTENIDO})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TEMA}")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${TEMA}" DESTINATION .)
    endif()
endforeach()

# Fuentes OFL por NOMBRE (E-27 / fila 17). Misma lista que LISTA_BLANCA.
# NO GLOB ni install(DIRECTORY third_party...): en el lab vive
# HachiMaruPop-Regular.v1.201.ttf (All rights reserved, E-3).
set(FUENTES_OFL
    third_party/fonts/nunito/Nunito-Regular.ttf
    third_party/fonts/nunito/OFL.txt
    third_party/fonts/m_plus_rounded_1c/MPLUSRounded1c-Bold.ttf
    third_party/fonts/m_plus_rounded_1c/MPLUSRounded1c-Regular.ttf
    third_party/fonts/m_plus_rounded_1c/OFL.txt
    third_party/fonts/hachi_maru_pop/HachiMaruPop-Regular.ttf
    third_party/fonts/hachi_maru_pop/OFL.txt
)
foreach(F ${FUENTES_OFL})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${F}")
        get_filename_component(_eclips_font_dir "${F}" DIRECTORY)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${F}" DESTINATION "${_eclips_font_dir}")
    endif()
endforeach()

# Scripts Lua
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lua")
    install(DIRECTORY lua/
        DESTINATION lua
    )
endif()

# Crear carpeta saves vacía
install(DIRECTORY DESTINATION saves)

# ============================================================================
# Mensajes informativos
# ============================================================================
message(STATUS "")
message(STATUS "╔══════════════════════════════════════════╗")
message(STATUS "║         ECLIPS ENGINE v${PROJECT_VERSION}║")
message(STATUS "║  Corre en una pluma. Crea sin límites.   ║")
message(STATUS "╚══════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform:      ${CMAKE_SYSTEM_NAME}")
message(STATUS "")
