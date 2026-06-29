/*!
*****************************************************************************
** \file        ./adi/inc/adi_vdec.h
**
** \brief       adi video decode porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2014-2015 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_VDEC_H_
#define _ADI_VDEC_H_

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
#define GADI_VDEC_ERR_BAD_PARAMETER                                          \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_VDEC_ERR_OUT_OF_MEMORY                                          \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_VDEC_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_VDEC_ERR_NOT_INITIALIZED                                        \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_VDEC_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_VDEC_ERR_TIMEOUT                                                \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_VDEC_ERR_DEVICE_BUSY                                            \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_VDEC_ERR_INVALID_HANDLE                                         \
                              (GADI_VDEC_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_VDEC_ERR_SEMAPHORE_CREATE                                       \
                               (GADI_VDEC_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_VDEC_ERR_UNSUPPORTED_VERSION                                    \
                               (GADI_VDEC_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_VDEC_ERR_FROM_DRIVER                                            \
                               (GADI_VDEC_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_VDEC_ERR_NOT_OPEN                                               \
                               (GADI_VDEC_MODULE_BASE + GADI_ERR_NOT_OPEN)
    /*! The device/map is err.. */
#define GADI_VDEC_ERR_MAP                                               \
                               (GADI_VDEC_MODULE_BASE + GADI_ERR_MAP)


//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************
typedef enum
{
    /*fast decode 1X.*/
    GADI_VDEC_SPEED_1X = 0,
    /*fast decode 1X.*/
    GADI_VDEC_SPEED_2X,
    /*fast decode 1X.*/
    GADI_VDEC_SPEED_4X,
    /*slow decode 1/2.*/
    GADI_VDEC_SPEED_1BY2,
    /*slow decode 1/4.*/
    GADI_VDEC_SPEED_1BY4,
}GADI_VDEC_SpeedEnumT;

typedef enum
{
    GADI_VDEC_WAIT_BSB          =(1 << 0),
    GADI_VDEC_WAIT_FRAME        =(1 << 1),
    GADI_VDEC_WAIT_EOS          =(1 << 2),
    GADI_VDEC_WAIT_BUFFER       =(1 << 3),

    GADI_VDEC_WAIT_BITS_FIFO    =(1 << 4),  // udec
    GADI_VDEC_WAIT_MV_FIFO      =(1 << 5),  // udec
    GADI_VDEC_WAIT_OUTPIC       =(1 << 6),  // udec
    GADI_VDEC_WAIT_UDEC_EOS     =(1 << 7),  // udec
    GADI_VDEC_WAIT_UDEC_ERROR   =(1 << 8),  // udec
    GADI_VDEC_WAIT_VO_STATE     =(1 << 9),  // udec

    //ugly code: each dsp2 interrupt
    GADI_VDEC_WAIT_DSP2_INTERRUPT=(1 << 10),    // udec
} GADI_VDEC_WaitEnumT;

//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief decoder buffer struct.
*******************************************************************************
*/
typedef struct
{
    /*video decode bit stream buffer start address.*/
    GADI_U8     *bsbAddr;
    /*video decode bit stream buffer size.*/
    GADI_U32    bsbSize;
} GADI_VDEC_DecBufT;

/*!
*******************************************************************************
** \brief feed one jpeg data struct.
*******************************************************************************
*/
typedef struct
{
    /*feed buffer addr.*/
    GADI_U8     *addr;
    /*feed buffer size.*/
    GADI_U32    size;
} GADI_VDEC_FeedJpegInfoT;

/*!
*******************************************************************************
** \brief feed h264 stream data struct.
*******************************************************************************
*/
typedef struct
{
    /*feed buffer start addr.*/
    GADI_U8  *startAddr;
    /*feed buffer end addr.*/
    GADI_U8  *endAddr;
    /*h264 stream start pts.*/
    GADI_U32 firstPTS;
    /*number of picture in this feed buffer.*/
    GADI_U32 picNums;
    /*next feed size.*/
    GADI_U32 nextSize;
    /*picture width.*/
    GADI_U32 picWidth;
    /*picture height.*/
    GADI_U32 picHeight;
} GADI_VDEC_FeedH264InfoT;


/*!
*******************************************************************************
** \brief h264 stream decoder config struct.
*******************************************************************************
*/
typedef struct
{
    /*h264 stream picture height.*/
    GADI_U16    picHeight;
    /*h264 stream picture width.*/
    GADI_U16    picWidth;
} GADI_VDEC_ConfigInfoT;

/*!
*******************************************************************************
** \brief wait decoder struct.
*******************************************************************************
*/
typedef struct
{
    /*wait buffer addr.*/
    GADI_U8     *addr;
    /*wait buffer size.*/
    GADI_U32    size;
    /*wait buffer flag.*/
    GADI_U32    flag;
} GADI_VDEC_WaitInfoT;


