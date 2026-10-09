# Vocabulario 2D — Manual del creador de .eclmap

Este manual describe los campos que lee el motor y los límites de su ejecución. Los ejemplos de mapa usan una sola grafía por clave. Las barras en tablas o prosa separan alternativas: no se escriben como parte de la clave.

Un fragmento de definición necesita un mapa y las entidades o recursos que referencia; los archivos de assets de los ejemplos son rutas que debes aportar. Los bloques Lua con variables como `placed_id` son fragmentos de integración: tu juego debe obtener esos IDs y decidir cuándo ejecutar las operaciones.

Pon los comentarios `#` en renglones propios; no los añadas detrás de un valor o de una cabecera de evento. El lector no retira esos sufijos de forma uniforme. Los colores hexadecimales y el carácter de pared `#` son datos, no comentarios.

Usa cabeceras en minúsculas, espacios de indentación y las grafías indicadas. `daño`/`dano` y `tamaño`/`tamano` se aceptan; `posicion` no lleva tilde. No deduzcas otras variantes: que una palabra se entienda en español no implica que el parser la reconozca.

El loader puede omitir datos sin avisar. Revisa modo, entidades, coordenadas y oleadas cargadas; una carga sin avisos no certifica el ejemplo. No existe un bloque raíz `camara:`: usa acciones o la API Lua (§1 y §7).

Guardar/cargar está implementado (§16). Las APIs de actividades de §11 mantienen sus contratos por tipo de entidad; no hay una API universal de actor ni un inventario utilizable por tecla.

---

## Cómo leer los ejemplos

1. La raíz configura el nivel; los campos indentados pertenecen a su bloque.
2. `tamaño` en la raíz es el lado del jugador. Los tiles del runtime miden 60 píxeles.
3. El grid sitúa al jugador con `P`, entidades con su letra y tiles con su carácter.
4. Una definición aislada no coloca la entidad: necesita una aparición en el grid o un spawn.
5. Lua añade lógica mediante los bindings documentados en §12. No convierte cualquier idea de juego en una función nativa.

---

## Anatomía de un .eclmap

```
# ── Configuración ──
(config del nivel: modo, música, jugador, cámara, HUD)

# ── Tiles custom (opcional) ──
(definir qué aspecto y propiedades tiene cada carácter del mapa)

# ── Rutas (opcional) ──
(caminos predefinidos para enemigos, cutscenes, waypoints)

# ── Marcadores espaciales (opcional) ──
(regiones identificadas en píxeles de mundo, independientes del grid)

# ── Habilidades reutilizables (opcional) ──
(definiciones raíz que los actores conectan mediante usos nombrados)

# ── Mapa ──
(grid ASCII de tamaño libre)

# ── Entidades ──
(definición de NPCs, enemigos, items, triggers, torres, etc.)

# ── Oleadas (opcional) ──
(sistema de waves para tower defense, survival, arenas)
```

---

## 1. CONFIGURACIÓN DEL NIVEL

### Configuración básica
```
# topdown | sidescroll
modo: topdown
# Música de fondo (loop)
musica: "assets/bosque.ogg"
# Fuente del HUD
fuente: "assets/fuente.ttf"
# Velocidad del jugador (px/s)
velocidad: 400
# Lado del jugador (px); el tile mide 60 px
tamaño: 48
# Condición de salida del nivel
salida: hablar_todos
# Script Lua global del nivel
lua: "logica.lua"
```

### Configuración del jugador
```
jugador:
  # Carpeta con sprites direccionales
  sprite: "assets/player/"
  # Ritmo de ciclos numerados (0 < fps <= 240)
  fps: 8
  # Puntos de vida (default: sin HP = inmortal)
  hp: 5
  # ¿Recibe daño al tocar enemigos? (default: true)
  daño_contacto: false
  # Segundos de invencibilidad tras recibir daño
  invencible: 1.5
  # ¿Tiene ataque melee? (default: false)
  puede_atacar: true
  # Alcance del ataque en px
  ataque_rango: 60
  # Daño del ataque
  ataque_daño: 1
  # Segundos entre ataques
  ataque_cooldown: 0.3
```

Si no ponemos un bloque `jugador:`, el jugador es un cuadrado de color, inmortal y sin ataque. Puede caminar y hablar. Cada campo que agregamos activa esa mecánica.

#### Frames direccionales del jugador

La carpeta de `sprite:` conserva sus cinco PNG base:

```text
idle.png  up.png  down.png  left.png  right.png
```

Cada dirección puede añadir, de forma independiente, un ciclo automático:

```text
left_1.png  left_2.png  ...  left_N.png
```

- Cero numerados mantiene el PNG base estático.
- Dos o más numerados, contiguos desde `_1`, forman el ciclo en orden numérico.
- Un único numerado, un hueco o una grafía no canónica invalida sólo esa
  dirección, avisa y vuelve a su PNG base.
- El PNG base no forma parte del ciclo: es el fallback estable y sigue siendo
  obligatorio.
- `fps:` usa `8` por defecto. Sólo acepta un número finito con
  `0 < fps <= 240`; un valor inválido avisa y conserva el default.
- Todos los frames numerados de una dirección deben usar el mismo lienzo que
  su PNG base. Un lienzo incompatible invalida el ciclo antes de publicarlo.
- En top-down, la intención de `mover_x/mover_y` elige izquierda, derecha,
  arriba o abajo; una tecla sostenida continúa animando aunque una pared
  bloquee el desplazamiento.
- En side-scroll, suelo + eje horizontal eligen izquierda/derecha; en el aire,
  velocidad vertical negativa elige `up` y desde el ápice/caída se usa `down`.
- Entrada, puerta y retry reinician en `idle`, frame 1. Pausa y fades congelan
  la fase; save no serializa el cuadro visual.

Estos ciclos automáticos sólo funcionan para el jugador. Las animaciones
nombradas, los sprites de enemigos, torres y proyectiles, los offsets y pivotes,
los filtros de pixel art y las hojas de sprites en cuadrícula no están cubiertos por este sistema.

### Configuración de side-scroll
```
# Fuerza de gravedad (px/s², default: 1800)
gravedad: 1800
# Fuerza de salto (px/s, default: 700)
salto: 700
# Coyote time en segundos (default: 0.12)
coyote: 0.12
# ¿Permitido? (default: false)
doble_salto: false
# ¿Tiene dash? (default: false)
dash: false
# Velocidad del dash
dash_velocidad: 1200
# Duración del dash
dash_duracion: 0.15
# ¿Se agarra de paredes? (default: false)
wall_slide: false
# Salto contra pared (default: true)
wall_jump: true
# Pausa entre dash (segundos)
dash_cooldown: 0.4
```

### Cámara desde Lua
```lua
-- En el script Lua del nivel:
world.camera_pan(8, 4, 1.5) -- destino en tiles; duración en segundos
-- world.camera_follow() devuelve el seguimiento cuando tu lógica lo decida.
world.camera_zoom(1.2, 0.5)
world.camera_shake(0.5)
```

### HUD
```
hud:
  # Muestra barra de HP del jugador
  hp: visible
  # Muestra HP de la base (Tower Defense)
  hp_base: visible
  # Texto fijo en el HUD
  texto: "Nivel 1 - El Bosque"
  # Muestra "Oleada 2/5" (para tower defense)
  oleada: visible
  # Variable + formato para mostrar
  contador: "monedas" "Monedas: {}"
```

`contador/counter` es una vista de solo lectura sobre una variable compartida:

- el formato requiere **exactamente un** placeholder literal `{}`;
- el valor se presenta como string, sin convertirlo ni apropiarse de su tipo;
- una variable ausente o vacía se muestra como `0`;
- cero o varios `{}` desactivan esa línea y producen un aviso del loader;
- el bloque admite un contador y el runtime lo coloca en el cuarto slot de la
  banda inferior, después de posición, NPCs y enemigos.

Si no pones bloque `hud:`, no se agrega ningún elemento **declarativo**. Los
indicadores de controles, posición y dirección continúan dibujándose:
se dibujan por separado y no los configura el bloque `hud:`.

---

## 2. TILES CUSTOM

El mapa ASCII usa caracteres. Por defecto:
- `#` = pared sólida (gris)
- `.` = suelo (vacío, caminable)
- `=` = pared decorativa (azul)
- `B` = caja/obstáculo
- `P` = posición del jugador
- `>` = tile de salida
- `A-Z` = entidades (NPCs, enemigos, items, torres...)

### Tipos de tile disponibles
- `solido/solid` — pared (default para `#`)
- `suelo/floor` — caminable (default para `.`)
- `daño/damage` — aplica al menos 1 HP por tick mientras el jugador permanece encima; el runtime usa ticks de 0.5 segundos
- `resbaloso/slippery` — derrape con momentum (hielo)
- `lento/slow` — reduce velocidad del jugador
- `plataforma/platform` — sólida desde arriba, atravesable desde abajo (side-scroll)
- `checkpoint` — punto de guardado
- `puerta/door` — transición a otro nivel
- `empujable/pushable` — bloque que el jugador mueve al caminar contra él
- `base` — **punto crítico de Tower Defense**, enemigos lo atacan
- `spawn` — marca un tile como punto de aparición; la oleada debe indicar su coordenada con `en COL, ROW`
- `slot_torre/tower_slot` — **tile válido para colocar torres**

### Definir tiles custom
```
tile ~ "assets/agua.png":
  tipo: daño
  # Daño por tick (cada 0.5 s mientras permanece encima)
  daño: 1
  # El runtime aplica este factor sólo a tipo lento
  velocidad: 0.5

tile - "assets/hielo.png":
  tipo: resbaloso
  # Factor de fricción (default: 0.3 para hielo)
  friccion: 0.05

tile ^ "assets/pincho.png":
  tipo: daño
  daño: 2

tile + "assets/checkpoint.png":
  tipo: checkpoint
  # SFX al activar
  sonido: "assets/save.ogg"

tile < "assets/puerta.png":
  tipo: puerta
  # Carga otro nivel
  destino: "nivel2.eclmap"
  # Posición de spawn en el destino (en tiles)
  posicion: 5, 3
  # Variable requerida (opcional)
  condicion: tiene_llave == 1
  # Mensaje si no cumple condición
  cerrada: "Necesitas la llave."
```

### Tiles de Tower Defense

**Base (`@` por convención) — el objetivo que defender:**
```
tile @ "assets/base.png":
  tipo: base
  # Puntos de vida de la base
  hp: 10
  # Acciones cuando la base cae
  al_destruir:
    notificar: "¡La base ha caído!"
    lua: "game_over.lua"
```

Cuando un enemigo alcanza la base, se retira por llegada y aplica su daño: no ejecuta una muerte con recompensa. Una base con HP que llega a cero dispara `al_destruir`, luego `base_destroyed` y resuelve derrota. Un hook puede llamar `world.exit()` para gestionar esa salida por su cuenta (§12).

**Spawn (`*` por convención) — punto de aparición de oleadas:**
```
tile * "assets/portal.png":
  tipo: spawn
```

Las oleadas no buscan ni reparten enemigos entre los `*`. Para usar ese punto, escribe `spawnear: E 3 en COL, ROW` con su coordenada. `aleatorio` elige suelo, haya o no tiles spawn.

**Tower slot (`%` por convención) — dónde se pueden colocar torres:**
```
tile % "assets/torre_slot.png":
  tipo: slot_torre
```

Si una torre tiene `donde: slot_torre`, solo se coloca sobre estos tiles. Si la torre no lo especifica, se puede colocar en cualquier tile caminable (como en los juegos de defensa en cuadrícula, donde sirve cualquier celda del jardín).

### Tiles caminables con textura
```
# Suelo con textura (sin bloque de propiedades)
tile . "assets/pasto.png"
# Pared con textura
tile # "assets/ladrillo.png"
```

### Plataforma one-way (side-scroll)
```
tile - "assets/plataforma.png":
  # Sólida desde arriba, atravesable desde abajo
  tipo: plataforma
```

Nota: los caracteres para tiles custom son libres. Puedes usar `~`, `-`, `^`, `+`, `<`, `*`, `%`, `@`, `:`, `;`, etc. — cualquier carácter ASCII que no sea letra A-Z, `#`, `.`, `=`, `B`, `P`, `>`.

---

## 3. ENTIDADES

Todo lo que se define con una letra en el mapa (A-Z excepto B y P) es una **entidad**.
El tipo de entidad se define en su bloque de definición.

