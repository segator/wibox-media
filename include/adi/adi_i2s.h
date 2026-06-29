/*!
*****************************************************************************
** \file           ./adi/inc/adi_gpio.h
**
** \brief         adi gpio module porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**                 ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**                 OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_I2S_H_
#define _ADI_I2S_H_


//*****************************************************************************
//*****************************************************************************
//** Defines and Macros
//*****************************************************************************
//*****************************************************************************

/*
**************************************************************************
** Defines for general error codes of the module.
**************************************************************************
*/
/*! Bad parameter passed. */
#define GADI_I2S_ERR_BAD_PARAMETER                                          \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_I2S_ERR_OUT_OF_MEMORY                                          \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_I2S_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_I2S_ERR_NOT_INITIALIZED                                        \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_I2S_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_I2S_ERR_TIMEOUT                                                \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_I2S_ERR_DEVICE_BUSY                                            \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_I2S_ERR_INVALID_HANDLE                                         \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_I2S_ERR_SEMAPHORE_CREATE                                       \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_I2S_ERR_UNSUPPORTED_VERSION                                    \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_I2S_ERR_FROM_DRIVER                                            \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_I2S_ERR_NOT_OPEN                                               \
                              (GADI_I2S_MODULE_BASE + GADI_ERR_NOT_OPEN)
/*! operation not permit.*/
#define GADI_I2S_ERR_OPERATION_NOT_PERMIT                                            \
                            (GADI_AUDIO_MODULE_BASE + GADI_ERR_OPERATION_NOT_PERMIT)
//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief I2S mode enum.
*******************************************************************************
*/
typedef enum {
    GADI_I2S_MODE_LEFT  = 0,   
    GADI_I2S_MODE_RIGHT = 1,  
    GADI_I2S_MODE_MSB   = 2,   
    GADI_I2S_MODE_I2S   = 4,    
    GADI_I2S_MODE_DSP  = 6, 
}GADI_I2S_ModeEnumT;

/*!
*******************************************************************************
** \brief I2S work mode enum.
*******************************************************************************
*/
typedef enum {
    GADI_I2S_WORK_MODE_SLAVE  = 0,     
    GADI_I2S_WORK_MODE_MASTER,    
}GADI_I2S_WorkModeEnumT;

/*!
*******************************************************************************
** \brief I2S channel count.
*******************************************************************************
*/
typedef enum {
    GADI_I2S_CHANNEL_COUNT_2 = 0,     //!< I2S  2 channel.
    GADI_I2S_CHANNEL_COUNT_4,     //!< I2S  4 channel.
    GADI_I2S_CHANNEL_COUNT_6,     //!< I2S  6 channel.
}GADI_I2S_ChannelCountEnumT;

/*!
*******************************************************************************
** \brief I2S sample rate.
*******************************************************************************
*/
typedef enum {
    GADI_I2S_SPEED_8000 = 8000,
    GADI_I2S_SPEED_16000 = 16000,
	GADI_I2S_SPEED_24000 = 24000,
    GADI_I2S_SPEED_32000 = 32000,     
    GADI_I2S_SPEED_44100 = 44100,     
    GADI_I2S_SPEED_48000 = 48000,     
    GADI_I2S_SPEED_96000 = 96000,
}GADI_I2S_SpeedEnumT;

/*!
*******************************************************************************
** \brief I2S sample precision.
*******************************************************************************
*/
typedef enum {
    GADI_I2S_WORD_LENGTH_16BIT = 16,     
    GADI_I2S_WORD_LENGTH_24BIT = 24,
}GADI_I2S_WordLengthEnumT;
//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************
typedef struct
{    
    GADI_I2S_WorkModeEnumT    workMode;    
    GADI_I2S_ChannelCountEnumT    channel;
    GADI_I2S_ModeEnumT    i2sMode;
    GADI_I2S_SpeedEnumT    speed;
    GADI_I2S_WordLengthEnumT    wlen;
}GADI_I2S_devAttrT;

typedef struct
{
    GADI_S32    frameSize;
    GADI_S32    frameNum;
}GADI_I2S_FrameChnParamT;

typedef struct
{
    GADI_CHAR *data_addr;            
    GADI_U32 data_length;         
}GADI_I2S_FrameT;
//*****************************************************************************
//** API Functions
//*****************************************************************************
//*****************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

