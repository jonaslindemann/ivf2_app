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
#include "lattice_scene.h"
#include "midi_binder.h"
#include "control_center_window.h"
#include "scene_controls_window.h"

#include <ivf/gl.h>
#include <ivf/nodes.h>
#include <ivf/scene_timeline.h>
#include <ivf/midi_controller.h>
#include <ivfui/scene_timeline_player.h>
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
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

using namespace ivf;
using namespace ivfui;

class ExampleWindow : public GLFWSceneWindow {
private:
    SceneTimelinePtr m_timeline;
    SceneTimelinePlayerPtr m_timelinePlayer;
    bool m_repeatScene{false};        // loop the running scene instead of advancing the timeline
    size_t m_repeatSceneIndex{0};     // scene pinned for repeat
    std::shared_ptr<ParticleTimelineScene> m_particleScene;
    std::shared_ptr<TunnelTimelineScene> m_tunnelScene;
    std::shared_ptr<AttractorTimelineScene> m_attractorScene;
    std::shared_ptr<LatticeTimelineScene> m_latticeScene;
    FadeEffectPtr m_fadeEffect;
    double m_fadeDuration{3.0}; // seconds of fade-in and fade-out per scene
    AudioPlayer m_audioPlayer;
    std::filesystem::path m_settingsPath{"scene_settings.json"};

    MidiControllerPtr m_midi;
    MidiParameterBinder m_midiBinder;
    std::filesystem::path m_midiMappingPath{"midi_mappings.json"};
    int m_midiPort{-1};
    bool m_midiOpenInProgress{false}; // a guarded openPort() worker never returned; controller is poisoned
    std::string m_midiStatus;         // last MIDI open result, shown in the MIDI tab

    ControlCenterWindowPtr m_controlCenter;
    SceneControlsWindowPtr m_sceneControls;

    bool m_fullscreen{false};
    bool m_uiHidden{false};
    int m_savedX{0}, m_savedY{0}, m_savedW{0}, m_savedH{0}; // windowed geometry to restore
    int m_displayMonitor{-1}; // fullscreen output monitor index; -1 = auto (monitor under the window)

    // Performance-mode panel relocation (control panels move to the other monitor).
    int m_pendingLayoutFrames{0};
    ImVec2 m_panelTargets[3];
    static constexpr const char* k_panelNames[3] = {"Control Center", "Scene", "Effect Inspector"};

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
        m_latticeScene = LatticeTimelineScene::create();

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

        m_latticeScene->effect(blurEffect);            //  0
        m_latticeScene->effect(tintEffect);            //  1
        m_latticeScene->effect(chromaticEffect);       //  2
        m_latticeScene->effect(ditheringEffect);       //  3
        m_latticeScene->effect(bloomEffect);           //  4
        m_latticeScene->effect(filmgrainEffect);       //  7
        m_latticeScene->effect(glitchEffect);          // 11
        m_latticeScene->effect(scanlineEffect);        // 12
        m_latticeScene->effect(posterizeEffect);       // 13
        m_latticeScene->effect(colorGradingEffect);    // 14
        m_latticeScene->effect(motionBlurEffect);      // 16
        m_latticeScene->effect(feedbackEffect);        // 17
        m_latticeScene->effect(halftoneEffect);         // 18
        // Disable all effects by default

        for (int i = 0; i < static_cast<int>(m_latticeScene->effects().size()); ++i)
            m_latticeScene->effects()[static_cast<size_t>(i)]->program()->setEnabled(false);

        // Fade transition effect (dip-to-black). Added last so it is applied on top of
        // every other effect, and kept enabled so scene fade-in / fade-out is always active.

        m_fadeEffect = FadeEffect::create();
        m_fadeEffect->setFadeColor(glm::vec3(0.0f, 0.0f, 0.0f));
        m_fadeEffect->setFadeAmount(0.0f);
        m_fadeEffect->load();

