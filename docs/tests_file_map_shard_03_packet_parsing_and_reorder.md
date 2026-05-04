### tests/test_rtp_parser.cpp
- Роль:
    - проверяет RTP header parsing:
        - version;
        - minimum header size;
        - payload type;
        - marker;
        - sequence number;
        - timestamp;
        - SSRC;
        - payload offset / payload length.

### tests/test_rtp_seq.cpp
- Роль:
    - проверяет RTP sequence wrap comparison/distance helpers.

### tests/test_rtp_payload.cpp
- Роль:
    - проверяет RTP payload span extraction;
    - покрывает CSRC/header-offset behavior.

### tests/test_st2110_20_types.cpp
- Роль:
    - проверяет базовые ST 2110-20 payload header structs/types.

### tests/test_st2110_20_parse.cpp
- Роль:
    - проверяет parsing ST 2110-20 payload header:
        - extended sequence high bits;
        - SRD headers;
        - segment count / header size.

### tests/test_st2110_20_validate.cpp
- Роль:
    - проверяет structural validation ST 2110-20 payload header.

### tests/test_extended_seq.cpp
- Роль:
    - проверяет `combine_extended_seq(...)`.

### tests/test_zero_length_srd.cpp
- Роль:
    - regression coverage для стандартного zero-length SRD special-case.

### tests/test_st2110_20_ordering.cpp
- Роль:
    - проверяет packet-local SRD ordering rules:
        - row number ordering;
        - offset ordering within row.

### tests/test_packet_view.cpp
- Роль:
    - проверяет normalized `PacketView` contract.

### tests/test_packet_view_parse.cpp
- Роль:
    - проверяет full UDP datagram -> RTP/ST2110-20 `PacketView` parsing path.

### tests/test_packet_view_trailing_padding.cpp
- Роль:
    - проверяет generic extraction of trailing payload bytes after SRD-covered data.

### tests/test_packet_parse_stats.cpp
- Роль:
    - проверяет packet parse stats structs/counters.

### tests/test_packet_parse_policy.cpp
- Роль:
    - проверяет packet-size policy boundary:
        - UDP datagram size semantics;
        - absent `MAXUDP` defaulting to Standard UDP Size Limit;
        - acceptance of explicit Standard / Extended limits only;
        - oversize rejection before wire parse.
- Покрывает:
    - `udp_datagram_size_bytes(...)` adds UDP header bytes;
    - absent policy override => effective Standard UDP Size Limit;
    - explicit Standard UDP Size Limit accepted and enforced;
    - explicit Extended UDP Size Limit accepted and enforced;
    - non-boundary numeric policy values rejected by config validation;
    - oversized packet rejected at packet-policy stage before wire parsing;
    - invalid policy config rejected before packet checks.

### tests/test_packet_parse_integration_stats.cpp
- Роль:
    - проверяет integrated packet parse path with stage-specific stats recording.

### tests/test_reorder_buffer_interface.cpp
- Роль:
    - проверяет `IReorderBuffer` abstraction, `StoredPacket` ownership/view restoration, explicit gap-flush hook, and reorder stats snapshot boundary.
- Покрывает:
    - abstract/interface shape:
        - `push(...)`;
        - `pop_next()`;
        - `flush_missing_once()`;
        - `stats() -> ReorderBufferStats`;
        - `reset()`.
    - `StoredPacket::view()`:
        - RTP header field restoration;
        - extended sequence restoration;
        - SRD header restoration;
        - payload segment view reconstruction.
    - fake reorder-buffer behavior:
        - push/pop lifecycle;
        - blocked-head / missing-once accounting;
        - single-step gap flush unblocks the head once;
        - reset behavior;
        - zero-after-reset stats behavior.
- Фиксирует:
    - generic reorder interface now exposes both stats and an explicit one-step gap-flush boundary;
    - backend/runtime code can stay on the abstraction boundary without depending on concrete reorder-buffer types.

### tests/test_fixed_reorder_buffer.cpp
- Роль:
    - проверяет fixed-window reorder behavior by extended sequence number.

### tests/test_fixed_reorder_buffer_stats.cpp
- Роль:
    - проверяет reorder stats:
        - duplicates;
        - out-of-window;
        - late/missing accounting.

### tests/test_fixed_reorder_buffer_flush.cpp
- Роль:
    - проверяет missing sequence flush behavior.

### tests/test_video_packet_admission.cpp
- Роль:
    - regression tests для explicit RTP payload-type admission boundary in the video receive path.
    - проверяет, что payload-type admission remains separate from generic RTP/ST 2110-20 parsing and that wrong-PT packets are ignored before reorder/depacketizer use.
- Покрывает:
    - helper-level admission boundary:
        - matching dynamic RTP payload type accepted by `validate_rtp_payload_type_admission(...)`;
        - matching video packet accepted by `validate_video_packet_payload_type_admission(...)`;
        - mismatching payload type rejected as `InvalidValue`.
    - separation of concerns:
        - generic `PacketView` parsing still succeeds for structurally valid RTP/ST 2110-20 packets regardless of whether the payload type matches the configured stream;
        - payload-type admission remains a separate stream-specific boundary above generic packet parsing.
    - runtime backend behavior with the new operational start boundary:
        - `SocketRxVideoBackend` is started from `SocketRxVideoOperationalConfig`, not manual `RxVideoConfig`;
        - wrong-payload-type packet is treated as non-media datagram and dropped locally;
        - wrong-PT packet does not enter reorder/depacketizer state;
        - no frame is delivered to the sink;
        - backend stats record:
            - one received datagram;
            - one ignored non-media datagram;
            - zero parsed-ok media packets;
            - zero rejected media packets;
            - zero depacketizer/reorder activity.
- Фиксирует:
    - payload-type admission is an explicit receive-path boundary distinct from generic RTP parsing;
    - wrong-PT packets are ignored before reorder/depacketizer mutation even after the backend’s move to the operational-only socket start API.