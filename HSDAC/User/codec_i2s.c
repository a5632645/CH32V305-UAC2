#include "codec_i2s.h"

#include <string.h>
#include <stdatomic.h>

#include "ch32v30x_gpio.h"
#include "ch32v30x_rcc.h"
#include "ch32v30x_spi.h"
#include "ch32v30x_dma.h"
#include "ch32v30x_misc.h"
#include "audio_dsp.h"
#include "config.h"

// --------------------------------------------------------------------------------
// marco
// --------------------------------------------------------------------------------

#define I2S_DMA_BUFFER_SIZE_MASK (I2S_DMA_BUFFER_SIZE - 1)

// --------------------------------------------------------------------------------
// type
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// variable
// --------------------------------------------------------------------------------

static struct StereoSample i2s_dma_buffer_[I2S_DMA_BUFFER_SIZE] = {0};
static uint32_t last_i2s_dma_wpos_ = 0;

// --------------------------------------------------------------------------------
// declare
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// implement
// --------------------------------------------------------------------------------

static void DMA_Tx_Init(DMA_Channel_TypeDef* DMA_CHx, u32 ppadr, u32 memadr, u16 bufsize) {
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA_CHx);

    DMA_InitTypeDef DMA_InitStructure={0};
    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = bufsize;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);
}

static void I2S2_Init(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);
    SPI_I2S_DeInit(SPI2);

    GPIO_InitTypeDef GPIO_InitStructure={0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    I2S_InitTypeDef I2S_InitStructure={0};
    I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;
    I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;
    I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_32b;
    I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Enable;
    I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq_48k;
    I2S_InitStructure.I2S_CPOL = I2S_CPOL_High;
    I2S_Init(SPI2, &I2S_InitStructure);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
    I2S_Cmd(SPI2, ENABLE);
}

static int32_t Swap16(int32_t x) {
    uint32_t r = x;
    uint32_t up = r & 0xffff;
    uint32_t down = r >> 16;
    return down | (up << 16);
}

static uint32_t GetDmaReadPos() {
    uint32_t a = (I2S_DMA_BUFFER_SIZE * 4 - DMA_GetCurrDataCounter(DMA1_Channel5)) / 4;
    return a & I2S_DMA_BUFFER_SIZE_MASK;
}

static uint32_t GetDmaCanWrite() {
    uint32_t can_write = GetDmaReadPos() - last_i2s_dma_wpos_;
    return can_write & I2S_DMA_BUFFER_SIZE_MASK;
}

static uint32_t GetDmaSamplesHave() {
    return I2S_DMA_BUFFER_SIZE - GetDmaCanWrite();
}

static void I2S2_PrescaleConfig(uint32_t v) {
    uint16_t odd = (v & 1) << 8;
    uint16_t div = v >> 1;
    uint16_t mlck = 1 << 9;
    SPI2->I2SPR = odd | div | mlck;
}

// --------------------------------------------------------------------------------
// public
// --------------------------------------------------------------------------------

void CodecI2s_Init(void) {
    I2S2_Init();
    DMA_Tx_Init(DMA1_Channel5, (u32)&SPI2->DATAR, (u32)&i2s_dma_buffer_, I2S_DMA_BUFFER_SIZE * sizeof(struct StereoSample) / sizeof(uint16_t));
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

void CodecI2s_DeInit(void) {
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA_DeInit(DMA1_Channel5);
    I2S_Cmd(SPI2, DISABLE);
    SPI_I2S_DeInit(SPI2);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, DISABLE);
}

void CodecI2s_Start(void) {
    // memset(i2s_dma_buffer_, 0, sizeof(i2s_dma_buffer_));
    // last_i2s_dma_wpos_ = (GetDmaReadPos() + I2S_DMA_BUFFER_SIZE / 2) & I2S_DMA_BUFFER_SIZE_MASK;
}

void CodecI2s_Stop(void) {
    // memset(i2s_dma_buffer_, 0, sizeof(i2s_dma_buffer_));
}

