#include "App.hpp"
#include "Executable.hpp"
#include "FileManager.hpp"
#include "LibraryResolver.hpp"
#include <expected>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <stdexcept>

namespace fs = std::filesystem;

auto main(int argc, char *argv[]) -> int
{
    dynpax::App app{"dynpax"};
    auto parseResult = app.parse(argc, argv);
    if (!parseResult.has_value())
    {
        return parseResult.error();
    }
    dynpax::FileManager fileManager{std::move(parseResult->fakeRoot)};
    dynpax::Executable binary{parseResult->target.string()};
    if (!binary)
    {
        fmt::println("Error: unable to open {}\n",
                     parseResult->target.string());
        return 1;
    }
    fmt::println("Copy dynamic dependencies to {}",
                 fileManager.fakeRoot().string());
    for (const auto &library : binary.neededLibraries())
    {
        auto dst = fileManager.joinFakeRoot({library});
        fmt::println("Copying {} => {}", library, dst.string());
        if (!dynpax::FileManager::copyFile(library, dst))
        {
            fmt::println("Error: failed to copy: {} => {}", library,
                         dst.string());
        }
    }
    if (parseResult->includeInterpreter)
    {
        // Example of very easy to read and follow functional style
        // programming using C++23, enjoy! Ha-ha:)
        auto interpreter = binary.interpreter();
        if (!interpreter)
        {
            fmt::println(
                "Error: failed to read interpreter section: {}",
                interpreter.error().what());
            return 1;
        }
        if (!interpreter.value())
        {
            fmt::println(
                "Error: binary does not contain interpreter section");
            return 1;
        }

        const auto src = interpreter.value().value();
        const auto dst = fileManager.joinFakeRoot({src});
        fmt::println("Copy {} => {}", src.string(), dst.string());
        if (!dynpax::FileManager::copyFile(src, dst))
        {
            fmt::println(
                "Error: failed to copy interpreter: {} => {}",
                src.string(), dst.string());
            return 1;
        }
    }

    const auto binDst = fileManager.joinFakeRoot(
        {"bin", binary.filePath().filename()});
    fmt::println("Copy binary {} => {} {}",
                 binary.filePath().string(), binDst.string(),
                 binary.filePath().filename().string());
    if (!dynpax::FileManager::copyFile(binary.filePath(), binDst))
    {
        fmt::println("Error: failed to copy binary to new root");
        return 1;
    }

    return 0;
}