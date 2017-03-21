#ifndef _MQ_DEVICE_H
#define _MQ_DEVICE_H
#include <device.h>

struct next_edge_item{
	uint16_t fwd_behavior;
	int edge_entry;
	char* next_ref;
};
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


void mq_device_module_test(void);

#endif