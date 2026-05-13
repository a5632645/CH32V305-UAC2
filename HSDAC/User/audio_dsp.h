#pragma once

#include <stdint.h>
#include "config.h"
#include "kfifo.h"

#define AUDIO_DSP_BUFFER_SIZE 512

struct AudioDsp {
    struct Kfifo fifo;
    struct StereoSample buffer[AUDIO_DSP_BUFFER_SIZE];
};

void AudioDsp_Push(const struct StereoSample* src, uint32_t bytes);
void AudioDsp_Handler();
