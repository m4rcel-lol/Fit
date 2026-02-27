# Fit - Quick Start

## What is Fit?

**Fit** (Filesystem Inside Terminal) is a production-grade distributed version control and backup system written in C from scratch, inspired by Git's internal architecture.

Use it for:
- Version control (like Git)
- Distributed backups between machines
- Disaster recovery
- File synchronization

---

## Installation

### Arch Linux

```bash
sudo pacman -S gcc make zlib openssl
git clone <repository-url>
cd Fit
make
sudo make install
```

### Alpine Linux

```bash
apk add gcc musl-dev make zlib-dev openssl-dev
git clone <repository-url>
cd Fit
make
make install
```

---

## Basic Usage

```bash
# Initialize repository
fit init

# Add files
fit add file1.txt file2.txt

# Commit
fit commit -m "Initial commit"

# View history
fit log

# Create branch
fit branch feature

# Switch branch
fit checkout feature

# Quick snapshot (backup all files)
fit snapshot -m "Backup before upgrade"
```

---

## Remote Backup

### On Server (Backup Storage)

```bash
# Start daemon
fit daemon --port 9418

# Or with Docker
docker-compose up -d
```

### On Client (Work Machine)

```bash
# Push to server
fit push server.local main
```

---

## Commands

| Command | Description |
|---------|-------------|
| `fit init` | Initialize repository |
| `fit add <files>` | Stage files for commit |
| `fit commit -m <msg>` | Create commit |
| `fit log` | Show commit history |
| `fit status` | Show repository status |
| `fit branch [name]` | List or create branches |
| `fit checkout <branch>` | Switch branches |
| `fit snapshot -m <msg>` | Quick backup of all files |
| `fit push <host> <branch>` | Push to remote |
| `fit daemon --port <port>` | Start server daemon |
| `fit gc` | Garbage collection |

---

## Architecture Highlights

- **SHA-256 hashing** for content addressing
- **zlib compression** for all objects
- **Packfile format** for efficient network transfer
- **Custom TCP protocol** on port 9418
- **Mark-and-sweep GC** for cleanup

---

## Documentation

- **README.md** - Overview and features
- **ARCHITECTURE.md** - Detailed design documentation
- **SETUP_GUIDE.md** - Two-node setup walkthrough

---

## Testing

```bash
make test
```

All tests should pass:
- Repository initialization
- File staging and commits
- Branching and checkout
- Snapshots
- Object integrity
- Garbage collection

---

## Project Structure

```
Fit/
├── src/           # Source code
│   ├── main.c     # CLI interface
│   ├── hash.c     # SHA-256 hashing
│   ├── object.c   # Object storage
│   ├── tree.c     # Tree objects
│   ├── commit.c   # Commit objects
│   ├── index.c    # Staging area
│   ├── refs.c     # References
│   ├── pack.c     # Packfiles
│   ├── network.c  # Network protocol
│   ├── gc.c       # Garbage collection
│   └── util.c     # Utilities
├── include/       # Headers
├── tests/         # Test suite
├── Makefile       # Build system
├── Dockerfile     # Container image
└── docker-compose.yml
```

---

## Example Workflow

```bash
# Initialize
cd ~/projects/myproject
fit init

# Work and commit
echo "code" > main.c
fit add main.c
fit commit -m "Initial code"

# Create backup
fit snapshot -m "Daily backup"

# Push to backup server
fit push laptop.local main

# Continue working
echo "more code" >> main.c
fit add main.c
fit commit -m "Added feature"
fit push laptop.local main
```

---

## Security Note

⚠️ Current version has no authentication or encryption. Use on trusted networks or behind VPN/SSH tunnel:

```bash
# SSH tunnel
ssh -L 9418:localhost:9418 server.local

# Then push through tunnel
fit push localhost main
```

---

## License

MIT License - See LICENSE file

---

## Credits

Inspired by Git's internals. Built from scratch without copying Git source code.

**Fit** - Because your filesystem belongs inside your terminal.
