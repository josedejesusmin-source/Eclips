#pragma once

// ============================================================================
// VNSecuencia.h -- LA LEY PURA DE LA SECUENCIA (Konami como mecanismo)
// ============================================================================
//
// Contrato: DISENO_SECUENCIA.md -- SQ-5 (reinicio por prefijo / KMP), SQ-11
// (gana la primera escrita), SQ-12 (este header, sin SDL, sobre codigos de
// tecla enteros). Lo prueba harness_secuencia_ley.cpp (K1-K5).
//
// Sin SDL. Sin VNScriptState. ASCII puro.
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace eclips
{
    // Tabla de falla KMP: falla[i] = longitud del borde mas largo propio de
    // teclas[0..i] (prefijo que tambien es sufijo). Al fallar en el progreso j,
    // el reinicio es falla[j-1], nunca 0-o-1 del fork de la P1 (HO-K1).
    inline std::vector<int> vnSecuenciaFallaDe(const std::vector<std::int32_t>& teclas)
    {
        const int n = static_cast<int>(teclas.size());
        std::vector<int> falla(static_cast<size_t>(n), 0);
        for (int i = 1; i < n; ++i)
        {
            int len = falla[static_cast<size_t>(i - 1)];
            while (len > 0 &&
                   teclas[static_cast<size_t>(i)] != teclas[static_cast<size_t>(len)])
            {
                len = falla[static_cast<size_t>(len - 1)];
            }
            if (teclas[static_cast<size_t>(i)] == teclas[static_cast<size_t>(len)])
                ++len;
            falla[static_cast<size_t>(i)] = len;
        }
        return falla;
    }

    struct VNSecuencia
    {
        std::vector<std::int32_t> teclas;
        std::string               target;
        int                       progreso = 0;
        std::vector<int>          falla;   // helper: reinicio por prefijo (KMP)
    };

    inline VNSecuencia vnSecuenciaArmar(const std::vector<std::int32_t>& teclas,
                                        const std::string& target)
    {
        VNSecuencia s;
        s.teclas   = teclas;
        s.target   = target;
        s.progreso = 0;
        s.falla    = vnSecuenciaFallaDe(teclas);
        return s;
    }

    // true si ESTA tecla completa la secuencia. Al completar, progreso vuelve a 0
    // (sin seguir el borde post-match: el contrato reinicia al armar de nuevo).
    inline bool vnSecuenciaAvanzar(VNSecuencia& s, std::int32_t tecla)
    {
        const int n = static_cast<int>(s.teclas.size());
        if (n == 0) return false;
        if (static_cast<int>(s.falla.size()) != n)
            s.falla = vnSecuenciaFallaDe(s.teclas);

        int j = s.progreso;
        for (;;)
        {
            if (s.teclas[static_cast<size_t>(j)] == tecla)
            {
                ++j;
                break;
            }
            if (j == 0) break;
            j = s.falla[static_cast<size_t>(j - 1)];
        }
        if (j == n)
        {
            s.progreso = 0;
            return true;
        }
        s.progreso = j;
        return false;
    }

    // Indice de la primera ESCRITA que esta tecla completo; -1 si ninguna.
    // Todas avanzan; el desempate es el orden del vector (SQ-11).
    inline int vnSecuenciasAvanzar(std::vector<VNSecuencia>& lista, std::int32_t tecla)
    {
        int gana = -1;
        for (size_t i = 0; i < lista.size(); ++i)
        {
            if (vnSecuenciaAvanzar(lista[i], tecla) && gana < 0)
                gana = static_cast<int>(i);
        }
        return gana;
    }
} // namespace eclips
