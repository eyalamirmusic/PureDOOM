#include "App.h"
#include "Iwad.h"

#include <cstdio>
#include <exception>

using namespace eacp;

namespace PureDoom
{
void configureDefaults()
{
    Doom::setDefaultInt("key_up", Doom::Key::W);
    Doom::setDefaultInt("key_down", Doom::Key::S);
    Doom::setDefaultInt("key_strafeleft", Doom::Key::A);
    Doom::setDefaultInt("key_straferight", Doom::Key::D);
    Doom::setDefaultInt("key_use", Doom::Key::Space);
    Doom::setDefaultInt("mouse_move", 0);
}
} // namespace PureDoom

int main(int argc, char** argv)
{
    // Before the engine boots, and before the app's own loop: the fetch pumps
    // the event loop itself while the file comes down, and the engine finds the
    // WAD through DOOMWADDIR, which this sets.
    try
    {
        PureDoom::ensureIwad();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    PureDoom::configureDefaults();
    Doom::initGame(argc, argv, Doom::DOOM_FLAG_MENU_DARKEN_BG);

    // Doom::initGame reads ~/.doomrc back over the defaults just set, so
    // without this an old config keeps its keys forever and the bindings above
    // do nothing.
    PureDoom::Engine::bindKeys();

    return Apps::run<PureDoom::App>();
}
