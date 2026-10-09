# config.ecl — la pantalla de Configuración de la demo, hecha con un .ecl y sin
# tocar C++.
#
# Cómo se conecta:
# 1) project.eclips la registra: pantallas { config "config.ecl" }.
# 2) Cualquier opción que lleve a "config" (en el menú o en la pausa) la abre
#    encima de lo que estabas viendo.
# 3) Cada "control <nombre>" es un control del motor (barra, casilla o botones)
#    ya conectado a los ajustes del jugador: lo que se mueve se guarda solo.
# 4) "Volver" -> fin la cierra, y vuelve a verse lo de abajo.
#
# El fondo es liso y no hay imágenes a propósito: aquí importan los controles.

escena config

capa fondo "#101018"
ocultar_caja

texto titulo "Configuración" x=140 y=60 z=10

# El nombre después de "control" dice qué ajuste mueve. La lista completa está
# en el manual: «control — widget de setting».
texto l_mus "Música" x=140 y=180 z=10
control musica_volumen    x=140 y=210 ancho=520

texto l_sfx "Efectos" x=140 y=300 z=10
control sonido_volumen    x=140 y=330 ancho=520

texto l_txt "Velocidad del diálogo" x=140 y=420 z=10
control velocidad_texto   x=140 y=450 ancho=520

texto l_fs "Pantalla completa" x=140 y=540 z=10
control pantalla_completa x=140 y=570

texto l_skip "Skip no visto" x=700 y=540 z=10
control skip_no_visto     x=700 y=570

texto l_res "Resolución" x=700 y=180 z=10
# tipo=cajas: botones W×H (alternativa al default de flechas del catálogo).
control resolucion x=700 y=210 ancho=520 tipo=cajas

texto l_idioma "Idioma" x=700 y=360 z=10
control idioma x=700 y=390 ancho=520 tipo=cajas

# Mientras el elegir espera, los controles de arriba siguen funcionando.
elegir:
    "Volver" -> fin