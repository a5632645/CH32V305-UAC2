#pragma once
#include "usb.hpp"
#include <cstdint>

namespace tpusb::uac2 {

// --------------------------------------------------------------------------------
// UAC2  https://www.usb.org/sites/default/files/Audio2_with_Errata_and_ECN_through_Apr_2_2025.pdf
// TODO
//     add other audio functions
// --------------------------------------------------------------------------------

// ----------------------------------------
// UAC控制接口
// ----------------------------------------

enum class AudioFunctionCatalog : uint8_t {
    FUNCTION_SUBCLASS_UNDEFINED = 0x00,
    kDesktopSpeaker,
    kHomeTheater,
    kMicrophone,
    kHeadset,
    kTelephone,
    kConverter,
    kVoiceSoundRecorder,
    kIoBox,
    kMusicalInstrument,
    kProAudio,
    kAudioVideo,
    kControlPanel,
    kOther = 0xff
};

struct AudioFunctionInitPack {
    // 0x2000
    uint16_t bcd_adc;
    AudioFunctionCatalog catalog;
    // 0x00 不存在
    // 0x01 只读
    // 0x03 可读可写
    // bits(1, 0) latency
    uint8_t controls;
};

/**
 * @brief Audio Function描述符
 * 
 * @tparam DESCS one @class AudioFunctionInitPack
 *               any Audio Function Descriptor
 */
template<class... DESCS>
struct AudioFunction {
    static constexpr size_t len = DESC_LEN_SUMMER<DESCS...>::len + 9;
    CharArray<len> char_array {
        9,
        0x24,
        0x01
    };
    size_t begin = 9;

    constexpr AudioFunction(AudioFunctionInitPack pack, const DESCS&... desc) {
        char_array[3] = pack.bcd_adc & 0xff;
        char_array[4] = pack.bcd_adc >> 8;
        char_array[5] = static_cast<uint8_t>(pack.catalog);
        char_array[6] = len & 0xff;
        char_array[7] = len >> 8;
        char_array[8] = pack.controls;
        (AppendDesc(desc),...);
    }

    template<class DESC>
    constexpr void AppendDesc(const DESC& desc) {
        size_t desc_len = desc.len;
        for (size_t i = 0; i < desc_len ; ++i) {
            char_array[begin + i] = desc.char_array[i];
        }
        begin += desc_len;
    }
};

/**
 * @brief 定义了 @class ChannelConfig 中的通道空间位置
 * 
 */
enum ChannelSpatialLocation : uint32_t {
    kFrontLeft               = 0x1,
    kFrontRight              = 0x2,
    kFrontCenter             = 0x4,
    kLowFrequencyEffect      = 0x8,
    kBackLeft                = 0x10,
    kBackRight               = 0x20,
    kFrontLeftOfCenter       = 0x40,
    kFrontRightOfCenter      = 0x80,
    kBackCenter              = 0x100,
    kSideLeft                = 0x200,
    kSideRight               = 0x400,
    kTopCenter               = 0x800,
    kTopFrontLeft            = 0x1000,
    kTopFrontCenter          = 0x2000,
    kTopFrontRight           = 0x4000,
    kTopBackLeft             = 0x8000,
    kTopBackCenter           = 0x10000,
    kTopBackRight            = 0x20000,
    kTopFrontLeftOfCenter    = 0x40000,
    kTopFrontRightOfCenter   = 0x80000,
    kLeftLowFrequencyEffect  = 0x100000,
    kRightLowFrequencyEffect = 0x200000,
    kTopSideLeft             = 0x400000,
    kTopSideRight            = 0x800000,
    kBottomCenter            = 0x1000000,
    kBackLeftOfCenter        = 0x2000000,
    kBackRightOfCenter       = 0x4000000,
    RawData                  = 0x80000000
};

/**
 * @brief UAC通道初始化
 * 
 */
struct ChannelInitPack {
    uint8_t num_channel;
    // @enum ChannelSpatialLocation
    uint32_t enum_channel_spatial_location;
    uint8_t channel_str_id;
};

// ----------------------------------------
// Audio Function
// ----------------------------------------

namespace audio_function {

struct ClockAttribute {
    uint8_t data{};

