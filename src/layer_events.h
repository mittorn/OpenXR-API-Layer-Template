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
	EVENT_POLL_DUMP_SOURCES,
	EVENT_POLL_DUMP_VARIABLES,
	EVENT_POLL_DUMP_ACTION_MAPS,
	EVENT_POLL_DUMP_EXPRESSIONS,
	EVENT_POLL_DUMP_CUSTOM_ACTIONS,
	EVENT_POLL_DUMP_SESSION
};

constexpr static struct CommandDef
{
	SubStr name;
	SubStr sign;
	SubStr description;
} gCommands[] =
{
	{"", "", ""},
	{"triggerInteractionProfileChanged", "", "\nSimulate Interaction Profile switch.\nMay trigger app to restart session/bindings\nMay be useful to reload bindings from config"},
	{"reloadConfig", "", "\nReload configuration from file.\nResets all runtime binding changes, but keeps variables and sources"},
	{"setExternalSource","sif", "<source name> <axis> <value>\nSets value of source marked with action_external\nUseful for external scripting"},
	{"mapDirectSource","sis", "<action name> <controller index> <source mapping>[.<source_controller>]\nMap action to same-type source\nThe most cahce-friendly way"},
	{"mapAxis","siis", "<action name> <controller index> <axis index> <mapping expression>\nMap action axis to any source or calculated expression"},
	{"mapAction","sis", "<action name> <controller index> <actionmap name>\nMap action to actionmap section in config"},
	{"resetAction","s", "<action name>\nRe-apply action definition from config"},
	{"setProfile","s", "<profile name>\nApply maps and custom actions based on config profile section"},
	{"dumpAppBindings","", "\nDump action sets created by App and it's active bindings"},
	{"dumpLayerBindings","", "\nDump action sets created by Layer and it's active bindings"},
	{"dumpSources","", "\nDump active layer sources and it's values"},
	{"dumpVariables","", "\nDump variables and values"},
	{"dumpActionMaps","", "\nDump active bound actionmaps and current values"},
	{"dumpExpressions","","\nDump Expressions internal RPN representation"},
	{"dumpCustomActions","", "\nDump custom action trigger time and period"},
	{"dumpSession","", "\nDump session handle. Session state is only sent when it's changed"},
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
	void _ParseArg(int j, const SubStr &s0, char c)
	{
		if(c == 's')
			_AddStr(s0, j);
		if(c == 'i')
			args[j].i = atoi(s0.begin);
		if(c == 'f')
			args[j].f = atof(s0.begin);
	}
	Command(const SubStr &s)
	{
		datasize = 0;
		ctype = EVENT_POLL_NULL;
		SubStr s0, si, sn; // todo: encode signature to string/array instead
		if(!s.Split2(s0, si, ' '))
			s0 = s, si = {nullptr, nullptr};
		for(int i = 1; i < sizeof(gCommands) / sizeof(gCommands[0]);i++)
		{
			if(!s0.Equals(gCommands[i].name))
				continue;
			for(int j = 0; j < gCommands[i].sign.Len();j++)
			{
				if(!si.begin)
					return;
				if( ( j == gCommands[i].sign.Len() - 1 ) || !si.Split2( s0, sn, ' ' ))
					s0 = si, sn = {nullptr, nullptr};
				_ParseArg(j, s0, gCommands[i].sign.begin[j]);
				si = sn;
			}
			ctype = (CommandType)i;
			return;
		}
	}
	Command(const SubStr &cmd, const SubStr &arg0, const SubStr &arg1, const SubStr &arg2, const SubStr &arg3)
	{
		for(int i = 1; i < sizeof(gCommands) / sizeof(gCommands[0]);i++)
		{
			if(!cmd.Equals(gCommands[i].name))
				continue;
			int l = gCommands[i].sign.Len();
			if(!arg0.begin && l > 0)
				return;
			if(l > 0)
				_ParseArg(0, arg0, gCommands[i].sign.begin[0] );
			if(!arg1.begin && l > 1)
				return;
			if(l > 1)
				_ParseArg(1, arg1, gCommands[i].sign.begin[1] );
			if(!arg2.begin && l > 2)
				return;
			if(l > 2)
				_ParseArg(2, arg2, gCommands[i].sign.begin[2] );
			if(!arg3.begin && l > 3)
				return;
			if(l > 3)
				_ParseArg(3, arg3, gCommands[i].sign.begin[3] );
			ctype = (CommandType)i;
			return;
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
	//Field_(const Field_ &other) = delete;
	//Field_& operator=(const Field_ &) = delete;
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
	//StringField_(const StringField_ &other) = delete;
	//StringField_& operator=(const StringField_ &) = delete;
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
	Field(int, actionType);
	StringField(setName,32);
	StringField(actName,32);
	StringField(description,32);
	// todo: subactions info
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
	Field(int, index);
	StringField(actName, 32);
	StringField(setName,32);
	Field(unsigned long long,session);
	StringField(path,64);
	StringField(description,64);
};
struct AppSource
{
	constexpr static EventType type = EVENT_APP_SOURCE;
	Field(unsigned long long,session);
	StringField(name,32);
	Field(int, index);
	Field(float,x);
	Field(float,y);
	Field(unsigned long long, lastChangeTime);
	Field(bool, changedSinceLastSync);
	Field(bool, isActive);
	Field(int, stype);

};
struct AppRPN
{
	constexpr static EventType type = EVENT_APP_RPN;
	Field(unsigned long long,session);
	StringField(source,64);
	StringField(rpn,64);
};
struct AppActionMap
{
	constexpr static EventType type = EVENT_APP_ACTION_MAP;
	Field(unsigned long long,session);
	Field(unsigned long long,handle);
	StringField(actName,32);
	Field(unsigned char,hand);
	Field(int,mapIndex);
	Field(unsigned char,handIndex);
	Field(int,actionIndex1);
	Field(short,funcIndex1);
	Field(unsigned char,axisIndex1);
	Field(unsigned char,handIndex1);
	Field(int,actionIndex2);
	Field(short,funcIndex2);
	Field(unsigned char,axisIndex2);
	Field(unsigned char,handIndex2);
	Field(float, x);
	Field(float, y);
	Field(bool, override);
	Field(bool, hasAxisMapping);
};


struct AppCustomAction
{
	constexpr static EventType type = EVENT_APP_CUSTOM_ACTION;
	Field(unsigned long long,session);
	Field(int, index);
	Field(int, cmdIndex);
	Field(bool, hasCondition);
	Field(bool, hasVariables);
	Field(float, triggerPeriod);
	Field(float, lastTrigger);
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
	static unsigned long long startTime = 0;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	if(!startTime)
		startTime = ts.tv_sec;
	ts.tv_sec -= startTime;
	return ts.tv_sec*1e9 + ts.tv_nsec;
}

#endif // LAYER_EVENTS_H
