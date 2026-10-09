# Testigo — demo de novela visual y juego 2D.
# Texto: Jesús.
#
# Cada "escena" es un punto del guion; "ir_a" cambia a otra escena.
# Las líneas como sol: "..." muestran diálogo y esperan al jugador.
# "elegir" abre ramas que pueden reunirse o llevar a distintos finales.
#
# En el VN miramos desde el jugador; su sprite se controla en los mapas 2D.
# Las posiciones están ajustadas al lienzo de diseño de 1920 × 1080.
# Si cambias el arte, las fuentes o sus tamaños, revisa también la colocación.
#
# Dónde ver cada cosa (busca "escena <nombre>"):
#   inicio        texto con fuente propia, fundidos con capa_fade, sonido
#   interrupcion  un personaje que se asoma: animaciones con mover_x
#   despertar     por qué casi todo dice "centro"; un personaje en piezas
#                 (cuerpo y ojos como capas); al final, la primera elección
#   hola          un paso hacia el jugador: escala y altura en una receta
#   forma         cambiar el punto de vista; al final, entrar a un mapa 2D
#                 (gameplay) y seguir según lo que dejó
#   luna_partes   ponerle algo delante al reparto: pasarlo a capas
#   quedarnos     fundir una ilustración sobre otra sin que se oscurezca
#   sol_golpe     un texto narrado, y cómo se centra en inglés

# Aquí declaramos las rutas de los sprites. "project.eclips" define los nombres
# visibles, las apariencias y las cajas de diálogo de cada personaje.
# "neutral" es un nombre de esa tabla; un número como "013" elige "Sol_013.png".
# La ruta base sirve para construir las variantes. "look_default neutral"
# selecciona la cara inicial; para restablecerla después, la nombramos al mostrar.
personaje sol "assets/demo/sol/Sol.png"

# La pose de Sol asomándose usa otro alias y otra carpeta, pero conserva
# su nombre y su caja. Indicamos siempre una expresión: aquí no hay imagen base.
personaje solasoma "assets/demo/sol/Sol images/Sol.png"

personaje luna "assets/demo/luna/Luna.png"
personaje estrella "assets/demo/estrella/Estrella.png"


# Apertura: una página blanca con texto que entra por fundido.
# "texto" permite presentar la narración fuera de la caja de diálogo.

escena inicio
  ocultar_caja

  # La intro usa dos pistas; la segunda arranca al avanzar el diálogo.
  # Aquí van como "sonido" para reproducirse una vez: "musica" activa un bucle.
  # Cada llamada dispara un efecto independiente; pueden sonar varios a la vez.
  # "sonido" no espera a que acabe la pista: el guion continúa inmediatamente.
  # Estas pistas responden al volumen de efectos, no a "parar_musica".
  # Sin volumen por efecto en el guion, su nivel se ajusta en el archivo de audio.
  sonido "assets/musica/intro_a.ogg"

  # La capa nace visible. La ocultamos antes de hacerla aparecer desde negro.
  capa blanco "#FFFFFF" z=900 x=0 y=0 w=1920 h=1080
  capa_visible blanco no
  capa_fade blanco aparecer 2.5
  esperar 1.0

  # Un clic no adelanta "esperar"; una pausa dentro del diálogo, como
  # "{pausa=0.8}", sí puede adelantarse. El modo skip también salta las esperas
  # ordinarias; más adelante usamos "cinematica" cuando deben respetarse.

  esperar 2.5

  # La fuente se declara dentro de la escena, antes del primer texto que la usa.
  fuente cuento "third_party/fonts/m_plus_rounded_1c/MPLUSRounded1c-Regular.ttf" tamano=48

  # "x" e "y" son coordenadas del lienzo de diseño. Cada línea tiene su propio
  # centrado; si cambias la fuente o su tamaño, revisa esas posiciones.
  # Cada línea entra, permanece para leerla y se retira antes de la siguiente.
  # "capa_fade" no detiene el guion: los "esperar" dan tiempo a esos fundidos.


  texto n1 "En un mundo suspendido entre el olvido y la memoria..." x=523 y=512 color=#1a1a1a z=950 fuente=cuento
  capa_visible n1 no
  capa_fade n1 aparecer 2.5
  esperar 4.4
  capa_fade n1 desaparecer 2.0
  esperar 2.0
  esperar 1.0

  texto n2 "tres almas aguardan el momento de despertar..." x=578 y=512 color=#1a1a1a z=950 fuente=cuento
  capa_visible n2 no
  capa_fade n2 aparecer 2.5
  esperar 2.5
  esperar 3.0
  capa_fade n2 desaparecer 2.0
  esperar 2.0
  esperar 1.0

  # Sol interrumpe la frase y el texto pasa de oscuro a tenue sobre el blanco.
  # Cruzamos los fundidos de dos copias, en la misma posición y de distinto color.
  # "capa_fade" funde de 0 a 1 al aparecer y del alpha actual a 0 al desaparecer;
  # no permite elegir un alpha final intermedio. Aquí la copia clara fija el tono.
  # Las dos nacen ocultas para que cada una se vea sólo cuando le toca.

  texto n3  "y tú, viajero, has llegado para..." x=708 y=512 color=#1a1a1a z=950 fuente=cuento
  texto n3t "y tú, viajero, has llegado para..." x=708 y=512 color=#C9C4BC z=950 fuente=cuento
  capa_visible n3 no
  capa_visible n3t no

  capa_fade n3 aparecer 2.5
  esperar 2.5
  esperar 2.5

  ir_a interrupcion


# Interrupción: Sol se asoma y la página pierde fuerza.
# Los fundidos cruzados de "n3" y "n3t" conservan el texto mientras se atenúa.

escena interrupcion
  # Declaramos las animaciones antes de usarlas. "mover_x" desplaza el dibujo
  # respecto de su ancla; estos offsets alinean el borde de esta pose con
  # el borde de pantalla. Revísalos si cambias el dibujo o su tamaño.
  # Estas recetas conservan su valor final hasta que las detenemos.
  animacion asoma_entra mover_x de=-330 a=113 en=0.4 curva=salida
  animacion asoma_sale alpha de=1 a=0 en=1.2 curva=suave
  # Esta receta coloca a Sol ya asomada, sin repetir el deslizamiento.
  animacion asoma_queda mover_x de=113 a=113 en=0.05

  # La entrada instantánea deja ver la pose mientras empieza a deslizarse,
  # antes de que llegue su caja de diálogo.
  mostrar solasoma 002 izquierda [efecto: instant]
  animar solasoma asoma_entra

  capa_fade n3 desaparecer 0.7
  capa_fade n3t aparecer 0.7

  esperar 0.3
  mostrar_caja
  solasoma: "¿Qué escribes?"

  # La segunda mitad de la intro empieza con el clic que avanza la línea de Sol.
  sonido "assets/musica/intro_b.ogg"

  animar solasoma asoma_sale
  ocultar_caja

  capa_fade n3t desaparecer 1.2
  capa_fade n3 aparecer 1.2

  texto n4 "...para..." x=895 y=589 color=#1a1a1a z=950 fuente=cuento
  capa_visible n4 no
  capa_fade n4 aparecer 1.2
  # La cuarta línea también tiene una copia tenue para el segundo atenuado.
  texto n4t "...para..." x=895 y=589 color=#C9C4BC z=950 fuente=cuento
  capa_visible n4t no
  esperar 1.2

  # Después del fade retiramos la receta de alpha antes de ocultar a Sol:
  # si quedara en cero, su siguiente entrada podría seguir invisible.
  detener_animacion solasoma
  ocultar solasoma [efecto: instant]
  esperar 1.4

  esperar 2.0

  # En la segunda entrada Sol ya está en su sitio y aparece con un fade corto.
  mostrar solasoma 005 izquierda [efecto: fade]
  animar solasoma asoma_queda

  capa_fade n3 desaparecer 0.7
  capa_fade n3t aparecer 0.7
  capa_fade n4 desaparecer 0.7
  capa_fade n4t aparecer 0.7

  esperar 0.3
  mostrar_caja
  solasoma: "Luna. ¿Qué escribes?"

  capa_fade n3t desaparecer 1.0
  capa_fade n4t desaparecer 1.0
  esperar 1.0

  luna: "El intro."

  # Al cambiar la cara repetimos la posición para conservar el ancla.
  # Sin posición, "mostrar" usa "centro"; esta pose debe seguir en "izquierda".
  mostrar solasoma 003 izquierda
  solasoma: "Está muy pretencioso."

  luna: "Es una visual novel. Se supone que es pretencioso."

  mostrar solasoma 005 izquierda
  solasoma: "Se supone que cargue."

  ir_a despertar


# Despertar: Sol y Luna juntas; después dejan espacio a Estrella.
#
# ¿Por qué casi todos los "mostrar" dicen "centro"? Porque "centro" es el
# ANCLA, no el lugar donde se ven. Las tres se paran en el mismo punto y las
# recetas "mover_x" corren a cada una hacia su lado (en el trío: Sol -578,
# Luna +530, Estrella -20). Los desplazamientos de distintas recetas se suman:
# en Sol, -270 y -308 dan -578. Si cambias la escala del arte, revisa también
# estos offsets.
#
# Si no escribes posición, el motor pone "centro" solo: "mostrar luna 08" y
# "mostrar luna 08 centro" son el mismo comando. Lo escribimos igual porque a
# quien ya está en otro lado, un "mostrar" sin posición lo trae al centro: por
# eso Sol asomándose repite "izquierda" cada vez que cambia de cara.
# El fondo es una capa de color: puedes cambiar su tono en "capa mundo".

escena despertar
  capa mundo "#E9E5DE" z=10 x=0 y=0 w=1920 h=1080
  # Para animar la mirada separamos a Sol en cuerpo y ojos: dos capas propias, en
  # vez de un personaje compuesto del motor. Hoy nos resulta más cómodo así, sobre
  # todo para mover los ojos. "z" pone los ojos encima del cuerpo, y sus coordenadas
  # coinciden con el retrato al que sustituyen; las capas nacen ocultas y solo se
  # muestran durante esa mirada.
  capa cuerpo "assets/demo/sol/Sol_Compuesto/0.png" x=231 y=144 w=918 h=918 z=30
  capa ojos "assets/demo/sol/Sol_Compuesto/02.png" x=139 y=144 w=918 h=918 z=40
  capa_visible cuerpo no
  capa_visible ojos no

  # Este segundo juego de capas coincide con Sol después de apartarse
  # para Estrella. Las piezas de "Sol_Compuesto" son dibujos independientes:
  # su número no corresponde necesariamente a la cara completa del mismo número.
  capa cuerpo_trio "assets/demo/sol/Sol_Compuesto/0.png" x=-77 y=144 w=918 h=918 z=30
  capa ojos_09 "assets/demo/sol/Sol_Compuesto/09.png" x=-17 y=144 w=918 h=918 z=40
  capa ojos_07 "assets/demo/sol/Sol_Compuesto/07.png" x=-25 y=144 w=918 h=918 z=40
  capa ojos_07b "assets/demo/sol/Sol_Compuesto/07.png" x=-41 y=144 w=918 h=918 z=40
  capa_visible cuerpo_trio no
  capa_visible ojos_09 no
  capa_visible ojos_07 no
  capa_visible ojos_07b no

  # Las recetas "_queda" fijan una colocación; "_aparta2" añade el movimiento
  # para dejar sitio a Estrella sobre esa colocación.
  animacion sol_queda mover_x de=-270 a=-270 en=0.05
  animacion luna_queda mover_x de=313 a=313 en=0.05
  animacion sol_trio_queda mover_x de=-578 a=-578 en=0.05
  animacion sol_aparta2 mover_x de=0 a=-308 en=1.0 curva=suave
  animacion luna_aparta2 mover_x de=0 a=217 en=1.0 curva=suave
  animacion estrella_pos mover_x de=-20 a=-20 en=0.05
  # Una receta de "alpha" permite elegir la duración de la entrada.
  animacion entra_suave alpha de=0 a=1 en=1.0 curva=suave
  animacion entra_lenta alpha de=0 a=1 en=1.6 curva=suave

  capa_fade blanco desaparecer 2.4
  ocultar solasoma [efecto: fade]

  esperar 2.4

  mostrar sol mira_der centro [efecto: fade]
  animar sol sol_queda
  esperar 1.0

  # "instant" evita el fade de entrada para usar únicamente "entra_suave".
  mostrar luna mira_izq centro [efecto: instant]
  animar luna luna_queda
  animar luna entra_suave
  esperar 1.0
  musica "assets/musica/tema_vn.ogg"

  luna: "Ah. Ya."
  sol: "Tardó."
  luna: "No tanto."
  sol: "Lo pone la pestaña."

  luna: "¿Qué pestaña?"

  mostrar sol mira_izq centro
  sol: "Esa. La negra."

  # "{esperar_click}" detiene la línea en ese punto hasta el siguiente clic.
  mostrar luna dudosa centro
  luna: "...{esperar_click}¿Y esa qué es?"

  sol: "No sé."

  # Cambiamos el retrato por las capas y movemos solo los ojos hacia Luna.
  # El diálogo deja la mirada allí hasta que el jugador avance.
  ocultar sol [efecto: instantaneo]
  capa_visible cuerpo si
  capa_visible ojos si
  capa_mover ojos a (291, 144) en 0.3 suave
  sol: "Pero dice cosas."
  capa_mover ojos a (139, 144) en 0.35 suave
  esperar 0.4

  mostrar luna neutral centro
  esperar 0.9
  mostrar luna dudosa centro
  esperar 0.6
  mostrar luna neutral centro
  esperar 2.0

  luna: "...Oye."
  sol: "¿Qué?"
  luna: "Eso."
  sol: "¿Qué cosa?"
  luna: "Eso de ahí. En medio."

  capa_mover ojos a (231, 144) en 0.8 suave
  esperar 1.3
  sol: "Ah. El jugador."

  # Al volver al retrato, limpiamos sus animaciones y reponemos su posición
  # antes de ocultar las capas. Ambos dibujos coinciden durante el cambio.
  mostrar sol neutral centro [efecto: instantaneo]
  detener_animacion sol
  animar sol sol_queda
  capa_visible cuerpo no
  capa_visible ojos no

  mostrar luna abierta centro
  luna: "¿El jugador?"

  mostrar sol mira_der centro
  sol: "Sí."

  luna: "Pero no es un cubo."

  mostrar sol neutral centro
  sol: "No."

  mostrar luna 023 centro
  luna: "Entonces ¿qué es?"
  sol: "No sé. Algo."

  mostrar luna mira_izq centro
  luna: "¿Algo como qué?"

  # "{pausa=0.8}" introduce un respiro dentro de la misma línea de diálogo.
  sol: "Como...{pausa=0.8} un agujero. Pero al revés."

  mostrar luna dudosa centro
  luna: "¿Un agujero al revés?"

  mostrar sol plana centro
  sol: "Un agujero que mira."

  mostrar luna neutral centro
  parar_musica
  esperar 1.6

  mostrar luna 017 centro
  sonido "assets/sonidos/eso_es_peor.wav"
  luna: "...Eso es peor."

  # Las recetas de Sol y Luna las apartan mientras entra Estrella en el centro.
  mostrar estrella neutral centro [efecto: instant]
  animar estrella estrella_pos
  animar estrella entra_lenta
  animar sol sol_aparta2
  animar luna luna_aparta2
  esperar 1.6
  esperar 0.6

  musica "assets/musica/tema_vn_con_estrella.ogg"
  estrella: "Ah."

  mostrar sol mira_der centro
  estrella: "Sí está ahí."

  mostrar luna mira_izq centro
  luna: "¿Tú lo ves?"

  mostrar estrella 010 centro
  estrella: "No."
  mostrar estrella neutral centro

  luna: "¿Entonces?"
  estrella: "Pero está."
  luna: "¿Cómo sabes?"

  # Una nueva línea de diálogo sustituye el texto anterior. Para continuar
  # la frase con otra cara, repetimos la parte ya leída a velocidad alta
  # y restauramos la velocidad normal antes de escribir la parte nueva.
  mostrar estrella 04 centro
  estrella: "Porque el cielo tampoco se ve"
  mostrar estrella 05 centro
  estrella: "{velocidad=200}Porque el cielo tampoco se ve {velocidad=1}y está."

  mostrar luna neutral centro
  esperar 1.8

  mostrar sol 013 centro
  sol: "...Técnicamente tiene razón."

  mostrar luna 021 centro
  luna: "No tiene razón."

  mostrar estrella 03 centro
  estrella: "Tengo razón."

  esperar 0.8
  mostrar estrella neutral centro
  esperar 0.35
  mostrar sol neutral centro
  esperar 0.35
  mostrar luna neutral centro
  esperar 1.2

  luna: "...Mmm."

  mostrar sol mira_der centro
  sol: "¿Qué?"

  luna: "Yo creo que es un testigo."

  ocultar sol [efecto: instantaneo]
  capa_visible cuerpo_trio si
  capa_visible ojos_09 si
  sol: "¿Un qué?"

  mostrar luna mira_izq centro
  luna: "Un testigo. Alguien que está ahí sin estar. Como en las novelas."

  mostrar sol 013 centro [efecto: instantaneo]
  detener_animacion sol
  animar sol sol_trio_queda
  capa_visible ojos_09 no
  capa_visible cuerpo_trio no
  sol: "Eso no es una persona, Luna."

  mostrar luna 02 centro
  luna: "¡Es una figura narrativa!"

  sol: "Es un agujero."

  mostrar luna 020 centro
  luna: "¡Es un agujero con propósito!"

  ocultar sol [efecto: instantaneo]
  capa_visible cuerpo_trio si
  capa_visible ojos_07 si
  sol: "Los agujeros no tienen propósito."

  mostrar sol neutral centro [efecto: instantaneo]
  detener_animacion sol
  animar sol sol_trio_queda
  capa_visible ojos_07 no
  capa_visible cuerpo_trio no
  mostrar luna neutral centro
  luna: "Bueno, el punto es que no podemos llamarlo agujero."

  mostrar sol mira_der centro
  sol: "¿Por qué?"
  mostrar luna 03 centro
  luna: "Porque está aquí."
  sol: "Los agujeros también."

  mostrar luna 022 centro
  luna: "¡No de esa forma!"

  ocultar sol [efecto: instantaneo]
  capa_visible cuerpo_trio si
  capa_visible ojos_07b si
  sol: "¿Entonces de cuál?"

  mostrar luna 020 centro
  luna: "¡Es...!"

  esperar 2.0

  mostrar luna 017 centro
  luna: "...No sé."

  mostrar sol neutral centro [efecto: instantaneo]
  detener_animacion sol
  animar sol sol_trio_queda
  capa_visible ojos_07b no
  capa_visible cuerpo_trio no
  mostrar luna neutral centro
  sol: "Ah."

  mostrar luna mira_izq centro
  luna: "Pero "agujero" no."

  mostrar luna neutral centro
  sol: "Eso parece."

  ir_a hablar


