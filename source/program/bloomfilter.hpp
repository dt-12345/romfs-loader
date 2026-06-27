#pragma once

#include "murmur3.hpp"

#include <array>
#include <bitset>
#include <cstdint>
#include <string_view>

// https://michaelschmatz.com/posts/how-to-write-a-bloom-filter-cpp/
template <std::size_t FILTER_SIZE, std::size_t HASH_FUNCTION_COUNT>
class BloomFilter {
public:
    BloomFilter() = default;
    ~BloomFilter() = default;

    void reset() {
        mFilter.reset();
    }

    void add(std::string_view data) {
        const auto hash = CalcHash(data);

        for (std::size_t n = 0; n < HASH_FUNCTION_COUNT; ++n) {
            mFilter.set(BitIndex(n, hash[0], hash[1]));
        }
    }

    bool contains(std::string_view data) const {
        const auto hash = CalcHash(data);

        for (std::size_t n = 0; n < HASH_FUNCTION_COUNT; ++n) {
            if (!mFilter.test(BitIndex(n, hash[0], hash[1]))) {
                return false;
            }
        }

        return true;
    }

private:
    [[gnu::always_inline]] static std::uint64_t BitIndex(std::size_t n, std::uint64_t hashA, std::uint64_t hashB) {
        return (hashA + n * hashB) % FILTER_SIZE;
    }

    [[gnu::always_inline]] static std::array<std::uint64_t, 2> CalcHash(std::string_view data) {
        std::array<std::uint64_t, 2> hash{};
        MurmurHash3_x64_128(data.data(), data.size(), 0, hash.data());
        return hash;
    }

    std::bitset<FILTER_SIZE> mFilter;
};