#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
action=${1:-build}
preset=${2:-linux-debug}
case "$preset" in linux-debug|linux-release) ;; *) echo "Use linux-debug or linux-release" >&2; exit 2;; esac
case "$action" in
  configure) cmake --preset "$preset" ;;
  build) cmake --build --preset "$preset" --parallel ;;
  test) ctest --preset "$preset" ;;
  package) ctest --preset "$preset"; cpack --config "build/$preset/CPackConfig.cmake" ;;
  *) echo "Use configure, build, test or package" >&2; exit 2 ;;
esac
