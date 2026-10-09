// ============================================================================
// ProjectManifest.h — Manifest del proyecto Eclips
// ============================================================================
//
// Carga UNA VEZ desde Engine::init() leyendo "proyecto.eclips" (o
// "project.eclips") en la raíz del working directory. Si ninguno existe,
// loaded queda false y el motor sigue corriendo como antes — modo "test
// rápido sin proyecto", útil para prototipar un .ecl suelto.
//
// ── Filosofía (sesión proyecto.eclips) ──────────────────────────────────
//   - OPCIONAL. Sin manifest, fallback a comportamiento actual.
//   - Bilingüe: titulo/title, autor/author, etc. (equivalencias intencionales)
//   - Mismo parser flat key-value que Theme.h (coherencia §7.x).
//   - Mismo rechazo pedagógico de ':' decorativo.
//   - Mismo warning §1.9 para keys desconocidas.
//
// ── Scope por pasada ────────────────────────────────────────────────────
//   Pasada 1: Tier 2 — metadata + paths
//     titulo, autor, version, script_inicial, script_dir, tema_default
//   Pasada 2A (esta versión): Tier 1 — bloque "personaje { ... }"
//     + Tier 3 parseable-inerte: velocidad_texto, caja, escala_base
//   Pasada 2B: integración con EclipParser y VNScriptState.
//   Pasada 3: conectar script_dir con resolveScriptPath y tema_default
//             con VNScriptState.
//
// ── Acceso ──────────────────────────────────────────────────────────────
//   Owner: Engine (via getProjectManifest() retorna const ProjectManifest*).
//   Patrón: §1.5 — puntero como fuente única, no singleton.
//
#pragma once

#include <string>
#include <map>
#include <vector>      // CP3: las filas del bloque `teclas` (transporte)
#include <fstream>
#include "engine/core/FontDeclaration.h"
#include "engine/core/TextStroke.h"
#include "engine/core/TextSpeed.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include "engine/core/EclipsParse.h"   // parseBool centralizado (§1.45)
#include "engine/core/TransicionPantalla.h"  // el bloque `transiciones {}` (D-14)
#include "engine/core/VNSystemFlow.h" // nombres libres y reservas FLF-01

namespace eclips
{

    // Helpers locales (mismos que Theme/Settings, copiados para evitar
    // dependencia cruzada entre headers).
    namespace pm_detail
    {

        inline std::string trim(const std::string& s)
        {
            size_t a = 0;
            while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
            size_t b = s.size();
            while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
            return s.substr(a, b - a);
        }

        inline std::string stripQuotes(const std::string& s)
        {
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                return s.substr(1, s.size() - 2);
            return s;
        }

        // Delega al parseo central (§1.45) preservando el stripQuotes propio
        // del manifest (acepta `"si"` con comillas, que el central no pela).
        // Calificado `eclips::` para no llamarse a sí mismo. Default false +
        // sin context (paridad con el comportamiento previo: resto → false).
        inline bool parseBool(const std::string& s)
        {
            return eclips::parseBool(stripQuotes(s), false);
        }

        inline float parseFloat(const std::string& s, float fallback = 0.0f)
        {
            try { return std::stof(stripQuotes(s)); }
            catch (...) { return fallback; }
        }

        // El `#RRGGBB` valido, en UN solo lugar. Extraido de la construccion de
        // `ControlStylePart` (mucho mas abajo), que lo tenia inline y DESPUES del
        // split por `_`. El borde de `estilo_control` entra por lista blanca
        // ANTES de ese split, asi que no podia alcanzarlo: o se extraia, o el
        // motor quedaba con DOS definiciones de *color valido* que pueden derivar.
        inline bool hexValido(const std::string& s)
        {
            if (s.size() != 7 || s[0] != '#') return false;
            for (size_t i = 1; i < s.size(); ++i)
                if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
            return true;
        }

        // Intensidad: "60%" -> 0.6, "0.6" -> 0.6. Espeja el comando 'texto'
        // para que estilo e inline guarden el mismo formato (merge trivial).
        inline float parseIntensity(const std::string& s, float fallback = 0.0f)
        {
            std::string v = stripQuotes(s);
            if (v.empty()) return fallback;
            try {
                if (v.back() == '%')
                    return std::stof(v.substr(0, v.size() - 1)) / 100.0f;
                return std::stof(v);
            }
            catch (...) { return fallback; }
        }

    } // namespace pm_detail

    // ════════════════════════════════════════════════════════════════════════════
    // PersonajeManifest — Definición de un personaje en proyecto.eclips
    // ════════════════════════════════════════════════════════════════════════════
    //
    // Personajes declarados en el manifest se siembran en EclipParser ANTES
    // de procesar cada .ecl, para que el creador no tenga que redeclararlos
    // en cada script. Un .ecl puede redefinir un personaje localmente con
    // aviso de consola (override Caso 1).
    //
    // Tier 1 = consumido en v1. Tier 3 parseable-inerte = se guarda pero
    // ningún consumidor lo lee todavía; se cablea cuando llegue su pasada.
    //
    struct PersonajeManifest
    {
        // ─── Tier 1 — Identidad y assets ─────────────────────────────────
        std::string alias;            // Key/clave (ej: "l", "luna")
        std::string nombre;            // Display name ("Luna"). Vacío = usar alias.
        std::string path;              // Ruta a sprites
        bool        compuesto = false; // Personaje compuesto (carpeta con partes)

        // ─── Overrides por-personaje (todos consumidos en v1) ────────────
        std::optional<TextSpeed> velocidadTexto;
        bool        velocidadLock = false;   // Si true, ignora override del jugador
        // (paralelo a Theme.textSpeedLocked)
        std::string caja;                    // "" = usar caja default del tema
        float       escalaBase = 1.0f;       // Escala base del sprite (1.0 = nativo)
        float       anclaY = 0.0f;       // C-ANCLA: offset vertical en px (+ baja). Leido en draw-time (§7.64).

        // ─── Presentación de voz (consumido en v1, P1) ───────────────────
        // Comillas visibles que el motor ENVUELVE alrededor del diálogo de
        // este personaje (NO las teclea el creador). "" = sin comillas
        // (default; narrador incluido — el narrador no tiene entrada aquí).
        // String de 2 glifos: 1º abre, 2º cierra ("«»", "''", "『』").
        // 1 glifo = simétrico. Se parte por code-point (multi-byte safe).
        std::string comillas;

        // ─── Apariencias (C-ALIAS) — nombres logicos de looks compuestos ──────
        // alias del look -> cadena compuesta cruda ("feliz" -> "1l_2r_a"). Vive
        // por-personaje: los nombres de archivo colisionan entre carpetas (el
        // "1l_2r_a" de Monika != el de Natsuki). Resuelto execute-time en
        // cmdMostrar (espejo de resolverImagen): el parser parsea, el ejecutor
        // resuelve. Valor = token interpretado, sin comillas (§7.44).
        std::map<std::string, std::string> apariencias;
        // Look que usa 'mostrar X' sin EXPR. "" = sin default (motor exige EXPR
        // como hoy). Puede apuntar a un alias o ser una cadena cruda.
        std::string defaultApariencia;
    };

    // ════════════════════════════════════════════════════════════════════════════
    // EstiloManifest — Paquete reutilizable de fuente + color + glow (texto-capa)
    // ════════════════════════════════════════════════════════════════════════════
    //
    // Define-una-vez / invoca-por-nombre (tipo 'define' de Ren'Py). NO agrega
    // capacidad: empaqueta ladrillos que ya existen (fuente, color, glow) para
    // no retipear params en cada 'texto'. Centinelas de ausencia espejan
    // EclipCmd: string vacio = no declarado; glow*/borde* en 0 = no declarado.
    // Tamano NO entra (vive en la 'fuente', §7.43).
    //
    // El BORDE si entra (lote OUTLINE-TEXTO-CAPA). El renglon que vivia aca
    // decia "Outline tampoco (aun no cableado a texto-capa)" y era cierto: el
    // motor sabe dibujar borde desde siempre (`drawTextOutlined*`), la caja de
    // dialogo lo usa, y el TEMA ya se lo deja elegir al creador
    // (`nombre_outline_color`/`nombre_outline_grosor`) — pero el texto-capa y
    // las opciones no podian pedirlo. Era un cable faltante, no capacidad
    // nueva (§7.383).
    //
    // La grafia ES es `borde_*` y NO `outline_*`: `outline_grosor` seria
    // prefijo INGLES + sufijo ESPANOL, o sea exactamente la quimera que
    // SYN-4c-ii retiro de este mismo bloque (`glow_radio`/`glow_intensidad`).
    // El alias EN limpio es `outline_color`/`outline_thickness`. Las claves del
    // TEMA (`nombre_outline_grosor`) siguen siendo quimeras y quedan fichadas:
    // migrarlas es vocabulario, y `Theme.h` no tiene un solo harness.
    //
    struct EstiloManifest
    {
        std::string nombre;          // Nombre que el creador inventa ("titulo_brillante")
        std::string fontName;        // Fuente nombrada referenciada ("" = ausente)
        std::string colorHex;        // Color del texto, hex crudo ("" = ausente)
        std::string glowColorHex;    // Color del halo, hex crudo ("" = ausente)
        float       glowRadius = 0.0f;     // 0 = ausente
        float       glowIntensity = 0.0f;  // 0 = ausente (ya en [0..1])

        // ── Borde (OUTLINE-TEXTO-CAPA) — hermano del glow, misma forma ──
        std::string bordeColorHex;         // hex crudo ("" = ausente)
        float       bordeGrosor = 0.0f;    // px (0 = ausente)

        // ── Lista autorable (TRAZOS-LISTA, A1) ────────────────────────────
        // Una línea repetida conserva el orden de escritura. Si existe una
        // lista base, ésta es atómica y gana al borde legado del estilo.
        std::vector<TextStrokeSpec> trazos;
        std::vector<TextStrokeSpec> trazosHover;

        // ── Variante de hover (átomo enlace) — vacío/0 = hereda del base ──
        bool        hasHover = false;          // ¿se declaró algún *_hover?
        std::string hoverColorHex;             // "" = hereda color base
        std::string hoverGlowColorHex;         // "" = hereda glow_color base
        float       hoverGlowRadius = 0.0f;    // 0 = hereda brillo_radio base
        float       hoverGlowIntensity = 0.0f; // 0 = hereda brillo_intensidad base
        std::string hoverBordeColorHex;        // "" = hereda borde_color base
        float       hoverBordeGrosor = 0.0f;   // 0 = hereda borde_grosor base
        // (la fuente NO tiene variante hover: cambiar fuente = letra-crece, diferido)
    };

    // Paquete visual tipado para controles horizontales. `declared` distingue
    // ausencia (hereda) de `none` (suprime); `color` distingue hex de path.
    struct ControlStylePart
    {
        bool declared = false;
        bool valid = true;
        bool none = false;
        bool color = false;
        std::string value;
        int line = 0;
        std::string sourcePath, styleName, property, stateName;
    };

    // FORMAS_01 - LA SILUETA DE UNA PIEZA
    //
    // El eje que el fundador firmo es la SILUETA, no la esquina, y la razon
    // es MEDIDA: sobre estilo_control la esquina tiene CERO callers en las
    // cuatro referencias -el riel de Ren Py mide 42 filas x 473 px con recorte
    // 0 arriba y 0 abajo, o sea CUADRADO; DDLC y Katawa resuelven sus piezas
    // con ARTE (la frontera arte/primitiva de HC-BOR-07 las exime); y Sunrider
    // es familia theme.diamond, que es ESTE eje y no aquel-.
    //
    // Conjunto CERRADO y enumerado (P5 del 2.9):
    //
    //     Rectangulo   lo que toda pieza es hoy. Es el DEFAULT, asi que un
    //                  proyecto que no escribe la clave no se mueve un pixel.
    //     Rombo        1 caller medido al pixel: la perilla de Sunrider, 9x12.
    //
    // circulo, capsula, triangulo: CERO callers, NO entran (7.118). Cuando
    // aparezca uno, este enum crece Y la voz del parser tiene que enumerarlo:
    // su harness se pone rojo si no.
    enum class ControlFormaPieza { Rectangulo, Rombo };

