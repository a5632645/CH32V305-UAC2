#include "usb_desc.h"

#include "tpusb/device.hpp"
#include "tpusb/usb.hpp"
#include "tpusb/cdc.hpp"
#include "tpusb/uac2.hpp"
#include "tpusb/hid.hpp"

#include "usb_device.h"
#include "usb_impl.h"

static constexpr auto kDevice =
tpusb::Device{
    0x0200,
    0xef,
    0x02,
    0x01,
    USB_EP0_MAX_PACKAGE_SIZE,
    0x1A86,
    0x0001,
    0x0008,
    1,
    2,
    3,
    1
}.Composite();

static constexpr auto kConfigFs =
tpusb::Config{
    tpusb::ConfigInitPack{
        .config_no = 1,
        .str_id = 0,
        .attribute = 0x80,
        .power = 100
    }
};

using namespace tpusb;
using namespace tpusb::uac2;
using namespace tpusb::cdc;
using namespace tpusb::hid;
static constexpr auto kConfigHs =
Config{
    ConfigInitPack{
        1,
        0,
        0x80,
        250
    },
    UAC2_InterfaceAssociation{
        UAC2_InterfaceAssociation_InitPack{
            .str_id = 0,
            .protocol = 0x20
        },
        AudioControlInterface{
            InterfaceInitPackClassed{
                .interface_no = UAC2_CONTROL_INTERFACE,
                .alter = 0,
                .protocol = 0x20,
                .str_id = 0
            },
            AudioFunction{
                AudioFunctionInitPack{
                    .bcd_adc = 0x0200,
                    .catalog = AudioFunctionCatalog::kDesktopSpeaker,
                    .controls = 0
                },
                audio_function::Clock{
                    kUac2EntityId_Clock,
                    0,
                    0,
                    audio_function::ClockAttribute{}
                        .InternalProgrammableClock(),
                    audio_function::ClockControls{}
                        .FrequencyControl(true, true)
                },
                audio_function::InputTerminal{
                    kUac2EntityId_InputTerminal,
                    audio_function::TerminalType::kUsbStream,
                    0,
                    kUac2EntityId_Clock,
                    0,
                    ChannelInitPack{
                        2,
                        ChannelSpatialLocation::kFrontLeft | ChannelSpatialLocation::kFrontRight,
                        0
                    },
                    audio_function::InputTerminalControls{}
                },
                audio_function::FeatureUnit<3>{
                    audio_function::FeatureUnitInitPack{
                        kUac2EntityId_FeatureUnit,
                        kUac2EntityId_InputTerminal,
                        0
                    },
                    audio_function::ChannelFeatureBuilder{}
                        .Add(audio_function::ChannelFeatures::kMute, true, true)
                        .Add(audio_function::ChannelFeatures::kVolume, true, true)
                        .Duplicate<3>()
                },
                audio_function::OutputTerminal{
                    kUac2EntityId_OutputTerminal,
                    audio_function::TerminalType::kSpeaker,
                    0,
                    kUac2EntityId_FeatureUnit,
                    kUac2EntityId_Clock,
                    0,
                    audio_function::OutputTerminalControls{}
                }
            }
        },
        AudioStreamInterface{
            InterfaceInitPackClassed{
                .interface_no = UAC2_STREAM_INTERFACE,
                .alter = 0,
                .protocol = 0x20,
                .str_id = 0
            },
            TerminalLink{
                kUac2EntityId_InputTerminal,
                0,
                1,
                1,
                ChannelInitPack{
                    2,
                    3,
                    0
                }
            },
            AudioStreamFormat{
                1,
                4,
                32
            },
            Endpoint{
                IsochronousInitPack{
                    UAC2_STREAM_DATA_OUT_EP_ADDRESS,
                    UAC2_STREAM_DATA_OUT_EP_MPSIZE,
                    1,
                    SynchronousType::Isochronous,
                    IsoEpType::Data
                },
                // 手册86页
                CustomDesc<8>{
                    {8, 0x25, 0x01, 0, 0, 0, 0, 0}
                }
            },
            Endpoint{
                IsochronousInitPack{
                    UAC2_STREAM_FEEDBACK_IN_EP_ADDRESS,
                    UAC2_STREAM_FEEDBACK_IN_EP_MPSIZE,
                    1,
                    SynchronousType::None,
                    IsoEpType::Feedback
                }
            }
        }
    },
    InterfaceAssociation{
        InterfaceAssociationInitPack{
            2,
            2,
            1,
            0
        },
        CDCControlInterface{
            InterfaceInitPackClassed{
                CDC_CONTROL_INTERFACE,
                0,
                1,
                0
            },
            FunctionDesc{
                0x0110
            },
            CDCLength{
                0,
                CDC_DATA_INTERFACE
            },
            CDCManagement{
                2
            },
            CDCInterfaceSpecify{
                CDC_CONTROL_INTERFACE,
                CDC_DATA_INTERFACE
            },
            Endpoint{
                InterruptInitPack{
                    CDC_CONTROL_UPLOAD_IN_EP_ADDRESS,
                    CDC_CONTROL_UPLOAD_IN_EP_MPSIZE,
                    4
                }
            }
        },
        CDCDataInterface{
            InterfaceInitPackClassed{
                CDC_DATA_INTERFACE,
                0,
                0,
                0
            },
            Endpoint{
                BulkInitPack{
                    CDC_DATA_IN_EP_ADDRESS,
                    CDC_DATA_IN_EP_MPSIZE,
                    4
                }
            },
            Endpoint{
                BulkInitPack{
                    CDC_DATA_OUT_EP_ADDRESS,
                    CDC_DATA_OUT_EP_MPSIZE,
                    4
                }
            }
        }
    },
    HID_Interface{
        InterfaceInitPackClassed{
            .interface_no = HID_DATA_INTERFACE,
            .alter = 0,
            .protocol = 0,
            .str_id = 0
        },
        HID_Descriptor<1>{
            HID_Descriptor_InitPack{
                .bcd_hid = 0x0111,
                .country_code = 0,
            },
            {
                HID_DescriptorLengthDesc{
                    .type = 0x22,
                    .length = 32
                }
            }
        },
        Endpoint{
            InterruptInitPack{
                .address = HID_DATA_OUT_EP_ADDRESS,
                .max_pack_size = HID_DATA_OUT_EP_MPSIZE,
                .interval = 4
            }
        }
    }
};

