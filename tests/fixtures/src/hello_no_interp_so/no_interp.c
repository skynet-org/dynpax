enum
{
    kDynpaxNoInterpValue = 42,
};

__attribute__((visibility("default"), used))
// NOLINTNEXTLINE(misc-use-internal-linkage)
int dynpax_no_interp_value(void)
{
    return kDynpaxNoInterpValue;
}