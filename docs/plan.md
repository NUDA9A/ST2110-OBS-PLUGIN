# ST2110-OBS-PLUGIN — Plan

## Правила работы
- 1 задача за раз (маленькая, 15–60 мин).
- Реализацию пишу я сам; ассистент должен:
  - формулировать следующую задачу;
  - подробно описывать, что именно нужно сделать;
  - сразу писать/предлагать тест(ы) под задачу;
  - проверять результат после моей реализации.
- После завершения задачи я присылаю код; ассистент проверяет:
  - соответствие плану;
  - отсутствие ненужных упрощений;
  - отсутствие расхождений со стандартом;
  - расширяемость архитектуры;
  - качество тестового покрытия под задачу.
- При формулировании задачи и при приемке ассистент обязан сверять не только текущую функциональность, но и соответствие целевой архитектуре из плана:
  - `PixelFormat` и `VideoScanMode` рассматриваются как независимые оси модели;
  - progressive-only поведение в MVP допустимо только как локализованное ограничение;
  - новые изменения не должны закреплять assumptions вида `marker == end of frame`, `timestamp group == frame`, `F is always zero` вне специально выделенных policy / mode-aware точек.
- Пока задача не принята, к следующей не переходим.
- Сложные детали реализации можно разбирать в отдельном чате, но приемка задачи — только по коду и тестам.
- `plan.md` является не только списком задач, но и краткой картой текущей архитектуры/кода.
- В `plan.md` должен поддерживаться раздел с описанием актуальных файлов реализации (`file map` / `code map`).
- Для каждого значимого файла реализации (прежде всего public headers) в этом разделе должны быть указаны:
  - архитектурная роль файла;
  - основные зависимости / связи с другими файлами;
  - перечисление ключевых enum / struct / class / function / method с кратким назначением.
- Описание должно быть достаточно подробным, чтобы ассистент мог восстановить архитектурный контекст без повторной пересылки всего кода, но без копирования полной реализации в `plan.md`.
- Перед началом очередной задачи ассистент может запросить у пользователя только те файлы (обычно header’ы и при необходимости связанные `.cpp` / тесты), которые относятся к текущей задаче или приемке.
- Если для формулирования задачи или приемки достаточно информации из `plan.md` и актуального file map, ассистент не должен запрашивать лишние файлы.
- После завершения каждой принятой задачи нужно:
  - обновить `plan.md`;
  - добавить описания для новых файлов;
  - обновить описания существующих файлов, если изменились их публичные структуры, классы, методы, связи или архитектурная роль.
- Если фактический код начинает расходиться с описанием файла в `plan.md`, приоритет у фактического кода; такое расхождение должно быть устранено сразу после приемки текущей задачи обновлением file map.
- Когда задача требует новые тесты или обновление существующих тестов, ассистент обязан присылать:
  - точную новую строку `add_st2110_test(...)` для `tests/CMakeLists.txt`, если нужен новый test target;
  - полный готовый `.cpp` файл каждого нового теста;
  - полный готовый `.cpp` файл для каждого теста, который нужно заменить целиком.
- Ассистент не должен ограничиваться описанием тестовой идеи; тестовые файлы должны быть даны в копируемом виде.

## Правила проектирования
- Код должен быть написан в **расширяемом виде**.
- При добавлении новых форматов / режимов / backend’ов / типов потоков в типичных местах должно быть достаточно:
  - добавить новый enum/value;
  - дописать `case`/adapter/mapper;
  - добавить тесты;
  - а не переписывать существующую реализацию целиком.
- Нельзя жестко зашивать архитектуру только под:
  - один pixel format;
  - только video без возможности добавить audio;
  - только socket backend;
  - только консольный pipeline без дальнейшей OBS-интеграции.
- Нельзя жестко зашивать video pipeline только под progressive semantics.
- Архитектурно должны быть разделены:
  - pixel/storage format;
  - scan mode (`Progressive | Interlaced | PsF`);
  - mode-dependent completion semantics (marker/timestamp/end-of-unit logic).
- Даже если MVP реализует только `Progressive`, код и API должны позволять добавить `Interlaced` и `PsF` через локальное расширение policy/state/config, а не через переписывание depacketizer/assembly pipeline.
- **MVP может быть ограничен только в реализации, но не в архитектуре.**
  - если стандартная ось уже известна в рамках ST 2110 family, она должна быть архитектурно представлена уже в MVP;
  - для такой оси в MVP должны существовать:
    - explicit modeled representation;
    - explicit config/runtime axis или projection boundary;
    - explicit dispatch / boundary / adapter / mapper / policy point;
    - локализованные future implementation branches;
  - допустимо, что часть веток в MVP пока возвращает `Unsupported`, `InvalidValue` через reject-by-policy или работает как placeholder, но только если сама ось и extension points уже существуют явно;
  - поздние фазы (`Medium` / `Hardening`) должны по возможности сводиться к “заполнить уже существующие ветки реализацией”, а не к “впервые протащить эту ось через архитектуру”.
- Это правило в частности относится к уже известным стандартным осям и вариантам:
  - `Progressive | Interlaced | PsF`;
  - `GPM | BPM`;
  - signaling / clock / timestamp / timing-related variants;
  - standards-aware video media-property representation;
  - standards-aware audio signaling / channel-order / channel-mapping representation;
  - receiver timing / playout timing / capability boundaries.
- Реализационные задачи для уже архитектурно заложенных веток **могут оставаться в `Medium`**; это не нарушает MVP-принцип, если сама ось, boundary и dispatch уже заведены в MVP.
- Ассистент обязан отдельно проверять архитектуру на расширяемость при приемке каждой задачи.
- Все временные упрощения должны быть:
  - явно зафиксированы в плане;
  - локализованы;
  - сопровождаться отдельной задачей на устранение.
- **MVP не должен сознательно накапливать расхождения со стандартом**, если их можно избежать без взрывного роста сложности.
- Если найдено расхождение со стандартом, оно должно быть сразу занесено в:
  - раздел `Spec notes / deviations`;
  - отдельную задачу на исправление или доведение до полного соответствия.

## Цели этапов
- **MVP**: минимально полноценный прием ST 2110 для видео и аудио на Linux, с двумя backend’ами, базовой OBS-интеграцией и готовностью к ручному end-to-end тестированию.
- **Medium**: расширение форматов, улучшение устойчивости, более полная обработка edge-cases, улучшение UX/наблюдаемости, подготовка к полноценному тестированию.
- **Plugin**: доведение OBS-плагина до удобного и стабильного состояния для повседневного использования.
- **Tests**: систематическое полноценное тестирование, фиксация проблемных мест, regression-набор.
- **Hardening**: исправление узких мест, performance, recovery policy, дополнительная корректность.
- **Windows**: опциональный перенос только собственного socket backend, без MTL.

## Референсы (куда смотреть)
- SMPTE ST 2110-10:2022 (system timing, definitions, common RTP/SDP/clock signaling requirements).
- SMPTE ST 2110-20:2022 (uncompressed video).
- SMPTE ST 2110-21:2022 (traffic shaping and delivery timing for video).
- SMPTE ST 2110-30:2025 (PCM digital audio).
- SMPTE RP 2110-25:2023 (measurement practices).
- RTP: RFC 3550 (структура заголовка, seq/timestamp/marker).
- Video over RTP: RFC 4175 (концепция packetization по строкам/фрагментам).
- Wireshark dissector ST2110-20 (для сверки полей SRD/ExtSeq/marker).
- Intel MTL (Media Transport Library) docs + st_pipeline_api (для MTL backend).
---

## Текущая структура кода (file map)

> Назначение раздела:
> - держать краткую карту текущей реализации;
> - уменьшать необходимость пересылать весь код в чат;
> - помогать ассистенту формулировать задачи и делать приемку в контексте реальной архитектуры.
>
> Правила ведения:
> - описываем прежде всего public headers и значимые entry points;
> - фиксируем архитектурную роль, связи и публичные сущности;
> - не дублируем полную реализацию;
> - после принятия задачи обновляем этот раздел вместе с кодом.

### apps/st2110_rx_dump/main.cpp
- Роль:
  - минимальный CLI entry point для будущего dump/tool-приложения.
  - пока выполняет роль buildable stub.
- Связи:
  - собирается в `st2110_rx_dump`;
  - линкуется со `st2110core`, но пока фактически core API не использует.
- Сущности:
  - `main()` — временная заглушка, печатает имя приложения.

### libs/st2110core/include/st2110/backend.hpp
- Роль:
  - базовые backend/sink интерфейсы для video receive path.
  - текущая точка расширения для socket/MTL video backend’ов.
- Связи:
  - использует `VideoFrameView` и `RxVideoConfig`;
  - должна остаться совместимой с будущим audio/generalized backend layer.
- Сущности:
  - `IVideoFrameSink::on_video_frame(const VideoFrameView&)` — прием готового video frame/view.
  - `IRxBackend` — общий интерфейс backend’а (`backend_name()`, `stop()`).
  - `IRxVideoBackend` — video-специализация backend’а, `start(const RxVideoConfig&, IVideoFrameSink&)`.

### libs/st2110core/include/st2110/bytes.hpp
- Роль:
  - общая базовая alias-конвенция для неизменяемых байтовых диапазонов.
- Связи:
  - используется в RTP/ST2110 parsing, assembler/depacketizer и других low-level helper’ах.
