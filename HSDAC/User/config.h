#pragma once

// i2s dma缓冲区大小，延迟大概是1/3 ~ 2/3
#define I2S_DMA_BUFFER_SIZE 256

// 如果有这么多的可写空间会尝试填充0到缓冲区中间
#define I2S_DMA_FILL_ZERO_THRESHOULD (I2S_DMA_BUFFER_SIZE * 5 / 6)

// USB端点的缓冲区大小/端点大小, 必须大于等于slowWidth*最大采样率/高速usb微帧速度
#define UAC_MAX_PACKAGE_SIZE (192000 / 8000)

// 采样率见 usb/usb_desc.cpp kUac2SampleRateTable变量 codec_i2s.c采样率处理相关

// 位数见 usb/usb_desc.cpp kConfigHs流接口
