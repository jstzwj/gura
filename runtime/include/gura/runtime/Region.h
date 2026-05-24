#pragma once

#include "gura/runtime/ObjectHeader.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace gura::runtime {

enum class RegionState {
  Active,
  Paused,
  Closed,
};

enum class RegionStrategy {
  Arena,
  RC,
  GC,
};

struct Region {
  RegionId id = 0;
  RegionState state = RegionState::Closed;
  RegionStrategy strategy = RegionStrategy::Arena;
  void* bridge = nullptr;
  std::vector<void*> allocations;
};

extern "C" Region* __gura_region_new_iso(std::size_t bridgeSize);
extern "C" void __gura_region_enter(Region* region);
extern "C" void __gura_region_exit(Region* region, void* newBridge);
extern "C" void __gura_region_destroy(Region* region);

} // namespace gura::runtime
