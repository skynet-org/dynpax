# Dynpax Implementation Backlog

This backlog turns the current investigation into a sequence of concrete issues.

Ordering rule:

- Design the fixture set and Docker matrix first so implementation has clear acceptance targets.
- Start implementation with the resolver and manifest refactor.

## Milestone 0: Test Design Baseline

### Issue 0.1: Define ELF Fixture Set

Goal:

- Create a small, deterministic set of ELF fixtures that exercise dependency resolution, SONAME aliasing, interpreter rewriting, and chroot execution.

Deliverables:

- A `tests/fixtures/src/` tree with source for fixture executables and shared libraries.
- A `tests/fixtures/README.md` describing what each fixture validates.
- A fixture build script or CMake target that produces all test binaries reproducibly.

Required fixtures:

- `hello_glibc`: minimal dynamically linked executable with one direct dependency.
- `hello_transitive_glibc`: executable linked to `libgreet.so`, where `libgreet.so` depends on `libmessage.so`.
- `hello_soname_glibc`: executable that loads a library distributed as symlink chain such as `libalias.so -> libalias.so.1 -> libalias.so.1.0`.
- `hello_abs_runpath_glibc`: executable or library built with a host absolute RUNPATH, used to verify cleanup.
- `hello_musl`: minimal musl-linked executable built inside Alpine.
- `hello_no_interp_so`: shared object fixture to prove interpreter rewriting is only applied to ELFs that actually have `PT_INTERP`.

Acceptance criteria:

- Each fixture has one documented purpose.
- The transitive fixture requires recursive dependency traversal to succeed.
- The SONAME fixture requires alias preservation, not just copying canonical filenames.
- At least one fixture produces a shared object with no `PT_INTERP`.

Notes:

- Keep fixture code trivial. Complexity should be in ELF topology, not program logic.
- Avoid relying on host toolchain quirks beyond glibc and musl runtime differences.

### Issue 0.2: Define Docker Validation Matrix

Goal:

- Specify the container environments used to validate fake-root execution, especially glibc and musl behavior.

Deliverables:

- A test matrix document in `tests/fixtures/README.md`.
- One Dockerfile per scenario or a parameterized build script.

Required matrix:

- `ubuntu-24.04`: build and validate glibc fixtures, run bundled output with `chroot`.
- `debian-stable-slim`: validate the same glibc bundle against a second glibc base image.
- `alpine-latest`: build musl fixtures and validate musl-native bundles with `chroot`.
- `alpine-latest` with mounted glibc bundle: verify bundled glibc program still runs inside Alpine because it carries its own loader and libraries.

Test modes:

- Metadata validation only.
- Bundle filesystem layout validation.
- In-container `chroot` execution validation.

Acceptance criteria:

- Every required runtime scenario maps to at least one fixture.
- The matrix explicitly distinguishes host-built glibc bundles from Alpine-built musl bundles.
- The matrix uses volume mounts so bundle outputs can be inspected after container runs.

### Issue 0.3: Choose Test Harness Structure

Goal:

- Decide how unit, integration, and container tests will be organized before implementation work begins.

Deliverables:

- Proposed layout:

  - `tests/unit/`
  - `tests/integration/`
  - `tests/fixtures/`
  - `tests/docker/`

- CMake test target plan using `CTest`.
- Decision on assertion library and test runner.

Acceptance criteria:

- Unit tests can run without Docker.
- Integration tests can parse bundled ELFs and filesystem layout.
- Container tests can be gated separately for environments where Docker is unavailable.

Recommended decision:

- Use `CTest` with a small C++ unit test framework such as Catch2 or doctest.
- Keep Docker-driven tests as opt-in integration targets, not mandatory for a plain compile.

## Milestone 1: Resolver And Manifest Refactor

### Issue 1.1: Introduce Bundle Manifest Model

Goal:

- Replace ad hoc dependency copying with a typed manifest that becomes the source of truth for copy, rewrite, and verification steps.

Deliverables:

- New types for `BundleEntry`, `BundleManifest`, `ResolvedDependency`, and `InterpreterSpec`.
- Each manifest entry records:

  - source path
  - bundled path
  - ELF kind
  - SONAME if present
  - requested library name from `DT_NEEDED`
  - whether the file has `PT_INTERP`
  - rewrite status for `RUNPATH`, `RPATH`, and interpreter

