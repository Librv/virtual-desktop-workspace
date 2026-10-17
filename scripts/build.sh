#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Librv
# SPDX-License-Identifier: MIT
# build.sh: one command to rebuild, test or install the project.
#
# Usage:
#   scripts/build.sh [build]   configure (first time only) and build the dev preset
#   scripts/build.sh test      build, then run the unit tests
#   scripts/build.sh format    run clang-format on the C++ sources
#   scripts/build.sh clean     delete build/dev, the next build configures again
#   scripts/build.sh install   build and install the Arch package (asks for
#                              sudo), then stop the running vdwd so the next
#                              shortcut or vdwctl call starts the new one

set -euo pipefail

cd "$(dirname "$0")/.."

build() {
    [[ -f build/dev/build.ninja ]] || cmake --preset dev
    cmake --build --preset dev
}

case ${1:-build} in
    build) build ;;
    test) build && ctest --preset dev ;;
    format) build && cmake --build --preset dev --target clang-format ;;
    clean) rm -rf build/dev ;;
    install)
        # -f: rebuild even when the package version did not change.
        (cd packaging/arch && makepkg -sif)
        # The old daemon keeps running the old binary; D-Bus activation starts the new one.
        pkill -x vdwd && echo "Stopped the old vdwd; the next Ctrl+Meta+D or vdwctl call starts the new one." || true
        ;;
    *)
        sed -n 's/^#   //p' "$0" >&2
        exit 2
        ;;
esac
