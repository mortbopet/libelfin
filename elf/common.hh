// Copyright (c) 2013 Austin T. Clements. All rights reserved.
// Use of this source code is governed by an MIT license
// that can be found in the LICENSE file.

#ifndef _ELFPP_COMMON_HH_
#define _ELFPP_COMMON_HH_

#define ELFPP_BEGIN_NAMESPACE namespace elf {
#define ELFPP_END_NAMESPACE   }
#define ELFPP_BEGIN_INTERNAL  namespace internal {
#define ELFPP_END_INTERNAL    }

#include <cstdint>

ELFPP_BEGIN_NAMESPACE

/**
 * A byte ordering.
 */
enum class byte_order
{
        native,
        lsb,
        msb
};

inline uint32_t bswap32_internal(uint32_t x) {
        return  ((x & 0xFF000000) >> 24u) |
                ((x & 0x00FF0000) >> 8u) |
                ((x & 0x0000FF00) << 8u) |
                ((x & 0xFF0000FF) << 24u);
}

inline uint64_t bswap64_internal(uint64_t x) {
        return
                ((x & 0xFF00000000000000u) >> 56u) |
                ((x & 0x00FF000000000000u) >> 40u) |
                ((x & 0x0000FF0000000000u) >> 24u) |
                ((x & 0x000000FF00000000u) >>  8u) |
                ((x & 0x00000000FF000000u) <<  8u) |      
                ((x & 0x0000000000FF0000u) << 24u) |
                ((x & 0x000000000000FF00u) << 40u) |
                ((x & 0x00000000000000FFu) << 56u);
}

/**
 * Return either byte_order::lsb or byte_order::msb.  If the argument
 * is byte_order::native, it will be resolved to whatever the native
 * byte order is.
 */
static inline byte_order
resolve_order(byte_order o)
{
        static const union
        {
                int i;
                char c[sizeof(int)];
        } test = {1};

        if (o == byte_order::native)
                return test.c[0] == 1 ? byte_order::lsb : byte_order::msb;
        return o;
}

/**
 * Return v converted from one byte order to another.
 */
template<typename T>
T
swizzle(T v, byte_order from, byte_order to)
{
        static_assert(sizeof(T) == 1 ||
                      sizeof(T) == 2 ||
                      sizeof(T) == 4 ||
                      sizeof(T) == 8,
                      "cannot swizzle type");

        from = resolve_order(from);
        to = resolve_order(to);

        if (from == to)
                return v;

        switch (sizeof(T)) {
        case 1:
                return v;
        case 2: {
                std::uint16_t x = (std::uint16_t)v;
                return (T)(((x&0xFF) << 8) | (x >> 8));
        }
        case 4:
                return (T)bswap32_internal((std::uint32_t)v);
        case 8:
                return (T)bswap64_internal((std::uint64_t)v);
        }
}

ELFPP_BEGIN_INTERNAL

/**
 * OrderPick selects between Native, LSB, and MSB based on ord.
 */
template<byte_order ord, typename Native, typename LSB, typename MSB>
struct OrderPick;

template<typename Native, typename LSB, typename MSB>
struct OrderPick<byte_order::native, Native, LSB, MSB>
{
        typedef Native T;
};

template<typename Native, typename LSB, typename MSB>
struct OrderPick<byte_order::lsb, Native, LSB, MSB>
{
        typedef LSB T;
};

template<typename Native, typename LSB, typename MSB>
struct OrderPick<byte_order::msb, Native, LSB, MSB>
{
        typedef MSB T;
};

ELFPP_END_INTERNAL

ELFPP_END_NAMESPACE

#endif
