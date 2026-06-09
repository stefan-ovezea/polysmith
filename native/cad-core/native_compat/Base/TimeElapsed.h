// Native stub for FreeCAD Base::TimeElapsed — planegcs uses this for
// solver timing diagnostics in GCS.cpp.
#ifndef PLANEGCS_NATIVE_BASE_TIMEELAPSED_H
#define PLANEGCS_NATIVE_BASE_TIMEELAPSED_H

#include <chrono>

namespace Base {
class TimeElapsed {
    std::chrono::high_resolution_clock::time_point _tp;
public:
    TimeElapsed() : _tp(std::chrono::high_resolution_clock::now()) {}
    static double diffTimeF(const TimeElapsed& t1, const TimeElapsed& t2) {
        return std::chrono::duration<double>(t2._tp - t1._tp).count();
    }
};
} // namespace Base

#endif
