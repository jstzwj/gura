#include "gura/runtime/Freeze.h"

#include <stdexcept>

namespace gura::runtime {

extern "C" void* __gura_region_freeze(Region* region) {
  if (region == nullptr || region->state != RegionState::Closed) {
    throw std::runtime_error("freeze requires a closed region");
  }
  if (auto* header = __gura_header_for_payload(region->bridge)) {
    header->flags |= ObjectFlagFrozen;
  }
  region->bridgeToken.state = BridgeState::Frozen;
  return region->bridge;
}

} // namespace gura::runtime
