## Orchestrator step 0 — SDP media-kind classification and parser dispatch

After discovery, the orchestrator may have one or more raw SDP objects whose media kind is already known or still unknown.

The media-specific SDP parsers are not responsible for guessing whether an SDP object is video or audio. They parse already-selected media-specific SDP objects.

Therefore, before calling a media-specific parser, the orchestrator/ingress boundary must ensure that the media kind of each selected SDP object is known.

Allowed sources of media-kind information:

    1. provider-declared media kind;
    2. lightweight SDP classification;
    3. full media-specific SDP parsing after classification dispatch.

If discovery/provider already declared media kind:

    declared_media_kind = video -> call parse_video_stream_signaling(...)
    declared_media_kind = audio -> call parse_audio_stream_signaling(...)

If discovery/provider did not declare media kind, the project must first classify the raw SDP object by inspecting its SDP media sections.

Classification rule:

    all media sections are video -> media kind = video
    all media sections are audio  -> media kind = audio
    mixed video + audio sections  -> reject explicitly
    no supported media sections   -> reject explicitly

A valid supported SDP object may contain:

    one media section for one RTP stream

or:

    two media sections of the same media kind for duplicate/redundant topology

Examples:

    m=video ...
    -> video SDP object

    m=audio ...
    -> audio SDP object

    m=video ...
    m=video ...
    -> video SDP object with duplicate/redundant topology

    m=audio ...
    m=audio ...
    -> audio SDP object with duplicate/redundant topology

Unsupported for the current project receive contract:

    m=video ...
    m=audio ...
    -> mixed media kinds inside one SDP object

After classification, dispatch is explicit:

    video SDP object -> video SDP ingress parser
    audio SDP object -> audio SDP ingress parser

The result of this step is a typed parsed stream object per accepted SDP object.

The downstream pipeline must not guess media kind again after this dispatch.

## Orchestrator step 1 — SDP media-kind classification

After discovery, the orchestrator has one or more selected raw SDP objects.

If the discovery/provider already declared the media kind of an SDP object, the orchestrator may use that declared media kind.

If the media kind is unknown, the orchestrator must classify the SDP object before calling a media-specific parser.

Classification is intentionally lightweight:

    raw SDP object
      -> parse_raw_sdp_document(...)
      -> inspect the first m= media section
      -> return media kind:
           video -> SdpMediaKind::Video
           audio -> SdpMediaKind::Audio

The classifier does not validate the full SDP object.

The classifier does not check:

    fmtp consistency
    rtpmap consistency
    duplicate-stream structure
    whether all media sections have the same media kind

Those checks are not owned by this step.

After classification, dispatch is explicit:

    SdpMediaKind::Video
      -> parse_video_stream_signaling(...)

    SdpMediaKind::Audio
      -> parse_audio_stream_signaling(...)

The media-specific parsers remain responsible for accepting only their own media kind.

Therefore, if an SDP object contains mixed video and audio media sections, it will be rejected by the selected media-specific parser.

Result of this step:

    raw SDP object with unknown media kind
      -> known SdpMediaKind
      -> media-specific SDP parser dispatch

## Orchestrator step 2 — media-specific SDP parsing

After SDP media-kind classification, the orchestrator has:

    raw SDP object
    SdpMediaKind::Video | SdpMediaKind::Audio

The orchestrator dispatches the SDP object to the matching media-specific parser:

    SdpMediaKind::Video
      -> parse_video_stream_signaling(...)

    SdpMediaKind::Audio
      -> parse_audio_stream_signaling(...)

This step converts external raw SDP into the project typed ingress model:

    raw SDP
      -> RawSdpDocument
      -> ParsedSdpStreamSet

The resulting ParsedSdpStreamSet represents one essence stream:

    one parsed leg
      -> one RTP stream

    two parsed legs
      -> duplicate/redundant RTP topology for the same essence

For video SDP objects, parsed legs contain VideoStreamSignaling.

For audio SDP objects, parsed legs contain AudioStreamSignaling.

The media-specific parser is responsible for:

    accepting only its own media kind
    accepting only one media section or a same-kind duplicate pair
    parsing required media-specific SDP parameters
    building the typed video/audio signaling model
    returning an explicit error if the project cannot build that model

