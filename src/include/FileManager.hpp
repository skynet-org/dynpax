#pragma once
#include <filesystem>
#include <initializer_list>
namespace dynpax
{

namespace fs = std::filesystem;

struct FileManager
{
    explicit FileManager(fs::path fakeRoot);

    void fakeRoot(fs::path path);

    [[nodiscard]] auto fakeRoot() const -> fs::path;

    [[nodiscard]] auto joinFakeRoot(
        std::initializer_list<fs::path>) const -> fs::path;

    static auto copyFile(const fs::path &src, const fs::path &dst)
        -> bool;

  private:
    fs::path m_fakeRoot;
};
} // namespace dynpax
