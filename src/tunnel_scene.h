#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ivf/gl.h>
#include <ivf/nodes.h>
#include <ivf/scene_timeline.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>

class TunnelTimelineScene : public ivf::TimelineScene {
private:
    ivf::CompositeNodePtr m_root;
    ivf::ParticleSystemPtr m_ps;

    float m_audioInputLevel{0.0f};
    bool m_audioInputPlaying{false};
    float m_audioLevel{0.0f};
    float m_audioSensitivity{1.7f};
    float m_audioImpact{1.0f};

    float m_emitRate{1800.0f};
    float m_tunnelRadius{7.0f};
    float m_ringSpacing{2.1f};
    float m_forwardSpeed{10.0f};
    float m_twistAmount{0.45f};
    float m_pulseStrength{1.6f};
    float m_tunnelLength{48.0f};

    float m_startSize{0.65f};
    float m_endSize{2.4f};
    float m_audioStartSizeBoost{0.9f};
    float m_audioEndSizeBoost{1.3f};
    glm::vec4 m_startColor{0.15f, 0.75f, 1.0f, 0.45f};
    glm::vec4 m_endColor{0.95f, 0.38f, 0.08f, 0.0f};
    glm::vec4 m_startColorAudioBoost{0.65f, 0.25f, 0.0f, 0.25f};
    glm::vec4 m_endColorAudioBoost{0.05f, 0.35f, 0.7f, 0.0f};

    bool m_depthFog{true};
    float m_fogNear{2.0f};
    float m_fogFar{42.0f};
    glm::vec3 m_fogColor{0.0f, 0.0f, 0.0f};

    float m_cameraSpeed{7.0f};
    float m_cameraSway{1.4f};
    float m_cameraRollHeight{0.9f};
    float m_time{0.0f};

public:
    TunnelTimelineScene();

    static std::shared_ptr<TunnelTimelineScene> create();

    void drawControlsContent();
    void setAudioInput(float level, bool playing);
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& json);

protected:
    void setupProperties() override;
    void onPropertyChanged(const std::string& propertyName) override;

private:
    void setupSceneGraph();
    void update(double localTime, double deltaTime);
    void updateAudioReaction(float dt);
    void updateCameraMotion(float t);
    void updateTunnelParticle(ivf::ParticleSystem::Particle& p, float dt);
    void applyEmitterSettings();
    void applyParticleAppearance(float pulse, float bright);
    void applyFogSettings();

    std::filesystem::path findAsset(const std::filesystem::path& relativePath) const;
};
