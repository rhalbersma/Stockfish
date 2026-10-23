#!/bin/bash
# Installs the Boost headers that xstd-bits reaches into the directory given as
# the first argument, from Boost's release tarball, checked against the SHA-256
# that Boost publishes beside it. Prints nothing on success.

set -euo pipefail

dest="$1"
version=1.92.0
sha256=9bed76128d4e46755dbe818487788c6fceb6f72b378f4daa49b7e1e600d9088d
libs="assert config container container_hash describe hash2 move mp11"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

curl -fsSL --retry 3 --retry-delay 5 -o "$work/boost.tar.xz" \
  "https://github.com/boostorg/boost/releases/download/boost-$version/boost-$version-cmake.tar.xz"

if command -v sha256sum > /dev/null; then
  echo "$sha256  $work/boost.tar.xz" | sha256sum --check --quiet
else
  echo "$sha256  $work/boost.tar.xz" | shasum -a 256 --check --quiet
fi

members=""
for lib in $libs; do
  members="$members boost-$version/libs/$lib/include"
done
# shellcheck disable=SC2086
tar -xJf "$work/boost.tar.xz" -C "$work" $members

mkdir -p "$dest"
for lib in $libs; do
  cp -R "$work/boost-$version/libs/$lib/include/." "$dest/"
done
