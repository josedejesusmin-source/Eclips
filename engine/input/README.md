# engine/input/

Sistema de input basado en acciones.

El código del juego pregunta por acciones ("confirmar", "saltar"), nunca por teclas.
Esto permite añadir gamepad, touch, o remapeo sin cambiar el código del juego.

- **InputManager** — Mapeo de controles a acciones, estados pressed/held/released
