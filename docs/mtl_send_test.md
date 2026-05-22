#### `mtl_send_test`

`mtl_send_test` — это утилита для генерации тестового ST 2110 source-а через MTL TX и публикации SDP через NDI metadata. Она нужна, чтобы OBS plugin мог обнаружить тестовый источник через NDI discovery и затем принять media по Socket или MTL backend-у.

Основные возможности:

```text
1. Публикация NDI source-а с ST 2110 SDP metadata
```

Утилита создает NDI sender и периодически публикует metadata block:

```xml
<st2110_sdp_bundle>
  <st2110_sdp media="video" ...>
  <st2110_sdp media="audio" ...>
</st2110_sdp_bundle>
```

Именно это потом читает `discovery_provider`.

---

```text
2. Режимы media
```

Поддерживаются:

```text
--media video  → только ST 2110-20 video
--media audio  → только ST 2110-30 audio
--media av     → video + audio, default
```

---

```text
3. Video TX через MTL ST20P
```

Для video используется MTL `st20p_tx`.

Поддерживаемые video modes:

```text
1080p60 → 1920x1080 60 fps
720p30  → 1280x720 30 fps
360p30  → 640x360 30 fps
```

Формат сейчас фиксированный:

```text
UYVY
YCbCr 4:2:2 8-bit
BT.709
narrow range
```

Тестовая картинка — animated UYVY gradient.

---

```text
4. Audio TX через MTL ST30P
```

Для audio используется MTL `st30p_tx`.

Поддерживается:

```text
48 kHz
1 ms packet time
PCM16 или PCM24
1..8 channels
```

Payload — синусоидальный tone, по умолчанию `440 Hz`.

---

```text
5. Primary и redundant legs
```

По умолчанию создается один primary stream.

С `--duplicate` включается redundant topology:

```text
a=group:DUP primary redundant
```

Тогда создаются два MTL TX leg-а:

```text
primary port / local IP / destination IP / UDP port
redundant port / local IP / destination IP / UDP port
```

Это полезно для проверки DUP topology и receive-side redundant handling.

---

```text
6. Metadata-only режим
```

Опция:

```text
--metadata-only
```

В этом режиме утилита публикует только NDI SDP metadata, но не запускает MTL TX.

Это удобно для проверки:

```text
NDI discovery
SDP extraction
SDP media selection
SDP parser dispatch
receive graph construction errors
```

без реального media sender-а.

---

```text
7. Настраиваемые transport параметры
```

Можно задавать:

```text
--port-name
--local-ip
--video-dst-ip
--video-udp-port
--video-payload-type
--audio-dst-ip
--audio-udp-port
--audio-payload-type
--packing gpm|bpm
--pmd dpdk-user|kernel-socket|native-af-xdp|dpdk-af-xdp|dpdk-af-packet
--frame-buffer-count
```

Для redundant leg-а отдельно:

```text
--redundant-port-name
--redundant-local-ip
--redundant-video-dst-ip
--redundant-video-udp-port
--redundant-audio-dst-ip
--redundant-audio-udp-port
```

---

```text
8. Runtime control
```

Можно задавать:

```text
--duration-ms
--repeat-ms
--name
```

То есть source name, длительность работы и частоту повторной публикации NDI metadata.

---

##### Кратко

`mtl_send_test` умеет:

```text
NDI metadata discovery source
+ generated ST 2110-20 video SDP
+ generated ST 2110-30 audio SDP
+ optional real MTL ST20P/ST30P TX
+ optional redundant DUP topology
+ metadata-only режим
```

Это тестовый генератор для end-to-end проверки:

```text
mtl_send_test
→ NDI metadata discovery
→ plugin source list
→ SDP parser
→ receive graph
→ Socket/MTL RX backend
→ OBS output
```