/**
 * @file ivf_app.cpp
 * @brief Timeline-driven flow-field particle example.
 *
 * Controls:
 *   Space  - pause / resume global time and audio
 *   ESC    - quit
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "audio_player.h"
#include "particle_scene.h"
#include "tunnel_scene.h"
#include "lorenz_scene.h"
#include "midi_binder.h"

#include <ivf/gl.h>
#include <ivf/nodes.h>
#include <ivf/scene_timeline.h>
#include <ivf/midi_controller.h>
#include <ivfui/scene_timeline_player.h>
#include <ivfui/time_control_panel.h>
#include <ivfui/ui.h>

#include <ivf/tint_effect.h>
#include <ivf/filmgrain_effect.h>
#include <ivf/chromatic_effect.h>
#include <ivf/vignette_effect.h>
#include <ivf/bloom_effect.h>
#include <ivf/dithering_effect.h>
#include <ivf/pixelation_effect.h>
#include <ivf/wave_distortion_effect.h>
#include <ivf/swirl_effect.h>
#include <ivf/kaleidoscope_effect.h>
#include <ivf/glitch_effect.h>
#include <ivf/scanline_effect.h>
#include <ivf/posterize_effect.h>
#include <ivf/color_grading_effect.h>
#include <ivf/night_vision_effect.h>
#include <ivf/blur_effect.h>
#include <ivf/motion_blur_effect.h>
#include <ivf/feedback_effect.h>
#include <ivf/halftone_effect.h>
#include <ivf/fade_effect.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace ivf;
using namespace ivfui;

class ExampleWindow : public GLFWSceneWindow {
private:
    SceneTimelinePtr m_timeline;
    SceneTimelinePlayerPtr m_timelinePlayer;
    std::shared_ptr<ParticleTimelineScene> m_particleScene;
    std::shared_ptr<TunnelTimelineScene> m_tunnelScene;
    std::shared_ptr<AttractorTimelineScene> m_attractorScene;
    FadeEffectPtr m_fadeEffect;
    double m_fadeDuration{3.0}; // seconds of fade-in and fade-out per scene
    AudioPlayer m_audioPlayer;
    std::filesystem::path m_settingsPath{"scene_settings.json"};

    MidiControllerPtr m_midi;
    MidiParameterBinder m_midiBinder;
    std::filesystem::path m_midiMappingPath{"midi_mappings.json"};
    int m_midiPort{-1};

public:
    ExampleWindow(int w, int h, std::string title) : GLFWSceneWindow(w, h, title) {}

    static std::shared_ptr<ExampleWindow> create(int w, int h, std::string title)
    {
        return std::make_shared<ExampleWindow>(w, h, title);
    }

    int onSetup() override
    {
        setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        enableHeadlight();
        this->setRenderToTexture(true);

        // Create and configure post-processing effects

        // Blur effect

        auto blurEffect = BlurEffect::create();
        blurEffect->setBlurRadius(2.0);
        blurEffect->load();

        // Tint effect

        auto tintEffect = TintEffect::create();

        tintEffect->setTintColor(glm::vec3(1.2, 0.9, 0.7));
        tintEffect->setTintStrength(0.5);
        tintEffect->setGrayScaleWeights(glm::vec3(0.299, 0.587, 0.114));
        tintEffect->load();

        // Film grain effect

        auto filmgrainEffect = FilmgrainEffect::create();
        filmgrainEffect->setNoiseIntensity(0.5);
        filmgrainEffect->setGrainBlending(0.5);
        filmgrainEffect->load();

        // Chromatic aberration effect

        auto chromaticEffect = ChromaticEffect::create();
        chromaticEffect->setOffset(0.01);
        chromaticEffect->load();

        // Vignette effect

        auto vignetteEffect = VignetteEffect::create();
        vignetteEffect->setSize(1.0);
        vignetteEffect->setSmoothness(0.7);
        vignetteEffect->load();

        // Bloom effect

        auto bloomEffect = BloomEffect::create();
        bloomEffect->setThreshold(1.0);
        bloomEffect->setIntensity(1.0);
        bloomEffect->load();

        // Dithering effect

        auto ditheringEffect = DitheringEffect::create();
        ditheringEffect->load();

        // Pixelation effect

        auto pixelationEffect = PixelationEffect::create();
        pixelationEffect->setPixelSize(4.0);
        pixelationEffect->load();

        // Wave distortion effect

        auto waveDistortionEffect = WaveDistortionEffect::create();
        waveDistortionEffect->setFrequency(10.0f);
        waveDistortionEffect->setAmplitude(0.01f);
        waveDistortionEffect->setSpeed(1.0f);
        waveDistortionEffect->load();

        // Swirl effect

        auto swirlEffect = SwirlEffect::create();
        swirlEffect->setRadius(0.5f);
        swirlEffect->setAngle(3.14159f);
        swirlEffect->load();

        // Kaleidoscope effect

        auto kaleidoscopeEffect = KaleidoscopeEffect::create();
        kaleidoscopeEffect->setSegments(6.0f);
        kaleidoscopeEffect->setRotation(0.0f);
        kaleidoscopeEffect->load();

        // Glitch effect

        auto glitchEffect = GlitchEffect::create();
        glitchEffect->setIntensity(0.05f);
        glitchEffect->setBlockSize(0.05f);
        glitchEffect->setSpeed(4.0f);
        glitchEffect->load();

        // Scanline effect

        auto scanlineEffect = ScanlineEffect::create();
        scanlineEffect->setLineSpacing(4.0f);
        scanlineEffect->setLineIntensity(0.4f);
        scanlineEffect->setScrollSpeed(0.0f);
        scanlineEffect->load();

        // Posterize effect

        auto posterizeEffect = PosterizeEffect::create();
        posterizeEffect->setLevels(4.0f);
        posterizeEffect->load();

        // Color grading effect

        auto colorGradingEffect = ColorGradingEffect::create();
        colorGradingEffect->setShadows(glm::vec3(0.6f, 0.5f, 0.5f));
        colorGradingEffect->setMidtones(glm::vec3(0.5f, 0.5f, 0.5f));
        colorGradingEffect->setHighlights(glm::vec3(0.5f, 0.5f, 0.6f));
        colorGradingEffect->setContrast(1.1f);
        colorGradingEffect->setSaturation(1.2f);
        colorGradingEffect->load();

        // Night vision effect

        auto nightVisionEffect = NightVisionEffect::create();
        nightVisionEffect->setNoiseIntensity(0.05f);
        nightVisionEffect->setGlowStrength(0.3f);
        nightVisionEffect->setPhosphorColor(glm::vec3(0.1f, 1.0f, 0.1f));
        nightVisionEffect->load();

		// Motion blur effect
		
        auto motionBlurEffect = MotionBlurEffect::create();
		motionBlurEffect->load();

		// Feedback effect

		auto feedbackEffect = FeedbackEffect::create();
		feedbackEffect->setFeedbackAmount(0.9f);
		feedbackEffect->load();

		// Halftone effect

		auto halftoneEffect = HalftoneEffect::create();
		halftoneEffect->load();

		// Name effects so MIDI bindings have stable, human-readable target ids.

		blurEffect->setName("Blur");
		tintEffect->setName("Tint");
		chromaticEffect->setName("Chromatic");
		ditheringEffect->setName("Dithering");
		bloomEffect->setName("Bloom");
		filmgrainEffect->setName("Film grain");
		glitchEffect->setName("Glitch");
		scanlineEffect->setName("Scanline");
		posterizeEffect->setName("Posterize");
		colorGradingEffect->setName("Color grading");
		motionBlurEffect->setName("Motion blur");
		feedbackEffect->setName("Feedback");
		halftoneEffect->setName("Halftone");

		m_particleScene = ParticleTimelineScene::create();
        m_tunnelScene = TunnelTimelineScene::create();
        m_attractorScene = AttractorTimelineScene::create();

		// Add effects to the timeline scene (SceneTimelinePlayer will apply scene effects)

		m_particleScene->effect(blurEffect);            //  0
		m_particleScene->effect(tintEffect);            //  1
		m_particleScene->effect(chromaticEffect);       //  2
		m_particleScene->effect(ditheringEffect);       //  3
		m_particleScene->effect(bloomEffect);           //  4
		m_particleScene->effect(filmgrainEffect);       //  7
		m_particleScene->effect(glitchEffect);          // 11
		m_particleScene->effect(scanlineEffect);        // 12
		m_particleScene->effect(posterizeEffect);       // 13
		m_particleScene->effect(colorGradingEffect);    // 14
        m_particleScene->effect(motionBlurEffect);      // 16
		m_particleScene->effect(feedbackEffect);        // 17
		m_particleScene->effect(halftoneEffect);         // 18
		// Disable all effects by default

		for (int i = 0; i < static_cast<int>(m_particleScene->effects().size()); ++i)
			m_particleScene->effects()[static_cast<size_t>(i)]->program()->setEnabled(false);

        m_tunnelScene->effect(blurEffect);            //  0
        m_tunnelScene->effect(tintEffect);            //  1
        m_tunnelScene->effect(chromaticEffect);       //  2
        m_tunnelScene->effect(ditheringEffect);       //  3
        m_tunnelScene->effect(bloomEffect);           //  4
        m_tunnelScene->effect(filmgrainEffect);       //  7
        m_tunnelScene->effect(glitchEffect);          // 11
        m_tunnelScene->effect(scanlineEffect);        // 12
        m_tunnelScene->effect(posterizeEffect);       // 13
        m_tunnelScene->effect(colorGradingEffect);    // 14
        m_tunnelScene->effect(motionBlurEffect);      // 16
        m_tunnelScene->effect(feedbackEffect);        // 17
        m_tunnelScene->effect(halftoneEffect);         // 18
        // Disable all effects by default

        for (int i = 0; i < static_cast<int>(m_tunnelScene->effects().size()); ++i)
            m_tunnelScene->effects()[static_cast<size_t>(i)]->program()->setEnabled(false);

        m_attractorScene->effect(blurEffect);            //  0
        m_attractorScene->effect(tintEffect);            //  1
        m_attractorScene->effect(chromaticEffect);       //  2
        m_attractorScene->effect(ditheringEffect);       //  3
        m_attractorScene->effect(bloomEffect);           //  4
        m_attractorScene->effect(filmgrainEffect);       //  7
        m_attractorScene->effect(glitchEffect);          // 11
        m_attractorScene->effect(scanlineEffect);        // 12
        m_attractorScene->effect(posterizeEffect);       // 13
        m_attractorScene->effect(colorGradingEffect);    // 14
        m_attractorScene->effect(motionBlurEffect);      // 16
        m_attractorScene->effect(feedbackEffect);        // 17
        m_attractorScene->effect(halftoneEffect);         // 18
        // Disable all effects by default

        for (int i = 0; i < static_cast<int>(m_attractorScene->effects().size()); ++i)
            m_attractorScene->effects()[static_cast<size_t>(i)]->program()->setEnabled(false);

        // Fade transition effect (dip-to-black). Added last so it is applied on top of
        // every other effect, and kept enabled so scene fade-in / fade-out is always active.

        m_fadeEffect = FadeEffect::create();
        m_fadeEffect->setFadeColor(glm::vec3(0.0f, 0.0f, 0.0f));
        m_fadeEffect->setFadeAmount(0.0f);
        m_fadeEffect->load();

        m_particleScene->effect(m_fadeEffect);
        m_tunnelScene->effect(m_fadeEffect);
        m_attractorScene->effect(m_fadeEffect);

        m_timeline = SceneTimeline::create();
        m_timeline->setLoop(true);
        m_timeline->addScene(m_particleScene->name(), m_particleScene->duration()) = *m_particleScene;
        m_timeline->addScene(m_tunnelScene->name(), m_tunnelScene->duration()) = *m_tunnelScene;
        m_timeline->addScene(m_attractorScene->name(), m_attractorScene->duration()) = *m_attractorScene;
        m_timeline->pause();

        m_timelinePlayer = SceneTimelinePlayer::create(this, m_timeline);
        m_timelinePlayer->update(0.0);

        loadSceneSettings();
        m_audioPlayer.load(findAsset("assets/slow_beats.wav"));

        addUiWindow(TimeControlPanel::create());

        TimeController::instance()->setScale(1.0f);
        setDemoPlaying(false);

        this->showEffectInspector();

        setupMidi();

        return 0;
    }

    void setupMidi()
    {
        // Register MIDI-controllable targets. Scenes and effects both derive from
        // PropertyInspectable, so the binder drives them through one uniform path.
        m_midiBinder.addTarget("scene:" + m_particleScene->name(), "Scene: " + m_particleScene->name(),
                               m_particleScene.get());
        m_midiBinder.addTarget("scene:" + m_tunnelScene->name(), "Scene: " + m_tunnelScene->name(),
                               m_tunnelScene.get());
        m_midiBinder.addTarget("scene:" + m_attractorScene->name(), "Scene: " + m_attractorScene->name(),
                               m_attractorScene.get());

        // Effect objects are shared across scenes; registering them once is enough.
        for (const auto& effect : m_particleScene->effects()) {
            if (!effect || effect->name().empty())
                continue;
            m_midiBinder.addTarget("effect:" + effect->name(), "Effect: " + effect->name(), effect.get());
        }

        m_midiBinder.load(m_midiMappingPath);

        m_midi = MidiController::create();
        const auto ports = m_midi->listPorts();
        if (!ports.empty())
            if (m_midi->openPort(0))
                m_midiPort = 0;
    }

    void onUpdate() override
    {
        const double dt = TimeController::instance()->delta();

        // Drain MIDI on the main thread and apply CC messages to bound parameters.
        if (m_midi) {
            for (const auto& msg : m_midi->poll())
                if (msg.type == MidiMessage::ControlChange)
                    m_midiBinder.applyCC(msg.channel, msg.data1, msg.data2);
        }

        if (m_particleScene)
            m_particleScene->setAudioInput(m_audioPlayer.energy(), m_audioPlayer.isPlaying());
        if (m_tunnelScene)
            m_tunnelScene->setAudioInput(m_audioPlayer.energy(), m_audioPlayer.isPlaying());
        if (m_attractorScene)
            m_attractorScene->setAudioInput(m_audioPlayer.energy(), m_audioPlayer.isPlaying());

        if (m_timelinePlayer)
            m_timelinePlayer->update(dt);

        updateSceneFade();

        const size_t activeIndex = m_timeline ? m_timeline->activeSceneIndex() : SceneTimeline::npos;
        if (activeIndex == 2 && m_attractorScene)
            applyCamera(m_attractorScene->camera());
        else if (activeIndex == 1 && m_tunnelScene)
            applyCamera(m_tunnelScene->camera());
        else if (m_particleScene)
            applyCamera(m_particleScene->camera());
    }

    void onKey(int key, int /*sc*/, int action, int /*mods*/) override
    {
        if (action == GLFW_PRESS && key == GLFW_KEY_SPACE)
            setDemoPlaying(!isDemoPlaying());
    }

    void onDrawUi() override
    {
        drawSceneSwitcher();

        const size_t activeIndex = m_timeline ? m_timeline->activeSceneIndex() : SceneTimeline::npos;
        if (activeIndex == 2 && m_attractorScene)
            m_attractorScene->drawControls();
        else if (activeIndex == 1 && m_tunnelScene)
            m_tunnelScene->drawControls();
        else if (m_particleScene)
            m_particleScene->drawControls();

        m_audioPlayer.drawControls();

        drawMidiPortSelector();
        m_midiBinder.drawControls();
    }

	void onClose() override
	{
		saveSceneSettings();
		m_midiBinder.save(m_midiMappingPath);
		if (m_audioPlayer.isPlaying())
			m_audioPlayer.setPaused(true);
    }

