# third_party/

Librerías externas de un solo archivo, que ya vienen en este árbol:

- stb_image.h, stb_image_write.h, stb_truetype.h y stb_vorbis.c (imágenes, fuentes y audio OGG)
- miniaudio.h (audio)
- fonts/ — las fuentes OFL (ver `THIRD_PARTY_NOTICES.md`)

SDL2, Lua, sol2 y glad no vienen aquí: vcpkg los baja al compilar (ver `vcpkg.json`).
