#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>


#include "rhma.h"
#include "rhma_log.h"
#include "rhma_config.h"
#include "rhma_context.h"
#include "rhma_transport.h"
#include "rhma_task.h"
#include "rhma_node.h"

extern struct rhma_node curnode;

static int rhma_device_init ( struct ibv_context* verbs,struct rhma_device* dev )
{
	int retval=0;

	retval=ibv_query_device ( verbs, &dev->device_attr );
	if ( retval<0 )
	{
		ERROR_LOG ( "rdma query device attr error." );
		goto out;
	}

	dev->pd=ibv_alloc_pd ( verbs );
	if ( !dev->pd )
	{
		ERROR_LOG ( "Allocate ibv_pd error." );
		retval=-1;
		goto out;
	}

	dev->verbs=verbs;
	return retval;

out:
	dev->verbs=NULL;
	return retval;
}


int rhma_trans_init ( struct rhma_device* dev )
{
	struct ibv_context** ctx_list;
	const char* dev_name;
	int i,num_devices=0,err=0;

	ctx_list=rdma_get_devices ( &num_devices );
	if ( !ctx_list )
	{
		ERROR_LOG ( "Failed to get the rdma device list." );
		err=-1;
		goto out;
	}

	for ( i=0; i<num_devices; i++ )
	{
		if ( !ctx_list[i] )
		{
			ERROR_LOG ( "RDMA device [%d] is NULL.",i );
			continue;
		}
		dev_name=ibv_get_device_name ( ctx_list[i]->device );
		if ( strcmp ( dev_name,"siw_lo" ) ==0 )
		{
			continue;
		}

		rhma_device_init ( ctx_list[i],dev );
		if ( !dev->verbs )
		{
			ERROR_LOG ( "RDMA device [%d]: name= %s allocate error.",
			            i, ibv_get_device_name ( ctx_list[i]->device ) );
			continue;
		}
		else
			INFO_LOG ( "RDMA device [%d]: name= %s allocate success.",
			           i, ibv_get_device_name ( ctx_list[i]->device ) );
	}
	rdma_free_devices ( ctx_list );
out:
	return err;
}

void rhma_trans_destroy ( struct rhma_device* dev )
{
	if ( dev->verbs )
		ibv_dealloc_pd ( dev->pd );

}


static int on_cm_addr_resolved ( struct rdma_cm_event* event, struct rhma_transport* rdma_trans )
{
	int retval=0;

	retval=rdma_resolve_route ( rdma_trans->cm_id, ROUTE_RESOLVE_TIMEOUT );
	if ( retval )
	{
		ERROR_LOG ( "RDMA resolve route error." );
		return retval;
	}

	return retval;
}

const char* rhma_ibv_wc_opcode_str ( enum ibv_wc_opcode opcode )
{
	switch ( opcode )
	{
		case IBV_WC_SEND:
			return "IBV_WC_SEND";
		case IBV_WC_RDMA_WRITE:
			return "IBV_WC_RDMA_WRITE";
		case IBV_WC_RDMA_READ:
			return "IBV_WC_RDMA_READ";
		case IBV_WC_COMP_SWAP:
			return "IBV_WC_COMP_SWAP";
		case IBV_WC_FETCH_ADD:
			return "IBV_WC_FETCH_ADD";
		case IBV_WC_BIND_MW:
			return "IBV_WC_BIND_MW";
		case IBV_WC_RECV:
			return "IBV_WC_RECV";
		case IBV_WC_RECV_RDMA_WITH_IMM:
			return "IBV_WC_RECV_RDMA_WITH_IMM";
		default:
			return "IBV_WC_UNKNOWN";
	};
}

