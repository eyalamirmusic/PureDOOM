#include "App.h"

using namespace eacp;

int main(int argc, char** argv)
{
    // PureDOOM locates WAD files via DOOMWADDIR (defaulting to the current
    // directory), so point it at the repository's shareware WAD unless the user
    // has already chosen a directory.
    if (!getEnv("DOOMWADDIR"))
        setEnv("DOOMWADDIR", PUREDOOM_ROOT_DIR);

    Doom::setDefaultInt("key_up", Doom::DOOM_KEY_W);
    Doom::setDefaultInt("key_down", Doom::DOOM_KEY_S);
    Doom::setDefaultInt("key_strafeleft", Doom::DOOM_KEY_A);
    Doom::setDefaultInt("key_straferight", Doom::DOOM_KEY_D);
    Doom::setDefaultInt("key_use", Doom::DOOM_KEY_SPACE);
    Doom::setDefaultInt("mouse_move", 0);

    Doom::initGame(argc, argv, Doom::DOOM_FLAG_MENU_DARKEN_BG);

    // Doom::initGame reads ~/.doomrc back over the defaults just set, so
    // without this an old config keeps its keys forever and the bindings above
    // do nothing.
    eacpDoomBindKeys();

    return Apps::run<PureDoom::App>();
}
