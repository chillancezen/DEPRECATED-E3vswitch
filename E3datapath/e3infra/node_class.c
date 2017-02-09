
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
	struct node_class * pclass=NULL;
	FOREACH_NODE_CLASS_START(pclass){
		fprintf(fp,"node class :%d %s\n",
			pclass->node_class_index,
			pclass->class_name);
	}
	FOREACH_NODE_CLASS_END();
}

__attribute__((constructor)) void node_class_module_init(void)
{
	int idx=0;
	for(idx=0;idx<MAX_NR_NODE_CLASSES;idx++)
		gnode_class_array[idx]=NULL;
	
}

int _add_node_into_nodeclass(struct node_class *pclass,struct node *pnode)
{
	struct node_entry entry;
	int iptr=0;
	/*check whether node already in the class*/
	FOREACH_NODE_ENTRY_IN_CLASS_START(pclass,entry){
		if((entry.node_index==pnode->node_index)&&validate_node_entry(entry))
			return -1;
	}
	FOREACH_NODE_ENTRY_IN_CLASS_END();
	
	FOREACH_NODE_ENTRY_IN_CLASS_START(pclass,entry){
		if(!validate_node_entry(entry))
			break;
		iptr++;
	}
	FOREACH_NODE_ENTRY_IN_CLASS_END();
	if(iptr>=MAX_NODE_IN_CLASS)
		return -2;
	entry.is_valid=1;
	entry.node_attached_cnt=0;
	entry.node_index=pnode->node_index;
	rcu_assign_pointer(pclass->node_entries[iptr].entries_as_ptr,entry.entries_as_ptr);
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
	int iptr=0;
	struct node_entry entry;
	FOREACH_NODE_ENTRY_IN_CLASS_START(pclass,entry){
		if(entry.node_index==pnode->node_index)
			break;
		iptr++;
	}
	FOREACH_NODE_ENTRY_IN_CLASS_END();
	if(iptr>=MAX_NODE_IN_CLASS)
		return -1;
	entry.is_valid=0;
	entry.node_attached_cnt=0;
	entry.node_index=0;
	rcu_assign_pointer(pclass->node_entries[iptr].entries_as_ptr,entry.entries_as_ptr);
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

