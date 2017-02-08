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
		pnode->fwd_entries[idx].forward_behavior=NODE_NEXT_EDGE_NONE;
		pnode->fwd_entries[idx].last_entry_cached=INVALID_ENTRY;
		pnode->fwd_entries[idx].reserved=0;
		pnode->fwd_entries[idx].next_node=0;
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