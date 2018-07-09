#include <c4/simd.hpp>

#include <array>
#include <vector>
#include <iostream>

#include <random>

using namespace std;
using namespace c4::simd;

// ===================================================== HELPERS ================================================================

template<class T>
T random() {
    static std::mt19937 mt;
    static std::uniform_int_distribution<long long> d;
    
    return (T)d(mt);
}

template<>
float random<float>() {
    static std::mt19937 mt;
    static std::uniform_real_distribution<float> d;

    return d(mt);
}

template<class T, int n>
array<T, n> random_array() {
    array<T, n> v;
    for (T& t : v)
        t = random<T>();
    return v;
}

template<class T>
T all_ones() {
    T t;
    memset(&t, -1, sizeof(t));
    return t;
}

class Exception : public std::runtime_error {
public:
    Exception(std::string msg, std::string filename, int line) : runtime_error(msg + " at " + filename + ":" + std::to_string(line)) {}
};

#define THROW_EXCEPTION(MSG) throw Exception(MSG, __FILE__, __LINE__)

#define ASSERT_TRUE(C) if( C ) {} else THROW_EXCEPTION("Runtime assertion failed: " #C)
#define ASSERT_EQUAL(A, B) if( (A) == (B) ) {} else THROW_EXCEPTION("Runtime assertion failed: " #A " == " #B ", " + to_string(A) + " != " + to_string(B))

// ====================================================== TESTS =================================================================

template<class T>
void test_cmpgt() {
    constexpr int n = 16 / sizeof(T);
    auto a = random_array<T, n>();
    auto b = random_array<T, n>();
    auto r = random_array<T, n>();

    auto va = load(a.data());
    auto vb = load(b.data());
    auto vr = cmpgt(va, vb);

    store(r.data(), vr);

    for (int i = 0; i < n; i++) {
        ASSERT_EQUAL(r[i], a[i] > b[i] ? all_ones<T>() : 0);
    }
}

void multitest_cmpgt() {
    test_cmpgt<int8_t>();
    test_cmpgt<uint8_t>();
    test_cmpgt<int16_t>();
    test_cmpgt<uint16_t>();
    test_cmpgt<int32_t>();
    test_cmpgt<uint32_t>();
}

// ======================================================= MAIN =================================================================

int main()
{
    const int n_steps = 1;

    for (int k = 0; k < n_steps; k++) {
        multitest_cmpgt();
    }
    return 0;
}

