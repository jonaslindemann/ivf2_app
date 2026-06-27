#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "particle_scene.h"

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

// Catmull-Rom spline segment: p1..p2, with p0 and p3 as outer control points
glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1,
                     const glm::vec3& p2, const glm::vec3& p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1)
        + (-p0 + p2) * t
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

}

using namespace ivf;

ParticleTimelineScene::ParticleTimelineScene()
    : TimelineScene("Flow Field", 60.0)
{
    setupSceneGraph();
    buildCameraPath();

    root(m_root);
    camera(TimelineCamera::lookAt(glm::vec3(0.0f, 10.0f, 26.0f), m_cameraTarget));
    onUpdate([this](double localTime, double deltaTime) { update(localTime, deltaTime); });
}

std::shared_ptr<ParticleTimelineScene> ParticleTimelineScene::create()
{
    return std::make_shared<ParticleTimelineScene>();
}

void ParticleTimelineScene::drawControls()
{
    ImGui::SetNextWindowSize({280, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Flow Field");

    bool changed = false;
    changed |= ImGui::SliderFloat("Scale", &m_scale, 0.05f, 2.0f);
    changed |= ImGui::SliderFloat("Strength", &m_strength, 0.1f, 10.0f);
    changed |= ImGui::SliderInt("Octaves", &m_octaves, 1, 4);
    changed |= ImGui::SliderFloat("Time speed", &m_timeScale, 0.0f, 0.5f);
    changed |= ImGui::SliderFloat("Emit rate", &m_baseEmitRate, 100.0f, 2000.0f);

    if (changed) {
        m_field->setScale(m_scale);
        m_field->setOctaves(m_octaves);
        m_field->setTimeScale(m_timeScale);
    }

    ImGui::Separator();
    ImGui::Text("Alive : %d / %d", m_ps->aliveCount(), m_ps->maxParticles());
    ImGui::Text("Time  : %.2f s", TimeController::instance()->elapsed());

    ImGui::Separator();
    ImGui::TextUnformatted("Particle appearance");
    bool appearanceChanged = false;
    appearanceChanged |= ImGui::SliderFloat("Start size", &m_startSize, 0.01f, 6.0f);
    appearanceChanged |= ImGui::SliderFloat("End size", &m_endSize, 0.0f, 6.0f);
    appearanceChanged |= ImGui::ColorEdit4("Start color", &m_startColor.x);
    appearanceChanged |= ImGui::ColorEdit4("End color", &m_endColor.x);
    if (appearanceChanged)
        applyParticleAppearance(m_audioLevel * m_audioImpact, std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));

    ImGui::Separator();
    ImGui::TextUnformatted("Depth fog");
    bool fogChanged = false;
    fogChanged |= ImGui::Checkbox("Enabled", &m_depthFog);
    fogChanged |= ImGui::SliderFloat("Fog near", &m_fogNear, 0.0f, 60.0f);
    fogChanged |= ImGui::SliderFloat("Fog far", &m_fogFar, 1.0f, 120.0f);
    fogChanged |= ImGui::ColorEdit3("Fog color", &m_fogColor.x);
    if (fogChanged) {
        m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);
        applyFogSettings();
    }

    ImGui::Separator();
    ImGui::Checkbox("Path camera", &m_pathCamera);
    if (m_pathCamera) {
        ImGui::SliderFloat("Path speed", &m_pathSpeed, 0.005f, 0.2f);
        ImGui::Text("Path pos: %.2f  dir: %+.0f", m_pathT, m_pathDir);
        if (ImGui::Button("Reset path"))
        {
            m_pathT = 0.0f;
            m_pathDir = 1.0f;
        }
    }
    ImGui::Checkbox("Auto camera", &m_autoCamera);
    if (m_autoCamera) {
        ImGui::SliderFloat("Orbit radius", &m_cameraRadius, 12.0f, 48.0f);
        ImGui::SliderFloat("Camera height", &m_cameraHeight, 2.0f, 24.0f);
        ImGui::SliderFloat("Undulation", &m_cameraUndulation, 0.0f, 10.0f);
        ImGui::SliderFloat("Orbit speed", &m_cameraOrbitSpeed, 0.0f, 0.4f);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Audio reaction");
    ImGui::ProgressBar(m_audioLevel, {-1.0f, 0.0f}, "Energy");
    ImGui::SliderFloat("Sensitivity", &m_audioSensitivity, 0.2f, 4.0f);
    ImGui::SliderFloat("Impact", &m_audioImpact, 0.0f, 2.0f);
    ImGui::SliderFloat("Start size boost", &m_audioStartSizeBoost, 0.0f, 4.0f);
    ImGui::SliderFloat("End size boost", &m_audioEndSizeBoost, 0.0f, 4.0f);
    ImGui::ColorEdit4("Start color boost", &m_startColorAudioBoost.x);
    ImGui::ColorEdit4("End color boost", &m_endColorAudioBoost.x);

    ImGui::End();
}

void ParticleTimelineScene::setAudioInput(float level, bool playing)
{
    m_audioInputLevel = std::clamp(level, 0.0f, 1.0f);
    m_audioInputPlaying = playing;
}

nlohmann::json ParticleTimelineScene::toJson() const
{
    return {
        {"scale", m_scale},
        {"strength", m_strength},
        {"octaves", m_octaves},
        {"timeScale", m_timeScale},
        {"baseEmitRate", m_baseEmitRate},
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
        {"autoCamera", m_autoCamera},
        {"cameraRadius", m_cameraRadius},
        {"cameraHeight", m_cameraHeight},
        {"cameraUndulation", m_cameraUndulation},
        {"cameraOrbitSpeed", m_cameraOrbitSpeed},
        {"cameraUndulationSpeed", m_cameraUndulationSpeed},
        {"pathCamera", m_pathCamera},
        {"pathSpeed", m_pathSpeed},
        {"audioSensitivity", m_audioSensitivity},
        {"audioImpact", m_audioImpact}
    };
}

void ParticleTimelineScene::fromJson(const nlohmann::json& json)
{
    if (!json.is_object())
        return;

    readIfPresent(json, "scale", m_scale);
    readIfPresent(json, "strength", m_strength);
    readIfPresent(json, "octaves", m_octaves);
    readIfPresent(json, "timeScale", m_timeScale);
    readIfPresent(json, "baseEmitRate", m_baseEmitRate);

    readIfPresent(json, "startSize", m_startSize);
    readIfPresent(json, "endSize", m_endSize);
    readIfPresent(json, "audioStartSizeBoost", m_audioStartSizeBoost);
    readIfPresent(json, "audioEndSizeBoost", m_audioEndSizeBoost);

    const auto readVec4 = [](const nlohmann::json& value, glm::vec4& target) {
        if (!value.is_array() || value.size() != 4)
            return;

        target = glm::vec4(
            value[0].get<float>(),
            value[1].get<float>(),
            value[2].get<float>(),
            value[3].get<float>());
    };

    const auto readVec3 = [](const nlohmann::json& value, glm::vec3& target) {
        if (!value.is_array() || value.size() != 3)
            return;

        target = glm::vec3(
            value[0].get<float>(),
            value[1].get<float>(),
            value[2].get<float>());
    };

    if (const auto it = json.find("startColor"); it != json.end())
        readVec4(*it, m_startColor);
    if (const auto it = json.find("endColor"); it != json.end())
        readVec4(*it, m_endColor);
    if (const auto it = json.find("startColorAudioBoost"); it != json.end())
        readVec4(*it, m_startColorAudioBoost);
    if (const auto it = json.find("endColorAudioBoost"); it != json.end())
        readVec4(*it, m_endColorAudioBoost);

    readIfPresent(json, "depthFog", m_depthFog);
    readIfPresent(json, "fogNear", m_fogNear);
    readIfPresent(json, "fogFar", m_fogFar);
    if (const auto it = json.find("fogColor"); it != json.end())
        readVec3(*it, m_fogColor);

    readIfPresent(json, "autoCamera", m_autoCamera);
    readIfPresent(json, "cameraRadius", m_cameraRadius);
    readIfPresent(json, "cameraHeight", m_cameraHeight);
    readIfPresent(json, "cameraUndulation", m_cameraUndulation);
    readIfPresent(json, "cameraOrbitSpeed", m_cameraOrbitSpeed);
    readIfPresent(json, "cameraUndulationSpeed", m_cameraUndulationSpeed);
    readIfPresent(json, "pathCamera", m_pathCamera);
    readIfPresent(json, "pathSpeed", m_pathSpeed);

    readIfPresent(json, "audioSensitivity", m_audioSensitivity);
    readIfPresent(json, "audioImpact", m_audioImpact);

    m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);

    if (m_field) {
        m_field->setScale(m_scale);
        m_field->setStrength(m_strength);
        m_field->setOctaves(m_octaves);
        m_field->setTimeScale(m_timeScale);
    }

    if (m_ps) {
        m_ps->setEmitRate(m_baseEmitRate);
        applyParticleAppearance(m_audioLevel * m_audioImpact, std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));
        applyFogSettings();
    }
}

