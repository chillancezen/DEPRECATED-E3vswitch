/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <urcu-qsbr.h>
#include <unistd.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <node.h>
#include <node_class.h>
#include <node_adjacency.h>
#include <lcore_extension.h>
#include <device.h>
//#include <l2-input.h>

#include <mbuf_delivery.h>
#include <mq-device.h>
#include <lb-common.h>
#include <fast-index.h>
#include <vip-resource.h>
#include <init.h>
#include <l3-interface.h>
#include <real-server.h>
#include <lb-instance.h>
#include <device-wrapper.h>
#include <l4-tunnel-process.h>
int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	
	init_registered_tasks();
	//init_lcore_extension();
	//preserve_lcore_for_io(2);
	//preserve_lcore_for_worker(1);
	//l2_input_early_init();
	//vip_resource_early_init();

	
	
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_default_entry, NULL, lcore_id);
	}
	add_e3_interface("eth_tap",NIC_VIRTUAL_DEV,PORT_TYPE_VLINK,NULL);
	add_e3_interface("0000:01:00.1",NIC_INTEL_82599,PORT_TYPE_LB_EXTERNAL,NULL);
	add_e3_interface("0000:03:00.1",NIC_INTEL_XL710,PORT_TYPE_LB_INTERNAL,NULL);


	
	struct l3_interface * l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_PHYSICAL;
	l3iface->lower_if_index=2;
	l3iface->vlan_vid=507;
	l3iface->if_ip_as_u32=MAKE_IP32(130,140,150,1);
	register_l3_interface(l3iface);

	
	l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_PHYSICAL;
	l3iface->lower_if_index=1;
	l3iface->vlan_vid=508;
	l3iface->if_ip_as_u32=MAKE_IP32(130,140,151,1);
	register_l3_interface(l3iface);
	

	/*register a tunnel L3 interface on interface:130,140,150,1*/
	l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_VIRTUAL;
	l3iface->lower_if_index=0;
	l3iface->if_ip_as_u32=MAKE_IP32(4,4,4,44);
	copy_ether_address(l3iface->if_mac,"\x22\x95\x5b\x6b\x6d\x22");
	register_l3_interface(l3iface);

	l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_VIRTUAL;
	l3iface->lower_if_index=0;
	l3iface->if_ip_as_u32=MAKE_IP32(5,5,5,55);
	copy_ether_address(l3iface->if_mac,"\x22\x95\x5b\x6b\x6d\x23");
	register_l3_interface(l3iface);
	
	
	struct real_server *rs=allocate_real_server();
	rs->tunnel_id=VNI_SWAP_ORDER(3685);
	rs->rs_ipv4=MAKE_IP32(4,4,4,4);
	rs->lb_iface=2;
	copy_ether_address(rs->rs_mac,"\x82\xe1\x5b\x6b\x6d\x2c");
	printf("register rc:%d\n",register_real_server(rs));
	printf("register rc lb-local-index:%d\n",rs->local_index);

	rs=allocate_real_server();
	rs->tunnel_id=VNI_SWAP_ORDER(3686);
	rs->rs_ipv4=MAKE_IP32(5,5,5,5);
	rs->lb_iface=3;
	copy_ether_address(rs->rs_mac,"\x96\x3d\xb1\xcf\x3a\x96");
	printf("register rc:%d\n",register_real_server(rs));
	printf("register rc lb-local-index:%d\n",rs->local_index);


	/*register vip resource*/
	struct virtual_ip * vip=allocate_virtual_ip();
	vip->ip_as_u32=MAKE_IP32(130,140,151,1);
	vip->virt_if_index=1;
	vip->lb_instance_index=0;
	register_virtual_ip(vip);
	copy_ether_address(vip->next_mac,"\x3c\xfd\xfe\x9e\x97\x26");
	
	struct lb_instance * lb=allocate_lb_instance("lb-test");
	register_lb_instance(lb);
	lb->vip_index=0;
	
	add_real_server_num_into_lb_member_pool(lb,0);
	//add_real_server_num_into_lb_member_pool(lb,1);
	dump_lb_members(lb);
	dump_l3_interfaces(stdout);

	
	
	
	#if 0
	struct lb_instance * lb=allocate_lb_instance("lb-test");
	lb->vip_index=0;
	register_lb_instance(lb);
	int idx=0;
	for(idx=0;idx<16;idx++)
		add_real_server_num_into_lb_member_pool(lb,idx);
	dump_lb_members(lb);
	add_real_server_num_into_lb_member_pool(lb,127);
	dump_lb_members(lb);
	getchar();
	for(idx=0;idx<120;idx++)
		del_real_server_num_from_lb_member_pool(lb,idx);
	dump_lb_members(lb);

	//del_real_server_num_from_lb_member_pool(lb,6);
	//dump_lb_members(lb);
	l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_PHYSICAL;
	l3iface->lower_if_index=2;
	l3iface->vlan_vid=0;
	l3iface->if_ip_as_u32=MAKE_IP32(130,140,160,1);
	register_l3_interface(l3iface);
	l3iface=allocate_l3_interface();
		l3iface->if_type=L3_INTERFACE_TYPE_PHYSICAL;
		l3iface->lower_if_index=1;
		l3iface->vlan_vid=507;
		l3iface->if_ip_as_u32=MAKE_IP32(130,140,150,3);
		register_l3_interface(l3iface);
	
	struct lb_instance * lb=allocate_lb_instance("lb-test");
	E3_ASSERT(lb);
	int rc=register_lb_instance(lb);
	printf("reg:%d %d\n",rc,lb->local_index);

	lb=allocate_lb_instance("lb-test1");
	rc=register_lb_instance(lb);
	printf("reg:%d %d\n",rc,lb->local_index);

	unregister_lb_instance(find_lb_instance_by_name("lb-test1"));
	dump_lb_instances(stdout);
	struct lb_instance * lb=allocate_lb_instance("lb-test");
	E3_ASSERT(lb);

	int rc=register_lb_instance(lb);
	printf("reg:%d %d\n",rc,lb->local_index);

	lb=allocate_lb_instance("lb-test1");
	rc=register_lb_instance(lb);
	printf("reg:%d %d\n",rc,lb->local_index);

	char buffer[64];
	int idx=0;
	for(idx=0;idx<256;idx++){
		memset(buffer,0x0,sizeof(buffer));
		sprintf(buffer,"test-lb-%d",idx);
		lb=allocate_lb_instance(buffer);
		rc=register_lb_instance(lb);
		printf("reg:%d %d %d\n",idx,rc,lb->local_index);
	}
		int rc;
	struct real_server * rs;
	int idx=0;
	for(idx=0;idx<128;idx++){
		rs=allocate_real_server();
			E3_ASSERT(rs);
		rs->tunnel_id=0x23df4c+idx;
		rs->rs_mac[0]=0x03;
		rs->rs_mac[1]=0x5d;
		rs->rs_mac[2]=0xfa;
		rs->rs_mac[3]=0x23;
		rs->rs_mac[4]=0x8f;
		rs->rs_mac[5]=0x4f;
		rc=register_real_server(rs);
	}
	for(idx=20;idx<1010;idx++){
		unregister_real_server(find_real_server_at_index(idx));
	}
	dump_real_servers(stdout);
	dump_findex_2_1_6_base(rs_base);
	struct findex_2_1_6_base * base=allocate_findex_2_1_6_base();
	E3_ASSERT(base);
	struct findex_2_1_6_key key;
	key.key_index=0x3;
	key.tag0=0x23;
	key.tag1=0x2564;
	key.tag2=0x1f2e;
	key.tag3=0x2530;
	key.value_as_u64=0x23564fd;
	add_index_2_1_6_item_unsafe(base,&key);

	key.key_index=0x3;
	key.tag0=0x23;
	key.tag1=0x2564;
	key.tag2=0x1f2e;
	key.tag3=0x2531;
	key.value_as_u64=0x23564fd;
	add_index_2_1_6_item_unsafe(base,&key);

	key.key_index=0x3;
	key.tag0=0x25;
	key.tag1=0x2564;
	key.tag2=0x12fd;
	key.tag3=0x2531;
	key.value_as_u64=0x245d4;
	add_index_2_1_6_item_unsafe(base,&key);

	
	dump_findex_2_1_6_base(base);
	
	key.key_index=0x3;
	key.tag0=0x23;
	key.tag1=0x2564;
	key.tag2=0x1f2e;
	key.tag3=0x2530;
	delete_index_2_1_6_item_unsafe(base,&key);

	key.key_index=0x3;
	key.tag0=0x23;
	key.tag1=0x2564;
	key.tag2=0x1f2e;
	key.tag3=0x2531;
	delete_index_2_1_6_item_unsafe(base,&key);

	key.key_index=0x3;
	key.tag0=0x25;
	key.tag1=0x2564;
	key.tag2=0x12fd;
	key.tag3=0x2531;
	delete_index_2_1_6_item_unsafe(base,&key);

	dump_findex_2_1_6_base(base);
	struct findex_2_1_6_base * base=allocate_findex_2_1_6_base();
		E3_ASSERT(base);
		base[5].next=allocate_findex_2_1_6_entry();
		E3_ASSERT(base[5].next);
		struct findex_2_1_6_entry * pentry=base[5].next;
		pentry->tag0=0x12;
	#define INDEX0 13
	#define TAG_INDEX (INDEX0/16)
		pentry->tags[TAG_INDEX].tag1[INDEX0-16*TAG_INDEX]=0x3412;
		pentry->tags[TAG_INDEX].tag2[INDEX0-16*TAG_INDEX]=0x0233;
		pentry->tags[TAG_INDEX].tag3[INDEX0-816*TAG_INDEX]=0x3332;
		pentry->values[INDEX0]=0x25df45;
		//e3_bitmap_set_bit(pentry->tag_avail,INDEX0);
	
	
		
	#define INDEX0 47
	#define TAG_INDEX (INDEX0/16)
			pentry->tags[TAG_INDEX].tag1[INDEX0-16*TAG_INDEX]=0x3412;
			pentry->tags[TAG_INDEX].tag2[INDEX0-16*TAG_INDEX]=0x0233;
			pentry->tags[TAG_INDEX].tag3[INDEX0-16*TAG_INDEX]=0x3332;
			pentry->values[INDEX0]=0x25dddff;
			e3_bitmap_set_bit(pentry->tag_avail,INDEX0);
	
		struct findex_2_1_6_key key;
		key.key_index=5;
		key.tag0=0x12;
		key.tag1=0x3412;
		key.tag2=0x0233;
		key.tag3=0x3332;
	
		
		printf("find-rc:%d\n",fast_index_2_1_6_item_safe(base,&key));
		printf("find-val:%p\n",key.value_as_ptr);

	struct findex_2_1_6_base * base=allocate_findex_2_1_6_base();
	E3_ASSERT(base);
	base[5].next=allocate_findex_2_1_6_entry();
	E3_ASSERT(base[5].next);
	struct findex_2_1_6_entry * pentry=base[5].next;
	pentry->tag0=0xfe;
	#define INDEX0 13
	#define TAG_INDEX (INDEX0/8)
	pentry->tags[TAG_INDEX].tag1[INDEX0-8*TAG_INDEX]=0x3412;
	pentry->tags[TAG_INDEX].tag2[INDEX0-8*TAG_INDEX]=0x0233;
	pentry->tags[TAG_INDEX].tag3[INDEX0-8*TAG_INDEX]=0x3332;
	pentry->values[INDEX0]=0x25df45;
	//e3_bitmap_set_bit(pentry->tag_avail,INDEX0);

	#undef INDEX0
	#define INDEX0 14
	#define TAG_INDEX (INDEX0/8)
	pentry->tags[TAG_INDEX].tag1[INDEX0-8*TAG_INDEX]=0x3416;
	pentry->tags[TAG_INDEX].tag2[INDEX0-8*TAG_INDEX]=0x0233;
	pentry->tags[TAG_INDEX].tag3[INDEX0-8*TAG_INDEX]=0x3332;
	pentry->values[INDEX0]=0x52584d;
	e3_bitmap_set_bit(pentry->tag_avail,INDEX0);

	
	struct findex_2_1_6_key key;
	key.key_index=5;
	key.tag0=0xfe;
	key.tag1=0x3412;
	key.tag2=0x0233;
	key.tag3=0x3332;

	
	printf("find-rc:%d\n",fast_index_2_1_6_item_safe(base,&key));
	printf("find-val:%p\n",key.value_as_ptr);
	register_native_dpdk_port("eth_tap",0,NULL);
	register_native_dpdk_port("eth_tap",0,NULL);
	

	struct l3_interface * l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_PHYSICAL;
	l3iface->lower_if_index=0;
	l3iface->if_ip_as_u32=0xe421e852;
	register_l3_interface(l3iface);

	l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_PHYSICAL;
	l3iface->lower_if_index=0;
	l3iface->if_ip_as_u32=0x25d56e2a;
	register_l3_interface(l3iface);

	l3iface=allocate_l3_interface();
	l3iface->if_type=L3_INTERFACE_TYPE_VIRTUAL;
	l3iface->lower_if_index=0;
	l3iface->if_ip_as_u32=0x25d56e2a;
	register_l3_interface(l3iface);
	
	//unregister_l3_interface(gl3if_array[0]);

	foreach_phy_l3_interface_safe_start(0,l3iface){
		printf("list:%p\n",l3iface);
	}
	foreach_phy_l3_interface_safe_end();
	//
	dump_l3_interfaces(stdout);
	int idx=0;
	struct virtual_ip* vip=NULL;
	for(idx=0;idx<25;idx++){
		vip=allocate_virtual_ip();
		vip->ip_as_u32=0x124d54f+idx;
		register_virtual_ip(vip);
	}
	
	for(idx=20;idx<29;idx++)
		printf("%d\n",search_virtual_ip_index(0x124d54f+idx));
	//dump_virtual_ips(stdout);

	for(idx=0;idx<1022;idx++)
		unregister_virtual_ip(find_virtual_ip_at_index(idx));
	dump_findex_2_2_base(ip2vip_base);
	exit(0);

	
	int idx=0;
	struct virtual_ip* vip=NULL;
	
	for(idx=0;idx<1300;idx++){
		vip=allocate_virtual_ip();
		vip->ip_as_u32=0x124d54f+idx;
		register_virtual_ip(vip);
	}
	for(idx=100;idx<1022;idx++)
		unregister_virtual_ip(find_virtual_ip_at_index(idx));
	
	getchar();
	dump_virtual_ips(stdout);
	struct findex_2_2_base * base=allocate_findex_2_2_base();
	struct findex_2_2_key key;
	int idx=0;
	key.key_tag=0x23;
	key.value_as_u64=0x232;
	for(idx=0;idx<64;idx++){
		key.key_index=0x233;
		key.key_tag++;
		key.value_as_u64++;
		add_index_2_2_item_unsafe(base,&key);
	}


	
	key.key_tag=0x23;
	key.value_as_u64=0x232;
	for(idx=0;idx<85;idx++){
		
		key.key_index=0x233;
		key.key_tag++;
		key.value_as_u64++;
		if(idx==43)
			continue;
		delete_index_2_2_item_unsafe(base,&key);
	}
	
	int rc=fast_index_2_2_item_safe(base,&key);
	printf("%d %p\n",rc,key.value_as_ptr);

	dump_findex_2_2_base(base);
	
	
	device_module_test();
	l2_input_runtime_init();
	mq_device_module_test();
	
	if (0)
	{
			/*express delivery framework */
			int iptr;
			int start_index,end_index;
			uint64_t fwd_id=1;
			int process_rc;
			DEF_EXPRESS_DELIVERY_VARS();
			RESET_EXPRESS_DELIVERY_VARS();
			pre_setup_env(32);
			while((iptr=peek_next_mbuf())>=0){
				/*here to decide fwd_id*/
				fwd_id=(iptr%4)?fwd_id:fwd_id+1;
				process_rc=proceed_mbuf(iptr,fwd_id);
				if(process_rc==MBUF_PROCESS_RESTART){
					fetch_pending_index(start_index,end_index);
					printf("handle buffer:%d-%d\n",start_index,end_index);
					flush_pending_mbuf();
					proceed_mbuf(iptr,fwd_id);
				}
				
			}
			fetch_pending_index(start_index,end_index);
			printf("handle buffer:%d-%d\n",start_index,end_index);
	}
	struct node *pnode=find_node_by_name("device-input-node-0");
	printf("next_node:%d\n",next_forwarding_node(pnode,DEVICE_NEXT_ENTRY_TO_L2_INPUT));
	unregister_native_dpdk_port(find_port_id_by_ifname("1GEthernet2/1/0"));
	getchar();
	pnode=find_node_by_name("device-input-node-1");
	printf("next_node:%d\n",next_forwarding_node(pnode,DEVICE_NEXT_ENTRY_TO_L2_INPUT));
	
	
	struct node *pnode=find_node_by_name("device-input-node-0");

	printf("next_node:%d\n",next_forwarding_node(pnode,DEVICE_NEXT_ENTRY_TO_L2_INPUT));
	printf("next_node:%d\n",next_forwarding_node(pnode,DEVICE_NEXT_ENTRY_TO_L2_INPUT));
	
	pnode=find_node_by_name("device-input-node-1");
	printf("next_node:%d\n",next_forwarding_node(pnode,DEVICE_NEXT_ENTRY_TO_L2_INPUT));
	#endif
	getchar();
	
	dump_nodes(stdout);
	dump_node_class(stdout);
	
	lcore_default_entry(NULL);/*master core enters loops*/
	while(1)
		sleep(1);
	rte_eal_mp_wait_lcore();
	return 0;
}
