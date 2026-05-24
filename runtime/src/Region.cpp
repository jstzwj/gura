#include "gura/runtime/Region.h"

#include <atomic>
#include <cstdlib>
#include <stdexcept>

namespace gura::runtime {

namespace {
std::atomic<RegionId> nextRegionId{1};
}

extern "C" Region* __gura_region_new_iso(std::size_t bridgeSize) {
  auto* region = new Region();
  region->id = nextRegionId.fetch_add(1);
  region->state = RegionState::Closed;
  region->strategy = RegionStrategy::Arena;
  region->bridge = std::calloc(1, bridgeSize);
  if (region->bridge == nullptr) {
    delete region;
    throw std::bad_alloc();
  }
  region->allocations.push_back(region->bridge);
  return region;
}

extern "C" void __gura_region_enter(Region* region) {
  if (region == nullptr || region->state != RegionState::Closed) {
    throw std::runtime_error("cannot enter non-closed region");
  }
  region->state = RegionState::Active;
}

extern "C" void __gura_region_exit(Region* region, void* newBridge) {
  if (region == nullptr || region->state != RegionState::Active) {
    throw std::runtime_error("cannot exit non-active region");
  }
  if (newBridge != nullptr) {
    region->bridge = newBridge;
  }
  region->state = RegionState::Closed;
}

extern "C" void __gura_region_destroy(Region* region) {
  if (region == nullptr) {
    return;
  }
  for (void* allocation : region->allocations) {
    std::free(allocation);
  }
  delete region;
}

} // namespace gura::runtime
