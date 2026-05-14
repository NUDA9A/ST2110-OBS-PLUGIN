#include <obs_st2110/discovery_provider.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace obs_st2110 {
namespace {
class NullDiscoveryProvider final : public IDiscoveryProvider {
public:
    [[nodiscard]] std::vector<DiscoveredSourceListItem> list_sources() override { return {}; }

    [[nodiscard]] std::optional<SelectedDiscoveredSource> resolve_source(std::string_view selection_key) override {
        (void)selection_key;
        return std::nullopt;
    }
};
} // namespace

std::unique_ptr<IDiscoveryProvider> create_discovery_provider() {
    /*
     * Correct temporary provider.
     *
     * It models the provider boundary without inventing fake SDP or manual
     * debug input. Later this function becomes the factory for the real
     * NDI-backed discovery provider.
     */
    return std::make_unique<NullDiscoveryProvider>();
}

} // namespace obs_st2110