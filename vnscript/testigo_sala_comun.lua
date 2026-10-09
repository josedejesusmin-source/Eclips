-- testigo_sala_comun.lua -- lo que comparten los tres tramos de la sala de Sol.
--
-- Cada tramo (testigo_sala_1/2/3.lua) es casi puro DATO: dónde están sus
-- torretas, a qué ritmo, desde dónde vuelve el cubo. Todo lo demás vive acá,
-- una sola vez:
--
--   EL METRÓNOMO. Las torretas disparan a ritmo fijo: un
--   pulso cada `ritmo` segundos, y cada torreta dispara cada `cada` pulsos,
--   corrida `fase` pulsos. Dos clases:
--     - de línea fija: siempre hacia su `boca` (abajo, izquierda...);
--     - `apunta = true`: hacia donde está el cubo EN ESE INSTANTE, y solo si
--       lo tiene a menos de `alcance` px. Como en la demostración: "la
--       torreta apunta al señuelo".
--   EL AVISO. `AVISO` segundos antes de cada tiro aparece un cuadro claro en
--   la boca de la torreta: un proyectil de velocidad 0, sin daño y AMIGO (los
--   amigos no tocan al cubo), que se apaga solo por `tiempo_vida`.
--   SOL LO PARA ("-¿Y si le dan? -Lo paro."). Nadie muere en su test: al
--   primer golpe las torretas se callan, el cubo recupera la vida y el golpe
--   se cuenta. En la historia (el guion pone sala_historia = 1) ahí termina
--   la prueba y se vuelve al VN. Si la sala se juega suelta, Sol devuelve el
--   cubo a la entrada del tramo y el metrónomo arranca de cero.
--   LO QUE SE MIDE ("con un final que se pueda medir"). En variables del VN,
--   que sobreviven a las puertas (el Lua de cada tramo se reinicia al
--   cruzar):
--     sala_golpes            total de la sala
--     sala_golpes_1/2/3      por tramo
--     sala_completada        1 al llegar a la salida (tramo 3)
--
-- Se carga con dofile desde cada tramo. La ruta es relativa a la carpeta del
-- juego (donde está project.eclips), no a la del mapa.

local M = {}

M.HP = 3                 -- la vida real del cubo; se restaura en cada golpe
M.AVISO = 0.35           -- segundos de aviso antes de cada tiro
M.PRIMER_PULSO = 1.2     -- despues del fundido de entrada
M.RECOLOCAR = 0.35       -- cuanto tarda Sol en devolver al cubo
M.PAUSA = 1.6            -- cuanto se callan las torretas despues de un golpe
M.TILE = 60
M.CUBO = 54              -- `tamano` del jugador en los tres mapas

local function centroTile(col, fila)
    return col * M.TILE + M.TILE / 2, fila * M.TILE + M.TILE / 2
end

-- El punto justo afuera de la caja de la torreta, del lado de su boca.
local function boca(t, margen)
    local cx, cy = centroTile(t.col, t.fila)
    local h = M.TILE / 2 + margen
    if t.boca == "abajo" then return cx, cy + h, 0, 1 end
    if t.boca == "arriba" then return cx, cy - h, 0, -1 end
    if t.boca == "izquierda" then return cx - h, cy, -1, 0 end
    return cx + h, cy, 1, 0                       -- "derecha"
end

local function centroCubo()
    local x, y = world.get_player_pos()
    return x + M.CUBO / 2, y + M.CUBO / 2
end