- Сущности:
  - `ByteSpan = std::span<const uint8_t>`.

### libs/st2110core/include/st2110/config_validation.hpp
- Роль:
  - общие helper’ы для явной валидации конфигов и значений.
  - отражает правило “strict parse, explicit fallback”.
- Связи:
  - используется `RxVideoConfig` и будущими signaling/config model.
- Сущности:
  - `is_non_empty(std::string_view)`
  - `is_dynamic_rtp_payload_type(uint8_t)`
  - `validate_frame_rate(uint32_t num, uint32_t den)`
  - `validate_udp_port(uint16_t)`
  - `validate_video_dimensions(uint32_t width, uint32_t height)`
  - `validate_video_format_constraints(PixelFormat, width, height)` — формат-специфичная валидация размеров.
  - `validate_video_scan_mode(VideoScanMode)`

### libs/st2110core/include/st2110/depacketizer.hpp
- Роль:
  - packet-to-video-unit assembly layer.
  - собирает `PacketView` в `AssembledVideoUnit`, используя mode-aware grouping/completion, format-aware placement и mode-aware packet padding validation.
- Связи:
  - зависит от `PacketView`, `FrameAssembler`, `VideoReceiveSemantics`, `VideoSegmentPlacement`, `VideoPacketPadding`, `Stats`.
  - выше него находится `VideoReceivePipeline`, ниже — packet parsing.
- Сущности:
  - `DepacketizerConfig`
    - `width`, `height`, `format`
    - `partial_frame_policy`
    - `scan_mode`
  - `DepacketizerAssemblyState`
    - `current_key` — текущий `VideoAssemblyKey` в сборке.
  - `Depacketizer`
    - `push(const PacketView&) -> std::vector<AssembledVideoUnit>` — основной API depacketizer’а.
    - `reset()`
    - `stats()`
    - `scan_mode()`
    - `has_unit_in_progress()`
    - `current_unit_rtp_timestamp()`
    - `assembly_unit_kind()`
    - `current_unit_key()`
  - Внутренние responsibilities:
    - берет completion policy через `video_receive_completion_policy(...)`;
    - берет assembly key через `video_packet_assembly_key(...)`;
    - валидирует trailing payload padding через `validate_video_packet_trailing_padding(...)` до изменения assembly state;
    - пишет сегменты через `map_video_segment_to_frame_write(...)`;
    - пока runtime-реализация только для `Progressive`, non-progressive локализованно отвергается.
- Примечание:
  - packing mode как runtime axis должен быть доведен сюда уже на уровне MVP architecture; если часть packing branches еще не реализована, они должны быть локализованно ограничены, а не отсутствовать в shape/config path.

### libs/st2110core/include/st2110/endian.hpp
- Роль:
  - простые big-endian helpers для чтения multibyte полей из wire data.
- Связи:
  - используются RTP/ST2110 payload parser’ами.
- Сущности:
  - `read_be16(std::span<const uint8_t>)`
  - `read_be32(std::span<const uint8_t>)`

### libs/st2110core/include/st2110/error.hpp
- Роль:
  - общий enum ошибок проекта на текущем этапе.
- Связи:
  - используется в parse/validation/reconstructor/signaling layers.
- Сущности:
  - `enum class Error`
    - `Ok`
    - `BufferTooSmall`
    - `InvalidValue`
    - `Unsupported`
    - `ShortPacket`
    - `BadRTPVersion`
  - `to_string(Error)` — строковое представление ошибки.

### libs/st2110core/include/st2110/fixed_reorder_buffer.hpp
- Роль:
  - MVP-реализация reorder buffer’а с фиксированным окном по `extended_seq`.
- Связи:
  - реализует `IReorderBuffer`;
  - использует `StoredPacket`/`PacketView`.
- Сущности:
  - `FixedWindowReorderBuffer`
    - `push(const PacketView&)`
    - `pop_next() -> std::optional<StoredPacket>`
    - `reset()`
    - `stats()`
    - `flush_missing_once()`
  - Поведение:
    - хранит пакеты в `std::map<uint32_t, StoredPacket>`;
    - учитывает duplicates / out_of_window / late / missing_seq / missing_seq_flushed.

### libs/st2110core/include/st2110/frame_assembler.hpp
- Роль:
  - byte-oriented сборка одного текущего assembled video unit в owning `VideoFrame`.
  - не содержит format-aware packet semantics; получает уже рассчитанные row/byte offsets.
- Связи:
  - использует `VideoFrame`, `FrameWriteCoverage`, `VideoReceiveSemantics`.
  - вызывается из `Depacketizer`.
- Сущности:
  - `PartialFramePolicy`
    - `EmitWithFlag`
    - `Drop`
  - `FrameAssemblerEndStatus`
    - `NotEmittable`
    - `EmittedComplete`
    - `EmittedPartial`
    - `DroppedPartial`
  - `AssembledVideoUnit`
    - `frame`
    - `unit_kind`
    - `rtp_timestamp`
    - `marker_seen`
    - `can_emit`
    - `complete`
    - `partial()`
  - `FrameAssemblerEndResult`
    - `unit`
    - `status`
  - `FrameAssembler`
    - `begin(uint32_t rtp_timestamp)`
    - `write_segment(std::size_t plane, uint32_t row, std::size_t byte_offset, ByteSpan bytes)`
    - `end(bool marker) -> FrameAssemblerEndResult`
    - `in_progress()`
    - `current_rtp_timestamp()`
    - `partial_frame_policy()`

### libs/st2110core/include/st2110/frame_write_coverage.hpp
- Роль:
  - helper для отслеживания, какие байты кадра уже были записаны, и определения completeness.
- Связи:
  - используется `FrameAssembler`.
- Сущности:
  - `PlaneWriteCoverage`
    - `active_row_bytes`
    - `height_rows`
    - `expected_bytes`
    - `written_unique_bytes`
    - `written`
  - `FrameWriteCoverage`
    - `reset_from(const VideoFrame&)`
    - `mark_written(plane, row, byte_offset, length)`
    - `is_complete()`
    - `total_expected_bytes()`
    - `total_written_unique_bytes()`
    - `plane_count()`
    - `plane_expected_bytes(plane)`
    - `plane_written_unique_bytes(plane)`

### libs/st2110core/include/st2110/packet_parse.hpp
- Роль:
  - интегрированный entry point для packet parsing с optional packet-size policy и stats recording.
- Связи:
  - опирается на `PacketView::from_udp_datagram()` / `parse_packet_view_staged()`;
  - использует `PacketParseStats`.
- Сущности:
  - константы:
    - `udpHeaderBytes`
    - `standardUdpDatagramSizeLimitBytes`
    - `extendedUdpDatagramSizeLimitBytes`
    - `minRtpHeaderBytes`
    - `minParsableUdpDatagramBytes`
  - `PacketParsePolicy`
    - `max_udp_datagram_bytes`
  - `udp_datagram_size_bytes(ByteSpan)`
  - `effective_max_udp_datagram_bytes(const PacketParsePolicy&)`
  - `validate_packet_parse_policy_config(const PacketParsePolicy&)`
  - `validate_packet_parse_policy(ByteSpan, const PacketParsePolicy&)`
  - `parse_packet_view(ByteSpan, const PacketParsePolicy& = {})`
  - `parse_packet_view(ByteSpan, PacketParseStats&, const PacketParsePolicy& = {})`

### libs/st2110core/include/st2110/packet_view.hpp
- Роль:
  - нормализованное представление уже распарсенного video RTP/ST2110-20 packet.
- Связи:
  - объединяет `RtpHeaderView`, extended seq, SRD segment headers, segment payload spans и trailing payload bytes.
  - основной вход для reorder/depacketizer pipeline.
- Сущности:
  - `maxPacketSrdSegments = 3`
  - `SrdSegmentView`
    - `header`
    - `data`
  - `PacketViewParseFailure`
    - `error`
    - `stage`
  - `PacketView`
    - `rtp`
    - `extended_seq`
    - `segments[3]`
    - `segment_count`
    - `payload_data` — весь RTP payload после ST 2110-20 payload header
    - `trailing_padding` — trailing bytes после байтов, покрытых суммой `SRD Length`
    - `static from_udp_datagram(ByteSpan)`
  - `parse_packet_view_staged(ByteSpan)`
    - поэтапно парсит RTP header, ST 2110-20 payload header, split’ит payload по SRD segments и отдельно выделяет trailing padding.
- Примечание:
  - generic parse слой только выделяет trailing bytes, но не принимает mode-aware решение об их допустимости.
  - tolerance to RTP header extensions должна оставаться локализованной в RTP/payload extraction path, а не размазываться по `PacketView` consumers.

### libs/st2110core/include/st2110/pixel_format.hpp
- Роль:
  - enum текущих поддерживаемых pixel/storage format’ов video pipeline.
- Связи:
  - используется в frame storage, config, depacketizer, segment constraints/placement, reconstructor.
- Сущности:
  - `enum class PixelFormat`
    - `UYVY`

### libs/st2110core/include/st2110/reorder_buffer.hpp
- Роль:
  - абстракция reorder layer и owning stored-packet representation.
- Связи:
  - используется `FixedWindowReorderBuffer`;
  - отделяет packet ownership от `PacketView` с non-owning spans.
