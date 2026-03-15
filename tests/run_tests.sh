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

# Test 11: Remove files from index
echo "Test 11: Remove files from index"
echo "remove me" > removable.txt
$FIT add removable.txt
grep -q removable.txt .fit/index || { echo "FAIL: file not in index after add"; exit 1; }
$FIT rm removable.txt
! grep -q removable.txt .fit/index || { echo "FAIL: file still in index after rm"; exit 1; }
echo "PASS"

# Test 12: Branch delete
echo "Test 12: Delete branch"
$FIT branch to-delete
$FIT branch | grep -q to-delete || { echo "FAIL: branch not created"; exit 1; }
$FIT branch -d to-delete
! $FIT branch | grep -q to-delete || { echo "FAIL: branch not deleted"; exit 1; }
echo "PASS"

# Test 13: Prevent deleting current branch
echo "Test 13: Prevent deleting current branch"
$FIT branch -d main 2>&1 | grep -q "Cannot delete" || { echo "FAIL: should prevent deleting current branch"; exit 1; }
echo "PASS"

# Test 14: Version command
echo "Test 14: Version command"
$FIT version | grep -q "fit version" || { echo "FAIL: version not showing"; exit 1; }
$FIT --version | grep -q "fit version" || { echo "FAIL: --version not working"; exit 1; }
$FIT -v | grep -q "fit version" || { echo "FAIL: -v not working"; exit 1; }
echo "PASS"

# Test 15: Log --oneline
echo "Test 15: Log --oneline"
ONELINE=$($FIT log --oneline | head -1)
[ -n "$ONELINE" ] || { echo "FAIL: oneline log empty"; exit 1; }
# Oneline should be a single line per commit, not multi-line
LINES=$($FIT log --oneline | wc -l)
NORMAL_LINES=$($FIT log | wc -l)
[ "$LINES" -lt "$NORMAL_LINES" ] || { echo "FAIL: oneline should be shorter than full log"; exit 1; }
echo "PASS"

# Test 16: Log -n limit
echo "Test 16: Log -n limit"
LINES=$($FIT log --oneline -n 1 | wc -l)
[ "$LINES" -eq 1 ] || { echo "FAIL: expected 1 line, got $LINES"; exit 1; }
echo "PASS"

# Test 17: Show command
echo "Test 17: Show command"
$FIT show | grep -q "commit" || { echo "FAIL: show not displaying commit"; exit 1; }
$FIT show | grep -q "Author:" || { echo "FAIL: show not displaying author"; exit 1; }
$FIT show | grep -q "Tree contents:" || { echo "FAIL: show not displaying tree"; exit 1; }
echo "PASS"

# Test 18: Show by branch name
echo "Test 18: Show by branch name"
$FIT show main | grep -q "commit" || { echo "FAIL: show by branch name not working"; exit 1; }
echo "PASS"

# Test 19: Diff with single commit (compare with HEAD)
echo "Test 19: Diff with single commit"
# Get an older commit hash
OLDER_HASH=$($FIT log | grep "^commit " | tail -1 | awk '{print $2}')
$FIT diff "$OLDER_HASH" 2>&1 | grep -q "Comparing commits" || { echo "FAIL: single-arg diff not working"; exit 1; }
echo "PASS"

echo ""
echo "=== All tests passed ==="
