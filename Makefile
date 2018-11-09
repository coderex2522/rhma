.PHONY:clean

CC :=gcc
CFLAGS := -Wall -g
LDFLAGS	:= ${LDFLAGS} -lxml2 -lrdmacm -libverbs -lpthread

all:test

rhma_log.o:rhma_log.c
	gcc -Wall -g	-c -o $@ $^

rhma_config.o:rhma_config.c
	gcc -Wall -g	-c -o $@ $^ -I /usr/local/include/libxml2/ -lxml2

rhma_context.o:rhma_context.c
	gcc -Wall -g	-c -o $@ $^ -lpthread

rhma_transport.o:rhma_transport.c
	gcc -Wall -g	-c -o $@ $^ ${LDFLAGS}
	
rhma_node.o:rhma_node.c 
	gcc -Wall -g	-c -o $@ $^ 

rhma_task.o:rhma_task.c
	gcc -Wall -g	-c -o $@ $^
	
test:rhma_log.o rhma_config.o rhma_context.o rhma_transport.o rhma_task.o rhma_node.o test.o
	gcc -Wall -g $^ -o $@  ${LDFLAGS}
	rm -rf *.o
	
clean:
	rm -rf *.o
	rm -rf test