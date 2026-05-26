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
  Arena = 0,
  RC = 1,
  GC = 2,
  Manual = 3,
};

enum class BridgeState {
  ExternalIso,
  ActiveMut,
  PausedRead,
  Frozen,
  Merged,
};

struct BridgeToken {
  RegionId regionId = 0;
  void* object = nullptr;
  TypeId typeId = 0;
  BridgeState state = BridgeState::ExternalIso;
};

struct BoundaryMetadata {
  RegionId regionId = 0;
  TypeId bridgeTypeId = 0;
  std::size_t bridgeSize = 0;
};

struct Region {
  RegionId id = 0;
  RegionState state = RegionState::Closed;
  RegionStrategy strategy = RegionStrategy::Arena;
  void* bridge = nullptr;
  TypeId bridgeTypeId = 0;
  std::size_t bridgeSize = 0;
  BridgeToken bridgeToken;
  BoundaryMetadata boundary;
  std::vector<void*> allocations;
};

ObjectHeader* __gura_header_for_payload(void* payload);
void* __gura_payload_for_header(ObjectHeader* header);

extern "C" Region* __gura_region_new_iso(std::size_t bridgeSize, RegionStrategy strategy);
extern "C" void* __gura_region_bridge(Region* region);
extern "C" void __gura_region_set_bridge_type(Region* region, TypeId typeId);
extern "C" void __gura_region_enter(Region* region);
extern "C" void __gura_region_exit(Region* region, void* newBridge);
extern "C" void __gura_region_destroy(Region* region);

} // namespace gura::runtime
