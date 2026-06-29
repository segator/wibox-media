/*!
*****************************************************************************
** \file        adi/include/adi_venc.h
**
** \brief       ADI video encode module header file
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2012-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_VENC_H_
#define _ADI_VENC_H_

#include "stdio.h"
#include "adi_types.h"

//*****************************************************************************
//*****************************************************************************
//** Defines and Macros
//*****************************************************************************
//*****************************************************************************

/*! The num of chan .. */
#define MAX_STREAM_NUM  4 /*MAX STREAM*/

/*
*******************************************************************************
** Defines for general error codes of the module.
*******************************************************************************
*/
/*! Bad parameter passed. */
#define GADI_VENC_ERR_BAD_PARAMETER                                          \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_VENC_ERR_OUT_OF_MEMORY                                          \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_VENC_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_VENC_ERR_NOT_INITIALIZED                                        \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_VENC_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_VENC_ERR_TIMEOUT                                                \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_VENC_ERR_DEVICE_BUSY                                            \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_VENC_ERR_INVALID_HANDLE                                         \
                              (GADI_VENC_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_VENC_ERR_SEMAPHORE_CREATE                                       \
                               (GADI_VENC_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_VENC_ERR_UNSUPPORTED_VERSION                                    \
                               (GADI_VENC_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_VENC_ERR_FROM_DRIVER                                            \
                               (GADI_VENC_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_VENC_ERR_NOT_OPEN                                               \
                               (GADI_VENC_MODULE_BASE + GADI_ERR_NOT_OPEN)
/*! The device/map is err.. */
#define GADI_VENC_ERR_MAP                                               \
                               (GADI_VENC_MODULE_BASE + GADI_ERR_MAP)


//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief video encode output stream enum.
*******************************************************************************
*/
typedef enum {
    GADI_VENC_STREAM_FIRST = 0,
    GADI_VENC_STREAM_SECOND,
    GADI_VENC_STREAM_THIRD,
    GADI_VENC_STREAM_FORTH,
    GADI_VENC_STREAM_NUM,
} GADI_VENC_StreamEnumT;

/*!
*******************************************************************************
** \brief video encode stream state enum.
*******************************************************************************
*/
typedef enum {
    /*uninitialized or unconfigured*/
    GADI_VENC_STREAM_STATE_UNKNOWN = 0,
    /*configured ready, but not started encoding yet*/
    GADI_VENC_STREAM_STATE_READY_FOR_ENCODING = 1,
    /*encoding*/
    GADI_VENC_STREAM_STATE_ENCODING = 2,
    /*transition state: starting to encode*/
    GADI_VENC_STREAM_STATE_STARTING = 3,
    /*transition state: stopping encoding*/
    GADI_VENC_STREAM_STATE_STOPPING = 4,
    /*known error*/
    GADI_VENC_STREAM_STATE_ERROR    = 255,
} GADI_VENC_StreamStateEnumT;


/*!
*******************************************************************************
** \brief video encode channel enum.
*******************************************************************************
*/
typedef enum {
    GADI_VENC_CHANNEL_1 = 0,
    GADI_VENC_CHANNEL_2,
    GADI_VENC_CHANNEL_3,
    GADI_VENC_CHANNEL_4,
    GADI_VENC_CHANNEL_NUM,
} GADI_VENC_ChannelEnumT;

/*!
*******************************************************************************
** \brief video encode channel state enum.
*******************************************************************************
*/
typedef enum {
    /*uninitialized or unconfigured*/
    GADI_VENC_CHANNEL_STATE_UNKNOWN = 0,
    /*configured, but not used by any stream to encode*/
    GADI_VENC_CHANNEL_STATE_IDLE    = 1,
    /*configured and used by at least one stream to encode*/
    GADI_VENC_CHANNEL_STATE_BUSY    = 2,
    /*known error*/
    GADI_VENC_CHANNEL_STATE_ERROR   = 255,
}GADI_VENC_ChannelStateEnumT;


/*!
*******************************************************************************
** \brief video encode channel type for used enum.
*******************************************************************************
*/
typedef enum {
    /*channel disabled*/
    GADI_VENC_CHANNEL_TYPE_OFF = 0,
    /*channel for encoding*/
    GADI_VENC_CHANNEL_TYPE_ENCODE = 1,
    /*channel for preview(CVBS output)*/
    GADI_VENC_CHANNEL_TYPE_PREVIEW = 2,
} GADI_VENC_ChannelTypeEnumT;

/*!
*******************************************************************************
** \brief video encode type enum.
*******************************************************************************
*/
typedef enum {
    GADI_VENC_TYPE_OFF = 0,
    GADI_VENC_TYPE_H264,
    GADI_VENC_TYPE_MJPEG,
} GADI_VENC_TypeEnumT;

/*!
*******************************************************************************
** \brief video encode bit rate control mode enum.
*******************************************************************************
*/
typedef enum {
    GADI_VENC_CBR_MODE = 0,
    GADI_VENC_VBR_MODE,
    GADI_VENC_CBR_QUALITY_MODE,
    GADI_VENC_VBR_QUALITY_MODE,
    GADI_VENC_BRC_MODE_NUM,
}GADI_VENC_BrcModeEnumT;

/*!
*******************************************************************************
** \brief video encode support frame rate.
*******************************************************************************
*/
typedef enum {
    /*use default sensor frame rate.*/
    GADI_VENC_FPS_AUTO   = 0,
    /*fps:1.*/
    GADI_VENC_FPS_1      = 1,
    GADI_VENC_FPS_2      = 2,
    GADI_VENC_FPS_3      = 3,
    GADI_VENC_FPS_4      = 4,
    GADI_VENC_FPS_5      = 5,
    GADI_VENC_FPS_6      = 6,
    GADI_VENC_FPS_7      = 7,
    GADI_VENC_FPS_8      = 8,
    GADI_VENC_FPS_9      = 9,
    GADI_VENC_FPS_10     = 10,
    GADI_VENC_FPS_11     = 11,
    GADI_VENC_FPS_12     = 12,
    GADI_VENC_FPS_13     = 13,
    GADI_VENC_FPS_14     = 14,
    GADI_VENC_FPS_15     = 15,
    GADI_VENC_FPS_16     = 16,
    GADI_VENC_FPS_17     = 17,
    GADI_VENC_FPS_18     = 18,
    GADI_VENC_FPS_19     = 19,
    GADI_VENC_FPS_20     = 20,
    GADI_VENC_FPS_21     = 21,
    GADI_VENC_FPS_22     = 22,
    GADI_VENC_FPS_23     = 23,
    GADI_VENC_FPS_24     = 24,
    GADI_VENC_FPS_25     = 25,
    GADI_VENC_FPS_26     = 26,
    GADI_VENC_FPS_27     = 27,
    GADI_VENC_FPS_28     = 28,
    GADI_VENC_FPS_29     = 29,
    GADI_VENC_FPS_30     = 30,
    GADI_VENC_FPS_50     = 50,
    GADI_VENC_FPS_60     = 60,
    GADI_VENC_FPS_120    = 120,
    /*fps:3.125.*/
    GADI_VENC_FPS_3_125  = 3125000,
    /*fps:3.75.*/
    GADI_VENC_FPS_3_75   = 37500,
    /*fps:6.25.*/
    GADI_VENC_FPS_6_25   = 62500,
    /*fps:7.5.*/
    GADI_VENC_FPS_7_5    = 750,
    /*fps:12.5.*/
    GADI_VENC_FPS_12_5   = 1250,
    /*fps:23.976.*/
    GADI_VENC_FPS_23_976 = 23976000,
    /*fps:29.97.*/
    GADI_VENC_FPS_29_97  = 299700,
    /*fps:59.94.*/
    GADI_VENC_FPS_59_94  = 599400,
}GADI_VENC_FrameRateEnumT;

/*!
*******************************************************************************
** \brief video encode stream frame type.
*******************************************************************************
*/
typedef enum
{
    GADI_VENC_IDR_FRAME  = 1,
    GADI_VENC_I_FRAME    = 2,
    GADI_VENC_P_FRAME    = 3,
    GADI_VENC_B_FRAME    = 4,
    GADI_VENC_JPEG_FRAME = 5,
}GADI_VENC_FrameTypeEnumT;


/*!
*******************************************************************************
** \brief video h264 stream reencode threshold enum.
*******************************************************************************
*/
typedef enum
{
    GADI_VENC_REENC_THRESH_0    = 0,
    GADI_VENC_REENC_THRESH_1    = 1,
    GADI_VENC_REENC_THRESH_2    = 2,
    GADI_VENC_REENC_THRESH_3    = 3,
    GADI_VENC_REENC_THRESH_4    = 4,
    GADI_VENC_REENC_THRESH_5    = 5,
    GADI_VENC_REENC_THRESH_6    = 6,
    GADI_VENC_REENC_THRESH_7    = 7,
    GADI_VENC_REENC_THRESH_NUM  = 8,
}GADI_VENC_ReEncThreshEnumT;

/*!
*******************************************************************************
** \brief video h264 stream strength enum.
*******************************************************************************
*/
typedef enum
{
    GADI_VENC_REENC_STRENGTH_0   = 0,
    GADI_VENC_REENC_STRENGTH_1   = 1,
    GADI_VENC_REENC_STRENGTH_2   = 2,
    GADI_VENC_REENC_STRENGTH_3   = 3,
    GADI_VENC_REENC_STRENGTH_4   = 4,
    GADI_VENC_REENC_STRENGTH_NUM = 5,
}GADI_VENC_ReEncStrengthEnumT;


/*!
*******************************************************************************
** \brief video h264 stream iframe size control threshold enum.
*******************************************************************************
*/
typedef enum
{
    GADI_VENC_I_SIZE_CTL_THRESH_0    = 0,
    GADI_VENC_I_SIZE_CTL_THRESH_1    = 1,
    GADI_VENC_I_SIZE_CTL_THRESH_2    = 2,
    GADI_VENC_I_SIZE_CTL_THRESH_3    = 3,
    GADI_VENC_I_SIZE_CTL_THRESH_4    = 4,
    GADI_VENC_I_SIZE_CTL_THRESH_5    = 5,
    GADI_VENC_I_SIZE_CTL_THRESH_NUM  = 6,
}GADI_VENC_ISizeCtlThreshEnumT;

/*!
*******************************************************************************
** \brief video h264 stream iframe size control strength enum.
*******************************************************************************
*/
typedef enum
{
    GADI_VENC_I_SIZE_CTL_STRENGTH_0   = 0,
    GADI_VENC_I_SIZE_CTL_STRENGTH_1   = 1,
    GADI_VENC_I_SIZE_CTL_STRENGTH_2   = 2,
    GADI_VENC_I_SIZE_CTL_STRENGTH_3   = 3,
    GADI_VENC_I_SIZE_CTL_STRENGTH_4   = 4,
    GADI_VENC_I_SIZE_CTL_STRENGTH_5   = 5,
    GADI_VENC_I_SIZE_CTL_STRENGTH_NUM = 6,
}GADI_VENC_ISizeCtlStrengthEnumT;

/*!
*******************************************************************************
** \brief encoder buffer mode enum.
*******************************************************************************
*/
typedef enum
{
    GADI_VENC_BUF_MERGE_MODE_0   = 0,
    GADI_VENC_BUF_MERGE_MODE_1   = 1,
    GADI_VENC_BUF_MERGE_MODE_NUM = 2,
}GADI_VENC_BufModeEnumT;


//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief one channel state struct.
*******************************************************************************
*/
typedef struct
{
    /*channel index.*/
    GADI_U32            channelId;
    /*channel state.*/
    GADI_U32            state;
} GADI_VENC_ChannelStateT;

/*!
*******************************************************************************
** \brief channel parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*first channel(capture buffer) type 0:disable, 1:for encoding, 2:for preview.*/
    GADI_U32            chan1Type;
    /*first channel buffer width.*/
    GADI_U16            chan1Width;
    /*first channel buffer height.*/
    GADI_U16            chan1Height;
    /*deintlc mode of first channel buffer.*/
    GADI_U16            chan1DeintlcForIntlcVin;
    /*second channel type.*/
    GADI_U32            chan2Type;
    /*second channel buffer width.*/
    GADI_U16            chan2Width;
    /*second channel buffer height.*/
    GADI_U16            chan2Height;
    /*deintlc mode of second channel buffer.*/
    GADI_U16            chan2DeintlcForIntlcVin;
    /*third channel type.*/
    GADI_U32            chan3Type;
    /*third channel buffer width.*/
    GADI_U16            chan3Width;
    /*third channel buffer height.*/
    GADI_U16            chan3Height;
    /*deintlc mode of third channel buffer.*/
    GADI_U16            chan3DeintlcForIntlcVin;
    /*fourth channel type.*/
    GADI_U32            chan4Type;
    /*fourth channel buffer width.*/
    GADI_U16            chan4Width;
    /*third channel buffer height.*/
    GADI_U16            chan4Height;
    /*deintlc mode of fourth channel buffer.*/
    GADI_U16            chan4DeintlcForIntlcVin;
    /*0: OFF  1: use progressive VIN to encode interlaced video*/
    GADI_U8             intlcScan;
}GADI_VENC_ChannelsParamsT;

/*!
*******************************************************************************
** \brief one stream state struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;
    /*stream state.*/
    GADI_U32            state;
} GADI_VENC_StreamStateT;

/*!
*******************************************************************************
** \brief one stream format struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32                    streamId;
    /*0: none, 1: H.264, 2: MJPEG*/
    GADI_U8                     encodeType;
    /*channel index:GADI_VENC_ChannelEnumT.*/
    GADI_U8                     channelId;
    /*rotate: 0x01:means flip horizontal,0x02:means flip vertical, 0x04:means rotate 90.*/
    GADI_U8                     flipRotate;
    /*encode width.*/
    GADI_U16                    width;
    /*encode height.*/
    GADI_U16                    height;
    /*encode x offset.*/
    GADI_U16                    xOffset;
    /*encode y offset.*/
    GADI_U16                    yOffset;
    /*encode frame rate.*/
    GADI_VENC_FrameRateEnumT    fps;
    /*encode video keep aspect ratio.*/
    /*vi width scale to video width ratio not equal video height ratio.*/
    /*keep aspect ration means use the small aspect ratio.*/
    /* 1: means keep aspect ration, 0: means do not keep.*/
    GADI_U8                     keepAspRat;
}GADI_VENC_StreamFormatT;

/*!
*******************************************************************************
** \brief one h264 stream configuration struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;
    /*gop M value.*/
    GADI_U8             gopM;
    /*gop N value.*/
    GADI_U8             gopN;
    /*IDR interval .*/
    GADI_U8             idrInterval;
    /*gop model.*/
    GADI_U8             gopModel;
    /*encode profile.*/
    GADI_U8             profile;
    /*0: CBR; 1: VBR; 2: CBR keep quality; 3: VBR keep quality: GADI_VENC_BrcModeEnumT*/
    GADI_U8             brcMode;
    /*cbr mode, bit rate.*/
    GADI_U32            cbrAvgBps;
    /*vbr mode, min bit rate.*/
    GADI_U32            vbrMinbps;
    /*vbr mode, max bit rate.*/
    GADI_U32            vbrMaxbps;
    /*reencode mode: 0:disable(default), 1:enable.*/
    GADI_U8             reEncMode;
}GADI_VENC_H264ConfigT;

