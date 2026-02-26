#pragma once
#include <cmath>
#include <iostream>
#include <type_traits>

namespace Genesis {

    template<typename ST, typename LT, auto SectorSize> // SectorSize Must be same type as LT
    struct SectorFloat {
        ST sector = 0;
        LT local = 0;

        static_assert(std::is_same_v<decltype(SectorSize), LT>,
            "Genesis Error: SectorSize type must match Local Type (LT) as 'SectorSize' should also share the same units as the 'local' variable");

        void rebase() {
            LT offset = std::floor(local / SectorSize);
            sector += static_cast<ST>(offset);
            local -= offset * SectorSize;
        }

        // 1. Force Zeros: No arguments allowed
        // Usage: SectorFloat p;
        SectorFloat() : sector(0), local(0) {}

        SectorFloat(ST s, LT l) : sector(s), local(l) {
            rebase();
        }

        // Math operators...
        SectorFloat operator+(const SectorFloat& other) const {
            return SectorFloat(sector + other.sector, local + other.local);
        }

        SectorFloat operator-(const SectorFloat& other) const {
            return SectorFloat(sector - other.sector, local - other.local);
        }

        SectorFloat operator-() const {
            return SectorFloat(-sector, -local);
        }

        SectorFloat operator*(const float scalar) const {

            return SectorFloat(sector * scalar, local * scalar);
        }
    };

} // namespace Genesis