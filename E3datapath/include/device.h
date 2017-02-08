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

struct E3interface
{
	uint8_t ifname[MAX_INTERFACE_NAME_LEN];
	__attribute__((aligned(64))) uint64_t cacheline1[0];
	struct rcu_head  rcu;
	void* if_avail_ptr;/*indicate whether this interface is available,non-NULL reveals availability*/
	uint16_t input_node;
	uint16_t output_node;
	uint16_t port_id;
	uint8_t port_status;/*not same with status of physical link*/
	uint8_t under_releasing;
	
	__attribute__((aligned(64))) uint64_t cacheline2[0];
	struct ether_addr mac_addr;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link_info;
};

extern struct E3interface ginterface_array[RTE_MAX_ETHPORTS];




void device_module_test(void);



#endif