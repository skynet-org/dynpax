#include "Resolver.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fmt/printf.h>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dynpax
{
namespace
{

auto is_elf(const fs::path &path) -> bool
{
    static constexpr std::array<uint8_t, 4> ELF_MAGIC = {0x7F, 0x45,
                                                         0x4C, 0x46};
    static constexpr size_t magic_size = ELF_MAGIC.size();

    std::ifstream file{path, std::ios::binary};
    if (!file)
    {
        return false;
    }
    std::array<char, 4> buffer{0};
    file.read(buffer.data(), magic_size);
    return std::equal(buffer.begin(), buffer.end(),
                      ELF_MAGIC.begin());
}

auto canonicalize(const fs::path &path) -> fs::path
{
    std::error_code errc;
    auto canonical = fs::weakly_canonical(path, errc);
    if (errc)
    {
        return path.lexically_normal();
    }
    return canonical;
}

auto normalize_link_target(const fs::path &linkPath,
                           const fs::path &targetPath) -> fs::path
{
    if (targetPath.is_absolute())
    {
        return targetPath.lexically_normal();
    }
    return (linkPath.parent_path() / targetPath).lexically_normal();
}

auto collect_sorted_paths(const fs::path &root) -> std::vector<fs::path>
{
    auto errc = std::error_code{};
    auto paths = std::vector<fs::path>{};
    std::filesystem::recursive_directory_iterator iter(
        root, std::filesystem::directory_options::skip_permission_denied |
                  std::filesystem::directory_options::follow_directory_symlink,
        errc);
    for (const auto &entry : iter)
    {
        paths.push_back(entry.path());
    }
    std::ranges::sort(paths, [](const auto &left, const auto &right) {
        return left.string() < right.string();
    });
    return paths;
}

} // namespace

struct Resolver::Impl
{
    struct ResolvedRecord
    {
        std::string lookupName;
        fs::path aliasPath;
        fs::path canonicalPath;
    };

    struct ScanResult
    {
        std::unordered_map<std::string, ResolvedRecord> records;
        std::unordered_map<std::string, std::vector<ResolvedSymlink>>
            aliasLinksByCanonical;
    };

    explicit Impl(ResolverOptions options)
        : m_options{std::move(options)}
    {
    }

    auto populate() -> void
    {
        scan();
    }

    [[nodiscard]] auto resolve(
        const std::string &libName,
        const std::vector<fs::path> &additionalSearchRoots) const
        -> std::optional<ResolvedDependency>
    {
        if (!additionalSearchRoots.empty())
        {
            auto dynamicScan = scanRoots(additionalSearchRoots);
            auto dynamicResolved = makeResolvedDependency(
                libName, dynamicScan.records,
                dynamicScan.aliasLinksByCanonical);
            if (dynamicResolved.has_value())
            {
                return dynamicResolved;
            }
        }

        return makeResolvedDependency(libName, m_records,
                                      m_aliasLinksByCanonical);
    }

  private:
    static auto split_colon(std::string_view value)
        -> std::vector<std::string_view>
    {
        std::vector<std::string_view> out{};
        for (auto part : value | std::views::split(':'))
        {
            out.emplace_back(
                part.begin(),
                static_cast<size_t>(std::ranges::distance(part)));
        }

        return out;
    }

    static auto parse_env_var(const char *name)
        -> std::vector<fs::path>
    {
        auto result = std::vector<fs::path>{};
        auto *env = std::getenv(name); // NOLINT
        if (env == nullptr)
        {
            return result;
        }
        auto value = std::string{env};
        auto paths = split_colon(value);
        for (const auto &path : paths)
        {
            if (!path.empty())
            {
                result.emplace_back(path);
            }
        }
        return result;
    }

    static void parse_ld_config_file(const fs::path &path,
                                     std::vector<fs::path> &acc)
    {
        std::ifstream file{path};
        if (!file)
        {
            fmt::print(stderr, "Failed to open ld config file: {}\n",
                       "/etc/ld.so.conf");
            return;
        }
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line.starts_with('#'))
            {
                continue;
            }
            if (line.starts_with('/'))
            {
                acc.emplace_back(line);
            }
        }
    }

    static auto parse_ld_config() -> std::vector<fs::path>
    {
        constexpr static auto ldConfigPath = "/etc/ld.so.conf";
        constexpr static auto ldConfigDir = "/etc/ld.so.conf.d";
        auto result = std::vector<fs::path>{};
        if (fs::exists(ldConfigPath))
        {
            parse_ld_config_file(ldConfigPath, result);
        }

        if (fs::exists(ldConfigDir))
        {
            for (const auto &entryPath : collect_sorted_paths(ldConfigDir))
            {
                if (fs::is_regular_file(entryPath))
                {
                    parse_ld_config_file(entryPath, result);
                }
            }
        }
        return result;
    }

    [[nodiscard]] static auto makeResolvedDependency(
        const std::string &libName,
        const std::unordered_map<std::string, ResolvedRecord> &records,
        const std::unordered_map<std::string,
                                 std::vector<ResolvedSymlink>>
            &aliasLinksByCanonical)
        -> std::optional<ResolvedDependency>
    {
        auto iter = records.find(libName);
        if (iter == records.end())
        {
            return std::nullopt;
        }

        auto aliasLinks = std::vector<ResolvedSymlink>{};
        auto aliasIter = aliasLinksByCanonical.find(
            iter->second.canonicalPath.string());
        if (aliasIter != aliasLinksByCanonical.end())
        {
            aliasLinks = aliasIter->second;
            std::ranges::sort(aliasLinks,
                              [](const auto &left, const auto &right) {
                                  return left.linkPath().string() <
                                         right.linkPath().string();
                              });
        }

        return ResolvedDependency{iter->second.lookupName,
                                  iter->second.aliasPath,
                                  iter->second.canonicalPath,
                                  std::move(aliasLinks)};
    }

    static void register_alias_link(
        std::unordered_map<std::string, std::vector<ResolvedSymlink>>
            &aliasLinksByCanonical,
        const fs::path &canonicalPath, ResolvedSymlink candidate)
    {
        auto &aliasLinks =
            aliasLinksByCanonical[canonicalPath.string()];
        auto found = std::ranges::find_if(
            aliasLinks, [&](const auto &existingAliasLink) {
                return existingAliasLink.linkPath() ==
                           candidate.linkPath() &&
                       existingAliasLink.targetPath() ==
                           candidate.targetPath();
            });
        if (found == aliasLinks.end())
        {
            aliasLinks.push_back(std::move(candidate));
        }
    }

    [[nodiscard]] static auto scanRoots(
        const std::vector<fs::path> &searchDirs) -> ScanResult
    {
        auto result = ScanResult{};
        std::error_code errc{};

        for (const auto &dir : searchDirs)
        {
            if (!fs::exists(dir))
            {
                continue;
            }
            for (const auto &entryPath : collect_sorted_paths(dir))
            {
                if (fs::is_symlink(entryPath))
                {
                    try
                    {
                        auto symlink_target =
                            std::filesystem::read_symlink(
                            entryPath);
                        auto resolved_path =
                            normalize_link_target(entryPath,
                                                  symlink_target);
                        if (is_elf(resolved_path))
                        {
                            auto lookup_name =
                                entryPath.filename().string();
                            auto canonical_path =
                                canonicalize(resolved_path);
                            result.records.insert_or_assign(
                                lookup_name,
                                ResolvedRecord{lookup_name, entryPath,
                                               canonical_path});
                            register_alias_link(
                                result.aliasLinksByCanonical,
                                canonical_path,
                                ResolvedSymlink{entryPath,
                                                resolved_path});
                            continue;
                        }
                    }
                    catch (const std::filesystem::filesystem_error &)
                    {
                        fmt::print(stderr,
                                   "Failed to resolve symlink: {}\n",
                                   entryPath.string());
                    }
                }
                if (fs::is_regular_file(entryPath) &&
                    !result.records.contains(
                        entryPath.filename().string()) &&
                    is_elf(entryPath))
                {
                    auto lookup_name =
                        entryPath.filename().string();
                    result.records.emplace(
                        lookup_name,
                        ResolvedRecord{lookup_name, entryPath,
                                       canonicalize(entryPath)});
                }
            }
        }

        return result;
    }

    [[nodiscard]] auto collectSearchDirs() const
        -> std::vector<fs::path>
    {
        constexpr static auto sysDirs = {"/lib", "/lib64", "/usr/lib",
                                         "/usr/lib64"};

        auto searchDirs = std::vector<fs::path>{};
        if (m_options.includeDefaultSearchRoots)
        {
            searchDirs.insert(searchDirs.end(), sysDirs.begin(),
                              sysDirs.end());
        }
        if (m_options.includeLdConfigSearchRoots)
        {
            auto ldConfigDirs = parse_ld_config();
            searchDirs.insert(searchDirs.end(), ldConfigDirs.begin(),
                              ldConfigDirs.end());
        }
        if (m_options.includeEnvironmentSearchRoots)
        {
            auto envDirs = parse_env_var("LD_LIBRARY_PATH");
            searchDirs.insert(searchDirs.end(), envDirs.begin(),
                              envDirs.end());
        }
        searchDirs.insert(searchDirs.end(), m_options.searchRoots.begin(),
                          m_options.searchRoots.end());
        return searchDirs;
    }

    auto scan() -> void
    {
        auto scanResult = scanRoots(collectSearchDirs());
        m_records = std::move(scanResult.records);
        m_aliasLinksByCanonical =
            std::move(scanResult.aliasLinksByCanonical);
    }

    ResolverOptions m_options;
    std::unordered_map<std::string, ResolvedRecord> m_records;
    std::unordered_map<std::string, std::vector<ResolvedSymlink>>
        m_aliasLinksByCanonical;
};

