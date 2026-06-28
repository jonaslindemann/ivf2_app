#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "tunnel_scene.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {

template<typename T>
void readIfPresent(const nlohmann::json& json, const char* key, T& value)
{
    const auto it = json.find(key);
    if (it != json.end() && !it->is_null())
        value = it->get<T>();
}

void readVec4IfPresent(const nlohmann::json& json, const char* key, glm::vec4& target)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array() || it->size() != 4)
        return;

    target = glm::vec4(
        (*it)[0].get<float>(),
        (*it)[1].get<float>(),
        (*it)[2].get<float>(),
        (*it)[3].get<float>());
}

void readVec3IfPresent(const nlohmann::json& json, const char* key, glm::vec3& target)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array() || it->size() != 3)
        return;

    target = glm::vec3(
        (*it)[0].get<float>(),
        (*it)[1].get<float>(),
        (*it)[2].get<float>());
}

float wrapPositive(float value, float range)
{
    if (range <= 0.0f)
        return 0.0f;

    value = std::fmod(value, range);
    if (value < 0.0f)
        value += range;
    return value;
}

}

using namespace ivf;

TunnelTimelineScene::TunnelTimelineScene()
    : TimelineScene("Pulse Corridor", 45.0)
{
    setupSceneGraph();

    root(m_root);
    camera(TimelineCamera::lookAt(glm::vec3(0.0f, 0.5f, 10.0f), glm::vec3(0.0f, 0.0f, -8.0f), 58.0));
    onUpdate([this](double localTime, double deltaTime) { update(localTime, deltaTime); });
}

std::shared_ptr<TunnelTimelineScene> TunnelTimelineScene::create()
{
    return std::make_shared<TunnelTimelineScene>();
}