The parser is not a universal SDP correctness auditor.

The project assumes the sender/control plane is responsible for producing semantically correct ST 2110 SDP.

The parser validates only what is required to safely build the trusted internal typed model.

After this step, downstream pipeline stages must use the typed ParsedSdpStreamSet instead of raw SDP.

## Orchestrator step 3 — parsed SDP projection to receive bootstrap

After media-specific SDP parsing, the orchestrator has a typed ParsedSdpStreamSet for one essence stream.

The orchestrator projects that parsed SDP result into the project receive bootstrap model:

    video ParsedSdpStreamSet
      -> project_parsed_video_sdp_to_receive_bootstrap(...)
      -> VideoReceiveBootstrap

    audio ParsedSdpStreamSet
      -> project_parsed_audio_sdp_to_receive_bootstrap(...)
      -> AudioReceiveBootstrap

The receive bootstrap model is receive-oriented and no longer raw-SDP-oriented.

For common receive state it carries:

    receive topology:
      SingleStream
      RedundantPair

    remote legs:
      mid
      UDP port
      destination endpoint
      source filter
      max UDP datagram size

    signaled stream state:
      expected RTP payload type
      timing signaling

For video, the bootstrap additionally carries:

    VideoMediaDescription
    scan mode
    packing mode
    sender type
    TROFF
    CMAX

For audio, the bootstrap additionally carries:

    AudioMediaDescription
    channel order

A ParsedSdpStreamSet with one leg becomes SingleStream topology.

A ParsedSdpStreamSet with two legs becomes RedundantPair topology.

Duplicate/redundant topology must remain explicitly modeled as one essence with two legs, not as two unrelated streams.

This step must not re-parse raw SDP.

This step may rely on invariants guaranteed by the media-specific SDP parser.

## Orchestrator step 4 — automatic local receive policy selection

After parsed SDP projection, the orchestrator has a receive bootstrap object:

    VideoReceiveBootstrap
    or
    AudioReceiveBootstrap

The receive bootstrap contains the remote receive description:

    receive topology
    remote legs
    destination endpoint
    source filter
    UDP port
    max UDP datagram size
    media timing/signaling

Before constructing the final receive start request, the orchestrator must build the local receive policy automatically.

The project does not require user-selected network interfaces for this step.

Local policy selection flow:

    ReceiveBootstrap
      -> auto_select_receive_local_policy(...)
      -> ReceiveLocalPolicy

For each remote leg, the policy selector determines the route lookup target:

    if source-filter has a source address:
        use the source-filter source address

    otherwise:
        use the destination address

Then it determines:

    address family:
      IPv4 or IPv6

    preferred local IP:
      local address selected by the OS route to that remote target

For single-stream topology:

    one remote leg
      -> one local policy leg

For redundant-pair topology:

    two remote legs
      -> two local policy legs

This step only selects local network receive policy.

It must not:

    re-parse SDP
    modify media signaling
    choose backend
    start sockets or MTL runtime

Result of this step:

    ReceiveBootstrap
    ReceiveLocalPolicy

These are then used to construct the ReceiveStartRequest.

## Orchestrator step 5 — receive start request construction

After receive bootstrap projection and automatic local policy selection, the orchestrator has two typed project-level objects:

    VideoReceiveBootstrap
    or
    AudioReceiveBootstrap

    ReceiveLocalPolicy

The orchestrator combines them into one receive start request:

    media bootstrap
    local receive policy
      -> ReceiveStartRequest

ReceiveStartRequest is the last common project-level startup object before backend-specific projection.

It contains:

    ReceiveMediaBootstrap media
      VideoReceiveBootstrap
      or
      AudioReceiveBootstrap

    ReceiveLocalPolicy local

This step must only compose already-built typed objects.

It must not:

    re-parse SDP
    classify media kind again
    rebuild receive bootstrap
    reselect local policy
    choose backend
    project to socket or MTL config
    start runtime

The receive topology remains inside the media bootstrap:

    SingleStream
    or
    RedundantPair

A redundant pair remains one essence with two receive legs, not two unrelated receive requests.

Result of this step:

    ReceiveStartRequest

The next step is backend selection and backend-specific start config projection.

