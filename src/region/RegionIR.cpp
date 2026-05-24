#include "gura/region/RegionIR.h"

namespace gura::region {

std::string opcodeName(Opcode opcode) {
  switch (opcode) {
  case Opcode::Load: return "load";
  case Opcode::Swap: return "swap";
  case Opcode::Move: return "move";
  case Opcode::AllocMut: return "alloc_mut";
  case Opcode::AllocTmp: return "alloc_tmp";
  case Opcode::AllocIso: return "alloc_iso";
  case Opcode::Enter: return "enter";
  case Opcode::Exit: return "exit";
  case Opcode::ExploreEnter: return "explore_enter";
  case Opcode::ExploreExit: return "explore_exit";
  case Opcode::Freeze: return "freeze";
  case Opcode::Merge: return "merge";
  case Opcode::CownNew: return "cown_new";
  case Opcode::CownAcquire: return "cown_acquire";
  case Opcode::CownRelease: return "cown_release";
  case Opcode::Spawn: return "spawn";
  case Opcode::TopologyAssert: return "topology_assert";
  }
  return "unknown";
}

} // namespace gura::region
