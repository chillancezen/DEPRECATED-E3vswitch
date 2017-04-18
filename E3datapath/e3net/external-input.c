#include <external-input.h>
#include <node.h>
#include <node_class.h>
#include <init.h>
#include <util.h>
#include <rte_malloc.h>
#include <lcore_extension.h>
#include <vip-resource.h>
#include <mbuf_delivery.h>
#include <lb-instance.h>
#include <real-server.h>
#include <l3-interface.h>
#include <mq-device.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <vxlan-encap.h>
#define EXTERNAL_INPUT_CLASS_NAME "external-input"
#define EXTERNAL_INPUT_NODES_PER_SOCKET 2


#define EXTERNAL_INPUT_PROCESS_FWD_DROP 0x0
#define EXTERNAL_INPUT_PROCESS_FWD_XMIT 0x1


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


#define NEXT_FWD_ID
__attribute__((always_inline))
	static inline uint64_t translate_inbound_packet(struct rte_mbuf * mbuf,
	uint16_t *plast_cached_port,
	uint32_t *plast_cached_packet_type,
	uint32_t *plsst_cached_packet_hash,
	struct virtual_ip ** plast_cached_pvip,
	struct lb_instance ** plast_cached_plb,
	struct real_server ** plast_cached_prs)
{	

	#define _(con) if(!(con)) goto ret;
	int nr_vip;
	int rs_index;
	int nr_xmit_queue;
	uint32_t lo_part;
	struct virtual_ip	* pvip;
	struct lb_instance 	* lb;
	struct real_server 	* preal_srv;
	struct l3_interface * plocal_l3iface;
	struct l3_interface * pphy_l3iface;
	struct E3interface  * pe3_iface;
	uint64_t fwd_id=MAKE_UINT64(EXTERNAL_INPUT_PROCESS_FWD_DROP,0);
	struct ether_hdr * eth_hdr=rte_pktmbuf_mtod(mbuf,struct ether_hdr *);
	struct ipv4_hdr  * ip_hdr=(struct ipv4_hdr*)(eth_hdr+1);
	struct udp_hdr   * udp_hdr;
	struct tcp_hdr   * tcp_hdr;
	if(PREDICT_TRUE(((mbuf->packet_type==*plast_cached_packet_type)&&
		(mbuf->hash.fdir.lo==*plsst_cached_packet_hash)&&
		(mbuf->port==*plast_cached_port)))){
		pvip=*plast_cached_pvip;
		lb=*plast_cached_plb;
		preal_srv=*plast_cached_prs;
	}else{/*decode the packet payload to determine where to go,
		anything wrong leads to direct return,and does not track previous state*/
		_(eth_hdr->ether_type==0x0008);
		_((ip_hdr->next_proto_id==0x11)||(ip_hdr->next_proto_id==0x6));
		nr_vip=search_virtual_ip_index(ip_hdr->dst_addr);
		_(pvip=find_virtual_ip_at_index(nr_vip));
		_(lb=find_lb_instance_at_index(pvip->lb_instance_index));
		rs_index=lb->indirection_table[INDIRECTION_TABLE_MASK&mbuf->hash.fdir.lo];
		_(preal_srv=find_real_server_at_index(rs_index));
		*plast_cached_pvip=pvip;
		*plast_cached_plb=lb;
		*plast_cached_prs=preal_srv;

		*plast_cached_packet_type=mbuf->packet_type;
		*plast_cached_port=mbuf->port;
		*plsst_cached_packet_hash=mbuf->hash.fdir.lo;
	}

	_(pvip&&lb&&preal_srv);
	_(rte_pktmbuf_prepend(mbuf,50));
	_(plocal_l3iface=find_l3_interface_at_index(preal_srv->lb_iface));
	_(pphy_l3iface=find_l3_interface_at_index(plocal_l3iface->lower_if_index));
	_(pe3_iface=find_e3iface_by_index(pphy_l3iface->lower_if_index));
	
	
	/*rewrite ip hdr and upper-layer pseudo header*/
	ip_hdr->dst_addr=preal_srv->rs_ipv4;
	ip_hdr->hdr_checksum=0;
	switch(ip_hdr->next_proto_id)
	{
		case 0x11:
			udp_hdr=(struct udp_hdr*)(((ip_hdr->version_ihl&0xf)<<2)+(uint8_t*)ip_hdr);
			if(udp_hdr->dgram_cksum){
				udp_hdr->dgram_cksum=rte_ipv4_phdr_cksum(ip_hdr,0);
				mbuf->ol_flags=PKT_TX_UDP_CKSUM;
			}
			break;
		case 0x6:
			tcp_hdr=(struct tcp_hdr*)(((ip_hdr->version_ihl&0xf)<<2)+(uint8_t*)ip_hdr);
			tcp_hdr->cksum=rte_ipv4_phdr_cksum(ip_hdr,0);
			mbuf->ol_flags=PKT_TX_TCP_CKSUM;
			break;
		default:
			_(0);
	}
	copy_ether_address(eth_hdr->s_addr.addr_bytes,plocal_l3iface->if_mac);
	copy_ether_address(eth_hdr->d_addr.addr_bytes,preal_srv->rs_mac);

