// SPDX-FileCopyrightText: 2021-2023 Arthur Brainville (Ybalrid) <ybalrid@ybalrid.info>
//
// SPDX-FileCopyrightText: 2024 mittorn <mittorn@disroot.org>
//
// SPDX-License-Identifier: MIT
//
// Initial Author: Arthur Brainville <ybalrid@ybalrid.info>

#include "layer_shims.hpp"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "layer_utl.h"
#define FMT_ENABLE_STDIO
#include "fmt_util.h"
#include "thread_utl.h"
#include "struct_utl.h"
#include "ini_parser.h"
#include <unistd.h>
#ifdef QTCREATOR_SUCKS
}
#endif

#if 1
#define Log(...) FPrint(stderr, __VA_ARGS__);
enum EventType{
	EVENT_NULL,
	EVENT_POLL_INTERACTION_PROFILE_CHANGED,
	EVENT_POLL_RELOAD_CONFIG,
	EVENT_ACTION_BOOL,
};

union PollEvent{
	EventType type;
	struct InteractionProfileChanged
	{
		EventType type = EVENT_POLL_INTERACTION_PROFILE_CHANGED;
	};
};

struct EventPoller
{
	Thread pollerThread = Thread([](void *poller){
		EventPoller *p = (EventPoller*)poller;
		p->Run();
	});
	CycleQueue<PollEvent> pollEvents;
	SpinLock pollLock;
	int fd = -1;
	bool InitSocket()
	{
		return false;
	}
	void Run()
	{
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

// stringview-like
struct SubString
{
	const char *begin, *end;
};
// fmt_util extension
template<typename Buf>
forceinline constexpr static inline void ConvertS(Buf &s, const SubString &arg)
{
	for(const char *str = arg.begin; str < arg.end;str++)
		s.w(*str);
}

struct ConfigLoader
{
	HashArrayMap<const char*, const char*> *CurrentSection;
	const char *CurrentSectionName = "[root]";
	void *CurrentSectionPointer = nullptr;
	HashMap<const char*, void*> parsedSecions;
	HashMap<const char*, GrowArray<void**>> forwardSections;
	IniParser &parser;
	ConfigLoader(IniParser &p) : CurrentSection(&p.mDict["[root]"]), parsedSecions(), parser(p) {}
	void SetIndex(int idx){}
	void End(size_t size){}
};



struct SectionHeader_
{
	const char *name = nullptr;
	SectionHeader_(){}
	SectionHeader_(const SectionHeader_ &other) = delete;
	SectionHeader_& operator=(const SectionHeader_ &) = delete;
	SectionHeader_(ConfigLoader &l) : name(l.CurrentSectionName){
	}
};
#define SectionHeader(PREFIX) \
	constexpr static const char * prefix = #PREFIX; \
	SectionHeader_ h;

template <typename S>
struct Sections
{

	HashMap<const char*, S> mSections;
	Sections(){}
	Sections(const Sections &other) = delete;
	Sections& operator=(const Sections &) = delete;
	Sections(ConfigLoader &l)
	{
		HashArrayMap<const char*, const char*> *PrevioisSection = l.CurrentSection;
		void *PrevioisSectionPointer = l.CurrentSectionPointer;
		for(int i = 0; i < l.parser.mDict.TblSize; i++)
		{
			for(auto *node = l.parser.mDict.table[i]; node; node = node->n)
			{
				char prefix[256];
				int len = SBPrint(prefix, "[%s.", S::prefix);
				if(!strncmp(node->k, prefix, len-1))
				{
					auto *sectionNode = mSections.GetOrAllocate(node->k);
					S& section = sectionNode->v;
					l.CurrentSection = &node->v;
					l.CurrentSectionName = sectionNode->k;
					l.parsedSecions[node->k] = (void*)&section;
					l.CurrentSectionPointer = &section;
					ConstructorVisitor<S, ConfigLoader>().Fill(&section,l);
					auto *fwd = l.forwardSections.GetPtr(node->k);
					if(fwd)
					{
						for(int j = 0; j < fwd->count; j++)
						{
							void **sec = (*fwd)[j];
							*sec = (void*)&section;
						}
					}

					l.CurrentSection = PrevioisSection;
					l.CurrentSectionPointer = PrevioisSectionPointer;
				}
			}
		}
	}
};


template <typename S, const auto &NAME>
struct SectionReference_
{
	constexpr static const char *name = NAME;
	S *ptr = 0;
	const char *suffix = nullptr;
	SectionReference_(const SectionReference_ &other) = delete;
	SectionReference_& operator=(const SectionReference_ &) = delete;
	SectionReference_(){}
	~SectionReference_()
	{
		if(suffix)
			free((void*)suffix);
		suffix = nullptr;
	}
	SectionReference_(ConfigLoader &l)
	{
		const char *&str = (*l.CurrentSection)[name];
		if(str)
		{
			char sectionName[256];
			SubString s{str};
			s.end = strchr(str, '.');
			if(!s.end)
				s.end = s.begin + strlen(str);
			else
				suffix = strdup(s.end + 1);
			SBPrint(sectionName, "[%s.%s]", S::prefix, s);
			auto *n = l.parser.mDict.GetNode(sectionName);
			if(!n)
			{
				Log("Section %s, key %s: missing config section referenced: %s!\n", ((SectionHeader_*)l.CurrentSectionPointer)->name, str, sectionName);
				return;
			}
			ptr = (S*)l.parsedSecions[n->k];
			if(!ptr)
				l.forwardSections[n->k].Add((void**)&ptr);
		}
		str = nullptr;
	}
};
#define SectionReference(type,name) \
constexpr static const char *opt_name_##name = #name; \
	SectionReference_<type, opt_name_##name> name;


template <typename T, const auto &NAME>
struct Option_
{
	constexpr static const char *name = NAME;
	T val;
	operator T() const
	{
		return val;
	}
	Option_(const Option_ &other) = delete;
	Option_& operator=(const Option_ &) = delete;
	Option_(){}
	Option_(const T &def):val(def){}
	Option_(ConfigLoader &l){
		const char *&str = (*l.CurrentSection)[name];
		if(str)
			val = (T)atof(str);
		str = nullptr;
	}
};
#define Option(type,name) \
	constexpr static const char *opt_name_##name = #name; \
	Option_<type, opt_name_##name> name;



template <const auto &NAME>
struct StringOption_
{
	constexpr static const char *name = NAME;
	const char *val = nullptr;
	operator const char *()
	{
		return val;
	}
	StringOption_(const StringOption_ &other) = delete;
	StringOption_& operator=(const StringOption_ &) = delete;
	StringOption_(){}
	~StringOption_()
	{
		if(val)
			free((void*)val);
		val = nullptr;
	}
	StringOption_(ConfigLoader &l){
		const char *&str = (*l.CurrentSection)[name];
		if(str)
			val = strdup(str);
		str = nullptr;
	}
};

#define StringOption(name) \
constexpr static const char *opt_name_##name = #name; \
	StringOption_<opt_name_##name> name;

#define IsDelim(c) (!(c) || ((c) == ' '|| (c) == ','))
static size_t GetEnum(const char *scheme, const char *val)
{
	size_t index = 0;
	while(true)
	{
		const char *pval = val;
		char c;
		while((c = *scheme))
		{
			if(!IsDelim(c))
			{
				index++;
				break;
			}
			scheme++;
		}
		while(*scheme && *scheme == *pval)scheme++, pval++;
		if(IsDelim(*scheme) && !*pval)
			return index;
		if(!*scheme)
			return 0;
		while(!IsDelim(*scheme))
			scheme++;
	}
}


template <typename T, const auto &NAME, const auto &SCHEME>
struct EnumOption_
{
	constexpr static const char *name = NAME;
	T val;
	operator T() const
	{
		return val;
	}
	EnumOption_(const EnumOption_ &other) = delete;
	EnumOption_& operator=(const EnumOption_ &) = delete;
	EnumOption_(){}
	EnumOption_(ConfigLoader &l){
		const char *&str = (*l.CurrentSection)[name];
		if(str)
		{
			val = (T)GetEnum(SCHEME, str);
		}
		else
			val = (T)0;
		str = nullptr;
	}
};

#define EnumOption(name, ...) \
	enum name ## _enum { \
		name ## _none, \
		__VA_ARGS__, \
		name ## _count \
	}; \
	constexpr static const char *name ## _name = #name; \
	constexpr static const char *name ## _scheme = #__VA_ARGS__; \
	EnumOption_<name ## _enum, name ## _name, name ## _scheme> name;

struct SourceSection
{
	SectionHeader(source);
	EnumOption(sourceType, remap, server);
	EnumOption(actionType, action_bool, action_float, action_vector2);
	// todo: bindings=/user/hand/left/input/grip/pose,/interaction_profiles/valve/index_controller:/user/hand/left/input/select/click
	StringOption(bindings);
	StringOption(subactionOverride);
	Option(float, minXIn);
	Option(float, maxXIn);
	Option(float, minXOut);
	Option(float, maxXOut);
	Option(float, minYIn);
	Option(float, maxYIn);
	Option(float, minYOut);
	Option(float, maxYOut);
	Option(float, threshold);
	Option(int, transformFunc);

};

struct BindingProfileSection;
struct ActionMapSection
{
	SectionHeader(actionmap);
	Option(bool, override);
	// TODO: RPN or something similar
	SectionReference(SourceSection, axis1);
	SectionReference(SourceSection, axis2);
	SectionReference(SourceSection, map);
	EnumOption(customAction, reloadSettings, changeProfile, triggerInteractionChange);
	SectionReference(BindingProfileSection, profileName);
};

static constexpr const char *const gszUserSuffixes[] =
{
	"left",
	"right",
	"head",
	"gamepad",
	//"/user/treadmill"
};


enum eUserPaths{
	USER_HAND_LEFT = 0,
	USER_HAND_RIGHT,
	USER_HEAD,
	USER_GAMEPAD,
	USER_INVALID,
	USER_PATH_COUNT
};

static int PathIndexFromSuffix(const char *suffix)
{
	int i;
	int len = strlen(suffix);
	const char *end = strchr(suffix, '[');
	if(end)
		len = end - suffix;
	for(i = 0; i < USER_INVALID; i++)
		if(!strncmp(suffix, gszUserSuffixes[i], len))
			break;
	return i;
}

struct BindingProfileSection
{
	SectionHeader(bindings);
	struct DynamicActionMaps {
		HashMap<const char *, ActionMapSection*> maps[USER_PATH_COUNT];
		DynamicActionMaps(){}
		DynamicActionMaps(const DynamicActionMaps &other) = delete;
		DynamicActionMaps& operator=(const DynamicActionMaps &) = delete;
		DynamicActionMaps(ConfigLoader &l)
		{
			for(int i = 0; i < l.CurrentSection->TblSize; i++)
			{
				for(int j = 0; j < l.CurrentSection->table[i].count; j++)
				{
					char sectionName[256];
					if(!l.CurrentSection->table[i][j].v)
						continue;
					SBPrint(sectionName, "[%s.%s]", ActionMapSection::prefix, l.CurrentSection->table[i][j].v);
					auto *n = l.parser.mDict.GetNode(sectionName);
					if(!n)
					{
						Log("Section %s, actionmap %s: missing config section referenced: %s\n", ((SectionHeader_*)l.CurrentSectionPointer)->name, l.CurrentSection->table[i][j].k, sectionName);
						continue;
					}

					ActionMapSection *map = (ActionMapSection *)l.parsedSecions[n->k];
					if(!map)
						continue;
					const char *suffix = strchr(l.CurrentSection->table[i][j].k, '.');
					if(suffix)
					{
						char str[suffix - l.CurrentSection->table[i][j].k + 1];
						memcpy(str, l.CurrentSection->table[i][j].k, suffix - l.CurrentSection->table[i][j].k);
						str[suffix - l.CurrentSection->table[i][j].k] = 0;
						suffix++;
						maps[PathIndexFromSuffix(suffix)][str] = map;
					}
					else
					{
						maps[USER_HAND_LEFT][l.CurrentSection->table[i][j].k] = map;
						maps[USER_HAND_RIGHT][l.CurrentSection->table[i][j].k] = map;
					}

					l.CurrentSection->table[i][j].v = nullptr;
				}
			}
		}
	} actionMaps;
};
struct Config
{
	// SectionHeader name
	Config(const Config &other) = delete;
	Config& operator=(const Config &) = delete;
	Option(int, serverPort);
	StringOption(interactionProfile);
	Sections<SourceSection> sources;
	Sections<ActionMapSection> actionMaps;
	Sections<BindingProfileSection> bindings;
	SectionReference(BindingProfileSection,startupProfile);
};

static void LoadConfig(Config *c)
{
	const char *cfg = "layer_config.ini";
	IniParser p(cfg);
	if(!p)
	{
		Log("Cannot load %s", cfg);
		return;
	}
	ConfigLoader t = ConfigLoader(p);
	t.CurrentSectionPointer = c;
	ConstructorVisitor<Config, ConfigLoader>().Fill(c,t);
	for(int i = 0; i < p.mDict.TblSize; i++)
		for(auto *node = p.mDict.table[i]; node; node = node->n)
			for(int j = 0; j < node->v.TblSize; j++)
				for(int k = 0; k < node->v.table[j].count; k++ )
					if(node->v.table[j][k].v)
						Log("Section %s: unused config key %s = %s\n", node->k, node->v.table[j][k].k, node->v.table[j][k].v);

	Log("ServerPort %d\n", (int)c->serverPort);
}



//Define next function pointer
#define DECLARE_NEXT_FUNC(x) PFN_##x nextLayer_##x
//Load next function pointer
#define LOAD_NEXT_FUNC(x) nextLayer_xrGetInstanceProcAddr(mInstance, #x, (PFN_xrVoidFunction*)&nextLayer_##x)


#endif

static float GetBoolAction(void *priv, int act, int hand, int ax);
static float GetFloatAction(void *priv, int act, int hand, int ax);
static float GetVec2Action(void *priv, int act, int hand, int ax);
constexpr float (*actionFuncs[4])(void *priv, int act, int hand, int ax) =
{
	GetBoolAction,
	GetFloatAction,
	GetVec2Action
};

struct Layer
{

