#include "ch32v30x_misc.h"
#include "debug.h"
#include "tick.h"
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
    for (;;) {
        Codec_Handler();
        AudioDsp_Handler();

        uint32_t now = Tick_GetTick();
        if (now - tick > 1000) {
            if (!inited) {
                printf("codec inited failed\n\r");
            }
            tick = now;

            printf("codec dma free space %ld\n\r", CodecI2s_GetFreeSpace());
        }
    }

    return 0;
}
