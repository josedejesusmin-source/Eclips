// ============================================================================
// ECLIPS ENGINE - FontRutaEspejo (ESPEJO-INUTILIZABLE-HORNEA-VACIO)
// ============================================================================
//
// Fork firmado 2026-09-05: una fuente del arbol espejo que EXISTE pero NO ABRE
// se trata como miss de ruta y cae al original. La ausencia de codepoint sigue
// en OMITIR + AVISAR (DA-21); esto NO la toca.
//
// VA EN engine/render/ Y NO EN core/: necesita stb_truetype.h (misma casa que
// FontSourceStore.h). LangPaths.h permanece SDL-free y stb-free.
//
// Compone con la ley pura: langResolverRutaCon toma el predicado por parametro.
// Memo PROPIO — no reutiliza los memos genericos del cable flojo.
// ============================================================================

#pragma once

#include "../core/LangPaths.h"
#include "../../third_party/stb_truetype.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace eclips
{
    // Existe Y abre: misma definicion de «usable» que prepareSource
    // (FontSourceStore.h ~prepareSource / stbtt_InitFont + GetFontOffsetForIndex).
    // SILENCIOSO: nunca escribe a cerr (disciplina de langExisteEnDisco).
    inline bool fontRutaUsableEnDisco(const std::string& ruta)
    {
        std::ifstream file(ruta, std::ios::binary | std::ios::ate);
        if (!file) return false;
        const std::streamsize length = file.tellg();
        if (length <= 0) return false;
        file.seekg(0);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), length))
            return false;

        const int offset = stbtt_GetFontOffsetForIndex(bytes.data(), 0);
        if (offset < 0) return false;
        stbtt_fontinfo font{};
        if (!stbtt_InitFont(&font, bytes.data(), offset))
            return false;
        return true;
    }

    // Memo PROPIO del vivo (separado del generico).
    inline std::unordered_map<std::string, std::string>& fontMemoEspejoRef()
    {
        static std::unordered_map<std::string, std::string> s_memo;
        return s_memo;
    }

    // Memo PROPIO del candidato (separado del generico).
    inline std::unordered_map<std::string, std::string>& fontMemoEspejoCandidatoRef()
    {
        static std::unordered_map<std::string, std::string> s_memoCandidato;
        return s_memoCandidato;
    }

    // Ciclo de vida: heredan la obligacion de los memos genericos.
    // Candidato muere donde vive LangCandidatoGuard (FontGeneration);
    // vivo, donde el idioma activo cambia (Engine F8 / selector).
    inline void fontMemoEspejoLimpiar() { fontMemoEspejoRef().clear(); }
    inline void fontMemoEspejoCandidatoLimpiar() { fontMemoEspejoCandidatoRef().clear(); }

    // Las DOS ramas de langResolverRuta (LangPaths.h ~L258): candidato Y vivo.
    // Predicado estricto: fontRutaUsableEnDisco.
    inline std::string fontResolverRutaEspejo(const std::string& path)
    {
        if (const LangStore* cand = langCandidato())
        {
            if (cand->code().empty() || cand->code() == cand->original())
                return path;
            if (!langPuedeEspejar(path)) return path;
            auto& memo = fontMemoEspejoCandidatoRef();
            const auto it = memo.find(path);
            if (it != memo.end()) return it->second;
            std::string resuelta =
                langResolverRutaCon(cand, path, &fontRutaUsableEnDisco);
            memo.emplace(path, resuelta);
            return resuelta;
        }

        // Rama viva — espejo de langResolverRutaMemo con memo y predicado propios.
        const LangStore* store = langActivo();
        if (store == nullptr) return path;
        const std::string& codigo = store->code();
        if (codigo.empty() || codigo == store->original()) return path;
        if (!langPuedeEspejar(path)) return path;

        auto& memo = fontMemoEspejoRef();
        const auto it = memo.find(path);
        if (it != memo.end()) return it->second;

        std::string resuelta =
            langResolverRutaCon(store, path, &fontRutaUsableEnDisco);
        memo.emplace(path, resuelta);
        return resuelta;
    }

} // namespace eclips
