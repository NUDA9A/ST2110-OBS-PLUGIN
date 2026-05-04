### tests/video_sdp_media_section_test.cpp
- Роль:
    - проверяет raw SDP video media-section selection boundary in `video_sdp_media_section.hpp`;
    - проверяет payload-bound `rtpmap` / `fmtp`;
    - проверяет preservation of raw media-line and unknown attributes;
    - проверяет tightened raw `m=video` validation for ST 2110 video SDP.
- Покрывает:
    - selection of the correct `m=video` section by expected payload type;
    - dynamic RTP payload-type requirement for selected video SDP;
    - media-line protocol validation;
    - preservation of raw `rtpmap`, `fmtp`, and unknown media attributes without prematurely collapsing them into final signaling;
    - preservation of scoped standalone timing/reference-clock attributes separately from fmtp media parameters.
- Фиксирует:
    - raw media-section parsing remains a non-destructive SDP boundary;
    - final requirements such as mandatory `TP` for standards-clean video SDP ingestion remain above this raw selection layer.

### tests/video_sdp_fmtp_test.cpp
- Роль:
    - проверяет strict raw parser for video `a=fmtp`.
- Покрывает:
    - parsing of required and optional known fmtp parameters;
    - preservation of unknown syntactically valid parameters;
    - canonical `exactframerate` parsing:
        - integer `25` accepted;
        - non-canonical integer rational `25/1` rejected;
        - canonical rational `30000/1001` accepted;
        - reducible rational such as `60000/2002` rejected;
        - zero numerator/denominator rejected;
        - malformed `exactframerate` rejected;
        - existing valid SDP examples using `30000/1001` remain accepted.
    - `PAR` parsing:
        - valid `PAR=1:1`;
        - valid non-square `PAR=12:11`;
        - canonical/minimal reduction where practical (for example `2:2 -> 1:1`);
        - malformed `PAR` rejection;
        - duplicate `PAR` rejection.
    - attribute-level parsing for matching / non-matching payload types;
    - malformed numeric and duplicate known-parameter rejection.
- Фиксирует:
    - raw fmtp parser keeps absent `PAR` as absent raw property;
    - signaling-level defaulting to `1:1` is left to later mapping/model layers;
    - `exactframerate` canonical form is enforced locally in the raw fmtp parser rather than in runtime cadence/timestamp logic.

### tests/video_sdp_signaling_adapter_test.cpp
- Роль:
    - проверяет adapter from raw SDP/fmtp media-description fields to `VideoStreamSignaling`.
- Покрывает:
    - mapping of known progressive video fmtp fields into explicit signaling enums/values;
    - default signaling-model `PAR=1:1` when raw `PAR` is absent;
    - mapping of explicit square `PAR=1:1`;
    - mapping of non-square `PAR=12:11`;
    - mapping of `FULLPROTECT` into `VideoRange::Known::FullProtect`;
    - interlaced / PsF scan-mode derivation;
    - packing-mode mapping;
    - rejection of malformed scan-mode / packing-mode combinations;
    - preservation of unknown future tokens through `Other + raw_token`, including unknown `RANGE` tokens;
    - rejection of malformed depth outside the signaling model.
- Фиксирует:
    - SDP-to-signaling adapter preserves PAR as a media-description property;
    - runtime/storage behavior remains outside this mapping boundary.

### tests/video_sdp_timing_attributes_test.cpp
- Роль:
    - проверяет raw parsing of SDP timing/reference/sender attributes.
- Покрывает:
    - parsing of known `ts-refclk` forms:
        - `ptp=IEEE1588-2008:<gmid>[:domain]`
        - `ptp=IEEE1588-2008:traceable`
        - `localmac=<EUI-48 MAC>`
    - preservation of unknown/open-ended reference-clock forms through `Other`;
    - rejection of malformed known `ptp` / `localmac` forms:
        - malformed PTP GMID;
        - malformed PTP domain;
        - malformed localmac.
    - parsing of known `mediaclk`, `TSMODE`, `TSDELAY`, `TP`, `TROFF`, `CMAX`;
    - session/media scoped timing resolution and media-over-session precedence;
    - helper-level detection of presence of resolved reference clock and media-level `mediaclk`.
- Фиксирует:
    - strict known reference-clock parsing is localized in the raw timing parser;
    - malformed known forms do not silently fall through as unknown/open-ended forms.

### tests/video_sdp_rtpmap_test.cpp
- Роль:
    - проверяет raw SDP `a=rtpmap` parsing/binding for selected payload type.

### tests/video_sdp_ingestion_test.cpp
- Роль:
    - проверяет final SDP-to-`VideoStreamSignaling` ingestion entry point and composition with raw media-section parsing.
