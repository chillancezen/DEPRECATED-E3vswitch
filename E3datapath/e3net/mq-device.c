#include <mq-device.h>
#include <util.h>
#include <rte_malloc.h>
#include <node.h>
#include <lcore_extension.h>
#include <mbuf_delivery.h>


static void input_and_output_reclaim_func(struct rcu_head * rcu)
{
	struct node *pnode=container_of(rcu,struct node,rcu);
	rte_free(pnode);
}

int register_native_mq_dpdk_port(const char * params,
	struct mq_device_ops * dev_ops)
{
	int rc,idx,iptr;
	uint8_t port_id;
	struct E3interface *pif;
	char dev_data_name[64];
	char* lptr=NULL;
	struct node * pinput_node_array[MAX_NR_QUEUES_TO_POLL];
	struct node * poutput_node_array[MAX_NR_QUEUES_TO_POLL];
	int input_lcore_id,output_lcore_id;
	int node_socket_id;
	struct rte_eth_conf mq_port_conf;
	struct rte_mempool * mempool=NULL;
	rc=rte_eth_dev_attach(params,&port_id);
	if(rc){
		E3_ERROR("creating mq-device:%s fails\n",params);
		return -1;
	}
	
	pif=&ginterface_array[port_id];
	memset(pinput_node_array,0x0,sizeof(pinput_node_array));
	memset(poutput_node_array,0x0,sizeof(poutput_node_array));
	memset(dev_data_name,0x0,sizeof(dev_data_name));
	memset(pif,0x0,sizeof(struct E3interface));
	pif->port_id=port_id;
	pif->port_type=dev_ops->mq_device_port_type;
	/*determine the number of queues to poll*/
	rte_eth_dev_info_get(port_id,&pif->dev_info);
	pif->nr_queues=E3_MIN(pif->dev_info.max_rx_queues,pif->dev_info.max_tx_queues);
	pif->nr_queues=E3_MIN(pif->nr_queues,dev_ops->nr_queues_to_poll);
	pif->nr_queues=E3_MIN(pif->nr_queues,MAX_NR_QUEUES_TO_POLL);
	if(!pif->nr_queues){
		E3_ERROR("invalid nr_queues_to_poll\n");
		goto error_dev_detach;
	}
	/*device capability check*/
	rc=dev_ops->capability_check(port_id);
	if(rc){
		E3_ERROR("device capability check fails:%s\n",params);
		goto error_dev_detach;
	}
	
	/*allocate input and output nodes*/
	idx=0;
	for(;idx<pif->nr_queues;idx++){
		pinput_node_array[idx]=rte_zmalloc(NULL,sizeof(struct node),64);
		poutput_node_array[idx]=rte_zmalloc(NULL,sizeof(struct node),64);
		if(!pinput_node_array[idx]||!poutput_node_array[idx])
			break;
	}
	if(idx<pif->nr_queues){
		E3_ERROR("no enough memory during node allocation phase\n");
		goto error_node_allocation;
	}

	for(idx=0;idx<pif->nr_queues;idx++){
		sprintf((char*)pinput_node_array[idx]->name,"dev-input-node-%d.%d",port_id,idx);
		sprintf((char*)poutput_node_array[idx]->name,"dev-output-node-%d.%d",port_id,idx);

		pinput_node_array[idx]->node_process_func=dev_ops->input_node_process_func;
		poutput_node_array[idx]->node_process_func=dev_ops->output_node_process_func;

		pinput_node_array[idx]->node_type=node_type_input;
		poutput_node_array[idx]->node_type=node_type_output;

		pinput_node_array[idx]->burst_size=NODE_BURST_SIZE;
		poutput_node_array[idx]->burst_size=NODE_BURST_SIZE;

		/*port_id and queue_index comprise the proivate field*/
		pinput_node_array[idx]->node_priv=(void*)MAKE_UINT64(port_id,idx);
		poutput_node_array[idx]->node_priv=(void*)MAKE_UINT64(port_id,idx);

		pinput_node_array[idx]->node_reclaim_func=NULL;
		poutput_node_array[idx]->node_reclaim_func=NULL;
	}

