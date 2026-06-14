#include "gura/runtime/Region.h"

#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace gura::runtime {

namespace {
std::atomic<RegionId> nextRegionId{1};
thread_local std::vector<Region*> regionStack;

void requireStackTop(Region* region, const char* message) {
  if (regionStack.empty() || regionStack.back() != region) {
    throw std::runtime_error(message);
  }
}
}

ObjectHeader* __gura_header_for_payload(void* payload) {
  if (payload == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<ObjectHeader*>(static_cast<unsigned char*>(payload) - sizeof(ObjectHeader));
}

void* __gura_payload_for_header(ObjectHeader* header) {
  if (header == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<unsigned char*>(header) + sizeof(ObjectHeader);
}

extern "C" Region* __gura_region_new_iso(std::size_t bridgeSize, RegionStrategy strategy) {
  auto* region = new Region();
  region->id = nextRegionId.fetch_add(1);
  region->state = RegionState::Closed;
  region->strategy = strategy;
  region->bridgeSize = bridgeSize;
  region->boundary = BoundaryMetadata{region->id, region->bridgeTypeId, bridgeSize};
  auto* allocation = static_cast<ObjectHeader*>(std::calloc(1, sizeof(ObjectHeader) + bridgeSize));
  if (allocation == nullptr) {
    delete region;
    throw std::bad_alloc();
  }
  allocation->regionId = region->id;
  allocation->typeId = region->bridgeTypeId;
  allocation->flags = ObjectFlagNone;
  region->bridge = __gura_payload_for_header(allocation);
  region->bridgeToken = BridgeToken{region->id, region->bridge, region->bridgeTypeId, BridgeState::ExternalIso};
  region->allocations.push_back(allocation);
  return region;
}

extern "C" void* __gura_region_bridge(Region* region) {
  if (region == nullptr) {
    throw std::runtime_error("region bridge requires a region");
  }
  return region->bridge;
}

extern "C" void __gura_region_set_bridge_type(Region* region, TypeId typeId) {
  if (region == nullptr) {
    throw std::runtime_error("region bridge type requires a region");
  }
  region->bridgeTypeId = typeId;
  region->bridgeToken.typeId = typeId;
  region->boundary.bridgeTypeId = typeId;
  if (auto* header = __gura_header_for_payload(region->bridge)) {
    header->typeId = typeId;
  }
}

extern "C" void __gura_region_enter(Region* region) {
  if (region == nullptr || region->state != RegionState::Closed) {
    throw std::runtime_error("cannot enter non-closed region");
  }
  if (!regionStack.empty() && regionStack.back()->state == RegionState::Active) {
    regionStack.back()->state = RegionState::Paused;
  }
  regionStack.push_back(region);
  region->state = RegionState::Active;
  region->bridgeToken.state = BridgeState::ActiveMut;
}

extern "C" void __gura_region_exit(Region* region, void* newBridge) {
  if (region == nullptr || region->state != RegionState::Active) {
    throw std::runtime_error("cannot exit non-active region");
  }
  requireStackTop(region, "cannot exit region that is not on top of the region stack");
  if (newBridge != nullptr) {
    const auto* header = __gura_header_for_payload(newBridge);
    if (header == nullptr || header->regionId != region->id) {
      throw std::runtime_error("new bridge must belong to the exiting region");
    }
    if ((header->flags & ObjectFlagFrozen) != 0) {
      throw std::runtime_error("new bridge must not be frozen");
    }
    region->bridge = newBridge;
    region->bridgeTypeId = header->typeId;
    region->boundary.bridgeTypeId = header->typeId;
  }
  region->bridgeToken = BridgeToken{region->id, region->bridge, region->bridgeTypeId, BridgeState::ExternalIso};
  region->state = RegionState::Closed;
  regionStack.pop_back();
  if (!regionStack.empty() && regionStack.back()->state == RegionState::Paused) {
    regionStack.back()->state = RegionState::Active;
  }
}

extern "C" void __gura_region_explore(Region* region) {
  if (region == nullptr || region->state != RegionState::Closed) {
    throw std::runtime_error("cannot explore non-closed region");
  }
  if (!regionStack.empty() && regionStack.back()->state == RegionState::Active) {
    regionStack.back()->state = RegionState::Paused;
  }
  regionStack.push_back(region);
  region->state = RegionState::Paused;
  region->bridgeToken.state = BridgeState::PausedRead;
}

extern "C" void __gura_region_explore_exit(Region* region) {
  if (region == nullptr || region->state != RegionState::Paused) {
    throw std::runtime_error("cannot exit non-paused region");
  }
  requireStackTop(region, "cannot exit explored region that is not on top of the region stack");
  region->bridgeToken = BridgeToken{region->id, region->bridge, region->bridgeTypeId, BridgeState::ExternalIso};
  region->state = RegionState::Closed;
  regionStack.pop_back();
  if (!regionStack.empty() && regionStack.back()->state == RegionState::Paused) {
    regionStack.back()->state = RegionState::Active;
  }
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
