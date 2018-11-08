#ifndef RHMA_CONFIG_H
#define RHMA_CONFIG_H

#define RHMA_CONFIG_FILE "config.xml"
#define RHMA_ADDR_LEN 18

struct rhma_net_info{
	char addr[RHMA_ADDR_LEN];
	int port;
};

struct rhma_config{
	struct rhma_net_info net_infos[RHMA_NODE_NUM];
	int curnet_id;//store net infos index;
	int nets_cnt;
};

int rhma_config_init(struct rhma_config *config);
#endif

