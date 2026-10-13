#!/bin/sh
# Build the local.outputstream QML plugin and install it under $PREFIX/lib/qt6/qml
# (default PREFIX: $HOME/.local), where bin/projector-preview adds it to QML_IMPORT_PATH.
# Safe to re-run.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
cmake -S "$here/plugin" -B "$here/build" -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$here/build" --parallel
cmake --install "$here/build"
