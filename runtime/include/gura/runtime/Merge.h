#pragma once

#include "gura/runtime/Region.h"

namespace gura::runtime {

extern "C" void* __gura_region_merge(Region* target, Region* source);
extern "C" void __gura_topology_assert(Region* region);

} // namespace gura::runtime
