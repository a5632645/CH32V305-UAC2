#include "codec_i2c.h"

#include <stddef.h>

#include "ch32v30x_i2c.h"
#include "ch32v30x_rcc.h"
#include "ch32v30x_gpio.h"
#include "ch32v30x_misc.h"
#include "tick.h"

// ----------------------------------------
// type
// ----------------------------------------
struct CodecI2cState {
    uint8_t address;
    uint8_t reg;
    uint8_t value;

    uint32_t start_tick;
    uint32_t timeout_ticks;

    uint8_t busy;
    // see @enum CodecI2cError
    uint8_t status;
    uint8_t send_status;
};

// ----------------------------------------
// variable
// ----------------------------------------
static struct CodecI2cState state_ = {0};

// ----------------------------------------
// declare
// ----------------------------------------
static void CodecI2c_DisableInterrupt();
static void CodecI2c_EnableInterrupt();

// ----------------------------------------
// implement
// ----------------------------------------
static void CodecI2c_DisableInterrupt() {
    NVIC_DisableIRQ(I2C2_ER_IRQn);
    NVIC_DisableIRQ(I2C2_EV_IRQn);
}

static void CodecI2c_EnableInterrupt() {
    I2C_ClearITPendingBit(I2C2, I2C_IT_AF | I2C_IT_BERR);
    I2C_ITConfig(I2C2, I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);
    NVIC_EnableIRQ(I2C2_ER_IRQn);
    NVIC_EnableIRQ(I2C2_EV_IRQn);
}

__attribute__((interrupt("WCH-Interrupt-fast"), used))
void I2C2_EV_IRQHandler(void) {
    if (I2C_GetITStatus(I2C2, I2C_IT_SB)) {
        I2C_Send7bitAddress(I2C2, state_.address, I2C_Direction_Transmitter);
    }
    if (I2C_GetITStatus(I2C2, I2C_IT_ADDR)) {
        (void)I2C2->STAR2;
        I2C_SendData(I2C2, state_.reg);
    }
    if (I2C_GetITStatus(I2C2, I2C_IT_TXE) && state_.send_status == 0) {
        I2C_SendData(I2C2, state_.value);
        state_.send_status = 1;
    }
    if (I2C_GetITStatus(I2C2, I2C_IT_TXE) && state_.send_status == 1) {
        I2C_GenerateSTOP(I2C2, ENABLE);
        state_.busy = 0;
        state_.status = kCodecI2cError_Finish;
        state_.send_status = 0;
    }
}

__attribute__((interrupt("WCH-Interrupt-fast"), used))
void I2C2_ER_IRQHandler(void) {
    I2C_ClearITPendingBit(I2C2, I2C_IT_AF);
    I2C_ClearITPendingBit(I2C2, I2C_IT_BERR);
    I2C_GenerateSTOP(I2C2, ENABLE);
    state_.busy = 0;
    state_.status = kCodecI2cError_Timeout;
}

// ----------------------------------------
// public
// ----------------------------------------
void CodecI2c_Init() {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    I2C_DeInit(I2C2);

    GPIO_InitTypeDef init;
    init.GPIO_Mode = GPIO_Mode_AF_OD;
    init.GPIO_Speed = GPIO_Speed_50MHz;
    init.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &init);

    I2C_InitTypeDef i2c_init = {
        .I2C_ClockSpeed = 400000,
        .I2C_Mode = I2C_Mode_I2C,
        .I2C_DutyCycle = I2C_DutyCycle_2,
        .I2C_Ack = I2C_Ack_Enable,
        .I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit,
    };
    I2C_Init(I2C2, &i2c_init);
    I2C_Cmd(I2C2, ENABLE);

    NVIC_InitTypeDef nvic = {
        .NVIC_IRQChannel = I2C2_ER_IRQn,
        .NVIC_IRQChannelCmd = DISABLE,
        .NVIC_IRQChannelPreemptionPriority = 2,
        .NVIC_IRQChannelSubPriority = 0
    };
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = I2C2_EV_IRQn;
    NVIC_Init(&nvic);
}

void CodecI2c_Deinit() {
    I2C_DeInit(I2C2);
}

#define QUIT_IF_TIMEOUT()\
    if (Tick_GetTick() - start_tick > timeout_ticks) {\
        I2C_GenerateSTOP(I2C2, ENABLE);\
        return kCodecI2cError_Timeout;\
    }

