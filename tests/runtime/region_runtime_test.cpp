#include "gura/runtime/Freeze.h"
#include "gura/runtime/Merge.h"
#include "gura/runtime/Region.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

using namespace gura::runtime;

TEST_CASE("runtime creates enters and exits a region") {
  Region* region = __gura_region_new_iso(8, RegionStrategy::Arena);
  REQUIRE(region != nullptr);
  CHECK(region->state == RegionState::Closed);
  CHECK(region->bridgeSize == 8);
  REQUIRE(__gura_region_bridge(region) != nullptr);
  CHECK(__gura_header_for_payload(region->bridge)->regionId == region->id);
  __gura_region_enter(region);
  CHECK(region->state == RegionState::Active);
  CHECK(region->bridgeToken.state == BridgeState::ActiveMut);
  __gura_region_exit(region, nullptr);
  CHECK(region->state == RegionState::Closed);
  CHECK(region->bridgeToken.state == BridgeState::ExternalIso);
  __gura_region_destroy(region);
}

TEST_CASE("runtime stores bridge type metadata") {
  Region* region = __gura_region_new_iso(8, RegionStrategy::Arena);
  __gura_region_set_bridge_type(region, 42);
  CHECK(region->bridgeTypeId == 42);
  CHECK(region->boundary.bridgeTypeId == 42);
  CHECK(region->bridgeToken.typeId == 42);
  CHECK(__gura_header_for_payload(region->bridge)->typeId == 42);
  __gura_region_destroy(region);
}

TEST_CASE("runtime preserves region allocation strategy metadata") {
  Region* arena = __gura_region_new_iso(8, RegionStrategy::Arena);
  Region* rc = __gura_region_new_iso(8, RegionStrategy::RC);
  Region* gc = __gura_region_new_iso(8, RegionStrategy::GC);
  Region* manual = __gura_region_new_iso(8, RegionStrategy::Manual);

  CHECK(arena->strategy == RegionStrategy::Arena);
  CHECK(rc->strategy == RegionStrategy::RC);
  CHECK(gc->strategy == RegionStrategy::GC);
  CHECK(manual->strategy == RegionStrategy::Manual);

  __gura_region_destroy(arena);
  __gura_region_destroy(rc);
  __gura_region_destroy(gc);
  __gura_region_destroy(manual);
}

TEST_CASE("runtime rejects foreign bridge on exit") {
  Region* region = __gura_region_new_iso(8, RegionStrategy::Arena);
  Region* foreign = __gura_region_new_iso(8, RegionStrategy::Arena);
  __gura_region_enter(region);
  CHECK_THROWS(__gura_region_exit(region, foreign->bridge));
  __gura_region_exit(region, nullptr);
  __gura_region_destroy(region);
  __gura_region_destroy(foreign);
}

TEST_CASE("runtime syncs bridge type metadata on bridge switch") {
  Region* region = __gura_region_new_iso(8, RegionStrategy::Arena);
  __gura_region_set_bridge_type(region, 11);
  auto* allocation = static_cast<ObjectHeader*>(std::calloc(1, sizeof(ObjectHeader) + 8));
  REQUIRE(allocation != nullptr);
  allocation->regionId = region->id;
  allocation->typeId = 22;
  allocation->flags = ObjectFlagNone;
  void* newBridge = __gura_payload_for_header(allocation);
  region->allocations.push_back(allocation);

  __gura_region_enter(region);
  __gura_region_exit(region, newBridge);

  CHECK(region->bridge == newBridge);
  CHECK(region->bridgeTypeId == 22);
  CHECK(region->boundary.bridgeTypeId == 22);
  CHECK(region->bridgeToken.typeId == 22);
  CHECK(region->bridgeToken.state == BridgeState::ExternalIso);
  __gura_region_destroy(region);
}

TEST_CASE("runtime rejects frozen bridge on exit") {
  Region* region = __gura_region_new_iso(8, RegionStrategy::Arena);
  auto* allocation = static_cast<ObjectHeader*>(std::calloc(1, sizeof(ObjectHeader) + 8));
  REQUIRE(allocation != nullptr);
  allocation->regionId = region->id;
  allocation->typeId = 22;
  allocation->flags = ObjectFlagFrozen;
  void* frozenBridge = __gura_payload_for_header(allocation);
  region->allocations.push_back(allocation);

  __gura_region_enter(region);
  CHECK_THROWS(__gura_region_exit(region, frozenBridge));
  __gura_region_exit(region, nullptr);
  __gura_region_destroy(region);
}

TEST_CASE("runtime marks frozen bridge") {
  Region* region = __gura_region_new_iso(8, RegionStrategy::Arena);
  void* bridge = __gura_region_freeze(region);
  CHECK(bridge == region->bridge);
  CHECK((__gura_header_for_payload(bridge)->flags & ObjectFlagFrozen) != 0);
  CHECK(region->bridgeToken.state == BridgeState::Frozen);
  __gura_region_destroy(region);
}

TEST_CASE("runtime merges closed source into active target") {
  Region* target = __gura_region_new_iso(8, RegionStrategy::Arena);
  Region* source = __gura_region_new_iso(8, RegionStrategy::Arena);
  void* sourceBridge = source->bridge;
  __gura_region_enter(target);
  void* merged = __gura_region_merge(target, source);
  CHECK(merged == sourceBridge);
  CHECK(__gura_header_for_payload(merged)->regionId == target->id);
  CHECK(target->bridgeToken.state == BridgeState::Merged);
  __gura_region_exit(target, nullptr);
  __gura_region_destroy(target);
}
