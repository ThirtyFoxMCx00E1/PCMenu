#pragma once
#include <stdint.h>
typedef uint32_t SLuint32; typedef SLuint32 SLboolean; typedef SLuint32 SLresult; typedef const void* SLInterfaceID;
#define SL_RESULT_SUCCESS 0
#define SL_BOOLEAN_FALSE 0
#define SL_BOOLEAN_TRUE 1
#define SL_DATAFORMAT_PCM 2
#define SL_PCMSAMPLEFORMAT_FIXED_16 16
#define SL_SPEAKER_FRONT_LEFT 1
#define SL_SPEAKER_FRONT_RIGHT 2
#define SL_SPEAKER_FRONT_CENTER 4
#define SL_BYTEORDER_LITTLEENDIAN 2
#define SL_DATALOCATOR_OUTPUTMIX 4
#define SL_PLAYSTATE_PLAYING 3
struct SLObjectItf_; typedef const SLObjectItf_* const* SLObjectItf;
struct SLEngineItf_; typedef const SLEngineItf_* const* SLEngineItf;
struct SLPlayItf_; typedef const SLPlayItf_* const* SLPlayItf;
struct SLObjectItf_ { SLresult (*Realize)(SLObjectItf, SLboolean); SLresult (*GetInterface)(SLObjectItf, SLInterfaceID, void*); };
struct SLEngineItf_ { SLresult (*CreateOutputMix)(SLEngineItf, SLObjectItf*, SLuint32, const SLInterfaceID*, const SLboolean*);
                      SLresult (*CreateAudioPlayer)(SLEngineItf, SLObjectItf*, void*, void*, SLuint32, const SLInterfaceID*, const SLboolean*); };
struct SLPlayItf_ { SLresult (*SetPlayState)(SLPlayItf, SLuint32); };
struct SLDataFormat_PCM { SLuint32 formatType, numChannels, samplesPerSec, bitsPerSample, containerSize, channelMask, endianness; };
struct SLDataSource { void* pLocator; void* pFormat; };
struct SLDataLocator_OutputMix { SLuint32 locatorType; SLObjectItf outputMix; };
struct SLDataSink { void* pLocator; void* pFormat; };
extern const SLInterfaceID SL_IID_ENGINE; extern const SLInterfaceID SL_IID_PLAY;
SLresult slCreateEngine(SLObjectItf*, SLuint32, const void*, SLuint32, const SLInterfaceID*, const SLboolean*);
