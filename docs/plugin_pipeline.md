#### `plugin_api.hpp`

Содержит общие строковые идентификаторы OBS plugin/source и property values, которые используются в OBS UI и при чтении настроек source-а.

Основные группы констант:

```text
pluginId / pluginName / pluginDescription
```

Идентифицируют OBS module/plugin.

```text
sourceId / sourceName
```

Идентифицируют OBS source type: `ST 2110 Source`.

```text
sourceSelectionPropertyId
```

Property для выбора найденного ST 2110 source-а.

```text
sourceBackendPropertyId
sourceBackendSocketValue
sourceBackendMtlValue
```

Property и значения выбора receive backend-а:

```text
Socket
MTL
```

```text
sourceReorderGapPolicyPropertyId
sourceReorderGapPolicy*Value
```

Property и string-values для выбора socket reorder gap policy.

```text
sourcePartialUnitPolicyPropertyId
sourcePartialUnitPolicy*Value
```

Property и значения для partial unit behavior:

```text
emit_with_flag
drop
```

```text
sourceRuntimeStatusPropertyId
sourceRuntimeDebugCountersPropertyId
sourceRefreshDebugCountersButtonPropertyId
sourceStartReceiveButtonPropertyId
sourceStopReceiveButtonPropertyId
```

Идентификаторы runtime/status/debug UI элементов и кнопок управления receive lifecycle.

`plugin_api.hpp` нужен, чтобы все OBS callbacks, source properties и settings parsing использовали одни и те же стабильные string IDs, без дублирования строк по разным `.cpp` файлам.

#### `plugin-main.cpp`

Содержит OBS module entry point.

Это файл, через который OBS видит plugin и регистрирует новый source type:

```text
OBS loads plugin module
→ obs_module_load()
→ create_st2110_source_info()
→ obs_register_source(...)
→ ST 2110 Source становится доступен в OBS
```

---

##### OBS module declaration

```cpp
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("st2110_obs", "en-US")
```

Регистрирует файл как OBS module и задает default locale domain.

`"st2110_obs"` здесь должен соответствовать plugin/module id.

---

##### Module metadata

```cpp
MODULE_EXPORT const char *obs_module_name() {
    return obs_st2110::pluginName;
}

MODULE_EXPORT const char *obs_module_description() {
    return obs_st2110::pluginDescription;
}
```

OBS вызывает эти функции, чтобы получить отображаемое имя и описание plugin-а.

Значения берутся из `plugin_api.hpp`, чтобы не дублировать строки:

```text
pluginName
pluginDescription
```

---

##### Source registration

```cpp
bool obs_module_load() {
    st2110_source_info = create_st2110_source_info();
    obs_register_source(&st2110_source_info);
    return true;
}
```

Главная логика файла.

Pipeline:

```text
create_st2110_source_info()
→ заполнить obs_source_info callbacks
→ obs_register_source(...)
→ зарегистрировать ST 2110 Source в OBS
```

`plugin-main.cpp` сам не описывает поведение source-а. Он только вызывает factory из `st2110-source.hpp/.cpp`.

---

##### Unload

```cpp
void obs_module_unload() {}
```

Сейчас unload hook пустой, так как все per-source ресурсы освобождаются через OBS source destroy callback, а глобальных plugin-level ресурсов нет.

---

#### `st2110-source.hpp` / `st2110-source.cpp`

Содержит регистрацию и OBS-callbacks для source type `ST 2110 Source`.

Этот файл связывает OBS source lifecycle/UI/settings с project runtime:

```text
OBS source instance
→ St2110Source
→ SourceRuntime
→ discovery / SDP / backend construction / frame delivery
```

Файл читает OBS settings, строит `SourceConfig`, управляет кнопками UI и передает изменения в `SourceRuntime`.

---

##### `create_st2110_source_info`

```cpp
obs_source_info create_st2110_source_info();
```

Функция создает `obs_source_info`, который регистрируется в `plugin-main.cpp`.

Основные параметры:

```cpp
info.id = obs_st2110::sourceId;
info.type = OBS_SOURCE_TYPE_INPUT;
info.output_flags = OBS_SOURCE_ASYNC | OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
```

То есть source объявляется как async input source с video и audio output.

Дальше в `obs_source_info` назначаются callbacks:

```text
create / destroy / update
get_properties / get_defaults
get_width / get_height
activate / deactivate / show / hide
```

---

##### Source instance state

```cpp
struct St2110Source {
    std::unique_ptr<obs_st2110::SourceRuntime> runtime;
};
```

Каждый OBS source instance владеет своим `SourceRuntime`.

---

##### Create / update / destroy

```cpp
void *st2110_source_create(obs_data_t *settings, obs_source_t *source)
```

Создает `St2110Source`, внутри создает `SourceRuntime(source)` и сразу применяет настройки:

```cpp
ctx->runtime->update(read_source_config(settings));
```

```cpp
void st2110_source_update(void *data, obs_data_t *settings)
```

Повторно читает OBS settings и передает новый `SourceConfig` в runtime.

```cpp
void st2110_source_destroy(void *data)
```

Удаляет `St2110Source`, а значит уничтожает `SourceRuntime`. Cleanup receive graph/backend должен происходить внутри runtime/destructors.

---

##### Settings parsing

Главная функция чтения настроек:

```cpp
read_source_config(obs_data_t *settings)
```

Она собирает:

```text
selected_source
receive_settings.backend_kind
receive_settings.reorder_tolerance_policy
receive_settings.partial_unit_policy
```

Выбранный source берется из OBS setting:

```cpp
sourceSelectionPropertyId
```

и резолвится через discovery provider:

```cpp
discovery_provider().resolve_source(selection_key)
```

Backend читается из:

```text
st2110_receive_backend:
    "socket" → ReceiveBackendKind::Socket
    "mtl"    → ReceiveBackendKind::Mtl
```

Reorder policy и partial unit policy читаются как строковые значения из UI и преобразуются в project enums.

---

##### Properties UI

```cpp
obs_properties_t *st2110_source_get_properties(void *data)
```

Создает UI source settings.

Основные элементы:

```text
Source selection dropdown
Runtime status text
Runtime debug counters text
Refresh debug counters button
Start receive button
Stop receive button
Receive backend dropdown
Receive reorder gap policy dropdown
Partial unit policy dropdown
```

Список source-ов (NDI источников) заполняется из discovery provider:

```cpp
for (const auto &item : discovery_provider().list_sources()) {
    obs_property_list_add_string(source_list, item.display_name.c_str(), item.selection_key.c_str());
}
```

То есть OBS UI сейчас получает обнаруженные ST 2110 sources через plugin-side discovery abstraction.

---

##### Runtime status/debug UI

```cpp
runtime_status_text(ctx)
runtime_debug_text(ctx)
```

Status показывает состояние runtime:

```text
Receive graph is running.
Receive graph is configured but stopped.
Receive graph is stopped or idle.
```

Если есть `last_error`, он добавляется к status text.

Debug counters берутся из:

```cpp
ctx->runtime->debug_status()
```

Кнопка `Refresh debug counters` просто возвращает `true`, чтобы OBS перестроил properties UI и заново прочитал counters.

---

##### Start / Stop buttons

```cpp
st2110_source_start_receive_clicked(...)
→ ctx->runtime->start_receive()

st2110_source_stop_receive_clicked(...)
→ ctx->runtime->stop_receive()
```

Receive lifecycle сейчас запускается вручную кнопками UI.

OBS callbacks `activate`, `deactivate`, `show`, `hide` пока пустые. То есть source activation в OBS сам по себе не стартует receive graph.

---

##### Backend UI

В backend dropdown всегда добавляется Socket:

```cpp
obs_property_list_add_string(backend_list, "Socket", "socket");
```

MTL добавляется только если включен compile-time флаг:

```cpp
#if ST2110_HAS_MTL_BACKEND
    obs_property_list_add_string(backend_list, "MTL", "mtl");
#endif
```

---

##### Defaults

```cpp
st2110_source_get_defaults(obs_data_t *settings)
```

Задает default settings:

```text
selected source: ""
backend: socket
reorder gap policy: wait_for_missing
partial unit policy: emit_with_flag
```

---

##### Width / height

```cpp
st2110_source_get_width(void *data)
st2110_source_get_height(void *data)
```

OBS получает текущие dimensions из runtime:

```cpp
ctx->runtime->width()
ctx->runtime->height()
```

Если source/runtime еще нет, возвращается `0`.

---

#### `source_config.hpp`

Содержит plugin-side configuration model для одного OBS `ST 2110 Source`.

Этот файл описывает данные, которые OBS UI/settings передают в `SourceRuntime`:

```text
OBS settings
→ SourceConfig
→ SourceRuntime::update(...)
```

---

##### `ProviderSdpObject`

```cpp
struct ProviderSdpObject {
    std::string provider_object_id;
    std::string display_name;
    std::string raw_sdp;
    std::optional<st2110::SdpMediaKind> declared_media_kind;
};
```

Описывает один SDP object, полученный от discovery provider-а.

Поля:

```text
provider_object_id:
    provider-local id SDP объекта

display_name:
    имя для UI/debug

raw_sdp:
    исходный SDP text

declared_media_kind:
    media kind, если provider уже сообщил video/audio
```

Если `declared_media_kind` отсутствует, дальше pipeline должен классифицировать SDP по media sections перед вызовом media-specific parser-а.

---

##### `SelectedDiscoveredSource`

```cpp
struct SelectedDiscoveredSource {
    std::string provider_id;
    std::string source_id;
    std::string display_name;
    std::vector<ProviderSdpObject> sdp_objects;
};
```

Описывает выбранный пользователем discovered source.

Один source может содержать несколько SDP objects, например отдельно video/audio или primary/redundant descriptions — точная структура зависит от discovery provider-а.

---

##### `SourceConfig`

```cpp
struct SourceConfig {
    std::optional<SelectedDiscoveredSource> selected_source;
    st2110::Settings receive_settings;
};
```

Это итоговая конфигурация source instance-а.

Содержит:

```text
selected_source:
    выбранный discovered source, если пользователь что-то выбрал

receive_settings:
    backend kind, reorder policy, partial unit policy
```

`SourceRuntime::update(...)` сравнивает этот config с предыдущим и решает, нужно ли пересобрать receive graph.

---

#### `discovery_provider.hpp` / `discovery_provider.cpp`

Содержит plugin-side discovery abstraction для получения списка доступных ST 2110 sources и извлечения SDP из provider metadata.

---

##### `IDiscoveryProvider`

```cpp
class IDiscoveryProvider {
  public:
    virtual std::vector<DiscoveredSourceListItem> list_sources() = 0;
    virtual std::optional<SelectedDiscoveredSource> resolve_source(std::string_view selection_key) = 0;
};
```

Это provider-neutral interface.

Он скрывает конкретный discovery mechanism от OBS source code.

`st2110-source.cpp` работает только с:

```text
list_sources()
resolve_source(...)
```

и не знает, используется ли NDI, null provider или другой discovery backend.

---

##### `DiscoveredSourceListItem`

```cpp
struct DiscoveredSourceListItem {
    std::string selection_key;
    std::string display_name;
};
```

Минимальная модель для OBS dropdown.

```text
selection_key:
    stable key, который сохраняется в OBS settings

display_name:
    строка, которую видит пользователь
```

---

##### Provider factory

```cpp
std::unique_ptr<IDiscoveryProvider> create_discovery_provider();
```

Возвращает конкретный provider в зависимости от compile-time flag:

```text
ST2110_HAS_NDI_DISCOVERY = 1
→ NdiDiscoveryProvider

ST2110_HAS_NDI_DISCOVERY = 0
→ NullDiscoveryProvider
```

Fallback provider:

```text
list_sources() → empty list
resolve_source(...) → nullopt
```

То есть без NDI discovery plugin остается собираемым, но не создает fake SDP/debug input.

---

##### NDI discovery path

При включенном `ST2110_HAS_NDI_DISCOVERY` используется `NdiDiscoveryProvider`.

Он делает две операции.

###### `list_sources()`

```text
NDI find_create
→ wait for sources
→ get current NDI sources
→ return dropdown list
```

Каждый NDI source добавляется как:

```text
selection_key = p_ndi_name
display_name  = p_ndi_name
```

Результат сортируется по `display_name`.

---

###### `resolve_source(selection_key)`

```text
selection_key
→ create NDI metadata-only receiver
→ capture metadata frames up to 3 seconds
→ extract ST 2110 SDP objects from metadata XML
→ SelectedDiscoveredSource
```

NDI receiver создается в metadata-only режиме:

```cpp
recv_desc.bandwidth = NDIlib_recv_bandwidth_metadata_only;
```

То есть этот provider использует NDI только как discovery/metadata transport для SDP.

---

##### SDP extraction from NDI metadata

Из metadata ищутся теги:

```text
<st2110_sdp>
<st2110_sdp_video>
<st2110_sdp_audio>
```

Результат превращается в `ProviderSdpObject`:

```text
provider_object_id:
    XML attribute id или fallback "sdp_N"

display_name:
    XML attribute name или fallback "<NDI source> SDP N"

raw_sdp:
    text внутри tag-а, с unwrap CDATA

declared_media_kind:
    из forced tag kind:
        st2110_sdp_video → Video
        st2110_sdp_audio → Audio

    или из attribute media="video"/"audio" для st2110_sdp
```

Если provider не указал media kind, дальше `SourceRuntime`/SDP selection layer должен классифицировать raw SDP сам.

---

##### XML parsing model

Metadata extraction здесь lightweight, не полноценный XML parser.

Он ищет opening/closing tags строковыми операциями, читает простые attributes и unwrap-ит CDATA:

```text
trim
unwrap <![CDATA[ ... ]]>
read id/name/media attributes
```

---

##### Dynamic NDI loading

На Linux `NdiLibrary` пытается загрузить NDI runtime динамически:

```text
NDI_RUNTIME_DIR_V6/libndi.so.6
NDI_RUNTIME_DIR_V6/libndi.so
NDILIB_REDIST_FOLDER/libndi.so.6
NDILIB_REDIST_FOLDER/libndi.so
libndi.so.6
libndi.so
/usr/lib/...
/usr/local/lib/...
```

После `dlopen` ищется:

```text
NDIlib_v6_load
```

Затем вызывается:

```text
ndi->initialize()
```

Если runtime не найден или initialize failed, provider фактически недоступен и возвращает пустые результаты.

На не-Linux платформах dynamic loading сейчас не реализован.

---

#### `sdp_media_selection.hpp` / `sdp_media_selection.cpp`

Содержит plugin-side слой выбора media-specific SDP objects из выбранного discovered source-а.

Задача файла — определить, какой raw SDP object относится к video, а какой к audio, чтобы дальше вызвать правильный parser.

---

##### Назначение

Discovery provider возвращает source как набор SDP objects:

```cpp
std::vector<ProviderSdpObject> sdp_objects;
```

Каждый object может уже иметь provider-declared media kind:

```cpp
std::optional<st2110::SdpMediaKind> declared_media_kind;
```

Если provider указал kind, этот слой доверяет ему для dispatch-а.

Если kind не указан, выполняется lightweight classification:

```cpp
st2110::classify_sdp_media_kind(sdp_object.raw_sdp);
```

---

##### `ClassifiedProviderSdpObject`

```cpp
struct ClassifiedProviderSdpObject {
    ProviderSdpObject provider_object;
    st2110::SdpMediaKind media_kind;
};
```

Это исходный provider SDP object плюс уже resolved media kind.

---

##### `SelectedSourceMediaSet`

```cpp
struct SelectedSourceMediaSet {
    std::optional<ClassifiedProviderSdpObject> video;
    std::optional<ClassifiedProviderSdpObject> audio;
};
```

Это результат media selection для одного selected source-а.

Текущий контракт:

```text
0 или 1 video SDP object
0 или 1 audio SDP object
```

Если source содержит два video SDP object-а или два audio SDP object-а, функция возвращает ошибку:

```cpp
Error::Unsupported
```

redundant RTP topology должен быть описан внутри одного SDP object-а, а не как два независимых SDP object-а одного media kind.

---

##### `resolve_selected_source_media_set`

```cpp
std::expected<SelectedSourceMediaSet, st2110::Error>
resolve_selected_source_media_set(const SelectedDiscoveredSource &selected_source);
```

Основная функция файла.

Порядок работы:

```text
1. Проверить, что selected_source.sdp_objects не пустой
2. Для каждого ProviderSdpObject:
   - проверить raw_sdp
   - получить media kind:
       provider-declared или lightweight classification
   - добавить object в video/audio slot
3. Если media set пустой → InvalidValue
4. Вернуть SelectedSourceMediaSet
```

---

#### `sdp_parser_dispatch.hpp` / `sdp_parser_dispatch.cpp`

Содержит plugin-side dispatch слой, который передает уже классифицированные SDP objects в core media-specific SDP parsers.

Этот файл вызывает правильный parser доверяя определенным ранее media-kind.

---

##### `ParsedSelectedSourceStreams`

```cpp
struct ParsedSelectedSourceStreams {
    std::optional<st2110::ParsedSdpStreamSet> video;
    std::optional<st2110::ParsedSdpStreamSet> audio;
};
```

Это результат parsing-а выбранного source-а.

Он может содержать:

```text
video only
audio only
video + audio
```

Каждое поле — это уже typed core-level `ParsedSdpStreamSet`, полученный из media-specific parser-а.

---

##### `parse_selected_source_streams`

```cpp
std::expected<ParsedSelectedSourceStreams, st2110::Error>
parse_selected_source_streams(const SelectedSourceMediaSet &media_set);
```

Основная функция файла.

Порядок работы:

```text
1. Проверить, что media_set не пустой

2. Если есть media_set.video:
   - вызвать parse_video_stream_signaling(raw_sdp)
   - сохранить результат в out.video

3. Если есть media_set.audio:
   - вызвать parse_audio_stream_signaling(raw_sdp)
   - сохранить результат в out.audio

4. Если результат пустой → InvalidValue

5. Вернуть ParsedSelectedSourceStreams
```

---

#### `synchronized_playout_tuning.hpp`

Содержит расчет параметров synchronized playout для общего video/audio sink-а.

Этот файл вычисляет три числа по уже parsed receive bootstrap-ам:

```cpp
struct SynchronizedPlayoutTuning {
    TimestampNs playout_delay_ns;
    std::size_t max_queued_video_frames;
    std::size_t max_queued_audio_blocks;
};
```

Эти параметры дальше используются `SynchronizedFrameSink`, чтобы выравнивать video/audio delivery по RTP timestamp-derived времени и не отдавать media в OBS сразу при получении.

---

##### Что именно вычисляется

Файл вычисляет:

```text
playout_delay_ns:
    на сколько задерживать выдачу media после receive timestamp baseline

max_queued_video_frames:
    сколько video frames можно держать в очереди sink-а

max_queued_audio_blocks:
    сколько audio blocks можно держать в очереди sink-а
```

Расчет основан не на runtime statistics, а на signal model:

```text
video fps
audio packet_time_us
наличие video/audio streams
```

---

##### Video frame period

```cpp
synchronized_playout_video_frame_period_ns(media)
```

Вычисляет длительность одного video frame:

```text
frame_period_ns = ceil(1_000_000_000 * fps_den / fps_num)
```

Например:

```text
59.94 fps = 60000/1001

frame_period_ns =
ceil(1_000_000_000 * 1001 / 60000)
≈ 16_683_334 ns
≈ 16.68 ms
```

Если `fps_num == 0` или `fps_den == 0`, функция возвращает `0`, потому что frame period вывести нельзя.

---

##### Audio packet time

```cpp
synchronized_playout_audio_packet_time_ns(media)
```

Вычисляет duration одного audio block-а из SDP-derived `packet_time_us`:

```text
packet_time_ns = packet_time_us * 1000
```

Например:

```text
packet_time_us = 1000
→ packet_time_ns = 1_000_000 ns
→ 1 ms
```

---

##### Video playout delay

```cpp
derive_video_playout_delay_ns(video_bootstrap)
```

Если video stream отсутствует:

```text
video_delay = 0
```

Если video есть и frame period известен:

```text
video_delay = one_frame_period + 5 ms jitter margin
```

Константа:

```cpp
synchronizedPlayoutVideoJitterMarginNs = 5_000_000
```

Пример для 59.94 fps:

```text
frame_period ≈ 16.68 ms
video_delay ≈ 16.68 ms + 5 ms
video_delay ≈ 21.68 ms
```

Если frame period вывести нельзя:

```text
video_delay = 5 ms
```

Смысл: video sink получает минимум один frame interval запаса плюс небольшой jitter margin.

---

##### Audio playout delay

```cpp
derive_audio_playout_delay_ns(audio_bootstrap)
```

Если audio stream отсутствует:

```text
audio_delay = 0
```

Если audio есть и packet time известен:

```text
audio_delay = max(20 ms, packet_time * 20)
```

Константы:

```cpp
synchronizedPlayoutAudioMinimumDelayNs = 20_000_000
synchronizedPlayoutAudioPacketTimeMultiplier = 20
```

Пример для ST 2110-30 1 ms audio packet time:

```text
packet_time = 1 ms
packet_time * 20 = 20 ms
audio_delay = 20 ms
```

Пример для 125 us packet time:

```text
packet_time = 0.125 ms
packet_time * 20 = 2.5 ms
audio_delay = max(20 ms, 2.5 ms)
audio_delay = 20 ms
```

Если packet time вывести нельзя:

```text
audio_delay = 20 ms
```

---

##### Итоговый playout delay

```cpp
derive_synchronized_playout_tuning(...)
```

Сначала отдельно считается video delay и audio delay:

```text
video_delay_ns = derive_video_playout_delay_ns(video)
audio_delay_ns = derive_audio_playout_delay_ns(audio)
```

Затем итоговая задержка берется как максимум:

```text
playout_delay_ns = max(video_delay_ns, audio_delay_ns)
```

Смысл: если source содержит и video, и audio, общий synchronized sink должен выбрать задержку, достаточную для обоих потоков.

Например:

```text
video 59.94 fps:
    video_delay ≈ 21.68 ms

audio 1 ms packet time:
    audio_delay = 20 ms

result:
    playout_delay ≈ 21.68 ms
```

Если source audio-only:

```text
video_delay = 0
audio_delay = 20 ms
playout_delay = 20 ms
```

Если source video-only:

```text
playout_delay = video_delay
```

---

##### Video queue size

```cpp
derive_max_queued_video_frames(video_bootstrap, playout_delay_ns)
```

Если video отсутствует:

```text
max_queued_video_frames = 0
```

Если video есть и frame period известен:

```text
delay_frames = ceil(playout_delay_ns / frame_period_ns)

max_queued_video_frames =
    max(4, delay_frames + 2)
```

Константы:

```cpp
synchronizedPlayoutMinimumVideoQueueFrames = 4
synchronizedPlayoutVideoQueueMarginFrames = 2
```

Пример для 59.94 fps и `playout_delay ≈ 21.68 ms`:

```text
frame_period ≈ 16.68 ms
delay_frames = ceil(21.68 / 16.68) = 2

max_queued_video_frames = max(4, 2 + 2) = 4
```

То есть очередь video должна вместить playout delay window плюс небольшой запас.

---

##### Audio queue size

```cpp
derive_max_queued_audio_blocks(audio_bootstrap, playout_delay_ns)
```

Если audio отсутствует:

```text
max_queued_audio_blocks = 0
```

Если audio есть и packet time известен:

```text
delay_blocks = ceil(playout_delay_ns / packet_time_ns)

max_queued_audio_blocks =
    max(32, delay_blocks + 8)
```

Константы:

```cpp
synchronizedPlayoutMinimumAudioQueueBlocks = 32
synchronizedPlayoutAudioQueueMarginBlocks = 8
```

Пример для 1 ms audio packet time и `playout_delay ≈ 21.68 ms`:

```text
delay_blocks = ceil(21.68 / 1) = 22

max_queued_audio_blocks = max(32, 22 + 8) = 32
```

Для большего playout delay очередь audio будет расти пропорционально количеству audio blocks, которые помещаются в delay window.

---

##### Общий расчет

Главная функция:

```cpp
derive_synchronized_playout_tuning(video_bootstrap, audio_bootstrap)
```

