#include "Text.h"

namespace Doom
{
std::string hexString(int value)
{
    constexpr auto digits = std::string_view {"0123456789ABCDEF"};

    auto out = std::string {};

    if (value < 0)
        out += '-';

    auto magnitude = value < 0 ? -static_cast<unsigned int>(value)
                               : static_cast<unsigned int>(value);

    auto reversed = std::string {};

    do
    {
        reversed += digits[magnitude & 0xF];
        magnitude >>= 4;
    } while (magnitude != 0);

    out.append(reversed.rbegin(), reversed.rend());
    return out;
}
} // namespace Doom
