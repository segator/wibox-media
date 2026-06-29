/*!
*****************************************************************************
** \file        ./adi/inc/adi_sys.h
**
** \brief       system functionality porting.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/
#ifndef _ADI_SYS_H_
#define _ADI_SYS_H_

#include "stdio.h"
#include "adi_types.h"

//*****************************************************************************
//*****************************************************************************
//** Defines and Macros
//*****************************************************************************
//*****************************************************************************

#define Q9_BASE         (512000000)

#ifndef DIV_ROUND
#define DIV_ROUND(divident, divider)    ( ( (divident)+((divider)>>1)) / (divider) )
#endif

#ifndef ROUND_UP
#define ROUND_UP(x, n)  ( ((x)+(n)-1u) & ~((n)-1u) )
#endif

#ifndef MIN
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef ABS
#define ABS(x) ({               \
        int __x = (x);          \
        (__x < 0) ? -__x : __x;     \
            })
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array)   (sizeof(array) / sizeof(array[0]))
#endif


/*
*******************************************************************************
** Defines for general error codes of the module.
*******************************************************************************
*/
/*! Bad parameter passed. */
#define GADI_SYS_ERR_BAD_PARAMETER                                          \
                              (GADI_SYS_MODULE_BASE + GADI_ERR_BAD_PARAMETER)
/*! Memory allocation failed. */
#define GADI_SYS_ERR_OUT_OF_MEMORY                                          \
                              (GADI_SYS_MODULE_BASE + GADI_ERR_OUT_OF_MEMORY)
/*! Device already initialised. */
#define GADI_SYS_ERR_ALREADY_INITIALIZED                                    \
                        (GADI_SYS_MODULE_BASE + GADI_ERR_ALREADY_INITIALIZED)
/*! Device not initialised. */
#define GADI_SYS_ERR_NOT_INITIALIZED                                        \
                            (GADI_SYS_MODULE_BASE + GADI_ERR_NOT_INITIALIZED)
/*! Feature or function is not available. */
#define GADI_SYS_ERR_FEATURE_NOT_SUPPORTED                                  \
                      (GADI_SYS_MODULE_BASE + GADI_ERR_FEATURE_NOT_SUPPORTED)
/*! Timeout occured. */
#define GADI_SYS_ERR_TIMEOUT                                                \
                                    (GADI_SYS_MODULE_BASE + GADI_ERR_TIMEOUT)
/*! The device is busy, try again later. */
#define GADI_SYS_ERR_DEVICE_BUSY                                            \
                                (GADI_SYS_MODULE_BASE + GADI_ERR_DEVICE_BUSY)
/*! Invalid handle was passed. */
#define GADI_SYS_ERR_INVALID_HANDLE                                         \
                             (GADI_SYS_MODULE_BASE + GADI_ERR_INVALID_HANDLE)
/*! Semaphore could not be created. */
#define GADI_SYS_ERR_SEMAPHORE_CREATE                                       \
                           (GADI_SYS_MODULE_BASE + GADI_ERR_SEMAPHORE_CREATE)
/*! The driver's used version is not supported. */
#define GADI_SYS_ERR_UNSUPPORTED_VERSION                                    \
                        (GADI_SYS_MODULE_BASE + GADI_ERR_UNSUPPORTED_VERSION)
/*! Thread could not be created. */
#define GADI_SYS_ERR_THREAD_CREATE                                          \
                        (GADI_SYS_MODULE_BASE + GADI_ERR_THREAD_CREATE)
/*
*******************************************************************************
** Defines for specialized error codes which are very specific
** to the behaviour of this module. These codes are offset by -100 from
** the module general error codes.
*******************************************************************************
*/
/*!
*******************************************************************************
** \brief Error base for system module local errors.
*******************************************************************************
*/
#define GAPI_SYS_ERR_BASE                      (GADI_SYS_MODULE_BASE - 100)
/*!
*******************************************************************************
** \brief Seamphore handle is invalid.
*******************************************************************************
*/
#define GADI_SYS_ERR_INVALID_SEMAPHORE_HANDLE                                 \
                                                      (GAPI_SYS_ERR_BASE -   1)
