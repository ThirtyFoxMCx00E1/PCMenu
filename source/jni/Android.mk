LOCAL_PATH := $(call my-dir)

# Two builds per ABI:
#   PCMenu32 / PCMenu64   - the PC menu (opaque backdrop, like the PC game)
#   PCMenuT32 / PCMenuT64 - the same menu, transparent: the paused game shows (blurred) behind it
# Both share the GUID net.gtasa.pcmenu, so AML loads only one of them: install one variant.

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

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_MODULE := PCMenuT32
    LOCAL_CFLAGS += -mfloat-abi=softfp
else
    LOCAL_MODULE := PCMenuT64
endif
LOCAL_SRC_FILES := main.cpp mod/logger.cpp
LOCAL_CFLAGS += -O2 -DNDEBUG -DPCMENU_TRANSPARENT -std=c++17 -fno-exceptions -fno-rtti -Wall -Wno-unused-function -Wno-unused-variable
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_LDLIBS += -llog -lOpenSLES -ldl
include $(BUILD_SHARED_LIBRARY)
