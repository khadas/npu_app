ifeq ($(AMLOGIC_PRODUCT),true)
LOCAL_PATH:=$(call my-dir)
include $(LOCAL_PATH)/ADLA_TEST/Android.mk
endif
