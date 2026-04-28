#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dynpax
{

namespace fs = std::filesystem;

class ResolvedSymlink
{
    public:
        ResolvedSymlink(fs::path linkPath, fs::path targetPath);

        [[nodiscard]] auto linkPath() const -> const fs::path &;

        [[nodiscard]] auto targetPath() const -> const fs::path &;

    private:
        fs::path linkPath_;
        fs::path targetPath_;
};

class ResolvedDependency
{
    public:
        ResolvedDependency(std::string lookupName, fs::path aliasPath,
                                             fs::path canonicalPath,
                                             std::vector<ResolvedSymlink> aliasLinks = {});

        [[nodiscard]] auto lookupName() const -> const std::string &;

        [[nodiscard]] auto aliasPath() const -> const fs::path &;

        [[nodiscard]] auto canonicalPath() const -> const fs::path &;

    [[nodiscard]] auto aliasName() const -> std::string;
    [[nodiscard]] auto canonicalName() const -> std::string;
    [[nodiscard]] auto hasAlias() const -> bool;
        [[nodiscard]] auto aliasLinks() const
                -> const std::vector<ResolvedSymlink> &;

    private:
        std::string lookupName_;
        fs::path aliasPath_;
        fs::path canonicalPath_;
        std::vector<ResolvedSymlink> aliasLinks_;
};

struct ResolverOptions
{
    std::vector<fs::path> searchRoots;
    bool includeDefaultSearchRoots{true};
    bool includeLdConfigSearchRoots{true};
    bool includeEnvironmentSearchRoots{true};
};

struct Resolver
{
    Resolver();
    explicit Resolver(ResolverOptions options);
    Resolver(Resolver &&) noexcept;
    auto operator=(Resolver &&) noexcept -> Resolver &;
    ~Resolver();

    Resolver(const Resolver &) = delete;
    auto operator=(const Resolver &) -> Resolver & = delete;

    void swap(Resolver &other) noexcept;

    auto populate() -> void;

    [[nodiscard]] auto resolve(
        const std::string &libName,
        const std::vector<fs::path> &additionalSearchRoots = {}) const
        -> std::optional<ResolvedDependency>;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

void swap(Resolver &lhs, Resolver &rhs) noexcept;

} // namespace dynpax