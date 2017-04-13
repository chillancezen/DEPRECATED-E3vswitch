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

#define MAKE_UINT32(hi,lo) ((((uint32_t)(hi))<<16)|(((uint32_t)(lo))&0xffff))
#define LOW_UINT32(v) ((uint16_t)((v)&0xffff))
#define HIGH_UINT32(v) ((uint16_t)(((v)>>16)&0xffff))

__attribute__((always_inline)) 
	static inline int is_ether_address_equal(void* a,void* b)
{
	return ((*(uint32_t*)(0+(uint8_t*)(a)))==(*(uint32_t*)(0+(uint8_t*)(b))))&& \
		((*(uint16_t*)(4+(uint8_t*)(a)))==(*(uint16_t*)(4+(uint8_t*)(b)))) ;
}
__attribute__((always_inline))
	static inline void copy_ether_address(void* dst,void * src)
{
	*(uint32_t*)(0+(uint8_t*)(dst))=*(uint32_t*)(0+(uint8_t*)(src)); 
	*(uint16_t*)(4+(uint8_t*)(dst))=*(uint16_t*)(4+(uint8_t*)(src)); 
}


#define VNI_SWAP_ORDER(vni) (((((uint32_t)(vni))>>16)&0xff)| \
	(((uint32_t)(vni))&0xff00)| \
	((((uint32_t)(vni))<<16)&0xff0000))

#endif