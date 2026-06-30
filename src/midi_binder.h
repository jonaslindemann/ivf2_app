#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ivf/property_inspectable.h>

#include <filesystem>
#include <string>
#include <vector>

/**
 * @class MidiParameterBinder
 * @brief Maps incoming MIDI Control-Change messages to named PropertyInspectable
 *        parameters of any number of targets (scenes, effects, ...).
 *
 * Because both ivf::TimelineScene and ivf::Effect derive from PropertyInspectable,
 * a single binder can drive scene tunables and post-processing effect parameters
 * with no per-target code. Bindings are keyed by a stable target id + property name
 * (+ vector component) so they survive across runs when saved to JSON.
 */
class MidiParameterBinder {
public:
    struct Target {
        std::string id;                 ///< Stable key, e.g. "scene:Flow Field".
        std::string label;              ///< Display label for the UI.
        ivf::PropertyInspectable* ptr;  ///< Live object (not owned).
    };

    struct Binding {
        int channel{-1};        ///< MIDI channel 0-15, or -1 for "any".
        int cc{0};              ///< Control-Change number.
        std::string targetId;   ///< Target::id.
        std::string property;   ///< Property name within the target.
        int component{-1};      ///< Vector component 0..3, or -1 for scalar.
    };

    /**
     * @brief Register a controllable target. The pointer must outlive the binder.
     */
    void addTarget(const std::string& id, const std::string& label, ivf::PropertyInspectable* ptr);

    /**
     * @brief Apply (or, if learn is armed, capture) a Control-Change message.
     * @param channel MIDI channel 0-15.
     * @param cc      Control-Change number.
     * @param value   Control-Change value 0-127.
     */
    void applyCC(int channel, int cc, int value);

    /**
     * @brief Draw the MIDI-Learn / bindings ImGui contents (no window wrapper).
     */
    void drawContent();

    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;

private:
    std::vector<Target> m_targets;
    std::vector<Binding> m_bindings;

    // Learn state.
    bool m_learnArmed{false};
    std::string m_learnTarget;
    std::string m_learnProperty;
    int m_learnComponent{-1};

    Target* findTarget(const std::string& id);
    const ivf::Property* findProperty(const Target& target, const std::string& name) const;
    void writeValue(const Target& target, const Binding& binding, int value0_127);
    Binding* findBinding(int channel, int cc);
    void setBinding(const Binding& binding);
    void clearBinding(const std::string& targetId, const std::string& property, int component);
    const Binding* bindingFor(const std::string& targetId, const std::string& property, int component) const;
};
