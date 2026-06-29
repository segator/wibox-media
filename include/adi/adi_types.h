/*!
*****************************************************************************
** \file        ./adi/inc/adi_types.h
**
** \brief       adapte driver interface type define
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2012-2013 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _ADI_TYPES_H_
#define _ADI_TYPES_H_

#include "adi_sys_error.h"
#include "basetypes.h"

//*****************************************************************************
//*****************************************************************************
//** Defines and Macros
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief ADI type define
*******************************************************************************
*/
typedef unsigned char       GADI_U8;         /*!< 8 bit unsigned integer. */
typedef unsigned short      GADI_U16;        /*!< 16 bit unsigned integer. */
typedef unsigned int        GADI_U32;        /*!< 32 bit unsigned integer. */
typedef unsigned long long  GADI_U64;        /*!< 64 bit unsigned integer. */
typedef signed char         GADI_S8;         /*!< 8 bit signed integer. */
typedef signed short        GADI_S16;        /*!< 16 bit signed integer. */
typedef signed int          GADI_S32;        /*!< 32 bit signed integer. */
typedef signed long long    GADI_S64;        /*!< 64 bit unsigned integer. */
typedef signed int          GADI_ERR;        /*!< error code type .*/
typedef unsigned int        GADI_HANDLE;     /*!< 32 bit unsigned integer. */
typedef unsigned short      GADI_USHORT;     /*!< unsigned short. */
typedef short               GADI_SHORT;      /*!< short. */
typedef unsigned int        GADI_UINT;       /*!< unsigned int. */
typedef int                 GADI_INT;        /*!< int. */
typedef unsigned long       GADI_ULONG;      /*!< unsigned long. */
typedef long                GADI_LONG;       /*!< long. */
typedef unsigned long long  GADI_ULONGLONG; /*!< unsigned long long. */
typedef long long           GADI_LONGLONG;  /*!< long long. */
typedef char                GADI_UCHAR;      /*!< unsigned char */
typedef char                GADI_CHAR;       /*!< char */
typedef void                GADI_VOID;       /*!< void */

/*! common handle type */
typedef void*               GADI_SYS_HandleT;

/*! semaphore handle type */
typedef unsigned int        GADI_SYS_SemHandleT;

/*! thread handle type */
typedef unsigned int        GADI_SYS_ThreadHandleT;


#if defined(__HIGHC__)
    #undef NULL
    #define NULL 0
#else

#ifndef NULL
    #define NULL 0  /*!< NULL define to 0. */
#endif
#endif


/*!
*******************************************************************************
** \brief Common inline key word.
*******************************************************************************
*/
#if defined(__HIGHC__)
#define FINLINE _Inline
#else
#define FINLINE inline
#endif

#if defined(__GNUC__)
#define _Unaligned
#endif

/*!
*******************************************************************************
** \brief Branch optimize macro.
*******************************************************************************
*/
#ifdef __GNUC__
#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)       (x)
#define UNLIKELY(x)     (x)
#endif

//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************
/*!
*******************************************************************************
** \brief ADI BOOL enum
*******************************************************************************
*/
typedef enum {
    GADI_FALSE = 0,  /*!< Logical false. */
    GADI_TRUE  = 1   /*!< Logical true. */
} GADI_BOOL;


//*****************************************************************************
//*****************************************************************************
//** Data Structures
//*****************************************************************************
//*****************************************************************************



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



#ifdef __cplusplus
}
#endif


#endif /* _ADI_TYPES_H_ */

