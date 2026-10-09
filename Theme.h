// ============================================================================
// Theme.h — Sistema de temas visuales para Eclips Engine
// ============================================================================
//
// Carga un archivo .eclips con colores en formato hex (#RRGGBB)
// y opacidades en porcentaje (88%).
//
// Uso desde EclipScript:
//   cargar_tema "tema_pastel.eclips"
//   load_theme  "tema_pastek.eclips"
//
// ── Formato del archivo de tema ──────────────────────────────────────────
//
//   # Comentario — se ignora
//
//   caja_color      #0D0514    # color en hex
//   caja_opacidad   88%        # opacidad en porcentaje
//   caja_borde      #CC66E0
//
//   nombre_fondo    #330852
//   nombre_borde    #CC66E0
//   nombre_texto    #FFD9F0
//
//   texto_color     #F5F0FF
//
//   boton_fondo     #2E0B3D
//   boton_borde     #A64DCC
//   boton_texto     #E0AAFF
//
//   fuente          "assets/fonts/mifuente.ttf"
//
// ── Notas ────────────────────────────────────────────────────────────────
//
//   - Hex sin alpha → alpha = 1.0 (totalmente opaco)
//   - Hex con alpha (#RRGGBBAA) también funciona
//   - caja_opacidad / nombre_opacidad sobreescriben el alpha del color
//   - El formato anterior (0.0 - 1.0) sigue siendo compatible
//
// ============================================================================
// ============================================================================
// Theme.h — Sistema de temas visuales para Eclips Engine (v2)
// ============================================================================
// Ahora incluye geometría de la caja de diálogo, choices, QTE,
// botones de UI, delays y límite de historial.
// Compatible hacia atrás: los valores por defecto replican el comportamiento duro.
//
// ── v4: refactor estructural §7.4 pasada 1 ──────────────────────────────
// Todos los campos visuales/geométricos de la caja de diálogo viven ahora
// en una sub-struct `DialogBoxStyle`. Theme la contiene como
// `defaultDialogBox`. Los keys del .eclips NO cambian — el creador escribe
// el mismo archivo de antes, solo cambia adónde apunta el parser
// internamente. Esto prepara el terreno para `caja_dialogo "X" { ... }`
// (pasada 2) sin romper compatibilidad de archivos existentes.
//
// Renombrado selectivo: solo los campos cuyo prefijo duplicaba el nombre
// de la struct lo perdieron (dialogBoxHeight → height, etc). Sub-elementos
// como namePlate*, uiBtn*, choice*, qte*, autoBar*, advanceIndicator*
// mantienen su prefijo porque identifican qué elemento dentro de la caja.
//
// Lo que se quedó en Theme: feel del juego (textSpeed, autoDelay,
// skipInterval) + sus locks + historyMaxEntries + sourcePath.
// ============================================================================
#pragma once

#include "engine/core/Types.h"
#include "engine/core/EclipsParse.h"   // parseBool centralizado (§1.45)
#include "engine/core/TextSpeed.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <array>
#include <map>

namespace eclips
{

    // ═══════════════════════════════════════════════════════════════
    // Enums de presentación (v3 — usados por el bloque de UI flexible)
    // ═══════════════════════════════════════════════════════════════
    // Right = default histórico (botones a la derecha de la caja, estilo Ren'Py).
    // Center / Left disponibles para themes que quieran otra disposición.
    enum class UIBtnAlign : u8 { Right = 0, Center = 1, Left = 2 };

    // Box = caja con fondo+borde (default histórico).
    // TextOnly = solo el label, sin fondo ni borde (estilo pastel-VN).
    enum class UIBtnStyle : u8 { Box = 0, TextOnly = 1 };

    // Inline = thumb a la izq + texto corrido a la derecha (default, estilo TF).
    // Vertical = thumb ancho arriba + texto apilado debajo (estilo DDLC/Ren'Py).
    enum class SlotLayout : u8 { Inline = 0, Vertical = 1 };

    // ═══════════════════════════════════════════════════════════════
    // DialogBoxStyle — todo el look + geometría de UNA caja de diálogo
    // ═══════════════════════════════════════════════════════════════
    // Definición visual completa de una caja: colores, geometría, fuente,
    // botones UI, choices, QTE, indicadores. Una `DialogBoxStyle` es
    // suficiente para describir "cómo se ve una caja pastel-VN" o "cómo se ve
    // una caja de combate pixel" — independiente del feel global del juego.
    //
    // Hoy `Theme` contiene una sola (`defaultDialogBox`). En pasada 2 se
    // agregará un map de cajas nominadas con sintaxis declarativa
    // `caja_dialogo "X" { ... }` y un comando `usar_caja X` desde EclipScript.
    //
    struct DialogBoxStyle
    {
        // ═══════════════════════════════════════════════════════════════
        // Colores
        // ═══════════════════════════════════════════════════════════════
        // v3 — Identidad Eclips: azul nocturno con cyan
        f32 boxR = 0.059f, boxG = 0.102f, boxB = 0.180f, boxA = 0.85f;       // #0F1A2E
        f32 boxBorderR = 0.310f, boxBorderG = 0.765f, boxBorderB = 0.863f,
            boxBorderA = 0.95f;                                              // #4FC3DC

        f32 namePlateR = 0.102f, namePlateG = 0.180f, namePlateB = 0.302f,
            namePlateA = 0.92f;                                              // #1A2E4D
        f32 nameBorderR = 0.310f, nameBorderG = 0.765f, nameBorderB = 0.863f,
            nameBorderA = 0.95f;                                             // #4FC3DC
        f32 nameTextR = 0.910f, nameTextG = 0.957f, nameTextB = 1.000f,
            nameTextA = 1.00f;                                               // #E8F4FF

        f32 textR = 0.910f, textG = 0.957f, textB = 1.000f, textA = 1.00f;   // #E8F4FF

        f32 btnBgR = 0.102f, btnBgG = 0.180f, btnBgB = 0.302f;               // #1A2E4D
        f32 btnBorderR = 0.310f, btnBorderG = 0.765f, btnBorderB = 0.863f;   // #4FC3DC
        f32 btnTextR = 0.690f, btnTextG = 0.847f, btnTextB = 0.933f,
            btnTextA = 0.80f;                                                // #B0D8EE

        // ═══════════════════════════════════════════════════════════════
        // Fuente de la caja
        // ═══════════════════════════════════════════════════════════════
        // Dos mecanismos coexisten — capas distintas, no se pisan:
        //
        //   fontPath  (ruta cruda, key 'fuente'/'font' en .eclips):
        //     baseline del juego. Se hornea en m_textRenderer al cargar tema.
        //     Una fuente, todo el juego. Mecanismo histórico, intocable por
        //     compat (§7.14).
        //
        //   fontName  (identificador del FontManager, key 'caja_fuente'/'box_font'):
        //     override per-caja. Se resuelve en render-time vía m_fm->get(name).
        //     Hot-reload-safe (§1.5: identidad lógica, no instancia física).
        //     Requiere que el .ecl haya declarado 'fuente nombre "ruta" tamano=N'.
        //     Si no se encuentra → fallback a la fuente base + aviso 1× (§1.31).
        //
        // Precedencia (decisión del consumidor, §7.25):
        //   Cuerpo:     {fuente=Y} inline > caja_fuente > fontPath baseline
        //   Nameplate:  nombre_fuente > caja_fuente > fontPath baseline
        //   (nombre_fuente es para la placa lo que el inline es para el cuerpo:
        //    el override más específico. Sin él, la placa hereda caja_fuente.)
        //
        // Scope: cuerpo (caja_fuente) + nameplate (caja_fuente, o nombre_fuente
        // si diverge, C-NOMBRE). Choices y QTE rutean por resolveChoiceFont /
        // resolveQteFont (keys opciones_fuente / qte_fuente, C-FUENTE-CHROME),
        // con fallback a base. Botones del bottom-bar e historial siguen base
        // (chrome, §7.70). Tamaño per-caja queda fuera — el atlas es
        // atómico (§7.43), el tamaño lo define la fuente nombrada.
        std::string fontPath;   // key: fuente / font          — ruta cruda
        std::string fontName;   // key: caja_fuente / box_font — id del FontManager
        std::string nameFontName; // key: nombre_fuente / name_font — id FontManager,
        // override SOLO de la placa de nombre (C-NOMBRE). Precedencia
        // placa: nombre_fuente > caja_fuente > m_name crudo. Tamaño
        // horneado en la fuente nombrada (§7.43), como caja_fuente.
        std::string choiceFontName; // key: opciones_fuente / choices_font — id FontManager.
        // Fuente de los botones de elegir (C-FUENTE-CHROME). Precedencia:
        // opciones_fuente > base. NO pasa por caja_fuente — los choices nunca
        // lo siguieron (usaban m_text crudo) → preserva compat.
        std::string qteFontName; // key: qte_fuente / qte_font — id FontManager.
        // Fuente de la tecla/hint de QTE (C-FUENTE-CHROME). Precedencia: qte_fuente > base.
        std::string chromeFontName; // key: boton_fuente / ui_btn_font — id FontManager.
        // Fuente del bottom-bar (AUTO/SKIP/SAVE/LOAD/LOG/MENU). Precedencia:
        // boton_fuente > base. NO pasa por caja_fuente — el chrome siempre usó
        // m_name crudo → caer directo a base preserva compat.

        // Kerning/tracking de la fuente BASELINE (px sumados al advance de cada
        // glifo; negativo = aprieta). Se hornea en m_textRenderer + m_nameRenderer
        // al cargar tema (ambos comparten fontPath → misma fuente → mismo kerning).
        // La divergencia (namebox en otra fuente) va por nombre_fuente, cuya fuente
        // nombrada hornea su propio kerning. Aplica SOLO junto a fuente custom.
        f32 kerning = 0.0f;   // key: espaciado_letras / letter_spacing  (interno: kerning, por brevedad)

        // ── Interlineado: la pareja VERTICAL de kerning ────────────────────
        // px SIGNADOS sumados al PASO entre líneas (negativo aprieta).
        // Centinela 0 = heredar lo que dicen las métricas del ttf.
        //
        // NO es propiedad del atlas como kerning, y NO es un filtro de fuente
        // como textSize: es LAYOUT puro. Por eso no viaja por `init` ni por
        // `getSized` — se pasa como parámetro en el sitio que apila líneas, y
        // el default 0 deja bit-idéntico a todo el que no la nombra.
        //
        // Ojo con la distinción que el nombre no dice: esto suma al PASO, no al
        // ALTO DE CAJA de una línea. `TextRenderer::getLineHeight()` sigue
        // significando lo de siempre — lo consumen ~30 sitios de CENTRADO
        // ((h-lh)/2) y de chrome, y ninguno de ellos debe moverse por esta clave.
        //
        // Nació medida (lote `interlineado`, 2026-08-01): con la misma frase de
        // tres líneas, DDLC pasa 41 px de pantalla entre topes de línea y el
        // motor 39.5 → faltan ~1.6 px virtuales. NO es el tamaño de fuente (la
        // mayúscula ya da 23.5 contra 23.5, exacto) ni redondeo (ceil(39.5)=40,
        // no 41): Ren'Py suma leading POR ENCIMA de lo que dice el ttf.
        f32 lineSpacing = 0.0f;   // key: interlineado / line_spacing

        // ── La ENTRADA de la caja (`caja-entrada`) ────────────────────────
        // Segundos que tarda la caja en aparecer FUNDIENDO, con su texto.
        // Centinela 0 = apagado: la caja aparece de golpe, como siempre. Es el
        // mismo centinela que textSize/lineSpacing, y acá además cae solo — la
        // primera rama de `evalScalarRamp` devuelve el destino si `dur<=0`.
        //
        // La caja NO es un sprite: son ~20 dibujos con cuatro resoluciones de
        // fuente. El fundido no se aplica dibujo por dibujo sino abriendo un
        // ÁMBITO (`GroupAlpha.h`) alrededor de todo `renderDialogBox`, así que
        // "que entre todo junto" es una propiedad de la frontera y no una lista
        // que haya que mantener.
        //
        // Se dispara cuando el jugador NO estaba viendo la caja — no cuando
        // cambia la fase del VN. Medido al abrir el lote sobre el corpus vivo
        // (134 `.ecl`): 232 apariciones de verdad contra 923 líneas seguidas;
        // por fase serían las 1155, cinco veces de más. El detalle de POR QUÉ
        // sigue en `VNDialogBox.h`, arriba de `m_appear`; el MECANISMO se mudó
        // en el lote 2 a `engine/render/AppearPolicy.h`, que además captura
        // este valor al armar la aparición: cambiar de caja a media entrada ya
        // no mueve la rampa en curso (ley 3, `harness_aparicion` §3).
        //
        // Deliberadamente NO trae dirección ni curva: `caja_aparicion_desde` y
        // una entrada deslizada son el lote siguiente, y el ámbito ya está
        // hecho para crecer a un `AnimOutput` entero (dx/dy/escala) sin tocar
        // los call-sites. Un ticket por sesión.
        //
        // Y cuando ese eje llegue, su vocabulario NO se inventa: el motor ya lo
        // tiene escrito en otra superficie. `MapData.h:3659` (el panel de fin de
        // nivel de `.eclmap`) declara `animacion_entrada` / `entry_animation`
        // con los valores `desvanecer` · `desde_arriba` · `desde_abajo` · `zoom`
        // · `ninguna` — que es, palabra por palabra, la entrada deslizada. Se
        // cita, no se re-nombra (mesa `vocabulario-disperso`, `HO-9`/`FO-3`).
        //
        // OJO al citarlo, que es un fork abierto y no un detalle: esa familia
        // pone el evento SEGUNDO (`duracion_entrada` = <magnitud>_<evento>) y
        // ésta lo pone PRIMERO (`caja_aparicion_duracion` = <sujeto>_<evento>_
        // <magnitud>), igual que `indicador_avance_vaiven_periodo`. Hoy el
        // creador no puede predecir el orden entre dialectos. Sin decidir a
        // propósito: `FO-5`, con gatillo en R1 o en este mismo lote siguiente.
        f32 fadeInDuration = 0.0f;   // key: caja_aparicion_duracion / box_entry_duration

        // ── El RETRASO DEL TEXTO (`caja-entrada`, fork 5a) ────────────────
        // Segundos que el typewriter espera antes de la PRIMERA letra cuando
        // la caja aparece. 0 = ninguno (el texto arranca con la caja).
        //
        // Es el caso de DDLC, que NO funde: entra de golpe y lo que se retrasa
        // son las letras. O sea que `caja_aparicion_duracion 0` +
        // `caja_aparicion_texto_retraso 0.3` es la referencia, y esta clave y su
        // hermana de arriba son efectos DISTINTOS que se pueden combinar o usar
        // sueltos.
        //
        // Se arma en la APARICIÓN, no en cada línea — mismo disparador que el
        // fundido. Si se armara siempre, sería un cambio de ritmo global y eso
        // ya tiene su perilla (la cadena de velocidad, 6 niveles).
        //
        // NO es la única fuente: una línea puede pedir el suyo escribiendo
        // `{pausa=N}` al principio, y ESA gana (el más específico manda, misma
        // ley que la cadena de velocidad). Las dos terminan escribiendo el
        // mismo `m_pauseTimer` — un solo mecanismo, dos puertas.
        //
        // El nombre cuelga de `caja_aparicion_*` y no de un prefijo propio a
        // propósito: describe el MISMO evento que su hermana de arriba. Con el
        // nombre viejo (`caja_texto_retraso`) la familia tenía dos prefijos para
        // un evento y no podía crecer sin dispersarse otra vez.
        f32 textDelay = 0.0f;        // key: caja_aparicion_texto_retraso / box_entry_text_delay