void ParticleTimelineScene::setupSceneGraph()
{
    m_root = CompositeNode::create();

    auto texture = Texture::create();
    texture->load(findAsset("assets/circle.png").string());

    m_field = FlowField::create();
    m_field->setScale(m_scale);
    m_field->setStrength(m_strength);
    m_field->setOctaves(m_octaves);
    m_field->setTimeScale(m_timeScale);
    m_field->setOffset(glm::vec3(31.4f, 17.3f, 57.1f));

    m_ps = ParticleSystem::create(80000);
    m_ps->setEmitRate(m_baseEmitRate);
    m_ps->setLifetime(6.0f, 12.0f);
    m_ps->setGravity(glm::vec3(0.0f));
    m_ps->setInitialVelocity(glm::vec3(-0.2f, -0.1f, -0.2f), glm::vec3(0.2f, 0.1f, 0.2f));
    m_ps->setEmitFromBox(glm::vec3(-8.0f, -0.5f, -8.0f), glm::vec3(8.0f, 0.5f, 8.0f));
    applyParticleAppearance(0.0f, 0.0f);
    m_ps->setBillboard(true);
    applyFogSettings();
    m_ps->setTexture(texture);
    m_ps->start();
    m_root->add(m_ps);

    m_field->applyToParticleSystem(m_ps, 3.0f);
}

