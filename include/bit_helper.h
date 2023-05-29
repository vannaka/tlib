/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BIT_HELPER_H_
#define BIT_HELPER_H_

#include <stdint.h>

#include "infrastructure.h"

// Used by deposit32.
static inline uint64_t deposit64(uint64_t dst_val, uint8_t start, uint8_t length, uint64_t val);

/* Deposits 'length' bits from 'val' into 'dst_val' at 'start' bit. */
static inline uint32_t deposit32(uint32_t dst_val, uint8_t start, uint8_t length, uint32_t val)
{
    tlib_assert(start + length <= 32);
    return deposit64(dst_val, start, length, val);
}

/* Deposits 'length' bits from 'val' into 'dst_val' at 'start' bit. */
static inline uint64_t deposit64(uint64_t dst_val, uint8_t start, uint8_t length, uint64_t val)
{
    tlib_assert(start + length <= 64);

    // Number with only relevant bits ('start' to 'start+length-1') set.
    uint64_t relevant_bits = (UINT64_MAX >> (64 - length)) << start;

    // Shift value into the start bit and reset the irrelevant bits.
    val = (val << start) & relevant_bits;

    // Reset the relevant bits in the destination value.
    dst_val &= ~relevant_bits;

    // Return the above values merged.
    return dst_val | val;
}

/* Extracts 'length' bits of 'value' at 'start' bit. */
static inline uint32_t extract32(uint32_t value, uint8_t start, uint8_t length)
{
    tlib_assert(start + length <= 32);
    return (value >> start) & (UINT32_MAX >> (32 - length));
}

/* Extracts 'length' bits of 'value' at 'start' bit. */
static inline uint64_t extract64(uint64_t value, uint8_t start, uint8_t length)
{
    tlib_assert(start + length <= 64);
    return (value >> start) & (UINT64_MAX >> (64 - length));
}

static inline int32_t sextract32(uint32_t value, uint8_t start, uint8_t length)
{
    int32_t result = (int32_t)extract32(value, start, length);
    if (result >> (length - 1)) {
        // Set all the bits from 'length' to bit 31.
        result |= UINT32_MAX << length;
    }
    return result;
}

static inline int64_t sextract64(uint64_t value, uint8_t start, uint8_t length)
{
    int64_t result = (int64_t)extract64(value, start, length);
    if (result >> (length - 1)) {
        // Set all the bits from 'length' to bit 63.
        result |= UINT64_MAX << length;
    }
    return result;
}

static inline uint32_t rol32(uint32_t word, unsigned int shift)
{
    return (word << shift) | (word >> ((32 - shift) & 31));
}

static inline uint64_t rol64(uint64_t word, unsigned int shift)
{
    return (word << shift) | (word >> ((64 - shift) & 63));
}

static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
    return (word >> shift) | (word << ((32 - shift) & 31));
}

static inline uint64_t ror64(uint64_t word, unsigned int shift)
{
    return (word >> shift) | (word << ((64 - shift) & 63));
}

static inline uint32_t ctpop(uint32_t val) {
    int cnt = 0;
    while (val > 0) {
        cnt += val & 1;
        val >>= 1;
    }
    return cnt;
}

static inline uint8_t ctpop8(uint8_t val) {
    return (uint8_t)ctpop(val);
}

static inline uint16_t ctpop16(uint16_t val) {
    return (uint16_t)ctpop(val);
}

static inline uint32_t ctpop32(uint32_t val) {
    return (uint32_t)ctpop(val);
}

#endif  // BIT_HELPER_H_
