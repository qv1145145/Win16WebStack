/* gcc_compat.h - GCC extensions compatibility for Open Watcom */

#ifndef GCC_COMPAT_H
#define GCC_COMPAT_H

#ifdef __WATCOMC__

#include <stdint.h>

/* Ignore GCC attributes */
#define __attribute__(x)

/* likely/unlikely macros */
#define likely(x)   (x)
#define unlikely(x) (x)
#define __builtin_expect(x, y) (x)

/* Fallback implementations for GCC built-ins (using uint32_t/uint64_t to avoid 16-bit shift issues) */
static inline int __builtin_clz(uint32_t x) {
    if (x == 0) return 32;
    int n = 0;
    if ((x & 0xFFFF0000UL) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF000000UL) == 0) { n += 8;  x <<= 8; }
    if ((x & 0xF0000000UL) == 0) { n += 4;  x <<= 4; }
    if ((x & 0xC0000000UL) == 0) { n += 2;  x <<= 2; }
    if ((x & 0x80000000UL) == 0) { n += 1; }
    return n;
}

static inline int __builtin_clzll(uint64_t x) {
    if (x == 0) return 64;
    int n = 0;
    if ((x & 0xFFFFFFFF00000000ULL) == 0) { n += 32; x <<= 32; }
    if ((x & 0xFFFF000000000000ULL) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF00000000000000ULL) == 0) { n += 8;  x <<= 8; }
    if ((x & 0xF000000000000000ULL) == 0) { n += 4;  x <<= 4; }
    if ((x & 0xC000000000000000ULL) == 0) { n += 2;  x <<= 2; }
    if ((x & 0x8000000000000000ULL) == 0) { n += 1; }
    return n;
}

static inline int __builtin_ctz(uint32_t x) {
    if (x == 0) return 32;
    int n = 1;
    if ((x & 0x0000FFFFUL) == 0) { n += 16; x >>= 16; }
    if ((x & 0x000000FFUL) == 0) { n += 8;  x >>= 8; }
    if ((x & 0x0000000FUL) == 0) { n += 4;  x >>= 4; }
    if ((x & 0x00000003UL) == 0) { n += 2;  x >>= 2; }
    if ((x & 0x00000001UL) == 0) { n += 1; }
    return n - 1;
}

static inline int __builtin_ctzll(uint64_t x) {
    if (x == 0) return 64;
    int n = 1;
    if ((x & 0x00000000FFFFFFFFULL) == 0) { n += 32; x >>= 32; }
    if ((x & 0x000000000000FFFFULL) == 0) { n += 16; x >>= 16; }
    if ((x & 0x00000000000000FFULL) == 0) { n += 8;  x >>= 8; }
    if ((x & 0x000000000000000FULL) == 0) { n += 4;  x >>= 4; }
    if ((x & 0x0000000000000003ULL) == 0) { n += 2;  x >>= 2; }
    if ((x & 0x0000000000000001ULL) == 0) { n += 1; }
    return n - 1;
}

#endif /* __WATCOMC__ */

#endif /* GCC_COMPAT_H */