	// maximum active instances loaded in class
	// normally it should be 1, but reserve more in case some library checks OpenXR
	constexpr static size_t max_instances = 4;
	static Layer mInstances[max_instances];


	// Only handle this XrInatance in this object
	XrInstance mInstance = XR_NULL_HANDLE;
	XrSession mActiveSession = XR_NULL_HANDLE;

	EventPoller poller;

	// Extensions list
	const char **mExtensions;
	uint32_t mExtensionsCount;

	Config config = {};


	static constexpr const char *const mszUserPaths[] =
	{
		"/user/hand/left",
		"/user/hand/right",
		"/user/head",
		"/user/gamepad",
		//"/user/treadmill"
	};

	XrPath mUserPaths[USER_PATH_COUNT];

	// Always need this for instance creation/destroy
	DECLARE_NEXT_FUNC(xrGetInstanceProcAddr);
	DECLARE_NEXT_FUNC(xrDestroyInstance);
#define NEXT_FUNC(f, x) f(x)

	// Declare all used OpenXR functions here
#define NEXT_FUNC_LIST(f) \
	NEXT_FUNC(f, xrCreateAction); \
	NEXT_FUNC(f, xrCreateActionSet); \
	NEXT_FUNC(f, xrEnumerateBoundSourcesForAction); \
	NEXT_FUNC(f, xrPathToString); \
	NEXT_FUNC(f, xrGetInputSourceLocalizedName); \
	NEXT_FUNC(f, xrAttachSessionActionSets); \
	NEXT_FUNC(f, xrStringToPath); \
	NEXT_FUNC(f, xrGetCurrentInteractionProfile); \
	NEXT_FUNC(f, xrPollEvent); \
	NEXT_FUNC(f, xrCreateSession); \
	NEXT_FUNC(f, xrDestroySession); \
	NEXT_FUNC(f, xrDestroyAction); \
	NEXT_FUNC(f, xrGetActionStateBoolean); \
	NEXT_FUNC(f, xrGetActionStateFloat); \
	NEXT_FUNC(f, xrGetActionStateVector2f); \
	NEXT_FUNC(f, xrSyncActions); \
	NEXT_FUNC(f, xrWaitFrame); \
	NEXT_FUNC(f, xrSuggestInteractionProfileBindings );

