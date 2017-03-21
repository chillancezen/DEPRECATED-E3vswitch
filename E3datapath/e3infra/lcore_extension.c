#include <lcore_extension.h>
#include <e3_log.h>
#include <stdio.h>
#include <string.h>

struct node * lcore_task_list[MAX_LCORE_SUPPORTED];
struct e3_lcore lcore_records[MAX_LCORE_SUPPORTED];
struct rte_mempool * gmempool_array[MAX_SOCKET_SUPPORTED];
int nr_sockets=0;

int preserve_lcore_for_io(int lcore_id)
{
	if(!validate_lcore_id(lcore_id))
		return -1;
	lcore_records[lcore_id].attached_nodes=ATTACHED_NODES_PRESERVE_THRESHOLD;
	E3_LOG("lcore %d is preserved exclusively for IO\n",lcore_id);
	return 0;
}

int preserve_lcore_for_worker(int lcore_id)
{
	if(!validate_lcore_id(lcore_id))
		return -1;
	lcore_records[lcore_id].attached_io_nodes=ATTACHED_NODES_PRESERVE_THRESHOLD;
	E3_LOG("lcore %d is preserved exclusively for worker\n",lcore_id);
	return 0;
}


struct rte_mempool * get_mempool_by_socket_id(int socket_id)
{
	if(socket_id>=nr_sockets || socket_id<0)
		return NULL;
	return gmempool_array[socket_id];
}
int _init_per_socket_mempool(void)
{
	int idx;
	char buffer_name[128];
	E3_ASSERT(nr_sockets<=MAX_SOCKET_SUPPORTED);
	for(idx=0;idx<nr_sockets;idx++){
		memset(buffer_name,0x0,sizeof(buffer_name));
		sprintf(buffer_name,"per_socket_mempool-%d",idx);
		gmempool_array[idx]=rte_pktmbuf_pool_create(buffer_name,
				DEFAULT_NR_MBUF_PERSOCKET,
				MEMPOOL_CACHE_SIZE, 
				0, 
				RTE_MBUF_DEFAULT_BUF_SIZE,
				idx);
		E3_ASSERT(gmempool_array[idx]);
	}
	return 0;	
}

/*extension of DPDK eal init,should be called immediately after rte_eal_init()*/
int init_lcore_extension(void)
{
	unsigned lcore_id;
	int max_socket=0;
	RTE_LCORE_FOREACH(lcore_id){
		lcore_records[lcore_id].is_enabled=1;
		lcore_records[lcore_id].socket_id=rte_lcore_to_socket_id(lcore_id);
		lcore_records[lcore_id].attached_nodes=0;
		lcore_records[lcore_id].attached_io_nodes=0;
		if(lcore_records[lcore_id].socket_id>max_socket)
			max_socket=lcore_records[lcore_id].socket_id;
	}
	
	nr_sockets=max_socket+1;
	E3_ASSERT(nr_sockets<=MAX_SOCKET_SUPPORTED);
	_init_per_socket_mempool();
	return 0;
	
}
inline int  lcore_to_socket_id(int lcore_id)
{
	return rte_lcore_to_socket_id(lcore_id);
}

int  get_lcore_by_socket_id(int socket_id)
{
	int target_lcore_id=-1;
	int lcore_id;
	RTE_LCORE_FOREACH(lcore_id){/*all the lcores are online*/
	#if defined(PRESERVE_MASTER_LCORE)
		if(lcore_id==rte_get_master_lcore())
			continue;
	#endif
		if(lcore_records[lcore_id].attached_nodes==ATTACHED_NODES_PRESERVE_THRESHOLD)
			continue;
		if(lcore_records[lcore_id].socket_id!=socket_id)
			continue;
		if(target_lcore_id<0){
			target_lcore_id=lcore_id;
			continue;
		}
		if(lcore_records[lcore_id].attached_nodes<lcore_records[target_lcore_id].attached_nodes)
			target_lcore_id=lcore_id;
	}
	if(target_lcore_id>=0){
		lcore_records[target_lcore_id].attached_nodes++;
	}
	return target_lcore_id;
}
int  get_lcore(void)
{
	int target_lcore_id=-1;
	int lcore_id;
	RTE_LCORE_FOREACH(lcore_id){
	#if defined(PRESERVE_MASTER_LCORE)
		if(lcore_id==rte_get_master_lcore())
			continue;
	#endif
		if(lcore_records[lcore_id].attached_nodes==ATTACHED_NODES_PRESERVE_THRESHOLD)
			continue;
		if(target_lcore_id<0){
			target_lcore_id=lcore_id;
			continue;
		}
		if(lcore_records[lcore_id].attached_nodes<lcore_records[target_lcore_id].attached_nodes)
			target_lcore_id=lcore_id;
	}
	
	if(target_lcore_id>=0){
		lcore_records[target_lcore_id].attached_nodes++;
	}
	return (int)target_lcore_id;
}