	mbuf->l2_len=64;
	mbuf->l3_len=(ip_hdr->version_ihl&0xf)<<2;
	mbuf->ol_flags|=PKT_TX_IP_CKSUM|PKT_TX_IPV4;
	
	vxlan_encapsulate_mbuf(mbuf,
		preal_srv,
		plocal_l3iface,
		pphy_l3iface,
		pe3_iface,
		0);
	nr_xmit_queue=mbuf->hash.fdir.lo%pe3_iface->nr_queues;

	lo_part=MAKE_UINT32(pe3_iface->port_id,nr_xmit_queue);
	fwd_id=MAKE_UINT64(EXTERNAL_INPUT_PROCESS_FWD_XMIT,lo_part);
	#undef _
	ret:
	return fwd_id;
}
__attribute__((always_inline))static inline int _external_input_node_post_process(
	struct node * pnode,
	struct rte_mbuf **mbufs,
	int nr_mbufs,
	uint64_t fwd_id)
{
	int idx=0;
	int drop_start=0;
	int nr_dropped=0;
	int nr_delivered;
	uint32_t low_part;
	uint16_t port_id;
	uint16_t queue_id;
	struct E3interface * pe3_iface;
	switch(HIGH_UINT64(fwd_id))
	{
		case EXTERNAL_INPUT_PROCESS_FWD_XMIT:
			low_part=LOW_UINT64(fwd_id);
			port_id=HIGH_UINT32(low_part);
			queue_id=LOW_UINT32(low_part);
			pe3_iface=find_e3iface_by_index(port_id);
			if(PREDICT_FALSE(!pe3_iface)){
				nr_dropped=nr_mbufs;
				break;
			}
			nr_delivered=deliver_mbufs_to_node(pe3_iface->output_node_arrar[queue_id],
				mbufs,
				nr_mbufs);
			drop_start=nr_delivered;
			nr_dropped=nr_mbufs-nr_delivered;
			break;
		default:
			nr_dropped=nr_mbufs;
			break;
	}
	for(idx=0;idx<nr_dropped;idx++)
		rte_pktmbuf_free(mbufs[drop_start+idx]);
	return 0;
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

	uint16_t last_cached_port=-1;
	uint32_t last_cached_packet_type=0;
	uint32_t last_cached_packet_hash=0;
	

	struct virtual_ip * plast_cached_pvip=NULL;
	struct lb_instance * plast_cached_plb=NULL;
	struct real_server * plast_cached_prs=NULL;
	DEF_EXPRESS_DELIVERY_VARS();
	RESET_EXPRESS_DELIVERY_VARS();
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,(void**)mbufs,64);
	if(!nr_mbufs)
		return 0;
	pre_setup_env(nr_mbufs);
	while((iptr=peek_next_mbuf())>=0){
		prefetch_next_mbuf(mbufs,iptr);
		fwd_id=translate_inbound_packet(mbufs[iptr],
			&last_cached_port,
			&last_cached_packet_type,
			&last_cached_packet_hash,
			&plast_cached_pvip,
			&plast_cached_plb,
			&plast_cached_prs);
		process_rc=proceed_mbuf(iptr,fwd_id);
		if(process_rc==MBUF_PROCESS_RESTART){
			fetch_pending_index(start_index,end_index);
			fetch_pending_fwd_id(last_fwd_id);
			_external_input_node_post_process(pnode,
				&mbufs[start_index],
				end_index-start_index+1,
				last_fwd_id);
			flush_pending_mbuf();
			proceed_mbuf(iptr,fwd_id);
		}
	}
	fetch_pending_index(start_index,end_index);
	fetch_pending_fwd_id(last_fwd_id);
	if(start_index>=0)
		_external_input_node_post_process(pnode,
		&mbufs[start_index],
		end_index-start_index+1,
		last_fwd_id);
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

