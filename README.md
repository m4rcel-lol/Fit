# Fit - Filesystem Inside Terminal

**Fit** is a production-grade distributed version control and backup system inspired by Git's internal architecture, written in C from scratch. It functions as both a version control system and a distributed personal backup solution between Linux machines.

## Architecture

Fit reimplements Git's core concepts:

- **Content-addressable storage** using SHA-256 hashing
- **Object model**: blobs (files), trees (directories), commits
- **Compression**: zlib compression for all objects
- **Packfiles**: efficient object transfer format
- **References**: branches and HEAD management
- **Garbage collection**: removes unreachable objects
- **Network protocol**: custom TCP-based object transfer

### Key Differences from Git

1. **SHA-256 by default** (Git uses SHA-1, transitioning to SHA-256)
2. **Simplified protocol** - custom lightweight TCP protocol with smart negotiation
3. **Backup-first design** - includes `snapshot` command for quick full-directory backups
4. **Minimal dependencies** - only zlib and OpenSSL
5. **No delta compression yet** - stores full objects (stretch goal)

## Object Model

### Storage Structure

```
.fit/
├── objects/
│   └── ab/
│       └── cdef123...  (compressed object)
├── refs/
│   └── heads/
│       └── main
├── HEAD
├── index
└── config
```

### Object Types

1. **Blob**: Raw file content
2. **Tree**: Directory listing with modes, names, and hashes
3. **Commit**: Points to tree, parent commit, author, timestamp, message

### Object Format

```
<type> <size>\0<data>
```

Compressed with zlib and stored at `.fit/objects/<first-2-hex>/<remaining-hex>`.

## Network Protocol

Extensible TCP protocol on port 9418 with version negotiation:

### Protocol Version 1 (Legacy)
```
[VERSION:1][COMMAND:1][PACKFILE_DATA]
```

### Protocol Version 2+ (With Negotiation)
```
[VERSION:2][CMD_NEGOTIATE:3]
[CLIENT_MIN_VER:1][CLIENT_MAX_VER:1][CLIENT_CAPS:4]
[SERVER_MIN_VER:1][SERVER_MAX_VER:1][SERVER_CAPS:4]
[COMMAND:1][PACKFILE_DATA]
```

Commands:
- `CMD_SEND_OBJECTS (1)`: Send packfile to server
- `CMD_REQUEST_OBJECTS (2)`: Request objects from server
- `CMD_NEGOTIATE (3)`: Initiate protocol negotiation

Capabilities:
- `CAP_MULTI_THREADED`: Server supports concurrent connections
- `CAP_COMPRESSION`: Enhanced compression support
- `CAP_STREAMING`: Optimized streaming transfer

The protocol automatically negotiates the highest common version and capability set between client and server, with automatic fallback to legacy protocol v1 for backward compatibility.

## Installation

### Arch Linux

```bash
# Install dependencies
sudo pacman -S gcc make zlib openssl

# Build
make

# Install
sudo make install
```

### Alpine Linux

```bash
# Install dependencies
apk add gcc musl-dev make zlib-dev openssl-dev

# Build
make

# Install
make install
```

## Usage

### Basic Workflow

```bash
# Initialize repository
fit init

# Add files
fit add file1.txt file2.txt

# Commit changes
fit commit -m "Initial commit"

# View history
fit log

# Check status
fit status

# Compare commits
fit diff <commit1> <commit2>
```

### Comparing Changes

```bash
# View differences between two commits
fit diff abc123 def456

# Compare a commit with HEAD
fit diff abc123

# See what changed in recent commits
fit log  # Get commit hashes
fit diff <older-hash> <newer-hash>
```

### Branching

```bash
# List branches
fit branch

# Create branch
fit branch feature

# Switch branch (restores files)
fit checkout feature

# Merge branch
fit merge feature
```

### Tag Management

```bash
# List tags
fit tag

# Create tag
fit tag v1.0.0

# Create tag with message
fit tag create v1.0.0 -m "Release version 1.0.0"

# Delete tag
fit tag delete v1.0.0
```

### Remote Management

```bash
# List remotes
fit remote

# Add remote
fit remote add origin server.local

# Remove remote
fit remote remove origin
```

### Stash

```bash
# Save uncommitted changes
fit stash

# Save with message
fit stash save -m "Work in progress"

# List stashes
fit stash list

# Apply and remove stash
fit stash pop

# Remove specific stash
fit stash drop stash@1234567890
```

### Backup Mode

```bash
# Create snapshot of entire directory
fit snapshot -m "Backup before system upgrade"

# View snapshots
fit log
```

### Remote Operations

```bash
# Start daemon on server
fit daemon --port 9418

# Push from client
fit push server.local main

# Pull from server
fit pull server.local main

# Clone repository
fit clone server.local main ~/myproject
```

### Disaster Recovery

```bash
# Restore files from a specific commit
fit restore <commit-hash>

# Or checkout a branch (restores files automatically)
fit checkout main
```

### Maintenance

```bash
# Run garbage collection
fit gc
```

## Docker Deployment

Run the Fit daemon 24/7 with Docker:

```bash
# Build and start server
docker-compose up -d

# View logs
docker-compose logs -f

# Stop server
docker-compose down
```

**Auto-start on boot (systemd):**

```bash
# Install as system service
chmod +x install_service.sh
./install_service.sh

# Check status
sudo systemctl status fit-server

# View logs
docker-compose logs -f
```

This will:
- Start Fit daemon on port 9418
- Auto-restart on failure
- Auto-start on system boot

Access:
- Daemon: `fit push localhost main`

## Docker Deployment (Alt Method)

### Build and Run

```bash
# Build image
docker build -t fit:latest .

# Run daemon
docker run -d -p 9418:9418 -v fit-data:/data/.fit fit:latest

# Or use docker-compose
docker-compose up -d
```

### Auto-start on Boot

The `docker-compose.yml` includes `restart: unless-stopped` for automatic startup.

For systemd on bare metal:

```bash
# Create /etc/systemd/system/fit-daemon.service
[Unit]
Description=Fit Daemon
After=network.target

[Service]
Type=simple
User=fit
ExecStart=/usr/local/bin/fit daemon --port 9418
Restart=always

[Install]
WantedBy=multi-target.target

# Enable and start
sudo systemctl enable fit-daemon
sudo systemctl start fit-daemon
```

## Two-Node Setup Example

### Server (Alpine Linux - Laptop)

```bash
# Install Fit
make && sudo make install

# Initialize repository
mkdir -p ~/backup
cd ~/backup
fit init

# Start daemon
fit daemon --port 9418

# Or with Docker
docker-compose up -d
```

### Client (Arch Linux - Main PC)

```bash
# Install Fit
make && sudo make install

# Initialize repository
cd ~/projects/myproject
fit init

# Create initial snapshot
fit snapshot -m "Initial backup"

# Push to laptop
fit push laptop.local main

# Continue working...
echo "new content" > file.txt
fit add file.txt
fit commit -m "Updated file"
fit push laptop.local main
```

### Disaster Recovery

```bash
# On new machine or after data loss
fit init
fit pull laptop.local main  # (requires implementing pull)

# Or manually:
# 1. Copy .fit directory from backup server
# 2. Checkout desired commit
```

## Testing

```bash
# Run test suite
make test
```

### Manual Testing

```bash
# Test hashing
echo "test" > test.txt
fit init
fit add test.txt
fit commit -m "test"
fit log

# Test network transfer
# Terminal 1:
fit daemon --port 9418

# Terminal 2:
fit push localhost main
```

## Performance Considerations

1. **Object storage**: Each object stored separately until packed
2. **Compression**: zlib level 6 (default) balances speed/size
3. **Network**: Streams packfiles, no chunking yet
4. **Memory**: Loads full objects into memory (optimize for large files later)
5. **Multi-threading**: Daemon now supports concurrent client connections via pthreads

## Limitations & Future Work

### Current Limitations

- No delta compression (stores full objects)
- No sparse checkout
- ~~No complex merge algorithm (fast-forward only)~~ **Three-way merge implemented**
- No encryption (transport or storage)
- No authentication
- ~~Single-threaded daemon~~ **Multi-threaded daemon implemented**
- No index v2 format (simple text format)

### Stretch Goals

- [ ] Delta compression for packfiles
- [ ] End-to-end encryption
- [ ] Signed commits (GPG)
- [ ] File chunking for large files
- [x] Multi-threaded daemon - **✓ Implemented**
- [x] Smart protocol negotiation - **✓ Implemented**
- [ ] Shallow clones
- [ ] Submodule support
- [x] Three-way merge algorithm - **✓ Implemented**

## Security Notes

- **No authentication**: Daemon accepts all connections
- **No encryption**: Data sent in plaintext
- **No signing**: Commits not cryptographically signed

For production use, run behind VPN or SSH tunnel:

```bash
# SSH tunnel example
ssh -L 9418:localhost:9418 laptop.local

# Then push to localhost
fit push localhost main
```

## Troubleshooting

### "Failed to add file"
- Check file exists and is readable
- Ensure `.fit` directory initialized

### "Push failed"
- Verify daemon running: `netstat -ln | grep 9418`
- Check firewall rules
- Test connectivity: `telnet server.local 9418`

### "No commits yet"
- Create initial commit before branching/pushing

## Development

### Project Structure

```
src/
├── main.c      - CLI interface
├── hash.c      - SHA-256 hashing
├── object.c    - Object storage
├── tree.c      - Tree objects
├── commit.c    - Commit objects
├── index.c     - Staging area
├── refs.c      - Reference management
├── pack.c      - Packfile format
├── network.c   - Network protocol
├── gc.c        - Garbage collection
└── util.c      - Utilities

include/
└── fit.h       - Public API

tests/
└── run_tests.sh
```

### Contributing

This is a personal project demonstrating Git internals. Feel free to fork and extend.

## License

MIT License - See LICENSE file

## Credits

Inspired by:
- Git by Linus Torvalds
- "Git from the Bottom Up" by John Wiegley
- "Building Git" by James Coglan

Built from scratch without copying Git source code.

---

**Fit** - Because your filesystem belongs inside your terminal.
