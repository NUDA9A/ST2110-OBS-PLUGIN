MTL backend is a separate receive backend.
It does not use project-owned socket reorder/depacketize/reconstruct stack.
It uses MTL ST20P/ST30P receive sessions and exposes received media back through the common IRxBackend/IFrameSink boundary.

### `st2110/backends/mtl`

#### `mtl_runtime_config.hpp`

Содержит project-level config для MTL runtime device layer.

То есть это конфигурация уровня MTL device initialization, а не ST20P/ST30P video/audio receive session.

---

##### `MtlRuntimePortConfig`

```c++
struct MtlRuntimePortConfig {
    std::string port_name;
    std::array<std::uint8_t, 4> sip_addr;

    friend bool operator==(
        const MtlRuntimePortConfig &,
        const MtlRuntimePortConfig &
    ) = default;
};
```

`MtlRuntimePortConfig` описывает один MTL device port.

`port_name` — идентификатор MTL device port-а.

Для целевого Linux/DPDK path это PCI BDF сетевого адаптера, например:

```text
0000:af:01.0
```

Дальше MTL backend проецирует это значение в:

```c++
mtl_init_params::port[...]
```

`st2110core`-уровень не должен сам знать детали `mtl_init_params`. Это остается ответственностью MTL backend/worker implementation.

`sip_addr` — IPv4 address, назначенный этому MTL device port-у.

Дальше MTL backend проецирует это значение в:

```c++
mtl_init_params::sip_addr[...]
```

---

##### `MtlRuntimeConfig`

```c++
struct MtlRuntimeConfig {
    MtlRuntimePortConfig primary_port;
    std::optional<MtlRuntimePortConfig> redundant_port;

    friend bool operator==(
        const MtlRuntimeConfig &,
        const MtlRuntimeConfig &
    ) = default;
};
```

`MtlRuntimeConfig` описывает runtime ports, с которыми должен быть поднят MTL device.

`primary_port` — основной MTL device port.

Projection target:

```text
primary_port.port_name
→ mtl_init_params::port[MTL_PORT_P]

primary_port.sip_addr
→ mtl_init_params::sip_addr[MTL_PORT_P]
```

`redundant_port` — optional redundant MTL device port.

Если `redundant_port` отсутствует, backend строит one-port MTL runtime.

Если `redundant_port` задан, MTL backend может построить two-port runtime и использовать redundant MTL/session port:

```text
MTL_PORT_P
MTL_PORT_R
```

При этом поддержка redundant-port режима должна оставаться явной support boundary. Сам config допускает redundant port структурно, но не означает, что все MTL video/audio session projections уже обязаны поддерживать двухпортовый прием.

---

MTL runtime config отделен от media receive config-а, потому что один MTL device/runtime может быть концептуально ниже video/audio sessions.

Разделение такое:

```text
MtlRuntimeConfig:
    какой MTL device port использовать
    какой source IP назначен port-у
    есть ли redundant device port

MtlVideo/MtlAudio session config:
    destination IP/UDP port
    payload type
    media format
    frame/buffer settings
    ST20P/ST30P ops projection
```

#### `mtl_runtime_resolver.hpp`

Содержит общий resolver, который строит `MtlRuntimeConfig` из receive bootstrap-а и выбранной local policy.

---

##### IPv4 parsing

```c++
std::expected<std::uint8_t, Error>
parse_mtl_ipv4_octet(std::string_view value);

std::expected<std::array<std::uint8_t, 4>, Error>
parse_mtl_ipv4_address(std::string_view address);
```

Эти helper-ы переводят строковый IPv4 address из `ReceiveLocalLegPolicy::local_ip` в массив из 4 octets:

```c++
std::array<std::uint8_t, 4>
```

Этот формат дальше используется как project-side представление для projection в:

```c++
mtl_init_params::sip_addr[...]
```

Если address некорректный, возвращается:

```c++
Error::InvalidValue
```

---

##### Projection одной local leg

```c++
std::expected<MtlRuntimePortConfig, Error>
project_receive_local_leg_to_mtl_runtime_port(
    const ReceiveLocalLegPolicy &local_leg
);
```

Функция строит `MtlRuntimePortConfig` для одной receive leg.

Поддерживается только IPv4:

```c++
local_leg.family == SocketAddressFamily::IPv4
```

IPv6 сейчас возвращает:

```c++
Error::Unsupported
```

`local_ip` парсится в `sip_addr`.

Дальше выбирается `port_name`.

В production/default path используется PCI BDF:

```c++
local_leg.local_pci_bdf
→ MtlRuntimePortConfig::port_name
```

Если `local_pci_bdf` отсутствует, возвращается:

```c++
Error::Unsupported
```

Если включен dev mode (нужно для тестирования на VM):

```c++
ST2110_MTL_DEV_KERNEL_SOCKET
```

то вместо PCI BDF используется Linux interface name:

```text
kernel:<local_interface_name>
```

То есть:

```c++
local_leg.local_interface_name = "enp0s8"
→ port_name = "kernel:enp0s8"
```

Этот режим нужен для MTL kernel socket PMD/dev-конфигурации, а не для основного DPDK-user path.

---

##### Projection всей local policy

```c++
std::expected<MtlRuntimeConfig, Error>
project_receive_local_policy_to_mtl_runtime_config(
    const ReceiveBootstrap &bootstrap,
    const ReceiveLocalPolicy &local_policy
);
```

Функция строит полный `MtlRuntimeConfig`.

Общие проверки:

```text
bootstrap.legs не должен быть пустым
local_policy.legs не должен быть пустым
bootstrap.legs.size() == local_policy.legs.size()
```

---

#### `mtl_video_start_config.hpp`

Содержит project-level start config для MTL video receive path и projection helper из общего `ReceiveStartRequest`.

---

##### Frame rate model

```c++
enum class MtlVideoFrameRate {
    P23_98,
    P24,
    P25,
    P29_97,
    P30,
    P50,
    P59_94,
    P60,
    P100,
    P119_88,
    P120,
};
```

`MtlVideoFrameRate` — project-side enum для тех frame rate значений, которые дальше можно маппить в MTL ST20P `fps`.

Функция:

```c++
std::expected<MtlVideoFrameRate, Error>
project_video_media_to_mtl_frame_rate(
    const VideoMediaDescription &media
);
```

переводит SDP-derived `fps_num / fps_den` в этот enum.

Поддерживаются только явно перечисленные rates:

```text
24000/1001 → P23_98
24/1       → P24
25/1       → P25
30000/1001 → P29_97
30/1       → P30
50/1       → P50
60000/1001 → P59_94
60/1       → P60
100/1      → P100
120000/1001→ P119_88
120/1      → P120
```

Все остальные frame rates возвращают:

```c++
Error::Unsupported
```

---

##### Transport format model

```c++
enum class MtlVideoTransportFormat {
    Yuv422_8Bit,
    Yuv422_10Bit,
    Yuv422_12Bit,
    Yuv422_16Bit,
    Yuv420_8Bit,
    Yuv420_10Bit,
    Yuv420_12Bit,
    Yuv420_16Bit,
    Yuv444_8Bit,
    Yuv444_10Bit,
    Yuv444_12Bit,
    Yuv444_16Bit,
    Rgb8,
    Rgb10,
    Rgb12,
    Rgb16,
};
```

`MtlVideoTransportFormat` описывает ST20 transport-side video format в project-owned виде.

Функция:

```c++
std::expected<MtlVideoTransportFormat, Error>
project_video_media_to_mtl_transport_format(
    const VideoMediaDescription &media
);
```

выбирает transport format из SDP-derived:

```text
VideoSampling
VideoBitDepth
```

Поддерживаемые группы:

```text
YCbCr422: 8 / 10 / 12 / 16 bit
YCbCr420: 8 / 10 / 12 / 16 bit
YCbCr444: 8 / 10 / 12 / 16 bit
RGB:      8 / 10 / 12 / 16 bit
```

Floating-point depth сейчас не поддержан:

```c++
media.depth.floating_point == true
→ Error::Unsupported
```

Все остальные sampling kinds возвращают `Error::Unsupported`.

---

##### Session port config

```c++
struct MtlVideoSessionPortConfig {
    std::array<std::uint8_t, 4> ip_addr;
    std::optional<std::array<std::uint8_t, 4>> source_ip;
    std::uint16_t udp_port;
};
```

`MtlVideoSessionPortConfig` описывает одну ST20P RX session leg.

`ip_addr` — destination/session address:

```text
multicast → multicast group address
unicast   → stream/sender address for MTL session setup
```

Projection target:

```c++
st20p_rx_ops::port.ip_addr[...]
```

`source_ip` — optional source-filter address из SDP `source-filter`.

Сейчас берется только первый source address:

```c++
leg.source_filter.source_addresses.front()
```

Projection target — MTL session source-filter field для соответствующего session port.

`udp_port` — UDP destination port:

```c++
st20p_rx_ops::port.udp_port[...]
```

Функция:

```c++
std::expected<MtlVideoSessionPortConfig, Error>
project_receive_remote_leg_to_mtl_video_session_port(
    const ReceiveRemoteLeg &leg
);
```

переводит `ReceiveRemoteLeg` в `MtlVideoSessionPortConfig`.

Текущее ограничение: session address и source-filter address парсятся только как IPv4 через `parse_mtl_ipv4_address`.

---

##### Full video start config

```c++
struct MtlVideoStartConfig {
    MtlRuntimeConfig runtime;

    MtlVideoSessionPortConfig primary;
    std::optional<MtlVideoSessionPortConfig> redundant;

    std::uint8_t expected_payload_type;

    std::uint32_t width;
    std::uint32_t height;

    MtlVideoFrameRate fps;
    VideoScanMode scan_mode;

    MtlVideoTransportFormat transport_format;
    PixelFormat output_format;

    std::uint16_t frame_buffer_count;
};
```

`MtlVideoStartConfig` объединяет:

```text
MTL runtime device config
ST20P session port config
RTP payload type
video dimensions
frame rate
scan mode
transport format
MTL output/project output format
frame buffer count
```

`runtime` — MTL device/runtime config для `mtl_init`.

`primary` — primary ST20P session leg.

`redundant` — optional redundant ST20P session leg.

Если `redundant` задан, `runtime.redundant_port` также должен быть задан, потому что worker/backend должен будет инициализировать второй MTL runtime port и заполнить второй MTL session port.

`expected_payload_type` проецируется в:

```c++
st20p_rx_ops::port.payload_type
```

`width` / `height` проецируются в:

```c++
st20p_rx_ops::width
st20p_rx_ops::height
```

`fps` проецируется в:

```c++
st20p_rx_ops::fps
```

`scan_mode` сохраняет project scan-mode axis. Concrete MTL projection позже должна либо сматчить его на `st20p_rx_ops::interlaced`, либо вернуть `Unsupported`.

`transport_format` проецируется в:

```c++
st20p_rx_ops::transport_fmt
```

`output_format` — project `PixelFormat`, который должен соответствовать MTL output frame format projection:

```c++
st20p_rx_ops::output_fmt
```

`frame_buffer_count` проецируется в:

```c++
st20p_rx_ops::framebuff_cnt
```

---

##### Projection settings

```c++
struct MtlVideoStartProjectionSettings {
    PixelFormat output_format = PixelFormat::UYVY;
    std::uint16_t frame_buffer_count = 3;
};
```

`MtlVideoStartProjectionSettings` задает локальную policy для projection-а, которая не приходит напрямую из SDP:

- какой output `PixelFormat` запросить у MTL;
- сколько ST20P frame buffers использовать.

Сейчас default output format:

```text
PixelFormat::UYVY
```

и default frame buffer count:

```text
3
```

---

##### Main projection helper

```c++
std::expected<MtlVideoStartConfig, Error>
project_receive_start_request_to_mtl_video_start(
    const ReceiveStartRequest &request,
    const MtlVideoStartProjectionSettings &settings
);
```

