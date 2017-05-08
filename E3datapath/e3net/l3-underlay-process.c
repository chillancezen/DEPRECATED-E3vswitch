#include <l3-underlay-process.h>
#include <node.h>
#include <util.h>
#include <e3_log.h>
#include <rte_malloc.h>
#include <lcore_extension.h>
#include <init.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_icmp.h>
#include <device.h>
#include <l3-interface.h>
#include <mbuf_delivery.h>

#define L3_UNDERLAY_NODE_NAME "l3-underlay-node"
/*this route is part of L3 slow path as with l2-underlay-node,
it's ok to walk through the whole L3 interface list*/
int l3_under_process_poll_func(void * arg)
{
	struct node * pnode=(struct node * )arg;
	struct rte_mbuf * mbufs[64];
	int nr_mbufs;
	int nr_delivered;
	struct ether_hdr * eth_hdr;
	struct ipv4_hdr * ip_hdr;
	struct icmp_hdr * icmp_hdr;
	int idx=0;
	int port;
	uint32_t csum;
	struct E3interface * pe3if;
	struct l3_interface * l3iface,* target_l3iface=NULL;
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,
		(void**)mbufs,
		E3_MIN(pnode->burst_size,64));
	if(!nr_mbufs)
		return  0;
	for(idx=0;idx<nr_mbufs;idx++){
		{/*prefetch next packet payload into cache line*/
			if((idx+1)<nr_mbufs)
				rte_prefetch1(rte_pktmbuf_mtod(mbufs[idx+1],void*));
		}
		target_l3iface=NULL;
		port=mbufs[idx]->port;
		pe3if=find_e3iface_by_index(port);
		if(!is_e3interface_available(pe3if))
			goto drop_this_packet;
		eth_hdr=rte_pktmbuf_mtod(mbufs[idx],struct ether_hdr *);
		ip_hdr=(struct ipv4_hdr*)(eth_hdr+1);
		if(ip_hdr->next_proto_id!=0x1)
			goto drop_this_packet;
		icmp_hdr=(struct icmp_hdr*)(((ip_hdr->version_ihl&0xf)<<2)+(uint8_t*)ip_hdr);
		/*check whether the target IP address is one of them attached on the E3interface*/
		foreach_phy_l3_interface_safe_start(port,l3iface){
			if(l3iface->if_ip_as_u32==ip_hdr->dst_addr){
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
		/*modify icmp type and check sum*/
		icmp_hdr->icmp_type=IP_ICMP_ECHO_REPLY;
		csum=(~(icmp_hdr->icmp_cksum))&0xffff;
		csum+=0xfff7;
		while(csum>>16)
			csum=(csum&0xffff)+(csum>>16);
		icmp_hdr->icmp_cksum=(~csum)&0xffff;
		/*modify IP header,leave cheksum calcalation to hardware function*/
		ip_hdr->dst_addr=ip_hdr->src_addr;
		ip_hdr->src_addr=target_l3iface->if_ip_as_u32;
		ip_hdr->time_to_live=127;
		ip_hdr->hdr_checksum=0;
		mbufs[idx]->l2_len=sizeof(struct ether_hdr);
		mbufs[idx]->l3_len=(ip_hdr->version_ihl&0xf)<<2;
		mbufs[idx]->ol_flags=PKT_TX_IPV4|PKT_TX_IP_CKSUM;
		/*modify ethernet layer header*/
		copy_ether_address(eth_hdr->d_addr.addr_bytes,eth_hdr->s_addr.addr_bytes);
		//copy_ether_address(eth_hdr->s_addr.addr_bytes,pe3if->mac_addr.addr_bytes);
		copy_ether_address(eth_hdr->s_addr.addr_bytes,target_l3iface->if_mac);
		
		
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

void l3_under_process_early_init(void)
{
	struct node * pnode=NULL;
	pnode=rte_zmalloc(NULL,sizeof(struct node),64);
	E3_ASSERT(pnode);
	sprintf((char*)pnode->name,L3_UNDERLAY_NODE_NAME);
	pnode->lcore_id=get_lcore();
	E3_ASSERT(validate_lcore_id(pnode->lcore_id));
	pnode->burst_size=NODE_BURST_SIZE;
	pnode->node_priv=NULL;
	pnode->node_type=node_type_worker;
	pnode->node_reclaim_func=default_rte_reclaim_func;
	pnode->node_process_func=l3_under_process_poll_func;
	E3_ASSERT(!register_node(pnode));
	E3_ASSERT(pnode==find_node_by_name(L3_UNDERLAY_NODE_NAME));
	E3_ASSERT(!attach_node_to_lcore(pnode));
	E3_LOG("register node:%s on %d\n",L3_UNDERLAY_NODE_NAME,
		(int)pnode->lcore_id);
}

E3_init(l3_under_process_early_init,TASK_PRIORITY_EXTRA_LOW);


