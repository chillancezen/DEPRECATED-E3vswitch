
#include <l2-learn.h>


#include <rte_malloc.h>
#include <e3_log.h>
#include <string.h>

struct node* pnode_l2_learn=NULL;
int l2_learn_process_func(void * arg)
{
	struct node *pnode=(struct node*)arg;
	struct l2fib_key key;
	uint64_t command;
	int rc;
	int entry_index;
	struct l2fib_entry *entry;
	
	rc=rte_ring_sc_dequeue(pnode->node_ring,(void**)&command);
	if(rc)
		return 0;
	uint64_t cmd_hi,cmd_low;
	cmd_hi=HIGH_UINT64(command);
	switch(cmd_hi)
	{
		case L2_COMMAND_ADD_FIB:
			cmd_low=LOW_UINT64(command);
			rc=rte_ring_sc_dequeue(pnode->node_ring,(void**)&command);
			if(rc)
				break;/*illegal command*/
			key.key_as64=command;
			add_key_unsafe(key,((uint16_t)cmd_low),entry_index,entry);
			if(entry_index>=0){
				l2fib_timestamp(entry,entry_index)=rte_rdtsc();
				E3_LOG("add l2fib(vlan:%d %02x:%02x:%02x:%02x:%02x:%02x:) to port:%d\n",
					key.vlan_id,
					key.mac_addr[0],
					key.mac_addr[1],
					key.mac_addr[2],
					key.mac_addr[3],
					key.mac_addr[4],
					key.mac_addr[5],
					(uint16_t)l2fib_port(entry,entry_index));
			}else{
				E3_WARN("fails to add l2fib(vlan:%d %02x:%02x:%02x:%02x:%02x:%02x:) to port:%d\n",
					key.vlan_id,
					key.mac_addr[0],
					key.mac_addr[1],
					key.mac_addr[2],
					key.mac_addr[3],
					key.mac_addr[4],
					key.mac_addr[5],
					(uint16_t)cmd_low);
			}
			break;
	}
	return 0;
}

int l2_learn_early_init(void)
{
	
	struct node *pnode=rte_zmalloc(NULL,sizeof(struct node),64);
	E3_ASSERT(pnode);

	sprintf((char*)pnode->name,"l2-learn-node");
	pnode->node_process_func=l2_learn_process_func;
	pnode->node_type=node_type_worker;
	pnode->burst_size=NODE_BURST_SIZE;
	//pnode->lcore_id=rte_get_master_lcore();
	/*we use master for debug reason*/
	pnode->lcore_id=get_lcore();
	pnode->node_priv=NULL;
	pnode->node_reclaim_func=NULL;
	E3_ASSERT(!register_node(pnode));
	E3_ASSERT(!attach_node_to_lcore(pnode));
	E3_LOG("register node:%s on %d\n",(char*)pnode->name,(int)pnode->lcore_id);
	pnode_l2_learn=pnode;
	return 0;
}
