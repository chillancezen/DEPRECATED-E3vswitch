#include <node_adjacency.h>

int set_node_to_node_edge(const char * cur_node_name,int entry_index,const char * next_node_name)
{
	struct node * pnode=find_node_by_name(cur_node_name);
	struct node * pnext_node=find_node_by_name(next_node_name);
	
	if(entry_index<0 ||entry_index>=MAX_NR_EDGES)
		return -1;
	if(!pnode || !pnext_node)
		return -2;

	pnode->fwd_entries[entry_index].forward_behavior=NODE_TO_NODE_FWD;
	pnode->fwd_entries[entry_index].last_entry_cached=pnext_node->node_index;
	pnode->fwd_entries[entry_index].reserved=0;
	pnode->fwd_entries[entry_index].next_node=pnext_node->node_index;
	
	return 0;
}

int set_node_to_class_edge(const char* cur_node_name,int entry_index,const char * next_node_class)
{
	struct node * pnode=find_node_by_name(cur_node_name);
	struct node_class * pnext_class=find_node_class_by_name(next_node_class);
	if(entry_index<0 ||entry_index>=MAX_NR_EDGES)
		return -1;
	
	if(!pnode || !pnext_class)
		return -2;
	
	pnode->fwd_entries[entry_index].forward_behavior=NODE_TO_CLASS_FWD;
	pnode->fwd_entries[entry_index].last_entry_cached=INVALID_ENTRY;
	pnode->fwd_entries[entry_index].reserved=0;
	pnode->fwd_entries[entry_index].next_class=pnext_class->node_class_index;
	return 0;
}

int clean_node_next_edge(const char* cur_node_name,int entry_index)
{
	struct node * pnode=find_node_by_name(cur_node_name);
	if(!pnode)
		return -1;
	if(entry_index<0 ||entry_index>=MAX_NR_EDGES)
		return -2;
	if(pnode->fwd_entries[entry_index].forward_behavior==NODE_TO_CLASS_FWD){
		struct node_class *pclass=find_node_class_by_index(pnode->fwd_entries[entry_index].next_class);
		if(!pclass)
			goto normal;
		if(!validate_node_entry_at_index_safe(pclass,pnode->fwd_entries[entry_index].last_entry_cached))
			goto normal;
		/*decrement node_attached_cnt counter*/
		printf("hello world\n");
		rcu_assign_pointer(pclass->node_entries[pnode->fwd_entries[entry_index].last_entry_cached].entries_as_ptr,
			({
				struct node_entry _entry;
				_entry.entries_as_ptr=pclass->node_entries[pnode->fwd_entries[entry_index].last_entry_cached].entries_as_ptr;
				_entry.node_attached_cnt--;
				_entry.entries_as_ptr;
			}));
	}
	normal:
	pnode->fwd_entries[entry_index].forward_behavior=NODE_NEXT_EDGE_NONE;
	pnode->fwd_entries[entry_index].last_entry_cached=INVALID_ENTRY;
	pnode->fwd_entries[entry_index].reserved=0;
	pnode->fwd_entries[entry_index].next_node=0;
	return 0;
}
void clean_node_next_edges(const char* cur_node_name)
{
	struct node * pnode=find_node_by_name(cur_node_name);
	if(!pnode)
		return;
	int idx=0;
	for(idx=0;idx<MAX_NR_EDGES;idx++){
		clean_node_next_edge(cur_node_name,idx);
		#if 0
		pnode->fwd_entries[idx].forward_behavior=NODE_NEXT_EDGE_NONE;
		pnode->fwd_entries[idx].last_entry_cached=INVALID_ENTRY;
		pnode->fwd_entries[idx].reserved=0;
		pnode->fwd_entries[idx].next_node=0;
		#endif
	}
}
void dump_node_edges(const char* cur_node_name)
{
	struct node * pnode=find_node_by_name(cur_node_name);
	int idx=0;
	if(!pnode){
		printf("dump error:%s may not be registered yet\n",cur_node_name);
		return;
	}
	printf("dump node edges:%s\n",cur_node_name);
	for(idx=0;idx<MAX_NR_EDGES;idx++){
		switch(pnode->fwd_entries[idx].forward_behavior)
		{
			case NODE_TO_NODE_FWD:
				printf("\tentry index:%d node to node:0x%04x(%s)\n",idx,
					pnode->fwd_entries[idx].next_node,
					({
					struct node* next_node=find_node_by_index(pnode->fwd_entries[idx].next_node);
					next_node?(char*)next_node->name:(char*)"node not found";
					}));
				break;
			case NODE_TO_CLASS_FWD:
				printf("\tentry index:%d node to class:0x%04x(%s)\n",idx,
					pnode->fwd_entries[idx].next_class,
					({
					struct node_class * next_class=find_node_class_by_index(pnode->fwd_entries[idx].next_class);
					next_class?(char*)next_class->class_name:"class not found";
					}));
				break;
			default:

				break;
		}
	}
}

uint16_t reselect_next_forwarding_node(struct node * pnode,
	struct node_class*pclass,
	int next_entry_index,
	int16_t * last_entry_cached)
{
	uint16_t next_node=MAX_NR_NODES;
	int prefered_socket=rte_lcore_to_socket_id(pnode->lcore_id);
	struct node_entry entry,target_entry={.entries_as_ptr=0};
	struct node *pnode_tmp;
	int target_entry_index=-1;
	/*numa socket selection first*/
	int rc;
	int check_socket_id=1;
	again:
	target_entry_index=-1;
	FOREACH_NODE_ENTRY_IN_CLASS_START(pclass,entry)
		if(!validate_node_entry(entry))
			continue;
		pnode_tmp=find_node_by_index(entry.node_index);
		if(!pnode_tmp)
			continue;
		if(check_socket_id&&(rte_lcore_to_socket_id(pnode_tmp->lcore_id)!=prefered_socket))
			continue;
		if(target_entry_index<0){
			target_entry_index=_index;
			target_entry.entries_as_ptr=entry.entries_as_ptr;
			continue;
		}
		if(entry.node_attached_cnt<target_entry.node_attached_cnt){
			target_entry_index=_index;
			target_entry.entries_as_ptr=entry.entries_as_ptr;
		}
	FOREACH_NODE_ENTRY_IN_CLASS_END();
	if(target_entry_index>=0){
		/*atomically increment counter*/
		rc=__sync_bool_compare_and_swap(&pclass->node_entries[target_entry_index].entries_as_ptr,
			target_entry.entries_as_ptr,
			({
			struct node_entry _entry;
			_entry.entries_as_ptr=target_entry.entries_as_ptr;
			_entry.node_attached_cnt++;
			_entry.entries_as_ptr;
			}));
		if(!rc)/*preemption fails*/
			goto again;
		next_node=target_entry.node_index;
		*last_entry_cached=(int16_t)target_entry_index;
		E3_LOG("node2class selection:source node:%d to destination node:%d by edge %d\n",
			(int)pnode->node_index,
			(int)next_node,
			(int)next_entry_index);
	}else if(check_socket_id){
		check_socket_id=0;
		goto again;
	}else ;/*even not one node can be found*/
	
	return next_node;
}