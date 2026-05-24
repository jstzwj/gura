#pragma once

#include "gura/region/RegionIR.h"

#include <string>
#include <vector>

namespace gura::region {

class RegionVerifier {
public:
  [[nodiscard]] bool verify(const Module& module);
  [[nodiscard]] const std::vector<std::string>& errors() const { return errors_; }

private:
  std::vector<std::string> errors_;
};

} // namespace gura::region
