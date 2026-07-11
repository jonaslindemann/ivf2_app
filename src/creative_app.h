#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "audio_player.h"
#include "creative_scene.h"
#include "midi_binder.h"
#include "control_center_window.h"
#include "scene_controls_window.h"

#include <ivf/scene_timeline.h>
#include <ivf/midi_controller.h>
#include <ivf/fade_effect.h>
#include <ivfui/scene_timeline_player.h>
#include <ivfui/glfw_scene_window.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

/**
 * @class CreativeApplicationWindow
 * @brief Reusable base window for timeline-driven creative-coding applications.
 *
 * Encapsulates everything a scene-based audiovisual app needs but that is not specific to
 * any particular set of scenes: a shared post-processing effect chain, an ivf::SceneTimeline
 * driven by a SceneTimelinePlayer, dip-to-black fade transitions, an AudioPlayer, a guarded
 * MIDI subsystem with a MidiParameterBinder, the Control Center / Scene control windows,
 * borderless-fullscreen "performance mode" with multi-monitor panel relocation, and JSON
 * settings persistence.
 *
 * A concrete application subclasses this window and overrides onRegisterScenes() to create its
 * scenes and register them with addScene(). Everything else - audio input routing, control
 * panels, MIDI binding, camera application, persistence - is handled generically over the
 * registered CreativeScene objects, so no per-scene-type code is required.
 *
 * Override createEffects() to supply a custom effect chain; the default is a broad set of the
 * ivf post-processing effects, all disabled until enabled from the Effect Inspector or MIDI.
 */
class CreativeApplicationWindow : public ivfui::GLFWSceneWindow {
public:
    CreativeApplicationWindow(int width, int height, const std::string& title);

    /**
     * @brief Register a scene and give it a slot on the timeline.
     *
     * The first time a scene is passed it is wired up: the shared effect chain (disabled) plus
     * the fade effect are attached, it is registered as a MIDI target, and it is tracked for
     * per-frame audio input and settings persistence. Passing the same scene again simply adds
     * another timeline slot (e.g. a closing reprise) of the given duration.
     *
     * @param scene    Scene to register / schedule.
     * @param duration Length of this timeline slot, in seconds.
     * @return The same scene pointer, for convenience.
     */
    CreativeScenePtr addScene(CreativeScenePtr scene, double duration);

    // Configuration, typically called from onRegisterScenes().
    void setAudioAsset(const std::filesystem::path& path);
    void setSettingsPath(const std::filesystem::path& path);
    void setMidiMappingPath(const std::filesystem::path& path);
    void setFadeDuration(double seconds);
    void setTimelineLoop(bool loop);

protected:
    /**
     * @brief Create the scenes and register them with addScene(). Override in a subclass.
     */
    virtual void onRegisterScenes() {}

    /**
     * @brief Build the shared post-processing effect chain. Override to customize.
     *
     * Returned effects are named (for stable MIDI target ids) and loaded, but left disabled;
     * addScene() attaches them to every registered scene. The fade transition effect is managed
     * separately by the framework and is not part of this list.
     */
    virtual std::vector<ivf::EffectPtr> createEffects();

    // GLFWSceneWindow / GLFWWindow overrides.
    int onSetup() override;
    void onUpdate() override;
    void onKey(int key, int scancode, int action, int mods) override;
    void onAddMenuItems(ivfui::UiMenu* menu) override;
    void onClose() override;
    void doDrawUi() override;

    // Accessors for subclasses that need finer control.
    ivf::SceneTimelinePtr timeline() const { return m_timeline; }
    AudioPlayer& audioPlayer() { return m_audioPlayer; }

    std::filesystem::path findAsset(const std::filesystem::path& relativePath) const;

private:
    // --- Timeline / scenes ------------------------------------------------------------------
    ivf::SceneTimelinePtr m_timeline;
    ivfui::SceneTimelinePlayerPtr m_timelinePlayer;
    bool m_timelineLoop{true};
    bool m_repeatScene{false};    // loop the running scene instead of advancing the timeline
    size_t m_repeatSceneIndex{0}; // scene pinned for repeat

    std::vector<CreativeScenePtr> m_scenes;         // unique registered scenes
    std::vector<CreativeScenePtr> m_timelineScenes; // one per timeline slot, index-aligned

    std::vector<ivf::EffectPtr> m_effects; // shared effect chain attached to every scene
    ivf::FadeEffectPtr m_fadeEffect;
    double m_fadeDuration{3.0}; // seconds of fade-in and fade-out per scene

    // --- Audio ------------------------------------------------------------------------------
    AudioPlayer m_audioPlayer;
    std::filesystem::path m_audioAsset;
    std::filesystem::path m_settingsPath{"scene_settings.json"};

    // --- MIDI -------------------------------------------------------------------------------
    ivf::MidiControllerPtr m_midi;
    MidiParameterBinder m_midiBinder;
    std::filesystem::path m_midiMappingPath{"midi_mappings.json"};
    int m_midiPort{-1};
    bool m_midiOpenInProgress{false}; // a guarded openPort() worker never returned; controller poisoned
    std::string m_midiStatus;         // last MIDI open result, shown in the MIDI tab

    // --- UI ---------------------------------------------------------------------------------
    ControlCenterWindowPtr m_controlCenter;
    SceneControlsWindowPtr m_sceneControls;

    // --- Performance / fullscreen mode ------------------------------------------------------
    bool m_fullscreen{false};
    bool m_uiHidden{false};
    int m_savedX{0}, m_savedY{0}, m_savedW{0}, m_savedH{0}; // windowed geometry to restore
    int m_displayMonitor{-1}; // fullscreen output monitor index; -1 = auto (monitor under window)

    int m_pendingLayoutFrames{0};
    ImVec2 m_panelTarget{};    // where to move the merged control dock in performance mode
    bool m_dockInitDone{false}; // default merged-panel dock layout applied once
    int m_dockOrderFrames{0};   // frames left to enforce tab order + focus after a fresh build

    // --- MIDI helpers -----------------------------------------------------------------------
    void setupMidi();
    ivf::MidiControllerPtr createMidiGuarded(int timeoutMs = 1500);
    static bool isVirtualMidiPort(const std::string& name);
    void autoOpenMidiPort();
    bool openMidiPortGuarded(unsigned int index, int timeoutMs = 500);

    // --- Fullscreen / monitor helpers -------------------------------------------------------
    GLFWmonitor* monitorForWindow();
    GLFWmonitor* otherMonitor(GLFWmonitor* current);
    GLFWmonitor* targetMonitor();
    void layoutPanelsOn(GLFWmonitor* monitor);
    void toggleFullscreen();

    // --- Control-panel tab content ----------------------------------------------------------
    void drawTransportContent();
    void drawDisplayContent();
    void drawMidiContent();
    void drawActiveSceneContent();

    // --- Playback / transitions -------------------------------------------------------------
    bool isDemoPlaying() const;
    void setDemoPlaying(bool playing);
    void updateSceneFade();
    void applyCamera(const ivf::TimelineCamera& camera);

    // --- Persistence ------------------------------------------------------------------------
    void loadSceneSettings();
    void saveSceneSettings();
};

typedef std::shared_ptr<CreativeApplicationWindow> CreativeApplicationWindowPtr;
