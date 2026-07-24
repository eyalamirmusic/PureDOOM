#include "App.h"

using namespace eacp;

namespace PureDoom
{
void configureDefaults()
{
    // PureDOOM locates WAD files via DOOMWADDIR (defaulting to the current
    // directory), so point it at the repository's shareware WAD unless the user
    // has already chosen a directory.
    if (!getEnv("DOOMWADDIR"))
        setEnv("DOOMWADDIR", PUREDOOM_ROOT_DIR);

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
    PureDoom::configureDefaults();
    Doom::initGame(argc, argv, Doom::DOOM_FLAG_MENU_DARKEN_BG);

    // Doom::initGame reads ~/.doomrc back over the defaults just set, so
    // without this an old config keeps its keys forever and the bindings above
    // do nothing.
    PureDoom::Engine::bindKeys();

    return Apps::run<PureDoom::App>();
}
