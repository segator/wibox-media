/*!
*****************************************************************************
** \file        ./adi/inc/adi_pwm.h
**
** \brief       adi pwm module(include DC iris) porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_PWM_H_
#define _ADI_PWM_H_

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
#define GADI_PWM_ERR_BAD_PARAMETER                                          \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_PWM_ERR_OUT_OF_MEMORY                                          \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_PWM_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_PWM_ERR_NOT_INITIALIZED                                        \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_PWM_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_PWM_ERR_TIMEOUT                                                \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_PWM_ERR_DEVICE_BUSY                                            \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_PWM_ERR_INVALID_HANDLE                                         \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_PWM_ERR_SEMAPHORE_CREATE                                       \
                              (GADI_PWM_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_PWM_ERR_UNSUPPORTED_VERSION                                    \
                               (GADI_PWM_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_PWM_ERR_FROM_DRIVER                                            \
                               (GADI_PWM_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_PWM_ERR_NOT_OPEN                                               \
                               (GADI_PWM_MODULE_BASE + GADI_ERR_NOT_OPEN)


//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief prm iris control state enum.
*******************************************************************************
*/
typedef enum{
    GADI_PWM_IRIS_IDLE = 0,
    GADI_PWM_IRIS_FLUCTUATE,
    GADI_PWM_IRIS_STATE_NUM,
} GADI_PWM_IRIS_STATE;


//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************

typedef struct
{
    GADI_S32    pCoef;
    GADI_S32    iCoef;
    GADI_S32    dCoef;
}GADI_PWM_IrisPidCoef;


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
** \brief init pwm device.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_ALREADY_INITIALIZED
** - #GADI_PWM_ERR_NOT_OPEN
**
** \sa gadi_pwm_init
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_init(void);

/*!
*******************************************************************************
** \brief exit pwm device.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_NOT_INITIALIZED
**
** \sa gadi_pwm_exit
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_exit(void);

/*!
*******************************************************************************
** \brief open one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  index     Valid pwm channel params [0~3].
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
** - #GADI_PWM_ERR_NOT_INITIALIZED
** - #GADI_PWM_ERR_OUT_OF_MEMORY
**
** \sa gadi_pwm_open
**
*******************************************************************************
*/
GADI_SYS_HandleT gadi_pwm_open(GADI_ERR *ErrorCode, GADI_U8 channel);

/*!
*******************************************************************************
** \brief close one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
**
** \sa gadi_pwm_close
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief start one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
** - #GADI_PWM_ERR_FROM_DRIVER
**
** \sa gadi_pwm_start
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_start(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief stop one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
** - #GADI_PWM_ERR_FROM_DRIVER
**
** \sa gadi_pwm_stop
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_stop(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief get speed of one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  *speed     Valid ADI pwm speed(Hz).
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
** - #GADI_PWM_ERR_FROM_DRIVER
**
** \sa gadi_pwm_get_speed
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_get_speed(GADI_SYS_HandleT handle, GADI_U32 *speed);

/*!
*******************************************************************************
** \brief set speed of one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  speed     Valid ADI pwm speed(Hz).
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
** - #GADI_PWM_ERR_FROM_DRIVER
**
** \sa gadi_pwm_set_speed
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_set_speed(GADI_SYS_HandleT handle, GADI_U32 speed);

/*!
*******************************************************************************
** \brief get mode of one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  *mode     Valid ADI pwm work mode [0~1].
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
** - #GADI_PWM_ERR_FROM_DRIVER
**
** \sa gadi_pwm_get_mode
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_get_mode(GADI_SYS_HandleT handle, GADI_U8 *mode);

/*!
*******************************************************************************
** \brief set mode of one pwm channel.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  mode     Valid ADI pwm work mode [0~1].
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_BAD_PARAMETER
** - #GADI_PWM_ERR_FROM_DRIVER
**
** \sa gadi_pwm_set_mode
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_set_mode(GADI_SYS_HandleT handle, GADI_U8 mode);

/*!
*******************************************************************************
** \brief get the pwm device duty rate.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  *duty       get duty rate.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_FROM_DRIVER
** - #GADI_PWM_ERR_BAD_PARAMETER
**
** \sa gadi_pwm_get_duty
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_get_duty(GADI_SYS_HandleT handle, GADI_U32 *duty);

/*!
*******************************************************************************
** \brief set the pwm device duty rate.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  duty       duty rate [1 ~ 999].
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_FROM_DRIVER
** - #GADI_PWM_ERR_BAD_PARAMETER
**
** \sa gadi_pwm_set_duty
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_set_duty(GADI_SYS_HandleT handle, GADI_U32 duty);

#if 0
/*!
*******************************************************************************
** \brief enable the dc iris.
**
** \param[in]  handle     Valid ADI pwm instance handle previously opened by
**                        #gadi_pwm_open.
** \param[in]  enable     flag of disable or enable.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_FROM_DRIVER
** - #GADI_PWM_ERR_BAD_PARAMETER
**
** \sa gadi_pwm_open
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_dc_iris_enable(GADI_SYS_HandleT handle, GADI_U8 enable);

/*!
*******************************************************************************
** \brief get the dc iris pid data.
**
** \param[in]  pPidCoef     date pointer of struct GADI_PWM_IrisPidCoef.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_FROM_DRIVER
** - #GADI_PWM_ERR_BAD_PARAMETER
**
** \sa gadi_pwm_open
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_dc_iris_get_pid_coef(GADI_PWM_IrisPidCoef * pPidCoef);

/*!
*******************************************************************************
** \brief set the dc iris pid data.
**
** \param[in]  pPidCoef     date pointer of struct GADI_PWM_IrisPidCoef.
**
** \return
** - #GADI_OK
** - #GADI_PWM_ERR_FROM_DRIVER
** - #GADI_PWM_ERR_BAD_PARAMETER
**
** \sa gadi_pwm_open
**
*******************************************************************************
*/
GADI_ERR gadi_pwm_dc_iris_set_pid_coef(GADI_PWM_IrisPidCoef * pPidCoef);
#endif

#ifdef __cplusplus
    }
#endif


#endif /* _ADI_PWM_H_ */
