#pragma once

namespace dash_echo {

[[nodiscard]] double sanitizeDeltaSeconds(double dt, double maximum = 0.25);

} // namespace dash_echo
