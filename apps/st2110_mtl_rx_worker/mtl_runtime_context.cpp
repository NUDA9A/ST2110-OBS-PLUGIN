#include "mtl_runtime_context.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

#ifndef ST2110_MTL_DEV_KERNEL_SOCKET
#define ST2110_MTL_DEV_KERNEL_SOCKET 0
#endif

namespace st2110_mtl_rx_worker {
namespace {

[[nodiscard]] constexpr mtl_pmd_type default_mtl_pmd() noexcept {
#if ST2110_MTL_DEV_KERNEL_SOCKET
    return MTL_PMD_KERNEL_SOCKET;
#else
    return MTL_PMD_DPDK_USER;
#endif
}

void fill_mtl_runtime_port(mtl_init_params &params, const enum mtl_port port_index,
                           const st2110::MtlRuntimePortConfig &port_cfg) {
    std::snprintf(params.port[port_index], MTL_PORT_MAX_LEN, "%s", port_cfg.port_name.c_str());

    params.pmd[port_index] = default_mtl_pmd();
    params.net_proto[port_index] = MTL_PROTO_STATIC;

    /*
     * RX worker runtime needs RX queues. TX queues are not needed for receive.
     *
     * Later, queue counts may become explicit worker runtime settings if video
     * and audio sessions need independent queues.
     */
    params.tx_queues_cnt[port_index] = 0;
    params.rx_queues_cnt[port_index] = 1;

    std::memcpy(params.sip_addr[port_index], port_cfg.sip_addr.data(), port_cfg.sip_addr.size());
}

[[nodiscard]] std::expected<mtl_init_params, st2110::Error>
make_mtl_init_params(const st2110::MtlRuntimeConfig &cfg) {
    mtl_init_params params{};

    params.num_ports = cfg.redundant_port.has_value() ? 2 : 1;

    fill_mtl_runtime_port(params, MTL_PORT_P, cfg.primary_port);

    if (cfg.redundant_port.has_value()) {
        fill_mtl_runtime_port(params, MTL_PORT_R, *cfg.redundant_port);
    }

    params.flags |= MTL_FLAG_DEV_AUTO_START_STOP;

    return params;
}

} // namespace

struct MtlRuntimeContext::Impl {
    st2110::MtlRuntimeConfig cfg{};
    mtl_handle mt = nullptr;

    explicit Impl(st2110::MtlRuntimeConfig runtime_cfg, mtl_handle runtime_handle)
        : cfg(std::move(runtime_cfg)), mt(runtime_handle) {}

    ~Impl() {
        if (mt) {
            mtl_uninit(mt);
            mt = nullptr;
        }
    }
};

std::expected<std::unique_ptr<MtlRuntimeContext>, st2110::Error>
MtlRuntimeContext::create(st2110::MtlRuntimeConfig cfg) {
    auto params = make_mtl_init_params(cfg);
    if (!params.has_value()) {
        return std::unexpected(params.error());
    }

    mtl_handle mt = mtl_init(&*params);
    if (!mt) {
        return std::unexpected(st2110::Error::SystemFailure);
    }

    auto impl = std::make_unique<Impl>(std::move(cfg), mt);
    return std::unique_ptr<MtlRuntimeContext>(new MtlRuntimeContext(std::move(impl)));
}

MtlRuntimeContext::MtlRuntimeContext(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

MtlRuntimeContext::~MtlRuntimeContext() = default;

const st2110::MtlRuntimeConfig &MtlRuntimeContext::config() const noexcept {
    return impl_->cfg;
}

mtl_handle MtlRuntimeContext::handle() const noexcept {
    return impl_->mt;
}

} // namespace st2110_mtl_rx_worker