	/*determine nodes' lcore id*/
	idx=0;
	for(;idx<pif->nr_queues;idx++){
		pinput_node_array[idx]->lcore_id=0xff;
		poutput_node_array[idx]->lcore_id=0xff;
		input_lcore_id=get_io_lcore();
		if(!validate_lcore_id(input_lcore_id))
			break;
		node_socket_id=lcore_to_socket_id(input_lcore_id);
		output_lcore_id=get_io_lcore_by_socket_id(node_socket_id);
		if(!validate_lcore_id(output_lcore_id)){
			put_lcore(input_lcore_id,1);
			break;
		}
		pinput_node_array[idx]->lcore_id=input_lcore_id;
		poutput_node_array[idx]->lcore_id=output_lcore_id;
	}
	if(idx<pif->nr_queues){
		E3_ERROR("can not allocate lcore during dev ndoe setup\n");
		goto error_lcore_allocation;
	}
	/*register these nodes*/
	idx=0;
	for(;idx<pif->nr_queues;idx++){
		if(register_node(pinput_node_array[idx]))
			break;
		if(register_node(poutput_node_array[idx]))
			break;
	}
	if(idx<pif->nr_queues)
		goto error_node_registration;
	for(idx=0;idx<pif->nr_queues;idx++){
		pif->input_node_arrar[idx]=pinput_node_array[idx]->node_index;
		pif->output_node_arrar[idx]=poutput_node_array[idx]->node_index;
	}
	
	/*start configuring port*/
	memset(&mq_port_conf,0x0,sizeof(struct rte_eth_conf));
	mq_port_conf.rxmode.mq_mode=ETH_MQ_RX_RSS;
	mq_port_conf.rxmode.header_split=0;
	mq_port_conf.rxmode.hw_ip_checksum=1;
	mq_port_conf.rxmode.hw_vlan_filter=0;
	mq_port_conf.rxmode.hw_vlan_strip=0;
	mq_port_conf.rxmode.hw_vlan_extend=0;
	mq_port_conf.rxmode.jumbo_frame=0;
	mq_port_conf.rxmode.hw_strip_crc=0;
	mq_port_conf.rxmode.enable_scatter=0;
	mq_port_conf.rxmode.enable_lro=0;
	mq_port_conf.txmode.mq_mode=ETH_MQ_TX_NONE;
	mq_port_conf.rx_adv_conf.rss_conf.rss_key=NULL;
	mq_port_conf.rx_adv_conf.rss_conf.rss_hf=dev_ops->hash_function;
	mq_port_conf.fdir_conf.mode=RTE_FDIR_MODE_PERFECT;/*enable ptype classification*/
	
	rc=rte_eth_dev_configure(port_id,pif->nr_queues,pif->nr_queues,&mq_port_conf);
	if(rc<0){
		E3_ERROR("errors occurs during device configuration phase:%s\n",params);
		goto error_node_registration;
	}
	rte_eth_promiscuous_enable(port_id);

	/*setup rx-queue and tx queue*/
	rte_eth_dev_info_get(port_id,&pif->dev_info);
	pif->dev_info.default_txconf.txq_flags=0;
	idx=0;
	for(;idx<pif->nr_queues;idx++){
		mempool=get_mempool_by_socket_id(lcore_to_socket_id(pinput_node_array[idx]->lcore_id));
		if(!mempool)
			break;
		rc=rte_eth_rx_queue_setup(port_id,
			idx,
			DEFAULT_RX_DESCRIPTORS,
			lcore_to_socket_id(pinput_node_array[idx]->lcore_id),
			&pif->dev_info.default_rxconf,
			mempool);
		if(rc<0)
			break;
		rc=rte_eth_tx_queue_setup(port_id,
			idx,
			DEFAULT_TX_DESCRIPTORS,
			lcore_to_socket_id(poutput_node_array[idx]->lcore_id),
			&pif->dev_info.default_txconf);
		if(rc<0)
			break;
	}
	if(idx<pif->nr_queues){
		E3_ERROR("errors occur during rx/tx queues setup phase:%s\n",params);
		goto error_node_registration;
	}
	/*start device*/
	rte_eth_macaddr_get(port_id,&pif->mac_addr);
	rc=rte_eth_dev_start(port_id);
	if(rc<0){
		E3_ERROR("errors occur during dev startup setup phase:%s\n",params);
		goto error_node_registration;
	}
	rte_eth_link_get(port_id,&pif->link_info);
	{
		rte_eth_dev_get_name_by_port(port_id,dev_data_name);
		lptr=dev_data_name;
		if(*lptr<='z'&&*lptr>='a')
			*lptr+='A'-'a';
		while(*lptr){
			if(*lptr==':'||*lptr=='.')
				*lptr='/';
			lptr++;
		}
		sprintf((char*)pif->ifname,"%s%s",link_speed_to_string(pif->link_info.link_speed),dev_data_name);
	}

