#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "lattice_scene.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

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

glm::vec4 clampColor(const glm::vec4& c)
{
    return glm::clamp(c, glm::vec4(0.0f), glm::vec4(1.0f));
}

// Sprites placed along an edge to make it a continuous glowing tube. Bounded so a very thin
// setting can't explode the particle count.
int edgeSampleCount(float length, float thickness)
{
    const float step = (std::max)(thickness * 0.5f, 0.12f);
    const int n = static_cast<int>(std::ceil(length / step)) + 1;
    return std::clamp(n, 2, 64);
}

} // namespace

using namespace ivf;

LatticeTimelineScene::LatticeTimelineScene() : TimelineScene("Lattice Walk", 50.0), m_rng(m_seed)
{
    setupSceneGraph();

    root(m_root);
    camera(TimelineCamera::lookAt(glm::vec3(0.0f, m_cameraHeight, m_cameraRadius), glm::vec3(0.0f)));
    onUpdate([this](double localTime, double deltaTime) { update(localTime, deltaTime); });
}

std::shared_ptr<LatticeTimelineScene> LatticeTimelineScene::create()
{
    return std::make_shared<LatticeTimelineScene>();
}

void LatticeTimelineScene::setupSceneGraph()
{
    m_root = CompositeNode::create();

    auto texture = Texture::create();
    texture->load(findAsset("assets/circle.png").string());

    // Walker trail sprites: manual emission (Lorenz-style) at each walker's position.
    m_walkerPs = ParticleSystem::create(40000);
    m_walkerPs->setEmitRate(0.0f);
    m_walkerPs->setLifetime(m_trailSeconds * 0.6f, m_trailSeconds);
    m_walkerPs->setGravity(glm::vec3(0.0f));
    m_walkerPs->setInitialVelocity(glm::vec3(0.0f), glm::vec3(0.0f));
    m_walkerPs->setBillboard(true);
    m_walkerPs->setTexture(texture);
    applyWalkerAppearance(0.0f, 0.0f);
    m_walkerPs->start();
    m_root->add(m_walkerPs);

    regenerate();
    applyFogSettings();
}

// Rebuild lattice topology, edge/node billboards and walkers from the current parameters.
void LatticeTimelineScene::regenerate()
{
    buildLattice();
    buildEdgeParticles();
    buildNodeParticles();
    initWalkers();

    applyGlowColors(std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));
    applyFogSettings();
}

