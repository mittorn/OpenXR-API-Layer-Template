// SPDX-FileCopyrightText: 2021-2023 Arthur Brainville (Ybalrid) <ybalrid@ybalrid.info>
//
// SPDX-FileCopyrightText: 2024 mittorn <mittorn@disroot.org>
//
// SPDX-License-Identifier: MIT
//
// Initial Author: Arthur Brainville <ybalrid@ybalrid.info>

#pragma once

#include "openxr/openxr.h"

bool CreateLayerInstance(XrInstance instance, PFN_xrGetInstanceProcAddr gpa, const char **exts, uint32_t extcount, const XrInstanceCreateInfo *info);