        // ── Tamaño per-caja (FASE 2) ──────────────────────────────────────
        // Sentinela 0 = heredar (cuerpo → baseline 34 / placa → baseline 26).
        // NO es propiedad del atlas como kerning: el tamaño se aplica EN
        // RENDER-TIME como filtro sobre la fuente ya resuelta (sizedFont),
        // así compone con caja_fuente (Fork 3a) — texto_tamano=30 sobre
        // caja_fuente=Blackrush = Blackrush@30. El atlas anónimo lo crea
        // FontManager::getSized bajo demanda (§7.43: (ttf,tamaño) = recurso).
        f32 textSize = 0.0f;  // key: texto_tamano / text_size  — cuerpo
        f32 nameSize = 0.0f;  // key: nombre_tamano / name_size — placa de nombre
        f32 btnSize = 0.0f;  // key: boton_tamano / ui_btn_size — bottom-bar (0 = heredar; reescala boton_fuente, no la geometría boton_altura)

        bool hasCustomFont() const { return !fontPath.empty(); }

        // ═══════════════════════════════════════════════════════════════
        // Sprites PNG de la caja (override opcional de las primitivas)
        // ═══════════════════════════════════════════════════════════════
        // Si están seteados, el render reemplaza la primitiva correspondiente
        // (rounded rect de la caja / rect del nameplate) por el PNG escalado
        // bilineal al rectángulo que ya calcula el layout. Cada uno es
        // independiente: podés declarar PNG solo para la caja, solo para el
        // nameplate, ambos, o ninguno. Si el path es inválido, fallback a la
        // primitiva del theme con aviso pedagógico (pasada 3).
        //
        // Pasada 1 (esta): solo se parsean los paths. Sin carga ni render.
        std::string boxSpritePath;        // key: caja_sprite / box_sprite
        std::string namePlateSpritePath;  // key: nombre_sprite / nameplate_sprite

        bool hasBoxSprite()       const { return !boxSpritePath.empty(); }
        bool hasNamePlateSprite() const { return !namePlateSpritePath.empty(); }

        // ═══════════════════════════════════════════════════════════════
        // 9-slice scaling para los sprites PNG (override del stretch puro)
        // ═══════════════════════════════════════════════════════════════
        // Bordes en pixels del PNG fuente que NO deben estirarse al escalar
        // la caja al rect del layout. Patrón estándar de UI escalable:
        // textura dividida en 9 regiones (4 esquinas + 4 bordes + centro).
        // Esquinas tamaño nativo, bordes laterales escalan en una dimensión,
        // borde top/bottom en la otra, centro estira libre.
        //
        // Aplica solo cuando hay sprite declarado (caja_sprite / nombre_sprite).
        // Default 0 = stretch puro = comportamiento de pasada 3 (compat 100%).
        //
        // Source-overflow check (bL+bR > texW o bT+bB > texH) se valida en
        // pinThemeSprites: warn pedagógico una vez + clear a 0 (fallback stretch).
        // Destination-overflow check (bL+bR > destW) se hace silente dentro de
        // drawTexture9Slice (fallback a single-quad).
        float boxSliceLeft = 0.0f;     // caja_sprite_borde_izq / box_sprite_border_left
        float boxSliceRight = 0.0f;    // caja_sprite_borde_der / box_sprite_border_right
        float boxSliceTop = 0.0f;      // caja_sprite_borde_arriba / box_sprite_border_top
        float boxSliceBottom = 0.0f;   // caja_sprite_borde_abajo / box_sprite_border_bottom

        float namePlateSliceLeft = 0.0f;
        float namePlateSliceRight = 0.0f;
        float namePlateSliceTop = 0.0f;
        float namePlateSliceBottom = 0.0f;

        bool hasBox9Slice() const
        {
            return boxSliceLeft > 0.0f || boxSliceRight > 0.0f
                || boxSliceTop > 0.0f || boxSliceBottom > 0.0f;
        }
        bool hasNamePlate9Slice() const
        {
            return namePlateSliceLeft > 0.0f || namePlateSliceRight > 0.0f
                || namePlateSliceTop > 0.0f || namePlateSliceBottom > 0.0f;
        }

        // ═══════════════════════════════════════════════════════════════
        // Geometría de la caja de diálogo (renombrado: pierden prefijo)
        // ═══════════════════════════════════════════════════════════════
        // Antes (en Theme): dialogBoxHeight, dialogBoxMarginBottom, etc.
        // Ahora (en DialogBoxStyle): height, marginBottom, etc.
        // Razón: el nombre de la struct ya dice "DialogBox", el prefijo era
        // redundancia.
        float height = 185.0f;             // antes dialogBoxHeight
        float marginBottom = 30.0f;         // antes dialogBoxMarginBottom
        float widthRatio = 0.88f;           // antes dialogBoxWidthRatio (respecto al ancho virtual)
        float textPaddingLeft = 28.0f;      // antes dialogTextPaddingLeft
        float textPaddingRight = 28.0f;     // antes dialogTextPaddingRight
        float textPaddingTop = 18.0f;       // antes dialogTextPaddingTop

        // Placa del nombre (sin renombrado — namePlate* es sub-elemento)
        float namePlateHeight = 36.0f;
        float namePlateOffsetY = 4.0f;   // +baja la placa, −la sube (se suma a nY)
        float namePlatePaddingX = 16.0f;
        float namePlateTextOffsetY = 5.0f;
        // Ancho fijo de la placa (modo namebox DDLC). 0 = auto-fit al ancho del
        // nombre (default). >0 = ancho fijo en px virtuales, texto centrado; el
        // sprite (si hay) NO se deforma al largo del nombre. Pareja de namePlateHeight.
        float namePlateWidth = 0.0f;
        // Z-order de la placa respecto a la caja. false = delante (default,
        // como siempre). true = detrás (estilo DDLC: la pestaña asoma por
        // arriba de la caja). Decisión del creador — algunos themes la querrán
        // delante, otros detrás.
        bool  namePlateBehindBox = false;
        // Offset horizontal respecto al ancla de la caja. Se SUMA a namePlatePaddingX
        // → 0 = posición actual (top-left). Desliza el namebox a cualquier zona;
        // sigue a la caja si esta se mueve.
        float namePlateOffsetX = 0.0f;

        // ═══════════════════════════════════════════════════════════════
        // Botones de UI (Auto, Skip, Save, Load, Log, Menu)
        // ═══════════════════════════════════════════════════════════════
        float uiBtnHeight = 28.0f;
        float uiBtnPaddingX = 10.0f;
        float uiBtnGap = 6.0f;
        float uiBtnMarginRight = 16.0f;
        float uiBtnMarginBottom = 8.0f;
        bool  uiBtnAutoVisible = true;
        bool  uiBtnSkipVisible = true;
        bool  uiBtnSaveVisible = true;
        bool  uiBtnLoadVisible = true;
        bool  uiBtnLogVisible = true;
        bool  uiBtnMenuVisible = true;
        std::array<std::string, 6> uiBtnLabels = { "AUTO", "SKIP", "SAVE", "LOAD", "LOG", "MENU" };

        // ═══════════════════════════════════════════════════════════════
        // Choices
        // ═══════════════════════════════════════════════════════════════
        float choiceBtnWidthRatio = 0.55f;
        float choiceBtnHeight = 46.0f;
        float choiceBtnGap = 6.0f;
        // Juice de hover (F2a/F3): escala de la opción bajo el cursor.
        // 1.0 = apagado (engine neutro, sin movimiento impuesto); >1.0 = crece.
        // No es un branch — 1.0 hace target_hover == target_reposo, misma ruta.
        // choice_hover_escala / choice_hover_scale
        float choiceHoverScale = 1.0f;
        // Posición del bloque de opciones — ancla = CENTRO del bloque, en
        // (posX, posY) ratio del lienzo lógico. 0.5/0.5 = centro de pantalla
        // (clásico). Espeja qteBarPos*. El Modo Creador lo escribirá por arrastre.
        float choicePosXRatio = 0.5f;
        float choicePosYRatio = 0.5f;

        // Barra de timer de choice (solo aplica con timer activo)
        float choiceTimerBarHeight = 10.0f;
        float choiceTimerBarOffsetY = 14.0f;       // separación de la barra hasta los botones
        float choiceTimerBarBorderThickness = 1.5f;
        f32 choiceTimerBarBgR = 0.12f, choiceTimerBarBgG = 0.12f,
            choiceTimerBarBgB = 0.18f, choiceTimerBarBgA = 0.60f;
        f32 choiceTimerBarFillB = 0.15f;            // R y G son dinámicos (transición rojo→verde)
        f32 choiceTimerBarFillA = 0.85f;
        f32 choiceTimerBarBorderR = 0.40f, choiceTimerBarBorderG = 0.40f,
            choiceTimerBarBorderB = 0.50f, choiceTimerBarBorderA = 0.50f;
        // Aspecto del botón de choice (cuando NO está hover)
        f32 choiceBtnBgR = 0.10f, choiceBtnBgG = 0.14f,
            choiceBtnBgB = 0.32f, choiceBtnBgA = 0.90f;
        f32 choiceBtnBorderR = 0.30f, choiceBtnBorderG = 0.55f,
            choiceBtnBorderB = 1.00f, choiceBtnBorderA = 0.85f;
        float choiceBtnBorderThickness = 2.0f;
        f32 choiceBtnTextR = 1.00f, choiceBtnTextG = 1.00f,
            choiceBtnTextB = 1.00f, choiceBtnTextA = 1.00f;
        // ── Hover de choice (CP4 átomo) — colores completos, no modular-alpha.
        // Defaults = realce derivado del normal, para que el hover se vea sin
        // que el creador configure nada. text recupera el tinte azul del viejo
        // hardcode (VNDialogBox.cpp:725) ahora como default themeable.
        f32 choiceBtnHoverBgR = 0.18f, choiceBtnHoverBgG = 0.26f,
            choiceBtnHoverBgB = 0.50f, choiceBtnHoverBgA = 0.95f;
        f32 choiceBtnHoverBorderR = 0.50f, choiceBtnHoverBorderG = 0.75f,
            choiceBtnHoverBorderB = 1.00f, choiceBtnHoverBorderA = 1.00f;
        f32 choiceBtnHoverTextR = 0.85f, choiceBtnHoverTextG = 0.92f,
            choiceBtnHoverTextB = 1.00f, choiceBtnHoverTextA = 1.00f;

        // ── Cara PNG del botón de choice (override del rect de color) ──
        // Espeja boxSprite: sprite opcional + hover-sprite opcional + 9-slice.
        // Vacío → fallback al path de primitivas (rects). El sprite manda
        // (bg/border/text-color se ignoran cuando hay sprite, igual que el box).
        std::string choiceBtnSpritePath;       // key: choice_sprite
        std::string choiceBtnHoverSpritePath;  // key: choice_hover_sprite
        bool hasChoiceBtnSprite()      const { return !choiceBtnSpritePath.empty(); }
        bool hasChoiceBtnHoverSprite() const { return !choiceBtnHoverSpritePath.empty(); }

        // 9-slice del sprite de choice (default 0 = stretch puro, igual que box).
        float choiceBtnSliceLeft = 0.0f;    // choice_sprite_borde_izq / choice_sprite_border_left
        float choiceBtnSliceRight = 0.0f;   // choice_sprite_borde_der / choice_sprite_border_right
        float choiceBtnSliceTop = 0.0f;     // choice_sprite_borde_arriba / choice_sprite_border_top
        float choiceBtnSliceBottom = 0.0f;  // choice_sprite_borde_abajo / choice_sprite_border_bottom
        bool hasChoiceBtn9Slice() const
        {
            return choiceBtnSliceLeft > 0.0f || choiceBtnSliceRight > 0.0f
                || choiceBtnSliceTop > 0.0f || choiceBtnSliceBottom > 0.0f;
        }

        // ── Cara PNG del chrome del bottom-bar (boton_imagen) ──────────────
        // Plate COMPARTIDO por los 6 (AUTO/SKIP/SAVE/LOAD/LOG/MENU), espejo de
        // choices: idle + hover, label encima. Vacío → rect+borde de siempre.
        std::string chromeBtnSpritePath;       // key: boton_imagen / ui_btn_image
        std::string chromeBtnHoverSpritePath;  // key: boton_imagen_hover / ui_btn_image_hover
        bool hasChromeBtnSprite()      const { return !chromeBtnSpritePath.empty(); }
        bool hasChromeBtnHoverSprite() const { return !chromeBtnHoverSpritePath.empty(); }

        float chromeBtnSliceLeft = 0.0f;    // boton_imagen_borde_izq / ui_btn_image_border_left
        float chromeBtnSliceRight = 0.0f;   // boton_imagen_borde_der / ui_btn_image_border_right
        float chromeBtnSliceTop = 0.0f;     // boton_imagen_borde_arriba / ui_btn_image_border_top
        float chromeBtnSliceBottom = 0.0f;  // boton_imagen_borde_abajo / ui_btn_image_border_bottom
        bool hasChromeBtn9Slice() const
        {
            return chromeBtnSliceLeft > 0.0f || chromeBtnSliceRight > 0.0f
                || chromeBtnSliceTop > 0.0f || chromeBtnSliceBottom > 0.0f;
        }

        // Sonido del botón de choice (one-shot). Vacío → silencio. Lo dispara
        // VNScriptState (la caja es renderer puro): hover en el flanco, click
        // en la costura ChoiceMade.
        std::string choiceBtnHoverSound;  // choice_sonido_hover / choice_sound_hover
        std::string choiceBtnClickSound;  // choice_sonido_click / choice_sound_click

        // ═══════════════════════════════════════════════════════════════
        // Tarjeta de ranura (slot-widget Tier-A) — chrome cosmético
        // ═══════════════════════════════════════════════════════════════
        // Familia paralela a choiceBtn*: la `ranura` lee estos campos de
        // activeStyle() (theme-global, igual que choice/chrome). Todos los
        // idle = los literales que renderSlotCard tenia hardcodeados → render
        // byte-identico sin keys. Todos los hover default = su idle → off por
        // IDENTIDAD (§7.182), cero branch: un tema que no opta no se mueve.
        // El hover de texto es FAN-OUT (§7.166): una sola key slot_hover_text_color
        // escribe los 3 (escena/fecha/preview) → "todas las letras al rosita" (DDLC).
        // Keys: Modelo A bilingue puro — ranura_* (es) / slot_* (en).