    constexpr ClockAttribute ExternalClock() {
        data &= ~0b11;
        data |= 0b00;
        return *this;
    }
    constexpr ClockAttribute InternalFixedClock() {
        data &= ~0b11;
        data |= 0b01;
        return *this;
    }
    constexpr ClockAttribute InternalVariableClock() {
        data &= ~0b11;
        data |= 0b10;
        return *this;
    }
    constexpr ClockAttribute InternalProgrammableClock() {
        data &= ~0b11;
        data |= 0b11;
        return *this;
    }

    constexpr ClockAttribute SyncToSOF(bool sync) {
        data &= ~0b100;
        if (sync) data |= 0b000;
        return *this;
    }
};

struct ClockControls {
    uint8_t data{};

    constexpr ClockControls FrequencyControl(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");

        data &= ~0b11;
        if (read)   data |= 0b01;
        if (write)  data |= 0b10;
        return *this;
    }
    constexpr ClockControls ValidControl(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");

        data &= ~0b1100;
        if (read)   data |= 0b0100;
        if (write)  data |= 0b1000;
        return *this;
    }
};

/**
 * @brief UAC时钟
 * 
 */
struct Clock {
    static constexpr size_t len = 8;
    CharArray<len> char_array {
        8,
        0x24,
        0x0a
    };

    // 手册50页
    constexpr Clock(
        uint8_t id,
        uint8_t associate_terminal,
        uint8_t str_id,
        ClockAttribute attribute,
        ClockControls controls
    ) {
        char_array[3] = id;
        char_array[4] = attribute.data;
        char_array[5] = controls.data;
        char_array[6] = associate_terminal;
        char_array[7] = str_id;
    }
};

template<size_t NSampleRates>
struct ClockSampleRateList {
    CharArray<2 + NSampleRates * 12> char_array{
        NSampleRates & 0xff,
        NSampleRates >> 8
    };

    constexpr ClockSampleRateList(
        std::array<uint32_t, NSampleRates> list
    ) {
        size_t begin = 2;
        for (size_t i = 0; i < NSampleRates; ++i) {
            char_array[begin++] = list[i] & 0xff;
            char_array[begin++] = list[i] >> 8;
            char_array[begin++] = list[i] >> 16;
            char_array[begin++] = list[i] >> 24;

            char_array[begin++] = list[i] & 0xff;
            char_array[begin++] = list[i] >> 8;
            char_array[begin++] = list[i] >> 16;
            char_array[begin++] = list[i] >> 24;

            char_array[begin++] = 0;
            char_array[begin++] = 0;
            char_array[begin++] = 0;
            char_array[begin++] = 0;
        }
    }
};

/**
 * @brief UAC终端类型
 * @ref https://www.usb.org/sites/default/files/termt10.pdf
 * 
 */
enum class TerminalType : uint16_t {
    // usb
    /* IO */ kUsbUndefined = 0x0100,
    /* IO */ kUsbStream = 0x0101,
    /* IO */ kVendorDefined = 0x01ff,

    // input
    /* I */ kInputUndefined = 0x0200,
    /* I */ kMicrophone = 0x0201,
    /* I */ kDesktopMicrophone = 0x0202,
    /* I */ kPersonalMicrophone = 0x0203,
    /* I */ kOmniDirectionalMicrophone = 0x0204,
    /* I */ kMicrophoneArray = 0x0205,
    /* I */ kProcessingMicrophoneArray = 0x0206,

    // output
    /* O */ kOutputUndefined = 0x0300,
    /* O */ kSpeaker = 0x0301,
    /* O */ kHeadphone = 0x0302,
    /* O */ kHeadMountedDisplayAudio = 0x0303,
    /* O */ kDesktopSpeaker = 0x0304,
    /* O */ kRoomSpeaker = 0x0305,
    /* O */ kCommunicationSpeaker = 0x0306,
    /* O */ kLowFrequencyEffectSpeaker = 0x0307,

