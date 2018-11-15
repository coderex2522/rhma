#ifndef RHMA_TASK_H
#define RHMA_TASK_H

#define RHMA_TASK_SIZE 128

struct rhma_sge
{
	void	*addr;
	uint32_t	length;
	uint32_t	lkey;
};

enum rhma_task_type
{
	RHMA_TASK_INIT,
	RHMA_TASK_SEND,
	RHMA_TASK_RECV,
	RHMA_TASK_READ,
	RHMA_TASK_WRITE,
	RHMA_TASK_DONE
};

struct rhma_task
{
	enum rhma_task_type type;
	struct rhma_sge sge;
	struct rhma_transport* rdma_trans;
};

struct rhma_task* rhma_recv_task_create ( struct rhma_transport* rdma_trans, int size );
struct rhma_task* rhma_send_task_create ( struct rhma_transport* rdma_trans, struct rhma_msg *msg );

struct rhma_task* rhma_read_task_create ( struct rhma_transport* rdma_trans, int length);

#endif