        // ── Ocupado · idle ──
        f32 slotCardBgR = 0.12f, slotCardBgG = 0.14f, slotCardBgB = 0.20f, slotCardBgA = 0.92f;
        f32 slotCardBorderR = 0.31f, slotCardBorderG = 0.76f, slotCardBorderB = 0.86f, slotCardBorderA = 0.85f;
        float slotCardBorderThickness = 2.0f;
        f32 slotCardSceneR = 0.95f, slotCardSceneG = 0.97f, slotCardSceneB = 1.00f, slotCardSceneA = 1.00f;
        f32 slotCardDateR = 0.60f, slotCardDateG = 0.66f, slotCardDateB = 0.74f, slotCardDateA = 0.95f;
        f32 slotCardPreviewR = 0.72f, slotCardPreviewG = 0.76f, slotCardPreviewB = 0.82f, slotCardPreviewA = 0.90f;

        // ── Ocupado · hover (default = idle → identidad) ──
        f32 slotCardHoverBgR = 0.12f, slotCardHoverBgG = 0.14f, slotCardHoverBgB = 0.20f, slotCardHoverBgA = 0.92f;
        f32 slotCardHoverBorderR = 0.31f, slotCardHoverBorderG = 0.76f, slotCardHoverBorderB = 0.86f, slotCardHoverBorderA = 0.85f;
        // Texto al hover — fan-out de slot_hover_text_color a los 3:
        f32 slotCardHoverSceneR = 0.95f, slotCardHoverSceneG = 0.97f, slotCardHoverSceneB = 1.00f, slotCardHoverSceneA = 1.00f;
        f32 slotCardHoverDateR = 0.60f, slotCardHoverDateG = 0.66f, slotCardHoverDateB = 0.74f, slotCardHoverDateA = 0.95f;
        f32 slotCardHoverPreviewR = 0.72f, slotCardHoverPreviewG = 0.76f, slotCardHoverPreviewB = 0.82f, slotCardHoverPreviewA = 0.90f;

        // ── Vacio · idle ──
        f32 slotCardEmptyBgR = 0.10f, slotCardEmptyBgG = 0.10f, slotCardEmptyBgB = 0.13f, slotCardEmptyBgA = 0.80f;
        f32 slotCardEmptyBorderR = 0.30f, slotCardEmptyBorderG = 0.30f, slotCardEmptyBorderB = 0.36f, slotCardEmptyBorderA = 0.55f;
        float slotCardEmptyBorderThickness = 1.5f;
        f32 slotCardEmptyTextR = 0.50f, slotCardEmptyTextG = 0.50f, slotCardEmptyTextB = 0.56f, slotCardEmptyTextA = 0.80f;

        // ── Vacio · hover (default = idle → identidad) ──
        f32 slotCardEmptyHoverBgR = 0.10f, slotCardEmptyHoverBgG = 0.10f, slotCardEmptyHoverBgB = 0.13f, slotCardEmptyHoverBgA = 0.80f;
        f32 slotCardEmptyHoverBorderR = 0.30f, slotCardEmptyHoverBorderG = 0.30f, slotCardEmptyHoverBorderB = 0.36f, slotCardEmptyHoverBorderA = 0.55f;
        f32 slotCardEmptyHoverTextR = 0.50f, slotCardEmptyHoverTextG = 0.50f, slotCardEmptyHoverTextB = 0.56f, slotCardEmptyHoverTextA = 0.80f;

        // ── Etiqueta del slot vacio (texto autorable) ──────────────────────
        // El rotulo que se centra en la ranura vacia. Default byte-identico al
        // "Vacio" hardcoded → off por identidad (§7.182). El creador lo cambia a
        // "empty slot", "Vacío" (con tilde), o lo que quiera. Centrado dinamico.
        std::string slotCardEmptyLabel = "Vacio";

        // ── Vacio · inactiva (6.6) · opacidad del estado deshabilitado ──────
        // Cuando la accion de la ranura es INERTE sobre un slot vacio (cargar/
        // borrar = no-op; ver dispatchSlotAction) el tema puede marcar la vacia
        // como "deshabilitada": se ATENUA a este factor Y enmudece (no reacciona
        // al hover, sin barra). Es el azul-apagado + "empty slot" gris de Ren'Py
        // en Load. Default 1.0 = OFF POR IDENTIDAD (§7.182): no atenua, hover
        // normal, no-op de siempre intacto. <1.0 enciende el estado inactiva.
        // 'guardar' (vacio = destino valido) NUNCA se atenua. El nombre dice
        // opacidad, pero el estado tambien enmudece (§7.116: el doc lo aclara).
        // Key: ranura_vacia_inactiva_opacidad / slot_empty_inactive_opacity.
        float slotCardEmptyInactiveOpacity = 1.0f;

        // ── Barra de hover (4a senal de foco, asset-free) · solo hover, comun
        // a ocupada/vacia. Off por grosor 0 (sin key = byte-identico). Rect
        // vertical pegado al borde izq; color default = cyan de acento.
        f32 slotCardHoverBarR = 0.31f, slotCardHoverBarG = 0.76f, slotCardHoverBarB = 0.86f, slotCardHoverBarA = 1.00f;
        float slotCardHoverBarThickness = 0.0f;   // 0 = off

        // ── Formato de fecha de la ranura (CP2 desacople) ──────────────────
        // Plantilla de tokens {x} bilingües sobre el epoch del save. Default
        // byte-identico al strftime "%d/%m/%Y  %H:%M" (dos espacios). Vocabulario
        // = el de resolveVariable sin el _: dia[_pad], mes[_pad], anio, hora[_pad],
        // minuto[_pad], dia_semana (y day/month/year/hour/minute/weekday en ingles).
        std::string slotDateFormat = "{dia_pad}/{mes_pad}/{anio}  {hora_pad}:{minuto_pad}";

        // ── Idioma de los NOMBRES de la fecha (idiomas CP3 · DI-4) ─────────
        // Vacío = `auto`: los nombres siguen al idioma activo del jugador.
        // Un código fijo (`en`) da el look Ren'Py — "siempre inglés aunque
        // cambies de idioma". Solo afecta {mes_nombre}/{dia_nombre}; los
        // números son universales.
        //
        // Funciona AUN sin sistema de idiomas: un creador sin la key `idiomas`
        // que escriba `ranura_fecha_idioma en` + `{mes_nombre}` obtiene "July"
        // directo de tabla. Esa es la proporcionalidad fina que el §4 firma.
        //
        // La grafía inglesa es `slot_date_language` y NO `slot_date_lang`:
        // DI-4, por simetría con `slot_date_format` — en esta familia no hay
        // una sola abreviatura previa.
        std::string slotDateLanguage = "";   // "" = auto (sigue al jugador)

        // ── Sprite (cara PNG) de la tarjeta OCUPADA · idle/hover + 9-slice ──
        // Solo ocupada: Ren'Py/DDLC pintan la vacia como rect plano (color), no
        // hay caller de sprite vacio (§7.118). drawButtonBg cae a color si el
        // path esta vacio → sin sprite, idéntico a CP1.
        std::string slotCardSpritePath;       // key: ranura_sprite / slot_sprite
        std::string slotCardHoverSpritePath;  // key: ranura_sprite_hover / slot_sprite_hover
        bool hasSlotCardSprite()      const { return !slotCardSpritePath.empty(); }
        bool hasSlotCardHoverSprite() const { return !slotCardHoverSpritePath.empty(); }

        float slotCardSliceLeft = 0.0f;    // ranura_sprite_borde_izq / slot_sprite_border_left
        float slotCardSliceRight = 0.0f;   // ranura_sprite_borde_der / slot_sprite_border_right
        float slotCardSliceTop = 0.0f;     // ranura_sprite_borde_arriba / slot_sprite_border_top
        float slotCardSliceBottom = 0.0f;  // ranura_sprite_borde_abajo / slot_sprite_border_bottom
        bool hasSlotCard9Slice() const
        {
            return slotCardSliceLeft > 0.0f || slotCardSliceRight > 0.0f
                || slotCardSliceTop > 0.0f || slotCardSliceBottom > 0.0f;
        }

        // ── Thumbnail · caja de fit (CP2 · ranura-norte) ────────────────────
        // aspecto: FORMA fija de la caja del thumb (ancho/alto). 0 = AUTO → la
        // caja sigue el aspecto de cada captura (byte-identico a hoy, off por
        // identidad §7.182). >0 = caja fija (uniforme entre slots) → el ajuste
        // decide como cae la captura. Key: ranura_thumb_aspecto/slot_thumb_aspect.
        float slotThumbAspect = 0.0f;
        // ajuste: modo cuando la caja es fija. Default Cubrir (llena + recorta,
        // look Ren'Py/DDLC). Durmiente mientras aspecto=auto.
        // Key: ranura_thumb_ajuste / slot_thumb_fit.
        TextureFit slotThumbFit = TextureFit::Cubrir;

        // ── Layout de la tarjeta (CP2 · ranura-norte) ──────────────────────
        // Inline (default) = thumb izq + texto derecha (hoy, off por identidad).
        // Vertical = thumb ancho arriba + texto apilado (look DDLC/Ren'Py).
        // Key: ranura_layout / slot_layout (inline|horizontal / vertical).
        SlotLayout slotLayout = SlotLayout::Inline;

        // ═══════════════════════════════════════════════════════════════
        // QTE
        // ═══════════════════════════════════════════════════════════════
        float qteBarWidthRatio = 0.45f;
        float qteBarHeight = 14.0f;
        float qteKeyBoxSize = 80.0f;
        float qteBarPosYRatio = 0.30f;
        float qteBarPosXRatio = 0.50f;   // 0.5 = centro (clásico). Simétrico a pos_y; mueve el QTE entero.
        float qteBarSpacing = 30.0f;

        // ═══════════════════════════════════════════════════════════════
        // Indicador de avance al final del diálogo (auto-bar y ">")
        // ═══════════════════════════════════════════════════════════════
        // Barra de progreso del modo Auto
        float autoBarWidth = 60.0f;
        float autoBarHeight = 6.0f;
        float autoBarMarginX = 16.0f;        // desde el lado izquierdo de la caja
        float autoBarMarginBottom = 18.0f;   // desde el fondo de la caja
        f32 autoBarBgR = 0.20f, autoBarBgG = 0.25f, autoBarBgB = 0.35f, autoBarBgA = 0.60f;
        f32 autoBarFillR = 0.45f, autoBarFillG = 0.80f, autoBarFillB = 0.55f, autoBarFillA = 0.85f;

        // Indicador ">" cuando el diálogo espera avance manual
        float advanceIndicatorOffsetX = 20.0f;       // desde el lado izquierdo
        float advanceIndicatorOffsetBottom = 30.0f;  // desde el fondo de la caja
        f32 advanceIndicatorR = 0.70f, advanceIndicatorG = 0.80f,
            advanceIndicatorB = 1.00f, advanceIndicatorA = 0.90f;
        std::string advanceIndicatorGlyph = ">";     // glifo del indicador (configurable)
        bool        advanceIndicatorBlink = false;   // animar parpadeo (alpha senoidal)
        float       advanceIndicatorBlinkSpeed = 2.0f; // Hz (parpadeos por segundo)

        // Vaivén (vaiven/nudge): desplaza el indicador en X (o Y) con
        // easeIn→easeOut (el ctc de DDLC). Offset PURO — compone con el blink
        // (alpha) que corre aparte. Default OFF = regresión cero.
        bool        advanceIndicatorNudge = false;          // on/off
        float       advanceIndicatorNudgeAmplitude = 5.0f;  // px (DDLC ctc = 5)
        float       advanceIndicatorNudgePeriod = 1.5f;     // seg por ciclo (DDLC)
        bool        advanceIndicatorNudgeVertical = false;  // false=eje X, true=eje Y
        bool        advanceIndicatorNudgeBrillo = false;    // alpha derivado de la pos del vaivén (soldado, ctc DDLC); manda sobre blink

        // Imagen opcional del indicador (C-CONTINUAR eje-1): si carga OK,
        // reemplaza al glifo; si falta/falla, fallback al glifo. Tamaño nativo
        // (coords lógicas), tint blanco fijo, el blink modula su alpha igual
        // que al glifo. Per-caja gratis (vive en este struct, lo resuelve activeStyle()).
        std::string advanceIndicatorImagePath;       // indicador_avance_imagen / advance_indicator_image
        float advanceIndicatorScale = 1.0f;          // multiplica el tamaño nativo de la imagen (720→1080 ≈ 1.5)
        bool hasAdvanceIndicatorImage() const { return !advanceIndicatorImagePath.empty(); }

        // ═══════════════════════════════════════════════════════════════
        // v3 — Esquinas redondeadas, outlines de texto, layout de botones UI
        // ═══════════════════════════════════════════════════════════════
        // Defaults a 0 / Box / Right preservan el comportamiento histórico:
        // radios=0 → drawRoundedRect cae a drawRect, thickness=0 → outline
        // se omite, align=Right + style=Box = el render actual.

        // Esquinas (en píxeles virtuales). Se clampan a min(w,h)/2 internamente.
        // Identidad Eclips: redondeo sutil. Themes que no quieran redondeo
        // ponen 0 explícitamente (ej. tema_undertale.eclips).
        float radius = 6.0f;             // antes dialogBoxRadius
        float namePlateRadius = 4.0f;

        // Outline del texto del NOMBRE (en la placa). Útil para el efecto
        // Pastel-VN clásico: fill rosa + stroke blanco grueso.
        f32 nameOutlineR = 0.0f, nameOutlineG = 0.0f,
            nameOutlineB = 0.0f, nameOutlineA = 0.0f;
        float nameOutlineThickness = 0.0f;

        // Outline del texto del DIÁLOGO. En el estilo pastel-VN el cuerpo va sin outline
        // (más legible), pero el campo está disponible para themes que lo
        // quieran (ej. Undertale dark con stroke blanco fino).
        f32 textOutlineR = 0.0f, textOutlineG = 0.0f,
            textOutlineB = 0.0f, textOutlineA = 0.0f;
        float textOutlineThickness = 0.0f;

        // Layout de los botones de UI dentro de la caja de diálogo.
        UIBtnAlign uiBtnAlign = UIBtnAlign::Right;
        UIBtnStyle uiBtnStyle = UIBtnStyle::Box;

        // ═══════════════════════════════════════════════════════════════
        // Entrada de texto ('entrada'/'input') — bloque + botón OK
        // ═══════════════════════════════════════════════════════════════
        // Ancla vertical del bloque prompt+buffer+OK (ratio del lienzo).
        // Default 0.40 = el centro-alto histórico (antes hardcode en
        // renderTextInput). Subir en touch para que el teclado no lo tape.
        float inputPosYRatio = 0.40f;          // entrada_pos_y / input_pos_y

