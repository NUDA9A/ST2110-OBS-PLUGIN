### `st2110/backends/socket`

#### `socket_rx_video_backend.hpp`

Содержит concrete Socket receive backend для video.

Файл связывает общий socket receive runtime из `SocketRxSingleMediaBackendBase` с video-specific обработкой:

```text
SocketVideoStartConfig
→ socket receive runtime
→ RTP/ST 2110-20 packet parsing
→ video reorder buffer
→ video receive pipeline
→ IFrameSink::on_video_frame
```

---

##### `SocketRxVideoBackend`

```c++
class SocketRxVideoBackend final : public SocketRxSingleMediaBackendBase;
```

`SocketRxVideoBackend` наследуется от общего socket backend base и реализует video-specific части pipeline-а.

Конструктор принимает:

```c++
SocketRxVideoBackend(const SocketVideoStartConfig &cfg);
```

Из `SocketVideoStartConfig` он передает в base class:

```text
default socket port factory
MAXUDP первой leg
ReorderBufferConfig
SocketRxOpenConfig для всех legs
expected RTP payload type
```

Также backend создает video receive pipeline:

```c++
VideoReceivePipeline video_receive_pipeline_;
```

из:

```c++
cfg.video_receive_pipeline_config
```

---

##### Start sequence

```c++
RxBackendLifecycleResult start(IFrameSink *sink) override;
```

`start` выполняет video-specific initialization, а затем запускает общий socket runtime:

```text
1. Создать video reorder buffer
2. Вызвать start_common_runtime(sink)
```

После этого управление переходит в общий pipeline из `SocketRxSingleMediaBackendBase`:

---

##### Packet parsing

```c++
std::expected<std::unique_ptr<PacketView>, Error>
parse_packet(std::size_t leg_index, ByteSpan udp_payload) override;
```

`parse_packet` — video-specific реализация abstract method из base class.

Она получает raw UDP payload и вызывает video packet parser:

```c++
parse_packet_view(udp_payload)
```

Результат оборачивается в polymorphic `PacketView`:

```c++
std::make_unique<VideoPacketView>(*packet)
```

---

##### Reorder buffer

```c++
std::unique_ptr<IReorderBuffer>
make_reorder_buffer(const ReorderBufferConfig &cfg) override;
```

Video backend использует fixed-window reorder buffer в video mode:

```c++
FixedWindowReorderBuffer<true>
```

Размер окна берется из:

```c++
cfg.window_size_packets
```

То есть video path использует 32-bit reorder sequence из `VideoPacketView`.

---

##### Media delivery

```c++
void deliver_media(std::unique_ptr<StoredPacket> packet) override;
```

`deliver_media` вызывается base class-ом после reorder stage.

Pipeline:

```text
StoredPacket
→ VideoPacketView
→ VideoReceivePipeline::push
→ ReconstructedVideoFrame[]
→ IFrameSink::on_video_frame
```

Шаги:

1. Получить media-specific view из stored packet:
2. Передать packet в video receive pipeline:
3. Доставить все reconstructed frames в sink:
---

##### Frame delivery

```c++
void deliver_reconstructed_frame(
    ReconstructedVideoFrame &&frame
) const noexcept;
```

Метод обновляет статистику:

```text
frames_delivered
media_units_delivered
```

и вызывает sink callback.

---

##### Shutdown

```c++
~SocketRxVideoBackend() override;
```

Деструктор вызывает:

```c++
SocketRxSingleMediaBackendBase::stop()
```

Это гарантирует остановку receive threads, закрытие ports, очистку queue и reorder buffer.

---

#### `video_packet_view.hpp`

Содержит media-specific packet view для ST 2110-20 video packets.

---

##### SRD segment view

```c++
inline constexpr std::size_t maxPacketSrdSegments = 3;

struct SrdSegmentView {
    SrdHeader header;
    ByteSpan data;
};
```

`SrdSegmentView` описывает один SRD segment внутри ST 2110-20 RTP payload-а.

Он содержит:

- `header` — разобранный SRD header;
- `data` — view на payload bytes этого segment-а.

---

##### `VideoPacketView`

```c++
struct VideoPacketView final : PacketView {
    SrdSegmentView segments[maxPacketSrdSegments];
    std::uint8_t segment_count;
    std::uint32_t extended_seq;
    ByteSpan trailing_padding;

    std::unique_ptr<StoredPacket> store() const override;
    std::uint32_t reorder_sequence() const override;
};
```

`VideoPacketView` — concrete `PacketView` для video RTP packet-а.

Он наследует common packet metadata из `PacketView`:

```c++
RtpHeaderView rtp;
ByteSpan payload_data;
TimestampNs receive_timestamp_ns;
```

и добавляет video-specific данные:

- `segments` — массив разобранных SRD segments;
- `segment_count` — фактическое количество SRD segments;
- `extended_seq` — extended sequence number для reorder buffer-а;
- `trailing_padding` — оставшиеся bytes после разобранных SRD segments.

---

##### Store

```c++
std::unique_ptr<StoredPacket> store() const override;
```

`store()` превращает non-owning `VideoPacketView` в owning `VideoStoredPacket`.

---

#### `st2110_20.hpp`

Содержит parser и validation helper-ы для ST 2110-20 payload header.

---

##### Основные структуры

```c++
struct ExtendedSequenceNumber {
    std::uint16_t hi16;
};
```

`ExtendedSequenceNumber` хранит старшие 16 бит extended sequence number из ST 2110-20 payload header.

Полный 32-битный sequence number получается из:

```text
extended high 16 bits + RTP sequence number low 16 bits
```

```c++
struct SrdHeader {
    std::uint16_t length;
    std::uint16_t row_number;
    std::uint16_t offset;
    bool field_id;
    bool continuation;
};
```

`SrdHeader` описывает один SRD segment:

- `length` — длина segment data;
- `row_number` — номер строки;
- `offset` — offset внутри строки;
- `field_id` — field bit для interlaced/PsF processing;
- `continuation` — признак, что после текущего SRD header-а идет следующий SRD header.

```c++
struct St2110PayloadHeaderView {
    ExtendedSequenceNumber ext_seq;
    SrdHeader srd[3];
    std::uint8_t srd_count;
    std::size_t header_bytes;
};
```

`St2110PayloadHeaderView` хранит результат parsing-а ST 2110-20 payload header-а:

- extended sequence high bits;
- до трех SRD headers;
- фактическое количество SRD headers;
- размер payload header-а в bytes.

---

##### Parsing ST 2110-20 payload header

```c++
std::expected<St2110PayloadHeaderView, Error>
parse_st2110_20_payload_header(ByteSpan payload);
```

Функция разбирает начало RTP payload-а как ST 2110-20 payload header.

Порядок parsing-а:

```text
1. Проверить, что payload содержит минимум 8 bytes
2. Прочитать extended sequence high 16 bits
3. Прочитать первый SRD header
4. Если continuation bit установлен — читать следующий SRD header
5. Остановиться после SRD header-а без continuation bit
6. Не разрешать больше 3 SRD headers в одном packet-е
7. Вернуть header view с количеством SRD headers и размером header-а
```

Каждый SRD header занимает `6 bytes`.

Формат одного SRD header-а:

```text
length:          16 bits
F + row number:  1 bit field_id + 15 bits row_number
C + offset:      1 bit continuation + 15 bits offset
```

Все multi-byte значения читаются в Big-Endian порядке.

---

##### Validation SRD ordering

```c++
Error validate_st2110_20_srd_ordering(
    const St2110PayloadHeaderView &h
);
```

Проверяет порядок SRD segments внутри packet-а.

Для нескольких SRD headers требуется:

```text
row_number должен возрастать
или
при одинаковом row_number offset должен строго возрастать
```

То есть запрещены случаи:

```text
следующий row_number меньше предыдущего
одинаковый row_number, но offset не увеличился
```

---

##### Validation payload header-а

