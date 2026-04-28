#include "App.hpp"
#include "BundleBuilder.hpp"
#include "BundlePaths.hpp"
#include "BundleVerifier.hpp"
#include "Executable.hpp"
#include "FakeRoot.hpp"
#include "Resolver.hpp"
#include <expected>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <memory>

namespace
{

auto kind_name(dynpax::BundleEntryKind kind) -> const char *
{
    switch (kind)
    {
    case dynpax::BundleEntryKind::Executable:
        return "executable";
    case dynpax::BundleEntryKind::SharedObject:
        return "shared-object";
    case dynpax::BundleEntryKind::Interpreter:
        return "interpreter";
    case dynpax::BundleEntryKind::SymlinkAlias:
        return "symlink-alias";
    case dynpax::BundleEntryKind::Unknown:
        break;
    }

    return "unknown";
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    try
    {
        dynpax::App app{"dynpax"};
        auto parseResult = app.parse(argc, argv);
        if (!parseResult.has_value())
        {
            return parseResult.error();
        }
        dynpax::FakeRoot rootManager{
            std::move(parseResult->fakeRoot),
            parseResult->layoutPolicy};

        auto resolver = std::make_shared<dynpax::Resolver>();
        resolver->populate();
        auto builder = dynpax::BundleBuilder{resolver};
        auto verifier = dynpax::BundleVerifier{resolver};
        for (const auto &target : parseResult->targets)
        {
            fmt::println("Target: {}", target.string());
            fmt::println("Layout policy: {}",
                         dynpax::bundle_layout_policy_name(
                             parseResult->layoutPolicy));
            auto buildResult =
                builder.build(target, rootManager,
                              parseResult->includeInterpreter);
            if (!buildResult.has_value())
            {
                fmt::println("Error: {}", buildResult.error());
                return 1;
            }
            fmt::println("Copy dynamic dependencies to {}",
                         rootManager.path().string());

            for (const auto &entry : buildResult->manifest.entries)
            {
                auto dest = dynpax::materialized_path(
                    buildResult->bundleRoot, entry.bundledPath);

                switch (entry.kind)
                {
                case dynpax::BundleEntryKind::Executable:
                    fmt::println("Materialized executable {} => {}",
                                 entry.sourcePath.string(),
                                 dest.string());
                    break;
                case dynpax::BundleEntryKind::Interpreter:
                    fmt::println("Materialized interpreter {} => {}",
                                 entry.sourcePath.string(),
                                 dest.string());
                    break;
                case dynpax::BundleEntryKind::SharedObject:
                    fmt::println("Materialized library {} => {}",
                                 entry.sourcePath.string(),
                                 dest.string());
                    break;
                case dynpax::BundleEntryKind::SymlinkAlias:
                    fmt::println("Materialized alias {} => {}",
                                 entry.bundledPath.string(),
                                 dest.string());
                    break;
                case dynpax::BundleEntryKind::Unknown:
                    fmt::println("Skipped unknown manifest entry for {}",
                                 entry.sourcePath.string());
                    break;
                }
            }

            auto report = verifier.verify(*buildResult);
            if (!report.ok())
            {
                for (const auto &issue : report.issues)
                {
                    if (!issue.path.empty())
                    {
                        fmt::println("Verification error {}: {}",
                                     issue.path.string(),
                                     issue.message);
                    }
                    else
                    {
                        fmt::println("Verification error: {}",
                                     issue.message);
                    }
                }
                return 1;
            }
        }
    }
    catch (const std::exception &except)
    {
        fmt::println("Error: {}", except.what());
        return 1;
    }

    return 0;
}