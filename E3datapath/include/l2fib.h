#ifndef _L2FIB_H
#define _L2FIB_H

#include <inttypes.h>
#include <urcu-qsbr.h>
#include <e3_bitmap.h>
#include <string.h>
#include <e3_log.h>
#include <util.h>

struct l2fib_key{
	union{
		void * key_as_ptr;
		uint64_t key_as64;
		struct {
			uint16_t vlan_id;
			uint8_t mac_addr[6];
		};
		struct {
			uint8_t dummy[6];
			uint16_t L1_index;
		};
	};
};
#define MAX_ITEM_IN_AN_ENTRY 16

struct l2fib_entry{
	__attribute__((aligned(64))) void * cacheline0[0];
	struct rcu_head rcu;
	e3_bitmap bm_avail;
	struct l2fib_entry * next;
	uint16_t output_ports[MAX_ITEM_IN_AN_ENTRY];
	
	__attribute__((aligned(64))) void * cacheline1[0];
	struct l2fib_key keys[MAX_ITEM_IN_AN_ENTRY];
	__attribute__((aligned(64))) void * cacheline2[0];
	uint64_t timestamp[MAX_ITEM_IN_AN_ENTRY];
};
#define L2FIB_LIST_SIZE (1<<16)

struct l2fib_header{
	struct l2fib_entry* next;
};

extern struct l2fib_header *l2fib_list;

#define l2fib_entry_init(entry) {\
	e3_bitmap_init((entry)->bm_avail); \
}

/*return an relative index within the entry ,-1 indicates an error(no such entry)*/
#define find_key_in_an_entry(key,entry_ptr) ({\
	int _idx=-1; \
	e3_bitmap_foreach_set_bit_start((entry_ptr)->bm_avail,_idx){ \
		if((entry_ptr)->keys[_idx].key_as64==(key).key_as64) \
			break; \
	} \
	e3_bitmap_foreach_set_bit_end(); \
	_idx; \
})

/*return an relative index within the entry ,-1 indicates an error(no such entry)*/
/*the caller must be sure ,the key does not exist in the entry,
or the behavior will be undefined*/
__attribute__((always_inline)) static inline int _add_key_in_an_entry(
	struct l2fib_key *key,
	uint16_t port_id,
	struct l2fib_entry*entry) 
{
	int idx=e3_bitmap_first_zero_bit_to_index(entry->bm_avail);
	if((idx>=0)&&(idx<MAX_ITEM_IN_AN_ENTRY)){
		entry->keys[idx].key_as64=key->key_as64;
		entry->output_ports[idx]=port_id;
		e3_bitmap_set_bit(entry->bm_avail,idx);
	}
	else
		idx=-1;
	return idx;
}
/*it will check whether key exists before adding it genuinely*/
__attribute__((always_inline)) static inline int add_key_in_an_entry(
	struct l2fib_key *key,
	uint16_t port_id,
	struct l2fib_entry *entry)
{
	int idx=find_key_in_an_entry(*key,entry);
	if(idx>=0){
		entry->output_ports[idx]=port_id;
		return idx;
	}
	return _add_key_in_an_entry(key,port_id,entry);
}

/*if success,return the corresponding slot index,other -1*/
__attribute__((always_inline)) static inline int delete_key_from_an_entry(
	struct l2fib_key *key,
	struct l2fib_entry *entry)
{
	int idx=find_key_in_an_entry(*key,entry);
	if(idx>=0)
		e3_bitmap_clear_bit(entry->bm_avail,idx);
	return idx;
}

#define l2fib_key(entry,idx) ((entry)->keys[(idx)])
#define l2fib_port(entry,idx) ((entry)->output_ports[(idx)])
#define l2fib_timestamp(entry,idx) ((entry)->timestamp[(idx)])

/*input:key,not pointer
output:
	index in the entry
	entry
this is multi-thread safe
*/
#define index_key_safe(key,index,entry) {\
	struct l2fib_entry *ptr=rcu_dereference(l2fib_list[(key).L1_index].next); \
	(index)=-1; \
	for(;ptr;ptr=rcu_dereference(ptr->next)){ \
		(index)=find_key_in_an_entry((key),ptr); \
		if((index)>=0) \
			break; \
	} \
	(entry)=ptr; \
}

/*check whether key exists,if not try to allocate a slot to accomondate the key
,if no slot could be found,try to allocate a new entry
input:key,port_id
output:index,entry
*/

#define add_key_unsafe(key,port_id,index,entry) {\
	index_key_safe((key),(index),(entry)); \
	if((index)<0){ \
		struct l2fib_entry *ptr=l2fib_list[(key).L1_index].next; \
		for(;ptr;ptr=ptr->next){ \
			(index)=add_key_in_an_entry(&(key),(port_id),ptr); \
			if((index)>=0){ \
				l2fib_port(ptr,(index))=(port_id); \
				(entry)=ptr; \
				break; \
			} \
		} \
		if(!ptr){ \
			struct l2fib_entry *ptr_new=allocate_l2fib_entry(); \
			if(ptr_new){ \
				(index)=add_key_in_an_entry(&(key),(port_id),ptr_new); \
				(entry)=ptr_new; \
				if(l2fib_list[(key).L1_index].next==NULL) \
					rcu_assign_pointer(l2fib_list[(key).L1_index].next,ptr_new); \
				else { \
					for(ptr=l2fib_list[(key).L1_index].next;ptr->next;ptr=ptr->next); \
					rcu_assign_pointer(ptr->next,ptr_new); \
				} \
			} \
		} \
	} \
	else { \
		l2fib_port((entry),(index))=port_id;\
	} \
}

#define delete_key_unsafe(key) {\
	int _index=0; \
	struct l2fib_entry *_entry=NULL; \
	index_key_safe((key),_index,_entry); \
	if(_index>=0) \
		delete_key_from_an_entry(&(key),_entry); \
}

#define foreach_entry_at_list_index(lst_index,entry) \
	for((entry)=rcu_dereference(l2fib_list[lst_index].next); \
		(entry); \
		(entry)=rcu_dereference((entry)->next)) 
		

#define foreach_key_at_list_index_start(lst_index,index,entry) {\
	for((entry)=rcu_dereference(l2fib_list[lst_index].next); \
		(entry); \
		(entry)=rcu_dereference((entry)->next)) \
		e3_bitmap_foreach_set_bit_start((entry)->bm_avail,(index))


#define foreach_key_at_list_index_end() \
	e3_bitmap_foreach_set_bit_end() }
		
		

void l2fib_early_init(void);
struct l2fib_entry * allocate_l2fib_entry(void);
#endif