# engine/core/

Sistemas fundamentales del motor. Éstos son los de base; la carpeta tiene más.

- **Engine.h/cpp** — Clase principal. Inicializa SDL2, ejecuta el game loop con Fixed Timestep
- **GameState.h** — Interfaz base para estados de juego (VN, gameplay, menús)
- **StateStack.h** — Pila de estados para transiciones VN ↔ Gameplay
- **Handle.h** — Sistema de IDs seguros para el bridge C++/Lua
- **Types.h** — Aliases de tipos y constantes globales del motor
