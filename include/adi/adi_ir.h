/*!
*****************************************************************************
** \file        ./adi/inc/adi_ir.h
**
** \brief       ADI infrared remote header.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2012-2013 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/


#ifndef _ADI_IR_H_
#define _ADI_IR_H_

#include "adi_types.h"

/*!
*******************************************************************************
** Defines for general error codes of the module.
*******************************************************************************
*/
/*! Bad parameter passed. */
#define GADI_IR_ERR_BAD_PARAMETER                                          \
                              (GADI_IR_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_IR_ERR_OUT_OF_MEMORY                                          \
                              (GADI_IR_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_IR_ERR_ALREADY_INITIALIZED                                    \
                        (GADI_IR_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_IR_ERR_NOT_INITIALIZED                                        \
                            (GADI_IR_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_IR_ERR_FEATURE_NOT_SUPPORTED                                  \
                      (GADI_IR_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_IR_ERR_TIMEOUT                                                \
                                    (GADI_IR_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_IR_ERR_DEVICE_BUSY                                            \
                                (GADI_IR_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_IR_ERR_INVALID_HANDLE                                         \
                             (GADI_IR_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_IR_ERR_SEMAPHORE_CREATE                                       \
                           (GADI_IR_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_IR_ERR_UNSUPPORTED_VERSION                                    \
                        (GADI_IR_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! The error occurs at the lower driver layer. */
#define GADI_IR_ERR_FROM_DRIVER                                            \
                                (GADI_IR_MODULE_BASE + GADI_ERR_FROM_DRIVER)


/*!
****************************************************************************
** Additional driver error codes (reserved from -100 to -999)
**
** Each error code shall be decreased in steps of 1.
****************************************************************************
*/

/*! In case no key available */
#define GADI_IR_KEY_UNDEFINED  0x0000

/*!
*******************************************************************************
** \brief Infrared remote type enumeration.
*******************************************************************************
*/
typedef enum
{
    /*! use as receiver. */
    GADI_IR_TYPE_RECEIVER,

    /*! use as transmitter. */
    GADI_IR_TYPE_TRANSMITTER,

}GADI_IR_TypeEnumT;

/*!
*******************************************************************************
** \brief Infrared remote mode enumeration.
*******************************************************************************
*/
typedef enum
{
    /*! use software mode */
    GADI_IR_SOFTWARE_MODE,

    /*! use hardware mode */
    GADI_IR_HARDWARE_MODE,

}GADI_IR_ModeEnumT;

/*!
*******************************************************************************
** \brief Hardware code protocol mode enumeration for infrared remote.
*******************************************************************************
*/
typedef enum
{

    /*!
        A protocol defined by NEC.
    */
    GADI_IR_PROTOCOL_NEC = 0,

    /*!
        Phillips RC5 infrared remote protocol.
    */
    GADI_IR_PROTOCOL_RC5,

    /*!
        Phillips RC6 infrared remote protocol.
    */
    GADI_IR_PROTOCOL_RC6,

    /*!
        RC-MM was defined by Philips to be a multi-media IR protocol to be used
        in wireless keyboards, mice and game pads.
    */
    GADI_IR_PROTOCOL_RCMM,

    /*!
        A protocol defined by SONY.
    */
    GADI_IR_PROTOCOL_SIRC,

    /*!
        User defined protocol 1.
    */
    GADI_IR_PROTOCOL_USER_DEFINED1,

    /*!
        User defined protocol 2.
    */
    GADI_IR_PROTOCOL_USER_DEFINED2

}GADI_IR_ProtocolEnumT;

/*!
*******************************************************************************
** \brief Key event type enumeration for infrared remote.
*******************************************************************************
*/
typedef enum
{

    /*!
        Key event is triggered by key pressed.
    */
    GADI_IR_KEY_PRESSED = 1,

    /*!
        Key event is triggered by key released.
    */
    GADI_IR_KEY_RELEASED,

    /*!
        Key event is triggered by key repeated.
    */
    GADI_IR_KEY_REPEATED

}GADI_IR_KeyTypeEnumT;

/*!
*************************************************************************
** \brief Receive notify funtion for notification if a valid key is pressed.
*************************************************************************
*/

typedef void (*GADI_IR_UserFunctionT)(GADI_SYS_HandleT irHandle,
                                       GADI_U16 keyValue,
                                       GADI_U16 rcmCode,
                                       GADI_U32 deviceId,
                                       GADI_U32 *userOptDataPtr);

/*!
*******************************************************************************
** \brief Parameters for opening an IR ADI instance.
*******************************************************************************
*/
typedef struct
{
    /*!
        IR node path.
    */
    GADI_CHAR            *nodePath;
    /*!
        IR type selection.
    */
    GADI_IR_TypeEnumT     type;

  /*!
        IR mode selection.
    */
    GADI_IR_ModeEnumT     mode;

   /*!
        IR decode protocol selection.
     */
    GADI_IR_ProtocolEnumT protocol;

   /*!
        This function allows you to be notified in case the IR handler
        has identified a IR key.
        \sa gadi_ir_set_notify_function()
     */
    GADI_IR_UserFunctionT userFunctionPtr;
   /*!
       This optData pointer is used for callback function (receiveNotifyFunction).
   */
   GADI_U32              *userOptDataPtr;

   GADI_U32              deviceID;

} GADI_IR_OpenParamsT;

/*!
*******************************************************************************
** \brief Capabilities of this chip on IR module.
*******************************************************************************
*/
typedef struct
{
    /*! Maximum number of mappping tables */
    GADI_U16    maxNumTables;

} GADI_IR_CapabilitiesT;

/*!
*************************************************************************
** \brief Struct of the mapping table.
**
** \sa    gadi_ir_register_map_table()
**
*************************************************************************
*/
typedef struct
{
    /*!
        Key value which is received by the protocol (NEC/RC5/...)
    */
    GADI_U32        keyValue;

    /*!
    ** Remote key mapping code
    ** The mapped keyspace is set from 0x0080 to 0x009F which is in ASCII a reserved
    ** character space. This is to avoid character overlapping with existing.
    */
    GADI_U32        rcmCode;

}GADI_IR_KeycodeMapT;

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
** \brief Initialize the ir ADI driver.
**
** \return
** - #GADI_OK                        initialize success
** - #GADI_ERR_ALREADY_INITIALIZED   videc ADI already initialized
**
** \sa gadi_ir_exit
*******************************************************************************
*/
GADI_ERR gadi_ir_init(void);

/*!
*******************************************************************************
** \brief Exit the ir ADI driver.
**
** This function shutdowns the ir ADI driver and safely disables the
** underlying hardware block. Pending accesses will be flushed and all allocated
** resources will be released.
**
** \sa gadi_ir_init
*******************************************************************************
*/
void gadi_ir_exit(void);

/*!
*******************************************************************************
** \brief Opens an instance of the driver..
**
**
** \param[in]  openParams    The parameters required for opening a decoder.
** \param[out] errorCodePtr  The pointer for knowing the cause of internal failure.
**
**
** \return Return an valid handle of IR ADI instance.
**
** \sa gadi_ir_close
*******************************************************************************
*/
GADI_SYS_HandleT gadi_ir_open(GADI_IR_OpenParamsT *openParams,
                              GADI_ERR *errorCodePtr);


/*!
*******************************************************************************
** \brief Close ir ADI instance.
**
**
** \param[in]  irHandle   Valid ir ADI instance handle previously opened by #gadi_ir_open.
** \param[out]              Faild to close ir device.
**
** \sa
**      - gadi_ir_open
**
*******************************************************************************
*/

GADI_ERR gadi_ir_close(GADI_SYS_HandleT handle);

/*!
*******************************************************************************
** \brief get the ir module capabilities of the this chip.
**
**
** \param[out]  caps Pointer of the data structure to be filled with capabilities.
**
**
** \return
** - #GADI_OK
**
** \sa
*******************************************************************************
*/
GADI_ERR gadi_ir_get_capabilities(GADI_IR_CapabilitiesT *caps);

/*!
*******************************************************************************
** \brief Register a key mapping table to ADI.
**
**  This function allows you to set a key mapping table.
**  Each table entry consist of a tuple with the IR key code and the
**  corresponding RCM code.
**  Different RCM devices may have different IR key codes but the corresponding
**  RCM key code should be the same.
**
** \param[in]  deviceId     Device ID from remote control.
** \param[in]  numKeyCodes  Number of key-code pairs of the map table.
** \param[in]  tablePtr     Pointer to first key code table entry.
**
**
** \return
** - #GADI_OK   Successful completion.
** - other
**
** \sa
** - gadi_ir_unregister_map_table
**
*******************************************************************************
*/
GADI_ERR gadi_ir_register_map_table(GADI_S32 deviceId, GADI_U16 numKeyCodes,
                                    GADI_IR_KeycodeMapT* tablePtr);

/*!
*******************************************************************************
** \brief Remove key mapping table from ADI.
**
**
** \param[in]  deviceId   Device ID from remote control.
**
**
** \return
** - #GADI_OK   Successful completion.
** - other      Fail.
**
** \sa
** - gadi_ir_register_map_table
**
*******************************************************************************
*/
GADI_ERR gadi_ir_unregister_map_table(GADI_S32 deviceId);

#ifdef __cplusplus
}
#endif

#endif /* _ADI_IR_H_ */

