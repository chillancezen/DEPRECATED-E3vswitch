#ifndef DEVICE_WRAPPER_H
#define DEVICE_WRAPPER_H

#include <device.h>
#include <mq-device.h>
/*supported nic types*/

#define NIC_VIRTUAL_DEV 0x0
#define NIC_INTEL_XL710 0x1
#define NIC_INTEL_X710 0x2
#define NIC_INTEL_82599 0x3

#define NIC_40GE 0x1
#define NIC_10GE 0x2
#define NIC_GE 0x3

#define QUEUES_OF_40GE_NIC 8
#define QUEUES_OF_10GE_NIC 2
#define QUEUES_OF_GE_NIC 1

#define L2_NEXT_EDGE_L2_PROCESS 0x0
#define L2_NEXT_EDGE_L3_PROCESS 0x1
#define L2_NEXT_EDGE_L4_TUNNEL_PROCESS 0x2
#define L2_NEXT_EDGE_EXTERNAL_INPUT 0x3

int add_e3_interface(const char *params,uint8_t nic_type,uint8_t if_type,int *pport_id);

#endif 
