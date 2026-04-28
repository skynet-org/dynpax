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
    dynpax::BundleLayoutPolicy layoutPolicy;
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

auto host_resolver_options() -> dynpax::ResolverOptions
{
    auto options = dynpax::ResolverOptions{};
    options.includeEnvironmentSearchRoots = false;
    return options;
}

auto materialized_path(const fs::path &bundleRoot,
                       const fs::path &bundledPath) -> fs::path
{
    return dynpax::materialized_path(bundleRoot, bundledPath);
}

auto manifest_snapshot(const dynpax::BundleManifest &manifest)
    -> std::vector<std::string>
{
    auto snapshot = std::vector<std::string>{};
    for (const auto &entry : manifest.entries)
    {
        snapshot.push_back(
            std::to_string(static_cast<int>(entry.kind)) + "|" +
            entry.sourcePath.string() + "|" + entry.bundledPath.string() +
            "|" +
            (entry.linkTarget.has_value() ? entry.linkTarget->string() : ""));
    }
    return snapshot;
}

auto find_manifest_entry(const dynpax::BundleManifest &manifest,
                         dynpax::BundleEntryKind kind,
                         std::string_view filename)
    -> const dynpax::BundleEntry *
{
    auto found = std::find_if(
        manifest.entries.begin(), manifest.entries.end(),
        [&](const auto &entry) {
            return entry.kind == kind &&
                   entry.bundledPath.filename() == filename;
        });
    if (found == manifest.entries.end())
    {
        return nullptr;
    }
    return &(*found);
}

auto bundle_fixture(
    const fs::path &fixtureBinary, const fs::path &bundleRoot,
    const dynpax::ResolverOptions &options = resolver_options(),
    dynpax::BundleLayoutPolicy layoutPolicy =
        dynpax::BundleLayoutPolicy::FlatLib64)
    -> BundleArtifacts
{
    auto resolver = std::make_shared<dynpax::Resolver>(options);
    resolver->populate();

    auto builder = dynpax::BundleBuilder{resolver};
    auto built = builder.build(
        fixtureBinary, dynpax::FakeRoot{bundleRoot, layoutPolicy}, true);
    if (!built.has_value())
    {
        expect(false, built.error());
    }

    auto artifacts = BundleArtifacts{};
    artifacts.layoutPolicy = built->layoutPolicy;
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
        .layoutPolicy = artifacts.layoutPolicy,
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
        .layoutPolicy = artifacts.layoutPolicy,
        .manifest = artifacts.manifest,
        .executableOutput = artifacts.executableOutput,
        .interpreterBundlePath = artifacts.interpreterBundlePath,
    };
    auto report = verifier.verify(buildResult);
    expect(report.ok(), report.summary());
}

void test_bundle_resolves_from_embedded_runpath_without_extra_roots()
{
    auto tempDir = TempDir{};
    auto artifacts = bundle_fixture(
        fs::path{DYNPAX_FIXTURE_HELLO_ABS_RUNPATH}, tempDir.path(),
        host_resolver_options());

    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SharedObject,
                          "libdynpaxgreet.so.1.2.0");
    verify_manifest_entry(artifacts.manifest,
                          dynpax::BundleEntryKind::SharedObject,
                          "libdynpaxmessage.so.1.0.0");

    auto resolver =
        std::make_shared<dynpax::Resolver>(host_resolver_options());
    resolver->populate();
    auto verifier = dynpax::BundleVerifier{resolver};
    auto buildResult = dynpax::BundleBuildResult{
        .bundleRoot = tempDir.path(),
        .layoutPolicy = artifacts.layoutPolicy,
        .manifest = artifacts.manifest,
        .executableOutput = artifacts.executableOutput,
        .interpreterBundlePath = artifacts.interpreterBundlePath,
    };
    auto report = verifier.verify(buildResult);
    expect(report.ok(), report.summary());
}

