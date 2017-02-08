#ifndef _L2_INPUT_H
#define _L2_INPUT_H
#include <node.h>
#include <node_class.h>

void l2_input_early_init(void);

struct l2_input_main{
	int16_t node_allocation_indicator; 
};

int register_l2_input_node(int socket_id);

#endif