Функция строит полный `MtlVideoStartConfig`.

Pipeline:

```text
1. Проверить, что request.media содержит VideoReceiveBootstrap
2. Проверить, что receive legs и local legs не пустые
3. Проверить совпадение количества remote/local legs
4. Построить MtlRuntimeConfig из ReceiveBootstrap + ReceiveLocalPolicy
5. Построить primary MTL video session port из первой remote leg
6. Спроецировать frame rate из VideoMediaDescription
7. Спроецировать MTL transport format из VideoMediaDescription
8. Заполнить MtlVideoStartConfig
9. Если topology == RedundantPair, построить redundant session port
```

---

##### Что переносится из `VideoReceiveBootstrap`

Из `VideoReceiveBootstrap` используются:

```text
receive_bootstrap.legs
receive_bootstrap.topology
stream.receive_signaled_stream.expected_payload_type
stream.media.width
stream.media.height
stream.media.fps_num / fps_den
stream.media.sampling
stream.media.depth
stream.scan_mode
```

Из `ReceiveStartRequest::local` используется local policy для построения `MtlRuntimeConfig`.

---

##### Что сейчас не переносится

`VideoReceiveBootstrap` содержит ST 2110-21 параметры:

```text
sender_type
troff_us
cmax
```

В `MtlVideoStartConfig` они сейчас не попадают.

То есть текущая MTL video start projection не использует signaled traffic-shaping/timing параметры.

---

#### `mtl_audio_start_config.hpp`

Содержит project-level start config для MTL audio receive path и projection helper из общего `ReceiveStartRequest`.

---

##### Default audio frame-buffer policy

```c++
inline constexpr std::uint64_t mtlAudioDefaultFrameBufferDurationNs = 10'000'000ULL;
inline constexpr std::uint16_t mtlAudioDefaultFrameBufferCount = 3;
```

Задает default policy для ST30P frame buffers:

```text
frame buffer duration = 10 ms
frame buffer count    = 3
```

Это MTL-specific buffering policy. Она не берется из SDP и не относится к Socket audio path.

---

##### MTL audio projection enums

```c++
enum class MtlAudioPcmFormat {
    Pcm16,
    Pcm24,
};

enum class MtlAudioSampling {
    K48,
};

enum class MtlAudioPacketTime {
    Ptime1ms,
};
```

Эти enum-ы задают project-side projection axes для MTL ST30P RX session.

Сейчас поддерживается только MVP subset:

```text
PCM format:  PCM16 / PCM24
Sampling:    48 kHz
Packet time: 1 ms
```

---

##### Session port config

```c++
struct MtlAudioSessionPortConfig {
    std::array<std::uint8_t, 4> ip_addr;
    std::optional<std::array<std::uint8_t, 4>> source_ip;
    std::uint16_t udp_port;
};
```

`MtlAudioSessionPortConfig` описывает одну ST30P RX session leg.

`ip_addr` — destination/session address:

```text
multicast → multicast group address
unicast   → stream/sender address for MTL session setup
```

Projection target:

```c++
st30p_rx_ops::port.ip_addr[...]
```

`source_ip` — optional source-filter address из SDP `source-filter`.

Сейчас берется только первый source address:

```c++
leg.source_filter.source_addresses.front()
```

`udp_port` — UDP destination port:

```c++
st30p_rx_ops::port.udp_port[...]
```

Функция:

```c++
std::expected<MtlAudioSessionPortConfig, Error>
project_receive_remote_leg_to_mtl_audio_session_port(
    const ReceiveRemoteLeg &leg
);
```

строит `MtlAudioSessionPortConfig` из `ReceiveRemoteLeg`.

Текущее ограничение: session address и source-filter address парсятся только как IPv4 через `parse_mtl_ipv4_address`.

---

##### Full audio start config

```c++
struct MtlAudioStartConfig {
    MtlRuntimeConfig runtime;

    MtlAudioSessionPortConfig primary;
    std::optional<MtlAudioSessionPortConfig> redundant;

    std::uint8_t expected_payload_type;

    AudioMediaDescription media;
    std::uint32_t samples_per_packet;

    MtlAudioPcmFormat pcm_format;
    MtlAudioSampling sampling;
    MtlAudioPacketTime packet_time;

    std::uint16_t frame_buffer_count;
    std::uint64_t frame_buffer_duration_ns;
};
```

`MtlAudioStartConfig` объединяет:

```text
MTL runtime device config
ST30P session port config
RTP payload type
audio media description
derived samples-per-packet
MTL PCM/sampling/ptime projection values
ST30P frame-buffer policy
```

`runtime` — MTL device/runtime config для `mtl_init`.

`primary` — primary ST30P session leg.

`redundant` — optional redundant ST30P session leg.

Если `redundant` задан, `runtime.redundant_port` также должен быть задан, потому что backend/worker должен будет инициализировать второй MTL runtime port и заполнить второй MTL session port.

`expected_payload_type` проецируется в ST30P RX session port payload type.

`media` сохраняет project audio media model:

```text
PCM encoding
PCM bit depth
sampling rate
packet time
channel count
```

`samples_per_packet` вычисляется из `media.sampling_rate_hz` и `media.packet_time_us`, чтобы MTL audio path не hardcode-ил значение `48`.

`pcm_format`, `sampling`, `packet_time` — уже подготовленные MTL-supported projection axes для последующего заполнения:

```c++
st30p_rx_ops::fmt
st30p_rx_ops::sampling
st30p_rx_ops::ptime
```

`frame_buffer_count` и `frame_buffer_duration_ns` задают ST30P frame-buffer policy.

---

##### Projection settings

```c++
struct MtlAudioStartProjectionSettings {
    std::uint16_t frame_buffer_count = mtlAudioDefaultFrameBufferCount;
    std::uint64_t frame_buffer_duration_ns = mtlAudioDefaultFrameBufferDurationNs;
};
```

`MtlAudioStartProjectionSettings` задает локальную MTL audio policy, которая не приходит напрямую из SDP.

---

##### Audio media projection helpers

```c++
std::expected<MtlAudioPcmFormat, Error>
project_audio_media_to_mtl_pcm_format(
    const AudioMediaDescription &media
);
```

Поддерживает только linear PCM:

```text
AudioPcmBitDepth::Bits16 → MtlAudioPcmFormat::Pcm16
AudioPcmBitDepth::Bits24 → MtlAudioPcmFormat::Pcm24
```

Если encoding не `LinearPcm`, возвращается `Error::Unsupported`.

```c++
std::expected<MtlAudioSampling, Error>
project_audio_media_to_mtl_sampling(
    const AudioMediaDescription &media
);
```

Поддерживает только:

```text
48000 Hz → MtlAudioSampling::K48
```

```c++
std::expected<MtlAudioPacketTime, Error>
project_audio_media_to_mtl_packet_time(
    const AudioMediaDescription &media
);
```

Поддерживает только:

```text
1000 us → MtlAudioPacketTime::Ptime1ms
```

```c++
std::expected<std::uint16_t, Error>
project_audio_media_to_mtl_channel_count(
    const AudioMediaDescription &media
);
```

Поддерживает channel count:

```text
1..8
```

---

##### Main projection helper

```c++
std::expected<MtlAudioStartConfig, Error>
project_receive_start_request_to_mtl_audio_start(
    const ReceiveStartRequest &request,
    const MtlAudioStartProjectionSettings &settings
);
```

Функция строит полный `MtlAudioStartConfig`.

Pipeline:

```text
1. Проверить frame-buffer settings
2. Проверить, что request.media содержит AudioReceiveBootstrap
3. Проверить, что receive legs и local legs не пустые
4. Проверить совпадение количества remote/local legs
5. Спроецировать PCM format
6. Спроецировать sampling rate
7. Спроецировать packet time
8. Проверить channel count support boundary
9. Вычислить samples_per_packet
10. Построить MtlRuntimeConfig из ReceiveBootstrap + ReceiveLocalPolicy
11. Построить primary MTL audio session port
12. Заполнить MtlAudioStartConfig
13. Если topology == RedundantPair, построить redundant session port
```

---

##### Что переносится из `AudioReceiveBootstrap`

Из `AudioReceiveBootstrap` используются:

```text
receive_bootstrap.legs
receive_bootstrap.topology
stream.receive_signaled_stream.expected_payload_type
stream.media
```

Из `ReceiveStartRequest::local` используется local policy для построения `MtlRuntimeConfig`.

---

##### Что сейчас не переносится

`AudioReceiveBootstrap` содержит:

```text
channel_order
```

В `MtlAudioStartConfig` он сейчас не попадает.

То есть MTL audio start projection не использует SDP `channel-order` для MTL session setup, channel mapping или OBS speaker layout projection.

---

#### `mtl_worker_protocol.hpp`

Содержит typed protocol contract между основным процессом и MTL worker process.

Этот файл также задает shared-memory metadata, через которую worker сообщает, где лежит готовый video frame или audio block.

---

##### Общая роль файла

`mtl_worker_protocol.hpp` задает:

```text
1. request/event IDs
2. graph/session IDs
3. shared-memory ring descriptors
4. worker control requests
5. worker control/events responses
6. MTL/backend-local stats structures
```

---

##### ID types

```c++
using MtlWorkerRequestId = std::uint64_t;
using MtlWorkerGraphId = std::uint64_t;
using MtlWorkerSlotId = std::uint64_t;
using MtlWorkerSharedMemoryRingId = std::uint64_t;
```

Эти aliases задают основные identity values protocol-а:

- `MtlWorkerRequestId` — ID конкретного control request-а;
- `MtlWorkerGraphId` — ID receive graph-а внутри worker-а;
- `MtlWorkerSlotId` — slot ID внутри shared-memory ring-а;
- `MtlWorkerSharedMemoryRingId` — ID shared-memory ring-а.

`request_id` нужен для связывания request/response.

`graph_id` нужен для связи событий с конкретным receive graph-ом.

`ring_id` + `slot_id` нужны для доставки media через shared memory.

---

##### Media kind

```c++
enum class MtlWorkerMediaKind : std::uint32_t {
    Video = 0,
    Audio = 1,
};
```

Определяет, какой media type хранится в shared-memory ring-е.

Используется в descriptor-е ring-а, чтобы core-side и worker-side одинаково понимали layout и payload semantics.

---

##### Shared-memory ring descriptor

```c++
struct MtlWorkerSharedMemoryRingDescriptor {
    MtlWorkerSharedMemoryRingId ring_id;

    MtlWorkerMediaKind media_kind;

    std::uint32_t fd_index;

    std::uint32_t layout_version;

    std::uint64_t mapped_size_bytes;

    std::uint64_t slot_region_offset_bytes;

    std::uint32_t slot_count;

    std::uint64_t slot_stride_bytes;

    std::uint64_t slot_payload_offset_bytes;

    std::uint64_t slot_payload_capacity_bytes;
};
```

`MtlWorkerSharedMemoryRingDescriptor` описывает один shared-memory ring, который worker может использовать для публикации media data.

Ключевые поля:

- `ring_id` — logical ID ring-а;
- `media_kind` — video или audio;
- `fd_index` — индекс file descriptor-а, через который передана shared memory;
- `layout_version` — версия layout-а;
- `mapped_size_bytes` — полный размер mapped shared-memory region;
- `slot_region_offset_bytes` — offset области со slots;
- `slot_count` — количество slots;
- `slot_stride_bytes` — расстояние между началами соседних slots;
- `slot_payload_offset_bytes` — offset payload-а внутри slot-а;
- `slot_payload_capacity_bytes` — максимальный размер payload-а внутри одного slot-а.

Версия layout-а задается константой:

```c++
inline constexpr std::uint32_t mtlWorkerSharedMemoryRingLayoutVersion = 3;
```

