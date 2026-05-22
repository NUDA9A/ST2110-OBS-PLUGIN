### `st2110/delivery`

#### `socket_start_config.hpp`

Содержит common Socket start config model и helper-ы для projection-а receive bootstrap/local policy в socket-specific leg config.

Файл находится на этапе backend-specific projection:

```text
ReceiveBootstrap + ReceiveLocalPolicy
→ SocketStartConfig
→ Socket backend construction
```

Основная структура одной socket receive leg:

```c++
struct SocketMediaLegConfig {
    SocketAddressFamily family;
    std::string local_ip;

    std::string dest_ip;
    std::uint16_t udp_port;

    SourceFilterSignaling source_filter;
    std::size_t max_udp_datagram_bytes;

    SocketRxOpenConfig open_config;
};
```

`SocketMediaLegConfig` объединяет remote receive leg и local receive policy в форму, которую может использовать Socket backend.

Поля:

- `family` — IPv4 или IPv6;
- `local_ip` — локальный IP, выбранный receive local policy;
- `dest_ip` — destination IP из SDP `c=`;
- `udp_port` — UDP port из SDP `m=`;
- `source_filter` — SDP source filter;
- `max_udp_datagram_bytes` — effective `MAXUDP`, по умолчанию `1460`;
- `open_config` — platform-specific config для открытия socket receive port.

Общий Socket start config:

```c++
struct SocketStartConfig {
    ReceiveTopologyKind topology;
    ReorderBufferConfig reorder_buffer_config;

    std::vector<SocketMediaLegConfig> legs;
};
```

Файл содержит helper для преобразования SDP source filter в socket source filter:

```c++
std::optional<SocketSourceFilter>
make_socket_source_filter(
    const SourceFilterSignaling &source_filter,
    SocketAddressFamily family
);
```

Если `source_filter.source_addresses` пустой, функция возвращает `std::nullopt`.

Если source addresses есть, функция строит `SocketSourceFilter` с address family и списком source addresses.

Helper для projection-а одной leg:

```c++
SocketMediaLegConfig make_socket_media_leg_config(
    const ReceiveRemoteLeg &remote_leg,
    const ReceiveLocalLegPolicy &local_leg
);
```

`make_socket_media_leg_config` объединяет:

- remote leg из `ReceiveBootstrap`;
- local leg из `ReceiveLocalPolicy`.

Функция строит `SocketRxOpenConfig` через:

```c++
build_socket_rx_open_config(
    remote_leg.udp_port,
    local_leg.local_ip,
    remote_leg.destination.destination_address,
    socket_source_filter
);
```

Если socket open config нельзя построить, функция бросает `std::runtime_error`.

Helper для projection-а всех legs:

```c++
std::vector<SocketMediaLegConfig>
make_socket_media_leg_configs(
    const ReceiveBootstrap &bootstrap,
    const ReceiveLocalPolicy &local_policy
);
```

Функция проверяет, что количество remote legs совпадает с количеством local policy legs.

Затем для каждой пары:

```text
ReceiveRemoteLeg + ReceiveLocalLegPolicy
→ SocketMediaLegConfig
```

Если количество legs не совпадает, функция бросает `std::runtime_error`.

### `st2110/receive/shared`

#### `receive_reorder_tolerance_policy.hpp`

Содержит настройки reorder buffer-а и enum policy, который определяет, что делать, если в RTP sequence есть gap.

Основной enum:

```c++
enum class ReceiveReorderGapPolicy {
    WaitForMissing,
    FlushGapOnce,
    FlushAlways,
    FlushAfterTimeout,
    DropFrameOnGap,
    FlushOnMarkerBoundary,
    TopologyAwareWait,
    FlushAfterNPackets
};
```

`ReceiveReorderGapPolicy` описывает стратегию поведения reorder buffer-а при обнаружении пропущенного RTP packet-а.

Значения policy:

- `WaitForMissing` — ждать отсутствующий packet в пределах reorder window;
- `FlushGapOnce` — один раз пропустить gap и продолжить выдачу;
- `FlushAlways` — всегда flush-ить при gap;
- `FlushAfterTimeout` — ждать missing packet до timeout-а;
- `DropFrameOnGap` — сбрасывать frame при gap;
- `FlushOnMarkerBoundary` — flush-ить на marker boundary;
- `TopologyAwareWait` — учитывать receive topology, например redundant pair;
- `FlushAfterNPackets` — flush-ить gap после накопления заданного количества packets.

Сейчас реализованы только `WaitForMissing`, `FlushGapOnce`, `FlushAfterNPackets` и `FlushOnMarkerBoundary`.
Остальные варианты можно выбрать, но прямо сейчас в месте где должна быть логика под них бросается исключение, которое приводит к крашу OBS.

Файл задает default значения:

```c++
inline constexpr std::uint32_t defaultReorderWindowPackets = 32;
inline constexpr std::uint32_t defaultFlushAfterNPackets = 8;
```

`defaultReorderWindowPackets` — размер reorder window по умолчанию в RTP packets.

`defaultFlushAfterNPackets` — default threshold для policy `FlushAfterNPackets`.

Основная структура:

```c++
struct ReorderBufferConfig {
    std::uint32_t window_size_packets;
    ReceiveReorderGapPolicy reorder_tolerance_policy;
    std::uint32_t flush_after_n_packets;

    friend bool operator==(
        const ReorderBufferConfig &,
        const ReorderBufferConfig &
    ) = default;
};
```

`window_size_packets` определяет, сколько RTP packets reorder buffer может держать для восстановления порядка.

`reorder_tolerance_policy` определяет поведение при gap-е в sequence numbers.

`flush_after_n_packets` используется policy, которым нужен packet-count threshold, в первую очередь `FlushAfterNPackets`.

Файл также содержит helper для создания default config:

```c++
ReorderBufferConfig make_default_reorder_buffer_config(
    ReceiveReorderGapPolicy reorder_tolerance_policy
) noexcept;
```

Функция возвращает `ReorderBufferConfig` с default window size и default `flush_after_n_packets`, но с указанной gap policy:

```c++
ReorderBufferConfig {
    .window_size_packets = defaultReorderWindowPackets,
    .reorder_tolerance_policy = reorder_tolerance_policy,
    .flush_after_n_packets = defaultFlushAfterNPackets,
};
```

Этот файл сам не реализует reorder algorithm. Он только описывает configuration model для reorder buffer-а.

Фактический размер window может быть переопределен ниже по пайплайну. Например Socket projection использует `socket_reorder_tuning.hpp`, чтобы вывести `window_size_packets` из media signaling-а, packet timing-а и jitter budget-а.

#### `socket_reorder_tuning.hpp`

Содержит расчеты для автоматического выбора размера reorder buffer-а в Socket backend-е.

Главная идея файла: reorder buffer измеряется не во времени, а в количестве RTP-пакетов. Поэтому файл переводит фиксированный jitter budget в наносекундах в количество packets, которое нужно держать в reorder window.

Базовый jitter budget:

```c++
inline constexpr std::uint64_t socketReorderJitterBudgetNs = 5'000'000ULL;
```

То есть Socket backend пытается подобрать reorder window так, чтобы покрыть примерно `5 ms` сетевого jitter-а.

Для video и audio расчет разный, потому что у них разная структура потока:

- video — много RTP-пакетов на один frame;
- audio — обычно один RTP-пакет на фиксированный packet time.

---

##### Video reorder window

Для video файл сначала оценивает, сколько RTP-пакетов приходится на один frame.

Общая формула:

```text
packets_per_frame =
    ceil(active_frame_bytes / effective_payload_bytes_per_packet)
```

Затем считается примерный интервал между RTP-пакетами внутри frame:

```text
packet_spacing_ns =
    frame_period_ns / packets_per_frame
```

После этого jitter budget переводится в количество packets:

```text
required_window =
    ceil(socketReorderJitterBudgetNs / packet_spacing_ns)
    + socketVideoReorderMarginPackets
```

И результат ограничивается диапазоном:

```text
64..8192 packets
```

То есть итоговая video window — это оценка:

```text
сколько video RTP packets может прийти за 5 ms
+
запас 16 packets
```

---

##### Расчет video frame period

```c++
std::uint64_t socket_reorder_video_frame_period_ns(
    const VideoMediaDescription &media
) noexcept;
```

Функция вычисляет длительность одного video frame:

```text
frame_period_ns =
    ceil(1_000_000_000 * fps_den / fps_num)
```

Например для `30000/1001`:

```text
frame_period_ns ≈ 33_366_667 ns
```

Если `fps_num` или `fps_den` равны `0`, функция возвращает `0`, а дальнейший расчет использует минимальное video reorder window.

