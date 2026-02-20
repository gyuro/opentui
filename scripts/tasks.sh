#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

SCRIPT_NAME="$(basename "$0")"

usage() {
  cat <<EOF
Usage: ${SCRIPT_NAME} <task>

Tasks collected from README.md:
  list         Print the available tasks and underlying commands.
  build        Run CMake configure and build.
  lint-format  Run clang-format dry-run check.
  lint-tidy    Run clang-tidy (adds macOS SDK args when available).
  run-example  Run the example debugger binary.
  all          Run: build, lint-format, lint-tidy.

Examples:
  ./${SCRIPT_NAME} list
  ./${SCRIPT_NAME} build
  ./${SCRIPT_NAME} lint-format
EOF
}

require_tool() {
  local tool="$1"
  if ! command -v "${tool}" >/dev/null 2>&1; then
    echo "error: required tool '${tool}' not found in PATH" >&2
    exit 1
  fi
}

task_list() {
  cat <<'EOF'
[build]
  cmake -S . -B build
  cmake --build build

[lint-format]
  find include src examples -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 | xargs -0 clang-format --dry-run --Werror

[lint-tidy]
  macOS:
    SDKROOT=$(xcrun --show-sdk-path)
    clang-tidy -p build src/*.cpp examples/debugger/main.cpp --extra-arg=-isysroot --extra-arg=$SDKROOT --quiet
  Linux:
    clang-tidy -p build src/*.cpp examples/debugger/main.cpp --quiet

[run-example]
  ./build/open_tui_example
EOF
}

task_build() {
  require_tool cmake
  cmake -S . -B build
  cmake --build build
}

task_lint_format() {
  require_tool clang-format
  find include src examples -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 | \
    xargs -0 clang-format --dry-run --Werror
}

task_lint_tidy() {
  require_tool cmake
  require_tool clang-tidy

  cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null

  local -a tidy_args
  tidy_args=(-p build src/*.cpp examples/debugger/main.cpp --quiet)

  if command -v xcrun >/dev/null 2>&1; then
    local sdkroot
    sdkroot="$(xcrun --show-sdk-path)"
    tidy_args+=(--extra-arg=-isysroot "--extra-arg=${sdkroot}")
  fi

  clang-tidy "${tidy_args[@]}"
}

task_run_example() {
  if [[ ! -x ./build/open_tui_example ]]; then
    echo "error: ./build/open_tui_example not found. Run '${SCRIPT_NAME} build' first." >&2
    exit 1
  fi
  ./build/open_tui_example
}

task_all() {
  task_build
  task_lint_format
  task_lint_tidy
}

main() {
  local task="${1:-list}"

  case "${task}" in
    list)
      task_list
      ;;
    build)
      task_build
      ;;
    lint-format)
      task_lint_format
      ;;
    lint-tidy)
      task_lint_tidy
      ;;
    run-example)
      task_run_example
      ;;
    all)
      task_all
      ;;
    -h | --help | help)
      usage
      ;;
    *)
      echo "error: unknown task '${task}'" >&2
      usage
      exit 1
      ;;
  esac
}

main "$@"