void LatticeTimelineScene::buildLattice()
{
    // Seed from m_seed so the topology is deterministic: rebuilding after a non-structural
    // parameter change reproduces the same network instead of scrambling it.
    m_rng.seed(m_seed);

    const int nx = (std::max)(1, m_nx);
    const int ny = (std::max)(1, m_ny);
    const int nz = (std::max)(1, m_nz);
    const int nodeCount = nx * ny * nz;

    const auto index = [ny, nz](int ix, int iy, int iz) { return (ix * ny + iy) * nz + iz; };

    // Node world positions, lattice centred on the origin.
    m_nodePos.assign(static_cast<size_t>(nodeCount), glm::vec3(0.0f));
    const glm::vec3 origin(-(nx - 1) * 0.5f * m_spacing, -(ny - 1) * 0.5f * m_spacing,
                           -(nz - 1) * 0.5f * m_spacing);
    for (int ix = 0; ix < nx; ++ix)
        for (int iy = 0; iy < ny; ++iy)
            for (int iz = 0; iz < nz; ++iz)
                m_nodePos[static_cast<size_t>(index(ix, iy, iz))] =
                    origin + glm::vec3(ix * m_spacing, iy * m_spacing, iz * m_spacing);

    // Random-walk paths between adjacent lattice nodes -> a set of unique edges.
    std::set<std::pair<int, int>> edges;
    std::uniform_int_distribution<int> distX(0, nx - 1), distY(0, ny - 1), distZ(0, nz - 1);
    std::uniform_int_distribution<int> distAxis(0, 2);
    std::uniform_int_distribution<int> distDir(0, 1);

    const int paths = (std::max)(1, m_numPaths);
    const int steps = (std::max)(1, m_pathLength);

    for (int p = 0; p < paths; ++p) {
        int cx = distX(m_rng), cy = distY(m_rng), cz = distZ(m_rng);
        int lastAxis = -1, lastDir = 0;

        for (int s = 0; s < steps; ++s) {
            int nxp = cx, nyp = cy, nzp = cz;
            bool moved = false;

            for (int attempt = 0; attempt < 8 && !moved; ++attempt) {
                const int axis = distAxis(m_rng);
                const int dir = distDir(m_rng) == 0 ? -1 : 1;
                // Avoid immediately retracing the previous step when we can.
                if (attempt < 6 && axis == lastAxis && dir == -lastDir)
                    continue;

                int tx = cx, ty = cy, tz = cz;
                if (axis == 0)
                    tx += dir;
                else if (axis == 1)
                    ty += dir;
                else
                    tz += dir;

                if (tx < 0 || tx >= nx || ty < 0 || ty >= ny || tz < 0 || tz >= nz)
                    continue;

                nxp = tx;
                nyp = ty;
                nzp = tz;
                lastAxis = axis;
                lastDir = dir;
                moved = true;
            }

            if (!moved)
                break;

            const int a = index(cx, cy, cz);
            const int b = index(nxp, nyp, nzp);
            edges.insert({(std::min)(a, b), (std::max)(a, b)});
            cx = nxp;
            cy = nyp;
            cz = nzp;
        }
    }

    // Adjacency + a flat endpoint list for the edge sprite chains.
    m_adjacency.assign(static_cast<size_t>(nodeCount), {});
    m_edgeVerts.clear();
    m_edgeVerts.reserve(edges.size() * 2);
    for (const auto& e : edges) {
        m_adjacency[static_cast<size_t>(e.first)].push_back(e.second);
        m_adjacency[static_cast<size_t>(e.second)].push_back(e.first);
        m_edgeVerts.push_back(m_nodePos[static_cast<size_t>(e.first)]);
        m_edgeVerts.push_back(m_nodePos[static_cast<size_t>(e.second)]);
    }

    m_activeNodes.clear();
    for (int i = 0; i < nodeCount; ++i)
        if (!m_adjacency[static_cast<size_t>(i)].empty())
            m_activeNodes.push_back(i);
}

// Draw each walk edge as a dense chain of glowing billboards (world-space sized, so they
// foreshorten with distance like a real tube — no dependency on hardware line width).
void LatticeTimelineScene::buildEdgeParticles()
{
    if (!m_root)
        return;

    if (m_edgePs)
        m_root->remove(m_edgePs);
    m_edgePs.reset();

    if (m_edgeVerts.size() < 2)
        return;

    const int edgeCount = static_cast<int>(m_edgeVerts.size() / 2);
    const int samples = edgeSampleCount(m_spacing, m_lineThickness);

    auto texture = Texture::create();
    texture->load(findAsset("assets/circle.png").string());

    m_edgePs = ParticleSystem::create(edgeCount * samples + 1);
    m_edgePs->setEmitRate(0.0f);
    m_edgePs->setLifetime(1.0e9f, 1.0e9f);
    m_edgePs->setGravity(glm::vec3(0.0f));
    m_edgePs->setInitialVelocity(glm::vec3(0.0f), glm::vec3(0.0f));
    m_edgePs->setBillboard(true);
    m_edgePs->setTexture(texture);
    m_edgePs->setStartColor(m_lineColor);
    m_edgePs->setEndColor(m_lineColor);
    m_edgePs->setStartSize(m_lineThickness);
    m_edgePs->setEndSize(m_lineThickness);
    m_edgePs->start();

    for (int e = 0; e < edgeCount; ++e) {
        const glm::vec3& a = m_edgeVerts[static_cast<size_t>(e * 2)];
        const glm::vec3& b = m_edgeVerts[static_cast<size_t>(e * 2 + 1)];
        for (int i = 0; i < samples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(samples - 1);
            m_edgePs->setEmitFromPoint(glm::mix(a, b, t));
            m_edgePs->emit(1);
        }
    }

    m_root->add(m_edgePs);
}

