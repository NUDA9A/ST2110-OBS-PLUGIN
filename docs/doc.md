# Implementation files overview

This document describes implementation files in the project.

## `st2110core/include`

### `st2110/foundation`

#### `bytes.hpp`

Этот файл содержит базовый alias для представления неизменяемого набора байтов:

```c++
using ByteSpan = std::span<const std::uint8_t>;
```

#### `endian.hpp`

Содержит 2 метода для ручного чтения беззнаковых 16-битных и 32-битных чисел из byte span в **Big-Endian** порядке.

```c++
std::uint16_t read_be16(const ByteSpan &s);
std::uint32_t read_be32(const ByteSpan &s);
```

#### `error.hpp`

Содержит общий enum ошибок проекта:

```c++
enum class Error {
    Ok,
    BufferTooSmall,
    InvalidValue,
    Unsupported,
    ShortPacket,
    BadRTPVersion,
    InvalidBackendState,
    SystemFailure,
    OperationInterrupted,
    OperationAborted,
};
```

Также содержит метод `to_string`, который преобразует `Error` в текстовое сообщение:

```c++
const char *to_string(Error error);
```

И метод `is_backend_runtime_error`, который определяет, относится ли ошибка к runtime-состоянию backend-а:

```c++
bool is_backend_runtime_error(Error error) noexcept;
```

Runtime-ошибками backend-а считаются:

```c++
Error::InvalidBackendState
Error::SystemFailure
Error::OperationInterrupted
Error::OperationAborted
```

#### `rtp_timestamp_anchor_policy.hpp`

Содержит enum, который задает режим начальной привязки RTP timestamp к project timestamp при работе timestamp mapper-а:

```c++
enum class RtpTimestampInitialAnchorMode {
    ConfiguredReference,
    FirstObservedBecomesLocalZero,
};
```

`ConfiguredReference` означает, что mapper использует заранее заданную reference-привязку.

`FirstObservedBecomesLocalZero` означает, что первый полученный RTP timestamp становится локальным нулем.

Сейчас timestamp mapper используется только для сценария `mediaclk=direct:<offset>`, где reference-привязка может быть задана заранее.

Для поддержки `mediaclk=sender` стоит добавить в enum отдельную политику, например `WaitForSenderReportReference`. Такая политика должна означать, что mapper не имеет начальной reference-привязки до получения RTCP Sender Report с парой RTP timestamp / reference timestamp.

#### `rtp_timestamp_mapper.hpp`

Содержит helper-функции и класс `RtpTimestampMapper` для преобразования RTP timestamp в project timestamp `TimestampNs`.

Основные helper-функции:

```c++
std::expected<std::uint64_t, Error>
checked_rtp_timestamp_add_u64(std::uint64_t a, std::uint64_t b);

std::expected<std::uint64_t, Error>
checked_rtp_timestamp_mul_u64(std::uint64_t a, std::uint64_t b);

std::expected<std::uint32_t, Error>
forward_rtp_timestamp_delta(std::uint32_t previous, std::uint32_t current);

std::expected<TimestampNs, Error>
rtp_ticks_to_timestamp_ns(std::uint64_t ticks, std::uint32_t rtp_clock_rate, TimestampNs anchor_timestamp_ns);
```

`checked_rtp_timestamp_add_u64` и `checked_rtp_timestamp_mul_u64` выполняют безопасное сложение и умножение `uint64_t` с проверкой overflow.

`forward_rtp_timestamp_delta` вычисляет forward delta между двумя 32-битными RTP timestamp-ами. Если delta попадает в неоднозначную область wrap-around, функция возвращает `Error::InvalidValue`.

`rtp_ticks_to_timestamp_ns` переводит количество RTP ticks в nanoseconds относительно заданного `anchor_timestamp_ns`.

Класс `RtpTimestampMapper` хранит состояние RTP timestamp mapping-а:

```c++
class RtpTimestampMapper;
```

Он использует `RtpTimestampMapperConfig`, в котором задаются:

```c++
rtp_clock_rate
initial_anchor_mode
anchor_rtp_timestamp
anchor_timestamp_ns
```

Метод `map` принимает очередной RTP timestamp и возвращает соответствующий `TimestampNs`:

```c++
std::expected<TimestampNs, Error> map(std::uint32_t rtp_timestamp);
```

Метод `reset` заменяет config mapper-а и сбрасывает runtime-состояние:

```c++
void reset(const RtpTimestampMapperConfig &cfg) noexcept;
```

Сейчас mapper поддерживает два режима начальной привязки:

- `ConfiguredReference` — использует заранее заданные `anchor_rtp_timestamp` и `anchor_timestamp_ns`;
- `FirstObservedBecomesLocalZero` — первый полученный RTP timestamp становится локальным нулем.

Для будущей поддержки `mediaclk=sender` текущей арифметики может быть достаточно после получения RTCP Sender Report, потому что RTCP SR дает reference-пару RTP timestamp / reference timestamp.

#### `timestamp.hpp`

Содержит alias для project timestamp в наносекундах:

```c++
using TimestampNs = std::uint64_t;
```

`TimestampNs` используется как единый тип для timestamp-ов, которые уже приведены к project timeline / nanoseconds representation.

### `st2110/model`

#### `common_sdp_parameters.hpp`

Содержит общие модели SDP-сигнализации, которые не относятся только к video или только к audio.

Файл описывает timing/signaling параметры, transport параметры, reference clock, media clock, source filters и duplicate stream grouping.

Основные enum-ы:

```c++
enum class MediaClockKind { Direct, Sender, Other };
enum class TimestampMode { Samp, New, Pres };
enum class ReferenceClockKind { LocalMac, Ptp, Other };
```

`MediaClockKind` описывает тип `mediaclk`:

- `Direct` — `mediaclk=direct:<offset>`;
- `Sender` — `mediaclk=sender`;
- `Other` — неизвестная или пока не поддержанная форма, сохраненная как raw token.

`TimestampMode` описывает RTP timestamp mode:

- `Samp`;
- `New`;
- `Pres`.

`ReferenceClockKind` описывает тип `ts-refclk`:

- `Ptp`;
- `LocalMac`;
- `Other`.

Основные структуры:

```c++
struct DirectMediaClock;
struct MediaClockSignaling;
struct PtpReferenceClock;
struct LocalMacReferenceClock;
struct ReferenceClock;
struct SourceFilterSignaling;
struct DuplicateStreamGroup;
struct StreamTimingSignaling;
struct StreamTransportSignaling;
```

`MediaClockSignaling` хранит распарсенный `mediaclk`.

`ReferenceClock` хранит распарсенный `ts-refclk`.

`SourceFilterSignaling` хранит SDP source-filter параметры.

`DuplicateStreamGroup` хранит пару `mid`, которые образуют duplicate/redundant stream group.

`StreamTimingSignaling` объединяет общие timing-параметры потока:

```c++
std::uint32_t rtp_clock_rate;
MediaClockSignaling media_clock;
TimestampMode timestamp_mode;
ReferenceClock reference_clock;
std::optional<std::uint32_t> ts_delay_us;
```

`StreamTransportSignaling` объединяет transport-параметры потока:

```c++
std::optional<std::size_t> max_udp_datagram_bytes;
std::vector<SourceFilterSignaling> source_filters;
std::optional<std::string> mid;
std::optional<DuplicateStreamGroup> duplicate_group;
```

Также файл содержит validation helper-ы:

```c++
Error validate_media_clock_signaling(const MediaClockSignaling &clock);
Error validate_reference_clock(const ReferenceClock &clock);
Error validate_duplicate_stream_group(const DuplicateStreamGroup &group);
Error validate_source_filter_signaling(const SourceFilterSignaling &filter);
Error validate_stream_timing_signaling(const StreamTimingSignaling &timing_signaling);
Error validate_stream_transport_signaling(const StreamTransportSignaling &stream_transport_signaling);
```

Эти функции проверяют структурную корректность уже собранных SDP-моделей.

Файл не парсит raw SDP-текст. Он только задает typed-модели и правила их валидации.

### `st2110/model/video`

#### `video_media_types.hpp`

Содержит typed-модели для описания video media parameters из SDP / ST 2110-20.

Основные enum-ы:

```c++
enum class VideoPackingMode { Gpm, Bpm };
enum class VideoScanMode { Progressive, Interlaced, PsF };
```

`VideoPackingMode` описывает packing mode video payload-а:

- `Gpm` — General Packing Mode;
- `Bpm` — Block Packing Mode.

`VideoScanMode` описывает scan mode:

- `Progressive`;
- `Interlaced`;
- `PsF`.

Основные typed-модели video параметров:

```c++
struct VideoSampling;
struct VideoBitDepth;
struct VideoColorimetry;
struct VideoTransferCharacteristicSystem;
struct VideoSignalStandard;
struct VideoRange;
struct VideoPixelAspectRatio;
struct VideoMediaDescription;
```

`VideoSampling` описывает sampling system:

```c++
enum class Known {
    YCbCr422,
    YCbCr444,
    YCbCr420,
    RGB,
    XYZ,
    Key,
    CLYCbCr444,
    CLYCbCr422,
    CLYCbCr420,
    ICtCp444,
    ICtCp422,
    ICtCp420,
    Other
};
```