Default количество descriptors:

```c++
inline constexpr std::size_t defaultMtlWorkerMaxSharedMemoryRingDescriptors = 16;
```

---

##### Control requests

```c++
using MtlWorkerControlRequest =
    std::variant<
        MtlWorkerConfigHandshakeRequest,
        MtlWorkerStartSessionsRequest,
        MtlWorkerStopSessionsRequest,
        MtlWorkerStatsRequest,
        MtlWorkerHealthCheckRequest,
        MtlWorkerShutdownRequest
    >;
```

`MtlWorkerControlRequest` — variant всех control messages, которые основной процесс может отправить worker-у.

---

##### Config handshake request

```c++
struct MtlWorkerConfigHandshakeRequest {
    MtlWorkerRequestId request_id;
    MtlRuntimeConfig runtime;
};
```

Handshake request передает worker-у MTL runtime config.

Pipeline:

```text
MtlRuntimeConfig
→ worker config handshake
→ worker validates/accepts runtime-level configuration
```

Этот request задает runtime/device configuration boundary.

---

##### Start sessions request

```c++
struct MtlWorkerStartSessionsRequest {
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;

    std::optional<MtlVideoStartConfig> video;
    std::optional<MtlAudioStartConfig> audio;

    std::vector<MtlWorkerSharedMemoryRingDescriptor> media_rings;
};
```

`MtlWorkerStartSessionsRequest` просит worker создать receive graph и запустить video/audio sessions.

Он содержит:

- `graph_id` — ID graph-а;
- optional `video` config;
- optional `audio` config;
- descriptors shared-memory rings, куда worker должен публиковать media.

Config может содержать video, audio или оба направления, если graph поддерживает combined receive model.

Pipeline:

```text
MtlVideoStartConfig / MtlAudioStartConfig
+ shared-memory rings
→ worker graph
→ MTL ST20P/ST30P sessions
```

---

##### Stop / stats / health / shutdown requests

```c++
struct MtlWorkerStopSessionsRequest {
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;
};
```

Останавливает sessions конкретного graph-а.

```c++
struct MtlWorkerStatsRequest {
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;
};
```

Запрашивает stats snapshot по graph-у.

```c++
struct MtlWorkerHealthCheckRequest {
    MtlWorkerRequestId request_id;
};
```

Запрашивает health status worker-а.

```c++
struct MtlWorkerShutdownRequest {
    MtlWorkerRequestId request_id;
};
```

Просит worker process завершиться.

---

##### Control events

```c++
using MtlWorkerControlEvent =
    std::variant<
        MtlWorkerStartedEvent,
        MtlWorkerStoppedEvent,
        MtlWorkerErrorEvent,
        MtlWorkerStatsEvent,
        MtlWorkerHealthEvent,
        MtlWorkerFrameReadyEvent,
        MtlWorkerAudioBlockReadyEvent
    >;
```

`MtlWorkerControlEvent` — variant всех событий, которые worker может отправить основному процессу.

---

##### Started / stopped / error events

```c++
struct MtlWorkerStartedEvent {
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;
};

struct MtlWorkerStoppedEvent {
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;
};
```

Эти события подтверждают start/stop graph-а.

```c++
struct MtlWorkerErrorEvent {
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;
    Error error;
    std::string message;
};
```

`MtlWorkerErrorEvent` сообщает об ошибке, связанной с request-ом или graph-ом.

---

##### Media ready events

```c++
struct MtlWorkerFrameReadyEvent {
    MtlWorkerGraphId graph_id;
    MtlWorkerSharedMemoryRingId ring_id;
    MtlWorkerSlotId slot_id;
    std::uint64_t sequence;
};

struct MtlWorkerAudioBlockReadyEvent {
    MtlWorkerGraphId graph_id;
    MtlWorkerSharedMemoryRingId ring_id;
    MtlWorkerSlotId slot_id;
    std::uint64_t sequence;
};
```

Эти события сообщают:

```text
в таком-то graph-е
в таком-то shared-memory ring-е
готов такой-то slot
с такой-то sequence
```

Дальше core-side backend/proxy должен:

```text
1. найти shared-memory ring по ring_id
2. найти slot по slot_id
3. прочитать metadata/payload из slot-а
4. доставить VideoFrame/AudioBuffer в IFrameSink
5. освободить slot / отправить release, если такая логика есть в следующем protocol layer
```

---

##### Stats structures

Файл содержит несколько уровней MTL stats.

```c++
struct MtlWorkerRxPortStats;
```

Session/port-level RX counters:

```text
packets
bytes
frames
incomplete_frames
err_packets
out_of_order_packets
```

```c++
struct MtlWorkerDeviceRxPortStats;
```

Device-level MTL port counters:

```text
rx_packets
rx_bytes
rx_err_packets
rx_hw_dropped_packets
rx_nombuf_packets
```

```c++
struct MtlWorkerSt20RxUserStats;
```

Расширенный набор ST20 RX user/session counters.

Он содержит MTL/ST20-specific counters: wrong SSRC/PT drops, dropped frames, missed packets, redundant drops, interlace stats, burst stats и другие поля, которые относятся к MTL video RX internals.

---

##### Stats event

```c++
struct MtlWorkerStatsEvent {
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;

    ...
};
```

`MtlWorkerStatsEvent` — общий stats snapshot graph-а.

Он содержит:

```text
video counters
audio counters
MTL session counters
MTL device port counters
core-side delivery/event counters
```

Video group включает:

```text
video_frames_received
video_frames_dropped
video_frame_packets_total
video_reconstructed_frames
video_corrupted_frames
video_complete_frames
video_session_* counters
video_st20_rx
```

Audio group включает:

```text
audio_blocks_received
audio_blocks_dropped
audio_block_bytes_received
audio_block_packets_total
audio_session_* counters
```

Device stats group включает:

```text
mtl_primary_port
mtl_redundant_port
mtl_port_stats_query_failures
```

Delivery/event group включает:

```text
frame_ready_events
audio_block_ready_events
video_frames_delivered
audio_blocks_delivered
released_slots
malformed_ready_events
stale_ready_events
delivery_failures
release_failures
ignored_events
```

Поля `*_stats_available` показывают, какие группы статистики доступны.

---

##### Health event

```c++
struct MtlWorkerHealthEvent {
    MtlWorkerRequestId request_id;
    bool healthy;
    std::string message;
};
```

Возвращает health status worker-а.

`message` содержит diagnostic text, если worker unhealthy или хочет передать дополнительное состояние.

---

#### `mtl_worker_shared_memory_ring.hpp` / `mtl_worker_shared_memory_ring.cpp`

Содержит shared-memory ring abstraction для передачи готовых video frames и audio blocks между MTL worker process и основным процессом.

Файл описывает низкоуровневый layout и state-machine shared-memory slots:

```text
worker process
→ writes media payload into shared-memory slot
→ publishes slot as Ready
→ sends FrameReady / AudioBlockReady event
→ core process reads slot
→ releases slot back to Empty
```

---

##### Slot state model

```c++
enum class MtlWorkerSharedMemorySlotState {
    Empty,
    Writing,
    Ready,
    Reading,
};
```

Каждый slot проходит простой lifecycle:

```text
Empty - slot свободен, worker может начать запись
→ Writing - worker занял slot и записывает payload/metadata
→ Ready - slot опубликован, core process может читать его после ready event-а
→ Reading - core process занял slot для чтения
→ Empty - slot свободен, worker может начать запись
```

Состояние хранится в shared memory и меняется через atomic operations.

---

##### Slot flags

```c++
enum class MtlWorkerSharedMemorySlotFlags {
    None,
    Partial,
};
```

`Partial` означает, что media unit был доставлен не полностью.

Для video это соответствует partial/incomplete frame.

Для audio сейчас обычно ожидается complete block, но flag остается общим для media slot-а.

---

##### Video field flags

```c++
enum class MtlWorkerVideoFieldFlags {
    None,
    Interlaced,
    SecondField,
};
```

Эти flags используются в slot metadata для video.

Они позволяют передать downstream-слою информацию о field semantics:

```text
progressive frame
interlaced field
second field
```

---

##### Slot media metadata

```c++
struct MtlWorkerSharedMemorySlotMediaMetadata {
    MtlWorkerMediaKind media_kind;

    std::uint32_t media_format;

    std::uint32_t width;
    std::uint32_t height;

    std::uint32_t sample_rate_hz;
    std::uint32_t channels;
    std::uint32_t samples_per_channel;

    std::uint32_t rtp_timestamp;
    TimestampNs receive_timestamp_ns;

    std::uint32_t plane_count;

    std::uint32_t video_scan_mode;
    std::uint32_t video_field_flags;

    std::uint64_t plane_offset_bytes[4];
    std::uint64_t plane_size_bytes[4];
    std::uint64_t plane_line_size_bytes[4];
};
```

`MtlWorkerSharedMemorySlotMediaMetadata` описывает media payload, лежащий в slot-е.

Для video используются:

```text
media_kind = Video
media_format = static_cast<uint32_t>(PixelFormat)
width / height
plane_count
plane_offset_bytes
plane_size_bytes
plane_line_size_bytes
video_scan_mode
video_field_flags
rtp_timestamp
receive_timestamp_ns
```

Для audio используются:

```text
media_kind = Audio
media_format = static_cast<uint32_t>(MtlAudioPcmFormat)
sample_rate_hz
channels
samples_per_channel
rtp_timestamp
receive_timestamp_ns
```

Plane metadata позволяет описывать как single-plane, так и multi-plane media layout внутри одного slot payload-а.

---

##### Slot header

```c++
struct alignas(64) MtlWorkerSharedMemorySlotHeader {
    std::uint32_t magic;
    std::uint32_t layout_version;

    std::uint32_t state;
    std::uint32_t flags;

    std::uint64_t sequence;
    std::uint64_t payload_size;

    MtlWorkerSharedMemorySlotMediaMetadata media;
};
```

Каждый slot начинается с fixed header-а.

Header хранит:

- magic value `MTLS`; (MTLS - mtlWorkerSharedMemorySlotMagic = 0x4D544C53 - magic value для проверки, что в начале shared-memory slot-а действительно лежит ожидаемый header проекта. Значение - ASCII-последовательность)
- layout version;
- slot state;
- slot flags;
- sequence number;
- payload size;
- media metadata.

Ready events не несут media metadata inline. Event сообщает только routing information:

```text
graph_id
ring_id
slot_id
sequence
```

А вся media/layout информация находится в `MtlWorkerSharedMemorySlotHeader`.

---

##### Ring map

```c++
class MtlWorkerSharedMemoryRingMap;
```

`MtlWorkerSharedMemoryRingMap` представляет mapped shared-memory ring.

Он не создает shared memory сам. Он получает descriptor и file descriptor, проверяет их и делает `mmap`.

Основной entry point:

```c++
static std::expected<MtlWorkerSharedMemoryRingMap, Error>
map_from_descriptor(
    const MtlWorkerSharedMemoryRingDescriptor &descriptor,
    int fd
);
```

Pipeline:

```text
MtlWorkerSharedMemoryRingDescriptor
+ fd
→ validate descriptor
→ validate fd size
→ mmap
→ MtlWorkerSharedMemoryRingMap
```

RAII destructor вызывает `munmap`.

Copy запрещен, move разрешен.

---

##### Descriptor validation

```c++
std::expected<bool, Error>
validate_mtl_worker_shared_memory_ring_descriptor_for_mapping(
    const MtlWorkerSharedMemoryRingDescriptor &descriptor
);
```

Проверяет, что descriptor безопасен для mapping-а и обращения к slots.

---

##### Slot access

```c++
std::expected<MtlWorkerSharedMemorySlotHeader *, Error>
slot_header(std::uint32_t slot_index);

std::expected<std::span<std::byte>, Error>
slot_payload(std::uint32_t slot_index);
```