    struct ControlStyleManifest
    {
        std::string nombre;
        ControlStylePart track, fill, thumb;
        std::map<std::string, ControlStylePart> variants;
        // ── `D-52`..`D-56` · LA GEOMETRIA SIN PNG ───────────────────────────
        //
        // Cuatro ESCALARES, y no cuatro `ControlStylePart`: esa struct es
        // `color | ruta | ninguno`, asi que un `43` pelado caeria a su `else` y
        // entraria como RUTA. Viven aca, al lado de las piezas, porque son
        // propiedades DE esas piezas.
        //
        // `D-56`: los defaults SON la conducta de hoy — riel 8 (el `kBarTrackH`
        // de `VNScriptState.h`), perilla 14x22, perilla centrada en los
        // extremos. Un proyecto que no escribe ninguna de las cuatro se dibuja
        // byte a byte como antes.
        float rielAltura    = 8.0f;
        float perillaAncho  = 14.0f;
        float perillaAltura = 22.0f;
        // `centrada` (false) = el centro de la perilla recorre `x..x+ancho`, y
        // medio ancho sobresale en cada extremo. `dentro` (true) = la caja
        // visual no sobresale. NO es el default, y la razon es un contrato
        // firmado: mover el eje de valor cambia QUE VALOR produce cada pixel, y
        // `ECLIPSCRIPT_DOCS` firmo en `v0.8.54` que `x..x+ancho` no se movia.
        bool  perillaDentro = false;
        // `declarada` separa *omitido* de *escrito*, igual que `declared` en las
        // piezas. Lo necesita el render para saber si el numero manda sobre la
        // textura del riel/perilla.
        bool rielAlturaDeclarada      = false;
        bool perillaAnchoDeclarada    = false;
        bool perillaAlturaDeclarada   = false;
        bool perillaExtremosDeclarada = false;
        // ── `BORDE_Y_FORMAS_01` · `A1`+`B1` · EL BORDE DE UNA PIEZA ─────────
        //
        // Escalares, y por la misma razon que la geometria: un `#1C1C1C` pelado
        // entraria por el `else` de `ControlStylePart` como RUTA, y un `3`
        // tambien. Viven aca porque son propiedades DE esas piezas.
        //
        // `A1`: el grosor default es 1, y NO es el de la primitiva.
        // `drawRectBorder` trae `thickness = 2.0f` (`Draw2D.h:225`), que no
        // reproduce NINGUNO de los dos callers: medidos con sonda de pixeles
        // sobre las capturas del fundador, el borde del riel de Sunrider y el de
        // la perilla de DDLC miden 1 px los dos.
        //
        // `B1`: el COLOR es el interruptor -sin color declarado no hay borde, y
        // el grosor solo avisa en vez de callarse-, y decora la caja de la pieza
        // AUNQUE la pieza no este declarada, o sea tambien sobre el riel de
        // fallback. Por eso el render lee estos campos y no `ControlStylePart`.
        std::string rielBordeColor;
        std::string perillaBordeColor;
        float rielBordeGrosor    = 1.0f;
        float perillaBordeGrosor = 1.0f;
        bool rielBordeColorDeclarada     = false;
        bool rielBordeGrosorDeclarada    = false;
        bool perillaBordeColorDeclarada  = false;
        bool perillaBordeGrosorDeclarada = false;
        // FORMAS_01 - LA FORMA DE LA PERILLA
        //
        // Solo la PERILLA, y no es economia: riel_forma y relleno_forma tienen
        // CERO callers medidos (7.118). El relleno ademas no seria expresable:
        // se pinta sobre la MISMA caja del riel recortada a bc.width * fill, o
        // sea que su borde derecho es un CORTE y no un extremo.
        //
        // La forma NO tiene estados, y la razon NO se hereda de D-54: aquella
        // dice que la geometria no los tiene porque *un tamano es uno solo por
        // pieza*, un argumento sobre la MAGNITUD que la vuelve IRREVERSIBLE.
        // Una silueta no es un tamano. La razon firmada aca es CERO CALLERS
        // MEDIDOS, o sea REVERSIBLE, igual que C3. Es 7.577 aplicada antes de
        // cometerla, y el 7.2 del harness custodia que la voz lo diga asi.
        //
        // rx/ry NO son geometria nueva: salen de perillaAncho/perillaAltura,
        // que existen desde v0.8.57 y ya alimentan el hit-test (D-55). El 9x12
        // de Sunrider ya se podia declarar; lo que faltaba era la palabra.
        //
        // F1(a): el hit-test NO se mueve. Una perilla rombo se agarra por su
        // CAJA ancho x altura, para que la verdad espacial siga siendo una
        // sola (7.100) y el jugador no pierda las esquinas que ve.
        ControlFormaPieza perillaForma = ControlFormaPieza::Rectangulo;
        bool perillaFormaDeclarada = false;
    };

    // ════════════════════════════════════════════════════════════════════════════
    // ProjectManifest — Datos completos del proyecto cargados de disco
    // ════════════════════════════════════════════════════════════════════════════
    // ════════════════════════════════════════════════════════════════════════════
    // IntroMotor — Política de la intro del motor (eclipse que cede la luz)
    // ════════════════════════════════════════════════════════════════════════════
    //
    // Controla si el splash del MOTOR (EclipsSplashState, DISENO_INTRO_ECLIPS.md)
    // corre al arranque. OPT-IN (§3, revisado): default = No. La intro del motor
    // NO se impone — un juego Eclips no se anuncia como Eclips (north star). El
    // creador la enciende conscientemente. Saltable siempre (>1.5s).
    //   - No      → nunca (DEFAULT: el motor es invisible salvo que lo invoquen).
    //   - Siempre → cada arranque (la firma se construye por repetición).
    //   - UnaVez  → solo la PRIMERA ejecución del jugador (persiste vía meta.eclips).
    //
    enum class IntroMotor { Siempre, UnaVez, No };

    struct ProjectManifest
    {
        // ─── Tier 2 — Metadata del proyecto ──────────────────────────────────
        std::string titulo;          // titulo / title
        std::string autor;           // autor / author
        std::string version;         // version

        // ─── Tier 2 — Arranque y paths ───────────────────────────────────────
        std::string scriptInicial;   // script_inicial / initial_script
        std::string scriptDir;       // script_dir (vacío = fallback "vnscript/")
        std::string temaDefault;     // tema_default / default_theme
        std::string menu;            // menu (.ecl del menú; vacío = MainMenu C++)

        // ─── Tier 2 — Catálogo de idiomas (DISENO_IDIOMAS CP1) ─────────────
        // `idiomas es en` / `languages es en`: tokens bare, orden estable.
        // El primero es el idioma original. Vacío = sistema completamente off.
        std::vector<std::string> idiomas;

        // ─── Tier 2 — Política de la intro del motor (CP3) ───────────────────
        // intro_motor / engine_intro. Default No (opt-in, §3 revisado): el motor
        // es invisible salvo que el creador lo invoque. Consumido en el gating
        // del push (main.cpp); 'una_vez' persiste vía meta.eclips.
        IntroMotor  introMotor = IntroMotor::No;

        // ─── Tier 2 — Resolución de diseño (A1) ──────────────────────────────
        // Lienzo lógico en el que el creador autora. El motor escala el frame
        // entero a la ventana. Default 1920×1080 (= viewport::width()/HEIGHT).
        // Engine siembra viewport:: desde aquí en init() y reloadManifest().
        int designW = 1920;          // resolucion_diseno (ancho)
        int designH = 1080;          // resolucion_diseno (alto)

        // ─── Tier 2 — Transiciones de pantalla (D-14, DISENO_MENU_RENPY §11.7d)
        // Bloque `transiciones {}`. Los defaults SON la conducta previa al lote
        // (`fade 0.5` en los dos momentos), así que un proyecto sin el bloque se
        // ve exactamente igual que antes: `D-13·3`, perilla antes del retiro.
        // La ley que decide cuál momento corre vive en `TransicionPantalla.h`;
        // acá sólo se transporta lo que el creador escribió.
        TransicionesTabla transiciones;

        // ─── Tier 1 — Personajes ─────────────────────────────────────────────
        // alias → datos del personaje. Map para lookup O(log n) en EclipParser.
        std::map<std::string, PersonajeManifest> personajes;

        // ─── Estilos — paquetes fuente+color+glow reutilizables ──────────────
         // nombre → datos del estilo. Map para lookup O(log n) en cmdTexto.
        std::map<std::string, EstiloManifest> estilos;
        // nombre → paquete visual de controles (`estilo_control`/`control_style`).
        std::map<std::string, ControlStyleManifest> estilosControl;

        // ─── Imágenes — registro nombre lógico → ruta (bucket simple, §13) ────
        // nombre → ruta. Lookup O(log n) en resolverImagen (execute-time, Pasada 3).
        std::map<std::string, std::string> imagenes;

        // ─── Pantallas — override de pantalla: verbo → .ecl del creador.
        // Capa 2 / menú-front β. config/... → script que sustituye el State C++
        // (openSettings y familia consultan esto antes de pushear el fallback).
        // β rutea SOLO 'config'. Consulta en el helper open* (CP2).
        std::map<std::string, std::string> pantallas;

        // ─── Teclas — la capa del creador del átomo de teclado (CP3) ─────────
        //
        // **Esto es TRANSPORTE, no interpretación.** Cada fila sale del archivo
        // tal cual se escribió y nadie la valida acá: quién es un dominio, qué
        // es un verbo y qué nombres de tecla existen lo decide
        // `inputParsearCapaCreador` (`engine/input/InputManager.h`), que es
        // donde vive la ley y donde la batería la alcanza.
        //
        // **Y por eso este struct existe teniendo `InputFilaCreador` el mismo
        // contenido:** copiarlo cuesta cuatro campos sin semántica; incluir
        // `InputManager.h` acá le metería `<SDL_keycode.h>` a este header, y
        // `harness_grafia` y `harness_borde` —que lo incluyen— pasarían a
        // necesitar `EXTRAINC` en el `.bat`. El adaptador vive en `Engine`.
        struct TeclaFila
        {
            std::string dominio;   // el nombre del bloque: `teclas vn {`
            std::string verbo;     // la primera palabra del renglón
            std::string teclas;    // el resto, sin partir
            int         linea = 0;
        };
        std::vector<TeclaFila> teclas;

        // ─── Fuentes — el catálogo del proyecto (ATLAS-DE-FUENTE CP-R3) ──────
        //
        // **Esto es TRANSPORTE, no propiedad.** Acá viaja lo que el creador
        // escribió —nombre lógico, rutas en orden, tamaño, kerning, medida y
        // procedencia—; el manifest no adquiere un TTF, no arma un plan y no
        // materializa un atlas. La verdad compilada la posee el
        // `FontSourceStore` de `Engine` (`DA-20`) y el atlas su `FontManager`.
        //
        // Es un `vector` y no un `map` porque el ORDEN DE ARCHIVO es dato: el
        // conflicto conserva la primera declaración válida y el aviso cita las
        // dos líneas. `fuenteDeclarada` es el lookup por nombre.
        std::vector<fontdecl::Declaracion> fuentes;

        const fontdecl::Declaracion* fuenteDeclarada(const std::string& nombre) const
        {
            for (const auto& f : fuentes)
                if (f.name == nombre) return &f;
            return nullptr;
        }

        // ─── Estado ──────────────────────────────────────────────────────────
        bool        loaded = false;  // false = no había proyecto.eclips
        // loaded = el archivo ABRIÓ. parseValid = el parseo cerró todos los
        // bloques. Un fuente { sin } imprime error, descarta el perfil, y
        // hasta CP-R4 salía loaded=true. F8 no puede publicar eso.
        bool        parseValid = true;
        std::string sourcePath;      // De dónde se cargó (debug)

        // ─── Carga ───────────────────────────────────────────────────────────

        // Intenta cargar desde la raíz: proyecto.eclips primero, luego
        // project.eclips (D3 — equivalencia bilingüe). Si ninguno existe,
        // retorna manifest con loaded=false (no es error, es modo opcional).
        // Si existen ambos, gana proyecto.eclips y emite aviso.
        static ProjectManifest tryLoad()
        {
            const std::string esp = "proyecto.eclips";
            const std::string eng = "project.eclips";

            bool hasEsp = std::ifstream(esp).good();
            bool hasEng = std::ifstream(eng).good();

            if (hasEsp && hasEng)
            {
                std::cerr << "[Proyecto] Aviso: existen ambos 'proyecto.eclips' y "
                    << "'project.eclips' en la raiz.\n"
                    << "  Cargando 'proyecto.eclips' e ignorando 'project.eclips'.\n"
                    << "  Manten solo uno para evitar confusion."
                    << std::endl;
                return loadFromFile(esp);
            }
            if (hasEsp) return loadFromFile(esp);
            if (hasEng) return loadFromFile(eng);

            ProjectManifest empty;
            empty.loaded = false;
            return empty;
        }

