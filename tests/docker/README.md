# Docker Matrix

This directory holds the opt-in container validation slice from the implementation backlog.

## Covered Scenarios

- `ubuntu:24.04`: runs a glibc bundle under `chroot`.
- `debian:stable-slim`: reruns the same glibc bundle in a second glibc image.
- `alpine:latest`: runs the same glibc bundle in Alpine to validate cross-environment self-containment.
- `alpine:latest` with a musl fixture: runs a musl-native bundle when `musl-gcc` produced `hello_musl` locally.

## Usage

- Configure with `-DDYNPAX_ENABLE_DOCKER_MATRIX=ON` to add the `dynpax_docker_matrix` build target.
- Leave `DYNPAX_ENABLE_MUSL_FIXTURE=ON` to build `hello_musl` automatically when `musl-gcc` is available.
- Run `cmake --build build --target dynpax_docker_matrix` to execute the matrix.

## Notes

- Docker validation stays opt-in so default local builds do not require container tooling.
- The script writes bundle-generation and scenario logs under `build/tests/docker/logs` when invoked through CMake.
- If `musl-gcc` is unavailable, the Alpine musl-native scenario is skipped while the glibc scenarios still run.