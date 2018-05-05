LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := app-launcher
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := app-launcher
LOCAL_MODULE_HOST_OS := linux
LOCAL_REQUIRED_MODULES := computestats computestatsf
include $(BUILD_PREBUILT)
