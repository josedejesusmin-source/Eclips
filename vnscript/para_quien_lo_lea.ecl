# ============================================================
# Algo permaneció.
#
# para_quien_lo_lea.ecl — el poema del autor, el secreto del menú del título.
# No está en el menú: se abre con el código Konami (la secuencia de teclas que
# vive en menu_principal.ecl) y, al terminar, vuelve solo al menú.
#
# CÓMO ESTÁ ESCRITO: cada estrofa es su propia escena ("escena poema_NN"). Todas
# repiten el mismo esquema: un "hotspot avanzar" que cubre la pantalla, los textos
# (los diferidos nacen ocultos), sus "capa_fade" para aparecer, un "esperar" y un
# "esperar_click" que salta a la estrofa siguiente cuando el jugador hace clic.
# Hoy el motor no tiene una forma más corta de escribir esto.
#
# La regla del texto: un "texto" nace VISIBLE. Por eso, cuando su entrada es
# diferida (con "capa_fade ... aparecer"), hay que ocultarlo al nacer con
# "capa_visible X no"; si no, se ve de golpe durante la espera, se apaga y vuelve
# a entrar. El "aparecer" ya lo desoculta solo (visible + alpha 0→1).
#
# Nota: el verso "¿Y si un día ya no estoy aquí?" venía con
# letras en cursiva (𝒴 𝓈𝒾...) que las fuentes del
# proyecto no traen y el motor omitiría en silencio; quedó en
# letra normal para que sobreviva a cualquier máquina.
# ============================================================

escena poema_01
  ocultar_caja
  capa blanco "#FFFFFF" z=900 x=0 y=0 w=1920 h=1080
  hotspot avanzar x=0 y=0 w=1920 h=1080
  # La fuente se declara DENTRO de la escena (no arriba del todo):
  # fuera de una escena el motor ignora el comando (y avisa),
  # así que lo de arriba de la primera escena nunca corre.
  fuente poema "third_party/fonts/nunito/Nunito-Regular.ttf" tamano=38
  esperar 2.0
  texto b01_a "Quise hacer algo que sobreviviera a mis manos." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b01_b "No porque creyera que sería perfecto." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b01_c "Nunca lo fue." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  # las de entrada diferida nacen ocultas (si no, se ven durante la espera)
  capa_visible b01_b no
  capa_visible b01_c no
  capa_fade b01_a aparecer 2.5
  esperar 2.5
  esperar 1.0
  capa_fade b01_b aparecer 1.5
  capa_fade b01_c aparecer 1.5
  esperar 1.5
  esperar_click:
    "avanzar" -> poema_02

escena poema_02
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b01_a desaparecer 0.8
  capa_fade b01_b desaparecer 0.8
  capa_fade b01_c desaparecer 0.8
  esperar 0.8
  texto b02_a "Al principio había una imagen." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b02_b "Lejana." x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b02_c "Clara." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b02_d "Mucho más grande que yo." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  capa_fade b02_a aparecer 1.0
  capa_fade b02_b aparecer 1.0
  capa_fade b02_c aparecer 1.0
  capa_fade b02_d aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_03

escena poema_03
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b02_a desaparecer 0.8
  capa_fade b02_b desaparecer 0.8
  capa_fade b02_c desaparecer 0.8
  capa_fade b02_d desaparecer 0.8
  esperar 0.8
  texto b03_a "Podía verla completa" x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b03_b "aunque todavía no existiera." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b03_c "Sabía cómo debía sentirse." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b03_d "Sabía hasta dónde quería llevarla." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b03_e "Y empecé." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b03_a aparecer 1.0
  capa_fade b03_b aparecer 1.0
  capa_fade b03_c aparecer 1.0
  capa_fade b03_d aparecer 1.0
  capa_fade b03_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_04

escena poema_04
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b03_a desaparecer 0.8
  capa_fade b03_b desaparecer 0.8
  capa_fade b03_c desaparecer 0.8
  capa_fade b03_d desaparecer 0.8
  capa_fade b03_e desaparecer 0.8
  esperar 0.8
  texto b04_a "No siempre avancé." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b04_b "Hubo días que se fueron" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b04_c "sin dejar nada detrás." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  capa_fade b04_a aparecer 1.0
  capa_fade b04_b aparecer 1.0
  capa_fade b04_c aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_05

