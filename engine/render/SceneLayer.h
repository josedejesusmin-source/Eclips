// ============================================================================
// SceneLayer.h — Sistema de capas Z-Order para Eclips Engine
// ============================================================================
//
// Organiza elementos de una escena en capas con z-order dinamico.
// Cada elemento tiene un nombre, posicion, z-order y una textura o
// spritesheet asociada. Al renderizar, se dibujan de menor a mayor z
// (z bajo = atras, z alto = frente).
//
// Uso basico:
//
//   SceneLayerManager scene;
//
//   scene.addLayer("fondo",   &texFondo,   0,   0, 0, 1920, 1080);
//   scene.addLayer("mesa",    &texMesa,    100, 200, 600, 1520, 400);
//   scene.addLayer("pizarra", &texPiz,     50,  600, 300, 720, 500);
//   scene.addLayer("luna",    &texMon,     300, 700, 100, 520, 800);
//
//   // Luna saca el pizarron:
//   scene.setZOrder("pizarra", 250);  // Ahora esta arriba de mesa (100)
//                                      // pero debajo de Luna (300)
//
//   // En render:
//   scene.render();
//
// Tambien soporta capas animadas (SpriteSheet):
//
//   scene.addAnimatedLayer("Luna", &spriteMonika, 300, x, y, w, h);
//   scene.update(dt);  // actualiza animaciones
//
// ============================================================================

#pragma once

#include "engine/render/Texture.h"
#include "engine/render/Draw2D.h"
#include "engine/render/SpriteSheet.h"
#include "engine/render/TweenSystem.h"   // C-CP1: tabla de curvas SSOT (hoja sin deps)
#include "engine/render/TextRenderer.h"
#include "engine/render/FontManager.h"
#include "engine/core/Types.h"
#include "engine/core/ScenePatch.h"   // RT-1: el vocabulario de la ley del UPDATE
#include "engine/core/TextBudget.h"   // CP-R1: reloj/prefijo UTF-8 de consola

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <functional>   // E-CP2: hook de efectos de render por capa

namespace eclips
{
    // ── Datos de una consola (CP4 de la cinemática) ─────────────────────────
    // Una consola es un SceneElement cuyo contenido es texto que se ACUMULA
    // (buffer append), se teclea (typewriter), se retracta (typewriter en
    // reversa) y se recorta al rect (clip duro). NO es texto-capa (que
    // reemplaza) ni la caja VN (que reemplaza al avanzar): es primitiva propia
    // hospedada en la familia capa, así hereda placement/fade/move/z GRATIS.
    // El typewriter se ESPEJA del de VNDialogBox (acumular dt, revelar N chars
    // a `cps`) SIN acoplar su flujo VN — con dirección añadida para el retract.
    struct ConsoleData
    {
        std::string buffer;            // texto acumulado completo
        f32  revealed = 0.0f;          // chars visibles hoy (animado; float = sub-char timing)
        i32  target   = 0;             // hacia dónde va revealed (escribir sube, borrar baja)
        f32  cps      = 40.0f;         // chars por segundo del typewriter

        // ── Estilo ──
        f32  bgR = 0.0f, bgG = 0.0f, bgB = 0.0f, bgA = 0.6f;   // fondo; alpha = el resaltante
        f32  textR = 1.0f, textG = 1.0f, textB = 1.0f;
        f32  fontSize = 0.0f;          // 0 = renderer default
        std::string fontName;          // fuente nombrada (vacío = default)
        // CP-R3 · identidad tardía 3 de 3. El nombre crudo de arriba sobrevive
        // para diagnóstico; ESTA es la clave de lookup, resuelta contra el
        // catálogo del archivo que creó la consola. Sin ella, un
        // `cargar_script` que reusara el nombre le cambiaría la fuente a una
        // consola que ya estaba escrita.
        std::string fontKey;
        bool cursor = false;           // mostrar cursor '_' parpadeante
        f32  cursorBlink = 0.0f;       // timer del parpadeo

        // Confirma un retract a medio camino: si target < codepoints del buffer
        // (hay cola pendiente de borrar), trunca YA al target. Lo llaman
        // escribir/imprimir ANTES de appendear → un append que llega mientras
        // se borra NO resucita la cola que se estaba retractando (bug de carrera
        // timing+estado: el retract es async en update(), un esperar corto no lo
        // termina, y poner revealed=size sobre el buffer sin truncar mostraba todo).
        void commitErase()
        {
            const i32 total = text::countCodepoints(buffer);
            if (target < total)
            {
                buffer.resize(text::utf8PrefijoBytes(buffer, target));
                if (revealed > (f32)target) revealed = (f32)target;
            }
        }
    };

    // ── Un elemento de la escena con z-order ────────────────────────────────

    struct SceneElement
    {
        std::string name;           // Identificador unico ("mesa", "Luna")
        std::uint64_t scopeOwner = 0; // 0=host; >0=ambito nombrado
        i32         zOrder = 0;     // Orden de dibujo (bajo=atras, alto=frente)

        f32 x = 0, y = 0;          // Posicion en coordenadas virtuales
        f32 w = 0, h = 0;          // Tamanio en coordenadas virtuales

        bool visible = true;        // Se dibuja?
        f32  alpha   = 1.0f;        // Opacidad (0.0 - 1.0)
        f32  parallax = 0.0f;       // Factor de parallax (0=fijo, 1.0=max movimiento)

        // ── Tinte estático (capa ... tinte=#rrggbb — "asset de noche" gratis) ──
        // tintHex = la grafía del guion ("" = sin tinte; es la que viaja al
        // save); los floats son derivados para el draw. Identidad en 1 (§7.208).
        std::string tintHex;
        f32 tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;

        // ── Contenido visual (usar UNO de estos) ────────────────────────
        const Texture* texture = nullptr;   // Textura estatica (no-owning)
        SpriteSheet*   sprite  = nullptr;   // Spritesheet animado (no-owning)

        // ── Color solido (capa id "#rrggbb") — CUARTA forma de contenido ──
        // colorHex vacio = esta capa NO es un color. No-vacio = es un rect
        // solido y no hay textura que cargar. El motor pinta colores solidos
        // desde el primer dia (el fondo del gameplay 2D lo hace hardcodeado);
        // esto es el cable hasta la sintaxis, no capacidad nueva.
        // Hereda alpha (o sea `capa_fade`), parallax, z y tinte GRATIS, porque
        // vive en la misma lista ordenada que todo lo demas.
        std::string colorHex;                          // "" = no es color
        f32 colorR = 1.0f, colorG = 1.0f, colorB = 1.0f, colorA = 1.0f;

        // ── Capa-texto (si !text.empty() → es texto, no imagen) ─────────
        // El tamanio lo da la fuente, NO w/h (quedan en 0).
        std::string text;                    // Contenido a dibujar
        f32 textR = 1.0f, textG = 1.0f, textB = 1.0f;  // Color (alpha = elem.alpha)
        std::string fontName;                // Fuente nombrada (vacio = renderer default)
        // CP-R3 · identidad tardía 1 de 3. La capa conserva la identidad ya
        // resuelta del archivo que la creó, y por eso sobrevive intacta a un
        // `cargar_script` que declare el mismo nombre con otra fuente.
        std::string fontKey;

