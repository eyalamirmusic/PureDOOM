#pragma once

#include "Layout.h"
#include "View.h"

namespace PureDoom
{
// The window is declared before the view, so the view can take it as a reference
// and never has to ask whether it has one.
struct App
{
    App()
    {
        window.setContentView(view);
        view.focus();
    }

    Graphics::Window window {windowOptions()};
    View view {window};
};
} // namespace PureDoom
