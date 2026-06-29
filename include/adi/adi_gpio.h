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

#ifndef _ADI_GPIO_H_
#define _ADI_GPIO_H_


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
#define GADI_GPIO_ERR_BAD_PARAMETER                                          \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_GPIO_ERR_OUT_OF_MEMORY                                          \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_GPIO_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_GPIO_ERR_NOT_INITIALIZED                                        \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_GPIO_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_GPIO_ERR_TIMEOUT                                                \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_GPIO_ERR_DEVICE_BUSY                                            \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_GPIO_ERR_INVALID_HANDLE                                         \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_GPIO_ERR_SEMAPHORE_CREATE                                       \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_GPIO_ERR_UNSUPPORTED_VERSION                                    \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_GPIO_ERR_FROM_DRIVER                                            \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_GPIO_ERR_NOT_OPEN                                               \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_NOT_OPEN)
/*! The file is written failed. */
#define GADI_GPIO_ERR_WRITE_FAILED                                           \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_WRITE_FAILED)
/*! The file is read failed. */
#define GADI_GPIO_ERR_READ_FAILED                                            \
                              (GADI_GPIO_MODULE_BASE + GADI_ERR_READ_FAILED)


/*
*******************************************************************************
** \brief ADC channel type.
*******************************************************************************
*/
typedef enum
{
    GADI_GPIO_ADC_CHANNEL_ONE,
    GADI_GPIO_ADC_CHANNEL_TWO,
}GADI_GPIO_ADC_ChannelT;

/*
*******************************************************************************
** \brief Configuration parameters for GPIO.
*******************************************************************************
*/

typedef struct
{
    /* channel number */
    GADI_GPIO_ADC_ChannelT channel;
    /* adc number */
    GADI_U32 value;

}GADI_GPIO_AdcValue;


/*
*******************************************************************************
** \brief adc parameter.
*******************************************************************************
*/

typedef struct
{

    GADI_U8 num_gpio;
    /* level mode, 1 for negative 0 for positive */
    GADI_U8 active_low;
    /* output/input, 1/0 */
    GADI_U8 direction;
    /* high/low level, 1/0 in positive mode, 0/1 in negative mode */
    GADI_U8 value;
}GADI_GPIO_OpenParam;

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
** \brief Open one GPIO ADI instance.
**
** \param[in] ErrorCode: A pointer to return the error code.
**
** \param[in] UserConfig: Contains GPIO parameters.
**
** \return
**         - #(GADI_SYS_HandleT)gpioHandle                  An valid handle of gpio ADI instance
**                                                                            when function calls success.
** \sa
**         - gadi_gpio_close
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_gpio_open(GADI_ERR *errorCodePtr,
                                        GADI_GPIO_OpenParam *userConfig);




/*!
*******************************************************************************
** \brief Close GPIO ADI instance.
**
** \param[in] handle: Valid GPIO ADI instance handle previously opened by #gadi_gpio_open.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #NULL                                                         Error occurred.
**         - #GADI_GPIO_ERR_BAD_PARAMETER                Invalid parameter.
**         - #GADI_GPIO_ERR_FEATURE_NOT_SUPPORTED  File of "export" doesn't exist.
**         - #GADI_GPIO_ERR_OUT_OF_MEMORY               Get memory failed!
**         - #GADI_GPIO_ERR_FROM_DRIVER                    Can't get file descriptor of "unexport"
**                                                                              or file doesn`t exist!by #open.
**         - #GADI_GPIO_ERR_WRITE_FAILED                   File written failed.
**
** \sa
**         - gadi_gpio_open
**
*******************************************************************************
*/
GADI_ERR gadi_gpio_close(GADI_SYS_HandleT handle);



/*!
*******************************************************************************
** \brief set output value to "1".
**
** \param[in] handle: Valid GPIO ADI instance handle previously opened by #gadi_gpio_open.
**
** return
**        - #GADI_OK                                                    On success.
**        - #GADI_GPIO_ERR_BAD_PARAMETER                 Invalid parameter.
**        - #GADI_GPIO_ERR_FEATURE_NOT_SUPPORTED   Direction is output mode.
**        - #GADI_GPIO_ERR_FROM_DRIVER                     Can't get file descriptor of "value"
**                                                                              by #open.
**        - #GADI_GPIO_ERR_WRITE_FAILED                    File written failed.
**
** \sa
**        - gadi_gpio_clear
**
*******************************************************************************
*/
GADI_ERR gadi_gpio_set(GADI_SYS_HandleT handle);




/*!
*******************************************************************************
** \brief set output value to "0".
**
** \param[in] handle: Valid GPIO ADI instance handle previously opened by #gadi_gpio_open.
**
** \return
**        - #GADI_OK                                                    On success.
**        - #GADI_GPIO_ERR_BAD_PARAMETER                 Invalid parameter.
**        - #GADI_GPIO_ERR_FEATURE_NOT_SUPPORTED   Direction is output mode.
**        - #GADI_GPIO_ERR_FROM_DRIVER                     Can't get file descriptor of "value" by
**                                                                              #open.
**        - #GADI_GPIO_ERR_WRITE_FAILED                    File written failed.
**
** \sa
**        - gadi_gpio_set
**
*******************************************************************************
*/
GADI_ERR gadi_gpio_clear(GADI_SYS_HandleT handle);




/*!
*******************************************************************************

** \brief Get value of high-low level.
**
** \parame[in] handle: Valid GPIO ADI instance handle previously opened by #gadi_gpio_open.
**
** \parame[in] value:  A pointer to return the 1/0 that represent high-low level.
**
** \return
**         - #GADI_OK                                                   On success.
**         - #GADI_GPIO_ERR_BAD_PARAMETER                Invalid parameter.
**         - #GADI_GPIO_ERR_FEATURE_NOT_SUPPORTED  Direction is input mode.
**         - #GADI_GPIO_ERR_FROM_DRIVER                    Can't get file descriptor of "value" by
**                                                                              #open.
**         - #GADI_GPIO_ERR_READ_FAILED                     File read failed.
**
** \sa
**         - gadi_gpio_open
**
*******************************************************************************
*/
GADI_ERR gadi_gpio_read_value(GADI_SYS_HandleT handle, GADI_S32 *value);


/*!
*******************************************************************************

** \brief Get value of adc channel.
**
** \parame[out] adcValue: adc channel and channel's value
**

**
** \return
**         - #GADI_OK                                     On success.
**         - #GADI_GPIO_ERR_BAD_PARAMETER                 Invalid parameter.
**         - #GADI_GPIO_ERR_FROM_DRIVER                   Read error
**
**
** \sa
**
*******************************************************************************
*/
GADI_ERR gadi_gpio_read_adc(GADI_GPIO_AdcValue *adcValue);

#ifdef __cplusplus
    }
#endif



#endif /* _ADI_GPIO_H_ */
