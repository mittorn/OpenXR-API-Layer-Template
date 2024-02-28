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

//Define next function pointer
#define DECLARE_NEXT_FUNC(x) PFN_##x nextLayer_##x
//Load next function pointer
#define LOAD_NEXT_FUNC(x) nextLayer_xrGetInstanceProcAddr(mInstance, #x, (PFN_xrVoidFunction*)&nextLayer_##x)


struct Layer
{
	// maximum active instances loaded in class
	// normally it should be 1, but reserve more in case some library checks OpenXR
	constexpr static size_t max_instances = 4;
	static Layer mInstances[max_instances];

	// Only handle this XrInatance in this object
	XrInstance mInstance = XR_NULL_HANDLE;
	XrSession mActiveSession = XR_NULL_HANDLE;

	// Extensions list
	const char **mExtensions;
	uint32_t mExtensionsCount;

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
	NEXT_FUNC(f, xrGetActionStateFloat);

	NEXT_FUNC_LIST(DECLARE_NEXT_FUNC);

	// Find Layer index for XrInstance
	static int FindInstance(XrInstance inst)
	{
		for(int i = 0; i < max_instances; i++) if(mInstances[i].mInstance == inst) return i;
		return -1;
	}

#define INSTANCE_FALLBACK(call) \
	if( instance != mInstance ) \
	{ \
		int i = FindInstance(instance);\
		if(i >= 0) \
			return mInstances[i].call; \
		return XR_ERROR_HANDLE_INVALID; \
	}
	struct Action
	{
		XrActionCreateInfo info = { XR_TYPE_UNKNOWN };
		bool ignoreDefault = false;
		bool trigger = false;
	};

	HashMap<XrAction, Action> gActionInfos;

	struct SessionState
	{
		XrSession mSession = XR_NULL_HANDLE;
		XrSessionCreateInfo info = { XR_TYPE_UNKNOWN };
		XrActionSet *mActionSets = nullptr;
		size_t mActionSetsCount = 0;
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
		GrowArray<XrAction> mActions;
	};
	HashMap<XrActionSet, ActionSet> gActionSetInfos;

	HashMap<XrSession, SessionState> mSessions;
	SessionState *mpActiveSession;

