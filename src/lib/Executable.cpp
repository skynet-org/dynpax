#include "Executable.hpp"
#include "BundleManifest.hpp"
#include "BundleLayout.hpp"
#include "LIEF/Abstract/Binary.hpp"
#include "LIEF/Abstract/Parser.hpp"
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/DynamicEntry.hpp"
#include "LIEF/ELF/DynamicEntryRpath.hpp"
#include "LIEF/ELF/DynamicEntryRunPath.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "Resolver.hpp"
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
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dynpax
{
namespace
{

struct PendingDependency
{
    std::string requestedName;
    std::vector<fs::path> searchRoots;
};

struct PendingAliasEntry
{
    BundleEntry entry;
    fs::path sourceTargetPath;
};

auto normalize_or_canonical(const fs::path &path) -> fs::path
{
    std::error_code errc;
    auto canonical = fs::weakly_canonical(path, errc);
    if (errc)
    {
        return path.lexically_normal();
    }
    return canonical;
}

auto fallback_libc_name(
    const std::vector<std::string> &importedLibraries)
    -> std::optional<std::string>
{
    for (const auto &library : importedLibraries)
    {
        if (library == "libc.so.6")
        {
            return library;
        }
    }
    for (const auto &library : importedLibraries)
    {
        if (library.starts_with("libc.musl-") ||
            (library.starts_with("libc.") &&
             library.ends_with(".so.1")))
        {
            return library;
        }
    }
    return std::nullopt;
}

} // namespace

struct Executable::Impl
{
    explicit Impl(std::string name,
                  std::shared_ptr<Resolver> resolver)
        : binary_{LIEF::Parser::parse(name)},
          filePath_{std::move(name)}, resolver_{std::move(resolver)}
    {
    }

    explicit operator bool() const
    {
        return binary_ != nullptr;
    }

    [[nodiscard]] static auto importedLibraries(
        const std::unique_ptr<LIEF::Binary> &binary)
        -> std::vector<std::string>
    {
        if (!binary || !LIEF::ELF::Binary::classof(binary.get()))
        {
            return {};
        }

        auto &elf = static_cast<LIEF::ELF::Binary &>(*binary); // NOLINT
        auto imported = elf.imported_libraries();
        return {imported.begin(), imported.end()};
    }

    [[nodiscard]] static auto expandDynamicRoot(
        std::string rawPath, const fs::path &ownerPath)
        -> std::optional<fs::path>
    {
        auto origin = ownerPath.parent_path().string();

        auto replaceAll = [](std::string &value,
                             std::string_view needle,
                             const std::string &replacement) {
            auto position = size_t{0};
            while ((position = value.find(needle, position)) !=
                   std::string::npos)
            {
                value.replace(position, needle.size(), replacement);
                position += replacement.size();
            }
        };

        replaceAll(rawPath, "$ORIGIN", origin);
        replaceAll(rawPath, "${ORIGIN}", origin);

        if (rawPath.empty())
        {
            return std::nullopt;
        }

        auto expanded = fs::path{rawPath};
        if (!expanded.is_absolute())
        {
            expanded = ownerPath.parent_path() / expanded;
        }

        std::error_code errc;
        auto normalized = fs::weakly_canonical(expanded, errc);
        if (errc)
        {
            normalized = expanded.lexically_normal();
        }
        return normalized;
    }

    [[nodiscard]] static auto loaderSearchRoots(
        const std::unique_ptr<LIEF::Binary> &binary,
        const fs::path &ownerPath) -> std::vector<fs::path>
    {
        if (!binary || !LIEF::ELF::Binary::classof(binary.get()))
        {
            return {};
        }

        auto collectPaths = [&](LIEF::ELF::DynamicEntry::TAG tag) {
            auto roots = std::vector<fs::path>{};
            auto &elf = static_cast<LIEF::ELF::Binary &>(*binary); // NOLINT
            for (auto &entry : elf.dynamic_entries())
            {
                if (entry.tag() != tag)
                {
                    continue;
                }

                auto paths = std::vector<std::string>{};
                if (tag == LIEF::ELF::DynamicEntry::TAG::RUNPATH)
                {
                    paths = static_cast<
                                LIEF::ELF::DynamicEntryRunPath &>(entry)
                                .paths();
                }
                else
                {
                    paths = static_cast<LIEF::ELF::DynamicEntryRpath &>(
                                entry)
                                .paths();
                }

                for (const auto &path : paths)
                {
                    auto expanded = expandDynamicRoot(path, ownerPath);
                    if (expanded.has_value())
                    {
                        roots.push_back(*expanded);
                    }
                }
            }
            return roots;
        };

        auto roots = collectPaths(LIEF::ELF::DynamicEntry::TAG::RUNPATH);
        if (roots.empty())
        {
            roots = collectPaths(LIEF::ELF::DynamicEntry::TAG::RPATH);
        }

        auto normalizedRoots = std::vector<fs::path>{};
        auto seen = std::unordered_set<std::string>{};
        for (const auto &root : roots)
        {
            auto key = root.lexically_normal().string();
            if (seen.insert(key).second)
            {
                normalizedRoots.push_back(root);
            }
        }

        return normalizedRoots;
    }

    [[nodiscard]] static auto makeEntry(
        const fs::path &sourcePath,
        const std::unique_ptr<LIEF::Binary> &binary,
        std::string requestedName, BundleEntryKind kind,
        BundleLayoutPolicy layoutPolicy,
        std::optional<fs::path> fallbackLibraryDirectory = std::nullopt,
        fs::path bundledPath = {})
        -> BundleEntry
    {
        auto entry = BundleEntry{};
        entry.sourcePath = sourcePath;
        entry.kind = kind;
        entry.requestedName = std::move(requestedName);
        entry.bundledPath =
            bundledPath.empty()
                ? bundle_path_for(layoutPolicy, kind, sourcePath,
                                  fallbackLibraryDirectory)
                : std::move(bundledPath);
        if (binary && LIEF::ELF::Binary::classof(binary.get()))
        {
            auto &elf =
                static_cast<LIEF::ELF::Binary &>(*binary); // NOLINT
            entry.hasInterpreter = elf.has_interpreter();
        }
        entry.rewritePlan.needsInterpreterRewrite =
            entry.hasInterpreter;
        return entry;
    }

    [[nodiscard]] auto dependencyManifest(
        bool includeInterpreter, BundleLayoutPolicy layoutPolicy) const
        -> BundleManifest
    {
        auto manifest = BundleManifest{};
        manifest.primaryInput = filePath_;

        if (!binary_ || !LIEF::ELF::Binary::classof(binary_.get()))
        {
            return manifest;
        }

        manifest.entries.push_back(makeEntry(
            filePath_, binary_, filePath_.filename().string(),
            BundleEntryKind::Executable, layoutPolicy));

        auto interpreter_path = std::optional<fs::path>{};
        auto interpreter_canonical_key = std::optional<std::string>{};
        if (includeInterpreter)
        {
            auto binary_interpreter = interpreter();
            if (binary_interpreter && binary_interpreter.value())
            {
                interpreter_path = binary_interpreter.value().value();
                std::error_code errc;
                auto canonical_interpreter =
                    fs::weakly_canonical(*interpreter_path, errc);
                if (errc)
                {
                    canonical_interpreter =
                        interpreter_path->lexically_normal();
                }
                interpreter_canonical_key =
                    canonical_interpreter.string();
            }
        }

        auto seen_payloads = std::unordered_set<std::string>{};
        auto seen_aliases = std::unordered_set<std::string>{};
        auto plannedBundlePathsBySource =
            std::unordered_map<std::string, fs::path>{};
        auto plannedBundlePathsByCanonical =
            std::unordered_map<std::string, fs::path>{};
        auto registerPlannedPayloadPath =
            [&](const fs::path &sourcePath, const fs::path &bundledPath) {
                auto normalizedSource = sourcePath.lexically_normal();
                plannedBundlePathsBySource.insert_or_assign(
                    normalizedSource.string(), bundledPath);
                auto canonicalSource =
                    normalize_or_canonical(sourcePath);
                plannedBundlePathsByCanonical.insert_or_assign(
                    canonicalSource.string(), bundledPath);
            };
        auto registerPlannedAliasPath =
            [&](const fs::path &sourcePath, const fs::path &bundledPath) {
                auto normalizedSource = sourcePath.lexically_normal();
                plannedBundlePathsBySource.insert_or_assign(
                    normalizedSource.string(), bundledPath);
            };
        auto resolvePlannedLinkTarget =
            [&](const fs::path &sourceTargetPath,
                const fs::path &fallbackTargetPath) {
                auto normalizedTarget =
                    sourceTargetPath.lexically_normal();
                if (auto exactIter = plannedBundlePathsBySource.find(
                        normalizedTarget.string());
                    exactIter != plannedBundlePathsBySource.end())
                {
                    return exactIter->second;
                }

                auto canonicalTarget =
                    normalize_or_canonical(sourceTargetPath);
                if (auto canonicalIter = plannedBundlePathsByCanonical.find(
                        canonicalTarget.string());
                    canonicalIter != plannedBundlePathsByCanonical.end())
                {
                    return canonicalIter->second;
                }

                return fallbackTargetPath;
            };
        auto imported = importedLibraries(binary_);
        auto initialSearchRoots =
            loaderSearchRoots(binary_, filePath_);
        auto fallbackLibraryDirectory =
            std::optional<fs::path>{};
        if (layoutPolicy == BundleLayoutPolicy::PreserveSourceTree)
        {
            if (auto libcName = fallback_libc_name(imported);
                libcName.has_value())
            {
                auto libcDependency = resolver_->resolve(
                    *libcName, initialSearchRoots);
                if (libcDependency.has_value())
                {
                    fallbackLibraryDirectory = bundle_path_for(
                        layoutPolicy,
                        BundleEntryKind::SharedObject,
                        libcDependency->canonicalPath())
                                                   .parent_path();
                }
            }
            if (!fallbackLibraryDirectory.has_value() &&
                interpreter_path.has_value())
            {
                fallbackLibraryDirectory = bundle_path_for(
                    layoutPolicy, BundleEntryKind::Interpreter,
                    *interpreter_path)
                                               .parent_path();
            }
        }
        auto todo = std::queue<PendingDependency>{};
        for (const auto &importedLibrary : imported)
        {
            todo.push(
                PendingDependency{importedLibrary, initialSearchRoots});
        }
        while (!todo.empty())
        {
            auto pending = std::move(todo.front());
            todo.pop();

            auto requested_name =
                fs::path(pending.requestedName).filename().string();

            auto resolved = resolver_->resolve(requested_name,
                                               pending.searchRoots);
            if (!resolved.has_value())
            {
                throw std::runtime_error{fmt::format(
                    "Failed to resolve library: {}",
                    requested_name)}; // NOLINT
            }

            auto canonical_key = resolved->canonicalPath()
                                     .lexically_normal()
                                     .string();
            if (interpreter_canonical_key.has_value() &&
                canonical_key == *interpreter_canonical_key)
            {
                continue;
            }

            auto canonical_bundle_path = bundle_path_for(
                layoutPolicy, BundleEntryKind::SharedObject,
                resolved->canonicalPath(), fallbackLibraryDirectory);
            registerPlannedPayloadPath(resolved->canonicalPath(),
                                       canonical_bundle_path);
            if (seen_payloads.insert(canonical_key).second)
            {
                auto dependency_binary = LIEF::Parser::parse(
                    resolved->canonicalPath().string());
                if (!dependency_binary)
                {
                    continue;
                }

                manifest.entries.push_back(makeEntry(
                    resolved->canonicalPath(), dependency_binary,
                    requested_name, BundleEntryKind::SharedObject,
                    layoutPolicy, fallbackLibraryDirectory,
                    canonical_bundle_path));

                auto nested = importedLibraries(dependency_binary);
                auto nestedSearchRoots = loaderSearchRoots(
                    dependency_binary, resolved->canonicalPath());
                for (const auto &nested_dep : nested)
                {
                    todo.push(PendingDependency{nested_dep,
                                                nestedSearchRoots});
                }
            }

            auto pendingAliasEntries =
                std::vector<PendingAliasEntry>{};
            for (const auto &aliasLink : resolved->aliasLinks())
            {
                auto alias_bundle_path = bundle_path_for(
                    layoutPolicy, BundleEntryKind::SymlinkAlias,
                    aliasLink.linkPath(), fallbackLibraryDirectory);
                auto alias_key =
                    alias_bundle_path.lexically_normal().string();
                if (seen_aliases.insert(alias_key).second)
                {
                    auto alias_entry = BundleEntry{};
                    alias_entry.sourcePath = aliasLink.linkPath();
                    alias_entry.bundledPath = alias_bundle_path;
                    alias_entry.kind = BundleEntryKind::SymlinkAlias;
                    alias_entry.requestedName =
                        aliasLink.linkPath().filename().string();
                    registerPlannedAliasPath(aliasLink.linkPath(),
                                             alias_bundle_path);
                    pendingAliasEntries.push_back(PendingAliasEntry{
                        .entry = std::move(alias_entry),
                        .sourceTargetPath = aliasLink.targetPath(),
                    });
                }
            }

            for (auto &pendingAliasEntry : pendingAliasEntries)
            {
                auto fallbackTargetPath = bundle_path_for(
                    layoutPolicy, BundleEntryKind::SymlinkAlias,
                    pendingAliasEntry.sourceTargetPath,
                    fallbackLibraryDirectory);
                pendingAliasEntry.entry.linkTarget =
                    resolvePlannedLinkTarget(
                        pendingAliasEntry.sourceTargetPath,
                        fallbackTargetPath);
                manifest.entries.push_back(
                    std::move(pendingAliasEntry.entry));
            }
        }

        if (interpreter_path.has_value())
        {
            auto interpreter_binary =
                LIEF::Parser::parse(interpreter_path->string());
            manifest.entries.push_back(makeEntry(
                *interpreter_path, interpreter_binary,
                interpreter_path->filename().string(),
                BundleEntryKind::Interpreter, layoutPolicy));
        }

        return manifest;
    }
    [[nodiscard]] auto interpreter() const
        -> std::expected<std::optional<fs::path>, std::runtime_error>
    {
        if (binary_ && LIEF::ELF::Binary::classof(binary_.get()))
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
        if (binary_ && LIEF::ELF::Binary::classof(binary_.get()))
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

    [[nodiscard]] auto rpath() const
        -> std::expected<std::optional<std::vector<std::string>>,
                         std::runtime_error>
    {
        if (binary_ && LIEF::ELF::Binary::classof(binary_.get()))
        {
            // NOLINTNEXTLINE
            auto &elf = static_cast<LIEF::ELF::Binary &>(*binary_);
            for (auto &entry : elf.dynamic_entries())
            {
                if (entry.tag() == LIEF::ELF::DynamicEntry::TAG::RPATH)
                {
                    // NOLINTNEXTLINE
                    return static_cast<LIEF::ELF::DynamicEntryRpath &>(
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
        if (binary_ && LIEF::ELF::Binary::classof(binary_.get()))
        {
            // NOLINTNEXTLINE
            auto &elf = static_cast<LIEF::ELF::Binary &>(*binary_);
            if (elf.has_interpreter())
            {
                elf.interpreter(path);
            }
        }
    }

    void runpath(const std::vector<std::string> &runpath)
    {
        if (binary_ && LIEF::ELF::Binary::classof(binary_.get()))
        {
            auto &elf =
                static_cast<LIEF::ELF::Binary &>(*binary_); // NOLINT
            auto *existing =
                elf.get(LIEF::ELF::DynamicEntry::TAG::RUNPATH);
            if (existing != nullptr)
            {
                static_cast<LIEF::ELF::DynamicEntryRunPath &>(*existing)
                    .paths(runpath);
            }
            else if (!runpath.empty())
            {
                LIEF::ELF::DynamicEntryRunPath entry{runpath};
                elf.add(entry);
            }

            while (true)
            {
                auto *legacy =
                    elf.get(LIEF::ELF::DynamicEntry::TAG::RPATH);
                if (legacy == nullptr)
                {
                    break;
                }
                elf.remove(*legacy);
            }
        }
    }

    void rpath(const std::vector<std::string> &rpath)
    {
        if (binary_ && LIEF::ELF::Binary::classof(binary_.get()))
        {
            auto &elf =
                static_cast<LIEF::ELF::Binary &>(*binary_); // NOLINT
            auto *existing = elf.get(LIEF::ELF::DynamicEntry::TAG::RPATH);
            if (existing != nullptr)
            {
                elf.remove(*existing);
            }
            if (!rpath.empty())
            {
                LIEF::ELF::DynamicEntryRpath entry{};
                entry.paths(rpath);
                elf.add(entry);
            }
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
    std::shared_ptr<Resolver> resolver_;
};

Executable::Executable(std::string name,
                       std::shared_ptr<Resolver> resolver)
    : pimpl{std::make_unique<Impl>(std::move(name),
                                   std::move(resolver))}
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
    return pimpl && static_cast<bool>(*pimpl);
}

[[nodiscard]] auto Executable::interpreter() const
    -> std::expected<std::optional<fs::path>, std::runtime_error>
{
    return pimpl->interpreter();
}

[[nodiscard]] auto Executable::dependencyManifest(
    bool includeInterpreter, BundleLayoutPolicy layoutPolicy) const
    -> BundleManifest
{
    return pimpl->dependencyManifest(includeInterpreter,
                                     layoutPolicy);
}

[[nodiscard]] auto Executable::runpath() const
    -> std::expected<std::optional<std::vector<std::string>>,
                     std::runtime_error>
{
    return pimpl->runpath();
}

[[nodiscard]] auto Executable::rpath() const
    -> std::expected<std::optional<std::vector<std::string>>,
                     std::runtime_error>
{
    return pimpl->rpath();
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

void Executable::rpath(const std::vector<std::string> &rpath)
{
    pimpl->rpath(rpath);
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