    // bi-direction
    /* IO */ kBiDirectionalUndefined = 0x0400,
    /* IO */ kHandset = 0x0401,
    /* IO */ kHeadset = 0x0402,
    /* IO */ kSpeakerphoneNoAEC = 0x0403,
    /* IO */ kSpeakerphoneEchoSuppressing = 0x0404,
    /* IO */ kSpeakerphoneEchoCancel = 0x0405,

    // telephony
    /* IO */ kTelephonyUndefined = 0x0500,
    /* IO */ kPhoneLine = 0x0501,
    /* IO */ kTelephone = 0x0502,
    /* IO */ kDownLinePhone = 0x0503,

    // external
    /* IO */ kExtenalUndefined = 0x0600,
    /* IO */ kAnalogConnector = 0x0601,
    /* IO */ kDigitalAudioInterface = 0x0602,
    /* IO */ kLineConnector = 0x0603,
    /* IO */ kLegacyAudioConnector = 0x0604,
    /* IO */ kSPDIFInterface = 0x0605,
    /* IO */ kDA1394Stream = 0x0606,
    /* IO */ kDV1394StreamSoundTrack = 0x0607,

    // embedded
    /* IO */ kEmbeddedUndefined = 0x0700,
    /*  O */ kLevelCalibrationNoise = 0x0701,
    /*  O */ kEqualizationNoise = 0x0702,
    /* I  */ kCdPlayer = 0x0703,
    /* IO */ kDigitalAudioTape = 0x0704,
    /* IO */ kDigitalCompactCassette = 0x0705,
    /* IO */ kMiniDisk = 0x0706,
    /* IO */ kAnalogTape = 0x0707,
    /* I  */ kPhonograph = 0x0708,
    /* I  */ kVCRAudio = 0x0709,
    /* I  */ kVideoDiscAudio = 0x070a,
    /* I  */ kDVDAudio = 0x070b,
    /* I  */ kTvTunerAudio = 0x070c,
    /* I  */ kSatelliteReceiverAudio = 0x070d,
    /* I  */ kCableTunerAudio = 0x070e,
    /* I  */ kDSSAudio = 0x070f,
    /* I  */ kRatioReceiver = 0x0710,
    /*  O */ kRatioTransmitter = 0x0711,
    /* IO */ kMultiTrackRecorder = 0x0712,
    /* I  */ kSynthesizer = 0x0713,
};

struct InputTerminalControls {
    uint16_t data{};

    constexpr InputTerminalControls CopyProtect(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b11;
        if (read)   data |= 0b01;
        if (write)  data |= 0b10;
        return *this;
    }

    constexpr InputTerminalControls Connector(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b1100;
        if (read)   data |= 0b0100;
        if (write)  data |= 0b1000;
        return *this;
    }

    constexpr InputTerminalControls Overload(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b110000;
        if (read)   data |= 0b010000;
        if (write)  data |= 0b100000;
        return *this;
    }

    constexpr InputTerminalControls Cluster(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b11000000;
        if (read)   data |= 0b01000000;
        if (write)  data |= 0b10000000;
        return *this;
    }

    constexpr InputTerminalControls Underflow(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b1100000000;
        if (read)   data |= 0b0100000000;
        if (write)  data |= 0b1000000000;
        return *this;
    }

    constexpr InputTerminalControls OverFlow(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b110000000000;
        if (read)   data |= 0b010000000000;
        if (write)  data |= 0b100000000000;
        return *this;
    }

    constexpr InputTerminalControls PhantomPower(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b11000000000000;
        if (read)   data |= 0b01000000000000;
        if (write)  data |= 0b10000000000000;
        return *this;
    }
};

/**
 * @brief UAC输入终端
 * 
 */
struct InputTerminal {
    static constexpr size_t len = 17;
    CharArray<len> char_array {
        17,
        0x24,
        0x02
    };

