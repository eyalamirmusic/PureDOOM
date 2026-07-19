#pragma once

// Per-compiler warning suppression, spelled once.
//
// A suppression is written in one compiler's dialect and does nothing at all in
// another's - and it fails in the direction that looks clean, which is the
// expensive direction. This repository has been bitten by that twice:
//
//   - Two generated tables carried `#pragma GCC diagnostic ignored
//     "-Wwritable-strings"`, which is Clang's name for the flag. Clang went
//     quiet, GCC did not recognise the option, warned about *that*, and then
//     emitted 314 warnings the other compiler had never shown (REFACTOR.md
//     item 7).
//   - Three tables here carried a bare `#pragma GCC diagnostic`, which MSVC does
//     not understand at all. It reported 16 C4068 "unknown pragma" and applied
//     no suppression - so whatever the pragma was hiding on GCC had simply never
//     been measured on MSVC.
//
// So a call site says which warning it wants gone, not which compiler it thinks
// it is talking to. On a compiler with no equivalent the macro expands to
// nothing, which is the honest answer rather than a silent misfire.
//
// Suppress the narrowest thing that works, around the smallest scope that works,
// and prefer fixing the code: a pragma here is a statement that the warning is
// wrong about this specific site, and that claim goes stale without telling you.

#if defined(__clang__) || defined(__GNUC__)

#define DOOM_PRAGMA(x) _Pragma(#x)

#define DOOM_DIAGNOSTIC_PUSH DOOM_PRAGMA(GCC diagnostic push)
#define DOOM_DIAGNOSTIC_POP DOOM_PRAGMA(GCC diagnostic pop)

#define DOOM_IGNORE_MISSING_FIELD_INITIALIZERS                                      \
    DOOM_PRAGMA(GCC diagnostic ignored "-Wmissing-field-initializers")

// Clang split -Wcast-function-type-mismatch out of -Wcast-function-type in
// Clang 19. Naming the narrower one unconditionally would raise
// -Wunknown-warning-option on every Clang older than that, so ask rather than
// assume; __has_warning is a Clang extension, hence the nesting.
#if defined(__clang__)
#if __has_warning("-Wcast-function-type-mismatch")
#define DOOM_IGNORE_CAST_FUNCTION_TYPE                                              \
    DOOM_PRAGMA(clang diagnostic ignored "-Wcast-function-type-mismatch")
#else
#define DOOM_IGNORE_CAST_FUNCTION_TYPE                                              \
    DOOM_PRAGMA(clang diagnostic ignored "-Wcast-function-type")
#endif
#else
#define DOOM_IGNORE_CAST_FUNCTION_TYPE                                              \
    DOOM_PRAGMA(GCC diagnostic ignored "-Wcast-function-type")
#endif

#else // MSVC, which raises none of these at /W4.

#define DOOM_DIAGNOSTIC_PUSH
#define DOOM_DIAGNOSTIC_POP
#define DOOM_IGNORE_MISSING_FIELD_INITIALIZERS
#define DOOM_IGNORE_CAST_FUNCTION_TYPE

#endif