        // ── Glow del texto-capa (sesion glow) ──
        // glowRadius<=0 o glowIntensity<=0 → sin glow (cae a drawText normal).
        f32 glowR = 1.0f, glowG = 1.0f, glowB = 1.0f;  // color del halo
        f32 glowRadius = 0.0f;       // 0 = sin glow
        f32 glowIntensity = 0.0f;    // 0 = sin glow

        // ── Borde del texto-capa (OUTLINE-TEXTO-CAPA) ──
        // bordeGrosor<=0 o bordeA<=0 → sin borde. Es la misma condicion que ya
        // usa `drawTextOutlined_noBatch` adentro, asi que el render no inventa
        // una segunda ley: pasa los valores y la funcion decide.
        f32 bordeR = 0.0f, bordeG = 0.0f, bordeB = 0.0f, bordeA = 1.0f;
        f32 bordeGrosor = 0.0f;      // 0 = sin borde
        std::vector<ResolvedTextStroke> bordeStrokes;

        // ── Fade (tween de alpha — sirve para texto Y para imagenes) ────
        bool fadeActive   = false;   // ¿hay un fade en curso?
        f32  fadeFrom      = 0.0f;   // alpha inicial
        f32  fadeTo        = 1.0f;   // alpha objetivo
        f32  fadeElapsed   = 0.0f;   // segundos transcurridos
        f32  fadeDuration  = 0.0f;   // segundos totales
        bool fadeHideAtEnd = false;  // al terminar, marcar visible=false (desaparecer)

        // ── Move (tween de posición — espeja el patrón de fade, CP2) ────
        // una_vez = one-shot persistente (se queda en destino). loop = monótono
        // con wrap (scroll infinito, siempre lineal). El easing solo aplica a una_vez.
        bool moveActive = false;
        f32  moveFromX = 0.0f, moveFromY = 0.0f;
        f32  moveToX = 0.0f, moveToY = 0.0f;
        f32  moveElapsed = 0.0f;
        f32  moveDuration = 0.0f;
        bool moveLoop = false;   // true = wrap infinito; false = una_vez
        EasingId moveCurve = EasingId::Lineal;  // C-CP1: curva POR NOMBRE (tabla SSOT).
        // Default lineal = el 0 de antes, bit a bit.

        // ── TRACK-CAPA-XY-01: tracks nombrados propiedad del elemento ──
        // Son offsets aditivos sobre x/y, que siguen siendo la posicion BASE
        // gobernada por `capa` + el tween legado `capa_mover`. El runtime solo
        // admite OffsetX/OffsetY con reloj Seconds; guardar el tipo generico
        // conserva una sola especie matematica y deja el cinturon en el cable.
        // Al vivir aca, el stack muere con el elemento y con su scopeOwner.
        std::vector<TrackInstance> layerTracks;

        // ── Enlace clickable (átomo de texto) ───────────────────────────
        // Marcado por `enlace <id>`. CP1 = dato. CP2 = hover render. CP3 = click.
        bool        interactive = false;  // ¿es un enlace clickable?
        std::string target;               // destino del dispatch ("" = continuar)
        bool        hovered = false;  // runtime: el mouse está encima (CP2)

        // Variante de hover (resuelta en cmdTexto desde el estilo; draw-time).
        // hasHover=false → el enlace no cambia al pasar el mouse ("no miente").
        bool hasHover = false;
        f32  hoverTextR = 1.0f, hoverTextG = 1.0f, hoverTextB = 1.0f;
        f32  hoverGlowR = 1.0f, hoverGlowG = 1.0f, hoverGlowB = 1.0f;
        f32  hoverGlowRadius = 0.0f;
        f32  hoverGlowIntensity = 0.0f;
        f32  hoverBordeR = 0.0f, hoverBordeG = 0.0f, hoverBordeB = 0.0f,
             hoverBordeA = 1.0f;
        f32  hoverBordeGrosor = 0.0f;
        std::vector<ResolvedTextStroke> hoverBordeStrokes;
        // ── Enlace-imagen (átomo de imagen, gemelo del de texto) ──
        // Una capa con `texture` marcada interactive: al hover se intercambia
        // por hoverTexture. nullptr = sin swap visual ("no miente"), pero sigue
        // clickable si interactive + target. Espeja las hover-vars de texto.
        const Texture * hoverTexture = nullptr;

        // ── Consola (CP4) ───────────────────────────────────────────────
        // isConsole=true → el contenido es una consola; se ignoran texture/
        // sprite/text y se dibuja `console` (rama propia en render). Inline
        // (no puntero) para que SceneElement siga siendo copiable — m_elements
        // guarda por valor. El caso común carga una ConsoleData vacía (barato).
        bool        isConsole = false;
        ConsoleData console;
    };

    // ── Manager: ordena y dibuja las capas ──────────────────────────────────

    class SceneLayerManager
    {
    public:
        SceneLayerManager() = default;

        // ── Agregar capas ───────────────────────────────────────────────

        // Capa con textura estatica
        void addLayer(const std::string& name, const Texture* tex,
                      i32 zOrder,
                      f32 x, f32 y, f32 w, f32 h,
                      f32 alpha = 1.0f,
                      std::uint64_t scopeOwner = 0)
        {
            SceneElement elem;
            elem.name    = name;
            elem.scopeOwner = scopeOwner;
            elem.texture = tex;
            elem.zOrder  = zOrder;
            elem.x = x; elem.y = y;
            elem.w = w; elem.h = h;
            elem.alpha   = alpha;
            elem.visible = true;

            m_elements.push_back(elem);
            m_dirty = true;

            std::cout << "[SceneLayer] Agregado: \"" << name
                      << "\" z=" << zOrder << std::endl;
        }

        // Capa de COLOR SOLIDO (capa id "#rrggbb" w= h=). Gemela exacta de
        // `addLayer`: mismo z, mismo placement, mismo alpha — lo unico que
        // cambia es que el contenido es un color y no una textura, asi que
        // `capa_fade`, `capa_visible`, `capa_mover` y `capa_z` la agarran sin
        // saber que es distinta.
        void addColorLayer(const std::string& name, const std::string& hex,
                           f32 r, f32 g, f32 b, f32 a,
                           i32 zOrder,
                           f32 x, f32 y, f32 w, f32 h,
                           f32 alpha = 1.0f,
                           std::uint64_t scopeOwner = 0)
        {
            SceneElement elem;
            elem.name     = name;
            elem.scopeOwner = scopeOwner;
            elem.colorHex = hex;
            elem.colorR = r; elem.colorG = g; elem.colorB = b; elem.colorA = a;
            elem.zOrder   = zOrder;
            elem.x = x; elem.y = y;
            elem.w = w; elem.h = h;
            elem.alpha    = alpha;
            elem.visible  = true;

            m_elements.push_back(elem);
            m_dirty = true;

            std::cout << "[SceneLayer] Agregado (color): \"" << name
                      << "\" " << hex << " z=" << zOrder << std::endl;
        }

