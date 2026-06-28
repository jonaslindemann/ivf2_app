#ifndef NOMINMAX
#define NOMINMAX
#endif

#define MINIAUDIO_IMPLEMENTATION

#include "audio_player.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

AudioPlayer::~AudioPlayer()
{
    unloadSound();

    if (m_engineReady)
        ma_engine_uninit(&m_engine);
}

bool AudioPlayer::load(const std::filesystem::path& audioPath)
{
    if (!initEngine())
        return false;

    unloadSound();

    m_audioPath = audioPath;
    m_envelope.clear();
    m_windowSeconds = 0.0f;

    if (ma_sound_init_from_file(
            &m_engine,
            audioPath.string().c_str(),
            MA_SOUND_FLAG_STREAM,
            nullptr,
            nullptr,
            &m_music) != MA_SUCCESS) {
        m_status = "Audio: failed to load " + audioPath.string();
        return false;
    }

    m_status = "Audio: loaded " + audioPath.filename().string();
    buildEnvelope(audioPath);

    m_musicReady = true;
    ma_sound_set_looping(&m_music, m_repeat ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&m_music, m_volume);
    return true;
}

void AudioPlayer::drawControls()
{
    ImGui::SetNextWindowSize({280, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({10, 360}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Audio");

    ImGui::TextUnformatted(m_status.c_str());
    if (m_musicReady) {
        if (ImGui::Button(m_playing ? "Pause audio" : "Play audio")) {
            if (m_playing)
                pause();
            else
                play();
        }

        if (ImGui::SliderFloat("Volume", &m_volume, 0.0f, 1.0f))
            ma_sound_set_volume(&m_music, m_volume);

        if (ImGui::Checkbox("Repeat", &m_repeat))
            ma_sound_set_looping(&m_music, m_repeat ? MA_TRUE : MA_FALSE);

        ImGui::ProgressBar(energy(), {-1.0f, 0.0f}, "Energy");
    }

    ImGui::End();
}

void AudioPlayer::setPaused(bool paused)
{
    if (paused)
        pause();
    else
        play();
}

bool AudioPlayer::isPlaying() const
{
    return m_playing;
}

float AudioPlayer::energy() const
{
    if (!m_musicReady || !m_playing || m_envelope.empty() || m_windowSeconds <= 0.0f)
        return 0.0f;

    float cursorSeconds = 0.0f;
    if (ma_sound_get_cursor_in_seconds(&m_music, &cursorSeconds) != MA_SUCCESS)
        return 0.0f;

    const auto index = static_cast<size_t>(cursorSeconds / m_windowSeconds) % m_envelope.size();
    return std::clamp(m_envelope[index], 0.0f, 1.0f);
}

bool AudioPlayer::initEngine()
{
    if (m_engineReady)
        return true;

    if (ma_engine_init(nullptr, &m_engine) != MA_SUCCESS) {
        m_status = "Audio: failed to initialize engine";
        return false;
    }

    m_engineReady = true;
    return true;
}

void AudioPlayer::play()
{
    if (!m_musicReady)
        return;

    if (ma_sound_start(&m_music) == MA_SUCCESS) {
        m_playing = true;
        m_status = "Audio: playing " + m_audioPath.filename().string();
    } else {
        m_status = "Audio: failed to start playback";
    }
}

void AudioPlayer::pause()
{
    if (!m_musicReady)
        return;

    ma_sound_stop(&m_music);
    m_playing = false;
    m_status = "Audio: paused";
}

void AudioPlayer::unloadSound()
{
    if (!m_musicReady)
        return;

    ma_sound_uninit(&m_music);
    m_musicReady = false;
    m_playing = false;
}

void AudioPlayer::buildEnvelope(const std::filesystem::path& audioPath)
{
    constexpr ma_uint32 analysisSampleRate = 44100;
    constexpr ma_uint32 analysisChannels = 1;
    constexpr ma_uint64 framesPerWindow = 1024;

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, analysisChannels, analysisSampleRate);
    ma_decoder decoder{};
    if (ma_decoder_init_file(audioPath.string().c_str(), &config, &decoder) != MA_SUCCESS)
        return;

    std::vector<float> frames(static_cast<size_t>(framesPerWindow) * analysisChannels);
    float peak = 0.0f;

    for (;;) {
        ma_uint64 framesRead = 0;
        if (ma_decoder_read_pcm_frames(&decoder, frames.data(), framesPerWindow, &framesRead) != MA_SUCCESS || framesRead == 0)
            break;

        float sumSquares = 0.0f;
        for (ma_uint64 i = 0; i < framesRead; ++i)
            sumSquares += frames[static_cast<size_t>(i)] * frames[static_cast<size_t>(i)];

        const float rms = std::sqrt(sumSquares / static_cast<float>(framesRead));
        m_envelope.push_back(rms);
        peak = (std::max)(peak, rms);
    }

    ma_decoder_uninit(&decoder);

    if (peak > 0.0f) {
        for (auto& level : m_envelope)
            level = std::clamp(level / peak, 0.0f, 1.0f);

        m_windowSeconds = static_cast<float>(framesPerWindow) / static_cast<float>(analysisSampleRate);
        m_status += " + reactive envelope";
    }
}
