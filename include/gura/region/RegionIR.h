#pragma once

#include <string>
#include <vector>

namespace gura::region {

enum class Opcode {
  Load,
  Swap,
  Move,
  AllocMut,
  AllocTmp,
  AllocIso,
  Enter,
  Exit,
  ExploreEnter,
  ExploreExit,
  Freeze,
  Merge,
  CownNew,
  CownAcquire,
  CownRelease,
  Spawn,
  TopologyAssert,
};

struct Instruction {
  Opcode opcode;
  std::vector<std::string> operands;
};

struct Module {
  std::vector<Instruction> instructions;
};

[[nodiscard]] std::string opcodeName(Opcode opcode);

} // namespace gura::region
