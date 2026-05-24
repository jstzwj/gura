#include "gura/basic/Diagnostic.h"

#include <fmt/format.h>

namespace gura {

void DiagnosticEngine::report(DiagnosticSeverity severity, Span span, std::string message) {
  diagnostics_.push_back(Diagnostic{severity, span, std::move(message)});
}

void DiagnosticEngine::error(Span span, std::string message) {
  report(DiagnosticSeverity::Error, span, std::move(message));
}

bool DiagnosticEngine::hasError() const {
  for (const auto& diagnostic : diagnostics_) {
    if (diagnostic.severity == DiagnosticSeverity::Error) {
      return true;
    }
  }
  return false;
}

std::string DiagnosticEngine::format() const {
  std::string output;
  for (const auto& diagnostic : diagnostics_) {
    const char* label = "error";
    if (diagnostic.severity == DiagnosticSeverity::Warning) {
      label = "warning";
    } else if (diagnostic.severity == DiagnosticSeverity::Note) {
      label = "note";
    }
    output += fmt::format("{}:{}:{}: {}: {}\n", diagnostic.span.begin.line, diagnostic.span.begin.column,
                          diagnostic.span.end.column, label, diagnostic.message);
  }
  return output;
}

} // namespace gura
