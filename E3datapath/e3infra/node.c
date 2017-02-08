#include <node.h>
#include <stdio.h>
#include <string.h>
#include <lcore_extension.h>


struct node *gnode_array[MAX_NR_NODES];


int register_node(struct node *node)
{
	int idx=0;
	char buffer_name[128];
	/*two nodes can not have same names*/
	if(find_node_by_name((char*)node->name)){
		E3_ERROR("node name:%s already exists\n",(char*)node->name);
		return -1;
	}
	/*check that whether this node is already registered*/
	for(idx=0;idx<MAX_NR_NODES;idx++)
		if(gnode_array[idx]==node){
			E3_ERROR("node:%s already exists\n",(char*)node->name);
			return -1;
		}
	idx=0;
	for(;idx<MAX_NR_NODES;idx++)/*find a free slot to accommodate new node*/
		if(!gnode_array[idx])
			break;
	if(idx==MAX_NR_NODES)/*buffer overflows*/{
		E3_ERROR("no enough room for node:%s\n",(char*)node->name);
		return -2;
	}
	/*alocate mempool/mem-ring resource*/
	switch(node->node_type)
	{
		case node_type_input:
			node->node_ring=NULL;
			#if 0
			memset(buffer_name,0x0,sizeof(buffer_name));
			sprintf(buffer_name,"%s-mempool",(char*)node->name);
			node->node_pool= rte_pktmbuf_pool_create(buffer_name,
				DEFAULT_NR_MBUF_PERPORT,
				MEMPOOL_CACHE_SIZE, 
				0, 
				RTE_MBUF_DEFAULT_BUF_SIZE,
				-1/*todo:socket decision*/);
			if(!node->node_pool){
				E3_ERROR("allocating mempool:%s fails\n",buffer_name);
				return -3;
			}
			#endif
			break;
		default :
			memset(buffer_name,0x0,sizeof(buffer_name));
			sprintf(buffer_name,"%s-mempool",(char*)node->name);
			node->node_ring=rte_ring_create(buffer_name,
				DEFAULT_NR_RING_PERNODE,
				lcore_to_socket_id(node->lcore_id)/*todo:socket decision*/,
				RING_F_SC_DEQ);
			if(!node->node_ring){
				E3_ERROR("allocating ring:%s fails\n",buffer_name);
				return -3;
			}
			break;
	}
	
	node->node_index=idx;
	rcu_assign_pointer(gnode_array[idx],node);
	
	return 0;
}
void unregister_node(struct node * node)
{
	int idx=0;
	for(idx=0;idx<MAX_NR_NODES;idx++)
		if(gnode_array[idx]==node)
			break;
	if(idx==MAX_NR_NODES){
		E3_ERROR("node %s may not be registered yet\n",(char*)node->name);
		return ;
	}
	
	rcu_assign_pointer(gnode_array[idx],NULL);
	switch(node->node_type)
	{
		case node_type_input:
			break;
		default:
			if(node->node_ring){
				rte_ring_free(node->node_ring);
				node->node_ring=NULL;
			}
			
			break;
	}
	if(node->node_reclaim_func)
		call_rcu(&node->rcu,node->node_reclaim_func);
}

void dump_nodes(FILE*fp)
{
	int idx=0;
	for(idx=0;idx<MAX_NR_NODES;idx++){
		if(gnode_array[idx])
			fprintf(fp,"node :%d %s\n",gnode_array[idx]->node_index,
			gnode_array[idx]->name);
	}
}

int node_module_test(void)
{	
	return 0;
}

__attribute__((constructor)) void node_module_init(void)
{
	int idx=0;
	for(idx=0;idx<MAX_NR_NODES;idx++)
		gnode_array[idx]=NULL;

}