/*!
*******************************************************************************
** \brief Message queue creating fail.
*******************************************************************************
*/
#define GADI_SYS_ERR_MSG_QUEUE_CREATE                                         \
                                                      (GAPI_SYS_ERR_BASE -   2)
/*!
*******************************************************************************
** \brief Message queue full.
*******************************************************************************
*/
#define GADI_SYS_ERR_MSG_QUEUE_FULL                                           \
                                                      (GAPI_SYS_ERR_BASE -   3)
/*!
*******************************************************************************
** \brief Message queue error happened in sending process.
*******************************************************************************
*/
#define GADI_SYS_ERR_MSG_QUEUE_SEND                                           \
                                                      (GAPI_SYS_ERR_BASE -   4)
#define GADI_SYS_THREAD_PRIO_MIN                      0
#define GADI_SYS_THREAD_PRIO_MAX                      99
#define GADI_SYS_THREAD_PRIO_DEFAULT                  GADI_SYS_THREAD_PRIO_MIN
#define GADI_SYS_THREAD_STATCK_SIZE_DEFAULT           4096 /* Kbyte*/
/*!
*******************************************************************************
** \brief thread function prototype
*******************************************************************************
*/
typedef GADI_VOID (*GADI_SYS_ThreadFunctionT)(GADI_VOID *);


//*****************************************************************************
//*****************************************************************************
//** Enumerated types
//*****************************************************************************
//*****************************************************************************

/*!
*******************************************************************************
** \brief adi log level.
*******************************************************************************
*/
typedef enum {
    GADI_SYS_LOG_LEVEL_ERROR = 0,
    GADI_SYS_LOG_LEVEL_DEBUG,
    GADI_SYS_LOG_LEVEL_INFO,
    GADI_SYS_LOG_LEVEL_NUM,
} GADI_SYS_LogLevelEnumT;

typedef enum {
    GADI_SYS_CHIP_ID_UNKNOWN      = 0,
    GADI_SYS_CHIP_ID_GK7102       = 1,
    GADI_SYS_CHIP_ID_GK7101       = 2,
    GADI_SYS_CHIP_ID_GK7102S      = 3,
    GADI_SYS_CHIP_ID_GK7101S      = 4,
    GADI_SYS_CHIP_ID_GK7102C      = 5,
    GADI_SYS_CHIP_ID_GK7202       = 6,
    GADI_SYS_CHIP_ID_GK7102C_A    = 7,
    GADI_SYS_CHIP_ID_GK7102C_T    = 8,
    GADI_SYS_CHIP_ID_GK7102C_W    = 9,
} GADI_SYS_ChipIdEnumT;

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
** \brief Initialize the ADI system module.
**
**
** \return
** - #GADI_OK
** - #GADI_ERR_ALREADY_INITIALIZED
**
** \sa gadi_sys_exit
**
*******************************************************************************
*/
GADI_ERR gadi_sys_init(void);

/*!
*******************************************************************************
** \brief Shutdown the ADI system module.
**
** \return
** - #GADI_OK
** - #GADI_ERR_NOT_INITIALIZED
**
** \sa gadi_sys_init
**
*******************************************************************************
*/
GADI_ERR gadi_sys_exit(void);

/*!
*******************************************************************************
** \brief Get the ADI layer printf log level.
**
** \param[out] plogLevel pointer to get the log level.
**
** \return
** - #GADI_OK
** - #GADI_SYS_ERR_BAD_PARAMETER
**
** \sa gadi_sys_set_log_level
**
*******************************************************************************
*/
GADI_ERR gadi_sys_get_log_level(GADI_U32* plogLevel);

