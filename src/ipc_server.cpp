#include <arpa/inet.h>
#include <sys/socket.h>

#include "layer_events.h"


int main(int argc, char **argv)
{
	if(argc < 2)
		return 0;
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(atoi(argv[1]));
	if(bind(fd,(sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		close(fd);
		return 1;
	}
	HashMap<unsigned short, EventHeader> clients;
	EventPacket p;
	while(true)
	{
		socklen_t slen = sizeof(addr);
		int len = recvfrom(fd,&p,sizeof(p),0, (sockaddr *)&addr, &slen);
		if(len < 0)
			return 1;
		if(len > sizeof(EventHeader))
		{
			if(p.head.target & TARGET_BUS && (p.head.type == EVENT_CLIENT_REGISTER || p.head.type ==EVENT_APP_REGISTER))
				clients[ntohs(addr.sin_port)] = p.head;
			HASHMAP_FOREACH(clients, node)
			{
				if(node->k == ntohs(addr.sin_port))
					continue;
				if(p.head.targetPid & node->v.sourcePid != p.head.targetPid)
					continue;
				if( p.head.target & TARGET_APP && node->v.type == EVENT_APP_REGISTER ||
					p.head.target & (TARGET_CLI | TARGET_GUI) && node->v.type == EVENT_CLIENT_REGISTER)
				{
					sockaddr_in addr1 = {};
					addr1.sin_family = AF_INET;
					addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
					addr1.sin_port = htons(node->k);
					sendto(fd, &p, len, 0, (sockaddr*)&addr1, sizeof(addr1));
				}
			}
		}
	}

	return 0;
}