        // Capa con spritesheet animado
        void addAnimatedLayer(const std::string& name, SpriteSheet* spr,
                              i32 zOrder,
                              f32 x, f32 y, f32 w, f32 h,
                              f32 alpha = 1.0f,
                              std::uint64_t scopeOwner = 0)
        {
            SceneElement elem;
            elem.name   = name;
            elem.scopeOwner = scopeOwner;
            elem.sprite = spr;
            elem.zOrder = zOrder;
            elem.x = x; elem.y = y;
            elem.w = w; elem.h = h;
            elem.alpha  = alpha;
            elem.visible = true;

            m_elements.push_back(elem);
            m_dirty = true;

            std::cout << "[SceneLayer] Agregado (animado): \"" << name
                      << "\" z=" << zOrder << std::endl;
        }

       // Capa de texto (el tamanio lo da la fuente; w/h quedan en 0)
        void addTextLayer(const std::string& name, const std::string& text,
                          i32 zOrder,
                          f32 x, f32 y,
                          f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f,
                          f32 alpha = 1.0f,
                          const std::string& fontName = "",
                          std::uint64_t scopeOwner = 0)
        {
            SceneElement elem;
            elem.name   = name;
            elem.scopeOwner = scopeOwner;
            elem.text   = text;
            elem.zOrder = zOrder;
            elem.x = x; elem.y = y;
            elem.textR = r; elem.textG = g; elem.textB = b;
            elem.alpha  = alpha;
            elem.visible = true;
            elem.fontName = fontName;

            m_elements.push_back(elem);
            m_dirty = true;

            std::cout << "[SceneLayer] Agregado (texto): \"" << name
                      << "\" z=" << zOrder << std::endl;
        }

        // Consola (CP4): SceneElement con contenido `console`. w/h definen el
        // rect (wrap + clip). Si ya existe un elemento con ese nombre, se
        // reconfigura en sitio (re-crear = re-estilar, no duplicar) — espeja
        // el trato re-entrante del comando `capa`.
        void addConsole(const std::string& name, i32 zOrder,
                        f32 x, f32 y, f32 w, f32 h,
                        const ConsoleData& style,
                        std::uint64_t scopeOwner = 0)
        {
            if (SceneElement* e = get(name))
            {
                e->scopeOwner = scopeOwner;
                e->zOrder = zOrder;
                e->x = x; e->y = y; e->w = w; e->h = h;
                e->visible = true;
                e->isConsole = true;
                std::string keepBuf = e->console.buffer;  // re-estilar no borra lo escrito
                e->console = style;
                e->console.buffer = keepBuf;
                m_dirty = true;
                return;
            }

            SceneElement elem;
            elem.name      = name;
            elem.scopeOwner = scopeOwner;
            elem.zOrder    = zOrder;
            elem.x = x; elem.y = y; elem.w = w; elem.h = h;
            elem.visible   = true;
            elem.isConsole = true;
            elem.console   = style;   // copia el estilo; buffer arranca vacío

            m_elements.push_back(elem);
            m_dirty = true;

            std::cout << "[SceneLayer] Agregado (consola): \"" << name
                      << "\" z=" << zOrder << std::endl;
        }

        // ── Buscar un elemento por nombre ───────────────────────────────

        SceneElement* get(const std::string& name)
        {
            for (auto& elem : m_elements)
                if (elem.name == name)
                    return &elem;
            return nullptr;
        }

        const SceneElement* get(const std::string& name) const
        {
            for (const auto& elem : m_elements)
                if (elem.name == name)
                    return &elem;
            return nullptr;
        }

        // ── Modificar propiedades ───────────────────────────────────────

        void setZOrder(const std::string& name, i32 newZ)
        {
            SceneElement* elem = get(name);
            if (!elem)
            {
                std::cerr << "[SceneLayer] No encontrado: " << name << std::endl;
                return;
            }
            if (elem->zOrder != newZ)
            {
                std::cout << "[SceneLayer] \"" << name << "\" z: "
                          << elem->zOrder << " -> " << newZ << std::endl;
                elem->zOrder = newZ;
                m_dirty = true;
            }
        }

        void setVisible(const std::string& name, bool vis)
        {
            SceneElement* elem = get(name);
            if (elem) elem->visible = vis;
        }

        void setAlpha(const std::string& name, f32 alpha)
        {
            SceneElement* elem = get(name);
            if (elem) elem->alpha = alpha;
        }

        void setPosition(const std::string& name, f32 x, f32 y)
        {
            SceneElement* elem = get(name);
            if (elem) { elem->x = x; elem->y = y; }
        }

        void setSize(const std::string& name, f32 w, f32 h)
        {
            SceneElement* elem = get(name);
            if (elem) { elem->w = w; elem->h = h; }
        }

        void setParallax(const std::string& name, f32 factor)
        {
            SceneElement* elem = get(name);
            if (elem) elem->parallax = factor;
        }

        // Tinte estático multiplicativo. hex "" = quitar tinte (identidad).
        void setTint(const std::string& name, const std::string& hex,
                     f32 r, f32 g, f32 b)
        {
            SceneElement* elem = get(name);
            if (!elem) return;
            elem->tintHex = hex;
            const bool on = !hex.empty();
            elem->tintR = on ? r : 1.0f;
            elem->tintG = on ? g : 1.0f;
            elem->tintB = on ? b : 1.0f;
        }

        // ── RT-1 · LA LEY DEL UPDATE, en un solo lugar ──────────────────────
        // "Sobre algo que ya existe, una linea cambia SOLO lo que nombro."
        //
        // Es la misma ley que `mostrar` ya obedece desde la mesa D2 (§7.306,
        // `VNScriptState.h`: *"la ley unica del update"*) — R-RT-01/02 son los
        // dos verbos a los que nunca se les escribio ese segundo contrato, y a
        // los que por eso "les salio" el de CREATE por accidente.
        //
        // Vive ACA, y no repetida en cada rama de `VNScriptState.h`, por la
        // leccion de §7.323: cuando un arreglo de vocabulario agrega lineas en
        // cada caller, estas escribiendo la ley N+1 en vez de subirla. El dueno
        // del estado que se pisa es el dueno de la regla que decide si se pisa.
        //
        // Ninguna de las dos CREA: si el id no existe, no hacen nada y lo dicen.
        // El CREATE sigue viviendo en `addLayer`/`addTextLayer`, que rellenan
        // defaults porque ese SI es su trabajo.

