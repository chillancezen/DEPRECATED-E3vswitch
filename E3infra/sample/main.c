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

extern struct node * lcore_task_list[MAX_LCORE_SUPPORTED];/*exported from lcore_extension*/
int node_dummy_process(void * arg __rte_unused);
int node_dummy_process(void * arg __rte_unused)
{

	
	return 0;
}
void node_dummy_reclaim(struct rcu_head * rcu __rte_unused);
void node_dummy_reclaim(struct rcu_head * rcu __rte_unused)
{
	printf("helloworld:%d\n",rte_lcore_id());
}

struct node node={
	.name="management-node",
	.node_process_func=node_dummy_process,
	.node_reclaim_func=node_dummy_reclaim,
	.node_type=node_type_misc,
	.lcore_id=1
};
struct node node1={
	.name="dpdk-input",
	.node_process_func=node_dummy_process,
	.node_reclaim_func=node_dummy_reclaim,
	.node_type=node_type_misc,
	.lcore_id=2
};

struct node_class class1={
	.class_name="l2-input-class",
	.class_reclaim_func=node_dummy_reclaim,
	.current_node_nr=0
};
struct node_class class2={
	.class_name="if1-mq-input-class",
	.class_reclaim_func=node_dummy_reclaim,
	.current_node_nr=0
};


static int
lcore_hello(__attribute__((unused)) void *arg)
{
	//struct node ** lcore_task_list=(struct node **)get_lcore_task_list_base();

	int last_cnt=0;
	int cnt=0;
	struct node* pnode;
	unsigned lcore_id=rte_lcore_id();
	//printf("enter :%d\n",lcore_id);
	rcu_register_thread();
	while(1/*should_stop()*/){
		cnt=0;
		foreach_node_in_lcore(pnode,lcore_id){
			cnt++;
			pnode->node_process_func(pnode);
			rcu_quiescent_state();
		}
		if(cnt!=last_cnt){
			last_cnt=cnt;
			printf("node in lcore %d:%d\n",lcore_id,cnt);
		}
		if(!cnt)
			rcu_quiescent_state();
	}
	rcu_thread_offline();
	rcu_unregister_thread();
	return 0;
}
int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	init_lcore_extension();

	#if 0
	register_node(&node);
	register_node(&node1);
	register_node_class(&class1);
	register_node_class(&class2);
	dump_node_class(stdout);
	attach_node_to_lcore(&node);
	attach_node_to_lcore(&node1);
	#endif
	
	

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
	}
	#if 0
	getchar();
	set_node_to_node_edge("management-node",23,"dpdk-input");
	set_node_to_class_edge("management-node",2,"l2-input-class");
	set_node_to_class_edge("management-node",63,"if1-mq-input-class");
	//unregister_node_class(&class1);
	//unregister_node_class(&class2);
	dump_node_edges("management-node");
	#endif
	device_module_test();
	lcore_hello(NULL);
	while(1)
		sleep(1);
	rte_eal_mp_wait_lcore();
	return 0;
}