static void rhma_malloc_event_handler ( struct rhma_transport* rdma_trans, struct rhma_msg* msg )
{
	struct rhma_mc_res_msg mc_res_msg;
	struct rhma_msg res_msg;
	void* alloc_region=NULL;
	struct ibv_mr* mr;
	struct rhma_addr *raddr;
	
	if ( msg->msg_type==RHMA_MSG_MALLOC_REQUEST )
	{
		memcpy ( &mc_res_msg.req_msg, msg->data, sizeof ( struct rhma_mc_req_msg ) );
		INFO_LOG("req size %d addr %p",mc_res_msg.req_msg.req_size,mc_res_msg.req_msg.raddr);
		alloc_region=malloc ( 32 );
		if ( !alloc_region )
		{
			ERROR_LOG ( "alloc memory error." );
			return ;
		}
		mr=ibv_reg_mr ( rdma_trans->device->pd, alloc_region,
		                32, IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_READ);
		if ( !mr )
		{
			ERROR_LOG ( "ib register memory error." );
			free ( alloc_region );
			return ;
		}
		INFO_LOG("malloc addr is %p lkey is %ld",mr->addr,mr->lkey);
		snprintf(mr->addr,32,"welcome beijing");
		memcpy ( &mc_res_msg.mr, mr, sizeof ( struct ibv_mr ) );
		res_msg.msg_type=RHMA_MSG_MALLOC_RESPONSE;
		res_msg.data_size=sizeof ( struct rhma_mc_res_msg );
		res_msg.data=&mc_res_msg;

		rhma_post_send ( rdma_trans, &res_msg );
	}
	if ( msg->msg_type==RHMA_MSG_MALLOC_RESPONSE )
	{
		memcpy(&mc_res_msg, msg->data, sizeof(struct rhma_mc_res_msg));
		raddr=mc_res_msg.req_msg.raddr;
		memcpy(&raddr->mr, &mc_res_msg.mr, sizeof(struct ibv_mr));
		raddr->addr=mc_res_msg.mr.addr;
		INFO_LOG("recv mr addr is %p lkey is %ld",raddr->mr.addr,raddr->mr.lkey);
	}
}

static void rhma_wc_success_handler ( struct ibv_wc* wc )
{
	struct rhma_transport* rdma_trans;
	struct rhma_task* task;
	struct rhma_msg msg;

	task= ( struct rhma_task* ) ( uintptr_t ) wc->wr_id;
	rdma_trans= task->rdma_trans;
	if ( wc->opcode==IBV_WC_SEND||wc->opcode==IBV_WC_RECV )
	{
		msg.msg_type=* ( enum rhma_msg_type* ) task->sge.addr;
		msg.data_size=* ( int* ) ( task->sge.addr+sizeof ( enum rhma_msg_type ) );
		msg.data=task->sge.addr+sizeof ( enum rhma_msg_type )+sizeof ( int );
	}
	if ( msg.data_size!=0 && wc->opcode==IBV_WC_RECV &&
	        ( msg.msg_type==RHMA_MSG_MALLOC_REQUEST||
	          msg.msg_type==RHMA_MSG_MALLOC_RESPONSE ) )
		rhma_malloc_event_handler ( rdma_trans, &msg );

	switch ( wc->opcode )
	{
		case IBV_WC_SEND:
			INFO_LOG ( "send content" );
			break;
		case IBV_WC_RECV:
			INFO_LOG ( "recv content" );
			break;
		case IBV_WC_RDMA_WRITE:
			break;
		case IBV_WC_RDMA_READ:
			task->type=RHMA_TASK_DONE;
			INFO_LOG ( "task read done." );
			break;
		case IBV_WC_RECV_RDMA_WITH_IMM:
			break;
		default:
			ERROR_LOG ( "unknown opcode:%s",
			            rhma_ibv_wc_opcode_str ( wc->opcode ) );
			break;
	}

}

static void rhma_wc_error_handler ( struct ibv_wc* wc )
{
	if ( wc->status==IBV_WC_WR_FLUSH_ERR )
	{
		INFO_LOG ( "work request flush error." );
	}
	else
		ERROR_LOG ( "wc status [%s] is error.",
		            ibv_wc_status_str ( wc->status ) );

}

static void rhma_cq_comp_channel_handler ( int fd, void* data )
{
	struct rhma_cq* rcq= ( struct rhma_cq* ) data;
	struct ibv_cq* cq;
	void* cq_ctx;
	struct ibv_wc wc;
	int err=0;

	err=ibv_get_cq_event ( rcq->comp_channel, &cq, &cq_ctx );
	if ( err )
	{
		ERROR_LOG ( "ibv get cq event error." );
		return ;
	}

	ibv_ack_cq_events ( rcq->cq, 1 );
	err=ibv_req_notify_cq ( rcq->cq, 0 );
	if ( err )
	{
		ERROR_LOG ( "ibv req notify cq error." );
		return ;
	}

	while ( ibv_poll_cq ( rcq->cq,1,&wc ) )
	{
		if ( wc.status==IBV_WC_SUCCESS )
		{
			rhma_wc_success_handler ( &wc );
		}
		else
		{
			rhma_wc_error_handler ( &wc );
		}

	}
}

