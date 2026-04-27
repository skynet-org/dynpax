# Dynpax Agent Guide

## Project Summary

Dynpax is a C++26 tool for turning a dynamically linked ELF executable into a
relocatable fake-root bundle that can run under plain `chroot` without relying
on the target host's original loader or shared-library layout.

The current implementation supports two bundle layout policies:

- `flat-lib64`: place bundled runtime payloads under a flattened `lib64`
  scheme and expose compatibility symlinks such as `lib`, `usr/lib`, and
  `usr/lib64`.
- `preserve-source-tree`: preserve source runtime paths when possible and place
  non-preservable application libraries under the source runtime's own libc
  directory scheme.

The project is built around LIEF for ELF parsing and rewriting, CLI11 for the
CLI, fmt for formatting support, CMake for orchestration, CTest for normal
validation, and an opt-in Docker matrix for runtime `chroot` verification.

## Repo Custom Agents

The repo also exposes three task-focused custom agents under `.github/agents/`:

- `dynpax-layout-specialist`: use for FlatLib64 or PreserveSourceTree layout
  work, bundle path planning, `RUNPATH` expectations, compatibility symlinks,
  and fake-root materialization.
- `dynpax-runtime-debugger`: use for `chroot` failures, missing-library
  debugging, alias-chain breakage, Docker-matrix regressions, and real-binary
  runtime reproductions such as `/usr/bin/openssl`.
- `dynpax-release-validator`: use for GitHub Actions, semantic-release,
  release-tag flow, GHCR publishing, manifest tagging, and token-trigger
  correctness.

## Default Agent Handoff Rules

If the current task clearly matches one of the repo specialists, the default
agent should delegate instead of solving it generically.

- Handoff to `dynpax-layout-specialist` for layout-policy changes, bundled path
  planning, `RUNPATH` expectations, FakeRoot filesystem shape, compatibility
  symlinks, or verifier updates driven by layout semantics.
- Handoff to `dynpax-runtime-debugger` for `chroot` failures, missing-library
  runtime errors, Docker-matrix regressions, broken alias chains, real-binary
  reproductions such as `/usr/bin/openssl`, or loader-search debugging.
- Handoff to `dynpax-release-validator` for GitHub Actions CI, release-tag
  automation, semantic-release, GHCR publishing, multi-arch image tagging, or
  token/permission questions in release workflows.

The default agent may keep the task when the work is obviously trivial and does
not benefit from a specialist role, but it should still follow the same local
anchor, narrow-validation, and determinism rules described below.

## What Matters Most

Dynpax is not just a file copier. It has to produce a deterministic runtime
filesystem that remains executable across different distro environments.

That means changes usually need to preserve all of the following:

- correct `DT_NEEDED` resolution, including transitive dependencies
- preserved SONAME alias chains as symlinks
- correct interpreter handling
- correct bundled path planning for the selected layout policy
- correct `RUNPATH` rewriting for bundled ELFs
- deterministic manifest ordering and materialization
- runtime compatibility under direct `chroot /bundle /bin/...`

If a change breaks any of those, it is probably incomplete even if the code
still compiles.

## Current Architecture

### Entry And Orchestration

- `src/bin/dynpax.cpp`: process entrypoint, CLI wiring, logging of build result
- `src/lib/App.cpp`: parses CLI arguments, including `--layout-policy`
- `src/lib/BundleBuilder.cpp`: high-level bundling orchestration

### Core Runtime Planning

- `src/lib/Executable.cpp`: inspects ELF metadata, resolves interpreter and
  dependency manifests, plans alias entries, and drives rewrite inputs
- `src/lib/Resolver.cpp`: finds shared libraries from configured and embedded
  search roots; ordering must stay deterministic
- `src/lib/BundleLayout.cpp`: central policy-aware mapping from source paths to
  bundled paths and expected `RUNPATH` values
- `src/lib/FakeRoot.cpp`: materializes the fake-root filesystem layout

### Verification

- `src/lib/BundleVerifier.cpp`: validates bundle structure and ELF rewrite
  expectations against the produced manifest
- `tests/unit/ResolverTests.cpp`: focused resolver behavior tests
- `tests/integration/BundleVerifierTests.cpp`: realistic fixture-driven bundle
  planning, rewriting, and verifier coverage