`slot_header` возвращает pointer на header slot-а.

`slot_payload` возвращает span на payload area slot-а.

Оба метода проверяют:

```text
ring mapped
slot_index < slot_count
computed offsets do not overflow
computed ranges fit into mapping_size
```

---

##### Slot initialization

```c++
std::expected<bool, Error>
initialize_slot_headers();

std::expected<bool, Error>
validate_initialized_slot_headers() const;
```

`initialize_slot_headers` подготавливает все slots:

```text
magic = MTLS
layout_version = current layout version
flags = None
sequence = 0
payload_size = 0
media metadata reset
state = Empty
```

`validate_initialized_slot_headers` проверяет, что уже mapped ring содержит корректные slot headers.

---

##### Worker write path

Worker-side write sequence выглядит так:

```text
begin_write_slot(slot)
→ copy media payload into slot_payload(slot)
→ publish_written_slot(slot, payload_size, sequence, flags, metadata)
→ send FrameReady / AudioBlockReady event
```

Основные методы:

```c++
std::expected<bool, Error>
begin_write_slot(std::uint32_t slot_index);
```

Пытается перевести slot:

```text
Empty → Writing
```

Если slot не `Empty`, метод возвращает `false`.

```c++
std::expected<bool, Error>
publish_written_slot(
    std::uint32_t slot_index,
    std::uint64_t payload_size,
    std::uint64_t sequence,
    std::uint32_t flags,
    const MtlWorkerSharedMemorySlotMediaMetadata &metadata
);
```

Публикует записанный slot:

```text
Writing → Ready
```

После записи `payload_size`, `sequence`, `flags` и `media` состояние atomically публикуется как `Ready`.

Если worker начал запись, но не может ее завершить, используется:

```c++
std::expected<bool, Error>
abort_write_slot(std::uint32_t slot_index);
```

Он возвращает slot из `Writing` обратно в `Empty`.

---

##### Core read path

Core-side read sequence после ready event-а выглядит так:

```text
receive FrameReady / AudioBlockReady event
→ find ring by ring_id
→ begin_read_slot_if_matches(slot_id, sequence)
→ read header + payload
→ deliver VideoFrame / AudioBuffer
→ release_read_slot(slot_id)
```

Основной метод:

```c++
std::expected<MtlWorkerSharedMemoryBeginReadResult, Error>
begin_read_slot_if_matches(
    std::uint32_t slot_index,
    std::uint64_t expected_sequence
);
```

Он проверяет:

```text
slot state == Ready
header.sequence == expected_sequence
metadata валидна
```

и затем переводит slot:

```text
Ready → Reading
```

Результаты:

```text
Acquired:
    slot успешно занят для чтения

NotReady:
    slot еще не Ready или уже занят/освобожден

Stale:
    sequence в slot-е не совпал с sequence из event-а
```

`Stale` нужен для защиты от устаревших ready events, когда slot уже был переиспользован.

После чтения core-side код вызывает:

```c++
std::expected<bool, Error>
release_read_slot(std::uint32_t slot_index);
```

Он очищает payload metadata и переводит slot:

```text
Reading → Empty
```

После этого worker снова может использовать slot для нового frame/block.

---

##### Metadata validation

Перед публикацией и перед чтением проверяется `MtlWorkerSharedMemorySlotMediaMetadata`.

Основные правила:

```text
media_kind должен быть Video или Audio
payload_size должен быть > 0 и <= payload capacity
plane_count должен быть 1..4
каждый plane должен иметь ненулевой size
plane range должен помещаться в payload_size
video metadata должна иметь width/height
audio metadata должна иметь sample_rate_hz/channels/samples_per_channel
```

---

#### `mtl_worker_shared_memory_ring_owner.hpp` / `mtl_worker_shared_memory_ring_owner.cpp`

Содержит OBS/core-process-side owner для shared-memory media ring-а.

Если `mtl_worker_shared_memory_ring.hpp/.cpp` описывает уже mapped ring и slot state-machine, то этот файл отвечает за создание и владение shared-memory объектом:

```text
core process
→ create memfd
→ size/truncate memory object
→ mmap as MtlWorkerSharedMemoryRingMap
→ initialize slot headers
→ pass descriptor + duplicated fd to worker
```

Worker не владеет этим объектом напрямую. Worker получает только:

```text
MtlWorkerSharedMemoryRingDescriptor
+ borrowed/duplicated fd through SCM_RIGHTS
```

---

##### Owner config

```c++
struct MtlWorkerSharedMemoryRingOwnerConfig {
    MtlWorkerSharedMemoryRingId ring_id;
    MtlWorkerMediaKind media_kind;

    std::uint32_t fd_index;

    std::uint32_t slot_count;
    std::uint64_t slot_payload_capacity_bytes;

    std::string debug_name;
};
```

`MtlWorkerSharedMemoryRingOwnerConfig` задает параметры создаваемого ring-а.

Основные поля:

- `ring_id` — logical ID ring-а в worker protocol-е;
- `media_kind` — video или audio;
- `fd_index` — индекс fd в ancillary fd vector, который прикладывается к `StartSessions`;
- `slot_count` — количество slots в ring-е;
- `slot_payload_capacity_bytes` — максимальный payload size одного slot-а;
- `debug_name` — человекочитаемое имя `memfd`, используется только для диагностики.

`fd_index` важен, потому что сам descriptor не содержит fd. Он содержит только индекс, по которому worker найдет соответствующий fd среди переданных через IPC file descriptors.

---

##### Descriptor construction

```c++
std::expected<MtlWorkerSharedMemoryRingDescriptor, Error>
make_mtl_worker_shared_memory_ring_descriptor(
    const MtlWorkerSharedMemoryRingOwnerConfig &cfg
) noexcept;
```

Функция строит `MtlWorkerSharedMemoryRingDescriptor` из owner config-а.

Она вычисляет layout shared-memory ring-а:

```text
slot header
→ aligned payload offset
→ payload capacity
→ aligned slot stride
→ mapped size = slot_count * slot_stride
```

Descriptor заполняет:

```text
ring_id
media_kind
fd_index
layout_version
mapped_size_bytes
slot_region_offset_bytes
slot_count
slot_stride_bytes
slot_payload_offset_bytes
slot_payload_capacity_bytes
```

---

##### Ring owner

```c++
class MtlWorkerSharedMemoryRingOwner final;
```

`MtlWorkerSharedMemoryRingOwner` владеет двумя ресурсами:

```text
1. memfd fd
2. mmap mapping через MtlWorkerSharedMemoryRingMap
```

Copy запрещен, move разрешен.

Destructor вызывает:

```text
munmap
close(fd)
```

через `close_noexcept`.

---

##### Создание ring-а

Основной entry point:

```c++
static std::expected<MtlWorkerSharedMemoryRingOwner, Error>
create(const MtlWorkerSharedMemoryRingOwnerConfig &cfg);
```

Pipeline создания:

```text
1. make_mtl_worker_shared_memory_ring_descriptor(cfg)
2. memfd_create(debug_name)
3. ftruncate(fd, descriptor.mapped_size_bytes)
4. MtlWorkerSharedMemoryRingMap::map_from_descriptor(descriptor, fd)
5. initialize_slot_headers()
6. return MtlWorkerSharedMemoryRingOwner
```

После successful `create` у core process есть:

```text
valid fd
valid mmap
initialized slot headers
descriptor ready for worker protocol
```

---

##### Resource access

```c++
bool valid() const noexcept;
int fd() const noexcept;

const MtlWorkerSharedMemoryRingDescriptor &descriptor() const noexcept;

MtlWorkerSharedMemoryRingMap &ring_map() noexcept;
const MtlWorkerSharedMemoryRingMap &ring_map() const noexcept;
```

`fd()` возвращает owned memfd. Этот fd должен быть duplicated/передан worker-у через IPC file descriptor passing.

`descriptor()` возвращает descriptor, который помещается в:

```c++
MtlWorkerStartSessionsRequest::media_rings
```

`ring_map()` дает core-side доступ к mapped ring-у. Core process использует его для чтения slots после `FrameReady` / `AudioBlockReady` events и для release slot-а обратно в `Empty`.

---

##### Close

```c++
void close_noexcept() noexcept;
```

Освобождает оба ресурса:

```text
ring_map_.unmap_noexcept()
close(fd_)
```

Метод безопасен для повторного вызова.

---

#### `mtl_worker_ipc_framing.hpp` / `mtl_worker_ipc_framing.cpp`

Содержит низкоуровневый framing layer для control IPC между основным процессом и MTL worker process.

Этот файл работает только с byte payload-ами и optional Unix file descriptors:

```text
typed request/event
→ serialized bytes
→ IPC frame
→ Unix socket / process channel
→ IPC frame
→ serialized bytes
→ typed request/event
```

---

##### Frame format

Базовый byte frame имеет простой формат:

```text
uint32 big-endian payload_size
payload_size bytes
```

То есть каждый control message в IPC stream-е передается как length-prefixed payload.

`payload_size` кодируется в Big-Endian порядке, чтобы framing не зависел от host endianess.

---

##### File descriptor passing

Fd-aware API использует тот же byte frame, но дополнительно может attached file descriptors через Unix ancillary data:

```text
payload bytes
+ optional SCM_RIGHTS file descriptors
```

Это нужно для shared-memory media rings:

```text
core process
→ creates memfd
→ sends descriptor in StartSessions payload
→ sends actual fd through SCM_RIGHTS
→ worker maps fd using descriptor
```

То есть descriptor передается как обычные serialized bytes, а сам fd передается out-of-band через Unix socket ancillary data.

---

##### Limits

```c++
inline constexpr std::uint32_t defaultMtlWorkerMaxControlFrameBytes = 1024 * 1024;
inline constexpr std::size_t defaultMtlWorkerMaxAncillaryFileDescriptors = 64;
```

Framing layer ограничивает:

```text
max payload size = 1 MiB
max attached fds = 64
```

Эти лимиты защищают control channel от некорректных или поврежденных frame-ов.

---

##### `MtlWorkerIpcFrame`

```c++
class MtlWorkerIpcFrame final;
```

`MtlWorkerIpcFrame` — результат чтения одного IPC frame-а.

Он содержит:

```text
payload bytes
owned received file descriptors
```

Важная особенность: полученные через `SCM_RIGHTS` descriptors принадлежат `MtlWorkerIpcFrame`.

Если caller не забрал их через:

```c++
release_file_descriptors()
```

то destructor закроет их автоматически.

Это защищает от fd leaks на error paths.

Основные методы:

```c++
const std::vector<std::uint8_t> &payload() const;
std::vector<std::uint8_t> release_payload();

const std::vector<int> &file_descriptors() const;
bool has_file_descriptors() const;

std::vector<int> release_file_descriptors();
```

`release_payload()` и `release_file_descriptors()` передают ownership caller-у.

---

##### Payload-only API

```c++
std::expected<bool, Error>
write_mtl_worker_control_frame(
    int fd,
    std::span<const std::uint8_t> payload
);
```

Записывает обычный frame без attached descriptors:

```text
4-byte payload size
payload bytes
```

```c++
std::expected<std::vector<std::uint8_t>, Error>
read_mtl_worker_control_frame(
    int fd,
    std::uint32_t max_frame_bytes = defaultMtlWorkerMaxControlFrameBytes
);
```

Читает frame без descriptors.

Если входящий frame содержит ancillary file descriptors, payload-only reader возвращает:

```c++
Error::InvalidValue
```

Для сообщений, где descriptors ожидаются, нужно использовать fd-aware API.

---

##### Fd-aware API

```c++
std::expected<bool, Error>
write_mtl_worker_control_frame_with_fds(
    int fd,
    std::span<const std::uint8_t> payload,
    std::span<const int> file_descriptors
);
```