void TunnelTimelineScene::drawControls()
{
    ImGui::SetNextWindowSize({300, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({300, 10}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Pulse Corridor");

    ImGui::Text("Alive : %d / %d", m_ps->aliveCount(), m_ps->maxParticles());
    ImGui::Text("Time  : %.2f s", TimeController::instance()->elapsed());

    ImGui::Separator();
    ImGui::TextUnformatted("Tunnel");
    bool emitterChanged = false;
    emitterChanged |= ImGui::SliderFloat("Emit rate##tunnel", &m_emitRate, 200.0f, 5000.0f);
    ImGui::SliderFloat("Radius", &m_tunnelRadius, 2.0f, 16.0f);
    ImGui::SliderFloat("Ring spacing", &m_ringSpacing, 0.4f, 6.0f);
    ImGui::SliderFloat("Forward speed", &m_forwardSpeed, 1.0f, 26.0f);
    ImGui::SliderFloat("Twist", &m_twistAmount, -2.0f, 2.0f);
    ImGui::SliderFloat("Pulse strength", &m_pulseStrength, 0.0f, 4.0f);
    emitterChanged |= ImGui::SliderFloat("Tunnel length", &m_tunnelLength, 16.0f, 90.0f);
    if (emitterChanged)
        applyEmitterSettings();

    ImGui::Separator();
    ImGui::TextUnformatted("Particle appearance");
    bool appearanceChanged = false;
    appearanceChanged |= ImGui::SliderFloat("Start size##tunnel", &m_startSize, 0.01f, 5.0f);
    appearanceChanged |= ImGui::SliderFloat("End size##tunnel", &m_endSize, 0.0f, 7.0f);
    appearanceChanged |= ImGui::ColorEdit4("Start color##tunnel", &m_startColor.x);
    appearanceChanged |= ImGui::ColorEdit4("End color##tunnel", &m_endColor.x);
    if (appearanceChanged)
        applyParticleAppearance(m_audioLevel * m_audioImpact, std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));

    ImGui::Separator();
    ImGui::TextUnformatted("Depth fog");
    bool fogChanged = false;
    fogChanged |= ImGui::Checkbox("Enabled##tunnelFog", &m_depthFog);
    fogChanged |= ImGui::SliderFloat("Fog near##tunnel", &m_fogNear, 0.0f, 50.0f);
    fogChanged |= ImGui::SliderFloat("Fog far##tunnel", &m_fogFar, 1.0f, 120.0f);
    fogChanged |= ImGui::ColorEdit3("Fog color##tunnel", &m_fogColor.x);
    if (fogChanged) {
        m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);
        applyFogSettings();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Camera");
    ImGui::SliderFloat("Camera speed", &m_cameraSpeed, 0.0f, 20.0f);
    ImGui::SliderFloat("Sway", &m_cameraSway, 0.0f, 5.0f);
    ImGui::SliderFloat("Height roll", &m_cameraRollHeight, 0.0f, 3.0f);

    ImGui::Separator();
    ImGui::TextUnformatted("Audio reaction");
    ImGui::ProgressBar(m_audioLevel, {-1.0f, 0.0f}, "Energy");
    ImGui::SliderFloat("Sensitivity##tunnel", &m_audioSensitivity, 0.2f, 5.0f);
    ImGui::SliderFloat("Impact##tunnel", &m_audioImpact, 0.0f, 3.0f);
    ImGui::SliderFloat("Start size boost##tunnel", &m_audioStartSizeBoost, 0.0f, 4.0f);
    ImGui::SliderFloat("End size boost##tunnel", &m_audioEndSizeBoost, 0.0f, 5.0f);
    ImGui::ColorEdit4("Start color boost##tunnel", &m_startColorAudioBoost.x);
    ImGui::ColorEdit4("End color boost##tunnel", &m_endColorAudioBoost.x);

    ImGui::End();
}

void TunnelTimelineScene::setAudioInput(float level, bool playing)
{
    m_audioInputLevel = std::clamp(level, 0.0f, 1.0f);
    m_audioInputPlaying = playing;
}

nlohmann::json TunnelTimelineScene::toJson() const
{
    return {
        {"audioSensitivity", m_audioSensitivity},
        {"audioImpact", m_audioImpact},
        {"emitRate", m_emitRate},
        {"tunnelRadius", m_tunnelRadius},
        {"ringSpacing", m_ringSpacing},
        {"forwardSpeed", m_forwardSpeed},
        {"twistAmount", m_twistAmount},
        {"pulseStrength", m_pulseStrength},
        {"tunnelLength", m_tunnelLength},
        {"startSize", m_startSize},
        {"endSize", m_endSize},
        {"audioStartSizeBoost", m_audioStartSizeBoost},
        {"audioEndSizeBoost", m_audioEndSizeBoost},
        {"startColor", {m_startColor.r, m_startColor.g, m_startColor.b, m_startColor.a}},
        {"endColor", {m_endColor.r, m_endColor.g, m_endColor.b, m_endColor.a}},
        {"startColorAudioBoost", {m_startColorAudioBoost.r, m_startColorAudioBoost.g, m_startColorAudioBoost.b, m_startColorAudioBoost.a}},
        {"endColorAudioBoost", {m_endColorAudioBoost.r, m_endColorAudioBoost.g, m_endColorAudioBoost.b, m_endColorAudioBoost.a}},
        {"depthFog", m_depthFog},
        {"fogNear", m_fogNear},
        {"fogFar", m_fogFar},
        {"fogColor", {m_fogColor.r, m_fogColor.g, m_fogColor.b}},
        {"cameraSpeed", m_cameraSpeed},
        {"cameraSway", m_cameraSway},
        {"cameraRollHeight", m_cameraRollHeight}
    };
}

void TunnelTimelineScene::fromJson(const nlohmann::json& json)
{
    if (!json.is_object())
        return;

    readIfPresent(json, "audioSensitivity", m_audioSensitivity);
    readIfPresent(json, "audioImpact", m_audioImpact);
    readIfPresent(json, "emitRate", m_emitRate);
    readIfPresent(json, "tunnelRadius", m_tunnelRadius);
    readIfPresent(json, "ringSpacing", m_ringSpacing);
    readIfPresent(json, "forwardSpeed", m_forwardSpeed);
    readIfPresent(json, "twistAmount", m_twistAmount);
    readIfPresent(json, "pulseStrength", m_pulseStrength);
    readIfPresent(json, "tunnelLength", m_tunnelLength);
    readIfPresent(json, "startSize", m_startSize);
    readIfPresent(json, "endSize", m_endSize);
    readIfPresent(json, "audioStartSizeBoost", m_audioStartSizeBoost);
    readIfPresent(json, "audioEndSizeBoost", m_audioEndSizeBoost);
    readVec4IfPresent(json, "startColor", m_startColor);
    readVec4IfPresent(json, "endColor", m_endColor);
    readVec4IfPresent(json, "startColorAudioBoost", m_startColorAudioBoost);
    readVec4IfPresent(json, "endColorAudioBoost", m_endColorAudioBoost);
    readIfPresent(json, "depthFog", m_depthFog);
    readIfPresent(json, "fogNear", m_fogNear);
    readIfPresent(json, "fogFar", m_fogFar);
    readVec3IfPresent(json, "fogColor", m_fogColor);
    readIfPresent(json, "cameraSpeed", m_cameraSpeed);
    readIfPresent(json, "cameraSway", m_cameraSway);
    readIfPresent(json, "cameraRollHeight", m_cameraRollHeight);

    m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);

    if (m_ps) {
        applyEmitterSettings();
        applyParticleAppearance(m_audioLevel * m_audioImpact, std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));
        applyFogSettings();
    }
}

