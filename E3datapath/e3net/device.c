#include <device.h>
#include <stdio.h>
#include <string.h>
#include <rte_malloc.h>
#include <node.h>
#include <node_adjacency.h>
#include <lcore_extension.h>
#include <mbuf_delivery.h>

struct E3interface ginterface_array[RTE_MAX_ETHPORTS];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};



int input_node_process_func(void *arg)
{
	struct node *pnode=(struct node*)arg;
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int nr_delivered;
	int idx=0;
	struct E3interface * pif=(struct E3interface*)pnode->node_priv;
	nr_mbufs=rte_eth_rx_burst(pif->port_id,0,mbufs,E3_MIN(pnode->burst_size,64));
	if(!nr_mbufs)
		return 0;
	/*loopback all the packets*/
	#if 1
	for(idx=0;idx<nr_mbufs;idx++){
		char tmp;
		char * data=
			rte_pktmbuf_mtod(mbufs[idx],char *);
		tmp=data[0];data[0]=data[0+6];data[0+6]=tmp;
		tmp=data[1];data[1]=data[1+6];data[1+6]=tmp;
		tmp=data[2];data[2]=data[2+6];data[2+6]=tmp;
		tmp=data[3];data[3]=data[3+6];data[3+6]=tmp;
		tmp=data[4];data[4]=data[4+6];data[4+6]=tmp;
		tmp=data[5];data[5]=data[5+6];data[5+6]=tmp;
		
	}
	#endif
	nr_delivered=deliver_mbufs_by_next_entry(pnode,DEVICE_NEXT_ENTRY_TO_L2_INPUT,mbufs,nr_mbufs);
	
	//nr_delivered=deliver_mbufs_between_nodes(pif->output_node,pif->input_node,mbufs,nr_mbufs);
	for(idx=nr_delivered;idx<nr_mbufs;idx++)
		rte_pktmbuf_free(mbufs[idx]);
	#if 1
	if(nr_delivered!=nr_mbufs)
		printf("%s remain %d unsent\n",pif->ifname,nr_mbufs-nr_delivered);
	#endif
	
	return 0;
}
int output_node_process_func(void *arg)
{
	struct node *pnode=(struct node*)arg;
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int nr_xmited;
	int idx=0;
	struct E3interface * pif=(struct E3interface*)pnode->node_priv;
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,(void**)mbufs,E3_MIN(pnode->burst_size,64));
	if(!nr_mbufs)
		return 0;
	nr_xmited=rte_eth_tx_burst(pif->port_id,0,mbufs,nr_mbufs);
	for(idx=nr_xmited;idx<nr_mbufs;idx++){
		rte_pktmbuf_free(mbufs[idx]);
	}
	#if 0
	if(nr_xmited!=nr_mbufs)
		printf("%s remain %d unsent\n",pif->ifname,nr_mbufs-nr_xmited);
	#endif
	return 0;
}

