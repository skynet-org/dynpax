#pragma once

#include "ELFCache.hpp"
#include "LIEF/Abstract/Binary.hpp"
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dynpax
{
namespace fs = std::filesystem;
struct Executable
{
    explicit Executable(std::string name,
                        std::shared_ptr<ELFCache> cache);
    Executable(const Executable &rhs) = delete;
    Executable(Executable &&rhs) noexcept;
    auto operator=(const Executable &rhs) -> Executable & = delete;
    auto operator=(Executable &&rhs) noexcept -> Executable &;
    ~Executable();

    void swap(Executable &rhs) noexcept;

    [[nodiscard]] auto neededLibraries() const
        -> std::vector<std::string>;

    [[nodiscard]] auto interpreter() const
        -> std::expected<std::optional<fs::path>, std::runtime_error>;

    void interpreter(const fs::path &interpreter);

    [[nodiscard]] auto runpath() const
        -> std::expected<std::optional<std::vector<std::string>>,
                         std::runtime_error>;

    void runpath(const std::vector<std::string> &runpath);

    void rpath(const std::vector<std::string> &rpath);

    auto write(const fs::path &destFile) -> bool;

    explicit operator bool() const;

    [[nodiscard]] auto filePath() const -> fs::path;

  private:
    [[nodiscard]] auto needed(
        const std::unique_ptr<LIEF::Binary> &binary) const
        -> std::vector<std::string>;
    [[nodiscard]] auto needed(const std::string &name) const
        -> std::vector<std::string>;
    struct Impl;
    std::unique_ptr<Impl> pimpl{nullptr};
};

void swap(Executable &lhs, Executable &rhs) noexcept;

} // namespace dynpax
