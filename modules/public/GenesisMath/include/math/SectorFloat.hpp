#pragma once
#include <cmath>
#include <iostream>
#include <type_traits>

namespace Genesis {

    template<typename ST, typename LT, typename SectorSize> // SectorSize Must be same type as LT
    struct SectorFloat {
        ST sector = 0;
        LT local = 0;

        // We use std::remove_cvref_t to strip 'const' so 'const double' matches 'double'
        static_assert(std::is_same_v<std::remove_cvref_t<decltype(SectorSize::sectorSize)>, LT>,
            "Genesis Error: SectorSize type must match Local Type (LT)...");

        void rebase() {
            LT offset = std::floor(local / SectorSize::sectorSize);
            sector += static_cast<ST>(offset);
            local -= offset * SectorSize::sectorSize;
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