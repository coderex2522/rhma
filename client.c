#include <stdio.h>
#include "rhma.h"


int main(int argc,char *argv[])
{
	struct rhma_addr raddr;
	char *str=malloc(32);
	if(!str){
		printf("alloc mem error");
		return -1;
	}
	snprintf(str,32,"hello world");
	rhma_init();
	raddr=rhma_malloc(32);
	if(raddr.addr!=NULL){
		rhma_read(raddr, str, 32);
	}
	rhma_destroy();
	return 0;
}

