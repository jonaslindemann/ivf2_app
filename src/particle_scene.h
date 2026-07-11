#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "creative_scene.h"

#include <ivf/gl.h>
#include <ivf/nodes.h>
#include <ivf/scene_timeline.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <vector>

class ParticleTimelineScene : public CreativeScene {
private:
    ivf::CompositeNodePtr m_root;
    ivf::ParticleSystemPtr m_ps;
    ivf::FlowFieldPtr m_field;

    float m_audioInputLevel{0.0f};
    bool m_audioInputPlaying{false};
    float m_audioLevel{0.0f};
    float m_audioSensitivity{1.5f};
    float m_audioImpact{1.0f};

    float m_scale{0.25f};
    float m_strength{3.5f};
    int m_octaves{2};
    float m_timeScale{0.06f};
    float m_baseEmitRate{800.0f};

    float m_startSize{1.0f};
    float m_endSize{2.0f};
    float m_audioStartSizeBoost{1.5f};
    float m_audioEndSizeBoost{1.0f};
    glm::vec4 m_startColor{0.7f, 0.5f, 0.2f, 0.3f};
    glm::vec4 m_endColor{0.7f, 0.1f, 0.0f, 0.0f};
    glm::vec4 m_startColorAudioBoost{0.3f, 0.35f, 0.6f, 0.35f};
    glm::vec4 m_endColorAudioBoost{0.5f, 0.1f, 0.25f, 0.0f};

    bool m_depthFog{true};
    float m_fogNear{12.0f};
    float m_fogFar{38.0f};
    glm::vec3 m_fogColor{0.0f, 0.0f, 0.0f};

    glm::vec3 m_cameraTarget{0.0f, 0.0f, 0.0f};
    bool m_autoCamera{true};
    float m_cameraRadius{26.0f};
    float m_cameraHeight{10.0f};
    float m_cameraUndulation{4.0f};
    float m_cameraOrbitSpeed{0.08f};
    float m_cameraUndulationSpeed{0.18f};

    // Path camera
    bool m_pathCamera{false};
    float m_pathSpeed{0.025f};      // cycles per second (ping-pong)
    float m_pathT{0.0f};            // accumulated normalised position [0,1] ping-pong
    float m_pathDir{1.0f};          // +1 forward, -1 backward
    std::vector<glm::vec3> m_pathPoints;

public:
    ParticleTimelineScene();

    static std::shared_ptr<ParticleTimelineScene> create();

    void onDrawControlsContent() override;
    void setAudioInput(float level, bool playing) override;
    nlohmann::json toJson() const override;
    void fromJson(const nlohmann::json& json) override;

protected:
    void setupProperties() override;
    void onPropertyChanged(const std::string& propertyName) override;

private:
    void setupSceneGraph();
    void buildCameraPath();
    void update(double localTime, double deltaTime);
    void updateAudioReaction(float dt);
    void updateCameraMotion(float t, float dt);
    glm::vec3 evalPath(float t) const;
    void applyParticleAppearance(float pulse, float bright);
    void applyFogSettings();

    std::filesystem::path findAsset(const std::filesystem::path& relativePath) const;
};