        // Tamaño de fuente del texto del aviso (prompt + buffer + cursor).
        // Espeja texto_tamano/nombre_tamano: 0 = heredar baseline (guard
        // size<=0 de sizedFont). NO toca el OK (chrome, F2) ni la fuente
        // (sigue el base, no caja_fuente — F1).
        float inputSize = 0.0f;                // entrada_tamano / input_size

        // Botón OK (confirmar). Rect-default; sprite PNG opcional abajo.
        // Tamaño y offset = control absoluto del creador (sin auto-fit):
        // offset 0 = auto-centrado bajo el buffer; descentrar a propósito
        // permitido ("clickeás lo que ves", el hit-test sigue al rect real).
        std::string inputOkLabel = "OK";       // entrada_ok / input_ok
        float inputOkW = 140.0f;               // entrada_ok_ancho / input_ok_width
        float inputOkH = 48.0f;                // entrada_ok_alto  / input_ok_height
        float inputOkOffsetX = 0.0f;           // entrada_ok_offset_x / input_ok_offset_x
        float inputOkOffsetY = 0.0f;           // entrada_ok_offset_y / input_ok_offset_y
        float inputOkBorderThickness = 2.0f;   // entrada_ok_borde_grosor
        // Estilo: caja (rect/PNG + letra) o texto (letra sola; hover = swap de
        // color de texto, ya existe). 'texto' = el "OK" de DDLC. Reusa UIBtnStyle.
        UIBtnStyle inputOkStyle = UIBtnStyle::Box;   // entrada_ok_estilo / input_ok_style

        // Colores del OK — 3 estados (normal / hover / deshabilitado). El
        // deshab. lo usa 'requerido' con buffer vacío (DDLC grisea el OK).
        // Defaults: normal/hover = los del choice (familia UI); deshab. = apagado.
        f32 inputOkBgR = 0.10f, inputOkBgG = 0.14f, inputOkBgB = 0.32f, inputOkBgA = 0.90f;
        f32 inputOkBorderR = 0.30f, inputOkBorderG = 0.55f, inputOkBorderB = 1.00f, inputOkBorderA = 0.85f;
        f32 inputOkTextR = 1.00f, inputOkTextG = 1.00f, inputOkTextB = 1.00f, inputOkTextA = 1.00f;
        f32 inputOkHoverBgR = 0.18f, inputOkHoverBgG = 0.26f, inputOkHoverBgB = 0.50f, inputOkHoverBgA = 0.95f;
        f32 inputOkHoverBorderR = 0.50f, inputOkHoverBorderG = 0.75f, inputOkHoverBorderB = 1.00f, inputOkHoverBorderA = 1.00f;
        f32 inputOkHoverTextR = 0.85f, inputOkHoverTextG = 0.92f, inputOkHoverTextB = 1.00f, inputOkHoverTextA = 1.00f;
        f32 inputOkOffBgR = 0.10f, inputOkOffBgG = 0.10f, inputOkOffBgB = 0.12f, inputOkOffBgA = 0.50f;
        f32 inputOkOffBorderR = 0.30f, inputOkOffBorderG = 0.30f, inputOkOffBorderB = 0.35f, inputOkOffBorderA = 0.40f;
        f32 inputOkOffTextR = 0.55f, inputOkOffTextG = 0.55f, inputOkOffTextB = 0.60f, inputOkOffTextA = 0.60f;

        // Cara PNG del OK (override del rect). Espeja choice_sprite. Vacío →
        // rect (el default del motor). Nosotros sí usaremos PNG.
        std::string inputOkSpritePath;         // entrada_ok_sprite
        std::string inputOkHoverSpritePath;    // entrada_ok_hover_sprite
        bool hasInputOkSprite()      const { return !inputOkSpritePath.empty(); }
        bool hasInputOkHoverSprite() const { return !inputOkHoverSpritePath.empty(); }
        float inputOkSliceLeft = 0.0f, inputOkSliceRight = 0.0f, inputOkSliceTop = 0.0f, inputOkSliceBottom = 0.0f;
        bool hasInputOk9Slice() const {
            return inputOkSliceLeft > 0.0f || inputOkSliceRight > 0.0f
                || inputOkSliceTop > 0.0f || inputOkSliceBottom > 0.0f;
        }

        // ═══════════════════════════════════════════════════════════════
        // Marco/fondo del aviso ('entrada_marco'/'input_frame')
        // ═══════════════════════════════════════════════════════════════
        // Cuadrado DETRÁS del cluster prompt+buffer+OK del 'entrada', para
        // hacer legible el aviso sobre cualquier escena (el "pon tu nombre"
        // de DDLC). Decoración pura: NO clickeable, sin estados de runtime.
        // Default OFF — hoy el aviso flota sin fondo y eso se preserva; el
        // marco es opt-in. On + sprite → PNG (9-slice/stretch); on sin sprite
        // → cuadrado nativo + borde. Geometría = control absoluto del creador
        // (sin auto-fit; doctrina nombre_anchura), anclada en inputPosYRatio.
        // Se dibuja con drawButtonBg (mismo primitivo que la caja y el OK).
        bool inputFrameVisible = false;        // entrada_marco / input_frame

        float inputFrameW = 700.0f;            // entrada_marco_ancho / input_frame_width
        float inputFrameH = 320.0f;            // entrada_marco_alto  / input_frame_height
        float inputFrameOffsetX = 0.0f;        // entrada_marco_offset_x / input_frame_offset_x
        float inputFrameOffsetY = 0.0f;        // entrada_marco_offset_y / input_frame_offset_y
        float inputFrameBorderThickness = 2.0f;// entrada_marco_borde_grosor / input_frame_border_thickness

        // Cara nativa (cuando NO hay sprite). Default = espejo de la caja
        // (navy translúcido + borde cian), look coherente con el resto del VN.
        f32 inputFrameBgR = 0.059f, inputFrameBgG = 0.102f, inputFrameBgB = 0.180f, inputFrameBgA = 0.85f;
        f32 inputFrameBorderR = 0.310f, inputFrameBorderG = 0.765f, inputFrameBorderB = 0.863f, inputFrameBorderA = 0.95f;

        // Cara PNG (override del rect). Espeja boxSprite. Vacío → rect nativo.
        std::string inputFrameSpritePath;      // entrada_marco_sprite
        bool hasInputFrameSprite() const { return !inputFrameSpritePath.empty(); }
        float inputFrameSliceLeft = 0.0f, inputFrameSliceRight = 0.0f,
            inputFrameSliceTop = 0.0f, inputFrameSliceBottom = 0.0f;
        bool hasInputFrame9Slice() const {
            return inputFrameSliceLeft > 0.0f || inputFrameSliceRight > 0.0f
                || inputFrameSliceTop > 0.0f || inputFrameSliceBottom > 0.0f;
        }

    };

    // ═══════════════════════════════════════════════════════════════
    // Theme — feel global del juego + caja default
    // ═══════════════════════════════════════════════════════════════
    // Lo que queda en Theme tras pasada 1 §7.4:
    //   - DialogBoxStyle defaultDialogBox  (toda la caja)
    //   - Velocidad/auto/skip + locks      (feel del juego)
    //   - historyMaxEntries                (cap del historial)
    //   - sourcePath                       (de qué archivo se cargó)
    //
    struct Theme
    {
        // ═══════════════════════════════════════════════════════════════
         // Caja de diálogo default — todo el look visual
         // ═══════════════════════════════════════════════════════════════
         // (§7.4 pasada 2) Las cajas nominadas heredan parse-time del
         // defaultDialogBox vigente en su punto de declaración. Si el creador
         // no declara ningún bloque, namedDialogBoxes queda vacío y el motor
         // usa solo defaultDialogBox como antes (backward-compat 100%).
        DialogBoxStyle defaultDialogBox;

        // Plantillas nominadas declaradas con sintaxis "caja_dialogo X { ... }".
        // Snapshot parse-time del default + deltas del bloque. Lookup O(log n).
        // Vacío si el .eclips no declara ningún bloque.
        std::map<std::string, DialogBoxStyle> namedDialogBoxes;

        // ═══════════════════════════════════════════════════════════════
        // Comportamiento (feel del juego, no del look de la caja)
        // ═══════════════════════════════════════════════════════════════
        TextSpeed textSpeed = textSpeedDefault();
        TextSpeedDomain textSpeedDomain = textSpeedLegacyDomain();
        float autoDelay = 2.0f;
        float skipInterval = 0.05f;
        // Skip-de-leído (Ren'Py "skip unseen text"): si false (default), el skip
        // (Ctrl) solo atraviesa texto YA visto y frena en seco al topar texto
        // nuevo. Si true, atraviesa todo. Es feel del juego → vive en Theme.
        bool  skipUnseen = false;
        float interruptDelay = 0.15f;   // respiro (s) tras {interrumpir} antes de saltar

        // Locks del patrón override — el creador puede impedir que el jugador
        // toque estos campos desde el menú de Settings. Si true, la UI del
        // menú ocultará (o deshabilitará) el slider correspondiente.
        // En 0.2 D solo se persisten/parsean; la UI los honrará en próxima
        // sesión. Útil para juegos como Undertale donde la velocidad de
        // texto es decisión inamovible del creador (parte del feel).
        bool textSpeedLocked = false;
        bool autoDelayLocked = false;
        bool skipIntervalLocked = false;
        // Un juego de terror/narrativo puede forzar skip-solo-visto
        // (skipUnseen=false + lock) para que no le adelanten sustos no leídos.
        bool skipUnseenLocked = false;

        int   historyMaxEntries = 100;

        // ═══════════════════════════════════════════════════════════════
        // Hotspot hover outline (CP5 átomo) — visual de ESCENA, global.
        // NO vive en DialogBoxStyle: un hotspot no pertenece a ninguna caja.
        // Default OFF (alpha 0): los hotspots de VN suelen ser exploratorios;
        // el creador activa el outline vía hotspot_hover_color si lo quiere.
        // ═══════════════════════════════════════════════════════════════
        f32 hotspotHoverR = 1.0f, hotspotHoverG = 1.0f,
            hotspotHoverB = 0.2f, hotspotHoverA = 0.0f;   // alpha 0 = off
        float hotspotHoverThickness = 2.0f;

        // ═══════════════════════════════════════════════════════════════
        // Metadata
        // ═══════════════════════════════════════════════════════════════
        std::string sourcePath;

        // ═══════════════════════════════════════════════════════════════
        // Constructor por defecto y carga desde archivo
        // ═══════════════════════════════════════════════════════════════
        static Theme defaults() { return Theme{}; }