	NEXT_FUNC_LIST(DECLARE_NEXT_FUNC);

	// Find Layer index for XrInstance
	static int FindInstance(XrInstance inst)
	{
		for(int i = 0; i < max_instances; i++) if(mInstances[i].mInstance == inst) return i;
		return -1;
	}

#define INSTANCE_FALLBACK(call) \
	if( unlikely(instance != mInstance) ) \
	{ \
		int i = FindInstance(instance);\
		if(i >= 0) \
			return mInstances[i].call; \
		return XR_ERROR_HANDLE_INVALID; \
	}

	// custom action set or server app should write data here
	struct ActionSource
	{
		int actionIndex;
		int funcIndex;
		int axisIndex;
		int handIndex;
		//SourceSection *mpConfig;
		void *priv;
		float GetValue()
		{
			return actionFuncs[funcIndex](priv, actionIndex, handIndex, axisIndex);
		}
	};

	struct ActionMap
	{
		//ActionMapSection *mpConfig;
		ActionSource src[2];
		int actionIndex = -1;
		int handIndex = 0;
		float GetAxis(int axis)
		{
			// todo: actual axis mapping and calculations should be done here?
			return src[axis].GetValue();
		}
	};

	struct ActionState
	{
		bool hasAxisMapping = false;
		bool override = false;
		ActionMap map;
	};

	struct Action
	{
		XrActionCreateInfo info = { XR_TYPE_UNKNOWN };
		XrAction action;
		ActionState baseState[USER_PATH_COUNT];
	};

	struct ActionBoolean : Action
	{
		XrActionStateBoolean typedState[USER_PATH_COUNT];
		void Update(int hand)
		{
			typedState[hand].currentState = baseState[hand].map.GetAxis(0);
		}
	};

	struct ActionFloat : Action
	{
		XrActionStateFloat typedState[USER_PATH_COUNT];
		void Update(int hand)
		{
			typedState[hand].currentState = baseState[hand].map.GetAxis(0);
		}
	};

	struct ActionVec2 : Action
	{
		XrActionStateVector2f typedState[USER_PATH_COUNT];
		void Update(int hand)
		{
			typedState[hand].currentState.x = baseState[hand].map.GetAxis(0);
			typedState[hand].currentState.y = baseState[hand].map.GetAxis(1);
		}
	};

	struct SessionState
	{
		XrSession mSession = XR_NULL_HANDLE;
		XrSessionCreateInfo info = { XR_TYPE_UNKNOWN };
		XrActionSet *mActionSets = nullptr;
		size_t mActionSetsCount = 0;

		// application actions
		HashArrayMap<XrAction, ActionBoolean> mActionsBoolean;
		HashArrayMap<XrAction, ActionFloat> mActionsFloat;
		HashArrayMap<XrAction, ActionVec2> mActionsVec2;

		// layer actions
		GrowArray<ActionBoolean> mLayerActionsBoolean;
		GrowArray<ActionFloat> mLayerActionsFloat;
		GrowArray<ActionVec2> mLayerActionsVec2;

