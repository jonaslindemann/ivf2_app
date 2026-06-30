#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <miniaudio.h>

#include <filesystem>
#include <string>
#include <vector>

class AudioPlayer {
private:
    ma_engine m_engine{};
    ma_sound m_music{};
    bool m_engineReady{false};
    bool m_musicReady{false};
    bool m_playing{false};
    bool m_repeat{true};
    float m_volume{0.65f};
    std::string m_status{"Audio: not initialized"};
    std::vector<float> m_envelope;
    float m_windowSeconds{0.0f};
    std::filesystem::path m_audioPath;

public:
    ~AudioPlayer();

    bool load(const std::filesystem::path& audioPath);
    void drawContent();
    void setPaused(bool paused);

    bool isPlaying() const;
    float energy() const;

private:
    bool initEngine();
    void play();
    void pause();
    void unloadSound();
    void buildEnvelope(const std::filesystem::path& audioPath);
};
