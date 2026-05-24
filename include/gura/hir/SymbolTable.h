#pragma once

#include "gura/hir/Type.h"

#include <string>
#include <unordered_map>

namespace gura::hir {

class SymbolTable {
public:
  void define(std::string name, Type type) { symbols_.emplace(std::move(name), std::move(type)); }

private:
  std::unordered_map<std::string, Type> symbols_;
};

} // namespace gura::hir
