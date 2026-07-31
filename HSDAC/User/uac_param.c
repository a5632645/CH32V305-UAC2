#include "uac_param.h"
#include <string.h>
#include "ch32v30x.h"
#include "ch32v30x_flash.h"

// ------------------------------------------------------------
// define
// ------------------------------------------------------------

#define UAC_PARAM_BASE 0x1F000u
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

bool UacParam_Write(const struct UacParam* param) {
    // 读回旧内容，仅覆盖参数结构体所在区域（其余 216 字节保持不变）
    __attribute__((aligned(4)))
    static uint8_t buf[UAC_PARAM_SIZE];
    memcpy(buf, (const void*)UAC_PARAM_BASE, sizeof(buf));
    memcpy(buf, param, sizeof(*param));
    ((struct UacParam*)buf)->magic = UAC_PARAM_MAGIC; // 保证 magic 一致

    // 快速模式整块编程：256B 对齐（内部自带解锁/上锁）
    FLASH_Status st = FLASH_ROM_ERASE(UAC_PARAM_BASE, UAC_PARAM_SIZE);
    if (st == FLASH_COMPLETE) {
        st = FLASH_ROM_WRITE(UAC_PARAM_BASE, (uint32_t*)buf, UAC_PARAM_SIZE);
    }
    return st == FLASH_COMPLETE;
}
