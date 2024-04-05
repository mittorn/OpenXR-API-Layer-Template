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
#include <unistd.h>
#include <fcntl.h>
#include "layer_utl.h"
#include "config_shared.h"
#include "layer_events.h"
#include "rpn_calc.h"

//Define next function pointer
#define DECLARE_NEXT_FUNC(x) PFN_##x nextLayer_##x
//Load next function pointer
#define LOAD_NEXT_FUNC(x) nextLayer_xrGetInstanceProcAddr(mInstance, #x, (PFN_xrVoidFunction*)&nextLayer_##x)

static float GetBoolAction(void *priv, int act, int hand, int ax);
static float GetFloatAction(void *priv, int act, int hand, int ax);
static float GetVec2Action(void *priv, int act, int hand, int ax);
static float GetExtAction(void *priv, int act, int hand, int ax);
static float GetRPNAction(void *priv, int act, int hand, int ax);
constexpr float (*actionFuncs[])(void *priv, int act, int hand, int ax) =
{
	GetBoolAction,
	GetFloatAction,
	GetVec2Action,
	GetExtAction,
	GetRPNAction
};

struct ActionSource
{
	int actionIndex;
	int funcIndex;
	int axisIndex;
	int handIndex;
	void *priv;
	float GetValue() const
	{
		return actionFuncs[funcIndex](priv, actionIndex, handIndex, axisIndex);
	}
};

static bool GetSource( void *priv, ActionSource &s, const SubStr &src );
static float *GetVar(void *priv, const SubStr &v );
struct RPNToken
{
	enum
	{
		val,
		op,
		func,
		var,
		act
	} mode;
	union
	{
		float val;
		char ch[2];
		int funcindex;
		float *var;
		ActionSource src;
	} d;
	RPNToken(){}
	RPNToken(void *priv, const char *begin, const char *end, bool _op)
	{
		SubStr tok{begin,end};
		if(tok.Equals("sin") || tok.Equals("func3"))
		{
			mode = func;
			d.funcindex = *begin == 'f';
		}
		else if(_op)
		{
			mode = op;
			strncpy(d.ch,begin, 2);
			if(end < begin + 2)
				d.ch[end - begin] = 0;
		}
		else if(*begin == '$')
		{
			mode = var;
			d.var = GetVar( priv, tok.GetSubStr(1));
		}
		else
		{
			if( GetSource( priv, d.src, tok))
				mode = act;
			else
			{
				mode = val;
				d.val = atof(tok.begin);
			}
		}
	}
	RPNToken(float v)
	{
		mode = val;
		d.val = v;
	}
	char Op() const
	{
		return mode == op ? d.ch[0]:0;
	}
	bool IsOperator() const
	{
		return (mode == op) && CalcIsOp(Op());
	}
	bool IsIdent() const
	{
		return mode == val || mode == var || mode == act;
	}
	bool IsFunction() const
	{
		return mode == func;
	}
	void Stringify(char *buf, size_t len) const
	{
		if(len < 2)
			return;
		if(mode == op)
		{
			strncpy(buf, d.ch, 2);
			buf[2] = 0;
		}
		else if(mode == val)
		{
			snprintf(buf, len - 1, "%f",(double)Val());
		}
		else if(mode == func)
		{
			snprintf(buf, len - 1, "func%d", d.funcindex);
		}
		else if(mode == var)
		{
			snprintf(buf, len - 1, "$%llu", (long long unsigned int)d.var);
		}
		else if(mode == act)
		{
			snprintf(buf, len - 1, "a_%d_%d_%d_%d", d.src.funcIndex, d.src.actionIndex, d.src.handIndex, d.src.axisIndex);
		}
	}
	int ArgCount() const
	{
		if(mode == func)
			return d.funcindex?3:1;
		if(mode != op)
			return 0;
		return CalcArgCount(d.ch[0], d.ch[1]);
	}
	float Val() const
	{
		if(mode == val)
			return d.val;
		if(mode == act)
			return d.src.GetValue();
		if(mode == var)
			return *d.var;
		return 0;
	}
#define StackPop(var) auto var = stack[--sp].Val()
#define StackPush(val) stack[sp++] = val
	template<size_t stacksize>
	bool Calculate(RPNToken (&stack)[stacksize], size_t &sp)
	{
		int ac = ArgCount();
		if(sp < ac)
			return false;
		if(mode == func)
		{
			if(d.funcindex == 0)
			{
				StackPop(val1);
				StackPush(sinf(val1));
				return true;
			}
			if(d.funcindex == 1)
			{
				StackPop(val3);
				StackPop(val2);
				StackPop(val1);
				StackPush(val1 + val2 + val3);
				return true;
			}
		}
		else if(mode == op)
		{
			if(ac == 2)
			{
				if(d.ch[0] == '=' && d.ch[1] != '=' && stack[sp - 2].mode == var)
				{
					StackPop(val2);
					*stack[sp - 1].d.var = val2;
					return true;
				}
				StackPop(val2);
				StackPop(val1);
				float res = CalcOp2(val1, val2, Op(), d.ch[1]);
				StackPush(res);
				return true;
			}
			if(ac == 1)
			{
				StackPop(val1);
				if(Op() == '!')
				{
					StackPush(!val1);
					return true;
				}
				else return false;
			}
		}

		return false;
	}
#undef StackPop
#undef StackPush
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
	struct RPNInstance
	{
		GrowArray<RPNToken> data;
	};