---

##### Расчет video bits-per-pixel

```c++
VideoBitsPerPixelRatio socket_reorder_video_bits_per_pixel_ratio(
    const VideoMediaDescription &media
) noexcept;
```

Функция оценивает количество bits per pixel по sampling и bit depth.

Для `YCbCr 4:2:0`:

```text
bits_per_pixel = 3 * bits / 2
```

Потому что на 2 пикселя приходится 2 luma samples и 1 chroma pair.

Для `YCbCr 4:2:2`:

```text
bits_per_pixel = 2 * bits
```

Для `YCbCr 4:4:4`, `RGB`, `XYZ`, `ICtCp 4:4:4` и неизвестных форматов:

```text
bits_per_pixel = 3 * bits
```

Для `Key`:

```text
bits_per_pixel = bits
```

Эта оценка нужна не для точной реконструкции frame-а, а для грубой оценки количества RTP-пакетов на frame.

---

##### Расчет active frame size

```c++
std::uint64_t socket_reorder_video_active_frame_bytes(
    const VideoMediaDescription &media
) noexcept;
```

Функция оценивает размер active video frame:

```text
pixels = width * height

active_frame_bytes =
    ceil(pixels * bits_per_pixel_numerator
         / (bits_per_pixel_denominator * 8))
```

Если `width`, `height` или `depth.bits` равны `0`, функция возвращает `0`.

---

##### Расчет effective payload bytes

```c++
std::size_t socket_reorder_video_effective_payload_bytes(
    const VideoReceiveBootstrap &bootstrap
) noexcept;
```

Функция оценивает, сколько байтов внутри UDP packet-а остается под video payload data.

Берется `MAXUDP` из первой receive leg:

```text
max_udp_datagram_bytes = bootstrap.receive_bootstrap.legs.front().max_udp_datagram_bytes
```

Если legs нет или `MAXUDP == 0`, используется default:

```text
1460 bytes
```

Дальше по текущей реализации вычитается overhead:

```text
UDP header:              8 bytes
RTP header:             12 bytes
ST 2110-20 header min:   8 bytes
```

Итого:

```text
effective_payload_bytes =
    MAXUDP - 8 - 12 - 8
```

Если `MAXUDP` меньше или равен overhead, функция возвращает `1`, чтобы не получить деление на ноль.

---

##### Расчет packets per frame

```c++
std::uint64_t derive_socket_video_packets_per_frame(
    const VideoReceiveBootstrap &bootstrap
) noexcept;
```

Функция объединяет предыдущие расчеты:

```text
active_frame_bytes = estimated frame size
payload_bytes      = estimated payload bytes per packet

packets_per_frame =
    ceil(active_frame_bytes / payload_bytes)
```

Если расчет дал `0`, возвращается `1`.

---

##### Итоговый video reorder window

```c++
std::uint32_t derive_socket_video_reorder_window_packets(
    const VideoReceiveBootstrap &bootstrap
) noexcept;
```

Функция выбирает размер reorder window в RTP-пакетах.

Расчет:

```text
frame_period_ns = frame duration
packets_per_frame = estimated packets per frame
packet_spacing_ns = frame_period_ns / packets_per_frame

required_window =
    ceil(5 ms / packet_spacing_ns) + 16
```

Затем:

```text
window_size_packets =
    clamp(required_window, 64, 8192)
```

Если frame period не может быть рассчитан, используется минимальное значение:

```text
64 packets
```

---

##### Audio reorder window

Для audio расчет проще: packet spacing уже задан через packet time.

```c++
std::uint64_t socket_reorder_audio_packet_time_ns(
    const AudioMediaDescription &media
) noexcept;
```

Функция переводит `packet_time_us` в nanoseconds:

```text
packet_time_ns = packet_time_us * 1000
```

Дальше jitter budget переводится в количество audio packets:

```text
required_window =
    ceil(socketReorderJitterBudgetNs / packet_time_ns)
    + socketAudioReorderMarginPackets
```

И результат ограничивается диапазоном:

```text
8..512 packets
```

Примеры:

```text
packet_time = 1 ms

ceil(5 ms / 1 ms) + 4 = 9 packets
```

```text
packet_time = 125 us

ceil(5 ms / 0.125 ms) + 4 = 44 packets
```

---

##### Итоговый audio reorder window

```c++
std::uint32_t derive_socket_audio_reorder_window_packets(
    const AudioReceiveBootstrap &bootstrap
) noexcept;
```

Функция выбирает размер reorder window в RTP-пакетах для audio.

Если `packet_time_ns == 0`, используется минимальное значение:

```text
8 packets
```

Иначе:

```text
window_size_packets =
    clamp(ceil(5 ms / packet_time_ns) + 4, 8, 512)
```

---

##### Построение `ReorderBufferConfig`

Video:

```c++
ReorderBufferConfig derive_socket_video_reorder_buffer_config(
    const VideoReceiveBootstrap &bootstrap,
    ReceiveReorderGapPolicy reorder_tolerance_policy
) noexcept;
```

Audio:

```c++
ReorderBufferConfig derive_socket_audio_reorder_buffer_config(
    const AudioReceiveBootstrap &bootstrap,
    ReceiveReorderGapPolicy reorder_tolerance_policy
) noexcept;
```

Обе функции возвращают:

```c++
ReorderBufferConfig {
    window_size_packets,
    reorder_tolerance_policy,
    flush_after_n_packets
}
```

`flush_after_n_packets` вычисляется как четверть от window size:

```text
flush_after_n_packets = max(1, window_size_packets / 4)
```

Для audio есть специальная замена policy:

```text
FlushOnMarkerBoundary → FlushAfterNPackets
```

Потому что audio path не должен использовать marker-boundary policy как video. У audio RTP packets не дают такой же frame-boundary semantics, как video marker boundary.

---

##### Зачем нужен этот файл

`socket_reorder_tuning.hpp` нужен для того, чтобы Socket backend не получал произвольный hardcoded reorder window.

Вместо этого размер окна выводится из signaling-а:

```text
video:
    resolution
    fps
    sampling
    bit depth
    MAXUDP

audio:
    packet time
```

и из выбранной receive policy:

```text
ReceiveReorderGapPolicy
```

### `st2110/contracts/video`

#### `partial_unit_policy.hpp`

Содержит policy для обработки частично собранных video units в project-owned video receive pipeline.

```c++
enum class PartialUnitPolicy {
    EmitWithFlag,
    Drop
};
```

`PartialUnitPolicy` используется там, где video pipeline уже обнаружил, что frame/unit не был собран полностью: например из-за packet loss, gap-а после reorder stage или неполного набора payload data.

Значения:

- `EmitWithFlag` — выдать частично собранный unit дальше по pipeline, но пометить его как partial/incomplete;
- `Drop` — не выдавать частично собранный unit дальше.

Эта policy относится к video receive/depacketize/reconstruction path.

### `st2110/contracts`

#### `settings.hpp`

Содержит общие пользовательские/project settings, которые влияют на построение receive pipeline.

Основной enum выбора receive backend-а:

```c++
enum class ReceiveBackendKind {
    Socket,
    Mtl,
};
```

Основная структура:

```c++
struct Settings {
    ReceiveReorderGapPolicy reorder_tolerance_policy;
    PartialUnitPolicy partial_unit_policy;
    ReceiveBackendKind backend_kind;

    friend bool operator==(
        const Settings &,
        const Settings &
    ) = default;
};
```

На текущем этапе эта настройка применяется к Socket path. Socket backend сам принимает RTP packets и строит reorder buffer config. MTL backend через project socket-side reorder buffer не проходит, поэтому эта настройка не должна восприниматься как MTL runtime setting.

Default settings:

```c++
Settings {
    .reorder_tolerance_policy = ReceiveReorderGapPolicy::WaitForMissing,
    .partial_unit_policy = PartialUnitPolicy::EmitWithFlag,
    .backend_kind = ReceiveBackendKind::Socket,
};
```

Файл не строит pipeline сам. Он только задает typed settings contract, который higher-level orchestration / OBS source layer может использовать при выборе backend-а и построении backend-specific config.

### `st2110/delivery/video`

#### `pixel_format.hpp`

Содержит enum project-level pixel formats.

```c++
enum class PixelFormat {
    UYVY,
    RGB8,

    YUV422RFC4175PG2BE10,
    YUV422RFC4175PG2BE12,
    YUV444RFC4175PG4BE10,
    YUV444RFC4175PG2BE12,
    RGBRFC4175PG4BE10,
    RGBRFC4175PG2BE12,

    YUV422PLANAR8,
    YUV422PLANAR10LE,
    YUV422PLANAR12LE,
    YUV422PLANAR16LE,
    YUV444PLANAR10LE,
    YUV444PLANAR12LE,
    YUV420PLANAR8,

    BGRA,
    ARGB,
    V210,
    Y210,
};
```