void LatticeTimelineScene::buildNodeParticles()
{
    if (!m_root)
        return;

    if (m_nodePs)
        m_root->remove(m_nodePs);
    m_nodePs.reset();

    if (!m_showNodes || m_nodePos.empty())
        return;

    auto texture = Texture::create();
    texture->load(findAsset("assets/circle.png").string());

    // Persistent billboards: effectively infinite lifetime, no motion.
    m_nodePs = ParticleSystem::create(static_cast<int>(m_nodePos.size()) + 1);
    m_nodePs->setEmitRate(0.0f);
    m_nodePs->setLifetime(1.0e9f, 1.0e9f);
    m_nodePs->setGravity(glm::vec3(0.0f));
    m_nodePs->setInitialVelocity(glm::vec3(0.0f), glm::vec3(0.0f));
    m_nodePs->setBillboard(true);
    m_nodePs->setTexture(texture);
    m_nodePs->setStartColor(m_nodeColor);
    m_nodePs->setEndColor(m_nodeColor);
    m_nodePs->setStartSize(m_nodeSize);
    m_nodePs->setEndSize(m_nodeSize);
    m_nodePs->start();

    for (const auto& pos : m_nodePos) {
        m_nodePs->setEmitFromPoint(pos);
        m_nodePs->emit(1);
    }

    m_root->add(m_nodePs);
}

void LatticeTimelineScene::initWalkers()
{
    m_walkers.clear();
    if (m_activeNodes.empty())
        return;

    std::uniform_int_distribution<int> distNode(0, static_cast<int>(m_activeNodes.size()) - 1);
    std::uniform_real_distribution<float> distT(0.0f, 1.0f);

    const int count = (std::max)(0, m_numWalkers);
    m_walkers.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Walker w;
        w.current = m_activeNodes[static_cast<size_t>(distNode(m_rng))];
        w.previous = -1;
        w.target = randomNeighbour(w.current, -1);
        w.t = distT(m_rng);
        if (w.target < 0)
            w.target = w.current;
        m_walkers.push_back(w);
    }
}

// Pick a random neighbour of @p node, preferring not to walk straight back to @p avoid.
int LatticeTimelineScene::randomNeighbour(int node, int avoid)
{
    if (node < 0 || node >= static_cast<int>(m_adjacency.size()))
        return -1;

    const auto& neighbours = m_adjacency[static_cast<size_t>(node)];
    if (neighbours.empty())
        return -1;
    if (neighbours.size() == 1)
        return neighbours[0];

    // Gather candidates excluding the node we came from; fall back to all if that empties it.
    int candidates[64];
    int n = 0;
    for (int nb : neighbours) {
        if (nb != avoid && n < 64)
            candidates[n++] = nb;
    }
    if (n == 0) {
        for (int nb : neighbours)
            if (n < 64)
                candidates[n++] = nb;
    }

    std::uniform_int_distribution<int> dist(0, n - 1);
    return candidates[dist(m_rng)];
}

void LatticeTimelineScene::update(double localTime, double deltaTime)
{
    const auto dt = static_cast<float>(deltaTime);
    m_time = static_cast<float>(localTime);

    updateAudioReaction(dt);
    updateWalkers(dt);

    m_walkerPs->update(dt);
    if (m_edgePs)
        m_edgePs->update(dt);
    if (m_nodePs)
        m_nodePs->update(dt);

    updateCameraMotion(m_time);
}

void LatticeTimelineScene::updateAudioReaction(float dt)
{
    const float targetLevel = m_audioInputPlaying ? m_audioInputLevel * m_audioSensitivity : 0.0f;
    const float smoothing = 1.0f - std::exp(-(std::max)(dt, 1.0f / 120.0f) * 14.0f);
    m_audioLevel += (targetLevel - m_audioLevel) * smoothing;

    const float pulse = m_audioLevel * m_audioImpact;
    const float bright = std::clamp(pulse, 0.0f, 1.0f);

    applyWalkerAppearance(pulse, bright);
    applyGlowColors(bright);
}

