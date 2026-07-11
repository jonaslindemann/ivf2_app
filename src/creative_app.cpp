#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "creative_app.h"

#include <ivf/gl.h>
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

#include <imgui.h>
#include <imgui_internal.h> // DockBuilder API for the default merged-panel layout
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

using namespace ivf;
using namespace ivfui;

// Stable id for the dock node that hosts the three merged control panels. A fixed literal
// (rather than a scope-derived GetID) keeps the id constant across runs, so once the layout is
// saved to imgui.ini it is recognized and the user's own arrangement is respected thereafter.
static constexpr ImGuiID k_controlsDockId = 0xCD00CD02u;

CreativeApplicationWindow::CreativeApplicationWindow(int width, int height, const std::string& title)
    : GLFWSceneWindow(width, height, title)
{
}

std::vector<EffectPtr> CreativeApplicationWindow::createEffects()
{
    // Create and configure the default post-processing effects. Each is loaded and named (so
    // MIDI bindings have stable, human-readable target ids) but left disabled; addScene()
    // attaches them to every registered scene.

    auto blurEffect = BlurEffect::create();
    blurEffect->setBlurRadius(2.0);
    blurEffect->load();

    auto tintEffect = TintEffect::create();
    tintEffect->setTintColor(glm::vec3(1.2, 0.9, 0.7));
    tintEffect->setTintStrength(0.5);
    tintEffect->setGrayScaleWeights(glm::vec3(0.299, 0.587, 0.114));
    tintEffect->load();

    auto filmgrainEffect = FilmgrainEffect::create();
    filmgrainEffect->setNoiseIntensity(0.5);
    filmgrainEffect->setGrainBlending(0.5);
    filmgrainEffect->load();

    auto chromaticEffect = ChromaticEffect::create();
    chromaticEffect->setOffset(0.01);
    chromaticEffect->load();

    auto vignetteEffect = VignetteEffect::create();
    vignetteEffect->setSize(1.0);
    vignetteEffect->setSmoothness(0.7);
    vignetteEffect->load();

    auto bloomEffect = BloomEffect::create();
    bloomEffect->setThreshold(1.0);
    bloomEffect->setIntensity(1.0);
    bloomEffect->load();

    auto ditheringEffect = DitheringEffect::create();
    ditheringEffect->load();

    auto pixelationEffect = PixelationEffect::create();
    pixelationEffect->setPixelSize(4.0);
    pixelationEffect->load();

    auto waveDistortionEffect = WaveDistortionEffect::create();
    waveDistortionEffect->setFrequency(10.0f);
    waveDistortionEffect->setAmplitude(0.01f);
    waveDistortionEffect->setSpeed(1.0f);
    waveDistortionEffect->load();

    auto swirlEffect = SwirlEffect::create();
    swirlEffect->setRadius(0.5f);
    swirlEffect->setAngle(3.14159f);
    swirlEffect->load();

    auto kaleidoscopeEffect = KaleidoscopeEffect::create();
    kaleidoscopeEffect->setSegments(6.0f);
    kaleidoscopeEffect->setRotation(0.0f);
    kaleidoscopeEffect->load();

    auto glitchEffect = GlitchEffect::create();
    glitchEffect->setIntensity(0.05f);
    glitchEffect->setBlockSize(0.05f);
    glitchEffect->setSpeed(4.0f);
    glitchEffect->load();

    auto scanlineEffect = ScanlineEffect::create();
    scanlineEffect->setLineSpacing(4.0f);
    scanlineEffect->setLineIntensity(0.4f);
    scanlineEffect->setScrollSpeed(0.0f);
    scanlineEffect->load();

    auto posterizeEffect = PosterizeEffect::create();
    posterizeEffect->setLevels(4.0f);
    posterizeEffect->load();

    auto colorGradingEffect = ColorGradingEffect::create();
    colorGradingEffect->setShadows(glm::vec3(0.6f, 0.5f, 0.5f));
    colorGradingEffect->setMidtones(glm::vec3(0.5f, 0.5f, 0.5f));
    colorGradingEffect->setHighlights(glm::vec3(0.5f, 0.5f, 0.6f));
    colorGradingEffect->setContrast(1.1f);
    colorGradingEffect->setSaturation(1.2f);
    colorGradingEffect->load();

    auto nightVisionEffect = NightVisionEffect::create();
    nightVisionEffect->setNoiseIntensity(0.05f);
    nightVisionEffect->setGlowStrength(0.3f);
    nightVisionEffect->setPhosphorColor(glm::vec3(0.1f, 1.0f, 0.1f));
    nightVisionEffect->load();

    auto motionBlurEffect = MotionBlurEffect::create();
    motionBlurEffect->load();

    auto feedbackEffect = FeedbackEffect::create();
    feedbackEffect->setFeedbackAmount(0.9f);
    feedbackEffect->load();

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

    return {
        blurEffect,       tintEffect,       chromaticEffect,   ditheringEffect, bloomEffect,
        filmgrainEffect,  glitchEffect,     scanlineEffect,    posterizeEffect, colorGradingEffect,
        motionBlurEffect, feedbackEffect,   halftoneEffect,
    };
}

