#include <device-wrapper.h>
#include <string.h>
#include <util.h>
#include <node.h>
#include <rte_ethdev.h>
#include <mbuf_delivery.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>

int type_vlink_capability_check(int port_id)
{
	return 0;
}
int type_default_capability_check(int port_id)
{
	return 0;
}

int type_lb_internal_capability_check(int port_id)
{
	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(port_id,&dev_info);
	#define _r(c) if(!(dev_info.rx_offload_capa&(c))) \
		goto error_check;
	#define _t(c) if(!(dev_info.tx_offload_capa&(c))) \
		goto error_check;
	_r(DEV_RX_OFFLOAD_VLAN_STRIP);
	_r(DEV_RX_OFFLOAD_IPV4_CKSUM);
	_r(DEV_RX_OFFLOAD_UDP_CKSUM);
	_r(DEV_RX_OFFLOAD_TCP_CKSUM);
	
	/*we need outter checksum is accomplished by hardware*/
	_t(DEV_TX_OFFLOAD_IPV4_CKSUM);
	_t(DEV_TX_OFFLOAD_UDP_CKSUM);
	_t(DEV_TX_OFFLOAD_TCP_CKSUM);
	_t(DEV_TX_OFFLOAD_VLAN_INSERT);
	//_t(DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM);
	#undef _r
	#undef _t 
	return 0;
	error_check:
		return -1;
	return 0;
}
int type_lb_external_capability_check(int port_id)
{
	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(port_id,&dev_info);
	#define _r(c) if(!(dev_info.rx_offload_capa&(c))) \
		goto error_check;
	#define _t(c) if(!(dev_info.tx_offload_capa&(c))) \
		goto error_check;
	_r(DEV_RX_OFFLOAD_VLAN_STRIP);
	_r(DEV_RX_OFFLOAD_IPV4_CKSUM);
	_r(DEV_RX_OFFLOAD_UDP_CKSUM);
	_r(DEV_RX_OFFLOAD_TCP_CKSUM);
	
	/*we need outter checksum is accomplished by hardware*/
	_t(DEV_TX_OFFLOAD_IPV4_CKSUM);
	_t(DEV_TX_OFFLOAD_UDP_CKSUM);
	_t(DEV_TX_OFFLOAD_TCP_CKSUM);
	_t(DEV_TX_OFFLOAD_VLAN_INSERT);
	#undef _r
	#undef _t 
	return 0;
	error_check:
		return -1;
	return 0;
}

/*naming regulation:we regard arp as L2 process,and icmp as L3 proces
even if we know naming is not as that good*/
#define LB_DEVICE_INPUT_FWD_DROP 0x0
#define LB_DEVICE_INPUT_FWD_L2_ARP_PROCESS 0x1
#define LB_DEVICE_INPUT_FWD_L3_PROCESS 0x2
#define LB_DEVICE_INPUT_FWD_L4_PROCESS 0x3
#define LB_DEVICE_INPUT_FWD_L3_ICMP_PROCESS 0x4
#define LB_DEVICE_INPUT_FWD_TUNNEL_PROCESS 0x5
#define LB_DEVICE_INPUT_FWD_EXTERNAL_INPUT_PROCESS 0x6

