#ifndef _LCORE_EXTENSION_H
#define _LCORE_EXTENSION_H
#include <node.h>

#define MAX_LCORE_SUPPORTED 127
#define MAX_SOCKET_SUPPORTED 8

#define MEMPOOL_CACHE_SIZE 256
#define DEFAULT_NR_MBUF_PERSOCKET (1024*64)


#define PRESERVE_MASTER_LCORE /*preserve master lcore for control plane purpose*/

/*#define validate_lcore_id(id)   (((id)>=0)&&((id)<=MAX_LCORE_SUPPORTED))*/

#define validate_lcore_id(id)   ({\
	unsigned _lcore_id; \
	int _is_valid=0; \
	RTE_LCORE_FOREACH(_lcore_id) \
		if(_lcore_id==id){ \
			_is_valid=1; \
			break; \
		} \
	_is_valid; \
})
			
		
	

#define ATTACHED_NODES_PRESERVE_THRESHOLD 0x7fff


struct e3_lcore{
	int is_enabled;
	int socket_id;
	int attached_nodes;
	int attached_io_nodes;
};
extern int nr_sockets;
extern struct node * lcore_task_list[MAX_LCORE_SUPPORTED];

int preserve_lcore_for_io(int lcore_id);
int preserve_lcore_for_worker(int lcore_id);

void init_lcore_extension(void);
int attach_node_to_lcore(struct node *node);
int detach_node_from_lcore(struct node*node);
int dump_lcore_list(FILE*fp);
uint64_t get_lcore_task_list_base(void);

int lcore_extension_test(void);

/*lcore_task_list must be exported as an external variable*/
#define foreach_numa_socket(socket_id) \
	for((socket_id)=0; \
	(socket_id)<nr_sockets; \
	(socket_id)++)
		
#define foreach_node_in_lcore(node,lcore_id) \
	for((node)=rcu_dereference(lcore_task_list[(lcore_id)]); \
	(node); \
	(node)=rcu_dereference((node)->next))

int  get_lcore_by_socket_id(int socket_id);
int  get_lcore(void);
int  get_io_lcore_by_socket_id(int socket_id);
int  get_io_lcore(void);
void put_lcore(int lcore_id,int  is_io);
inline int  lcore_to_socket_id(int lcore_id);
struct rte_mempool * get_mempool_by_socket_id(int socket_id);


int lcore_default_entry(__attribute__((unused)) void *arg);
#endif