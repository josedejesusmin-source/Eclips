-- El mismo encuadre que la primera visita. Las marcas de charla se
-- borran: si no, el paseo (o la primera vuelta) las dejaría en "1"
-- y esto se cerraría al entrar. La posición no se escribe al caminar.

local ZOOM = 1920 / (22 * 60)
world.camera_zoom(ZOOM, 0)

local NPCS = { "S", "L", "E" }
local PAUSA = 1.0
local reloj, salida_en, terminado, cuantos = 0, nil, false, 0
local vistos = {}

for _, letra in ipairs(NPCS) do
    world.set_var("_npc_" .. letra .. "_talked", "0")
end
print("[LUGAR] despues")

world.on("update", function(dt)
    if terminado then return end
    reloj = reloj + dt
    for _, letra in ipairs(NPCS) do
        if not vistos[letra] and world.get_var("_npc_" .. letra .. "_talked") == "1" then
            vistos[letra] = true
            cuantos = cuantos + 1
            print("[LUGAR] hablo " .. letra .. " (" .. cuantos .. ")")
        end
    end
    if cuantos == #NPCS and not salida_en then
        salida_en = reloj + PAUSA
    end
    if salida_en and reloj >= salida_en then
        terminado = true
        print("[LUGAR] fin")
        world.exit()
    end
end)