void LatticeTimelineScene::updateWalkers(float dt)
{
    if (m_walkers.empty() || m_nodePos.empty())
        return;

    const float pulse = m_audioLevel * m_audioImpact;
    const float speed = m_walkerSpeed * (1.0f + m_audioSpeedBoost * pulse);
    const float advance = speed * dt / (std::max)(0.001f, m_spacing);
    const int emitCount = (std::max)(1, m_emitPerFrame);

    for (auto& w : m_walkers) {
        w.t += advance;

        // A walker may cross several nodes in one frame at high speed.
        int guard = 0;
        while (w.t >= 1.0f && guard++ < 64) {
            w.t -= 1.0f;
            const int came = w.current;
            w.current = w.target;
            const int next = randomNeighbour(w.current, came);
            w.previous = came;
            w.target = next >= 0 ? next : came;
        }

        const glm::vec3 pos = glm::mix(m_nodePos[static_cast<size_t>(w.current)],
                                       m_nodePos[static_cast<size_t>(w.target)],
                                       std::clamp(w.t, 0.0f, 1.0f));
        m_walkerPs->setEmitFromPoint(pos);
        m_walkerPs->emit(emitCount);
    }
}

void LatticeTimelineScene::updateCameraMotion(float t)
{
    const float angle = t * m_cameraOrbitSpeed;
    const float height = m_cameraHeight + std::sin(t * m_cameraUndulationSpeed) * m_cameraUndulation;
    const glm::vec3 position(std::sin(angle) * m_cameraRadius, height, std::cos(angle) * m_cameraRadius);

    camera(TimelineCamera::lookAt(position, glm::vec3(0.0f)));
}

void LatticeTimelineScene::applyWalkerAppearance(float pulse, float bright)
{
    if (!m_walkerPs)
        return;

    m_walkerPs->setStartSize((std::max)(0.0f, m_startSize + m_audioStartSizeBoost * pulse));
    m_walkerPs->setEndSize((std::max)(0.0f, m_endSize + m_audioEndSizeBoost * pulse));
    m_walkerPs->setStartColor(clampColor(m_startColor + m_startColorAudioBoost * bright));
    m_walkerPs->setEndColor(clampColor(m_endColor + m_endColorAudioBoost * bright));
}

// Edge and node billboards read their colour live (mix(startColor, endColor, age) each frame),
// so the audio glow can simply update the colour members without rebuilding anything.
void LatticeTimelineScene::applyGlowColors(float bright)
{
    if (m_edgePs) {
        const glm::vec4 c = clampColor(m_lineColor + m_lineColorAudioBoost * bright);
        m_edgePs->setStartColor(c);
        m_edgePs->setEndColor(c);
    }
    if (m_nodePs) {
        const glm::vec4 c = clampColor(m_nodeColor + m_nodeColorAudioBoost * bright);
        m_nodePs->setStartColor(c);
        m_nodePs->setEndColor(c);
    }
}

void LatticeTimelineScene::applyFogSettings()
{
    const auto apply = [this](ParticleSystemPtr ps) {
        if (!ps)
            return;
        ps->setDepthFog(m_depthFog);
        ps->setFogRange(m_fogNear, m_fogFar);
        ps->setFogColor(m_fogColor);
    };
    apply(m_walkerPs);
    apply(m_edgePs);
    apply(m_nodePs);
}

void LatticeTimelineScene::setAudioInput(float level, bool playing)
{
    m_audioInputLevel = std::clamp(level, 0.0f, 1.0f);
    m_audioInputPlaying = playing;
}

