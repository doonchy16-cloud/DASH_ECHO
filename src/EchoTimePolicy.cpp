#include "EchoTimePolicy.hpp"

#include <algorithm>
#include <cmath>

namespace dash_echo {

double sanitizeDeltaSeconds(double dt, double maximum) {
    if (!std::isfinite(dt) || !std::isfinite(maximum)) return 0.0;
    if (dt <= 0.0 || maximum <= 0.0) return 0.0;
    return std::min(dt, maximum);
}

} // namespace dash_echo