/*!
*******************************************************************************
** \brief one h264 stream qp configuration struct. on the fly.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;
    /*rate control factor. value:1~51, qpMinOnI <= qpMinOnP.*/
    GADI_U8             qpMinOnI;
    /*rate control factor.qpMinOnI <= qpMaxOnI*/
    GADI_U8             qpMaxOnI;
    /*rate control factor. value:1~51, qpMinOnI <= qpMinOnP.*/
    GADI_U8             qpMinOnP;
    /*rate control factor.qpMinOnP <=  qpMaxOnP*/
    GADI_U8             qpMaxOnP;
    /*rate control factor: I frame qp weight, range: 1~10,*/
    GADI_U8             qpIWeight;
    /*rate control factor. P frame qp weight, range: 1~5.*/
    GADI_U8             qpPWeight;
    /*picture quality consistency, range: 0~2. 2:will be best.*/
    GADI_U8             adaptQp;
}GADI_VENC_H264QpConfigT;

/*!
*******************************************************************************
** \brief one h264 stream gop N configuration struct. on the fly.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;
    /*gop N value.*/
    GADI_U8             gopN;
}GADI_VENC_H264GopConfigT;

typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;
    GADI_U8             i16x16Cost;         // 0~128,
    GADI_U8             i4x4Cost;           // 0~128,
    GADI_U8             p16x16Cost;         // 0~128,
    GADI_U8             p8x8Cost;           // 0~128,
    GADI_U8             skipChance;
    GADI_U8             uvQpOffset;         // between 0 and 24
    GADI_U8             dbfAlpha;           // between 0 and 12
    GADI_U8             dbfBeta;            // between 0 and 12
}GADI_VENC_BiasConfigT;

typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;

    GADI_U16            iChance;
}GADI_VENC_IConfigT;

typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;
    GADI_U8             zeroMvTh;
}GADI_VENC_ZeromvConfigT;

/*!
*******************************************************************************
** \brief one MJPEG stream configuration struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32            streamId;
    /*0: YUV 422, 1: YUV 420.*/
    GADI_U8             chromaFormat;
    /*1 ~ 100, 100 is best quality.*/
    GADI_U8             quality;
} GADI_VENC_MjpegConfigT;

/*!
*******************************************************************************
** \brief video encode module open parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*video input module handle.*/
    GADI_SYS_HandleT            viHandle;
    /*video output module handle.*/
    GADI_SYS_HandleT            voutHandle;
}GADI_VENC_OpenParamsT;


/*!
*******************************************************************************
** \brief setting video encode bitrate range parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index for setting the bitrate.*/
    GADI_U32            streamId;
    /*0: CBR; 1: VBR; 2: CBR keep quality; 3: VBR keep quality: GADI_VENC_BrcModeEnumT*/
    GADI_U8             brcMode;
    /*cbr mode, bit rate.*/
    GADI_U32            cbrAvgBps;
    /*vbr mode, min bit rate.*/
    GADI_U32            vbrMinbps;
    /*vbr mode, max bit rate.*/
    GADI_U32            vbrMaxbps;
}GADI_VENC_BitRateRangeT;