int  get_io_lcore_by_socket_id(int socket_id)
{
	int target_lcore_id=-1;
	int lcore_id;
	RTE_LCORE_FOREACH(lcore_id){
	#if defined(PRESERVE_MASTER_LCORE)
		if(lcore_id==rte_get_master_lcore())
			continue;
	#endif
		if(lcore_records[lcore_id].attached_io_nodes==ATTACHED_NODES_PRESERVE_THRESHOLD)
			continue;
		if(lcore_records[lcore_id].socket_id!=socket_id)
			continue;
		if(target_lcore_id<0){
			target_lcore_id=lcore_id;
			continue;
		}
		if(lcore_records[lcore_id].attached_io_nodes<lcore_records[target_lcore_id].attached_io_nodes)
			target_lcore_id=lcore_id;
	}
	if(target_lcore_id>=0)
		lcore_records[target_lcore_id].attached_io_nodes++;
	return target_lcore_id;
}

int get_io_lcore(void)
{
	int target_lcore_id=-1;
	int lcore_id;
	RTE_LCORE_FOREACH(lcore_id){
	#if defined(PRESERVE_MASTER_LCORE)
		if(lcore_id==rte_get_master_lcore())
			continue;
	#endif
		if(lcore_records[lcore_id].attached_io_nodes==ATTACHED_NODES_PRESERVE_THRESHOLD)
			continue;
		if(target_lcore_id<0){
			target_lcore_id=lcore_id;
			continue;
		}
		if(lcore_records[lcore_id].attached_io_nodes<lcore_records[target_lcore_id].attached_io_nodes)
			target_lcore_id=lcore_id;
	}
	if(target_lcore_id>=0)
		lcore_records[target_lcore_id].attached_io_nodes++;
	return target_lcore_id;
}

void put_lcore(int lcore_id,int  is_io)
{
	#if defined(PRESERVE_MASTER_LCORE)
	if(lcore_id==rte_get_master_lcore())
		return;
	#endif
	if(lcore_id<0 || lcore_id>=MAX_LCORE_SUPPORTED)
		return;
	if(is_io)
		lcore_records[lcore_id].attached_io_nodes--;
	else
		lcore_records[lcore_id].attached_nodes--;
}
int attach_node_to_lcore(struct node *node)
{
	struct node *pnode=NULL;
	int lcore_id=node->lcore_id;
	if(lcore_id<0)
		lcore_id=rte_get_master_lcore();/*todo:choose a least-loaded lcore to accommodate this node*/
										/*Feb 8 ,2017:master core service it for simplicity*/
	if(lcore_id>=MAX_LCORE_SUPPORTED)
		return -1;
	for(pnode=lcore_task_list[lcore_id];pnode&&(pnode!=node);pnode=pnode->next);
	if(pnode)
		return -2;
	node->next=lcore_task_list[lcore_id];
	rcu_assign_pointer(lcore_task_list[lcore_id],node);
	
	return 0;
}

int detach_node_from_lcore(struct node*node)
{
	int lcore_id=node->lcore_id;
	if(lcore_id<0)/*not yet attached*/
		return 0;
	if(lcore_id>=MAX_LCORE_SUPPORTED)
		return -1;
	if(!lcore_task_list[lcore_id])
		return -2;
	if(lcore_task_list[lcore_id]==node){
		rcu_assign_pointer(lcore_task_list[lcore_id],node->next);
		return 0;
	}else{
		struct node * parent_node=lcore_task_list[lcore_id];
		while(parent_node&&parent_node->next){
			if(parent_node->next==node){
				rcu_assign_pointer(parent_node->next,node->next);
				return 0;
			}
			parent_node=parent_node->next;
		}
	}
	return -3;
}
uint64_t get_lcore_task_list_base(void)
{
	return (uint64_t)(&lcore_task_list[0]);
}
int dump_lcore_list(FILE*fp)
{
	struct node* node;
	int lcore_id=0;
	for(lcore_id=0;lcore_id<MAX_LCORE_SUPPORTED;lcore_id++){
		if(!lcore_task_list[lcore_id])
			continue;
		printf("lcore task list id:%d ",lcore_id);
		for(node=lcore_task_list[lcore_id];node;node=node->next)
			printf(" (%d %s) ",node->node_index,node->name);
		puts("");
	}
	return 0;
}



__attribute__((constructor)) void lcore_extension_module_init(void)
{
	int idx=0;
	for(idx=0;idx<MAX_LCORE_SUPPORTED;idx++){
		lcore_task_list[idx]=NULL;
		lcore_records[idx].is_enabled=0;
		lcore_records[idx].attached_nodes=0;
		lcore_records[idx].socket_id=0;
		lcore_records[idx].attached_io_nodes=0;
	}
	for(idx=0;idx<MAX_SOCKET_SUPPORTED;idx++)
		gmempool_array[idx]=NULL;
}


int lcore_extension_test()
{
	
	return 0;
}
static int lcore_should_stop(void)
{
	return 0;
}

int lcore_default_entry(__attribute__((unused)) void *arg)
{
	int last_cnt=0;
	int cnt=0;
	struct node* pnode;
	unsigned lcore_id=rte_lcore_id();
	rcu_register_thread();
	while(!lcore_should_stop()){
		cnt=0;
		foreach_node_in_lcore(pnode,lcore_id){
			cnt++;
			pnode->node_process_func(pnode);
			rcu_quiescent_state();
		}
		
		if(cnt!=last_cnt){
			last_cnt=cnt;
			#if 0
			E3_LOG("node in lcore %d:%d\n",lcore_id,cnt);
			#endif
		}
		
		if(!cnt)
			rcu_quiescent_state();
	}
	rcu_thread_offline();
	rcu_unregister_thread();
	return 0;
}

