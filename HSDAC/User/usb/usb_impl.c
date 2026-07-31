#include "usb_impl.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "usb_desc.h"
#include "usb_hardware.h"
#include "codec.h"
#include "tick.h"
#include "config.h"
#include "audio_dsp.h"

// ----------------------------------------
// define
// ----------------------------------------

// ----------------------------------------
// typedef
// ----------------------------------------
struct InterruptDataMessage {
    struct {
        uint8_t is_vendor : 1;
        uint8_t is_endpoint : 1;
        uint8_t _ : 6;
    } bInfo;
    uint8_t bAttribute;
    uint8_t channel;
    uint8_t entity_parameter;
    uint8_t interface_no;
    uint8_t entity_id;
};

// --------------------------------------------------------------------------------
// variable
// --------------------------------------------------------------------------------
static uint8_t interface_alters[USBIMPL_INTERFACE_COUNT];

static struct UsbEndpoint cdc_data_tx_ep;

__attribute__((aligned(4)))
static uint8_t cdc_data_rx_buffer[CDC_DATA_OUT_EP_MPSIZE];
static struct {
    uint32_t baudrate;
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
} cdc_line_coding = {
    115200, 1, 0, 8, 1
};
static bool cdc_can_send;

static struct StereoSample uac2_audio_buffer_[UAC_MAX_PACKAGE_SIZE];
static uint32_t uac2_feedback_val;

static uint8_t hid_idle;

// 固定 64 字节 HID 报告缓冲区（阻塞式：端点收到一包后 NAK，
// 主循环 UsbHid_GetData() 取走数据后才恢复 ACK）
__attribute__((aligned(4)))
static uint8_t hid_rx_buffer[HID_RX_BUFFER_SIZE];
static volatile uint32_t hid_rx_len;
static volatile bool hid_rx_pending;

// --------------------------------------------------------------------------------
// declare
// --------------------------------------------------------------------------------
static void UsbUac2_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase);
static void UsbCdc_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase);
static void UsbHid_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase);
static uint32_t ConvertSamplerate2FeedbackRate(uint32_t fs);

// --------------------------------------------------------------------------------
// implement
// --------------------------------------------------------------------------------
static uint32_t ConvertSamplerate2FeedbackRate(uint32_t fs) {
    uint32_t intergal = fs / 8000;
    uint32_t frac = (fs % 8000) * 65536 / 8000;
    uint32_t rate = (intergal << 16) | (frac & 0xffff);
    return rate;
}

void UsbImpl_InitAndOpenEndpoints() {
    USBHSD->ENDP_CONFIG |= USBHS_UEP1_T_EN | USBHS_UEP1_R_EN
                         | USBHS_UEP2_T_EN
                         | USBHS_UEP3_T_EN | USBHS_UEP3_R_EN
                         | USBHS_UEP4_R_EN;

    USBHSD->ENDP_TYPE = USBHS_UEP1_T_TYPE | USBHS_UEP1_R_TYPE;

    USBHSD->UEP1_TX_DMA = (uint32_t)&uac2_feedback_val;
    USBHSD->UEP1_TX_LEN = 4;
    USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_RES_ACK;

    USBHSD->UEP1_MAX_LEN = UAC2_STREAM_DATA_OUT_EP_MPSIZE;
    USBHSD->UEP1_RX_DMA = (uint32_t)uac2_audio_buffer_;
    USBHSD->UEP1_RX_CTRL = USBHS_UEP_R_RES_ACK;

    USBHSD->UEP2_TX_DMA = 0;
    USBHSD->UEP2_TX_CTRL = USBHS_UEP_T_RES_NAK;

    USBHSD->UEP3_TX_DMA = 0;
    USBHSD->UEP3_TX_CTRL = USBHS_UEP_T_RES_NAK;
    cdc_data_tx_ep.attribute.busy = 0;
    cdc_data_tx_ep.attribute.stall = 0;
    cdc_data_tx_ep.attribute.type = kUsbEndpointType_Bulk;
    cdc_data_tx_ep.attribute.zero_package = 0;
    cdc_data_tx_ep.attribute.address = CDC_DATA_IN_EP_ADDRESS;
    cdc_data_tx_ep.max_packet_size = CDC_DATA_IN_EP_MPSIZE;
    cdc_data_tx_ep.transfer_buffer = NULL;
    cdc_data_tx_ep.transfer_remain = 0;
    cdc_can_send = false;

    USBHSD->UEP3_MAX_LEN = CDC_DATA_OUT_EP_MPSIZE;
    USBHSD->UEP3_RX_DMA = (uint32_t)cdc_data_rx_buffer;
    USBHSD->UEP3_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;

    USBHSD->UEP4_MAX_LEN = HID_DATA_OUT_EP_MPSIZE;
    USBHSD->UEP4_RX_DMA = (uint32_t)hid_rx_buffer;
    USBHSD->UEP4_RX_CTRL = USBHS_UEP_R_RES_ACK;
    hid_rx_len = 0;
    hid_rx_pending = false;
}

