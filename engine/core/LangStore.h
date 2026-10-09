#pragma once
// ============================================================================
// LangStore.h - EL CATALOGO DE TRADUCCIONES (CP0+CP1 de DISENO_IDIOMAS.md)
// ============================================================================
//
// Header-only y SIN dependencias del motor: no conoce SDL, ni el parser del
// `.ecl`, ni el manifest. Es una tabla `original -> traduccion` y su lector.
// Engine lo enciende desde CP1; quien lo consume para texto visible es CP2.
//
// ----------------------------------------------------------------------------
// LOS TRES VOTOS DEL CONTRATO, EN ESTE ARCHIVO
// ----------------------------------------------------------------------------
//  1. El guion no se toca: la CLAVE es el texto original tal cual esta en el
//     `.ecl`, con sus `${var}` y sus `{tags}` crudos adentro.
//  2. Traducir jamas rompe: toda falla -linea malformada, clave ausente,
//     traduccion vacia, archivo entero ilegible- devuelve EL ORIGINAL. Nunca
//     un hueco, nunca una excepcion. Lo unico que cambia es que se avisa.
//  3. Off por identidad: un store que no cargo nada devuelve su argumento con
//     los MISMOS bytes.
//
// ----------------------------------------------------------------------------
// LA GRAMATICA, Y DE DONDE SALE CADA REGLA
// ----------------------------------------------------------------------------
// Las tres decisiones que definen el formato las firmo el fundador en la mesa
// de tres lecturas (`DISENO_IDIOMAS.md` seccion 12.1). No se cambian aca:
//
//   DI-1   el catalogo se escribe en archivos `.lang`
//   DI-2   la carpeta es `idiomas/`, grafia UNICA (no hay `languages/`)
//   DI-9   un `#` ENTRE COMILLAS es contenido; el valor se toma hasta la
//          ULTIMA comilla de la linea -> las comillas internas sobreviven
//   DI-10  tambien con comillas anidadas; la key `comillas` de project.eclips
//          es capa POSTERIOR de presentacion y no participa de este parser
//   DI-11  NO hay comentario al final de linea: comentario = linea cuyo primer
//          caracter no blanco es `#`
//
// El porque de DI-11 esta medido y no es gusto: sin escapes, *comillas
// internas en el campo* y *comentario al final* son MUTUAMENTE EXCLUYENTES -un
// `"` dentro de la nota se roba el borde del valor-, y de los dos, las
// comillas internas tienen tres lineas de dialogo vivas en el corpus
// (`vnscript/test_color_inline_1.ecl:26/36/47`) y el comentario al final tiene
// cero usuarios, porque este formato todavia no existia. Ademas deja al `.lang`
// mas fiel al dialecto gettext que la seccion 2 del contrato declara: ahi los
// comentarios viven en su propia linea.
//
// ----------------------------------------------------------------------------
// POR QUE ESTO NO LLAMA A `stripInlineComment` (la ley que se descarto)
// ----------------------------------------------------------------------------
// La lectura facil de DI-9 era "reusar B2", el helper de comentario inline del
// motor. B2 alterna el estado de comillas en CADA `"`
// (`engine/core/EclipsParse.h:614-629`) y su propia doctrina llama a la
// divergencia ARISTA CONOCIDA (:584-590): con comillas ANIDADAS decide que el
// `#` quedo afuera y corta. O sea que reusar el helper truncaria justo la linea
// que DI-9 promete conservar. Se reusa la ley SEMANTICA -un `#` dentro del
// campo es contenido-, no el helper. `harness_lang` seccion 7 lo mide contra el
// header real, y por eso ese control negativo no replica nada.
//
// El limite honesto que queda, declarado y AUDIBLE: un original que contenga
// literalmente el separador deja DOS ocurrencias en la linea, y ahi no hay
// forma de saber cual es el borde. La seccion 2 del contrato ya lo declaraba
// invalido; aca ademas HABLA, en vez de partir la clave en silencio.
//
// Sellado en `Resumenes_Sesiones/Harness/harness_lang.cpp`.
// ============================================================================

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eclips
{
    // ── El separador del registro, en un solo lugar ─────────────────────────
    // Comilla-espacio-igual-espacio-comilla. Vive aca y no replicado en el
    // harness (regla 5): si manana cambia, el harness cambia con el.
    inline const char* langSeparador() { return "\" = \""; }

    // ── Que clase de linea es ───────────────────────────────────────────────
    enum class LangLinea
    {
        Vacia,        // solo blancos
        Comentario,   // primer caracter no blanco es '#'  (DI-11)
        Registro,     // "clave" = "valor"
        Malformada    // habla y se ignora (voto 2)
    };

    struct LangRegistro
    {
        LangLinea   tipo  = LangLinea::Malformada;
        std::string clave;
        std::string valor;
        const char* causa = nullptr;   // solo si tipo == Malformada
    };

    // Vista de solo lectura para los consumidores que derivan texto visible.
    // No expone el unordered_map ni le da ownership de repertorio al store.
    struct LangEntrada
    {
        std::string original;
        std::string traduccion;
    };

    // ── La seleccion efectiva de CP1, pura y sin dueños globales ────────────
    // Settings transporta la preferencia cruda antes de que Engine cargue el
    // manifest. Engine junta ambas fuentes con esta ley: catalogo vacio = off;
    // sin override = primer codigo; override valido = ese codigo; override que
    // ya no existe = primer codigo + bandera para que el dueño emita la voz.
    struct LangSeleccion
    {
        bool sistemaActivo = false;
        bool preferenciaInvalida = false;
        std::string original;
        std::string efectivo;
    };

    inline LangSeleccion langSeleccionar(
        const std::vector<std::string>& catalogo,
        const std::string& preferido,
        bool userSet)
    {
        LangSeleccion s;
        if (catalogo.empty()) return s;

        s.sistemaActivo = true;
        s.original = catalogo.front();
        s.efectivo = s.original;

        if (!userSet) return s;

        const auto it = std::find(catalogo.begin(), catalogo.end(), preferido);
        if (it != catalogo.end())
            s.efectivo = *it;
        else
            s.preferenciaInvalida = true;
        return s;
    }

    // ── EL PLAN DEL F8: el catalogo cambia bajo un juego corriendo ───────────
    // (ficha `F8-SIN-IDIOMA`, lote `idiomas-f8`, fork 4c. Custodia:
    //  `harness_lang` seccion 15.)
    //
    // `Engine::reloadManifest` re-aplica teclas, titulo y viewport y NO TOCA EL
    // IDIOMA: el creador edita la key `idiomas`, aprieta F8, y se queda con el
    // catalogo nuevo y el store viejo, en silencio.
    //
    // La ley vive aca -y no en `Engine.cpp`- porque esa TU arrastra SDL y
    // ningun harness la alcanza. Fork 3b del atomo de teclado: ley pura, cable
    // afuera.
    //
    // NO HAY SEGUNDA LEY DE SELECCION: el efectivo y el original salen de
    // `langSeleccionar`, el mismo helper que usa el arranque. Si el F8 eligiera
    // por su cuenta, el mismo `project.eclips` daria idiomas distintos segun se
    // hubiera booteado o recargado, mudo y selectivo.
    //
    // LA ACCION NUNCA DESTRUYE EL OBJETO, y esa es la mitad cara de la ley. El
    // `LangStore*` lo cachean DOS clases de state (`VNScriptState`,
    // `MainMenuState`), inyectado SOLO en boot desde `main.cpp`: un `reset()` o
    // un `make_unique` en el F8 cuelga los punteros de todo state vivo. Por eso
    // `Apagar` viaja SIN codigo -su unico movimiento posible es `load("")`, que
    // vacia el store sin destruirlo- y `Recargar` es `load(codigo)` sobre el
    // objeto que ya existe. El unico caso que construye es `Encender`, y ahi no
    // hay nada que colgar: los states vivos cachearon `nullptr`. Ese es el caso
    // que necesita re-inyectar (fork 4c).
    enum class LangF8Accion
    {
        Nada,       // no habia catalogo y sigue sin haberlo
        Apagar,     // la key desaparecio con el juego corriendo -> load("")
        Encender,   // la key nace donde no habia -> construir + re-inyectar
        Recargar    // el caso comun -> load(efectivo) sobre el objeto vivo
    };

    struct LangF8Plan
    {
        LangF8Accion accion = LangF8Accion::Nada;
        std::string  original;
        std::string  efectivo;
        bool         preferenciaInvalida = false;
    };

    // NO lleva el codigo actual del store, y la ausencia es la ley: con store y
    // catalogo, el F8 recarga SIEMPRE. Es la INVERSA deliberada de la guarda de
    // `CP5` (`VNScriptState.h`, `if (codigo == m_lang->code()) break;`), y las
    // dos son correctas porque el pagador es otro: alla un JUGADOR clickeando
    // la misma caja no debe pagar un `clear()` + N archivos; aca el CREADOR
    // pidio la recarga con una tecla. Y sin recarga incondicional el caso que
    // la ficha nombra literal -agregar `idiomas/fr.lang` SIN tocar
    // `project.eclips`- no se arregla: ahi el catalogo sale identico.
    inline LangF8Plan langPlanF8(const std::vector<std::string>& catalogo,
                                 const std::string& preferido,
                                 bool userSet,
                                 bool hayStore)
    {
        LangF8Plan p;

        const LangSeleccion sel = langSeleccionar(catalogo, preferido, userSet);

        if (!sel.sistemaActivo)
        {
            // `Apagar` viaja SIN codigo, y el duenio de eso NO es este early
            // return: es `langSeleccionar`, que con catalogo vacio devuelve una
            // seleccion vacia. Se midio -mover estas dos asignaciones arriba del
            // return no pone rojo a NADIE- y queda escrito para que nadie lea
            // esta linea como la guarda que no es (§7.518, misma especie). Lo
            // que si es ley: quien complete el codigo aca -"para recordar lo que
            // tenia"- rompe el unico movimiento posible del cable, y ESO cae en
            // `harness_lang` 15.8.
            p.accion = hayStore ? LangF8Accion::Apagar : LangF8Accion::Nada;
            return p;
        }

        p.original            = sel.original;
        p.efectivo            = sel.efectivo;
        p.preferenciaInvalida = sel.preferenciaInvalida;
        p.accion = hayStore ? LangF8Accion::Recargar : LangF8Accion::Encender;
        return p;
    }

    // ── El parser de UNA linea. Puro: no toca disco, no imprime ─────────────
    //
    // Las causas llevan NOMBRE a proposito: un rojo tiene que decir CUAL se
    // rompio, no gritar entero. Misma disciplina que las seis causas del bloque
    // `teclas <dominio> {}` del atomo de teclado.
    inline LangRegistro langParsearLinea(const std::string& linea)
    {
        LangRegistro r;
        const std::string sep = langSeparador();

        std::size_t i = 0;
        while (i < linea.size()
               && (linea[i] == ' ' || linea[i] == '\t' || linea[i] == '\r'))
            ++i;

        if (i >= linea.size()) { r.tipo = LangLinea::Vacia;      return r; }
        if (linea[i] == '#')   { r.tipo = LangLinea::Comentario; return r; }

        if (linea[i] != '"')
        {
            r.causa = "la linea no empieza con una comilla";
            return r;
        }

        const std::size_t sepPos = linea.find(sep, i);
        if (sepPos == std::string::npos)
        {
            r.causa = "falta el separador comilla-espacio-igual-espacio-comilla";
            return r;
        }

        // El limite de la seccion 2, vuelto audible: dos separadores = no hay
        // forma de saber cual es el borde de la clave.
        if (linea.find(sep, sepPos + 1) != std::string::npos)
        {
            r.causa = "el separador aparece mas de una vez: la linea es ambigua";
            return r;
        }

        const std::size_t valIni = sepPos + sep.size();

        // DI-9: hasta la ULTIMA comilla de la linea. Eso es lo que conserva las
        // comillas internas de una traduccion.
        const std::size_t ultima = linea.rfind('"');
        if (ultima == std::string::npos || ultima < valIni)
        {
            r.causa = "falta la comilla de cierre del valor";
            return r;
        }

        // DI-11: despues de esa comilla solo hay blancos. Cualquier otra cosa
        // -incluida una nota con `#`- es registro malformado, no un comentario.
        for (std::size_t k = ultima + 1; k < linea.size(); ++k)
        {
            const char c = linea[k];
            if (c != ' ' && c != '\t' && c != '\r')
            {
                r.causa = "hay texto despues de la comilla final "
                          "(el .lang no tiene comentario al final de linea)";
                return r;
            }
        }

        r.tipo  = LangLinea::Registro;
        r.clave = linea.substr(i + 1, sepPos - (i + 1));
        r.valor = linea.substr(valIni, ultima - valIni);
        return r;
    }

    // ── La tabla ────────────────────────────────────────────────────────────
    class LangStore
    {
    public:
        // Carga el paquete del idioma `codigo` desde `rutaProyecto`:
        //   <ruta>/idiomas/<codigo>.lang    (si existe, PRIMERO)
        //   <ruta>/idiomas/<codigo>/*.lang  (despues, en orden alfabetico)
        // Clave repetida entre archivos -> avisa y gana la ULTIMA.
        // Devuelve false si no habia paquete: NO es un error, es un juego sin
        // traducir a ese idioma (voto 2, todo cae al original).
        // `original` (CP3) es el primer codigo del catalogo: el idioma en el que
        // esta escrito el guion, o sea el idioma de las CLAVES. Se pasa aca y no
        // por un setter para que no dependa del orden de dos llamadas.
        bool load(const std::string& codigo, const std::string& rutaProyecto,
                  const std::string& original = std::string())
        {
            clear();
            m_original = original;
            if (codigo.empty()) return false;
            m_codigo = codigo;

            namespace fs = std::filesystem;
            std::error_code ec;
            const fs::path base = fs::path(rutaProyecto) / "idiomas";

            std::vector<fs::path> archivos;

            const fs::path unico = base / (codigo + ".lang");
            if (fs::is_regular_file(unico, ec)) archivos.push_back(unico);

            const fs::path carpeta = base / codigo;
            if (fs::is_directory(carpeta, ec))
            {
                std::vector<fs::path> sueltos;
                for (const fs::directory_entry& e :
                     fs::directory_iterator(carpeta, ec))
                {
                    if (!fs::is_regular_file(e.path(), ec)) continue;
                    if (e.path().extension() != ".lang")    continue;
                    sueltos.push_back(e.path());
                }
                // Orden DETERMINISTA: dos maquinas tienen que resolver la misma
                // colision de la misma forma, o "ultima gana" no significa nada.
                std::sort(sueltos.begin(), sueltos.end(),
                          [](const fs::path& a, const fs::path& b)
                          { return a.filename().string() < b.filename().string(); });
                for (const fs::path& p : sueltos) archivos.push_back(p);
            }

            for (const fs::path& p : archivos) cargarArchivo(p);

            m_noVacias = 0;
            for (const auto& kv : m_tabla)
                if (!kv.second.empty()) ++m_noVacias;

            if (!archivos.empty()) latido(codigo);
            return !archivos.empty();
        }

        // Voto 2: hit no vacio -> la traduccion. Miss, vacia o ausente -> el
        // crudo, con los mismos bytes.
        std::string translate(const std::string& crudo) const
        {
            const auto it = m_tabla.find(crudo);
            if (it == m_tabla.end() || it->second.empty()) return crudo;
            return it->second;
        }

        // Snapshot determinista del paquete ACTIVO. Incluye traducciones
        // vacias porque `translate` cae al original y ese original si alcanza
        // una superficie dibujable. Ordena bytes como unsigned para que una
        // maquina con `char` signed y otra con `char` unsigned no discrepen.
        std::vector<LangEntrada> entradasOrdenadas() const
        {
            std::vector<LangEntrada> out;
            out.reserve(m_tabla.size());
            for (const auto& pair : m_tabla)
                out.push_back(LangEntrada{ pair.first, pair.second });

            std::sort(out.begin(), out.end(),
                [](const LangEntrada& a, const LangEntrada& b)
                {
                    return std::lexicographical_compare(
                        a.original.begin(), a.original.end(),
                        b.original.begin(), b.original.end(),
                        [](char left, char right)
                        {
                            return static_cast<unsigned char>(left) <
                                   static_cast<unsigned char>(right);
                        });
                });
            return out;
        }

        // count() son las traducciones EFECTIVAS (no vacias), no los registros.
        // La diferencia no es cosmetica: la seccion 9.3 del contrato promete un
        // extractor que genera una plantilla semilla con TODAS las derechas
        // vacias. Si esto contara registros, esa plantilla anunciaria cobertura
        // completa traduciendo cero.
        std::size_t count() const { return m_noVacias; }
        std::size_t entries() const { return m_tabla.size(); }
        std::size_t files() const { return m_archivos; }
        const std::string& code() const { return m_codigo; }

        // ── CP3: el idioma ORIGINAL del proyecto ────────────────────────────
        //
        // Hasta CP2 este objeto conocia UN solo codigo: el efectivo. El
        // original existia un instante dentro de `LangSeleccion` en
        // `Engine::init` y se perdia con el scope. La regla 2 de la seccion 4
        // del contrato lo necesita vivo -"si el idioma activo no tiene tabla,
        // la base sale de la tabla del ORIGINAL"-, y darle una segunda casa es
        // como se desincronizan: el store ya es el dueño del idioma, asi que
        // se guarda aca.
        //
        // Vacio = no se declaro. Un store apagado responde vacio a los dos.
        //
        // NO lleva setter: entra por el tercer parametro de `load`, que corre
        // despues del `clear()`. Un setter aparte haria que el ORDEN de dos
        // llamadas decidiera si el original sobrevive - la clase de trampa que
        // no se ve en una linea de diff.
        const std::string& original() const { return m_original; }

        void clear()
        {
            m_tabla.clear();
            m_codigo.clear();
            m_original.clear();
            m_noVacias = 0;
            m_archivos = 0;
        }

    private:
        void latido(const std::string& codigo) const
        {
            std::cout << "[Eclips] " << codigo << ": " << m_noVacias << " de "
                      << m_tabla.size() << " traducciones ("
                      << m_archivos << (m_archivos == 1 ? " archivo)" : " archivos)")
                      << std::endl;
        }

        bool cargarArchivo(const std::filesystem::path& p)
        {
            std::ifstream f(p, std::ios::binary);
            if (!f)
            {
                std::cerr << "[idiomas] No se pudo abrir '" << p.string()
                          << "'. Se ignora." << std::endl;
                return false;
            }
            ++m_archivos;

            const std::string nombre = p.filename().string();
            std::string linea;
            std::size_t nro = 0;
            bool primera = true;

            while (std::getline(f, linea))
            {
                ++nro;
                if (primera)
                {
                    primera = false;
                    // BOM pelado, y solo en la primera linea: en cualquier otra
                    // esos tres bytes son texto del traductor.
                    if (linea.size() >= 3
                        && static_cast<unsigned char>(linea[0]) == 0xEF
                        && static_cast<unsigned char>(linea[1]) == 0xBB
                        && static_cast<unsigned char>(linea[2]) == 0xBF)
                        linea.erase(0, 3);
                }
                if (!linea.empty() && linea.back() == '\r') linea.pop_back();

                const LangRegistro r = langParsearLinea(linea);

                if (r.tipo == LangLinea::Vacia || r.tipo == LangLinea::Comentario)
                    continue;

                if (r.tipo == LangLinea::Malformada)
                {
                    // Anti-silencio: archivo:linea + la causa, y la carga sigue.
                    // Una traduccion al 60% es un juego 100% jugable.
                    std::cerr << "[idiomas] " << nombre << ":" << nro << ": "
                              << (r.causa ? r.causa : "linea invalida")
                              << ". Se ignora." << std::endl;
                    continue;
                }

                const auto it = m_tabla.find(r.clave);
                if (it != m_tabla.end())
                {
                    std::cerr << "[idiomas] " << nombre << ":" << nro
                              << ": la clave ya estaba definida. Gana la ultima."
                              << std::endl;
                    it->second = r.valor;
                }
                else
                {
                    m_tabla.emplace(r.clave, r.valor);
                }
            }
            return true;
        }

        std::unordered_map<std::string, std::string> m_tabla;
        std::string m_codigo;
        std::string m_original;
        std::size_t m_noVacias = 0;
        std::size_t m_archivos = 0;
    };

    // ========================================================================
    // CP3 - EL MOTOR ES BILINGUE NATIVO, Y EL `.lang` CUBRE AL RESTO
    // ========================================================================
    //
    // La seccion 5 del contrato firma UN solo mecanismo para los strings del
    // motor: `tr(es, en)`. Idioma activo `es`/`en` -> tabla interna; cualquier
    // otro -> el español de fabrica pasa por el `.lang` del activo. O sea que
    // **la clave de motor ES el español**, igual que la clave del guion es el
    // texto original: un juego japones escribe `"Reanudar" = "再開"` en su
    // `ja.lang` y no hay un segundo formato que aprender.
    //
    // Sin sistema de idiomas (`store == nullptr`) devuelve `es` -> byte
    // identico a hoy, que es el voto 3.
    //
    // `en == nullptr` significa "es la MISMA palabra en los dos idiomas" y cae
    // a `es`. No es un caso raro: es la convencion que `InputKeyName` ya usa
    // para `TAB`, `F1`-`F12` y la enie (`InputManager.h:286`), o sea 14 de los
    // 40 nombres canonicos de tecla. Reusarla evita inventar un segundo
    // dialecto para decir lo mismo.
    inline std::string langTr(const LangStore* store,
                              const char* es,
                              const char* en)
    {
        const std::string sEs(es ? es : "");
        if (!store) return sEs;

        const std::string& codigo = store->code();
        if (codigo.empty() || codigo == "es") return sEs;
        if (codigo == "en") return en ? std::string(en) : sEs;

        // Cualquier otro idioma: la clave de motor ES el español de fabrica.
        return store->translate(sEs);
    }

    // ── El store ACTIVO, registrado por Engine (fork 9b) ────────────────────
    //
    // Misma especie que `Settings::textSpeed()`: config global de solo lectura
    // que cualquier pantalla puede consultar. `Engine` es el dueño (lo tiene en
    // un `unique_ptr` que vive todo el programa) y aca solo queda una vista.
    //
    // Lo que esto PIERDE, escrito para que nadie lo redescubra: el compilador
    // deja de obligar a un state a declarar si le importa el idioma. La perdida
    // es menor de lo que suena y se midio: `CP1` inyecto el puntero por
    // constructor y NO cazo a las cinco pantallas de chrome que llevaban desde
    // entonces con texto español horneado -las encontro el censo, no el
    // constructor-, porque un state que no DECLARA el parametro compila igual.
    // La red que reemplaza a esa es el guardian de cable de `harness_lang` 12,
    // que enumera los archivos del chrome y exige que sus literales viajen
    // dentro de `langUI`.
    //
    // Y hay un lado en el que esto es MEJOR que el puntero copiado: el dia que
    // `CP5` recargue el idioma con F8, re-apuntar aca es una linea; cinco copias
    // en cinco constructores habria que ir a buscarlas.
    inline const LangStore*& langActivoRef()
    {
        static const LangStore* s_activo = nullptr;
        return s_activo;
    }

    inline void langRegistrarActivo(const LangStore* store)
    {
        langActivoRef() = store;
    }

    // nullptr = sin sistema de idiomas. Es el MISMO apagado por identidad del
    // voto 3, no un caso nuevo.
    inline const LangStore* langActivo() { return langActivoRef(); }

    // Lo que llama el chrome. Tres lineas, y la ley sigue viviendo en `langTr`
    // -con el store por parametro- para que el harness la pruebe sin tocar
    // estado global.
    inline std::string langUI(const char* es, const char* en)
    {
        return langTr(langActivo(), es, en);
    }

    // ── El hueco de una plantilla del chrome (fork 6b) ──────────────────────
    //
    // Tokens NOMBRADOS, misma ley que `ranura_fecha_formato`, y de GRAFIA
    // UNICA: el par bilingüe existe para que el creador elija la palabra mas
    // comoda linea por linea (§7.445), y aca NO HAY LINEA DONDE ELEGIR -estos
    // tokens los escribe el motor y el traductor solo los conserva-. Una grafia
    // inglesa seria peso muerto; mismo argumento que DI-2 para `idiomas/`.
    //
    // UNA sola pasada, y eso no es optimizacion: es la ley. Sustituir en
    // pasadas sucesivas haria que un VALOR que contenga `{hp}` -por ejemplo el
    // nombre de escena del creador, que entra por `{escena}`- se volviera a
    // sustituir en la pasada siguiente. El creador escribe esos nombres; el
    // motor no puede tratarlos como plantilla.
    //
    // Un token que la plantilla nombra y el caller no pasa queda LITERAL, igual
    // que en `formatTimestamp`: visible, no roto.
    //
    // Misma FORMA que el dispatch de `formatTimestamp`, y a proposito: son la
    // misma ley -`{token}` nombrado, desconocido queda literal- en dos
    // dialectos. Si un dia divergen, el creador tiene que aprender dos.
    inline std::string langPoner(
        const std::string& plantilla,
        std::initializer_list<std::pair<const char*, std::string>> pares)
    {
        std::string out;
        out.reserve(plantilla.size());

        for (std::size_t i = 0; i < plantilla.size(); )
        {
            if (plantilla[i] == '{')
            {
                const std::size_t fin = plantilla.find('}', i + 1);
                if (fin != std::string::npos)
                {
                    const std::string tk = plantilla.substr(i + 1, fin - i - 1);

                    const std::pair<const char*, std::string>* hit = nullptr;
                    for (const auto& p : pares)
                        if (p.first && tk == p.first) { hit = &p; break; }

                    // El valor entra como TEXTO y no se vuelve a mirar: `out`
                    // ya no se recorre. Eso es lo que hace segura una escena
                    // que el creador haya llamado `{hp}`.
                    if (hit) out += hit->second;
                    else   { out += '{'; out += tk; out += '}'; }

                    i = fin + 1;
                    continue;
                }
            }
            out += plantilla[i++];
        }
        return out;
    }

    // ── El ENDONIMO: como se llama un idioma EN SU PROPIO IDIOMA ────────────
    // (DISENO_IDIOMAS §16, fork `2a` — firmado SIN la fila `ja`.)
    //
    // Lo consume el selector de `CP5`: el jugador que no entiende la pantalla
    // en la que esta parado tiene que poder reconocer la suya. No es
    // traduccion, es un LOOKUP, y por eso vive aca y no en los `.lang`: pedirlo
    // desde los paquetes obligaria a abrir los N para dibujar UNA pantalla
    // (fork `2c`, fichado con gatillo — su dia llega con el primer idioma cuyo
    // endonimo el atlas no dibuje).
    //
    // BYTES EXPLICITOS por §1.60: los fuentes son UTF-8 sin BOM y MSVC los lee
    // como ANSI, asi que un `"Español"` literal sale doble-encodeado. Y la fila
    // `fr` va PARTIDA en literales adyacentes por una razon que se midio, no se
    // supuso: `"Fran\xC3\xA7ais"` NO COMPILA — el `\x` de C++ consume todos los
    // digitos hex que puede y la `a` de `ais` es uno, asi que MSVC lee `\xA7a`
    // y tira `error C7744: secuencia de escape fuera del intervalo`. Las otras
    // dos con byte alto compilan pegadas porque la letra que sigue no es hex
    // (`\xB1o`, `\xAAs`). O sea que esta trampa la caza el compilador, no un
    // rojo mudo — pero se deja escrito para que nadie la "arregle" juntandola.
    //
    // POR QUE NO ESTA `ja`, que el §10 del contrato si pedia: el atlas hornea
    // ASCII 32-126 + 160-383 + seis signos (`TextRenderer.cpp:79-87`), asi que
    // `日本語` saldria como cuadros vacios. SIN la fila, el fallback muestra
    // `ja` — que se dibuja y no miente. La tabla, en ese caso, seria PEOR que
    // no tenerla. Lo custodia `harness_lang` §14.7/14.8.
    //
    // Desconocido -> el codigo tal cual: el selector nunca queda con un hueco,
    // y un catalogo con un idioma que el motor no conoce se lista igual. El
    // match es BYTE-EXACTO, sin fold, como todo el resto del dialecto del
    // idioma (`langSeleccionar` usa `std::find`, `LangStore` guarda el codigo
    // como vino): si `ES` mostrara `Español`, el store igual buscaria en
    // `idiomas/ES/`.
    struct LangEndonimoFila { const char* codigo; const char* nombre; };

    // La tabla se EXPONE, y no por prolijidad: encerrada adentro de
    // `langEndonimo` no se puede enumerar, y un guardian que no la enumera
    // barre una lista escrita a mano que no ve las filas nuevas. Se midio:
    // con la tabla privada, agregar `ja` dejaba VERDE al assert que existe
    // justamente para cazar eso.
    inline const std::vector<LangEndonimoFila>& langEndonimosDeFabrica()
    {
        static const std::vector<LangEndonimoFila> kTabla = {
            { "es", "Espa\xC3\xB1ol" },
            { "en", "English"        },
            { "fr", "Fran" "\xC3\xA7" "ais" },
            { "pt", "Portugu" "\xC3\xAA" "s" },
            { "de", "Deutsch"        },
            { "it", "Italiano"       },
        };
        return kTabla;
    }

    inline std::string langEndonimo(const std::string& codigo)
    {
        for (const LangEndonimoFila& f : langEndonimosDeFabrica())
            if (codigo == f.codigo) return f.nombre;
        return codigo;
    }

    // ── Las dos tablas de fabrica (seccion 4) ───────────────────────────────
    //
    // 12 meses + 7 dias, `es` + `en`, const estaticas y cero alocacion. NO son
    // la ley: son el alfabeto. La ley es la regla de resolucion de abajo, y es
    // la que nace inerte.
    //
    // Minusculas a proposito: es la grafia española ("6 de julio"). Un tema que
    // quiera "Julio" lo capitaliza en su plantilla, y un `en.lang` que quiera
    // "July" lo escribe asi. No se hornea una politica de mayusculas que
    // despues haya que deshacer por idioma.
    inline const char* const* langMeses(const std::string& codigo)
    {
        static const char* const kEs[12] = {
            "enero", "febrero", "marzo", "abril", "mayo", "junio",
            "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"
        };
        static const char* const kEn[12] = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };
        if (codigo == "es") return kEs;
        if (codigo == "en") return kEn;
        return nullptr;
    }

    // Indice 0 = domingo, para entrar directo con `tm_wday` sin aritmetica en
    // el caller. La aritmetica en el caller es como se cuela un off-by-one que
    // solo se ve un dia de cada siete.
    inline const char* const* langDias(const std::string& codigo)
    {
        static const char* const kEs[7] = {
            "domingo", "lunes", "martes", "miercoles",
            "jueves", "viernes", "sabado"
        };
        static const char* const kEn[7] = {
            "Sunday", "Monday", "Tuesday", "Wednesday",
            "Thursday", "Friday", "Saturday"
        };
        if (codigo == "es") return kEs;
        if (codigo == "en") return kEn;
        return nullptr;
    }

    inline bool langTieneTabla(const std::string& codigo)
    {
        return langMeses(codigo) != nullptr;
    }

    // ── El contexto de fecha: TRES codigos, no uno ──────────────────────────
    //
    // La seccion 4 se contradice a si misma y esto es lo que resuelve la
    // contradiccion. Su bullet de FIRMA dice que `formatTimestamp` "recibe el
    // codigo efectivo como parametro nuevo" -uno-; su bullet de RESOLUCION
    // necesita tres cosas: el codigo que manda los nombres (el knob ya
    // resuelto), el ORIGINAL (para la base cuando el activo no tiene tabla) y
    // el `.lang` del activo (para traducir esa base). Gana la regla, porque la
    // regla es la conducta.
    //
    //   `nombres` vacio  -> los nombres siguen al idioma del store (knob
    //                       `auto`, que es el default).
    //   `nombres` con codigo -> el knob fijo del tema: "siempre ingles aunque
    //                       cambies de idioma" (el look Ren'Py). Solo afecta
    //                       NOMBRES; los numeros son universales.
    //   `store` nullptr  -> sin sistema de idiomas. El knob sigue funcionando
    //                       (proporcionalidad fina de la seccion 4) y sin knob
    //                       la base es `es`.
    struct LangFecha
    {
        const LangStore* store = nullptr;
        std::string      nombres;
    };

    // El codigo que manda los nombres, con el knob ya aplicado.
    inline std::string langCodigoDeNombres(const LangFecha& ctx)
    {
        if (!ctx.nombres.empty()) return ctx.nombres;
        return ctx.store ? ctx.store->code() : std::string();
    }

    // ── La regla de resolucion de la seccion 4 ──────────────────────────────
    //
    //   1. El idioma de nombres tiene tabla motor (`es`/`en`) -> directo, y NO
    //      pasa por el `.lang`: la tabla de fabrica gana.
    //   2. Si no: la base sale de la tabla del ORIGINAL si el original tiene
    //      tabla; si tampoco, base `es` (fabrica). Esa base pasa por `t()`, o
    //      sea que el `.lang` del activo la cubre (`"junio" = "juin"`).
    //      Coherente con la seccion 2: las claves siempre en idioma original.
    //
    // La regla vive UNA vez y las dos familias la llaman. Escribirla dos veces
    // -una para meses, otra para dias- es como se desincronizan: el segundo
    // caller hereda la version vieja y nada se pone rojo, porque los dos
    // resultados siguen siendo strings plausibles.
    namespace detail
    {
        inline std::string langNombreDeTabla(
            const LangFecha& ctx,
            int indice,
            int cardinal,
            const char* const* (*tablaDe)(const std::string&))
        {
            if (indice < 0 || indice >= cardinal) return std::string();

            // Regla 1: el idioma de los nombres tiene tabla motor -> directo.
            // NO pasa por el `.lang`: la tabla de fabrica gana, y por eso un
            // `en.lang` que traduzca "julio" no puede secuestrar la ranura.
            const std::string nombres = langCodigoDeNombres(ctx);
            if (const char* const* propia = tablaDe(nombres))
                return std::string(propia[indice]);

            // Regla 2: la base sale del ORIGINAL si el original tiene tabla; si
            // tampoco, `es` de fabrica.
            const std::string orig =
                ctx.store ? ctx.store->original() : std::string();
            const char* const* base = tablaDe(orig);
            if (!base) base = tablaDe("es");

            const std::string nombreBase(base[indice]);

            // ...y esa base pasa por el `.lang` del activo. Miss -> el original,
            // nunca un hueco (voto 2).
            return ctx.store ? ctx.store->translate(nombreBase) : nombreBase;
        }
    }

    inline std::string langNombreMes(int mes0, const LangFecha& ctx)
    {
        return detail::langNombreDeTabla(ctx, mes0, 12, &langMeses);
    }

    inline std::string langNombreDia(int diaSemana0, const LangFecha& ctx)
    {
        return detail::langNombreDeTabla(ctx, diaSemana0, 7, &langDias);
    }

} // namespace eclips