Записывает frame и может attached descriptors через `SCM_RIGHTS`.

Важно: переданные descriptors только borrowed.

```text
caller owns fd before and after call
```

Функция не закрывает их и не забирает ownership.

Если descriptors есть, первый `sendmsg` отправляет header + payload + SCM_RIGHTS. Если byte stream write оказался partial, оставшиеся bytes досылаются уже без descriptors, потому что fd нельзя отправлять повторно для того же logical frame.

```c++
std::expected<MtlWorkerIpcFrame, Error>
read_mtl_worker_control_frame_with_fds(
    int fd,
    std::uint32_t max_frame_bytes = defaultMtlWorkerMaxControlFrameBytes,
    std::size_t max_file_descriptors = defaultMtlWorkerMaxAncillaryFileDescriptors
);
```

Читает frame и возвращает:

```text
payload
owned fds
```

Полученные fds получают `FD_CLOEXEC`.

Если descriptors больше допустимого лимита, лишние descriptors закрываются, а функция возвращает ошибку.

---

##### Read/write behavior

Write path:

```text
validate fd/payload/fds
→ encode 4-byte big-endian payload size
→ write header
→ write payload
→ optionally attach fds with SCM_RIGHTS
```

Read path:

```text
read 4-byte big-endian payload size
→ validate payload size
→ read payload bytes
→ collect optional SCM_RIGHTS fds
→ return MtlWorkerIpcFrame
```

---

Главная причина существования этого слоя — отделить:

```text
typed protocol serialization
```

от:

```text
length-prefixed byte transport
+ file descriptor passing
```

---

#### `mtl_worker_ipc_codec.hpp` / `mtl_worker_ipc_codec.cpp`

Содержит typed binary codec для MTL worker control protocol.

Если `mtl_worker_ipc_framing.hpp/.cpp` работает только с length-prefixed byte payload и optional file descriptors, то этот файл преобразует typed protocol structures в byte payload и обратно:

```text
MtlWorkerControlRequest / MtlWorkerControlEvent
→ binary payload
→ mtl_worker_ipc_framing
→ Unix socket
→ mtl_worker_ipc_framing
→ binary payload
→ MtlWorkerControlRequest / MtlWorkerControlEvent
```

---

##### Основные public functions

```c++
std::expected<std::vector<std::uint8_t>, Error>
serialize_mtl_worker_control_request(
    const MtlWorkerControlRequest &request
);
```

Преобразует typed request в byte payload.

```c++
std::expected<MtlWorkerControlRequest, Error>
deserialize_mtl_worker_control_request(
    std::span<const std::uint8_t> payload
);
```

Преобразует byte payload обратно в typed request.

```c++
std::expected<std::vector<std::uint8_t>, Error>
serialize_mtl_worker_control_event(
    const MtlWorkerControlEvent &event
);
```

Преобразует typed event в byte payload.

```c++
std::expected<MtlWorkerControlEvent, Error>
deserialize_mtl_worker_control_event(
    std::span<const std::uint8_t> payload
);
```

Преобразует byte payload обратно в typed event.

---

##### Binary message tags

Каждый serialized payload начинается с `MessageTag`.

Request tags:

```text
1 → ConfigHandshakeRequest
2 → StartSessionsRequest
3 → StopSessionsRequest
4 → ShutdownRequest
5 → StatsRequest
6 → HealthCheckRequest
```

Event tags:

```text
101 → HealthEvent
102 → ErrorEvent
103 → StartedEvent
104 → StoppedEvent
105 → StatsEvent
106 → FrameReadyEvent
107 → AudioBlockReadyEvent
```

Tag определяет, какую concrete structure нужно читать дальше.

---

##### Wire encoding

Codec использует простой binary format:

```text
u8      → 1 byte
u16     → Big-Endian
u32     → Big-Endian
u64     → Big-Endian
bool    → u8, только 0 или 1
string  → u32 length + raw bytes
IPv4    → 4 raw bytes
optional value → u8 presence flag + value if present
```

То есть codec не зависит от host endianess.

Внутри реализации для этого используются `Writer` и `Reader`.

`Writer` последовательно добавляет primitive values в `std::vector<std::uint8_t>`.

`Reader` последовательно читает primitive values из `std::span<const std::uint8_t>`.

---

##### Request serialization

Codec поддерживает serialization/deserialization для всех worker requests:

```text
ConfigHandshakeRequest:
    tag
    request_id
    runtime config

StartSessionsRequest:
    tag
    request_id
    graph_id
    optional video start config
    optional audio start config
    shared-memory ring descriptors

StopSessionsRequest:
    tag
    request_id
    graph_id

StatsRequest:
    tag
    request_id
    graph_id

HealthCheckRequest:
    tag
    request_id

ShutdownRequest:
    tag
    request_id
```

Для `StartSessionsRequest` дополнительно проверяется:

```text
video или audio session должна быть задана
media_rings должны быть валидны
каждый media ring должен соответствовать имеющейся session по media_kind
ring_id не должны дублироваться
```

То есть video ring без video session или audio ring без audio session считается invalid protocol payload.

---

##### Start config encoding

Codec умеет кодировать MTL runtime/video/audio configs, подготовленные предыдущими projection layers.

Для runtime config передаются:

```text
primary port name
primary SIP address
optional redundant port
```

Для video start config передаются:

```text
runtime
primary session port
optional redundant session port
expected payload type
width / height
fps enum
scan mode enum
transport format enum
output PixelFormat enum
frame buffer count
```

Для audio start config передаются:

```text
runtime
primary session port
optional redundant session port
expected payload type
AudioMediaDescription
samples_per_packet
MTL PCM format enum
MTL sampling enum
MTL packet time enum
frame buffer count
frame buffer duration ns
```

Shared-memory ring descriptors передаются как обычные typed fields внутри payload. Actual file descriptors к этим descriptors передаются не здесь, а через fd-aware framing layer с `SCM_RIGHTS`.

---

##### Event serialization

Codec поддерживает serialization/deserialization для worker events:

```text
HealthEvent:
    request_id
    healthy
    message

ErrorEvent:
    request_id
    graph_id
    error
    message

StartedEvent:
    request_id
    graph_id

StoppedEvent:
    request_id
    graph_id

StatsEvent:
    full stats snapshot

FrameReadyEvent:
    graph_id
    ring_id
    slot_id
    sequence

AudioBlockReadyEvent:
    graph_id
    ring_id
    slot_id
    sequence
```

`FrameReadyEvent` и `AudioBlockReadyEvent` не содержат media payload. Они только передают routing metadata:

```text
graph_id
ring_id
slot_id
sequence
```

По этим значениям core-side proxy later находит shared-memory ring и slot.

---

##### Stats encoding

`StatsEvent` кодируется как большой fixed-order набор counters.

Он включает:

```text
graph-level video/audio counters
video session stats
extended ST20 RX stats
audio session stats
MTL device port stats
core-side delivery/event counters
```

Поля `*_stats_available` кодируются как bool и показывают, доступны ли соответствующие группы статистики.

Важно: порядок полей в `StatsEvent` является частью binary protocol. Если fields меняются, нужно синхронно менять writer и reader и учитывать compatibility/versioning.

---

#### `mtl_worker_control_channel.hpp` / `mtl_worker_control_channel.cpp`

Содержит typed control-channel interface для общения основного процесса с MTL worker process.

Если предыдущие слои разделены так:

```text
mtl_worker_protocol.hpp
    typed request/event structures

mtl_worker_ipc_codec.hpp/.cpp
    typed request/event ↔ byte payload

mtl_worker_ipc_framing.hpp/.cpp
    byte payload + optional fds ↔ IPC frame
```

то `mtl_worker_control_channel.hpp/.cpp` задает уже удобную high-level границу:

```text
core-side code
→ IMtlWorkerControlChannel
→ worker process
```

Этот файл не реализует конкретный Unix socket transport. Он задает interface, через который higher-level MTL graph client / worker manager / proxy backend будут отправлять requests worker-у и получать events.

---

##### Event envelope

```c++
struct MtlWorkerControlEventEnvelope {
    MtlWorkerControlEvent event;
    MtlWorkerIpcFrame ipc_frame;
};
```

`MtlWorkerControlEventEnvelope` связывает typed event и transport frame, из которого этот event был получен.

Это нужно потому, что один IPC frame может содержать:

```text
serialized MtlWorkerControlEvent
+ optional ancillary file descriptors
```

Typed event хранится в поле:

```c++
event
```

а полученные file descriptors остаются во владении:

```c++
ipc_frame
```

Если higher-level component должен забрать fd ownership, он вызывает:

```c++
release_file_descriptors()
```

Если не заберет — descriptors будут закрыты destructor-ом `MtlWorkerIpcFrame`.

---

##### Async event handler

```c++
using MtlWorkerAsyncEventHandler =
    std::function<void(MtlWorkerControlEventEnvelope)>;
```

Callback для asynchronous worker events.

Сейчас ожидаемые async events:

```text
MtlWorkerFrameReadyEvent
MtlWorkerAudioBlockReadyEvent
```

То есть worker может прислать событие не как прямой response на `transact()`, а как media-ready notification:

```text
worker session received frame/block
→ wrote shared-memory slot
→ sent FrameReady/AudioBlockReady event
→ control channel routes event to registered graph handler
```

Handler получает envelope move-only, потому что он может владеть descriptors.

---

##### `IMtlWorkerControlChannel`

```c++
class IMtlWorkerControlChannel;
```

Это core-side interface control channel-а к MTL worker process.

Он не содержит MTL API types и не владеет MTL runtime state.

Concrete implementation может быть построена на Unix domain socket, pipe или другом IPC transport-е.

---

##### Payload-only transaction API

```c++
std::expected<MtlWorkerControlEvent, Error>
transact(const MtlWorkerControlRequest &request);
```

Compatibility helper для request/response exchange без file descriptors.

В реализации base class вызывает:

```c++
transact_with_fds(request, {})
```

и затем проверяет, что response не содержит descriptors.

Если response принес fds, возвращается:

```c++
Error::InvalidValue
```

То есть `transact()` подходит только для обычных control messages:

```text
HealthCheck
Stats
Stop
Shutdown
```

когда fd passing не является частью контракта.

---

##### Fd-aware transaction API

```c++
virtual std::expected<MtlWorkerControlEventEnvelope, Error>
transact_with_fds(
    const MtlWorkerControlRequest &request,
    std::span<const int> file_descriptors
) = 0;
```

Основной transaction API.

Он отправляет typed request worker-у и возвращает typed response envelope.

`file_descriptors` передаются как borrowed descriptors:

```text
caller владеет fd до вызова
caller продолжает владеть fd после вызова
```

Response descriptors, если worker их вернул, принадлежат `MtlWorkerControlEventEnvelope`.

Этот API нужен для сообщений, где рядом с typed payload нужно передать fd через IPC. Например:

```text
StartSessionsRequest
+ shared-memory ring descriptors in payload
+ memfd descriptors through SCM_RIGHTS
```

---

##### Async event registration

```c++
virtual std::expected<bool, Error>
register_async_event_handler(
    MtlWorkerGraphId graph_id,
    MtlWorkerAsyncEventHandler handler
) = 0;

virtual void unregister_async_event_handler_noexcept(
    MtlWorkerGraphId graph_id
) noexcept = 0;
```

Эти методы задают graph-scoped routing asynchronous events.

Идея:

```text
FrameReadyEvent / AudioBlockReadyEvent contains graph_id
→ control channel reader thread receives event
→ finds handler by graph_id
→ invokes handler
```

Handler вызывается из reader thread-а control channel-а, поэтому он не должен выполнять долгую media processing работу. Его задача — быстро передать событие дальше в graph/proxy delivery path.

---

##### Health

```c++
virtual bool healthy() const noexcept = 0;
```

Возвращает состояние control channel-а.

