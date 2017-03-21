#ifndef _UTIL_H
#define _UTIL_H

#ifndef typeof
#define typeof __typeof__
#endif


#ifndef offsetof
#define offsetof(t, m) ((size_t) &((t *)0)->m)
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({ \
		typeof(((type *)0)->member)(*__mptr) = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); })
#endif

#ifndef PREDICT_TRUE
#define PREDICT_TRUE(exp) __builtin_expect(!!(exp),1)
#endif

#ifndef PREDICT_FALSE
#define PREDICT_FALSE(exp) __builtin_expect(!!(exp),0)
#endif 

#define E3_MAX(a,b) (((a)>(b))?(a):(b))
#define E3_MIN(a,b) (((a)<(b))?(a):(b))

#define MAKE_UINT64(hi,lo)  ((((uint64_t)(hi))<<32)|(((uint64_t)(lo))&0xffffffff))
#define HIGH_UINT64(v) (((uint64_t)(v)>>32)&0xffffffff)
#define LOW_UINT64(v) (((uint64_t)(v))&0xffffffff)

#endif