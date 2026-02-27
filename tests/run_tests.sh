#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIT="$SCRIPT_DIR/../bin/fit"
TEST_DIR=/tmp/fit_test_$$

cleanup() {
    rm -rf $TEST_DIR
}

trap cleanup EXIT

echo "=== Fit Test Suite ==="

# Test 1: Init
echo "Test 1: Initialize repository"
mkdir -p $TEST_DIR
cd $TEST_DIR
$FIT init
[ -d .fit ] || { echo "FAIL: .fit directory not created"; exit 1; }
[ -f .fit/HEAD ] || { echo "FAIL: HEAD not created"; exit 1; }
echo "PASS"

# Test 2: Add files
echo "Test 2: Add files"
echo "test content" > test.txt
$FIT add test.txt
grep -q test.txt .fit/index || { echo "FAIL: file not in index"; exit 1; }
echo "PASS"

# Test 3: Commit
echo "Test 3: Create commit"
$FIT commit -m "Initial commit"
$FIT log | grep -q "Initial commit" || { echo "FAIL: commit not found"; exit 1; }
echo "PASS"

# Test 4: Branch
echo "Test 4: Create branch"
$FIT branch feature
$FIT branch | grep -q feature || { echo "FAIL: branch not created"; exit 1; }
echo "PASS"

# Test 5: Checkout
echo "Test 5: Checkout branch"
$FIT checkout feature
grep -q "refs/heads/feature" .fit/HEAD || { echo "FAIL: HEAD not updated"; exit 1; }
echo "PASS"

# Test 6: Multiple commits
echo "Test 6: Multiple commits"
echo "second" > file2.txt
$FIT add file2.txt
$FIT commit -m "Second commit"
$FIT log | grep -q "Second commit" || { echo "FAIL: second commit not found"; exit 1; }
echo "PASS"

# Test 7: Status
echo "Test 7: Status"
echo "third" > file3.txt
$FIT add file3.txt
$FIT status | grep -q file3.txt || { echo "FAIL: status not showing staged file"; exit 1; }
echo "PASS"

# Test 8: Snapshot
echo "Test 8: Snapshot"
$FIT checkout main
echo "snapshot test" > snap.txt
$FIT snapshot -m "Test snapshot"
$FIT log | grep -q "Test snapshot" || { echo "FAIL: snapshot not created"; exit 1; }
echo "PASS"

# Test 9: Object integrity
echo "Test 9: Object integrity"
OBJECTS=$(find .fit/objects -type f | wc -l)
[ $OBJECTS -gt 0 ] || { echo "FAIL: no objects created"; exit 1; }
echo "PASS: $OBJECTS objects stored"

# Test 10: GC
echo "Test 10: Garbage collection"
$FIT gc
echo "PASS"

echo ""
echo "=== All tests passed ==="
