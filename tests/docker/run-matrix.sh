#!/usr/bin/env bash

set -euo pipefail

docker_bin="docker"
dynpax_bin=""
glibc_fixture=""
musl_fixture=""
log_dir="${DYNPAX_DOCKER_LOG_DIR:-}"
docker_config="${DYNPAX_DOCKER_CONFIG:-}"
work_root="${DYNPAX_DOCKER_WORK_ROOT:-}"

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

if [[ -n "$work_root" ]]; then
    rm -rf "$work_root"
    mkdir -p "$work_root"
else
    work_root="$(mktemp -d "${TMPDIR:-/tmp}/dynpax-docker-matrix-XXXXXX")"
fi
cleanup() {
    rm -rf "$work_root"
}
trap cleanup EXIT

if [[ -z "$log_dir" ]]; then
    log_dir="$work_root/logs"
fi
mkdir -p "$log_dir"

if [[ -z "$docker_config" ]]; then
    docker_config="$work_root/docker-config"
    mkdir -p "$docker_config"
    cat >"$docker_config/config.json" <<'EOF'
{
  "auths": {}
}
EOF
fi

run_docker() {
    DOCKER_CONFIG="$docker_config" "$docker_bin" "$@"
}

bundle_fixture() {
    local layout_policy="$1"
    local fixture_path="$2"
    local bundle_root="$3"
    local log_path="$4"

    mkdir -p "$bundle_root"
    "$dynpax_bin" \
        --target "$fixture_path" \
        --fake-root "$bundle_root" \
        --layout-policy "$layout_policy" \
        --interpreter >"$log_path" 2>&1
}

verify_flat_bundle_layout() {
    local bundle_root="$1"

    [[ -d "$bundle_root/lib64" ]]
    [[ -L "$bundle_root/lib" ]]
    [[ "$(readlink "$bundle_root/lib")" == "lib64" ]]
    [[ -L "$bundle_root/usr/lib" ]]
    [[ "$(readlink "$bundle_root/usr/lib")" == "../lib64" ]]
    [[ -L "$bundle_root/usr/lib64" ]]
    [[ "$(readlink "$bundle_root/usr/lib64")" == "../lib64" ]]
}

run_policy_matrix() {
    local layout_policy="$1"
    local fixture_label="$2"
    local fixture_path="$3"
    local executable_name="$4"
    shift 4

    local bundle_root="$work_root/${fixture_label}-${layout_policy}"
    local bundle_log="$log_dir/${fixture_label}-${layout_policy}-bundle.log"

    bundle_fixture "$layout_policy" "$fixture_path" "$bundle_root" "$bundle_log"
    if [[ "$layout_policy" == "flat-lib64" ]]; then
        verify_flat_bundle_layout "$bundle_root"
    fi

    while [[ $# -gt 0 ]]; do
        local scenario_name="$1"
        local image_name="$2"
        run_chroot_case "${fixture_label}-${layout_policy}-${scenario_name}" \
            "$image_name" "$bundle_root" "$executable_name"
        shift 2
    done
}

run_chroot_case() {
    local scenario_name="$1"
    local image_name="$2"
    local bundle_root="$3"
    local executable_name="$4"
    local log_path="$log_dir/${scenario_name}.log"

    run_docker run --rm \
        --volume "$bundle_root:/bundle:ro" \
        --entrypoint /bin/sh \
        "$image_name" \
        -lc "chroot /bundle /bin/${executable_name}" >"$log_path" 2>&1
}

for layout_policy in flat-lib64 preserve-source-tree; do
    run_policy_matrix "$layout_policy" glibc "$glibc_fixture" \
        "$(basename "$glibc_fixture")" \
        ubuntu-24.04 ubuntu:24.04 \
        debian-stable-slim debian:stable-slim \
        alpine-latest alpine:latest
done

if [[ -n "$musl_fixture" ]]; then
    for layout_policy in flat-lib64 preserve-source-tree; do
        run_policy_matrix "$layout_policy" musl "$musl_fixture" \
            "$(basename "$musl_fixture")" \
            ubuntu-24.04 ubuntu:24.04 \
            debian-stable-slim debian:stable-slim \
            alpine-latest alpine:latest
    done
else
    echo "Skipping musl Docker scenario because no musl fixture was provided."
fi

echo "Docker matrix completed successfully. Logs: $log_dir"