- Сущности:
  - `ReorderBufferStats`
  - `StoredPacket`
    - owning-копия packet content;
    - хранит полный packet payload;
    - `view() -> PacketView` — восстанавливает non-owning `PacketView` поверх внутреннего буфера, включая `segments[i].data` и `trailing_padding`.
  - `IReorderBuffer`
    - `push(const PacketView&)`
    - `pop_next()`
    - `reset()`

### libs/st2110core/include/st2110/rtp.hpp
- Роль:
  - RTP header parsing и seq helper’ы.
- Связи:
  - используется `PacketView` parsing.
- Сущности:
  - `RtpHeaderView`
    - `version`
    - `padding_flag`
    - `extension_flag`
    - `csrc_count`
    - `marker`
    - `payload_type`
    - `seq_number`
    - `timestamp`
    - `ssrc`
    - `payload_offset`
    - `payload_len`
  - `parse_rtp_header(ByteSpan)`
  - `seq_less(uint16_t a, uint16_t b)`
  - `seq_distance(uint16_t a, uint16_t b)`
  - `rtp_payload_span(ByteSpan, const RtpHeaderView&)`
- Примечание:
  - explicit receiver-side tolerance к RTP header extensions должна жить именно здесь / в связанном payload-span path;
  - parser path должен быть архитектурно готов к корректному skipping extensions уже в MVP, даже если semantic interpretation extension data не требуется.

### libs/st2110core/include/st2110/rx_config.hpp
- Роль:
  - manual video RX config model для текущего MVP-path.
- Связи:
  - используется backend/video pipeline слоями;
  - в будущем должен сосуществовать со standards-aware signaling model, а не заменять его.
- Сущности:
  - `RxVideoConfig`
    - `width`, `height`
    - `fps_num`, `fps_den`
    - `udp_port`
    - `payload_type`
    - `local_ip`, `dest_ip`
    - `format`
    - `scan_mode`
    - `is_valid()`
  - `validate_rx_video_config(const RxVideoConfig&)`

### libs/st2110core/include/st2110/st2110_20.hpp
- Роль:
  - low-level parsing/validation helpers для ST 2110-20 payload header.
- Связи:
  - используется `PacketView` parser’ом и format-aware validation слоями.
- Сущности:
  - `ExtendedSequenceNumber`
  - `SrdHeader`
    - `length`
    - `row_number`
    - `offset`
    - `field_id`
    - `continuation`
  - `St2110PayloadHeaderView`
    - `ext_seq`
    - `srd[3]`
    - `srd_count`
    - `header_bytes`
  - `parse_st2110_20_payload_header(ByteSpan)`
  - `validate_st2110_20_srd_ordering(const St2110PayloadHeaderView&)`
  - `validate_st2110_20_payload_header(const St2110PayloadHeaderView&)`
  - `combine_extended_seq(const ExtendedSequenceNumber&, uint16_t lo16)`

### libs/st2110core/include/st2110/stats.hpp
- Роль:
  - общие stats/counters для parser/depacketizer/backend слоев.
- Связи:
  - используется packet parsing, depacketizer и будущими backend’ами.
- Сущности:
  - `PacketParseStage`
  - `ParserStats`
  - `PacketParseStats`
  - `DepacketizerStats`
  - `BackendStats`
  - `record_parse_result(ParserStats&, Error)`
  - `record_packet_parse_result(PacketParseStats&, Error, PacketParseStage)`

### libs/st2110core/include/st2110/timestamp.hpp
- Роль:
  - базовый внутренний тип времени для media timestamps.
- Связи:
  - используется `VideoFrameView`;
  - future timestamp mapping boundary должна переводить RTP-domain в этот тип.
- Сущности:
  - `using TimestampNs = std::uint64_t`

### libs/st2110core/include/st2110/video_frame.hpp
- Роль:
  - owning storage object для assembled video frame/unit и соответствующий non-owning view.
- Связи:
  - используется assembler/reconstructor/backend sink path.
- Сущности:
  - `VideoFrameView`
    - `format`
    - `width`, `height`
    - `data[4]`
    - `stride[4]`
    - `timestamp_ns`
  - `Plane`
    - `offset_bytes`
    - `stride_bytes`
    - `active_row_bytes`
    - `height_rows`
  - `VideoFrame`
    - ctor `(width, height, format)`
    - `view(TimestampNs = 0)`
    - `size_bytes()`
    - `width()`, `height()`, `format()`
    - `stride_bytes(plane)`
    - `data(plane)`
    - `row_data(row, plane)`
    - `plane_count()`
    - `active_row_bytes(plane)`
    - `plane_height_rows(plane)`
  - Текущий MVP format:
    - `UYVY`, single-plane, `active_row_bytes = width * 2`.

### libs/st2110core/include/st2110/video_receive_pipeline.hpp
- Роль:
  - composition layer: depacketizer + video unit reconstructor.
  - текущий public receive pipeline для video.
- Связи:
  - использует `Depacketizer` и `IVideoUnitReconstructor`.
- Сущности:
  - `VideoReceivePipelineConfig`
    - `depacketizer`
    - `reconstructor`
  - `VideoReceivePipeline`
    - ctor `(const VideoReceivePipelineConfig&)`
    - `push(const PacketView&) -> std::vector<ReconstructedVideoFrame>`
    - `reset()`
- Примечание:
  - signaling-driven bootstrap и receiver-timing interaction должны быть доведены сюда как explicit boundaries уже на уровне MVP architecture;
  - более поздние фазы должны в основном заполнять behavior в существующих adapters/policies, а не менять shape pipeline.

### libs/st2110core/include/st2110/video_receive_semantics.hpp
- Роль:
  - mode-aware abstraction для unit kind, completion policy и packet grouping key.
  - ключевая точка локализации различий между `Progressive | Interlaced | PsF`.
- Связи:
  - используется `Depacketizer`, `FrameAssembler`, future non-progressive support.
- Сущности:
  - `VideoAssemblyUnitKind`
    - `Frame`
    - `Field`
    - `Segment`
  - `VideoReceiveCompletionPolicy`
    - `unit_kind`
    - `marker_terminates_current_unit`
    - `key_change_terminates_previous_unit`
  - `VideoAssemblyKey`
    - `unit_kind`
    - `rtp_timestamp`
    - `sub_unit_index`
    - `operator==`
  - `video_assembly_unit_kind(VideoScanMode)`
  - `video_receive_completion_policy(VideoScanMode)`
  - `video_packet_assembly_key(VideoScanMode, const PacketView&)`
  - `same_video_assembly_key(const VideoAssemblyKey&, const VideoAssemblyKey&)`
  - Текущий runtime status:
    - `Progressive` реализован;
    - `Interlaced` / `PsF` пока локализованно `Unsupported`.
- Примечание:
  - эта ось уже должна считаться архитектурно заложенной в MVP; поздние задачи по `Interlaced` / `PsF` должны только заполнять уже существующие extension points.

### libs/st2110core/include/st2110/video_scan_mode.hpp
- Роль:
  - enum transport/assembly scan mode, независимый от `PixelFormat`.
- Связи:
  - используется в config, semantics, depacketizer, placement, reconstructor и future signaling.
- Сущности:
  - `enum class VideoScanMode`
    - `Progressive`
    - `Interlaced`
    - `PsF`

### libs/st2110core/include/st2110/video_segment_constraints.hpp
- Роль:
  - формат-специфичная валидация packet segment constraints.
- Связи:
  - используется segment placement layer;
  - отделяет generic ST2110-20 parsing от format-specific receive constraints.
- Сущности:
  - `VideoSegmentConstraints`
    - `pgroup_bytes`
    - `offset_alignment_samples`
  - `video_segment_constraints(PixelFormat)`
  - `validate_video_segment_for_format(PixelFormat, const SrdHeader&, ByteSpan)`
  - Текущий MVP format:
    - `UYVY`: `pgroup_bytes = 4`, `offset_alignment_samples = 2`.

### libs/st2110core/include/st2110/video_segment_placement.hpp
- Роль:
  - mode-aware + format-aware mapping от semantics packet segment’а к byte-oriented frame write operation.
- Связи:
  - используется `Depacketizer`;
  - держит локализованную связь между `SrdHeader` semantics и `FrameAssembler::write_segment(...)`.
- Сущности:
  - `VideoFrameWriteOp`
    - `plane`
    - `row`
    - `byte_offset`
    - `bytes`
  - `map_progressive_segment_to_frame_write(PixelFormat, const SrdSegmentView&)`
  - `map_interlaced_segment_to_frame_write(...)`
  - `map_psf_segment_to_frame_write(...)`
  - `map_video_segment_to_frame_write(PixelFormat, VideoScanMode, const SrdSegmentView&)`
  - Текущий runtime status:
    - реализован `Progressive + UYVY`;
    - `Interlaced` / `PsF` пока локализованно `Unsupported`.

### libs/st2110core/include/st2110/video_signaling.hpp
- Роль:
  - standards-aware signaling/model boundary для video stream description.
  - задает типизированную модель ключевых video SDP/signaling свойств отдельно от low-level receive/depacketizer config.
- Связи:
  - использует `PixelFormat`, `VideoScanMode`, `PacketParsePolicy`, `RxVideoConfig`, общие config validation helper’ы.
  - связывает signaling model с packet parse policy и manual `RxVideoConfig` path.
  - должна дальше расширяться в рамках `069B`, а затем состыковаться с receiver timing boundary из `069C`.
