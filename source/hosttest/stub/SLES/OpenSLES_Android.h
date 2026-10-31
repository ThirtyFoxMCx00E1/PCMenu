#pragma once
#include "OpenSLES.h"
#define SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE 0x800007BD
struct SLAndroidSimpleBufferQueueItf_; typedef const SLAndroidSimpleBufferQueueItf_* const* SLAndroidSimpleBufferQueueItf;
struct SLAndroidSimpleBufferQueueItf_ { SLresult (*Enqueue)(SLAndroidSimpleBufferQueueItf, const void*, SLuint32); SLresult (*Clear)(SLAndroidSimpleBufferQueueItf); };
struct SLDataLocator_AndroidSimpleBufferQueue { SLuint32 locatorType; SLuint32 numBuffers; };
extern const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
#define SL_DATALOCATOR_ANDROIDFD 0x800007BC
struct SLDataLocator_AndroidFD { SLuint32 locatorType; int32_t fd; SLAint64 offset; SLAint64 length; };
