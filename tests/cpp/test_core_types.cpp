#include "TestHarness.hpp"
#include "EchoCoreTypes.hpp"

#include <type_traits>

ECHO_TEST(core_semantic_types_are_not_implicitly_interchangeable) {
    ECHO_CHECK((!std::is_convertible_v<dash_echo::AttemptId, dash_echo::FrameSequence>));
    ECHO_CHECK((!std::is_convertible_v<dash_echo::FrameSequence, dash_echo::AttemptId>));
    ECHO_CHECK((!std::is_convertible_v<dash_echo::ReplayTime, dash_echo::NormalizedCursor>));
}

ECHO_TEST(core_semantic_types_preserve_explicit_values) {
    dash_echo::AttemptId const attempt {42};
    dash_echo::FrameSequence const sequence {7};
    dash_echo::ReplayTime const time {1.25};
    dash_echo::ProgressPercent const progress {83.5f};
    dash_echo::NormalizedCursor const cursor {0.75f};

    ECHO_CHECK(attempt.value == 42);
    ECHO_CHECK(sequence.value == 7);
    ECHO_CHECK(time.seconds == 1.25);
    ECHO_CHECK(progress.value == 83.5f);
    ECHO_CHECK(cursor.value == 0.75f);
}
