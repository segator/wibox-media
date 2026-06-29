/*!
*****************************************************************************
** \file           ./adi/inc/adi_i2c.h
**
** \brief          adi i2c module porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**                  ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**                  OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_I2C_H_
#define _ADI_I2C_H_



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
#define GADI_I2C_ERR_BAD_PARAMETER                                          \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_I2C_ERR_OUT_OF_MEMORY                                          \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_I2C_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_I2C_ERR_NOT_INITIALIZED                                        \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_I2C_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_I2C_ERR_TIMEOUT                                                \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_I2C_ERR_DEVICE_BUSY                                            \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_I2C_ERR_INVALID_HANDLE                                         \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_I2C_ERR_SEMAPHORE_CREATE                                       \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_I2C_ERR_UNSUPPORTED_VERSION                                    \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_I2C_ERR_FROM_DRIVER                                            \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_I2C_ERR_NOT_OPEN                                               \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_NOT_OPEN)
/*! The file is written failed. */
#define GADI_I2C_ERR_WRITE_FAILED                                           \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_WRITE_FAILED)
/*! The file is read failed. */
#define GADI_I2C_ERR_READ_FAILED                                            \
                              (GADI_I2C_MODULE_BASE + GADI_ERR_READ_FAILED)


/*
*******************************************************************************
** \brief Configuration parameters for I2C.
*******************************************************************************
*/

typedef struct
{
    /* Device node number. */
    GADI_CHAR *nodePath;
    /*Address of slave.*/
    GADI_U8  slaveAddr;
}GADI_I2C_OpenParam;

/*
*******************************************************************************
** \brief Configuration parameters for messages.
*******************************************************************************
*/
typedef struct
{
    GADI_U8 addr;     /* slave address */
    GADI_U16 flags;    /* read/write flag */
    GADI_U16 len;      /* msg length */
    GADI_U8 *buf;      /* pointer to msg data */
}GADI_I2C_MsgT;

/*
*******************************************************************************
** \brief Configuration parameters for gadi_i2c_general_ioctl();.
*******************************************************************************
*/
typedef struct
{
    GADI_I2C_MsgT *pMsgs;
    GADI_U8  nMsgs;  /* numbers of i2c_msg structure*/
}GADI_I2C_OpenParamT;

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
** \brief Open one I2C ADI instance.
**
** \param[in] ErrorCode:  A Pointer to return the error code.
**
** \param[in] UserConfig: Contains I2C configuration parameters.
**
** \return
**         - #(GADI_SYS_HandleT)i2cHandle                  An valid handle of i2c ADI instance
**                                                                          when function calls success.
** \sa
**         - gadi_i2c_close
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_i2c_open(GADI_ERR *errorCodePtr, GADI_I2C_OpenParam *userConfig);

/*!
*******************************************************************************
** \brief Close I2C ADI instance.
**
** \param[in] handle: Valid I2C ADI instance handle previously opened by #gadi_i2c_open.
**
** \return
**         - #GADI_OK                                                 On success.
**         - #GADI_I2C_ERR_BAD_PARAMETER                Invalid parameter.
**
** \sa
**         - gadi_i2c_open
**
*******************************************************************************
*/
GADI_ERR gadi_i2c_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************

** \brief Send data by I2C.
**
** \parame[in] handle: Valid I2C ADI instance handle previously opened by #gadi_i2c_open.
**
** \parame[in] data:  A pointer to buffer user want to send.
**
** \parame[in] dataLen:  Length of data.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_I2C_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_I2C_ERR_WRITE_FAILED                     Data written failed.
**
** \sa
**         - gadi_i2c_read
**
*******************************************************************************
*/
GADI_ERR gadi_i2c_write(GADI_SYS_HandleT handle, void *data, GADI_U32 dataLen);

/*!
*******************************************************************************

** \brief Receive data by I2C.
**
** \parame[in] handle: Valid I2C ADI instance handle previously opened by #gadi_i2c_open.
**
** \parame[in] data:  A pointer to buffer user want to send.
**
** \parame[in] dataLen:  Length of buffer.

**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_I2C_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_I2C_ERR_READ_FAILED                       Data received failed.
**
** \sa
**         - gadi_i2c_write
*******************************************************************************
*/
GADI_ERR gadi_i2c_read(GADI_SYS_HandleT handle, void *data, GADI_U32 dataLen);

/*!
*******************************************************************************

** \brief Some devices need a repeated start, send data and then receive data.
**
** \parame[in] handle: Valid I2C ADI instance handle previously opened by #gadi_i2c_open.
**
** \parame[in] addr:  Slave address.
**
** \parame[in] txData:  A pointer to data user want to send.
**
** \parame[in] txdataLen: Length of txdata.
**
** \parame[in] rxData:  A pointer to buffer user want to receive.
**
** \parame[in] rxdataLen: Length of buffer.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_I2C_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_I2C_ERR_OUT_MEMORY                      Get memory failed.
**         - #GADI_I2C_ERR_FROM_DRIVER                      Can't set by I2C driver.
**
** \sa
**         - gadi_i2c_open
*******************************************************************************
*/
GADI_ERR gadi_i2c_read_ioctl(GADI_SYS_HandleT handle, GADI_U8 addr,
    void *txData, GADI_U16 txDataLen, void *rxData, GADI_U16 rxDataLen);

/*!
*******************************************************************************
** \brief Send data by I2C.
**
** \parame[in] handle: Valid I2C ADI instance handle previously opened by #gadi_i2c_open.
**
** \parame[in] addr:  Slave address.
**
** \parame[in] data:  A pointer to buffer user want to send.
**
** \parame[in] dataLen:  Length of buffer.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_I2C_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_I2C_ERR_OUT_MEMORY                      Get memory failed.
**         - #GADI_I2C_ERR_FROM_DRIVER                      Can't set by I2C driver.
**
** \sa
**         - gadi_i2c_open
**
*******************************************************************************
*/
GADI_ERR gadi_i2c_write_ioctl(GADI_SYS_HandleT handle, GADI_U16 addr,
                                                void *data, GADI_U16 dataLen);

/*!
*******************************************************************************
** \brief General I2C operation ADI instance,
**
** \parame[in] handle: Valid I2C ADI instance handle previously opened by #gadi_i2c_open.
**
** \parame[in] p:  A pointer to data structures user want to set.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_I2C_ERR_BAD_PARAMETER                  Invalid parameter.
**         - #GADI_I2C_ERR_OUT_MEMORY                      Get memory failed.
**         - #GADI_I2C_ERR_FROM_DRIVER                      Can't set by I2C driver.
**
** \sa
**         - gadi_i2c_open
**
*******************************************************************************
*/
GADI_ERR gadi_i2c_general_ioctl(GADI_SYS_HandleT handle, GADI_I2C_OpenParamT *p);


#ifdef __cplusplus
    }
#endif



#endif /* _ADI_I2C_H_ */