private:
    void drawMidiPortSelector()
    {
        if (!m_midi)
            return;

        ImGui::SetNextWindowSize({320, 0}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({960, 320}, ImGuiCond_FirstUseEver);
        ImGui::Begin("MIDI Device");

        const auto ports = m_midi->listPorts();
        if (ports.empty()) {
            ImGui::TextUnformatted("No MIDI input ports found.");
            const std::string err = m_midi->lastError();
            if (!err.empty())
                ImGui::TextColored({1.0f, 0.5f, 0.5f, 1.0f}, "%s", err.c_str());
            ImGui::End();
            return;
        }

        const char* current = (m_midiPort >= 0 && m_midiPort < static_cast<int>(ports.size()))
                                  ? ports[static_cast<size_t>(m_midiPort)].c_str()
                                  : "None";
        if (ImGui::BeginCombo("Input port", current)) {
            for (size_t i = 0; i < ports.size(); ++i) {
                const bool selected = static_cast<int>(i) == m_midiPort;
                if (ImGui::Selectable(ports[i].c_str(), selected)) {
                    if (m_midi->openPort(static_cast<unsigned int>(i)))
                        m_midiPort = static_cast<int>(i);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Status: %s", m_midi->isOpen() ? "connected" : "closed");

        ImGui::End();
    }

    void drawSceneSwitcher()
    {
        if (!m_timeline || m_timeline->sceneCount() == 0)
            return;

        ImGui::SetNextWindowSize({260, 0}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({10, 560}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Scenes");

        const size_t activeIndex = m_timeline->activeSceneIndex();
        const auto* activeScene = m_timeline->activeScene();
        const char* activeName = activeScene ? activeScene->name().c_str() : "None";
        const bool playing = isDemoPlaying();

        if (ImGui::Button(playing ? "Pause demo" : "Play demo"))
            setDemoPlaying(!playing);

        ImGui::SameLine();
        ImGui::TextUnformatted(playing ? "Playing" : "Paused");

        if (ImGui::BeginCombo("Active scene", activeName)) {
            for (size_t i = 0; i < m_timeline->sceneCount(); ++i) {
                const auto* scene = m_timeline->scene(i);
                if (!scene)
                    continue;

                const bool selected = i == activeIndex;
                if (ImGui::Selectable(scene->name().c_str(), selected))
                    m_timeline->seekToScene(i);

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Previous") && m_timeline->sceneCount() > 1)
            m_timeline->previousScene();
        ImGui::SameLine();
        if (ImGui::Button("Next") && m_timeline->sceneCount() > 1)
            m_timeline->nextScene();

        ImGui::Text("Timeline %.1f / %.1f s", m_timeline->time(), m_timeline->duration());

        ImGui::End();
    }

    bool isDemoPlaying() const
    {
        return m_timeline && m_timeline->isPlaying() && !TimeController::instance()->isPaused();
    }

    void setDemoPlaying(bool playing)
    {
        if (m_timeline) {
            if (playing)
                m_timeline->play();
            else
                m_timeline->pause();
        }

        if (playing)
            TimeController::instance()->resume();
        else
            TimeController::instance()->pause();

        m_audioPlayer.setPaused(!playing);
    }

    // Compute the dip-to-black fade amount for the active scene: fade in from black
    // over the first m_fadeDuration seconds, fade out to black over the last
    // m_fadeDuration seconds. At a scene boundary (including the loop wrap) the outgoing
    // fade-out meets the incoming fade-in, producing a smooth dip-to-black transition.
    void updateSceneFade()
    {
        if (!m_fadeEffect || !m_timeline)
            return;

        const auto* scene = m_timeline->activeScene();
        if (!scene) {
            m_fadeEffect->setFadeAmount(0.0f);
            return;
        }

        const double duration = scene->duration();
        const double localTime = m_timeline->localTime();
        const double fadeDuration = std::min(m_fadeDuration, duration * 0.5);

        double amount = 0.0;
        if (fadeDuration > 0.0) {
            if (localTime < fadeDuration)
                amount = 1.0 - localTime / fadeDuration; // fade in from black
            else if (localTime > duration - fadeDuration)
                amount = (localTime - (duration - fadeDuration)) / fadeDuration; // fade out to black
        }

        m_fadeEffect->setFadeAmount(static_cast<float>(std::clamp(amount, 0.0, 1.0)));
    }

    void applyCamera(const TimelineCamera& camera)
    {
        auto manipulator = cameraManipulator();
        if (!manipulator)
            return;

        manipulator->setCameraPosition(camera.position);
        manipulator->setCameraTarget(camera.target);
        manipulator->setFov(camera.fov);
        manipulator->setNearZ(camera.nearZ);
        manipulator->setFarZ(camera.farZ);
    }

    void loadSceneSettings()
    {
        if (!m_particleScene && !m_tunnelScene)
            return;

        std::ifstream in(m_settingsPath);
        if (!in)
            return;

        nlohmann::json json;
        in >> json;

        if (const auto it = json.find(m_particleScene->name()); it != json.end())
            m_particleScene->fromJson(*it);
        if (m_tunnelScene) {
            if (const auto it = json.find(m_tunnelScene->name()); it != json.end())
                m_tunnelScene->fromJson(*it);
        }
        if (m_attractorScene) {
            if (const auto it = json.find(m_attractorScene->name()); it != json.end())
                m_attractorScene->fromJson(*it);
        }
    }

    void saveSceneSettings()
    {
        if (!m_particleScene && !m_tunnelScene)
            return;

        nlohmann::json json;

        std::ifstream in(m_settingsPath);
        if (in)
            in >> json;

        if (!json.is_object())
            json = nlohmann::json::object();

        if (m_particleScene)
            json[m_particleScene->name()] = m_particleScene->toJson();
        if (m_tunnelScene)
            json[m_tunnelScene->name()] = m_tunnelScene->toJson();
        if (m_attractorScene)
            json[m_attractorScene->name()] = m_attractorScene->toJson();

        std::ofstream out(m_settingsPath);
        if (out)
            out << json.dump(4);
    }

    std::filesystem::path findAsset(const std::filesystem::path& relativePath) const
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
};

int main()
{
    auto app = GLFWApplication::create();
    app->hint(GLFW_SAMPLES, 4);

    auto win = ExampleWindow::create(1280, 720, "Flow Field - timeline particles");
    app->addWindow(win);
    //win->maximize();

    return app->loop();
}