__attribute__((always_inline))static inline int _lb_device_input_node_post_process(
	struct node * pnode,
	struct rte_mbuf **mbufs,
	int nr_mbufs,
	uint64_t fwd_id
)
{
	int idx=0;
	int drop_start=0;
	int nr_dropped=0;
	int nr_delivered=0;
	switch(HIGH_UINT64(fwd_id))
	{
		case LB_DEVICE_INPUT_FWD_L2_ARP_PROCESS:
			nr_delivered=deliver_mbufs_by_next_entry(pnode,
				L2_NEXT_EDGE_L2_PROCESS,
				mbufs,
				nr_mbufs);
			drop_start=nr_delivered;
			nr_dropped=nr_mbufs-nr_delivered;
			break;
		case LB_DEVICE_INPUT_FWD_L3_ICMP_PROCESS:
			nr_delivered=deliver_mbufs_by_next_entry(pnode,
				L2_NEXT_EDGE_L3_PROCESS,
				mbufs,
				nr_mbufs);
			drop_start=nr_delivered;
			nr_dropped=nr_mbufs-nr_delivered;
			break;
		case LB_DEVICE_INPUT_FWD_TUNNEL_PROCESS:
			nr_delivered=deliver_mbufs_by_next_entry(pnode,
				L2_NEXT_EDGE_L4_TUNNEL_PROCESS,
				mbufs,
				nr_mbufs);
			drop_start=nr_delivered;
			nr_dropped=nr_mbufs-nr_delivered;
			break;
			
		case LB_DEVICE_INPUT_FWD_EXTERNAL_INPUT_PROCESS:
			nr_delivered=deliver_mbufs_by_next_entry(pnode,
				L2_NEXT_EDGE_EXTERNAL_INPUT,
				mbufs,
				nr_mbufs);
			drop_start=nr_delivered;
			nr_dropped=nr_mbufs-nr_delivered;
			break;

		default:
			nr_dropped=nr_mbufs;
			break;
	}
	pnode->nr_tx_packets+=nr_delivered;
	pnode->nr_drop_packets+=nr_dropped;
	for(idx=0;idx<nr_dropped;idx++)
		rte_pktmbuf_free(mbufs[drop_start+idx]);
	
	return 0;
}

__attribute__((always_inline)) static inline uint64_t nic_intel_xl710_classification(
	struct node * pnode,
	struct rte_mbuf* mbuf,
	struct E3interface * pif)
{
	/*intel xl710 can be both internal and external*/
	uint64_t fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_DROP,0);
	if(mbuf->l2_type==RTE_PTYPE_L2_ETHER_ARP)
		fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_L2_ARP_PROCESS,0);
	else if(((mbuf->l3_type==(RTE_PTYPE_L3_IPV4>>4)) ||
			(mbuf->l3_type==(RTE_PTYPE_L3_IPV4_EXT>>4)) ||
			(mbuf->l3_type==(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN>>4)))){/*ipv4 only*/
		if(mbuf->l4_type==(RTE_PTYPE_L4_ICMP>>8))/*ICMP traffic*/
			fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_L3_ICMP_PROCESS,0);
		else if(pif->port_type==PORT_TYPE_LB_INTERNAL){
			if(mbuf->tun_type)/*XL710 only recognize vxlan as RTE_PTYPE_TUNNEL_GRENAT sometimes,sometimes it does not */
				fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_TUNNEL_PROCESS,0);
			else if((mbuf->l4_type==(RTE_PTYPE_L4_UDP>>8))||
					(mbuf->l4_type==(RTE_PTYPE_L4_TCP>>8)))/*we also want TCP traffic in case it's vlan project network*/
				fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_TUNNEL_PROCESS,0);
		}else if(pif->port_type==PORT_TYPE_LB_EXTERNAL){
			if((mbuf->l4_type==(RTE_PTYPE_L4_UDP>>8))||(mbuf->l4_type==(RTE_PTYPE_L4_TCP>>8)))
				fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_EXTERNAL_INPUT_PROCESS,0);
		}
	}
	return fwd_id;
}

__attribute__((always_inline)) static inline uint64_t nic_intel_82599_classification(
	struct node * pnode,
	struct rte_mbuf* mbuf,
	struct E3interface * pif)
{
	/*when classify trtaffic on Intel 82599,it can only be external
	,it's essential to inspect the packet payload*/
	
