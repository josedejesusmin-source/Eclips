-- Cámara del lugar: el cuarto de 22 x 12 llena el ancho, y el borde se ve.
-- Las marcas de "ya hable" vienen del paseo. Sin esto, Sol no volvería a hablar.

local ZOOM = 1920 / (22 * 60)
world.camera_zoom(ZOOM, 0)

world.set_var("_npc_S_talked", "0")
world.set_var("_npc_L_talked", "0")
world.set_var("_npc_E_talked", "0")
print("[LUGAR] inicio")
