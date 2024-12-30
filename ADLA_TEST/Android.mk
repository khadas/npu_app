LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	adla_runtime.cpp

#LOCAL_C_INCLUDES := $(LOCAL_PATH)


#LOCAL_LDFLAGS := \
	$(LDFLAGS) \
#	-Wl,-z,defs \
#	-Wl,--version-script=$(LOCAL_PATH)/libOpenCL12.map
LOCAL_CFLAGS := \
	-Wno-missing-field-initializers

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libdl \
	libadla

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif


LOCAL_MODULE         := runtime
LOCAL_MODULE_TAGS := tests optional
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
LOCAL_CPP_FEATURES += exceptions
LOCAL_CPPFLAGS += -frtti
include $(BUILD_EXECUTABLE)

#include $(AQROOT)/copy_installed_module.mk