- `tests/docker/run-matrix.sh`: opt-in end-to-end `chroot` runtime validation

### Test Fixtures

- `tests/fixtures/src/hello_glibc`: basic glibc executable
- `tests/fixtures/src/hello_transitive_glibc`: transitive dependency fixture
- `tests/fixtures/src/hello_soname_glibc`: SONAME alias-chain fixture
- `tests/fixtures/src/hello_abs_runpath_glibc`: absolute-RUNPATH cleanup fixture
- `tests/fixtures/src/hello_musl`: musl-native fixture when `musl-gcc` exists
- `tests/fixtures/src/hello_no_interp_so`: shared object without `PT_INTERP`

## Verified Behavioral Invariants

These invariants are already learned the hard way. Agents should treat them as
design constraints, not optional details.

1. `FakeRoot` may materialize executable destinations as placeholders first;
   bundle rewriting must write the actual source `Executable` into that planned
   destination instead of reparsing the placeholder.
2. `Executable` validity must reflect whether LIEF actually parsed a binary,
   not whether a wrapper object merely exists.
3. Interpreter-path verification applies to the bundled executable, not to
   every shared object.
4. In `flat-lib64`, plain `chroot` execution depends on compatibility symlinks
   for `lib`, `usr/lib`, and `usr/lib64` resolving into the bundled runtime.
5. If `--interpreter` is enabled, the manifest must not duplicate the same
   loader as both interpreter payload and rewritten shared-object dependency.
6. In `preserve-source-tree`, fallback placement for application libraries must
   derive from the bundled libc or interpreter scheme instead of using a fixed
   fallback such as `lib64`.
7. Resolver scans must be path-sorted before insertion so manifests and alias
   materialization stay deterministic.
8. Symlink alias targets in `preserve-source-tree` must resolve through the
   planned bundled-path map, not by recomputing a path directly from the raw
   source symlink target.

## Current Runtime Validation State

- Focused CTest coverage passes for resolver and bundle verifier flows.
- The Docker matrix currently validates both layout policies for the glibc
  fixture.
- The musl-native scenario is wired but still depends on local `musl-gcc`
  availability.
- A real-world `/usr/bin/openssl` preserve-source-tree reproduction exposed and
  fixed alias-planning issues around broken symlink checks.

## How To Work In This Repo

### Default Development Flow

1. Start from a concrete anchor.
   Use the failing binary, failing test, failing command, or owning class.
   Avoid broad repo exploration before you can name the controlling code path.
2. Read the nearest deciding abstraction.
   For layout bugs, this is usually `BundleLayout.cpp`, `Executable.cpp`,
   `FakeRoot.cpp`, or `BundleVerifier.cpp`. For resolution bugs, start at
   `Resolver.cpp` and its tests.
3. Form one local hypothesis.
   Example: "alias targets are being mapped from raw source paths instead of
   planned bundled paths under preserve-source-tree".
4. Make the smallest grounded edit.
   Prefer one local fix over speculative multi-file cleanup.
5. Run the narrowest falsifying validation immediately.
   Use the relevant unit or integration test first. Use the Docker matrix when
   the change affects runtime layout or `chroot` execution.
6. Expand only if validation points to an adjacent control point.
   Do not widen scope without evidence.
7. Update tests and docs if behavior or invariants changed.

### Preferred Validation Order

1. Build the directly affected targets.
2. Run focused CTest coverage for resolver and verifier slices.
3. Run the Docker matrix only when layout/runtime behavior changed or when the
   bug is only reproducible under `chroot`.

### Normal Commands

Build:

```bash
cmake -S . -B build
cmake --build build
```

Focused tests:

```bash
cmake --build build --target dynpax_resolver_tests dynpax_bundle_verifier_tests
ctest --test-dir build --output-on-failure \
  -R '^dynpax\.(resolver|bundle_verifier)$'
```

Docker matrix:

```bash
cmake -S . -B build -DDYNPAX_ENABLE_DOCKER_MATRIX=ON
cmake --build build --target dynpax_docker_matrix
```

## Examples For AI Agents