```c++
Error validate_st2110_20_payload_header(
    const St2110PayloadHeaderView &h
);
```

Проверяет структурную корректность уже распаршенного ST 2110-20 payload header-а.

Проверки:

```text
srd_count должен быть от 1 до 3
одиночный SRD не должен иметь continuation bit
для нескольких SRD:
    промежуточные SRD должны иметь continuation bit
    последний SRD не должен иметь continuation bit
    length каждого SRD должен быть больше 0
SRD headers должны идти в корректном row/offset порядке
header_bytes должен совпадать с 2 + 6 * srd_count
```

Функция не проверяет, помещается ли вся segment data в RTP payload.

---

##### Extended sequence construction

```c++
std::uint32_t combine_extended_seq(
    const ExtendedSequenceNumber &ext,
    std::uint16_t lo16
);
```

Объединяет старшие 16 бит из ST 2110-20 payload header-а и младшие 16 бит из RTP sequence number:

---

#### `packet_parse.hpp`

Содержит video packet parser для Socket video receive path.

Файл связывает общий RTP parsing и ST 2110-20 payload header parsing в один этап, который строит `VideoPacketView`:

```text
UDP payload
→ RTP header
→ RTP payload
→ ST 2110-20 payload header
→ SRD segments
→ VideoPacketView
```

---

##### Staged parse failure

```c++
struct PacketViewParseFailure {
    Error error;
    PacketParseStage stage;
};
```

`PacketViewParseFailure` хранит не только ошибку, но и stage, на котором packet parsing failed.

Это нужно для статистики и диагностики:

```text
RtpHeader
St2110PayloadHeaderParse
St2110PayloadHeaderValidate
SrdPayloadSplit
PacketPolicy
```

---

##### Основной staged parser

```c++
std::expected<VideoPacketView, PacketViewParseFailure>
parse_packet_view_staged(ByteSpan udp_payload);
```

Это основной parser, который строит `VideoPacketView`.

Порядок работы:

```text
1. parse_rtp_header(udp_payload)
2. rtp_payload_span(...)
3. parse_st2110_20_payload_header(rtp_payload)
4. validate_st2110_20_payload_header(...)
5. combine_extended_seq(...)
6. отделить ST 2110-20 payload header от segment payload data
7. посчитать суммарную длину SRD payload segments
8. нарезать payload data на SrdSegmentView
9. сохранить trailing padding
10. вернуть VideoPacketView
```

---

##### Extended sequence

После successful ST 2110-20 header parsing-а строится 32-bit sequence number:

```c++
res.extended_seq =
    combine_extended_seq(
        st2110_20_payload_header->ext_seq,
        rtp_header->seq_number
    );
```

Этот `extended_seq` дальше используется video reorder buffer-ом как reorder key.

---

##### SRD payload split

После ST 2110-20 header-а остается payload data:

```c++
res.payload_data =
    rtp_payload.subspan(st2110_20_payload_header->header_bytes);
```

Parser суммирует `length` всех SRD headers:

```text
sum_length = srd[0].length + srd[1].length + ...
```

Если SRD count равен `1` и `length == 0`, то payload data должен быть пустым.
Затем payload нарезается на segment views:

```c++
res.segments[i].data =
    res.payload_data.subspan(segment_size, srd[i].length);
```

Оставшиеся bytes после всех SRD segments сохраняются как:

```c++
res.trailing_padding
```

---

##### Non-staged overload

```c++
std::expected<VideoPacketView, Error>
parse_packet_view(ByteSpan udp_payload);
```

Это упрощенный overload.

Он вызывает `parse_packet_view_staged`, но возвращает только `Error`, без информации о failed stage.

---

##### Overload со статистикой

```c++
std::expected<VideoPacketView, Error>
parse_packet_view(
    ByteSpan udp_payload,
    PacketParseStats &stats,
    std::size_t maxudp
);
```

Этот overload дополнительно:

1. Проверяет packet policy через `validate_packet_parse_policy`.
2. Записывает результат parsing-а в `PacketParseStats`.
3. Возвращает `VideoPacketView` или `Error`.

