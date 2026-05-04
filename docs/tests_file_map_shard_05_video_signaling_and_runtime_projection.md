### tests/test_video_signaling.cpp
- Роль:
    - проверяет modeled `VideoStreamSignaling` validation.
- Покрывает:
    - valid progressive `GPM` signaling;
    - valid `BPM` signaling at the structural signaling layer;
    - standards-clean `SSN` requirement:
        - missing `SSN` rejected by `validate_video_stream_signaling(...)`;
        - existing valid signaling with explicit `SSN` remains accepted.
    - signaled dimension limits in the signaling/media-description boundary:
        - width/height `1` accepted structurally;
        - width/height `32767` accepted structurally;
        - width/height `0` rejected structurally;
        - width/height `32768` rejected structurally.
    - invalid frame rate;
    - signaling-valid but runtime-invalid odd-width projection case;
    - finalized `MAXUDP` signaling policy:
        - Standard UDP Size Limit accepted;
        - Extended UDP Size Limit accepted;
        - non-boundary numeric values rejected;
        - values above Extended UDP Size Limit rejected;
        - packet-parse-policy derivation from signaling for Standard / Extended values;
        - absent `MAXUDP` keeps empty policy override and therefore Standard-by-default behavior.
    - signaling-valid but runtime-unsupported sampling projection case.
    - tightened ST 2110-20 `SSN` cross-field validation:
        - `BT709 + SDR + SSN=ST2110-20:2017` accepted;
        - `BT709 + SDR + SSN=ST2110-20:2022` rejected;
        - `ALPHA`/KEY signaling requires `SSN=ST2110-20:2022`;
        - `TCS=ST2115LOGS3` requires `SSN=ST2110-20:2022`.
    - localized runtime support boundary:
        - structurally valid odd-width signaling still fails only through runtime UYVY projection/config validation, not in signaling dimension-limit validation;
        - structurally valid `ALPHA`/KEY signaling remains runtime-unsupported only through `pixel_format_from_video_stream_signaling(...)`.

### tests/test_video_signaling_rx_match.cpp
- Роль:
    - проверяет consistency между signaling model и manual `RxVideoConfig`.
- Покрывает:
    - matching signaling and `RxVideoConfig` accepted;
    - width / height / frame-rate / scan-mode mismatch rejected;
    - missing `SSN` in signaling rejected before RX-match consistency logic.

### tests/test_video_signaling_to_rx_config.cpp
- Роль:
    - проверяет projection from `VideoStreamSignaling` to `RxVideoConfig`.
- Покрывает:
    - valid standards-clean signaling projects successfully;
    - invalid signaling rejected before projection;
    - missing `SSN` rejected before projection;
    - invalid transport arguments rejected after signaling projection;
    - scan mode is preserved structurally;
    - invalid payload type rejected;
    - structurally valid but unsupported runtime pixel-format mapping rejected as `Unsupported`.

### tests/test_video_signaling_to_pipeline_config.cpp
- Роль:
    - проверяет projection from `VideoStreamSignaling` to runtime video receive pipeline config.
- Покрывает:
    - depacketizer config projection;
    - reconstructor config projection;
    - composed pipeline config projection;
    - structural preservation of interlaced scan mode in projection helpers;
    - invalid signaling rejected before runtime projection;
    - missing `SSN` rejected before runtime projection;
    - structurally valid but runtime-unmappable media rejected as `Unsupported`.

### tests/test_video_receiver_bootstrap.cpp
- Роль:
    - проверяет generic signaling-driven receiver bootstrap composition.
- Покрывает:
    - successful composition of packet-parse policy, RX config, and receive-pipeline config from valid signaling;
    - absent `MAXUDP` override preserved through bootstrap policy projection;
    - invalid signaling rejected before downstream bootstrap projection;
    - missing `SSN` rejected before downstream bootstrap projection;
    - invalid transport inputs rejected after signaling-derived projection;
    - structurally valid but runtime-unmappable signaling rejected as `Unsupported`.

### tests/test_video_packing_mode_runtime_projection.cpp
- Роль:
    - проверяет runtime projection/support boundary для `VideoPackingMode`.
- Покрывает:
    - `GPM` projection into depacketizer / receive-pipeline / bootstrap configs;
    - `BPM` remains structurally valid in signaling model;
    - `BPM` remains rejected by current runtime projection/support boundaries as `Unsupported`;
    - missing `SSN` is rejected before packing-mode runtime projection is attempted.

