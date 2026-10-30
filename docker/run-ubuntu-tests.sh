#!/usr/bin/env bash
# Runs inside the id-vision-lab-ubuntu container (docker/ubuntu-24.04.Dockerfile): the same steps as the CI's
# Ubuntu job, on a copy of the source mounted read-only at /src. The copy leaves out build trees and .git, so
# the host's own builds are never touched or reused.
set -euo pipefail

mkdir -p /work
tar --exclude=./build --exclude=./.git -C /src -cf - . | tar -C /work -xf -
cd /work

echo "== Ubuntu 24.04: configure, build and test (debug)"
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

echo "== Ubuntu 24.04: build and test without Qt"
cmake --preset debug -B build/no-qt -DID_VISION_LAB_BUILD_QT_APPS=OFF
cmake --build build/no-qt
ctest --test-dir build/no-qt --output-on-failure
