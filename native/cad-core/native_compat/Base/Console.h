// Native stub for FreeCAD Base::Console — planegcs uses this for
// warning/diagnostic output.  Routed to stderr so solver diagnostics
// are visible in the core process log.
#ifndef PLANEGCS_NATIVE_BASE_CONSOLE_H
#define PLANEGCS_NATIVE_BASE_CONSOLE_H

#include <cstdarg>
#include <cstdio>

namespace Base {
class ConsoleClass {
public:
    static ConsoleClass& Instance() {
        static ConsoleClass instance;
        return instance;
    }
    void Warning(const char* format, ...) const {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
};
inline ConsoleClass& Console() {
    return ConsoleClass::Instance();
}
} // namespace Base

#endif
