#include "audio_hw.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "adi_audio.h"
#include "adi_sys.h"

#ifdef WIBOX_AUDIO_HW_TEST
int ap_aec_register(int *ptr_aec_handle);
int ap_aec_unregister(int aec_handle);
#else
#include "../audio_bridge/ap.c"
#endif

#define THIS_FILE "audio_hw"

static pthread_mutex_t audio_hw_mutex = PTHREAD_MUTEX_INITIALIZER;
static GADI_AUDIO_AioAttrT audio_config;
static int audio_started = 0;
static int audio_chip_enabled = 0;
static int audio_chip_gpio = 18;
static int aec_handle = -1;
static int frame_size = 160;

static int clamp_percent(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static GADI_AUDIO_GainLevelEnumT gain_from_percent(int percent)
{
    int index = (clamp_percent(percent) * 15) / 100;
    if (index < 0) index = 0;
    if (index > 15) index = 15;
    return (GADI_AUDIO_GainLevelEnumT)index;
}

static GADI_AUDIO_VolumeLevelEnumT volume_from_percent(int percent)
{
    static const GADI_AUDIO_VolumeLevelEnumT levels[] = {
        VLEVEL_0, VLEVEL_1, VLEVEL_2, VLEVEL_3, VLEVEL_4,
        VLEVEL_5, VLEVEL_6, VLEVEL_7, VLEVEL_8, VLEVEL_9,
        VLEVEL_10, VLEVEL_11, VLEVEL_12
    };
    int index = (clamp_percent(percent) * 12) / 100;

    if (index < 0) index = 0;
    if (index > 12) index = 12;
    return levels[index];
}

static void log_msg(const char* msg) {
    printf("%s: %s\n", THIS_FILE, msg);
}

static int write_gpio_value(int gpio, const char* value) {
    char gpio_path[64];
    int fd;

    snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/value", gpio);
    fd = open(gpio_path, O_WRONLY);
    if (fd < 0) {
        printf("%s: failed to open GPIO%d: %s\n", THIS_FILE, gpio, strerror(errno));
        return -1;
    }

    if (write(fd, value, 1) != 1) {
        printf("%s: failed to write GPIO%d: %s\n", THIS_FILE, gpio, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int enable_audio_chip(void) {
    if (audio_chip_enabled) {
        return 0;
    }
    if (write_gpio_value(audio_chip_gpio, "0") < 0) {
        return -1;
    }
    audio_chip_enabled = 1;
    log_msg("audio chip enabled");
    return 0;
}

static void disable_audio_chip(void) {
    if (!audio_chip_enabled) {
        return;
    }
    if (write_gpio_value(audio_chip_gpio, "1") == 0) {
        log_msg("audio chip disabled");
    }
    audio_chip_enabled = 0;
}

int audio_hw_start(int gpio, int frame_samples,
                   int input_gain_percent, int output_volume_percent) {
    GADI_ERR ret;
    GADI_AUDIO_SampleFormatEnumT sample_format = GADI_AUDIO_SAMPLE_FORMAT_A_LAW;
    GADI_AUDIO_GainLevelEnumT input_gain;
    GADI_AUDIO_VolumeLevelEnumT output_volume;

    pthread_mutex_lock(&audio_hw_mutex);
    if (audio_started) {
        pthread_mutex_unlock(&audio_hw_mutex);
        return 0;
    }

    audio_chip_gpio = gpio > 0 ? gpio : 18;
    frame_size = frame_samples > 0 ? frame_samples : 160;
    input_gain = gain_from_percent(input_gain_percent);
    output_volume = volume_from_percent(output_volume_percent);

    if (enable_audio_chip() < 0) {
        pthread_mutex_unlock(&audio_hw_mutex);
        return -1;
    }

    ret = gadi_audio_init();
    if (ret != GADI_OK) {
        printf("%s: gadi_audio_init failed: %d\n", THIS_FILE, ret);
        disable_audio_chip();
        pthread_mutex_unlock(&audio_hw_mutex);
        return -1;
    }

    memset(&audio_config, 0, sizeof(audio_config));
    audio_config.bitWidth = GADI_AUDIO_BIT_WIDTH_16;
    audio_config.soundMode = GADI_AUDIO_SOUND_MODE_SINGLE;
    audio_config.sampleRate = GADI_AUDIO_SAMPLE_RATE_8000;
    audio_config.frameSamples = (GADI_U32)frame_size;
    audio_config.frameNum = 30;

    ret = gadi_audio_set_sample_format(gadi_audio_ai_get_fd(), sample_format);
    if (ret != GADI_OK) {
        printf("%s: set AI sample format failed: %d\n", THIS_FILE, ret);
        goto fail_audio;
    }

    ret = gadi_audio_set_sample_format(gadi_audio_ao_get_fd(), sample_format);
    if (ret != GADI_OK) {
        printf("%s: set AO sample format failed: %d\n", THIS_FILE, ret);
        goto fail_audio;
    }

    ret = gadi_audio_ai_set_attr(&audio_config);
    if (ret != GADI_OK) {
        printf("%s: set AI attr failed: %d\n", THIS_FILE, ret);
        goto fail_audio;
    }

    ret = gadi_audio_ao_set_attr(&audio_config);
    if (ret != GADI_OK) {
        printf("%s: set AO attr failed: %d\n", THIS_FILE, ret);
        goto fail_audio;
    }

    ret = gadi_audio_ai_set_gain(&input_gain);
    if (ret != GADI_OK) {
        printf("%s: set AI gain failed: %d\n", THIS_FILE, ret);
        goto fail_audio;
    }

    ret = gadi_audio_ao_set_volume(&output_volume);
    if (ret != GADI_OK) {
        printf("%s: set AO volume failed: %d\n", THIS_FILE, ret);
        goto fail_audio;
    }

    ret = gadi_audio_ai_enable();
    if (ret != GADI_OK) {
        printf("%s: enable AI failed: %d\n", THIS_FILE, ret);
        goto fail_audio;
    }

    ret = gadi_audio_ao_enable();
    if (ret != GADI_OK) {
        printf("%s: enable AO failed: %d\n", THIS_FILE, ret);
        gadi_audio_ai_disable();
        goto fail_audio;
    }

    ret = ap_aec_register(&aec_handle);
    if (ret == GADI_OK) {
        ret = gadi_audio_ai_aec_enable();
        if (ret != GADI_OK) {
            printf("%s: AEC enable failed: %d\n", THIS_FILE, ret);
        }
    } else {
        printf("%s: AEC register failed: %d\n", THIS_FILE, ret);
        aec_handle = -1;
    }

    audio_started = 1;
    printf("%s: started frame_size=%d gpio=%d input_gain_percent=%d input_gain=0x%02x output_volume_percent=%d output_volume=0x%02x\n",
           THIS_FILE, frame_size, audio_chip_gpio,
           clamp_percent(input_gain_percent), (unsigned int)input_gain,
           clamp_percent(output_volume_percent), (unsigned int)output_volume);
    pthread_mutex_unlock(&audio_hw_mutex);
    return 0;

fail_audio:
    gadi_audio_exit();
    disable_audio_chip();
    pthread_mutex_unlock(&audio_hw_mutex);
    return -1;
}

void audio_hw_stop(void) {
    pthread_mutex_lock(&audio_hw_mutex);
    if (!audio_started) {
        pthread_mutex_unlock(&audio_hw_mutex);
        return;
    }

    if (aec_handle >= 0) {
        gadi_audio_ai_aec_disable();
        ap_aec_unregister(aec_handle);
        aec_handle = -1;
    }

    gadi_audio_ao_disable();
    gadi_audio_ai_disable();
    gadi_audio_exit();
    disable_audio_chip();
    audio_started = 0;
    log_msg("stopped");
    pthread_mutex_unlock(&audio_hw_mutex);
}

int audio_hw_get_frame(unsigned char* buffer, size_t buffer_size) {
    GADI_AUDIO_AioFrameT ai_frame;
    GADI_AEC_AioFrameT aec_frame;
    GADI_ERR ret;
    size_t copy_len;

    if (!buffer || buffer_size == 0 || !audio_started) {
        return -1;
    }

    ret = gadi_audio_ai_get_frame_aec(&ai_frame, &aec_frame, GADI_TRUE);
    if (ret != GADI_OK) {
        printf("%s: get AI frame failed: %d\n", THIS_FILE, ret);
        return -1;
    }

    copy_len = ai_frame.len < buffer_size ? ai_frame.len : buffer_size;
    memcpy(buffer, ai_frame.virAddr, copy_len);
    return (int)copy_len;
}

int audio_hw_send_frame(const unsigned char* buffer, size_t len) {
    GADI_AUDIO_AioFrameT ao_frame;
    GADI_ERR ret;

    if (!buffer || len == 0 || !audio_started) {
        return -1;
    }

    /* Zero the whole frame: it is an input to the (proprietary) AO driver, so any
     * fields beyond virAddr/len must be defined rather than left as stack garbage. */
    memset(&ao_frame, 0, sizeof(ao_frame));
    ao_frame.virAddr = (GADI_U8*)buffer;
    ao_frame.len = (GADI_U32)len;

    ret = gadi_audio_ao_send_frame_aec(&ao_frame, GADI_TRUE);
    if (ret != GADI_OK) {
        printf("%s: send AO frame failed: %d\n", THIS_FILE, ret);
        return -1;
    }

    return 0;
}

int audio_hw_frame_size(void) {
    return frame_size;
}
