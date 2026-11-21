#!/bin/bash
# usage: build_invader.sh <invader-0.55.0.tar.gz> <riat.tar.gz> <prefix> <out-dir>
# Builds the command line tools the converter uses, with invader-protected-names.patch applied, in <out-dir>/b.
# <prefix> holds include/squish.h and lib/libsquish.a. Needs cmake, a C++ compiler and cargo on PATH; the GUI
# editor needs Qt and is left out.
set -e
tarball=$(readlink -f "$1"); riat=$(readlink -f "$2"); prefix=$(readlink -f "$3"); out=$4
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out/src" "$out/b"; out=$(readlink -f "$out")
tar xzf "$tarball" --strip-components=1 -C "$out/src"
tar xzf "$riat" --strip-components=1 -C "$out/src/ext/riat"
( cd "$out/src" && patch -p1 < "$here/invader-protected-names.patch" )
sed -i 's/find_package(Qt6 COMPONENTS Core Widgets REQUIRED)/find_package(Qt6 COMPONENTS Core Widgets)/; s/find_package(SDL2 REQUIRED)/find_package(SDL2)/' "$out/src/dependencies.cmake"
sed -i 's#^include(src/edit/qt/qt.cmake)#\#include(src/edit/qt/qt.cmake)#' "$out/src/CMakeLists.txt"
cd "$out/b"
export CPATH=$prefix/include LIBRARY_PATH=$prefix/lib
cmake ../src -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix" "-DCMAKE_CXX_FLAGS=-include climits -include cstdint"
nice make -j8 invader-extract invader-convert invader-refactor invader-bludgeon invader-build invader-edit