	struct SessionState
	{
		XrSession mSession = XR_NULL_HANDLE;
		XrSessionCreateInfo info = { XR_TYPE_UNKNOWN };
		XrActionSet *mActionSets = nullptr;
		size_t mActionSetsCount = 0;
		Layer *mpInstance;

		// application actions
		HashArrayMap<XrAction, ActionBoolean> mActionsBoolean;
		HashArrayMap<XrAction, ActionFloat> mActionsFloat;
		HashArrayMap<XrAction, ActionVec2> mActionsVec2;

		// layer actions
		GrowArray<ActionBoolean> mLayerActionsBoolean;
		GrowArray<ActionFloat> mLayerActionsFloat;
		GrowArray<ActionVec2> mLayerActionsVec2;

		// need to dynamicly inject new sources
		HashArrayMap<SubStr, int> mBoolIndexes;
		HashArrayMap<SubStr, int> mFloatIndexes;
		HashArrayMap<SubStr, int> mVec2Indexes;

		HashArrayMap<SubStr, float[2], 0> mExternalSources;

		HashMap<SubStr, RPNInstance> mRPNs;
		GrowArray<RPNInstance*> mRPNPointers;

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
	HashArrayMap<SubStr, int> mLayerActionIndexes;
	HashMap<XrPath, GrowArray<XrActionSuggestedBinding>> mLayerSuggestedBindings;
	bool mfLayerActionSetSuggested = false;

	HashMap<SubStr, float> mRPNVariables;

	HashMap<XrSession, SessionState> mSessions;
	SessionState *mpActiveSession;
	// must be session-private, but always need most recent
	XrTime mPredictedTime;
	bool mTriggerInteractionProfileChanged, mTriggerInteractionProfileChangedOld;


