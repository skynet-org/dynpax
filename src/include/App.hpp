#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace dynpax
{
namespace fs = std::filesystem;

struct App
{

    struct Params
    {
        std::vector<fs::path> targets;
        fs::path fakeRoot;
        bool includeInterpreter;
    };
    using Result = std::expected<Params, int>;

    explicit App(std::string_view name);

    App(const App &) = delete;

    auto operator=(const App &) -> App & = delete;

    App(App &&) = delete;

    auto operator=(App &&) -> App & = delete;

    ~App();
    auto parse(int argc, char *argv[]) noexcept -> Result; // NOLINT

  private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};
} // namespace dynpax
