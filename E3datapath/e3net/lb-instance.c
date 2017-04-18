#include <lb-instance.h>
#include <string.h>
#include <rte_malloc.h>

struct lb_instance *glbi_array[MAX_LB_INSTANCEN_NR];

void default_lb_instance_rcu_reclaim_func(struct rcu_head * rcu)
{
	struct lb_instance * lb=container_of(rcu,struct lb_instance ,rcu);
	rte_free(lb);
}
struct lb_instance * allocate_lb_instance(char * name)
{
	struct lb_instance * lb=rte_zmalloc(NULL,sizeof(struct lb_instance),64);
	if(lb){
		lb->lb_instance_reclaim_func=default_lb_instance_rcu_reclaim_func;
		strcpy((char*)lb->name,name);
		lb->nr_real_servers=0;
		int idx=0;
		for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
			lb->real_servers[idx]=0x7fff;
			lb->indirection_table[idx]=0x7fff;
		}
		
	}
	return lb;
}
int register_lb_instance(struct lb_instance * lb)
{
	int idx=0;
	for(idx=0;idx<MAX_LB_INSTANCEN_NR;idx++)
		if(glbi_array[idx]==lb)
			return -1;

	for(idx=0;idx<MAX_LB_INSTANCEN_NR;idx++){
		if(!glbi_array[idx])
			continue;
		if(!strcmp((char*)glbi_array[idx]->name,(char*)lb->name))
			return -2;
	}
	idx=0;
	for(;(idx<MAX_LB_INSTANCEN_NR)&&(glbi_array[idx]);idx++);
	if(idx>=MAX_LB_INSTANCEN_NR)
		return -3;

	lb->vip_index=0xffff;
	lb->local_index=idx;
	lb->nr_real_servers=0;
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++)
		lb->indirection_table[idx]=0xffff;
	rcu_assign_pointer(glbi_array[lb->local_index],lb);
	E3_LOG("registering lb instance:%s succeeds\n",(char*)lb->name);
	return 0;
}

void unregister_lb_instance(struct lb_instance *lb)
{
	int idx=0;
	if(!lb)
		return ;
	for(idx=0;(idx<MAX_LB_INSTANCEN_NR)&&(glbi_array[idx]!=lb);idx++);
	if(idx<MAX_LB_INSTANCEN_NR){
		rcu_assign_pointer(glbi_array[idx],NULL);
	}
	if(lb->lb_instance_reclaim_func)
		call_rcu(&lb->rcu,lb->lb_instance_reclaim_func);
}

struct lb_instance * find_lb_instance_by_name(char * name)
{
	int idx=0;
	for(idx=0;idx<MAX_LB_INSTANCEN_NR;idx++){
		if(!glbi_array[idx])
			continue;
		if(!strcmp((char*)glbi_array[idx]->name,name))
			return  glbi_array[idx];
	}
	return NULL;
}

