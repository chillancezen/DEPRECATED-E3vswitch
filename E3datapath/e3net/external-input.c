#include <external-input.h>
#include <node.h>
#include <node_class.h>
#include <init.h>
#include <util.h>
#include <rte_malloc.h>
#include <lcore_extension.h>
#include <vip-resource.h>
#include <mbuf_delivery.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>

#define EXTERNAL_INPUT_CLASS_NAME "external-input"
#define EXTERNAL_INPUT_NODES_PER_SOCKET 2


#define EXTERNAL_INPUT_PROCESS_FWD_DROP 0x0

void external_input_process_class_early_init(void)
{
	struct node_class * pclass=rte_zmalloc(NULL,sizeof(struct node_class),64);
	E3_ASSERT(pclass);
	sprintf((char*)pclass->class_name,"%s",EXTERNAL_INPUT_CLASS_NAME);
	pclass->class_reclaim_func=NULL;/*never reclaim this class*/
	pclass->node_class_priv=NULL;
	E3_ASSERT(!register_node_class(pclass));
	E3_ASSERT(find_node_class_by_name(EXTERNAL_INPUT_CLASS_NAME)==pclass);
	E3_LOG("register node class:%s\n",(char*)pclass->class_name);
}
E3_init(external_input_process_class_early_init,TASK_PRIORITY_LOW);

__attribute__((always_inline))
	static inline uint64_t translate_inbound_packet(struct rte_mbuf * mbuf,
	uint32_t *plast_cached_packet_type,
	uint32_t *plsst_cached_packet_hash,
	uint64_t *plast_cached_fwd_id)
{	

	#define _(con) if(!(con)) goto ret;
	int nr_vip;
	struct virtual_ip * pvip;
	uint64_t fwd_id=MAKE_UINT64(EXTERNAL_INPUT_PROCESS_FWD_DROP,0);
	struct ether_hdr * eth_hdr=rte_pktmbuf_mtod(mbuf,struct ether_hdr *);
	struct ipv4_hdr  * ip_hdr=(struct ipv4_hdr*)(eth_hdr+1);
	
	if(PREDICT_TRUE(((mbuf->packet_type==*plast_cached_packet_type)&&
		(mbuf->hash.fdir.lo==*plsst_cached_packet_hash)))){
		fwd_id=*plast_cached_fwd_id;
		goto ret;
	}
	_(eth_hdr->ether_type==0x0008);
	_((ip_hdr->next_proto_id==0x11)||(ip_hdr->next_proto_id==0x6));
	nr_vip=search_virtual_ip_index(ip_hdr->dst_addr);
	_(pvip=find_virtual_ip_at_index(nr_vip));
	printf("found vip:%p,%d\n",pvip,nr_vip);
	
	#undef _
	ret:
	return fwd_id;
}

int external_input_node_process_func(void* arg)
{
	struct node * pnode=(struct node *)arg;
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int iptr;
	int process_rc;
	int start_index,end_index;
	uint64_t fwd_id;
	uint64_t last_fwd_id;

	uint32_t last_cached_packet_type=0;
	uint32_t last_cached_packet_hash=0;
	uint64_t last_cached_fwd_id=MAKE_UINT64(EXTERNAL_INPUT_PROCESS_FWD_DROP,0);
	
	DEF_EXPRESS_DELIVERY_VARS();
	RESET_EXPRESS_DELIVERY_VARS();
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,(void**)mbufs,64);
	if(!nr_mbufs)
		return 0;
	pre_setup_env(nr_mbufs);
	while((iptr=peek_next_mbuf())>=0){
		prefetch_next_mbuf(mbufs,iptr);
		fwd_id=translate_inbound_packet(mbufs[iptr],
			&last_cached_packet_type,
			&last_cached_packet_hash,
			&last_cached_fwd_id);
		process_rc=proceed_mbuf(iptr,fwd_id);
		if(process_rc==MBUF_PROCESS_RESTART){
			fetch_pending_index(start_index,end_index);
			fetch_pending_fwd_id(last_fwd_id);
			
			flush_pending_mbuf();
			proceed_mbuf(iptr,fwd_id);
		}
	}
	fetch_pending_index(start_index,end_index);
	fetch_pending_fwd_id(last_fwd_id);
	return 0;
}
static int external_input_node_indicator=0;
int register_external_input_node(int socket_id)
{
	struct node * pnode=NULL;
	pnode=rte_zmalloc_socket(NULL,sizeof(struct node),64,socket_id);
	if(!pnode){
		E3_ERROR("can not allocate memory\n");
		return -1;
	}
	sprintf((char*)pnode->name,"ext-input-node-%d",external_input_node_indicator++);
	pnode->lcore_id=(socket_id>=0)?
		get_lcore_by_socket_id(socket_id):
		get_lcore();
		
	if(!validate_lcore_id(pnode->lcore_id)){
		E3_ERROR("lcore id:%d is not legal\n",pnode->lcore_id)
		goto error_node_dealloc;
	}
	pnode->burst_size=NODE_BURST_SIZE;
	pnode->node_priv=find_node_class_by_name(EXTERNAL_INPUT_CLASS_NAME);
	pnode->node_type=node_type_worker;
	pnode->node_process_func=external_input_node_process_func;
	pnode->node_reclaim_func=default_rte_reclaim_func;

	if(register_node(pnode)){
		put_lcore(pnode->lcore_id,0);
		goto error_node_dealloc;
	}
	E3_ASSERT(find_node_by_name((char*)pnode->name)==pnode);

	if(add_node_into_nodeclass(EXTERNAL_INPUT_CLASS_NAME,(char*)pnode->name)){
		E3_ERROR("adding node:%s to class:%s fails\n",(char*)pnode->name,EXTERNAL_INPUT_CLASS_NAME);
		goto error_node_unregister;
	}
	if(attach_node_to_lcore(pnode)){
		E3_ERROR("attaching node:%s to lcore fails\n",(char*)pnode->name);
		goto error_detach_node_from_class;
	}
	/*setup next edges*/
	
	E3_LOG("register node:%s on lcore %d\n",(char*)pnode->name,pnode->lcore_id);
	return 0;
	error_detach_node_from_class:
		delete_node_from_nodeclass(EXTERNAL_INPUT_CLASS_NAME,(char*)pnode->name);
	error_node_unregister:
		put_lcore(pnode->lcore_id,0);
		unregister_node(pnode);
		
	error_node_dealloc:
		if(pnode)
			rte_free(pnode);
		return -1;	
	return 0;
}

void external_input_node_early_init(void)
{
	int idx=0;
	int socket_id;
	foreach_numa_socket(socket_id){
		for(idx=0;idx<EXTERNAL_INPUT_NODES_PER_SOCKET;idx++)
			register_external_input_node(socket_id);
	}	
}
E3_init(external_input_node_early_init,TASK_PRIORITY_ULTRA_LOW);

