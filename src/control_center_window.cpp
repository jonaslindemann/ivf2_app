#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "control_center_window.h"

#include <imgui.h>

ControlCenterWindow::ControlCenterWindow(const std::string& name) : ivfui::UiWindow(name)
{
}

std::shared_ptr<ControlCenterWindow> ControlCenterWindow::create(const std::string& name)
{
    return std::make_shared<ControlCenterWindow>(name);
}

void ControlCenterWindow::doDraw()
{
    if (ImGui::BeginTabBar("ControlCenterTabs")) {
        if (transportContent && ImGui::BeginTabItem("Transport")) {
            transportContent();
            ImGui::EndTabItem();
        }
        if (displayContent && ImGui::BeginTabItem("Display")) {
            displayContent();
            ImGui::EndTabItem();
        }
        if (audioContent && ImGui::BeginTabItem("Audio")) {
            audioContent();
            ImGui::EndTabItem();
        }
        if (midiContent && ImGui::BeginTabItem("MIDI")) {
            midiContent();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

ImGuiWindowFlags ControlCenterWindow::doWindowFlags() const
{
    // Drop AlwaysAutoResize (the base-class default) so the tab bar keeps a stable size
    // instead of snapping to each tab's content every frame.
    return ImGuiWindowFlags_None;
}
