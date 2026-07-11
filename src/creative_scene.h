#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ivf/scene_timeline.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <string_view>

/**
 * @class CreativeScene
 * @brief Common base for the app's timeline scenes.
 *
 * Every scene in a CreativeApplicationWindow shares the same surface: it draws its own
 * tunables, reacts to an audio-energy input, and serializes its settings to JSON. Those
 * four methods are not part of ivf::TimelineScene, so promoting them to virtuals here lets
 * the application treat any scene uniformly (audio, controls, persistence, MIDI binding)
 * instead of dispatching on the concrete scene type.
 *
 * Per-frame draw callbacks take the `on` prefix to match the framework's onSetup/onUpdate/
 * onKey convention; setAudioInput keeps the set* convention (it is a state setter, not an
 * event), and toJson/fromJson are serialization rather than per-frame events.
 *
 * ivf::TimelineScene already provides name(), duration(), setDuration(), camera(), and the
 * effect() accessors, and (via PropertyInspectable) the property surface the MIDI binder uses.
 */
class CreativeScene : public ivf::TimelineScene {
public:
    CreativeScene(std::string_view name = {}, double duration = 1.0) : ivf::TimelineScene(name, duration) {}

    /// Draw the scene's ImGui tunables (no window wrapper). Called every frame for the active scene.
    virtual void onDrawControlsContent() {}

    /// Feed the current audio energy and playing state to the scene.
    virtual void setAudioInput(float /*level*/, bool /*playing*/) {}

    /// Serialize the scene's settings.
    virtual nlohmann::json toJson() const
    {
        return nlohmann::json::object();
    }

    /// Restore the scene's settings.
    virtual void fromJson(const nlohmann::json& /*json*/) {}
};

using CreativeScenePtr = std::shared_ptr<CreativeScene>;