/*!
*******************************************************************************
** \brief settting video encode frame rate parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index for setting the bitrate.*/
    GADI_U32                    streamId;
    /*encoding stream target frame rate value.*/
    GADI_VENC_FrameRateEnumT    fps;
}GADI_VENC_FrameRateT;


/*!
*******************************************************************************
** \brief digital PTZ setting parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*channel index.*/
    GADI_U32            channelId;
    /*zoom factor.*/
    GADI_U32            zoomFactor;
    /*zoom offset x.*/
    GADI_S32            offsetX;
    /*zoom offset y.*/
    GADI_S32            offsetY;
}GADI_VENC_DptzParamT;

/*!
*******************************************************************************
** \brief getting digital PTZ orginal information parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*channel index.*/
    GADI_U32            channelId;
    /*zoom x factor.*/
    GADI_U32            zoomFactorX;
    /*zoom y factor.*/
    GADI_U32            zoomFactorY;
    /*zoom offset x.*/
    GADI_S32            offsetX;
    /*zoom offset y.*/
    GADI_S32            offsetY;
}GADI_VENC_DptzOrgParamT;


/*!
*******************************************************************************
** \brief getting VENC attr information.
*******************************************************************************
*/
typedef struct
{
    /*stream id.*/
    GADI_U32 id;
    /*stream type.*/
    GADI_VENC_TypeEnumT type;
    /*stream state.*/
    GADI_VENC_StreamStateEnumT state;
}GADI_CHN_AttrT;

