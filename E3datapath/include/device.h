#ifndef _DEVICE_H
#define _DEVICE_H
#include <rte_config.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <e3_log.h>
#include <urcu-qsbr.h>
#include <util.h>

#define MAX_INTERFACE_NAME_LEN 64
#define DEFAULT_RX_DESCRIPTORS 1024
#define DEFAULT_TX_DESCRIPTORS 1024
#define PORT_STATUS_UP 0x1
#define PORT_STATUS_DOWN 0x0

#define PORT_TYPE_DEFAULT 0x0
#define PORT_TYPE_VLINK 0x1
#define PORT_TYPE_LB_EXTERNAL 0x10
#define PORT_TYPE_LB_INTERNAL 0x20

#define MAX_NR_QUEUES_TO_POLL 8

struct E3interface
{
	uint8_t ifname[MAX_INTERFACE_NAME_LEN];
	__attribute__((aligned(64))) uint64_t cacheline1[0];
	
	void* if_avail_ptr;/*indicate whether this interface is available,non-NULL reveals availability*/
	uint8_t port_status;/*not same with status of physical link*/
	uint8_t port_type;
	uint16_t input_node;
	uint16_t output_node;
	uint16_t nr_queues;
	uint16_t input_node_arrar[MAX_NR_QUEUES_TO_POLL];
	uint16_t output_node_arrar[MAX_NR_QUEUES_TO_POLL];
	union{
		void * private;
		uint64_t private_as_u64;
	};
	__attribute__((aligned(64))) uint64_t cacheline2[0];
	struct rcu_head  rcu;
	uint16_t port_id;
	uint8_t under_releasing;
	struct ether_addr mac_addr;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link_info;
};

struct next_edge_item{
	uint16_t fwd_behavior;
	int edge_entry;
	char* next_ref;
};

struct device_ops{
	uint8_t device_port_type;
	int (*capability_check)(int);
	int (*input_node_process_func)(void *arg);
	int (*output_node_process_func)(void *arg);
	int predefined_edges;
	struct next_edge_item edges[8];
};

extern struct E3interface ginterface_array[RTE_MAX_ETHPORTS];
void unregister_native_dpdk_port(int port_id);
int register_native_dpdk_port(const char * params,struct device_ops * ops,int *pport_id);
int find_port_id_by_ifname(const char* ifname);
void device_module_test(void);



#define DEVICE_NEXT_ENTRY_TO_L2_INPUT 0x0
#define DEVICE_NEXT_ENTRY_TO_MIRROR_PORT 0x1

char * link_speed_to_string(uint32_t speed);


#endif
