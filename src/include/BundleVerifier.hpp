#pragma once

#include "BundleBuilder.hpp"
#include "BundleLayout.hpp"
#include "BundleManifest.hpp"
#include "Resolver.hpp"
#include <filesystem>
#include <memory>
#include <optional>
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
    std::optional<BundleLayoutPolicy> layoutPolicy;
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
                                         BundleLayoutPolicy layoutPolicy,
                     BundleVerificationReport &report) const -> void;

    std::shared_ptr<Resolver> resolver_;
};

} // namespace dynpax