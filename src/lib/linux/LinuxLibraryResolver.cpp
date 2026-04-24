#include "linux/LinuxLibraryResolver.hpp"
#include <cstdlib>
#include <dlfcn.h>
#include <expected>
#include <fmt/format.h>
#include <link.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace dynpax
{

namespace
{
void handleCloser(void *handle)
{
    dlclose(handle);
}

} // namespace

auto LinuxLibraryResolver::resolveLibrary(const std::string &name)
    -> std::expected<std::string, std::runtime_error>
{
    static std::mutex dlMutex{};
    std::lock_guard lock{dlMutex};
    std::unique_ptr<void, decltype(&handleCloser)> handle(
        dlopen(name.c_str(), RTLD_LAZY), &handleCloser);

    struct link_map *link_map_ptr{nullptr};
    if (dlinfo(handle.get(), RTLD_DI_LINKMAP,
               static_cast<void *>(&link_map_ptr)) != 0)
    {
        return std::unexpected(std::runtime_error{fmt::format(
            "Failed to get library path: {}", dlerror())}); // NOLINT
    }
    auto copy = std::string{link_map_ptr->l_name};
    return copy;
}
} // namespace dynpax