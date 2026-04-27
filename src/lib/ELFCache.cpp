#include "ELFCache.hpp"
#include "Resolver.hpp"
#include <utility>

namespace dynpax
{

ELFCache::ELFCache() : ELFCache(ResolverOptions{}) {};

ELFCache::ELFCache(ResolverOptions options)
    : m_resolver{std::make_unique<Resolver>(std::move(options))}
{
}

ELFCache::ELFCache(ELFCache &&rhs) noexcept
{
    swap(rhs);
}

auto ELFCache::operator=(ELFCache &&rhs) noexcept -> ELFCache &
{
    swap(rhs);
    return *this;
}

ELFCache::~ELFCache() = default;

void ELFCache::swap(ELFCache &other) noexcept
{
    using std::swap;
    swap(m_resolver, other.m_resolver);
}

auto ELFCache::populate() -> void
{
    m_resolver->populate();
}

[[nodiscard]] auto ELFCache::getELFPath(const std::string &libName)
    const -> std::optional<std::filesystem::path>
{
    auto resolved = m_resolver->resolve(libName);
    if (!resolved.has_value())
    {
        return std::nullopt;
    }
    return resolved->canonicalPath();
}

void swap(ELFCache &lhs, ELFCache &rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace dynpax