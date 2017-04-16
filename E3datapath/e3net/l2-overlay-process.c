#include <l2-overlay-process.h>
#include <node.h>
#include <rte_malloc.h>
#include <e3_log.h>
#include <util.h>
#include <init.h>
#include <lcore_extension.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_arp.h>
#include <rte_ip.h>
#include <real-server.h>
#include <l3-interface.h>
#include <vxlan-encap.h>
#include <mbuf_delivery.h>
#define L2_OVERLAY_NODE_NAME "l2-overlay-node"

int l2_overlay_process_func(void *arg)
{
	struct ether_hdr * outer_eth_hdr;
	struct ether_hdr * inner_eth_hdr;
	struct ipv4_hdr  * outer_ip_hdr;
	struct arp_hdr   * arp_hdr;
	int rs_number;
	struct real_server  * preal_svr;
	struct l3_interface * plocal_l3iface;
	struct l3_interface * pphy_l3iface;
	struct E3interface  * pe3_iface;
	
	int out_headers_len;
	struct node * pnode=(struct node*)arg;
	struct rte_mbuf *mbufs[64];
	int nr_mbufs;
	int idx=0;
	nr_mbufs=rte_ring_sc_dequeue_burst(pnode->node_ring,
		(void**)mbufs,
		E3_MIN(pnode->burst_size,64));
	if(!nr_mbufs)
		return 0;
	for(idx=0;idx<nr_mbufs;idx++){
		#define _(con) if(!(con)) goto drop_this_packlet;
		
		outer_eth_hdr=rte_pktmbuf_mtod(mbufs[idx],struct ether_hdr*);
		inner_eth_hdr=(struct ether_hdr*)(14+mbufs[idx]->outer_l3_len+8+8+(uint8_t*)outer_eth_hdr);
		rs_number=(int)mbufs[idx]->udata64;
		_(preal_svr=find_real_server_at_index(rs_number));
		_(plocal_l3iface=find_l3_interface_at_index(preal_svr->lb_iface));
		_(pphy_l3iface=find_l3_interface_at_index(plocal_l3iface->lower_if_index));
		_(pe3_iface=find_e3iface_by_index(pphy_l3iface->lower_if_index));
		E3_ASSERT(plocal_l3iface->if_type==L3_INTERFACE_TYPE_VIRTUAL);
		
		/*check whether content is legal*/
		_(inner_eth_hdr->ether_type==0x0608);
		arp_hdr=(struct arp_hdr*)(inner_eth_hdr+1);
		_(arp_hdr->arp_op==0x0100);
		_(arp_hdr->arp_data.arp_sip==preal_svr->rs_ipv4);
		_(arp_hdr->arp_data.arp_tip==plocal_l3iface->if_ip_as_u32);
		
		/*arp snooping to determine outer host MAC and ip address*/
		outer_ip_hdr=(struct ipv4_hdr*)(outer_eth_hdr+1);
		preal_svr->rs_host_ipv4=outer_ip_hdr->src_addr;
		copy_ether_address(preal_svr->rs_host_mac,outer_eth_hdr->s_addr.addr_bytes);
		
		/*adjust outer_headers in case ip headers has options*/
		out_headers_len=(uint8_t*)inner_eth_hdr-(uint8_t*)outer_eth_hdr;
		if(PREDICT_FALSE(out_headers_len!=50)){
			rte_pktmbuf_adj(mbufs[idx],out_headers_len);
			rte_pktmbuf_prepend(mbufs[idx],50);
			outer_eth_hdr=rte_pktmbuf_mtod(mbufs[idx],struct ether_hdr*);
		}
		/*generate ARP response packet*/
		arp_hdr->arp_op=0x0200;
		arp_hdr->arp_data.arp_tip=arp_hdr->arp_data.arp_sip;
		arp_hdr->arp_data.arp_sip=plocal_l3iface->if_ip_as_u32;
		copy_ether_address(arp_hdr->arp_data.arp_tha.addr_bytes,arp_hdr->arp_data.arp_sha.addr_bytes);
		copy_ether_address(arp_hdr->arp_data.arp_sha.addr_bytes,plocal_l3iface->if_mac);
		copy_ether_address(inner_eth_hdr->d_addr.addr_bytes,inner_eth_hdr->s_addr.addr_bytes);
		copy_ether_address(inner_eth_hdr->s_addr.addr_bytes,plocal_l3iface->if_mac);

		 
		mbufs[idx]->l3_len=0;
		mbufs[idx]->l4_len=0;
		vxlan_encapsulate_mbuf(mbufs[idx],
			preal_svr,
			plocal_l3iface,
			pphy_l3iface,
			pe3_iface,
			0);
		
		_(deliver_mbufs_to_node(pe3_iface->output_node_arrar[0],&mbufs[idx],1));
		#undef _
		continue;
		drop_this_packlet:
			rte_pktmbuf_free(mbufs[idx]);
	}
	
	
	return 0;
}
void l2_overlay_process_early_init(void)
{
	struct node * pnode=(struct node *)rte_zmalloc(NULL,sizeof(struct node),64);
	E3_ASSERT(pnode);
	sprintf((char*)pnode->name,"%s",L2_OVERLAY_NODE_NAME);
	pnode->lcore_id=get_lcore();
	E3_ASSERT(validate_lcore_id(pnode->lcore_id));
	pnode->burst_size=NODE_BURST_SIZE;
	pnode->node_priv=NULL;
	pnode->node_type=node_type_worker;
	pnode->node_reclaim_func=default_rte_reclaim_func;
	pnode->node_process_func=l2_overlay_process_func;
	E3_ASSERT(!register_node(pnode));
	E3_ASSERT(pnode==find_node_by_name(L2_OVERLAY_NODE_NAME));
	E3_ASSERT(!attach_node_to_lcore(pnode));
	E3_LOG("register node:%s on %d\n",(char*)pnode->name,
		(int)pnode->lcore_id);
}

E3_init(l2_overlay_process_early_init,TASK_PRIORITY_EXTRA_LOW);


