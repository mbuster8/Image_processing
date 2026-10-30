#!/usr/bin/env bash
# The Ubuntu 24.04 packages that build and test the project: the distribution's OpenCV 4.6 and Qt 6.4, the
# oldest versions it supports. The CI's install-ubuntu-deps action and docker/ubuntu-24.04.Dockerfile both run
# this, so the list lives in one place. Needs root.
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install --yes --no-install-recommends \
    build-essential cmake ninja-build pkgconf \
    qt6-base-dev qt6-qpa-plugins libgl-dev \
    libopencv-dev libzmq3-dev cppzmq-dev nlohmann-json3-dev libgtest-dev
