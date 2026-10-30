#!/usr/bin/env bash
# The CI pipeline on this machine, so that a push meets no surprises. Run it from anywhere in the repository:
#
#     scripts/check.sh           what CI runs: clang-format, debug and asan builds and tests here, and the
#                                Ubuntu job in Docker (a few minutes; the first Docker run takes longer)
#     scripts/check.sh --quick   clang-format, and the debug build and tests here (about a minute)
#     scripts/check.sh --linux   only the Ubuntu job in Docker
#
# It stops at the first failing step and names it. Each step's full output goes to build/check-logs/; a failing
# step's last lines are printed. The pre-push hook (scripts/git-hooks/pre-push) runs it with no options.
set -uo pipefail

repo="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
cd "$repo" || exit 2
logs="$repo/build/check-logs"
mkdir -p "$logs"

readonly kClangFormatVersion="22.1.8"
readonly kDockerImage="id-vision-lab-ubuntu"

usage() {
    sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
}

mode="all"
case "${1:-}" in
"") ;;
--quick) mode="quick" ;;
--linux) mode="linux" ;;
-h | --help)
    usage
    exit 0
    ;;
*)
    usage >&2
    exit 2
    ;;
esac

# Runs one step with its output in build/check-logs/<id>.log; prints its time, or the log's end when it fails,
# and stops there.
step() {
    local id="$1"
    local name="$2"
    shift 2
    local log="$logs/$id.log"
    local started=$SECONDS
    printf '%-28s' "$name"
    if "$@" >"$log" 2>&1; then
        printf 'passed  %4ds\n' $((SECONDS - started))
        return 0
    fi
    printf 'FAILED  %4ds   (full output: %s)\n\n' $((SECONDS - started)) "${log#"$repo"/}"
    tail -n 40 "$log"
    echo
    echo "check.sh: [$name] failed."
    exit 1
}

# The same clang-format as CI and Qt Creator: other versions format a few constructs differently.
find_clang_format() {
    local candidate
    for candidate in "${CLANG_FORMAT:-}" clang-format \
        "/Applications/Qt Creator.app/Contents/Resources/libexec/clang/bin/clang-format"; do
        if [[ -n "$candidate" ]] && command -v "$candidate" >/dev/null 2>&1 &&
            "$candidate" --version 2>/dev/null | grep -q "version $kClangFormatVersion"; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

check_format() {
    local clang_format
    if ! clang_format="$(find_clang_format)"; then
        echo "clang-format $kClangFormatVersion not found. Install it with: pipx install clang-format==$kClangFormatVersion"
        echo "or point CLANG_FORMAT at it."
        return 1
    fi
    git ls-files -z '*.cpp' '*.h' | xargs -0 "$clang_format" --dry-run --Werror
}

build_and_test() {
    local preset="$1"
    cmake --preset "$preset" && cmake --build --preset "$preset" && ctest --preset "$preset"
}

linux_in_docker() {
    if ! docker info >/dev/null 2>&1; then
        echo "Docker is not running. Start Docker Desktop, or run scripts/check.sh --quick."
        return 1
    fi
    docker build --tag "$kDockerImage" --file docker/ubuntu-24.04.Dockerfile scripts &&
        docker run --rm --volume "$repo":/src:ro "$kDockerImage"
}

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    echo "Note: checking the working tree, which has uncommitted changes."
fi

if [[ "$mode" != "linux" ]]; then
    step format "clang-format" check_format
    step debug "debug ($(uname -s))" build_and_test debug
fi
if [[ "$mode" == "all" ]]; then
    step asan "asan ($(uname -s))" build_and_test asan
fi
if [[ "$mode" != "quick" ]]; then
    step ubuntu "Ubuntu 24.04 (Docker)" linux_in_docker
fi
echo "check.sh: all steps passed."
