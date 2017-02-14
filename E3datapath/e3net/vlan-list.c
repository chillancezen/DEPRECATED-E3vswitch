#include <vlan-list.h>


struct vlan_list_header vlan_list[MAX_VLAN_RANGE];





__attribute__((constructor)) void vlan_list_module_init(void)
{
	memset(vlan_list,0x0,sizeof(vlan_list));
}
 
