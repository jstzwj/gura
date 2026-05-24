#include "gura/runtime/Cown.h"

#include <atomic>

namespace gura::runtime {

namespace {
std::atomic<std::uint64_t> nextCownId{1};
}

extern "C" Cown* __gura_cown_new(Region* region) {
  auto* cown = new Cown();
  cown->id = nextCownId.fetch_add(1);
  cown->region = region;
  return cown;
}

extern "C" void __gura_cown_acquire(Cown* cown) {
  cown->mutex.lock();
}

extern "C" void __gura_cown_release(Cown* cown) {
  cown->mutex.unlock();
}

extern "C" void __gura_cown_destroy(Cown* cown) {
  delete cown;
}

} // namespace gura::runtime
