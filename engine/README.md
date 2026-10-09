# engine/

El núcleo del motor Eclips, en C++. Los estados grandes (la novela visual, el gameplay 2D, los menús)
y el parser de los guiones viven en la raíz del árbol, junto a `main.cpp`.

## Subcarpetas

- **core/** — Game loop (Fixed Timestep), StateStack, Handle system, tipos fundamentales
- **render/** — SDL2 + OpenGL (3.1 Compatibility en escritorio; OpenGL ES 2.0 de respaldo), SpriteBatch, TextRenderer, FBOs
- **audio/** — Música y efectos de sonido (miniaudio)
- **input/** — Sistema de acciones abstractas (teclado y mouse; el mando y el touch, todavía no)
- **scripting/** — Puente C++ ↔ Lua (sol2, `LuaManager.h`)

La meta es que quien crea un juego pueda trabajar cómodo sin tocar C++: con sus guiones, mapas y
temas (`.ecl`, `.eclmap`, `.eclips`) y, cuando haga falta más, con Lua. Algo que el motor todavía no
trae —generar terreno, por ejemplo— sí podría pedir escribir C++, y para eso está este código.
