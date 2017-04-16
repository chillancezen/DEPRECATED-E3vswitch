#ifndef _L3_INTERFACE_H
#define _L3_INTERFACE_H
#include <lb-common.h>
#include <stdio.h>

#define MAX_L3_INTERFACE_NR 1024
extern struct l3_interface * gl3if_array[MAX_L3_INTERFACE_NR];
int register_l3_interface(struct l3_interface * l3iface);
void unregister_l3_interface(struct l3_interface * l3iface);
void dump_l3_interfaces(FILE* fp);
struct l3_interface* allocate_l3_interface(void);

#define find_l3_interface_at_index(idx) ((((idx)<MAX_L3_INTERFACE_NR)&&((idx)>=0))?\
	(rcu_dereference(gl3if_array[(idx)])): \
	NULL)

#define foreach_phy_l3_interface_safe_start(e3_iface,l3iface) {\
	int _idx; \
	for(_idx=0;_idx<MAX_L3_INTERFACE_NR;_idx++){ \
		(l3iface)=rcu_dereference(gl3if_array[_idx]); \
		if((!(l3iface))||((l3iface)->if_type!=L3_INTERFACE_TYPE_PHYSICAL) ||((l3iface)->lower_if_index!=(e3_iface))) \
			continue; 
		
#define foreach_phy_l3_interface_safe_end() }}

#endif