## Socket pipeline step 6 — Socket start config projection

After receive start request construction, the orchestrator has one or more typed ReceiveStartRequest objects.

The number and media kind of requests is determined by the earlier pipeline state:

    single video
      -> one video ReceiveStartRequest

    single audio
      -> one audio ReceiveStartRequest

    audio + video
      -> one video ReceiveStartRequest
      -> one audio ReceiveStartRequest

Backend kind is selected only from Settings:

    Settings.backend_kind = ReceiveBackendKind::Socket

For the Socket backend, each ReceiveStartRequest is projected into the matching Socket-specific start config:

    video ReceiveStartRequest + Settings
      -> project_receive_start_request_to_socket_video_start(...)
      -> SocketVideoStartConfig

    audio ReceiveStartRequest + Settings
      -> project_receive_start_request_to_socket_audio_start(...)
      -> SocketAudioStartConfig

This step is backend-specific projection.

It maps common receive state into Socket runtime input:

    receive topology
      -> SocketStartConfig.topology

    ReceiveBootstrap remote legs + ReceiveLocalPolicy
      -> SocketMediaLegConfig[]
      -> SocketRxOpenConfig[]

    expected payload type
      -> Socket media stream config

    reorder settings
      -> Socket reorder buffer config

    video media signaling
      -> SocketVideoStreamConfig
      -> VideoReceivePipelineConfig

    audio media signaling
      -> SocketAudioStreamConfig
      -> samples_per_packet derived from AudioMediaDescription

This step must not:

    re-parse SDP
    classify media kind
    rebuild receive bootstrap
    reselect local policy
    start sockets
    receive packets
    depacketize media
    deliver frames to OBS

Result of this step:

    SocketVideoStartConfig, optional
    SocketAudioStartConfig, optional

The next Socket-specific step is construction of the corresponding Socket backend instance:

    SocketVideoStartConfig
      -> SocketRxVideoBackend

    SocketAudioStartConfig
      -> SocketRxAudioBackend

## Socket video pipeline step 7 — Socket video backend construction

After Socket video start config projection, the orchestrator has a fully built SocketVideoStartConfig.

The orchestrator constructs the Socket video backend:

    SocketVideoStartConfig
      -> SocketRxVideoBackend

This step is construction-only.

The SocketRxVideoBackend constructor stores common Socket receive runtime configuration:

    socket open configs
    max UDP datagram size
    reorder buffer config
    expected RTP payload type
    port factory

It also constructs the video-specific receive pipeline:

    VideoReceivePipelineConfig
      -> VideoReceivePipeline

This step must not:

    open sockets
    start receive threads
    receive packets
    parse RTP packets
    reorder packets
    depacketize video
    reconstruct frames
    deliver frames to the sink

After this step, the backend object is ready to be started, but no network runtime is active yet.

Result of this step:

    SocketRxVideoBackend constructed

The next Socket video step is backend startup:

    SocketRxVideoBackend::start(...)
      -> start_common_runtime(...)

## Socket video pipeline step 8 — Socket video backend startup

After SocketRxVideoBackend construction, the backend object exists but no network runtime is active yet.

The orchestrator starts the backend by calling:

    SocketRxVideoBackend::start(sink)

Startup flow:

    SocketRxVideoBackend::start(...)
      -> create video reorder buffer
      -> start_common_runtime(sink)

The common Socket runtime startup performs:

    store sink pointer
    clear duplicate merge history
    create one socket port per configured leg
    open each socket port
    allocate one receive buffer per leg
    reset packet queue
    enable duplicate merge when more than one leg is active
    start downstream processing thread
    start one receive thread per leg

For single-stream topology:

    one SocketRxOpenConfig
      -> one socket port
      -> one receive thread

For redundant-pair topology:

    two SocketRxOpenConfig entries
      -> two socket ports
      -> two receive threads
      -> duplicate merge enabled

On Linux, socket port opening performs:

    create UDP socket
    configure socket options
    bind socket
    join multicast group when configured
    apply source filtering when configured

This step activates the Socket video runtime.

It must not:

    parse SDP
    build SocketVideoStartConfig
    choose backend
    reconstruct video frames directly
    deliver frames before packets are received and processed

