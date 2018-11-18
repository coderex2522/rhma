#include "rhma.h"
#include "rhma_log.h"
#include "rhma_hash.h"
#include "rhma_config.h"
#include "rhma_context.h"
#include "rhma_transport.h"
#include "rhma_timerfd.h"


int rhma_timerfd_create(struct itimerspec *new_value)
{
	int tfd,err=0;

	tfd=timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(tfd<0){
		ERROR_LOG("create timerfd error.");
		return -1;
	}

	err=timerfd_settime(tfd, 0, new_value, NULL);
	if(err){
		ERROR_LOG("timerfd settime error.");
		return err;
	}
	
	return tfd;
}

void rhma_test_send_recv_func(int fd, void *data)
{
	uint64_t exp=0;
	//struct rhma_transport *rdma_trans;
	//struct rhma_msg msg;
	read(fd, &exp, sizeof(uint64_t));
	
	/*
	rdma_trans=(struct rhma_transport*)data;

	if(rdma_trans->node_id==0)
		return ;
	
	INFO_LOG("%d rhma_test_send_recv_func.",fd);
	msg.msg_type=RHMA_MSG_TEST;
	msg.data_size=48;
	msg.data=malloc(48);
	snprintf(msg.data,48,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh123");

	rhma_post_send(rdma_trans, &msg);*/
}