        // La textura va aparte del patch: es el argumento REQUERIDO del verbo.
        bool applyLayerPatch(const std::string& name, const Texture* tex,
                             const LayerPatch& p)
        {
            SceneElement* e = get(name);
            if (!e)
            {
                std::cerr << "[SceneLayer] applyLayerPatch: no existe \""
                          << name << "\"" << std::endl;
                return false;
            }

            if (tex) e->texture = tex;

            if (p.x) e->x = *p.x;
            if (p.y) e->y = *p.y;
            if (p.w) e->w = *p.w;
            if (p.h) e->h = *p.h;
            // Delegado a proposito: `setZOrder` ademas imprime el cambio de z y
            // marca el re-sort. Asignar aca a mano se comeria esa linea de log.
            if (p.z) setZOrder(name, *p.z);
            if (p.parallax) e->parallax = *p.parallax;

            if (p.tintHex)
            {
                // Enganchado con "" = "quitame el tinte" (identidad). Vacio ya
                // se filtro arriba y significa "no dije nada del tinte".
                e->tintHex = *p.tintHex;
                const bool on = !p.tintHex->empty();
                e->tintR = on ? p.tintR : 1.0f;
                e->tintG = on ? p.tintG : 1.0f;
                e->tintB = on ? p.tintB : 1.0f;
            }

            // OJO: NADA de `m_dirty = true` aca. El re-sort es por z y solo lo
            // marca `setZOrder`, y solo cuando el z CAMBIO. Marcarlo siempre
            // re-ordena de gratis, y `sortLayers` usa `std::sort`, que NO es
            // estable: dos capas con el MISMO z (la cadena menu/pausa/load usa
            // z=10 en las tres) podrian intercambiar su orden de dibujo sin
            // que nadie lo pidiera. Cazado re-leyendo este propio diff.
            return true;
        }

        // El contenido va aparte: es el argumento requerido de `texto`.
        bool applyTextPatch(const std::string& name, const std::string& content,
                            const TextPatch& p)
        {
            SceneElement* e = get(name);
            if (!e)
            {
                std::cerr << "[SceneLayer] applyTextPatch: no existe \""
                          << name << "\"" << std::endl;
                return false;
            }

            e->text = content;

            if (p.x) e->x = *p.x;
            if (p.y) e->y = *p.y;
            if (p.z) setZOrder(name, *p.z);   // ver nota en applyLayerPatch

            if (p.color)
            {
                e->textR = p.color->r; e->textG = p.color->g; e->textB = p.color->b;
            }
            if (p.fontName) e->fontName = *p.fontName;

            if (p.glow)
            {
                e->glowR = p.glow->r; e->glowG = p.glow->g; e->glowB = p.glow->b;
                e->glowRadius    = p.glow->radius;
                e->glowIntensity = p.glow->intensity;
            }

            // OUTLINE-TEXTO-CAPA. El patch ya resolvio la exclusion con el glow
            // (`makeTextPatch`: si hay borde, `p.glow` viene vacio), asi que
            // aca NO se vuelve a decidir — pero SI hay que apagar el glow que
            // el elemento traia de antes, o un `texto` que agrega borde a una
            // capa que ya existia dejaria los dos puestos y el render elegiria
            // uno solo, en silencio.
            if (p.borde)
            {
                e->bordeR = p.borde->r; e->bordeG = p.borde->g;
                e->bordeB = p.borde->b; e->bordeA = p.borde->a;
                e->bordeGrosor = p.borde->grosor;
                e->bordeStrokes = p.borde->strokes;
                e->glowRadius = 0.0f; e->glowIntensity = 0.0f;
            }

            if (p.hover)
            {
                e->hasHover = true;
                e->hoverTextR = p.hover->text.r;
                e->hoverTextG = p.hover->text.g;
                e->hoverTextB = p.hover->text.b;
                e->hoverGlowR = p.hover->glow.r;
                e->hoverGlowG = p.hover->glow.g;
                e->hoverGlowB = p.hover->glow.b;
                e->hoverGlowRadius    = p.hover->glow.radius;
                e->hoverGlowIntensity = p.hover->glow.intensity;
                e->hoverBordeR = p.hover->borde.r;
                e->hoverBordeG = p.hover->borde.g;
                e->hoverBordeB = p.hover->borde.b;
                e->hoverBordeA = p.hover->borde.a;
                e->hoverBordeGrosor = p.hover->borde.grosor;
                e->hoverBordeStrokes = p.hover->borde.strokes;
            }

            // Sin `m_dirty` — ver la nota en `applyLayerPatch`.
            return true;
        }

        // Inicia un fade de alpha. appear=true → fade-in (0→alpha actual o 1),
        // appear=false → fade-out (actual→0) y al terminar se oculta.
        void startFade(const std::string& name, bool appear,
                       f32 duration)
        {
            SceneElement* elem = get(name);
            if (!elem) return;

            if (appear)
            {
                elem->visible      = true;
                elem->fadeFrom     = 0.0f;
                elem->fadeTo       = 1.0f;
                elem->alpha        = 0.0f;   // arranca invisible
                elem->fadeHideAtEnd = false;
            }
            else
            {
                elem->fadeFrom     = elem->alpha;  // desde donde esté
                elem->fadeTo       = 0.0f;
                elem->fadeHideAtEnd = true;
            }
            elem->fadeElapsed  = 0.0f;
            elem->fadeDuration = (duration > 0.0f) ? duration : 0.0001f;
            elem->fadeActive   = true;
        }

        // Inicia un tween de posición. una_vez (loop=false) = one-shot que se
        // queda en destino; loop=true = scroll monótono con wrap. Espeja startFade.
        void startMove(const std::string& name, f32 toX, f32 toY,
            f32 duration, bool loop, EasingId curva)
        {
            SceneElement* elem = get(name);
            if (!elem) return;

            elem->moveFromX = elem->x;
            elem->moveFromY = elem->y;
            elem->moveToX = toX;
            elem->moveToY = toY;
            elem->moveElapsed = 0.0f;
            elem->moveDuration = (duration > 0.0f) ? duration : 0.0001f;
            elem->moveLoop = loop;
            elem->moveCurve = curva;
            elem->moveActive = true;
        }

        // ── TRACK-CAPA-XY-01: ciclo de vida del stack nombrado ─────────
        // Re-emitir el MISMO origen reemplaza el conjunto y reinicia su reloj;
        // origenes distintos apilan. El caller valida canales/reloj antes de
        // entrar: SceneLayer almacena/evalua, no interpreta EclipScript.
        size_t replaceTracks(const std::string& name, const std::string& origin,
                             const std::vector<AnimTrack>& tracks,
                             float afterSeconds = 0.0f)
        {
            SceneElement* elem = get(name);
            if (!elem) return 0;

            auto& stack = elem->layerTracks;
            stack.erase(std::remove_if(stack.begin(), stack.end(),
                [&origin](const TrackInstance& ti)
                { return ti.origen == origin; }),
                stack.end());

            for (const auto& tr : tracks)
                stack.push_back(startTrackInstance(tr, origin, afterSeconds));
            return tracks.size();
        }

