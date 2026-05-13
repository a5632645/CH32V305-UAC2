#include "audio_dsp.h"
#include "codec_i2s.h"

static struct AudioDsp audio_dsp_ = {
    .fifo = {
        .wpos = 0,
        .rpos = 0,
        .mask = AUDIO_DSP_BUFFER_SIZE - 1
    }
};

#define AUDIO_DSP_MIN(a, b) ((a) > (b) ? (b) : (a))

// ----------------------------------------
// implement
// ----------------------------------------
static void ProcessBlock(struct StereoSample* ptr, uint32_t len) {
    (void)ptr;
    (void)len;
    // put your dsp code here
    // also have a HID interface to control
    // ...
}

// ----------------------------------------
// public
// ----------------------------------------
void AudioDsp_Push(const struct StereoSample* src, uint32_t bytes) {
    uint32_t count = bytes / sizeof(struct StereoSample);
    Kfifo_TryPush(&audio_dsp_.fifo, src, count);
}

void AudioDsp_Handler() {
    uint32_t free_space = CodecI2s_GetFreeSpace();
    uint32_t count = 0;
    struct StereoSample* ptr = Kfifo_ContinueReadBegin(&audio_dsp_.fifo, &count);
    uint32_t can_do = AUDIO_DSP_MIN(free_space, count);
    ProcessBlock(ptr, can_do);
    CodecI2s_WriteUACBufferNocheck(ptr, can_do);
    Kfifo_ContinueReadEnd(&audio_dsp_.fifo, can_do);

    free_space = CodecI2s_GetFreeSpace();
    if (Kfifo_Size(&audio_dsp_.fifo) < free_space) {
        CodecI2s_FillZeroIfTooSmallData();
    }
}