# Primera elección: hablar o guardar silencio.
# "mostrar_caja" conserva el diálogo anterior mientras aparecen las opciones.
# Cada flecha de "elegir" nombra la escena que sigue a esa respuesta.

escena hablar
  mostrar_caja
  elegir:
    "Hablar." -> siguio
    "No hablar." -> no_hablar

escena no_hablar
  mostrar sol 02 centro
  mostrar luna 01 centro
  mostrar estrella 01 centro
  cinematica {
    esperar 1.2
  }
  luna: "..."
  mostrar luna 011 centro
  luna: "No dijo nada."
  sol: "No."
  mostrar luna 04 centro
  luna: "..."
  luna: "¿Y ahora?"
  mostrar estrella 03 centro
  estrella: "Podemos saludarlo."
  mostrar sol 04 centro
  sol: "Eso podemos hacerlo."
  mostrar luna 01 centro
  luna: "..."
  mostrar sol 02 centro
  mostrar estrella 01 centro
  luna: "Bueno."
  # Las dos respuestas se reúnen en "hola", donde ocurre el mismo saludo.
  ir_a hola

escena siguio
  # El silencio separa la elección de su reacción: Luna tarda en darse cuenta.
  esperar 1.4

  mostrar luna abierta centro
  luna: "Habló."

  mostrar sol plana centro
  sol: "O clickeó."

  estrella: "Es lo mismo si nadie le contestaba."

  mostrar luna neutral centro
  esperar 1.2

  ir_a hola


# Saludo: Luna se acerca al jugador y después vuelve al grupo.
# Un paso hacia el frente se simula aumentando escala y bajando el dibujo.
# Una receta puede reunir varios canales: "luna_paso1" cambia escala y altura.
# Para volver, compensamos los desplazamientos y usamos escalas inversas:
# las escalas se multiplican, así que deshacer ×1.08 requiere ×(1 / 1.08).

escena hola
  animacion luna_trio_queda mover_x de=530 a=530 en=0.05
  animacion luna_paso1 escala de=1 a=1.08 en=0.45 curva=suave
  animacion luna_paso1 mover_y de=0 a=22 en=0.45 curva=suave
  animacion luna_al_centro mover_x de=0 a=-530 en=0.9 curva=suave
  animacion luna_paso2 escala de=1 a=1.07 en=0.45 curva=suave
  animacion luna_paso2 mover_y de=0 a=26 en=0.45 curva=suave
  animacion estrella_a_der mover_x de=0 a=550 en=0.9 curva=suave
  animacion luna_atras2 escala de=1 a=0.934579 en=0.45 curva=suave
  animacion luna_atras2 mover_y de=0 a=-26 en=0.45 curva=suave
  animacion luna_a_der mover_x de=0 a=530 en=0.9 curva=suave
  animacion estrella_a_centro mover_x de=0 a=-550 en=0.9 curva=suave
  animacion luna_atras1 escala de=1 a=0.925926 en=0.45 curva=suave
  animacion luna_atras1 mover_y de=0 a=-22 en=0.45 curva=suave

  ocultar_caja

  # Los personajes se dibujan en su orden de entrada. Volvemos a mostrar
  # a Luna para que pase delante de Estrella; una capa con su mismo dibujo
  # cubre el cambio. "cinematica" respeta estas esperas aun con skip activo.
  # Limpiamos las animaciones antes de colocarla de nuevo para no sumar
  # dos veces el desplazamiento.
  cinematica {
  capa luna_doble "assets/demo/luna/Luna_01.png" x=1031 y=149 w=918 h=918 z=60
  ocultar luna [efecto: instantaneo]
  esperar 0.05
  mostrar luna neutral centro [efecto: fade]
  detener_animacion luna
  animar luna luna_trio_queda
  esperar 0.45
  capa_visible luna_doble no
  }

  mostrar estrella 010 centro
  animar luna luna_paso1
  esperar 0.5

  mostrar luna mira_izq centro
  animar luna luna_al_centro
  esperar 0.95

  mostrar luna neutral centro
  animar luna luna_paso2
  esperar 0.5

  animar estrella estrella_a_der
  esperar 0.95
  mostrar estrella neutral centro

  esperar 0.6

  mostrar luna 02 centro
  luna: "Hola."
  mostrar luna neutral centro
  luna: "No te hemos puesto cara todavía. No es personal."
  ir_a despues_hola

# Después del saludo: el jugador necesita un cuerpo.
escena despues_hola
  mostrar sol neutral centro
  sol: "Es un poco personal."

  estrella: "Puede quedarse. El centro ya se acostumbró."

  esperar 1.2

  mostrar luna neutral centro
  luna: "Bien."

  # Las recetas de regreso deshacen la caminata y devuelven a Estrella al centro.
  ocultar_caja
  mostrar luna mira_izq centro
  esperar 0.35
  animar luna luna_atras2
  esperar 0.5
  mostrar luna 06 centro
  animar luna luna_a_der
  mostrar estrella 09 centro
  animar estrella estrella_a_centro
  esperar 0.95
  mostrar estrella neutral centro
  mostrar luna mira_izq centro
  esperar 0.4
  animar luna luna_atras1
  esperar 0.5

  mostrar luna neutral centro
  esperar 0.4
  luna: "Entonces…"

  mostrar luna 02 centro
  luna: "El viajero dio un paso."

  # Aquí el silencio hace visible la espera de las tres: el jugador no da el paso.
  esperar 2.2
  mostrar luna neutral centro
  esperar 0.6

  mostrar luna 08 centro
  luna: "…{pausa=0.7}Dio un paso."

  mostrar sol plana centro
  sol: "No dio nada."

  # "{interrumpir}" pasa a la siguiente línea sin esperar otro clic.
  mostrar luna mira_izq centro
  luna: "Es que—{interrumpir}"

  mostrar sol mira_der centro
  sol: "No tiene dónde."
  luna: "Tiene el centro."
  mostrar sol 013 centro
  sol: "El centro no es un sitio. Es un hueco."

  mostrar luna 010 centro
  luna: "¡Es un viajero!"

  sol: "Es un input."

  mostrar sol mira_der centro
  mostrar luna mira_izq centro
  estrella: "¿Cómo estás?"

  mostrar sol neutral centro
  mostrar luna neutral centro
  esperar 2.0

  mostrar estrella 07 centro
  estrella: "…Bueno."

  mostrar luna mira_izq centro
  luna: "Tiene que ir."
  mostrar luna neutral centro
  luna: "Hacia nosotros."

  sol: "Y no va."

  mostrar luna mira_izq centro
  luna: "Porque no puede."

  luna: "Oye. ¿Cómo se supone que va a pasar todo lo que escribí?"

  mostrar sol mira_der centro
  mostrar estrella 09 centro
  sol: "Lo que escribiste todavía no existe."

  mostrar luna neutral centro
  mostrar estrella 010 centro
  luna: "El ir. El acercarse. El…"

  mostrar sol neutral centro
  mostrar estrella 07 centro
  sol: "El verbo."

  mostrar luna 021 centro
  luna: "No lo pongas así."

  mostrar estrella neutral centro
  mostrar sol mira_der centro
  mostrar luna mira_izq centro
  estrella: "Podemos darle una."

  # Reutilizamos el cuerpo y los ojos de Sol: declarar de nuevo una capa con el
  # mismo id actualiza la que ya existe, no la duplica.
  # (Sol regresará en Avengers: Doomsday.)
  capa cuerpo_trio "assets/demo/sol/Sol_Compuesto/0.png" x=-77 y=144 w=918 h=918 z=30
  capa ojos_09 "assets/demo/sol/Sol_Compuesto/09.png" x=-17 y=144 w=918 h=918 z=40
  ocultar sol [efecto: instantaneo]
  capa_visible cuerpo_trio si
  capa_visible ojos_09 si
  sol: "¿Una qué?"

  mostrar estrella 09 centro
  estrella: "Una forma."

  mostrar luna 010 centro
  mostrar estrella 010 centro
  luna: "Sí."

  mostrar sol 011 centro [efecto: instantaneo]
  detener_animacion sol
  animar sol sol_trio_queda
  capa_visible ojos_09 no
  capa_visible cuerpo_trio no
  mostrar luna neutral centro
  mostrar estrella neutral centro
  sol: "¿Cómo se supone que se hace eso?"

  esperar 3.0

  mostrar luna mira_izq centro
  luna: "…Chico."
  mostrar luna neutral centro
  luna: "Y que mire."

  esperar 1.0

  mostrar sol mira_der centro
  sol: "¿De qué color?"

  luna: "El del centro."

  mostrar sol 013 centro
  sol: "Eso no es un color."

  luna: "Ahora sí."

  ir_a forma


# La forma: el jugador recibe un cuerpo y cambia su punto de vista.
# "camara_baja" sube los retratos 40 px para simular que ahora los ve desde abajo.
#
# El apagón que separa las dos vistas es un fundido a negro, y ADEMÁS bajamos
# el alpha del reparto. Un barrido no alcanzaría: las capas se dibujan SIEMPRE
# detrás de los personajes, aunque tengan un "z" alto, así que pasaría por
# debajo y dejaría al reparto a la vista.

escena forma
  animacion se_acercan escala de=1 a=1.07 en=0.7 curva=suave
  animacion se_acercan mover_y de=0 a=20 en=0.7 curva=suave
  animacion se_apagan alpha de=1 a=0 en=1.0 curva=suave
  animacion se_alejan escala de=1 a=0.934579 en=0.05
  animacion se_alejan mover_y de=0 a=-20 en=0.05
  # Fijamos la altura desde el primer cuadro, también al volver de las capas.
  animacion camara_baja mover_y de=-40 a=-40 en=0.05
  animacion se_encienden alpha de=0 a=1 en=2.0 curva=suave
  capa negro_forma "#000000" z=900 x=0 y=0 w=1920 h=1080
  capa_visible negro_forma no

  mostrar sol neutral centro
  mostrar luna neutral centro
  mostrar estrella neutral centro
  esperar 1.2

  ocultar_caja
  animar sol se_acercan
  animar luna se_acercan
  animar estrella se_acercan
  esperar 0.8

  capa_fade negro_forma aparecer 1.0
  animar sol se_apagan
  animar luna se_apagan
  animar estrella se_apagan
  esperar 1.1

  # Ajustamos tamaño y altura durante el negro para que el cambio no se vea.
  animar sol se_alejan
  animar luna se_alejan
  animar estrella se_alejan
  animar sol camara_baja
  animar luna camara_baja
  animar estrella camara_baja
  esperar 1.2

  # Las alphas de distintas recetas se multiplican. Retiramos "se_apagan",
  # que terminó en cero, antes de encenderlos; de otro modo seguirían invisibles.
  detener_animacion sol se_apagan
  detener_animacion luna se_apagan
  detener_animacion estrella se_apagan
  animar sol se_encienden
  animar luna se_encienden
  animar estrella se_encienden
  capa_fade negro_forma desaparecer 2.0
  esperar 2.0

  esperar 2.2

  mostrar sol neutral centro
  sol: "Ah."

  mostrar luna neutral centro
  luna: "Hola otra vez."

  mostrar estrella 03 centro
  estrella: "Ahora te veo un poco."

  mostrar sol plana centro
  mostrar estrella 09 centro
  sol: "Es un cubo."

  mostrar luna 04 centro
  mostrar estrella 010 centro
  luna: "Es un viajero."

  mostrar sol 04 centro
  mostrar estrella 09 centro
  sol: "Es un cubo negro."

  mostrar luna 01 centro
  mostrar estrella 01 centro
  luna: "Eso es del centro."

  mostrar sol 02 centro
  sol: "Del agujero."

  mostrar luna 021 centro
  luna: "No lo llames así delante de—{interrumpir}"

  mostrar estrella 010 centro
  estrella: "¿De quién?"

  mostrar sol 04 centro
  mostrar luna 04 centro
  esperar 2.0

  luna: "De él."

  mostrar estrella 09 centro
  sol: "De nadie."

  mostrar estrella 01 centro
  estrella: "De un cubo."

  mostrar luna 01 centro
  luna: "No."

  mostrar sol 02 centro
  estrella: "Da igual."

  esperar 1.2

  luna: "Cuando camine, empieza la expedición."
  mostrar sol 04 centro
  mostrar estrella 010 centro
  sol: "¿Qué expedición?"
  mostrar luna 04 centro
  luna: "La de mi historia. Tú buscas un lugar."

  # Las capas también suben 40 px para coincidir con el nuevo punto de vista.
  capa cuerpo_trio "assets/demo/sol/Sol_Compuesto/0.png" x=-77 y=104 w=918 h=918 z=30
  capa ojos_09 "assets/demo/sol/Sol_Compuesto/09.png" x=-17 y=104 w=918 h=918 z=40
  ocultar sol [efecto: instantaneo]
  capa_visible cuerpo_trio si
  capa_visible ojos_09 si
  mostrar estrella 09 centro
  sol: "¿Cuál?"
  luna: "Uno al que se pueda llegar."
  mostrar estrella 010 centro
  estrella: "¿Y yo?"

  # El acercamiento para susurrar suma -60 px; la vuelta añade +60 px
  # y compensa ese desplazamiento sin borrar las otras recetas de Luna.
  animacion luna_susurra_va mover_x de=0 a=-60 en=0.9 curva=suave
  animacion luna_susurra_vuelve mover_x de=0 a=60 en=0.9 curva=suave
  ocultar_caja
  animar luna luna_susurra_va
  esperar 1.0
  mostrar estrella 04 centro
  mostrar luna 01 centro
  esperar 1.0
  mostrar luna 04 centro
  esperar 0.5
  mostrar luna 01 centro
  esperar 1.0
  mostrar luna 04 centro
  mostrar estrella 05 centro
  esperar 0.6
  mostrar luna 01 centro
  animar luna luna_susurra_vuelve
  esperar 1.0
  mostrar estrella 03 centro
  mostrar_caja

  estrella: "Ah. Bueno."

  mostrar sol 02 centro [efecto: instantaneo]
  detener_animacion sol
  animar sol sol_trio_queda
  animar sol camara_baja
  capa_visible ojos_09 no
  capa_visible cuerpo_trio no
  mostrar luna 01 centro
  mostrar estrella 01 centro
  sol: "Camina."

  luna: "Hacia nosotros."

  estrella: "Que camine."

  sol: "Nada más. A ver si carga el paso."

  # Paramos la música del VN antes del 2D para que no se reanude al regresar.
  parar_musica

  # "gameplay" abre el mapa y el guion continúa en la línea siguiente al volver.
  # El Lua del paseo escribe "paseo_eleccion"; esa variable elige la ruta de Luna
  # o la de Sol. Ocultamos el reparto y la caja antes de ceder la pantalla.
  ocultar sol [efecto: instantaneo]
  ocultar luna [efecto: instantaneo]
  ocultar estrella [efecto: instantaneo]
  ocultar_caja
  # Para entrar al 2D basta la ruta del mapa. La palabra de en medio
  # (topdown) no elige el modo: lo decide "modo:" dentro del .eclmap. La
  # dejamos porque se lee mejor; sin ella también funciona.
  gameplay topdown "testigo_paseo.eclmap"
  mostrar_caja
  si paseo_eleccion == luna
    ir_a ruta_sol
  fin_si
  si paseo_eleccion == sol
    ir_a ruta_sol
  fin_si
  # Si se abandona el paseo sin elegir ruta, terminamos este guion.
  # "fin" devuelve al menú cuando este juego se lanzó con "nuevo_juego";
  # si ejecutas el guion por sí solo, termina el motor.
  fin


# Aparición común de las copias: sucede al elegir Luna o Sol.
# Conservamos "ruta_sol" como nombre de entrada de esta escena.

escena ruta_sol
  capa mundo "#E9E5DE" z=10 x=0 y=0 w=1920 h=1080
  animacion sol_trio mover_x de=-578 a=-578 en=0.05
  animacion luna_trio mover_x de=530 a=530 en=0.05
  animacion estrella_trio mover_x de=-20 a=-20 en=0.05
  mostrar sol neutral centro [efecto: instantaneo]
  mostrar luna neutral centro [efecto: instantaneo]
  mostrar estrella neutral centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_trio
  animar luna luna_trio
  animar estrella estrella_trio
  esperar 0.6
  mostrar luna 03 centro
  mostrar sol 05 centro
  luna: "Bien. Entonces, el viajero—{interrumpir}"
  mostrar luna 04 centro
  sol: "Espera."
  mostrar estrella 09 centro
  mostrar luna 09 centro
  esperar 1.2
  estrella: "Hay otro."
  ocultar sol [efecto: instantaneo]
  ocultar luna [efecto: instantaneo]
  ocultar estrella [efecto: instantaneo]
  ocultar_caja
  gameplay topdown "testigo_copias.eclmap"
  mostrar_caja
  ir_a sol_despues_copias