- Сущности:
  - `VideoPackingMode`
    - `Gpm`
    - `Bpm`
  - `MediaClockMode`
    - `Direct`
    - `Sender`
  - `TimestampMode`
    - `Samp`
    - `New`
    - `Pres`
  - `ReferenceClockKind`
    - `LocalMac`
    - `Ptp`
    - `Other`
  - `PtpReferenceClock`
    - `clock_identity`
    - `domain_number`
    - `traceable`
  - `LocalMacReferenceClock`
    - `mac`
  - `ReferenceClock`
    - `kind`
    - `ptp`
    - `local_mac`
    - `raw_token`
  - `VideoSenderType`
    - `Narrow`
    - `NarrowLinear`
    - `Wide`
  - `VideoStreamSignaling`
    - `format`
    - `scan_mode`
    - `width`, `height`
    - `fps_num`, `fps_den`
    - `packing_mode`
    - `max_udp_datagram_bytes`
    - `media_clock_mode`
    - `timestamp_mode`
    - `reference_clock`
    - `ts_delay_sender_ticks`
    - `sender_type`
    - `troff_us`
    - `cmax`
  - `validate_video_sender_signaling(VideoSenderType, const std::optional<uint32_t>&, const std::optional<uint32_t>&)`
    - structural validation of ST 2110-21 sender timing fields:
      - `Narrow` => `troff_us` absent, `cmax` absent
      - `NarrowLinear` => `troff_us` absent, `cmax` absent
      - `Wide` => `troff_us` absent, `cmax` present and non-zero
  - `validate_reference_clock(const ReferenceClock&)`
    - structural validation of `ReferenceClock` consistency:
      - `Ptp` => only `ptp`
      - `LocalMac` => only `local_mac`
      - `Other` => only non-empty `raw_token`
  - `validate_media_clock_mode(MediaClockMode)`
    - validates known `mediaclk` modeling enum values.
  - `validate_timestamp_mode(TimestampMode)`
    - validates known `TSMODE` modeling enum values.
  - `validate_video_timing_signaling(MediaClockMode, TimestampMode, uint32_t)`
    - localized timing-signaling validation boundary for:
      - `mediaclk`
      - `TSMODE`
      - future `TSDELAY` semantics
    - current MVP behavior:
      - validates enum values explicitly
      - carries `ts_delay_sender_ticks` through the boundary
      - does not yet impose detailed `TSDELAY` semantics
  - `validate_video_stream_signaling(const VideoStreamSignaling&)`
    - базовая structural/config validation signaling model, включая:
      - timing signaling
      - sender timing fields
      - `ReferenceClock`
  - `packet_parse_policy_from_video_stream_signaling(const VideoStreamSignaling&)`
    - выводит `PacketParsePolicy` из signaling model.
  - `validate_video_stream_signaling_against_rx_video_config(const VideoStreamSignaling&, const RxVideoConfig&)`
    - проверяет согласованность signaling model и manual video config path по ключевым video properties.
  - `rx_video_config_from_video_stream_signaling(const VideoStreamSignaling&, uint16_t, uint8_t, std::string, std::string)`
    - explicit adapter from signaling model to runtime/manual `RxVideoConfig`
    - maps video stream properties from signaling and injects transport/network fields separately
    - validates signaling first, then validates the projected runtime config
- Примечание:
  - signaling model уже должна рассматриваться как архитектурная ось MVP, а не как later refactor;
  - modeled signaled video properties должны оставаться отличимыми от internal runtime/storage concepts;
  - future expansion of this file under `069B` must add explicit modeled representation for signaled SDP/media properties such as `sampling`, `width`, `height`, `exactframerate`, `depth`, `colorimetry`, `TCS`, `PM`, `SSN`, with `RANGE` allowed as optional / future-expansion coverage, instead of collapsing them prematurely into internal `PixelFormat` / storage-only notions;
  - packing mode, timing-related signaling и future receiver bootstrap должны быть доведены через explicit adapters/boundaries уже в MVP shape even if some branches remain runtime-limited.

### libs/st2110core/include/st2110/video_unit_reconstructor.hpp
- Роль:
  - слой реконструкции final output frame из generic assembled video units.
  - отделяет depacketizer unit semantics от final frame reconstruction policy.
- Связи:
  - используется `VideoReceivePipeline`;
  - в будущем здесь должны жить field/segment pairing policies для interlaced/PsF.
- Сущности:
  - `ReconstructedVideoFrame`
    - `frame`
    - `rtp_timestamp`
    - `complete`
    - `partial()`
  - `VideoUnitReconstructorConfig`
    - `format`
    - `scan_mode`
  - `IVideoUnitReconstructor`
    - `push(AssembledVideoUnit)`
    - `reset()`
  - `ProgressiveVideoUnitReconstructor`
    - MVP-реализация для assembled frame -> reconstructed frame.
  - `make_video_unit_reconstructor(const VideoUnitReconstructorConfig&)`
  - Текущий runtime status:
    - `Progressive` реализован;
    - `Interlaced` / `PsF` пока локализованно `Unsupported`.

### libs/st2110core/src/stub.cpp
- Роль:
  - временная `.cpp` единица для сборки статической библиотеки `st2110core`.
- Связи:
  - архитектурного значения не имеет;
  - может быть удалена/заменена по мере появления реальных `.cpp` файлов.
- Сущности:
  - `stub()`

### libs/st2110core/include/st2110/video_packet_padding.hpp
- Роль:
  - локализованная mode-aware boundary для валидации trailing payload padding после Sample Row Data segments.
- Связи:
  - использует `PacketView`, `VideoScanMode`, `Error`;
  - вызывается из `Depacketizer`;
  - отделяет generic packet parsing от mode-aware решения о допустимости trailing bytes.
- Сущности:
  - `validate_progressive_video_packet_trailing_padding(const PacketView&)`
    - для MVP progressive path:
      - `trailing_padding.empty()` => `Ok`
      - trailing padding в non-marker packet => `InvalidValue`
      - non-zero trailing padding bytes => `InvalidValue`
      - zero trailing padding in marker packet => `Ok`
  - `validate_interlaced_video_packet_trailing_padding(const PacketView&)`
    - пока `Unsupported`
  - `validate_psf_video_packet_trailing_padding(const PacketView&)`
    - пока `Unsupported`
  - `validate_video_packet_trailing_padding(VideoScanMode, const PacketView&)`
    - scan-mode dispatcher к mode-specific helper’ам.
- Примечание:
  - current padding validation boundary уже должна существовать архитектурно в MVP и позже только заполняться implementation branches;
  - packing-mode-specific behavior must also be able to plug into this area without reshaping the rest of the receive pipeline.

## Done
- [x] 001: Repo skeleton + buildable stub
- [x] 002: Fix WSL networking/DNS for git push
- [x] 003: CTest smoke test
- [x] 004: Endian helpers (read_be16/read_be32) + tests
- [x] 005: Add common error/result type (enum error codes) + tests
- [x] 006: Add `ByteSpan` alias (std::span<const uint8_t>) and conventions doc snippet
- [x] 007: Define `RxVideoConfig` (width/height/fps, ip/port, payload_type, format)
- [x] 008: Define `VideoFrame`/`VideoFrameView` contract (format, planes, stride, ts_ns)
- [x] 009: Define interfaces:
  - `IVideoFrameSink` (on_video_frame; stats optional later)
  - `IRxBackend` / `IRxVideoBackend`
  - Unit test with FakeBackend -> FakeVideoSink (one frame delivered)
- [x] 010: Define RTP header view struct (version, pt, marker, seq16, ts32, ssrc)
- [x] 011: Implement RTP header parser (validate version=2, min length=12) + tests
- [x] 012: Add helper for seq wrap comparison/distance + tests
- [x] 013: Add "extract payload span" (skip CSRC; explicit RTP header-extension tolerance remains a separate follow-up task) + tests
- [x] 020: Define structs for: ExtendedSeqHi16 + SRD header (len,row,offset,F,C)
- [x] 021: Implement parser for ExtSeqHi16 + 1..3 SRD headers + tests (synthetic bytes)
- [x] 022: Implement validation rules (SRD length > 0, <= MAXUDP, C chaining) + tests
- [x] 023: Implement helper: combine 16-bit RTP seq + ExtSeqHi16 => 32-bit ext seq + tests

---