/*!
*******************************************************************************
** \brief getting VENC Stream information.
*******************************************************************************
*/
typedef struct
{
    GADI_U32    stream_id;
    GADI_U32    size;
    GADI_U32    frame_num;
    GADI_U64    PTS;
    GADI_U8     *addr;
    GADI_U32    stream_end : 1;
    GADI_U32    pic_type : 3;
}GADI_VENC_StreamT;

/*!
*******************************************************************************
** \brief DSP map memory informations.
*******************************************************************************
*/
typedef struct
{
    GADI_U8    *addr;
    GADI_U32    length;
}GADI_VENC_DspMapInfoT;


/*!
*******************************************************************************
** \brief roi region informations.
*******************************************************************************
*/
typedef struct
{
    GADI_U32            offsetX;
    GADI_U32            offsetY;
    GADI_U32            width;
    GADI_U32            height;
}GADI_VENC_RoiRegionInfoT;

/*!
*******************************************************************************
** \brief roi configuration struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32                  streamId;
    /*index of roi, supports indexes range: 0~4.*/
    GADI_U32                  roiIndex;
    /*relative QP of I frame*/
    GADI_S8                   relativeQpI;
    /*relative QP of P frame*/
    GADI_S8                   relativeQpP;
    /*the roi enable flags*/
    GADI_U8                   roiEnable;
    /*region info of the roi.*/
    GADI_VENC_RoiRegionInfoT  roiRegionInfo;
}GADI_VENC_RoiConfigT;


/*!
*******************************************************************************
** \brief h264 stream reencode config parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32                     streamId;
    /*reencode threshold:scenes from simple to complex.*/
    GADI_VENC_ReEncThreshEnumT   threshStC;
    /*reencode strength:scenes from simple to complex.*/
    GADI_VENC_ReEncStrengthEnumT strengthStC;
    /*reencode threshold:scenes from complex to simple.*/
    GADI_VENC_ReEncThreshEnumT   threshCtS;
    /*reencode strength:scenes from complex to simple.*/
    GADI_VENC_ReEncStrengthEnumT strengthCtS;
}GADI_VENC_ReEncConfigT;

/*!
*******************************************************************************
** \brief h264 stream iframe size control parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32                        streamId;
    /*iframe size control threshold.*/
    GADI_VENC_ISizeCtlThreshEnumT   IsizeCtlThresh;
    /*iframe size control strength.*/
    GADI_VENC_ISizeCtlStrengthEnumT IsizeCtlStrength;
}GADI_VENC_IsizeCtlT;


/*!
*******************************************************************************
** \brief h264 stream MB QP control parameters struct.
*******************************************************************************
*/
typedef struct
{
    /*stream index.*/
    GADI_U32    streamId;
    /*threshold value of MB QP control.*/
    u16         mbQpThrds[15];
    /*QP delta value of MB QP control.*/
    s8          mbQpDelta[16];
    /*enable MB QP control. 0:disable, 1:enable.*/
    u8          mbQpCtlEnalbe;
}GADI_VENC_MbQpCtlT;

