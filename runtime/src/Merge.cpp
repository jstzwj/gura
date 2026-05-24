#include "gura/runtime/Merge.h"

#include <stdexcept>

namespace gura::runtime {

extern "C" void* __gura_region_merge(Region* target, Region* source) {
  if (target == nullptr || target->state != RegionState::Active) {
    throw std::runtime_error("merge target must be active");
  }
  if (source == nullptr || source->state != RegionState::Closed) {
    throw std::runtime_error("merge source must be closed");
  }
  target->allocations.insert(target->allocations.end(), source->allocations.begin(), source->allocations.end());
  source->allocations.clear();
  void* bridge = source->bridge;
  delete source;
  return bridge;
}

extern "C" void __gura_topology_assert(Region*) {}

} // namespace gura::runtime