## Spec notes / deviations
- [x] S001: `validate_st2110_20_payload_header()` currently rejects `SRD Length == 0` unconditionally, but ST 2110-20 allows this special case when there is exactly one SRD header and no sample row data follows. This must be fixed before video RX is considered spec-clean.
- [ ] S002: While MVP behavior may stay progressive-only, internal configs, state machines, packet-to-unit grouping, completion logic, and segment placement must model scan mode separately from pixel format and must not hardcode assumptions such as “timestamp group == frame”, “marker == end of frame”, or “F is always zero” in ways that make future interlace / PsF support invasive. ST 2110-20 explicitly distinguishes progressive, interlaced, and PsF behavior for `F`, marker, row numbering, grouping, placement, and signaling, so these assumptions must remain localized in dedicated mode-aware / format-aware helpers.
- [x] S003: `Depacketizer::map_segment_to_frame_write()` currently treats `SRD Offset` as a byte offset, but ST 2110-20 defines it as the horizontal position of the first full-bandwidth sample in the image pixel matrix. For UYVY / 4:2:2 this must be mapped through format-aware logic instead of written directly as bytes. This must be fixed before video RX is considered spec-clean.
- [x] S004: Current UYVY receive path does not validate pgroup alignment constraints implied by ST 2110-20 4:2:2 sampling. For 8-bit 4:2:2, packetized data must respect the pgroup structure and `SRD Length` must remain a multiple of pgroup octet size; offset semantics must also remain aligned with full-bandwidth sample positions. Validation must be added in a localized, format-aware way.
- [x] S005: Current payload validation does not enforce monotonic ordering rules for `SRD Row Number` / `SRD Offset` within a packet. ST 2110-20 requires sample rows to progress top-to-bottom and offsets within a row to progress left-to-right. This must be validated explicitly.
- [x] S006: Task 022 covered only part of payload-header validation. Size/limit checks that depend on packet/payload sizing policy (including the path toward MAXUDP-aware validation) still need an explicit follow-up task so completed work and remaining work are not conflated.
- [x] S007: Public headers currently contain non-trivial function definitions in a way that risks ODR / multiple-definition problems once the project grows beyond the current “mostly one translation unit per test executable” shape. The linkage model must be made explicit (true header-only with `inline`, or moved implementations) before backend/app growth.
- [x] S008: `PacketParseStats` structures exist, but packet parsing does not yet expose a single integrated path that records stage-specific parse results through the real parse flow. This should be fixed so parse observability is not only nominal.
- [ ] S009: Current packet size policy models a configurable UDP payload-size limit, but ST 2110-10 defines `MAXUDP` and receiver size expectations in terms of UDP datagram size, not only essence payload size. Standard UDP Size Limit and Extended UDP Size Limit handling, default behavior when `MAXUDP` is absent, and the receiver assumption around fragmented IP datagrams must be aligned with ST 2110-10 before packet sizing is considered spec-clean.
- [ ] S010: The project now has an initial standards-aware video signaling model boundary, structural validation for key signaling fields, and projection paths into runtime/manual config. However, the receive path is still primarily driven by manual config and does not yet have full runtime integration of signaling-derived configuration or SDP ingestion. ST 2110-10 / -20 / -21 require or define stream interpretation/signaling through SDP attributes such as video sampling/depth/packing/framerate and timing-related attributes including `mediaclk`, `ts-refclk`, `MAXUDP`, `TSMODE`, `TSDELAY`, and sender timing parameters. This work must be completed through explicit runtime integration and a separate SDP parsing/ingestion path rather than by expanding ad hoc manual config assumptions.
- [ ] S011: The current timestamp-strategy plan is still phrased as if internal video timestamps could be derived only from local fps cadence or arrival-time smoothing. For standards-aware ST 2110 receive, internal presentation timestamps must be mapped from RTP timestamp domain and associated clock/signaling model, not from a standalone frame counter alone. The timestamp plan must therefore be reworked around RTP/clock-based mapping.
- [ ] S012: Receiver timing / conformance assumptions from ST 2110-21 are not yet represented in the architecture. The project currently has reorder/depacketize logic but no explicit model for receiver timing class/capability, dependence on stream timing signaling, or the future boundary where ST 2110-21 conformance-related buffering/tolerance behavior will live.
- [x] S013: `parse_packet_view_staged()` currently accepts arbitrary trailing octets after the bytes covered by `SRD Length` values. ST 2110-20 allows octets after the last Sample Row Data Segment only as terminal field/frame padding, and GPM/BPM padding octets are zero-valued. This must be validated through a localized packing-mode-aware / mode-aware boundary rather than silently tolerated on any packet.
- [ ] S014: Current RTP parsing/payload extraction path does not yet provide explicit receiver-side tolerance to RTP header extensions. For a standards-aware receiver, packets with valid RTP header extensions must still have payload location derived correctly rather than being handled only under an implicit “no extensions” assumption. This must be fixed locally in RTP parsing / payload extraction logic and not spread across the rest of the receive pipeline.
- [ ] S015: `VideoPackingMode` is currently modeled in video signaling, but it is not yet carried as an explicit runtime receive axis through depacketizer/runtime config/padding validation. The current MVP runtime path must not stay architecturally GPM-only. If BPM remains unsupported in MVP runtime behavior, that limitation must be explicit, localized, and implemented as an already-existing branch/boundary rather than as absence of architecture.
- [ ] S016: Current standards-aware video signaling representation is still too close to internal runtime/storage concepts and does not yet model enough signaled SDP/media properties separately from internal `PixelFormat` / storage format. This must be expanded so signaled stream description is not collapsed prematurely into runtime-only concepts. In particular, the modeled representation must explicitly account for signaled stream-description properties such as `sampling`, `width`, `height`, `exactframerate`, `depth`, `colorimetry`, `TCS`, `PM`, and `SSN`, with `RANGE` allowed as optional / future-expansion coverage.
- [ ] S017: Audio path currently has no fully completed first-class ST 2110-30 signaling/model boundary, no explicit structural validation layer for that model, no explicit SDP ingestion path into such a model, and no clear modeled representation for signaled channel order / channel mapping distinct from internal audio buffer layout. The audio MVP target must be made explicit as a **Level A-oriented receiver baseline** (`48 kHz`, `1 ms packet time`, `1..8 channels`), and these axes/boundaries must be architecturally introduced in MVP even if some runtime variants remain later implementation work.
- [ ] S018: Receiver-side playout / reconstruction timing boundary is not yet explicit. Mapping from RTP timestamp domain to internal `ts_ns`, receiver playout timing policy, and receiver-side offset/delay configuration must live above reorder/depacketize logic rather than collapsing into arrival-time smoothing or local cadence heuristics.
- [ ] S019: RTP timestamp wraparound and long-running-stream continuity are not yet explicitly covered by timestamp-mapping tasks/tests. This must be handled and tested across reconstruction boundaries so long-lived streams do not silently drift or reset at wraparound.

---

# Phase 1 — MVP

## Track A — Core library foundations (shared / portable)

### A0. Common base abstractions
- [x] 030: Generalize media-facing naming where needed so types/interfaces can grow from video-only to media-oriented without rewrite
  - review current naming/API surface
  - keep video-specific contracts where appropriate
  - avoid blocking future audio path
- [x] 031: Add common stats structs for parsers / depacketizers / backends
- [x] 032: Add common config validation helpers and conventions doc for "strict parse, explicit fallback"
- [x] 033: Make current public-header implementation ODR-safe
  - audit all non-template/non-class function definitions placed in headers
  - either mark true header-only functions `inline` or move implementations into `.cpp`
  - keep the decision consistent across the library
  - add a multi-translation-unit link test so this does not regress
- [x] 034: Fix repo build/test script correctness
  - fix `scripts/build_and_test.sh` strict-mode typo
  - verify the script actually configures, builds, and runs tests from a clean checkout
  - add a minimal CI-oriented smoke check or documented manual verification step

### A1. Video packet model
- [x] 040: Define `PacketView` (rtp header + ext seq32 + srd list + payload bytes)
- [x] 041: Implement `PacketView::from_udp_datagram()` (parses all layers) + tests
- [x] 042: Add packet stats counters (parse_fail, bad_version, short_packet, bad_srd, etc.)
- [x] 043: Fix zero-length SRD special-case according to ST 2110-20 + tests
- [x] 044: Add localized format-aware segment constraints helper(s)
  - define helper/API boundary where packet/segment validation can depend on active video format
  - keep generic ST 2110-20 parsing separate from format-specific receive constraints
  - ensure adding a new format later only requires a new `case`/helper/tests
- [x] 045: Validate SRD ordering rules within one RTP packet
  - `SRD Row Number` must not go backwards
  - within the same row, `SRD Offset` must not go backwards
  - keep progressive-only assumptions localized so interlace/PsF support can be added later
  - add focused tests for valid and invalid 2-SRD / 3-SRD packets
- [x] 046: Add explicit follow-up validation for size-limit policy
  - separate pure wire-format parsing from size-limit/config-policy checks
  - define where MAXUDP-related constraints will live for MVP
  - add tests covering oversized payload / inconsistent header+payload sizing behavior
- [x] 046A: Align packet size policy with ST 2110-10 UDP datagram size semantics
  - model packet-size policy in terms of UDP datagram size, not only essence payload bytes
  - define Standard UDP Size Limit / Extended UDP Size Limit behavior for MVP
  - define default behavior when `MAXUDP` is absent
  - keep pure wire-format parsing separate from SDP/signaling-derived sizing policy
  - document/localize the current stance on fragmented IP datagrams
  - add focused positive/negative tests
- [x] 046B: Add localized validation for trailing payload padding semantics
  - distinguish bytes covered by SRD segments from optional trailing payload padding
  - for current MVP progressive path, allow trailing padding only where current completion / packing policy permits terminal-packet padding
  - validate that accepted padding octets are zero-valued
  - keep generic `PacketView` parsing separate from packing-mode / scan-mode-specific padding policy
  - add focused positive/negative tests
- [x] 047: Add integrated packet-parse stats recording path
  - provide one real parse entry point that records `PacketParseStage` failures/successes
  - make sure the counters reflect the actual parse pipeline instead of only helper-level unit tests
  - add tests for per-stage accounting
