#include "Resolver.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <stdlib.h>

namespace fs = std::filesystem;

namespace
{

class TempDir
{
  public:
    TempDir()
    {
        auto pathTemplate =
            std::to_array("/tmp/dynpax-resolver-XXXXXX");

        auto *created = ::mkdtemp(pathTemplate.data());
        if (created == nullptr)
        {
            throw std::runtime_error{"mkdtemp failed"};
        }
        path_ = created;
    }

    TempDir(const TempDir &) = delete;
    auto operator=(const TempDir &) -> TempDir & = delete;
    TempDir(TempDir &&) = delete;
    auto operator=(TempDir &&) -> TempDir & = delete;

    ~TempDir()
    {
        std::error_code errc;
        fs::remove_all(path_, errc);
    }

    [[nodiscard]] auto path() const -> const fs::path &
    {
        return path_;
    }

  private:
    fs::path path_;
};

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error{std::string{message}};
    }
}

void write_fake_elf(const fs::path &path)
{
    std::error_code errc;
    fs::create_directories(path.parent_path(), errc);
    if (errc)
    {
        throw std::runtime_error{"create_directories failed"};
    }

    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file)
    {
        throw std::runtime_error{"failed to create fake ELF"};
    }

    static constexpr auto ELF_MAGIC =
        std::array<char, 8>{0x7F, 'E', 'L', 'F', 0x02, 0x01, 0x01,
                            0x00};
    file.write(ELF_MAGIC.data(),
               static_cast<std::streamsize>(ELF_MAGIC.size()));
    static constexpr auto PADDING =
        std::array<char, 4>{0x00, 0x00, 0x00, 0x00};
    file.write(PADDING.data(),
               static_cast<std::streamsize>(PADDING.size()));
}

auto make_options(const fs::path &root) -> dynpax::ResolverOptions
{
    auto options = dynpax::ResolverOptions{};
    options.searchRoots = {root};
    options.includeDefaultSearchRoots = false;
    options.includeLdConfigSearchRoots = false;
    options.includeEnvironmentSearchRoots = false;
    return options;
}

void test_resolves_regular_elf_from_custom_root()
{
    auto temp_dir = TempDir{};
    auto regular = temp_dir.path() / "lib" / "libplain.so.1";
    write_fake_elf(regular);

    auto resolver = dynpax::Resolver{make_options(temp_dir.path())};
    resolver.populate();

    auto resolved = resolver.resolve("libplain.so.1");
    expect(resolved.has_value(), "expected regular ELF to resolve");
    expect(resolved->lookupName() == "libplain.so.1",
           "lookup name mismatch");
    expect(!resolved->hasAlias(),
           "regular file should not be reported as an alias");
    expect(resolved->aliasPath().lexically_normal() ==
               regular.lexically_normal(),
           "alias path should match created file");
    expect(resolved->canonicalPath().lexically_normal() ==
               regular.lexically_normal(),
           "canonical path should match created file");
}

void test_tracks_alias_and_canonical_paths_for_symlink()
{
    auto temp_dir = TempDir{};
    auto canonical = temp_dir.path() / "lib" / "libalias.so.1.2";
    auto alias = temp_dir.path() / "lib" / "libalias.so.1";
    auto sonameAlias = temp_dir.path() / "lib" / "libalias.so";
    write_fake_elf(canonical);

    std::error_code errc;
    fs::create_symlink(canonical.filename(), alias, errc);
    if (errc)
    {
        throw std::runtime_error{"failed to create symlink alias"};
    }
    fs::create_symlink(alias.filename(), sonameAlias, errc);
    if (errc)
    {
        throw std::runtime_error{"failed to create soname symlink alias"};
    }

    auto resolver = dynpax::Resolver{make_options(temp_dir.path())};
    resolver.populate();

    auto resolved = resolver.resolve("libalias.so.1");
    expect(resolved.has_value(), "expected alias ELF to resolve");
    expect(resolved->hasAlias(),
           "symlink resolution should retain alias metadata");
    expect(resolved->aliasName() == "libalias.so.1",
           "alias filename mismatch");
    expect(resolved->canonicalName() == "libalias.so.1.2",
           "canonical filename mismatch");
    expect(resolved->aliasPath().lexically_normal() ==
               alias.lexically_normal(),
           "alias path should preserve symlink location");
    expect(resolved->canonicalPath().lexically_normal() ==
               canonical.lexically_normal(),
           "canonical path should resolve to the payload file");

        auto aliasLinks = resolved->aliasLinks();
        expect(aliasLinks.size() == 2,
            "expected full alias chain to be preserved");
        expect(aliasLinks[0].linkPath().filename() == "libalias.so",
            "expected SONAME alias to be tracked");
        expect(aliasLinks[0].targetPath().filename() == "libalias.so.1",
            "expected SONAME alias to point to intermediate link");
        expect(aliasLinks[1].linkPath().filename() == "libalias.so.1",
            "expected linker name alias to be tracked");
        expect(aliasLinks[1].targetPath().filename() == "libalias.so.1.2",
            "expected linker name alias to point to payload");
}

void test_returns_nullopt_for_unknown_library()
{
    auto temp_dir = TempDir{};
    auto resolver = dynpax::Resolver{make_options(temp_dir.path())};
    resolver.populate();

    auto resolved = resolver.resolve("libmissing.so");
    expect(!resolved.has_value(),
           "missing libraries should not resolve");
}

} // namespace

auto main() -> int
{
    try
    {
        test_resolves_regular_elf_from_custom_root();
        test_tracks_alias_and_canonical_paths_for_symlink();
        test_returns_nullopt_for_unknown_library();
    }
    catch (const std::exception &except)
    {
        std::cerr << "Resolver test failure: " << except.what()
                  << '\n';
        return 1;
    }

    return 0;
}