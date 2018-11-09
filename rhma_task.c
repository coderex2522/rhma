#include "rhma.h"
#include "rhma_log.h"
#include "rhma_config.h"
#include "rhma_context.h"
#include "rhma_transport.h"
#include "rhma_task.h"

struct rhma_task* rhma_recv_task_create ( struct rhma_transport* rdma_trans, int size )
{
	struct rhma_task *recv_task;

	recv_task=malloc(sizeof(struct rhma_task));
	if(!recv_task){
		ERROR_LOG("alloc memory error.");
		return NULL;
	}

	recv_task->type=RHMA_TASK_INIT;
	recv_task->sge.addr=rdma_trans->recv_region+rdma_trans->recv_region_used*RHMA_TASK_SIZE;
	recv_task->sge.length=RHMA_TASK_SIZE;
	recv_task->sge.lkey=rdma_trans->recv_region_mr->lkey;
	recv_task->rdma_trans=rdma_trans;

	rdma_trans->recv_region_used=(rdma_trans->recv_region_used+1)%4;

	return recv_task;
}

struct rhma_task* rhma_send_task_create ( struct rhma_transport* rdma_trans, struct rhma_msg *msg )
{
	struct rhma_task *send_task;

	send_task=malloc(sizeof(struct rhma_task));
	if(!send_task){
		ERROR_LOG("alloc memory error.");
		return NULL;
	}

	send_task->type=RHMA_TASK_INIT;
	send_task->sge.addr=rdma_trans->send_region+rdma_trans->send_region_used*RHMA_TASK_SIZE;
	send_task->sge.length=sizeof(enum rhma_msg_type)+sizeof(int)+msg->data_size;
	send_task->sge.lkey=rdma_trans->send_region_mr->lkey;
	send_task->rdma_trans=rdma_trans;
	rdma_trans->send_region_used=(rdma_trans->send_region_used+1)%4;
	
	/*use msg build send task*/
	memcpy(send_task->sge.addr, &msg->msg_type, sizeof(enum rhma_msg_type));
	memcpy(send_task->sge.addr+sizeof(enum rhma_msg_type), &msg->data_size, sizeof(int));
	memcpy(send_task->sge.addr+sizeof(enum rhma_msg_type)+sizeof(int), msg->data, msg->data_size);
	
	return send_task;

}