`PixelFormat` описывает формат video frame data внутри project delivery/conversion layer.

Форматы можно разделить на несколько групп.

Packed / common output formats:

```c++
UYVY
RGB8
BGRA
ARGB
V210
Y210
```

RFC 4175 / ST 2110 transport-oriented formats:

```c++
YUV422RFC4175PG2BE10
YUV422RFC4175PG2BE12
YUV444RFC4175PG4BE10
YUV444RFC4175PG2BE12
RGBRFC4175PG4BE10
RGBRFC4175PG2BE12
```

Planar project-side formats:

```c++
YUV422PLANAR8
YUV422PLANAR10LE
YUV422PLANAR12LE
YUV422PLANAR16LE
YUV444PLANAR10LE
YUV444PLANAR12LE
YUV420PLANAR8
```

`PixelFormat` используется позже, когда pipeline должен выбрать конкретное представление frame buffer-а для backend-а, conversion layer-а или OBS-facing output.

Socket depacketizer сейчас поддерживает только такие форматы:
```c++
UYVY
RGB8
YUV422RFC4175PG2BE10
YUV422RFC4175PG2BE12
YUV444RFC4175PG4BE10
RGBRFC4175PG4BE10
YUV444RFC4175PG2BE12
RGBRFC4175PG2BE12
```

VideoReceivePipelineConfig сейчас также поддерживает только:
```
YCbCr422_8/10/12
YCbCr444_10/12
RGB_8/10/12
```

И в OBS проецируется только 5 форматов:
```
PixelFormat::UYVY          → VIDEO_FORMAT_UYVY
PixelFormat::BGRA          → VIDEO_FORMAT_BGRA
PixelFormat::V210          → VIDEO_FORMAT_V210
PixelFormat::YUV420PLANAR8 → VIDEO_FORMAT_I420
PixelFormat::YUV422PLANAR8 → VIDEO_FORMAT_I422
```

Реально полная поддержка (на всех стадиях) есть только для UYVY.

#### `depacketizer_config.hpp`

Содержит config для video depacketizer-а: какой тип video unit нужно собирать, когда считать unit завершенным, в какой `PixelFormat` писать payload data и что делать с partial units.

Основной enum:

```c++
enum class VideoAssemblyUnitKind {
    Frame,
    Field,
    Segment
};
```

`VideoAssemblyUnitKind` описывает, какую единицу сборки должен выдавать depacketizer:

- `Frame` — полный progressive frame;
- `Field` — отдельное поле interlaced video;
- `Segment` — отдельный segment для PsF.

Политика завершения video unit:

```c++
struct VideoReceiveCompletionPolicy {
    VideoAssemblyUnitKind unit_kind;
    bool marker_terminates_current_unit;
};
```

`unit_kind` задает тип собираемой единицы.

`marker_terminates_current_unit` определяет, завершает ли RTP marker bit текущую единицу сборки.

Для progressive и interlaced режимов marker считается границей текущего unit-а.

Для PsF marker не завершает каждый segment, потому что PsF frame представлен двумя segmented частями, и граница сборки определяется не только marker bit-ом.

Основной config depacketizer-а:

```c++
struct DepacketizerConfig {
    VideoReceiveCompletionPolicy video_receive_completion_policy;
    VideoScanMode scan_mode;
    PixelFormat format;
    std::uint32_t width;
    std::uint32_t height;
    PartialUnitPolicy policy;
};
```

`scan_mode` определяет, как RTP packets группируются в assembly units:

- progressive packets собираются в frame;
- interlaced packets собираются в fields;
- PsF packets собираются в segments.

`format` задает target `PixelFormat`, в который depacketizer будет раскладывать payload data.

`width` и `height` задают размеры video frame из SDP media model.

`policy` задает, что делать с неполно собранным unit-ом:

- `EmitWithFlag` — выдать partial unit дальше;
- `Drop` — отбросить partial unit.

Helper для выбора completion policy:

```c++
VideoReceiveCompletionPolicy video_receive_completion_policy(
    VideoScanMode mode
) noexcept;
```

Он задает правила по scan mode:

```text
Progressive:
    unit_kind = Frame
    marker_terminates_current_unit = true

Interlaced:
    unit_kind = Field
    marker_terminates_current_unit = true

PsF:
    unit_kind = Segment
    marker_terminates_current_unit = false
```

Helper для создания полного config-а:

```c++
DepacketizerConfig make_depacketizer_config(
    VideoScanMode mode,
    PixelFormat format,
    std::uint32_t width,
    std::uint32_t height,
    PartialUnitPolicy policy
);
```

`make_depacketizer_config` собирает `DepacketizerConfig` из scan mode, target pixel format, размеров frame-а и partial-unit policy.

#### `video_unit_reconstructor_config.hpp`

Содержит config для video unit reconstructor-а.

Video unit reconstructor работает после depacketizer-а и отвечает за преобразование собранных video units в итоговый video frame contract, который дальше может быть передан в delivery/sink слой.

Основная структура:

```c++
struct VideoUnitReconstructorConfig {
    PixelFormat format;
    std::uint32_t width;
    std::uint32_t height;
};
```

`format` задает `PixelFormat`, в котором reconstructor ожидает входные units и формирует итоговый frame.

`width` и `height` задают размер итогового video frame.

Helper для создания config-а:

```c++
VideoUnitReconstructorConfig make_video_unit_reconstructorConfig(
    PixelFormat format,
    std::uint32_t width,
    std::uint32_t height
);
```

Функция просто упаковывает `format`, `width` и `height` в `VideoUnitReconstructorConfig`.

#### `video_receive_pipeline_config.hpp`

Содержит сборку config-а для project-owned video receive pipeline:

```text
VideoMediaDescription
+ VideoScanMode
+ PartialUnitPolicy
→ VideoReceivePipelineConfig
```

Этот файл связывает signaling-level video model с конкретными internal pipeline config-ами:

```c++
struct VideoReceivePipelineConfig {
    DepacketizerConfig depacketizer;
    VideoUnitReconstructorConfig reconstructor;
};
```

`depacketizer` задает, как RTP payload segments будут раскладываться в video unit.

`reconstructor` задает, как собранные video units будут преобразовываться в итоговый frame.

---

##### Выбор `PixelFormat` из SDP signaling-а

```c++
PixelFormat pixel_format_from_video_stream_signaling(
    const VideoMediaDescription &media
);
```

Функция выбирает internal `PixelFormat` на основе:

```text
media.sampling
media.depth.bits
```

Поддерживаемые варианты сейчас:

```text
YCbCr 4:2:2 8-bit  → UYVY
YCbCr 4:2:2 10-bit → YUV422RFC4175PG2BE10
YCbCr 4:2:2 12-bit → YUV422RFC4175PG2BE12

YCbCr 4:4:4 10-bit → YUV444RFC4175PG4BE10
YCbCr 4:4:4 12-bit → YUV444RFC4175PG2BE12

RGB 8-bit  → RGB8
RGB 10-bit → RGBRFC4175PG4BE10
RGB 12-bit → RGBRFC4175PG2BE12
```

Все остальные варианты сейчас считаются unsupported и приводят к exception:

```c++
throw std::runtime_error("Unsupported format");
```

Неподдерживаемые sampling groups:

```text
YCbCr420
XYZ
Key
CLYCbCr444
CLYCbCr422
CLYCbCr420
ICtCp444
ICtCp422
ICtCp420
Other
```

Также сейчас не поддерживаются, например:

```text
YCbCr422 16-bit
YCbCr444 8-bit
YCbCr444 16-bit
RGB 16-bit
floating-point formats
```

---

##### Сборка полного video receive pipeline config-а

```c++
VideoReceivePipelineConfig make_video_receive_pipeline_config(
    VideoScanMode scan_mode,
    const VideoMediaDescription &media,
    PartialUnitPolicy policy
);
```

Функция выполняет три шага:

1. Выбирает internal `PixelFormat` из `VideoMediaDescription`.
2. Создает `DepacketizerConfig`.
3. Создает `VideoUnitReconstructorConfig`.

Итоговый config:

```c++
VideoReceivePipelineConfig {
    .depacketizer = ...,
    .reconstructor = ...
};
```

---

#### `socket_video_start_config.hpp`

Содержит projection из общего `ReceiveStartRequest` в Socket-specific video start config.

