// ============================================================================
// ECLIPS ENGINE - LangPaths (CP4 de DISENO_IDIOMAS.md: los ASSETS ESPEJO)
// ============================================================================
//
// El §6 del contrato: un asset traducido vive en `idiomas/<codigo>/<path>`, y
// el motor lo elige SOLO si hay sistema de idiomas y el idioma activo no es el
// original. Miss -> la ruta original, con los mismos bytes.
//
// ── POR QUE LA LEY VIVE ACA Y NO ADENTRO DE `AssetManager` ─────────────────
//
// `AssetManager.h` incluye `engine/render/Texture.h`, que incluye
// <SDL_opengl.h> (Texture.h:22). O sea que esa casa arrastra GL y NINGUN
// harness de la bateria la puede compilar: es la misma pared que la ficha
// `MERGE-DUPLICADO`. Si la ley viviera ahi, no habria forma de probarla.
//
// Aca es SDL-free -sólo <filesystem>, <string> y LangStore.h-, y su linea del
// `.bat` (fuera del bloque EXTRAINC) es el guardian de que siga siendolo.
//
// ── EL PREDICADO DE EXISTENCIA ENTRA POR PARAMETRO ─────────────────────────
//
// `langResolverRutaCon` toma `existe` como argumento. No es ceremonia: es el
// fork 3b del atomo de teclado otra vez (`inputHeldDeTabla` toma el predicado
// del teclado para que la ley se pruebe sin linkear SDL). Aca compra dos
// cosas: el harness prueba la ley SIN tocar disco, y puede CONTAR los probes
// -que es la unica forma de afirmar la mitad silenciosa del §6, "un juego con
// 3 assets traducidos de 300 no genera 297 warnings".
//
// El cable (`langResolverRuta`) es el que le pasa `fs::exists` y el memo.
//
// ── LAS TRES CONDICIONES DE `HO-20`, QUE ES LO QUE HACE QUE ESTO COMPONGA ──
// CON EL CONFINAMIENTO DE PATHS DE `DISENO_MODELO_CONFIANZA_ECLIPS §5` ──────
//
//   1. SELECCIONA Y DEVUELVE. Nunca decide si un path es legitimo. Frente a
//      una ruta que no puede espejar (absoluta, con `..`, ya bajo `idiomas/`)
//      NO opina: devuelve la original y que la juzgue quien deba.
//   2. SU SALIDA ES RELATIVA Y BAJO LA RAIZ. Nunca absoluta, nunca con `..`,
//      para que la puerta unica de F3 la ENVUELVA sin reescribirla.
//   3. PRIMERO ELEGIR LA VARIANTE, DESPUES CONFINAR. Al reves, un probe de
//      variante podria mirar afuera de la raiz antes de que nadie lo valide.
//      Por eso los casos del punto 1 salen con CERO probes: no se prueba una
//      ruta que no vamos a poder espejar.
//
// ============================================================================

#pragma once

