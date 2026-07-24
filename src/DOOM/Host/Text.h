#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include "Platform.h"

namespace Doom
{
inline void appendText(std::string& out, std::string_view part)
{
    out += part;
}

inline void appendText(std::string& out, char part)
{
    out += part;
}

template <typename T>
    requires std::is_integral_v<T>
void appendText(std::string& out, T part)
{
    out += std::to_string(part);
}

template <typename... Parts>
std::string concat(const Parts&... parts)
{
    auto out = std::string {};
    (appendText(out, parts), ...);
    return out;
}

// Uppercase, magnitude-with-sign — what the retired doom_itoa(value, 16) printed.
std::string hexString(int value);

// ASCII-only, as the engine has always compared names: no locale, no UTF-8.
constexpr char toUpper(char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

// For the key-code path, which upper-cases values a char cannot hold.
constexpr int toUpper(int c)
{
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

// What the retired doom_atoi did, exactly: a bare digit accumulator - no sign
// handling, no whitespace skip. Every value it parses (config numbers, argv
// counts) is unsigned decimal, and the goldens pin the behaviour.
constexpr int parseInt(std::string_view text)
{
    int value = 0;

    for (auto c: text)
    {
        value *= 10;
        value += c - '0';
    }

    return value;
}

// The retired doom_atox: upper-case hex digits only, as the config writes them.
constexpr int parseHex(std::string_view text)
{
    int value = 0;

    for (auto c: text)
    {
        value *= 16;

        if (c >= '0' && c <= '9')
            value += c - '0';
        else
            value += c - 'A' + 10;
    }

    return value;
}

constexpr bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;

    for (std::size_t i = 0; i < a.size(); ++i)
        if (toUpper(a[i]) != toUpper(b[i]))
            return false;

    return true;
}

template <typename... Parts>
void printTo(void* handle, const Parts&... parts)
{
    auto text = concat(parts...);
    host().write(handle, text.data(), static_cast<int>(text.size()));
}

// Writes text into a fixed-width, zero-padded field (the savegame's description
// and version fields are fixed-size on disk). Truncates to size; the padding is
// what keeps the written bytes deterministic.
inline void fillField(void* destination, int size, std::string_view text)
{
    auto* bytes = static_cast<char*>(destination);
    auto length = std::min(static_cast<int>(text.size()), size);

    for (auto i = 0; i < length; ++i)
        bytes[i] = text[i];

    for (auto i = length; i < size; ++i)
        bytes[i] = 0;
}

// A view of a fixed-width name field that may not be NUL-terminated (WAD lump
// and texture names are 8 bytes, padded with NULs only when short): up to size
// characters, stopping at the first NUL.
inline std::string_view nameView(const char* text, int size)
{
    auto length = 0;

    while (length < size && text[length] != '\0')
        ++length;

    return {text, static_cast<std::size_t>(length)};
}

template <typename... Parts>
void print(const Parts&... parts)
{
    host().print(concat(parts...));
}
} // namespace Doom
