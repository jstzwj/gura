#pragma once

#include "gura/basic/SourceLocation.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gura::ast {

using NodeId = std::uint64_t;

struct Node {
  NodeId id = 0;
  Span span;
  virtual ~Node() = default;
};

template <typename T>
using Ptr = std::unique_ptr<T>;

template <typename T>
using List = std::vector<Ptr<T>>;

} // namespace gura::ast
