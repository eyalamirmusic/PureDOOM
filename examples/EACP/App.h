#pragma once

#include "Layout.h"
#include "View.h"

namespace PureDoom
{
struct App
{
    App()
    {
        window.setContentView(view);
        view.window = &window;
        view.focus();
    }

    View view;
    Graphics::Window window {windowOptions()};
};
} // namespace PureDoom