		// need to dynamicly inject new sources
		HashArrayMap<const char*, int> mBoolIndexes;
		HashArrayMap<const char*, int> mFloatIndexes;
		HashArrayMap<const char*, int> mVec2Indexes;

		~SessionState()
		{
			delete[] mActionSets;
			mActionSets = nullptr;
			mActionSetsCount = 0;
		}
	};

	SessionState &GetSession(XrSession s)
	{
		if(mActiveSession == s && mpActiveSession)
			return *mpActiveSession;

		SessionState &w = mSessions[s];
		w.mSession = s;
		return w;
	}

	struct ActionSet
	{
		XrActionSetCreateInfo info = { XR_TYPE_UNKNOWN };
		XrInstance instance = XR_NULL_HANDLE;
		GrowArray<Action> mActions;
	};
	HashMap<XrActionSet, ActionSet> gActionSetInfos;
	ActionSet mLayerActionSet;
	XrActionSet mhLayerActionSet;
	HashArrayMap<const char *, int> mLayerActionIndexes;
	HashMap<XrPath, GrowArray<XrActionSuggestedBinding>> mLayerSuggestedBindings;
	bool mfLayerActionSetSuggested = false;

	HashMap<XrSession, SessionState> mSessions;
	SessionState *mpActiveSession;
	// must be session-private, but always need most recent
	XrTime mPredictedTime;


	XrResult noinline thisLayer_xrCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *session)
	{
		INSTANCE_FALLBACK(thisLayer_xrCreateSession(instance, createInfo, session));
		XrResult res = nextLayer_xrCreateSession(instance, createInfo, session);
		if(res == XR_SUCCESS)
		{
			SessionState &w = GetSession(*session);
			w.info = *createInfo;
		}
		return res;
	}
	XrResult noinline thisLayer_xrDestroySession(XrSession session)
	{
		if(mActiveSession == session)
		{
			mActiveSession = XR_NULL_HANDLE;
			mpActiveSession = nullptr;
		}
		mSessions.Remove(session);
		return nextLayer_xrDestroySession(session);
	}
	void InitLayerActionSet()
	{
		if( mhLayerActionSet )
			return;
		XrActionSetCreateInfo &asInfo = mLayerActionSet.info;
		asInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
		strcpy( asInfo.actionSetName, "layer_action_aet");
		strcpy( asInfo.localizedActionSetName, "LayerActionSet");
		asInfo.priority = 0;
		nextLayer_xrCreateActionSet(mInstance, &asInfo, &mhLayerActionSet);
		XrPath defaultProfile;
		const char *profile = config.interactionProfile.val;
		if(!profile)
			profile = "/interaction_profiles/khr/simple_controller";
		nextLayer_xrStringToPath(mInstance, profile, &defaultProfile);
		mfLayerActionSetSuggested = false;
		for(int i = 0; i < config.sources.mSections.TblSize; i++)
		{
			for(auto *node = config.sources.mSections.table[i]; node; node = node->n)
			{
				if(node->v.sourceType == SourceSection::remap)
				{
					mLayerActionSet.mActions.Add({});
					Action &act = mLayerActionSet.mActions[mLayerActionSet.mActions.count - 1];
					XrActionCreateInfo &info = act.info;
					info.type = XR_TYPE_ACTION_CREATE_INFO;
					info.actionType = (XrActionType)(int)node->v.actionType;
					XrPath sub;
					if(node->v.subactionOverride)
					{
						info.countSubactionPaths = 1;
						nextLayer_xrStringToPath(mInstance, node->v.subactionOverride, &sub);
						info.subactionPaths = &sub;
					}
					else
					{
						info.countSubactionPaths = USER_HEAD;
						info.subactionPaths = mUserPaths;
					}
					strncpy(info.localizedActionName, node->v.h.name, sizeof(info.localizedActionName) - 1);
					strncpy(info.actionName, node->v.h.name + sizeof("[source"), strlen(node->v.h.name) - sizeof("[source]"));
					nextLayer_xrCreateAction(mhLayerActionSet, &info, &act.action);

					const char *str = node->v.bindings;
					mLayerActionIndexes[node->v.h.name] = mLayerActionSet.mActions.count - 1;
					if(!str)
					{
						Log("Missing bindings for source %s\n", node->v.h.name);
						continue;
					}

					do
					{
						XrPath path;
						XrPath profile = defaultProfile;
						const char *pstr = str;
						const char *delim = strchr(str, ':');
						if(delim)
						{
							char pr[delim - str + 1];
							memcpy(pr, str, delim - str);
							pr[delim - str] = 0;
							nextLayer_xrStringToPath(mInstance, pr, &profile);
							str = delim + 1;
						}
						str = strchr(pstr, ',');
						if(!str)
							str = pstr + strlen(pstr) + 1;
						else str++;

						char bnd[str - pstr];
						memcpy(bnd, pstr, str - pstr - 1);
						bnd[str - pstr - 1] = 0;
						nextLayer_xrStringToPath(mInstance, bnd, &path);
						mLayerSuggestedBindings[profile].Add({act.action,path});
					}while(*(str-1));

				}
			}
		}
	}

	// Connect this layer to new XrInstance
	void Initialize(XrInstance inst, PFN_xrGetInstanceProcAddr gpa, const char**exts, uint32_t extcount )
	{
		mInstance = inst;
		// Need this for LOAD_FUNC
		nextLayer_xrGetInstanceProcAddr = gpa;
		LOAD_NEXT_FUNC(xrDestroyInstance);

		// Load all used OpenXR functions here
		NEXT_FUNC_LIST(LOAD_NEXT_FUNC);

		// Swap extensions list
		delete[] mExtensions;
		mExtensions = exts;
		mExtensionsCount = extcount;
		for(int i = 0; i < USER_INVALID; i++)
			nextLayer_xrStringToPath(inst, mszUserPaths[i], &mUserPaths[i]);
		LoadConfig(&config);
		if(config.serverPort)
			poller.Start(config.serverPort);
	}

