#include <l2fib.h>
#include <rte_config.h>
#include <rte_malloc.h>
#include <device.h>



struct l2fib_header *l2fib_list=NULL;


void l2fib_early_init(void)
{
	l2fib_list=rte_zmalloc(NULL,sizeof(struct l2fib_header)*L2FIB_LIST_SIZE,64);
	E3_ASSERT(l2fib_list);
	E3_LOG("l2fib table base initialized\n");
}
struct l2fib_entry * allocate_l2fib_entry(void)
{
	struct l2fib_entry *entry=rte_zmalloc(NULL,sizeof(struct l2fib_entry),64);
	if(entry)
		l2fib_entry_init(entry);
	return entry;
}

void l2fib_entry_reclaim_func(struct rcu_head *rcu)
{
	struct l2fib_entry * entry=container_of(rcu,struct l2fib_entry,rcu);
	rte_free(entry);
}

void reclaim_l2fib_entry(struct l2fib_entry *entry)
{
	
}


void dump_l2fib(FILE* fp)
{
	int index;
	struct l2fib_key key;
	struct l2fib_entry * entry;
	int list_index=0;
	for(list_index=0;list_index<L2FIB_LIST_SIZE;list_index++)
		foreach_key_at_list_index_start(list_index,index,entry){
			key.key_as64=l2fib_key(entry,index).key_as64;
			fprintf(fp,"%04d %02x:%02x:%02x:%02x:%02x:%02x     %d(%s)\n",
				key.vlan_id,
				key.mac_addr[0],
				key.mac_addr[1],
				key.mac_addr[2],
				key.mac_addr[3],
				key.mac_addr[4],
				key.mac_addr[5],
				l2fib_port(entry,index),
				(char*)interface_at_index(l2fib_port(entry,index))->ifname);
		}
		foreach_key_at_list_index_end();
		fflush(fp);
}
