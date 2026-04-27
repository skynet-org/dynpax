#include "App.hpp"
#include "BundleLayout.hpp"
#include <CLI/CLI.hpp>
#include <exception>
#include <expected>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dynpax
{
using namespace std::string_literals;

struct App::Impl
{
    std::string layoutPolicyName{std::string{
        bundle_layout_policy_name(BundleLayoutPolicy::FlatLib64)}};

    explicit Impl(std::string_view name) : app{std::string{name}}
    {
    }

    void setup()
    {
        app.add_option(
               "-t,--target"s, params.targets,
               "Target ELF executables list (coma-separated)"s)
            ->required();
        app.add_option("-f,--fake-root"s, params.fakeRoot,
                       "Fake root to be used as RUNPATH"s);
        app.add_option(
            "--layout-policy"s, layoutPolicyName,
            "Bundle layout policy: flat-lib64 or preserve-source-tree"s);
        app.add_flag("-i,--interpreter"s, params.includeInterpreter,
                     "Add linker/interpreter to bundle"s);
    }

    auto parse(int argc, char *argv[]) noexcept -> Result // NOLINT
    {
        try
        {
            setup();
            app.parse(argc, argv);
            auto layoutPolicy =
                parse_bundle_layout_policy(layoutPolicyName);
            if (!layoutPolicy.has_value())
            {
                throw CLI::ValidationError(
                    "--layout-policy",
                    "expected flat-lib64 or preserve-source-tree");
            }
            params.layoutPolicy = *layoutPolicy;
            for (auto &target : params.targets)
            {
                target = fs::absolute(target.lexically_normal());
            }
            if (!params.fakeRoot.empty())
            {
                params.fakeRoot =
                    fs::absolute(params.fakeRoot.lexically_normal());
            }
            else
            {
                params.fakeRoot = fs::current_path();
            }
            return params;
        }
        catch (const CLI::ParseError &e)
        {
            return std::unexpected(app.exit(e));
        }
    }

  private:
    CLI::App app;
    Params params;
};

App::App(std::string_view name)
    : pimpl{std::make_unique<App::Impl>(name)}
{
}

App::~App() = default;

// NOLINTNEXTLINE
auto App::parse(int argc, char *argv[]) noexcept -> App::Result
{
    return pimpl->parse(argc, argv);
}

} // namespace dynpax