        static Theme loadFromFile(const std::string& path)
        {
            Theme t;
            t.sourcePath = path;

            std::ifstream file(path);
            if (!file.is_open())
            {
                std::cerr << "[Theme] No se pudo abrir: " << path << std::endl;
                return t;
            }

            // ── §7.4 pasada 2 — State machine para caja_dialogo X { ... } ──
            // 'd' es puntero a la DialogBoxStyle activa: arranca apuntando a
            // defaultDialogBox y se mueve a una entrada del map cuando se
            // abre un bloque "caja_dialogo X {". Vuelve al default al ver '}'.
            //
            // Herencia parse-time (decisión 2): al ver "caja_dialogo X {",
            // copiamos el defaultDialogBox VIGENTE EN ESE PUNTO al map.
            // Las keys dentro del bloque sobreescriben la copia. El orden
            // de declaración importa y es determinista — coherente con la
            // filosofía declarativa del editor visual a futuro.
            enum class State { TopLevel, InsideBox };
            State state = State::TopLevel;
            DialogBoxStyle* d = &t.defaultDialogBox;
            std::string currentBoxAlias;
            int blockStartLine = 0;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line))
            {
                ++lineNum;

                // Quitar comentarios — pero respetando colores hex (#RRGGBB).
                size_t commentPos = std::string::npos;
                for (size_t i = 0; i < line.size(); ++i)
                {
                    if (line[i] != '#') continue;
                    char next = (i + 1 < line.size()) ? line[i + 1] : '\0';
                    bool isHexDigit =
                        (next >= '0' && next <= '9') ||
                        (next >= 'a' && next <= 'f') ||
                        (next >= 'A' && next <= 'F');
                    if (!isHexDigit) { commentPos = i; break; }
                }
                if (commentPos != std::string::npos)
                {
                    if (commentPos == 0) continue;
                    line = line.substr(0, commentPos);
                }
                line = trim(line);
                if (line.empty()) continue;

                std::istringstream ss(line);
                std::string key;
                ss >> key;
                if (key.empty()) continue;

                // ── FOLD-PARTIDO: la clave del tema es vocabulario del MOTOR ──
                //
                // Hasta hoy este `ss >> key` entregaba la clave byte-exacta y
                // `Theme.h` no llamaba a `vocabToLower` NI a `asciiToLower` una
                // sola vez, o sea que el fold de SYN-4c-i nunca habia llegado a
                // este dialecto (ficha `TEMA-SIN-FOLD`, absorbida por
                // `FOLD-PARTIDO`). Un creador que escribia `CAJA_APARICION_
                // DURACION` recibia «key desconocida» de una clave que el motor
                // conoce perfectamente.
                //
                // Plegar ACA -en la extraccion, no en las 404 comparaciones- es
                // legitimo por dos medidas tomadas antes de tocar nada:
                //
                //   1. Los 404 literales de clave distintos de este archivo ya
                //      estan TODOS en minuscula, y plegar no crea un solo
                //      choque. Conjunto de colision: VACIO.
                //   2. NINGUN nombre inventado por el creador pasa por esta
                //      variable. El alias de `caja_dialogo mi_caja {` sale de
                //      `valueStr`, no de `key`, y este archivo no tiene una sola
                //      insercion en mapa con `[key]`. Es la diferencia con
                //      `ProjectManifest.h`, donde la MISMA variable sirve las
                //      dos cosas y por eso alla el fold va por bloque.
                key = eclips::vocabToLower(key);

                std::string valueStr;
                std::getline(ss, valueStr);
                valueStr = trim(valueStr);

                // ── Cierre de bloque '}' ──────────────────────────────
                if (key == "}")
                {
                    if (state == State::InsideBox)
                    {
                        state = State::TopLevel;
                        d = &t.defaultDialogBox;
                        currentBoxAlias.clear();
                    }
                    else
                    {
                        std::cerr << "[Theme] Aviso linea " << lineNum
                            << ": '}' suelto fuera de bloque caja_dialogo en "
                            << path << " -- ignorado." << std::endl;
                    }
                    continue;
                }

                // ── Apertura de bloque 'caja_dialogo X {' ──────────────
                if (key == "caja_dialogo" || key == "dialog_box")
                {
                    if (state == State::InsideBox)
                    {
                        std::cerr << "[Theme] Aviso linea " << lineNum
                            << ": ya hay un bloque 'caja_dialogo " << currentBoxAlias
                            << "' abierto desde linea " << blockStartLine
                            << ". Anidamiento no permitido. Linea ignorada."
                            << std::endl;
                        continue;
                    }
                    std::istringstream rest(valueStr);
                    std::string alias, brace;
                    rest >> alias >> brace;
                    // Caso 'caja_dialogo {' — alias quedó como "{" y brace vacío.
                    // Detectamos antes del check de brace para mensaje claro.
                    if (alias.empty() || alias == "{")
                    {
                        std::cerr << "[Theme] Error linea " << lineNum
                            << ": '" << key << "' sin alias en " << path
                            << ". Sintaxis: 'caja_dialogo nombre {'. Ignorado." << std::endl;
                        continue;
                    }
                    // Identificador SIN comillas (decisión 5). Si vienen,
                    // las quitamos como cortesia y avisamos.
                    if (alias.size() >= 2 && alias.front() == '"' && alias.back() == '"')
                    {
                        std::cerr << "[Theme] Aviso linea " << lineNum
                            << ": el alias del bloque va sin comillas (ej: 'caja_dialogo undertale {')."
                            << " Aceptado igual, pero quita las comillas." << std::endl;
                        alias = alias.substr(1, alias.size() - 2);
                    }
                    if (brace != "{")
                    {
                        std::cerr << "[Theme] Error linea " << lineNum
                            << ": '" << key << " " << alias
                            << "' debe ir seguido de '{' en " << path
                            << ". Ignorado." << std::endl;
                        continue;
                    }
                    if (t.namedDialogBoxes.count(alias) > 0)
                    {
                        std::cerr << "[Theme] Aviso linea " << lineNum
                            << ": caja_dialogo '" << alias
                            << "' ya estaba definida. Sobreescribiendo." << std::endl;
                    }
                    // Snapshot parse-time del default actual (decisión 2)
                    t.namedDialogBoxes[alias] = t.defaultDialogBox;
                    d = &t.namedDialogBoxes[alias];
                    currentBoxAlias = alias;
                    blockStartLine = lineNum;
                    state = State::InsideBox;
                    continue;
                }

                // ── Rechazo pedagogico de ':' decorativo ──────────────
                {
                    bool hasColon = false;
                    if (!key.empty() && key.back() == ':') hasColon = true;
                    else if (!valueStr.empty() && valueStr.front() == ':') hasColon = true;
                    if (hasColon)
                    {
                        std::cerr << "[.eclips] Aviso linea " << lineNum
                            << " en '" << path << "' (key '" << key << "'):\n"
                            << "  Los .eclips no usan ':' (eso es solo para .ecl).\n"
                            << "  Linea ignorada. Quita el ':' y vuelve a correr.\n"
                            << "  Mas info: ver \"Sobre los ':'\" en ECLIPSCRIPT_DOCS.md"
                            << std::endl;
                        continue;
                    }
                }

                // ── Pre-check: keys de Theme global dentro de bloque ──
                // Estas aplican al feel del juego (textSpeed, autoDelay, etc),
                // NO al look de la caja. Si el creador las pone dentro de un
                // caja_dialogo, aviso pedagogico explicando el motivo.
                if (state == State::InsideBox)
                {
                    bool isThemeOnly =
                        key == "velocidad_texto" || key == "text_speed" ||
                        key == "velocidad_texto_lock" || key == "text_speed_lock" ||
                        key == "auto_demora" || key == "auto_delay" ||
                        key == "auto_demora_lock" || key == "auto_delay_lock" ||
                        key == "skip_intervalo" || key == "skip_interval" ||
                        key == "skip_intervalo_lock" || key == "skip_interval_lock" ||
                        key == "skip_no_visto" || key == "skip_unseen" ||
                        key == "skip_no_visto_lock" || key == "skip_unseen_lock" ||
                        key == "historial_max" || key == "history_max" ||
                        key == "hotspot_hover_color" ||
                        key == "hotspot_hover_grosor" || key == "hotspot_hover_thickness";
                    if (isThemeOnly)
                    {
                        std::cerr << "[Theme] Aviso linea " << lineNum
                            << ": key '" << key << "' aplica al theme global (feel del juego)"
                            << ", no a la caja '" << currentBoxAlias
                            << "'. Sacala fuera del bloque caja_dialogo. Ignorada."
                            << std::endl;
                        continue;
                    }
                }

                // ── Cadena de despacho de keys ────────────────────────
                // C1061 (MSVC): la cadena if/else-if entera cuenta como
                // bloques anidados; pasado el tope, se parte en 2 con este
                // flag. headMatched = true salvo que NINGUNA rama de la 1ª
                // mitad case (su 'else' final lo baja). La 2ª mitad + el
                // catch-all §1.9 corren solo si headMatched quedó false.
                bool headMatched = true;

                // ── Colores ───────────────────────────────────────────
                if (key == "caja_color" || key == "box_color")
                    parseColor(valueStr, d->boxR, d->boxG, d->boxB, d->boxA);
                else if (key == "caja_opacidad" || key == "box_opacity")
                    d->boxA = parsePercent(valueStr);
                else if (key == "caja_borde" || key == "box_border")
                    parseColor(valueStr, d->boxBorderR, d->boxBorderG, d->boxBorderB, d->boxBorderA);
                else if (key == "nombre_fondo" || key == "name_bg")
                    parseColor(valueStr, d->namePlateR, d->namePlateG, d->namePlateB, d->namePlateA);
                else if (key == "nombre_opacidad" || key == "name_opacity")
                    d->namePlateA = parsePercent(valueStr);
                else if (key == "nombre_borde" || key == "name_border")
                    parseColor(valueStr, d->nameBorderR, d->nameBorderG, d->nameBorderB, d->nameBorderA);
                else if (key == "nombre_texto" || key == "name_text")
                    parseColor(valueStr, d->nameTextR, d->nameTextG, d->nameTextB, d->nameTextA);
                else if (key == "texto_color" || key == "text_color")
                    parseColor(valueStr, d->textR, d->textG, d->textB, d->textA);
                else if (key == "boton_fondo" || key == "btn_bg")
                {
                    f32 dummy = 1.0f;
                    parseColor(valueStr, d->btnBgR, d->btnBgG, d->btnBgB, dummy);
                }
                else if (key == "boton_borde" || key == "btn_border")
                {
                    f32 dummy = 1.0f;
                    parseColor(valueStr, d->btnBorderR, d->btnBorderG, d->btnBorderB, dummy);
                }
                else if (key == "boton_texto" || key == "btn_text")
                    parseColor(valueStr, d->btnTextR, d->btnTextG, d->btnTextB, d->btnTextA);
                else if (key == "fuente" || key == "font")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->fontPath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                }
                else if (key == "espaciado_letras" || key == "letter_spacing")
                    d->kerning = parseFloat(valueStr, d->kerning);  // px, signado (negativo aprieta)
                else if (key == "interlineado" || key == "line_spacing")
                    d->lineSpacing = parseFloat(valueStr, d->lineSpacing);  // px, signado; 0 = heredar del ttf
                // Renombradas por la mesa `vocabulario-disperso` (v0.8.26). Se
                // llamaban `caja_entrada`/`box_fade_in` y `caja_texto_retraso`/
                // `box_text_delay`; RETIRO LIMPIO, sin aliases (`D-11`). Las
                // cuatro viejas caen ahora en el aviso `key desconocida` de §1.9,
                // que es la forma correcta de retirarlas: hablan. Guardián del
                // retiro: `harness_tema` §7.
                //
                // ES y EN divergen a propósito y hay que dejarlo escrito o el
                // próximo lector "corrige" uno de los dos (`D-12`/`RO-8`):
                // `aparicion` porque `entrada` ya nombraba el campo de captura y
                // la curva `curva=entrada`; `entry` porque ya era el sustantivo
                // vivo del motor para esto mismo (`entry_duration`, MapData.h).
                else if (key == "caja_aparicion_duracion" || key == "box_entry_duration")
                    d->fadeInDuration = parseFloat(valueStr, d->fadeInDuration);  // segundos; 0 = sin entrada
                else if (key == "caja_aparicion_texto_retraso" || key == "box_entry_text_delay")
                    d->textDelay = parseFloat(valueStr, d->textDelay);  // segundos; 0 = el texto arranca con la caja
                else if (key == "texto_tamano" || key == "text_size")
                    d->textSize = parseFloat(valueStr, d->textSize);  // FASE 2: 0 = heredar baseline
                else if (key == "nombre_tamano" || key == "name_size")
                    d->nameSize = parseFloat(valueStr, d->nameSize);  // FASE 2: 0 = heredar baseline
                else if (key == "boton_tamano" || key == "ui_btn_size")
                    d->btnSize = parseFloat(valueStr, d->btnSize);  // 0 = heredar baseline (chrome)
                else if (key == "caja_fuente" || key == "box_font")
                {
                    // Identificador del FontManager. §7.44: token sin comillas.
                    // Si vienen, aviso pedagógico y parseamos sin ellas (la
                    // creadora aprende la convención, el archivo sigue cargando).
                    std::string val = valueStr;
                    if (!val.empty() && val.front() == '"')
                    {
                        size_t q1 = val.find('"');
                        size_t q2 = (q1 != std::string::npos) ? val.find('"', q1 + 1) : std::string::npos;
                        if (q1 != std::string::npos && q2 != std::string::npos)
                        {
                            val = val.substr(q1 + 1, q2 - q1 - 1);
                            std::cerr << "[Theme] Aviso linea " << lineNum
                                << ": '" << key << "' espera un identificador sin comillas (ej: "
                                << key << " pixel). Parseando sin las comillas." << std::endl;
                        }
                    }
                    d->fontName = val;
                }
                else if (key == "nombre_fuente" || key == "name_font")
                {
                    // Override de fuente SOLO para la placa de nombre (C-NOMBRE).
                    // Identificador del FontManager, §7.44: token sin comillas
                    // (mismo ritual que caja_fuente — strip + aviso si vienen).
                    std::string val = valueStr;
                    if (!val.empty() && val.front() == '"')
                    {
                        size_t q1 = val.find('"');
                        size_t q2 = (q1 != std::string::npos) ? val.find('"', q1 + 1) : std::string::npos;
                        if (q1 != std::string::npos && q2 != std::string::npos)
                        {
                            val = val.substr(q1 + 1, q2 - q1 - 1);
                            std::cerr << "[Theme] Aviso linea " << lineNum
                                << ": '" << key << "' espera un identificador sin comillas (ej: "
                                << key << " riffic). Parseando sin las comillas." << std::endl;
                        }
                    }
                    d->nameFontName = val;
                }
                else if (key == "opciones_fuente" || key == "choices_font")
                {
                    // Fuente de los botones de elegir (C-FUENTE-CHROME).
                    // Identificador del FontManager, §7.44: token sin comillas
                    // (mismo ritual que caja_fuente/nombre_fuente — strip + aviso).
                    std::string val = valueStr;
                    if (!val.empty() && val.front() == '"')
                    {
                        size_t q1 = val.find('"');
                        size_t q2 = (q1 != std::string::npos) ? val.find('"', q1 + 1) : std::string::npos;
                        if (q1 != std::string::npos && q2 != std::string::npos)
                        {
                            val = val.substr(q1 + 1, q2 - q1 - 1);
                            std::cerr << "[Theme] Aviso linea " << lineNum
                                << ": '" << key << "' espera un identificador sin comillas (ej: "
                                << key << " riffic). Parseando sin las comillas." << std::endl;
                        }
                    }
                    d->choiceFontName = val;
                }
                else if (key == "qte_fuente" || key == "qte_font")
                {
                    // Fuente de la tecla/hint de QTE (C-FUENTE-CHROME). Mismo ritual.
                    std::string val = valueStr;
                    if (!val.empty() && val.front() == '"')
                    {
                        size_t q1 = val.find('"');
                        size_t q2 = (q1 != std::string::npos) ? val.find('"', q1 + 1) : std::string::npos;
                        if (q1 != std::string::npos && q2 != std::string::npos)
                        {
                            val = val.substr(q1 + 1, q2 - q1 - 1);
                            std::cerr << "[Theme] Aviso linea " << lineNum
                                << ": '" << key << "' espera un identificador sin comillas (ej: "
                                << key << " riffic). Parseando sin las comillas." << std::endl;
                        }
                    }
                    d->qteFontName = val;
                }
                else if (key == "boton_fuente" || key == "ui_btn_font")
                {
                    // Fuente del bottom-bar. Mismo ritual §7.44 (identificador
                    // sin comillas, strip + aviso).
                    std::string val = valueStr;
                    if (!val.empty() && val.front() == '"')
                    {
                        size_t q1 = val.find('"');
                        size_t q2 = (q1 != std::string::npos) ? val.find('"', q1 + 1) : std::string::npos;
                        if (q1 != std::string::npos && q2 != std::string::npos)
                        {
                            val = val.substr(q1 + 1, q2 - q1 - 1);
                            std::cerr << "[Theme] Aviso linea " << lineNum
                                << ": '" << key << "' espera un identificador sin comillas (ej: "
                                << key << " riffic). Parseando sin las comillas." << std::endl;
                        }
                    }
                    d->chromeFontName = val;
                }
                // ── Sprites PNG (override opcional de las primitivas) ──
                else if (key == "caja_sprite" || key == "box_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->boxSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                }
                else if (key == "nombre_sprite" || key == "nameplate_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->namePlateSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                }

                // ── 9-slice borders (px del sprite que NO se estiran) ──
                // Default 0 = stretch puro. Validación de overflow en pinThemeSprites
                // (loud, una vez) y drawTexture9Slice (silente, defensivo).
                else if (key == "caja_sprite_borde_izq" || key == "box_sprite_border_left")
                    d->boxSliceLeft = parseFloat(valueStr, d->boxSliceLeft);
                else if (key == "caja_sprite_borde_der" || key == "box_sprite_border_right")
                    d->boxSliceRight = parseFloat(valueStr, d->boxSliceRight);
                else if (key == "caja_sprite_borde_arriba" || key == "box_sprite_border_top")
                    d->boxSliceTop = parseFloat(valueStr, d->boxSliceTop);
                else if (key == "caja_sprite_borde_abajo" || key == "box_sprite_border_bottom")
                    d->boxSliceBottom = parseFloat(valueStr, d->boxSliceBottom);
                else if (key == "nombre_sprite_borde_izq" || key == "nameplate_sprite_border_left")
                    d->namePlateSliceLeft = parseFloat(valueStr, d->namePlateSliceLeft);
                else if (key == "nombre_sprite_borde_der" || key == "nameplate_sprite_border_right")
                    d->namePlateSliceRight = parseFloat(valueStr, d->namePlateSliceRight);
                else if (key == "nombre_sprite_borde_arriba" || key == "nameplate_sprite_border_top")
                    d->namePlateSliceTop = parseFloat(valueStr, d->namePlateSliceTop);
                else if (key == "nombre_sprite_borde_abajo" || key == "nameplate_sprite_border_bottom")
                    d->namePlateSliceBottom = parseFloat(valueStr, d->namePlateSliceBottom);
                // ── Geometria de la caja (renombrados: pierden prefijo) ──
                else if (key == "caja_altura" || key == "box_height")
                    d->height = parseFloat(valueStr, d->height);
                else if (key == "caja_margen_inferior" || key == "box_bottom_margin")
                    d->marginBottom = parseFloat(valueStr, d->marginBottom);
                else if (key == "caja_ancho_ratio" || key == "box_width_ratio")
                    d->widthRatio = parseFloat(valueStr, d->widthRatio);
                else if (key == "texto_padding_izq" || key == "text_padding_left")
                    d->textPaddingLeft = parseFloat(valueStr, d->textPaddingLeft);
                else if (key == "texto_padding_der" || key == "text_padding_right")
                    d->textPaddingRight = parseFloat(valueStr, d->textPaddingRight);
                else if (key == "texto_padding_arriba" || key == "text_padding_top")
                    d->textPaddingTop = parseFloat(valueStr, d->textPaddingTop);
                // Placa de nombre (sub-elemento, sin renombrado)
                else if (key == "nombre_altura" || key == "name_height")
                    d->namePlateHeight = parseFloat(valueStr, d->namePlateHeight);
                else if (key == "nombre_anchura" || key == "name_width")
                    d->namePlateWidth = parseFloat(valueStr, d->namePlateWidth);
                else if (key == "nombre_detras" || key == "name_behind")
                    d->namePlateBehindBox = parseBool(valueStr, d->namePlateBehindBox);
                else if (key == "nombre_offset_y" || key == "name_offset_y")
                    d->namePlateOffsetY = parseFloat(valueStr, d->namePlateOffsetY);
                else if (key == "nombre_offset_x" || key == "name_offset_x")
                    d->namePlateOffsetX = parseFloat(valueStr, d->namePlateOffsetX);
                else if (key == "nombre_padding_x" || key == "name_padding_x")
                    d->namePlatePaddingX = parseFloat(valueStr, d->namePlatePaddingX);
                else if (key == "nombre_texto_offset_y" || key == "name_text_offset_y")
                    d->namePlateTextOffsetY = parseFloat(valueStr, d->namePlateTextOffsetY);
                // Botones UI
                else if (key == "boton_altura" || key == "ui_btn_height")
                    d->uiBtnHeight = parseFloat(valueStr, d->uiBtnHeight);
                else if (key == "boton_padding_x" || key == "ui_btn_padding_x")
                    d->uiBtnPaddingX = parseFloat(valueStr, d->uiBtnPaddingX);
                else if (key == "boton_gap" || key == "ui_btn_gap")
                    d->uiBtnGap = parseFloat(valueStr, d->uiBtnGap);
                else if (key == "boton_margen_derecho" || key == "ui_btn_margin_right")
                    d->uiBtnMarginRight = parseFloat(valueStr, d->uiBtnMarginRight);
                else if (key == "boton_margen_inferior" || key == "ui_btn_margin_bottom")
                    d->uiBtnMarginBottom = parseFloat(valueStr, d->uiBtnMarginBottom);
                else if (key == "boton_auto_visible" || key == "ui_btn_auto_visible")
                    d->uiBtnAutoVisible = parseBool(valueStr, true);
                else if (key == "boton_skip_visible" || key == "ui_btn_skip_visible")
                    d->uiBtnSkipVisible = parseBool(valueStr, true);
                else if (key == "boton_save_visible" || key == "ui_btn_save_visible")
                    d->uiBtnSaveVisible = parseBool(valueStr, true);
                else if (key == "boton_load_visible" || key == "ui_btn_load_visible")
                    d->uiBtnLoadVisible = parseBool(valueStr, true);
                else if (key == "boton_log_visible" || key == "ui_btn_log_visible")
                    d->uiBtnLogVisible = parseBool(valueStr, true);
                else if (key == "boton_menu_visible" || key == "ui_btn_menu_visible")
                    d->uiBtnMenuVisible = parseBool(valueStr, true);
                    // ── Atajo plural: los 6 botones de UI de un saque ──────────
                    // Preset GRUESO. Los flags finos de arriba lo refinan si se
                    // escriben DESPUÉS (orden secuencial, §7.16): 'botones_visibles
                    // no' + 'boton_menu_visible si' = todo off menos el menú.
                    // HONESTO: solo visibilidad (render+hit-test). NO toca función
                    // — skip(Ctrl)/auto(A)/save(K/F5) siguen por teclado. Esa
                    // supresión funcional es la modalidad-aviso (fichada aparte).
                else if (key == "botones_visibles" || key == "ui_buttons_visible")
                {
                    bool v = parseBool(valueStr, true);
                    d->uiBtnAutoVisible = v; d->uiBtnSkipVisible = v;
                    d->uiBtnSaveVisible = v; d->uiBtnLoadVisible = v;
                    d->uiBtnLogVisible = v; d->uiBtnMenuVisible = v;
                    }
                else if (key == "boton_labels" || key == "ui_btn_labels")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                    {
                        std::string labelsStr = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        std::istringstream lss(labelsStr);
                        std::string token;
                        size_t idx = 0;
                        while (std::getline(lss, token, ',') && idx < 6)
                        {
                            token = trim(token);
                            if (!token.empty())
                                d->uiBtnLabels[idx++] = token;
                        }
                    }
                }
                // Choices
                else if (key == "choice_ancho_ratio" || key == "choice_width_ratio")
                    d->choiceBtnWidthRatio = parseFloat(valueStr, d->choiceBtnWidthRatio);
                else if (key == "choice_altura" || key == "choice_height")
                    d->choiceBtnHeight = parseFloat(valueStr, d->choiceBtnHeight);
                else if (key == "choice_espaciado" || key == "choice_gap")
                    d->choiceBtnGap = parseFloat(valueStr, d->choiceBtnGap);
                else if (key == "choice_pos_x" || key == "choice_pos_x_ratio")
                    d->choicePosXRatio = parseFloat(valueStr, d->choicePosXRatio);
                else if (key == "choice_pos_y" || key == "choice_pos_y_ratio")
                    d->choicePosYRatio = parseFloat(valueStr, d->choicePosYRatio);
                else if (key == "choice_timer_altura" || key == "choice_timer_height")
                    d->choiceTimerBarHeight = parseFloat(valueStr, d->choiceTimerBarHeight);
                else if (key == "choice_timer_offset_y" || key == "choice_timer_offset_below")
                    d->choiceTimerBarOffsetY = parseFloat(valueStr, d->choiceTimerBarOffsetY);
                else if (key == "choice_timer_borde_grosor" || key == "choice_timer_border_thickness")
                    d->choiceTimerBarBorderThickness = parseFloat(valueStr, d->choiceTimerBarBorderThickness);
                else if (key == "choice_timer_fondo_color" || key == "choice_timer_bg_color")
                    parseColor(valueStr, d->choiceTimerBarBgR, d->choiceTimerBarBgG, d->choiceTimerBarBgB, d->choiceTimerBarBgA);
                else if (key == "choice_timer_borde_color" || key == "choice_timer_border_color")
                    parseColor(valueStr, d->choiceTimerBarBorderR, d->choiceTimerBarBorderG, d->choiceTimerBarBorderB, d->choiceTimerBarBorderA);
                else if (key == "choice_fondo_color" || key == "choice_bg_color")
                    parseColor(valueStr, d->choiceBtnBgR, d->choiceBtnBgG, d->choiceBtnBgB, d->choiceBtnBgA);
                else if (key == "choice_borde_color" || key == "choice_border_color")
                    parseColor(valueStr, d->choiceBtnBorderR, d->choiceBtnBorderG, d->choiceBtnBorderB, d->choiceBtnBorderA);
                else if (key == "choice_borde_grosor" || key == "choice_border_thickness")
                    d->choiceBtnBorderThickness = parseFloat(valueStr, d->choiceBtnBorderThickness);
                else if (key == "choice_texto_color" || key == "choice_text_color")
                    parseColor(valueStr, d->choiceBtnTextR, d->choiceBtnTextG, d->choiceBtnTextB, d->choiceBtnTextA);
                else if (key == "choice_hover_fondo_color" || key == "choice_hover_bg_color")
                    parseColor(valueStr, d->choiceBtnHoverBgR, d->choiceBtnHoverBgG, d->choiceBtnHoverBgB, d->choiceBtnHoverBgA);
                else if (key == "choice_hover_borde_color" || key == "choice_hover_border_color")
                    parseColor(valueStr, d->choiceBtnHoverBorderR, d->choiceBtnHoverBorderG, d->choiceBtnHoverBorderB, d->choiceBtnHoverBorderA);
                else if (key == "choice_hover_texto_color" || key == "choice_hover_text_color")
                    parseColor(valueStr, d->choiceBtnHoverTextR, d->choiceBtnHoverTextG, d->choiceBtnHoverTextB, d->choiceBtnHoverTextA);
                    // ── Tarjeta de ranura (slot-widget) · Modelo A: ranura_* (es) / slot_* (en) ──
                else if (key == "ranura_fondo_color" || key == "slot_bg_color")
                    parseColor(valueStr, d->slotCardBgR, d->slotCardBgG, d->slotCardBgB, d->slotCardBgA);
                else if (key == "ranura_borde_color" || key == "slot_border_color")
                    parseColor(valueStr, d->slotCardBorderR, d->slotCardBorderG, d->slotCardBorderB, d->slotCardBorderA);
                else if (key == "ranura_borde_grosor" || key == "slot_border_thickness")
                    d->slotCardBorderThickness = parseFloat(valueStr, d->slotCardBorderThickness);
                else if (key == "ranura_hover_fondo_color" || key == "slot_hover_bg_color")
                    parseColor(valueStr, d->slotCardHoverBgR, d->slotCardHoverBgG, d->slotCardHoverBgB, d->slotCardHoverBgA);
                else if (key == "ranura_hover_borde_color" || key == "slot_hover_border_color")
                    parseColor(valueStr, d->slotCardHoverBorderR, d->slotCardHoverBorderG, d->slotCardHoverBorderB, d->slotCardHoverBorderA);
                else if (key == "ranura_hover_texto_color" || key == "slot_hover_text_color")
                {
                    // FAN-OUT (§7.166): una key tiñe los 3 textos al hover (escena/fecha/preview).
                    parseColor(valueStr, d->slotCardHoverSceneR, d->slotCardHoverSceneG, d->slotCardHoverSceneB, d->slotCardHoverSceneA);
                    parseColor(valueStr, d->slotCardHoverDateR, d->slotCardHoverDateG, d->slotCardHoverDateB, d->slotCardHoverDateA);
                    parseColor(valueStr, d->slotCardHoverPreviewR, d->slotCardHoverPreviewG, d->slotCardHoverPreviewB, d->slotCardHoverPreviewA);
                    }
                else if (key == "ranura_vacia_fondo_color" || key == "slot_empty_bg_color")
                    parseColor(valueStr, d->slotCardEmptyBgR, d->slotCardEmptyBgG, d->slotCardEmptyBgB, d->slotCardEmptyBgA);
                else if (key == "ranura_vacia_borde_color" || key == "slot_empty_border_color")
                    parseColor(valueStr, d->slotCardEmptyBorderR, d->slotCardEmptyBorderG, d->slotCardEmptyBorderB, d->slotCardEmptyBorderA);
                else if (key == "ranura_vacia_hover_fondo_color" || key == "slot_empty_hover_bg_color")
                    parseColor(valueStr, d->slotCardEmptyHoverBgR, d->slotCardEmptyHoverBgG, d->slotCardEmptyHoverBgB, d->slotCardEmptyHoverBgA);
                else if (key == "ranura_vacia_hover_borde_color" || key == "slot_empty_hover_border_color")
                    parseColor(valueStr, d->slotCardEmptyHoverBorderR, d->slotCardEmptyHoverBorderG, d->slotCardEmptyHoverBorderB, d->slotCardEmptyHoverBorderA);
                else if (key == "ranura_vacia_hover_texto_color" || key == "slot_empty_hover_text_color")
                    parseColor(valueStr, d->slotCardEmptyHoverTextR, d->slotCardEmptyHoverTextG, d->slotCardEmptyHoverTextB, d->slotCardEmptyHoverTextA);
                else if (key == "ranura_vacia_etiqueta" || key == "slot_empty_label")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->slotCardEmptyLabel = valueStr.substr(q1 + 1, q2 - q1 - 1);
                    else
                    {
                        // Rechazo pedagogico (espeja el de ':'): no se asigna → queda
                        // el default. Una sola forma valida: el texto entre comillas.
                        std::cerr << "[.eclips] Aviso linea " << lineNum
                            << " en '" << path << "' (key '" << key << "'):\n"
                            << "  El texto que se ve va entre comillas (ej: " << key
                            << " \"empty slot\").\n"
                            << "  Valor ignorado (queda el default). Pon comillas y vuelve a correr.\n"
                            << "  Mas info: ver \"Comillas en .eclips\" en ECLIPSCRIPT_DOCS.md"
                            << std::endl;
                    }
                }
                else if (key == "ranura_vacia_inactiva_opacidad" || key == "slot_empty_inactive_opacity")
                    // 6.6: factor de opacidad del estado "inactiva" (vacia + accion
                    // inerte sobre vacio). <1.0 atenua + enmudece; 1.0 = off por
                    // identidad. Numero desnudo (valor tipado, §7.188 — sin comillas).
                        d->slotCardEmptyInactiveOpacity = parseFloat(valueStr, d->slotCardEmptyInactiveOpacity);
                else if (key == "ranura_hover_barra_color" || key == "slot_hover_bar_color")
                    parseColor(valueStr, d->slotCardHoverBarR, d->slotCardHoverBarG, d->slotCardHoverBarB, d->slotCardHoverBarA);
                else if (key == "ranura_hover_barra_grosor" || key == "slot_hover_bar_thickness")
                    d->slotCardHoverBarThickness = parseFloat(valueStr, d->slotCardHoverBarThickness);
                else if (key == "ranura_fecha_formato" || key == "slot_date_format")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->slotDateFormat = valueStr.substr(q1 + 1, q2 - q1 - 1);
                    else
                    {
                        // Rechazo pedagogico (espeja el de ':'): no se asigna → queda
                        // el default. Una sola forma valida: la plantilla entre comillas.
                        std::cerr << "[.eclips] Aviso linea " << lineNum
                            << " en '" << path << "' (key '" << key << "'):\n"
                            << "  El texto que se ve va entre comillas (ej: " << key
                            << " \"{dia_pad}/{mes_pad}/{anio}\").\n"
                            << "  Valor ignorado (queda el default). Pon comillas y vuelve a correr.\n"
                            << "  Mas info: ver \"Comillas en .eclips\" en ECLIPSCRIPT_DOCS.md"
                            << std::endl;
                    }
                }
                    // ── Idioma de los nombres de la fecha (idiomas CP3 · DI-4) ──
                    //
                    // Valor DESNUDO, sin comillas, y eso no es inconsistencia
                    // con `ranura_fecha_formato`: aquél es prosa que el jugador
                    // lee, éste es un CÓDIGO de idioma — la misma especie que
                    // `lang=en` en settings y que los tokens de `idiomas es en`
                    // en el manifest. §7.188: valor tipado, sin comillas.
                    //
                    // `auto` es la palabra explícita del default. Se acepta y
                    // vale lo mismo que no escribir la key, para que un tema
                    // pueda DECIR que sigue al jugador en vez de callarlo.
                else if (key == "ranura_fecha_idioma" || key == "slot_date_language")
                {
                    const std::string v = trim(valueStr);
                    // FOLD-PARTIDO. Ranura MIXTA (`auto` o un id de idioma), y
                    // se pliega igual por el corolario: un id de idioma real es
                    // un codigo corto en minuscula (`es`, `en`), asi que `Auto`
                    // no puede ser el otro lado. Antes quedaba como un idioma
                    // LITERAL llamado "Auto" -mudo- en vez de seguir al jugador.
                    // El valor que NO es `auto` se guarda tal cual: es un id, y
                    // plegarlo seria decidir por el creador.
                    d->slotDateLanguage =
                        (eclips::vocabToLower(v) == "auto") ? std::string() : v;
                }
                    // ── Sprite (cara PNG) de la ranura ocupada · Modelo A ──
                else if (key == "ranura_sprite" || key == "slot_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->slotCardSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "ranura_sprite_hover" || key == "slot_sprite_hover")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->slotCardHoverSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "ranura_sprite_borde_izq" || key == "slot_sprite_border_left")
                    d->slotCardSliceLeft = parseFloat(valueStr, d->slotCardSliceLeft);
                else if (key == "ranura_sprite_borde_der" || key == "slot_sprite_border_right")
                    d->slotCardSliceRight = parseFloat(valueStr, d->slotCardSliceRight);
                else if (key == "ranura_sprite_borde_arriba" || key == "slot_sprite_border_top")
                    d->slotCardSliceTop = parseFloat(valueStr, d->slotCardSliceTop);
                else if (key == "ranura_sprite_borde_abajo" || key == "slot_sprite_border_bottom")
                    d->slotCardSliceBottom = parseFloat(valueStr, d->slotCardSliceBottom);

                    // ── Thumbnail: caja de fit (aspecto + ajuste) · Modelo A ──
                else if (key == "ranura_thumb_aspecto" || key == "slot_thumb_aspect")
                {
                    // FOLD-PARTIDO. Ranura MIXTA (`auto`, un `16:9` o un
                    // flotante), y se pliega igual: ni una razon ni un numero
                    // pueden escribirse con letras, asi que `Auto` no puede ser
                    // el otro lado.
                    if (eclips::vocabToLower(valueStr) == "auto")
                        d->slotThumbAspect = 0.0f;   // caja sigue el aspecto de la captura
                    else
                    {
                        // "16:9" (ancho:alto, legible) o un flotante desnudo (1.777).
                        size_t colon = valueStr.find(':');
                        if (colon != std::string::npos)
                        {
                            float wv = parseFloat(valueStr.substr(0, colon), 0.0f);
                            float hv = parseFloat(valueStr.substr(colon + 1), 0.0f);
                            if (wv > 0.0f && hv > 0.0f)
                                d->slotThumbAspect = wv / hv;
                            else
                                std::cerr << "[Theme] Aviso linea " << lineNum
                                << ": aspecto invalido para " << key << " ('"
                                << valueStr << "'). Ej: 16:9, 1.777, o auto."
                                << std::endl;
                        }
                        else
                            d->slotThumbAspect = parseFloat(valueStr, d->slotThumbAspect);
                    }
                    }
                else if (key == "ranura_thumb_ajuste" || key == "slot_thumb_fit")
                {
                    // FOLD-PARTIDO: ranura PURA (vocabulario cerrado). El aviso
                    // de abajo sigue citando `valueStr`, o sea lo que el creador
                    // escribio de verdad, no la version plegada.
                    const std::string vFit = eclips::vocabToLower(valueStr);
                    if (vFit == "estirar" || vFit == "stretch")
                        d->slotThumbFit = TextureFit::Estirar;
                    else if (vFit == "contener" || vFit == "contain")
                        d->slotThumbFit = TextureFit::Contener;
                    else if (vFit == "cubrir" || vFit == "cover")
                        d->slotThumbFit = TextureFit::Cubrir;
                    else
                        std::cerr << "[Theme] Aviso linea " << lineNum
                        << ": valor invalido para " << key << " ('" << valueStr
                        << "'). Esperado: estirar/contener/cubrir o "
                        << "stretch/contain/cover." << std::endl;
                        }

                else if (key == "ranura_layout" || key == "slot_layout")
                {
                    // FOLD-PARTIDO: ranura PURA.
                    const std::string vLay = eclips::vocabToLower(valueStr);
                    if (vLay == "inline" || vLay == "horizontal")
                        d->slotLayout = SlotLayout::Inline;
                    else if (vLay == "vertical")
                        d->slotLayout = SlotLayout::Vertical;
                    else
                        std::cerr << "[Theme] Aviso linea " << lineNum
                        << ": valor invalido para " << key << " ('" << valueStr
                        << "'). Esperado: inline (u horizontal) / vertical."
                        << std::endl;
                        }

                    // ── Cara PNG + sonido del botón de choice ──
                else if (key == "choice_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->choiceBtnSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "choice_hover_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->choiceBtnHoverSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "choice_sprite_borde_izq" || key == "choice_sprite_border_left")
                    d->choiceBtnSliceLeft = parseFloat(valueStr, d->choiceBtnSliceLeft);
                else if (key == "choice_sprite_borde_der" || key == "choice_sprite_border_right")
                    d->choiceBtnSliceRight = parseFloat(valueStr, d->choiceBtnSliceRight);
                else if (key == "choice_sprite_borde_arriba" || key == "choice_sprite_border_top")
                    d->choiceBtnSliceTop = parseFloat(valueStr, d->choiceBtnSliceTop);
                else if (key == "choice_sprite_borde_abajo" || key == "choice_sprite_border_bottom")
                    d->choiceBtnSliceBottom = parseFloat(valueStr, d->choiceBtnSliceBottom);
                else if (key == "choice_hover_escala" || key == "choice_hover_scale")
                    d->choiceHoverScale = parseFloat(valueStr, d->choiceHoverScale);
                    // ── Cara PNG del chrome del bottom-bar (boton_imagen) ──
                else if (key == "boton_imagen" || key == "ui_btn_image")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->chromeBtnSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "boton_imagen_hover" || key == "ui_btn_image_hover")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->chromeBtnHoverSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "boton_imagen_borde_izq" || key == "ui_btn_image_border_left")
                    d->chromeBtnSliceLeft = parseFloat(valueStr, d->chromeBtnSliceLeft);
                else if (key == "boton_imagen_borde_der" || key == "ui_btn_image_border_right")
                    d->chromeBtnSliceRight = parseFloat(valueStr, d->chromeBtnSliceRight);
                else if (key == "boton_imagen_borde_arriba" || key == "ui_btn_image_border_top")
                    d->chromeBtnSliceTop = parseFloat(valueStr, d->chromeBtnSliceTop);
                else if (key == "boton_imagen_borde_abajo" || key == "ui_btn_image_border_bottom")
                    d->chromeBtnSliceBottom = parseFloat(valueStr, d->chromeBtnSliceBottom);
                else if (key == "choice_sonido_hover" || key == "choice_sound_hover")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->choiceBtnHoverSound = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "choice_sonido_click" || key == "choice_sound_click")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->choiceBtnClickSound = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else
                    headMatched = false;   // ── fin de la 1ª mitad de la cadena

                // ── 2ª mitad (corte C1061): entrada + cola + catch-all ──
                if (!headMatched)
                {
                // ── Entrada de texto ('entrada'/'input') — bloque + OK ──
                if (key == "entrada_pos_y" || key == "input_pos_y")
                    d->inputPosYRatio = parseFloat(valueStr, d->inputPosYRatio);
                else if (key == "entrada_tamano" || key == "input_size")
                    d->inputSize = parseFloat(valueStr, d->inputSize);  // 0 = heredar baseline
                else if (key == "entrada_ok_ancho" || key == "input_ok_width")
                    d->inputOkW = parseFloat(valueStr, d->inputOkW);
                else if (key == "entrada_ok_alto" || key == "input_ok_height")
                    d->inputOkH = parseFloat(valueStr, d->inputOkH);
                else if (key == "entrada_ok_offset_x" || key == "input_ok_offset_x")
                    d->inputOkOffsetX = parseFloat(valueStr, d->inputOkOffsetX);
                else if (key == "entrada_ok_offset_y" || key == "input_ok_offset_y")
                    d->inputOkOffsetY = parseFloat(valueStr, d->inputOkOffsetY);
                else if (key == "entrada_ok_borde_grosor" || key == "input_ok_border_thickness")
                    d->inputOkBorderThickness = parseFloat(valueStr, d->inputOkBorderThickness);
                else if (key == "entrada_ok_estilo" || key == "input_ok_style")
                {
                    // FOLD-PARTIDO: ranura PURA.
                    const std::string vOk = eclips::vocabToLower(valueStr);
                    if (vOk == "caja" || vOk == "box")
                        d->inputOkStyle = UIBtnStyle::Box;
                    else if (vOk == "texto" || vOk == "text_only")
                        d->inputOkStyle = UIBtnStyle::TextOnly;
                    else
                        std::cerr << "[Theme] Aviso linea " << lineNum
                        << ": valor invalido para " << key << " ('" << valueStr
                        << "'). Esperado: caja/texto o box/text_only."
                        << std::endl;
                }
                else if (key == "entrada_ok_fondo_color" || key == "input_ok_bg_color")
                    parseColor(valueStr, d->inputOkBgR, d->inputOkBgG, d->inputOkBgB, d->inputOkBgA);
                else if (key == "entrada_ok_borde_color" || key == "input_ok_border_color")
                    parseColor(valueStr, d->inputOkBorderR, d->inputOkBorderG, d->inputOkBorderB, d->inputOkBorderA);
                else if (key == "entrada_ok_texto_color" || key == "input_ok_text_color")
                    parseColor(valueStr, d->inputOkTextR, d->inputOkTextG, d->inputOkTextB, d->inputOkTextA);
                else if (key == "entrada_ok_hover_fondo_color" || key == "input_ok_hover_bg_color")
                    parseColor(valueStr, d->inputOkHoverBgR, d->inputOkHoverBgG, d->inputOkHoverBgB, d->inputOkHoverBgA);
                else if (key == "entrada_ok_hover_borde_color" || key == "input_ok_hover_border_color")
                    parseColor(valueStr, d->inputOkHoverBorderR, d->inputOkHoverBorderG, d->inputOkHoverBorderB, d->inputOkHoverBorderA);
                else if (key == "entrada_ok_hover_texto_color" || key == "input_ok_hover_text_color")
                    parseColor(valueStr, d->inputOkHoverTextR, d->inputOkHoverTextG, d->inputOkHoverTextB, d->inputOkHoverTextA);
                else if (key == "entrada_ok_deshab_fondo_color" || key == "input_ok_disabled_bg_color")
                    parseColor(valueStr, d->inputOkOffBgR, d->inputOkOffBgG, d->inputOkOffBgB, d->inputOkOffBgA);
                else if (key == "entrada_ok_deshab_borde_color" || key == "input_ok_disabled_border_color")
                    parseColor(valueStr, d->inputOkOffBorderR, d->inputOkOffBorderG, d->inputOkOffBorderB, d->inputOkOffBorderA);
                else if (key == "entrada_ok_deshab_texto_color" || key == "input_ok_disabled_text_color")
                    parseColor(valueStr, d->inputOkOffTextR, d->inputOkOffTextG, d->inputOkOffTextB, d->inputOkOffTextA);
                else if (key == "entrada_ok" || key == "input_ok")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->inputOkLabel = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                // El `|| input_ok_sprite` faltaba (ticket #8, `cobertura-bilingue`):
                // sus DERIVADAS `entrada_ok_sprite_borde_*` si eran bilingues, o
                // sea que en ingles se podia pedir el BORDE del sprite y no el
                // sprite. Alias ADITIVO: la grafia ES no se toca.
                else if (key == "entrada_ok_sprite" || key == "input_ok_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->inputOkSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "entrada_ok_hover_sprite" || key == "input_ok_hover_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->inputOkHoverSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                        }
                else if (key == "entrada_ok_sprite_borde_izq" || key == "input_ok_sprite_border_left")
                    d->inputOkSliceLeft = parseFloat(valueStr, d->inputOkSliceLeft);
                else if (key == "entrada_ok_sprite_borde_der" || key == "input_ok_sprite_border_right")
                    d->inputOkSliceRight = parseFloat(valueStr, d->inputOkSliceRight);
                else if (key == "entrada_ok_sprite_borde_arriba" || key == "input_ok_sprite_border_top")
                    d->inputOkSliceTop = parseFloat(valueStr, d->inputOkSliceTop);
                else if (key == "entrada_ok_sprite_borde_abajo" || key == "input_ok_sprite_border_bottom")
                    d->inputOkSliceBottom = parseFloat(valueStr, d->inputOkSliceBottom);
                // ── Marco/fondo del aviso ('entrada_marco'/'input_frame') ──
                // Decoración detrás del cluster; default OFF (opt-in).
                else if (key == "entrada_marco" || key == "input_frame")
                    d->inputFrameVisible = parseBool(valueStr, false);
                else if (key == "entrada_marco_ancho" || key == "input_frame_width")
                    d->inputFrameW = parseFloat(valueStr, d->inputFrameW);
                else if (key == "entrada_marco_alto" || key == "input_frame_height")
                    d->inputFrameH = parseFloat(valueStr, d->inputFrameH);
                else if (key == "entrada_marco_offset_x" || key == "input_frame_offset_x")
                    d->inputFrameOffsetX = parseFloat(valueStr, d->inputFrameOffsetX);
                else if (key == "entrada_marco_offset_y" || key == "input_frame_offset_y")
                    d->inputFrameOffsetY = parseFloat(valueStr, d->inputFrameOffsetY);
                else if (key == "entrada_marco_borde_grosor" || key == "input_frame_border_thickness")
                    d->inputFrameBorderThickness = parseFloat(valueStr, d->inputFrameBorderThickness);
                else if (key == "entrada_marco_fondo_color" || key == "input_frame_bg_color")
                    parseColor(valueStr, d->inputFrameBgR, d->inputFrameBgG, d->inputFrameBgB, d->inputFrameBgA);
                // Opacidad del CUERPO (relleno del rect nativo, o la imagen del
                // PNG — multiplicador). Espeja caja_opacidad. Decoupla la opacidad
                // del color: la tocas sin reescribir el hex.
                else if (key == "entrada_marco_opacidad" || key == "input_frame_opacity")
                    d->inputFrameBgA = parsePercent(valueStr);
                else if (key == "entrada_marco_borde_color" || key == "input_frame_border_color")
                    parseColor(valueStr, d->inputFrameBorderR, d->inputFrameBorderG, d->inputFrameBorderB, d->inputFrameBorderA);
                // Opacidad del BORDE. Solo marco nativo: el PNG no dibuja borde
                // por engine (trae el suyo) → no-op con sprite, igual que caja_borde.
                else if (key == "entrada_marco_borde_opacidad" || key == "input_frame_border_opacity")
                    d->inputFrameBorderA = parsePercent(valueStr);
                else if (key == "entrada_marco_sprite" || key == "input_frame_sprite")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->inputFrameSpritePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                }
                else if (key == "entrada_marco_sprite_borde_izq" || key == "input_frame_sprite_border_left")
                    d->inputFrameSliceLeft = parseFloat(valueStr, d->inputFrameSliceLeft);
                else if (key == "entrada_marco_sprite_borde_der" || key == "input_frame_sprite_border_right")
                    d->inputFrameSliceRight = parseFloat(valueStr, d->inputFrameSliceRight);
                else if (key == "entrada_marco_sprite_borde_arriba" || key == "input_frame_sprite_border_top")
                    d->inputFrameSliceTop = parseFloat(valueStr, d->inputFrameSliceTop);
                else if (key == "entrada_marco_sprite_borde_abajo" || key == "input_frame_sprite_border_bottom")
                    d->inputFrameSliceBottom = parseFloat(valueStr, d->inputFrameSliceBottom);
                // QTE
                else if (key == "qte_barra_ancho_ratio" || key == "qte_bar_width_ratio")
                    d->qteBarWidthRatio = parseFloat(valueStr, d->qteBarWidthRatio);
                else if (key == "qte_barra_altura" || key == "qte_bar_height")
                    d->qteBarHeight = parseFloat(valueStr, d->qteBarHeight);
                else if (key == "qte_caja_tecla" || key == "qte_key_box_size")
                    d->qteKeyBoxSize = parseFloat(valueStr, d->qteKeyBoxSize);
                else if (key == "qte_barra_pos_y" || key == "qte_bar_pos_y_ratio")
                    d->qteBarPosYRatio = parseFloat(valueStr, d->qteBarPosYRatio);
                else if (key == "qte_barra_pos_x" || key == "qte_bar_pos_x_ratio")
                    d->qteBarPosXRatio = parseFloat(valueStr, d->qteBarPosXRatio);
                else if (key == "qte_espaciado" || key == "qte_bar_spacing")
                    d->qteBarSpacing = parseFloat(valueStr, d->qteBarSpacing);
                // Indicador de avance (auto-bar y ">")
                else if (key == "auto_barra_ancho" || key == "auto_bar_width")
                    d->autoBarWidth = parseFloat(valueStr, d->autoBarWidth);
                else if (key == "auto_barra_altura" || key == "auto_bar_height")
                    d->autoBarHeight = parseFloat(valueStr, d->autoBarHeight);
                else if (key == "auto_barra_margen_x" || key == "auto_bar_margin_x")
                    d->autoBarMarginX = parseFloat(valueStr, d->autoBarMarginX);
                else if (key == "auto_barra_margen_abajo" || key == "auto_bar_margin_bottom")
                    d->autoBarMarginBottom = parseFloat(valueStr, d->autoBarMarginBottom);
                else if (key == "auto_barra_fondo_color" || key == "auto_bar_bg_color")
                    parseColor(valueStr, d->autoBarBgR, d->autoBarBgG, d->autoBarBgB, d->autoBarBgA);
                else if (key == "auto_barra_relleno_color" || key == "auto_bar_fill_color")
                    parseColor(valueStr, d->autoBarFillR, d->autoBarFillG, d->autoBarFillB, d->autoBarFillA);
                else if (key == "indicador_avance_offset_x" || key == "advance_indicator_offset_x")
                    d->advanceIndicatorOffsetX = parseFloat(valueStr, d->advanceIndicatorOffsetX);
                else if (key == "indicador_avance_offset_abajo" || key == "advance_indicator_offset_bottom")
                    d->advanceIndicatorOffsetBottom = parseFloat(valueStr, d->advanceIndicatorOffsetBottom);
                else if (key == "indicador_avance_color" || key == "advance_indicator_color")
                    parseColor(valueStr, d->advanceIndicatorR, d->advanceIndicatorG, d->advanceIndicatorB, d->advanceIndicatorA);
                else if (key == "indicador_avance_glifo" || key == "advance_indicator_glyph")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->advanceIndicatorGlyph = valueStr.substr(q1 + 1, q2 - q1 - 1);
                }
                else if (key == "indicador_avance_imagen" || key == "advance_indicator_image")
                {
                    size_t q1 = valueStr.find('"');
                    size_t q2 = (q1 != std::string::npos) ? valueStr.find('"', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        d->advanceIndicatorImagePath = valueStr.substr(q1 + 1, q2 - q1 - 1);
                }
                else if (key == "indicador_avance_escala" || key == "advance_indicator_scale")
                    d->advanceIndicatorScale = parseFloat(valueStr, d->advanceIndicatorScale);
                else if (key == "indicador_avance_parpadeo" || key == "advance_indicator_blink")
                    d->advanceIndicatorBlink = parseBool(valueStr, d->advanceIndicatorBlink);
                else if (key == "indicador_avance_parpadeo_velocidad" || key == "advance_indicator_blink_speed")
                    d->advanceIndicatorBlinkSpeed = parseFloat(valueStr, d->advanceIndicatorBlinkSpeed);
                else if (key == "indicador_avance_vaiven" || key == "advance_indicator_nudge")
                    d->advanceIndicatorNudge = parseBool(valueStr, d->advanceIndicatorNudge);
                else if (key == "indicador_avance_vaiven_amplitud" || key == "advance_indicator_nudge_amplitude")
                    d->advanceIndicatorNudgeAmplitude = parseFloat(valueStr, d->advanceIndicatorNudgeAmplitude);
                else if (key == "indicador_avance_vaiven_periodo" || key == "advance_indicator_nudge_period")
                    d->advanceIndicatorNudgePeriod = parseFloat(valueStr, d->advanceIndicatorNudgePeriod);
                else if (key == "indicador_avance_vaiven_vertical" || key == "advance_indicator_nudge_vertical")
                    d->advanceIndicatorNudgeVertical = parseBool(valueStr, d->advanceIndicatorNudgeVertical);
                else if (key == "indicador_avance_vaiven_brillo" || key == "advance_indicator_nudge_glow")
                    d->advanceIndicatorNudgeBrillo = parseBool(valueStr, d->advanceIndicatorNudgeBrillo);
                    // Comportamiento (queda en Theme — feel del juego, solo TopLevel
                // gracias al pre-check de arriba que filtra estas keys cuando
                // estado == InsideBox)
                else if (key == "velocidad_texto" || key == "text_speed")
                {
                    const TextSpeedDeclaration parsed =
                        parseTextSpeedDeclaration(valueStr);
                    if (parsed.error.empty())
                    {
                        t.textSpeed = parsed.value;
                        t.textSpeedDomain = parsed.domain;
                    }
                    else
                    {
                        std::cerr << "[Theme] Aviso linea " << lineNum
                            << " en '" << path << "': " << key << " "
                            << valueStr << " no es valida: " << parsed.error
                            << "; se conserva la velocidad y dominio anteriores. "
                            << "Ejemplo copiable: " << key << " "
                            << parsed.repair << std::endl;
                    }
                }
                else if (key == "auto_demora" || key == "auto_delay")
                    t.autoDelay = parseFloat(valueStr, t.autoDelay);
                else if (key == "skip_intervalo" || key == "skip_interval")
                    t.skipInterval = parseFloat(valueStr, t.skipInterval);
                else if (key == "skip_no_visto" || key == "skip_unseen")
                    t.skipUnseen = parseBool(valueStr, t.skipUnseen);
                else if (key == "interrumpir_demora" || key == "interrupt_delay")
                    t.interruptDelay = parseFloat(valueStr, t.interruptDelay);
                else if (key == "velocidad_texto_lock" || key == "text_speed_lock")
                    t.textSpeedLocked = parseBool(valueStr, false);
                else if (key == "auto_demora_lock" || key == "auto_delay_lock")
                    t.autoDelayLocked = parseBool(valueStr, false);
                else if (key == "skip_intervalo_lock" || key == "skip_interval_lock")
                    t.skipIntervalLocked = parseBool(valueStr, false);
                else if (key == "skip_no_visto_lock" || key == "skip_unseen_lock")
                    t.skipUnseenLocked = parseBool(valueStr, false);
                else if (key == "historial_max" || key == "history_max")
                    t.historyMaxEntries = parseInt(valueStr, t.historyMaxEntries);
                    // ── Hotspot hover outline (CP5 átomo) — top-level, global ──
                else if (key == "hotspot_hover_color")
                    parseColor(valueStr, t.hotspotHoverR, t.hotspotHoverG, t.hotspotHoverB, t.hotspotHoverA);
                else if (key == "hotspot_hover_grosor" || key == "hotspot_hover_thickness")
                    t.hotspotHoverThickness = parseFloat(valueStr, t.hotspotHoverThickness);
                // ── v3: esquinas redondeadas ─────────────────────────
                else if (key == "caja_radio" || key == "box_radius")
                    d->radius = parseFloat(valueStr, d->radius);
                else if (key == "nombre_radio" || key == "name_radius")
                    d->namePlateRadius = parseFloat(valueStr, d->namePlateRadius);
                // ── v3: outlines de texto ────────────────────────────
                else if (key == "nombre_outline_color" || key == "name_outline_color")
                    parseColor(valueStr, d->nameOutlineR, d->nameOutlineG,
                        d->nameOutlineB, d->nameOutlineA);
                else if (key == "nombre_outline_grosor" || key == "name_outline_thickness")
                    d->nameOutlineThickness = parseFloat(valueStr, d->nameOutlineThickness);
                else if (key == "texto_outline_color" || key == "text_outline_color")
                    parseColor(valueStr, d->textOutlineR, d->textOutlineG,
                        d->textOutlineB, d->textOutlineA);
                else if (key == "texto_outline_grosor" || key == "text_outline_thickness")
                    d->textOutlineThickness = parseFloat(valueStr, d->textOutlineThickness);
                // ── v3: layout de botones UI ─────────────────────────
                else if (key == "boton_alineacion" || key == "ui_btn_align")
                {
                    // FOLD-PARTIDO: ranura PURA.
                    const std::string vAli = eclips::vocabToLower(valueStr);
                    if (vAli == "izq" || vAli == "left")
                        d->uiBtnAlign = UIBtnAlign::Left;
                    else if (vAli == "centro" || vAli == "center")
                        d->uiBtnAlign = UIBtnAlign::Center;
                    else if (vAli == "der" || vAli == "right")
                        d->uiBtnAlign = UIBtnAlign::Right;
                    else
                        std::cerr << "[Theme] Aviso linea " << lineNum
                        << ": valor invalido para " << key << " ('" << valueStr
                        << "'). Esperado: izq/centro/der o left/center/right."
                        << std::endl;
                }
                else if (key == "boton_estilo" || key == "ui_btn_style")
                {
                    // FOLD-PARTIDO: ranura PURA.
                    const std::string vBtn = eclips::vocabToLower(valueStr);
                    if (vBtn == "caja" || vBtn == "box")
                        d->uiBtnStyle = UIBtnStyle::Box;
                    else if (vBtn == "texto" || vBtn == "text_only")
                        d->uiBtnStyle = UIBtnStyle::TextOnly;
                    else
                        std::cerr << "[Theme] Aviso linea " << lineNum
                        << ": valor invalido para " << key << " ('" << valueStr
                        << "'). Esperado: caja/texto o box/text_only."
                        << std::endl;
                }
                else
                {
                    // §1.9 — fail visible para keys desconocidas, contextualizado
                    if (state == State::InsideBox)
                        std::cerr << "[Theme] Aviso linea " << lineNum
                        << ": key desconocida '" << key << "' en bloque '"
                        << currentBoxAlias << "' en " << path
                        << " -- ignorada." << std::endl;
                    else
                        std::cerr << "[Theme] Aviso linea " << lineNum
                        << ": key desconocida '" << key << "' en " << path
                        << " -- ignorada." << std::endl;
                        }
                } // cierra el if (!headMatched) — 2ª mitad del corte C1061
            }

            // ── EOF check: bloque sin cerrar ─────────────────────────────
            // Decision: preservar el bloque parcial. Razon: el bloque ya
            // esta en el map con snapshot del default + cualquier delta
            // aplicada antes del EOF. Borrarlo seria mas estricto pero
            // menos util. El aviso es la senal de que falta el '}'.
            if (state == State::InsideBox)
            {
                std::cerr << "[Theme] Aviso: bloque 'caja_dialogo " << currentBoxAlias
                    << "' (linea " << blockStartLine
                    << ") no se cerro con '}' antes de EOF en " << path
                    << ". Bloque preservado con lo definido hasta el EOF." << std::endl;
            }

            std::cout << "[Theme] Cargado (v5): " << path << std::endl;
            return t;
        }

    private:
        // ── Helpers de parseo (mantenemos los existentes) ──────────────
        static std::string trim(const std::string& s)
        {
            size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return "";
            size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }

        static f32 parsePercent(const std::string& s)
        {
            try {
                std::string num = s;
                if (!num.empty() && num.back() == '%') num.pop_back();
                return std::max(0.0f, std::min(1.0f, std::stof(num) / 100.0f));
            }
            catch (...) { return 1.0f; }
        }

        static f32 hexChannelToFloat(const std::string& hex, size_t offset)
        {
            try {
                std::string ch = hex.substr(offset, 2);
                return std::stoi(ch, nullptr, 16) / 255.0f;
            }
            catch (...) { return 1.0f; }
        }

        static void parseColor(const std::string& value, f32& r, f32& g, f32& b, f32& a)
        {
            if (value.empty()) return;
            if (value[0] == '#')
            {
                std::string hex = value.substr(1);
                while (!hex.empty() && std::isspace((unsigned char)hex.back())) hex.pop_back();
                if (hex.size() >= 6)
                {
                    r = hexChannelToFloat(hex, 0);
                    g = hexChannelToFloat(hex, 2);
                    b = hexChannelToFloat(hex, 4);
                    a = (hex.size() >= 8) ? hexChannelToFloat(hex, 6) : 1.0f;
                }
                return;
            }
            // Formato float 0.0 - 1.0
            std::istringstream ss(value);
            f32 rv = r, gv = g, bv = b, av = a;
            if (ss >> rv && ss >> gv && ss >> bv)
            {
                r = rv; g = gv; b = bv;
                if (ss >> av) a = av;
            }
        }

        // Nuevos helpers para los valores numéricos
        static float parseFloat(const std::string& s, float defaultVal)
        {
            try { return std::stof(s); }
            catch (...) { return defaultVal; }
        }

        static int parseInt(const std::string& s, int defaultVal)
        {
            try { return std::stoi(s); }
            catch (...) { return defaultVal; }
        }

        // Delega al parseo central (§1.45). Calificado `eclips::` a propósito:
        // sin calificar, este método-de-clase se llamaría a sí mismo (recursión).
        // Sin context aquí → no avisa; los callers de Theme podrían pasar campo
        // si se quisieran warnings (fichado, no en esta pasada).
        static bool parseBool(const std::string& s, bool defaultVal)
        {
            return eclips::parseBool(s, defaultVal);
        }
    };

} // namespace eclips
