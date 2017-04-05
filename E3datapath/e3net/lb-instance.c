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

