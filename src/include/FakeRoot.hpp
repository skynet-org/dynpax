#pragma once
#include "BundleLayout.hpp"
#include "BundleManifest.hpp"
#include "abstract/AbstractFileManager.hpp"
#include <filesystem>

namespace dynpax
{

namespace fs = std::filesystem;

struct FakeRoot : public AbstractFileManager<FakeRoot>
{
    explicit FakeRoot(
        fs::path fakeRoot,
        BundleLayoutPolicy layoutPolicy = BundleLayoutPolicy::FlatLib64);

    [[nodiscard]] auto path() const -> fs::path;

    [[nodiscard]] auto layoutPolicy() const -> BundleLayoutPolicy;

    [[nodiscard]] auto addLibrary(const fs::path &src,
                                  std::error_code &errc) const
        -> fs::path;

    [[nodiscard]] auto binaryStub(const fs::path &src,
                                  std::error_code &errc) const
        -> fs::path;

    [[nodiscard]] auto materialize(const BundleEntry &entry,
                                   std::error_code &errc) const
        -> fs::path;

    [[nodiscard]] auto stripRoot(const fs::path &path) const
        -> fs::path;

  private:
    fs::path m_fakeRoot;
        BundleLayoutPolicy m_layoutPolicy;
};
} // namespace dynpax
