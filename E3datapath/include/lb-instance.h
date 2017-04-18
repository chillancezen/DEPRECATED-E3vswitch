#ifndef LB_INSTANCE_H
#define LB_INSTANCE_H
#include <lb-common.h>
#include <e3_log.h>

#define MAX_LB_INSTANCEN_NR 256
extern struct lb_instance *glbi_array[MAX_LB_INSTANCEN_NR];
struct lb_instance * allocate_lb_instance(char * name);
int register_lb_instance(struct lb_instance * lb);
void unregister_lb_instance(struct lb_instance *lb);
void dump_lb_instances(FILE * fp);
struct lb_instance * find_lb_instance_by_name(char * name);

#define find_lb_instance_at_index(idx) ((((idx)<MAX_LB_INSTANCEN_NR)&&((idx)>=0))?\
	((struct lb_instance *)rcu_dereference(glbi_array[(idx)])): \
	NULL)

int add_real_server_num_into_lb_member_pool(struct lb_instance * lb,uint16_t rs_num);
int del_real_server_num_from_lb_member_pool(struct lb_instance * lb,uint16_t rs_num);

void dump_lb_members(struct lb_instance *lb);


#endif