	XrResult thisLayer_xrCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *session)
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
	XrResult thisLayer_xrDestroySession(XrSession session)
	{
		if(mActiveSession == session)
		{
			mActiveSession = XR_NULL_HANDLE;
			mpActiveSession = nullptr;
		}
		mSessions.Remove(session);
		return nextLayer_xrDestroySession(session);
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
	}

	// Only need this if extensions used
	bool IsExtensionEnabled(const char *extension)
	{
		for(int i = 0; i < mExtensionsCount; i++)
			if(!strcmp(extension, mExtensions[i]))
				return true;
		return false;
	}
	XrResult thisLayer_xrCreateAction (XrActionSet actionSet, const XrActionCreateInfo *info, XrAction *action)
	{
		XrAction act;
		XrResult r = nextLayer_xrCreateAction(actionSet, info, &act);
		if(r == XR_SUCCESS)
		{
			*action = act;
			// test: animate grab_object action
			bool ignore = !strcmp(info->actionName, "grab_object");
			gActionInfos[act] = {*info, ignore, false};
			if(!gActionSetInfos[actionSet].mActions.Add(act))
			{
				nextLayer_xrDestroyAction(act);
				return XR_ERROR_OUT_OF_MEMORY;
			}
		}
		return r;
	}
	XrResult thisLayer_xrCreateActionSet (XrInstance instance, const XrActionSetCreateInfo *info, XrActionSet *actionSet)
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

	XrResult thisLayer_xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateBoolean *state)
	{
		Action *a = gActionInfos.GetPtr(getInfo->action);
		if(a)
		{
			XrResult r = XR_SUCCESS;
			if(!a->ignoreDefault)
				r = nextLayer_xrGetActionStateBoolean(session, getInfo, state);
			else
			{
				static bool chg = false;
				chg = !chg;
				state->type = XR_TYPE_ACTION_STATE_BOOLEAN;
				state->currentState = chg;
				state->isActive = true;
				state->changedSinceLastSync = true;
				state->lastChangeTime = 0; // todo: xrSyncActions
			}
			return r;
		}
		return nextLayer_xrGetActionStateBoolean(session, getInfo, state);
	}

	XrResult thisLayer_xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo *getInfo, XrActionStateFloat *state)
	{
		Action *a = gActionInfos.GetPtr(getInfo->action);
		if(a)
		{
			XrResult r = XR_SUCCESS;
			if(!a->ignoreDefault)
				r = nextLayer_xrGetActionStateFloat(session, getInfo, state);
			else
			{
				static float chg = 0;
				chg = fmodf(chg + 0.01f,1.0f);
				state->type = XR_TYPE_ACTION_STATE_FLOAT;
				state->currentState = chg;
				state->isActive = true;
				state->changedSinceLastSync = true;
				state->lastChangeTime = 0; // todo: xrSyncActions
			}
			return r;
		}
		return nextLayer_xrGetActionStateFloat(session, getInfo, state);
	}
	void DumpActionSet(XrSession session, XrActionSet s)
	{
		ActionSet &seti = gActionSetInfos[s];
		printf("Attached action set: %s %s\n", seti.info.actionSetName, seti.info.localizedActionSetName );
		for(int i = 0; i < seti.mActions.count; i++ )
		{
			XrAction a = seti.mActions[i];
			Action &as = gActionInfos[a];
			XrActionCreateInfo &cinfo = as.info;
			printf("info %p: %s %s\n", (void*)a, cinfo.actionName, cinfo.localizedActionName);
			XrBoundSourcesForActionEnumerateInfo einfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
			einfo.action = a;
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
						printf("Description %s\n", str);
					}
					nextLayer_xrPathToString(seti.instance, paths[j], 0, &slen, NULL);
					{
						char str[slen + 1];
						nextLayer_xrPathToString(seti.instance, paths[j], slen + 1, &slen, str);
						printf("raw_path %s\n", str);
					}
				}
			}
		}
	}

	XrResult thisLayer_xrAttachSessionActionSets (XrSession session, const XrSessionActionSetsAttachInfo *info)
	{
		XrResult r = nextLayer_xrAttachSessionActionSets(session, info);
		if(r == XR_SUCCESS)
		{
			SessionState &w = GetSession(session);
			delete[] w.mActionSets;
			w.mActionSets = new XrActionSet[info->countActionSets];

			for(int i = 0; i < info->countActionSets; i++)
			{
				XrActionSet s = info->actionSets[i];
				w.mActionSets[i] = s;
				DumpActionSet(session,s);
			}

		}
		return r;
	}

	XrResult thisLayer_xrPollEvent(
		XrInstance instance, XrEventDataBuffer* eventData)
	{
		INSTANCE_FALLBACK(thisLayer_xrPollEvent(instance, eventData));
		XrResult res = nextLayer_xrPollEvent(instance, eventData);

		if(res == XR_SUCCESS)
		{
			if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
			{
				XrEventDataSessionStateChanged *eSession = (XrEventDataSessionStateChanged*) eventData;
				if(eSession->state >= XR_SESSION_STATE_IDLE && eSession->state <= XR_SESSION_STATE_FOCUSED && mActiveSession != eSession->session )
				{
					mActiveSession = eSession->session;
					SessionState *w = &mSessions[mActiveSession];
					mpActiveSession = w;
//					for(int i = 0; i < w->mActionSetsCount; i++)
//						DumpActionSet(mActiveSession, w->mActionSets[i]);

				}
			}

			if(eventData->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED && mActiveSession)
			{
				SessionState *w = mpActiveSession;
				const char * const userPaths[] = {
					"/user/hand/left",
					"/user/hand/right",
					"/user/head",
					"/user/gamepad",
					//				"/user/treadmill"
				};
				if(!w)
					return res;

				XrInteractionProfileState state = {XR_TYPE_INTERACTION_PROFILE_STATE};
				for(int i = 0; i < sizeof(userPaths)/sizeof(userPaths[0]); i++)
				{
					XrPath path;
					if(nextLayer_xrStringToPath(instance, userPaths[i], &path) != XR_SUCCESS)
						continue;
					if(nextLayer_xrGetCurrentInteractionProfile(mActiveSession, path, &state) != XR_SUCCESS)
						continue;
					if(state.interactionProfile == XR_NULL_PATH)
						continue;
					char profileStr[XR_MAX_PATH_LENGTH];
					uint32_t len;
					nextLayer_xrPathToString(instance, state.interactionProfile, XR_MAX_PATH_LENGTH, &len, profileStr);
					printf("New interaction profile for %s: %s\n", userPaths[i], profileStr );
				}
				for(int i = 0; i < w->mActionSetsCount; i++)
					DumpActionSet(mActiveSession, w->mActionSets[i]);
			}
		}

		return res;
	}

	//IMPORTANT: to allow for multiple instance creation/destruction, the contect of the layer must be re-initialized when the instance is being destroyed.
	//Hooking xrDestroyInstance is the best way to do that.
	XrResult thisLayer_xrDestroyInstance(XrInstance instance)
	{
		INSTANCE_FALLBACK(thisLayer_xrDestroyInstance(instance));
		mInstance = XR_NULL_HANDLE;
		mExtensionsCount = 0;
		delete[] mExtensions;
		mExtensions = nullptr;
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
		if constexpr(instance_index < T::max_instances)
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

#if XR_THISLAYER_HAS_EXTENSIONS
	if(Layer::mInstances[i].IsExtensionEnabled("XR_TEST_test_me"))
	{
		WRAP_FUNC(xrTestMeTEST);
	}
#endif

	//Not wrapped? Chain call
	return Layer::mInstances[i].nextLayer_xrGetInstanceProcAddr(instance, name,function);
}
