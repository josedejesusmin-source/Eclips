# Eclips Engine 0.9.0 — Technical Preview

> **English:** Eclips is a C++17 engine for visual novels and light 2D games. This is its first
> public preview. The demo can be played in Spanish or English. Its tutorial comments, the two
> manuals (`ECLIPSCRIPT_DOCS.md`, with parts in English, and `VOCABULARIO_2D_ECLIPMAP.md`, for the
> 2D maps) and most of this README are in Spanish; the story of how it was made is translated
> at the end. The engine is MIT; the demo art is CC BY-NC-SA 4.0.

**Este es el árbol de código público de Eclips Engine**: el motor, sus archivos de compilación y una
demo corta.

## Qué trae

- El código del motor, en C++17, en `engine/`
- CMake + `vcpkg.json` + `CMakePresets.json`
- Una demo corta (`project.eclips`, `vnscript/`, `assets/`) con Sol, Luna y Estrella. Sus guiones
  vienen comentados para leerse como tutorial: son el motor en práctica.
- El manual de EclipScript, el lenguaje de los guiones de novela visual, con ejemplos:
  `ECLIPSCRIPT_DOCS.md`. Está en español, con partes en inglés.
- El manual de los mapas 2D (`.eclmap`) y de su Lua: `VOCABULARIO_2D_ECLIPMAP.md`. Está en español.
- Fuentes OFL en `third_party/fonts/` (ver `THIRD_PARTY_NOTICES.md` — **OFL ≠ MIT**)

La meta es que quien crea un juego pueda trabajar cómodo sin tocar C++: con guiones, mapas y temas, y
con Lua cuando haga falta más. Lo que el motor todavía no trae sí podría pedir escribir C++.

## Qué no es

- No es una versión 1.0.
- Todavía no dibuja texto con SDF ni tiene cableadas todas las ranuras de fuente.

## Requisitos y equipos probados

**Los requisitos mínimos de CPU y RAM todavía no están medidos.** Por ahora, estos son los
datos conocidos de la descarga compilada del motor con Testigo:

- **Windows de 64 bits.** La preview se ha probado en Windows 10. La compatibilidad de esta
  descarga con Windows 7, 8 u 8.1 todavía no se ha comprobado.
- **Gráficos y controlador con soporte de OpenGL 3.1**, para la ruta de escritorio.
- **Unos 132 MB libres** para los archivos descomprimidos del motor y la demo, más espacio para
  partidas y proyectos propios.

La he probado en mi PC, con estas características:

| Componente | Equipo de referencia |
|---|---|
| Modelo | HP EliteOne 800 G1 AiO |
| Procesador | Intel Core i5-4590S, 4 núcleos a 3,0 GHz |
| Memoria RAM | 8 GB |
| Gráficos | Intel HD Graphics 4600 |
| Disco | HDD de 500 GB |
| Sistema | Windows 10 Pro de 64 bits |

También la probé en una **tableta Surface 3**. En un tramo de gameplay 2D observé una bajada a
unos **35 FPS**. Todavía no he identificado la causa. El gameplay 2D es la parte menos
desarrollada del motor y seguirá recibiendo mejoras, también de rendimiento.

