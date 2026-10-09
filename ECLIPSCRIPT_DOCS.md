# EclipScript — Referencia Completa / Complete Reference

**Versión:** 0.9.0 Eclips Engine
**Formato:** `.ecl` (texto plano UTF-8)

EclipScript es el lenguaje de scripting de Eclips Engine para crear visual novels y juegos 2D.
Está diseñado para leerse como un guión de cine: sin llaves, sin punto y coma, sin compilar.

EclipScript is Eclips Engine's scripting language for creating visual novels and 2D games.
It's designed to read like a screenplay: no braces, no semicolons, no compiling.

---

## Bilingüe / Bilingual

Todos los comandos se aceptan en **español** e **inglés**. Puedes mezclar ambos idiomas en el mismo script.

All commands are accepted in both **Spanish** and **English**. You can mix both languages in the same script.

```
# Esto es válido / This is valid:
scene intro
personaje luna "assets/luna.png"
show luna center
luna: "Hello!"
fin
```

### Equivalencias canónicas

Las dos formas son **igualmente válidas** — no hay una "correcta" y otra "tolerada". Ejemplos:

| Español | Inglés |
|---------|--------|
| `escena` | `scene` |
| `personaje` | `character` |
| `mostrar` | `show` |
| `ocultar` | `hide` |
| `si` *(valor booleano, no comando)* | `yes` |
| `cargar_tema` | `load_theme` |

> **`si`/`yes` acá no son el condicional.** Son el afirmativo de los flags (`compuesto si`, `composite yes`). El condicional es `si`/`if` — ver **Tabla de Comandos** y §10.
>
> La familia booleana completa que acepta el motor: afirmativo `si` · `sí` · `yes` · `true` · `on` · `1`; negativo `no` · `false` · `off` · `0`. Mayúsculas y acentos no cuentan como grafía distinta.

Elegí la que prefieras y mantenela en tu proyecto por consistencia visual, pero el motor no te juzga por mezclarlas — **de una línea a otra**. Lo único que sí te va a decir algo es escribir las **dos grafías del mismo parámetro en la misma línea**; mirá la nota de abajo.

> **Las comillas no son adorno: dicen "esto es un literal".** Texto que se ve, o una ruta de archivo. Todo lo demás —**los ids que vos inventás**, las palabras del motor, los valores de parámetro— va **desnudo**:
>
> ```
> capa mesa "assets/mesa.png" z=100     ← el id desnudo, la ruta entre comillas
> capa "mesa" "assets/mesa.png" z=100   ← ✗ el motor avisa y descarta la línea
> ```
>
> Vale para **todos** los comandos que llevan id: `capa`, `capa_z`, `capa_visible`, `capa_fade`, `capa_mover`, `texto`, `hotspot`, `enlace`, `control`, `ranura`, `consola crear`, `evento_random`. *(Hasta v0.8.6 algunos toleraban la forma con comillas avisándote que migraras, y `evento_random` al revés las exigía. Desde v0.8.7 hay una sola ley. Si tenías scripts con la forma vieja, el aviso te dice el archivo y la línea.)*
>
> **Y ojo con la diferencia**, porque abajo vas a leer la regla opuesta: un **sinónimo** (`ancho=` y `width=`) sí se acepta en las dos formas, porque son dos maneras de decir **lo mismo**. Una comilla de más **no** dice lo mismo de otra manera: **dice otra cosa** — que lo que sigue es un literal y no un nombre. Por eso una se tolera y la otra no.

> **Las dos grafías de un parámetro, en la misma línea: gana la primera, y el motor te avisa.** Los parámetros `k=v` también son bilingües (`ancho=`/`width=`, `estilo=`/`style=`, `tinte=`/`tint=`, `probabilidad=`/`chance=`, `tamano=`/`size=`…). Si escribís **una**, perfecto, en el idioma que quieras. Si escribís **las dos a la vez**, el motor no adivina: **usa la primera de la lista** —que es siempre la forma en español— y te lo dice con el número de línea.
>
> ```
> capa marco "assets/marco.png" z=5 tinte=#5FF59F tint=#FF0000
> ```
> ```
> [EclipScript] AVISO linea 12: capa: 'tinte=', 'tint=' son el mismo parametro escrito de varias maneras. Se usa 'tinte=#5FF59F'.
> ```
>
> Esa capa queda **verde**, no roja. Y la misma regla vale si repetís la misma clave tal cual (`x=10 x=20` → se usa `10`, con aviso): para el motor es el mismo error —escribiste una cosa dos veces— y te contesta igual en los dos casos.

> **El motor te contesta en la palabra que VOS escribiste** *(v0.8.20)*. Si un valor está mal, el aviso nombra la grafía que tecleaste — no la forma en español a la que el motor la traduce por dentro:
>
> ```
> fuente titulo "assets/fonts/serif.ttf" size=perro
> ```
> ```
> [EclipScript] AVISO linea 4: fuente: size= espera un numero, recibio 'perro'. Se ignora.
> ```
>
> Antes ese aviso decía `tamano=`, una palabra que nunca habías escrito, y buscarla en tu línea no encontraba nada. Vale para las tres familias: los parámetros numéricos, los que llevan color o duración (`tint=`, `chance=`, `every=`…) y los de `animar` (`in=`, `intensity=`).
>
> **Dos excepciones, y las dos a propósito:** cuando el aviso te **enseña** la forma correcta, la enseña en español (*"ej: `tamano=72`"*) — es la canónica, la que conviene aprender. Y las **mayúsculas y los acentos** no cuentan como grafía distinta: si escribís `TAMAÑO=`, el aviso dice `tamaño=`, porque desde v0.8.19 son la misma palabra.

> **Si escribís un parámetro que el comando no conoce, te lo dice — y el resto de la línea sigue funcionando.** Cada comando tiene su propia lista de parámetros, y **no son intercambiables entre comandos**: `capa` mide con `w=`/`h=`, mientras `control` y `ranura` miden con `ancho=`/`alto=`. Escribir `ancho=` en una `capa` no es un typo cualquiera —es una palabra real del motor, de otro comando—, y antes se descartaba en silencio.
>
> ```
> texto titulo "Hola" x=430 y=200 anchoo=200
> ```
> ```
> [EclipScript] AVISO linea 8: texto: no conozco el parametro 'anchoo='. Parametros: x= y= z= color= fuente= estilo= brillo_color= brillo_radio= brillo_intensidad= borde_color= borde_grosor=.
> ```
>
> El aviso trae **la lista de los que sí existen**, así que no hace falta volver acá. Y fijate que el texto **igual se dibuja**: un parámetro que no existe descarta *ese valor*, no el comando — la misma ley que los valores numéricos malos.

> **Y lo mismo vale para los parámetros entre corchetes** (`elegir [tiempo: 5s, defecto: "…"]`, `qte [tecla: E]`, `entrada nombre "…" [maximo: 12]`, `mostrar luna centro [efecto: instant]`). Son otra forma de escribir —se separan por comas y su valor puede ir entre comillas— pero obedecen **la misma ley**: cada comando tiene su lista, la clave que no está en ella te lo dice con la lista al lado, y el comando sigue funcionando.
>
> ```
> elegir [tiempo: 5s, defualt: "Quedarse"]:
> ```
> ```
> [EclipScript] AVISO linea 12: elegir: no conozco el parametro 'defualt:'. Parametros: tiempo: defecto:.
> ```
>
> **Las listas no son intercambiables entre comandos, igual que en los `k=v`.** `tecla:` es de `qte`, `defecto:` es de `elegir` y de `entrada`, `maximo:` es solo de `entrada`, y `esperar_click` **no lleva corchetes** (no tiene reloj: no hay nada que configurarle). Si escribís uno donde no va, el motor te lo dice en vez de tragárselo.

### Acentos y eñes

La regla es una sola, y separa **tus palabras** de **las del motor**:

> **Lo que vos inventás y lo que se ve: UTF-8 libre. El vocabulario del motor: ASCII.**

**Tuyo — acentos y eñes sin problema, siempre funcionaron:**

| Dónde | Ejemplo |
|---|---|
| Nombres de escena | `escena mañana_lluviosa` |
| Alias de personaje | `personaje niña "assets/nina.png"` |
| Ids de capa, texto, hotspot | `capa señal` · `texto año_actual` |
| Variables | `set años_pasados = 3` |
| **Texto que lee el jugador** | `"El niño soñaba con el próximo día."` · `niña: "Mañana será distinto."` |
| Rutas de archivo | `fondo "assets/fondos/mañana.png"` |

**Del motor — los comandos y los parámetros son ASCII:** `escena`, `personaje`, `capa`, `ancho=`, `tamano=`, `parallax=`… **Nunca vas a necesitar una tecla que no tengas** para escribir un comando de EclipScript. Eso es a propósito: hay teclados sin `ñ`, y el lenguaje no te puede pedir una letra que no podés escribir.

**El motor tiene exactamente DOS palabras con acento**, y las dos son alias de cortesía con gemelo ASCII: la clave `tamaño=` (donde `tamano=` y `size=` hacen exactamente lo mismo) y el valor `sí` (donde `si`, `yes`, `true`, `on` y `1` hacen lo mismo). Escribí la que te salga natural.

> **Las mayúsculas del motor bajan solas, acentos incluidos, y en LOS CINCO FORMATOS.** `TAMANO=`, `TAMAÑO=`, `SI`, `SÍ` y `YES` funcionan todas: al comparar su vocabulario, el motor baja las A-Z **y el juego español** (`Á É Í Ó Ú Ü Ñ`). Escribí como te salga.
>
> **La regla, en una frase:** *si la mayúscula no tiene sentido que cambie algo, no cambia nada.* Vale para el nombre de una clave, para un valor del vocabulario y para las claves de tus `.eclips` y `.eclmap` — `RANURA_LAYOUT vertical`, `ranura_layout Vertical` y `Ranura_Layout VERTICAL` son la misma línea. Desde **v0.8.56**; antes esto sólo pasaba en los `.ecl` y en los otros cuatro formatos una mayúscula te daba *«key desconocida»* de una clave que el motor conocía perfectamente.
>
> **Dos cosas que esto NO hace, y conviene saberlas:**
>
> - **No saca acentos.** Bajar la mayúscula de `Ó` te da `ó`, no `o`. Las claves del motor se escriben **sin tilde** a propósito (`caja_aparicion_duracion`, `periodo`, `tamano`), así que la versión con tilde no carga — ni en minúscula ni en mayúscula. La única excepción es donde la tilde **es** la grafía canónica: `tamaño=` y el `sí` booleano, que tienen su alias sin tilde.
> - **No toca lo que vos inventás.** Los nombres que ponés vos —escenas, capas, variables, imágenes, recetas de animación— **sí** distinguen mayúsculas: `MAÑANA` y `mañana` son dos escenas distintas, y `Fondo` y `fondo` son dos imágenes distintas. Es a propósito, y está garantizado por sus propias pruebas. La frontera es simple: **el vocabulario del motor es ciego a las mayúsculas; tus nombres no.**
>
> **Y esto NO toca tu texto ni tus nombres, a propósito.** Ahí `MAÑANA` y `mañana` son **dos ids distintos**, como cualquier par de palabras distintas, y las dos funcionan. Es la misma frontera de siempre: el motor normaliza *sus* palabras, nunca las tuyas — si dos escenas tuyas se llaman parecido, siguen siendo dos.

> **Valores sí/no (booleanos).** Donde un comando espera un sí/no (ej. `parallax si`, `wall_slide no`, `loop: true`), el motor acepta como **afirmativo**: `si`, `sí`, `yes`, `true`, `on`, `1` — y como **negativo**: `no`, `false`, `off`, `0`. **Mayúsculas incluidas, con acento o sin él**: `SI`, `SÍ`, `YES`, `TRUE` funcionan igual. Si escribís un valor que no reconoce (un typo como `parallax sii`), **no falla en silencio**: cae al valor por defecto y deja un aviso en consola diciéndote el valor, el comando y la línea.

> **Valores numéricos.** Donde un comando espera un número (`x=`, `y=`, `w=`, `h=`, `z=`, `ancho=`, `min=`, `max=`, `paso=`, `tamano=`, `parallax=`, `brillo_radio=`…), el motor exige que el valor sea **un número entero**, de punta a punta:
>
> | Escribís | Pasa | Por qué |
> |---|---|---|
> | `x=200` · `x=-100` · `x=1.5` · `x=.5` | ✅ | negativos y decimales son válidos donde el campo los admite |
> | `x=200px` · `x=1.5abc` · `x=8patos` | ❌ | **el motor no se queda con el pedacito que entiende.** Antes `200px` valía 200 sin decir nada |
> | `x=perro` | ❌ | no es un número |
> | `x=nan` · `x=inf` | ❌ | no son cantidades que se puedan dibujar |
> | `z=1.5` | ❌ | **`z` es un campo entero**: un decimal ahí significa que quisiste otra cosa |
>
> En todos los casos ❌ el motor **no falla en silencio**: descarta *ese valor* (el campo queda en su default), deja el resto del comando intacto, y avisa en consola con el comando, el parámetro, lo que recibió y **el número de línea**:
>
> ```
> [EclipScript] AVISO linea 27: capa: x= espera un numero, recibio '1.5abc'. Se ignora.
> [EclipScript] AVISO linea 39: capa: z= espera un entero (sin decimales), recibio '1.5'. Se ignora.
> ```
>
> Dos excepciones, ambas porque la palabra sí significa algo: `ranura ... alto=auto` (mide el contenido) y el `%` de las intensidades (`brillo_intensidad=60%` es lo mismo que `0.6` — pero `60%abc` no vale).

> **Lo que NO se acepta** son variaciones cosméticas de una misma forma (ej. agregar `:` decorativos donde no van). Esas variaciones generan aviso pedagógico en consola y la línea no se carga. Ver siguiente sección. **Las mayúsculas ya no son una de ellas** (v0.8.56): en el vocabulario del motor son la misma palabra, en los cinco formatos. Ver más abajo.

> **Un comando es una palabra completa.** `qte`, `elegir`, `transicion`, `esperar_click` van solos, o seguidos de un espacio y su argumento, de un `[` o `:` (los headers de bloque), o de nada. Si los **pegás** a más texto (`qte_extra`, `transicionrojo`, `esperar_click_2`), el motor **no** los adivina como ese comando: te avisa `linea no reconocida` con su número, en vez de tragarse la línea como si fuera el comando + basura. Es la misma honestidad que la de los `:` cosméticos: una palabra parecida a un keyword no es el keyword.

> **Un comando incompleto no corre a medias.** Si a una línea le falta algo que su comando **necesita** —el nombre (`personaje` sin alias), la ruta entre comillas (`lua`, `cargar_tema`), el operador (`set x`, `si x`), el personaje (`mostrar`, `ocultar`) o el destino (`capa_mover` sin `(x, y)`, o con una coordenada mal escrita)— el motor **no la ejecuta a medias ni inventa un default que haga algo raro**: descarta la línea entera y avisa con su número. La diferencia con los valores numéricos de arriba: ahí falta *un valor opcional* y el comando sigue sin ese campo; acá falta *lo que le da sentido al comando*, así que el comando no se emite. (Ejemplo del matiz: `capa cuadro x=perro` dibuja la capa sin mover su `x`; pero `capa_mover cuadro a (perro, 150)` **no mueve nada** —el destino es lo que ese comando hace, no un extra—.) Y una escena repetida (`escena X` dos veces en el mismo archivo) **no borra la primera**: se conserva la primera y se ignora la segunda, con aviso.

> **Podés comentar al final de una línea.** Un `#` **con un espacio antes** empieza un comentario, y el motor ignora todo lo que sigue — funciona en cualquier comando, incluidos los que van solos (`ocultar_caja`, `esperar_animacion`, `fin_si`) y las opciones dentro de un `elegir` o un `qte`:
>
> ```eclips
> ocultar_caja                      # el menú no lleva barra de diálogo
> esperar 1.2                       # un respiro antes del corte
> elegir:
>   "Quedarse" -> patio             # la opción segura
>   "Correr"   -> pasillo
> ```
>
> **Un `#` pegado a lo que tiene a la izquierda NO es un comentario: es parte de lo que estás escribiendo.** Por eso los colores siguen funcionando igual, hasta en la misma línea que un comentario:
>
> ```eclips
> fondo "assets/bg/club.png" tinte=#3355aa   # el azul de la noche
> texto rotulo "Bug #3: sigue aquí" color=#5FF59F
> ```
>
> La regla completa, en una frase: **un `#` con espacio antes (o al empezar la línea), y fuera de comillas, abre un comentario; cualquier otro `#` es texto tuyo.** Dentro de comillas nunca hay comentarios — un `#` en un diálogo, en un texto o en una ruta se ve tal cual. Y si te falta cerrar una comilla, el motor **no** corta nada: te avisa de la comilla, que es el problema de verdad.
>
> *(Una línea que empieza con `#` sigue siendo un comentario entero, como siempre.)*
>
> ⚠️ **Esta regla es de los `.ecl`. Los archivos de tema y `project.eclips` tienen la suya**, porque ahí los colores se escriben **sueltos**, sin `=`: `boton_fondo     #2E0B3D    # (comentario)`. En esos archivos un `#` abre comentario **salvo que lo siga un dígito hex** — así que el color con espacio antes está a salvo, pero `#final` o `#abc` **no** son comentarios ahí (la `f` y la `a` son dígitos hex), y un `#` dentro de comillas **sí** corta. Los dos formatos los leen dos parsers distintos y por eso pueden tener leyes distintas; en la práctica, en un tema **dejá un espacio y una palabra que no empiece con letra hex** después del `#` y no te vas a topar con la diferencia.

---

## Sobre los `:` en EclipScript

Los dos puntos en EclipScript significan **"lo que sigue es contenido o un bloque multilínea"**:

- **Diálogos** llevan `:` → `luna: "Hola"`
- **Bloques** llevan `:` → `elegir:`, `esperar_click:`
- **Comandos con argumentos** NO llevan → `fondo "x.png"`, `mostrar luna`

En archivos `.eclips` (themes y proyecto) no hay diálogos ni bloques
multilínea, así que **no se usan `:`**. Si por costumbre escribís uno, el motor
lo rechaza con un aviso explicativo y la línea no se carga:

```
[.eclips] Aviso en 'tema_pastel.eclips' (key 'caja_color:'):
Los .eclips no usan ':' (eso es solo para .ecl).
Linea ignorada. Quita el ':' y vuelve a correr.
```

Esto es intencional: cada formato del motor mantiene sus reglas claras y
consistentes. Un `.eclmap` es otro lenguaje: ahí `:` separa propiedad y valor
(`modo: topdown`) y abre bloques indentados (`enemigo E ...:`,
`marcador paso_central:`). No apliques la regla de `.eclips` al mapa.

## Comillas en .eclips

En un `.eclips` (theme o proyecto) las **comillas marcan el texto que se ve** y las rutas de archivo; los valores "tipados" del motor van **desnudos**:

- **Con comillas** (texto de pantalla o ruta):
  - `ranura_vacia_etiqueta "empty slot"`
  - `ranura_fecha_formato "{dia_pad}/{mes_pad}/{anio}"`
  - `ranura_sprite "assets/ui/slot.png"`
- **Sin comillas** (color, número, porcentaje, palabra-clave):
  - `ranura_fondo_color #1a1a2e`
  - `ranura_borde_grosor 2`
  - `caja_opacidad 92%`

Es la misma regla del `.ecl` (un diálogo o un narrador llevan comillas; un nombre de capa no), traída al theme. Si te olvidás las comillas en un valor de texto, el motor **no lo aplica** (queda el default) y te avisa por consola con un ejemplo — igual que con los `:`:

```
[.eclips] Aviso linea 42 en 'tema_pastel.eclips' (key 'ranura_vacia_etiqueta'):
El texto que se ve va entre comillas (ej: ranura_vacia_etiqueta "empty slot").
Valor ignorado (queda el default). Pon comillas y vuelve a correr.
```

Una sola forma correcta: el texto entre comillas se ve tal cual; sin comillas, no se aplica.

### Ejemplos rápidos

```
.ecl — el ':' es semántico
escena intro          ← sin ':', es comando
fondo "bg.png"        ← sin ':', es comando
luna: "Hola"          ← CON ':', introduce diálogo
elegir:               ← CON ':', abre bloque
"Sí" -> escena_a
fin

.eclips — sin ':' nunca
titulo "Mi Juego"      ← correcto
caja_color #FAE0E8     ← correcto
titulo: "Mi Juego"     ← rechazado con aviso

.eclmap — ':' pertenece a su gramática declarativa
modo: topdown
jugador:
  sprite: "assets/player/"
  fps: 8
marcador paso_central:
  rectangulo: 240, 180, 60, 60
habilidad pulso:
  alcance: 300
  daño_directo: 2
  preferencia: mas_cercano
  adquisicion:
    relacion: opuesto
    capacidad: recibe_dano
    vinculo: obstructor_locomocion_actual
  coordinacion:
    daño_directo_comprometido: omitir_objetivo_cubierto
```

La referencia completa de `.eclmap` —el mapa, los actores, las habilidades, los hooks
de Lua como `marker_enter(marker_id, actor_kind, actor_id)` y los frames direccionales
numerados (`left_1.png`, `left_2.png`, ...) con `fps:` por jugador— vive en
`VOCABULARIO_2D_ECLIPMAP.md` (el manual del 2D, que viene junto a éste).

---

## Tabla de Comandos / Command Table

| Español | English | Descripción |
|---------|---------|-------------|
| `escena` | `scene` | Declarar una escena |
| `personaje` | `character` | Definir un personaje |
| `personaje_compuesto` | `composite_character` | Personaje con capas (body+face) |
| `fondo` | `background` | Cambiar imagen de fondo (ruta `"..."` o nombre lógico) |
| `mostrar` | `show` | Poner personaje en pantalla |
| `ocultar` | `hide` | Quitar personaje de pantalla |
| `musica` | `music` | Reproducir música (loop) |
| `sonido` | `sound` | Reproducir efecto de sonido |
| `parar_musica` | `stop_music` | Detener la música |
| `transicion` | `transition` | Efecto visual al cambiar fondo |
| `ir_a` | `goto` | Saltar a otra escena |
| `fin` | `end` | Terminar el script |
| `elegir` | `choose` | Mostrar opciones al jugador. Pegado al final de una línea (`"texto" elegir:`) las opciones llegan **con** la línea — §14.5.ter. Acepta también renglones `secuencia`/`sequence` - §11. |
| `mutar` | `set` | Asignar/modificar variable |
| `meta_mutar` | `meta_set` | Asignar/modificar variable **persistente** (`meta.eclips`): sobrevive a borrar la partida — ver sección 28 |
| `meta_borrar` | `meta_reset` | Vaciar **todo** el progreso permanente (`meta.eclips`) — ver sección 28 |
| `entrada` | `input` | Pedir texto al jugador (nombre, etc.) y guardarlo en variable |
| `si` | `if` | Condicional |
| `sino` | `else` | Rama alternativa |
| `fin_si` | `end_if` | Cerrar condicional |
| `qte` | `qte` | Quick-Time Event |
| `cargar_script` | `load_script` | Encadenar otro script .ecl |
| `cargar_tema` | `load_theme` | Cambiar el tema visual mid-script (la ruta va entre comillas: `cargar_tema "temas/noche"`) |
| `capa` | `layer` | Agregar capa con z-order (imagen **o color sólido**: `capa x "#000000"`) |
| `texto` | `text` | Texto posicionable como capa |
| `fuente` | `font` | Declara una fuente nombrada (ttf + tamaño) para texto-capa |
| `estilo` | `style` | Paquete reutilizable de fuente+color+glow (en `proyecto.eclips`); se usa con `texto ... estilo=N` |
| `capa_z` | `layer_z` | Cambiar z-order en runtime |
| `capa_visible` | `layer_visible` | Mostrar/ocultar capa |
| `capa_fade` | `layer_fade` | Fade-in/out de una capa (texto o imagen) |
| `capa_mover` | `layer_move` | Mover una capa a (x,y): una sola vez o en loop |
| `consola crear` | `console create` | Crear una consola (terminal): rect estilado con buffer de texto |
| `consola_escribir` | `console_write` | Appendea texto que se **teclea** (typewriter) |
| `consola_imprimir` | `console_print` | Appendea texto **de golpe** (output del sistema) |
| `consola_borrar` | `console_erase` | Retract: borra N chars (o `todo`) char-por-char |
| `consola_limpiar` | `console_clear` | Vacía el buffer (instantáneo) |
| `hotspot` | `hotspot` | Definir zona clickeable |
| `esperar_click` | `wait_click` | Esperar click en hotspot |
| `esperar_interfaz` | `wait_ui` | Mantener una interfaz viva y focable, sin diálogo ni duración |
| `enlace` | `link` | Volver un `texto` clickeable (menús, "OK") |
| `evento_random` | `random_event` | Definir evento aleatorio (con gate opcional `[si:]`, v0.8.18; ver sección 25) |
| `azar [repetir=si\|no]:` | `random [repeat=yes\|no]:` | Elegir UNA de varias salidas por peso (v0.8.12; ver sección 25.5) |
| `palabras N [k=v]:` | `words N [k=v]:` | Pool de palabras clickeables: muestra N al azar y el jugador elige **una** (v0.8.45; ver sección 25.6) |
| `iniciar_eventos` | `start_events` | Activar sistema de eventos |
| `detener_eventos` | `stop_events` | Desactivar sistema de eventos |
| `azar_semilla` | `random_seed` | Fijar la semilla del azar (sin número: azar de verdad) |
| `azar_traza` | `random_trace` | Consola: un renglón por ensayo, diciendo qué pasó |
| `parallax` | `parallax` | Toggle global del efecto parallax |
| `vigneta` | `vignette` | Viñeta (oscurecido de bordes): `vigneta 0.8`, con fade `vigneta 0.8 2s`, apagar `vigneta off` |
| `resaltar_hablante` | `highlight_speaker` | Highlight del personaje que habla |
| `deshabilitar` | `disable` | **Apagar acciones del jugador** (save, load, historial, menú, config, auto, skip) — ver 14.5.quater |
| `habilitar` | `enable` | Devolverlas — refina un `deshabilitar` anterior |
| `ocultar_caja` | `hide_box` | Esconder UI del diálogo (cinemáticas) |
| `mostrar_caja` | `show_box` | Volver a mostrar la UI del diálogo — y pedir que **sobreviva a un `elegir`** (v0.8.29; ver 14.5.bis) |
| `usar_caja` | `use_box` | Cambiar plantilla de caja mid-escena |
| `lua` / `ejecutar_lua` | `lua` | Ejecutar script Lua |
| `efecto_lua` | `lua_effect` | Declarar un efecto de render Lua (v0.8.2; ver sección 16) |
| `animar` | `animate` | Aplicar animación/efecto a personaje o pantalla |
| `animacion` | `animation` | Definir una animación NOMBRADA (canales + efectos, reutilizable) |
| `detener_animacion` | `stop_animation` | Detener animación activa |
| `esperar_animacion` | `wait_animation` | Esperar a que termine una animación |
| `esperar` | `wait` | Pausa por N segundos |
| `cinematica` | `cinematic` | Bloque que sella el input (skip/save) para que una secuencia corra completa |
| `ambito` | `scope` | Bloque léxico nombrado para retirar sus recursos al reentrar |
| `control` | `control` |  Widget de setting (slider o toggle) atado a un valor del motor |
| `ranura` | `slot` |  Tarjeta de slot de guardado (carga/guarda/borra a slot N) — Capa 2 |

## Ámbitos nombrados / Named scopes

`ambito <nombre> {` y `scope <nombre> {` abren el mismo bloque léxico nombrado.
La llave de cierre es `}`:

```eclips
escena pagina_a
  ambito pagina {
    capa panel_a "#AA2233" x=250 y=100 w=780 h=520 z=10
    texto rotulo_a "PAGINA A" x=520 y=280 z=20
    hotspot page_action x=0 y=0 w=640 h=720
    esperar_click:
      "page_action" -> pagina_b
  }
```

Al entrar a otro bloque con el mismo nombre, el motor retira atómicamente la
generación anterior: capas, textos, consolas, tweens, pins, hotspots,
mappings, controles, widgets de tecla, ranuras, thumbnails y su foco/hover/
drag/captura derivados. Las altas escritas dentro de las llaves pertenecen a
la página; las altas fuera de un ámbito pertenecen al host y siguen vivas. Un
salto válido abandona la adscripción actual, pero no borra el ámbito; un
destino inexistente no retira la página actual.

---

## 0. Tu primer `.ecl` / Your first `.ecl`

Antes de la sintaxis de §1: el umbral es **hacer que el motor corra tu archivo**.
Tres piezas del manifest (`proyecto.eclips` / `project.eclips`) y un botón del menú.

Before §1's syntax: the threshold is **getting the engine to run your file**.
Three manifest fields and one menu button.

### 1. Poné el archivo donde el motor lo busca

1. Creá `hola.ecl` (UTF-8) con al menos una `escena` y un `fin`.
2. Guardalo en la carpeta de scripts — casi siempre la de `script_dir`.

En un `project.eclips` típico:

```
script_dir "vnscript/"
```

→ el archivo vive en `vnscript/hola.ecl`, relativo a la **raíz del proyecto** (el directorio de trabajo del motor, no una ruta absoluta inventada).

Si no hay manifest, el motor igual busca en la **raíz** y en `vnscript/` (ver §27 *Sin manifest = todo opcional*).

### 2. Decile al motor *qué* `.ecl` es el juego

En `proyecto.eclips` / `project.eclips` (raíz del proyecto):

```
script_dir "vnscript/"
script_inicial "hola.ecl"
```

| Campo | Qué es |
|-------|--------|
| `script_dir` | Carpeta donde el motor busca `.ecl` si no están en la raíz. |
| `script_inicial` / `initial_script` | El `.ecl` del **juego** — lo que lanza **Nueva Partida**. |
| `menu` | El `.ecl` de **boot / pantalla de inicio**. **No** es lo mismo que `script_inicial`. |

`menu` y `script_inicial` son independientes (ver *Tu menú es una escena* y §27):

- **`menu "….ecl"`** → al abrir el motor corre ESE script como menú.
- **`script_inicial "….ecl"`** → se carga cuando el jugador elige **Nueva Partida** (`nuevo_juego` / `new_game`).
- **Sin `menu`** → menú interno del motor; igual podés usar `script_inicial`.

Toque mínimo al manifest: **cambiá solo** `script_inicial` (una línea). No hace falta tocar `menu`, tema, ni personajes para una prueba.

Ejemplo (campos reales de un proyecto en marcha):

```
script_dir "vnscript/"
script_inicial "hola.ecl"    # ← lo único que necesitás editar para probar
menu "aviso.ecl"             # boot/menú; dejarlo como está
```

### 3. Arrancá y elegí Nueva Partida

1. Compilá / ejecutá el motor como siempre (p. ej. **F5** en Visual Studio).
2. En el menú (o tras el aviso de boot), elegí **Nueva Partida**.
3. Eso dispara `nuevo_juego` → carga `script_inicial`.

Atajo de laboratorio (opcional): apuntá `menu` al mismo `.ecl` para bootearlo sin pasar por el menú. **Restauralo** después; no es el flujo de un juego publicado.

### Lo que F8 **no** hace

**F8 no recarga el `.ecl` en ejecución.** El hot-reload actualiza texturas, tema y (desde v0.8.41) el catálogo de idiomas — ver **Notas Técnicas** al final del manual.

Para ver cambios en el guion: guardá el `.ecl` → volvé al menú → **Nueva Partida** de nuevo (o recargá partida con L / F9). Editar el `.ecl` y seguir en la partida ya cargada **no** cambia el script en ejecución.

> **Por qué no, y por qué no es un olvido:** tu posición en el guion se guarda como un **índice** (escena + número de comando), y así viaja también a los saves. Si el motor re-parseara el archivo mientras jugás, una línea agregada más arriba correría todo lo de abajo un lugar: saltarías a otro comando, y **las partidas guardadas apuntarían a la línea equivocada**. Recargar guion en caliente necesita un ancla que sobreviva a la edición — es diseño pendiente, no plomería faltante.

### Mapa rápido

```
project.eclips
  script_dir      → dónde están los .ecl
  menu            → pantalla de inicio (boot)
  script_inicial  → lo que lanza "Nueva Partida"
       ↓
  vnscript/hola.ecl   ← tu guion
```

Detalle completo de campos: **§27. El archivo `proyecto.eclips`**.
Verbos `nuevo_juego` / `fin` / `titulo`: **Tu menú es una escena → Verbos de ciclo de vida**.

---

## 1. Estructura Básica / Basic Structure

Todo script necesita al menos una **escena**. Los comandos van dentro de escenas.

```
# Los comentarios empiezan con #
# Comments start with #

escena inicio
  luna: "Hola mundo."      # también al final de una línea
fin
```

Equivalente en inglés:

```
scene start
  luna: "Hello world."
end
```

> **Sin `personaje` / `character`:** la línea `luna: "…"` **igual se muestra**. La nameplate usa el alias tal cual (`luna`). Lo que **sí** exige declaración (en el `.ecl` o en el manifest, §27) es `mostrar`/`show` — sin sprite declarado no hay nada que poner en pantalla. Para un nombre visible distinto del alias («Luna»), ver *Nameplate* en §2 y el campo `nombre`/`name` en §27.

### Reglas generales:
- Una instrucción por línea
- Espacios al inicio se ignoran (usa indentación libre)
- Líneas vacías se ignoran
- `#` inicia un comentario (toda la línea)
- Las rutas de archivo van entre comillas: `"assets/fondo.png"`
- Los textos de diálogo van entre comillas: `"Hola mundo."`

---

## 2. Personajes / Characters

Define personajes antes de usarlos. Pueden ir antes de la primera escena o dentro de una.

```
personaje luna "assets/personajes/luna.png"
personaje sol  "assets/personajes/sol.png"
```

```
character luna "assets/characters/luna.png"
character sol  "assets/characters/sol.png"
```

- El **alias** (`luna`) es el nombre que usas en el script
- La **ruta** es la imagen base del personaje (PNG recomendado, con transparencia)
- Un personaje puede tener múltiples **expresiones** (ver sección 7)

### Nameplate: el `.ecl` pone el sprite; el nombre visible viene del manifest

`personaje hero "assets/personajes/heroe.png"` (o `character …`) registra el **alias** y la **imagen**. Eso alcanza para `mostrar hero` y para hablar con `hero: "…"`.

La **placa de nombre** (nameplate) **no** toma un display name de esa línea. El nombre bonito sale **solo** del manifest:

```
# en proyecto.eclips / project.eclips
personaje hero {
    nombre "Héroe"          # ← esto ve el jugador en la placa
    path "assets/personajes/heroe.png"
}
```

(Equivalente EN: `character hero { name "Hero" … }`.)

| Qué escribís | Qué controla |
|--------------|--------------|
| `personaje alias "ruta.png"` en el `.ecl` | Sprite (y que `mostrar` funcione) |
| `nombre` / `name` en el bloque del manifest | Texto de la nameplate |
| Sin `nombre` en el manifest (o sin manifest) | La placa muestra el **alias crudo** (`hero`, no «Héroe») |

Declarar el personaje en el `.ecl` **no cambia** el texto de la nameplate; cambia si podés hacer `mostrar`.

> **Y esta mitad sí es caliente:** el nombre de la placa se re-resuelve contra el manifest en cada **F8**, así que podés editar `nombre` y verlo cambiar sin reiniciar. El guion no se recarga (ver §0), pero el nombre sí. Detalle de campos: **§27 — bloque `personaje { … }`**.

---

## 3. Personajes Compuestos / Composite Characters

Los personajes compuestos son personajes cuyo sprite se arma a partir de **capas separadas** (body + face), como hacen muchas novelas visuales. Esto permite cambiar la expresión facial sin necesitar un sprite completo por cada combinación.

Composite characters have their sprites assembled from **separate layers** (body + face), as many visual novels do. This allows changing facial expressions without needing a full sprite for every combination.

### Definir personaje compuesto

```
personaje_compuesto luna "assets/luna/"
composite_character luna "assets/luna/"
```

- La **ruta** apunta a una **carpeta** (no a un archivo). Debe terminar en `/`.
- Dentro de la carpeta van los sprites de partes con sufijo numérico/nombre.

### Modo 2 capas (body + face)

Los archivos siguen el patrón `body_face.png`. Un guión bajo = 2 capas.

```
assets/luna/
  1r_a.png      ← body "1r" + face "a"
  1r_b.png      ← body "1r" + face "b"
  2l_a.png      ← body "2l" + face "a"
```

Uso en script:

```
mostrar luna 1r_a centro         # body=1r, face=a
luna b: "..."                  # cambia solo face → 1r_b
mostrar luna 2l_a izquierda      # cambia body y face
```

### Modo 3 capas (left + right + face)

Los archivos siguen el patrón `left_right_face.png`. Dos guiones bajos = 3 capas.

```
assets/luna/
  1l_1r_a.png   ← left "1l" + right "1r" + face "a"
  1l_2r_b.png   ← left "1l" + right "2r" + face "b"
```

Uso en script:

```
mostrar luna 1l_1r_a centro      # left=1l, right=1r, face=a
luna b: "..."                  # cambia solo face → 1l_1r_b
```

### Sprite completo en personaje compuesto

Si el nombre de expresión tiene **cero guiones bajos**, se trata como sprite completo (no compuesto). El body/left/right anterior se descarta.

```
mostrar luna 3a centro           # sprite completo "3a.png", sin capas
```

Para volver a modo compuesto, simplemente usa una expresión con guiones bajos:

```
mostrar luna 1r_a centro         # vuelve a modo 2 capas
```

### Detección automática

El motor detecta el modo automáticamente por conteo de guiones bajos en el nombre de expresión:

| Guiones bajos | Modo | Ejemplo |
|---------------|------|---------|
| 0 | Sprite completo | `3a` |
| 1 | 2 capas (body + face) | `1r_a` |
| 2 | 3 capas (left + right + face) | `1l_1r_a` |

Un personaje compuesto puede cambiar entre los tres modos dinámicamente durante el juego.

### Heurística por convención (sintaxis de diálogo)

En la **sintaxis de diálogo** (`personaje EXPR: "..."`, atajo de la sintaxis explícita `mostrar` + diálogo), el motor distingue entre cambio de pose y cambio de cara por el **primer carácter** del nombre:

| Primer carácter de EXPR | Interpretación | Ejemplos |
|-------------------------|----------------|----------|
| Dígito (`0-9`) | Pose: aplica conteo de guiones bajos para decidir modo | `l 3a:`, `l 1r_a:`, `l 1l_1r_a:` |
| Letra (`a-z`) | Cara: cambia solo la cara, mantiene cuerpo/capas activas | `l b:`, `l feliz:` |

```
mostrar l 1l_1r_a centro     # 3 capas: pose explícita
l b: "..."                   # cara solo: empieza con letra
l 3a: "..."                  # sprite completo: empieza con dígito
l 1r_a: "..."                # 2 capas: empieza con dígito
```

Esta convención es común entre creadores de novelas visuales. Para nombrar tus poses y caras: **dígitos para poses**, **letras para caras**.

**Caso atípico:** si tu sprite se llama con palabra (`boss.png`, `hero_form.png`), la sintaxis de diálogo lo trata como cara. El escape es usar el comando explícito antes:

```
mostrar l boss centro
l: "Veniste."
```

El comando `mostrar` mantiene semántica fija sin heurística:
- `mostrar l EXPR posicion` siempre interpreta `EXPR` por conteo de guiones bajos.

### El modelo completo: dos preguntas

Las dos tablas de arriba son, en realidad, **dos preguntas que el motor se hace en orden** cada vez que escribís `luna X` (sea en `mostrar` o en el atajo de diálogo `luna X:`):

**Pregunta 1 · ¿Le pusiste nombre a este look?**
Si definiste una **apariencia** con ese nombre (`apariencia X ...` en el bloque del personaje — ver **Apariencias**, más abajo), el motor cambia `X` por su cadena completa (`feliz` → `2l_1r_c`) y sigue con **esa**. Si `X` no es una apariencia, lo usa tal cual.

**Pregunta 2 · ¿Qué forma tiene el nombre ya resuelto?**
Sobre el resultado de la pregunta 1, decide por el primer carácter:

- empieza con **letra** (`b`, `feliz`) → **solo la cara** (conserva el cuerpo actual)
- empieza con **dígito** → una **pose**: cuenta los `_` (0 = sprite completo · 1 = cuerpo+cara · 2 = izq+der+cara)

```
# con 'apariencia feliz 2l_1r_c' y 'look_default neutral' definidas:
mostrar luna            # sin EXPR → look_default (neutral) → pose completa
luna feliz: "¡Hola!"    # 'feliz' es apariencia → 2l_1r_c → pose completa
luna b: "..."           # 'b' no es apariencia → letra → solo la cara, sobre el cuerpo de feliz
```

**La regla de oro:** una apariencia nombrada **gana** sobre la heurística. El "letra = cara, dígito = pose" solo aplica a nombres que **no** son apariencias.

> **⚠ Cuidado — "solo la cara" necesita un cuerpo en pantalla.**
> Cambiar solo la cara (`luna b:`) funciona **únicamente si el personaje ya está mostrado como una pose compuesta**. Si su estado actual es un **sprite completo** (un nombre de 0 guiones como `3a`), no hay cuerpo sobre el cual poner la cara y el motor **ignora la línea** — la cara no cambia:
>
> ```
> luna 3a: "..."   # sprite completo → sin cuerpo separable
> luna b:  "..."   # IGNORADO: no hay cuerpo → la cara no cambia
> ```
>
> **Escapes:** usá otro sprite completo (`luna 3b:`) o volvé a una pose compuesta (`luna 1r_b:`) antes de cambiar caras.
>
> De arranque, además: `mostrar luna` **sin** EXPR y **sin** `look_default` no dibuja nada — dale un look (una pose explícita o un `look_default`).

### Notas técnicas

- Las capas se componen usando FBO (Framebuffer Object) a resolución nativa para evitar costuras visuales al escalar la ventana. Si FBO no está disponible, usa render directo como fallback.
- El cambio de cara en la sintaxis de diálogo (EXPR que empieza con letra) solo cambia la cara, manteniendo body/left/right actuales.
- Save/load preserva el estado completo del personaje compuesto (modo, partes activas).

---

## 4. Escenas / Scenes

Las escenas son bloques de comandos con un nombre único. Son los "puntos de anclaje" del script.

```
escena inicio
  # comandos aquí...

escena final
  # más comandos...
```

```
scene start
  # commands here...

scene ending
  # more commands...
```

- La **primera escena** declarada es el punto de entrada automático
- Los nombres de escena no llevan comillas
- No pueden tener espacios (usa guion bajo: `escena_bonus`)
- Puedes saltar entre escenas con `ir_a` / `goto`

---

## 5. Fondos y Multimedia / Backgrounds & Media

### Fondo / Background

```
fondo "assets/fondos/aula.png"
background "assets/backgrounds/classroom.png"
```

Cambia la imagen de fondo. Soporta PNG, JPG, BMP. Se escala automáticamente a la resolución virtual (1920×1080).

**Dos formas — ruta literal o nombre lógico:**

```
fondo "assets/fondos/aula.png"    ← ruta literal (con comillas)
fondo aula                        ← nombre lógico (sin comillas)
```

La diferencia la marcan **las comillas**: con comillas es una ruta de archivo tal cual; sin comillas es un **nombre lógico** que buscás en el bloque `imagenes {}` de tu `proyecto.eclips` (ver sección de proyecto.eclips). El nombre lógico es la forma recomendada — definís la ruta una sola vez en el manifest y la referís por nombre en todos tus scripts; si movés el archivo, cambiás una línea en el manifest y todos los `fondo aula` siguen funcionando.

Si usás `fondo nombre` y ese nombre no está definido en `imagenes {}`, el motor te avisa por consola (con el nombre que escribiste, por si fue un typo) y **mantiene el fondo actual** — nunca te deja la pantalla en negro. Si lo que querías era una ruta literal, recordá las comillas: `fondo "nombre"`.

> **Nota:** por ahora solo `fondo`/`background` soporta nombre lógico. `musica` y `sonido` usan ruta literal (con comillas). El registro de audio por nombre llegará en una versión futura.

**Tinte de día/noche — `tinte=`** *(v0.8.0)*

```
fondo "assets/fondos/aula.png" tinte=#5566aa    ← el aula, teñida de noche
```

Multiplica el fondo por un color — un **atardecer o una noche gratis**, sin pintar otra versión del arte. `tinte=#ffffff` (blanco) es "sin tinte" (lo normal). El tinte **sobrevive al guardado**, y funciona igual en las **capas**:

```
capa ventana "assets/ventana.png" z=50 tinte=#334488
```

Si escribís un color inválido, el motor te avisa con el número de línea y usa el fondo sin teñir.

### Música / Music

```
musica "assets/audio/tema.ogg"
music "assets/audio/theme.ogg"
```

Reproduce en loop. Solo una pista a la vez — la nueva reemplaza la anterior.
Formatos: OGG, WAV, MP3, FLAC (vía miniaudio). Usa ruta literal (con comillas).

### Sonido / Sound

```
sonido "assets/audio/campana.wav"
sound "assets/audio/bell.wav"
```

Efecto de sonido one-shot. No interrumpe la música.
Suena al volumen de su propio archivo: todavía no hay volumen por efecto, así que el nivel se ajusta en el archivo. El jugador controla la música y los efectos por separado.

### Parar Música / Stop Music

```
parar_musica
stop_music
```

Detiene la música de fondo.

---

## 6. Personajes en Pantalla / Characters On Screen

### Mostrar / Show

```
mostrar luna centro
show luna center
```

Formatos aceptados:

| Formato | Ejemplo | Resultado |
|---------|---------|-----------|
| Solo alias | `mostrar luna` | Sprite base, posición centro |
| Alias + posición | `mostrar luna izquierda` | Sprite base, posición indicada |
| Alias + expresión | `mostrar luna feliz` | Expresión feliz, posición centro |
| Alias + expresión + posición | `mostrar luna feliz izquierda` | Expresión feliz, posición indicada |

*(La tabla describe a alguien que **entra** a escena. Si ya está en pantalla, seguí leyendo.)*

### Volver a mostrar a alguien que ya está en pantalla

`mostrar` sirve para dos momentos: **meter a alguien a escena** y **cambiarle algo a quien ya está**. Para el segundo, la ley es una sola:

> **Cambia solo lo que la línea nombró.** La posición si diste posición, la expresión si diste expresión. **Lo que no nombraste, no se toca.**

```
mostrar luna feliz centro     ← entra, contenta, al centro
mostrar luna izquierda        ← se mueve a la izquierda y SIGUE contenta
mostrar luna triste izquierda ← cambia de cara; la posición va escrita (ver ⚠ abajo)
```

> ⚠️ **Hoy, al cambiar la cara con `mostrar`, escribí la posición aunque no cambie.** Sin posición,
> `mostrar luna triste` sobre alguien que ya está en pantalla **le cambia la cara y lo manda al
> centro**: el motor lee *«no dijiste posición»* como `centro`. Si ya está en la izquierda, escribí
> `mostrar luna triste izquierda` y no se mueve. Cambiar la cara **dentro de una línea de diálogo**
> (`luna triste: "…"`) no la mueve nunca. *(Este aviso se retira
> cuando se arregle.)*

- **El movimiento es instantáneo** (un corte, no un desplazamiento). Para que se deslice, animá el canal: `animar luna mover_x a=-300 en=1s` (ver *Animaciones*).
- **`look_default` es el valor inicial**, así que solo se aplica al entrar. Un `mostrar` sin expresión sobre alguien ya visible **no** le devuelve su cara por defecto.
- **Si venía saliendo** de escena (`ocultar` con fade o con slide), volver a mostrarlo lo **rescata**: cancela la salida y regresa a su sitio.

> ⏱️ **El rescate tiene ventana.** Solo funciona mientras la salida está **en curso**. Un `ocultar [efecto: slide]` tarda **~0.5s** en sacar a alguien de pantalla; pasado eso el personaje ya se fue de verdad, y un `mostrar` posterior **no lo rescata: lo hace entrar de nuevo**. Eso importa porque una entrada sí necesita expresión — si el personaje es compuesto y no tiene `look_default`, `mostrar luna izquierda` no dibuja nada (y el motor te lo avisa en consola). Para traer de vuelta a alguien que ya salió, dale una pose: `mostrar luna 1r_a izquierda`.

> ⚠️ Esto vale igual para personajes simples, compuestos y compuestos sin `look_default`. Antes de v0.8.5 cada uno hacía algo distinto con la misma línea.

### Posiciones / Positions

```
┌─────────────────────────────────────────────────┐
│                                                 │
│  izquierda  centro_izq  centro  centro_der  derecha  │
│  left       center_left center  center_right right    │
│  (~18%)     (~33%)      (~50%)  (~67%)       (~82%)   │
│                                                 │
└─────────────────────────────────────────────────┘
```

| Español | English | Posición |
|---------|---------|----------|
| `izquierda` | `left` | ~18% del ancho |
| `centro_izq` / `centro_izquierda` | `center_left` | ~33% |
| `centro` | `center` | ~50% (default) |
| `centro_der` / `centro_derecha` | `center_right` | ~67% |
| `derecha` | `right` | ~82% |

Si no especificas posición, se usa **centro/center**.

### Ocultar / Hide

```
ocultar luna
hide luna
```

Quita al personaje de la pantalla.

### Efectos de Aparición / Character Effects

Por defecto, los personajes aparecen y desaparecen con un **fade** suave. Puedes cambiar el efecto con `[efecto: X]` / `[effect: X]`:

```
mostrar luna centro [efecto: instant]
show luna center [effect: instant]

ocultar luna [efecto: slide]
hide luna [effect: slide]
```

| Español | English | Efecto |
|---------|---------|--------|
| `desvanecer` / `fade` | `fade` | Aparición/desaparición gradual (default) |
| `instantaneo` / `instante` | `instant` | Aparición/desaparición inmediata |
| `deslizar` | `slide` | Entra/sale deslizándose |

> **`desvanecer` es nuevo desde la v0.8.28**, y no se inventó: el motor ya la
> escribía para este mismo concepto en los mapas (`animacion_entrada: desvanecer`).
> `fade` era el único de los tres efectos sin grafía española — sus dos hermanos
> ya la tenían. **`fade` sigue funcionando igual:** la grafía nueva se suma, no
> reemplaza.

Si no se especifica `[efecto:]`, se usa **fade** automáticamente.

> **Esos tres son todos, y escribir otro te avisa.** `mostrar luna centro [efecto: temblar]` deja un aviso con la lista y usa `fade`, en vez de caer en `fade` sin decir nada. (Para sacudir a un personaje lo que querés es `animar luna temblar`, que es otro comando.)

---

## 7. Diálogos y Narración / Dialogue & Narration

### Diálogo (con personaje)

```
luna: "Hola, bienvenido al club."
sol: "Me gusta leer."
```

El alias debe coincidir con un personaje definido con `personaje`/`character`.

### Narración (sin personaje)

```
"Una voz llegó desde el pasillo."
"The hallway echoed with silence."
```

Las líneas que empiezan directamente con comillas son narración — se muestran sin placa de nombre.

**Comillas dentro del texto:** tanto en narración como en diálogo, las comillas externas son el delimitador (no se ven) y **todo lo que va entre la primera y la última comilla se muestra literal** — incluidas comillas internas. Un personaje puede citar a otro sin trucos:

```
sol: "Luna dijo "buenos días" y se fue."
# se ve:   Luna dijo "buenos días" y se fue.

"Ella susurró "no mires atrás" antes de huir."
# narración, se ve:   Ella susurró "no mires atrás" antes de huir.
```

El texto vacío `""` es válido (caja vacía que avanza con click). Si dejás una comilla **sin cerrar** (impar), el motor avisa por consola con el número de línea y salta esa línea — no se muestra texto cortado en silencio.

### Diálogo con cambio de expresión

```
luna feliz: "¡Esto es genial!"
luna triste: "Oh no..."
```

Cambia la expresión del personaje al mostrar el diálogo (ver sección 8).

### Efectos de texto / Text Effects

EclipScript soporta tags inline para efectos visuales dentro del diálogo. Los tags se ponen directamente en el texto entre llaves `{}`.

```
luna: "{wavy}Todo este texto ondula suavemente~"
luna: "Normal, {shake}¡ESTO TIEMBLA!{/shake}, normal otra vez."
luna: "{speed=0.3}Esto... sale... muy... lento..."
luna: "Mira esta {color=#ff5555}palabra roja{/color} en medio de la frase."
luna: "{wavy}{color=#66ccff}Ondula Y es celeste a la vez.{/color}{/wavy}"
```

#### Tags disponibles

| Tag (English) | Tag (Español) | Efecto |
|---------------|---------------|--------|
| `{wavy}` | `{ondulado}` | Texto con onda sinusoidal vertical |
| `{shake}` | `{temblar}` | Texto con temblor aleatorio |
| `{glitch}` | `{glitch}` | Tramo corrupto (v0.8.2): revoltijo de glifos + temblor + franja roja/azul. Cognado (misma palabra en ambos idiomas), como `{color}`. Cierre: `{/glitch}`; sin cierre corre hasta el final. El ancho es el del texto ORIGINAL — la línea no se re-acomoda. Compone con `{color=}` y `{fuente=}` (el revoltijo sale del atlas de esa fuente). El espacio no se revuelve. |
| `{color=#RRGGBB}` | `{color=#RRGGBB}` | Tiñe el texto con un color hex. Es cognado (misma palabra en ambos idiomas). Cierre: `{/color}`. Ver nota abajo. |
| `{font=X}` | `{fuente=X}` | Cambia la tipografía a una fuente nombrada (declarada con `fuente`). Cierre: `{/font}` / `{/fuente}`. Ver nota abajo. |
| `{size=N}` | `{tam=N}` | Cambia el tamaño del texto a N píxeles a partir de ahí. Cierre: `{/size}` / `{/tam}` (vuelve al tamaño de línea). Ver nota abajo. |
| `{speed=N}` | `{velocidad=N}` | **Multiplicador** local de velocidad (0.5 = lento, 2.0 = rápido). Modifica per-char sobre la velocidad base. |
| `{global_speed=N}` | `{velocidad_global=N}` | **Override absoluto** de la velocidad de toda la línea. Ignora la cadena de fallback (ver "Cadena de velocidad"). |
| `{pause=N}` | `{pausa=N}` | Pausa el typewriter N segundos antes del siguiente carácter |
| `{wait_click}` | `{esperar_click}` | Pausa el typewriter hasta que el jugador haga click |
| `{interrupt}` | `{interrumpir}` | Al completar la línea, salta al siguiente diálogo sin esperar click (auto-advance forzado). Ver "Interrupciones". |

#### Tags rítmicos: pausa por tiempo y por click

Los tags `{pausa=N}` y `{esperar_click}` controlan el ritmo del typewriter dentro de una misma línea de diálogo. No tienen tag de cierre (son puntuales: ejecutan una pausa y siguen).

```
# Pausa por tiempo dentro de la línea
l: ". . . {pausa=1.0}sí{pausa=0.5}lo recuerdo."

# Espera click mid-línea
l: "Hola.{esperar_click} ¿Volviste?"

# Combinados — pausa, click, sigue
l: "Yo... {pausa=0.8}no sé.{esperar_click} ¿Tú qué opinas?"

# Al PRINCIPIO de la línea: retrasa la primera letra
l: "{pausa=0.8}...no sé qué decirte."
```

> **`{pausa=N}` al principio de la línea retrasa la primera letra.** La caja aparece (o ya está), y el texto tarda N segundos en arrancar. Sirve para un silencio antes de hablar, y **no necesita que la caja aparezca**: funciona igual a mitad de una conversación.
>
> Hasta la v0.8.24 esto se **ignoraba** con un aviso en consola (*"al inicio sin char previo"*), porque la pausa se cuelga del carácter anterior y ahí no hay ninguno. Si tenés líneas viejas que empiezan con `{pausa=N}`, **ahora sí pausan**.
>
> Su hermano `{esperar_click}` al principio de línea **se sigue ignorando** — no es un olvido, es un ticket aparte.
>
> Si lo que querés es que **todas** las apariciones de la caja retrasen el texto, no lo escribas línea por línea: usá `caja_aparicion_texto_retraso` en el tema. La línea gana sobre el tema cuando las dos están.

##### `{pausa=0}` — cancelar el retraso del tema en UNA línea *(v0.8.27)*

Si tu tema declara `caja_aparicion_texto_retraso` y querés que **una** línea
arranque ya, escribí `{pausa=0}` al principio:

```
# tema: caja_aparicion_texto_retraso 0.2
l: "Esta línea espera los 0.2 del tema."
l: "{pausa=0}Y ésta arranca YA."
```

**Cero cuenta como una respuesta, no como silencio.** Lo que decide no es el
número sino que vos lo hayas **escrito**: `{pausa=0}` es *«decido yo, y decido
ninguno»*, y le gana al tema igual que `{pausa=2}`. Una línea que no escribe el
tag hereda lo que el tema diga.

> Hasta la v0.8.26 esto **no se podía decir**: `{pausa=0}` se descartaba en
> silencio y la línea terminaba heredando el retraso igual. Si escribiste
> `{pausa=0}` alguna vez y no pasó nada, no era tu guion.

##### Un valor que no es un número ahora **te avisa** *(v0.8.27)*

`{pausa=}` sigue la misma ley del número que el resto del motor: el valor tiene
que consumirse **entero**.

| Escribís | Antes | Ahora |
|---|---|---|
| `{pausa=abc}` | se ignoraba, callado | se ignora **y te lo dice** |
| `{pausa=0.5s}` | valía `0.5` — la `s` se tiraba | se ignora **y te lo dice** |
| `{pausa=-1}` | se ignoraba, callado | vale `0` **y te lo dice** |

El negativo es el único que **sí** cuenta como escrito: la duda ahí es el valor,
no si lo pusiste. Los otros dos no se pueden adivinar, así que la línea hereda
lo del tema como si no hubieras escrito nada.

**Diferencias clave:**

| Tag | Pausa por... | El indicador `>` aparece |
|-----|--------------|--------------------------|
| `{pausa=N}` | tiempo (N segundos) | No (es pausa silenciosa) |
| `{esperar_click}` | input del jugador | Sí (señaliza al jugador que clickee) |

**Interacción con modos:**

| Modo | `{pausa=N}` | `{esperar_click}` |
|------|-------------|-------------------|
| Normal | Espera N segundos | Espera click |
| Auto | Espera N segundos | Espera autoDelay (el motor "auto-clickea") |
| Skip | Atraviesa inmediato | Atraviesa inmediato |

**Notas:**

- Los tags se aplican al carácter que los precede inmediatamente. `"hola{pausa=1.0}mundo"` pausa **después** de la `a` de hola.
- **`{pausa=N}` al inicio de línea es la excepción, y es a propósito:** ahí no hay carácter previo del cual colgarse, así que la pausa **es de la línea** (ver el recuadro de arriba). `{esperar_click}` al inicio **sí** se sigue ignorando, con log.
- **Los tags que abren la línea van antes de las comillas de presentación.** Si tu personaje declara `comillas`, el motor las agrega alrededor del texto y la de apertura entra **después** de los tags: escribís `"{pausa=0}Hola"` y se ve `"Hola"` con la comilla puesta. No tenés que hacer nada — sólo que no te sorprenda que el tag vaya primero.
- El comando `esperar N` (entre líneas, ver sección 14) y el tag `{pausa=N}` (dentro de línea) son hermanos: misma idea, distintos contextos.

#### El tag de color

`{color=#RRGGBB}` tiñe el texto a partir de ahí; `{/color}` lo devuelve al color
normal de la caja. Si no cerrás, el color sigue hasta el final de la línea.

```
sol: "El cielo se puso {color=#ff8800}naranja{/color} al atardecer."
sol: "{color=#88ff88}Todo esto en verde hasta el final, sin cerrar."
```

Tres cosas que lo hacen cómodo:

- **Combina con todo.** El color es independiente de `{wavy}`/`{shake}`: una
  palabra puede ondular **y** estar teñida a la vez. Anidá los tags sin miedo.
- **Respeta el typewriter.** La palabra de color aparece letra por letra con el
  mismo fade que el resto — no salta de golpe.
- **Respeta el contorno.** Si tu caja usa texto con borde (outline), el color
  tiñe solo el relleno; el borde se queda como está.

> **Formato:** solo `#RRGGBB` (seis dígitos hex). Nombres como `{color=rojo}` no
> funcionan todavía. Si el color está mal escrito, el texto usa el color normal de
> la caja y verás un aviso en la consola — no rompe nada.

#### El tag de fuente

`{fuente=X}` cambia la tipografía a una **fuente nombrada** a partir de ahí;
`{/fuente}` vuelve a la fuente normal de la caja. Si no cerrás, la fuente sigue
hasta el final de la línea. La fuente tiene que estar **declarada antes** con el
comando `fuente` (ver sección de fuentes):

```
fuente titulo "assets/fonts/manuscrita.ttf" tamano=34

sol: "Una palabra en {fuente=titulo}otra tipografía{/fuente} y vuelve a la normal."
sol: "{fuente=titulo}Toda la línea en la otra fuente, sin cerrar."
```

Tres cosas que lo hacen cómodo:

- **Cursiva y negrita reales, gratis.** No hay un tag `{negrita}` separado: si
  declarás la versión bold o itálica de tu fuente como una fuente nombrada
  (`fuente titulo_italica "...Italic.ttf"`), entonces `{fuente=titulo_italica}`
  *es* tu cursiva. Control total: vos elegís el archivo exacto.
- **Combina con todo.** La fuente es independiente del color y de `{wavy}`/`{shake}`:
  una palabra puede ir en otra tipografía **y** teñida **y** ondulando. Anidá sin miedo.
- **Respeta el contorno.** Si tu caja usa texto con borde, el borde sigue la fuente
  nueva (contorno y relleno quedan alineados).

> **Bilingüe:** `{fuente=X}` y `{font=X}` son equivalentes (y podés cruzarlas:
> abrir con `{fuente=X}` y cerrar con `{/font}` funciona). El nombre va **sin
> comillas** (las comillas son solo para la *ruta* al declarar la fuente).
>
> **Si la fuente no existe** (no fue declarada, o el nombre está mal escrito), el
> texto usa la fuente normal de la caja y verás un aviso en consola — no rompe nada.
>
> **Tamaño:** la fuente se muestra al tamaño con que la declaraste (`tamano=N`). Si
> se ve más grande o más chica que el resto, ajustá ese `tamano` — o usá `{tam=N}`
> inline para reescalarla a media línea (ver "El tag de tamaño" abajo).

#### El tag de tamaño

`{tam=N}` cambia el **tamaño del texto** a N píxeles a partir de ahí; `{/tam}` vuelve
al tamaño de la línea. Si no cerrás, sigue hasta el final de la línea. N es
**absoluto en píxeles** (`{tam=18}` = 18 px), igual que `texto_tamano`.

`luna: "Hablo normal y {tam=18}bajo la voz{/tam} y vuelvo. {tam=50}¡O grito!{/tam}" `

Tres cosas que conviene saber:

- **Comparte el baseline.** Los tramos de distinto tamaño se apoyan en la misma
  línea base; el alto de la línea sube solo al tamaño más grande presente. No
  ajustás nada para que no se pisen.
- **Compone con `{fuente=X}`.** `{fuente=blackrush}{tam=44}…{/tam}{/fuente}` da
  Blackrush a 44 px — el tamaño reescala la fuente activa (la de la caja o una
  nombrada), no usa su tamaño horneado. El orden de los tags no importa.
- **Reset triple.** Volver al tamaño de línea = `{/tam}`, `{tam=0}`, o dejar que
  termine la línea. Las tres son equivalentes.

> **Bilingüe:** `{tam=N}` y `{size=N}` son equivalentes (y cruzables: abrir con
> `{tam=N}` y cerrar con `{/size}` funciona). Valores inválidos o ≤ 0 (`{tam=abc}`,
> `{tam=-5}`) caen al tamaño de línea sin romper nada.
>
> **Relativo (`%` / delta):** todavía no — `{tam=N}` es siempre absoluto. Fichado
> para una futura iteración.

#### Interrupciones: saltar sin click ({interrumpir})

El tag `{interrumpir}` / `{interrupt}` marca la línea entera: cuando el typewriter termina de escribirla, el diálogo **salta solo al siguiente sin esperar click**. Es la herramienta para diálogos cortados, tensión rápida o interrupciones entre personajes.

```
l: "Espera, eso significa que tú—{interrumpir}"
s: "¡No lo digas en voz alta!"
```

Luna escribe su línea, y al llegar al `—` salta sola a Sol. Solo la última línea de la cadena (la de Sol, sin tag) espera click normal.

**El respiro (`interrumpir_demora`):** entre que la línea termina de escribirse y el salto hay un respiro corto y fijo, para que el texto se alcance a leer — incluso si el jugador tiene la velocidad en "Instantáneo". Se configura en el tema:

```
# en tu .eclips de tema
interrumpir_demora   0.15      # segundos (default). Más bajo = más seco.
```

A diferencia de `velocidad_texto` / `auto_demora` / `skip_intervalo`, **no admite lock** ni aparece en Settings: es un beat narrativo del creador, no una preferencia del jugador.

**Interacción con modos:**

| Modo | `{interrumpir}` |
|------|-----------------|
| Normal | Salta tras `interrumpir_demora` (sin click) |
| Auto | Igual — **pisa** el modo auto, no espera el `auto_demora` largo |
| Skip | Atraviesa inmediato (como todo) |

**Notas:**

- Es line-level: convencionalmente al final, pero funciona en cualquier posición (marca toda la línea).
- En una línea que interrumpe, el indicador de avance no se muestra (no hay click que esperar).
- El respiro se mide desde que el texto **completa**, no desde que empieza: por eso a velocidad instantánea el texto igual se ve antes de saltar.

#### Cadena de velocidad / Speed Chain

La velocidad del typewriter en cualquier línea se resuelve siguiendo una jerarquía de 6 niveles, evaluada de mayor a menor prioridad. **El primer nivel que aplique decide.**

| # | Nivel | Origen | Quién lo controla |
|---|-------|--------|-------------------|
| 1 | Line override | `{velocidad_global=N}` en la línea | Creador (script) |
| 2 | Character lock | `velocidad_lock si` + `velocidad_texto N` en personaje | Creador (manifest) |
| 3 | Player Settings | Slider en menú de configuración | Jugador |
| 4 | Character default | `velocidad_texto N` en personaje (sin lock) | Creador (manifest) |
| 5 | Theme | `velocidad_texto N` en `tema_*.eclips` | Creador (theme) |
| 6 | Engine default | Fallback hardcoded del motor | — |

**Cómo leer la cadena:** la lectura es "el más específico gana". Una decisión de línea (1) gana sobre una decisión de personaje (2/4), que gana sobre una decisión global de tema (5).

**`theme_lock` como kill-switch:** si el theme tiene `velocidad_texto_lock si`, los niveles 3 (player) y 4 (character default sin lock) se saltan. La cadena efectiva queda: 1 → 2 → 5.
Esto permite a un theme decir "este tema decide la velocidad,
no el jugador" sin negar la posibilidad de que un personaje específico (con `velocidad_lock`) o una línea específica (con `{velocidad_global=N}`) tengan su propio ritmo.

**Semántica vigente de velocidad (0.8.54):** la forma legacy de una sola palabra
(`velocidad_texto N` / `text_speed N`) sigue usando un CPS positivo y finito.
`Instantaneo` es un estado nombrado del runtime y de la persistencia v2; no se
agrega un literal numérico nuevo al codec. El valor exacto `9999` solo se
interpreta como `Instantaneo` al leer la key legacy de disco `text_speed`; en un
tema, personaje o línea, `9999` sigue siendo `Cps(9999)`.

Un tema puede declarar el dominio visible de su velocidad con una frase atómica:

```eclips
velocidad_texto 50 entre 20 y 200
velocidad_texto 60 entre 20 y 200 en pasos de 5
velocidad_texto 70 entre 1 y 150 con instantaneo
velocidad_texto 70 entre 1 y 150 en pasos de 0.5 con instantaneo
```

También se acepta la voz inglesa equivalente (`text_speed 70 between 1 and 150
with instant`). El valor por defecto debe estar dentro de `MIN..MAX`; `MAX` debe
ser mayor que `MIN`, y `PASO` debe ser positivo y no superar el rango. El
endpoint `Instantaneo` solo existe cuando la frase lo declara. Si una línea no
coincide exactamente con una forma válida, se emite un aviso con ejemplo
copiable y se conserva sin cambios el valor y dominio anteriores: el parser no
acepta una frase para ejecutar otra.

El dominio pertenece al theme; `SettingsState`, `control velocidad_texto` y el
runtime consumen esa misma validación y coordenada. Sin dominio, el control sigue
usando el catálogo legacy. Con dominio numérico, el control usa `MIN..MAX`, el
paso declarado y, si corresponde, el endpoint `Instantaneo` en `MAX + PASO`.
Para un dominio numérico, `tipo=barra` (o ningún `tipo`) es la presentación
compatible. `tipo=toggle`, `tipo=flechas` y `tipo=cajas` emiten un aviso una vez
por control y dominio, y caen a `barra`; no convierten el dominio en booleano ni dibujan un
catálogo de celdas. Sin dominio, la presentación legacy compatible conserva sus
flechas/cajas y la guardia existente de `toggle`.
El override del jugador se conserva exacto aunque quede fuera del dominio y solo
se proyecta al valor efectivo de la velocidad; la proyección se avisa una vez,
no escribe ni cuantiza el override, y revive si otro theme vuelve a admitirlo.
Línea, personaje, locks, precedencia y codec v2 no se clampan ni cambian de dueño.

**Niveles inactivos sin penalidad:** si un personaje no define `velocidad_texto`, su nivel 4 queda inerte y la cadena sigue al 5.
Si una línea no usa `{velocidad_global=N}`, el nivel 1 queda inerte. No hay penalty por niveles vacíos: la cadena los salta limpiamente.

**Composición con `{speed=N}`:** los dos tags son **ortogonales** y se componen.
`{velocidad_global=N}` setea la base de la línea entera; `{speed=N}` aplica un multiplicador per-char encima. Velocidad final = `lineBase * speedMult`.

Línea entera a 60 cps base, con un fragmento ralentizado a 60*0.25 = 15 cps

```
l: "{velocidad_global=60}Esto va rápido. {speed=0.25}Esto va lento. {/speed}Vuelve a rápido."
```

**Narrador y la cadena:** las líneas de narración (sin personaje) no tienen niveles 2 y 4 — saltan directo a la cadena 1 → 3 → 5 → 6.
Es decisión intencional: el narrador no es un personaje, no tiene velocidad propia.


**Filosofía de los locks:** los locks (`velocidad_texto_lock` en theme, `velocidad_lock` en personaje) protegen la decisión del **creador** frente a overrides del **jugador**.
NO bloquean otras decisiones del propio creador en niveles más granulares.
Si el creador necesita velocidad inviolable absoluta (caso de accesibilidad pura), la solución es **no exponer el slider en Settings UI**, no escalar la semántica de los locks.

#### Tags de cierre

El cierre es **opcional**. Si no pones cierre, el efecto aplica hasta el final del texto.

```
# Sin cierre — todo el texto tiene el efecto
luna: "{wavy}Todo esto ondula"

# Con cierre — solo parte del texto
luna: "Normal, {wavy}esto ondula{/wavy}, normal otra vez."

# Speed con cierre
luna: "Normal, {speed=0.2}ahora lento...{/speed} y rápido otra vez."
```

Tags de cierre: `{/wavy}`, `{/ondulado}`, `{/shake}`, `{/temblar}`, `{/glitch}`, `{/speed}`, `{/velocidad}`

#### Combinaciones

Los efectos se pueden combinar en el mismo diálogo:

```
luna: "{shake}¡PELIGRO!{/shake} Uf, ya pasó... {wavy}ahora todo está en calma~"
luna: "{wavy}Ondulando {speed=0.5}y ahora más lento...{/speed} rápido otra vez~"
```

#### Notas

- Los tags se procesan antes del typewriter — el texto limpio (sin tags) es lo que avanza carácter por carácter.
- Los efectos se preservan correctamente en save/load.
- `{speed=N}` solo afecta la velocidad del typewriter, no el movimiento visual.

### Comillas de presentación / Presentation Quotes

Un personaje puede tener **comillas visibles** que el motor dibuja automáticamente alrededor de todo lo que dice — al estilo de las VNs donde el diálogo va siempre entre `「」` o `«»`. **Vos no las tecleás**: las declarás una vez en el personaje y el motor las envuelve solo.

Es una perilla del personaje en tu `proyecto.eclips`, en la misma familia que `velocidad_texto` o `caja`:

```
# en proyecto.eclips
personaje s {
    nombre "Sol"
    path "assets/demo/sol/"
    comillas "«»"
}
```

Con eso, en el script escribís normal:

```
s: "Hola, soy yo."
```

…y en pantalla se ve: `«Hola, soy yo.»` — los guillemets los puso el motor.

**La regla de los 2 glifos:**

El valor de `comillas` es un string que se parte por **glifo** (no por byte, así que los caracteres multi-byte funcionan):

| Valor | Abre | Cierra | Resultado para `"hola"` |
| --- | --- | --- | --- |
| `comillas "«»"` | `«` | `»` | `«hola»` |
| `comillas "''"` | `'` | `'` | `'hola'` |
| `comillas "|"` (1 glifo) | `\|` | `\|` | `\|hola\|` (simétrico) |
| `comillas ""` (vacío) | — | — | `hola` (sin comillas) |

Si ponés **más de 2 glifos**, el motor usa el primero para abrir y el último para cerrar, y te avisa por consola (probablemente no era lo que querías).

**El caso de las comillas dobles rectas (`"`):** como `"` es también el delimitador del valor, `comillas ""` queda **vacío** (las dos `"` son el delimitador, no contenido) — el motor te avisa por consola y no pone comillas. Para conseguir dobles rectas, escribí **cuatro**: `comillas """"` → el par externo delimita, el interno es el contenido → `"hola"`.

**Sobre la fuente:** las comillas usan cualquier glifo que tu **fuente** tenga rasterizado. Seguros con fuentes Latin: `«»`, `''`, `""`. Si pedís un glifo que tu fuente no incluye (p. ej. `「」` con una fuente pixel-art solo-Latin), verás un `?` en su lugar — no es un bug del motor, es tu fuente avisando que le falta ese carácter. Para `「」` u otros scripts necesitás una fuente que los incluya.

**Notas:**

- **Per-personaje.** Las comillas cuelgan del personaje, no de la caja: dos personajes pueden compartir caja y tener comillas distintas, y un personaje que cambia de caja a mitad de escena (`usar_caja`) **conserva sus comillas**.
- **El narrador no lleva comillas.** La narración (texto sin personaje) no tiene entrada en el manifest, así que nunca recibe comillas — es el default sin configurar nada.
- **Default = sin comillas.** Un personaje sin la perilla `comillas` habla sin comillas. Backward-compatible: los scripts y manifests viejos no cambian.
- **Comillas internas siguen siendo literales.** Si escribís `s: "Dijo "hola" y se fue."`, las comillas internas se muestran tal cual (extractor glotón) y las de presentación las envuelven por fuera: `«Dijo "hola" y se fue.»`. Las dos cosas no se pisan.
- **Las comillas son texto normal.** Fluyen por el typewriter, el word-wrap y los tags de efecto como cualquier otro carácter. El historial y el preview de guardado muestran el texto con sus comillas.

---

## 8. Expresiones / Expressions

Las expresiones son variantes visuales de un personaje. Se basan en archivos de imagen con sufijo.

### Personajes normales

Si el personaje base es `assets/personajes/luna.png`:

| Comando | Archivo buscado | Fallback |
|---------|----------------|----------|
| `luna: "texto"` | `luna.png` | — |
| `luna feliz: "texto"` | `luna_feliz.png` | `luna.png` |
| `luna triste: "texto"` | `luna_triste.png` | `luna.png` |
| `luna seria: "texto"` | `luna_seria.png` | `luna.png` |

También puedes usar expresiones con `mostrar`/`show`:

```
mostrar luna feliz centro
show luna happy center
```

**Regla de fallback:** si el archivo de expresión no existe, se usa el sprite base automáticamente. No hay error.

### Personajes compuestos

En personajes compuestos, la expresión se refiere a la combinación de partes (capas de cuerpo + cara), y `luna X` se resuelve en dos pasos: apariencia nombrada → forma del nombre. Ver **sección 3 · Personajes compuestos**, subsección *El modelo completo: dos preguntas*.

---

## 9. Transiciones / Transitions

Las transiciones se aplican al **siguiente** comando `fondo`/`background`. Son un "modificador" que se coloca antes.

```
transicion fade
fondo "assets/fondos/nuevo.png"

transition dissolve
background "assets/backgrounds/new.png"
```

### Tipos disponibles

| Español | English | Efecto |
|---------|---------|--------|
| `fade` | `fade` | Dip to black (oscurece → aclara) |
| `dissolve` / `disolver` | `dissolve` | Crossfade entre fondo viejo y nuevo |
| `slide_izquierda` | `slide_left` | Nuevo fondo entra desde la derecha |
| `slide_derecha` | `slide_right` | Nuevo fondo entra desde la izquierda |

- Duración: 1.5 segundos con easing smoothstep
- Sin `transicion`/`transition` antes, el cambio de fondo es instantáneo
- La transición se consume después de un `fondo` — no se acumula

---

## 9.5 Texto en Pantalla / On-Screen Text

El comando `texto` pone texto **posicionable en cualquier parte de la pantalla**, como un cartel, un reloj, un título de capítulo o la pantalla de una computadora. Es distinto del diálogo: el diálogo es una **voz** (alguien habla, va a la caja y al historial); el texto-en-capa es un **objeto** escrito en la escena (nadie lo dice, está ahí hasta que lo ocultás).

```
texto "Capitulo 1" x=760 y=200
texto reloj "16:17" x=270 y=85 color=#00FF66
texto titulo "MEMORIA LISTA" x=600 y=400 color=#EAEAEA z=300
```

### La regla del id

```
texto "Capitulo 1" x=760 y=200          ← SIN id: lo pongo y me olvido
texto sello "5 minutos despues" x=760 y=400   ← CON id: lo voy a tocar despues
```

- **Sin id** = fire-and-forget. Un título que aparece y ya.
- **Con id** = lo vas a referenciar luego (para fade, ocultar, mover).
- **Si el id ya existe**, la línea no crea otro texto: actualiza ese. Cambia sólo
  lo que nombra (el contenido, la x, el color…) y deja lo demás como estaba.

El **id va sin comillas** (es un nombre que vos inventás, como un personaje). El **contenido va entre comillas** (es lo que se ve). Regla mental: *lo que se muestra lleva comillas; el nombre no.*

### Parámetros

| Parámetro | Qué hace | Default |
|-----------|----------|---------|
| `x=` `y=` | Posición en pantalla | 0, 0 |
| `color=#RRGGBB` | Color del texto | blanco |
| `z=` | Orden de capa (alto = al frente) | 0 |
| `fuente=` | Fuente nombrada a usar (ver §9.6); si no existe, cae al default | default 34px |
| `glow_color=#RRGGBB` | Color del halo de brillo (alias `brillo_color=`) | sin glow |
| `brillo_radio=` | Grosor del halo en px (alias EN `glow_radius=`) | 2 |
| `brillo_intensidad=` | Fuerza del halo, `60%` o `0.6` (alias EN `glow_intensity=`) | 50% |
| `borde_color=#RRGGBB` | Color del borde de las letras (alias EN `outline_color=`) | negro |
| `borde_grosor=` | Grosor del borde en px (alias EN `outline_thickness=`) | 2 |

El **tamaño lo da la fuente** — el texto-en-capa no lleva `w=`/`h=` (a diferencia de `capa` con imágenes).

### Borde de las letras / Text outline *(v0.8.22)*

El contorno de color alrededor de cada letra — lo que hace legible un título sobre
cualquier fondo, y la firma visual de muchos menús de novela visual.

```
texto op1 "New Game" x=140 y=180 borde_color=#00FF88 borde_grosor=3
```

- **`borde_color=`** — el color del contorno. Acepta `#RRGGBB` o `#RRGGBBAA` (con
  alpha, para un borde translúcido). Si lo ponés **sin** grosor, el motor usa **2 px**.
- **`borde_grosor=`** — el grosor en px. Si lo ponés **sin** color, el borde sale
  **negro** (que es el caso clásico: letras blancas, línea negra).

> **`borde_grosor=0` apaga el borde de verdad.** Sirve para quitarle el borde a un
> texto que lo hereda de un `estilo`. *(Ojo: el glow **no** se comporta así —
> `brillo_radio=0` no lo apaga. Es un bug conocido, no una regla.)*

**El borde y el brillo no se pueden usar juntos.** Si escribís los dos, gana el borde
y el motor te avisa:

```
[EclipScript] AVISO linea 12: texto: el borde y el brillo no se pueden usar juntos en la misma linea. Se usa el borde.
```

#### Hasta qué grosor se ve bien

El borde se dibuja con muestras circulares uniformes alrededor del texto. La cantidad
crece con el grosor (separación objetivo de 2 px, acotada a 8..48), así que el criterio
visual sigue dependiendo del tamaño de la fuente — no hay un techo fijo de copias:

| Tamaño de la fuente | Grosor que se ve liso |
|---|---|
| 72 px (títulos) | **1–8 continuos**; 8 ya es un borde deliberadamente pesado |
| 24 px (texto normal) | **1–4 limpios**; 5–8 siguen continuos, pero el borde domina la letra |

Resultado del feel-test de OUTLINE-CIRCULO (2026-08-21): no aparecieron copias separadas
ni esquinas cuadradas en el barrido; una fuente de títulos redondeada en negrita a grosor 4 y el cuerpo de
diálogo a grosor 2 también quedaron continuos. Es una referencia del corpus medido, no
una promesa de calidad arbitraria para cualquier fuente o proporción. La ley automática
no cambia la sintaxis ni el color/alpha del relleno.

### Se manipula con la familia de capas

Como el texto **es** una capa, reusás los mismos comandos de capa sin aprender nada nuevo:

```
texto reloj "16:17" x=270 y=85
capa_z reloj 300          # traer al frente
capa_visible reloj no     # esconder
capa_visible reloj si     # mostrar
```

> ⚠️ El id de `capa_z`/`capa_visible`/`hotspot` ahora va **sin comillas** (igual que `capa_fade` y `texto`). La forma vieja con comillas se acepta con un aviso de migración, pero se rechazará en una versión futura.

### Fade-in / Fade-out

`capa_fade` desvanece una capa (texto **o** imagen) suavemente. El id va **sin comillas** (igual que `texto`):

```
texto sello "5 minutos despues" x=700 y=400 color=#EAEAEA
capa_fade sello aparecer 1.0     # entra suave en 1 segundo
esperar 3.0                       # se queda 3 segundos
capa_fade sello desaparecer 0.8   # se va suave en 0.8 segundos
```

| Dirección | Español | English | Efecto |
|-----------|---------|---------|--------|
| Entrar | `aparecer` / `entrar` | `in` | alpha 0 → 1 |
| Salir | `desaparecer` | `out` | alpha → 0, luego se oculta |

El fade es un **hermano**, no un parámetro de `texto` — el texto se crea flaco, y vos dirigís el ritmo (aparición, cuánto se lee, salida) componiendo comandos. El que quiere un cartel permanente, no escribe el `desaparecer`. El que lo quiere instantáneo, no escribe ningún `capa_fade`.

> ⚠️ El id de `capa_fade` va **sin comillas**: `capa_fade sello`, no `capa_fade "sello"`. Si ponés comillas, la línea se rechaza con un aviso (las comillas son para texto que se ve, no para nombres de capa).

---

### Mover una capa / Layer move (`capa_mover`)

`capa_mover` desliza una capa (texto **o** imagen) hacia un destino `(x, y)`. El id va **sin comillas**, igual que `capa_fade`:

    capa logo "assets/logo.png" x=-500 y=400 w=420 h=300
    capa_mover logo a (760, 400) en 1.5 suave      # entra eased y se queda

**Forma:** `capa_mover <id> a (x, y) en <segundos> [una_vez|loop] [curva]`

**Curvas** — no cambian *cuánto* dura el movimiento, cambian **cómo se siente**:

| Español | English | Se siente como |
|---------|---------|----------------|
| `lineal` | `linear` | Velocidad constante, sin acelerar ni frenar *(default)* |
| `suave` | `smooth` | Arranca despacio, acelera, y frena al llegar |
| `salida` | `ease_out` | Arranca rápido y frena al llegar |
| `entrada` | `ease_in` | Arranca lento y llega con velocidad — el **espejo exacto** de `salida` |
| `resorte` | `spring` | **Se pasa** del destino y vuelve — como un resorte |
| `rebote` | `bounce_out` | **Rebota** al llegar, como una pelota que cae |
| `cubica` | `cubic` | Acelera y frena de forma simétrica, continua y sin cuantizar |

`cubica` usa la curva simétrica `4t³` para `t < 0.5` y
`1 - 4(1-t)³` para `t >= 0.5`: sus muestras son `0 → 0`,
`.25 → .0625`, `.5 → .5`, `.75 → .9375` y `1 → 1`.

    capa_mover logo a (760, 400) en 1.2 rebote     # cae, pica, y se queda
    capa_mover caja a (500, 300) en 0.8 resorte    # se pasa y vuelve
    capa_mover puerta a (960, 540) en 1.0 entrada  # arranca perezosa, llega con hambre

> `entrada` es el espejo temporal de `salida`. Dato de la casa: el vaivén del
> indicador de avance del motor es exactamente esta curva sobre un vaivén. Y tiene
> sabor de amenaza — algo que se te viene encima ganando velocidad.

Si escribís una curva que no existe, el motor te avisa **al cargar el guión** (con el número de línea) y usa `lineal`.

| Parte | Qué hace | Default |
|-------|----------|---------|
| `a (x, y)` | destino en coordenadas de diseño | — (obligatorio) |
| `en <segundos>` | cuánto dura el movimiento | — (obligatorio) |
| `una_vez` / `once` | se mueve una vez y **se queda** en el destino | ✓ default |
| `loop` / `bucle` | scroll **monótono con wrap** (infinito): al llegar, vuelve al inicio | — |
| `suave` / `smooth` | easing `easeInOutSine` (arranca y frena suave) | — |
| `lineal` / `linear` | velocidad constante | ✓ default |

**`una_vez` vs `loop`:**
- `una_vez` es para entradas, salidas y el **rollo de créditos** (una capa más alta que la pantalla que sube una vez y se va).
- `loop` es para **fondos en scroll infinito** (una textura tileable más grande que la pantalla que se recorre sin fin).

**La curva solo aplica a `una_vez`.** Un `loop` es **siempre lineal** — una curva pulsaría en cada vuelta del wrap. Si combinás `loop rebote`, el motor avisa e ignora la curva.

`capa_mover` es un **hermano** del fade, no un parámetro de `capa`: la capa se coloca quieta, y vos dirigís su movimiento. Re-emitirlo sobre una capa que ya se mueve reinicia el movimiento desde su posición actual.

> ⚠️ Como `capa_fade`, el id va **sin comillas**: `capa_mover logo`, no `capa_mover "logo"`. La ruta del asset (en `capa`) sí lleva comillas — es dato externo; el id es símbolo del guión.

> Más curvas se agregan cuando una escena las pida — así llegaron `rebote`, `resorte` y `entrada`.

---

## 9.6 Fuentes Nombradas / Named Fonts

Por defecto, el texto-en-capa usa la fuente del motor a 34px. Si querés un
título grande, una letra de computadora monoespaciada, o varios tamaños a la
vez, declarás una **fuente nombrada** con `fuente` y la usás por su nombre.

```
fuente titulo "assets/fonts/serif.ttf" tamano=72
fuente terminal "assets/fonts/mono.ttf" tamano=20
texto cartel "Capitulo 1" x=550 y=250 fuente=titulo
texto consola "C:> iniciar" x=100 y=400 fuente=terminal
```

### La regla del nombre y la ruta

- **El nombre va sin comillas** (`titulo`) — es un id que vos inventás.
- **La ruta va entre comillas** (`"assets/fonts/serif.ttf"`) — es un archivo
  literal, puede tener espacios.
- `tamano=N` (también `tamaño=N` con eñe, o `size=N`) es obligatorio. Las tres son la misma cosa; escribí una sola. *(Nota histórica: hasta v0.8.6 la grafía con eñe **no funcionaba de verdad** —el motor la anunciaba acá y no la reconocía, así que la fuente no se cargaba—. Si tenías un `tamaño=` que "no hacía nada", ya no es tu script.)*
- `espaciado_letras=N` (también `letter_spacing=N`) es **opcional** — el espaciado entre letras en píxeles (negativo aprieta, positivo separa). Si no lo ponés, la fuente usa su espaciado natural.
- `medida=em` (también `measure=em`) es **opcional**. Cambia la escala nominal de esa fuente a unidades em; `medida=altura` / `measure=height` nombra explícitamente la escala histórica. Si no escribís la clave, no cambia nada: la fuente conserva identidad y píxeles legacy.
- La medida viaja con la fuente nombrada y se hereda al pedir tamaños derivados (`{tam=N}` / `{size=N}`), a las cajas y a las consolas. No existe un default de proyecto ni cambia globalmente `tamano`.

Ejemplo opt-in:

```
fuente menu_nav "third_party/fonts/m_plus_rounded_1c/MPLUSRounded1c-Bold.ttf" tamano=24 medida=em
```

`em` y `altura`/`height` son los únicos valores. Una medida desconocida avisa y no
emite la fuente; si escribís las dos grafías (`medida=` y `measure=`), el parser avisa
el conflicto y conserva la primera forma canónica.

Misma regla mental de siempre: *lo literal (ruta, texto que se ve) lleva
comillas; lo que el motor interpreta (nombres, números) no.*

### Por qué el tamaño va en la fuente, no en el texto

El tamaño se **hornea** en la fuente al declararla (se rasteriza a ese tamaño
para que se vea nítida). Por eso una misma `.ttf` a 72px y a 18px son **dos
fuentes distintas** — declarás una para cada tamaño que quieras:

```
fuente grande "x.ttf" tamano=72
fuente chica  "x.ttf" tamano=18
```

### Espaciado de letras / Letter spacing

Igual que el tamaño, el **espaciado entre letras** (tracking) se hornea en la fuente al declararla. **Negativo** junta las letras (compacto, estilizado); **positivo** las separa. Opcional:

```
fuente apretada "x.ttf" tamano=34 espaciado_letras=-3
fuente aireada  "x.ttf" tamano=34 espaciado_letras=2
```

Como vive en la fuente, la misma `.ttf` con dos espaciados distintos son **dos fuentes** (igual que con dos tamaños). Los valores reales son chicos: **−2 a −4** aprieta de forma elegante; pasado **~−6/−8** las letras empiezan a tocarse según la fuente.

> *Nota:* esto es **tracking** (espaciado uniforme), no *kerning* de pares específicos. Ren'Py llama "kerning" a su perilla equivalente; acá la nombramos por lo que hace.

### Solo se carga lo que declarás

Una fuente que está en `assets/` pero que nunca declarás con `fuente` **no
ocupa memoria**. El motor solo carga las que pedís.

**Desde `v0.8.60` el orden ya no importa** (antes sí: una selección escrita
arriba de su declaración caía a la fuente base y una escrita abajo funcionaba).
El motor lee el archivo entero y prepara todas sus fuentes **antes** de ejecutar
la primera línea, así que podés declararlas donde te quede cómodo — arriba de
todo, al final, o al lado de la escena que las usa.

### El catálogo del proyecto / The project catalog (v0.8.60)

Una fuente declarada en un `.ecl` vive **en ese archivo**. Si la usás en varios
libretos, declarala una sola vez en `proyecto.eclips` (o `project.eclips`), con
exactamente la misma escritura:

```
# proyecto.eclips
fuente aller "assets/fonts/Aller_Rg.ttf" tamano=34

estilo titulo_grande {
    fuente aller
    color #6BF0FF
}
```

Reglas del catálogo, en una línea cada una:

- **El proyecto manda.** Si un `.ecl` declara un nombre que el proyecto ya
  declaró **con otra forma**, gana el del proyecto y el motor te avisa citando
  los dos archivos y las dos líneas. Si la declaración es **idéntica**, no pasa
  nada y no se dice nada: es compatibilidad, no un error.
- **Un nombre que el proyecto no declara es del archivo.** Dos libretos pueden
  llamar `titulo` a dos fuentes distintas sin pisarse, y el texto que ya
  escribiste conserva su fuente aunque después `cargar_script` traiga otro
  archivo que reuse el nombre.
- **Dos declaraciones del mismo nombre en el mismo archivo**: gana la primera y
  el motor te dice en qué dos líneas están.

### La cadena de respaldos / The fallback chain (v0.8.60)

Una fuente de autor bonita casi nunca tiene **todos** los caracteres. Si tu
novela mezcla español con japonés, con emoji o con símbolos raros, podés
escribir una **cadena**: el motor prueba la fuente base y, sólo para el
carácter que ésa no tiene, sigue por los respaldos en el orden que escribiste.

```
fuente poema "assets/fonts/DamagrafikScript-Regular.ttf" tamano=34 {
    respaldo "assets/fonts/NotoSans.ttf"
    fallback "assets/fonts/NotoEmoji.ttf"
}
```

- `respaldo` y `fallback` son **la misma palabra** en los dos idiomas, y sólo
  significan esto **dentro** del bloque de una fuente. Sueltas, fuera del
  bloque, el motor no las conoce y te lo dice.
- **El orden es el que escribís** y no se reordena: gana la primera fuente de la
  cadena que tenga ese carácter. Un respaldo **nunca** le gana a una elección
  tuya: sólo repara un carácter que faltaba.
- `tamano`, `espaciado_letras` y `medida` son de **la cadena completa**: van en
  la cabecera, no en los respaldos.
- Dentro del bloque sólo se escriben líneas `respaldo "..."`. Cualquier otra
  cosa se ignora **con aviso**, y el perfil sigue vivo.
- **Si te olvidás el `}`, el perfil entero se descarta** y el motor te avisa. No
  se queda con la fuente base sola: eso te daría la ruta correcta con la
  cobertura equivocada, en silencio.
- Si ningún miembro de la cadena tiene el carácter, el motor **lo omite y
  avisa** — no dibuja un cuadrito ni inventa una fuente que vos no pediste.

La misma forma vale en `proyecto.eclips` y en un `.ecl`: es un solo parser, con
los mismos defaults y los mismos avisos.

### Lo que NO cambió (compatibilidad)

- `fuente "ruta.ttf"` dentro de un **tema** (`.eclips` de tema) sigue siendo una
  **ruta cruda**, no un nombre de catálogo. No hay que migrar ningún tema.
- `fuente: "ruta.ttf"` dentro de un **mapa** (`.eclmap`) también sigue siendo
  ruta cruda.
- Las cinco claves `caja_fuente`, `nombre_fuente`, `opciones_fuente`,
  `qte_fuente` y `boton_fuente` del tema siguen seleccionando **nombres
  lógicos**, y sus precedencias no se movieron.

### Si pedís una fuente que no declaraste

`texto x "hola" fuente=inexistente` no rompe nada: el texto sale en la fuente
default y el motor te avisa **una vez** en consola qué fuente faltó declarar.

---

## 9.7 Brillo de Texto / Text Glow

El texto-en-capa puede llevar un **halo de brillo** difuso alrededor de las
letras — el efecto "letra de computadora" de una pantalla vieja, carteles de neón,
títulos que resplandecen. Es opcional: si no lo pedís, el texto sale plano.

```
texto cartel "ESTRELLA" x=550 y=250 glow_color=#FF6BAA
texto titulo "ECLIPS" x=400 y=150 glow_color=#6BF0FF brillo_radio=3 brillo_intensidad=80%
```

### Los tres controles

- **`glow_color=#RRGGBB`** — el color del halo. Es RGB puro (lo pegás tal cual
  del selector de color). La transparencia NO va acá — va en la intensidad.
- **`brillo_radio=N`** — qué tan lejos llega el halo (en px). Más alto = halo más
  grande y difuso. Default útil: 2.
- **`brillo_intensidad=`** — qué tan fuerte brilla. Acepta `60%` o `0.6` (es lo
  mismo). Default útil: 50%.

### La regla "puse color → veo glow"

Si escribís solo `glow_color`, el motor completa radio e intensidad con sus
defaults útiles (2 y 50%) — así no tenés que poner los tres para ver algo.
Y si no escribís ningún `glow_*`/`brillo_*`, no hay glow: el texto sale plano,
idéntico a antes.

### Color y fuerza van separados (a propósito)

El color es el color, la intensidad es una perilla aparte — igual que en los
`.eclips` separás `caja_color` de `caja_opacidad`. Así podés pegar tu hex de
seis dígitos sin hacer mates de transparencia, y ajustar el brillo sin tocar
el color. Ejes independientes = control real.

### Bilingüe

Todo acepta español (`brillo_*`) e inglés (`glow_*`): `brillo_color`,
`brillo_radio`, `brillo_intensidad` · `glow_color`, `glow_radius`,
`glow_intensity`. Mezclables libremente como siempre.

> **Cambió en v0.8.20 — `glow_radio` y `glow_intensidad` ya no valen.** Eran
> mitad inglés y mitad español (`glow_` + `radio`), o sea un tercer idioma que
> el motor nunca prometió: acá se escribe en **español** o en **inglés**, y esas
> dos no eran ninguno de los dos. Las formas buenas son `brillo_radio` /
> `brillo_intensidad` (español, y son las canónicas) o `glow_radius` /
> `glow_intensity` (inglés). `glow_color` **sí se queda**: `color` se escribe
> igual en los dos idiomas, así que no es una mezcla.
>
> Vale para las dos superficies: los `k=v` de `texto` **y** el bloque `estilo`
> de `proyecto.eclips`, incluidas sus variantes `_hover`. Si tenías la forma
> vieja, el motor te lo dice con archivo y línea — no falla en silencio.

> 💡 El glow convive con la fuente nombrada: `texto x "..." fuente=titulo glow_color=#FF6BAA`
> brilla y usa tu fuente a la vez.

---

## 9.8 Estilos / Styles

Si te encontrás escribiendo la **misma combinación** de fuente + color + brillo en
muchos `texto`, un **estilo** la empaqueta bajo un nombre. Lo definís una vez y lo
invocás con `estilo=nombre`. Es como tener tu propia "fuente con personalidad"
lista para usar.

> Un estilo **no hace nada nuevo** — solo te ahorra retipear. Todo lo que mete
> (fuente, color, brillo) ya lo podías escribir suelto en cada `texto`. Si solo
> tenés un cartel, no hace falta estilo; escribilo inline y listo. El estilo
> aparece cuando empezás a **repetir**.

### Definir un estilo

Los estilos viven en `proyecto.eclips` (donde está tu elenco), como un bloque.
**Dentro del bloque NO se usa `=`** (cada línea ya es su propio par — igual que en
`personaje { ... }`):

```
# en proyecto.eclips
fuente aller_titulo "assets/fonts/Aller_Rg.ttf" tamano=72

estilo titulo_brillante {
    fuente aller_titulo
    color #6BF0FF
    glow_color #6BF0FF
    brillo_radio 3
    brillo_intensidad 80%
}
```

| Clave (dentro del bloque) | Alias EN | Qué hace |
|---|---|---|
| `fuente` | `font` | Nombre de una fuente declarada con `fuente N "..." tamano=N` |
| `color` | — | Color del texto, `#RRGGBB` |
| `brillo_color` | `glow_color` | Color del halo |
| `brillo_radio` | `glow_radius` | Grosor del halo en px |
| `brillo_intensidad` | `glow_intensity` | Fuerza, `80%` o `0.8` |
| `borde_color` | `outline_color` | Color del borde de las letras (v0.8.22) |
| `borde_grosor` | `outline_thickness` | Grosor del borde en px (v0.8.22) |
| `trazo` | `outline` | Una pasada de borde: `grosor color [dx dy]`; se puede repetir y conserva el orden |
| `trazo_hover` | `outline_hover` | Una pasada para hover; se puede repetir y reemplaza la lista hover completa |
| `color_hover` | — | Color del texto **al pasar el mouse** (solo `enlace`) |
| `brillo_color_hover` | `glow_color_hover` | Color del halo al hover |
| `brillo_radio_hover` | `glow_radius_hover` | Grosor del halo al hover |
| `brillo_intensidad_hover` | `glow_intensity_hover` | Fuerza del halo al hover |
| `borde_color_hover` | `outline_color_hover` | Color del borde al hover (v0.8.22) |
| `borde_grosor_hover` | `outline_thickness_hover` | Grosor del borde al hover (v0.8.22) |

> **El borde va en español: `borde_grosor`, no `outline_grosor`.** El motor habla dos
> idiomas, no tres — una clave es entera en español o entera en inglés. *(El **tema**
> (`.eclips` de caja de diálogo) todavía escribe `nombre_outline_grosor`, mezclado;
> es deuda vieja, no un tercer idioma que se pueda usar acá.)*

El menú de la referencia se escribe así, entero:

```
estilo menu_opcion {
    color #FFFFFF
    trazo       4 #BB5599
    trazo       2 #BB5599 2 2
    trazo_hover 4 #FFAACC
    trazo_hover 2 #FFAACC 2 2
}
```

`trazo` y `trazo_hover` aceptan `grosor color [dx dy]`. El desplazamiento es opcional y
por defecto vale `0 0`; las líneas repetidas forman una lista ordenada. Si no declarás
ningún `trazo_hover`, el hover hereda la lista base. El borde legado (`borde_color` y
`borde_grosor`, o sus aliases ingleses) sigue siendo válido y equivale a una lista de un
elemento centrado. Un borde legado escrito inline reemplaza la lista completa del estilo.

No mezcles lista y `borde_*` dentro del mismo estilo: el motor avisa y conserva la lista
completa, sin dejar una lista parcial. Si una línea de trazo tiene color, grosor u offset
inválido, avisa y descarta esa línea entera. Los trazos se dibujan detrás y el relleno del
texto se emite una sola vez al final.

> **v0.8.20:** `glow_radio`, `glow_intensidad` y sus `_hover` se retiraron acá
> también — ver la nota de la sección del glow. Las formas buenas son las de la
> tabla de arriba.

El nombre del estilo (`titulo_brillante`) lo inventás vos, **sin comillas** — igual
que el alias de un personaje.

### Usar un estilo

En tu `.ecl`, con `estilo=` (acá **sí** lleva `=`, porque va pegado al comando):

```
texto cartel "ECLIPS" x=400 y=150 estilo=titulo_brillante
```

Esa línea sola te da fuente + color + glow de una. Sin el estilo, tendrías que
escribir los cinco params en cada cartel.

### Pisar un campo suelto (override)

Lo que escribís **inline gana**, campo por campo. El resto del estilo sigue vivo:

```
texto c2 "ODIO" x=200 y=400 estilo=titulo_brillante glow_color=#FF0000
```

Acá el halo sale **rojo** (lo pisaste), pero el color del texto sigue cyan y el
radio sigue siendo 3 — esos vienen del estilo, no los tocaste. Cambiás solo lo que
querés sin perder lo demás.

### Notas

- **El tamaño no es parte del estilo.** Vive en la `fuente` (porque el tamaño se
  hornea al cargarla). Un estilo que necesita otro tamaño apunta a otra `fuente`.
- **Estilo que no existe** → el texto sale plano (o con lo que pusiste inline) y
  aparece un aviso en consola. No rompe nada, igual que `fuente=inexistente`.
- Bilingüe: `estilo`/`style`, y dentro del bloque las claves aceptan ES/EN.
- Por ahora los estilos aplican a **texto-en-capa**. La caja de diálogo, el nombre
  del personaje y los botones toman su apariencia del **tema**, no de estilos.

### Variante de hover (para `enlace`)

Un estilo puede declarar cómo se ve el texto **al pasar el mouse**, con los campos
`*_hover`. Solo tiene efecto cuando el `texto` es un **`enlace`** (ver abajo);
sobre texto normal no hace nada. Se define una vez:

```
estilo menu_opcion {

color #FFFFFF

color_hover #FF6BAA

glow_color_hover #FF6BAA

brillo_radio_hover 4

}
```

Base blanco; al pasar el mouse, rosa con halo. Lo que no declarás en hover
**hereda del base**. El **tamaño NO** tiene variante hover (cambiarlo sería cambiar
de fuente); el **contorno** (el halo, vía `glow_*_hover`) sí — es gratis. Sin
ningún `*_hover`, el enlace no cambia al pasar el mouse: el motor solo repinta lo
que declaraste.

## 9.9 Texto clickable / `enlace`

Un `texto` por sí solo es decorado. **`enlace`** lo vuelve **clickeable**: al hacer
click, despacha. Es "una elección sin su 'hey, esto es una elección'" — la base de
los **menús** (palabras sueltas que brillan y llevan a algún lado) y del botón
**"OK"/"Aceptar"** de un aviso.

Es un **hermano** de `texto` (igual que `capa_fade`): el `texto` se queda flaco, y
`enlace` se cuelga por fuera, referenciando su id:

```
texto nuevo "Juego Nuevo" x=850 y=400 estilo=menu_opcion
enlace nuevo -> intro       # al click, navega a la escena 'intro'
```

### Los tres destinos

- **`enlace <id> -> escena`** → navega a esa escena.
- **`enlace <id> -> verbo`** → ejecuta un verbo de shell: `guardar`, `cargar`,
  `menu`, `historial`, `config`, `salir` (y sus alias EN).
- **`enlace <id>`** (sin flecha) → **continuar**: avanza la línea actual / sigue.
  Es el "OK, lo leíste, sigo" de un aviso.

### Aviso modal (`requerido`)

Agregá `requerido` (o `required`) a un `enlace`-OK y la línea se vuelve **modal**:
mientras ese OK viva, el jugador **no puede esquivar el aviso** — quedan sellados el
avance plano (click al vacío / Espacio / Enter / Z), el auto, el skip y el guardado
(`K`/`L`/`F5`/`F9`). La **única** salida es clickear el OK. (`ESC` sigue abriendo la
pausa — no se sella.)

    texto ok "Acepto"

    enlace ok requerido            # continuar, pero obligatorio

    "AVISO: este juego contiene escenas intensas."

- Con cualquier destino: `enlace ok requerido` (continuar) o
  `enlace ok requerido -> intro` (navegar) — **el flag va pegado al id, siempre**, del
  mismo lado que `imagen_hover=` y **antes** de la flecha. *(Hasta v0.8.42 iba al final,
  después del destino; esa forma quedó retirada y el motor te lo dice.)*
- El sello se **prende cuando ejecuta el `enlace`** y se **apaga al clickear el OK**
  (o al cambiar de escena). Por eso el `enlace … requerido` va **justo antes de la
  línea que debe ser modal**: un aviso modal es de **una línea**. Antes de varias,
  el jugador queda atrapado en la primera (no avanza entre ellas).
- Es el "OK obligatorio" de un aviso de contenido. Reusa `requerido` de `entrada`:
  misma palabra, misma promesa ("no salís sin confirmar"), ahora también sin input.

### Enlace de imagen (botón PNG)

`enlace` no solo viste texto — también vuelve clickeable una **capa-imagen**. Con
`imagen_hover=` le das el PNG "encendido", y el botón intercambia normal↔encendido al
hover. Rutea a los mismos tres destinos:

    capa boton "assets/ui/slot_idle.png" x=850 y=400 w=200 h=80
    enlace boton imagen_hover="assets/ui/slot_hover.png" -> guardar

- La imagen **normal** es la de la `capa`; la **encendida** cuelga del `enlace`.
- Sin `imagen_hover=` la capa sigue clickeable, pero no cambia al hover ("no miente").
- El PNG encendido se escala al mismo `w×h` (cae exacto sobre el normal).
- Bajo un modal (caja de `entrada`, historial) el botón se **congela**: ni enciende,
  ni suena, ni rutea — solo se resalta lo que se puede activar.

### Un menú es solo varios `enlace`

No hay un "modo menú" — un menú son palabras colocadas donde quieras, cada una con
su `enlace`:

```
texto m1 "Juego Nuevo" x=860 y=400 estilo=menu_opcion

enlace m1 -> intro

texto m2 "Continuar"   x=860 y=480 estilo=menu_opcion

enlace m2 -> cargar

texto m3 "Opciones"    x=860 y=560 estilo=menu_opcion

enlace m3 -> config

texto m4 "Salir"       x=860 y=640 estilo=menu_opcion

enlace m4 -> salir

```

> **⚠ Un menú de `enlace` es MOUSE-ONLY hoy** *(v0.8.34)*. Las flechas del teclado
> recorren los `elegir`, los `control` y las `ranura`, pero **no los `enlace`** — salvo
> el caso de abajo (el OK de un aviso `requerido`), que sí se alcanza con las flechas.
> Si tu menú tiene que ser navegable sin mouse, escribilo con `elegir`.

### El "OK" de un aviso

```
texto ok "Aceptar" x=850 y=600 estilo=menu_opcion

enlace ok                    # sin flecha = continuar
enlace ok requerido          # modal: es la UNICA salida
```

Con `requerido`, la caja queda **modal** —el avance, el auto, el skip y el quicksave
quedan sellados hasta que aceptes— y el OK es la única salida. **Se alcanza con
`ARRIBA`/`ABAJO` y se activa con `avanzar`** (`ESPACIO`/`ENTER`/`Z`/clic), así que un
aviso modal no encierra a quien juega sin mouse *(v0.8.34)*.

### Detalles

- **Hover:** el cambio al pasar el mouse viene del `estilo` (campos `*_hover`,
  arriba). Sin hover declarado, el enlace no cambia — sigue clickeable, solo que
  discreto.
- **Zona de click = las letras.** El área sensible son los límites medidos del
  texto, no un rectángulo inflado: clickeás lo que ves.
- **Sonido:** si el tema declaró sonido de hover/click para sus opciones, el enlace
  lo reusa.
- **Si el id no existe:** `enlace fantasma` sin un `texto fantasma` que matchee →
  aviso en consola, se ignora (no rompe).
- **Tu escena gana:** si nombrás una escena igual que un verbo reservado (ej. una
  escena `historial`), va a **tu** escena, con un aviso de que tapaste el verbo.
- Bilingüe: `enlace` / `link`.

---


### Bloque `imagenes` — nombres lógicos para fondos

En tu `proyecto.eclips` podés declarar un registro de **nombres lógicos** para tus imágenes: das un nombre corto a cada ruta, y después en tus `.ecl` usás el nombre en vez de la ruta completa. Es un bloque, **igual que `personaje` o `estilo`**, pero sin nombre en la apertura (es una tabla, no una entidad):

```
# en proyecto.eclips
imagenes {
    aula     "assets/fondos/aula_dia.png"
    pasillo  "assets/fondos/pasillo.png"
    azotea   "assets/fondos/azotea_atardecer.png"
}
```

Y en cualquier `.ecl`:

```
fondo aula        # ← usa el nombre lógico, sin comillas
fondo pasillo
```

**Por qué usarlo:** definís cada ruta **una sola vez**. Si reorganizás tus carpetas o renombrás un archivo, cambiás una línea en el manifest y todos los `fondo aula` de todos tus scripts siguen funcionando. Sin esto, una ruta repetida en 30 escenas son 30 ediciones cuando movés el PNG.

**Reglas del bloque (idénticas a `estilo`/`personaje`):**
- **Dentro del bloque NO se usa `=`** ni `:` — cada línea es `nombre "ruta"`.
- El **nombre** lo inventás vos (sin comillas); la **ruta** va entre comillas.
- Bilingüe: `imagenes` / `images`.
- Nombre duplicado → aviso, gana el último. Ruta vacía (`aula ""` o `aula` sin ruta) → aviso, se ignora esa línea.

**Cómo se usa desde el `.ecl`:** ver sección 5 (Fondo / Background) — `fondo nombre` (lógico) vs `fondo "ruta"` (literal). La comilla es la que decide cuál es cuál.

> **Alcance actual:** el bloque `imagenes` resuelve para el comando `fondo`/`background`. Audio (`musica`/`sonido`) todavía usa ruta literal; un bloque equivalente para sonidos llegará en una versión futura.

---

## 10. Variables y Condicionales / Variables & Conditionals

### Set / Mutar

```
set ruta = curiosidad          # Asignar texto
set puntos = 0                 # Asignar número
set puntos + 1                 # Incrementar
set puntos - 1                 # Decrementar
set nombre = "Juan Carlos"     # Valor con espacios
```

`mutar` es lo mismo que `set`, en español — y `meta_mutar` lo mismo que
`meta_set`. Los cuatro comparten la forma `variable operador valor`:

```
mutar ruta = curiosidad
mutar puntos + 1
meta_mutar aviso_visto = 1
```

Elegí el que te salga más cómodo; podés mezclarlos línea por línea.

### Cambiar una variable AL CLICKEAR / Mutation on click

Un destino con la forma `variable operador valor` **cambia la variable en vez de
saltar a una escena**, y el guion sigue en la línea de abajo. Sirve en las tres
cosas que se clickean: las opciones de `elegir`, los `enlace` y los hotspots de
`esperar_click`.

```
"¿La ayudás con el poema?" elegir:
  "Sí, claro"    -> afecto + 1
  "Ahora no"     -> afecto - 1
  "Preguntarle"  -> tema = poema
  "Irse"         -> escena_pasillo      # esto sigue siendo un salto
```

- La **forma** decide: si hay operador (`=`, `+`, `-`), es una mutación; si es
  una sola palabra, es una escena o un verbo — o sea que nada de lo que ya
  escribiste cambia de significado.
- Los operadores son los mismos de `set`, y el valor puede ir entre comillas:
  `-> nombre = "Luna Nueva"`.
- Después de mutar, cada cosa hace lo suyo: la opción se elige y el guion sigue,
  el hotspot resuelve la espera, y un `enlace` se comporta igual que un enlace
  sin flecha (avanza la línea).
- Se puede combinar con `[si: ...]` como cualquier otra opción.

> **Ojo con el `enlace`, que sorprende la primera vez: un enlace NO reemplaza el
> avance de la línea, lo agrega.** Como cualquier línea de diálogo, un clic en
> cualquier parte de la pantalla la avanza — clickear la palabra hace las dos
> cosas (muta **y** avanza), clickear al lado hace sólo la segunda. Si querés que
> la palabra sea la **única** salida, es para lo que existe `requerido`:
>
> ```
> texto lnk "Aceptar"
> enlace lnk requerido -> visto = 1      # el flag va ANTES de la flecha
> ```
>
> **El flag va con los parámetros, o sea antes de la flecha** — igual que
> `imagen_hover=`, y en el mismo lugar que cuando no hay flecha (`enlace ok
> requerido`). Lo que va **después** de `->` es el destino y el motor no lo vuelve a
> leer, así que un valor puede decir cualquier cosa.
>
> **Si lo escribís al final** —`enlace lnk -> visto = 1 requerido`, que era la forma
> hasta v0.8.42— el motor te avisa **y te escribe la línea corregida**:
>
> ```
> enlace: el flag 'requerido' va ANTES de la flecha; aca se lee como parte del
> destino. Escribi: enlace lnk requerido -> visto = 1
> ```
>
> **Y hay un caso que se avisa distinto, porque es genuinamente ambiguo:** cuando el
> valor **es** la palabra (`-> ruta = requerido`) el motor no adivina — te nombra las
> dos salidas, y las comillas son el escape:
>
> ```
> enlace: 'requerido' al final es ambiguo. Como FLAG va antes de la flecha (enlace ok
> requerido -> ...); como VALOR va entre comillas (-> ruta = "requerido").
> ```

> Un destino que **parece** una mutación pero le falta el operando —`-> vida +`—
> no hace nada y te lo dice en la consola. No se lo trata como nombre de escena
> a propósito: ahí leerías *"no es una escena"* sobre algo que nunca quisiste que
> lo fuera.

### Mostrar variables en el texto / Interpolation `${var}`

Cualquier variable se puede **mostrar dentro del texto** escribiendo `${nombre}`. El motor la reemplaza por su valor antes de dibujar.

Any variable can be **shown inside text** by writing `${name}`. The engine replaces it with its value before drawing.

```
set mc = "Jesús"
luna: "Hola, ${mc}. Bienvenido al club."     # → Hola, Jesús. Bienvenido al club.

set muertes = 3
"Has muerto ${muertes} veces."               # → Has muerto 3 veces.
```

Funciona en **diálogo, narración, opciones (`elegir`) y texto-capa** — en todo lo que el jugador lee:

```
elegir:
  "Sí, soy ${mc}"      -> confirmar
  "No, me equivoqué"   -> volver

texto reloj "${_hora}:${_minuto}" x=270 y=85 color=#00FF66
```

**Reglas:**

- **Variables del sistema también sirven.** `${_hora}`, `${_minuto}`, etc. (ver §24). Así se hace el reloj en pantalla.
- **El valor puede llevar efectos.** Si una variable contiene un tag, se renderiza:
  ```
  set grito = "{wavy}AAAAH{/wavy}"
  luna: "Y entonces gritó: ${grito}"          # "AAAAH" sale ondulado
  ```
- **Variable que no existe → se queda como está.** Si escribís `${pepe}` y nunca hiciste `set pepe`, el texto muestra literal `${pepe}` y el motor avisa una vez en consola (así cazás el error de tipeo). No rompe nada.
- **Un solo nivel.** Si una variable contiene `${otra}`, eso NO se vuelve a expandir (evita bucles).
- **`${}` se resuelve AL ESCRIBIR, también en `set`/`mutar`.** `mutar copia = ${luna}` guarda el **valor** de `luna`, no el texto `${luna}`. Es lo que hace posible copiar y acumular contadores (ver §10, *Comparar dos variables*).

> **Tip:** las variables son strings. `set mc = "Jesús"` (el nombre va con comillas porque es **dato**), pero `mc` se escribe **sin** comillas dentro de `${...}` porque ahí es el **nombre** de la variable. Combiná esto con la captura de nombre del jugador y toda la historia puede decir su nombre — un patrón clásico de los juegos narrativos.

### Condicionales / Conditionals

```
si puntos >= 4
  luna feliz: "¡Increíble!"
sino
  luna: "No está mal."
fin_si
```

```
if points >= 4
  luna happy: "Incredible!"
else
  luna: "Not bad."
end_if
```

#### Operadores / Operators

| Operador | Significado |
|----------|-------------|
| `==` | Igual (texto o número) |
| `!=` | Diferente |
| `>` | Mayor que (numérico) |
| `<` | Menor que (numérico) |
| `>=` | Mayor o igual (numérico) |
| `<=` | Menor o igual (numérico) |

#### Comparar dos variables / Comparing two variables *(v0.8.47)*

Del lado derecho, **lo que escribís pelado es un valor literal**. Para comparar contra **otra variable**, envolvela en `${...}` — el mismo marcador que usás en el texto:

On the right-hand side, **a bare word is a literal value**. To compare against **another variable**, wrap it in `${...}` — the same marker you use in text:

```
si sol > luna       # ❌ compara contra la palabra "luna"
si sol > ${luna}    # ✅ compara los dos contadores
```

**Por qué el marcador.** Los dos lados no significan lo mismo, y nunca lo hicieron: a la izquierda va **el nombre** de una variable, a la derecha **un valor**. Eso es lo que hace que este patrón —muy común— siga funcionando:

```
mutar gano = sol       # `gano` guarda el NOMBRE "sol"
si gano == sol         # y esto lo compara contra ese nombre
```

Si el lado derecho se resolviera solo, esa segunda línea leería el **puntaje** de Sol y la rama nunca entraría.

**Funciona con los seis operadores**, y `==` es el que habilita preguntar por un **empate**:

```
si sol == ${luna}
  "Empataron."
fin_si
```

**Y también en `mutar` / `set`:**

```
mutar top   = ${sol}         # copiar un valor
mutar total + ${luna}        # acumular
mutar dif   - ${estrella}    # restar otra variable
```

##### Receta: «gana quien tenga más puntos»

El motor no trae un `maximo`: trae la pieza y vos armás la regla, así que podés desempatar como quieras. Con tres personajes:

```
mutar gano = sol
mutar top  = ${sol}

si luna > ${top}
  mutar gano = luna
  mutar top  = ${luna}
fin_si
si estrella > ${top}
  mutar gano = estrella
  mutar top  = ${estrella}
fin_si

"El poema es para ${gano}, con ${top}."
```

Y para saber si hubo empate, contá cuántas llegan a la cima:

```
mutar empatadas = 0
si sol      == ${top}
  mutar empatadas + 1
fin_si
si luna     == ${top}
  mutar empatadas + 1
fin_si
si estrella == ${top}
  mutar empatadas + 1
fin_si

si empatadas > 1
  "No gana nadie: empataron ${empatadas}."
fin_si
```

##### Si te equivocás, te lo dice

Escribir la forma sin marcador **no rompe nada y no pasa desapercibido**. En una comparación numérica (`>`, `<`, `>=`, `<=`), si el lado derecho no es un número el motor avisa y la condición **da falso**:

```
[si] 'sol > luna': el lado derecho tiene que ser un numero, y 'luna' no lo es. Si querias comparar con la VARIABLE 'luna', escribi 'si sol > ${luna}'. Por ahora la condicion da falso.
```

En `mutar`, además, **la variable no se toca**:

```
[mutar] 'testigo - luna': el operando tiene que ser un numero, y 'luna' no lo es. Si querias usar la VARIABLE 'luna', escribi 'mutar testigo - ${luna}'. Por ahora 'testigo' no cambia.
```

> **Ojo con la frontera, que es a propósito:** `==` y `!=` comparan **texto**, así que ahí nunca hay aviso — `si ruta == unirse` es correcto y compara contra la palabra `unirse`. El aviso vive sólo donde el valor **tiene** que ser un número.
>
> Y una **variable que nunca seteaste vale 0 en silencio**: `si puntos >= 4` antes del primer `mutar puntos` funciona y no dice nada. Lo que avisa es un valor **escrito con basura**, no uno vacío.

#### Anidación / Nesting

Los condicionales se pueden anidar:

```
si ruta == unirse
  si puntos >= 3
    luna feliz: "Eres increíble."
  sino
    luna: "Bienvenido al club."
  fin_si
fin_si
```

**Nota:** Todo `si`/`if` debe cerrarse con `fin_si`/`end_if`. `sino`/`else` es opcional.

---

### Pedir texto al jugador / Text input (`entrada`)

`entrada` (o `input`) pausa el juego, muestra un prompt, deja al jugador escribir, y guarda el texto en una variable. Es el complemento de `${var}`: `entrada` *escribe*, `${var}` *lee*.

```
entrada nombre "¿Cómo te llamas?"
"Encantado de conocerte, ${nombre}."
```

La variable va **sin comillas** (es un id, como en `fuente`); el prompt va **con comillas**. Usá la misma palabra al capturar y al leer (`entrada saludo` ↔ `${saludo}`).

**Parámetros opcionales** entre `[ ]` (como `elegir`/`qte`):

```
entrada nombre "Tu nombre:" [max: 12, default: "Main Character"]
```

- `maximo:` / `max:` — máximo de **caracteres** (no bytes; "ñ" cuenta 1). `0` o ausente = sin límite. Al llegar al tope, el campo deja de aceptar letras.
- `defecto:` / `default:` — valor si el jugador confirma sin escribir, o cancela. Acepta varias palabras entre comillas, **comas incluidas** (`defecto: "Hola, mundo"`).
- `requerido` / `required` — bandera (sin valor): el jugador **no puede confirmar vacío**. El OK queda gris y Enter/Esc se ignoran hasta que escriba algo. El patrón clásico: no podés empezar sin nombre.

**Garantía:** tras un `entrada`, la variable **siempre queda definida**. Confirmar con Enter sin escribir → el `default` (o `""` si no hay). Cancelar (Esc en PC, botón atrás en Android) → igual. Así un `${var}` posterior nunca falla.

**Controles:** Enter o **click en el botón OK** confirman · Esc / botón-atrás cancela. Acentos y ñ funcionan. (Mover el cursor con flechas es mejora futura.)

**El botón OK (themeable).** Bajo el campo hay un botón de confirmar (clickable, con hover + sonido, reusa el del `elegir`). Se personaliza desde el **tema**, a **nivel top** (fuera de cualquier `caja_dialogo`, porque la captura usa la caja default):

| Key | Qué hace |
|---|---|
| `entrada_ok` / `input_ok` | Texto del botón (default `"OK"`). Puede ser `"Aceptar"`, `"受け入れる"`, lo que sea. |
| `entrada_ok_estilo` / `input_ok_style` | `caja` (rect/PNG + letra) o `texto` (letra sola; el hover cambia el color del texto — un "OK" pelado, sin caja). |
| `entrada_pos_y` / `input_pos_y` | Ancla vertical del bloque (ratio 0..1, default `0.40`). Subir en Android para que el teclado no lo tape. |
| `entrada_tamano` / `input_size` | Tamaño de fuente del texto del aviso (prompt + buffer + cursor; el OK **no** escala, sigue su propia geometría). `0` = heredar el baseline. Ojo: el aviso no hace wrap → letra grande + prompt largo puede desbordar el marco; el creador controla el largo. |
| `entrada_ok_ancho/alto` · `_offset_x/_offset_y` | Tamaño y desplazamiento (control total: descentrar a propósito vale). |
| `entrada_ok_fondo_color` · `_borde_color` · `_borde_grosor` · `_texto_color` | Cara normal (+ variantes `_hover_*` y `_deshab_*`). |
| `entrada_ok_sprite` / `input_ok_sprite` · `_hover_sprite` / `input_ok_hover_sprite` · `_sprite_borde_*` | Cara PNG con 9-slice (opcional; vacío = rect). |

> Las `entrada_ok_*` van a **nivel top**, igual que las `choice_*`. Dentro de un `caja_dialogo { }` no las lee la captura.

### El marco/fondo del aviso

Por default el `entrada` flota sobre la escena **sin fondo** (perfecto sobre negro o una imagen que ya contrasta). Si querés un **cuadrado detrás** que haga legible el aviso — el clásico recuadro de "pon tu nombre" — prendé el marco. Es **opt-in** y **decoración pura** (no se clickea); el OK vive encima.

```
entrada_marco            si

entrada_marco_ancho      600

entrada_marco_alto       250
```

| Key | Qué hace |
|---|---|
| `entrada_marco` / `input_frame` | `si`/`no` (default `no`). Prende el marco. |
| `entrada_marco_ancho/alto` / `input_frame_width/height` | Tamaño del cuadrado (px del lienzo). Sin auto-fit: vos mandás. |
| `entrada_marco_offset_x/y` / `input_frame_offset_x/y` | Mueve **solo** el marco respecto al cluster (− sube/izq, + baja/der). |
| `entrada_marco_fondo_color` · `entrada_marco_opacidad` | Relleno del rect nativo (color hex + opacidad `NN%`). |
| `entrada_marco_borde_color` · `_borde_opacidad` · `_borde_grosor` | Borde del rect nativo (color + opacidad + grosor px). |
| `entrada_marco_sprite` / `input_frame_sprite` · `_sprite_borde_*` | Cara PNG con 9-slice (opcional; vacío = rect nativo). |

**Nativo o PNG, una de dos.** Sin `entrada_marco_sprite` → cuadrado nativo (color + borde + opacidad, control paramétrico total — ideal para un marco simple sin depender de un asset). Con sprite → el PNG manda: se dibuja a `ancho`×`alto` (a tamaño nativo del PNG = 1:1 sin deformar; si lo escalás, poné `_sprite_borde_*` para 9-slice).

**Con PNG, el borde y el color de relleno son no-op** — el PNG trae los suyos en los píxeles; solo `entrada_marco_opacidad` actúa (multiplica la transparencia de la imagen). Mismo trato que `caja_borde` bajo `caja_sprite`.

**Dos ubicaciones distintas:** `entrada_marco_offset_*` mueve el marco solo; `entrada_pos_y` mueve **todo el bloque** (prompt+buffer+OK) y el marco lo sigue (se centra sobre el cluster). Para subir el conjunto entero, bajá `entrada_pos_y`.

> Las `entrada_marco_*` van a **nivel top**, igual que las `entrada_ok_*`.

---

## 11. Opciones / Choices

### Básico

```
elegir:
  "Sí, me uno"     -> ruta_si
  "Necesito pensar" -> ruta_duda

choose:
  "Yes, I'll join"   -> route_yes
  "I need to think"  -> route_doubt
```

- Las opciones van indentadas debajo de `elegir:`/`choose:`
- Cada opción: `"texto visible" -> nombre_escena`
- El jugador elige con teclas 1-4 o click
- Máximo recomendado: 4 opciones

> **Dónde termina el bloque, y cómo se distingue una opción de una narración.** El bloque se acaba en la primera línea que no es una opción — no lleva cierre. Y lo que decide es **dónde está la flecha**:
>
> | Línea | Qué es | Por qué |
> |---|---|---|
> | `"Correr" -> correr` | **opción** | la flecha va **después** de la comilla que cierra |
> | `"Corrió hacia la puerta -> y no volvió."` | **narración** | la flecha va **adentro** de las comillas: es texto |
> | `"Correr" > correr` | **error, te avisa** | quiso ser opción y le falta el `->` |
>
> La tercera deja un aviso con su número de línea, **se descarta esa línea sola, y el bloque sigue** — así una opción bien escrita debajo de una rota no se pierde con ella.

> **Un gate mal escrito también avisa.** `[sii: llave == 1] "Abrir" -> cofre` (con el typo) te lo dice en vez de cortar el bloque en silencio.

### Con Timer

```
elegir [timer: 5s, default: "Quedarse quieto"]:
  "¡Correr!"         -> correr
  "¡Esconderse!"     -> esconder
  "Quedarse quieto"  -> quieto

choose [timer: 5s, default: "Stay still"]:
  "Run!"         -> run
  "Hide!"        -> hide
  "Stay still"   -> still
```

- `tiempo:` / `timer:` / `time:` — segundos antes de auto-seleccionar
- `defecto:` / `default:` — **copia literal del texto** de la opción que se elige si el tiempo se acaba
- La opción default es la que se **auto-selecciona** si el timer expira (su *resaltado visual* distintivo aún no está implementado — hoy no se ve distinta de las demás; llegará en una versión futura)
- Barra de progreso verde→amarillo→rojo encima de las opciones

> **`defecto:` pide una copia del texto de una opción, no una escena ni una descripción.** Es la parte que más confunde, porque todo lo demás del bloque apunta a escenas (`-> destino`). Si lo que escribís no coincide con ninguna opción, el motor **usa la última y te lo dice**, listándote las que sí existen:
>
> ```
> elegir [tiempo: 15s, defecto: "Tiempo agotado"]:
>   "Rapido"     -> fase_4
>   "Mas rapido" -> fase_4
> ```
> ```
> [EclipScript] AVISO linea 45: elegir: el default "Tiempo agotado" no es ninguna de
> las opciones. Opciones: "Rapido", "Mas rapido". Al acabarse el tiempo se usa la ultima.
> ```
>
> **Ojo con `${variable}`:** la comparación es contra el texto **tal como lo escribiste**, antes de reemplazar la variable. Si tu opción dice `"${nombre} ayuda"`, el defecto tiene que decir `"${nombre} ayuda"` también — no `"Luna ayuda"`, que es lo que se ve en pantalla.

> **`defecto:` sin `tiempo:` no hace nada, y el motor te avisa.** El defecto es *la respuesta a que se acabe el tiempo*: sin reloj no hay pregunta que responder. Un `elegir` sin `tiempo:` espera al jugador para siempre, que es lo normal.

> **¿Querés que el defecto cambie según lo que hizo el jugador?** No hace falta sintaxis nueva: meté el `elegir` dentro de un `si`, o movete con los gates `[si:]` de abajo.
>
> ```
> si karma > 5
> elegir [tiempo: 5s, defecto: "Ayudarla"]:
>   "Ayudarla"  -> ayuda
>   "Ignorarla" -> ignora
> sino
> elegir [tiempo: 5s, defecto: "Ignorarla"]:
>   "Ayudarla"  -> ayuda
>   "Ignorarla" -> ignora
> fin_si
> ```

### Opciones condicionales (gate `[si:]`)

Cada opción puede llevar un **modificador `[si:]`** que la muestra solo si una condición se cumple. Útil para opciones secretas, rutas que requieren un objeto, o ramas que dependen de decisiones previas.

```
elegir:

[si: llave == 1] "Abrir el cofre secreto" -> cofre

"Seguir caminando"                          -> pasillo
choose:

[if: key == 1] "Open the secret chest" -> chest

"Keep walking"                          -> hallway
```

- El modificador va **al inicio** de la línea, antes de las comillas: `[si: variable operador valor]`
- Operadores: `==`, `!=`, `>`, `<`, `>=`, `<=`
- Bilingüe: `[si: ...]` o `[if: ...]`
- Si la condición es **falsa**, la opción **no aparece** (las demás se renumeran automáticamente)
- Funciona igual en `esperar_click:`, pero ahí el hotspot **no desaparece**: queda **deshabilitado** (no responde al click) en vez de ausente

> ⚠️ Si **todas** las opciones de un `elegir:` quedan ocultas por su gate, el jugador se queda sin salida (atascado). El motor te avisa en consola con la línea del script. Deja siempre al menos una opción sin condición.

### Apariencia de los botones de opción

Los botones de `elegir` toman su color del tema. Cada estado (normal / hover) tiene su propio juego de keys, **a nivel de tema** (fuera de cualquier bloque `caja_dialogo`):

| Key (ES) | Key (EN) | Qué pinta |
|----------|----------|-----------|
| `choice_fondo_color` | `choice_bg_color` | Fondo del botón (normal) |
| `choice_borde_color` | `choice_border_color` | Borde (normal) |
| `choice_texto_color` | `choice_text_color` | Texto (normal) |
| `choice_hover_fondo_color` | `choice_hover_bg_color` | Fondo al pasar el mouse |
| `choice_hover_borde_color` | `choice_hover_border_color` | Borde al pasar el mouse |
| `choice_hover_texto_color` | `choice_hover_text_color` | Texto al pasar el mouse |


en el tema (.eclips), a nivel raíz:


```
choice_fondo_color        #1A2452E5

choice_hover_fondo_color  #2E4280F2

choice_hover_borde_color  #80BFFFFF
```

- Colores en `#RRGGBB` o `#RRGGBBAA` (con alpha).
- El **hover** se activa solo con mouse/cursor: al pasar por encima, la opción se pinta con sus colores `_hover_`. Con teclado (1-4) o toque táctil no hay hover — se elige directo.
- Si no defines las keys `_hover_`, el motor usa un realce por defecto (fondo más brillante, borde claro, texto con tinte azulado).


### Cara PNG del botón de opción (v0.5.2)

El botón de `elegir` puede vestir un **PNG** en vez del rect de color, con estado
**hover** propio y **sonido**. Todo a nivel de tema (raíz del `.eclips`):

| Key (ES) | Key (EN) | Qué hace |
|----------|----------|----------|
| `choice_sprite` | `choice_sprite` | PNG de fondo del botón (estado normal) |
| `choice_hover_sprite` | `choice_hover_sprite` | PNG al pasar el mouse (cambia la textura) |
| `choice_sprite_borde_izq` | `choice_sprite_border_left` | Borde 9-slice izquierdo (px del PNG) |
| `choice_sprite_borde_der` | `choice_sprite_border_right` | Borde 9-slice derecho |
| `choice_sprite_borde_arriba` | `choice_sprite_border_top` | Borde 9-slice superior |
| `choice_sprite_borde_abajo` | `choice_sprite_border_bottom` | Borde 9-slice inferior |
| `choice_sonido_hover` | `choice_sound_hover` | Sonido al entrar al botón (1× por hover) |
| `choice_sonido_click` | `choice_sound_click` | Sonido al elegir la opción |

```
choice_sprite        "assets/ui/boton_opcion.png"
choice_hover_sprite  "assets/ui/boton_opcion_hover.png"

choice_sprite_borde_izq    40
choice_sprite_borde_der    40
choice_sprite_borde_arriba 8
choice_sprite_borde_abajo  8

choice_sonido_hover  "assets/ui/sfx/hover.wav"
choice_sonido_click  "assets/ui/sfx/select.wav"
```

- **Idle + hover** apuntan a PNGs distintos → la textura cambia al pasar el mouse.
  Si solo declarás `choice_sprite`, el botón queda estático en hover (el sonido da
  el feedback). Si el PNG no carga, hay aviso en consola y cae a primitivas.
- **9-slice:** los bordes (en píxeles del PNG) congelan las tapas/marco; solo el
  centro se estira. Mismos mecanismos y validación que `caja_sprite` (ver la
  sección "9-slice scaling"). Default 0 en los 4 = stretch puro. **Recomendado**
  para botones tipo barra: sin 9-slice, un arte de tamaño fijo se deforma al
  estirarse al rect del botón (que sale de `choice_ancho_ratio` / `choice_altura`,
  no del tamaño del PNG).
- **Sonido:** el de hover suena **una vez** al entrar a cada botón (no cada frame);
  el de click al confirmar. Formato: **WAV / MP3 / .OGG / FLAC**.
- Con `choice_sprite`, los colores `choice_fondo_color` / `choice_borde_color` se
  ignoran (el PNG manda); el color de **texto** sí se sigue aplicando encima.

### Movimiento del botón de opción — juice de hover (v0.6.20)

La opción bajo el cursor puede **crecer suave** y volver a su tamaño al salir — el
"feel" de un menú vivo, como el de muchos juegos. Una sola key:

| ES | EN | Qué hace |
|----|----|----------|
| `choice_hover_escala` | `choice_hover_scale` | Escala de la opción al pasar el mouse. `1.0` = apagado (estático). `1.06` ≈ crece 6%. |

    choice_hover_escala  1.06     # crece 6% al hover

- **Default `1.0` = apagado.** Si no declarás la key, las opciones quedan estáticas
  (idéntico a antes). El engine no impone movimiento — el tema **opta**.
- **Crece desde el centro:** se infla la **placa**; el **texto queda firme**. El
  área de clic NO cambia (sigue siendo el tamaño de reposo) → no hay parpadeo de
  borde al entrar/salir.
- **Suave en ambos sentidos**, independiente del framerate (no es un salto).
  Valores típicos `1.04`–`1.10`; más alto = más "punchy" (a escalas muy grandes el
  texto se ve un pelín suave — eso lo afina la futura migración a SDF).
- Combina con todo lo de arriba: el **sonido de hover + la escala** juntos son el
  combo clásico de un menú que responde.

### Posición del bloque de opciones (v0.5.3)

Por defecto el bloque de opciones aparece **centrado** en pantalla. Con dos keys
de tema lo anclás donde quieras — una esquina, un costado, arriba:

| Key (ES) | Key (EN) | Qué hace |
|----------|----------|----------|
| `choice_pos_x` | `choice_pos_x_ratio` | Posición horizontal del **centro** del bloque (ratio 0..1 del lienzo) |
| `choice_pos_y` | `choice_pos_y_ratio` | Posición vertical del **centro** del bloque (ratio 0..1 del lienzo) |

```
choice_pos_x  0.85      # bloque hacia la derecha
choice_pos_y  0.80      # y hacia abajo → esquina inferior-derecha
```

- **Ratio 0..1** sobre el lienzo lógico (no píxeles): `0` = borde izq/arriba,
  `1` = borde der/abajo, `0.5` = centro. Resolución-independiente.
- El ancla es el **centro del bloque**, no la esquina de un botón. `0.5 / 0.5`
  (el default) deja las opciones exactamente donde siempre estuvieron.
- El **tamaño** de cada botón sigue saliendo de `choice_altura` / `choice_ancho_ratio`,
  y la separación de `choice_espaciado`. La posición solo mueve el bloque entero.
- Las opciones siguen apilándose en vertical. (Dispersarlas opción-por-opción o
  ponerlas en fila horizontal son extensiones futuras.)

### Secuencia de teclas (dentro de un `elegir`)

Un renglón `secuencia` (o `sequence`) vive **dentro del bloque de un `elegir`**, al lado de las opciones. No es un comando suelto del guion: se arma cuando ese `elegir` se ejecuta, oye teclas solo mientras la espera está viva, y deja de existir cuando la espera termina (por opción, por tiempo, o porque la secuencia misma disparó la salida).

```
elegir:
  "Ver" -> ver
  "Salir" -> salir
  secuencia ARRIBA ARRIBA ABAJO ABAJO IZQUIERDA DERECHA IZQUIERDA DERECHA B A -> poema

choose:
  "Look" -> look
  sequence UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A -> poem
```

**Forma.** `secuencia TECLA TECLA ... -> destino` (hace falta **al menos dos** teclas, la flecha y un destino no vacío). Separás las teclas con espacio o con coma. El alias inglés es `sequence`. La forma pegada (`"texto" elegir:`) también las lleva: el renglón pasa por el mismo armado del bloque.

**Nombres de tecla.** Los mismos de la tabla única del motor — citá §12, *«Los nombres de tecla que podés escribir»*; no se repiten acá a propósito. Mouse y teclas del dominio `dev` (p. ej. `F3`) **no** sirven como miembros: el motor avisa una vez (con la palabra como la escribiste y el número de línea) y ese renglón queda muerto; las opciones y las demás secuencias del mismo bloque siguen.

**Gate.** Podés poner `[si: ...]` / `[if: ...]` delante, igual que en una opción. Si el gate es falso al armar, esa secuencia no escucha.

**Qué hace al completar.** Cierra la lista y su reloj, despacha el destino (escena, mutación o pantalla, la misma forma que una opción) **sin** sonido de clic y **sin** `on_choice_made`. Las teclas del medio no se tragan: el foco del menú y el resto del input siguen actuando como sin la secuencia; solo la tecla que completa se consume (así una `A` final no prende el auto).

**Qué teclas poner en el medio.** Como las teclas del medio no se tragan, una tecla que tiene acción en la novela la hace **a mitad del código**: abrir la pausa o el historial, elegir una opción, el guardado rápido… (las de fábrica están en §17, *«Controles del Jugador»*, y el jugador las puede remapear). En el medio usá teclas que no hagan nada durante un `elegir`: las flechas mueven el resaltado y nada más, y una letra sin acción no hace nada. Una tecla con acción sirve como **la última**, que es la única que la secuencia se queda. El Konami cumple solo: flechas, `B` y `A`.

**Dónde no vale.** Fuera de un bloque, o dentro de un `esperar_click`, el parser **avisa y descarta** el renglón (no lo trata como «linea no reconocida» genérica, y no se come a sus vecinos). Un `elegir` que solo tiene secuencias y cero opciones **no se emite** (misma voz de hoy: sin opciones).

**Save / load.** Guardar a mitad de tecleo y cargar **re-arma en cero**: la otra mitad no alcanza; hay que teclearla entera de nuevo. El progreso no sobrevive a salir del `elegir` (reloj, opción, u otro salto).

> La secuencia oye **después** del historial y **antes** de la caja. Con el historial abierto las teclas no cuentan (ni avanzan ni reinician). Una tecla sostenida (auto-repeat) tampoco avanza.


---

## 12. Quick-Time Events (QTE)


```
qte [tecla: E, tiempo: 3s]:
  exito -> escena_exito
  fallo -> escena_fallo

qte [key: E, time: 3s]:
  success -> scene_success
  fail -> scene_fail
```

- Muestra un cuadro grande con la tecla a presionar
- Barra de tiempo con gradiente verde→rojo
- Si presiona a tiempo: va a la escena de éxito
- Si no presiona: va a la escena de fallo

### Parámetros

| Español | English | Descripción |
|---------|---------|-------------|
| `tecla:` | `key:` | Tecla requerida (ver la tabla de abajo) |
| `tiempo:` | `time:` / `timer:` | Segundos para reaccionar |

Default sin parámetros: tecla E, 3 segundos.

> **Mientras el reloj corre, las demás teclas siguen haciendo lo suyo:** `ESC` abre
> la pausa, `K` guarda. El QTE se queda **sólo con la tecla que pediste** — así que
> si escribís `qte [tecla: ESC]`, ahí el `ESC` es del QTE y no abre nada.

#### Los nombres de tecla que podés escribir (v0.8.32)

Antes eran cuatro (`A`-`Z`, `Space`, `Enter`, `Shift`). Ahora `tecla:` lee la
**tabla única de nombres del motor**, así que entienden todos éstos — en español o
en inglés, y sin importar mayúsculas o minúsculas:

| Familia | Español | English |
|---------|---------|---------|
| letras y números | `A`…`Z` · `0`…`9` | igual |
| la eñe *(v0.8.34)* | `Ñ` (y `ñ`) | igual |
| confirmar / editar | `ESPACIO` · `ENTER` · `ENTER_NUM` · `ESC` · `TAB` · `RETROCESO` · `SUPR` | `SPACE` · `RETURN` · `KP_ENTER` · `ESCAPE` · `TAB` · `BACKSPACE` · `DELETE` |
| navegación | `ARRIBA` · `ABAJO` · `IZQUIERDA` · `DERECHA` · `REPAG` · `AVPAG` · `INICIO` · `FIN` | `UP` · `DOWN` · `LEFT` · `RIGHT` · `PAGEUP` · `PAGEDOWN` · `HOME` · `END` |
| modificadores | `SHIFT_IZQ` · `SHIFT_DER` · `CTRL_IZQ` · `CTRL_DER` · `ALT_IZQ` · `ALT_DER` | `LSHIFT` · `RSHIFT` · `LCTRL` · `RCTRL` · `LALT` · `RALT` |
| función | `F1`…`F12` | igual |
| Android | `ATRAS` | `BACK` |

`SHIFT`, `CTRL` y `ALT` a secas siguen valiendo, y son **el modificador
izquierdo** — igual que antes.

> **La `Ñ` llegó tarde y por una razón que vale contar:** las letras no son filas
> de esta tabla, son una **regla** (`A`-`Z` y `0`-`9`, una sola letra), y la eñe
> ocupa **dos bytes**, así que se caía por el borde. Era la única tecla del
> teclado español que este motor no podía ligar. Ahora es una fila propia. Las
> vocales acentuadas **no** entran: en un teclado español no son teclas, son
> composiciones.

> **Un nombre que el motor no conoce sigue cayendo a `E`**, como siempre. Es la
> red de este comando y no cambió: un `qte [tecla: PATO]` te da un QTE de la tecla
> E, no un error.

> **Los nombres de mouse (`CLIC`, `RUEDA_ARRIBA`) existen en el motor pero NO
> sirven acá**: el QTE espera una tecla del teclado, así que un `tecla: CLIC` cae
> a la red y te da `E`. Los vas a poder usar cuando llegue el bloque de
> reasignación de controles.

> **El QTE no lleva `defecto:`, y no es un olvido: ya tiene uno, y se llama `fallo`.** Un `elegir` con reloj tiene varias salidas y ninguna se llama "la del tiempo agotado", por eso hay que decirle cuál. Un QTE tiene **una sola**, y está escrita con su nombre dos renglones más abajo, en el propio bloque. Si escribís `defecto:` en un `qte`, el motor te lo dice y te manda a `fallo`.

> **`exito` y `fallo` son palabras completas.** `exitoso -> escena` no cuenta como `exito` — el motor te avisa con su línea en vez de aceptarlo callado. Y podés poner los comentarios y líneas en blanco que quieras entre el header y las dos ramas: no gastan cupo.

### Posición del bloque QTE (v0.5.3)

El conjunto del QTE (barra + caja de tecla + hint "¡Presiona!") se posiciona con
dos keys de tema, mismo idioma de ratio que `choice_pos_*`:

| Key (ES) | Key (EN) | Qué hace |
|----------|----------|----------|
| `qte_barra_pos_x` | `qte_bar_pos_x_ratio` | Centro horizontal del bloque (mueve barra, caja y hint juntos) |
| `qte_barra_pos_y` | `qte_bar_pos_y_ratio` | Altura vertical de la barra |

Defaults: `0.5` (centrado horizontal) y `0.3`.

---

## 13. Navegación / Navigation

### Saltar a escena / Goto

```
ir_a final
goto ending
```

Salta inmediatamente a la escena indicada. Los comandos después del `ir_a` en la escena actual no se ejecutan.

### Verbos shell / Shell verbs (v0.4.15)

El destino de un `->` (en `elegir` y en `esperar_click`) no tiene por qué ser una escena: también puede ser un **verbo shell** reservado, que ejecuta una acción del cascarón del juego en vez de navegar.

```
elegir:
  "Guardar partida"  -> guardar
  "Cargar"           -> cargar
  "Opciones"         -> config
  "Volver al juego"  -> menu
  "Salir"            -> salir
```

```
esperar_click:
  "icono_guardar"  -> guardar
  "icono_config"   -> config
```

| Verbo (ES) | Verbo (EN) | Acción |
| --- | --- | --- |
| `guardar` | `save` | Abre el menú de guardado |
| `cargar` | `load` | Abre el menú de carga |
| `menu` | `menu` | Abre el menú de pausa |
| `historial` | `history` | Abre el backlog de diálogos |
| `config` | `settings` | Abre Configuración |
| `salir` | `quit` | Cierra la aplicación |

**Precedencia — la escena siempre gana:** si existe una escena con el mismo nombre que un verbo (ej. una escena llamada `menu`), el `->` navega **a la escena**, no al verbo. Por eso agregar verbos nuevos nunca rompe un script viejo. Si un destino no es ni escena ni verbo conocido, el clickable no hace nada (y se registra el error en consola).

## Tu menú es una escena / Your menu is a scene

En Eclips **el menú principal no es un sistema aparte** — es un `.ecl` normal
corriendo en *rol Menu*. No hay un segundo lenguaje de menús (a diferencia del
screen language de Ren'Py): tu menú usa el vocabulario completo —fondo, música,
personajes, glitches—, así que un menú diegético (que es parte del mundo del juego) sale gratis.

### Conectar tu menú (manifest)

En `proyecto.eclips`:

    menu "menu_demo.ecl"     # el .ecl que actúa como menú principal

- **Con `menu`** → el motor arranca ESE `.ecl` como menú, directo.
- **Sin `menu`** → arranca el menú interno por defecto.
- **Ruta inexistente** → aviso en consola + menú interno (siempre se ve algo).

`menu` es independiente de `script_inicial`: `menu` es la pantalla de inicio;
`script_inicial` es el script del JUEGO (lo que `nuevo_juego` lanza).

Para lanzar el juego desde tu menú usá `nuevo_juego`, no `cargar_script`: `cargar_script` cambia el guion pero no el rol, y el juego quedaría corriendo como menú, sin pausa ni guardado.

### Tus pantallas también son escenas — override (v0.6.7)

Igual que el menú principal, las **pantallas del juego** (Configuración, guardar,
cargar y pausa) pueden ser un `.ecl` tuyo que **sustituye** la
pantalla interna del motor. Así un menú de opciones estilo Ren'Py —con tu
fondo, tu música, tus controles— sale del mismo vocabulario, sin un lenguaje de
pantallas aparte.

Hoy puedes overridear **`config`** (Configuración), **`cargar`/`load`** (pantalla de
Carga, vía `ranura`), **`guardar`/`save`** (pantalla de Guardado, también vía
`ranura`, **desde v0.8.49**) y **`pausa`/`pause`** (menú de pausa, navegacional). La pausa corre en rol
Menu sobre el juego: `fin` reanuda (revela el juego), y ESC también vuelve;
`config`/`titulo`/`salir` navegan con sus verbos. Es una pantalla **opaca** (pon tu
`fondo`). Las no soportadas se avisan en consola y usan la pantalla interna.

**`guardar` tiene una condición que las otras no, y conviene saberla antes de
probarlo:** sólo se abre desde una **pausa sobre el juego**, que es la única
situación en la que hay una partida que guardar. Desde el menú **título** el motor
lo suprime a propósito —no hay nada abajo que snapshotear—, así que si abres tu
`guardar.ecl` desde el título no va a aparecer, **y eso no es un fallo**. Tu
pantalla hereda el juego congelado de abajo, así que un `ranura accion=guardar`
dentro de ella guarda **esa** partida y no la pantalla en la que estás.

**Registrar el override** — bloque `pantallas {}` en `proyecto.eclips`:

    pantallas {
        config "config.ecl"
    }

- **Con override** → cuando el jugador abre Configuración (un `-> config` en un
  `elegir`/`esperar_click`), se abre TU `config.ecl` en rol Menu, encima del juego.
- **Sin override, o ruta rota/inexistente** → se usa la pantalla interna del motor
  (nunca te quedás sin Configuración).

### Flujos libres llamados desde el juego (FLF-01)

El mismo bloque `pantallas {}` también puede registrar una pantalla libre. El
nombre es un identificador ASCII en minúsculas: empieza por `a-z` y continúa
con `a-z`, `0-9` o `_`. Las siete claves nativas (`config`, `cargar`, `load`,
`guardar`, `save`, `pausa`, `pause`) conservan su routing dedicado.

    pantallas {
        ayuda "ayuda.ecl"
    }

Se llama desde cualquier enlace, `esperar_click`, `elegir` o pool de palabras:

    "Ayuda" -> ayuda
    "Ayuda" -> ayuda al_volver=reponer
    "Ayuda" -> ayuda al_volver=continuar

La forma sin política y `al_volver=reponer` vuelven a la espera que originó la
llamada. `continuar` consume esa espera y ejecuta el comando siguiente al
cerrar la pantalla. También se aceptan las grafías inglesas, incluso mezcladas:

    "Help" -> help on_return=restore
    "Help" -> help on_return=continue

Sólo se acepta una política. Una clave o valor desconocido, un valor vacío, un
duplicado de política, un nombre inválido o un nombre reservado se avisa y no
abre la pantalla. Si el archivo registrado no existe o no parsea, el motor
avisa y conserva el origen; no hay fallback silencioso. Si una pantalla libre
comparte nombre con una escena local, la forma simple salta a la escena y lo
avisa; usa `al_volver=` para pedir explícitamente el archivo registrado.

La pantalla libre es un estado de menú suspendido: su cierre normal es el que
arma el regreso. FLF-01 no promete audio continuo durante Ayuda.

**Escribir la pantalla** — es una escena normal con `control`s y un "Volver":

    escena config
      fondo "fondos/config.png"

      control musica_volumen    x=140 y=260 ancho=420
      control sonido_volumen    x=140 y=350 ancho=420
      control velocidad_texto   x=140 y=440 ancho=420
      control pantalla_completa x=140 y=530
      control skip_no_visto     x=140 y=600

      elegir:
        "Volver" -> fin

**El "hold" del menú:** una pantalla no es un script lineal — tiene que *esperar*
a que el jugador se vaya, no correr hasta el final. La forma natural (la misma del
menú principal) es **terminar en una espera interactiva**: un `elegir:` o un
`esperar_click:`. Mientras esa espera está activa, tus `control`s siguen vivos
(arrastrables). El destino `-> fin` cierra la pantalla y vuelve al juego.

> El override dispara por el **verbo shell `config`** (`-> config`), y el botón
> "Opciones" del menú de pausa interno **también lo respeta**: pasa por la misma
> puerta del VN. La excepción es el **gameplay 2D**, cuya pausa no cablea esa ruta
> y cae a la pantalla interna del motor.

### Confirmar (¿seguro?) como patrón

Un diálogo de confirmación —"¿Seguro que quieres salir?", "¿Borrar este guardado?"— **no necesita un comando nuevo**: es un `elegir` con dos opciones, en su propia escena. A la izquierda del `->` puede ir una **escena** (la confirmación SIEMPRE gana sobre el verbo):

    escena menu
      fondo "fondos/titulo.png"
      ocultar_caja
      elegir:
        "Nueva Partida" -> nuevo_juego
        "Salir"         -> confirmar_salir   # salta a la escena de confirm

    escena confirmar_salir
      fondo "fondos/titulo.png"
      ocultar_caja
      texto preg "Seguro que quieres salir?" x=140 y=400 z=10
      elegir:
        "No" -> menu       # vuelve al menu, sin cerrar nada
        "Si" -> salir      # el verbo cierra la app

- **"No" -> menu** regresa a la escena del menú **sin cerrar nada**: saltar entre escenas del mismo `.ecl` no toca el stack — sólo navega.
- **"Si" -> salir** corre el verbo (`salir`/`quit` cierra; o `fin`, etc.).

**Cuidado — el intro va en su propia escena.** Saltar a una escena la corre DESDE EL PRINCIPIO. Si tu menú tiene una entrada de una-sola-vez (un fade del título), ponla en una escena `intro` que fluya al menú con `ir_a menu`; si la pones en `menu`, cada "No" la repetiría. (`fondo`/`capa`/`texto`/`control` son idempotentes por nombre: re-entrar no los duplica.)

> **Hoy es un patrón, no un overlay.** Como es un salto de escena, la confirm **reemplaza** la pantalla (no se dibuja translúcida ENCIMA del menú). Para el look modal —menú atenuado detrás, "No" por default, ESC para cancelar— redibujá el fondo + una capa de oscurecido en la escena de confirm. Un comando `confirmar` con overlay real llegará en una versión futura.

### Verbos de ciclo de vida / Lifecycle verbs (v0.5.0)

Se usan como destino de un `elegir`/`esperar_click` (igual que `guardar`/`menu`):

| Verbo / Verb | Hace | Notas |
|---|---|---|
| `nuevo_juego` / `new_game` | Lanza el juego desde cero (`script_inicial`) | Desde el menú, o como "reiniciar" dentro del juego |
| `continuar` / `continue` | Carga la partida MÁS RECIENTE directo (sin pantalla de Load) | Solo desde el menú (rol Menu). Gátealo con `[si: _saves > 0]`. ≠ `cargar` (que abre la pantalla a elegir slot) |
| `titulo` / `title` | Vuelve al menú principal | Baja todo al fondo; el menú reanuda *congelado* (música seamless) |
| `fin` / `end` | Sube un nivel | Desde el juego → revela lo de abajo; desde el fondo → cierra la app |
| `salir` / `quit` | Cierra la app | Cierre directo, sin importar la profundidad |

`fin` también existe como **comando suelto** (línea propia, sin `->`): termina la
escena/script actual. Diferencia con `titulo`: `fin` sube UN nivel; `titulo` baja
TODO hasta el menú.

> ¿Más simple que hotspots? Usa `elegir:` — botones de texto clickeables, sin
> coordenadas. Tu menú, tu decisión.

### Fin / End

```
fin
end
```

Termina la ejecución del script y vuelve al menú principal (o cierra el estado VN).

### Encadenar scripts / Script Chaining

```
cargar_script "vnscript/acto2.ecl"
load_script "vnscript/act2.ecl"
```

Carga un nuevo archivo `.ecl` y salta a su primera escena. El script actual se reemplaza por el nuevo, pero **todo el estado se preserva**:

- Variables (`set`)
- Personajes activos en pantalla
- Fondo y música actuales
- Capas, hotspots, eventos random
- Historial de diálogos

Esto permite dividir juegos largos en capítulos/actos sin perder continuidad.

**Con escena destino:**

```
cargar_script "vnscript/acto2.ecl" medio
load_script "vnscript/act2.ecl" middle
```

Salta directamente a una escena específica del nuevo script en vez de la primera.

**Notas importantes:**

- Los personajes definidos en el script anterior siguen funcionando si están en pantalla
- El nuevo script puede redefinir personajes existentes (la nueva definición gana)
- Las escenas del script anterior dejan de existir — solo las del nuevo script están disponibles
- Save/load funciona correctamente: al guardar se registra qué script está activo, y al cargar se re-parsea el script correcto
- Dos scripts pueden tener escenas con el mismo nombre sin conflicto (ej: ambos pueden tener "inicio")

---

## 14. Speaker Highlight / Resaltar Hablante

Efecto visual que destaca al personaje que está hablando y oscurece a los demás. Desactivado por defecto — el creador lo activa con un comando EclipScript.

Visual effect that highlights the speaking character and dims the others. Disabled by default — the creator activates it via EclipScript command.

### Activar / desactivar

```
resaltar_hablante si
resaltar_hablante no

highlight_speaker yes
highlight_speaker no
```

### Efecto

- **Personaje que habla:** brillo normal + zoom 1.05x anclado al fondo (los pies no se mueven)
- **Personajes no-hablantes:** oscurecidos a 55% brillo
- **Durante narración** (sin personaje): todos se oscurecen

### Notas

- Es una decisión estilística, no universal. El creador decide si usarlo.
- El estado se guarda y restaura con las partidas.
- Funciona tanto con personajes normales como compuestos.

---

## 14.5 Ocultar / Mostrar Caja de Diálogo / Hide & Show Dialog Box

Comandos para esconder y volver a mostrar la UI de la caja de diálogo durante una escena. Permite cinemáticas silenciosas en medio del flujo VN: cambios de sprite, sonidos, fades de fondo, transiciones de tiempo (día/noche/día/noche), o "el momento" de un evento dramático sin la caja del diálogo previo flotando en pantalla.

Commands to hide and re-show the dialog box UI during a scene. Enables silent cinematics in the middle of a VN flow: sprite changes, sounds, background fades, time transitions, or "the moment" of a dramatic event without the previous dialog box hanging on screen.

### Sintaxis

```
ocultar_caja
mostrar_caja
hide_box
show_box
```

Ambos son comandos sin argumentos. Standalone, una línea.

### Comportamiento

- `ocultar_caja` esconde la caja, el nameplate y el texto. Los **sprites de personajes** y el **fondo** siguen visibles. Los choices y QTE no se ven afectados (siguen mostrándose si están activos).
- `mostrar_caja` re-muestra la caja **con el último diálogo** (si lo había). **No limpia el texto, y no da un beat de caja vacía.** `mostrar_caja` es instantáneo —no cuesta frames—, así que lo que vuelve a verse es la línea anterior, no una caja en blanco. Si querés una caja vacía antes del próximo texto, **hoy no existe**.
- **`mostrar_caja` es también cómo pedís que la caja SOBREVIVA a un `elegir`** *(v0.8.29)*. Ver **14.5.bis**, acá abajo.
- **Auto-mostrar al iniciar diálogo:** un `personaje: "texto"` o `dialogo` después de `ocultar_caja` re-muestra la caja automáticamente. NO hace falta escribir `mostrar_caja` antes de cada nuevo diálogo.
- **`mostrar_caja` pelado es INSTANTÁNEO** *(v0.8.26)*: la caja aparece entera y las letras arrancan enseguida, **sin el fundido ni el retraso de texto que declare el tema** (`caja_aparicion_duracion` / `caja_aparicion_texto_retraso`). La regla, en una frase:

  > **El fundido es lo que la caja hace SOLA. `mostrar_caja` es cómo pedís que NO lo haga.**

  Una caja que reaparece **sola** —el auto-mostrar de la línea de arriba— sí hereda el tema. Así que si querés la entrada con efecto, **no escribas el comando**: dejá que la línea de diálogo la traiga.

  ```
  ocultar_caja
  esperar 1
  x: "vuelve con el fundido del tema"      # aparición automática -> hereda el tema

  ocultar_caja
  esperar 1
  mostrar_caja                              # yo mando: de golpe
  x: "vuelve de golpe, sin fundido"
  ```

  La línea que viene pegada abajo **se incorpora** a esa aparición: no la vuelve a armar ni le devuelve el efecto del tema. Pedir un efecto concreto para una aparición puntual (*«mostrala fundiendo, 0.4 s»*) todavía no existe.
- **Una aparición se resuelve una sola vez** *(v0.8.26)*: lo que decide cómo entra la caja —duración del fundido y retraso del texto— **se captura al empezar la aparición**. Cambiar de caja a mitad de camino con `usar_caja`, o cambiar de hablante, cambia el **look** de la caja pero **no** la entrada que ya está corriendo. Antes de esta versión la duración se releía en cada cuadro, así que un `usar_caja` a media entrada movía la opacidad hacia adelante o hacia atrás.

### Ejemplo: cinemática "Estrella se fue"

```
escena estrella_se_fue
mostrar e 2l_2r_a centro
e: "Espérame aquí, ¿sí?"
ocultar_caja
esperar 2.0
sonido "puerta_cerrar.ogg"
esperar 1.5
mostrar e 2l_2r_b
esperar 1.0
ocultar e [efecto: deslizar]
esperar 1.0
fondo "habitacion_vacia.png"
esperar 2.0
l: "...se fue."
fin
```

Lo que ve el jugador:
1. Estrella dice "Espérame aquí, ¿sí?".
2. Click → la caja desaparece. Estrella sigue en pantalla.
3. 2 segundos en silencio absoluto. Solo Estrella y el fondo.
4. Sonido de puerta cerrándose.
5. Estrella cambia de expresión (ya no sonríe).
6. Estrella sale de escena deslizándose (desde el centro, por la derecha).
7. El fondo cambia a la habitación vacía.
8. La caja vuelve con Luna ("…se fue."): el `l:` la mostró solo.

Sin `ocultar_caja`, la caja con "Espérame aquí, ¿sí?" estaría plantada en pantalla durante toda la cinemática, arruinando el ritmo.

### Composición con otros comandos

`ocultar_caja` solo afecta la **UI del diálogo**. Es ortogonal y componible:

| Comando | ¿Lo afecta `ocultar_caja`? |
|---------|----------------------------|
| `mostrar` / `ocultar` (sprites) | No — siguen funcionando normal |
| `fondo`, `transicion` | No — siguen funcionando normal |
| `animar` | No — sigue corriendo en sprites |
| `sonido`, `musica` | No — el audio es independiente del UI |
| `vigneta` | No — ortogonal |
| `choices` (`elegir`) | No — los choices se muestran igual |
| `qte` | No — el QTE se muestra igual |
| Menú ESC (Save/Load/Settings/Salir) | No — el jugador puede acceder al menú |

Si querés también ocultar el sprite del personaje activo durante la cinemática, usá `ocultar` aparte:

```
ocultar_caja
ocultar e         # también esconde a Estrella
esperar 2.0
mostrar e 2l_2r_b
mostrar_caja
e: "..."
```

### Skip y Auto

- **Skip** atraviesa el `esperar N` que sigue a `ocultar_caja` (skip cancela el ritmo del creador).
- **Auto** también atraviesa los `esperar`. Pero en una cinemática puramente visual sin diálogo, auto y skip se ven similar (no hay diálogo donde aplicar timing de auto).

Si querés que una cinemática NO sea skipeable, hoy no es posible — es una limitación conocida. Una posible vía futura sería un comando `escena_no_skipeable` o similar.

### Save / Load

El estado de la caja (oculta o visible) se persiste en saves. Si guardás durante una cinemática con la caja oculta, al cargar la caja seguirá oculta.

**Importante (estado discreto, no continuo):** los timers de `esperar` en progreso NO se persisten. Si guardás a la mitad de un `esperar 5.0`, al cargar el `esperar 5.0` empieza de cero (5 segundos completos). Lo mismo aplica a fades, sonidos one-shot y transiciones en curso. Esto es intencional — persistir tiempos continuos es fuente de bugs sutiles. Lo discreto (caja oculta sí/no, índice del comando, sprites en pantalla) sí se persiste con fidelidad.

### Notas

- La caja oculta solo afecta `renderDialogBox`. El typewriter sigue corriendo si había uno activo (aunque no se ve), por lo que `ocultar_caja` mid-typewriter es un caso de uso poco probable y no especialmente útil. Lo natural es ocultar la caja **entre líneas**, después de un click.
- No es para uso constante — es una herramienta cinematográfica. Para diálogos normales con pausas, usá `esperar` o tags como `{pausa=N}`.

---

## 14.5.bis Elegir SIN ocultar la caja / Choices with the box kept *(v0.8.29)*

Normalmente, un `elegir` **hace desaparecer la caja de diálogo**: aparecen las
opciones y el texto que venías leyendo se va. Es lo que hacen casi todas las
visual novels, y sigue siendo el comportamiento por defecto — **no cambió nada
en tus guiones**.

Pero a veces querés lo contrario: que la pregunta **siga leyéndose** mientras
elegís. El caso típico es un aviso.

```eclips
"iniciar protocolo?"
mostrar_caja
elegir:
  "aceptar" -> protocolo_si
  "negar"   -> protocolo_no
```

Escribí **`mostrar_caja` entre la línea y el `elegir`**, y la caja se queda con
esa línea puesta. Las opciones salen arriba, como siempre.

### La regla, en una frase *(reescrita en v0.8.30)*

> **La caja de diálogo muestra la línea que está VIVA.**

`elegir` **no oculta nada** — lo que pasa es que, cuando el `elegir` va en su
propio renglón, vos ya diste el click y esa línea **quedó atrás**: no hay nada
que mostrar. `mostrar_caja` es *«conservámela aunque ya la haya pasado»*.

Con esa sola idea caen las tres formas:

| Forma | ¿Cuántos avances hasta las opciones? | ¿Se ve la caja? | Para qué |
|---|---:|---|---|
| `"línea" elegir:` (§14.5.ter) | **0** | sí | el aviso, la pregunta directa |
| `"línea"` + `elegir:` | 1 | no | el menú |
| `"línea"` + `mostrar_caja` + `elegir:` | 1 | **sí** | el beat dramático: leés, pasás, y la línea se queda mientras elegís |

*(Hasta v0.8.29 esta sección decía **«`elegir` oculta la caja; `mostrar_caja` es
cómo pedís que no lo haga»**. Describe la misma conducta —no cambió nada— pero
necesitaba una regla por forma. La de arriba las explica con una.)*

### El pedido dura UNA línea

Una línea de diálogo nueva **apaga el pedido**. Es la misma vida útil que ya tenía
la caja oculta (un diálogo nuevo siempre la re-muestra), y evita que un
`mostrar_caja` escrito hace veinte renglones te cambie un `elegir` lejano.

Consecuencia práctica: **el orden importa**, y si lo escribís al revés no se
rompe nada — te da el comportamiento de siempre.

```eclips
"pregunta?"
mostrar_caja
elegir:            # la caja SE QUEDA

mostrar_caja
"pregunta?"        # esta linea apago el pedido
elegir:            # la caja se va, como siempre
```

`ocultar_caja` también lo apaga: si pedís esconder, no reaparece.

### Lo que este comando NO hace

**Preserva la caja tal cual está, y nada más.** No le quita los botones y no
limita el guardado. Los dos son cosas aparte, y ya existen:

| Lo que querés | Cómo se pide |
|---|---|
| la caja **sin** sus botones | `botones_visibles no` dentro de un `caja_dialogo` con nombre, + `usar_caja` (§14.6) |
| que no se pueda guardar ni abrir menú | un aviso modal: hoy lo arma un `enlace … requerido` |

> **Durante un `elegir`, sólo se puede clickear las opciones.** Eso vale con la
> caja visible o sin ella: los botones de la caja **se ven pero no responden**, y
> es a propósito. Si preferís que no se vean, usá `botones_visibles no`.

### Con qué se compone

- **Las opciones van encima de la caja.** Con las posiciones por defecto ni se
  tocan: las opciones viven en el centro de la pantalla y la caja abajo.
  `choice_pos_y` las mueve si tu caja es muy alta.
- **El texto queda congelado**, con la línea completa — cuando llegás acá ya
  clickeaste para pasar esa línea, así que no queda nada por escribir.
- **Lo que nunca corre durante una elección es el AVANCE**: ni auto, ni skip, ni
  `{interrumpir}`. Sólo salís eligiendo una opción. *(El texto sí puede seguir
  escribiéndose, pero eso pasa en la forma de abajo — ver 14.5.ter.)*
- **El fundido de la caja no se contagia a las opciones.** Son dos superficies
  distintas.

---

## 14.5.ter Las opciones en el MISMO renglón que la línea *(v0.8.30)*

La regla, y no hay más que ésta:

> **Lo que va en el mismo renglón pasa al mismo tiempo.**

### Las dos formas, lado a lado

```eclips
# EN DOS RENGLONES — la línea primero, las opciones después de un click
escena dos_renglones
"¿iniciar protocolo?"
elegir:
  "proceder" -> protocolo_si
  "denegar"  -> protocolo_no

# EN UN RENGLÓN — la línea y las opciones llegan JUNTAS
escena un_renglon
"¿iniciar protocolo?" elegir:
  "proceder" -> protocolo_si
  "denegar"  -> protocolo_no
```

Las dos son válidas y hacen cosas distintas **a propósito**:

| Forma | Cuándo aparecen las opciones | Para qué sirve |
|---|---|---|
| en dos renglones | después de que el jugador avanza la línea | el beat dramático: leés, respirás, y recién ahí elegís |
| **pegada** | **junto con la línea**, sin clicks de por medio | el aviso, la pregunta directa, una decisión rápida |

**La forma de dos renglones sigue siendo la normal**: si tenés `elegir` escritos
en tu guion, no cambió ni uno.

### Con un personaje hablando, igual

```eclips
m: "¿Y bien?" elegir:
  "Sí" -> a
  "No" -> b
```

### Qué pasa mientras el texto se escribe

En la forma pegada las opciones ya están en pantalla **mientras la línea todavía
se escribe**. Ahí:

- un click **en cualquier lado que no sea una opción** completa el texto de
  golpe, y nada más — no avanza ni elige;
- un click **sobre una opción** elige, aunque el texto esté a medias;
- con el texto ya completo **no se avanza**: sólo una opción te saca de ahí;
- si tenías **skip** activado, el texto sale completo de una;
- si el `elegir` tiene `[tiempo: Ns]`, **el reloj espera** a que el texto termine
  de escribirse. `tiempo:` te da N segundos para *decidir*, no para leer.

### La caja se queda sola

En la forma pegada **no hace falta `mostrar_caja`**: unirse a una línea implica
que la caja se ve — si no, esa línea correría invisible. Es la regla de la línea
viva (§14.5.bis): no avanzaste esa línea, así que sigue ahí.

**Y por eso no existe la combinación «pegado pero sin caja»**: no hay renglón
donde meter un `ocultar_caja`, y escrito arriba de la línea no cuenta (toda
línea nueva vuelve a mostrar la caja). No es un olvido — es que esa forma no
significaría nada.

### Hoy sólo `elegir` se pega

Ningún otro comando puede ir en el mismo renglón que una línea. Si escribís
`"una línea" sonido "golpe.ogg"`, eso **no se ejecuta** — el motor lo lee como
parte del texto —, y desde v0.8.30 **te lo dice en la consola**.

> Si tu línea lleva comillas adentro y la palabra citada resulta ser un comando
> (`"él dijo "si" y ya"`), vas a ver ese aviso sin haber hecho nada malo. El
> propio aviso te dice que lo ignores: el motor no puede distinguir esos dos
> casos por la forma, sólo por la palabra.

### `{interrumpir}` no aplica

Ese tag pide que la línea avance sola al terminar, y en una elección no hay
avance que dar. Si lo escribís en una línea pegada, queda inerte y el motor lo
avisa.

---

## 14.5.quater Deshabilitar acciones / Disable actions *(v0.8.31)*

**Es diferente ocultar un botón a que realmente no haga su función.**

`botones_visibles no` (en el tema) esconde el chrome de la caja — y eso es
**todo** lo que hace: el jugador que aprieta `F5` sigue abriendo el menú de
guardado, y la rueda le sigue abriendo el historial. Este comando es la otra
mitad: apaga **la función**, la escriba el jugador con la tecla, con el botón o
con la rueda.

```eclips
escena advertencia

deshabilitar acciones

"Esta pantalla está hecha para verse una vez."
"¿INICIAR PROTOCOLO?" elegir:
  "PROCEDER" -> aceptado
  "DENEGAR"  -> denegado
```

**Las dos funciones son separadas y las dos son opt-in.** Por default el motor
**no oculta ni deshabilita nada** — un `.ecl` que no escribe este comando se
comporta exactamente como siempre.

### Las TRES familias de acciones

Ésta es la tabla que hay que mirar antes de escribir el comando: **el grueso
`acciones` no las cubre todas, y eso es a propósito.**

| Familia | Acciones | ¿La toca `deshabilitar acciones`? |
|---|---|---|
| **Salida** | `avanzar` · `opcion_1..4` · `foco_*` | **NO, nunca** — ver abajo |
| **Meta / servicios** | `guardado_rapido` · `carga_rapida` · `menu_guardar` · `menu_cargar` · `historial` · `menu` · `config` | **sí** |
| **Ritmo** | `auto` · `saltar_texto` | **sí** |

**`acciones` = Meta + Ritmo.** La familia **Salida** está fuera por
construcción: son las que dejan al jugador seguir adelante, y si se pudieran
apagar, una pantalla como la de arriba —que sale **únicamente** por
`PROCEDER`/`DENEGAR`— dejaría al jugador encerrado sin salida.

**Si intentás sellarlas, el motor no lo hace y te lo dice al cargar el `.ecl`:**

```
[EclipScript] AVISO linea 4: deshabilitar: `opcion_1` NO se puede deshabilitar.
Avanzar y elegir son la salida del jugador: sellarlas lo dejaria encerrado. Se
ignora esa palabra.
```

### Grueso, y después fino

Podés escribir el paquete y refinarlo, o listar acciones sueltas. **Manda el
orden en que están escritas**, igual que `botones_visibles no` +
`boton_menu_visible si` del lado del tema:

```eclips
deshabilitar acciones          # apaga las nueve
habilitar historial            # ...menos el historial

deshabilitar guardado_rapido, menu_guardar   # o sólo fino, sin grueso
```

### Los nombres, en los dos idiomas

| Español | English | Qué apaga |
|---|---|---|
| `acciones` | `actions` | **el grueso**: Meta + Ritmo |
| `guardado_rapido` | `quicksave` | el guardado rápido (`K`) |
| `carga_rapida` | `quickload` | la carga rápida (`L`) |
| `menu_guardar` | `save_menu` | el menú de guardado (`F5`, botón SAVE, `-> guardar`) |
| `menu_cargar` | `load_menu` | el menú de carga (`F9`, botón LOAD, `-> cargar`) |
| `historial` | `history` | el backlog (rueda, botón LOG, `-> historial`) |
| `menu` | `menu` | abrir el menú de pausa (`ESC`, botón MENU, `-> menu`) |
| `config` | `settings` | la pantalla de configuración (`-> config`) |
| `auto` | `auto` | el modo automático (`A`, botón AUTO) |
| `saltar_texto` | `skip` | el skip (`Ctrl`, botón SKIP) |

Podés mezclar idiomas dentro de la misma línea: `deshabilitar quicksave, historial`
es válido.

### Lo que hay que saber, y no es obvio

- **El sello va sobre la ACCIÓN, no sobre la tecla.** El historial se abre por
  **rueda**, por **botón** y por **tecla**, y `deshabilitar historial` tapa las
  tres de un saque. El día que las teclas sean remapeables, tu `.ecl` no cambia.
- **Un botón deshabilitado se ve apagado.** Si lo dejaste visible, se pinta en
  gris y no responde — el motor no finge que sirve.
- **El sello dura la escena.** Se apaga solo al entrar a una escena nueva, al
  `cargar_script` y al cargar una partida. Es la misma vida útil que
  `cinematica`. **Si querés que dure dos escenas, escribilo en las dos.**
- **No se guarda en la partida.** Una partida cargada nunca llega con acciones
  apagadas.
- **Sellar `auto` o `saltar_texto` los APAGA**, no sólo traba el botón: si el
  jugador tenía auto encendido cuando el comando corre, se apaga. Y un
  `habilitar auto` después le devuelve el permiso, no el estado — lo vuelve a
  prender él.
- **Sella al JUGADOR, no a tu guion**… con un borde que conviene tener claro: un
  `-> cargar` escrito en un `enlace`, un hotspot o una opción **sí queda
  sellado**, porque eso es el jugador clickeando. El motor te lo avisa en
  consola cuando pasa:
  `[deshabilitar] accion 'menu_cargar' sellada por el guion.`
- **`ESC` para *volver* de un submenú nunca se sella.** Ahí `ESC` no abre nada:
  cierra. Sellarlo te dejaría encerrado.
- **Esto no toca el click como superficie.** No existe un «no puedes clickear
  nada»: `deshabilitar` apaga acciones, no la pantalla.

### Lo que NO hace

Este comando **no** esconde los botones — eso sigue siendo del tema
(`botones_visibles`, `boton_*_visible`). Son dos ejes distintos y se combinan:
escondidos y apagados es la pantalla de advertencia clásica; visibles y apagados
es el botón gris.

---

## 14.6 Cambiar Caja de Diálogo / Change Dialog Box

Comando para switchear la plantilla visual de la caja de diálogo durante una escena. Las plantillas se definen en el `tema_*.eclips` con bloques `caja_dialogo nombre { ... }`.

Command to switch the dialog box visual template during a scene. Templates are defined in the `tema_*.eclips` file with `caja_dialogo name { ... }` blocks.

### Sintaxis

```
usar_caja alerta
usar_caja default
use_box alert
use_box default
```

`usar_caja X` activa la plantilla `X` definida en el theme. `usar_caja default` vuelve a la caja default del theme.

### Cadena de prioridad

La caja activa se resuelve con esta cadena (gana la primera que aplique):

1. **`usar_caja` mid-escena** — override explícito del creador. Sticky hasta `usar_caja default`.
2. **`caja` del personaje** — definida en `proyecto.eclips` dentro del bloque `personaje X { caja nombre }`. Se recalcula automáticamente al cambiar de speaker.
3. **Default del theme** — la caja definida por las keys sueltas del `tema_*.eclips`.

### Ejemplo: momento de alerta


En tema_pastel.eclips:
```
caja_dialogo alerta {
caja_color      #3D0A0A
caja_opacidad   95%
caja_borde      #FF2222
texto_color     #FFCCCC
}
```

En el .ecl:

```
l: "De repente, las luces parpadean."
usar_caja alerta
"Algo no está bien."
"Las sombras se mueven de forma extraña."
usar_caja default
l: "¿Qué fue eso?"
```

### Save / Load

El override de `usar_caja` se persiste en saves. Al cargar, la caja activa se restaura. Al cargar un nuevo script (`cargar_script`), el override se resetea automáticamente.

### Caja per-personaje

Si un personaje tiene `caja` definida en `proyecto.eclips`, esa caja se activa automáticamente cada vez que habla, sin necesidad de `usar_caja` en cada línea:


En proyecto.eclips:
```
personaje luna {
nombre "Luna"
path "assets/personajes/luna/"
compuesto si
caja pastel_clasica
}
```
En el .ecl — la caja de Luna se activa sola:

```
l: "Bienvenido al club."
```

Si la plantilla referenciada no existe en el theme, aparece un aviso en consola y se usa la caja default como fallback.

### Mostrar / ocultar botones del bottom-bar (chrome)

La barra de botones de la caja (AUTO · SKIP · SAVE · LOAD · LOG · MENU) se esconde **por plantilla**: cada `caja_dialogo` decide qué botones muestra. Útil para un **aviso o contrato** donde solo querés el mensaje (y, si hace falta, un `enlace` de "OK").

**Atajo — los seis de un saque:**

```
caja_dialogo aviso {
botones_visibles no        # ui_buttons_visible no
}
```

**Finos — uno por uno** (bilingües, `ui_btn_*_visible` en inglés):

```
boton_auto_visible  no
boton_skip_visible  no
boton_save_visible  no
boton_load_visible  no
boton_log_visible   no
boton_menu_visible  no
```

**Mezclar (preset + repesca):** el atajo es un preset grueso; un flag fino escrito **después** lo refina. Todo off menos el menú:

```
caja_dialogo aviso {
botones_visibles    no
boton_menu_visible  si     # repesca el MENU
}
```

El orden importa: se leen de arriba abajo, el atajo primero y los finos después.

**El "aviso con OK":** caja sin botones + un `enlace` que diga "OK" (§9.9):

```
usar_caja aviso

texto ok "Aceptar" x=900 y=500 estilo=menu

enlace ok                      # sin flecha = continuar

"Leé esto antes de seguir."

usar_caja default
```


> ⚠️ **Esconder un botón es estético, no funcional.** `botones_visibles no` apaga lo que se *ve* y se puede clickear, pero **no** las funciones por teclado: durante el aviso el jugador todavía puede mantener **Ctrl** (skip), **A** (auto), **K**/**F5** (guardar), **F9** (cargar). Un aviso que de verdad bloquee esos atajos es modalidad funcional (en el roadmap), no esta perilla.

### Etiquetas y layout de los botones (chrome)

Más allá de esconderlos, la barra del bottom-bar se puede **renombrar** y **reacomodar** por plantilla de `caja_dialogo`. Útil para localizar (`Save`→`Guardar`) o mover toda la barra.

**Renombrar — `boton_labels` / `ui_btn_labels`:** una cadena entre comillas con las 6 etiquetas separadas por comas, en orden fijo **AUTO, SKIP, SAVE, LOAD, LOG, MENU**. Solo cambia el texto; las acciones no se tocan. Si das menos de 6, las restantes quedan en su default.

```
caja_dialogo caja_sol {
    boton_labels "AUTO, SALTAR, GUARDAR, CARGAR, REGISTRO, MENÚ"
}
```

**Alineación — `boton_alineacion` / `ui_btn_align`:** dónde se ancla la barra dentro de la caja. Valores: `izq`/`left`, `centro`/`center`, `der`/`right` (default). Valor inválido → aviso en consola, conserva el actual.

**Layout fino** (px, per-caja):

| Key (ES) | Key (EN) | Qué controla | Default |
|---|---|---|---|
| `boton_altura` | `ui_btn_height` | alto de cada botón | 28 |
| `boton_padding_x` | `ui_btn_padding_x` | relleno horizontal interno (por lado) | 10 |
| `boton_gap` | `ui_btn_gap` | separación entre botones | 6 |
| `boton_margen_derecho` | `ui_btn_margin_right` | margen lateral (en `izq` hace de margen izquierdo) | 16 |
| `boton_margen_inferior` | `ui_btn_margin_bottom` | margen al borde inferior de la caja | 8 |

> El **ancho** de cada botón no se fija a mano: sale de `measureWidth(label)` + `boton_padding_x`×2. Por eso una etiqueta más larga ensancha el botón sola.

### Fuente, tamaño y PNG de los botones (chrome)

Tres perillas más, per-caja, hermanas de las de choices:

**Fuente — `boton_fuente` / `ui_btn_font`:** identificador (sin comillas) de una fuente ya cargada con `fuente <id> "..."`. Precedencia `boton_fuente > base`; NO pasa por `caja_fuente` (el chrome es independiente del cuerpo). Fuente inexistente → cae a base + 1 aviso en consola.

**Tamaño — `boton_tamano` / `ui_btn_size`:** tamaño de letra del label en px. `0` (default) = heredar baseline. Reescala SOBRE `boton_fuente`. Es el tamaño de la LETRA, no del botón — la altura del recuadro es `boton_altura` (perilla aparte). El ancho sigue a la letra solo.

**PNG (plate) — `boton_imagen` / `ui_btn_image`:** un PNG compartido por los 6 botones como fondo, con el label encima. `boton_imagen_hover` / `ui_btn_image_hover` para la cara al pasar el mouse (si falta, se queda el idle). 9-slice opcional: `boton_imagen_borde_{izq,der,arriba,abajo}` / `ui_btn_image_border_{left,right,top,bottom}` (px desde cada lado; 0 = stretch). Sin `boton_imagen` → rect+borde de siempre. El botón disabled (SKIP no permitido) atenúa el plate.

### Fuente per-caja / Per-box font

Una plantilla de caja puede declarar su **propia fuente** con `caja_fuente` (o `box_font` en inglés). Aplica al **cuerpo del texto y al nameplate** de esa caja. Los botones del bottom-bar (AUTO/SKIP/SAVE) mantienen la fuente base del juego — son chrome de UI. Las opciones de `elegir` y el QTE tienen ahora sus propias keys (`opciones_fuente` / `qte_fuente`, ver más abajo); por defecto heredan la base. El nameplate puede además llevar su **propia fuente distinta del cuerpo** con `nombre_fuente` (ver abajo) — útil cuando el nombre va en tipografía de título (ej.: cuerpo en Nunito, nombre en M PLUS Rounded Bold, dos fuentes que trae el motor).

A box template can declare its **own font** with `caja_fuente` (or `box_font`). It applies to the **body text and the nameplate** of that box. The bottom-bar buttons (AUTO/SKIP/SAVE) keep the game's base font — they are UI chrome. The `elegir` choices and the QTE now have their own keys (`opciones_fuente` / `qte_fuente`, see below); by default they inherit the base. The nameplate can additionally take its **own font, distinct from the body**, via `nombre_fuente` (see below).

En `tema_pixel.eclips`:

```
caja_dialogo combate {
caja_color   #000000
caja_borde   #FFFFFF
texto_color  #FFFFFF
caja_fuente  determination
}
```

En el `.ecl` (la fuente debe declararse dentro de una escena antes del primer uso):

```
escena pelea_final
fuente determination "assets/fonts/DeterminationMono.ttf" tamano=34
fondo "assets/bg/sala_oscura.png"
bardo: "El aire se vuelve pesado."
usar_caja combate
bardo: "Aquí ya no hay vuelta atrás."
```

#### Precedencia / Precedence

Tres mecanismos de fuente coexisten, en este orden de prioridad:

| Nivel | Mecanismo | Alcance | Donde se declara |
|---|---|---|---|
| 1 | `{fuente=X}…{/fuente}` inline | Mid-párrafo, por glifo (cuerpo) | En el diálogo |
| 1′ | `nombre_fuente` | Solo el nameplate (placa de nombre) | En `caja_dialogo { }` |
| 1″ | `opciones_fuente` / `qte_fuente` | Botones de `elegir` / tecla+hint de QTE | En `caja_dialogo { }` |
| 2 | `caja_fuente` | Caja entera (cuerpo + nameplate) | En `caja_dialogo { }` |
| 3 | `fuente "ruta.ttf"` (fontPath baseline) | Todo el juego | Al tope del `.eclips` |

Hay carriles que comparten la baseline:
- **Cuerpo:** `{fuente=X}` inline > `caja_fuente` > baseline.
- **Nameplate:** `nombre_fuente` > `caja_fuente` > baseline.
- **Choices:** `opciones_fuente` > baseline. **QTE:** `qte_fuente` > baseline. (NO pasan por `caja_fuente`: los choices/QTE nunca lo siguieron, así que el override es directo sobre la base — poner `caja_fuente` no les cambia la fuente, hay que usar su propia key.)

> **El tamaño es un eje aparte, no un nivel de esta tabla.** `{tam=N}` inline y `texto_tamano`/`nombre_tamano` per-caja **reescalan** la fuente que esta tabla resolvió — no compiten con ella. Por eso componen: primero gana la fuente (por precedencia), después el tamaño la reescala.

### Tamaño per-caja / Per-box text size

Una caja puede fijar el **tamaño** del cuerpo y/o del nombre, sin declarar una fuente nombrada:

```
caja_dialogo susurro {
texto_tamano 18      # cuerpo a 18 px (text_size en inglés)
}
caja_dialogo titular {
nombre_tamano 40     # nombre a 40 px (name_size en inglés)
}
```

- **Sentinela:** sin la key (o `0`) → hereda el tamaño base del juego (cuerpo 34 / nombre 26). Volver al base = quitar la key, ponerla en `0`, o `usar_caja default`.
- **Absoluto en px:** `texto_tamano 18` son 18 píxeles, no "más chico que el base". Para algo más chico que lo normal, poné un número menor que el base.
- **Compone con `caja_fuente`:** si la caja tiene `caja_fuente blackrush` Y `texto_tamano 30`, el cuerpo sale en **Blackrush a 30** — el tamaño se aplica sobre la fuente de la caja, no sobre la base.
- **Theme-level:** las dos keys también valen al tope del tema (fuera de `caja_dialogo`) → mueven el tamaño base de TODO el juego.
- **Inline `{fuente=X}`:** un tramo `{fuente=blackrush}…{/fuente}` usa el tamaño **horneado** de esa fuente (su `tamano=`), no el `texto_tamano` de la caja — salvo que agregues `{tam=N}` inline, que reescala ese tramo a N px (`{fuente=blackrush}{tam=44}…` → Blackrush a 44). El alto de la línea sube al máximo de los tamaños presentes.

> No reemplaza al `tamano=` de la declaración `fuente` (ver "Por qué el tamaño va en la fuente"): aquél hornea un atlas a ese tamaño para una fuente **nombrada y reutilizable**; `texto_tamano` reescala **per-caja** sin nombrar nada. Primero para una fuente fija reutilizable, segundo para ajustar una caja puntual.

#### Dónde cae el texto dentro de la caja, y su borde / Text padding & outline

Estas cinco keys existen desde hace tiempo y **no estaban en el manual**; se documentan al calzar un tema contra capturas de un juego real, que es donde hicieron falta.

| Clave (ES) | Alias EN | Qué hace | Default |
|---|---|---|---|
| `texto_padding_izq` | `text_padding_left` | Margen izquierdo del texto **dentro** de la caja | `28` |
| `texto_padding_der` | `text_padding_right` | Margen derecho — **es lo que fija dónde corta el wrap** | `28` |
| `texto_padding_arriba` | `text_padding_top` | Margen superior | `18` |
| `texto_outline_color` | `text_outline_color` | Color del borde de las letras del cuerpo | apagado |
| `texto_outline_grosor` | `text_outline_thickness` | Grosor del borde en px | `0` |

```
texto_padding_izq      55
texto_padding_der      55
texto_padding_arriba   42
texto_color            #FFFFFF
texto_outline_color    #4F303F
texto_outline_grosor   2
```

- **Van al tope del tema o dentro de un `caja_dialogo { }`**, como el resto de las keys de caja.
- **Los defaults no saben nada de tu arte.** Si usás `caja_sprite`, el PNG trae su propio margen dibujado, y al estirarlo ese margen escala con él — los paddings **no**. Es la causa típica de "el texto se ve pegado a la izquierda": el sprite creció y el padding se quedó.
- **El de la derecha se mide solo por el wrap.** No lo vas a "ver" como espacio: se nota en **por qué palabra corta la línea**. Si el texto llega más lejos de lo que querés, subilo.
- **El borde cuesta N+1 draw calls del texto**, donde N depende del grosor y queda acotado a 8..48. La caja se redibuja cada frame; para diálogo normal no se nota, pero si algún día ves el typewriter pesado, es el primer sospechoso.

> ⚠️ **La geometría ya no usa 8 copias cuadradas.** OUTLINE-CIRCULO calcula muestras sobre un círculo y conserva el relleno una sola vez. El feel-test cerró continuos 24 px (1–4; 5–8 ya muy pesados), 72 px (1–8), Riffic 24/4 y diálogo 41/2. Si otro caller muestra fantasmas, documentalo en una captura antes de cambiar el tema.

> Estas keys son del **cuerpo**. El nombre tiene las suyas (`nombre_outline_color` / `nombre_outline_grosor`), y el texto-en-capa usa `borde_color` / `borde_grosor` — ojo que **ésas van en español entero**; el tema escribe `outline` por deuda vieja, no porque haya un tercer idioma.

These five keys place the body text inside the box (`text_padding_left` / `_right` / `_top`) and give it an outline (`text_outline_color` / `_thickness`). Defaults are 28/28/18 and outline off. Note the padding does **not** scale with `caja_sprite`, and outline thickness has a size-dependent ceiling (2 is safe at the default 34 px body).

#### Separación entre líneas / Line spacing

| Clave (ES) | Alias EN | Qué hace | Default |
|---|---|---|---|
| `interlineado` | `line_spacing` | Píxeles **sumados a la separación entre renglones** | `0` |

```
interlineado 1.6      # un pelo más aireado
interlineado -3       # apretado
interlineado 0        # lo que diga la fuente (default)
```

**Es la pareja vertical de `espaciado_letras`.** Aquélla separa letras; ésta, renglones. Las dos son píxeles **con signo** y las dos usan `0` como "no toques nada".

- **Sólo hace algo si el texto ENVUELVE.** Es la separación *entre* renglones: con una línea sola no hay ninguna separación que mover, así que la key no se nota — ni con `10`, ni con `-10`. **Si estás probándola y no ves nada, ese es el motivo el 90% de las veces.** En la caja por default entran unos **66 caracteres** por renglón; abajo de eso, no envuelve.
- **`0` no es "apagado", es "heredar".** La fuente `.ttf` ya trae su propia separación y el motor la respeta; esta key **suma** encima. Por eso el default deja el motor exactamente como estaba.
- **Va al tope del tema o dentro de un `caja_dialogo { }`**, como el resto.
- **Dónde se aplica:** el cuerpo de la caja de diálogo **y el historial** (para que la misma frase se lea igual en los dos lados). Las opciones de `elegir`, la placa de nombre, los botones y el texto-capa **no la usan** — son de un renglón.
- **No lo confundas con el tamaño.** Subir `texto_tamano` también separa los renglones, pero además agranda las letras. Si lo que te sobra o falta es aire y no tamaño, ésta es la key.

`line_spacing` adds signed pixels to the gap **between** wrapped lines (`0` = inherit the font's own metrics). It's the vertical counterpart of `letter_spacing`. It only has a visible effect on text that actually wraps — a single-line dialogue looks identical at any value. Applies to the dialogue body and the history log.

#### Cómo APARECE la caja / How the box shows up *(v0.8.26)*

Cuando la caja de diálogo vuelve a la pantalla, por default aparece de golpe. Estas dos keys te dejan elegir otra cosa — y son **efectos distintos**, no dos modos de lo mismo:

| Clave (ES) | Alias EN | Qué hace | Default |
|---|---|---|---|
| `caja_aparicion_duracion` | `box_entry_duration` | La caja **funde**, entera y con su texto, durante N segundos | `0` |
| `caja_aparicion_texto_retraso` | `box_entry_text_delay` | La caja entra de golpe y **el texto arranca N segundos después** | `0` |

```
caja_aparicion_duracion 0.4       # la caja aparece fundiendo
caja_aparicion_texto_retraso 0.3  # entra de golpe, las letras llegan después
```

> **Se llamaban distinto hasta la v0.8.25.** `caja_entrada`/`box_fade_in` y `caja_texto_retraso`/`box_text_delay` **ya no existen**: si tenés un tema viejo, el motor te va a decir `key desconocida` nombrando la clave, y la caja va a aparecer de golpe hasta que la renombres. Cambiaron por dos razones distintas, una por idioma: **`entrada`** en español ya nombraba otras dos cosas del manual (el comando `entrada`, que es el campo donde el jugador escribe, y la curva `curva=entrada`), y una familia que va a crecer no puede colgar de una palabra ocupada; **`entry`** en inglés no es una traducción de `aparicion` sino la palabra que el motor **ya usaba** para esto mismo en los mapas (`entry_duration`). Los alias ES/EN ponen la mejor palabra de cada idioma, no la traducción literal — igual que `demora` y `retraso` son los dos `delay`.

- **Fundir es TODO junto.** La caja, el borde, la placa del nombre, el texto y la barra de botones comparten el mismo fundido — no se van encendiendo por partes.
- **`0` es apagado**, en las dos. Un tema que no las nombra se ve exactamente como antes.
- **Se disparan cuando la caja APARECE — no en cada línea.** "Aparecer" significa *el jugador no la estaba viendo*: la primera línea del guion, la que sigue a un `esperar`, a un `elegir`, a un `esperar_click` o a un `ocultar_caja`. *(Excepción desde la v0.8.29: si el `elegir` conservó la caja con `mostrar_caja` —ver 14.5.bis—, la caja nunca se fue, así que la línea de después **no** es una aparición.)* **Entre dos líneas de diálogo seguidas no pasa nada**, que es lo que hace que el efecto no canse: en un guion típico, ~1 de cada 5 líneas es una aparición.
- **Van al tope del tema o dentro de un `caja_dialogo { }`**, como el resto — y ahí está la gracia: podés tener **una caja que funde y otra que no**, y pedir cada una desde el guion con `usar_caja`, o colgársela a un personaje con `caja`.
- **Se pueden combinar** (fundir *y* atrasar el texto) o usar sueltas. **Y el número decide si el texto entra fundiendo o de golpe:** `caja_aparicion_texto_retraso` es una **compuerta**, no un fundido — cuando se cumple el tiempo, el typewriter arranca y las letras salen como salen siempre. Así que si el **retraso es igual o mayor que `caja_aparicion_duracion`**, el fundido ya terminó cuando el texto arranca y el texto aparece **a opacidad plena, de golpe** (como en muchas novelas visuales). Si el retraso es **menor**, el texto arranca a media rampa y **sube con la caja**. Con `caja_aparicion_duracion 0.4` + `caja_aparicion_texto_retraso 0.2` el texto funde; con `0.4` y `0.4`, no.

```
# Una caja normal y otra "dramática", elegibles desde el .ecl
caja_aparicion_duracion 0            # el default del juego: de golpe

caja_dialogo suave {
    caja_aparicion_duracion 0.6      # ésta funde
}
```

> **Ojo con las unidades:** estas dos van en **segundos** (`0.4`). `caja_opacidad`, que está al lado y también es "de la caja", va en **porcentaje** (`92%`). No comparten unidad aunque compartan prefijo.

`box_entry_duration` fades the whole box in (background, border, nameplate, body text and the button bar, all together) over N seconds. `box_entry_text_delay` instead keeps the instant pop-in and delays only the typewriter's first letter — as many visual novels do. Both default to `0` (off) and both fire **only when the box appears**, i.e. when the player wasn't already looking at it: never between two consecutive dialogue lines. They live at the theme's top level or inside a `caja_dialogo { }` block, so a script can switch between entrances with `usar_caja`. *(Renamed in v0.8.26 — `box_fade_in` and `box_text_delay` are gone and now warn. `entry` is the same noun the map format already uses for this, as in `entry_duration`.)*


`nombre_fuente` es para el nameplate lo que `{fuente=X}` inline es para el cuerpo: el override más específico. Si una caja declara solo `caja_fuente`, el nameplate la hereda (cuerpo y nombre en la misma fuente). Si además declara `nombre_fuente`, el nombre diverge y el cuerpo se queda con `caja_fuente`.

Two tracks share the baseline — body: inline `{fuente=X}` > `caja_fuente` > baseline; nameplate: `nombre_fuente` > `caja_fuente` > baseline.

#### Espaciado de letras del tema / Theme letter spacing

Al tope del `.eclips`, junto a `fuente "ruta.ttf"`, podés poner `espaciado_letras N` (o `letter_spacing N`): el espaciado de la **fuente baseline**, en píxeles (negativo aprieta). Como el kerning va horneado en cada fuente (ver §9.6), **sigue al carril de la fuente**:

- El `espaciado_letras` del **tema** (baseline) afecta **cuerpo + nameplate base + chrome** (botones AUTO/SKIP, choices, historial) — todos usan la fuente base. Es la tipografía global del juego.
- El `espaciado_letras=N` de una **fuente nombrada** afecta **solo** donde esa fuente se usa (`{fuente=X}`, `caja_fuente`, `nombre_fuente`). El chrome no lo hereda.

```
fuente "third_party/fonts/nunito/Nunito-Regular.ttf"
espaciado_letras -3        # cuerpo + chrome, apretado
```

Esto es distinto de `caja_fuente`/`nombre_fuente` (que el chrome **nunca** hereda): el baseline es tipografía global; las nombradas son por-viñeta.

#### Fallback / Fallback behavior

Si `caja_fuente X` referencia una fuente que **no fue declarada** en el `.ecl` (o se escribió mal), el motor cae a la fuente base del juego y emite un aviso una sola vez por nombre faltante:


`[VNDialogBox] Aviso: fuente 'X' no encontrada, usando fuente base`

No es un error fatal — la escena sigue funcionando con la fuente base. Solo significa que la creadora todavía no declaró esa fuente nombrada.

If `caja_fuente X` references a font that was not declared in the `.ecl` (or is misspelled), the engine falls back to the game's base font and emits a one-time warning per missing name. Not fatal — the scene keeps running.

#### Notas / Notes

- **Tamaño** lo define la declaración de la fuente nombrada (`fuente determination "..." tamano=34`), no la plantilla de caja. Si querés Determination a 24pt en una caja y a 34pt en otra, declará dos fuentes nombradas (`determination_chica` y `determination`).
- **Choices, QTE e historial** mantienen la fuente base del juego en v1 — son chrome de UI/sistema, no parte de la viñeta. Si tu juego pide otra cosa, abrime un issue.
- **Métricas de baseline** varían entre fuentes — dos fuentes "a 34pt" pueden verse de distinto tamaño aparente. Es realidad tipográfica, no bug del motor. Ajustar con `tamano=` distinto o (futuro) offsets per-fuente.
- **Font size** is set by the named font declaration, not by the box template. Choices, QTE, and history keep the game base font in v1 (UI chrome). Baseline metrics vary between fonts — adjust with `tamano=` if needed.

### Fuente solo del nombre / Nameplate-only font (`nombre_fuente`, v0.4.9)

`nombre_fuente` (o `name_font`) le da al **nameplate** una fuente distinta del cuerpo. Mismas reglas que `caja_fuente` (identificador de una fuente declarada con `fuente N "..." tamano=N`, tamaño horneado en esa declaración). Alcance: **solo la placa de nombre**. El cuerpo, los botones AUTO/SKIP/SAVE y el nombre del hablante en el **historial** mantienen su fuente (caja_fuente o base) — la viñeta y el log son superficies distintas.

```
# en tu .eclips de tema
caja_dialogo dorm {
caja_fuente   nunito_box     # cuerpo en Nunito
nombre_fuente mplus          # nombre en M PLUS Rounded Bold
}
```

.ecl — declarar ambas fuentes antes del primer uso:

```
fuente nunito_box "third_party/fonts/nunito/Nunito-Regular.ttf" tamano=34
fuente mplus      "third_party/fonts/m_plus_rounded_1c/MPLUSRounded1c-Bold.ttf" tamano=28
```

Precedencia para la placa: `nombre_fuente` > `caja_fuente` > base. Si la fuente no se declaró, la placa cae al fallback (caja_fuente o base) con un aviso 1×:
`[VNDialogBox] Aviso: fuente de nombre 'X' no encontrada, usando caja_fuente/base`

`nombre_fuente` (or `name_font`) gives the **nameplate** a font distinct from the body. Same rules as `caja_fuente`; scope is **the name plate only** — body, UI buttons, and the speaker name in the history log keep their font.

### Fuente de opciones y QTE / Choices & QTE font (`opciones_fuente`, `qte_fuente`, v0.4.12)

`opciones_fuente` (o `choices_font`) le da a los **botones de `elegir`** una fuente propia; `qte_fuente` (o `qte_font`) hace lo mismo con la **tecla y el hint del QTE**. Mismas reglas que `caja_fuente` (identificador de una fuente declarada con `fuente N "..." tamano=N`, tamaño horneado ahí).

Precedencia: **`opciones_fuente` > base** y **`qte_fuente` > base** — directo, sin pasar por `caja_fuente`. Es a propósito: los choices y el QTE nunca heredaron `caja_fuente`, así que ponerlo en una caja **no** les cambia la fuente; para eso está su key propia. Es la diferencia con `nombre_fuente`, que sí cae por `caja_fuente`.

```
caja_dialogo combate {
    caja_fuente     determination   # cuerpo en Determination
    opciones_fuente riffic          # opciones en Riffic (independiente del cuerpo)
    qte_fuente      riffic          # QTE también en Riffic
}
```

Alcance: solo los choices (`opciones_fuente`) y la tecla+hint del QTE (`qte_fuente`). Los botones del bottom-bar (AUTO/SKIP/SAVE) y el historial siguen en base (chrome). Si la fuente no se declaró, cae a la base con aviso 1× por nombre:
`[VNDialogBox] Aviso: fuente de opciones 'X' no encontrada, usando fuente base`

`opciones_fuente` / `choices_font` styles the `elegir` buttons; `qte_fuente` / `qte_font` styles the QTE key and hint. Precedence is **> base directly** (not through `caja_fuente`) — choices/QTE never followed `caja_fuente`, so setting it won't change them; use their own key. Scope is choices and QTE only; bottom-bar buttons and history stay base.

### `indicador_avance_imagen` / `advance_indicator_image`  (v0.4.10)

PNG para el indicador de avance, en vez del glifo. Va en el tema (default) o
dentro de un `caja_dialogo X {}` (per-caja). Si el PNG carga, reemplaza al glifo;
si falta o falla, fallback automático al glifo (`indicador_avance_glifo`) + 1 aviso.

    indicador_avance_imagen "assets\ui\ctc.png"

- Tamaño nativo del PNG × `indicador_avance_escala` (default 1.0). Asset 720-nativo a 1080 → escala 1.5.
- Tint blanco: el PNG sale tal cual; `indicador_avance_color` aplica solo al glifo.
- El parpadeo (`indicador_avance_parpadeo` + `_velocidad`) le modula el alpha igual.
- Posición: reusa `indicador_avance_offset_x` / `indicador_avance_offset_abajo`
  (ambos, glifo e imagen, anclan por su esquina sup-izq → el offset los mueve igual).

---

### `indicador_avance_vaiven` / `advance_indicator_nudge`  (v0.4.11)

Vaivén suave del indicador — el "empujoncito" clásico del ctc (click-to-continue). Mueve el indicador
en un eje (X default) con curva easeIn→easeOut en loop. Tema o `caja_dialogo X {}`
(per-caja). Default off → sin cambio.

    indicador_avance_vaiven si
    indicador_avance_vaiven_amplitud 5     # px de recorrido (default 5)
    indicador_avance_vaiven_periodo 1.5    # seg por ciclo (default 1.5)
    indicador_avance_vaiven_vertical no    # si → eje Y (arriba/abajo) en vez de X
    indicador_avance_vaiven_brillo si      # alpha derivado de la posición (ver abajo)

- **Offset puro:** mueve posición, no toca alpha. Compone con `indicador_avance_parpadeo`.
- **`_brillo` (soldado):** el alpha NACE de la posición — brillante en reposo (recorrido
  0), tenue en el extremo (rango 0.5–1.0). Un solo oscilador: posición y brillo
  no pueden desincronizarse. **Manda sobre el parpadeo** (si `_brillo si`, el blink suelto
  se ignora). Requiere `_vaiven si`.
- **Sin `_brillo`:** `_vaiven` + `_parpadeo` a periodos distintos **baten** (entran/salen
  de fase como un reloj). No es bug, es física de dos relojes libres. Para el ctc exacto,
  usá `_brillo` (suelda los canales).

**Receta de ctc con PNG** (a 1080 con asset 720-nativo):

    indicador_avance_imagen "assets\ui\ctc.png"
    indicador_avance_escala 1.5            # 720→1080
    indicador_avance_vaiven si
    indicador_avance_vaiven_amplitud 8     # 5 × 1.5, escala con la imagen
    indicador_avance_vaiven_periodo 1.5
    indicador_avance_vaiven_brillo si
    indicador_avance_parpadeo no
    indicador_avance_offset_x 1186         # ajustá mirando tu propia caja
    indicador_avance_offset_abajo 40

---

## 14.7 Sprites PNG en la caja de diálogo / PNG Sprites in Dialog Box

Por defecto la caja de diálogo se renderiza con primitivas (rounded rect + borde de color sólido). Para más control visual, podés declarar un **PNG como sprite** y la caja lo usa como fondo, manteniendo el texto, nameplate, indicadores y botones encima.

By default, the dialog box renders with primitives (rounded rect + solid color border). For more visual control, you can declare a **PNG sprite** as background, and the box uses it; text, nameplate, indicators and buttons stay on top.

### Sintaxis básica / Basic syntax

En el archivo `tema_*.eclips`:

```
caja_sprite        "assets/ui/textbox.png"
nombre_sprite      "assets/ui/namebox.png"

box_sprite         "assets/ui/textbox.png"
nameplate_sprite   "assets/ui/namebox.png"
```

`caja_sprite` aplica al fondo de la caja principal. `nombre_sprite` al fondo del nameplate (la solapita con el nombre del hablante).

Cualquiera de los dos es opcional. Si solo declarás `caja_sprite`, el nameplate sigue siendo primitivo. Si el PNG no se puede cargar, hay un aviso en consola y el motor cae a primitivas — no crashea.

### Geometría: ancho y alto del rect destino

El PNG se renderiza dentro de un rect calculado a partir del theme:

```
caja_ancho_ratio     0.6375    # fracción del ancho de pantalla (0.0 a 1.0)
caja_altura          219       # alto en pixels virtuales
caja_margen_inferior 10        # distancia desde el fondo de pantalla

box_width_ratio      0.6375
box_height           219
box_bottom_margin    10
```

### El problema del estirado / The stretching problem

El motor estira el PNG para que llene el rect. Si el aspect ratio del rect destino difiere del aspect ratio del PNG fuente, las **esquinas redondeadas se deforman** y el contenido del centro queda visualmente inconsistente.

The engine stretches the PNG to fill the rect. If destination aspect ratio differs from source, **rounded corners deform**.

**Dos soluciones:**

1. **Calibrar la geometría destino al aspect ratio del PNG** (escalado uniforme — ver mini-guía abajo).
2. **9-slice scaling** — declarar qué pixels son esquinas (no se estiran) y qué pixels son centro (estira libre). Útil cuando el aspect ratio destino debe variar (cajas con ancho cambiante, distintas resoluciones, ventana redimensionable a futuro).

### 9-slice scaling

Dividir el sprite en 9 regiones: 4 esquinas (tamaño nativo), 4 bordes (escalan en una dimensión), 1 centro (estira libre).

```
caja_sprite_borde_izq      30     # pixels desde el borde izquierdo del PNG
caja_sprite_borde_der      30
caja_sprite_borde_arriba   10
caja_sprite_borde_abajo    10

box_sprite_border_left     30
box_sprite_border_right    30
box_sprite_border_top      10
box_sprite_border_bottom   10
```

Análogo para el nameplate:

```
nombre_sprite_borde_izq      10
nombre_sprite_borde_der      10
nombre_sprite_borde_arriba   5
nombre_sprite_borde_abajo    5
```

**Default 0** en los 4 bordes = stretch puro (sin 9-slice). El feature solo se activa si al menos uno de los 4 bordes es mayor que 0.

### Cuándo 9-slice aporta y cuándo no

**Aporta** cuando el rect destino tiene aspect ratio distinto al PNG source. Ejemplos:
- Cajas con `caja_ancho_ratio` variable.
- Mismo PNG en distintas resoluciones de juego.
- Diseños donde el creador quiere flexibilidad de tamaño.

**No aporta** si el aspect ratio destino y source son iguales (escalado uniforme). En ese caso, las dimensiones escalan parejo y el centro no se deforma con o sin 9-slice. La única diferencia es que las esquinas quedan a tamaño nativo (crispier) en vez de escalado.

### Validación de errores

Si declarás bordes que exceden el tamaño del PNG (ej. `caja_sprite_borde_izq 700 caja_sprite_borde_der 700` en un sprite de 1000px ancho — los 1400px de borde no caben), el motor avisa en consola **una sola vez al cargar el theme** y cae a stretch puro para esa caja:

```
[Theme] Aviso: bordes 9-slice de caja 'default' (izq+der=1400, arriba+abajo=0) exceden el sprite (1000x200). 9-slice deshabilitado; fallback a stretch.
```

### Mini-guía de rescalado / Rescaling mini-guide

Si tenés un PNG diseñado para una resolución X y querés usarlo en Eclips (que renderiza en 1920×1080 virtual), la forma simple de hacerlo "sin estirado" es escalar uniforme y calibrar la geometría destino.

**Fórmula general:**

```
factor       = 1080 / altura_original_resolución
ancho_dest   = ancho_PNG × factor
alto_dest    = alto_PNG × factor

caja_ancho_ratio   = ancho_dest / 1920
caja_altura        = alto_dest
```

**Resoluciones comunes:**

| Resolución origen | Factor a 1080p | Factor a 1440p |
|------------------:|---------------:|---------------:|
| 1280 × 720        | × 1.5          | × 2.0          |
| 1366 × 768        | × ~1.40        | × ~1.875       |
| 1600 × 900        | × 1.2          | × 1.6          |
| 1920 × 1080       | × 1.0          | × ~1.333       |

**Ejemplo trabajado: portar VN clásica 720p a 1080p**

PNG original: `textbox.png` 816 × 146 px (diseñado para lienzo 1280 × 720).

1. Factor de escala: `1080 / 720 = 1.5`.
2. Dimensiones destino: `816 × 1.5 = 1224`, `146 × 1.5 = 219`.
3. `caja_ancho_ratio = 1224 / 1920 = 0.6375`.
4. `caja_altura = 219`.

Resultado en `tema_X.eclips`:
```
caja_sprite          "assets/ui/textbox.png"
caja_ancho_ratio     0.6375
caja_altura          219
caja_margen_inferior 10
```

Las esquinas y el centro escalan parejo 1.5x, sin deformación. 9-slice opcional acá — no cambia el resultado visible. Si en el futuro querés que el creador del juego pueda ajustar `caja_ancho_ratio` libremente, agregale 9-slice y las esquinas se mantienen pase lo que pase.

### Namebox: tamaño fijo, posición y z-order

El nameplate (la solapita con el nombre del hablante) tiene su propia geometría, independiente de la caja. Solo aparece cuando habla un personaje — con narrador (sin nombre) no se dibuja. Cuatro campos lo controlan:

```
nombre_altura    59     # alto del rect destino en px virtuales
nombre_anchura   0      # ancho fijo del rect. 0 = auto-ajusta al ancho del nombre
nombre_offset_x  35     # desliza horizontal desde el ancla de la caja
nombre_offset_y  0      # desliza vertical (+ baja la placa, − la sube)
nombre_detras    no     # z-order: no = placa delante de la caja; si = detrás

name_height      59
name_width       0
name_offset_x    35
name_offset_y    0
name_behind      no
```

**`nombre_anchura` (ancho fijo vs auto-ajuste).** Default `0` = la placa se ajusta sola al ancho del nombre (nombre corto = placa angosta, nombre largo = placa ancha). Útil para placas de color simples. Con un valor `> 0` la placa tiene **ancho fijo** y el nombre se centra dentro — es el modo correcto para un namebox de arte (PNG): el sprite mantiene su forma sin estirarse según el largo del nombre. Si el nombre es más largo que el ancho fijo, se desborda centrado a propósito (no auto-encoge). La solución para nombres largos es alargar la caja de ese personaje, no encoger el texto.

**`nombre_offset_x` / `nombre_offset_y` (posición).** Se suman al ancla de la placa (esquina superior de la caja). Con ambos en `0`, la placa queda en la posición clásica: arriba a la izquierda de la caja. Subir `offset_x` la desliza a la derecha; `offset_y` positivo la baja, negativo la sube. Permiten poner el namebox donde el creador quiera — lo clásico es arriba a la izquierda, pero podría ir al centro, a la derecha, etc.

**`nombre_detras` (z-order).** Default `no` = la placa se dibuja **delante** de la caja. Con `si` se dibuja **detrás**, de modo que la parte que sobresale (típicamente una solapa que asoma por arriba) queda visible y el resto queda tapado por la caja. Es el estilo de solapa, donde el namebox es una solapita que nace detrás de la caja de texto. El texto del nombre siempre se dibuja encima de la placa, vaya esta delante o detrás. Algunos themes querrán la placa delante, otros detrás — es decisión del creador, no un valor fijo del motor.

**Ejemplo: namebox de solapa.** PNG `namebox.png` de 168×39 nativo (diseñado para 720), portado a 1080 (factor ~1.5):

```
nombre_sprite    "assets/ui/namebox.png"
nombre_altura    59       # 39 × 1.5 ≈ 59
nombre_anchura   0        # o un ancho fijo si el arte lo pide
nombre_offset_x  35       # ligeramente a la derecha
nombre_offset_y  0
nombre_detras    si       # la solapa asoma por arriba de la caja
```

El tamaño y color del texto del nombre (para cuadrar con el arte del namebox) se ajustan con los campos de color/outline del nombre — eso es decisión del creador, no parte de esta geometría.

### Pintura, no features

El PNG puede tener gradientes, patrones, texturas, sombras suaves, o cualquier cosa que pinte un editor de imágenes. El motor no impone restricciones — solo lo dibuja como fondo. Esto es deliberado: el arte viene del creador, no del motor.

The PNG can have gradients, patterns, textures, soft shadows, or anything an image editor can produce. The engine doesn't restrict — it just draws it as background. This is deliberate: art comes from the creator, not from the engine.

### Centrar el nombre en una namebox alta / PNG

``` nombre_texto_offset_y ```

Posiciona el texto del nombre DENTRO de la placa (baja desde el tope; default 5). Ese 5 asume placa baja (~36px); con nombre_sprite PNG o nombre_altura grande, el nombre queda pegado arriba.
Para centrarlo (punto de partida, luego ajustá a ojo):

```nombre_texto_offset_y ≈ (nombre_altura − alto_de_la_letra) / 2 ```

Ej.: placa 59px, letra ~30px → ~15. En X se centra solo. Todos losoffsets de nombre/caja son per-caja, combinables, y responden idéntico con placa PNG o primitiva.

---

## 14.8 Resolución de diseño / Design Resolution

Tu juego se dibuja en un **lienzo lógico** y el motor lo escala a la ventana del jugador (con barras si el aspecto no calza). Por defecto ese lienzo es **1920×1080**. Si tus assets están hechos para otra resolución, declarás la tuya:


en proyecto.eclips:
`resolucion_diseno 1280 720      # o design_resolution 1280 720`

A partir de ahí, **todas tus coordenadas viven en ese lienzo**: `capa ... x=100`, `mostrar m centro`, `texto ... x=640` se interpretan en 1280×720. El motor escala el frame entero; vos pensás "¿se ve bien?", no "esto es 720".

**¿Cuándo cambiarla?** Cuando tus fondos/sprites/caja están dibujados para una resolución concreta (p. ej. assets nativos de 720). Diseñás en esa resolución y todo calza 1:1, sin multiplicar a mano.

**Notas:**
- Si no la declarás, es 1920×1080 — todo sigue igual que siempre.
- La resolución de diseño y el **tamaño de la ventana** son cosas distintas: el lienzo es tu espacio de coordenadas; la ventana es lo que el jugador tenga. El frame se ajusta solo.
- Espera **dos enteros positivos**. Si escribís uno solo o texto, el motor avisa y mantiene el default.
- Cambia en vivo con **F8** (hot-reload): editás la línea, guardás, y el frame re-escala sin reiniciar.

---

## 14.8 Cinemáticas / Cinematics

Una **cinemática** es un bloque que **sella el input** mientras dura, para que una secuencia se reproduzca **completa** — sin que el jugador la salte con skip ni la interrumpa guardando a media.

    cinematica saltable=no {
    capa estrellas "assets/stars.png" z=0 x=0 y=0 w=1920 h=2160
    capa_mover estrellas a (0, -1080) en 4 loop

    texto linea1 "En aquel entonces..." x=400 y=300 z=500
    esperar 4
    capa_fade linea1 desaparecer 1.0

    esperar 2
    }

**Forma:** `cinematica [saltable=si|no] {` … comandos … `}`
- El header va **en la línea de apertura** (junto a la `{`), no adentro.
- El cuerpo son **comandos normales** (`texto`, `capa`, `capa_mover`, `esperar`, `capa_fade`…) que corren en secuencia. El cierre es un `}` solo en su renglón.

### `saltable` — la única perilla

| Valor | Qué pasa | Para qué |
|-------|----------|----------|
| `saltable no` (default) | el **skip se sella**: aunque el jugador traiga skip/Ctrl activo desde antes, la secuencia corre por reloj. El **save/load** también queda sellado. | intros, finales, la canción de un personaje — lo que NO querés que se salte |
| `saltable si` / `skippable yes` | el bloque **no sella nada**: el skip funciona normal | intros largas ya vistas, donde el jugador sí puede adelantar |

**Por qué el default es `no`:** el sentido de una cinemática es que se reproduzca entera. Si el default dejara pasar el skip que el jugador traía activo, perderías el final sin querer. Por eso el sello ignora el skip venga de antes o de en medio; `saltable si` es el escape explícito.

### El ritmo lo lleva `esperar`, no el click

Dentro de la cinemática el tiempo avanza con `esperar`, no con clicks. Un click **no salta** un `esperar`. Por eso una cinemática se arma con `texto` + `esperar` + `capa_mover`/`capa_fade`, **no** con líneas de diálogo (`personaje: "..."`), que sí esperan click.

### Qué sella y qué no (hoy)

- **Sella:** skip (Ctrl/toggle) y los atajos de guardado/carga (`k`, `l`, `F5`, `F9`).
- **No sella todavía:** `ESC` (abre el menú de pausa). El sello completo del teclado en cinemáticas llega con el sistema de remapeo de teclas; hasta entonces, evitá guardar desde el menú de pausa a media cinemática.
- Gemelo inglés: `cinematic`, y `skippable` para el campo.

---

## 14.9 La consola / The console

Una **consola** es una caja de terminal: un rect estilado con un buffer de texto que **se acumula** (no se reemplaza), se **teclea** en vivo, se **retracta**, y **recorta** lo que rebasa su alto. Es la pieza para los momentos "alguien escribe en una terminal" (un juego que se corrompe, el boot de un SO, romper la cuarta pared).

No es un `texto`-capa (que reemplaza su string) ni la caja de diálogo (que reemplaza al avanzar): es **primitiva propia**. Pero vive en la **familia de capas** — tiene `id`, posición y tamaño — así que `capa_mover`, `capa_fade`, `capa_visible` y `capa_z` le funcionan por su id, **gratis**.

    consola crear terminal x=300 y=180 w=920 h=440 color_fondo=#101820D0 color_texto=#5FF59F tamano=34 cursor=si

    consola_escribir terminal ">>> iniciando sistema...\n"   # se teclea
    esperar 2
    consola_imprimir terminal "[OK] listo en 0.3s\n"          # de golpe
    esperar 1
    consola_borrar terminal 12                                 # retract de 12 chars
    esperar 1
    consola_limpiar terminal                                   # vacía el buffer

### Crear / `consola crear <id> ...`

| Param | Qué hace | Default |
|-------|----------|---------|
| `x= y= w= h=` | posición y tamaño del rect (coords virtuales) | 0 |
| `color_fondo=#RRGGBB[AA]` | fondo del rect. El **AA** opcional = opacidad (el resaltante semitransparente) | negro 60% |
| `color_texto=#RRGGBB` | color del texto | blanco |
| `tamano=` / `fuente=` | tamaño en px y/o fuente nombrada (como en `texto`) | renderer default |
| `cursor=si` | muestra un `_` parpadeante tras el último char (periodo 1s) | no |

El `id` va **sin comillas** (es símbolo del guión, igual que `capa`/`capa_mover`). Re-crear con el mismo id **re-estiliza** sin borrar lo ya escrito.

**Sobre los dos colores:**

- El `AA` de `color_fondo` es **opcional de verdad**: si escribís `color_fondo=#101820` (seis dígitos), la opacidad se queda en su default del 60% — no sube a 100% por la espalda.
- **`color_texto` no lleva alpha.** Si le pasás ocho dígitos, el motor te **avisa con el número de línea y conserva el blanco**, en vez de ignorar el `AA` en silencio.
- Un color mal escrito (`#10182`, `#0000FZ`, `rojo`) **se descarta con aviso y deja el default**. El motor no inventa un color a partir de lo que sí pudo leer.

### Los cuatro verbos — la sintaxis no miente

| Comando | Qué hace |
|---------|----------|
| `consola_escribir <id> "..."` | appendea + **typewriter** ("como si lo escribieras"). |
| `consola_imprimir <id> "..."` | appendea **de golpe** (instant real — output del sistema). |
| `consola_borrar <id> [N\|todo]` | **retract**: el typewriter corre en reversa y borra N glifos (o `todo`) del final. Sin N = 1. |
| `consola_limpiar <id>` | vacía el buffer de inmediato (sin animación). |

Escribir teclea, imprimir imprime, borrar borra, limpiar limpia. Los prefijos `>>>` y la indentación son **tu contenido** (no se hornean → no se fuerza la estética de ningún juego).

### Saltos de línea, clip y composición

- **`\n`** dentro del string de `escribir`/`imprimir` = salto de línea real (la consola es multi-línea por diseño). El resto del motor no procesa escapes; esto es exclusivo de la consola.
- **Clip duro:** lo que rebasa el alto `h` del rect **no se ve** — la consola no crece ni scrollea. Si escribís de más, lo limpiás a mano (`consola_limpiar`).
- **Composición (gratis):** como es una capa, `capa_mover terminal a (x,y) en N suave` la **desliza**, `capa_fade terminal desaparecer 1` la **desvanece**. Para que **tiemble** o **se agrande** en tiempo real hace falta el puente `animar`→capa (fichado, próxima pieza de cinemática).

> ⚠️ El `id` va **sin comillas** en todos los verbos: `consola_escribir terminal "..."`, no `consola_escribir "terminal" "..."`.

---

## 14.10. `control` — widget de setting (Capa 2)

Coloca un control interactivo atado a un setting del motor. El motor cablea el
binding (leer/escribir en vivo) y honra el lock del creador; tú das layout.

    control musica_volumen    x=140 y=440 ancho=400
    control velocidad_texto   x=140 y=540 ancho=400 min=0 max=3 paso=1
    control pantalla_completa x=140 y=620
    control skip_no_visto     x=140 y=680

- `control <setting>` → id Tier-A. Hoy:
  - Numéricos: `musica_volumen`/`music_volume`, `sonido_volumen`/`sound_volume`.
  - Velocidad: `velocidad_texto`/`text_speed`. Sin dominio en el theme usa el
    catálogo legacy; con dominio numérico usa el rango, paso y endpoint del theme.
    El widget no declara un dominio nuevo ni cuantiza el override guardado.
  - Booleanos: `pantalla_completa`/`fullscreen`, `skip_no_visto`/`skip_unseen`.
  - Catálogo: `resolucion`/`resolution`, `idioma`/`language` (los dos abajo).
- `x` `y` posición · `ancho`/`width` largo (barra ~400, pill ~56 por default).
- `min` `max` `paso`/`step` modelo de valor (defaults por setting; omitir = default).
  En `velocidad_texto`, `min`/`max`/`paso` explícitos solo son compatibles si
  coinciden con el dominio numérico del theme; una contradicción emite aviso y
  conserva el dominio del theme. Si el theme es legacy, el control conserva el
  catálogo legacy.
- `tipo=`/`type=` **cómo se pinta**: `barra`/`bar` (slider) o `toggle`/`interruptor`
  (pill on/off). **Se elige solo según el setting**: un booleano sale toggle, lo
  demás barra. En `velocidad_texto`, pedir `tipo=toggle` es incompatible: emite
  aviso y conserva el render de barra del catálogo.
- **El hit-test de una barra cubre toda la perilla visible, tenga `estilo=` o no.**
  Desde `v0.8.54`, la mitad exterior del pomo —la que sobresale del riel en los
  extremos— captura el clic. Es un cambio de conducta deliberado, y **alcanza a
  toda barra, esté vestida o no**: repara un desacuerdo render↔hit que ya vivía.
  El valor, los píxeles, la ruta de escritura y el eje `x..x+ancho` no se
  movieron; sólo dejó de haber zona muerta.
- `estilo=`/`style=` elige un bloque `estilo_control`/`control_style` declarado en
  `project.eclips`; no es texto ni un dominio nuevo. La superficie v0.8.54 es una
  barra horizontal (`barra`/`bar`) y conserva el valor, el dominio y la
  persistencia del setting.

  El bloque puede escribir `riel`/`track`, `relleno`/`fill` y
  `perilla`/`thumb`. Cada pieza admite `#RRGGBB`, una ruta de imagen o
  `ninguno`/`none`. **`ninguno`/`none` es vocabulario del motor y NO distingue
  mayúsculas** (`Ninguno`, `NONE` y `None` valen lo mismo); una ruta, en cambio,
  se toma literal, porque el nombre de un archivo sí puede depender del caso.
  **Y todo valor que empieza con `#` se juzga como color**: si no es `#RRGGBB`
  exacto emite la voz de color e ignora la pieza — nunca se intenta como ruta.
  Los sufijos de estado aceptan las dos grafías:
  `_al_pasar`/`_hover`, `_al_enfocar`/`_focus`, `_arrastrando`/`_dragging` y
  `_bloqueado`/`_locked`. Un campo omitido no declara una variante; `none` la
  declara como ausencia intencional; un color o una ruta inválidos conservan
  el fallback y emiten una voz de fuente, línea, alias, propiedad,
  consecuencia y reparación.

  La precedencia viva es `locked > dragging > hover/focus > base`. El foco usa
  una variante válida y visualmente distinta; si falta, es idéntica o está
  rota, permanece el indicador de foco fallback. **Si no escribís geometría**, el
  riel base fija la altura, el relleno comparte su caja y recorta UV
  horizontalmente y la perilla conserva su tamaño nativo; con las claves de
  geometría de abajo, **el número que escribís gana sobre el tamaño del PNG**. El
  hit-test incluye la superficie visible de la perilla, y la geometría resuelta se
  comparte entre render, hit-test, hover del puntero, arrastre y mapeo del valor.
  La geometría no selecciona por sí sola el foco.

  **La geometría, sin un solo PNG** (desde `v0.8.57`). El bloque acepta además
  cuatro claves que cambian el **tamaño** en vez del color, y ninguna necesita una
  imagen:

  | clave | alias | valor | default |
  |---|---|---|---|
  | `riel_altura` | `track_height` | píxeles | `8` |
  | `perilla_ancho` | `thumb_width` | píxeles | `14` |
  | `perilla_altura` | `thumb_height` | píxeles | `22` |
  | `perilla_en_extremos` | `thumb_at_ends` | `centrada`/`centered` · `dentro`/`inside` | `centrada` |

  **Los defaults son la conducta de siempre**, así que un bloque que no escribe
  ninguna de las cuatro se dibuja exactamente como antes. Se pueden escribir
  sueltas: un bloque de **pura geometría**, sin `riel` ni `perilla`, conserva los
  colores de fábrica y sólo cambia el tamaño.

  **`perilla_en_extremos` es una política, no una medida.** `centrada` deja que el
  centro de la perilla recorra `x..x+ancho`, así que en los topes sobresale medio
  ancho — es lo que el motor hizo siempre. `dentro` mete el recorrido medio ancho
  hacia adentro, de modo que la perilla **no se sale nunca del riel**, que es lo que
  hace Ren'Py. No es el default porque **mueve el eje de valor**: cambia qué valor
  produce cada píxel, y eso alcanzaría a todo proyecto que ya existe. Lo que **no**
  cambia es la superficie clickeable: la barra entera sigue capturando.

  **La geometría no tiene estados.** No existe `riel_altura_al_pasar` ni ninguna
  otra combinación con sufijo: el tamaño de una pieza es uno solo para todos sus
  estados, y el motor lo dice con esas palabras si lo escribís. *(Con un PNG sí
  podés dar variantes de estado de tamaños distintos — ahí el tamaño sale de cada
  archivo.)* Un valor que no sea un número mayor que cero conserva el default y
  emite aviso con su fuente, su línea, su alias y su reparación.

  Ejemplo: el aspecto de Ren'Py, medido de una captura real y reproducido sin un
  solo archivo de imagen.

  ```eclips
  # en project.eclips
  estilo_control renpy_color {
      riel    #003D51
      riel_altura 43
      relleno ninguno
      perilla #0099CC
      perilla_ancho 22
      perilla_altura 43
      perilla_en_extremos dentro
  }
  ```

  ```ecl
  # en tu .ecl
  control musica_volumen x=140 y=260 ancho=494 estilo=renpy_color
  ```

  **El contorno, sin un solo PNG** (desde `v0.8.58`). Las dos referencias que
  pidieron geometría llevan además **borde**, y cada una en una pieza distinta: el
  **riel** de una tiene un contorno fino oscuro, y la **perilla** de la otra
  tiene el suyo. El bloque acepta cuatro claves más:

  | clave | alias | valor | default |
  |---|---|---|---|
  | `riel_borde_color` | `track_border_color` | `#RRGGBB` | *(sin borde)* |
  | `riel_borde_grosor` | `track_border_thickness` | píxeles | `1` |
  | `perilla_borde_color` | `thumb_border_color` | `#RRGGBB` | *(sin borde)* |
  | `perilla_borde_grosor` | `thumb_border_thickness` | píxeles | `1` |

  **El color es el interruptor.** Sin un `borde_color` escrito no se dibuja borde
  alguno, así que un bloque que no nombra ninguna de las cuatro queda exactamente
  como antes. Un `borde_grosor` **sin** su color no dibuja nada: el motor lo avisa
  al cerrar el bloque, en vez de dejarte creyendo que escribiste algo que anda.

  **El borde se pinta hacia adentro de la pieza**, así que no la agranda. El riel
  mide lo mismo con borde y sin borde, y ni la zona clickeable ni el arrastre
  cambian por declararlo.

  **Dónde NO aparece, y es a propósito.** Si la pieza resolvió a una **imagen**,
  sus píxeles ya traen su propio contorno y el motor no le dibuja un rectángulo
  limpio encima. Si la pieza está en `ninguno`, no hay caja que vestir y no
  aparece un rectángulo fantasma. En cambio, si **no declaraste la pieza** —o sea
  que se dibuja el riel de fábrica—, el borde **sí** la viste.

  **El borde no tiene estados**: no existe `riel_borde_color_al_pasar` ni ninguna
  otra combinación con sufijo, y el motor lo dice con esas palabras si lo
  escribís. *Comparte la forma con la geometría, pero no la razón, y la diferencia
  importa por si algún día cambia:* el tamaño de una pieza **no puede** variar por
  estado —es uno solo—, mientras que el color de un borde **sí podría**. Hoy no lo
  hace porque ninguna referencia medida lo pidió.

  Ejemplo: un riel fino, medido de una captura real. Su borde es de **1 px**
  y por eso ése es el default.

  ```eclips
  # en project.eclips
  estilo_control riel_fino {
      riel    #C6C6C6
      riel_altura 12
      riel_borde_color #1C1C1C
      riel_borde_grosor 1
      relleno ninguno
      perilla #1C1C1C
      perilla_ancho 9
      perilla_altura 12
      perilla_en_extremos dentro
  }
  ```

  **La silueta de la perilla** (desde `v0.8.59`). Hasta acá toda pieza es un
  rectángulo. La perilla puede además ser un **rombo**:

  | clave | alias | valor | default |
  |---|---|---|---|
  | `perilla_forma` | `thumb_shape` | `rectangulo` \| `rombo` | `rectangulo` |

  Los valores llevan sus dos grafías —`rombo`/`diamond` y
  `rectangulo`/`rectangle`, este último también con tilde— y no distinguen
  mayúsculas.

  **El rombo se dibuja adentro de la caja que la perilla ya tiene**, o sea
  `perilla_ancho` × `perilla_altura`. No hay tamaño nuevo que declarar: el rombo
  del riel fino de arriba mide 9×12 y esos dos números ya se escribían desde `v0.8.57`.

  **Se agarra por la caja, no por el rombo.** Un pomo con forma se clickea y se
  arrastra igual que antes, en todo su rectángulo — si el hit siguiera el rombo
  exacto, perderías las esquinas que estás viendo.

  **Dónde NO se aplica, y el motor te lo dice.** Si la perilla resolvió a una
  **imagen**, sus píxeles ya traen su silueta y no se le recorta nada encima; el
  motor lo avisa al cerrar el bloque, nombrando la clave y explicando que la
  forma no se aplicó. Si la perilla está en `ninguno`, no hay pieza que vestir.
  En cambio, si **no declaraste la perilla** —o sea que se dibuja la de
  fábrica—, la forma **sí** la viste: es la misma ley que el borde.

  **El contorno sigue a la silueta.** Un `perilla_borde_color` sobre una perilla
  rombo dibuja el contorno del rombo, no una caja alrededor.

  **La forma es sólo de la perilla.** No existen `riel_forma` ni `relleno_forma`,
  y el motor los rechaza con voz: ninguna referencia medida los pidió, y el
  relleno además se recorta por el valor, así que su borde derecho es un corte y
  no un extremo.

  **La forma no tiene estados**: no existe `perilla_forma_al_pasar` ni ninguna
  otra combinación con sufijo, y el motor lo dice con esas palabras si lo
  escribís. *Comparte la forma con la geometría y con el borde, pero no la
  razón:* el tamaño de una pieza **no puede** variar por estado —es uno solo—,
  mientras que su silueta **sí podría**. Hoy no lo hace porque ninguna
  referencia medida lo pidió.

  Ejemplo: la perilla de ese mismo riel fino, medida de una captura real píxel por píxel.

  ```eclips
  # en project.eclips
  estilo_control riel_fino_rombo {
      riel    #C6C6C6
      riel_altura 12
      riel_borde_color #1C1C1C
      riel_borde_grosor 1
      relleno ninguno
      perilla #1C1C1C
      perilla_ancho 9
      perilla_altura 12
      perilla_forma rombo
      perilla_en_extremos dentro
  }
  ```
  En un toggle, flechas, cajas, `tecla.*` o `ranura`, `estilo=`/`style=` emite
  voz de superficie no soportada y conserva el render normal; el puntero de
  skin queda nulo. La declaración sigue siendo válida para una barra horizontal.

  Ejemplo mínimo con PNG. El bloque vive en `project.eclips`:

      estilo_control mi_slider {
          riel "assets/ui/slider/riel.png"
          relleno ninguno
          perilla "assets/ui/slider/perilla.png"
      }

  La pantalla o configuración selecciona el alias en el caller:

      control velocidad_texto estilo=mi_slider x=120 y=180 ancho=420

  El skin no escribe el valor ni altera el dominio, `Theme` o la persistencia.
  `Cps(9999)` sigue siendo CPS finito y distinto de `Instantaneo`.
- Lock automático: si el tema lockea el setting (ej. `velocidad_texto_lock si`,
  `skip_no_visto_lock si`), el control se pinta gris y no responde.
- El volumen se oye al arrastrar; los sfx aplican al próximo sonido. El toggle de
  `pantalla_completa` cambia la ventana **en vivo** al clic.
- El toggle no trae texto propio: poné un `texto "Pantalla completa"` al lado para
  etiquetarlo (estados nombrados tipo `Sí/No` = futuro).
- **Persiste solo.** Lo que el jugador cambia con un control se guarda al **cerrar
  el menú** y se restaura al volver a abrir el juego — incluido el tamaño de ventana,
  que se aplica al arrancar. No tenés que hacer nada: el motor lo persiste por vos.

**`resolucion`/`resolution` — selector de flechas (catálogo nombrado).**

    control resolucion x=760 y=400

  - Cambia el **tamaño de la ventana** (≠ resolución de diseño; tu lienzo y tus
    coordenadas no se tocan — solo el zoom de salida). Aplica **en vivo** al clic.
  - Sale como `◄ 1280x720 ►` (flechas) sin pedir `tipo=`. Las opciones las
    **deriva el motor** del lienzo (escalas 0.5×–2× del mismo aspect → sin barras),
    filtrando las que no caben en tu monitor. ◄ ► se atenúan en los extremos.
  - `tipo=flechas`/`arrows` es el render explícito; cualquier setting de catálogo
    nombrado lo usa por default. La resolución elegida **persiste**: sobrevive al
    reinicio y la ventana abre directo en ella.

**`tipo=cajas`/`boxes` — el catálogo como fila de cajas (en vez de flechas).**

    control resolucion tipo=cajas x=140 y=400 ancho=900

  - Mismo catálogo nombrado que `flechas`, pintado distinto: una **fila de N cajas**,
    la opción actual resaltada. Picás la caja que querés **directo** (no hay que
    ciclar con ◄ ►). Toda la caja registra el clic, no solo el centro.
  - Las cajas se reparten el `ancho`/`width` en **partes iguales** (`ancho ÷ Nº de
    opciones`). Labels que no caben se encieman — `cajas` está pensado para enums
    **cortos** (idioma, dificultad); para labels largos como la resolución, usá un
    `ancho=` holgado (las 5 opciones de resolución piden ~900) o quedate con `flechas`.
  - Default del Enum sigue siendo `flechas`; `cajas` es opt-in con `tipo=`.

**`idioma`/`language` — el jugador elige su idioma (v0.8.41).**

    control idioma tipo=cajas x=200 y=460 ancho=760

  - Lista **los idiomas que declaraste** en `project.eclips` con `idiomas`/`languages`,
    en ese orden, y el primero es el original. No hay nada más que configurar: el
    catálogo sale de ahí.
  - Cada opción sale con el nombre del idioma **escrito en sí mismo** —`Español`,
    `English`, `Français`, `Português`, `Deutsch`, `Italiano`—, no con el código.
    Es a propósito: el jugador que no entiende la pantalla en la que está parado
    igual reconoce la suya. **Un código que el motor no conoce se lista con el
    código tal cual** (`nl` sale como `nl`), así que nunca queda un hueco.
  - **Cambia al instante**, sin reiniciar y sin modal de confirmación: el texto, las
    fechas y los strings del motor pasan al idioma nuevo con el próximo renglón que
    se dibuje. Lo que ya está en pantalla no se re-escribe solo — para verlo, volvé a
    entrar a la escena.
  - Los **assets** ya cargados siguen en el idioma viejo hasta su próxima carga
    natural (cambio de escena, re-entrada de menú). Es la misma política que el resto
    del sistema de idiomas.
  - **Necesita al menos dos idiomas declarados.** Con uno solo el control no se
    registra y el motor te lo dice por consola — un selector de un elemento no es un
    selector.
  - Persiste solo, como los demás: se guarda al cerrar el menú y el juego abre en ese
    idioma la próxima vez.

  > Este control es el que hace que declarar `idiomas es en` sirva de algo sin que
  > el jugador tenga que editar archivos. **Si tu proyecto no tiene una pantalla
  > `config` propia**, el motor cae a su pantalla de configuración interna, que
  > todavía no lleva este control: ahí el idioma se sigue eligiendo a mano.

### `control tecla.<dominio>.<accion>` — remapear una tecla (v0.8.37)

El mismo comando, con un argumento de otra forma: si lo que escribís empieza con
`tecla.` o `key.`, el `control` deja de ser un setting y pasa a ser **la fila de una
tecla** — muestra la que esa acción tiene hoy, y al clickearla el jugador la cambia.

    texto  etq "Avanzar" x=140 y=240 z=10
    control tecla.vn.avanzar x=560 y=232 ancho=240

- El nombre es **el mismo string** que el jugador escribe en su `settings.eclips` y el
  mismo que pinta `${_tecla.vn.avanzar}` (§24). Una sola palabra para las tres cosas.
- **Bilingüe por partes**, igual que la clave del jugador: `control key.game.jump`
  funciona idéntico. La grafía que escribas decide la palabra del vacío
  (`ninguna` / `none`).
- **La etiqueta es tuya.** El widget pinta **sólo la tecla**; el nombre de la acción lo
  ponés con un `texto` al lado, donde quieras. Eclips no impone la pantalla de controles:
  es un `.ecl` más.
- Acepta `x=` `y=` `ancho=` `estilo=`. `min=`/`max=`/`paso=` no aplican (una tecla no
  tiene modelo numérico) y se ignoran.
- El dominio `dev` **se rechaza con aviso**: sus teclas se compilan fuera en build de
  export, así que remapearlas escribiría una línea que el binario del jugador no lee.

**Cómo se usa, del lado del jugador:**

| gesto | qué hace |
|---|---|
| clic en la fila | entra en captura — la fila dice *«presiona una tecla…»* |
| una tecla | la liga y sale |
| **`SUPR`** | **deja la acción sin tecla** → la fila dice `ninguna` |
| la tecla de **Pausa/Volver** | cancela sin cambiar nada |

> **La captura cambia LA PRIMERA tecla, no la lista.** Muchas acciones traen varias de
> fábrica — `avanzar` trae `ESPACIO`, `ENTER`, `ENTER_NUM`, `Z` y el **clic** —, y la fila
> muestra una sola. Remapearla reemplaza **esa**, y las demás siguen funcionando: cambiar
> «Avanzar» a `P` no te deja sin el clic para avanzar. Para dejar la acción **sin ninguna
> tecla**, `SUPR`.

> **Dos cosas que hoy NO se pueden desde la pantalla**, y conviene saberlas antes de
> escribirla: la captura toma **sólo teclas** —ni clics ni rueda—, así que una acción que
> venga ligada a la rueda (como abrir el historial) se puede mover a una tecla y no volver;
> y no hay forma de **restaurar los valores por defecto**. Las dos se pueden hacer a mano
> editando `saves/settings.eclips`.

> **El remapeo se guarda al SALIR de la pantalla**, no en cada cambio, y sólo si el `.ecl`
> corre como menú. Si dos acciones del mismo dominio quedan con la misma tecla, el motor
> **avisa por consola y deja pasar**: compartir tecla es legal —la fase desempata— pero si
> no era tu intención, ahí te enterás.


## 14.11. `ranura` — tarjeta de slot de guardado (Capa 2, Ola 2)

Coloca una **tarjeta de slot** de guardado: muestra la meta del slot (escena, fecha,
último diálogo) y al hacer clic dispara una acción sobre ese slot. Hermana del
`control`, pensada para escribir tu **pantalla de Cargar/Guardar** en `.ecl` y
sustituir el menú de guardado C++.

    ranura 1 accion=cargar x=140 y=150 ancho=460
    ranura 2 accion=cargar x=140 y=240 ancho=460
    ranura 3 accion=cargar x=140 y=330 ancho=460

- `ranura <N>` → número de slot **1..5** (1-based, lo que ve el jugador). Fuera de
  rango = aviso y se ignora.
- `ranura N accion=borrar` → borra el slot N. Borrar es destructivo, así que SIEMPRE
  confirma: un clic en una ranura ocupada levanta un diálogo modal centrado
  (la pantalla queda atenuada detrás) con **No** preseleccionado.
  **Sí** borra el slot y la tarjeta pasa a *Vacío* al instante; **No**, **ESC** o
  clic fuera cancelan sin tocar nada. Una ranura vacía con `accion=borrar` no hace nada.
  El confirm es automático — no se escribe aparte (no hay borrado sin confirmación).
- `accion=`/`action=` → qué hace el clic: **`cargar`/`load`**, **`guardar`/`save`**
  o **`borrar`/`delete`** (las tres cableadas; ver detalle arriba y abajo).
- `x` `y` posición · `ancho`/`width` (default 360) y `alto`/`height` → tamaño de la
  tarjeta; cada ranura se dimensiona individual. El **hitbox sigue al tamaño** (clic
  y hover crecen con la tarjeta).
- **`alto=auto`** — el motor mide la tarjeta a su contenido (thumbnail + líneas de
  texto + márgenes) y la ajusta sola. En **layout vertical**, si NO pones `alto=`,
  el default YA es `auto` (72 no contiene el stack vertical → la tarjeta quedaría
  corta y el hover no cubriría todo). En layout inline sin `alto=` sigue en 72.
  Un `alto=N` explícito siempre manda sobre el auto.
- `estilo=` (estilo nombrado por-instancia) reservado para un caller futuro. El
  **color y el sprite de la tarjeta SÍ se tematizan** hoy vía keys de tema — ver
  "Estilo de la tarjeta" abajo.
- **Render automático:** slot ocupado → borde vivo + escena + fecha + preview del
  último diálogo (recortado con "…" si no cabe). Slot vacío → "Vacío" atenuado.
  La meta se lee una vez al abrir la pantalla.
- **`cargar` en un slot vacío no hace nada** (no arranca un juego nuevo). En slot
  ocupado: desde el título carga la partida; a mitad de juego carga in-place.
- **`accion=guardar`** (o `save`) — **solo desde una pausa-sobre-juego** (una pantalla
  abierta con `pausa`/`guardar` mientras corre el juego). Clic en la ranura snapshotea el
  juego CONGELADO de abajo en ese slot; la tarjeta pasa a "ocupada" al instante. Desde un
  menú-título (sin juego abajo) se ignora con aviso. Hermano simétrico de `accion=cargar`,
  que desde una pausa carga **in-place** y te devuelve al juego (no apila un juego nuevo).

**Tu pantalla de Cargar es una escena.** Igual que `config` (override), registrás la
pantalla en `project.eclips` y la abrís con el verbo `cargar`:

    # project.eclips
    pantallas {
        cargar "load.ecl"
    }

    # load.ecl
    escena cargar
      fondo "assets/.../fondo.png"
      ocultar_caja
      texto titulo "CARGAR PARTIDA" x=140 y=70 z=10
      ranura 1 accion=cargar x=140 y=150 ancho=460
      ranura 2 accion=cargar x=140 y=240 ancho=460
      elegir:
        "Volver" -> fin

Un clic en una ranura ocupada carga la partida y te devuelve al título (la pantalla
de Cargar se cierra sola); "Volver" la cierra y revela el menú. La pantalla de Cargar
vive en **rol Menu** (se abre desde el título).

### Estilo de la tarjeta (tema) — colores y sprite idle/hover (v0.6.20)

El **color y el sprite** de la ranura se tematizan desde tu `.eclips` de tema (no
en el comando `ranura`). Familia paralela a `choice_*`, **Modelo A bilingüe puro**:
prefijo `ranura_*` (español) o `slot_*` (inglés), cada key monolingüe pura.

Todo es **opt-in**: sin keys, la tarjeta usa el default del engine (idéntico a
antes). El hover por defecto = el idle → off gratis.

**Colores** (`#RRGGBB`):

| Español | English | Qué |
|---|---|---|
| `ranura_fondo_color` | `slot_bg_color` | panel ocupado |
| `ranura_borde_color` | `slot_border_color` | borde ocupado |
| `ranura_borde_grosor` | `slot_border_thickness` | grosor del borde (px) |
| `ranura_hover_fondo_color` | `slot_hover_bg_color` | panel ocupado al hover |
| `ranura_hover_borde_color` | `slot_hover_border_color` | borde ocupado al hover |
| `ranura_hover_texto_color` | `slot_hover_text_color` | **texto al hover** — tiñe escena+fecha+preview a la vez |
| `ranura_vacia_fondo_color` | `slot_empty_bg_color` | panel vacío |
| `ranura_vacia_borde_color` | `slot_empty_border_color` | borde vacío |
| `ranura_vacia_hover_fondo_color` | `slot_empty_hover_bg_color` | panel vacío al hover |
| `ranura_vacia_hover_borde_color` | `slot_empty_hover_border_color` | borde vacío al hover |
| `ranura_vacia_hover_texto_color` | `slot_empty_hover_text_color` | "Vacío" al hover |
| `ranura_hover_barra_color` | `slot_hover_bar_color` | **barra de foco al hover** — palito de color a la izquierda |
| `ranura_hover_barra_grosor` | `slot_hover_bar_thickness` | grosor de la barra en px (0 = off, default) |
| `ranura_vacia_inactiva_opacidad` | `slot_empty_inactive_opacity` | opacidad de la vacía **inerte** (modos `cargar`/`borrar`, donde no hay nada que hacer): `<1.0` la atenúa **y** la enmudece (no se ilumina al hover, sin barra) — el "empty slot" gris de Ren'Py en Load. `1.0` = off (default, byte-idéntico). En `guardar` no aplica (el vacío es destino válido → viva). Número desnudo (sin comillas). |

**Texto autorable (entre comillas — ver "Comillas en .eclips"):**

| Español | Inglés | Qué hace |
|---|---|---|
| `ranura_vacia_etiqueta` | `slot_empty_label` | rótulo del slot vacío (default `"Vacio"`) |
| `ranura_fecha_formato` | `slot_date_format` | plantilla de la fecha (tokens `{dia_pad}`, `{mes_nombre}` etc.) |
| `ranura_fecha_idioma` | `slot_date_language` | idioma de los nombres de mes/día (`auto` = el del jugador) |

**Sprite (cara PNG) — solo ranura ocupada:**

| Español | English | Qué |
|---|---|---|
| `ranura_sprite` | `slot_sprite` | PNG idle |
| `ranura_sprite_hover` | `slot_sprite_hover` | PNG al hover (sin él, queda el idle) |
| `ranura_sprite_borde_izq` | `slot_sprite_border_left` | borde 9-slice izquierdo |
| `ranura_sprite_borde_der` | `slot_sprite_border_right` | borde 9-slice derecho |
| `ranura_sprite_borde_arriba` | `slot_sprite_border_top` | borde 9-slice superior |
| `ranura_sprite_borde_abajo` | `slot_sprite_border_bottom` | borde 9-slice inferior |

Los 4 bordes 9-slice marcan los px que **no** se estiran (0 = stretch puro). El
sprite tapa el color (los `*_color` quedan de fallback si el PNG no carga). La
ranura **vacía** no tiene sprite — se pinta con color (como en Ren'Py).

**Ejemplo (tarjetas con PNG):**

    # en tu .eclips de tema
    ranura_hover_texto_color  #FF6BAA          # las letras se tiñen de rosita al hover
    ranura_sprite        "assets/ui/slot_idle.png"
    ranura_sprite_hover  "assets/ui/slot_hover.png"

**Thumbnail: caja de fit y layout (v0.6.29):**

El thumbnail (la captura del save) cae dentro de una **caja** cuya forma y ajuste
controlás desde el tema. Modelo A:

| Español | English | Qué |
|---|---|---|
| `ranura_thumb_aspecto` | `slot_thumb_aspect` | forma de la caja: `auto` (sigue el aspecto real de cada captura, default), `16:9` (ancho:alto) o un flotante (`1.777`) |
| `ranura_thumb_ajuste` | `slot_thumb_fit` | cómo cae la captura en la caja (solo aplica si `aspecto` ≠ `auto`) |

Con `aspecto auto` cada thumbnail conserva su proporción → idéntico a antes. Al
fijar un aspecto las tarjetas quedan **uniformes** y el `ajuste` decide:

- `contener` / `contain` — cabe entera, sin recorte (deja barras si no calza).
- `cubrir` / `cover` — llena la caja y **recorta** lo que sobra, centrado (default).
- `estirar` / `stretch` — llena deformando (sin recorte, sin barras).

**Layout de la tarjeta:**

| Español | English | Qué |
|---|---|---|
| `ranura_layout` | `slot_layout` | `inline` (thumb izq + texto derecha, default) o `vertical` (thumb ancho arriba + texto debajo, estilo Ren'Py) |

En `vertical` el thumb va a **ancho completo**, así que la tarjeta necesita más
alto: dale un `alto=` generoso en el comando `ranura` o el contenido se desborda
por debajo (y con él, el área clickable — el hitbox sigue a la tarjeta, no al contenido).

**Ejemplo (thumbnails uniformes 16:9, look Ren'Py):**

    ranura_thumb_aspecto 16:9
    ranura_thumb_ajuste  cubrir
    # ranura_layout vertical   # descomenta para el stack vertical (recordá subir alto=)

**Formato de fecha (tema) — tokens (v0.6.22):**

La fecha que muestra cada tarjeta se arma con una **plantilla de tokens** que tú
defines. Una sola key, monolingüe Modelo A:

| Español | English | Qué |
|---|---|---|
| `ranura_fecha_formato` | `slot_date_format` | plantilla de la fecha del slot |
| `ranura_fecha_idioma` | `slot_date_language` | idioma de `{mes_nombre}`/`{dia_nombre}` (`auto` = sigue al jugador) |

Sin la key, el default = `{dia_pad}/{mes_pad}/{anio}  {hora_pad}:{minuto_pad}` → se
ve igual que siempre (`23/06/2026  15:35`). Los tokens van entre llaves `{}`, son
**bilingües**, y `_pad` rellena con cero a la izquierda:

| Token (es / en) | Qué | Ej. |
|---|---|---|
| `{dia}` / `{day}` | día del mes | `5` |
| `{dia_pad}` / `{day_pad}` | día con cero | `05` |
| `{mes}` / `{month}` | mes (número) | `6` |
| `{mes_pad}` / `{month_pad}` | mes con cero | `06` |
| `{anio}` / `{year}` | año | `2026` |
| `{hora}` / `{hour}` | hora 24h | `7` |
| `{hora_pad}` / `{hour_pad}` | hora con cero | `07` |
| `{minuto}` / `{minute}` | minuto | `3` |
| `{minuto_pad}` / `{minute_pad}` | minuto con cero | `03` |
| `{dia_semana}` / `{weekday}` | día de semana (1=domingo … 7=sábado) | `4` |
| `{mes_nombre}` / `{month_name}` | mes con **nombre** | `julio` |
| `{dia_nombre}` / `{weekday_name}` | día con **nombre** | `lunes` |

Todo lo que NO sea un token se copia tal cual (`/`, `:`, `-`, comas, espacios,
texto). Un token mal escrito sale literal (`{xyz}` → `{xyz}`), así lo cazas a ojo.

    # ejemplos en tu .eclips de tema
    ranura_fecha_formato  "{anio}-{mes_pad}-{dia_pad} {hora_pad}:{minuto_pad}"          # ISO → 2026-06-23 15:35
    ranura_fecha_formato  "{dia_semana}, {mes} {dia} {anio}, {hora_pad}:{minuto_pad}"   # orden tipo Ren'Py (en números)
    ranura_fecha_formato  "{dia} de {mes_nombre} de {anio}"                             # → 6 de julio de 2026

Los nombres salen en **minúscula** (`julio`, `lunes`), que es la grafía española. Si
querés `Julio`, escribilo capitalizado en tu traducción; el motor no impone mayúsculas
que después haya que deshacer por idioma.

**Las abreviaturas (`{mes_abrev}`) NO existen todavía.** Un tema que las escriba las va
a ver literales — visible, no roto.

### El idioma de los nombres: `ranura_fecha_idioma` / `slot_date_language`

    ranura_fecha_idioma   auto      # default: los nombres siguen el idioma del jugador
    ranura_fecha_idioma   en        # siempre en inglés, aunque el jugador cambie de idioma
    slot_date_language    en        # lo mismo, grafía inglesa

Sólo afecta a `{mes_nombre}` y `{dia_nombre}`: **los números son universales** y no se
mueven. Es el look Ren'Py, donde la fecha del save queda en inglés siempre.

**Funciona aunque tu proyecto no declare `idiomas`.** Un tema con
`ranura_fecha_idioma en` y `{mes_nombre}` obtiene `July` directo, sin sistema de
idiomas de por medio. Sin la key y sin catálogo, los nombres salen en español.

> **Si tu juego habla un idioma que el motor no trae de fábrica** (trae `es` y `en`):
> los nombres salen en el idioma **original** de tu proyecto y pasan por tu `.lang`,
> así que se traducen escribiendo `"julio" = "juillet"` como cualquier otra línea.
>
> **Y la plantilla entera también se traduce**, lo cual resuelve el orden y el relleno
> de un saque — los tokens viajan crudos dentro de la clave:
>
>     "{dia} de {mes_nombre}" = "{mes_nombre} {dia}"
>
> En `en.lang` eso convierte `6 de julio` en `July 6`: se voltea el orden y el «de»
> desaparece, sin tocar el tema.

## 15. Animaciones y Tweens / Animations & Tweens

Sistema de animación para sprites de personajes y pantalla. Los efectos se aplican con `animar`/`animate` y se detienen con `detener_animacion`/`stop_animation`.

Animation system for character sprites and screen. Effects are applied with `animar`/`animate` and stopped with `detener_animacion`/`stop_animation`.

### Sintaxis

```
animar [objetivo] [efecto] [duración] [intensidad=N] [periodo=N] [eje=x|y]
animate [target] [effect] [duration] [intensity=N] [period=N] [axis=x|y]
```

- **objetivo**: alias del personaje, el `fondo`, el nombre de una capa, o `pantalla`/`screen` (ver **Los blancos**, más abajo)
- **efecto**: nombre del efecto (ver tabla)
- **duración**: opcional, en segundos con sufijo `s` (ej: `0.5s`) — o usá `en=N` (ver **Los drivers**)
- **intensidad**: opcional, controla la fuerza del efecto (default: 1.0)
- **periodo**: opcional, segundos por ciclo — **solo lo usa `vaiven`** (default: 1.5)
- **eje**: opcional, `x`/`horizontal` o `y`/`vertical` — **solo lo usa `vaiven`** (default: `x`)

**El orden de los `nombre=valor` no importa.** `animar m vaiven eje=y intensidad=20` y `animar m vaiven intensidad=20 eje=y` son exactamente lo mismo. Si le pedís `periodo=` o `eje=` a un efecto que no los usa, el motor te avisa: **el ritmo de los demás efectos está horneado en su receta**.

**El motor revisa tu línea al cargar el guion, no cuando corre.** Si algo está mal, te lo dice con el número de línea — antes de que juegues:

    animar m xyz                      → "no conozco el efecto 'xyz'. Ese comando no hara nada."
    animar m franja colorr=#00ff00    → "franja: no conozco el parametro 'colorr='. Parametros: color1= color2= ..."
    animar m franja separacion=hola   → "franja: separacion= espera un numero, recibio 'hola'."
    animar m franja color1=verde      → "franja: color1= espera un color (#rrggbb), recibio 'verde'."

En todos los casos el motor **usa el valor por defecto y sigue** — no se rompe. Pero te avisa, porque un parámetro mal escrito que se ignora en silencio es la peor forma de perder una tarde.

### Efectos disponibles

| Español | English | Tipo | Descripción |
|---------|---------|------|-------------|
| `temblar` | `shake` | Puntual | Vibración horizontal rápida con decaimiento |
| `saltar` | `bounce` | Puntual | Salto parabólico con gravedad natural — **no es `brincar`**, ver la nota de abajo |
| `acercar` | `zoom_in` | Persiste | Escala sube y se mantiene |
| `alejar` | `zoom_out` | Puntual | Escala vuelve a 1.0 |
| `flotar` | `float` | Loop | Sube y baja suavemente (aterriza suave al detener) |
| `pulso` | `pulse` | Loop | Alpha oscila (aterriza suave al detener) |
| `vaiven` | `nudge` | Loop | Vaivén suave de ida y vuelta. **El único con `periodo=` y `eje=`** — y ahora el motor lo hace cumplir: si se los pedís a otro, te avisa |
| `destello` | `tint_flash` | Puntual | Flash blanco sobre sprite o pantalla |
| `caer` | `drop` | Persiste | Sprite baja y se queda (aterriza suave al detener) |
| `subir` | `rise` | Persiste | Sprite sube y se queda (aterriza suave al detener) |
| `brincar` | `hop` | Puntual | Brinquito rápido, vuelve a normal — **no es `saltar`**, ver la nota de abajo |
| `retroceder` | `lean_back` | Puntual | Salto hacia atrás + vuelve a normal |

> **Sobre `pantalla`:** dibuja **desplazamiento** (`temblar`, `vaiven`) y **destello**. Los efectos de **escala** (`acercar` / `alejar` / `retroceder`) sobre `pantalla` **todavía no tienen render** — el motor te avisa por consola si se los pedís.

> **⚠ `saltar` y `brincar` NO son la misma palabra en distinto tono.** Suenan a
> sinónimos y son **dos efectos distintos**: `saltar`/`bounce` es un **salto
> parabólico con gravedad** —sube alto, cae y rebota— y `brincar`/`hop` es un
> **brinquito corto** que vuelve a su sitio de una. Si escribís uno esperando el
> otro, el motor **no te va a avisar**: los dos existen y los dos son válidos.
>
> El par bilingüe de cada uno es **completo y no se cruza**: `saltar` = `bounce`,
> `brincar` = `hop`. Lo que no existe es un `saltar` en inglés que sea `hop`.
>
> La regla general, por si te topás con otro caso así: **dos nombres parecidos
> del mismo tipo de efecto casi nunca son alias.** Los alias reales de este
> manual son siempre **un español y un inglés** — nunca dos palabras del mismo
> idioma. Si ves dos en español, son dos cosas.

### Efectos de render — la corrupción, en piezas

Los de la tabla de arriba **mueven** al personaje: lo desplazan, lo escalan, le cambian el alpha. Los de **render** cambian **cómo se dibuja**. Son otra especie, se piden con el mismo `animar`, y **se apilan con los otros**.

Hasta hace poco, `glitch` era **una receta cerrada**: siempre rojo y azul, siempre 5 píxeles, siempre al mismo ritmo. Cualquier juego que lo usara **se delataba como hecho en Eclips**. Eso se acabó — **el glitch está hecho de tres piezas, y ahora podés pedirlas sueltas.**

| Español | English | Qué hace |
|---------|---------|----------|
| `franja` | `fringe` | Separación de color (la "aberración cromática"). Dos copias del personaje, teñidas, oscilando a los lados |
| `tajadas` | `slices` | Corta al personaje en rebanadas horizontales y **desplaza algunas** |
| `parpadeo` | `flicker` | Le baja la opacidad **a golpes**, con un patrón que parece azaroso |
| `bloques` | `blocks` | Grilla de celdas; algunas **muestran otra parte de la imagen** (el ojo en el torso, en cubitos) |
| `estatica` | `static` | Ruido de TV encima — **sin necesitar ningún PNG**, el motor lo genera |
| `sustituto` | `override` | **Tapa** el elemento con otra imagen (cambiar de cara, "señal perdida") |
| `glitch` | `glitch` | **franja + parpadeo + tajadas juntas.** La receta de siempre, sellada |

---

**`franja` — la aberración cromática**

| Parámetro | Default | Qué hace |
|-----------|---------|----------|
| `color1=` | `#ff0000` | El color de la copia que va a la derecha |
| `color2=` | `#0000ff` | El de la que va a la izquierda |
| `separacion=` | `5` | Cuántos píxeles se separan |
| `alpha=` | `0.35` | Qué tan visibles son las copias (0 a 1) |
| `velocidad=` | `1` | Qué tan rápido oscilan |
| `intensidad=` | `1` | Multiplica separación y opacidad a la vez |

    animar m franja color1=#00ff00 color2=#ff00ff separacion=8

**Verde y magenta, más ancha.** Ese es el punto: el mismo efecto, sin la firma.

---

**`tajadas` — la imagen rota**

| Parámetro | Default | Qué hace |
|-----------|---------|----------|
| `n=` | `10` | En cuántas rebanadas se corta |
| `fuerza=` | `12` | Hasta cuántos píxeles se corre una rebanada rota |
| `proporcion=` | `0.33` | Qué fracción de las rebanadas se rompe (0 a 1) |
| `velocidad=` | `8` | Cuántas veces por segundo cambia el desgarro |
| `intensidad=` | `1` | Multiplica la fuerza |

    animar m tajadas n=4 fuerza=30

**Cuatro cortes grandes, sin franja.** El glitch nunca te dejó pedir esto.

    animar m tajadas velocidad=0 proporcion=0.5

**Una imagen rota, QUIETA.** `velocidad=0` **congela el efecto**: las mismas rebanadas rotas, en el mismo lugar, para siempre. Una corrupción que no se mueve inquieta más que una que tiembla. *(Vale para todos: `velocidad=0` congela cualquier efecto en su primer cuadro.)*

    animar m tajadas n=1 proporcion=1 fuerza=40

**Con `n=1` la "rebanada" es el personaje entero** — así que salta de golpe, completo. No es un error: es la misma ley, con una sola tajada.

---

**`parpadeo` — la falla eléctrica**

| Parámetro | Default | Qué hace |
|-----------|---------|----------|
| `alpha=` | `0.15` | **Cuánto se atenúa** cuando parpadea. Es un **factor**: `1` = no atenúa nada (o sea, no hace nada) |
| `velocidad=` | `15` | El ritmo del parpadeo, en cuadros por segundo |
| `intensidad=` | `1` | Qué tan **hondo** es el bajón. `0` = nada; `2` = el doble |

    animar m parpadeo alpha=0 velocidad=30

**Desaparece a intervalos**, rápido. No hace falta un glitch entero para que alguien no esté del todo ahí.

---

**`glitch` — la receta, sellada**

    animar m glitch
    animar m glitch 2s
    animar m glitch intensidad=1.5

`glitch` **es** `franja` + `parpadeo` + `tajadas`, con las constantes de siempre. **Se ve exactamente igual que antes**, y su única perilla es `intensidad=` — es un **preset sellado**: si le pedís `n=` o `color1=`, el motor te avisa y te dice que use las piezas sueltas.

| Intensidad | Qué sale |
|------------|----------|
| 0.1 – 0.5 | Solo separación RGB sutil. *"Algo está mal"* |
| 0.8 – 1.0 | RGB + parpadeo. *"El juego falló"* |
| 1.5 – 3.0 | RGB + parpadeo + cortes. Terror puro |

---

**`bloques` — los cubitos**

| Parámetro | Default | Qué hace |
|-----------|---------|----------|
| `nx=` | `6` | Columnas de la grilla |
| `ny=` | `6` | Filas de la grilla |
| `proporcion=` | `0.15` | Qué fracción de las celdas se corrompe (0 a 1) |
| `fuerza=` | `0` | Píxeles de corrimiento extra de las celdas rotas (opcional) |
| `velocidad=` | `8` | Cuántas veces por segundo cambia qué celdas se rompen |
| `intensidad=` | `1` | Multiplica proporción y fuerza |

    animar m bloques proporcion=0.25 fuerza=20

**Una celda muestra lo que hay en otra** — el ojo aparece en el torso, la boca en la frente. Una imagen que se corrompe a pedazos.

---

**`estatica` — el ruido de TV** *(cero assets)*

| Parámetro | Default | Qué hace |
|-----------|---------|----------|
| `alpha=` | `0.5` | Qué tan opaco es el ruido (0 a 1) |
| `grano=` | `2` | Tamaño del grano (más grande = más gordo) |
| `velocidad=` | `12` | Cuántas veces por segundo se re-sortea |
| `textura=` | *(generada)* | Una imagen tuya en vez del ruido gris del motor |
| `intensidad=` | `1` | Multiplica la opacidad |

    animar fondo estatica grano=4 alpha=0.3

**El motor genera el ruido solo** — no necesitás ningún PNG. Ideal sobre el `fondo` (ver abajo) para una escena de terror.

> ✅ **Sobre un personaje, el ruido se recorta a su SILUETA** *(v0.8.5)*. Antes estampaba
> el rectángulo completo del sprite —padding transparente incluido—, y se veía un cuadrado
> de ruido alrededor del personaje. Ahora el ruido vive **dentro del cuerpo**. Sobre un
> `fondo` no cambia nada (ahí el rectángulo ES la pantalla, que es lo correcto).
> No hay nada que activar: pasa solo.

> 💡 **El ORDEN de las líneas de una receta elige el efecto.** Con `estatica` y `tajadas`
> juntas hay **dos imágenes distintas**, y la línea que escribas primero decide cuál:
>
>     # el ruido se ROMPE con ella: una sola pieza desgarrada
>     animacion rota estatica alpha=0.7
>     animacion rota tajadas n=5 fuerza=60
>
>     # el ruido se queda QUIETO encima mientras el cuerpo se rompe atrás
>     animacion rota tajadas n=5 fuerza=60
>     animacion rota estatica alpha=0.7
>
> Ninguna está mal — la primera se siente "el personaje está corrupto", la segunda
> "algo se interpone entre vos y el personaje". Es la ley general de las recetas de
> efecto (cada línea transforma lo que dejó la anterior), pero acá se ve de un vistazo.

---

**`sustituto` — cambiar la imagen**

| Parámetro | Default | Qué hace |
|-----------|---------|----------|
| `textura=` | *(obligatorio)* | La imagen que **tapa** al elemento |
| `intensidad=` | `1` | Opacidad del sustituto: `1` tapa del todo, `0.5` lo deja como fantasma |

    animar fondo sustituto textura="assets/fondo_roto.png"

**Tapa el elemento entero** con otra imagen. Sirve para un cambio de cara súbito, un *"señal perdida"*, o el fondo que se rompe por medio segundo. Sin `textura=`, el motor te avisa y no hace nada.

---

**Todos se apilan, con todo:**

    animar m flotar                    # sube y baja...
    animar m franja color1=#00ff00     # ...y encima se le corre el color
    animar m parpadeo alpha=0.4        # ...y encima falla la luz

**Duración, igual que siempre:** `animar m franja 2s` dura dos segundos y se apaga.
**Se detienen igual que siempre:** `detener_animacion m franja` (o `detener_animacion m` para bajar todo).

> **El motor te corrige.** Si escribís `separacion=hola`, `color1=verde`, `velocidad=-1` o un parámetro que ese efecto no conoce, **te lo dice por consola, con el número de línea** — y te lista los que sí acepta. `velocidad=-1` además te dice a qué valor lo ajustó.

### Los blancos: quién se corrompe

Los efectos de render no son solo para personajes. Podés corromper:

| Blanco | Ejemplo |
|--------|---------|
| Un **personaje** (por su alias) | `animar sol franja` |
| El **`fondo`** / `background` | `animar fondo estatica` |
| Una **capa** de imagen (por su nombre) | `animar mesa bloques` |

*(Los `capa` de **texto** o **consola** no se pueden corromper con esto — el motor te avisa. Y `pantalla` completa es una ficha aparte: por ahora los efectos corren sobre elementos, no sobre toda la pantalla.)*

> **Ojo:** los efectos de **render** corren sobre `fondo`/capas, pero las **animaciones de movimiento** (`flotar`, `vaiven`, etc.) sobre fondo/capas siguen yendo por `capa_mover`/`capa_fade`. El motor te avisa si te confundís.

### El cuándo: los drivers

Por defecto, un efecto corre **hasta que lo detenés**. Pero podés decidir *cuándo*:

| Driver | Cómo se pide | Qué hace |
|--------|--------------|----------|
| *(continuo)* | *(nada)* | Corre hasta `detener_animacion`. Lo de siempre. |
| **una sola vez** | `en=N` | Corre **N segundos y se apaga solo** (sin `detener`). |
| **esporádico** | `media=` `varianza=` `frames=` `desde=` | **Dormido**; dispara ráfagas cortas a intervalos irregulares. El reloj del terror. |

**El esporádico** — el más interesante para el terror:

| Parámetro | Default | Qué hace |
|-----------|---------|----------|
| `media=` | `8` | Segundos promedio entre disparos |
| `varianza=` | `0` | Cuánto varía ese intervalo (± segundos) → `media=3 varianza=2` = entre 1 y 5s |
| `frames=` | `2` | Cuántos cuadros dura cada ráfaga |
| `desde=` | `0` | Fase de confianza: **jamás dispara antes de N segundos** de vida |

    # "ausencia": el personaje desaparece 2 frames, sin patrón, cada 1-5s
    animar m parpadeo alpha=0 velocidad=0 media=3 varianza=2 frames=2 desde=2

**Es sutil a propósito.** Si el jugador duda de si lo vio, el efecto está haciendo su trabajo. `desde=` te deja que la escena "respire" antes de que empiece el horror.

> **Un driver por línea.** Si mezclás `en=` (una vez) con `media=` (esporádico), el motor te avisa y gana el esporádico. Las claves de driver (`en`, `media`, `varianza`, `frames`, `desde`) están **reservadas** — ninguna primitiva puede usarlas para otra cosa.

### Tipos de comportamiento

- **Puntual**: dura un tiempo fijo y termina. El sprite vuelve a su estado normal.
- **Persiste**: el efecto se mantiene hasta que se detiene manualmente. Se guarda en partidas.
- **Loop**: repite infinitamente hasta detenerse. Se guarda en partidas. Aterrizaje suave al detener.
- **Loop/Puntual**: sin duración = loop, con duración = puntual.

### Animaciones apilables

**Un personaje puede correr varias animaciones a la vez.** Se componen solas: los desplazamientos se **suman**, las escalas y los alphas se **multiplican**.

    animar m flotar          # sube y baja...
    animar m pulso           # ...mientras le late el alpha...
    animar m glitch          # ...y se corrompe. Los tres a la vez.

**Re-emitir el mismo efecto lo reinicia** (no lo duplica). Un efecto **distinto** se apila encima.

**Excepción — los efectos de escala se turnan.** `acercar`, `alejar` y `retroceder` **no** se apilan entre sí: sus escalas se multiplicarían y el personaje quedaría en un tamaño que nadie pidió. El nuevo **hereda la escala actual** y reemplaza al anterior.

### Detener

`detener_animacion <alias>` baja **todas** las animaciones del personaje;
`detener_animacion <alias> <efecto>` baja **sólo** esa:

    animar m flotar
    animar m pulso
    detener_animacion m pulso    # deja de latir, pero SIGUE flotando
    detener_animacion m          # ahora sí, todo

Los efectos **Loop** y **Persiste** **aterrizan suave** al detenerse — no cortan de golpe.

> ✅ **Guardar y cargar — el payload v4 conserva los blancos que cubre** *(v0.8.3)*: un `vaiven` de personaje conserva su `periodo=` y su `eje=`; los efectos de render (`franja`, `tajadas`, `estatica`...) sobreviven con sus parámetros; un efecto `esporadico` dormido **carga con su reloj intacto** — no re-empieza su cuenta. Los efectos apilados vuelven **en el mismo orden** en que los pediste (y en los efectos de render, el orden importa). *(Los saves VN v1, v2 y v3 siguen cargando sin reescribirse. Los tracks de capa explicados abajo son runtime-only en su primer corte: todavía no entran al save.)*

---

## 15.5 Animaciones nombradas / Named animations (`animacion`, v0.8.1)

Hasta acá, los movimientos son **presets** (recetas del motor: `flotar`, `temblar`...)
y los efectos son **primitivas** con parámetros. Con `animacion` armás **las tuyas**:
un nombre que agrupa **canales de movimiento** y/o **líneas de efecto**, y se aplica
con el mismo `animar` de siempre.

```
# El logo cae desde arriba, PICA (rebote) y se queda — mientras aparece en fade:
animacion caida_logo mover_y de=-300 a=0 en=1.2 curva=rebote
animacion caida_logo alpha de=0 a=1 en=0.4 curva=suave

# Un fantasma: flota Y se transparenta, EN FASE (mismo periodo):
animacion fantasma mover_y amplitud=18 periodo=3 modo=vaiven
animacion fantasma alpha amplitud=0.3 periodo=3 modo=vaiven

# Corrupción: dos efectos de render bajo un nombre:
animacion corrupcion franja separacion=8 alpha=0.5
animacion corrupcion tajadas n=6 fuerza=20

animar sol caida_logo      # se aplica como cualquier efecto
animar sol corrupcion
detener_animacion sol corrupcion   # baja el CONJUNTO entero
```

**Cada línea agrega UNA cosa a la receta** (no hay bloque con llaves). Se definen
donde quieras — arriba del archivo, como `personaje`, es lo natural — pero **antes
de usarse, en orden de archivo** (como `fuente`). La línea pelada
(`animacion corrupcion`) declara la receta vacía — o la **resetea** si ya existía.

### Los canales / Channels

| Canal | Inglés | Qué mueve |
|-------|--------|-----------|
| `mover_x` | `move_x` | Desplazamiento horizontal (px) |
| `mover_y` | `move_y` | Desplazamiento vertical (px) |
| `escala` | `scale` | Tamaño (factor: 1 = normal) |
| `alpha` | — | Transparencia (0 a 1) |

*(El canal `tinte` llega con el tinte animado — el motor te avisa si lo pedís.)*

### Parámetros de un canal

| Parámetro | Inglés | Qué hace |
|-----------|--------|----------|
| `de=` / `a=` | `from=` / `to=` | Valor inicial y final. Sin `de=`, arranca del reposo del canal (0 en movimiento, 1 en escala/alpha) |
| `amplitud=` | `amplitude=` | Azúcar de `de=`/`a=`: oscila desde el reposo. `mover_y amplitud=10` va de 0 a 10; `alpha amplitud=0.3` atenúa hasta 0.7; **negativa crece** (`escala amplitud=-0.2` agranda a 1.2) |
| `en=` | `over=` | Duración para `modo=una_vez`: segundos (`0.5s`) o, solo en escala de personaje, avances de diálogo (`8avances_dialogo`) |
| `periodo=` | `period=` | Segundos por ciclo (para `loop`/`vaiven`); nunca acepta avances de diálogo |
| `curva=` | `curve=` | `lineal`, `suave`, `salida`, `entrada`, `resorte`, `rebote`, `cubica` |
| `modo=` | `mode=` | `una_vez` *(default)*, `loop` (o `bucle`), `vaiven` (o `pingpong`) |

- **`una_vez` termina y SE QUEDA**: el logo que cayó no vuelve a subir. Solo
  `detener`/`ocultar` lo regresan al reposo.
- **`loop` es lineal por ley** (un easing pulsaría en cada vuelta — el motor avisa
  e ignora la curva).
- **`resorte` en `alpha`**: la curva sobrepasa 1.0 a propósito, y un alpha no puede
  — el motor avisa y lo recorta al consumirse (meseta en el pico).
- **Mismo periodo = en fase por construcción**: dos canales con `periodo=3` van
  juntos SIEMPRE, sin soldadura.
- Redefinir un canal ya declarado **reemplaza esa línea** (edición iterativa: el eco
  lo marca con `~`).

### Demorar el despacho de una receta / Delay a named recipe (v0.8.51)

Una receta nombrada **pura de canales** puede esperar antes de comenzar su
transición:

```eclips
# Español
animar logo caida_logo despues de 0.5s

# English — la misma demora y la misma época para todos los canales
animate logo caida_logo after 0.5s
```

`despues` y `después` son la misma palabra. La demora pertenece a la
invocación completa: mientras transcurren `N` segundos, todos sus canales
conservan `de`; en `N` comienza la curva y `esperar_animacion` cuenta tanto la
demora como la rampa. Volver a emitir la receta reinicia la invocación; detener
su nombre también la cancela si todavía está en la demora. `despues de 0s` (o
`after 0s`) equivale a omitir la cláusula.

Este corte admite únicamente recetas nombradas cuyos canales sean
`una_vez` y usen reloj `Seconds`. Presets, efectos, Lua, recetas mixtas,
`loop`, `vaiven` y relojes de diálogo se rechazan completos; no se ejecuta una
parte compatible como degradación silenciosa. La forma provisional
`despues=` / `after=` está retirada y se rechaza con orientación hacia la forma
con `de` / `after` escrita arriba.

### Mover una capa con una animación nombrada

Una capa que ya existe puede consumir los canales horizontales y verticales de una
receta nombrada:

```eclips
capa panel "assets/ui/panel.png" x=100 y=200 w=600 h=300 z=10
animacion entrada_panel mover_x de=-80 a=0 en=0.6 curva=cubica
animacion entrada_panel mover_y de=20 a=0 en=0.6 curva=cubica
animar panel entrada_panel
esperar_animacion
```

- Sobre capas, este primer puente acepta **solo** `mover_x`/`move_x` y
  `mover_y`/`move_y`, con reloj en **segundos**.
- Los valores son offsets: se **suman** a la posición base de `capa` y a cualquier
  `capa_mover` que ya esté corriendo. Ninguno reinicia al otro ni acumula deriva.
- Repetir `animar panel entrada_panel` reemplaza esa receta sobre esa capa y
  reinicia su reloj. Otra receta se apila. `detener_animacion panel entrada_panel`
  baja solo ese grupo; sin el segundo nombre, baja todos sus tracks.
- Render, efectos de la capa y click usan la misma posición efectiva. Si el panel
  se mueve, se ve y se clickea en el mismo lugar.
- Si una receta mezcla XY con `escala`, `alpha`, `tinte` o reloj por diálogo, sus
  canales se rechazan **todos juntos**; no corre solo la mitad compatible.
- El blanco especial `fondo`/`background` todavía no entra por este puente. Una
  capa común puede llamarse como quieras; no existe ningún trato especial para
  nombres o assets concretos.
- Si el blanco no está en escena, conserva la regla histórica de `animar`: se
  preconfigura como personaje futuro. Si hay a la vez un personaje vivo y una capa
  con el mismo id, gana el personaje vivo.
- Los tracks de capa son de ejecución: **todavía no se guardan en slots**. Alpha,
  escala, tinte y reloj por diálogo sobre capa permanecen fuera de este corte.

### Reloj por avance de diálogo / Dialogue-advance clock (v0.8.3)

Cuando una transformación debe crecer con la historia y no con un cronómetro, la unidad
puede decirlo directamente:

```eclips
# Español
animacion tension escala a=0.95 en=8avances_dialogo
animar luna tension

# English — exactamente el mismo comportamiento
animation tension scale to=0.95 over=8dialogue_advances
animate luna tension
```

No hace falta saber programar para leerlo: la escala empieza en `1` (tamaño normal),
se acerca a `0.95` una vez por cada avance aceptado y llega exactamente al destino en
el octavo. Si escribís `de=`, empieza en ese valor; si lo omitís, usa la **identidad del
canal**, no el tamaño que casualmente tenga el personaje en ese instante.

**Qué cuenta como un avance:**

- Confirmar una línea de diálogo o narración cuyo texto ya está completo: **1**.
- Confirmar mientras el typewriter todavía escribe: **0**; solo completa el texto.
- Auto, skip e `{interrumpir}` cuentan cuando convergen en ese mismo avance aceptado.
- Choices, QTE, hotspots, comandos, escenas y el paso del tiempo: **0**.

Reglas de esta primera versión:

- Solo admite el canal `escala` / `scale` sobre **personajes**.
- Solo admite `modo=una_vez`; no convive con `periodo=` ni con `loop`/`vaiven`.
- La cantidad debe ser un entero positivo: `8avances_dialogo` es válido;
  `1.5avances_dialogo`, `8lineas` y `8 avances_dialogo` no lo son.
- `esperar_animacion` no espera a un track de diálogo: eso bloquearía precisamente el
  evento que necesita para avanzar. El motor avisa y continúa.
- El valor alcanzado se sostiene. `detener_animacion` lo retira; ocultar al personaje lo
  retira cuando la ocultación se confirma. Cambiar expresión, saltar de escena o
  encadenar otro script no lo borra por accidente.
- Guardar en el paso `k` y cargar restaura el mismo valor; el siguiente avance produce
  `k+1`.

### Intensidad de efectos por avances de diálogo (envolvente grupal)

La misma unidad `avances_dialogo` puede subir (o bajar) la **intensidad de los
efectos** de una receta, escalón por escalón:

```eclips
animacion tension estatica
animacion tension glitch
animacion tension intensidad_efectos de=0 a=1.5 en=8avances_dialogo
animar luna tension
```

`intensidad_efectos` (`effects_intensity` en inglés) es la **envolvente grupal**:
multiplica la intensidad de **todos** los efectos de esa receta a la vez, avanzando
una vez por cada avance de diálogo aceptado. La intensidad final de un efecto es:

    intensidad de línea × intensidad de la invocación × envolvente grupal

- `de=` es la identidad del multiplicador (**1.0**) si lo omitís; `a=` es obligatorio.
- Solo por avances de diálogo (los segundos quedan para más adelante) y solo
  `modo=una_vez`, igual que la escala.
- El **reloj interno** del efecto sigue en segundos: un glitch parpadea de forma
  continua; lo que sube por escalones es su *fuerza*.
- Sobrevive guardar/cargar exactamente en su paso `k`.

> Ojo con la lectura: el preset `glitch` casi no dibuja a intensidad baja (su
> parpadeo aparece en `I≥0.8` y sus rebanadas en `I≥1.5`), así que una envolvente
> que rampa desde `0` necesita rango suficiente —y una `curva=` que cruce esos
> umbrales— para verse crecer. La `estatica`, en cambio, se ve a cualquier intensidad.

### Líneas de efecto en la receta

Cualquier primitiva de render (`franja`, `tajadas`, `parpadeo`, `bloques`,
`estatica`, `sustituto` — y el preset `glitch`) entra a la receta con sus mismos
parámetros de siempre. **El driver NO va en la receta**: la receta dice *qué*,
el guion dice *cuándo* — el driver viaja en el `animar`:

```
animar sol corrupcion en=1                          # UNA rotura de 1s
animar sol corrupcion media=3 varianza=2 frames=12  # esporádica
animar sol corrupcion intensidad=2                  # más fuerte (multiplica)
```

**La receta esporádica pulsa JUNTA**: todas sus líneas disparan en la MISMA
ventana (comparten sorteo), incluso después de guardar y cargar. La `corrupcion`
es *un* evento, no dos parpadeos sueltos.

> **Ojo del terror fino:** dentro de una ventana abierta, `tajadas` re-sortea sus
> rebanadas **por frame** — con `proporcion=1/3` (default) hay frames donde ninguna
> rebanada se rompe, y la rotura puede *leerse* como "franja sola". No es
> desincronización: es la naturaleza binomial de la primitiva. Si querés que la
> rotura lea como UN bloque sólido, subí `proporcion=` (0.6+) o `frames=`.

### Blancos, apilado y detener

- Los **canales** corren sobre personajes y `pantalla` (sobre fondo/capas siguen
  siendo de `capa_mover`/`capa_fade` — el motor avisa). Los **efectos** corren
  sobre personajes, `fondo` y capas de imagen (sobre `pantalla` completa: aún no —
  avisa).
- Recetas con nombres **distintos se apilan** (componen por el álgebra de siempre);
  re-`animar` el **mismo nombre reinicia** la instancia entera.
- `detener_animacion <alias> <receta>` baja **el conjunto** (canales y efectos).
  `detener_animacion <alias> <primitiva>` sigue existiendo para cirugía fina —
  puede bajar UNA línea de una receta aplicada.

> ✅ **Guardar y cargar:** las animaciones nombradas sobreviven **resueltas** — no
> hace falta que la definición se re-lea al cargar. Un `una_vez` terminado vuelve
> **en su estado final** (el logo que cayó sigue donde cayó), los `loop`/`vaiven`
> siguen respirando, un track por avances vuelve en su paso exacto `k`, y `detener` por
> nombre **sigue funcionando** después de cargar.

### Ejemplos

```
# La caída de un logo, como receta (sobre un personaje-logo):
animacion caida_logo mover_y de=-300 a=0 en=1.2 curva=rebote
animacion caida_logo alpha de=0 a=1 en=0.4 curva=suave
animar logo caida_logo
esperar_animacion                # el guion ESPERA a que termine de caer

# "Poseída": flota Y tiene el color corrido, un solo nombre:
animacion poseida mover_y amplitud=6 periodo=0.8 modo=vaiven
animacion poseida franja separacion=5
animar sol poseida
...
detener_animacion sol poseida
```

---

### Ejemplos

```
# Vibración de susto
animar luna temblar 0.5s

# Personaje flotando (loop)
animar luna flotar
esperar 3.0
detener_animacion luna

# Flash blanco en toda la pantalla (deslumbra)
animar pantalla destello 0.5s

# Glitch sutil
animar luna glitch intensidad=0.3

# Glitch catastrófico
animar luna glitch intensidad=2.5

# Glitch de susto (puntual, medio segundo)
animar luna glitch 0.5s intensidad=2.0

# Sprite baja con tristeza
animar luna caer 0.3s
luna: "Me siento mal..."
detener_animacion luna

# Brinquito de emoción
animar luna brincar

# Acercarse + retroceder (el susto de cerca)
animar luna acercar 0.3s
luna: "Estoy muy cerca..."
animar luna retroceder
```

### Intensidad del Glitch

La intensidad controla el nivel de "catástrofe" visual:

| Intensidad | Efecto |
|------------|--------|
| 0.1 - 0.5 | Separación RGB sutil, "algo está mal" |
| 0.8 - 1.0 | RGB + parpadeo aleatorio, "el juego falló" |
| 1.5 - 3.0 | RGB + parpadeo + cortes horizontales, terror puro |

### Detener animación

```
detener_animacion luna
stop_animation luna
```

Los loops (`flotar`, `pulso`, `vaiven`) y los persistentes (`caer`, `subir`, `acercar`) tienen **aterrizaje suave** — el sprite vuelve a su posición natural de forma gradual en vez de cortarse de golpe.

Los **efectos de render** (`franja`, `tajadas`, `parpadeo`, `glitch`) se cortan **en seco**. Es a propósito: una corrupción que se desvanece con elegancia no da miedo.

### Esperar animación

```
animar luna temblar 0.5s
esperar_animacion

animate luna shake 0.5s
wait_animation
```

Pausa la ejecución del script hasta que termine una animación temporal. Solo es útil
para efectos puntuales (los loops nunca terminan). Los tracks por avances de diálogo se
excluyen con aviso: esperar por ellos impediría el click que debe hacerlos avanzar.

### Esperar por tiempo / Wait by time

```
l: "Vamos al club."
esperar 1.5
l: "...todos están esperando."

l: "Let's go to the club."
wait 1.5
l: "...everyone is waiting."
```

Pausa la ejecución del script por N segundos antes de continuar al siguiente comando. Útil para ritmo narrativo entre líneas: silencios dramáticos, transiciones lentas, dejar respirar la escena.

#### Interacción con modos del jugador

| Modo | Comportamiento |
|------|----------------|
| Normal | Espera los N segundos completos. |
| Auto | Espera los N segundos completos. El ritmo narrativo del creador se respeta incluso en auto. |
| Skip | Atraviesa el `esperar` inmediatamente. Skip cancela el ritmo del creador. |

#### Notas

- `esperar` solo acepta segundos como float (`esperar 0.5`, `esperar 2.0`). No segundos+ms.
- No conflicta con `esperar_click` ni `esperar_animacion`: cada uno tiene su semántica propia.
- Para pausas dentro de una línea de diálogo (no entre líneas), usar el tag inline `{pausa=N}` (ver sección 7 — Efectos de texto).

### Esperar interfaz / Wait for interface

```
esperar_interfaz
wait_ui
```

Entra en una fase explícita de espera de interfaz: no muestra diálogo ni acepta
duración, segundos o temporizador. Mantiene vivos y focables los enlaces,
controles, ranuras y widgets de tecla de la interfaz. Solo sale por navegación,
continuación o cierre.

Las dos grafías son bilingües y producen la misma fase. `esperar_interfaz_x` no
es una extensión válida: el parser exige la frontera exacta de la palabra y
deja esa línea sin reconocer.

### Save/Load

Las animaciones persistentes (zoom_in, drop, rise) y loops (float, pulse, glitch sin duración) se guardan automáticamente. Al cargar, los loops reinician su ciclo desde 0 y los persistentes aparecen ya en su posición final. Los efectos puntuales (shake, bounce, hop, etc.) no se guardan.

### Notas técnicas

- Las animaciones APILAN por personaje (desde v0.7.3): re-emitir el mismo tipo/nombre reemplaza ESA entrada; tipos distintos componen (offsets suman, factores multiplican). `detener_animacion <alias>` baja todo; con nombre, baja solo esa.
- Los efectos funcionan tanto con personajes normales (PNG) como compuestos (body+face, 3 capas).
- `lean_back`/`retroceder` captura el zoom actual del personaje — úsalo después de `zoom_in`/`acercar`.
- Los efectos de pantalla (`pantalla`/`screen`) afectan toda la imagen. Screen flash cubre incluyendo caja de diálogo y UI.

---

## 16. Integración Lua / Lua Integration

EclipScript puede ejecutar scripts Lua para lógica compleja que va más allá de variables y condicionales simples.

EclipScript can execute Lua scripts for complex logic beyond simple variables and conditionals.

### Ejecutar script Lua

```
lua "scripts/logica.lua"
ejecutar_lua "scripts/logica.lua"
```

Ejecuta el archivo Lua indicado. El script Lua tiene acceso a bindings del motor para leer/modificar variables, controlar personajes, y más.

### Notas

- Los scripts Lua se ejecutan síncronamente — el VN espera a que terminen
- Las variables EclipScript son accesibles desde Lua y viceversa
- El estado Lua se preserva en save/load
- Para documentación completa de los bindings Lua disponibles, consultar la documentación de LuaManager

### Efectos de render en Lua / Lua render effects (`efecto_lua`, v0.8.2)

Un **efecto Lua** es un transformador de pasadas escrito por el creador: se aplica con `animar`, convive con las primitivas (`franja`, `tajadas`...) en la misma tubería y en las recetas del bloque `animacion`, acepta los drivers (`en=`, `media=`/`varianza=`/`frames=`/`desde=`), se detiene por nombre y sobrevive al save.

**Dos pasos: declarar en el `.ecl`, registrar en el `.lua`.**

```
# En el .ecl — FUERA de escena, antes de usarse (orden de archivo,
# como `personaje` y `animacion`). El nombre va sin comillas; la ruta
# con comillas, relativa al script.
efecto_lua testigo "fx/testigo.lua"

escena inicio
    # ...
    animar sol testigo textura="assets/caras/otra.png" media=40 varianza=15 frames=2
    # ...
    detener_animacion sol testigo
```

```lua
-- En el .lua — corre al ARRANCAR el guion (no al llegar a una escena):
eclips.register_effect("testigo", function(ctx, params, pasadas)
    -- ctx (solo lectura):
    --   ctx.t          segundos desde que el efecto se aplicó
    --   ctx.width      ancho proyectado del elemento (px virtuales)
    --   ctx.height     alto proyectado
    --   ctx.intensity  la clave universal intensidad= (default 1)
    --   ctx.typing     true mientras el typewriter revela texto
    -- params: los key=value de la línea `animar` (o de la línea de
    --   receta), como STRINGS — usar tonumber() para números.
    -- pasadas: la lista de pasadas (secuencia 1..N, sin huecos).
    --   Cada pasada es una tabla; mutarla, agregar o quitar entradas
    --   ES el efecto. Claves (identidad si no se tocan):
    --   u0,v0,u1,v1 (sub-rect de muestreo) · dx,dy (offset px) ·
    --   sx,sy (escala al centro) · r,g,b (tinte ×) · alpha ·
    --   texture ("" = el elemento; ruta = otra textura) ·
    --   blend ("alpha" | "additive" | "multiply") ·
    --   dest (bool) + dest_x/dest_y/dest_w/dest_h (región destino)
    --   mask (bool) recorta la pasada a la SILUETA del elemento
end)
```

**Las leyes:**

- **`mask` — el recorte a la silueta.** Una pasada que trae **su propia textura** normalmente ya se recorta sola (la transparencia de tu PNG hace el trabajo). Pero si tu textura es **opaca de borde a borde** y la estampas sobre el elemento entero, vas a pintar un **rectángulo**: el sprite del personaje tiene padding transparente alrededor, y ahí no hay nada que te recorte. `mask = true` multiplica el alpha de tu pasada por el del elemento → tu efecto queda **dentro de la silueta**. Es lo que hace `estatica` con su ruido. Sobre un elemento opaco (un `fondo`) no cambia nada, así que es seguro dejarlo puesto. **Ojo:** si tu efecto lee la lista y la devuelve entera, conserva la clave — una pasada reconstruida sin `mask` pierde el recorte y el cuadrado reaparece.

- **El nombre se resuelve al parsear** (typo en `animar` → aviso con línea, como siempre); **la función se resuelve al correr** (declarado pero sin `register_effect` → aviso, no se aplica).
- Los archivos declarados corren **al boot** del guion — por eso un save con un efecto Lua vivo **lo revive al cargar**, con su reloj (`esporadico` incluido). Si al cargar la declaración ya no existe, la entrada se descarta con aviso.
- **Un error en el efecto jamás crashea**: aviso en consola con el traceback Lua + el efecto se remueve solo.
- Los params son **claves libres** (el vocabulario es tuyo), pero las claves de la tabla común conservan su rango (`alpha=` sigue siendo un factor 0-1) y las claves de driver (`en`, `media`, `varianza`, `frames`, `desde`) son del motor: el efecto nunca las ve. `escalada=` no está reservada; la envolvente grupal se declara explícitamente como `intensidad_efectos` dentro de una receta.
- Un efecto Lua **continuo** mantiene el render despierto (el motor no puede saber si anima); bajo `esporadico`, dormido concede quietud — el camino barato, como las primitivas.
- La API Lua es en **inglés** (la convención de los bindings); el `.ecl` sigue bilingüe.
- **La especie correcta para cada cosa**: mover/escalar/desvanecer un personaje es trabajo de los CANALES (`animacion` + `mover_x`/`escala`/`alpha` — componen por álgebra, aterrizan bien y quiescen fino). El efecto Lua es para lo que los canales no expresan: sustituciones de textura, lógica condicional (`ctx.typing`), progresiones custom sobre `ctx.t`. Una pasada TIENE `dx/sx` y puedes moverla desde Lua — pero esa copia no compone con el álgebra de canales ni aterriza suave: úsalo cuando el movimiento sea parte de la conducta del efecto, no como reemplazo de `animar`.

**El primer ejemplo de receta es El Testigo** (`fx/testigo.lua`, el del ejemplo de arriba; su `.lua` todavía no viene en la copia pública) — otra expresión que aparece un parpadeo, agendada por `esporadico`. Params: `textura=` (obligatoria), `alpha=`, `reemplazar=1` (posesión: para sprites completos; el default estampa-encima es para caras-PNG con transparencia), `solo_tecleando=1` (solo aparece mientras el jugador lee — la ventana que cae fuera se pierde: negación plausible gratis).

### Hooks Lua / Lua Hooks

El creador puede definir estas funciones en su Lua — todas opcionales (si no existen, el motor las ignora en silencio):

| Hook | Cuándo se llama | Detalle |
|------|----------------|---------|
| `on_scene_enter(scene)` | Al entrar a una escena | recibe el nombre de la escena |
| `on_choice_made(label, target)` | Al elegir una opción | recibe texto y destino |
| `on_variable_changed(var, old, new_val)` | Al cambiar una variable | nombre, valor viejo, valor nuevo |
| `on_save()` | Al guardar partida | **retorna un string** con el estado Lua a persistir |
| `on_load(data)` | Al cargar partida | recibe ese string para restaurar |

> ✅ `on_save()` **puede retornar varias líneas.** Desde la versión 2 del formato de
> save, los saltos de línea (y el `%`) se escapan al escribir y se restauran al leer:
> un `table` serializado, un JSON a mano, un `os.date()` — todo viaja intacto. Lo que
> devuelvas es exactamente lo que `on_load(data)` recibe, byte por byte.
>
> *(Los saves creados antes de esta versión se siguen cargando sin tocarlos. Ver 16.5.)*

---

## 16.5 Tus saves por dentro / Inside Your Saves

Los saves son **texto plano UTF-8**: abrilos con el Bloc de notas cuando quieras. Viven en `saves/`:

| Archivo | Qué guarda |
|---------|-----------|
| `vn_saves.eclips` | las ranuras de partida |
| `meta.eclips` | preferencias del jugador (volumen, resolución...) |
| `seen.eclips` | qué diálogos ya se leyeron (para el *skip*) |

Formato: `clave=valor`, una por línea, con las claves ordenadas alfabéticamente.

### La cabecera de versión

La primera línea es el **piso del archivo**: `[Eclips Save v2]`.

- **v2** — los valores viajan escapados: `%` → `%25`, salto de línea → `%0A`, retorno → `%0D`.
  Por eso `on_save()` puede devolver multilínea. Si editás a mano y querés un `%` literal, escribí `%25`.
- **v1** o sin cabecera — saves viejos, sin escape. **Se siguen cargando tal cual.**

### Los sellos de cada ranura

Cada ranura lleva **dos sellos independientes**:

| Sello | Qué versiona |
|-------|-------------|
| `slot_N_ver` | la gramática de la mitad **VN** |
| `slot_N_gp_ver` | la gramática de la mitad **gameplay** |

El writer actual usa **VN v4** y **gameplay v2**. VN v4 conserva también el dominio de
reloj y el progreso `k` de las animaciones; sus lectores v1-v3 siguen presentes.

Un save escrito por una versión **más nueva** del motor no se carga a medias: la ranura
aparece en el menú como **"Ranura incompatible (formato más nuevo)"** y se niega a abrirse.
Preferimos decirte la verdad antes que cargarte basura.

### Lo que el motor te promete

- **Cargar nunca reescribe.** Un save viejo sigue siendo viejo hasta que vos guardes encima.
- **Los saves viejos no se rompen.** Cada versión del formato conserva su lector.
- El `preview` de la ranura son los **primeros 50 caracteres** del último diálogo — una `é` cuenta 1, no 2.

---

## 17. Controles del Jugador / Player Controls

> **Todo lo de esta sección son las teclas DE FÁBRICA.** Desde v0.8.33 vos las cambiás con el bloque `teclas {}` de tu `proyecto.eclips`, y desde v0.8.34 tu jugador las cambia con `tecla.*` en su `settings.eclips` (§27) — así que esta tabla describe el punto de partida, no lo que el jugador tiene en la mano. Si querés escribir una tecla **en el texto de tu juego**, no la tipees: usá `${_tecla.vn.avanzar}` (§24), que sigue al remapeo.

### Teclado

| Tecla | Acción |
|-------|--------|
| `Space` / `Enter` / Click izquierdo | Avanzar diálogo / completar typewriter |
| `1` `2` `3` `4` | Elegir opción |
| Rueda arriba / `Page Up` | Abrir historial de diálogos |
| Rueda / `PgUp` / `PgDn` | Navegar historial |
| `ESC` / Click / Rueda al final | Cerrar historial / Abrir menú de pausa |
| `A` | Toggle Auto-mode (avanza solo tras 2s) |
| `Ctrl` (mantener) | Skip (avance rápido, hold) |
| `K` | Quick Save (slot 0) |
| `L` | Quick Load (slot 0) |
| `F5` | Menú Guardar |
| `F9` | Menú Cargar |

### Teclado del gameplay 2D

> ⚠ **La palabra de `gameplay` (`topdown` / `sidescroll`) no elige el modo.** El motor sólo lee
> la ruta del mapa; el modo lo decide la clave `modo:` del `.eclmap`. La palabra se lee y sale
> en el log (`[EclipScript]   Gameplay: modo=…`), nada más: `gameplay sidescroll "paseo.eclmap"`
> con `modo: topdown` en el mapa se juega en vista cenital, y `gameplay "paseo.eclmap"`, sin
> palabra, también funciona.

Adentro de un `gameplay topdown` / `gameplay sidescroll`. **Varias teclas significan
cosas distintas según el modo, y el motor decide por el modo del mapa — no por la
tecla:**

| Tecla | Top-down | Side-scroll |
|-------|----------|-------------|
| `A` `D` / `←` `→` | Mover | Mover |
| `W` `S` / `↑` `↓` | Mover | **nada** (no hay eje vertical) |
| `Space` | **Atacar** | **Saltar** (doble salto y wall-jump si el mapa los tiene) |
| `F` | **Atacar** | Atacar / disparar |
| `Shift` (izq. o der.) | — | Dash |
| `E` / `Enter` | Hablar con el NPC cercano | Hablar con el NPC cercano |
| `ESC` | Cancelar modo construcción; si no, menú de pausa | Menú de pausa |

- Atacar necesita `puede_atacar: true` en el bloque `jugador:` del `.eclmap`; el dash
  necesita `dash: true`, y el doble salto `doble_salto: true`.
- Estando al lado de un NPC, `Space` **no** abre su diálogo: ataca o salta según el
  modo. Para hablar se usa `E` o `Enter`.
- Toda tecla que el gameplay **no** consume sigue disparando el hook `key_press` de Lua.
- Y desde v0.8.35 hay un segundo canal, `on("action")`, que entrega **acciones** en
  vez de teclas y por eso sobrevive al remapeo del jugador. Los dos conviven: uno
  sirve para *qué quiso hacer*, el otro para *qué tecla tocó*. Detalle en
  `VOCABULARIO_2D_ECLIPMAP.md`, sección «Entrada: acciones, no teclas» (el manual del 2D, que viene
  junto a éste).

### Botones UI en caja de diálogo

Durante el diálogo (estado WaitingInput), aparecen 6 botones clickeables alineados a la derecha en la parte inferior de la caja de diálogo:

| Botón | Acción |
|-------|--------|
| **AUTO** | Toggle auto-mode (se ilumina en verde cuando está activo) |
| **SKIP** | Toggle skip mode (se ilumina en verde cuando está activo) |
| **SAVE** | Abre menú de guardar |
| **LOAD** | Abre menú de cargar |
| **LOG** | Abre historial de diálogos |
| **MENU** | Abre menú de pausa (Resume, Settings, Save, Load, Main Menu, Quit) |

- Los botones desaparecen durante choices y QTEs (estándar de VN)
- Hacer click en un botón **no** avanza el diálogo
- SKIP por botón funciona como toggle on/off (en touch no hay "mantener tecla")
- AUTO y SKIP son mutuamente exclusivos
- MENU desactiva AUTO y SKIP al abrirse
- Los botones tienen hover visual (cambio de color/opacidad al pasar el mouse)

---

## 18. Sistema de Guardado / Save System

- **5 slots** + quick save/load
- Guarda: escena actual, línea de comando, fondo, música, personajes activos (con expresión, posición, modo compuesto y partes), variables, estado de speaker highlight, animaciones persistentes y loops
- Preview: primeros 50 caracteres del último diálogo
- Timestamp legible (DD/MM/YYYY HH:MM)
- Las variables del script se preservan completamente
- Los personajes compuestos guardan su modo (2 capas, 3 capas, o completo) y se restauran correctamente
- Animaciones loop (float, pulse, glitch) y persistentes (zoom_in, drop, rise) se guardan y restauran automáticamente
- Saves antiguos son retrocompatibles con saves nuevos

---

## 19. Ejemplo Completo / Complete Example

```ecl
# ── Demo bilingüe de EclipScript ──

character luna "assets/characters/luna.png"

scene intro

background "assets/backgrounds/classroom.png"
music "assets/audio/theme.ogg"
show luna center
highlight_speaker yes

set points = 0

luna: "Welcome to the Literature Club."
luna happy: "We write, we read... and sometimes weird things happen."

"A strange silence filled the room."

choose:
  "I want to join"  -> route_join
  "I'm leaving"     -> route_leave

scene route_join

set points + 2
sound "assets/audio/bell.wav"
luna happy: "That's great!"
animate luna hop

qte [key: E, time: 3s]:
  success -> qte_win
  fail -> qte_lose

scene qte_win
set points + 1
luna happy: "Nice reflexes!"
goto epilogue

scene qte_lose
luna sad: "Ouch..."
animate luna drop 0.3s
goto epilogue

scene route_leave
luna sad: "...Oh. Okay."
goto epilogue

scene epilogue

transition dissolve
background "assets/backgrounds/sunset.png"

if points >= 3
  luna happy: "I feel like we really connected today."
else
  luna: "See you around..."
end_if

hide luna
stop_music

"End of demo."
end
```

---

## 20. Referencia Rápida / Quick Reference

> Las formas **más usadas** — no es exhaustiva. El inventario completo de cada
> comando vive en sus secciones de arriba.

```
# Estructura
escena/scene nombre
fin/end

# Personajes
personaje/character alias "ruta"
personaje_compuesto/composite_character alias "carpeta/"
mostrar/show alias [expresion] [posicion] [efecto: fade|instant|slide]
ocultar/hide alias [efecto: fade|instant|slide]

# Media
fondo/background "ruta"
musica/music "ruta"
sonido/sound "ruta"
parar_musica/stop_music
transicion/transition fade|dissolve|slide_izquierda|slide_derecha

# Diálogo
alias: "texto"
alias expresion: "texto"
"texto narrador"

# Text effects (inline en diálogos)
{wavy}/{ondulado}         — texto ondulante
{shake}/{temblar}         — texto tembloroso
{glitch}                  — tramo corrupto (revoltijo + franja R/B)
{speed=N}/{velocidad=N}   — velocidad del typewriter
{/tag}                    — cierre (opcional)

# Variables
set var = valor                 # o `mutar var = valor`
set var + n
set var - n
-> var + n                      # como destino de un click: muta y sigue
meta_set var = valor            # persiste entre partidas (meta.eclips)
meta_borrar/meta_reset          # vacía meta.eclips

# Condicionales
si/if var op valor
sino/else
fin_si/end_if

# Opciones
elegir/choose [tiempo/timer/time: Xs, defecto/default: "texto de una opcion"]:
  "opcion" -> escena

# QTE
qte [tecla/key: X, tiempo/time: Xs]:
  exito/success -> escena
  fallo/fail -> escena

# Navegación
ir_a/goto escena
cargar_script/load_script "ruta.ecl" [escena]

# Animaciones
animar/animate objetivo efecto [Ns] [intensidad=N]
animacion/animation nombre escala/scale a=/to=V en=/over=Navances_dialogo/dialogue_advances
detener_animacion/stop_animation objetivo
esperar_animacion/wait_animation
esperar/wait N

# Efectos: temblar/shake, saltar/bounce, acercar/zoom_in,
#          alejar/zoom_out, flotar/float, pulso/pulse,
#          destello/tint_flash, glitch,
#          caer/drop, subir/rise, brincar/hop, retroceder/lean_back

# Capas
capa/layer nombre "ruta" z=N x=N y=N w=N h=N [parallax=N]
capa_z/layer_z nombre N
capa_visible/layer_visible nombre si/no
capa_fade/layer_fade nombre aparecer|desaparecer N
capa_mover/layer_move nombre a (x,y) en N [una_vez|loop] [curva]

# Texto-capa
texto/text id "contenido" x=N y=N color=#RGB z=N

# Hotspots
hotspot nombre x=N y=N w=N h=N
esperar_click/wait_click:
  "nombre" -> escena

# Eventos random
evento_random/random_event id cada/every=N probabilidad/chance=N cooldown=N -> escena
iniciar_eventos/start_events
detener_eventos/stop_events
azar_semilla/random_seed [N]
azar_traza/random_trace si/no

# Efectos visuales
parallax si/no
resaltar_hablante/highlight_speaker si/no

# Caja de diálogo
ocultar_caja/hide_box
mostrar_caja/show_box
usar_caja/use_box plantilla

# Cinemática (bloque con llaves — sella el input mientras dura)
cinematica/cinematic [saltable/skippable=si|no] {
  ...
}

# Lua
lua/ejecutar_lua "ruta.lua"
efecto_lua/lua_effect <nombre> "ruta.lua"   — declara efecto de render Lua
```

---

## 21. Capas Z-Order / Z-Order Layers

Las capas permiten colocar imágenes en pantalla con profundidad controlable. Cada capa tiene un nombre, una textura, y un valor `z` que determina el orden de dibujo (z bajo = atrás, z alto = frente).

Layers allow placing images on screen with controllable depth. Each layer has a name, a texture, and a `z` value that determines draw order (low z = back, high z = front).

### Agregar capa / Add layer

```
capa fondo "assets/club/fondo.png" z=0 x=0 y=0 w=1920 h=1080
capa mesa "assets/club/mesa.png" z=100 x=200 y=600 w=1520 h=400
capa luna "assets/club/luna.png" z=300 x=700 y=100 w=520 h=800
capa pizarra "assets/club/pizarra.png" z=50 x=600 y=300 w=720 h=500
```

English:
```
layer background "assets/club/bg.png" z=0 x=0 y=0 w=1920 h=1080
layer desk "assets/club/desk.png" z=100 x=200 y=600 w=1520 h=400
```

| Parámetro | Descripción |
|-----------|-------------|
| `z=N` | Profundidad (bajo = atrás, alto = frente) |
| `x=N` | Posición horizontal en coordenadas virtuales (1920px) |
| `y=N` | Posición vertical en coordenadas virtuales (1080px) |
| `w=N` | Ancho en coordenadas virtuales |
| `h=N` | Alto en coordenadas virtuales |

El orden de los parámetros no importa: `z=100 x=200` es igual que `x=200 z=100`.

**NOTA:**  El id va sin comillas (como `texto`/`enlace`/`hotspot` — y como **todos** los comandos que llevan id). Escribirlo entre comillas (`capa "mesa" "ruta"`) ya **no** se acepta: el motor avisa y descarta la línea. Hasta v0.8.6 esa forma vieja funcionaba avisando que migraras; desde v0.8.7 la deprecación está cobrada.

> **La regla, en dos renglones.** La comilla dice **"esto es un literal"**: texto que se ve, o una ruta de archivo. Todo lo demás —los ids que vos inventás, las palabras del motor, los valores de parámetro— va **desnudo**.
> Los **sinónimos** sí se aceptan (`ancho=` y `width=` son la misma cosa: gana la primera y el motor te avisa); las **comillas de más no**, porque no dicen lo mismo de otra manera: **dicen otra cosa.**

### Una capa de COLOR SÓLIDO / A solid-color layer *(v0.8.21)*

Donde va la ruta, podés poner un **color**. La capa deja de ser una imagen y pasa a ser un **rectángulo sólido** de ese color.

Where the path goes, you can put a **color** instead. The layer becomes a **solid rectangle**.

```
capa oscuridad "#000000" z=999 x=0 y=0 w=1920 h=1080
capa aviso "#FFFFFF" z=500 x=0 y=0 w=1920 h=1080
capa banda "#00FF88" z=700 x=0 y=440 w=1920 h=200
```

No hace falta ningún PNG. El color acepta las dos formas de siempre: **`#RRGGBB`** y **`#RRGGBBAA`** con alpha opcional.

```
capa velo "#FF000060" z=800 x=0 y=0 w=1920 h=1080   # rojo translúcido
```

**Y hereda todo lo demás gratis**, porque una capa de color es una capa como cualquier otra: `capa_fade`, `capa_visible`, `capa_mover`, `capa_z`, `parallax=` y `tinte=` funcionan igual.

```
# "Negro puro... abro los ojos y ahí está el fondo."
fondo "assets/club/cuarto.png"
capa oscuridad "#000000" z=999 x=0 y=0 w=1920 h=1080
"..."
capa_fade oscuridad desaparecer 3s
```

> **⚠ El color va SIEMPRE entre comillas.** `capa x #000000` **no funciona** — y no es un capricho del comando: `#` fuera de comillas es el **comentario**, así que el motor leería `capa x` y tiraría el resto de la línea. Adentro de las comillas el `#` está a salvo.
>
> ```
> capa x #000000        # ✗ el motor lee `capa x`. El resto es comentario.
> capa x "#000000"      # ✓
> ```

**Si el color está mal escrito, la línea se ignora y el motor te lo dice como color** — no como una imagen que no carga:

```
capa rota "#GG0000" z=1
```
```
[EclipScript] AVISO linea 12: capa: '#GG0000' no es un color valido
              (usa #RRGGBB o #RRGGBBAA). La linea se ignora.
```

**Una ruta con `#` adentro sigue siendo una ruta.** Lo que decide es que el `#` esté **al principio**:

```
capa cuadro "assets/raro#2.png" z=50 x=0 y=0 w=300 h=200   # ✓ es una imagen
```

**Diferencia con `tinte=`:** el color dice **qué ES** la capa; `tinte=` es un filtro **encima**. Sobre una capa de color, `tinte=` multiplica igual que sobre una textura.

**Se guarda con la partida** *(v0.9.0)*: el color, la posición, el tamaño, la `z`, el `parallax=`, si está visible y el `tinte=`. Si guardás mientras la capa se desvanece (`capa_fade … desaparecer`), al cargar vuelve ya oculta, que es como iba a quedar.

Lo que **todavía no** se guarda: la opacidad de un `capa_fade` a medias (una capa que estaba apareciendo vuelve entera) y los `texto` que estaban en pantalla, que no vuelven al cargar.

> **Ojo con lo que una capa NO puede tapar:** la **caja de diálogo** se dibuja siempre después de todas las capas, así que ni con `z=999` queda por debajo del color. Y los **personajes** tampoco: se dibujan **siempre después de todas las capas**, así que ninguna capa —ni con `z=999`— queda encima de un personaje. Para llevar la pantalla a negro con personajes a la vista, apagalos a ellos (una `animacion` de `alpha`) mientras la capa negra aparece. Entre ellos, los personajes se dibujan en el orden en que entraron a escena: el último que entró queda encima.

### Re-emitir una capa que ya existe (actualizar) / Re-emitting an existing layer

Si escribís un `capa` con un id que **ya está en pantalla**, no se crea otra: se
**actualiza la que existe**. Y la regla es sencilla:

> **Una re-emisión cambia SOLO lo que la línea nombra. Lo que no nombrás, se queda.**

```
capa placa "noche.png" x=300 y=200 w=500 h=300 z=20 tinte=#334488
capa placa "dia.png"                 # ← solo cambia la imagen
```

La segunda línea cambia la textura y **conserva** posición, tamaño, z y tinte. Si
después querés moverla, nombrás solo eso:

```
capa placa "dia.png" x=700          # se mueve; el tamaño, el z y el tinte NO se tocan
```

Es la misma regla que `mostrar <alias> <posicion>` sobre un personaje ya visible:
mover no le borra la expresión. La primera vez (cuando la capa **no** existía) sí se
rellenan los valores por defecto que no pusiste — eso es *crear*; una re-emisión es
*actualizar*.

**Un detalle honesto:** como omitir un parámetro ahora significa "dejalo como está",
**no hay forma de QUITAR el tinte** re-emitiendo sin él (antes sí, por accidente).
Si necesitás una capa con tinte y después la misma sin él, hoy conviene dos ids
distintos; una grafía para quitarlo (`tinte=ninguno`) llegará en una versión futura.

Lo mismo vale para `texto <id> ...` re-emitido: cambiar el contenido no le devuelve
la fuente, el color ni el glow al default — solo cambia lo que nombraste.

### Cambiar z-order en runtime

```
capa_z pizarra 250
```

Mueve la pizarra al frente de la mesa (z=100) pero detrás de Luna (z=300).

English: `layer_z blackboard 250`

### Mostrar / ocultar capa

```
capa_visible pizarra si
capa_visible pizarra no
```

English: `layer_visible blackboard yes` / `layer_visible blackboard no`

Valores aceptados: `si`, `no`, `yes`, `no`, `true`, `false`, `on`, `off`, `1`, `0`

### Nota

Las capas coexisten con el sistema clásico de personajes (`mostrar`/`ocultar`). Los personajes clásicos se dibujan encima de todas las capas. Puedes usar capas para fondos complejos y `mostrar` para personajes de VN tradicional, o usar solo capas para escenas más complejas.

---

## 22. Hotspots Clickeables / Clickable Hotspots

Los hotspots son zonas invisibles en pantalla que detectan clicks del mouse. Se combinan con `esperar_click` para crear escenas interactivas estilo point-and-click.

Hotspots are invisible screen zones that detect mouse clicks. They combine with `wait_click` to create point-and-click interactive scenes.

### Definir hotspots

```
hotspot carta x=350 y=800 w=150 h=120
hotspot libro x=1400 y=750 w=160 h=130
hotspot engranaje x=100 y=50 w=80 h=80
```

| Parámetro | Descripción |
|-----------|-------------|
| `x=N` | Posición X del rectángulo |
| `y=N` | Posición Y del rectángulo |
| `w=N` | Ancho del rectángulo |
| `h=N` | Alto del rectángulo |

### Esperar click / Wait for click

```
esperar_click:
  "carta" -> escena_carta
  "libro" -> escena_libro
  "engranaje" -> escena_config
```

English:
```
wait_click:
  "letter" -> scene_letter
  "book" -> scene_book
```

El motor se detiene en `esperar_click` y espera a que el jugador haga click en uno de los hotspots definidos. Cuando detecta un click en una zona mapeada, salta a la escena indicada.

Si una entrada nombra un id que no existe en los hotspots vivos, el motor la
ignora y emite una voz reparable con el id, destino, linea del header y la
forma `hotspot <id> x= y= w= h=`. La ausencia se informa aunque la entrada
tenga un gate `[si:]`; un id ausente no se agrega a la espera. Despues de
evaluar todos los gates, solo quedan mappings cuyo id conserva al menos un
hotspot habilitado. Si no queda ninguna salida efectiva, la fase sigue en
`WaitingClick`, consume el input y avisa el soft-lock: no inventa un destino ni
avanza por su cuenta. Al cargar una partida, un mapping persistido sin hotspot
restaurado se descarta con voz; si no sobrevive ninguno, el comando vivo se
reejecuta sin cambiar el formato del save.

### Outline de hover / Hover outline

A diferencia del debug de F3 (que muestra **todos** los hotspots, herramienta de desarrollo), puedes darle al jugador un **contorno sutil** que aparece solo en el hotspot bajo el mouse. Es un visual de tema **global** (no por caja de diálogo) y está **apagado por defecto** — los hotspots suelen ser exploratorios, y un contorno siempre visible los delataría.

| Key (ES) | Key (EN) | Qué hace |
|----------|----------|----------|
| `hotspot_hover_color` | `hotspot_hover_color` | Color del contorno (`#RRGGBBAA`). Alpha 0 = apagado (default) |
| `hotspot_hover_grosor` | `hotspot_hover_thickness` | Grosor del contorno en píxeles |

en el tema (.eclips), a nivel raíz:

```
hotspot_hover_color   #a692cd
hotspot_hover_grosor  3
```

- Va a **nivel de tema** (fuera de cualquier `caja_dialogo`). Si lo pones dentro de un bloque de caja, el motor te avisa que es una key global.
- Solo se dibuja sobre hotspots **habilitados**: uno deshabilitado por un gate `[si:]` no muestra contorno aunque pases el mouse.
- Sin la key (o con alpha 0) no se dibuja nada — exploración pura.
- `hotspot` es un término técnico adoptado; no tiene par en español (igual que `glow` o `fade`).

### Debug

Presiona **F3** durante el juego para ver los rectángulos de los hotspots:
- **Verde** = habilitado
- **Amarillo** = mouse encima (hover)
- **Rojo** = deshabilitado

---

## 23. Parallax por Mouse / Mouse Parallax

El parallax es un efecto visual donde las capas se mueven a diferentes velocidades al mover el mouse, creando sensación de profundidad.

Parallax is a visual effect where layers move at different speeds as the mouse moves, creating a sense of depth.

### Uso

Se añade como parámetro a cualquier capa:

```
capa fondo "assets/bg.png"     z=0   x=0 y=0 w=1920 h=1080 parallax=0.2
capa mesa  "assets/mesa.png"   z=100 x=200 y=600 w=1520 h=400 parallax=0.5
capa luna  "assets/luna.png"   z=300 x=700 y=100 w=520 h=800 parallax=0.8
```

| Valor | Efecto |
|-------|--------|
| `parallax=0`   | La capa no se mueve (default si no se especifica) |
| `parallax=0.2` | Movimiento sutil (fondos lejanos) |
| `parallax=0.5` | Movimiento medio |
| `parallax=1.0` | Movimiento máximo (objetos cercanos) |

### Toggle global

```
parallax no      # Pausa el efecto (útil para escenas estáticas)
parallax si      # Reactiva el efecto
```

English: `parallax yes` / `parallax no`

### Nota

El parallax se pausa automáticamente durante diálogos y choices para no distraer al jugador.

---

## 24. Variables del Sistema / System Variables

Eclips expone la hora y fecha del sistema como variables. El creador puede **leerlas con `si`/`if`** (para reaccionar al mundo real) y **mostrarlas en pantalla con `${...}`** (ver §10) — por ejemplo, un reloj de verdad.

Eclips exposes the system clock as variables. Creators can **read them with `si`/`if`** (to react to the real world) and **show them on screen with `${...}`** (see §10) — for example, a real clock.

### Variables disponibles

| Variable | English | Rango | Descripción |
|----------|---------|-------|-------------|
| `_hora` | `_hour` | 0-23 | Hora actual del sistema |
| `_minuto` | `_minute` | 0-59 | Minuto actual |
| `_dia` | `_day` | 1-31 | Día del mes |
| `_mes` | `_month` | 1-12 | Mes actual |
| `_anio` | `_year` | 2025+ | Año actual |
| `_dia_semana` | `_weekday` | 1-7 | Día de la semana (1=domingo) |

#### Variantes con cero a la izquierda / Zero-padded variants

Para **mostrar** un reloj bien formateado (`08:05` en vez de `8:5`), usá las variantes `_pad`:

For displaying a well-formatted clock (`08:05` instead of `8:5`), use the `_pad` variants:

| Variable | English | Ejemplo | Descripción |
|----------|---------|---------|-------------|
| `_hora_pad` | `_hour_pad` | `08`, `22` | Hora con 2 dígitos |
| `_minuto_pad` | `_minute_pad` | `05`, `59` | Minuto con 2 dígitos |

```
# Reloj de pantalla bien formateado
texto reloj "${_hora_pad}:${_minuto_pad}" x=270 y=85 color=#00FF66   # → 08:05
```

> **¿Cuándo cuál?** Usá las **crudas** (`_hora`, `_minuto`) para comparar en condicionales (`si _minuto > 30`) — son números. Usá las **`_pad`** solo para mostrar en pantalla, donde el cero a la izquierda se ve bien.

### Estado del juego / Game state

No todo lo que empieza con `_` viene del reloj. Eclips también expone **cuántas partidas guardadas existen**, para que tu **menú principal** decida si mostrar "Cargar".

Not everything starting with `_` comes from the clock. Eclips also exposes **how many saved games exist**, so your **main menu** can decide whether to show "Load".

| Variable | English | Rango | Descripción |
|----------|---------|-------|-------------|
| `_saves` | `_saves` | 0-5 | Cuántos slots de guardado están ocupados |
| `_partidas` | — | 0-5 | Alias en español de `_saves` |

Menú principal: "Cargar" solo si hay partidas guardadas

```

escena menu

fondo "fondos/titulo.png"
ocultar_caja
elegir:

"Nueva Partida"            -> nuevo_juego

[si: _saves > 0] "Continuar" -> continuar

[si: _saves > 0] "Cargar"  -> cargar

"Configuracion"            -> config

"Salir"                    -> salir

```

> **Se refresca al volver.** En *rol Menu*, al volver de una partida (o de `config`), el menú **re-evalúa** sus opciones: si guardaste durante el juego y volviste al título con `titulo`, "Cargar" **aparece sin reiniciar**. (En *rol Game* el `elegir` no se re-evalúa — sólo el menú, que es donde `_saves` cambia entre visitas.)

> **`continuar` vs `cargar`.** `continuar`/`continue` carga la partida **más reciente** directo, sin abrir la pantalla de Load — es el "Continue" de Ren'Py. `cargar`/`load` abre la pantalla para que el jugador **elija** el slot. Gatea ambos con `[si: _saves > 0]`: sin partidas, `continuar` no hace nada (solo avisa en consola).


### Versión del motor / Engine version

Solo lectura. Pintan el número canónico de `ENGINE_VERSION` en `engine/core/Types.h` (**sin** prefijo `v`, **sin** etiqueta). Úsalas para **mostrar** en pantalla, no para comparar versiones en `si`/`if`.

Read-only. They paint the canonical number from `ENGINE_VERSION` in `engine/core/Types.h` (**no** `v` prefix, **no** label). Use them to **display** on screen, not to compare versions in `si`/`if`.

| Variable | English | Ejemplo | Descripción |
|----------|---------|---------|-------------|
| `_version_motor` | `_engine_version` | `0.9.0` | Número del motor (fuente única) |

### La tecla de una acción / An action's key (v0.8.36)

Si escribís *"Presioná ESPACIO para seguir"* y tu jugador remapeó `avanzar` a `ENTER`, **tu tutorial le está mintiendo**. Esta variable lo arregla: pregunta qué tecla tiene una acción **ahora mismo**, después de las tres capas (fábrica → tu bloque `teclas {}` → el `settings.eclips` del jugador).

If you write *"Press SPACE to continue"* and your player remapped `advance` to `ENTER`, **your tutorial is lying to them**. This variable asks which key an action has **right now**.

```
"Presiona ${_tecla.vn.avanzar} para seguir."
"Guardas rapido con ${_tecla.vn.guardado_rapido}."
"Saltas con ${_key.game.jump}."
```

**Se escribe igual que la clave del jugador**, con un `_` adelante — tres partes separadas por punto:

| | |
|---|---|
| `${_tecla.<dominio>.<accion>}` | español |
| `${_key.<domain>.<action>}` | inglés |

Es **el mismo string** que el jugador escribe en su `settings.eclips` (`tecla.vn.guardado_rapido=F2`). Una sola forma para las dos caras: la que liga la tecla y la que la muestra.

Los dominios son `vn` y `juego`/`game`, los mismos del bloque `teclas {}` (§27). Las tres partes son bilingües **por separado**, igual que la clave del jugador: `${_key.juego.saltar}` y `${_tecla.game.jump}` apuntan a la misma acción y pintan la misma tecla.

> **Lo único que el prefijo decide aparte es la palabra del vacío** (abajo): con `_tecla.` sale `ninguna`, con `_key.` sale `none`, sin importar en qué idioma escribiste el dominio y la acción.

#### Una acción sin tecla dice `ninguna`, no un hueco

```
teclas vn {
    auto ninguna
}
```
```
"El modo automatico esta en ${_tecla.vn.auto}."   # -> "...esta en ninguna."
```

**Es la misma palabra que la desligó.** La alternativa —dejar el hueco, *"El modo automático está en   "*— es igual de honesta y **no se distingue de un bug de render**. Con `${_key...}` sale `none`: el idioma lo elige la grafía que escribiste vos.

> **Lo que garantiza y lo que no.** La variable garantiza que **la tecla que nombra sea la correcta**. No puede garantizar que **la frase alrededor** sea verdadera. Si escribís *"Presioná ${_tecla.vn.avanzar} para elegir"*, la tecla va a estar bien — pero durante un `elegir` esa tecla sólo activa la opción **si hay una resaltada** (con el mouse encima o después de mover con las flechas). La variable no sabe qué estás afirmando: escribí la oración pensando en el estado, no sólo en la tecla.

#### Si trae varias teclas, muestra la primera

`avanzar` viene de fábrica con cinco (`ESPACIO`, `ENTER`, el Enter del numpad, `Z` y el clic) y `${_tecla.vn.avanzar}` dice **`ESPACIO`**. Es a propósito: un tutorial nombra una tecla, no una lista. Si querés enseñar las demás, escribilas vos.

#### Si te equivocás, te lo dice

A diferencia del resto de las variables de sistema, ésta **caza tus typos** — porque las acciones son una lista cerrada y el reloj no:

```
"Presiona ${_tecla.vn.avanzr} para seguir."
```
```
[interpolacion] Aviso: 'avanzr' no es una accion del dominio 'vn'.
  Dejo el literal '${_tecla.vn.avanzr}' para que se vea en
  pantalla: una accion sin tecla se muestra 'ninguna', no vacia.
```

El literal queda **visible en pantalla** para que lo caces jugando, y el aviso sale **una sola vez** por nombre.

> **`dev` no se muestra.** `${_tecla.dev.dev_recargar}` avisa y deja el literal: las teclas de desarrollo (F8, F10, F3) no viajan al build que juega tu jugador, así que pintarlas sería justamente mentir.

#### La pantalla de controles es una pantalla `.ecl` más

Con esto ya podés escribir la tuya, y el motor no te impone ninguna: son `texto` y nada más.

```
escena controles
  texto l_avanzar "Avanzar"          x=140 y=240
  texto k_avanzar "${_tecla.vn.avanzar}"          x=560 y=240
  texto l_qsave   "Guardado rapido"  x=140 y=290
  texto k_qsave   "${_tecla.vn.guardado_rapido}"  x=560 y=290
  elegir:
    "Volver" -> fin
```

Para la columna del gameplay 2D es el mismo patrón, con el dominio `juego` (`${_tecla.juego.saltar}`).

> **Hoy muestra; todavía no captura.** El jugador remapea editando su `settings.eclips` (§27). Que la fila se pueda clickear para capturar una tecla nueva es el paso siguiente, y cuando llegue **esta pantalla no cambia de forma**: donde hay un `texto` va a haber un `control`.

### La palabra que el jugador eligió / The word the player picked *(v0.8.46)*

Las llena el bloque `palabras:` (§25.6) cada vez que el jugador clickea una palabra del pool.

| Variable | English | Descripción |
|----------|---------|-------------|
| `_palabra` | `_word` | El texto de la palabra elegida, tal como se veía en pantalla |
| `_palabra_var` | `_word_var` | El nombre de la variable que ese ítem movió; vacío si el ítem saltó de escena o no mutaba |

```
si _palabra_var == estrella
  sonido pop
fin_si
```

A diferencia del resto de las de esta sección, **éstas se guardan con la partida**: son el resultado de algo que el jugador hizo, no una lectura del reloj. Antes del primer pick valen vacío. La sección 25.6 tiene el ejemplo completo.

### Ejemplos

```
# Saludo por hora del día
si _hora >= 6
  si _hora < 12
    luna: "¡Buenos días!"
  fin_si
fin_si

si _hora >= 18
  luna: "Buenas noches. ¿No deberías descansar?"
fin_si

# Evento especial de Navidad
si _mes == 12
  si _dia == 25
    fondo "assets/club/navidad.png"
    luna: "¡Feliz Navidad!"
  fin_si
fin_si

# Algo a las 3AM
si _hora == 3
  fondo "assets/club/glitch.png"
  sonido "assets/audio/static.wav"
  "...no deberías estar aquí a estas horas."
fin_si

# Fines de semana
si _dia_semana == 1
  luna: "Es domingo. Descansa un poco."
fin_si
si _dia_semana == 7
  luna: "¡Sábado! Día perfecto para crear algo."
fin_si
```

### Nota

Las variables del sistema son de solo lectura y se actualizan en tiempo real. **Desde v0.8.47 el motor lo hace cumplir y te lo dice**: un `set _hora = 5` no escribe nada y aparece un aviso en consola. Antes la línea se aceptaba en silencio y no servía para nada, que es peor: parecía que funcionaba.

**Y lo hace cumplir por las nueve puertas que escriben variables, no sólo por `set`:**

| lo que escribís | dónde escribe | qué pasa con un nombre reservado |
|---|---|---|
| `set` / `mutar` (y la mutación como destino, `-> _hora + 1`) | la partida | no escribe, y avisa |
| `entrada _hora "..."` | la partida | **ni siquiera abre el teclado** — se rechaza antes de que el jugador escriba |
| `eclips.set` / `eclips.add` / `eclips.sub` desde Lua | la partida | no escriben, y avisan |
| `meta_set` / `meta_mutar` | `meta.eclips` (§28) | no escribe, y avisa |
| `eclips.meta_set` / `eclips.meta_add` / `eclips.meta_sub` desde Lua | `meta.eclips` (§28) | no escriben, y avisan |

El aviso **nombra el comando que escribiste**, así que dice `entrada:` o `eclips.add:` y no un `set` que no está en tu guion. Y sale **una vez por comando y nombre**: si el mismo nombre te falló en un `set` y en un `entrada`, te avisa de los dos, porque son dos líneas que arreglar.

**Y nombra también su archivo.** La lista de nombres reservados es la misma para los dos almacenes —el dueño de un nombre no cambia según dónde lo guardes—, pero el daño sí: en la partida el dato se pierde con el save, y en `meta.eclips` **quedaría para siempre**, porque ese archivo sobrevive a borrar los saves. Por eso los avisos de la familia `meta_` hablan de `meta.eclips` y los de `set` hablan de la partida guardada: te mandan al archivo correcto.

Las variantes `_pad` dan el mismo valor con cero a la izquierda, pensadas para mostrar (no para comparar).

> ⚠ **El `_` NO significa «variable de sistema».** Es una convención de nombre, no una regla del motor, y esa diferencia importa porque **vos podés usar `_` en tus propias variables** y funcionan normal:
>
> ```
> set _mi_contador = 7        # tuya. Funciona.
> meta_set _mi_contador = 7   # tuya, y persistente. Tambien funciona.
> set _hora = 5               # del sistema. El motor avisa y no escribe.
> ```
>
> Lo que el motor te reserva es una **lista concreta**, no un prefijo: las de esta sección (con sus dos grafías), la familia `${_tecla.*}`/`${_key.*}`, y **`_tema_activo`** — ésa última no es de sólo lectura por el reloj sino porque es **estado interno del motor y viaja adentro de la partida guardada**; para cambiar el tema existe `cargar_tema`. El aviso te dice cuál de los dos casos te tocó.
>
> Y las que el motor publica **para vos** —`_palabra`, `_palabra_var` (§10) y `_batalla_ganada`— **sí se pueden escribir**: son datos tuyos, sólo que el motor los llenó primero.

---

## 25. Eventos Random / Random Events

Los eventos random permiten que cosas ocurran de forma espontánea mientras el jugador está en una escena interactiva (durante `esperar_click`). Perfectos para dar vida a personajes con acciones sorpresa.

Random events allow spontaneous things to happen while the player is in an interactive scene (during `wait_click`). Perfect for giving characters life with surprise actions.

### Definir eventos

```
evento_random cafe cada=5 probabilidad=0.35 cooldown=60 -> escena_cafe
evento_random bostezo probabilidad=0.2 cooldown=30 -> escena_bostezo
```

English:
```
random_event coffee every=5 chance=0.35 cooldown=60 -> scene_coffee
random_event yawn chance=0.2 cooldown=30 -> scene_yawn
```

**Cómo funciona, en una frase: cada tanto el motor hace un *ensayo*, y en cada
ensayo tira contra la probabilidad.** `cada=` es el ritmo de los ensayos,
`probabilidad=` es la tirada.

| Parámetro | English | Descripción |
|-----------|---------|-------------|
| `cada=N` | `every=N` | **El ritmo:** cada cuántos segundos hay un ensayo. **Por defecto `1`**, o sea un ensayo por segundo. Mínimo `0.05` (más rápido que eso, el motor avisa y usa `0.05`). No admite `avances_dialogo`: los eventos corren durante `esperar_click`, donde no hay avances de diálogo. |
| `probabilidad=N` | `chance=N` | **La tirada:** qué tan probable es que ese ensayo salga, de `0` a `1`. Con `cada=1` (el default), `probabilidad=0.2` es **20% por segundo, exacto**. Con `probabilidad=1` dispara **siempre** que le toque el ensayo. Fuera de `[0,1]` el motor avisa y recorta; **si la omitís también avisa**, porque un evento sin probabilidad no dispara nunca. |
| `cooldown=N` | `cooldown=N` | Segundos mínimos entre activaciones del mismo evento. El cooldown veta **la tirada, no el ritmo**: el reloj sigue corriendo por debajo. Sin negativos (aviso + 0 = sin espera). |

Los dos ejes se combinan como los leerías en voz alta:

```
evento_random gaviota cada=8 probabilidad=0.5 -> escena_gaviota
# "cada 8 segundos, mitad y mitad"
```

### El gate `[si:]`: cuándo el evento ni siquiera intenta *(v0.8.18)*

Un evento puede llevar una condición **delante de sus claves**, y mientras esa
condición sea falsa el evento **no tira**:

```
evento_random lluvia [si: _hora >= 18] cada=5 probabilidad=0.3 -> escena_lluvia
```

English: `random_event rain [if: _hora >= 18] every=5 chance=0.3 -> scene_rain`

Se lee tal cual: *"cada 5 segundos, si ya son las 6 de la tarde, 30% de que
llueva"*. Es la misma condición que ya usás en las opciones de `elegir` y en los
hotspots de `esperar_click`, con los mismos seis operadores: `==` `!=` `>` `<`
`>=` `<=`. Y lee cualquier variable — tuya (`set afecto = 40`) o del sistema
(`_hora`, `_saves`).

| Dónde va | Qué pasa |
|---|---|
| **Justo después del nombre**, antes de las claves | es el único lugar. Si lo escribís al final, el motor te avisa con la línea y **no define el evento** |
| El gate se evalúa **en el beat**, no en cada frame | con `cada=5` se pregunta una vez cada 5 segundos, no 300 veces |
| Un gate cerrado **no gasta nada** | ni la tirada ni el cooldown: el evento queda intacto para cuando la condición se cumpla |
| El **ritmo sigue corriendo** por debajo | cuando el gate se abre, el evento espera al siguiente beat en vez de disparar en ese instante — así no se nota un tirón al cambiar la condición |

**Un gate mal escrito NO define el evento**, y el motor te dice por qué (con
número de línea):

```
evento_random lluvia [si: _hora <> 18] cada=5 probabilidad=0.3 -> escena_lluvia
# AVISO linea N: el gate [si: usa el operador '<>', que no existe.
#                Los operadores son: == != > < >= <=. Se ignora esa linea.
```

Es a propósito, y es la decisión menos peligrosa de las dos: si el motor
ignorara sólo el gate y dejara el evento vivo, **un typo lo pondría a dispararse
siempre** — justo lo contrario de lo que escribiste.

**El gate se guarda con la partida.** Si guardás después de definir el evento y
cargás, el evento vuelve **con su condición puesta**: no hay forma de que un
`load` lo suelte a dispararse por su cuenta.

> **Para depurar:** `azar_traza si` te dice, beat por beat, si el silencio fue
> el gate (`gate cerrado (_hora >= 18) -> no tira`), el cooldown, o mala suerte.
> Ojo: la traza es un interruptor de **sesión** — si cargás una partida, se
> apaga y hay que volver a encenderla.

**Cada evento arranca con su propio desfase.** Si defines tres eventos con
`cada=5`, el motor les sortea a cada uno un momento distinto para su primer
ensayo, de modo que no disparen todos al mismo tiempo y no se oiga el pulso.

Si defines dos eventos y ambos "ganan su tirada" en el mismo instante, dispara
**el que definiste primero** en el guion. Con los desfases eso es raro, pero
cuando pasa el segundo no pierde su turno: lo cobra enseguida.

Si la escena destino no existe, el evento **avisa en consola y se desactiva solo** — nunca rompe el juego.

### Activar / desactivar

```
iniciar_eventos     # Activa el sistema de eventos random
detener_eventos     # Desactiva (los eventos definidos se mantienen)
```

English: `start_events` / `stop_events`

`iniciar_eventos` sin ningún evento definido avisa en consola (probablemente olvidaste los `evento_random`, o los pusiste después).

### El retorno: la escena del evento VUELVE sola

Cuando un evento dispara, el motor salta a su escena — y **recuerda dónde estaba el jugador**. Si la escena del evento termina **sin ningún salto explícito**, el jugador **vuelve automáticamente al `esperar_click` original**, con sus hotspots vivos y sus condiciones `[si:]` re-evaluadas:

```
escena evento_cafe
"Alguien trajo cafe al club."
# fin de la escena: el jugador vuelve solo a donde estaba
```

Dos reglas que hacen esto predecible:

- **Un evento a la vez.** Mientras un evento corre, no dispara otro (aunque la escena del evento tenga su propio `esperar_click`).
- **El salto explícito toma el volante.** Si la escena del evento usa `ir_a`, `elegir`, o cualquier salto, el retorno automático **se cancela**: quien salta, rutea. Un evento que quiere volver, simplemente no salta.

Y una cosa más para no sorprenderte: **el escenario se comparte**. La escena del evento pinta sobre el mismo escenario que cualquier otra — si cambia el fondo o agrega capas, esos cambios **persisten al volver** (igual que con cualquier salto del motor). Un evento que quiere ser invisible, no toca el escenario.

### Repetir una tirada: `azar_semilla`

```
azar_semilla 7      # random_seed 7
azar_semilla        # sin número: vuelve al azar de verdad
```

Fija la semilla del azar, así que **la misma corrida da los mismos resultados**.
Sirve para balancear: cambiás una probabilidad, volvés a correr, y la diferencia
que ves es tu cambio y no la suerte. Sin argumento vuelve a la semilla del
reloj. **La semilla nunca se guarda en la partida** — al cargar, el azar
arranca fresco.

### Cuando no pasa nada: `azar_traza`

```
azar_traza si       # random_trace yes
```

Enciende un renglón de consola **por cada ensayo**, diciendo qué pasó:

```
[azar] cafe  beat  tirada 0.61 vs 0.35 -> pierde
[azar] cafe  beat  cooldown 4.2s -> no tira
[azar] cafe  beat  tirada 0.11 vs 0.35 -> GANA -> escena_cafe
```

Es para el momento en que escribís un evento, jugás cinco minutos y no ves
nada: la traza te dice si fue mala suerte o si el evento ni siquiera está
tirando. **Es ruidosa a propósito** —con `cada=1` y cinco eventos son cinco
renglones por segundo— por eso está apagada por defecto.

### Guardar y cargar

- Guardar **dentro de la escena de un evento** funciona: al cargar, la escena termina y el retorno sigue vivo.
- **El ritmo sobrevive al guardado.** Si un evento estaba a mitad de camino de su próximo ensayo, al cargar sigue donde iba — si se reiniciara, todos los eventos volverían a arrancar juntos y se oiría el pulso.
- Los **cooldowns se reinician al cargar** (un evento puede disparar antes de lo esperado *una vez* tras cargar una partida).
- **La semilla no se guarda:** cargar es barajar de nuevo.

### Importante

Los eventos random **solo se evalúan durante `esperar_click`**, nunca durante diálogos o choices — y tampoco con el historial u otro aviso del motor abierto. Esto evita que interrumpan la narrativa.

---

## 25.5 El bloque `azar:` / The `azar:` block *(v0.8.12)*

Un `evento_random` contesta **cuándo** pasa algo. El bloque `azar:` contesta la otra
pregunta: **cuál** de varias cosas pasa.

```
azar:                          # random:
  peso=5   "Hola."
  peso=50  "No lo creo."
  peso=55  "¿Viste eso?"
```

El motor elige **una sola** de las líneas y la muestra. Las otras no pasan.

Las entradas también pueden ser saltos, y podés mezclar los dos:

```
azar:
  peso=3 -> sala_a
  peso=1 -> sala_b
```

> **Una entrada es un texto O un salto, nunca las dos.** Si escribís
> `peso=1 "texto" -> escena` el motor te lo dice y descarta ese renglón — los demás
> siguen. *(Una flecha **dentro** de las comillas es texto normal: `"anda al norte ->
> derecha"` se muestra tal cual.)*

### `peso=` es un reparto, no un porcentaje

```
peso=5   →  5 de 110  =  4.5%
peso=50  → 50 de 110  = 45.5%
peso=55  → 55 de 110  = 50.0%
```

**Los pesos no tienen que sumar 100 ni nada.** Lo único que importa es cuánto pesa cada
uno **comparado con los otros**: `5/50/55` y `10/100/110` hacen exactamente lo mismo. Si
querés que todas salgan igual, escribí `peso=1` en todas.

`peso=0` **apaga** una entrada sin borrarla — útil para silenciar una línea mientras
probás, sin perderla.

> **Cada entrada lleva su `peso=`, y eso es lo que dice dónde termina el bloque.** La
> primera línea que no empiece con `peso=` (o `weight=`) ya no es del bloque: es el guion
> siguiendo. Por eso podés escribir una narración justo debajo sin que el motor se la
> coma como si fuera una opción más:
>
> ```
> azar:
>   peso=1 "Una."
>   peso=1 "Otra."
> "Esto es narración normal, y el bloque terminó arriba."
> ```

En inglés: `weight=`.

### `repetir=` — dos formas de sortear

```
azar:                          # repetir=si (el default): DISTRIBUCIÓN
  peso=90 "casi siempre esta"
  peso=10 "de vez en cuando esta"

azar repetir=no:               # random repeat=no: BOLSA
  peso=3 "la de siempre"
  peso=1 "la rara"
  peso=1 "la otra rara"
```

- **`repetir=si`** (default) — cada vez es una tirada nueva e independiente. La misma
  línea puede salir dos veces seguidas. Es lo que querés para *"esta frase es diez veces
  más común que la otra"*.
- **`repetir=no`** — **bolsa**: el motor va sacando sin reponer hasta agotarlas todas, y
  ahí rebaraja. Es lo que querés para *"quiero que se vean las veinte, sin repetir"*.

> ⚠️ **Bajo `repetir=no`, `peso=` cambia de significado.** Como cada entrada sale
> **exactamente una vez por ciclo**, el peso ya no decide **cuántas veces** sale sino
> **qué tan temprano** le toca dentro del ciclo. Con dos entradas eso significa que
> `peso=90` y `peso=10` te dan **50/50 igual** — el motor te avisa si escribís
> `repetir=no` con menos de tres entradas, justamente por eso.

### Guardar y cargar

**El sorteo no se guarda.** Si cargás una partida a media bolsa, la bolsa **arranca de
cero** — cargar es barajar de nuevo. *(El **ritmo** de un `evento_random` sí sobrevive
al guardado; son dos cosas distintas y cada una hace lo suyo.)*

> **Y una consecuencia que conviene saber antes de que te sorprenda: si guardás mientras
> estás leyendo la salida de un `azar:`, al cargar te puede tocar OTRA.** El guardado
> recuerda **en qué línea del guion ibas** —y esa línea es el bloque—, así que al volver el
> bloque **sortea de nuevo**, con la bolsa llena. No es un error: es la misma ley de arriba
> vista desde el otro lado. Si querés que una elección al azar **quede fijada** para el
> resto de la partida, guardá su resultado en una variable con `set` y ramificá con `si`:
> eso sí se guarda.

### Verlo funcionar

`azar_semilla` (sección 25) también gobierna este bloque, así que podés repetir una
corrida entera mientras balanceás pesos. Y `azar_traza si` imprime un renglón por
sorteo:

```
[azar] bloque L23 - distribucion - peso 50/110 -> "No lo creo."
[azar] bloque L31 - bolsa - peso 1 - quedan 2 de 3 -> "la rara"
```

En modo bolsa el `quedan N de M` te dice dónde va el ciclo — y es la forma más fácil de
ver que al cargar una partida la bolsa vuelve a estar llena.

---

## 25.6 El bloque `palabras:` / The `words:` block *(v0.8.45)*

Un pool de palabras en pantalla, todas clickeables, de las que el jugador elige **una**.
Nació para el juego de poemas: un montón de palabras sueltas, cada una vale para alguien,
y la que elegís mueve una variable.

```
palabras 10 estilo=cuaderno:        # words 10 style=cuaderno:
    "amor" estrella
    "lluvia" luna
    "eternidad" -> sol + 2
    "..." -> rotura
    # ... y el resto del pool: veinte, cien palabras
```

El número **es cuántas se muestran, no cuántas hay**: escribís el pool entero —veinte,
cien— y el motor saca diez al azar. Al clickear una, el pool desaparece y el guion sigue
en la línea de abajo.

### La forma corta de un ítem

`"amor" estrella` es azúcar de `"amor" -> estrella + 1`. Es la forma que vas a escribir el
99% del tiempo, porque un pool de poema es justo eso: cada palabra le suma un punto a
alguien.

Cuando necesitás otra cosa, la escribís entera:

```
palabras 5:
    "amor" estrella                 # suma 1 a estrella
    "eternidad" -> sol + 2          # suma 2 a sol
    "vacio" -> sol - 1              # resta
    "nombre" -> humor = triste      # asigna
    "..." -> rotura                 # SALTA de escena
```

> **Un ítem que salta sale del pool y del flujo**, como una opción de `elegir` con
> destino. Los que mutan **no saltan**: el guion continúa donde estaba.

### El peso y el gate van ADELANTE

```
palabras 3 repetir=no:
    peso=0.01 "..." -> rotura       # la palabra rara, casi nunca sale
    peso=0 "prohibida" estrella     # apagada: nunca sale, y no se borra
    [si: acto == 1] "festival" luna
```

`peso=` es el mismo reparto relativo del bloque `azar:` (sección 25.5), y **`peso=0`
apaga sin borrar**: las demás no se corren de lugar.

> **Los dos van antes del texto, y en cualquier orden.** Lo que va después de la flecha es
> el destino, y no lo vuelve a leer nadie — si escribís un parámetro ahí, se lo come el
> destino.

### El `[si:]` de un ítem: la palabra ni siquiera entra al sorteo *(v0.8.46)*

```
palabras 3:
    "amor" estrella
    [si: acto == 2] "festival" luna       # solo existe en el acto 2
    [si: sol > 5]   "eclipse" -> rotura   # solo si Sol ya te tiene ley
```

Un ítem cuyo `[si:]` da falso **no se sortea**: no es que se dibuje apagada, es que no
compite. Y el gate se evalúa **cada vez que el bloque corre**, así que el mismo pool
cambia de contenido a medida que cambia la partida.

**Apagar no es borrar:** las demás no se corren de lugar, así que `repetir=no` sigue
llevando la cuenta de cuáles ya salieron aunque el gate abra y cierre entre visitas.

Y como el pool efectivo puede quedar por debajo de lo que pediste —cosa que el aviso del
parser no puede ver, porque él cuenta el pool entero—, el motor lo dice al correr, **una
vez por bloque** y no una por palabra:

```
[VNScriptState] palabras: linea 45: 2 de 3 palabras ocultas por su gate [si:], asi que
se muestran 1 y no las 3 que pediste.
```

> **Si el gate deja el pool sin nadie, el bloque se saltea y el motor nombra las dos
> causas por separado** (`3 ocultas por su gate [si:], 0 en peso=0`): una te manda a
> revisar tu `.ecl` y la otra el estado de la partida.

### Qué palabra eligió el jugador: `${_palabra}` y `${_palabra_var}` *(v0.8.46)*

| variable | inglés | qué trae |
|---|---|---|
| `_palabra` | `_word` | el texto de la palabra que clickeó, tal como se veía en pantalla |
| `_palabra_var` | `_word_var` | el nombre de la variable que ese ítem movió — vacío si el ítem saltó de escena o no mutaba |

```
palabras 3 estilo=cuaderno:
    "amor" estrella
    "lluvia" luna
    "deseo" estrella

si _palabra_var == estrella
    sonido pop
fin_si

si _palabra == deseo
    estrella feliz: "¡Ese es mi favorito!"
fin_si
```

Con esas dos, el contador, el feedback, los empates y el ruteo del minijuego son tuyos:
**el motor sortea y te dice qué se eligió; el juicio es del guion.**

> **Se guardan con la partida.** Si el jugador guarda después de elegir y carga más tarde,
> `${_palabra}` sigue valiendo lo mismo — igual que el punto que ese click le sumó a
> Estrella. Antes del primer pick valen vacío.
>
> **Un ítem que salta también las llena**, y las llena *antes* de saltar: la escena a la
> que te manda la palabra rara puede leer cuál fue.

### `repetir=no` es el default de este bloque, y es al revés que `azar:`

| | `palabras` | `azar:` |
|---|---|---|
| default | `repetir=no` | `repetir=si` |
| qué significa | pool a agotar: cada palabra sale **una vez por ciclo** | distribución: puede repetir siempre |

Con `repetir=no`, **una palabra no puede salir dos veces en la misma pantalla**, ni cuando
el pool se agota a mitad de la página y el motor rebaraja.

Y pedir más de lo que hay es imposible en ese modo, así que el motor lo dice:

```
[EclipScript] AVISO linea 12: palabras: pediste 10 y el pool tiene 3. Con repetir=no
cada palabra sale una sola vez por ciclo, asi que se van a ver 3. Usa repetir=si si
querias que se repitan.
```

### Dónde se dibuja, y con qué letra

Las palabras se acomodan solas: de izquierda a derecha, y cuando no entra una más, fila
nueva. No hay posiciones por palabra **a propósito** — el azar las necesita automáticas.

```
palabras 10 x=120 y=140 ancho=1680 alto=700 estilo=cuaderno:
```

| clave | qué hace | si no la ponés |
|---|---|---|
| `x` `y` | esquina del área | una franja centrada que **no tapa la caja de diálogo** |
| `ancho` `alto` | tamaño del área; el `ancho` es donde envuelve la fila | 88% × 65% del lienzo |
| `estilo` / `style` | el paquete de `project.eclips`: fuente, color, glow, borde **y hover** | texto plano, sin resaltado al pasar el mouse |
| `z` | orden de dibujo, como en `texto` | 0 |

**El `estilo=` es lo que hace que el pool se sienta escrito a mano**, porque de ahí sale la
fuente — y su variante `_hover` es la que resalta la palabra debajo del cursor:

```
estilo cuaderno {
    fuente manuscrita
    color #E8DCC8
    color_hover #FF3E7F
    brillo_radio_hover 4
}
```

> **Sin un `*_hover` en el estilo, la palabra no cambia al pasarle el mouse** — el motor
> no inventa un resaltado que no pediste. Es la misma regla del `enlace`.

Si las palabras no entran en el área, se dibujan igual y el motor avisa una vez:

```
[VNScriptState] palabras: linea 12: las 10 palabras necesitan 6 filas y no entran en el
area. Se dibujan igual, desbordadas: subi `alto=` o pedi menos.
```

### Con teclado

`ABAJO` y `ARRIBA` recorren las palabras y `ESPACIO` elige la focada, igual que en un
`elegir`. **El recorrido es el orden en que las escribiste**, no el geométrico: en una
grilla, `ABAJO` te lleva a la palabra de al lado.

### Lo que el pool NO hace, y es tuyo

El contador, el feedback, los empates, las rutas y volver a mostrar el pool son del guion:

```
escena ronda
    mutar ronda + 1
    texto marcador "${ronda} / 20" x=1180 y=40 estilo=cuaderno
    palabras 10 estilo=cuaderno:
        "amor" estrella
        "lluvia" luna
        # ... el pool sigue
    si ronda < 20
        ir_a ronda
    fin_si
    ir_a evaluar
```

**El motor sortea; vos juzgás.** Volver a entrar al comando **re-sortea**, y cargar una
partida guardada arriba del pool también: el barajado siempre es fresco.

### Comparar dos contadores: con `${}`, o con un umbral

Ésta es la trampa del minijuego:

```
si luna > estrella        # ⚠ NO compara los dos contadores
```

En un `si`, **la izquierda es una variable y la derecha es un valor literal**. `estrella`
no es un número, así que el motor avisa en consola y la condición **da falso**. Lo mismo
con `mutar`: `mutar dif - estrella` avisa y **no toca** `dif`. Para comparar los dos
contadores, escribí la derecha con `${}`: `si luna > ${estrella}` (§10, *Comparar dos variables*).

**Y con cinco picks hay otra forma, exacta:** preguntá por un umbral. Quien llegue
a 3 de 5 tiene mayoría sobre los otros dos juntos.

```
mutar gano = nadie

si luna >= 3
    mutar gano = luna
fin_si
si estrella >= 3
    mutar gano = estrella
fin_si

si gano == luna
    luna feliz: "¿Es para mí?"
fin_si
si gano == nadie
    "Nadie sacó mayoría."
fin_si
```

> **El ganador se guarda en una variable en vez de imprimirse dentro de cada `si`**, y no
> es estilo: así existe **un lugar donde preguntar «no ganó nadie»**. El empate no es un
> borde — con cinco picks entre tres personajes, el 2-2-1 es el caso común, y sin ese
> flag la escena se queda **muda**.

### Cuándo se cuenta la palabra

**Apenas el jugador la elige, en el mismo instante.** La mutación corre dentro del pick,
antes de que el guion avance, así que un `si` puesto **justo debajo** del bloque ya ve el
valor nuevo:

```
palabras 6 estilo=cuaderno:
    "amor" estrella
    # ...

si estrella >= 3         # ya cuenta el click que acabás de hacer
    animar estrella glitch 0.5s
fin_si
```

> **Lo que NO se actualiza solo es lo que ya está dibujado.** Un `texto` con el marcador
> escrito **arriba** del bloque se queda con el número viejo hasta que el guion vuelva a
> pasar por esa línea. Si querés que el contador cambie **en el momento del click**,
> poné el `texto` **debajo** del `palabras`.

### Guardar a mitad del poema *(v0.8.46)*

Con el pool abierto, el jugador **puede** guardar y cargar: `K`/`L`, el menú de guardado,
o la pausa. **Al cargar, el bloque se vuelve a ejecutar**, así que el pool aparece **con
otras palabras** — el barajado siempre es fresco, igual que al re-entrar.

> **Si tu juego necesita que esa parte NO se pueda guardar**, es una decisión tuya y la
> escribís: `deshabilitar acciones` **en la misma escena que el bloque** (sección
> 14.5.quater). El motor no lo decide por vos. Va en la misma escena porque el sello se
> apaga solo al cambiar de escena.
>
> Y donde el jugador cae al cargar lo decide **tu contador**, no el motor: si la ronda
> vive en una variable, vuelve a la ronda donde estaba (con palabras nuevas); si querés
> que el poema empiece de cero, reseteala al entrar al bloque.

---

## 26. Bloqueo de Ajustes del Tema / Theme Setting Locks

Algunos juegos quieren que ciertas preferencias visuales o de
comportamiento sean **decisión inamovible del creador**, no del
jugador. Por ejemplo: en un RPG de pixel art la velocidad del texto es parte
del feel del juego — si el jugador la cambia a "instantáneo", el
juego pierde su carácter. En una novela visual tradicional, en cambio, el jugador debería
poder ajustar todo libremente.

Eclips soporta esto con **flags de bloqueo** (`*_lock`) en el
archivo `.eclips` del tema. Cuando un campo está locked:

- El slider correspondiente en el menú de Configuración aparece
  **gris con un icono de candado** al lado del nombre.
- El slider no responde a teclado, click ni flechas.
- El cursor del menú salta el ítem locked al navegar.
- El runtime del juego **respeta el valor del tema**, ignorando
  cualquier override que el jugador haya hecho en otro tema sin
  lock. (El override sigue persistido — revive si el creador
  quita el lock en una versión futura del juego.)

### Campos disponibles para lock

Los tres campos del patrón "Theme propone, Settings sobreescribe"
admiten lock:

| Campo (ES) | Campo (EN) | Qué bloquea |
|---|---|---|
| `velocidad_texto_lock` | `text_speed_lock` | Velocidad del typewriter, en **caracteres por segundo** |
| `auto_demora_lock` | `auto_delay_lock` | Espera antes de avanzar en modo auto |
| `skip_intervalo_lock` | `skip_interval_lock` | Velocidad de skip |
| `skip_no_visto_lock` | `skip_unseen_lock` | Si el skip puede atravesar texto no leído |

Volumen de música, volumen de efectos y pantalla completa **no
admiten lock** — son preferencias puras del jugador (audio según
sus auriculares, ventana según su monitor) y bloquearlas sería
invasivo sin razón válida.

### Sintaxis

Cada lock (y cada campo Sí/No como `skip_no_visto`) acepta varias
formas equivalentes para que escribas natural en cualquier idioma:

- **Afirmativo:** `true`, `si`, `sí`, `yes`, `1`
- **Negativo:** `false`, `no`, `0`

El formato del archivo usa whitespace como separador (NO usar `:`).
Default es `false` (no bloqueado) — solo declarás los campos que
quieras activar.


tema_pixel.eclips — feel anclado a un RPG de pixel art:

```
velocidad_texto       90
velocidad_texto_lock  true
auto_demora           0.8
auto_demora_lock      true
skip_intervalo        0.02
skip_intervalo_lock   true
skip_no_visto         no
skip_no_visto_lock    true
```
tema_pastel.eclips — el jugador personaliza libremente:

```
velocidad_texto  60
auto_demora      2.0
```

**La unidad es el CARÁCTER, no el byte.** `velocidad_texto 30` son treinta
caracteres por segundo escribas en el idioma que escribas: una `ñ` y una `n`
tardan lo mismo, y una línea acentuada termina de escribirse **cuando se ve
completa**, no unos instantes después. Suena obvio y hasta el lote
`typewriter-wrap` no era cierto — el motor contaba bytes, así que cada acento
costaba un tic de más y la flechita de avance aparecía tarde.

### Skip de texto no visto (`skip_no_visto`)

Por defecto, Eclips se comporta como Ren'Py: el **skip** (mantener Ctrl,
o el botón Skip de la caja de diálogo) **solo atraviesa texto que el
jugador ya leyó**. Al llegar a una línea nueva, el skip **se detiene en
seco** y el jugador lee a velocidad normal. Esto evita que alguien se
salte contenido sin querer la primera vez que juega.

El jugador puede cambiar esto desde Configuración con el toggle
**"Saltar no visto"** (Sí/No). En `No` (default) el skip respeta lo no
leído; en `Sí`, el skip atraviesa todo, visto o no.

Como creador, podés proponer el valor inicial desde el tema:

```
skip_no_visto       no      # default — el skip respeta texto nuevo (recomendado)
skip_no_visto       si      # el skip atraviesa todo desde el inicio
```

Igual que `velocidad_texto`, esto es "el tema propone, Settings dispone":
el jugador puede sobreescribir tu valor desde el menú, salvo que lo
bloquees con `skip_no_visto_lock`.

> ⚠️ **Sobre bloquear este campo:** a diferencia de la velocidad de texto,
> "saltar texto no visto" es una **preferencia de accesibilidad del
> jugador** — en un VN normal, lo más respetuoso es dejarla libre (sin
> lock). Ren'Py, de hecho, nunca permite al juego forzarla. Eclips te da
> la opción de lockearla, pero **úsala solo si tu juego tiene una razón
> real**: por ejemplo, un juego de terror que no quiere que le adelanten
> sustos no leídos, o un híbrido VN+gameplay donde el ritmo es parte del
> diseño. Si no tenés un motivo concreto, **dejalo sin lock**.

Cómo se comporta el skip según el contexto:

- **Texto ya visto** → el skip avanza (rápido, según `skip_intervalo`).
- **Texto nuevo, `skip_no_visto` en No** → el skip se detiene; el botón
  Skip se ve **gris e inactivo** (no responde al mouse). Revive solo al
  volver a texto visto.
- **Texto nuevo, `skip_no_visto` en Sí** → el skip atraviesa.
- **Durante una elección (choice)** → el skip nunca avanza (no atropella
  decisiones); el botón Skip se ve gris.

El skip por **botón** es un interruptor continuo (lo activás una vez y
sigue solo, no hay que mantener nada); el skip por **Ctrl** dura mientras
mantengas la tecla. Ambos pueden convivir con el modo **Auto** activado
al mismo tiempo, igual que en Ren'Py.

### Cuándo usar lock

**Sí, considéralo cuando:**

- La velocidad o timing es parte central del feel del juego
  (un RPG de pixel art, un juego de acción, un juego rítmico).
- Tu juego tiene voice acting o efectos sonoros sincronizados al
  texto y un "instantáneo" rompe la sincronización.
- Estás portando un juego clásico que originalmente no tenía estos
  controles y quieres preservar la experiencia original.

**No, déjalo libre cuando:**

- Tu juego es una novela visual tradicional — el
  jugador espera personalizar y forma parte de la accesibilidad.
- Tu audiencia incluye lectores rápidos y lentos por igual y
  forzar una velocidad excluye a uno de los dos grupos.
- No tienes razón concreta. **El default es libre**; lock es la
  excepción cuando hay motivo creativo claro.

### Aviso técnico

El lock afecta a la UI del menú y al runtime del dialog box, pero
**no es protección anti-modificación**. Un jugador determinado que
edite `save/settings.eclips` a mano puede seguir alterando los
valores. Esto es por diseño — Eclips no implementa anti-cheat para
single-player. El lock comunica intención del creador y previene
modificación accidental, no maliciosa.

### Indicador visual del candado

El candado se dibuja como una primitiva geométrica gris al lado del
nombre del slider. En versiones futuras del motor, los creadores
podrán reemplazarlo con un sprite PNG custom (idle / hover /
animación) declarando texturas en el theme. Hoy es default
universal — sin assets requeridos.

## 27. El archivo `proyecto.eclips` / Project Manifest

`proyecto.eclips` (o `project.eclips` — ambos nombres son canónicos) es el **manifest** opcional de tu proyecto. Si existe en la raíz, el motor lo carga al arrancar y usa su contenido para configurar todo el proyecto. Si no existe, el motor sigue funcionando como siempre — útil para prototipar un `.ecl` suelto.

`proyecto.eclips` (or `project.eclips` — both names are canonical) is your project's optional **manifest**. If it exists at the root, the engine loads it at startup. If absent, the engine works as usual — useful for prototyping single `.ecl` files.

### ¿Por qué usarlo? / Why use it?

- **Definí personajes UNA vez** — no más redeclarar `personaje luna "..."` en cada `.ecl`.
- **Display names automáticos** — la nameplate muestra "Luna" cuando escribís `l: "Hola"`.
- **Título de la ventana del SO** se setea automáticamente.
- **Cambiá la carpeta de scripts** sin tocar `main.cpp`.
- **Tema default** del juego sin escribir `cargar_tema` en cada script.

### Sintaxis

`proyecto.eclips` usa el mismo formato que los archivos de tema (`.eclips`): pares clave-valor a nivel raíz, y bloques `{ ... }` para entidades estructuradas como personajes.

```
# proyecto.eclips
titulo "Mi Visual Novel"
autor "Tu Nombre"
version "0.1"
script_inicial "intro.ecl"
script_dir "vnscript/"
tema_default "tema_pastel.eclips"
personaje l {
nombre "Luna"
path "assets/personajes/luna/"
compuesto si
}
personaje e {
nombre "Estrella"
path "assets/personajes/estrella/"
compuesto si
}
```

### Campos a nivel raíz

| Campo | Inglés | Descripción |
|-------|--------|-------------|
| `titulo` | `title` | Título del juego. Se usa en la barra de la ventana del SO. |
| `autor` | `author` | Nombre del autor (consultable desde scripts si lo necesitás). |
| `version` | `version` | Versión del juego. |
| `script_inicial` | `initial_script` | `.ecl` que arranca con "Nueva Partida". |
| `script_dir` | (igual) | Carpeta donde el motor busca scripts si no los encuentra en la raíz. |
| `tema_default` | `default_theme` | Tema visual aplicado al iniciar cada partida (override-eable con `cargar_tema`). |
| `intro_motor` | `engine_intro` | Intro de marca del motor al arrancar: `no` (default) · `siempre` · `una_vez`. Apagada salvo que la enciendas — ver abajo. |

### El campo `intro_motor` / `engine_intro`

Eclips tiene una intro de marca propia (un eclipse animado con sonido) que puede correr al arrancar tu juego. **Está apagada por default**: un juego hecho con Eclips no se anuncia como Eclips salvo que vos lo decidas. Si querés mostrarla, elegí el modo:

| Valor | Inglés | Qué hace |
|-------|--------|----------|
| `no` | `no` | Nunca se muestra. **Default** (también si omitís la línea). |
| `siempre` | `always` | Corre en cada arranque del juego. |
| `una_vez` | `once` | Corre solo la **primera vez** que el jugador abre el juego; después nunca más. |

El jugador **siempre puede saltarla** con click o tecla. El modo `una_vez` recuerda que ya se vio guardando una marca en `meta.eclips`; si el jugador borra sus datos o reinstala, vuelve a aparecer esa primera vez.

### Bloque `personaje { ... }`

Definí un personaje una vez y úsalo en todos tus `.ecl` sin redeclararlo.

```
personaje l {
nombre "Luna"
path "assets/personajes/luna/"
compuesto si
}
```

Después en cualquier `.ecl`:

```
escena intro
mostrar l 1a center        # ← funciona sin "personaje l" en el .ecl
l: "Hola, te he estado esperando."   # ← nameplate dice "Luna"
fin
```

#### Campos del bloque

| Campo | Inglés | Descripción |
|-------|--------|-------------|
| `nombre` | `name` | Nombre que aparece en la nameplate. Si vacío, usa el alias. |
| `path` | `ruta` | Ruta a los sprites del personaje. |
| `compuesto` | `composite` | `si`/`yes` para personajes compuestos (carpeta con partes). |
| `velocidad_texto` | `text_speed` | Velocidad de texto custom para este personaje. |
| `caja` | `box` | Caja de diálogo custom (referencia a tema). |
| `escala_base` | `base_scale` | Multiplicador de escala del sprite (uniforme, no deforma). |
| `ancla_y` | `anchor_y` | Offset vertical estático en px. `+` baja, `−` sube. Para encuadre tipo busto (v0.4.8). |

> Los campos marcados como "parseable, no consumido aún" se aceptan en la sintaxis pero el motor todavía no los aplica. Se cablearán en versiones futuras sin romper la sintaxis actual.

### Apariencias — nombres lógicos para looks compuestos (v0.4.7)

Un personaje compuesto se muestra con cadenas como `1l_2r_a` (left_right_face). Memorizarlas es incómodo. Con **apariencias** les ponés nombre, dentro del bloque del personaje:

```
personaje Luna {
path "assets/luna/"
compuesto on
apariencia neutral 1l_2r_b
apariencia feliz   2l_1r_c
apariencia triste  1l_1r_d
look_default neutral
}
```

(Bilingüe: `look` / `look_default`. Sin comillas — el nombre y la cadena son tokens, no texto literal.)

**Uso en el `.ecl`:**

```
mostrar Luna centro          # sin EXPR → usa el look_default (neutral → 1l_2r_b)
mostrar Luna feliz centro    # expande feliz → 2l_1r_c
mostrar Luna 1l_2r_a centro  # cadena cruda → sigue funcionando
Luna triste: "..."           # un alias también vale en diálogo
Luna a: "..."                # una cara real (no alias) cambia solo la cara, encima del look actual
```

- El **default** es solo "qué alias usa `mostrar X` sin expresión". Puede apuntar a un alias o ser una cadena cruda.
- Un alias siembra un look completo; podés seguir cambiando **solo la cara** encima (las capas se mantienen).
- Un token que no es alias cae a su significado normal (cadena compuesta cruda, o cara real en diálogo). El orden completo — **apariencia primero, después la forma del nombre** — está en la sección 3, *El modelo completo: dos preguntas*.
- Las apariencias son **por personaje** (el `feliz` de Sol ≠ el de Luna, aunque los archivos se llamen igual).

### Override local desde `.ecl`

Si un `.ecl` redeclara un personaje del manifest, gana la versión del `.ecl` **localmente para ese script** (no afecta otros scripts ni al manifest).

Útil para escenas especiales (versión glitch de un personaje, versión alternativa, etc).

```
# proyecto.eclips
personaje l {
nombre "Luna"
path "assets/personajes/luna/"
compuesto si
}
```

```
# escena_glitch.ecl
personaje_compuesto l "assets/personajes/luna_glitch/"   # ← override local
escena glitch
l: "?????"   # nameplate sigue diciendo "Luna" si no se override "nombre"
fin
```

El motor avisa en consola cuando detecta un override (por si fue accidental):

```
[EclipScript] Aviso linea 4: personaje compuesto 'l' redefinido en 'escena_glitch.ecl'
(era 'assets/personajes/luna/', ahora 'assets/personajes/luna_glitch/').
Override local OK; revisa si fue intencional.
```

Si la ruta es exactamente la misma que la del manifest, no hay aviso (es redeclaración inofensiva).

### Sin manifest = todo opcional

Cuando `proyecto.eclips` no existe, el motor funciona como antes:

- Los personajes se declaran en cada `.ecl` con `personaje X "ruta"` o `personaje_compuesto X "ruta/"`.
- La nameplate muestra el alias tal cual.
- El título de la ventana viene del parámetro de `Engine::init()` en `main.cpp`.
- Los scripts se buscan en raíz y en `vnscript/` por convención.

Esto preserva el modo "test rápido" — un `.ecl` suelto en la raíz funciona sin armar estructura formal de proyecto.

### Equivalencia bilingüe del nombre

El motor acepta `proyecto.eclips` o `project.eclips` indistintamente. Si existen ambos, gana `proyecto.eclips` y se emite aviso. Mantené uno solo para evitar confusión.

### Bloque `teclas <dominio> {}` / `keys <domain> {}` — remapear el teclado

Cada acción del motor tiene sus teclas de fábrica. Con este bloque las cambiás vos, sin tocar código.

```
teclas vn {
    guardado_rapido F2       # la K se va; ahora guarda la F2
    auto ninguna             # apaga el modo automático
}

keys game {
    interact E ENTER X       # amplía la lista de "interactuar"
}
```

**El dominio es el nombre del bloque**, igual que `estilo X {` o `personaje X {`. Hay dos que podés escribir:

| Dominio | Inglés | Qué controla |
|---|---|---|
| `vn` | (igual) | la novela visual: avanzar, opciones, historial, guardado, pausa |
| `juego` | `game` | el gameplay 2D: moverse, saltar, interactuar, dash, atacar, pausa |

Existe un tercero, `dev` (F8 recargar, F10 vsync, F3 hotspots), y **no es remapeable**: son teclas de desarrollo que no viajan al build que juega tu jugador. Si lo escribís, el motor te lo dice y sigue.

**Las teclas se separan con coma o con espacio** — las dos formas valen, como en `resolucion_diseno 1920 1080`.

#### La regla que sorprende: la acción presente **reemplaza** su lista entera

```
teclas juego {
    interactuar X            # ahora interactuar es SOLO X
}                            # la E y el ENTER de fábrica se fueron
```

No se suma a lo que había: lo reemplaza. Tiene que ser así, o **no habría forma de sacar una tecla**, solo de agregar. Una acción que no nombrás hereda sus teclas de fábrica intactas — el merge es por **acción**, no por bloque ni por dominio.

#### `ninguna` / `none` — dejar una acción sin tecla

```
teclas vn {
    auto ninguna
}
```

Es distinto de no escribir la línea: sin la línea la acción hereda; con `ninguna` queda apagada. Y no se mezcla — `auto A, ninguna` desliga igual, y te avisa.

#### Lo único que el motor no te deja apagar: `menu`

`menu` es la acción de pausa/volver (ESC de fábrica). Si la dejás sin ninguna tecla, **el motor le devuelve el ESC y lo avisa fuerte**:

```
[teclas] RED DE SEGURIDAD: 'menu' quedo sin ninguna tecla en 'vn'.
  Le vuelvo a poner ESC: sin esa accion no hay como salir. Es la
  unica accion con piso garantizado; el resto se puede desligar.
```

No es que no confíe en vos: es que un jugador sin forma de abrir la pausa queda atrapado, y eso no se puede deshacer desde adentro del juego. **Mover** `menu` a otra tecla sí funciona perfecto — lo único que no se permite es dejarla en cero.

#### Cuando algo está mal, el motor te dice qué

Cada error tiene su voz, y todas salen al arrancar, arriba de todo:

| Escribiste | Te dice |
|---|---|
| `guardado_rapdio F2` | `'guardado_rapdio' no es una accion del dominio 'vn'` |
| `interactuar E, PATATA` | `pide la tecla 'PATATA', que no existe` — y la E **sí** se liga |
| `interactuar PATATA` | `quedo sin ninguna tecla valida` — conserva las de fábrica, y te enseña `ninguna` |
| `guardado_rapido: F2` | `Los .eclips no usan ':'` — este archivo no lleva dos puntos |
| la misma acción dos veces | `ya tenia teclas en este bloque. Uso la ultima.` |

Nada de esto rompe el arranque: la línea mala se ignora y el resto del bloque funciona.

#### Los nombres de las teclas

Son **los mismos** que escribís en `tecla:` del comando `qte`, y viven en una sola tabla: la de la sección del QTE, *Los nombres de tecla que podés escribir*. No se repite acá a propósito — una segunda lista de lo mismo se desactualiza, y la que se desactualiza es la que nadie mira.

Lo único que este bloque acepta y el `qte` no es el **mouse**, porque un QTE no te pide un clic:

| Español | English |
|---|---|
| `CLIC` · `CLIC_DER` · `CLIC_MED` | `CLICK` · `RIGHT_CLICK` · `MIDDLE_CLICK` |
| `RUEDA_ARRIBA` · `RUEDA_ABAJO` | `WHEEL_UP` · `WHEEL_DOWN` |

Sí hay una diferencia de trato: el `qte` cae a la tecla `E` cuando no reconoce un nombre, y acá **no se inventa nada** — se avisa y esa tecla no se liga. El `qte` necesita esa red porque su nombre viene de un guion que ya está corriendo; el manifest se lee una vez al arrancar, así que puede decirte el problema y seguir.

#### Si usás Lua: esto cambia lo que le llega a `key_press`

El hook `key_press` recibe **solo las teclas que el motor no consumió**. Al remapear estás cambiando qué consume el motor, así que estás cambiando qué le cae a tu Lua.

Si movés `interactuar` de `E` a `X`, la `E` empieza a llegarle a `key_press` y la `X` deja de llegarle. Es la conducta correcta, pero no es adivinable — si tenés scripts que escuchan teclas por nombre, revisalos después de remapear.

#### Se recarga en vivo con F8

`F8` re-lee el manifest y **re-aplica el bloque entero**, así que podés probar un remapeo sin cerrar el juego. Sacar una línea la saca de verdad: la tabla se recalcula desde cero, no se acumula.

#### `foco_arriba` / `foco_abajo` — navegar las opciones sin mouse

Durante un `elegir`, **las flechas mueven el resaltado y ESPACIO elige el resaltado**. De fábrica:

| Acción | Inglés | Tecla de fábrica |
|---|---|---|
| `foco_arriba` | `focus_up` | `ARRIBA` |
| `foco_abajo` | `focus_down` | `ABAJO` |

Se remapean como cualquier otra — por ejemplo, para el jugador de WASD:

```
teclas vn {
    foco_arriba W
    foco_abajo  S
}
```

Tres cosas que conviene saber al escribir menús con `elegir`:

- **Los números `1`-`4` siguen intactos**, y el anillo se suma a ellos. La diferencia se nota **de la quinta opción en adelante**: hasta ahora esas sólo se podían clickear.
- **El resaltado del teclado y el del mouse son el mismo**, así que se ve igual y suena igual (usa tu `opciones_sonido_hover`). Mover el mouse se lo devuelve al mouse; una flecha se lo devuelve al teclado.
- **El anillo envuelve**: bajar desde la última vuelve a la primera. Y arranca **sin nada resaltado** — quien no toque las flechas ve exactamente lo de siempre.

Con el **historial abierto** las flechas scrollean el log, no el anillo: durante una elección se puede releer antes de decidir.

`foco_izquierda` / `foco_derecha` **ajustan el valor de un `control` focado** (v0.8.34) — no mueven el resaltado, porque un bloque de opciones es una columna y no tiene vecino horizontal. Sobre un slider suben y bajan; sobre un `Toggle` las dos hacen lo mismo (lo invierten).

> *Este párrafo decía **«todavía no hacen nada… reservadas para cuando esa parte llegue»**. Esa parte llegó, y el renglón se quedó dos días diciendo que no. Es la regla de «DOCS documenta sólo código vivo» fallando por el lado inverso —el manual declarando ausente algo que ya funciona—, que se ve mucho menos que una promesa de más.*

### `tecla.<dominio>.<accion>` en `settings.eclips` — el remapeo del JUGADOR (v0.8.34)

Lo de arriba es tuyo, el creador. Esto es de **tu jugador**, y va en su archivo de preferencias (`saves/settings.eclips`), no en tu proyecto:

```
tecla.vn.guardado_rapido=F2
tecla.juego.saltar=ESPACIO,W
key.vn.auto=ninguna
```

Es el mismo vocabulario de siempre —los mismos dominios, las mismas acciones, los mismos nombres de tecla, en los dos idiomas— escrito en el dialecto de ese archivo, que es **plano y con `=`**. Tres partes separadas por punto: `tecla` (o `key`), el dominio, y la acción.

**Gana sobre lo tuyo.** Son tres capas y se resuelven en este orden:

| | |
|---|---|
| **1. fábrica** | la tabla del motor |
| **2. creador** | tu bloque `teclas <dominio> {}` |
| **3. jugador** | sus claves `tecla.*` — **ésta gana** |

Que gane es a propósito: es accesibilidad. Un jugador zurdo, o con una mano, o con un teclado raro tiene que poder arreglar lo que vos elegiste. **Y funciona en las dos direcciones** — si vos apagaste una acción con `ninguna`, él puede volver a ligarla.

**Las mismas reglas que tu bloque, sin excepciones:** la acción presente reemplaza su lista entera · `ninguna`/`none` desliga · una tecla mala no tira la línea (se ligan las buenas y se avisa) · si ninguna sirve, la acción **hereda de fábrica** en vez de quedar apagada · y `menu` tiene el mismo piso garantizado, así que un `tecla.vn.menu=ninguna` recupera el ESC y lo grita.

**Los avisos son los mismos**, y salen al arrancar diciendo `[settings.eclips]` en vez del nombre de tu proyecto, para que tu jugador sepa qué archivo abrir. Hay uno propio de esta forma:

```
[settings.eclips] Aviso: no entiendo la clave 'tecla.vn'.
  Una tecla del jugador se escribe en TRES partes separadas
  por punto: 'tecla.<dominio>.<accion>=<teclas>'.
  Por ejemplo: 'tecla.vn.guardado_rapido=F2'. Linea ignorada.
```

**Dos cosas que conviene saber:**

- **Se lee una vez, al arrancar.** El `F8` recarga *tu* manifest, no el settings de él. Para probar un cambio hay que reabrir el juego.
- **Las mayúsculas no importan** (desde v0.8.56). `Tecla.vn.auto`, `TECLA.VN.AUTO` y `tecla.vn.auto` son **la misma clave**, y `Key.` funciona igual que `key.`. Las tres partes son vocabulario del motor, así que escribí como te salga. *(Hasta v0.8.55 el motor comparaba byte a byte y una mayúscula hacía que la línea se ignorara **sin avisar**.)*

> **Todavía no hay pantalla para esto**, así que hoy se edita a mano. Cuando llegue el widget de controles, va a escribir estas mismas claves: el archivo es el formato, no un rincón temporal.

> **Pero sí podés MOSTRARLE sus teclas ya.** `${_tecla.vn.guardado_rapido}` pinta la tecla que esa acción tiene ahora, después de las tres capas — la misma clave que él edita acá, con un `_` adelante. Sirve para tutoriales que no mienten y para armar tu propia pantalla de controles: **§24, *La tecla de una acción***.

### Bloque `transiciones {}` / `transitions {}` — cómo entran y cómo se van tus pantallas (v0.8.51)

Cuando el motor abre una pantalla —la pausa, guardar, cargar, el juego al empezar— hace un fundido de medio segundo pasando por negro. Ese fundido **siempre existió y nunca fue tuyo**: ni cuándo ocurría, ni cuánto duraba, ni si pasaba por un color sólido. Y cuando una pantalla se iba, no había nada: desaparecía de golpe. Con este bloque las tres cosas pasan a ser tuyas.

```
transiciones {
    abrir    encima  duracion=0.5
    navegar  ninguna
    cerrar   encima  duracion=0.3
}
```

#### Las tres claves

| Clave | Inglés | Cuándo corre |
|---|---|---|
| `abrir` | `open` | una pantalla nace **encima de otra cosa, o de la nada**: el arranque del juego, empezar partida, abrir la pausa desde el juego |
| `navegar` | `navigate` | una pantalla de menú nace **encima de otra pantalla de menú**: de la pausa a guardar, de la pausa a config |
| `cerrar` | `close` | una pantalla **se va**: el jugador eligió `fin`, apretó `ESC`/atrás, o la escena se terminó |

> **`cerrar` no es lo contrario de `abrir`, y por eso es una clave aparte.** `abrir` y `navegar` describen **cómo nace** una pantalla, y el motor los distingue solo mirando qué había debajo. Irse no es nacer: no hay nada que mirar, así que lo elegís vos y punto.
>
> **Y no corre en los tres casos que te dejarían sin diagnóstico:** si tu guion tiene un error (una escena que no existe, un `.ecl` que no parsea) la pantalla se va **de inmediato**, sin fundido — un guion roto que se desvanece lentamente tarda más en avisarte. Tampoco corre cuando el motor desmonta **varias** pantallas de un saque (`titulo`, o volver al juego desde un menú anidado): ahí las que se van no lo pidieron.

**Esa distinción es todo el punto.** Hasta v0.8.49 el motor no la tenía: abrir la pausa desde el juego y abrir guardar **estando ya en la pausa** costaban exactamente lo mismo, así que navegar por tu menú se sentía como abrirlo de cero cada vez.

> Si eso es justo lo que te molestaba, tu arreglo es **una línea**:
> ```
> transiciones {
>     navegar ninguna
> }
> ```
> `abrir` no se escribe y conserva su fundido de siempre.

#### Las transiciones que podés pedir

| Valor | Inglés | Qué hace |
|---|---|---|
| `fade` | `desvanecer` | **velo**: atravesás un negro sólido. El de abajo se corta de golpe. Es el default de `abrir` y `navegar` |
| `encima` | `over` | **se pinta encima**: la pantalla sube su propia opacidad **sobre la de abajo, que se sigue viendo**. No hay nada sólido en el medio |
| `ninguna` | `none` | corte seco, instantáneo. Es el default de `cerrar` |

**`fade` y `encima` no son dos ajustes de lo mismo: son dos mecanismos.** Con `fade` hay un momento en que no ves ni la pantalla que se va ni la que llega, sólo negro. Con `encima` las dos están ahí a la vez, una encima de la otra. Si lo que te molesta es *atravesar un color*, `encima` es la respuesta; si lo que querés es justamente tapar lo de abajo mientras cambia, `fade`.

`duracion=` / `duration=` va en **segundos** y no tiene sentido con `ninguna`. Escribirla ahí igual no es un error: queda guardada y no se usa, así que apagar y volver a prender una transición no te hace perder el número que ya habías elegido.

#### Una cosa de `encima` que conviene saber antes de escribirlo

**`encima` necesita que haya algo debajo.** Cuando no lo hay —el arranque del juego, que es la primera pantalla de todas— no hay nada sobre lo cual pintarse, así que esa pantalla **aparece de golpe**.

Y hay un caso intermedio que te puede sorprender: **empezar partida también es `abrir`** (el juego nace sobre el menú). Con `abrir encima` vas a ver **tu menú detrás del juego** mientras entra. No está roto — es que `abrir` cubre las dos cosas. Si eso te molesta, la salida de hoy es elegir: `abrir encima` (y el arranque va seco) o `abrir fade` (y abrir la pausa pasa por negro).

#### Lo que pasa si no escribís el bloque

**Nada cambia.** Los defaults son exactamente la conducta anterior: `abrir` y `navegar` los dos en `fade duracion=0.5`, y **`cerrar` en `ninguna`**, que es como se fueron las pantallas desde siempre. Un proyecto que no lo nombra se ve igual que siempre. El bloque no cambia el default: **hace que el default deje de ser el único**.

Y las tres claves son independientes: escribir `cerrar` no toca cómo entran tus pantallas, y escribir `abrir` no te agrega una salida que no pediste.

#### Sobre cuáles pantallas manda, exactamente

**Manda sobre las pantallas que vos escribís**, o sea los `.ecl` que registraste en `pantallas {}` (`pausa`, `guardar`, `cargar`, `config`…) más el `.ecl` del menú y el del juego.

**Las pantallas de respaldo que trae el motor —la pausa, la configuración y el menú de guardado en C++, los que aparecen cuando NO registraste el tuyo— no consultan este bloque y nunca tuvieron transición.** Si escribís `transiciones {}` y no ves ningún cambio al abrir la pausa, eso es lo primero que hay que mirar: revisá que tengas `pantallas { pausa "tu_pausa.ecl" }`. No es que el bloque no funcione — es que esa pantalla no es tuya todavía.

#### Un cambio de escena dentro de la misma pantalla no pasa por acá

`ir_a otra_escena` dentro del mismo `.ecl` es instantáneo y siempre lo fue — esto gobierna el cambio de **pantalla**, no de escena. Para la transición entre **fondos** dentro de una escena está el comando `transicion` (**§9, *Transiciones / Transitions***), que es otra cosa y tiene sus propios valores.

#### Todavía no

Ésta el motor **te la rechaza con voz**, y es a propósito: aceptar una palabra que después no hace nada es peor que decirte que todavía no está.

| Escribiste | Por qué no |
|---|---|
| `color=` | el velo sigue siendo **negro**. `encima` no atraviesa nada sólido, así que el único tipo que podría querer un color es `fade`, y todavía no hay nadie que lo haya pedido |

*(`cerrar` y `encima` estaban en esta tabla hasta v0.8.50. Ya no: son código vivo desde v0.8.51.)*

---

## 28. El archivo `meta.eclips` / Persistent meta variables

Variables que **persisten entre partidas** — sobreviven a borrar saves y a empezar de cero. Tercer nivel de persistencia: `settings.eclips` es del jugador, los saves son de una partida, `meta.eclips` es del **juego entero**.

Para qué: rutas completadas, logros propios, galería/CGs desbloqueados, "ya viste la intro", stats entre playthroughs.

**Filosofía:** las variables meta son del *juego*, no del *save*.

### Escribir (explícito)
```
meta_set ruta_luna = true
meta_set actos_vistos = 2
meta_set puntos + 10        # incrementar
meta_set puntos - 5         # decrementar
```
`meta_set` es como `set`, pero la variable se guarda en `meta.eclips` y sobrevive a todo. Acepta `=`, `+ n`, `- n`.

### Leer (transparente)
No hay `meta_get` en EclipScript: leés una var meta como cualquier otra.
```
si ruta_luna == true
  "Solo si completaste la ruta de Luna (en CUALQUIER partida)."
fin_si

"Llevas ${puntos} puntos en total."
```
Si una var meta y una de partida (`set`) se llaman igual, **gana la de partida** al leer (la consola avisa una vez).

### Borrar el progreso
```
meta_borrar      # (o meta_reset)
```
Vacía TODO el meta. Es lo **opuesto** a borrar saves: borrar saves NO toca meta.

### Desde Lua
```
eclips.meta_set("cg_playa", "true")
local p = eclips.meta_get("puntos")
eclips.meta_add("puntos", 10)
eclips.meta_sub("puntos", 5)
```
Lo que Lua escribe en meta, EclipScript lo lee transparente, y al revés.

### Notas
- **Los nombres con `_` son tuyos acá también, con la misma lista de excepciones que en `set`.** Desde **v0.8.48** `meta_set _mi_racha = 3` funciona y persiste; lo único que el motor te reserva son las variables de sistema de §24 (`_hora`, `_saves`, la familia `${_tecla.*}`, `_tema_activo`), y ahí te avisa nombrando tu comando.
  > **Y el aviso de meta te dice DÓNDE habría quedado el dato**, que no es lo mismo que el de `set`: una variable de sistema escrita acá no sólo no se lee — **se queda en `meta.eclips` para siempre**, porque ese archivo sobrevive a borrar los saves. Por eso el texto habla de `meta.eclips` y nunca de la partida guardada.
  >
  > *Hasta v0.8.47 `meta_set` rechazaba **todo** nombre con `_` diciendo «reservado para variables de sistema» — y en `meta.eclips` no vive ninguna. Encima `eclips.meta_set` desde Lua escribía igual, así que la regla se esquivaba por la puerta de al lado. Desde v0.8.48 las dos superficies preguntan la misma ley.*
- `meta.eclips` vive en la raíz, es texto legible, y se escribe atómico (un crash no lo corrompe).
- El motor NO entiende "actos"/"rutas" — vos los armás con `meta_set` + condicionales.

---

## 28.5 Un `.ecl` ANTES del menú (el aviso de contenido)

Un aviso de contenido que se muestra **una sola vez** —la primera que el jugador
abre el juego— y después se olvida. **No hay sintaxis nueva para
esto:** es un patrón que combina tres cosas que ya existen, y el ejemplo completo
está abajo.

**La idea:** la key `menu` del manifiesto apunta al **aviso**, no al menú. El aviso
se marca como visto en `meta.eclips` y encadena al menú real con `cargar_script`,
que reemplaza las escenas **en el mismo state** — no hay parpadeo ni se toca el
stack.

```
# proyecto.eclips
menu "aviso.ecl"
```

```eclips
escena aviso

# La puerta: si ya lo vio, al menu sin mostrar nada.
si aviso_visto == 1
  cargar_script "mi_menu.ecl"
fin_si

ocultar_caja
capa aviso_lienzo "#FFFFFF" z=900 x=0 y=0 w=1920 h=1080
texto aviso_t "Advertencia" x=854 y=300 z=901

hotspot aviso_todo x=0 y=0 w=1920 h=1080
esperar_click:
  "aviso_todo" -> aviso_aceptado

escena aviso_aceptado
meta_set aviso_visto = 1
capa_fade aviso_t      desaparecer 0.6
capa_fade aviso_lienzo desaparecer 0.6
esperar 0.7
cargar_script "mi_menu.ecl"
```

### Las cuatro reglas que hay que respetar

**1. El `si` va con operador y valor.** `si aviso_visto` pelado **no existe** — se
descarta con aviso. La forma es `si aviso_visto == 1`. La primera vez la variable
no existe, resuelve a vacío, y la condición es falsa: seguís de largo.

**2. El hold NO puede ser `esperar`.** Ése es un **timer**, y al expirar cae al
fin-de-script → cierra el juego, porque un pre-menú corre en el fondo del stack. El
hold tiene que ser interactivo: `hotspot` + `esperar_click` (clic en cualquier lado
si el hotspot es de pantalla completa) o `elegir`. Y un `esperar_click` **sin
mapeos no hace nada** — no se emite. *(`esperar` sí es seguro cuando hay un comando
después, como arriba: le da tiempo al fundido antes del `cargar_script`.)*

**3. `cargar_script` NO limpia las capas.** Todo lo que el aviso deje en pantalla
**tapa el menú**. Por eso el aviso se funde a sí mismo antes de saltar. Y como las
capas fundidas **sobreviven con su nombre**, si el menú (o cualquier script
posterior) declara `capa` con un id que el aviso ya usó, esa capa **hereda el
invisible y no se ve, sin aviso**. → **prefijá los ids del pre-menú** (`aviso_*`).

**4. `meta_set` va ANTES del fundido.** Si el jugador cierra la ventana a mitad de
la transición, el aviso ya quedó marcado: no lo vuelve a ver por impaciente.

### Para volver a verlo mientras lo probás

`meta.eclips` es texto plano en la raíz: borrá la línea `aviso_visto=1`. O
`meta_borrar` (que vacía **todo** el progreso permanente).

### Notas

- El aviso **no necesita `fondo`**: el motor limpia a negro, así que fundir el
  lienzo revela negro y el menú entra encima.
- Sirve para cualquier cosa antes del menú, no sólo un aviso: un logo, un prólogo
  jugable, una pantalla de idioma. Es un `.ecl` normal con el vocabulario completo.
- El texto de un `texto` se dibuja **desde `x` hacia la derecha**: no hay clave de
  alineación, así que para centrar elegí el `x` a mano.

---

## 29. Idiomas y Traducciones / Languages & Translations

> **Todo esto funciona desde v0.8.40.** El motor lee paquetes `.lang`,
> declara un catálogo por proyecto, elige un idioma efectivo, lo expone al
> guion, sustituye el texto visible en las puertas de la VN, traduce sus
> propias pantallas, sus fechas y los nombres de tecla, **y también
> cambia de ARCHIVO: imágenes, fuentes y sonidos pueden tener una versión por
> idioma** (el árbol `idiomas/<código>/`). Para las fechas hay una clave de tema,
> `ranura_fecha_idioma`/`slot_date_language`, y dos tokens, `{mes_nombre}` y
> `{dia_nombre}`.

### Declarar el catálogo

En `proyecto.eclips` o `project.eclips`, escribí los códigos sin comillas y en
el orden deseado. El primero es siempre el idioma original:

```eclips
idiomas es en ja
# English spelling:
# languages es en ja
```

Las dos keys son equivalentes. Sin ninguna de ellas, el sistema queda apagado:
no se crea un catálogo en memoria, no se buscan archivos y no aparecen logs de
idiomas. Un proyecto anterior se comporta exactamente igual que antes.

#### Ver los cambios sin reiniciar: **F8** *(v0.8.41)*

El hot-reload del proyecto también re-resuelve el idioma. Con el juego abierto:

- **Agregás o sacás un código** de `idiomas` → F8, y el catálogo nuevo queda activo.
- **Editás un `.lang`, o agregás uno nuevo**, sin tocar `proyecto.eclips` → F8, y las
  traducciones se releen igual. Es el caso más común mientras traducís, y es la razón
  de que F8 recargue **siempre**, aunque el catálogo no haya cambiado.
- **Borrás la key entera** → F8, y el sistema queda apagado: todo vuelve al texto
  original, como en un proyecto que nunca la declaró.
- **Sacás el idioma que estabas usando** → F8, y el motor cae al original y te lo
  dice en consola, con la misma línea que usa al arrancar.

Cada recarga **que encontró archivos** imprime su recibo, así que sabés que la tecla
hizo algo:

```text
[Eclips] en: 12 de 14 traducciones (1 archivo)
```

> **Y al revés: no ver el recibo no siempre es una falla.** Si el idioma activo no
> tiene ningún `.lang` —o si borraste la key y el sistema quedó apagado— no hay nada
> que reportar y el motor calla. El recibo dice *«cargué esto»*, no *«la tecla
> funcionó»*.

> **Lo único que F8 no hace, para que no lo busques como un bug:** no vuelve a
> ejecutar los comandos de la escena que ya está en pantalla. Si agregaste `idiomas`
> **ahora** y tu pantalla de configuración tiene un `control idioma`, ese control no
> va a aparecer hasta que **vuelvas a entrar** a la escena — cuando se cargó, todavía
> no había catálogo. El texto ya traducido sí cambia al re-entrar, que es la política
> de display-time de siempre.
>
> F8 es del **Modo Creador**: es tu tecla mientras escribís, no algo que el jugador
> vea.

### Escribir un paquete `.lang`

La raíz tiene una sola grafía: `idiomas/`. Para `en`, podés usar un archivo
único, una carpeta de archivos, o ambos:

```text
idiomas/
  en.lang
  en/
    dialogos.lang
    menu.lang
```

El archivo único carga primero. Los `.lang` de la carpeta cargan después, en
orden alfabético; si una clave se repite, gana la última y el motor avisa.
El formato es UTF-8 y conserva el texto original del `.ecl` como clave:

```lang
# Los comentarios ocupan su propia línea.
"Hola" = "Hello"
"Presioná ${_tecla.vn.avanzar}" = "Press ${_key.vn.advance}"
"Ella dijo "hola # mundo" y se fue." = "She said "hello # world" and left."
"Pendiente" = ""
```

Las comillas internas y `#` dentro del campo son contenido. No existe el
comentario al final de un registro. Una línea ambigua o malformada avisa con
archivo y número, se ignora y la carga continúa. Si falta la clave, su valor
está vacío o el paquete completo no existe, el motor devuelve el original.

### Traducir un asset: la carpeta espejo

No sólo el texto cambia de idioma. Si tenés una imagen con letras adentro, una
fuente con otros glifos o una voz doblada, poné el archivo bajo
`idiomas/<código>/` **repitiendo su ruta tal cual**, y el motor lo elige solo:

```text
assets/bg/aula.png              <- el original (el idioma del guion)
idiomas/en/assets/bg/aula.png   <- la version inglesa, MISMA ruta relativa
```

**El guion no cambia una letra.** Seguís escribiendo la ruta original:

```eclips
capa fondo "assets/bg/aula.png" z=0 x=0 y=0 w=1920 h=1080
```

Vale para las tres familias: **imágenes** (todo lo que carga una `capa`, un
sprite o un fondo), **fuentes** (`.ttf` declarados con `fuente`) y **audio**
(`musica` y `sonido`).

Y los **mapas 2D**: un `.eclmap` traducido va en `idiomas/<código>/` con la
misma ruta (`idiomas/en/vnscript/nivel1.eclmap`), y `gameplay` lo abre solo.
Su `lua:` sigue siendo el del original, el mismo archivo para todos los idiomas.
Por eso **un nombre que el lua compara no se traduce**: si el lua pregunta por
`"Copia"`, el mapa en inglés sigue diciendo `"Copia"`.

Las reglas, y son pocas:

- **Si el archivo espejo no existe, se usa el original.** No es un error y no
  avisa: un juego traducido a medias se juega entero. Traducí sólo lo que
  necesites — tres imágenes de trescientas no generan doscientos noventa y siete
  avisos.
- **En el idioma original no se busca nada.** Sus archivos ya son los de siempre.
- **La ruta espejo es relativa a la raíz del proyecto**, igual que la original.
  Una ruta absoluta, o una que salga de la raíz con `..`, no se espeja.
- **Un asset ya cargado no cambia al vuelo.** Si el idioma cambia mientras jugás,
  las imágenes en pantalla siguen siendo las viejas hasta el próximo cambio de
  escena o re-entrada de menú. El texto sí cambia al instante.

La carpeta es la **misma** donde viven los `.lang`, y no se pisan: los paquetes
de traducción son los `.lang` sueltos en `idiomas/<código>/`, y todo lo que esté
en subcarpetas es el árbol de assets.

```text
idiomas/
  en.lang                          <- paquete (archivo unico)
  en/
    menu.lang                      <- paquete (carpeta)
    assets/bg/aula.png             <- asset espejo
    assets/fuentes/titulo.ttf      <- asset espejo
```

**Dos límites honestos:** el fondo del menú de título, los sprites del gameplay
2D y las fuentes de las pantallas internas del motor todavía **no** pasan por el
espejo — cargan por un camino propio. Y si el idioma no tiene su archivo, no hay
aviso: es silencio a propósito, así que revisá la ruta si esperabas ver el cambio.

### Consultar el idioma efectivo

El código activo está disponible como `${_idioma}` y `${_language}`. Sirve en
interpolaciones y condiciones igual que las demás variables de sistema. Lua lo
lee mediante `eclips.get("_idioma")` o `eclips.get("_language")`; `eclips.get`
no obtiene por esa vía variables meta ni variables normales ajenas a su API.

La preferencia del jugador se guarda como `lang=<código>` en
`saves/settings.eclips`. Se elige con `control idioma` (sección 14.10), y también
puede editarse esa línea para probar otro paquete. Si el código guardado
ya no figura en el catálogo, el motor avisa y usa el primer idioma, pero conserva
la preferencia cruda en el archivo para no borrar una elección recuperable.

### Qué se traduce y en qué orden

El motor consulta el paquete al mostrar:

- diálogos y narración, incluidas las salidas de `azar:`;
- labels de `elegir`, prompts de `entrada` y contenido de capas `texto`;
- el nombre visible del hablante, después de resolver alias → nombre del
  manifest; el alias interno no cambia;
- el `msg` entregado por Lua a `eclips.notify(msg)`.

Los textos de guion siguen usando como clave el contenido crudo del `.ecl`.
Primero se traduce, después se interpolan `${variables}` y finalmente se
interpretan los tags rich-text. Por ejemplo:

```lang
"Hola ${_idioma}" = "Hello ${_language}"
```

Con `lang=en`, esa línea muestra `Hello en`. Si el store no existe o no encuentra
la clave, conserva el original. El historial y el preview reciben el texto ya
traducido que vio el jugador; las claves de línea vista se calculan todavía sobre
speaker y texto crudos, así que cambiar de idioma no vuelve una línea «nueva».

`eclips.notify` traduce sólo el mensaje enviado por Lua. Los toasts y strings
internos del motor —por ejemplo, `Partida cargada`— entran por otra puerta, la de
abajo.

### El texto del propio motor (desde v0.8.39)

Además de tu guion, ahora se traducen **las pantallas del motor**: menú principal,
pausa, configuración, fin de nivel, guardar/cargar, historial, el hint del QTE y el
nombre de cada tecla.

**Y no hay un segundo formato que aprender: la clave es el español de fábrica**, igual
que la clave de tu guion es el texto original. En tu `.lang`:

```lang
"Reanudar" = "Resume"
"Guardar" = "Save"
"Configuracion" = "Settings"
"Eliminar ranura {ranura}?" = "Delete slot {ranura}?"
```

El motor trae `es` e `inglés` de fábrica, así que **si tu juego es es→en no necesitás
escribir ninguna de esas líneas**: ya están. Se escriben para un **tercer** idioma.

> **Las llaves `{...}` de esas frases hay que conservarlas.** Son huecos que el motor
> rellena: `{ranura}` `{segundos}` `{escena}` `{mapa}` `{hp}` `{hp_max}` `{ancho}`
> `{alto}`. **Podés moverlos de lugar** —para eso son nombrados—, pero si borrás uno,
> ese dato desaparece de la pantalla. Un hueco que escribas mal sale literal
> (`{xyz}` → `{xyz}`), así que se caza a ojo.

**Dos avisos honestos sobre el alcance:**

- **La consola no se traduce** —los avisos del parser, los diagnósticos y los mensajes
  de error siguen en la grafía en que vos escribiste—, y es a propósito: esos los leés
  vos, no tu jugador.
- **El nombre de una tecla se muestra traducido pero se guarda canónico.** En pantalla
  vas a ver `SPACE` con el juego en inglés, y en `saves/settings.eclips` va a seguir
  diciendo `ESPACIO`. No es un bug: el archivo es el mismo para todos los idiomas.

### Límites actuales

- **F8 recarga idiomas desde v0.8.41.** Catálogo, `.lang` nuevos y el apagado al
  borrar la key — ver «Ver los cambios sin reiniciar: **F8**» más arriba. Lo
  único que no hace es re-ejecutar los comandos de la escena que ya está en
  pantalla, así que un `control idioma` recién agregado aparece al re-entrar.

---

## Apéndice A — Samples ≠ API estable / Samples ≠ stable API

Algunos `.ecl` de demostración en el árbol del motor usan formas que **el parser acepta** pero que **este manual no promete**. Si copiás un sample al pie de la letra, podés creer que esas palabras son parte del lenguaje documentado. No lo son.

Some demo `.ecl` files in the engine tree use forms the **parser accepts** but that **this manual does not promise**. Copying a sample verbatim can make those words look like documented language. They are not.

### Regla

| | Contrato del manual | Sample / demo |
|--|---------------------|---------------|
| ¿Aparece en la **Tabla de Comandos** o en una sección numerada con semántica? | Sí → podés depender de ello | — |
| ¿Solo aparece en un `.ecl` de prueba / intro? | — | Tratarlo como **ejemplo local**, no como API |

Si una forma no está en la tabla ni en las secciones de este archivo, **no asumas** que es estable, completa, ni que se mantendrá. Puede vivir en el parser para demos internas.

If a form is not in the command table and not in this file's numbered sections, **do not assume** it is stable, complete, or kept. It may exist in the parser for internal demos.

### Tres trampas concretas (vistas en guiones de prueba del motor)

#### 1. `narrador:` no es narración

La **narración** documentada (§7) es una línea que **empieza con comillas**, sin nameplate:

```
"El viento movía las cortinas."
```

En la referencia rápida eso aparece como `"texto narrador"` — significa *texto de narrador*, **no** un comando ni un hablante mágico llamado `narrador`.

Escribir:

```
narrador: "El viento movía las cortinas."
```

hace que el motor trate `narrador` como un **alias de personaje** cualquiera: suele pintar nameplate con la palabra «narrador» en vez de narración real. No hay keyword `narrador` en el motor.

#### 2. `iniciar_batalla` — demo, no referencia

Es un resto de las pruebas del motor (p. ej. `iniciar_batalla estrella hp:5`). **No** está en la Tabla de Comandos ni en ninguna sección de este manual. Si el motor se compiló con la batalla de muestra (`BulletHellState.h`), la abre; la copia publicada no la trae, así que ahí sólo avisa en consola, deja `_batalla_ganada` en `0` y el guion sigue. **No** lo copies a un juego nuevo esperando contrato estable ni parámetros documentados.

#### 3. `ir_a_pasillo` — no es `ir_a`

`ir_a` / `goto` **sí** está documentado (§13 Navegación + Tabla de Comandos).  
`ir_a_pasillo …` es **otra** forma, usada en demos de corredor; **no** está en este manual. No la confundas con el salto de escenas.

### Lo que SÍ está documentado (para no sobregirar)

`gameplay topdown` / `gameplay sidescroll` **sí** forman parte del journey documentado (p. ej. controles en §17 *Teclado del gameplay 2D*; mapas en `VOCABULARIO_2D_ECLIPMAP.md`, el manual del 2D). Verlos en un sample **no** los pone en esta lista de trampas.

### Cómo leer un sample de ahora en más

1. Abrí la **Tabla de Comandos**. Si el verbo no está, es sospechoso.
2. Buscá la sección numerada. Si no hay semántica acá, es demo.
3. Preferí los ejemplos de **este** archivo (§1, §19, §0) sobre intros del repo cuando aprendas el lenguaje.

Cuando una demo se vuelva API estable, **entra a este manual** (tabla + sección) — no al revés.

---

## Notas Técnicas

- **RAM:** El Asset Manager con LRU cache mantiene un máximo de 32 texturas en memoria simultáneamente. Las texturas no usadas se descargan automáticamente.
- **Resolución virtual:** 1920×1080. Todo se escala automáticamente a la ventana real con letterboxing.
- **Hot-reload (F8):** Recarga en caliente **texturas** y **tema** sin recompilar ni salir al menú. Tres dominios distintos:
  - **Imágenes (PNG):** repinta un sprite o fondo y pulsa **F8** → se actualiza en pantalla al instante. ⚠️ **Importante con Aseprite/Photoshop:** F8 recarga el archivo **`.png` exportado**, no el proyecto nativo. Guardar el `.aseprite`/`.psd` (Ctrl+S) NO basta — debes **exportar el PNG** (en Aseprite: *Export* / "Export Again" / Ctrl+Alt+Shift+S). Si F8 no muestra el cambio, casi siempre es que el PNG no se re-exportó.
  - **Tema (`.eclips`):** edita caja, nameplate, colores o bordes 9-slice y pulsa F8 → se re-aplica en vivo.
  - **Script (`.ecl`):** cambiar qué se muestra (`fondo`, `mostrar`, diálogos) NO se recarga con F8 — el script ya está en ejecución. Para verlo, recarga la partida (L / F9) o reentra desde el menú.
- **Formatos de imagen:** PNG (recomendado, con transparencia), JPG, BMP vía stb_image.
- **Formatos de audio:** OGG, WAV, MP3, FLAC vía miniaudio.
- **Encoding:** UTF-8. Acentos y caracteres especiales funcionan en diálogos.
- **FBO Compositing:** Los personajes compuestos se renderizan mediante Framebuffer Objects a resolución nativa para evitar artefactos visuales (costuras entre capas) al escalar la ventana.
