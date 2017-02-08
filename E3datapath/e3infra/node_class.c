
#include <string.h>
#include <stdio.h>
#include <node.h>
#include <node_class.h>



struct node_class * gnode_class_array[MAX_NR_NODE_CLASSES];

int register_node_class(struct node_class *nclass)
{
	int idx=0;
	if(find_node_class_by_name((char*)nclass->class_name)){
		E3_ERROR("node class name:%s already exists\n",(char*)nclass->class_name);
		return -1;/*node class name can not conflict*/
	}
	
	for(idx=0;idx<MAX_NR_NODE_CLASSES;idx++)
		if(gnode_class_array[idx]==nclass)
			return -1;
	idx=0;
	for(;idx<MAX_NR_NODE_CLASSES;idx++)
		if(!gnode_class_array[idx])
			break;
	if(idx==MAX_NR_NODE_CLASSES)
		return -2;
	nclass->node_class_index=idx;
	rcu_assign_pointer(gnode_class_array[idx],nclass);
	return 0;
}

void unregister_node_class(struct node_class *nclass)
{
	int idx=0;
	for(;idx<MAX_NR_NODE_CLASSES;idx++)
		if(gnode_class_array[idx]==nclass)
			break;
	if(idx==MAX_NR_NODE_CLASSES)
		return ;
	rcu_assign_pointer(gnode_class_array[idx],NULL);
	if(nclass->class_reclaim_func)
		call_rcu(&nclass->rcu,nclass->class_reclaim_func);
}


void dump_node_class(FILE* fp)
{	
	int idx=0;
	struct node_class * pclass=NULL;
	for(idx=0;idx<MAX_NR_NODE_CLASSES;idx++){
		pclass=gnode_class_array[idx];
		if(!pclass)
			continue;
		fprintf(fp,"node class :%d %s\n",pclass->node_class_index,
			pclass->class_name);
	}
}

__attribute__((constructor)) void node_class_module_init(void)
{
	int idx=0;
	for(idx=0;idx<MAX_NR_NODE_CLASSES;idx++)
		gnode_class_array[idx]=NULL;
	
}

int _add_node_into_nodeclass(struct node_class *pclass,struct node *pnode)
{
	int idx=0;
	uint64_t current_node_nr=(uint64_t)rcu_dereference((pclass->current_node_nr_as_ptr));
	if(current_node_nr>=MAX_NODE_IN_CLASS)/*no enough room for new node*/
		return -1;
	for(idx=0;idx<current_node_nr;idx++)
		if(pclass->nodes[idx]==pnode->node_index)
			break;
	if(idx<current_node_nr)/*already in the set*/
		return -2;
	pclass->nodes[current_node_nr]=pnode->node_index;
	current_node_nr++;
	rcu_assign_pointer(pclass->current_node_nr_as_ptr,(void*)current_node_nr);
	return 0;
}
int add_node_into_nodeclass(const char* class_name,const char* node_name)
{
	struct node_class *pclass=find_node_class_by_name(class_name);
	struct node * pnode=find_node_by_name(node_name);
	if(!pclass || !pnode)
		return -1;
	return _add_node_into_nodeclass(pclass,pnode);	
}
int _delete_node_from_nodeclass(struct node_class *pclass,struct node *pnode)
{
	int idx=0;
	uint64_t current_node_nr=(uint64_t)rcu_dereference((pclass->current_node_nr_as_ptr));
	for(idx=0;idx<current_node_nr;idx++)
		if(pclass->nodes[idx]==pnode->node_index)
			break;
	if(idx==current_node_nr)/*not found in the list*/
		return -1;
	pclass->nodes[idx]=pclass->nodes[current_node_nr-1];
	current_node_nr--;
	rcu_assign_pointer(pclass->current_node_nr_as_ptr,(void*)current_node_nr);
	return 0;
	
}

int delete_node_from_nodeclass(const char* class_name,const char* node_name)
{
	struct node_class *pclass=find_node_class_by_name(class_name);
	struct node * pnode=find_node_by_name(node_name);
	if(!pclass || !pnode)
		return -1;
	return _delete_node_from_nodeclass(pclass,pnode);
}

