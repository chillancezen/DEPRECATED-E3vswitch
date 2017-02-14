#ifndef _E3_BITMAP_H
#define _E3_BITMAP_H


typedef long long e3_bitmap;

#define e3_bitmap_init(bm) {\
	(bm)=0;\
}

#define e3_bitmap_first_set_bit_to_index(bm) (__builtin_ffsll(bm)-1)
#define e3_bitmap_set_bit(bm,idx) ((bm)=(bm)|((e3_bitmap)1)<<(idx))
#define e3_bitmap_clear_bit(bm,idx) ((bm)=(bm)&~(((e3_bitmap)1)<<(idx)))
#define e3_bitmap_is_bit_set(bm,idx) (!!((bm)&(((e3_bitmap)1)<<(idx))))
#define e3_bitmap_first_zero_bit_to_index(bm) (__builtin_ffsll(~(bm))-1)

/*with the context of iteration,do not change idx's value*/
#define e3_bitmap_foreach_set_bit_start(bm,idx) {\
	e3_bitmap _bm=(bm); \
	for((idx)=e3_bitmap_first_set_bit_to_index(_bm); \
		(idx)!=-1; \
		(idx)=e3_bitmap_first_set_bit_to_index(e3_bitmap_clear_bit((_bm),(idx))))  

#define e3_bitmap_foreach_set_bit_end() }




#endif