	uint64_t fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_DROP,0);
	struct ether_hdr * eth_hdr=rte_pktmbuf_mtod(mbuf,struct ether_hdr *);
	struct ipv4_hdr * ip_hdr;
	
	switch(eth_hdr->ether_type)
	{
		case 0x0608:
			fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_L2_ARP_PROCESS,0);
			break;
		case 0x0008:
			ip_hdr=(struct ipv4_hdr *)(eth_hdr+1);
			if(ip_hdr->next_proto_id==0x1)
				fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_L3_ICMP_PROCESS,0);
			else if((ip_hdr->next_proto_id==0x11) || (ip_hdr->next_proto_id==6))
				fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_EXTERNAL_INPUT_PROCESS,0);
			break;
	}
	return fwd_id;
}
int lb_device_input_node_process_func(void *arg)
{
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int port_id;
	int queue_id;
	int process_rc;
	struct node * pnode=(struct node *)arg;
	struct E3interface * pif;
	uint64_t fwd_id=0;
	int iptr=0;
	int start_index,end_index;
	uint64_t last_fwd_id;
	/*classification cache entries*/
	uint64_t last_classification_fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_DROP,0);
	uint32_t last_classification_hash=0;
	uint32_t last_classification_packet_type=0;
	uint16_t last_classification_port=-1;
	
	DEF_EXPRESS_DELIVERY_VARS();
	RESET_EXPRESS_DELIVERY_VARS();
	
	port_id=(int)HIGH_UINT64((uint64_t)pnode->node_priv);
	queue_id=(int)LOW_UINT64((uint64_t)pnode->node_priv);
	pif=find_e3iface_by_index(port_id);
	if(PREDICT_FALSE((!pif->if_avail_ptr)||(pif->port_status==PORT_STATUS_DOWN)))
		return 0;
	
	nr_mbufs=rte_eth_rx_burst(port_id,queue_id,mbufs,E3_MIN(64,pnode->burst_size));
	if(!nr_mbufs)
		return 0;
	pnode->nr_rx_packets+=nr_mbufs;
	pre_setup_env(nr_mbufs);
	/*check ptypes and deliver them directly,do not touch any packet payload*/
	while((iptr=peek_next_mbuf())>=0){
		fwd_id=MAKE_UINT64(LB_DEVICE_INPUT_FWD_DROP,0);
		/*hardware dependent classification*/
		
		if(PREDICT_TRUE(is_e3interface_available(pif))){
			/*TODO:cache recent packets with their packet-type,hash to identify subsequent packets*/
			switch(pif->hardware_nic_type)
			{
					#if 0
					printf("type:%x of:%p vlan:%d %4x-%4x l2:%d\n",mbufs[iptr]->packet_type,
						(void*)mbufs[iptr]->ol_flags,
						mbufs[iptr]->vlan_tci,
						mbufs[iptr]->hash.fdir.hash,
						mbufs[iptr]->hash.fdir.id,
						mbufs[iptr]->l2_len);
					#endif
				case NIC_INTEL_XL710:
					if((last_classification_hash==mbufs[iptr]->hash.fdir.lo)&&
						(last_classification_packet_type==mbufs[iptr]->packet_type)&&
						(last_classification_port==mbufs[iptr]->port))
						fwd_id=last_classification_fwd_id;
					else{
						fwd_id=nic_intel_xl710_classification(pnode,mbufs[iptr],pif);
						last_classification_hash=mbufs[iptr]->hash.fdir.lo;
						last_classification_packet_type=mbufs[iptr]->packet_type;
						last_classification_fwd_id=fwd_id;
						last_classification_port=mbufs[iptr]->port;
					}
					break;
				case NIC_INTEL_82599:
					if((last_classification_hash==mbufs[iptr]->hash.fdir.lo)&&
						(last_classification_packet_type==mbufs[iptr]->packet_type)&&
						(last_classification_port==mbufs[iptr]->port))
						fwd_id=last_classification_fwd_id;
					else{
						fwd_id=nic_intel_82599_classification(pnode,mbufs[iptr],pif);
						last_classification_hash=mbufs[iptr]->hash.fdir.lo;
						last_classification_packet_type=mbufs[iptr]->packet_type;
						last_classification_fwd_id=fwd_id;
						last_classification_port=mbufs[iptr]->port;
						prefetch_next_mbuf(mbufs,iptr);
					}
					break;
				default:
					break;
			}
		}		
		process_rc=proceed_mbuf(iptr,fwd_id);
		if(process_rc==MBUF_PROCESS_RESTART){
			fetch_pending_index(start_index,end_index);
			fetch_pending_fwd_id(last_fwd_id);
			_lb_device_input_node_post_process(pnode,&mbufs[start_index],end_index-start_index+1,last_fwd_id);
			flush_pending_mbuf();
			proceed_mbuf(iptr,fwd_id);
		}
	}
	fetch_pending_index(start_index,end_index);
	fetch_pending_fwd_id(last_fwd_id);
	if(start_index>=0)
		_lb_device_input_node_post_process(pnode,&mbufs[start_index],end_index-start_index+1,last_fwd_id);
	return 0;
}
int lb_vlink_device_input_node_process_func(void *arg)
{

	return 0;	
}
int default_device_output_node_process_func(void* arg)
{
	struct node * pnode=(struct node*)arg;
	struct E3interface * pif;
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int nr_delivered=0;
	int idx=0;
	
	int port_id=HIGH_UINT64((uint64_t)pnode->node_priv);
	int queue_id=LOW_UINT64((uint64_t)pnode->node_priv);
	pif=find_e3iface_by_index(port_id);
	if(PREDICT_FALSE((!pif->if_avail_ptr)||(pif->port_status==PORT_STATUS_DOWN)))
		return 0;
	
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,
		(void**)mbufs,
		E3_MIN(pnode->burst_size,64));
	if(!nr_mbufs)
		return 0;
	nr_delivered=rte_eth_tx_burst(port_id,queue_id,mbufs,nr_mbufs);
	for(idx=nr_delivered;idx<nr_mbufs;idx++)
		rte_pktmbuf_free(mbufs[idx]);
	pnode->nr_rx_packets+=nr_mbufs;
	pnode->nr_tx_packets+=nr_delivered;
	pnode->nr_drop_packets+=nr_mbufs-nr_delivered;
	return 0;
	
}
int add_e3_interface(const char *params,uint8_t nic_type,uint8_t if_type,int *pport_id)
{
	int rc;
	int _pport_id=0;
	uint8_t nic_speed=NIC_GE;
	struct mq_device_ops ops;
	struct E3interface * pif;
	memset(&ops,0x0,sizeof(struct mq_device_ops));
	switch(nic_type)
	{
		case NIC_VIRTUAL_DEV:
			nic_speed=NIC_GE;
			ops.hash_function=ETH_RSS_IP;
			break;
		case NIC_INTEL_XL710:
			nic_speed=NIC_40GE;
			ops.hash_function=ETH_RSS_PROTO_MASK;
			break;
		case NIC_INTEL_82599:
			nic_speed=NIC_10GE;
			ops.hash_function=ETH_RSS_IP|ETH_RSS_TCP|ETH_RSS_UDP;
			break;
		default:
			E3_ERROR("unsupported NIC types\n");
			return -1;
			break;
	}
	ops.mq_device_port_type=if_type;
	//ops.hash_function=ETH_RSS_PROTO_MASK;/*ETH_RSS_PROTO_MASK:all fields will be checked,I40E need it*/
	ops.predefined_edges=0;
	
	switch(ops.mq_device_port_type)
	{
		case PORT_TYPE_LB_EXTERNAL:
			ops.capability_check=type_lb_external_capability_check;
			ops.input_node_process_func=lb_device_input_node_process_func;
			break;
		case PORT_TYPE_LB_INTERNAL:
			ops.capability_check=type_lb_internal_capability_check;
			ops.input_node_process_func=lb_device_input_node_process_func;
			break;
		case PORT_TYPE_VLINK:
			ops.capability_check=type_vlink_capability_check;
			ops.input_node_process_func=lb_vlink_device_input_node_process_func;
			break;
		default:
			ops.capability_check=type_default_capability_check;
			ops.input_node_process_func=lb_vlink_device_input_node_process_func;
			break;
	}
	ops.output_node_process_func=default_device_output_node_process_func;
	
	switch(nic_speed)
	{
		case NIC_40GE:
			ops.nr_queues_to_poll=QUEUES_OF_40GE_NIC;
			break;
		case NIC_10GE:
			ops.nr_queues_to_poll=QUEUES_OF_10GE_NIC;
			break;
		case NIC_GE:
			ops.nr_queues_to_poll=QUEUES_OF_GE_NIC;
			break;
		default:
			ops.nr_queues_to_poll=1;
			E3_WARN("unsupported nic speed\n");
			break;
	}
	if(ops.mq_device_port_type==PORT_TYPE_LB_INTERNAL 
		|| ops.mq_device_port_type==PORT_TYPE_LB_EXTERNAL){
		
		ops.edges[0].fwd_behavior=NODE_TO_NODE_FWD;
		ops.edges[0].edge_entry=L2_NEXT_EDGE_L2_PROCESS;
		ops.edges[0].next_ref="l2-underlay-node";

		ops.edges[1].fwd_behavior=NODE_TO_NODE_FWD;
		ops.edges[1].edge_entry=L2_NEXT_EDGE_L3_PROCESS;
		ops.edges[1].next_ref="l3-underlay-node";

		ops.edges[2].fwd_behavior=NODE_TO_CLASS_FWD;
		ops.edges[2].edge_entry=L2_NEXT_EDGE_L4_TUNNEL_PROCESS;
		ops.edges[2].next_ref="l4-tunnel-class";

		ops.edges[3].fwd_behavior=NODE_TO_CLASS_FWD;
		ops.edges[3].edge_entry=L2_NEXT_EDGE_EXTERNAL_INPUT;
		ops.edges[3].next_ref="external-input";
		
		ops.predefined_edges=4;
	}
	rc=register_native_mq_dpdk_port(params,
		&ops,
		&_pport_id);
	if(!rc){
		if(pport_id)
			*pport_id=_pport_id;
		pif=find_e3iface_by_index(_pport_id);
		pif->hardware_nic_type=nic_type;
		change_e3_interface_mtu(_pport_id,1600);
		int exec_rc=e3interface_turn_vlan_strip_on(_pport_id);
		E3_WARN("vlan stripping on %d %s\n",_pport_id,!exec_rc?"succeeds":"fails");
	}
	
	return rc;
}

