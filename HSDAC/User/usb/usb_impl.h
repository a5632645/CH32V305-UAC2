#pragma once

#include "usb_device.h"
#include "config.h"
#include "kfifo.h"

void UsbImpl_InitAndOpenEndpoints();

void UsbImpl_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase);
void UsbImpl_HandleVendorRequest(struct UsbDevice* device, bool* allow, bool setup_phase);
void UsbImpl_HandleSof();

void UsbImpl_SetInterfaceAlter(uint8_t interface, uint8_t alter, bool* allow);
uint8_t UsbImpl_GetInterfaceAlter(uint8_t interface, bool* allow);

void UsbImpl_StallEndpoint(uint8_t address);
void UsbImpl_ClearStallEndpoint(uint8_t address);

void UsbImpl_GetDescriptor(struct UsbDevice* device, bool* allow);

void UsbImpl_EpInComplete(uint8_t ep_num);
void UsbImpl_EpOutComplete(uint8_t ep_num, uint16_t count);

enum Uac2EntityId {
    kUac2EntityId_Clock = 3,
    kUac2EntityId_FeatureUnit = 4,
    kUac2EntityId_InputTerminal = 1,
    kUac2EntityId_OutputTerminal = 2,
};

#define UAC2_CONTROL_INTERFACE 0
#define UAC2_CONTROL_DATA_IN_ADDRESS 0x84

#define UAC2_STREAM_INTERFACE 1
#define UAC2_STREAM_DATA_OUT_EP_ADDRESS 0x01
#define UAC2_STREAM_DATA_OUT_EP_MPSIZE (UAC_MAX_PACKAGE_SIZE * sizeof(struct StereoSample))
#define UAC2_STREAM_FEEDBACK_IN_EP_ADDRESS 0x81
#define UAC2_STREAM_FEEDBACK_IN_EP_MPSIZE 4

#define CDC_CONTROL_INTERFACE 2
#define CDC_CONTROL_UPLOAD_IN_EP_ADDRESS 0x82
#define CDC_CONTROL_UPLOAD_IN_EP_MPSIZE 64

#define CDC_DATA_INTERFACE 3
#define CDC_DATA_IN_EP_ADDRESS 0x83
#define CDC_DATA_IN_EP_MPSIZE 64
#define CDC_DATA_OUT_EP_ADDRESS 0x03
#define CDC_DATA_OUT_EP_MPSIZE 64

#define HID_DATA_INTERFACE 4
#define HID_DATA_OUT_EP_ADDRESS 0x04
#define HID_DATA_OUT_EP_MPSIZE 4

#define USBIMPL_INTERFACE_COUNT 5

void UsbCdc_Write(uint8_t const* buffer, uint32_t bytes);
bool UsbCdc_CanWrite();