---

##### Video stream config для Socket backend-а

```c++
struct SocketVideoStreamConfig {
    std::uint8_t expected_payload_type;

    VideoMediaDescription media;
    VideoScanMode scan_mode;
    VideoPackingMode packing_mode;
};
```

`SocketVideoStreamConfig` хранит video-specific signaling, который нужен Socket receive path:

- `expected_payload_type` — RTP payload type, который должен приниматься;
- `media` — video media description из SDP;
- `scan_mode` — progressive/interlaced/PsF;
- `packing_mode` — ST 2110-20 packing mode: GPM или BPM.

Эта структура не содержит network legs. Network часть находится в базовом `SocketStartConfig`.

---

##### Полный Socket video start config

```c++
struct SocketVideoStartConfig : SocketStartConfig {
    SocketVideoStreamConfig stream;
    VideoReceivePipelineConfig video_receive_pipeline_config;
};
```

##### Projection video signaling-а

```c++
SocketVideoStreamConfig make_socket_video_stream_config(
    const VideoReceiveBootstrap &bootstrap
);
```

Функция переносит video signaling из `VideoReceiveBootstrap` в `SocketVideoStreamConfig`:

Переносятся:

```text
expected_payload_type
VideoMediaDescription
VideoScanMode
VideoPackingMode
```

`sender_type`, `TROFF` и `CMAX` сейчас в Socket video start config не переносятся и в пайплайне не используются

---

##### Основной projection helper

```c++
SocketVideoStartConfig project_receive_start_request_to_socket_video_start(
    const ReceiveStartRequest &request,
    Settings settings
);
```

Функция строит полный Socket video start config из общего receive request-а.

Последовательность:

```text
1. Взять VideoReceiveBootstrap из request.media
2. Перенести topology
3. Рассчитать ReorderBufferConfig
4. Собрать SocketVideoStreamConfig
5. Собрать VideoReceivePipelineConfig
6. Собрать SocketMediaLegConfig для всех receive legs
```
---

##### Какие settings реально используются

Из `Settings` здесь используются:

```c++
settings.reorder_tolerance_policy
settings.partial_unit_policy
```

`reorder_tolerance_policy` влияет на `ReorderBufferConfig` для Socket reorder buffer-а.

`partial_unit_policy` передается в video receive pipeline config и дальше влияет на поведение depacketizer/frame assembler при неполной сборке video unit-а.

`settings.backend_kind` здесь не используется. Эта функция уже предполагает, что caller выбрал Socket video path и вызывает правильный projection.

---

Дальше этот config должен использоваться Socket backend-ом.

#### `socket_audio_start_config.hpp`

Содержит projection из общего `ReceiveStartRequest` в Socket-specific audio start config.
---

##### Audio stream config для Socket backend-а

```c++
struct SocketAudioStreamConfig {
    std::uint8_t expected_payload_type;

    AudioMediaDescription media;
    std::optional<AudioChannelOrder> channel_order;

    std::uint32_t samples_per_packet;
};
```

`SocketAudioStreamConfig` хранит audio-specific signaling, который нужен Socket receive path:

- `expected_payload_type` — RTP payload type, который должен приниматься;
- `media` — audio media description из SDP;
- `channel_order` — optional SDP `channel-order`;
- `samples_per_packet` — вычисленное количество audio samples в одном RTP packet-е.

`channel_order` здесь сохраняется и прокидывается дальше, но текущий downstream audio path пока не использует его для выбора OBS speaker layout, перестановки каналов или проверки layout compatibility.

---

##### Полный Socket audio start config

```c++
struct SocketAudioStartConfig : SocketStartConfig {
    SocketAudioStreamConfig stream;
};
```
---

##### Расчет `samples_per_packet`

```c++
SocketAudioStreamConfig make_socket_audio_stream_config(
    const AudioReceiveBootstrap &bootstrap
);
```

Функция берет `AudioMediaDescription` из `AudioReceiveBootstrap` и вычисляет:

```text
samples_per_packet =
    sampling_rate_hz * packet_time_us / 1_000_000
```

Например:

```text
48000 Hz, 1000 us → 48 samples per packet
48000 Hz, 125 us  → 6 samples per packet
96000 Hz, 1000 us → 96 samples per packet
```

Если количество samples per packet нельзя корректно вывести, функция бросает:

```c++
std::runtime_error("Can not derive audio samples per packet")
```

После этого функция собирает:

```text
AudioReceiveBootstrap
→ SocketAudioStreamConfig
```

Переносятся:

```text
expected_payload_type
AudioMediaDescription
AudioChannelOrder
samples_per_packet
```

---

##### Основной projection helper

```c++
SocketAudioStartConfig project_receive_start_request_to_socket_audio_start(
    const ReceiveStartRequest &request,
    Settings settings
);
```

Функция строит полный Socket audio start config из общего receive request-а.

Последовательность:

```text
1. Взять AudioReceiveBootstrap из request.media
2. Перенести topology
3. Рассчитать ReorderBufferConfig
4. Собрать SocketAudioStreamConfig
5. Собрать SocketMediaLegConfig для всех receive legs
```

##### Audio-specific reorder behavior

Для audio reorder config строится через:

```c++
derive_socket_audio_reorder_buffer_config(...)
```

потому что audio RTP packets не дают такой же frame-boundary semantics, как video marker boundary.

---

Дальше этот config должен использоваться Socket backend-ом.

### `st2110/backends/socket/platform`

#### `socket_runtime.hpp`

Содержит platform-neutral contract для Socket receive runtime.

Файл описывает не конкретную Linux/Windows socket-реализацию, а общий интерфейс, через который Socket backend открывает receive port и читает UDP datagrams.

Ключевые сущности файла:

```c++
struct SocketRxOpenConfig;
std::expected<SocketRxOpenConfig, Error> build_socket_rx_open_config(...);

struct SocketReceiveResult;

class ISocketRxPort;
class ISocketRxPortFactory;
```

---

##### `SocketRxOpenConfig`

```c++
struct SocketRxOpenConfig {
    SocketEndpoint bind_endpoint;
    std::optional<SocketMulticastMembership> multicast_membership;
    std::optional<SocketSourceFilter> source_filter;
    bool reuse_address = true;
};
```

`SocketRxOpenConfig` — это полный config для открытия UDP receive socket-а.

Он содержит:

- `bind_endpoint` — address/port, на который нужно выполнить bind;
- `multicast_membership` — multicast group/interface, если destination address является multicast;
- `source_filter` — optional source filter из SDP `source-filter`;
- `reuse_address` — policy для повторного использования address/port.

Эта структура является результатом projection-а из receive/start config в platform socket layer.

---

##### `build_socket_rx_open_config`

```c++
std::expected<SocketRxOpenConfig, Error>
build_socket_rx_open_config(
    std::uint16_t udp_port,
    const std::string &local_ip,
    const std::string &dest_ip,
    const std::optional<SocketSourceFilter> &source_filter
) noexcept;
```

Функция строит `SocketRxOpenConfig` из:

```text
UDP port
local IP
destination IP
optional source filter
```

Основная логика:

1. Определяет address family по `local_ip`, если он задан, иначе по `dest_ip`.
2. Проверяет, что `dest_ip` соответствует выбранной address family.
3. Выставляет bind endpoint.
4. Если `local_ip` пустой, bind идет на wildcard address:
    - IPv4 → `0.0.0.0`;
    - IPv6 → `::`.
5. Если `dest_ip` является multicast address:
    - bind address заменяется на wildcard;
    - создается `SocketMulticastMembership`;
    - если `local_ip` задан, он используется как interface address для multicast join.
6. Если задан `source_filter`, он переносится в config.
7. Итоговый config валидируется через `validate_socket_rx_open_config`.

Для unicast receive функция строит обычный bind config.

Для multicast receive функция строит bind + multicast membership config.

---

##### `SocketReceiveResult`

```c++
struct SocketReceiveResult {
    std::size_t size_bytes;
    TimestampNs receive_timestamp_ns;
};
```

`SocketReceiveResult` описывает результат одного successful receive operation:

- `size_bytes` — сколько bytes было прочитано в caller-provided buffer;
- `receive_timestamp_ns` — timestamp приема datagram-а.

Этот timestamp дальше может использоваться receive pipeline-ом как локальное время поступления packet-а.

---

##### `ISocketRxPort`

```c++
class ISocketRxPort {
  public:
    virtual bool is_open() const noexcept = 0;

    virtual Error open(const SocketRxOpenConfig &cfg) = 0;

    virtual Error close() = 0;

    virtual std::expected<SocketReceiveResult, Error>
    receive(std::span<std::uint8_t> buffer) = 0;

    virtual ~ISocketRxPort() = default;
};
```

