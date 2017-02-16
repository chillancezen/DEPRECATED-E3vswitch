#ifndef _VLAN_LIST_H
#define _VLAN_LIST_H
#include <device.h>
#include <e3_log.h>
#include <util.h>
#include <urcu-qsbr.h>
#define MAX_VLAN_RANGE 4096
/*record interface list which belong to the same vlan,
this list will ease multi-destination forwarding*/

struct vlan_list_header{
	struct E3interface * next;
};
extern struct vlan_list_header vlan_list[MAX_VLAN_RANGE];

/*please makesure vlan_id is legal*/
#define is_interface_in_vlan_list(iface,vlan_id) ({\
	struct E3interface *_pif=rcu_dereference(vlan_list[vlan_id].next); \
	while(_pif) { \
		if(_pif->port_id==iface) \
			break; \
		_pif=rcu_dereference(_pif->vlan_if_next); \
	} \
	!!_pif; \
})

/*caller must make sure vlan is not already in the list*/
#define _add_interface_into_vlan_list(iface,vlan_id) {\
	struct E3interface *_pif_target=interface_at_index(iface); \
	_pif_target->vlan_if_next=vlan_list[(vlan_id)].next; \
	rcu_assign_pointer(vlan_list[(vlan_id)].next,_pif_target);\
}

/*this routine does sanity check before genuinely adding the interface into list*/
#define add_interface_into_vlan_list(iface,vlan_id)({\
	struct E3interface *_pif_target=interface_at_index(iface); \
	int success=-1; \
	for(;;){ \
		if(is_interface_in_vlan_list((iface),(vlan_id))) \
			break; \
		if(is_interface_in_vlan_list((iface),_pif_target->access_vlan)) \
			break; \
		_pif_target->access_vlan=(vlan_id); \
		_add_interface_into_vlan_list((iface),(vlan_id)); \
		success=0; \
		break; \
	}\
	success;\
})

#define delete_interface_from_vlan_list(iface,vlan_id) ({\
	struct E3interface *_pif_target=interface_at_index(iface); \
	struct E3interface *_pif=NULL; \
	if(vlan_list[(vlan_id)].next==_pif_target) \
		rcu_assign_pointer(vlan_list[(vlan_id)].next,_pif_target->vlan_if_next); \
	else if(vlan_list[(vlan_id)].next) { \
		_pif=vlan_list[(vlan_id)].next; \
		while(_pif->vlan_if_next&&(_pif->vlan_if_next!=_pif_target)) \
			_pif=_pif->vlan_if_next; \
		if(_pif->vlan_if_next) \
			rcu_assign_pointer(_pif->vlan_if_next,_pif_target->vlan_if_next); \
	}\
})

#define foreach_interface_in_vlan_safe(pif,vlan_id) \
	for((pif)=rcu_dereference(vlan_list[(vlan_id)].next); \
	(pif); \
	(pif)=rcu_dereference(((pif)->vlan_if_next))) 

#define set_interface_vlan(iface,vlan_id) ({ \
	delete_interface_from_vlan_list((iface),interface_at_index((iface))->access_vlan); \
	add_interface_into_vlan_list((iface),(vlan_id)); \
})
/*default vlan is 0,all port initially is set to vlan 0*/

#define reset_interface_vlan(iface) \
	set_interface_vlan((iface),0)
#define clean_interface_vlan(iface) \
	delete_interface_from_vlan_list((iface),interface_at_index((iface))->access_vlan) 
#endif