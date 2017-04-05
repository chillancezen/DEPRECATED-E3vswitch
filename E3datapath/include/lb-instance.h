#ifndef LB_INSTANCE_H
#define LB_INSTANCE_H
#include <lb-common.h>
#include <e3_log.h>

#define MAX_LB_INSTANCEN_NR 256

struct lb_instance * allocate_lb_instance(char * name);
int register_lb_instance(struct lb_instance * lb);
void unregister_lb_instance(struct lb_instance *lb);
void dump_lb_instances(FILE * fp);
struct lb_instance * find_lb_instance_by_name(char * name);





#endif
