#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "midi_binder.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <variant>

using ivf::Property;
using ivf::PropertyEditor;
using ivf::PropertyInspectable;

namespace {

// Number of MIDI-addressable components for a property: 1 for scalars/bool,
// 3/4 for vectors, 0 for unsupported types (e.g. string).
int componentCount(const Property& prop)
{
    if (std::holds_alternative<float*>(prop.value) || std::holds_alternative<double*>(prop.value) ||
        std::holds_alternative<int*>(prop.value) || std::holds_alternative<glm::uint*>(prop.value) ||
        std::holds_alternative<bool*>(prop.value))
        return 1;
    if (std::holds_alternative<glm::vec3*>(prop.value) || std::holds_alternative<glm::uvec3*>(prop.value))
        return 3;
    if (std::holds_alternative<glm::vec4*>(prop.value) || std::holds_alternative<glm::uvec4*>(prop.value))
        return 4;
    return 0;
}

bool isScalar(const Property& prop)
{
    return componentCount(prop) == 1;
}

} // namespace

void MidiParameterBinder::addTarget(const std::string& id, const std::string& label, PropertyInspectable* ptr)
{
    if (!ptr)
        return;
    // Ensure the target's properties are registered before any lookups.
    ptr->getProperties();
    m_targets.push_back({id, label, ptr});
}

MidiParameterBinder::Target* MidiParameterBinder::findTarget(const std::string& id)
{
    for (auto& t : m_targets)
        if (t.id == id)
            return &t;
    return nullptr;
}

const Property* MidiParameterBinder::findProperty(const Target& target, const std::string& name) const
{
    const auto& props = target.ptr->getProperties();
    for (const auto& p : props)
        if (p.name == name)
            return &p;
    return nullptr;
}

void MidiParameterBinder::writeValue(const Target& target, const Binding& binding, int value0_127)
{
    const Property* prop = findProperty(target, binding.property);
    if (!prop)
        return;

    const double t = std::clamp(value0_127 / 127.0, 0.0, 1.0);
    const double mn = prop->minValue;
    const double mx = prop->maxValue;
    const double v = mn + t * (mx - mn);

    if (auto p = std::get_if<float*>(&prop->value)) {
        **p = static_cast<float>(v);
    } else if (auto p = std::get_if<double*>(&prop->value)) {
        **p = v;
    } else if (auto p = std::get_if<int*>(&prop->value)) {
        **p = static_cast<int>(std::lround(v));
    } else if (auto p = std::get_if<glm::uint*>(&prop->value)) {
        **p = static_cast<glm::uint>(std::lround(std::max(0.0, v)));
    } else if (auto p = std::get_if<bool*>(&prop->value)) {
        **p = value0_127 >= 64;
    } else if (auto p = std::get_if<glm::vec3*>(&prop->value)) {
        if (binding.component >= 0 && binding.component < 3)
            (**p)[binding.component] = static_cast<float>(v);
    } else if (auto p = std::get_if<glm::vec4*>(&prop->value)) {
        if (binding.component >= 0 && binding.component < 4)
            (**p)[binding.component] = static_cast<float>(v);
    } else {
        return; // unsupported type
    }

    target.ptr->notifyPropertyChanged(binding.property);
}

MidiParameterBinder::Binding* MidiParameterBinder::findBinding(int channel, int cc)
{
    for (auto& b : m_bindings)
        if (b.cc == cc && (b.channel == -1 || b.channel == channel))
            return &b;
    return nullptr;
}

const MidiParameterBinder::Binding* MidiParameterBinder::bindingFor(const std::string& targetId,
                                                                    const std::string& property,
                                                                    int component) const
{
    for (const auto& b : m_bindings)
        if (b.targetId == targetId && b.property == property && b.component == component)
            return &b;
    return nullptr;
}

