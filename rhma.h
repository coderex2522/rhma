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

//后期需要将msg隔离到base头文件中
enum rhma_msg_type{
	RHMA_MSG_INIT,
	RHMA_MSG_NORMAL,
	RHMA_MSG_MALLOC_REQUEST,
	RHMA_MSG_MALLOC_RESPONSE
};

struct rhma_msg{
	enum rhma_msg_type msg_type;
	int  data_size;
	void *data;
};

struct rhma_addr{
	int node_id;
	void *addr;
	struct ibv_mr mr;
};

/*rhma malloc request msg*/
struct rhma_mc_req_msg{
	int req_size;
	struct rhma_addr *raddr;
};

/*rhma malloc response msg*/
struct rhma_mc_res_msg{
	struct rhma_mc_req_msg req_msg;
	struct ibv_mr mr;
};

void rhma_init();
void rhma_destroy();

struct rhma_addr rhma_malloc(int length);

int rhma_read(struct rhma_addr raddr, void * buf, size_t count);
int rhma_write(struct rhma_addr raddr, void * buf, size_t count);

void rhma_free(struct rhma_addr raddr);

#endif