CreativeScenePtr CreativeApplicationWindow::addScene(CreativeScenePtr scene, double duration)
{
    if (!scene)
        return scene;

    const bool isNew = std::find(m_scenes.begin(), m_scenes.end(), scene) == m_scenes.end();
    if (isNew) {
        scene->setDuration(duration);

        // Attach the shared effect chain, disabled by default, then the fade effect on top so
        // it applies after every other effect and stays enabled for scene fade-in / fade-out.
        for (const auto& effect : m_effects)
            scene->effect(effect);
        for (const auto& effect : scene->effects())
            if (effect && effect->program())
                effect->program()->setEnabled(false);
        if (m_fadeEffect)
            scene->effect(m_fadeEffect);

        // Register as a MIDI target (scenes derive from PropertyInspectable).
        m_midiBinder.addTarget("scene:" + scene->name(), "Scene: " + scene->name(), scene.get());

        m_scenes.push_back(scene);
    }

    // Append a timeline slot. The assignment slices the scene's timeline state (root, effects,
    // camera, callbacks) into the stored copy; the captured onUpdate keeps driving the live
    // scene object, so m_timelineScenes below can read its camera each frame.
    m_timeline->addScene(scene->name(), duration) = *scene;
    m_timelineScenes.push_back(scene);

    return scene;
}

void CreativeApplicationWindow::setAudioAsset(const std::filesystem::path& path)
{
    m_audioAsset = path;
}

void CreativeApplicationWindow::setSettingsPath(const std::filesystem::path& path)
{
    m_settingsPath = path;
}

void CreativeApplicationWindow::setMidiMappingPath(const std::filesystem::path& path)
{
    m_midiMappingPath = path;
}

void CreativeApplicationWindow::setFadeDuration(double seconds)
{
    m_fadeDuration = seconds;
}

void CreativeApplicationWindow::setTimelineLoop(bool loop)
{
    m_timelineLoop = loop;
    if (m_timeline)
        m_timeline->setLoop(loop);
}

