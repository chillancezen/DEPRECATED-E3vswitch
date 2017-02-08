#ifndef _NODE_ADJACENCY_H
#define _NODE_ADJACENCY_H
#include <node.h>
#include <node_class.h>



int set_node_to_node_edge(const char * cur_node_name,int entry_index,const char * next_node_name);
int set_node_to_class_edge(const char* cur_node_name,int entry_index,const char * next_node_class);
int clean_node_next_edge(const char* cur_node_name,int entry_index);
void clean_node_next_edges(const char* cur_node_name);
void dump_node_edges(const char* cur_node_name);

#endif
