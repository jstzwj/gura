#pragma once

#include "gura/basic/SourceLocation.h"

#include <string>
#include <vector>

namespace gura {

enum class DiagnosticSeverity {
  Note,
  Warning,
  Error,
};

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  Span span;
  std::string message;
};

class DiagnosticEngine {
public:
  void report(DiagnosticSeverity severity, Span span, std::string message);
  void error(Span span, std::string message);

  [[nodiscard]] bool hasError() const;
  [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
  [[nodiscard]] std::string format() const;

private:
  std::vector<Diagnostic> diagnostics_;
};

} // namespace gura