escena poema_05
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b04_a desaparecer 0.8
  capa_fade b04_b desaparecer 0.8
  capa_fade b04_c desaparecer 0.8
  esperar 0.8
  texto b05_a "Hubo cosas que construí" x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b05_b "solo para descubrir después" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b05_c "que no podían quedarse." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  capa_fade b05_a aparecer 1.0
  capa_fade b05_b aparecer 1.0
  capa_fade b05_c aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_06

escena poema_06
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b05_a desaparecer 0.8
  capa_fade b05_b desaparecer 0.8
  capa_fade b05_c desaparecer 0.8
  esperar 0.8
  texto b06_a "Hubo caminos que parecían correctos" x=560 y=370 color=#1a1a1a z=950 fuente=poema
  texto b06_b "hasta que dejaron de serlo." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b06_c "Y hubo momentos" x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b06_d "en los que el mundo de afuera" x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b06_e "pesaba más" x=560 y=590 color=#1a1a1a z=950 fuente=poema
  texto b06_f "que todo lo que intentaba construir aquí dentro." x=560 y=645 color=#1a1a1a z=950 fuente=poema
  capa_fade b06_a aparecer 1.0
  capa_fade b06_b aparecer 1.0
  capa_fade b06_c aparecer 1.0
  capa_fade b06_d aparecer 1.0
  capa_fade b06_e aparecer 1.0
  capa_fade b06_f aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_07

escena poema_07
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b06_a desaparecer 0.8
  capa_fade b06_b desaparecer 0.8
  capa_fade b06_c desaparecer 0.8
  capa_fade b06_d desaparecer 0.8
  capa_fade b06_e desaparecer 0.8
  capa_fade b06_f desaparecer 0.8
  esperar 0.8
  texto b07_a "No quería que eso fuera una excusa." x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b07_b "Así que seguí." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b07_c "A veces mucho." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b07_d "A veces apenas lo suficiente" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b07_e "para no dejarlo morir." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b07_a aparecer 1.0
  capa_fade b07_b aparecer 1.0
  capa_fade b07_c aparecer 1.0
  capa_fade b07_d aparecer 1.0
  capa_fade b07_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_08

escena poema_08
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b07_a desaparecer 0.8
  capa_fade b07_b desaparecer 0.8
  capa_fade b07_c desaparecer 0.8
  capa_fade b07_d desaparecer 0.8
  capa_fade b07_e desaparecer 0.8
  esperar 0.8
  texto b08_a "La imagen seguía delante de mí." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b08_b "Y yo seguía viendo" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b08_c "todo lo que faltaba." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  capa_fade b08_a aparecer 1.0
  capa_fade b08_b aparecer 1.0
  capa_fade b08_c aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_09

escena poema_09
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b08_a desaparecer 0.8
  capa_fade b08_b desaparecer 0.8
  capa_fade b08_c desaparecer 0.8
  esperar 0.8
  texto b09_a "Lo que podía haber sido." x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b09_b "Lo que habría cambiado con más tiempo." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b09_c "Las partes que todavía me molestaban." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b09_d "Las cosas que quería entregar" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b09_e "y que nunca estuvieron listas." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b09_a aparecer 1.0
  capa_fade b09_b aparecer 1.0
  capa_fade b09_c aparecer 1.0
  capa_fade b09_d aparecer 1.0
  capa_fade b09_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_10