static struct rhma_cq* rhma_cq_get ( struct rhma_device* device, struct rhma_context* ctx )
{
	struct rhma_cq* rcq;
	int retval,alloc_size,flags=0;

	rcq= ( struct rhma_cq* ) calloc ( 1,sizeof ( struct rhma_cq ) );
	if ( !rcq )
	{
		ERROR_LOG ( "allocate the memory of struct rhma_cq error." );
		return NULL;
	}

	rcq->comp_channel=ibv_create_comp_channel ( device->verbs );
	if ( !rcq->comp_channel )
	{
		ERROR_LOG ( "rdma device %p create comp channel error.",device );
		goto cleanhcq;
	}

	flags=fcntl ( rcq->comp_channel->fd, F_GETFL, 0 );
	if ( flags!=-1 )
	{
		flags=fcntl ( rcq->comp_channel->fd, F_SETFL, flags|O_NONBLOCK );
	}

	if ( flags==-1 )
	{
		ERROR_LOG ( "set hcq comp channel fd nonblock error." );
		goto cleanchannel;
	}

	rcq->ctx=ctx;
	retval=rhma_context_add_event_fd ( rcq->ctx,
	                                   EPOLLIN,
	                                   rcq->comp_channel->fd, rcq,
	                                   rhma_cq_comp_channel_handler );
	if ( retval )
	{
		ERROR_LOG ( "context add comp channel fd error." );
		goto cleanchannel;
	}

	alloc_size=min ( CQE_ALLOC_SIZE, device->device_attr.max_cqe );
	rcq->cq=ibv_create_cq ( device->verbs, alloc_size, rcq, rcq->comp_channel, 0 );
	if ( !rcq->cq )
	{
		ERROR_LOG ( "ibv create cq error." );
		goto cleaneventfd;
	}

	retval=ibv_req_notify_cq ( rcq->cq, 0 );
	if ( retval )
	{
		ERROR_LOG ( "ibv req notify cq error." );
		goto cleaneventfd;
	}

	rcq->device=device;
	return rcq;

cleaneventfd:
	rhma_context_del_event_fd ( ctx, rcq->comp_channel->fd );

cleanchannel:
	ibv_destroy_comp_channel ( rcq->comp_channel );

cleanhcq:
	free ( rcq );
	rcq=NULL;

	return rcq;
}

static int rhma_qp_create ( struct rhma_transport* rdma_trans )
{
	int retval=0;
	struct ibv_qp_init_attr qp_init_attr;
	struct rhma_cq* rcq;

	rcq=rhma_cq_get ( rdma_trans->device, rdma_trans->ctx );
	if ( !rcq )
	{
		ERROR_LOG ( "rhma cq get error." );
		return -1;
	}

	memset ( &qp_init_attr,0,sizeof ( qp_init_attr ) );
	qp_init_attr.qp_context=rdma_trans;
	qp_init_attr.qp_type=IBV_QPT_RC;
	qp_init_attr.send_cq=rcq->cq;
	qp_init_attr.recv_cq=rcq->cq;

	qp_init_attr.cap.max_send_wr=MAX_SEND_WR;
	qp_init_attr.cap.max_send_sge=min ( rdma_trans->device->device_attr.max_sge,
	                                    MAX_SEND_SGE );

	qp_init_attr.cap.max_recv_wr=MAX_RECV_WR;
	qp_init_attr.cap.max_recv_sge=1;

	retval=rdma_create_qp ( rdma_trans->cm_id,
	                        rdma_trans->device->pd,
	                        &qp_init_attr );
	if ( retval )
	{
		ERROR_LOG ( "rdma create qp error." );
		goto cleanhcq;
	}

	rdma_trans->qp=rdma_trans->cm_id->qp;
	rdma_trans->rcq=rcq;

	return retval;

cleanhcq:
	free ( rcq );
	return retval;
}

static void rhma_qp_release ( struct rhma_transport* rdma_trans )
{
	if ( rdma_trans->qp )
	{
		ibv_destroy_qp ( rdma_trans->qp );
		ibv_destroy_cq ( rdma_trans->rcq->cq );
		rhma_context_del_event_fd ( rdma_trans->ctx, rdma_trans->rcq->comp_channel->fd );
		free ( rdma_trans->rcq );
		rdma_trans->rcq=NULL;
	}
}

