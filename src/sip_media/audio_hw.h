#ifndef AUDIO_HW_H
#define AUDIO_HW_H

#include <stddef.h>

int audio_hw_start(int audio_chip_gpio, int frame_samples);
void audio_hw_stop(void);
int audio_hw_get_frame(unsigned char* buffer, size_t buffer_size);
int audio_hw_send_frame(const unsigned char* buffer, size_t len);
int audio_hw_frame_size(void);

#endif
