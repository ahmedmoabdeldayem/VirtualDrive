/* cc.h — compiler compatibility macros for rt-labs SOES on Linux/GCC */
#pragma once

#define CC_PACKED               __attribute__((packed))
#define CC_PACKED_BEGIN
#define CC_PACKED_END
#define CC_STATIC_ASSERT(e)     _Static_assert(e, #e)
#define CC_INLINE               static inline __attribute__((always_inline))
#define CC_UNUSED(x)            (void)(x)
#define CC_ASSERT(e)            do { if (!(e)) { __builtin_trap(); } } while (0)
#define CC_ALIGNED(n)           __attribute__((aligned(n)))
#define BIT(n)                  (1U << (n))
#define NELEMENTS(a)            (sizeof(a) / sizeof((a)[0]))