/*!
*******************************************************************************
** \brief Set the ADI layer printf log level.
**
** \param[in] logLevel Specifies the print log level.
**
** \return
** - #GADI_OK
** - #GADI_SYS_ERR_BAD_PARAMETER
**
** \sa gadi_sys_get_log_level
**
*******************************************************************************
*/
GADI_ERR gadi_sys_set_log_level(GADI_SYS_LogLevelEnumT logLevel);

/*!
*******************************************************************************
** \brief Allocates \e size bytes and returns the pointer to the allocated memory.
**
** \param[in]  size   Memory size in bytes.
**
** \return In success, return the valid memory pointer.
**         In failure, return NULL.
**
** \sa gadi_sys_free
**
*******************************************************************************
*/
GADI_VOID *gadi_sys_malloc(GADI_U32 size);

/*!
*******************************************************************************
** \brief Free the memory space pointed to by \e prt, which must have been
**        returned by a previous call to #gadi_sys_malloc.
**
** \note  If ptr is NULL, no operation will be performed.
**
** \param[in]  ptr   Pointer to the memory space allocated previously by
**                   #gadi_sys_malloc.
**
**
** \sa
**  - gadi_sys_malloc_osd
**
*******************************************************************************
*/
GADI_VOID gadi_sys_free(GADI_VOID *ptr);

/*!
*******************************************************************************
** \brief This API copies \e size bytes from source memory \e src to the
**        destnation memory \e dest.
**
** \note  The \e dest and \e src should not overlap.
**
**
** \param[in]  dest  Destnation memory.
** \param[in]  src   Source memory.
** \param[in]  size  Size in bytes.
**
** \return
**      - #NULL memory copy fail.
**      - >0    a pointer to the destination address.
**
** \sa gadi_sys_memset
**
*******************************************************************************
*/
GADI_VOID* gadi_sys_memcpy(GADI_VOID *dest, GADI_VOID *src, GADI_U32 size);

/*!
*******************************************************************************
** \brief This API fills the first \e n bytes of the memory area pointed to by
**        the pointer of \e ptr, with the constant byte \e c.
**
**
** \param[in]  ptr  Pointer to the memory area to be filled.
** \param[in]  c    Constant byte value used to fill the specified memory.
** \param[in]  n    Number of bytes to fill.
**
** \return
**      - #NULL memory set fail.
**      - >0    a pointer to the destination address.
**
** \sa gadi_sys_memcpy
**
*******************************************************************************
*/
GADI_VOID* gadi_sys_memset(GADI_VOID *ptr, GADI_U8 c, GADI_U32 n);

/*!
*******************************************************************************
** \brief Create an semphore with the given initial value \e initValue.
**
**
** \param[in] initValue Specifies the initial value for the semaphore.
**
**
** \return Return an valid handle value if the semaphore creating is successful.
**         Return 0 if creating failed.
**
** \sa gadi_sys_sem_destroy
**
*******************************************************************************
*/
GADI_SYS_SemHandleT gadi_sys_sem_create(GADI_U32 initValue);

/*!
*******************************************************************************
** \brief Increments(unlocks) the semaphore referred by \e sem.
**
**
** \param[in] sem   Valid semaphore handle created previously by
**                  #gadi_sys_sem_create.
**
**
** \return
** - #GADI_OK   Successful completion.
**
** \sa gadi_sys_sem_wait
** \sa gadi_sys_sem_wait_timeout
**
*******************************************************************************
*/
GADI_ERR gadi_sys_sem_post(GADI_SYS_SemHandleT sem);

/*!
*******************************************************************************
** \brief Dncrements(locks) the semaphore referred by \e sem.
**
**
** \param[in] sem   Valid semaphore handle created previously by
**                  #gadi_sys_sem_create.
**
**
** \return
** - #GADI_OK   Successful completion.
**
** \sa
**  - gadi_sys_sem_post
**  - gadi_sys_sem_wait_timeout
**
*******************************************************************************
*/
GADI_ERR gadi_sys_sem_wait(GADI_SYS_SemHandleT sem);