void input_and_output_reclaim_func(struct rcu_head * rcu)
{/*this should be a dynamically allocated node*/
	struct node *pnode=container_of(rcu,struct node,rcu);
	
	rte_free(pnode);
}
char * link_speed_to_string(uint32_t speed)
{
	char *ret="Ethernet";
	switch(speed)
	{
	#define _(s) \
		case ETH_SPEED_NUM_##s: \
			ret=#s"Ethernet"; \
			break;
		_(10M);
		_(100M);
		_(1G);
		_(2_5G);
		_(5G);
		_(10G);
		_(20G);
		_(25G);
		_(40G);
		_(50G);
		_(56G);
		_(100G);
	#undef _	
	}
	
	return ret;
}
int register_native_dpdk_port(const char * params,int use_dev_numa)
{
	int node_socket_id=0;
	struct node * pinput_node=NULL;
	struct node * poutput_node=NULL;
	struct E3interface *pif;
	struct rte_mempool * mempool=NULL;
	int rc;
	uint8_t  port_id;
	uint8_t  dev_lcore_id=0;
	char  dev_data_name[64];
	char* lptr=NULL;
	memset(dev_data_name,0x0,sizeof(dev_data_name));
	rc=rte_eth_dev_attach(params,&port_id);
	if(rc){
		E3_ERROR("creating device:%s fails\n",params);
		return rc;
	}
	
	pif=&ginterface_array[port_id];
	pif->port_id=port_id;
	rte_eth_dev_info_get(port_id,&pif->dev_info);
	rte_eth_link_get_nowait(port_id,&pif->link_info);
	rte_eth_macaddr_get(port_id,&pif->mac_addr);
	

	/*setup input nodes for this Ethernet device*/
	pinput_node=rte_zmalloc(NULL,sizeof(struct node),64);
	poutput_node=rte_zmalloc(NULL,sizeof(struct node),64);
	if(!pinput_node || !poutput_node){
		E3_ERROR("error occurs during allocation of nodes\n")
		goto error_dev_detach;
	}

	sprintf((char*)pinput_node->name,"device-input-node-%d",port_id);
	sprintf((char*)poutput_node->name,"device-output-node-%d",port_id);

	pinput_node->node_process_func=input_node_process_func;
	poutput_node->node_process_func=output_node_process_func;

	pinput_node->node_type=node_type_input;
	poutput_node->node_type=node_type_output;

	pinput_node->burst_size=NODE_BURST_SIZE;
	poutput_node->burst_size=NODE_BURST_SIZE;

	pinput_node->node_priv=pif;
	poutput_node->node_priv=pif;

	/*do not set them,let's manully reclaim them if we meet any errors during setup period*/
    pinput_node->node_reclaim_func=NULL;
	poutput_node->node_reclaim_func=NULL;
	
	if(use_dev_numa){
		node_socket_id=rte_eth_dev_socket_id(pif->port_id);
		dev_lcore_id=get_io_lcore_by_socket_id(node_socket_id);
	}else{
		dev_lcore_id=get_io_lcore();/*allocate a randomized numa and lcore*/
		if(validate_lcore_id(dev_lcore_id))
			node_socket_id=lcore_to_socket_id(dev_lcore_id);
	}

	pinput_node->lcore_id=dev_lcore_id;/*arrange two nodes on the same sockets*/
	if(!validate_lcore_id(pinput_node->lcore_id))
		goto error_node_dealloc;
	
	poutput_node->lcore_id=get_io_lcore_by_socket_id(node_socket_id);
	if(!validate_lcore_id(poutput_node->lcore_id)){
		goto error_node_dealloc;
	}
	
	if(register_node(pinput_node))
		goto error_node_uninit;
	if(register_node(poutput_node))
		goto error_node_uninit;
	
	pif->input_node=pinput_node->node_index;
	pif->output_node=poutput_node->node_index;
	
	#if 1
		E3_ASSERT(find_node_by_index(pif->input_node)==pinput_node);
		E3_ASSERT(find_node_by_index(pif->output_node)==poutput_node);
	#endif
	
	/*configure port and queues*/
	rc=rte_eth_dev_configure(pif->port_id,1,1,&port_conf);
	if(rc<0){
		E3_ERROR("error occurs during configuring port:%d\n",pif->port_id);
		goto error_node_uninit;
	}
	rte_eth_promiscuous_enable(pif->port_id);
	mempool=get_mempool_by_socket_id(node_socket_id);
	if(!mempool){
		E3_ERROR("error occurs during fetch per-socket mempool :%d\n",node_socket_id);
		goto error_node_uninit;
	}
	
	rc=rte_eth_rx_queue_setup(pif->port_id,
		0,
		DEFAULT_RX_DESCRIPTORS,
		node_socket_id,
		NULL,
		mempool);
	if(rc<0){
		E3_ERROR("error occurs during rx queue setup:%d\n",pif->port_id);
		goto error_node_uninit;
	}
	rc=rte_eth_tx_queue_setup(pif->port_id,
		0,
		DEFAULT_TX_DESCRIPTORS,
		node_socket_id,
		NULL);
	if(rc<0){
		E3_ERROR("error occurs during tx queue setup:%d\n",pif->port_id);
		goto error_node_uninit;
	}
	rc=rte_eth_dev_start(pif->port_id);
	if(rc<0){
		E3_ERROR("error occurs during start interface:%d\n",pif->port_id);
		goto error_node_uninit;
	}
	rte_eth_link_get(port_id,&pif->link_info);/*may block for seconds before it returns */
	{/*make interface more human readable*/
		rte_eth_dev_get_name_by_port(port_id,dev_data_name);
		lptr=dev_data_name;
		if(*lptr<='z'&&*lptr>='a')
			*lptr+='A'-'a';
		while(*lptr){
			if(*lptr==':'||*lptr=='.')
				*lptr='/';
			lptr++;
		}
		sprintf((char*)pif->ifname,"%s%s",link_speed_to_string(pif->link_info.link_speed),dev_data_name);/*rename interface name*/
	}
	
	
	/*attach node to lcore right now*/
	rc=attach_node_to_lcore(pinput_node);
	if(rc){
		E3_ERROR("can not attach node:%s to lcore :%d\n",pinput_node->name,pinput_node->lcore_id);
		goto error_node_uninit;
	}
	rc=attach_node_to_lcore(poutput_node);
	if(rc){
		E3_ERROR("can not attach node:%s to lcore :%d\n",poutput_node->name,poutput_node->lcore_id);
		goto error_node_detach;
	}

	/*set next nodes entries*/
	rc=set_node_to_class_edge((char*)pinput_node->name,DEVICE_NEXT_ENTRY_TO_L2_INPUT,"l2-input-class");
	if(rc){
		E3_ERROR("can not set node:%s to class:%s edge\n",pinput_node->name,"l2-input-class");
		goto error_node_detach;
	}
	pif->port_status=PORT_STATUS_DOWN;
	rcu_assign_pointer(pif->if_avail_ptr,!(NULL));/*make this port available now*/
	
	E3_LOG("add interface:%s with input-node:%s on %d and output-node:%s on %d\n",pif->ifname,
		pinput_node->name,
		pinput_node->lcore_id,
		poutput_node->name,
		poutput_node->lcore_id);
	
	pinput_node->node_reclaim_func=input_and_output_reclaim_func;
	poutput_node->node_reclaim_func=input_and_output_reclaim_func;
	
	return 0;
	error_node_detach:
		detach_node_from_lcore(pinput_node);
		detach_node_from_lcore(poutput_node);
	error_node_uninit:
		unregister_node(pinput_node);
		unregister_node(poutput_node);
		
	error_node_dealloc:
		if(validate_lcore_id(pinput_node->lcore_id))
			put_lcore(pinput_node->lcore_id,1);
		if(validate_lcore_id(poutput_node->lcore_id))
			put_lcore(poutput_node->lcore_id,1);
		if(pinput_node)
			rte_free(pinput_node);
		if(poutput_node)
			rte_free(poutput_node);
		
	error_dev_detach:
		{
			int release_rc;
			char dev_name[128];
			rte_eth_dev_stop(port_id);
			rte_eth_dev_close(port_id);
			release_rc=rte_eth_dev_detach(port_id,dev_name);
			if(release_rc)
				E3_LOG("error occurs in releasing %s due to error in registering dpdk vport\n",dev_name);
		}

		return -1;
}
int find_port_id_by_ifname(const char* ifname)
{
	int idx=0;
	int port_id=-1;
	struct E3interface *pif;
	void *if_avail_ptr;
	for(idx=0;idx<RTE_MAX_ETHPORTS;idx++){/*search available list*/
		pif=&ginterface_array[idx];
		if_avail_ptr=rcu_dereference(pif->if_avail_ptr);
		if(!if_avail_ptr)
			continue;
		if(!strcmp((char*)pif->ifname,ifname)){
			port_id=(int)pif->port_id;
			break;
		}
	}
	if(port_id<0&&port_id>RTE_MAX_ETHPORTS)
		port_id=-1;
	return port_id;
}
void interface_release_rcu_callback(struct rcu_head * rcu)
{
	char dev_name[128];
	
	int rc;
	struct E3interface *pif;
	struct node * pinput_node=NULL;
	struct node * poutput_node=NULL;
	pif=container_of(rcu,struct E3interface,rcu);
	pinput_node=find_node_by_index(pif->input_node);
	poutput_node=find_node_by_index(pif->output_node);
	/*free mbus in output node*/
	clear_node_ring_buffer(poutput_node);
	/*clear next edge entries*/
	clean_node_next_edges((char*)pinput_node->name);
	/*unregister nodes*/
	if(pinput_node)
		unregister_node(pinput_node);
	if(poutput_node)
		unregister_node(poutput_node);
	/*close dpdk device*/
	rte_eth_dev_stop(pif->port_id);
	rte_eth_dev_close(pif->port_id);
	rc=rte_eth_dev_detach(pif->port_id,dev_name);
	if(rc)
		E3_WARN("detaching port id:%d fails",pif->port_id)
	E3_LOG("delete interface:%s\n",pif->ifname);

	/*clean previous port fingerprint*/
	memset(pif,0x0,sizeof(struct E3interface));
	rcu_assign_pointer(pif->if_avail_ptr,NULL);/*additional notification*/
}