	XrResult noinline thisLayer_xrCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *session)
	{
		INSTANCE_FALLBACK(thisLayer_xrCreateSession(instance, createInfo, session));
		XrResult res = nextLayer_xrCreateSession(instance, createInfo, session);
		if(res == XR_SUCCESS)
		{
			SessionState &w = GetSession(*session);
			w.info = *createInfo;
			w.mpInstance = this;
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
		const char *profile = config.interactionProfile;
		if(!profile)
			profile = "/interaction_profiles/khr/simple_controller";
		nextLayer_xrStringToPath(mInstance, profile, &defaultProfile);
		mfLayerActionSetSuggested = false;
		HASHMAP_FOREACH(config.sources.mSections, node)
		{
			if(node->v.actionType < SourceSection::action_external)
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
				SBPrint(info.localizedActionName, "Layer: %s", node->v.h.name);
				node->v.h.name.CopyTo(info.actionName);

				nextLayer_xrCreateAction(mhLayerActionSet, &info, &act.action);

				SubStr s = node->v.bindings.val;
				mLayerActionIndexes[node->v.h.name] = mLayerActionSet.mActions.count - 1;

				if(!s.begin)
				{
					Log("Missing bindings for source %s\n", node->v.h.name);
					continue;
				}
				do
				{
					XrPath path;
					XrPath profile = defaultProfile;
					SubStr n, s1;

					if(s.Split2(n, s1, ':'))
					{
						char pr[n.Len() + 1];
						n.CopyTo(pr,n.Len() + 1);
						nextLayer_xrStringToPath(mInstance, pr, &profile);
						s = s1;
					}
					if(!s.Split2(n, s1, ','))
						n = s, s = "";
					else
						s = s1;
					char bnd[n.Len() + 1];
					n.CopyTo(bnd, n.Len() + 1);
					nextLayer_xrStringToPath(mInstance, bnd, &path);
					mLayerSuggestedBindings[profile].Add({act.action,path});
				} while(s.Len());
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
			return USER_HEAD + _FindPath2(&mUserPaths[USER_HEAD], USER_PATH_COUNT - USER_HEAD, p);
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
				//if(hand.map.actionIndex < 0)
				{
					// todo: detect change
					state.changedSinceLastSync = true;
					state.lastChangeTime = mPredictedTime;
				}
			}
		}
	}
	Action *FindAppSessionAction(SessionState &w, const char *name)
	{
		HASHMAP_FOREACH(w.mActionsBoolean, n)
			if(!strcmp(n->v.info.actionName, name))
				return &n->v;
		HASHMAP_FOREACH(w.mActionsFloat, n)
			if(!strcmp(n->v.info.actionName, name))
				return &n->v;
		HASHMAP_FOREACH(w.mActionsVec2, n)
			if(!strcmp(n->v.info.actionName, name))
				return &n->v;

		return nullptr;
	}

	forceinline void ProcessExternalActions(SessionState &w)
	{
		if( unlikely( poller.pollLock.TryLock()))
		{
			PollEvent ev;
			while( poller.pollEvents.Dequeue( ev ))
			{
				switch ( ev.type )
				{
				case EVENT_POLL_DUMP_APP_BINDINGS:
					for(int i = 0; i < w.mActionSetsCount; i++)
						DumpActionSet(mActiveSession, gActionSetInfos[w.mActionSets[i]]);
					break;
				case EVENT_POLL_RELOAD_CONFIG:
						config.~Config();
						LoadConfig(&config);
						InitProfile(w, config.startupProfile.ptr);
					break;
				case EVENT_POLL_SET_PROFILE:
						InitProfile(w, &config.bindings.mSections[SubStrB(ev.str1)]);
					break;
				case EVENT_POLL_MAP_ACTION:
					{
						Action *a = FindAppSessionAction(w,ev.str1);
						ActionMapSection *s = config.actionMaps.mSections.GetPtr(SubStrB(ev.str2));
						if(a && s)
							ApplyActionMap(w, *a, s, ev.i1 );
					}
					break;
				case EVENT_POLL_RESET_ACTION:
					{
						Action *a = FindAppSessionAction(w,ev.str1);
						if(a)
						{
							for(int i = 0; i < USER_PATH_COUNT; i++)
							{
								ActionMapSection *s = config.startupProfile.ptr->actionMaps.maps[i][SubStrB(a->info.actionName)];
								if(s)
									ApplyActionMap(w, *a, s, i);
							}
						}
					}
					break;
				case EVENT_POLL_SET_EXTERNAL_SOURCE:
						// todo: add float? Use union?
						w.mExternalSources[SubStrB(ev.str1)][ev.i1] = ev.f1;
					break;
				case EVENT_POLL_TRIGGER_INTERACTION_PROFILE_CHANGED: // todo: move to xrPollEvents? Separate queue?
					mTriggerInteractionProfileChanged = true;
					mTriggerInteractionProfileChangedOld = false;
					break;
				case EVENT_POLL_DUMP_LAYER_BINDINGS:
					DumpActionSet(w.mSession, mLayerActionSet);
					break;
				case EVENT_POLL_MAP_DIRECT_SOURCE:
					{
						Action *a = FindAppSessionAction(w,ev.str1);
						SubStr ss = SubStrB(ev.str2);
						SubStr base, suf;
						if(!ss.Split2(base, suf, '.'))
							base = ss, suf = "";
						if(a)
						{
							SourceSection *s = config.sources.mSections.GetPtr(base);
							if((int)s->actionType == (int)a->info.actionType)
								a->baseState[ev.i1].map.actionIndex = AddSourceToSession(w, a->info.actionType, s->h.name );
							a->baseState[ev.i1].map.handIndex = HandFromConfig(*s, suf);
						}
					}
				break;
				case EVENT_POLL_MAP_AXIS:
				{
						Action *a = FindAppSessionAction(w,ev.str1);
						if(a)
							AxisFromConfig(*a, ev.i1, SubStrB(ev.str2), ev.f1, w);
				}
				break;
				default:
					break;
				}
			}
			poller.pollLock.Unlock();
		}
	}

