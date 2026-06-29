/*!
*****************************************************************************
** \file        /include/adi_pda.h
**
** \brief       Picture data analysis by luma buffer
**              It analysis luma variation and return SAD value of each micro block.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2012-2015 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/
#ifndef ADI_PDA_H
#define ADI_PDA_H

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
#define GADI_PDA_ERR_BAD_PARAMETER                                          \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_PDA_ERR_OUT_OF_MEMORY                                          \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_PDA_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_PDA_ERR_NOT_INITIALIZED                                        \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_PDA_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_PDA_ERR_TIMEOUT                                                \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_PDA_ERR_DEVICE_BUSY                                            \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_PDA_ERR_INVALID_HANDLE                                         \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_PDA_ERR_SEMAPHORE_CREATE                                       \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_PDA_ERR_UNSUPPORTED_VERSION                                    \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_PDA_ERR_FROM_DRIVER                                            \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_PDA_ERR_NOT_OPEN                                               \
                              (GADI_PDA_MODULE_BASE + GADI_ERR_NOT_OPEN)
/*! The file is written failed. */
#define GADI_PDA_ERR_WRITE_FAILED                                           \
                                  (GADI_PDA_MODULE_BASE + GADI_ERR_WRITE_FAILED)
/*! The file is read failed. */
#define GADI_PDA_ERR_READ_FAILED                                            \
                                  (GADI_PDA_MODULE_BASE + GADI_ERR_READ_FAILED)
/*! The file is read failed. */
#define GADI_PDA_ERR_MAP                                            \
                                  (GADI_PDA_MODULE_BASE + GADI_ERR_MAP)


/*! Select monitor buffer  */
typedef enum {
    GADI_PDA_MD_FIRST_BUFFER = 0,   /*use first preview buffer*/
    GADI_PDA_MD_SECOND_BUFFER,      /*use second preview buffer*/
    GADI_PDA_MD_NUM_BUFFER
} GADI_PDA_MD_Buffer_TypeT;

/*! Select monitor buffer  */
typedef enum {
    GADI_PDA_CAP_MAIN_BUFFER = 0,   /*use main buffer*/
    GADI_PDA_CAP_SECOND_BUFFER,     /*use first preview buffer*/
    GADI_PDA_CAP_FOURTH_BUFFER,     /*use second preview buffer*/
    GADI_PDA_CAP_NUM_BUFFER
} GADI_PDA_CAP_Buffer_TypeT;

/*MD algorithm*/
typedef enum
{
    GADI_PDA_MD_ALG_REF,    /*base on reference pictrue*/
    GADI_PDA_MD_ALG_NUM,
}GADI_PDA_MD_Alg_t;

/*Micro Block size*/
typedef enum
{
    GADI_PDA_MD_MB_8PIXEL,      /* 8*8 */
    GADI_PDA_MD_MB_16PIXEL,     /* 16*16 */
    GADI_PDA_MD_MB_NUM,
}GADI_PDA_MD_MB_SizeT;

/*SAD(Sum of Absolute Difference) bits,
the macroblock SAD is the sum of the absolute luminance differences between the
macroblocks of two frames. The greater the macroblock SAD, the greater the luminance
difference*/
typedef enum
{
    GADI_PDA_MD_MB_SAD_8BIT = 0,  /*SAD precision 8bits,it return the high 8 bits of result*/
    GADI_PDA_MD_MB_SAD_16BIT,     /*SAD precision 16bits*/
    GADI_PDA_MD_MB_SAD_NUM,
} GADI_PDA_MD_MB_SAD_BitT;

/*bufffer information */
typedef struct
{
    GADI_U32 width;     /* preview buffer width*/
    GADI_U32 height;    /* preview buffer height*/
    GADI_U32 stride;     /* one line's width of preview buffer */
}GADI_PDA_Buffer_InfoT;

/*MD attribute*/
typedef struct
{
    /*static attribute, it isn't modified after call gadi_pda_set_attr*/
    GADI_PDA_MD_Alg_t           mdAlg;  /*arithmetic*/
    GADI_PDA_MD_MB_SizeT        mbSize;      /*MB size*/
    GADI_PDA_MD_MB_SAD_BitT     mbSadBits;   /*MB bits size*/
    GADI_U32                    mdBufNum;   /*Result buffer number,range:[1,8]*/
       /*dynamic attribute*/
    GADI_U32                    mdFrameIntvel;   /*md framr interval,range:[0,256]*/

}GADI_PDA_MD_AttrT;