escena poema_10
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b09_a desaparecer 0.8
  capa_fade b09_b desaparecer 0.8
  capa_fade b09_c desaparecer 0.8
  capa_fade b09_d desaparecer 0.8
  capa_fade b09_e desaparecer 0.8
  esperar 0.8
  # Esto fue más difícil de admitir que de escribir.
  texto b10_a "No llegué hasta ella." x=560 y=370 color=#1a1a1a z=950 fuente=poema
  texto b10_b "Eso me dolió más de lo que esperaba." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b10_c "Porque cuando miras algo durante tanto tiempo," x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b10_d "empiezas a confundir" x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b10_e "la distancia que todavía falta" x=560 y=590 color=#1a1a1a z=950 fuente=poema
  texto b10_f "con todo el camino recorrido." x=560 y=645 color=#1a1a1a z=950 fuente=poema
  capa_visible b10_b no
  capa_visible b10_c no
  capa_visible b10_d no
  capa_visible b10_e no
  capa_visible b10_f no
  capa_fade b10_a aparecer 1.5
  esperar 1.5
  esperar 1.2
  capa_fade b10_b aparecer 1.2
  capa_fade b10_c aparecer 1.2
  capa_fade b10_d aparecer 1.2
  capa_fade b10_e aparecer 1.2
  capa_fade b10_f aparecer 1.2
  esperar 1.2
  esperar_click:
    "avanzar" -> poema_11

escena poema_11
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b10_a desaparecer 0.8
  capa_fade b10_b desaparecer 0.8
  capa_fade b10_c desaparecer 0.8
  capa_fade b10_d desaparecer 0.8
  capa_fade b10_e desaparecer 0.8
  capa_fade b10_f desaparecer 0.8
  esperar 0.8
  texto b11_a "Durante un tiempo," x=560 y=370 color=#1a1a1a z=950 fuente=poema
  texto b11_b "eso fue casi todo lo que pude ver." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b11_c "Hasta que miré hacia atrás." x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b11_d "Y allí estaba." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b11_e "No la imagen." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  texto b11_f "Algo real." x=560 y=645 color=#1a1a1a z=950 fuente=poema
  capa_visible b11_d no
  capa_visible b11_e no
  capa_visible b11_f no
  capa_fade b11_a aparecer 1.0
  capa_fade b11_b aparecer 1.0
  capa_fade b11_c aparecer 1.0
  esperar 1.0
  capa_fade b11_d aparecer 1.4
  esperar 1.4
  esperar 0.8
  capa_fade b11_e aparecer 1.2
  esperar 1.2
  esperar 0.8
  capa_fade b11_f aparecer 1.5
  esperar 1.5
  esperar_click:
    "avanzar" -> poema_12

escena poema_12
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b11_a desaparecer 0.8
  capa_fade b11_b desaparecer 0.8
  capa_fade b11_c desaparecer 0.8
  capa_fade b11_d desaparecer 0.8
  capa_fade b11_e desaparecer 0.8
  capa_fade b11_f desaparecer 0.8
  esperar 0.8
  texto b12_a "Imperfecto." x=560 y=370 color=#1a1a1a z=950 fuente=poema
  texto b12_b "Inacabado en algunos bordes." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b12_c "Lleno de decisiones" x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b12_d "que hoy tomaría de otra manera." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b12_e "Pero estaba ahí." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  texto b12_f "Donde antes no había nada." x=560 y=645 color=#1a1a1a z=950 fuente=poema
  capa_fade b12_a aparecer 1.0
  capa_fade b12_b aparecer 1.0
  capa_fade b12_c aparecer 1.0
  capa_fade b12_d aparecer 1.0
  capa_fade b12_e aparecer 1.0
  capa_fade b12_f aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_13

escena poema_13
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b12_a desaparecer 0.8
  capa_fade b12_b desaparecer 0.8
  capa_fade b12_c desaparecer 0.8
  capa_fade b12_d desaparecer 0.8
  capa_fade b12_e desaparecer 0.8
  capa_fade b12_f desaparecer 0.8
  esperar 0.8
  texto b13_a "Hecho de tiempo." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b13_b "De errores." x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b13_c "De curiosidad." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b13_d "De terquedad." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  capa_fade b13_a aparecer 1.0
  capa_fade b13_b aparecer 1.0
  capa_fade b13_c aparecer 1.0
  capa_fade b13_d aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_14