/*!
*******************************************************************************
** \brief This API is the same as gadi_sys_sem_wait, except that \e msecs
**        specifies a limitation on the amount of the time that the call should
**        block if the decrement can not be performed immediately.
**
**
** \param[in] sem   Valid semaphore handle created previously by
**                  #gadi_sys_sem_create.
** \param[in] msecs Timeout value in millisecond.
**
**
** \return
** - #GADI_OK   Successful completion.
**
** \sa
**      - gadi_sys_sem_post
**      - gadi_sys_sem_wait
**
*******************************************************************************
*/
GADI_ERR gadi_sys_sem_wait_timeout(GADI_SYS_SemHandleT sem, GADI_U32 msecs);

/*!
*******************************************************************************
** \brief Destroy the semaphore \e sem.
**
**
** \param[in] sem   Valid semaphore handle created previously by
**                  #gadi_sys_sem_create.
**
**
** \return
** - #GADI_OK   Successful completion.
**
** \sa gadi_sys_sem_create
**
*******************************************************************************
*/
GADI_ERR gadi_sys_sem_destroy(GADI_SYS_SemHandleT sem);

/*!
*******************************************************************************
** \brief Starts a new thread in the call process.
**
**
** \param[in]  func     The routine executed in the created thread.
** \param[in]  arg      The argument passed to the \e func.
** \param[in]  priority Priority setting.
** \param[in]  ssize    Stack size in Kbytes.
**                      In RTOS environment, this parameter is ignored.
** \param[in]  name     Thread name.
** \param[out] thread   Thread identifier or handle.
**
** \return
** - #GADI_OK
**
** \sa
**      - gadi_sys_thread_destroy
**
*******************************************************************************
*/
GADI_ERR gadi_sys_thread_create(GADI_SYS_ThreadFunctionT func,
                            GADI_VOID              *arg,
                            GADI_S32                priority,
                            GADI_S32                ssize,
                            const GADI_CHAR          *name,
                            GADI_SYS_ThreadHandleT *thread);

/*!
*******************************************************************************
** \brief Start multitasking.
**
** \note  This API is valid only in RTOS environment. In Linux, this API does
**        nothing.
**
*******************************************************************************
*/
GADI_VOID gadi_sys_multi_task_start(void);

/*!
*******************************************************************************
** \brief Stop and destroy the given thread.
**
**
** \param[in]  thread The identifier of the thread to be destroyed.
**
** \return
** - #GADI_OK
**
** \sa
**      - gadi_sys_thread_create
**
*******************************************************************************
*/
GADI_ERR gadi_sys_thread_destroy(GADI_SYS_ThreadHandleT thread);

/*!
*******************************************************************************
** \brief Stop and destroy the calling thread.
**
**
** \sa
**      - gadi_sys_thread_self
**
*******************************************************************************
*/
GADI_SYS_ThreadHandleT gadi_sys_thread_self(void);

/*!
*******************************************************************************
** \brief Stop and destroy the calling thread.
**
**
** \sa
**      - gadi_sys_thread_destroy
**
*******************************************************************************
*/
GADI_VOID gadi_sys_thread_self_destroy(void);

/*!
*******************************************************************************
** \brief Makes the call thread to sleep until the given milliseconds have passed.
**
**
** \param[in] msecs Sleep time in milliseconds.
**
** \return
**      - #GADI_OK
**
*******************************************************************************
*/
GADI_VOID gadi_sys_thread_sleep(GADI_U32 msecs);

/*!
*******************************************************************************
** \brief causes the calling thread to relinquish the CPU.
**
** \return
**      - #GADI_OK
**
*******************************************************************************
*/
GADI_ERR gadi_sys_thread_yield(void);

/*!
*******************************************************************************
** \brief Suspend the specified thread.
**
**
** \param[in]  thread The identifier of the thread to be suspended.
**
**
** \sa
**      - gadi_sys_thread_resume
**
*******************************************************************************
*/
GADI_VOID gadi_sys_thread_suspend(GADI_SYS_ThreadHandleT thread);

