#ifndef _L2_LEARN_H
#define _L2_LEARN_H

#define L2_COMMAND_ADD_FIB 0x1
#define L2_COMMAND_RECLAIM_FIB 0x2

#include <node.h>
#include <node_class.h>
#include <node_adjacency.h>
#include <lcore_extension.h>
#include <l2fib.h>


extern struct node* pnode_l2_learn;

int l2_learn_early_init(void);
#endif
