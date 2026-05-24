#pragma once

#include "gura/runtime/Region.h"

namespace gura::runtime {

extern "C" void* __gura_region_freeze(Region* region);

} // namespace gura::runtime
