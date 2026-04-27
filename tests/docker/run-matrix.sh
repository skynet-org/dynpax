#!/usr/bin/env bash

set -euo pipefail

docker_bin="docker"
dynpax_bin=""
glibc_fixture=""
musl_fixture=""
log_dir="${DYNPAX_DOCKER_LOG_DIR:-}"

usage() {
    cat <<'EOF'
Usage: run-matrix.sh --dynpax <path> --glibc-fixture <path> [--musl-fixture <path>] [--docker <path>]

Runs the dynpax Docker validation matrix from the implementation backlog.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --docker)
            docker_bin="$2"
            shift 2
            ;;
        --dynpax)
            dynpax_bin="$2"
            shift 2
            ;;
        --glibc-fixture)
            glibc_fixture="$2"
            shift 2
            ;;
        --musl-fixture)
            musl_fixture="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$dynpax_bin" || -z "$glibc_fixture" ]]; then
    usage >&2
    exit 1
fi

if [[ ! -x "$dynpax_bin" ]]; then
    echo "dynpax binary is not executable: $dynpax_bin" >&2
    exit 1
fi

if [[ ! -f "$glibc_fixture" ]]; then
    echo "glibc fixture is missing: $glibc_fixture" >&2
    exit 1
fi

if [[ -n "$musl_fixture" && ! -f "$musl_fixture" ]]; then
    echo "musl fixture is missing: $musl_fixture" >&2
    exit 1
fi

work_root="$(mktemp -d "${TMPDIR:-/tmp}/dynpax-docker-matrix-XXXXXX")"
cleanup() {
    rm -rf "$work_root"
}
trap cleanup EXIT

if [[ -z "$log_dir" ]]; then
    log_dir="$work_root/logs"
fi
mkdir -p "$log_dir"

bundle_fixture() {
    local fixture_path="$1"
    local bundle_root="$2"
    local log_path="$3"

    mkdir -p "$bundle_root"
    "$dynpax_bin" --target "$fixture_path" --fake-root "$bundle_root" --interpreter >"$log_path" 2>&1
}

run_chroot_case() {
    local scenario_name="$1"
    local image_name="$2"
    local bundle_root="$3"
    local executable_name="$4"
    local log_path="$log_dir/${scenario_name}.log"

    "$docker_bin" run --rm \
        --volume "$bundle_root:/bundle:ro" \
        --entrypoint /bin/sh \
        "$image_name" \
        -lc "chroot /bundle /bin/${executable_name}" >"$log_path" 2>&1
}

glibc_bundle="$work_root/glibc-bundle"
bundle_fixture "$glibc_fixture" "$glibc_bundle" "$log_dir/glibc-bundle.log"

run_chroot_case ubuntu-24.04 ubuntu:24.04 "$glibc_bundle" "$(basename "$glibc_fixture")"
run_chroot_case debian-stable-slim debian:stable-slim "$glibc_bundle" "$(basename "$glibc_fixture")"
run_chroot_case alpine-latest-glibc alpine:latest "$glibc_bundle" "$(basename "$glibc_fixture")"

if [[ -n "$musl_fixture" ]]; then
    musl_bundle="$work_root/musl-bundle"
    bundle_fixture "$musl_fixture" "$musl_bundle" "$log_dir/musl-bundle.log"
    run_chroot_case alpine-latest-musl alpine:latest "$musl_bundle" "$(basename "$musl_fixture")"
else
    echo "Skipping musl Docker scenario because no musl fixture was provided."
fi

echo "Docker matrix completed successfully. Logs: $log_dir"