### libs/st2110core/CMakeLists.txt
- Роль:
    - build integration для `st2110core`.
    - теперь собирает общий socket single-media runtime implementation отдельно от header-only concrete socket backend’ов.
- Связи:
    - собирает:
        - `src/stub.cpp`;
        - `src/socket_rx_single_media_backend_base.cpp`.
    - public headers concrete socket backend’ов (`socket_rx_video_backend.hpp`, `socket_rx_audio_backend.hpp`) остаются header-only и линкуются через `st2110core` include surface.
- Примечание:
    - platform-default socket port factory selection больше не живет в concrete video backend `.cpp`;
    - общий runtime/default-factory слой локализован в `socket_rx_single_media_backend_base.cpp`.

### apps/st2110_rx_dump/main.cpp
- Роль:
    - минимальный CLI entry point для будущего dump/tool-приложения.
    - пока выполняет роль buildable stub.
- Связи:
    - собирается в `st2110_rx_dump`;
    - линкуется со `st2110core`, но пока фактически core API не использует.
- Сущности:
    - `main()` — временная заглушка, печатает имя приложения.

### libs/st2110core/src/stub.cpp
- Роль:
    - временная `.cpp` единица для сборки статической библиотеки `st2110core`.
- Связи:
    - архитектурного значения не имеет;
    - может быть удалена/заменена по мере появления реальных `.cpp` файлов.
- Сущности:
    - `stub()`