Для реальной реализации это может означать:

```text
worker process alive
IPC socket alive
reader thread alive
no fatal protocol error
```

В этом interface конкретная семантика остается за implementation.

---

##### Unsupported skeleton channel

```c++
std::shared_ptr<IMtlWorkerControlChannel>
create_unsupported_mtl_worker_control_channel();
```

Файл содержит temporary skeleton implementation:

```c++
UnsupportedMtlWorkerControlChannel
```

Она нужна, чтобы OBS/core-side code уже зависел от правильной control-channel abstraction, даже если реальный IPC transport еще не доступен.

Поведение skeleton-а:

```text
transact_with_fds(...)
    → Error::Unsupported

register_async_event_handler(...)
    → Error::Unsupported

unregister_async_event_handler_noexcept(...)
    → no-op

healthy()
    → false
```

---

#### `mtl_worker_process_control_channel.hpp` / `mtl_worker_process_control_channel.cpp`

Содержит concrete OBS/core-process-side implementation of `IMtlWorkerControlChannel`, backed by a real MTL worker process.

Этот файл связывает уже описанные protocol/codec/framing layers с отдельным worker process-ом:

```text
core process
→ MtlWorkerProcessControlChannel
→ fork/exec worker process
→ Unix socketpair
→ length-prefixed typed IPC frames
→ worker process
```

Главная задача файла — держать MTL API ownership вне OBS process-а. OBS/core-side код общается с worker-ом через typed control requests/events, а MTL runtime/session живет в worker process-е.

---

##### Общая роль

`MtlWorkerProcessControlChannel` реализует:

```text
1. lazy worker process startup
2. Unix socketpair creation
3. request serialization + framed write
4. response/event read thread
5. request_id-based synchronous response routing
6. graph_id-based asynchronous event routing
7. worker shutdown/termination
8. channel health state
```

---

##### Worker process startup

```c++
explicit MtlWorkerProcessControlChannel(
    std::filesystem::path worker_executable_path
);
```

Constructor сохраняет путь к worker executable, но worker стартует не сразу.

Worker запускается лениво при первом `transact_with_fds(...)` через internal `ensure_worker_started()`.

Startup pipeline:

```text
1. Проверить, что channel не shutdown и reader не failed
2. Если worker уже started — использовать существующий process/socket
3. Создать Unix socketpair(AF_UNIX, SOCK_STREAM)
4. fork()
5. child:
   - dup2(worker socket, STDIN_FILENO)
   - dup2(worker socket, STDOUT_FILENO)
   - execlp(worker executable)
   - stderr остается diagnostic-only
6. parent:
   - закрыть child-side fd
   - сохранить worker_pid
   - сохранить worker_control_fd
   - настроить send timeout
   - запустить reader thread
```

Идея в том, что worker получает control IPC через стандартные потоки:

```text
stdin  ← framed requests
stdout → framed events/responses
stderr → diagnostics/logging
```

---

##### `transact_with_fds`

```c++
std::expected<MtlWorkerControlEventEnvelope, Error>
transact_with_fds(
    const MtlWorkerControlRequest &request,
    std::span<const int> file_descriptors
) override;
```

Основной synchronous request/response API.

Pipeline:

```text
1. ensure_worker_started()
2. извлечь request_id из typed request
3. request_id должен быть != 0
4. serialize_mtl_worker_control_request(request)
5. write_mtl_worker_control_frame_with_fds(...)
6. wait_for_response(request_id, timeout_for_request(request))
7. вернуть MtlWorkerControlEventEnvelope
```

`file_descriptors` передаются как borrowed descriptors. Ownership остается у caller-а.

Это нужно для `StartSessionsRequest`, где typed payload содержит shared-memory ring descriptors, а actual memfd descriptors передаются через `SCM_RIGHTS`.

---

##### Request timeouts

Для разных request types используются разные timeouts:

```text
ConfigHandshakeRequest → 30 s
StartSessionsRequest   → 30 s
StopSessionsRequest    → 5 s
StatsRequest           → 2 s
HealthCheckRequest     → 2 s
ShutdownRequest        → 5 s
```

Если response не пришел за timeout, channel помечается failed и caller получает:

```c++
Error::OperationAborted
```

Это важно: timeout считается fatal для текущего control channel-а, потому что после потери request/response ordering безопасно продолжать тот же channel нельзя.

---

##### Reader thread

После запуска worker-а создается reader thread:

```text
reader_loop_noexcept(fd)
```

Он непрерывно читает worker output:

```text
read_mtl_worker_control_frame_with_fds
→ deserialize_mtl_worker_control_event
→ route event
```

Routing зависит от `request_id`.

Для обычных response events:

```text
request_id != 0
→ completed_responses[request_id] = envelope
→ notify waiters
```

Для asynchronous media-ready events:

```text
request_id == 0
→ dispatch_async_event_noexcept(envelope)
```

`FrameReadyEvent` и `AudioBlockReadyEvent` имеют `request_id == 0`, поэтому они не считаются response-ами на `transact()`.

---

##### Async event routing

```c++
std::expected<bool, Error>
register_async_event_handler(
    MtlWorkerGraphId graph_id,
    MtlWorkerAsyncEventHandler handler
) override;

void unregister_async_event_handler_noexcept(
    MtlWorkerGraphId graph_id
) noexcept override;
```

Async handlers регистрируются по `graph_id`.

Pipeline async event-а:

```text
worker sends FrameReady / AudioBlockReady
→ reader thread receives event
→ graph_id extracted from event
→ handler = async_event_handlers[graph_id]
→ handler(envelope)
```

Handler вызывается из reader thread-а. Поэтому он не должен выполнять долгую media processing работу. Его задача — быстро передать event дальше в graph/proxy delivery path.

Исключения из handler-а перехватываются внутри reader thread-а, чтобы один graph-level handler не убил весь control-channel reader.

---

##### Response envelope ownership

Reader thread сохраняет response как:

```c++
MtlWorkerControlEventEnvelope
```

Envelope содержит:

```text
typed MtlWorkerControlEvent
+ original MtlWorkerIpcFrame
```

Если response принес file descriptors, они остаются owned внутри `ipc_frame`, пока higher-level code не вызовет:

```c++
release_file_descriptors()
```

Если ownership не забрали, descriptors закроются автоматически.

---

##### Shutdown

```c++
void shutdown_noexcept() noexcept;
```

Shutdown sequence:

```text
1. shutdown_requested = true
2. notify waiters
3. shutdown/close worker_control_fd
4. join reader_thread
5. terminate worker process
6. clear completed responses
7. clear async handlers
8. mark reader_failed / OperationAborted
```

Worker termination выполняется так:

```text
SIGTERM
→ wait short grace timeout
→ SIGKILL if still alive
→ wait short grace timeout
```

Код специально не блокируется бесконечно: если child завис в uninterruptible kernel state, parent все равно инвалидирует channel и продолжает работу.

---

##### Health

```c++
bool healthy() const noexcept override;
```

`healthy()` возвращает `true`, только если:

```text
shutdown не requested
reader не failed
worker_pid > 0
worker_control_fd >= 0
```

До lazy startup worker еще не запущен, поэтому `healthy()` не должен трактоваться как “MTL backend вообще unavailable”. Это состояние означает именно runtime health уже поднятого process channel-а.

---

##### Factory

```c++
std::shared_ptr<IMtlWorkerControlChannel>
create_mtl_worker_process_control_channel(
    std::filesystem::path worker_executable_path
);
```

Создает concrete process-backed channel и возвращает его через interface type:

```text
MtlWorkerProcessControlChannel
→ shared_ptr<IMtlWorkerControlChannel>
```

Это позволяет worker manager / graph client зависеть только от `IMtlWorkerControlChannel`.

---

#### `mtl_worker_graph_client.hpp` / `mtl_worker_graph_client.cpp`

Содержит core-side graph client для MTL worker path.

Это первый слой, который связывает вместе уже описанные компоненты:

```text
MtlVideoStartConfig / MtlAudioStartConfig
→ MtlWorkerGraphClient
→ MtlWorkerManager
→ IMtlWorkerControlChannel
→ MtlWorkerStartSessionsRequest
→ worker process
→ shared-memory ready events
→ IFrameSink
```

`MtlWorkerGraphClient` управляет одним logical receive graph-ом на стороне основного процесса: конфигурирует video/audio sessions, получает worker lease, создает shared-memory rings, отправляет worker-у `StartSessions`, принимает async ready events и доставляет media в `IFrameSink`. Реализация client-а и async delivery state находятся в приложенном `.cpp`.

---

##### `MtlWorkerErrorDetail`

```c++
struct MtlWorkerErrorDetail {
    Error error;
    MtlWorkerRequestId request_id;
    MtlWorkerGraphId graph_id;

    std::string message;
    bool worker_side;
};
```

`MtlWorkerErrorDetail` хранит последнюю подробную ошибку graph client-а.

Поле `worker_side` разделяет два класса ошибок:

```text
worker_side = false:
    ошибка возникла на core/client side
    например: нет sink-а, не удалось подготовить rings, IPC transaction failed

worker_side = true:
    worker вернул ErrorEvent или unhealthy HealthEvent
```

---

##### Назначение `MtlWorkerGraphClient`

```c++
class MtlWorkerGraphClient final;
```

Один instance client-а соответствует одному `MtlWorkerGraphId`.

Client хранит:

```text
graph_id
optional video config
optional audio config
attached IFrameSink
worker lease
shared-memory async state
running state
last error detail
```

Он может быть video-only, audio-only или combined video+audio graph, но если video и audio заданы вместе, их `MtlRuntimeConfig` должен совпадать.

---

##### Configuration phase

```c++
std::expected<bool, Error> configure_video(MtlVideoStartConfig cfg);
std::expected<bool, Error> configure_audio(MtlAudioStartConfig cfg);
```

Эти методы задают video/audio конфигурацию до старта graph-а.

Правила:

```text
если graph уже running → InvalidBackendState

если video и audio заданы вместе,
их runtime config должен быть одинаковым
```

Runtime equality важна потому, что один worker graph должен подниматься на одном совместимом MTL runtime/device context-е. Нельзя объединить video config для одного MTL runtime и audio config для другого.

---

##### Sink attach/detach

```c++
std::expected<bool, Error> attach_sink(IFrameSink *sink);
void detach_sink_noexcept(IFrameSink *sink) noexcept;
```

`sink` — core-side delivery target, куда graph client будет отдавать готовые frames/blocks после shared-memory ready events.

`attach_sink` требует non-null pointer.

Если sink уже attached и это другой pointer, возвращается:

```c++
Error::InvalidBackendState
```

`detach_sink_noexcept` очищает sink и в основном state, и в async event state. Это нужно, чтобы async ready event не доставил frame/block в уже detached backend/sink.

---

##### Start pipeline

```c++
std::expected<bool, Error> start();
```

`start()` — основной orchestration method.

Порядок работы:

```text
1. Если graph уже running:
   - увеличить active_start_count
   - вернуть success

2. Проверить, что sink attached

3. Разрешить graph runtime config:
   - video runtime
   - audio runtime
   - проверить совместимость, если оба заданы

4. Запросить worker lease у MtlWorkerManager:
   acquire_or_spawn_compatible_worker_for_graph(runtime, graph_id)

5. Проверить, что lease содержит control_channel

6. Проверить worker health через HealthCheck request

7. Подготовить shared-memory media rings:
   - video ring, если есть video config
   - audio ring, если есть audio config

8. Создать StartSessions request:
   - request_id
   - graph_id
   - optional video config
   - optional audio config
   - media_rings descriptors

9. Создать async event state:
   - configs
   - sink
   - ring owners

10. Зарегистрировать async event handler по graph_id

11. Отправить StartSessions через control channel:
   - typed payload
   - media ring fds через SCM_RIGHTS

12. Интерпретировать response:
   - StartedEvent → success
   - ErrorEvent   → worker-side error
   - другое       → InvalidBackendState

13. Пометить graph running
```

