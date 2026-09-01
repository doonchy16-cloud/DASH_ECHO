#include "TestHarness.hpp"
#include "EchoRecorder.hpp"

ECHO_TEST(current_recorder_limits_are_preserved) {
    ECHO_CHECK(dash_echo::EchoRecorder::kMinCaptureSampleRate == 30.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kDefaultCaptureSampleRate == 120.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kMaxCaptureSampleRate == 240.0);
}
