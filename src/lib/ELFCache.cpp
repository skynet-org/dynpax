#include "ELFCache.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fstream>
#include <ranges>
#include <sys/mman.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace dynpax
{
namespace
{

auto is_elf(const char *path) -> bool
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
} // namespace

struct ELFCache::Impl
{
    struct linux_dirent64
    {
        uint64_t d_ino;
        int64_t d_off;
        unsigned short d_reclen;
        unsigned char d_type;
        char *d_name;
    };

    Impl(fs::path storageDir) : m_storageDir{std::move(storageDir)}
    {
    }

    auto populate()
    {
        scan();
    }

    [[nodiscard("Path must be used")]] auto getELFPath(
        const std::string &libName) const -> std::optional<fs::path>
    {
        auto iter = m_cache.find(libName);
        if (iter != m_cache.end())
        {
            return iter->second;
        }
        return std::nullopt;
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

            std::filesystem::recursive_directory_iterator iter(
                ldConfigDir, std::filesystem::directory_options::
                                     skip_permission_denied |
                                 std::filesystem::directory_options::
                                     follow_directory_symlink);

            for (const auto &entry : iter)
            {
                if (entry.is_regular_file())
                {
                    parse_ld_config_file(entry.path(), result);
                }
            }
        }
        return result;
    }

    auto scan() -> void
    {
        std::error_code errc{};
        constexpr static auto sysDirs = {"/lib", "/lib64", "/usr/lib",
                                         "/usr/lib64"};

        auto ldConfigDirs = parse_ld_config();
        auto envDirs = parse_env_var("LD_LIBRARY_PATH");
        std::vector<fs::path> search_dirs{sysDirs.begin(),
                                          sysDirs.end()};
        search_dirs.insert(search_dirs.end(), ldConfigDirs.begin(),
                           ldConfigDirs.end());
        search_dirs.insert(search_dirs.end(), envDirs.begin(),
                           envDirs.end());

        for (const auto &dir : search_dirs)
        {
            std::filesystem::recursive_directory_iterator root_iter(
                dir,
                std::filesystem::directory_options::
                        skip_permission_denied |
                    std::filesystem::directory_options::
                        follow_directory_symlink,
                errc);
            for (const auto &entry : root_iter)
            {
                if (entry.is_regular_file() &&
                    !m_cache.contains(entry.path().filename()) &&
                    is_elf(entry.path().c_str()))
                {
                    m_cache[entry.path().filename()] = entry.path();
                    continue;
                }
                if (entry.is_symlink())
                {
                    try
                    {
                        auto resolved = std::filesystem::read_symlink(
                            entry.path());
                        if (is_elf(resolved.c_str()))
                        {
                            m_cache[entry.path().filename()] = resolved;
                        }
                    }
                    catch (const std::filesystem::filesystem_error &)
                    {
                        fmt::print(stderr,
                                   "Failed to resolve symlink: {}\n",
                                   entry.path().string());
                    }
                }
            }
        }
    }

    fs::path m_storageDir;
    std::unordered_map<std::string, fs::path> m_cache;
};

ELFCache::ELFCache() : ELFCache(fs::current_path()) {};

ELFCache::ELFCache(fs::path storageDir)
    : m_impl{std::make_unique<Impl>(std::move(storageDir))}
{
}

ELFCache::ELFCache(ELFCache &&rhs) noexcept
{
    swap(rhs);
}

auto ELFCache::operator=(ELFCache &&rhs) noexcept -> ELFCache &
{
    swap(rhs);
    return *this;
}

ELFCache::~ELFCache() = default;

void ELFCache::swap(ELFCache &other) noexcept
{
    using std::swap;
    swap(m_impl, other.m_impl);
}

auto ELFCache::populate() -> void
{
    m_impl->populate();
}

[[nodiscard]] auto ELFCache::getELFPath(const std::string &libName)
    const -> std::optional<std::filesystem::path>
{
    return m_impl->getELFPath(libName);
}

void swap(ELFCache &lhs, ELFCache &rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace dynpax