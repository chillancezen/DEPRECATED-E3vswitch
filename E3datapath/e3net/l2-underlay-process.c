#include <l2-underlay-process.h>
#include <node.h>
#include <e3_log.h>
#include <init.h>
#include <rte_malloc.h>
#include <lcore_extension.h>
#include <rte_mbuf.h>
#include <rte_arp.h>
#include <rte_ether.h>
#include <l3-interface.h>
#include <mq-device.h>
#include <mbuf_delivery.h>

#define L2_UNDERLAY_NODE_NAME "l2-underlay-node"
int l2_under_process_poll_func(void * arg)
{
	int idx=0;
	int port;
	struct node * pnode=(struct node *)arg;
	struct rte_mbuf *mbufs[64];
	int nr_mbufs;
	struct ether_hdr * eth_hdr;
	struct arp_hdr * arp_hdr;
	struct E3interface *pe3if=NULL;
	struct l3_interface * l3iface=NULL;
	struct l3_interface * target_l3iface=NULL;
	int nr_delivered;
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,
		(void**)mbufs,
		E3_MIN(pnode->burst_size,64));
	if(!nr_mbufs)
		return 0;
	
	/*construct arp respone packet,*/
	for(idx=0;idx<nr_mbufs;idx++){
		target_l3iface=NULL;
		port=mbufs[idx]->port;
		
		{/*prefetch next mbuf's payload into L1 cache*/
			if((idx+1)<nr_mbufs)
				rte_prefetch1(rte_pktmbuf_mtod(mbufs[idx+1],void*));
		}
		pe3if=find_e3iface_by_index(port);
		if(!is_e3interface_available(pe3if))
			goto drop_this_packet;
		eth_hdr=rte_pktmbuf_mtod(mbufs[idx],struct ether_hdr*);
		if(eth_hdr->ether_type!=0x0608)
			goto drop_this_packet;
		arp_hdr=(struct arp_hdr*)(eth_hdr+1);
		if(arp_hdr->arp_op!=0x0100)
			goto drop_this_packet;
		/*search L3 interface with this E3inteface as its lower interface index*/
		foreach_phy_l3_interface_safe_start(port,l3iface){
			if(l3iface->if_ip_as_u32==arp_hdr->arp_data.arp_tip){
				target_l3iface=l3iface;
				break;
			}
		}
		foreach_phy_l3_interface_safe_end();
		if(!target_l3iface)
			goto drop_this_packet;
		/*check vlan whether matches*/
		if(mbufs[idx]->vlan_tci!=target_l3iface->vlan_vid)
			goto drop_this_packet;
		/*mangle arp packet to be a legal arp responder*/
		arp_hdr->arp_op=0x0200;
		arp_hdr->arp_data.arp_tip=arp_hdr->arp_data.arp_sip;
		arp_hdr->arp_data.arp_sip=target_l3iface->if_ip_as_u32;
		copy_ether_address(arp_hdr->arp_data.arp_tha.addr_bytes,arp_hdr->arp_data.arp_sha.addr_bytes);
		//copy_ether_address(arp_hdr->arp_data.arp_sha.addr_bytes,pe3if->mac_addr.addr_bytes);
		copy_ether_address(arp_hdr->arp_data.arp_sha.addr_bytes,l3iface->if_mac);
		copy_ether_address(eth_hdr->d_addr.addr_bytes,eth_hdr->s_addr.addr_bytes);
		//copy_ether_address(eth_hdr->s_addr.addr_bytes,pe3if->mac_addr.addr_bytes);
		copy_ether_address(eth_hdr->s_addr.addr_bytes,l3iface->if_mac);
		if(mbufs[idx]->vlan_tci)
			mbufs[idx]->ol_flags|=PKT_TX_VLAN_PKT;
		nr_delivered=deliver_mbufs_to_node(pe3if->output_node_arrar[0],&mbufs[idx],1);
		if(nr_delivered!=1)
			goto drop_this_packet;
		continue;
		drop_this_packet:
			rte_pktmbuf_free(mbufs[idx]);
	}
	return 0;
}

void l2_under_process_early_init(void)
{
	struct node * pnode=NULL;
	pnode=rte_zmalloc(NULL,sizeof(struct node),64);
	E3_ASSERT(pnode);
	sprintf((char*)pnode->name,L2_UNDERLAY_NODE_NAME);
	pnode->lcore_id=get_lcore();/*any node is ok*/
	E3_ASSERT(validate_lcore_id(pnode->lcore_id));
	pnode->burst_size=NODE_BURST_SIZE;
	pnode->node_priv=NULL;
	pnode->node_type=node_type_worker;
	pnode->node_reclaim_func=default_rte_reclaim_func;
	pnode->node_process_func=l2_under_process_poll_func;
	E3_ASSERT(!register_node(pnode));
	E3_ASSERT(pnode==find_node_by_name(L2_UNDERLAY_NODE_NAME));
	E3_ASSERT(!attach_node_to_lcore(pnode));
	E3_LOG("register node:%s on %d\n",L2_UNDERLAY_NODE_NAME,
		(int)pnode->lcore_id);
}
E3_init(l2_under_process_early_init,TASK_PRIORITY_EXTRA_LOW);