Для известных значений используется `known`. Для неизвестного или расширенного значения используется `Known::Other` и сохраняется исходный token в `raw_token`.

`VideoBitDepth` описывает bit depth:

```c++
struct VideoBitDepth {
    std::uint8_t bits = 8;
    bool floating_point = false;
};
```

`floating_point = true` используется для `16f`.

`VideoMediaDescription` объединяет основные параметры video essence:

```c++
struct VideoMediaDescription {
    VideoSampling sampling;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t fps_num;
    std::uint32_t fps_den;
    VideoBitDepth depth;
    VideoColorimetry colorimetry;
    std::optional<VideoTransferCharacteristicSystem> transfer_characteristic_system;
    VideoSignalStandard signal_standard;
    std::optional<VideoRange> range;
    VideoPixelAspectRatio pixel_aspect_ratio;
};
```

Файл содержит validation helper-ы для отдельных полей:

```c++
Error validate_video_sampling(const VideoSampling &sampling);
Error validate_video_bit_depth(const VideoBitDepth &depth);
Error validate_video_colorimetry(const VideoColorimetry &colorimetry);
Error validate_video_transfer_characteristic_system(const VideoTransferCharacteristicSystem &tcs);
Error validate_video_signal_standard(const VideoSignalStandard &ssn);
Error validate_video_range(const VideoRange &range);
Error validate_video_media_description_dimensions(std::uint32_t width, std::uint32_t height);
```

Эти функции проверяют структурную корректность отдельных video-параметров:

- для `Other` должен быть непустой `raw_token`;
- для известных значений `raw_token` не должен быть установлен;
- bit depth должен быть допустимым;
- width/height должны быть ненулевыми и не превышать максимальный signaled video dimension.

Также файл содержит helper-ы для проверки bit depth и sampling:

```c++
bool is_420_video_sampling(const VideoSampling &sampling);
bool video_bit_depth_is_integer(const VideoBitDepth &depth, std::uint8_t bits);
bool video_bit_depth_is_16f(const VideoBitDepth &depth);
bool video_bit_depth_is_standard_8_10_12_16_or_16f(const VideoBitDepth &depth);
```

Отдельные validation helper-ы проверяют cross-field constraints:

```c++
Error validate_video_sampling_depth_combination(
    const VideoSampling &sampling,
    const VideoBitDepth &depth
);

Error validate_video_tcs_depth_combination(
    const std::optional<VideoTransferCharacteristicSystem> &tcs,
    const VideoBitDepth &depth
);

Error validate_video_media_description_structure(
    const VideoMediaDescription &media
);

Error validate_video_media_description_cross_field_constraints(
    const VideoMediaDescription &media,
    VideoScanMode scan_mode
);
```

`validate_video_media_description_structure` проверяет базовую структурную корректность `VideoMediaDescription`.

`validate_video_media_description_cross_field_constraints` проверяет зависимости между полями:

- совместимость sampling и bit depth;
- совместимость TCS и bit depth;
- запрет 4:2:0 для non-progressive scan mode;
- специальные ограничения для `Key` / alpha;
- ограничения, требующие ST 2110-20:2022;
- допустимость `range` в зависимости от colorimetry.

#### `video_signaling_types.hpp`

Содержит typed-модель полной video stream signaling-информации.

Файл объединяет:

- video media description;
- scan mode;
- packing mode;
- common timing signaling;
- common transport signaling;
- ST 2110-21 sender-related параметры.

Основной enum:

```c++
enum class VideoSenderType {
    Narrow,
    NarrowLinear,
    Wide
};
```

`VideoSenderType` описывает тип sender-а для ST 2110-21 traffic shaping / delivery timing:

- `Narrow`;
- `NarrowLinear`;
- `Wide`.

Основная структура:

```c++
struct VideoStreamSignaling {
    VideoMediaDescription media;
    VideoScanMode scan_mode;
    VideoPackingMode packing_mode;

    StreamTimingSignaling timing;
    StreamTransportSignaling transport;

    VideoSenderType sender_type;
    std::optional<std::uint32_t> troff_us;
    std::optional<std::uint32_t> cmax;
};
```

`VideoStreamSignaling` — это итоговая typed-модель video SDP signaling-а после парсинга.

Она содержит video-specific параметры из `video_media_types.hpp` и общие SDP/timing/transport параметры из `common_sdp_parameters.hpp`.

Файл также содержит validation helper-ы:

```c++
Error validate_video_sender_signaling(
    VideoSenderType sender_type,
    const std::optional<std::uint32_t> &troff_us,
    const std::optional<std::uint32_t> &cmax
);

Error validate_video_max_udp_datagram_bytes(
    const std::optional<std::size_t> &max_udp_datagram_bytes,
    const VideoPackingMode packing_mode
);

Error validate_video_stream_signaling(
    const VideoStreamSignaling &signaling
);
```

`validate_video_sender_signaling` проверяет video sender параметры:

- `troff_us`, если задан, не должен быть `0`;
- `cmax`, если задан, не должен быть `0`.

`validate_video_max_udp_datagram_bytes` проверяет ограничение UDP datagram size для video packing mode.

Для `VideoPackingMode::Bpm` effective max UDP size не должен превышать `1460` bytes.

`validate_video_stream_signaling` выполняет полную структурную проверку `VideoStreamSignaling`:

- проверяет `VideoMediaDescription` вместе со scan mode;
- проверяет sender signaling;
- проверяет common timing signaling;
- проверяет common transport signaling;
- проверяет max UDP datagram size относительно packing mode;
- проверяет, что video RTP clock rate равен `90000`.

### `st2110/model/audio`

#### `audio_channel_order.hpp`

Содержит typed-модель для описания порядка аудиоканалов из SDP `channel-order`.

Основные enum-ы:

```c++
enum class AudioChannelOrderConvention {
    Unspecified,
    Smpte2110,
    Other
};

enum class AudioChannelGroupKind {
    Mono,
    Stereo,
    DualMono,
    MatrixStereo,
    FiveOne,
    SevenOne,
    TwentyTwoTwo,
    SdiGroup,
    Undefined,
    Other
};
```

`AudioChannelOrderConvention` описывает convention, используемый в `channel-order`:

- `Unspecified` — `channel-order` не задан;
- `Smpte2110` — используется convention `SMPTE2110`;
- `Other` — неизвестная или пока не поддержанная convention, сохраненная как raw value.

`AudioChannelGroupKind` описывает отдельную группу каналов внутри `SMPTE2110` channel order:

- `Mono`;
- `Stereo`;
- `DualMono`;
- `MatrixStereo`;
- `FiveOne`;
- `SevenOne`;
- `TwentyTwoTwo`;
- `SdiGroup`;
- `Undefined`;
- `Other`.

Основные структуры:

```c++
struct AudioChannelOrderGroup;
struct AudioChannelOrder;
```

`AudioChannelOrderGroup` описывает одну группу каналов:

```c++
struct AudioChannelOrderGroup {
    AudioChannelGroupKind kind;
    std::string symbol;
    std::uint16_t channel_count;
};
```

`symbol` хранит исходный символ группы, например `M`, `ST`, `51`, `U02`.

`channel_count` хранит количество каналов в этой группе.

`AudioChannelOrder` описывает весь `channel-order`:

```c++
struct AudioChannelOrder {
    AudioChannelOrderConvention convention;
    std::optional<std::string> raw_value;
    std::vector<AudioChannelOrderGroup> groups;
    std::uint16_t declared_channel_count;
};
```

Для `Smpte2110` поле `groups` содержит разобранные группы каналов, а `declared_channel_count` содержит суммарное количество каналов, объявленное через эти группы.

Если convention неизвестен, исходное значение сохраняется в `raw_value`.

Файл также содержит validation helper-ы:

```c++
Error validate_audio_channel_order_group(
    const AudioChannelOrderGroup &group
);

Error validate_audio_channel_order(
    const AudioChannelOrder &channel_order
);

Error validate_audio_channel_order_against_channel_count(
    const AudioChannelOrder &channel_order,
    const std::uint16_t channel_count
);
```

`validate_audio_channel_order_group` проверяет корректность одной группы каналов:

- `channel_count` не должен быть `0`;
- `symbol` не должен быть пустым;
- известные group kinds должны иметь ожидаемое количество каналов;
- `Undefined` допускает количество каналов до `64`.

`validate_audio_channel_order` проверяет структурную корректность всего `AudioChannelOrder`:

- для `Unspecified` не должно быть `raw_value`, групп и declared channel count;
- для `Smpte2110` должны быть группы и ненулевой `declared_channel_count`;
- сумма каналов по группам должна совпадать с `declared_channel_count`;
- для `Other` должен быть непустой `raw_value`, но не должно быть разобранных групп.

`validate_audio_channel_order_against_channel_count` дополнительно проверяет channel order относительно фактического количества каналов в audio stream.

#### `audio_signaling.hpp`

Содержит typed-модели для описания audio media signaling из SDP / ST 2110-30.

Файл объединяет:

- audio media description;
- optional channel order;
- common timing signaling;
- common transport signaling.

Основные enum-ы:

```c++
enum class AudioPcmEncoding {
    LinearPcm
};

enum class AudioPcmBitDepth {
    Bits16,
    Bits24,
};
```

`AudioPcmEncoding` сейчас поддерживает только linear PCM.

`AudioPcmBitDepth` описывает поддерживаемую PCM bit depth:

- `Bits16`;
- `Bits24`.

Основная media-модель:

```c++
struct AudioMediaDescription {
    AudioPcmEncoding pcm_encoding;
    AudioPcmBitDepth pcm_bit_depth;
    std::uint32_t sampling_rate_hz;
    std::uint32_t packet_time_us;
    std::uint16_t channel_count;
};
```

`AudioMediaDescription` хранит основные параметры audio essence:

- PCM encoding;
- PCM bit depth;
- sampling rate;
- packet time;
- channel count.

Полная модель audio stream signaling:

```c++
struct AudioStreamSignaling {
    AudioMediaDescription media;
    std::optional<AudioChannelOrder> channel_order;

    StreamTimingSignaling timing;
    StreamTransportSignaling transport;
};
```

`AudioStreamSignaling` — итоговая typed-модель audio SDP signaling-а после парсинга.

Она содержит audio-specific параметры и общие SDP/timing/transport параметры из `common_sdp_parameters.hpp`.

Файл содержит validation helper-ы для ST 2110-30 scope:

```c++
Error validate_audio_sampling_rate_st2110_scope(const std::uint32_t sampling_rate_hz);
Error validate_audio_packet_time_st2110_scope(const std::uint32_t packet_time_us);
Error validate_audio_channel_count_st2110_scope(const std::uint16_t channel_count);
```

`validate_audio_sampling_rate_st2110_scope` принимает:

- `44100`;
- `48000`;
- `96000`.

`0` считается `InvalidValue`, остальные значения — `Unsupported`.

`validate_audio_packet_time_st2110_scope` принимает:

- `1000`;
- `125`.

`0` считается `InvalidValue`, остальные значения — `Unsupported`.

`validate_audio_channel_count_st2110_scope` проверяет channel count:

- `0` считается `InvalidValue`;
- значения больше `64` считаются `Unsupported`;
- значения от `1` до `64` считаются допустимыми.

Файл также содержит helper-ы для вычисления количества samples per packet:

```c++
std::expected<std::uint32_t, Error>
derive_audio_samples_per_packet(
    const std::uint32_t sampling_rate_hz,
    const std::uint32_t packet_time_us
);

std::expected<std::uint32_t, Error>
audio_samples_per_packet_from_media_description(
    const AudioMediaDescription &media
);
```

`derive_audio_samples_per_packet` вычисляет количество audio samples per packet из sampling rate и packet time.

Функция возвращает `Error::InvalidValue`, если входные значения равны нулю, если результат не является целым числом samples per packet или если результат не помещается в `uint32_t`.

Файл содержит validation helper-ы для полной audio-модели:

```c++
Error validate_audio_media_description_structure(
    const AudioMediaDescription &media
);

Error validate_audio_stream_signaling(
    const AudioStreamSignaling &signaling
);
```

`validate_audio_media_description_structure` проверяет:

- sampling rate;
- packet time;
- channel count;
- возможность вывести samples per packet.

`validate_audio_stream_signaling` выполняет полную структурную проверку `AudioStreamSignaling`:

- проверяет `AudioMediaDescription`;
- проверяет common timing signaling;
- проверяет, что RTP clock rate равен audio sampling rate;
- проверяет common transport signaling;
- если `channel_order` задан, проверяет его структуру;
- проверяет `channel_order` относительно фактического channel count.

### `st2110/ingress/shared`
### SDP парсинг

SDP парсинг полагается на структурную корректность raw SDP, а также что в одном SDP object может быть либо 1, либо 2 (при условии duplicate stream) медиа секции.
При duplicate stream предполагается, что обе медиа секции согласованы между собой.
Предполагается, что в 1 SDP object находятся медиа секции только для видео или только для аудио.
Предполагается, что в медиа секциях максимум 1 PT (Payload Type).

#### `sdp_common.hpp`

Содержит общие helper-структуры и функции для первичного SDP parsing-а.

Файл находится на ingress-слое: он работает с raw SDP-текстом, извлекает из него общие SDP-поля и преобразует их в промежуточные структуры или common typed-модели. Он не строит полную video/audio signaling-модель сам, а предоставляет общую базу для media-specific SDP парсеров.

Основные raw-структуры:

```c++
struct RawSdpSessionLines;
struct RawSdpMediaSectionLines;
struct RawSdpDocument;
struct RawSdpParsedMediaLine;
```

`RawSdpSessionLines` хранит session-level строки, которые нужны дальнейшему parsing-у:

```c++
std::optional<std::string> group;
```

`RawSdpMediaSectionLines` хранит media-level строки одной SDP media section:

```c++
std::string media_value;
std::string connection;
std::string ts_refclk;
std::string mediaclk;
std::string source_filter;
std::optional<std::string> mid;
std::optional<std::string> ptime;
std::string rtpmap;

std::vector<std::string> fmtp_common_parameters;
std::vector<std::string> fmtp_media_specific_parameters;
```

`RawSdpDocument` объединяет session-level данные и список media sections:

```c++
struct RawSdpDocument {
    RawSdpSessionLines session;
    std::vector<RawSdpMediaSectionLines> media_sections;
};
```

`RawSdpParsedMediaLine` хранит разобранную `m=` строку:

```c++
struct RawSdpParsedMediaLine {
    std::string media_type;
    std::uint16_t udp_port;
    std::string protocol;
    std::uint8_t payload_type;
};
```

Также файл содержит enum для lightweight media-kind classification:

```c++
enum class SdpMediaKind {
    Video,
    Audio
};
```

Файл содержит базовые string helper-ы для SDP parsing-а:

```c++
std::string_view strip_cr(std::string_view line);
std::string_view trim_left_ws(std::string_view text);
std::string_view trim_right_ws(std::string_view text);
std::string_view trim_ws(std::string_view text);
std::optional<std::string_view> parse_attribute_value(
    std::string_view line,
    std::string_view prefix
);
```

Также есть helper-ы для split-а и ASCII case-insensitive matching:

```c++
std::vector<std::string_view> split_char(std::string_view text, char delimiter);
std::vector<std::string_view> split_ws(std::string_view line);

bool ascii_ieq_char(char a, char b);
bool ascii_iequals(std::string_view a, std::string_view b);
bool ascii_istarts_with(std::string_view text, std::string_view prefix);
```

Для числового parsing-а используются:

```c++
template <typename T>
std::expected<T, Error> parse_sdp_numeric_value(std::string_view text);

std::optional<std::uint8_t> parse_payload_type(std::string_view text);
```

`parse_sdp_numeric_value` парсит беззнаковое число в заданный тип и возвращает `Error::InvalidValue`, если значение пустое, содержит лишние символы или не помещается в целевой тип.

`parse_payload_type` парсит RTP payload type и принимает только значения до `127`.

Для parsing-а `m=` строки используется:

```c++
std::expected<RawSdpParsedMediaLine, Error>
parse_raw_sdp_media_line_single_payload_type(std::string_view media_value);
```

Функция ожидает media line с одним payload type:

```text
<media> <port> <proto> <payload-type>
```

Она проверяет:

- ровно 4 token-а;
- UDP port не равен `0`;
- payload type находится в dynamic range `96..127`.

Для `fmtp` файл разделяет common и media-specific параметры:

```c++
bool is_common_fmtp_parameter_name(std::string_view key);

Error split_fmtp_parameters(
    std::string_view fmtp_value,
    std::vector<std::string> &common_parameters,
    std::vector<std::string> &media_specific_parameters
);
```

Common `fmtp` параметрами считаются:

```text
MAXUDP
TSMODE
TSDELAY
```

Остальные `fmtp` параметры складываются в media-specific список.

Для накопления single-line SDP fields используются:

```c++
Error set_single_line(std::optional<std::string> &field, std::string_view value);
Error set_required_single_line(std::string &field, std::string_view value);
```

Обе функции запрещают пустые значения и повторное задание одного и того же поля.

Основная функция первичного parsing-а raw SDP:

```c++
std::expected<RawSdpDocument, Error>
parse_raw_sdp_document(std::string_view sdp);
```

Она проходит по SDP построчно и собирает `RawSdpDocument`.

На session-level сейчас извлекается:

```text
a=group:
```

На media-level сейчас извлекаются:

```text
m=
c=
a=ts-refclk:
a=mediaclk:
a=source-filter:
a=mid:
a=rtpmap:
a=fmtp:
a=ptime:
```

Функция не выполняет полный media-specific parsing. Она только раскладывает нужные строки по raw-полям и отделяет common `fmtp` параметры от media-specific параметров.

Для lightweight classification используются:

```c++
std::expected<SdpMediaKind, Error>
classify_sdp_media_kind_from_document(const RawSdpDocument &raw_sdp);

std::expected<SdpMediaKind, Error>
classify_sdp_media_kind(std::string_view sdp);
```

Classification смотрит на первую `m=` секцию и возвращает:

- `SdpMediaKind::Video`, если media type равен `video`;
- `SdpMediaKind::Audio`, если media type равен `audio`;
- `Error::Unsupported` для других media type.

