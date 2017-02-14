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
#include <l2-input.h>
#include <mbuf_delivery.h>
#include <vlan-list.h>
#include <l2fib.h>
#include <e3_bitmap.h>





int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	
	
	init_lcore_extension();
	preserve_lcore_for_io(2);
	preserve_lcore_for_worker(1);
	l2fib_early_init();
	l2_input_early_init();
	l2_input_runtime_init();
	
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_default_entry, NULL, lcore_id);
	}

	
	
	device_module_test();
	
	#if 0
	struct l2fib_key key;
	//struct l2fib_entry *fib=allocate_l2fib_entry();
	int index=0;
	struct l2fib_entry *entry;
	key.vlan_id=0x235;
	key.mac_addr[0]=0x02;
	key.mac_addr[1]=0x12;
	key.mac_addr[2]=0x22;
	key.mac_addr[3]=0x32;
	key.mac_addr[4]=0x42;
	key.mac_addr[5]=0x52;
	int idx=0;
	for(idx=0;idx<33;idx++){
		key.vlan_id++;
		add_key_unsafe(key,idx,index,entry);
	}
	
	uint16_t lst_index=key.L1_index;

	key.vlan_id-=40;
	delete_key_unsafe(key);
	
	foreach_key_at_list_index_start(lst_index,index,entry){
		printf("%d %p\n",l2fib_port(entry,index),entry);
	}
	foreach_key_at_list_index_end();
	
	
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
