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

//Define next function pointer
#define DECLARE_NEXT_FUNC(x) PFN_##x nextLayer_##x
//Load next function pointer
#define LOAD_NEXT_FUNC(x) nextLayer_xrGetInstanceProcAddr(mInstance, #x, (PFN_xrVoidFunction*)&nextLayer_##x)



#include <unordered_map>
#include <vector>
struct Layer
{
#if 0
#include "./HashMap.h"
struct MyKeyHash {
	unsigned long operator()(const void *key) const
	{
		return (uintptr_t)key % 10;
	}
};
template<class Key, class T>
using Map =  HashMap<Key, T, 10, MyKeyHash>;
#else
template<class Key, class T>
using Map = std::unordered_map<Key, T>;

#endif

Map<XrAction, XrActionCreateInfo> gActionInfos;


struct InputWrapper
{
	XrSession mSession;
	std::vector<XrActionSet> mActionSets;
};

struct ActionSet
{
	XrActionSetCreateInfo info;
	XrInstance instance;
	std::vector<XrAction> mActions;
};
Map<XrActionSet, ActionSet> gActionSetInfos;

Map<XrSession, InputWrapper*> gWrappers;
InputWrapper *gLastWrapper;
#if 0
InputWrapper *GetWrapper(XrSession session)
{
	if(gLastWrapper && gLastWrapper->mSession == session)
		return gLastWrapper;

	InputWrapper * w = 0;
	gWrappers.get(session,w);
	if(!w)
	{
		w = new InputWrapper;
		w->mSession = session;
		gWrappers.put(session,w);
	}
	return w;
}
#endif
	// maximum active instances loaded in class
	// normally it should be 1, but reserve more in case some library checks OpenXR
	constexpr static size_t max_instances = 4;
	static Layer mInstances[max_instances];

	// Only handle this XrInatance in this object
	XrInstance mInstance = XR_NULL_HANDLE;

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
	NEXT_FUNC(f, xrPollEvent)

	NEXT_FUNC_LIST(DECLARE_NEXT_FUNC);

	// Find Layer index for XrInstance
	static int FindInstance(XrInstance inst)
	{
		for(int i = 0; i < max_instances; i++) if(mInstances[i].mInstance == inst) return i;
		return -1;
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
			gActionInfos[act] = *info;
			gActionSetInfos[actionSet].mActions.push_back(act);
		}
		return r;
	}
	XrResult thisLayer_xrCreateActionSet (XrInstance instance, const XrActionSetCreateInfo *info, XrActionSet *actionSet)
	{
		XrActionSet acts;
		XrResult r = nextLayer_xrCreateActionSet(instance, info, &acts);
		if(r == XR_SUCCESS)
		{
			*actionSet = acts;
			gActionSetInfos[acts] = ActionSet{*info, instance};
		}
		return r;
	}

	void DumpActionSet(XrSession session, XrActionSet s)
	{
		ActionSet &seti = gActionSetInfos[s];
		printf("Attached action set: %s %s\n", seti.info.actionSetName, seti.info.localizedActionSetName );
		for(XrAction a: seti.mActions)
		{
			XrActionCreateInfo &cinfo = gActionInfos[a];
			printf("info %p: %s %s\n", (void*)a, cinfo.actionName, cinfo.localizedActionName);
			XrBoundSourcesForActionEnumerateInfo einfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
			einfo.action = a;
			uint32_t count;
			nextLayer_xrEnumerateBoundSourcesForAction(session, &einfo, 0, &count, NULL);
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

	XrResult thisLayer_xrAttachSessionActionSets (XrSession session, const XrSessionActionSetsAttachInfo *info)
	{
		XrResult r = nextLayer_xrAttachSessionActionSets(session, info);
		if(r == XR_SUCCESS)
		{
			InputWrapper *& w = gWrappers[session];
			if(!w)
			{
				w = new InputWrapper;
				w->mSession = session;
				for(int i = 0; i < info->countActionSets; i++)
				{
					XrActionSet s = info->actionSets[i];
					w->mActionSets.push_back(info->actionSets[i]);
					DumpActionSet(session,s);
				}

			}

		}
		return r;
	}


	XrResult thisLayer_xrPollEvent(
		XrInstance instance, XrEventDataBuffer* eventData)
	{
		XrResult res = nextLayer_xrPollEvent(instance, eventData);

		if(res == XR_SUCCESS)
		{
			static XrSession lastSession;
			if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
			{
				XrEventDataSessionStateChanged *eSession = (XrEventDataSessionStateChanged*) eventData;
				if(eSession->state >= XR_SESSION_STATE_IDLE && eSession->state <= XR_SESSION_STATE_FOCUSED)
					lastSession = eSession->session;
			}

			if(eventData->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED && lastSession)
			{
				InputWrapper *w = gWrappers[lastSession];
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
					if(nextLayer_xrGetCurrentInteractionProfile(lastSession, path, &state) != XR_SUCCESS)
						continue;
					if(state.interactionProfile == XR_NULL_PATH)
						continue;
					char profileStr[XR_MAX_PATH_LENGTH];
					uint32_t len;
					nextLayer_xrPathToString(instance, state.interactionProfile, XR_MAX_PATH_LENGTH, &len, profileStr);
					printf("New interaction profile for %s: %s\n", userPaths[i], profileStr );
				}
				for(XrActionSet s: w->mActionSets)
					DumpActionSet(lastSession, s);
			}
		}

		return res;
	}

	//IMPORTANT: to allow for multiple instance creation/destruction, the contect of the layer must be re-initialized when the instance is being destroyed.
	//Hooking xrDestroyInstance is the best way to do that.
	XrResult thisLayer_xrDestroyInstance(XrInstance instance)
	{
		if( instance != mInstance )
		{
			int i = FindInstance(instance);
			if(i >= 0)
				return mInstances[i].thisLayer_xrDestroyInstance(instance);
			return XR_ERROR_HANDLE_INVALID;
		}
		mInstance = XR_NULL_HANDLE;
		mExtensionsCount = 0;
		delete[] mExtensions;
		mExtensions = nullptr;
		return nextLayer_xrDestroyInstance(instance);
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

#if XR_THISLAYER_HAS_EXTENSIONS
	if(Layer::mInstances[i].IsExtensionEnabled("XR_TEST_test_me"))
	{
		WRAP_FUNC(xrTestMeTEST);
	}
#endif

	//Not wrapped? Chain call
	return Layer::mInstances[i].nextLayer_xrGetInstanceProcAddr(instance, name,function);
}
