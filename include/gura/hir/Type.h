#pragma once

#include "gura/hir/Capability.h"

#include <memory>
#include <optional>
#include <string>

namespace gura::hir {

struct Type {
  Capability capability = Capability::None;
  std::string name;
  bool isArray = false;
  std::shared_ptr<Type> elementType = nullptr;
  std::optional<std::size_t> arrayLength = std::nullopt;
};

} // namespace gura::hir