escena poema_14
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b13_a desaparecer 0.8
  capa_fade b13_b desaparecer 0.8
  capa_fade b13_c desaparecer 0.8
  capa_fade b13_d desaparecer 0.8
  esperar 0.8
  texto b14_a "De noches en las que habría sido más fácil" x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b14_b "dejarlo para otro día" x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b14_c "y no volver." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b14_d "Pero, sobre todo," x=560 y=565 color=#1a1a1a z=950 fuente=poema
  # Si alguna razón tenía que sobrevivir, era esta.
  texto b14_e "hecho porque quise hacerlo." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_visible b14_e no
  capa_fade b14_a aparecer 1.0
  capa_fade b14_b aparecer 1.0
  capa_fade b14_c aparecer 1.0
  capa_fade b14_d aparecer 1.0
  esperar 1.0
  esperar 0.8
  capa_fade b14_e aparecer 1.7
  esperar 1.7
  esperar_click:
    "avanzar" -> poema_15

escena poema_15
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b14_a desaparecer 0.8
  capa_fade b14_b desaparecer 0.8
  capa_fade b14_c desaparecer 0.8
  capa_fade b14_d desaparecer 0.8
  capa_fade b14_e desaparecer 0.8
  esperar 0.8
  texto b15_a "Antes de pensar en quién podría usarlo." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b15_b "Antes de preguntarme cuánto valdría." x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b15_c "Antes de saber" x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b15_d "si alguien más llegaría a verlo." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  capa_fade b15_a aparecer 1.0
  capa_fade b15_b aparecer 1.0
  capa_fade b15_c aparecer 1.0
  capa_fade b15_d aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_16

escena poema_16
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b15_a desaparecer 0.8
  capa_fade b15_b desaparecer 0.8
  capa_fade b15_c desaparecer 0.8
  capa_fade b15_d desaparecer 0.8
  esperar 0.8
  texto b16_a "Lo hice porque quería ver" x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b16_b "si aquella imagen" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b16_c "podía existir fuera de mi cabeza." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  capa_fade b16_a aparecer 1.0
  capa_fade b16_b aparecer 1.0
  capa_fade b16_c aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_17

escena poema_17
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b16_a desaparecer 0.8
  capa_fade b16_b desaparecer 0.8
  capa_fade b16_c desaparecer 0.8
  esperar 0.8
  texto b17_a "Después pensé" x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b17_b "que podría servirme otra vez." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b17_c "Después pensé" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b17_d "que quizá le serviría a alguien más." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b17_e "Y entonces dejó de pertenecerme por completo." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_visible b17_e no
  capa_fade b17_a aparecer 1.0
  capa_fade b17_b aparecer 1.0
  capa_fade b17_c aparecer 1.0
  capa_fade b17_d aparecer 1.0
  esperar 1.0
  esperar 1.0
  capa_fade b17_e aparecer 1.8
  esperar 1.8
  esperar_click:
    "avanzar" -> poema_18

escena poema_18
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b17_a desaparecer 0.8
  capa_fade b17_b desaparecer 0.8
  capa_fade b17_c desaparecer 0.8
  capa_fade b17_d desaparecer 0.8
  capa_fade b17_e desaparecer 0.8
  esperar 0.8
  texto b18_a "Tal vez las cosas cambien así." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b18_b "Empiezas construyendo algo para ti" x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b18_c "y un día descubres" x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b18_d "que has dejado una puerta abierta." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  capa_fade b18_a aparecer 1.0
  capa_fade b18_b aparecer 1.0
  capa_fade b18_c aparecer 1.0
  capa_fade b18_d aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_19

escena poema_19
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b18_a desaparecer 0.8
  capa_fade b18_b desaparecer 0.8
  capa_fade b18_c desaparecer 0.8
  capa_fade b18_d desaparecer 0.8
  esperar 0.8
  texto b19_a "No sabes quién la cruzará." x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b19_b "Quizá nadie." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b19_c "Quizá alguien encuentre una pequeña parte" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b19_d "y la lleve mucho más lejos" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b19_e "de lo que tú pudiste." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b19_a aparecer 1.0
  capa_fade b19_b aparecer 1.0
  capa_fade b19_c aparecer 1.0
  capa_fade b19_d aparecer 1.0
  capa_fade b19_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_20