        auto t0 = 0.0;
        auto t1 = 60.0f + 25.201f;
        auto t2 = 2 * 60.0f + 8.004f;
        auto t3 = 2 * 60.0f + 29.352f;
		auto t4 = 3 * 60.0f + 11.972f;
        auto t5 = 3 * 60.0f + 55.0f;

        m_particleScene->effect(m_fadeEffect);
		m_particleScene->setDuration(t1-t0);
        m_tunnelScene->effect(m_fadeEffect);
        m_tunnelScene->setDuration(t2-t1);
        m_attractorScene->effect(m_fadeEffect);
		m_attractorScene->setDuration(t3 - t2);
        m_latticeScene->effect(m_fadeEffect);
		m_latticeScene->setDuration(t4 - t3);

        m_timeline = SceneTimeline::create();
        m_timeline->setLoop(true);
        m_timeline->addScene(m_particleScene->name(), m_particleScene->duration()) = *m_particleScene;
        m_timeline->addScene(m_tunnelScene->name(), m_tunnelScene->duration()) = *m_tunnelScene;
        m_timeline->addScene(m_attractorScene->name(), m_attractorScene->duration()) = *m_attractorScene;
        m_timeline->addScene(m_latticeScene->name(), m_latticeScene->duration()) = *m_latticeScene;
        m_timeline->addScene(m_particleScene->name(), t5-t4) = *m_particleScene;
        m_timeline->pause();

        m_timelinePlayer = SceneTimelinePlayer::create(this, m_timeline);
        m_timelinePlayer->update(0.0);

        loadSceneSettings();
        m_audioPlayer.load(findAsset("assets/slow_beats.wav"));

        m_controlCenter = ControlCenterWindow::create("Control Center");
        m_controlCenter->setPosition(10, 30);
        m_controlCenter->setSize(360, 320);
        m_controlCenter->transportContent = [this] { drawTransportContent(); };
        m_controlCenter->displayContent = [this] { drawDisplayContent(); };
        m_controlCenter->audioContent = [this] { m_audioPlayer.drawContent(); };
        m_controlCenter->midiContent = [this] { drawMidiContent(); };
        addUiWindow(m_controlCenter);

        m_sceneControls = SceneControlsWindow::create("Scene");
        m_sceneControls->setPosition(380, 30);
        m_sceneControls->setSize(320, 480);
        m_sceneControls->content = [this] { drawActiveSceneContent(); };
        addUiWindow(m_sceneControls);

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
        m_midiBinder.addTarget("scene:" + m_latticeScene->name(), "Scene: " + m_latticeScene->name(),
                               m_latticeScene.get());

        // Effect objects are shared across scenes; registering them once is enough.
        for (const auto& effect : m_particleScene->effects()) {
            if (!effect || effect->name().empty())
                continue;
            m_midiBinder.addTarget("effect:" + effect->name(), "Effect: " + effect->name(), effect.get());
        }

        m_midiBinder.load(m_midiMappingPath);

