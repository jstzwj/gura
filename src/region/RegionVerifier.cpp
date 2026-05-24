#include "gura/region/RegionVerifier.h"

namespace gura::region {

bool RegionVerifier::verify(const Module& module) {
  errors_.clear();
  int depth = 0;
  for (const auto& instruction : module.instructions) {
    if (instruction.opcode == Opcode::Enter || instruction.opcode == Opcode::ExploreEnter) {
      ++depth;
    } else if (instruction.opcode == Opcode::Exit || instruction.opcode == Opcode::ExploreExit) {
      --depth;
      if (depth < 0) {
        errors_.push_back("region exit without matching enter");
        depth = 0;
      }
    }
  }
  if (depth != 0) {
    errors_.push_back("unclosed region enter");
  }
  return errors_.empty();
}

} // namespace gura::region