	// Only need this if extensions used
	bool IsExtensionEnabled(const char *extension)
	{
		for(int i = 0; i < mExtensionsCount; i++)
			if(!strcmp(extension, mExtensions[i]))
				return true;
		return false;
	}
	XrResult noinline thisLayer_xrCreateAction (XrActionSet actionSet, const XrActionCreateInfo *info, XrAction *action)
	{
		XrAction act;
		XrResult r = nextLayer_xrCreateAction(actionSet, info, &act);
		//DumpGenericStruct(info);
		if(r == XR_SUCCESS)
		{
			*action = act;

			if(!gActionSetInfos[actionSet].mActions.Add({*info, act}))
			{
				nextLayer_xrDestroyAction(act);
				return XR_ERROR_OUT_OF_MEMORY;
			}
		}
		return r;
	}
	XrResult noinline thisLayer_xrCreateActionSet (XrInstance instance, const XrActionSetCreateInfo *info, XrActionSet *actionSet)
	{
		INSTANCE_FALLBACK(thisLayer_xrCreateActionSet(instance, info, actionSet));
		XrActionSet acts;
		XrResult r = nextLayer_xrCreateActionSet(instance, info, &acts);
		if(r == XR_SUCCESS)
		{
			*actionSet = acts;

			ActionSet &as = gActionSetInfos[acts];
			as.info = *info;
			//as.instance = instance;
		}
		return r;
	}
	static size_t _FindPath2(XrPath *path, size_t max, XrPath p)
	{
		size_t i = 0;
		while(path[i] != p) i++;
		return i;
	}

	// gcc doing strange jumpy unroll here, so split branchless hands from others
	forceinline size_t FindPath(XrPath p)
	{
		size_t i = 0;
		int notright = p != mUserPaths[USER_HAND_RIGHT];
		int notleft = p != mUserPaths[USER_HAND_LEFT];

		// fast branchess path
		i = !notright + (notleft & notright) * USER_HEAD;
		if(unlikely(i == USER_HEAD))
			return _FindPath2(&mUserPaths[USER_HEAD], USER_PATH_COUNT - USER_HEAD, p);
		return i;
	}