- [ ] 047A: Add receiver-side RTP header extension tolerance in RTP parsing / payload extraction path
  - correctly skip RFC 3550 RTP header extension area when extension bit is set
  - keep extension handling localized in RTP parsing / payload-span logic
  - payload extraction must remain correct with combinations of CSRC and header extensions
  - extension contents do not need semantic interpretation in MVP unless required later
  - add focused positive/negative tests

### A2. Video reorder / assemble / depacketize
- [x] 050: Define interface for `ReorderBuffer` (push(packet), pop_next())
- [x] 051: Implement fixed window reorder by ext seq32 (size configurable) + tests
- [x] 052: Implement drop/late accounting (out_of_window, missing_seq) + tests
- [x] 053: Add simple timeout/flush policy (optional but localized) + tests
- [x] 054: Extend `VideoFrame` with mutable storage access for UYVY (`width/height/format`, `stride_bytes`, `data`, `row_data`) + tests
- Note:
  - No separate `FrameBuffer` type for MVP video assembly.
  - `VideoFrame` is the owning assembled-frame storage object.
  - `VideoFrameView` remains the non-owning presentation/view type.
  - `FrameAssembler` should write directly into `VideoFrame`.
- [x] 055: Define `FrameAssembler` lifecycle over `VideoFrame`: begin(ts_rtp), write_segment(row, byte_off, bytes), end(marker)
- [x] 056: Implement bounds checks (row range, offset+len <= stride) + tests
- [x] 057: Implement frame completeness rule:
  - marker seen => frame can be emitted
  - partial state must be tracked explicitly
- [x] 058: Implement partial frame policy: drop / emit-with-flag (configurable) + tests
- [x] 059: Define `Depacketizer` API (push PacketView, returns 0..N completed video units)
- [x] 060: Implement current MVP grouping logic for video units (progressive path) + tests
- [x] 061: Implement current MVP completion behavior for progressive video units + tests
- [x] 062: Connect PacketView SRD list => FrameAssembler writes + tests
- [x] 063: Add depacketizer stats (`units_ok`, `units_partial`, `units_dropped`, `packets_used`)
- [x] 064: Define `VideoScanMode` as a transport / assembly property independent from `PixelFormat`
  - add enum for `Progressive | Interlaced | PsF`
  - thread it through `RxVideoConfig`, `DepacketizerConfig`, and other relevant internal video config/state types
  - keep current MVP behavior implemented only for `Progressive`
  - add tests proving scan mode is modeled separately from pixel format
- [x] 065: Generalize video receive completion semantics so marker/timestamp handling is scan-mode-aware by architecture
  - remove hardcoded internal assumption that `marker => end of frame`
  - remove hardcoded internal assumption that one RTP timestamp group always corresponds to a complete frame
  - introduce a localized mode-dependent policy point for end-of-unit / completion decisions
  - implement only the `Progressive` policy in MVP; non-progressive branches may stay localized as `Unsupported` / not-yet-implemented
  - add tests proving current progressive behavior is unchanged
- [x] 066: Refactor depacketizer / assembly contracts so future interlace / PsF support can be added by filling pre-defined extension points, not by rewriting the pipeline
  - make depacketizer output/unit model generic (`AssembledVideoUnit`, unit-oriented stats, unit-oriented public API)
  - avoid baking “frame-only” semantics into depacketizer public contract, state, and counters
  - keep `FrameAssembler` byte-oriented and format-agnostic
  - keep current public progressive behavior intact for MVP
  - document/localize the current non-progressive runtime boundary
  - add focused tests for architecture-level behavior and localized rejection of non-progressive modes
- [x] 066A: Introduce `VideoAssemblyKey` and move packet-to-unit grouping decisions out of `Depacketizer::push()`
  - define a mode-aware `VideoAssemblyKey` type for "which assembly unit this packet belongs to"
  - add helper(s) that derive assembly key from `PacketView` + `VideoScanMode`
  - add helper(s) for "same unit / starts new unit" comparison
  - make `Depacketizer::push()` use assembly-key helpers instead of hardcoding raw RTP timestamp grouping
  - implement only the `Progressive` case in MVP; keep `Interlaced` / `PsF` branches localized and explicitly unsupported / placeholder
  - add tests proving future scan modes can extend grouping semantics without changing `push()`
- [x] 067: Introduce an explicit video segment placement boundary and fix UYVY mapping so `SRD Offset` is interpreted in full-bandwidth sample units, not raw bytes
  - define a localized mode-aware + format-aware mapper from packet segment semantics to frame write operations
  - keep `FrameAssembler` byte-oriented and format-agnostic
  - implement only the current `Progressive + UYVY` case in MVP
  - keep `Interlaced` / `PsF` placement branches localized as placeholder / unsupported until later implementation
  - convert ST 2110-20 offset semantics to frame write byte offsets explicitly
  - add tests proving the write lands at the correct byte position
- [x] 068: Enforce pgroup alignment constraints for the current MVP video format through the localized segment-placement / validation boundary
  - for the current MVP format (`UYVY` / 4:2:2 / 8-bit), validate segment length/alignment rules implied by ST 2110-20 packetization
  - reject misaligned segment offsets/lengths explicitly
  - keep the checks localized so future formats, depths, and scan modes add their own rules rather than branching through generic assembler or depacketizer code
  - add positive/negative tests
- [x] 069: Ensure depacketizer / frame-assembly / future reconstruction path stays extensible
  - review where grouping, completion, placement, and validation logic live after 066 / 066A / 067 / 068
  - confirm that adding a second pixel format later requires localized additions only
  - confirm that adding `Interlaced` / `PsF` later requires filling mode-aware helpers / mappers, not rewriting `Depacketizer::push()`
  - define and document the boundary where future field / segment pairing and final picture reconstruction will plug in above depacketizer-emitted generic units
  - add a small architecture-focused test or compile-time check where useful
- [x] 069A: Add explicit video receive pipeline that composes depacketizer with video-unit reconstructor
  - keep `Depacketizer` and `IVideoUnitReconstructor` as separate layers
  - define `VideoReceivePipelineConfig` with `DepacketizerConfig` + `VideoUnitReconstructorConfig`
  - validate config consistency (`format` / `scan_mode`) between depacketizer and reconstructor configs
  - construct the reconstructor via `make_video_unit_reconstructor(...)`
  - implement `push(const PacketView&) -> std::vector<ReconstructedVideoFrame>` by feeding depacketizer-emitted units into the reconstructor
  - implement `reset()` so both depacketizer and reconstructor are reset
  - keep current MVP behavior implemented only for `Progressive`
  - keep non-progressive runtime boundary localized at reconstructor creation / factory path
  - add focused tests for composition, reset, and config mismatch
- [ ] 069B: Add a standards-aware video SDP/signaling model boundary
  - **цель этой группы задач в MVP — заложить полную архитектурную ось signaling model и projection boundaries, даже если часть runtime branches и parsing coverage будет заполняться позже**
  - [x] 069B1: Define modeled video stream/signaling types separate from low-level depacketizer/runtime config
    - include key stream-description properties needed for current MVP architecture:
      - video packing mode (`GPM` / `BPM`)
      - timing-related signaling such as `mediaclk`, `ts-refclk`, `MAXUDP`, `TSMODE`, `TSDELAY`
      - ST 2110-21 sender timing/signaling properties such as sender type (`TP`) and optional `TROFF` / `CMAX`
    - model reference-clock signaling through an extensible `ReferenceClock` structure rather than a closed enum-only representation
    - предусмотреть modeled video SDP/media properties, которые нельзя сводить только к internal `PixelFormat` / runtime storage format
    - explicitly cover modeled representation for signaled stream-description properties such as:
      - `sampling`
      - `width`
      - `height`
      - `exactframerate`
      - `depth`
      - `colorimetry`
      - `TCS`
      - `PM`
      - `SSN`
      - `RANGE` as optional / future-expansion field
  - [x] 069B2: Add explicit structural validation boundaries inside signaling model
    - validate reference clock consistency
    - validate sender timing signaling consistency
    - validate media clock / timestamp mode enums
    - add a localized future timing-related interpretation entry point where `TSDELAY` is carried through even if full semantics are not yet implemented
  - [x] 069B3: Add explicit adapters/projections from `VideoStreamSignaling`
    - derive `PacketParsePolicy` from signaling model
    - derive runtime/manual `RxVideoConfig` from signaling model while injecting transport/network fields separately
    - validate signaling first, then validate projected runtime config
  - [ ] 069B4: Add explicit projection from `VideoStreamSignaling` to runtime video receive pipeline config
    - derive `DepacketizerConfig` from signaling model
    - derive `VideoUnitReconstructorConfig` from signaling model
    - derive `VideoReceivePipelineConfig` from signaling model
    - keep runtime policy inputs that are not signaled (for example `PartialFramePolicy`) as explicit adapter parameters rather than hiding them inside signaling model
    - ensure future non-progressive modes are projected structurally without baking runtime-support assumptions into the adapter
  - [ ] 069B5: Define the runtime integration boundary where signaling-derived config becomes the primary receiver bootstrap path
    - make signaling-derived config a first-class runtime input alongside the current manual/synthetic path
    - keep current manual-config path usable for tests and scaffolding
    - do not require full SDP parser yet; only make the receiver-side integration boundary explicit
    - include composition of signaling-derived packet parse policy and signaling-derived receive pipeline config as one receiver bootstrap path
    - add focused tests for signaling-driven config composition / mismatch handling
