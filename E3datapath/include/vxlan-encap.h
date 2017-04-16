#ifndef VXLAN_ENCAP_H
#define VXLAN_ENCAP_H
#include <real-server.h>
#include <l3-interface.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <mq-device.h>
__attribute__((always_inline))
	static inline uint16_t calculate_ipv4_csum_mannually(struct ipv4_hdr * ip_hdr)
{
	uint32_t csum=0;
	int idx=0;
	ip_hdr->hdr_checksum=0;
	for(idx=0;idx<10;idx++){
		csum+=*(idx+(uint16_t*)ip_hdr);
		while(csum>>16)
			csum=(csum&0xffff)+(csum>>16);
	}
	ip_hdr->hdr_checksum=~csum;
	
	return 0;
}
__attribute__((always_inline))
	static inline int vxlan_encapsulate_mbuf(struct rte_mbuf * mbuf,
	struct real_server * preal_svr,
	struct l3_interface * plocal_interface,
	struct l3_interface * pphy_l3iface,
	struct E3interface * pe3_iface,
	int offload_outer_csum)
{
	struct ether_hdr * outer_eth_hdr=rte_pktmbuf_mtod(mbuf,struct ether_hdr*);
	struct ipv4_hdr  * ip_hdr=(struct ipv4_hdr*)(outer_eth_hdr+1);
	struct udp_hdr   * udp_hdr=(struct udp_hdr*)(ip_hdr+1);
	struct vxlan_hdr * vxlan_hdr=(struct vxlan_hdr*)(udp_hdr+1);
	
	vxlan_hdr->vx_flags=0x0008;
	vxlan_hdr->vx_vni=preal_svr->tunnel_id;

	/*how to determine SRC port?,here we use CRC hash which is extremely fast*/
	udp_hdr->src_port=0x7fff&crc32_hash(8+(uint8_t*)vxlan_hdr,7);
	udp_hdr->dst_port=0xb512;
	udp_hdr->dgram_len=mbuf->pkt_len-34;
	udp_hdr->dgram_len=SWAP_ORDER16(udp_hdr->dgram_len);
	udp_hdr->dgram_cksum=0;

	ip_hdr->version_ihl=0x45;
	ip_hdr->type_of_service=0x0;
	ip_hdr->total_length=mbuf->pkt_len-14;
	ip_hdr->total_length=SWAP_ORDER16(ip_hdr->total_length);
	ip_hdr->packet_id=preal_svr->rs_host_ipv4_identity++;
	ip_hdr->packet_id=SWAP_ORDER16(ip_hdr->packet_id);
	ip_hdr->fragment_offset=0x0040;
	ip_hdr->time_to_live=0x40;
	ip_hdr->next_proto_id=0x11;
	ip_hdr->hdr_checksum=0;
	ip_hdr->src_addr=pphy_l3iface->if_ip_as_u32;
	ip_hdr->dst_addr=preal_svr->rs_host_ipv4;

	outer_eth_hdr->ether_type=0x0008;
	copy_ether_address(outer_eth_hdr->d_addr.addr_bytes,preal_svr->rs_host_mac);
	copy_ether_address(outer_eth_hdr->s_addr.addr_bytes,pe3_iface->mac_addr.addr_bytes);

	if(offload_outer_csum){
		mbuf->outer_l2_len=14;
		mbuf->outer_l3_len=20;
		mbuf->ol_flags|=PKT_TX_OUTER_IPV4|PKT_TX_OUTER_IP_CKSUM|PKT_TX_TUNNEL_VXLAN;
	}else
		calculate_ipv4_csum_mannually(ip_hdr);
	
	if(pphy_l3iface->vlan_tci){
		mbuf->vlan_tci=pphy_l3iface->vlan_vid;
		mbuf->ol_flags|=PKT_TX_VLAN_PKT;
	}
	return 0;
}
#endif
