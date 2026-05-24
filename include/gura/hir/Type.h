#pragma once

#include "gura/hir/Capability.h"

#include <string>

namespace gura::hir {

struct Type {
  Capability capability = Capability::None;
  std::string name;
};

} // namespace gura::hir
