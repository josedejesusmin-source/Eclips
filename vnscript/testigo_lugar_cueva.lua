-- La cueva es más chica que el lugar (16 tiles de ancho), así que la cámara se
-- acerca un poco más, para que la copia se lea.

local ZOOM = 1920 / (16 * 60)
world.camera_zoom(ZOOM, 0)
print("[LUGAR] cueva")
