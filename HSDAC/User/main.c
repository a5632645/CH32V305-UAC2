#include "ch32v30x_misc.h"
#include "debug.h"
#include "tick.h"
#include "usb/usb_impl.h"
#include "usbd.h"
#include "codec.h"
#include "audio_dsp.h"
#include "codec_i2s.h"
#include "uac_param.h"

static uint32_t feedback_fs_;

static void updateFeedback(void) {
    // 1ms 节流：从 CodecI2s 取反馈参数，按水位方向选采样率并设置 USB 反馈
    static uint32_t last_fb_tick;
    uint32_t now = Tick_GetTick();
    if (now - last_fb_tick >= 1) {
        last_fb_tick = now;
        const struct UacFeedbackRate* rate = Codec_GetFeedbackRate();
        int32_t diff = CodecI2s_GetCurrentSizeDiffFromCenter();
        uint32_t sr = (diff < 0) ? rate->ceil : (diff > 0) ? rate->floor : rate->normal;
        feedback_fs_ = sr;
        UsbImpl_SetFeedbackSr(sr);
    }
}

enum HidCommand {
    kHidCommand_Update = 0,
    kHidCommand_Save = 1
};

static void handleHid(void) {
    if (!UsbHid_HasData()) {
        return;
    }
    
    const uint32_t* hid_data = UsbHid_GetData();
    switch (hid_data[0]) {
        case kHidCommand_Update: {
            struct UacParam* param = Codec_GetUacParam();
            memcpy(&param->sr48k, hid_data + 1, sizeof(struct UacParam) - sizeof(uint32_t));
            UacParam_Clamp(param);   // 限制到合法范围
        }
            break;

        case kHidCommand_Save: {
            if (UacParam_Write(Codec_GetUacParam())) {
                printf("uac param saved\n\r");
            }
            else {
                printf("uac param save failed\n\r");
            }
        }
            break;
    }

    UsbHid_BeginRecv();
}

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
        updateFeedback();
        handleHid();

        uint32_t now = Tick_GetTick();
        if (now - tick > 1000) {
            if (!inited) {
                printf("codec inited failed\n\r");
            }
            tick = now;

            printf("i2s dma free space[%ld/%d]    sr[%ld/%ld]\n\r",
                CodecI2s_GetFreeSpace(), I2S_DMA_BUFFER_SIZE,
                feedback_fs_, Codec_GetSampleRate()
            );
        }
    }
}