        size_t removeTracks(const std::string& name, const std::string& origin)
        {
            SceneElement* elem = get(name);
            if (!elem) return 0;
            auto& stack = elem->layerTracks;
            const size_t before = stack.size();
            stack.erase(std::remove_if(stack.begin(), stack.end(),
                [&origin](const TrackInstance& ti)
                { return ti.origen == origin; }),
                stack.end());
            return before - stack.size();
        }

        size_t clearTracks(const std::string& name)
        {
            SceneElement* elem = get(name);
            if (!elem) return 0;
            const size_t count = elem->layerTracks.size();
            elem->layerTracks.clear();
            return count;
        }

        // Posicion efectiva SSOT: base legado + algebra neutral de tracks.
        // Render, efectos e hit-test pasan por esta misma funcion; el parallax
        // se aplica DESPUES de sumar los offsets, como transformacion de camara.
        static void effectivePosition(const SceneElement& elem,
                                      f32& outX, f32& outY)
        {
            const AnimOutput ao = composeTracks(elem.layerTracks);
            outX = elem.x + ao.dx;
            outY = elem.y + ao.dy;
        }

        static void effectivePositionWithParallax(const SceneElement& elem,
                                                   f32 mouseNormX,
                                                   f32 mouseNormY,
                                                   f32& outX, f32& outY)
        {
            effectivePosition(elem, outX, outY);
            static constexpr f32 PARALLAX_MAX = 40.0f;
            outX -= (mouseNormX - 0.5f) * elem.parallax * PARALLAX_MAX;
            outY -= (mouseNormY - 0.5f) * elem.parallax * PARALLAX_MAX * 0.5f;
        }

        static bool hitTestEffectiveRect(const SceneElement& elem,
                                         f32 mouseVX, f32 mouseVY,
                                         f32 mouseNormX, f32 mouseNormY,
                                         f32 width, f32 height)
        {
            f32 px = 0.0f, py = 0.0f;
            effectivePositionWithParallax(elem, mouseNormX, mouseNormY,
                                          px, py);
            return mouseVX >= px && mouseVX <= px + width &&
                   mouseVY >= py && mouseVY <= py + height;
        }

        bool tracksBlockAnimationWait() const
        {
            for (const auto& elem : m_elements)
                for (const auto& ti : elem.layerTracks)
                    if (trackBlocksAnimationWait(ti.track)) return true;
            return false;
        }

        // Cambiar la textura de un elemento (ej: idle → select)
        void setTexture(const std::string& name, const Texture* tex)
        {
            SceneElement* elem = get(name);
            if (elem) elem->texture = tex;
        }

        // ── Verbos de la consola (CP4) ──────────────────────────────────
        // escribir: appendea + sube `revealed` a `cps` (se teclea en vivo).
        void consoleWrite(const std::string& name, const std::string& text)
        {
            SceneElement* e = get(name);
            if (!e || !e->isConsole) return;
            e->console.commitErase();   // no resucitar un retract a medio camino
            e->console.buffer += text;
            e->console.target = text::countCodepoints(e->console.buffer);
        }

        // imprimir: appendea DE GOLPE (revealed salta al final; output del sistema).
        void consolePrint(const std::string& name, const std::string& text)
        {
            SceneElement* e = get(name);
            if (!e || !e->isConsole) return;
            e->console.commitErase();   // no resucitar un retract a medio camino
            e->console.buffer += text;
            e->console.target = text::countCodepoints(e->console.buffer);
            e->console.revealed = (f32)e->console.target;
        }

        // limpiar: vacía el buffer (borrado manual, instantáneo).
        void consoleClear(const std::string& name)
        {
            SceneElement* e = get(name);
            if (!e || !e->isConsole) return;
            e->console.buffer.clear();
            e->console.target = 0;
            e->console.revealed = 0.0f;
        }

        // borrar: target baja N codepoints (o todo) → update() retracta el
        // typewriter en reversa y trunca el buffer al llegar. N cuenta GLIFOS,
        // no bytes. La conversion a frontera de byte ocurre una sola vez al
        // truncar, con `utf8PrefijoBytes` -- la misma ley que usa el draw.
        void consoleErase(const std::string& name, i32 n, bool all)
        {
            SceneElement* e = get(name);
            if (!e || !e->isConsole) return;
            if (all) { e->console.target = 0; return; }

            const i32 total = text::countCodepoints(e->console.buffer);
            const i32 cantidad = n > 0 ? n : 0;
            e->console.target = std::max(0, total - cantidad);
        }

        // ── Actualizar animaciones ──────────────────────────────────────

        void update(f64 dt)
        {
            for (auto& elem : m_elements)
            {
                if (elem.visible && elem.sprite)
                {
                    elem.sprite->update(dt);
                    elem.sprite->checkAutoTransition();
                }

                // ── Fade de alpha (texto o imagen) ──────────────────
                if (elem.fadeActive)
                {
                    elem.fadeElapsed += (f32)dt;
                    f32 t = elem.fadeElapsed / elem.fadeDuration;
                    if (t >= 1.0f) t = 1.0f;

                    // C-CP1: des-inlineado contra la tabla SSOT. Byte-idéntico
                    // (harness: 1001 pasos, bit-exacto). capa_fade NO gana knob
                    // de curva todavía (§7.118): `rebote`/`resorte` sobrepasan
                    // 1.0 y en un alpha eso pide clamp + una decisión. Sin caller.
                    f32 eased = applyEasing(EasingId::Suave, t);
                    elem.alpha = elem.fadeFrom +
                                 (elem.fadeTo - elem.fadeFrom) * eased;

                    if (t >= 1.0f)
                    {
                        elem.fadeActive = false;
                        if (elem.fadeHideAtEnd)
                            elem.visible = false;
                    }
                }

                // ── Move de posición (espeja el fade, CP2) ──────────
                if (elem.moveActive)
                {
                    elem.moveElapsed += (f32)dt;
                    f32 t = elem.moveElapsed / elem.moveDuration;

                    if (elem.moveLoop)
                    {
                        // Monótono con wrap: fmod preserva el sub-frame. Lineal
                        // siempre (un easing pulsaría en cada vuelta). No termina.
                        t = std::fmod(t, 1.0f);
                        elem.x = elem.moveFromX + (elem.moveToX - elem.moveFromX) * t;
                        elem.y = elem.moveFromY + (elem.moveToY - elem.moveFromY) * t;
                    }
                    else
                    {
                        if (t >= 1.0f) t = 1.0f;
                        // C-CP1: la curva se resuelve por NOMBRE contra la tabla.
                        // `lineal`/`suave` byte-idénticos; `rebote`/`resorte` caen
                        // gratis (H7 revisitado: se acopla la TABLA, no el sistema).
                        f32 e = applyEasing(elem.moveCurve, t);                                       // lineal
                        elem.x = elem.moveFromX + (elem.moveToX - elem.moveFromX) * e;
                        elem.y = elem.moveFromY + (elem.moveToY - elem.moveFromY) * e;

                        if (t >= 1.0f)
                            elem.moveActive = false; // una_vez: persistente en destino
                    }
                }

                // TRACK-CAPA-XY-01: mismo reloj puro que personajes/pantalla.
                // No hay poda: una_vez terminado sostiene su offset final;
                // detener, retirar el ambito o borrar la capa bajan el stack.
                for (auto& ti : elem.layerTracks)
                    tickTrackSeconds(ti.track, (f32)dt);

                // ── Consola: typewriter bidireccional + cursor (CP4) ────
                if (elem.isConsole)
                {
                    ConsoleData& c = elem.console;
                    f32 tgt = (f32)c.target;
                    if (c.revealed < tgt)        // escribir: revela subiendo
                    {
                        c.revealed += c.cps * (f32)dt;
                        if (c.revealed > tgt) c.revealed = tgt;
                    }
                    else if (c.revealed > tgt)   // borrar/retract: revela bajando
                    {
                        c.revealed -= c.cps * (f32)dt;
                        if (c.revealed <= tgt)
                        {
                            c.revealed = tgt;
                            // el retract terminó → truncar el buffer al target
                            const size_t targetBytes =
                                text::utf8PrefijoBytes(c.buffer, c.target);
                            if (c.buffer.size() > targetBytes)
                                c.buffer.resize(targetBytes);
                        }
                    }
                    if (c.cursor) c.cursorBlink += (f32)dt;
                }

            }
        }

