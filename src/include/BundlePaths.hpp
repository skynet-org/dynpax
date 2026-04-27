#pragma once

#include <filesystem>

namespace dynpax
{

namespace fs = std::filesystem;

[[nodiscard]] inline auto materialized_path(
    const fs::path &bundleRoot, const fs::path &bundledPath)
    -> fs::path
{
    return bundleRoot /
           (bundledPath.is_absolute()
                ? bundledPath.lexically_relative("/")
                : bundledPath);
}

[[nodiscard]] inline auto materialized_symlink_target(
    const fs::path &bundledPath, const fs::path &targetPath)
    -> fs::path
{
    auto relativeTarget =
        targetPath.lexically_relative(bundledPath.parent_path());
    if (relativeTarget.empty())
    {
        return targetPath;
    }
    return relativeTarget;
}

} // namespace dynpax