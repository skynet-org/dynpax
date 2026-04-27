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

## Testing

Dynpax has three practical test layers:

- resolver unit tests
- bundle-verifier integration tests built from real ELF fixtures
- an opt-in Docker matrix that executes bundled binaries under `chroot`

### 1. Build the test targets

`BUILD_TESTING` is handled through CTest. A normal local configure is enough:

```bash
cmake -S . -B build
cmake --build build --target dynpax_resolver_tests dynpax_bundle_verifier_tests
```

If you want the Docker matrix target as well, configure with:

```bash
cmake -S . -B build \
	-DDYNPAX_ENABLE_DOCKER_MATRIX=ON
```

### 2. Run the focused local tests

Run the two main non-container checks with CTest:

```bash
ctest --test-dir build --output-on-failure \
	-R '^dynpax\.(resolver|bundle_verifier)$'
```

What they cover:

- `dynpax.resolver`: synthetic resolver tests, including alias chains and
	runtime search-root precedence
- `dynpax.bundle_verifier`: real fixture bundling, metadata rewriting,
	manifest verification, and fake-root layout validation

You can also run them individually:

```bash
ctest --test-dir build --output-on-failure -R '^dynpax\.resolver$'
ctest --test-dir build --output-on-failure -R '^dynpax\.bundle_verifier$'
```

### 3. Run the Docker matrix

The Docker matrix is opt-in by design. It is not required for normal local
builds.

Prerequisites:

- Docker must be installed and runnable from the current shell.
- The project must be configured with `-DDYNPAX_ENABLE_DOCKER_MATRIX=ON`.
- If `musl-gcc` is available and `DYNPAX_ENABLE_MUSL_FIXTURE=ON` remains set,
	the matrix will also include the musl-native Alpine case.

Configure and run:

```bash
cmake -S . -B build \
	-DDYNPAX_ENABLE_DOCKER_MATRIX=ON

cmake --build build --target dynpax_docker_matrix
```

What the matrix currently does:

- bundles a glibc fixture with Dynpax
- runs that bundle in `ubuntu:24.04`
- reruns the same bundle in `debian:stable-slim`
- reruns the same bundle in `alpine:latest`
- if the musl fixture exists, builds and runs the musl-native Alpine case too

Where to inspect output:

- bundle-generation and container logs are written under
	`build/tests/docker/logs`
- the matrix is driven by `tests/docker/run-matrix.sh`
- the runner uses an isolated temporary Docker config by default so broken host
	credential-helper settings do not block pulls for the public matrix images
- set `DYNPAX_DOCKER_CONFIG=/path/to/docker-config` if you want the matrix to
	use an existing Docker config instead

### 4. Common test workflows

Quick local regression check:

```bash
cmake --build build --target dynpax_resolver_tests dynpax_bundle_verifier_tests
ctest --test-dir build --output-on-failure \
	-R '^dynpax\.(resolver|bundle_verifier)$'
```

Full container validation:

```bash
cmake -S . -B build \
	-DDYNPAX_ENABLE_DOCKER_MATRIX=ON
cmake --build build --target dynpax_docker_matrix
```

If `musl-gcc` is missing, the Docker matrix still runs the glibc scenarios and
skips the musl-native case cleanly.

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

Dynpax now resolves dependencies from the target ELF's embedded `RUNPATH` or
`RPATH` before falling back to the populated system search roots.

```bash
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
- Bundling the SONAME fixture succeeds from its embedded `RUNPATH` without
	extra resolver roots or `LD_LIBRARY_PATH`.
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
