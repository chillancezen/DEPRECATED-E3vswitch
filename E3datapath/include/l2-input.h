#ifndef _L2_INPUT_H
#define _L2_INPUT_H
#include <node.h>
#include <node_class.h>

#define L2_INPUT_NODES_PER_SOCKET 2


void l2_input_early_init(void);

struct l2_input_main{
	int16_t node_allocation_indicator; 
};

int register_l2_input_node(int socket_id);
int unregister_l2_input_node(const char * node_name);
void l2_input_runtime_init(void);


#endif