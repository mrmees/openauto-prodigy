#pragma once

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
#include <boost/log/trivial.hpp>

namespace oap {
namespace aa {

struct PerfStats {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct Metric {
        double sum = 0.0;
        double min = 1e9;
        double max = 0.0;
        uint64_t count = 0;

        void record(double ms) {
            sum += ms;
            min = std::min(min, ms);
            max = std::max(max, ms);
            ++count;
        }

        double avg() const { return count > 0 ? sum / count : 0.0; }

        void reset() {
            sum = 0.0;
            min = 1e9;
            max = 0.0;
            count = 0;
        }
    };

    static double msElapsed(TimePoint start, TimePoint end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

} // namespace aa
} // namespace oap