escena poema_20
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b19_a desaparecer 0.8
  capa_fade b19_b desaparecer 0.8
  capa_fade b19_c desaparecer 0.8
  capa_fade b19_d desaparecer 0.8
  capa_fade b19_e desaparecer 0.8
  esperar 0.8
  texto b20_a "Quizá cambie tanto" x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b20_b "que un día apenas puedas reconocerte en ella." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b20_c "Quizá todo esto sea reemplazado" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b20_d "por algo mejor." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b20_e "Espero que sí." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b20_a aparecer 1.0
  capa_fade b20_b aparecer 1.0
  capa_fade b20_c aparecer 1.0
  capa_fade b20_d aparecer 1.0
  capa_fade b20_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_21

escena poema_21
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b20_a desaparecer 0.8
  capa_fade b20_b desaparecer 0.8
  capa_fade b20_c desaparecer 0.8
  capa_fade b20_d desaparecer 0.8
  capa_fade b20_e desaparecer 0.8
  esperar 0.8
  texto b21_a "Las cosas no tienen que durar para siempre" x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b21_b "para haber valido la pena." x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b21_c "Tampoco tienen que ser perfectas." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b21_d "A veces basta con que existan." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  capa_fade b21_a aparecer 1.0
  capa_fade b21_b aparecer 1.0
  capa_fade b21_c aparecer 1.0
  capa_fade b21_d aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_22

escena poema_22
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b21_a desaparecer 0.8
  capa_fade b21_b desaparecer 0.8
  capa_fade b21_c desaparecer 0.8
  capa_fade b21_d desaparecer 0.8
  esperar 0.8
  texto b22_a "Con que ocupen un lugar" x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b22_b "que antes estaba vacío." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b22_c "Con que demuestren" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b22_d "que alguien estuvo aquí" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b22_e "y decidió intentarlo." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b22_a aparecer 1.0
  capa_fade b22_b aparecer 1.0
  capa_fade b22_c aparecer 1.0
  capa_fade b22_d aparecer 1.0
  capa_fade b22_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_23

escena poema_23
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b22_a desaparecer 0.8
  capa_fade b22_b desaparecer 0.8
  capa_fade b22_c desaparecer 0.8
  capa_fade b22_d desaparecer 0.8
  capa_fade b22_e desaparecer 0.8
  esperar 0.8
  texto b23_a "Volví a mirar la distancia." x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b23_b "Seguía ahí." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b23_c "La imagen también." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b23_d "Todavía más lejos" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b23_e "de lo que habría querido." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b23_a aparecer 1.0
  capa_fade b23_b aparecer 1.0
  capa_fade b23_c aparecer 1.0
  capa_fade b23_d aparecer 1.0
  capa_fade b23_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_24

escena poema_24
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b23_a desaparecer 0.8
  capa_fade b23_b desaparecer 0.8
  capa_fade b23_c desaparecer 0.8
  capa_fade b23_d desaparecer 0.8
  capa_fade b23_e desaparecer 0.8
  esperar 0.8
  texto b24_a "Pero esta vez" x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b24_b "también pude ver" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b24_c "todo lo que había entre ambos puntos." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  capa_fade b24_a aparecer 1.0
  capa_fade b24_b aparecer 1.0
  capa_fade b24_c aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_25

escena poema_25
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b24_a desaparecer 0.8
  capa_fade b24_b desaparecer 0.8
  capa_fade b24_c desaparecer 0.8
  esperar 0.8
  texto b25_a "Y aquí quedó algo." x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b25_b "No todo." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b25_c "Pero algo." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b25_d "Algo que puedo dejar atrás" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b25_e "sin fingir que está terminado." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_visible b25_b no
  capa_visible b25_c no
  capa_visible b25_d no
  capa_visible b25_e no
  capa_fade b25_a aparecer 1.2
  esperar 1.2
  esperar 0.7
  capa_fade b25_b aparecer 1.0
  esperar 1.0
  esperar 0.6
  capa_fade b25_c aparecer 1.0
  esperar 1.0
  esperar 0.6
  capa_fade b25_d aparecer 1.0
  capa_fade b25_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_26

escena poema_26
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b25_a desaparecer 0.8
  capa_fade b25_b desaparecer 0.8
  capa_fade b25_c desaparecer 0.8
  capa_fade b25_d desaparecer 0.8
  capa_fade b25_e desaparecer 0.8
  esperar 0.8
  texto b26_a "Algo que puede seguir creciendo" x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b26_b "aunque algún día" x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b26_c "yo deje de hacerlo crecer." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  capa_fade b26_a aparecer 1.0
  capa_fade b26_b aparecer 1.0
  capa_fade b26_c aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_27

