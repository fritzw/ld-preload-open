#!/bin/bash

set -o functrace
set -o errtrace # trap ERR in functions
set -o errexit
set -o nounset

lib="$PWD/path-mapping.so"
tempdir=/tmp/path-mapping
testdir="$tempdir"/tests

mkdir -p "$tempdir/out" "$tempdir/strace"

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
    test '!' -e "$testdir/real/dir1"
    teardown
}

test_rename() {
    setup
    LD_PRELOAD="$lib" strace /usr/bin/mv "$testdir/virtual/dir1" "$testdir/virtual/dir1_renamed" 2>../rename.strace
    check_strace ../rename.strace
    test '!' -e "$testdir/real/dir1"
    test -e "$testdir/real/dir1_renamed"
    teardown
}

test_bash_exec() {
    setup
    cp /usr/bin/echo "$testdir/real/dir1/"
    LD_PRELOAD="$lib" strace bash -c "'$testdir/virtual/dir1/echo' arg1 arg2 arg3 arg4 arg5" >../bash_exec.out 2>../bash_exec.strace
    check_strace ../bash_exec.strace
    test x"$(cat ../bash_exec.out)" == x"arg1 arg2 arg3 arg4 arg5"
    teardown
}

test_execl() {
    setup
    cp ../testtool-execl ../testtool-printenv real/
    LD_PRELOAD="$lib" strace "$testdir/virtual/testtool-execl" execl "$testdir/virtual/testtool-printenv" 0 >../execl0.out 2>../execl0.strace
    check_strace ../execl0.strace
    test x"$(cat ../execl0.out)" == $'xTEST0=value0'
    LD_PRELOAD="$lib" strace "$testdir/virtual/testtool-execl" execl "$testdir/virtual/testtool-printenv" 1 >../execl1.out 2>../execl1.strace
    check_strace ../execl1.strace
    test x"$(cat ../execl1.out)" == $'xarg1\nTEST0=value0'
    LD_PRELOAD="$lib" strace "$testdir/virtual/testtool-execl" execlp "$testdir/virtual/testtool-printenv" 2 >../execlp2.out 2>../execlp2.strace
    check_strace ../execlp2.strace
    test x"$(cat ../execlp2.out)" == $'xarg1\narg2\nTEST0=value0'
    LD_PRELOAD="$lib" strace "$testdir/virtual/testtool-execl" execle "$testdir/virtual/testtool-printenv" 3 >../execle3.out 2>../execle3.strace
    check_strace ../execle3.strace
    test x"$(cat ../execle3.out)" == $'xarg1\narg2\narg3\nTEST1=value1\nTEST2=value2'
    teardown
}

CFLAGS='-D QUIET' make clean all || true

test_cat
test_rm
test_find
test_grep
test_chmod
test_bash_exec
test_execl
test_rename
test_readlink
#test_thunar

echo "ALL TESTS PASSED!"
#rm -rf "$tempdir"