> ⚠ **`sprite:` hoy sólo dibuja al jugador.** El motor usa imagen **únicamente** para el
> jugador: la carpeta de su `sprite:` y su `ataque_sprite`. En NPC, enemigos, ítems,
> proyectiles, torres y tiles custom, `sprite:` **se lee y no se usa**: se dibujan como formas
> de color. Los `sprite:` de los ejemplos de este documento describen el diseño, no lo que se
> ve hoy. *(El único que dibuja un `spritePath` es el jugador, en `loadPlayerPresentation` de
> `Gameplay2DState.h`; los de `MapData.h` para las demás entidades no tienen quién los
> lea.)*

### 3.1 NPC
```
npc A "Luna" #9966BB:
  set: afecto + 1
  "Hola! Soy Luna."
  elegir:
    "Hola" ->
      "Que tal!"
```

### 3.2 Enemigo
```
enemigo E "Slime" #00FF00:
  sprite: "assets/enemies/slime/"
  hp: 3
  # Daño al contacto con el jugador
  daño: 1
  # Velocidad de movimiento
  velocidad: 100
  # Tipo de movimiento + parámetro
  movimiento: patrulla_h 3
  # Acciones al morir (bloque indentado)
  al_morir:
    set: slimes_muertos + 1
    sonido: "assets/boom.ogg"
    # Letra del item Moneda definido más abajo
    soltar: M
```

**Tipos de movimiento disponibles:**
```
enemigo A "Movimiento 1":
  # No se mueve (torreta, trampa)
  movimiento: quieto

enemigo B "Movimiento 2":
  # Patrulla horizontal, 3 tiles
  movimiento: patrulla_h 3

enemigo C "Movimiento 3":
  # Patrulla vertical, 4 tiles
  movimiento: patrulla_v 4

enemigo D "Movimiento 4":
  # Sigue al jugador si está a ≤200px
  movimiento: seguir 200

enemigo E "Movimiento 5":
  # Huye del jugador si está a ≤200px
  movimiento: huir 200

enemigo F "Movimiento 6":
  # Orbita su posición, radio 80px
  movimiento: circular 80

enemigo G "Movimiento 7":
  # Sigue una ruta predefinida (ver sección 4)
  movimiento: ruta camino1

enemigo H "Movimiento 8":
  # Rebota en paredes, como en un laberinto de arcade
  movimiento: rebote

enemigo I "Movimiento 9":
  # Se mueve a posición random cada 2s
  movimiento: aleatorio 2
```

**Propiedades extra de combate:**
```
enemigo E "Torreta":
  # Distancia de detección del jugador
  rango: 150
  # Dispara cada 1.5 segundos (requiere proyectil)
  cadencia: 1.5
  # Nombre de un proyectil definido en el nivel
  proyectil: "bola_fuego"
  # Fuerza de empuje al golpear (0 = sin empuje)
  knockback: 300
  # ¿Es inmune a daño? (default: false)
  invulnerable: false
```

`cadencia/fire_rate` y `proyectil/projectile`, escritos directamente en el
enemigo, configuran un único disparo al jugador. Al cargar, el motor crea un uso
`legacy_attack`; no ejecuta otro ciclo de disparo por separado.

### 3.3 Item

Los items se recogen al entrar en contacto. La poción de abajo aplica `set: hp + 3` durante el pickup, limitado al HP máximo del jugador. El parser conserva `tipo: inventario` y `al_usar`, pero el runtime actual no ofrece el uso de ese inventario: no los emplees para prometer una tecla de consumir.
```
item I "Llave" #FFD700:
  sprite: "assets/items/llave.png"
  al_recoger:
    set: tiene_llave = 1
    sonido: "assets/pickup.ogg"
    notificar: "Obtuviste la Llave!"
    # El item desaparece (default: true)
    desaparecer: true
```

**Poción de curación al recoger:**
```
item K "Poción" #FF6688:
  sprite: "assets/items/pocion.png"
  al_recoger:
    set: hp + 3
    sonido: "assets/heal.ogg"
    notificar: "Poción recogida"
```

**Item como moneda (auto-pickup sin inventario):**
```
item M "Moneda" #FFD700:
  sprite: "assets/items/moneda.png"
  al_recoger:
    set: monedas + 1
    sonido: "assets/coin.ogg"
```

### 3.4 Trigger (entidad invisible)
```
trigger T:
  # Área del trigger (ancho, alto en px)
  tamaño: 96, 96
  al_entrar:
    # aviso flotante, no caja
    dialogo: "¡Cuidado, es una trampa!"
    sonido: "assets/alarma.ogg"
    musica: "assets/battle.ogg"
    # Spawnear 3 enemigos E en posiciones aleatorias
    spawnear: E 3 aleatorio
    set: trampa_activada = 1
  # Solo se activa una vez (default: false)
  una_vez: true
```

**Trigger de salida (cambio de mapa):**
```
trigger S:
  tamaño: 48, 96
  al_entrar:
    # Nivel destino + posición de spawn
    cargar_mapa: "nivel2.eclmap" 5, 3
```

**Trigger de cutscene:**
```
trigger C:
  tamaño: 192, 48
  una_vez: true
  al_entrar:
    # Destino en tiles, duración en segundos
    camara: pan 8, 4 1.5
    # Spawn de acción en píxeles
    spawnear: E 1 en 400, 200
    # Aviso flotante
    dialogo: "¿Qué fue eso?!"
# Las acciones se ejecutan en la misma llamada; no esperan a que termine el pan.
```

**Trigger condicional:**
```
trigger D:
  condicion: tiene_llave == 1
  al_entrar:
    # aviso flotante, no caja
    dialogo: "La puerta se abre..."
    sonido: "assets/puerta.ogg"
    # Eliminar un tile de pared (coordenada en tiles)
    destruir_tile: 15, 8
```

### 3.4.1 Marcador espacial neutral

Un marcador identifica una región del mundo sin asignarle una consecuencia:

```text
marcador paso_central:
  rectangulo: 240, 180, 60, 60

marker central_pass:
  rectangle: 240, 180, 60, 60
```

`rectangulo/rectangle` recibe `x, y, ancho, alto` en píxeles de mundo. Los
cuatro valores deben ser finitos; ancho y alto deben ser positivos. El ID
(`paso_central`) es un token desnudo, no una letra del grid, y debe ser único
dentro del mapa. Si se repite, se conserva la primera declaración con aviso.

Cuando un enemigo entra o atraviesa la región, el runtime publica:

```lua
world.on("marker_enter", function(marker_id, actor_kind, actor_id)
    -- actor_kind siempre es "enemy": este evento sólo observa enemigos.
end)
```

La entrada se confirma una vez por cruce y por actor. Permanecer dentro no
repite y salir rearma. Un cruce rápido se detecta aunque el actor termine
completamente al otro lado. Tocar sólo el borde no cuenta.

Nacer dentro también cuenta en el primer tick observado, aunque el enemigo
esté quieto. Se publica una sola entrada y permanecer dentro no vuelve a
publicarla.


El marcador no daña, mata, retira, pausa, guarda, cambia navegación ni ejecuta
acciones por sí mismo. Lua decide la consecuencia, incluso no hacer nada.

No confundir:

- `ruta/path`: waypoints que gobiernan navegación;
- tile `checkpoint`: guardado/autosave del jugador;
- `trigger/zona`: entidad del grid con acciones y activación propia;
- tile `base`: estructura dañable;
- `marcador/marker`: región general identificada y neutral.

`world.move_enemy` es una recolocación. Aterrizar dentro se observa como
entrada; saltar desde un lado hasta el otro no se interpreta como trayectoria.

### 3.4.2 Habilidad reutilizable

Una habilidad se define en la raíz del `.eclmap`; no ocupa una letra del grid y
no pertenece a un género concreto:

```text
habilidad pulso:
  alcance: 300
  cadencia: 1.0
  proyectil: F
  preferencia: mas_cercano
  adquisicion:
    relacion: opuesto
    capacidad: recibe_dano
    vinculo: obstructor_locomocion_actual

ability pulse:
  range: 300
  fire_rate: 1.0
  projectile: F
  preference: nearest
  acquisition:
    relation: opposing
    capability: damageable
    binding: current_locomotion_blocker
```

Una habilidad también puede publicar un efecto directo sin proyectil:

```text
habilidad golpe_corto:
  alcance: 70
  cadencia: 0.5
  daño_directo: 2

ability short_hit:
  range: 70
  fire_rate: 0.5
  direct_damage: 2
```

Los defaults son alcance `200`, cadencia `0`, proyectil vacío y preferencia
`mas_cercano/nearest`; el daño directo y el vínculo de adquisición están
ausentes, y la coordinación es `independiente/independent`. Alcance y cadencia
deben ser finitos y no negativos. `daño_directo/dano_directo/direct_damage`,
cuando se autora, debe ser un entero positivo.
Los IDs de definición, uso y referencias son tokens desnudos: sin comillas,
espacios, `#` ni `:`.

`adquisicion/acquisition` es opcional. La habilidad puede solicitar:

- `relacion: opuesto` / `relation: opposing`;
- `capacidad: recibe_dano` / `capability: damageable`;
- `vinculo: obstructor_locomocion_actual` /
  `binding: current_locomotion_blocker`.

La habilidad no inventa esos hechos: el motor los obtiene de la instancia que
la ejecuta y de los candidatos, usando la misma lectura de estado con la que ordena la preferencia.

El vínculo `obstructor_locomocion_actual/current_locomotion_blocker` es
estricto: hoy lo aportan los enemigos con `movimiento: ruta` o `movimiento: seguir`.
La habilidad sólo considera a quien detuvo su movimiento en ese tick.
Ese candidato todavía debe estar vivo, dentro de
alcance y satisfacer los demás filtros de adquisición; el vínculo no los
salta. Si no existe obstructor actual o deja de ser elegible, el uso queda sin
blanco, conserva una oportunidad lista y **no** cae sobre otro candidato.
Sin ese campo, se conserva la selección que la instancia usa por defecto.
`independiente/independent` permite desactivar en un uso un vínculo heredado;
al estar escrito dentro de `adquisicion`, sigue siendo un opt-in explícito a la
selección de esa instancia. Si no tiene contexto de movimiento, tampoco
busca un blanco alternativo.

`coordinacion/coordination` es otro subbloque opcional, separado de
adquisición y preferencia:

```text
habilidad golpe_coordinado:
  alcance: 70
  cadencia: 0.5
  daño_directo: 2
  coordinacion:
    daño_directo_comprometido: omitir_objetivo_cubierto

ability coordinated_hit:
  range: 70
  fire_rate: 0.5
  direct_damage: 2
  coordination:
    committed_direct_damage: skip_covered_target
```

El campo acepta también el alias ASCII
`dano_directo_comprometido`. Su valor explícito inverso es
`independiente/independent`, útil para que un uso desactive la política heredada
de su definición.

`omitir_objetivo_cubierto/skip_covered_target` sólo tiene efecto en un uso
que compromete `daño_directo`. En cada fase, los usos listos se
ordenan por `(actor_id, use_id)`. Toda decisión previa de daño directo alimenta
un registro local —aunque el uso emisor sea independiente— y un uso coordinado
omite un candidato cuando ese daño comprometido alcanza su HP actual del
snapshot. La preferencia normal ordena después los candidatos restantes. Si no
queda ninguno, el uso conserva el reloj listo y no compromete target.

Enemigos y torres mantienen registros separados en sus respectivas fases. Un enemigo
sin `adquisicion` sólo considera al jugador dentro del mismo orden de
fase; no gana colocados por declarar coordinación. Los proyectiles no consultan
ni alimentan ese registro: no se reservan impactos futuros. La política tampoco
reduce el exceso de un golpe individual, no bloquea el target en exclusiva y
no coordina entre fases.

Cada torre o enemigo conecta la definición con un uso nombrado.
Una torre escribe:

```text
torre T "Selector" #FFFFFF:
  habilidad disparo:
    tipo: pulso

  habilidad remate:
    tipo: pulso
    preferencia: mayor_hp_actual
```

Un enemigo conecta sus usos a las habilidades de la misma manera:

```text
enemigo E "Torreta doble" #DD5577:
  movimiento: quieto

  habilidad aguijon:
    tipo: pulso
    cadencia: 0.45

  habilidad peso:
    tipo: pulso
    cadencia: 1.05
    proyectil: B
    adquisicion:
      relacion: opuesto
      capacidad: recibe_dano
```

