#include "BundleVerifier.hpp"
#include "BundleBuilder.hpp"
#include "BundlePaths.hpp"
#include "Executable.hpp"
#include <filesystem>
#include <sstream>
#include <string>

namespace dynpax
{

BundleVerifier::BundleVerifier(std::shared_ptr<Resolver> resolver)
    : resolver_{std::move(resolver)}
{
}

auto BundleVerificationReport::ok() const -> bool
{
    return issues.empty();
}

auto BundleVerificationReport::summary() const -> std::string
{
    auto stream = std::ostringstream{};
    for (const auto &issue : issues)
    {
        if (!issue.path.empty())
        {
            stream << issue.path.string() << ": ";
        }
        stream << issue.message << '\n';
    }
    return stream.str();
}

auto BundleVerifier::verify(
    const BundleBuildResult &result,
    const BundleVerificationOptions &options) const
    -> BundleVerificationReport
{
    auto report = BundleVerificationReport{};
    if (!std::filesystem::exists(result.bundleRoot / "bin"))
    {
        report.issues.push_back(
            {result.bundleRoot / "bin", "bundle missing bin directory"});
    }
    if (!std::filesystem::exists(result.bundleRoot / "lib64"))
    {
        report.issues.push_back({result.bundleRoot / "lib64",
                                 "bundle missing lib64 directory"});
    }
    if (!std::filesystem::exists(result.bundleRoot / "lib") ||
        !std::filesystem::is_symlink(result.bundleRoot / "lib"))
    {
        report.issues.push_back(
            {result.bundleRoot / "lib", "bundle missing lib symlink"});
    }
    if (!std::filesystem::exists(result.bundleRoot / "usr/lib") ||
        !std::filesystem::is_symlink(result.bundleRoot / "usr/lib"))
    {
        report.issues.push_back({result.bundleRoot / "usr/lib",
                                 "bundle missing usr/lib symlink"});
    }
    if (!std::filesystem::exists(result.bundleRoot / "usr/lib64") ||
        !std::filesystem::is_symlink(result.bundleRoot / "usr/lib64"))
    {
        report.issues.push_back({result.bundleRoot / "usr/lib64",
                                 "bundle missing usr/lib64 symlink"});
    }

    for (const auto &entry : result.manifest.entries)
    {
        verifyEntry(result, entry, options, report);
    }

    return report;
}

auto BundleVerifier::verifyEntry(
    const BundleBuildResult &result, const BundleEntry &entry,
    const BundleVerificationOptions &options,
    BundleVerificationReport &report) const -> void
{
    auto outputPath = materialized_path(result.bundleRoot, entry.bundledPath);
    if (!std::filesystem::exists(outputPath))
    {
        report.issues.push_back(
            {outputPath, "manifest output path missing from bundle"});
        return;
    }

    if (entry.kind == BundleEntryKind::SymlinkAlias)
    {
        if (!std::filesystem::is_symlink(outputPath))
        {
            report.issues.push_back(
                {outputPath, "alias entry should materialize as a symlink"});
            return;
        }
        if (!entry.linkTarget.has_value())
        {
            report.issues.push_back(
                {outputPath, "alias entry should declare a link target"});
            return;
        }

        auto expectedTarget = materialized_symlink_target(
            entry.bundledPath, *entry.linkTarget);
        if (std::filesystem::read_symlink(outputPath) != expectedTarget)
        {
            report.issues.push_back(
                {outputPath, "materialized alias target mismatch"});
        }
        return;
    }

    auto bundledElf = Executable{outputPath.string(), resolver_};
    if (!bundledElf)
    {
        report.issues.push_back({outputPath, "failed to open bundled ELF"});
        return;
    }

    if (entry.kind == BundleEntryKind::Executable)
    {
        auto interpreter = bundledElf.interpreter();
        if (!interpreter.has_value())
        {
            report.issues.push_back(
                {outputPath, "failed to read interpreter section"});
            return;
        }
        if (interpreter->has_value() != entry.hasInterpreter)
        {
            report.issues.push_back(
                {outputPath, "bundled interpreter presence mismatch"});
        }
        if (entry.hasInterpreter &&
            result.interpreterBundlePath.has_value() &&
            interpreter->has_value() &&
            interpreter->value() != *result.interpreterBundlePath)
        {
            report.issues.push_back(
                {outputPath, "bundled interpreter path mismatch"});
        }
    }

    if (entry.kind != BundleEntryKind::Executable &&
        entry.kind != BundleEntryKind::SharedObject)
    {
        return;
    }

    auto runpath = bundledElf.runpath();
    if (!runpath.has_value() || !runpath->has_value())
    {
        report.issues.push_back({outputPath,
                                 "bundled ELF is missing RUNPATH"});
        return;
    }

    auto rpath = bundledElf.rpath();
    if (!rpath.has_value())
    {
        report.issues.push_back({outputPath,
                                 "failed to read RPATH section"});
        return;
    }
    if (rpath->has_value())
    {
        report.issues.push_back(
            {outputPath, "bundled ELF should not retain RPATH"});
    }

    if (runpath->value().size() != 1 ||
        runpath->value().front() != options.expectedRunpath)
    {
        report.issues.push_back(
            {outputPath, "bundled ELF RUNPATH mismatch"});
    }
}

} // namespace dynpax