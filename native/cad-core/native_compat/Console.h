#ifndef PLANEGCS_NATIVE_CONSOLE_H
#define PLANEGCS_NATIVE_CONSOLE_H

#include <cstdio>
#include <cstdarg>

// Native replacement for the Emscripten-based Console.h.
// planegcs uses Console::Log for debug/diagnostic output; on native
// builds we route it through printf so solver diagnostics are visible
// in the core process stderr.

class Console {
public:
    static void Log(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
};

#endif // PLANEGCS_NATIVE_CONSOLE_H
