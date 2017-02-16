/*here we just wanna verify l2 function and control plane functions,
so we let l2 forwarding functions be implemented here*/

#include <l2-input.h>
#include <l2-learn.h>
#include <e3_log.h>
#include <rte_malloc.h>
#include <lcore_extension.h>
#include <util.h>
#include <mbuf_delivery.h>
#include <vlan-list.h>
#include <l2fib.h>
#include <rte_ether.h>

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
#define L2_MBUF_PROCESS_DROP 0x0
#define L2_MBUF_PROCESS_DIRECT_PORT_OUTPUT 0x1
#define L2_MBUF_PROCESS_MULTI_PORT_OUTPUT 0x2

__attribute__((always_inline)) static inline int _l2_input_node_post_process(
	struct node*pnode,
	struct rte_mbuf **mbufs,
	int nr_mbufs,
	uint64_t fwd_id)
{
	int idx;
	int nr_delivered;
	uint64_t hi_dword=HIGH_UINT64(fwd_id);
	uint64_t lo_dword=LOW_UINT64(fwd_id);
	struct E3interface * pif_dst,*pif_tmp,*pif_src,*pif_last;
	int drop_start=0;
	int nr_dropped=0;
	
	struct rte_mbuf * mbuf_multicast_array[64];/*it's supposed to be sufficient*/
	int nr_mbufs_multicast;
	
	if(nr_mbufs<=0)
		return 0;
	switch(hi_dword)
	{	
		case L2_MBUF_PROCESS_DIRECT_PORT_OUTPUT:
			printf("unicast:%d\n",nr_mbufs);
			pif_dst=interface_at_index((uint16_t)lo_dword);
			nr_delivered=deliver_mbufs_to_node(pif_dst->output_node,mbufs,nr_mbufs);
			drop_start=nr_delivered;
			nr_dropped=nr_mbufs-nr_delivered;
			break;
		case L2_MBUF_PROCESS_MULTI_PORT_OUTPUT:
			printf("multicast:%d\n",nr_mbufs);
			pif_last=NULL;
			pif_src=interface_at_index((uint16_t)lo_dword);
			foreach_interface_in_vlan_safe(pif_tmp,pif_src->access_vlan){
				if(pif_tmp==pif_src)/*skip ingress port*/
					continue;
				if(!validate_interface(pif_tmp))
					continue;
				if(pif_last){/*clone mbufs arrary,forward to last recorded interface*/
					nr_mbufs_multicast=clone_mbufs_array(mbuf_multicast_array,mbufs,nr_mbufs);
					nr_delivered=deliver_mbufs_to_node(pif_last->output_node,mbuf_multicast_array,nr_mbufs_multicast);
					/*free unsent packets*/
					for(idx=nr_delivered;idx<nr_mbufs_multicast;idx++)
						rte_pktmbuf_free(mbuf_multicast_array[idx]);
				}
				pif_last=pif_tmp;
			}
			nr_delivered=0;
			nr_dropped=nr_mbufs;
			if(pif_last){
				nr_delivered=deliver_mbufs_to_node(pif_last->output_node,mbufs,nr_mbufs);
				drop_start=nr_delivered;
				nr_dropped=nr_mbufs-nr_delivered;
			}
			break;
		case L2_MBUF_PROCESS_DROP:
			nr_dropped=nr_mbufs;
			break;
	}
	for(idx=0;idx<nr_dropped;idx++)
		rte_pktmbuf_free(mbufs[drop_start+idx]);
	return 0;
}

