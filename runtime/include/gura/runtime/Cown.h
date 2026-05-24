#pragma once

#include "gura/runtime/Region.h"

#include <cstdint>
#include <mutex>

namespace gura::runtime {

struct Cown {
  std::uint64_t id = 0;
  Region* region = nullptr;
  std::mutex mutex;
};

extern "C" Cown* __gura_cown_new(Region* region);
extern "C" void __gura_cown_acquire(Cown* cown);
extern "C" void __gura_cown_release(Cown* cown);
extern "C" void __gura_cown_destroy(Cown* cown);

} // namespace gura::runtime
