#pragma once


#if defined(WIN32)
#define DOOM_WIN32
#elif defined(__APPLE__)
#define DOOM_APPLE
#else
#define DOOM_LINUX
#endif


#include "../DOOM.h"


// A template rather than a macro so it works on Doom::Fixed as well as on ints
// (and evaluates its argument once, which the macro did not).
template <typename T>
constexpr T doom_abs(T x)
{
    return x < T {} ? -x : x;
}


extern char error_buf[260];
extern int doom_flags;

// The 13 host callbacks are references onto Doom::host()'s members (Host/Host.h) - the
// storage moved off these loose globals into one Host owner (REFACTOR.md, the
// doom_config->Host fold). Every reader resolves through the reference unchanged.
extern doom_print_fn& doom_print;
extern doom_malloc_fn& doom_malloc;
extern doom_free_fn& doom_free;
extern doom_open_fn& doom_open;
extern doom_close_fn& doom_close;
extern doom_read_fn& doom_read;
extern doom_write_fn& doom_write;
extern doom_seek_fn& doom_seek;
extern doom_tell_fn& doom_tell;
extern doom_eof_fn& doom_eof;
extern doom_gettime_fn& doom_gettime;
extern doom_exit_fn& doom_exit;
extern doom_getenv_fn& doom_getenv;


const char* doom_itoa(int i, int radix);
const char* doom_ctoa(char c);
const char* doom_ptoa(void* p);
void doom_memset(void* ptr, int value, int num);
void* doom_memcpy(void* destination, const void* source, int num);
int doom_fprint(void* handle, const char* str);
int doom_strlen(const char* str);
char* doom_concat(char* dst, const char* src);
char* doom_strcpy(char* destination, const char* source);
char* doom_strncpy(char* destination, const char* source, int num);
int doom_strcmp(const char* str1, const char* str2);
int doom_strncmp(const char* str1, const char* str2, int n);
int doom_strcasecmp(const char* str1, const char* str2);
int doom_strncasecmp(const char* str1, const char* str2, int n);
int doom_atoi(const char* str);
int doom_atox(const char* str);
int doom_toupper(int c);