void UsbImpl_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase) {
    switch (device->setup_request.wIndex & 0xff) {
        case UAC2_CONTROL_INTERFACE:
            UsbUac2_HandleClassRequest(device, allow, setup_phase);
            break;
        case UAC2_STREAM_INTERFACE:
            break;
        case CDC_CONTROL_INTERFACE:
            UsbCdc_HandleClassRequest(device, allow, setup_phase);
            break;
        case CDC_DATA_INTERFACE:
            break;
        case HID_DATA_INTERFACE:
            UsbHid_HandleClassRequest(device, allow, setup_phase);
            break;
    }
}

void UsbImpl_HandleVendorRequest(struct UsbDevice* device, bool* allow, bool setup_phase) {
    (void)device;
    (void)allow;
    (void)setup_phase;
}

void UsbImpl_HandleSof() {

}

void UsbImpl_SetInterfaceAlter(uint8_t interface, uint8_t alter, bool* allow) {
    *allow = interface < USBIMPL_INTERFACE_COUNT;
    if (*allow) {
        interface_alters[interface] = alter;

        switch (interface) {
            case UAC2_STREAM_INTERFACE:
                if (alter == 0) {
                    Codec_Stop();
                }
                else {
                    Codec_Start();
                }
                break;
        }
    }
}

uint8_t UsbImpl_GetInterfaceAlter(uint8_t interface, bool* allow) {
    *allow = interface < USBIMPL_INTERFACE_COUNT;
    if (*allow) {
        return interface_alters[interface];
    }
    return 0;
}

void UsbImpl_GetDescriptor(struct UsbDevice* device, bool* allow) {
    uint8_t type = device->setup_request.wValue >> 8;
    uint8_t index = device->setup_request.wValue & 0xff;
    uint16_t hid_interface_id = device->setup_request.wIndex;

    uint8_t const* desc = NULL;
    uint16_t len = 0;
    switch (type) {
        // device
        case 1:
            desc = UsbDesc_Device(&len);
            break;
        // configuration
        case 2:
            if ((USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK) == USBHS_SPEED_HIGH) {
                desc = UsbDesc_ConfigHs(&len);
            }
            else {
                desc = UsbDesc_ConfigFs(&len);
            }
            break;
        // string
        case 3:
            desc = UsbDesc_String(index, &len);
            break;
        // hid
        case 0x22:
            desc = UsbDesc_Hid(hid_interface_id, &len);
            break;
        // device qualifier
        case 6:
            desc = UsbDesc_Qualifier(&len);
            break;
        // bos, usb2.0 does not support
        case 0xf:
            break;
        // other speed
        case 7:
            break;
        default:
            break;
    }

    *allow = desc != NULL;
    if (*allow) {
        device->ep0.transfer_remain = len;
        device->ep0.transfer_buffer = (uint8_t*)desc;
    }
}

