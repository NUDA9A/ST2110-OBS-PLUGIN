#ifndef ST2110_OBS_PLUGIN_SOURCE_RUNTIME_HPP
#define ST2110_OBS_PLUGIN_SOURCE_RUNTIME_HPP

#include <obs_st2110/source_config.hpp>

#include <obs-module.h>

#include <cstdint>
#include <memory>
#include <string>

namespace obs_st2110 {

class SourceRuntime {
public:
    explicit SourceRuntime(obs_source_t *source);
    ~SourceRuntime();

    SourceRuntime(const SourceRuntime &) = delete;
    SourceRuntime &operator=(const SourceRuntime &) = delete;

    SourceRuntime(SourceRuntime &&) = delete;
    SourceRuntime &operator=(SourceRuntime &&) = delete;

    void update(const SourceConfig &config);

    void start();
    void stop() noexcept;

    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] const std::string &last_error() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_SOURCE_RUNTIME_HPP