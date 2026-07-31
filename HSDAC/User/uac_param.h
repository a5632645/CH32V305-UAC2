#pragma once
#include <stdint.h>
#include <stdbool.h>

struct UacFeedbackRate {
    uint32_t ceil;
    uint32_t normal;
    uint32_t floor;
};

struct UacParam {
    uint32_t magic;
    struct UacFeedbackRate sr48k;   // 48kHz
    struct UacFeedbackRate sr96k;   // 96kHz
    struct UacFeedbackRate sr192k;  // 192kHz
};

// 初始化参数区：无 MAGIC 时写入默认值。
void UacParam_Init(struct UacParam* param);

// 整块读取参数区（flash -> param）。
void UacParam_Read(struct UacParam* param);

// 整块写入结构体（自动保证 magic 一致）。
bool UacParam_Write(const struct UacParam* param);

// 将各采样率校准值限制到合法范围（标称 ±10%）。
void UacParam_Clamp(struct UacParam* param);