- [ ] 069C: Define an explicit ST 2110-21 video receiver timing/conformance boundary
  - **цель этой задачи в MVP — заложить capability/timing/tolerance architecture boundary, even if полный standards-aware behavior будет реализовываться позже**
  - introduce a receiver timing/capability model boundary instead of burying receiver assumptions inside depacketizer/reorder code
  - define explicit receiver capability / receiver class assumptions and where they are configured
  - define dependence on signaled timing properties (`mediaclk`, `ts-refclk`, `TSMODE`, `TSDELAY`, sender timing signaling such as `TP` / `TROFF` / `CMAX`)
  - define where buffering / tolerance policy and future ST 2110-21 conformance-related behavior will live
  - coordinate this boundary with the receiver playout / reconstruction timing boundary from A3 instead of mixing it into parser/depacketizer logic
  - keep these behaviors outside reorder/depacketizer internals
  - add architecture-focused tests or compile-time checks where useful
- [ ] 069D: Add SDP parsing / ingestion path for video signaling model
  - parse relevant SDP / media-description attributes into `VideoStreamSignaling`
  - include signaled video media properties, timing-related signaling, and transport/signaling fields required by current modeled boundary
  - keep parsing separate from validation and separate from runtime config projection
  - add focused tests for valid/invalid SDP field mapping
- [ ] 069E: Thread `VideoPackingMode` into runtime receive path as an explicit axis
  - **цель этой задачи в MVP — протащить packing mode как runtime/config/policy axis уже сейчас, even if часть branches пока останется `Unsupported`**
  - extend runtime receive configs / projections so packing mode reaches depacketizer, packet interpretation, and padding-validation boundaries
  - localize GPM/BPM-specific receive rules instead of leaving packing behavior implicit
  - if current MVP runtime remains GPM-only, reject BPM through an explicit localized runtime-support boundary rather than silently ignoring it
  - ensure later BPM work can be done by filling already-existing branches without changing pipeline shape/contracts
  - add focused tests for config projection and localized packing-mode behavior / rejection

### A3. Video timestamp strategy
- [x] 070: Define internal timestamp type: `uint64_t ts_ns`
- [ ] 071: Define a standards-aware video timestamp mapping boundary from RTP timestamp domain to internal `ts_ns`
  - **цель этой задачи в MVP — заложить correct timing architecture boundary, even if fuller standards-aware implementation comes later**
  - keep RTP timestamp domain distinct from internal nanoseconds-domain timestamps
  - explicitly handle 32-bit RTP timestamp wraparound and long-running streams
  - define where `mediaclk` / `ts-refclk` / `TSMODE` / `TSDELAY`-related interpretation will plug into the receive pipeline
  - allow a localized synthetic/manual timing path for tests and offline tools, but do not make standalone fps cadence the primary standards-facing timing model
  - keep timestamp mapping above depacketizer and separate from segment placement / packet grouping logic
- [ ] 071A: Define explicit receiver-side playout / reconstruction timing boundary
  - separate RTP-domain timestamp mapping from receiver playout / reconstruction release policy
  - define where receiver-side offset/delay configuration (including future Link Offset Delay-like boundary) will live
  - define how this boundary interacts with reconstructed units / frames without burying policy in parser/reorder/depacketizer code
  - keep arrival-time smoothing, if any, as a localized optional policy and not the standards-facing timing model
- [ ] 072: Add tests for video timestamp mapping invariants
  - monotonicity of emitted internal timestamps
  - correct behavior across 32-bit RTP timestamp wraparound
  - stable mapping behavior across packet grouping / reconstruction boundaries
  - long-running stream continuity tests
  - focused tests for the synthetic/manual timing path used in MVP scaffolding

---

## Track B — Audio foundations (MVP scope)

> Audio MVP should be planned against ST 2110-30 from the start. Current MVP target should assume a narrow but explicit standards-aware baseline first: a **Level A-oriented receiver baseline** with `48 kHz`, `1 ms packet time`, and `1..8 channels`, with broader profile/level expansion later on top of the already-laid architecture.

### B0. Audio common abstractions
- [ ] 079: Define a standards-aware audio SDP/signaling model boundary for ST 2110-30
  - **цель этой группы задач в MVP — заложить audio signaling/model architecture boundary уже сейчас, even if only a narrow baseline is fully implemented**
  - define modeled audio stream/signaling config separate from low-level `RxAudioConfig`
  - make the MVP target explicit as a **Level A-oriented receiver baseline** rather than a generic PCM placeholder
  - capture at least the signaled media properties needed for the initial baseline (`48 kHz`, `1 ms packet time`, `1..8 channels`, AES67-compatible receive assumptions where applicable)
  - keep signaled audio/media properties separate from internal audio buffer layout/runtime config
- [ ] 079A: Add explicit structural validation boundary inside audio signaling model
  - validate structural consistency of modeled audio signaling
  - validate MVP baseline / conformance assumptions for the initial Level A-oriented receiver baseline
  - validate sample rate / packet-time / channel-count consistency within the chosen baseline
  - validate signaled channel-order / channel-mapping consistency where applicable
  - keep this validation boundary separate from SDP parsing and separate from runtime config projection
- [ ] 079B: Add explicit projection/adapters from audio signaling model to runtime config
  - derive `RxAudioConfig` and later runtime audio receive config from modeled signaling
  - keep transport/network fields and local receiver policy inputs explicit rather than hiding them inside signaling model
  - add focused tests for signaling-to-runtime projection and mismatch handling
- [ ] 079C: Add SDP parsing / ingestion path for audio signaling model
  - parse relevant ST 2110-30 / SDP attributes into modeled audio signaling structures
  - keep parsing separate from validation and separate from runtime config projection
  - add focused tests for valid/invalid SDP field mapping
- [ ] 079D: Add channel-order / channel-mapping modeled boundary and validation
  - represent signaled channel order / channel mapping separately from internal audio buffer layout
  - define where future reordering/adaptation will live
  - add focused tests
- [ ] 079E: Define the runtime integration boundary where signaling-derived audio config becomes the primary receiver bootstrap path
  - make signaling-derived audio config a first-class runtime input alongside the current manual/synthetic path
  - keep current manual-config path usable for tests and scaffolding
  - do not require full SDP parser yet; only make the receiver-side integration boundary explicit
  - ensure future channel-order / channel-mapping implementations plug into already-existing modeled/projection boundaries rather than reshaping runtime contracts
  - add focused tests for signaling-driven audio config composition / mismatch handling
- [ ] 080: Define `RxAudioConfig` (sample_rate, channels, packet_time / samples_per_packet, payload_type, ip/port, format)
  - make the initial MVP target a narrow ST 2110-30 baseline rather than a format-free placeholder
  - make that baseline explicit as a **Level A-oriented receiver baseline** with `48 kHz`, `1 ms packet time`, and `1..8 channels`
  - capture at least the parameters needed for that initial receive path
- [ ] 081: Define `AudioBuffer` / `AudioFrameView` contract
- [ ] 082: Define audio sink/backend-facing interfaces or extend shared interfaces so audio can be supported without ломки video API
- [ ] 083: Add FakeAudioBackend -> FakeAudioSink test

### B1. Audio packet/depacketize MVP
- [ ] 090: Define audio RTP packet model needed by MVP
  - align the MVP audio packet model with the initial ST 2110-30 baseline rather than a generic future audio placeholder
- [ ] 091: Implement minimal audio RTP parser integration + tests
- [ ] 092: Implement audio reorder/jitter handling MVP + tests
- [ ] 093: Implement audio frame/block assembly MVP + tests
- [ ] 094: Add audio stats (packets_ok, packets_lost, blocks_ok, blocks_partial/dropped)

### B2. Audio timestamp strategy
- [ ] 095: Define audio timestamp mapping / playout timing boundary to internal `ts_ns`
  - keep RTP timestamp domain distinct from internal timestamps
  - handle RTP timestamp wraparound / long-running streams explicitly
  - keep standards-aware timing interpretation separate from local cadence heuristics
  - separate RTP-domain mapping from receiver-side playout / block-release policy
- [ ] 096: Add monotonicity / cadence tests for audio path
  - include wraparound and long-running continuity cases where applicable

---

## Track C — Linux backends (both required in MVP)

### C0. Socket backend common
- [ ] 100: Refactor backend layer so socket/mtl can expose both video and audio capabilities without duplication explosion
- [ ] 101: Add backend factory / selector design (`socket|mtl`) in extendable form

### C1. Socket video RX
- [ ] 110: Implement `SocketRxVideoBackend` skeleton + smoke test
- [ ] 111: Implement UDP socket open/bind (unicast base path)
- [ ] 112: Implement multicast join/leave (Linux) for socket video receive path
- [ ] 113: Add receive loop (recvfrom/recvmmsg later) and feed PacketView pipeline
- [ ] 114: Add periodic stats print (pps, drops, frames/s)
- [ ] 115: Add graceful stop (SIGINT) and cleanup

### C2. Socket audio RX
- [ ] 120: Implement `SocketRxAudioBackend` skeleton + smoke test
- [ ] 121: Implement UDP socket open/bind for audio
- [ ] 121A: Implement multicast join/leave (Linux) for socket audio receive path
- [ ] 122: Connect audio parser/reorder/assembler pipeline
- [ ] 123: Add periodic stats print (pps, drops, audio blocks/s)
- [ ] 124: Add graceful stop and cleanup reuse

