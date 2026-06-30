#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ivfui/ui_window.h>

#include <functional>
#include <memory>
#include <string>

/**
 * @class SceneControlsWindow
 * @brief Stable host window whose body is supplied by the application.
 *
 * Used to give the active timeline scene's tunables a single fixed window instead of one
 * window per scene. The application sets @ref content to draw the active scene's controls.
 */
class SceneControlsWindow : public ivfui::UiWindow {
public:
    std::function<void()> content;

    SceneControlsWindow(const std::string& name);

    static std::shared_ptr<SceneControlsWindow> create(const std::string& name);

protected:
    void doDraw() override;
};

typedef std::shared_ptr<SceneControlsWindow> SceneControlsWindowPtr;