---

#### `video_receive_pipeline.hpp`

Содержит компактный video receive pipeline, который связывает video depacketizer и video unit reconstructor в один processing stage.

---

##### `VideoReceivePipeline`

`VideoReceivePipeline` объединяет два этапа video receive обработки:

```text
Depacketizer
VideoUnitReconstructor
```

---

##### Основной pipeline

`push` принимает один уже распаршенный и прошедший reorder video packet.

Дальше выполняется два шага.

Сначала packet передается в depacketizer.

Затем каждый assembled unit передается в reconstructor.

Если reconstructor вернул frame, он добавляется в результат.

---

#### `depacketizer.hpp`

Содержит video depacketizer для Socket video receive pipeline.

Файл принимает уже распаршенные и упорядоченные `VideoPacketView` и собирает из их SRD segments промежуточные video units:

```text
VideoPacketView
→ SRD segments
→ FrameAssembler
→ AssembledVideoUnit[]
```

---

##### Основной pipeline

Главный метод:

```c++
std::vector<AssembledVideoUnit>
push(std::unique_ptr<VideoPacketView> packet);
```

Порядок обработки одного packet-а:

```text
1. Увеличить packets_in
2. Определить assembly key для packet-а
3. Если unit еще не начат:
   - начать новый unit
   - записать SRD segments в FrameAssembler
   - если RTP marker завершает unit — закрыть unit
4. Если текущий packet относится к другому unit:
   - закрыть предыдущий unit как завершенный без marker
   - начать новый unit
5. Записать SRD segments текущего packet-а
6. Если RTP marker завершает unit — закрыть unit
7. Вернуть все units, которые были завершены на этом push-е
```

---

##### Assembly key

Для группировки packets используется:

```c++
struct VideoAssemblyKey {
    VideoAssemblyUnitKind unit_kind;
    std::uint32_t rtp_timestamp;
    std::uint8_t sub_unit_index;
};
```

`VideoAssemblyKey` определяет, к какой video unit относится packet.

Правила зависят от `VideoScanMode`:

```text
Progressive:
    unit_kind = Frame
    rtp_timestamp = packet.rtp.timestamp
    sub_unit_index = 0

Interlaced:
    unit_kind = Field
    rtp_timestamp = packet.rtp.timestamp
    sub_unit_index = field_id ? 1 : 0

PsF:
    unit_kind = Segment
    rtp_timestamp = packet.rtp.timestamp
    sub_unit_index = field_id ? 1 : 0
```

То есть progressive video собирается как frame, interlaced video — как отдельные fields, PsF — как отдельные segments.

---

##### Начало и завершение unit-а

Когда начинается новый unit, depacketizer создает новый `FrameAssembler`:

```c++
FrameAssembler(
    width,
    unit_height,
    format,
    partial_unit_policy
)
```

Для progressive используется полная высота frame-а.

Для interlaced и PsF высота unit-а зависит от `sub_unit_index`:

```text
first field/segment  → (height + 1) / 2
second field/segment → height / 2
```

Завершение unit-а выполняется через:

```c++
assembler_.end(marker_seen, true);
```

Результат может быть:

```text
EmittedComplete
EmittedPartial
DroppedPartial
NotEmittable
```

Depacketizer обновляет `DepacketizerStats` и, если `FrameAssembler` вернул unit, добавляет его в output vector.

---

##### Запись SRD segments в frame/unit buffer

Для каждого segment-а packet-а вызывается:

```c++
map_segment_to_unit_local_write(cfg_.format, packet.segments[i])
→ VideoFrameWriteOp
→ assembler_.write_segment(...)
```

`VideoFrameWriteOp` описывает, куда записать bytes segment-а:

```c++
struct VideoFrameWriteOp {
    std::size_t plane;
    std::uint32_t row;
    std::size_t byte_offset;
    ByteSpan bytes;
};
```

Для packed/RFC4175 форматов depacketizer переводит ST 2110-20 SRD `row_number` и `offset` в:

