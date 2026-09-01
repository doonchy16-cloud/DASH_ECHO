#include "TestHarness.hpp"

#include <exception>
#include <iostream>

int main() {
    int failures = 0;

    for (auto const& test : echo_test::registry()) {
        try {
            if (!test.function) {
                throw std::runtime_error("test function is null");
            }
            test.function();
            std::cout << "PASS " << test.name << '\n';
        } catch (std::exception const& error) {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "FAIL " << test.name << ": unknown exception\n";
        }
    }

    std::cout << "Tests: " << echo_test::registry().size()
              << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