Cada instancia enemiga materializa un reloj y valores efectivos por uso. En
ausencia de adquisición, sólo considera al jugador vivo como
candidato. Cuando la habilidad o el uso declara adquisición, la fase fotografía
jugador + colocados y filtra ambos tipos por los hechos solicitados. Esta
ampliación es opt-in: no crea facciones ni una taxonomía universal, y no activa
coordinación por sí sola.

Cada uso hereda `alcance/range`, `cadencia/fire_rate`,
`proyectil/projectile`, `daño_directo/dano_directo/direct_damage`,
`preferencia/preference` y los campos de `adquisicion/acquisition`; puede
sobrescribirlos campo por campo. También hereda
`coordinacion/coordination.daño_directo_comprometido` y puede volverlo
`independiente`. El valor efectivo debe tener proyectil
**o** daño directo, nunca ambos: el conflicto avisa y omite el uso. El efecto
directo aplica su cantidad al blanco elegido sin crear un proyectil; torre y
enemigo son las dos clases de instancia que hoy pueden ejecutarlo.

Los dos primeros valores públicos de preferencia son:

- `mas_cercano/nearest`: menor distancia euclídea entre centros AABB;
- `mayor_hp_actual/highest_current_hp`: mayor HP actual del snapshot.

La preferencia ordena candidatos que ya pasaron elegibilidad; no concede por sí
sola relación, compatibilidad, percepción, daño ni retención. En empate gana el
menor ID local; si también coincide, el orden es
`Player < Enemy < Placed`. Esos nombres son dominios de dirección —espacios de
ID—, no clases universales de actor. El borde exacto del alcance sí cuenta.

Una referencia explícita inexistente avisa y omite ese uso. No cae a los campos
directos del enemigo o la torre: si existe cualquier bloque de uso explícito, ese grupo gobierna.

### 3.5 Proyectil (template, no se coloca en mapa)

Se puede referenciar por letra o nombre. `tamaño` es un único lado, `tiempo_vida`/`lifetime` está en segundos y `atraviesa_paredes`/`pierce_walls` sólo controla paredes. `gravedad`/`gravity` es booleano en este bloque.
```
proyectil "bola_fuego":
  sprite: "assets/fireball.png"
  velocidad: 500
  daño: 2
  tamaño: 16
  # Segundos
  tiempo_vida: 3.0
  # Paredes, no enemigos
  atraviesa_paredes: false
```

Los proyectiles no van en el mapa. Son templates que los enemigos, torres o el jugador usan. Se instancian en runtime cuando un enemigo con `cadencia` dispara, cuando una torre detecta un blanco, o cuando un trigger hace `spawnear`.

**Intercepción física:** adquirir un blanco fija la dirección inicial, pero el
proyectil actual no conserva su dirección compuesta como target ligado. En cada
update resuelve paredes y cuerpos compatibles mediante colisión; un proyectil
hostil puede golpear un colocado destruible interpuesto antes de alcanzar al
jugador. Así funciona hoy. No hay seguimiento automático del blanco, impacto exclusivo
ni vínculo persistente al objetivo: `adquisicion` elige a quién apuntar,
pero no cambia las colisiones del proyectil.

### 3.6 Tile empujable

Coloca `K` en el grid. No existe un bloque `bloque:` ni un evento `sobre_boton:`.
```
tile K "assets/caja.png":
  tipo: empujable
```

### 3.7 Torre (Tower Defense)

Una torre es una entidad estática que puede ejecutar usos de
habilidad. Tiene bando aliado y no colisiona con el jugador; la selección vive
en el núcleo neutral de Gameplay 2D, no en un modo llamado tower defense.

```
habilidad flecha_simple:
  alcance: 200
  cadencia: 1.0
  proyectil: F
  preferencia: mas_cercano

torre T "Arquero" #CC6633:
  sprite: "assets/towers/archer/"
  # Oro sugerido para colocarla (opcional)
  costo: 50
  # 0 = indestructible (default); >0 = atacable
  hp: 0
  habilidad disparo:
    tipo: flecha_simple
  # Solo se coloca sobre tiles de este tipo (opcional)
  donde: slot_torre
  # Acciones al colocarse
  al_colocar:
    sonido: "assets/build.ogg"
    notificar: "¡Arquero listo!"
  # Solo si hp > 0
  al_destruir:
    set: torres_perdidas + 1
    sonido: "assets/tower_down.ogg"
```

Un colocado sólo participa como obstructor si lo declara:

```text
torre B "Compuerta":
  hp: 8
  obstruye_locomocion: true

tower G "Gate":
  hp: 8
  obstructs_locomotion: true
```

**Compatibilidad con mapas anteriores:**

Los campos planos `rango/range`, `cadencia/fire_rate`,
`proyectil/projectile` y `objetivo/target` siguen funcionando. Al cargar se
materializan como un único uso `legacy_attack`; una torre sin ataque conserva
ese uso inerte. Si se mezclan campos planos y usos explícitos, el loader avisa y
gobiernan los usos explícitos.

Los valores históricos de `objetivo/target` son:
- `cercano/nearest` — enemigo más cercano a la torre (default)
- `lejano/farthest` — enemigo más lejano dentro del rango
- `primero/first` — enemigo más adelantado en su ruta (el más cerca de la base)
- `ultimo/last` — enemigo más atrasado en su ruta
- `fuerte/strongest` — enemigo con más HP actual
- `debil/weakest` — enemigo con menos HP actual

**Notas importantes:**

- Las torres **NO se colocan en el mapa ASCII**. El `.eclmap` las define como templates; la colocación ocurre en runtime.
- El creador decide cómo el jugador las obtiene: compra con oro (`costo` + modo construcción), drop de enemigo, colocación automática desde Lua, o cualquier combinación. Ver sección 11 "Modo construcción".
- Si `donde:` no se especifica, la torre se puede colocar en cualquier tile caminable.
- Si `hp: 0`, la torre es indestructible (no recibe daño de enemigos). Si `hp > 0`, los enemigos con `daño > 0` pueden atacarla.
- `obstruye_locomocion/obstructs_locomotion` vale `false` por defecto. Ser
  colocado, destruible o indestructible no implica obstruir.
- La obstrucción funciona con `movimiento: ruta` y `movimiento: seguir`. Ambos prueban por sweep AABB
  cerrado el vector completo intentado, retienen un solo bloqueador mientras
  siga obstruyendo y reanudan en el tick posterior a su destrucción.
- Al seguir al jugador, el motor calcula primero el desplazamiento sin mover al enemigo,
  conserva su colisión con tiles y después comprueba ese mismo vector contra los colocados.
- No se promete colisión con jugador, proyectiles, knockback, patrullas, huida,
  rebote, círculo ni otros tipos de movimiento.

---

## 4. RUTAS / WAYPOINTS

Definen caminos que los enemigos pueden seguir. La cabecera es `ruta` o `route`; `path` sólo es el valor inglés de `movimiento`. Cada ruta necesita al menos dos puntos.

```
ruta camino1:
  2, 1
  10, 1
  10, 5
  2, 5
  recorrido: ciclo
```

Cada punto usa coordenadas de tile y va en su propia línea. La política
`recorrido/traversal` acepta:

- `ciclo/loop` — del último waypoint vuelve al primero; es el default;
- `ida_vuelta/ping_pong` — recorre los puntos en sentido inverso;
- `una_vez/once` — completa el recorrido en el último waypoint y deja al actor
  vivo y estable allí.

La compatibilidad histórica se conserva:

```
ruta bucle_compatible:
  2, 1
  10, 1
  # Equivale a recorrido: ciclo
  loop: true

ruta ida_vuelta_compatible:
  2, 1
  10, 1
  # Equivale a recorrido: ida_vuelta
  loop: false
```

Completar un recorrido `una_vez/once` solo termina ese movimiento. No implica
muerte, retirada, daño, recompensa, evento de llegada ni salida del nivel. La
velocidad continúa perteneciendo a la entidad.

**Uso en una entidad:**
```
enemigo G "Guardia" #CC0000:
  movimiento: ruta camino1
```

**Uso en oleadas (tower defense):** el enemigo E debe declarar `movimiento: ruta camino1`. La oleada sólo indica cuántos E aparecen y dónde.
```
oleadas:
  avance: eliminar
  oleada 1:
    tiempo: 5
    spawnear: E 5 en 2, 1
```

---

## 5. OLEADAS / WAVES

Para survival, tower defense, arenas de combate.

```
oleadas:
  avance: eliminar
  oleada 1:
    tiempo: 3
    spawnear: E 5 aleatorio
  oleada 2:
    tiempo: 10
    spawnear: E 3 aleatorio
    spawnear: F 2 en 2, 1
  oleada 3:
    tiempo: 15
    spawnear: G 1 en 2, 1
    al_completar:
      set: arena_superada = 1
      notificar: "¡Todas las oleadas superadas!"
      musica: "assets/victoria.ogg"
```

**Spawn de oleadas:** `aleatorio` elige suelo; no busca los tiles `tipo: spawn`. `en COL, ROW` usa coordenadas de tile. Todos los miembros de una entrada aparecen juntos tras `tiempo`/`delay`; no existe un intervalo individual entre ellos. `avance`/`advance` admite `eliminar`/`eliminate` o `tiempo`/`time`; el segundo usa `intervalo`/`interval` del bloque `oleadas`. `al_completar` pertenece a cada oleada.

---

## 6. CONDICIONES DE SALIDA

Cada renglón de esta lista es una alternativa: elige uno para tu nivel. Si escribes varios, prevalece el último. Omite el campo para no declarar una condición automática; `ninguna` no es un valor reconocido.

Condiciones disponibles:
```
# Hablar con todos los NPCs
salida: hablar_todos
# Pisar el tile '>'
salida: llegar
# Derrotar a todos los enemigos
salida: matar_todos
# Completar todas las oleadas
salida: oleadas
# Sobrevivir 60 segundos
salida: timer 60
# Recoger todos los items
salida: recoger_todos
# Cuando una variable tenga cierto valor
salida: variable victoria == 1
# La base cae a 0 HP (condición de FRACASO)
salida: base_destruida
# Lua decide cuándo termina (world.exit())
salida: lua
```

Nota sobre `base_destruida`: es una condición de **derrota**. Al dispararse, ejecuta el bloque `al_destruir:` del tile base y sale del nivel. Para niveles TD lo típico es combinar `salida: oleadas` (victoria) con el hook `base_destroyed` en Lua (derrota).

---

## 7. EVENTOS DE ENTIDAD COMPARTIDOS

Las acciones se escriben dentro de un evento. `destruir`, `colocar_torre`, `modo_construccion`, `cancelar_construccion` y `dañar_base` no son acciones `.eclmap`; usa los bindings Lua específicos de §12. `camara: pan` recibe tiles; `spawnear` dentro de un evento recibe píxeles.

El lector reconoce `esperar`/`wait`, pero la ejecución actual sólo activa un temporizador: continúa el bucle de acciones y no difiere la siguiente. Para secuenciar una escena usa tu lógica de `update` y `dt`.

Otras acciones disponibles: `cargar_mapa: "nivel2.eclmap" 5, 3` (destino en tiles), `lua: "script.lua"` y `vn: "historia.ecl"` (con escena opcional). `camara: seguir`/`follow` devuelve el seguimiento; `camara: sacudir`/`shake` indica duración en segundos.

Bloques de acciones que pueden usarse en `al_morir`, `al_recoger`, `al_entrar`, `al_colocar`, etc.:

```
trigger T:
  una_vez: true
  al_entrar:
    set: monedas + 1
    sonido: "ruta.ogg"
    musica: "musica.ogg"
    notificar: "Texto"
    dialogo: "Aviso flotante, sin caja de diálogo"
    spawnear: E 2 en 400, 200
    destruir_tile: 15, 8
    crear_tile: 15, 8 #
    camara: sacudir 0.5
    camara: pan 8, 4 1.5
    camara: zoom 1.2 0.5
    soltar: M
    desaparecer: true
# Referencias: E es un enemigo y M un item definidos en este nivel.
```

