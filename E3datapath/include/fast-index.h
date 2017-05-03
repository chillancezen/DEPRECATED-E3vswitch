#ifndef _FAST_INDEX_H
#define _FAST_INDEX_H
#include <inttypes.h>
#include <urcu-qsbr.h>
#include <e3_bitmap.h>
#include <x86intrin.h>
/*todo:ip to 2_2key*/
/*part 0:2-2 fast index which can map ipv4 addresses into corresponding data structure*/
#define FINDEX_2_2_TAGS_LENGTH 48
struct findex_2_2_entry{
	uint16_t tags[FINDEX_2_2_TAGS_LENGTH];/*must be 16 or 32 bytes aligned,place it at the begining*/
	e3_bitmap  tags_avail;
	struct findex_2_2_entry * next_entry;
	struct rcu_head rcu;
	/*previous fields consist of  two cache line*/
	__attribute__((aligned(64))) 
	uint64_t value[FINDEX_2_2_TAGS_LENGTH];
};

struct findex_2_2_base{
	struct findex_2_2_entry * next;
}__attribute__((packed));


struct findex_2_2_key{
	union{
		uint32_t key_as_u32;
		struct {
			uint16_t key_index;
			uint16_t key_tag;
		};
	};
	union {
		void * value_as_ptr;
		uint64_t value_as_u64;
	};
};

__attribute__((always_inline)) static inline 
	int fast_index_2_2_item_safe(struct findex_2_2_base * base,struct findex_2_2_key * key)
{
	int ret=1;
	int idx=0;
	int found_index,local_index;
	int index_base=0;
	e3_bitmap cmp_rc;
	#if defined(__AVX2__)
	__m256i ymm_tags;
	__m256i ymm_target;
	__m256i ymm_tags_cmp_rc;
	__m256i ymm_shuffle_mask;
	ymm_target=_mm256_set1_epi16(key->key_tag);
	ymm_shuffle_mask=_mm256_set_epi8(
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		15,13,11,9,7,5,3,1,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		15,13,11,9,7,5,3,1);
	#define PERMUTATION_MASK (0|(2<<2)|(1<<4)|(3<<6))
	#elif defined(__SSE4_1__)
	__m128i xmm_tags;
	__m128i xmm_target;
	__m128i xmm_tags_cmp_rc;
	__m128i xmm_shuffle_mask;
	xmm_target=_mm_set1_epi16(key->key_tag);
	xmm_shuffle_mask=_mm_set_epi8(
		0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,
		15,13,11,9,
		7,5,3,1);
	#endif
	struct findex_2_2_entry * pentry=rcu_dereference(base[key->key_index].next);
	for(;pentry;pentry=rcu_dereference(pentry->next_entry)){
		#if defined(__AVX2__)
		for(idx=0,index_base=0;ret&&(idx<3);idx++,index_base+=16){
			//ymm_tags=_mm256_load_si256((__m256i*)(idx*32+(char*)pentry->tags));
			ymm_tags=_mm256_stream_load_si256((__m256i*)(idx*32+(char*)pentry->tags));
			ymm_tags_cmp_rc=_mm256_cmpeq_epi16(ymm_tags,ymm_target);
			ymm_tags_cmp_rc=_mm256_shuffle_epi8(ymm_tags_cmp_rc,ymm_shuffle_mask);
			ymm_tags_cmp_rc=_mm256_permute4x64_epi64(ymm_tags_cmp_rc,PERMUTATION_MASK);
			cmp_rc=_mm256_movemask_epi8(ymm_tags_cmp_rc);
			if(!cmp_rc)
				continue;
			e3_bitmap_foreach_set_bit_start(cmp_rc,local_index){
				found_index=local_index+index_base;
				ret=!e3_bitmap_is_bit_set(pentry->tags_avail,found_index);
				if(!ret){
					key->value_as_u64=pentry->value[found_index];\
					break;
				}
			}
			e3_bitmap_foreach_set_bit_end();
			
		}
		#elif defined(__SSE4_1__)
		for(idx=0,index_base=0;ret&&(idx<6);idx++,index_base+=8){
			//xmm_tags=_mm_load_si128((__m128i *)(idx*16+(char*)pentry->tags));
			xmm_tags=_mm_stream_load_si128((__m128i *)(idx*16+(char*)pentry->tags));
			xmm_tags_cmp_rc=_mm_cmpeq_epi16(xmm_tags,xmm_target);
			xmm_tags_cmp_rc=_mm_shuffle_epi8(xmm_tags_cmp_rc,xmm_shuffle_mask);
			cmp_rc=_mm_movemask_epi8(xmm_tags_cmp_rc);
			if(!cmp_rc)/*even find no matched item*/
				continue;
			/*tags may be equal ,but we'd make sure this item is valid*/
			e3_bitmap_foreach_set_bit_start(cmp_rc,local_index){
				found_index=local_index+index_base;
				ret=!e3_bitmap_is_bit_set(pentry->tags_avail,found_index);
				if(!ret){
					key->value_as_u64=pentry->value[found_index];\
					break;
				}
			}
			e3_bitmap_foreach_set_bit_end();
		}
		#else
		#error "machine not supported,please make sure "
		"your target machine support at least SSE4.1 instruction set"
		#endif
		if(!ret)
			break;
	}
	return ret;
}

