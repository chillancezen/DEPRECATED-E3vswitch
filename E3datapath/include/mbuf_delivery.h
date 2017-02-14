#ifndef _MBUF_DELIVERY_H
#define _MBUF_DELIVERY_H
#include <node.h>
#include <node_class.h>
#include <node_adjacency.h>

__attribute__((always_inline)) static inline int deliver_mbufs_between_nodes(uint16_t dst_node,uint16_t src_node,struct rte_mbuf **mbufs,int nr_mbufs)
{
	int nr_delivered=0;
	struct node* pnode_src=find_node_by_index(src_node);
	struct node* pnode_dst=find_node_by_index(dst_node);

	if(!pnode_src || !pnode_dst) 
		goto ret;
	nr_delivered=rte_ring_mp_enqueue_burst(pnode_dst->node_ring,(void**)mbufs,nr_mbufs);
	ret:
	return nr_delivered;
}

__attribute__((always_inline)) inline static int deliver_mbufs_by_next_entry(struct node * pnode,int next_entry,struct rte_mbuf **mbufs,int nr_mbufs)
{
	int nr_delivered=0;
	struct node * pnext=NULL;
	pnext=find_node_by_index(next_forwarding_node(pnode,next_entry));
	if(!pnext)
		goto ret;
	nr_delivered=rte_ring_mp_enqueue_burst(pnext->node_ring,(void**)mbufs,nr_mbufs);
	ret:
	return nr_delivered;

}

#define DEF_EXPRESS_DELIVERY_VARS() \
	uint64_t _latest_identifier=0;  \
	int _nr_mbufs=0; \
	int _latest_unsent_iptr=-1; \
	int _current_iptr=0; \
	int _peek_iptr=0; 
/*_latest_identifier:*/
/*1.for explicit node2node fwd,latest_identifier is next_node*/
/*2.for next_entry(including node2node edge and node2class,edge entry is set)*/
/*3....(application make it and explain it later)*/

/*_peek_iptr:next available mbuf*/
#define RESET_EXPRESS_DELIVERY_VARS() \
	_nr_mbufs=0; \
	_latest_identifier=0; \
	_latest_unsent_iptr=-1; \
	_current_iptr=0; \
	_peek_iptr=0; 



#define pre_setup_env(nr_mbufs_to_process) \
	_nr_mbufs=(nr_mbufs_to_process);

#define peek_next_mbuf() ( (_peek_iptr>=_nr_mbufs)?-1:_peek_iptr++)

#define mbufs_left_to_process() (_nr_mbufs-_peek_iptr)

#define MBUF_PROCESS_OK 0x0
#define MBUF_PROCESS_RESTART 0x1

/*iptr must be in increasing order,or things  may go wrong*/

#define proceed_mbuf(iptr,fwd_id)({\
	int _ret=MBUF_PROCESS_OK; \
	if(_latest_unsent_iptr<0){ \
		_latest_unsent_iptr=(iptr); \
		_current_iptr=(iptr); \
		_latest_identifier=(fwd_id);\
	}else if((fwd_id)==_latest_identifier) \
		_current_iptr=(iptr); \
	else \
		_ret=MBUF_PROCESS_RESTART; \
	_ret; \
})

#define flush_pending_mbuf() {\
	_latest_unsent_iptr=-1; \
}

#define fetch_pending_index(start,end) {\
	(start)=_latest_unsent_iptr; \
	(end)=(_latest_unsent_iptr<0)?-2:_current_iptr; \
}


#endif