/*!
*******************************************************************************
** \brief Suspend the thread which is call this API.
**
**
** \sa
**      - gadi_sys_thread_resume
**
*******************************************************************************
*/
GADI_VOID gadi_sys_thread_self_suspend(void);

/*!
*******************************************************************************
** \brief Resume the specified thread which has been suspended before.
**
**
** \param[in]  thread The identifier of the thread to be resumed.
**
**
** \sa gadi_sys_thread_suspend
**
*******************************************************************************
*/
GADI_VOID gadi_sys_thread_resume(GADI_SYS_ThreadHandleT thread);

/*!
*******************************************************************************
** \brief Wait for termination of another thread.
**
** this function suspends execution of the calling thread until the target
** thread terminates, unless the target thread has already terminated.
**
** \param[in]  thread The identifier of the thread to be waited.
**
** \return
**      - #GADI_OK
**
** \sa gadi_sys_thread_create
**
*******************************************************************************
*/
GADI_ERR gadi_sys_wait_end_thread(GADI_SYS_ThreadHandleT thread);

/*!
*******************************************************************************
** \brief Display the thread statistics information via uart message.
*******************************************************************************
*/
GADI_VOID gadi_sys_thread_statistics(void);


/*!
*******************************************************************************
** \brief get the current date info.
**
** \str[out] out put the date information as string format.
**
** \sa gadi_sys_init
**
*******************************************************************************
*/
GADI_VOID gadi_sys_get_date(GADI_CHAR *str);


/*!
*******************************************************************************
** \brief load firmware.
**
** \**
** \sa gadi_sys_load_firmware
**
*******************************************************************************
*/
GADI_ERR gadi_sys_load_firmware(void);

/*!
*******************************************************************************
** \brief Get adi version.
**
** \return
** - #adi version
**
**
*******************************************************************************
*/
GADI_U32 gadi_sys_get_version(void);

/*!
*******************************************************************************
** \brief Get dsp interrupte count value.
**
** \return
** - #adi version
**
**
*******************************************************************************
*/

GADI_ERR gadi_sys_get_interrupt(GADI_ULONG *times);

/*!
*******************************************************************************
** \brief Get chip id.
**
** \return
** - #chip id
**
**
*******************************************************************************
*/

GADI_SYS_ChipIdEnumT gadi_sys_get_chipid(void);

/*!
*******************************************************************************
** \brief Print a debug message in debug mode.
**
** \param format The aruments in 'printf' style.
**
*******************************************************************************
*/
#ifdef DEBUG /* DEBUG is only valid in the debug-mode make process */

extern GADI_U32 adiLogLevel;

#define ADI_PRINT(mylog, LOG_LEVEL, format, args...)    \
                do {                                    \
                    if (mylog >= LOG_LEVEL) {           \
                        printf(format, ##args);         \
                    }                                   \
                } while (0)

#define GADI_ERROR(format, args...) ADI_PRINT(adiLogLevel, GADI_SYS_LOG_LEVEL_ERROR, "[ERROR] [%s: %d]" format "\n", __FILE__, __LINE__, ##args)
#define GADI_DEBUG(format, args...) ADI_PRINT(adiLogLevel, GADI_SYS_LOG_LEVEL_DEBUG, "[DEBUG] " format, ##args)
#define GADI_INFO(format, args...)  ADI_PRINT(adiLogLevel, GADI_SYS_LOG_LEVEL_INFO,  "[INFO] " format, ##args)
#define GADI_PRI(format, args...)   printf("[PRI] " format, ##args)

#else

#define GADI_ERROR(format, args...) printf("[ERROR] " format, ##args)
#define GADI_DEBUG(format, args...)
#define GADI_INFO(format, args...)
#define GADI_PRI(format, args...)   printf("[PRI] " format, ##args)

#endif

#ifdef __cplusplus
    }
#endif


#endif /* _ADI_SYS_H_ */
