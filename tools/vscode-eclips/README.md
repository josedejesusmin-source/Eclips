# Eclips Engine

Colores y plantillas para escribir proyectos de Eclips Engine en VS Code.

Es su primera versión, y todavía es una vista previa: pinta tus archivos y te da cuatro plantillas para arrancar, pero no ejecuta el juego ni revisa si algo está mal escrito. Para eso sigue estando el motor.

## Instalar la extensión de VS Code

La extensión pone colores y plantillas a tus guiones. Necesita **VS Code 1.139.1 o más nuevo**.
Para jugar basta con abrir el motor; la extensión es para escribir.

### Con el archivo descargado

1. Descarga `eclips-engine-0.1.0.vsix`, el archivo de la extensión.
2. Abre VS Code y presiona `Ctrl+Shift+P`.
3. Escribe **VSIX** y elige **Extensions: Install from VSIX…** (instalar desde VSIX).
4. Selecciona el archivo descargado y espera a que termine.

El zip del motor se extrae; el `.vsix` se elige directamente en VS Code.

### Desde el Marketplace

1. En VS Code, presiona `Ctrl+Shift+X` para abrir Extensiones.
2. Busca **Eclips Engine**, del publicador **sicigia**.
3. Pulsa **Instalar**.

### Comprobar que quedó

En Extensiones, busca **Eclips Engine** entre las instaladas. Luego abre un `.ecl`:
abajo a la derecha debe decir **EclipScript**, y el texto tendrá colores según tu tema.

Si dice «Texto sin formato», haz clic en ese nombre y elige **EclipScript**.
También reconoce mapas `.eclmap`, `project.eclips` y temas como `tema.eclips`.

## Qué archivos reconoce

- Los guiones de novela visual, `.ecl` (EclipScript)
- Los mapas 2D, `.eclmap`
- Los temas: `tema.eclips`, `theme.eclips` y cualquier `tema_algo.eclips`
- El proyecto: `project.eclips` o `proyecto.eclips`

En todos, `#` es comentario, así que el atajo de comentar de VS Code funciona como en cualquier otro lenguaje.

## Cómo pinta

La idea es sencilla: el motor tiene color, lo que se lee en el juego tiene color y tus notas tienen color; tus nombres no.

Los comandos se reconocen por ser la primera palabra del renglón. Solo las palabras de flujo (`escena`, `si`, `sino`, `fin_si`, `elegir`, `ir_a`, `fin` y sus versiones en inglés) llevan una lista aparte.

Los colores exactos los pone tu tema de VS Code, así que en cada quien se ven distintos.

## Las plantillas

Escribe la palabra y elige la sugerencia:

- `escena`, en un `.ecl`: una escena mínima, con su nombre, una narración y su fin
- `personaje`, en el proyecto: el bloque de un personaje, con su nombre visible
- `tema`, en un tema: el color de la caja y el del texto
- `mapa`, en un `.eclmap`: un mapa mínimo en vista cenital, con el jugador en el centro

## Con qué versión va

Va de la mano con Eclips Engine 0.9.0 (Technical Preview): cada ejemplo se prueba contra el lector del motor y contra estos colores. Si el motor cambia algo de lo que la extensión enseña, lo voy a decir en las notas de cada publicación.

Necesita VS Code 1.139.1 o más nuevo.

## Gracias

Gracias por probarla. Es la primera versión, así que si algo se pinta raro o te hace falta algo, cuéntamelo con un issue en el repositorio de GitHub: me ayuda mucho.

https://github.com/josedejesusmin-source/Eclips

## Licencia

MIT, la misma del motor: puedes usarla, copiarla y cambiarla como quieras, siempre que conserves el aviso de la licencia.