	XrResult thisLayer_xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateBoolean *state)
	{
		if(likely(mpActiveSession && mActiveSession == session))
		{
			ActionBoolean *a = mpActiveSession->mActionsBoolean.GetPtr(getInfo->action);
			if(likely(a))
			{
				XrResult r = XR_SUCCESS;
				int handPath = FindPath(getInfo->subactionPath);
				ActionState &hand = a->baseState[handPath];
				if(!hand.override)
					r = nextLayer_xrGetActionStateBoolean(session, getInfo, state);
				else
					*state = a->typedState[handPath];
				return r;
			}
		}
		return nextLayer_xrGetActionStateBoolean(session, getInfo, state);
	}

	XrResult thisLayer_xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateFloat *state)
	{
		if(likely(mpActiveSession && mActiveSession == session))
		{
			ActionFloat *a = mpActiveSession->mActionsFloat.GetPtr(getInfo->action);
			if(likely(a))
			{
				XrResult r = XR_SUCCESS;
				int handPath = FindPath(getInfo->subactionPath);
				ActionState &hand = a->baseState[handPath];
				if(!hand.override)
					r = nextLayer_xrGetActionStateFloat(session, getInfo, state);
				else
					*state = a->typedState[handPath];
				return r;
			}
		}
		return nextLayer_xrGetActionStateFloat(session, getInfo, state);
	}

	XrResult thisLayer_xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateVector2f *state)
	{
		if(likely(mpActiveSession && mActiveSession == session))
		{
			ActionVec2 *a = mpActiveSession->mActionsVec2.GetPtr(getInfo->action);
			if(likely(a))
			{
				XrResult r = XR_SUCCESS;
				int handPath = FindPath(getInfo->subactionPath);
				ActionState &hand = a->baseState[handPath];
				if(!hand.override)
					r = nextLayer_xrGetActionStateVector2f(session, getInfo, state);
				else
					*state = a->typedState[handPath];
				return r;
			}
		}
		return nextLayer_xrGetActionStateVector2f(session, getInfo, state);
	}

	XrResult thisLayer_xrWaitFrame(XrSession session, const XrFrameWaitInfo *frameWaitInfo, XrFrameState *frameState)
	{

		XrResult res = nextLayer_xrWaitFrame(session, frameWaitInfo, frameState);
		mPredictedTime = frameState->predictedDisplayTime;
		return res;
	}

	template <typename A, typename L>
	void UpdateActionState(A &a, L &array)
	{
		for(int handPath = 0; handPath < USER_PATH_COUNT; handPath++)
		{
			ActionState &hand = a.baseState[handPath];
			auto &state = a.typedState[handPath];
			if(hand.map.actionIndex >= 0)
			{
				// todo: axismap and map should have handIndex, not just SectionReference
				// source should allow multiple bindings (possible need string list or even dict)
				state = array[hand.map.actionIndex].typedState[hand.map.handIndex];
			}
			if(unlikely(hand.hasAxisMapping))
			{
				a.Update(handPath);
				if(hand.map.actionIndex < 0)
				{
					// todo: detect change
					state.changedSinceLastSync = true;
					state.lastChangeTime = mPredictedTime;
				}
			}
		}
	}

	forceinline XrResult thisLayer_xrSyncActions(XrSession session, const XrActionsSyncInfo *syncInfo)
	{

		//XrResult ret = nextLayer_xrSyncActions(session, syncInfo);

		XrActionsSyncInfo nsyncInfo = *syncInfo;//{XR_TYPE_ACTIONS_SYNC_INFO};
		//nsyncInfo.countActiveActionSets = 1;
		XrActiveActionSet as[syncInfo->countActiveActionSets + 1];
		memcpy(as, syncInfo->activeActionSets, syncInfo->countActiveActionSets + sizeof(XrActiveActionSet));
		as[syncInfo->countActiveActionSets].actionSet = mhLayerActionSet;
		as[syncInfo->countActiveActionSets].subactionPath = XR_NULL_PATH;
		nsyncInfo.activeActionSets = as;
		nsyncInfo.countActiveActionSets = syncInfo->countActiveActionSets + 1;
		//return res;

		// todo: find out if it's openxr breaks second sync call or just monado broken
		XrResult ret = nextLayer_xrSyncActions(session, &nsyncInfo);
		if(unlikely(!mpActiveSession && mActiveSession == session))
		{
			mpActiveSession = mSessions.GetPtr(session);
			if(!mpActiveSession)
				return ret;
			mActiveSession = session;
		}
		for(int i = 0; i < mpActiveSession->mLayerActionsBoolean.count; i++)
		{
			for(int hand = 0; hand < 2; hand++)
			{
				XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
				getInfo.action = mpActiveSession->mLayerActionsBoolean[i].action;
				getInfo.subactionPath = mUserPaths[hand];
				mpActiveSession->mLayerActionsBoolean[i].typedState[hand].type = XR_TYPE_ACTION_STATE_BOOLEAN;
				nextLayer_xrGetActionStateBoolean(session, &getInfo, &mpActiveSession->mLayerActionsBoolean[i].typedState[hand]);
			}
		}
		for(int i = 0; i < mpActiveSession->mLayerActionsFloat.count; i++)
		{
			for(int hand = 0; hand < 2; hand++)
			{
				XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
				getInfo.action = mpActiveSession->mLayerActionsFloat[i].action;
				getInfo.subactionPath = mUserPaths[hand];
				mpActiveSession->mLayerActionsFloat[i].typedState[hand].type = XR_TYPE_ACTION_STATE_FLOAT;
				nextLayer_xrGetActionStateFloat(session, &getInfo, &mpActiveSession->mLayerActionsFloat[i].typedState[hand]);
			}
		}
		for(int i = 0; i < mpActiveSession->mLayerActionsVec2.count; i++)
		{
			for(int hand = 0; hand < 2; hand++)
			{
				XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
				getInfo.action = mpActiveSession->mLayerActionsVec2[i].action;
				getInfo.subactionPath = mUserPaths[hand];
				mpActiveSession->mLayerActionsFloat[i].typedState[hand].type = XR_TYPE_ACTION_STATE_VECTOR2F;
				nextLayer_xrGetActionStateVector2f(session, &getInfo, &mpActiveSession->mLayerActionsVec2[i].typedState[hand]);
			}
		}

		for(int i = 0; i < mpActiveSession->mActionsBoolean.TblSize; i++)
			for(int j = 0; j < mpActiveSession->mActionsBoolean.table[i].count;j++)
				UpdateActionState(mpActiveSession->mActionsBoolean.table[i][j].v, mpActiveSession->mLayerActionsBoolean);

		for(int i = 0; i < mpActiveSession->mActionsFloat.TblSize; i++)
			for(int j = 0; j < mpActiveSession->mActionsFloat.table[i].count;j++)
				UpdateActionState(mpActiveSession->mActionsFloat.table[i][j].v, mpActiveSession->mLayerActionsFloat);

		for(int i = 0; i < mpActiveSession->mActionsVec2.TblSize; i++)
			for(int j = 0; j < mpActiveSession->mActionsVec2.table[i].count;j++)
				UpdateActionState(mpActiveSession->mActionsVec2.table[i][j].v, mpActiveSession->mLayerActionsVec2);

		return ret;
	}
	void DumpActionSet(XrSession session, XrActionSet s)
	{
		ActionSet &seti = gActionSetInfos[s];
		Log("Attached action set: %s %s\n", seti.info.actionSetName, seti.info.localizedActionSetName );
		for(int i = 0; i < seti.mActions.count; i++ )
		{
			Action &as = seti.mActions[i];
			XrActionCreateInfo &cinfo = as.info;
			Log("info %p: %s %s\n", (void*)as.action, cinfo.actionName, cinfo.localizedActionName);
			XrBoundSourcesForActionEnumerateInfo einfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
			einfo.action = as.action;
			uint32_t count;
			nextLayer_xrEnumerateBoundSourcesForAction(session, &einfo, 0, &count, NULL);
			if(count)
			{
				/// TODO: non-VLA compilers fallback
				XrPath paths[count];
				nextLayer_xrEnumerateBoundSourcesForAction(session, &einfo, count, &count, paths);
				for(int j = 0; j < count; j++)
				{
					uint32_t slen;
					XrInputSourceLocalizedNameGetInfo linfo = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
					linfo.sourcePath = paths[j];
					linfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT | XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;
					nextLayer_xrGetInputSourceLocalizedName(session, &linfo, 0, &slen, NULL);
					{
						char str[slen + 1];
						nextLayer_xrGetInputSourceLocalizedName(session, &linfo, slen + 1, &slen, str);
						Log("Description %s\n", (char*)str);
					}
					nextLayer_xrPathToString(mInstance, paths[j], 0, &slen, NULL);
					{
						char str[slen + 1];
						nextLayer_xrPathToString(mInstance, paths[j], slen + 1, &slen, str);
						Log("raw_path %s\n", (char*)str);
					}
				}
			}
		}
	}

	template<typename AT>
	int AddSourceToSessionT( HashArrayMap<const char*, int> &indexMap, GrowArray<AT>& array, const char *name )
	{
		int &idx = indexMap[name];
		if(!idx)
		{
			idx = array.count + 1;
			array.Add({mLayerActionSet.mActions[mLayerActionIndexes[name]]});
		}
		return idx - 1;
	}

	int AddSourceToSession( SessionState &w, XrActionType t, const char *name )
	{
		if(t == XR_ACTION_TYPE_BOOLEAN_INPUT)
			return AddSourceToSessionT( w.mBoolIndexes, w.mLayerActionsBoolean, name );
		else if(t == XR_ACTION_TYPE_FLOAT_INPUT)
			return AddSourceToSessionT( w.mFloatIndexes, w.mLayerActionsFloat, name );
		else if(t == XR_ACTION_TYPE_VECTOR2F_INPUT)
			return AddSourceToSessionT( w.mVec2Indexes, w.mLayerActionsVec2, name );
		return -1;
	}

	int HandFromConfig(const SourceSection &c, const char *suffix)
	{
		if(suffix)
			return PathIndexFromSuffix(suffix);
		else if(!c.subactionOverride.val)
			return 1;
		XrPath p;
		nextLayer_xrStringToPath(mInstance, c.subactionOverride.val, &p);
		return FindPath(p);
	}

	void AxisFromConfig(Action &a, int hand, SourceSection &c, const char *suffix, int axis, SessionState &w)
	{
		int t = c.actionType;
		a.baseState[hand].map.src[axis].actionIndex = AddSourceToSession(w, (XrActionType)t, c.h.name );
		if(a.baseState[hand].map.src[axis].actionIndex < 0)
		{
			Log( "Invalid action type: axis%d %s\n", axis, t, suffix?suffix:"(auto)", c.h.name );
			t = 1;
			a.baseState[hand].map.src[axis].actionIndex  = 0;
		}
		a.baseState[hand].map.src[axis].handIndex = HandFromConfig(c, suffix);
		a.baseState[hand].map.src[axis].funcIndex = t - 1;
		a.baseState[hand].map.src[axis].priv = &w;
		a.baseState[hand].map.src[axis].axisIndex = suffix && !!strstr(suffix, "[1]");
		a.baseState[hand].hasAxisMapping = true;
	}

	void ApplyActionMap(SessionState &w, Action &a, ActionMapSection *s, int hand)
	{
		a.baseState[hand].override = true;

		if(s->map.ptr)
		{
			if((int)s->map.ptr->actionType == (int)a.info.actionType)
				a.baseState[hand].map.actionIndex = AddSourceToSession(w, a.info.actionType, s->map.ptr->h.name );
			if(a.baseState[hand].map.actionIndex < 0)
				Log( "Invalid direct action map (types must be same): %s %s %s %d\n", s->h.name, s->map.suffix? s->map.suffix: "(auto)", s->map.ptr->h.name, hand );
			a.baseState[hand].map.handIndex = HandFromConfig(*s->map.ptr, s->map.suffix);
		}

		if(s->axis1.ptr)
			AxisFromConfig(a, hand, *s->axis1.ptr, s->axis1.suffix, 0, w);
		if(s->axis2.ptr)
			AxisFromConfig(a, hand, *s->axis2.ptr, s->axis2.suffix, 1, w);
	}

	void InitProfile(SessionState &w, BindingProfileSection *p)
	{
		w.mActionsBoolean.~HashArrayMap();// = {};
		w.mActionsFloat.~HashArrayMap();// = {};
		w.mActionsVec2.~HashArrayMap();// = {};
		w.mLayerActionsBoolean.~GrowArray();// = {};
		w.mLayerActionsFloat.~GrowArray();// = {};
		w.mLayerActionsVec2.~GrowArray();// = {};
		w.mBoolIndexes.~HashArrayMap();
		w.mFloatIndexes.~HashArrayMap();
		w.mVec2Indexes.~HashArrayMap();
		for(int i = 0; i < w.mActionSetsCount; i++)
		{
			ActionSet &set = gActionSetInfos[w.mActionSets[i]];
			for(int j = 0; j < set.mActions.count; j++)
			{
				Action &a = set.mActions[j];
				for(int i = 0; i < USER_PATH_COUNT; i++)
				{
					ActionMapSection *s = p->actionMaps.maps[i][a.info.actionName];
					if(s)
						ApplyActionMap(w, a, s, i);
				}
				switch(a.info.actionType)
				{
					case XR_ACTION_TYPE_BOOLEAN_INPUT:
					{
						ActionBoolean &ab = w.mActionsBoolean[a.action];
						*(Action*)&ab = a;
						for(int i = 0; i < USER_PATH_COUNT; i++)
						{
							ab.typedState[i].type = XR_TYPE_ACTION_STATE_BOOLEAN;
							ab.typedState[i].isActive = true;
						}
						break;
					}
					case XR_ACTION_TYPE_FLOAT_INPUT:
					{
						ActionFloat &af = w.mActionsFloat[a.action];
						*(Action*)&af = a;
						for(int i = 0; i < USER_PATH_COUNT; i++)
						{
							af.typedState[i].type = XR_TYPE_ACTION_STATE_FLOAT;
							af.typedState[i].isActive = true;
						}
						break;
					}
					case XR_ACTION_TYPE_VECTOR2F_INPUT:
					{
						ActionVec2 &af = w.mActionsVec2[a.action];
						*(Action*)&af = a;
						for(int i = 0; i < USER_PATH_COUNT; i++)
						{
							af.typedState[i].type = XR_TYPE_ACTION_STATE_VECTOR2F;
							af.typedState[i].isActive = true;
						}
						break;
					}
					default:
						break;
				}
			}
		}
	}
	XrResult thisLayer_xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding *suggestedBindings)
	{
		InitLayerActionSet();
		auto *layerSuggest = mLayerSuggestedBindings.GetPtr(suggestedBindings->interactionProfile);
		if(layerSuggest)
		{
			XrActionSuggestedBinding bindings[suggestedBindings->countSuggestedBindings + layerSuggest->count];
			memcpy(bindings, suggestedBindings->suggestedBindings, suggestedBindings->countSuggestedBindings * sizeof(XrActionSuggestedBinding));
			XrInteractionProfileSuggestedBinding newSuggestedBindings = *suggestedBindings;
			newSuggestedBindings.countSuggestedBindings = suggestedBindings->countSuggestedBindings + layerSuggest->count;
			newSuggestedBindings.suggestedBindings = bindings;
			for(int i = 0; i < layerSuggest->count; i++ )
				bindings[suggestedBindings->countSuggestedBindings + i] = (*layerSuggest)[i];

			mfLayerActionSetSuggested = true;
			nextLayer_xrSuggestInteractionProfileBindings(instance, &newSuggestedBindings);
		}
		else
			nextLayer_xrSuggestInteractionProfileBindings(instance, suggestedBindings);

		return XR_SUCCESS;
	}

	XrResult noinline thisLayer_xrAttachSessionActionSets (XrSession session, const XrSessionActionSetsAttachInfo *info)
	{
		XrSessionActionSetsAttachInfo newInfo = *info;
		newInfo.countActionSets++;
		XrActionSet newActionSets[newInfo.countActionSets];
		memcpy(newActionSets, info->actionSets, info->countActionSets * sizeof(XrActionSet));
		newActionSets[info->countActionSets] = mhLayerActionSet;
		newInfo.actionSets = newActionSets;
		XrResult r = nextLayer_xrAttachSessionActionSets(session, &newInfo);
		if(r == XR_SUCCESS)
		{
			SessionState &w = GetSession(session);
			mActiveSession = session;
			mpActiveSession = &w;
			delete[] w.mActionSets;
			w.mActionSets = new XrActionSet[info->countActionSets];
			w.mActionSetsCount = info->countActionSets;

			for(int i = 0; i < info->countActionSets; i++)
			{
				XrActionSet s = info->actionSets[i];
				w.mActionSets[i] = s;
				DumpActionSet(session,s);
			}
			InitProfile(w, config.startupProfile.ptr);
		}
		return r;
	}

	forceinline XrResult thisLayer_xrPollEvent(
		XrInstance instance, XrEventDataBuffer* eventData)
	{
		//INSTANCE_FALLBACK(thisLayer_xrPollEvent(instance, eventData));
		XrResult res = nextLayer_xrPollEvent(instance, eventData);

		// most times it should be XR_EVENT_UNAVAILABLE
		if(unlikely(instance == mInstance && res == XR_SUCCESS))
		{
			if(likely(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED))
			{
				XrEventDataSessionStateChanged *eSession = (XrEventDataSessionStateChanged*) eventData;
				if(unlikely(eSession->state >= XR_SESSION_STATE_IDLE && eSession->state <= XR_SESSION_STATE_FOCUSED && mActiveSession != eSession->session))
				{
					mActiveSession = eSession->session;
					mpActiveSession = mSessions.GetPtr(mActiveSession);
//					for(int i = 0; i < w->mActionSetsCount; i++)
//						DumpActionSet(mActiveSession, w->mActionSets[i]);

				}
			}

			if(unlikely(eventData->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) && mpActiveSession)
			{
				SessionState *w = mpActiveSession;
				if(!w)
					return res;

				XrInteractionProfileState state = {XR_TYPE_INTERACTION_PROFILE_STATE};
				for(int i = 0; i < USER_INVALID; i++)
				{
					if(nextLayer_xrGetCurrentInteractionProfile(mActiveSession, mUserPaths[i], &state) != XR_SUCCESS)
						continue;
					if(state.interactionProfile == XR_NULL_PATH)
						continue;
					char profileStr[XR_MAX_PATH_LENGTH];
					uint32_t len;
					nextLayer_xrPathToString(instance, state.interactionProfile, XR_MAX_PATH_LENGTH, &len, profileStr);
					Log("New interaction profile for %s: %s\n", mszUserPaths[i], profileStr );
				}
				for(int i = 0; i < w->mActionSetsCount; i++)
					DumpActionSet(mActiveSession, w->mActionSets[i]);
			}
		}
		return res;
	}

	//IMPORTANT: to allow for multiple instance creation/destruction, the contect of the layer must be re-initialized when the instance is being destroyed.
	//Hooking xrDestroyInstance is the best way to do that.
	XrResult noinline thisLayer_xrDestroyInstance(XrInstance instance)
	{
		INSTANCE_FALLBACK(thisLayer_xrDestroyInstance(instance));
		mInstance = XR_NULL_HANDLE;
		mExtensionsCount = 0;
		delete[] mExtensions;
		mExtensions = nullptr;
		poller.Stop();
		return nextLayer_xrDestroyInstance(instance);
	}

	~Layer()
	{
		// prevent leak if someone forgot to call xrDestroyInstance
		mInstance = XR_NULL_HANDLE;
		mExtensionsCount = 0;
		delete[] mExtensions;
		mExtensions = nullptr;
	}

