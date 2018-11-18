#ifndef RHMA_TIMERFD_H
#define RHMA_TIMERFD_H

int rhma_timerfd_create(struct itimerspec *new_value);
void rhma_test_send_recv_func(int fd, void *data);

#endif