# Ruta de Luna: la expedición. "viene_de_sol" distingue la visita directa
# de la que sigue a las pruebas de Sol.

escena ruta_luna
  capa mundo "#E8CDB5" z=10 x=0 y=0 w=1920 h=1080
  animacion sol_trio mover_x de=-578 a=-578 en=0.05
  animacion luna_trio mover_x de=530 a=530 en=0.05
  animacion estrella_trio mover_x de=-20 a=-20 en=0.05
  mostrar sol 04 centro [efecto: instantaneo]
  mostrar luna 04 centro [efecto: instantaneo]
  mostrar estrella 010 centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_trio
  animar luna luna_trio
  animar estrella estrella_trio
  esperar 0.6

  set viene_de_sol = 0
  # La vuelta sin daño ya incluye esas respuestas; no las repetimos.
  mostrar luna 04 centro
  luna: "Creí que bastaba con darle una forma."
  sol: "Bastó para caminar."
  mostrar luna 01 centro
  mostrar sol 02 centro
  mostrar estrella 01 centro
  luna: "Y luego tuvo que defenderse."
  sol: "No se lo pedimos."
  esperar 0.6

  mostrar luna 04 centro
  mostrar sol 04 centro
  luna: "No era lo que había escrito."
  sol: "Tampoco lo que yo quería probar."
  esperar 0.8

  mostrar luna 04 centro
  luna: "¿Y si aparecen más?"
  sol: "No sé."
  esperar 0.4
  mostrar sol 02 centro
  sol: "Pero ahora no viene ninguno."
  mostrar luna 01 centro
  luna: "...Bueno."

  luna: "..."
  luna: "No esperaba que pasara todo eso."
  estrella: "Nadie."
  # Luna ya mira al viajero; el «...» sostiene ese silencio.
  luna: "..."
  luna: "No voy a pedirte que pelees con nada más."
  mostrar sol 04 centro
  sol: "No había peleas en tu historia."
  mostrar luna 04 centro
  luna: "Precisamente."
  esperar 0.4

  mostrar estrella 010 centro
  estrella: "Habías preparado otra cosa."
  mostrar luna 04 centro
  luna: "Sí."
  mostrar luna 01 centro
  luna: "Algo bastante más tranquilo."
  sol: "Podemos hacerlo."
  luna: "..."
  luna: "Sí."
  # Luna conserva la mirada al viajero al retomar los preparativos.
  ir_a luna_partes


# El mapa de Sol y los preparativos de la expedición.
escena luna_partes
  # Estrella cruza como personaje: las capas sólo aceptan tracks XY.
  # Al abrir el mapa, los tres pasan a capas para que quede delante de todos.
  capa mapa_enrollado "assets/demo/Mapa_01.png" x=-77 y=-176 w=918 h=918 z=25
  capa_visible mapa_enrollado no
  capa sol_doble "assets/demo/sol/Sol_04.png" x=-77 y=144 w=918 h=918 z=30
  capa_visible sol_doble no
  capa estrella_mapa "assets/demo/estrella/Estrella_010.png" x=-77 y=150 w=918 h=918 z=20
  capa_visible estrella_mapa no
  capa sol_mapa "assets/demo/sol/Sol_02.png" x=481 y=144 w=918 h=918 z=30
  capa_visible sol_mapa no
  capa luna_mapa "assets/demo/luna/Luna_04.png" x=1031 y=149 w=918 h=918 z=35
  capa_visible luna_mapa no
  capa mapa_abierto "assets/demo/Mapa_02.png" x=493.43 y=204 w=918 h=918 z=60
  capa_visible mapa_abierto no
  animacion mapa_sale mover_y de=320 a=0 en=0.5 curva=resorte
  animacion mapa_guarda mover_y de=0 a=320 en=0.35 curva=entrada
  animacion mapa_sube mover_y de=430 a=0 en=0.6 curva=salida
  animacion mapa_baja mover_y de=0 a=430 en=0.5 curva=entrada
  animacion estrella_mapa_atras escala de=1 a=0.92 en=0.55 curva=suave
  animacion estrella_mapa_atras mover_y de=0 a=-28 en=0.55 curva=suave
  animacion sol_al_medio mover_x de=0 a=558 en=0.85 curva=suave
  animacion estrella_al_lado_sol mover_x de=0 a=-558 en=0.85 curva=suave
  animacion estrella_mapa_adelante escala de=1 a=1.086956522 en=0.55 curva=suave
  animacion estrella_mapa_adelante mover_y de=0 a=28 en=0.55 curva=suave
  animacion sol_mapa_queda mover_x de=-20 a=-20 en=0.05
  animacion estrella_mapa_queda mover_x de=-578 a=-578 en=0.05

  mostrar sol 04 centro
  mostrar estrella 010 centro
  mostrar luna 04 centro
  luna: "¿Recuerdan sus partes?"
  sol: "Encontrar un lugar."
  estrella: "Yo sé la mía."
  mostrar luna 021 centro
  luna: "Estrella."
  estrella: "Sí me acuerdo."
  mostrar luna 01 centro
  luna: "Bien."
  luna: "Entonces podemos empezar."
  # (Luna recupera un poco de entusiasmo.)
  esperar 0.6

  # La visita desde Sol usa otra música. En la visita directa el fondo
  # empieza a cambiar aquí, mientras avanza la conversación.
  si viene_de_sol == 1
    musica "assets/musica/expedicion_atardecer.ogg"
  sino
    musica "assets/musica/expedicion.ogg"
    capa mundo_b "#E2BBA3" z=11 x=0 y=0 w=1920 h=1080
    capa_visible mundo_b no
    capa_fade mundo_b aparecer 3
  fin_si
  mostrar luna 07 centro
  esperar 1.0
  luna: "Las tres almas aguardaban al viajero..."
  mostrar estrella 01 centro
  sol: "Ya llegó."
  mostrar luna 022 centro
  luna: "¡Estoy narrando!"
  mostrar luna 013 centro
  mostrar estrella 010 centro
  sol: "Sigue."
  mostrar luna 01 centro
  luna: "Y cuando finalmente llegó, emprendieron juntos su primera expedición."
  esperar 1.2
  mostrar luna 04 centro
  esperar 1.2
  luna: "Por cierto, Sol."
  mostrar estrella 09 centro 
  sol: "¿Qué?"
  luna: "¿Cómo vas con el lugar?"
  sol: "Ya tengo uno."
  mostrar luna 09 centro
  luna: "¿Cuál elegiste?"
  mostrar luna 04 centro
  sol: "Te lo enseño cuando lleguemos."
  mostrar luna 09 centro
  luna: "¿Y cómo es?"
  mostrar estrella 04 centro
  sol: "Se puede llegar caminando. Hay sitio para todos."
  mostrar luna 01 centro
  luna: "Eso no me dice mucho."
  sol: "No quería contarte el final."
  # (Luna lo mira.)
  mostrar luna 024 centro
  esperar 1.0
  luna: "..."
  mostrar luna 04 centro
  luna: "Eso ha sido muy considerado de tu parte."
  mostrar sol 016 centro
  sol: "De nada."
  mostrar estrella 05 centro
  estrella: "Yo quiero verlo."
  luna: "Yo también."
  mostrar sol 02 centro
  mostrar estrella 01 centro 
  mostrar luna 01 centro
  # (Luna vuelve a mirar al viajero.)
  esperar 1.0

  luna: "Bien."
  luna: "Entonces continuemos."
  esperar 0.8
  mostrar luna 07 centro
  mostrar sol 05 centro
  # El mapa sale mientras corre la frase; la interrupción no pide otro clic.
  animar mapa_enrollado mapa_sale
  capa_visible mapa_enrollado si
  mostrar estrella 010 centro
  luna: "Las tres almas y el viajero partieron hacia—{interrumpir}"
  mostrar sol 02 centro
  mostrar luna 04 centro
  luna: "..."
  mostrar estrella 09 centro
  mostrar luna 09 centro
  luna: "¿Hiciste un mapa?"
  mostrar sol 04 centro
  sol: "Sí."
  mostrar luna 04 centro
  luna: "Te pedí que encontraras un lugar."
  mostrar sol 03 centro
  sol: "Lo encontré."
  mostrar luna 09 centro
  luna: "¿Y todo eso?"
  mostrar sol 04 centro
  sol: "Para llegar."

  # Estrella retrocede, cruza detrás de Sol y vuelve a su altura original.
  # Sol entra al final del orden de personajes para dejar a Estrella detrás.
  mostrar luna 04 centro
  mostrar estrella 01 centro
  ocultar_caja
  cinematica {
    capa_visible sol_doble si
    ocultar sol [efecto: instantaneo]
    esperar 0.05
    mostrar sol 04 centro [efecto: instantaneo]
    detener_animacion sol
    animar sol sol_trio
    esperar 0.05
    capa_visible sol_doble no

    animar estrella estrella_mapa_atras
    esperar 0.65
    mostrar estrella 09 centro
    esperar 0.2
    animar sol sol_al_medio
    animar mapa_enrollado sol_al_medio
    esperar 0.95
    animar estrella estrella_al_lado_sol
    esperar 0.95
    mostrar estrella 010 centro
    animar estrella estrella_mapa_adelante
    esperar 0.65
    detener_animacion estrella
    animar estrella estrella_mapa_queda
    detener_animacion sol
    animar sol sol_mapa_queda
    # Dejamos respirar al grupo ya acomodado antes de cambiar de mapa.
    esperar 0.35
    animar mapa_enrollado mapa_guarda
    esperar 0.4
    capa_visible mapa_enrollado no
    detener_animacion mapa_enrollado

    # Pasamos temporalmente a Sol, Luna y Estrella a capas para que el mapa
    # pueda dibujarse delante de los tres. Hoy los personajes se dibujan
    # después de todas las capas, sin importar su z; el mapa quedaría detrás.
    # Es una solución provisional: poner un objeto delante del reparto debería
    # poder dirigirse sin tener que pasar los personajes a capas.
    capa_visible estrella_mapa si
    capa_visible sol_mapa si
    capa_visible luna_mapa si
    ocultar estrella [efecto: instantaneo]
    ocultar sol [efecto: instantaneo]
    ocultar luna [efecto: instantaneo]
    animar mapa_abierto mapa_sube
    capa_visible mapa_abierto si
    esperar 0.7
  }
  mostrar_caja
  # Luna lo examina de reojo; Sol sostiene el mapa centrado en su posición.
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  luna: "..."
  capa estrella_mapa "assets/demo/estrella/Estrella_018.png"
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Bueno."
  capa luna_mapa "assets/demo/luna/Luna_02.png"
  luna: "Al menos alguien se tomó en serio la expedición."
  capa estrella_mapa "assets/demo/estrella/Estrella_07.png"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Tú la llamaste así."
  capa estrella_mapa "assets/demo/estrella/Estrella_0003.png"
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  capa luna_mapa "assets/demo/luna/Luna_025.png"
  luna: "¡Y mantengo el nombre!"
  # Estrella ya lleva bufanda desde el remate de Luna.
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  esperar 1.5

  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  capa sol_mapa "assets/demo/sol/Sol_017.png"
  luna: "¿Para qué la bufanda?"
  capa estrella_mapa "assets/demo/estrella/Estrella_0009.png"
  estrella: "Por si hace frío."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Puedes calentarte sola."
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  estrella: "Sí."
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "¿Entonces?"
  capa estrella_mapa "assets/demo/estrella/Estrella_0005.png"
  estrella: "Por si no quiero."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  esperar 1.0
  luna: "..."
  capa sol_mapa "assets/demo/sol/Sol_021.png"
  luna: "Bien."
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  luna: "Tenemos un mapa,"
  capa sol_mapa "assets/demo/sol/Sol_022.png"
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "una bufanda..."
  capa luna_mapa "assets/demo/luna/Luna_02.png"
  luna: "¡Ahora sí parece una expedición!"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Podemos ir."
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Un momento."
  capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  # Luna recupera la solemnidad; Sol sólo está intentando guiar al grupo.
  esperar 1.8
  capa luna_mapa "assets/demo/luna/Luna_07.png"
  esperar 1.0
  luna: "Y así, las tres almas y el viajero emprendieron su primera expedición."
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  capa sol_mapa "assets/demo/sol/Sol_019.png"
  luna: "Guiados por un mapa cuyos caminos prometían conducirlos hacia lo desconocido...{interrumpir}"
  sol: "Por aquí."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_0003.png"
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_07.png"
  luna: "Hacia lo desconocido."
  capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  capa luna_mapa "assets/demo/luna/Luna_014.png"
  sol: "Sí."
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  capa luna_mapa "assets/demo/luna/Luna_021.png"
  luna: "¡Entonces déjame narrarlo!"
  capa luna_mapa "assets/demo/luna/Luna_013.png"
  esperar 0.35
  capa luna_mapa "assets/demo/luna/Luna_07.png"
  luna: "No sabían cuánto duraría.{interrumpir}"
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  sol: "Poco."
  capa luna_mapa "assets/demo/luna/Luna_021.png"
  esperar 0.4
  luna: "Sol."
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "¿Qué?"
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "Déjame exagerar la distancia."
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  capa sol_mapa "assets/demo/sol/Sol_017.png"
  estrella: "Podemos caminar lento."
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_0005.png"
  luna: "Gracias, Estrella."
  capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  capa sol_mapa "assets/demo/sol/Sol_022.png"
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  sol: "No hace falta."
  capa luna_mapa "assets/demo/luna/Luna_022.png"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  luna: "¡Es una novela!"
  # Después del remate vuelve a narrar; no arrastra el enfado al camino.
  capa luna_mapa "assets/demo/luna/Luna_07.png"

  # "viene_de_sol" vale 1 en "luna_retoma" y 0 en "ruta_luna";
  # solo la visita desde Sol pasa por el trayecto adicional.
  si viene_de_sol == 1
    ir_a trayecto_sol_luna
  fin_si
  ir_a luna_camino


# El camino compartido: ambas rutas continúan desde aquí.

escena luna_camino
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  luna: "Y así, siguiendo los caminos señalados en el mapa, avanzaron hacia su primer destino."
  capa sol_mapa "assets/demo/sol/Sol_021.png"
  capa luna_mapa "assets/demo/luna/Luna_027.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_00012.png"
  luna: "Sin saber qué encontrarían al final..."
  esperar 1.0
  capa sol_mapa "assets/demo/sol/Sol_019.png"
  esperar 0.8
  sol: "Ya casi."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "¿Ya casi qué?"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Llegamos."
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "¿Tan rápido?"
  capa sol_mapa "assets/demo/sol/Sol_019.png"
  sol: "Sí."
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Bueno."
  capa sol_mapa "assets/demo/sol/Sol_021.png"
  capa luna_mapa "assets/demo/luna/Luna_014.png"
  luna: "En la novela tardaba más."
  ir_a luna_llegada


# La llegada: las dos rutas se reúnen antes de entrar al lugar en 2D.

escena luna_llegada
  capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Podemos esperar un poco antes de llegar."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "No, tampoco tanto."
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  estrella: "Entonces ya está bien."
  capa sol_mapa "assets/demo/sol/Sol_019.png"
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  esperar 1.1
  luna: "..."
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Debo admitir que fue una expedición sorprendentemente bien organizada."
  capa sol_mapa "assets/demo/sol/Sol_016.png"
  sol: "Gracias."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_021.png"
  luna: "Para lo poco que caminamos."
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  sol: "También marqué la vuelta."
  esperar 0.8
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Bien."
  capa estrella_mapa "assets/demo/estrella/Estrella_0006.png"
  capa luna_mapa "assets/demo/luna/Luna_07.png"
  luna: "El viajero siguió a las tres almas."
  luna: "Y al final del recorrido encontraron—{interrumpir}"
  capa sol_mapa "assets/demo/sol/Sol_019.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_00012.png"
  sol: "Aquí."
  capa luna_mapa "assets/demo/luna/Luna_013.png"
  esperar 1.2
  luna: "..."
  luna: "Qué manera más seca de arruinar una revelación."
  capa luna_mapa "assets/demo/luna/Luna_028.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_0005.png"
  sol: "Llegamos."
  # Sol guarda el mapa al terminar el trayecto y sus comentarios.
  ocultar_caja
  cinematica {
    animar mapa_abierto mapa_baja
    esperar 0.6
    capa_visible mapa_abierto no
    detener_animacion mapa_abierto
    mostrar sol 019 centro [efecto: instantaneo]
    detener_animacion sol
    animar sol sol_mapa_queda
    mostrar estrella 0005 centro [efecto: instantaneo]
    detener_animacion estrella
    animar estrella estrella_mapa_queda
    mostrar luna 04 centro [efecto: instantaneo]
    detener_animacion luna
    animar luna luna_trio
    capa_visible estrella_mapa no
    capa_visible sol_mapa no
    capa_visible luna_mapa no
  }
  mostrar_caja
  estrella: "Ya quiero verlo."
  mostrar luna 029 centro
  luna: "Yo también."

  # Terminamos la música del VN: el mapa 2D tiene su propia pista.
  parar_musica
  ocultar sol [efecto: instantaneo]
  ocultar luna [efecto: instantaneo]
  ocultar estrella [efecto: instantaneo]
  # Retiramos las capas de transición; al volver prepararemos otro fondo.
  capa_visible mundo_b no
  capa_visible mundo_c no
  ocultar_caja
  gameplay topdown "testigo_lugar.eclmap"
  mostrar_caja
  ir_a lugar_vuelta


