#include "rhma.h"
#include "rhma_log.h"
#include "rhma_config.h"
#include "rhma_context.h"
#include "rhma_transport.h"
#include "rhma_node.h"
 
struct rhma_node curnode;

void rhma_init()
{
	int i, err=0;
	rhma_config_init(&curnode.config);
	rhma_context_init(&curnode.ctx);
	rhma_trans_init(&curnode.dev);
	
	curnode.listen_trans=rhma_transport_create(&curnode.ctx,&curnode.dev);
	if(!curnode.listen_trans){
		ERROR_LOG("create listen trans error.");
		goto error;
	}

	err=rhma_transport_listen(curnode.listen_trans,
						curnode.config.net_infos[curnode.config.curnet_id].port);
	if(err)
		goto error;

	memset(curnode.connect_trans, 0, RHMA_NODE_NUM*sizeof(struct rhma_transport*));
	for(i=curnode.config.curnet_id+1;i<curnode.config.nets_cnt;i++){
		INFO_LOG("create the [%d]-th transport.",i);
		curnode.connect_trans[i]=rhma_transport_create(&curnode.ctx, &curnode.dev);
		if(!curnode.connect_trans[i]){
			ERROR_LOG("create rdma trans error.");
			continue;
		}
		curnode.connect_trans[i]->node_id=i;
		rhma_transport_connect(curnode.connect_trans[i],
							curnode.config.net_infos[i].addr,
							curnode.config.net_infos[i].port);
	}
	
	return ;
error:
	exit(-1);
}


void rhma_destroy()
{
	INFO_LOG("rhma destroy start.");
	pthread_join(curnode.ctx.epoll_thread, NULL);
	INFO_LOG("rhma destroy end.");
/*
	sleep(8);
	INFO_LOG("rhma destroy start.");
	rhma_trans_destroy(&curnode.dev);
	curnode.ctx.stop=1;
	pthread_join(curnode.ctx.epoll_thread, NULL);
	INFO_LOG("rhma destroy end.");*/
}

