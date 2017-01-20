#ifndef _NODE_CLASS_H
#define _NODE_CLASS_H
#include <inttypes.h>
#include <urcu-qsbr.h>
#define MAX_NR_NODE_CLASSES 256
#define MAX_NODECLASS_NAME_LEN 64
#define MAX_NODE_IN_CLASS 8
typedef void (*node_class_rcu_callback)(struct rcu_head * rcu);
struct node ;
struct node_class{
	uint8_t class_name[MAX_NODECLASS_NAME_LEN];
	
	__attribute__((aligned(64))) uint64_t cacheline1[0];
	struct rcu_head rcu;
	uint16_t node_class_index;
	node_class_rcu_callback class_reclaim_func;
	void * node_class_priv;
	union{
		uint64_t current_node_nr;
		void  *  current_node_nr_as_ptr;
	};
	
	__attribute__((aligned(64))) uint64_t cacheline2[0];
	uint16_t nodes[MAX_NODE_IN_CLASS];
}__attribute__((aligned(64)));

int register_node_class(struct node_class *nclass);
void unregister_node_class(struct node_class *nclass);
inline struct node_class * find_node_class_by_name(const char * class_name);
inline struct node_class * find_node_class_by_index(int index);
/*it's not recommanded to use _ beginning function.*/
int _add_node_into_nodeclass(struct node_class *pclass,struct node *pnode);
int add_node_into_nodeclass(const char* class_name,const char* node_name);
int _delete_node_from_nodeclass(struct node_class *pclass,struct node *pnode);
int delete_node_from_nodeclass(const char* class_name,const char* node_name);
void dump_node_class(FILE* fp);
#endif