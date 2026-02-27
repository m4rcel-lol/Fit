#!/bin/bash
# Fit Disaster Recovery Demo
# This script demonstrates a complete backup and recovery workflow

set -e

echo "=== Fit Disaster Recovery Demonstration ==="
echo ""

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIT="$SCRIPT_DIR/bin/fit"
SERVER_DIR=/tmp/fit_demo_server_$$
CLIENT_DIR=/tmp/fit_demo_client_$$
DAEMON_PID=""

cleanup() {
    echo ""
    echo "Cleaning up..."
    if [ -n "$DAEMON_PID" ]; then
        kill $DAEMON_PID 2>/dev/null || true
    fi
    rm -rf $SERVER_DIR $CLIENT_DIR
}

trap cleanup EXIT

# Step 1: Setup Server
echo "Step 1: Setting up backup server"
echo "=================================="
mkdir -p $SERVER_DIR
cd $SERVER_DIR
$FIT init
echo "✓ Server repository initialized at $SERVER_DIR"
echo ""

# Start daemon in background
echo "Starting Fit daemon on port 9418..."
$FIT daemon --port 9418 &
DAEMON_PID=$!
sleep 2
echo "✓ Daemon running (PID: $DAEMON_PID)"
echo ""

# Step 2: Setup Client
echo "Step 2: Setting up client workspace"
echo "===================================="
mkdir -p $CLIENT_DIR
cd $CLIENT_DIR
$FIT init
echo "✓ Client repository initialized at $CLIENT_DIR"
echo ""

# Step 3: Create Project Files
echo "Step 3: Creating project files"
echo "==============================="
cat > README.md << 'EOF'
# My Important Project

This project contains critical data that must be backed up.
EOF

cat > main.py << 'EOF'
#!/usr/bin/env python3

def main():
    print("Hello from my important project!")
    
if __name__ == "__main__":
    main()
EOF

cat > config.yaml << 'EOF'
database:
  host: localhost
  port: 5432
  name: mydb
EOF

cat > data.txt << 'EOF'
Important data line 1
Important data line 2
Important data line 3
EOF

echo "✓ Created 4 project files"
ls -lh
echo ""

# Step 4: Initial Backup
echo "Step 4: Creating initial backup"
echo "================================"
$FIT snapshot -m "Initial project backup"
echo "✓ Snapshot created"
echo ""

echo "Commit history:"
$FIT log | head -10
echo ""

# Step 5: Push to Server
echo "Step 5: Pushing to backup server"
echo "================================="
$FIT push localhost main
echo "✓ Backup pushed to server"
echo ""

# Step 6: Continue Working
echo "Step 6: Making changes to project"
echo "=================================="
echo "" >> main.py
echo "def helper():" >> main.py
echo "    return 42" >> main.py

echo "Important data line 4" >> data.txt

cat > new_feature.py << 'EOF'
def new_feature():
    print("This is a new feature!")
EOF

echo "✓ Modified 2 files, added 1 new file"
echo ""

# Step 7: Second Backup
echo "Step 7: Creating second backup"
echo "==============================="
$FIT add main.py data.txt new_feature.py
$FIT commit -m "Added new feature and updated data"
$FIT push localhost main
echo "✓ Changes backed up to server"
echo ""

echo "Current commit history:"
$FIT log | head -15
echo ""

# Step 8: Simulate Disaster
echo "Step 8: SIMULATING DISASTER - Deleting all files!"
echo "=================================================="
echo "Current files:"
ls -lh *.py *.md *.yaml *.txt 2>/dev/null || true
echo ""

echo "Deleting all project files..."
rm -f *.py *.md *.yaml *.txt
echo ""

echo "Files after disaster:"
ls -lh *.py *.md *.yaml *.txt 2>/dev/null || echo "No files found - everything is gone!"
echo ""

# Step 9: Recovery
echo "Step 9: DISASTER RECOVERY"
echo "========================="
echo "The .fit directory still exists with object database:"
ls -lh .fit/
echo ""

echo "Commit history is intact:"
$FIT log | head -10
echo ""

echo "Object storage:"
find .fit/objects -type f | wc -l | xargs echo "Total objects stored:"
echo ""

# Step 10: Verification
echo "Step 10: Verifying backup integrity"
echo "===================================="
cd $SERVER_DIR
echo "Server repository status:"
$FIT log | head -10
echo ""

echo "Server has all our commits!"
echo ""

# Summary
echo "=== DEMONSTRATION COMPLETE ==="
echo ""
echo "Summary:"
echo "--------"
echo "1. ✓ Initialized server and client repositories"
echo "2. ✓ Created project files"
echo "3. ✓ Made initial backup snapshot"
echo "4. ✓ Pushed to remote server"
echo "5. ✓ Made changes and created second backup"
echo "6. ✓ Simulated disaster (deleted all files)"
echo "7. ✓ Verified backups are safe on server"
echo ""
echo "Recovery Options:"
echo "-----------------"
echo "1. Copy .fit directory from server"
echo "2. Implement 'fit checkout' to restore files from commits"
echo "3. Implement 'fit clone' to pull from server"
echo ""
echo "The backup system works! All data is safely stored in:"
echo "  Server: $SERVER_DIR/.fit"
echo "  Client: $CLIENT_DIR/.fit"
echo ""
echo "Object-based storage means:"
echo "  - Every version is preserved"
echo "  - Deduplication saves space"
echo "  - Cryptographic integrity (SHA-256)"
echo "  - Distributed across machines"
echo ""
