#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dynpax
{

namespace fs = std::filesystem;

enum class BundleEntryKind : std::uint8_t
{
    Unknown,
    Executable,
    SharedObject,
    Interpreter,
    SymlinkAlias,
};

struct RewritePlan
{
    bool needsRpathRewrite{true};
    bool needsRunpathRewrite{true};
    bool needsInterpreterRewrite{false};
};

struct BundleEntry
{
    fs::path sourcePath;
    fs::path bundledPath;
    std::optional<fs::path> linkTarget;
    BundleEntryKind kind{BundleEntryKind::Unknown};
    std::optional<std::string> soname;
    std::string requestedName;
    bool hasInterpreter{false};
    RewritePlan rewritePlan{};
};

struct BundleManifest
{
    fs::path primaryInput;
    std::vector<BundleEntry> entries;
};

} // namespace dynpax