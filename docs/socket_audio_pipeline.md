#### `socket_rx_audio_backend.hpp`

Содержит concrete Socket receive backend для audio.

Файл связывает общий socket receive runtime из `SocketRxSingleMediaBackendBase` с audio-specific обработкой:

```text
SocketAudioStartConfig
→ socket receive runtime
→ RTP/ST 2110-30 audio packet parsing
→ audio reorder buffer
→ audio frame/block assembly
→ IFrameSink::on_audio_frame
```

---

##### `SocketRxAudioBackend`

```c++
class SocketRxAudioBackend final : public SocketRxSingleMediaBackendBase;
```

`SocketRxAudioBackend` наследуется от общего socket backend base и реализует audio-specific части pipeline-а.

Конструктор принимает:

```c++
SocketRxAudioBackend(const SocketAudioStartConfig &cfg);
```

Из `SocketAudioStartConfig` он передает в base class:

```text
default socket port factory
MAXUDP первой leg
ReorderBufferConfig
SocketRxOpenConfig для всех legs
expected RTP payload type
```

Audio-specific state сохраняется отдельно:

```c++
AudioMediaDescription media_;
std::uint32_t samples_per_packet_;
AudioFrameAssembler audio_frame_assembler_;
```

`media_` и `samples_per_packet_` нужны для parsing-а audio RTP payload-а и сборки audio block-а.

`cfg.stream.channel_order` в этом backend-е сейчас не сохраняется и не используется. То есть SDP `channel-order` доходит до `SocketAudioStartConfig`, но дальше Socket audio backend не применяет его для channel mapping, speaker layout или перестановки каналов.

---

##### Start sequence

```c++
RxBackendLifecycleResult start(IFrameSink *sink) override;
```

`start` выполняет audio-specific initialization, а затем запускает общий socket runtime:

```text
1. Создать audio reorder buffer
2. Вызвать start_common_runtime(sink)
```

После этого общий base class открывает socket ports, запускает receive threads и downstream thread.

---

##### Packet parsing

```c++
std::expected<std::unique_ptr<PacketView>, Error>
parse_packet(std::size_t leg_index, ByteSpan udp_payload) override;
```

`parse_packet` — audio-specific реализация abstract method из `SocketRxSingleMediaBackendBase`.

Pipeline:

```text
UDP payload
→ parse_audio_rtp_packet_view(...)
→ AudioPacketView
→ PacketView
```

---

##### Reorder buffer

```c++
std::unique_ptr<IReorderBuffer>
make_reorder_buffer(const ReorderBufferConfig &cfg) override;
```

Audio backend использует fixed-window reorder buffer в audio mode:

```c++
FixedWindowReorderBuffer<false>
```

Размер окна берется из:

```c++
cfg.window_size_packets
```

В отличие от video mode, audio mode внутри `FixedWindowReorderBuffer` использует 16-bit RTP sequence-style ordering.

---

##### Media delivery

```c++
void deliver_media(std::unique_ptr<StoredPacket> packet) override;
```

`deliver_media` вызывается base class-ом после reorder stage.

Pipeline:

```text
StoredPacket
→ AudioPacketView
→ AudioFrameAssembler::push
→ AssembledAudioBlock
→ IFrameSink::on_audio_frame
```

Шаги:

1. Получить media-specific view из stored packet:
2. Передать packet в audio assembler:
3. Доставить assembled audio block в sink:

---

##### Audio block delivery

```c++
void deliver_assembled_audio_block(
    AssembledAudioBlock &&block
) const noexcept;
```

Метод обновляет статистику и вызывает sink callback.

Наружу backend отдает уже собранный `AudioBuffer`.

---

##### Shutdown

```c++
~SocketRxAudioBackend() override;
```

Деструктор вызывает:

```c++
SocketRxSingleMediaBackendBase::stop()
```

Это останавливает receive threads, закрывает ports, очищает queue и сбрасывает reorder state.

---

#### `audio_packet.hpp`

Содержит media-specific packet view и parser для ST 2110-30 audio RTP packets.

---

##### `AudioPacketView`

