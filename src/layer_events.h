#ifndef LAYER_EVENTS_H
#define LAYER_EVENTS_H
#include "layer_utl.h"
#include "thread_utl.h"

enum EventType{
	EVENT_POLL_NULL,
	EVENT_POLL_TRIGGER_INTERACTION_PROFILE_CHANGED,
	EVENT_POLL_RELOAD_CONFIG,
	EVENT_POLL_SET_EXTERNAL_SOURCE,
	EVENT_POLL_MAP_DIRECT_SOURCE,
	EVENT_POLL_MAP_AXIS,
	EVENT_POLL_MAP_ACTION,
	EVENT_POLL_RESET_ACTION,
	EVENT_POLL_SET_PROFILE,
	EVENT_POLL_DUMP_APP_BINDINGS,
	EVENT_POLL_DUMP_LAYER_BINDINGS,
};

struct PollEvent{
	EventType type;
	int i1;
	float f1;
	char str1[32];
	char str2[32];
};

struct EventPoller
{
	Thread pollerThread;
	EventPoller() : pollerThread([](void *poller){
			EventPoller *p = (EventPoller*)poller;
			p->Run();
		},this) {}
	CycleQueue<PollEvent> pollEvents;
	SpinLock pollLock;
	int fd = -1;
	bool InitSocket()
	{
		fd = open("/tmp/command_pipe", O_RDONLY);
		return fd >= 0;
	}
	void Run()
	{
		char buf[256];
		int pos = 0;
		while( read(fd, &buf[pos], 1) > 0 )
		{
			if(!Running)
				return;
			if(pos >= 255)
				break;
			if(buf[pos] == '\n')
			{
				PollEvent ev = {EVENT_POLL_NULL};
				char *cmd = buf;
				buf[pos] = 0;
				pos = 0;
				char *i1s = strchr(buf, ' ');
				if(!i1s)
					continue;
				*i1s++ = 0;
				ev.type = (EventType)atoi(cmd);
				char *f1s = strchr(i1s, ' ');
				if(!f1s)
					continue;
				*f1s++ = 0;
				ev.i1 = atoi(i1s);
				char *str1 = strchr(f1s, ' ');
				if(!str1)
					continue;
				*str1++ = 0;
				ev.f1 = atof(f1s);
				char *str2 = strchr(str1, ' ');
				if(!str2)
					continue;
				*str2++ = 0;
				strncpy(ev.str1, str1, sizeof(ev.str1) - 1);
				strncpy(ev.str2, str2, sizeof(ev.str2) - 1);
				{
					Lock l{pollLock};
					pollEvents.Enqueue(ev);
				}

			}
			else pos++;
		}
	}
	volatile bool Running = false;
	void Start(int port)
	{
		if(!InitSocket())
			return;
		Running = true;
		SyncBarrier();
		pollerThread.Start();
	}
	void Stop()
	{
		Running = false;
		SyncBarrier();
		pollerThread.RequestStop();
	}
};

#endif // LAYER_EVENTS_H
