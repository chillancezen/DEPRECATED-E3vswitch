#include <vip-resource.h>
#include <rte_malloc.h>
#include <init.h>

struct virtual_ip * gvip_array[MAX_VIP_NR];
struct findex_2_2_base * ip2vip_base;


void vip_resource_early_init(void)
{
	
	ip2vip_base=allocate_findex_2_2_base();
	E3_ASSERT(ip2vip_base);
	E3_LOG("vip fast index table established\n");
}

E3_init(vip_resource_early_init,TASK_PRIORITY_LOW);


int register_virtual_ip(struct virtual_ip *vip)
{
	int idx,rc;
	struct findex_2_2_key fkey;
	/*check whether the nodeis already in the array*/
	for(idx=0;idx<MAX_VIP_NR;idx++){
		if(gvip_array[idx]==vip)
			return -1;
	}
	/*check whether target ip is already in the array*/
	for(idx=0;idx<MAX_VIP_NR;idx++){
		if(!gvip_array[idx])
			continue;
		if(gvip_array[idx]->ip_as_u32==vip->ip_as_u32)
			return -2;
	}
	/*find a new slot*/
	idx=0;
	for(;(idx<MAX_VIP_NR)&&(gvip_array[idx]);idx++);
	if(idx>=MAX_VIP_NR)
		return -3;
	vip->local_index=idx;
	rcu_assign_pointer(gvip_array[idx],vip);

	/*setup fast index entry*/
	fkey.key_as_u32=vip->ip_as_u32;
	fkey.value_as_u64=vip->local_index;
	rc=add_index_2_2_item_unsafe(ip2vip_base,&fkey);
	if(rc){
		E3_ERROR("error occurs during setuping the fast index entry\n");
		rcu_assign_pointer(gvip_array[idx],NULL);
		return -4;
	}
	E3_LOG("register virtual ip:%d.%d.%d.%d with l3 interface:%d\n",vip->ip_as_u8[0]
		,vip->ip_as_u8[1]
		,vip->ip_as_u8[2]
		,vip->ip_as_u8[3],
		vip->virt_if_index);
	return 0;
}


void unregister_virtual_ip(struct virtual_ip *vip)
{
	struct findex_2_2_key fkey;
	int idx=0;
	if(!vip)
		return ;
	fkey.key_as_u32=vip->ip_as_u32;
	for(;(idx<MAX_VIP_NR)&&(gvip_array[idx]!=vip);idx++);
	
	if(idx<MAX_VIP_NR){
		delete_index_2_2_item_unsafe(ip2vip_base,&fkey);
		rcu_assign_pointer(gvip_array[idx],NULL);
	}
	
	if(vip->vip_reclaim_function)
		call_rcu(&vip->rcu,vip->vip_reclaim_function);
}

struct virtual_ip* allocate_virtual_ip(void)
{
	struct virtual_ip * vip=NULL;
	
	vip=rte_zmalloc(NULL,sizeof(struct virtual_ip),64);
	if(vip){
		vip->vip_reclaim_function=default_virtual_ip_reclaim_fun;
	}
	return vip;
}
void default_virtual_ip_reclaim_fun(struct rcu_head * rcu)
{
	struct virtual_ip * vip=container_of(rcu,struct virtual_ip,rcu);
	rte_free(vip);
}

void dump_virtual_ips(FILE* fp)
{
	int idx=0;
	struct virtual_ip * vip;
	for(idx=0;idx<MAX_VIP_NR;idx++){
		vip=find_virtual_ip_at_index(idx);
		if(!vip)
			continue;
		
		fprintf(fp,"%d %d.%d.%d.%d\n",vip->local_index,
			vip->ip_as_u8[0],
			vip->ip_as_u8[1],
			vip->ip_as_u8[2],
			vip->ip_as_u8[3]);
	}
}