#include <l4-tunnel-process.h>
#include <mbuf_delivery.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <real-server.h>

void l4_tunnel_process_class_early_init(void)
{
	struct node_class * pclass=rte_zmalloc(NULL,sizeof(struct node_class),64);
	E3_ASSERT(pclass);
	sprintf((char*)pclass->class_name,"%s",L4_TUN_NODE_CLASS_NAME);
	pclass->class_reclaim_func=NULL;/*never reclaim this class*/
	pclass->node_class_priv=NULL;
	E3_ASSERT(!register_node_class(pclass));
	E3_ASSERT(find_node_class_by_name(L4_TUN_NODE_CLASS_NAME)==pclass);
	E3_LOG("register node class:%s\n",(char*)pclass->class_name);
}
E3_init(l4_tunnel_process_class_early_init,TASK_PRIORITY_LOW);

#define L4_TUNNEL_PROCESS_FWD_DROP 0x0
#define L4_TUNNEL_PROCESS_FWD_INNER_L2_PROCESS 0x1
#define L4_TUNNEL_PROCESS_FWD_INNER_L3_PROCESS 0x1

__attribute__((always_inline)) 
	static inline int _tunnel_decoede_post_process(
	struct node* pnode,
	struct rte_mbuf **mbufs,
	int nr_mbufs,
	uint64_t fwd_id)
{
	return 0;
}
__attribute__((always_inline)) 
	static inline uint64_t decode_tunnel_packet(struct rte_mbuf* mbuf,
	uint32_t * cached_vni,
	uint8_t * cached_mac,
	uint64_t *cached_search_result)
{
	#define _(c) if(!(c)) goto ret;
	uint64_t fwd_id=MAKE_UINT64(L4_TUNNEL_PROCESS_FWD_DROP,0);
	uint32_t rs_num;
	struct ether_hdr * eth_hdr=rte_pktmbuf_mtod(mbuf,struct ether_hdr*);
	struct ipv4_hdr * ip_hdr=(struct ipv4_hdr*)(eth_hdr+1);
	struct udp_hdr * udp_hdr;
	struct vxlan_hdr * vxlan_hdr;

	struct ether_hdr * inner_eth_hdr;
	
	_(eth_hdr->ether_type==0x0008);
	_(ip_hdr->next_proto_id==0x11);
	udp_hdr=(struct udp_hdr*)(((ip_hdr->version_ihl&0xf)<<2)+(uint8_t*)ip_hdr);
	_(udp_hdr->dst_port==VXLAN_UDP_PORT);
	vxlan_hdr=(struct vxlan_hdr*)(udp_hdr+1);
	_((vxlan_hdr->vx_flags==0x8)&&(!(vxlan_hdr->vx_vni&0xff000000)));
	inner_eth_hdr=(struct ether_hdr *)(vxlan_hdr+1);/*already a vxlan packet*/

	/*even though I optimized real-server indexing using AVX/SSE techniques,
	it's still supposed to be very costy,so we cache last searching result,
	the risk is the real-servers list may change during this scheduling phase,
	and we may miss newly updated real-servers,
	fortunately the result will not be fatal*/
	if(PREDICT_FALSE(!((vxlan_hdr->vx_vni==*cached_vni)
		&&is_ether_address_equal(cached_mac,inner_eth_hdr->s_addr.addr_bytes)))){
		*cached_search_result=search_real_server(vxlan_hdr->vx_vni,inner_eth_hdr->s_addr.addr_bytes);
		*cached_vni=vxlan_hdr->vx_vni;
		copy_ether_address(cached_mac,inner_eth_hdr->s_addr.addr_bytes);
	}
	rs_num=(int)*cached_search_result;
	_(find_real_server_at_index(rs_num));

	
	if(inner_eth_hdr->ether_type==0x0608){
		fwd_id=MAKE_UINT64(L4_TUNNEL_PROCESS_FWD_INNER_L2_PROCESS,0);
		goto ret;
	}
	_(inner_eth_hdr->ether_type==0x0008);/*even inner vlan is not supported*/
	/*here we are not gonna parse inner L3 header ,because it's within another cache line
	otherwise it may influence performance :14+20+8+8+14==64*/
	
	
	#undef _
	ret:
	return fwd_id;
}
int l4_tunnel_process_func(void*arg)
{
	
	int iptr;
	uint64_t fwd_id;
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int process_rc;
	uint64_t last_fwd_id;
	int start_index,end_index;
	
	uint32_t cached_vni=0;
	uint8_t cached_mac[6]={0x0,0x0,0x0,0x0,0x0,0x0};
	uint64_t cached_search_result=-1;
	DEF_EXPRESS_DELIVERY_VARS();
	RESET_EXPRESS_DELIVERY_VARS();
	struct node * pnode=(struct node*)arg;
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,(void**)mbufs,64);
	if(!nr_mbufs)
		return 0;
	/*here we are gonna to decode the packet payload to decide which node to go*/
	pre_setup_env(nr_mbufs);
	while((iptr=peek_next_mbuf())>=0){
		prefetch_next_mbuf(mbufs,iptr);
		/*here we can not use hash+packet_type to track the flow,
		because these flags has nothing to do with inner payload*/
		
		fwd_id=decode_tunnel_packet(mbufs[iptr],&cached_vni,cached_mac,&cached_search_result);

		process_rc=proceed_mbuf(iptr,fwd_id);
		if(process_rc==MBUF_PROCESS_RESTART){
			fetch_pending_index(start_index,end_index);
			fetch_pending_fwd_id(last_fwd_id);
			_tunnel_decoede_post_process(pnode,&mbufs[start_index],end_index-start_index+1,last_fwd_id);
			flush_pending_mbuf();
			proceed_mbuf(iptr,fwd_id);
		}
	}
	fetch_pending_index(start_index,end_index);
	fetch_pending_fwd_id(last_fwd_id);
	if(PREDICT_TRUE(start_index>=0))
		_tunnel_decoede_post_process(pnode,&mbufs[start_index],end_index-start_index+1,last_fwd_id);;
	
	return 0;
}
static int l4_tunnel_node_indicator=0;
int register_l4_tunnel_node(int socket_id)
{
	struct node * pnode=NULL;
	pnode=rte_zmalloc_socket(NULL,sizeof(struct node),64,socket_id);
	if(!pnode){
		E3_ERROR("can not allocate memory\n");
		return -1;
	}
	sprintf((char*)pnode->name,"l4-tunnel-node-%d",l4_tunnel_node_indicator++);
	pnode->lcore_id=(socket_id>=0)?
		get_lcore_by_socket_id(socket_id):
		get_lcore();
		
	if(!validate_lcore_id(pnode->lcore_id)){
		E3_ERROR("lcore id:%d is not legal\n",pnode->lcore_id)
		goto error_node_dealloc;
	}
	pnode->burst_size=NODE_BURST_SIZE;
	pnode->node_priv=find_node_class_by_name(L4_TUN_NODE_CLASS_NAME);
	pnode->node_type=node_type_worker;
	pnode->node_process_func=l4_tunnel_process_func;
	pnode->node_reclaim_func=default_rte_reclaim_func;

	if(register_node(pnode)){
		put_lcore(pnode->lcore_id,0);
		goto error_node_dealloc;
	}
	E3_ASSERT(find_node_by_name((char*)pnode->name)==pnode);

	if(add_node_into_nodeclass(L4_TUN_NODE_CLASS_NAME,(char*)pnode->name)){
		E3_ERROR("adding node:%s to class:%s fails\n",(char*)pnode->name,L4_TUN_NODE_CLASS_NAME);
		goto error_node_unregister;
	}
	if(attach_node_to_lcore(pnode)){
		E3_ERROR("attaching node:%s to lcore fails\n",(char*)pnode->name);
		goto error_detach_node_from_class;
	}
	E3_LOG("register node:%s on lcore %d\n",(char*)pnode->name,pnode->lcore_id);
	return 0;
	error_detach_node_from_class:
		delete_node_from_nodeclass(L4_TUN_NODE_CLASS_NAME,(char*)pnode->name);
	error_node_unregister:
		put_lcore(pnode->lcore_id,0);
		unregister_node(pnode);
		
	error_node_dealloc:
		if(pnode)
			rte_free(pnode);
		return -1;
}

