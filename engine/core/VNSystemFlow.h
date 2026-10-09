// ============================================================================
// VNSystemFlow.h - nucleo SDL-free de llamadas a pantallas de sistema FLF-01
// ============================================================================
#pragma once

#include <cctype>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace eclips::system_flow
{
    enum class PantallaRetorno
    {
        Reponer,
        Continuar
    };

    enum class PantallaCara
    {
        Enlace,
        EsperarClick,
        Eleccion,
        Palabras
    };

    enum class ParseDestinoCausa
    {
        Ninguna,
        DestinoVacio,
        ClaveDesconocida,
        ValorDesconocido,
        ValorVacio,
        ClaveDuplicada
    };

    struct DestinoPantalla
    {
        std::string nombre;
        PantallaRetorno politica = PantallaRetorno::Reponer;
        bool politicaExplicita = false;
    };

    struct ParseDestinoResultado
    {
        bool ok = false;
        ParseDestinoCausa causa = ParseDestinoCausa::Ninguna;
        std::string detalle;
        DestinoPantalla destino;
    };

    // Es una gramatica de una sola cola: el primer token es el nombre y, si
    // existe, queda exactamente una politica key=value. No hay un segundo
    // destino escondido ni fall-through silencioso.
    inline ParseDestinoResultado parseDestinoPantalla(const std::string& raw)
    {
        ParseDestinoResultado result;
        std::istringstream in(raw);
        std::vector<std::string> tokens;
        std::string token;
        while (in >> token) tokens.push_back(token);

        if (tokens.empty())
        {
            result.causa = ParseDestinoCausa::DestinoVacio;
            result.detalle = "destino vacio";
            return result;
        }

        result.destino.nombre = tokens.front();
        if (tokens.size() == 1)
        {
            result.ok = true;
            return result;
        }

        bool sawPolicy = false;
        for (size_t i = 1; i < tokens.size(); ++i)
        {
            const std::string& item = tokens[i];
            const size_t equal = item.find('=');
            if (equal == std::string::npos)
            {
                result.causa = ParseDestinoCausa::ClaveDesconocida;
                result.detalle = "clave de politica desconocida '" + item + "'";
                return result;
            }

            const std::string key = item.substr(0, equal);
            const std::string value = item.substr(equal + 1);
            if (key != "al_volver" && key != "on_return")
            {
                result.causa = ParseDestinoCausa::ClaveDesconocida;
                result.detalle = "clave de politica desconocida '" + key + "'";
                return result;
            }
            if (sawPolicy)
            {
                result.causa = ParseDestinoCausa::ClaveDuplicada;
                result.detalle = "politica de regreso duplicada";
                return result;
            }
            sawPolicy = true;
            if (value.empty())
            {
                result.causa = ParseDestinoCausa::ValorVacio;
                result.detalle = "valor de politica vacio";
                return result;
            }
            if (value == "reponer" || value == "restore")
                result.destino.politica = PantallaRetorno::Reponer;
            else if (value == "continuar" || value == "continue")
                result.destino.politica = PantallaRetorno::Continuar;
            else
            {
                result.causa = ParseDestinoCausa::ValorDesconocido;
                result.detalle = "valor de politica desconocido '" + value + "'";
                return result;
            }
        }

        result.destino.politicaExplicita = true;
        result.ok = true;
        return result;
    }

    inline bool esIdentificadorPantalla(const std::string& nombre)
    {
        if (nombre.empty()) return false;
        if (nombre.front() < 'a' || nombre.front() > 'z') return false;
        for (size_t i = 1; i < nombre.size(); ++i)
        {
            const char c = nombre[i];
            if (!((c >= 'a' && c <= 'z')
                  || (c >= '0' && c <= '9') || c == '_')) return false;
        }
        return true;
    }

    inline bool esClavePantallaNativa(const std::string& nombre)
    {
        return nombre == "config" || nombre == "cargar" || nombre == "load"
            || nombre == "guardar" || nombre == "save" || nombre == "pausa"
            || nombre == "pause";
    }

    inline bool esVerboDestinoReservado(const std::string& nombre)
    {
        return nombre == "config" || nombre == "settings"
            || nombre == "cargar" || nombre == "load"
            || nombre == "guardar" || nombre == "save"
            || nombre == "pausa" || nombre == "pause"
            || nombre == "menu" || nombre == "historial" || nombre == "history"
            || nombre == "salir" || nombre == "quit"
            || nombre == "nuevo_juego" || nombre == "new_game"
            || nombre == "continuar" || nombre == "continue"
            || nombre == "titulo" || nombre == "title"
            || nombre == "fin" || nombre == "end";
    }

    inline bool esNombrePantallaLibre(const std::string& nombre)
    {
        return esIdentificadorPantalla(nombre)
            && !esVerboDestinoReservado(nombre);
    }

    struct TicketRetorno
    {
        PantallaRetorno politica = PantallaRetorno::Reponer;
        PantallaCara cara = PantallaCara::Enlace;
        bool cierreNormalSolicitado = false;
        bool consumido = false;
    };

    using TicketRetornoPtr = std::shared_ptr<TicketRetorno>;

    enum class RegresoEfecto
    {
        NoOp,
        Enlace,
        EsperarClick,
        Eleccion,
        Palabras
    };

    // El estado que vuelve es el único dueño de este consumo. Un ticket sin
    // cierre normal (o ya consumido) jamás inventa un avance.
    inline RegresoEfecto consumirRetorno(TicketRetorno& ticket)
    {
        if (ticket.consumido) return RegresoEfecto::NoOp;
        ticket.consumido = true;
        if (!ticket.cierreNormalSolicitado
            || ticket.politica == PantallaRetorno::Reponer)
            return RegresoEfecto::NoOp;

        switch (ticket.cara)
        {
        case PantallaCara::Enlace:       return RegresoEfecto::Enlace;
        case PantallaCara::EsperarClick:return RegresoEfecto::EsperarClick;
        case PantallaCara::Eleccion:    return RegresoEfecto::Eleccion;
        case PantallaCara::Palabras:    return RegresoEfecto::Palabras;
        }
        return RegresoEfecto::NoOp;
    }
}