- Покрывает:
    - full valid video SDP ingestion into signaling;
    - composition equivalence between raw-media-section path and final SDP entry point;
    - signaling-level default `PAR=1:1` when absent in SDP;
    - explicit square `PAR=1:1`;
    - non-square `PAR=12:11` surviving final SDP-to-signaling mapping;
    - malformed `PAR` rejection through final ingestion;
    - propagation of invalid RTP/timing/media errors from final ingestion;
    - final-ingestion requirement for:
        - `ts-refclk`
        - media-level `mediaclk`
    - acceptance of valid reference-clock forms:
        - PTP GMID form
        - PTP traceable form
        - localmac form
    - rejection of missing or malformed reference-clock signaling.
- Фиксирует:
    - final video SDP ingestion is standards-clean only when both required timing-clock attributes are present in the accepted form;
    - runtime video projection/bootstrapping behavior remains separate from SDP timing/reference parsing.

### tests/video_sdp_fmtp_timing_parameters_test.cpp
- Роль:
    - проверяет known ST 2110 timing/sender parameters inside `a=fmtp`:
        - TP;
        - TROFF;
        - CMAX;
        - TSMODE;
        - TSDELAY.

### tests/video_sdp_maxudp_parameters_test.cpp
- Роль:
    - проверяет parsing/mapping of `MAXUDP` from SDP `a=fmtp`;
    - проверяет propagation into signaling and packet parse policy;
    - проверяет финальную policy semantics for absent / Standard / Extended values.
- Покрывает:
    - raw fmtp parser extracts `MAXUDP` as known parameter rather than leaving it in unknown parameters;
    - duplicate `MAXUDP` rejected;
    - malformed numeric `MAXUDP` rejected in raw fmtp parsing;
    - final SDP ingestion maps Standard UDP Size Limit `MAXUDP` into signaling;
    - final SDP ingestion maps Extended UDP Size Limit `MAXUDP` into signaling;
    - packet parse policy receives the final Standard / Extended effective limit from signaling;
    - absent `MAXUDP` preserves empty signaling/policy override and therefore Standard-by-default behavior;
    - final SDP ingestion rejects non-boundary numeric `MAXUDP` values via signaling validation;
    - final SDP ingestion rejects values above Extended UDP Size Limit via signaling validation.

### tests/video_sdp_depth_16f_test.cpp
- Роль:
    - проверяет SDP `depth=16f` parsing and signaling representation.

### tests/video_sdp_media_property_enum_coverage_test.cpp
- Роль:
    - проверяет explicit enum coverage for known ST 2110-20 SDP media-property tokens;
    - проверяет `Other + raw_token` для unknown future tokens.
- Покрывает:
    - explicit enum mapping for known `sampling` tokens;
    - explicit enum mapping for known `colorimetry` tokens;
    - explicit enum mapping for known `TCS` tokens;
    - explicit enum mapping for known `RANGE` tokens:
        - `NARROW`;
        - `FULLPROTECT`;
        - `FULL`;
    - preservation of unknown future `sampling` / `colorimetry` / `TCS` / `RANGE` tokens through `Other + raw_token`;
    - separation between explicit signaling-model enum coverage and runtime `PixelFormat` projection support.
- Фиксирует:
    - `FULLPROTECT` is now a first-class known `VideoRange` value rather than a fallback `Other` token;
    - unsupported runtime projection still remains localized outside the enum-mapping boundary.

### tests/video_sdp_optional_sender_timing_test.cpp
- Роль:
    - проверяет relaxed receiver-side optional sender timing validation for SDP ingestion.

### tests/video_sdp_transport_boundary_test.cpp
- Роль:
    - проверяет preservation boundary for raw SDP transport/redundancy metadata around the selected video media section.
- Покрывает:
    - preservation of session-level and media-level `c=` data in the selected raw media section;
    - preservation of `mid`, `a=source-filter`, and `a=group:DUP`;
    - separate preservation of unknown session/media attributes;
    - proof that preserved raw transport metadata does not break final video signaling ingestion;
    - rejection of duplicate media/session connection-data lines;
    - rejection of duplicate `mid`;
    - proof that transport metadata from other media sections is not leaked into the selected video section.
- Фиксирует:
    - raw transport/redundancy SDP metadata remains separate from final `VideoStreamSignaling`;
    - detailed `c=` structural validation itself is covered separately by `video_sdp_connection_data_test.cpp`.

### tests/video_sdp_media_cross_field_validation_test.cpp
- Роль:
    - проверяет ST 2110-20 media-description cross-field validation:
        - progressive-only `4:2:0` variants;
        - `KEY + ALPHA`;
        - rejection of invalid KEY/TCS/colorimetry combinations;
        - tightened `SSN` cross-field rule;
        - tightened `RANGE` cross-field rule.