`ISocketRxPort` — platform-neutral interface одного socket receive port-а.

Socket backend работает через этот interface, не завязываясь напрямую на Linux socket API.

Lifecycle:

```text
create port
→ open(SocketRxOpenConfig)
→ receive(buffer)
→ close()
```

`receive` читает один UDP datagram в переданный buffer и возвращает `SocketReceiveResult`.

Ошибки socket layer-а приводятся к project-level `Error`.

---

##### `ISocketRxPortFactory`

```c++
class ISocketRxPortFactory {
  public:
    virtual std::unique_ptr<ISocketRxPort> create_port() const = 0;

    virtual ~ISocketRxPortFactory() = default;
};
```

`ISocketRxPortFactory` создает concrete `ISocketRxPort`.

Это нужно, чтобы Socket backend мог быть независимым от конкретной platform implementation:

```text
Socket backend
→ ISocketRxPortFactory
→ ISocketRxPort
```

На Linux factory должна создавать Linux socket receive port.

В тестовой или unsupported среде factory может создавать stub implementation.

---

##### Остальные helper-ы файла

Файл также содержит вспомогательные функции и структуры для:

- определения multicast IPv4/IPv6 address;
- валидации endpoint/membership/source-filter config;
- преобразования socket-specific errors в project-level `Error`;
- сравнения socket config-ов.

Эти helper-ы обслуживают построение и проверку `SocketRxOpenConfig`, но не являются самостоятельным этапом pipeline-а.

---

#### `linux_socket_rx_port.hpp`

Содержит Linux-реализацию platform-neutral socket receive port interface из `socket_runtime.hpp`.

Файл реализует concrete UDP receive port для Socket backend-а:

```c++
class LinuxSocketRxPort final : public ISocketRxPort;
```

и factory для создания таких портов:

```c++
class LinuxSocketRxPortFactory final : public ISocketRxPortFactory;

std::unique_ptr<ISocketRxPortFactory> make_linux_socket_rx_port_factory();
```

---

##### `LinuxSocketRxPort`

`LinuxSocketRxPort` реализует lifecycle одного UDP receive socket-а:

```text
open(SocketRxOpenConfig)
→ receive(buffer)
→ close()
```

Класс хранит:

```c++
int native_socket_;
std::optional<SocketRxOpenConfig> open_cfg_;
```

`native_socket_` — Linux socket file descriptor.

`open_cfg_` — config, с которым port был открыт. Он нужен не только для проверки состояния, но и для source-filter проверки при чтении packets.

---

##### Opening socket-а

```c++
Error open(const SocketRxOpenConfig &cfg) override;
```

`open` выполняет Linux-specific setup по `SocketRxOpenConfig`:

1. Проверяет, что port еще не открыт.
2. Проверяет, что запрошенная конфигурация поддерживается текущей реализацией.
3. Создает native UDP socket через `socket(...)`.
4. Настраивает socket перед bind-ом, сейчас это `SO_REUSEADDR`.
5. Выполняет `bind(...)` на address/port из `cfg.bind_endpoint`.
6. Если в config есть multicast membership, выполняет multicast join.
7. Сохраняет `cfg` и native socket fd как открытое состояние.

Для IPv4 multicast поддерживаются:

```text
IP_ADD_MEMBERSHIP
IP_ADD_SOURCE_MEMBERSHIP
```

То есть обычный multicast join и source-specific multicast join.

IPv6 multicast membership не реализован.

---

##### Closing socket-а

```c++
Error close() override;
```

`close` закрывает открытый port:

1. Если port не открыт, возвращает `Error::Ok`.
2. Если был multicast join, пытается выполнить multicast leave.
3. Вызывает `shutdown(..., SHUT_RDWR)`.
4. Закрывает native fd через `close(...)`.
5. Сбрасывает внутреннее состояние.

Для IPv4 multicast leave используются:

```text
IP_DROP_MEMBERSHIP
IP_DROP_SOURCE_MEMBERSHIP
```

---

##### Receiving datagram-а

```c++
std::expected<SocketReceiveResult, Error>
receive(std::span<std::uint8_t> buffer) override;
```

`receive` читает один UDP datagram через `recvfrom(...)`.

Если чтение успешно, функция формирует:

```c++
SocketReceiveResult {
    .size_bytes = ...,
    .receive_timestamp_ns = ...
}
```

`receive_timestamp_ns` берется из локального monotonic clock:

```c++
std::chrono::steady_clock
```

Если задан `source_filter`, port проверяет sender address datagram-а. Datagram-ы от неподходящих source address-ов silently пропускаются, и `receive` продолжает ждать следующий packet.

Ошибки `recvfrom` маппятся так:

```text
EINTR             → OperationInterrupted
EBADF / ENOTSOCK → OperationAborted
остальное        → SystemFailure
```

---

##### Source filter

Source filter применяется на receive side после `recvfrom`.

Для IPv4 sender address сравнивается с каждым разрешенным source address из `SocketSourceFilter`.

Для IPv6 используется сравнение `in6_addr`.

Это runtime-фильтрация на уровне socket port-а. Для IPv4 multicast с source filter также используется kernel-level source-specific membership через `IP_ADD_SOURCE_MEMBERSHIP`, если config содержит source addresses.

---

##### Factory

```c++
class LinuxSocketRxPortFactory final : public ISocketRxPortFactory {
  public:
    std::unique_ptr<ISocketRxPort> create_port() const override;
};
```

Factory создает новые экземпляры `LinuxSocketRxPort`.

```c++
std::unique_ptr<ISocketRxPortFactory> make_linux_socket_rx_port_factory();
```

`make_linux_socket_rx_port_factory` возвращает factory, которую Socket backend может использовать без прямой зависимости от concrete class.

---

#### `socket_stub_rx_port.hpp`

Содержит stub-реализацию `ISocketRxPort` для сред, где реальный platform socket receive port не поддержан или не выбран.

Основной класс:

```c++
class SocketStubRxPort final : public ISocketRxPort;
```

`SocketStubRxPort` реализует тот же interface, что и реальный socket port, но не выполняет фактический receive UDP datagrams.

Поведение:

- `open` валидирует `SocketRxOpenConfig`, сохраняет config и переводит port в open state;
- `close` сбрасывает open state;
- `is_open` возвращает текущее состояние port-а;
- `receive` не читает данные и возвращает `Error::Unsupported`.

Factory:

```c++
class SocketStubRxPortFactory final : public ISocketRxPortFactory;

std::unique_ptr<ISocketRxPortFactory> make_socket_stub_rx_port_factory();
```

`SocketStubRxPortFactory` создает экземпляры `SocketStubRxPort`.

Файл нужен как заглушка для unsupported/non-Linux окружений или для конфигураций, где реальный socket backend platform adapter недоступен.

Этот файл не реализует production receive path. Он только сохраняет общий interface contract и позволяет коду зависеть от `ISocketRxPortFactory`, даже когда реальная socket implementation отсутствует.

### `st2110/backends`

#### `backend.hpp`

Содержит общий contract для receive backend-ов и sink interface, через который backend передает готовые media units наружу.

Файл задает границу между backend runtime и downstream delivery/synchronization layer:

```text
IRxBackend
→ IFrameSink
→ video/audio delivery
```

---

##### Lifecycle result

```c++
using RxBackendLifecycleResult = std::expected<bool, Error>;
```

`RxBackendLifecycleResult` используется методами lifecycle-а backend-а.

`bool` позволяет backend-у вернуть не только ошибку, но и факт изменения состояния, например:

```text
true  — операция реально изменила состояние;
false — backend уже был в нужном состоянии.
```

---

##### Backend stats

```c++
struct BackendStats {
    uint64_t datagrams_received;
    uint64_t bytes_received;

    uint64_t control_datagrams_ignored;
    uint64_t nonmedia_datagrams_ignored;

    uint64_t packets_parsed_ok;
    uint64_t packets_rejected;

    uint64_t frames_delivered;
    uint64_t datagrams_dropped;
    uint64_t media_units_delivered;

    PacketParseStats packet_parse;
    ReorderBufferStats reorder;
    DepacketizerStats depacketizer;
};
```

`BackendStats` — общий snapshot статистики receive backend-а.

Поля верхнего уровня отражают общие счетчики:

- сколько datagrams принято;
- сколько bytes принято;
- сколько datagrams отброшено или проигнорировано;
- сколько packets успешно распарсено;
- сколько packets отклонено;
- сколько media units / frames доставлено наружу.

