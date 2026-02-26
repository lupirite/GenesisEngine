#include <iostream>

#include <iomanip>// show more precision

#include "math/Vector.hpp" // Ensure this path matches your folder structure
using namespace Genesis;

int main() {
    // Test our N-dimensional Vector
    dvec3 myPos(1.0, 2.0, 3.0);

    double gooberism = 4.2000000000023;
    float test = static_cast<float>(gooberism);


    std::cout << std::setprecision(17) << std::fixed;

    std::cout << gooberism << std::endl;
    std::cout << test << std::endl;

    std::cout << "Vector length: " << myPos.length() << std::endl;

    // Test the global length function (GLSL style)
    std::cout << "GLSL-style length: " << Genesis::length(myPos) << std::endl;

    return 0;
}