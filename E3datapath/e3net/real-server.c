#include <real-server.h>
#include <init.h>
#include <util.h>
#include <rte_malloc.h>

struct real_server * grs_array[MAX_REAL_SERVER_NR];
struct findex_2_1_6_base * rs_base;

void real_server_early_init(void)
{
	int idx=0;
	rs_base=allocate_findex_2_1_6_base();
	E3_ASSERT(rs_base);
	for(idx=0;idx<MAX_REAL_SERVER_NR;idx++)
		grs_array[idx]=NULL;
}
E3_init(real_server_early_init,TASK_PRIORITY_LOW);


void real_server_rcu_reclaim_func(struct rcu_head * rcu)
{
	struct real_server *rs=container_of(rcu,struct real_server,rcu);
	rte_free(rs);
}
struct real_server * allocate_real_server(void)
{
	struct real_server * rs=NULL;
	rs=rte_zmalloc(NULL,sizeof(struct real_server),64);
	if(rs)
		rs->real_server_rcu_reclaim_func=real_server_rcu_reclaim_func;
	return rs;
}


int register_real_server(struct real_server * rs)
{
	int idx=0;
	int rc;
	struct findex_2_1_6_key  key;
	/*1.check whether this real_server is already registered*/
	for(idx=0;idx<MAX_REAL_SERVER_NR;idx++){
		if(grs_array[idx]==rs)
			return -1;
	}
	
	/*2.check whether the key exists*/
	for(idx=0;idx<MAX_REAL_SERVER_NR;idx++){
		if(!grs_array[idx])
			continue;
		if((grs_array[idx]->tunnel_id==rs->tunnel_id)&&
			(grs_array[idx]->rs_mac[0]==rs->rs_mac[0])&&
			(grs_array[idx]->rs_mac[1]==rs->rs_mac[1])&&
			(grs_array[idx]->rs_mac[2]==rs->rs_mac[2])&&
			(grs_array[idx]->rs_mac[3]==rs->rs_mac[3])&&
			(grs_array[idx]->rs_mac[4]==rs->rs_mac[4])&&
			(grs_array[idx]->rs_mac[5]==rs->rs_mac[5]))
			return -2;
	}
	
	/*3.find a new slot*/
	idx=0;
	for(;(idx<MAX_REAL_SERVER_NR)&&grs_array[idx];idx++);
	if(idx>=MAX_REAL_SERVER_NR)
		return -3;
	rs->local_index=idx;
	rcu_assign_pointer(grs_array[idx],rs);
	/*3.setup fast index item*/
	make_findex_2_1_6_key(rs->tunnel_id,rs->rs_mac,&key);
	key.value_as_u64=rs->local_index;
	rc=add_index_2_1_6_item_unsafe(rs_base,&key);
	if(rc){
		E3_ERROR("error occurs during setting up the fast index entry\n");
		rcu_assign_pointer(grs_array[idx],NULL);
		return -4;
	}
	E3_LOG("register %s real-server:%02x:%02x:%02x:%02x:%02x:%02x with local index:%d and lb-side l3 iface:%d\n",
                (rs->rs_network_type==RS_NETWORK_TYPE_VLAN)?
                "vlan":
                ((rs->rs_network_type==RS_NETWORK_TYPE_VXLAN)?
                        "vxlan":
                        "unknown"),
                rs->rs_mac[0],
                rs->rs_mac[1],
                rs->rs_mac[2],
                rs->rs_mac[3],
                rs->rs_mac[4],
                rs->rs_mac[5],
                rs->local_index,
                rs->lb_iface);	
	return 0;
}


void dump_real_servers(FILE * fp)
{
	int idx=0;
	for(idx=0;idx<MAX_REAL_SERVER_NR;idx++){
		if(!grs_array[idx])
			continue;
		fprintf(fp,"%d :(%x,%02x:%02x:%02x:%02x:%02x:%02x)\n",idx,
			grs_array[idx]->tunnel_id,
			grs_array[idx]->rs_mac[0],
			grs_array[idx]->rs_mac[1],
			grs_array[idx]->rs_mac[2],
			grs_array[idx]->rs_mac[3],
			grs_array[idx]->rs_mac[4],
			grs_array[idx]->rs_mac[5]);
	}
}

void unregister_real_server(struct real_server * rs)
{
	struct findex_2_1_6_key key;
	int idx=0;
	if(!rs)
		return ;
	for(idx=0;idx<MAX_REAL_SERVER_NR&&(grs_array[idx]!=rs);idx++);
	if(idx<MAX_REAL_SERVER_NR){
		make_findex_2_1_6_key(rs->tunnel_id,rs->rs_mac,&key);
		delete_index_2_1_6_item_unsafe(rs_base,&key);
		rcu_assign_pointer(grs_array[idx],NULL);
	}
	if(rs->real_server_rcu_reclaim_func)
		call_rcu(&rs->rcu,rs->real_server_rcu_reclaim_func);
	
}