static int on_cm_route_resolved ( struct rdma_cm_event* event, struct rhma_transport* rdma_trans )
{
	struct rdma_conn_param conn_param;
	int i,retval=0;

	retval=rhma_qp_create ( rdma_trans );
	if ( retval )
	{
		ERROR_LOG ( "hmr qp create error." );
		return retval;
	}

	memset ( &conn_param, 0, sizeof ( conn_param ) );
	conn_param.retry_count=3;
	conn_param.rnr_retry_count=7;

	conn_param.responder_resources = 1;
	//rdma_trans->device->device_attr.max_qp_rd_atom;
	conn_param.initiator_depth = 1;
	//rdma_trans->device->device_attr.max_qp_init_rd_atom;

	retval=rdma_connect ( rdma_trans->cm_id, &conn_param );
	if ( retval )
	{
		ERROR_LOG ( "rdma connect error." );
		goto cleanqp;
	}
	for ( i=0; i<2; i++ )
	{
		rhma_post_recv ( rdma_trans );
	}
	return retval;

cleanqp:
	rhma_qp_release ( rdma_trans );
	rdma_trans->ctx->stop=1;
	rdma_trans->trans_state=RHMA_TRANSPORT_STATE_ERROR;
	return retval;
}

static int on_cm_connect_request ( struct rdma_cm_event* event, struct rhma_transport* rdma_trans )
{
	struct rhma_transport* new_trans;
	struct rdma_conn_param conn_param;
	int i,retval=0;
	char* peer_addr;

	INFO_LOG ( "event id %p rdma_trans cm_id %p event_listenid %p",event->id,rdma_trans->cm_id,event->listen_id );

	new_trans=rhma_transport_create ( rdma_trans->ctx, rdma_trans->device );
	if ( !new_trans )
	{
		ERROR_LOG ( "rdma trans process connect request error." );
		return -1;
	}

	new_trans->cm_id=event->id;
	event->id->context=new_trans;

	retval=rhma_qp_create ( new_trans );
	if ( retval )
	{
		ERROR_LOG ( "rhma qp create error." );
		goto out;
	}

	peer_addr=inet_ntoa ( event->id->route.addr.dst_sin.sin_addr );
	for ( i=0; i<curnode.config.curnet_id; i++ )
	{
		if ( strcmp ( curnode.config.net_infos[i].addr,peer_addr ) ==0 )
		{
			INFO_LOG ( "find addr trans %d %s",i,peer_addr );
			curnode.connect_trans[i]=new_trans;
			new_trans->node_id=i;
			break;
		}
	}

	memset ( &conn_param, 0, sizeof ( conn_param ) );
	retval=rdma_accept ( new_trans->cm_id, &conn_param );
	if ( retval )
	{
		ERROR_LOG ( "rdma accept error." );
		return -1;
	}
	new_trans->trans_state=RHMA_TRANSPORT_STATE_CONNECTING;

	for ( i=0; i<2; i++ )
	{
		rhma_post_recv ( new_trans );
	}

	return retval;

out:
	free ( new_trans );
	return retval;
}

static int on_cm_established ( struct rdma_cm_event* event, struct rhma_transport* rdma_trans )
{
	int retval=0;

	memcpy ( &rdma_trans->local_addr,
	         &rdma_trans->cm_id->route.addr.src_sin,
	         sizeof ( rdma_trans->local_addr ) );

	memcpy ( &rdma_trans->peer_addr,
	         &rdma_trans->cm_id->route.addr.dst_sin,
	         sizeof ( rdma_trans->peer_addr ) );

	rdma_trans->trans_state=RHMA_TRANSPORT_STATE_CONNECTED;
	return retval;
}

static void rhma_destroy_source ( struct rhma_transport* rdma_trans )
{

	if ( rdma_trans->send_region )
	{
		ibv_dereg_mr ( rdma_trans->send_region_mr );
		ibv_dereg_mr ( rdma_trans->recv_region_mr );
		free ( rdma_trans->send_region );
	}

	rdma_destroy_qp ( rdma_trans->cm_id );
	rhma_context_del_event_fd ( rdma_trans->ctx, rdma_trans->rcq->comp_channel->fd );
	rhma_context_del_event_fd ( rdma_trans->ctx, rdma_trans->event_channel->fd );
}

