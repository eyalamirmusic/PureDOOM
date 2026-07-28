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

    // Their order is now only their order. It used to be a constraint: the view
    // took a Graphics::Window& because a view could not reach the window it was
    // in, so declaring them the other way round handed it a reference to an
    // object that did not exist yet. It asks the window for itself now.
    Graphics::Window window {windowOptions()};
    View view;
};
} // namespace PureDoom