### C3. MTL video RX
- [ ] 130: CMake option `ST2110_WITH_MTL` + build guard
- [ ] 131: Implement `MtlRxVideoBackend` skeleton + smoke test
- [ ] 132: Implement minimal start/stop using MTL ST20P RX (get_frame/put_frame)
- [ ] 133: Map MTL frame -> `VideoFrame`/`VideoFrameView` and deliver to sink
- [ ] 134: Basic stats (frames, drops if available)

### C4. MTL audio RX
- [ ] 140: Investigate minimal viable MTL audio receive path/API
- [ ] 141: Implement `MtlRxAudioBackend` skeleton + smoke test
- [ ] 142: Implement minimal audio start/stop and frame/block delivery
- [ ] 143: Add basic audio stats

---

## Track D — Apps (MVP tools, no OBS yet)

### D0. Unified dump tools
- [ ] 150: Create app skeleton(s): args parsing for media type, width/height/fps or audio params, format, backend + help
- [ ] 151: Add backend selector: `--backend=socket|mtl`
- [ ] 152: Add media selector: `--media=video|audio`
- [ ] 153: Implement synthetic mode for video:
  - feed depacketizer with synthetic packets
  - produce N frames to files + basic stats
- [ ] 154: Implement synthetic mode for audio:
  - feed audio pipeline with synthetic packets
  - produce N output blocks/files + stats
- [ ] 155: Add video frame file writer (UYVY raw) + filename scheme + tests
- [ ] 156: Add audio dump writer + size/sanity tests
- [ ] 157: Add debug output for first N packet headers
- [ ] 158: Add ability to save bad packets for offline repro

---

## Track E — OBS plugin MVP (video + audio, basic UI)

### E0. Environment
- [ ] 170: Create Ubuntu 24.04 VM / environment for OBS plugin work
- [ ] 171: Install OBS and verify it runs
- [ ] 172: Decide code sync method (git clone / shared folder / artifacts)

### E1. Plugin skeleton
- [ ] 180: Add `obs_plugin/` target that builds `.so` and installs to OBS plugins dir
- [ ] 181: Implement source/input skeleton for ST 2110 media
- [ ] 182: Implement minimal UI/properties for:
  - media kind
  - backend selector
  - IP/port/payload type
  - video params
  - audio params
  - start/stop
- [ ] 183: Implement black video / silence audio fallback path with no network

### E2. Connect backends to OBS
- [ ] 190: Wire socket backend to OBS source path
- [ ] 191: Wire MTL backend to OBS source path where available
- [ ] 192: Implement background receive threads and handoff into OBS
- [ ] 193: Implement frame queue / audio queue from backend threads to OBS
- [ ] 194: Implement timestamp mapping for OBS
- [ ] 195: Verify start/stop stability and repeated reconfiguration

---

## Track F — MVP exit / readiness for testing
- [ ] 198: End-to-end video MVP demo on Linux: receive and display/save progressive ST2110-20 GPM stream
- [ ] 199: End-to-end audio MVP demo on Linux: receive and play/save audio stream
- [ ] 200: End-to-end OBS demo with selectable backend and basic UI
- [ ] 201: Document manual test procedure for MVP
- [ ] 202: Document known limitations still allowed at MVP exit
  - if some runtime branches remain intentionally `Unsupported` behind already-modeled standard axes, document them explicitly as localized MVP limitations
  - if signaling-driven bootstrap is still partial, document the remaining manual-config scaffolding explicitly

---

# Phase 2 — Medium

## Track G — Video formats / audio formats / extensibility
- [ ] 210: Add at least one more video pixel format beyond UYVY
- [ ] 211: Audit format-specific code paths so new formats require localized additions only
- [ ] 212: Add additional audio format/profile support if needed
- [ ] 213: Add shared format capability description/query API
- [ ] 214: Expand standards-aware video signaling / media-property coverage through the already-modeled video signaling representation
  - add support for additional signaled video/media-property variants without changing the core signaling/runtime contracts introduced in MVP
  - keep parsing, validation, and projection extensions localized to existing model/adapter boundaries
- [ ] 215: Expand audio signaling / channel-order / channel-mapping support through the already-modeled audio signaling boundary
  - add implementation for additional channel-order / channel-mapping cases without changing the core audio signaling/runtime contracts introduced in MVP
  - keep reordering/adaptation localized to the pre-defined boundaries

## Track H — Correctness improvements
- [ ] 220: Improve loss handling policies for video (freeze/black/emit partial/drop)
- [ ] 221: Improve loss handling for audio (gap/conceal/drop policy)
- [ ] 222: Review parser/assembler behavior against spec corner-cases
- [ ] 223: Add stricter validation for payload sizes, pgroups, offsets, timestamps
- [ ] 224: Re-check known deviations list and burn it down
- [ ] 225: Implement interlaced video receive semantics
  - accept and interpret `F` for first/second field
  - implement correct marker semantics for end-of-field
  - implement row numbering semantics per field
  - fill the pre-defined scan-mode-aware grouping / completion / placement extension points introduced in MVP
  - keep `Depacketizer::push()` and the generic depacketizer pipeline unchanged
  - add focused tests
- [ ] 226: Implement PsF video receive semantics
  - accept and interpret `F` as segment indicator for PsF
  - implement correct marker semantics for end-of-segment
  - implement row numbering semantics per segment
  - fill the pre-defined scan-mode-aware grouping / completion / placement extension points introduced in MVP
  - keep `Depacketizer::push()` and the generic depacketizer pipeline unchanged
  - add focused tests
- [ ] 227: Implement field / segment pairing and final picture reconstruction policy
  - define how two fields or two PsF segments are paired into the final output picture
  - keep pairing / reconstruction logic separate from depacketizer packet grouping, completion, and byte-writing
  - make pairing consume generic depacketizer-emitted video units instead of changing depacketizer contracts
  - add tests for ordering, completeness, and partial/loss behavior
- [ ] 228: Add scan-mode signaling / selection path from stream description and config
  - define where `Progressive | Interlaced | PsF` is selected from SDP/config
  - validate consistency between signaled mode and runtime packet semantics
  - add tests for signaling/selection and mismatch handling
- [ ] 229: Implement BPM runtime receive behavior through the already-modeled packing-mode branches
  - fill BPM-specific depacketize / padding / validation / runtime-policy logic through existing packing-mode dispatch/boundaries
  - keep signaling model, runtime config shape, and pipeline contracts unchanged
  - add focused tests
- [ ] 229A: Implement fuller ST 2110-21 receiver timing / tolerance behavior through the already-defined timing/capability/playout boundaries
  - fill buffering / tolerance / release behavior inside the boundaries introduced in MVP
  - keep parser/reorder/depacketizer contracts unchanged
  - add focused tests

## Track I — Operational quality
- [ ] 230: Better logging and structured stats
- [ ] 231: Runtime config validation / better error messages
- [ ] 232: More ergonomic CLI for tools
- [ ] 233: More informative OBS UI and status readout
- [ ] 234: Better shutdown/restart/reconfigure stability

---

# Phase 3 — Plugin

## Track J — Plugin polish
- [ ] 240: Improve OBS properties UI and defaults
- [ ] 241: Add validation in UI before start
- [ ] 242: Add clearer runtime state/errors in OBS
- [ ] 243: Improve frame/audio queue behavior for live usage
- [ ] 244: Reduce plugin-specific code duplication
- [ ] 245: Verify plugin architecture stays backend-agnostic and media-agnostic

---

# Phase 4 — Tests

## Track K — Systematic testing
- [ ] 250: Expand unit tests for all parser/reorder/assembler paths
- [ ] 251: Add synthetic integration tests for full video pipeline
- [ ] 252: Add synthetic integration tests for full audio pipeline
- [ ] 253: Add backend smoke/integration tests (socket + mtl where possible)
- [ ] 254: Add OBS plugin smoke tests / scripted manual checklist
- [ ] 255: Add regression tests for every fixed bug from `Spec notes / deviations`
- [ ] 256: Add corpus of captured bad packets / sample streams for repro
- [ ] 257: Run focused testing and identify weakest subsystems before hardening

---

# Phase 5 — Hardening

## Track L — Performance / resilience / deeper correctness
- [ ] 260: Replace recvfrom with recvmmsg where useful + benchmark
- [ ] 261: Memory/pool optimizations and frame/buffer reuse
- [ ] 262: Optional: pcap ingest tool for offline tests
- [ ] 263: Better loss recovery and concealment policy
- [ ] 264: Investigate PTP / CLOCK_TAI / PHC only if really needed later
- [ ] 265: Harden thread lifecycle and shutdown ordering
- [ ] 266: Harden long-run stability and leak checks
- [ ] 267: Final audit of spec compliance and removal of temporary limitations

---

# Phase 6 — Windows port (optional, own backend only)

## Track M — Optional Windows support
- [ ] 300: Decide whether Windows port is worth doing after Linux result is stable
- [ ] 301: Introduce OS abstraction layer for sockets if still justified
- [ ] 302: Implement Winsock backend (unicast first)
- [ ] 303: Implement multicast join on Windows
- [ ] 304: Build & run dump tool(s) on Windows
- [ ] 305: Evaluate whether OBS Windows plugin integration is worth the effort
- [ ] 306: Do not port MTL backend; Linux-only by design