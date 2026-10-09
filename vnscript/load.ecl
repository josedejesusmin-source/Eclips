# load.ecl - la pantalla de "Cargar partida" de la demo.
# Sustituye la del motor: se registra en project.eclips (pantallas { cargar ... }).
# Clic en una ranura ocupada carga esa partida; una vacía no hace nada; "Volver"
# cierra esta pantalla.

escena cargar
capa fondo "#101018"
ocultar_caja

texto titulo "Cargar partida" x=140 y=70 z=10

ranura 1 accion=cargar x=140 y=150 ancho=460
ranura 2 accion=cargar x=140 y=240 ancho=460
ranura 3 accion=cargar x=140 y=330 ancho=460
ranura 4 accion=cargar x=140 y=420 ancho=460
ranura 5 accion=cargar x=140 y=510 ancho=460

elegir:
    "Volver" -> fin