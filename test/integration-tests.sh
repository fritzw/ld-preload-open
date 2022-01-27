#!/bin/bash

set -o functrace
set -o errtrace # trap ERR in functions
set -o errexit
set -o nounset

lib="$PWD/path-mapping.so"
testdir="${TESTDIR:-/tmp/path-mapping}"

export PATH_MAPPING="$testdir/virtual:$testdir/real"

failure() {
    local lineno="$1"
    local msg="$2"
    local test_case="${FUNCNAME[1]}"
    echo
    echo "Failed  $test_case  in line $lineno at command:"
    echo "$msg"
    echo
    echo "stderr:"
    cat "out/$test_case.err"
    echo
}
trap 'failure "${LINENO}" "${BASH_COMMAND}"' ERR

setup() {
    rm -rf "$testdir/real" # clean up previous test case if present
    mkdir -p "$testdir/real"
    cd "$testdir/real"
    mkdir -p dir1/dir2
    echo content0 >file0
    echo content1 >dir1/file1
    echo content2 >dir1/dir2/file2
    echo content3 >dir1/dir2/file3
    cd "$testdir"
}

check_strace_file() {
    test_name="${FUNCNAME[1]}"
    if [[ $# == 2 ]]; then
        test_name="$1"; shift
    fi
    strace_file="$testdir/strace/$test_name"
    lines="$( grep virtual "$strace_file" | grep -vE '^execve|^write|^Mapped Path:|PATH_MAPPING: ' || true )"
    if [[ "$lines" ]] ; then
        echo "Unmapped path in $strace_file:"
        echo "$lines"
        return 1
    fi
}

check_output_file() {
    test_name="${FUNCNAME[1]}"
    if [[ $# == 2 ]]; then
        test_name="$1"; shift
    fi
    expected="$1"
    out_file="$testdir/out/$test_name"
    output="$(cat "$out_file")"
    if ! [[ "$output" == "$expected" ]]; then
        echo "ERROR: output was not as expected:"
        echo "'$output' != '$expected'"
        return 1
    fi
}

assert_readlink() {
    create_link_path="$1"
    link_content="$2"
    readlink_path="$3"
    expected="$4"
    ln -sf "$link_content" "$create_link_path"
    result="$(LD_PRELOAD="$lib" readlink -f "$readlink_path" 2>/dev/null)"
    if ! [[ "$result" == "$expected" ]]; then
        echo "assert_readlink $@:"
        echo "'$result' != '$expected'"
        return 1
    fi
}
test_readlink() {
    setup
    assert_readlink "$testdir/real/link" "$testdir/real/target" "$testdir/real/link" "$testdir/real/target" # no mapping
    assert_readlink "$testdir/real/link" "$testdir/real/target" "$testdir/virtual/link" "$testdir/real/target" # link name mapped
    assert_readlink "$testdir/real/link" "$testdir/virtual/target" "$testdir/virtual/link" "$testdir/virtual/target" # link contents not mapped
}

test_readlink_f_relative() {
    setup
    ln -s "dir2/file2" "$testdir/real/dir1/relativelink"
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        readlink -f "$testdir/virtual/dir1/relativelink" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$testdir/virtual/dir1/dir2/file2"
    test x"$(cat "$testdir/real/dir1/relativelink")" == xcontent2
}

test_readlink_f_real() {
    setup
    ln -s "$testdir/real/dir1/dir2/file2" "$testdir/real/dir1/reallink"
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        readlink -f "$testdir/virtual/dir1/reallink" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$testdir/real/dir1/dir2/file2"
}

test_readlink_f_virtual() {
    setup
    LD_PRELOAD="$lib" ln -s "$testdir/virtual/dir1/dir2/file2" "$testdir/virtual/dir1/virtlink" 2>/dev/null
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        readlink -f "$testdir/virtual/dir1/virtlink" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    #check_strace_file # False positive because link contains the word "virtual"
    check_output_file "$testdir/virtual/dir1/dir2/file2"
}

test_ln() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        ln -s "linkcontent" "$testdir/virtual/dir1/link" \
        2>out/${FUNCNAME[0]}.err
    readlink "$testdir/real/dir1/link" >out/${FUNCNAME[0]}
    check_strace_file
    check_output_file "linkcontent"
}

disabled_test_thunar() { # Disabled because slow and not really useful
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
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        Thunar "$testdir/virtual" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err &
    sleep 3; kill %1
    check_strace_file Thunar
}

test_cat() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        cat "$testdir/virtual/file0" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_output_file "content0"
    check_strace_file
}

test_find() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        find "$testdir/virtual" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$testdir/virtual
$testdir/virtual/file0
$testdir/virtual/dir1
$testdir/virtual/dir1/file1
$testdir/virtual/dir1/dir2
$testdir/virtual/dir1/dir2/file3
$testdir/virtual/dir1/dir2/file2"
}

test_grep() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        grep -R content "$testdir/virtual" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$testdir/virtual/file0:content0
$testdir/virtual/dir1/file1:content1
$testdir/virtual/dir1/dir2/file3:content3
$testdir/virtual/dir1/dir2/file2:content2"
}

test_chmod() {
    setup
    chmod 700 "$testdir/real/file0"
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        chmod 777 "$testdir/virtual/file0" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    test "$(stat -c %a "$testdir/real/file0")" == 777
}

