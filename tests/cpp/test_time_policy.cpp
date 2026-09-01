#include "TestHarness.hpp"
#include "EchoTimePolicy.hpp"

#include <limits>

ECHO_TEST(delta_time_policy_accepts_normal_values) {
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(0.125) == 0.125);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(0.25) == 0.25);
}

ECHO_TEST(delta_time_policy_rejects_invalid_values) {
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(-1.0) == 0.0);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(std::numeric_limits<double>::quiet_NaN()) == 0.0);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(std::numeric_limits<double>::infinity()) == 0.0);
}

ECHO_TEST(delta_time_policy_clamps_spikes_and_invalid_maximum) {
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(1.0) == 0.25);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(0.10, 0.05) == 0.05);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(0.10, 0.0) == 0.0);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(0.10, -1.0) == 0.0);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(0.10, std::numeric_limits<double>::quiet_NaN()) == 0.0);
}
