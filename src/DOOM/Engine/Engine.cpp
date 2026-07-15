#include "Engine.h"

namespace Doom
{
// A function-local static, so it is constructed on the first call whoever makes
// it, regardless of translation-unit init order. That matters: m_random.cpp binds
// `int& rndindex = randomness().menuIndex` at static-init time, which reaches
// through here before main() runs.
Engine& engine()
{
    static auto instance = Engine {};
    return instance;
}

// The vanilla free functions, now views onto the one Engine's members rather than
// singletons of their own. This is the composition root: the three subsystems are
// owned in one place, and every caller - rewritten or not - reaches the same one.
Random& randomness()
{
    return engine().random;
}

WadFile& wad()
{
    return engine().wad;
}

Level& level()
{
    return engine().level;
}

Clip& clip()
{
    return engine().clip;
}

ViewPoint& viewPoint()
{
    return engine().viewPoint;
}
} // namespace Doom