    // 手册53页
    constexpr InputTerminal(
        uint8_t id,
        TerminalType type,
        uint8_t associate_output_Terminal,
        uint8_t clock_id,
        uint8_t str_id,
        ChannelInitPack pack,
        InputTerminalControls controls
    ) {
        char_array[3] = id;
        char_array[4] = static_cast<uint16_t>(type) & 0xff;
        char_array[5] = static_cast<uint16_t>(type) >> 8;
        char_array[6] = associate_output_Terminal;
        char_array[7] = clock_id;
        char_array[8] = pack.num_channel;
        char_array[9] = pack.enum_channel_spatial_location & 0xff;
        char_array[10] = pack.enum_channel_spatial_location >> 8;
        char_array[11] = pack.enum_channel_spatial_location >> 16;
        char_array[12] = pack.enum_channel_spatial_location >> 24;
        char_array[13] = controls.data & 0xff;
        char_array[14] = controls.data >> 8;
        char_array[15] = pack.channel_str_id;
        char_array[16] = str_id;
    }
};

struct OutputTerminalControls {
    uint16_t data{};

    constexpr OutputTerminalControls CopyProtect(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b11;
        if (read)   data |= 0b01;
        if (write)  data |= 0b10;
        return *this;
    }

    constexpr OutputTerminalControls Connector(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b1100;
        if (read)   data |= 0b0100;
        if (write)  data |= 0b1000;
        return *this;
    }

    constexpr OutputTerminalControls Overload(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b110000;
        if (read)   data |= 0b010000;
        if (write)  data |= 0b100000;
        return *this;
    }

    constexpr OutputTerminalControls Underflow(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b11000000;
        if (read)   data |= 0b01000000;
        if (write)  data |= 0b10000000;
        return *this;
    }

    constexpr OutputTerminalControls Overflow(bool read, bool write) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        data &= ~0b1100000000;
        if (read)   data |= 0b0100000000;
        if (write)  data |= 0b1000000000;
        return *this;
    }
};

/**
 * @brief UAC输出终端
 * 
 */
struct OutputTerminal {
    static constexpr size_t len = 12;
    CharArray<len> char_array {
        12,
        0x24,
        0x03
    };

    // 手册54页
    constexpr OutputTerminal(
        uint8_t id,
        TerminalType type,
        uint8_t associate,
        uint8_t input_id,
        uint8_t clock_id,
        uint8_t str_id,
        OutputTerminalControls controls
    ) {
        char_array[3] = id;
        char_array[4] = static_cast<uint16_t>(type) & 0xff;
        char_array[5] = static_cast<uint16_t>(type) >> 8;
        char_array[6] = associate;
        char_array[7] = input_id;
        char_array[8] = clock_id;
        char_array[9] = controls.data & 0xff;
        char_array[10] = controls.data >> 8;
        char_array[11] = str_id;
    }
};

/**
 * @brief UAC特征初始化
 * 
 */
struct FeatureUnitInitPack {
    uint8_t id;
    uint8_t source_id;
    uint8_t str_id;
};

enum class ChannelFeatures : uint32_t {
    kMute = 0,
    kVolume,
    kbass,
    kMid,
    kTreble,
    kGraphicEqualizer,
    kAudiomaticGain,
    kDelay,
    kBassBoost,
    kLoudness,
    kInputGain,
    kIutputGainPad,
    kPhaseInvert,
    kUnderflow,
    kOverfflow,
    kHighpass
};

struct ChannelFeatureBuilder {
    uint32_t data{};

    constexpr ChannelFeatureBuilder Add(
        ChannelFeatures feature,
        bool read,
        bool write
    ) {
        if (!read && write) TriggerConstexprError("usb不允许只写属性");
        uint32_t mask = 0b11 << (2 * static_cast<uint32_t>(feature));
        uint32_t read_mask = 0b01 << (2 * static_cast<uint32_t>(feature));
        uint32_t write_mask = 0b10 << (2 * static_cast<uint32_t>(feature));
        data &= ~mask;
        if (read)   data |= read_mask;
        if (write)  data |= write_mask;
        return *this;
    }

