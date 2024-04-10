#ifndef LAYER_EVENTS_H
#define LAYER_EVENTS_H
#include "layer_utl.h"
#include "thread_utl.h"

enum TargetFlags : unsigned char
{
	TARGET_APP = 1U << 0,
	TARGET_BUS = 1U << 1,
	TARGET_CLI = 1U << 2,
	TARGET_GUI = 1U << 3
};

enum EventType : unsigned char
{
	EVENT_APP_REGISTER,
	EVENT_APP_VAR,
	EVENT_APP_ACTION,
	EVENT_APP_ACTION_SET,
	EVENT_APP_SOURCE,
	EVENT_APP_PROFILE,
	EVENT_APP_ACTION_MAP,
	EVENT_APP_CUSTOM_ACTION,
	EVENT_APP_BINDING,
	EVENT_APP_RPN,
	EVENT_DIAGMSG,
	EVENT_COMMAND
};

enum CommandType{
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

#define MAX_CMDLEN 256

struct CommandHeader
{
	CommandType type;
	union{
		unsigned int i;
		float f;
		unsigned short offsets[2];
	} args[4];
	unsigned short datasize;
};

struct Command : CommandHeader
{
	char data[MAX_CMDLEN - sizeof(CommandHeader)];
	Command(){}
	void _AddStr(const SubStr &s, int arg)
	{
		int len = s.Len();
		if(len > sizeof(data) - datasize)
			len = sizeof(data) - datasize;
		if(len)
			memcpy(&data[datasize], s.begin, len);
		args[arg].offsets[0] = datasize;
		datasize += len;
		args[arg].offsets[1] = datasize;
	}
	Command(const SubStr &s)
	{
		datasize = 0;
		type = EVENT_POLL_NULL;
		SubStr s1, s2; // todo: encode signature to string/array instead
		if(!s.Split2(s1, s2, ' '))
		s1 = s, s2 = "";
		{
			if(s1.Equals("triggerInteractionProfileChanged"))
				type = EVENT_POLL_TRIGGER_INTERACTION_PROFILE_CHANGED;
			else if(s1.Equals("reloadConfig"))
				type = EVENT_POLL_RELOAD_CONFIG;
			else if(s1.Equals("dumpAppBindings"))
				type = EVENT_POLL_DUMP_APP_BINDINGS;
			else if(s1.Equals("dumpLayerBindings"))
				type = EVENT_POLL_DUMP_LAYER_BINDINGS;
			else
			{
				if(!s2.Len())
					return;
				if(s1.Equals("setExternalSource"))
				{
					SubStr s3, s4, s5, s6;
					if(s2.Split2(s3, s4, ' ') && s4.Split2(s5, s6, ' '))
					{
						_AddStr(s3, 0);
						args[1].i = atoi(s5.begin);
						args[2].f = atof(s6.begin);
						type = EVENT_POLL_SET_EXTERNAL_SOURCE;
					}
				}
				else if(s1.Equals("mapDirectSource"))
				{
					SubStr s3, s4, s5, s6;
					if(s2.Split2(s3, s4, ' ') && s4.Split2(s5, s6, ' '))
					{
						_AddStr(s3, 0);
						args[1].i = atoi(s5.begin);
						_AddStr(s6, 2);
						type = EVENT_POLL_MAP_DIRECT_SOURCE;
					}
				}
				else if(s1.Equals("mapAxis"))
				{
					SubStr s3, s4, s5, s6, s7, s8;
					if(s2.Split2(s3, s4, ' ') && s4.Split2(s5, s6, ' ') && s6.Split2(s7, s8, ' '))
					{
						_AddStr(s3, 0);
						args[1].i = atoi(s5.begin);
						_AddStr(s7, 2);
						args[3].i = atoi(s8.begin);
						type = EVENT_POLL_MAP_AXIS;
					}
				}
				else if(s1.Equals("resetAction"))
				{
					type = EVENT_POLL_RESET_ACTION;
					_AddStr(s2, 0);
				}
				else if(s1.Equals("setProfile"))
				{
					type = EVENT_POLL_SET_PROFILE;
					_AddStr(s2, 0);
				}
				else if(s1.Equals("mapAction"))
				{
					SubStr s3, s4, s5, s6;
					if(s2.Split2(s3, s4, ' ') && s4.Split2(s5, s6, ' '))

					{
						_AddStr(s3, 0);
						_AddStr(s5, 1);
						args[2].i = atoi(s6.begin);
						type = EVENT_POLL_MAP_ACTION;
					}
				}
			}
		}
	}
	Command & operator = (const Command &other)
	{
		type = other.type;
		memcpy(args, other.args, sizeof(other.args));
		datasize = other.datasize;
		memcpy(data, other.data, datasize);
		return *this;
	}
	SubStr Str(int index) const
	{
		return {&data[args[index].offsets[0]], &data[args[index].offsets[1]]};
	}
};

#define Field(type,name) \
constexpr static const char fld_name_##name[] = #name; \
	Field_<type, fld_name_##name, sizeof(fld_name_##name) - 1> name

template <typename T, const auto &NAME, size_t nlen>
struct Field_
{
	constexpr static const char *name = NAME;
	T val;
	operator T() const
	{
		return val;
	}
	Field_(const Field_ &other) = delete;
	Field_& operator=(const Field_ &) = delete;
	Field_(){}
	Field_(const T &def):val(def){}
	template <typename DumperType>
	Field_(Dumper<DumperType> &l){
		char s[32];
		SBPrint(s, "%f", (float)l.GetData(this).val);
		l.d.Dump(SubStr(NAME,nlen), SubStrB(s));
	}
};

#define StringField(name, len) \
constexpr static const char fld_name_##name[] = #name; \
	StringField_<fld_name_##name, sizeof(fld_name_##name) - 1, len> name

template <const auto &NAME, size_t nlen, size_t len>
struct StringField_
{
	constexpr static const char *name = NAME;
	char val[len];
	StringField_(const StringField_ &other) = delete;
	StringField_& operator=(const StringField_ &) = delete;
	StringField_(){}
	StringField_(const SubStr &def):val(){ def.CopyTo(val);}
	template <typename DumperType>
	StringField_(Dumper<DumperType> &l){
		l.d.Dump(SubStr(NAME,nlen), SubStrB(l.GetData(this).val));
	}
};

struct EventHeader
{
	TargetFlags target;
	EventType type;
	char displayName[14];
	pid_t targetPid, sourcePid;
	union EventData
	{
		Command cmd;
		struct AppRegister
		{
			Field(pid_t,pid);
		} appReg;
		struct AppVar
		{
			StringField(name,32);
			Field(float,value);
		} appVar;
		struct AppAction
		{
			Field(unsigned long long,handle);
			StringField(setName,32);
			StringField(actName,32);
		} appAct;
		struct AppBinding
		{
			Field(unsigned long long,handle);
			StringField(path,32);
			StringField(description,32);
		} appBinding;
		struct AppSource
		{
			StringField(name,32);
			Field(int, index);
			Field(float,x);
			Field(float,y);
		} appSource;
		struct AppRPN
		{
			StringField(source,64);
			Field(unsigned char,context);
			Field(int,index);
			StringField(rpn,64);
		} appRPN;
		struct AppActionMap
		{
			StringField(actName,32);
			Field(int,mapIndex);
			Field(int,funcIndex);
			Field(int,actionIndex);
			Field(int,axisIndex);
			Field(int,handIndex);
		} appActionMap;

		struct AppCustomAction
		{
			Field(int, index);
			StringField(command, 64);
			StringField(condition, 64);
			Field(float, triggerPeriod);
		} appCustomAction;

		struct DiagMsg
		{
			Field(unsigned int,level);
			Field(unsigned int,code);
			StringField(message, 64);
		} diagMsg;
	} data;
};

struct EventPoller
{
	Thread pollerThread;
	EventPoller() : pollerThread([](void *poller){
			EventPoller *p = (EventPoller*)poller;
			p->Run();
		},this) {}
	CycleQueue<Command> pollEvents;
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
				buf[pos] = 0;
				SubStr s = SubStr(buf, pos);
				pos = 0;
				Command c = Command(s);
				pollEvents.Enqueue(c);
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
