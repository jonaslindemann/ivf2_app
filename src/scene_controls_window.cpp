#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "scene_controls_window.h"

SceneControlsWindow::SceneControlsWindow(const std::string& name) : ivfui::UiWindow(name)
{
}

std::shared_ptr<SceneControlsWindow> SceneControlsWindow::create(const std::string& name)
{
    return std::make_shared<SceneControlsWindow>(name);
}

void SceneControlsWindow::doDraw()
{
    if (content)
        content();
}
