# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmodpb64
LOCAL_SRC_FILES := modp_b64.cc
LOCAL_CPP_EXTENSION := .cc
LOCAL_CFLAGS := -Wall -Werror
LOCAL_C_INCLUDES := $(LOCAL_PATH)/modp_b64
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmodpb64-host
LOCAL_SRC_FILES := modp_b64.cc
LOCAL_CPP_EXTENSION := .cc
LOCAL_CFLAGS := -Wall -Werror
LOCAL_C_INCLUDES := $(LOCAL_PATH)/modp_b64
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_HOST_STATIC_LIBRARY)