/*!
*******************************************************************************
** \brief change one channel resolution struct.
*******************************************************************************
*/
typedef struct
{
    /*channel index.*/
    GADI_U32    channelId;
    /*channel width.*/
    GADI_U16    width;
    /*channel height.*/
    GADI_U16    height;
}GADI_VENC_ChanlParamsT;


//*****************************************************************************
//*****************************************************************************
//** Global Data
//*****************************************************************************
//*****************************************************************************



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
** \brief Initialize the ADI video encode module.
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_OUT_OF_MEMORY
** - #GADI_VENC_ERR_ALREADY_INITIALIZED
**
** \sa gadi_vi_exit
**
*******************************************************************************
*/
GADI_ERR gadi_venc_init(void);

/*!
*******************************************************************************
** \brief Shutdown the ADI video encode module.
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_NOT_INITIALIZED
**
** \sa gadi_vi_init
**
*******************************************************************************
*/
GADI_ERR gadi_venc_exit(void);

/*!
*******************************************************************************
** \brief Open one ADI video venc module instance.
**
** \param[in] openParams a struct pointer of open prameters.
** \param[in] errorCodePtr pointer to return the error code.
**
** \return Return an valid handle of ADI vi module instance.
**
** \sa gadi_venc_close
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_venc_open(GADI_VENC_OpenParamsT* openParams, GADI_ERR* errorCodePtr);

/*!
*******************************************************************************
** \brief close one ADI video encode module instance.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief set encoder buffer mode.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_buf_mode(GADI_SYS_HandleT handle,
        GADI_VENC_BufModeEnumT mode);

/*!
*******************************************************************************
** \brief start encoding one stream.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  streamId stream id enum:GADI_VENC_StreamEnumT;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_stop_stream
**
*******************************************************************************
*/
GADI_ERR gadi_venc_start_stream(GADI_SYS_HandleT handle, GADI_U32 streamId);

/*!
*******************************************************************************
** \brief stop encoding one stream.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  streamId stream id enum:GADI_VENC_StreamEnumT;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_start_stream
**
*******************************************************************************
*/
GADI_ERR gadi_venc_stop_stream(GADI_SYS_HandleT handle, GADI_U32 streamId);