void dump_lb_instances(FILE * fp)
{
	int idx=0;
	for(idx=0;idx<MAX_LB_INSTANCEN_NR;idx++){
		if(!glbi_array[idx])
			continue;
		fprintf(fp,"(%d %s)\n",idx,(char*)glbi_array[idx]->name);
	}
}
/*del and add ops will automatically balance the layout*/
int del_real_server_num_from_lb_member_pool(struct lb_instance * lb,uint16_t rs_num)
{
	int slot_remainder;
	int itmp;
	int iborrow=0;
	int rs_iptr;
	int idx=0;
	int nr_to_borrow[MAX_MEMBER_LENGTH];
	int nr_tmp[MAX_MEMBER_LENGTH];
	
	for(idx=0;idx<lb->nr_real_servers;idx++){
		if(lb->real_servers[idx]==rs_num)
			break;
	}
	if(idx==lb->nr_real_servers)
		return -1;
	rs_iptr=idx;
	if(lb->nr_real_servers==1){
		for(idx=0;idx<MAX_MEMBER_LENGTH;idx++)
			lb->indirection_table[idx]=0x7fff;
		lb->nr_real_servers=0;
		return 0;
	}
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
		nr_tmp[idx]=0;
		nr_to_borrow[idx]=0;
	}
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
		for(itmp=0;itmp<lb->nr_real_servers;itmp++){
			if(lb->indirection_table[idx]==lb->real_servers[itmp]){
				nr_tmp[itmp]++;
				break;
			}
		}
	}
	slot_remainder=nr_tmp[rs_iptr];
	while(slot_remainder>0){
		itmp=-1;
		for(idx=0;idx<lb->nr_real_servers;idx++){
			if(idx==rs_iptr)
				continue;
			if(itmp==-1)
				itmp=idx;
			else if((nr_tmp[idx]+nr_to_borrow[idx])<(nr_tmp[itmp]+nr_to_borrow[itmp]))
				itmp=idx;
		}
		E3_ASSERT(itmp>=0);
		nr_to_borrow[itmp]++;
		slot_remainder--;
	}
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
		if(lb->indirection_table[idx]!=lb->real_servers[rs_iptr])
			continue;
		
		iborrow=-1;
		for(itmp=0;itmp<lb->nr_real_servers;itmp++){
			if(!nr_to_borrow[itmp])
				continue;
			if(iborrow==-1)
				iborrow=itmp;
			else if(nr_to_borrow[itmp]>nr_to_borrow[iborrow])
				iborrow=itmp;
		}
		E3_ASSERT(iborrow!=-1);
		
		lb->indirection_table[idx]=lb->real_servers[iborrow];
		nr_to_borrow[iborrow]--;
	}
	lb->real_servers[rs_iptr]=lb->real_servers[lb->nr_real_servers-1];
	lb->nr_real_servers--;
	return 0;
}

int add_real_server_num_into_lb_member_pool(struct lb_instance * lb,uint16_t rs_num)
{/*add an element into the indirection table,1/(n+1) 
	[where n is the current number of nodes] 
	part of nodes will be influenced ,when n increases ,the facter will be lowered*/

	int itmp;
	int idx=0;
	int slots_average;
	int nr_preempted=0;
	int nr_tmp[MAX_MEMBER_LENGTH];
	if(lb->nr_real_servers>=MAX_MEMBER_LENGTH)
		return -1;
	
	/*construct initial layout*/
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++)
		nr_tmp[idx]=0;
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
		for(itmp=0;itmp<lb->nr_real_servers;itmp++){
			if(lb->indirection_table[idx]==lb->real_servers[itmp]){
				nr_tmp[itmp]++;
				break;
			}
		}
	}
	
	slots_average=MAX_MEMBER_LENGTH/(lb->nr_real_servers+1);
	
	for(idx=0;idx<lb->nr_real_servers;idx++)
		nr_tmp[idx]-=slots_average;
	
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
		if(nr_preempted>=slots_average)
			break;
		itmp=0;
		for(;itmp<lb->nr_real_servers;itmp++)
			if(lb->indirection_table[idx]==lb->real_servers[itmp])
				break;
		if(itmp==lb->nr_real_servers){
			lb->indirection_table[idx]=rs_num;
			nr_preempted++;
		}else{
			if(nr_tmp[itmp]>0){
				lb->indirection_table[idx]=rs_num;
				nr_tmp[itmp]--;
				nr_preempted++;
			}
		}
	}
	lb->real_servers[lb->nr_real_servers]=rs_num;
	lb->nr_real_servers++;
	return 0;
}

void dump_lb_members(struct lb_instance *lb)
{
	int idx=0;
	int itmp;
	int nr_tmp[MAX_MEMBER_LENGTH];
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++)
		nr_tmp[idx]=0;
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
		for(itmp=0;itmp<lb->nr_real_servers;itmp++){
			if(lb->indirection_table[idx]==lb->real_servers[itmp]){
				nr_tmp[itmp]++;
				break;
			}
		}
	}
	printf("lb name:%s\n",(char*)lb->name);
	printf("lb number of members:%d\nmembers are:",(int)lb->nr_real_servers);
	for(idx=0;idx<lb->nr_real_servers;idx++)
		printf("%d(%d),",lb->real_servers[idx],nr_tmp[idx]);
	printf("\nindirection tables:\n");
	for(idx=0;idx<MAX_MEMBER_LENGTH;idx++){
		printf("%d ",lb->indirection_table[idx]);
		if(!((idx+1)%8))
			puts("");
	}
}
