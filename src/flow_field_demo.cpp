/**
 * @file flow_field_demo.cpp
 * @brief Timeline-driven flow-field particle demo.
 *
 * A thin example of building on CreativeApplicationWindow: the base window supplies the effect
 * chain, timeline playback, fades, audio, MIDI, control panels, and performance mode; this app
 * only creates its scenes and arranges them on the timeline.
 *
 * Controls:
 *   Space  - pause / resume global time and audio
 *   F11    - performance mode (borderless fullscreen)
 *   Tab/H  - hide UI
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "creative_app.h"
#include "particle_scene.h"
#include "tunnel_scene.h"
#include "lorenz_scene.h"
#include "lattice_scene.h"

#include <ivfui/glfw_application.h>

#include <memory>
#include <string>

using namespace ivfui;

class FlowFieldApp : public CreativeApplicationWindow {
private:
    std::shared_ptr<ParticleTimelineScene> m_particleScene;

public:
    FlowFieldApp(int w, int h, std::string title) : CreativeApplicationWindow(w, h, title) {}

    static std::shared_ptr<FlowFieldApp> create(int w, int h, std::string title)
    {
        return std::make_shared<FlowFieldApp>(w, h, title);
    }

protected:
    void onRegisterScenes() override
    {
        setAudioAsset("assets/slow_beats.wav");
        setTimelineLoop(true);

        // Scene boundaries (seconds) matched to the audio track.
        const double t0 = 0.0;
        const double t1 = 60.0 + 25.201;
        const double t2 = 2 * 60.0 + 8.004;
        const double t3 = 2 * 60.0 + 29.352;
        const double t4 = 3 * 60.0 + 11.972;
        const double t5 = 3 * 60.0 + 55.0;

        m_particleScene = ParticleTimelineScene::create();

        addScene(m_particleScene, t1 - t0);
        addScene(TunnelTimelineScene::create(), t2 - t1);
        addScene(AttractorTimelineScene::create(), t3 - t2);
        addScene(LatticeTimelineScene::create(), t4 - t3);
        addScene(m_particleScene, t5 - t4); // closing reprise of the flow field
    }
};

int main()
{
    auto app = GLFWApplication::create();
    app->hint(GLFW_SAMPLES, 4);

    auto win = FlowFieldApp::create(1280, 720, "Flow Field - timeline particles");
    app->addWindow(win);

    return app->loop();
}