-- cfg:
--   número     1, 2 o 3
--   ritmo      segundos por pulso
--   entrada    { col, fila }: a donde vuelve el cubo cuando Sol lo para
--   torretas   { { col, fila, boca, cada, fase, apunta?, alcance? }, ... }
--   meta       { col, fila } del `>` (solo el tramo que termina la sala)
--   tiro       nombre del proyectil de los tiros  (default "B")
--   aviso      nombre del proyectil del aviso     (default "A")
function M.tramo(cfg)
    local reloj, terminado, salida_en = 0, false, nil
    local parado, recolocar_en, reanudar_en = false, nil, nil
    local tiro, aviso = cfg.tiro or "B", cfg.aviso or "A"
    local tamTiro, tamAviso = 16, 24
    local n = cfg.numero

    if n == 1 then
        world.set_var("sala_golpes", "0")
        world.set_var("sala_completada", "0")
    end
    world.set_var("sala_golpes_" .. n, "0")
    local golpesTramo = 0

    local function programar(desde)
        for _, t in ipairs(cfg.torretas) do
            t.siguiente = desde + (t.fase or 0) * cfg.ritmo
            t.avisado = false
        end
    end
    programar(M.PRIMER_PULSO)
    print("[SALA] tramo " .. n .. " inicio; torretas=" .. #cfg.torretas
        .. " ritmo=" .. cfg.ritmo)

    local function loVe(t)
        if not t.apunta or not t.alcance then return true end
        local cx, cy = centroTile(t.col, t.fila)
        local px, py = centroCubo()
        local dx, dy = px - cx, py - cy
        return dx * dx + dy * dy <= t.alcance * t.alcance
    end

    local function disparar(t)
        local x, y, dx, dy = boca(t, tamTiro / 2 + 2)
        if t.apunta then
            local px, py = centroCubo()
            dx, dy = px - x, py - y
        end
        world.spawn_projectile(tiro, x, y, dx, dy, false)
        world.sound("assets/sonidos/pulso.wav")
    end

    local function avisar(t)
        local x, y = boca(t, tamAviso / 2 + 2)
        world.spawn_projectile(aviso, x, y, 1, 0, true)
    end

    world.on("player_damage", function(cantidad)
        -- Tocar una torreta (daño 0) también dispara este hook, con 0: eso no
        -- es un golpe.
        if terminado or parado or (cantidad or 0) <= 0 then return end
        world.set_player_hp(M.HP)                  -- antes del chequeo de muerte
        golpesTramo = golpesTramo + 1
        local total = (tonumber(world.get_var("sala_golpes")) or 0) + 1
        world.set_var("sala_golpes", tostring(total))
        world.set_var("sala_golpes_" .. n, tostring(golpesTramo))
        world.sound("assets/sonidos/impacto.wav")
        world.camera_shake(0.2)
        parado = true
        -- En la historia, un golpe detiene la prueba y vuelve al VN.
        -- Jugada suelta, Sol devuelve el cubo a la entrada del tramo.
        if world.get_var("sala_historia") == "1" then
            terminado = true
            salida_en = reloj + 1.2
            world.set_var("sala_fin", "golpe")
            world.set_var("sala_tramo", tostring(n))
            print("[SALA] golpe, la prueba se detiene")
        else
            recolocar_en = reloj + M.RECOLOCAR
            reanudar_en = reloj + M.PAUSA
            print("[SALA] golpe tramo=" .. n .. " (" .. golpesTramo .. ") total=" .. total)
        end
    end)

    world.on("action", function(verbo, dominio, ejecutado)
        if dominio ~= "juego" or not ejecutado then return end
        if verbo == "saltar" then world.sound("assets/sonidos/salto.wav") end
        -- El dash ya tiene su sonido en el motor; no se dispara dos veces.
    end)

    world.on("update", function(dt)
        if terminado then
            if salida_en and reloj >= salida_en then
                salida_en = nil
                world.exit()
            end
            reloj = reloj + dt
            return
        end
        reloj = reloj + dt

        if recolocar_en and reloj >= recolocar_en then
            recolocar_en = nil
            world.set_player_pos(cfg.entrada[1] * M.TILE, cfg.entrada[2] * M.TILE)
            world.set_player_hp(M.HP)
            print("[SALA] Sol lo paro: de vuelta a la entrada del tramo " .. n)
        end
        if reanudar_en and reloj >= reanudar_en then
            reanudar_en = nil
            parado = false
            programar(reloj + 0.4)
        end

        if cfg.meta then
            local x, y = world.get_player_pos()
            local mx, my = cfg.meta[1] * M.TILE, cfg.meta[2] * M.TILE
            if x + M.CUBO > mx and x < mx + M.TILE and y + M.CUBO > my and y < my + M.TILE then
                terminado = true
                salida_en = reloj + 0.45
                world.set_var("sala_completada", "1")
                world.set_var("sala_fin", "llegada")
                world.sound("assets/sonidos/llegada.wav")
                print("[SALA] llegada golpes=" .. world.get_var("sala_golpes"))
                return
            end
        end

        if parado then return end
        for _, t in ipairs(cfg.torretas) do
            if not t.avisado and reloj >= t.siguiente - M.AVISO then
                t.avisado = true
                if loVe(t) then avisar(t) end
            end
            if reloj >= t.siguiente then
                if loVe(t) then disparar(t) end
                t.siguiente = t.siguiente + (t.cada or 1) * cfg.ritmo
                t.avisado = false
            end
        end
    end)
end

return M
