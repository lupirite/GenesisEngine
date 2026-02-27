#include <iostream>

#include <iomanip>// show more precision

#include "GenesisMath.hpp"
#include "GenesisEditor.hpp"
using namespace Genesis;

struct SectorSize {
    static constexpr double sectorSize = 1000.0;
};

int main() {
    // Test our N-dimensional Vector
    dvec3 myPos(1.0, 2.0, 3.0);

    std::cout << std::setprecision(17) << std::fixed;



    // This should WORK: ST is int64_t, LT is double, SectorSize is double
    SectorFloat<int64_t, double, SectorSize> correct_pos;

    // This should FAIL at compile time: SectorSize is an 'int', but LT is 'double'
    // Genesis::SectorFloat<int64_t, double, 1000> broken_pos;

    std::cout << "Vector length: " << myPos.length() << std::endl;

    // Test the global length function (GLSL style)
    std::cout << "GLSL-style length: " << length(myPos) << std::endl;

    return 0;
}