Result of this step:

    active SocketRxVideoBackend runtime
    opened socket port(s)
    running receive thread(s)
    running downstream packet-processing thread

The next Socket video step is per-leg UDP datagram receive and packet admission.

## Socket video pipeline step 9 — per-leg UDP receive and packet admission

After Socket video backend startup, the runtime has:

    opened socket port(s)
    one receive buffer per leg
    one receive thread per leg
    one downstream packet-processing thread

Each receive thread runs the per-leg receive loop:

    socket port
      -> receive UDP datagram
      -> attach receive timestamp
      -> process_received_datagram(...)

On Linux, socket receive performs:

    blocking recvfrom(...)
    local monotonic receive timestamp capture
    source-filter check against sender address

Datagrams from non-matching source-filter senders are ignored before packet admission.

The common packet admission path performs:

    UDP datagram
      -> drop RTCP-like/control datagrams
      -> validate packet size against MAXUDP
      -> parse as video RTP/ST 2110-20 packet
      -> check expected RTP payload type
      -> attach receive timestamp
      -> store as owning StoredPacket
      -> push into packet queue

For video, packet parsing builds a VideoPacketView by parsing:

    RTP header
    ST 2110-20 payload header
    SRD segment headers
    SRD segment payload spans
    extended sequence number

The receive thread does not retain non-owning packet views.

Before enqueueing, the parsed VideoPacketView is converted into an owning StoredPacket so the downstream thread is independent of receive-buffer lifetime.

For single-stream topology:

    one receive thread
      -> common packet queue

For redundant-pair topology:

    two receive threads
      -> common packet queue

Duplicate merge is not performed in the receive thread.

This step must not:

    re-parse SDP
    choose backend
    open sockets
    reorder packets
    depacketize video
    reconstruct frames
    deliver frames to the sink

Result of this step:

    admitted owning StoredPacket entries in the downstream packet queue

The next Socket video step is downstream packet processing:

    packet queue
      -> duplicate merge
      -> reorder buffer
      -> video receive pipeline

## Socket video pipeline step 10 — downstream packet processing, duplicate merge, and reorder

After packet admission, receive thread(s) push owning StoredPacket objects into the common downstream packet queue.

The downstream thread consumes this queue:

    packet_queue_
      -> run_downstream_loop(...)
      -> process_stored_packet_downstream(...)

This step is responsible for ordering admitted packets before video reconstruction.

Processing flow:

    StoredPacket from queue
      -> duplicate merge, if enabled
      -> reorder buffer push
      -> reorder buffer drain
      -> ordered StoredPacket
      -> video-specific deliver_media(...)

For single-stream topology:

    one receive leg
      -> packet queue
      -> reorder buffer

For redundant-pair topology:

    two receive legs
      -> packet queue
      -> duplicate sequence check
      -> reorder buffer

Duplicate merge is enabled when more than one receive leg is active.

Duplicate merge uses packet reorder sequence history:

    if sequence was already seen:
        drop packet

    otherwise:
        accept packet into reorder buffer
        remember sequence

For video, the reorder buffer uses the ST 2110-20 extended sequence number.

The video reorder buffer:

    initializes from the first accepted packet
    stores packets by extended sequence number
    pops only the next expected sequence number
    applies the configured reorder gap policy when a sequence is missing

Currently supported gap policies:

    WaitForMissing
    FlushGapOnce

This step must not:

    receive UDP datagrams
    parse SDP
    choose backend
    parse newly received RTP from socket buffers
    depacketize video directly in receive threads
    perform AV synchronization
    deliver frames directly to OBS

Result of this step:

    ordered video StoredPacket objects handed to the video-specific media path

The next Socket video step is video packet-to-frame processing:

    ordered StoredPacket
      -> VideoPacketView
      -> VideoReceivePipeline::push(...)
      -> video depacketization / reconstruction

## Socket video pipeline step 11 — video packet-to-frame processing

After downstream packet processing, duplicate merge, and reorder, the Socket video backend receives ordered video StoredPacket objects.

The video backend converts each ordered packet back into a VideoPacketView and pushes it into the video receive pipeline:

    ordered StoredPacket
      -> VideoPacketView
      -> VideoReceivePipeline::push(...)