        m_midi = createMidiGuarded();
        if (!m_midi) {
            // RtMidiIn's WinMM backend queries midiInGetNumDevs() in its constructor, which can
            // hang indefinitely on a wedged MIDI driver. When that happens every RtMidi call
            // (even listing ports) would block, so we disable MIDI for this session entirely.
            m_midiStatus = "MIDI subsystem is not responding; MIDI disabled this session.";
            return;
        }
        autoOpenMidiPort();
    }

    // Construct the MidiController on a detached worker so a hung MIDI driver can't freeze
    // startup. RtMidiIn's constructor probes the subsystem (midiInGetNumDevs) and can block
    // forever; if it does not return in time we abandon the worker and report MIDI unavailable.
    MidiControllerPtr createMidiGuarded(int timeoutMs = 1500)
    {
        auto done = std::make_shared<std::atomic<bool>>(false);
        auto holder = std::make_shared<MidiControllerPtr>();

        std::thread([done, holder] {
            *holder = MidiController::create();
            done->store(true, std::memory_order_release);
        }).detach();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!done->load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() >= deadline)
                return nullptr; // subsystem not responding; worker left detached, holder kept alive by it
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return *holder;
    }

    // Names of MIDI inputs we should not auto-open. Virtual/loopback ports (loopMIDI, ALSA/
    // Windows "MIDI Through") are the usual cause of a blocking openPort(): they are often held
    // open by another application, and RtMidi's WinMM backend can hang instead of failing.
    static bool isVirtualMidiPort(const std::string& name)
    {
        static const char* const patterns[] = {"loopMIDI", "MIDI Through", "Through"};
        for (const char* p : patterns)
            if (name.find(p) != std::string::npos)
                return true;
        return false;
    }

    // Pick a sensible default MIDI input on startup: the first hardware (non-virtual) port,
    // opened through the guarded path so a misbehaving driver can never freeze the app. If it
    // does not respond, we leave the port closed and let the user choose one in the MIDI tab.
    void autoOpenMidiPort()
    {
        if (!m_midi)
            return;

        const auto ports = m_midi->listPorts();
        for (size_t i = 0; i < ports.size(); ++i) {
            if (isVirtualMidiPort(ports[i]))
                continue;
            if (openMidiPortGuarded(static_cast<unsigned int>(i)))
                m_midiPort = static_cast<int>(i);
            // Whether it opened or timed out, don't blindly probe further ports on startup.
            return;
        }
    }

    // Open a MIDI port without letting a stuck driver freeze the caller. RtMidi's WinMM
    // openPort() can block indefinitely on a port another app holds open, so we run it on a
    // detached worker and give up after a short timeout. The worker keeps its own shared_ptr to
    // the controller, so the object stays alive even if we stop waiting. On timeout the
    // controller is left poisoned (m_midiOpenInProgress) and not touched again, since a late
    // completion on the worker would race any further use.
    bool openMidiPortGuarded(unsigned int index, int timeoutMs = 500)
    {
        if (!m_midi || m_midiOpenInProgress)
            return false;

        auto midi = m_midi;
        auto done = std::make_shared<std::atomic<bool>>(false);
        auto ok = std::make_shared<std::atomic<bool>>(false);

        std::thread([midi, done, ok, index] {
            const bool opened = midi->openPort(index);
            ok->store(opened, std::memory_order_release);
            done->store(true, std::memory_order_release);
        }).detach();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!done->load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                m_midiOpenInProgress = true;
                m_midiStatus = "Port " + std::to_string(index) + " is not responding; left closed.";
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        const bool opened = ok->load(std::memory_order_acquire);
        m_midiStatus = opened ? std::string{} : ("Failed to open port " + std::to_string(index));
        return opened;
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
        if (m_latticeScene)
            m_latticeScene->setAudioInput(m_audioPlayer.energy(), m_audioPlayer.isPlaying());

        if (m_timelinePlayer)
            m_timelinePlayer->update(dt);

        // Repeat mode: if the timeline has advanced off the pinned scene, seek back to it so the
        // running scene loops in place instead of moving on to the next one.
        if (m_repeatScene && m_timeline && m_timeline->activeSceneIndex() != m_repeatSceneIndex)
            m_timeline->seekToScene(m_repeatSceneIndex);

        updateSceneFade();

        const size_t activeIndex = m_timeline ? m_timeline->activeSceneIndex() : SceneTimeline::npos;
        if (activeIndex == 3 && m_latticeScene)
            applyCamera(m_latticeScene->camera());
        else if (activeIndex == 2 && m_attractorScene)
            applyCamera(m_attractorScene->camera());
        else if (activeIndex == 1 && m_tunnelScene)
            applyCamera(m_tunnelScene->camera());
        else if (m_particleScene)
            applyCamera(m_particleScene->camera());
    }

    void onKey(int key, int /*sc*/, int action, int /*mods*/) override
    {
        if (action != GLFW_PRESS)
            return;

        if (key == GLFW_KEY_SPACE)
            setDemoPlaying(!isDemoPlaying());
        else if (key == GLFW_KEY_F11)
            toggleFullscreen();
        else if (key == GLFW_KEY_TAB || key == GLFW_KEY_H)
            m_uiHidden = !m_uiHidden;
    }

    // Expose Fullscreen / Hide UI as checkable View-menu entries (called once per menu).
    void onAddMenuItems(UiMenu* menu) override
    {
        if (!menu || menu->name() != "View")
            return;

        menu->addItem(UiMenuItem::create(
            "Performance mode", "F11", [this]() { toggleFullscreen(); }, [this]() { return m_fullscreen; }));
        menu->addItem(UiMenuItem::create(
            "Hide UI", "Tab", [this]() { m_uiHidden = !m_uiHidden; }, [this]() { return m_uiHidden; }));
    }

	void onClose() override
	{
		saveSceneSettings();
		m_midiBinder.save(m_midiMappingPath);
		if (m_audioPlayer.isPlaying())
			m_audioPlayer.setPaused(true);
    }