	/*attach node to lcores right now*/
	for(idx=0;idx<pif->nr_queues;idx++){
		rc=attach_node_to_lcore(pinput_node_array[idx]);
		if(rc)
			break;
		rc=attach_node_to_lcore(poutput_node_array[idx]);
		if(rc)
			break;
	}
	if(idx<pif->nr_queues){
		E3_ERROR("errors occur during lcore attachment phase:%s\n",params);
		goto error_node_attch_lcore;
	}
	/*next edge initialization*/
	iptr=0;
	for(iptr=0;iptr<dev_ops->predefined_edges;iptr++){
		switch(dev_ops->edges[iptr].fwd_behavior)
		{
			case NODE_TO_NODE_FWD:
				for(idx=0;idx<pif->nr_queues;idx++){
					rc=set_node_to_node_edge((char*)pinput_node_array[idx]->name,
						dev_ops->edges[iptr].edge_entry,
						dev_ops->edges[iptr].next_ref);
					if(rc)
						goto error_edge_setup;
				}
				break;
			case NODE_TO_CLASS_FWD:
				for(idx=0;idx<pif->nr_queues;idx++){
					rc=set_node_to_class_edge((char*)pinput_node_array[idx]->name,
						dev_ops->edges[iptr].edge_entry,
						dev_ops->edges[iptr].next_ref);
					if(rc)
						goto error_edge_setup;
				}
				break;
			default:
				break;
		}
		continue;
		error_edge_setup:
			break;
	}
	if(iptr<dev_ops->predefined_edges){
		E3_ERROR("errors occurs during edge setup phase\n");
		goto error_node_attch_lcore;
	}
	
	/*misc options setup*/
	pif->port_status=PORT_STATUS_DOWN;
	rcu_assign_pointer(pif->if_avail_ptr,!(NULL));
	for(idx=0;idx<pif->nr_queues;idx++){
		pinput_node_array[idx]->node_reclaim_func=input_and_output_reclaim_func;
		poutput_node_array[idx]->node_reclaim_func=input_and_output_reclaim_func;
	}
	
	E3_LOG("register interface:%s with\n",(char*)pif->ifname);
	for(idx=0;idx<pif->nr_queues;idx++){
		E3_LOG("\tnode %s on %d and node %s on %d\n",
			(char*)pinput_node_array[idx]->name,
			(int)pinput_node_array[idx]->lcore_id,
			(char*)poutput_node_array[idx]->name,
			(int)poutput_node_array[idx]->lcore_id);
	}
	return 0;
	error_node_attch_lcore:
		for(idx=0;idx<pif->nr_queues;idx++){
			detach_node_from_lcore(pinput_node_array[idx]);
			detach_node_from_lcore(poutput_node_array[idx]);
		}
	error_node_registration:
		for(idx=0;idx<pif->nr_queues;idx++){
			unregister_node(pinput_node_array[idx]);
			unregister_node(poutput_node_array[idx]);
		}
	error_lcore_allocation:
		for(idx=0;idx<pif->nr_queues;idx++){
			if(!validate_lcore_id(pinput_node_array[idx]->lcore_id))
				break;
			else{
				put_lcore(pinput_node_array[idx]->lcore_id,1);
				put_lcore(poutput_node_array[idx]->lcore_id,1);
			}
		}
	error_node_allocation:
		for(idx=0;idx<pif->nr_queues;idx++){
			if(pinput_node_array[idx])
				rte_free(pinput_node_array[idx]);
			if(poutput_node_array[idx])
				rte_free(poutput_node_array[idx]);
		}
	error_dev_detach:
			{
				int release_rc;
				char dev_name[128];
				rte_eth_dev_stop(port_id);
				rte_eth_dev_close(port_id);
				release_rc=rte_eth_dev_detach(port_id,dev_name);
				if(release_rc)
					E3_LOG("error occurs in releasing %s due to error in registering mq-dpdk vport\n",dev_name);
			}
		return -2;
		
}

void mq_interface_release_rcu_callback(struct rcu_head * rcu)
{
	
	char dev_name[128];
	struct E3interface *pif=container_of(rcu,struct E3interface,rcu);
	struct node * pinput_node_array[MAX_NR_QUEUES_TO_POLL];
	struct node * poutput_node_array[MAX_NR_QUEUES_TO_POLL];
	int idx,rc;
	for(idx=0;idx<pif->nr_queues;idx++){
		pinput_node_array[idx]=find_node_by_index(pif->input_node_arrar[idx]);
		poutput_node_array[idx]=find_node_by_index(pif->output_node_arrar[idx]);
		E3_ASSERT(pinput_node_array[idx]);
		E3_ASSERT(poutput_node_array[idx]);
	}
	/*clean next edges of input node and
	free mbufs in ring buffer of output node*/
	
	for(idx=0;idx<pif->nr_queues;idx++){
		clean_node_next_edges((char*)pinput_node_array[idx]->name);
		clear_node_ring_buffer(poutput_node_array[idx]);
	}
	/*unregister these nodes*/
	for(idx=0;idx<pif->nr_queues;idx++){
		unregister_node(pinput_node_array[idx]);
		unregister_node(poutput_node_array[idx]);
	}
	/*stop and detach device*/
	rte_eth_dev_stop(pif->port_id);
	rte_eth_dev_close(pif->port_id);
	rc=rte_eth_dev_detach(pif->port_id,dev_name);
	if(rc)
		E3_WARN("detaching port id:%d fails\n",pif->port_id);
	E3_LOG("successfully delete interface:%s\n",pif->ifname);

	memset(pif,0x0,sizeof(struct E3interface));
	rcu_assign_pointer(pif->if_avail_ptr,NULL);
}