This step is video-specific media reconstruction.

The video receive pipeline contains two stages:

    Depacketizer
    VideoUnitReconstructor

Depacketizer flow:

    VideoPacketView
      -> determine video assembly key
      -> map ST 2110-20 SRD segments to frame write operations
      -> write segment payload bytes into FrameAssembler
      -> emit AssembledVideoUnit when the current unit completes or changes

Assembly key depends on scan mode:

    Progressive
      -> frame by RTP timestamp

    Interlaced
      -> field by RTP timestamp + field-id

    PsF
      -> segment by RTP timestamp + field-id

FrameAssembler owns the currently assembled VideoFrame.

It writes packet segment bytes into the target frame and tracks write coverage.

At unit end, FrameAssembler may emit:

    complete unit
    partial unit
    dropped partial
    not emittable

Partial unit behavior is controlled by PartialUnitPolicy.

VideoUnitReconstructor converts assembled units into reconstructed frames:

    Frame unit
      -> emitted directly as ReconstructedVideoFrame

    Field unit
      -> pair field 0 and field 1
      -> weave rows into a full frame
      -> emit ReconstructedVideoFrame

    PsF segment unit
      -> pair segment 0 and segment 1 with matching RTP timestamp
      -> weave rows into a full frame
      -> emit ReconstructedVideoFrame

This step must not:

    receive UDP datagrams
    parse SDP
    choose backend
    open sockets
    perform source filtering
    perform duplicate merge
    reorder packets
    deliver directly from receive threads

Result of this step:

    zero or more ReconstructedVideoFrame objects

The next Socket video step is frame delivery to the configured sink:

    ReconstructedVideoFrame
      -> IFrameSink::on_video_frame(...)

## Socket video pipeline step 12 — reconstructed video frame delivery to sink

After video packet-to-frame processing, the Socket video backend may receive zero or more ReconstructedVideoFrame objects.

Each reconstructed frame is delivered through the backend sink contract:

    ReconstructedVideoFrame
      -> SocketRxVideoBackend::deliver_reconstructed_frame(...)
      -> IFrameSink::on_video_frame(...)

The delivered frame contains:

    VideoFrame
    RTP timestamp
    receive timestamp

Delivery flow:

    ReconstructedVideoFrame.frame
      -> moved into IFrameSink::on_video_frame(...)

    ReconstructedVideoFrame timing
      -> FrameTimingMetadata
           rtp_timestamp
           receive_timestamp_ns

IFrameSink is the boundary between the Socket backend and the higher OBS-facing layer.

The Socket backend must not know how OBS consumes, schedules, renders, or synchronizes the frame.

This step must not:

    receive UDP datagrams
    parse RTP packets
    reorder packets
    depacketize video
    assemble video frames
    perform AV synchronization
    render into OBS directly

Result of this step:

    reconstructed VideoFrame delivered to the configured IFrameSink
    with FrameTimingMetadata attached

This completes the current Socket video receive path.

## Socket audio pipeline step 7 — Socket audio backend construction

After Socket audio start config projection, the orchestrator has a fully built SocketAudioStartConfig.

The orchestrator constructs the Socket audio backend:

    SocketAudioStartConfig
      -> SocketRxAudioBackend

This step is construction-only.

The SocketRxAudioBackend constructor stores common Socket receive runtime configuration through the shared Socket backend base:

    socket open configs
    max UDP datagram size
    reorder buffer config
    expected RTP payload type
    port factory

It also stores audio-specific receive state:

    AudioMediaDescription
    samples_per_packet
    AudioFrameAssembler

Unlike the Socket video backend, the Socket audio backend does not construct a separate AudioReceivePipeline object.

Audio packet parsing and audio block assembly happen later in the runtime path through:

    parse_audio_rtp_packet_view(...)
    AudioFrameAssembler::push(...)

This step must not:

    parse SDP
    classify media kind
    select local IP
    project ReceiveStartRequest
    choose backend
    open sockets
    start receive threads
    receive packets
    assemble audio blocks
    deliver audio frames to the sink

After this step, the backend object is ready to be started, but no network runtime is active yet.

Result of this step:

    SocketRxAudioBackend constructed

The next Socket audio step is backend startup:

    SocketRxAudioBackend::start(...)
      -> start_common_runtime(...)