        // ── Renderizar todas las capas en orden Z ───────────────────────
        // mouseNormX/Y: posicion normalizada del mouse (0.0=borde, 1.0=borde)
        // 0.5 = centro (sin parallax). Si no se pasa, no hay parallax.
        // tr: TextRenderer para capas-texto. Si es nullptr, las capas-texto
        //     simplemente no se dibujan (call-sites viejos siguen igual).

        // E-CP2: el state puede inyectar un hook de efectos de render. Se
        // consulta SOLO en la rama de capas-imagen, con la textura ya resuelta
        // (hover incluido) y la posición con parallax; si devuelve true, el
        // hook pintó (tubería de pasadas) y la capa no se dibuja normal.
        using FxHook = std::function<bool(const SceneElement&, const Texture&,
                                          f32 px, f32 py)>;

        void render(f32 mouseNormX = 0.5f, f32 mouseNormY = 0.5f,
                    TextRenderer* tr = nullptr,
                    FontManager* fm = nullptr,
                    const FxHook& fxHook = {})
        {
            // Re-ordenar si cambio algun z-order
            if (m_dirty)
            {
                sortLayers();
                m_dirty = false;
            }

            // Dibujar de menor a mayor z (atras → frente)
            for (const auto& elem : m_sorted)
            {
                if (!elem->visible) continue;
                if (elem->alpha <= 0.0f) continue;

                // TRACK-CAPA-XY-01: posicion efectiva unica para render,
                // hook de efectos e hit-test (base + tracks + parallax).
                f32 px = 0.0f, py = 0.0f;
                effectivePositionWithParallax(*elem, mouseNormX, mouseNormY,
                                              px, py);

                if (elem->isConsole)
                {
                    const ConsoleData& c = elem->console;

                    // Fondo: el rect mismo (NO se clipea). alpha = bgA * elem.
                    draw2d::drawRect_noBatch(px, py, elem->w, elem->h,
                        c.bgR, c.bgG, c.bgB, c.bgA * elem->alpha);

                    // Resolver fuente: nombrada (fuente=) ∘ tamaño (tamano=).
                    // Espeja la composición del tamaño per-caja (getSized).
                    TextRenderer* useTR = tr;
                    if ((!c.fontKey.empty() || !c.fontName.empty()) && fm)
                    {
                        TextRenderer* nf = fm->getLigado(c.fontKey, c.fontName);
                        if (nf) useTR = nf;
                    }
                    if (c.fontSize > 0.0f && fm && useTR)
                    {
                        TextRenderer* sized = fm->getSized(*useTR,
                            c.fontSize, useTR->kerning(), useTR->fontMeasure());
                        if (sized) useTR = sized;
                    }

                    if (useTR)
                    {
                        // El reloj vive en codepoints; esta es su unica
                        // conversion a offset de byte para el prefijo visible.
                        const i32 shownCp = (c.revealed > 0.0f)
                            ? static_cast<i32>(c.revealed) : 0;
                        const size_t shownB =
                            text::utf8PrefijoBytes(c.buffer, shownCp);
                        std::string disp = c.buffer.substr(0, shownB);

                        // Cursor '_' parpadeante tras el último char revelado
                        // (periodo 1s: 0.5 visible, 0.5 oculto). El motor lo hace.
                        if (c.cursor && std::fmod(c.cursorBlink, 1.0f) < 0.5f)
                            disp += "_";

                        // Clip duro al rect: flushear el batch (fondo + capas
                        // previas salen sin recorte), angostar el scissor, dibujar
                        // el texto recortado, restaurar. Patrón immediate (glitch).
                        if (draw2d::g_batch)
                        {
                            draw2d::g_batch->end();
                            draw2d::pushScissorVirtual(px, py, elem->w, elem->h);
                            draw2d::g_batch->begin();
                        }
                        useTR->drawTextWrapped_noBatch(disp, px, py,
                            elem->w, -1,
                            c.textR, c.textG, c.textB, elem->alpha);
                        if (draw2d::g_batch)
                        {
                            draw2d::g_batch->end();
                            draw2d::popScissorVirtual();
                            draw2d::g_batch->begin();
                        }
                    }
                }
                else if (!elem->text.empty())
                {
                    // Capa-texto: resolver la fuente nombrada si la pidio;
                    // si no (o no existe) → renderer default (tr).
                    TextRenderer* useTR = tr;
                    if ((!elem->fontKey.empty() || !elem->fontName.empty())
                        && fm)
                    {
                        TextRenderer* named =
                            fm->getLigado(elem->fontKey, elem->fontName);
                        if (named) useTR = named;
                        // si named == nullptr, cae al default tr (no crashea)
                    }
                    if (useTR)
                    {
                        // Enlace hovered (CP2): swap a la variante de hover.
                        bool useHover = elem->interactive && elem->hovered
                            && elem->hasHover;
                        f32 tR = useHover ? elem->hoverTextR : elem->textR;
                        f32 tG = useHover ? elem->hoverTextG : elem->textG;
                        f32 tB = useHover ? elem->hoverTextB : elem->textB;
                        f32 gR = useHover ? elem->hoverGlowR : elem->glowR;
                        f32 gG = useHover ? elem->hoverGlowG : elem->glowG;
                        f32 gB = useHover ? elem->hoverGlowB : elem->glowB;
                        f32 gRad = useHover ? elem->hoverGlowRadius : elem->glowRadius;
                        f32 gInt = useHover ? elem->hoverGlowIntensity : elem->glowIntensity;

                        // OUTLINE-TEXTO-CAPA: el borde es el TERCER camino, no
                        // un cuarto. Va primero porque borde y glow son
                        // excluyentes por contrato (fork 3a) — si los dos
                        // llegaran puestos, el orden de este `if` seria quien
                        // decide, y eso es justo el fallo mudo que la ley
                        // evita. `drawTextOutlined_noBatch` ya cae solo a
                        // texto plano si grosor<=0 o alpha<=0, asi que no se
                        // duplica la condicion: se pregunta una vez aca para
                        // no pagar 8 pasadas cuando no hay borde.
                        const auto& strokes = useHover ? elem->hoverBordeStrokes
                                                       : elem->bordeStrokes;
                        f32 bR = useHover ? elem->hoverBordeR : elem->bordeR;
                        f32 bG = useHover ? elem->hoverBordeG : elem->bordeG;
                        f32 bB = useHover ? elem->hoverBordeB : elem->bordeB;
                        f32 bA = useHover ? elem->hoverBordeA : elem->bordeA;
                        f32 bGro = useHover ? elem->hoverBordeGrosor
                                            : elem->bordeGrosor;
                        if (!strokes.empty())
                        {
                            useTR->drawTextStrokes_noBatch(elem->text, px, py,
                                tR, tG, tB, elem->alpha, strokes);
                        }
                        else if (bGro > 0.0f && bA > 0.0f)
                        {
                            // El alpha del borde se multiplica por el de la
                            // capa, igual que hace el color solido: asi
                            // `capa_fade` se lleva el borde con el texto en vez
                            // de dejarlo flotando.
                            useTR->drawTextOutlined_noBatch(elem->text, px, py,
                                tR, tG, tB, elem->alpha,
                                bR, bG, bB, bA * elem->alpha, bGro);
                        }
                        else if (gRad > 0.0f && gInt > 0.0f)
                        {
                            useTR->drawTextGlow_noBatch(elem->text, px, py,
                                tR, tG, tB, elem->alpha,
                                gR, gG, gB, gRad, gInt);
                        }
                        else
                        {
                            useTR->drawText_noBatch(elem->text, px, py,
                                tR, tG, tB, elem->alpha);
                        }
                    }
                }
                else if (!elem->colorHex.empty())
                {
                    // COLOR-SOLIDO: un rect del color de la capa, en SU lugar
                    // del z-order. Es la misma llamada que ya hacen la consola
                    // (arriba), la caja de dialogo y el fondo del gameplay 2D.
                    //
                    // Hereda todo por multiplicacion, sin caso especial:
                    //   · `elem->alpha`  -> `capa_fade` funciona gratis
                    //   · `px`/`py`      -> parallax ya aplicado arriba
                    //   · `tintR/G/B`    -> `tinte=` filtra igual que a una
                    //                       textura (no se le invento una ley
                    //                       propia: un caso especial es una ley
                    //                       nueva que nadie pidio)
                    //   · `colorA`       -> el AA opcional del #RRGGBBAA
                    draw2d::drawRect_noBatch(px, py, elem->w, elem->h,
                        elem->colorR * elem->tintR,
                        elem->colorG * elem->tintG,
                        elem->colorB * elem->tintB,
                        elem->colorA * elem->alpha);
                }
                else if (elem->texture && elem->texture->isLoaded())
                {
                    // Enlace-IMAGEN hovered (CP5): swap al PNG "encendido".
                    // Espeja el useHover del texto; hoverTexture nullptr o la
                    // capa no-interactiva → sin swap ("no miente"). Mismo
                    // px/py/w/h → el lit cae exacto sobre el normal.
                    const Texture* tex = (elem->interactive && elem->hovered
                        && elem->hoverTexture && elem->hoverTexture->isLoaded())
                        ? elem->hoverTexture : elem->texture;

                    // E-CP2: si la capa tiene efectos, el hook pinta la tubería
                    // en ESTE lugar del z-order y la rama normal se salta.
                    if (fxHook && fxHook(*elem, *tex, px, py))
                        continue;

                    // F2.B.3: _noBatch — SceneLayer se renderiza dentro
                    // del begin/end del state que lo contiene. Con tinte en
                    // identidad (1,1,1) es la MISMA llamada al batch de siempre.
                    draw2d::drawTextureTinted_noBatch(*tex,
                        px, py,
                        elem->w, elem->h,
                        elem->tintR, elem->tintG, elem->tintB,
                        elem->alpha);
                }
                else if (elem->sprite && elem->sprite->isLoaded())
                {
                    // F2.B.3: drawNoBatch — misma razón.
                    elem->sprite->drawNoBatch(px, py,
                        elem->w, elem->h,
                        elem->alpha);
                }
            }
        }

