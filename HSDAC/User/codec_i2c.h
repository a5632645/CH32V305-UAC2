#pragma once

#include <stdint.h>
#include <stdbool.h>

enum CodecI2cError {
    kCodecI2cError_Idle,
    kCodecI2cError_Timeout,
    kCodecI2cError_Finish,
    kCodecI2cError_Busy
};

void CodecI2c_Init();
void CodecI2c_Deinit();

enum CodecI2cError CodecI2c_PollWrite(
    uint8_t address,
    uint8_t reg,
    uint8_t value,
    uint32_t timeout_ticks
);
enum CodecI2cError CodecI2c_PollRead(
    uint8_t address,
    uint8_t reg,
    uint8_t* value,
    uint32_t timeout_ticks
);

void CodecI2c_WriteInterrupt(
    uint8_t address,
    uint8_t reg,
    uint8_t value,
    uint32_t timeout_ticks
);
void CodecI2c_ReadInterrupt(
    uint8_t address,
    uint8_t reg,
    uint32_t timeout_ticks
);
enum CodecI2cError CodecI2c_CheckStatus();
uint8_t CodecI2c_GetReadValue();
void CodecI2c_SetStatusIdle();