static int on_cm_disconnected ( struct rdma_cm_event* event, struct rhma_transport* rdma_trans )
{
	rhma_destroy_source ( rdma_trans );
	return 0;
}

static int on_cm_error ( struct rdma_cm_event* event, struct rhma_transport* rdma_trans )
{
	rhma_destroy_source ( rdma_trans );
	rdma_trans->ctx->stop=1;
	rdma_trans->trans_state=RHMA_TRANSPORT_STATE_ERROR;
	return -1;
}

static int rhma_handle_ec_event ( struct rdma_cm_event* event )
{
	int retval=0;
	struct rhma_transport* rdma_trans;
	rdma_trans= ( struct rhma_transport* ) event->id->context;

	INFO_LOG ( "cm event [%s],status:%d",
	           rdma_event_str ( event->event ),event->status );

	switch ( event->event )
	{
		case RDMA_CM_EVENT_ADDR_RESOLVED:
			retval=on_cm_addr_resolved ( event, rdma_trans );
			break;
		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			retval=on_cm_route_resolved ( event, rdma_trans );
			break;
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			retval=on_cm_connect_request ( event,rdma_trans );
			break;
		case RDMA_CM_EVENT_ESTABLISHED:
			retval=on_cm_established ( event,rdma_trans );
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
			retval=on_cm_disconnected ( event,rdma_trans );
			break;
		case RDMA_CM_EVENT_CONNECT_ERROR:
			retval=on_cm_error ( event, rdma_trans );
			break;
		default:
			retval=-1;
			break;
	};

	return retval;
}

static void rhma_event_channel_handler ( int fd, void* data )
{
	struct rdma_event_channel* ec= ( struct rdma_event_channel* ) data;
	struct rdma_cm_event* event,event_copy;
	int retval=0;

	event=NULL;
	while ( ( retval=rdma_get_cm_event ( ec,&event ) ) ==0 )
	{
		memcpy ( &event_copy,event,sizeof ( *event ) );

		/*
		 * note: rdma ack cm event will clear event content
		 * so need to copy event content into event_copy.
		 */
		rdma_ack_cm_event ( event );

		if ( rhma_handle_ec_event ( &event_copy ) )
		{
			break;
		}
	}

	if ( retval && errno!=EAGAIN )
	{
		ERROR_LOG ( "rdma get cm event error." );
	}
}

static int rhma_event_channel_create ( struct rhma_transport* rdma_trans )
{
	int flags,retval=0;

	rdma_trans->event_channel=rdma_create_event_channel();
	if ( !rdma_trans->event_channel )
	{
		ERROR_LOG ( "rdma create event channel error." );
		return -1;
	}

	flags=fcntl ( rdma_trans->event_channel->fd, F_GETFL, 0 );
	if ( flags!=-1 )
		flags=fcntl ( rdma_trans->event_channel->fd,
		              F_SETFL, flags|O_NONBLOCK );

	if ( flags==-1 )
	{
		retval=-1;
		ERROR_LOG ( "set event channel nonblock error." );
		goto cleanec;
	}

	rhma_context_add_event_fd ( rdma_trans->ctx, EPOLLIN,
	                            rdma_trans->event_channel->fd,
	                            rdma_trans->event_channel,
	                            rhma_event_channel_handler );
	return retval;

cleanec:
	rdma_destroy_event_channel ( rdma_trans->event_channel );
	return retval;
}

static int rhma_memory_register ( struct rhma_transport* rdma_trans )
{
	void* base=malloc ( SEND_REGION_SIZE+RECV_REGION_SIZE );
	if ( !base )
	{
		ERROR_LOG ( "allocate memory error." );
		goto out;
	}

	rdma_trans->send_region_used=rdma_trans->recv_region_used=0;
	rdma_trans->send_region=base;
	rdma_trans->recv_region=base+SEND_REGION_SIZE;

	rdma_trans->send_region_mr=ibv_reg_mr ( rdma_trans->device->pd, rdma_trans->send_region,
	                                        SEND_REGION_SIZE,
	                                        IBV_ACCESS_LOCAL_WRITE);
	if ( !rdma_trans->send_region_mr )
	{
		ERROR_LOG ( "rdma register memory error." );
		goto out_base;
	}

	rdma_trans->recv_region_mr=ibv_reg_mr ( rdma_trans->device->pd, rdma_trans->recv_region,
	                                        RECV_REGION_SIZE,
	                                        IBV_ACCESS_LOCAL_WRITE );
	if ( !rdma_trans->recv_region_mr )
	{
		ERROR_LOG ( "rdma register memory error." );
		goto out_send_region_mr;
	}
	INFO_LOG ( "send region mr lkey %ld rkey %ld",rdma_trans->send_region_mr->lkey,rdma_trans->send_region_mr->rkey );
	INFO_LOG ( "recv region mr lkey %ld rkey %ld",rdma_trans->recv_region_mr->lkey,rdma_trans->recv_region_mr->rkey );

	return 0;

out_send_region_mr:
	ibv_dereg_mr ( rdma_trans->send_region_mr );

out_base:
	free ( base );

out:
	return -1;
}