/*!
*******************************************************************************
** \brief force one stream IDR .
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  streamId stream id enum:GADI_VENC_StreamEnumT;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_force_idr(GADI_SYS_HandleT handle, GADI_U32 streamId);
/*!
*******************************************************************************
** \brief get one stream bit rate.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  brPar pointer of setting bitrate struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_set_bitrate
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_bitrate(GADI_SYS_HandleT handle,
    GADI_VENC_BitRateRangeT *brPar);
/*!
*******************************************************************************
** \brief set one stream bit rate.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  brPar pointer of setting bitrate struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_get_bitrate
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_bitrate(GADI_SYS_HandleT handle, GADI_VENC_BitRateRangeT* brPar);

/*!
*******************************************************************************
** \brief set one stream frame rate.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  frPar pointer of setting frame rate struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_framerate(GADI_SYS_HandleT handle, GADI_VENC_FrameRateT* frPar);

/*!
*******************************************************************************
** \brief set one channel state.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  statePar pointer of channel state struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_channel_state(GADI_SYS_HandleT handle,
                                             GADI_VENC_ChannelStateT* statePar);

/*!
*******************************************************************************
** \brief get all(four) channels params.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  chansParams pointer of all channels parameters struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_channels_params(GADI_SYS_HandleT handle,
                                         GADI_VENC_ChannelsParamsT *chansParams);


/*!
*******************************************************************************
** \brief set all(four) channels parameters.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  bufsParams pointer of all channels parameters struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_channels_params(GADI_SYS_HandleT handle,
                                      GADI_VENC_ChannelsParamsT* chansParams);

/*!
*******************************************************************************
** \brief get one encode stream state.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  statePar pointer of stream state struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_stream_state(GADI_SYS_HandleT handle,
                                               GADI_VENC_StreamStateT* statePar);

/*!
*******************************************************************************
** \brief get one encode stream format.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  formatPar pointer of encode stream format struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_stream_format(GADI_SYS_HandleT handle,
                                             GADI_VENC_StreamFormatT *formatPar);

/*!
*******************************************************************************
** \brief set one encode stream format.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  formatPar pointer of encode stream format struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_stream_format(GADI_SYS_HandleT handle,
                                             GADI_VENC_StreamFormatT* formatPar);

/*!
*******************************************************************************
** \brief get one h264 encode stream configuration.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  configPar pointer of h264 stream configuration struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_config(GADI_SYS_HandleT handle,
                                               GADI_VENC_H264ConfigT* configPar);

/*!
*******************************************************************************
** \brief set one h264 encode stream configuration.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  configPar pointer of h264 stream configuration struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_config(GADI_SYS_HandleT handle,
                                               GADI_VENC_H264ConfigT* configPar);

/*!
*******************************************************************************
** \brief get one h264 encode stream qp configuration.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  configPar pointer of h264 stream qp configuration struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_qp_config(GADI_SYS_HandleT handle,
                                                GADI_VENC_H264QpConfigT* configPar);


/*!
*******************************************************************************
** \brief set one h264 encode stream qp configuration.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  configPar pointer of h264 stream qp configuration struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_qp_config(GADI_SYS_HandleT handle,
                                               GADI_VENC_H264QpConfigT* configPar);


/*!
*******************************************************************************
** \brief get one mjpeg encode stream configuration.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  configPar pointer of mjpeg stream configuration struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_mjpeg_config(GADI_SYS_HandleT handle,
                                              GADI_VENC_MjpegConfigT* configPar);

/*!
*******************************************************************************
** \brief set one mjpeg encode stream configuration.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  configPar pointer of mjpeg stream configuration struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_mjpeg_config(GADI_SYS_HandleT handle,
                                              GADI_VENC_MjpegConfigT* configPar);

/*!
*******************************************************************************
** \brief get digital ptz parameter.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[out]  dptzPar pointer of digital ptz struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_dptz_param(GADI_SYS_HandleT handle, GADI_VENC_DptzParamT* dptzPar);

/*!
*******************************************************************************
** \brief set digital ptz parameter.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  dptzPar pointer of digital ptz struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_dptz_param(GADI_SYS_HandleT handle,
                                              GADI_VENC_DptzParamT* vencDptzPar);

/*!
*******************************************************************************
** \brief get digital ptz orginal parameter.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   vencDptzPar
** \param[out]  vencDptzOrgPar pointer of digital ptz orginal parameters struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_dptz_org_param(GADI_SYS_HandleT handle,
                                                GADI_VENC_DptzParamT *vencDptzPar,
                                                GADI_VENC_DptzOrgParamT *vencDptzOrgPar);

/*!
*******************************************************************************
** \brief map dsp memory.
**
** \param[in]  venHandle   Valid ADI venc instance handle previously opened by
**                          #gadi_venc_open.
** \param[out]  info       pointer of dsp map memory struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_map_dsp(GADI_SYS_HandleT handle, GADI_VENC_DspMapInfoT *info);

/*!
*******************************************************************************
** \brief map bsb.
**
** \param[in]  venHandle     Valid ADI venc instance handle previously opened by
**                          #gadi_venc_open.
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_map_bsb(GADI_SYS_HandleT handle);
/*!
*******************************************************************************
** \brief query status of parameter.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   streamId.
** \param[out]  chn_attr pointer of GADI_CHN_AttrT parameters struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_query(GADI_SYS_HandleT handle, GADI_S32 streamId,
GADI_CHN_AttrT * chn_attr);
/*!
*******************************************************************************
** \brief get stream of current.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   streamId.
** \param[out]  stream    pointer of GADI_VENC_StreamT parameters struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_stream(GADI_SYS_HandleT handle, GADI_S32 streamId,
    GADI_VENC_StreamT * stream);

/*!
*******************************************************************************
** \brief print out all parameters.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_print_params(GADI_SYS_HandleT handle);


/*!
*******************************************************************************
** \brief check encoder state, if encoder state is iavalid, this API will return
**        GADI_VENC_ERR_FROM_DRIVER, and print state in screen.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   streamId.
**
** \return
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_OK
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_check_encoder_state(GADI_SYS_HandleT handle);


/*!
*******************************************************************************
** \brief           get H264 gop value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_H264GopNConfigT.streamId.
**
** \param[ou]   GADI_VENC_H264GopNConfigT.gopN.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_get_h264_gop
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_gop(GADI_SYS_HandleT handle,
        GADI_VENC_H264GopConfigT* configPar);

/*!
*******************************************************************************
** \brief           set H264 gop value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_H264GopNConfigT.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_gop
**
*******************************************************************************
*/

GADI_ERR gadi_venc_set_h264_gop(GADI_SYS_HandleT handle,
        GADI_VENC_H264GopConfigT* configPar);

