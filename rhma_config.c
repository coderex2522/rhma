#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "rhma.h"
#include "rhma_log.h"
#include "rhma_config.h"


#define RHMA_NODES_INFO_STR "nodes_info"
#define RHMA_NODE_STR "node"
#define RHMA_ID_STR "id"
#define RHMA_ADDR_STR "addr"
#define RHMA_PORT_STR "port"
#define RHMA_IB_DEVICE "ens33"//ens33

static void rhma_print_nets_info(struct rhma_config *global_config)
{
	int i;
	for(i=0;i<global_config->nets_cnt;i++){
		INFO_LOG("addr %s",global_config->net_infos[i].addr);
		INFO_LOG("port %d",global_config->net_infos[i].port);
	}
}

static int rhma_parse_node(struct rhma_config *config, int index, xmlDocPtr doc, xmlNodePtr curnode)
{
	xmlChar *val;

	curnode=curnode->xmlChildrenNode;
	while(curnode!=NULL){
		if(!xmlStrcmp(curnode->name, (const xmlChar *)RHMA_ADDR_STR)){
			val=xmlNodeListGetString(doc, curnode->xmlChildrenNode, 1);
			memcpy(config->net_infos[index].addr,
				(const void*)val,strlen((const char*)val)+1);
			xmlFree(val);
		}

		if(!xmlStrcmp(curnode->name, (const xmlChar *)RHMA_PORT_STR)){
			val=xmlNodeListGetString(doc, curnode->xmlChildrenNode, 1);
			config->net_infos[index].port=atoi((const char*)val);
			xmlFree(val);
		}

		curnode=curnode->next;
	}
	return 0;
}

static void rhma_set_curnode_id(struct rhma_config *config)
{
	int socketfd, i, dev_num;
	char buf[BUFSIZ];
	const char *addr;
	struct ifconf conf;
	struct ifreq *ifr;
	struct sockaddr_in *sin;
	

	socketfd = socket(PF_INET, SOCK_DGRAM, 0);
	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buf;

	ioctl(socketfd, SIOCGIFCONF, &conf);
	dev_num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;
	
	for(i=0;i < dev_num;i++)
	{
    	sin = (struct sockaddr_in *)(&ifr->ifr_addr);

    	ioctl(socketfd, SIOCGIFFLAGS, ifr);
    	if(((ifr->ifr_flags & IFF_LOOPBACK) == 0) && (ifr->ifr_flags & IFF_UP)&&(strcmp(ifr->ifr_name,RHMA_IB_DEVICE)==0))
    	{
        	INFO_LOG("%s %s",
            	ifr->ifr_name,
            	inet_ntoa(sin->sin_addr));
			addr=inet_ntoa(sin->sin_addr);
			break;
    	}
    	ifr++;
	}
	

	if(i!=dev_num){
		for(i=0;i<config->nets_cnt;i++){
			if(!strcmp(config->net_infos[i].addr, addr)){
				config->curnet_id=i;
				INFO_LOG("curnet id %d",i);
				return ;
			}
		}
	}
	
	ERROR_LOG("don't have rdma device or config.xml exist error.");
	exit(-1);		
}

int rhma_config_init(struct rhma_config *config)
{
	const char *config_file=RHMA_CONFIG_FILE;
	int index=0;
	xmlDocPtr config_doc;
	xmlNodePtr curnode;
	
	config_doc=xmlParseFile(config_file);
	if(!config_doc){
		ERROR_LOG("xml parse file error.");
		return -1;
	}

	curnode=xmlDocGetRootElement(config_doc);
	if(!curnode){
		ERROR_LOG("xml doc get root element error.");
		goto cleandoc;
	}

	if(xmlStrcmp(curnode->name, (const xmlChar *)RHMA_NODES_INFO_STR)){
		ERROR_LOG("xml root node is not nodes_info.");
		goto cleandoc;
	}
	
	config->nets_cnt=0;
	config->curnet_id=-1;
	curnode=curnode->xmlChildrenNode;
	while(curnode){
		if(!xmlStrcmp(curnode->name, (const xmlChar *)RHMA_NODE_STR)){
			rhma_parse_node(config, index, config_doc, curnode);
			config->nets_cnt++;
			index++;
		}
		curnode=curnode->next;
	}
	
	rhma_print_nets_info(config);
	xmlFreeDoc(config_doc);
	rhma_set_curnode_id(config);
	return 0;
	
cleandoc:
	xmlFreeDoc(config_doc);
	return -1;
}