# Trayecto adicional de Sol a Luna: al terminar vuelve a "luna_camino".

escena trayecto_sol_luna
  capa_fade mundo_c aparecer 6
  capa sol_mapa "assets/demo/sol/Sol_023.png"
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  luna: "..."
  luna: "Sol, ¿me prestas el mapa?"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Sí."
  # Sol acerca el mapa; sólo cambia de posición, sin ocultarlo ni cambiar su tamaño.
  # La entrega termina aun con skip activo; Luna baja la mirada cuando lo recibe.
  ocultar_caja
  cinematica {
    capa_mover mapa_abierto a (1042.47,209) en 0.85 suave
    esperar 0.9
    capa luna_mapa "assets/demo/luna/Luna_026.png"
    capa sol_mapa "assets/demo/sol/Sol_022.png"
  }
  mostrar_caja
  luna: "..."
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  esperar 1.0
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  esperar 1.9
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  luna: "Dibujaste bastante."
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Había que llegar."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Sí, pero también marcaste otros caminos."
  sol: "Por si queríamos mirar un poco más."
  capa estrella_mapa "assets/demo/estrella/Estrella_0005.png"
  estrella: "Yo quiero."
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  luna: "¿Mirar?"
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  estrella: "Sí."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Bueno, primero vamos a conocer el lugar."
  estrella: "Y después miramos."
  capa sol_mapa "assets/demo/sol/Sol_021.png"
  sol: "Podemos."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Sí."

  # Luna consulta el mapa y vuelve a Sol al preguntar por el lugar.
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  capa sol_mapa "assets/demo/sol/Sol_022.png"
  luna: "..."
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Por cierto, ¿cómo lo encontraste?"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Caminando."
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "¿Y por qué elegiste ese?"
  sol: "Se llega fácil."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "..."
  capa sol_mapa "assets/demo/sol/Sol_016.png"
  sol: "Y me gustó."
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "¿Te gustó?"
  sol: "Sí."
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Eso no me lo habías dicho."
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Quería que lo vieran."
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Bueno."
  capa luna_mapa "assets/demo/luna/Luna_025.png"
  capa sol_mapa "assets/demo/sol/Sol_016.png"
  luna: "Me alegra."

  estrella: "¿Tiene nombre?"
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  capa sol_mapa "assets/demo/sol/Sol_021.png"
  luna: "¿Qué cosa?"
  estrella: "El lugar."
  sol: "No le puse."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "¿No?"
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "No."
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  luna: "..."
  luna: "En mis notas tampoco tiene."
  capa estrella_mapa "assets/demo/estrella/Estrella_0002.png"
  estrella: "Entonces está bien."
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  luna: "¿No quieres ponerle uno?"
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  estrella: "Cuando lo vea."
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Sí."
  luna: "Primero vamos a conocerlo."
  capa estrella_mapa "assets/demo/estrella/Estrella_0005.png"
  estrella: "Y después le ponemos uno."
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  luna: "Si encontramos alguno que nos guste."
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Podemos pensar varios."
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "¿Tú también?"
  sol: "Sí."
  capa estrella_mapa "assets/demo/estrella/Estrella_00015.png"
  estrella: "Yo voy a pensar más."
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  luna: "¿Más que Sol?"
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  estrella: "No sé cuántos va a pensar."
  capa sol_mapa "assets/demo/sol/Sol_021.png"
  sol: "Yo tampoco."
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Bueno, tenemos tiempo."

  # El mapa se queda con Luna durante el intercambio; ella alterna lectura y charla.
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  capa estrella_mapa "assets/demo/estrella/Estrella_00012.png"
  estrella: "¿Y si encontramos otra cosa?"
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  luna: "¿En el camino?"
  capa estrella_mapa "assets/demo/estrella/Estrella_0001.png"
  estrella: "O cuando lleguemos."
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "..."
  luna: "¿Qué cosa?"
  capa estrella_mapa "assets/demo/estrella/Estrella_0002.png"
  estrella: "No sé."
  estrella: "Todavía no la encontramos."
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_01.png"
  luna: "Bueno."
  capa luna_mapa "assets/demo/luna/Luna_03.png"
  luna: "Si encuentras algo, me lo enseñas."
  capa estrella_mapa "assets/demo/estrella/Estrella_0005.png"
  estrella: "Sí."
  capa sol_mapa "assets/demo/sol/Sol_04.png"
  sol: "Yo marqué por dónde volver."
  capa luna_mapa "assets/demo/luna/Luna_09.png"
  luna: "¿También?"
  sol: "Sí."
  capa luna_mapa "assets/demo/luna/Luna_026.png"
  luna: "..."
  capa luna_mapa "assets/demo/luna/Luna_025.png"
  luna: "Entonces podremos mirar un rato."
  estrella: "Bien."
  # Luna devuelve el mapa al mismo punto de Sol antes del camino compartido.
  ocultar_caja
  cinematica {
    capa_mover mapa_abierto a (493.43,204) en 0.85 suave
    esperar 0.9
    capa sol_mapa "assets/demo/sol/Sol_023.png"
    capa estrella_mapa "assets/demo/estrella/Estrella_00011.png"
  }
  mostrar_caja
  capa luna_mapa "assets/demo/luna/Luna_04.png"
  luna: "Gracias."
  capa sol_mapa "assets/demo/sol/Sol_016.png"
  sol: "De nada."
  ir_a luna_camino


# Vuelta del lugar: anochece mientras el grupo conversa.


escena lugar_vuelta
  # Los fundidos de cuatro capas llevan el cielo hacia el color de la ilustración
  # final. Cada ruta empieza a una hora distinta, así que usa colores distintos.
  # Los fundidos no bloquean el diálogo: avanzan mientras el jugador lee.
  # Cada paso tarda diez segundos para que el anochecer respire.
  si viene_de_sol == 1
    capa mundo "#8A7790" z=10 x=0 y=0 w=1920 h=1080
    capa paso_1 "#74667F" z=11 x=0 y=0 w=1920 h=1080
    capa paso_2 "#5E566E" z=12 x=0 y=0 w=1920 h=1080
    capa paso_3 "#3A3851" z=13 x=0 y=0 w=1920 h=1080
    capa paso_4 "#31344D" z=14 x=0 y=0 w=1920 h=1080
  sino
    capa mundo "#D3A597" z=10 x=0 y=0 w=1920 h=1080
    capa paso_1 "#BB948C" z=11 x=0 y=0 w=1920 h=1080
    capa paso_2 "#A28381" z=12 x=0 y=0 w=1920 h=1080
    capa paso_3 "#8A7276" z=13 x=0 y=0 w=1920 h=1080
    capa paso_4 "#72616B" z=14 x=0 y=0 w=1920 h=1080
  fin_si
  capa_visible paso_1 no
  capa_visible paso_2 no
  capa_visible paso_3 no
  capa_visible paso_4 no
  # Conservamos el orden del mapa: Estrella izquierda, Sol centro, Luna derecha.
  animacion sol_trio mover_x de=-20 a=-20 en=0.05
  animacion luna_trio mover_x de=530 a=530 en=0.05
  animacion estrella_trio mover_x de=-578 a=-578 en=0.05
  mostrar sol 04 centro [efecto: instantaneo]
  mostrar luna 04 centro [efecto: instantaneo]
  mostrar estrella 0003 centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_trio
  animar luna luna_trio
  animar estrella estrella_trio
  esperar 0.6

  # La música del final combina la visita previa a Sol ("viene_de_sol")
  # con el resultado del combate: queda 1 HP ("copias_hp") o hubo golpes
  # ("copias_golpes"). Esas dos señales se reúnen en "luna_dano".
  set luna_dano = 0
  si copias_hp == 1
    set luna_dano = 1
  fin_si
  si copias_golpes > 0
    set luna_dano = 1
  fin_si
  si viene_de_sol == 1
    si luna_dano == 1
      musica "assets/musica/final_luna_con_sol_y_dano.ogg"
    sino
      musica "assets/musica/final_luna_con_sol.ogg"
    fin_si
  sino
    si luna_dano == 1
      musica "assets/musica/final_luna_con_dano.ogg"
    sino
      musica "assets/musica/final_luna.ogg"
    fin_si
  fin_si
  sol: "Ya estamos todos."
  luna: "Sí."
  sol: "¿Terminamos de mirar?"
  mostrar estrella 0002 centro
  estrella: "Yo no."
  # Estrella devuelve la mirada a Luna durante la pausa.
  mostrar estrella 0001 centro
  esperar 1.0
  estrella: "Pero podemos."
  capa_fade paso_1 aparecer 10
  luna: "Bueno."
  mostrar luna 025 centro
  luna: "Debo admitir que elegiste un buen lugar, Sol."
  mostrar sol 016 centro
  sol: "Gracias."
  mostrar luna 04 centro
  luna: "No era exactamente como lo imaginaba."
  mostrar sol 04 centro
  mostrar estrella 0003 centro
  sol: "No me dijiste cómo lo imaginabas."
  mostrar luna 03 centro
  luna: "¡Porque se suponía que lo encontraras tú!"
  mostrar sol 016 centro
  sol: "Entonces salió bien."
  mostrar luna 04 centro
  luna: "..."
  luna: "Sí."
  mostrar sol 022 centro
  mostrar luna 025 centro
  luna: "Salió bien."
  capa_fade paso_2 aparecer 10
  esperar 0.8
  mostrar luna 04 centro
  mostrar estrella 0001 centro
  luna: "¿Y tú, Estrella?"
  mostrar sol 05 centro
  estrella: "Me gusta aquí."
  mostrar luna 09 centro
  luna: "¿Eso es todo?"
  estrella: "No."
  luna: "¿Ah, no?"
  mostrar sol 02 centro
  mostrar estrella 0005 centro
  estrella: "Quiero volver."
  # Luna deja asentarse la respuesta de Estrella.
  mostrar luna 04 centro
  esperar 1.0
  luna: "..."
  mostrar luna 025 centro
  luna: "Eso me gusta más."
  mostrar estrella 00011 centro
  mostrar sol 05 centro
  sol: "Marqué la vuelta."
  mostrar sol 04 centro
  mostrar luna 04 centro
  luna: "¡Ya sé que marcaste la vuelta!"
  mostrar estrella 0001 centro
  estrella: "Entonces podemos volver."
  mostrar luna 01 centro
  luna: "..."
  mostrar luna 04 centro
  luna: "Sí."
  esperar 0.8
  mostrar luna 01 centro
  luna: "Bueno."
  capa_fade paso_3 aparecer 10
  luna: "Creo que ya podemos terminar la historia."
  mostrar sol 04 centro
  sol: "¿Aquí?"
  mostrar luna 04 centro
  luna: "Aquí."
  sol: "Bien."
  # El grupo escucha; Luna toma aire mirando al viajero.
  mostrar sol 04 centro
  mostrar luna 01 centro
  esperar 1.0
  mostrar luna 07 centro
  esperar 0.8
  luna: "Las tres almas aguardaban al viajero."
  mostrar luna 028 centro
  luna: "Habían imaginado su llegada de muchas maneras."
  mostrar luna 01 centro
  luna: "Pero cuando finalmente apareció, descubrieron que ni siquiera podía dar el primer paso."
  luna: "Así que le dieron una forma."
  mostrar luna 02 centro
  mostrar sol 02 centro
  mostrar estrella 0003 centro
  luna: "Y el viajero caminó."
  mostrar luna 04 centro
  luna: "Conoció a las tres almas, y juntos emprendieron su primera expedición."
  mostrar luna 07 centro
  luna: "Recorrieron caminos que ninguno había recorrido de la misma manera."
  mostrar luna 04 centro
  luna: "Y llegaron a un lugar que todavía no tenía nombre."
  esperar 0.6
  mostrar luna 01 centro
  luna: "No era el lugar más grande."
  luna: "Ni el más lejano."
  mostrar luna 025 centro
  mostrar estrella 0005 centro
  luna: "Pero habían llegado juntos."
  # La mirada al grupo sostiene esta admisión sin otra caja de puntos suspensivos.
  esperar 0.7
  luna: "Y eso era suficiente para su primera expedición."
  esperar 1.4
  capa_fade paso_4 aparecer 10
  mostrar sol 04 centro
  esperar 0.8
  mostrar luna 04 centro
  # Una sola espera: Sol y Estrella siguen pendientes de Luna.
  esperar 1.0
  mostrar luna 09 centro
  mostrar estrella 0001 centro
  luna: "¿Qué?"
  sol: "Nada."
  estrella: "Sigue."
  mostrar luna 01 centro
  luna: "..."
  mostrar luna 09 centro
  luna: "¿Quieren que siga?"
  sol: "Todavía no termina."
  mostrar luna 04 centro
  luna: "¿Qué falta?"
  mostrar sol 019 centro
  sol: "Hay un camino más adelante."
  mostrar sol 05 centro
  mostrar estrella 0003 centro
  estrella: "Y podemos quedarnos."
  mostrar sol 04 centro
  mostrar luna 01 centro
  esperar 0.4
  mostrar luna 07 centro
  mostrar estrella 0001 centro
  luna: "Las tres almas habían alcanzado su primer destino..."
  luna: "Pero todavía quedaba una decisión."
  mostrar sol 02 centro
  mostrar luna 01 centro
  esperar 0.4
  mostrar estrella 0003 centro
  luna: "¿Qué hacemos?"
  elegir:
    "Quedarnos un rato" -> quedarnos
    "Seguir un poco más" -> seguir


# Finales de Luna: variantes del mismo plano del grupo.
# La capa de anochecer comparte el color de su cielo para suavizar la transición.