Acceptance criteria:

- Manifest can represent executables, interpreters, real shared objects, and symlink aliases.
- Manifest is independent from CLI parsing and filesystem copy operations.
- The bundling pipeline can serialize manifest contents for debug output.

### Issue 1.2: Replace Filename-Only Resolution With Canonical Resolver

Goal:

- Upgrade host library discovery so resolution is based on canonical file identity and alias metadata, not just the first matching basename.

Current limitation:

- [src/lib/ELFCache.cpp](/home/vasyl/Dev/skynet/dynpax/src/lib/ELFCache.cpp) stores a single path per filename and can lose alias and symlink information.

Deliverables:

- `Resolver` abstraction extracted from the current cache implementation.
- Search-source handling for:

  - default system library directories
  - `LD_LIBRARY_PATH`
  - `ld.so.conf` and `ld.so.conf.d`

- Canonicalization rules that preserve both:

  - the real file path
  - the alias name used to satisfy a `DT_NEEDED` entry

Acceptance criteria:

- Resolution can distinguish canonical payload from link name.
- Multiple aliases to the same target do not create duplicate payload entries.
- Resolver behavior is covered by unit tests using synthetic directories.

### Issue 1.3: Build Recursive Dependency Graph From Parsed ELF Metadata

Goal:

- Traverse runtime dependencies once, recursively, and emit a stable graph that can be converted into the bundle manifest.

Deliverables:

- Dependency walker that parses `DT_NEEDED`, interpreter, and SONAME information via LIEF.
- Cycle-safe recursion with stable deduplication rules.
- Graph to manifest conversion step.

Acceptance criteria:

- Recursive traversal succeeds for direct and transitive fixtures.
- Missing libraries are surfaced as structured resolution errors.
- Manifest order is deterministic for repeatable tests.

### Issue 1.4: Refactor CLI To Consume Manifest Pipeline

Goal:

- Move orchestration out of [src/bin/dynpax.cpp](/home/vasyl/Dev/skynet/dynpax/src/bin/dynpax.cpp) so the CLI becomes a thin wrapper over reusable bundling components.

Deliverables:

- `BundlePlanner` or equivalent service that owns resolution and manifest generation.
- CLI output updated to report manifest summary rather than inline copy decisions.

Acceptance criteria:

- The CLI no longer performs direct dependency traversal itself.
- Core bundling logic is invokable from tests without going through argument parsing.

## Milestone 2: Fake Root Layout And Copy Policy

### Issue 2.1: Bootstrap Deterministic Fake Root Layout

Goal:

- Always create a predictable fake-root structure that supports loader lookup and chroot execution.

Deliverables:

- Explicit creation of `bin`, `lib`, and `lib64`.
- Layout policy that decides where executables, interpreters, libraries, and symlinks live.

Acceptance criteria:

- Empty bundle creation succeeds before any copy operations begin.
- Layout policy is not hardcoded to x86_64 glibc assumptions alone.

### Issue 2.2: Preserve Loader-Relevant Aliases And Symlinks

Goal:

- Recreate the filesystem names the dynamic loader expects, not just copied payload files.

Deliverables:

- Copy logic for canonical ELF payloads.
- Symlink recreation logic for SONAME and alias chains.

Acceptance criteria:

- The SONAME fixture bundle retains the name requested by `DT_NEEDED`.
- Duplicate payloads are not copied multiple times.

## Milestone 3: ELF Rewrite Pipeline

### Issue 3.1: Rewrite RUNPATH And Clear RPATH For Every Bundled ELF

Goal:

- Apply loader path normalization to every bundled ELF that participates in runtime linking.

Deliverables:

- Rewrite pass over manifest entries.
- Standard bundled RUNPATH value of `$ORIGIN/../lib64` unless layout policy requires a different relative path.

Acceptance criteria:

- No bundled ELF retains a stale absolute RPATH.
- Executables and shared libraries are both covered.

### Issue 3.2: Rewrite Interpreter Only Where `PT_INTERP` Exists

Goal:

- Make interpreter changes correct and explicit instead of assuming every dependency can or should receive one.

