#include <iostream>
#include <stdexcept>
#include <string_view>

void check(bool condition, std::string_view description) {
    if (!condition) throw std::runtime_error(std::string(description));
}
int main() {
    try {
        check(true, "runner smoke check");
        std::cout << "All checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
