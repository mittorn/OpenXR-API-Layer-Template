// SPDX-FileCopyrightText: 2021-2023 Arthur Brainville (Ybalrid) <ybalrid@ybalrid.info>
//
// SPDX-FileCopyrightText: 2024 mittorn <mittorn@disroot.org>
//
// SPDX-License-Identifier: MIT
//
// Initial Author: Arthur Brainville <ybalrid@ybalrid.info>

#include "layer_bootstrap.hpp"
#include "layer_shims.hpp"
#include "layer_config.hpp"
#include <string.h>

extern "C" XrResult LAYER_EXPORT XRAPI_CALL xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo * loaderInfo, const char* apiLayerName,
	XrNegotiateApiLayerRequest* apiLayerRequest)
{
	if (nullptr == loaderInfo || nullptr == apiLayerRequest || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
		loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
		apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
		apiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
		apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
		loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
		loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
		loaderInfo->maxInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
		loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION ||
		loaderInfo->minApiVersion > XR_CURRENT_API_VERSION ||
		0 != strcmp(apiLayerName, XR_THISLAYER_NAME)) {
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
	apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
	apiLayerRequest->getInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(thisLayer_xrGetInstanceProcAddr);
	apiLayerRequest->createApiLayerInstance = reinterpret_cast<PFN_xrCreateApiLayerInstance>(thisLayer_xrCreateApiLayerInstance);

	return XR_SUCCESS;
}

XrResult thisLayer_xrCreateApiLayerInstance(const XrInstanceCreateInfo* info, const XrApiLayerCreateInfo* apiLayerInfo,
	XrInstance* instance)
{
	if (nullptr == apiLayerInfo || XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO != apiLayerInfo->structType ||
		XR_API_LAYER_CREATE_INFO_STRUCT_VERSION > apiLayerInfo->structVersion ||
		sizeof(XrApiLayerCreateInfo) > apiLayerInfo->structSize || nullptr == apiLayerInfo->nextInfo ||
		XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO != apiLayerInfo->nextInfo->structType ||
		XR_API_LAYER_NEXT_INFO_STRUCT_VERSION > apiLayerInfo->nextInfo->structVersion ||
		sizeof(XrApiLayerNextInfo) > apiLayerInfo->nextInfo->structSize ||
		0 != strcmp(XR_THISLAYER_NAME, apiLayerInfo->nextInfo->layerName) ||
		nullptr == apiLayerInfo->nextInfo->nextGetInstanceProcAddr ||
		nullptr == apiLayerInfo->nextInfo->nextCreateApiLayerInstance)
	{
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	//Prepare to call this function down the layer chain
	XrApiLayerCreateInfo newApiLayerCreateInfo;
	memcpy(&newApiLayerCreateInfo, &apiLayerInfo, sizeof(newApiLayerCreateInfo));
	newApiLayerCreateInfo.nextInfo = apiLayerInfo->nextInfo->next;

	XrInstanceCreateInfo instanceCreateInfo = *info;
	const char **extension_list_without_implemented_extensions = nullptr;
	const char **enabled_this_layer_extensions = nullptr;
	uint32_t extension_list_without_implemented_extensions_count = 0;
	uint32_t enabled_this_layer_extensions_count = 0;

	//If we deal with extensions, we will check the list of enabled extensions.
	//We remove ours form the list if present, and we store the list of *our* extensions that were enabled
	#if XR_THISLAYER_HAS_EXTENSIONS
	{
		extension_list_without_implemented_extensions = new const char*[instanceCreateInfo.enabledExtensionCount];
		enabled_this_layer_extensions = new const char*[sizeof(enabled_this_layer_extensions)/sizeof(enabled_this_layer_extensions[0])];
		for (size_t enabled_extension_index = 0; enabled_extension_index < instanceCreateInfo.enabledExtensionCount; ++enabled_extension_index)
		{
			const char* enabled_extension_name = instanceCreateInfo.enabledExtensionNames[enabled_extension_index];
			bool implemented_by_us = false;

			for (const auto layer_extension_name : layer_extension_names)
			{
				if(0 == strcmp(enabled_extension_name, layer_extension_name))
				{
					implemented_by_us = true;
					break;
				}
			}

			if(implemented_by_us)
				enabled_this_layer_extensions[enabled_this_layer_extensions_count++] = enabled_extension_name;
			else
				extension_list_without_implemented_extensions[extension_list_without_implemented_extensions_count++] = enabled_extension_name;
		}

		instanceCreateInfo.enabledExtensionCount = extension_list_without_implemented_extensions_count;
		instanceCreateInfo.enabledExtensionNames = extension_list_without_implemented_extensions;
	}
#endif


	XrInstance newInstance = *instance;
	const auto result = apiLayerInfo->nextInfo->nextCreateApiLayerInstance(&instanceCreateInfo, &newApiLayerCreateInfo, &newInstance);
	if (XR_FAILED(result))
	{
		return XR_ERROR_LAYER_INVALID;
	}

	CreateLayerInstance(newInstance, apiLayerInfo->nextInfo->nextGetInstanceProcAddr, enabled_this_layer_extensions, enabled_this_layer_extensions_count);

	*instance = newInstance;
	delete[] extension_list_without_implemented_extensions;
	return result;
}