int CreativeApplicationWindow::onSetup()
{
    setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    enableHeadlight();
    this->setRenderToTexture(true);

    // Build the shared effect chain and the fade transition effect before scenes are registered,
    // so addScene() can attach them.
    m_effects = createEffects();

    m_fadeEffect = FadeEffect::create();
    m_fadeEffect->setFadeColor(glm::vec3(0.0f, 0.0f, 0.0f));
    m_fadeEffect->setFadeAmount(0.0f);
    m_fadeEffect->load();

    m_timeline = SceneTimeline::create();
    m_timeline->setLoop(m_timelineLoop);

    // Subclass creates scenes and registers them (and may set audio asset, loop, etc.).
    onRegisterScenes();

    m_timeline->pause();

    m_timelinePlayer = SceneTimelinePlayer::create(this, m_timeline);
    m_timelinePlayer->update(0.0);

    loadSceneSettings();
    if (!m_audioAsset.empty())
        m_audioPlayer.load(findAsset(m_audioAsset));

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

void CreativeApplicationWindow::setupMidi()
{
    // Effect objects are shared across scenes; registering each named one once is enough.
    // (Scene targets were registered in addScene().)
    for (const auto& effect : m_effects) {
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
MidiControllerPtr CreativeApplicationWindow::createMidiGuarded(int timeoutMs)
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
bool CreativeApplicationWindow::isVirtualMidiPort(const std::string& name)
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
void CreativeApplicationWindow::autoOpenMidiPort()
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
bool CreativeApplicationWindow::openMidiPortGuarded(unsigned int index, int timeoutMs)
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

void CreativeApplicationWindow::onUpdate()
{
    const double dt = TimeController::instance()->delta();

    // Drain MIDI on the main thread and apply CC messages to bound parameters.
    if (m_midi) {
        for (const auto& msg : m_midi->poll())
            if (msg.type == MidiMessage::ControlChange)
                m_midiBinder.applyCC(msg.channel, msg.data1, msg.data2);
    }

    for (const auto& scene : m_scenes)
        scene->setAudioInput(m_audioPlayer.energy(), m_audioPlayer.isPlaying());

    if (m_timelinePlayer)
        m_timelinePlayer->update(dt);

    // Repeat mode: if the timeline has advanced off the pinned scene, seek back to it so the
    // running scene loops in place instead of moving on to the next one.
    if (m_repeatScene && m_timeline && m_timeline->activeSceneIndex() != m_repeatSceneIndex)
        m_timeline->seekToScene(m_repeatSceneIndex);

    updateSceneFade();

    const size_t activeIndex = m_timeline ? m_timeline->activeSceneIndex() : SceneTimeline::npos;
    if (activeIndex < m_timelineScenes.size() && m_timelineScenes[activeIndex])
        applyCamera(m_timelineScenes[activeIndex]->camera());
}

void CreativeApplicationWindow::onKey(int key, int /*sc*/, int action, int /*mods*/)
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
void CreativeApplicationWindow::onAddMenuItems(UiMenu* menu)
{
    if (!menu || menu->name() != "View")
        return;

    menu->addItem(UiMenuItem::create(
        "Performance mode", "F11", [this]() { toggleFullscreen(); }, [this]() { return m_fullscreen; }));
    menu->addItem(UiMenuItem::create(
        "Hide UI", "Tab", [this]() { m_uiHidden = !m_uiHidden; }, [this]() { return m_uiHidden; }));
}

void CreativeApplicationWindow::onClose()
{
    saveSceneSettings();
    m_midiBinder.save(m_midiMappingPath);
    if (m_audioPlayer.isPlaying())
        m_audioPlayer.setPaused(true);
}

// Clean-output mode: skip all ImGui rendering so only the scene shows. The ImGui frame is
// still begun/ended by the framework, just with nothing drawn.
void CreativeApplicationWindow::doDrawUi()
{
    // Merge the transport (Control Center), Scene, and Effect Inspector panels into one tabbed
    // dock group on first use. Because the Scene panel is a single persistent window whose body
    // is swapped per active scene, this dock slot survives scene changes (and restarts, via
    // imgui.ini). Guarded by the node's existence so a saved layout - including the user's own
    // arrangement - always wins.
    if (!m_dockInitDone) {
        m_dockInitDone = true;
        if (ImGui::DockBuilderGetNode(k_controlsDockId) == nullptr) {
            ImGui::DockBuilderAddNode(k_controlsDockId, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderSetNodePos(k_controlsDockId, ImVec2(20.0f, 40.0f));
            ImGui::DockBuilderSetNodeSize(k_controlsDockId, ImVec2(400.0f, 720.0f));
            ImGui::DockBuilderDockWindow("Control Center", k_controlsDockId);
            ImGui::DockBuilderDockWindow("Scene", k_controlsDockId);
            ImGui::DockBuilderDockWindow("Effect Inspector", k_controlsDockId);
            ImGui::DockBuilderFinish(k_controlsDockId);
            m_dockOrderFrames = 4; // enforce the tab order + focus once the windows exist
        }
    }

    // The tab order in a dock node follows window submission order, and the base class submits
    // Effect Inspector before this app's Control Center / Scene. Pin the desired order explicitly
    // (Control Center, Scene, Effect Inspector) by reordering the node's tab bar, and focus
    // Control Center. Runs only for the few frames after a fresh build, so any tab reordering the
    // user does afterwards is preserved (and persisted to imgui.ini).
    if (m_dockOrderFrames > 0) {
        auto rank = [](const ImGuiWindow* w) -> int {
            if (!w || !w->Name)
                return 99;
            if (std::strcmp(w->Name, "Control Center") == 0) return 0;
            if (std::strcmp(w->Name, "Scene") == 0) return 1;
            if (std::strcmp(w->Name, "Effect Inspector") == 0) return 2;
            return 98;
        };
        if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(k_controlsDockId); node && node->TabBar) {
            ImVector<ImGuiTabItem>& tabs = node->TabBar->Tabs;
            for (int i = 1; i < tabs.Size; ++i) { // stable insertion sort by desired rank
                ImGuiTabItem key = tabs[i];
                int j = i - 1;
                while (j >= 0 && rank(tabs[j].Window) > rank(key.Window)) {
                    tabs[j + 1] = tabs[j];
                    --j;
                }
                tabs[j + 1] = key;
            }
        }
        if (m_dockOrderFrames == 1)
            ImGui::SetWindowFocus("Control Center");
        --m_dockOrderFrames;
    }

    // Performance mode: move the merged control dock to the other monitor. Runs inside the ImGui
    // frame so the node's platform window relocates this frame.
    if (m_pendingLayoutFrames > 0) {
        if (ImGui::DockBuilderGetNode(k_controlsDockId) != nullptr)
            ImGui::DockBuilderSetNodePos(k_controlsDockId, m_panelTarget);
        --m_pendingLayoutFrames;
    }

    if (m_uiHidden)
        return;
    GLFWSceneWindow::doDrawUi();
}

// Pick the monitor the window currently sits on (by window center), for multi-monitor setups.
GLFWmonitor* CreativeApplicationWindow::monitorForWindow()
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
GLFWmonitor* CreativeApplicationWindow::otherMonitor(GLFWmonitor* current)
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
GLFWmonitor* CreativeApplicationWindow::targetMonitor()
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (m_displayMonitor >= 0 && m_displayMonitor < count)
        return monitors[m_displayMonitor];
    return monitorForWindow();
}

// Schedule the merged control dock to move near the top-left of the given monitor's work area.
void CreativeApplicationWindow::layoutPanelsOn(GLFWmonitor* monitor)
{
    if (!monitor)
        return;

    int wax = 0, way = 0, waw = 0, wah = 0;
    glfwGetMonitorWorkarea(monitor, &wax, &way, &waw, &wah);

    m_panelTarget = ImVec2(static_cast<float>(wax + 20), static_cast<float>(way + 20));
    m_pendingLayoutFrames = 2;
}

// Performance mode: borderless fullscreen output on the current monitor, control panels moved
// to the other monitor, main menu hidden. Toggling off restores the windowed editing layout.
void CreativeApplicationWindow::toggleFullscreen()
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
void CreativeApplicationWindow::drawTransportContent()
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
void CreativeApplicationWindow::drawDisplayContent()
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
void CreativeApplicationWindow::drawMidiContent()
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
void CreativeApplicationWindow::drawActiveSceneContent()
{
    const size_t activeIndex = m_timeline ? m_timeline->activeSceneIndex() : SceneTimeline::npos;
    if (activeIndex < m_timelineScenes.size() && m_timelineScenes[activeIndex])
        m_timelineScenes[activeIndex]->onDrawControlsContent();
}

bool CreativeApplicationWindow::isDemoPlaying() const
{
    return m_timeline && m_timeline->isPlaying() && !TimeController::instance()->isPaused();
}

void CreativeApplicationWindow::setDemoPlaying(bool playing)
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
void CreativeApplicationWindow::updateSceneFade()
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

void CreativeApplicationWindow::applyCamera(const TimelineCamera& camera)
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

void CreativeApplicationWindow::loadSceneSettings()
{
    if (m_scenes.empty())
        return;

    std::ifstream in(m_settingsPath);
    if (!in)
        return;

    nlohmann::json json;
    in >> json;

    for (const auto& scene : m_scenes) {
        if (const auto it = json.find(scene->name()); it != json.end())
            scene->fromJson(*it);
    }
}

void CreativeApplicationWindow::saveSceneSettings()
{
    if (m_scenes.empty())
        return;

    nlohmann::json json;

    std::ifstream in(m_settingsPath);
    if (in)
        in >> json;

    if (!json.is_object())
        json = nlohmann::json::object();

    for (const auto& scene : m_scenes)
        json[scene->name()] = scene->toJson();

    std::ofstream out(m_settingsPath);
    if (out)
        out << json.dump(4);
}

std::filesystem::path CreativeApplicationWindow::findAsset(const std::filesystem::path& relativePath) const
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