Ключевой момент: shared-memory ring owners создаются на core side, а worker получает только descriptors и fds. Ring owners остаются жить в `MtlWorkerGraphClientAsyncState`, потому что именно core side потом читает slots и release-ит их.

---

##### Shared-memory ring preparation

Client создает media rings сам перед `StartSessions`.

Фиксированные ring IDs:

```text
video ring_id = 1
audio ring_id = 2
```

Slot count:

```text
max(2, frame_buffer_count)
```

Video payload capacity вычисляется через project `VideoFrame`:

```text
VideoFrame(width, height, output_format).size_bytes()
```

То есть video shared-memory slot должен вмещать один output frame в project `PixelFormat`.

Audio payload capacity вычисляется из:

```text
ceil(sample_rate_hz * frame_buffer_duration_ns / 1_000_000_000)
* channel_count
* bytes_per_sample
```

где `bytes_per_sample` зависит от `MtlAudioPcmFormat`:

```text
Pcm16 → 2 bytes
Pcm24 → 3 bytes
```

После создания owner-а client получает:

```text
descriptor → идет в StartSessionsRequest.media_rings
fd         → передается worker-у через SCM_RIGHTS
owner      → остается в async state для core-side чтения slots
```

---

##### Async event state

В `.cpp` есть internal structure:

```c++
MtlWorkerGraphClientAsyncState
```

Она владеет:

```text
graph_id
video/audio configs
sink pointer
media ring owners
async delivery counters
```

Именно она обрабатывает asynchronous worker events:

```text
MtlWorkerFrameReadyEvent
MtlWorkerAudioBlockReadyEvent
```

`MtlWorkerProcessControlChannel` получает event из reader thread-а и вызывает handler, зарегистрированный graph client-ом. Handler передает event в `MtlWorkerGraphClientAsyncState::handle_event_noexcept`.

---

##### FrameReady event pipeline

Video ready path:

```text
MtlWorkerFrameReadyEvent
→ validate graph_id / slot_id
→ find video ring by ring_id
→ begin_read_slot_if_matches(slot_id, sequence)
→ validate header/payload/media metadata
→ copy payload planes into VideoFrame
→ sink->on_video_frame(...)
→ release_read_slot(slot_id)
```

Client проверяет:

```text
event.graph_id == graph_id
slot_id fits uint32
ring exists and media_kind == Video
slot sequence matches event.sequence
payload is non-empty
media kind is Video
media_format == configured output_format
width/height match config
scan mode matches config
field flags are consistent with scan mode
plane metadata fits payload
plane stride >= active row bytes
```

При доставке video client создает новый `VideoFrame`, затем копирует plane data из shared-memory payload в frame row-by-row, учитывая `plane_line_size_bytes`.

shared-memory metadata поддерживает plane-aware layout. Core-side delivery не предполагает, что frame payload обязательно tightly packed без stride.

`FrameTimingMetadata` получает:

```text
rtp_timestamp
receive_timestamp_ns
video_scan_mode
video_second_field
```

---

##### AudioBlockReady event pipeline

Audio ready path:

```text
MtlWorkerAudioBlockReadyEvent
→ validate graph_id / slot_id
→ find audio ring by ring_id
→ begin_read_slot_if_matches(slot_id, sequence)
→ validate header/payload/media metadata
→ create AudioBuffer
→ convert interleaved PCM16/PCM24 BE to S32
→ sink->on_audio_frame(...)
→ release_read_slot(slot_id)
```

Client проверяет:

```text
media kind is Audio
media_format == configured MtlAudioPcmFormat
sample_rate_hz matches config
channel count matches config
samples_per_channel != 0
payload_size matches expected sample count * bytes_per_sample
```

Audio payload в shared memory рассматривается как interleaved PCM wire samples.

При delivery client конвертирует:

```text
PCM16 BE → signed int32, left-aligned by * 65536
PCM24 BE → signed int32, left-aligned by * 256
```

То есть дальше в `IFrameSink::on_audio_frame` уходит уже project `AudioBuffer` с `int32_t` samples, как и в Socket audio path.

---

##### Slot release rule

После успешной или неуспешной попытки delivery acquired slot освобождается:

```text
release_read_slot(slot_id)
```

Если release failed, увеличивается `release_failures`.

Это критично: core-side client не должен удерживать shared-memory slot после callback-а. Worker сможет переиспользовать slot только после перехода:

```text
Reading → Empty
```

---

##### Stats path

```c++
std::expected<MtlWorkerStatsEvent, Error> stats();
```

`stats()` делает не только `StatsRequest`.

Порядок:

```text
1. Проверить, что graph running и есть control channel
2. Выполнить HealthCheck
3. Отправить StatsRequest
4. Интерпретировать StatsEvent/ErrorEvent
5. Смерджить worker stats с local async delivery counters
```

Worker-side stats приходят из worker process-а.

Core-side async counters добавляются локально:

```text
frame_ready_events
audio_block_ready_events
video_frames_delivered
audio_blocks_delivered
released_slots
malformed_ready_events
stale_ready_events
delivery_failures
release_failures
ignored_events
```

---

##### Stop pipeline

```c++
std::expected<bool, Error> stop();
void stop_noexcept() noexcept;
```

`stop()` учитывает local start refcount:

```text
если graph не running:
    release graph/lease state and return success

если active_start_count > 1:
    decrement active_start_count and return success

иначе:
    send StopSessionsRequest
    expect StoppedEvent
    unregister async handler
    deactivate async state
    release graph in manager
    clear worker lease
```

`stop_noexcept()` force-ит graph-level shutdown:

```text
active_start_count = 1
stop()
```

Это нужно для destructor/failure cleanup, когда надо остановить graph независимо от количества предыдущих `start()` calls.

---

##### Worker manager interaction

Graph client сам не запускает worker process.

Он обращается к:

```c++
default_mtl_worker_manager()
```

для получения lease:

```text
acquire_or_spawn_compatible_worker_for_graph(runtime, graph_id)
```

После успешного acquire manager считает graph зарегистрированным за worker-ом.

На нормальном stop client вызывает:

```text
release_graph_noexcept(worker_id, graph_id)
```

При runtime/IPC/backend failure client вызывает:

```text
invalidate_worker_noexcept(worker_id)
```

Разделение важное:

```text
normal graph stop:
    release only this graph

fatal worker/channel/backend runtime error:
    invalidate whole worker
```

---

##### Request builders

```c++
std::expected<MtlWorkerStartSessionsRequest, Error>
make_start_sessions_request() const;

MtlWorkerStopSessionsRequest
make_stop_sessions_request() const;

std::expected<MtlWorkerStatsRequest, Error>
make_stats_request() const;
```

Эти методы создают typed worker requests с новым `request_id` и текущим `graph_id`.

`make_start_sessions_request()` сначала проверяет, что graph configured и video/audio runtime config совместимы. `media_rings` изначально пустой; в `start()` он заполняется descriptors из prepared rings перед отправкой worker-у.

---

##### Error handling model

Client различает несколько cleanup paths:

```text
release_graph_and_clear_lease_noexcept:
    используется при обычных ошибках graph setup/stop,
    когда worker не обязательно сломан

invalidate_worker_and_clear_lease_noexcept:
    используется при health/IPC/runtime ошибках,
    когда безопаснее считать worker unusable
```

`last_error_detail()` и `last_error_message()` возвращают последнюю записанную diagnostic ошибку.

---

#### `mtl_worker_manager.hpp` / `mtl_worker_manager.cpp`

Содержит OBS/core-process-side manager для MTL worker processes.

Задача `MtlWorkerManager` — выбрать, переиспользовать, создать, инвалидировать или завершить worker process, совместимый с нужным `MtlRuntimeConfig`.

---

##### Общая роль

Manager отвечает за process-level ownership MTL worker-ов:

```text
runtime config
→ compatible cached worker?
    yes → reuse worker
    no  → spawn new worker process/control channel
→ ConfigHandshake
→ WorkerLease
```

Worker-ы кэшируются по `MtlRuntimeConfig`.

Это важно, потому что MTL runtime/device initialization относится к worker process-у. Несколько receive graph-ов с одинаковым runtime config могут использовать один worker, а graph-ы с несовместимыми runtime config должны получить другой worker.

---

##### `WorkerLease`

```c++
struct WorkerLease {
    std::uint64_t worker_id;
    MtlRuntimeConfig runtime;
    std::shared_ptr<IMtlWorkerControlChannel> control_channel;
};
```

`WorkerLease` — handle, который manager возвращает graph client-у.

Он содержит:

- `worker_id` — internal ID worker-а в manager-е;
- `runtime` — runtime config, с которым worker был принят;
- `control_channel` — typed control channel к worker process-у.

Graph client дальше использует `control_channel` для:

```text
HealthCheck
StartSessions
StopSessions
Stats
Shutdown
```

---

##### Worker executable resolution

Default constructor пытается найти `st2110_mtl_rx_worker`.

Resolution order:

```text
1. ST2110_MTL_RX_WORKER
2. worker рядом с текущим plugin/module
3. ST2110_MTL_RX_WORKER_INSTALL_DIR, если задан compile-time macro
4. PATH fallback только если ST2110_MTL_RX_WORKER_PATH_FALLBACK=1
```

Product path intentionally не полагается на обычный PATH lookup. Если worker не найден, manager пишет diagnostic message в `last_error_message()`.

Ключевые environment variables:

```text
ST2110_MTL_RX_WORKER
    explicit path к worker binary

ST2110_MTL_RX_WORKER_PATH_FALLBACK
    development-only fallback, разрешает искать worker по имени
```

Если `ST2110_MTL_RX_WORKER` задан как абсолютный/relative path с parent path, файл должен существовать и быть executable.

---

##### Acquire / spawn pipeline

Основной метод:

```c++
std::expected<WorkerLease, Error>
acquire_or_spawn_compatible_worker_for_graph(
    const MtlRuntimeConfig &runtime,
    MtlWorkerGraphId graph_id
);
```

Порядок работы:

```text
1. Проверить graph_id != 0

2. Убедиться, что worker executable path resolved

3. Удалить retirable workers:
   - unhealthy workers
   - idle workers with incompatible runtime

4. Найти healthy compatible worker:
   - record state == Healthy
   - control_channel exists
   - control_channel->healthy()
   - lease.runtime == requested runtime

5. Если compatible worker найден:
   - добавить graph_id в active_graphs
   - вернуть WorkerLease

6. Если не найден:
   - создать MtlWorkerProcessControlChannel
   - отправить ConfigHandshakeRequest(runtime)
   - ожидать successful HealthEvent
   - создать WorkerRecord
   - добавить graph_id в active_graphs
   - вернуть WorkerLease
```

`ConfigHandshakeRequest` — это первый worker-level validation boundary. Worker должен принять runtime config до того, как graph client будет отправлять `StartSessions`.

---

##### Worker compatibility

Worker compatible, если:

```text
worker is Healthy
control_channel exists
control_channel->healthy()
worker.runtime == requested runtime
```

То есть compatibility сейчас строгая: `MtlRuntimeConfig` должен совпадать полностью.

Это означает, что разные PCI BDF / SIP / redundant-port topology получают разные worker processes.

---

##### Retire policy

Manager держит workers в cache, но не бесконечно.

Перед acquire он удаляет:

```text
unhealthy workers
idle incompatible workers
```

Также при `release_graph_noexcept`, если worker стал idle и уже существует другой healthy worker с другим runtime, idle worker завершается.

Практическая политика:

```text
healthy idle compatible worker:
    может остаться cached для будущего reuse

healthy idle incompatible worker:
    retire, когда появляется/существует worker другого runtime

unhealthy worker:
    invalidate and shutdown
```

