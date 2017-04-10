#include <device-wrapper.h>
#include <string.h>
#include <util.h>
#include <node.h>
#include <rte_ethdev.h>
#include <mbuf_delivery.h>

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
	_r(DEV_RX_OFFLOAD_IPV4_CKSUM);
	_r(DEV_RX_OFFLOAD_UDP_CKSUM);
	_r(DEV_RX_OFFLOAD_TCP_CKSUM);
	
	/*we need outter checksum is accomplished by hardware*/
	_t(DEV_TX_OFFLOAD_IPV4_CKSUM);
	_t(DEV_TX_OFFLOAD_UDP_CKSUM);
	_t(DEV_TX_OFFLOAD_TCP_CKSUM);
	_t(DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM);
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



int lb_device_input_node_process_func(void *arg)
{
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int port_id;
	int queue_id;
	int idx=0;
	struct node * pnode=(struct node *)arg;
	//DEF_EXPRESS_DELIVERY_VARS();
	//RESET_EXPRESS_DELIVERY_VARS();
	
	port_id=(int)HIGH_UINT64((uint64_t)pnode->node_priv);
	queue_id=(int)LOW_UINT64((uint64_t)pnode->node_priv);
	nr_mbufs=rte_eth_rx_burst(port_id,queue_id,mbufs,E3_MIN(64,pnode->burst_size));
	if(!nr_mbufs)
		return 0;
	
	for(idx=0;idx<nr_mbufs;idx++){
		printf("%d.%d %x(%x-%x)%p\n",port_id,queue_id,
			mbufs[idx]->packet_type,
			mbufs[idx]->hash.fdir.hash,
			mbufs[idx]->hash.fdir.id,
			(void*)mbufs[idx]->ol_flags);
	}
	return 0;
}
int lb_vlink_device_input_node_process_func(void *arg)
{

	return 0;	
}
int default_device_output_node_process_func(void* arg)
{
	return 0;
	
}
int add_e3_interface(const char *params,uint8_t nic_type,uint8_t if_type,int *pport_id)
{
	int rc;
	uint8_t nic_speed=NIC_GE;
	struct mq_device_ops ops;
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
			ops.hash_function=ETH_RSS_PROTO_MASK;
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
		/*to-do:setup edges*/
	}
	rc=register_native_mq_dpdk_port(params,
		&ops,
		pport_id);
	
	return rc;
}