#include "engine/core/LangStore.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace eclips
{
    // El prefijo del arbol espejo. Grafia UNICA, igual que `DI-2` para el
    // paquete `.lang`: lo escribe el motor y el traductor solo lo copia, asi
    // que un par bilingue seria peso muerto.
    inline const char* langCarpetaEspejo() { return "idiomas/"; }

    // Predicado de existencia. El cable pasa `fs::exists`; el harness pasa una
    // tabla, y de paso cuenta.
    using LangExistePred = bool (*)(const std::string&);

    // ── La ruta espejo de `path` para `codigo`, sin preguntarle al disco ────
    //
    // Composicion pura: "idiomas/" + codigo + "/" + path. Separador `/` a
    // proposito -es el que escribe el corpus entero y el que Windows tambien
    // acepta-, para que la salida sea la MISMA cadena en las dos plataformas:
    // el memo y el cache de `AssetManager` estan tecleados por esta cadena.
    inline std::string langRutaEspejo(const std::string& codigo,
                                      const std::string& path)
    {
        return std::string(langCarpetaEspejo()) + codigo + "/" + path;
    }

    // ── Puede esta ruta tener espejo? ───────────────────────────────────────
    //
    // Condicion 1 de `HO-20` en una funcion: esto NO dice si el path es
    // legitimo -eso es de la puerta de F3-, dice si TIENE SENTIDO buscarle una
    // variante. Un `false` aca sale con cero probes (condicion 3).
    inline bool langPuedeEspejar(const std::string& path)
    {
        if (path.empty()) return false;

        // Absoluta: no se puede componer bajo la raiz sin salirse de ella.
        // Las dos formas: POSIX (`/etc/x`) y Windows (`C:/x`, `\\servidor\x`).
        if (path[0] == '/' || path[0] == '\\') return false;
        if (path.size() >= 2 && path[1] == ':') return false;

        // Un segmento `..` en cualquier posicion. Se mira por SEGMENTO y no con
        // un `find("..")` pelado: un archivo que se llame `mapa..png` es legal y
        // no sale de ningun lado.
        for (std::size_t i = 0; i + 1 < path.size(); ++i)
        {
            if (path[i] != '.' || path[i + 1] != '.') continue;
            const bool abreSegmento = (i == 0) || path[i - 1] == '/' || path[i - 1] == '\\';
            const bool cierraSegmento = (i + 2 == path.size())
                                      || path[i + 2] == '/' || path[i + 2] == '\\';
            if (abreSegmento && cierraSegmento) return false;
        }

        // Ya es una ruta espejo: re-espejarla daria `idiomas/en/idiomas/en/...`.
        const std::string pref = langCarpetaEspejo();
        if (path.size() >= pref.size() && path.compare(0, pref.size(), pref) == 0)
            return false;
        const std::string prefWin = "idiomas\\";
        if (path.size() >= prefWin.size() && path.compare(0, prefWin.size(), prefWin) == 0)
            return false;

        return true;
    }

    // ── LA LEY (pura, sin disco, sin memo, sin estado) ──────────────────────
    //
    // Identidad -la MISMA cadena, con los mismos bytes- cuando:
    //   - no hay sistema de idiomas (`store == nullptr`), que es el voto 3;
    //   - el idioma activo ES el original (sus assets ya viven en `path`);
    //   - la ruta no puede espejarse (`langPuedeEspejar`);
    //   - el espejo no existe en disco (voto 2: todo miss cae al original).
    //
    // NO lleva contador de probes, y la primera version SI lo llevaba: el
    // harness cuenta adentro de SU predicado, que es lo unico que mide el
    // `stat` de verdad. Una ley que incrementara su propio contador y se
    // olvidara de preguntar se veria identica desde afuera.
    inline std::string langResolverRutaCon(const LangStore* store,
                                           const std::string& path,
                                           LangExistePred existe)
    {
        if (store == nullptr) return path;          // voto 3: apagado por identidad
        if (existe == nullptr) return path;

        const std::string& codigo = store->code();
        if (codigo.empty()) return path;
        if (codigo == store->original()) return path;  // sus assets YA viven ahi

        // Condicion 3 de HO-20: esto va ANTES del probe. Una ruta que no se
        // puede espejar no se consulta - no se mira afuera de la raiz ni para
        // preguntar.
        if (!langPuedeEspejar(path)) return path;

        const std::string espejo = langRutaEspejo(codigo, path);
        if (existe(espejo)) return espejo;

        return path;   // voto 2: todo miss es el original, nunca un hueco
    }

    // ── El memo: ruta -> ruta resuelta ──────────────────────────────────────
    //
    // Existe para no repetir un `stat` por frame. NO se auto-invalida cuando
    // cambia el idioma, y eso es deliberado: si cambiara de respuesta a mitad
    // de la vida de un cache, un `unpin(rutaGuardada)` dejaria de encontrar lo
    // que su `pin` habia guardado. El fork 3a lo cuelga de `AssetManager::
    // clear()` y `reloadAll()`, que son los dos sitios que ya barren el cache
    // ENTERO y sus pins -asi una generacion del cache vive entera en un solo
    // idioma-. Es, ademas, exactamente la politica que el §3 ya firmaba:
    // "los assets ya pinneados permanecen en el idioma viejo hasta el proximo
    // load natural".
    inline std::unordered_map<std::string, std::string>& langMemoRef()
    {
        static std::unordered_map<std::string, std::string> s_memo;
        return s_memo;
    }

    inline void langMemoLimpiar() { langMemoRef().clear(); }
    inline std::size_t langMemoTamano() { return langMemoRef().size(); }

    // ── La ley + el memo, todavia SIN disco ─────────────────────────────────
    //
    // Existe partida del cable por la misma razon que la ley toma el predicado
    // por parametro: asi el harness puede afirmar que la segunda consulta
    // cuesta CERO probes. Metido adentro del cable, eso solo se podria medir
    // contra un fixture en disco.
    inline std::string langResolverRutaMemo(const LangStore* store,
                                            const std::string& path,
                                            LangExistePred existe)
    {
        // Los apagados NO entran al memo, y no es ahorro de RAM: es que un
        // proyecto sin catalogo tiene que salir de aca sin haber tocado una
        // sola estructura. Byte-identidad incluye no crecer.
        if (store == nullptr || existe == nullptr) return path;
        const std::string& codigo = store->code();
        if (codigo.empty() || codigo == store->original()) return path;
        if (!langPuedeEspejar(path)) return path;

        auto& memo = langMemoRef();
        const auto it = memo.find(path);
        if (it != memo.end()) return it->second;

        // Se memoiza tambien el MISS -la entrada queda apuntando a la ruta
        // original-, porque el caso masivo es justo ese: 3 traducidos de 300.
        // Sin esto, los otros 297 pagarian un `stat` por consulta.
        std::string resuelta = langResolverRutaCon(store, path, existe);
        memo.emplace(path, resuelta);
        return resuelta;
    }

    // ── El cable: la ley + el memo + el disco ───────────────────────────────
    //
    // Lo que llaman las cinco puertas de `AssetManager` y las cuatro de
    // `FontManager`/`AudioManager`. Devuelve por valor porque la cadena puede
    // ser nueva; el caller la usa como clave de su propio cache.
    //
    // Probe SILENCIOSO (trampa A5 del §6): `exists()` directo. NUNCA pasa por
    // el flujo de fallo, NUNCA toca `m_failedPaths`, NUNCA escribe a cerr. Un
    // juego con 3 assets traducidos de 300 no genera 297 avisos.
    inline bool langExisteEnDisco(const std::string& p)
    {
        std::error_code ec;
        return std::filesystem::is_regular_file(std::filesystem::path(p), ec);
    }

    // ── Idioma CANDIDATO del corte tipográfico ────────────────────────────
    // F8 / selector hornean fuentes ANTES de publicar el LangStore vivo.
    // El resolver global mira este puntero si está puesto; si no, el activo.
    inline const LangStore*& langCandidatoRef()
    {
        static const LangStore* s_candidato = nullptr;
        return s_candidato;
    }
    inline void langRegistrarCandidato(const LangStore* store)
    {
        langCandidatoRef() = store;
    }
    inline const LangStore* langCandidato() { return langCandidatoRef(); }

    inline std::unordered_map<std::string, std::string>& langMemoCandidatoRef()
    {
        static std::unordered_map<std::string, std::string> s_memoCandidato;
        return s_memoCandidato;
    }
    inline void langMemoCandidatoLimpiar() { langMemoCandidatoRef().clear(); }

    struct LangCandidatoGuard
    {
        explicit LangCandidatoGuard(const LangStore* store)
            : m_anterior(langCandidatoRef())
            , m_memoAnterior(std::move(langMemoCandidatoRef()))
        {
            langRegistrarCandidato(store);
            langMemoCandidatoLimpiar();
        }
        ~LangCandidatoGuard()
        {
            langRegistrarCandidato(m_anterior);
            langMemoCandidatoRef() = std::move(m_memoAnterior);
        }
        LangCandidatoGuard(const LangCandidatoGuard&) = delete;
        LangCandidatoGuard& operator=(const LangCandidatoGuard&) = delete;

    private:
        const LangStore* m_anterior = nullptr;
        std::unordered_map<std::string, std::string> m_memoAnterior;
    };

    inline std::string langResolverRuta(const std::string& path)
    {
        if (const LangStore* cand = langCandidato())
        {
            // Memo PROPIO del candidato: hornear contra el idioma nuevo no
            // puede ensuciar el memo vivo. Si el bake falla, el memo vivo
            // sigue contestando el idioma anterior.
            if (cand->code().empty() || cand->code() == cand->original())
                return path;
            if (!langPuedeEspejar(path)) return path;
            auto& memo = langMemoCandidatoRef();
            const auto it = memo.find(path);
            if (it != memo.end()) return it->second;
            std::string resuelta =
                langResolverRutaCon(cand, path, &langExisteEnDisco);
            memo.emplace(path, resuelta);
            return resuelta;
        }
        return langResolverRutaMemo(langActivo(), path, &langExisteEnDisco);
    }

    // ── El mapa: la ruta que se ABRE, no la que se recuerda ────────────────
    //
    // `gameplay topdown "testigo_paseo.eclmap"` llega a `loadFromFile` con
    // las barras que `std::filesystem::path::string()` pone en Windows
    // (`vnscript\testigo_paseo.eclmap`). El espejo se compone con `/`.
    // Sin esta normalizacion el probe busca una ruta mezclada y el miss
    // devuelve el original en silencio.
    //
    // Se normaliza SOLO el argumento de la ley. Quien llama sigue guardando
    // la cadena que le llego: el save compara esa cadena y el lua resuelve
    // contra su carpeta. Una puerta ya trae `/`; normalizarla no la cambia.
    // En el idioma original, o si el espejo no esta, el archivo que se abre
    // queda con `/`. En Windows abre el mismo archivo.
    inline std::string langNormalizarSeparadores(const std::string& path)
    {
        std::string n = path;
        for (char& c : n)
            if (c == '\\') c = '/';
        return n;
    }

    inline std::string langRutaMapaCon(const LangStore* store,
                                       const std::string& path,
                                       LangExistePred existe)
    {
        return langResolverRutaCon(store, langNormalizarSeparadores(path), existe);
    }

    inline std::string langRutaMapa(const std::string& path)
    {
        return langResolverRuta(langNormalizarSeparadores(path));
    }

} // namespace eclips