Файл содержит helper-ы для parsing-а hex byte последовательностей:

```c++
std::expected<std::uint8_t, Error>
parse_hex_u8(std::string_view text);

template <std::size_t N>
std::expected<std::array<std::uint8_t, N>, Error>
parse_dash_separated_hex_bytes(std::string_view text);
```

Они используются для parsing-а PTP clock identity и local MAC address.

Для поиска одиночного `fmtp` параметра используется:

```c++
Error parse_single_fmtp_parameter_value(
    const std::vector<std::string> &parameters,
    std::string_view expected_key,
    std::optional<std::string_view> &out
);
```

Функция ищет параметр по ключу и возвращает `Error::InvalidValue`, если один и тот же параметр задан больше одного раза или если найденный key/value некорректен.

Для parsing-а `mediaclk` используется:

```c++
std::expected<MediaClockSignaling, Error>
parse_media_clock_signaling_value(std::string_view raw_value);
```

Функция распознает:

```text
sender
direct=<offset>
```

`sender` преобразуется в `MediaClockKind::Sender`.

`direct=<offset>` преобразуется в `MediaClockKind::Direct` и `DirectMediaClock`.

Все остальные значения сохраняются как `MediaClockKind::Other` с `raw_token`.

Для parsing-а `c=` строки используется:

```c++
std::expected<ParsedSdpConnectionEndpoint, Error>
parse_sdp_connection_endpoint_value(std::string_view raw_value);
```

Функция разбирает network type, address type и destination address. Для IPv4 дополнительно поддерживает TTL и address count, а для IPv6 — address count.

Для parsing-а `ts-refclk` используется:

```c++
std::expected<ReferenceClock, Error>
parse_reference_clock_value(std::string_view raw_value);
```

Функция распознает:

```text
ptp=IEEE1588-2008:traceable
ptp=IEEE1588-2008:<clockIdentity>
ptp=IEEE1588-2008:<clockIdentity>:<domain>
localmac=<mac>
```

PTP clock identity и local MAC parsing выполняются через dash-separated hex bytes.

Неизвестные формы `ts-refclk` сохраняются как `ReferenceClockKind::Other` с `raw_token`.

Для parsing-а timestamp mode используется:

```c++
std::expected<TimestampMode, Error>
parse_timestamp_mode_value(std::string_view raw_value);
```

Поддерживаемые значения:

```text
SAMP
NEW
PRES
```

Для parsing-а `source-filter` используется:

```c++
std::expected<SourceFilterSignaling, Error>
parse_source_filter_signaling_value(
    std::string_view raw_value,
    SourceFilterSignaling::Scope scope
);
```

Функция принимает только известные filter modes:

```text
incl
excl
```

И заполняет:

```c++
filter_mode
network_type
address_type
destination_address
source_addresses
```

Для session-level duplicate stream group используется:

```c++
std::expected<std::optional<DuplicateStreamGroup>, Error>
parse_session_duplicate_stream_group(const RawSdpSessionLines &session);
```

Функция разбирает session-level group вида:

```text
a=group:DUP <mid-1> <mid-2>
```

Для проверки SDP-документа под single essence stream profile используется:

```c++
Error validate_sdp_document_media_sections_for_single_stream_profile(
    const RawSdpDocument &raw_sdp,
    std::string_view expected_media_type
);
```

Функция проверяет:

- media sections должны быть `1` или `2`;
- каждая media section должна иметь ожидаемый media type;
- одна media section не должна иметь duplicate group;
- две media sections должны иметь корректный `a=group:DUP`;
- `mid` обеих секций должны совпадать с duplicate group.

Для сборки common timing-модели используется:

```c++
std::expected<StreamTimingSignaling, Error>
parse_stream_timing_signaling(
    const RawSdpSessionLines &session,
    const RawSdpMediaSectionLines &media,
    std::uint32_t rtp_clock_rate
);
```

Функция собирает `StreamTimingSignaling` из:

- RTP clock rate;
- `a=ts-refclk`;
- `a=mediaclk`;
- common `fmtp` параметра `TSMODE`;
- common `fmtp` параметра `TSDELAY`.

Для сборки common transport-модели используется:

```c++
std::expected<StreamTransportSignaling, Error>
parse_stream_transport_signaling(
    const RawSdpSessionLines &session,
    const RawSdpMediaSectionLines &media
);
```

Функция собирает `StreamTransportSignaling` из:

- `a=source-filter`;
- `a=mid`;
- common `fmtp` параметра `MAXUDP`;
- session-level duplicate group.

`MAXUDP` должен быть больше `0` и не больше `8960`.

Файл является общей SDP ingress-базой. Он не строит `VideoStreamSignaling` или `AudioStreamSignaling` целиком.

#### `parsed_sdp.hpp`

Содержит typed-результат SDP parsing-а, который возвращают media-specific SDP парсеры.

Файл описывает уже распаршенный SDP stream set: один essence stream может состоять из одной RTP-leg или из двух legs для duplicated/redundant topology.

Основные структуры:

```c++
struct ParsedSdpConnectionEndpoint;
struct ParsedSdpStreamLeg;
struct ParsedSdpStreamSet;
```

`ParsedSdpConnectionEndpoint` описывает endpoint из SDP `c=` строки:

```c++
struct ParsedSdpConnectionEndpoint {
    std::string network_type;
    std::string address_type;
    std::string destination_address;

    std::optional<std::uint8_t> ttl;
    std::optional<std::uint16_t> address_count;
};
```

`network_type` хранит тип сети, например `IN`.

`address_type` хранит тип адреса, например `IP4` или `IP6`.

`destination_address` хранит destination IP address из SDP.

`ttl` используется для IPv4 multicast формы с TTL.

`address_count` используется для SDP connection address forms, где указан count.

`ParsedSdpStreamLeg` описывает одну RTP-leg внутри stream set:

```c++
struct ParsedSdpStreamLeg {
    std::uint8_t expected_payload_type;
    std::uint16_t udp_port;

    ParsedSdpConnectionEndpoint connection;

    std::optional<VideoStreamSignaling> video_stream_signaling;
    std::optional<AudioStreamSignaling> audio_stream_signaling;
};
```

`expected_payload_type` хранит RTP payload type, который receiver должен ожидать для этой leg.

`udp_port` хранит destination UDP port из `m=` строки.

`connection` хранит endpoint из `c=` строки.

`video_stream_signaling` заполняется для video SDP.

`audio_stream_signaling` заполняется для audio SDP.

`ParsedSdpStreamSet` описывает один распаршенный essence stream:

```c++
struct ParsedSdpStreamSet {
    std::vector<ParsedSdpStreamLeg> legs;

    bool is_duplicated() const;
};
```

Если `legs.size() == 1`, stream set описывает single RTP stream.

Если `legs.size() == 2`, stream set описывает duplicated/redundant stream topology.

Метод `is_duplicated` возвращает `true`, когда stream set содержит две legs:

```c++
bool is_duplicated() const { return legs.size() == 2; }
```

### `st2110/ingress/video`

#### `video_sdp_parse.hpp`

Содержит video-specific SDP parser.

Файл преобразует raw SDP document / raw SDP text в `ParsedSdpStreamSet`, где каждая leg содержит заполненный `VideoStreamSignaling`.

Файл использует общие SDP helper-ы из `sdp_common.hpp` и video typed-модели из `video_signaling_types.hpp`.

Основные промежуточные структуры:

```c++
struct RawVideoSdpParseExactFrameRate;
struct RawVideoSdpParsePixelAspectRatio;
struct RawVideoSdpParseRtpMap;
struct RawVideoSdpParseFmtpToken;
```

`RawVideoSdpParseExactFrameRate` хранит разобранный `exactframerate`:

```c++
std::uint32_t numerator;
std::uint32_t denominator;
```

`RawVideoSdpParsePixelAspectRatio` хранит разобранный `PAR`:

```c++
std::uint32_t width;
std::uint32_t height;
```

`RawVideoSdpParseRtpMap` хранит разобранный `a=rtpmap`:

```c++
std::uint8_t payload_type;
std::string encoding_name;
std::uint32_t clock_rate;
std::optional<std::string> encoding_parameters;
```

`RawVideoSdpParseFmtpToken` хранит один разобранный media-specific `fmtp` token:

```c++
std::string_view name;
std::optional<std::string_view> value;
```

Файл содержит helper-ы для parsing-а video SDP значений:

```c++
std::expected<RawVideoSdpParseRtpMap, Error>
parse_video_sdp_parse_rtpmap_payload(std::string_view raw_rtpmap);

std::expected<RawVideoSdpParseFmtpToken, Error>
parse_video_sdp_parse_fmtp_token(std::string_view parameter);

std::expected<std::uint32_t, Error>
parse_video_sdp_parse_positive_u32(std::string_view text);

std::expected<RawVideoSdpParseExactFrameRate, Error>
parse_video_sdp_parse_exactframerate(std::string_view text);

std::expected<RawVideoSdpParsePixelAspectRatio, Error>
parse_video_sdp_parse_pixel_aspect_ratio(std::string_view text);

std::expected<std::pair<std::uint16_t, bool>, Error>
parse_video_sdp_parse_depth(std::string_view text);
```