> **Ni `notificar:` ni `dialogo:` abren una caja de diálogo.** Las dos empujan el **mismo aviso
> flotante**, y lo único que las separa es cuánto dura en pantalla (2.5 s contra 3 s). Vale para
> todos los ejemplos de este documento que las usan.
>
> **El único diálogo de verdad dentro del 2D —con caja, nombre del que habla, typewriter y
> opciones— es el del bloque `npc`** (§3). No hay forma de abrirlo desde un bloque de evento.

## 8. EJEMPLO COMPLETO — Nivel de terror: escapar de la escuela

```
# nivel_escuela.eclmap — Pasillo embrujado
# El jugador explora, recoge una llave, huye de un fantasma.

modo: topdown
musica: "assets/audio/tension.ogg"
velocidad: 350
salida: variable escapó == 1

jugador:
  sprite: "assets/player/"
  hp: 3

tile . "assets/piso_madera.png"
tile # "assets/pared_escuela.png"
tile ~ "assets/sangre.png":
  tipo: daño
  daño: 1

hud:
  hp: visible

mapa:
####################
#..................#
#..P...........I...#
#..................#
#.....######.......#
#.....#....#.......#
#..A..#....#...T...#
#.....######.......#
#..................#
#.................X#
####################

npc A "Estrella" #FF99CC:
  "¡No puedo creer que estemos atrapados aquí!"
  "Vi una llave al fondo del pasillo..."
  "Ten cuidado. Hay algo en el cuarto oscuro."

item I "Llave oxidada" #FFD700:
  sprite: "assets/items/llave.png"
  al_recoger:
    set: tiene_llave = 1
    sonido: "assets/pickup.ogg"
    notificar: "Obtuviste la Llave Oxidada"

trigger T:
  tamaño: 96, 96
  una_vez: true
  condicion: tiene_llave == 1
  al_entrar:
    musica: "assets/audio/chase.ogg"
    camara: shake 0.8
    spawnear: G 1 en 200, 200
    # aviso flotante, no caja
    dialogo: "¡¿Qué es eso?!"

enemigo G "Fantasma" #AA00FF:
  sprite: "assets/enemies/ghost/"
  hp: 0
  invulnerable: true
  velocidad: 280
  movimiento: seguir 400
  daño: 1

trigger X:
  tamaño: 48, 48
  condicion: tiene_llave == 1
  al_entrar:
    set: escapó = 1
```

---

## 9. EJEMPLO COMPLETO — Tower Defense básico

```
# nivel_td.eclmap — Defiende la base
# Los enemigos siguen la ruta. Coloca torres para detenerlos.

modo: topdown
musica: "assets/audio/battle.ogg"
salida: oleadas
lua: "td_logic.lua"

hud:
  hp_base: visible
  oleada: visible
  contador: "oro" "Oro: {}"
  texto: "¡Defiende la base!"

tile . "assets/pasto.png"
tile # "assets/roca.png"

tile * "assets/portal.png":
  tipo: spawn

tile @ "assets/base.png":
  tipo: base
  hp: 20
  al_destruir:
    notificar: "¡La base ha caído!"

tile % "assets/torre_slot.png":
  tipo: slot_torre

ruta camino1:
  5, 2
  12, 2
  12, 8
  18, 8
  recorrido: una_vez

mapa:
####################
#..................#
#..%..*............#
#..................#
#......%...........#
#..................#
#..................#
#...........%......#
#................%@#
####################

proyectil "flecha":
  sprite: "assets/proyectil/flecha.png"
  velocidad: 600
  daño: 2
  tamaño: 12

proyectil "fuego":
  sprite: "assets/proyectil/fuego.png"
  velocidad: 400
  daño: 5
  tamaño: 20

enemigo E "Slime" #00FF00:
  sprite: "assets/enemies/slime/"
  hp: 3
  velocidad: 80
  daño: 1
  movimiento: ruta camino1
  al_morir:
    set: oro + 5

enemigo F "Orco" #885522:
  sprite: "assets/enemies/orco/"
  hp: 8
  velocidad: 50
  daño: 3
  movimiento: ruta camino1
  al_morir:
    set: oro + 15

torre T "Arquero" #CC6633:
  sprite: "assets/towers/archer/"
  costo: 50
  rango: 200
  cadencia: 1.5
  proyectil: "flecha"
  objetivo: cercano
  donde: slot_torre

torre M "Mago" #3366CC:
  sprite: "assets/towers/mage/"
  costo: 120
  rango: 160
  cadencia: 0.8
  proyectil: "fuego"
  objetivo: fuerte
  donde: slot_torre

oleadas:
  avance: eliminar
  oleada 1:
    tiempo: 5
    spawnear: E 5 en 5, 2
  oleada 2:
    tiempo: 15
    spawnear: E 8 en 5, 2
  oleada 3:
    tiempo: 20
    spawnear: E 5 en 5, 2
    spawnear: F 2 en 5, 2
    al_completar:
      set: nivel_superado = 1
      notificar: "¡Victoria!"
```

**Script Lua acompañante (`td_logic.lua`):**
```lua
-- Oro inicial
world.set_var("oro", "100")

-- Selección de torre por tecla numérica
world.on("key_press", function(key)
    local oro = tonumber(world.get_var("oro")) or 0
    if key == "1" and oro >= world.get_tower_cost("T") then
        world.enter_build_mode("T")
    elseif key == "2" and oro >= world.get_tower_cost("M") then
        world.enter_build_mode("M")
    end
end)

-- Cobrar al colocar torre
world.on("tower_placed", function(name, id, col, row)
    local cost = world.get_tower_cost(name)
    world.add_var("oro", -cost)
end)

-- Game over si cae la base
world.on("base_destroyed", function()
    world.notify("¡Has perdido!")
    world.exit()
end)
```

---

## 10. EJEMPLO COMPLETO — Plataformero side-scroll

```
# nivel_cueva.eclmap — Cueva con enemigos
# Plataformero clásico con salto y combate.

modo: sidescroll
musica: "assets/audio/cueva.ogg"
gravedad: 1800
salto: 700
coyote: 0.12
salida: llegar

jugador:
  sprite: "assets/player/"
  hp: 5
  puede_atacar: true
  ataque_rango: 60
  ataque_daño: 1
  invencible: 1.0

tile . "assets/tiles/cueva_fondo.png"
tile # "assets/tiles/cueva_pared.png"
tile ^ "assets/tiles/pincho.png":
  tipo: daño
  daño: 2
tile - "assets/tiles/plataforma.png":
  tipo: plataforma

hud:
  hp: visible

mapa:
################################
#..............................#
#..............................#
#..P...........................#
#.####.......-.....E...........#
#..........####.......-........#
#....E...........####...I......#
#..####....^^..........####..>.#
################################

enemigo E "Murciélago" #880088:
  sprite: "assets/enemies/bat/"
  hp: 2
  daño: 1
  velocidad: 120
  movimiento: patrulla_h 4
  al_morir:
    set: score + 100
    sonido: "assets/hit.ogg"

item I "Corazón" #FF0066:
  sprite: "assets/items/heart.png"
  al_recoger:
    set: hp + 1
    sonido: "assets/heal.ogg"
    notificar: "+1 HP"
```

---

## 11. MODO CONSTRUCCIÓN (Tower Defense)

Para que el jugador coloque torres en runtime, el motor ofrece un "modo construcción": un cursor visible sobre el grid que valida dónde se puede colocar, muestra el rango de la torre, y confirma con clic o Enter.

### Flujo típico

1. El jugador presiona una tecla o botón de UI.
2. Lua llama `world.enter_build_mode("T")`.
3. El motor muestra un cursor semitransparente sobre el tile más cercano al jugador.
4. El jugador mueve el cursor con flechas/WASD o el mouse.
5. El cuadro se ve **VERDE** si el tile es válido, **ROJO** si no.
6. El rango de la torre se dibuja como un círculo semitransparente desde el cursor.
7. `Enter` / clic izquierdo → confirma la colocación.
8. `Escape` / clic derecho → cancela.
9. Al colocar, se dispara el hook `tower_placed` y se ejecuta el bloque `al_colocar:` de la torre.

### Primitivas Lua

```lua
world.enter_build_mode(tower_name)        -- Activa el modo construcción
world.exit_build_mode()                   -- Cancela
world.place_tower(name, col, row)         -- Coloca directo sin UI; retorna bool
world.remove_tower(id)                    -- Destruye torre por ID
world.can_place(name, col, row)           -- Valida posición; retorna bool
world.get_tower_cost(name)                -- Lee el costo; retorna int
world.inhibit_placed_activities(id)       -- Inhibe todos los usos; retorna handle
world.inhibit_placed_activities(id, use_id) -- Inhibe un uso exacto; retorna handle
world.remove_activity_inhibition(handle)  -- Retira una causa exacta; retorna bool
world.get_placed_hp(id)                   -- HP actual/máximo; -1,-1 si no está vivo
world.restore_placed_hp(id)               -- Restaura a HP máximo; retorna bool
world.replace_placed(id)                  -- Misma definición, ID nuevo; retorna id|-1
world.extract_placed(id)                  -- Retira del mundo; conserva identidad y estado
world.redeploy_placed(id, col, row)       -- Misma identidad en otro destino válido
world.add_placed_activity_cost(id, use_id, variable, amount) -- Coste al ejecutar el uso
world.remove_activity_cost(handle)        -- Retira una causa exacta; retorna bool
world.add_placed_activity_interval_factor(id, use_id, factor) -- Factor de intervalo
world.add_enemy_activity_interval_factor(id, use_id, factor) -- Mismo factor para un enemigo
world.remove_activity_interval_factor(handle) -- Retira una causa; retorna bool
world.add_placed_activity_range_factor(id, use_id, factor) -- Factor de alcance
world.remove_activity_range_factor(handle) -- Retira una causa; retorna bool
world.add_placed_activity_direct_damage_delta(id, use_id, delta) -- Delta entero
world.remove_activity_direct_damage_delta(handle) -- Retira una causa; retorna bool
world.find_nearest_enemy(x, y, max_range) -- Para torres con AI custom; retorna id, x, y
world.get_base_hp()                       -- HP actual de la base
world.damage_base(n)                      -- Aplicar daño manual a la base
```

### Inhibición identificada de actividades

La inhibición impide **emisiones nuevas** de una instancia `Placed` (torre colocada) o `Enemy` (enemigo) sin dañarla,
destruirla ni retirarla del mapa. Elegimos la instancia por su ID y conservamos
el `handle` positivo que devuelve el motor:

```lua
local todo = world.inhibit_placed_activities(torre_id)
local disparo = world.inhibit_placed_activities(torre_id, "disparo")
local enemigo = world.inhibit_enemy_activities(enemy_id, "disparo")

-- La condición y el momento pertenecen al juego.
if jefe_termino_el_bloqueo then
    world.remove_activity_inhibition(todo)
    world.remove_activity_inhibition(enemigo)
end
```

- Un `id` o `use_id` inexistente devuelve `-1` y no crea estado.
- Varias causas se apilan. Retirar un `handle` no borra las demás.
- Omitir `use_id` cubre todos los usos; indicarlo cubre sólo esa identidad.
- Mientras un uso está inhibido, su reloj avanza como máximo hasta quedar
  listo. Al reactivarse puede emitir una vez, pero no acumula una ráfaga.
- Un proyectil o efecto ya comprometido sigue su lifecycle aunque la fuente
  sea inhibida después.
- El token no contiene duración, azar, selección de receptores ni conteo de
  clics. Todo eso se compone desde Lua y los hooks existentes.
- Si el `Placed` o Enemy deja de estar vivo, sus tokens dejan de ser
  retirables; no transfieren identidad a otra instancia posterior.

### Restauración y reemplazo de un `Placed`

Podemos consultar los puntos de vida de una torre colocada y elegir entre
dos operaciones: restaurar la misma instancia o reemplazarla por una nueva.

```lua
local hp, hp_max = world.get_placed_hp(placed_id)

-- Misma identidad: sólo HP cambia.
local restaurado = world.restore_placed_hp(placed_id)

-- Otra identidad de la misma definición.
local nuevo_id = world.replace_placed(placed_id)
```

- Ambas mutaciones sólo aceptan una instancia viva, destructible y degradada:
  `maxHP > 0` y `0 < hp < maxHP`.
- `get_placed_hp` devuelve `-1, -1` para un ID ausente o no vivo. `0, 0`
  continúa describiendo un `Placed` indestructible vivo.
- Restaurar conserva ID, definición, posición, presentación, cooldowns,
  targets, inhibiciones, costes y factores runtime de actividad, handles y
  referencias; asigna únicamente `hp = maxHP`.
