#### `apps/st2110_mtl_rx_worker/main.cpp`

Содержит entry point MTL worker process-а.

Этот файл задает внешнюю IPC-границу worker-а:

```text
OBS process
→ worker stdin
→ framed control request
→ worker process state
→ framed control event
→ worker stdout
→ OBS process
```

`main.cpp` читает IPC requests, передает их в `MtlWorkerProcessState` и записывает ответные events через `MtlWorkerEventWriter`.

---

##### IPC streams

Worker использует standard streams как transport boundary:

```text
stdin:
    framed requests from OBS process

stdout:
    framed events/responses to OBS process

stderr:
    diagnostics/logging only
```

`stdout` нельзя использовать для обычных логов, потому что OBS-side `MtlWorkerProcessControlChannel` читает оттуда binary framed IPC messages.

---

##### Startup objects

```c++
st2110_mtl_rx_worker::MtlWorkerEventWriter event_writer{STDOUT_FILENO};
st2110_mtl_rx_worker::MtlWorkerProcessState state{event_writer};
```

На старте создаются два ключевых объекта:

```text
MtlWorkerEventWriter:
    единая worker-side write boundary для events/responses в stdout

MtlWorkerProcessState:
    typed request dispatcher и владелец worker-local state
```

`MtlWorkerProcessState` дальше управляет:

```text
ConfigHandshake
StartSessions
StopSessions
Stats
HealthCheck
Shutdown
```

и внутри себя владеет worker-local runtime/graphs.

---

##### Main loop

```c++
while (!state.shutdown_requested()) {
    ...
}
```

Worker работает, пока `MtlWorkerProcessState` не обработает `ShutdownRequest` и не выставит shutdown flag.

Один цикл обработки выглядит так:

```text
1. read_mtl_worker_control_frame_with_fds(STDIN_FILENO)
2. deserialize_mtl_worker_control_request(frame->payload())
3. state.handle(request, frame->file_descriptors())
4. event_writer.write_event(event)
```

То есть `main.cpp` реализует synchronous command loop:

```text
one request in
→ one immediate control event out
```

Async media-ready events не создаются в `main.cpp`; они должны писаться через тот же `MtlWorkerEventWriter` из receive graph/session layer-а.

---

##### File descriptor handling

```c++
auto frame = st2110::read_mtl_worker_control_frame_with_fds(STDIN_FILENO);
...
state.handle(*request, frame->file_descriptors());
```

Worker читает IPC frame вместе с ancillary file descriptors.

Это нужно для `StartSessionsRequest`, где OBS process передает worker-у shared-memory ring fds через `SCM_RIGHTS`.

Важная граница ownership:

```text
MtlWorkerIpcFrame owns received fds
state.handle(...) receives borrowed span<const int>
```

Значит, worker-side handler должен использовать descriptors синхронно внутри `handle`, например замапить shared-memory rings во время обработки `StartSessions`. Нельзя сохранять raw fd pointers/spans за пределами lifetime текущего `frame`.

---

framing/codec/write failure считается fatal для worker process-а.

---

#### `mtl_worker_event_writer.hpp` / `mtl_worker_event_writer.cpp`

Содержит worker-side event writer — единую точку записи typed events из MTL worker process обратно в OBS/core process.

`MtlWorkerEventWriter` не знает семантику конкретного event-а. Он только сериализует typed event и записывает его в framed IPC stream.

---

##### Назначение

`MtlWorkerEventWriter` используется внутри worker process-а для записи:

```text
synchronous responses:
    HealthEvent
    ErrorEvent
    StartedEvent
    StoppedEvent
    StatsEvent

asynchronous events:
    FrameReadyEvent
    AudioBlockReadyEvent
```

В `main.cpp` он используется для ответа на каждый request:

```text
state.handle(request)
→ MtlWorkerControlEvent
→ event_writer.write_event(event)
```

В session/graph слоях тот же writer может использоваться для async media-ready events:

```text
MTL video/audio session received frame/block
→ wrote shared-memory slot
→ write FrameReadyEvent / AudioBlockReadyEvent
```

---

##### Основной write path

```c++
std::expected<bool, st2110::Error>
write_event(const st2110::MtlWorkerControlEvent &event);
```

`write_event` — обычный путь записи event-а без file descriptors.

Он просто вызывает:

```c++
write_event_with_fds(event, {})
```

---

##### Fd-aware write path

```c++
std::expected<bool, st2110::Error>
write_event_with_fds(
    const st2110::MtlWorkerControlEvent &event,
    std::span<const int> file_descriptors
);
```

Порядок работы:

```text
1. Проверить fd_ >= 0
2. serialize_mtl_worker_control_event(event)
3. Взять write_mutex_
4. write_mtl_worker_control_frame_with_fds(fd_, payload, file_descriptors)
5. Вернуть success/error
```

`file_descriptors` передаются как borrowed descriptors. `MtlWorkerEventWriter` их не закрывает и не забирает ownership.

Сейчас основной worker → OBS поток обычно не должен возвращать fds, но fd-aware method оставлен как полноценная IPC boundary, симметричная control-channel framing layer-у.

---

##### Thread-safety

```c++
std::mutex write_mutex_;
```

Writer защищает запись mutex-ом.

Это важно, потому что events могут писаться из разных worker-side контекстов:

```text
main command loop
video receive/session thread
audio receive/session thread
future health/error path
```

---

#### `mtl_worker_process_state.hpp` / `mtl_worker_process_state.cpp`

Содержит worker-local typed command dispatcher и владельца основного состояния MTL worker process-а.

`MtlWorkerProcessState` уже знает semantics worker protocol-а: `ConfigHandshake`, `StartSessions`, `StopSessions`, `Stats`, `HealthCheck`, `Shutdown`.

---

##### Что хранит `MtlWorkerProcessState`

```c++
MtlWorkerEventWriter *event_writer_;
std::unique_ptr<MtlRuntimeContext> runtime_;
std::unordered_map<MtlWorkerGraphId, std::unique_ptr<MtlReceiveGraph>> graphs_;
bool shutdown_requested_;
```

Worker process владеет:

```text
one optional MTL runtime context
multiple receive graphs keyed by graph_id
health state
shutdown flag
event writer reference
```