## Socket audio pipeline step 8 — Socket audio backend startup

After SocketRxAudioBackend construction, the backend object exists but no network runtime is active yet.

The orchestrator starts the backend by calling:

    SocketRxAudioBackend::start(sink)

Startup flow:

    SocketRxAudioBackend::start(...)
      -> create audio reorder buffer
      -> start_common_runtime(sink)

The Socket audio backend uses the audio reorder buffer variant:

    FixedWindowReorderBuffer<false>

The common Socket runtime startup performs:

    store sink pointer
    clear duplicate merge history
    create one socket port per configured leg
    open each socket port
    allocate one receive buffer per leg
    reset packet queue
    enable duplicate merge when more than one leg is active
    start downstream processing thread
    start one receive thread per leg

For single-stream topology:

    one SocketRxOpenConfig
      -> one socket port
      -> one receive thread

For redundant-pair topology:

    two SocketRxOpenConfig entries
      -> two socket ports
      -> two receive threads
      -> duplicate merge enabled

On Linux, socket port opening performs:

    create UDP socket
    configure socket options
    bind socket
    join multicast group when configured
    apply source filtering when configured

This step activates the Socket audio runtime.

It must not:

    parse SDP
    build SocketAudioStartConfig
    choose backend
    parse audio RTP packets directly in start(...)
    assemble audio blocks
    deliver audio frames to the sink

Result of this step:

    active SocketRxAudioBackend runtime
    opened socket port(s)
    running receive thread(s)
    running downstream packet-processing thread

The next Socket audio step is per-leg UDP datagram receive and audio packet admission.

## Socket audio pipeline step 9 — per-leg UDP receive and audio packet admission

After Socket audio backend startup, the runtime has:

    opened socket port(s)
    one receive buffer per leg
    one receive thread per leg
    one downstream packet-processing thread

Each receive thread runs the shared Socket receive loop:

    socket port
      -> receive UDP datagram
      -> attach receive timestamp
      -> process_received_datagram(...)

On Linux, socket receive performs:

    blocking recvfrom(...)
    local monotonic receive timestamp capture
    source-filter check against sender address

Datagrams from non-matching source-filter senders are ignored before packet admission.

The common packet admission path performs:

    UDP datagram
      -> drop RTCP-like/control datagrams
      -> validate packet size against MAXUDP
      -> call audio-specific parse_packet(...)
      -> check expected RTP payload type
      -> attach receive timestamp
      -> store as owning StoredPacket
      -> push into packet queue

For audio, packet parsing uses:

    parse_audio_rtp_packet_view(...)

Audio packet parsing performs:

    RTP header parse
    RTP payload extraction
    PCM payload size check
    AudioPacketView construction

The expected PCM payload size is derived from:

    samples_per_packet
    channel_count
    PCM bit depth

Audio packet ordering uses the RTP sequence number.

The receive thread does not retain non-owning packet views.

Before enqueueing, the parsed AudioPacketView is converted into an owning StoredPacket so the downstream thread is independent of receive-buffer lifetime.

For single-stream topology:

    one receive thread
      -> common packet queue

For redundant-pair topology:

    two receive threads
      -> common packet queue

Duplicate merge is not performed in the receive thread.

This step must not:

    re-parse SDP
    choose backend
    open sockets
    perform duplicate merge
    reorder packets
    assemble audio blocks
    deliver audio frames to the sink

Result of this step:

    admitted owning AudioStoredPacket entries in the downstream packet queue

The next Socket audio step is downstream packet processing:

    packet queue
      -> duplicate merge
      -> audio reorder buffer
      -> AudioFrameAssembler::push(...)

## Socket audio pipeline step 10 — downstream packet processing, duplicate merge, and reorder

After audio packet admission, receive thread(s) push owning AudioStoredPacket objects into the common downstream packet queue.

The downstream thread consumes this queue:

    packet_queue_
      -> run_downstream_loop(...)
      -> process_stored_packet_downstream(...)

This step is responsible for ordering admitted audio packets before audio block assembly.

Processing flow:

    AudioStoredPacket from queue
      -> duplicate merge, if enabled
      -> reorder buffer push
      -> reorder buffer drain
      -> ordered AudioStoredPacket
      -> audio-specific deliver_media(...)