Deliverables:

- Detection of `PT_INTERP` on each ELF.
- Rewrite and persistence verification step after write-out.

Acceptance criteria:

- Main executable points to the bundled interpreter path.
- Shared-object fixture without `PT_INTERP` is left unchanged.
- Interpreter rewrite is verified by re-parsing the output ELF.

## Milestone 4: Verification And Automated Tests

### Issue 4.1: Build Static Bundle Verifier

Goal:

- Parse the produced fake root and assert that all required invariants hold before runtime tests begin.

Deliverables:

- Verifier that checks:

  - expected directories exist
  - every bundled ELF resolves its `DT_NEEDED` entries inside the bundle
  - interpreter paths point inside the bundle
  - RUNPATH and RPATH policies are satisfied

Acceptance criteria:

- Verifier produces actionable failures, not just boolean pass or fail.
- Integration tests can run verifier without requiring `chroot`.

### Issue 4.2: Add Unit Tests For Resolver And Manifest Logic

Goal:

- Lock down the new resolver and graph behavior before copy and rewrite work expands.

Deliverables:

- Unit tests for search path parsing, symlink handling, alias preservation, canonical deduplication, and graph determinism.

Acceptance criteria:

- Resolver regressions are caught without Docker.
- Tests cover both success and missing-library cases.

### Issue 4.3: Add Integration Tests For Fixture Bundles

Goal:

- Validate end-to-end bundling behavior using the defined ELF fixtures.

Deliverables:

- Tests that invoke dynpax on fixture binaries.
- Assertions against bundle layout and ELF metadata.

Acceptance criteria:

- The transitive, SONAME, absolute-RUNPATH, and musl fixtures all have coverage.

## Milestone 5: Docker And Chroot Validation

### Issue 5.1: Add Docker Scenarios For Glibc Bundles

Goal:

- Verify glibc bundles run under `chroot` in more than one glibc container.

Deliverables:

- Docker scenarios for Ubuntu and Debian.
- Volume-mounted bundle output and logs.

Acceptance criteria:

- `chroot <bundle> /bin/<fixture>` succeeds for required glibc fixtures.

### Issue 5.2: Add Alpine Scenarios For Musl And Cross-Environment Validation

Goal:

- Prove musl-native bundles work and confirm a glibc bundle remains self-contained enough to run in Alpine.

Deliverables:

- Alpine scenario for musl fixture build and execution.
- Alpine scenario that mounts a previously generated glibc bundle and runs it via `chroot`.

Acceptance criteria:

- Musl fixture succeeds in Alpine.
- Bundled glibc fixture also succeeds in Alpine without relying on host Alpine libraries.

## Milestone 6: CLI, Docs, And Release Readiness

### Issue 6.1: Improve CLI Reporting And Debug Output

Goal:

- Make it easy to inspect what dynpax resolved, copied, rewrote, and skipped.

Deliverables:

- Summary output derived from the manifest.
- Optional verbose mode for detailed resolution traces.

Acceptance criteria:

- A failed run clearly reports which dependency or rewrite step failed.

### Issue 6.2: Update Project Documentation

Goal:

- Document the new bundling guarantees and test workflow.

Deliverables:

- Update [README.md](/home/vasyl/Dev/skynet/dynpax/README.md) with supported behavior and limitations.
- Add usage notes for fixture builds and Docker validation.

Acceptance criteria:

- A contributor can reproduce unit, integration, and Docker validation from the docs.

## Suggested Execution Order

1. Issue 0.1
2. Issue 0.2
3. Issue 0.3
4. Issue 1.1
5. Issue 1.2
6. Issue 1.3
7. Issue 1.4
8. Issue 2.1
9. Issue 2.2
10. Issue 3.1
11. Issue 3.2
12. Issue 4.1
13. Issue 4.2
14. Issue 4.3
15. Issue 5.1
16. Issue 5.2
17. Issue 6.1
18. Issue 6.2

## Next Implementation Slice

Start here:

- Issue 1.1: introduce the bundle manifest model.
- Issue 1.2: extract a canonical resolver from the current ELF cache.

Do not start copy-policy or LIEF rewrite changes until the fixture set and Docker matrix from Milestone 0 are written down and accepted.
