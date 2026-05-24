#include "gura/basic/SourceManager.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace gura {

std::size_t SourceManager::addVirtualFile(std::filesystem::path path, std::string contents) {
  files_.push_back(SourceFile{std::move(path), std::move(contents)});
  return files_.size() - 1;
}

std::size_t SourceManager::loadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to open source file: " + path.string());
  }
  std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  return addVirtualFile(path, std::move(contents));
}

const SourceManager::SourceFile& SourceManager::file(std::size_t id) const {
  return files_.at(id);
}

std::string_view SourceManager::contents(std::size_t id) const {
  return files_.at(id).contents;
}

} // namespace gura
