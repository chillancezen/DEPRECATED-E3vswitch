#ifndef _NODE_H
#define _NODE_H
#include <inttypes.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <urcu-qsbr.h>
#include <e3_log.h>


#define MAX_NR_NODES 1024 
#define MAX_NODENAME_LEN 64
#define NODE_BURST_SIZE 32
#define MAX_NR_EDGES 64 
#define NODE_FLAG_DETACHABLE 0x1


#define DEFAULT_NR_RING_PERNODE (1024*1)

typedef int (*node_process_func)(void * arg);
typedef void (*node_rcu_callback)(struct rcu_head * rcu);
enum node_type{
	node_type_misc=0,
	node_type_input=1,
	node_type_output=2,
	node_type_worker=3
};

#define INVALID_ENTRY 0xfff

#define NODE_NEXT_EDGE_NONE 0x0
#define NODE_TO_NODE_FWD 0x1
#define NODE_TO_CLASS_FWD 0x2

struct next_entry{
	uint16_t forward_behavior;
	uint16_t reserved;
	int16_t last_entry_cached;/*for class edge,this is the inner array index,otherwise ,it's real node index*/
	union{
		uint16_t next_node;
		uint16_t next_class;
	};
};



struct node{
	uint8_t name[MAX_NODENAME_LEN];
	__attribute__((aligned(64))) uint64_t cacheline1[0];
	
	/*read-most cache line*/
	struct rcu_head rcu;
	node_process_func node_process_func;
	node_rcu_callback node_reclaim_func;
	enum node_type node_type;
	uint16_t node_index;/*will be allocated by registery framework*/
	uint8_t lcore_id;/*prioritize which cpu it will run on,must we take best effort ,do not gurantee that*/
	uint8_t burst_size;/*default is 32,it could be larger if node is with higher priority,we 
	schedule node by weighted fair algorithm*/
	struct rte_ring *node_ring;
	#if 0
	union{
		struct rte_ring *node_ring;
		struct rte_mempool *node_pool;/*used by Input node*/
	};
	#endif
	
	struct node * next;/*pointer to next node in lcore task list*/
	void * node_priv;/*it may point to node-main instance*/
	
	__attribute__((aligned(64))) uint64_t cacheline2[0];
	struct next_entry fwd_entries[MAX_NR_EDGES];
	__attribute__((aligned(64))) uint64_t cacheline3[0];
	
}__attribute__((aligned(64)));



extern struct node *gnode_array[MAX_NR_NODES];


#define FOREACH_NODE_START(node) {\
	int _index=0; \
	for(_index=0;_index<MAX_NR_NODES;_index++){ \
		(node)=rcu_dereference(gnode_array[_index]); \
		if(node)
			
#define FOREACH_NODE_END() }}


/*let them to be inlined*/
__attribute__((always_inline)) static inline struct node* find_node_by_name(const char * name)
{	
	struct node * pnode=NULL;
	int idx=0;
	for(idx=0;idx<MAX_NR_NODES;idx++)
		if(gnode_array[idx]&&!strcmp((char*)gnode_array[idx]->name,name)){
			pnode=gnode_array[idx];
			break;
		}
	return pnode;
}
__attribute__((always_inline)) static inline struct node* find_node_by_index(int index)
{
	return ((index>=MAX_NR_NODES)||(index<0))?
		NULL:
		rcu_dereference(gnode_array[index]);
}


int register_node(struct node *node);
void unregister_node(struct node * node);
int node_module_test(void);
void dump_nodes(FILE*fp);




void default_rte_reclaim_func(struct rcu_head * rcu);
void reclaim_non_input_node_bottom_half(struct rcu_head * rcu);

void clear_node_ring_buffer(struct node * pnode) ;



#endif

