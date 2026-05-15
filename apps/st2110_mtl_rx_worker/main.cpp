#include "mtl_worker_process_state.hpp"

#include <st2110/backends/mtl/mtl_worker_ipc_codec.hpp>
#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>

#include <unistd.h>

#include <iostream>

namespace {

[[nodiscard]] st2110::MtlWorkerControlEvent make_worker_decode_error(st2110::Error error, const char *message) {
    return st2110::MtlWorkerErrorEvent{
        .request_id = 0,
        .graph_id = 0,
        .error = error,
        .message = message ? message : "",
    };
}

} // namespace

int main() {
    /*
     * stdout/stdin are reserved for framed control IPC.
     * stderr is diagnostic-only.
     */
    std::cerr << "st2110_mtl_rx_worker started\n";

    st2110_mtl_rx_worker::MtlWorkerProcessState state{};

    while (!state.shutdown_requested()) {
        auto frame = st2110::read_mtl_worker_control_frame(STDIN_FILENO);
        if (!frame.has_value()) {
            std::cerr << "MTL worker control frame read failed\n";
            return 1;
        }

        auto request = st2110::deserialize_mtl_worker_control_request(*frame);

        st2110::MtlWorkerControlEvent event =
            request.has_value()
                ? state.handle(*request)
                : make_worker_decode_error(request.error(), "Failed to decode MTL worker control request");

        auto response = st2110::serialize_mtl_worker_control_event(event);
        if (!response.has_value()) {
            std::cerr << "MTL worker control event serialization failed\n";
            return 1;
        }

        auto wrote = st2110::write_mtl_worker_control_frame(STDOUT_FILENO, *response);
        if (!wrote.has_value()) {
            std::cerr << "MTL worker control frame write failed\n";
            return 1;
        }
    }

    std::cerr << "st2110_mtl_rx_worker stopped\n";
    return 0;
}