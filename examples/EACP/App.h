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
        view.input = &input;
        view.focus();
    }

    // The window and the view's order is only their order: the view asks the
    // window for itself rather than being handed it. The input's is not - it
    // listens to the window, which must outlive it, so it is declared last and
    // destroyed first.
    Graphics::Window window {windowOptions()};
    View view;
    Graphics::GameInput input {window};
};
} // namespace PureDoom
