#ifndef RHMA_H
#define RHMA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <linux/list.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#define RHMA_NODE_NUM 2

#ifndef bool
#define bool char
#define true 1
#define false 0
#endif
//void *rhma_malloc(int length);
//后期需要将msg隔离到base头文件中
enum rhma_msg_type{
	RHMA_MSG_INIT,
	RHMA_MSG_NORMAL
};
	
struct rhma_msg{
	enum rhma_msg_type msg_type;
	int  data_size;
	void *data;
};

void rhma_init();
void rhma_destroy();

void* rhma_malloc(int length);

void rhma_free(void *addr, int length);

#endif