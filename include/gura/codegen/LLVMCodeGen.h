#pragma once

#include "gura/ast/Decl.h"

#include <string>

namespace gura {

class LLVMCodeGen {
public:
  [[nodiscard]] std::string emitModule(const ast::SourceFile& file) const;
};

} // namespace gura
