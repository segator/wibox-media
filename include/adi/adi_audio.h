/*!
*****************************************************************************
** \file        ./adi/inc/adi_audio.h
**
** \brief       adi audio porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_AUDIO_H_
#define _ADI_AUDIO_H_

#include "stdio.h"
#include "adi_types.h"


//*****************************************************************************
//*****************************************************************************
//** Defines and Macros
//*****************************************************************************
//*****************************************************************************

/*
*******************************************************************************
** Defines for general error codes of the module.
*******************************************************************************
*/
/*! Bad parameter passed. */
#define GADI_AUDIO_ERR_BAD_PARAMETER                                          \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_AUDIO_ERR_OUT_OF_MEMORY                                          \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_AUDIO_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_AUDIO_ERR_NOT_INITIALIZED                                        \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_AUDIO_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_AUDIO_ERR_TIMEOUT                                                \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_AUDIO_ERR_DEVICE_BUSY                                            \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_AUDIO_ERR_INVALID_HANDLE                                         \
                              (GADI_AUDIO_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_AUDIO_ERR_SEMAPHORE_CREATE                                       \
                               (GADI_AUDIO_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_AUDIO_ERR_UNSUPPORTED_VERSION                                    \
                               (GADI_AUDIO_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The device/handle is not open.. */
#define GADI_AUDIO_ERR_NOT_OPEN                                               \
                               (GADI_AUDIO_MODULE_BASE + GADI_ERR_NOT_OPEN)
/*! Error from driver.*/
#define GADI_AUDIO_ERR_FROM_DRIVER                                            \
                               (GADI_AUDIO_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! operation not permit.*/
#define GADI_AUDIO_ERR_OPERATION_NOT_PERMIT                                            \
                            (GADI_AUDIO_MODULE_BASE + GADI_ERR_OPERATION_NOT_PERMIT)


//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief audio sample rate enum.
*******************************************************************************
*/
typedef enum {
    GADI_AUDIO_SAMPLE_RATE_8000  = 8000,
    GADI_AUDIO_SAMPLE_RATE_11025 = 11025,
    GADI_AUDIO_SAMPLE_RATE_12000 = 12000,
    GADI_AUDIO_SAMPLE_RATE_16000 = 16000,
    GADI_AUDIO_SAMPLE_RATE_22050 = 22050,
    GADI_AUDIO_SAMPLE_RATE_24000 = 24000,
    GADI_AUDIO_SAMPLE_RATE_32000 = 32000,
    GADI_AUDIO_SAMPLE_RATE_44100 = 44100,
    GADI_AUDIO_SAMPLE_RATE_48000 = 48000,
    GADI_AUDIO_SAMPLE_RATE_NUM,
} GADI_AUDIO_SampleRateEnumT;

/*!
*******************************************************************************
** \brief audio sound channel mode enum.
*******************************************************************************
*/
typedef enum {
    GADI_AUDIO_SOUND_MODE_LEFT = 0,
    GADI_AUDIO_SOUND_MODE_RIGHT = 1,
    GADI_AUDIO_SOUND_MODE_STEREO = 2,
    GADI_AUDIO_SOUND_MODE_MONO = 3,
    GADI_AUDIO_SOUND_MODE_SINGLE = 4,
    GADI_AUDIO_SOUND_MODE_NUM,
}GADI_AUDIO_SoundModeEnumT;


/*!
*******************************************************************************
** \brief audio sample precision enum.
*******************************************************************************
*/
typedef enum {
    GADI_AUDIO_BIT_WIDTH_16 = 16,
    GADI_AUDIO_BIT_WIDTH_NUM,
}GADI_AUDIO_BitWidthEnumT;

/*!
*******************************************************************************
** \brief audio sample date format enum.
*******************************************************************************
*/
typedef enum {
    /** Raw-Pcm */
    GADI_AUDIO_SAMPLE_FORMAT_RAW_PCM,
    /** Mu-Law */
    GADI_AUDIO_SAMPLE_FORMAT_MU_LAW,
    /** A-Law */
    GADI_AUDIO_SAMPLE_FORMAT_A_LAW,
}GADI_AUDIO_SampleFormatEnumT;

/*!
*******************************************************************************
** \brief audio AO device volume level.
*******************************************************************************
*/
typedef enum
{
    VLEVEL_0 = 0xaf,
    VLEVEL_1 = 0xad,
    VLEVEL_2 = 0xab,
    VLEVEL_3 = 0xa9,
    VLEVEL_4 = 0xa7,
    VLEVEL_5 = 0xa5,
    VLEVEL_6 = 0xa3,
    VLEVEL_7 = 0xa1,
    VLEVEL_8 = 0xa0,
    VLEVEL_9 = 0xbe,
    VLEVEL_10 = 0xb9,
    VLEVEL_11 = 0xb4,
    VLEVEL_12 = 0xb0,
}GADI_AUDIO_VolumeLevelEnumT;
/*!
*******************************************************************************
** \brief audio AI device gain level.
*******************************************************************************
*/
typedef enum
{
    GLEVEL_0 = 0,
    GLEVEL_1,
    GLEVEL_2,
    GLEVEL_3,
    GLEVEL_4,
    GLEVEL_5,
    GLEVEL_6,
    GLEVEL_7,
    GLEVEL_8,
    GLEVEL_9,
    GLEVEL_10,
    GLEVEL_11,
    GLEVEL_12,
    GLEVEL_13,
    GLEVEL_14,
    GLEVEL_15,
    GLEVEL_MAX,
}GADI_AUDIO_GainLevelEnumT;

//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************

/*!
*******************************************************************************
** \brief AI/AO device attributes struct.
*******************************************************************************
*/
typedef struct
{
    GADI_AUDIO_SampleRateEnumT            sampleRate;
    GADI_AUDIO_BitWidthEnumT              bitWidth;
    GADI_AUDIO_SoundModeEnumT                 soundMode;
    GADI_U32                                frameSamples;
    GADI_U32                             frameNum;

}GADI_AUDIO_AioAttrT;
/*!
*******************************************************************************
** \brief audio frame struct.
*******************************************************************************
*/
typedef struct
{
    GADI_AUDIO_BitWidthEnumT     bitWidth;
    GADI_AUDIO_SoundModeEnumT     soundMode;
    GADI_U8        *virAddr;
    GADI_U32            phyAddr;
    GADI_U64      timeStamp;
    GADI_U32         seq;
    GADI_U32            len;
}GADI_AUDIO_AioFrameT;

/*!
*******************************************************************************
** \brief audio frame struct.
*******************************************************************************
*/
typedef struct
{
    GADI_U8        *virAddr;
    GADI_U32            len;
    GADI_BOOL      valid;
}GADI_AEC_AioFrameT;
/*!
*******************************************************************************
** \brief audio echo cancellation struct.
*******************************************************************************
*/
typedef struct
{
GADI_CHAR aecName[16];
GADI_S32 (*aecInit)(int sampleRate, int frameSamples);
GADI_S32 (*aecDeinit)(void);
GADI_S32 (*aecProcess)(GADI_VOID *aiData, GADI_VOID *aoData, GADI_VOID *aecData);
} GADI_AUDIO_EchoCancellationT;
/*!
*******************************************************************************
** \brief frame buffer status of the AO device channel.
*******************************************************************************
*/
typedef struct 
{
/*total number of frame buffers**/
GADI_U32 frameNum;
/** used frame buffers count.*/
GADI_U32 frameCnt;
} GADI_AUDIO_AO_StateT;

//*****************************************************************************
//*****************************************************************************
//** API Functions
//*****************************************************************************
//*****************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

/*!
*******************************************************************************
** \brief Initialize the ADI AUDIO module.
**
** \return
** - #GADI_OK
** - #GADI_DSP_ERR_ALREADY_INITIALIZED
** - #GADI_DSP_ERR_OUT_OF_MEMORY
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_exit
**
*******************************************************************************
*/
GADI_ERR gadi_audio_init();

/*!
*******************************************************************************
** \brief Shutdown the ADI AUDIO module.
**
** \return
** - #GADI_OK
** - #GADI_DSP_ERR_NOT_INITIALIZED
**
** \sa gadi_audio_init
**
*******************************************************************************
*/
GADI_ERR gadi_audio_exit();

/*!
*******************************************************************************
** \brief Sets up attributes for AI device.
**
** \param[in] ptrAttr Pointer to the AI device attributes.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_get_attr
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ai_set_attr(GADI_AUDIO_AioAttrT* ptrAttr);

/*!
*******************************************************************************
** \brief Gets attributes of AI device.
**
** \param[in]  ptrAttr Pointer to the AI device attributes.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_set_attr
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ai_get_attr(GADI_AUDIO_AioAttrT* ptrAttr);

/*!
*******************************************************************************
** \brief Enables AI device.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_OUT_OF_MEMORY
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_disable
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ai_enable(void);

/*!
*******************************************************************************
** \brief Disables AI device.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_enable
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ai_disable(void);


/*!
*******************************************************************************
** \brief receives audio pcm frame.
**
** \param[out]  ptrFrame Pointer to audio frame.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_send_frame
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ai_get_frame(GADI_AUDIO_AioFrameT *ptrFrame, GADI_BOOL block);

/*!
*******************************************************************************
** \brief receives audio pcm frame and aec frame.
**
** \param[out]  ptrFrame Pointer to audio frame.
** \param[out]  ptrAecFrame Pointer to aec frame.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_send_frame_aec
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ai_get_frame_aec(GADI_AUDIO_AioFrameT *ptrFrame,
                                GADI_AEC_AioFrameT *ptrAecFrame, GADI_BOOL block);

/*!
*******************************************************************************
** \brief Gets fd of AI device.
**
**
** \return
** - #Valid fd value
** - #GADI_AUDIO_ERR_NOT_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_S32 gadi_audio_ai_get_fd(void);

/*!
*******************************************************************************
** \brief Sets the gain of AI device.
**
** \param[in] level  pointer to gain level.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_get_gain
**
*******************************************************************************
*/
    GADI_ERR gadi_audio_ai_set_gain(GADI_AUDIO_GainLevelEnumT *ptrgLevel);
/*!
*******************************************************************************
** \brief Gets the gain of AI device.
**
**
** \param[in] level  pointer to gain level.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_set_gain
**
*******************************************************************************
*/
    GADI_ERR gadi_audio_ai_get_gain(GADI_AUDIO_GainLevelEnumT *ptrgLevel);

/*!
*******************************************************************************
** \brief Get audio ai frame size.
**
** \param[in] soundMode    audio sound channel mode.
**
** \return
** - #frame size
**
** \sa
**
*******************************************************************************
*/
GADI_U32 gadi_audio_ai_get_frame_size(void);

/*!
*******************************************************************************
** \brief Enable the echo cancellation function
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_OUT_OF_MEMORY
**
** \sa gadi_audio_ai_aec_disable
**
*******************************************************************************
*/
    GADI_ERR gadi_audio_ai_aec_enable(void);

/*!
*******************************************************************************
** \brief Disable the echo cancellation function
**
**
** \return
** - #GADI_OK
**
** \sa gadi_audio_ai_aec_enable
**
*******************************************************************************
*/
    GADI_ERR gadi_audio_ai_aec_disable(void);

/*!
*******************************************************************************
** \brief Sets up attributes for AO device.
**
** \param[in] ptrAttr Pointer to the AO device attributes.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_get_attr
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_set_attr(GADI_AUDIO_AioAttrT* ptrAttr);

/*!
*******************************************************************************
** \brief Gets attributes of AO device.
**
** \param[in]  ptrAttr Pointer to the AO device attributes.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_set_attr
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_get_attr(GADI_AUDIO_AioAttrT* ptrAttr);

/*!
*******************************************************************************
** \brief Enables AO device.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_disable
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_enable(void);

/*!
*******************************************************************************
** \brief Disables AO device.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_enable
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_disable(void);


/*!
*******************************************************************************
** \brief Transmits audio pcm frame.
**
** \param[in]  ptrFrame Pointer to audio pcm frame.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_BAD_PARAMETER
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_get_frame
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_send_frame(GADI_AUDIO_AioFrameT *ptrFrame, GADI_BOOL block);

/*!
*******************************************************************************
** \brief Transmits audio pcm frame.(support aec)
**
** \param[in]  ptrFrame Pointer to audio pcm frame.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_BAD_PARAMETER
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ai_get_frame_aec
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_send_frame_aec(GADI_AUDIO_AioFrameT *ptrFrame, GADI_BOOL block);

/*!
*******************************************************************************
** \brief Gets fd of AO device.
**
**
** \return
** - #Valid fd value
** - #GADI_AUDIO_ERR_NOT_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_S32 gadi_audio_ao_get_fd(void);

/*!
*******************************************************************************
** \brief Sets the volume of AO device.
**
** \param[in] level  pointer to volume level.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_get_volume
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_set_volume(GADI_AUDIO_VolumeLevelEnumT *ptrvLevel);

/*!
*******************************************************************************
** \brief Gets the volume of AO device.
**
**
** \param[in] level  pointer to volume level.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_set_volume
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_get_volume(GADI_AUDIO_VolumeLevelEnumT *ptrvLevel);

/*!
*******************************************************************************
** \brief Binds AO device channel and AI device channel.
**
** \param[in] ptrAttr Pointer to the AI and AO device attributes.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_unbind_ai
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_bind_ai(GADI_AUDIO_AioAttrT* ptrAttr);

/*!
*******************************************************************************
** \brief Unbinds AO device channel and AI device channel.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_audio_ao_bind_ai
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_unbind_ai(void);

/*!
*******************************************************************************
** \brief Query the frame buffer status of the AO device channel.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa 
**
*******************************************************************************
*/
GADI_ERR gadi_audio_ao_query(GADI_AUDIO_AO_StateT *ptrAoState);

/*!
*******************************************************************************
** \brief Set audio sample format.
**
** \param[in] fd  file descriptor of aio device.
** \param[in] format  audio sample date format.
**
** \return
** - #sample format
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_audio_set_sample_format(GADI_S32 fd, GADI_AUDIO_SampleFormatEnumT format);

/*!
*******************************************************************************
** \brief query valid data size.
**
** \param[out] validDataSize get valid data size in aio cache.
**
** \return
** - #valid data size
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_audio_aio_query(GADI_U32 *validDataSize);

/*!
*******************************************************************************
** \brief Registers AEC.
**
** \param[out] ptrAecHandle  pointer to aec handle.
** \param[in]  ptrAec     pointer to aec attributes Structure.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_OUT_OF_MEMORY
** - #GADI_AUDIO_ERR_BAD_PARAMETER
** - #GADI_AUDIO_ERR_OPERATION_NOT_PERMIT
**
** \sa gadi_audio_unregister_echo_cancellation
**
*******************************************************************************
*/
GADI_ERR gadi_audio_register_echo_cancellation(GADI_S32 *ptrAecHandle,
                                            GADI_AUDIO_EchoCancellationT *ptrAec);
/*!
*******************************************************************************
** \brief unregisters AEC.
**
** \param[in] aecHandle  aec handle.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_BAD_PARAMETER
** - #GADI_AUDIO_ERR_OPERATION_NOT_PERMIT
**
** \sa gadi_audio_register_echo_cancellation
**
*******************************************************************************
*/
GADI_ERR gadi_audio_unregister_echo_cancellation(GADI_S32 aecHandle);


#ifdef __cplusplus
    }
#endif


#endif /* _ADI_AUDIO_H_ */
