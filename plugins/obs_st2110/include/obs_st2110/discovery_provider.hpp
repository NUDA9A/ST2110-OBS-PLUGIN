#ifndef ST2110_OBS_PLUGIN_DISCOVERY_PROVIDER_HPP
#define ST2110_OBS_PLUGIN_DISCOVERY_PROVIDER_HPP

#include <obs_st2110/source_config.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace obs_st2110 {

struct DiscoveredSourceListItem {
    std::string selection_key{};
    std::string display_name{};
};

class IDiscoveryProvider {
public:
    virtual ~IDiscoveryProvider() = default;

    [[nodiscard]] virtual std::vector<DiscoveredSourceListItem> list_sources() = 0;

    [[nodiscard]] virtual std::optional<SelectedDiscoveredSource>
    resolve_source(std::string_view selection_key) = 0;
};

[[nodiscard]] std::unique_ptr<IDiscoveryProvider> create_discovery_provider();

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_DISCOVERY_PROVIDER_HPP