/*!
*******************************************************************************
** \brief           get H264 enc value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_EncConfigT.streamId.
**
** \param[ou]   GADI_VENC_EncConfigT.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_get_h264_enc
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_iconfig(GADI_SYS_HandleT handle,
        GADI_VENC_IConfigT * configPar);

/*!
*******************************************************************************
** \brief           set H264 enc value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_EncConfigT.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_enc
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_iconfig(GADI_SYS_HandleT handle,
        GADI_VENC_IConfigT * configPar);

/*!
*******************************************************************************
** \brief           get H264 bias value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_BiasConfigT.streamId.
**
** \param[ou]   GADI_VENC_BiasConfigT.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_get_h264_bias
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_bias(GADI_SYS_HandleT handle,
        GADI_VENC_BiasConfigT * configPar);

/*!
*******************************************************************************
** \brief           set H264 bias value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_BiasConfigT.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_bias
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_bias(GADI_SYS_HandleT handle,
        GADI_VENC_BiasConfigT * configPar);

/*!
*******************************************************************************
** \brief           get H264 zmv value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_ZeromvConfigT.streamId.
**
** \param[ou]   GADI_VENC_ZeromvConfigT.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_get_h264_zmv
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_zeromv(GADI_SYS_HandleT handle,
        GADI_VENC_ZeromvConfigT * configPar);

/*!
*******************************************************************************
** \brief           set H264 zmv value.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   GADI_VENC_ZeromvConfigT.
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_zmv
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_zeromv(GADI_SYS_HandleT handle,
        GADI_VENC_ZeromvConfigT * configPar);
/*!
*******************************************************************************
** \brief           Gets a ROI config of a h264 stream.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   roiIndex     ROI index
** \param[ou]   configPar    pointer of a roi configuration struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_qp_roi_config
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_qp_roi_config(GADI_SYS_HandleT handle,
    GADI_U32 roiIndex, GADI_VENC_RoiConfigT *configPar);
/*!
*******************************************************************************
** \brief           Sets a ROI config of a h264 stream.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   configPar  pointer of a roi configuration struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_get_qp_roi_config
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_qp_roi_config(GADI_SYS_HandleT handle,
    GADI_VENC_RoiConfigT *configPar);

/*!
*******************************************************************************
** \brief           Gets a reencode config of a h264 stream.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   reEncConfig  pointer of a reencode configuration struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_reenc
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_reenc(GADI_SYS_HandleT handle,
    GADI_VENC_ReEncConfigT * reEncConfig);

/*!
*******************************************************************************
** \brief           Sets a reencode config of a h264 stream.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   reEncConfig  pointer of a reencode configuration struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_reenc
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_reenc(GADI_SYS_HandleT handle,
    GADI_VENC_ReEncConfigT * reEncConfig);

/*!
*******************************************************************************
** \brief           Gets a iframe size config of a h264 stream.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   IsizeCtl  pointer of a iframe size control configuration struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_reenc
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_iframe_size(GADI_SYS_HandleT handle,
    GADI_VENC_IsizeCtlT * IsizeCtl);

/*!
*******************************************************************************
** \brief           Sets a iframe size config of a h264 stream.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   IsizeCtl  pointer of a iframe size control configuration struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_reenc
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_iframe_size(GADI_SYS_HandleT handle,
    GADI_VENC_IsizeCtlT * IsizeCtl);


/*!
*******************************************************************************
** \brief   Gets h264 stream MB QP control parameters.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   mbQpCtlPara  pointer of MB QP control parameters struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_set_h264_mb_qp_ctl
**
*******************************************************************************
*/
GADI_ERR gadi_venc_get_h264_mb_qp_ctl(GADI_SYS_HandleT handle,
    GADI_VENC_MbQpCtlT *mbQpCtlPara);

/*!
*******************************************************************************
** \brief   Sets h264 stream MB QP control parameters.
**
** \param[in]   handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]   mbQpCtlPara  pointer of MB QP control parameters struct;
**
** \return
** - #GADI_OK
**
** \sa gadi_venc_get_h264_mb_qp_ctl
**
*******************************************************************************
*/
GADI_ERR gadi_venc_set_h264_mb_qp_ctl(GADI_SYS_HandleT handle,
    GADI_VENC_MbQpCtlT *mbQpCtlPara);


/*!
*******************************************************************************
** \brief change specified channel parameters on the fly.
**
** \param[in]  handle     Valid ADI venc instance handle previously opened by
**                        #gadi_venc_open.
** \param[in]  chanlPar pointer of  channel parameters struct;
**
** \return
** - #GADI_OK
** - #GADI_VENC_ERR_FROM_DRIVER
** - #GADI_VENC_ERR_BAD_PARAMETER
**
** \sa gadi_venc_open
**
*******************************************************************************
*/
GADI_ERR gadi_venc_change_channel_onfly(GADI_SYS_HandleT handle,
    GADI_VENC_ChanlParamsT * chanlPar);


#ifdef __cplusplus
}
#endif



#endif /* _ADI_VENC_H_ */