- Reemplazar consume la identidad anterior sin publicar colocación ni
  destrucción. La instancia nueva recibe HP máximo, ID nuevo, cooldowns en
  cero, targets vacíos, inhibiciones vacías y cero costes runtime.
- Los handles y referencias al ID consumido no migran ni reciben alias. El
  nuevo ID es el único que puede direccionarse después del commit.
- El reemplazo crea una instancia nueva de la **misma definición**. Cambiar de forma,
  mejorar o convertir entre definiciones no está cubierto por estas funciones.
- `costo/cost` sigue siendo metadato. Lua comprueba recursos y los compromete
  sólo después de que la operación mecánica devuelve éxito.
- Restaurar y reemplazar no ejecutan `al_colocar/on_place`, `al_destruir/on_destroy`,
  `tower_placed` ni `tower_destroyed`. Tampoco resucitan una instancia que ya
  llegó a destrucción terminal.

### Extracción y custodia de un `Placed`

Podemos retirar una torre del mundo sin destruirla. Lua conserva el ID durante la
custodia y decide cuándo devolver la instancia al mundo:

```lua
local extraido = world.extract_placed(placed_id)
local redeplegado = world.redeploy_placed(placed_id, col, row)
```

- Extraer sólo acepta un `Placed` vivo y desplegado. Repetir la operación
  devuelve `false` sin mutación.
- La custodia conserva ID, definición, HP, coste, cooldowns, targets,
  inhibiciones, costes y factores runtime de actividad. La instancia no renderiza, ocupa slot, obstruye, recibe
  targeting/daño ni ejecuta actividades; sus relojes quedan congelados.
- El slot anterior queda disponible. El redeploy valida límites, solidez,
  requisito de slot y ocupación mediante las reglas normales de colocación.
- Un redeploy inválido devuelve `false` y conserva la custodia. Uno válido
  compromete la posición y reanuda simulación con el mismo ID/estado.
- No hay cobro, reembolso, `al_colocar/on_place`, `al_destruir/on_destroy`,
  `tower_placed` ni `tower_destroyed`.
- La inhibición de actividades, `get_placed_hp` y `restore_placed_hp` siguen alcanzando la identidad
  custodiada. `replace_placed` devuelve `-1` para no redeplegarla de forma
  implícita.
- `remove_tower` sigue siendo una destrucción terminal explícita si Lua la
  invoca. La extracción no añade transporte visual, un captor, un punto de anclaje, un dueño ni capacidades
  activas fuera del mundo.

### Costes al ejecutar una actividad

Podemos hacer que un uso nombrado de una torre colocada (`Placed`) consuma una variable numérica
del mundo cuando compromete un efecto. El nombre y el significado del recurso
los decidimos en la lógica del juego:

```lua
world.set_var("municion", "3")

local coste = world.add_placed_activity_cost(
    placed_id, "disparo", "municion", 1)

-- Retira exactamente esa causa; no modifica saldo ni otros costes.
world.remove_activity_cost(coste)
```

- La API es runtime y no agrega sintaxis `.eclmap`.
- `placed_id` y `use_id` deben existir; `variable` no puede estar vacía y
  `amount` debe ser un entero positivo. Un registro inválido devuelve `-1`.
- Varias causas se apilan. Todos los costes del uso se comprueban juntos; si
  falta uno, no se consume ninguno. Costes repetidos sobre la misma variable
  se suman antes de comprobar el saldo.
- La variable debe contener un entero decimal no negativo al llegar el commit.
  Ausencia, texto, decimal, basura o saldo negativo significan insuficiencia y
  no mutan el mundo.
- Sólo un uso con blanco válido puede consumir. Un rechazo conserva el cooldown
  listo y reintenta al tick siguiente sin acumular ráfaga.
- La admisión sigue el orden estable `(actor_id, use_id)`. Un uso sin saldo no
  reserva daño directo comprometido ni cambia el blanco de usos posteriores.
- La inhibición conserva prioridad: una actividad inhibida no crea oportunidad ni consume.
- Restaurar, extraer y volver a colocar conservan los costes porque preservan
  identidad. Reemplazar o destruir consume la identidad y sus handles; la
  instancia nueva nace sin costes runtime.
- No hay callback de commit ni presentación automática. Proyectiles existentes
  pagan al desprenderse, no al impacto.

### Factores de intervalo de actividad

Podemos modificar durante el juego cuánto tarda en volver a estar listo un uso
nombrado de un `Placed` o `Enemy`, sin convertir HP, avería o combustible en
reglas universales:

```lua
local averia = world.add_placed_activity_interval_factor(
    placed_id, "disparo", 2.0) -- espera el doble del intervalo base

local lentitud = world.add_enemy_activity_interval_factor(
    enemy_id, "peso", 4.0) -- sólo ese uso del Enemy espera cuatro veces más

-- Retira exactamente esa causa; otros factores siguen activos.
world.remove_activity_interval_factor(averia)
world.remove_activity_interval_factor(lentitud)
```

- La API es Lua runtime; no agrega sintaxis `.eclmap`.
- El ID del owner y `use_id` deben existir en la instancia viva. `factor` debe
  ser finito y mayor que cero; un registro inválido devuelve `-1` sin consumir
  handle.
- Los factores se apilan multiplicativamente y son independientes de su orden.
  Con intervalo base `0.2`, factores `2` y `3` producen `1.2` segundos.
- Cada handle retira una causa exacta. Un handle desconocido, ya retirado o
  perteneciente a una identidad consumida devuelve `false`.
- El reloj conserva segundos absolutos ya transcurridos. Cambiar el intervalo
  no reinicia el reloj ni conserva un porcentaje de avance. Si el nuevo umbral
  ya fue alcanzado, aparece una sola oportunidad en el siguiente update, nunca
  una ráfaga acumulada.
- La inhibición sigue decidiendo si la actividad puede crear una oportunidad. Los costes se siguen
  cobrando al ejecutar el uso; variar el intervalo no cobra, devuelve ni
  inventa recursos.
- Restaurar, extraer y volver a colocar preservan los factores y handles de `Placed`
  porque conservan identidad. Reemplazar o destruir consume esa identidad. En
  Enemy, derrota, llegada a Base o `destroy_enemy` también invalidan los
  handles. Toda instancia nueva nace con intervalo base y sin factores runtime.
- El motor no deriva factores de HP ni altera daño, alcance, preferencia o
  proyectiles. En Enemy tampoco modifica otro uso ni la locomoción. La
  condición, duración y significado de la causa pertenecen a Lua.

### Factores de alcance de actividad

Podemos modificar durante el juego qué receptores futuros quedan dentro del alcance de
un uso nombrado de `Placed`, sin convertir HP, recursos o partes en reglas
automáticas:

```lua
local interferencia = world.add_placed_activity_range_factor(
    placed_id, "disparo", 0.5) -- usa la mitad del alcance autorado

-- Retira exactamente esa causa; otros factores siguen activos.
world.remove_activity_range_factor(interferencia)
```

- La API es Lua runtime; no agrega sintaxis `.eclmap`.
- `placed_id` y `use_id` deben existir en la instancia viva. El alcance base y
  `factor` deben ser finitos y mayores que cero; un registro inválido devuelve
  `-1`. El factor cero no sustituye a la inhibición de actividades.
- Los factores se apilan multiplicativamente. Cada handle retira una causa
  exacta; uno desconocido, retirado o perteneciente a una identidad consumida
  devuelve `false`.
- Sólo cambia la próxima adquisición. No reescribe targets, proyectiles,
  impactos ni efectos ya comprometidos. El borde exacto del alcance continúa
  siendo elegible.
- Si no queda blanco válido, el uso no paga sus costes ni compromete su
  oportunidad lista; puede reintentar en el siguiente tick.
- Restaurar, extraer y volver a colocar preservan factores y handles porque
  conservan identidad. Reemplazar o destruir consume la identidad; la nueva
  instancia nace con el alcance autorado y sin factores runtime.
- El motor no deriva factores de HP ni altera daño, intervalo, preferencia o
  presentación. Causa, duración y retirada pertenecen a Lua.

### Cambios de daño directo de actividad

Podemos ajustar durante el juego el daño directo de futuras ejecuciones de un uso nombrado
de `Placed`, sin reescribir el valor autorado:

```lua
local averia = world.add_placed_activity_direct_damage_delta(
    placed_id, "golpe", -2) -- base 4; las ejecuciones futuras aplican 2

world.remove_activity_direct_damage_delta(averia) -- vuelve a base 4
```

- La API es Lua runtime; no agrega sintaxis `.eclmap`.
- `placed_id` y `use_id` deben existir en la instancia viva, y el uso debe
  poseer `dano_directo` base positivo. Un uso de proyectil no modifica el daño
  de su carrier por esta vía.
- `delta` debe ser finito, entero exacto y distinto de cero. No se redondean
  fracciones.
- Las causas componen por suma y cada handle retira exactamente una. Para que
  cualquier orden de retirada sea seguro, todos los negativos juntos deben
  dejar al menos 1 y todos los positivos juntos no pueden superar `INT_MAX`.
- El mismo entero comprometido se usa para coordinar la selección de blancos y aplicar el daño real. Cambiar o
  retirar una causa sólo afecta a ejecuciones futuras.
- Restaurar, extraer y volver a colocar conservan delta, handle, coste y cooldown.
  Reemplazar o destruir consume la identidad; la instancia nueva usa el
  daño autorado y los handles viejos devuelven `false`.
- Inhibición, costes, intervalo, alcance, adquisición y preferencia son ejes separados.
  El motor no deriva deltas desde HP, recursos o partes.

### Cambios de preferencia de actividad

Podemos cambiar durante el juego cómo un uso nombrado de `Placed` o `Enemy` ordena los
receptores elegibles de adquisiciones futuras, sin reescribir la preferencia
autorada:

```lua
local averia = world.add_placed_activity_preference_override(
    placed_id, "pulso", "mayor_hp_actual")

local alerta = world.add_enemy_activity_preference_override(
    enemy_id, "pulso", "mayor_hp_actual")

world.remove_activity_preference_override(averia)
world.remove_activity_preference_override(alerta)
```

- La API es Lua runtime; no agrega sintaxis `.eclmap`.
- `placed_id`/`enemy_id` y `use_id` deben existir en la instancia viva del
  owner correspondiente. Sólo se aceptan
  `mas_cercano`/`nearest` y `mayor_hp_actual`/`highest_current_hp`; una entrada
  inválida devuelve `-1`.
- Cada causa contiene una preferencia absoluta. La última **insertada** que
  siga viva gobierna; el número del handle no es prioridad. Retirar la superior
  revela la anterior y vaciar el conjunto restaura el valor autorado.
- No es una lista de criterios: no compone pesos, fallbacks ni desempates
  personalizados.
- Sólo afecta adquisiciones futuras. Elegibilidad, retención, alcance, coste,
  daño, HP, cooldown, inhibición, solicitudes y efectos ya comprometidos no cambian.
- Restaurar, extraer y volver a colocar conservan las causas y handles de `Placed`.
  Reemplazar o destruir consume esa identidad. Cualquier retirada de `Enemy`
  —derrota, llegada a Base o `destroy_enemy`— consume también su identidad.
  Las instancias futuras usan la preferencia autorada y los handles viejos
  devuelven `false`.
- Causa, duración y retirada pertenecen a Lua. Estas funciones no añaden consultas, presentación,
  una API universal de actor, alcance o daño efectivo para `Enemy`, partes,
  callbacks ni persistencia de estas causas en partidas guardadas.
  Los factores de intervalo de `Enemy` también conservan esos límites.

### Hooks

```lua
world.on("tile_click", function(col, row, button) end)    -- Clic en el mundo
world.on("tower_placed", function(name, id, col, row) end) -- Torre colocada
world.on("base_destroyed", function() end)                 -- Base llegó a 0 HP
world.on("key_press", function(key) end)                   -- Tecla presionada
world.on("marker_enter", function(marker_id, actor_kind, actor_id) end)
world.on("action", function(verbo, dominio, ejecutado) end) -- Acción del jugador
```

### Entrada: acciones, no teclas

El jugador puede **remapear sus teclas**. Un script que escucha `"F"` deja de
funcionar el día que alguien mueve el ataque a otra tecla. Por eso hay un canal
que habla de **acciones**:

