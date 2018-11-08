#ifndef RHMA_TRANSPORT_H
#define RHMA_TRANSPORT_H

#define ADDR_RESOLVE_TIMEOUT 500
#define ROUTE_RESOLVE_TIMEOUT 500

/*set the max send/recv wr num*/
#define MAX_SEND_WR 128
#define MAX_RECV_WR 128
#define MAX_SEND_SGE 4

/*call ibv_create_cq alloc size--second parameter*/
#define CQE_ALLOC_SIZE 20

#define RECV_REGION_SIZE 256
#define SEND_REGION_SIZE 256

#define min(a,b) (a>b?b:a)

enum rhma_transport_state {
	RHMA_TRANSPORT_STATE_INIT,
	RHMA_TRANSPORT_STATE_LISTEN,
	RHMA_TRANSPORT_STATE_CONNECTING,
	RHMA_TRANSPORT_STATE_CONNECTED,
	RHMA_TRANSPORT_STATE_DISCONNECTED,
	RHMA_TRANSPORT_STATE_RECONNECT,
	RHMA_TRANSPORT_STATE_CLOSED,
	RHMA_TRANSPORT_STATE_DESTROYED,
	RHMA_TRANSPORT_STATE_ERROR
};
	
struct rhma_device{
	struct ibv_context	*verbs;
	struct ibv_pd	*pd;
	struct ibv_device_attr device_attr;
};

struct rhma_cq{
	struct ibv_cq	*cq;
	struct ibv_comp_channel	*comp_channel;
	struct rhma_device *device;
	
	/*add the fd of comp_channel into the ctx*/
	struct rhma_context *ctx;
};

struct rhma_transport{
	struct sockaddr_in	peer_addr;
	struct sockaddr_in	local_addr;
	
	int node_id;
	enum rhma_transport_state trans_state;
	struct rhma_context *ctx;
	struct rhma_device *device;
	struct rhma_cq *rcq;
	struct ibv_qp	*qp;
	struct rdma_event_channel *event_channel;
	struct rdma_cm_id	*cm_id;

	void *recv_region;
	int recv_region_used;
	struct ibv_mr *recv_region_mr;

	void *send_region;
	int send_region_used;
	struct ibv_mr *send_region_mr;
};

int rhma_trans_init(struct rhma_device *dev);
void rhma_trans_destroy(struct rhma_device *dev);

struct rhma_transport *rhma_transport_create(struct rhma_context *ctx,
													struct rhma_device *dev);

int rhma_transport_listen(struct rhma_transport *rdma_trans, int listen_port);
int rhma_transport_connect(struct rhma_transport *rdma_trans, const char *url, int port);

int rhma_post_recv ( struct rhma_transport* rdma_trans );

int rhma_post_send(struct rhma_transport *rdma_trans, struct rhma_msg *msg);
#endif