Обычный `StopSessions` удаляет только выбранный graph, но **не уничтожает runtime**. Runtime остается живым для reuse тем же worker-ом.

Runtime уничтожается при:

```text
ShutdownRequest
worker process exit
```

---

##### Dispatch boundary

Есть два основных dispatcher overload-а:

```c++
MtlWorkerControlEvent handle(const MtlWorkerControlRequest &request);

MtlWorkerControlEvent handle(
    const MtlWorkerControlRequest &request,
    std::span<const int> ancillary_file_descriptors
);
```

Второй overload используется из `main.cpp`, потому что IPC frame может принести file descriptors через `SCM_RIGHTS`.

Правило по fd:

```text
ancillary file descriptors принимаются только с StartSessionsRequest
```

Если fds пришли с любым другим request type, worker возвращает `ErrorEvent` с `Error::InvalidValue`.

---

##### `ConfigHandshake`

```c++
handle(const MtlWorkerConfigHandshakeRequest &request)
```

Handshake используется manager-ом после spawn worker process-а.

Порядок:

```text
1. Если shutdown уже requested → OperationAborted
2. Проверить worker health
3. Если runtime уже создан:
   - runtime config должен совпадать с request.runtime
   - иначе InvalidBackendState
4. Если runtime еще нет:
   - MtlRuntimeContext::create(request.runtime)
   - сохранить runtime_
5. Вернуть HealthEvent{healthy = true}
```

То есть handshake либо создает worker-owned `MtlRuntimeContext`, либо подтверждает, что уже созданный runtime совместим с запрошенным config-ом.

---

##### `StartSessions`

```c++
handle(
    const MtlWorkerStartSessionsRequest &request,
    std::span<const int> ancillary_file_descriptors
)
```

`StartSessions` создает worker-local receive graph.

Основные проверки:

```text
shutdown не requested
graph_id != 0
есть video или audio session
если есть ancillary fds, должны быть media_rings descriptors
каждый descriptor.fd_index должен ссылаться на существующий fd
graph_id еще не занят
video/audio runtime config совместимы
worker runtime совместим или может быть создан
event_writer_ задан
```

После проверок:

```text
MtlReceiveGraph::create(runtime, request, event_writer, ancillary_fds)
→ graphs_[graph_id] = graph
→ StartedEvent
```

Именно здесь shared-memory fds переходят из IPC-level frame в graph creation boundary. `MtlReceiveGraph::create` должен синхронно использовать переданные descriptors/fds для mapping-а rings.

---

##### Runtime config resolution для StartSessions

`StartSessions` может содержать:

```text
video only
audio only
video + audio
```

Runtime выбирается так:

```text
если есть video:
    runtime = video.runtime
    если есть audio, audio.runtime должен совпадать

иначе:
    runtime = audio.runtime
```

Если video/audio configs содержат разные `MtlRuntimeConfig`, request отклоняется как `InvalidValue`.

---

##### `StopSessions`

```c++
handle(const MtlWorkerStopSessionsRequest &request)
```

Останавливает и удаляет graph по `graph_id`.

Поведение:

```text
если graph не найден:
    вернуть StoppedEvent

если graph найден:
    refresh health before stop
    graph->stop_sessions_noexcept()
    erase graph
    если health до stop уже был bad:
        вернуть ErrorEvent
    иначе:
        вернуть StoppedEvent
```

Важно: `StopSessions` удаляет receive graph и его shared-memory mappings/sessions, но не сбрасывает `runtime_`.

---

##### `Stats`

```c++
handle(const MtlWorkerStatsRequest &request)
```

Порядок:

```text
1. refresh worker health
2. найти graph по graph_id
3. если graph найден — взять graph->stats_snapshot()
4. если graph не найден — вернуть пустой snapshot
5. переложить snapshot fields в MtlWorkerStatsEvent
```

Stats event содержит worker-side graph/session/device counters.

Core-side delivery counters (`frame_ready_events`, `audio_block_ready_events`, `released_slots` и т.п.) здесь не заполняются: они принадлежат OBS/core-side `MtlWorkerGraphClient` shared-memory reader path и добавляются позже на стороне клиента.

---

##### `HealthCheck`

```c++
handle(const MtlWorkerHealthCheckRequest &request)
```

Вызывает:

```c++
refresh_worker_health_noexcept()
```

и возвращает:

```text
HealthEvent{healthy=true, message="MTL worker healthy"}
```

или

```text
HealthEvent{healthy=false, message=worker_health_message_}
```

Health проверяет:

```text
shutdown не requested
если runtime_ есть, runtime_->handle() live
каждый graph non-null
каждый graph->healthy()
```

Если одна проверка провалилась, worker помечается unhealthy один раз через `mark_worker_unhealthy_noexcept`.

---

##### `Shutdown`

```c++
handle(const MtlWorkerShutdownRequest &request)
```

Shutdown полностью завершает worker-local state:

```text
1. stop_sessions_noexcept() для всех graphs
2. graphs_.clear()
3. runtime_.reset()
4. shutdown_requested_ = true
5. вернуть StoppedEvent
```

После этого `main.cpp` увидит:

```c++
state.shutdown_requested() == true
```

и выйдет из main loop.

---

##### Health state model

Worker health хранится в полях:

```c++
bool worker_unhealthy_;
Error worker_health_error_;
std::string worker_health_message_;
```

`mark_worker_unhealthy_noexcept` фиксирует первую health error и дальше не перезаписывает ее.

Это делает diagnostics стабильными: если worker стал unhealthy из-за runtime/graph failure, последующие запросы не затирают исходную причину.

---

#### `mtl_runtime_context.hpp` / `mtl_runtime_context.cpp`

Содержит worker-local wrapper над реальным MTL runtime handle.

Это первый слой в MTL worker pipeline, где используется MTL API:

```text
MtlWorkerProcessState
→ MtlRuntimeContext::create(...)
→ mtl_init(...)
→ mtl_handle
→ MtlReceiveGraph / MtlVideoRxSession / MtlAudioRxSession
```

---

##### Назначение

`MtlRuntimeContext` владеет настоящим MTL runtime:

```c++
mtl_handle mt;
```

и отвечает за lifecycle:

```text
create:
    MtlRuntimeConfig
    → mtl_init_params
    → mtl_init
    → mtl_handle

destroy:
    mtl_uninit
```

