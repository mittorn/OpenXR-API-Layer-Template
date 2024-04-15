#include <arpa/inet.h>
#include <sys/socket.h>

#include "layer_events.h"


struct EventDumper
{
	const char *tname;
	void Begin(){}
	void Dump(const SubStr &name, const SubStr &val)
	{
		Log("received %s: %s %s\n", tname, name, val);
	}
	void End(){}
};


struct HandleStdout
{
	template <typename T>
	void Handle(const T &packet) const
	{
		EventDumper d{TypeName<T>()};
		DumpNamedStruct(d, &packet);
	}
};

template <typename T>
int Send(int fd, const T &data, unsigned char target = 0xF, pid_t targetPid = 0)
{
	EventPacket p;
	p.head.sourcePid = getpid();
	SubStr("client_simple").CopyTo(p.head.displayName);
	p.head.target = target;
	p.head.targetPid = targetPid;
	p.head.type = T::type;
	memcpy(&p.data, &data, sizeof(data));
	return send(fd,&p, ((char*)&p.data - (char*)&p) + sizeof(data), 0);
}

void PrintHelp()
{
	Log("Commands:\n");
	for (int i = 1; i < sizeof(gCommands)/sizeof(gCommands[0]); i++)
		Log("- %s (%s) %s\n", gCommands[i].name, gCommands[i].sign, gCommands[i].description);
}

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		Log("Usage %s <port> [<pid>] [<command>]\n\nRunning without command enables interactive mode\n", argv[0]);
		PrintHelp();
		return 0;
	}
	unsigned short port = atoi(argv[1]);
	int pid = 0;
	if(argc > 2)
		pid = atoi(argv[2]);
	if(pid)
		argc--, argv++;
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);
	connect(fd, (sockaddr*)&addr, sizeof(addr));
	if(argc > 2)
	{
		Command c = Command(SubStrL(argv[2]),
				argc>3?SubStrL(argv[3]):SubStr{nullptr, nullptr},
				argc>4?SubStrL(argv[4]):SubStr{nullptr, nullptr},
				argc>5?SubStrL(argv[5]):SubStr{nullptr, nullptr},
				argc>6?SubStrL(argv[6]):SubStr{nullptr, nullptr});
		if(c.ctype != EVENT_POLL_NULL)
			Send(fd,c,TARGET_APP, pid);
		else
			Log("Invalid command\n");
		return c.ctype == EVENT_POLL_NULL;
	}
	ClientReg reg = {false};
	Send(fd, reg);
	EventPacket p;
	char buf[256];
	int pos = 0;
	struct timeval tv;
	fd_set rfds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO( &rfds );
	FD_SET( 0, &rfds); // stdin
	FD_SET(fd, &rfds);
	while( select( fd + 1, &rfds, NULL, NULL, NULL ) > 0 )
	{
		if(FD_ISSET(fd,&rfds) && (recv(fd, &p, sizeof(p), MSG_DONTWAIT) >= 0))
		{
			HandleStdout h;
			HandlePacket(h, p);
		}
		else if(FD_ISSET(0,&rfds) && read(0, &buf[pos], 1) == 1)
		{
			if(buf[pos] == '\n')
			{
				buf[pos] = 0;
				SubStr s = SubStr(buf, pos);
				pos = 0;
				Command c = Command(s);
				if(c.ctype != EVENT_POLL_NULL)
					Send(fd,c,TARGET_APP, pid);
				else if(s.Equals("help"))
					PrintHelp();
				else
					Log("Invalid command\n");
			}
			else pos++;
		}
		FD_ZERO( &rfds );
		FD_SET( 0, &rfds); // stdin
		FD_SET(fd, &rfds);
	}
	return 0;
}
