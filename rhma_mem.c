#include "rhma.h"
#include "rhma_log.h"
#include "rhma_hash.h"
#include "rhma_config.h"
#include "rhma_context.h"
#include "rhma_transport.h"
#include "rhma_node.h"

extern struct rhma_node curnode;

static struct rhma_transport* rhma_node_select()
{
	int i;
	for ( i=0; i<RHMA_NODE_NUM; i++ )
	{
		if ( i!=curnode.config.curnet_id &&
		        curnode.connect_trans[i]!=NULL &&
		        curnode.connect_trans[i]->trans_state==RHMA_TRANSPORT_STATE_CONNECTED )
		{
			return curnode.connect_trans[i];
		}
	}
	return NULL;
}

struct rhma_addr rhma_malloc ( int length )
{
	struct rhma_addr raddr;
	struct rhma_transport *rdma_trans=NULL;
	struct rhma_msg msg;
	struct rhma_mc_req_msg req_msg;

	if(length<=0)
	{
		ERROR_LOG("length is error.");
		goto out;
	}

	/*选择使用哪个服务器进行malloc*/
	rdma_trans=rhma_node_select();
	if(!rdma_trans)
	{
		ERROR_LOG("don't exist remote server.");
		goto out;
	}
	raddr.node_id=rdma_trans->node_id;
	raddr.mr.length=0;
	
	/*build malloc request msg*/
	req_msg.req_size=length;
	req_msg.raddr=&raddr;
	
	msg.msg_type=RHMA_MSG_MALLOC_REQUEST;
	msg.data_size=sizeof(struct rhma_mc_req_msg);
	msg.data=&req_msg;
	
	rhma_post_send(rdma_trans, &msg);

	while(raddr.mr.length==0);
	
	INFO_LOG("raddr addr %p",raddr.addr);
	return raddr;
out:
	ERROR_LOG("alloc memory error.");
	raddr.addr=NULL;
	return raddr;
	
}

int rhma_read(struct rhma_addr raddr, void * buf, size_t count)
{
	struct rhma_transport *rdma_trans=NULL;

	rdma_trans=curnode.connect_trans[raddr.node_id];
	if(!rdma_trans||rdma_trans->trans_state!=RHMA_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("read data error.");
		return -1;
	}

	return rhma_rdma_read ( rdma_trans, &raddr.mr, buf, count);
}

void rhma_free(struct rhma_addr raddr)
{
	struct rhma_msg msg;
	struct rhma_transport *rdma_trans=NULL;

	if(raddr.addr==NULL)
	{
		ERROR_LOG("raddr address is NULL");
		return ;
	}
	
	rdma_trans=curnode.connect_trans[raddr.node_id];
	if(!rdma_trans||rdma_trans->trans_state!=RHMA_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("read data error.");
		return ;
	}
	
	/*build a msg of free addr*/
	msg.msg_type=RHMA_MSG_FREE;
	msg.data_size=sizeof(struct ibv_mr);
	msg.data=&raddr.mr;

	rhma_post_send(rdma_trans, &msg);
}