То есть это worker-side RAII boundary для `mtl_init` / `mtl_uninit`.

---

##### Runtime config projection

```c++
static std::expected<std::unique_ptr<MtlRuntimeContext>, st2110::Error>
create(st2110::MtlRuntimeConfig cfg);
```

`create` строит `mtl_init_params` из project-owned `MtlRuntimeConfig`.

Projection делает:

```text
primary_port
→ params.port[MTL_PORT_P]
→ params.sip_addr[MTL_PORT_P]

optional redundant_port
→ params.port[MTL_PORT_R]
→ params.sip_addr[MTL_PORT_R]
```

Количество MTL ports:

```text
runtime.redundant_port exists → num_ports = 2
otherwise                     → num_ports = 1
```

---

##### Port projection

Для каждого runtime port-а заполняется:

```c++
params.port[port_index]
params.pmd[port_index]
params.net_proto[port_index]
params.tx_queues_cnt[port_index]
params.rx_queues_cnt[port_index]
params.sip_addr[port_index]
```

Смысл:

```text
port_name:
    PCI BDF или kernel socket port name из MtlRuntimePortConfig

pmd:
    MTL_PMD_DPDK_USER по умолчанию
    MTL_PMD_KERNEL_SOCKET если ST2110_MTL_DEV_KERNEL_SOCKET=1

net_proto:
    MTL_PROTO_STATIC

tx_queues_cnt:
    0, потому что worker сейчас receive-only

rx_queues_cnt:
    1

sip_addr:
    local source IP address выбранного MTL port-а
```

То есть `MtlRuntimeConfig` — это project-side описание выбранного local device/port-а, а `MtlRuntimeContext` превращает его в native `mtl_init_params`.

---

##### Runtime flags

```c++
params.flags |= MTL_FLAG_DEV_AUTO_START_STOP;
```

Worker включает MTL device auto start/stop policy.

Это fixed worker runtime policy. Она не приходит из SDP и не является частью `ReceiveStartRequest`.

---

##### Handle ownership

```c++
struct MtlRuntimeContext::Impl {
    st2110::MtlRuntimeConfig cfg;
    mtl_handle mt = nullptr;

    ~Impl() {
        if (mt) {
            mtl_uninit(mt);
            mt = nullptr;
        }
    }
};
```

`Impl` хранит:

```text
original MtlRuntimeConfig
native mtl_handle
```

Destructor вызывает `mtl_uninit`.

Copy/move у `MtlRuntimeContext` запрещены, поэтому handle имеет одного владельца.

---

##### Native handle accessor

```c++
mtl_handle handle() const noexcept;
```

Возвращает worker-private native MTL handle.

Этот handle нужен ниже по pipeline:

```text
MtlReceiveGraph
→ MtlVideoRxSession
→ MtlAudioRxSession
```

Но он не должен выходить за worker process boundary.

---

##### Config accessor

```c++
const st2110::MtlRuntimeConfig &config() const noexcept;
```

Используется для проверки compatibility:

```text
ConfigHandshake runtime == existing runtime
StartSessions runtime == current worker runtime
```

То есть worker process может переиспользовать уже созданный runtime только для совместимых graph-ов.

---

##### Runtime stats

```c++
void append_stats_snapshot(MtlWorkerGraphStatsSnapshot &snapshot) const noexcept;
```

Добавляет MTL device RX port stats в worker graph snapshot.

---

#### `mtl_receive_graph.hpp` / `mtl_receive_graph.cpp`

Содержит worker-local receive graph.

`MtlReceiveGraph` — это слой, который связывает worker-owned `MtlRuntimeContext`, imported shared-memory rings и concrete video/audio MTL sessions:

```text
MtlWorkerProcessState
→ MtlReceiveGraph::create(...)
→ import shared-memory rings
→ create MtlVideoRxSession / MtlAudioRxSession
→ sessions write slots
→ sessions emit async ready events through MtlWorkerEventWriter
```

Этот объект живет только внутри worker process-а.

---

##### Назначение

`MtlReceiveGraph` представляет один receive graph по `MtlWorkerGraphId`.

Он может содержать:

```text
video session only
audio session only
video + audio sessions
```

Внутри graph хранит:

```c++
MtlReceiveGraphConfig cfg;
MtlRuntimeContext *runtime;
MtlWorkerEventWriter *event_writer;
MtlWorkerGraphStats stats;
std::vector<MtlWorkerSharedMemoryRingMap> media_rings;
std::unique_ptr<MtlVideoRxSession> video;
std::unique_ptr<MtlAudioRxSession> audio;
```

Graph **не владеет** `MtlRuntimeContext`. Runtime принадлежит `MtlWorkerProcessState`.

Graph **владеет**:

```text
imported shared-memory mappings
video session
audio session
graph-local stats
```

---

##### Create pipeline

```c++
static std::expected<std::unique_ptr<MtlReceiveGraph>, Error>
create(
    MtlRuntimeContext &runtime,
    MtlReceiveGraphConfig cfg,
    MtlWorkerEventWriter &event_writer,
    std::span<const int> ancillary_file_descriptors
);
```

`create` выполняет полный graph setup:

```text
1. Resolve runtime config from StartSessions request
2. Проверить, что graph runtime config совпадает с runtime.config()
3. Import shared-memory rings из request.media_rings + ancillary fds
4. Создать Impl
5. Вызвать start_sessions()
6. Вернуть готовый graph
```

То есть после successful `MtlReceiveGraph::create` sessions уже стартованы.

---

##### Runtime compatibility

Graph config — это alias:

```c++
using MtlReceiveGraphConfig = st2110::MtlWorkerStartSessionsRequest;
```

Runtime config извлекается из `StartSessionsRequest` так:

```text
если есть video:
    runtime = video.runtime
    если есть audio, audio.runtime должен совпадать

если video нет:
    runtime = audio.runtime
```

Если video/audio runtime config отличаются, graph creation возвращает:

```c++
Error::InvalidValue
```

Дальше graph проверяет:

```text
runtime.config() == resolved graph runtime
```

Это гарантирует, что graph создается против worker runtime-а, совместимого с request-ом.

---

##### Shared-memory ring import

```c++
import_shared_memory_rings(cfg, ancillary_file_descriptors)
```

Worker получает shared memory от OBS process-а в двух частях:

```text
MtlWorkerSharedMemoryRingDescriptor
    внутри StartSessionsRequest.media_rings

actual fd
    через SCM_RIGHTS ancillary file descriptors
```

Import pipeline:

```text
for each descriptor:
    validate descriptor.fd_index < ancillary_file_descriptors.size()
    map fd with MtlWorkerSharedMemoryRingMap::map_from_descriptor(...)
    validate_initialized_slot_headers()
    store ring map in graph
```

После этого worker-side graph имеет mapped shared-memory rings и может передать нужный ring video/audio session-у.

---

##### Session startup

```c++
std::expected<bool, Error> start_sessions();
```

`start_sessions` создает concrete media sessions.

Порядок:

```text
1. Проверить runtime handle
2. Если sessions уже running → return true
3. Если video config есть:
   - найти unique video ring
   - проверить event_writer
   - MtlVideoRxSession::create(...)
4. Если audio config есть:
   - найти unique audio ring
   - проверить event_writer
   - MtlAudioRxSession::create(...)
5. Commit staged sessions into impl_
6. return true
```

Sessions создаются staged-переменными:

```c++
std::unique_ptr<MtlVideoRxSession> staged_video;
std::unique_ptr<MtlAudioRxSession> staged_audio;
```

Это полезно: если audio creation падает после successful video creation, graph не остается в частично committed состоянии.

---

##### Media ring selection

```c++
find_unique_media_ring(rings, MtlWorkerMediaKind::Video)
find_unique_media_ring(rings, MtlWorkerMediaKind::Audio)
```

Graph выбирает ring по `media_kind`.

Если найдено больше одного ring-а одного типа, возвращается:

```c++
Error::InvalidValue
```

Это защищает от ambiguous mapping:

```text
один video session → один video ring
один audio session → один audio ring
```

---

##### Stop / wake

```c++
void stop_sessions_noexcept() noexcept;
```

Stop делает:

```text
1. wake_block()
2. reset audio session
3. reset video session
```

`wake_block()` вызывает session-level wake:

```c++
video->wake_block();
audio->wake_block();
```

Смысл: если receive thread внутри session заблокирован на MTL get-frame/get-block call, graph пытается его разбудить перед destruction/reset.

Порядок reset-а:

```text
audio.reset()
video.reset()
```

После reset destructors sessions должны остановить свои receive loops и освободить MTL session handles.

---

##### Running state

```c++
bool sessions_running() const noexcept;
```

Возвращает `true`, если есть хотя бы одна active session:

```text
video != nullptr || audio != nullptr
```

Это graph-level running state, а не worker-process-level state.

---

##### Stats snapshot

```c++
MtlWorkerGraphStatsSnapshot stats_snapshot() const noexcept;
```

Stats собираются из нескольких источников:

```text
1. impl_->stats.snapshot()
   graph/session shared counters

2. video->append_stats_snapshot(snapshot)
   video ST20P/session-specific counters

3. audio->append_stats_snapshot(snapshot)
   audio ST30P/session-specific counters

4. runtime->append_stats_snapshot(snapshot)
   MTL device port counters
```

То есть `MtlReceiveGraph` — aggregator worker-side stats для конкретного graph-а.

Эти stats потом попадают в:

```text
MtlWorkerProcessState::handle(StatsRequest)
→ MtlWorkerStatsEvent
→ OBS process
```

---

##### Health

```c++
bool healthy() const noexcept;
Error health_error() const noexcept;
std::string health_message() const;
```

Graph считается healthy, если:

```text
impl_ существует
runtime существует
runtime->handle() live
если video config есть → video session exists and healthy
если audio config есть → audio session exists and healthy
```

Если video/audio config был запрошен, но соответствующая session отсутствует, graph unhealthy.

Health error/message прокидываются из конкретной failed session, если она есть:

```text
video->health_error()
video->health_message()

audio->health_error()
audio->health_message()
```

Если проблема на уровне runtime:

```text
Error::InvalidBackendState
"MTL receive graph has no live runtime handle"
```

---

#### `mtl_video_rx_session.hpp` / `mtl_video_rx_session.cpp`

Содержит worker-local MTL ST20P video receive session.

Это уже concrete MTL video data-plane слой внутри worker process-а:

```text
MtlReceiveGraph
→ MtlVideoRxSession::create(...)
→ st20p_rx_create(...)
→ receive thread
→ st20p_rx_get_frame(...)
→ shared-memory video slot
→ FrameReadyEvent
→ st20p_rx_put_frame(...)
```

`MtlVideoRxSession` владеет `st20p_rx_handle`, но не владеет `mtl_handle`. Runtime handle принадлежит `MtlRuntimeContext`.

---

##### Назначение `MtlVideoRxSession`

`MtlVideoRxSession` отвечает за один ST20P RX session внутри worker graph-а.

Он хранит:

```text
MtlVideoStartConfig cfg
st20p_rx_handle rx
graph_id
MtlWorkerGraphStats*
MtlWorkerEventWriter*
MtlWorkerSharedMemoryRingMap*
MtlWorkerHealthState
receive thread
next shared-memory slot index
next sequence number
```

Слой не знает `IFrameSink` и не доставляет `VideoFrame` напрямую в OBS. Он только пишет frame data в shared memory и сообщает OBS/core process-у, какой slot готов.

---

##### Create pipeline

```c++
static std::expected<std::unique_ptr<MtlVideoRxSession>, Error>
create(
    MtlRuntimeContext &runtime,
    MtlWorkerGraphId graph_id,
    MtlVideoStartConfig cfg,
    MtlWorkerGraphStats &stats,
    MtlWorkerEventWriter &event_writer,
    MtlWorkerSharedMemoryRingMap *media_ring
);
```

Порядок создания:

```text
1. Проверить runtime.handle()
2. Проверить video media ring:
   - ring может быть nullptr
   - если ring есть, он должен быть mapped
   - media_kind должен быть Video
   - slot headers должны быть initialized/valid

3. Создать MtlWorkerHealthState
4. Построить st20p_rx_ops из MtlVideoStartConfig
5. st20p_rx_create(runtime.handle(), &ops)
6. Создать Impl
7. Запустить receive thread
8. Вернуть session
```

Если `st20p_rx_create` не вернул handle, session creation возвращает:

```c++
Error::SystemFailure
```

---

##### Projection в `st20p_rx_ops`

`make_st20p_rx_ops` превращает project `MtlVideoStartConfig` в MTL `st20p_rx_ops`.

Основные projection axes:

```text
MtlVideoFrameRate
→ st_fps

MtlVideoTransportFormat
→ st20_fmt

PixelFormat output_format
→ st_frame_fmt

VideoScanMode
→ ops.interlaced

primary/redundant session ports
→ ops.port fields
```

Заполняются:

```text
ops.name
ops.priv
ops.notify_event

ops.port.num_port
ops.port.ip_addr[]
ops.port.mcast_sip_addr[]
ops.port.port[]
ops.port.udp_port[]
ops.port.payload_type

ops.width
ops.height
ops.fps
ops.interlaced
ops.transport_fmt
ops.output_fmt
ops.device
ops.framebuff_cnt
ops.flags
```

Для blocking receive используется:

```c++
ST20P_RX_FLAG_BLOCK_GET
```

Это важно для receive thread-а: `st20p_rx_get_frame()` может блокироваться до готового frame-а, а stop path будит его через `st20p_rx_wake_block`.

---

##### Fatal MTL event handling

`ops.notify_event` указывает на:

```c++
on_st20p_rx_event
```

Если MTL session сообщает:

```c++
ST_EVENT_FATAL_ERROR
```

session health помечается unhealthy:

```text
Error::InvalidBackendState
"MTL ST20P RX session reported fatal event"
```

Дальше graph/process health checks увидят, что video session уже не healthy.

---

##### Receive thread

После creation запускается `std::jthread`.

Основной loop:

```text
while not stop requested:
    frame = st20p_rx_get_frame(rx)
    if frame == nullptr:
        continue

    record video frame received
    record MTL frame packet metadata

    export frame to shared-memory ring
    if export error:
        record drop
        mark session unhealthy
        st20p_rx_put_frame
        break

    if export returned false:
        record drop

    st20p_rx_put_frame(rx, frame)
    if put failed:
        mark session unhealthy
        break
```

То есть worker не удерживает MTL frame. После copy/export frame обязательно возвращается MTL через `st20p_rx_put_frame`.

---

##### Shared-memory export

```c++
export_video_frame_to_ring(...)
```

Экспорт делает:

```text
1. Если ring/event_writer/frame отсутствуют → return false
2. Найти Empty slot через begin_write_slot
3. Получить slot_payload
4. Скопировать MTL frame planes в payload
5. Заполнить slot media metadata
6. publish_written_slot(...)
7. Отправить MtlWorkerFrameReadyEvent
```

Если свободного slot-а нет:

```text
return false
```

Это считается dropped frame, но не обязательно fatal error.

Если произошла ошибка копирования, publish или IPC write:

```text
return unexpected(error)
```

Это уже marks session unhealthy и останавливает receive loop.

---

##### Plane copy model

```c++
copy_video_frame_planes_to_payload(...)
```

Копирование plane-aware.

Проверяется:

```text
frame != nullptr
frame width/height == cfg width/height
frame fmt == expected output format
interlaced/second_field flags согласованы с cfg.scan_mode
VideoFrame layout валиден
plane_count <= mtlWorkerSharedMemoryMaxPlanes
frame->addr[plane] != nullptr
source_stride >= active_row_bytes
payload capacity достаточна
```

Payload в shared memory пишется tightly packed по plane-ам:

```text
plane 0 bytes
plane 1 bytes
plane 2 bytes
...
```

Для каждой plane metadata содержит:

```text
plane_offset_bytes
plane_size_bytes
plane_line_size_bytes
```

`plane_line_size_bytes` — это line size внутри shared-memory payload, а не обязательно исходный `frame->linesize[plane]` от MTL.

```text
worker:
    копирует из MTL layout в project shared-memory layout

OBS/core:
    читает metadata и восстанавливает VideoFrame plane-by-plane
```

---

##### Slot metadata

Для опубликованного video slot-а заполняется:

```text
media_kind = Video
media_format = static_cast<uint32_t>(cfg.output_format)
width
height
rtp_timestamp
receive_timestamp_ns
plane_count
video_scan_mode
video_field_flags
plane offsets/sizes/line sizes
```

`video_field_flags` отражает:

```text
Interlaced
SecondField
```

Для progressive frame `SecondField` запрещен.

---

##### FrameReady event

После successful publish session отправляет:

```c++
MtlWorkerFrameReadyEvent{
    .graph_id = graph_id,
    .ring_id = ring->descriptor().ring_id,
    .slot_id = slot_index,
    .sequence = sequence,
}
```

Event не содержит payload. Payload уже лежит в shared-memory slot-е.

OBS/core side позже делает:

```text
FrameReadyEvent
→ find graph/ring/slot
→ begin_read_slot_if_matches(slot_id, sequence)
→ read metadata/payload
→ deliver VideoFrame
→ release_read_slot
```

`sequence` защищает от stale ready events.

---

##### Stop / wake

```c++
void wake_block() noexcept;
```

Вызывает:

```c++
st20p_rx_wake_block(rx)
```

Destructor session-а делает:

```text
stop_thread_noexcept()
→ request_stop
→ st20p_rx_wake_block
→ join receive thread
→ st20p_rx_free(rx)
```

Таким образом `st20p_rx_handle` освобождается после остановки receive loop.

---

##### Stats

Session пишет два уровня stats.

В receive loop:

```text
record_video_frame_received()
record_video_frame_mtl_metadata(...)
record_video_frame_dropped()
```

Из MTL frame берется:

```text
pkts_total
pkts_recv primary/redundant
status complete/reconstructed/corrupted
```

В `append_stats_snapshot` session дополнительно запрашивает native MTL session stats:

```c++
st20p_rx_get_session_stats(impl_->rx, &session_stats)
```

и перекладывает их в:

```text
video_st20_rx
video_session_primary
video_session_redundant
video_session_packets_received
video_session_packets_out_of_order
video_session_packets_wrong_ssrc_dropped
video_session_packets_wrong_payload_type_dropped
video_session_bytes_received
video_session_frames_dropped
video_session_frames_packets_missed
video_session_packets_wrong_length_dropped
video_session_slot_get_frame_failures
```

Если query failed:

```text
++video_session_stats_query_failures
```

---

##### Health

```c++
bool healthy() const noexcept;
Error health_error() const noexcept;
std::string health_message() const;
```

Session healthy, если:

```text
impl_ exists
rx handle exists
health state exists and healthy
receive_loop_active == true
```

Unhealthy причины:

```text
нет native handle
нет health state
receive loop inactive
MTL fatal event
shared-memory/IPC export failed
st20p_rx_put_frame failed
receive loop threw
```

Эти ошибки поднимаются выше:

```text
MtlVideoRxSession
→ MtlReceiveGraph::healthy()
→ MtlWorkerProcessState::refresh_worker_health_noexcept()
→ HealthEvent / ErrorEvent
```

---

#### `mtl_audio_rx_session.hpp` / `mtl_audio_rx_session.cpp`

Содержит worker-local MTL ST30P audio receive session.

Это concrete MTL audio data-plane слой внутри worker process-а:

```text
MtlReceiveGraph
→ MtlAudioRxSession::create(...)
→ st30p_rx_create(...)
→ receive thread
→ st30p_rx_get_frame(...)
→ shared-memory audio slot
→ AudioBlockReadyEvent
→ st30p_rx_put_frame(...)
```

`MtlAudioRxSession` владеет `st30p_rx_handle`, но не владеет `mtl_handle`. Runtime handle принадлежит `MtlRuntimeContext`.

---

##### Назначение `MtlAudioRxSession`

`MtlAudioRxSession` отвечает за один ST30P RX session внутри worker graph-а.

Он хранит:

```text
MtlAudioStartConfig cfg
st30p_rx_handle rx
graph_id
MtlWorkerGraphStats*
MtlWorkerEventWriter*
MtlWorkerSharedMemoryRingMap*
MtlWorkerHealthState
receive thread
next shared-memory slot index
next sequence number
```

Слой не знает `IFrameSink` и не доставляет `AudioBuffer` напрямую в OBS/core process. Он только пишет audio block в shared-memory ring и отправляет `AudioBlockReadyEvent`.

---

##### Create pipeline

```c++
static std::expected<std::unique_ptr<MtlAudioRxSession>, Error>
create(
    MtlRuntimeContext &runtime,
    MtlWorkerGraphId graph_id,
    MtlAudioStartConfig cfg,
    MtlWorkerGraphStats &stats,
    MtlWorkerEventWriter &event_writer,
    MtlWorkerSharedMemoryRingMap *media_ring
);
```

Порядок создания:

```text
1. Проверить runtime.handle()
2. Проверить audio media ring:
   - ring может быть nullptr
   - если ring есть, он должен быть mapped
   - media_kind должен быть Audio
   - slot headers должны быть initialized/valid

3. Создать MtlWorkerHealthState
4. Построить st30p_rx_ops из MtlAudioStartConfig
5. st30p_rx_create(runtime.handle(), &ops)
6. Создать Impl
7. Запустить receive thread
8. Вернуть session
```

Если `st30p_rx_create` не вернул handle, session creation возвращает:

```c++
Error::SystemFailure
```

---

##### Projection в `st30p_rx_ops`

`make_st30p_rx_ops` превращает project `MtlAudioStartConfig` в MTL `st30p_rx_ops`.

Основные projection axes:

```text
MtlAudioPcmFormat
→ st30_fmt

MtlAudioSampling
→ st30_sampling

MtlAudioPacketTime
→ st30_ptime

primary/redundant session ports
→ ops.port fields
```

Заполняются:

```text
ops.name
ops.priv

ops.port.num_port
ops.port.ip_addr[]
ops.port.mcast_sip_addr[]
ops.port.port[]
ops.port.udp_port[]
ops.port.payload_type

ops.fmt
ops.channel
ops.sampling
ops.ptime
ops.framebuff_cnt
ops.framebuff_size
ops.flags
```

`framebuff_size` вычисляется через MTL helper:

```c++
st30_calculate_framebuff_size(...)
```

На вход идут:

```text
PCM format
packet time
sampling rate
channel count
frame_buffer_duration_ns
```

Если MTL возвращает `framebuff_size <= 0`, config считается непригодным и возвращается `Error::SystemFailure`.

---

##### Blocking receive mode

Session включает:

```c++
ST30P_RX_FLAG_BLOCK_GET
```

Это означает, что:

```text
st30p_rx_get_frame()
```

может блокироваться до готового audio frame/block-а.

Для stop path используется:

```c++
st30p_rx_wake_block(rx)
```

чтобы разбудить receive thread перед join/destruction.

---

##### Receive thread

После creation запускается `std::jthread`.

Основной loop:

```text
while not stop requested:
    frame = st30p_rx_get_frame(rx)
    if frame == nullptr:
        continue

    record audio block received
    record MTL audio packet metadata

    export frame to shared-memory ring
    if export error:
        record drop
        mark session unhealthy
        st30p_rx_put_frame
        break

    if export returned false:
        record drop

    st30p_rx_put_frame(rx, frame)
    if put failed:
        mark session unhealthy
        break
```

Worker не удерживает MTL frame после обработки. После copy/export frame возвращается MTL через:

```c++
st30p_rx_put_frame(rx, frame)
```

---

##### Shared-memory export

```c++
export_audio_frame_to_ring(...)
```

Экспорт делает:

```text
1. Если ring/event_writer/frame отсутствуют → return false
2. Проверить, что ring mapped
3. Проверить frame->addr и frame->data_size
4. Проверить, что data_size помещается в slot payload capacity
5. Найти Empty slot через begin_write_slot
6. Получить slot_payload
7. Скопировать audio payload в shared-memory slot
8. Заполнить slot media metadata
9. publish_written_slot(...)
10. Отправить MtlWorkerAudioBlockReadyEvent
```

Если свободного slot-а нет или payload не помещается:

```text
return false
```

Это считается dropped audio block, но не обязательно fatal error.

Если произошла ошибка ring operation, publish или IPC write:

```text
return unexpected(error)
```

Это marks session unhealthy и останавливает receive loop.

---

##### Audio payload model

Audio payload копируется как один contiguous block:

```text
frame->addr
→ shared-memory slot payload
```

Metadata описывает его как одну plane:

```text
plane_count = 1
plane_offset_bytes[0] = 0
plane_size_bytes[0] = payload_size
plane_line_size_bytes[0] = payload_size
```

