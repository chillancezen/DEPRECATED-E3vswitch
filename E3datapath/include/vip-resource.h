#ifndef _VIP_RESOURCE_H
#define _VIP_RESOURCE_H
#include <lb-common.h>
#include <fast-index.h>
#include <stdio.h>

#define MAX_VIP_NR 256
extern struct virtual_ip * gvip_array[MAX_VIP_NR];
extern struct findex_2_2_base * ip2vip_base;

int register_virtual_ip(struct virtual_ip *vip);
void unregister_virtual_ip(struct virtual_ip *vip);
struct virtual_ip* allocate_virtual_ip(void);
void default_virtual_ip_reclaim_fun(struct rcu_head * rcu);

#define find_virtual_ip_at_index(idx) ((((idx)<MAX_VIP_NR)&&((idx)>=0))?\
	((struct virtual_ip *)rcu_dereference(gvip_array[(idx)])): \
	NULL)
	
void dump_virtual_ips(FILE* fp);
void vip_resource_early_init(void);

#define search_virtual_ip_index(target_ip) ({\
	int _rc; \
	struct findex_2_2_key _key; \
	_key.key_as_u32=(target_ip); \
	_key.value_as_u64=0x0; \
	_rc=fast_index_2_2_item_safe(ip2vip_base,&_key); \
	_rc?-1:(uint16_t)_key.value_as_u64; \
})
#endif