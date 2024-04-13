#ifndef LAYER_EVENTS_H
#define LAYER_EVENTS_H
#include "layer_utl.h"
#include "thread_utl.h"

#define TARGET_APP (1U << 0)
#define TARGET_BUS (1U << 1)
#define TARGET_CLI (1U << 2)
#define TARGET_GUI (1U << 3)

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
	EVENT_APP_SESSION,
	EVENT_DIAGMSG,
	EVENT_COMMAND,
	EVENT_CLIENT_REGISTER
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
	CommandType ctype;
	union{
		unsigned int i;
		float f;
		unsigned short offsets[2];
	} args[4];
	unsigned short datasize;
};

struct Command : CommandHeader
{
	constexpr static EventType type = EVENT_COMMAND;
	char data[MAX_CMDLEN - sizeof(CommandHeader)];
	Command() = default;
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
		ctype = EVENT_POLL_NULL;
		SubStr s1, s2; // todo: encode signature to string/array instead
		if(!s.Split2(s1, s2, ' '))
		s1 = s, s2 = "";
		{
			if(s1.Equals("triggerInteractionProfileChanged"))
				ctype = EVENT_POLL_TRIGGER_INTERACTION_PROFILE_CHANGED;
			else if(s1.Equals("reloadConfig"))
				ctype = EVENT_POLL_RELOAD_CONFIG;
			else if(s1.Equals("dumpAppBindings"))
				ctype = EVENT_POLL_DUMP_APP_BINDINGS;
			else if(s1.Equals("dumpLayerBindings"))
				ctype = EVENT_POLL_DUMP_LAYER_BINDINGS;
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
						ctype = EVENT_POLL_SET_EXTERNAL_SOURCE;
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
						ctype = EVENT_POLL_MAP_DIRECT_SOURCE;
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
						ctype = EVENT_POLL_MAP_AXIS;
					}
				}
				else if(s1.Equals("resetAction"))
				{
					ctype = EVENT_POLL_RESET_ACTION;
					_AddStr(s2, 0);
				}
				else if(s1.Equals("setProfile"))
				{
					ctype = EVENT_POLL_SET_PROFILE;
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
						ctype = EVENT_POLL_MAP_ACTION;
					}
				}
			}
		}
	}
	Command & operator = (const Command &other)
	{
		ctype = other.ctype;
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
	Field_() = default;
	Field_(const T &def):val(def){}
	template <typename DumperType>
	Field_(Dumper<DumperType> &l){
		char s[32] = {0};
		if constexpr(sizeof(T) == 8 && ((T)0.1 == (T)0.0))
			SBPrint(s, "%lld", (unsigned long long)l.GetData(this).val);
		else
			SBPrint(s, "%f", (double)l.GetData(this).val);
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
	StringField_() = default;
	StringField_(const SubStr &def):val(){ def.CopyTo(val);}
	template <typename DumperType>
	StringField_(Dumper<DumperType> &l){
		l.d.Dump(SubStr(NAME,nlen), SubStrB(l.GetData(this).val));
	}
};

struct AppReg
{
	constexpr static EventType type = EVENT_APP_REGISTER;
	StringField(name,32);
	Field(unsigned int, version);
	StringField(engine,32);
};

struct AppSession
{
	constexpr static EventType type = EVENT_APP_SESSION;
	Field(unsigned long long, handle);
	Field(unsigned int, state);
};

struct AppVar
{
	constexpr static EventType type = EVENT_APP_VAR;
	StringField(name,32);
	Field(float,value);
};
struct AppAction
{
	constexpr static EventType type = EVENT_APP_ACTION;
	Field(unsigned long long,handle);
	Field(unsigned long long,session);
	StringField(setName,32);
	StringField(actName,32);
};

struct AppActionSet
{
	constexpr static EventType type = EVENT_APP_ACTION_SET;
	Field(unsigned long long,handle);
	Field(unsigned long long,session);
	StringField(setName,32);
	StringField(description,32);
};
struct AppBinding
{
	constexpr static EventType type = EVENT_APP_BINDING;
	Field(unsigned long long,handle);
	StringField(path,32);
	StringField(description,32);
};
struct AppSource
{
	constexpr static EventType type = EVENT_APP_SOURCE;
	Field(unsigned long long,session);
	StringField(name,32);
	Field(int, index);
	Field(float,x);
	Field(float,y);
};
struct AppRPN
{
	constexpr static EventType type = EVENT_APP_RPN;
	Field(unsigned long long,session);
	StringField(source,64);
	Field(unsigned char,context);
	Field(int,index);
	StringField(rpn,64);
};
struct AppActionMap
{
	constexpr static EventType type = EVENT_APP_ACTION_MAP;
	Field(unsigned long long,session);
	StringField(actName,32);
	Field(int,mapIndex);
	Field(int,funcIndex);
	Field(int,actionIndex);
	Field(int,axisIndex);
	Field(int,handIndex);
};

struct AppCustomAction
{
	constexpr static EventType type = EVENT_APP_CUSTOM_ACTION;
	Field(unsigned long long,session);
	Field(int, index);
	StringField(command, 64);
	StringField(condition, 64);
	Field(float, triggerPeriod);
};

struct DiagMsg
{
	constexpr static EventType type = EVENT_DIAGMSG;
	Field(unsigned int,level);
	Field(unsigned int,code);
	StringField(message, 64);
};

struct ClientReg
{
	constexpr static EventType type = EVENT_CLIENT_REGISTER;
	Field(bool,gui);
};

struct EventHeader
{
	unsigned char target;
	EventType type;
	char displayName[14];
	pid_t targetPid, sourcePid;
};

struct EventPacket
{
	EventHeader head;
	union EventData
	{
		Command cmd;
		AppReg appReg;
		AppVar appVar;
		AppSession appSession;
		AppAction appAction;
		AppActionSet appActionSet;
		AppBinding appBinding;
		AppSource appSource;
		AppRPN appRPN;
		AppActionMap appActionMap;
		AppCustomAction appCustomAction;
		DiagMsg diagMsg;
		ClientReg clReg;
	} data;
};

// todo: polymorphic storage/array/growarray/queue?

template <typename Handler, typename T, typename... Ts>
static void HandlePacketImpl(const Handler &h, const EventPacket &p)
{
	if(p.head.type == T::type)
		h.Handle(*(T*)&p.data);
	else
		HandlePacketImpl<Handler, Ts...>(h, p);
}
template <typename Handler>static void HandlePacketImpl(const Handler &h, const EventPacket &p){}

template <typename Handler>
static void HandlePacket(const Handler &h, const EventPacket &p)
{
	HandlePacketImpl<Handler, AppReg, AppVar, AppSession, AppAction, AppActionSet, AppBinding, AppSource, AppRPN, AppActionMap, AppCustomAction, DiagMsg, ClientReg>(h, p);
}

static unsigned long long GetTimeU64()
{
	static uint64_t startTime = 0;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	if(!startTime)
		startTime = ts.tv_sec;
	ts.tv_sec -= startTime;
	return ts.tv_sec*1e9 + ts.tv_nsec;
}

#endif // LAYER_EVENTS_H
