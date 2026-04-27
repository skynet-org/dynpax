#pragma once
#include "abstract/AbstractFileManager.hpp"
#include <filesystem>

namespace dynpax
{

namespace fs = std::filesystem;

struct FakeRoot : public AbstractFileManager<FakeRoot>
{
    explicit FakeRoot(fs::path fakeRoot);

    [[nodiscard]] auto path() const -> fs::path;

    [[nodiscard]] auto addLibrary(const fs::path &src,
                                  std::error_code &errc) const
        -> fs::path;

    [[nodiscard]] auto binaryStub(const fs::path &src,
                                  std::error_code &errc) const
        -> fs::path;

    [[nodiscard]] auto stripRoot(const fs::path &path) const
        -> fs::path;

  private:
    fs::path m_fakeRoot;
};
} // namespace dynpax