### tests/test_video_signaled_media_properties.cpp
- Роль:
    - проверяет modeled video SDP/media properties separate from runtime `PixelFormat`.
- Покрывает:
    - validation of token-backed signaling/media enums:
        - `sampling`;
        - `colorimetry`;
        - `TCS`;
        - `SSN`;
        - `RANGE`.
    - structural `VideoBitDepth` validation including `16f`;
    - structural `VideoPixelAspectRatio` validation:
        - `1:1` accepted;
        - non-square ratios such as `12:11` accepted;
        - zero parts rejected.
    - signaling/media-description dimension validation:
        - explicit helper-level acceptance of `1` and `32767`;
        - rejection of `0` and `32768`;
        - stream-level acceptance of signaled min/max dimensions;
        - stream-level rejection of out-of-range dimensions.
    - standards-clean media-description behavior:
        - valid BT709/SDR media-description with explicit `SSN=ST2110-20:2017` accepted;
        - missing `SSN` rejected;
        - optional `TCS` and `RANGE` may still be absent where currently allowed.
    - rejection of invalid structural media fields;
    - tightened `SSN` cross-field validation;
    - tightened `RANGE` modeling and cross-field validation.
    - PAR/pixel-aspect-ratio as a signaling/media-description property:
        - non-square PAR accepted structurally;
        - invalid PAR rejected structurally;
        - runtime projection remains independent from PAR.
    - runtime projection remains separate:
        - valid `YCbCr422 + 8-bit` projects to `UYVY`;
        - structurally valid but unsupported media, including KEY/ALPHA, remain rejected only by runtime projection.
- Фиксирует:
    - `SSN` is no longer treated as an optional signaling/media-description field in standards-clean validation;
    - current UYVY-specific even-width runtime constraint still remains localized below the signaling boundary.

### tests/test_video_reference_clock.cpp
- Роль:
    - проверяет modeled `ReferenceClock` validation on the signaling boundary.
- Покрывает:
    - valid PTP reference clock with non-zero clock identity;
    - valid traceable PTP form with zero clock identity allowed only when `traceable=true`;
    - rejection of invalid modeled PTP shapes:
        - missing payload;
        - mixed `Ptp` + `LocalMac`;
        - unexpected raw token;
        - all-zero non-traceable clock identity.
    - valid localmac reference clock with non-zero MAC;
    - rejection of invalid modeled localmac shapes:
        - missing payload;
        - all-zero MAC;
        - mixed `LocalMac` + `Ptp`.
    - explicit `Other + raw_token` support for unknown/open-ended forms;
    - propagation of reference-clock validation through `validate_video_stream_signaling(...)`;
    - missing `SSN` rejection remains above/reference-clock-using signaling validation paths.
- Фиксирует:
    - strict modeled validation of known reference-clock forms remains localized on the signaling boundary;
    - standards-clean signaling still requires explicit `SSN` in addition to a valid reference clock.

### tests/test_video_sender_signaling.cpp
- Роль:
    - проверяет sender timing signaling fields:
        - sender type;
        - `TROFF`;
        - `CMAX`;
        - structural validation.
- Покрывает:
    - direct sender-field validation for `Narrow`, `NarrowLinear`, and `Wide`;
    - corrected ST 2110-21 optional-parameter semantics:
        - `TROFF` is allowed for `Narrow`, `NarrowLinear`, and `Wide` when present and positive;
        - `CMAX` is allowed for `Narrow`, `NarrowLinear`, and `Wide` when present and valid for the local modeled policy;
        - absent optional sender parameters remain accepted;
        - `TROFF=0` rejected;
        - `CMAX=0` rejected.
    - stream-level signaling validation for valid and invalid sender-timing cases;
    - missing `SSN` rejection remains explicit and happens before stream-level sender-timing acceptance is treated as standards-clean signaling.
- Фиксирует:
    - generic signaling validation no longer hardcodes a stricter sender-class policy than ST 2110-21 optional-parameter semantics;
    - stricter receiver/conformance checks must remain outside this generic signaling-validation boundary.

### tests/test_video_timing_signaling.cpp
- Роль:
    - проверяет timing-related signaling:
        - media clock mode;
        - timestamp mode;
        - timing validation.
