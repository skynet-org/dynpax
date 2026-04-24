#pragma once

#ifdef __linux__

#include "linux/LinuxLibraryResolver.hpp"
namespace dynpax
{
using LibraryResolver = LinuxLibraryResolver;
}
#else
#error "LibraryResolver not implemented for this platform"
#endif
