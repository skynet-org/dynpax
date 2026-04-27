#pragma once

#include <filesystem>
#include <system_error>

namespace dynpax
{

namespace fs = std::filesystem;

template <typename T> struct AbstractFileManager
{

    [[nodiscard]] auto path() const -> fs::path;

    [[nodiscard]] auto addLibrary(const fs::path &src,
                                  std::error_code &errc) const
        -> fs::path;

    [[nodiscard]] auto binaryStub(const fs::path &src,
                                  std::error_code &errc) const
        -> fs::path;

    [[nodiscard]] auto stripRoot(const fs::path &path) const -> fs::path;
};

} // namespace dynpax