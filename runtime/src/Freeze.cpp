#include "gura/runtime/Freeze.h"

#include <stdexcept>

namespace gura::runtime {

extern "C" void* __gura_region_freeze(Region* region) {
  if (region == nullptr || region->state != RegionState::Closed) {
    throw std::runtime_error("freeze requires a closed region");
  }
  return region->bridge;
}

} // namespace gura::runtime