escena poema_27
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b26_a desaparecer 1.2
  capa_fade b26_b desaparecer 1.2
  capa_fade b26_c desaparecer 1.2
  esperar 1.2
  texto b27_a "..." x=560 y=500 color=#1a1a1a z=950 fuente=poema
  capa_fade b27_a aparecer 2.0
  esperar 2.0
  esperar 1.5
  esperar_click:
    "avanzar" -> poema_28

escena poema_28
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b27_a desaparecer 1.2
  esperar 1.2
  texto b28_a "¿Y si un día ya no estoy aquí?" x=560 y=500 color=#1a1a1a z=950 fuente=poema
  capa_fade b28_a aparecer 2.0
  esperar 2.0
  esperar 1.5
  esperar_click:
    "avanzar" -> poema_28b

escena poema_28b
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b28_a desaparecer 0.8
  esperar 0.8
  esperar 1.2
  texto b28b_a "Entonces alguien abrirá este archivo." x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b28b_b "Tal vez buscando otra cosa." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b28b_c "Tal vez por accidente." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b28b_d "Tal vez cuando todo esto" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b28b_e "ya pertenezca a otro tiempo." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_visible b28b_b no
  capa_visible b28b_c no
  capa_visible b28b_d no
  capa_visible b28b_e no
  capa_fade b28b_a aparecer 1.2
  esperar 1.2
  esperar 1.0
  capa_fade b28b_b aparecer 1.0
  capa_fade b28b_c aparecer 1.0
  capa_fade b28b_d aparecer 1.0
  capa_fade b28b_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_29

escena poema_29
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b28b_a desaparecer 0.8
  capa_fade b28b_b desaparecer 0.8
  capa_fade b28b_c desaparecer 0.8
  capa_fade b28b_d desaparecer 0.8
  capa_fade b28b_e desaparecer 0.8
  esperar 0.8
  texto b29_a "No sé quién eres." x=560 y=370 color=#1a1a1a z=950 fuente=poema
  texto b29_b "No sé cuánto ha pasado." x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b29_c "No sé qué existe" x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b29_d "mientras lees estas palabras." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b29_e "No sé si aquello que construí" x=560 y=590 color=#1a1a1a z=950 fuente=poema
  texto b29_f "todavía conserva mi nombre." x=560 y=645 color=#1a1a1a z=950 fuente=poema
  capa_fade b29_a aparecer 1.0
  capa_fade b29_b aparecer 1.0
  capa_fade b29_c aparecer 1.0
  capa_fade b29_d aparecer 1.0
  capa_fade b29_e aparecer 1.0
  capa_fade b29_f aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_30

escena poema_30
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b29_a desaparecer 0.8
  capa_fade b29_b desaparecer 0.8
  capa_fade b29_c desaparecer 0.8
  capa_fade b29_d desaparecer 0.8
  capa_fade b29_e desaparecer 0.8
  capa_fade b29_f desaparecer 0.8
  esperar 0.8
  texto b30_a "Quizá cambió." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b30_b "Quizá mejoró." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b30_c "Quizá solo quedan fragmentos." x=560 y=565 color=#1a1a1a z=950 fuente=poema
  capa_fade b30_a aparecer 1.0
  capa_fade b30_b aparecer 1.0
  capa_fade b30_c aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_31

escena poema_31
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b30_a desaparecer 0.8
  capa_fade b30_b desaparecer 0.8
  capa_fade b30_c desaparecer 0.8
  esperar 0.8
  texto b31_a "Pero llegaste hasta aquí." x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b31_b "Así que algo permaneció." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  capa_fade b31_a aparecer 1.5
  capa_fade b31_b aparecer 1.5
  esperar 1.5
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_32

