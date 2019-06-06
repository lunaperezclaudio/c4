//MIT License
//
//Copyright(c) 2018 Alex Kasitskyi
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
#include <type_traits>

#include "exception.hpp"

namespace c4 {
    template<typename T>
    inline T pi() {
        static const T __pi = 2 * acos((T)0);
        return __pi;
    }

    template<typename T>
    inline T abs_or_rel_error(T x, T x0) {
        T abs_err = std::abs(x - x0);
        return abs_err / std::max(std::abs(x0), (T)1);
    }

    template<typename T>
    inline bool almost_equal(T x, T x0, T eps) {
        T r = abs_or_rel_error(x, x0);

        return r < eps;
    }

    template<typename T>
    inline auto sqr(T q) -> decltype(q * q) {
        return q * q;
    }

    template<typename T>
    inline int sign(T x) {
        constexpr T zero = T(0);

        if (x == zero)
            return 0;

        return x  > zero ? 1 : -1;
    }

    template<typename T>
    inline T signed_sqrt(T x) {
        return x >= 0 ? sqrt(x) : -sqrt(-x);
    }

    inline float logistic_function(float k, float x0, float x) {
        return 1.f / (1.f + std::exp(-k * (x - x0)));
    }

    template<class T1, class T2>
    inline typename std::enable_if<std::is_floating_point<T1>::value || std::is_floating_point<T2>::value, bool>::type safe_less(T1 a, T2 b) {
        return a < b;
    }

    template<class T1, class T2>
    inline typename std::enable_if<!std::is_floating_point<T1>::value && !std::is_floating_point<T2>::value && std::is_signed<T1>::value == std::is_signed<T2>::value, bool>::type safe_less(T1 a, T2 b) {
        return a < b;
    }

    template<class T1, class T2>
    inline typename std::enable_if<!std::is_floating_point<T1>::value && !std::is_floating_point<T2>::value && std::is_signed<T1>::value && std::is_unsigned<T2>::value, bool>::type safe_less(T1 a, T2 b) {
        if (a < 0)
            return true;
        else
            return static_cast<typename std::make_unsigned<T1>::type>(a) < b;
    }

    template<class T1, class T2>
    inline typename std::enable_if<!std::is_floating_point<T1>::value && !std::is_floating_point<T2>::value && std::is_unsigned<T1>::value && std::is_signed<T2>::value, bool>::type safe_less(T1 a, T2 b) {
        if (b < 0)
            return false;
        else
            return a < static_cast<typename std::make_unsigned<T1>::type>(b);
    }

    template<class T1, class T2>
    inline bool safe_greater(T1 a, T2 b) {
        return safe_less(b, a);
    }

    template<class T1, class T2>
    inline bool safe_less_equal(T1 a, T2 b) {
        return !safe_greater(a, b);
    }

    template<class T1, class T2>
    inline bool safe_greater_equal(T1 a, T2 b) {
        return !safe_less(a, b);
    }

    template<class dst_t, class src_t>
    inline bool fits_within(src_t x) {
        return safe_less_equal(std::numeric_limits<dst_t>::lowest(), x) && safe_less_equal(x, std::numeric_limits<dst_t>::max());
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if<std::is_arithmetic<src_t>::value && std::is_arithmetic<dst_t>::value, dst_t>::type clamp(src_t x, dst_t x0, dst_t x1) {
        if (x < x0)
            return x0;
        if (x > x1)
            return x1;

        return (dst_t)x;
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if<std::is_same<src_t, dst_t>::value, dst_t>::type clamp(src_t x) {
        return x;
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if<!std::is_same<src_t, dst_t>::value && std::is_floating_point<dst_t>::value, dst_t>::type clamp(src_t x) {
        return (dst_t)x;
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if<!std::is_same<src_t, dst_t>::value && std::is_integral<src_t>::value && std::is_same<dst_t, uint8_t>::value, uint8_t>::type clamp(src_t x) {
        return uint8_t((x & ~255) == 0 ? x : (x < 0 ? 0 : 255));
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if<!std::is_same<src_t, dst_t>::value && std::is_integral<src_t>::value && std::is_same<dst_t, int8_t>::value, int8_t>::type clamp(src_t x) {
        return int8_t(clamp<uint8_t>(x + 128) - 128);
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if<!std::is_same<src_t, dst_t>::value && std::is_floating_point<src_t>::value && (std::is_same<dst_t, uint8_t>::value || std::is_same<dst_t, int8_t>::value), dst_t>::type clamp(src_t x) {
        return clamp<dst_t>((int)x);
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if<!std::is_same<src_t, dst_t>::value && std::is_integral<dst_t>::value && !std::is_same<dst_t, uint8_t>::value && !std::is_same<dst_t, int8_t>::value, dst_t>::type clamp(src_t x) {
        return clamp(x, std::numeric_limits<dst_t>::lowest(), std::numeric_limits<dst_t>::max());
    }


    template<class dst_t, class src_t>
    inline typename std::enable_if_t<std::is_integral<dst_t>::value && std::is_floating_point<src_t>::value, dst_t> round(src_t x) {
        return x >= 0 ? dst_t(x + 0.5f) : dst_t(x - 0.5f);
    }

    template<class dst_t, class src_t>
    inline typename std::enable_if_t<!std::is_integral<dst_t>::value || !std::is_floating_point<src_t>::value, dst_t> round(src_t x) {
        return dst_t(x);
    }


    template<class T>
    inline int clz(T x) {
        constexpr int n = 8 * sizeof(T);
        constexpr T mask = T(1ll << (n - 1));

        int r = 0;
            
        for (; r < n && !(x & mask); r++) {
            x <<= 1;
        }

        return r;
    }

    template<typename T1, typename T2>
    double mean_squared_error(const std::vector<T1>& a, const std::vector<T2>& b) {
        ASSERT_EQUAL(a.size(), b.size());

        if (isize(a) == 0)
            return 0.;

        double s = 0.;

        for (int i : c4::range(a)) {
            s += sqr(a[i] - b[i]);
        }

        return s / a.size();
    }

    // https://en.wikipedia.org/wiki/Linear_congruential_generator
    class fast_rand {
        inline uint32_t f(uint32_t seed) const {
            return 214013u * seed + 2531011u;
        }

        int seed;
    public:
        fast_rand(uint32_t seed = 0) : seed(seed) {}

        inline uint32_t operator()() {
            seed = f(seed);
            return seed;
        }

        inline uint32_t min() const {
            return std::numeric_limits<uint32_t>::min();
        }

        inline uint32_t max() const {
            return std::numeric_limits<uint32_t>::max();
        }
    };

    class fast_rand_float_uniform {
        fast_rand rnd;
        const float c;
        const float k;
    public:
        fast_rand_float_uniform(float from = 0.f, float to = 1.f, uint32_t seed = 0) : rnd(seed), c(from), k((to - from) / std::numeric_limits<uint32_t>::max()) {}

        inline float operator()() {
            return rnd() * k + c;
        }
    };

    class fast_rand_float_normal {
        fast_rand_float_uniform rnd;
        float store;
        bool have;
    public:
        fast_rand_float_normal(uint32_t seed = 0) : rnd(-1.f, 1.f, seed), store(0.f), have(false) {}

        inline float operator()() {
            if (have) {
                have = false;
                return store;
            }

            float x, y, r2;
            do {
                x = rnd();
                y = rnd();
                r2 = x * x + y * y;
            } while (r2 > 1.f);

            float t = std::sqrt(-2.f * std::log(r2) / (r2));
            have = true;
            store = t * x;
            return t * y;
        }
    };
}; // namespace c4