# Final de Luna: quedarse.
escena quedarnos
  # Retomamos el último color del fondo y retiramos las capas intermedias.
  si viene_de_sol == 1
    capa mundo "#31344D" z=10 x=0 y=0 w=1920 h=1080
  sino
    capa mundo "#72616B" z=10 x=0 y=0 w=1920 h=1080
  fin_si
  capa_visible paso_1 no
  capa_visible paso_2 no
  capa_visible paso_3 no
  capa_visible paso_4 no
  capa tarde "#31344D" z=12 x=0 y=0 w=1920 h=1080
  # Un solo plano, con posición y tamaño fijos. fondo_a conserva la imagen
  # actual; fondo_b está encima y sólo se usa mientras entra la siguiente.
  capa fondo_a "assets/demo/Cgs/fondo apagados.png" z=80 x=0 y=0 w=1920 h=1080
  capa fondo_b "assets/demo/Cgs/fondo apagados.png" z=81 x=0 y=0 w=1920 h=1080
  capa_visible tarde no
  capa_visible fondo_a no
  capa_visible fondo_b no
  # Conservamos el orden del mapa: Estrella izquierda, Sol centro, Luna derecha.
  animacion sol_trio mover_x de=-20 a=-20 en=0.05
  animacion luna_trio mover_x de=530 a=530 en=0.05
  animacion estrella_trio mover_x de=-578 a=-578 en=0.05
  mostrar sol 04 centro [efecto: instantaneo]
  mostrar luna 01 centro [efecto: instantaneo]
  mostrar estrella 0003 centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_trio
  animar luna luna_trio
  animar estrella estrella_trio

  # Luna responde al viajero; la elección ya dio espacio para pensar.
  esperar 0.3
  luna: "Está bien."
  sol: "Bien."
  mostrar estrella 0001 centro
  estrella: "Sí."
  mostrar luna 04 centro
  luna: "Entonces nos quedamos un rato."
  # La noche alcanza el color de la ilustración durante esta conversación.
  capa_fade tarde aparecer 8
  esperar 0.8
  mostrar luna 025 centro
  luna: "Me alegra."
  sol: "¿Qué cosa?"
  mostrar luna 04 centro
  luna: "Haber venido."
  mostrar sol 016 centro
  luna: "Haber llegado hasta aquí."
  mostrar estrella 0005 centro
  estrella: "A mí también."
  esperar 0.5
  mostrar luna 04 centro
  luna: "No era exactamente como lo imaginaba."
  mostrar sol 04 centro
  sol: "No."
  mostrar luna 029 centro
  luna: "Creo que mejor."
  estrella: "Sí."
  esperar 0.7
  # Estrella retoma los nombres del trayecto; Luna escucha con curiosidad tranquila.
  # El remate plano de Estrella se apoya en el diálogo, sin otra espera.
  si viene_de_sol == 1
    mostrar estrella 0001 centro [efecto: instantaneo]
    mostrar luna 04 centro [efecto: instantaneo]
    mostrar sol 021 centro [efecto: instantaneo]
    estrella: "Todavía no le pusimos nombre."
    mostrar luna 03 centro [efecto: instantaneo]
    luna: "Es verdad."
    mostrar estrella 00015 centro [efecto: instantaneo]
    estrella: "Ya pensé varios."
    mostrar luna 04 centro [efecto: instantaneo]
    luna: "¿Ah, sí?"
    mostrar estrella 0005 centro [efecto: instantaneo]
    estrella: "Sí."
    mostrar luna 03 centro [efecto: instantaneo]
    luna: "¿Cuál te gusta?"
    mostrar estrella 0001 centro [efecto: instantaneo]
    estrella: "Todavía no sé."
    mostrar sol 05 centro [efecto: instantaneo]
    sol: "No hace falta elegir hoy."
    mostrar luna 04 centro [efecto: instantaneo]
    luna: "No."
    mostrar sol 016 centro [efecto: instantaneo]
    mostrar luna 025 centro [efecto: instantaneo]
    luna: "Tenemos tiempo."
    # La charla termina en el mismo encuadre antes de abrir el plano ilustrado.
    mostrar sol 02 centro [efecto: instantaneo]
    mostrar estrella 0002 centro [efecto: instantaneo]
    mostrar luna 01 centro [efecto: instantaneo]
  fin_si

  # Abrimos el plano en la admisión de Luna; los sprites ceden al grupo dibujado.
  ocultar_caja
  animacion vuelta_cede_plano alpha de=1 a=0 en=4 curva=suave
  cinematica {
    animar sol vuelta_cede_plano
    animar luna vuelta_cede_plano
    animar estrella vuelta_cede_plano
    capa_fade fondo_a aparecer 4
    esperar 4
    ocultar sol [efecto: instantaneo]
    ocultar luna [efecto: instantaneo]
    ocultar estrella [efecto: instantaneo]
    detener_animacion sol
    detener_animacion luna
    detener_animacion estrella
  }
  mostrar_caja

  luna: "Supongo que ya puedo terminarla."
  sol: "Sí."
  estrella: "Termínala."
  esperar 1.6
  luna: "Las tres almas aguardaban al viajero."
  luna: "Habían imaginado su llegada de muchas maneras."
  luna: "Pero cuando finalmente apareció, descubrieron que ni siquiera podía dar el primer paso."
  luna: "Así que le dieron una forma."
  luna: "Y el viajero caminó."
  luna: "Conoció a las tres almas, y juntos emprendieron su primera expedición."
  luna: "Recorrieron un camino pequeño."
  luna: "Encontraron un lugar que todavía no tenía nombre."
  luna: "No era el más grande."
  luna: "Ni el más lejano."
  luna: "Pero habían llegado juntos."
  esperar 1.2
  luna: "Y eso bastó para su primera expedición."
  # Las variantes comparten el mismo encuadre: sólo cambian luces y miradas.
  # Cada bloque carga la siguiente en fondo_b, la funde sobre fondo_a opaca
  # y, cuando terminó, actualiza fondo_a debajo y esconde fondo_b.
  # Al cambiar una ilustración, usar la misma ruta en las DOS líneas capa.
  # ¿Por qué no fundir las dos a la vez? Porque a la mitad del fundido las dos
  # están a medias, se ve lo de abajo a través, y el dibujo se oscurece. Con la
  # de abajo opaca hasta el final, el encuadre nunca pierde luz.
  # El encendido merece verse completo, sin una caja vacía para prolongarlo.
  ocultar_caja
  # 2. Se enciende Estrella; Luna sigue mirando al frente.
  cinematica {
    capa fondo_b "assets/demo/Cgs/Luna sigue_.png"
    sonido "assets/sonidos/estrella_encendido.wav"
    capa_fade fondo_b aparecer 2
    esperar 2
    capa fondo_a "assets/demo/Cgs/Luna sigue_.png"
    capa_visible fondo_b no
  }
  esperar 1.3
  # 3. Luna mira a Estrella.
  cinematica {
    capa fondo_b "assets/demo/Cgs/Luna mira a Estrella encenderse.png"
    capa_fade fondo_b aparecer 0.6
    esperar 0.6
    capa fondo_a "assets/demo/Cgs/Luna mira a Estrella encenderse.png"
    capa_visible fondo_b no
  }
  esperar 1.3
  # 4. Sol mira a Luna antes de animarla a seguir.
  cinematica {
    capa fondo_b "assets/demo/Cgs/sol_dice_continua.png"
    capa_fade fondo_b aparecer 0.6
    esperar 0.6
    capa fondo_a "assets/demo/Cgs/sol_dice_continua.png"
    capa_visible fondo_b no
  }
  esperar 1.3
  mostrar_caja
  sol: "Sigue."
  esperar 0.6
  cinematica {
    capa fondo_b "assets/demo/Cgs/Sol mira a Luna y Luna a el no.png"
    capa_fade fondo_b aparecer 0.6
    esperar 0.6
    capa fondo_a "assets/demo/Cgs/Sol mira a Luna y Luna a el no.png"
    capa_visible fondo_b no
  }
  esperar 1.9
  cinematica {
    capa fondo_b "assets/demo/Cgs/Luna sigue_.png"
    capa_fade fondo_b aparecer 0.6
    esperar 0.6
    capa fondo_a "assets/demo/Cgs/Luna sigue_.png"
    capa_visible fondo_b no
  }
  esperar 1.2
  # 5. Luna vuelve la mirada al frente para seguir el cuento.
  luna: "Las tres almas habían alcanzado su primer destino."
  luna: "Y cuando llegó el momento de decidir adónde ir después..."
  ocultar_caja
  # 6. Se enciende Sol, que vuelve la mirada al frente.
  cinematica {
    capa fondo_b "assets/demo/Cgs/Sol se enciende.png"
    sonido "assets/sonidos/sol_encendido.wav"
    capa_fade fondo_b aparecer 2
    esperar 2
    capa fondo_a "assets/demo/Cgs/Sol se enciende.png"
    capa_visible fondo_b no
  }
  mostrar_caja
  luna: "Decidieron quedarse un rato más."
  esperar 1.2
  ocultar_caja
  # Este es el último plano: lo dejamos respirar con la caja oculta.
  # El negro entra sobre la misma imagen, sin cambiarla por otra ilustración.
  capa negro_luna_cierre "#000000" z=900 x=0 y=0 w=1920 h=1080
  capa_visible negro_luna_cierre no
  cinematica {
    esperar 3
    capa_fade negro_luna_cierre aparecer 3
    esperar 3
    parar_musica
    esperar 1
  }
  fin
# Nota personal: mejorar esto. Por favor.

# Final de Luna: seguir.
escena seguir
  # Retomamos el último color del fondo y retiramos las capas intermedias.
  si viene_de_sol == 1
    capa mundo "#31344D" z=10 x=0 y=0 w=1920 h=1080
  sino
    capa mundo "#72616B" z=10 x=0 y=0 w=1920 h=1080
  fin_si
  capa_visible paso_1 no
  capa_visible paso_2 no
  capa_visible paso_3 no
  capa_visible paso_4 no
  capa tarde "#31344D" z=12 x=0 y=0 w=1920 h=1080
  capa_visible tarde no
  # Conservamos el orden del mapa: Estrella izquierda, Sol centro, Luna derecha.
  animacion sol_trio mover_x de=-20 a=-20 en=0.05
  animacion luna_trio mover_x de=530 a=530 en=0.05
  animacion estrella_trio mover_x de=-578 a=-578 en=0.05
  mostrar sol 019 centro [efecto: instantaneo]
  mostrar luna 01 centro [efecto: instantaneo]
  mostrar estrella 0003 centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_trio
  animar luna luna_trio
  animar estrella estrella_trio

  # Luna responde a la elección mirando al viajero; Sol ya ve el camino.
  sol: "Hay camino."
  mostrar estrella 0005 centro
  estrella: "Yo también quiero."
  mostrar luna 04 centro
  luna: "Bueno."
  mostrar estrella 0001 centro
  mostrar luna 025 centro
  luna: "Entonces sigamos."

  # Ha pasado un momento de camino: negro breve antes de retomar la charla.
  # La capa negra va detrás del reparto; fundimos también a los tres personajes.
  animacion seguir_paso_apaga alpha de=1 a=0 en=1 curva=suave
  animacion seguir_paso_vuelve alpha de=0 a=1 en=1 curva=suave
  capa negro_seguir_paso "#000000" z=900 x=0 y=0 w=1920 h=1080
  capa_visible negro_seguir_paso no
  ocultar_caja
  cinematica {
    capa_fade negro_seguir_paso aparecer 1
    animar sol seguir_paso_apaga
    animar luna seguir_paso_apaga
    animar estrella seguir_paso_apaga
    esperar 1
    esperar 2.5

    # Preparamos las caras durante el negro y conservamos sus posiciones.
    mostrar luna 04 centro [efecto: instantaneo]
    mostrar sol 04 centro [efecto: instantaneo]
    mostrar estrella 0002 centro [efecto: instantaneo]
    detener_animacion sol seguir_paso_apaga
    detener_animacion luna seguir_paso_apaga
    detener_animacion estrella seguir_paso_apaga
    animar sol seguir_paso_vuelve
    animar luna seguir_paso_vuelve
    animar estrella seguir_paso_vuelve
    capa_fade negro_seguir_paso desaparecer 1
    esperar 1
    detener_animacion sol seguir_paso_vuelve
    detener_animacion luna seguir_paso_vuelve
    detener_animacion estrella seguir_paso_vuelve
  }

  # La continuación se cuenta durante la charla; el cielo cambia sin detenerla.
  capa_fade tarde aparecer 10
  mostrar_caja
  luna: "¿Hasta dónde llega tu mapa?"
  mostrar estrella 0001 centro
  sol: "Hasta aquí."
  mostrar luna 09 centro
  luna: "¿Y después?"
  mostrar sol 019 centro
  sol: "No he ido."
  luna: "¿Entonces tampoco sabes qué hay?"
  mostrar sol 04 centro
  sol: "No."
  mostrar estrella 00012 centro
  estrella: "Podemos mirar."
  mostrar luna 025 centro
  luna: "Sí."
  mostrar estrella 0001 centro
  mostrar luna 03 centro
  luna: "Para eso vinimos, ¿no?"
  mostrar sol 019
  mostrar estrella 00012 centro
  mostrar luna 028 centro
  esperar 4.2
  mostrar luna 04 centro
  esperar 2.1
  luna: "¿Eso es todo?"
  mostrar sol 04 centro
  sol: "Por ahora."
  mostrar estrella 0005 centro
  estrella: "Se ve bien desde aquí."
  mostrar luna 01 centro
  luna: "No se ve nada."
  mostrar estrella 0003 centro
  estrella: "Por eso."
  mostrar luna 028 centro
  luna: "Pensé que habría algo más."
  mostrar sol 019 centro
  sol: "Puede haberlo."
  mostrar luna 04 centro
  luna: "¿Dónde?"
  mostrar sol 04 centro
  sol: "Después."
  estrella: "O no."
  mostrar luna 013 centro
  luna: "Eso no ayuda."
  mostrar estrella 0001 centro
  estrella: "No intentaba ayudar."

  # Desde Sol, Estrella pide dejar una marca antes de pensar en el siguiente camino.
  si viene_de_sol == 1
    mostrar estrella 0001 centro [efecto: instantaneo]
    estrella: "Sol."
    mostrar sol 05 centro [efecto: instantaneo]
    sol: "¿Qué?"
    estrella: "¿Puedes marcar este lugar?"
    mostrar luna 04 centro [efecto: instantaneo]
    sol: "Sí."

    # El mapa exige pasar temporalmente el reparto a capas: los personajes
    # nativos se dibujan después de ellas, aunque subamos su z.
    # Copiamos las caras y posiciones actuales para que el cambio no se note.
    capa seguir_sol_mapa "assets/demo/sol/Sol_05.png" x=481 y=144 w=918 h=918 z=30
    capa seguir_estrella_mapa "assets/demo/estrella/Estrella_0001.png" x=-77 y=150 w=918 h=918 z=20
    capa seguir_luna_mapa "assets/demo/luna/Luna_04.png" x=1031 y=149 w=918 h=918 z=35
    capa seguir_mapa "assets/demo/Mapa_02.png" x=493.43 y=204 w=918 h=918 z=60
    capa_visible seguir_mapa no
    animacion seguir_mapa_sube mover_y de=430 a=0 en=0.6 curva=salida
    animacion seguir_mapa_baja mover_y de=0 a=430 en=0.5 curva=entrada
    ocultar_caja
    cinematica {
      ocultar sol [efecto: instantaneo]
      ocultar luna [efecto: instantaneo]
      ocultar estrella [efecto: instantaneo]
      animar seguir_mapa seguir_mapa_sube
      capa_visible seguir_mapa si
      esperar 0.6
      # Estrella y Luna examinan el mapa cuando ya está a la altura de Sol.
      capa seguir_estrella_mapa "assets/demo/estrella/Estrella_00011.png"
      capa seguir_luna_mapa "assets/demo/luna/Luna_026.png"
    }
    mostrar_caja
    capa seguir_sol_mapa "assets/demo/sol/Sol_04.png"
    luna: "¿Todavía tienes sitio?"
    # Sol vuelve al dibujo al responder; la propia línea le da tiempo para marcar.
    capa seguir_sol_mapa "assets/demo/sol/Sol_023.png"
    sol: "Un poco."
    capa seguir_estrella_mapa "assets/demo/estrella/Estrella_00015.png"
    # Luna deja de leer y mira el camino; conserva esta cara al volver a personaje.
    capa seguir_luna_mapa "assets/demo/luna/Luna_028.png"
    estrella: "Bien."
    ocultar_caja
    cinematica {
      animar seguir_mapa seguir_mapa_baja
      esperar 0.5
      capa_visible seguir_mapa no
      detener_animacion seguir_mapa
      mostrar sol 019 centro [efecto: instantaneo]
      mostrar luna 028 centro [efecto: instantaneo]
      mostrar estrella 00012 centro [efecto: instantaneo]
      detener_animacion sol
      detener_animacion luna
      detener_animacion estrella
      animar sol sol_trio
      animar luna luna_trio
      animar estrella estrella_trio
      capa_visible seguir_sol_mapa no
      capa_visible seguir_luna_mapa no
      capa_visible seguir_estrella_mapa no
    }
    mostrar_caja
  fin_si

  # Cierre vivo: la misma formacion acompaña la nueva historia hasta el negro.
  mostrar sol 019 centro [efecto: instantaneo]
  mostrar estrella 00012 centro [efecto: instantaneo]
  esperar 1.5
  mostrar luna 028 centro [efecto: instantaneo]
  esperar 2.8
  luna: "Creo que quiero escribir otra."
  mostrar sol 04 centro [efecto: instantaneo]
  mostrar estrella 0001 centro [efecto: instantaneo]
  sol: "¿Otra expedición?"
  mostrar luna 04 centro [efecto: instantaneo]
  luna: "Quizás."
  mostrar luna 028 centro [efecto: instantaneo]
  luna: "Todavía no sé."

  # Sí: otra vez recurrimos a capas para estos efectos. Sol necesita cuerpo y
  # ojos independientes; Estrella necesita un fundido entre sus dos dibujos.
  # Luna acompaña en capa para que el negro final pueda tapar a los tres:
  # hoy los personajes nativos se dibujan después de las capas, incluso con z alto.
  # Conservamos las coordenadas de sus retratos; no es un cambio de plano.
  # Los ojos 02 de Sol van 60 px a la derecha del cuerpo, como su mirada a Luna.
  capa seguir_sol_cuerpo "assets/demo/sol/Sol_Compuesto/0.png" x=481 y=144 w=918 h=918 z=30
  capa seguir_sol_luz "assets/demo/sol/Sol_Compuesto/Light.png" x=481 y=144 w=918 h=918 z=31
  capa seguir_sol_ojos "assets/demo/sol/Sol_Compuesto/02.png" x=541 y=144 w=918 h=918 z=40
  capa seguir_estrella_cuerpo "assets/demo/estrella/Estrella_0001.png" x=-77 y=150 w=918 h=918 z=20
  capa seguir_estrella_luz "assets/demo/estrella/Variante/Estrella_brillante_018.png" x=-77 y=150 w=918 h=918 z=21
  capa seguir_luna_cuerpo "assets/demo/luna/Luna_028.png" x=1031 y=149 w=918 h=918 z=35
  capa_visible seguir_sol_luz no
  capa_visible seguir_estrella_luz no
  ocultar sol [efecto: instantaneo]
  ocultar luna [efecto: instantaneo]
  ocultar estrella [efecto: instantaneo]
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella

  # Estrella se enciende con su bufanda; Luna vuelve la mirada hacia ella.
  # El dibujo normal sigue opaco debajo: el encendido no oscurece el fondo.
  capa seguir_luna_cuerpo "assets/demo/luna/Luna_04.png"
  ocultar_caja
  cinematica {
    sonido "assets/sonidos/estrella_encendido.wav"
    capa_fade seguir_estrella_luz aparecer 2
    esperar 2
    capa_visible seguir_estrella_cuerpo no
  }
  esperar 1.2
  mostrar_caja
  capa seguir_estrella_luz "assets/demo/estrella/Variante/Estrella_brillante_017.png"
  estrella: "Yo quiero salir en la otra."
  luna: "¿En la historia?"
  capa seguir_estrella_luz "assets/demo/estrella/Variante/Estrella_brillante_018.png"
  estrella: "Sí."
  sol: "Yo también."

  ocultar_caja
  cinematica {
    sonido "assets/sonidos/sol_encendido.wav"
    capa_fade seguir_sol_luz aparecer 2
    esperar 2
    capa_visible seguir_sol_cuerpo no
  }
  mostrar_caja
  capa seguir_luna_cuerpo "assets/demo/luna/Luna_025.png"
  luna: "Bueno."
  # El último diálogo reúne las tres miradas al frente, en neutro y con su luz.
  capa seguir_luna_cuerpo "assets/demo/luna/Luna_01.png"
  capa seguir_estrella_luz "assets/demo/estrella/Variante/Estrella_brillante_019.png"
  capa seguir_sol_ojos "assets/demo/sol/Sol_Compuesto/02.png" x=481 y=144
  luna: "Entonces tendré que hacerles sitio."
  esperar 0.6
  ocultar_caja
  # Alternative ya muestra a Sol y Estrella encendidos: entra después de ambos.
  # Es el último plano, con los cuatro; termina el fundido antes de respirar.
  capa seguir_fondo_final "assets/demo/Cgs/Alternative.png" z=80 x=0 y=0 w=1920 h=1080
  capa_visible seguir_fondo_final no
  capa negro_seguir_cierre "#000000" z=900 x=0 y=0 w=1920 h=1080
  capa_visible negro_seguir_cierre no
  cinematica {
    capa_fade seguir_fondo_final aparecer 2
    esperar 2
    capa_visible seguir_sol_luz no
    capa_visible seguir_sol_ojos no
    capa_visible seguir_estrella_luz no
    capa_visible seguir_luna_cuerpo no
    esperar 3
    capa_fade negro_seguir_cierre aparecer 3
    esperar 3
    parar_musica
    esperar 1
  }
  fin

