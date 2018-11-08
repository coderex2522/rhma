#include "rhma.h"
#include "rhma_log.h"
#include "rhma_context.h"

void *rhma_context_run(void *data)
{
	struct epoll_event events[1024];
	struct rhma_event_data *event_data;
	struct rhma_context *ctx=(struct rhma_context*)data;
	int i,events_nr=0;
	INFO_LOG("rhma_context_run.");
	while(1){
		
		events_nr=epoll_wait(ctx->epfd,events,ARRAY_SIZE(events),5000);
		
		if(events_nr>0){
			for(i=0;i<events_nr;i++){
				event_data=(struct rhma_event_data*)events[i].data.ptr;
				event_data->event_handler(event_data->fd,event_data->data);
			}
		}

		if(ctx->stop)
			break;
	}
	INFO_LOG("rhma_context stop.");
	return 0;
}


int rhma_context_init(struct rhma_context *ctx)
{
	int retval=0;

	ctx->epfd=epoll_create(RHMA_EPOLL_SIZE);
	if(ctx->epfd < 0){
		ERROR_LOG("create epoll fd error.");
		return -1;
	}

	retval=pthread_create(&ctx->epoll_thread, NULL, rhma_context_run, ctx);
	if(retval){
		ERROR_LOG("pthread create error.");
		close(ctx->epfd);
	}

	return retval;
}

int rhma_context_add_event_fd(struct rhma_context *ctx,
							int events,
							int fd, void *data,
							rhma_event_handler event_handler)
{
	struct epoll_event ee;
	struct rhma_event_data *event_data;
	int retval=0;
	
	event_data=(struct rhma_event_data*)calloc(1,sizeof(struct rhma_event_data));
	if(!event_data){
		ERROR_LOG("allocate memory error.");
		return -1;
	}

	event_data->fd=fd;
	event_data->data=data;
	event_data->event_handler=event_handler;
	
	memset(&ee,0,sizeof(ee));
	ee.events=events;
	ee.data.ptr=event_data;

	retval=epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, fd, &ee);
	if(retval){
		ERROR_LOG("Context add event fd error.");
		free(event_data);
	}
	else
		INFO_LOG("rhma_context add event fd success.");
	
	return retval;
}

int rhma_context_del_event_fd(struct rhma_context *ctx, int fd)
{
	int retval=0;
	retval=epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, fd, NULL);
	if(retval<0)
		ERROR_LOG("rhma_context del event fd error.");
	
	return retval;
}

