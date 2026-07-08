#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ivfui/ui_window.h>

#include <functional>
#include <memory>
#include <string>

/**
 * @class ControlCenterWindow
 * @brief Tabbed host window consolidating the demo's transport, audio and MIDI panels.
 *
 * The window itself is intentionally dumb: it renders a tab bar and defers each tab's
 * contents to a std::function supplied by the application, so all wiring (timeline,
 * TimeController, audio player, MIDI) stays centralized in the owning window.
 */
class ControlCenterWindow : public ivfui::UiWindow {
public:
    std::function<void()> transportContent;
    std::function<void()> displayContent;
    std::function<void()> audioContent;
    std::function<void()> midiContent;

    ControlCenterWindow(const std::string& name);

    static std::shared_ptr<ControlCenterWindow> create(const std::string& name);

protected:
    void doDraw() override;
    ImGuiWindowFlags doWindowFlags() const override;
};

typedef std::shared_ptr<ControlCenterWindow> ControlCenterWindowPtr;
