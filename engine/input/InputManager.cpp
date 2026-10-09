// ============================================================================
// InputManager.cpp - la TU que sostiene el header
// ============================================================================
//
// `InputManager.h` es header-only e `inline`, como sus vecinos puros
// (`VNActionSeal.h`, `TrialClock.h`, `AppearPolicy.h`): asi lo puede incluir un
// harness standalone sin arrastrar nada.
//
// Este archivo sobrevive por DOS razones, y ninguna es inercia:
//
//   1. esta listado en `CMakeLists.txt:70`, y borrarlo obligaria a tocar el
//      build en el mismo diff que la primera pieza del atomo de teclado. Un CP0
//      que se declara "off-por-identidad" no es el lugar para mover el build;
//   2. es el unico sitio del proyecto donde el header se compila SOLO. Si
//      alguna vez el header empieza a depender de algo que solo existe porque
//      otro `.h` lo incluyo antes, esta TU se pone roja y lo dice.
//
// Cuando `held()`/`eje()` lleguen (CP1/CP2) van a necesitar funciones de SDL, y
// **esta es su casa**: sacarlas del header mantiene al harness sin linkear SDL.
//
// ── LLEGARON, Y LA FIRMA SE PARTIO EN DOS (CP2, fork 3b) ────────────────────
//
// Lo de arriba se cumplio a medias, y la mitad que cambio importa: lo que vive
// aca abajo es el **CABLE** -traducir un `InputBind` a "esta abajo?" contra
// `SDL_GetKeyboardState`-, no la LEY. La ley es `inputHeldDeTabla` /
// `inputEjeDeTabla`, que se quedaron en el header porque toman el predicado
// como parametro: `harness_juego` §6 las prueba **sin linkear SDL**.
//
// Si esto hubiera bajado entero, la ley del eje -incluido su SIGNO, que falla
// mudo- no la podria custodiar nadie. La regla que sale: **lo que baja a la TU
// es lo que TOCA el mundo, y nada mas.**
// ============================================================================

#include "InputManager.h"

#include <SDL_keyboard.h>
#include <SDL_mouse.h>

namespace eclips {

    namespace {

        // El unico punto del proyecto que traduce un bind a "esta apretado".
        //
        // **La tecla va por SCANCODE, y no es un detalle**: `SDL_GetKeyboardState`
        // se indexa por posicion fisica, asi que el keycode -que es lo que la
        // tabla guarda, porque es lo que el creador escribe- tiene que pasar por
        // `SDL_GetScancodeFromKey`. Indexar el array con el keycode compila,
        // corre, y lee una tecla cualquiera.
        //
        // La rueda contesta `false` siempre y es correcto: una rueda no se
        // MANTIENE, es un evento. Un verbo poleado ligado a `RUEDA_ARRIBA`
        // simplemente nunca esta held.
        bool bindAbajo(const InputBind& b)
        {
            switch (b.kind)
            {
            case InputBind::Kind::Tecla:
            {
                const SDL_Scancode sc = SDL_GetScancodeFromKey(b.tecla);
                if (sc == SDL_SCANCODE_UNKNOWN) return false;
                const Uint8* estado = SDL_GetKeyboardState(nullptr);
                return estado && estado[sc] != 0;
            }
            case InputBind::Kind::Boton:
                return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(b.boton)) != 0;
            case InputBind::Kind::Rueda:
            default:
                return false;
            }
        }

    } // namespace

    bool InputManager::held(const std::string& verbo,
                            const std::string& dominio) const
    {
        return inputHeldDeTabla(m_resuelta, verbo, dominio, bindAbajo);
    }

    float InputManager::eje(const std::string& nombre,
                            const std::string& dominio) const
    {
        return inputEjeDeTabla(m_resuelta, nombre, dominio, bindAbajo);
    }

} // namespace eclips
