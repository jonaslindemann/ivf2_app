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

/**
 * @class AttractorTimelineScene
 * @brief Timeline scene tracing a Lorenz strange attractor as a glowing particle trail.
 *
 * The attractor is integrated on the CPU. Each frame, one additive billboard
 * sprite is emitted at every new integration point along the path the head
 * traversed this frame, so the glowing trail draws out the attractor's shape and
 * fades behind the head. This reuses the same ParticleSystem + bloom look as the
 * other scenes. Audio energy drives the integration speed, sprite size, and color.
 */
class AttractorTimelineScene : public CreativeScene {
private:
    ivf::CompositeNodePtr m_root;
    ivf::ParticleSystemPtr m_ps;

    // Attractor state
    glm::vec3 m_state{0.1f, 0.0f, 0.0f};

    // Lorenz parameters
    float m_sigma{10.0f};
    float m_rho{28.0f};
    float m_beta{8.0f / 3.0f};

    float m_speed{600.0f};  // integration steps (= sprites) per second
    float m_stepSize{0.005f};
    float m_scale{0.3f};
    glm::vec3 m_center{0.0f, -7.5f, 0.0f}; // scaled-space offset to frame at origin

    // Trail appearance
    float m_trailSeconds{5.0f}; // sprite lifetime => trail length
    float m_startSize{0.6f};
    float m_endSize{0.0f};
    glm::vec4 m_startColor{0.2f, 0.8f, 1.0f, 0.6f};
    glm::vec4 m_endColor{0.7f, 0.2f, 1.0f, 0.0f};

    // Audio reaction
    float m_audioInputLevel{0.0f};
    bool m_audioInputPlaying{false};
    float m_audioLevel{0.0f};
    float m_audioSensitivity{1.6f};
    float m_audioImpact{1.0f};
    float m_audioSpeedBoost{1.4f};
    float m_audioStartSizeBoost{0.8f};
    float m_audioEndSizeBoost{0.4f};
    glm::vec4 m_startColorAudioBoost{0.6f, 0.1f, 0.0f, 0.3f};
    glm::vec4 m_endColorAudioBoost{0.2f, 0.3f, 0.0f, 0.0f};

    // Depth fog
    bool m_depthFog{true};
    float m_fogNear{8.0f};
    float m_fogFar{55.0f};
    glm::vec3 m_fogColor{0.0f, 0.0f, 0.0f};

    // Camera
    float m_cameraRadius{26.0f};
    float m_cameraHeight{6.0f};
    float m_cameraUndulation{3.0f};
    float m_cameraOrbitSpeed{0.12f};
    float m_cameraUndulationSpeed{0.2f};

public:
    AttractorTimelineScene();

    static std::shared_ptr<AttractorTimelineScene> create();

    void onDrawControlsContent() override;
    void setAudioInput(float level, bool playing) override;
    nlohmann::json toJson() const override;
    void fromJson(const nlohmann::json& json) override;

protected:
    void setupProperties() override;
    void onPropertyChanged(const std::string& propertyName) override;

private:
    void setupSceneGraph();
    void stepAttractor();
    glm::vec3 mapState(const glm::vec3& state) const;
    void emitAlongPath(int steps);
    void update(double localTime, double deltaTime);
    void updateAudioReaction(float dt);
    void updateCameraMotion(float t);
    void applyParticleAppearance(float pulse, float bright);
    void applyFogSettings();

    std::filesystem::path findAsset(const std::filesystem::path& relativePath) const;
};
