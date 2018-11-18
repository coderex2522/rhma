.PHONY:clean

CC :=gcc
CFLAGS := -Wall -g
LDFLAGS	:= ${LDFLAGS} -lxml2 -lrdmacm -libverbs -lpthread

APPS := client server

all:${APPS}

rhma_log.o:rhma_log.c
	gcc -Wall -g	-c -o $@ $^

murmur3_hash.o:murmur3_hash.c
	gcc -Wall -g	-c -o $@ $^
	
rhma_hash.o:rhma_hash.c
	gcc -Wall -g	-c -o $@ $^
	
rhma_config.o:rhma_config.c
	gcc -Wall -g	-c -o $@ $^ -I /usr/local/include/libxml2/ -lxml2

rhma_context.o:rhma_context.c
	gcc -Wall -g	-c -o $@ $^ -lpthread

rhma_transport.o:rhma_transport.c
	gcc -Wall -g	-c -o $@ $^ ${LDFLAGS}

rhma_timerfd.o:rhma_timerfd.c 
	gcc -Wall -g	-c -o $@ $^
	
rhma_node.o:rhma_node.c 
	gcc -Wall -g	-c -o $@ $^ 

rhma_task.o:rhma_task.c
	gcc -Wall -g	-c -o $@ $^

rhma_mem.o:rhma_mem.c 
	gcc -Wall -g	-c -o $@ $^ 
	
client:rhma_log.o murmur3_hash.o rhma_hash.o rhma_config.o rhma_context.o rhma_transport.o rhma_timerfd.o rhma_task.o rhma_node.o rhma_mem.o client.o
	gcc -Wall -g $^ -o $@  ${LDFLAGS}

server:rhma_log.o murmur3_hash.o rhma_hash.o rhma_config.o rhma_context.o rhma_transport.o rhma_timerfd.o rhma_task.o rhma_node.o rhma_mem.o server.o
	gcc -Wall -g $^ -o $@  ${LDFLAGS}
	
clean:
	rm -rf *.o