struct rhma_transport* rhma_transport_create ( struct rhma_context* ctx,
                                               struct rhma_device* dev )
{
	struct rhma_transport* rdma_trans;
	int err=0;

	rdma_trans= ( struct rhma_transport* ) calloc ( 1,sizeof ( struct rhma_transport ) );
	if ( !rdma_trans )
	{
		ERROR_LOG ( "allocate memory error." );
		return NULL;
	}

	rdma_trans->trans_state=RHMA_TRANSPORT_STATE_INIT;
	rdma_trans->ctx=ctx;
	rdma_trans->device=dev;

	err=rhma_event_channel_create ( rdma_trans );
	if ( err )
	{
		ERROR_LOG ( "rhma event channel create error." );
		goto out;
	}

	err=rhma_memory_register ( rdma_trans );
	if ( err )
	{
		goto out_event_channel;
	}

	return rdma_trans;

out_event_channel:
	rhma_context_del_event_fd ( rdma_trans->ctx, rdma_trans->event_channel->fd );

out:
	free ( rdma_trans );
	return NULL;

}


int rhma_transport_listen ( struct rhma_transport* rdma_trans, int listen_port )
{
	int retval=0, backlog;
	struct sockaddr_in addr;

	retval=rdma_create_id ( rdma_trans->event_channel,
	                        &rdma_trans->cm_id,
	                        rdma_trans, RDMA_PS_TCP );
	if ( retval )
	{
		ERROR_LOG ( "rdma create id error." );
		return retval;
	}

	memset ( &addr, 0, sizeof ( addr ) );
	addr.sin_family=AF_INET;
	addr.sin_port=htons ( listen_port );

	retval=rdma_bind_addr ( rdma_trans->cm_id,
	                        ( struct sockaddr* ) &addr );
	if ( retval )
	{
		ERROR_LOG ( "rdma bind addr error." );
		goto cleanid;
	}

	backlog=10;
	retval=rdma_listen ( rdma_trans->cm_id, backlog );
	if ( retval )
	{
		ERROR_LOG ( "rdma listen error." );
		goto cleanid;
	}

	rdma_trans->trans_state=RHMA_TRANSPORT_STATE_LISTEN;
	INFO_LOG ( "rdma listening on port %d",
	           ntohs ( rdma_get_src_port ( rdma_trans->cm_id ) ) );

	return retval;

cleanid:
	rdma_destroy_id ( rdma_trans->cm_id );
	rdma_trans->cm_id=NULL;

	return retval;
}


static int rhma_port_uri_init ( struct rhma_transport* rdma_trans,
                                const char* url,int port )
{
	struct sockaddr_in peer_addr;
	int retval=0;

	memset ( &peer_addr,0,sizeof ( peer_addr ) );
	peer_addr.sin_family=AF_INET;//PF_INET=AF_INET
	peer_addr.sin_port=htons ( port );

	retval=inet_pton ( AF_INET, url, &peer_addr.sin_addr );
	if ( retval<=0 )
	{
		ERROR_LOG ( "IP Transfer Error." );
		goto exit;
	}
	memcpy ( &rdma_trans->peer_addr, &peer_addr, sizeof ( struct sockaddr_in ) );
exit:
	return retval;
}