void test_preserve_source_tree_keeps_runtime_roots_and_flattens_build_libs()
{
    auto resolver =
        std::make_shared<dynpax::Resolver>(resolver_options());
    resolver->populate();

    auto source = dynpax::Executable{DYNPAX_FIXTURE_HELLO_ABS_RUNPATH,
                                     resolver};
    expect(static_cast<bool>(source), "failed to open fixture executable");
    auto interpreter = source.interpreter();
    expect(interpreter.has_value() && interpreter->has_value(),
           "fixture should expose an interpreter");

    auto tempDir = TempDir{};
    auto artifacts = bundle_fixture(
        fs::path{DYNPAX_FIXTURE_HELLO_ABS_RUNPATH}, tempDir.path(),
        resolver_options(),
        dynpax::BundleLayoutPolicy::PreserveSourceTree);

    expect(artifacts.interpreterBundlePath.has_value(),
           "preserve-source-tree should materialize an interpreter");
        expect(*artifacts.interpreterBundlePath == interpreter->value(),
           "preserve-source-tree should preserve the interpreter path");

    auto buildLibEntry = find_manifest_entry(
        artifacts.manifest, dynpax::BundleEntryKind::SharedObject,
        "libdynpaxgreet.so.1.2.0");
    expect(buildLibEntry != nullptr,
           "expected build-tree shared object entry was not found");

    auto preservedSystemLibrary = std::find_if(
        artifacts.manifest.entries.begin(), artifacts.manifest.entries.end(),
        [&](const auto &entry) {
            return entry.kind == dynpax::BundleEntryKind::SharedObject &&
                   entry.sourcePath.is_absolute() &&
                   (entry.sourcePath.string().starts_with("/lib") ||
                    entry.sourcePath.string().starts_with("/usr/lib"));
        });
    expect(preservedSystemLibrary != artifacts.manifest.entries.end(),
           "expected at least one preserved system shared object");
    expect(preservedSystemLibrary->bundledPath ==
               preservedSystemLibrary->sourcePath.lexically_normal(),
           "preserve-source-tree should keep system shared objects in place");
    expect(buildLibEntry->bundledPath.parent_path() ==
               preservedSystemLibrary->bundledPath.parent_path(),
           "build-tree shared objects should follow the source runtime library scheme");

    auto executable =
        dynpax::Executable{artifacts.executableOutput.string(), resolver};
    auto runpath = executable.runpath();
    expect(runpath.has_value() && runpath->has_value(),
           "bundled executable should carry RUNPATH");
    auto executableEntry = find_manifest_entry(
        artifacts.manifest, dynpax::BundleEntryKind::Executable,
        fs::path{DYNPAX_FIXTURE_HELLO_ABS_RUNPATH}.filename().string());
    expect(executableEntry != nullptr,
           "expected executable entry was not found");
    auto expectedRunpath = dynpax::bundle_runpath(
        artifacts.manifest, *executableEntry,
        dynpax::BundleLayoutPolicy::PreserveSourceTree);
    expect(runpath->value() == expectedRunpath,
           "preserve-source-tree should compute deterministic RUNPATH values");

    auto verifier = dynpax::BundleVerifier{resolver};
    auto buildResult = dynpax::BundleBuildResult{
        .bundleRoot = tempDir.path(),
        .layoutPolicy = artifacts.layoutPolicy,
        .manifest = artifacts.manifest,
        .executableOutput = artifacts.executableOutput,
        .interpreterBundlePath = artifacts.interpreterBundlePath,
    };
    auto report = verifier.verify(buildResult);
    expect(report.ok(), report.summary());
}

void test_bundle_manifests_are_deterministic_for_each_layout()
{
    auto flatFirstDir = TempDir{};
    auto flatSecondDir = TempDir{};
    auto flatFirst = bundle_fixture(fs::path{DYNPAX_FIXTURE_HELLO_SONAME},
                                    flatFirstDir.path());
    auto flatSecond = bundle_fixture(fs::path{DYNPAX_FIXTURE_HELLO_SONAME},
                                     flatSecondDir.path());
    expect(manifest_snapshot(flatFirst.manifest) ==
               manifest_snapshot(flatSecond.manifest),
           "flat-lib64 manifest generation should be deterministic");

    auto preserveFirstDir = TempDir{};
    auto preserveSecondDir = TempDir{};
    auto preserveFirst = bundle_fixture(
        fs::path{DYNPAX_FIXTURE_HELLO_SONAME}, preserveFirstDir.path(),
        resolver_options(),
        dynpax::BundleLayoutPolicy::PreserveSourceTree);
    auto preserveSecond = bundle_fixture(
        fs::path{DYNPAX_FIXTURE_HELLO_SONAME}, preserveSecondDir.path(),
        resolver_options(),
        dynpax::BundleLayoutPolicy::PreserveSourceTree);
    expect(manifest_snapshot(preserveFirst.manifest) ==
               manifest_snapshot(preserveSecond.manifest),
           "preserve-source-tree manifest generation should be deterministic");
}

} // namespace

auto main() -> int
{
    try
    {
        test_manifest_preserves_full_soname_chain();
        test_bundle_verifier_rewrites_runpath_and_interpreter();
        test_bundle_resolves_from_embedded_runpath_without_extra_roots();
        test_preserve_source_tree_keeps_runtime_roots_and_flattens_build_libs();
        test_bundle_manifests_are_deterministic_for_each_layout();
    }
    catch (const std::exception &except)
    {
        std::cerr << "Bundle verifier test failure: " << except.what()
                  << '\n';
        return 1;
    }

    return 0;
}