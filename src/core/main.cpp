#include "GenesisCore.hpp"
#include <iostream>

int main() {
    try {
        Genesis::GenesisCore engine;
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ENGINE FAILURE: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}