# Vuelta de las copias: el resultado del combate cambia el diálogo.

escena sol_despues_copias
  capa mundo "#E8CDB5" z=10 x=0 y=0 w=1920 h=1080
  animacion sol_trio mover_x de=-578 a=-578 en=0.05
  animacion luna_trio mover_x de=530 a=530 en=0.05
  animacion estrella_trio mover_x de=-20 a=-20 en=0.05
  mostrar sol neutral centro [efecto: instantaneo]
  mostrar luna neutral centro [efecto: instantaneo]
  mostrar estrella neutral centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_trio
  animar luna luna_trio
  animar estrella estrella_trio
  # Primero comprobamos si queda 1 HP; después, si hubo golpes.
  # Elegimos la rama antes de una espera para mostrar sus caras en el mismo cuadro.
  si copias_hp == 1
    ir_a copias_un_hp
  fin_si
  si copias_golpes > 0
    ir_a copias_dano
  fin_si
  mostrar sol 05 centro
  mostrar luna 01 centro
  mostrar estrella 01 centro
  esperar 0.6
  musica "assets/musica/despues_sin_dano.ogg"
  estrella: "Ya no vienen."
  sol: "La pestaña ya no dice nada."
  mostrar sol 02 centro
  esperar 1.2
  luna: "Se parecían a ti."
  sol: "No eran iguales."
  luna: "Ya sé."
  esperar 1.2
  mostrar luna 04 centro
  luna: "Todos querían venir aquí."
  sol: "Sí."
  luna: "Pero ya estaba ocupado."
  mostrar sol 04 centro
  sol: "No se detuvieron."
  mostrar luna 01 centro
  mostrar sol 02 centro
  esperar 1.2
  estrella: "Él se quedó."
  esperar 1.0
  luna: "...Sí."
  esperar 1.2
  luna: "¿Estás bien?"
  sol: "Se movió hasta el final."
  mostrar luna 04 centro
  luna: "Eso no es lo que pregunté."
  mostrar sol 04 centro
  sol: "No sé contestarte."
  ir_a copias_comun

# Resultado de las copias: hubo daño.
escena copias_dano
  mostrar sol 05 centro
  mostrar luna 01 centro
  mostrar estrella 01 centro
  cinematica {
    esperar 0.6
  }
  musica "assets/musica/despues_con_dano.ogg"
  estrella: "Ya no vienen."
  sol: "La pestaña ya no dice nada."
  mostrar sol 02 centro
  cinematica {
    esperar 1.2
  }
  mostrar luna 04 centro
  luna: "Se parecían a ti."
  sol: "No eran iguales."
  mostrar luna 01 centro
  luna: "Ya sé."
  luna: "Todos querían venir aquí."
  sol: "Sí."
  luna: "Pero ya estaba ocupado."
  sol: "No se detuvieron."
  mostrar sol 02 centro
  mostrar luna 01 centro
  estrella: "Él se quedó."
  cinematica {
    esperar 1.0
  }
  luna: "...Sí."
  luna: "¿Estás bien?"
  sol: "Se movió hasta el final."
  luna: "Te alcanzaron."
  sol: "Sí."
  esperar 1.2
  luna: "Eso se vio raro."
  estrella: "Se le subieron."
  sol: "No se quedaron."
  luna: "Lo sé."
  luna: "Igual lo vi."
  esperar 1.2
  sol: "Siguió moviéndose."
  mostrar luna 04 centro
  luna: "Eso no es lo mismo."
  sol: "No sé qué tanto cambia."
  mostrar luna 01 centro
  luna: "Yo tampoco."
  ir_a copias_comun

# Resultado de las copias: queda 1 HP.
escena copias_un_hp
  mostrar sol 05 centro
  mostrar luna 01 centro
  mostrar estrella 01 centro
  cinematica {
    esperar 0.6
  }
  musica "assets/musica/despues_un_golpe.ogg"
  estrella: "Ya no vienen."
  sol: "La pestaña ya no dice nada."
  mostrar sol 02 centro
  mostrar estrella 09 centro
  cinematica {
    esperar 0.8
  }
  mostrar luna 01 centro
  luna: "Se parecían a ti."
  mostrar sol 04 centro
  sol: "No eran iguales."
  mostrar luna 01 centro
  luna: "Ya sé."
  mostrar estrella 01 centro
  mostrar sol 02 centro
  esperar 0.5
  luna: "Todos querían venir aquí."
  sol: "Sí."
  luna: "Pero ya estaba ocupado."
  mostrar sol 02 centro
  sol: "No se detuvieron."
  mostrar estrella 01 centro
  estrella: "Él se quedó."
  cinematica {
    esperar 0.6
  }
  luna: "...Sí."
  mostrar luna 01 centro
  luna: "¿Estás bien?"
  mostrar sol 04 centro
  sol: "Se movió hasta el final."
  mostrar luna 04 centro
  luna: "Hubo un momento en que pensé que no iba a hacerlo."
  # Sol deja de contestar a Luna y vuelve a mirar al viajero.
  mostrar sol 02 centro
  mostrar luna 01 centro
  sol: "..."
  sol: "No estaba seguro."
  mostrar luna 01 centro
  luna: "Te alcanzaron."
  sol: "Sí."
  mostrar estrella 09 centro
  estrella: "Se le subieron."
  mostrar sol 04 centro
  sol: "No se quedaron."
  mostrar luna 04 centro
  luna: "Lo sé."
  mostrar luna 01 centro
  luna: "Pero pensé que..."
  mostrar sol 02 centro
  mostrar estrella 010 centro
  cinematica {
    esperar 0.8
  }
  mostrar estrella 01 centro
  estrella: "Aquí está."
  cinematica {
    esperar 0.6
  }
  mostrar luna 01 centro
  luna: "Sí."
  luna: "Aquí está."
  ir_a copias_comun

# Las ramas del combate se reúnen antes de la demostración.
escena copias_comun
  # Después de las reacciones al combate, retomamos la elección del paseo.
  si paseo_eleccion == luna
    ir_a ruta_luna
  fin_si
  # Como en "forma", la capa negra se combina con el alpha del reparto
  # para que el fundido también oculte a los personajes.
  capa negro_vamos "#000000" z=900 x=0 y=0 w=1920 h=1080
  capa_visible negro_vamos no
  mostrar sol 02 centro
  mostrar luna 01 centro
  esperar 1.2
  luna: "Creí que bastaba con darle una forma."
  sol: "Bastó para caminar."
  luna: "Y luego tuvo que defenderse."
  sol: "No se lo pedimos."
  esperar 1.2
  luna: "No era lo que había escrito."
  sol: "Tampoco lo que yo quería probar."
  esperar 2.0
  mostrar luna 04 centro
  luna: "¿Y si aparecen más?"
  sol: "No sé."
  esperar 1.2
  mostrar sol 04 centro
  sol: "Pero ahora no viene ninguno."
  mostrar luna 01 centro
  mostrar sol 02 centro
  luna: "...Bueno."
  # La preparación de la demostración va en silencio.
  parar_musica
  esperar 1.6
  luna: "Bien. Entonces, el viajero—{interrumpir}"
  mostrar sol 04 centro
  mostrar luna 04 centro
  mostrar estrella 09 centro
  sol: "Todavía no les enseño lo mío."
  mostrar luna 09 centro
  luna: "¿Ahora?"
  mostrar sol 02 centro
  sol: "Cuando quieras."
  mostrar estrella 01 centro
  mostrar luna 04 centro
  esperar 1.2
  mostrar luna 09 centro
  luna: "¿También es un test?"
  mostrar sol 04 centro
  sol: "Sí."
  mostrar luna 04 centro
  luna: "Acabamos de salir de uno."
  sol: "Ese no era mío."
  mostrar luna 01 centro
  esperar 1.2
  luna: "Pero él acaba de salir de eso."
  sol: "No voy a pedirle que haga nada todavía."
  mostrar luna 04 centro
  luna: "Acabas de decir que quieres un test."
  mostrar sol 02 centro
  sol: "Quiero enseñarle lo que preparé."
  luna: "¿Y después?"
  sol: "Después vemos."
  esperar 1.2
  mostrar luna 09 centro
  luna: "...¿Solo enseñárselo?"
  mostrar sol 04 centro
  sol: "Para empezar."
  esperar 1.0
  mostrar luna 04 centro
  luna: "No sabemos si está bien."
  sol: "Por eso."
  esperar 1.6
  mostrar estrella 09 centro
  estrella: "Yo quiero verlo."
  mostrar luna 09 centro
  luna: "¿El test?"
  mostrar estrella 010 centro
  estrella: "Lo que preparó."
  mostrar sol 02 centro
  mostrar estrella 01 centro
  mostrar luna 01 centro
  esperar 2.0
  mostrar luna 04 centro
  luna: "Voy con ustedes."
  mostrar sol 04 centro
  sol: "No te estaba dejando aquí."
  mostrar estrella 09 centro
  estrella: "Yo ya iba."
  esperar 0.8
  mostrar sol 05 centro
  sol: "...Vamos."
  ocultar_caja

  capa_fade negro_vamos aparecer 1.0
  animar sol se_apagan
  animar luna se_apagan
  animar estrella se_apagan
  esperar 1.1
  esperar 5.0
  # "sonido" no detiene el guion: los "esperar" espacian los efectos del arrastre.
  # Si cambias la duración de esos archivos, revisa las esperas para conservar el ritmo.
  sonido "assets/sonidos/arrastre_cesped.wav"
  esperar 2.0
  sonido "assets/sonidos/tiron_cesped.wav"
  esperar 1.3
  mostrar_caja
  luna: "¿Qué estás haciendo?"
  sol: "Poniéndolo en su sitio."
  luna: "¿A quién?"
  sol: "Al señuelo."
  sonido "assets/sonidos/arrastre_cesped.wav"
  luna: "¿Lo estás arrastrando?"
  sol: "Sí."
  sonido "assets/sonidos/tiron_cesped.wav"
  estrella: "No está ayudando."
  sonido "assets/sonidos/tiron_cesped.wav"
  sol: "No puede."
  esperar 0.4
  sonido "assets/sonidos/acomodar_cesped.wav"
  esperar 1.0
  luna: "¿Y para qué lo necesitas?"
  sol: "Para que disparen primero a otra cosa."
  esperar 1.2
  luna: "...¿Disparen?"
  sol: "Sí."
  luna: "¿A qué?"
  sol: "Al señuelo."
  estrella: "Que dispare."
  luna: "¡Estrella!"
  estrella: "Al señuelo."
  esperar 1.2
  luna: "...¿"Primero"?"
  sol: "Bien. Miren."
  ocultar sol [efecto: instantaneo]
  ocultar luna [efecto: instantaneo]
  ocultar estrella [efecto: instantaneo]
  # Retiramos "se_apagan", que quedó en alpha cero, antes de volver al VN.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  capa_visible negro_vamos no
  ocultar_caja
  gameplay sidescroll "testigo_demostracion.eclmap"
  mostrar_caja
  ir_a sol_despues_demo


# Sol explica la prueba antes de ofrecerla al jugador.
escena sol_despues_demo
  # Vuelta de la demostración: retomamos el VN con el cielo más avanzado.
  capa mundo "#E2BBA3" z=10 x=0 y=0 w=1920 h=1080
  animacion sol_trio mover_x de=-578 a=-578 en=0.05
  animacion luna_trio mover_x de=530 a=530 en=0.05
  animacion estrella_trio mover_x de=-20 a=-20 en=0.05
  mostrar sol 04 centro [efecto: instantaneo]
  mostrar luna 010 centro [efecto: instantaneo]
  mostrar estrella 09 centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_trio
  animar luna luna_trio
  animar estrella estrella_trio
  esperar 0.6
  luna: "..."
  sol: "¿Qué?"
  musica "assets/musica/tema_vn_con_estrella.ogg"
  mostrar luna 04 centro
  luna: "Ya entendí."
  sol: "Bien."
  mostrar luna 021 centro
  mostrar estrella 010 centro
  luna: "No, Sol."
  mostrar luna 09 centro
  luna: "¿Eso era lo que ibas a ponerle enfrente?"
  mostrar estrella 09 centro
  sol: "Sí."
  mostrar luna 021 centro
  luna: "Qué buena idea."
  mostrar sol 016 centro
  sol: "Gracias."
  mostrar estrella 01 centro
  luna: "No era un cumplido."
  mostrar sol 04 centro
  sol: "Ah."
  esperar 1.2
  si copias_hp == 1
    ir_a demo_un_hp
  fin_si
  si copias_golpes > 0
    ir_a demo_dano
  fin_si
  mostrar luna 04 centro
  luna: "Acaba de pelear contra seis cosas que se le parecían."
  sol: "Lo vi."
  mostrar luna 09 centro
  luna: "¿Y ahora quieres que esquive eso?"
  sol: "No dije que ahora."
  mostrar luna 04 centro
  luna: "¿Y si le dan?"
  sol: "Lo paro."
  luna: "..."
  mostrar luna 09 centro
  luna: "¿Puedes?"
  sol: "Sí."
  ir_a demo_sigue

# La explicación cambia si hubo daño.
escena demo_dano
  mostrar luna 01 centro
  mostrar sol 02 centro
  luna: "Acaba de pelear contra seis cosas que se le parecían."
  sol: "Lo vi."
  mostrar luna 09 centro
  luna: "¿Y ahora quieres que esquive eso?"
  mostrar sol 04 centro
  sol: "No dije que ahora."
  mostrar luna 04 centro
  luna: "¿Y si vuelven a alcanzarlo?"
  sol: "Lo paro."
  luna: "..."
  mostrar luna 09 centro
  luna: "¿Puedes?"
  sol: "Sí."
  ir_a demo_sigue

# La explicación cambia si queda 1 HP.
escena demo_un_hp
  mostrar luna 01 centro
  mostrar sol 02 centro
  mostrar estrella 01 centro
  luna: "Acaba de pelear contra seis cosas que se le parecían."
  mostrar sol 04 centro
  sol: "Lo vi."
  mostrar luna 04 centro
  luna: "Apenas consiguió salir de ahí."
  mostrar sol 02 centro
  sol: "Sí."
  cinematica {
    esperar 0.8
  }
  mostrar luna 04 centro
  luna: "¿Y ahora quieres que esquive eso?"
  mostrar sol 04 centro
  sol: "No dije que ahora."
  mostrar luna 09 centro
  luna: "¿Y si vuelve a pasar?"
  mostrar sol 02 centro
  sol: "Lo paro."
  mostrar luna 04 centro
  luna: "..."
  cinematica {
    esperar 0.6
  }
  mostrar luna 09 centro
  luna: "¿Puedes?"
  mostrar sol 04 centro
  sol: "Sí."
  ir_a demo_sigue

# Elección: intentar la prueba de Sol o continuar con Luna.
escena demo_sigue
  mostrar luna 04 centro
  esperar 1.2
  luna: "¿Eso era lo que llevabas preparando?"
  sol: "Una parte."
  luna: "No me dijiste que tenía torretas."
  sol: "No preguntaste."
  luna: "..."
  esperar 1.2
  luna: "¿Y qué se supone que tiene que hacer?"
  sol: "Llegar al otro lado."
  mostrar luna 09 centro
  luna: "¿Y qué hay al otro lado?"
  sol: "La salida."
  luna: "..."
  sol: "Lo terminé. Quería verlo funcionar."
  mostrar luna 04 centro
  luna: "Ya funciona. Le disparó dos veces."
  sol: "Esa parte."
  esperar 1.2
  sol: "El señuelo no puede cruzarlo."
  esperar 1.0
  mostrar luna 01 centro
  luna: "..."
  luna: "¿Y si no quiere?"
  sol: "No lo hace."
  esperar 1.2
  estrella: "No le has preguntado."
  esperar 0.8
  sol: "Todavía no."
  esperar 1.2
  luna: "..."
  luna: "Bueno."
  esperar 0.8
  mostrar sol 02 centro
  sol: "¿Quieres probarlo?"
  esperar 1.0
  elegir:
    "Quiero intentarlo." -> sol_entra
    "Prefiero seguir con Luna." -> luna_retoma


