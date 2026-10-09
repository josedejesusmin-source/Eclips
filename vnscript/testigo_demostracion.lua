-- testigo_demostracion.lua -- la demostración de Sol, paso por paso.
--
-- El guion (escena "Descrita por Sol"):
--   2. presentación del espacio ....... (pausa)
--   3. primer disparo ................ el señuelo se desplaza ligeramente
--                                      y vuelve a quedar inmovil
--   4. segundo disparo ............... reacción MAYOR: se desplaza un poco
--                                      más y se queda en su nueva posición
--   5. final ......................... (pausa ligeramente más larga)
--   6. regreso a VN
-- Exactamente dos disparos; el cubo no recibe nada.
--
-- EL PRIMER IMPACTO es el del motor, gratis: destello de 0.15 s y un empujón de
-- 200 px/s que se frena solo -- unos 25 px.
-- EL SEGUNDO no puede ser mayor por sí solo (el empujón está clavado en 200),
-- así que se le suma desde acá: un deslizamiento extra, la sacudida de cámara y
-- el sonido.
--
-- LA ALTURA DEL TIRO NO ES ESTÉTICA. El empujón sale en la dirección
-- caja->caja, medida desde las esquinas de arriba. Si el proyectil no viaja con
-- su borde de arriba alineado al del señuelo, el empujón lleva componente
-- vertical, y en side-scroll los enemigos no tienen gravedad: el señuelo
-- quedaría flotando. Por eso el tiro va a la altura del borde superior, y además
-- cada frame se le devuelve la y al suelo.

local SENUELO = "Señuelo"
local Y_SUELO = 900                 -- fila 15: la caja del senuelo va de 900 a 960
local TAM_TIRO = 16                 -- el `tamano` del proyectil B
local TIRO_X = 240 + TAM_TIRO / 2 + 2    -- borde derecho de la torreta (col 3) + medio tiro
local TIRO_Y = Y_SUELO + TAM_TIRO / 2    -- borde de arriba del tiro = borde de arriba del senuelo

local ESPERA_PRIMER_TIRO = 2.2      -- 2. presentacion + (pausa breve) de 3.
local ESPERA_SEGUNDO_TIRO = 2.6     -- "deja de disparar durante un instante" + (pausa) de 4.
local DESLIZ_EXTRA = 60             -- lo que el segundo agrega al empujon del motor
local DESLIZ_DURACION = 0.45
local ESPERA_FINAL = 2.8            -- 5. (pausa ligeramente más larga)
local SIN_IMPACTO = 3.0             -- si un tiro no pega en este tiempo, se sigue igual

local reloj, terminado = 0, false
local fase, en = "presentacion", ESPERA_PRIMER_TIRO
local x_antes, disparado_en = nil, nil
local desliz_desde, desliz_inicio = nil, nil

world.set_var("demo_sol_vista", "0")
print("[DEMO] inicio")

local function senueloX()
    local x = world.get_enemy_pos(SENUELO)
    return x
end

local function disparar(n)
    x_antes = senueloX()
    disparado_en = reloj
    world.spawn_projectile("B", TIRO_X, TIRO_Y, 1, 0, true)
    world.sound("assets/sonidos/pulso.wav")
    print("[DEMO] tiro " .. n .. " senuelo_x=" .. tostring(x_antes))
end

local function pego()
    local x = senueloX()
    return x and x_antes and x > x_antes + 0.5
end

local function suavizar(t)          -- sale rapido y frena
    return 1 - (1 - t) * (1 - t)
end

world.on("update", function(dt)
    if terminado then return end
    reloj = reloj + dt

    -- El señuelo nunca deja el suelo (ver cabecera).
    local sx, sy = world.get_enemy_pos(SENUELO)
    if sy and sy >= 0 and math.abs(sy - Y_SUELO) > 0.01 then
        world.move_enemy(SENUELO, sx, Y_SUELO)
    end

    if fase == "presentacion" and reloj >= en then
        disparar(1)
        fase = "vuela1"

    elseif fase == "vuela1" then
        if pego() or reloj - disparado_en > SIN_IMPACTO then
            world.sound("assets/sonidos/impacto.wav")
            print("[DEMO] impacto 1 senuelo_x=" .. tostring(senueloX()))
            fase, en = "instante", reloj + ESPERA_SEGUNDO_TIRO
        end

    elseif fase == "instante" and reloj >= en then
        disparar(2)
        fase = "vuela2"

    elseif fase == "vuela2" then
        if pego() or reloj - disparado_en > SIN_IMPACTO then
            world.sound("assets/sonidos/impacto.wav")
            world.camera_shake(0.25)
            desliz_desde, desliz_inicio = senueloX(), reloj
            print("[DEMO] impacto 2 senuelo_x=" .. tostring(desliz_desde))
            fase = "desliz"
        end

    elseif fase == "desliz" then
        local p = math.min(1, (reloj - desliz_inicio) / DESLIZ_DURACION)
        world.move_enemy(SENUELO, desliz_desde + DESLIZ_EXTRA * suavizar(p), Y_SUELO)
        if p >= 1 then
            local qx, qy = world.get_enemy_pos(SENUELO)
            print("[DEMO] quieto senuelo_x=" .. tostring(qx) .. " y=" .. tostring(qy))
            fase, en = "final", reloj + ESPERA_FINAL
        end

    elseif fase == "final" and reloj >= en then
        terminado = true
        world.set_var("demo_sol_vista", "1")
        print("[DEMO] fin")
        world.exit()
    end
end)