#if XR_THISLAYER_HAS_EXTENSIONS
	//The following function doesn't exist in the spec, this is just a test for the extension mecanism
	XrResult thisLayer_xrTestMeTEST(XrSession session)
	{
		(void)session;
		printf("xrTestMe()\n");
		return XR_SUCCESS;
	}
#endif
};

float GetBoolAction(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return w->mLayerActionsBoolean[act].typedState[hand].currentState;
}

float GetFloatAction(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return w->mLayerActionsFloat[act].typedState[hand].currentState;
}

float GetVec2Action(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return ax?w->mLayerActionsVec2[act].typedState[hand].currentState.y
			: w->mLayerActionsVec2[act].typedState[hand].currentState.x;
}


Layer Layer::mInstances[max_instances];

// Thunk template. This allows calling non-static layer members with static function pointers
// This generates separate functions for each Layer::mInstance
// C++17 required
template<class T, size_t instance_index, class Result, class... Args>
struct InstanceThunk
{
	template<Result(T::*pfn)(Args...)>
	XRAPI_ATTR static Result XRAPI_CALL Call(Args... a)
	{
		return (T::mInstances[instance_index].*pfn)(a...);
	}
	//static XrResult call1()
};

template<typename T, typename Result, typename... Args>
struct FunctionPointerGenerator
{
	FunctionPointerGenerator(Result(T::*pfn)(Args...)){}
	template<Result(T::*pfn)(Args...), size_t instance_index = 0>
	static Result(*getFunc(int i))(Args...)
	{
		if(i == instance_index)
			return (&InstanceThunk<T,instance_index, Result,Args...>::template Call<pfn>);
		if constexpr(instance_index < T::max_instances - 1)
				return getFunc<pfn, instance_index + 1>(i);
		return nullptr;
	}

};

