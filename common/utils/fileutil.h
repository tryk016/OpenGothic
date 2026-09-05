#pragma once

#include <Tempest/Dir>
#include <string>
#include <string_view>

namespace FileUtil {
  bool exists(const std::u16string& path);
  bool replaceFile(std::u16string_view source, std::u16string_view destination);
  void removeFile(std::u16string_view path);
  std::u16string caseInsensitiveSegment(std::u16string_view path, const char16_t* segment, Tempest::Dir::FileType type);
  std::u16string nestedPath(std::u16string_view gpath, const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type);
  }
