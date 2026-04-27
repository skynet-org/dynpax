#include "BundleBuilder.hpp"
#include "BundleManifest.hpp"
#include "BundlePaths.hpp"
#include "BundleVerifier.hpp"
#include "Executable.hpp"
#include "Resolver.hpp"
#include <array>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <stdlib.h>

#ifndef DYNPAX_FIXTURE_HELLO_SONAME
#error DYNPAX_FIXTURE_HELLO_SONAME is not defined
#endif

#ifndef DYNPAX_FIXTURE_HELLO_ABS_RUNPATH
#error DYNPAX_FIXTURE_HELLO_ABS_RUNPATH is not defined
#endif

#ifndef DYNPAX_FIXTURE_LIB_DIR
#error DYNPAX_FIXTURE_LIB_DIR is not defined
#endif

namespace fs = std::filesystem;

namespace
{

class TempDir
{
  public:
    TempDir()
    {
        auto pathTemplate = std::to_array("/tmp/dynpax-bundle-XXXXXX");
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

struct BundleArtifacts
{
    dynpax::BundleManifest manifest;
    fs::path executableOutput;
    std::optional<fs::path> interpreterBundlePath;
};

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error{std::string{message}};
    }
}

auto fixture_lib_dir() -> fs::path
{
    return fs::path{DYNPAX_FIXTURE_LIB_DIR};
}

auto resolver_options() -> dynpax::ResolverOptions
{
    auto options = dynpax::ResolverOptions{};
    options.searchRoots = {fixture_lib_dir()};
    return options;
}

auto materialized_path(const fs::path &bundleRoot,
                       const fs::path &bundledPath) -> fs::path
{
    return dynpax::materialized_path(bundleRoot, bundledPath);
}

auto bundle_fixture(const fs::path &fixtureBinary,
                    const fs::path &bundleRoot) -> BundleArtifacts
{
    auto resolver =
        std::make_shared<dynpax::Resolver>(resolver_options());
    resolver->populate();

    auto builder = dynpax::BundleBuilder{resolver};
    auto built = builder.build(fixtureBinary,
                               dynpax::FakeRoot{bundleRoot}, true);
    if (!built.has_value())
    {
        expect(false, built.error());
    }

    auto artifacts = BundleArtifacts{};
    artifacts.manifest = built->manifest;
    artifacts.executableOutput = built->executableOutput.value_or(fs::path{});
    artifacts.interpreterBundlePath = built->interpreterBundlePath;
    return artifacts;
}

void verify_manifest_entry(
    const dynpax::BundleManifest &manifest,
    dynpax::BundleEntryKind kind, std::string_view filename,
    std::optional<std::string_view> linkTarget = std::nullopt)
{
    auto found = std::find_if(
        manifest.entries.begin(), manifest.entries.end(),
        [&](const auto &entry) {
            if (entry.kind != kind ||
                entry.bundledPath.filename() != filename)
            {
                return false;
            }
            if (!linkTarget.has_value())
            {
                return true;
            }
            return entry.linkTarget.has_value() &&
                   entry.linkTarget->filename() == *linkTarget;
        });
    expect(found != manifest.entries.end(),
           "expected manifest entry was not found");
}

void test_manifest_preserves_full_soname_chain()
{
    auto tempDir = TempDir{};
    auto artifacts =
        bundle_fixture(fs::path{DYNPAX_FIXTURE_HELLO_SONAME},
                       tempDir.path());

    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SharedObject,
                          "libdynpaxgreet.so.1.2.0");
    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SharedObject,
                          "libdynpaxmessage.so.1.0.0");
    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SymlinkAlias,
                          "libdynpaxgreet.so", "libdynpaxgreet.so.1");
    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SymlinkAlias,
                          "libdynpaxgreet.so.1",
                          "libdynpaxgreet.so.1.2.0");
    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SymlinkAlias,
                          "libdynpaxmessage.so", "libdynpaxmessage.so.1");
    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SymlinkAlias,
                          "libdynpaxmessage.so.1",
                          "libdynpaxmessage.so.1.0.0");

    auto resolver =
        std::make_shared<dynpax::Resolver>(resolver_options());
    auto verifier = dynpax::BundleVerifier{resolver};
    auto buildResult = dynpax::BundleBuildResult{
        .bundleRoot = tempDir.path(),
        .manifest = artifacts.manifest,
        .executableOutput = artifacts.executableOutput,
        .interpreterBundlePath = artifacts.interpreterBundlePath,
    };
    auto report = verifier.verify(buildResult);
    expect(report.ok(), report.summary());
}

void test_bundle_verifier_rewrites_runpath_and_interpreter()
{
    auto resolver =
        std::make_shared<dynpax::Resolver>(resolver_options());
    resolver->populate();

    auto source = dynpax::Executable{DYNPAX_FIXTURE_HELLO_ABS_RUNPATH,
                                     resolver};
    expect(static_cast<bool>(source), "failed to open absolute-runpath fixture");
    auto originalRunpath = source.runpath();
    expect(originalRunpath.has_value() && originalRunpath->has_value(),
           "source fixture should carry an absolute RUNPATH");
    auto libDir = fixture_lib_dir().string();
    expect(std::find(originalRunpath->value().begin(),
                     originalRunpath->value().end(),
                     libDir) != originalRunpath->value().end(),
           "source fixture RUNPATH should include fixture lib dir");

    auto tempDir = TempDir{};
    auto artifacts = bundle_fixture(
        fs::path{DYNPAX_FIXTURE_HELLO_ABS_RUNPATH}, tempDir.path());

    auto verifier = dynpax::BundleVerifier{resolver};
    auto buildResult = dynpax::BundleBuildResult{
        .bundleRoot = tempDir.path(),
        .manifest = artifacts.manifest,
        .executableOutput = artifacts.executableOutput,
        .interpreterBundlePath = artifacts.interpreterBundlePath,
    };
    auto report = verifier.verify(buildResult);
    expect(report.ok(), report.summary());
}

} // namespace

auto main() -> int
{
    try
    {
        test_manifest_preserves_full_soname_chain();
        test_bundle_verifier_rewrites_runpath_and_interpreter();
    }
    catch (const std::exception &except)
    {
        std::cerr << "Bundle verifier test failure: " << except.what()
                  << '\n';
        return 1;
    }

    return 0;
}