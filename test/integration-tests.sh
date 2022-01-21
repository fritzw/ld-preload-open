#!/bin/bash

set -o functrace
set -o errtrace # trap ERR in functions
set -o errexit
set -o nounset

lib="$PWD/path-mapping.so"
tempdir=/tmp/path-mapping
testdir="$tempdir"/tests

failure() {
  local lineno=$1
  local msg=$2
  echo "Failed at $lineno: $msg"
}
trap 'failure ${LINENO} "$BASH_COMMAND"' ERR

setup() {
    mkdir -p "$testdir/real"; cd "$testdir/real"
    mkdir -p dir1/dir2
    echo content0 >file0
    echo content1 >dir1/file1
    echo content2 >dir1/dir2/file2
    echo content3 >dir1/dir2/file3
    cd "$testdir"
}
teardown() {
    cd "$tempdir"
    rm -rf "$testdir"
}

check_strace() {
    strace_file="$1"
    lines="$( grep virtual "$strace_file" | grep -vE '^execve|^write' || true )"
    if [[ "$lines" ]] ; then
        echo "Unmapped path in $strace_file:"
        echo "$lines"
    fi
}

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
    setup
    assert_readlink "$testdir/real/link" "$testdir/real/target" "$testdir/real/link" "$testdir/real/target" # no mapping
    assert_readlink "$testdir/real/link" "$testdir/real/target" "$testdir/virtual/link" "$testdir/real/target" # link name mapped
    assert_readlink "$testdir/real/link" "$testdir/virtual/target" "$testdir/virtual/link" "$testdir/virtual/target" # link contents not mapped
    teardown
}

test_thunar() {
    if ! which Thunar >/dev/null; then
        echo "Thunar not found, skipping test case"
        return
    fi
    if pgrep Thunar; then
        echo "Thunar is running. Execute Thunar -q if you want to run it."
        return
    fi
    setup
    cd real
    touch file1  dir1/file2  dir1/dir2/file3  dir1/dir2/file4
    LD_PRELOAD="$lib" strace Thunar "$testdir/virtual" 2> ../thunar.strace &
    sleep 3; kill %1
    check_strace ../thunar.strace
    teardown
}

test_cat() {
    setup
    LD_PRELOAD="$lib" strace cat "$testdir/virtual/file0" >../cat.out 2>../cat.strace
    test x"$(cat ../cat.out)" == "xcontent0"
    check_strace ../cat.strace
    teardown
}

test_find() {
    setup
    LD_PRELOAD="$lib" strace find "$testdir/virtual" >../find.out 2>../find.strace
    check_strace ../find.strace
    test x"$(cat ../find.out)" == x"/tmp/path-mapping/tests/virtual
/tmp/path-mapping/tests/virtual/file0
/tmp/path-mapping/tests/virtual/dir1
/tmp/path-mapping/tests/virtual/dir1/file1
/tmp/path-mapping/tests/virtual/dir1/dir2
/tmp/path-mapping/tests/virtual/dir1/dir2/file3
/tmp/path-mapping/tests/virtual/dir1/dir2/file2"
    teardown
}

test_grep() {
    setup
    LD_PRELOAD="$lib" strace grep -R content "$testdir/virtual" >../grep.out 2>../grep.strace
    check_strace ../grep.strace
    test x"$(cat ../grep.out)" == x"/tmp/path-mapping/tests/virtual/file0:content0
/tmp/path-mapping/tests/virtual/dir1/file1:content1
/tmp/path-mapping/tests/virtual/dir1/dir2/file3:content3
/tmp/path-mapping/tests/virtual/dir1/dir2/file2:content2"
    teardown
}

test_chmod() {
    setup
    chmod 700 "$testdir/real/file0"
    LD_PRELOAD="$lib" strace chmod 777 "$testdir/virtual/file0" >../chmod.out 2>../chmod.strace
    check_strace ../chmod.strace
    test "$(stat -c %a "$testdir/real/file0")" == 777
    teardown
}

test_rm() {
    setup
    LD_PRELOAD="$lib" strace rm -r "$testdir/virtual/dir1" 2>../rm.strace
    check_strace ../rm.strace
    mkdir "$testdir/real/dir1" # Fails if rm did not remove dir1
    teardown
}

CFLAGS='-D QUIET' make clean all || true

test_cat
test_rm
test_find
test_grep
test_chmod
test_readlink
test_thunar

echo "ALL TESTS PASSED!"
rm -rf "$tempdir"