    template<size_t NDuplicate>
    constexpr std::array<ChannelFeatureBuilder, NDuplicate> Duplicate() {
        std::array<ChannelFeatureBuilder, NDuplicate> ret{};
        for (size_t i = 0; i < NDuplicate; ++i) ret[i] = *this;
        return ret;
    }
};

/**
 * @brief UAC特征单元描述符
 * 
 * @tparam N 通道数
 */
template<size_t N>
struct FeatureUnit {
    static constexpr size_t len = 6 + N * 4;
    CharArray<len> char_array {
        len,
        0x24,
        0x06
    };

    constexpr FeatureUnit(
        FeatureUnitInitPack pack,
        std::array<ChannelFeatureBuilder, N> channel_features
    ) {
        char_array[3] = pack.id;
        char_array[4] = pack.source_id;
        char_array[len - 1] = pack.str_id;
        for (size_t i = 0; i < N; ++i) {
            size_t offset = 5 + i * 4;
            uint32_t feature = channel_features[i].data;
            char_array[offset] = feature & 0xff;
            char_array[offset + 1] = feature >> 8;
            char_array[offset + 2] = feature >> 16;
            char_array[offset + 3] = feature >> 24;
        }
    }
};

} // namespace audio_function

/**
 * @brief UAC音频控制接口描述符
 * 
 * @tparam AUDIOFUNCTION 
 *         1. 一个 @class AudioFunctionInitPack
 *            protocol 0x20 IP_VERSION_02_00
 *         2. 一个 @class AudioFunction
 */
template<class... AUDIOFUNCTION>
struct AudioControlInterface : public Interface<AudioFunction<AUDIOFUNCTION...>> {
    constexpr AudioControlInterface(
        InterfaceInitPackClassed pack,
        const AudioFunction<AUDIOFUNCTION...>& function
    ) : Interface<AudioFunction<AUDIOFUNCTION...>>(
        InterfaceInitPack{
            .interface_no = pack.interface_no,
            .alter = pack.alter,
            .class_ = 1,
            .subclass = 1,
            .protocol = pack.protocol,
            .str_id = pack.str_id
        },
        function
    ) {}
};

/**
 * @brief UAC音频控制接口描述符2，带有一个中断端点
 * 
 * @tparam AUDIOFUNCTION 
 *         1. 一个 @class AudioFunctionInitPack
 *            protocol 0x20 IP_VERSION_02_00
 *         2. 一个 @class AudioFunction
 *         3. 一个 @class InterruptInitPack, 自动将长度设为6
 */
template<class... AUDIOFUNCTION>
struct AudioControlInterface2 : public Interface<AudioFunction<AUDIOFUNCTION...>, Endpoint<>> {
    constexpr AudioControlInterface2(
        InterfaceInitPackClassed pack,
        const AudioFunction<AUDIOFUNCTION...>& function,
        InterruptInitPack interrupt
    ) : Interface<AudioFunction<AUDIOFUNCTION...>, Endpoint<>>(
        InterfaceInitPack{
            .interface_no = pack.interface_no,
            .alter = pack.alter,
            .class_ = 1,
            .subclass = 1,
            .protocol = pack.protocol,
            .str_id = pack.str_id
        },
        function,
        Endpoint<>{
            InterruptInitPack{
                .address = interrupt.address,
                .max_pack_size = 6,
                .interval = interrupt.interval
            }
        }
    ) {}
};

// ----------------------------------------
// UAC流接口
// ----------------------------------------

/**
 * @brief UAC流格式描述符
 * 
 */
struct AudioStreamFormat {
    static constexpr size_t len = 6;
    CharArray<len> char_array {
        6,
        0x24,
        0x02
    };
    
    constexpr AudioStreamFormat(uint8_t format_type, uint8_t subslotsize, uint8_t bits) {
        char_array[3] = format_type;
        char_array[4] = subslotsize;
        char_array[5] = bits;
    }
};

/**
 * @brief UAC流终端描述符
 * 
 */
struct TerminalLink {
    static constexpr size_t len = 16;
    CharArray<len> char_array {
        len,
        0x24,
        1
    };

