#include <fast-index.h>
#include <rte_malloc.h>
#include <e3_log.h>
struct findex_2_2_base * allocate_findex_2_2_base(void)
{
	struct findex_2_2_base * base=NULL;
	base=rte_zmalloc(NULL,sizeof(struct findex_2_2_base)*(1<<16),64);
	return base;
}
void _deallocate_findex_2_2_base(struct findex_2_2_base * base)
{
	if(base)
		rte_free(base);
	
}
struct findex_2_2_entry * allocate_findex_2_2_entry(void)
{
	struct findex_2_2_entry  * entry=NULL;
	entry=rte_zmalloc(NULL,sizeof(struct findex_2_2_entry),64);
	if(entry)
		{e3_bitmap_init(entry->tags_avail);}
	else 
		{E3_WARN("out of memory\n");}
	return entry;
}

void _deallocate_findex_2_2_entry(struct findex_2_2_entry * entry)
{
	if(entry)
		rte_free(entry);
}

void findex_2_2_entry_rcu_callback(struct rcu_head * rcu)
{
	struct findex_2_2_entry * entry=container_of(rcu,struct findex_2_2_entry,rcu);
	_deallocate_findex_2_2_entry(entry);
}
void dump_findex_2_2_base(struct findex_2_2_base * base)
{	
	int idx=0;
	struct findex_2_2_entry * pentry;
	int local_index;
	for(idx=0;idx<(1<<16);idx++){
		if(base[idx].next){
			printf("%d:",idx);
			for(pentry=base[idx].next;pentry;pentry=pentry->next_entry){
				e3_bitmap_foreach_set_bit_start(pentry->tags_avail,local_index){
					printf("(%d,0x%x) ",local_index,pentry->tags[local_index]);
				}
				e3_bitmap_foreach_set_bit_end();
			}
			printf("\n");
		}
	}
}
int add_index_2_2_item_unsafe(struct findex_2_2_base * base,struct findex_2_2_key * key)
{
	int ret=-1;
	int local_index=0;
	struct findex_2_2_entry * pentry;
	struct findex_2_2_entry * ptmp_entry;
	/*find out whether the target key already in the list*/
	for(pentry=base[key->key_index].next;
		pentry;
		pentry=pentry->next_entry){
		e3_bitmap_foreach_set_bit_start(pentry->tags_avail,local_index){
			if(pentry->tags[local_index]==key->key_tag){
				pentry->value[local_index]=key->value_as_u64;
				_mm_sfence();/*make this modification globally visible*/
				return 0;
			}
		}
		e3_bitmap_foreach_set_bit_end();
	}
	/*find a new slot to insert new key*/
	for(pentry=base[key->key_index].next;
		pentry;
		pentry=pentry->next_entry){
			local_index=e3_bitmap_first_zero_bit_to_index(pentry->tags_avail);
			if((local_index>=0)&&(local_index<FINDEX_2_2_TAGS_LENGTH)){
				pentry->tags[local_index]=key->key_tag;
				pentry->value[local_index]=key->value_as_u64;
				e3_bitmap_set_bit(pentry->tags_avail,local_index);
				_mm_sfence();
				return 0;
			}
	}
	/*no available room for new item*/
	ptmp_entry=allocate_findex_2_2_entry();
	if(!ptmp_entry)
		return -1;
	ptmp_entry->tags[0]=key->key_tag;
	ptmp_entry->value[0]=key->value_as_u64;
	e3_bitmap_set_bit(ptmp_entry->tags_avail,0);
	__sync_synchronize();

	if(!base[key->key_index].next){
		ptmp_entry->next_entry=NULL;
		rcu_assign_pointer(base[key->key_index].next,ptmp_entry);
	}else{
		ptmp_entry->next_entry=NULL;
		for(pentry=base[key->key_index].next;pentry->next_entry;pentry=pentry->next_entry);
		rcu_assign_pointer(pentry->next_entry,ptmp_entry);
	}
	_mm_sfence();
	ret=0;
	return ret;
}

void delete_index_2_2_item_unsafe(struct findex_2_2_base* base,struct findex_2_2_key * key)
{
	struct findex_2_2_entry * pentry,*ptmp;
	int local_index;
	for(pentry=base[key->key_index].next;pentry;pentry=pentry->next_entry){
		e3_bitmap_foreach_set_bit_start(pentry->tags_avail,local_index){
			if(pentry->tags[local_index]==key->key_tag){
				e3_bitmap_clear_bit(pentry->tags_avail,local_index);
				pentry->tags[local_index]=0x0;
				_mm_sfence();
				if(!pentry->tags_avail){/*delete this entry from the list*/
					if(base[key->key_index].next==pentry)
						rcu_assign_pointer(base[key->key_index].next,pentry->next_entry);
					else{
						for(ptmp=base[key->key_index].next;
							ptmp&&(ptmp->next_entry!=pentry);
							ptmp=ptmp->next_entry);
						rcu_assign_pointer(ptmp->next_entry,pentry->next_entry);
					}
					call_rcu(&pentry->rcu,findex_2_2_entry_rcu_callback);
				}
				return;
			}
		}
		e3_bitmap_foreach_set_bit_end();
	}
}


struct findex_2_1_6_base *allocate_findex_2_1_6_base(void)
{
	struct findex_2_1_6_base* base=
		rte_zmalloc(NULL,sizeof(struct findex_2_1_6_base)*(1<<16),64);
	return base;
}
struct findex_2_1_6_entry * allocate_findex_2_1_6_entry(void)
{
	struct findex_2_1_6_entry * entry=
		rte_zmalloc(NULL,sizeof(struct findex_2_1_6_entry),64);
	if(entry)
		{e3_bitmap_init(entry->tag_avail);}
	else
		{E3_WARN("out of memory");}
	return entry;
	
}