void TunnelTimelineScene::setupSceneGraph()
{
    m_root = CompositeNode::create();

    auto texture = Texture::create();
    texture->load(findAsset("assets/circle.png").string());

    m_ps = ParticleSystem::create(90000);
    m_ps->setEmitRate(m_emitRate);
    m_ps->setLifetime(5.0f, 8.0f);
    m_ps->setGravity(glm::vec3(0.0f));
    m_ps->setInitialVelocity(glm::vec3(0.0f), glm::vec3(0.0f));
    m_ps->setEmitFromBox(glm::vec3(-1.0f, -1.0f, -m_tunnelLength), glm::vec3(1.0f, 1.0f, 0.0f));
    m_ps->setBillboard(true);
    m_ps->setTexture(texture);
    m_ps->setUpdateFunction([this](ParticleSystem::Particle& p, float dt) { updateTunnelParticle(p, dt); });
    applyParticleAppearance(0.0f, 0.0f);
    applyFogSettings();
    m_ps->start();
    m_root->add(m_ps);
}

void TunnelTimelineScene::update(double localTime, double deltaTime)
{
    const auto dt = static_cast<float>(deltaTime);
    m_time = static_cast<float>(localTime);

    updateAudioReaction(dt);
    updateCameraMotion(m_time);
    m_ps->update(dt);
}

void TunnelTimelineScene::updateAudioReaction(float dt)
{
    const float targetLevel = m_audioInputPlaying ? m_audioInputLevel * m_audioSensitivity : 0.0f;
    const float smoothing = 1.0f - std::exp(-(std::max)(dt, 1.0f / 120.0f) * 14.0f);
    m_audioLevel += (targetLevel - m_audioLevel) * smoothing;

    const float pulse = m_audioLevel * m_audioImpact;
    const float bright = std::clamp(pulse, 0.0f, 1.0f);

    m_ps->setEmitRate(m_emitRate * (1.0f + 1.2f * pulse));
    applyParticleAppearance(pulse, bright);
}

void TunnelTimelineScene::updateCameraMotion(float t)
{
    const float travelPhase = t * m_cameraSpeed * 0.08f;
    const glm::vec3 position(
        std::sin(t * 0.47f) * m_cameraSway,
        std::cos(t * 0.31f) * m_cameraRollHeight,
        8.0f + std::sin(travelPhase) * 0.6f);
    const glm::vec3 target(
        std::sin(t * 0.47f + 0.4f) * m_cameraSway * 0.6f,
        std::cos(t * 0.31f + 0.3f) * m_cameraRollHeight * 0.4f,
        -18.0f + std::sin(travelPhase + 0.7f) * 1.2f);

    camera(TimelineCamera::lookAt(position, target, 58.0, 0.2, 140.0));
}