enum CodecI2cError CodecI2c_PollWrite(
    uint8_t address,
    uint8_t reg,
    uint8_t value,
    uint32_t timeout_ticks
) {
    CodecI2c_DisableInterrupt();

    uint32_t start_tick = Tick_GetTick();
    while (I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) != RESET) {
        QUIT_IF_TIMEOUT();
    }

    I2C_GenerateSTART(I2C2, ENABLE);
    while (!I2C_CheckEvent (I2C2, I2C_EVENT_MASTER_MODE_SELECT)) {
        QUIT_IF_TIMEOUT();
    }

    I2C_Send7bitAddress(I2C2, address, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent (I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        QUIT_IF_TIMEOUT();
    }
    while (I2C_GetFlagStatus (I2C2, I2C_FLAG_TXE) == RESET) {
        QUIT_IF_TIMEOUT();
    }

    I2C_SendData(I2C2, reg);
    while (I2C_GetFlagStatus (I2C2, I2C_FLAG_TXE) == RESET) {
        QUIT_IF_TIMEOUT();
    }

    I2C_SendData(I2C2, value);
    while (I2C_GetFlagStatus (I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == RESET) {
        QUIT_IF_TIMEOUT();
    }

    I2C_GenerateSTOP (I2C2, ENABLE);
    return kCodecI2cError_Finish;
}

enum CodecI2cError CodecI2c_PollRead(
    uint8_t address,
    uint8_t reg,
    uint8_t* value,
    uint32_t timeout_ticks
) {
    CodecI2c_DisableInterrupt();

    uint32_t start_tick = Tick_GetTick();
    while (I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) != RESET) {
        QUIT_IF_TIMEOUT();
    }

    I2C_GenerateSTART(I2C2, ENABLE);
    while (I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT) == NoREADY) {
        QUIT_IF_TIMEOUT();
    }

    I2C_Send7bitAddress(I2C2, address, I2C_Direction_Transmitter);
    while (I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == NoREADY) {
        QUIT_IF_TIMEOUT();
    }
    while (I2C_GetFlagStatus(I2C2, I2C_FLAG_TXE) == RESET) {
        QUIT_IF_TIMEOUT();
    }

    I2C_SendData(I2C2, reg);
    while (I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == NoREADY) {
        QUIT_IF_TIMEOUT();
    }

    I2C_GenerateSTART(I2C2, ENABLE);
    while (I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT) == NoREADY) {
        QUIT_IF_TIMEOUT();
    }

    I2C_Send7bitAddress(I2C2, address, I2C_Direction_Receiver);
    while (I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) == NoREADY) {
        QUIT_IF_TIMEOUT();
    }

    while (I2C_GetFlagStatus(I2C2, I2C_FLAG_RXNE) == RESET) {
        QUIT_IF_TIMEOUT();
    }
    *value = I2C_ReceiveData(I2C2);

    I2C_GenerateSTOP(I2C2, ENABLE);
    return kCodecI2cError_Finish;
}

void CodecI2c_WriteInterrupt(
    uint8_t address,
    uint8_t reg,
    uint8_t value,
    uint32_t timeout_ticks
) {
    state_.address = address;
    state_.reg = reg;
    state_.value = value;
    state_.timeout_ticks = timeout_ticks;
    state_.status = kCodecI2cError_Busy;
    state_.send_status = 0;
    state_.start_tick = Tick_GetTick();
    CodecI2c_EnableInterrupt();
    I2C_GenerateSTART(I2C2, ENABLE);
}

void CodecI2c_ReadInterrupt(
    uint8_t address,
    uint8_t reg,
    uint32_t timeout_ticks
) {
    // qwqfixme
}

enum CodecI2cError CodecI2c_CheckStatus() {
    if (state_.status == kCodecI2cError_Busy) {
        uint32_t now = Tick_GetTick();
        if (now - state_.start_tick > state_.timeout_ticks) {
            state_.busy = 0;
            state_.status = kCodecI2cError_Timeout;

            CodecI2c_DisableInterrupt();
            I2C_ClearITPendingBit(I2C2, I2C_IT_AF | I2C_IT_BERR);
            I2C_GenerateSTOP(I2C2, ENABLE);
        }
    }

    return state_.status;
}

uint8_t CodecI2c_GetReadValue() {
    return state_.value;
}

void CodecI2c_SetStatusIdle() {
    state_.status = kCodecI2cError_Idle;
}
