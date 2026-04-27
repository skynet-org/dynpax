#include "BundleBuilder.hpp"
#include "BundleLayout.hpp"
#include "BundlePaths.hpp"
#include "Executable.hpp"
#include "FakeRoot.hpp"
#include <expected>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <string>

namespace dynpax
{

BundleBuilder::BundleBuilder(std::shared_ptr<Resolver> resolver)
    : resolver_{std::move(resolver)}
{
}

auto BundleBuilder::build(const fs::path &target,
                          const FakeRoot &fakeRoot,
                          bool includeInterpreter) const
    -> std::expected<BundleBuildResult, std::string>
{
    auto executable = Executable{target.string(), resolver_};
    if (!executable)
    {
        return std::unexpected{fmt::format(
            "unable to open binary {}", target.string())};
    }

    auto result = BundleBuildResult{};
    result.bundleRoot = fakeRoot.path();
    result.layoutPolicy = fakeRoot.layoutPolicy();
    result.manifest = executable.dependencyManifest(
        includeInterpreter, result.layoutPolicy);

    for (const auto &entry : result.manifest.entries)
    {
        std::error_code errc;
        auto destination = fakeRoot.materialize(entry, errc);
        if (errc)
        {
            return std::unexpected{fmt::format(
                "failed to materialize {} => {}: {}",
                entry.sourcePath.string(), entry.bundledPath.string(),
                errc.message())};
        }

        if (entry.kind == BundleEntryKind::Executable)
        {
            result.executableOutput = destination;
        }
        if (entry.kind == BundleEntryKind::Interpreter)
        {
            result.interpreterBundlePath = entry.bundledPath;
        }
    }

    if (!result.executableOutput.has_value())
    {
        return std::unexpected{fmt::format(
            "manifest did not produce executable output for {}",
            target.string())};
    }

    if (auto rewriteError = rewritePayloads(result, executable);
        rewriteError.has_value())
    {
        return std::unexpected{std::move(*rewriteError)};
    }

    return result;
}

auto BundleBuilder::rewritePayloads(
    const BundleBuildResult &result,
    Executable &sourceExecutable,
    const BundleRewriteOptions &options) const
    -> std::optional<std::string>
{
    for (const auto &entry : result.manifest.entries)
    {
        if (entry.kind != BundleEntryKind::Executable &&
            entry.kind != BundleEntryKind::SharedObject)
        {
            continue;
        }

        auto outputPath =
            materialized_path(result.bundleRoot, entry.bundledPath);

        if (entry.kind == BundleEntryKind::Executable &&
            result.interpreterBundlePath.has_value() &&
            entry.hasInterpreter)
        {
            sourceExecutable.interpreter(*result.interpreterBundlePath);
        }

        if (entry.kind == BundleEntryKind::Executable)
        {
            auto runpath =
                bundle_runpath(result.manifest, entry, result.layoutPolicy);
            sourceExecutable.rpath(options.rpath);
            sourceExecutable.runpath(runpath);
            if (!sourceExecutable.write(outputPath))
            {
                return fmt::format(
                    "failed to rewrite bundled ELF {}",
                    outputPath.string());
            }
            continue;
        }

        auto outputElf = Executable{outputPath.string(), resolver_};
        if (!outputElf)
        {
            return fmt::format("failed to reopen bundled ELF {}",
                               outputPath.string());
        }

        auto runpath =
            bundle_runpath(result.manifest, entry, result.layoutPolicy);
        outputElf.rpath(options.rpath);
        outputElf.runpath(runpath);
        if (!outputElf.write(outputPath))
        {
            return fmt::format("failed to rewrite bundled ELF {}",
                               outputPath.string());
        }
    }

    return std::nullopt;
}

} // namespace dynpax