`parse_video_sdp_parse_rtpmap_payload` разбирает payload `a=rtpmap`.

Ожидаемый формат:

```text
<payload-type> <encoding-name>/<clock-rate>
```

или:

```text
<payload-type> <encoding-name>/<clock-rate>/<encoding-parameters>
```

`parse_video_sdp_parse_exactframerate` поддерживает целочисленный frame rate и дробный frame rate:

```text
60
30000/1001
```

`parse_video_sdp_parse_pixel_aspect_ratio` разбирает `PAR` в формате:

```text
<width>:<height>
```

`parse_video_sdp_parse_depth` разбирает bit depth. Суффикс `f` означает floating-point depth, например:

```text
16f
```

Файл содержит mapping helper-ы, которые преобразуют raw SDP tokens в video model values:

```c++
VideoSampling video_sampling_from_raw_value(std::string_view raw_value);
VideoColorimetry video_colorimetry_from_raw_value(std::string_view raw_value);
VideoTransferCharacteristicSystem video_tcs_from_raw_value(std::string_view raw_value);
VideoSignalStandard video_signal_standard_from_raw_value(std::string_view raw_value);
VideoRange video_range_from_raw_value(std::string_view raw_value);
```

Неизвестные значения сохраняются как `Other` с исходным `raw_token`.

Для video-specific параметров также есть parsing helper-ы:

```c++
std::expected<VideoPackingMode, Error>
parse_video_packing_mode_from_raw_value(std::string_view raw_value);

std::expected<VideoSenderType, Error>
parse_video_sender_type_from_raw_value(std::string_view raw_value);
```

`parse_video_packing_mode_from_raw_value` распознает:

```text
2110GPM
2110BPM
```

`parse_video_sender_type_from_raw_value` распознает:

```text
2110TPN
2110TPNL
2110TPW
```

Основной helper для применения video-specific `fmtp` параметров:

```c++
Error apply_video_media_specific_fmtp_to_signaling(
    const std::vector<std::string> &parameters,
    VideoStreamSignaling &signaling
);
```

Он заполняет `VideoStreamSignaling` из media-specific `fmtp` параметров:

```text
sampling
width
height
exactframerate
depth
colorimetry
PM
SSN
TCS
RANGE
PAR
interlace
segmented
TP
TROFF
CMAX
```

Обязательные параметры:

```text
sampling
width
height
exactframerate
depth
colorimetry
PM
SSN
```

`interlace` и `segmented` определяют `VideoScanMode`:

- нет `interlace` и нет `segmented` → `Progressive`;
- есть `interlace`, нет `segmented` → `Interlaced`;
- есть `interlace` и есть `segmented` → `PsF`;
- есть `segmented`, но нет `interlace` → invalid.

Основная функция parsing-а одной video leg:

```c++
std::expected<ParsedSdpStreamLeg, Error>
parse_video_stream_signaling_leg(
    const RawSdpSessionLines &session,
    const RawSdpMediaSectionLines &media
);
```

Она выполняет parsing одной `m=video` media section:

- разбирает `m=` строку;
- проверяет media type `video`;
- проверяет protocol `RTP/AVP`;
- требует наличие `c=`;
- требует наличие `a=rtpmap`;
- требует наличие media-specific `fmtp` параметров;
- разбирает connection endpoint;
- разбирает `rtpmap`;
- проверяет совпадение payload type из `m=` и `rtpmap`;
- собирает common timing signaling;
- собирает common transport signaling;
- применяет video-specific `fmtp`;
- валидирует итоговый `VideoStreamSignaling`;
- возвращает `ParsedSdpStreamLeg` с заполненным `video_stream_signaling`.

Основная функция parsing-а video stream set из уже разобранного raw SDP document:

```c++
std::expected<ParsedSdpStreamSet, Error>
parse_video_stream_signaling(const RawSdpDocument &raw_sdp);
```

Она проверяет, что SDP document соответствует single-stream profile для `video`:

- разрешена одна media section;
- или две media sections для duplicated/redundant topology;
- все media sections должны быть `video`;
- для duplicated topology должен быть корректный `a=group:DUP`.

Затем функция парсит каждую media section через `parse_video_stream_signaling_leg`.

Если stream set duplicated, payload type обеих legs должен совпадать.

Также есть overload, который принимает raw SDP text:

```c++
std::expected<ParsedSdpStreamSet, Error>
parse_video_stream_signaling(std::string_view sdp);
```

Он сначала вызывает общий `parse_raw_sdp_document`, а затем video-specific parsing.

### `st2110/ingress/audio`

#### `audio_sdp_parse.hpp`

Содержит audio-specific SDP parser.

Файл преобразует raw SDP document / raw SDP text в `ParsedSdpStreamSet`, где каждая leg содержит заполненный `AudioStreamSignaling`.

Файл использует общие SDP helper-ы из `sdp_common.hpp` и audio typed-модели из `audio_signaling.hpp`.

Основные промежуточные структуры:

```c++
struct RawAudioSdpParseRtpMap;
struct RawAudioSdpParseFmtpToken;
```

`RawAudioSdpParseRtpMap` хранит разобранный `a=rtpmap` для audio stream:

```c++
struct RawAudioSdpParseRtpMap {
    std::uint8_t payload_type;
    std::string encoding_name;
    std::uint32_t sampling_rate_hz;
    std::uint16_t channel_count;
};
```

`RawAudioSdpParseFmtpToken` хранит один разобранный media-specific `fmtp` token:

```c++
struct RawAudioSdpParseFmtpToken {
    std::string_view name;
    std::optional<std::string_view> value;
};
```

Файл содержит helper для parsing-а audio `rtpmap`:

```c++
std::expected<RawAudioSdpParseRtpMap, Error>
parse_audio_sdp_parse_rtpmap_payload(std::string_view raw_rtpmap);
```

Ожидаемый формат:

```text
<payload-type> <encoding-name>/<sampling-rate>
```

или:

```text
<payload-type> <encoding-name>/<sampling-rate>/<channel-count>
```

Если channel count не указан, используется значение по умолчанию `1`.

Файл содержит helper для parsing-а `ptime`:

```c++
std::expected<std::uint32_t, Error>
parse_audio_sdp_parse_ptime_us(std::string_view value);
```

`parse_audio_sdp_parse_ptime_us` переводит SDP `ptime` из milliseconds в microseconds.

Поддерживаются целые и дробные значения milliseconds, например:

```text
1
0.125
```

Функция возвращает `Error::InvalidValue`, если значение пустое, содержит некорректную дробную часть, равно нулю или не помещается в `uint32_t`.

Для parsing-а `fmtp` token используется:

```c++
std::expected<RawAudioSdpParseFmtpToken, Error>
parse_audio_sdp_parse_fmtp_token(std::string_view parameter);
```

Файл содержит helper-ы для parsing-а `channel-order`:

```c++
bool is_audio_sdp_parse_channel_order_digit(char c);

bool audio_sdp_parse_channel_order_token_contains_ws(
    std::string_view token
);

std::expected<std::uint16_t, Error>
parse_audio_sdp_parse_channel_order_u_two_digit_count(
    std::string_view symbol
);

std::expected<AudioChannelOrderGroup, Error>
parse_audio_sdp_parse_channel_order_group_from_smpte2110_symbol(
    std::string_view symbol
);

std::expected<AudioChannelOrder, Error>
parse_audio_sdp_parse_smpte2110_channel_order(
    std::string_view raw_value
);

std::expected<AudioChannelOrder, Error>
parse_audio_sdp_parse_channel_order(
    std::string_view raw_value
);
```

`parse_audio_sdp_parse_channel_order_group_from_smpte2110_symbol` распознает SMPTE 2110 channel-order symbols:

```text
M
ST
DM
LtRt
51
71
222
SGRP
Uxx
```

`Uxx` используется для undefined channel group, где `xx` — двухзначное количество каналов от `01` до `64`.

`parse_audio_sdp_parse_smpte2110_channel_order` разбирает значения вида:

```text
SMPTE2110.(ST)
SMPTE2110.(51)
SMPTE2110.(ST,U02)
```

Она заполняет `AudioChannelOrder` с convention `Smpte2110`, списком groups и суммарным `declared_channel_count`.

Если `channel-order` не начинается с `SMPTE2110.`, значение сохраняется как `AudioChannelOrderConvention::Other`.

Файл содержит ASCII helper-ы для case-insensitive сравнения audio encoding names:

```c++
char audio_sdp_parse_ascii_lower(char c);

bool audio_sdp_parse_ascii_iequals(
    std::string_view lhs,
    std::string_view rhs
);
```

Для преобразования `rtpmap` encoding name в audio model используются:

```c++
std::expected<AudioPcmBitDepth, Error>
audio_pcm_bit_depth_from_raw_audio_sdp_parse_rtpmap_encoding_name(
    std::string_view encoding_name
);

std::expected<AudioPcmEncoding, Error>
audio_pcm_encoding_from_raw_audio_sdp_parse_rtpmap_encoding_name(
    std::string_view encoding_name
);
```

Сейчас поддерживаются encoding names:

```text
L16
L24
```

Оба соответствуют `AudioPcmEncoding::LinearPcm`.