// Thunk template usage:
// define max_instances and mInstances array in target class
// use GET_WRAPPER(CLASS, METHOD, N) to generate function pointer for N-th instance
#define GET_WRAPPER(Type, Method, i) FunctionPointerGenerator(&Type::Method).getFunc<&Type::Method>(i)

//Create Layer context for XrInstance
bool CreateLayerInstance(XrInstance instance, PFN_xrGetInstanceProcAddr gpa, const char **exts, uint32_t extcount)
{
	int i = Layer::FindInstance(instance);
	if( i < 0 )
		i = Layer::FindInstance(XR_NULL_HANDLE);
	if(i < 0)
	{
		// Out of instances, refuse to create layer
		delete[] exts;
		return false;
	}
	Layer &lInstance = Layer::mInstances[i];
	lInstance.Initialize(instance, gpa, exts, extcount);
	return true;
}



//This function gets overrides from Layer object and selects correct instance
XrResult thisLayer_xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function)
{
	int i = Layer::FindInstance(instance);
	if(i < 0)
	{
		// no instance? CreateLayerInstance failed?
		// do not have nextLayer_xrGetInstanceProcAddr, cannot continue
		*function = nullptr;
		return XR_ERROR_HANDLE_INVALID;
	}
	// This actually generates function wrappers
	// Get wrapper for method thisLayer_x in Layer object passing Layer::mInstances[i] as this
	// Function types are got from method declarations
#define WRAP_FUNC(x) \
	do { \
		if(!strcmp(name, #x)) \
		{ \
			/* this will fail if method declared with wrong signature */ \
			PFN_##x pFunc = GET_WRAPPER(Layer,thisLayer_##x,i); \
			*function = (PFN_xrVoidFunction)pFunc; \
			return XR_SUCCESS; \
		} \
	} while(0)

	//Need this to free instance correctly
	WRAP_FUNC(xrDestroyInstance);

	//List every functions that should be overriden
	WRAP_FUNC(xrCreateAction);
	WRAP_FUNC(xrCreateActionSet);
	WRAP_FUNC(xrAttachSessionActionSets);
	WRAP_FUNC(xrPollEvent);
	WRAP_FUNC(xrDestroySession);
	WRAP_FUNC(xrCreateSession);
	WRAP_FUNC(xrGetActionStateBoolean);
	WRAP_FUNC(xrGetActionStateFloat);
	WRAP_FUNC(xrGetActionStateVector2f);
	WRAP_FUNC(xrSyncActions);
	WRAP_FUNC(xrWaitFrame);
	WRAP_FUNC(xrSuggestInteractionProfileBindings);

#if XR_THISLAYER_HAS_EXTENSIONS
	if(Layer::mInstances[i].IsExtensionEnabled("XR_TEST_test_me"))
	{
		WRAP_FUNC(xrTestMeTEST);
	}
#endif

	//Not wrapped? Chain call
	return Layer::mInstances[i].nextLayer_xrGetInstanceProcAddr(instance, name,function);
}
