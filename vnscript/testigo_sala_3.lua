-- testigo_sala_3.lua -- tramo 3: el dash. Torretas que APUNTAN.
--
-- `apunta = true`: el tiro sale hacia donde está el cubo en el instante del
-- disparo, y solo si lo tiene a menos de `alcance` px (desde el centro de la
-- torreta). Mapa de prueba 96 x 18:
--   - la del claro (col 50, fila 2) cuelga 606 px arriba del piso: con 600 no
--     veía a nadie que caminara, solo si saltaba. Con 900 ve el piso de las
--     cols 39-61, y no el hueco de práctica (cols 23-28) ni la boca del túnel.
--   - la del túnel (col 92, fila 12) ve el túnel y un poquito antes de la
--     boca (col 68): 1500 px. No ve el claro de atrás.
-- La salida (`>`) está en la columna 88, antes de la torreta.

local sala = dofile("vnscript/testigo_sala_comun.lua")

sala.tramo {
    numero   = 3,
    ritmo    = 1.2,
    entrada  = { 4, 12 },        -- la P, y el `posicion` de la puerta del tramo 2
    dash     = true,
    meta     = { 88, 12 },
    torretas = {
        { col = 50, fila = 2,  boca = "abajo",     cada = 1, fase = 0,
          apunta = true, alcance = 900 },
        { col = 92, fila = 12, boca = "izquierda", cada = 1, fase = 0.5,
          apunta = true, alcance = 1500 },
    },
}
