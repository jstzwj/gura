#pragma once

#include "gura/ast/Node.h"

#include <string>

namespace gura::ast {

struct Pattern : Node {
  std::string binding;
  bool isVar = false;
};

} // namespace gura::ast
