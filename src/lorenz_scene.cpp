#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "lorenz_scene.h"

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

void readVec3IfPresent(const nlohmann::json& json, const char* key, glm::vec3& target)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array() || it->size() != 3)
        return;

    target = glm::vec3((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>());
}

void readVec4IfPresent(const nlohmann::json& json, const char* key, glm::vec4& target)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array() || it->size() != 4)
        return;

    target = glm::vec4((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>(), (*it)[3].get<float>());
}

// Cap on sprites emitted per frame, guarding against frame-time spikes.
constexpr int k_maxStepsPerFrame = 2000;

} // namespace

using namespace ivf;

AttractorTimelineScene::AttractorTimelineScene() : TimelineScene("Lorenz Drift", 50.0)
{
    setupSceneGraph();

    root(m_root);
    camera(TimelineCamera::lookAt(glm::vec3(0.0f, m_cameraHeight, m_cameraRadius), glm::vec3(0.0f)));
    onUpdate([this](double localTime, double deltaTime) { update(localTime, deltaTime); });
}

std::shared_ptr<AttractorTimelineScene> AttractorTimelineScene::create()
{
    return std::make_shared<AttractorTimelineScene>();
}

glm::vec3 AttractorTimelineScene::mapState(const glm::vec3& state) const
{
    // Swap y/z so the classic "butterfly" stands upright, then scale and centre.
    return glm::vec3(state.x, state.z, state.y) * m_scale + m_center;
}

void AttractorTimelineScene::stepAttractor()
{
    const float h = m_stepSize;
    const float dx = m_sigma * (m_state.y - m_state.x);
    const float dy = m_state.x * (m_rho - m_state.z) - m_state.y;
    const float dz = m_state.x * m_state.y - m_beta * m_state.z;
    m_state.x += dx * h;
    m_state.y += dy * h;
    m_state.z += dz * h;
}

void AttractorTimelineScene::emitAlongPath(int steps)
{
    if (!m_ps || steps <= 0)
        return;

    // One additive sprite per integration point, placed exactly on the path so
    // the trail is continuous regardless of how fast the head is moving.
    for (int i = 0; i < steps; ++i) {
        stepAttractor();
        m_ps->setEmitFromPoint(mapState(m_state));
        m_ps->emit(1);
    }
}

void AttractorTimelineScene::setupSceneGraph()
{
    m_root = CompositeNode::create();

    auto texture = Texture::create();
    texture->load(findAsset("assets/circle.png").string());

    // Capacity must cover max speed * max lifetime with headroom.
    m_ps = ParticleSystem::create(60000);
    m_ps->setEmitRate(0.0f); // manual emission only — we emit along the path
    m_ps->setLifetime(m_trailSeconds * 0.85f, m_trailSeconds);
    m_ps->setGravity(glm::vec3(0.0f));
    m_ps->setInitialVelocity(glm::vec3(-0.08f), glm::vec3(0.08f));
    m_ps->setBillboard(true);
    m_ps->setTexture(texture);
    applyParticleAppearance(0.0f, 0.0f);
    applyFogSettings();
    m_ps->start();

    // Warm-up: integrate past the transient and pre-seed a full trail so the
    // attractor is already drawn on the first visible frame.
    m_state = glm::vec3(0.1f, 0.0f, 0.0f);
    for (int i = 0; i < 200; ++i)
        stepAttractor();
    emitAlongPath(static_cast<int>(m_speed * m_trailSeconds));

    m_root->add(m_ps);
}

void AttractorTimelineScene::update(double localTime, double deltaTime)
{
    const auto dt = static_cast<float>(deltaTime);
    const auto t = static_cast<float>(localTime);

    updateAudioReaction(dt);

    const float pulse = m_audioLevel * m_audioImpact;
    const float speedFactor = 1.0f + m_audioSpeedBoost * pulse;
    int steps = static_cast<int>(std::lround(m_speed * speedFactor * dt));
    steps = std::clamp(steps, 0, k_maxStepsPerFrame);

    emitAlongPath(steps);
    m_ps->update(dt);

    updateCameraMotion(t);
}

void AttractorTimelineScene::updateAudioReaction(float dt)
{
    const float targetLevel = m_audioInputPlaying ? m_audioInputLevel * m_audioSensitivity : 0.0f;
    const float smoothing = 1.0f - std::exp(-(std::max)(dt, 1.0f / 120.0f) * 12.0f);
    m_audioLevel += (targetLevel - m_audioLevel) * smoothing;

    const float pulse = m_audioLevel * m_audioImpact;
    const float bright = std::clamp(pulse, 0.0f, 1.0f);
    applyParticleAppearance(pulse, bright);
}