```lua
world.on("action", function(verbo, dominio, ejecutado)
    if verbo == "atacar" and ejecutado then
        world.notify("¡Zas!")
    end
end)
```

| Argumento | Qué trae |
|---|---|
| `verbo` | la acción, en su **grafía canónica** (`"atacar"`, `"saltar"`, `"mover_derecha"`) |
| `dominio` | `"juego"` |
| `ejecutado` | `true` si el efecto **corrió de verdad** |

**`ejecutado` no es «la tecla se apretó»** — eso ya lo sabemos porque el hook
disparó. Es *«¿pasó algo?»*. Un ataque en cooldown, un dash sin `dash: true`, un
salto en el aire sin saltos restantes: los tres llegan con `ejecutado = false`, y
el motor te dice **por qué** en la consola:

```
[Gameplay2D] 'atacar' no corrio: todavia esta en cooldown.
```

**Una tecla puede activar varias acciones**, y entonces el hook dispara varias
veces. `Space` en top-down resuelve a `atacar`, `interactuar` y `saltar`: te
llegan las tres, y sólo una trae `ejecutado = true`.

> ⚠ **Sólo tres acciones pueden llegar con `ejecutado = true`: `saltar`,
> `atacar` y `dash`.** Las otras seis nunca, y por dos razones distintas que
> conviene saber antes de escribir el `if`:
>
> | acción | qué pasa |
> |---|---|
> | `mover_izquierda` · `mover_derecha` · `mover_arriba` · `mover_abajo` | el movimiento se consulta continuamente, no es un evento. Te llegan siempre con `ejecutado = false` — para leerlas usa **`world.axis`** |
> | `interactuar` · `menu` | cuando **sí** funcionan, el motor **consume** la tecla, así que ni siquiera llegan al hook |
>
> O sea: `if verbo == "mover_derecha" and ejecutado then` **no entra nunca**.
> Si quieres reaccionar a que el jugador *quiso* moverse, mira el verbo y
> no compruebes `ejecutado`; si quieres saber si *se está* moviendo, usa `world.axis("mover_x")`.

> **El verbo llega siempre en español, aunque escribas tu bloque `keys` en
> inglés.** `keys game { jump: SPACE }` es válido, y en Lua vas a comparar contra
> `"saltar"`. No es un olvido: es un identificador que tu código **compara**, y
> tener dos grafías posibles volvería tu `if` una coincidencia en vez de un
> contrato. Los **nombres de función** sí son ingleses (`world.axis`,
> `world.action_active`), porque el resto de tu línea de Lua también lo es.

### Preguntar por acciones (polling)

Para lo que se **mantiene apretado** —caminar, cargar un ataque— el evento no
sirve: queremos el estado, no el momento.

```lua
world.on("update", function(dt)
    local x = world.axis("mover_x")            -- -1 .. 1
    if world.action_active("dash") then
        -- Tu lógica al mantener la acción.
    end
end)
```

| Función | Devuelve |
|---|---|
| `world.axis(eje)` | `-1`, `0` o `1`. Ejes: `mover_x` / `move_x`, `mover_y` / `move_y` |
| `world.action_active(accion)` | `true` mientras la acción esté apretada |

Los **ejes no son acciones**: `mover_x` es la resta de `mover_izquierda` y
`mover_derecha`, por eso no se le puede ligar una tecla. Positivo es **derecha** y
**abajo**.

Las dos aceptan las dos grafías (`move_x` y `mover_x`, `jump` y `saltar`). Si les
pasas un nombre que no existe devuelven `0` / `false` **y lo avisan una vez** por
consola — un cero mudo se lee como un bug de física y no lo es.

### `action` y `key_press` no son lo mismo, y ninguno reemplaza al otro

| | `on("action")` | `on("key_press")` |
|---|---|---|
| te entrega | **acciones** | **teclas crudas**, como las nombra SDL |
| sobrevive al remapeo | **sí** | no |
| alcanza | sólo lo que es una acción | **todo** lo que el motor no consumió |

`key_press` **no está deprecado y no va a desaparecer.** Es la única forma de
escuchar una tecla que no es una acción — el `1` y el `2` con los que eliges
torre, o la tecla Windows. `action` es el canal correcto cuando lo que te importa
es *qué quiso hacer el jugador*; `key_press`, cuando lo que te importa es *qué
tecla tocó*.

Si escuchas una tecla que **sí** es una acción, el motor te lo dice una vez:

```
[Gameplay2D] world.on('key_press') recibio 'F', que en este proyecto es la accion 'atacar'.
  Escuchar la TECLA se rompe cuando el jugador la remapea; on('action') te
  entrega el VERBO y sobrevive al remapeo.
```

*(Ese aviso calla si tu script ya usa `on("action")`: ahí ya conocés el canal.)*

### Colocación fuera del modo construcción

El creador puede colocar torres por lógica propia (drop de enemigo, compra en shop VN, automático):

```lua
-- Al matar un boss, spawnear una torre gratis
world.on("enemy_death", function(name, id)
    if name == "Jefe" then
        world.place_tower("Arquero", 10, 5)
        world.notify("¡Torre bonus obtenida!")
    end
end)
```

### Límite: movimiento de una torre colocada

Una torre colocada tiene identidad `Placed`. `find_enemies` y `move_enemy` sólo operan sobre enemigos. Una orden de caminar para una torre/héroe no existe en esta API.

```lua
-- Una torre colocada es un Placed; no aparece en find_enemies.
-- No hay un binding move_placed para darle una orden de caminar.
-- extract_placed/redeploy_placed (§11) retiran y recolocan una identidad;
-- no representan desplazamiento animado de un héroe.
```

---

## 12. QUÉ PODEMOS HACER CON LUA

Lua permite conectar eventos, variables, economía, spawn y las operaciones listadas aquí. Cada función conserva su dominio: no presupongas inventario, crafting, turnos, rewind o movimiento de un Placed porque Lua esté disponible.

**API Lua (tabla `world`):**

29 funciones base: `get_player_pos`, `set_player_pos`, `get_player_hp`, `set_player_hp`, `damage_player`, `spawn`, `enemy_count`, `destroy_enemy`, `move_enemy`, `get_enemy_pos`, `get_enemy_hp`, `find_enemies`, `destroy_tile`, `create_tile`, `get_tile`, `spawn_projectile`, `camera_shake`, `camera_pan`, `camera_zoom`, `camera_follow`, `notify`, `sound`, `music`, `stop_music`, `get_var`, `set_var`, `add_var`, `wave`, `exit`.

11 funciones para defensa de torres: `get_base_hp`, `get_base_max_hp`,
`damage_base`, `set_base_hp`, `place_tower`, `enter_build_mode`,
`exit_build_mode`, `can_place`, `remove_tower`, `get_tower_cost`,
`find_nearest_enemy`.

2 funciones de fin de nivel: `victory`, `defeat`.

3 funciones de inhibición de actividades: `inhibit_placed_activities`,
`inhibit_enemy_activities`, `remove_activity_inhibition`.

3 funciones de consulta, restauración y reemplazo: `get_placed_hp`, `restore_placed_hp`,
`replace_placed`.

2 funciones de extracción y recolocación: `extract_placed`, `redeploy_placed`.

2 funciones de costes de actividad: `add_placed_activity_cost`,
`remove_activity_cost`.

3 funciones de factores de intervalo: `add_placed_activity_interval_factor`,
`add_enemy_activity_interval_factor`, `remove_activity_interval_factor`.

2 funciones de factores de alcance: `add_placed_activity_range_factor`,
`remove_activity_range_factor`.

2 funciones de cambios de daño directo: `add_placed_activity_direct_damage_delta`,
`remove_activity_direct_damage_delta`.

3 funciones de cambios de preferencia: `add_placed_activity_preference_override`,
`add_enemy_activity_preference_override`,
`remove_activity_preference_override`.

2 funciones de input por polleo: `axis`, `action_active`.

1 registrador de hooks: `on`.

**Total vivo:** 65 bindings en `world`.

16 hooks actuales: `enemy_death`, `player_damage`, `wave_complete`,
`item_pickup`, `trigger`, `marker_enter`, `update`, `tile_click`,
`base_damaged`, `base_destroyed`, `tower_placed`, `tower_destroyed`,
`key_press`, `victory`, `defeat`, `action`.

**Notas de uso:**
- `get_var(name)` devuelve un string; para compararlo numéricamente usa `tonumber(...) or 0`. `set_var(name, value)` exige `value` como string: usa `tostring(n)` o una cadena literal.
- `remove_tower(id, silent)` — `silent` opcional (default false). Con silent=true no ejecuta `al_destruir` ni dispara hook.
- `find_nearest_enemy(x, y, max_range)` — retorna tupla `(id, ex, ey)`. Si no hay candidato, retorna `(-1, 0, 0)`.
- Hook `key_press(key_name)` — se dispara solo en presses iniciales (no key-repeat) y solo por teclas que ningún handler del engine consumió. String viene de `SDL_GetKeyName`, **a propósito y de forma permanente**: es el canal de las teclas *crudas*, incluidas las ~150 que el vocabulario de teclas del motor no nombra (puntuación, numpad, la tecla Windows). Para acciones remapeables, `on("action")`.
- Hook `action(verbo, dominio, ejecutado)` — dispara una vez **por cada acción** que la tecla resolvió y que el motor no consumió. `verbo` viene en grafía canónica española; `ejecutado` dice si el efecto corrió (los gates internos —cooldown, capacidad, suelo— pueden frenarlo, y el motor explica cuál en `stderr`).
- `axis(nombre)` / `action_active(nombre)` — polling. Aceptan grafía ES o EN; un nombre desconocido devuelve `0`/`false` y avisa **una vez** por nombre.
- Hook `marker_enter(marker_id, actor_kind, actor_id)` — publica
  entradas de enemigos a marcadores neutrales; el ID del actor dura el mapa.

---

## 13. CAPACIDADES IMPLEMENTADAS

### Nivel y entidades
Lo mínimo para que funcione un nivel con entidades:
- Entidad genérica (struct con tipo, sprite, hp, movimiento)
- Enemigo con patrulla simple (patrol_h, patrol_v)
- Item con pickup + variable
- Trigger de área con acciones
- Tile con propiedades (daño, checkpoint, puerta)
- Puerta/transición entre mapas
- Game Over state
- HP del jugador + daño de contacto
- HUD de HP
- Side-scroll data-driven

### Combate y movimiento
- Ataque melee del jugador
- Knockback
- Invencibilidad frames
- Enemigos que siguen/huyen del jugador
- Proyectiles (template + disparo)
- Enemigos que disparan
- Al_morir con drop
- Side-scroll: plataformas one-way, dash, wall-slide, doble salto, wall-jump
- Camera pan, zoom, shake desde acciones
- Bloques empujables, destroy/create tile, movimiento circular

### Oleadas y rutas
- Oleadas/waves con spawning temporal
- Rutas/waypoints para enemigos
- Condición de salida: oleadas
- Spawn points (`*`)
- Trigger condicional

### Lua y defensa de torres
**Tabla `world` (API del mundo 2D)** — 65 funciones en total,
de las cuales 29 son funciones base.
**Eventos de juego**
**Defensa de torres** (guardar/cargar: §16)
- Tile `base` con HP y `al_destruir`
- Tile `spawn` formalizado (nota: modo `random` de oleadas lo ignora — usa modo `en` con coords de tile)
- Tile `slot_torre` para colocación
- Bloque `torre:` con auto-targeting (6 modos)
- Modo construcción con cursor visible
- 11 bindings Lua: `get_base_hp`, `get_base_max_hp`, `damage_base`,
  `set_base_hp`, `enter_build_mode`, `exit_build_mode`, `place_tower`,
  `remove_tower`, `can_place`, `get_tower_cost`, `find_nearest_enemy`
- 5 eventos de esta API: `tile_click`, `tower_placed`, `tower_destroyed`,
  `base_destroyed`, `key_press`
- Ataque a torres mediante campos directos del enemigo
  (`atacan_torres: true`); detiene locomoción, no las habilidades nombradas
  independientes del actor
- Torres indestructibles (`hp: 0`) ignoradas por targeting enemigo
- Grid lookup de colisiones (optimización O(1))
- Mouse input en Gameplay2D

