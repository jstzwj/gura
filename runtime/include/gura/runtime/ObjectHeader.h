#pragma once

#include <cstdint>

namespace gura::runtime {

using RegionId = std::uint64_t;
using TypeId = std::uint64_t;

enum ObjectFlags : std::uint32_t {
  ObjectFlagNone = 0,
  ObjectFlagFrozen = 1u << 0,
};

struct ObjectHeader {
  RegionId regionId = 0;
  TypeId typeId = 0;
  std::uint32_t flags = ObjectFlagNone;
  std::uint32_t reserved = 0;
};

} // namespace gura::runtime