	forceinline XrResult thisLayer_xrSyncActions(XrSession session, const XrActionsSyncInfo *syncInfo)
	{
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
		ProcessExternalActions(*mpActiveSession);
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
	void DumpActionSet(XrSession session, ActionSet &seti)
	{
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
	int AddSourceToSessionT( HashArrayMap<SubStr, int> &indexMap, GrowArray<AT>& array, const SubStr &name )
	{
		int &idx = indexMap[name];
		if(!idx)
		{
			idx = array.count + 1;
			array.Add({mLayerActionSet.mActions[mLayerActionIndexes[name]]});
		}
		return idx - 1;
	}

	int AddSourceToSession( SessionState &w, XrActionType t, const SubStr &name )
	{
		if(t == XR_ACTION_TYPE_BOOLEAN_INPUT)
			return AddSourceToSessionT( w.mBoolIndexes, w.mLayerActionsBoolean, name );
		else if(t == XR_ACTION_TYPE_FLOAT_INPUT)
			return AddSourceToSessionT( w.mFloatIndexes, w.mLayerActionsFloat, name );
		else if(t == XR_ACTION_TYPE_VECTOR2F_INPUT)
			return AddSourceToSessionT( w.mVec2Indexes, w.mLayerActionsVec2, name );
		return -1;
	}

	int HandFromConfig(const SourceSection &c, const SubStr &suffix)
	{
		if(suffix.Len())
			return PathIndexFromSuffix(suffix);
		else if(!(const char*)c.subactionOverride)
			return 1;
		XrPath p;
		nextLayer_xrStringToPath(mInstance, c.subactionOverride, &p);
		return FindPath(p);
	}

	int AddExternalSource(SessionState &w, const SubStr &name)
	{
		auto *n = w.mExternalSources.GetOrAllocate(name);
		return n - w.mExternalSources.table[0].mem;
	}
	SourceSection *SourceFromConfig(const SubStr &name, int &hand)
	{
		SubStr n, s;
		if(name.Split2(n,s,'.'))
		{
			hand = PathIndexFromSuffix(s);
			if(hand == USER_INVALID)
				return  nullptr;
		}
		else
		{
			hand = USER_INVALID;
			n = name;
		}

		SourceSection *c = config.sources.mSections.GetPtr(n);
		if(c && hand == USER_INVALID)
			hand = HandFromConfig(*c, "");
		return c;
	}
	bool FillSource(ActionSource &s, SessionState &w, SourceSection *c, const SubStr &mapping )
	{
		int t = c->actionType;
		bool ret = true;
		if( t < SourceSection::action_external)
			s.actionIndex = AddSourceToSession( w, (XrActionType)t, c->h.name );
		else
			s.actionIndex = AddExternalSource( w, c->h.name );
		if(s.actionIndex < 0)
			ret = false, t = 1, s.actionIndex = 0;
		s.funcIndex = t - 1;
		s.priv = &w;
		s.axisIndex = mapping.Contains( "[1]");
		return ret;
	}

	void AxisFromConfig(Action &a, int hand, const SubStr &mapping, int axis, SessionState &w)
	{
		SourceSection *c = SourceFromConfig(mapping, a.baseState[hand].map.src[axis].handIndex);
		if(!c)
		{
			GrowArray<RPNToken> tokens;
			if(!ParseTokens(&w, tokens, mapping.begin, mapping.Len()))
				return;
			RPNInstance &inst = w.mRPNs[mapping];
			if(!ShuntingYard(tokens, inst.data))
				return;
			a.baseState[hand].map.src[axis].actionIndex = w.mRPNPointers.count;
			a.baseState[hand].map.src[axis].funcIndex = 4;
			a.baseState[hand].map.src[axis].priv = &w;
			w.mRPNPointers.Add(&inst);
			a.baseState[hand].hasAxisMapping = true;
			return;
		}

		if(!FillSource( a.baseState[hand].map.src[axis], w, c, mapping))
			Log( "Invalid action type: axis%d  %s %s\n", axis, mapping, c->h.name );
		else
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
				Log( "Invalid direct action map (types must be same): %s %s %s %d\n", s->h.name, s->map.suffix.begin? s->map.suffix.begin: "(auto)", s->map.ptr->h.name, hand );
			a.baseState[hand].map.handIndex = HandFromConfig(*s->map.ptr, s->map.suffix);
		}

		if(s->axis1)
			AxisFromConfig(a, hand, s->axis1.val, 0, w);
		if(s->axis2)
			AxisFromConfig(a, hand, s->axis2.val, 1, w);
	}

	void InitProfile(SessionState &w, BindingProfileSection *p)
	{
		w.mActionsBoolean.Clear();
		w.mActionsFloat.Clear();
		w.mActionsVec2.Clear();
		w.mLayerActionsBoolean.Clear();
		w.mLayerActionsFloat.Clear();
		w.mLayerActionsVec2.Clear();
		w.mBoolIndexes.Clear();
		w.mFloatIndexes.Clear();
		w.mVec2Indexes.Clear();
		for(int i = 0; i < w.mActionSetsCount; i++)
		{
			ActionSet &set = gActionSetInfos[w.mActionSets[i]];
			for(int j = 0; j < set.mActions.count; j++)
			{
				Action &a = set.mActions[j];
				for(int i = 0; i < USER_PATH_COUNT; i++)
				{
					ActionMapSection *s = p->actionMaps.maps[i][SubStrB(a.info.actionName)];
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
				ActionSet &seti = gActionSetInfos[s];
				Log("Attached action set: %s %s\n", seti.info.actionSetName, seti.info.localizedActionSetName );
				DumpActionSet(session,seti);
			}
			InitProfile(w, config.startupProfile.ptr);
		}
		return r;
	}

	forceinline XrResult thisLayer_xrPollEvent(
		XrInstance instance, XrEventDataBuffer* eventData)
	{
		//INSTANCE_FALLBACK(thisLayer_xrPollEvent(instance, eventData));
		if(unlikely(mTriggerInteractionProfileChanged > mTriggerInteractionProfileChangedOld))
		{
			mTriggerInteractionProfileChangedOld = mTriggerInteractionProfileChanged;
			eventData->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
			return XR_SUCCESS;
		}
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
				{
					ActionSet &seti = gActionSetInfos[w->mActionSets[i]];
					Log("Session action set: %s %s\n", seti.info.actionSetName, seti.info.localizedActionSetName );
					DumpActionSet(mActiveSession, seti);
				}
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

static float GetBoolAction(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return w->mLayerActionsBoolean[act].typedState[hand].currentState;
}

static float GetFloatAction(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return w->mLayerActionsFloat[act].typedState[hand].currentState;
}

static float GetVec2Action(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return ax?w->mLayerActionsVec2[act].typedState[hand].currentState.y
			: w->mLayerActionsVec2[act].typedState[hand].currentState.x;
}

static float GetExtAction(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return w->mExternalSources.table[0][act].v[ax];
}

static float GetRPNAction(void *priv, int act, int hand, int ax)
{
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return Calculate(w->mRPNPointers[act]->data);
}
static bool GetSource( void *priv, ActionSource &s, const SubStr &src )
{
	if(!priv)
		return false;
	Layer::SessionState *w = (Layer::SessionState *)priv;
	SourceSection *c = w->mpInstance->SourceFromConfig(src, s.handIndex);
	if(!c)
		return false;
	return w->mpInstance->FillSource(s, *w, c, src);
}
static float *GetVar( void *priv, const SubStr &v )
{
	if(!priv)
		return (float*)(uint64_t)atoi(v.begin);
	Layer::SessionState *w = (Layer::SessionState *)priv;
	return &w->mpInstance->mRPNVariables[v];
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
