#pragma once
#include <cmath>
#include <array>
#include <iostream>

namespace Genesis {

    template<typename T, size_t N>
    struct Vector {
        std::array<T, N> data;

        // Default constructor (zeros)
        Vector() { data.fill(0); }

        // Variadic constructor: allows Vector<float, 3> v(1.0, 2.0, 3.0);
        template<typename... Args>
        Vector(Args... args) : data{args...} {}

        // Overload [] for easy access: v[0] = 10.0;
        T& operator[](size_t i) { return data[i]; }
        const T& operator[](size_t i) const { return data[i]; }

        // Magnitude (Length)
        T length() const {
            T sum = 0;
            for (size_t i = 0; i < N; ++i) sum += data[i] * data[i];
            return std::sqrt(sum);
        }
    };

    // Aliases for common types
    using dvec3 = Vector<double, 3>;
    using dvec4 = Vector<double, 4>;

    // Global math functions to match GLSL
    template<typename T, size_t N>
    T length(const Vector<T, N>& v) {
        return v.length();
    }

} // namespace Genesis