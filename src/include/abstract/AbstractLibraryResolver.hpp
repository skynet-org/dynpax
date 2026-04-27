#pragma once

#include <expected>
#include <stdexcept>
#include <string>

namespace dynpax
{

template <typename T> struct AbstractLibraryResolver
{
    [[nodiscard]] auto resolveLibrary(const std::string &name) const
        -> std::expected<std::string, std::runtime_error>
    {
        return T::resolveLibrary(name);
    }
};

} // namespace dynpax