void LatticeTimelineScene::drawControlsContent()
{
    ImGui::Text("Nodes : %d   Connected : %d", static_cast<int>(m_nodePos.size()),
                static_cast<int>(m_activeNodes.size()));
    ImGui::Text("Edges : %d   Walkers : %d", static_cast<int>(m_edgeVerts.size() / 2),
                static_cast<int>(m_walkers.size()));
    ImGui::Text("Time  : %.2f s", TimeController::instance()->elapsed());

    ImGui::Separator();
    ImGui::TextUnformatted("Lattice");
    bool structureChanged = false;
    structureChanged |= ImGui::SliderInt("Nodes X", &m_nx, 2, 12);
    structureChanged |= ImGui::SliderInt("Nodes Y", &m_ny, 2, 12);
    structureChanged |= ImGui::SliderInt("Nodes Z", &m_nz, 2, 12);
    structureChanged |= ImGui::SliderFloat("Spacing", &m_spacing, 1.5f, 9.0f);
    structureChanged |= ImGui::SliderInt("Paths", &m_numPaths, 1, 60);
    structureChanged |= ImGui::SliderInt("Path length", &m_pathLength, 2, 80);
    if (ImGui::Button("Reseed")) {
        m_seed = m_rng();
        structureChanged = true;
    }
    if (structureChanged)
        regenerate();

    ImGui::Separator();
    ImGui::TextUnformatted("Network appearance");
    // Thickness resamples the edge sprite chains; colour is applied live by applyGlowColors().
    if (ImGui::SliderFloat("Line thickness", &m_lineThickness, 0.05f, 1.5f))
        buildEdgeParticles();
    ImGui::ColorEdit4("Line color", &m_lineColor.x);
    if (ImGui::Checkbox("Show nodes", &m_showNodes))
        buildNodeParticles();
    bool nodeChanged = false;
    nodeChanged |= ImGui::SliderFloat("Node size", &m_nodeSize, 0.05f, 3.0f);
    nodeChanged |= ImGui::ColorEdit4("Node color", &m_nodeColor.x);
    if (nodeChanged)
        buildNodeParticles();

    ImGui::Separator();
    ImGui::TextUnformatted("Walkers");
    if (ImGui::SliderInt("Count", &m_numWalkers, 0, 400))
        initWalkers();
    ImGui::SliderFloat("Walker speed", &m_walkerSpeed, 0.5f, 24.0f);
    ImGui::SliderInt("Trail density", &m_emitPerFrame, 1, 6);
    bool appearanceChanged = false;
    appearanceChanged |= ImGui::SliderFloat("Trail length (s)", &m_trailSeconds, 0.1f, 2.5f);
    appearanceChanged |= ImGui::SliderFloat("Start size##lattice", &m_startSize, 0.01f, 3.0f);
    appearanceChanged |= ImGui::SliderFloat("End size##lattice", &m_endSize, 0.0f, 3.0f);
    appearanceChanged |= ImGui::ColorEdit4("Start color##lattice", &m_startColor.x);
    appearanceChanged |= ImGui::ColorEdit4("End color##lattice", &m_endColor.x);
    if (appearanceChanged) {
        m_walkerPs->setLifetime(m_trailSeconds * 0.6f, m_trailSeconds);
        applyWalkerAppearance(m_audioLevel * m_audioImpact, std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f));
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Depth fog");
    bool fogChanged = false;
    fogChanged |= ImGui::Checkbox("Enabled##latticeFog", &m_depthFog);
    fogChanged |= ImGui::SliderFloat("Fog near##lattice", &m_fogNear, 0.0f, 60.0f);
    fogChanged |= ImGui::SliderFloat("Fog far##lattice", &m_fogFar, 1.0f, 160.0f);
    fogChanged |= ImGui::ColorEdit3("Fog color##lattice", &m_fogColor.x);
    if (fogChanged) {
        m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);
        applyFogSettings();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Camera");
    ImGui::SliderFloat("Orbit radius", &m_cameraRadius, 10.0f, 80.0f);
    ImGui::SliderFloat("Camera height", &m_cameraHeight, 0.0f, 30.0f);
    ImGui::SliderFloat("Undulation", &m_cameraUndulation, 0.0f, 12.0f);
    ImGui::SliderFloat("Orbit speed", &m_cameraOrbitSpeed, 0.0f, 0.5f);

    ImGui::Separator();
    ImGui::TextUnformatted("Audio reaction");
    ImGui::ProgressBar(m_audioLevel, {-1.0f, 0.0f}, "Energy");
    ImGui::SliderFloat("Sensitivity##lattice", &m_audioSensitivity, 0.2f, 5.0f);
    ImGui::SliderFloat("Impact##lattice", &m_audioImpact, 0.0f, 3.0f);
    ImGui::SliderFloat("Speed boost##lattice", &m_audioSpeedBoost, 0.0f, 4.0f);
    ImGui::SliderFloat("Start size boost##lattice", &m_audioStartSizeBoost, 0.0f, 4.0f);
    ImGui::SliderFloat("End size boost##lattice", &m_audioEndSizeBoost, 0.0f, 4.0f);
    ImGui::ColorEdit4("Start color boost##lattice", &m_startColorAudioBoost.x);
    ImGui::ColorEdit4("Line glow boost##lattice", &m_lineColorAudioBoost.x);
    ImGui::ColorEdit4("Node glow boost##lattice", &m_nodeColorAudioBoost.x);
}