std::filesystem::path ParticleTimelineScene::findAsset(const std::filesystem::path& relativePath) const
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

void ParticleTimelineScene::update(double localTime, double deltaTime)
{
    const auto dt = static_cast<float>(deltaTime);
    const auto t = static_cast<float>(localTime);

    updateAudioReaction(dt);
    updateCameraMotion(t, dt);

    m_field->setTime(t);
    m_ps->update(dt);
}

void ParticleTimelineScene::updateAudioReaction(float dt)
{
    const float targetLevel = m_audioInputPlaying ? m_audioInputLevel * m_audioSensitivity : 0.0f;
    const float smoothing = 1.0f - std::exp(-(std::max)(dt, 1.0f / 120.0f) * 12.0f);
    m_audioLevel += (targetLevel - m_audioLevel) * smoothing;

    const float pulse = m_audioLevel * m_audioImpact;
    const float bright = std::clamp(pulse, 0.0f, 1.0f);

    m_field->setStrength(m_strength * (1.0f + 1.4f * pulse));
    m_ps->setEmitRate(m_baseEmitRate * (1.0f + 1.8f * pulse));
    applyParticleAppearance(pulse, bright);
}

void ParticleTimelineScene::buildCameraPath()
{
    // S-curve through the particle field, weaving between clusters.
    // The field lives in [-8,8] x [-8,8] on X/Z; height varies gently.
    m_pathPoints = {
        // ghost point before start (used by Catmull-Rom p0)
        { 10.0f,  8.0f,  18.0f },
        // --- actual path ---
        {  6.0f,  8.0f,  16.0f },  // entry – high, off to one side
        {  0.0f,  5.0f,  10.0f },  // sweep in towards centre
        { -6.0f,  3.0f,   4.0f },  // through the field, left
        {  5.0f,  2.0f,  -2.0f },  // cross to the right, lower
        { -4.0f,  4.0f,  -8.0f },  // back left, rising
        {  2.0f,  6.0f, -14.0f },  // deeper, right
        {  0.0f,  9.0f, -20.0f },  // far end – pause here
        // ghost point after end (used by Catmull-Rom p3)
        { -2.0f, 11.0f, -24.0f }
    };
}

glm::vec3 ParticleTimelineScene::evalPath(float t) const
{
    // t in [0,1] spans the inner segments (indices 1 .. n-2)
    const int n = static_cast<int>(m_pathPoints.size());
    const int segments = n - 3;  // number of inner segments

    const float scaled = std::clamp(t, 0.0f, 1.0f) * static_cast<float>(segments);
    const int   seg    = std::min(static_cast<int>(scaled), segments - 1);
    const float u      = scaled - static_cast<float>(seg);

    return catmullRom(
        m_pathPoints[static_cast<size_t>(seg)],
        m_pathPoints[static_cast<size_t>(seg + 1)],
        m_pathPoints[static_cast<size_t>(seg + 2)],
        m_pathPoints[static_cast<size_t>(seg + 3)],
        u);
}

void ParticleTimelineScene::updateCameraMotion(float t, float dt)
{
    if (m_pathCamera)
    {
        // Advance along the path
        m_pathT += m_pathDir * m_pathSpeed * dt;
        if (m_pathT >= 1.0f) { m_pathT = 1.0f; m_pathDir = -1.0f; }
        if (m_pathT <= 0.0f) { m_pathT = 0.0f; m_pathDir =  1.0f; }

        const glm::vec3 pos = evalPath(m_pathT);

        // Look slightly ahead in travel direction for a natural feel
        constexpr float lookahead = 0.02f;
        const float tAhead = std::clamp(m_pathT + m_pathDir * lookahead, 0.0f, 1.0f);
        glm::vec3 target = evalPath(tAhead);

        // Avoid gimbal when pos and target collapse
        if (glm::length(target - pos) < 0.01f)
            target = pos + glm::vec3(0.0f, 0.0f, -1.0f);

        camera(TimelineCamera::lookAt(pos, target));
        return;
    }

    if (!m_autoCamera)
        return;

    const float angle = t * m_cameraOrbitSpeed;
    const float height = m_cameraHeight + std::sin(t * m_cameraUndulationSpeed) * m_cameraUndulation;
    const glm::vec3 position(
        std::sin(angle) * m_cameraRadius,
        height,
        std::cos(angle) * m_cameraRadius);

    camera(TimelineCamera::lookAt(position, m_cameraTarget));
}

void ParticleTimelineScene::applyParticleAppearance(float pulse, float bright)
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

void ParticleTimelineScene::applyFogSettings()
{
    if (!m_ps)
        return;

    m_ps->setDepthFog(m_depthFog);
    m_ps->setFogRange(m_fogNear, m_fogFar);
    m_ps->setFogColor(m_fogColor);
}