/*MD attribute*/
typedef union
{
    GADI_PDA_MD_AttrT mdAttr;

}GADI_PDA_AttrU;

/*MB SAD data*/
typedef struct
{
    void            *pAddr;             /*address*/
    GADI_U32        stride;             /*stride*/
    GADI_PDA_MD_MB_SAD_BitT mbSadBits;  /*the MB SAD size*/
}GADI_PDA_MB_SAD_DataT;

/*MD data*/
typedef union
{
    GADI_PDA_MB_SAD_DataT stMdData; /*MD data*/

}GADI_PDA_DataU;

/*PDA data*/
typedef struct
{
    GADI_PDA_DataU      unData;      /*PDA data*/
    GADI_PDA_MD_MB_SizeT       enMbSize;    /*MB size*/
    GADI_U32            u32MbWidth;  /*PDA channle width in MB*/
    GADI_U32            u32MbHeight; /*PDA channle height in MB*/
}GADI_PDA_DataT;

/*format of caputre frame*/
typedef enum
{
    FRAME_FORMAT_YUV_SEMIPLANAR_420,/*Y is planer,U V is interleaved*/
    FRAME_FORMAT_YUV_SEMIPLANAR_422,/*Y is planer,U V is interleaved*/
}GADI_PDA_Frame_Format;


/*information of caputre frame*/
typedef struct
{
    GADI_PDA_Frame_Format frameFormat; /*format of caputre frame*/
    GADI_U32    width;                 /*width of caputre frame*/
    GADI_U32    height;                /*height of caputre frame*/
    GADI_U8     *yAddr;                /*Y address of caputre frame*/
    GADI_U8     *uvAddr;               /*UV address of caputre frame, the UV data is interleaved*/
    GADI_U32    stride;                /*stride of caputre frame*/
}GADI_PDA_Frame_InfoT;


#ifdef __cplusplus
extern "C" {
#endif

/*!
*******************************************************************************
** \brief Initialize the PDA module
**
** \param[in] type  motion buffer type.the encode must be running.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa gadi_pda_exit
**
*******************************************************************************
*/
GADI_ERR gadi_pda_init(GADI_PDA_MD_Buffer_TypeT type);

/*!
*******************************************************************************
** \brief get preivew buffer size
**
** \param[out] info  motion buffer information.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_buffer_info(GADI_PDA_Buffer_InfoT *info);


/*!
*******************************************************************************
** \brief capture motion buffer picture
**
** \param[out] frameInfo  frame information.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_capture_buffer(GADI_PDA_CAP_Buffer_TypeT buffer, GADI_PDA_Frame_InfoT *frameInfo);


/*!
*******************************************************************************
** \brief open a pda handle
**
** \param[in] pstAttr  motion attribute.
**
** \return
** - #NULL
** - #GADI_SYS_HandleT
**
** \sa
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_pda_open(GADI_PDA_AttrU *pstAttr);

/*!
*******************************************************************************
** \brief close pda handle
**
** \param[in] handle  pda handle.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief get pda attribute
**
** \param[in]  handle   pda handle.
** \param[out] pstAttr  attribute handle
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_get_attr(GADI_SYS_HandleT handle, GADI_PDA_AttrU *pstAttr);

/*!
*******************************************************************************
** \brief set pda attribute
**
** \param[in]  handle   pda handle.
** \param[in] pstAttr  attribute handle
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_set_attr(GADI_SYS_HandleT handle, GADI_PDA_AttrU *pstAttr);

/*!
*******************************************************************************
** \brief start picture analysis
**
** \param[in]  handle   pda handle.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_start_recv_pic(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief stop picture analysis
**
** \param[in]  handle   pda handle.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_stop_recv_pic(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief get sad information of all Micro block
**
** \param[in]   handle          pda handle.
** \param[out]  pstPdaData      pda SAD informtion.
** \param[in]   bBlock          block or not.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_get_data(GADI_SYS_HandleT handle, GADI_PDA_DataT *pstPdaData, GADI_BOOL bBlock);

/*!
*******************************************************************************
** \brief relese sad information
**
** \param[in]   handle          pda handle.
** \param[in]  pstPdaData      pda SAD informtion.
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_release_data(GADI_SYS_HandleT handle, const GADI_PDA_DataT* pstPdaData);


/*!
*******************************************************************************
** \brief exit pda module
**
** \return
** - #GADI_OK
** - #GADI_PM_ERR_OUT_OF_MEMORY
** - #GADI_PM_ERR_ALREADY_INITIALIZED
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_pda_exit(void);

#ifdef __cplusplus
}
#endif


#endif