void unregister_native_dpdk_port(int port_id)
{
	int rc;
	struct node * pinput_node=NULL;
	struct node * poutput_node=NULL;
	struct E3interface *pif;
	if(port_id<0&&port_id>RTE_MAX_ETHPORTS){
		E3_WARN("invalid port id to be released:%d\n",port_id);
		return ;
	}
	pif=&ginterface_array[port_id];
	if(!pif->if_avail_ptr){
		E3_WARN("port: %d not available currently\n",port_id);
		return ;
	}
	/*detach node from lcore first*/
	pinput_node=find_node_by_index(pif->input_node);
	if(!pinput_node){
		E3_WARN("input node :%d not found\n",pif->input_node);
	}else{
		rc=detach_node_from_lcore(pinput_node);
		if(rc)
			{E3_WARN("errors occurs in detaching node :%d from lcore\n",pif->input_node);}
		else 
			put_lcore(pinput_node->lcore_id,1);
		
	}
	poutput_node=find_node_by_index(pif->output_node);
	if(!poutput_node){
		E3_WARN("output node :%d not found\n",pif->output_node);
	}else{
		rc=detach_node_from_lcore(poutput_node);
		if(rc)
			{E3_WARN("errors occurs in detaching node :%d from lcore\n",pif->output_node);}
		else
			put_lcore(poutput_node->lcore_id,1);
	}

	#if 0
	/*stop queues immediately*/
	if(rte_eth_dev_rx_queue_stop(pif->port_id,0))
		E3_WARN("errors occurs in release port %d rx queue:%d\n",pif->port_id,0);
	if(rte_eth_dev_tx_queue_stop(pif->port_id,0))
		E3_WARN("errors occurs in release port %d tx queue:%d\n",pif->port_id,0);
	#endif
	
	/*call releasing bottom half in quiescent state*/
	if(pif->under_releasing)
		return ;
	pif->under_releasing=1;
	call_rcu(&pif->rcu,interface_release_rcu_callback);
	
}
void device_module_test(void)
{
	#if 1
	register_native_dpdk_port("0000:02:01.0",0);
	register_native_dpdk_port("0000:02:06.0",0);
	register_native_dpdk_port("0000:02:07.0",0);
	//getchar();
	//unregister_native_dpdk_port(find_port_id_by_ifname("1GEthernet2/1/0"));
	
	#else 
	register_native_dpdk_port("0000:01:00.0",0);
	register_native_dpdk_port("0000:01:00.1",0);
	return ;
	getchar();
	puts("ready:");
	unregister_native_dpdk_port(find_port_id_by_ifname("10GEthernet1/0/0"));
	getchar();
	puts("ready1:");
	unregister_native_dpdk_port(find_port_id_by_ifname("10GEthernet1/0/1"));
	getchar();
	puts("back:");
	register_native_dpdk_port("0000:01:00.0",0);
	getchar();
	puts("back1:");
	register_native_dpdk_port("0000:01:00.1",0);

	
	#endif
	
	#if 0
	//register_native_dpdk_port("0000:01:00.2",0);
	register_native_dpdk_port("eth_pcap0,iface=virbr0",0);
	//register_native_dpdk_port("eth_pcap1,iface=tapbfefc8e2-b1",0);
	getchar();
	unregister_native_dpdk_port(find_port_id_by_ifname("1GEthernet2/6/0"));
	getchar();
	register_native_dpdk_port("0000:02:07.0",0);
	getchar();
	register_native_dpdk_port("0000:02:06.0",0);

	//unregister_native_dpdk_port(find_port_id_by_ifname("10GEthernet3/0/1"));
	//dump_nodes(stdout);
	#endif
}
__attribute__((constructor)) void device_module_init(void)
{
	memset(ginterface_array,0x0,sizeof(ginterface_array));
}