**Inhibición de actividades `Placed` y `Enemy`**
- Tokens positivos, identificados y apilables por instancia.
- Scope global o por `use_id` exacto.
- Retirada aislada; sin reloj, azar, clics ni selección nativa.
- Gate anterior a la nueva adquisición/commit; efectos desprendidos sobreviven.
- El reloj queda listo sin acumular ráfagas durante la inhibición.
- Altas públicas separadas; namespace y retirada por handle compartidos.
- Muerte/retirada del Enemy vuelve obsoletos sus handles.

**Vínculo al obstructor de locomoción actual**
- Campo `adquisicion.vinculo` / `acquisition.binding`, heredable por uso.
- El blanco es exactamente el colocado que detuvo el movimiento del enemigo.
- `independiente/independent` conserva selección normal o apaga la herencia.
- Sin encuentro o elegibilidad válida: target vacío, reloj listo y sin fallback.
- No cambia `atacan_torres`, impacto físico, HP, destrucción ni save.

**Restauración y reemplazo de un `Placed` dañado**
- Consulta HP actual/máximo por ID sin exponer internals de habilidad.
- Restauración in-place: mismo ID y mismo estado runtime salvo HP máximo.
- Reemplazo: misma definición, ID y estado runtime nuevos; sin migración.
- Coste autorado en Lua; cero callbacks, sintaxis `.eclmap`, save o disco.

**Extracción y custodia de un `Placed`**
- Extraer conserva identidad/estado y retira presencia espacial/simulación.
- Redeploy valida destino y devuelve el mismo ID sin callbacks ni coste.
- La inhibición y la restauración alcanzan la instancia custodiada; el reemplazo no la devuelve al mapa.
- Sin captor/anchor, transporte visible, sintaxis `.eclmap`, save o disco.

**Factores de intervalo de actividad**
- Handles positivos sobre `(Placed,use_id)` o `(Enemy,use_id)` mediante altas
  específicas y factores finitos mayores que cero.
- Composición multiplicativa; retirada exacta y tiempo absoluto preservado.
- Restaurar, extraer y volver a colocar conservan factores y handles de `Placed`.
  Reemplazar o destruir `Placed`, o retirar un `Enemy`, invalida sus handles; una instancia nueva nace sin factores.
- Sin degradación automática por HP, sintaxis `.eclmap`, save o disco.

**Factores de alcance de actividad**
- Handles positivos sobre `(Placed,use_id)` y factores finitos mayores que cero.
- Composición multiplicativa; retirada exacta y adquisición futura solamente.
- Sin blanco no hay coste ni ejecución; restaurar y extraer conservan los factores, y reemplazar crea una instancia sin ellos.
- Sin degradación automática por HP, owner enemigo, sintaxis `.eclmap`, save o disco.

**Cambios de daño directo de actividad**
- Handles positivos sobre `(Placed,use_id)` y deltas enteros no cero.
- Composición aditiva con admisión de subconjuntos dentro de `[1, INT_MAX]`.
- El mismo daño se usa para coordinar blancos y aplicar el efecto; restaurar y extraer conservan los cambios.
- Replace/destrucción invalidan; sin carrier, owner enemigo, `.eclmap`, save o disco.

**Cambios de preferencia de actividad**
- Handles positivos sobre `(Placed,use_id)` o `(Enemy,use_id)`; aceptan `mas_cercano/nearest` y `mayor_hp_actual/highest_current_hp`.
- Preferencias absolutas LIFO por inserción; el handle no constituye prioridad.
- Sólo afecta a elecciones futuras; restaurar y extraer conservan los cambios, y reemplazar usa la preferencia de la definición.
- Sin criterios compuestos, API universal de actor, `.eclmap`, query, save o disco.

**Marcador espacial neutral**
- Bloque `marcador/marker` con `rectangulo/rectangle` en píxeles de mundo
- Identidad de marcador y enemigo runtime
- Memoria por pareja, reentrada y entrada inicial de actor nacido dentro
- Cruce AABB barrido entre frames
- Orden estable por marcador e ID de actor
- Hook `marker_enter` sin consecuencia propia

**Guardar y cargar:** política `guardar`, checkpoints y menú compartido con VN (§16). Esto no añade persistencia a los handles y operaciones que declaran explícitamente «sin save o disco».

---

## 14. LÍMITES QUE AFECTAN A LOS EJEMPLOS

- Las rutas usan un punto por línea; no aceptan flechas ni el sufijo `[tiles]`. La velocidad es del enemigo.
- El HUD dibuja HP, HP de base, oleada, texto y contador. `inventario` se carga pero no se dibuja. Para variables usa `contador`; el campo histórico `score` no es su alias y conserva su comportamiento anterior.
- Enemy y Placed tienen contratos distintos. Los bindings de uno no encuentran ni mueven al otro.
- `movimiento: ruta` y `movimiento: seguir` eligen locomoción; no son dos géneros de juego. La obstrucción opt-in y las habilidades de §3 conservan sus propias reglas.
- No hay un límite documental de 80 o 200 renglones: divide tus niveles según su contenido y la lógica de transición que implementes.
- Los recursos, la puesta en escena, el balance y la interacción visual deben probarse en el juego. Que MapLoader conserve los campos no prueba todo eso.

## 15. ALIAS Y CAMPOS COMPLEMENTARIOS

La tabla separa alternativas; escribe una sola grafía. Los alias no autorizan cambiar el dominio de una clave.

| Lugar | Grafías | Qué recibe |
|---|---|---|
| Raíz | `tamano`, `tamaño`, `size` | Lado del jugador en píxeles |
| Raíz | `wall_jump`, `dash_cooldown` | Salto de pared booleano; cooldown de dash en segundos |
| Grid / diálogo NPC | `mapa`/`map`; `elegir`/`choose` | Cabecera de mapa; opciones del diálogo |
| Jugador | `ataque_tipo`/`attack_type`; `ataque_sprite`/`attack_sprite` | Tipo de ataque y ruta del sprite |
| Jugador | `ataque_daño`/`ataque_dano`/`attack_damage`; `ataque_cooldown`/`attack_cooldown` | Daño entero y segundos entre ataques |
| Jugador | `daño_contacto`/`dano_contacto`/`contact_damage` | Recibir daño de contacto, booleano |
| Daño de tile, enemigo o proyectil | `daño`/`dano`/`damage` | Daño entero en ese dominio |
| Enemigo, ataque a torres | `atacan_torres`/`attacks_towers`/`atacar_torres`/`attack_towers` | Activar el ataque a torres mediante campos directos, booleano |
| Enemigo, ataque a torres | `rango_ataque_torre`/`tower_attack_range`; `cadencia_torre`/`tower_attack_rate` | Alcance y cadencia de ese ataque; no modifica habilidades nombradas |
| Proyectil | `tiempo_vida`/`lifetime`; `atraviesa_paredes`/`pierce_walls` | Duración en segundos; atravesar paredes, booleano |
| Ruta | `ruta`/`route`; `recorrido`/`traversal` | Cabecera y política de recorrido |
| Recorrido | `ida_vuelta`/`ping_pong`/`pingpong` | Ida y vuelta |
| Oleadas | `avance`/`advance`; `tiempo`/`delay` | Política del grupo; espera de una oleada |
| Spawn | `en`/`at`; `aleatorio`/`random` | Posición explícita o suelo aleatorio; unidades según §7 y §5 |
| Acciones | `camara`/`camera` con `sacudir`/`shake`; `vn` | Sacudida y entrada a un script VN |
| Guardar | `checkpoint`/`solo_checkpoint`/`checkpoint_only` | Sólo guardado por checkpoint |
| Fin | `estilo`/`style` con `clasico`/`classic`; `duracion_entrada`/`entry_duration` | Estilo clásico; segundos de entrada |
| Fin, colores | `fondo`/`background`; `acento`/`accent`; `boton_hover`/`button_hover` | Colores del panel, acento y botón seleccionado |
| Fin, criterios | `una_estrella`/`one_star`; `dos_estrellas`/`two_stars`; `tres_estrellas`/`three_stars` | Condición para cada cantidad de estrellas |

---


| Lugar | Otras grafías equivalentes | Alcance |
|---|---|---|
| Raíz | `modo`/`mode`, `fuente`/`font`, `velocidad`/`speed` | Modo, ruta de fuente y velocidad del jugador |
| Raíz side-scroll | `doble_salto`/`double_jump`, `dash_velocidad`/`dash_speed`, `dash_duracion`/`dash_duration` | Booleano; velocidad y duración del dash |
| Jugador | `puede_atacar`/`can_attack`, `ataque_rango`/`attack_range`, `invencible`/`invincible` | Ataque habilitado, alcance en píxeles, segundos de invencibilidad |
| Tile | `tile`/`casilla`, `tipo`/`type`, `friccion`/`friction` | Cabecera, tipo y fricción |
| Puerta | `destino`/`destination`, `posicion`/`position`, `cerrada`/`locked_msg` | Mapa destino, tiles de llegada y mensaje |
| Puerta / trigger | `condicion`/`condition` | Expresión que debe cumplirse |
| Enemigo | `movimiento`/`movement`: `quieto`/`still`, `huir`/`flee`, `circular`/`circle`, `rebote`/`bounce` | Tipo de locomoción (§3.2) |
| Eventos | `al_morir`/`on_death`, `al_entrar`/`on_enter`, `al_recoger`/`on_pickup`, `al_completar`/`on_complete` | Enemigo, trigger, item y oleada, respectivamente |
| Item | `item`/`objeto`; `al_usar`/`on_use`; `inventario`/`inventory` | Los dos últimos se parsean, pero no habilitan un inventario utilizable (§3.3) |
| Torre | `donde`/`where` | Tipo de tile donde se permite colocar |
| HUD | `hp_base`/`base_hp` | Visibilidad del HP de base |
| Salida | `hablar_todos`/`talk_all`, `llegar`/`reach`, `matar_todos`/`kill_all`, `recoger_todos`/`collect_all` | Valores alternativos de `salida`, no cabeceras |
| Acciones | `cargar_mapa`/`load_map`, `desaparecer`/`vanish`; `camara: mover`/`camera: pan` | Transición, desaparición y destino de cámara en tiles |

## 16. GUARDAR/CARGAR EN NIVELES 2D

> Podemos decidir si el nivel permite guardar, cuándo y en qué archivo.



Por defecto, un `.eclmap` **no se puede guardar** — el menú de pausa del
gameplay oculta Guardar y Cargar. Para habilitar save, el creador añade
un bloque `guardar:` explícito.

### Sintaxis

```
guardar:
  # nunca | checkpoint | siempre
  permitir: siempre
  # número de slots visibles (1-5)
  slots: 5
  # opcional, default compartido con VN
  archivo: "saves/bosque.eclips"
```

Todos los campos son opcionales. El bloque completo también lo es.

### Campos

| Campo | Alias | Valores | Default | Descripción |
|---|---|---|---|---|
| `permitir:` | `allow:` | `nunca`/`never`, `checkpoint`, `siempre`/`always` | `nunca` | Cuándo puede guardar el jugador |
| `slots:` | — | 1-5 | 5 | Slots visibles en el menú (slot 0 es autosave reservado) |
| `archivo:` | `file:` | ruta a `.eclips` | `saves/vn_saves.eclips` | Archivo propio del nivel (aislado del save compartido del VN) |

### Modos de `permitir:`

- **`nunca`/`never`** — sin save. El menú de pausa oculta Guardar/Cargar.
  Útil para niveles tipo arcade puros, tutoriales breves, cutscenes
  jugables.
- **`checkpoint`** — solo se guarda automáticamente al tocar tiles
  `tipo: checkpoint`. Sin save manual. Como en los plataformeros clásicos.
- **`siempre`/`always`** — save manual completo desde el menú de pausa.
  Estilo RPG/VN convencional.

### Ejemplo mínimo — nivel que usa el save compartido del VN

```
modo: topdown
size: 48

guardar:
  permitir: siempre

jugador:
  sprite: "assets/player/"
  hp: 5

mapa:
#####
#P..#
#####
```

El nivel guarda en `saves/vn_saves.eclips` junto al VN. El slot puede
contener VN-solo, GP-solo, o VN+GP (híbrido) dependiendo de desde dónde
guardó el jugador.

### Ejemplo con archivo propio — un mundo aparte, con sus propias partidas

```
guardar:
  permitir: siempre
  slots: 3
  archivo: "saves/bosque_encantado.eclips"
```

Útil cuando el nivel es un mundo independiente que no debería pisar los
saves del VN principal. La carpeta se crea automáticamente si no existe.

