// SPDX-FileCopyrightText: 2021-2023 Arthur Brainville (Ybalrid) <ybalrid@ybalrid.info>
//
// SPDX-License-Identifier: MIT
//
// Initial Author: Arthur Brainville <ybalrid@ybalrid.info>

#include "layer_shims.hpp"

#include <cassert>
#include <iostream>

#include <unordered_map>
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
XRAPI_ATTR XrResult XRAPI_CALL thisLayer_xrCreateAction (XrActionSet actionSet, const XrActionCreateInfo *info, XrAction *action)
{
	static PFN_xrCreateAction nextLayer_xrCreateAction = GetNextLayerFunction(xrCreateAction);
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
XRAPI_ATTR XrResult XRAPI_CALL thisLayer_xrCreateActionSet (XrInstance instance, const XrActionSetCreateInfo *info, XrActionSet *actionSet)
{
	static PFN_xrCreateActionSet nextLayer_xrCreateActionSet = GetNextLayerFunction(xrCreateActionSet);
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
	static PFN_xrEnumerateBoundSourcesForAction nextLayer_xrEnumerateBoundSourcesForAction = GetNextLayerFunction(xrEnumerateBoundSourcesForAction);
	static PFN_xrPathToString nextLayer_xrPathToString = GetNextLayerFunction(xrPathToString);
	static PFN_xrGetInputSourceLocalizedName nextLayer_xrGetInputSourceLocalizedName = GetNextLayerFunction(xrGetInputSourceLocalizedName);
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

XRAPI_ATTR XrResult XRAPI_CALL thisLayer_xrAttachSessionActionSets (XrSession session, const XrSessionActionSetsAttachInfo *info)
{
	static PFN_xrAttachSessionActionSets nextLayer_xrAttachSessionActionSets = GetNextLayerFunction(xrAttachSessionActionSets);
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


XRAPI_ATTR XrResult XRAPI_CALL thisLayer_xrPollEvent(
	XrInstance instance, XrEventDataBuffer* eventData)
{
	static PFN_xrPollEvent nextLayer_xrPollEvent = GetNextLayerFunction(xrPollEvent);
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
			static PFN_xrPathToString nextLayer_xrPathToString = GetNextLayerFunction(xrPathToString);
			static PFN_xrStringToPath nextLayer_xrStringToPath = GetNextLayerFunction(xrStringToPath);
			static PFN_xrGetCurrentInteractionProfile nextLayer_xrGetCurrentInteractionProfile = GetNextLayerFunction(xrGetCurrentInteractionProfile);

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
XRAPI_ATTR XrResult XRAPI_CALL thisLayer_xrDestroyInstance(
	XrInstance instance)
{
	PFN_xrDestroyInstance nextLayer_xrDestroyInstance = GetNextLayerFunction(xrDestroyInstance);

	OpenXRLayer::DestroyLayerContext();

	assert(nextLayer_xrDestroyInstance != nullptr);
	return nextLayer_xrDestroyInstance(instance);
}



//Define the functions implemented in this layer like this:
XRAPI_ATTR XrResult XRAPI_CALL thisLayer_xrEndFrame(XrSession session,
	const XrFrameEndInfo* frameEndInfo)
{
	//First time this runs, it will fetch the pointer from the loaded OpenXR dispatch table
	static PFN_xrEndFrame nextLayer_xrEndFrame = GetNextLayerFunction(xrEndFrame);

	//Do some additional things;
	std::cout << "Display frame time is " << frameEndInfo->displayTime << "\n";

	//Call down the chain
	const auto result = nextLayer_xrEndFrame(session, frameEndInfo);

	//Maybe do something with the original return value?
	if(result == XR_ERROR_TIME_INVALID)
		std::cout << "frame time is invalid?\n";

	//Return what should be returned as if you were the actual function
	return result;
}

#if XR_THISLAYER_HAS_EXTENSIONS
//The following function doesn't exist in the spec, this is just a test for the extension mecanism
XRAPI_ATTR XrResult XRAPI_CALL thisLayer_xrTestMeTEST(XrSession session)
{
	(void)session;
	std::cout << "xrTestMe()\n";
	return XR_SUCCESS;
}
#endif


//This functions returns the list of function pointers and name we implement, and is called during the initialization of the layer:
std::vector<OpenXRLayer::ShimFunction> ListShims()
{
	std::vector<OpenXRLayer::ShimFunction> functions;
	functions.emplace_back("xrDestroyInstance", PFN_xrVoidFunction(thisLayer_xrDestroyInstance));

	//List every functions that is callable on this API layer
//	functions.emplace_back("xrEndFrame", PFN_xrVoidFunction(thisLayer_xrEndFrame));
//	functions.emplace_back("xrSuggestInteractionProfileBindings", PFN_xrVoidFunction(thisLayer_xrEndFrame));
	functions.emplace_back("xrCreateActionSet", PFN_xrVoidFunction(thisLayer_xrCreateActionSet));
	functions.emplace_back("xrCreateAction", PFN_xrVoidFunction(thisLayer_xrCreateAction));
	functions.emplace_back("xrAttachSessionActionSets", PFN_xrVoidFunction(thisLayer_xrAttachSessionActionSets));
	functions.emplace_back("xrPollEvent", PFN_xrVoidFunction(thisLayer_xrPollEvent));

#if XR_THISLAYER_HAS_EXTENSIONS
//	if (OpenXRLayer::IsExtensionEnabled("XR_TEST_test_me"))
//		functions.emplace_back("xrTestMeTEST", PFN_xrVoidFunction(thisLayer_xrTestMeTEST));
#endif

	return functions;
}
