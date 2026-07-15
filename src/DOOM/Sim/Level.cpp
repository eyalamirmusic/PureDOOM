#include "Level.h"

namespace Doom
{
Level& level()
{
    static auto instance = Level {};
    return instance;
}
} // namespace Doom