/*!
*******************************************************************************
** \brief Initialize the ADI I2S module.
**
** \return
** - #GADI_OK
** - #GADI_DSP_ERR_ALREADY_INITIALIZED
** - #GADI_DSP_ERR_OUT_OF_MEMORY
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_i2s_exit
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_init();

/*!
*******************************************************************************
** \brief Shutdown the ADI I2S module.
**
** \return
** - #GADI_OK
** - #GADI_DSP_ERR_NOT_INITIALIZED
**
** \sa gadi_i2s_init
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_exit();

/*!
*******************************************************************************
** \brief Sets up attributes for I2S device.
**
** \param[in] pstI2sAttr Pointer to the I2S device attributes.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_i2s_get_attr
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_set_attr(GADI_I2S_devAttrT *pstI2sAttr);

/*!
*******************************************************************************
** \brief Gets attributes of I2S device.
**
** \param[in]  pstI2sAttr Pointer to the I2S device attributes.
**
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_i2s_set_attr
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_get_attr(GADI_I2S_devAttrT *pstI2sAttr); 

/*!
*******************************************************************************
** \brief set i2s channel parameters.
**
** \param[in]  fd   file descriptor of i2s device.
** \param[in]  pstFrameChnParam  pointer of i2s channel parameters struct;
**
** \return
** - #GADI_I2S_ERR_BAD_PARAMETER
** - #GADI_I2S_ERR_OPERATION_NOT_PERMIT
** - #GADI_I2S_ERR_FROM_DRIVER
**
** \sa gadi_i2s_dev_get_frame_channel_param
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_dev_set_frame_channel_param(GADI_S32 fd, 
                                GADI_I2S_FrameChnParamT *pstFrameChnParam);

/*!
*******************************************************************************
** \brief get i2s channel parameters.
**
** \param[in]  fd   file descriptor of i2s device.
** \param[out]  pstFrameChnParam  pointer of i2s channel parameters struct;
**
** \return
** - #GADI_I2S_ERR_BAD_PARAMETER
** - #GADI_I2S_ERR_FROM_DRIVER
**
** \sa gadi_i2s_dev_set_frame_channel_param
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_dev_get_frame_channel_param(GADI_S32 fd, 
                                GADI_I2S_FrameChnParamT *pstFrameChnParam);

/*!
*******************************************************************************
** \brief Enables I2S device.
**
** \param[in]  fd   file descriptor of i2s device.
**
** \return
** - #GADI_OK
** - #GADI_I2S_ERR_BAD_PARAMETER
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_i2s_dev_disable
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_dev_enable(GADI_S32 fd);

/*!
*******************************************************************************
** \brief Disables I2S device.
**
** \param[in]  fd   file descriptor of i2s device.
**
** \return
** - #GADI_OK
** - #GADI_I2S_ERR_BAD_PARAMETER
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_i2s_dev_enable
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_dev_disable(GADI_S32 fd);

/*!
*******************************************************************************
** \brief receives i2s pcm frame.
**
** \param[out]  pstFrame Pointer to i2s pcm frame.
** \param[in]  block block/non-block flags.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_i2s_send_frame
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_get_frame(GADI_I2S_FrameT *pstFrame, GADI_BOOL block);

/*!
*******************************************************************************
** \brief transmits i2s pcm frame.
**
** \param[in]  pstFrame Pointer to i2s pcm frame.
** \param[in]  block block/non-block flags.
**
** \return
** - #GADI_OK
** - #GADI_AUDIO_ERR_BAD_PARAMETER
** - #GADI_AUDIO_ERR_FROM_DRIVER
**
** \sa gadi_i2s_get_frame
**
*******************************************************************************
*/
GADI_ERR gadi_i2s_send_frame(GADI_I2S_FrameT *pstFrame, GADI_BOOL block);

/*!
*******************************************************************************
** \brief Gets fd of i2s rx device.
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
GADI_S32 gadi_i2s_rx_get_fd(void);

/*!
*******************************************************************************
** \brief Gets fd of tx rx device.
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
GADI_S32 gadi_i2s_tx_get_fd(void);

#ifdef __cplusplus
    }
#endif



#endif /* _ADI_I2S_H_ */
