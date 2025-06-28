#ifndef SIGHTREAD_DETAIL_UTILS_HPP
#define SIGHTREAD_DETAIL_UTILS_HPP

#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>

#include "sightread/tempomap.hpp"

[[noreturn]] inline void throw_on_insufficient_bytes()
{
    throw SightRead::ParseError("insufficient bytes");
}

template <typename T>
T read_four_byte_be(std::span<const std::uint8_t> span, std::size_t offset)
{
    static_assert(sizeof(T) == 4);

    if (span.size() < offset + 4) {
        throw_on_insufficient_bytes();
    }
    const auto bytes = span[offset] << (3 * CHAR_BIT)
        | span[offset + 1] << (2 * CHAR_BIT) | span[offset + 2] << CHAR_BIT
        | span[offset + 3];
    return std::bit_cast<T>(bytes);
}

template <typename T>
T read_four_byte_le(std::span<const std::uint8_t> span, std::size_t offset)
{
    static_assert(sizeof(T) == 4);

    if (span.size() < offset + 4) {
        throw_on_insufficient_bytes();
    }
    const auto bytes = span[offset] | span[offset + 1] << CHAR_BIT
        | span[offset + 2] << (2 * CHAR_BIT)
        | span[offset + 3] << (3 * CHAR_BIT);
    return std::bit_cast<T>(bytes);
}

template <typename T>
T read_two_byte_be(std::span<const std::uint8_t> span, std::size_t offset)
{
    static_assert(sizeof(T) == 2);

    if (span.size() < offset + 2) {
        throw_on_insufficient_bytes();
    }
    const auto bytes = static_cast<std::int16_t>(span[offset] << CHAR_BIT
                                                 | span[offset + 1]);
    return std::bit_cast<T>(bytes);
}

template <typename T>
T read_two_byte_le(std::span<const std::uint8_t> span, std::size_t offset)
{
    static_assert(sizeof(T) == 2);

    if (span.size() < offset + 2) {
        throw_on_insufficient_bytes();
    }
    const auto bytes = static_cast<std::int16_t>(
        span[offset] | span[offset + 1] << CHAR_BIT);
    return std::bit_cast<T>(bytes);
}

inline std::uint8_t pop_front(std::span<const std::uint8_t>& data)
{
    if (data.empty()) {
        throw_on_insufficient_bytes();
    }
    const auto value = data.front();
    data = data.subspan(1);
    return value;
}

#endif