```text
plane
row
byte_offset
bytes
```

Например:

```text
UYVY:
    byte_offset = offset * 2

RGB8:
    byte_offset = offset * 3

YUV422RFC4175PG2BE10:
    byte_offset = (offset / 2) * 5

YUV422RFC4175PG2BE12:
    byte_offset = (offset / 2) * 6

YUV444/RGB RFC4175 PG4 BE10:
    byte_offset = (offset / 4) * 15

YUV444/RGB RFC4175 PG2 BE12:
    byte_offset = (offset / 2) * 9
```

---

##### Ограничение по pixel formats

`VideoFrameWriteOp` и `map_segment_to_unit_local_write` сейчас работают только с `plane = 0`.

Поэтому depacketizer сейчас поддерживает только single-plane packed/RFC4175 formats:

```text
UYVY
RGB8
YUV422RFC4175PG2BE10
YUV422RFC4175PG2BE12
YUV444RFC4175PG4BE10
RGBRFC4175PG4BE10
YUV444RFC4175PG2BE12
RGBRFC4175PG2BE12
```

Multi-plane planar formats сейчас не поддержаны:

```text
YUV422PLANAR8
YUV422PLANAR10LE
YUV422PLANAR12LE
YUV422PLANAR16LE
YUV444PLANAR10LE
YUV444PLANAR12LE
YUV420PLANAR8
```

Для таких форматов нужно отдельно реализовать mapping:

```text
SRD row/offset
→ target plane
→ plane-local row
→ plane-local byte offset
```

Сейчас unsupported format приводит к:

```c++
throw std::runtime_error("Unsupported pixel format");
```

---

#### `video_frame.hpp`

Содержит owning container для video frame data и view-структуру для передачи frame-а дальше по pipeline.

---

##### `VideoFrameView`

```c++
struct VideoFrameView {
    PixelFormat format;
    std::uint32_t width;
    std::uint32_t height;
    const std::uint8_t *data[4];
    std::size_t stride[4];
    TimestampNs timestamp_ns;
};
```

`VideoFrameView` — non-owning view на frame data.

Он содержит:

- `format` — pixel format frame-а;
- `width` / `height` — размер frame-а;
- `data[4]` — pointers на plane data;
- `stride[4]` — stride каждого plane-а;
- `timestamp_ns` — media/output timestamp.

`VideoFrameView` не владеет памятью. Он только ссылается на данные, которые принадлежат `VideoFrame`.

---

##### `Plane`

```c++
struct Plane {
    std::size_t offset_bytes;
    std::size_t stride_bytes;
    std::size_t active_row_bytes;
    std::size_t height_rows;
};
```

`Plane` описывает layout одного plane-а внутри общего `frame_data` buffer-а:

- `offset_bytes` — offset plane-а внутри `frame_data`;
- `stride_bytes` — bytes между началами соседних строк;
- `active_row_bytes` — количество активных bytes в строке;
- `height_rows` — количество строк в plane-е.

---

##### `VideoFrame`

```c++
class VideoFrame;
```

`VideoFrame` владеет памятью video frame-а.

Внутри frame data хранится как один continuous buffer:

```c++
std::vector<std::uint8_t> frame_data;
```

А plane layout описывается отдельно:

```c++
Plane planes_[4];
std::uint8_t planes_count_;
```

Это позволяет одному и тому же container-у поддерживать как single-plane packed formats, так и multi-plane planar formats.

---

##### Создание frame-а

```c++
VideoFrame(std::uint32_t w, std::uint32_t h, PixelFormat fmt);
```

Конструктор:

1. сохраняет width/height/format;
2. вызывает `fill_planes()`;
3. вычисляет общий размер frame buffer-а;
4. выделяет `frame_data`.

Если width/height некорректны или format требует кратности width/height, но она не выполнена, constructor бросает `std::invalid_argument`.

---

##### Single-plane formats

Для single-plane formats используется:

```c++
configure_single_plane(active_row_bytes);
```

Поддержанные single-plane layouts:

```text
UYVY:
    width must be multiple of 2
    row bytes = width * 2

RGB8:
    row bytes = width * 3

BGRA / ARGB:
    row bytes = width * 4

Y210:
    width must be multiple of 2
    row bytes = width * 4

V210:
    width must be multiple of 6
    row bytes = (width / 6) * 16

YUV422RFC4175PG2BE10:
    width must be multiple of 2
    row bytes = (width / 2) * 5

YUV422RFC4175PG2BE12:
    width must be multiple of 2
    row bytes = (width / 2) * 6

YUV444RFC4175PG4BE10 / RGBRFC4175PG4BE10:
    width must be multiple of 4
    row bytes = (width / 4) * 15

YUV444RFC4175PG2BE12 / RGBRFC4175PG2BE12:
    width must be multiple of 2
    row bytes = (width / 2) * 9
```

---

##### Multi-plane planar formats

Для planar formats используется:

```c++
configure_three_planes(...);
```

Поддержанные multi-plane layouts:

```text
YUV422PLANAR8:
    width must be multiple of 2
    Y plane: width bytes per row, height rows
    U plane: width / 2 bytes per row, height rows
    V plane: width / 2 bytes per row, height rows

YUV422PLANAR10LE / YUV422PLANAR12LE / YUV422PLANAR16LE:
    width must be multiple of 2
    Y plane: width * 2 bytes per row, height rows
    U plane: (width / 2) * 2 bytes per row, height rows
    V plane: (width / 2) * 2 bytes per row, height rows

YUV444PLANAR10LE / YUV444PLANAR12LE:
    Y plane: width * 2 bytes per row, height rows
    U plane: width * 2 bytes per row, height rows
    V plane: width * 2 bytes per row, height rows

YUV420PLANAR8:
    width must be multiple of 2
    height must be multiple of 2
    Y plane: width bytes per row, height rows
    U plane: width / 2 bytes per row, height / 2 rows
    V plane: width / 2 bytes per row, height / 2 rows
```

---

##### Access methods

Файл предоставляет доступ к frame data:

```c++
std::uint8_t *data(std::size_t plane = 0);
const std::uint8_t *data(std::size_t plane = 0) const;

std::uint8_t *row_data(std::uint32_t row, std::size_t plane = 0);
const std::uint8_t *row_data(std::uint32_t row, std::size_t plane = 0) const;
```

`data(plane)` возвращает pointer на начало plane-а.

`row_data(row, plane)` возвращает pointer на начало конкретной строки внутри plane-а.

Если `plane` или `row` некорректны, методы бросают `std::out_of_range`.

Также доступны layout query methods:

```c++
std::size_t stride_bytes(std::size_t plane = 0) const;
std::size_t active_row_bytes(std::size_t plane = 0) const;
std::size_t plane_height_rows(std::size_t plane = 0) const;
std::size_t plane_count() const;
std::size_t size_bytes() const;
```

---

##### View creation

```c++
VideoFrameView view(TimestampNs timestamp_ns = 0) const;
```

`view` создает non-owning `VideoFrameView` поверх внутреннего `frame_data`.

Для каждого plane-а заполняются:

```text
data[plane]
stride[plane]
```

Поля для неиспользуемых plane-ов остаются `nullptr` / `0`.

---

#### `frame_write_coverage.hpp`

Содержит helper для отслеживания, какие bytes внутри `VideoFrame` уже были записаны frame assembler-ом.

Файл нужен для определения полноты собранного video unit-а.

---

##### `PlaneWriteCoverage`

```c++
struct PlaneWriteCoverage {
    std::size_t active_row_bytes;
    std::size_t height_rows;
    std::size_t expected_bytes;
    std::size_t written_unique_bytes;
    std::vector<std::uint8_t> written;
};
```

Хранит coverage для одного plane-а:

- сколько bytes ожидается в plane-е;
- сколько уникальных bytes уже записано;
- bitmap/vector `written`, где отмечается каждый записанный byte.

---

##### `FrameWriteCoverage`

```c++
class FrameWriteCoverage;
```

