#pragma once

namespace gura::hir {

enum class Capability {
  Mut,
  Tmp,
  Iso,
  Imm,
  Paused,
  Cown,
  None,
};

[[nodiscard]] constexpr bool isCopyable(Capability capability) {
  return capability == Capability::Imm || capability == Capability::Cown || capability == Capability::None;
}

[[nodiscard]] constexpr bool isSendSafe(Capability capability) {
  return capability == Capability::Iso || capability == Capability::Imm || capability == Capability::Cown || capability == Capability::None;
}

} // namespace gura::hir
