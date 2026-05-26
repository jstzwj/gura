#pragma once

#include "gura/ast/Node.h"

#include <string>
#include <vector>

namespace gura::ast {

struct Path;

enum class Capability {
  None,
  Mut,
  Tmp,
  Iso,
  Imm,
  Paused,
  Cown,
};

struct TypeRef : Node {
  Capability capability = Capability::None;
  std::string name;
  std::vector<std::string> path;
  bool optional = false;
  bool isArray = false;
  Ptr<TypeRef> elementType;
};

} // namespace gura::ast
