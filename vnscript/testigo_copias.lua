-- testigo_copias.lua -- las tandas, el conteo y el mismo suelo.
--
-- Tandas de 1, 2 y 3: seis en total ("Acaba de pelear contra seis cosas
-- que se le parecían"). La siguiente entra cuando no queda ninguna viva, tras
-- una pausa. Al caer la sexta: "(La última copia se descarta.) (El cuadro de
-- ataque desaparece.) (El cubo negro permanece en el mismo suelo.) (Pausa.)"
-- -- se deja la pausa y se sale.
--
-- Vista cenital: entran por los bordes del cuarto (23 x 13), a la altura de
-- la fila del cubo (fila 6) por los costados, y por abajo por la columna del
-- centro (11). Posiciones en píxeles, tile de 60: columna 1 = x 60 (junto a
-- la pared izquierda), columna 21 = x 1260 (junto a la derecha), fila 11 =
-- y 660 (junto a la pared de abajo, debajo del checkpoint).

local IZQ   = { x = 60,   y = 360 }
local DER   = { x = 1260, y = 360 }
local ABAJO = { x = 660,  y = 660 }

-- La misma cámara que el paseo (ver testigo_paseo.lua): es el mismo suelo.
local ZOOM = 1920 / (23 * 60)
world.camera_zoom(ZOOM, 0)
local PAUSA_PRIMERA = 1.2        -- despues del fundido de entrada
local PAUSA_ENTRE_TANDAS = 1.6
local PAUSA_FINAL = 1.8

local NPCS = {
    { letra = "S", nombre = "sol" },
    { letra = "L", nombre = "luna" },
    { letra = "E", nombre = "estrella" },
}

local reloj, terminado = 0, false
local tanda, proxima_en, salida_en = 0, PAUSA_PRIMERA, nil
local eliminadas, golpes = 0, 0
local hablo = {}

for _, n in ipairs(NPCS) do
    world.set_var("_npc_" .. n.letra .. "_talked", "0")
    world.set_var("copias_hablo_" .. n.nombre, "0")
end
world.set_var("copias_eliminadas", "0")
world.set_var("copias_golpes", "0")

-- El mismo suelo: si el paseo dejó al cubo en algún lado, arranca ahí.
local cx = tonumber(world.get_var("cubo_x"))
local cy = tonumber(world.get_var("cubo_y"))
if cx and cy then
    world.set_player_pos(cx, cy)
end
print("[COPIAS] inicio cubo=" .. tostring(cx) .. "," .. tostring(cy))

-- La primera sale del lado contrario al cubo, para que "se acerque".
local function ladoLejano()
    local x = world.get_player_pos()
    if x < 690 then return DER end   -- 690 = centro del cuarto (columna 11)
    return IZQ
end

local function sacar(p)
    world.spawn("C", 1, p.x, p.y)
end

local function sacarTanda(n)
    if n == 1 then
        sacar(ladoLejano())
    elseif n == 2 then
        sacar(IZQ)
        sacar(DER)
    else
        sacar(IZQ)
        sacar(DER)
        sacar(ABAJO)
    end
    print("[COPIAS] tanda " .. n)
end

world.on("enemy_death", function(nombre, id)
    if nombre ~= "Copia" then return end
    eliminadas = eliminadas + 1
    world.set_var("copias_eliminadas", tostring(eliminadas))
    world.sound("assets/sonidos/rotura.wav")
    print("[COPIAS] eliminada " .. eliminadas)
end)

world.on("player_damage", function(cantidad)
    golpes = golpes + 1
    world.set_var("copias_golpes", tostring(golpes))
    world.sound("assets/sonidos/impacto.wav")
    print("[COPIAS] golpe " .. golpes .. " hp=" .. world.get_player_hp())
end)

world.on("action", function(verbo, dominio, ejecutado)
    if dominio == "juego" and verbo == "saltar" and ejecutado then
        world.sound("assets/sonidos/salto.wav")
    end
end)

world.on("update", function(dt)
    if terminado then return end
    reloj = reloj + dt

    -- Una vez por charla: cada `set_var` imprime una línea en el log del motor.
    for _, n in ipairs(NPCS) do
        if not hablo[n.letra] and
           world.get_var("_npc_" .. n.letra .. "_talked") == "1" then
            hablo[n.letra] = true
            world.set_var("copias_hablo_" .. n.nombre, "1")
            print("[COPIAS] hablo con " .. n.nombre)
        end
    end

    if salida_en then
        if reloj >= salida_en then
            terminado = true
            print("[COPIAS] fin eliminadas=" .. eliminadas .. " golpes=" .. golpes)
            world.exit()
        end
        return
    end

    if proxima_en then
        if reloj >= proxima_en then
            tanda = tanda + 1
            sacarTanda(tanda)
            proxima_en = nil
        end
        return
    end

    if world.enemy_count() == 0 then
        if tanda >= 3 then
            if not salida_en then
                world.set_var("copias_hp", tostring(world.get_player_hp()))
            end
            salida_en = reloj + PAUSA_FINAL
        else
            proxima_en = reloj + PAUSA_ENTRE_TANDAS
        end
    end
end)
