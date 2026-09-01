#pragma once

#include <cstdint>

namespace dash_echo {

struct AttemptId {
    std::uint64_t value = 0;
    bool operator==(AttemptId const&) const = default;
};

struct FrameSequence {
    std::uint64_t value = 0;
    bool operator==(FrameSequence const&) const = default;
};

struct ReplayTime {
    double seconds = 0.0;
    bool operator==(ReplayTime const&) const = default;
};

struct ProgressPercent {
    float value = 0.0f;
    bool operator==(ProgressPercent const&) const = default;
};

struct NormalizedCursor {
    float value = 0.0f;
    bool operator==(NormalizedCursor const&) const = default;
};

} // namespace dash_echo