static constexpr uint8_t MyQuaDesc[]{
    0x0A,
    0x06,
    0x00, 0x02,
    0xEF,
    0x02,
    0x01,
    0x40,
    0x01,
    0x00,
};

static constexpr uint8_t MyHIDReportDesc_HS[]{
    0x06, 0x00, 0xff,       // Usage Page (Vendor Defined)
    0x09, 0x01,             // Usage (Vendor Usage 1)
    0xA1, 0x01,             // Collection (Application)
    0x09, 0x02,             //   usage 2
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0xFF,             //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x03,             //   Report Count (3)
    0x91, 0x02,             //   Output (Data, Variable, Absolute)
    0x09, 0x03,             //   usage 3
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0xFF,             //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x03,             //   Report Count (3)
    0x81, 0x02,             //   Input (Data, Variable, Absolute)
    0xC0                    // End Collection
};

/* Language Descriptor */
const uint8_t MyLangDescr[] =
{
    0x04, 0x03, 0x09, 0x04
};

/* Manufacturer Descriptor */
const uint8_t MyManuInfo[] =
{
    14, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0
};

#define UC(X) (X & 0xff), (X >> 8)
/* Product Information */
const uint8_t MyProdInfo[] =
{
    18, 0x03, UC('C'), UC('H'), UC('3'), UC('2'), UC('-'), UC('U'), UC('A'), UC('C')
};

/* Serial Number Information */
const uint8_t MySerNumInfo[] =
{
    20, 0x03, UC('2'), UC('0'), UC('2'), UC('5'), UC('-'), UC('5'), UC('-'), UC('2'), UC('8')
};

static constexpr auto kUac2SampleRateTable =
tpusb::uac2::audio_function::ClockSampleRateList<4> {
    {44100, 48000, 96000, 192000}
};

extern "C" {
uint8_t const* UsbDesc_Device(uint16_t* len) {
    *len = kDevice.desc.desc_len;
    return kDevice.desc.desc;
}

uint8_t const* UsbDesc_ConfigFs(uint16_t* len) {
    *len = kConfigFs.char_array.desc_len;
    return kConfigFs.char_array.desc;
}

uint8_t const* UsbDesc_ConfigHs(uint16_t* len) {
    *len = kConfigHs.char_array.desc_len;
    return kConfigHs.char_array.desc;
}

uint8_t const* UsbDesc_String(uint8_t idx, uint16_t* len) {
    switch(idx) {
        case 0:
            *len = sizeof(MyLangDescr);
            return MyLangDescr;;
        case 1:
            *len = sizeof(MyManuInfo);
            return MyManuInfo;
        case 2:
            *len = sizeof(MyProdInfo);
            return MyProdInfo;
        case 3:
            *len = sizeof(MySerNumInfo);
            return MySerNumInfo;
        default:
            return nullptr;
            break;
    }
}

uint8_t const* UsbDesc_Hid(uint16_t interface_idx, uint16_t* len) {
    if (interface_idx == HID_DATA_INTERFACE) {
        *len = sizeof(MyHIDReportDesc_HS);
        return MyHIDReportDesc_HS;
    }
    else {
        return nullptr;
    }
}

uint8_t const* UsbDesc_Qualifier(uint16_t* len) {
    *len = sizeof(MyQuaDesc);
    return MyQuaDesc;
}

uint8_t const* UsbDesc_Uac2SampleRate(uint16_t* len) {
    *len = kUac2SampleRateTable.char_array.desc_len;
    return kUac2SampleRateTable.char_array.desc;
}
}
