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
#define Log(...) FPrint(stderr, __VA_ARGS__)

//Define next function pointer
#define DECLARE_NEXT_FUNC(x) PFN_##x nextLayer_##x
//Load next function pointer
#define LOAD_NEXT_FUNC(x) nextLayer_xrGetInstanceProcAddr(mInstance, #x, (PFN_xrVoidFunction*)&nextLayer_##x)
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
	SectionHeader_ h

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
	SectionReference_(const SectionReference_ &other) = delete;
	SectionReference_& operator=(const SectionReference_ &) = delete;
	SectionReference_(){}
	SectionReference_(ConfigLoader &l)
	{
		const char *&str = (*l.CurrentSection)[name];
		if(str)
		{
			char sectionName[256];
			SBPrint(sectionName, "[%s.%s]", S::prefix, str);
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
	SectionReference_<type, opt_name_##name> name


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
	Option_<type, opt_name_##name> name

template <const auto &NAME>
struct StringOption_
{
	constexpr static const char *name = NAME;
	char val[256] = "";
	operator const char *()
	{
		return val;
	}
	StringOption_(const StringOption_ &other) = delete;
	StringOption_& operator=(const StringOption_ &) = delete;
	StringOption_(){}
	StringOption_(const char *def)
	{
		strncpy(val, def, sizeof(val)-1);
	}
	StringOption_(ConfigLoader &l){
		const char *&str = (*l.CurrentSection)[name];
		if(str)
			strncpy(val, str, sizeof(val)-1);
		str = nullptr;
	}
};

#define StringOption(name) \
constexpr static const char *opt_name_##name = #name; \
	StringOption_<opt_name_##name> name

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
		while(*scheme == *pval)scheme++, pval++;
		if(IsDelim(*scheme) && !*pval)
			return index;
		if(!*scheme)
			return 0;
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
	EnumOption_<name ## _enum, name ## _name, name ## _scheme> name


struct SourceSection
{
	SectionHeader(source);
	EnumOption(sourceType, remap, server);
	StringOption(path);
	Option(float, minIn);
	Option(float, maxIn);
	Option(float, minOut);
	Option(float, maxOut);
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
	SectionReference(SourceSection, axis3);
	EnumOption(customAction, reloadSettings, changeProfile, triggerInteractionChange);
	SectionReference(BindingProfileSection, profileName);
};

struct BindingProfileSection
{
	SectionHeader(bindings);
	StringOption(overrideInteractionProfile);
	struct DynamicActionMaps {
		HashMap<const char *, ActionMapSection*> maps;
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
					maps[l.CurrentSection->table[i][j].k] = map;
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
	StringOption(overrideInteractionProfile);
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
	Log("OverrideInteractionProfile %s\n", (const char*)c->overrideInteractionProfile);
}

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

	enum eUserPaths{
		USER_HAND_LEFT = 0,
		USER_HAND_RIGHT,
		USER_HEAD,
		USER_GAMEPAD,
		USER_INVALID,
		USER_PATH_COUNT
	};

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
	NEXT_FUNC(f, xrWaitFrame);

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
		//SourceSection *mpConfig;
		void *priv;
		float (*func)(void *priv);
		float GetValue()
		{
			if(func)
				return func(priv);
			return 0;
		}
	};

	struct ActionMap
	{
		//ActionMapSection *mpConfig;
		ActionSource src[3];
		float GetAxis(int axis)
		{
			// todo: actual axis mapping and calculations should be done here?
			return src[axis].GetValue();
		}
	};

	struct ActionState
	{
		bool ignoreDefault = false;
		bool trigger = false;
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
		XrActionStateBoolean boolState[USER_PATH_COUNT];
		void Update(int hand)
		{
			boolState[hand].currentState = baseState[hand].map.GetAxis(0);
		}
	};

	struct ActionFloat : Action
	{
		XrActionStateFloat floatState[USER_PATH_COUNT];
		void Update(int hand)
		{
			floatState[hand].currentState = baseState[hand].map.GetAxis(0);
		}
	};

	struct ActionVec2 : Action
	{
		XrActionStateVector2f vec2State[USER_PATH_COUNT];
		void Update(int hand)
		{
			vec2State[hand].currentState.x = baseState[hand].map.GetAxis(0);
			vec2State[hand].currentState.y = baseState[hand].map.GetAxis(1);
		}
	};

	//HashMap<XrAction, Action> mActionsOther;

	struct SessionState
	{
		XrSession mSession = XR_NULL_HANDLE;
		XrSessionCreateInfo info = { XR_TYPE_UNKNOWN };
		XrActionSet *mActionSets = nullptr;
		size_t mActionSetsCount = 0;
		HashArrayMap<XrAction, ActionBoolean> mActionsBoolean;
		HashArrayMap<XrAction, ActionFloat> mActionsFloat;
		HashArrayMap<XrAction, ActionVec2> mActionsVec2;
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
	Config config = {};

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

			// test: animate grab_object action
			bool ignore = !strcmp(info->actionName, "grab_object");
			if(!gActionSetInfos[actionSet].mActions.Add({*info, act, {{false, false}, {ignore, false}}}))
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
			as.instance = instance;
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
				if(!hand.ignoreDefault)
					r = nextLayer_xrGetActionStateBoolean(session, getInfo, state);
				else
					*state = a->boolState[handPath];
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
				if(!hand.ignoreDefault)
					r = nextLayer_xrGetActionStateFloat(session, getInfo, state);
				else
					*state = a->floatState[handPath];
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
				if(!hand.ignoreDefault)
					r = nextLayer_xrGetActionStateVector2f(session, getInfo, state);
				else
					*state = a->vec2State[handPath];
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

	forceinline XrResult thisLayer_xrSyncActions(XrSession session, const XrActionsSyncInfo *syncInfo)
	{
		XrResult ret = nextLayer_xrSyncActions(session, syncInfo);
		if(unlikely(!mpActiveSession && mActiveSession == session))
		{
			mpActiveSession = mSessions.GetPtr(session);
			if(!mpActiveSession)
				return ret;
			mActiveSession = session;
		}
		for(int i = 0; i < mpActiveSession->mActionsBoolean.TblSize; i++)
		{
			for(int j = 0; j < mpActiveSession->mActionsBoolean.table[i].count;j++)
			{
				ActionBoolean &a = mpActiveSession->mActionsBoolean.table[i][j].v;
				for(int handPath = 0; handPath < USER_PATH_COUNT; handPath++)
				{
					ActionState &hand = a.baseState[handPath];
					if(hand.ignoreDefault)
					{
						XrActionStateBoolean &state = a.boolState[handPath];
						a.Update(handPath);
						state.changedSinceLastSync = true;
						state.lastChangeTime = mPredictedTime;
					}
				}
			}
		}
		for(int i = 0; i < mpActiveSession->mActionsFloat.TblSize; i++)
		{
			for(int j = 0; j < mpActiveSession->mActionsFloat.table[i].count;j++)
			{
				ActionFloat &a = mpActiveSession->mActionsFloat.table[i][j].v;
				for(int handPath = 0; handPath < USER_PATH_COUNT; handPath++)
				{
					ActionState &hand = a.baseState[handPath];
					if(hand.ignoreDefault)
					{
						XrActionStateFloat &state = a.floatState[handPath];
						a.Update(handPath);
						state.changedSinceLastSync = true;
						state.lastChangeTime = mPredictedTime;
					}
				}
			}
		}
		for(int i = 0; i < mpActiveSession->mActionsVec2.TblSize; i++)
		{
			for(int j = 0; j < mpActiveSession->mActionsVec2.table[i].count;j++)
			{
				ActionVec2 &a = mpActiveSession->mActionsVec2.table[i][j].v;
				for(int handPath = 0; handPath < USER_PATH_COUNT; handPath++)
				{
					ActionState &hand = a.baseState[handPath];
					if(hand.ignoreDefault)
					{
						XrActionStateVector2f &state = a.vec2State[handPath];
						a.Update(handPath);
						state.changedSinceLastSync = true;
						state.lastChangeTime = mPredictedTime;
					}
				}
			}
		}
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
					nextLayer_xrPathToString(seti.instance, paths[j], 0, &slen, NULL);
					{
						char str[slen + 1];
						nextLayer_xrPathToString(seti.instance, paths[j], slen + 1, &slen, str);
						Log("raw_path %s\n", (char*)str);
					}
				}
			}
		}
	}

	XrResult noinline thisLayer_xrAttachSessionActionSets (XrSession session, const XrSessionActionSetsAttachInfo *info)
	{
		XrResult r = nextLayer_xrAttachSessionActionSets(session, info);
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
				ActionSet &set = gActionSetInfos[s];
				for(int j = 0; j < set.mActions.count; j++)
				{
					Action &a = set.mActions[j];
					switch(a.info.actionType)
					{
						case XR_ACTION_TYPE_BOOLEAN_INPUT:
						{
							ActionBoolean &ab = w.mActionsBoolean[a.action];
							*(Action*)&ab = a;
							for(int i = 0; i < USER_PATH_COUNT; i++)
							{
								ab.boolState[i].type = XR_TYPE_ACTION_STATE_BOOLEAN;
								ab.boolState[i].isActive = true;
							}
							break;
						}
						case XR_ACTION_TYPE_FLOAT_INPUT:
						{
							ActionFloat &af = w.mActionsFloat[a.action];
							*(Action*)&af = a;
							for(int i = 0; i < USER_PATH_COUNT; i++)
							{
								af.floatState[i].type = XR_TYPE_ACTION_STATE_FLOAT;
								af.floatState[i].isActive = true;
							}
							break;
						}
						case XR_ACTION_TYPE_VECTOR2F_INPUT:
						{
							ActionVec2 &af = w.mActionsVec2[a.action];
							*(Action*)&af = a;
							for(int i = 0; i < USER_PATH_COUNT; i++)
							{
								af.vec2State[i].type = XR_TYPE_ACTION_STATE_VECTOR2F;
								af.vec2State[i].isActive = true;
							}
							break;
						}
						default:
							break;
					}
				}
			}

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

#if XR_THISLAYER_HAS_EXTENSIONS
	if(Layer::mInstances[i].IsExtensionEnabled("XR_TEST_test_me"))
	{
		WRAP_FUNC(xrTestMeTEST);
	}
#endif

	//Not wrapped? Chain call
	return Layer::mInstances[i].nextLayer_xrGetInstanceProcAddr(instance, name,function);
}
