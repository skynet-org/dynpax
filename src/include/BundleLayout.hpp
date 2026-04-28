#pragma once

#include "BundleManifest.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace dynpax
{

namespace fs = std::filesystem;

enum class BundleLayoutPolicy : std::uint8_t
{
    FlatLib64,
    PreserveSourceTree,
};

[[nodiscard]] auto bundle_layout_policy_name(
    BundleLayoutPolicy policy) -> std::string_view;

[[nodiscard]] auto parse_bundle_layout_policy(std::string_view name)
    -> std::optional<BundleLayoutPolicy>;

[[nodiscard]] auto bundle_path_for(BundleLayoutPolicy policy,
                                   BundleEntryKind kind,
                                   const fs::path &sourcePath,
                                   std::optional<fs::path>
                                       fallbackLibraryDirectory =
                                           std::nullopt)
    -> fs::path;

[[nodiscard]] auto bundle_runpath(const BundleManifest &manifest,
                                  const BundleEntry &entry,
                                  BundleLayoutPolicy policy)
    -> std::vector<std::string>;

[[nodiscard]] auto compatibility_directories(
    BundleLayoutPolicy policy) -> std::vector<fs::path>;

[[nodiscard]] auto compatibility_symlinks(
    BundleLayoutPolicy policy)
    -> std::vector<std::pair<fs::path, fs::path>>;

} // namespace dynpax