struct findex_2_2_base * allocate_findex_2_2_base(void);
struct findex_2_2_entry * allocate_findex_2_2_entry(void);
int add_index_2_2_item_unsafe(struct findex_2_2_base * base,struct findex_2_2_key * key);
void dump_findex_2_2_base(struct findex_2_2_base * base);
void delete_index_2_2_item_unsafe(struct findex_2_2_base* base,struct findex_2_2_key * key);


/*todo:tunnel+mac to keys*/
/*port 1,2_1_6 fast indexing which maps 24-bits tunnel id  plus 48-bit mac into lb instance*/
#define FINDEX_2_1_6_TAGS_LENGTH 48
#if defined(__AVX2__)

struct findex_2_1_6_tag_block{
	uint16_t tag1[16];
	uint16_t tag2[16];
	uint16_t tag3[16];
}__attribute__((packed));
#define MAX_TAGS_NR_OF_ENTRY 3

#elif defined(__SSE4_1__)

struct findex_2_1_6_tag_block{
	uint16_t tag1[8];
	uint16_t tag2[8];
	uint16_t tag3[8];
}__attribute__((packed));
#define MAX_TAGS_NR_OF_ENTRY 6

#else 
#error "please make sure target machine at least supports SSE4.1 instruction set"
#endif

struct findex_2_1_6_entry{
	/*32-bytes header room only*/
	e3_bitmap tag_avail;
	struct findex_2_1_6_entry * next_entry;
	uint8_t tag0;
	
	__attribute__((aligned(32))) 
		struct findex_2_1_6_tag_block 
			tags[MAX_TAGS_NR_OF_ENTRY];
	uint64_t values[48];
	
	__attribute__((aligned(64)))
		struct rcu_head rcu;
};

struct findex_2_1_6_base{
	struct findex_2_1_6_entry * next;
};

struct findex_2_1_6_key{
	uint16_t key_index;
	uint8_t tag0;
	uint16_t tag1;
	uint16_t tag2;
	uint16_t tag3;

	union{
		uint64_t value_as_u64;
		void  *  value_as_ptr;
	};
};