То есть shared-memory audio slot хранит interleaved PCM block в wire/sample layout, а OBS/core-side reader уже преобразует его в project `AudioBuffer`.

---

##### Samples-per-channel derivation

Для metadata session вычисляет:

```text
bytes_per_sample:
    Pcm16 → 2
    Pcm24 → 3

channels:
    frame->channel, если он не 0
    иначе cfg.media.channel_count

bytes_per_frame:
    channels * bytes_per_sample

samples_per_channel:
    payload_size / bytes_per_frame
```

Эти значения попадают в `MtlWorkerSharedMemorySlotMediaMetadata`:

```text
sample_rate_hz
channels
samples_per_channel
media_format = MtlAudioPcmFormat
```

---

##### AudioBlockReady event

После successful publish session отправляет:

```c++
MtlWorkerAudioBlockReadyEvent{
    .graph_id = graph_id,
    .ring_id = ring->descriptor().ring_id,
    .slot_id = slot_index,
    .sequence = sequence,
}
```

Event не содержит audio payload. Payload уже лежит в shared-memory slot-е.

OBS/core side дальше делает:

```text
AudioBlockReadyEvent
→ find graph/ring/slot
→ begin_read_slot_if_matches(slot_id, sequence)
→ read metadata/payload
→ convert PCM16/PCM24 to AudioBuffer int32 samples
→ IFrameSink::on_audio_frame
→ release_read_slot
```

`sequence` защищает от stale ready events.

---

##### Stop / wake

```c++
void wake_block() noexcept;
```

Вызывает:

```c++
st30p_rx_wake_block(rx)
```

Destructor session-а делает:

```text
stop_thread_noexcept()
→ request_stop
→ st30p_rx_wake_block
→ join receive thread
→ st30p_rx_free(rx)
```

То есть `st30p_rx_handle` освобождается только после остановки receive loop.

---

##### Stats

Session пишет два уровня stats.

В receive loop:

```text
record_audio_block_received()
record_audio_block_mtl_metadata(...)
record_audio_block_dropped()
```

Из `st30_frame` берется:

```text
data_size
pkts_total
pkts_recv primary
pkts_recv redundant
```

В `append_stats_snapshot` session дополнительно запрашивает native MTL session stats:

```c++
st30p_rx_get_session_stats(impl_->rx, &session_stats)
```

и перекладывает их в:

```text
audio_session_primary
audio_session_redundant
audio_session_packets_received
audio_session_packets_out_of_order
audio_session_packets_wrong_ssrc_dropped
audio_session_packets_wrong_payload_type_dropped
audio_session_packets_redundant
audio_session_packets_dropped
audio_session_packets_length_mismatch_dropped
audio_session_slot_get_frame_failures
```

Если query failed:

```text
++audio_session_stats_query_failures
```

---

##### Health

```c++
bool healthy() const noexcept;
Error health_error() const noexcept;
std::string health_message() const;
```

Session healthy, если:

```text
impl_ exists
rx handle exists
health state exists and healthy
receive_loop_active == true
```

Unhealthy причины:

```text
нет native handle
нет health state
receive loop inactive
shared-memory/IPC export failed
st30p_rx_put_frame failed
receive loop threw
```

Эти ошибки поднимаются выше:

```text
MtlAudioRxSession
→ MtlReceiveGraph::healthy()
→ MtlWorkerProcessState::refresh_worker_health_noexcept()
→ HealthEvent / ErrorEvent
```

---

#### `mtl_worker_stats.hpp`

Содержит worker-local stats model для MTL receive graph-а.

Этот файл описывает counters, которые собираются **внутри worker process-а**:

```text
MtlVideoRxSession / MtlAudioRxSession
→ MtlWorkerGraphStats
→ MtlReceiveGraph::stats_snapshot()
→ MtlWorkerProcessState::handle(StatsRequest)
→ MtlWorkerStatsEvent
→ OBS/core process
```

---

##### `MtlWorkerGraphStatsSnapshot`

```c++
struct MtlWorkerGraphStatsSnapshot;
```

Это immutable snapshot structure, в которую собираются все worker-side counters перед отправкой в `MtlWorkerStatsEvent`.

Snapshot включает несколько групп.

---

##### Worker-local receive counters

```text
video_frames_received
audio_blocks_received
video_frames_dropped
audio_blocks_dropped
```

Эти counters пишутся receive sessions:

```text
MtlVideoRxSession:
    record_video_frame_received()
    record_video_frame_dropped()

MtlAudioRxSession:
    record_audio_block_received()
    record_audio_block_dropped()
```

Смысл:

```text
received:
    session получила frame/block от MTL через st20p_rx_get_frame / st30p_rx_get_frame

dropped:
    worker не смог экспортировать frame/block дальше,
    например нет свободного shared-memory slot-а,
    payload не помещается,
    или export path вернул ошибку
```

---

##### Video frame packet metadata

```text
video_frame_packets_total
video_frame_packets_received_primary
video_frame_packets_received_redundant
video_reconstructed_frames
video_corrupted_frames
video_complete_frames
```

Эти значения берутся из MTL-provided metadata у полученного video frame.

`MtlVideoRxSession` вызывает:

```c++
record_video_frame_packet_metadata(...)
```

и передает туда:

```text
packets_total
packets received on primary leg
packets received on redundant leg
complete/reconstructed/corrupted frame status
```

Смысл этих counters:

```text
video_frame_packets_total:
    сколько RTP packets, по MTL metadata, относилось к полученным video frames

video_frame_packets_received_primary:
    сколько packets пришло через primary leg

video_frame_packets_received_redundant:
    сколько packets пришло через redundant leg

video_reconstructed_frames:
    frames, которые MTL смог реконструировать

video_corrupted_frames:
    frames, которые MTL отметил как corrupted

video_complete_frames:
    frames, которые MTL отметил как complete
```

---

##### Audio block packet metadata

```text
audio_block_bytes_received
audio_block_packets_total
audio_block_packets_received_primary
audio_block_packets_received_redundant
```

Эти значения берутся из `st30_frame` metadata.

`MtlAudioRxSession` вызывает:

```c++
record_audio_block_packet_metadata(...)
```

и передает:

