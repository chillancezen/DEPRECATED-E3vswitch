#ifndef L4_TUNNEL_PROCESS_H
#define L4_TUNNEL_PROCESS_H
#include <lb-common.h>
#include <node.h>
#include <node_class.h>
#include <rte_malloc.h>
#include <init.h>
#include <lcore_extension.h>
#define L4_TUN_NODE_CLASS_NAME "l4-tunnel-class"
#define L4_TUN_NODES_PER_SOCKET 4
#define L4_TUN_NODES_PER_SOCKET_PUBLIC_POOL 2
#define VXLAN_UDP_PORT 0xb512 

int register_l4_tunnel_node(int socket_id,int);
int _unregister_l4_tunnel_node(struct node * pnode);
#define unregister_l4_tunnel_node(node_name)  _unregister_l4_tunnel_node(find_node_by_name(node_name))

#endif