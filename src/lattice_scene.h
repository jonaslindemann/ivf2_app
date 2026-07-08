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
#include <random>
#include <vector>

/**
 * @class LatticeTimelineScene
 * @brief 3D point lattice threaded by random-walk paths, with particles wandering the lines.
 *
 * A regular 3D grid of nodes is generated; several random walks hop between adjacent nodes to
 * trace a network of axis-aligned edges. Nodes and edges are both drawn as glowing billboards
 * (the edges as dense sprite chains, which give real world-space thickness with correct
 * perspective instead of hardware 1px lines), and a swarm of "walkers" random-walks the same
 * edges, leaving a short glowing trail as each moves from node to node. Audio energy drives the
 * walker speed, trail brightness, and glow, matching the bloom + depth-fog look of the other
 * scenes.
 */
class LatticeTimelineScene : public ivf::TimelineScene {
private:
    struct Walker {
        int current{0};   // node the walker is leaving
        int target{0};    // node the walker is heading toward
        int previous{-1}; // node it came from (avoids immediate backtracking)
        float t{0.0f};    // 0..1 progress along the current edge
    };

    ivf::CompositeNodePtr m_root;
    ivf::ParticleSystemPtr m_edgePs;   // persistent sprite chains drawing the walk edges
    ivf::ParticleSystemPtr m_nodePs;   // persistent billboards marking the lattice nodes
    ivf::ParticleSystemPtr m_walkerPs; // short-lived trail sprites for the moving walkers

    // Lattice structure (a change to any of these regenerates the network).
    int m_nx{7};
    int m_ny{5};
    int m_nz{7};
    float m_spacing{4.5f};
    int m_numPaths{18};
    int m_pathLength{26};
    unsigned int m_seed{1337};

    // Edge (line) appearance
    glm::vec4 m_lineColor{0.15f, 0.85f, 1.0f, 0.7f};
    glm::vec4 m_lineColorAudioBoost{0.5f, 0.0f, 0.4f, 0.2f};
    float m_lineThickness{0.35f}; // sprite size along each edge

    // Node appearance
    bool m_showNodes{true};
    float m_nodeSize{0.7f};
    glm::vec4 m_nodeColor{0.4f, 0.8f, 1.0f, 0.7f};
    glm::vec4 m_nodeColorAudioBoost{0.5f, 0.1f, 0.0f, 0.2f};

    // Walkers / particle stream
    int m_numWalkers{140};
    float m_walkerSpeed{6.0f};
    float m_audioSpeedBoost{1.6f};
    float m_trailSeconds{0.55f};
    int m_emitPerFrame{2};

    float m_startSize{0.55f};
    float m_endSize{0.0f};
    float m_audioStartSizeBoost{0.9f};
    float m_audioEndSizeBoost{0.3f};
    glm::vec4 m_startColor{1.0f, 0.35f, 0.9f, 0.9f};
    glm::vec4 m_endColor{0.2f, 0.85f, 1.0f, 0.0f};
    glm::vec4 m_startColorAudioBoost{0.0f, 0.4f, 0.0f, 0.1f};
    glm::vec4 m_endColorAudioBoost{0.6f, 0.0f, 0.0f, 0.0f};

    // Audio
    float m_audioInputLevel{0.0f};
    bool m_audioInputPlaying{false};
    float m_audioLevel{0.0f};
    float m_audioSensitivity{1.6f};
    float m_audioImpact{1.0f};

    // Depth fog
    bool m_depthFog{true};
    float m_fogNear{6.0f};
    float m_fogFar{70.0f};
    glm::vec3 m_fogColor{0.0f, 0.0f, 0.0f};

    // Camera (orbit)
    float m_cameraRadius{36.0f};
    float m_cameraHeight{9.0f};
    float m_cameraUndulation{4.0f};
    float m_cameraOrbitSpeed{0.1f};
    float m_cameraUndulationSpeed{0.2f};
    float m_time{0.0f};

    // Generated topology (not serialized)
    std::vector<glm::vec3> m_nodePos;          // world position per lattice node
    std::vector<glm::vec3> m_edgeVerts;        // flat list of edge endpoint pairs
    std::vector<std::vector<int>> m_adjacency; // walk-edge neighbours per node
    std::vector<int> m_activeNodes;            // nodes that participate in the network
    std::vector<Walker> m_walkers;
    std::mt19937 m_rng;

public:
    LatticeTimelineScene();

    static std::shared_ptr<LatticeTimelineScene> create();

    void drawControlsContent();
    void setAudioInput(float level, bool playing);
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& json);

protected:
    void setupProperties() override;
    void onPropertyChanged(const std::string& propertyName) override;

private:
    void setupSceneGraph();
    void regenerate();
    void buildLattice();
    void buildEdgeParticles();
    void buildNodeParticles();
    void initWalkers();
    int randomNeighbour(int node, int avoid);

    void update(double localTime, double deltaTime);
    void updateAudioReaction(float dt);
    void updateWalkers(float dt);
    void updateCameraMotion(float t);
    void applyWalkerAppearance(float pulse, float bright);
    void applyGlowColors(float bright);
    void applyFogSettings();

    std::filesystem::path findAsset(const std::filesystem::path& relativePath) const;
};

typedef std::shared_ptr<LatticeTimelineScene> LatticeTimelineScenePtr;