### Slot 0 — autoguardado

Cuando abrimos el menú desde un nivel 2D, el
**slot 0 está reservado para autosave**. Se distingue visualmente:

- Etiqueta "A" en vez de "1"
- Tinte dorado en vez de azul
- Texto "Autoguardado (no sobrescribible)" en modo Save
- Bloqueado para escritura manual desde el menú

El autosave se dispara automáticamente en tiles `tipo: checkpoint`
(si `permitir: checkpoint` o `siempre`). El jugador puede cargarlo como
cualquier otro slot.

### Badge informativo VN/GP/VN+GP

Cada slot ocupado muestra una etiqueta en la esquina superior derecha
que indica qué tipo de datos contiene:

- `[VN]` — solo diálogo/escena del VN
- `[GP]` — solo estado de gameplay 2D
- `[VN+GP]` — híbrido: guardado en un punto donde ambos tienen estado

El creador no controla el badge — se deriva automáticamente del
contenido. No es interactivo.

### Merge silencioso VN ↔ GP

Si un slot contiene `[VN]` y el jugador guarda gameplay encima, pasa a
`[VN+GP]` sin prompt de confirmación. El VN previo se preserva intacto.
Viceversa: guardar VN sobre `[GP]` agrega VN sin borrar GP.

### Prefijos en el archivo `.eclips`

El formato es key-value. Cada slot N usa tres prefijos:

- `slot_N_*` — campos VN (scene, dialog, bg, music, chars, etc.)
- `slot_N_gp_*` — campos gameplay 2D (mappath, hp, pos, vars, etc.)
- `slot_N_time` — metadata genérica (última modificación del slot)

Esto permite borrar un slot completo con un solo prefijo (`slot_3_`) y
actualizar VN o GP de forma independiente sin pisar al otro.

### Limitaciones conocidas

- **Niveles con `archivo:` propio no tienen entrada propia en el
  MainMenu.** El jugador vuelve al nivel relanzándolo desde el VN o
  desde la entrada de gameplay del proyecto. Una entrada "Continuar mundo X" en el MainMenu queda como
  futura mejora.
- **El slot 0 (autosave) solo se escribe automáticamente** — no puede
  guardarse manualmente desde el menú. Sí puede eliminarse (con
  confirmación reforzada).

---

## 17. PANTALLA DE FIN DE NIVEL

El bloque `fin:` define qué se muestra cuando el nivel termina con
resultado: victoria o derrota. Se compone de dos sub-presets
independientes — `victoria:` y `derrota:` — cada uno con su propio
estilo, texto, colores, animación, sonido y siguiente nivel.

Si el bloque se omite, el engine usa un preset default Classic con
texto genérico ("Victoria" o "Derrota"). Niveles viejos que disparen
triggers de fin (oleadas completas, base destruida, muerte del player)
muestran el preset default sin necesidad de editar el `.eclmap`.

### Anatomía

```
fin:
  victoria:
    estilo: estrellas
    texto: "¡Defensa exitosa!"
    posicion: centro
    animacion_entrada: desde_arriba
    duracion_entrada: 0.5
    sonido_entrada: "assets/audio/fanfarria.ogg"
    criterios:
      tres_estrellas: hp_base >= 18
      dos_estrellas:  hp_base >= 10
      una_estrella:   siempre
    colores:
      fondo:        #0A1828
      texto:        #EAF4FF
      acento:       #FFC947
      boton:        #1E3A5F
      boton_hover:  #2D5A99
    siguiente: "nivel2.eclmap"
  derrota:
    estilo: ko
    texto: "La base cayó..."
    animacion_entrada: desvanecer
```

### Estilos disponibles

| Estilo | Cuándo usarlo |
|--------|---------------|
| `clasico` / `basic` | Default. Título + botones. Para lo que no requiera ceremonia visual. |
| `ko` | Pantalla dramática con shake + flash blanco + tinte rojo persistente. Ideal para derrotas estilo arcade/fighting. |
| `estrellas` / `stars` | 1-3 medallas con cascada según criterios evaluados. Patrón clásico de TD móvil. |
| `tally` / `puntaje` | Lista de stats con contadores animados de 0 → target. Ideal para arcades, plataformeros, score-based. |

### Texto y posición

```
fin:
  victoria:
    # Si se omite, usa "Victoria" o "Derrota"
    texto: "Mensaje del título"
    # centro (default) | arriba | abajo
    posicion: centro
                                       # center / top / bottom
```

### Animaciones de entrada

```
fin:
  victoria:
    # entry_animation
    animacion_entrada: desde_arriba
    # Segundos. Default 0.4. Clamp 0.05-3.0.
    duracion_entrada: 0.5
```

| Animación | Comportamiento |
|-----------|----------------|
| `desvanecer` / `fade` | Default. Alpha 0 → 1, todo aparece a la vez. |
| `desde_arriba` / `from_top` | El panel cae desde arriba con overshoot al settle. |
| `desde_abajo` / `from_bottom` | El panel sube desde abajo con overshoot. |
| `zoom` | Crece desde el centro con overshoot. El contenido (texto, botones) aparece al final, no durante. |
| `ninguna` / `none` | Aparece instantáneo, sin animación. |

En `desde_arriba`, `desde_abajo` y `zoom`, el contenido interno
(cascada de medallas, filas de tally) **espera** a que la animación de
entrada termine antes de empezar. En `desvanecer`, todo hace fade-in
simultáneo. En `ninguna`, todo aparece instantáneo.

### Sonido de entrada

```
fin:
  victoria:
    # entry_sound
    sonido_entrada: "assets/audio/fanfarria.ogg"
```

Se reproduce una sola vez al pushear la pantalla, sincronizado con la
animación. Formato libre — cualquier `.ogg`/`.wav`/`.mp3` que soporte
miniaudio. Si la ruta es inválida, se loguea error y la pantalla
aparece igual sin audio.

### Sub-bloque `colores:` / `colors:`

Define la paleta del panel. Todos los campos son hexadecimales `#RRGGBB`.
Todos son opcionales — los que se omiten usan defaults sensibles.

```
fin:
  victoria:
    colores:
      # Color del rectángulo del panel
      fondo:        #1A1A2E
      # Texto de botones, hint, labels de tally
      texto:        #EAEAEA
      # Borde, título, separador, medallas, valores tally, indicador ">"
      acento:       #FFC947
      # Botones no seleccionados
      boton:        #2C2C44
      # Botón seleccionado
      boton_hover:  #4A4A66
```

El `acento:` es el "color de identidad" del preset — afecta múltiples
elementos visuales a la vez para mantener coherencia con una sola
línea. Si solo quieres dar personalidad rápida, escribe únicamente
`acento:` y deja el resto al default.

### Sub-bloque `criterios:` / `criteria:` (solo para `estilo: estrellas`)

Define qué condiciones dan cuántas estrellas. Se evalúan en orden — el
primer criterio que matchea gana.

```
fin:
  victoria:
    criterios:
      tres_estrellas: hp_base >= 18
      dos_estrellas:  hp_base >= 10
      una_estrella:   siempre
```

Operadores soportados: `>=`, `<=`, `>`, `<`, `==`, `!=`. La condición
especial `siempre` / `always` matchea sin condición — úsala como
fallback de "una estrella mínimo si llegaste hasta aquí".

### Sub-bloque `mostrar:` / `show:` (solo para `estilo: tally`)

Define las filas del tally. La key es el label que se muestra; el value
es el nombre de una variable a leer del engine.

```
fin:
  victoria:
    mostrar:
      "HP base restante": hp_base
      "Oro acumulado":    oro
      "Tiempo (seg)":     tiempo
      "Oleada alcanzada": oleada
```

Si la variable resuelve a un número, se anima de 0 → target con
ease-out cubic. Si resuelve a un string (ej. `"1:23"`), se muestra
fijo. Las filas hacen fade-in escalonadas con 0.20s de delay entre
cada una.

### Variables especiales del engine

Disponibles tanto en `criterios:` como en `mostrar:`:

| Var | Significado |
|-----|-------------|
| `hp` | HP actual del jugador |
| `hp_max` | HP máximo del jugador |
| `hp_base` | HP actual de la base (TD) |
| `hp_base_max` | HP máximo inicial de la base |
| `oleada` | Número de oleada alcanzada |
| `oleada_max` | Total de oleadas configuradas |
| `tiempo` | Segundos transcurridos en el nivel |

Además, cualquier variable custom seteada con `set:` en el `.eclmap` o
con `world.set_var()` desde Lua es accesible por su nombre.

### Siguiente nivel

```
fin:
  victoria:
    # next
    siguiente: "nivel2.eclmap"
```

Si la victoria define un `siguiente:`, aparece un tercer botón
"Siguiente" en el panel. Al pulsarlo, el engine carga ese `.eclmap` en
lugar del actual. Si no se define, solo aparecen "Reintentar" y "Menu
Principal".

### Hooks Lua: `victory` y `defeat`

Para lógica que va más allá de lo declarativo, los hooks `victory` y
`defeat` permiten ejecutar código Lua justo antes de mostrar el panel,
con la opción de modificar texto, estrellas y tally al vuelo:

```lua
world.on("victory", function(stars_default)
    -- stars_default es lo que el engine calculó según los criterios.
    -- Si retornamos una tabla, sobrescribe el preset.
    if jugador_logro_secreto then
        return {
            estrellas = stars_default + 1,   -- bonus
            texto = "¡Hazaña épica!",
            tally = {
                { nombre = "Tiempo récord", valor = "1:23" },
                { nombre = "Oro sobrante",  valor = oro }
            }
        }
    end
    -- Sin return → se usa el preset del .eclmap tal cual.
end)

world.on("defeat", function()
    -- defeat no recibe args — consulta world.get_player_hp() o
    -- world.get_base_hp() si necesitas distinguir el motivo.
    return { texto = "Lo intentaste..." }
end)
```

Claves del retorno-tabla (todas opcionales, bilingües):

| Clave | Tipo | Efecto |
|-------|------|--------|
| `estrellas` / `stars` | int | Override del cálculo de criterios (solo `victory`) |
| `texto` / `text` | string | Override del título del panel |
| `tally` | array de `{nombre, valor}` | Override de las filas de tally — los valores son literales finales, no se hace lookup |

También están las funciones `world.victory()` y `world.defeat()` para
disparar el fin de nivel manualmente desde Lua (útil para mecánicas
custom: matar al boss, recolectar todas las llaves, sobrevivir N
segundos sin ser visto).

### Ejemplos por género

**TD móvil con estrellas y next-level:**
```
fin:
  victoria:
    estilo: estrellas
    animacion_entrada: desde_arriba
    sonido_entrada: "assets/audio/win.ogg"
    criterios:
      tres_estrellas: hp_base >= 18
      dos_estrellas:  hp_base >= 10
      una_estrella:   siempre
    siguiente: "nivel2.eclmap"
  derrota:
    estilo: ko
```

**Arcade con tally:**
```
fin:
  victoria:
    estilo: tally
    texto: "¡STAGE CLEAR!"
    animacion_entrada: zoom
    duracion_entrada: 0.6
    mostrar:
      "Score":     score
      "Time":      tiempo
      "Combo max": max_combo
  derrota:
    estilo: clasico
    texto: "GAME OVER"
```

**VN puro con clasico minimal:**
```
fin:
  victoria:
    estilo: clasico
    texto: "Fin del capítulo"
    animacion_entrada: desvanecer
    siguiente: "capitulo2.eclmap"
```

### Limitaciones conocidas

- **Las medallas del estilo `estrellas` presentan un artefacto visual
  menor** (línea horizontal atravesándolas). Se atribuye al render de
  `drawCircle` o al cálculo de coordenadas — pendiente de polish, no
  afecta funcionalidad.
- **El número máximo de medallas es 3** (hardcoded). Si un hook Lua
  retorna `estrellas = 4` o más, el render solo dibuja 3.
- **Customización Nivel 3** (timelines coreografiados, primitivas de
  dibujo expuestas a Lua, animar elementos individuales) no está
  implementada — reservada para sesión dedicada futura.
- **Panel de victoria con espera** (delay entre fin del gameplay y aparición del
  panel, con dim progresivo del fondo) no soportado en el engine —
  implementable manualmente desde Lua mediante `world.victory()` con
  timer custom. Sesión futura puede formalizarlo.

---