For single-stream topology:

    one receive leg
      -> packet queue
      -> audio reorder buffer

For redundant-pair topology:

    two receive legs
      -> packet queue
      -> duplicate sequence check
      -> audio reorder buffer

Duplicate merge is enabled when more than one receive leg is active.

Duplicate merge uses packet reorder sequence history:

    if sequence was already seen:
        drop packet

    otherwise:
        accept packet into reorder buffer
        remember sequence

For audio, the reorder sequence is the RTP sequence number.

The audio reorder buffer:

    initializes from the first accepted packet
    stores packets by RTP sequence number
    pops only the next expected RTP sequence number
    applies the configured reorder gap policy when a sequence is missing

Currently supported gap policies:

    WaitForMissing
    FlushGapOnce

This step must not:

    receive UDP datagrams
    parse SDP
    choose backend
    parse newly received RTP from socket buffers
    assemble audio blocks directly in receive threads
    perform AV synchronization
    deliver frames directly to the sink

Result of this step:

    ordered audio StoredPacket objects handed to the audio-specific media path

The next Socket audio step is audio packet-to-block processing:

    ordered AudioStoredPacket
      -> AudioPacketView
      -> AudioFrameAssembler::push(...)
      -> AssembledAudioBlock

## Socket audio pipeline step 11 — audio packet-to-block processing

After downstream packet processing, duplicate merge, and reorder, the Socket audio backend receives ordered AudioStoredPacket objects.

The audio backend converts each ordered packet back into an AudioPacketView and passes it to the audio frame assembler:

    ordered AudioStoredPacket
      -> AudioPacketView
      -> AudioFrameAssembler::push(...)

This step is audio-specific media assembly.

Unlike the Socket video path, the Socket audio path does not use a separate AudioReceivePipeline object.

AudioFrameAssembler performs:

    AudioPacketView
      -> create AudioBuffer
      -> decode PCM wire samples
      -> write interleaved int32 samples
      -> emit AssembledAudioBlock

The AudioBuffer is built from packet-signaled/runtime-carried parameters:

    sampling_rate_hz
    channel_count
    samples_per_channel

Supported PCM wire sample decoding:

    L16
      -> 2 bytes per sample
      -> signed int32 sample

    L24
      -> 3 bytes per sample
      -> signed int32 sample

Each accepted RTP audio packet currently becomes one complete AssembledAudioBlock.

The assembled block carries:

    AudioBuffer
    RTP timestamp
    receive timestamp
    RTP sequence number
    RTP marker
    complete = true

This step must not:

    receive UDP datagrams
    parse SDP
    choose backend
    open sockets
    perform source filtering
    perform duplicate merge
    reorder packets
    deliver directly from receive threads

Result of this step:

    AssembledAudioBlock

The next Socket audio step is audio block delivery to the configured sink:

    AssembledAudioBlock
      -> IFrameSink::on_audio_frame(...)

## Socket audio pipeline step 12 — assembled audio block delivery to sink

After audio packet-to-block processing, the Socket audio backend has an AssembledAudioBlock.

Each assembled block is delivered through the backend sink contract:

    AssembledAudioBlock
      -> SocketRxAudioBackend::deliver_assembled_audio_block(...)
      -> IFrameSink::on_audio_frame(...)

The delivered audio data contains:

    AudioBuffer
    RTP timestamp
    receive timestamp

Delivery flow:

    AssembledAudioBlock.buffer
      -> moved into IFrameSink::on_audio_frame(...)

    AssembledAudioBlock timing
      -> FrameTimingMetadata
           rtp_timestamp
           receive_timestamp_ns

IFrameSink is the boundary between the Socket backend and the higher OBS-facing layer.

The Socket backend must not know how OBS consumes, schedules, synchronizes, or plays the audio block.

This step must not:

    receive UDP datagrams
    parse RTP packets
    perform duplicate merge
    reorder packets
    assemble audio blocks
    perform AV synchronization
    render or play audio directly through OBS

Result of this step:

    assembled AudioBuffer delivered to the configured IFrameSink
    with FrameTimingMetadata attached

This completes the current Socket audio receive path.