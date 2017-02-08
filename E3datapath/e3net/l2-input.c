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
	l2_input_nclass.current_node_nr=0;
	E3_ASSERT(!register_node_class(&l2_input_nclass));
	E3_ASSERT(find_node_class_by_name("l2-input-class")==&l2_input_nclass);
	E3_LOG("register node class:%s\n",l2_input_nclass.class_name);
}

/*allocate a l2_input node ,and register it,I intend to allocate a node for each numa socket*/
/*when socket_id is specified with -1,chose one ourselves */
int register_l2_input_node(int socket_id)
{
	struct node * pnode=NULL;

	pnode=rte_malloc(NULL,sizeof(struct node),64);
	if(!pnode){
		E3_ERROR("could not allocate memory");
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

	E3_LOG("%d\n",pnode->lcore_id)
	return 0;
	
	error_node_dealloc:
		rte_free(pnode);
	return -1;
	
}

