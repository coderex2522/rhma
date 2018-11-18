#include "rhma.h"
#include "rhma_log.h"
#include "rhma_hash.h"
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
	
	recv_task->type=RHMA_TASK_RECV;
	if(rdma_trans->recv_region_used+size>=RECV_REGION_SIZE)
		rdma_trans->recv_region_used=0;
	recv_task->sge.addr=rdma_trans->recv_region+rdma_trans->recv_region_used;
	recv_task->sge.lkey=rdma_trans->recv_region_mr->lkey;
	recv_task->sge.length=size;
	recv_task->rdma_trans=rdma_trans;

	rdma_trans->recv_region_used+=size;

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

	send_task->type=RHMA_TASK_SEND;
	send_task->sge.length=sizeof(enum rhma_msg_type)+sizeof(int)+msg->data_size;
	if(rdma_trans->send_region_used+send_task->sge.length>=SEND_REGION_SIZE)
		rdma_trans->send_region_used=0;
	send_task->sge.addr=rdma_trans->send_region+rdma_trans->send_region_used;
	send_task->sge.lkey=rdma_trans->send_region_mr->lkey;
	send_task->rdma_trans=rdma_trans;
	INFO_LOG("send task sge length is %d",send_task->sge.length);
	rdma_trans->send_region_used+=send_task->sge.length;
	
	/*use msg build send task*/
	memcpy(send_task->sge.addr, &msg->msg_type, sizeof(enum rhma_msg_type));
	memcpy(send_task->sge.addr+sizeof(enum rhma_msg_type), &msg->data_size, sizeof(int));
	memcpy(send_task->sge.addr+sizeof(enum rhma_msg_type)+sizeof(int), msg->data, msg->data_size);
	
	return send_task;

}

struct rhma_task* rhma_read_task_create(struct rhma_transport * rdma_trans, int length)
{
	struct rhma_task *task;
	
	task=(struct rhma_task*)malloc(sizeof(struct rhma_task));
	if(!task){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	task->type=RHMA_TASK_READ;
	if(rdma_trans->send_region_used+length>=SEND_REGION_SIZE)
		rdma_trans->send_region_used=0;
	
	task->sge.addr=rdma_trans->send_region+rdma_trans->send_region_used;
	task->sge.length=length;
	task->sge.lkey=rdma_trans->send_region_mr->lkey;
	task->rdma_trans=rdma_trans;

	rdma_trans->send_region_used+=length;
	
	return task;
}