Порядок:

```text
1. Посчитать video_delay_ns
2. Посчитать audio_delay_ns
3. playout_delay_ns = max(video_delay_ns, audio_delay_ns)
4. max_queued_video_frames = derive by playout_delay_ns
5. max_queued_audio_blocks = derive by playout_delay_ns
6. Вернуть SynchronizedPlayoutTuning
```

Итоговый результат используется как configuration для synchronized sink-а:

```text
playout_delay_ns:
    когда media можно отдавать в OBS

max_queued_video_frames:
    сколько video frames можно накопить до drop/backpressure policy

max_queued_audio_blocks:
    сколько audio blocks можно накопить до drop/backpressure policy
```

---

#### `synchronized_frame_sink.hpp`

Содержит общий synchronized delivery sink для video/audio frames перед отправкой в конкретный OBS adapter.

`SynchronizedFrameSink` реализует timing/queue/playout логику, а конкретная доставка в OBS задается через virtual methods.

---

##### Назначение

`SynchronizedFrameSink` принимает video frames и audio blocks от backend-а через общий интерфейс:

```cpp
class SynchronizedFrameSink : public IFrameSink
```

Backend вызывает:

```cpp
on_video_frame(VideoFrame frame, FrameTimingMetadata timing_metadata)
on_audio_frame(AudioBuffer frame, FrameTimingMetadata timing_metadata)
```

Sink не отправляет их сразу. Он:

```text
1. Мапит RTP timestamp в media timestamp ns
2. Кладет frame/block в отсортированную очередь
3. Держит общий playout anchor
4. Отдельным playout thread-ом выдает media в правильное local playout time
```

---

##### Config

```cpp
struct SynchronizedFrameSinkConfig {
    bool enable_video;
    bool enable_audio;

    RtpTimestampMapperConfig video_timestamp_mapper;
    RtpTimestampMapperConfig audio_timestamp_mapper;

    TimestampNs playout_delay_ns;

    std::size_t max_queued_video_frames;
    std::size_t max_queued_audio_blocks;
};
```

Смысл полей:

```text
enable_video / enable_audio:
    какие media types реально принимать

video_timestamp_mapper / audio_timestamp_mapper:
    параметры преобразования RTP timestamp → media timestamp ns

playout_delay_ns:
    начальная задержка перед выдачей первого media item

max_queued_video_frames:
    ограничение video queue

max_queued_audio_blocks:
    ограничение audio queue
```

Этот config строится из parsed video/audio bootstrap-ов и `derive_synchronized_playout_tuning(...)`.

---

##### Stats

```cpp
struct SynchronizedFrameSinkStats {
    video_frames_accepted
    audio_blocks_accepted

    video_frames_delivered
    audio_blocks_delivered

    video_frames_dropped
    audio_blocks_dropped

    timestamp_mapping_errors
    output_errors
};
```

Counters разделены по этапам:

```text
accepted:
    frame/block успешно принят в очередь

delivered:
    frame/block реально передан в OBS-specific delivery method

dropped:
    frame/block отброшен из-за disabled/running/output/queue conditions

timestamp_mapping_errors:
    RTP timestamp не удалось преобразовать в media timestamp

output_errors:
    exception при вызове OBS-specific delivery method
```

---

##### Start / stop lifecycle

```cpp
void start();
void stop() noexcept;
```

`start()`:

```text
1. Проверяет, что sink еще не running
2. Сбрасывает stop flag и output_error
3. Очищает video/audio queues
4. Сбрасывает playout anchor
5. Запускает playout_thread_
```

`stop()`:

```text
1. Ставит stop_requested_
2. Ставит running_ = false
3. Будит condition variable
4. Останавливает jthread
5. Очищает очереди
6. Сбрасывает playout anchor
```

Destructor вызывает `stop()`, поэтому sink сам завершает playout thread при уничтожении.

---

##### Video input path

```cpp
void on_video_frame(VideoFrame frame, FrameTimingMetadata timing_metadata)
```

Порядок:

```text
1. Если video disabled → ignore
2. Под lock:
   - если sink не running или есть output_error → drop
   - map RTP timestamp через video_mapper_
   - если mapping failed → timestamp_mapping_errors + drop
   - если playout anchor еще нет → создать
   - вставить frame в video_queue_ по media timestamp
   - обрезать очередь до max_queued_video_frames
   - увеличить video_frames_accepted
3. Разбудить playout thread
```

Важно: video queue сортируется по `media_timestamp_ns`, а не по порядку прихода.

---

##### Audio input path

```cpp
void on_audio_frame(AudioBuffer frame, FrameTimingMetadata timing_metadata)
```

Audio path аналогичен video path:

```text
1. Если audio disabled → ignore
2. RTP timestamp → audio media timestamp ns
3. Создать anchor, если его еще нет
4. Вставить block в audio_queue_ по media timestamp
5. Обрезать очередь до max_queued_audio_blocks
6. Разбудить playout thread
```

Audio и video используют разные `RtpTimestampMapper`, потому что RTP clock rate у video и audio может отличаться.

---

##### RTP timestamp mapping

Входной backend передает `FrameTimingMetadata`:

```cpp
struct FrameTimingMetadata {
    std::uint32_t rtp_timestamp;
    TimestampNs receive_timestamp_ns;
    VideoScanMode video_scan_mode;
    bool video_second_field;
};
```

`SynchronizedFrameSink` использует здесь прежде всего:

```text
rtp_timestamp
```

Он преобразует RTP timestamp в monotonic media time:

```text
RTP timestamp
→ RtpTimestampMapper
→ media_timestamp_ns
```

Этот `media_timestamp_ns` используется для сортировки и playout scheduling.

---

##### Playout anchor

```cpp
struct PlayoutAnchor {
    TimestampNs media_timestamp_ns;
    Clock::time_point local_playout_time;
};
```

Anchor создается на первом принятом video/audio item-е:

```text
anchor.media_timestamp_ns = first media timestamp
anchor.local_playout_time = now + playout_delay_ns
```

После этого для любого следующего media timestamp:

```text
playout_time =
    anchor.local_playout_time
    + (media_timestamp_ns - anchor.media_timestamp_ns)
```

