/*!
*****************************************************************************
** \file        ./adi/inc/adi_osd.h
**
** \brief       adi osd porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_OSD_H_
#define _ADI_OSD_H_

#include "stdio.h"
#include "adi_types.h"


//*****************************************************************************
//*****************************************************************************
//** Defines and Macros
//*****************************************************************************
//*****************************************************************************
#define OSD_PLANE_NUM           (4)
#define OSD_AREA_NUM            (3)/*the number of areas of one plane.*/


/*
*******************************************************************************
** Defines for general error codes of the module.
*******************************************************************************
*/
/*! Bad parameter passed. */
#define GADI_OSD_ERR_BAD_PARAMETER                                          \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_OSD_ERR_OUT_OF_MEMORY                                          \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_OSD_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_OSD_ERR_NOT_INITIALIZED                                        \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_OSD_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_OSD_ERR_TIMEOUT                                                \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_OSD_ERR_DEVICE_BUSY                                            \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_OSD_ERR_INVALID_HANDLE                                         \
                              (GADI_OSD_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_OSD_ERR_SEMAPHORE_CREATE                                       \
                               (GADI_OSD_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_OSD_ERR_UNSUPPORTED_VERSION                                    \
                               (GADI_OSD_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_OSD_ERR_FROM_DRIVER                                            \
                               (GADI_OSD_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_OSD_ERR_NOT_OPEN                                               \
                               (GADI_OSD_MODULE_BASE + GADI_ERR_NOT_OPEN)


//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************



//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************


/*!
*******************************************************************************
** \brief the struct of osd plane's area.
*******************************************************************************
*/
typedef struct
{
    /*the index of plane(0~3), each stream has only one plane,
    so plane index means stream index.*/
    GADI_U8         planeId;
    /*the index of area(0~2), each plane has 3 areas.*/
    GADI_U8         areaId;
}GADI_OSD_AreaIndexT;

/*!
*******************************************************************************
** \brief the mapping address informations struct of osd plane's area.
*******************************************************************************
*/
typedef struct
{
    /*the index of area.*/
    GADI_U8         areaId;
    /*yuv colour look-up table start address.*/
    GADI_U8         *clutStartAddr;
    /*number of bytes of colour look-up table.*/
    GADI_U32        clutSize;
    /*plane area start address.*/
    GADI_U8         *areaStartAddr;
    /*number of bytes of area.*/
    GADI_U32        areaSize;
}GADI_OSD_AreaMappingT;

/*!
*******************************************************************************
** \brief the parameters struct of osd plane's area.
*******************************************************************************
*/
typedef struct
{
    /*the index of plane(0~3).*/
    GADI_U8         planeId;
    /*the index of area(0~2).*/
    GADI_U8         areaId;
    /*the enable/disable flag of area(0:disable, 1:enable).*/
    GADI_U8         enable;
    /*area width.*/
    GADI_U16        width;
    /*area height.*/
    GADI_U16        height;
    /*area x offset.*/
    GADI_U16        offsetX;
    /*area y offset.*/
    GADI_U16        offsetY;
}GADI_OSD_AreaParamsT;

typedef enum GADI_OSD_ColorFormatEnum_s {
    /* RGB color space used color format. */
    GADI_OSD_COLORFMT_RGB24 = 0,    /* GADI_OSD_BitMapRGB24T */
    GADI_OSD_COLORFMT_RGB888 = 0,   /* GADI_OSD_BitMapRGB24T */
    GADI_OSD_COLORFMT_RGB565,       /* GADI_OSD_BitMapRGB565T */
    GADI_OSD_COLORFMT_RGB555,       /* GADI_OSD_BitMapRGB555T */
    GADI_OSD_COLORFMT_RGB32,        /* GADI_OSD_BitMapRGB32T */
} GADI_OSD_ColorFormatEnumT;

typedef struct {
    GADI_U8 b;
    GADI_U8 g;
    GADI_U8 r;
}__attribute__ ((packed)) GADI_OSD_BitMapRGB24T;

typedef struct {
    GADI_U8 b;
    GADI_U8 g;
    GADI_U8 r;
    GADI_U8 alpha;
}__attribute__ ((packed)) GADI_OSD_BitMapRGB32T;

typedef struct {
    GADI_U16 b:5;
    GADI_U16 g:6;
    GADI_U16 r:5;
}__attribute__ ((packed)) GADI_OSD_BitMapRGB565T;

typedef struct {
    GADI_U16 b:5;
    GADI_U16 g:5;
    GADI_U16 r:5;
}__attribute__ ((packed)) GADI_OSD_BitMapRGB555T;


typedef struct GADI_OSD_BitMapAttr_s {
    /* color format */
    GADI_OSD_ColorFormatEnumT   bitMapColorFmt;
    /* picture width */
    GADI_U16                    width;
    /* picture height */
    GADI_U16                    height;
    /* picture pitch, unit of byte */
    GADI_U16                    pitch;
    /* setup transparency of picture, display on OSD */
    GADI_U8                     alpha;
    /*  picture data space */
    GADI_VOID                   *bitMapAddr;
} GADI_OSD_BitMapAttrT;

typedef enum GADI_OSD_InvertColorModeEnum_s {
    /* when less then threshold value, trigger invert OSD */
    GADI_OSD_LT_LUM_THRESH = 0,
    /* when greather then ... */
    GADI_OSD_GT_LUM_THRESH = 1,
} GADI_OSD_InvertColorModeEnumT;

typedef struct GADI_OSD_ChannelAttr_s {
    /* setup unit of invert OSD, need align 16,
            this limits value is 16 to 64 */
    GADI_U32                        invtAreaWidth;
    GADI_U32                        invtAreaHeight;
    /* setup threshold value of invert osd */
    GADI_U32                        lumThreshold;
    /* setup invert osd mode */
    GADI_OSD_InvertColorModeEnumT   invtColorMode;
    /* enable invert osd  */
    GADI_BOOL                       invtColorEn;
} GADI_OSD_ChannelAttrT;

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
** \brief Initialize the ADI osd module.
**
** \return
** - #GADI_OK
** - #GADI_OSD_ERR_OUT_OF_MEMORY
** - #GADI_OSD_ERR_ALREADY_INITIALIZED
**
** \sa gadi_osd_exit
**
*******************************************************************************
*/
GADI_ERR gadi_osd_init(void);

/*!
*******************************************************************************
** \brief Shutdown the ADI osd module.
**
** \return
** - #GADI_OK
** - #GADI_OSD_ERR_NOT_INITIALIZED
**
** \sa gadi_osd_init
**
*******************************************************************************
*/
GADI_ERR gadi_osd_exit(void);

/*!
*******************************************************************************
** \brief open the ADI osd module.
**
** \param[in] errorCodePtr pointer to return the error code.
**
** \return Return an valid handle of ADI osd module instance.
**
** \sa gadi_osd_close
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_osd_open(GADI_ERR* errorCodePtr);

/*!
*******************************************************************************
** \brief close one ADI osd module instance.
**
** \param[in]  handle     Valid ADI osd instance handle previously opened by
**                        #gadi_osd_open.
**
** \return
** - #GADI_OK
** - #GADI_OSD_ERR_BAD_PARAMETER
**
** \sa gadi_osd_open
**
*******************************************************************************
*/
GADI_ERR gadi_osd_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief get area mapping address information of osd plane.
**
** \param[in]  handle     Valid ADI osd instance handle previously opened by
**                        #gadi_osd_open.
** \param[in]  areaIndex  struct of area index, include plane id & area id.
** \param[out] areaMapping  pointer of area mapping struct.
**
** \return
** - #GADI_OK
** - #GADI_OSD_ERR_BAD_PARAMETER
**
** \sa gadi_osd_open
**
*******************************************************************************
*/
GADI_ERR gadi_osd_get_area_mapping(GADI_SYS_HandleT handle,
                                            GADI_OSD_AreaIndexT areaIndex,
                                            GADI_OSD_AreaMappingT *areaMapping);

/*!
*******************************************************************************
** \brief set area parameters of osd plane.
**
** \param[in]  handle     Valid ADI osd instance handle previously opened by
**                        #gadi_osd_open.
** \param[in] areaParams  pointer of area parameters struct.
**
** \return
** - #GADI_OK
** - #GADI_OSD_ERR_FROM_DRIVER
** - #GADI_OSD_ERR_BAD_PARAMETER
**
** \sa gadi_osd_get_area_params
**
*******************************************************************************
*/
GADI_ERR gadi_osd_set_area_params(GADI_SYS_HandleT
                                      handle, GADI_OSD_AreaParamsT *areaParams);

/*!
*******************************************************************************
** \brief set area parameters of osd plane.
**
** \param[in]  handle     Valid ADI osd instance handle previously opened by
**                        #gadi_osd_open.
** \param[in/out] areaParams  pointer of area parameters struct, must give out
**                            plane id & area id.
**
** \return
** - #GADI_OK
** - #GADI_OSD_ERR_FROM_DRIVER
** - #GADI_OSD_ERR_BAD_PARAMETER
**
** \sa gadi_osd_set_area_params
**
*******************************************************************************
*/
GADI_ERR gadi_osd_get_area_params(GADI_SYS_HandleT handle,
                                               GADI_OSD_AreaParamsT *areaParams);


/*!
*******************************************************************************
** \brief set area parameters of osd plane.
**
** \param[in]  handle     Valid ADI osd instance handle previously opened by
**                        #gadi_osd_open.
** \param[in/out] areaParams  pointer of area parameters struct, must give out
**                            plane id & area id.
**
** \return
** - #GADI_OK
** - #GADI_OSD_ERR_FROM_DRIVER
** - #GADI_OSD_ERR_BAD_PARAMETER
**
** \sa gadi_osd_load_bitmap
**
*******************************************************************************
*/
GADI_ERR gadi_osd_load_bitmap(GADI_SYS_HandleT handle,
                                GADI_OSD_AreaParamsT *areaParams,
                                GADI_OSD_BitMapAttrT *bitMapAttr);

GADI_ERR gadi_osd_set_channel_attr(GADI_SYS_HandleT handle,
                                GADI_OSD_AreaIndexT *areaIndex,
                                GADI_OSD_ChannelAttrT *chnAttr);

GADI_ERR gadi_osd_get_channel_attr(GADI_SYS_HandleT handle,
                                GADI_OSD_AreaIndexT *areaIndex,
                                GADI_OSD_ChannelAttrT *chnAttr);

#ifdef __cplusplus
    }
#endif


#endif /* _ADI_OSD_H_ */
