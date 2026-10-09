// ============================================================================
// ECLIPS ENGINE - miniaudio Implementation
// ============================================================================
// Este archivo compila miniaudio (header-only, como stb_image).
// Solo se incluye UNA VEZ con el #define IMPLEMENTATION.
//
// IMPORTANTE: Debes descargar miniaudio.h y colocarlo en third_party/
// Ver third_party/DOWNLOAD.md para instrucciones.
// ============================================================================

// ── Vorbis (.ogg) ───────────────────────────────────────────────────────────
// miniaudio trae pegamento de fábrica para stb_vorbis: con su header presente
// ANTES de la implementación de miniaudio, el .ogg queda habilitado en
// ma_decoder — y por ende en música-streaming Y efectos (ambos pasan por ahí).
// Header-only va antes; la implementación, después. Mismo TU.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
