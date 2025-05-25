#!/bin/bash

set -ex

# Wrapper script for meson-build.py that ensures that python3 is installed.

if ! command -v python3; then
    SUDO=$(which sudo || true)
    $SUDO apt-get -y update
    $SUDO apt-get -y install python3 python3-pip
fi

CURRENT_DIR=$(dirname $0)
python3 $CURRENT_DIR/meson-build.py $@