void MidiParameterBinder::setBinding(const Binding& binding)
{
    // A CC drives exactly one parameter, and a parameter is driven by one CC:
    // drop any existing binding that collides on either side.
    m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(),
                                    [&](const Binding& b) {
                                        const bool sameCC = b.cc == binding.cc && b.channel == binding.channel;
                                        const bool sameParam = b.targetId == binding.targetId &&
                                                               b.property == binding.property &&
                                                               b.component == binding.component;
                                        return sameCC || sameParam;
                                    }),
                     m_bindings.end());
    m_bindings.push_back(binding);
}

void MidiParameterBinder::clearBinding(const std::string& targetId, const std::string& property, int component)
{
    m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(),
                                    [&](const Binding& b) {
                                        return b.targetId == targetId && b.property == property &&
                                               b.component == component;
                                    }),
                     m_bindings.end());
}

void MidiParameterBinder::applyCC(int channel, int cc, int value)
{
    if (m_learnArmed) {
        Binding binding;
        binding.channel = channel;
        binding.cc = cc;
        binding.targetId = m_learnTarget;
        binding.property = m_learnProperty;
        binding.component = m_learnComponent;
        setBinding(binding);
        m_learnArmed = false;
        return;
    }

    if (Binding* b = findBinding(channel, cc)) {
        if (Target* t = findTarget(b->targetId))
            writeValue(*t, *b, value);
    }
}

void MidiParameterBinder::drawContent()
{
    if (m_learnArmed) {
        ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Move a MIDI control to bind:");
        ImGui::TextUnformatted(m_learnProperty.c_str());
        if (ImGui::Button("Cancel learn"))
            m_learnArmed = false;
        ImGui::Separator();
    }

    for (auto& target : m_targets) {
        if (!ImGui::CollapsingHeader(target.label.c_str()))
            continue;

        ImGui::PushID(target.id.c_str());
        const auto& props = target.ptr->getProperties();
        for (const auto& prop : props) {
            const int count = componentCount(prop);
            if (count == 0)
                continue;

            for (int c = 0; c < count; ++c) {
                const int component = isScalar(prop) ? -1 : c;

                std::string rowLabel = prop.name;
                if (!isScalar(prop))
                    rowLabel += " [" + PropertyEditor::getComponentName(prop, c) + "]";

                ImGui::PushID(rowLabel.c_str());
                ImGui::TextUnformatted(rowLabel.c_str());
                ImGui::SameLine(180.0f);

                const Binding* existing = bindingFor(target.id, prop.name, component);
                const bool listening = m_learnArmed && m_learnTarget == target.id &&
                                       m_learnProperty == prop.name && m_learnComponent == component;

                if (listening) {
                    ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "waiting...");
                } else if (existing) {
                    ImGui::Text("CC %d (ch %d)", existing->cc, existing->channel + 1);
                } else {
                    ImGui::TextDisabled("unbound");
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Learn")) {
                    m_learnArmed = true;
                    m_learnTarget = target.id;
                    m_learnProperty = prop.name;
                    m_learnComponent = component;
                }
                if (existing) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X"))
                        clearBinding(target.id, prop.name, component);
                }

                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
}

bool MidiParameterBinder::load(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
        return false;

    nlohmann::json json;
    in >> json;

    const auto it = json.find("bindings");
    if (it == json.end() || !it->is_array())
        return false;

    m_bindings.clear();
    for (const auto& entry : *it) {
        Binding b;
        b.channel = entry.value("channel", -1);
        b.cc = entry.value("cc", 0);
        b.targetId = entry.value("target", std::string{});
        b.property = entry.value("property", std::string{});
        b.component = entry.value("component", -1);
        if (!b.targetId.empty() && !b.property.empty())
            m_bindings.push_back(b);
    }
    return true;
}

bool MidiParameterBinder::save(const std::filesystem::path& path) const
{
    nlohmann::json bindings = nlohmann::json::array();
    for (const auto& b : m_bindings) {
        bindings.push_back({
            {"channel", b.channel},
            {"cc", b.cc},
            {"target", b.targetId},
            {"property", b.property},
            {"component", b.component},
        });
    }

    nlohmann::json json;
    json["bindings"] = bindings;

    std::ofstream out(path);
    if (!out)
        return false;
    out << json.dump(4);
    return true;
}
