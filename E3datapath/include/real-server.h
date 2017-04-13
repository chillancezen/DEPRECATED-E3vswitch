#ifndef REAL_SERVER_H
#define REAL_SERVER_H
#include <lb-common.h>
#include <fast-index.h>

#define MAX_REAL_SERVER_NR 4096
extern struct real_server * grs_array[MAX_REAL_SERVER_NR];
extern struct findex_2_1_6_base * rs_base;


__attribute__((always_inline)) 
	static inline void make_findex_2_1_6_key(uint32_t tunnel_id,
	uint8_t * mac,
	struct findex_2_1_6_key * key)
{
	key->key_index=(uint16_t)((tunnel_id>>4)&0xffff);
	key->tag0=((tunnel_id>>16)&0xf0)|((mac[0]>>4)&0xf);
	key->tag1=(((tunnel_id<<4)&0xf0)|(mac[0]&0xf))|((mac[1]<<8)&0xff00);
	key->tag2=*(uint16_t*)&mac[2];
	key->tag3=*(uint16_t*)&mac[4];
}
struct real_server * allocate_real_server(void);
int register_real_server(struct real_server * rs);
void unregister_real_server(struct real_server * rs);
void dump_real_servers(FILE * fp);

#define find_real_server_at_index(idx) ((((idx)<MAX_REAL_SERVER_NR)&&((idx)>=0))?\
	((struct real_server *)rcu_dereference(grs_array[(idx)])):\
	NULL)
	
#define search_real_server(tunnel_id,mac) ({\
	int _rc; \
	struct findex_2_1_6_key _key={.key_index=0}; \
	make_findex_2_1_6_key((uint32_t)(tunnel_id),(uint8_t *)(mac),&_key); \
	_rc=fast_index_2_1_6_item_safe(rs_base,&_key); \
	_rc?-1:(uint16_t)_key.value_as_u64; \
})

#endif