void LatticeTimelineScene::setupProperties()
{
    // Continuous / colour parameters exposed to MIDI (ranges mirror the sliders above).
    addProperty("Spacing", &m_spacing, 1.5, 9.0, "Lattice");
    addPropertyWithRange("Line color", &m_lineColor, 0.0, 1.0, "Network");
    addProperty("Line thickness", &m_lineThickness, 0.05, 1.5, "Network");
    addProperty("Node size", &m_nodeSize, 0.05, 3.0, "Network");
    addPropertyWithRange("Node color", &m_nodeColor, 0.0, 1.0, "Network");

    addProperty("Walker speed", &m_walkerSpeed, 0.5, 24.0, "Walkers");
    addProperty("Trail length", &m_trailSeconds, 0.1, 2.5, "Walkers");
    addProperty("Start size", &m_startSize, 0.01, 3.0, "Walkers");
    addProperty("End size", &m_endSize, 0.0, 3.0, "Walkers");
    addPropertyWithRange("Start color", &m_startColor, 0.0, 1.0, "Walkers");
    addPropertyWithRange("End color", &m_endColor, 0.0, 1.0, "Walkers");

    addProperty("Depth fog", &m_depthFog, "Fog");
    addProperty("Fog near", &m_fogNear, 0.0, 60.0, "Fog");
    addProperty("Fog far", &m_fogFar, 1.0, 160.0, "Fog");

    addProperty("Orbit radius", &m_cameraRadius, 10.0, 80.0, "Camera");
    addProperty("Camera height", &m_cameraHeight, 0.0, 30.0, "Camera");
    addProperty("Undulation", &m_cameraUndulation, 0.0, 12.0, "Camera");
    addProperty("Orbit speed", &m_cameraOrbitSpeed, 0.0, 0.5, "Camera");

    addProperty("Sensitivity", &m_audioSensitivity, 0.2, 5.0, "Audio");
    addProperty("Impact", &m_audioImpact, 0.0, 3.0, "Audio");
    addProperty("Speed boost", &m_audioSpeedBoost, 0.0, 4.0, "Audio");
    addProperty("Start size boost", &m_audioStartSizeBoost, 0.0, 4.0, "Audio");
    addProperty("End size boost", &m_audioEndSizeBoost, 0.0, 4.0, "Audio");
}

void LatticeTimelineScene::onPropertyChanged(const std::string& propertyName)
{
    m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);

    if (!m_walkerPs)
        return;

    // Only rebuild the sprite chains for the properties that move/resize them; everything else
    // is a light-weight re-apply so MIDI tweaks stay smooth. Colours pulse live per frame.
    if (propertyName == "Spacing") {
        buildLattice();
        buildEdgeParticles();
        buildNodeParticles();
    } else if (propertyName == "Line thickness") {
        buildEdgeParticles();
    } else if (propertyName == "Node size") {
        buildNodeParticles();
    }

    const float bright = std::clamp(m_audioLevel * m_audioImpact, 0.0f, 1.0f);
    m_walkerPs->setLifetime(m_trailSeconds * 0.6f, m_trailSeconds);
    applyWalkerAppearance(m_audioLevel * m_audioImpact, bright);
    applyGlowColors(bright);
    applyFogSettings();
}