void AttractorTimelineScene::updateCameraMotion(float t)
{
    const float angle = t * m_cameraOrbitSpeed;
    const float height = m_cameraHeight + std::sin(t * m_cameraUndulationSpeed) * m_cameraUndulation;
    const glm::vec3 position(std::sin(angle) * m_cameraRadius, height, std::cos(angle) * m_cameraRadius);

    camera(TimelineCamera::lookAt(position, glm::vec3(0.0f)));
}

void AttractorTimelineScene::applyParticleAppearance(float pulse, float bright)
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

void AttractorTimelineScene::applyFogSettings()
{
    if (!m_ps)
        return;

    m_ps->setDepthFog(m_depthFog);
    m_ps->setFogRange(m_fogNear, m_fogFar);
    m_ps->setFogColor(m_fogColor);
}

void AttractorTimelineScene::setAudioInput(float level, bool playing)
{
    m_audioInputLevel = std::clamp(level, 0.0f, 1.0f);
    m_audioInputPlaying = playing;
}

void AttractorTimelineScene::drawControls()
{
    ImGui::SetNextWindowSize({300, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({600, 10}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Lorenz Drift");

    ImGui::Text("Alive : %d / %d", m_ps->aliveCount(), m_ps->maxParticles());
    ImGui::Text("Time  : %.2f s", TimeController::instance()->elapsed());

    ImGui::Separator();
    ImGui::TextUnformatted("Attractor");
    ImGui::SliderFloat("Sigma", &m_sigma, 1.0f, 20.0f);
    ImGui::SliderFloat("Rho", &m_rho, 1.0f, 60.0f);
    ImGui::SliderFloat("Beta", &m_beta, 0.5f, 5.0f);
    ImGui::SliderFloat("Speed", &m_speed, 50.0f, 2000.0f);
    ImGui::SliderFloat("Scale", &m_scale, 0.05f, 0.8f);

    ImGui::Separator();
    ImGui::TextUnformatted("Trail appearance");
    bool appearanceChanged = false;
    appearanceChanged |= ImGui::SliderFloat("Trail length (s)", &m_trailSeconds, 0.5f, 12.0f);
    appearanceChanged |= ImGui::SliderFloat("Start size", &m_startSize, 0.01f, 4.0f);
    appearanceChanged |= ImGui::SliderFloat("End size", &m_endSize, 0.0f, 4.0f);
    appearanceChanged |= ImGui::ColorEdit4("Start color", &m_startColor.x);
    appearanceChanged |= ImGui::ColorEdit4("End color", &m_endColor.x);
    if (appearanceChanged) {
        m_ps->setLifetime(m_trailSeconds * 0.85f, m_trailSeconds);
        applyParticleAppearance(m_audioLevel * m_audioImpact, std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Depth fog");
    bool fogChanged = false;
    fogChanged |= ImGui::Checkbox("Enabled##lorenzFog", &m_depthFog);
    fogChanged |= ImGui::SliderFloat("Fog near##lorenz", &m_fogNear, 0.0f, 60.0f);
    fogChanged |= ImGui::SliderFloat("Fog far##lorenz", &m_fogFar, 1.0f, 120.0f);
    fogChanged |= ImGui::ColorEdit3("Fog color##lorenz", &m_fogColor.x);
    if (fogChanged) {
        m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);
        applyFogSettings();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Camera");
    ImGui::SliderFloat("Orbit radius", &m_cameraRadius, 8.0f, 60.0f);
    ImGui::SliderFloat("Camera height", &m_cameraHeight, 0.0f, 24.0f);
    ImGui::SliderFloat("Undulation", &m_cameraUndulation, 0.0f, 10.0f);
    ImGui::SliderFloat("Orbit speed", &m_cameraOrbitSpeed, 0.0f, 0.5f);

    ImGui::Separator();
    ImGui::TextUnformatted("Audio reaction");
    ImGui::ProgressBar(m_audioLevel, {-1.0f, 0.0f}, "Energy");
    ImGui::SliderFloat("Sensitivity##lorenz", &m_audioSensitivity, 0.2f, 5.0f);
    ImGui::SliderFloat("Impact##lorenz", &m_audioImpact, 0.0f, 3.0f);
    ImGui::SliderFloat("Speed boost", &m_audioSpeedBoost, 0.0f, 4.0f);
    ImGui::SliderFloat("Start size boost##lorenz", &m_audioStartSizeBoost, 0.0f, 4.0f);
    ImGui::SliderFloat("End size boost##lorenz", &m_audioEndSizeBoost, 0.0f, 4.0f);
    ImGui::ColorEdit4("Start color boost##lorenz", &m_startColorAudioBoost.x);
    ImGui::ColorEdit4("End color boost##lorenz", &m_endColorAudioBoost.x);

    ImGui::End();
}

nlohmann::json AttractorTimelineScene::toJson() const
{
    return {
        {"sigma", m_sigma},
        {"rho", m_rho},
        {"beta", m_beta},
        {"speed", m_speed},
        {"scale", m_scale},
        {"trailSeconds", m_trailSeconds},
        {"startSize", m_startSize},
        {"endSize", m_endSize},
        {"startColor", {m_startColor.r, m_startColor.g, m_startColor.b, m_startColor.a}},
        {"endColor", {m_endColor.r, m_endColor.g, m_endColor.b, m_endColor.a}},
        {"startColorAudioBoost", {m_startColorAudioBoost.r, m_startColorAudioBoost.g, m_startColorAudioBoost.b, m_startColorAudioBoost.a}},
        {"endColorAudioBoost", {m_endColorAudioBoost.r, m_endColorAudioBoost.g, m_endColorAudioBoost.b, m_endColorAudioBoost.a}},
        {"depthFog", m_depthFog},
        {"fogNear", m_fogNear},
        {"fogFar", m_fogFar},
        {"fogColor", {m_fogColor.r, m_fogColor.g, m_fogColor.b}},
        {"audioSensitivity", m_audioSensitivity},
        {"audioImpact", m_audioImpact},
        {"audioSpeedBoost", m_audioSpeedBoost},
        {"audioStartSizeBoost", m_audioStartSizeBoost},
        {"audioEndSizeBoost", m_audioEndSizeBoost},
        {"cameraRadius", m_cameraRadius},
        {"cameraHeight", m_cameraHeight},
        {"cameraUndulation", m_cameraUndulation},
        {"cameraOrbitSpeed", m_cameraOrbitSpeed},
        {"cameraUndulationSpeed", m_cameraUndulationSpeed}
    };
}

void AttractorTimelineScene::fromJson(const nlohmann::json& json)
{
    if (!json.is_object())
        return;

    readIfPresent(json, "sigma", m_sigma);
    readIfPresent(json, "rho", m_rho);
    readIfPresent(json, "beta", m_beta);
    readIfPresent(json, "speed", m_speed);
    readIfPresent(json, "scale", m_scale);
    readIfPresent(json, "trailSeconds", m_trailSeconds);
    readIfPresent(json, "startSize", m_startSize);
    readIfPresent(json, "endSize", m_endSize);
    readVec4IfPresent(json, "startColor", m_startColor);
    readVec4IfPresent(json, "endColor", m_endColor);
    readVec4IfPresent(json, "startColorAudioBoost", m_startColorAudioBoost);
    readVec4IfPresent(json, "endColorAudioBoost", m_endColorAudioBoost);
    readIfPresent(json, "depthFog", m_depthFog);
    readIfPresent(json, "fogNear", m_fogNear);
    readIfPresent(json, "fogFar", m_fogFar);
    readVec3IfPresent(json, "fogColor", m_fogColor);
    readIfPresent(json, "audioSensitivity", m_audioSensitivity);
    readIfPresent(json, "audioImpact", m_audioImpact);
    readIfPresent(json, "audioSpeedBoost", m_audioSpeedBoost);
    readIfPresent(json, "audioStartSizeBoost", m_audioStartSizeBoost);
    readIfPresent(json, "audioEndSizeBoost", m_audioEndSizeBoost);
    readIfPresent(json, "cameraRadius", m_cameraRadius);
    readIfPresent(json, "cameraHeight", m_cameraHeight);
    readIfPresent(json, "cameraUndulation", m_cameraUndulation);
    readIfPresent(json, "cameraOrbitSpeed", m_cameraOrbitSpeed);
    readIfPresent(json, "cameraUndulationSpeed", m_cameraUndulationSpeed);

    m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);

    if (m_ps) {
        m_ps->setLifetime(m_trailSeconds * 0.85f, m_trailSeconds);
        applyParticleAppearance(m_audioLevel * m_audioImpact, std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));
        applyFogSettings();
    }
}

std::filesystem::path AttractorTimelineScene::findAsset(const std::filesystem::path& relativePath) const
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
