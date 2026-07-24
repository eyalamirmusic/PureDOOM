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
        view.focus();
    }

    Graphics::Window window {windowOptions()};
    View view {window};
};
} // namespace PureDoom