escena poema_32
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b31_a desaparecer 0.8
  capa_fade b31_b desaparecer 0.8
  esperar 0.8
  texto b32_a "Y durante unos minutos" x=560 y=370 color=#1a1a1a z=950 fuente=poema
  texto b32_b "sabrás que detrás de todo esto" x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b32_c "hubo alguien que dudó," x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b32_d "que se cansó," x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b32_e "que quiso llegar más lejos," x=560 y=590 color=#1a1a1a z=950 fuente=poema
  texto b32_f "y que aun así continuó." x=560 y=645 color=#1a1a1a z=950 fuente=poema
  capa_fade b32_a aparecer 1.0
  capa_fade b32_b aparecer 1.0
  capa_fade b32_c aparecer 1.0
  capa_fade b32_d aparecer 1.0
  capa_fade b32_e aparecer 1.0
  capa_fade b32_f aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_33

escena poema_33
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b32_a desaparecer 0.8
  capa_fade b32_b desaparecer 0.8
  capa_fade b32_c desaparecer 0.8
  capa_fade b32_d desaparecer 0.8
  capa_fade b32_e desaparecer 0.8
  capa_fade b32_f desaparecer 0.8
  esperar 0.8
  texto b33_a "No sé qué harás después de cerrar este archivo." x=560 y=400 color=#1a1a1a z=950 fuente=poema
  texto b33_b "Tal vez nada." x=560 y=455 color=#1a1a1a z=950 fuente=poema
  texto b33_c "No importa." x=560 y=510 color=#1a1a1a z=950 fuente=poema
  texto b33_d "Ya ocurrió lo que alguna vez" x=560 y=565 color=#1a1a1a z=950 fuente=poema
  texto b33_e "me pregunté si ocurriría." x=560 y=620 color=#1a1a1a z=950 fuente=poema
  capa_fade b33_a aparecer 1.0
  capa_fade b33_b aparecer 1.0
  capa_fade b33_c aparecer 1.0
  capa_fade b33_d aparecer 1.0
  capa_fade b33_e aparecer 1.0
  esperar 1.0
  esperar_click:
    "avanzar" -> poema_34

escena poema_34
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b33_a desaparecer 0.8
  capa_fade b33_b desaparecer 0.8
  capa_fade b33_c desaparecer 0.8
  capa_fade b33_d desaparecer 0.8
  capa_fade b33_e desaparecer 0.8
  esperar 0.8
  texto b34_a "Alguien llegó." x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b34_b "Tú llegaste." x=560 y=535 color=#1a1a1a z=950 fuente=poema
  capa_visible b34_b no
  capa_fade b34_a aparecer 1.5
  esperar 1.5
  esperar 1.0
  capa_fade b34_b aparecer 2.0
  esperar 2.0
  esperar 1.5
  esperar_click:
    "avanzar" -> poema_35
  # Si abriste el archivo en vez de ejecutarlo:
  # Hola :3
escena poema_35
  hotspot avanzar x=0 y=0 w=1920 h=1080
  capa_fade b34_a desaparecer 0.8
  capa_fade b34_b desaparecer 0.8
  esperar 0.8
  texto b35_a "Y mientras estas palabras" x=560 y=425 color=#1a1a1a z=950 fuente=poema
  texto b35_b "todavía puedan encontrarte," x=560 y=480 color=#1a1a1a z=950 fuente=poema
  texto b35_c "una pequeña parte de mí" x=560 y=535 color=#1a1a1a z=950 fuente=poema
  texto b35_d "también estará aquí." x=560 y=590 color=#1a1a1a z=950 fuente=poema
  capa_visible b35_d no
  capa_fade b35_a aparecer 1.2
  capa_fade b35_b aparecer 1.2
  capa_fade b35_c aparecer 1.2
  esperar 1.2
  esperar 1.0
  capa_fade b35_d aparecer 1.8
  esperar 1.8
  esperar 1.5
  esperar_click:
    "avanzar" -> poema_fin

escena poema_fin
  capa_fade b35_a desaparecer 2.0
  capa_fade b35_b desaparecer 2.0
  capa_fade b35_c desaparecer 2.0
  capa_fade b35_d desaparecer 2.0
  esperar 2.0
  capa_fade blanco desaparecer 2.5
  esperar 2.5

# El poema termina y vuelve al menú del título (cargar_script cambia de guion).
cargar_script "menu_principal.ecl"

