#include "FakeRoot.hpp"
#include <filesystem>
#include <fmt/printf.h>
#include <fstream>
#include <initializer_list>
#include <numeric>
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