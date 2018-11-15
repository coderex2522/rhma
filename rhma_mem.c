#include "rhma.h"
#include "rhma_log.h"
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

	raddr.addr=NULL;
	if(length<=0)
	{
		ERROR_LOG("length is error.");
		goto out;
	}
		
	rdma_trans=rhma_node_select();
	if(!rdma_trans)
	{
		INFO_LOG("don't exist remote server.");
		goto out;
	}
	raddr.node_id=rdma_trans->node_id;
	
	/*build malloc request msg*/
	req_msg.req_size=length;
	req_msg.raddr=&raddr;
	INFO_LOG("raddr %p", &raddr);
	
	msg.msg_type=RHMA_MSG_MALLOC_REQUEST;
	msg.data_size=sizeof(struct rhma_mc_req_msg);
	msg.data=&req_msg;
	
	rhma_post_send(rdma_trans, &msg);

	while(raddr.addr==NULL);
	
	INFO_LOG("raddr addr %p",raddr.addr);
	return raddr;
out:
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