void TunnelTimelineScene::updateTunnelParticle(ParticleSystem::Particle& p, float dt)
{
    const float age = p.maxLife - p.life;
    const bool justSpawned = age <= dt * 1.5f;
    const float spacing = (std::max)(0.1f, m_ringSpacing);
    const float length = (std::max)(spacing * 2.0f, m_tunnelLength);
    const float pulse = m_audioLevel * m_audioImpact;
    const float beatRadius = 1.0f + m_pulseStrength * std::clamp(pulse, 0.0f, 1.5f);

    if (justSpawned) {
        const float angle = std::atan2(p.position.y, p.position.x);
        const float ring = std::floor(wrapPositive(-p.position.z, length) / spacing);
        const float ringPhase = ring * 0.73f + m_time * (0.8f + std::abs(m_twistAmount));
        const float radiusNoise = 0.88f + 0.24f * std::sin(angle * 3.0f + ringPhase);
        const float radius = m_tunnelRadius * beatRadius * radiusNoise;

        p.position.x = std::cos(angle + ringPhase * m_twistAmount) * radius;
        p.position.y = std::sin(angle + ringPhase * m_twistAmount) * radius;
        p.position.z = -ring * spacing - wrapPositive(m_time * m_forwardSpeed, spacing);
    }

    const float z01 = std::clamp((-p.position.z) / length, 0.0f, 1.0f);
    const float twist = (m_twistAmount * 0.9f + pulse * 0.35f) * dt;
    const float s = std::sin(twist);
    const float c = std::cos(twist);
    const float x = p.position.x;
    const float y = p.position.y;

    p.position.x = x * c - y * s;
    p.position.y = x * s + y * c;
    p.position.z += m_forwardSpeed * dt;

    const float targetRadius = m_tunnelRadius * (1.0f + m_pulseStrength * 0.25f * pulse) * (0.85f + 0.3f * z01);
    const float radius = (std::max)(0.001f, std::sqrt(p.position.x * p.position.x + p.position.y * p.position.y));
    const float radialBlend = 1.0f - std::exp(-dt * 4.0f);
    const float adjustedRadius = radius + (targetRadius - radius) * radialBlend;
    p.position.x *= adjustedRadius / radius;
    p.position.y *= adjustedRadius / radius;

    if (p.position.z > 10.0f) {
        p.position.z -= length;
        p.life = (std::min)(p.life, p.maxLife * 0.35f);
    }
}

void TunnelTimelineScene::applyParticleAppearance(float pulse, float bright)
{
    if (!m_ps)
        return;

    const auto clampColor = [](const glm::vec4& color) {
        return glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
    };

    m_ps->setStartSize((std::max)(0.0f, m_startSize + m_audioStartSizeBoost * pulse));
    m_ps->setEndSize((std::max)(0.0f, m_endSize + m_audioEndSizeBoost * pulse));
    m_ps->setStartColor(clampColor(m_startColor + m_startColorAudioBoost * bright));
    m_ps->setEndColor(clampColor(m_endColor + m_endColorAudioBoost * bright));
}

void TunnelTimelineScene::applyEmitterSettings()
{
    if (!m_ps)
        return;

    m_ps->setEmitRate(m_emitRate);
    m_ps->setEmitFromBox(glm::vec3(-1.0f, -1.0f, -m_tunnelLength), glm::vec3(1.0f, 1.0f, 0.0f));
}

void TunnelTimelineScene::applyFogSettings()
{
    if (!m_ps)
        return;

    m_ps->setDepthFog(m_depthFog);
    m_ps->setFogRange(m_fogNear, m_fogFar);
    m_ps->setFogColor(m_fogColor);
}

std::filesystem::path TunnelTimelineScene::findAsset(const std::filesystem::path& relativePath) const
{
    const std::filesystem::path candidates[] = {
        relativePath,
        std::filesystem::path("bin/Release") / relativePath,
        std::filesystem::path("bin/Debug") / relativePath,
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate))
            return candidate;
    }

    return relativePath;
}
