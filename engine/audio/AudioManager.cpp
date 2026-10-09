// ============================================================================
// ECLIPS ENGINE - AudioManager (Implementacion con miniaudio)
// ============================================================================

#include "AudioManager.h"
#include "engine/core/LangPaths.h"

#include <iostream>
#include <cctype>
#include <cmath>

// Incluir miniaudio completo
// La implementacion (#define MINIAUDIO_IMPLEMENTATION) esta en
// third_party/miniaudio_impl.cpp para evitar recompilaciones lentas
#include "miniaudio.h"

namespace eclips
{
    // ── Rider: hint de formato no soportado ──────────────────────────────────
    // Solo habla cuando la EXTENSIÓN no es de un formato que Eclips decodifica.
    // Si la extensión es válida y aun así falla, el problema es otra cosa
    // (archivo ausente, corrupto) → callamos y dejamos que el código crudo de
    // miniaudio lo diga. No culpamos al formato cuando no es el formato.
    static std::string formatoHint(const std::string& path)
    {
        auto dot = path.find_last_of('.');
        std::string ext = (dot == std::string::npos) ? "" : path.substr(dot);
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);

        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg")
            return "";

        return "  >> formato no soportado ("
            + (ext.empty() ? std::string("sin extension") : ext)
            + "). Eclips lee: .wav .mp3 .flac .ogg";
    }

    AudioManager::AudioManager()
    {
    }

    AudioManager::~AudioManager()
    {
        if (m_initialized)
            shutdown();
    }

    bool AudioManager::init()
    {
        return iniciar(true, 0);
    }

    bool AudioManager::initSinDispositivo(u32 sampleRate)
    {
        return iniciar(false, sampleRate);
    }

    bool AudioManager::iniciar(bool conDispositivo, u32 sampleRate)
    {
        if (m_initialized) return true;

        // ── Crear el engine de miniaudio ──
        // ma_engine maneja todo: device de audio, mixer, decodificadores.
        // Un solo objeto para todo el sistema de audio.
        m_engine = new ma_engine();

        ma_engine_config config = ma_engine_config_init();
        // Usar defaults: formato nativo del sistema, sample rate nativo, etc.
        // miniaudio auto-detecta el mejor backend de audio (WASAPI en Windows).
        if (!conDispositivo)
        {
            // Sin placa: miniaudio exige fijar canales y frecuencia a mano.
            config.noDevice   = MA_TRUE;
            config.channels   = 2;
            config.sampleRate = sampleRate;
        }

        ma_result result = ma_engine_init(&config, m_engine);
        if (result != MA_SUCCESS)
        {
            std::cerr << "[Eclips] Error al inicializar audio (miniaudio): " << result << std::endl;
            delete m_engine;
            m_engine = nullptr;
            return false;
        }

        // ── El canal de los efectos ──
        // Efectos (playSound) y voces procedurales (la intro) van por este
        // grupo, y setSoundVolume mueve SOLO su volumen; la música va directo
        // a la salida con el suyo. Antes setSoundVolume movía la salida entera
        // (ma_engine_set_volume) y la música sonaba a música × efectos.
        m_sfx = new ma_sound();
        if (ma_sound_group_init(m_engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, m_sfx) != MA_SUCCESS)
        {
            std::cerr << "[Eclips] AVISO: no se pudo crear el canal de efectos; "
                      << "el volumen de efectos movera tambien la musica." << std::endl;
            delete m_sfx;
            m_sfx = nullptr;
        }
        else
        {
            ma_sound_group_set_volume(m_sfx, m_soundVolume);
        }

        m_initialized = true;
        m_sinDispositivo = !conDispositivo;

        if (!conDispositivo)
        {
            std::cout << "[Eclips] Audio inicializado sin dispositivo ("
                      << sampleRate << " Hz, estereo)" << std::endl;
            return true;
        }

        std::cout << "[Eclips] Audio inicializado (miniaudio)" << std::endl;
        std::cout << "[Eclips]   Backend: WASAPI (Windows Audio Session API)" << std::endl;
        std::cout << "[Eclips]   Musica: streaming (bajo consumo RAM)" << std::endl;
        std::cout << "[Eclips]   Efectos: decodificados (respuesta instantanea)" << std::endl;

        return true;
    }

    u64 AudioManager::leerMezcla(f32* destino, u64 frames)
    {
        if (!m_initialized || !m_engine || !m_sinDispositivo || !destino) return 0;
        ma_uint64 leidos = 0;
        if (ma_engine_read_pcm_frames(m_engine, destino, frames, &leidos) != MA_SUCCESS)
            return 0;
        return static_cast<u64>(leidos);
    }

    void AudioManager::shutdown()
    {
        if (!m_initialized) return;

        // Liberar voces procedurales ANTES del engine (un sonido vivo tras el
        // uninit del engine es UB). Orden: stop → uninit sound → uninit buffer → pcm.
        for (auto& v : m_procVoices)
        {
            if (v.sound) { ma_sound_stop(v.sound); ma_sound_uninit(v.sound); delete v.sound; }
            if (v.buffer)
            {
                ma_audio_buffer* buf = static_cast<ma_audio_buffer*>(v.buffer);
                ma_audio_buffer_uninit(buf);
                delete buf;
            }
            delete[] v.pcm;
        }
        m_procVoices.clear();

        // Detener y liberar musica
        if (m_music)
        {
            ma_sound_stop(m_music);
            ma_sound_uninit(m_music);
            delete m_music;
            m_music = nullptr;
        }

        // El canal de efectos, después de sus sonidos y antes del engine: al
        // soltarlo, miniaudio desengancha los efectos en línea que sigan
        // vivos, y el engine los libera al cerrarse.
        if (m_sfx)
        {
            ma_sound_group_uninit(m_sfx);
            delete m_sfx;
            m_sfx = nullptr;
        }

        // Liberar engine
        if (m_engine)
        {
            ma_engine_uninit(m_engine);
            delete m_engine;
            m_engine = nullptr;
        }

        m_initialized = false;
        m_sinDispositivo = false;
        std::cout << "[Eclips] Audio liberado." << std::endl;
    }

    // ════════════════════════════════════════════════════════════════════
    // MUSICA — Streaming (bajo consumo de RAM)
    // ════════════════════════════════════════════════════════════════════
    void AudioManager::playMusic(const std::string& pathPedida, bool loop)
    {
        if (!m_initialized || !m_engine) return;

        // CP4 (DISENO_IDIOMAS §6): las voces localizadas caen gratis. Esta casa
        // no tiene caché, así que la resolución es sólo para abrir el archivo;
        // y como las dos puertas viven acá y no en los callers, los 33
        // call-sites de los tres states quedan cubiertos sin tocar ninguno.
        const std::string path = langResolverRuta(pathPedida);

        // Si ya hay musica sonando, detenerla primero
        stopMusic();

        // Crear nuevo sound para la musica
        m_music = new ma_sound();

        // MA_SOUND_FLAG_STREAM = streaming (no decodifica todo a RAM)
        // Esto es lo que hace que la musica use ~64KB en vez de ~30MB
        ma_uint32 flags = MA_SOUND_FLAG_STREAM;

        ma_result result = ma_sound_init_from_file(
            m_engine, path.c_str(), flags,
            nullptr,  // grupo (null = default)
            nullptr,  // fence
            m_music
        );

        if (result != MA_SUCCESS)
        {
            std::cerr << "[Eclips] Error al cargar musica: " << path
                << " (codigo: " << result << ")"
                << formatoHint(path) << std::endl;
            delete m_music;
            m_music = nullptr;
            return;
        }

        // Configurar loop y volumen
        ma_sound_set_looping(m_music, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(m_music, m_musicVolume);

        // Reproducir
        ma_sound_start(m_music);

        std::cout << "[Eclips] Musica: " << path
                  << (loop ? " (loop)" : " (una vez)") << std::endl;
    }

    void AudioManager::stopMusic()
    {
        if (m_music)
        {
            ma_sound_stop(m_music);
            ma_sound_uninit(m_music);
            delete m_music;
            m_music = nullptr;
        }
    }

    void AudioManager::pauseMusic()
    {
        if (m_music)
            ma_sound_stop(m_music); // stop sin uninit = pausa
    }

    void AudioManager::resumeMusic()
    {
        if (m_music)
            ma_sound_start(m_music);
    }

    void AudioManager::setMusicVolume(f32 volume)
    {
        m_musicVolume = volume;
        if (m_music)
            ma_sound_set_volume(m_music, volume);
    }

    bool AudioManager::isMusicPlaying() const
    {
        if (m_music)
            return ma_sound_is_playing(m_music) == MA_TRUE;
        return false;
    }

    // ════════════════════════════════════════════════════════════════════
    // EFECTOS DE SONIDO — "Fire and forget"
    // ════════════════════════════════════════════════════════════════════
    // ma_engine_play_sound es la forma mas simple: le das un archivo,
    // miniaudio lo decodifica, lo reproduce, y lo libera automaticamente.
    // No necesitas manejar punteros ni ciclo de vida.
    // Perfecto para efectos cortos que no necesitas pausar ni controlar.
    // ════════════════════════════════════════════════════════════════════
    void AudioManager::playSound(const std::string& pathPedida)
    {
        if (!m_initialized || !m_engine) return;

        const std::string path = langResolverRuta(pathPedida);   // CP4, §6

        ma_result result = ma_engine_play_sound(
            m_engine, path.c_str(),
            m_sfx    // el canal de efectos (null = salida directa)
        );

        if (result != MA_SUCCESS)
        {
            std::cerr << "[Eclips] Error al reproducir efecto: " << path
                << formatoHint(path) << std::endl;
        }
    }

    void AudioManager::setSoundVolume(f32 volume)
    {
        m_soundVolume = volume;
        // Mueve el canal de efectos, no la salida: la música no se entera.
        // (ma_engine_play_sound sigue sin volumen por efecto; el nivel de cada
        // uno vive en su archivo.) Sin canal, se vuelve a la salida entera.
        if (m_sfx)
            ma_sound_group_set_volume(m_sfx, volume);
        else if (m_engine)
            ma_engine_set_volume(m_engine, volume);
    }

    i32 AudioManager::prepareProceduralTone(f32 freq, f32 durationSec, f32 volume,
        Waveform wave, f32 attackSec, f32 pulseWidth, f32 releaseSec)
    {
        if (!m_initialized || !m_engine) return -1;
        if (freq <= 0.0f || durationSec <= 0.0f) return -1;

        const ma_uint32 sampleRate = ma_engine_get_sample_rate(m_engine);
        const ma_uint32 channels = 1;
        const ma_uint32 frames = (ma_uint32)((f32)sampleRate * durationSec);
        if (frames == 0) return -1;

        if (attackSec < 0.001f)        attackSec = 0.001f;
        if (attackSec > durationSec)   attackSec = durationSec * 0.5f;
        // releaseSec llega por parámetro (default 0.015 = corte limpio). Clamp:
        // ni negativo ni más largo que el propio buffer.
        if (releaseSec < 0.001f)       releaseSec = 0.001f;
        if (releaseSec > durationSec)  releaseSec = durationSec * 0.5f;

        f32* pcm = new f32[frames];
        for (ma_uint32 i = 0; i < frames; ++i)
        {
            const f32 t = (f32)i / (f32)sampleRate;
            const f32 phase = freq * t - std::floor(freq * t);  // [0,1)

            // Oscilador chiptune.
            f32 raw;
            switch (wave)
            {
            case Waveform::Triangle: raw = 1.0f - 4.0f * std::fabs(phase - 0.5f); break;
            case Waveform::Pulse:    raw = (phase < pulseWidth) ? 1.0f : -1.0f;   break;
            case Waveform::Sine:
            default:                 raw = std::sin(phase * 6.28318530718f);       break;
            }

            // Envolvente: attack lineal → decay exponencial → release a cero.
            f32 env;
            if (t < attackSec)  env = t / attackSec;
            else                env = std::exp(-3.5f * (t - attackSec) / durationSec);
            if (t > durationSec - releaseSec)
                env *= (durationSec - t) / releaseSec;

            pcm[i] = raw * env * volume;
        }

        ma_audio_buffer_config cfg = ma_audio_buffer_config_init(
            ma_format_f32, channels, frames, pcm, nullptr);

        ma_audio_buffer* buffer = new ma_audio_buffer();
        if (ma_audio_buffer_init(&cfg, buffer) != MA_SUCCESS)
        {
            delete buffer; delete[] pcm; return -1;
        }

        ma_sound* sound = new ma_sound();
        if (ma_sound_init_from_data_source(m_engine, buffer, 0, m_sfx, sound) != MA_SUCCESS)
        {
            ma_audio_buffer_uninit(buffer);
            delete sound; delete buffer; delete[] pcm; return -1;
        }

        m_procVoices.push_back({ sound, static_cast<void*>(buffer), pcm });
        return static_cast<i32>(m_procVoices.size() - 1);
    }

    bool AudioManager::playPreparedTone(i32 voiceId)
    {
        if (!m_initialized || !m_engine) return false;
        if (voiceId < 0 || static_cast<size_t>(voiceId) >= m_procVoices.size())
            return false;

        ma_sound* sound = m_procVoices[static_cast<size_t>(voiceId)].sound;
        if (!sound) return false;

        if (ma_sound_seek_to_pcm_frame(sound, 0) != MA_SUCCESS)
            return false;
        return ma_sound_start(sound) == MA_SUCCESS;
    }

    void AudioManager::playProceduralTone(f32 freq, f32 durationSec, f32 volume,
        Waveform wave, f32 attackSec, f32 pulseWidth, f32 releaseSec)
    {
        const i32 voiceId = prepareProceduralTone(
            freq, durationSec, volume, wave, attackSec, pulseWidth, releaseSec);
        if (voiceId >= 0)
            playPreparedTone(voiceId);
    }

} // namespace eclips
