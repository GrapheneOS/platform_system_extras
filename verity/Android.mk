LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := verity_signer
LOCAL_MODULE := verity_signer
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := VeritySigner
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := boot_signer
LOCAL_MODULE := boot_signer
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := BootSignature
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := build_verity_metadata.py
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := build_verity_metadata.py
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)
