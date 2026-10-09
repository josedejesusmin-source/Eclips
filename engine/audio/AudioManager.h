// ============================================================================
// ECLIPS ENGINE - AudioManager (miniaudio)
// ============================================================================
//
// Sistema de audio de Eclips usando miniaudio (un solo archivo .h).
//
// Dos modos de reproduccion:
//
//   MUSICA (streaming):
//     - Se decodifica en tiempo real, pedacito a pedacito
//     - Usa ~64KB de buffer en RAM, sin importar el tamano del archivo
//     - Ideal para musica de fondo (archivos largos)
//     - Soporta loop
//
//   EFECTOS DE SONIDO (decodificado completo):
//     - Se carga entero a RAM al reproducir
//     - Ideal para sonidos cortos (pasos, clicks, golpes)
//     - Respuesta instantanea, sin latencia
//
// Formatos soportados: WAV, MP3, FLAC (de fábrica) + OGG/Vorbis (vía stb_vorbis,
// habilitado en miniaudio_impl.cpp — ver LEEME_AUDIO, Paso 1b). El soporte de
// formato vive en un solo cuello de botella (ma_decoder): música (streaming) y
// efectos (decodificados) heredan cada formato a la vez.
//
//   Sobre tu pregunta de WAV vs MP3:
//   WAV en disco: ~30MB para 3 min → en RAM: ~30MB (sin comprimir)
//   MP3 en disco: ~3MB para 3 min → en RAM con streaming: ~64KB
//   La diferencia es ENORME. Por eso usamos streaming para musica.
//   Para efectos cortos (<1 seg), WAV o MP3 da igual (~100KB en RAM).
//
// ============================================================================
#pragma once

#include "../core/Types.h"

// Forward declarations de miniaudio (para no incluir el header enorme aqui)
struct ma_engine;
struct ma_sound;

#include <string>
#include <memory>
#include <vector>

namespace eclips
{
    // Formas de onda para audio procedural (firma chiptune de la intro).
    enum class Waveform { Sine, Triangle, Pulse };

    class AudioManager
    {
    public:
        AudioManager();
        ~AudioManager();

        // Inicializar el sistema de audio
        bool init();

        // ── Sin dispositivo (harness y renders fuera de línea) ──
        // La misma mezcla que init(), pero sin abrir la placa de audio: nadie
        // la consume en tiempo real, la saca quien llame a leerMezcla().
        // Estéreo intercalado. Con dispositivo, leerMezcla() devuelve 0.
        bool initSinDispositivo(u32 sampleRate = 48000);
        u64 leerMezcla(f32* destino, u64 frames);

        // Liberar todo
        void shutdown();

        // ── Musica (streaming, bajo consumo de RAM) ──
        // path: ruta al archivo (WAV, MP3, FLAC, OGG)
        // loop: repetir al terminar
        void playMusic(const std::string& path, bool loop = true);
        void stopMusic();
        void pauseMusic();
        void resumeMusic();
        void setMusicVolume(f32 volume);  // 0.0 = silencio, 1.0 = maximo
        bool isMusicPlaying() const;

        // ── Efectos de sonido (decodificado, respuesta instantanea) ──
        // Van por su propio canal, con las voces procedurales de la intro:
        // setSoundVolume mueve SOLO ese canal; la música tiene el suyo.
        void playSound(const std::string& path);
        void setSoundVolume(f32 volume);

        // ── Audio procedural chiptune (cero assets) — F5 de la intro ──
        // Genera una forma de onda (sine/triangle/pulse) con envolvente
        // attack-decay-release y la reproduce fire-and-forget. Pool interno
        // libera todo en shutdown(). attackSec controla pluck (corto) vs swell
        // (largo); pulseWidth da el duty del Pulse (0.5=square, 0.25=nasal GB).
        // releaseSec = rampa final a cero: corto (0.015) = corte limpio; largo
        // (~0.3) = la cola se DESVANECE en vez de cortarse (campanas/golpes graves).
        // prepareProceduralTone separa la generación del disparo: la intro prepara
        // sus tres voces durante el preroll y en el beat solo llama start.
        i32 prepareProceduralTone(f32 freq, f32 durationSec, f32 volume = 0.8f,
            Waveform wave = Waveform::Sine,
            f32 attackSec = 0.005f, f32 pulseWidth = 0.5f,
            f32 releaseSec = 0.015f);
        bool playPreparedTone(i32 voiceId);

        // Compatibilidad fire-and-forget para callers que no necesitan preroll.
        void playProceduralTone(f32 freq, f32 durationSec, f32 volume = 0.8f,
            Waveform wave = Waveform::Sine,
            f32 attackSec = 0.005f, f32 pulseWidth = 0.5f,
            f32 releaseSec = 0.015f);

        // ── Estado ──
        bool isInitialized() const { return m_initialized; }

    private:
        bool iniciar(bool conDispositivo, u32 sampleRate);

        ma_engine* m_engine = nullptr;
        ma_sound*  m_music  = nullptr;
        ma_sound*  m_sfx    = nullptr;   // canal de efectos (ma_sound_group es ma_sound)
        bool m_initialized  = false;
        bool m_sinDispositivo = false;
        f32 m_musicVolume   = 0.7f;
        f32 m_soundVolume   = 1.0f;

        // ── Pool de voces procedurales (intro del motor) ──
        // Cada voz retiene PCM + buffer + sound hasta shutdown(), liberados en
        // orden seguro (sonidos antes que el engine). Suficiente para un one-shot.
        struct ProceduralVoice
        {
            ma_sound* sound = nullptr;
            void* buffer = nullptr;   // ← era ma_audio_buffer*  (opaco en el header)
            f32* pcm = nullptr;
        };
        std::vector<ProceduralVoice> m_procVoices;
    };
}
