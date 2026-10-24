LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_MODULE := PCMenu32
    LOCAL_CFLAGS += -mfloat-abi=softfp
else
    LOCAL_MODULE := PCMenu64
endif
LOCAL_SRC_FILES := main.cpp mod/logger.cpp
LOCAL_CFLAGS += -O2 -DNDEBUG -std=c++17 -fno-exceptions -fno-rtti -Wall -Wno-unused-function -Wno-unused-variable
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_LDLIBS += -llog -lOpenSLES -ldl
include $(BUILD_SHARED_LIBRARY)
