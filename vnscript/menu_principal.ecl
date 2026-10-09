# menu_principal.ecl - el menú del título de la demo. Se abre desde la línea
# "menu" de project.eclips. El diseño (posición, tamaños y colores) es mío.

escena menu

# Las tres fuentes del menú: M PLUS Rounded Bold (título), Nunito (subtítulo) y
# Hachi Maru Pop (la versión, en letra chica). Las tres son OFL.
fuente titulo_mc    "third_party/fonts/m_plus_rounded_1c/MPLUSRounded1c-Bold.ttf" tamano=128
fuente subtitulo_mc "third_party/fonts/nunito/Nunito-Regular.ttf"                tamano=28
fuente ver          "third_party/fonts/hachi_maru_pop/HachiMaruPop-Regular.ttf"  tamano=22

# El titulo lleva una compensacion optica leve hacia la derecha (x=805.8).
# El subtitulo conserva su posicion, ya cercana al centro horizontal (960).
# La version queda a 45 px del borde derecho e inferior. Si cambias las fuentes
# o sus tamaños, vuelve a revisar las posiciones.
# Las opciones del "elegir" las centra el motor.
texto logo "ECLIPS" x=805.8 y=320 color=#FFFFFF z=10 fuente=titulo_mc
texto sub  "Technical Preview" x=876 y=450 color=#8A8FA8 z=10 fuente=subtitulo_mc

# Versión: coordenadas fijas; ocultamos el texto antes del fundido de entrada.
texto ver "${_version_motor}" x=1824 y=1015 color=#C0C0CC z=10 fuente=ver
capa_visible ver no
capa_fade ver aparecer 2.5

# "Jugar la demo" usa "nuevo_juego": lanza el script_inicial de project.eclips
# como juego, encima de este menú, y por eso tiene pausa (Esc) y guardado.
# Con "cargar_script" la demo corría DENTRO del menú y se quedaba sin las dos.
# Un "elegir" a secas: sólo opciones, y el motor las centra.
# Ojo: elegir "¿Qué hacemos?": NO muestra la pregunta; el motor ignora ese
# texto sin avisar. Para una pregunta encima, escríbela como línea propia antes
# del elegir (manual: «Las opciones en el MISMO renglón que la línea»).
elegir:
    "Jugar la demo" -> nuevo_juego
    "Cargar partida" -> cargar
    "Configuración" -> config
    "Salir" -> confirm_salir
    # Una salida que no se ve: el código Konami, tecla por tecla.
    # Mientras la tecleas, las flechas siguen moviendo el resaltado;
    # sólo la última tecla (la A) se la queda la secuencia.
    secuencia ARRIBA ARRIBA ABAJO ABAJO IZQUIERDA DERECHA IZQUIERDA DERECHA B A -> poema

escena poema
# La secuencia salta a una escena, no a un archivo: el cambio de guion
# lo hace cargar_script, como cualquier otro.
cargar_script "para_quien_lo_lea.ecl"

escena confirm_salir
elegir:
    "Sí" -> salir
    "No" -> menu