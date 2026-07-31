#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "uac_param.h"

bool Codec_Init();
void Codec_Handler();

void Codec_Start();
void Codec_Stop();

uint32_t Codec_GetSampleRate();
void     Codec_SetSampleRate(uint32_t sample_rate);

void    Codec_Mute(uint8_t channel, uint8_t mute);
uint8_t Codec_IsMute(uint8_t channel);
void    Codec_SetVolume(uint8_t channel, int16_t volume);
int16_t Codec_GetVolume(uint8_t channel);

// 获取当前采样率的 UAC 反馈速率参数（ceil/floor/normal）。
const struct UacFeedbackRate* Codec_GetFeedbackRate(void);

// HID 校准写入后刷新反馈参数缓存（从 flash 重新读入）。
void Codec_RefreshFeedbackParam(void);

// 获取当前 UacParam 缓存（flash 校准表）。
struct UacParam* Codec_GetUacParam(void);