void dump_e3iface_node_stats(int port_id)
{
	uint64_t tsc_now;
	uint64_t tsc_diff;
	struct node * pnode;
	int idx=0;
	struct E3interface * pe3iface=find_e3iface_by_index(port_id);
	if(!pe3iface){
		printf("port %d does not exist\n",port_id);
		return;
	}
	tsc_now=rte_rdtsc();
	tsc_diff=tsc_now-pe3iface->last_updated_ts;
	rte_eth_stats_get(pe3iface->port_id,&pe3iface->stats);
	rte_eth_stats_reset(pe3iface->port_id);
	pe3iface->last_updated_ts=rte_rdtsc();
	
	printf("there are %d queues in E3interface:%d\n",pe3iface->nr_queues,port_id);
	printf("rx-pps:%"PRIu64"\n",pe3iface->stats.ipackets*rte_get_tsc_hz()/tsc_diff);
	for(idx=0;idx<pe3iface->nr_queues;idx++){
		printf("queue %d:\n",idx);
		E3_ASSERT(pnode=find_node_by_index(pe3iface->input_node_arrar[idx]));
		printf("\tinput(%d) rx:%"PRIu64" tx:%"PRIu64" drop:%"PRIu64"\n",
			pnode->node_index,
			pnode->nr_rx_packets,
			pnode->nr_tx_packets,
			pnode->nr_drop_packets);
		E3_ASSERT(pnode=find_node_by_index(pe3iface->output_node_arrar[idx]));
		printf("\toutput(%d) rx:%"PRIu64" tx:%"PRIu64" drop:%"PRIu64"\n",
			pnode->node_index,
			pnode->nr_rx_packets,
			pnode->nr_tx_packets,
			pnode->nr_drop_packets);
	}
	
}
