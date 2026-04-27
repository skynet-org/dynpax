#include "FakeRoot.hpp"
#include <filesystem>
#include <fmt/printf.h>
#include <fstream>
#include <initializer_list>
#include <numeric>
#include <optional>
#include <system_error>
#include <utility>

namespace dynpax
{

namespace
{
auto join(const fs::path &fakeRoot,
          std::initializer_list<fs::path> paths) -> fs::path
{
    return std::accumulate(
        paths.begin(), paths.end(), fakeRoot,
        [](const auto &left, const auto &right) -> fs::path {
            return left / (right.is_absolute()
                               ? right.lexically_relative("/")
                               : right);
        });
}

    auto join(const fs::path &fakeRoot, const fs::path &path) -> fs::path
    {
        return fakeRoot /
           (path.is_absolute() ? path.lexically_relative("/") : path);
    }

auto copy(const fs::path &fakeRoot, const fs::path &subdir,
          const fs::path &src, std::error_code &errc) -> fs::path
{
    auto perm = fs::status(src).permissions();
    auto dst = join(fakeRoot, {subdir, src.filename()});
    if (!fs::exists(dst.parent_path()))
    {
        fs::create_directories(dst.parent_path(), errc);
    }
    fs::copy_file(src, dst,
                  fs::copy_options::overwrite_existing |
                      fs::copy_options::recursive,
                  errc);
    fs::permissions(dst, perm);
    return dst;
}

auto ensure_directory_symlink(const fs::path &path,
                              const fs::path &target,
                              std::error_code &errc) -> void
{
    if (fs::exists(path))
    {
        return;
    }

    if (!fs::exists(path.parent_path()))
    {
        fs::create_directories(path.parent_path(), errc);
    }
    if (errc)
    {
        return;
    }

    fs::create_directory_symlink(target, path, errc);
}

auto bootstrap_runtime_layout(const fs::path &fakeRoot,
                              std::error_code &errc) -> void
{
    fs::create_directories(fakeRoot / "bin", errc);
    if (errc)
    {
        return;
    }
    fs::create_directories(fakeRoot / "lib64", errc);
    if (errc)
    {
        return;
    }
    fs::create_directories(fakeRoot / "usr", errc);
    if (errc)
    {
        return;
    }

    ensure_directory_symlink(fakeRoot / "lib", "lib64", errc);
    if (errc)
    {
        return;
    }
    ensure_directory_symlink(fakeRoot / "usr/lib", "../lib64", errc);
    if (errc)
    {
        return;
    }
    ensure_directory_symlink(fakeRoot / "usr/lib64", "../lib64", errc);
}

auto copy_to(const fs::path &src, const fs::path &dst,
             std::error_code &errc) -> fs::path
{
    auto perm = fs::status(src).permissions();
    if (!fs::exists(dst.parent_path()))
    {
        fs::create_directories(dst.parent_path(), errc);
    }
    if (errc)
    {
        return dst;
    }
    fs::copy_file(src, dst,
                  fs::copy_options::overwrite_existing |
                      fs::copy_options::recursive,
                  errc);
    if (!errc)
    {
        fs::permissions(dst, perm);
    }
    return dst;
}

auto touch(const fs::path &fakeRoot, const fs::path &subdir,
           const fs::path &src, std::error_code &errc) -> fs::path
{
    auto perm = fs::status(src).permissions();
    auto dst = join(fakeRoot, {subdir, src.filename()});
    if (!fs::exists(dst.parent_path()))
    {
        fs::create_directories(dst.parent_path(), errc);
    }
    {
        std::ofstream _empty{dst, std::ios::trunc | std::ios::binary};
    }
    fs::permissions(dst, perm);
    return dst;
}

auto touch_to(const fs::path &dst, fs::perms permissions,
              std::error_code &errc) -> fs::path
{
    if (!fs::exists(dst.parent_path()))
    {
        fs::create_directories(dst.parent_path(), errc);
    }
    if (errc)
    {
        return dst;
    }
    {
        std::ofstream _empty{dst, std::ios::trunc | std::ios::binary};
    }
    fs::permissions(dst, permissions);
    return dst;
}

auto symlink_to(const fs::path &fakeRoot, const fs::path &bundlePath,
                const std::optional<fs::path> &targetPath,
                std::error_code &errc) -> fs::path
{
    auto dst = join(fakeRoot, bundlePath);
    if (!targetPath.has_value())
    {
        errc = std::make_error_code(std::errc::invalid_argument);
        return dst;
    }

    if (!fs::exists(dst.parent_path()))
    {
        fs::create_directories(dst.parent_path(), errc);
    }
    if (errc)
    {
        return dst;
    }

    fs::remove(dst, errc);
    if (errc)
    {
        return dst;
    }

    auto relative_target =
        targetPath->lexically_relative(bundlePath.parent_path());
    if (relative_target.empty())
    {
        relative_target = *targetPath;
    }
    fs::create_symlink(relative_target, dst, errc);
    return dst;
}

} // namespace

FakeRoot::FakeRoot(fs::path fakeRoot)
    : m_fakeRoot{std::move(fakeRoot)}
{
}

auto FakeRoot::path() const -> fs::path
{
    return m_fakeRoot;
}

[[nodiscard]] auto FakeRoot::addLibrary(const fs::path &src,
                                        std::error_code &errc) const
    -> fs::path
{
    return copy(m_fakeRoot, "lib64", src, errc);
}

[[nodiscard]] auto FakeRoot::binaryStub(const fs::path &src,
                                        std::error_code &errc) const
    -> fs::path
{
    return touch(m_fakeRoot, "bin", src, errc);
}

[[nodiscard]] auto FakeRoot::materialize(const BundleEntry &entry,
                                         std::error_code &errc) const
    -> fs::path
{
    bootstrap_runtime_layout(m_fakeRoot, errc);
    if (errc)
    {
        return join(m_fakeRoot, entry.bundledPath);
    }

    auto destination = join(m_fakeRoot, entry.bundledPath);
    switch (entry.kind)
    {
    case BundleEntryKind::Executable:
        return touch_to(destination,
                        fs::status(entry.sourcePath).permissions(),
                        errc);
    case BundleEntryKind::SharedObject:
    case BundleEntryKind::Interpreter:
        return copy_to(entry.sourcePath, destination, errc);
    case BundleEntryKind::SymlinkAlias:
        return symlink_to(m_fakeRoot, entry.bundledPath,
                          entry.linkTarget, errc);
    case BundleEntryKind::Unknown:
        break;
    }

    errc = std::make_error_code(std::errc::invalid_argument);
    return join(m_fakeRoot, entry.bundledPath);
}

[[nodiscard]] auto FakeRoot::stripRoot(const fs::path &path) const
    -> fs::path
{
    auto relative = path.lexically_relative(m_fakeRoot);
    if (relative.empty() || relative.string().starts_with(".."))
    {
        return path;
    }
    return fs::path{"/"} / relative;
}

} // namespace dynpax