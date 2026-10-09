-- testigo_sala_1.lua -- tramo 1: la gravedad. Nada dispara.
--
-- Solo datos; la mecánica común (Sol lo para, el conteo, los sonidos) vive
-- en testigo_sala_comun.lua. Este tramo además pone en cero los contadores
-- de la sala entera.

local sala = dofile("vnscript/testigo_sala_comun.lua")

sala.tramo {
    numero   = 1,
    ritmo    = 1.5,
    entrada  = { 2, 15 },        -- la P del mapa
    torretas = {},
}