# Continuar con Luna después de las pruebas de Sol.
escena luna_retoma
  set viene_de_sol = 1
  # Si seguimos con Luna después de Sol, la tarde está más avanzada.
  # Estas capas de color funden sobre mundo mientras continúa el diálogo;
  # se retiran al llegar al lugar en 2D.
  capa mundo_b "#BE9295" z=11 x=0 y=0 w=1920 h=1080
  capa_visible mundo_b no
  capa mundo_c "#A48494" z=12 x=0 y=0 w=1920 h=1080
  capa_visible mundo_c no
  parar_musica
  # Antes del intercambio con Estrella: Sol izquierda, Estrella centro, Luna derecha.
  # Reponemos también las posiciones al entrar directamente desde el checkpoint.
  animacion retoma_sol_pos mover_x de=-578 a=-578 en=0.05
  animacion retoma_estrella_pos mover_x de=-20 a=-20 en=0.05
  animacion retoma_luna_pos mover_x de=530 a=530 en=0.05
  mostrar sol 02 centro [efecto: instantaneo]
  mostrar luna 04 centro [efecto: instantaneo]
  mostrar estrella 01 centro [efecto: instantaneo]
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol retoma_sol_pos
  animar luna retoma_luna_pos
  animar estrella retoma_estrella_pos
  sol: "Bueno."
  luna: "¿Entonces seguimos?"
  mostrar sol 04 centro [efecto: instantaneo]
  sol: "Sí."
  mostrar luna 01 centro [efecto: instantaneo]
  luna: "..."
  capa_fade mundo_b aparecer 3
  mostrar luna 09 centro [efecto: instantaneo]
  luna: "¿Dónde nos habíamos quedado?"
  sol: "En tu historia."
  mostrar luna 04 centro [efecto: instantaneo]
  luna: "Sí, pero ¿en qué parte?"
  mostrar estrella 010 centro [efecto: instantaneo]
  estrella: "Íbamos a ir a algún sitio."
  mostrar luna 02 centro [efecto: instantaneo]
  luna: "¡La expedición!"
  mostrar sol 016 centro [efecto: instantaneo]
  sol: "Esa."
  mostrar luna 01 centro [efecto: instantaneo]
  mostrar estrella 01 centro [efecto: instantaneo]
  luna: "Bueno."
  ir_a luna_partes


# Prueba de Sol: tres salas conectadas en 2D.

escena sol_entra
  sol: "Bien."
  esperar 1.0
  luna: "..."
  luna: "Voy a mirar."
  estrella: "Yo también."
  sol: "Vamos."
  set sala_historia = 1
  set sala_fin = "-"
  set sala_golpes = 0
  # Paramos la música del VN para que no se reanude al regresar de las salas.
  parar_musica
  ocultar sol [efecto: instantaneo]
  ocultar luna [efecto: instantaneo]
  ocultar estrella [efecto: instantaneo]
  ocultar_caja
  gameplay sidescroll "testigo_sala_1.eclmap"
  si sala_fin == llegada
    ir_a sol_llego
  fin_si
  si sala_fin == golpe
    mostrar_caja
    ir_a sol_golpe
  fin_si
  # Si se abandona la prueba sin resultado, terminamos el guion y volvemos al menú.
  fin


# Final de Sol: se llegó a la salida.
escena sol_llego
  ocultar_caja
  capa mundo "#BE9295" z=10 x=0 y=0 w=1920 h=1080
  # Sol queda en el centro; Estrella a su izquierda y Luna a su derecha.
  animacion sol_cierre mover_x de=-25 a=-25 en=0.05
  animacion sol_cierre mover_y de=-16 a=-16 en=0.05
  animacion luna_cierre mover_x de=541 a=541 en=0.05
  animacion luna_cierre mover_y de=-16 a=-16 en=0.05
  animacion estrella_cierre mover_x de=-601 a=-601 en=0.05
  animacion estrella_cierre mover_y de=-16 a=-16 en=0.05
  mostrar sol neutral centro [efecto: instantaneo]
  mostrar luna neutral centro [efecto: instantaneo]
  # Estrella vuelve de las salas con bufanda.
  mostrar estrella 0003 centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_cierre
  animar luna luna_cierre
  animar estrella estrella_cierre
  esperar 1.2
  mostrar_caja
  # Sol reserva sus silencios para lo que siente; los hechos los dice sin dudar.
  sol: "Llegaste."
  sol: "Hasta el final."
  luna: "Sí."
  sol: "Las tres."
  mostrar estrella 0005 centro
  estrella: "Las tres."
  mostrar luna 04 centro
  luna: "¿Ya terminó?"
  mostrar estrella 0001 centro
  sol: "Sí."
  luna: "¿De verdad?"
  sol: "Llegó a la salida."
  luna: "¿No falta nada?"
  sol: "No."
  mostrar luna 01 centro
  mostrar estrella 0003 centro
  esperar 1.2
  luna: "..."
  luna: "Bueno."
  mostrar luna 04 centro
  luna: "Entonces lo consiguió."
  sol: "Sí."
  sol: "La primera era para caminar."
  luna: "Y saltar."
  sol: "Sí."
  sol: "Para empezar."
  sol: "La segunda tenía las torretas."
  mostrar estrella 0005 centro
  estrella: "Les ganó."
  mostrar sol 05 centro
  sol: "Las esquivó."
  mostrar estrella 0001 centro
  estrella: "Por eso."
  mostrar sol 02 centro
  esperar 1.2
  sol: "Y la última..."
  mostrar estrella 0003 centro
  sol: "También la terminaste."
  sol: "Quería ver cómo salía esa parte."
  luna: "¿La última?"
  sol: "Sí."
  mostrar luna 09 centro
  luna: "¿Por qué?"
  sol: "..."
  sol: "Era la que más quería ver."
  mostrar luna 01 centro
  luna: "Ah."
  mostrar luna 04 centro
  luna: "¿Y?"
  sol: "Funcionó."
  mostrar luna 09 centro
  luna: "¿Todo?"
  sol: "Todo."
  sol: "Bien."
  mostrar luna 01 centro
  musica "assets/musica/final_sol_cierre.ogg"
  ocultar_caja
  # Luna nota la mirada sostenida de Sol antes de preguntarle.
  cinematica {
    mostrar estrella 0001 centro
    mostrar luna 04 centro
    esperar 1.2
  }
  mostrar_caja
  luna: "..."
  mostrar luna 09 centro
  luna: "¿Qué?"
  sol: "Nada."
  mostrar luna 023 centro
  mostrar estrella 0003 centro
  luna: "Sigues mirándolo."
  sol: "Sí."
  mostrar luna 04 centro
  luna: "¿Pasa algo?"
  sol: "No."
  luna: "..."
  mostrar luna 09 centro
  luna: "¿No esperabas que lo consiguiera?"
  sol: "No sabía."
  luna: "¿Qué cosa?"
  sol: "Si iba a llegar."
  luna: "..."
  luna: "Pero tú hiciste la prueba."
  sol: "Sí."
  luna: "¿Y no sabías?"
  mostrar sol 04 centro
  sol: "Sabía dónde terminaba."
  esperar 1.2
  mostrar sol 02 centro
  sol: "No cómo iba a llegar."
  esperar 1.0
  mostrar luna 01 centro
  luna: "..."
  luna: "Ah."
  mostrar estrella 0001 centro
  estrella: "Yo tampoco sabía."
  mostrar luna 04 centro
  luna: "¿Si iba a llegar?"
  mostrar estrella 0003 centro
  estrella: "Cómo."
  sol: "Quería ver eso."
  esperar 1.2
  mostrar luna 01 centro
  luna: "..."
  luna: "Bueno."
  mostrar luna 04 centro
  luna: "Me alegra que pudieras verlo."
  sol: "Sí."
  sol: "Pensé que cuando terminara iba a querer revisar algunas cosas."
  luna: "¿Y?"
  sol: "..."
  sol: "No tengo ganas."
  mostrar luna 09 centro
  luna: "¿De revisarlas?"
  sol: "Ahora no."
  mostrar estrella 0007 centro
  estrella: "Podemos quedarnos un rato."
  mostrar sol 05 centro
  sol: "Sí."
  mostrar estrella 0005 centro
  mostrar sol 02 centro
  sol: "Me gustaría."
  esperar 1.0
  mostrar luna 01 centro
  luna: "..."
  luna: "Bueno."
  mostrar luna 02 centro
  luna: "No tenemos prisa."
  esperar 1.2
  sol: "..."
  mostrar estrella 0001 centro
  mostrar sol 02 centro
  sol: "Me gustó."
  mostrar luna 04 centro
  luna: "¿Qué cosa?"
  sol: "Todo esto."
  mostrar sol 02 centro
  sol: "Que vinieran."
  estrella: "Yo quería venir."
  mostrar sol 05 centro
  sol: "Ya sé."
  mostrar estrella 0003 centro
  estrella: "Y quería verlo."
  mostrar sol 02 centro
  sol: "Me alegra."
  mostrar estrella 0005 centro
  estrella: "A mí también."
  luna: "..."
  luna: "Sol."
  mostrar sol 04 centro
  sol: "¿Qué?"
  mostrar estrella 0001 centro
  luna: "Estás contento."
  sol: "..."
  cinematica {
    mostrar sol 04 centro
    esperar 2.0
    mostrar sol 05 centro
    esperar 1.0
    mostrar sol 02 centro
    esperar 0.8
  }
  mostrar sol 01 centro
  sol: "Sí."
  esperar 1.2
  mostrar sol 016 centro
  sol: "Bastante."
  esperar 1.2
  # La respuesta del jugador también puede ser un silencio.
  mostrar sol 02 centro
  sol: "Oye."
  mostrar estrella 0003 centro
  sol: "¿Qué te pareció?"
  mostrar luna 01 centro
  sol: "Lo que hice."
  sol: "¿Te gustó?"
  esperar 1.8
  elegir:
    "..." -> sol_ah


# La respuesta silenciosa del jugador.
escena sol_ah
  sol: "Ah."
  mostrar luna 09 centro
  luna: "¿Ah?"
  mostrar sol 04 centro
  sol: "Contestó."
  luna: "¿Y qué dijo?"
  sol: "No sé."
  estrella: "Yo tampoco."
  mostrar luna 01 centro
  luna: "..."
  luna: "Bueno."
  mostrar sol 02 centro
  sol: "No sé si te gustó."
  sol: "Pero quería preguntarte."
  esperar 1.2
  luna: "..."
  luna: "Oye."
  mostrar luna 023 centro
  luna: "Estás más bajito."
  mostrar sol 04 centro
  sol: "¿Yo?"
  mostrar luna 04 centro
  luna: "Él."
  mostrar sol 02 centro
  sol: "Está menos cuadrado."
  mostrar luna 016 centro
  luna: "..."
  luna: "Sí."
  mostrar luna 02 centro
  luna: "Te queda bien."
  sol: "..."
  esperar 1.2
  mostrar luna 01 centro
  sol: "Gracias por probarlo."
  esperar 2.0
  mostrar estrella 0001 centro
  estrella: "¿Ya no vas a hacer nada?"
  mostrar sol 05 centro
  mostrar luna 04 centro
  sol: "¿Ahora?"
  estrella: "Sí."
  sol: "No."
  mostrar estrella 0003 centro
  estrella: "Ah."
  mostrar sol 02 centro
  sol: "Pero creo que quiero hacer otra cosa."
  mostrar luna 09 centro
  luna: "¿Otra prueba?"
  mostrar sol 04 centro
  sol: "No sé."
  luna: "¿No sabes?"
  sol: "Todavía no."
  mostrar estrella 0005 centro
  estrella: "Yo quiero verla."
  mostrar sol 05 centro
  sol: "No existe."
  mostrar estrella 0001 centro
  estrella: "Ya sé."
  estrella: "Pero cuando exista."
  sol: "Bueno."
  mostrar luna 02 centro
  mostrar estrella 0003 centro
  luna: "Yo también quiero."
  mostrar sol 04 centro
  sol: "Bien."
  mostrar sol 02 centro
  sol: "Cuando tenga algo, te lo enseño."
  esperar 1.2
  sol: "Si quieres."
  mostrar luna 01 centro
  luna: "..."
  luna: "Bueno."
  mostrar luna 04 centro
  luna: "A nosotras también nos avisas."
  mostrar sol 04 centro
  sol: "Sí."
  mostrar estrella 0005 centro
  estrella: "Yo quiero probarlo."
  mostrar sol 05 centro
  sol: "Todavía no sabes qué es."
  mostrar estrella 0001 centro
  estrella: "Por eso."
  mostrar luna 021 centro
  luna: "Yo preferiría saber si tiene torretas."
  mostrar sol 04 centro
  sol: "Te aviso."
  luna: "Antes."
  mostrar luna 04 centro
  sol: "Antes."
  mostrar sol 02 centro
  sol: "Bueno."
  mostrar luna 01 centro
  mostrar estrella 0003 centro
  sol: "..."
  sol: "Entonces nos vemos."
  esperar 1.2
  sol: "Cuando haya más."
  ocultar_caja
  animacion sol_despedida alpha de=1 a=0 en=2.0 curva=suave
  capa negro_sol_cierre "#000000" z=80 x=0 y=0 w=1920 h=1080
  capa_visible negro_sol_cierre no
  cinematica {
    capa_fade negro_sol_cierre aparecer 2.0
    animar sol sol_despedida
    animar luna sol_despedida
    animar estrella sol_despedida
    esperar 2.0
    esperar 2.0
    parar_musica
  }
  fin


