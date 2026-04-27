# Fixture Scaffold

This directory holds the ELF fixture matrix used by unit and integration tests.

## Source Layout

- `src/hello_glibc/`: minimal glibc-linked executable source.
- `src/hello_transitive_glibc/`: executable that will link through `libgreet.so` into `libmessage.so`.
- `src/hello_soname_glibc/`: executable source used with linker flags to produce SONAME alias chains.
- `src/hello_abs_runpath_glibc/`: executable source used with linker flags that inject an absolute RUNPATH.
- `src/hello_musl/`: minimal executable source intended for Alpine or musl builds.
- `src/hello_no_interp_so/`: shared-object source used to validate `PT_INTERP` absence handling.
- `src/shared/`: reusable helper library sources for transitive dependency fixtures.

## Planned Docker Matrix

- `ubuntu-24.04`: build and validate glibc fixtures, including `chroot` execution.
- `debian-stable-slim`: rerun glibc bundle execution in a second glibc environment.
- `alpine-latest`: build musl fixtures and validate musl-native bundles.
- `alpine-latest` with mounted glibc bundle: validate bundled glibc execution without relying on Alpine host libraries.

## Current Scope

- The glibc fixtures and shared-library chain are wired into CMake build targets.
- The musl fixture now builds through `musl-gcc` when that toolchain is present; otherwise it is skipped cleanly.
- Resolver unit tests still create synthetic ELF files to isolate resolver behavior.
- Docker validation now lives under `tests/docker/` as an opt-in matrix target.
