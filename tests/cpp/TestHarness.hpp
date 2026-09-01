#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace echo_test {

using TestFunction = void (*)();

struct TestCase {
    std::string name;
    TestFunction function = nullptr;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> value;
    return value;
}

struct Register {
    Register(char const* name, TestFunction function) {
        registry().push_back({name, function});
    }
};

inline void check(bool ok, char const* expression, char const* file, int line) {
    if (!ok) {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) +
            " CHECK failed: " + expression
        );
    }
}

} // namespace echo_test

#define ECHO_TEST(name) \
    static void name(); \
    static echo_test::Register name##_register(#name, &name); \
    static void name()

#define ECHO_CHECK(expression) \
    echo_test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
