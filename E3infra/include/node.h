#ifndef _NODE_H
#define _NODE_H
#include <inttypes.h>
#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <urcu-qsbr.h>
#include <e3_log.h>

#define MAX_NR_NODES 256
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
	uint16_t last_entry_cached;
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
	
	struct node * next;
	void * node_priv;/*it may point to node-main instance*/
	
	__attribute__((aligned(64))) uint64_t cacheline2[0];
	struct next_entry fwd_entries[MAX_NR_EDGES];
	__attribute__((aligned(64))) uint64_t cacheline3[0];
	
}__attribute__((aligned(64)));




struct node_registery{
	
};
inline struct node* find_node_by_name(const char * name);
inline struct node* find_node_by_index(int index);

int register_node(struct node *node);
void unregister_node(struct node * node);
int node_module_test(void);
void dump_nodes(FILE*fp);


inline int deliver_mbufs_between_nodes(uint16_t dst_node,uint16_t src_node,struct rte_mbuf **mbufs,int nr_mbufs);



#endif

