#include "codec.h"

#include <stdatomic.h>

#include "ch32v30x_i2c.h"
#include "codec_i2c.h"
#include "codec_i2s.h"
#include "ch32v30x_rcc.h"
#include "ch32v30x_gpio.h"
#include "config.h"
#include "debug.h"

// --------------------------------------------------------------------------------
// define
// --------------------------------------------------------------------------------

#define REG_CHANGE_VOLUME1 0x01
#define REG_CHANGE_VOLUME2 0x02

// --------------------------------------------------------------------------------
// type
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// variable
// --------------------------------------------------------------------------------

static uint32_t sample_rate_;
static uint32_t diff_sample_rate_;

static int16_t usb_volume_[2];
static uint8_t usb_mute_;
static uint32_t es9018_reg_changes_;
static void(*i2c_handler_)(bool failed);

// --------------------------------------------------------------------------------
// implement
// --------------------------------------------------------------------------------

static uint8_t GetRegVolume(uint8_t channel) {
    uint8_t vol = 0;
    if (usb_volume_[channel] == -32768 || (usb_mute_ & (1 << channel))) {
        vol = 0xff;
    }
    else {
        vol = (uint16_t)(-usb_volume_[channel]) >> 7;
    }
    return vol;
}

static void _I2cHandler_RegVolume1(bool failed) {
    if (!failed) {
        atomic_fetch_and(&es9018_reg_changes_, ~REG_CHANGE_VOLUME1);
    }
}

static void _I2cHandler_RegVolume2(bool failed) {
    if (!failed) {
        atomic_fetch_and(&es9018_reg_changes_, ~REG_CHANGE_VOLUME2);
    }
}

// --------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------

bool Codec_Init() {
    CodecI2c_Init();
    CodecI2s_Init();

    // PA1/PA5 -> RESETB
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef init;
    init.GPIO_Mode = GPIO_Mode_AIN;
    init.GPIO_Speed = GPIO_Speed_50MHz;
    init.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOA, &init);
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    init.GPIO_Speed = GPIO_Speed_50MHz;
    init.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOA, &init);

    GPIO_WriteBit(GPIOA, GPIO_Pin_1, Bit_RESET);
    Delay_Ms(100);
    GPIO_WriteBit(GPIOA, GPIO_Pin_1, Bit_SET);
    Delay_Ms(100);

    bool inited = true;
    inited = inited && (kCodecI2cError_Finish == CodecI2c_PollWrite(0x90, 0, 0xf1, 1000));
    Delay_Ms(100);
    inited = inited && (kCodecI2cError_Finish == CodecI2c_PollWrite(0x90, 0, 0xf0, 1000));
    Delay_Ms(100);
    inited = inited && (kCodecI2cError_Finish == CodecI2c_PollWrite(0x90, 1, 0b10000000, 1000));
    return inited;
}

void Codec_Start() {
    CodecI2s_Start();
}

void Codec_Stop() {
    CodecI2s_Stop();
}

uint32_t Codec_GetSampleRate() {
    return sample_rate_;
}

void Codec_SetSampleRate(uint32_t sample_rate) {
    sample_rate_ = sample_rate;
    switch (sample_rate) {
        case 96000:
            diff_sample_rate_ = 200;
            break;
        case 48000:
            diff_sample_rate_ = 100;
            break;
        case 192000:
            diff_sample_rate_ = 400;
            break;
    }
    CodecI2s_SetSampleRate(sample_rate);
}

void Codec_Mute(uint8_t channel, uint8_t mute) {
    if (mute) {
        switch (channel) {
            case 0:
                usb_mute_ |= 0b11;
                atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME1 | REG_CHANGE_VOLUME2);
                break;
            case 1:
                usb_mute_ |= 0b1;
                atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME1);
                break;
            case 2:
                usb_mute_ |= 0b10;
                atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME2);
                break;
        }
    }
    else {
        switch (channel) {
            case 0:
                usb_mute_ &= ~0b11;
                atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME1 | REG_CHANGE_VOLUME2);
                break;
            case 1:
                usb_mute_ &= ~0b1;
                atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME1);
                break;
            case 2:
                usb_mute_ &= ~0b10;
                atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME2);
                break;
        }
    }
}

uint8_t Codec_IsMute(uint8_t channel) {
    switch (channel) {
        case 0:
            return usb_mute_ & 0b11;
            break;
        case 1:
            return usb_mute_ & 0b1;
            break;
        case 2:
            return usb_mute_ & 0b10;
            break;
    }
    return 0;
}

void Codec_SetVolume(uint8_t channel, int16_t volume) {
    switch (channel) {
        case 0:
            usb_volume_[0] = volume;
            usb_volume_[1] = volume;
            atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME1 | REG_CHANGE_VOLUME2);
            break;
        case 1:
            usb_volume_[0] = volume;
            atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME1);
            break;
        case 2:
            usb_volume_[1] = volume;
            atomic_fetch_or(&es9018_reg_changes_, REG_CHANGE_VOLUME2);
            break;
    }
}

int16_t Codec_GetVolume(uint8_t channel) {
    switch (channel) {
        case 0:
        case 1:
            return usb_volume_[0];
            break;
        case 2:
            return usb_volume_[1];
            break;
    }
    return 0;
}

uint32_t Codec_GetFeedbackFs() {
    int32_t diff_state = CodecI2s_GetCurrentSizeDiffFromCenter();
    if (diff_state == -1) {
        return sample_rate_ + diff_sample_rate_;
    }
    else if (diff_state == 0) {
        return sample_rate_;
    }
    else {
        return sample_rate_ - diff_sample_rate_;
    }
}

void Codec_Handler() {
    enum CodecI2cError status = CodecI2c_CheckStatus();
    if (status == kCodecI2cError_Busy) return;

    if (status == kCodecI2cError_Timeout) {
        printf("es9018k reg set timeout\n\r");

        if (i2c_handler_) {
            i2c_handler_(true);
        }
    }
    else if (status == kCodecI2cError_Finish) {
        if (i2c_handler_) {
            i2c_handler_(false);
        }
    }
    CodecI2c_SetStatusIdle();

    uint32_t mask = atomic_load(&es9018_reg_changes_);
    if (mask & REG_CHANGE_VOLUME1) {
        i2c_handler_ = _I2cHandler_RegVolume1;
        CodecI2c_WriteInterrupt(0x90, 15, GetRegVolume(0), 1000);
    }
    else if (mask & REG_CHANGE_VOLUME2) {
        i2c_handler_ = _I2cHandler_RegVolume2;
        CodecI2c_WriteInterrupt(0x90, 16, GetRegVolume(1), 1000);
    }
}
