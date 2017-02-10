#ifndef _NODE_ADJACENCY_H
#define _NODE_ADJACENCY_H
#include <node.h>
#include <node_class.h>
#include <util.h>



int set_node_to_node_edge(const char * cur_node_name,int entry_index,const char * next_node_name);
int set_node_to_class_edge(const char* cur_node_name,int entry_index,const char * next_node_class);
int clean_node_next_edge(const char* cur_node_name,int entry_index);
void clean_node_next_edges(const char* cur_node_name);
void dump_node_edges(const char* cur_node_name);
uint16_t reselect_next_forwarding_node(struct node * pnode,struct node_class*pclass,int next_entry_index,int16_t*);
__attribute__((always_inline)) inline static uint16_t next_forwarding_node(struct node *pnode,int next_entry_index)
{
	uint16_t next_node=MAX_NR_NODES;
	struct next_entry *pentry;
	struct node_class * pclass_next;
	E3_ASSERT(next_entry_index<MAX_NR_EDGES);
	pentry=&pnode->fwd_entries[next_entry_index];
	switch(pentry->forward_behavior)
	{
		case NODE_TO_NODE_FWD:
			next_node=pentry->next_node;
			break;
		case NODE_TO_CLASS_FWD:
			pclass_next=find_node_class_by_index(pentry->next_class);
			if(!pclass_next)/*invalid node class*/
				break;
			if(PREDICT_TRUE(validate_node_entry_at_index_safe(pclass_next,pentry->last_entry_cached))){
				next_node=node_index_of_node_entry(pclass_next,pentry->last_entry_cached);
				/*note that next_node is always valid*/
				break;
			}
			next_node=reselect_next_forwarding_node(pnode,pclass_next,next_entry_index,&pentry->last_entry_cached);
			
			break;
	}
	return next_node;
}

__attribute__((always_inline)) inline static int deliver_mbufs_by_next_entry(struct node * pnode,int next_entry,struct rte_mbuf **mbufs,int nr_mbufs)
{
	int nr_delivered=0;
	struct node * pnext=NULL;
	pnext=find_node_by_index(next_forwarding_node(pnode,next_entry));
	if(!pnext)
		goto ret;
	nr_delivered=rte_ring_mp_enqueue_burst(pnext->node_ring,(void**)mbufs,nr_mbufs);
	ret:
	return nr_delivered;

}
#endif