uint32_t CodecI2s_WriteUACBuffer(struct StereoSample* src, uint32_t len) {
    uint32_t can_write = GetDmaCanWrite();
    can_write = len < can_write ? len : can_write;

    uint32_t fill1 = I2S_DMA_BUFFER_SIZE - last_i2s_dma_wpos_;
    fill1 = fill1 < can_write ? fill1 : can_write;
    uint32_t fill2 = can_write - fill1;

    struct StereoSample* dst = &i2s_dma_buffer_[last_i2s_dma_wpos_];
    while (fill1--) {
        dst->left = Swap16(src->left);
        dst->right = Swap16(src->right);
        ++src;
        ++dst;
    }

    dst = &i2s_dma_buffer_[0];
    while (fill2--) {
        dst->left = Swap16(src->left);
        dst->right = Swap16(src->right);
        ++src;
        ++dst;
    }

    last_i2s_dma_wpos_ += can_write;
    last_i2s_dma_wpos_ &= I2S_DMA_BUFFER_SIZE_MASK;

    return can_write;
}

void CodecI2s_WriteUACBufferNocheck(struct StereoSample* src, uint32_t len) {
    uint32_t fill1 = I2S_DMA_BUFFER_SIZE - last_i2s_dma_wpos_;
    fill1 = fill1 < len ? fill1 : len;
    uint32_t fill2 = len - fill1;

    struct StereoSample* dst = &i2s_dma_buffer_[last_i2s_dma_wpos_];
    while (fill1--) {
        dst->left = Swap16(src->left);
        dst->right = Swap16(src->right);
        ++src;
        ++dst;
    }

    dst = &i2s_dma_buffer_[0];
    while (fill2--) {
        dst->left = Swap16(src->left);
        dst->right = Swap16(src->right);
        ++src;
        ++dst;
    }

    last_i2s_dma_wpos_ += len;
    last_i2s_dma_wpos_ &= I2S_DMA_BUFFER_SIZE_MASK;
}

void CodecI2s_FillZero(uint32_t len) {
    if (len == 0) return;

    uint32_t fill1 = I2S_DMA_BUFFER_SIZE - last_i2s_dma_wpos_;
    fill1 = fill1 < len ? fill1 : len;
    uint32_t fill2 = len - fill1;

    struct StereoSample* dst = &i2s_dma_buffer_[last_i2s_dma_wpos_];
    while (fill1--) {
        dst->left = 0;
        dst->right = 0;
        ++dst;
    }

    dst = &i2s_dma_buffer_[0];
    while (fill2--) {
        dst->left = 0;
        dst->right = 0;
        ++dst;
    }

    last_i2s_dma_wpos_ += len;
    last_i2s_dma_wpos_ &= I2S_DMA_BUFFER_SIZE_MASK;
}

void CodecI2s_FillZeroIfTooSmallData() {
    uint32_t can_write = GetDmaCanWrite();
    if (can_write > I2S_DMA_FILL_ZERO_THRESHOULD) {
        uint32_t fill_zero = can_write - I2S_DMA_BUFFER_SIZE / 2;
        CodecI2s_FillZero(fill_zero);
    }
}

uint32_t CodecI2s_GetFreeSpace() {
    return GetDmaCanWrite();
}

void CodecI2s_SetSampleRate(uint32_t sample_rate) {
    switch (sample_rate) {
    case 96000:
        I2S2_PrescaleConfig(6);
        break;
    case 192000:
        I2S2_PrescaleConfig(3);
        break;
    case 48000:
    default:
        I2S2_PrescaleConfig(12);
        break;
    }
}

int32_t CodecI2s_GetCurrentSizeDiffFromCenter() {
    uint32_t capacity_1_3 = I2S_DMA_BUFFER_SIZE / 3;
    uint32_t capacity_2_3 = I2S_DMA_BUFFER_SIZE * 2 / 3;
    uint32_t size = GetDmaSamplesHave();
    if (size < capacity_1_3) return -1;
    else if (size > capacity_2_3) return 1;
    else return 0;
}
