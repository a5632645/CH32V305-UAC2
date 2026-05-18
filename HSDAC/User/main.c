#include "ch32v30x_misc.h"
#include "debug.h"
#include "tick.h"
#include "usb/usb_impl.h"
#include "usbd.h"
#include "codec.h"
#include "audio_dsp.h"
#include "codec_i2s.h"

int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();
    Tick_Init();
    Usbd_Init();
    bool inited = Codec_Init();

    Usbd_Connect();
    uint32_t tick = Tick_GetTick();
    uint32_t adjust_tick = Tick_GetTick();
    for (;;) {
        Codec_Handler();
        AudioDsp_Handler();

        uint32_t now = Tick_GetTick();
        if (now - tick > 1000) {
            if (!inited) {
                printf("codec inited failed\n\r");
            }
            tick = now;

            printf("i2s dma free space[%ld/%d]     i2s sr[%ld/%ld]\n\r",
                CodecI2s_GetFreeSpace(), I2S_DMA_BUFFER_SIZE,
                Codec_GetFeedbackFs(), Codec_GetSampleRate()
            );
        }

        if (now - adjust_tick > 10) {
            adjust_tick = now;
            Codec_AdjustFeedbackFs();
            UsbUac_SetFeedbackFs(Codec_GetFeedbackFs());
        }
    }

    return 0;
}