`FrameWriteCoverage` хранит coverage для всех plane-ов frame-а.

При инициализации:

```c++
reset_from(const VideoFrame &frame);
```

он читает layout из `VideoFrame`:

```text
plane_count
active_row_bytes
plane_height_rows
```

и создает coverage map для каждого plane-а.

---

##### Mark written

```c++
void mark_written(
    std::size_t plane,
    std::uint32_t row,
    std::size_t byte_offset,
    std::size_t length
);
```

Отмечает диапазон bytes как записанный.

Если один и тот же byte записан повторно, `written_unique_bytes` не увеличивается. Поэтому overlapping writes не делают frame “более полным”, чем он реально есть.

---

##### Completion check

```c++
bool is_complete() const;
```

Возвращает `true`, если количество уникально записанных bytes равно ожидаемому количеству active bytes всего frame/unit-а:

```text
total_written_unique_bytes == total_expected_bytes
```

---

#### `frame_assembler.hpp`

Содержит helper, который физически собирает один video unit в `VideoFrame`.

`FrameAssembler` получает уже готовые write operations:

```text
plane
row
byte_offset
bytes
```

и записывает bytes в `VideoFrame`.

---

##### End status

```c++
enum class FrameAssemblerEndStatus {
    NotEmittable,
    EmittedComplete,
    EmittedPartial,
    DroppedPartial
};
```

Описывает результат завершения текущего unit-а:

- `NotEmittable` — unit нельзя выдать;
- `EmittedComplete` — unit полностью собран и выдан;
- `EmittedPartial` — unit неполный, но выдан из-за `PartialUnitPolicy::EmitWithFlag`;
- `DroppedPartial` — unit неполный и отброшен из-за `PartialUnitPolicy::Drop`.

---

##### Assembled video unit

```c++
struct AssembledVideoUnit {
    VideoFrame frame;
    VideoAssemblyUnitKind unit_kind;
    std::uint32_t rtp_timestamp;
    TimestampNs receive_timestamp_ns;
    std::uint8_t sub_unit_index;
    bool marker_seen;
    bool can_emit;
    bool complete;

    bool partial() const;
};
```

`AssembledVideoUnit` — результат сборки одного video unit-а.

Он содержит:

- собранный `VideoFrame`;
- тип unit-а: frame / field / segment;
- RTP timestamp;
- timestamp приема;
- индекс sub-unit-а для interlaced/PsF;
- признак, был ли marker bit;
- признак, можно ли unit выдавать;
- признак полной сборки.

```c++
partial()
```

возвращает `true`, если unit можно выдать, но он собран не полностью.

---

##### Frame assembler

```c++
class FrameAssembler;
```

`FrameAssembler` владеет текущим `VideoFrame` и отслеживает полноту записи через `FrameWriteCoverage`.

---

##### Начало unit-а

```c++
void begin(
    std::uint32_t rtp_timestamp,
    TimestampNs receive_timestamp_ns
);
```

`begin` начинает сборку нового unit-а:

- сохраняет RTP timestamp;
- сохраняет receive timestamp;
- сбрасывает coverage относительно текущего `VideoFrame`.

---

##### Запись segment-а

```c++
void write_segment(
    std::size_t plane,
    std::uint32_t row,
    std::size_t byte_offset,
    ByteSpan bytes
);
```

`write_segment` копирует bytes в текущий frame:

```text
VideoFrame::row_data(row, plane) + byte_offset
```

После записи метод отмечает этот диапазон в `FrameWriteCoverage`:

```text
coverage_.mark_written(...)
```

Именно coverage потом определяет, полностью ли был собран frame/unit.

---

##### Завершение unit-а

```c++
FrameAssemblerEndResult end(
    bool marker,
    bool can_emit
);
```

`end` завершает текущую сборку.

Если `can_emit == false`, текущий frame сбрасывается, а результат:

```text
NotEmittable
```

Если unit можно выдать, assembler проверяет полноту:

```c++
coverage_.is_complete()
```

Дальше поведение зависит от результата и `PartialUnitPolicy`:

