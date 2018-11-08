#ifndef RHMA_CONTEXT_H
#define RHMA_CONTEXT_H

#define RHMA_EPOLL_SIZE 1024

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

typedef void (*rhma_event_handler)(int fd, void *data);

struct rhma_event_data{
	int fd;
	void *data;
	rhma_event_handler event_handler;
};

struct rhma_context{
	int epfd;
	int stop;
	pthread_t epoll_thread;
};

int rhma_context_init(struct rhma_context *ctx);

int rhma_context_add_event_fd(struct rhma_context *ctx,
							int events,
							int fd, void *data,
							rhma_event_handler event_handler);

int rhma_context_del_event_fd(struct rhma_context *ctx, int fd);

#endif

