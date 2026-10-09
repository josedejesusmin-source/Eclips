#pragma once
// ============================================================================
// ScenePatch.h — RT-1: el vocabulario de la ley del UPDATE.
//
// Nace del drenaje de `AUDITORIA_SEMANTICA_RUNTIME_VN.md` (R-RT-01, R-RT-02).
//
// El problema que resuelve, en una frase: un `float = 0` no distingue "el
// creador escribio cero" de "no escribio la clave", asi que en UPDATE los
// defaults de CREATE salian como ORDENES — un `capa placa "b.png"` sobre una
// capa ya colocada la mandaba a (0,0), z=0 y tamano 0x0 (invisible: `elem->w/h`
// van crudos al draw), y le borraba el tinte. La misma linea sobre una capa
// ausente y una ya configurada NO significaba lo mismo.
//
// POR QUE ESTE HEADER EXISTE, y no vive en uno de los dos lados:
//   - `EclipScript.h` (parser) es quien SABE que campos nombro la linea.
//   - `SceneLayer.h` (render) es quien POSEE el estado que se pisa.
//   - Que el render incluya al parser seria invertir la dependencia.
// Asi que el vocabulario compartido vive aparte, sin deps: `Types.h`,
// `<optional>` y `<string>`. Los dos lo incluyen, ninguno depende del otro.
//
// Y hay una razon de INSTRUMENTO, que es la que decidio la forma: los dos
// extremos quedan alcanzables por un harness standalone (medido con
// `Resumenes_Sesiones/Harness/sonda_alcance_scenelayer.cpp`), mientras que
// `VNScriptState.h` — donde vive el dispatch — arrastra SDL/Lua/audio y no lo
// es. Con el patch en el medio, la LEY se puede probar aunque el dispatch no.
//
// `std::optional` aca si y en `EclipCmd` no (fork A de la mesa): esto es un
// struct TRANSITORIO entre parser y render, no un miembro que el codec T4/E4
// serialice. La objecion del fork A era sobre tocar tipos que el save roza.
//
// Un optional VACIO significa "la linea no lo nombro" -> se conserva.
// Un optional CON valor significa "la linea lo nombro" -> se escribe, aunque
// el valor sea cero. Eso ultimo es lo que arregla `parallax=0`, que hasta hoy
// no podia apagar el parallax porque la ausencia se codificaba como 0.
// ============================================================================

#include "engine/core/Types.h"
#include "engine/core/TextStroke.h"

#include <optional>
#include <string>
#include <vector>

namespace eclips
{
    // ── Lo que un `capa <id> "ruta" ...` pide cambiar ───────────────────────
    // La ruta/textura NO viaja aca: es el argumento REQUERIDO del verbo (SYN-2
    // ya lo hace fail-closed), asi que siempre esta presente y va por separado.
    struct LayerPatch
    {
        std::optional<f32> x, y, w, h;
        std::optional<i32> z;
        std::optional<f32> parallax;

        // El tinte viaja ya parseado (el hex crudo se valida en parse-time).
        // hex == "" con el optional ENGANCHADO significa "quitar el tinte";
        // el optional vacio significa "no dije nada del tinte".
        std::optional<std::string> tintHex;
        f32 tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;
    };

    // ── Lo que un `texto <id> "contenido" ...` pide cambiar ─────────────────
    // El contenido tampoco viaja como optional: es el argumento requerido.
    struct TextPatch
    {
        std::optional<f32> x, y;
        std::optional<i32> z;

        // Color del texto, ya resuelto (inline pisa estilo).
        struct Rgb { f32 r = 1.0f, g = 1.0f, b = 1.0f; };
        std::optional<Rgb> color;

        std::optional<std::string> fontName;

        // El glow es UN concepto para el creador ("puse color -> veo glow"), y
        // sus tres campos ya se resuelven juntos con defaults utiles. Viaja
        // como un bloque: o lo nombraste (inline o via estilo) o no.
        struct Glow { f32 r = 1.0f, g = 1.0f, b = 1.0f; f32 radius = 0.0f, intensity = 0.0f; };
        std::optional<Glow> glow;

        // El borde viaja igual que el glow y por la misma razon: para el
        // creador es UN concepto ("puse grosor -> veo borde"), asi que o lo
        // nombraste (inline o via estilo) o no. Lleva alpha propio porque
        // `#RRGGBBAA` es una forma valida del color y un borde semi-transparente
        // es util (el glow resuelve su alpha por `intensity`, el borde no tiene
        // ese canal).
        struct Borde
        {
            f32 r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
            f32 grosor = 0.0f;
            std::vector<ResolvedTextStroke> strokes;
        };
        std::optional<Borde> borde;

        // La variante de hover solo puede venir de un estilo (no hay override
        // inline). Vacia = la linea no trajo estilo con hover -> se conserva.
        struct Hover
        {
            Rgb   text;
            Glow  glow;
            Borde borde;
        };
        std::optional<Hover> hover;
    };
}
