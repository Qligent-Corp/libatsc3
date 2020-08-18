# Android.mk for libatsc3 Android sample app
#
#
# jjustman@ngbp.org - for libatsc3 inclusion 2019-09-28

# global pathing
MY_LOCAL_PATH := $(call my-dir)
LOCAL_PATH := $(call my-dir)
MY_CUR_PATH := $(LOCAL_PATH)

#jjustman-2020-08-10 - super hack
LOCAL_PATH := $(MY_LOCAL_PATH)

##jjustman-2020-08-12 - remove prefab LOCAL_SRC_FILES := $(LOCAL_PATH)/../libatsc3_core/build/intermediates/prefab_package/debug/prefab/modules/atsc3_core/libs/android.$(TARGET_ARCH_ABI)/libatsc3_core.so
include $(CLEAR_VARS)
LOCAL_MODULE := local-atsc3_core
LOCAL_SRC_FILES := $(LOCAL_PATH)/../atsc3_core/build/intermediates/ndkBuild/debug/obj/local/$(TARGET_ARCH_ABI)/libatsc3_core.so
ifneq ($(MAKECMDGOALS),clean)
include $(PREBUILT_SHARED_LIBRARY)
endif

# ---------------------------
# libatsc3_bridge jni interface

include $(CLEAR_VARS)

LOCAL_MODULE := atsc3_bridge

#LIBATSC3COREBRIDGECPP := \
#    $(wildcard $(LOCAL_PATH)/src/cpp/*.cpp)
#    $(LIBATSC3COREBRIDGECPP:$(LOCAL_PATH)/%=%)  \
# LOCAL_C_INCLUDES += $(LOCAL_PATH)/src/cpp

LIBATSC3JNIBRIDGECPP := \
    $(wildcard $(LOCAL_PATH)/src/jni/*.cpp)

# jjustman-2020-08-10 - temporary - refactor this out...
LOCAL_SRC_FILES += \
    $(LIBATSC3JNIBRIDGECPP:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src/jni
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../src/

# jjustman-2020-08-10 - hack-ish... needed for atsc3_pcre2_regex_utils.h
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../atsc3_core/libpcre/include

LOCAL_CFLAGS += -g -fpack-struct=8 -fPIC  \
                -D__DISABLE_LIBPCAP__ -D__DISABLE_ISOBMFF_LINKAGE__ -D__DISABLE_NCURSES__ \
                -D__MOCK_PCAP_REPLAY__ -D__LIBATSC3_ANDROID__ -DDEBUG

LOCAL_LDFLAGS += -fPIE -fPIC -L $(LOCAL_PATH)/../atsc3_core/build/intermediates/ndkBuild/debug/obj/local/$(TARGET_ARCH_ABI)/

LOCAL_LDLIBS := -ldl -lc++_shared -llog -landroid -lz -latsc3_core


# LOCAL_SHARED_LIBRARIES := atsc3_core

include $(BUILD_SHARED_LIBRARY)

