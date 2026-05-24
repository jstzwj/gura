#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gura {

class SourceManager {
public:
  struct SourceFile {
    std::filesystem::path path;
    std::string contents;
  };

  std::size_t addVirtualFile(std::filesystem::path path, std::string contents);
  std::size_t loadFile(const std::filesystem::path& path);

  [[nodiscard]] const SourceFile& file(std::size_t id) const;
  [[nodiscard]] std::string_view contents(std::size_t id) const;

private:
  std::vector<SourceFile> files_;
};

} // namespace gura