```text
frame.data_size
frame.pkts_total
frame.pkts_recv[primary]
frame.pkts_recv[redundant]
```

Смысл:

```text
audio_block_bytes_received:
    суммарный размер audio payload blocks, полученных от MTL

audio_block_packets_total:
    суммарное количество RTP packets по MTL metadata

audio_block_packets_received_primary:
    packets, полученные через primary leg

audio_block_packets_received_redundant:
    packets, полученные через redundant leg
```

---

##### Native MTL video session stats

```text
video_session_stats_available
video_session_primary
video_session_redundant
video_session_packets_received
video_session_packets_out_of_order
video_session_packets_wrong_ssrc_dropped
video_session_packets_wrong_payload_type_dropped
video_session_bytes_received
video_session_frames_dropped
video_session_frames_packets_missed
video_session_packets_wrong_length_dropped
video_session_slot_get_frame_failures
video_session_stats_query_failures
video_st20_rx
```

Эти поля не заполняются самим `MtlWorkerGraphStats`.

Они добавляются позже в:

```text
MtlVideoRxSession::append_stats_snapshot(...)
```

через MTL native stats query.

То есть `MtlWorkerGraphStats::snapshot()` возвращает базовые atomic counters, а video session затем дополняет snapshot native ST20P RX statistics.

---

##### Native MTL audio session stats

```text
audio_session_stats_available
audio_session_primary
audio_session_redundant
audio_session_packets_received
audio_session_packets_out_of_order
audio_session_packets_wrong_ssrc_dropped
audio_session_packets_wrong_payload_type_dropped
audio_session_packets_redundant
audio_session_packets_dropped
audio_session_packets_length_mismatch_dropped
audio_session_slot_get_frame_failures
audio_session_stats_query_failures
```

Эти поля аналогично заполняются не `MtlWorkerGraphStats`, а:

```text
MtlAudioRxSession::append_stats_snapshot(...)
```

через native ST30P RX stats query.

---

##### Native MTL device port stats

```text
mtl_primary_port_stats_available
mtl_primary_port
mtl_redundant_port_stats_available
mtl_redundant_port
mtl_port_stats_query_failures
```

Эта группа добавляется в:

```text
MtlRuntimeContext::append_stats_snapshot(...)
```

через MTL runtime/device port stats query.

Это уже не session-level stats, а MTL port/device-level counters:

```text
rx_packets
rx_bytes
rx_err_packets
rx_hw_dropped_packets
rx_nombuf_packets
```

---

##### `MtlWorkerGraphStats`

```c++
class MtlWorkerGraphStats final;
```

Это потокобезопасный worker-local accumulator для counters, которые пишутся из receive threads.

Он содержит только atomic counters:

```text
std::atomic_uint64_t ...
```

и использует:

```c++
std::memory_order_relaxed
```

---

##### Recording API

Основные методы:

```c++
record_video_frame_received()
record_audio_block_received()

record_video_frame_dropped()
record_audio_block_dropped()
```

Эти методы вызываются session receive loop-ами при получении или потере frame/block.

Для MTL metadata есть отдельные методы:

```c++
record_video_frame_packet_metadata(...)
record_audio_block_packet_metadata(...)
```

Они суммируют counters, полученные из MTL frame/block metadata.

---

##### Snapshot API

```c++
MtlWorkerGraphStatsSnapshot snapshot() const noexcept;
```

`snapshot()` читает atomic counters и возвращает `MtlWorkerGraphStatsSnapshot`.

На этом этапе snapshot содержит только counters, которые принадлежат `MtlWorkerGraphStats`.

Затем `MtlReceiveGraph::stats_snapshot()` дополняет его:

```text
1. base snapshot from MtlWorkerGraphStats
2. video session native stats
3. audio session native stats
4. runtime/device port stats
```

И только после этого `MtlWorkerProcessState` перекладывает итоговый snapshot в `MtlWorkerStatsEvent`.

---

#### `mtl_worker_health.hpp`

Содержит небольшой worker-local health latch для компонентов MTL worker-а.

задача файла — сохранить факт, что компонент стал unhealthy, вместе с первопричиной ошибки.

---

##### `MtlWorkerHealthState`

```c++
class MtlWorkerHealthState final;
```

`MtlWorkerHealthState` хранит:

```text
healthy flag
error code
diagnostic message
```

В начальном состоянии:

```text
healthy = true
error = Error::Ok
message = "MTL worker component healthy"
```

---

##### Mark unhealthy

```c++
void mark_unhealthy(st2110::Error error, std::string message) noexcept;
```

Помечает компонент unhealthy.

Важное поведение:

```text
если компонент уже unhealthy:
    ничего не менять

если error == Error::Ok:
    сохранить Error::InvalidBackendState

иначе:
    сохранить переданный error

сохранить diagnostic message
```

То есть health state фиксирует **первую ошибку** и не затирает ее последующими ошибками.

---

##### Read API

```c++
bool healthy() const noexcept;
st2110::Error error() const noexcept;
std::string message() const;
```

Эти методы используются higher-level слоями:

```text
MtlVideoRxSession::healthy()
MtlVideoRxSession::health_error()
MtlVideoRxSession::health_message()

MtlAudioRxSession::healthy()
MtlAudioRxSession::health_error()
MtlAudioRxSession::health_message()
```

Дальше состояние поднимается в:

```text
MtlReceiveGraph
→ MtlWorkerProcessState
→ MtlWorkerHealthEvent / MtlWorkerErrorEvent
```

---

##### Thread-safety

Внутри используется:

```c++
mutable std::mutex mutex_;
```

Это нужно потому, что health state может изменяться из receive thread-а:

```text
MTL get/put failure
shared-memory export failure
IPC write failure
receive loop exception
fatal session event
```

а читаться из command thread-а при обработке:

```text
HealthCheck
Stats
StartSessions
StopSessions
```

---

##### Exception safety

`mark_unhealthy`, `healthy`, `error` имеют `noexcept` и ловят исключения.

Если `healthy()` не смог получить lock или произошла ошибка:

```text
return false
```

Если `error()` не смог получить lock:

```text
return Error::InvalidBackendState
```

То есть failure внутри health-state механизма трактуется как unhealthy состояние, а не как success.

`message()` не `noexcept`, потому что возвращает `std::string` и может аллоцировать.

---
