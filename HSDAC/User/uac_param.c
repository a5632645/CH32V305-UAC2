#include "uac_param.h"
#include <string.h>
#include "ch32v30x.h"
#include "ch32v30x_flash.h"

// ------------------------------------------------------------
// define
// ------------------------------------------------------------

// 参数区位于最后一个 4K 扇区（物理 0x1F000）。FLASH_ROM_ERASE/FLASH_ROM_WRITE
// 要求地址在 [FLASH_BASE=0x08000000, ...) 的 alias 区域，因此用 FLASH_BASE + 0x1F000。
#define UAC_PARAM_BASE (FLASH_BASE + 0x1F000u)
#define UAC_PARAM_SIZE 256u

#define UAC_PARAM_MAGIC 0x53484344u

// ------------------------------------------------------------
// private
// ------------------------------------------------------------

static const struct UacParam kDefault = {
    .magic = UAC_PARAM_MAGIC,
    .sr48k = {
        48080,   // ceil
        48070,   // normal
        48060,   // floor
    },
    .sr96k = {
        48080 * 2,   // ceil
        48070 * 2,   // normal
        48060 * 2,   // floor
    },
    .sr192k = {
        48080 * 4,   // ceil
        48070 * 4,   // normal
        48060 * 4,   // floor
    },
};

void getDefaults(struct UacParam* param) {
    memcpy(param, &kDefault, sizeof(struct UacParam));
}

bool isValid(void) {
    return *(volatile uint32_t*)UAC_PARAM_BASE == UAC_PARAM_MAGIC;
}

// ----------------------------------------
// public
// ----------------------------------------

void UacParam_Read(struct UacParam* param) {
    memcpy(param, (const void*)UAC_PARAM_BASE, sizeof(struct UacParam));
}

void UacParam_Init(struct UacParam* param) {
    if (!isValid()) {
        getDefaults(param);
        UacParam_Write(param);
    }
    else {
        UacParam_Read(param);
    }
}

// 各采样率档位标称值（±0.5% 容差）
#define UAC_SR_48K   48000u
#define UAC_SR_96K   96000u
#define UAC_SR_192K  192000u
#define UAC_SR_TOLERANCE_PER_MILLE 5u   // 0.5% (5/1000)

static uint32_t ClampRate(uint32_t v, uint32_t nominal) {
    uint32_t lo = nominal * (1000u - UAC_SR_TOLERANCE_PER_MILLE) / 1000u;
    uint32_t hi = nominal * (1000u + UAC_SR_TOLERANCE_PER_MILLE) / 1000u;
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void UacParam_Clamp(struct UacParam* param) {
    param->sr48k.ceil    = ClampRate(param->sr48k.ceil, UAC_SR_48K);
    param->sr48k.normal  = ClampRate(param->sr48k.normal, UAC_SR_48K);
    param->sr48k.floor   = ClampRate(param->sr48k.floor, UAC_SR_48K);
    param->sr96k.ceil    = ClampRate(param->sr96k.ceil, UAC_SR_96K);
    param->sr96k.normal  = ClampRate(param->sr96k.normal, UAC_SR_96K);
    param->sr96k.floor   = ClampRate(param->sr96k.floor, UAC_SR_96K);
    param->sr192k.ceil   = ClampRate(param->sr192k.ceil, UAC_SR_192K);
    param->sr192k.normal = ClampRate(param->sr192k.normal, UAC_SR_192K);
    param->sr192k.floor  = ClampRate(param->sr192k.floor, UAC_SR_192K);
}

bool UacParam_Write(const struct UacParam* param) {
    // 读回旧内容，仅覆盖参数结构体所在区域（其余 216 字节保持不变）
    __attribute__((aligned(4)))
    static uint8_t buf[UAC_PARAM_SIZE];
    memcpy(buf, (const void*)UAC_PARAM_BASE, sizeof(buf));
    memcpy(buf, param, sizeof(*param));
    UacParam_Clamp((struct UacParam*)buf);          // 写 flash 前限制到合法范围
    ((struct UacParam*)buf)->magic = UAC_PARAM_MAGIC; // 保证 magic 一致

    // 快速模式整块编程：256B 对齐
    FLASH_Status st = FLASH_ROM_ERASE(UAC_PARAM_BASE, UAC_PARAM_SIZE);
    if (st == FLASH_COMPLETE) {
        st = FLASH_ROM_WRITE(UAC_PARAM_BASE, (uint32_t*)buf, UAC_PARAM_SIZE);
    }
    return st == FLASH_COMPLETE;
}