Вложенные структуры дают более детальную статистику отдельных этапов project-owned receive path:

```c++
PacketParseStats packet_parse;
ReorderBufferStats reorder;
DepacketizerStats depacketizer;
```

Эти поля особенно относятся к Socket path, где проект сам выполняет packet parsing, reorder и depacketize.

Для backend-ов, которые не проходят через эти этапы напрямую, например MTL path, часть этих counters может оставаться нулевой или заполняться backend-local образом.

---

##### Frame timing metadata

```c++
struct FrameTimingMetadata {
    std::uint32_t rtp_timestamp;
    TimestampNs receive_timestamp_ns;

    VideoScanMode video_scan_mode;
    bool video_second_field;
};
```

`FrameTimingMetadata` сопровождает video frame или audio block при передаче в sink.

Поля:

- `rtp_timestamp` — RTP timestamp исходного media unit-а;
- `receive_timestamp_ns` — локальный timestamp приема;
- `video_scan_mode` — scan mode для video path;
- `video_second_field` — признак второго поля для interlaced video.

---

##### Frame sink interface

```c++
class IFrameSink {
  public:
    virtual void on_video_frame(
        VideoFrame frame,
        FrameTimingMetadata timing_metadata
    ) = 0;

    virtual void on_audio_frame(
        AudioBuffer frame,
        FrameTimingMetadata timing_metadata
    ) = 0;

    virtual ~IFrameSink() = default;
};
```

`IFrameSink` — интерфейс получателя готовых media units от backend-а.

Backend не должен напрямую знать про OBS, synchronized delivery или конкретный output adapter. Он только вызывает:

```text
on_video_frame(...)
on_audio_frame(...)
```

и передает туда уже собранный `VideoFrame` или `AudioBuffer` вместе с timing metadata.

---

##### Receive backend interface

```c++
class IRxBackend {
  public:
    virtual RxBackendLifecycleResult stop() = 0;
    virtual RxBackendLifecycleResult start(IFrameSink *sink) = 0;

    virtual BackendStats stats_snapshot() const;
    virtual bool healthy() const;
    virtual std::string last_error_message() const;

    virtual ~IRxBackend() = default;
};
```

`IRxBackend` — общий interface receive backend-а.

Lifecycle:

```text
start(IFrameSink*)
→ receive/runtime loop
→ callbacks into sink
→ stop()
```

`start` получает `IFrameSink*`, через который backend должен отдавать готовые video/audio данные.

`stop` останавливает backend runtime.

`stats_snapshot` возвращает текущую статистику backend-а.

`healthy` показывает, находится ли backend в рабочем состоянии.

`last_error_message` возвращает последнюю diagnostic error string, если backend ее поддерживает.

Default implementations:

```c++
stats_snapshot()      → empty BackendStats
healthy()             → true
last_error_message()  → empty string
```

То есть конкретный backend может переопределить только те diagnostics, которые он реально поддерживает.

---

#### `socket_rx_single_media_backend_base.hpp`

Содержит общий base class для Socket receive backend-а одного media type.

Этот файл реализует общую socket-side receive pipeline логику, которая одинакова для video и audio:

```text
open socket ports
→ receive UDP datagrams
→ filter/control handling
→ RTP/media packet parsing
→ packet queue
→ duplicate merge for redundant legs
→ reorder buffer
→ media-specific delivery
```

Media-specific backend-ы наследуются от `SocketRxSingleMediaBackendBase` и должны определить только те части, которые зависят от media type.

---

##### Runtime leg

```c++
struct SocketRxRuntimeLeg {
    std::jthread thread;
    std::unique_ptr<ISocketRxPort> port;
    std::vector<std::uint8_t> receive_buffer;
};
```

`SocketRxRuntimeLeg` хранит runtime-состояние одной receive leg:

- thread, который читает datagrams из socket port-а;
- concrete `ISocketRxPort`;
- receive buffer для UDP payload-а.

При shutdown leg закрывает port, останавливает thread и очищает buffer.

---

##### Packet queue

```c++
class SocketRxStoredPacketQueue;
```

`SocketRxStoredPacketQueue` — thread-safe очередь между receive threads и downstream processing thread.

Receive threads кладут туда уже распаршенные и сохраненные пакеты:

```text
receive thread
→ StoredPacket queue
```

Downstream thread забирает пакеты из очереди:

```text
StoredPacket queue
→ reorder buffer
→ media delivery
```

Очередь поддерживает:

- `reset` — открыть очередь и очистить старые пакеты;
- `close` — закрыть очередь и разбудить waiting thread;
- `push` — добавить пакет;
- `wait_pop` — ждать пакет или stop/close.

---

##### Duplicate merge history

```c++
struct DuplicateMergeHistory;
```

`DuplicateMergeHistory` используется для redundant topology, когда backend принимает две legs одного stream-а.

Он хранит недавно обработанные `reorder_sequence` и позволяет отбрасывать duplicate packets, которые пришли с другой leg.

Размер history привязан к reorder window:

```c++
duplicate_history_capacity() == reorder_buffer_config_.window_size_packets
```

---

##### Base backend class

```c++
class SocketRxSingleMediaBackendBase : public virtual IRxBackend;
```

`SocketRxSingleMediaBackendBase` реализует общий lifecycle и общий receive path для Socket backend-а.

Класс сам не знает, video это или audio. Media-specific детали вынесены в virtual methods.

---

##### Что должен реализовать media-specific backend

Наследник обязан определить три метода:

```c++
virtual std::expected<std::unique_ptr<PacketView>, Error>
parse_packet(std::size_t leg_index, ByteSpan udp_payload) = 0;

virtual std::unique_ptr<IReorderBuffer>
make_reorder_buffer(const ReorderBufferConfig &cfg) = 0;

virtual void deliver_media(std::unique_ptr<StoredPacket> packet) = 0;
```

`parse_packet` выполняет media-specific parsing UDP payload-а.

Например:

```text
Socket video backend:
    RTP + ST 2110-20 video packet parse

Socket audio backend:
    RTP + ST 2110-30 audio packet parse
```

`make_reorder_buffer` создает reorder buffer нужного типа.

`deliver_media` получает packet после reorder stage и выполняет media-specific обработку:

```text
video:
    depacketizer
    video unit reconstructor
    IFrameSink::on_video_frame

audio:
    audio block assembly
    IFrameSink::on_audio_frame
```

---

##### Startup sequence

Общий startup выполняется через:

```c++
RxBackendLifecycleResult start_common_runtime(IFrameSink *sink);
```

Media-specific backend вызывает этот метод из своего `start`.

Порядок работы:

```text
1. Сбросить stopping flag
2. Сбросить diagnostics/statistics
3. Сохранить sink
4. Создать runtime legs
5. Для каждой SocketRxOpenConfig:
   - создать ISocketRxPort через factory
   - открыть port
   - создать receive buffer
6. Сбросить packet queue
7. Включить duplicate merge, если legs больше одной
8. Сохранить runtime legs
9. Запустить downstream thread
10. Запустить receive thread для каждой leg
```

После этого backend начинает принимать UDP datagrams.

---

##### Receive thread pipeline

Каждая leg имеет свой receive loop:

```c++
void run_receive_loop(std::size_t leg_index, std::stop_token stop_token) noexcept;
```

Pipeline внутри receive loop:

```text
ISocketRxPort::receive(buffer)
→ record datagram statistics
→ process_received_datagram(...)
```

Если `receive` возвращает `OperationInterrupted`, loop продолжает работу.

Если receive завершается другой ошибкой, leg помечается unhealthy, и receive loop выходит.

---

##### Datagram processing

```c++
void process_received_datagram(
    std::size_t leg_index,
    ByteSpan udp_payload,
    TimestampNs receive_timestamp_ns
) noexcept;
```

Этот метод выполняет common filtering/parsing path:

```text
UDP payload
→ RTCP-like datagram check
→ packet parse policy validation
→ media-specific parse_packet
→ expected payload type check
→ attach receive timestamp
→ store packet
→ push to packet queue
```

Подробно:

1. RTCP-like datagrams игнорируются и учитываются как `control_datagrams_ignored`.
2. Datagram проверяется через `validate_packet_parse_policy`.
3. Затем вызывается media-specific `parse_packet`.
4. Если RTP payload type не совпадает с `expected_payload_type_`, datagram игнорируется как non-media.
5. В packet записывается `receive_timestamp_ns`.
6. Packet превращается в `StoredPacket`.
7. `StoredPacket` кладется в `SocketRxStoredPacketQueue`.

В дальнейшем нужно будет добавить пайплайн для обработки RTCP-like датаграмм и передачи пар timestamp в маппер для поддержки mediaclk=sender.
---

