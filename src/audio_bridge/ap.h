/*!
*****************************************************************************
** \file        ./adidemo/demo/src/ap.h
**
** \brief       adi test ap header file.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#ifndef _AP_H__
#define _AP_H__


#ifdef __cplusplus
extern "C" {
#endif


int ap_aec_register(int *ptrAecHandle);
int ap_aec_unregister(int aecHandle);



#ifdef __cplusplus
}
#endif

#endif /* _AP_H__ */
