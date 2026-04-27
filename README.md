# Dynpax

Dynpax is a C++ utility built on LIEF that turns a dynamically linked ELF
binary into a relocatable fake-root bundle.

## What it does

Instead of collecting `ldd` output by hand, Dynpax:

- parses ELF metadata with LIEF
- resolves transitive `DT_NEEDED` dependencies
- copies the executable, shared objects, and optional interpreter into a fake root
- preserves SONAME alias chains as symlinks
- rewrites bundled executable and shared-object `RUNPATH` values to
	`$ORIGIN/../lib64`
- removes legacy `RPATH` entries from rewritten ELFs
- bootstraps compatibility symlinks for `lib`, `usr/lib`, and `usr/lib64`
	so the bundle can run under plain `chroot`

## Build

### Dependencies

1. LIEF 0.17.6
2. CLI11 2.6.2
3. fmt 12.1.0
4. CMake 3.28+
5. A C++26-capable compiler

### Local build

```bash
cmake -S . -B build
cmake --build build
```

### Docker image

```bash
docker build -t smartcoder/dynpax .
```

See `Dockerfile.example` for a containerized usage example.

## CLI

```text
./build/dynpax [OPTIONS]

	-t, --target TEXT ...   Target ELF executables list (comma-separated)
	-f, --fake-root TEXT    Output fake root directory
	-i, --interpreter       Copy and bundle the ELF interpreter
```

## Usage Examples

### 1. Bundle a system ELF into a fake root

```bash
./build/dynpax \
	--target /bin/echo \
	--fake-root /tmp/dynpax-example-echo \
	--interpreter
```

Expected layout:

- `/tmp/dynpax-example-echo/bin/echo`
- `/tmp/dynpax-example-echo/lib64/libc.so.6`
- `/tmp/dynpax-example-echo/lib64/ld-linux-x86-64.so.2`
- `/tmp/dynpax-example-echo/lib -> lib64`
- `/tmp/dynpax-example-echo/usr/lib -> ../lib64`

### 2. Bundle an ELF that depends on non-system libraries

If the target depends on libraries outside the default loader search roots,
make that directory discoverable during bundling.

```bash
LD_LIBRARY_PATH="$PWD/build/tests/fixtures/lib" \
./build/dynpax \
	--target "$PWD/build/tests/fixtures/bin/hello_soname_glibc" \
	--fake-root /tmp/dynpax-example-soname \
	--interpreter
```

This bundle preserves the SONAME chain:

- `libdynpaxgreet.so -> libdynpaxgreet.so.1`
- `libdynpaxgreet.so.1 -> libdynpaxgreet.so.1.2.0`

### 3. Validate the bundle with plain `chroot`

```bash
docker run --rm \
	--volume /tmp/dynpax-example-echo:/bundle:ro \
	--entrypoint /bin/sh \
	ubuntu:24.04 \
	-lc 'chroot /bundle /bin/echo dynpax-ok'
```

## Verified Requirements

The following outcomes were verified in this workspace:

- Bundling `/bin/echo` with `--interpreter` succeeds.
- Bundling the SONAME fixture succeeds when its library directory is exposed
	through `LD_LIBRARY_PATH`.
- Bundled executables and bundled shared objects retain only
	`RUNPATH [$ORIGIN/../lib64]`.
- Bundled ELFs no longer retain `RPATH`.
- SONAME alias chains are materialized as symlinks inside the bundle.
- `chroot /bundle /bin/echo dynpax-ok` succeeds in `ubuntu:24.04`.
- `chroot /bundle /bin/hello_soname_glibc` succeeds in both
	`ubuntu:24.04` and `alpine:latest`.

## Current Scope

- The glibc path is verified end to end: manifest planning, ELF rewrite,
	fake-root materialization, and runtime execution.
- The musl fixture path is wired into CMake when `musl-gcc` is available.
- The Docker matrix is available as an opt-in target for broader validation.

## Related Test Assets

- `tests/fixtures/README.md`: fixture inventory
- `tests/docker/README.md`: Docker validation matrix
