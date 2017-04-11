#ifndef _NODE_CLASS_H
#define _NODE_CLASS_H
#include <inttypes.h>
#include <urcu-qsbr.h>
#include <string.h>
#include <stdio.h>


#define MAX_NR_NODE_CLASSES 256
#define MAX_NODECLASS_NAME_LEN 64
#define MAX_NODE_IN_CLASS 8
typedef void (*node_class_rcu_callback)(struct rcu_head * rcu);
struct node ;


struct node_entry{
	union{
		void * entries_as_ptr;
		struct {
			uint16_t is_valid:1;
			uint16_t node_attached_cnt:15;
			uint16_t node_index;
			uint32_t padding;
		};
	};
};


struct node_class{
	uint8_t class_name[MAX_NODECLASS_NAME_LEN];
	
	__attribute__((aligned(64))) uint64_t cacheline1[0];
	struct rcu_head rcu;
	uint16_t node_class_index;
	node_class_rcu_callback class_reclaim_func;
	void * node_class_priv;
	
	__attribute__((aligned(64))) uint64_t cacheline2[0];
	struct node_entry node_entries[MAX_NODE_IN_CLASS];
}__attribute__((aligned(64)));

extern struct node_class * gnode_class_array[MAX_NR_NODE_CLASSES];


#define FOREACH_NODE_CLASS_START(node_class) {\
	int _index=0; \
	for(_index=0;_index<MAX_NR_NODE_CLASSES;_index++){ \
		(node_class)=rcu_dereference(gnode_class_array[_index]); \
		if(node_class)
			
#define FOREACH_NODE_CLASS_END() }}


#define FOREACH_NODE_ENTRY_IN_CLASS_START(pclass,node_entry) {\
	int _index=0; \
	for(_index=0;_index<MAX_NODE_IN_CLASS;_index++) {\
		(node_entry).entries_as_ptr=rcu_dereference((pclass)->node_entries[_index].entries_as_ptr);

#define FOREACH_NODE_ENTRY_IN_CLASS_END() }}

#define validate_node_entry(entry) (!!((entry).is_valid))

#define node_entry_at_index(pclass,idx) ({ \
	E3_ASSERT(((idx)>=0&&(idx)<MAX_NODE_IN_CLASS)); \
	(pclass)->node_entries[(idx)]; \
})

#define node_entry_at_index_safe(pclass,idx,entry) { \
	E3_ASSERT(((idx)>=0&&(idx)<MAX_NODE_IN_CLASS)); \
	(entry).entries_as_ptr=rcu_dereference((pclass)->node_entries[idx].entries_as_ptr); \
}

#define validate_node_entry_at_index(pclass,idx) (((idx)>=0) \
	&&((idx)<MAX_NODE_IN_CLASS) \
	&&(((pclass)->node_entries[(idx)]).is_valid))

#define validate_node_entry_at_index_safe(pclass,idx) ({ \
	struct node_entry _entry; \
	int _range_valid=((idx)>=0)&&((idx)<MAX_NODE_IN_CLASS); \
	if(_range_valid) \
		_entry.entries_as_ptr=rcu_dereference((pclass)->node_entries[idx].entries_as_ptr); \
	_range_valid&&_entry.is_valid; \
})


#define node_index_of_node_entry(pclass,idx) ({ \
	E3_ASSERT(((idx)>=0&&(idx)<MAX_NODE_IN_CLASS)); \
	(pclass)->node_entries[(idx)].node_index; \
})

int register_node_class(struct node_class *nclass);
void unregister_node_class(struct node_class *nclass);

__attribute__((always_inline)) static inline struct node_class * find_node_class_by_name(const char * class_name)
{
	struct node_class * pclass=NULL;
	int idx=0;
	for(idx=0;idx<MAX_NR_NODE_CLASSES;idx++)
		if(gnode_class_array[idx]&&!strcmp((char*)gnode_class_array[idx]->class_name,class_name)){
			pclass=gnode_class_array[idx];
			break;
		}
	return pclass;
}
__attribute__((always_inline)) static  inline struct node_class* find_node_class_by_index(int index)
{
	return (index>=MAX_NR_NODE_CLASSES)?NULL:rcu_dereference(gnode_class_array[index]);
}

/*it's not recommanded to use _ beginning function.*/
int _add_node_into_nodeclass(struct node_class *pclass,struct node *pnode);
int add_node_into_nodeclass(const char* class_name,const char* node_name);

int _delete_node_from_nodeclass(struct node_class *pclass,struct node *pnode);
int delete_node_from_nodeclass(const char* class_name,const char* node_name);
void dump_node_class(FILE* fp);
#endif