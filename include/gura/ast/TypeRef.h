#pragma once

#include "gura/ast/Node.h"

#include <string>

namespace gura::ast {

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
  bool optional = false;
};

} // namespace gura::ast
