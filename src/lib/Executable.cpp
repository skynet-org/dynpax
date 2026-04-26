#include "Executable.hpp"
#include "LIEF/Abstract/Binary.hpp"
#include "LIEF/Abstract/Parser.hpp"
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/DynamicEntry.hpp"
#include "LIEF/ELF/DynamicEntryRunPath.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LibraryResolver.hpp"
#include <LIEF/ELF.hpp>
#include <LIEF/LIEF.hpp>
#include <LIEF/Object.hpp>
#include <algorithm>
#include <expected>
#include <filesystem>
#include <fmt/printf.h>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dynpax
{

struct Executable::Impl
{
    explicit Impl(std::string name)
        : binary_{LIEF::Parser::parse(name)},
          filePath_{std::move(name)}
    {
    }

    explicit operator bool() const
    {
        return binary_ != nullptr;
    }

    [[nodiscard]] auto needed(const std::string &name) const
        -> std::vector<std::string>
    {
        auto binary = LIEF::Parser::parse(name);
        if (!binary)
        {
            return {};
        }
        return needed(binary);
    }

    [[nodiscard]] auto needed(
        const std::unique_ptr<LIEF::Binary> &binary) const
        -> std::vector<std::string>
    {
        auto result = std::vector<std::string>{};
        if (LIEF::ELF::Binary::classof(binary.get()))
        {
            // NOLINTNEXTLINE
            auto &elf = static_cast<LIEF::ELF::Binary &>(*binary);
            auto seen =
                std::unordered_map<std::string, std::string>{};
            auto imported = elf.imported_libraries();
            auto todo = std::queue<std::string>{imported.begin(),
                                                imported.end()};
            while (!todo.empty())
            {
                auto lib = todo.front();
                auto path = LibraryResolver::resolveLibrary(lib);
                if (!path.has_value())
                {
                    throw std::runtime_error{fmt::format(
                        "Failed to resolve library {}: {}", lib,
                        path.error().what())}; // NOLINT
                }
                todo.pop();
                if (seen.contains(lib))
                {
                    continue;
                }
                seen.insert({lib, path.value()});
                auto nested = needed(path.value());
                for (auto &nested_dep : nested)
                {
                    if (!seen.contains(nested_dep))
                    {
                        todo.push(nested_dep);
                    }
                }
            }
            result.reserve(seen.size());
            std::ranges::copy(seen | std::views::values,
                              std::back_inserter(result));
        }
        return result;
    }

    [[nodiscard]] auto neededLibraries() const
    {
        return needed(binary_);
    }

    [[nodiscard]] auto interpreter() const
        -> std::expected<std::optional<fs::path>, std::runtime_error>
    {
        if (LIEF::ELF::Binary::classof(binary_.get()))
        {
            // NOLINTNEXTLINE
            auto &elf = static_cast<LIEF::ELF::Binary &>(*binary_);
            if (!elf.has_interpreter())
            {
                return std::optional<fs::path>{};
            }
            return elf.interpreter();
        }
        return std::unexpected{std::runtime_error{"not ELF binary"}};
    }

    [[nodiscard]] auto runpath() const
        -> std::expected<std::optional<std::vector<std::string>>,
                         std::runtime_error>
    {
        if (LIEF::ELF::Binary::classof(binary_.get()))
        {
            // NOLINTNEXTLINE
            auto &elf = static_cast<LIEF::ELF::Binary &>(*binary_);
            for (auto &entry : elf.dynamic_entries())
            {
                if (entry.tag() ==
                    LIEF::ELF::DynamicEntry::TAG::RUNPATH)
                {
                    // NOLINTNEXTLINE
                    return static_cast<
                               LIEF::ELF::DynamicEntryRunPath &>(
                               entry)
                        .paths();
                }
            }
            return std::optional<std::vector<std::string>>{};
        }
        return std::unexpected{std::runtime_error{"not ELF binary"}};
    }

    void interpreter(const fs::path &path)
    {
        // TODO: this still does not work
        if (LIEF::ELF::Binary::classof(binary_.get()))
        {
            // NOLINTNEXTLINE
            auto &elf = static_cast<LIEF::ELF::Binary &>(*binary_);
            // for (auto &seg : elf.segments())
            // {
            //     if (seg.is_interpreter())
            //     {
            //         const auto &cnt =
            //             path.lexically_normal().u8string();
            //         // NOLINTNEXTLINE
            //         const auto *uint8Data =
            //             reinterpret_cast<const uint8_t *>( //
            //             NOLINT
            //                 cnt.data());
            //         // NOLINTNEXTLINE
            //         seg.content(std::vector(
            //             uint8Data,
            //             uint8Data + cnt.size())); // NOLINT
            //         fmt::println("Updated interpreter ...");
            //         return;
            //     }
            // }

            // fmt::println("Could not find interpreter segment");

            elf.interpreter(path.lexically_normal());
        }
    }

    void runpath(const std::vector<std::string> &runpath)
    {
        if (LIEF::ELF::Binary::classof(binary_.get()))
        {
            auto &elf =
                static_cast<LIEF::ELF::Binary &>(*binary_); // NOLINT
            LIEF::ELF::DynamicEntryRunPath entry{runpath};
            elf.add(entry);
        }
    }

    auto write(const fs::path &destFile) -> bool
    {
        try
        {
            if (destFile.has_parent_path() &&
                !fs::exists(destFile.parent_path()))
            {
                std::error_code errc;
                fs::create_directories(destFile.parent_path(), errc);
            }
            if (LIEF::ELF::Binary::classof(binary_.get()))
            {
                // NOLINTNEXTLINE
                auto &elf = static_cast<LIEF::ELF::Binary &>(
                    *binary_); // NOLINT
                LIEF::ELF::Builder::config_t builder_config{};
                elf.write(destFile.string(), builder_config);
            }
            else
            {
                return false;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    [[nodiscard]] auto filePath() const -> fs::path
    {
        return filePath_;
    }

  private:
    decltype(LIEF::Parser::parse("")) binary_{nullptr};
    fs::path filePath_;
};

Executable::Executable(std::string name)
    : pimpl{std::make_unique<Impl>(std::move(name))}
{
}

Executable::Executable(Executable &&rhs) noexcept
{
    if (this != &rhs)
    {
        swap(rhs);
    }
}

auto Executable::operator=(Executable &&rhs) noexcept -> Executable &
{
    if (this == &rhs)
    {
        return *this;
    }
    swap(rhs);
    return *this;
}

Executable::~Executable() = default;

void Executable::swap(Executable &rhs) noexcept
{
    std::swap(this->pimpl, rhs.pimpl);
}

Executable::operator bool() const
{
    return bool(pimpl);
}

[[nodiscard]] auto Executable::interpreter() const
    -> std::expected<std::optional<fs::path>, std::runtime_error>
{
    return pimpl->interpreter();
}

[[nodiscard]] auto Executable::neededLibraries() const
    -> std::vector<std::string>
{
    return pimpl->neededLibraries();
}

[[nodiscard]] auto Executable::runpath() const
    -> std::expected<std::optional<std::vector<std::string>>,
                     std::runtime_error>
{
    return pimpl->runpath();
}

auto Executable::write(const fs::path &destFile) -> bool
{
    return pimpl->write(destFile);
}

void Executable::interpreter(const fs::path &interpreter)
{
    pimpl->interpreter(interpreter);
}

void Executable::runpath(const std::vector<std::string> &runpath)
{
    pimpl->runpath(runpath);
}

[[nodiscard]] auto Executable::filePath() const -> fs::path
{
    return pimpl->filePath();
}

void swap(Executable &lhs, Executable &rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace dynpax