### Example 1: Add Or Change A Layout Policy

When asked to change bundling layout semantics:

1. Read `src/include/BundleLayout.hpp` and `src/lib/BundleLayout.cpp` first.
2. Trace where the policy flows through `App`, `BundleBuilder`, `FakeRoot`,
   `Executable`, and `BundleVerifier`.
3. Update integration tests in `tests/integration/BundleVerifierTests.cpp`.
4. If runtime execution is affected, run the Docker matrix.

Typical mistake: updating only materialization paths and forgetting verifier or
`RUNPATH` expectations.

### Example 2: Fix A Resolver Or Alias Bug

When a bundled binary fails at runtime because a library cannot be found:

1. Inspect `Resolver.cpp` for source-path selection and scan ordering.
2. Inspect `Executable.cpp` for manifest planning and alias-target mapping.
3. Reproduce with a real binary if possible.
4. Add or tighten a fixture-based regression test.

Typical mistake: using `std::filesystem::exists()` as if it proves a symlink is
not present. Broken symlinks return false from `exists()` but still matter.

### Example 3: Verify Preserve-Source-Tree Behavior

Use this when the requested behavior is "keep the source runtime scheme intact
while still bundling app-local libraries in a runnable place":

```bash
./build/dynpax \
  --target /usr/bin/openssl \
  --fake-root /tmp/dynpax-openssl \
  --interpreter \
  --layout-policy preserve-source-tree
```

Then inspect:

- whether the interpreter stayed at its preserved runtime path
- whether bundled libc-linked payloads landed under the correct runtime tree
- whether alias symlinks resolve to planned bundled payloads
- whether verification passes without dangling planned entries

### Example 4: Validate Flat-Lib64 Compatibility

```bash
./build/dynpax \
  --target "$PWD/build/tests/fixtures/bin/hello_soname_glibc" \
  --fake-root /tmp/dynpax-flat \
  --interpreter \
  --layout-policy flat-lib64
```

Then verify the bundle still exposes compatibility links such as:

- `lib -> lib64`
- `usr/lib -> ../lib64`
- `usr/lib64 -> ../lib64`

### Example 5: Add A New Runtime Regression Test

When the bug is about layout, aliasing, or ELF rewrite behavior:

1. Prefer `tests/integration/BundleVerifierTests.cpp` if the bug can be proven
   from manifest structure or verifier expectations.
2. Prefer `tests/docker/run-matrix.sh` only when the failure requires runtime
   `chroot` execution.
3. Keep fixtures minimal. Put complexity in ELF topology, not application code.

## Guidance On Scope And Style

- Preserve deterministic behavior. Sorting and stable output matter.
- Do not change public behavior in unrelated code while fixing a local bug.
- Prefer extending existing fixtures over inventing ad hoc external repros when
  fixture coverage can express the same failure.
- If a repro depends on a host binary such as `/usr/bin/openssl`, treat it as a
  useful debugging case, then capture the durable expectation in tests.
- Keep validation focused. Running the full Docker matrix for a purely local
  parser issue is slower than needed.

## Good Starting Points By Task Type

- CLI or option bug: `src/lib/App.cpp`
- Manifest planning bug: `src/lib/Executable.cpp`
- Bundle layout bug: `src/lib/BundleLayout.cpp` and `src/lib/FakeRoot.cpp`
- Runtime verifier bug: `src/lib/BundleVerifier.cpp`
- Dependency search bug: `src/lib/Resolver.cpp`
- Integration expectation bug: `tests/integration/BundleVerifierTests.cpp`
- Container-runtime bug: `tests/docker/run-matrix.sh`

## Current Gaps To Be Aware Of

- Musl runtime coverage is conditional on local toolchain availability.
- There is still visible `Note not parsed!` noise during some runs; treat that
  as cleanup work, not as proof that the main bundle flow failed.

## Deliverable Expectations For Future Agents

Good changes in this repo usually include:

- one clear local hypothesis
- one small rooted fix
- one focused validation step immediately after the first edit
- updated tests when behavior changed
- no unrelated cleanup mixed into the same patch

If you cannot explain how a change preserves bundle layout, alias correctness,
and validation determinism, you probably have not finished reviewing it.