- Покрывает:
    - `4:2:0` accepted only with progressive scan signaling;
    - `KEY` requires `colorimetry=ALPHA` and forbids `TCS`;
    - `BT709 + SDR + SSN=ST2110-20:2017` accepted;
    - `BT709 + SDR + SSN=ST2110-20:2022` rejected;
    - `ALPHA` requiring `SSN=ST2110-20:2022`;
    - `TCS=ST2115LOGS3` requiring `SSN=ST2110-20:2022`;
    - `BT2100 + RANGE=FULL` accepted;
    - `BT2100 + RANGE=FULLPROTECT` rejected;
    - non-BT2100 `RANGE=FULLPROTECT` accepted;
    - absent `RANGE` remains accepted at signaling level;
    - unknown future `RANGE` token preserved through `Other + raw_token`.
    - the same cross-field rules through final SDP ingestion, not only manually constructed signaling objects;
    - unsupported runtime projection remaining localized after structurally valid KEY/ALPHA acceptance.

### tests/video_sdp_source_filter_scope_test.cpp
- Роль:
    - проверяет raw SDP `a=source-filter` grammar boundary in `video_sdp_media_section.hpp`;
    - проверяет preservation of parsed source-filter fields together with explicit session/media scope;
    - проверяет, что source-filter remains transport metadata and does not change final signaling/runtime behavior.
- Покрывает:
    - valid session-level source-filter parsing and preservation:
        - raw value;
        - scope;
        - filter mode;
        - nettype / addrtype;
        - destination address;
        - source address list.
    - valid media-level source-filter parsing and preservation.
    - simultaneous session-level + media-level source-filter parsing with preserved scope distinction.
    - invalid filter-mode rejection.
    - rejection of missing destination/source fields.
    - rejection of malformed packed source-list forms:
        - comma-separated packed addresses;
        - semicolon-separated packed addresses;
        - trailing packed separators inside one token.
    - final SDP ingestion/runtime boundary remains untouched:
        - valid SDP with source-filter still ingests into `VideoStreamSignaling`;
        - source-filter does not alter video media/signaling projection behavior.
- Фиксирует:
    - tightened source-filter grammar validation remains localized in the raw SDP media-section parser;
    - source-filter continues to live outside `VideoStreamSignaling` and outside backend/socket behavior in the current task.

### tests/video_sdp_redundancy_boundary_test.cpp
- Роль:
    - проверяет raw redundancy boundary:
        - `a=group:DUP`;
        - `a=mid`;
        - duplicate video media-section candidates.

### tests/video_sdp_fmtp_strict_parsing_test.cpp
- Роль:
    - проверяет strict SDP `a=fmtp` media-parameter parsing:
        - required separator whitespace;
        - whitespace around `=`;
        - doubled/trailing separators;
        - unknown syntactically valid parameters.

### tests/video_sdp_timing_scope_test.cpp
- Роль:
    - проверяет session/media scope resolution for standalone SDP timing/reference-clock attributes and their interaction with fmtp timing/sender media parameters.
- Покрывает:
    - preservation of session-level `ts-refclk` / `mediaclk` in the raw SDP model;
    - media-level override of session-level `ts-refclk` / `mediaclk`;
    - rejection of duplicate standalone timing attributes within the same scope;
    - explicit separation between:
        - standalone timing parsing in `video_sdp_timing_attributes.hpp`;
        - fmtp timing/sender parsing in `video_sdp_fmtp.hpp`;
        - final merge/conflict handling in `video_sdp_ingestion.hpp`.
    - `TSMODE` from fmtp overriding session-level standalone `tsmode`;
    - conflict rejection when fmtp timing field duplicates a media-level standalone timing field;
    - standards-clean final-ingestion requirement that `mediaclk` must be media-level;
    - existing media-level-only SDP timing behavior for `ts-refclk`, `mediaclk`, `tsmode`, `tsdelay`, `TROFF`, `CMAX`, and `TP=2110TPW`.
- Фиксирует:
    - `parse_video_sdp_timing_attributes(...)` parses standalone SDP timing attributes only;
    - `TP` from `a=fmtp` is not expected to appear inside the standalone raw timing model and is merged only at final SDP ingestion;
    - final signaling still carries the correct `sender_type` after fmtp merge.

### tests/video_sdp_connection_data_test.cpp
- Роль:
    - focused regression/acceptance coverage for raw SDP `c=` connection-data structural validation in `video_sdp_media_section.hpp`.
- Покрывает:
    - valid unicast connection-data parsing:
        - `c=IN IP4 192.0.2.10`;
    - valid IPv4 multicast parsing:
        - `c=IN IP4 239.1.1.1/32`;
        - `c=IN IP4 239.1.1.1/32/4`;
    - rejection of malformed raw connection-data structure:
        - empty base address;
        - empty TTL;
        - non-numeric TTL;
        - out-of-range TTL;
        - empty address count;
        - non-numeric address count;
        - zero address count;
        - too many slash parameters.
    - preservation of existing session/media `c=` behavior through `select_raw_video_sdp_media_section(...)`;
    - rejection of malformed session-level and media-level `c=` lines during media-section parsing.
- Фиксирует:
    - raw SDP `c=` remains transport metadata outside `VideoStreamSignaling`;
    - structural validation is now tighter, but session/media preservation behavior remains unchanged.
