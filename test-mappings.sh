#!/bin/bash

set -o errexit
set -o nounset

lib="$PWD/path-mapping.so"
tempdir=/tmp/path-mapping
testdir="$tempdir"/tests

assert_readlink() {
    create_link_path="$1"
    link_content="$2"
    readlink_path="$3"
    expected="$4"
    ln -sf "$link_content" "$create_link_path"
    result="$(LD_PRELOAD="$lib" readlink -f "$readlink_path")"
    if ! [[ "$result" == "$expected" ]]; then
        echo "assert_readlink $@:"
        echo "'$result' != '$expected'"
        false
    fi
}
test_readlink() {
    assert_readlink "$testdir/real/link" "$testdir/real/target" "$testdir/real/link" "$testdir/real/target" # no mapping
    assert_readlink "$testdir/real/link" "$testdir/real/target" "$testdir/virtual/link" "$testdir/real/target" # link name mapped
    assert_readlink "$testdir/real/link" "$testdir/virtual/target" "$testdir/virtual/link" "$testdir/virtual/target" # link contents not mapped
}

mkdir -p "$testdir/real"; cd "$testdir"
test_readlink
rm -rf "$tempdir"
