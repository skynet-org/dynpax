#pragma once

#include "BundleBuilder.hpp"
#include "BundleManifest.hpp"
#include "Resolver.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace dynpax
{

namespace fs = std::filesystem;

struct BundleVerificationIssue
{
    fs::path path;
    std::string message;
};

struct BundleVerificationOptions
{
    std::string expectedRunpath{"$ORIGIN/../lib64"};
};

struct BundleVerificationReport
{
    std::vector<BundleVerificationIssue> issues;

    [[nodiscard]] auto ok() const -> bool;

    [[nodiscard]] auto summary() const -> std::string;
};

class BundleVerifier
{
  public:
    explicit BundleVerifier(std::shared_ptr<Resolver> resolver);

    [[nodiscard]] auto verify(
        const BundleBuildResult &result,
        const BundleVerificationOptions &options = {}) const
        -> BundleVerificationReport;

  private:
    auto verifyEntry(const BundleBuildResult &result,
                     const BundleEntry &entry,
                     const BundleVerificationOptions &options,
                     BundleVerificationReport &report) const -> void;

    std::shared_ptr<Resolver> resolver_;
};

} // namespace dynpax