/*!
*******************************************************************************
** \brief get decoder info struct.
*******************************************************************************
*/
typedef struct
{
    /*current pts.*/
    GADI_U32     currPts;
    /*current pts high.*/
    GADI_U32     currPtsHigh;
    /*numbers of frames have been decode.*/
    GADI_U32     decodedFrames;
} GADI_VDEC_DecInfoT;

typedef struct
{
    /* uv address of yuv422.*/
    GADI_U32     yuv422_uv_addr;
    /*y address of yuv422.*/
    GADI_U32     yuv422_y_addr;
    /*numbers of picture have been decode.*/
    GADI_U32     pic_num;
} GADI_VDEC_DecFrameT;

typedef struct
{
    /* uv address of yuv422.*/
    GADI_U32     scale_width;
    /*y address of yuv422.*/
    GADI_U32     scale_height;
} GADI_VDEC_DecScaleT;


/*!
*******************************************************************************
** \brief trick play struct.
*******************************************************************************
*/
typedef struct
{
    /*speed.:GADI_VDEC_SpeedEnumT*/
    GADI_U16    speed;
    /*scan mode.*/
    GADI_U8     scanMode;
    /*direction.*/
    GADI_U8     direction;
} GADI_VDEC_TrickPlayInfoT;


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
** \brief Initialize the ADI video decode module.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_ALREADY_INITIALIZED
** - #GADI_VDEC_ERR_OUT_OF_MEMORY
**
** \sa gadi_vdec_exit
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_init(void);

/*!
*******************************************************************************
** \brief Shutdown the ADI video decode module.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_NOT_INITIALIZED
**
** \sa gadi_vdec_init
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_exit(void);

/*!
*******************************************************************************
** \brief Open one ADI video decode module instance.
**
** \param[in] errorCodePtr pointer to return the error code.
**
** \return Return an valid handle of ADI vdec module instance.
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_vdec_open(GADI_ERR* errorCodePtr);

/*!
*******************************************************************************
** \brief close one ADI video decode module instance.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief map decoder buffer.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_map_buf(GADI_SYS_HandleT handle, GADI_VDEC_DecBufT *bufInfo);


/*!
*******************************************************************************
** \brief unmap decoder buffer.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_unmap_buf(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief select decoder channel.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_select_channel(GADI_SYS_HandleT handle, GADI_U32 channelId);

/*!
*******************************************************************************
** \brief start decode.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_start(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief stop decode.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_stop(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief feed one jpeg.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_feed_jpeg(GADI_SYS_HandleT handle,
                                   GADI_VDEC_FeedJpegInfoT *jpegInfo);

/*!
*******************************************************************************
** \brief feed h264 stream.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_feed_h264(GADI_SYS_HandleT handle,
                                    GADI_VDEC_FeedH264InfoT *streamInfo);

/*!
*******************************************************************************
** \brief config h264 stream decoder.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_config(GADI_SYS_HandleT handle,
                                     GADI_VDEC_ConfigInfoT *configInfo);

/*!
*******************************************************************************
** \brief wait bsb free.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_wait(GADI_SYS_HandleT handle, GADI_VDEC_WaitInfoT *waitInfo);

/*!
*******************************************************************************
** \brief get decode info.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_get_dec_info(GADI_SYS_HandleT handle, GADI_VDEC_DecInfoT *decInfo);

/*!
*******************************************************************************
** \brief set tick play.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/
GADI_ERR gadi_vdec_trick_setting(GADI_SYS_HandleT handle,
                                       GADI_VDEC_TrickPlayInfoT *trickInfo);

/*!
*******************************************************************************
** \brief pause.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/

GADI_ERR gadi_vdec_pause(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief resume.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_open.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_open
**
*******************************************************************************
*/

GADI_ERR gadi_vdec_resume(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief resume.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_get_frame.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_get_frame
**
*******************************************************************************
*/

GADI_ERR gadi_vdec_get_frame(GADI_SYS_HandleT handle, GADI_VDEC_DecFrameT *decFrame);

/*!
*******************************************************************************
** \brief resume.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_set_scale.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_set_scale
**
*******************************************************************************
*/

GADI_ERR gadi_vdec_set_scale(GADI_SYS_HandleT handle, GADI_VDEC_DecScaleT *decScale);

/*!
*******************************************************************************
** \brief resume.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_vdec_get_scale.
**
** \return
** - #GADI_OK
** - #GADI_VDEC_ERR_BAD_PARAMETER
**
** \sa gadi_vdec_get_scale
**
*******************************************************************************
*/

GADI_ERR gadi_vdec_get_scale(GADI_SYS_HandleT handle, GADI_VDEC_DecScaleT *decScale);

#ifdef __cplusplus
    }
#endif


#endif /* _ADI_VDEC_H_ */