nlohmann::json LatticeTimelineScene::toJson() const
{
    return {
        {"nx", m_nx},
        {"ny", m_ny},
        {"nz", m_nz},
        {"spacing", m_spacing},
        {"numPaths", m_numPaths},
        {"pathLength", m_pathLength},
        {"seed", m_seed},
        {"lineColor", {m_lineColor.r, m_lineColor.g, m_lineColor.b, m_lineColor.a}},
        {"lineColorAudioBoost", {m_lineColorAudioBoost.r, m_lineColorAudioBoost.g, m_lineColorAudioBoost.b, m_lineColorAudioBoost.a}},
        {"lineThickness", m_lineThickness},
        {"showNodes", m_showNodes},
        {"nodeSize", m_nodeSize},
        {"nodeColor", {m_nodeColor.r, m_nodeColor.g, m_nodeColor.b, m_nodeColor.a}},
        {"nodeColorAudioBoost", {m_nodeColorAudioBoost.r, m_nodeColorAudioBoost.g, m_nodeColorAudioBoost.b, m_nodeColorAudioBoost.a}},
        {"numWalkers", m_numWalkers},
        {"walkerSpeed", m_walkerSpeed},
        {"audioSpeedBoost", m_audioSpeedBoost},
        {"trailSeconds", m_trailSeconds},
        {"emitPerFrame", m_emitPerFrame},
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
        {"cameraRadius", m_cameraRadius},
        {"cameraHeight", m_cameraHeight},
        {"cameraUndulation", m_cameraUndulation},
        {"cameraOrbitSpeed", m_cameraOrbitSpeed},
        {"cameraUndulationSpeed", m_cameraUndulationSpeed},
        {"audioSensitivity", m_audioSensitivity},
        {"audioImpact", m_audioImpact}
    };
}

void LatticeTimelineScene::fromJson(const nlohmann::json& json)
{
    if (!json.is_object())
        return;

    readIfPresent(json, "nx", m_nx);
    readIfPresent(json, "ny", m_ny);
    readIfPresent(json, "nz", m_nz);
    readIfPresent(json, "spacing", m_spacing);
    readIfPresent(json, "numPaths", m_numPaths);
    readIfPresent(json, "pathLength", m_pathLength);
    readIfPresent(json, "seed", m_seed);
    readVec4IfPresent(json, "lineColor", m_lineColor);
    readVec4IfPresent(json, "lineColorAudioBoost", m_lineColorAudioBoost);
    readIfPresent(json, "lineThickness", m_lineThickness);
    readIfPresent(json, "showNodes", m_showNodes);
    readIfPresent(json, "nodeSize", m_nodeSize);
    readVec4IfPresent(json, "nodeColor", m_nodeColor);
    readVec4IfPresent(json, "nodeColorAudioBoost", m_nodeColorAudioBoost);
    readIfPresent(json, "numWalkers", m_numWalkers);
    readIfPresent(json, "walkerSpeed", m_walkerSpeed);
    readIfPresent(json, "audioSpeedBoost", m_audioSpeedBoost);
    readIfPresent(json, "trailSeconds", m_trailSeconds);
    readIfPresent(json, "emitPerFrame", m_emitPerFrame);
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
    readIfPresent(json, "cameraRadius", m_cameraRadius);
    readIfPresent(json, "cameraHeight", m_cameraHeight);
    readIfPresent(json, "cameraUndulation", m_cameraUndulation);
    readIfPresent(json, "cameraOrbitSpeed", m_cameraOrbitSpeed);
    readIfPresent(json, "cameraUndulationSpeed", m_cameraUndulationSpeed);
    readIfPresent(json, "audioSensitivity", m_audioSensitivity);
    readIfPresent(json, "audioImpact", m_audioImpact);

    m_fogFar = (std::max)(m_fogFar, m_fogNear + 0.1f);

    if (m_walkerPs) {
        m_walkerPs->setLifetime(m_trailSeconds * 0.6f, m_trailSeconds);
        regenerate();
    }
}

std::filesystem::path LatticeTimelineScene::findAsset(const std::filesystem::path& relativePath) const
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
