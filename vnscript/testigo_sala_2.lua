-- testigo_sala_2.lua -- tramo 2: el ritmo. Torretas de LÍNEA FIJA.
--
-- Un pulso cada `ritmo` segundos. `cada = 2` = dispara un pulso sí y uno no;
-- `fase` corre esa torreta N pulsos. Mapa de prueba 96 x 18:
--   pulso:         1   2   3   4   5   6 ...
--   gotera col 18  x       x       x          la primera, sola
--   gotera col 52  x       x       x          \
--   gotera col 60      x       x       x      / alternadas
--   gotera col 80  x       x       x          sobre el pasillo
--   ras del suelo      x       x       x      contratiempo con la de arriba
-- Las columnas son las del mapa (tile de 60 px).

local sala = dofile("vnscript/testigo_sala_comun.lua")

sala.tramo {
    numero   = 2,
    ritmo    = 1.5,
    entrada  = { 3, 10 },        -- la P, y el `posicion` de la puerta del tramo 1
    torretas = {
        { col = 18, fila = 2,  boca = "abajo",     cada = 2, fase = 0 },
        { col = 52, fila = 2,  boca = "abajo",     cada = 2, fase = 0 },
        { col = 60, fila = 2,  boca = "abajo",     cada = 2, fase = 1 },
        { col = 80, fila = 2,  boca = "abajo",     cada = 2, fase = 0 },
        { col = 92, fila = 14, boca = "izquierda", cada = 2, fase = 1 },
    },
}
