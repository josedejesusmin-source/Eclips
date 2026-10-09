// ============================================================================
// VNExecutionPolicy.h — reglas puras del pump de comandos VN
// ============================================================================
//
// Seam mínimo de prueba para el bug preexistente de esperar_animacion: el
// batch de comandos instantáneos debe detenerse en el mismo dispatch que activa
// waitingForAnim. Mantener esta regla fuera del estado pesado permite probarla
// sin levantar SDL, OpenGL ni VNScriptState completo.
// ============================================================================

#pragma once

namespace eclips
{
    // Fases del intérprete en un header liviano: CP0 puede probar la allowlist
    // de Advance sin construir el estado gráfico completo. Los ordinales
    // existentes son parte del estado serializado; WaitingInterface se agrega
    // al final para no mover WaitingTextInput ni Fading.
    enum class VNPhase
    {
        Executing = 0,
        WaitingInput = 1,
        WaitingChoice = 2,
        WaitingQTE = 3,
        WaitingClick = 4,
        WaitingTextInput = 5,
        Fading = 6,
        WaitingInterface = 7
    };

    // Se llama únicamente ante VNDialogBox::Event::Advance. Confirm, auto,
    // skip e Interrumpir convergen en este mismo evento; Choice/QTE/hotspot
    // tienen eventos propios. Un modal requerido sella el commit.
    inline bool vnAcceptsDialogueAdvance(VNPhase phase, bool modalRequired)
    {
        return phase == VNPhase::WaitingInput && !modalRequired;
    }

    // Fases que pueden cobrar el skip-frame cuando no queda ninguna fuente
    // dinámica. WaitingInterface es una espera viva para input, no un timer:
    // una UI estática puede dormir igual que un diálogo o una espera de click.
    inline bool vnPhaseCanQuiesce(VNPhase phase)
    {
        return phase == VNPhase::WaitingInput ||
               phase == VNPhase::WaitingChoice ||
               phase == VNPhase::WaitingClick ||
               phase == VNPhase::WaitingInterface;
    }

    inline bool vnInstantBatchCanContinue(bool waitingForGameplay,
                                          bool waitingForAnim,
                                          float waitTimer)
    {
        return !waitingForGameplay && !waitingForAnim && waitTimer <= 0.0f;
    }
}
