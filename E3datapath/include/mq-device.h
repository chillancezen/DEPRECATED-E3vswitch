#ifndef _MQ_DEVICE_H
#define _MQ_DEVICE_H
#include <device.h>


struct mq_device_ops{
	int mq_device_port_type;
	int nr_queues_to_poll;
	uint64_t hash_function;
	int (*capability_check)(int port_id);
	int (*input_node_process_func)(void *arg);
	int (*output_node_process_func)(void *arg);
	int predefined_edges;
	struct next_edge_item edges[8];/*only maximum 8 initializer ,may be extended later*/
};

#define DEV_PRIVATE(dev)  ((dev)->private)
#define DEV_PRIVATE_AS_U64(dev) ((dev)->private_as_u64)

int register_native_mq_dpdk_port(const char * params,struct mq_device_ops * dev_ops,int *pport_id);
void unregister_native_mq_dpdk_port(int port_id);

int start_e3_interface(int iface);
void mq_device_module_test(void);

#endif