```c++
struct AudioPacketView final : PacketView {
    std::uint32_t sampling_rate_hz;
    std::uint16_t channel_count;
    std::uint32_t samples_per_channel;
    AudioPcmBitDepth pcm_bit_depth;

    std::unique_ptr<StoredPacket> store() const override;
    std::uint32_t reorder_sequence() const override;
};
```

`AudioPacketView` — concrete `PacketView` для audio RTP packet-а.


Добавляет audio-specific metadata:

- `sampling_rate_hz` — sample rate из SDP;
- `channel_count` — количество audio channels;
- `samples_per_channel` — количество samples per channel в одном RTP packet-е;
- `pcm_bit_depth` — PCM bit depth: `Bits16` или `Bits24`.

---

##### Store

```c++
std::unique_ptr<StoredPacket> store() const override;
```

`store()` превращает non-owning `AudioPacketView` в owning `AudioStoredPacket`.
---

##### Wire sample size

```c++
std::size_t audio_pcm_wire_sample_bytes(
    AudioPcmBitDepth bit_depth
);
```

Возвращает размер одного PCM sample-а в RTP payload-е:

```text
Bits16 → 2 bytes
Bits24 → 3 bytes
```

---

##### Expected RTP payload size

```c++
std::size_t audio_rtp_packet_payload_size_bytes(
    AudioPcmBitDepth bit_depth,
    std::uint32_t samples_per_packet,
    std::uint16_t channel_count
);
```

Вычисляет ожидаемый размер RTP payload-а:

```text
payload_size =
    samples_per_packet
    * channel_count
    * bytes_per_sample
```

Например:

```text
48 samples × 2 channels × 3 bytes = 288 bytes
```

для 48 kHz, 1 ms packet time, stereo, 24-bit PCM.

---

##### Packet view construction

```c++
AudioPacketView make_audio_rtp_packet_view(
    const RtpHeaderView &rtp,
    ByteSpan payload,
    const AudioMediaDescription &media,
    std::uint32_t samples_per_packet
);
```

Создает `AudioPacketView` из уже разобранного RTP header-а, RTP payload-а и audio media description.

Эта функция не валидирует payload size. Она только заполняет view.

---

##### Audio RTP packet parsing

```c++
std::expected<AudioPacketView, Error>
parse_audio_rtp_packet_view(
    ByteSpan udp_payload,
    const AudioMediaDescription &media,
    std::uint32_t samples_per_packet
);
```

Основной parser audio packet-а.

Порядок работы:

```text
1. parse_rtp_header(udp_payload)
2. rtp_payload_span(...)
3. вычислить expected audio RTP payload size
4. проверить, что payload.size() == expected size
5. создать AudioPacketView
```

Если RTP header некорректен, возвращается ошибка из `parse_rtp_header`.

---

#### `audio_frame.hpp`

Содержит owning container для decoded audio block-а и view-структуру для передачи audio data дальше по pipeline.

---

##### `AudioFrameView`

```c++
struct AudioFrameView {
    std::uint32_t sampling_rate_hz;
    std::uint16_t channel_count;
    std::uint32_t samples_per_channel;

    const std::int32_t *samples;
    std::size_t total_sample_count;
    std::size_t sample_frame_stride;
    std::size_t size_bytes;

    TimestampNs timestamp_ns;
};
```

`AudioFrameView` — non-owning view на audio samples.

Он содержит:

- `sampling_rate_hz` — sample rate;
- `channel_count` — количество каналов;
- `samples_per_channel` — количество samples на канал;
- `samples` — pointer на interleaved audio samples;
- `total_sample_count` — общее количество samples во всех каналах;
- `sample_frame_stride` — stride одного audio sample frame-а, сейчас равен `channel_count`;
- `size_bytes` — размер sample buffer-а в bytes;
- `timestamp_ns` — timestamp audio block-а.

---

##### `AudioBuffer`

```c++
class AudioBuffer;
```

`AudioBuffer` владеет decoded audio samples.

Внутри samples хранятся как interleaved `int32_t`:

```text
sample 0 channel 0
sample 0 channel 1
...
sample 1 channel 0
sample 1 channel 1
...
```

То есть linear index вычисляется так:

```text
linear_index = sample_index * channel_count + channel
```

