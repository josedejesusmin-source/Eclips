-- testigo_paseo.lua -- lo que el .eclmap solo no sabe hacer.
--
-- 1. EL ORDEN. El motor avisa QUE hablaste con alguien (escribe
--    `_npc_<letra>_talked = "1"` en las variables del VN al cerrar la charla),
--    pero no CUÁNDO: no hay hook de "hable". Se pregunta en cada frame y se
--    anota el orden de llegada.
-- 2. LA SALIDA. `salida: lua` y no `hablar_todos`: con `hablar_todos` el nivel
--    se cierra en el mismo instante en que termina la tercera charla y este
--    script no alcanza a anotar a la tercera. Acá se anota, se deja respirar un
--    segundo al mundo (vuelve del negro de la charla) y se sale.
-- 3. EL SUELO. La posición del cubo se anota una vez, al salir, y no en cada
--    paso: cada set_var deja una línea en el log del motor, y llamarlo en
--    cada frame vuelve lento el juego.

local NPCS = {
    { letra = "S", nombre = "sol" },
    { letra = "L", nombre = "luna" },
    { letra = "E", nombre = "estrella" },
}
local PAUSA_ANTES_DE_SALIR = 1.0

-- LA CÁMARA, MÁS CERCA. El cuarto mide 23 tiles de 60 = 1380 px de ancho; con
-- este zoom ocupa los 1920 de la pantalla justo (y los 13 de alto, 780 px,
-- quedan a 3 px de caber: la cámara casi no se mueve). Más cerca = un número
-- mayor; 1.0 = sin zoom, y el cuarto flota en el fondo verde.
-- testigo_copias.lua usa el mismo: es el mismo suelo.
local ZOOM = 1920 / (23 * 60)
world.camera_zoom(ZOOM, 0)

local reloj, salida_en, terminado = 0, nil, false
local vistos, cuantos = {}, 0
local ultimo_x, ultimo_y = nil, nil

-- Las marcas `_npc_*` viven en las variables del VN y SOBREVIVEN al nivel:
-- sin esto, la segunda vuelta del paseo (o las copias, que usan las mismas
-- letras) arrancaría con las tres ya "habladas".
for _, n in ipairs(NPCS) do
    world.set_var("_npc_" .. n.letra .. "_talked", "0")
end
for i = 1, #NPCS do
    world.set_var("paseo_orden_" .. i, "-")
end
world.set_var("paseo_eleccion", "-")
print("[PASEO] inicio")

world.on("action", function(verbo, dominio, ejecutado)
    if dominio == "juego" and verbo == "saltar" and ejecutado then
        world.sound("assets/sonidos/salto.wav")
    end
end)

world.on("update", function(dt)
    if terminado then return end
    reloj = reloj + dt

    -- La posición se escribe una sola vez, al salir (ver el punto 3 de arriba).
    local x, y = world.get_player_pos()
    ultimo_x, ultimo_y = math.floor(x), math.floor(y)

    for _, n in ipairs(NPCS) do
        if not vistos[n.letra] and
           world.get_var("_npc_" .. n.letra .. "_talked") == "1" then
            vistos[n.letra] = true
            cuantos = cuantos + 1
            world.set_var("paseo_orden_" .. cuantos, n.nombre)
            print("[PASEO] hablo con " .. n.nombre .. " (" .. cuantos .. ")"
                .. " eleccion=" .. world.get_var("paseo_eleccion"))
        end
    end

    if cuantos == #NPCS and not salida_en then
        salida_en = reloj + PAUSA_ANTES_DE_SALIR
    end
    if salida_en and reloj >= salida_en then
        terminado = true
        world.set_var("cubo_x", tostring(ultimo_x))
        world.set_var("cubo_y", tostring(ultimo_y))
        print("[PASEO] fin orden=" .. world.get_var("paseo_orden_1") .. ","
            .. world.get_var("paseo_orden_2") .. ","
            .. world.get_var("paseo_orden_3")
            .. " eleccion=" .. world.get_var("paseo_eleccion"))
        world.exit()
    end
end)
