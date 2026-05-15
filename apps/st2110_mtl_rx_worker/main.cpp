#include "mtl_worker_event_writer.hpp"
#include "mtl_worker_process_state.hpp"

#include <st2110/backends/mtl/mtl_worker_ipc_codec.hpp>
#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>

#include <unistd.h>

#include <iostream>

int main() {
    /*
     * stdout/stdin are reserved for framed control IPC.
     * stderr is diagnostic-only.
     */
    std::cerr << "st2110_mtl_rx_worker started\n";

    st2110_mtl_rx_worker::MtlWorkerProcessState state{};
    st2110_mtl_rx_worker::MtlWorkerEventWriter event_writer{STDOUT_FILENO};

    while (!state.shutdown_requested()) {
        auto frame = st2110::read_mtl_worker_control_frame_with_fds(STDIN_FILENO);
        if (!frame.has_value()) {
            std::cerr << "MTL worker control frame read failed\n";
            return 1;
        }

        auto request = st2110::deserialize_mtl_worker_control_request(frame->payload());
        if (!request.has_value()) {
            std::cerr << "MTL worker control request decode failed\n";
            return 1;
        }

        st2110::MtlWorkerControlEvent event = state.handle(*request, frame->file_descriptors());

        auto wrote = event_writer.write_event(event);
        if (!wrote.has_value()) {
            std::cerr << "MTL worker control event write failed\n";
            return 1;
        }
    }

    std::cerr << "st2110_mtl_rx_worker stopped\n";
    return 0;
}