То есть sink переносит media timeline на local steady_clock timeline с фиксированной начальной задержкой.

---

##### Queue trimming

Video:

```cpp
trim_video_queue_locked()
```

Если `video_queue_.size() > max_queued_video_frames`, удаляется самый ранний frame:

```text
pop_front()
++video_frames_dropped
```

Audio аналогично:

```text
pop_front()
++audio_blocks_dropped
```

То есть overflow policy сейчас drop oldest.

---

##### Playout loop

```cpp
run_playout_loop(std::stop_token stop_token)
```

Отдельный thread постоянно выбирает ближайший pending item:

```text
если обе очереди пустые:
    ждать

если есть только video:
    pending = video.front()

если есть только audio:
    pending = audio.front()

если есть оба:
    pending = item с меньшим media_timestamp_ns
```

Затем вычисляет deadline:

```text
deadline = playout_time_for(media_timestamp_ns)
```

Если deadline еще не наступил — ждет до него.

Если deadline наступил — извлекает item из очереди и вызывает delivery method вне lock-а:

```cpp
deliver_video_frame_to_obs(...)
```

или

```cpp
deliver_audio_block_to_obs(...)
```

После successful delivery увеличивает соответствующий delivered counter.

---

##### Delivery methods

```cpp
virtual void deliver_video_frame_to_obs(
    VideoFrame &&frame,
    FrameTimingMetadata timing,
    TimestampNs media_timestamp_ns
);

virtual void deliver_audio_block_to_obs(
    AudioBuffer &&block,
    FrameTimingMetadata timing,
    TimestampNs media_timestamp_ns
);
```

В базовом классе эти методы бросают `std::logic_error`.

Их должен реализовать subclass, например OBS-specific adapter:

```text
ObsSynchronizedFrameSink
→ override deliver_video_frame_to_obs
→ override deliver_audio_block_to_obs
```

---

##### Output error behavior

Если delivery method бросает exception:

```text
catch (...)
→ store_output_exception_and_stop()
```

Дальше sink:

```text
1. сохраняет exception_ptr в output_error_
2. увеличивает output_errors
3. stop_requested_ = true
4. running_ = false
5. будит cv_
```

После этого новые incoming frames/blocks будут dropped, потому что `output_error_` уже выставлен.

---

#### `obs-synchronized-frame-sink.hpp` / `obs-synchronized-frame-sink.cpp`

Содержит OBS-specific adapter поверх generic `SynchronizedFrameSink`.

Это конец receive pipeline.

`SynchronizedFrameSink` отвечает за timestamp mapping, очереди и playout scheduling.  
`ObsSynchronizedFrameSink` отвечает только за преобразование project `VideoFrame` / `AudioBuffer` в OBS structures.

---

##### Назначение

```cpp
class ObsSynchronizedFrameSink final : public st2110::SynchronizedFrameSink
```

Класс наследуется от `SynchronizedFrameSink` и реализует два OBS-specific hook-а:

```cpp
deliver_video_frame_to_obs(...)
deliver_audio_block_to_obs(...)
```

То есть базовый sink решает **когда** отдавать media, а этот класс решает **как именно** передать media в OBS API.

---

##### OBS source ownership

```cpp
obs_source_t *source_ = nullptr;
```

`ObsSynchronizedFrameSink` хранит raw pointer на OBS source.

Он не владеет `obs_source_t`. Владение остается у OBS/source lifecycle.

---

##### Video delivery path

```cpp
deliver_video_frame_to_obs(VideoFrame &&frame, FrameTimingMetadata timing, TimestampNs media_timestamp_ns)
```

Порядок:

```text
1. Проверить source_
2. Сконвертировать project PixelFormat в OBS video_format
3. Заполнить obs_source_frame
4. Передать pointers/strides по plane-ам
5. Заполнить color/range fields
6. Вызвать obs_source_output_video(source_, &obs_frame)
```

`timing` сейчас не используется:

```cpp
(void)timing;
```

OBS timestamp берется из synchronized media timestamp:

```cpp
obs_frame.timestamp = media_timestamp_ns;
```

То есть downstream OBS получает уже timestamp, рассчитанный generic synchronized sink-ом, а не raw receive timestamp.

---

##### Поддерживаемые OBS video formats

```cpp
map_video_format_to_obs(...)
```

Сейчас в OBS напрямую проецируются только эти project formats:

```text
PixelFormat::UYVY          → VIDEO_FORMAT_UYVY
PixelFormat::BGRA          → VIDEO_FORMAT_BGRA
PixelFormat::V210          → VIDEO_FORMAT_V210
PixelFormat::YUV420PLANAR8 → VIDEO_FORMAT_I420
PixelFormat::YUV422PLANAR8 → VIDEO_FORMAT_I422
```

Все остальные форматы возвращают `std::nullopt`, после чего delivery бросает:

```cpp
throw std::runtime_error("Unsupported OBS video pixel format");
```

---

##### Video plane handoff

Для каждой plane:

```cpp
obs_frame.data[plane] = frame.data(plane);
obs_frame.linesize[plane] = static_cast<std::uint32_t>(stride);
```

Перед этим проверяется, что stride помещается в OBS `linesize`:

```cpp
fits_obs_linesize(stride)
```

Если stride больше `uint32_t::max`, delivery завершается ошибкой.

Важно: adapter не копирует video payload в отдельный OBS-owned buffer. Он передает pointers на данные внутри `VideoFrame`, который живет на время вызова `obs_source_output_video`.

---

##### Color fields

```cpp
fill_obs_video_color_fields(obs_frame)
```

Логика сейчас локальная и упрощенная:

```text
если OBS format не YUV:
    full_range = true

если OBS format YUV:
    full_range = false
    color space/range = VIDEO_CS_709 + VIDEO_RANGE_PARTIAL
```

project `VideoFrame` не несет colorimetry/range metadata. Поэтому OBS adapter задает handoff policy сам, не расширяя core frame contract.

---

##### Audio delivery path

```cpp
deliver_audio_block_to_obs(AudioBuffer &&block, FrameTimingMetadata timing, TimestampNs media_timestamp_ns)
```

Порядок:

```text
1. Проверить source_
2. Заполнить obs_source_audio
3. Передать samples pointer
4. Выставить channel layout
5. Выставить sample format/rate/timestamp
6. Вызвать obs_source_output_audio(source_, &obs_audio)
```

`timing` также сейчас не используется.

OBS timestamp:

```cpp
obs_audio.timestamp = media_timestamp_ns;
```

Audio payload берется из project `AudioBuffer`:

```cpp
obs_audio.data[0] = reinterpret_cast<const std::uint8_t *>(block.samples());
obs_audio.frames = block.samples_per_channel();
obs_audio.format = AUDIO_FORMAT_32BIT;
obs_audio.samples_per_sec = block.sampling_rate_hz();
```

То есть adapter ожидает, что `AudioBuffer` уже содержит decoded `int32_t` samples.

---

##### Audio channel mapping

```cpp
map_audio_channels_to_obs_speakers(...)
```

Поддерживаемые layouts:

```text
1 channel  → SPEAKERS_MONO
2 channels → SPEAKERS_STEREO
3 channels → SPEAKERS_2POINT1
4 channels → SPEAKERS_4POINT0
5 channels → SPEAKERS_4POINT1
6 channels → SPEAKERS_5POINT1
8 channels → SPEAKERS_7POINT1
```

Любое другое количество каналов приводит к exception.

---

#### `source_runtime.hpp` / `source_runtime.cpp`

Содержит главный plugin-side orchestration layer для одного OBS `ST 2110 Source`.

Это центральный файл, который связывает уже описанные части:

```text
OBS SourceConfig
→ discovery-selected SDP
→ media selection
→ SDP parser dispatch
→ receive bootstrap
→ local receive policy
→ Socket/MTL backend construction
→ synchronized OBS sink
→ start/stop/debug lifecycle
```

`SourceRuntime` — это координатор receive graph-а для конкретного OBS source instance.

---

##### Public API

```cpp
class SourceRuntime {
  public:
    explicit SourceRuntime(obs_source_t *source);
    ~SourceRuntime();

    void update(const SourceConfig &config);

    void start_receive();
    void stop_receive() noexcept;

    std::uint32_t width() const noexcept;
    std::uint32_t height() const noexcept;

    bool running() const noexcept;
    bool configured() const noexcept;
    const std::string &last_error() const noexcept;
    std::string debug_status();
};
```

Снаружи `SourceRuntime` используется из `st2110-source.cpp`:

```text
source create/update
→ runtime->update(...)

Start receive button
→ runtime->start_receive()

Stop receive button
→ runtime->stop_receive()

OBS get_width/get_height
→ runtime->width()/height()

OBS properties/debug text
→ runtime->debug_status()
```

---

##### Internal graph model

Внутри runtime есть staging model:

```cpp
struct ConfiguredReceiveGraph {
    std::unique_ptr<ObsSynchronizedFrameSink> sink;

    std::shared_ptr<st2110::MtlWorkerGraphClient> mtl_graph_client;
    std::optional<st2110::MtlRuntimeConfig> mtl_runtime;

    std::unique_ptr<st2110::IRxBackend> video_backend;
    std::unique_ptr<st2110::IRxBackend> audio_backend;

    std::string description;

    std::uint32_t width;
    std::uint32_t height;
};
```

То есть receive graph состоит из:

```text
optional video backend
optional audio backend
shared synchronized sink
optional MTL graph client
metadata/debug description
current video dimensions
```

Socket backend path использует `SocketRxVideoBackend` / `SocketRxAudioBackend`.

MTL backend path использует `MtlWorkerGraphClient` плюс proxy backends:

```text
MtlRxVideoBackendProxy
MtlRxAudioBackendProxy
```

---

##### Update lifecycle

```cpp
void update(SourceConfig config)
```

`update()` сравнивает новый config с текущим:

```cpp
next.selected_source != current.selected_source
next.receive_settings != current.receive_settings
```

Если graph-relevant настройки не изменились:

```text
если receive уже requested, но graph почему-то не configured:
    попытаться start_receive_graph()

иначе:
    ничего не пересобирать
```

Если настройки изменились:

```text
1. destroy_configured_graph_noexcept()
2. сохранить новый config
3. если receive_requested_ == true:
       start_receive_graph()
   иначе:
       clear last_error
```

Смысл: изменение source/backend/reorder/partial policy инвалидирует весь configured receive graph.

---

##### Manual receive lifecycle

`start_receive()`:

```text
receive_requested_ = true
start_receive_graph()
```

`stop_receive()`:

```text
receive_requested_ = false
stop_active_sessions_noexcept()
```

Важно: `stop_receive()` останавливает active sessions, но не обязательно уничтожает configured graph. Graph может остаться configured/stopped и быть снова запущен без повторного parsing/build, если config не менялся.

---

##### Build graph pipeline

Главная сборка происходит в:

```cpp
build_configured_graph(const SourceConfig &config)
```

Порядок:

```text
1. Проверить, что есть selected_source с SDP
2. resolve_selected_source_media_set(...)
3. parse_selected_source_streams(...)
4. Для video:
   - project_parsed_video_sdp_to_receive_bootstrap(...)
   - auto_select_receive_local_policy(...)
   - собрать ReceiveStartRequest
   - make_video_backend(...)

5. Для audio:
   - project_parsed_audio_sdp_to_receive_bootstrap(...)
   - auto_select_receive_local_policy(...)
   - собрать ReceiveStartRequest
   - make_audio_backend(...)

6. Если backend = MTL:
   - создать MtlWorkerGraphClient
   - configure_video/configure_audio через proxy construction
   - resolve_configured_mtl_runtime_key(...)

7. Создать ObsSynchronizedFrameSink
   - make_sink_config(video_bootstrap, audio_bootstrap)

8. Вернуть ConfiguredReceiveGraph
```

Это основной plugin-side receive graph construction path.

---

##### Media selection and parsing

Runtime вызывает отдельные слои:

```text
resolve_selected_source_media_set(...)
→ выбрать video/audio SDP objects

parse_selected_source_streams(...)
→ вызвать video/audio SDP parsers
```

После этого runtime получает typed parsed streams:

```text
ParsedSelectedSourceStreams
```

и проектирует их в receive bootstrap:

```text
VideoReceiveBootstrap
AudioReceiveBootstrap
```

---

##### Local policy selection

Для каждого media stream runtime вызывает:

```cpp
auto_select_receive_local_policy(bootstrap.receive_bootstrap)
```

Это превращает receive topology/remote legs в local receive policy:

```text
remote destination/source-filter/topology
→ local IP/interface/BDF policy
```

Если local policy не удалось выбрать, graph construction останавливается с `last_error_`.

---

##### Backend construction

Video backend:

```cpp
make_video_backend(request, settings, mtl_graph_client)
```

Audio backend:

```cpp
make_audio_backend(request, settings, mtl_graph_client)
```

Socket path:

```text
ReceiveStartRequest
→ project_receive_start_request_to_socket_video_start(...)
→ SocketRxVideoBackend

ReceiveStartRequest
→ project_receive_start_request_to_socket_audio_start(...)
→ SocketRxAudioBackend
```

MTL path, если `ST2110_HAS_MTL_BACKEND` включен:

```text
ReceiveStartRequest
→ project_receive_start_request_to_mtl_video_start(...)
→ mtl_graph_client->configure_video(...)
→ MtlRxVideoBackendProxy

ReceiveStartRequest
→ project_receive_start_request_to_mtl_audio_start(...)
→ mtl_graph_client->configure_audio(...)
→ MtlRxAudioBackendProxy
```

Если MTL backend выбран, но plugin собран без MTL:

```text
Error::Unsupported
```

---

##### Sink config

```cpp
make_sink_config(video_bootstrap, audio_bootstrap)
```

Создает `SynchronizedFrameSinkConfig`.

Внутри:

```text
derive_synchronized_playout_tuning(...)
→ playout_delay_ns
→ max_queued_video_frames
→ max_queued_audio_blocks
```

Также настраиваются RTP timestamp mappers:

```text
video rtp_clock_rate
audio rtp_clock_rate
```

Если есть одновременно video и audio, включается общий AV sync reference mode:

```cpp
RtpTimestampInitialAnchorMode::ConfiguredReference
```

Anchor RTP timestamp берется из SDP-derived media clock direct offset:

```text
media_clock.direct->rtp_clock_offset
```

То есть audio/video timestamps мапятся в общую media timeline, если оба stream-а присутствуют.

---

##### Commit graph

После successful staging вызывается:

```cpp
commit_configured_graph(...)
```

Он переносит staged graph в runtime state:

```text
sink_
mtl_graph_client_
configured_mtl_runtime_
video_backend_
audio_backend_
configured_graph_description_
width_
height_
```

До commit-а старый graph уже уничтожен, а новый graph еще не стартован. Это снижает риск частично сконфигурированного active state.

---

##### Start active sessions

```cpp
start_active_sessions()
```

Порядок:

```text
1. Если sessions уже running → true
2. Проверить sink_
3. Проверить, что есть хотя бы один backend
4. sink_->start()
5. start video backend, если есть
6. start audio backend, если есть
7. active_sessions_running_ = true
8. last_error_ = "Receive graph started for ..."
```

Если video backend стартовал, а audio backend потом failed:

```text
cleanup_started_sessions()
→ stop video backend
→ stop audio backend
→ stop sink
→ active_sessions_running_ = false
```

То есть start path старается не оставить частично running graph.

---

##### Stop / destroy

```cpp
stop_active_sessions_noexcept()
```

Останавливает active sessions:

```text
active_sessions_running_ = false
video_backend_->stop()
audio_backend_->stop()
sink_->stop()
```

Для MTL учитывается не только локальный flag, но и состояние graph client:

```cpp
sessions_may_be_running()
→ active_sessions_running_ || mtl_graph_client_running()
```

```cpp
destroy_configured_graph_noexcept()
```

Полностью уничтожает configured graph:

```text
stop_active_sessions_noexcept()
reset video/audio backends
reset MTL graph client/runtime key
reset sink
width = 0
height = 0
clear graph description
active_sessions_running_ = false
```

Destructor `SourceRuntime::Impl` вызывает destroy path.

---

##### Debug status

```cpp
std::string debug_status()
```

Формирует текст для OBS properties UI.

Общие поля:

```text
Receive state
Configured graph
Configured media
Backend
Last status/error
```

Дальше branch по backend:

```text
MTL backend
→ make_mtl_debug_status()

Socket backend
→ make_socket_debug_status()
```

---

##### Socket debug path

```cpp
make_socket_debug_status()
```

Печатает diagnostics по `IRxBackend::stats_snapshot()`:

```text
health
last_error
datagrams_received
bytes_received
control_datagrams_ignored
nonmedia_datagrams_ignored
packets_parsed_ok
packets_rejected
datagrams_dropped
frames_delivered
media_units_delivered
```

Это относится к socket backend path, где проект сам делает UDP receive/RTP parse/reorder/depacketize.

---

##### MTL debug path

При MTL backend runtime обращается к:

```cpp
mtl_graph_client_->stats()
```

Если stats доступны, они форматируются через:

```cpp
format_mtl_worker_stats(...)
```

Выводятся группы:

```text
MTL graph
Video counters
Video session counters
Audio counters
Audio session counters
Shared-memory delivery counters
MTL device port counters
```

Если stats query failed, runtime:

```text
1. записывает last_error_
2. пишет, что counters unavailable
3. destroy_configured_graph_noexcept()
```

Это важно: если stats failure инвалидировал worker/lease, runtime не оставляет stale proxy state.

---

##### Width / height

```cpp
width()
height()
```

Возвращают размеры configured video stream.

Если source audio-only или graph не configured:

```text
width = 0
height = 0
```

Dimensions берутся из `VideoReceiveBootstrap` при graph construction.

---

##### Error handling

Runtime сохраняет user-visible status в:

```cpp
std::string last_error_;
```

Ошибки формируются через:

```text
set_error(message)
set_error(message, Error)
set_backend_error(message, backend, Error)
```

Для MTL proxy backend-а `set_backend_error` дополнительно пытается получить worker-side detail через:

```cpp
backend_error_detail(...)
```

То есть UI может показать не только `Error::SystemFailure`, но и worker-specific message.

---
