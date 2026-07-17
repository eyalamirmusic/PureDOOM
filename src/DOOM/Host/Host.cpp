#include "Host.h"

namespace Doom
{
// A function-local static, so it is constructed on the first call whoever makes it -
// which is Api.cpp binding `doom_print_fn& doom_print = host().print` (and the other
// twelve) at static-init time, before main(). The same shape as engine(), and
// deliberately a *separate* singleton: the host callbacks are platform state that must
// outlive any one constructed world.
Host& host()
{
    static auto instance = Host {};
    return instance;
}
} // namespace Doom
