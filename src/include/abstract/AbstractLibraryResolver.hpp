#pragma once

#include <expected>
#include <stdexcept>
#include <string>

namespace dynpax
{

template <typename T> struct AbstractLibraryResolver
{
    [[nodiscard]] static auto resolveLibrary(const std::string &name)
        -> std::expected<std::string, std::runtime_error>
    {
        return T::resolveLibrary(name);
    }
};

} // namespace dynpax