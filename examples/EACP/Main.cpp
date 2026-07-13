#include "App.h"

using namespace eacp;

int main(int argc, char** argv)
{
    // PureDOOM locates WAD files via DOOMWADDIR (defaulting to the current
    // directory), so point it at the repository's shareware WAD unless the user
    // has already chosen a directory.
    if (!getEnv("DOOMWADDIR"))
        setEnv("DOOMWADDIR", PUREDOOM_ROOT_DIR);

    doom_set_default_int("key_up", DOOM_KEY_W);
    doom_set_default_int("key_down", DOOM_KEY_S);
    doom_set_default_int("key_strafeleft", DOOM_KEY_A);
    doom_set_default_int("key_straferight", DOOM_KEY_D);
    doom_set_default_int("key_use", DOOM_KEY_SPACE);
    doom_set_default_int("mouse_move", 0);

    doom_init(argc, argv, DOOM_FLAG_MENU_DARKEN_BG);

    // doom_init reads ~/.doomrc back over the defaults just set, so without this
    // an old config keeps its keys forever and the bindings above do nothing.
    eacpDoomBindKeys();

    return Apps::run<PureDoom::App>();
}