# Final de Sol: la prueba se detuvo al recibir un golpe.
escena sol_golpe
  ocultar_caja
  capa mundo "#BE9295" z=10 x=0 y=0 w=1920 h=1080
  capa blanca "#FFFFFF" z=70 x=0 y=0 w=1920 h=1080
  capa negro "#000000" z=80 x=0 y=0 w=1920 h=1080
  capa_visible blanca no
  capa_visible negro no
  animacion sol_golpe_pos mover_x de=-25 a=-25 en=0.05
  animacion sol_golpe_pos mover_y de=-16 a=-16 en=0.05
  animacion luna_golpe_pos mover_x de=541 a=541 en=0.05
  animacion luna_golpe_pos mover_y de=-16 a=-16 en=0.05
  animacion estrella_golpe_pos mover_x de=-601 a=-601 en=0.05
  animacion estrella_golpe_pos mover_y de=-16 a=-16 en=0.05
  mostrar sol 02 centro [efecto: instantaneo]
  mostrar luna 01 centro [efecto: instantaneo]
  # Estrella vuelve de las salas con bufanda.
  mostrar estrella 0003 centro [efecto: instantaneo]
  # Reponemos la colocación del trío sin acumular sus animaciones anteriores.
  detener_animacion sol
  detener_animacion luna
  detener_animacion estrella
  animar sol sol_golpe_pos
  animar luna luna_golpe_pos
  animar estrella estrella_golpe_pos
  cinematica {
    esperar 0.8
  }
  mostrar_caja
  sol: "Ya está."
  esperar 1.2
  mostrar luna 04 centro
  luna: "¿La paraste?"
  mostrar sol 04 centro
  sol: "Sí."
  luna: "Dijiste que podías."
  sol: "Podía."
  esperar 1.2
  # El tramo alcanzado cambia la reacción de Estrella.
  si sala_tramo == 3
    estrella: "Faltaba poquito."
  sino
    estrella: "Quedaba más."
  fin_si
  mostrar sol 05 centro
  sol: "Sí."
  mostrar sol 02 centro
  esperar 1.2
  sol: "..."
  sol: "Quería seguir viendo."
  estrella: "Yo también."
  mostrar luna 01 centro
  esperar 1.2
  sol: "..."
  musica "assets/musica/final_sol_dano.ogg"
  sol: "¿Estás bien?"
  esperar 1.2
  mostrar luna 04 centro
  luna: "No sabemos si puede contestarte."
  mostrar sol 04 centro
  sol: "Igual quería preguntarle."
  mostrar sol 02 centro
  mostrar luna 01 centro
  luna: "Sí."
  estrella: "Yo también quiero saber."
  mostrar luna 04 centro
  luna: "¿Si está bien?"
  mostrar estrella 0001 centro
  estrella: "Sí."
  mostrar luna 01 centro
  mostrar estrella 0003 centro
  sol: "Podemos darle un momento."
  luna: "Sí."
  esperar 1.6
  mostrar estrella 0001 centro
  estrella: "¿Y lo que faltaba?"
  mostrar sol 05 centro
  sol: "¿De la prueba?"
  estrella: "Sí."
  sol: "Sigue ahí."
  estrella: "¿Vas a quitarlo?"
  sol: "No."
  estrella: "Ah."
  sol: "Me costó hacerlo."
  mostrar luna 04 centro
  luna: "Eso sí te lo creo."
  mostrar estrella 0003 centro
  estrella: "Yo quería ver la salida."
  mostrar sol 02 centro
  sol: "Yo también."
  mostrar luna 09 centro
  luna: "Tú sabes cómo es."
  mostrar sol 04 centro
  sol: "La hice."
  mostrar estrella 0001 centro
  estrella: "Eso es diferente."
  mostrar sol 05 centro
  sol: "Sí."
  esperar 1.2
  mostrar luna 04 centro
  luna: "Bueno."
  luna: "Tampoco tiene que desaparecer porque no llegamos."
  mostrar sol 04 centro
  sol: "No iba a quitarlo."
  luna: "Ya sé."
  estrella: "Entonces podemos verlo después."
  mostrar sol 05 centro
  sol: "Sí."
  estrella: "¿Cuando volvamos?"
  sol: "Cuando quieran."
  mostrar estrella 0005 centro
  mostrar luna 01 centro
  estrella: "Yo quiero volver."
  sol: "Bueno."
  mostrar sol 02 centro
  esperar 0.8
  sol: "A mí todavía me gustaría verte llegar."
  esperar 1.2
  mostrar estrella 0001 centro
  estrella: "¿Y mientras tanto?"
  mostrar sol 05 centro
  sol: "¿Mientras tanto?"
  estrella: "¿Qué hacemos ahora?"
  sol: "No preparé nada más."
  mostrar estrella 0003 centro
  estrella: "Ah."
  esperar 0.8
  mostrar estrella 0001 centro
  estrella: "¿Cómo se te ocurrió la prueba?"
  sol: "¿Toda?"
  estrella: "Sí."
  mostrar sol 03 centro
  sol: "Quería hacer algo que se pudiera recorrer."
  mostrar luna 09 centro
  luna: "¿Y de ahí salieron las torretas?"
  mostrar sol 04 centro
  sol: "No al principio."
  mostrar luna 021 centro
  luna: "Qué buenos tiempos."
  sol: "No estaba terminada."
  mostrar luna 04 centro
  estrella: "¿Qué había?"
  mostrar sol 05 centro
  sol: "Un camino."
  estrella: "¿Y ya?"
  sol: "Y una salida."
  luna: "Me gusta esa versión."
  mostrar sol 04 centro
  sol: "Era bastante corta."
  estrella: "¿Y luego?"
  mostrar sol 05 centro
  sol: "Empecé a cambiar cosas."
  estrella: "¿Como cuáles?"
  sol: "Pensé en poner plataformas que se movieran."
  estrella: "¿Dónde están?"
  sol: "Las quité."
  luna: "¿Por qué?"
  mostrar sol 04 centro
  sol: "No quedaron como quería."
  mostrar estrella 0003 centro
  estrella: "Ah."
  esperar 0.8
  mostrar luna 09 centro
  luna: "¿Y las torretas sí?"
  sol: "Sí."
  mostrar luna 021 centro
  luna: "..."
  luna: "Claro."
  mostrar estrella 0001 centro
  mostrar luna 04 centro
  estrella: "Yo habría querido verlas."
  mostrar sol 05 centro
  sol: "Quizá no te hubieran gustado."
  mostrar estrella 0005 centro
  estrella: "Quería averiguarlo."
  sol: "Ah."
  esperar 0.8
  mostrar luna 09 centro
  luna: "¿Cuántas cosas más quitaste?"
  mostrar sol 04 centro
  sol: "Unas cuantas."
  mostrar estrella 0001 centro
  estrella: "¿Nos las cuentas?"
  mostrar sol 05 centro
  sol: "Si quieren."
  luna: "¿Cuánto tiempo nos llevaría?"
  mostrar sol 04 centro
  sol: "No sé."
  luna: "¿No las contaste?"
  sol: "No conté cuánto tardé."
  mostrar luna 04 centro
  luna: "Ah."
  mostrar luna 021 centro
  luna: "Eso me preocupa más."
  sol: "¿Por qué?"
  mostrar luna 010 centro
  luna: "¡Porque ya llevamos un buen rato preguntándote!"
  mostrar luna 04 centro
  sol: "Ustedes preguntaron."
  mostrar estrella 0005 centro
  estrella: "Yo quiero seguir."
  mostrar sol 05 centro
  sol: "Bueno."
  esperar 0.8
  mostrar estrella 0003 centro
  estrella: "Aunque yo también tengo algo que contar."
  mostrar luna 09 centro
  luna: "¿Ah, sí?"
  mostrar estrella 0001 centro
  estrella: "Sí."
  mostrar luna 04 centro
  sol: "¿De qué?"
  estrella: "De algo que me pasó antes."
  mostrar luna 09 centro
  luna: "¿Y hasta ahora nos dices?"
  estrella: "Estábamos con la prueba."
  mostrar luna 04 centro
  luna: "Bueno, eso es verdad."
  esperar 1.2
  sol: "Puedes contarlo."
  estrella: "¿Ahora?"
  sol: "Sí."
  estrella: "¿Y tú?"
  sol: "¿Qué?"
  estrella: "No habías terminado."
  sol: "Puedo seguir después."
  mostrar estrella 0005 centro
  estrella: "Ah."
  mostrar luna 02 centro
  luna: "¡Tenemos tiempo para los dos!"
  mostrar sol 02 centro
  sol: "Sí."
  esperar 0.8
  mostrar luna 04 centro
  luna: "Bueno."
  luna: "¿Nos vamos?"
  mostrar sol 02 centro
  sol: "Podemos."
  mostrar estrella 0003 centro
  estrella: "Yo quiero ir."
  mostrar luna 01 centro
  luna: "¿Y tú?"
  esperar 0.8
  sol: "Te sigo contando mientras vamos."
  mostrar sol 05 centro
  estrella: "Yo también."
  mostrar luna 02 centro
  luna: "¡Entonces vamos!"
  esperar 0.8
  mostrar luna 04 centro
  estrella: "Tú todavía no estabas."
  # El espacio tras la pausa permite cumplirla antes del avance automático.
  estrella: "Resulta que...{pausa=0.6} {interrumpir}"
  ocultar_caja
  # Nota de autoría: perfecto, queria texto con fade, ¿como demonios termine con tantas capas? va fichado.
  # Eclips necesita simplificar este caso sin perder el control de las capas.
  animacion sol_golpe_despedida alpha de=1 a=0 en=4.0 curva=suave
  fuente cuento_final "third_party/fonts/m_plus_rounded_1c/MPLUSRounded1c-Regular.ttf" tamano=60
  cinematica {
    parar_musica
    sonido "assets/musica/final_sol_narrador.ogg"
    capa_fade blanca aparecer 4.0
    animar sol sol_golpe_despedida
    animar luna sol_golpe_despedida
    animar estrella sol_golpe_despedida
    esperar 4.5
    ocultar sol [efecto: instantaneo]
    ocultar luna [efecto: instantaneo]
    ocultar estrella [efecto: instantaneo]
    esperar 0.5

    # Grupo 01: los cuentos antiguos.
    texto cuento_sol_01 "Dicen los cuentos antiguos" x=690.7 y=469.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_01 no
    texto cuento_sol_02 "que todo camino conduce a algún lugar," x=568.2 y=541.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_02 no
    # En inglés estas líneas miden distinto, y un "texto" va en coordenadas
    # fijas. Por eso, si el idioma es inglés, volvemos a declarar cada texto
    # con otra x para que quede centrado. Volver a declarar un texto que ya
    # existe sólo cambia lo que la línea nombra (aquí, la x); el contenido
    # sigue en español y el .lang lo traduce al mostrarlo.
    si _idioma == en
      texto cuento_sol_01 "Dicen los cuentos antiguos" x=789.3
      texto cuento_sol_02 "que todo camino conduce a algún lugar," x=620.8
    fin_si
    capa_fade cuento_sol_01 aparecer 2.5
    esperar 4.5
    capa_fade cuento_sol_02 aparecer 2.1
    esperar 4.0
    capa_fade cuento_sol_01 desaparecer 1.0
    capa_fade cuento_sol_02 desaparecer 1.0
    esperar 1.1

    # Grupo 02: aventura e historia.
    texto cuento_sol_03 "que toda aventura merece ser contada" x=576.0 y=469.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_03 no
    texto cuento_sol_04 "y que toda historia ha de terminar." x=610.4 y=541.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_04 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_03 "que toda aventura merece ser contada" x=546.4
      texto cuento_sol_04 "y que toda historia ha de terminar." x=648.8
    fin_si
    capa_fade cuento_sol_03 aparecer 2.5
    esperar 4.7
    capa_fade cuento_sol_04 aparecer 1.9
    esperar 4.8
    capa_fade cuento_sol_03 desaparecer 0.6
    capa_fade cuento_sol_04 desaparecer 0.6
    esperar 0.6

    # Grupo 03: lo que dicen los cuentos.
    texto cuento_sol_05 "Dicen muchas cosas los cuentos antiguos." x=544.0 y=505.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_05 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_05 "Dicen muchas cosas los cuentos antiguos." x=656.5
    fin_si
    capa_fade cuento_sol_05 aparecer 0.8
    esperar 4.0
    capa_fade cuento_sol_05 desaparecer 0.6
    esperar 1.2

    # Grupo 04: el camino de Sol.
    # Las continuaciones comparten renglón; entran sin mover lo que ya apareció.
    texto cuento_sol_06 "Aquel día," x=533.5 y=433.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_06 no
    texto cuento_sol_06b "el camino de Sol quedó a medias." x=741.2 y=433.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_06b no
    texto cuento_sol_07 "Había una salida por alcanzar," x=369.2 y=505.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_07 no
    texto cuento_sol_08 "nuevos caminos por explorar" x=980.0 y=505.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_08 no
    texto cuento_sol_09 "y más de un plan que podía esperar." x=607.0 y=577.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_09 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_06 "Aquel día," x=579.6
      texto cuento_sol_06b "el camino de Sol quedó a medias." x=781.4
      texto cuento_sol_07 "Había una salida por alcanzar," x=375.9
      texto cuento_sol_08 "nuevos caminos por explorar" x=1028.3
      texto cuento_sol_09 "y más de un plan que podía esperar." x=558.8
    fin_si
    capa_fade cuento_sol_06 aparecer 0.8
    esperar 2.1
    capa_fade cuento_sol_06b aparecer 0.8
    esperar 2.4
    capa_fade cuento_sol_07 aparecer 0.8
    esperar 2.0
    capa_fade cuento_sol_08 aparecer 0.8
    esperar 2.9
    capa_fade cuento_sol_09 aparecer 0.8
    esperar 4.6
    capa_fade cuento_sol_06 desaparecer 0.6
    capa_fade cuento_sol_06b desaparecer 0.6
    capa_fade cuento_sol_07 desaparecer 0.6
    capa_fade cuento_sol_08 desaparecer 0.6
    capa_fade cuento_sol_09 desaparecer 0.6
    esperar 1.2

    # Grupo 05: escuchar a Estrella.
    # Apagar el bloque antes del remate, que queda solo en el centro.
    texto cuento_sol_10 "Pero," x=740.5 y=469.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_10 no
    texto cuento_sol_10b "por el momento," x=855.7 y=469.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_10b no
    texto cuento_sol_11 "había algo más sencillo que hacer." x=622.1 y=541.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_11 no
    texto cuento_sol_12 "Escuchar a Estrella." x=761.1 y=509.8 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_12 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_10 "Pero," x=747.6
      texto cuento_sol_10b "por el momento," x=842.6
      texto cuento_sol_11 "había algo más sencillo que hacer." x=605.4
      texto cuento_sol_12 "Escuchar a Estrella." x=775.7
    fin_si
    capa_fade cuento_sol_10 aparecer 0.8
    esperar 1.6
    capa_fade cuento_sol_10b aparecer 0.8
    esperar 2.0
    capa_fade cuento_sol_11 aparecer 0.8
    esperar 4.0
    capa_fade cuento_sol_10 desaparecer 0.8
    capa_fade cuento_sol_10b desaparecer 0.8
    capa_fade cuento_sol_11 desaparecer 0.8
    esperar 1.4
    capa_fade cuento_sol_12 aparecer 1.2
    esperar 4.1
    capa_fade cuento_sol_12 desaparecer 0.8
    esperar 1.1

    # Grupo 06: el viajero escucha.
    texto cuento_sol_13 "Y el viajero," x=841.9 y=433.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_13 no
    texto cuento_sol_14 "que había llegado sin saber para qué," x=591.8 y=505.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_14 no
    texto cuento_sol_15 "se quedó a escuchar también." x=667.5 y=577.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_15 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_13 "Y el viajero," x=788.2
      texto cuento_sol_14 "que había llegado sin saber para qué," x=522.7
      texto cuento_sol_15 "se quedó a escuchar también." x=747.5
    fin_si
    capa_fade cuento_sol_13 aparecer 1.8
    esperar 1.8
    capa_fade cuento_sol_14 aparecer 1.4
    esperar 3.1
    capa_fade cuento_sol_15 aparecer 1.2
    esperar 3.9
    capa_fade cuento_sol_13 desaparecer 1.1
    capa_fade cuento_sol_14 desaparecer 1.1
    capa_fade cuento_sol_15 desaparecer 1.1
    esperar 1.3

    # Grupo 07: el dia continua.
    texto cuento_sol_16 "Y así," x=598.0 y=401.8 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_16 no
    texto cuento_sol_16b "entre preguntas y respuestas," x=716.4 y=401.8 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_16b no
    texto cuento_sol_17 "entre recuerdos," x=671.5 y=473.8 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_17 no
    texto cuento_sol_17b "ocurrencias" x=1016.2 y=473.8 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_17b no
    texto cuento_sol_18 "y alguna que otra protesta," x=685.5 y=545.8 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_18 no
    texto cuento_sol_19 "continuaron su día." x=768.8 y=617.8 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_19 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_16 "Y así," x=555.6
      texto cuento_sol_16b "entre preguntas y respuestas," x=712.7
      texto cuento_sol_17 "entre recuerdos," x=707.0
      texto cuento_sol_17b "ocurrencias" x=1104.2
      texto cuento_sol_18 "y alguna que otra protesta," x=666.5
      texto cuento_sol_19 "continuaron su día." x=673.0
    fin_si
    capa_fade cuento_sol_16 aparecer 0.9
    esperar 1.1
    capa_fade cuento_sol_16b aparecer 0.7
    esperar 1.3
    capa_fade cuento_sol_17 aparecer 0.6
    esperar 1.1
    capa_fade cuento_sol_17b aparecer 0.6
    esperar 0.9
    capa_fade cuento_sol_18 aparecer 0.8
    esperar 0.9
    capa_fade cuento_sol_19 aparecer 0.8
    esperar 1.9
    capa_fade cuento_sol_16 desaparecer 1.9
    capa_fade cuento_sol_16b desaparecer 1.9
    capa_fade cuento_sol_17 desaparecer 1.9
    capa_fade cuento_sol_17b desaparecer 1.9
    capa_fade cuento_sol_18 desaparecer 1.9
    capa_fade cuento_sol_19 desaparecer 1.9
    esperar 2.4

    # Grupo 08: lo que no adelantaremos.
    texto cuento_sol_20 "Y de cuanto vino después," x=702.7 y=397.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_20 no
    texto cuento_sol_21 "no adelantaremos palabra." x=694.5 y=469.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_21 no
    texto cuento_sol_22 "Que no se cierran los caminos" x=665.5 y=541.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_22 no
    texto cuento_sol_23 "porque un cuento cierre sus páginas." x=592.8 y=613.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_23 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_20 "Y de cuanto vino después," x=605.9
      texto cuento_sol_21 "no adelantaremos palabra." x=572.5
      texto cuento_sol_22 "Que no se cierran los caminos" x=735.4
      texto cuento_sol_23 "porque un cuento cierre sus páginas." x=645.6
    fin_si
    capa_fade cuento_sol_20 aparecer 1.2
    esperar 2.0
    capa_fade cuento_sol_21 aparecer 0.6
    esperar 2.6
    capa_fade cuento_sol_22 aparecer 0.8
    esperar 1.8
    capa_fade cuento_sol_23 aparecer 0.8
    esperar 2.0
    capa_fade cuento_sol_20 desaparecer 0.6
    capa_fade cuento_sol_21 desaparecer 0.6
    capa_fade cuento_sol_22 desaparecer 0.6
    capa_fade cuento_sol_23 desaparecer 0.6
    esperar 1.5

    # Grupo 09: Sol y Luna.
    texto cuento_sol_24 "Sol todavía tenía un camino que mostrar." x=550.7 y=469.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_24 no
    texto cuento_sol_25 "Luna, una historia que contar." x=658.4 y=541.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_25 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_24 "Sol todavía tenía un camino que mostrar." x=680.2
      texto cuento_sol_25 "Luna, una historia que contar." x=752.6
    fin_si
    capa_fade cuento_sol_24 aparecer 0.8
    esperar 2.0
    capa_fade cuento_sol_25 aparecer 0.8
    esperar 2.0
    capa_fade cuento_sol_24 desaparecer 0.6
    capa_fade cuento_sol_25 desaparecer 0.6
    esperar 0.8

    # Grupo 10: Estrella ya había empezado.
    texto cuento_sol_26 "Y Estrella..." x=844.0 y=469.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_26 no
    texto cuento_sol_27 "Estrella ya había empezado." x=680.9 y=541.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_27 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_26 "Y Estrella..." x=818.8
      texto cuento_sol_27 "Estrella ya había empezado." x=686.3
    fin_si
    capa_fade cuento_sol_26 aparecer 0.8
    esperar 1.4
    capa_fade cuento_sol_27 aparecer 0.8
    esperar 1.8
    capa_fade cuento_sol_26 desaparecer 0.6
    capa_fade cuento_sol_27 desaparecer 0.6
    esperar 0.8

    # Grupo 11: el cuento descansa.
    texto cuento_sol_28 "De modo que, por esta vez," x=691.9 y=433.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_28 no
    texto cuento_sol_29 "dejaremos el cuento descansar." x=647.1 y=505.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_29 no
    texto cuento_sol_30 "Que sus protagonistas" x=734.0 y=577.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_30 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_28 "De modo que, por esta vez," x=787.8
      texto cuento_sol_29 "dejaremos el cuento descansar." x=715.9
      texto cuento_sol_30 "Que sus protagonistas" x=777.5
    fin_si
    capa_fade cuento_sol_28 aparecer 0.8
    esperar 1.8
    capa_fade cuento_sol_29 aparecer 0.8
    esperar 2.0
    capa_fade cuento_sol_30 aparecer 0.8
    esperar 1.4
    capa_fade cuento_sol_28 desaparecer 0.6
    capa_fade cuento_sol_29 desaparecer 0.6
    capa_fade cuento_sol_30 desaparecer 0.6
    esperar 0.7

    # Grupo 12: el último verso.
    # Ya ya ya, nos vemos narrador.
    texto cuento_sol_31 "todavía tienen mucho de qué hablar." x=598.4 y=505.3 color=#1a1a1a z=950 fuente=cuento_final
    capa_visible cuento_sol_31 no
    # En inglés, otra x (ver el grupo 01).
    si _idioma == en
      texto cuento_sol_31 "todavía tienen mucho de qué hablar." x=664.3
    fin_si
    capa_fade cuento_sol_31 aparecer 0.8
    esperar 3.0
    capa_fade cuento_sol_31 desaparecer 0.6
    esperar 0.8

    # Cierre más o menos afinado
    capa_fade negro aparecer 6.0
    esperar 6.0
    parar_musica
    esperar 3.0
  }
  fin