##### Downstream thread pipeline

Downstream loop:

```c++
void run_downstream_loop(std::stop_token stop_token) noexcept;
```

Pipeline:

```text
wait_pop packet from queue
→ process_stored_packet_downstream
```

`process_stored_packet_downstream` делает:

```text
StoredPacket
→ duplicate merge check
→ reorder_buffer.push
→ remember sequence in duplicate history
→ drain_reorder_buffer_to_sink
```

Если duplicate merge включен и sequence уже был обработан, packet отбрасывается.

---

##### Reorder drain

```c++
void drain_reorder_buffer_to_sink();
```

Метод пытается вытащить из reorder buffer все packets, которые уже можно доставить дальше.

Pipeline:

```text
reorder_buffer.pop_next()
→ deliver_media(packet)
```

Если next packet недоступен из-за gap-а, применяется configured reorder policy.

Сейчас base class реализует такие policies:

```text
WaitForMissing
FlushGapOnce
FlushAfterNPackets
FlushOnMarkerBoundary
```

Остальные значения `ReceiveReorderGapPolicy` в этом base class пока не реализованы и приводят к exception:

```c++
throw std::runtime_error("Such reorder policy is not implemented yet");
```

---

##### Stop sequence

```c++
RxBackendLifecycleResult stop() override;
```

Stop делает:

```text
1. Установить stopping flag
2. Закрыть все open ports
3. Очистить runtime legs
4. Закрыть packet queue
5. Остановить downstream thread
6. Очистить duplicate merge history
7. Сбросить reorder buffer
8. Выключить duplicate merge
9. Вернуть ошибку, если закрытие port-а завершилось ошибкой
```

При ошибке backend помечается unhealthy и сохраняет diagnostic message.

---

##### Diagnostics

Base class ведет общую backend diagnostics:

```c++
BackendStats stats_;
bool healthy_;
std::string last_error_message_;
```

Публичные методы:

```c++
BackendStats stats_snapshot() const override;
bool healthy() const override;
std::string last_error_message() const override;
```

Статистика обновляется на common этапах:

- datagrams received;
- bytes received;
- RTCP/control datagrams ignored;
- non-media datagrams ignored;
- packets parsed ok;
- packets rejected;
- datagrams dropped;
- media units delivered;
- frames delivered.

Media-specific backend может использовать protected helper-ы:

```c++
record_media_unit_delivered_noexcept();
record_frame_delivered_noexcept();
```

---

##### `.cpp`: default port factory

```c++
std::unique_ptr<ISocketRxPortFactory>
SocketRxSingleMediaBackendBase::make_default_port_factory();
```

Этот метод реализован в `.cpp`, потому что выбор concrete socket implementation зависит от ОС.

Текущая логика:

```text
Linux:
    make_linux_socket_rx_port_factory()

Other OS:
    make_socket_stub_rx_port_factory()
```

---

### `st2110/ingress/shared`

#### `rtp.hpp`

Содержит общий RTP header parser и helper-ы для работы с RTP sequence numbers.

Файл находится на ingress/common packet parsing уровне:

```text
UDP payload
→ RTP header parsing
→ RTP payload span
→ media-specific packet parsing
```

Он не знает, video это или audio. Его задача — строго разобрать общий RTP header и вернуть view на RTP payload, который дальше будет передан ST 2110-20 или ST 2110-30 parser-у.

---

##### RTP header view

```c++
struct RtpHeaderView {
    std::uint8_t version;
    bool padding_flag;
    bool extension_flag;
    std::uint8_t csrc_count;
    bool marker;
    std::uint8_t payload_type;
    std::uint16_t seq_number;
    std::uint32_t timestamp;
    std::uint32_t ssrc;
    std::size_t payload_offset;
    std::size_t payload_len;
};
```

`RtpHeaderView` не владеет packet data. Он только описывает разобранный RTP header и положение payload-а внутри исходного UDP payload-а.

Основные поля:

- `version` — RTP version, должен быть `2`;
- `padding_flag` — RTP padding bit;
- `extension_flag` — RTP extension bit;
- `csrc_count` — количество CSRC entries;
- `marker` — RTP marker bit;
- `payload_type` — RTP payload type;
- `seq_number` — RTP sequence number;
- `timestamp` — RTP timestamp;
- `ssrc` — RTP SSRC;
- `payload_offset` — offset RTP payload-а внутри UDP payload-а;
- `payload_len` — длина RTP payload-а без RTP padding.

---

##### RTP header parsing

```c++
std::expected<RtpHeaderView, Error>
parse_rtp_header(ByteSpan udp_payload);
```

`parse_rtp_header` разбирает RTP header из UDP payload-а.

Порядок parsing-а:

```text
1. Проверить минимальный размер RTP header-а: 12 bytes
2. Проверить RTP version == 2
3. Прочитать flags, marker, payload type
4. Прочитать sequence number, timestamp, SSRC в Big-Endian порядке
5. Учесть CSRC list
6. Если установлен extension bit — пропустить RTP extension header и extension data
7. Если установлен padding bit — вычесть padding из payload length
8. Вернуть RtpHeaderView
```

---

##### RTP extension handling

Если установлен `extension_flag`, parser ожидает RTP extension header:

```text
16-bit defined-by-profile
16-bit length in 32-bit words
extension data
```

Из extension header используется только длина:

```c++
const std::uint16_t len_words =
    endian::read_be16(udp_payload.subspan(offset + 2, 2));
```

Затем parser увеличивает `payload_offset` на:

```text
4 + len_words * 4
```

Содержимое RTP extension не интерпретируется. Файл только корректно пропускает extension block, чтобы найти начало media payload-а.

---

##### RTP padding handling

Если установлен `padding_flag`, последний byte packet-а считается размером padding-а:

```c++
pad = udp_payload.back();
```

Payload length вычисляется как:

```text
payload_len = udp_payload.size() - pad - payload_offset
```

Padding не входит в `rtp_payload_span`.

---

##### Sequence number helpers

```c++
bool seq_less(std::uint16_t a, std::uint16_t b);
```

`seq_less` сравнивает RTP sequence numbers с учетом 16-bit wrap-around.

Она возвращает `true`, если `b` находится впереди `a` на расстоянии меньше половины sequence number space:

```text
0 < (b - a) < 32768
```

Это нужно для корректного порядка RTP packets при переходе через `65535 → 0`.

```c++
std::int32_t seq_distance(std::uint16_t a, std::uint16_t b);
```

`seq_distance` возвращает signed distance между RTP sequence numbers с учетом wrap-around.

Примеры:

```text
seq_distance(10, 12)      → 2
seq_distance(65535, 0)    → 1
seq_distance(12, 10)      → -2
```

Эти helper-ы используются reorder логикой.

---

##### RTP payload span

```c++
ByteSpan rtp_payload_span(
    ByteSpan udp_payload,
    const RtpHeaderView &header
);
```

Возвращает view на RTP payload:

```text
udp_payload[payload_offset : payload_offset + payload_len]
```

Этот span уже не включает:

- RTP fixed header;
- CSRC list;
- RTP extension;
- RTP padding.

Дальше именно этот payload должен обрабатываться media-specific parser-ом.

---

#### `packet_view.hpp`

Содержит базовый polymorphic view для уже распаршенного media packet-а.

---

##### `PacketView`

```c++
struct PacketView {
    RtpHeaderView rtp;
    ByteSpan payload_data;
    TimestampNs receive_timestamp_ns;

    virtual std::uint32_t reorder_sequence() const = 0;
    virtual std::unique_ptr<StoredPacket> store() const = 0;

    virtual ~PacketView() = default;
};
```

`PacketView` — общий abstract base для media-specific packet view.

Он хранит common packet metadata:

- `rtp` — разобранный RTP header;
- `payload_data` — view на RTP payload;
- `receive_timestamp_ns` — локальный timestamp приема UDP datagram-а.

Сам `PacketView` не владеет packet bytes. Он только ссылается на данные через `ByteSpan`.

---

##### `reorder_sequence`

```c++
virtual std::uint32_t reorder_sequence() const = 0;
```

Возвращает sequence value, по которому packet должен упорядочиваться в reorder buffer-е.

Обычно это RTP sequence number, расширенный или приведенный к типу `std::uint32_t`.

Метод virtual, потому что разные media packet types могут определять reorder key по-разному, но для socket RTP path это обычно связано с RTP sequence number.

---

##### `store`

```c++
virtual std::unique_ptr<StoredPacket> store() const = 0;
```

Создает owning representation packet-а для downstream обработки.

