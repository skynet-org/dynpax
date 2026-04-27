#include "App.hpp"
#include "ELFCache.hpp"
#include "Executable.hpp"
#include "FakeRoot.hpp"
#include <expected>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <memory>

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
            std::move(parseResult->fakeRoot)};

        std::error_code errc{};
        auto elfCache = std::make_shared<dynpax::ELFCache>();
        elfCache->populate();
        for (const auto &target : parseResult->targets)
        {
            fmt::println("Target: {}", target.string());
            dynpax::Executable binary{target.string(), elfCache};
            if (!binary)
            {
                fmt::println("Error: unable to open binary {}\n",
                             target.string());
                return 1;
            }
            fmt::println("Copy dynamic dependencies to {}",
                         rootManager.path().string());
            for (const auto &library : binary.neededLibraries())
            {
                auto dest = rootManager.addLibrary(library, errc);
                if (errc)
                {
                    fmt::println("Error: failed to copy: {}",
                                 library);
                    return 1;
                }
                fmt::println("Copied {} to {}", library,
                             dest.string());
            }

            if (parseResult->includeInterpreter)
            {
                auto interpreter = binary.interpreter();
                if (!interpreter)
                {
                    fmt::println("Error: failed to read interpreter "
                                 "section: {}",
                                 interpreter.error().what());
                    return 1;
                }
                if (!interpreter.value())
                {
                    fmt::println("Error: binary does not contain "
                                 "interpreter section");
                    return 1;
                }

                const auto src = interpreter.value().value();
                auto dest = rootManager.addLibrary(src, errc);
                if (errc)
                {
                    fmt::println(
                        "Error: failed to copy interpreter: {}",
                        src.string());
                    return 1;
                }
                fmt::println("Copied {} => {}", src.string(),
                             dest.string());

                fmt::println("Set interpreter to {}", rootManager.stripRoot(dest).string());
                binary.interpreter(rootManager.stripRoot(dest).string());
            }
            binary.rpath({""});
            binary.runpath({"$ORIGIN/../lib64"});
            auto dest = rootManager.binaryStub(target, errc);
            if (errc)
            {
                fmt::println("Error: failed to copy binary stub: {}",
                             target.string());
                return 1;
            }
            fmt::println("Copy binary {}",
                         binary.filePath().string());
            binary.write(dest);
        }
    }
    catch (const std::exception &except)
    {
        fmt::println("Error: {}", except.what());
        return 1;
    }

    return 0;
}