void unregister_native_mq_dpdk_port(int port_id)
{
	struct node * pinput_node_array[MAX_NR_QUEUES_TO_POLL];
	struct node * poutput_node_array[MAX_NR_QUEUES_TO_POLL];
	struct E3interface *pif;
	int idx,rc;
	if(port_id<0&&port_id>RTE_MAX_ETHPORTS){
		E3_WARN("invalid port id to be released:%d\n",port_id);
		return ;
	}
	pif=&ginterface_array[port_id];
	if(!pif->if_avail_ptr){
		E3_WARN("port: %d not available currently\n",port_id);
		return ;
	}
	
	for(idx=0;idx<pif->nr_queues;idx++){
		pinput_node_array[idx]=find_node_by_index(pif->input_node_arrar[idx]);
		poutput_node_array[idx]=find_node_by_index(pif->output_node_arrar[idx]);
		E3_ASSERT(pinput_node_array[idx]);
		E3_ASSERT(poutput_node_array[idx]);
	}
	/*detach nodes from lcore list first*/
	for(idx=0;idx<pif->nr_queues;idx++){
		rc=detach_node_from_lcore(pinput_node_array[idx]);
		if(rc)
			{E3_WARN("errors occur during detaching node %s from lcore list\n",(char*)pinput_node_array[idx]->name);}
		else 
			put_lcore(pinput_node_array[idx]->lcore_id,1);

		rc=detach_node_from_lcore(poutput_node_array[idx]);
		if(rc)
			{E3_WARN("errors occur during detaching node %s from lcore list\n",(char*)poutput_node_array[idx]->name);}
		else 
			put_lcore(poutput_node_array[idx]->lcore_id,1);
	}

	if(pif->under_releasing)
		return ;

	pif->under_releasing=1;
	call_rcu(&pif->rcu,mq_interface_release_rcu_callback);
}
int checksum_cap_check(int port_id)
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

	_t(DEV_TX_OFFLOAD_IPV4_CKSUM);
	_t(DEV_TX_OFFLOAD_UDP_CKSUM);
	_t(DEV_TX_OFFLOAD_TCP_CKSUM);
	#undef _r
	#undef _t 
	return 0;

	error_check:
		return -1;
}
int dummy_cap_check(int port_id)
{
	return 0;
}
static int demo_input_node_process_func(void *arg)
{
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int port_id;
	int queue_id;
	int idx=0;
	struct node * pnode=(struct node *)arg;
	port_id=(int)HIGH_UINT64((uint64_t)pnode->node_priv);
	queue_id=(int)LOW_UINT64((uint64_t)pnode->node_priv);
	nr_mbufs=rte_eth_rx_burst(port_id,queue_id,mbufs,64);
	if(nr_mbufs==0)
		return 0;
	for(idx=0;idx<nr_mbufs;idx++){
		printf("%x\n",mbufs[idx]->packet_type);
	}
	return 0;
}
static int demo_output_node_process_func(void *arg)
{
	
	return 0;
}

void mq_device_module_test(void)
{
	struct mq_device_ops dev_ops={
		.mq_device_port_type=PORT_TYPE_LB_EXTERNAL,
		.nr_queues_to_poll=4,
		.hash_function=ETH_MQ_RX_RSS,
		.capability_check=dummy_cap_check,
		.input_node_process_func=demo_input_node_process_func,
		.output_node_process_func=demo_output_node_process_func,
		.predefined_edges=1,
		.edges={
			{.fwd_behavior=NODE_TO_CLASS_FWD,
			 .edge_entry=DEVICE_NEXT_ENTRY_TO_L2_INPUT,
			 .next_ref="l2-input-class" 
			},
		},
	};
	
	register_native_mq_dpdk_port("0000:01:00.1",&dev_ops);
	#if 0
	//register_native_mq_dpdk_port("eth_af_packet0,iface=enp0s3",&dev_ops);
	register_native_mq_dpdk_port("0000:03:00.0",&dev_ops);
	//register_native_dpdk_port("eth_af_packet0,iface=enp0s3",1);

	getchar();
	unregister_native_mq_dpdk_port(find_port_id_by_ifname("10GEthernetTap0"));
	getchar();
	register_native_mq_dpdk_port("eth_tap0,iface=tap0",&dev_ops);
	#endif
}