    constexpr TerminalLink(uint8_t link, uint8_t control, uint8_t format, uint32_t formats, ChannelInitPack pack) {
        char_array[3] = link;
        char_array[4] = control;
        char_array[5] = format;
        char_array[6] = formats & 0xff;
        char_array[7] = formats >> 8;
        char_array[8] = formats >> 16;
        char_array[9] = formats >> 24;
        char_array[10] = pack.num_channel;
        char_array[11] = pack.enum_channel_spatial_location & 0xff;
        char_array[12] = pack.enum_channel_spatial_location >> 8;
        char_array[13] = pack.enum_channel_spatial_location >> 16;
        char_array[14] = pack.enum_channel_spatial_location >> 24;
        char_array[15] = pack.channel_str_id;
    }
};

/**
 * @brief UAC流接口，该接口包含两个接口，alter=0为静音, alter=1为工作
 * 
 * @tparam DESCS 
 *         1. one @class InterfaceInitPackClassed, alter will be ignored
 *         2. one @class TerminalLink
 *         3. any @class AudioStreamFormat
 *         4. any @class Endpoint
 */
template<class... DESCS>
struct AudioStreamInterface : public IConfigCustom, public IInterfaceAssociationCustom {
    static constexpr size_t len = DESC_LEN_SUMMER<DESCS...>::len + TerminalLink::len + 9 * 2;
    CharArray<len> char_array;
    size_t begin = 0;

    constexpr AudioStreamInterface(
        InterfaceInitPackClassed pack,
        TerminalLink link,
        const DESCS&... desc
    ) {
        Interface<> empty_interface{
            InterfaceInitPack{
                .interface_no = pack.interface_no,
                .alter = 0,
                .class_ = 1,
                .subclass = 2,
                .protocol = pack.protocol,
                .str_id = pack.str_id
            }
        };
        Interface<TerminalLink, DESCS...> interface{
            InterfaceInitPack{
                .interface_no = pack.interface_no,
                .alter = 1,
                .class_ = 1,
                .subclass = 2,
                .protocol = pack.protocol,
                .str_id = pack.str_id
            },
            link,
            desc...
        };
        for (size_t i = 0; i < empty_interface.len; ++i) {
            char_array[i] = empty_interface.char_array[i];
        }
        begin = empty_interface.len;
        for (size_t i = 0; i < interface.len; ++i) {
            char_array[begin + i] = interface.char_array[i];
        }
    }

    template<class... CONFIG_DESCS>
    constexpr void OnAddToConfig(Config<CONFIG_DESCS...>& config) const {
        config.char_array[IConfig::num_interface_offset]++;
    }

    template<class... OTHER_DESCS>
    constexpr void OnAddToInterfaceAssociation(InterfaceAssociation<OTHER_DESCS...>& association) const {
        if (association.char_array[IInterfaceAssociation::interface_count_offset] == 0) {
            association.char_array[IInterfaceAssociation::first_interface_offset] = char_array[IInterface::interface_no_offset];
        }
        association.char_array[IInterfaceAssociation::interface_count_offset]++;
    }
};

// ----------------------------------------
// UAC2接口关联
// ----------------------------------------

struct UAC2_InterfaceAssociation_InitPack {
    uint8_t str_id;
    // 0x00 FUNCTION_PROTOCOL_UNDEFINED
    // 0x20 AF_VERSION_02_00
    uint8_t protocol;
};

/**
 * @brief 你应该直接在Config中使用该描述符
 * 
 * @tparam INTERFACE 
 *         1. one @class UAC2_InterfaceAssociation_InitPack
 *         2. one @class AudioControlInterface
 *         3. any @class AudioStreamInterface
 */
template<class... INTERFACE>
struct UAC2_InterfaceAssociation : public InterfaceAssociation<INTERFACE...> {
    constexpr UAC2_InterfaceAssociation(
        UAC2_InterfaceAssociation_InitPack pack,
        const INTERFACE&... interface
    ) : InterfaceAssociation<INTERFACE...>(
        InterfaceAssociationInitPack{
            .function_class = 1,
            .function_subclass = 0,
            .function_protocol = pack.protocol,
            .function_str_id = pack.str_id
        },
        interface...
    ) {}
};

} // namespace tpusb::uac2
