#pragma once

#include <stdint.h>
#include "kfifo.h"

/**
 * @brief 初始化i2s外设和i2s dma，立即启动dma工作
 * 
 */
void CodecI2s_Init(void);

/**
 * @brief 关闭i2s外设和i2s dma
 * 
 */
void CodecI2s_DeInit(void);


/**
 * @brief 初始化dma缓冲区写指针到缓冲区中间
 * 
 */
void CodecI2s_Start(void);

/**
 * @brief 将dma缓冲区填满0从而静音
 * 
 */
void CodecI2s_Stop(void);

/**
 * @brief 尝试写数据到dma缓冲区中
 * 
 * @param ptr 
 * @param len 
 * @return uint32_t 写入的采样数
 */
uint32_t CodecI2s_WriteUACBuffer(struct StereoSample* src, uint32_t len);

/**
 * @brief 尝试写数据到dma缓冲区中
 * 
 * @param ptr 
 * @param len 
 */
void CodecI2s_WriteUACBufferNocheck(struct StereoSample* src, uint32_t len);

/**
 * @brief 从写位置填充一些0
 * 
 * @param len 
 */
void CodecI2s_FillZero(uint32_t len);

/**
 * @brief 如果dma缓冲区可写空间过大将填充0
 * 
 */
void CodecI2s_FillZeroIfTooSmallData();

/**
 * @brief 获取dma缓冲区中可能的空闲空间
 * 
 * @return uint32_t 
 */
uint32_t CodecI2s_GetFreeSpace();

/**
 * @brief 调节i2s的时钟分频来设置采样率
 * 
 * @param sample_rate 
 *          - 192000
 *          - 96000
 *          - 48000
 */
void CodecI2s_SetSampleRate(uint32_t sample_rate);

/**
 * @brief 判断dma缓冲区中的数据是少了还是多了
 * 
 * @return int32_t 
 *          - -1 少了
 *          -  1 多了
 *          -  0 刚刚好
 */
int32_t CodecI2s_GetCurrentSizeDiffFromCenter();