int l2_input_node_process_func(void *arg)
{
	struct node * pnode=(struct node*)arg;
	struct rte_mbuf * mbufs[64];
	uint64_t commands[2];
	uint64_t fwd_id=1;
	int process_rc;
	int iptr=0;
	int nr_mbufs;
	//int nr_delivered=0;
	int start_index=0,end_index=0;
	uint64_t last_fwd_id;
	struct ether_hdr * ether_ptr=NULL;
	struct E3interface *pif=NULL;
	struct l2fib_key l2_key,l2_src_key;
	int entry_index,src_entry_index;
	struct l2fib_entry *entry,*src_entry;

	DEF_EXPRESS_DELIVERY_VARS();
	RESET_EXPRESS_DELIVERY_VARS();
	struct l2fib_key last_cached_key,last_cached_src_key;
	uint16_t last_cached_output_port;
	
	l2_key.key_as64=0;
	l2_src_key.key_as64=0;
	last_cached_key.key_as64=0;
	last_cached_src_key.key_as64=0;
	last_cached_output_port=-1;
	
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,(void**)mbufs,E3_MIN(pnode->burst_size,64));
	pre_setup_env(nr_mbufs);
	while((iptr=peek_next_mbuf())>=0){
		/*handle mbuf at index:iptr,and as a return ,fwd_id is set*/
		ether_ptr=rte_pktmbuf_mtod(mbufs[iptr],struct ether_hdr *);
		pif=interface_at_index(mbufs[iptr]->port);
		
		if(PREDICT_FALSE(!validate_interface(pif))){
			fwd_id=MAKE_UINT64(L2_MBUF_PROCESS_DROP,0);
		}else{
			/*forward  in a VLAN which inport belongs to*/
			if(is_multicast_ether_addr(&ether_ptr->d_addr))
				fwd_id=MAKE_UINT64(L2_MBUF_PROCESS_MULTI_PORT_OUTPUT,mbufs[iptr]->port);
			else{/*try to find a port basing on vlan+dest mac*/
				l2_key.vlan_id=pif->access_vlan;
				copy_mac_addr(l2_key.mac_addr,ether_ptr->d_addr.addr_bytes);
				if(l2_key.key_as64==last_cached_key.key_as64){/*already found in the cache*/
					fwd_id=MAKE_UINT64(L2_MBUF_PROCESS_DIRECT_PORT_OUTPUT,last_cached_output_port);
				}else{
					/*this is the only place where l2fib is searched*/
					index_key_safe(l2_key,entry_index,entry);
					if(entry_index>=0){/*an entry is found*/
						last_cached_key.key_as64=l2_key.key_as64;
						last_cached_output_port=l2fib_port(entry,entry_index);
						fwd_id=MAKE_UINT64(L2_MBUF_PROCESS_DIRECT_PORT_OUTPUT,last_cached_output_port);
					}else{ /*can not find entry,broadcast it in vlan*/
						fwd_id=MAKE_UINT64(L2_MBUF_PROCESS_MULTI_PORT_OUTPUT,mbufs[iptr]->port);
					}
				}
			}
			/*L2 learning*/
			if(PREDICT_TRUE(is_unicast_ether_addr(&ether_ptr->s_addr))){
				l2_src_key.vlan_id=pif->access_vlan;
				copy_mac_addr(l2_src_key.mac_addr,ether_ptr->s_addr.addr_bytes);
				if(PREDICT_FALSE(l2_src_key.key_as64!=last_cached_src_key.key_as64)){
					last_cached_src_key.key_as64=l2_src_key.key_as64;
					index_key_safe(l2_src_key,src_entry_index,src_entry);
					if(PREDICT_FALSE(src_entry_index<0)){
						/*entry not found,notify learning entity*/
						commands[0]=MAKE_UINT64(L2_COMMAND_ADD_FIB,mbufs[iptr]->port);
						commands[1]=l2_src_key.key_as64;
						deliver_message_to_node(pnode_l2_learn->node_index,(void**)commands,16);
					}else{
						if(PREDICT_FALSE(l2fib_port(src_entry,src_entry_index)!=mbufs[iptr]->port))
							l2fib_port(src_entry,src_entry_index)=mbufs[iptr]->port;
						l2fib_timestamp(src_entry,src_entry_index)=rte_rdtsc();
					}
				}
			}
		}

		/*end of fwd_id decision*/
		process_rc=proceed_mbuf(iptr,fwd_id);
		if(process_rc==MBUF_PROCESS_RESTART){
			fetch_pending_index(start_index,end_index);
			/*send pending mbufs to next node right now,note that if not all the mbufs are sent,
			the logic should free them itself*/
			fetch_pending_fwd_id(last_fwd_id);
			_l2_input_node_post_process(pnode,&mbufs[start_index],end_index-start_index+1,last_fwd_id);
			flush_pending_mbuf();
			proceed_mbuf(iptr,fwd_id);/*re try ,it will be MBUF_PROCESS_OK this time*/
		}
	}
	/*resend send left mbufs still in the buffer to next nodes*/
	fetch_pending_index(start_index,end_index);
	fetch_pending_fwd_id(last_fwd_id);
	if(start_index>=0)
		_l2_input_node_post_process(pnode,&mbufs[start_index],end_index-start_index+1,last_fwd_id);
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



