#include "gura/runtime/Region.h"

#include <catch2/catch_test_macros.hpp>

using namespace gura::runtime;

TEST_CASE("runtime creates enters and exits a region") {
  Region* region = __gura_region_new_iso(8);
  REQUIRE(region != nullptr);
  CHECK(region->state == RegionState::Closed);
  __gura_region_enter(region);
  CHECK(region->state == RegionState::Active);
  __gura_region_exit(region, nullptr);
  CHECK(region->state == RegionState::Closed);
  __gura_region_destroy(region);
}