void UsbImpl_EpInComplete(uint8_t ep_num) {
    switch (ep_num) {
        case UAC2_CONTROL_DATA_IN_ADDRESS & 0xf:
            USBHSD->UEP4_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
            USBHSD->UEP4_TX_CTRL &= ~USBHS_UEP_T_RES_MASK;
            USBHSD->UEP4_TX_CTRL |= USBHS_UEP_T_RES_NAK;
            break;
        case UAC2_STREAM_FEEDBACK_IN_EP_ADDRESS & 0xf:
            // do nothing
            break;
        case CDC_CONTROL_UPLOAD_IN_EP_ADDRESS & 0xf:
            // do nothing
            break;
        case CDC_DATA_IN_EP_ADDRESS & 0xf: {
            USBHSD->UEP3_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;

            uint32_t count = cdc_data_tx_ep.transfer_count;
            cdc_data_tx_ep.transfer_remain -= count;
            cdc_data_tx_ep.transfer_buffer += count;
            count = cdc_data_tx_ep.transfer_remain;
            count = count < cdc_data_tx_ep.max_packet_size ? count : cdc_data_tx_ep.max_packet_size;
            if (count == 0) {
                if (cdc_data_tx_ep.attribute.zero_package == 1) {
                    cdc_data_tx_ep.attribute.zero_package = 0;
                    goto _cdc_ep_send;
                }
                else {
                    cdc_data_tx_ep.transfer_buffer = NULL;
                    cdc_data_tx_ep.attribute.busy = 0;
                    USBHSD->UEP3_TX_CTRL = (USBHSD->UEP3_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_NAK;
                }
            }
            else {
_cdc_ep_send:
                USBHSD->UEP3_TX_LEN = count;
                USBHSD->UEP3_TX_DMA = (uint32_t)cdc_data_tx_ep.transfer_buffer;
                USBHSD->UEP3_TX_CTRL = (USBHSD->UEP3_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
            }
            break;
        }
    }
}

void UsbImpl_EpOutComplete(uint8_t ep_num, uint16_t count) {
    switch (ep_num) {
        case UAC2_STREAM_DATA_OUT_EP_ADDRESS & 0xf:
            AudioDsp_Push(uac2_audio_buffer_, count);
            break;
        case CDC_DATA_OUT_EP_ADDRESS & 0xf:
            USBHSD->UEP3_RX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
            USBHSD->UEP3_RX_CTRL = (USBHSD->UEP3_RX_CTRL & USBHS_UEP_R_TOG_MASK) | USBHS_UEP_R_RES_ACK;
            break;
        case HID_DATA_OUT_EP_ADDRESS & 0xf:
            // 阻塞式：收到一包 HID 报告后端点 NAK，直到主循环
            // UsbHid_GetData() 取走数据并恢复 ACK
            USBHSD->UEP4_RX_CTRL ^= USBHS_UEP_R_TOG_DATA1;
            USBHSD->UEP4_RX_CTRL = (USBHSD->UEP4_RX_CTRL & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_NAK;
            hid_rx_len = count;
            asm volatile("" ::: "memory");
            hid_rx_pending = true;
            break;
    }
}

void UsbImpl_SetFeedbackSr(uint32_t sr) {
    // 转换并写入反馈值；UEP1_TX_DMA 已指向 uac2_feedback_val，硬件每帧自动发送。
    uac2_feedback_val = ConvertSamplerate2FeedbackRate(sr);
}

void UsbImpl_StallEndpoint(uint8_t address) {
    if (address == cdc_data_tx_ep.attribute.address) {
        cdc_data_tx_ep.attribute.stall = 1;
    }
}

void UsbImpl_ClearStallEndpoint(uint8_t address) {
    if (address == cdc_data_tx_ep.attribute.address) {
        cdc_data_tx_ep.attribute.stall = 0;
    }
}

// --------------------------------------------------------------------------------
// uac2
// --------------------------------------------------------------------------------
#define UAC2_GET_CUR(X)   (0x80010000 | X)
#define UAC2_SET_CUR(X)   (0x00010000 | X)
#define UAC2_GET_RANGE(X) (0x80020000 | X)
static uint32_t UsbUac2_MergeRequest(struct UsbSetupRequest* req) {
    return ((req->bmRequestType & 0x80) << 24) | (req->bRequest << 16) | (req->wValue & 0xff00);
}

static void UsbUac2_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase) {
    uint8_t entity = device->setup_request.wIndex >> 8;
    switch (entity) {
        case kUac2EntityId_Clock: {
            switch (UsbUac2_MergeRequest(&device->setup_request)) {
                // sample rate
                case UAC2_GET_CUR(0x0100):
                    *allow = true;
                    *(uint32_t*)device->usb_ep0_buffer = Codec_GetSampleRate();
                    device->ep0.transfer_remain = 4;
                    break;
                case UAC2_SET_CUR(0x0100):
                    *allow = true;
                    if (!setup_phase) {
                        uint32_t fs = *(uint32_t*)device->usb_ep0_buffer;
                        Codec_SetSampleRate(fs);
                    }
                    break;
                case UAC2_GET_RANGE(0x0100): {
                    *allow = true;
                    uint16_t len;
                    device->ep0.transfer_buffer = (uint8_t*)UsbDesc_Uac2SampleRate(&len);
                    device->ep0.transfer_remain = len;
                    break;
                }
            }
            break;
        }
        case kUac2EntityId_FeatureUnit: {
            uint8_t channel = device->setup_request.wValue & 0xff;
            switch (UsbUac2_MergeRequest(&device->setup_request)) {
                // mute
                case UAC2_GET_CUR(0x0100):
                    *allow = true;
                    *(uint8_t*)device->usb_ep0_buffer = Codec_IsMute(channel);
                    device->ep0.transfer_remain = 1;
                    break;
                case UAC2_SET_CUR(0x0100):
                    *allow = true;
                    if (!setup_phase) {
                        uint8_t mute = device->usb_ep0_buffer[0];
                        Codec_Mute(channel, mute);
                    }
                    break;
                // volume
                case UAC2_GET_CUR(0x0200):
                    *allow = true;
                    *(int16_t*)device->usb_ep0_buffer = Codec_GetVolume(channel);
                    device->ep0.transfer_remain = 2;
                    break;
                case UAC2_SET_CUR(0x0200):
                    *allow = true;
                    if (!setup_phase) {
                        int16_t vol = *(int16_t*)device->usb_ep0_buffer;
                        Codec_SetVolume(channel, vol);
                    }
                    break;
                case UAC2_GET_RANGE(0x0200): {
                    *allow = true;
                    // 过了太长时间我忘了这是怎么写的了，似乎是根据es9108寄存器给的值推导了
                    // 虽然是dB刻度，Windows发送的还是线性刻度(?)
                    int16_t* p = (int16_t*)device->usb_ep0_buffer;
                    p[0] = 1;
                    p[1] = -32767;
                    p[2] = 0;
                    p[3] = 128;
                    device->ep0.transfer_remain = 8;
                    break;
                }
            }
            break;
        }
    }
}

// --------------------------------------------------------------------------------
// cdc
// --------------------------------------------------------------------------------
static void UsbCdc_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase) {
    switch (device->setup_request.bRequest) {
        // GET_LINE_CODING
        case 0x21:
            *allow = true;
            device->ep0.transfer_buffer = (uint8_t*)&cdc_line_coding;
            device->ep0.transfer_remain = sizeof(cdc_line_coding);
            break;
        // SET_LINE_CODING
        case 0x20:
            *allow = true;
            if (!setup_phase) {
                memcpy(&cdc_line_coding, device->usb_ep0_buffer, sizeof(cdc_line_coding));
            }
            break;
        // SET_LINE_CTLSTE
        case 0x22:
            *allow = true;
            cdc_can_send = device->setup_request.wValue & 0x1;
            break;
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x23:
            *allow = true;
            break;
    }
}

void UsbCdc_Write(uint8_t const* buffer, uint32_t bytes) {
    cdc_data_tx_ep.transfer_remain = bytes;
    cdc_data_tx_ep.transfer_buffer = (uint8_t*)buffer;
    cdc_data_tx_ep.transfer_count = bytes < cdc_data_tx_ep.max_packet_size ? bytes : cdc_data_tx_ep.max_packet_size;
    cdc_data_tx_ep.attribute.busy = 1;
    cdc_data_tx_ep.attribute.zero_package = bytes % cdc_data_tx_ep.max_packet_size == 0;
    USBHSD->UEP3_TX_DMA = (uint32_t)buffer;
    USBHSD->UEP3_TX_LEN = cdc_data_tx_ep.transfer_count;
    USBHSD->UEP3_TX_CTRL &= ~USBHS_UEP_T_RES_MASK;
    USBHSD->UEP3_TX_CTRL |= USBHS_UEP_T_RES_ACK;
}

bool UsbCdc_CanWrite() {
    return cdc_can_send && (cdc_data_tx_ep.attribute.busy == 0) && (cdc_data_tx_ep.attribute.stall == 0);
}

// --------------------------------------------------------------------------------
// hid
// --------------------------------------------------------------------------------
static void UsbHid_HandleClassRequest(struct UsbDevice* device, bool* allow, bool setup_phase) {
    switch (device->setup_request.bRequest) {
        // set report
        case 0x09:
            break;
        // get report
        case 0x01: {
            *allow = true;
            device->ep0.transfer_buffer = (uint8_t*)Codec_GetUacParam();
            device->ep0.transfer_remain = sizeof(struct UacParam);
            break;
        }
        // set idle
        case 0x0a:
            *allow = true;
            if (!setup_phase) {
                hid_idle = device->usb_ep0_buffer[0];
            }
            break;
        // set protocol
        case 0x0b:
            break;
        // get idle
        case 0x02:
            *allow = true;
            device->usb_ep0_buffer[0] = hid_idle;
            device->ep0.transfer_remain = 1;
            break;
        // get protocol
        case 0x03:
            break;
    }
}

bool UsbHid_HasData(void) {
    return hid_rx_pending;
}

const uint8_t* UsbHid_GetData(void) {
    return hid_rx_buffer;
}

void UsbHid_BeginRecv(void) {
    USBHSD->UEP4_RX_DMA = (uint32_t)hid_rx_buffer;
    USBHSD->UEP4_RX_CTRL = (USBHSD->UEP4_RX_CTRL & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
}
