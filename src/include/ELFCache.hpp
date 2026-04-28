#pragma once

#include "Resolver.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>


namespace dynpax
{

namespace fs = std::filesystem;

struct ELFCache
{
    ELFCache();
    ELFCache(ResolverOptions options);
    ELFCache(ELFCache &&) noexcept;
    auto operator=(ELFCache &&) noexcept -> ELFCache &;
    ~ELFCache();

    ELFCache(const ELFCache &) = delete;
    auto operator=(const ELFCache &) -> ELFCache & = delete;

    void swap(ELFCache &other) noexcept;

    /**
     * Populate the cache with the ELF files from the system.
     */
    auto populate() -> void;

    [[nodiscard]] auto getELFPath(const std::string &libName) const
        -> std::optional<std::filesystem::path>;

  private:
    std::unique_ptr<Resolver> m_resolver;
};

void swap(ELFCache &lhs, ELFCache &rhs) noexcept;

} // namespace dynpax