/******************************************************************************
** \file        adidemo/demo/src/ap.c
**
** \brief       ADI layer ap(aec,ns,agc etc.) test.
**
** \attention   THIS SAMPLE CODE IS PROVIDED AS IS. GOKE MICROELECTRONICS
**              ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR
**              OMMISSIONS
**
** (C) Copyright 2013-2014 by GOKE MICROELECTRONICS CO.,LTD
**
*****************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <user_wrappers/audio_process.h>
#include <adi_audio.h>
#include "ap.h"


static int ap_aec_init(int sample_rate, int frame_samples);
static int ap_aec_process(void *ai_data, void *ao_data, void *aec_data);
static int ap_aec_deinit(void);

int ap_aec_register(int *ptrAecHandle)
{

    GADI_AUDIO_EchoCancellationT webrtc_aec;

    sprintf(webrtc_aec.aecName, "ap");
    webrtc_aec.aecInit = ap_aec_init;
    webrtc_aec.aecDeinit = ap_aec_deinit;
    webrtc_aec.aecProcess = ap_aec_process;
    return gadi_audio_register_echo_cancellation(ptrAecHandle, &webrtc_aec);
}

int ap_aec_unregister(int aecHandle)
{

    return gadi_audio_unregister_echo_cancellation(aecHandle);
}

static int ap_aec_init(int sample_rate, int frame_samples)
{
    audio_process_attribute audio_process_attr;

    audio_process_attr.sampleRate = sample_rate;
    audio_process_attr.frameSamples = frame_samples;

    audio_process_attr.aecMode = AUDIO_PROCESS_AEC_FLOAT;   // float vs. fixed
    audio_process_attr.agcEnable = AUDIO_PROCESS_TRUE;      // automatic gain control
    audio_process_attr.nsEnable = AUDIO_PROCESS_TRUE;       // noise suppression on/off

    audio_process_attr.nsParam= 2;          // Noise suppression: 0-3, default 2
    audio_process_attr.aecFixedParam = 0;   // 0-4, default unclear
    audio_process_attr.aecFloatParam = 1;   // 0-2, default unclear
    audio_process_attr.agcParam= 9;         // 0-90, default 9

    return audio_process_init(&audio_process_attr);
}

static int ap_aec_process(void *ai_data, void *ao_data, void *aec_data)
{
    return audio_process_proc(ai_data, ao_data, aec_data);

}

int ap_aec_deinit(void)
{
    return audio_process_deinit();
}