protected:
    // Clean-output mode: skip all ImGui rendering so only the scene shows. The ImGui frame is
    // still begun/ended by the framework, just with nothing drawn.
    void doDrawUi() override
    {
        // Relocate control panels for performance mode. Runs inside the ImGui frame so
        // SetWindowPos-by-name takes effect (and viewports move the OS window) this frame.
        if (m_pendingLayoutFrames > 0) {
            for (int i = 0; i < 3; ++i)
                ImGui::SetWindowPos(k_panelNames[i], m_panelTargets[i], ImGuiCond_Always);
            --m_pendingLayoutFrames;
        }

        if (m_uiHidden)
            return;
        GLFWSceneWindow::doDrawUi();
    }

private:
    // Pick the monitor the window currently sits on (by window center), for multi-monitor setups.
    GLFWmonitor* monitorForWindow()
    {
        GLFWwindow* win = ref();
        if (!win)
            return glfwGetPrimaryMonitor();

        int wx, wy, ww, wh;
        glfwGetWindowPos(win, &wx, &wy);
        glfwGetWindowSize(win, &ww, &wh);
        const int cx = wx + ww / 2;
        const int cy = wy + wh / 2;

        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        for (int i = 0; i < count; ++i) {
            int mx, my;
            glfwGetMonitorPos(monitors[i], &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            if (!mode)
                continue;
            if (cx >= mx && cx < mx + mode->width && cy >= my && cy < my + mode->height)
                return monitors[i];
        }
        return glfwGetPrimaryMonitor();
    }

    // First monitor that is not 'current', or nullptr if there is only one.
    GLFWmonitor* otherMonitor(GLFWmonitor* current)
    {
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        for (int i = 0; i < count; ++i)
            if (monitors[i] != current)
                return monitors[i];
        return nullptr;
    }

    // Monitor selected for fullscreen output. Honors the user's choice from the Display tab;
    // falls back to the monitor under the window when set to Auto or when the index is stale.
    GLFWmonitor* targetMonitor()
    {
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if (m_displayMonitor >= 0 && m_displayMonitor < count)
            return monitors[m_displayMonitor];
        return monitorForWindow();
    }

    // Schedule the three control panels to stack near the top-left of the given monitor's work area.
    void layoutPanelsOn(GLFWmonitor* monitor)
    {
        if (!monitor)
            return;

        int wax = 0, way = 0, waw = 0, wah = 0;
        glfwGetMonitorWorkarea(monitor, &wax, &way, &waw, &wah);

        for (int i = 0; i < 3; ++i)
            m_panelTargets[i] = ImVec2(static_cast<float>(wax + 20), static_cast<float>(way + 20 + i * 360));
        m_pendingLayoutFrames = 2;
    }

    // Performance mode: borderless fullscreen output on the current monitor, control panels moved
    // to the other monitor, main menu hidden. Toggling off restores the windowed editing layout.
    void toggleFullscreen()
    {
        GLFWwindow* win = ref();
        if (!win)
            return;

        GLFWmonitor* monitor = m_fullscreen ? monitorForWindow() : targetMonitor();

        if (!m_fullscreen) {
            glfwGetWindowPos(win, &m_savedX, &m_savedY);
            glfwGetWindowSize(win, &m_savedW, &m_savedH);

            int mx = 0, my = 0;
            glfwGetMonitorPos(monitor, &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (!mode)
                return;

            glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_FALSE);
            glfwSetWindowMonitor(win, nullptr, mx, my, mode->width, mode->height, GLFW_DONT_CARE);

            // Controls must stay reachable: show panels, hide the menu, move panels to the other
            // monitor. With only one monitor the panels stay on the fullscreen output (fallback).
            hideMainMenu();
            m_uiHidden = false;
            if (m_controlCenter)
                m_controlCenter->show();
            if (m_sceneControls)
                m_sceneControls->show();
            showEffectInspector();
            layoutPanelsOn(otherMonitor(monitor));

            m_fullscreen = true;
        } else {
            glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_TRUE);
            glfwSetWindowMonitor(win, nullptr, m_savedX, m_savedY, m_savedW, m_savedH, GLFW_DONT_CARE);

            // Bring the menu and panels back onto the editing monitor.
            showMainMenu();
            layoutPanelsOn(monitor);

            m_fullscreen = false;
        }
    }

    // Transport tab: demo play/pause, scene selection, timeline readout, and global time scale.
    void drawTransportContent()
    {
        if (!m_timeline || m_timeline->sceneCount() == 0) {
            ImGui::TextUnformatted("No scenes.");
            return;
        }

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
                if (ImGui::Selectable(scene->name().c_str(), selected)) {
                    m_timeline->seekToScene(i);
                    if (m_repeatScene)
                        m_repeatSceneIndex = i;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Previous") && m_timeline->sceneCount() > 1) {
            m_timeline->previousScene();
            if (m_repeatScene)
                m_repeatSceneIndex = m_timeline->activeSceneIndex();
        }
        ImGui::SameLine();
        if (ImGui::Button("Next") && m_timeline->sceneCount() > 1) {
            m_timeline->nextScene();
            if (m_repeatScene)
                m_repeatSceneIndex = m_timeline->activeSceneIndex();
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Repeat scene", &m_repeatScene)) {
            if (m_repeatScene)
                m_repeatSceneIndex = activeIndex;
        }

        ImGui::Text("Timeline %.1f / %.1f s", m_timeline->time(), m_timeline->duration());

        // Global time controls (ported from ivfui::TimeControlPanel).
        ImGui::Separator();
        auto* tc = TimeController::instance();
        float scale = tc->scale();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("Scale", &scale, 0.0f, 4.0f, "%.2fx"))
            tc->setScale(scale);
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
            tc->reset();
        ImGui::Text("Elapsed : %.3f s", tc->elapsed());
    }

    // Display tab: choose the fullscreen output screen and toggle performance mode. In
    // performance mode the main window goes borderless-fullscreen on the selected screen and
    // the control panels move to the other screen.
    void drawDisplayContent()
    {
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);

        // Keep a stale selection from persisting past a monitor unplug.
        if (m_displayMonitor >= count)
            m_displayMonitor = -1;

        const char* preview =
            (m_displayMonitor >= 0 && m_displayMonitor < count)
                ? glfwGetMonitorName(monitors[m_displayMonitor])
                : "Auto (window's screen)";

        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::BeginCombo("Output screen", preview)) {
            if (ImGui::Selectable("Auto (window's screen)", m_displayMonitor < 0))
                m_displayMonitor = -1;
            if (m_displayMonitor < 0)
                ImGui::SetItemDefaultFocus();

            for (int i = 0; i < count; ++i) {
                int mx = 0, my = 0;
                glfwGetMonitorPos(monitors[i], &mx, &my);
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
                const int mw = mode ? mode->width : 0;
                const int mh = mode ? mode->height : 0;

                char label[128];
                std::snprintf(label, sizeof(label), "%d: %s (%dx%d @ %d,%d)", i,
                              glfwGetMonitorName(monitors[i]), mw, mh, mx, my);

                const bool selected = i == m_displayMonitor;
                if (ImGui::Selectable(label, selected))
                    m_displayMonitor = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (count < 2)
            ImGui::TextColored({1.0f, 0.8f, 0.4f, 1.0f},
                               "Only one screen detected; controls stay on the fullscreen output.");

        ImGui::Separator();

        bool performance = m_fullscreen;
        if (ImGui::Checkbox("Performance mode (fullscreen)", &performance))
            toggleFullscreen();
        ImGui::SameLine();
        ImGui::TextDisabled("(F11)");

        ImGui::TextWrapped("Fullscreens the render window on the selected screen and moves these "
                           "control panels to the other screen.");
    }

    // MIDI tab: input-port selection and status, followed by the learn/bindings table.
    void drawMidiContent()
    {
        if (!m_midi) {
            ImGui::TextUnformatted("MIDI not initialized.");
            if (!m_midiStatus.empty())
                ImGui::TextColored({1.0f, 0.8f, 0.4f, 1.0f}, "%s", m_midiStatus.c_str());
        } else {
            const auto ports = m_midi->listPorts();
            if (ports.empty()) {
                ImGui::TextUnformatted("No MIDI input ports found.");
                const std::string err = m_midi->lastError();
                if (!err.empty())
                    ImGui::TextColored({1.0f, 0.5f, 0.5f, 1.0f}, "%s", err.c_str());
            } else {
                const char* current = (m_midiPort >= 0 && m_midiPort < static_cast<int>(ports.size()))
                                          ? ports[static_cast<size_t>(m_midiPort)].c_str()
                                          : "None";
                if (ImGui::BeginCombo("Input port", current)) {
                    for (size_t i = 0; i < ports.size(); ++i) {
                        const bool selected = static_cast<int>(i) == m_midiPort;
                        if (ImGui::Selectable(ports[i].c_str(), selected)) {
                            if (openMidiPortGuarded(static_cast<unsigned int>(i)))
                                m_midiPort = static_cast<int>(i);
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // Don't read isOpen() once poisoned: a stuck openPort() worker may still be
                // writing the controller's state on its own thread.
                const char* status = m_midiOpenInProgress ? "not responding"
                                                           : (m_midi->isOpen() ? "connected" : "closed");
                ImGui::Text("Status: %s", status);
                if (!m_midiStatus.empty())
                    ImGui::TextColored({1.0f, 0.8f, 0.4f, 1.0f}, "%s", m_midiStatus.c_str());
            }
        }

        ImGui::Separator();
        m_midiBinder.drawContent();
    }

    // Scene window: draw the active scene's tunables.
    void drawActiveSceneContent()
    {
        const size_t activeIndex = m_timeline ? m_timeline->activeSceneIndex() : SceneTimeline::npos;
        if (activeIndex == 3 && m_latticeScene)
            m_latticeScene->drawControlsContent();
        else if (activeIndex == 2 && m_attractorScene)
            m_attractorScene->drawControlsContent();
        else if (activeIndex == 1 && m_tunnelScene)
            m_tunnelScene->drawControlsContent();
        else if (m_particleScene)
            m_particleScene->drawControlsContent();
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
        if (m_latticeScene) {
            if (const auto it = json.find(m_latticeScene->name()); it != json.end())
                m_latticeScene->fromJson(*it);
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
        if (m_latticeScene)
            json[m_latticeScene->name()] = m_latticeScene->toJson();

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