int _unregister_l4_tunnel_node(struct node * pnode)
{
	if(!pnode)
		return -1;
	if(pnode->node_priv!=(void*)find_node_class_by_name(L4_TUN_NODE_CLASS_NAME))
		return -1;
	put_lcore(pnode->lcore_id,0);
	if(detach_node_from_lcore(pnode)){
		E3_WARN("detaching ndoe%s from lcore's task list fails\n",
			(char*)pnode->name);
	}
	
	if(delete_node_from_nodeclass(L4_TUN_NODE_CLASS_NAME,(char*)pnode->name)){
		E3_WARN("detaching ndoe%s from class:%s fails\n",
			(char*)pnode->name,
			L4_TUN_NODE_CLASS_NAME);
	}
	/*delay unregister_node() , other CPU may be dealing with 
	MBUFS in the associated rings ,in safe state we can release them*/
	call_rcu(&pnode->rcu,reclaim_non_input_node_bottom_half);
	E3_LOG("releasing node:%s\n",(char*)pnode->name);
	return 0;
}

void l4_tunnel_process_node_early_init(void)
{
	int idx=0;
	int socket_id;
	foreach_numa_socket(socket_id){
		for(idx=0;idx<L4_TUN_NODES_PER_SOCKET;idx++)
			register_l4_tunnel_node(socket_id);
	}
}
E3_init(l4_tunnel_process_node_early_init,TASK_PRIORITY_EXTRA_LOW);