ResolvedSymlink::ResolvedSymlink(fs::path linkPath, fs::path targetPath)
    : linkPath_{std::move(linkPath)},
      targetPath_{std::move(targetPath)}
{
}

auto ResolvedSymlink::linkPath() const -> const fs::path &
{
    return linkPath_;
}

auto ResolvedSymlink::targetPath() const -> const fs::path &
{
    return targetPath_;
}

ResolvedDependency::ResolvedDependency(std::string lookupName,
                                       fs::path aliasPath,
                                       fs::path canonicalPath,
                                       std::vector<ResolvedSymlink> aliasLinks)
    : lookupName_{std::move(lookupName)},
      aliasPath_{std::move(aliasPath)},
      canonicalPath_{std::move(canonicalPath)},
      aliasLinks_{std::move(aliasLinks)}
{
}

auto ResolvedDependency::lookupName() const -> const std::string &
{
    return lookupName_;
}

auto ResolvedDependency::aliasPath() const -> const fs::path &
{
    return aliasPath_;
}

auto ResolvedDependency::canonicalPath() const -> const fs::path &
{
    return canonicalPath_;
}

auto ResolvedDependency::aliasName() const -> std::string
{
    return aliasPath_.filename().string();
}

auto ResolvedDependency::canonicalName() const -> std::string
{
    return canonicalPath_.filename().string();
}

