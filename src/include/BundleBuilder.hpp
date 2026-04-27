#pragma once

#include "BundleManifest.hpp"
#include "BundleLayout.hpp"
#include "FakeRoot.hpp"
#include "Resolver.hpp"
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dynpax
{

namespace fs = std::filesystem;

struct Executable;

struct BundleBuildResult
{
    fs::path bundleRoot;
    BundleLayoutPolicy layoutPolicy{BundleLayoutPolicy::FlatLib64};
    BundleManifest manifest;
    std::optional<fs::path> executableOutput;
    std::optional<fs::path> interpreterBundlePath;
};

struct BundleRewriteOptions
{
    std::vector<std::string> rpath{""};
};

class BundleBuilder
{
  public:
    explicit BundleBuilder(std::shared_ptr<Resolver> resolver);

    [[nodiscard]] auto build(const fs::path &target,
                             const FakeRoot &fakeRoot,
                             bool includeInterpreter) const
        -> std::expected<BundleBuildResult, std::string>;

  private:
    [[nodiscard]] auto rewritePayloads(
        const BundleBuildResult &result,
                Executable &sourceExecutable,
        const BundleRewriteOptions &options = {}) const
        -> std::optional<std::string>;

    std::shared_ptr<Resolver> resolver_;
};

} // namespace dynpax