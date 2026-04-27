#include "BundleLayout.hpp"
#include <algorithm>
#include <array>
#include <string>

namespace dynpax
{
namespace
{

auto path_is_within(const fs::path &path, const fs::path &root) -> bool
{
    auto normalizedPath = path.lexically_normal();
    auto normalizedRoot = root.lexically_normal();
    if (normalizedPath == normalizedRoot)
    {
        return true;
    }

    auto relative = normalizedPath.lexically_relative(normalizedRoot);
    return !relative.empty() &&
           !relative.string().starts_with("..");
}

auto should_preserve_source_path(const fs::path &path) -> bool
{
    if (!path.is_absolute())
    {
        return false;
    }

    constexpr auto runtimeRoots = std::array{
        "/bin", "/sbin", "/usr", "/opt", "/lib", "/lib64"};
    return std::ranges::any_of(runtimeRoots, [&](const auto *root) {
        return path_is_within(path, fs::path{root});
    });
}

auto flat_bundle_path(BundleEntryKind kind, const fs::path &sourcePath)
    -> fs::path
{
    if (kind == BundleEntryKind::Executable)
    {
        return fs::path{"/bin"} / sourcePath.filename();
    }
    return fs::path{"/lib64"} / sourcePath.filename();
}

auto preserve_source_tree_fallback_path(
    BundleEntryKind kind, const fs::path &sourcePath,
    const std::optional<fs::path> &fallbackLibraryDirectory)
    -> fs::path
{
    if (kind == BundleEntryKind::Executable)
    {
        return fs::path{"/bin"} / sourcePath.filename();
    }
    return fallbackLibraryDirectory.value_or(fs::path{"/usr/lib"}) /
           sourcePath.filename();
}

auto append_unique(std::vector<fs::path> &paths, const fs::path &path)
    -> void
{
    auto normalized = path.lexically_normal();
    auto found = std::ranges::find(paths, normalized);
    if (found == paths.end())
    {
        paths.push_back(std::move(normalized));
    }
}

auto bundled_library_directories(const BundleManifest &manifest,
                                 BundleLayoutPolicy policy)
    -> std::vector<fs::path>
{
    if (policy == BundleLayoutPolicy::FlatLib64)
    {
        return {fs::path{"/lib64"}};
    }

    auto directories = std::vector<fs::path>{};
    for (const auto &entry : manifest.entries)
    {
        if (entry.kind != BundleEntryKind::SharedObject)
        {
            continue;
        }

        auto directory = entry.bundledPath.parent_path();
        if (directory.empty())
        {
            directory = "/";
        }
        append_unique(directories, directory);
    }

    if (directories.empty())
    {
        directories.push_back(fs::path{"/lib64"});
    }
    return directories;
}

auto make_origin_runpath_entry(const fs::path &fromDir,
                               const fs::path &toDir) -> std::string
{
    auto relative = toDir.lexically_relative(fromDir);
    if (relative.empty())
    {
        return "$ORIGIN";
    }

    auto text = relative.generic_string();
    if (text == ".")
    {
        return "$ORIGIN";
    }
    return "$ORIGIN/" + text;
}

} // namespace

auto bundle_layout_policy_name(BundleLayoutPolicy policy)
    -> std::string_view
{
    switch (policy)
    {
    case BundleLayoutPolicy::FlatLib64:
        return "flat-lib64";
    case BundleLayoutPolicy::PreserveSourceTree:
        return "preserve-source-tree";
    }

    return "flat-lib64";
}

auto parse_bundle_layout_policy(std::string_view name)
    -> std::optional<BundleLayoutPolicy>
{
    if (name == "flat-lib64" || name == "flat")
    {
        return BundleLayoutPolicy::FlatLib64;
    }
    if (name == "preserve-source-tree" || name == "preserve")
    {
        return BundleLayoutPolicy::PreserveSourceTree;
    }
    return std::nullopt;
}

auto bundle_path_for(
    BundleLayoutPolicy policy, BundleEntryKind kind,
    const fs::path &sourcePath,
    std::optional<fs::path> fallbackLibraryDirectory) -> fs::path
{
    if (policy == BundleLayoutPolicy::FlatLib64)
    {
        return flat_bundle_path(kind, sourcePath);
    }

    if (!should_preserve_source_path(sourcePath))
    {
        return preserve_source_tree_fallback_path(
            kind, sourcePath, fallbackLibraryDirectory);
    }

    switch (kind)
    {
    case BundleEntryKind::Executable:
    case BundleEntryKind::SharedObject:
    case BundleEntryKind::Interpreter:
    case BundleEntryKind::SymlinkAlias:
        return sourcePath.lexically_normal();
    case BundleEntryKind::Unknown:
        break;
    }

    return flat_bundle_path(kind, sourcePath);
}

auto bundle_runpath(const BundleManifest &manifest,
                    const BundleEntry &entry,
                    BundleLayoutPolicy policy)
    -> std::vector<std::string>
{
    if (entry.kind != BundleEntryKind::Executable &&
        entry.kind != BundleEntryKind::SharedObject)
    {
        return {};
    }

    if (policy == BundleLayoutPolicy::FlatLib64)
    {
        return {"$ORIGIN/../lib64"};
    }

    auto ownerDirectory = entry.bundledPath.parent_path();
    if (ownerDirectory.empty())
    {
        ownerDirectory = "/";
    }

    auto runpath = std::vector<std::string>{};
    for (const auto &libraryDirectory :
         bundled_library_directories(manifest, policy))
    {
        auto candidate =
            make_origin_runpath_entry(ownerDirectory, libraryDirectory);
        if (std::ranges::find(runpath, candidate) == runpath.end())
        {
            runpath.push_back(std::move(candidate));
        }
    }

    if (runpath.empty())
    {
        runpath.push_back("$ORIGIN");
    }
    return runpath;
}

auto compatibility_directories(BundleLayoutPolicy policy)
    -> std::vector<fs::path>
{
    if (policy == BundleLayoutPolicy::FlatLib64)
    {
        return {"bin", "lib64", "usr"};
    }
    return {};
}

auto compatibility_symlinks(BundleLayoutPolicy policy)
    -> std::vector<std::pair<fs::path, fs::path>>
{
    if (policy == BundleLayoutPolicy::FlatLib64)
    {
        return {{"lib", "lib64"},
                {"usr/lib", "../lib64"},
                {"usr/lib64", "../lib64"}};
    }
    return {};
}

} // namespace dynpax