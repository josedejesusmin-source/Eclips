// ============================================================================
// ECLIPS ENGINE - Tipos fundamentales
// ============================================================================
// Definiciones base que usa todo el motor.
// Aquí viven los tipos, constantes, y aliases que se usan en todas partes.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>

namespace eclips
{
    // ── Aliases de tipos para claridad ──
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using i32 = int32_t;
    using f32 = float;
    using f64 = double;

    // ── Constantes del motor ──
    namespace constants
    {
        // Fixed Timestep: 60 updates por segundo (ver doc 10.5)
        constexpr f64 FIXED_TIMESTEP = 1.0 / 60.0;
        constexpr i32 MAX_UPDATES_PER_FRAME = 5; // Evitar espiral de la muerte

        // Resolución virtual de diseño (ver doc 10.7)
        // El creador diseña para esta resolución.
        // El motor escala automáticamente a la pantalla real.
        constexpr i32 VIRTUAL_WIDTH  = 1920;
        constexpr i32 VIRTUAL_HEIGHT = 1080;

        // Versión del motor
        constexpr const char* ENGINE_VERSION = "0.9.0";
        constexpr const char* ENGINE_ETIQUETA = "Technical Preview";
        constexpr const char* ENGINE_NAME = "Eclips Engine";
    }

    // ── Resolución de diseño en RUNTIME (A1) ──
    // VIRTUAL_WIDTH/HEIGHT (arriba) son el DEFAULT semántico, constexpr.
    // Estos valores los siembra Engine desde proyecto.eclips (resolucion_diseno)
    // en init() y en cada reloadManifest() (F8). TODOS los call-sites de render
    // leen de viewport::width()/height() en vez de la constante.
    // Mismo contrato que draw2d::g_batch: Engine es el ÚNICO que escribe.
    // (El default va literal 1920/1080 a propósito → el find/replace de la
    //  Pasada 2 no tropieza con esta línea.)
    namespace viewport
    {
        inline i32 g_designW = 1920;
        inline i32 g_designH = 1080;
        inline i32 width() { return g_designW; }
        inline i32 height() { return g_designH; }
        inline void set(i32 w, i32 h) { g_designW = w; g_designH = h; }
    }

    // ── Posiciones predefinidas para personajes VN ──

    // ── Posiciones predefinidas para personajes VN ──
    // Usado por SpriteSheet, VNScriptState, EclipScript, etc.
    // Definido aqui para evitar dependencias circulares.
    enum class VNPosition
    {
        Izquierda,    // ~18% del ancho virtual
        CentroIzq,    // ~33%
        Centro,       // ~50%
        CentroDer,    // ~67%
        Derecha       // ~82%
    };

    // ── Modo de ajuste de una textura en una caja (fit) ──
    // Vocabulario compartido: lo USA draw2d (drawTextureFit_noBatch), lo GUARDA
    // Theme (slotThumbFit), lo PASA VNScriptState. Vive aqui (no en Draw2D) para
    // que Theme no arrastre SDL/GL — mismo motivo que VNPosition arriba.
    enum class TextureFit { Estirar, Contener, Cubrir };

} // namespace eclips