test_utime() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        ./testtool-utime "$testdir/virtual/dir1/file1" \
        2>out/${FUNCNAME[0]}.err
    chmod 700 real/dir1/file1
    stat -c %X:%Y "real/dir1/file1" >out/${FUNCNAME[0]}
    check_strace_file
    check_output_file '200000000:100000000'
}

test_rm() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        rm -r "$testdir/virtual/dir1" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    test '!' -e "$testdir/real/dir1"
}

test_rename() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        /usr/bin/mv "$testdir/virtual/dir1" "$testdir/virtual/dir1_renamed" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    test '!' -e "$testdir/real/dir1"
    test -e "$testdir/real/dir1_renamed"
}

test_bash_exec() {
    setup
    cp /usr/bin/echo "$testdir/real/dir1/"
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        bash -c "'$testdir/virtual/dir1/echo' arg1 arg2 arg3 arg4 arg5" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "arg1 arg2 arg3 arg4 arg5"
}

test_bash_cd() { # Test chdir()
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        bash -c "cd virtual; ls; cd dir1; ls" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file $'dir1\nfile0\ndir2\nfile1'
}

test_execl_0() {
    setup
    cp ./testtool-execl ./testtool-printenv real/
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        "$testdir/virtual/testtool-execl" execl "$testdir/virtual/testtool-printenv" 0 \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file $'TEST0=value0'
}

test_execl_1() {
    setup
    cp ./testtool-execl ./testtool-printenv real/
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        "$testdir/virtual/testtool-execl" execl "$testdir/virtual/testtool-printenv" 1 \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file $'arg1\nTEST0=value0'
}

test_execlp_2() {
    setup
    cp ./testtool-execl ./testtool-printenv real/
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        "$testdir/virtual/testtool-execl" execlp "$testdir/virtual/testtool-printenv" 2 \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file $'arg1\narg2\nTEST0=value0'
}

test_execle_3() {
    setup
    cp ./testtool-execl ./testtool-printenv real/
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        "$testdir/virtual/testtool-execl" execle "$testdir/virtual/testtool-printenv" 3 \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file $'arg1\narg2\narg3\nTEST1=value1\nTEST2=value2'

}

test_du() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        du "$testdir/virtual/" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "8	$testdir/virtual/dir1/dir2
12	$testdir/virtual/dir1
16	$testdir/virtual/"
}

test_df() { # Tests realpath()
    setup
    expected="$(df --output="source,fstype,itotal,size,target" "$testdir/real/")"
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        df --output="source,fstype,itotal,size,target" "$testdir/virtual/" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$expected"
}

test_getfacl() { # Tests getxattr()
    setup
    expected="$(getfacl "$testdir/real/" 2>/dev/null | sed 's/real/virtual/')"
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        getfacl "$testdir/virtual/" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$expected"
}

test_mkfifo() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        mkfifo "$testdir/virtual/dir1/fifo" \
        2>out/${FUNCNAME[0]}.err
    stat -c %F real/dir1/fifo >out/${FUNCNAME[0]}
    check_strace_file
    check_output_file "fifo"
}

test_mkdir() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        mkdir "$testdir/virtual/dir1/newdir" \
        2>out/${FUNCNAME[0]}.err
    stat -c %F real/dir1/newdir >out/${FUNCNAME[0]}
    check_strace_file
    check_output_file "directory"
}

test_ftw() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        ./testtool-ftw "$testdir/virtual/dir1" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$testdir/real/dir1
$testdir/real/dir1/file1
$testdir/real/dir1/dir2
$testdir/real/dir1/dir2/file3
$testdir/real/dir1/dir2/file2"
}

test_nftw() {
    setup
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        ./testtool-nftw "$testdir/virtual/dir1" \
        >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file "$testdir/real/dir1
$testdir/real/dir1/file1
$testdir/real/dir1/dir2
$testdir/real/dir1/dir2/file3
$testdir/real/dir1/dir2/file2"
}

test_fts() {
    setup
    mkdir -p real/dir1/dir4
    echo content4 > real/dir1/dir4/file4
    LD_PRELOAD="$lib" strace -o "strace/${FUNCNAME[0]}" \
        ./testtool-fts "$testdir/virtual/dir1/dir2" "$testdir/virtual/dir1/dir4" >out/${FUNCNAME[0]} 2>out/${FUNCNAME[0]}.err
    check_strace_file
    check_output_file $'dir2\nfile2\nfile3\ndir2\ndir4\nfile4\ndir4'
}

# Setup up output directories for the test cases
mkdir -p "$testdir/out"
mkdir -p "$testdir/strace"

# Find all declared functions starting with "test_" in a random order
testcases="$(declare -F | cut -d " " -f 3- | grep '^test_' | shuf)"
num_testcases="$(echo "$testcases" | wc -l)"

N=0
if [[ $# -gt 0 ]]; then
    while [[ $# -gt 0 ]]; do
        if [[ "$testcases" =~ "$1" ]]; then
            echo "$1"
            $1
            shift
        else
            echo "Unknown test case $1"
            exit 1
            N=$[N+1]
        fi
    done
else
    # If no argument is given, execute all test cases
    for cmd in $testcases; do
        echo "$cmd"
        $cmd
        N=$[N+1]
    done
fi

echo "$N/$num_testcases TESTS PASSED!"
#rm -rf "$testdir"   # use make clean to clean up instead