`L16` преобразуется в `AudioPcmBitDepth::Bits16`.

`L24` преобразуется в `AudioPcmBitDepth::Bits24`.

Основной helper для применения audio-specific `fmtp` параметров:

```c++
Error apply_audio_media_specific_fmtp_to_signaling(
    const std::vector<std::string> &parameters,
    AudioStreamSignaling &signaling
);
```

Сейчас он обрабатывает media-specific параметр:

```text
channel-order
```

Если `channel-order` указан больше одного раза, возвращается `Error::InvalidValue`.

Основная функция parsing-а одной audio leg:

```c++
std::expected<ParsedSdpStreamLeg, Error>
parse_audio_stream_signaling_leg(
    const RawSdpSessionLines &session,
    const RawSdpMediaSectionLines &media
);
```

Она выполняет parsing одной `m=audio` media section:

- разбирает `m=` строку;
- проверяет media type `audio`;
- проверяет protocol `RTP/AVP`;
- требует наличие `c=`;
- требует наличие `a=rtpmap`;
- требует наличие `a=ptime`;
- разбирает connection endpoint;
- разбирает `rtpmap`;
- проверяет совпадение payload type из `m=` и `rtpmap`;
- разбирает `ptime` в microseconds;
- собирает common timing signaling;
- собирает common transport signaling;
- определяет PCM encoding и bit depth по `rtpmap` encoding name;
- заполняет `AudioStreamSignaling`;
- применяет audio-specific `fmtp`;
- валидирует итоговый `AudioStreamSignaling`;
- возвращает `ParsedSdpStreamLeg` с заполненным `audio_stream_signaling`.

Основная функция parsing-а audio stream set из уже разобранного raw SDP document:

```c++
std::expected<ParsedSdpStreamSet, Error>
parse_audio_stream_signaling(const RawSdpDocument &raw_sdp);
```

Она проверяет, что SDP document соответствует single-stream profile для `audio`:

- разрешена одна media section;
- или две media sections для duplicated/redundant topology;
- все media sections должны быть `audio`;
- для duplicated topology должен быть корректный `a=group:DUP`.

Затем функция парсит каждую media section через `parse_audio_stream_signaling_leg`.

Если stream set duplicated, payload type обеих legs должен совпадать.

Также есть overload, который принимает raw SDP text:

```c++
std::expected<ParsedSdpStreamSet, Error>
parse_audio_stream_signaling(std::string_view sdp);
```

Он сначала вызывает общий `parse_raw_sdp_document`, а затем audio-specific parsing.

Сейчас `channel-order` моделируется, парсится, валидируется и прокидывается в `AudioReceiveBootstrap` / `SocketAudioStreamConfig`, но downstream audio delivery пока не использует его для выбора speaker layout, channel mapping или перестановки каналов.

Использование `channel-order` должно появиться на audio delivery / OBS handoff boundary: там нужно преобразовывать ST 2110 channel order в project/OBS audio layout или явно возвращать `Unsupported` для неподдержанных раскладок.

### `st2110/receive/shared`

#### `receive_bootstrap.hpp`

Содержит common receive bootstrap model — промежуточную receive-oriented модель между результатом SDP parsing-а и backend-specific start config.

Файл преобразует `ParsedSdpStreamSet` в структуру, которая уже описывает не SDP, а то, что receiver должен открыть и принимать.

Основной enum:

```c++
enum class ReceiveTopologyKind {
    SingleStream,
    RedundantPair,
};
```

`ReceiveTopologyKind` описывает receive topology:

- `SingleStream` — один RTP stream;
- `RedundantPair` — две RTP legs для duplicated/redundant topology.

Общая модель signaled stream:

```c++
struct ReceiveSignaledStream {
    std::uint8_t expected_payload_type;
    StreamTimingSignaling timing;
};
```

`ReceiveSignaledStream` хранит общие signaling-параметры, которые нужны receiver-у:

- ожидаемый RTP payload type;
- timing signaling.

Одна remote receive leg описывается структурой:

```c++
struct ReceiveRemoteLeg {
    std::optional<std::string> mid;

    std::uint16_t udp_port;
    ParsedSdpConnectionEndpoint destination;

    SourceFilterSignaling source_filter;
    std::size_t max_udp_datagram_bytes;
};
```

`ReceiveRemoteLeg` хранит параметры одной принимаемой RTP leg:

- `mid` из SDP, если он задан;
- UDP port из `m=` строки;
- destination endpoint из `c=` строки;
- source filter;
- effective `MAXUDP`.

Если `MAXUDP` не задан в SDP, используется значение по умолчанию `1460`.

Общий receive bootstrap:

```c++
struct ReceiveBootstrap {
    ReceiveTopologyKind topology;
    std::vector<ReceiveRemoteLeg> legs;
};
```

`ReceiveBootstrap` хранит topology и список remote legs.

Файл содержит helper для projection-а одной remote leg:

```c++
ReceiveRemoteLeg project_receive_remote_leg(
    const ParsedSdpConnectionEndpoint &connection,
    std::uint16_t udp_port,
    const StreamTransportSignaling &transport
);
```

`project_receive_remote_leg` переносит transport-информацию из parsed SDP model в receive-oriented leg model.

Для video и audio есть отдельные projection helper-ы:

```c++
ReceiveBootstrap project_video_receive_bootstrap(
    const ParsedSdpStreamSet &parsed
);

ReceiveBootstrap project_audio_receive_bootstrap(
    const ParsedSdpStreamSet &parsed
);
```

Обе функции:

- создают `ReceiveRemoteLeg` для каждой parsed SDP leg;
- выставляют topology в `SingleStream` или `RedundantPair`;
- переносят connection, UDP port, source filter и `MAXUDP`.

Отдельный helper строит common signaled stream:

```c++
ReceiveSignaledStream project_receive_signaled_stream(
    std::uint8_t expected_payload_type,
    const StreamTimingSignaling &timing
);
```

#### `video_receive_bootstrap.hpp`

Содержит video-specific receive bootstrap model.

Файл дополняет общий `ReceiveBootstrap` video-specific signaling-информацией, которая была получена из parsed video SDP.

Основная video-specific структура:

```c++
struct VideoReceiveSignaledStream {
    ReceiveSignaledStream receive_signaled_stream;

    VideoMediaDescription media;

    VideoScanMode scan_mode;
    VideoPackingMode packing_mode;

    VideoSenderType sender_type;
    std::optional<std::uint32_t> troff_us;
    std::optional<std::uint32_t> cmax;
};
```

`VideoReceiveSignaledStream` хранит параметры video stream-а, которые нужны receive pipeline / backend projection:

- common receive signaling через `ReceiveSignaledStream`;
- video media description;
- scan mode;
- packing mode;
- sender type;
- optional `TROFF`;
- optional `CMAX`.

Полная video receive bootstrap-модель:

```c++
struct VideoReceiveBootstrap {
    ReceiveBootstrap receive_bootstrap;
    VideoReceiveSignaledStream stream;
};
```

`receive_bootstrap` хранит common receive topology и remote legs.

`stream` хранит video-specific параметры одного essence stream-а.

Файл содержит projection helper:

```c++
VideoReceiveBootstrap project_parsed_video_sdp_to_receive_bootstrap(
    const ParsedSdpStreamSet &parsed
);
```

`project_parsed_video_sdp_to_receive_bootstrap` преобразует результат video SDP parsing-а в video receive bootstrap:

- строит common `ReceiveBootstrap`;
- переносит expected payload type и timing;
- переносит `VideoMediaDescription`;
- переносит scan mode;
- переносит packing mode;
- переносит sender type;
- переносит `TROFF` и `CMAX`.

Для video-specific stream parameters функция использует первую leg:

```c++
parsed.legs[0].video_stream_signaling
```

При duplicated/redundant topology это означает, что video media signaling считается общим для всего essence stream-а, а различия между legs остаются в `ReceiveBootstrap::legs`.

#### `audio_receive_bootstrap.hpp`

Содержит audio-specific receive bootstrap model.

Файл дополняет общий `ReceiveBootstrap` audio-specific signaling-информацией, которая была получена из parsed audio SDP.

Основная audio-specific структура:

```c++
struct AudioReceiveSignaledStream {
    ReceiveSignaledStream receive_signaled_stream;

    AudioMediaDescription media;
    std::optional<AudioChannelOrder> channel_order;
};
```

`AudioReceiveSignaledStream` хранит параметры audio stream-а, которые нужны receive pipeline / backend projection:

- common receive signaling через `ReceiveSignaledStream`;
- audio media description;
- optional `channel_order`.

Полная audio receive bootstrap-модель:

```c++
struct AudioReceiveBootstrap {
    ReceiveBootstrap receive_bootstrap;
    AudioReceiveSignaledStream stream;
};
```

`receive_bootstrap` хранит common receive topology и remote legs.

`stream` хранит audio-specific параметры одного essence stream-а.

Файл содержит projection helper:

```c++
AudioReceiveBootstrap project_parsed_audio_sdp_to_receive_bootstrap(
    const ParsedSdpStreamSet &parsed
);
```

`project_parsed_audio_sdp_to_receive_bootstrap` преобразует результат audio SDP parsing-а в audio receive bootstrap:

- строит common `ReceiveBootstrap`;
- переносит expected payload type и timing;
- переносит `AudioMediaDescription`;
- переносит optional `AudioChannelOrder`.

Для audio-specific stream parameters функция использует первую leg:

```c++
parsed.legs[0].audio_stream_signaling
```

При duplicated/redundant topology это означает, что audio media signaling считается общим для всего essence stream-а, а различия между legs остаются в `ReceiveBootstrap::legs`.

`channel_order` здесь только сохраняется и прокидывается дальше. Его фактическое использование должно происходить позже, на audio delivery / OBS handoff boundary, где ST 2110 channel order должен быть преобразован в project/OBS audio layout или явно отклонен как unsupported.

### `st2110/backends`

#### `receive_local_policy.hpp`

Пожалуй стоит перенести в st2110/receive/

Содержит common-модель локальной receive policy и helper-ы для выбора локального IP/interface по remote receive leg.

Основной enum:

```c++
enum class SocketAddressFamily {
    IPv4,
    IPv6,
};
```

`SocketAddressFamily` описывает address family для receive leg.

Основная структура локальной политики одной leg:

```c++
struct ReceiveLocalLegPolicy {
    SocketAddressFamily family;
    std::string local_ip;

    std::optional<std::string> local_interface_name;
    std::optional<std::string> local_pci_bdf;
};
```

`local_ip` — локальный IP, выбранный для маршрута к remote target.

`local_interface_name` — имя kernel network interface, выбранного для маршрута, например:

```text
enp175s0f0
```

Это backend-neutral metadata. Socket backend может игнорировать это поле, а MTL projection может использовать его для поиска PCI BDF.

`local_pci_bdf` — PCI BDF выбранного сетевого адаптера, например:

```text
0000:af:01.0
```

Для MTL DPDK-user mode это может проецироваться в `MtlRuntimePortConfig::port_name`.

Полная локальная receive policy:

```c++
struct ReceiveLocalPolicy {
    std::vector<ReceiveLocalLegPolicy> legs;
};
```

`ReceiveLocalPolicy` содержит по одной local policy leg на каждую remote receive leg.

Файл также содержит структуру route lookup target:

```c++
struct ReceiveRouteLookupTarget {
    SocketAddressFamily family;
    std::string remote_ip;
};
```

`ReceiveRouteLookupTarget` описывает remote IP, по которому нужно выполнить route lookup для выбора локального интерфейса.

Для проверки IP-адресов файл содержит helper-ы:

```c++
std::expected<uint8_t, Error> parse_ipv4_block(std::string_view block);

bool is_valid_ipv4_address(std::string_view address) noexcept;

bool is_ascii_hex_digit(char c) noexcept;

bool is_valid_ipv6_hextet(std::string_view hextet) noexcept;

bool parse_ipv6_side(
    std::string_view side,
    bool allow_ipv4_tail,
    int &group_count
) noexcept;

bool is_valid_ipv6_address(std::string_view address) noexcept;

bool is_valid_address(
    std::string_view address,
    SocketAddressFamily family
) noexcept;
```

`is_valid_ipv4_address` проверяет IPv4 address в dotted-decimal форме.

`is_valid_ipv6_address` проверяет IPv6 address, включая `::` compression и IPv4 tail. Zone id вида `%eth0` специально не поддерживается.

Для определения route lookup target используются:

```c++
std::expected<SocketAddressFamily, Error>
determine_receive_remote_leg_family(const ReceiveRemoteLeg &leg);

std::expected<std::string, Error>
determine_receive_remote_leg_route_lookup_target_ip(
    const ReceiveRemoteLeg &leg,
    SocketAddressFamily family
);

std::expected<ReceiveRouteLookupTarget, Error>
determine_receive_route_lookup_target(const ReceiveRemoteLeg &leg);
```

`determine_receive_remote_leg_family` определяет address family по `destination.address_type`:

- `IP4` → `IPv4`;
- `IP6` → `IPv6`.

Если `address_type` не распознан, функция пытается определить family по `destination.destination_address`.

`determine_receive_remote_leg_route_lookup_target_ip` выбирает IP для route lookup:

- если в `source_filter.source_addresses` есть source address, используется первый source address;
- иначе используется `destination.destination_address`.

`determine_receive_route_lookup_target` объединяет определение address family и выбор remote IP в один `ReceiveRouteLookupTarget`.

Файл объявляет функции, реализация которых находится в `.cpp`:

```c++
std::expected<ReceiveLocalLegPolicy, Error>
resolve_receive_local_leg_policy_for_route_target(
    const ReceiveRouteLookupTarget &target
);

std::expected<std::string, Error>
resolve_preferred_local_ip_for_remote_target(
    SocketAddressFamily family,
    const std::string &remote_ip
);

std::expected<ReceiveLocalPolicy, Error>
auto_select_receive_local_policy(
    const ReceiveBootstrap &bootstrap
);
```

`resolve_receive_local_leg_policy_for_route_target` строит локальную политику для одного route target.

`resolve_preferred_local_ip_for_remote_target` выбирает preferred local IP для remote target.

`auto_select_receive_local_policy` строит `ReceiveLocalPolicy` для всего `ReceiveBootstrap`.

#### `receive_local_policy.cpp`

Содержит реализацию выбора локальной receive policy для уже построенного `ReceiveBootstrap`.

Файл определяет, какой локальный IP, kernel interface и PCI BDF соответствуют маршруту до remote target каждой receive leg.

На Linux реализация использует системные network API. На других платформах функции возвращают `Error::Unsupported`. Здесь же можно будет написать реализацию под Windows.

Внутренние RAII helper-ы:

```c++
struct ScopedFd;
struct ScopedIfAddrs;
```

`ScopedFd` закрывает file descriptor через `close`.

`ScopedIfAddrs` освобождает список interface-ов через `freeifaddrs`.

Внутренний helper для преобразования socket address в строковый IP:

```c++
std::expected<std::string, Error>
sockaddr_to_ip_string(
    const sockaddr *addr,
    SocketAddressFamily family
);
```

Для `IPv4` использует `AF_INET`.

Для `IPv6` использует `AF_INET6`.

Если address family не совпадает или `inet_ntop` падает, возвращает ошибку.

Внутренний helper для поиска interface name по local IP:

```c++
std::optional<std::string>
find_interface_name_for_local_ip(
    SocketAddressFamily family,
    const std::string &local_ip
);
```

Функция вызывает `getifaddrs`, перебирает локальные interface addresses и ищет interface, чей IP совпадает с выбранным local IP.

Внутренний helper для получения PCI BDF по имени interface-а:

```c++
std::optional<std::string>
resolve_pci_bdf_for_interface(std::string_view interface_name);
```

Функция читает symlink:

```text
/sys/class/net/<interface>/device
```

и берет последний path component как PCI BDF, например:

```text
0000:af:01.0
```

Это используется как metadata для MTL DPDK-user projection.

Внутренний helper для построения remote `sockaddr`:

```c++
std::expected<sockaddr_storage, Error>
make_remote_sockaddr(
    SocketAddressFamily family,
    const std::string &remote_ip,
    socklen_t &remote_addr_len
);
```

Функция строит `sockaddr_in` или `sockaddr_in6` для remote IP.

Для route lookup используется UDP port `9`. Этот port не предназначен для реальной передачи данных; он нужен только чтобы OS выбрала маршрут при `connect`.

Основная Linux-функция выбора local policy для одного route target:

```c++
std::expected<ReceiveLocalLegPolicy, Error>
resolve_receive_local_leg_policy_for_route_target(
    const ReceiveRouteLookupTarget &target
);
```

Функция выполняет такие шаги:

1. Проверяет `target.remote_ip` относительно `target.family`.
2. Создает UDP socket нужной address family.
3. Строит remote socket address.
4. Вызывает `connect` на UDP socket.
5. Через `getsockname` получает local address, который OS выбрала для маршрута.
6. Преобразует local address в строковый `local_ip`.
7. Ищет kernel interface name для найденного `local_ip`.
8. Если interface найден, пытается получить PCI BDF.
9. Возвращает `ReceiveLocalLegPolicy`.

Результат содержит:

```c++
SocketAddressFamily family;
std::string local_ip;
std::optional<std::string> local_interface_name;
std::optional<std::string> local_pci_bdf;
```

Функция для получения только preferred local IP:

```c++
std::expected<std::string, Error>
resolve_preferred_local_ip_for_remote_target(
    SocketAddressFamily family,
    const std::string &remote_ip
);
```

Она строит `ReceiveRouteLookupTarget`, вызывает `resolve_receive_local_leg_policy_for_route_target` и возвращает только `local_ip`.

На non-Linux платформах функции:

```c++
resolve_receive_local_leg_policy_for_route_target(...)
resolve_preferred_local_ip_for_remote_target(...)
```

возвращают:

```c++
Error::Unsupported
```

Общая функция построения local policy для всего bootstrap-а:

```c++
std::expected<ReceiveLocalPolicy, Error>
auto_select_receive_local_policy(
    const ReceiveBootstrap &bootstrap
);
```

Она проходит по всем `ReceiveRemoteLeg` внутри `ReceiveBootstrap`:

1. Определяет `ReceiveRouteLookupTarget` через `determine_receive_route_lookup_target`.
2. Выбирает local policy для target.
3. Добавляет результат в `ReceiveLocalPolicy::legs`.

Для каждой remote receive leg создается одна local policy leg.

#### `receive_start_request.hpp`

Содержит общий receive start request — последний common startup object перед backend-specific projection.

Файл объединяет:

- media-specific receive bootstrap;
- local receive policy.

Media bootstrap представлен через variant:

```c++
using ReceiveMediaBootstrap =
    std::variant<VideoReceiveBootstrap, AudioReceiveBootstrap>;
```

`ReceiveMediaBootstrap` может содержать:

- `VideoReceiveBootstrap`;
- `AudioReceiveBootstrap`.

Основная структура:

```c++
struct ReceiveStartRequest {
    ReceiveMediaBootstrap media;
    ReceiveLocalPolicy local;
};
```

`media` хранит receive bootstrap для конкретного media type.

`local` хранит локальную receive policy: local IP/interface/BDF для каждой receive leg.

`ReceiveStartRequest` строится после SDP parsing, receive bootstrap projection и automatic local receive policy selection.

Далее см. socket/mtl_pipeline.md

Затем см. plugin_pipeline.md

Затем см. mtl_send_test.md

### `scripts/build_install_linux.sh`

#### Linux build/install script

Скрипт автоматизирует подготовку Linux-системы, сборку зависимостей, сборку проекта и установку runtime-артефактов: OBS plugin-а, MTL RX worker-а и `st2110_mtl_send_test`.

Общий pipeline:

```text
system dependencies
→ MTL / DPDK dependencies
→ NDI runtime/header check
→ optional NIC / hugepages / VFIO setup
→ CMake configure
→ build project targets
→ install plugin / worker / sender app
→ print smoke-test commands
```

---

##### Что собирает

Скрипт может собрать три основных target-а:

```text
st2110_obs
    OBS plugin binary

st2110_mtl_rx_worker
    отдельный MTL receive worker process

st2110_mtl_send_test
    тестовый MTL sender + NDI metadata publisher
```

По умолчанию собираются все три.

Отключение:

```text
--no-plugin
    не собирать/не устанавливать OBS plugin и MTL RX worker

--no-send-app
    не собирать/не устанавливать st2110_mtl_send_test
```

---

##### Установка OBS plugin-а

По умолчанию plugin ставится сюда:

```text
~/.config/obs-studio/plugins/st2110_obs/bin/64bit/st2110_obs.so
```

Если включен worker, рядом устанавливается:

```text
~/.config/obs-studio/plugins/st2110_obs/bin/64bit/st2110_mtl_rx_worker
```

Это важно для runtime lookup-а: `MtlWorkerManager` ожидает worker рядом с plugin binary, если не задан explicit path через environment.

Путь можно поменять:

```text
--obs-plugin-dir PATH
```

---

##### Установка sender app

`st2110_mtl_send_test` по умолчанию устанавливается в:

```text
~/.local/bin/st2110_mtl_send_test
```

Путь можно поменять:

```text
--app-install-dir PATH
```

---

##### System dependencies

Если не указан `--no-apt`, скрипт пытается установить системные пакеты через `apt-get`:

```text
build-essential
cmake
ninja-build
pkg-config
meson
libnuma-dev
libjson-c-dev
libpcap-dev
libobs-dev / obs-studio
avahi-daemon
и другие build/runtime dependencies
```

Если некоторых пакетов нет в configured repositories, скрипт не падает сразу, а выводит warning.

Также по умолчанию включает и запускает:

```text
avahi-daemon
```

Отключается через:

```text
--no-avahi-start
```

---

##### NDI проверка

Скрипт ищет:

```text
Processing.NDI.Lib.h
libndi.so.6 / libndi.so
```

Источники поиска:

```text
--ndi-include-dir
--ndi-runtime-dir
NDI_INCLUDE_DIR
NDI_RUNTIME_DIR_V6
NDILIB_REDIST_FOLDER
NDI_SDK_DIR
стандартные /opt, /usr/local, /usr paths
```

По умолчанию NDI обязателен:

```text
REQUIRE_NDI=1
```

Если NDI не найден, скрипт завершится ошибкой.

Можно разрешить сборку без NDI:

```text
--allow-no-ndi
```

В этом случае sender app будет отключен, если нет NDI header-а.

---

##### MTL / DPDK

По умолчанию скрипт сам подготавливает MTL stack:

```text
DPDK repo
→ checkout v25.11
→ apply MTL DPDK patches
→ meson/ninja build
→ ninja install
→ ldconfig

Media-Transport-Library repo
→ checkout v26.01
→ ./build.sh release
→ ldconfig
```

Default paths:

```text
<repo>/.deps/dpdk
<repo>/.deps/Media-Transport-Library
```

Можно задать вручную:

```text
--deps-dir PATH
--dpdk-source-dir PATH
--mtl-source-dir PATH
--dpdk-ref REF
--mtl-ref REF
```

Если MTL уже доступен через `pkg-config mtl`, rebuild пропускается.

Принудительный rebuild:

```text
--force-rebuild-dpdk
--force-rebuild-mtl
```

Полностью не устанавливать MTL:

```text
--no-install-mtl
```

Тогда `pkg-config mtl` должен уже работать.

---

##### Hugepages / VFIO / IOMMU

Скрипт может подготовить Linux runtime для DPDK/MTL.

По умолчанию:

```text
vm.nr_hugepages = 2048
mount /dev/hugepages
persist sysctl config
create vfio group / udev rule
add current user to vfio group
modprobe vfio/vfio-pci
```

Отключение:

```text
--no-hugepages
--no-persist-hugepages
--no-vfio-permissions
```

Опционально может добавить Intel IOMMU kernel args в GRUB:

```text
--configure-intel-iommu-grub
```

Добавляет:

```text
intel_iommu=on iommu=pt
```

После этого нужен reboot.

---

##### NIC setup

Скрипт может вызвать MTL `script/nicctl.sh`:

```text
--create-e800-vfs-bdf BDF
    создать VF для Intel E800 PF и привязать к PMD

--bind-pmd-bdf BDF
    привязать указанный BDF к DPDK PMD
```

Обе опции можно повторять несколько раз.

---

##### CMake configure

Скрипт конфигурирует проект через Ninja:

```text
cmake -S <repo> -B <build> -G Ninja
```

Передаются основные флаги:

```text
-DST2110_BUILD_OBS_PLUGIN=ON/OFF
-DST2110_BUILD_MTL_RX_WORKER=ON/OFF
-DST2110_BUILD_MTL_SEND_TEST_APP=ON/OFF
-DST2110_MTL_DEV_KERNEL_SOCKET=ON/OFF
-DST2110_OBS_PLUGIN_INSTALL_DIR=<plugin-dir>
-DST2110_MTL_RX_WORKER_INSTALL_DIR=<plugin-dir>/bin/64bit
```

Для VM/dev режима есть опция:

```text
--mtl-dev-kernel-socket
```

Она включает project build flag:

```text
ST2110_MTL_DEV_KERNEL_SOCKET=ON
```

То есть MTL backend будет использовать `MTL_PMD_KERNEL_SOCKET` и `kernel:<ifname>` projection для local port. При этом MTL/DPDK библиотеки все равно нужны.

---

##### Build

После configure скрипт собирает выбранные targets:

```text
cmake --build <build-dir> --target st2110_obs st2110_mtl_rx_worker st2110_mtl_send_test -j <jobs>
```

Количество потоков:

```text
--jobs N
```

По умолчанию используется `nproc`.

---

##### Install

После сборки скрипт ищет binaries в build directory и устанавливает:

```text
st2110_obs.so
→ OBS plugin bin dir

st2110_mtl_rx_worker
→ OBS plugin bin dir

st2110_mtl_send_test
→ app install dir
```

Для worker-а дополнительно проверяются runtime dependencies через `ldd`. Если есть `not found`, скрипт завершится ошибкой и подскажет проверить `ldconfig` / dynamic linker path.

---

##### Финальный summary

В конце скрипт печатает:

```text
REPO_DIR
DEPS_DIR
BUILD_DIR
MTL_SOURCE_DIR
DPDK_SOURCE_DIR
NDI paths
PKG_CONFIG_PATH
CMAKE_PREFIX_PATH
LD_LIBRARY_PATH
install locations
```

И сразу выводит smoke-test команды:

```text
metadata-only
video-only 720p30
audio-only
```

---

##### Кратко

Это основной Linux bootstrap/install script проекта:

```text
1. Ставит build/runtime пакеты
2. Настраивает hugepages/VFIO/IOMMU при необходимости
3. Проверяет NDI
4. Собирает или находит DPDK/MTL
5. Может подготовить NIC через MTL nicctl.sh
6. Конфигурирует CMake
7. Собирает plugin, MTL worker и sender app
8. Устанавливает их в OBS/user paths
9. Печатает команды для проверки
```

То есть после успешного выполнения у пользователя должен быть установленный OBS plugin, worker рядом с plugin-ом и доступный `st2110_mtl_send_test` для проверки discovery/media receive pipeline.