auto ResolvedDependency::hasAlias() const -> bool
{
    return aliasPath_.filename() != canonicalPath_.filename() ||
           canonicalize(aliasPath_) != canonicalPath_;
}

auto ResolvedDependency::aliasLinks() const
    -> const std::vector<ResolvedSymlink> &
{
    return aliasLinks_;
}

Resolver::Resolver() : Resolver(ResolverOptions{}) {}

Resolver::Resolver(ResolverOptions options)
    : m_impl{std::make_unique<Impl>(std::move(options))}
{
}

Resolver::Resolver(Resolver &&rhs) noexcept
{
    swap(rhs);
}

auto Resolver::operator=(Resolver &&rhs) noexcept -> Resolver &
{
    swap(rhs);
    return *this;
}

Resolver::~Resolver() = default;

void Resolver::swap(Resolver &other) noexcept
{
    using std::swap;
    swap(m_impl, other.m_impl);
}

auto Resolver::populate() -> void
{
    m_impl->populate();
}

[[nodiscard]] auto Resolver::resolve(
    const std::string &libName,
    const std::vector<fs::path> &additionalSearchRoots) const
    -> std::optional<ResolvedDependency>
{
    return m_impl->resolve(libName, additionalSearchRoots);
}

void swap(Resolver &lhs, Resolver &rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace dynpax