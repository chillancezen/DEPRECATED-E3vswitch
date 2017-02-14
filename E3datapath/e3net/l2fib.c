#include <l2fib.h>
#include <rte_config.h>
#include <rte_malloc.h>


struct l2fib_header *l2fib_list=NULL;


void l2fib_early_init(void)
{
	l2fib_list=rte_zmalloc(NULL,sizeof(struct l2fib_header)*L2FIB_LIST_SIZE,64);
	E3_ASSERT(l2fib_list);
	E3_LOG("l2fib table base initialized\n");
}
struct l2fib_entry * allocate_l2fib_entry(void)
{
	struct l2fib_entry *entry=rte_zmalloc(NULL,sizeof(struct l2fib_entry),64);
	if(entry)
		l2fib_entry_init(entry);
	return entry;
}

void l2fib_entry_reclaim_func(struct rcu_head *rcu)
{
	struct l2fib_entry * entry=container_of(rcu,struct l2fib_entry,rcu);
	rte_free(entry);
}

void reclaim_l2fib_entry(struct l2fib_entry *entry)
{
	
}