int rhma_transport_connect ( struct rhma_transport* rdma_trans,
                             const char* url, int port )
{
	int retval=0;
	if ( !url||!port )
	{
		ERROR_LOG ( "Url or port input error." );
		return -1;
	}

	retval=rhma_port_uri_init ( rdma_trans, url, port );
	if ( retval<0 )
	{
		ERROR_LOG ( "rdma init port uri error." );
		return retval;
	}

	/*rdma_cm_id dont init the rdma_cm_id's verbs*/
	retval=rdma_create_id ( rdma_trans->event_channel,
	                        &rdma_trans->cm_id, rdma_trans, RDMA_PS_TCP );
	if ( retval )
	{
		ERROR_LOG ( "rdma create id error." );
		goto clean_rdmatrans;
	}
	retval=rdma_resolve_addr ( rdma_trans->cm_id, NULL,
	                           ( struct sockaddr* ) &rdma_trans->peer_addr,
	                           ADDR_RESOLVE_TIMEOUT );
	if ( retval )
	{
		ERROR_LOG ( "RDMA Device resolve addr error." );
		goto clean_cmid;
	}
	rdma_trans->trans_state=RHMA_TRANSPORT_STATE_CONNECTING;
	return retval;

clean_cmid:
	rdma_destroy_id ( rdma_trans->cm_id );

clean_rdmatrans:
	rdma_trans->cm_id=NULL;

	return retval;
}



int rhma_post_recv ( struct rhma_transport* rdma_trans )
{
	struct ibv_recv_wr recv_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct rhma_task* recv_task;
	int err=0;

	recv_task=rhma_recv_task_create ( rdma_trans, RECV_REGION_SIZE );
	if ( !recv_task )
	{
		ERROR_LOG ( "create recv task error." );
		return -1;
	}
	recv_wr.wr_id= ( uintptr_t ) recv_task;
	recv_wr.next=NULL;
	recv_wr.sg_list=&sge;
	recv_wr.num_sge=1;

	sge.addr= ( uintptr_t ) recv_task->sge.addr;
	sge.length=recv_task->sge.length;
	sge.lkey=recv_task->sge.lkey;

	err=ibv_post_recv ( rdma_trans->qp, &recv_wr, &bad_wr );
	if ( err )
	{
		ERROR_LOG ( "ibv_post_recv error" );
		return err;
	}

	return 0;
}

int rhma_post_send ( struct rhma_transport* rdma_trans, struct rhma_msg* msg )
{
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct rhma_task* send_task;
	int err=0;

	send_task=rhma_send_task_create ( rdma_trans, msg );
	if ( !send_task )
	{
		ERROR_LOG ( "alloc send task error." );
		return -1;
	}

	memset ( &send_wr,0,sizeof ( send_wr ) );

	send_wr.wr_id= ( uintptr_t ) send_task;
	send_wr.sg_list=&sge;
	send_wr.num_sge=1;
	send_wr.opcode=IBV_WR_SEND;
	send_wr.send_flags=IBV_SEND_SIGNALED;

	sge.addr= ( uintptr_t ) send_task->sge.addr;
	sge.length=send_task->sge.length;
	sge.lkey=send_task->sge.lkey;

	err=ibv_post_send ( rdma_trans->qp, &send_wr, &bad_wr );
	if ( err )
	{
		ERROR_LOG ( "ibv_post_send error." );
	}

	return err;
}


int rhma_rdma_read ( struct rhma_transport* rdma_trans, struct ibv_mr *mr, void *local_addr, int length )
{
	struct rhma_task* read_task;
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	int err=0;

	read_task= rhma_read_task_create ( rdma_trans, length );
	if ( !read_task )
	{
		ERROR_LOG ( "create task error." );
		goto error;
	}

	memset ( &send_wr, 0, sizeof ( struct ibv_send_wr ) );

	send_wr.wr_id= ( uintptr_t ) read_task;
	send_wr.opcode=IBV_WR_RDMA_READ;
	send_wr.sg_list=&sge;
	send_wr.num_sge=1;
	send_wr.send_flags=IBV_SEND_SIGNALED;
	send_wr.wr.rdma.remote_addr= ( uintptr_t ) mr->addr;
	send_wr.wr.rdma.rkey=mr->rkey;

	sge.addr= ( uintptr_t ) read_task->sge.addr;
	sge.length=read_task->sge.length;
	sge.lkey=read_task->sge.lkey;

	err=ibv_post_send ( rdma_trans->qp, &send_wr, &bad_wr );
	if ( err )
	{
		ERROR_LOG ( "ibv_post_send error" );
		goto error;
	}

	while ( read_task->type!=RHMA_TASK_DONE );
	memcpy(local_addr, read_task->sge.addr, read_task->sge.length);
	INFO_LOG ( "read content is %s",local_addr );

	return 0;
error:
	return -1;
}

























































