/*!
*****************************************************************************
** \file        ./adi/include/adi_ptz.h
**
** \brief       adi PTZ module porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/
#ifndef _ADI_PTZ_H_
#define _ADI_PTZ_H_

#include "stdio.h"
#include "adi_types.h"

//*****************************************************************************
//*****************************************************************************
//** Defines and Macros
//*****************************************************************************
//*****************************************************************************
/*! Bad parameter passed. */
#define GADI_PTZ_ERR_BAD_PARAMETER                                          \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_PTZ_ERR_OUT_OF_MEMORY                                          \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_PTZ_ERR_ALREADY_INITIALIZED                                    \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_PTZ_ERR_NOT_INITIALIZED                                        \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_PTZ_ERR_FEATURE_NOT_SUPPORTED                                  \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_PTZ_ERR_TIMEOUT                                                \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_PTZ_ERR_DEVICE_BUSY                                            \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_PTZ_ERR_INVALID_HANDLE                                         \
                              (GADI_PTZ_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_PTZ_ERR_SEMAPHORE_CREATE                                       \
                               (GADI_PTZ_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_PTZ_ERR_UNSUPPORTED_VERSION                                    \
                               (GADI_PTZ_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The driver's used version is not supported. */
#define GADI_PTZ_ERR_FROM_DRIVER                                            \
                               (GADI_PTZ_MODULE_BASE + GADI_ERR_FROM_DRIVER)
/*! The device/handle is not open.. */
#define GADI_PTZ_ERR_NOT_OPEN                                               \
                               (GADI_PTZ_MODULE_BASE + GADI_ERR_NOT_OPEN)


//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************

/*!
*******************************************************************************
** \brief PTZ protocol type.
*******************************************************************************
*/

typedef enum  {
    GADI_PTZ_PELCO_D,
    GADI_PTZ_PELCO_P,
    GAID_PTZ_PROTOCOL_COUNT
}GADI_PTZ_ProtocolEnumT;

//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief tty device attributes
*******************************************************************************
*/
typedef struct
{
    /*! baud rate*/
    GADI_U32 speed;
    /*! date bits*/
    GADI_U8 dataBits;
    /*! parity bit*/
    GADI_U8 parity;
    /*! stop bits*/
    GADI_U8 stopBits;
}GADI_PTZ_TtyDevOptT;

/*!
*******************************************************************************
** \brief PTZ protocol command
*******************************************************************************
*/
typedef struct
{
    GADI_U32 cmdSize;
    void *cmd;
}GADI_PTZ_ProtocolCmdT;

/*!
*******************************************************************************
** \brief PTZ protocol function
*******************************************************************************
*/
typedef struct
{
    void (*ptzSetAddress)(GADI_U8 address);
    void (*ptzStop)(void);
    void (*ptzPanLeft)(GADI_U8 panSpeed);
    void (*ptzPanRight)(GADI_U8 panSpeed);
    void (*ptzTiltUp)(GADI_U8 tiltSpeed);
    void (*ptzTiltDown)(GADI_U8 tiltSpeed);
    void (*ptzPanTiltRightUp)(GADI_U8 panSpeed, GADI_U8 tiltSpeed);
    void (*ptzPanTiltRightDown)(GADI_U8 panSpeed, GADI_U8 tiltSpeed);
    void (*ptzPanTiltLeftUp)(GADI_U8 panSpeed, GADI_U8 tiltSpeed);
    void (*ptzPanTiltLeftDown)(GADI_U8 panSpeed, GADI_U8 tiltSpeed);
    void (*ptzZoomTele)(void);
    void (*ptzZoomWide)(void);
    void (*ptzFocusFar)(void);
    void (*ptzFocusNear)(void);
    void (*ptzIrisOpen)(void);
    void (*ptzIrisClose)(void);
    void (*ptzAutoScan)(void);
    void (*ptzSetPreset)(GADI_U8 presetNum);
    void (*ptzClearPreset)(GADI_U8 presetNum);
    void (*ptzGoToPreset)(GADI_U8 presetNum);
}GADI_PTZ_ProtocolFunsT;
/*!
*******************************************************************************
** \brief The Handle Data Structure containing operations for all ptz protocols
*******************************************************************************
*/
typedef struct
{
    const GADI_CHAR *name[GAID_PTZ_PROTOCOL_COUNT];
    GADI_PTZ_ProtocolEnumT protocol;
    const GADI_PTZ_ProtocolCmdT *ptzProtocolCmd[GAID_PTZ_PROTOCOL_COUNT];
    GADI_PTZ_ProtocolFunsT *ptzProtocolFuns[GAID_PTZ_PROTOCOL_COUNT];
    GADI_ERR (*ptzFeedCmd)(const GADI_PTZ_ProtocolCmdT *pCmd);
    void (*ptzFlushCmds)(GADI_PTZ_ProtocolCmdT *pCmd);
}GADI_PTZ_ControlHandleT;

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
** \brief Initialize the ADI PTZ module.
**
**
** \param[in]  ptzControlHandle   ptz protocols handle
**
**
** \return
** - #GADI_OK succeed
** - #GADI_PTZ_ERR_ALREADY_INITIALIZED ptz already initialised
** - #GADI_PTZ_ERR_OUT_OF_MEMORY out of memory
**
** \sa
** - gadi_ptz_exit
*******************************************************************************
*/
GADI_ERR gadi_ptz_init(GADI_PTZ_ControlHandleT  *ptzControlHandle);
/*!
*******************************************************************************
** \brief The function register a ptz protocol
**
**
** \param[in]  ptzControlHandle ptz protocols handle Initialized by
** gadi_ptz_init
** \param[in]  name ptz protocol name
** \param[in]  protocol ptz protocol type
** \param[in]  pCmd ptz protocol command
** \param[in]  pFuns ptz protocol functions
**
** \return NULL
**
** \sa
** - gadi_ptz_unregister_protocol
*******************************************************************************
*/
void gadi_ptz_register_protocol(GADI_PTZ_ControlHandleT *ptzControlHandle,
                                const GADI_CHAR *name,
                                GADI_PTZ_ProtocolEnumT protocol,
                                const GADI_PTZ_ProtocolCmdT *pCmd,
                                GADI_PTZ_ProtocolFunsT *pFuns);
/*!
*******************************************************************************
** \brief The function unregister a ptz protocol
**
**
** \param[in]  ptzControlHandle ptz protocols handle Initialized by
** gadi_ptz_init
** \param[in]  protocol ptz protocol type
**
** \return NULL
**
** \sa
** - gadi_ptz_register_protocol
*******************************************************************************
*/
void gadi_ptz_unregister_protocol(GADI_PTZ_ControlHandleT *ptzControlHandle,
                                GADI_PTZ_ProtocolEnumT protocol);
/*!
*******************************************************************************
** \brief Open a tty device
**
**
** \param[in]  node  The pointer to node of PTZ tty devide
** \param[in]  errorCodePtr    The pointer for knowing the cause of internal
** failure
**
**
** \return A handle of type GADI_SYS_HandleT is returned.This handle has all
** the fields for proper handling of the tty device.
**
** \sa
** - gadi_ptz_tty_close
*******************************************************************************
*/
GADI_SYS_HandleT gadi_ptz_tty_open(GADI_CHAR *node, GADI_ERR* errorCodePtr);
/*!
*******************************************************************************
** \brief set up a tty device attributes
**
** (addition description is here)
**
** \param[in]  handle  Specifies tty device is to be setup attributes
** \param[in]  ttyOpt  tty device options
**
** \return
** - #GADI_OK succeed
** - #GADI_PTZ_ERR_BAD_PARAMETER invalid parameter pass
** - #GADI_PTZ_ERR_FROM_DRIVER driver not support
**
** \sa
** - gadi_ptz_tty_reset
*******************************************************************************
*/
GADI_ERR gadi_ptz_tty_set(GADI_SYS_HandleT handle, GADI_PTZ_TtyDevOptT *ttyOpt);
/*!
*******************************************************************************
** \brief reset a tty device attributes
**
**
** \param[in]  handle  Specifies tty device is to be reset attributes
**
**
** \return
** - #GADI_OK succeed
** - #GADI_PTZ_ERR_FROM_DRIVER driver not support
**
** \sa
** - gadi_ptz_tty_set
*******************************************************************************
*/
GADI_ERR gadi_ptz_tty_reset(GADI_SYS_HandleT handle);
/*!
*******************************************************************************
** \brief close a tty device
**
**
** \param[in]  handle  Specifies tty device is to be close
**
**
** \return NULL
**
** \sa
** - gadi_ptz_tty_open
*******************************************************************************
*/
void gadi_ptz_tty_close(GADI_SYS_HandleT handle);
/*!
*******************************************************************************
** \brief Shutdown the ADI PTZ module.
**
**
** \param[in]  ptzControlHandle   ptz protocols handle Initialized by
** gadi_ptz_init
**
** \return
** - #GADI_OK  succeed
** - #GADI_PTZ_ERR_NOT_INITIALIZED   the tty device not initialised
**
** \sa
** - gadi_ptz_init
*******************************************************************************
*/
GADI_ERR gadi_ptz_exit(GADI_PTZ_ControlHandleT  *ptzControlHandle);


#ifdef __cplusplus
    }
#endif


#endif /* _ADI_PTZ_H_ */