        static ProjectManifest loadFromFile(const std::string& path)
        {
            ProjectManifest m;
            m.sourcePath = path;

            std::ifstream file(path);
            if (!file.is_open())
            {
                std::cerr << "[Proyecto] No se pudo abrir: " << path << std::endl;
                m.loaded = false;
                return m;
            }

            // ── State machine simple ──
            // El parser opera en uno de dos estados:
            //   TopLevel: keys planas (titulo, autor) o apertura de bloque.
            //   InsidePersonaje: keys del personaje hasta encontrar '}'.
            // EL MAPA DEL FOLD: LOS SIETE PUNTOS DE EXTRACCION DE `key`
            //
            // Este parser saca la clave con `ss >> key` en SIETE lugares, uno por
            // bloque, y NO todos pliegan. La diferencia no es descuido: es que en
            // este archivo -y en ninguno de los otros cuatro dialectos- la MISMA
            // variable sirve vocabulario del motor en unos bloques y NOMBRES QUE
            // INVENTA EL CREADOR en otros. Por eso el fold de `FOLD-PARTIDO` entro
            // por bloque y no en una puerta comun: una puerta comun habria vuelto
            // `Fondo` y `fondo` la misma imagen.
            //
            //   linea  bloque               key es...            pliega
            //   -----  -------------------  -------------------  ------
            //    480   InsidePersonaje      vocabulario          SI
            //    653   InsideControlEstilo  vocabulario          SI
            //    786   InsideEstilo         vocabulario          SI
            //    973   InsideImagenes       NOMBRE DEL CREADOR   NO   <- deliberado
            //   1049   InsidePantallas      MIXTA, ya avisa      NO   <- deliberado
            //   1224   InsideTeclas         vocabulario          SI
            //   1475   TopLevel             vocabulario          SI
            //
            // Las dos exenciones tienen su razon escrita EN SU SITIO, no aca, para
            // que el que lea el bloque la vea sin buscarla. Y las custodia la
            // seccion 6 de `harness_mayusculas`, que exige que `Fondo` y `fondo`
            // sigan siendo DOS imagenes: sin ese control negativo, el dia que
            // alguien "termine el trabajo" bajando el fold a una puerta comun, dos
            // nombres del creador se vuelven uno y la bateria dice VERDE.
            //
            // Los numeros de linea envejecen; el que manda es el BLOQUE.
            enum class State { TopLevel, InsidePersonaje, InsideEstilo, InsideControlEstilo, InsideImagenes, InsidePantallas, InsideTeclas, InsideTransiciones, InsideFuente };
            State            state = State::TopLevel;
            PersonajeManifest current;        // personaje en construcción
            fontdecl::Declaracion currentFuente;  // perfil de fuente en construcción
            EstiloManifest   currentEstilo;   // estilo en construcción
            ControlStyleManifest currentControlEstilo;
            bool              estiloTieneLista = false;
            bool              estiloTieneBordeLegacy = false;
            std::string      currentTeclasDom; // dominio del bloque `teclas X {`
            int              blockStartLine = 0;

            // ── CP-R3: la voz y la ley de conflicto del catálogo de fuentes ──
            // El CUERPO del mensaje sale de `fontdecl::cuerpoError`, que es el
            // mismo que imprime el `.ecl`; lo único de este dialecto es el
            // prefijo. Así un harness puede exigir que los dos digan lo mismo.
            auto vozFuente = [&](int ln, fontdecl::Error e,
                                 const std::string& detalle = std::string())
            {
                std::cerr << "[proyecto.eclips] Aviso linea " << ln << ": "
                    << fontdecl::cuerpoError(e, detalle) << std::endl;
            };
            // Dentro del mismo ámbito: repetición idéntica = no-op; conflicto =
            // gana la primera declaración válida y la voz cita las dos líneas.
            // No hay last-write silencioso.
            auto commitFuente = [&](const fontdecl::Declaracion& d, int ln)
            {
                for (const auto& f : m.fuentes)
                {
                    if (f.name != d.name) continue;
                    if (f.mismaFormaQue(d)) return;
                    std::cerr << "[proyecto.eclips] Aviso linea " << ln
                        << ": la fuente '" << d.name
                        << "' ya se declaro en la linea " << f.sourceLine
                        << " con otra forma. Se conserva la primera."
                        << std::endl;
                    return;
                }
                m.fuentes.push_back(d);
            };

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line))
            {
                ++lineNum;

                // Quitar comentarios '#' — pero respetando colores hex (#RRGGBB).
                // El bloque 'estilo' mete hex al manifest por 1ra vez; sin esto,
                // 'color #6BF0FF' perderia su valor (§1.4). Copiado de Theme.h.
                size_t commentPos = std::string::npos;
                for (size_t i = 0; i < line.size(); ++i)
                {
                    if (line[i] != '#') continue;
                    char next = (i + 1 < line.size()) ? line[i + 1] : '\0';
                    size_t tokenEnd = i + 1;
                    while (tokenEnd < line.size() &&
                           !std::isspace(static_cast<unsigned char>(line[tokenEnd])))
                        ++tokenEnd;
                    // Preserve every # + six-character token for semantic
                    // validation, including invalid hex; longer hashes remain
                    // comments and never enter the control-style parser.
                    if (tokenEnd - i == 7 && state == State::InsideControlEstilo)
                        continue;
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
                line = pm_detail::trim(line);
                if (line.empty()) continue;

                // ─── Estado: dentro de bloque fuente (CP-R3) ────────────────
                // El bloque es un PERFIL, no otra clase: sólo acepta rutas
                // hijas. Y no degrada en secreto: si nunca llega su '}', el
                // perfil completo se descarta — quedarse con la base sola
                // sería darle al creador una cadena que él no escribió.
                if (state == State::InsideFuente)
                {
                    if (line == "}")
                    {
                        commitFuente(currentFuente, lineNum);
                        currentFuente = fontdecl::Declaracion{};
                        state = State::TopLevel;
                        continue;
                    }

                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": bloque fuente '" << currentFuente.name
                            << "' (linea " << blockStartLine
                            << ") no se cerro con '}' antes de abrir otro. "
                            << "Perfil descartado." << std::endl;
                        currentFuente = fontdecl::Declaracion{};
                        state = State::TopLevel;
                        // Caer al procesado normal de TopLevel (abajo)
                    }
                    else
                    {
                        std::string rutaRespaldo;
                        bool esPalabraRespaldo = false;
                        if (fontdecl::partirRespaldo(line, rutaRespaldo,
                                                     &esPalabraRespaldo))
                        {
                            // Orden autoral, sin reordenar y sin deduplicar:
                            // una ruta repetida conserva su posición y `DA-21`
                            // mantiene a la primera como ganadora.
                            currentFuente.routes.push_back(rutaRespaldo);
                        }
                        else if (esPalabraRespaldo)
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea "
                                << lineNum << ": 'respaldo' espera UNA ruta "
                                << "entre comillas (ej: respaldo "
                                << "\"NotoSans.ttf\"). Linea ignorada."
                                << std::endl;
                        }
                        else
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea "
                                << lineNum
                                << ": dentro del bloque de la fuente '"
                                << currentFuente.name
                                << "' solo se escriben lineas 'respaldo "
                                << "\"ruta.ttf\"' (o 'fallback'). Linea "
                                << "ignorada: '" << line << "'." << std::endl;
                        }
                        continue;
                    }
                }

                // ─── Estado: dentro de bloque personaje ─────────────────────
                if (state == State::InsidePersonaje)
                {
                    // Cierre del bloque
                    if (line == "}")
                    {
                        if (current.velocidadLock && !current.velocidadTexto)
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << " (personaje '" << current.alias
                                << "'): velocidad_lock si sin velocidad_texto; "
                                << "el lock queda inerte. Declara una velocidad_texto "
                                << "positiva y finita para bloquearla."
                                << std::endl;
                            current.velocidadLock = false;
                        }
                        if (current.alias.empty())
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": bloque personaje sin alias. Ignorado."
                                << std::endl;
                        }
                        else if (m.personajes.count(current.alias) > 0)
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": personaje '" << current.alias
                                << "' ya estaba definido. Sobreescribiendo."
                                << std::endl;
                            m.personajes[current.alias] = current;
                        }
                        else
                        {
                            m.personajes[current.alias] = current;
                        }
                        current = PersonajeManifest{};
                        state = State::TopLevel;
                        continue;
                    }

                    // Apertura de otro bloque sin cerrar el actual = error
                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": bloque personaje '" << current.alias
                            << "' (linea " << blockStartLine
                            << ") no se cerro con '}' antes de abrir otro. Abortando bloque."
                            << std::endl;
                        current = PersonajeManifest{};
                        state = State::TopLevel;
                        // Caer al procesado normal de TopLevel (abajo)
                    }
                    else
                    {
                        // Key-value dentro del bloque
                        std::istringstream ss(line);
                        std::string key;
                        ss >> key;
                        if (key.empty()) continue;
                        // FOLD-PARTIDO: VOCABULARIO -> pliega (bloque `personaje`).
                        // El mapa de los SIETE puntos de extraccion de este archivo, con
                        // la razon de cada uno, esta junto a `enum class State`.
                        key = eclips::vocabToLower(key);

                        std::string valueStr;
                        std::getline(ss, valueStr);
                        valueStr = pm_detail::trim(valueStr);

                        // Rechazo pedagógico de ':' decorativo
                        {
                            bool hasColon = false;
                            if (!key.empty() && key.back() == ':') hasColon = true;
                            else if (!valueStr.empty() && valueStr.front() == ':') hasColon = true;
                            if (hasColon)
                            {
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum
                                    << " (dentro de personaje '" << current.alias
                                    << "', key '" << key << "'):\n"
                                    << "  Los .eclips no usan ':' (eso es solo para .ecl).\n"
                                    << "  Linea ignorada. Quita el ':' y vuelve a correr."
                                    << std::endl;
                                continue;
                            }
                        }

                        std::string value = pm_detail::stripQuotes(valueStr);

                        if (key == "nombre" || key == "name")
                            current.nombre = value;
                        else if (key == "path" || key == "ruta")
                            current.path = value;
                        else if (key == "compuesto" || key == "composite")
                            current.compuesto = pm_detail::parseBool(valueStr);
                        else if (key == "velocidad_texto" || key == "text_speed")
                        {
                            const auto parsed = parseTextSpeedCps(valueStr);
                            if (parsed)
                                current.velocidadTexto = *parsed;
                            else
                                std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << " (personaje '" << current.alias
                                << "'): velocidad_texto invalida '" << valueStr
                                << "'; se conserva el valor anterior o ausencia."
                                << std::endl;
                        }
                        else if (key == "velocidad_lock" || key == "speed_lock")
                            current.velocidadLock = pm_detail::parseBool(valueStr);
                        else if (key == "caja" || key == "box")
                            current.caja = value;
                        else if (key == "escala_base" || key == "base_scale")
                            current.escalaBase = pm_detail::parseFloat(valueStr, 1.0f);
                        else if (key == "ancla_y" || key == "anchor_y")
                            current.anclaY = pm_detail::parseFloat(valueStr, 0.0f);
                        else if (key == "comillas" || key == "quotes")
                        {
                            current.comillas = value;
                            // Anti-silencio (§7.58/§7.59): linea 'comillas' presente
                            // pero el valor quedo vacio. Caso tipico: comillas "" —
                            // las dos " son el delimitador, stripQuotes las pela y
                            // no queda glifo. Avisar en vez de no hacer nada mudo.
                            if (value.empty())
                                std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << " (personaje '" << current.alias
                                << "'): 'comillas' quedo vacio -> sin comillas. "
                                << "Las \" delimitan; para dobles rectas escribi "
                                << "cuatro: comillas \"\"\"\"." << std::endl;
                        }
                        else if (key == "apariencia" || key == "look")
                        {
                            // C-ALIAS (flat, una por linea): 'apariencia NOMBRE COMPUESTO'.
                            // Partir el valor en nombre + cadena compuesta; ambos sin
                            // comillas (tokens interpretados, §7.44).
                            std::istringstream as(value);
                            std::string apName, apExpr;
                            as >> apName;
                            std::getline(as, apExpr);
                            apExpr = pm_detail::trim(apExpr);
                            if (apName.empty() || apExpr.empty())
                            {
                                std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                    << " (personaje '" << current.alias
                                    << "'): 'apariencia' necesita NOMBRE y COMPUESTO "
                                    << "(ej: apariencia feliz 1l_2r_a). Ignorada."
                                    << std::endl;
                            }
                            else
                                current.apariencias[apName] = apExpr;
                        }
                        else if (key == "apariencia_default" || key == "look_default")
                        {
                            current.defaultApariencia = value;
                        }
                        else
                        {
                            std::cerr << "[proyecto.eclips] Aviso en '" << path
                                << "' linea " << lineNum
                                << ": key desconocida '" << key
                                << "' dentro de personaje '" << current.alias
                                << "'. Ignorada." << std::endl;
                        }
                        continue;
                    }
                }

                // ─── Estado: dentro de bloque estilo_control ────────────────
                if (state == State::InsideControlEstilo)
                {
                    if (line == "}")
                    {
                        if (currentControlEstilo.nombre.empty())
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": bloque estilo_control sin nombre. Ignorado." << std::endl;
                        else
                        {
                            // A focus variant equal to its BASE is legal but
                            // cannot add a visible signal; report it once at
                            // parse time instead of every render frame.
                            auto sameVisual = [](const ControlStylePart& a,
                                                 const ControlStylePart& b) {
                                if (a.none || b.none) return a.none == b.none;
                                if (a.color != b.color) return false;
                                if (a.color)
                                {
                                    std::string av=a.value, bv=b.value;
                                    std::transform(av.begin(), av.end(), av.begin(),
                                        [](unsigned char c){ return (char)std::tolower(c); });
                                    std::transform(bv.begin(), bv.end(), bv.begin(),
                                        [](unsigned char c){ return (char)std::tolower(c); });
                                    return av == bv;
                                }
                                return a.value == b.value;
                            };
                            for (const auto& v : currentControlEstilo.variants)
                            {
                                if (v.first.size() <= 13 ||
                                    v.first.rfind("_al_enfocar") == std::string::npos) continue;
                                if (!v.second.valid) continue;
                                const std::string component = v.first.substr(0, v.first.size()-11);
                                const ControlStylePart* basePart = component == "track"
                                    ? &currentControlEstilo.track
                                    : component == "fill" ? &currentControlEstilo.fill
                                    : &currentControlEstilo.thumb;
                                if (basePart->declared && sameVisual(v.second, *basePart))
                                    std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                        << "' linea " << v.second.line << " alias='"
                                        << currentControlEstilo.nombre << "' propiedad='"
                                        << v.second.property << "' estado='al_enfocar': variante"
                                        << " identica a BASE; consecuencia: conserva la marca de foco;"
                                        << " reparacion: declare un valor distinto o quite la variante.\n";
                            }
                            // `B1`, corolario: el COLOR es el interruptor del
                            // borde, asi que un grosor declarado SIN color no
                            // dibuja nada. Un no-op silencioso es el defecto que
                            // ficha `ECLMAP-BOOLEANOS-PROPIOS`: lo que cae a un
                            // default SIN VOZ es el problema, no el valor.
                            //
                            // Va al CERRAR el bloque y no al parsear la clave,
                            // porque el color puede venir en un renglon posterior
                            // -- aca ya vive el aviso de *variante identica a
                            // BASE*, que es la misma clase de chequeo.
                            {
                                struct Huerfano {
                                    bool grosor; bool color;
                                    const char* pieza; const char* claveColor;
                                };
                                const Huerfano kH[] = {
                                    { currentControlEstilo.rielBordeGrosorDeclarada,
                                      currentControlEstilo.rielBordeColorDeclarada,
                                      "riel", "riel_borde_color" },
                                    { currentControlEstilo.perillaBordeGrosorDeclarada,
                                      currentControlEstilo.perillaBordeColorDeclarada,
                                      "perilla", "perilla_borde_color" },
                                };
                                for (const auto& h : kH)
                                    if (h.grosor && !h.color)
                                        std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                            << "' linea " << blockStartLine << " alias='"
                                            << currentControlEstilo.nombre << "' propiedad='"
                                            << h.pieza << "_borde_grosor' estado='base': grosor de"
                                            << " borde sin color; consecuencia: no se dibuja borde;"
                                            << " reparacion: agregue '" << h.claveColor << "'."
                                            << std::endl;
                            }
                            // FORMAS_01 - LA FRONTERA ARTE/PRIMITIVA, CON VOZ
                            //
                            // HC-BOR-07: una pieza que resolvio a TEXTURA ya
                            // trae su silueta en los pixeles, asi que no se le
                            // dibuja una primitiva encima. Heredar esa ley EN
                            // SILENCIO seria MAGIA: el creador escribe rombo,
                            // ve su PNG igual que siempre, y no tiene como
                            // saber por que. Asi que el motor lo dice.
                            //
                            // Va al CERRAR el bloque y no al parsear la clave,
                            // por la misma razon que su hermano de borde: la
                            // perilla puede declararse en un renglon POSTERIOR,
                            // y avisar antes seria avisar sobre algo que el
                            // creador todavia no termino de escribir.
                            if (currentControlEstilo.perillaFormaDeclarada &&
                                currentControlEstilo.thumb.declared &&
                                currentControlEstilo.thumb.valid &&
                                !currentControlEstilo.thumb.none &&
                                !currentControlEstilo.thumb.color)
                                std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                    << "' linea " << blockStartLine << " alias='"
                                    << currentControlEstilo.nombre
                                    << "' propiedad='perilla_forma' estado='base': la"
                                    << " perilla resolvio a una imagen; consecuencia: la forma"
                                    << " no se aplica, el arte ya trae su silueta; reparacion:"
                                    << " quite 'perilla_forma', o use un color en 'perilla'."
                                    << std::endl;
                            if (m.estilosControl.count(currentControlEstilo.nombre) > 0)
                                std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                    << "' linea " << blockStartLine << " alias='"
                                    << currentControlEstilo.nombre << "' propiedad='estilo_control': bloque duplicado;"
                                    << " consecuencia: gana la ultima declaracion; reparacion: elimine la repetida." << std::endl;
                            m.estilosControl[currentControlEstilo.nombre] = currentControlEstilo;
                        }
                        currentControlEstilo = ControlStyleManifest{};
                        state = State::TopLevel;
                        continue;
                    }
                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": estilo_control anidado o sin cierre. Ignorado." << std::endl;
                        currentControlEstilo = ControlStyleManifest{};
                        state = State::TopLevel;
                    }
                    else
                    {
                        std::istringstream ss(line);
                        std::string key; ss >> key;
                        std::string valueStr; std::getline(ss, valueStr);
                        valueStr = pm_detail::trim(valueStr);
                        if (key.empty()) continue;
                        // FOLD-PARTIDO: VOCABULARIO -> pliega (riel/relleno/perilla y sus
                        // sufijos de estado; ranura PURA).
                        key = eclips::vocabToLower(key);
                        std::string value = pm_detail::stripQuotes(valueStr);
                        // ── `D-52`..`D-56` · LA GEOMETRIA: ENTERA, Y ANTES DEL SPLIT ──
                        //
                        // Cuatro lineas mas abajo, el PRIMER guion bajo de este
                        // bloque significa ESTADO. Una clave de geometria no es
                        // `<pieza>_<estado>`: es una clave COMPLETA. Si cayera al
                        // split, `riel_altura` se leeria como *el riel en estado
                        // 'altura'* y moriria pidiendole al creador que use
                        // `hover/focus/dragging/locked` — que para una magnitud es
                        // una instruccion imposible.
                        //
                        // Por eso la lista es CERRADA y se consulta PRIMERO. Y por
                        // eso `D-54` decide que la geometria NO TIENE ESTADOS:
                        // `riel_altura_al_pasar` no entra aca y recibe voz propia,
                        // en vez de la generica de estado que le mentiria.
                        {
                            struct GeoRanura { const char* es; const char* en; };
                            static const GeoRanura kGeo[] = {
                                { "riel_altura",         "track_height"  },   // 0
                                { "perilla_ancho",       "thumb_width"   },   // 1
                                { "perilla_altura",      "thumb_height"  },   // 2
                                { "perilla_en_extremos", "thumb_at_ends" },   // 3
                            };
                            int cual = -1;
                            for (int i = 0; i < 4; ++i)
                                if (key == kGeo[i].es || key == kGeo[i].en) { cual = i; break; }

                            // `D-54`, corolario 2: `<clave de geometria>_<algo>`
                            // no es un estado, es un error con nombre propio.
                            if (cual < 0)
                                for (int i = 0; i < 4 && cual < 0; ++i)
                                {
                                    const std::string pes = std::string(kGeo[i].es) + "_";
                                    const std::string pen = std::string(kGeo[i].en) + "_";
                                    if (key.rfind(pes, 0) == 0 || key.rfind(pen, 0) == 0)
                                    {
                                        std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                            << "' linea " << lineNum << " alias='"
                                            << currentControlEstilo.nombre << "' propiedad='" << key
                                            << "' estado='base': la geometria no tiene estados;"
                                            << " consecuencia: se ignora; reparacion: escriba '"
                                            << kGeo[i].es << "' sin sufijo: el tamano de una pieza"
                                            << " es uno solo para todos sus estados." << std::endl;
                                        cual = -2;
                                    }
                                }
                            if (cual == -2) continue;

                            if (cual >= 0)
                            {
                                const std::string low4 = eclips::vocabToLower(value);
                                if (cual == 3)
                                {
                                    // Ranura PURA de vocabulario: pliega (`FOLD-PARTIDO`).
                                    if (low4 == "centrada" || low4 == "centered")
                                        currentControlEstilo.perillaDentro = false;
                                    else if (low4 == "dentro" || low4 == "inside")
                                        currentControlEstilo.perillaDentro = true;
                                    else
                                    {
                                        std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                            << "' linea " << lineNum << " alias='"
                                            << currentControlEstilo.nombre << "' propiedad='" << key
                                            << "' estado='base': valor invalido ('" << value
                                            << "'); consecuencia: se conserva 'centrada';"
                                            << " reparacion: use centrada/centered o dentro/inside."
                                            << std::endl;
                                        continue;
                                    }
                                    currentControlEstilo.perillaExtremosDeclarada = true;
                                    continue;
                                }

                                // Las tres magnitudes. El `pos` es la parte que
                                // `pm_detail::parseFloat` no puede dar: distingue
                                // `43` de `43px` de `ancho`, y sin eso un typo
                                // entraria como el fallback SIN UNA PALABRA.
                                float px = 0.0f; bool numero = false;
                                try
                                {
                                    size_t pos = 0;
                                    px = std::stof(value, &pos);
                                    numero = (pos == value.size());
                                }
                                catch (...) { numero = false; }
                                if (!numero || px <= 0.0f)
                                {
                                    std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                        << "' linea " << lineNum << " alias='"
                                        << currentControlEstilo.nombre << "' propiedad='" << key
                                        << "' estado='base': se esperaba un numero de pixeles"
                                        << " mayor que cero ('" << value << "');"
                                        << " consecuencia: se conserva el default;"
                                        << " reparacion: escriba un numero, por ejemplo 43."
                                        << std::endl;
                                    continue;
                                }
                                if (cual == 0)
                                {
                                    currentControlEstilo.rielAltura = px;
                                    currentControlEstilo.rielAlturaDeclarada = true;
                                }
                                else if (cual == 1)
                                {
                                    currentControlEstilo.perillaAncho = px;
                                    currentControlEstilo.perillaAnchoDeclarada = true;
                                }
                                else
                                {
                                    currentControlEstilo.perillaAltura = px;
                                    currentControlEstilo.perillaAlturaDeclarada = true;
                                }
                                continue;
                            }
                        }
                        // ── `BORDE_Y_FORMAS_01` · `A1`+`B1`+`C3` · EL BORDE ──
                        //
                        // Misma trampa que la geometria, y por eso el mismo
                        // mecanismo: cuatro renglones mas abajo el PRIMER guion
                        // bajo ya significa ESTADO, asi que `riel_borde_color` se
                        // leeria como *el riel en estado 'borde_color'* y moriria
                        // pidiendole al creador `hover/focus/dragging/locked`.
                        // Lista CERRADA, matcheada COMPLETA, y ANTES del split.
                        //
                        // `C3`: el borde NO tiene estados — pero la razon NO es la
                        // de `D-54`, y la diferencia importa. La geometria no los
                        // tiene porque un TAMANO es uno solo por pieza: un
                        // argumento sobre la magnitud, y por eso irreversible. Un
                        // COLOR de borde no es un tamano. La razon firmada aca es
                        // CERO CALLERS MEDIDOS -los dos bordes de las referencias
                        // son de estado base-, y esa se revisa cuando aparezca uno.
                        // Por eso la voz de abajo dice *no tiene estados* sin
                        // invocar a `D-54`.
                        //
                        // `B1`: el COLOR es el interruptor. Un grosor solo no
                        // dibuja nada, y por eso avisa en vez de callarse.
                        {
                            struct BorRanura { const char* es; const char* en; };
                            static const BorRanura kBor[] = {
                                { "riel_borde_color",     "track_border_color"     },   // 0
                                { "riel_borde_grosor",    "track_border_thickness" },   // 1
                                { "perilla_borde_color",  "thumb_border_color"     },   // 2
                                { "perilla_borde_grosor", "thumb_border_thickness" },   // 3
                            };
                            int cual = -1;
                            for (int i = 0; i < 4; ++i)
                                if (key == kBor[i].es || key == kBor[i].en) { cual = i; break; }

                            // `C3`, corolario: `<clave de borde>_<algo>` no es un
                            // estado, es un error con nombre propio.
                            if (cual < 0)
                                for (int i = 0; i < 4 && cual < 0; ++i)
                                {
                                    const std::string pes = std::string(kBor[i].es) + "_";
                                    const std::string pen = std::string(kBor[i].en) + "_";
                                    if (key.rfind(pes, 0) == 0 || key.rfind(pen, 0) == 0)
                                    {
                                        std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                            << "' linea " << lineNum << " alias='"
                                            << currentControlEstilo.nombre << "' propiedad='" << key
                                            << "' estado='base': el borde no tiene estados;"
                                            << " consecuencia: se ignora; reparacion: escriba '"
                                            << kBor[i].es << "' sin sufijo: hoy el borde de una"
                                            << " pieza es uno solo para todos sus estados."
                                            << std::endl;
                                        cual = -2;
                                    }
                                }
                            if (cual == -2) continue;

                            if (cual >= 0)
                            {
                                if (cual == 0 || cual == 2)
                                {
                                    // La puerta del `#`, compartida con el camino
                                    // de las piezas via `pm_detail::hexValido`.
                                    if (!pm_detail::hexValido(value))
                                    {
                                        std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                            << "' linea " << lineNum << " alias='"
                                            << currentControlEstilo.nombre << "' propiedad='" << key
                                            << "' estado='base': color invalido ('" << value
                                            << "'); consecuencia: se ignora y la pieza queda sin"
                                            << " borde; reparacion: use #RRGGBB." << std::endl;
                                        continue;
                                    }
                                    if (cual == 0)
                                    {
                                        currentControlEstilo.rielBordeColor = value;
                                        currentControlEstilo.rielBordeColorDeclarada = true;
                                    }
                                    else
                                    {
                                        currentControlEstilo.perillaBordeColor = value;
                                        currentControlEstilo.perillaBordeColorDeclarada = true;
                                    }
                                    continue;
                                }

                                // El grosor. El `pos` distingue `3` de `3px` de
                                // `abc`, igual que las magnitudes de `D-52`.
                                float px = 0.0f; bool numero = false;
                                try
                                {
                                    size_t pos = 0;
                                    px = std::stof(value, &pos);
                                    numero = (pos == value.size());
                                }
                                catch (...) { numero = false; }
                                if (!numero || px <= 0.0f)
                                {
                                    std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                        << "' linea " << lineNum << " alias='"
                                        << currentControlEstilo.nombre << "' propiedad='" << key
                                        << "' estado='base': se esperaba un numero de pixeles"
                                        << " mayor que cero ('" << value << "');"
                                        << " consecuencia: se conserva el default;"
                                        << " reparacion: escriba un numero, por ejemplo 1."
                                        << std::endl;
                                    continue;
                                }
                                if (cual == 1)
                                {
                                    currentControlEstilo.rielBordeGrosor = px;
                                    currentControlEstilo.rielBordeGrosorDeclarada = true;
                                }
                                else
                                {
                                    currentControlEstilo.perillaBordeGrosor = px;
                                    currentControlEstilo.perillaBordeGrosorDeclarada = true;
                                }
                                continue;
                            }
                        }
                        // FORMAS_01 - LA SILUETA DE LA PERILLA
                        //
                        // TERCERA vez que este bloque necesita el mismo
                        // mecanismo -la geometria de D-54 y el borde de C3 son
                        // las otras dos- y por la misma razon: dos renglones
                        // mas abajo el PRIMER guion bajo ya significa ESTADO,
                        // asi que perilla_forma se leeria como *la perilla en
                        // estado forma* y moriria pidiendole al creador
                        // hover/focus/dragging/locked -que para una silueta es
                        // una instruccion imposible-. Lista CERRADA, matcheada
                        // COMPLETA, y ANTES del split.
                        //
                        // Solo la PERILLA: riel_forma y relleno_forma tienen
                        // CERO callers medidos y NO entran (7.118). Caen al
                        // split de abajo, que ya los rechaza con voz; lo que
                        // esta prohibido es que entren MUDOS.
                        {
                            const bool esForma =
                                (key == "perilla_forma" || key == "thumb_shape");

                            // Corolario: <clave de forma>_<algo> no es un
                            // estado, es un error con nombre propio. La voz NO
                            // ofrece los cuatro estados a proposito: ofrecerlos
                            // seria mentirle al creador sobre lo que le falta.
                            if (!esForma &&
                                (key.rfind("perilla_forma_", 0) == 0 ||
                                 key.rfind("thumb_shape_", 0) == 0))
                            {
                                std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                    << "' linea " << lineNum << " alias='"
                                    << currentControlEstilo.nombre << "' propiedad='" << key
                                    << "' estado='base': la forma no tiene estados;"
                                    << " consecuencia: se ignora; reparacion: escriba"
                                    << " 'perilla_forma' sin sufijo: hoy la silueta de una"
                                    << " pieza es una sola para todos sus estados." << std::endl;
                                continue;
                            }

                            if (esForma)
                            {
                                // Ranura PURA de vocabulario: pliega el VALOR
                                // igual que perilla_en_extremos (FOLD-PARTIDO).
                                const std::string lowF = eclips::vocabToLower(value);
                                // F2(a): las TRES grafias del rectangulo. La
                                // tildada va A MANO porque vocabToLower pliega
                                // CASE y NO pela acentos -- es exactamente la
                                // fila que MapData.h ya escribe para su marcador
                                // espacial (parseSpatialMarkerBlock). Sin ella,
                                // la tildada moriria MUDA.
                                if (lowF == "rombo" || lowF == "diamond")
                                    currentControlEstilo.perillaForma =
                                        ControlFormaPieza::Rombo;
                                else if (lowF == "rectangulo" ||
                                         lowF == "rect\xC3\xA1ngulo" ||
                                         lowF == "rectangle")
                                    currentControlEstilo.perillaForma =
                                        ControlFormaPieza::Rectangulo;
                                else
                                {
                                    // La voz ENUMERA el conjunto: es cerrado, y
                                    // uno cerrado que no lista sus miembros deja
                                    // al creador adivinando (P5 del 2.9).
                                    std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                        << "' linea " << lineNum << " alias='"
                                        << currentControlEstilo.nombre << "' propiedad='" << key
                                        << "' estado='base': forma desconocida ('" << value
                                        << "'); consecuencia: se conserva rectangulo;"
                                        << " reparacion: use rombo/diamond o"
                                        << " rectangulo/rectangle." << std::endl;
                                    continue;
                                }
                                currentControlEstilo.perillaFormaDeclarada = true;
                                continue;
                            }
                        }
                        std::string base = key;
                        std::string stateSuffix;
                        const size_t us = key.find('_');
                        if (us != std::string::npos) { base = key.substr(0, us); stateSuffix = key.substr(us + 1); }
                        if (base != "riel" && base != "track" && base != "relleno" &&
                            base != "fill" && base != "perilla" && base != "thumb")
                        {
                            std::cerr << "[proyecto.eclips] Aviso fuente='" << path << "' linea "
                                << lineNum << " alias='" << currentControlEstilo.nombre
                                << "' propiedad='" << key << "' estado='base': propiedad desconocida."
                                << " consecuencia: se ignora; reparacion: use riel/relleno/perilla"
                                << " y sufijos _al_pasar/_al_enfocar/_arrastrando/_bloqueado,"
                                << " la geometria riel_altura/perilla_ancho/perilla_altura/perilla_en_extremos,"
                                << " el borde riel_borde_color/riel_borde_grosor/perilla_borde_color/perilla_borde_grosor,"
                                << " o la forma perilla_forma."
                                << std::endl;
                            continue;
                        }
                        ControlStylePart part;
                        part.declared = true; part.line = lineNum;
                        const std::string low = eclips::vocabToLower(value);   /* P2-1: vocabulario del motor, ciego a mayusculas */
                        if (!stateSuffix.empty())
                        {
                            if (stateSuffix == "hover") stateSuffix = "al_pasar";
                            else if (stateSuffix == "focus") stateSuffix = "al_enfocar";
                            else if (stateSuffix == "dragging") stateSuffix = "arrastrando";
                            else if (stateSuffix == "locked") stateSuffix = "bloqueado";
                            else if (stateSuffix != "al_pasar" && stateSuffix != "al_enfocar" &&
                                     stateSuffix != "arrastrando" && stateSuffix != "bloqueado")
                            {
                                std::cerr << "[proyecto.eclips] Aviso fuente='" << path << "' linea " << lineNum
                                    << " alias='" << currentControlEstilo.nombre << "' propiedad='" << key
                                    << "' estado='" << stateSuffix << "': estado desconocido; consecuencia: se ignora;"
                                    << " reparacion: use hover/focus/dragging/locked.\n";
                                continue;
                            }
                        }
                        part.sourcePath = path;
                        part.styleName = currentControlEstilo.nombre;
                        part.property = key;
                        part.stateName = stateSuffix.empty() ? "base" : stateSuffix;
                        if (low == "ninguno" || low == "none") part.none = true;
                        else if (value[0] == '#')   /* P2-2: el numeral es la PUERTA; el largo es validez, no tipo */
                        {
                            // Extraido a `pm_detail::hexValido` (arriba) para
                            // que el borde de `estilo_control` -que entra por
                            // lista blanca ANTES del split y no llega hasta
                            // aca- comparta UNA definicion de color valido.
                            const bool hex = pm_detail::hexValido(value);
                            part.color = hex; part.valid = hex;
                            if (!hex) std::cerr << "[proyecto.eclips] Aviso fuente='" << path << "' linea " << lineNum
                                << " alias='" << currentControlEstilo.nombre << "' propiedad='" << key
                                << "' estado='" << part.stateName << "': color invalido; consecuencia: fallback;"
                                << " reparacion: use #RRGGBB." << std::endl;
                        }
                        else if (value.empty())
                        {
                            std::cerr << "[proyecto.eclips] Aviso fuente='" << path << "' linea " << lineNum
                                << " alias='" << currentControlEstilo.nombre << "' propiedad='" << key
                                << "' estado='" << part.stateName << "': valor vacio; consecuencia: se ignora;"
                                << " reparacion: use color, ruta o ninguno." << std::endl;
                            continue;
                        }
                        part.value = value;
                        const std::string canonical = (base == "riel" || base == "track") ? "track" :
                            (base == "relleno" || base == "fill") ? "fill" : "thumb";
                        if (stateSuffix.empty())
                        {
                            ControlStylePart* dst = canonical == "track" ? &currentControlEstilo.track :
                                canonical == "fill" ? &currentControlEstilo.fill : &currentControlEstilo.thumb;
                            if (dst->declared) std::cerr << "[proyecto.eclips] Aviso fuente='" << path
                                << "' linea " << lineNum << " alias='" << currentControlEstilo.nombre
                                << "' propiedad='" << key << "' estado='" << part.stateName
                                << "': duplicado; consecuencia: gana la ultima; reparacion: elimine la repetida." << std::endl;
                            *dst = part;
                        }
                        else
                        {
                            const std::string variantKey = canonical + "_" + stateSuffix;
                            if (currentControlEstilo.variants.count(variantKey))
                                std::cerr << "[proyecto.eclips] Aviso fuente='" << path << "' linea " << lineNum
                                    << " alias='" << currentControlEstilo.nombre << "' propiedad='" << key
                                    << "' estado='" << part.stateName << "': variante duplicada; consecuencia: gana la ultima;"
                                    << " reparacion: elimine la repetida.\n";
                            currentControlEstilo.variants[variantKey] = part;
                        }
                        continue;
                    }
                }

                // ─── Estado: dentro de bloque estilo ────────────────────────
                if (state == State::InsideEstilo)
                {
                    if (line == "}")
                    {
                        if (currentEstilo.nombre.empty())
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": bloque estilo sin nombre. Ignorado." << std::endl;
                        }
                        else if (m.estilos.count(currentEstilo.nombre) > 0)
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": estilo '" << currentEstilo.nombre
                                << "' ya estaba definido. Uso el ultimo." << std::endl;
                            m.estilos[currentEstilo.nombre] = currentEstilo;
                        }
                        else
                        {
                            m.estilos[currentEstilo.nombre] = currentEstilo;
                        }
                        currentEstilo = EstiloManifest{};
                        estiloTieneLista = false;
                        estiloTieneBordeLegacy = false;
                        state = State::TopLevel;
                        continue;
                    }

                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": bloque estilo '" << currentEstilo.nombre
                            << "' (linea " << blockStartLine
                            << ") no se cerro con '}' antes de abrir otro. Abortando bloque."
                            << std::endl;
                        currentEstilo = EstiloManifest{};
                        estiloTieneLista = false;
                        estiloTieneBordeLegacy = false;
                        state = State::TopLevel;
                        // Caer al procesado normal de TopLevel (abajo)
                    }
                    else
                    {
                        std::istringstream ss(line);
                        std::string key;
                        ss >> key;
                        if (key.empty()) continue;
                        // FOLD-PARTIDO: VOCABULARIO -> pliega (bloque `estilo`).
                        key = eclips::vocabToLower(key);

                        std::string valueStr;
                        std::getline(ss, valueStr);
                        valueStr = pm_detail::trim(valueStr);

                        // Rechazo pedagógico de ':' decorativo
                        {
                            bool hasColon = false;
                            if (!key.empty() && key.back() == ':') hasColon = true;
                            else if (!valueStr.empty() && valueStr.front() == ':') hasColon = true;
                            if (hasColon)
                            {
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum
                                    << " (dentro de estilo '" << currentEstilo.nombre
                                    << "', key '" << key << "'):\n"
                                    << "  Los .eclips no usan ':' (eso es solo para .ecl).\n"
                                    << "  Linea ignorada. Quita el ':' y vuelve a correr."
                                    << std::endl;
                                continue;
                            }
                        }

                        std::string value = pm_detail::stripQuotes(valueStr);

                        const bool esTrazo = key == "trazo" || key == "outline";
                        const bool esTrazoHover = key == "trazo_hover" ||
                                                  key == "outline_hover";
                        if (esTrazo || esTrazoHover)
                        {
                            TextStrokeSpec stroke;
                            std::string error;
                            if (!parseTextStrokeSpec(valueStr, stroke, error))
                            {
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum << ": key '" << key
                                    << "' invalida (" << error << "). Ignorada."
                                    << std::endl;
                            }
                            else
                            {
                                if (estiloTieneBordeLegacy)
                                    std::cerr << "[proyecto.eclips] Aviso en '" << path
                                        << "' linea " << lineNum << ": la lista de trazos "
                                        << "se mezcla con borde_*; gana la lista atomica."
                                        << std::endl;
                                if (esTrazoHover)
                                {
                                    currentEstilo.trazosHover.push_back(stroke);
                                    currentEstilo.hasHover = true;
                                }
                                else
                                {
                                    currentEstilo.trazos.push_back(stroke);
                                }
                                estiloTieneLista = true;
                            }
                            continue;
                        }

                        if (key == "fuente" || key == "font")
                            currentEstilo.fontName = value;
                        else if (key == "color")
                            currentEstilo.colorHex = value;
                        else if (key == "glow_color" || key == "brillo_color")
                            currentEstilo.glowColorHex = value;
                        // ── SYN-4c-ii (Y-VN-01): las QUIMERAS, retiradas ──────
                        // `glow_radio`/`glow_intensidad` (y sus `_hover`) son
                        // prefijo INGLES + sufijo ESPANOL: un tercer idioma que
                        // el manual nunca prometio. El bloque `estilo` es el
                        // SEGUNDO vocabulario que las hablaba — el primero es
                        // `kvKeysOf(Texto)` en `EclipScript.h`, y retirarlas
                        // solo alli habria dejado al motor pidiendo
                        // `brillo_radio=` en un `.ecl` y `glow_radio` en
                        // `project.eclips`, que es peor que no tocar nada.
                        // La key desconocida ya tiene voz abajo (`else`), asi
                        // que un archivo viejo se entera con archivo y linea.
                        else if (key == "glow_radius" || key == "brillo_radio")
                            currentEstilo.glowRadius = pm_detail::parseFloat(valueStr);
                        else if (key == "glow_intensity" || key == "brillo_intensidad")
                            currentEstilo.glowIntensity = pm_detail::parseIntensity(valueStr);
                        else if (key == "color_hover")
                        {
                            currentEstilo.hoverColorHex = value; currentEstilo.hasHover = true;
                        }
                        else if (key == "glow_color_hover" || key == "brillo_color_hover")
                        {
                            currentEstilo.hoverGlowColorHex = value; currentEstilo.hasHover = true;
                        }
                        else if (key == "glow_radius_hover" || key == "brillo_radio_hover")
                        {
                            currentEstilo.hoverGlowRadius = pm_detail::parseFloat(valueStr); currentEstilo.hasHover = true;
                        }
                        else if (key == "glow_intensity_hover" || key == "brillo_intensidad_hover")
                        {
                            currentEstilo.hoverGlowIntensity = pm_detail::parseIntensity(valueStr); currentEstilo.hasHover = true;
                        }
                        // ── El BORDE (OUTLINE-TEXTO-CAPA) ────────────────────
                        // Este es el SEGUNDO vocabulario del texto-capa; el
                        // primero es `kvKeysOf(Texto)` en `EclipScript.h`.
                        // Cablear solo uno dejaria al motor pidiendo
                        // `borde_grosor=` en un `.ecl` y otra cosa aca — que es
                        // peor que no tocar nada (§7.380, literal).
                        //
                        // `outline_grosor` NO existe: EN + ES es la quimera que
                        // este mismo bloque acaba de retirar del glow. La key
                        // desconocida ya tiene voz abajo (`else`).
                        else if (key == "borde_color" || key == "outline_color")
                        {
                            if (estiloTieneLista)
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum << ": borde_* se mezcla con "
                                    << "una lista de trazos; gana la lista atomica."
                                    << std::endl;
                            estiloTieneBordeLegacy = true;
                            currentEstilo.bordeColorHex = value;
                        }
                        else if (key == "borde_grosor" || key == "outline_thickness")
                        {
                            if (estiloTieneLista)
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum << ": borde_* se mezcla con "
                                    << "una lista de trazos; gana la lista atomica."
                                    << std::endl;
                            estiloTieneBordeLegacy = true;
                            currentEstilo.bordeGrosor = pm_detail::parseFloat(valueStr);
                        }
                        else if (key == "borde_color_hover" || key == "outline_color_hover")
                        {
                            if (estiloTieneLista)
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum << ": borde_* se mezcla con "
                                    << "una lista de trazos; gana la lista atomica."
                                    << std::endl;
                            estiloTieneBordeLegacy = true;
                            currentEstilo.hoverBordeColorHex = value; currentEstilo.hasHover = true;
                        }
                        else if (key == "borde_grosor_hover" || key == "outline_thickness_hover")
                        {
                            if (estiloTieneLista)
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum << ": borde_* se mezcla con "
                                    << "una lista de trazos; gana la lista atomica."
                                    << std::endl;
                            estiloTieneBordeLegacy = true;
                            currentEstilo.hoverBordeGrosor = pm_detail::parseFloat(valueStr); currentEstilo.hasHover = true;
                        }
                        else
                        {
                            std::cerr << "[proyecto.eclips] Aviso en '" << path
                                << "' linea " << lineNum
                                << ": key desconocida '" << key
                                << "' dentro de estilo '" << currentEstilo.nombre
                                << "'. Ignorada." << std::endl;
                        }
                        continue;
                    }
                }


                // ─── Estado: dentro de bloque imagenes ──────────────────────
                // Bloque-registro (sin nombre en la apertura): cada línea es un
                // binding nombre→ruta, no un campo de keyword fijo. Por eso NO
                // hay if-chain de keys conocidas: TODA key es un nombre de imagen.
                if (state == State::InsideImagenes)
                {
                    if (line == "}")
                    {
                        state = State::TopLevel;
                        continue;
                    }

                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": bloque imagenes (linea " << blockStartLine
                            << ") no se cerro con '}' antes de abrir otro. Abortando bloque."
                            << std::endl;
                        state = State::TopLevel;
                        // Caer al procesado normal de TopLevel (abajo)
                    }
                    else
                    {
                        std::istringstream ss(line);
                        std::string key;
                        ss >> key;
                        if (key.empty()) continue;
                        // FOLD-PARTIDO: **NO SE PLIEGA, Y ES DELIBERADO.**
                        // Aca `key` NO es vocabulario: es el nombre que el creador le puso
                        // a la imagen -ver el comentario del bloque, arriba: "TODA key es
                        // un nombre de imagen"-. Plegar volveria `Fondo` y `fondo` la MISMA
                        // imagen, que es lo contrario de lo que N1 midio y el manual
                        // promete. Custodiado por `harness_mayusculas` seccion 6.

                        std::string valueStr;
                        std::getline(ss, valueStr);
                        valueStr = pm_detail::trim(valueStr);

                        // Rechazo pedagógico de ':' decorativo (coherencia §7.10)
                        {
                            bool hasColon = false;
                            if (!key.empty() && key.back() == ':') hasColon = true;
                            else if (!valueStr.empty() && valueStr.front() == ':') hasColon = true;
                            if (hasColon)
                            {
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum
                                    << " (dentro de imagenes, nombre '" << key << "'):\n"
                                    << "  Los .eclips no usan ':' (eso es solo para .ecl).\n"
                                    << "  Linea ignorada. Quita el ':' y vuelve a correr."
                                    << std::endl;
                                continue;
                            }
                        }

                        // Pelar PRIMERO, validar vacío DESPUÉS: `aula ""` deja
                        // valueStr no-vacío (2 chars) pero ruta real vacía. Las
                        // dos formas de "sin ruta" (`aula` y `aula ""`) deben
                        // rechazarse igual — completo, no a medias.
                        std::string value = pm_detail::stripQuotes(valueStr);
                        if (value.empty())
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": imagen '" << key << "' sin ruta. Ignorada."
                                << std::endl;
                            continue;
                        }

                        if (m.imagenes.count(key) > 0)
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": imagen '" << key
                                << "' ya estaba definida. Uso la ultima." << std::endl;
                        }
                        m.imagenes[key] = value;
                        continue;
                    }
                }

                // ─── Estado: dentro de bloque pantallas ─────────────────────
                // Bloque-registro: cada línea es verbo→ruta (override de pantalla).
                // A diferencia de imagenes, la clave NO es libre: es un verbo de
                // pantalla. β rutea solo 'config' (CP2), así que solo esa se acepta;
                // lo demás se avisa "aún no" en vez de registrarse muda (§7.118:
                // override que no dispara == no existe). Al cablear save/load/pause
                // (Olas 2-3), ensancha el whitelist abajo.
                if (state == State::InsidePantallas)
                {
                    if (line == "}")
                    {
                        state = State::TopLevel;
                        continue;
                    }

                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": bloque pantallas (linea " << blockStartLine
                            << ") no se cerro con '}' antes de abrir otro. Abortando bloque."
                            << std::endl;
                        state = State::TopLevel;
                        // Caer al procesado normal de TopLevel (abajo)
                    }
                    else
                    {
                        std::istringstream ss(line);
                        std::string key;
                        ss >> key;
                        if (key.empty()) continue;
                        // FOLD-PARTIDO: **NO SE PLIEGA, Y ES DELIBERADO.**
                        // Ranura MIXTA: la clave puede ser una de las siete nativas
                        // (vocabulario del motor) o un nombre libre del creador. No se
                        // pliega porque el lado libre YA exige minusculas por construccion
                        // (`esIdentificadorPantalla`), asi que una mayuscula hoy se RECHAZA
                        // CON VOZ -o sea que esta ranura ya CUMPLE la ley del 7.569, que
                        // pide aviso o ceguera, no ceguera si o si-. Plegar aca aceptaria
                        // en silencio un `MI_PANTALLA` que hoy tiene diagnostico.

                        std::string valueStr;
                        std::getline(ss, valueStr);
                        valueStr = pm_detail::trim(valueStr);

                        // Rechazo pedagógico de ':' decorativo (coherencia §7.10)
                        {
                            bool hasColon = false;
                            if (!key.empty() && key.back() == ':') hasColon = true;
                            else if (!valueStr.empty() && valueStr.front() == ':') hasColon = true;
                            if (hasColon)
                            {
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum
                                    << " (dentro de pantallas, verbo '" << key << "'):\n"
                                    << "  Los .eclips no usan ':' (eso es solo para .ecl).\n"
                                    << "  Linea ignorada. Quita el ':' y vuelve a correr."
                                    << std::endl;
                                continue;
                            }
                        }

                        // Whitelist: 'config' (Ola 0) + 'cargar'/'load' (Ola 2 · C1)
                        // + 'pausa'/'pause' (Ola 3 · Slice A: pausa navegacional)
                        // + 'guardar'/'save' (Ola 3 · Slice B: guardar-desde-pausa).
                        // Cada pantalla se whitelistea EN LOCKSTEP con su routing
                        // (§7.118 hecho estructura: dato que no dispara == no existe).
                        //
                        // Y AHORA HAY QUIEN LO MIRE. Esta linea decia *«'guardar'/'save'
                        // siguen fuera a proposito → Slice B»* mientras Slice B ya
                        // habia pasado (v0.6.19): el routing se cableo en
                        // VNScriptState.h:12208 y nadie volvio a esta linea, asi que el
                        // dato RECHAZABA lo que si disparaba. La regla estaba escrita
                        // -«el dato nunca acepta lo que no dispara», DISENO_MENU_RUTA §5-
                        // y cubria una sola direccion; el defecto entro por la otra y
                        // sobrevivio hasta 2026-08-19. `harness_pantallas` cruza esta
                        // lista contra los sitios de routing de VNScriptState.h y se
                        // pone rojo en LAS DOS.
                        const bool nativa = eclips::system_flow::esClavePantallaNativa(key);
                        const bool libre = eclips::system_flow::esNombrePantallaLibre(key);
                        if (!nativa && !libre)
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": pantalla '" << key
                                << "' invalida o reservada. Se aceptan las siete claves nativas"
                                << " o un nombre libre ASCII en minusculas. Ignorada." << std::endl;
                            continue;
                        }

                        std::string value = pm_detail::stripQuotes(valueStr);
                        if (value.empty())
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": pantalla '" << key << "' sin ruta. Ignorada."
                                << std::endl;
                            continue;
                        }

                        if (m.pantallas.count(key) > 0)
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": pantalla '" << key
                                << "' ya estaba definida. Uso la ultima." << std::endl;
                        }
                        m.pantallas[key] = value;
                        continue;
                    }
                }

                // ─── Estado: dentro de bloque transiciones (D-14) ────────────
                // Como `teclas`, esto TRANSPORTA: la ley -qué palabras existen,
                // qué momento corre, y qué se rechaza- vive entera en
                // `TransicionPantalla.h`, que es SDL-free y la custodia
                // `harness_transiciones`. Acá sólo se le pasa el renglón y se
                // traduce su causa a voz.
                if (state == State::InsideTransiciones)
                {
                    if (line == "}")
                    {
                        state = State::TopLevel;
                        continue;
                    }

                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": bloque transiciones (linea " << blockStartLine
                            << ") no se cerro con '}' antes de abrir otro. Abortando bloque."
                            << std::endl;
                        state = State::TopLevel;
                        // Caer al procesado normal de TopLevel (abajo)
                    }
                    else
                    {
                        std::string ofensora;
                        const TransicionCausa causa =
                            transicionAplicarRenglon(line, m.transiciones, ofensora);

                        // La voz nombra la grafía que el CREADOR escribió, no la
                        // canónica a la que el motor la traduce (SYN-4c-ii), y
                        // ENUMERA desde la tabla: si mañana entra `encima`, el
                        // aviso lo dice solo. La otra forma tiene su precio
                        // escrito en EclipScript.h:1093.
                        switch (causa)
                        {
                        case TransicionCausa::Ok:
                            break;
                        case TransicionCausa::ClaveDesconocida:
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": momento '" << ofensora << "' aun no soportado (acepta: "
                                << transicionClavesAceptadas() << "). Ignorado." << std::endl;
                            break;
                        case TransicionCausa::TipoFaltante:
                            // La forma `Escribi '<clave> <lista>'` se probó y se
                            // lee como si hubiera que escribir la lista entera.
                            // El aviso es la interfaz del creador: se dice el
                            // molde y aparte los valores.
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": '" << ofensora << "' sin transicion. Escribi '"
                                << ofensora << " <transicion>', con <transicion> = "
                                << transicionTiposAceptados() << ". Ignorado." << std::endl;
                            break;
                        case TransicionCausa::TipoDesconocido:
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": transicion '" << ofensora << "' aun no soportada (acepta: "
                                << transicionTiposAceptados() << "). Ignorada." << std::endl;
                            break;
                        case TransicionCausa::ParametroDesconocido:
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": parametro '" << ofensora
                                << "' desconocido en transiciones (acepta: duracion/duration)."
                                << " Renglon ignorado." << std::endl;
                            break;
                        case TransicionCausa::DuracionInvalida:
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": '" << ofensora
                                << "' no es una duracion valida (segundos, >= 0)."
                                << " Renglon ignorado." << std::endl;
                            break;
                        }
                        continue;
                    }
                }

                // ─── Estado: dentro de bloque teclas (CP3) ──────────────────
                // Bloque-registro con NOMBRE, como `estilo X {`: el nombre es el
                // dominio. Cada línea es `accion tecla[, tecla...]`, y **nada se
                // valida acá** — esto transporta, la ley vive en
                // `InputManager.h` (ver el comentario de `TeclaFila`).
                if (state == State::InsideTeclas)
                {
                    if (line == "}")
                    {
                        currentTeclasDom.clear();
                        state = State::TopLevel;
                        continue;
                    }

                    if (line.find('{') != std::string::npos)
                    {
                        std::cerr << "[proyecto.eclips] Error linea " << lineNum
                            << ": bloque teclas '" << currentTeclasDom
                            << "' (linea " << blockStartLine
                            << ") no se cerro con '}' antes de abrir otro. Abortando bloque."
                            << std::endl;
                        currentTeclasDom.clear();
                        state = State::TopLevel;
                        // Caer al procesado normal de TopLevel (abajo)
                    }
                    else
                    {
                        std::istringstream ss(line);
                        std::string key;
                        ss >> key;
                        if (key.empty()) continue;
                        // FOLD-PARTIDO: VOCABULARIO -> pliega. El verbo NO lo inventa el
                        // creador: `inputVerboDe` lo resuelve contra la tabla del motor, y
                        // el dominio es vn/juego/dev.
                        // OJO: el dia que lleguen los verbos custom de Lua
                        // (`VERBOS-LIBRES-LUA`) esta ranura pasa a ser MIXTA y hay que
                        // volver a decidirla.
                        key = eclips::vocabToLower(key);

                        std::string valueStr;
                        std::getline(ss, valueStr);
                        valueStr = pm_detail::trim(valueStr);

                        // Rechazo pedagógico de ':' decorativo (coherencia §7.10).
                        //
                        // **Este es el que más falta hace de los cinco**, y no es
                        // una corazonada: la §6 del contrato del átomo dibuja
                        // este bloque CON dos puntos (`guardado_rapido: F2`), así
                        // que quien venga leyendo el diseño los va a escribir.
                        {
                            bool hasColon = false;
                            if (!key.empty() && key.back() == ':') hasColon = true;
                            else if (!valueStr.empty() && valueStr.front() == ':') hasColon = true;
                            if (hasColon)
                            {
                                std::cerr << "[proyecto.eclips] Aviso en '" << path
                                    << "' linea " << lineNum
                                    << " (dentro de teclas '" << currentTeclasDom
                                    << "', accion '" << key << "'):\n"
                                    << "  Los .eclips no usan ':' (eso es solo para .ecl).\n"
                                    << "  Linea ignorada. Quita el ':' y vuelve a correr."
                                    << std::endl;
                                continue;
                            }
                        }

                        if (valueStr.empty())
                        {
                            std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                                << ": la accion '" << key
                                << "' no dice ninguna tecla. Linea ignorada.\n"
                                << "  Si querias dejarla sin tecla, escribi '"
                                << key << " ninguna'." << std::endl;
                            continue;
                        }

                        TeclaFila f;
                        f.dominio = currentTeclasDom;
                        f.verbo   = key;
                        f.teclas  = pm_detail::stripQuotes(valueStr);
                        f.linea   = lineNum;
                        m.teclas.push_back(f);
                        continue;
                    }
                }

                // ─── Estado: TopLevel ───────────────────────────────────────
                // Detectar apertura de bloque personaje
                // Sintaxis: "personaje X {" o "character X {"
                {
                    std::istringstream ss(line);
                    std::string firstWord;
                    ss >> firstWord;
                    if (firstWord == "personaje" || firstWord == "character")
                    {
                        std::string aliasTok, brace;
                        ss >> aliasTok >> brace;
                        if (aliasTok.empty())
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << "' sin alias. Ignorado."
                                << std::endl;
                            continue;
                        }
                        if (brace != "{")
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << " " << aliasTok
                                << "' debe ir seguido de '{'. Ignorado."
                                << std::endl;
                            continue;
                        }
                        current = PersonajeManifest{};
                        current.alias = aliasTok;
                        blockStartLine = lineNum;
                        state = State::InsidePersonaje;
                        continue;
                    }
                }

                // Detectar apertura de bloque estilo: "estilo X {" o "style X {"
                {
                    std::istringstream ss(line);
                    std::string firstWord;
                    ss >> firstWord;
                    if (firstWord == "estilo_control" || firstWord == "control_style")
                    {
                        std::string nombreTok, brace; ss >> nombreTok >> brace;
                        if (nombreTok.empty() || brace != "{")
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << "' debe ser '" << firstWord
                                << " NOMBRE {'. Ignorado." << std::endl;
                            continue;
                        }
                        currentControlEstilo = ControlStyleManifest{};
                        currentControlEstilo.nombre = nombreTok;
                        blockStartLine = lineNum;
                        state = State::InsideControlEstilo;
                        continue;
                    }
                    if (firstWord == "estilo" || firstWord == "style")
                    {
                        std::string nombreTok, brace;
                        ss >> nombreTok >> brace;
                        if (nombreTok.empty())
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << "' sin nombre. Ignorado."
                                << std::endl;
                            continue;
                        }
                        if (brace != "{")
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << " " << nombreTok
                                << "' debe ir seguido de '{'. Ignorado." << std::endl;
                            continue;
                        }
                        currentEstilo = EstiloManifest{};
                        estiloTieneLista = false;
                        estiloTieneBordeLegacy = false;
                        currentEstilo.nombre = nombreTok;
                        blockStartLine = lineNum;
                        state = State::InsideEstilo;
                        continue;
                    }
                }

                // ─── Catálogo de fuentes (ATLAS-DE-FUENTE CP-R3) ────────────
                // `fuente <nombre> "<ruta>" tamano=N [params] [{`
                //
                // A diferencia de `personaje`/`estilo`, la llave es OPCIONAL:
                // la forma simple es una línea entera y el bloque sólo aparece
                // cuando hay respaldos. Por eso no se puede exigir `{` acá.
                //
                // No colisiona con los otros dos dialectos de la palabra: el
                // selector lógico `fuente X` vive DENTRO de `estilo` (y lo caza
                // `InsideEstilo`, que corre antes), y la ruta cruda `fuente "x"`
                // es de `Theme.h`, otro archivo y otro parser. Hasta este CP,
                // `fuente` en TopLevel caía en `key desconocida`.
                {
                    std::istringstream ss(line);
                    std::string firstWord;
                    ss >> firstWord;
                    // PLIEGA, y a diferencia de sus cinco hermanos de arriba
                    // (`personaje`, `estilo`, `imagenes`, `teclas`,
                    // `pantallas`) eso NO es una inconsistencia: es sintaxis
                    // NUEVA, y `FOLD-PARTIDO` manda que el vocabulario del
                    // motor pliegue. Copiarle el defecto a los viejos habría
                    // hecho que `FUENTE` divergiera del `.ecl`, que ya pliega
                    // sus claves. Los otros cinco quedan como están: son la
                    // ficha ajena, no este CP.
                    const std::string firstLow = eclips::vocabToLower(firstWord);
                    if (firstLow == "fuente" || firstLow == "font")
                    {
                        const std::string rest =
                            pm_detail::trim(line.substr(firstWord.size()));

                        fontdecl::Cabecera cab;
                        fontdecl::Error err = fontdecl::partirCabecera(rest, cab);
                        if (err != fontdecl::Error::Ninguno)
                        {
                            vozFuente(lineNum, err);
                            continue;
                        }

                        std::vector<std::string> desconocidas, choques;
                        const auto params = fontdecl::canonizarParams(
                            cab.params, &desconocidas, &choques);
                        for (const auto& k : desconocidas)
                            std::cerr << "[proyecto.eclips] Aviso linea "
                                << lineNum << ": la fuente '" << cab.nombre
                                << "' no conoce el parametro '" << k
                                << "'. Se ignora." << std::endl;
                        for (const auto& k : choques)
                            std::cerr << "[proyecto.eclips] Aviso linea "
                                << lineNum << ": la fuente '" << cab.nombre
                                << "' escribe el mismo parametro de varias "
                                << "maneras. Se usa '" << k << "='."
                                << std::endl;

                        fontdecl::Declaracion d;
                        std::string detalle;
                        err = fontdecl::construir(cab.nombre, cab.ruta,
                                                  params, d, &detalle);
                        if (err != fontdecl::Error::Ninguno)
                        {
                            vozFuente(lineNum, err, detalle);
                            continue;
                        }
                        d.sourceFile = path;
                        d.sourceLine = lineNum;

                        if (cab.abreBloque)
                        {
                            currentFuente = d;
                            blockStartLine = lineNum;
                            state = State::InsideFuente;
                        }
                        else
                        {
                            commitFuente(d, lineNum);
                        }
                        continue;
                    }
                }

                // Detectar apertura de bloque imagenes: "imagenes {" o "images {"
                // (sin nombre — a diferencia de personaje/estilo)
                {
                    std::istringstream ss(line);
                    std::string firstWord;
                    ss >> firstWord;
                    if (firstWord == "imagenes" || firstWord == "images")
                    {
                        std::string brace;
                        ss >> brace;
                        if (brace != "{")
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << "' debe ir seguido de '{'. Ignorado."
                                << std::endl;
                            continue;
                        }
                        blockStartLine = lineNum;
                        state = State::InsideImagenes;
                        continue;
                    }
                }

                // Detectar apertura de bloque teclas (CP3): "teclas vn {" o
                // "keys game {". Lleva NOMBRE como `personaje`/`estilo`, y ese
                // nombre es el dominio.
                //
                // **La §6 del contrato lo dibuja anidado** (`teclas { vn { … } }`)
                // y eso no se puede escribir en un `.eclips`: este parser tiene
                // DOS niveles (TopLevel / adentro), y un `{` adentro de un bloque
                // es un error declarado en los cinco. La forma de acá es la del
                // dialecto vivo, y es la firmada (fork 1a).
                {
                    std::istringstream ss(line);
                    std::string firstWord;
                    ss >> firstWord;
                    if (firstWord == "teclas" || firstWord == "keys")
                    {
                        std::string domTok, brace;
                        ss >> domTok >> brace;
                        if (domTok.empty())
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << "' sin dominio. Escribi '"
                                << firstWord << " vn {' o '" << firstWord
                                << " juego {'. Ignorado." << std::endl;
                            continue;
                        }
                        if (brace != "{")
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << " " << domTok
                                << "' debe ir seguido de '{'. Ignorado." << std::endl;
                            continue;
                        }
                        currentTeclasDom = domTok;
                        blockStartLine = lineNum;
                        state = State::InsideTeclas;
                        continue;
                    }
                }

                // Detectar apertura de bloque pantallas: "pantallas {"
                // (sin nombre, como imagenes). Override de pantalla por verbo.
                {
                    std::istringstream ss(line);
                    std::string firstWord;
                    ss >> firstWord;
                    if (firstWord == "pantallas")
                    {
                        std::string brace;
                        ss >> brace;
                        if (brace != "{")
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": 'pantallas' debe ir seguido de '{'. Ignorado."
                                << std::endl;
                            continue;
                        }
                        blockStartLine = lineNum;
                        state = State::InsidePantallas;
                        continue;
                    }
                }

                // Detectar apertura de bloque transiciones: "transiciones {"
                // (sin nombre, como pantallas). D-14 / DISENO_MENU_RENPY §11.7d.
                //
                // La palabra es PLURAL y eso la separa del comando `transicion`
                // del .ecl, que gobierna el FONDO y está documentado como tal
                // (ECLIPSCRIPT_DOCS: "Efecto visual al cambiar fondo"). Es el
                // mismo concepto sobre otro objeto, y por eso comparten valores
                // (`fade`) en vez de inventar una sexta palabra — el inventario
                // de §11.7a midió CINCO dialectos para "cómo entra algo".
                {
                    std::istringstream ss(line);
                    std::string firstWord;
                    ss >> firstWord;
                    if (firstWord == "transiciones" || firstWord == "transitions")
                    {
                        std::string brace;
                        ss >> brace;
                        if (brace != "{")
                        {
                            std::cerr << "[proyecto.eclips] Error linea " << lineNum
                                << ": '" << firstWord << "' debe ir seguido de '{'. Ignorado."
                                << std::endl;
                            continue;
                        }
                        blockStartLine = lineNum;
                        state = State::InsideTransiciones;
                        continue;
                    }
                }

                // ─── TopLevel — keys planas Tier 2 ──────────────────────────
                std::istringstream ss(line);
                std::string key;
                ss >> key;
                if (key.empty()) continue;
                // FOLD-PARTIDO: VOCABULARIO -> pliega (keys planas Tier 2).
                key = eclips::vocabToLower(key);

                std::string valueStr;
                std::getline(ss, valueStr);
                valueStr = pm_detail::trim(valueStr);

                // Rechazo pedagógico de ':' decorativo
                {
                    bool hasColon = false;
                    if (!key.empty() && key.back() == ':') hasColon = true;
                    else if (!valueStr.empty() && valueStr.front() == ':') hasColon = true;
                    if (hasColon)
                    {
                        std::cerr << "[proyecto.eclips] Aviso en '" << path
                            << "' linea " << lineNum << " (key '" << key << "'):\n"
                            << "  Los .eclips no usan ':' (eso es solo para .ecl).\n"
                            << "  Linea ignorada. Quita el ':' y vuelve a correr.\n"
                            << "  Mas info: ver \"Sobre los ':'\" en ECLIPSCRIPT_DOCS.md"
                            << std::endl;
                        continue;
                    }
                }

                // Llave suelta sin contexto = error
                if (key == "}" || key == "{")
                {
                    std::cerr << "[proyecto.eclips] Error linea " << lineNum
                        << ": '" << key << "' suelto sin bloque. Ignorado."
                        << std::endl;
                    continue;
                }

                std::string value = pm_detail::stripQuotes(valueStr);

                // Tier 2 — metadata
                if (key == "titulo" || key == "title")
                    m.titulo = value;
                else if (key == "autor" || key == "author")
                    m.autor = value;
                else if (key == "version")
                    m.version = value;
                // Tier 2 — arranque y paths
                else if (key == "script_inicial" || key == "initial_script")
                    m.scriptInicial = value;
                else if (key == "script_dir")
                    m.scriptDir = value;
                else if (key == "tema_default" || key == "default_theme")
                    m.temaDefault = value;
                else if (key == "menu")          // misma palabra EN/ES (§3.1)
                    m.menu = value;
                // Tier 2 — catálogo de idiomas (CP1 de DISENO_IDIOMAS)
                else if (key == "idiomas" || key == "languages")
                {
                    m.idiomas.clear();
                    std::istringstream vs(value);
                    std::string codigo;
                    while (vs >> codigo) m.idiomas.push_back(codigo);

                    if (m.idiomas.empty())
                    {
                        std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                            << ": '" << key << "' espera al menos un codigo "
                            << "(ej: " << key << " es en). El sistema de idiomas "
                            << "queda apagado." << std::endl;
                    }
                }
                // Tier 2 — política de la intro del motor (CP3, DISENO_INTRO_ECLIPS)
                else if (key == "intro_motor" || key == "engine_intro")
                {
                    std::string v = value;  // ya sin comillas (stripQuotes arriba)
                    std::transform(v.begin(), v.end(), v.begin(),
                        [](unsigned char c) { return (char)std::tolower(c); });
                    if (v == "siempre" || v == "always")
                        m.introMotor = IntroMotor::Siempre;
                    else if (v == "una_vez" || v == "once")
                        m.introMotor = IntroMotor::UnaVez;
                    else if (v == "no")
                        m.introMotor = IntroMotor::No;
                    else
                        std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                        << ": 'intro_motor' espera siempre|una_vez|no "
                        << "(o always|once|no). Recibido: '" << valueStr
                        << "'. Mantengo el default (siempre)." << std::endl;
                }
                // Tier 2 — resolución de diseño (A1): dos enteros, no string
                else if (key == "resolucion_diseno" || key == "design_resolution")
                {
                    std::istringstream vs(value);
                    int w = 0, h = 0;
                    if ((vs >> w >> h) && w > 0 && h > 0)
                    {
                        m.designW = w;
                        m.designH = h;
                    }
                    else
                    {
                        std::cerr << "[proyecto.eclips] Aviso linea " << lineNum
                            << ": 'resolucion_diseno' espera dos enteros positivos "
                            << "(ej: resolucion_diseno 1280 720). Recibido: '"
                            << valueStr << "'. Mantengo el default ("
                            << m.designW << "x" << m.designH << ")." << std::endl;
                    }
                }
                // Fallback: warning §1.9
                else
                {
                    std::cerr << "[proyecto.eclips] Aviso en '" << path
                        << "' linea " << lineNum << ": key desconocida '" << key
                        << "' (valor: '" << valueStr << "'). Ignorada."
                        << std::endl;
                }
            }

            // ─── EOF: validar que no hay bloque sin cerrar ─────────────
            // CP-R3: un bloque de fuente sin '}' descarta el PERFIL COMPLETO.
            // Conservar la base sola sería peor que descartarlo: el creador
            // pidió una cadena y el motor le daría una fuente sin respaldos,
            // en silencio, con la ruta correcta y la cobertura equivocada.
            if (state == State::InsideFuente)
            {
                std::cerr << "[proyecto.eclips] Error: bloque de la fuente '"
                    << currentFuente.name << "' (abierto en linea "
                    << blockStartLine << ") no fue cerrado con '}'. "
                    << "Perfil descartado." << std::endl;
                m.parseValid = false;
            }
            if (state == State::InsidePersonaje)
            {
                std::cerr << "[proyecto.eclips] Error: bloque personaje '"
                    << current.alias << "' (abierto en linea "
                    << blockStartLine << ") no fue cerrado con '}'. "
                    << "Personaje descartado." << std::endl;
                m.parseValid = false;
            }
            else if (state == State::InsideEstilo)
            {
                std::cerr << "[proyecto.eclips] Error: bloque estilo '"
                    << currentEstilo.nombre << "' (abierto en linea "
                    << blockStartLine << ") no fue cerrado con '}'. "
                    << "Estilo descartado." << std::endl;
                m.parseValid = false;
            }
            else if (state == State::InsideControlEstilo)
            {
                std::cerr << "[proyecto.eclips] Error: bloque estilo_control '"
                    << currentControlEstilo.nombre << "' (abierto en linea "
                    << blockStartLine << ") no fue cerrado con '}'. Estilo descartado." << std::endl;
                m.parseValid = false;
            }

            else if (state == State::InsideImagenes)
            {
                std::cerr << "[proyecto.eclips] Error: bloque imagenes "
                    << "(abierto en linea " << blockStartLine
                    << ") no fue cerrado con '}'." << std::endl;
                m.parseValid = false;
            }

            else if (state == State::InsideTeclas)
            {
                std::cerr << "[proyecto.eclips] Error: bloque teclas '"
                    << currentTeclasDom << "' (abierto en linea "
                    << blockStartLine << ") no fue cerrado con '}'." << std::endl;
                m.parseValid = false;
            }

            else if (state == State::InsidePantallas)
            {
                std::cerr << "[proyecto.eclips] Error: bloque pantallas "
                    << "(abierto en linea " << blockStartLine
                    << ") no fue cerrado con '}'." << std::endl;
                m.parseValid = false;
            }

            m.loaded = true;
            std::cout << "[Proyecto] Cargado: " << path;
            if (!m.titulo.empty())  std::cout << " | titulo: " << m.titulo;
            if (!m.autor.empty())   std::cout << " | autor: " << m.autor;
            if (!m.version.empty()) std::cout << " | version: " << m.version;
            std::cout << std::endl;
            std::cout << "[Proyecto] intro_motor: "
                << (m.introMotor == IntroMotor::Siempre ? "siempre"
                    : m.introMotor == IntroMotor::UnaVez ? "una_vez" : "no")
                << std::endl;

            // Log de personajes cargados
            if (!m.personajes.empty())
            {
                std::cout << "[Proyecto] Personajes (" << m.personajes.size() << "):";
                for (const auto& kv : m.personajes)
                {
                    std::cout << " " << kv.first;
                    if (!kv.second.nombre.empty() && kv.second.nombre != kv.first)
                        std::cout << "(\"" << kv.second.nombre << "\")";
                }
                std::cout << std::endl;
            }

            // Log de estilos cargados
            if (!m.estilos.empty())
            {
                std::cout << "[Proyecto] Estilos (" << m.estilos.size() << "):";
                for (const auto& kv : m.estilos)
                    std::cout << " " << kv.first;
                std::cout << std::endl;
            }

            // Log de imagenes cargadas
            if (!m.imagenes.empty())
            {
                std::cout << "[Proyecto] Imagenes (" << m.imagenes.size() << "):";
                for (const auto& kv : m.imagenes)
                    std::cout << " " << kv.first;
                std::cout << std::endl;
            }

            // Log de pantallas override cargadas
            if (!m.pantallas.empty())
            {
                std::cout << "[Proyecto] Pantallas override (" << m.pantallas.size() << "):";
                for (const auto& kv : m.pantallas)
                    std::cout << " " << kv.first << "->" << kv.second;
                std::cout << std::endl;
            }

            return m;
        }
    };

    // ════════════════════════════════════════════════════════════════════════════
    // introDebeMostrarse — ¿La política F4 pide mostrar la intro del motor? (CP3)
    // ════════════════════════════════════════════════════════════════════════════
    //
    // Query PURA: decide por la política del manifest, NO escribe meta. El caller
    // aplica los efectos (push + marcar la flag de 'una_vez'), para que el guard
    // poder mostrar-sin-marcar. Sin manifest / no-loaded → default No (opt-in):
    // sin proyecto no hay creador que pida la intro. 'yaVistaUnaVez' = el caller
    // ya consultó meta.has(...).
    //
    inline bool introDebeMostrarse(const ProjectManifest* m, bool yaVistaUnaVez)
    {
        const IntroMotor pol = (m && m->loaded) ? m->introMotor : IntroMotor::No;
        if (pol == IntroMotor::No)      return false;
        if (pol == IntroMotor::Siempre) return true;
        return !yaVistaUnaVez;  // UnaVez: solo si no se ha visto
    }

    // ════════════════════════════════════════════════════════════════════════════
    // resolveScriptPath — Resolver path de script consultando el manifest (D6)
    // ════════════════════════════════════════════════════════════════════════════
    //
    // Migrado desde MainMenuState (Pasada 3): la lógica es del motor, no de un
    // estado específico. Ahora consulta scriptDir del manifest cuando existe,
    // y conserva el fallback histórico "vnscript/" para preservar compatibilidad
    // con proyectos sin manifest o sin script_dir definido.
    //
    // Orden de búsqueda:
    //   1. Path tal cual (raíz del proyecto, o ruta explícita con separador
    //      como "vnscript/foo.ecl" o "C:/x/foo.ecl")
    //   2. manifest->scriptDir + path (si manifest existe Y define scriptDir
    //      Y path no contiene separador)
    //   3. "vnscript/" + path (fallback histórico, mismo gating que 2)
    //
    // Retorna "" si no se encontró en ninguna ubicación. El caller decide qué
    // hacer con el fallo (warning, abort, etc.).
    //
    inline std::string resolveScriptPath(const std::string& path,
        const ProjectManifest* manifest = nullptr)
    {
        if (path.empty()) return "";

        // 1) Probar tal cual
        if (std::filesystem::exists(path))
            return path;

        // Solo aplicamos prefijos si el path no es ya una ruta explícita
        // (evita patología "vnscript/vnscript/foo")
        const bool isPlainName = path.find('/') == std::string::npos
            && path.find('\\') == std::string::npos;
        if (!isPlainName) return "";

        // 2) Probar dentro del scriptDir del manifest, si existe
        if (manifest != nullptr && !manifest->scriptDir.empty())
        {
            std::string dir = manifest->scriptDir;
            if (dir.back() != '/' && dir.back() != '\\') dir += '/';
            std::string candidate = dir + path;
            if (std::filesystem::exists(candidate))
                return candidate;
        }

        // 3) Probar dentro de "vnscript/" (fallback histórico)
        std::string fallback = "vnscript/" + path;
        if (std::filesystem::exists(fallback))
            return fallback;

        return "";
    }

} // namespace eclips