        // ── Consultas ───────────────────────────────────────────────────

        // Hover de enlaces (átomo de texto clickable, CP2). Espeja la geometría
        // del render: mismo parallax, misma resolución de fuente. Hit-test del
        // mouse (en coords virtuales) contra los límites medidos del texto
        // (measureWidth × lineHeight) → "clickeás lo que ves". Solo toca los
        // elementos interactive; el resto se ignora.
        // ── LA MEDIDA DE UN INTERACTIVO, EN UN SOLO SITIO ───────────────────
        //
        // La saco `CP2a` de `DISENO_MUTAR_PALABRAS` de adentro de
        // `updateInteractiveHover`, y **no por prolijidad**: el acomodo del
        // bloque `palabras` tiene que COLOCAR cada palabra en el mismo rect que
        // despues la va a CLICKEAR. Dos copias de esta medida son dos nociones
        // de "clickeable" —la que se ve y la que responde—, que es justo lo que
        // §7.100 (render == hit-test) prohibe y lo que este mismo archivo le
        // escribe al foco del teclado veinte lineas mas abajo.
        //
        // Devuelve `false` cuando no hay con que medir (sin fuente): el caller
        // decide que hacer con eso — el hover apaga, el acomodo saltea.
        bool medirInteractivo(const SceneElement& elem, TextRenderer* tr,
                              FontManager* fm, f32& w, f32& h) const
        {
            if (!elem.text.empty())
            {
                // Enlace-TEXTO: medir contra la misma fuente que dibujará.
                // §7.100 (render == hit-test): la medida usa la MISMA puerta
                // que el dibujo, identidad resuelta incluida. Si acá quedara
                // `get(fontName)` y allá `getLigado`, una capa del archivo
                // anterior se dibujaría con una fuente y se mediría con otra.
                TextRenderer* useTR = tr;
                if ((!elem.fontKey.empty() || !elem.fontName.empty()) && fm)
                {
                    TextRenderer* named =
                        fm->getLigado(elem.fontKey, elem.fontName);
                    if (named) useTR = named;
                }
                if (!useTR) return false;
                w = useTR->measureWidth(elem.text);
                h = useTR->getLineHeight();
                return true;
            }
            // Enlace-IMAGEN: el rect ES la capa (x/y/w/h), igual que el draw
            // (render≡hit-test §7.100; misma px/py de parallax).
            w = elem.w;
            h = elem.h;
            return true;
        }

