/*!
*****************************************************************************
** \file           ./adi/inc/adi_spi.h
**
** \brief          adi spi module porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**                  ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**                  OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_SPI_H_
#define _ADI_SPI_H_



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
#define GADI_SPI_ERR_BAD_PARAMETER                                          \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_SPI_ERR_OUT_OF_MEMORY                                          \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_SPI_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_SPI_ERR_NOT_INITIALIZED                                        \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_SPI_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_SPI_ERR_TIMEOUT                                                \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_SPI_ERR_DEVICE_BUSY                                            \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_SPI_ERR_INVALID_HANDLE                                         \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_SPI_ERR_SEMAPHORE_CREATE                                       \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_SPI_ERR_UNSUPPORTED_VERSION                                    \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_SPI_ERR_FROM_DRIVER                                            \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.*/
#define GADI_SPI_ERR_NOT_OPEN                                               \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_NOT_OPEN)
/*! The file is written failed. */
#define GADI_SPI_ERR_WRITE_FAILED                                           \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_WRITE_FAILED)
/*! The file is read failed. */
#define GADI_SPI_ERR_READ_FAILED                                            \
                              (GADI_SPI_MODULE_BASE + GADI_ERR_READ_FAILED)


/*
*******************************************************************************
** \brief Configuration parameters for SPI.
*******************************************************************************
*/
typedef struct
{
    /*Device node path*/
    GADI_CHAR *nodePath;
    /* Mode of SPI */
    GADI_U8    mode;
    /* Temporary override of the device's wordsize. */
    GADI_U8    bits;
    /* temporary override of the device's bitrate. */
    GADI_U32   speed;
    /*if lsb = 0, SPI transmit high bit first, or transmit low bit first.*/
    GADI_U8    lsb;
}GADI_SPI_OpenParam;


/*
*******************************************************************************
** \brief Configuration parameters for gadi_spi_general_ioctl();.
*******************************************************************************
*/
typedef struct
{
    /*A pointer to data user want to send*/
    void *txBuf;
    /*A pointer to buffer where user want to save data*/
    void *rxBuf;
    /*Buffer size*/
    GADI_U32 bufLen;
}GADI_SPI_OpenParamTransfer;

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
** \brief Open one SPI ADI instance.
**
** \param[in] ErrorCode: A pointer to return the error code.
**
** \param[in] UserConfig: Contains SPI configuration parameters.
**
** \return
**         - #(GADI_SYS_HandleT)spiHandle                  An valid handle of spi ADI instance
**                                                                          when function calls success.
**         - #NULL                                                      Error occurred.
** \sa
**         - gadi_spi_close
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_spi_open(GADI_ERR *errorCodePtr,
                                        GADI_SPI_OpenParam *userConfig);

/*!
*******************************************************************************
** \brief Close SPI ADI instance.
**
** \param[in] handle: Valid SPI ADI instance handle previously opened by #gadi_spi_open.
**
** \return
**         - #GADI_OK                                                 On success.
**         - #GADI_SPI_ERR_BAD_PARAMETER                Invalid parameter.
**
** \sa
**         - gadi_spi_open
**
*******************************************************************************
*/
GADI_ERR gadi_spi_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief Only send data by SPI, in half-duplex mode.
**
** \parame[in] handle: Valid SPI ADI instance handle previously opened by #gadi_spi_open.
**
** \parame[in] data:  A pointer to data user want to send.
**
** \parame[in] dataLen: Length of data.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_SPI_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_SPI_ERR_READ_FAILED                       Data received failed.
**
** \sa
**         - gadi_spi_write_only
**
*******************************************************************************
*/
GADI_ERR gadi_spi_read_only(GADI_SYS_HandleT handle, void *data,
                                    GADI_U32 dataLen);



/*!
*******************************************************************************
** \brief Only receive data by SPI, in half-duplex mode.
**
** \parame[in] handle: Valid SPI ADI instance handle previously opened by #gadi_spi_open.
**
** \parame[in] data:  A pointer to buffer user want to receive.
**
** \parame[in] dataLen: Length of buffer.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_SPI_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_SPI_ERR_WRITE_FAILED                     Data sent failed.
**
** \sa
**         - gadi_spi_read_only
**
*******************************************************************************
*/
GADI_ERR gadi_spi_write_only(GADI_SYS_HandleT handle, void *data,
                                    GADI_U32 dataLen);



/*!
*******************************************************************************
** \brief Send and receive data simultaneously, in full-duplex mode.
**
** \parame[in] handle: Valid SPI ADI instance handle previously opened by #gadi_spi_open.
**
** \parame[in] txData:  A pointer to data user want to send.
**
** \parame[in] rxData:  A pointer to buffer user want to receive.
**
** \parame[in] dataLen:  Length of buffer.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_SPI_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_SPI_ERR_FROM_DRIVER                      Can't send spi message.
**
** \sa
**         - gadi_spi_open
**
*******************************************************************************
*/
GADI_ERR gadi_spi_write_and_read(GADI_SYS_HandleT handle, void *txData,
    void *rxData, GADI_U32 dataLen);


/*!
*******************************************************************************
** \brief Send data and then receive data, in half-duplex mode.
**
** \parame[in] handle: Valid SPI ADI instance handle previously opened by #gadi_spi_open.
**
** \parame[in] txData:  A pointer to data user want to send.
**
** \parame[in] txdataLen:  Length of txdata.
**
** \parame[in] rxData:  A pointer to buffer user want to receive.
**
** \parame[in] rxdataLen:  Length of buffer.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_SPI_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_SPI_ERR_FROM_DRIVER                      Can't send spi message.
**
** \sa
**         - gadi_spi_open
**
*******************************************************************************
*/
GADI_ERR gadi_spi_write_then_read(GADI_SYS_HandleT handle, void *txData,
    GADI_U32 txDataLen, void *rxData, GADI_U32 rxDataLen);

/*!
*******************************************************************************
** \brief General SPI operation ADI instance,
**          half-duplex or full-duplex mode can be set by GADI_SPI_OpenParamTransfer .
**
** \parame[in] handle: Valid SPI ADI instance handle previously opened by #gadi_spi_open.
**
** \parame[in] p:  A pointer to data structures user want to set.
**
** \parame[in] pNum:  Numbers of data structures.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_SPI_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_SPI_ERR_FROM_DRIVER                      Can't send spi message.
**
** \sa
**         - gadi_spi_open
**
*******************************************************************************
*/
GADI_ERR gadi_spi_general_ioctl(GADI_SYS_HandleT handle,
    GADI_SPI_OpenParamTransfer *p, GADI_U32 pNum);




#ifdef __cplusplus
    }
#endif



#endif /* _ADI_SPI_H_ */