static inline int fast_index_2_1_6_item_safe(struct findex_2_1_6_base * base,struct findex_2_1_6_key* key)
{
	struct findex_2_1_6_entry *pentry;
	int idx;
	e3_bitmap cmp_rc;
	int local_index;
	int index_base;
	int found_index;
	int ret=-1;
	#if defined(__AVX2__)
	__m256i ymm_tag1,ymm_tag2,ymm_tag3;
	__m256i ymm_cmp_target1,ymm_cmp_target2,ymm_cmp_target3;
	__m256i ymm_cmp_rc1,ymm_cmp_rc2,ymm_cmp_rc3;
	__m256i ymm_cmp_rc;
	__m256i ymm_shuffle_mask;
	
	ymm_cmp_target1=_mm256_set1_epi16(key->tag1);
	ymm_cmp_target2=_mm256_set1_epi16(key->tag2);
	ymm_cmp_target3=_mm256_set1_epi16(key->tag3);
	ymm_shuffle_mask=_mm256_set_epi8(
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			15,13,11,9,7,5,3,1,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			15,13,11,9,7,5,3,1);
	#define PERMUTATION_MASK (0|(2<<2)|(1<<4)|(3<<6))
	#elif defined(__SSE4_1__)
	__m128i xmm_tag1,xmm_tag2,xmm_tag3;
	__m128i xmm_target1,xmm_target2,xmm_target3;
	__m128i xmm_cmp_rc1,xmm_cmp_rc2,xmm_cmp_rc3;
	__m128i xmm_cmp_rc;
	__m128i xmm_shuffle_mask;
	
	xmm_target1=_mm_set1_epi16(key->tag1);
	xmm_target2=_mm_set1_epi16(key->tag2);
	xmm_target3=_mm_set1_epi16(key->tag3);
	xmm_shuffle_mask=_mm_set_epi8(
			0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,
			15,13,11,9,
			7,5,3,1);
	
	#endif
	for(pentry=rcu_dereference(base[key->key_index].next);
	pentry;
	pentry=rcu_dereference(pentry->next_entry)){
		if(key->tag0!=pentry->tag0)
			continue;
		#if defined(__AVX2__)
		for(idx=0,index_base=0;(ret)&&(idx<MAX_TAGS_NR_OF_ENTRY);idx++,index_base+=16){
			ymm_tag1=_mm256_load_si256((__m256i*)pentry->tags[idx].tag1);
			ymm_tag2=_mm256_load_si256((__m256i*)pentry->tags[idx].tag2);
			ymm_tag3=_mm256_load_si256((__m256i*)pentry->tags[idx].tag3);
			ymm_cmp_rc1=_mm256_cmpeq_epi16(ymm_tag1,ymm_cmp_target1);
			ymm_cmp_rc2=_mm256_cmpeq_epi16(ymm_tag2,ymm_cmp_target2);
			ymm_cmp_rc3=_mm256_cmpeq_epi16(ymm_tag3,ymm_cmp_target3);
			ymm_cmp_rc=_mm256_and_si256(ymm_cmp_rc1,ymm_cmp_rc2);
			ymm_cmp_rc=_mm256_and_si256(ymm_cmp_rc,ymm_cmp_rc3);
			ymm_cmp_rc=_mm256_shuffle_epi8(ymm_cmp_rc,ymm_shuffle_mask);
			ymm_cmp_rc=_mm256_permute4x64_epi64(ymm_cmp_rc,PERMUTATION_MASK);
			cmp_rc=_mm256_movemask_epi8(ymm_cmp_rc);
			if(!cmp_rc)
				continue;
			e3_bitmap_foreach_set_bit_start(cmp_rc,local_index){
				found_index=index_base+local_index;
				ret=!e3_bitmap_is_bit_set(pentry->tag_avail,found_index);
				if(!ret){
					key->value_as_u64=pentry->values[found_index];
					break;
				}
			}
			e3_bitmap_foreach_set_bit_end();
			
		}
		#elif defined (__SSE4_1__)
		for(idx=0,index_base=0;(ret)&&(idx<MAX_TAGS_NR_OF_ENTRY);idx++,index_base+=8){
			xmm_tag1=_mm_load_si128((__m128i *)pentry->tags[idx].tag1);
			xmm_tag2=_mm_load_si128((__m128i *)pentry->tags[idx].tag2);
			xmm_tag3=_mm_load_si128((__m128i *)pentry->tags[idx].tag3);
			xmm_cmp_rc1=_mm_cmpeq_epi16(xmm_tag1,xmm_target1);
			xmm_cmp_rc2=_mm_cmpeq_epi16(xmm_tag2,xmm_target2);
			xmm_cmp_rc3=_mm_cmpeq_epi16(xmm_tag3,xmm_target3);
			xmm_cmp_rc=_mm_and_si128(xmm_cmp_rc1,xmm_cmp_rc2);
			xmm_cmp_rc=_mm_and_si128(xmm_cmp_rc,xmm_cmp_rc3);
			xmm_cmp_rc=_mm_shuffle_epi8(xmm_cmp_rc,xmm_shuffle_mask);
			cmp_rc=_mm_movemask_epi8(xmm_cmp_rc);
			if(!cmp_rc)
				continue;
			
			e3_bitmap_foreach_set_bit_start(cmp_rc,local_index){
				found_index=index_base+local_index;
				ret=!e3_bitmap_is_bit_set(pentry->tag_avail,found_index);
				if(!ret){
					key->value_as_u64=pentry->values[found_index];
					break;
				}
			}
			e3_bitmap_foreach_set_bit_end();
		}
		
		#else
		#error "machine still not supported"
		#endif
	}
	return ret;
}

struct findex_2_1_6_base *allocate_findex_2_1_6_base(void);
struct findex_2_1_6_entry * allocate_findex_2_1_6_entry(void);
int add_index_2_1_6_item_unsafe(struct findex_2_1_6_base * base,struct findex_2_1_6_key * key);
void dump_findex_2_1_6_base(struct findex_2_1_6_base * base);
void delete_index_2_1_6_item_unsafe(struct findex_2_1_6_base * base,struct findex_2_1_6_key * key);




#endif
