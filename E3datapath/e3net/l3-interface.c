
#include <l3-interface.h>
#include <device.h>
#include <e3_log.h>
#include <rte_malloc.h>

struct l3_interface * gl3if_array[MAX_L3_INTERFACE_NR];

int register_l3_interface(struct l3_interface * l3iface)
{
	int idx=0;
	/*1.first check whether lower interface exists*/
	switch(l3iface->if_type)
	{
		case L3_INTERFACE_TYPE_PHYSICAL:
			if(!rcu_dereference(ginterface_array[l3iface->lower_if_index].if_avail_ptr)){
				E3_ERROR("lower physical interface:%d does not exist\n",l3iface->lower_if_index);
				return -1;
			}
			break;
		case L3_INTERFACE_TYPE_VIRTUAL:
			if(!rcu_dereference(gl3if_array[l3iface->lower_if_index])){
				E3_ERROR("lower virtual interface:%d does not exist\n",l3iface->lower_if_index);
				return 0;
			}
			break;
		default:
			E3_ERROR("unknown lower interface type:%d\n",l3iface->lower_if_index);
			return -1;
			break;
	}
	/*2.check wether the l3 interface is already registered*/
	for(idx=0;idx<MAX_L3_INTERFACE_NR;idx++){
		if(gl3if_array[idx]==l3iface){
			E3_ERROR("interface:%p to register is already in the global list\n",l3iface);
			return -2;
		}
	}
	/*3.combine the(ip,lower_if_type) pairs as key,and find if the key exists*/
	for(idx=0;idx<MAX_L3_INTERFACE_NR;idx++){
		if(!gl3if_array[idx])
			continue;
		if((gl3if_array[idx]->if_type==l3iface->if_type)&&
		   (gl3if_array[idx]->lower_if_index==l3iface->lower_if_index)&&
		   (gl3if_array[idx]->if_ip_as_u32==l3iface->if_ip_as_u32)){
		   E3_WARN("interface:(%s %x on%d) should not be registered more than once\n",(l3iface->if_type==L3_INTERFACE_TYPE_VIRTUAL)?"virtual":"physical",l3iface->if_ip_as_u32,l3iface->lower_if_index);
		   return -3;
		}
	}
	idx=0;
	for(;(gl3if_array[idx])&&(idx<MAX_L3_INTERFACE_NR);idx++);
	if(idx>=MAX_L3_INTERFACE_NR){
		E3_ERROR("l3 interface registration fails due to MAX_L3_INTERFACE_NR=%d\n",MAX_L3_INTERFACE_NR);
		return -4;
	}
	l3iface->local_index=idx;
	rcu_assign_pointer(gl3if_array[idx],l3iface);
	E3_LOG("register L3 %s interface:%d.%d.%d.%d with lower interface index %d \n",
			(gl3if_array[idx]->if_type==L3_INTERFACE_TYPE_PHYSICAL)?
			"phy":
			"virt",
			gl3if_array[idx]->if_ip_as_u8[0],
			gl3if_array[idx]->if_ip_as_u8[1],
			gl3if_array[idx]->if_ip_as_u8[2],
			gl3if_array[idx]->if_ip_as_u8[3],
			gl3if_array[idx]->lower_if_index);
	return 0;
}
void unregister_l3_interface(struct l3_interface * l3iface)
{
	int idx,iptr;
	if(!l3iface)
		return;
	idx=0;
	for(;(gl3if_array[idx]!=l3iface)&&(idx<MAX_L3_INTERFACE_NR);idx++);
	if(idx>=MAX_L3_INTERFACE_NR)
		return;
	/*remove all l3 interfaces which depend on this l3 interfaces*/
	for(iptr=0;iptr<MAX_L3_INTERFACE_NR;iptr++){
		if(!gl3if_array[iptr])
			continue;
		if((gl3if_array[iptr]->if_type==L3_INTERFACE_TYPE_VIRTUAL)&&
		   (gl3if_array[iptr]->lower_if_index==idx))
		   unregister_l3_interface(gl3if_array[iptr]);
	}
	rcu_assign_pointer(gl3if_array[idx],NULL);
	E3_LOG("unregister L3 %s interface:%d.%d.%d.%d with lower interface index %d \n",
			(l3iface->if_type==L3_INTERFACE_TYPE_PHYSICAL)?
			"phy":
			"virt",
			l3iface->if_ip_as_u8[0],
			l3iface->if_ip_as_u8[1],
			l3iface->if_ip_as_u8[2],
			l3iface->if_ip_as_u8[3],
			l3iface->lower_if_index);
	if(l3iface->l3iface_reclaim_function)
		call_rcu(&l3iface->rcu,l3iface->l3iface_reclaim_function);
}

void dump_l3_interfaces(FILE* fp)
{
	int idx=0;
	for(idx=0;idx<MAX_L3_INTERFACE_NR;idx++){
		if(!gl3if_array[idx])
			continue;
		fprintf(fp,"%d.  %d.%d.%d.%d on %d %s\n",
			gl3if_array[idx]->local_index,
			gl3if_array[idx]->if_ip_as_u8[0],
			gl3if_array[idx]->if_ip_as_u8[1],
			gl3if_array[idx]->if_ip_as_u8[2],
			gl3if_array[idx]->if_ip_as_u8[3],
			gl3if_array[idx]->lower_if_index,
			(gl3if_array[idx]->if_type==L3_INTERFACE_TYPE_PHYSICAL)?
			"phy":
			"virt");
	}
}

void l3_interface_default_rcu_reclaim_fun(struct rcu_head *rcu)
{
	struct l3_interface * l3iface=container_of(rcu,struct l3_interface ,rcu);
	rte_free(l3iface);
}
struct l3_interface* allocate_l3_interface(void)
{
	struct l3_interface * l3iface=rte_zmalloc(NULL,sizeof(struct l3_interface),64);
	if(l3iface){
		l3iface->l3iface_reclaim_function=l3_interface_default_rcu_reclaim_fun;
	}
	return l3iface;
}
