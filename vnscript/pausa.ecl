# pausa.ecl - la pausa de la demo: guardar y cargar sobre la partida en curso.
# Sustituye la pausa del motor (project.eclips → pantallas { pausa ... }) y se
# abre con Esc durante el juego.
# Las ranuras se numeran por grupo: "Guardar" y "Cargar" tienen cada uno su 1..2.
# "Seguir" (-> fin) cierra la pausa y vuelve al juego; "Menú principal" (-> titulo)
# va al título; "Salir" cierra el juego.
# Esc abre esta pausa sólo si el juego entró con nuevo_juego (así lo lanza
# menu_principal.ecl). Un guion abierto con cargar_script corre como si fuera el
# menú, y ahí Esc no abre nada.

escena pausa
capa fondo "#101018"
ocultar_caja
texto titulo "Pausa" x=140 y=40 z=10

texto lblg "Guardar" x=140 y=110 z=10
ranura 1 accion=guardar x=140 y=140 ancho=460
ranura 2 accion=guardar x=140 y=230 ancho=460

texto lblc "Cargar" x=140 y=340 z=10
ranura 1 accion=cargar x=140 y=370 ancho=460
ranura 2 accion=cargar x=140 y=460 ancho=460

elegir:
    "Seguir" -> fin
    "Configuración" -> config
    "Menú principal" -> titulo
    "Salir" -> salir