Это нужно потому, что `PacketView` содержит `ByteSpan` и живет поверх receive buffer-а, который будет переиспользован receive loop-ом.

Поэтому перед передачей packet-а в очередь нужно сделать owned copy / stored representation:

```text
PacketView over receive buffer
→ store()
→ StoredPacket owned by queue/reorder/downstream thread
```

---

Socket backend base работает с `PacketView` абстрактно:

```text
parse_packet(...)
→ PacketView
→ check RTP payload type
→ attach receive timestamp
→ store()
→ packet queue
```

Video и audio parser-ы должны возвращать свои concrete `PacketView` implementations.

#### `reorder_buffer.hpp`

Содержит общий contract для reorder buffer-а и owning representation packet-а, который можно безопасно передавать между receive thread, packet queue и downstream processing.

---

##### `StoredPacket`

```c++
struct StoredPacket {
    RtpHeaderView rtp_;
    std::vector<uint8_t> payload_data;
    std::uint32_t extended_seq;
    TimestampNs receive_timestamp_ns;

    std::uint32_t reorder_sequence() const;
    virtual std::unique_ptr<PacketView> view() const = 0;

    virtual ~StoredPacket() = default;
};
```

`StoredPacket` — owning packet representation.

`StoredPacket` хранит:

- `rtp_` — разобранный RTP header;
- `payload_data` — owned copy RTP payload-а;
- `extended_seq` — sequence number, по которому packet упорядочивается;
- `receive_timestamp_ns` — локальный timestamp приема UDP datagram-а.

```c++
std::uint32_t reorder_sequence() const;
```

возвращает `extended_seq`.

```c++
virtual std::unique_ptr<PacketView> view() const = 0;
```

создает media-specific `PacketView` поверх owned payload data.

---

##### `IReorderBuffer`

```c++
class IReorderBuffer {
  public:
    virtual Error push(std::unique_ptr<StoredPacket> packet) = 0;
    virtual Error push(const PacketView &packet);

    virtual std::unique_ptr<StoredPacket> pop_next() = 0;

    virtual bool flush_missing_once() = 0;
    virtual bool flush_after_n_packets(std::uint32_t threshold_packets) = 0;
    virtual bool flush_missing_until_marker_boundary() = 0;

    virtual void reset() = 0;

    virtual ReorderBufferStats stats() const = 0;

    virtual ~IReorderBuffer() = default;
};
```

`IReorderBuffer` — общий interface reorder stage-а.

Он принимает `StoredPacket`, хранит packets до тех пор, пока их можно будет выдать в правильном sequence order, и возвращает следующий готовый packet через `pop_next`.

Основной pipeline:

```text
StoredPacket
→ push(...)
→ reorder buffer internal state
→ pop_next()
→ media-specific deliver_media(...)
```

---

##### Вставка packet-а

```c++
virtual Error push(std::unique_ptr<StoredPacket> packet) = 0;
```

Добавляет owned packet в reorder buffer.

Также есть overload:

```c++
virtual Error push(const PacketView &packet) {
    return push(packet.store());
}
```

Он сразу вызывает `store()` у `PacketView`, чтобы получить owning representation.

---

##### Получение следующего packet-а

```c++
virtual std::unique_ptr<StoredPacket> pop_next() = 0;
```

Возвращает следующий packet, который можно доставить дальше по порядку.

---

##### Gap flush methods

```c++
virtual bool flush_missing_once() = 0;

virtual bool flush_after_n_packets(std::uint32_t threshold_packets) = 0;

virtual bool flush_missing_until_marker_boundary() = 0;
```

Эти методы не доставляют media сами. Они только меняют внутреннее состояние reorder buffer-а так, чтобы gap можно было пропустить.

`flush_missing_once` пропускает один missing sequence gap.

`flush_after_n_packets` разрешает flush, если после gap-а уже накопилось достаточно packets.

`flush_missing_until_marker_boundary` пропускает missing packets до marker boundary. Эта policy применима прежде всего к video path.

После successful flush backend снова вызывает:

```text
pop_next()
```

и доставляет packets через media-specific `deliver_media`.

---

##### Reset и stats

```c++
virtual void reset() = 0;
```

Сбрасывает внутреннее состояние reorder buffer-а.

```c++
virtual ReorderBufferStats stats() const = 0;
```

Возвращает статистику reorder stage-а.

---

Media-specific backend-и создают конкретный reorder buffer через:

```c++
make_reorder_buffer(const ReorderBufferConfig &cfg)
```

#### `fixed_reorder_buffer.hpp`

Содержит общую реализацию fixed-window reorder buffer-а для Socket receive path.

Файл реализует один reorder algorithm, который используется и для video, и для audio:

```c++
template <bool is_video_ = false>
class FixedWindowReorderBuffer final : public IReorderBuffer;
```

Различие между video/audio задается template-параметром:

- `FixedWindowReorderBuffer<true>` — video reorder buffer;
- `FixedWindowReorderBuffer<false>` — audio reorder buffer.

---

##### Stored packet representations

Файл содержит concrete `StoredPacket`-типы для video и audio:

```c++
struct VideoStoredPacket final : StoredPacket;
struct AudioStoredPacket final : StoredPacket;
```

`VideoStoredPacket` сохраняет owned copy RTP payload-а и дополнительно копирует SRD segment headers:

```c++
SrdHeader segment_headers[maxPacketSrdSegments];
std::uint8_t segment_count;
```

`AudioStoredPacket` сохраняет owned copy RTP payload-а и audio metadata:

```c++
sampling_rate_hz
channel_count
samples_per_channel
bit_depth
```

Оба типа реализуют:

```c++
std::unique_ptr<PacketView> view() const override;
```

`view()` восстанавливает media-specific `PacketView` поверх owned `payload_data`.

---

##### `store()` для media-specific packet views

В конце файла реализуются методы:

```c++
std::unique_ptr<StoredPacket> AudioPacketView::store() const;
std::unique_ptr<StoredPacket> VideoPacketView::store() const;
```

---

##### Fixed window reorder algorithm

`FixedWindowReorderBuffer` хранит packets в ordered map:

```c++
std::map<std::uint32_t, std::unique_ptr<StoredPacket>> packets_;
```

Ключ map-а — reorder sequence.

Для video используется 32-bit sequence:

```c++
std::uint32_t next_expected_seq_;
```

Для audio используется RTP-like 16-bit sequence с wrap-around логикой:

```c++
std::uint16_t next_expected_audio_seq_;
```

При первом packet-е buffer инициализирует `next_expected_*` этим packet sequence.

---

##### Push

```c++
Error push(std::unique_ptr<StoredPacket> packet) override;
```

`push` добавляет packet в reorder window.

Общая логика:

```text
1. Если buffer еще не инициализирован — принять sequence как next expected.
2. Посчитать distance от next expected до incoming sequence.
3. Если packet слишком старый — считать late packet и отклонить.
4. Если packet слишком далеко впереди window-а — выполнить recovery advance.
5. Если такой sequence уже есть — считать duplicate и отклонить.
6. Иначе сохранить packet в map.
```

Для forward out-of-window packet-а buffer пытается не сбрасывать все состояние. Он продвигает `next_expected_*` так, чтобы сохранить уже накопленные полезные packets, если это возможно.

---

##### Pop

```c++
std::unique_ptr<StoredPacket> pop_next() override;
```

`pop_next` возвращает следующий packet только если в buffer-е есть packet с текущим expected sequence.

Если packet найден:

```text
return packet
advance next expected sequence
```

Если packet отсутствует:

```text
return nullptr
```

После `nullptr` backend может применить configured gap policy через flush methods.

---

##### Gap flush methods

Файл реализует методы из `IReorderBuffer`:

```c++
bool flush_missing_once() override;

bool flush_after_n_packets(std::uint32_t threshold_packets) override;

bool flush_missing_until_marker_boundary() override;
```

`flush_missing_once` пропускает один missing sequence number и сдвигает expected sequence вперед.

`flush_after_n_packets` делает flush только если после gap-а уже накопилось не меньше заданного количества packets.

`flush_missing_until_marker_boundary` делает flush только если в buffer-е уже есть packet с RTP marker bit. Эта policy полезна прежде всего для video path.

---

##### Stats

Buffer обновляет `ReorderBufferStats` (см. reorder_stats.hpp):

```c++
ReorderBufferStats stats() const override;
```

Статистика учитывает:

- pushed packets;
- stored packets;
- popped packets;
- duplicates;
- late packets;
- out-of-window packets;
- missing sequence events;
- flushed missing sequences.

---

Далее см. socket_video/audio_pipeline.md