Это ограничивает количество живых MTL worker processes и при этом позволяет быстро переиспользовать worker для того же runtime.

---

##### Graph ownership tracking

Внутри manager-а каждый worker имеет:

```c++
std::unordered_set<MtlWorkerGraphId> active_graphs;
```

Когда graph получает lease:

```text
active_graphs.insert(graph_id)
```

Когда graph останавливается:

```text
release_graph_noexcept(worker_id, graph_id)
→ active_graphs.erase(graph_id)
```

Manager tracking отвечает именно за worker ownership:

```text
какие graph_id сейчас используют конкретный worker process
```

---

##### Release graph

```c++
void release_graph_noexcept(
    std::uint64_t worker_id,
    MtlWorkerGraphId graph_id
) noexcept;
```

Используется при нормальном завершении graph-а.

Порядок:

```text
1. Найти worker по worker_id
2. Удалить graph_id из active_graphs
3. Если worker еще имеет active graphs → оставить worker живым
4. Если worker стал idle:
   - если существует другой healthy worker с другим runtime,
     этот idle worker retired
   - иначе worker может остаться cached
```

`release_graph_noexcept` не означает немедленное завершение worker-а во всех случаях. Healthy idle worker может остаться для reuse.

---

##### Invalidate worker

```c++
void invalidate_worker_noexcept(
    std::uint64_t worker_id
) noexcept;
```

Используется при worker/channel/runtime failure.

Поведение:

```text
1. Найти worker по worker_id
2. Пометить Unhealthy
3. Удалить из manager records
4. Отправить ShutdownRequest best-effort
5. Force-close process control channel
```

Graph client вызывает invalidate path, когда ошибка делает worker ненадежным:

```text
IPC transaction failed
worker health failed
unexpected response / runtime backend failure
```

Отличие от `release_graph_noexcept`:

```text
release_graph:
    нормальный stop одного graph-а

invalidate_worker:
    worker больше нельзя безопасно использовать
```

---

##### Shutdown all workers

```c++
void shutdown_all_workers_noexcept() noexcept;
```

Используется в destructor-е manager-а и global cleanup.

Pipeline:

```text
1. Пометить все workers как Stopping
2. Забрать leases во временный vector
3. Очистить manager records
4. Для каждого lease:
   - best-effort ShutdownRequest
   - force-close process channel
```

Shutdown выполняется вне mutex-а, чтобы manager не держал lock во время потенциально долгих IPC/process операций.

---

##### Config handshake

При создании нового worker-а manager отправляет:

```c++
MtlWorkerConfigHandshakeRequest{
    .request_id = ...,
    .runtime = runtime,
}
```

Ожидаемый successful response:

```text
MtlWorkerHealthEvent
with matching request_id
healthy == true
```

Также worker может вернуть:

```text
MtlWorkerErrorEvent
```

В этом случае manager сохраняет diagnostic detail в `last_error_message()` и закрывает channel.

Если transaction не удалась, diagnostic message явно указывает:

```text
worker path
possible missing worker binary
possible missing MTL/DPDK runtime libraries
ldconfig hint
underlying error
```

---

##### Race handling при spawn

Manager учитывает race condition:

```text
thread A спавнит новый worker
thread B успел создать compatible worker раньше
```

После successful handshake нового process-а manager еще раз проверяет, появился ли compatible worker.

Если появился:

```text
reuse existing worker
shutdown extra newly spawned worker
```

Это предотвращает лишние duplicate workers для одного runtime.

---

##### `default_mtl_worker_manager`

```c++
MtlWorkerManager &default_mtl_worker_manager();
```

Возвращает process-wide singleton manager.

Graph client использует именно его:

```text
default_mtl_worker_manager()
→ acquire_or_spawn_compatible_worker_for_graph(...)
→ release_graph_noexcept(...)
→ invalidate_worker_noexcept(...)
```

---

#### `mtl_rx_video_backend_proxy.hpp` / `mtl_rx_video_backend_proxy.cpp`

Содержит OBS/core-process-side `IRxBackend` proxy для MTL video receive path.

Этот класс не является настоящим MTL receiver-ом. Он не создает `mtl_handle`, не вызывает `st20p_rx_create`, не читает MTL frames и не работает с MTL API напрямую.

Его задача — адаптировать общий backend interface проекта:

```text
IRxBackend::start(...)
IRxBackend::stop()
last_error_message()
```

к graph-level MTL worker orchestration через:

```text
MtlWorkerGraphClient
```

Общий pipeline:

```text
OBS / SourceRuntime
→ MtlRxVideoBackendProxy::start
→ MtlWorkerGraphClient::attach_sink
→ MtlWorkerGraphClient::start
→ MTL worker process
→ shared-memory frame ready events
→ MtlWorkerGraphClient delivery
→ IFrameSink::on_video_frame
```

---

##### `MtlRxVideoBackendProxy`

```c++
class MtlRxVideoBackendProxy final : public IRxBackend;
```

Это video-specific proxy backend для MTL worker path.

Он хранит:

```c++
MtlVideoStartConfig cfg_;
std::shared_ptr<MtlWorkerGraphClient> graph_client_;
IFrameSink *sink_ = nullptr;
bool started_ = false;
```

`graph_client_` выполняет настоящую orchestration работу:

```text
worker lease
shared-memory rings
StartSessions / StopSessions
async FrameReady events
delivery to sink
stats/error state
```

Proxy только управляет lifecycle boundary, видимой через `IRxBackend`.

---

##### Start lifecycle

```c++
RxBackendLifecycleResult start(IFrameSink *sink) override;
```

`start` делает минимальную proxy-обвязку вокруг `MtlWorkerGraphClient`.

Порядок:

```text
1. Проверить, что graph_client_ существует
2. Если proxy уже started:
   - разрешить повторный start только с тем же sink
   - иначе вернуть InvalidBackendState

3. attach_sink(sink) в graph_client
4. сохранить sink_
5. вызвать graph_client_->start()
6. если start failed:
   - если graph_client не running, detach sink
   - очистить sink_
   - вернуть ошибку
7. если start вернул false:
   - если graph_client не running, detach sink
   - очистить sink_
   - вернуть false
8. выставить started_ = true
9. вернуть true
```

То есть proxy не запускает worker напрямую. Он делегирует это в graph client.

---

##### Repeated start behavior

Если `start()` вызывается повторно, proxy проверяет sink identity:

```text
same sink:
    return true

different sink:
    Error::InvalidBackendState
```

Это защищает от ситуации, когда один и тот же backend instance пытаются одновременно подключить к другому delivery target.

---

##### Stop lifecycle

```c++
RxBackendLifecycleResult stop() override;
```

`stop` останавливает graph через `MtlWorkerGraphClient`.

Порядок:

```text
1. Если graph_client_ отсутствует:
   - очистить local state
   - return true

2. Если proxy не started:
   - очистить sink_
   - return true

3. Сохранить attached_sink
4. вызвать graph_client_->stop()
5. Если graph_client больше не running:
   - detach_sink_noexcept(attached_sink)

6. очистить sink_
7. started_ = false
8. вернуть результат graph_client_->stop()
```

Важная деталь: sink detached только если graph client больше не running.

Это согласуется с возможной refcount/start-count логикой внутри `MtlWorkerGraphClient`: если один graph client используется несколькими proxy/backend views, один `stop()` не обязательно полностью останавливает graph.

---

##### Error diagnostics

```c++
std::optional<MtlWorkerErrorDetail> last_error_detail() const;
std::string last_error_message() const;
```

Proxy не хранит собственную подробную ошибку.

Он просто пробрасывает diagnostic state из `MtlWorkerGraphClient`:

```text
MtlRxVideoBackendProxy
→ graph_client_->last_error_detail()
→ graph_client_->last_error_message()
```

Если `graph_client_` отсутствует, возвращается:

```text
std::nullopt
""
```

---

#### `mtl_rx_audio_backend_proxy.hpp` / `mtl_rx_audio_backend_proxy.cpp`

Содержит OBS/core-process-side `IRxBackend` proxy для MTL audio receive path.

Этот класс не является настоящим MTL audio receiver-ом. Он не создает `mtl_handle`, не вызывает `st30p_rx_create`, не читает MTL audio frames/blocks и не работает с MTL API напрямую.

Его задача — адаптировать общий backend interface проекта:

```text
IRxBackend::start(...)
IRxBackend::stop()
last_error_message()
```

к graph-level MTL worker orchestration через:

```text
MtlWorkerGraphClient
```

Общий pipeline:

```text
OBS / SourceRuntime
→ MtlRxAudioBackendProxy::start
→ MtlWorkerGraphClient::attach_sink
→ MtlWorkerGraphClient::start
→ MTL worker process
→ shared-memory audio block ready events
→ MtlWorkerGraphClient delivery
→ IFrameSink::on_audio_frame
```

---

##### `MtlRxAudioBackendProxy`

```c++
class MtlRxAudioBackendProxy final : public IRxBackend;
```

Это audio-specific proxy backend для MTL worker path.

Он хранит:

```c++
MtlAudioStartConfig cfg_;
std::shared_ptr<MtlWorkerGraphClient> graph_client_;
IFrameSink *sink_ = nullptr;
bool started_ = false;
```

`graph_client_` выполняет реальную работу:

```text
worker lease
shared-memory rings
StartSessions / StopSessions
async AudioBlockReady events
delivery to sink
stats/error state
```

Proxy только реализует lifecycle boundary, видимую через `IRxBackend`.

---

##### Start lifecycle

```c++
RxBackendLifecycleResult start(IFrameSink *sink) override;
```

`start` делает thin-wrapper вокруг `MtlWorkerGraphClient`.

Порядок:

```text
1. Проверить, что graph_client_ существует
2. Если proxy уже started:
   - разрешить повторный start только с тем же sink
   - иначе вернуть InvalidBackendState

3. attach_sink(sink) в graph_client
4. сохранить sink_
5. вызвать graph_client_->start()
6. если start failed:
   - если graph_client не running, detach sink
   - очистить sink_
   - вернуть ошибку
7. если start вернул false:
   - если graph_client не running, detach sink
   - очистить sink_
   - вернуть false
8. выставить started_ = true
9. вернуть true
```

Proxy не запускает worker process сам. Worker lifecycle, `StartSessions`, shared-memory rings и async delivery находятся в `MtlWorkerGraphClient`.

---

##### Repeated start behavior

Если `start()` вызывается повторно, proxy проверяет identity sink-а:

```text
same sink:
    return true

different sink:
    Error::InvalidBackendState
```

Это защищает backend instance от повторного подключения к другому delivery target.

---

##### Stop lifecycle

```c++
RxBackendLifecycleResult stop() override;
```

`stop` делегирует остановку graph-а в `MtlWorkerGraphClient`.

Порядок:

```text
1. Если graph_client_ отсутствует:
   - очистить local state
   - return true

2. Если proxy не started:
   - очистить sink_
   - return true

3. Сохранить attached_sink
4. вызвать graph_client_->stop()
5. Если graph_client больше не running:
   - detach_sink_noexcept(attached_sink)

6. очистить sink_
7. started_ = false
8. вернуть результат graph_client_->stop()
```

Sink detached только если graph client больше не running. Это согласуется с возможной start/refcount логикой внутри `MtlWorkerGraphClient`.

---

##### Error diagnostics

```c++
std::optional<MtlWorkerErrorDetail> last_error_detail() const;
std::string last_error_message() const;
```

Proxy не хранит собственную подробную ошибку.

Он пробрасывает diagnostic state из graph client-а:

```text
MtlRxAudioBackendProxy
→ graph_client_->last_error_detail()
→ graph_client_->last_error_message()
```

Если `graph_client_` отсутствует, возвращается:

```text
std::nullopt
""
```

---

Далее см. mtl_worker_pipeline.md