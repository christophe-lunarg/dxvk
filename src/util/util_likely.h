#pragma once

#ifdef __GNUC__
#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#define always_inline inline __attribute__((always_inline))
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#define always_inline inline
#endif
