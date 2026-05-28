#include "Core.hpp"
#include <iostream>

int main() {
    try {
        Genesis::Core engine;
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ENGINE FAILURE: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}