Estas pruebas describen cómo funcionó la demo en esos equipos. Todavía no permiten fijar los
requisitos mínimos ni garantizar el mismo rendimiento en otros juegos hechos con el motor.
Hay más observaciones en [Límites conocidos](#límites-conocidos).

> **English:** Minimum CPU and RAM requirements have not yet been measured. The compiled
> preview has been tested on 64-bit Windows 10. The desktop graphics path uses OpenGL 3.1.
> Allow about 132 MB for the extracted engine and demo, plus space for saves and your own
> projects. The PC above is a tested reference. On a Surface 3 tablet, one 2D gameplay section
> dropped to about 35 FPS; the cause has not yet been identified. The 2D system will continue
> receiving improvements, including performance work. Windows 7, 8 and 8.1 compatibility has
> not been verified.

## Compilar (Windows)

Hace falta CMake, el toolchain de C++ de MSVC y vcpkg con SDL2 / Lua / sol2 / glad (ver `vcpkg.json`).

```bat
cmake --preset windows-export
cmake --build --preset windows-export
```

O abre la carpeta en Visual Studio (Abrir carpeta) y compila.

El directorio de trabajo tiene que ser la raíz de este árbol: los assets van junto al exe.

## Empaquetar

`package.bat` (opcional) arma una carpeta lista para jugar en `out/package/`.

## Configuración

La pantalla de Configuración está hecha con pantallas autorables (`pantallas { config … }` +
`control`): no es una API fija del motor.

## Límites conocidos

Lo principal de lo que ya sabemos y todavía no se arregla.

1. **Las opciones de `elegir` siempre llevan número** («1. », «2. »…). El tema no tiene cómo
   quitarlo, y el número también sirve para elegir con las teclas 1, 2, 3.
2. **Ninguna transición tapa a los personajes.** Las capas siempre quedan debajo de ellos, aunque
   tengan una `z` alta, y `transicion` sólo sirve al cambiar de `fondo`. *Para esquivarlo:* apagar a
   los personajes con `alpha` mientras entra una capa.
3. **`ocultar` con `efecto: instantaneo` no saca al personaje en el acto**, sino en el cuadro
   siguiente. Un `mostrar` suyo justo después lo revive con sus animaciones viejas, y al adelantar
   (Ctrl) pasa aunque haya un `esperar` corto en medio. *Para esquivarlo:* un bloque `cinematica` y
   `detener_animacion`.
4. **El `.eclmap` no avisa en cinco errores comunes:** un `modo:` mal escrito corre como `topdown`;
   un evento que no existe en `world.on` nunca dispara; una cabecera de bloque con mayúscula no abre
   el bloque; una clave desconocida en un `tile` se pierde, y también una clave con tilde
   (`posicion:` sí, `posición:` no); y lo que cuelga de un bloque desconocido cae a la raíz.
5. **La consola repite el aviso de las letras que le faltan a una fuente**, una vez por cada
   horneado. Con Nunito son dos que la demo no usa (ĳ y ſ): es inofensivo.

6. **El gameplay 2D es la parte menos pulida del motor.** El sistema de novela visual está más
   desarrollado. Lo que hay ahora en 2D son las bases sobre las que quiero seguir trabajando,
   y probablemente cambiará bastante.

   Su rendimiento también necesita trabajo: en un tramo de la demo, mi tableta Surface 3 rondó los 35 FPS,
   mientras mi PC rondó los 60. Conectarla al cargador no cambió el resultado. Todavía tengo que
   investigar la causa y seguir puliendo esta parte del motor.

7. **El brillo del texto todavía necesita trabajo.** Con radios amplios, el halo puede mostrar
   contornos repetidos de las letras en vez de un resplandor suave.
   El sistema de texto seguirá recibiendo mejoras.

   En mi tableta Surface 3, durante una prueba de fuentes con un diseño de 1920×1080, noté un corte
   leve en la parte superior de algunas letras pequeñas. Seguían siendo legibles. Todavía
   falta medir qué lo provoca.

8. **Una vez, el menú se quedó sin opciones al activar la secuencia secreta después de volver
   de la demo.** El poema no llegó a abrirse. Fue un caso aislado, sin una reproducción
   controlada ni una causa confirmada.

9. **El guardado todavía no conserva toda la composición de una escena.** Las capas de texto
   y las consolas pueden faltar al cargar una partida. Las capas de color sí vuelven; si
   guardas mientras una de ellas desaparece, al cargar vuelve ya oculta. El guardado completo
   de estos elementos necesita más trabajo.

10. **`elegir "Pregunta":` ignora la pregunta sin avisar.** Si quieres mostrarla, escríbela
    como una línea de narración antes de `elegir:`. Las opciones del bloque sí se muestran.

11. **`mostrar Sol feliz` sin posición manda a Sol al centro**, aunque ya estuviera en
    otro lugar. Para conservar su ubicación al cambiar la cara con `mostrar`, escribe
    también la posición, por ejemplo `mostrar Sol feliz derecha`.

## Cómo empezó

Eclips empezó el **25 de marzo de 2026** con un documento de diseño, antes que con una línea de
código. Fue pensado para ser un motor de un mod, pero la idea fue creciendo hasta ser un motor abierto,
con licencia MIT. Desde el principio se pensó para juntar dos cosas en un mismo motor, la novela visual y
el gameplay 2D, con una frase: *«Potencia de C++ con la simplicidad de Ren'Py»*. La optimización salió de ahí, de preguntarse
qué es lo mínimo que necesitan una novela visual y un juego 2D para correr. Todavía le falta mucho de
esa optimización.

Lo primero fue un prototipo, con mucho escrito directo en el código, y todavía se encuentra: si un
tramo 2D no trae su `.eclmap`, `loadHardcoded()` en `Gameplay2DState.h` arma el nivel de prueba de
entonces, dibujado con caracteres adentro del código. Su jugador era una bolita, y en el código todavía
se llama así (`m_ballSize`); sin su imagen, hoy se dibuja como un cubo con ojos. Lo mismo pasa con los
menús: si un proyecto no trae los suyos, el motor usa los que tiene escritos en C++. La demo trae los
suyos para el menú, la pausa, la configuración y la carga, en `vnscript/`.

- **25 de marzo de 2026** — el documento de diseño fundacional.
- **De marzo a julio** — el motor se escribe conversando con Claude en la app, primero con
  Sonnet 4.6 y después con Opus 4.6. Cada cambio llega como archivos para copiar sobre el proyecto,
  y se aplica a mano.
- **Desde junio** — Fable y, más tarde, Astra escriben sobre todo los documentos de diseño. No
  estuvieron siempre.
- **14 de julio** — la primera sesión de trabajo en Claude Code.
- **De julio a septiembre** — entran más voces, para implementar y para revisar.
- **18 de septiembre** — el motor pasa a Technical Preview.
- **25 de septiembre** — seis meses desde el documento fundacional.
- **9 de octubre** — ajustes finales y preparación de la primera publicación.

## Cómo fue posible

- **El contrato va antes que el código.** Cada arco empieza por un documento de diseño con sus
  opciones numeradas y una recomendación; lo irreversible lo firma una persona.
- **Primero se mide el código vivo.** Antes de proponer nada se audita lo que existe. Si el plan y el
  código no coinciden, gana el código, y la diferencia se anota.
- **La prueba va primero.** Nace en rojo antes de tocar el motor, y la batería completa corre al abrir
  y al cerrar cada sesión.
- **Quien implementa no custodia su propia prueba**, y otra voz revisa la entrega.
- **Cada error deja una lección numerada.**
- **Un ticket por sesión.** Lo que queda pendiente se anota con la condición que lo dispara.

A septiembre de 2026: unas 82 mil líneas de C++ en 102 archivos, 68 pruebas en la batería, 35
contratos de diseño, 226 resúmenes de sesión y 680 lecciones. Esos documentos del laboratorio no se
publican: este árbol es lo que salió de ellos.

## Créditos

**Dirección, guion y arte:** Jesús.

**Con IA:**

- **Claude** (Anthropic): de Sonnet 4.6 a Opus 5.5, en la app y en Claude Code. Fable 5, en los
  documentos de diseño.
- **GPT** (OpenAI), en Codex: Sol (GPT-5.6), Luna y Astra. Astra, en los documentos de diseño; hizo
  además los sonidos de la demo.
- **Grok** (xAI): Lyra, que implementa; Jarvis, que revisa; y Nexo, que sostiene el trabajo cuando
  faltan los demás.
- **GLM-5.3**: una auditoría a ciegas.

Las bibliotecas y fuentes de terceros tienen su crédito y su licencia en `NOTICE` y
`THIRD_PARTY_NOTICES.md`.

## The story, in English

### How it started

Eclips started on **March 25, 2026** as a design document, before a single line of code. It was meant
to be the engine for a mod, but the idea kept growing until it became an open engine, under the MIT
license. From the start it was meant to bring two things together in one engine, visual novels and 2D gameplay, under one line: *"The
power of C++ with the simplicity of Ren'Py."* The focus on optimization came from there, from asking
what is the least a visual novel and a 2D game need to run. It still has a long way to go on that
front.

It began as a prototype, with a lot written straight into the code, and you can still find it: when a
2D section has no `.eclmap`, `loadHardcoded()` in `Gameplay2DState.h` builds the old test level,
drawn with characters inside the code. Its player was a ball, and the code still calls it that
(`m_ballSize`); without its image, it is drawn today as a cube with eyes. The menus work the same way:
if a project does not bring its own, the engine uses the ones written in C++. The demo brings its own
main menu, pause, settings and load screens, in `vnscript/`.

- **March 25, 2026** — the founding design document.
- **March to July** — the engine is written in conversation with Claude in the app, first with
  Sonnet 4.6 and then with Opus 4.6. Each change arrives as files to copy over the project, and is
  applied by hand.
- **From June** — Fable and, later, Astra write mostly the design documents. They were not always
  around.
- **July 14** — the first working session in Claude Code.
- **July to September** — more voices join, to implement and to review.
- **September 18** — the engine becomes a Technical Preview.
- **September 25** — six months since the founding design document.
- **October 9** — final adjustments and preparations for the first public release.

### How it was possible

- **The contract comes before the code.** Every arc starts with a design document, its options
  numbered and one recommendation; anything irreversible is signed by a person.
- **Measure the live code first.** Before proposing anything, what exists gets audited. If the plan
  and the code disagree, the code wins, and the difference is written down.
- **The test comes first.** It is born red before the engine is touched, and the whole battery runs
  when every session opens and closes.
- **Whoever implements does not guard their own test**, and another voice reviews the delivery.
- **Every mistake leaves a numbered lesson.**
- **One ticket per session.** Whatever is left pending is written down with the condition that
  triggers it.

As of September 2026: about 82 thousand lines of C++ in 102 files, 68 tests in the battery, 35 design
contracts, 226 session summaries and 680 lessons. Those lab documents are not published: this tree is
what came out of them.

## Licencias

- Motor: MIT — `LICENSE`
- Arte de la demo (`assets/demo/`): CC BY-NC-SA 4.0 — ver `NOTICE`
- Terceros: `NOTICE`, `THIRD_PARTY_NOTICES.md`, `third_party/fonts/*/OFL.txt`

## Secreto

Hay un secreto pequeño en el menú de título. Sin spoilers.
