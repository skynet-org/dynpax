#pragma once

#include "abstract/AbstractLibraryResolver.hpp"
#include <expected>
#include <stdexcept>
#include <string>

namespace dynpax
{
struct LinuxLibraryResolver : public AbstractLibraryResolver<LinuxLibraryResolver>
{
    [[nodiscard]] static auto resolveLibrary(const std::string &name)
        -> std::expected<std::string, std::runtime_error>;
};
} // namespace dynpax