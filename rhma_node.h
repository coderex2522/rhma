#ifndef RHMA_NODE_H
#define RHMA_NODE_H

struct rhma_node{
	struct rhma_config config;
	struct rhma_context ctx;
	struct rhma_device dev;

	struct rhma_transport *listen_trans;
	struct rhma_transport *connect_trans[RHMA_NODE_NUM];

	struct list_head used_mr_list;
	struct list_head free_mr_list;
};

#endif