```text
fully_written == true
    → EmittedComplete

fully_written == false && PartialUnitPolicy::Drop
    → DroppedPartial

fully_written == false && PartialUnitPolicy::EmitWithFlag
    → EmittedPartial
```

После завершения текущий `VideoFrame` заменяется новым пустым frame-ом того же размера и формата.

---

##### Где используется

`FrameAssembler` используется внутри `Depacketizer`.

`Depacketizer` определяет, к какому video unit-у относится packet, переводит SRD segments в write operations, а затем вызывает:

```text
FrameAssembler::begin
FrameAssembler::write_segment
FrameAssembler::end
```

---

#### `video_unit_reconstructor.hpp`

Содержит stage, который преобразует `AssembledVideoUnit` в итоговый `ReconstructedVideoFrame`.

`VideoUnitReconstructor` работает уже с собранными video units.

---

##### `ReconstructedVideoFrame`

```c++
struct ReconstructedVideoFrame {
    VideoFrame frame;
    std::uint32_t rtp_timestamp;
    TimestampNs receive_timestamp_ns;
    bool complete;

    bool partial() const;
};
```

`ReconstructedVideoFrame` — итоговый video frame, который можно передавать дальше в sink/delivery layer.

`complete` показывает, был ли frame полностью собран.

```c++
partial()
```

возвращает `true`, если frame неполный.

---

##### Pending pair

```c++
struct PendingUnitPair {
    std::optional<AssembledVideoUnit> first;
    std::optional<AssembledVideoUnit> second;
};
```

`PendingUnitPair` хранит пару sub-units для режимов, где один output frame собирается из двух частей:

```text
Interlaced:
    first field + second field → full frame

PsF:
    first segment + second segment → full frame
```

---

##### `VideoUnitReconstructor`

```c++
class VideoUnitReconstructor;
```

Основной метод:

```c++
std::optional<ReconstructedVideoFrame>
push(AssembledVideoUnit unit);
```

`push` принимает один assembled unit и либо сразу возвращает готовый frame, либо сохраняет unit до прихода второй части.

---

##### Progressive path

Если unit имеет тип:

```c++
VideoAssemblyUnitKind::Frame
```

то реконструкция не нужна.

`AssembledVideoUnit` сразу превращается в `ReconstructedVideoFrame`:

```text
Frame unit
→ ReconstructedVideoFrame
```

Переносятся:

```text
VideoFrame
rtp_timestamp
receive_timestamp_ns
complete flag
```

---

##### Interlaced path

Если unit имеет тип:

```c++
VideoAssemblyUnitKind::Field
```

то reconstructor ждет две field-части:

```text
sub_unit_index = 0 → first field
sub_unit_index = 1 → second field
```

Когда обе части есть, создается новый `VideoFrame` полной высоты:

```c++
VideoFrame frame(cfg_.width, cfg_.height, cfg_.format);
```

Затем строки копируются через чередование:

```text
first field:
    src row 0 → dst row 0
    src row 1 → dst row 2
    src row 2 → dst row 4

second field:
    src row 0 → dst row 1
    src row 1 → dst row 3
    src row 2 → dst row 5
```

После сборки возвращается один `ReconstructedVideoFrame`.

---

##### PsF path

Если unit имеет тип:

```c++
VideoAssemblyUnitKind::Segment
```

то reconstructor также ждет две части:

```text
sub_unit_index = 0 → first segment
sub_unit_index = 1 → second segment
```

Для PsF дополнительно проверяется RTP timestamp: если уже сохраненная противоположная часть относится к другому timestamp, она сбрасывается.

То есть reconstructor не объединяет segments от разных frames.

Когда обе части с совместимым timestamp есть, они копируются в итоговый frame тем же способом, что и fields:

```text
first segment  → even rows
second segment → odd rows
```

---

##### Роль файла в pipeline-е

Его задача — преобразовать результат depacketizer-а в единый `ReconstructedVideoFrame`, который затем Socket video backend передает в `IFrameSink`.