---

##### Доступ к samples

```c++
std::int32_t *samples();
const std::int32_t *samples() const;
```

Возвращают pointer на начало interleaved sample buffer-а.

Для доступа к конкретному sample/channel есть методы:

```c++
std::int32_t &sample(
    std::uint32_t sample_index,
    std::uint16_t channel
);

const std::int32_t &sample(
    std::uint32_t sample_index,
    std::uint16_t channel
) const;
```

Если `sample_index` или `channel` выходят за границы buffer-а, методы бросают:

```c++
std::out_of_range
```

---

##### View creation

```c++
AudioFrameView view(
    TimestampNs timestamp_ns = 0
) const;
```

`view` создает non-owning `AudioFrameView` поверх внутреннего sample buffer-а.

`AudioFrameView` используется для передачи audio data наружу без копирования.

---

#### `audio_frame_assembler.hpp`

Содержит audio assembler для Socket audio receive path.

Файл преобразует один упорядоченный `AudioPacketView` в один decoded audio block.

---

##### `AssembledAudioBlock`

```c++
struct AssembledAudioBlock {
    AudioBuffer buffer;
    std::uint32_t rtp_timestamp;
    TimestampNs receive_timestamp_ns;
    std::uint16_t rtp_sequence_number;
    bool rtp_marker;
    bool complete;
};
```

`AssembledAudioBlock` — результат обработки одного audio RTP packet-а.

Он содержит:

- `buffer` — decoded interleaved audio samples в `AudioBuffer`;
- `rtp_timestamp` — RTP timestamp packet-а;
- `receive_timestamp_ns` — локальный timestamp приема;
- `rtp_sequence_number` — RTP sequence number;
- `rtp_marker` — RTP marker bit;
- `complete` — признак полноты audio block-а.

Сейчас `complete` всегда выставляется в `true`, потому что assembler обрабатывает один уже валидированный RTP packet как один complete audio block.

---

##### PCM decoding

```c++
std::int32_t decode_audio_pcm_wire_sample_to_s32(
    ByteSpan sample_bytes,
    AudioPcmBitDepth bit_depth
);
```

Функция декодирует PCM sample из RTP wire format в signed 32-bit sample.

Поддерживаются два варианта:

```text
Bits16:
    2 bytes Big-Endian signed PCM
    sign extend
    scale to int32 by * 65536

Bits24:
    3 bytes Big-Endian signed PCM
    sign extend
    scale to int32 by * 256
```

То есть output format assembler-а — не исходные 16/24-bit bytes, а нормализованное `std::int32_t` представление.

---

##### `AudioFrameAssembler`

```c++
class AudioFrameAssembler;
```

Основной метод:

```c++
AssembledAudioBlock push(const AudioPacketView &packet);
```

`push` принимает один audio packet после RTP parsing и reorder stage-а.

Порядок работы:

```text
1. Определить bytes per sample по PCM bit depth
2. Создать AudioBuffer:
   - sampling_rate_hz
   - channel_count
   - samples_per_channel
3. Пройти по всем sample_index
4. Для каждого sample_index пройти по всем channel
5. Найти offset sample-а внутри RTP payload-а
6. Декодировать wire PCM sample в int32
7. Записать sample в AudioBuffer
8. Вернуть AssembledAudioBlock
```

Payload layout предполагается interleaved:

```text
sample 0 channel 0
sample 0 channel 1
...
sample 1 channel 0
sample 1 channel 1
...
```

Linear offset внутри RTP payload-а вычисляется как:

```text
sample_ordinal =
    sample_index * channel_count + channel

payload_offset =
    sample_ordinal * bytes_per_sample
```

---

##### Statistics

```c++
struct AudioFrameAssemblerStats {
    std::uint64_t packets_used;
    std::uint64_t blocks_emitted;
};
```

Assembler ведет простую статистику:

- `packets_used` — сколько audio packets было обработано;
- `blocks_emitted` — сколько audio blocks было выдано.

После каждого successful `push` оба счетчика увеличиваются на `1`.

```c++
void reset();
const AudioFrameAssemblerStats &stats() const;
```

`reset` сбрасывает статистику.

---