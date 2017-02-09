#include <l2-input.h>
#include <e3_log.h>
#include <rte_malloc.h>
#include <lcore_extension.h>


/*register l2-input-class */
static struct  node_class l2_input_nclass;
static struct l2_input_main l2_input_main;


void l2_input_early_init(void)
{
	memset(&l2_input_nclass,0x0,sizeof(struct node_class));
	memset(&l2_input_main,0x0,sizeof(struct l2_input_main));
	strcpy((char*)l2_input_nclass.class_name,"l2-input-class");
	l2_input_nclass.class_reclaim_func=NULL;
	l2_input_nclass.node_class_priv=&l2_input_main;
	E3_ASSERT(!register_node_class(&l2_input_nclass));
	E3_ASSERT(find_node_class_by_name("l2-input-class")==&l2_input_nclass);
	E3_LOG("register node class:%s\n",l2_input_nclass.class_name);
}

int l2_input_node_process_func(void *arg)
{

	return 0;
}


/*allocate a l2_input node ,and register it,I intend to allocate a node for each numa socket*/
/*when socket_id is specified with -1,chose one ourselves */
int register_l2_input_node(int socket_id)
{
	struct node * pnode=NULL;
	int rc;
	pnode=rte_malloc(NULL,sizeof(struct node),64);
	if(!pnode){
		E3_ERROR("could not allocate memory\n");
		return -1;
		goto error_node_dealloc;
	}
	memset(pnode,0x0,sizeof(struct node));
	sprintf((char*)pnode->name,"l2-input-node-%d",l2_input_main.node_allocation_indicator++);
	
	pnode->lcore_id=(socket_id>=0)?
		get_lcore_by_socket_id(socket_id):
		get_lcore();
		
	if(!validate_lcore_id(pnode->lcore_id)){
		E3_ERROR("can not allocate A lcore id\n");
		goto error_node_dealloc;
	}
	pnode->burst_size=NODE_BURST_SIZE;
	pnode->node_priv=&l2_input_nclass;
	pnode->node_type=node_type_worker;
	pnode->node_process_func=l2_input_node_process_func;
	pnode->node_reclaim_func=default_rte_reclaim_func;

	rc=register_node(pnode);
	if(rc){
		put_lcore(pnode->lcore_id,0);
		goto error_node_dealloc;
	}
	/*verify node registration*/
	if(find_node_by_name((char*)pnode->name)!=pnode){
		E3_ERROR("verifying node registration:%s fails\n",(char*)pnode->name);
		goto error_node_unregister;
	}
	
	/*put this node into l2-input-class*/
	rc=add_node_into_nodeclass((char*)l2_input_nclass.class_name,(char*)pnode->name);
	if(rc){
		E3_ERROR("adding node %s into class:%s fails\n",
			(char*)pnode->name,
			(char*)l2_input_nclass.class_name);
		goto error_node_unregister;
	}
	/*add node into lcore's task list*/
	rc=attach_node_to_lcore(pnode);
	if(rc){
		E3_ERROR("can not attach node:%s to lcore %d 's task list\n",
			(char*)pnode->name,
			(int)pnode->lcore_id);
		goto error_node_unregister;
	}
	
	E3_LOG("register node:%s affiliated to class:%s on %d\n",
		(char*)pnode->name,
		(char*)l2_input_nclass.class_name,
		(int)pnode->lcore_id);

	return 0;
	error_node_unregister:
		put_lcore(pnode->lcore_id,0);
		unregister_node(pnode);
		return -2;/*return ,let reclaim function do deallocation*/
		
	error_node_dealloc:
		rte_free(pnode);
	return -1;
	
}



int unregister_l2_input_node(const char * node_name)
{
	int rc=0;
	struct node * pnode=find_node_by_name(node_name);
	if(!pnode){
		E3_WARN("the node:%s does not exist\n",node_name);
		return -1;
	}
	/*detach node from lcore's task list*/
	put_lcore(pnode->lcore_id,0);
	rc=detach_node_from_lcore(pnode);
	if(rc){
		E3_WARN("detaching node:%s from lcore's task list fails\n",
			(char*)pnode->name);
	}
	/*try to detach node from class now*/
	rc=delete_node_from_nodeclass((char*)l2_input_nclass.class_name,(char*)pnode->name);
	if(rc){
		E3_WARN("detaching node:%s from class:%s fails\n",
			(char*)pnode->name,
			(char*)l2_input_nclass.class_name);
		return -2;
	}
	/*call unregistration bottom half routine*/
	#if 0
		#error "doing so may cause memory leak in node's ring buffer "
		#error "resource  must be deallocated in quiescent state"
		unregister_node(pnode);
	#else
		call_rcu(&pnode->rcu,reclaim_non_input_node_bottom_half);
	#endif
	E3_LOG("delete node:%s\n",node_name);
	
	return 0;
}
	
void l2_input_runtime_init(void)
{
	int idx;
	int socket_id;
	foreach_numa_socket(socket_id){
		for(idx=0;idx<L2_INPUT_NODES_PER_SOCKET;idx++)
			register_l2_input_node(socket_id);
	}
}



