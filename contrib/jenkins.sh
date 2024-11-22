#!/usr/bin/env bash
# jenkins build helper: this is how we build on jenkins.osmocom.org

if ! [ -x "$(command -v osmo-build-dep.sh)" ]; then
	echo "Error: We need to have scripts/osmo-deps.sh from http://git.osmocom.org/osmo-ci/ in PATH !"
	exit 2
fi


set -ex

base="$PWD"
deps="$base/deps"
inst="$deps/install"
export deps inst

osmo-clean-workspace.sh

mkdir "$deps" || true

osmo-build-dep.sh libosmocore "" '--disable-doxygen --enable-gnutls'

verify_value_string_arrays_are_terminated.py $(find . -name "*.[hc]")

export PKG_CONFIG_PATH="$inst/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$inst/lib"

osmo-build-dep.sh libosmo-netif "" '--disable-doxygen'
osmo-build-dep.sh libosmo-abis

set +x
echo
echo
echo
echo " =============================== osmo-sysmon ============================"
echo
set -x

cd "$base"
autoreconf --install --force
./configure --enable-sanitize --enable-external-tests --enable-werror
$MAKE $PARALLEL_MAKE

LD_LIBRARY_PATH="$inst/lib" \
  DISTCHECK_CONFIGURE_FLAGS="--enable-vty-tests --enable-external-tests --enable-werror" \
  $MAKE distcheck \
  || cat-testlogs.sh

$MAKE maintainer-clean
osmo-clean-workspace.sh