        void updateInteractiveHover(f32 mouseVX, f32 mouseVY,
            f32 mouseNormX, f32 mouseNormY,
            TextRenderer* tr, FontManager* fm)
        {
            for (auto& elem : m_elements)
            {
                if (!elem.interactive) continue;
                if (!elem.visible || elem.alpha <= 0.0f) { elem.hovered = false; continue; }

                f32 w = 0.0f, h = 0.0f;
                if (!medirInteractivo(elem, tr, fm, w, h))
                {
                    elem.hovered = false;
                    continue;
                }

                elem.hovered = hitTestEffectiveRect(elem,
                    mouseVX, mouseVY, mouseNormX, mouseNormY, w, h);
            }
        }

        // Enlace clickeado: el interactive hovered de mayor z (first-hit-wins).
        // nullptr si el mouse no está sobre ningún enlace. Lo usa el click (CP3).
        SceneElement* hoveredInteractive()
        {
            SceneElement* best = nullptr;
            for (auto& elem : m_elements)
                if (elem.interactive && elem.hovered)
                    if (!best || elem.zOrder >= best->zOrder)
                        best = &elem;
            return best;
        }

        // Apaga el hover de todos los interactivos (un modal congela el enlace).
        void clearInteractiveHover()
        {
            for (auto& elem : m_elements)
                elem.hovered = false;
        }

        // ── `CP6`: EL ENLACE COMO FOCABLE ───────────────────────────────────
        //
        // El anillo del `DISENO_ATOMO_TECLADO.md` §9 necesita **un indice**, y
        // esta capa nunca tuvo uno: el hover es un `bool` POR ELEMENTO y
        // `hoveredInteractive()` desempata por z. Estas tres funciones le dan la
        // vista indexada **sin agregar estado**: el orden es el de
        // `m_elements`, o sea el de declaracion del `.ecl`, que es exactamente
        // el que la ley 4 de `FocusRing.h` usa para las otras familias.
        //
        // **El filtro es el MISMO que `updateInteractiveHover`** -interactivo,
        // visible y con alpha- y eso no es prolijidad: si el teclado pudiera
        // focar un enlace que el mouse no puede tocar, tendriamos dos nociones
        // de "clickeable" y el §7.100 (render ≡ hit-test) se parte.
        bool esFocable(const SceneElement& e) const
        {
            return e.interactive && e.visible && e.alpha > 0.0f;
        }

        int interactiveCount() const
        {
            int n = 0;
            for (const auto& e : m_elements) if (esFocable(e)) ++n;
            return n;
        }

        SceneElement* interactiveAt(int idx)
        {
            if (idx < 0) return nullptr;
            int n = 0;
            for (auto& e : m_elements)
                if (esFocable(e) && n++ == idx) return &e;
            return nullptr;
        }

        // Escribe el hover del teclado sobre el MISMO campo que el mouse usa
        // (`elem.hovered`), asi el swap del enlace y su sonido se heredan gratis
        // -que es lo que el §9 promete con *"cero render nuevo"*-. Un `-1` apaga
        // todo, igual que sacar el mouse de encima.
        void setInteractiveFocus(int idx)
        {
            int n = 0;
            for (auto& e : m_elements)
            {
                if (!esFocable(e)) { e.hovered = false; continue; }
                e.hovered = (n++ == idx);
            }
        }

        // La inversa, para la CONVERGENCIA: el indice del que
        // `hoveredInteractive()` elegiria. Se deriva del MISMO recorrido para
        // que no puedan discrepar (§1.40 aplicado a dos lecturas del mismo dato).
        int hoveredInteractiveIndex()
        {
            const SceneElement* best = hoveredInteractive();
            if (!best) return -1;
            int n = 0;
            for (const auto& e : m_elements)
            {
                if (!esFocable(e)) continue;
                if (&e == best) return n;
                ++n;
            }
            return -1;
        }

        int layerCount() const { return static_cast<int>(m_elements.size()); }

        // Retira una generación completa sin enumerar ids desde el creador.
        // Al borrar el SceneElement tambien desaparecen sus fades, moves,
        // tracks, hover y contenido de consola asociados.
        void removeByOwner(std::uint64_t scopeOwner)
        {
            if (scopeOwner == 0) return;
            const auto oldSize = m_elements.size();
            m_elements.erase(
                std::remove_if(m_elements.begin(), m_elements.end(),
                    [scopeOwner](const SceneElement& elem) {
                        return elem.scopeOwner == scopeOwner;
                    }),
                m_elements.end());
            if (m_elements.size() != oldSize)
            {
                m_sorted.clear();
                m_dirty = true;
            }
        }

        // ── Skip-frame (Perfilado §6): ¿alguna capa anima este frame? ──────
        // CONSERVADOR a propósito (sesgo a "sí anima" = no quiescente): cualquier
        // mecanismo que el update() de arriba tickee por-frame cuenta. fade/move
        // y layerTracks son los tweens; un spritesheet se anima solo; consola e
        // hover tienen estado propio → over-approx a "anima" (CP2.x los precisa:
        // un sprite de 1 cuadro o una consola retraída-quieta no animan). Una
        // capa invisible no se dibuja, así que no cuenta.
        bool isAnimating() const
        {
            for (const auto& e : m_elements)
            {
                if (!e.visible) continue;
                if (e.fadeActive || e.moveActive) return true;
                for (const auto& ti : e.layerTracks)
                    if (trackCambia(ti.track)) return true;
                if (e.sprite)    return true;   // ↯ CP2.x: solo si el sheet corre
                if (e.isConsole) return true;   // ↯ CP2.x: consola typewriter/retract
                if (e.hovered)   return true;   // ↯ CP2.x: hover glow del enlace
            }
            return false;
        }

        // Iterar sobre las capas ordenadas (para HUD/debug)
        const std::vector<const SceneElement*>& getSortedLayers()
        {
            if (m_dirty) { sortLayers(); m_dirty = false; }
            return m_sorted;
        }

        // ── Limpiar ─────────────────────────────────────────────────────

        void clear()
        {
            m_elements.clear();
            m_sorted.clear();
            m_dirty = false;
        }

    private:
        void sortLayers()
        {
            m_sorted.clear();
            m_sorted.reserve(m_elements.size());
            for (auto& elem : m_elements)
                m_sorted.push_back(&elem);

            std::sort(m_sorted.begin(), m_sorted.end(),
                [](const SceneElement* a, const SceneElement* b)
                {
                    return a->zOrder < b->zOrder;
                });
        }

        std::vector<SceneElement>        m_elements;  // Datos reales
        std::vector<const SceneElement*> m_sorted;     // Vista ordenada por z
        bool m_dirty = false;
    };

} // namespace eclips
